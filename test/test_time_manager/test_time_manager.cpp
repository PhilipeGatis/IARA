// ============================================================================
// TimeManager Unit Tests
// Tests: clock trust, DateTime validation, schedule matching, time formatting
// NOTE: the schedule-matching tests below work through DateTime directly. The
//       clock-trust tests drive the real TimeManager against the RTC and NTP
//       mocks, which carry the controls those cases need.
// ============================================================================

#include "Arduino.h"
#include "RTClib.h" // DateTime mock
#include "TimeManager.h"
#include <unity.h>

/// What NTPClient hands back on a board where no packet ever arrived: the
/// cached epoch is still zero, so the answer is the bare UTC offset, and -10800
/// in 32-bit unsigned arithmetic is this. It reads as February 2106.
static const uint32_t NTP_UNSYNCED_UNDERFLOW = 4294956496UL;

/// A real reading: 2026-09-18 08:26:15 in the tank's own timezone.
static const uint32_t REAL_LOCAL_EPOCH = 1789730775UL;

void setUp() {
  mock_reset_pins();
  mock_millis_value = 0;
}

void tearDown() {}

// Helper: simulate isDailyScheduleTime logic (same as TimeManager)
bool isDailyScheduleTime(DateTime dt, uint8_t hour, uint8_t minute) {
  return (dt.hour() == hour && dt.minute() == minute);
}

// Helper: simulate isWeeklyScheduleDay logic (same as TimeManager)
bool isWeeklyScheduleDay(DateTime dt, uint8_t dayOfWeek, uint8_t hour,
                         uint8_t minute) {
  return (dt.dayOfTheWeek() == dayOfWeek && dt.hour() == hour &&
          dt.minute() == minute);
}

// ----------------------------------------------------------------------------
// DateTime (mock) Tests — verify mock is correct before using it
// ----------------------------------------------------------------------------

void test_datetime_components() {
  DateTime dt(2026, 2, 24, 9, 30, 45);
  TEST_ASSERT_EQUAL(2026, dt.year());
  TEST_ASSERT_EQUAL(2, dt.month());
  TEST_ASSERT_EQUAL(24, dt.day());
  TEST_ASSERT_EQUAL(9, dt.hour());
  TEST_ASSERT_EQUAL(30, dt.minute());
  TEST_ASSERT_EQUAL(45, dt.second());
}

void test_datetime_day_of_week_sunday() {
  // Feb 22, 2026 is a Sunday (0)
  DateTime dt(2026, 2, 22, 0, 0, 0);
  TEST_ASSERT_EQUAL(0, dt.dayOfTheWeek());
}

void test_datetime_day_of_week_tuesday() {
  // Feb 24, 2026 is a Tuesday (2)
  DateTime dt(2026, 2, 24, 0, 0, 0);
  TEST_ASSERT_EQUAL(2, dt.dayOfTheWeek());
}

void test_datetime_day_of_week_saturday() {
  // Feb 28, 2026 is a Saturday (6)
  DateTime dt(2026, 2, 28, 0, 0, 0);
  TEST_ASSERT_EQUAL(6, dt.dayOfTheWeek());
}

// ----------------------------------------------------------------------------
// Schedule: Daily
// ----------------------------------------------------------------------------

void test_daily_schedule_match() {
  DateTime dt(2026, 2, 24, 9, 0, 0);
  TEST_ASSERT_TRUE(isDailyScheduleTime(dt, 9, 0));
}

void test_daily_schedule_no_match_wrong_hour() {
  DateTime dt(2026, 2, 24, 10, 0, 0);
  TEST_ASSERT_FALSE(isDailyScheduleTime(dt, 9, 0));
}

void test_daily_schedule_no_match_wrong_minute() {
  DateTime dt(2026, 2, 24, 9, 1, 0);
  TEST_ASSERT_FALSE(isDailyScheduleTime(dt, 9, 0));
}

void test_daily_schedule_midnight() {
  DateTime dt(2026, 2, 24, 0, 0, 0);
  TEST_ASSERT_TRUE(isDailyScheduleTime(dt, 0, 0));
}

