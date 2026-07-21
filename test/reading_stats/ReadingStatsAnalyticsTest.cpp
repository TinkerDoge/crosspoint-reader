/**
 * ReadingStatsAnalyticsTest.cpp
 *
 * Host unit tests for streak math in ReadingStatsAnalytics.
 * Covers all edge cases from spec §7:
 *   - empty history
 *   - only-today
 *   - consecutive days ending today
 *   - read yesterday not today (alive)
 *   - full day skipped (broken)
 *   - gap in middle
 *   - unsorted + duplicate dates
 *   - clock-went-backwards
 */

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "ReadingStatsStore.h"
#include "util/ReadingStatsAnalytics.h"

static int testsPassed = 0;
static int testsFailed = 0;

#define ASSERT_EQ(a, b)                                                                            \
  do {                                                                                             \
    auto _a = (a);                                                                                 \
    auto _b = (b);                                                                                 \
    if (_a != _b) {                                                                                \
      fprintf(stderr, "  FAIL: %s:%d: %s == %u, expected %u\n", __FILE__, __LINE__, #a,            \
              (unsigned)_a, (unsigned)_b);                                                         \
      testsFailed++;                                                                               \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

#define PASS() testsPassed++

// Helper: build a ReadingDayStats entry for a given date with some reading activity.
static ReadingDayStats makeDay(const std::string& date, uint32_t minutes = 30) {
  ReadingDayStats d;
  d.date = date;
  d.totalMinutes = minutes;
  return d;
}

// ---- getCurrentStreak tests ----

void testEmptyHistory_currentStreak() {
  printf("  getCurrentStreak: empty history → 0\n");
  std::vector<ReadingDayStats> history;
  ASSERT_EQ(ReadingStatsAnalytics::getCurrentStreak(history, "2026-07-20"), 0u);
  PASS();
}

void testOnlyToday_currentStreak() {
  printf("  getCurrentStreak: only today → 1\n");
  std::vector<ReadingDayStats> history = {makeDay("2026-07-20")};
  ASSERT_EQ(ReadingStatsAnalytics::getCurrentStreak(history, "2026-07-20"), 1u);
  PASS();
}

void testThreeConsecutiveDaysEndingToday() {
  printf("  getCurrentStreak: 3 consecutive days ending today → 3\n");
  std::vector<ReadingDayStats> history = {
      makeDay("2026-07-18"),
      makeDay("2026-07-19"),
      makeDay("2026-07-20"),
  };
  ASSERT_EQ(ReadingStatsAnalytics::getCurrentStreak(history, "2026-07-20"), 3u);
  PASS();
}

void testReadYesterdayNotToday() {
  printf("  getCurrentStreak: read yesterday not today → streak alive\n");
  // Read 3 consecutive days ending yesterday (Jul 19), today is Jul 20
  std::vector<ReadingDayStats> history = {
      makeDay("2026-07-17"),
      makeDay("2026-07-18"),
      makeDay("2026-07-19"),
  };
  ASSERT_EQ(ReadingStatsAnalytics::getCurrentStreak(history, "2026-07-20"), 3u);
  PASS();
}

void testFullDaySkipped() {
  printf("  getCurrentStreak: full day skipped → 0\n");
  // Last reading was 2 days ago (Jul 18), today is Jul 20 → missed a full day
  std::vector<ReadingDayStats> history = {
      makeDay("2026-07-17"),
      makeDay("2026-07-18"),
  };
  ASSERT_EQ(ReadingStatsAnalytics::getCurrentStreak(history, "2026-07-20"), 0u);
  PASS();
}

void testGapInMiddle_currentStreak() {
  printf("  getCurrentStreak: gap in middle → only latest run\n");
  // Read Mon, skip Tue, read Wed-Thu (today)
  std::vector<ReadingDayStats> history = {
      makeDay("2026-07-14"),  // Mon
      // skip Tue Jul 15
      makeDay("2026-07-16"),  // Wed
      makeDay("2026-07-17"),  // Thu (today)
  };
  ASSERT_EQ(ReadingStatsAnalytics::getCurrentStreak(history, "2026-07-17"), 2u);
  PASS();
}

void testUnsortedAndDuplicates_currentStreak() {
  printf("  getCurrentStreak: unsorted + duplicate dates → handled\n");
  std::vector<ReadingDayStats> history = {
      makeDay("2026-07-20", 15),
      makeDay("2026-07-18"),
      makeDay("2026-07-19"),
      makeDay("2026-07-20", 10),  // duplicate
      makeDay("2026-07-19", 5),   // duplicate
  };
  ASSERT_EQ(ReadingStatsAnalytics::getCurrentStreak(history, "2026-07-20"), 3u);
  PASS();
}

void testClockWentBackwards_currentStreak() {
  printf("  getCurrentStreak: clock went backwards → anchors on latest date, no crash\n");
  // history.back().date is "2026-07-22" but today is "2026-07-20" (clock went back)
  std::vector<ReadingDayStats> history = {
      makeDay("2026-07-20"),
      makeDay("2026-07-21"),
      makeDay("2026-07-22"),
  };
  // Should anchor on latest date (2026-07-22) and count the run
  auto result = ReadingStatsAnalytics::getCurrentStreak(history, "2026-07-20");
  // The spec says "treat the latest recorded date as the anchor rather than returning 0"
  // The 3 dates are consecutive, so streak = 3
  ASSERT_EQ(result, 3u);
  PASS();
}

void testZeroMinuteDay_currentStreak() {
  printf("  getCurrentStreak: day with 0 minutes → not counted\n");
  std::vector<ReadingDayStats> history = {
      makeDay("2026-07-18"),
      makeDay("2026-07-19", 0),  // 0 minutes = no real reading
      makeDay("2026-07-20"),
  };
  // Day with 0 minutes should not count as a reading day
  ASSERT_EQ(ReadingStatsAnalytics::getCurrentStreak(history, "2026-07-20"), 1u);
  PASS();
}

// ---- getBestStreak tests ----

void testEmptyHistory_bestStreak() {
  printf("  getBestStreak: empty history → 0\n");
  std::vector<ReadingDayStats> history;
  ASSERT_EQ(ReadingStatsAnalytics::getBestStreak(history), 0u);
  PASS();
}

void testOnlyToday_bestStreak() {
  printf("  getBestStreak: single day → 1\n");
  std::vector<ReadingDayStats> history = {makeDay("2026-07-20")};
  ASSERT_EQ(ReadingStatsAnalytics::getBestStreak(history), 1u);
  PASS();
}

void testGapInMiddle_bestStreak() {
  printf("  getBestStreak: gap in middle → longest run\n");
  // 3-day run, gap, 2-day run → best = 3
  std::vector<ReadingDayStats> history = {
      makeDay("2026-07-10"),
      makeDay("2026-07-11"),
      makeDay("2026-07-12"),
      // gap
      makeDay("2026-07-15"),
      makeDay("2026-07-16"),
  };
  ASSERT_EQ(ReadingStatsAnalytics::getBestStreak(history), 3u);
  PASS();
}

void testLaterRunIsLonger_bestStreak() {
  printf("  getBestStreak: later run is longer → returns that\n");
  // 2-day run, gap, 4-day run → best = 4
  std::vector<ReadingDayStats> history = {
      makeDay("2026-07-10"),
      makeDay("2026-07-11"),
      // gap
      makeDay("2026-07-15"),
      makeDay("2026-07-16"),
      makeDay("2026-07-17"),
      makeDay("2026-07-18"),
  };
  ASSERT_EQ(ReadingStatsAnalytics::getBestStreak(history), 4u);
  PASS();
}

void testUnsortedAndDuplicates_bestStreak() {
  printf("  getBestStreak: unsorted + duplicate dates → handled\n");
  std::vector<ReadingDayStats> history = {
      makeDay("2026-07-12"),
      makeDay("2026-07-10"),
      makeDay("2026-07-11"),
      makeDay("2026-07-11", 5),  // duplicate
  };
  ASSERT_EQ(ReadingStatsAnalytics::getBestStreak(history), 3u);
  PASS();
}

void testZeroMinuteDay_bestStreak() {
  printf("  getBestStreak: 0-minute day breaks streak\n");
  std::vector<ReadingDayStats> history = {
      makeDay("2026-07-10"),
      makeDay("2026-07-11"),
      makeDay("2026-07-12", 0),  // 0 minutes = not a real reading day
      makeDay("2026-07-13"),
  };
  // 10,11 = 2-day run; 12 is 0 min (not counted); 13 = 1-day run → best = 2
  ASSERT_EQ(ReadingStatsAnalytics::getBestStreak(history), 2u);
  PASS();
}

void testMonthBoundary_bestStreak() {
  printf("  getBestStreak: streak crossing month boundary\n");
  std::vector<ReadingDayStats> history = {
      makeDay("2026-06-29"),
      makeDay("2026-06-30"),
      makeDay("2026-07-01"),
      makeDay("2026-07-02"),
  };
  ASSERT_EQ(ReadingStatsAnalytics::getBestStreak(history), 4u);
  PASS();
}

void testYearBoundary_bestStreak() {
  printf("  getBestStreak: streak crossing year boundary\n");
  std::vector<ReadingDayStats> history = {
      makeDay("2025-12-30"),
      makeDay("2025-12-31"),
      makeDay("2026-01-01"),
      makeDay("2026-01-02"),
  };
  ASSERT_EQ(ReadingStatsAnalytics::getBestStreak(history), 4u);
  PASS();
}

int main() {
  printf("=== Reading Stats Analytics Tests ===\n\n");

  printf("getCurrentStreak:\n");
  testEmptyHistory_currentStreak();
  testOnlyToday_currentStreak();
  testThreeConsecutiveDaysEndingToday();
  testReadYesterdayNotToday();
  testFullDaySkipped();
  testGapInMiddle_currentStreak();
  testUnsortedAndDuplicates_currentStreak();
  testClockWentBackwards_currentStreak();
  testZeroMinuteDay_currentStreak();

  printf("\ngetBestStreak:\n");
  testEmptyHistory_bestStreak();
  testOnlyToday_bestStreak();
  testGapInMiddle_bestStreak();
  testLaterRunIsLonger_bestStreak();
  testUnsortedAndDuplicates_bestStreak();
  testZeroMinuteDay_bestStreak();
  testMonthBoundary_bestStreak();
  testYearBoundary_bestStreak();

  printf("\n=== Results: %d passed, %d failed ===\n", testsPassed, testsFailed);
  return testsFailed > 0 ? 1 : 0;
}
