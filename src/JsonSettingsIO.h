#pragma once

class ReadingStatsStore;

namespace JsonSettingsIO {

// Reading stats still use their legacy JSON schema while the core settings,
// state, and bookmark stores use the upstream PersistableStore implementations.
bool saveReadingStats(const ReadingStatsStore& store, const char* path);
bool loadReadingStats(ReadingStatsStore& store, const char* json);

}  // namespace JsonSettingsIO
