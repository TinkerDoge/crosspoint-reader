#include "JsonSettingsIO.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <string>

#include "ReadingStatsStore.h"

bool JsonSettingsIO::saveReadingStats(const ReadingStatsStore& store, const char* path) {
  JsonDocument doc;
  doc["totalMinutes"] = store.getLifetimeMinutes();
  doc["bestStreak"] = store.getBestStreak();
  doc["dailyGoal"] = store.getDailyGoalMinutes();

  JsonArray historyArr = doc["history"].to<JsonArray>();
  for (const auto& day : store.getHistory()) {
    JsonObject dayObj = historyArr.add<JsonObject>();
    dayObj["date"] = day.date;
    dayObj["total"] = day.totalMinutes;

    JsonObject booksObj = dayObj["books"].to<JsonObject>();
    for (const auto& [bookPath, minutes] : day.bookMinutes) {
      booksObj[bookPath] = minutes;
    }

    JsonArray sessionsArr = dayObj["sessions"].to<JsonArray>();
    for (const auto& session : day.sessions) {
      JsonObject sessionObj = sessionsArr.add<JsonObject>();
      sessionObj["path"] = session.bookPath;
      sessionObj["start"] = session.startTime;
      sessionObj["mins"] = session.minutes;
    }
  }

  const auto& bookProgress = store.getBookProgress();
  if (!bookProgress.empty()) {
    JsonObject progressObj = doc["bookProgress"].to<JsonObject>();
    for (const auto& [bookPath, percent] : bookProgress) {
      progressObj[bookPath] = percent;
    }
  }

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(path, json);
}

bool JsonSettingsIO::loadReadingStats(ReadingStatsStore& store, const char* json) {
  JsonDocument doc;
  const auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("RSS", "JSON parse error: %s", error.c_str());
    return false;
  }

  store.lifetimeMinutes = doc["totalMinutes"] | 0u;
  store.bestStreak = doc["bestStreak"] | 0u;
  store.dailyGoalMinutes = doc["dailyGoal"] | 30u;

  store.history.clear();
  JsonArray historyArr = doc["history"].as<JsonArray>();
  for (JsonObject dayObj : historyArr) {
    ReadingDayStats day;
    day.date = dayObj["date"] | std::string("");
    day.totalMinutes = dayObj["total"] | 0u;

    JsonObject booksObj = dayObj["books"].as<JsonObject>();
    for (JsonPair entry : booksObj) {
      day.bookMinutes[entry.key().c_str()] = entry.value().as<uint32_t>();
    }

    JsonArray sessionsArr = dayObj["sessions"].as<JsonArray>();
    for (JsonObject sessionObj : sessionsArr) {
      ReadingSession session;
      session.bookPath = sessionObj["path"] | std::string("");
      session.startTime = sessionObj["start"] | 0u;
      session.minutes = sessionObj["mins"] | 0u;
      day.sessions.push_back(session);
    }

    store.history.push_back(day);
  }

  store.bookLastPercent.clear();
  JsonObject progressObj = doc["bookProgress"].as<JsonObject>();
  for (JsonPair entry : progressObj) {
    uint8_t percent = entry.value().as<uint8_t>();
    if (percent > 100) percent = 100;
    store.bookLastPercent[entry.key().c_str()] = percent;
  }

  LOG_DBG("RSS", "Reading stats loaded from file (%zu entries)", store.history.size());
  return true;
}
