#include "TimeManager.h"

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
  _timeClient.update();

  unsigned long epoch = _timeClient.getEpochTime();
  if (epoch < 1000000) {
    Serial.println("[Time] NTP returned invalid epoch.");
    return false;
  }

  if (_rtcConnected) {
    DateTime ntpTime(epoch);
    _rtc.adjust(ntpTime);
    Serial.println("[Time] RTC adjusted from NTP.");
  }

  _lastNtpSync = millis();
  return true;
}

DateTime TimeManager::now() {
  if (_rtcConnected) {
    return _rtc.now();
  }

  // Fallback: use cached NTP epoch (don't call update() here to avoid spam)
  unsigned long epoch = _timeClient.getEpochTime();
  if (epoch > 1000000) {
    return DateTime(epoch);
  }

  // No valid time yet — return a safe default
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
