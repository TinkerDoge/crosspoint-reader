#include "HalClock.h"

#include <Logging.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

HalClock halClock;  // Singleton instance

namespace {
// Epochs below this predate any supported firmware build (2023-11-14) and mean
// the clock was never set — time(nullptr) starts near 0 after boot.
constexpr time_t MIN_VALID_EPOCH = 1700000000;
}  // namespace

void HalClock::begin() {
  _available = _sdkRtc.begin();
  LOG_INF("CLK", _available ? "SDK RTC found" : "RTC not found");
}

bool HalClock::getTime(uint8_t& hour, uint8_t& minute) const {
  if (!_available) return false;

  const unsigned long now = millis();
  if (_lastPollMs != 0 && (now - _lastPollMs) < CLOCK_POLL_MS) {
    hour = _cachedHour;
    minute = _cachedMinute;
    return true;
  }

  Rtc::DateTime dt;
  if (!_sdkRtc.now(dt)) {
    if (!_hasCachedTime) return false;
    _lastPollMs = now;
    hour = _cachedHour;
    minute = _cachedMinute;
    return true;
  }
  _cachedHour = dt.hour;
  _cachedMinute = dt.minute;
  _lastPollMs = now;
  _hasCachedTime = true;
  hour = _cachedHour;
  minute = _cachedMinute;
  return true;
}

bool HalClock::formatTime(char* buf, size_t bufSize, uint8_t utcOffsetQuarterHoursBiased, bool use12Hour) const {
  if (bufSize < (use12Hour ? 9u : 6u)) return false;
  uint8_t h, m;
  if (!getTime(h, m)) return false;

  // Apply UTC offset: convert biased value to signed quarter-hours.
  // Clamp against corrupted persisted values so display time can't drift outside [-12:00, +14:00].
  if (utcOffsetQuarterHoursBiased > 104) utcOffsetQuarterHoursBiased = 104;
  int offsetQuarterHours = static_cast<int>(utcOffsetQuarterHoursBiased) - 48;
  int totalMinutes = static_cast<int>(h) * 60 + static_cast<int>(m) + offsetQuarterHours * 15;

  // Wrap around 24 hours
  totalMinutes = ((totalMinutes % 1440) + 1440) % 1440;

  const int hour24 = totalMinutes / 60;
  const int min = totalMinutes % 60;
  if (use12Hour) {
    const bool pm = hour24 >= 12;
    int hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;
    snprintf(buf, bufSize, "%d:%02d %s", hour12, min, pm ? "PM" : "AM");
  } else {
    snprintf(buf, bufSize, "%02d:%02d", hour24, min);
  }
  return true;
}

bool HalClock::systemTimeValid() { return time(nullptr) >= MIN_VALID_EPOCH; }

bool HalClock::syncSystemTimeFromNTP() {
  if (WiFi.status() != WL_CONNECTED) {
    LOG_ERR("CLK", "WiFi not connected, cannot sync NTP");
    return false;
  }

  LOG_INF("CLK", "Starting NTP sync...");

  // Stop any previous SNTP session to avoid state conflicts.
  esp_sntp_stop();

  // Configure and restart SNTP.
  configTzTime("UTC0", "pool.ntp.org", "time.nist.gov");

  // Give the SNTP task one tick to begin.
  delay(50);

  // Wait for SNTP sync to complete (up to 5 seconds)
  constexpr int maxAttempts = 50;
  sntp_sync_status_t lastStatus = SNTP_SYNC_STATUS_RESET;
  int statusChanges = 0;

  for (int i = 0; i < maxAttempts; i++) {
    sntp_sync_status_t status = sntp_get_sync_status();
    if (status != lastStatus) {
      LOG_DBG("CLK", "SNTP status changed: %d -> %d (attempt %d/%d)", (int)lastStatus, (int)status, i + 1,
              maxAttempts);
      lastStatus = status;
      statusChanges++;
    }

    if (status == SNTP_SYNC_STATUS_COMPLETED) {
      const time_t now = time(nullptr);
      LOG_INF("CLK", "SNTP sync completed. System epoch: %lld", (long long)now);

      // Reject epochs that predate any supported firmware build.
      if (now < MIN_VALID_EPOCH) {
        LOG_ERR("CLK", "SNTP returned invalid epoch: %lld (before 2024)", (long long)now);
        return false;
      }
      return true;
    }
    delay(100);
  }

  LOG_ERR("CLK", "NTP sync timed out after %d attempts (status=%d, changes=%d)", maxAttempts, (int)lastStatus,
          statusChanges);
  return false;
}

