#include "ReadingStatsAnalytics.h"
#include <ctime>
#include <map>
#include <algorithm>
#include <set>

uint8_t ReadingStatsAnalytics::getHeatLevel(uint32_t minutes) {
  if (minutes == 0) return 0;
  if (minutes <= 10) return 1;
  if (minutes <= 30) return 2;
  if (minutes <= 60) return 3;
  return 4;
}

// Convert "YYYY-MM-DD" to a day number (days since epoch, using noon to avoid DST edges).
int64_t ReadingStatsAnalytics::dateToDayNumber(const std::string& yyyy_mm_dd) {
  if (yyyy_mm_dd.size() < 10) return -1;

  struct tm t{};
  t.tm_year = std::stoi(yyyy_mm_dd.substr(0, 4)) - 1900;
  t.tm_mon = std::stoi(yyyy_mm_dd.substr(5, 2)) - 1;
  t.tm_mday = std::stoi(yyyy_mm_dd.substr(8, 2));
  t.tm_hour = 12;  // Noon — avoids DST edge cases
  t.tm_min = 0;
  t.tm_sec = 0;
  t.tm_isdst = -1;

  time_t epoch = mktime(&t);
  if (epoch == -1) return -1;
  return static_cast<int64_t>(epoch) / 86400;
}

uint32_t ReadingStatsAnalytics::getCurrentStreak(const std::vector<ReadingDayStats>& history,
                                                  const std::string& today) {
  if (history.empty()) return 0;

  // 1. Collect distinct dates with totalMinutes > 0 into a set.
  std::set<std::string> readingDates;
  for (const auto& day : history) {
    if (day.totalMinutes > 0) {
      readingDates.insert(day.date);
    }
  }
  if (readingDates.empty()) return 0;

  // 2. Determine the anchor date.
  int64_t todayNum = dateToDayNumber(today);
  if (todayNum < 0) return 0;

  // Find the latest date in the set for the clock-backwards defensive check.
  const std::string& latestDate = *readingDates.rbegin();
  int64_t latestNum = dateToDayNumber(latestDate);

  std::string anchor;
  if (latestNum > todayNum) {
    // Clock went backwards: latest recorded date is after today.
    // Anchor on the latest date rather than returning 0.
    anchor = latestDate;
  } else if (readingDates.count(today)) {
    // Today is in the set → anchor on today.
    anchor = today;
  } else {
    // Compute yesterday.
    time_t todayEpoch = todayNum * 86400 + 43200;  // noon
    time_t yesterdayEpoch = todayEpoch - 86400;
    struct tm* yt = localtime(&yesterdayEpoch);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", yt);
    std::string yesterday(buf);

    if (readingDates.count(yesterday)) {
      // Yesterday is in the set → streak still alive.
      anchor = yesterday;
    } else {
      // Missed a full day → streak broken.
      return 0;
    }
  }

  // 3. Walk backwards one calendar day at a time while each date is present.
  int64_t anchorNum = dateToDayNumber(anchor);
  if (anchorNum < 0) return 0;

  uint32_t streak = 0;
  for (int64_t dayNum = anchorNum;; dayNum--) {
    // Convert day number back to a YYYY-MM-DD string.
    time_t dayEpoch = dayNum * 86400 + 43200;  // noon
    struct tm* dt = localtime(&dayEpoch);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", dt);
    std::string dateStr(buf);

    if (readingDates.count(dateStr)) {
      streak++;
    } else {
      break;
    }
  }

  return streak;
}

uint32_t ReadingStatsAnalytics::getBestStreak(const std::vector<ReadingDayStats>& history) {
  if (history.empty()) return 0;

  // 1. Collect distinct reading dates (totalMinutes > 0), sort ascending.
  std::set<std::string> dateSet;
  for (const auto& day : history) {
    if (day.totalMinutes > 0) {
      dateSet.insert(day.date);
    }
  }
  if (dateSet.empty()) return 0;

  std::vector<std::string> dates(dateSet.begin(), dateSet.end());
  // Already sorted by set ordering (lexicographic = chronological for YYYY-MM-DD).

  // 2. Single pass: extend current run when next date is exactly +1 day, else reset.
  uint32_t bestStreak = 1;
  uint32_t currentRun = 1;
  int64_t prevDayNum = dateToDayNumber(dates[0]);

  for (size_t i = 1; i < dates.size(); i++) {
    int64_t dayNum = dateToDayNumber(dates[i]);
    if (dayNum < 0) continue;

    if (dayNum == prevDayNum + 1) {
      currentRun++;
    } else {
      currentRun = 1;
    }

    if (currentRun > bestStreak) {
      bestStreak = currentRun;
    }
    prevDayNum = dayNum;
  }

  return bestStreak;
}

std::vector<uint32_t> ReadingStatsAnalytics::getLast42DaysMinutes(const std::vector<ReadingDayStats>& history) {
  std::map<std::string, uint32_t> historyMap;
  for (const auto& day : history) {
    historyMap[day.date] = day.totalMinutes;
  }

  std::vector<uint32_t> result;
  result.reserve(42);

  time_t now = time(nullptr);
  // Round to start of day
  struct tm* t = localtime(&now);
  t->tm_hour = 12; // Use noon to avoid DST issues
  t->tm_min = 0;
  t->tm_sec = 0;
  time_t current = mktime(t);

  for (int i = 41; i >= 0; i--) {
    time_t dayTime = current - (i * 24 * 3600);
    struct tm* dt = localtime(&dayTime);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", dt);
    
    std::string dateStr(buf);
    if (historyMap.count(dateStr)) {
      result.push_back(historyMap[dateStr]);
    } else {
      result.push_back(0);
    }
  }

  return result;
}
