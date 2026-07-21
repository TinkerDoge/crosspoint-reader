#include "ReadingStatsActivity.h"
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include "components/UITheme.h"
#include "../../ReadingStatsStore.h"
#include "../../RecentBooksStore.h"
#include "../../fontIds.h"
#include "ReadingHeatmapActivity.h"
#include "ReadingProfileActivity.h"

void ReadingStatsActivity::onEnter() {
  Activity::onEnter();
  loadStats();
  requestUpdate();
}

void ReadingStatsActivity::onExit() {
  Activity::onExit();
}

void ReadingStatsActivity::loadStats() {
  startedBooks.clear();
  auto history = READING_STATS.getHistory();
  
  // Aggregate total minutes per book
  std::map<std::string, uint32_t> bookMinutesTotal;
  for (const auto& day : history) {
    for (const auto& [path, mins] : day.bookMinutes) {
      bookMinutesTotal[path] += mins;
    }
  }

  // Get books from RecentBooksStore
  auto recent = RECENT_BOOKS.getBooks();
  for (const auto& rb : recent) {
    if (bookMinutesTotal.count(rb.path)) {
      BookStat stat;
      stat.path = rb.path;
      stat.title = rb.title;
      stat.totalMinutes = bookMinutesTotal[rb.path];
      stat.progressPercent = READING_STATS.getBookPercent(rb.path);
      startedBooks.push_back(stat);
    }
  }
}

void ReadingStatsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activityManager.popActivity();
  }
  
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    // Navigate to Heatmap
    activityManager.pushActivity(std::make_unique<ReadingHeatmapActivity>(renderer, mappedInput));
  }
  
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    // Navigate to Profile
    activityManager.pushActivity(std::make_unique<ReadingProfileActivity>(renderer, mappedInput));
  }
}

void ReadingStatsActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_READING_STATS));

  drawStatsHeader();
  drawBookList();

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Heatmap", "Profile", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(firstRenderDone ? HalDisplay::FAST_REFRESH : HalDisplay::FULL_REFRESH);
  firstRenderDone = true;
}

void ReadingStatsActivity::drawStatsHeader() {
  const int startY = 120; // Increased from 80 to prevent clipping
  const int col1 = 20;
  const int col2 = 240;
  
  uint32_t currentStreak = READING_STATS.getCurrentStreak();
  uint32_t bestStreak = READING_STATS.getBestStreak();
  uint32_t totalMinutes = READING_STATS.getLifetimeMinutes();
  uint32_t todayMinutes = READING_STATS.getTodayMinutes();
  uint32_t goal = READING_STATS.getDailyGoalMinutes();

  // Grid of 4 boxes
  auto drawBox = [&](int x, int y, const char* label, const char* value) {
    renderer.drawRect(x, y, 220, 80, true);
    renderer.drawText(UI_10_FONT_ID, x + 10, y + 10, label, true);
    renderer.drawText(UI_12_FONT_ID, x + 10, y + 40, value, true, EpdFontFamily::BOLD);
  };

  char buf1[32], buf2[32], buf3[32], buf4[32];
  snprintf(buf1, sizeof(buf1), "%d", currentStreak);
  snprintf(buf2, sizeof(buf2), "%d", bestStreak);
  snprintf(buf3, sizeof(buf3), "%d/%d m", todayMinutes, goal);
  snprintf(buf4, sizeof(buf4), "%.1f h", totalMinutes / 60.0);

  drawBox(col1, startY, "Goal Streak", buf1);
  drawBox(col2, startY, "Max Streak", buf2);
  drawBox(col1, startY + 90, "Daily Goal", buf3);
  drawBox(col2, startY + 90, "Total Time", buf4);
}

void ReadingStatsActivity::drawBookList() {
  const int startY = 320; // Increased from 280 to prevent clipping
  renderer.drawText(UI_12_FONT_ID, 20, startY, "Started Books", true, EpdFontFamily::BOLD);
  
  auto truncateText = [&](int fontId, const std::string& text, int maxWidth) -> std::string {
    if (renderer.getTextWidth(fontId, text.c_str()) <= maxWidth) return text;
    std::string res = text;
    while (!res.empty() && renderer.getTextWidth(fontId, (res + "...").c_str()) > maxWidth) {
      res.pop_back();
    }
    return res + "...";
  };

  int y = startY + 50; // Increased from 40 to prevent title overlap
  for (size_t i = 0; i < startedBooks.size() && i < 5; i++) {
    const auto& book = startedBooks[i];
    std::string displayTitle = truncateText(UI_10_FONT_ID, book.title, 320);
    renderer.drawText(UI_10_FONT_ID, 20, y, displayTitle.c_str(), true);
    
    char timeBuf[32];
    snprintf(timeBuf, sizeof(timeBuf), "%dh %dm", book.totalMinutes / 60, book.totalMinutes % 60);
    renderer.drawText(UI_10_FONT_ID, 350, y, timeBuf, true);
    
    // Progress bar
    renderer.drawRect(20, y + 25, 440, 6, true);
    renderer.fillRect(20, y + 25, (440 * book.progressPercent) / 100, 6, true);
    
    y += 50;
  }
}