void test_daily_schedule_end_of_day() {
  DateTime dt(2026, 2, 24, 23, 59, 0);
  TEST_ASSERT_TRUE(isDailyScheduleTime(dt, 23, 59));
}

// ----------------------------------------------------------------------------
// Schedule: Weekly
// ----------------------------------------------------------------------------

void test_weekly_schedule_match() {
  // Tuesday Feb 24, 2026 at 10:00
  DateTime dt(2026, 2, 24, 10, 0, 0);
  TEST_ASSERT_TRUE(isWeeklyScheduleDay(dt, 2, 10, 0)); // Tuesday=2
}

void test_weekly_schedule_wrong_day() {
  // Tuesday but schedule is for Sunday
  DateTime dt(2026, 2, 24, 10, 0, 0);
  TEST_ASSERT_FALSE(isWeeklyScheduleDay(dt, 0, 10, 0)); // Sunday=0
}

void test_weekly_schedule_right_day_wrong_time() {
  DateTime dt(2026, 2, 24, 11, 0, 0);
  TEST_ASSERT_FALSE(isWeeklyScheduleDay(dt, 2, 10, 0));
}

void test_weekly_schedule_sunday() {
  // Feb 22, 2026 is Sunday
  DateTime dt(2026, 2, 22, 10, 0, 0);
  TEST_ASSERT_TRUE(isWeeklyScheduleDay(dt, 0, 10, 0));
}

// ----------------------------------------------------------------------------
// Time Formatting
// ----------------------------------------------------------------------------

void test_format_time() {
  DateTime dt(2026, 2, 24, 9, 5, 3);
  char buf[22];
  snprintf(buf, sizeof(buf), "%04d/%02d/%02d %02d:%02d:%02d", dt.year(),
           dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());

  TEST_ASSERT_EQUAL_STRING("2026/02/24 09:05:03", buf);
}

void test_format_time_midnight() {
  DateTime dt(2026, 1, 1, 0, 0, 0);
  char buf[22];
  snprintf(buf, sizeof(buf), "%04d/%02d/%02d %02d:%02d:%02d", dt.year(),
           dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());

  TEST_ASSERT_EQUAL_STRING("2026/01/01 00:00:00", buf);
}


// ----------------------------------------------------------------------------
// Clock trust
//
// A board whose DS3231 had dropped off the bus and whose NTP never landed a
// packet reported timeValid=true for hours while now() read a frozen date, so
// the 08:30 water change never matched its minute and nothing anywhere said
// why. These pin down what may be treated as a real instant.
// ----------------------------------------------------------------------------

void test_epoch_window_rejects_every_clock_this_board_has_faked() {
  TEST_ASSERT_TRUE(TimeManager::isEpochSane(REAL_LOCAL_EPOCH));

  TEST_ASSERT_FALSE(TimeManager::isEpochSane(0));
  // The NTP offset underflow, and what it wraps down to three hours into a run.
  TEST_ASSERT_FALSE(TimeManager::isEpochSane(NTP_UNSYNCED_UNDERFLOW));
  TEST_ASSERT_FALSE(TimeManager::isEpochSane(5294));
  // A DS3231 with a flat backup battery.
  TEST_ASSERT_FALSE(TimeManager::isEpochSane(DateTime(2000, 1, 1).unixtime()));
  // TimeManager's own "no idea" sentinel must never pass for a time.
  TEST_ASSERT_FALSE(TimeManager::isEpochSane(DateTime(2025, 1, 1).unixtime()));
}

void test_ntp_that_never_answered_is_not_a_valid_clock() {
  TimeManager tm;
  tm.mockRtc().mock_setPresent(false);
  tm.mockNtp().mock_setUpdateOk(false);

  tm.begin();

  TEST_ASSERT_FALSE(tm.syncWithNTP());
  TEST_ASSERT_FALSE(tm.isTimeValid());
}

