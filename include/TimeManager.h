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
  ///
  /// The source flags alone are not the answer, because both of them latch. A
  /// board whose RTC dropped off the bus and whose NTP never landed a packet
  /// still reported a trustworthy clock for hours, so the scheduler compared
  /// against a frozen date and the water change never fired. What the caller
  /// actually needs to know is whether the reading it is about to use is a real
  /// instant, so this asks the clock and looks at the answer.
  bool isTimeValid() {
    if (!((_rtcConnected && !_rtcLostPower) || _ntpEverSynced)) {
      return false;
    }
    return isEpochSane(now().unixtime());
  }

  /// A timestamp only counts as a real instant inside this window.
  ///
  /// Every wrong clock this board has actually produced falls outside it. A
  /// DS3231 that lost its battery reads year 2000. An NTPClient that never
  /// received a packet answers `_timeOffset + 0 + secondsSinceBoot`, and the
  /// offset is negative: for the first three hours of a run that underflows to
  /// roughly 4.29e9 — a year-2106 date, comfortably past any "is it greater
  /// than zero" check — and after that it wraps down to a few seconds past
  /// 1970. Both were accepted as valid; both are rejected here.
  ///
  /// The lower bound only has to sit above those, not track the calendar: it is
  /// a floor under nonsense, not an expiry date for the firmware.
  static constexpr uint32_t EPOCH_MIN_VALID = 1767225600UL; // 2026-01-01
  static constexpr uint32_t EPOCH_MAX_VALID = 2524608000UL; // 2050-01-01

  static bool isEpochSane(uint32_t epoch) {
    return epoch >= EPOCH_MIN_VALID && epoch <= EPOCH_MAX_VALID;
  }

#ifdef UNIT_TEST
  /// Test seams: the RTC and NTP client are owned by value, and the mocks carry
  /// their own controls.
  RTC_DS3231 &mockRtc() { return _rtc; }
  NTPClient &mockNtp() { return _timeClient; }
#endif

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
