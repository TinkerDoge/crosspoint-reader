#include "ReadingStatsStore.h"
#include <HalStorage.h>
#include <Logging.h>
#include <ctime>
#include <algorithm>
#include "JsonSettingsIO.h"
#include "util/ReadingStatsAnalytics.h"

ReadingStatsStore ReadingStatsStore::instance;

const char* STATS_FILE_PATH = "/stats/reading_stats.json";

void ReadingStatsStore::addMinutes(uint32_t minutes, const std::string& bookPath) {
  findOrCreateToday();
  ReadingDayStats& today = history.back();
  today.totalMinutes += minutes;
  today.bookMinutes[bookPath] += minutes;
  
  ReadingSession session;
  session.bookPath = bookPath;
  session.startTime = time(nullptr) - (minutes * 60);
  session.minutes = minutes;
  today.sessions.push_back(session);
  
  lifetimeMinutes += minutes;

  // Recompute bestStreak from history (full recompute — cheap, history is small).
  // This replaces the old `max(bestStreak, currentStreak)` heuristic and ensures
  // previously inflated values self-heal.
  bestStreak = ReadingStatsAnalytics::getBestStreak(history);

  saveToFile();
}

uint32_t ReadingStatsStore::getTodayMinutes() const {
  std::string todayDate = const_cast<ReadingStatsStore*>(this)->getTodayDate();
  if (!history.empty() && history.back().date == todayDate) {
    return history.back().totalMinutes;
  }
  return 0;
}

uint32_t ReadingStatsStore::getCurrentStreak() const {
  // Delegate to the single source of truth in ReadingStatsAnalytics.
  std::string today = const_cast<ReadingStatsStore*>(this)->getTodayDate();
  return ReadingStatsAnalytics::getCurrentStreak(history, today);
}

uint8_t ReadingStatsStore::getBookPercent(const std::string& path) const {
  auto it = bookLastPercent.find(path);
  return (it != bookLastPercent.end()) ? it->second : 0;
}

void ReadingStatsStore::recordProgress(const std::string& path, uint8_t percent) {
  // Clamp to 0–100.
  if (percent > 100) percent = 100;

  // Return early if unchanged (write-throttling).
  auto it = bookLastPercent.find(path);
  if (it != bookLastPercent.end() && it->second == percent) {
    return;
  }

  bookLastPercent[path] = percent;
  saveToFile();
}

std::vector<ReadingDayStats> ReadingStatsStore::getStatsForMonth(int month, int year) const {
  std::vector<ReadingDayStats> monthStats;
  char prefix[8];
  snprintf(prefix, sizeof(prefix), "%04d-%02d", year, month);
  std::string prefixStr(prefix);

  for (const auto& day : history) {
    if (day.date.find(prefixStr) == 0) {
      monthStats.push_back(day);
    }
  }
  return monthStats;
}

void ReadingStatsStore::findOrCreateToday() {
  std::string todayDate = getTodayDate();
  if (history.empty() || history.back().date != todayDate) {
    history.push_back({todayDate, 0, {}, {}});
  }
}

std::string ReadingStatsStore::getTodayDate() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  char buf[11];
  strftime(buf, sizeof(buf), "%Y-%m-%d", t);
  return std::string(buf);
}

bool ReadingStatsStore::saveToFile() const {
  if (!Storage.exists("/stats")) {
    Storage.mkdir("/stats");
  }
  return JsonSettingsIO::saveReadingStats(*this, STATS_FILE_PATH);
}

bool ReadingStatsStore::loadFromFile() {
  if (!Storage.exists(STATS_FILE_PATH)) {
    return false;
  }
  String json = Storage.readFile(STATS_FILE_PATH);
  if (json.length() == 0) {
    return false;
  }
  bool ok = JsonSettingsIO::loadReadingStats(*this, json.c_str());
  if (ok) {
    // Recompute bestStreak from history so any previously persisted bad value self-heals.
    bestStreak = ReadingStatsAnalytics::getBestStreak(history);
  }
  return ok;
}