void test_ntp_offset_underflow_is_not_a_valid_clock() {
  TimeManager tm;
  tm.mockRtc().mock_setPresent(false);
  // The packet "arrives", but nothing was ever set: this is the exact state
  // that was accepted as a sync and latched the clock as trustworthy.
  tm.mockNtp().mock_setUpdateOk(true);
  tm.mockNtp().mock_setEpoch(0);

  tm.begin();

  TEST_ASSERT_EQUAL_UINT32(NTP_UNSYNCED_UNDERFLOW,
                           (uint32_t)tm.mockNtp().getEpochTime());
  TEST_ASSERT_FALSE(tm.syncWithNTP());
  TEST_ASSERT_FALSE(tm.isTimeValid());
}

void test_real_ntp_reading_is_accepted() {
  TimeManager tm;
  tm.mockRtc().mock_setPresent(false);
  // The client applies the offset itself, so feed it UTC.
  tm.mockNtp().mock_setEpoch(REAL_LOCAL_EPOCH + 10800UL);

  tm.begin();

  TEST_ASSERT_TRUE(tm.syncWithNTP());
  TEST_ASSERT_TRUE(tm.isTimeValid());
  TEST_ASSERT_EQUAL_UINT32(REAL_LOCAL_EPOCH, tm.now().unixtime());
}

void test_rtc_that_lost_power_is_not_a_valid_clock() {
  TimeManager tm;
  tm.mockRtc().mock_setPresent(true);
  tm.mockRtc().mock_setLostPower(true);
  tm.mockRtc().mock_setNow(DateTime(2000, 1, 1));
  tm.mockNtp().mock_setUpdateOk(false);

  tm.begin();

  TEST_ASSERT_TRUE(tm.isRtcConnected());
  TEST_ASSERT_FALSE(tm.isTimeValid());
}

void test_rtc_reading_outside_the_window_is_refused_even_when_it_claims_health() {
  TimeManager tm;
  tm.mockRtc().mock_setPresent(true);
  // lostPower() says the module is fine; the reading says otherwise, which is
  // what a half-seated module does — it answers, with zeros.
  tm.mockRtc().mock_setLostPower(false);
  tm.mockRtc().mock_setNow(DateTime(2000, 1, 1));
  tm.mockNtp().mock_setUpdateOk(false);

  tm.begin();

  TEST_ASSERT_FALSE(tm.isTimeValid());
  // Not the year-2000 reading: now() falls through to NTP, which has nothing
  // either, so what comes back is the sentinel.
  TEST_ASSERT_EQUAL_UINT32(DateTime(2025, 1, 1).unixtime(), tm.now().unixtime());
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
  UNITY_BEGIN();

  // Clock trust
  RUN_TEST(test_epoch_window_rejects_every_clock_this_board_has_faked);
  RUN_TEST(test_ntp_that_never_answered_is_not_a_valid_clock);
  RUN_TEST(test_ntp_offset_underflow_is_not_a_valid_clock);
  RUN_TEST(test_real_ntp_reading_is_accepted);
  RUN_TEST(test_rtc_that_lost_power_is_not_a_valid_clock);
  RUN_TEST(test_rtc_reading_outside_the_window_is_refused_even_when_it_claims_health);

  // DateTime mock validation
  RUN_TEST(test_datetime_components);
  RUN_TEST(test_datetime_day_of_week_sunday);
  RUN_TEST(test_datetime_day_of_week_tuesday);
  RUN_TEST(test_datetime_day_of_week_saturday);

  // Daily schedule
  RUN_TEST(test_daily_schedule_match);
  RUN_TEST(test_daily_schedule_no_match_wrong_hour);
  RUN_TEST(test_daily_schedule_no_match_wrong_minute);
  RUN_TEST(test_daily_schedule_midnight);
  RUN_TEST(test_daily_schedule_end_of_day);

  // Weekly schedule
  RUN_TEST(test_weekly_schedule_match);
  RUN_TEST(test_weekly_schedule_wrong_day);
  RUN_TEST(test_weekly_schedule_right_day_wrong_time);
  RUN_TEST(test_weekly_schedule_sunday);

  // Formatting
  RUN_TEST(test_format_time);
  RUN_TEST(test_format_time_midnight);

  UNITY_END();
  return 0;
}
