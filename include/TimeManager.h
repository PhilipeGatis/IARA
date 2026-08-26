#pragma once

#include "Config.h"
#include <Arduino.h>
#include <NTPClient.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WiFiUdp.h>

/// @brief Manages RTC DS3231 + NTP synchronization and schedule checking.
class TimeManager {
public:
  TimeManager();

  /// Initialize RTC hardware and NTP client
  void begin();

  /// Periodically sync RTC with NTP (call in loop)
  void update();

  /// Force NTP sync now
  bool syncWithNTP();

  /// Get current DateTime (RTC preferred, NTP fallback)
  DateTime now();

  /// Window for daily match
  bool isDailyScheduleTime(uint8_t hour, uint8_t minute);

  /// Get formatted time string "YYYY/MM/DD HH:MM:SS"
  String getFormattedTime();

  /// RTC physically connected?
  bool isRtcConnected() const { return _rtcConnected; }
  bool hasRtcLostPower() const { return _rtcLostPower; }

  /// Is the clock trustworthy enough to schedule against?
  ///
  /// It is not, in two situations that both look completely normal from the
  /// outside. An RTC that lost power reports a year-2000 date, so
  /// `now >= lastRun + interval` is false forever and the water change simply
  /// never happens — no error, no notification, nothing to notice. And a garbage
  /// reading in the *future* gets stamped into _tpaLastRun, which poisons the
  /// comparison permanently even after NTP corrects the clock.
  ///
  /// A clock jump also moves FertManager's day key, and the same channel can
  /// then dose twice in one day.
  bool isTimeValid() const {
    return (_rtcConnected && !_rtcLostPower) || _ntpEverSynced;
  }

private:
  RTC_DS3231 _rtc;
  WiFiUDP _ntpUDP;
  NTPClient _timeClient;

  bool _rtcConnected;
  bool _rtcLostPower;
  bool _ntpStarted;
  bool _ntpEverSynced = false;
  unsigned long _lastNtpSync;

  static constexpr long UTC_OFFSET_BRASILIA = -3 * 3600;
};
