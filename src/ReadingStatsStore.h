#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstdint>

struct ReadingSession {
  std::string bookPath;
  uint32_t startTime; // Unix timestamp
  uint32_t minutes;
};

struct ReadingDayStats {
  std::string date;
  uint32_t totalMinutes;
  std::map<std::string, uint32_t> bookMinutes;
  std::vector<ReadingSession> sessions;
};

class ReadingStatsStore;
namespace JsonSettingsIO {
bool loadReadingStats(ReadingStatsStore& store, const char* json);
}

class ReadingStatsStore {
  static ReadingStatsStore instance;

  std::vector<ReadingDayStats> history;
  uint32_t lifetimeMinutes = 0;
  uint32_t bestStreak = 0;
  uint32_t dailyGoalMinutes = 30; // Default 30 mins

  // Per-book last-known reading progress (0–100%).
  // Populated when a reader exits via recordProgress().
  std::map<std::string, uint8_t> bookLastPercent;

  friend bool JsonSettingsIO::loadReadingStats(ReadingStatsStore&, const char*);

 public:
  static ReadingStatsStore& getInstance() { return instance; }

  void addMinutes(uint32_t minutes, const std::string& bookPath);
  
  const std::vector<ReadingDayStats>& getHistory() const { return history; }
  uint32_t getLifetimeMinutes() const { return lifetimeMinutes; }
  uint32_t getBestStreak() const { return bestStreak; }
  uint32_t getDailyGoalMinutes() const { return dailyGoalMinutes; }
  void setDailyGoalMinutes(uint32_t mins) { dailyGoalMinutes = mins; saveToFile(); }

  // Per-book progress (0–100), returns 0 if absent.
  uint8_t getBookPercent(const std::string& path) const;
  // Record the last-known reading progress for a book.  Clamps to 0–100,
  // returns early if unchanged (write-throttling), else updates and saves.
  void recordProgress(const std::string& path, uint8_t percent);
  const std::map<std::string, uint8_t>& getBookProgress() const { return bookLastPercent; }

  std::vector<ReadingDayStats> getStatsForMonth(int month, int year) const;
  uint32_t getTodayMinutes() const;
  uint32_t getCurrentStreak() const;

  bool saveToFile() const;
  bool loadFromFile();

 private:
  ReadingStatsStore() = default;
  void findOrCreateToday();
  std::string getTodayDate();
};

#define READING_STATS ReadingStatsStore::getInstance()
