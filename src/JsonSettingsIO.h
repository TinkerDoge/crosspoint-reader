#pragma once

#include <vector>

class CrossPointSettings;
class CrossPointState;
class WifiCredentialStore;
class RecentBooksStore;
class ReadingStatsStore;
class OpdsServerStore;
struct BookmarkEntry;

namespace JsonSettingsIO {

// CrossPointSettings
bool saveSettings(const CrossPointSettings& s, const char* path);
bool loadSettings(CrossPointSettings& s, const char* json, bool* needsResave = nullptr);

// CrossPointState
bool saveState(const CrossPointState& s, const char* path);
bool loadState(CrossPointState& s, const char* json);

// Bookmarks
bool saveBookmarks(const std::vector<BookmarkEntry>& bookmarks, const char* path);
bool loadBookmarks(std::vector<BookmarkEntry>& bookmarks, const char* json);

// ReadingStatsStore
bool saveReadingStats(const ReadingStatsStore& store, const char* path);
bool loadReadingStats(ReadingStatsStore& store, const char* json);

}  // namespace JsonSettingsIO
