#include "TimeManager.h"

// Wire is used directly here to bring the bus up before the RTC is probed. On
// the device RTClib drags it in anyway; the native build has no such luck.
#include <Wire.h>

TimeManager::TimeManager()
    : _timeClient(_ntpUDP, "pool.ntp.org", UTC_OFFSET_BRASILIA),
      _rtcConnected(false), _rtcLostPower(false), _ntpStarted(false), _lastNtpSync(0) {}

void TimeManager::begin() {
  // Initialize I2C and RTC
  Wire.begin(); // SDA=21, SCL=22 (ESP32 defaults)

  if (!_rtc.begin()) {
    Serial.println("[Time] RTC DS3231 not found — using NTP only.");
    _rtcConnected = false;
    _rtcLostPower = false;
  } else {
    _rtcConnected = true;
    Serial.println("[Time] RTC DS3231 detected.");

    if (_rtc.lostPower()) {
      Serial.println("[Time] RTC lost power, needs sync.");
      _rtcLostPower = true;
    } else {
      _rtcLostPower = false;
    }
  }

  // Start NTP client only if WiFi is available
  if (WiFi.status() == WL_CONNECTED) {
    _timeClient.begin();
    _timeClient.setTimeOffset(UTC_OFFSET_BRASILIA);
    syncWithNTP();
  } else {
    Serial.println("[Time] No WiFi — NTP sync deferred.");
    _ntpStarted = false;
  }
}

void TimeManager::update() {
  // Skip if no WiFi
  if (WiFi.status() != WL_CONNECTED)
    return;

  // Lazy-start NTP if WiFi came up after boot
  if (!_ntpStarted) {
    _timeClient.begin();
    _timeClient.setTimeOffset(UTC_OFFSET_BRASILIA);
    _ntpStarted = true;
    Serial.println("[Time] WiFi connected — starting NTP.");
  }

  unsigned long now = millis();
  if (_lastNtpSync == 0 || (now - _lastNtpSync) >= NTP_SYNC_INTERVAL_MS) {
    syncWithNTP();
  }
}

bool TimeManager::syncWithNTP() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Time] No Wi-Fi, skipping NTP sync.");
    return false;
  }

  Serial.println("[Time] Syncing with NTP...");

  // forceUpdate() rather than update(), and its return value rather than the
  // epoch afterwards. update() answers false when it simply decided not to ask
  // yet, and the epoch is not evidence of anything: with no packet ever
  // received the client keeps _currentEpoc at zero and still hands back
  // offset + uptime, which is what got accepted here as a valid time.
  if (!_timeClient.forceUpdate()) {
    Serial.println("[Time] NTP did not answer. Will retry in 10s.");
    // Prevent UDP spam: set last sync to trigger again in 10 seconds
    _lastNtpSync = millis() - NTP_SYNC_INTERVAL_MS + 10000;
    return false;
  }

  // Truncated to 32 bits on purpose: that is the width the client does its
  // arithmetic in on the device, so an underflow has to be seen here the same
  // way rather than sign-extended into something plausible on a 64-bit host.
  const uint32_t epoch = (uint32_t)_timeClient.getEpochTime();
  if (!isEpochSane(epoch)) {
    Serial.printf("[Time] NTP answered with an impossible epoch (%lu) — "
                  "ignoring it. Will retry in 10s.\n",
                  (unsigned long)epoch);
    _lastNtpSync = millis() - NTP_SYNC_INTERVAL_MS + 10000;
    return false;
  }

  if (_rtcConnected) {
    DateTime ntpTime(epoch);
    _rtc.adjust(ntpTime);
    // The RTC now holds a real time. Leaving this set would keep the clock
    // marked untrustworthy for as long as the board stays up.
    _rtcLostPower = false;
    Serial.println("[Time] RTC adjusted from NTP.");
  }

  _ntpEverSynced = true;
  _lastNtpSync = millis();
  return true;
}

DateTime TimeManager::now() {
  if (_rtcConnected) {
    const DateTime fromRtc = _rtc.now();
    if (isEpochSane(fromRtc.unixtime())) {
      return fromRtc;
    }
    // Present on the bus but talking nonsense — a dead backup battery reads
    // year 2000, and a module that half fell off reads back zeros. NTP is the
    // better answer if it has one, so fall through rather than hand the
    // scheduler a date from before the tank existed.
  }

  // Fallback: use cached NTP epoch (don't call update() here to avoid spam)
  const uint32_t epoch = (uint32_t)_timeClient.getEpochTime();
  if (isEpochSane(epoch)) {
    return DateTime(epoch);
  }

  // No valid time yet. This is not a safe default — it is a plausible-looking
  // wrong answer, and callers must not schedule against it. isTimeValid() is
  // the guard; this only keeps the return type honest.
  return DateTime(2025, 1, 1);
}

bool TimeManager::isDailyScheduleTime(uint8_t hour, uint8_t minute) {
  DateTime current = now();
  // Match hour and minute exactly (within a 60-second window)
  return (current.hour() == hour && current.minute() == minute);
}

String TimeManager::getFormattedTime() {
  DateTime dt = now();
  char buf[22];
  snprintf(buf, sizeof(buf), "%04d/%02d/%02d %02d:%02d:%02d", dt.year(),
           dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());
  return String(buf);
}