bool HalClock::syncFromNTP() {
  if (!_available) return false;

  // Ensure the system clock is valid before writing it to the RTC. Skips the
  // blocking SNTP round-trip when the clock was already set (e.g. by an
  // earlier syncSystemTimeFromNTP() on this WiFi connection).
  if (!systemTimeValid() && !syncSystemTimeFromNTP()) return false;

  const time_t now = time(nullptr);
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);

  Rtc::DateTime dt;
  dt.year = static_cast<uint16_t>(timeinfo.tm_year + 1900);
  dt.month = static_cast<uint8_t>(timeinfo.tm_mon + 1);
  dt.day = static_cast<uint8_t>(timeinfo.tm_mday);
  dt.hour = static_cast<uint8_t>(timeinfo.tm_hour);
  dt.minute = static_cast<uint8_t>(timeinfo.tm_min);
  dt.second = static_cast<uint8_t>(timeinfo.tm_sec);
  dt.weekday = static_cast<uint8_t>(timeinfo.tm_wday);

  const bool writeOk = _sdkRtc.set(dt);
  LOG_INF("CLK", "RTC write %s", writeOk ? "succeeded" : "FAILED");

  if (writeOk) {
    _lastPollMs = 0;
    _cachedHour = dt.hour;
    _cachedMinute = dt.minute;
    _hasCachedTime = true;
    LOG_INF("CLK", "RTC set to %04u-%02u-%02u %02u:%02u:%02u UTC", dt.year, dt.month, dt.day, dt.hour, dt.minute,
            dt.second);
    return true;
  }
  return false;
}

bool HalClock::setSystemTimeFromRtc() {
  if (!_available) return false;

  Rtc::DateTime dt;
  if (!_sdkRtc.now(dt)) return false;

  if (dt.year < 2024 || dt.year > 2100 || dt.month < 1 || dt.month > 12 || dt.day < 1 || dt.day > 31) {
    LOG_ERR("CLK", "RTC time implausible (%04u-%02u-%02u), not setting system clock", dt.year, dt.month, dt.day);
    return false;
  }

  // The RTC stores UTC (syncFromNTP writes gmtime), and TZ is UTC0 by default
  // (configTzTime keeps it that way), so mktime's local-time interpretation
  // yields the correct epoch here.
  struct tm t = {};
  t.tm_year = static_cast<int>(dt.year) - 1900;
  t.tm_mon = static_cast<int>(dt.month) - 1;
  t.tm_mday = dt.day;
  t.tm_hour = dt.hour;
  t.tm_min = dt.minute;
  t.tm_sec = dt.second;
  t.tm_isdst = -1;

  const time_t epoch = mktime(&t);
  if (epoch < MIN_VALID_EPOCH) {
    LOG_ERR("CLK", "RTC epoch %lld below validity floor, not setting system clock", (long long)epoch);
    return false;
  }

  const struct timeval tv = {epoch, 0};
  settimeofday(&tv, nullptr);
  LOG_INF("CLK", "System clock set from RTC: %04u-%02u-%02u %02u:%02u:%02u UTC", dt.year, dt.month, dt.day, dt.hour,
          dt.minute, dt.second);
  return true;
}
