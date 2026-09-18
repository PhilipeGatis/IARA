#pragma once
// ============================================================================
// RTClib.h Mock for Native Unit Tests
// Provides DateTime class and RTC_DS3231 stub
// ============================================================================

#include <cstdint>

/// DateTime: simplified version of Adafruit RTClib DateTime
class DateTime {
public:
  DateTime(uint32_t epoch = 0) : _epoch(epoch) {
    // Simplified epoch to date conversion (Unix timestamp)
    uint32_t t = epoch;
    _second = t % 60;
    t /= 60;
    _minute = t % 60;
    t /= 60;
    _hour = t % 24;
    t /= 24;

    // Days since epoch: Jan 1 1970 was Thursday (day 4)
    _dayOfWeek = (t + 4) % 7;

    // Approximate date calculation
    int year = 1970;
    while (true) {
      int daysInYear = (year % 4 == 0) ? 366 : 365;
      if (t < (uint32_t)daysInYear)
        break;
      t -= daysInYear;
      year++;
    }
    _year = year;

    static const int daysInMonth[] = {31, 28, 31, 30, 31, 30,
                                      31, 31, 30, 31, 30, 31};
    int month = 0;
    for (; month < 12; month++) {
      int dim = daysInMonth[month];
      if (month == 1 && (year % 4 == 0))
        dim = 29;
      if (t < (uint32_t)dim)
        break;
      t -= dim;
    }
    _month = month + 1;
    _day = t + 1;
  }

  DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour = 0,
           uint8_t minute = 0, uint8_t second = 0)
      : _year(year), _month(month), _day(day), _hour(hour), _minute(minute),
        _second(second), _dayOfWeek(0) {
    // Compute day of week using Zeller-like formula
    // Simplified: just store what we need
    // For proper day-of-week, use Tomohiko Sakamoto's algorithm
    static const int t_[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = year;
    if (month < 3)
      y--;
    _dayOfWeek = (y + y / 4 - y / 100 + y / 400 + t_[month - 1] + day) % 7;

    _epoch = (uint32_t)(_daysFromCivil(year, month, day) * 86400L +
                        hour * 3600L + minute * 60L + second);
  }

  uint16_t year() const { return _year; }
  uint8_t month() const { return _month; }
  uint8_t day() const { return _day; }
  uint8_t hour() const { return _hour; }
  uint8_t minute() const { return _minute; }
  uint8_t second() const { return _second; }
  uint8_t dayOfTheWeek() const { return _dayOfWeek; }

  /// Seconds since 1970-01-01, the same value the real RTClib returns. The
  /// clock-sanity checks in TimeManager are written against it, so the mock
  /// has to carry it for both constructors rather than only for the one that
  /// was handed an epoch to begin with.
  uint32_t unixtime() const { return _epoch; }

private:
  /// Days from 1970-01-01 to the given civil date (Howard Hinnant's algorithm),
  /// correct across century leap rules — unlike the year%4 shortcut above, which
  /// only ever has to place a date inside the current decade.
  static long _daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (long)era * 146097 + (long)doe - 719468;
  }

  uint16_t _year;
  uint8_t _month, _day, _hour, _minute, _second, _dayOfWeek;
  uint32_t _epoch = 0;
};

/// RTC_DS3231 mock
class RTC_DS3231 {
public:
  bool begin() { return _present; }
  void adjust(const DateTime &dt) {
    _now = dt;
    _lostPower = false;
  }
  DateTime now() { return _now; }
  bool lostPower() { return _lostPower; }

  // Mock control
  void mock_setPresent(bool p) { _present = p; }
  void mock_setLostPower(bool lp) { _lostPower = lp; }
  void mock_setNow(const DateTime &dt) { _now = dt; }

private:
  bool _present = true;
  bool _lostPower = false;
  DateTime _now;
};
