#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "../ReadingStatsStore.h"

class ReadingStatsAnalytics {
 public:
  static uint8_t getHeatLevel(uint32_t minutes);

  // Real streak algorithms — pure functions, no hidden time() calls.
  // "today" is passed as a YYYY-MM-DD string so the function is deterministic and testable.
  static uint32_t getCurrentStreak(const std::vector<ReadingDayStats>& history, const std::string& today);
  static uint32_t getBestStreak(const std::vector<ReadingDayStats>& history);
  
  // Helper to get stats for the last 42 days (7x6 grid)
  static std::vector<uint32_t> getLast42DaysMinutes(const std::vector<ReadingDayStats>& history);

 private:
  // Convert a "YYYY-MM-DD" date string to a day number (days since epoch, at noon to avoid DST).
  // Returns -1 on parse failure.
  static int64_t dateToDayNumber(const std::string& yyyy_mm_dd);
};
