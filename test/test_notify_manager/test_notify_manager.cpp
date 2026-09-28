// ============================================================================
// NotifyManager Unit Tests
// Tests: rate limiting, cooldown, per-type toggles, daily counter
// ============================================================================

#include "Arduino.h"
#include "NotifyManager.h"
#include "Preferences.h"
#include <unity.h>

void setUp() {
  mock_millis_value = 0;
  Preferences::mock_clearAll();
}

void tearDown() {}

// --- Disabled when no key ---

void test_disabled_when_no_key() {
  NotifyManager nm;
  nm.begin();
  // No key set → isEnabled() should be false
  TEST_ASSERT_FALSE(nm.isEnabled());
}

// --- Enabled when key set ---

void test_enabled_when_key_set() {
  NotifyManager nm;
  nm.begin();
  nm.setTopic("TEST_KEY_123");
  TEST_ASSERT_TRUE(nm.isEnabled());
}

// --- Can send when enabled ---

void test_can_send_when_enabled() {
  NotifyManager nm;
  nm.begin();
  nm.setTopic("TEST_KEY_123");
  // All types enabled by default
  TEST_ASSERT_TRUE(nm.isTypeEnabled(NOTIFY_TPA_COMPLETE));
  TEST_ASSERT_TRUE(nm.isTypeEnabled(NOTIFY_EMERGENCY));
}

// --- Type toggle ---

void test_type_toggle_disables_notification() {
  NotifyManager nm;
  nm.begin();
  nm.setTopic("TEST_KEY_123");
  nm.setTypeEnabled(NOTIFY_FERT_COMPLETE, false);
  TEST_ASSERT_FALSE(nm.isTypeEnabled(NOTIFY_FERT_COMPLETE));
  // Other types should still be enabled
  TEST_ASSERT_TRUE(nm.isTypeEnabled(NOTIFY_TPA_COMPLETE));
}

// --- Daily report config ---

void test_daily_report_config() {
  NotifyManager nm;
  nm.begin();
  nm.setDailyReportHour(10, 30);
  TEST_ASSERT_EQUAL(10, nm.getDailyReportHour());
  TEST_ASSERT_EQUAL(30, nm.getDailyReportMinute());
}

// --- Default daily report ---

void test_default_daily_report_time() {
  NotifyManager nm;
  nm.begin();
  TEST_ASSERT_EQUAL(8, nm.getDailyReportHour());
  TEST_ASSERT_EQUAL(0, nm.getDailyReportMinute());
}

// --- Daily count starts at zero ---

void test_daily_count_starts_at_zero() {
  NotifyManager nm;
  nm.begin();
  TEST_ASSERT_EQUAL(0, nm.getDailyCount());
}

// --- All types enabled by default ---

void test_all_types_enabled_by_default() {
  NotifyManager nm;
  nm.begin();
  for (uint8_t i = 0; i < NOTIFY_TYPE_COUNT; i++) {
    TEST_ASSERT_TRUE(nm.isTypeEnabled((NotifyType)i));
  }
}

// --- Invalid type returns false ---

void test_invalid_type_returns_false() {
  NotifyManager nm;
  nm.begin();
  TEST_ASSERT_FALSE(nm.isTypeEnabled((NotifyType)99));
}

// --- Key persistence ---

void test_key_cleared() {
  NotifyManager nm;
  nm.begin();
  nm.setTopic("SOME_KEY");
  TEST_ASSERT_TRUE(nm.isEnabled());
  nm.setTopic("");
  TEST_ASSERT_FALSE(nm.isEnabled());
}

// --- Toggle masks saved before a type existed ---

void test_new_type_enabled_under_old_mask() {
  // What a board flashed before NOTIFY_TPA_START wrote: six types, the
  // low-stock toggle off, and no type count.
  Preferences p;
  p.begin("notify", false);
  p.putUChar("mask", 0x3F & ~(1 << NOTIFY_FERT_LOW_STOCK));
  p.end();

  NotifyManager nm;
  nm.begin();
  TEST_ASSERT_TRUE(nm.isTypeEnabled(NOTIFY_TPA_START));
  TEST_ASSERT_FALSE(nm.isTypeEnabled(NOTIFY_FERT_LOW_STOCK));
  TEST_ASSERT_TRUE(nm.isTypeEnabled(NOTIFY_TPA_COMPLETE));
}

void test_new_type_toggle_persists() {
  NotifyManager nm;
  nm.begin();
  nm.setTypeEnabled(NOTIFY_TPA_START, false);

  NotifyManager reloaded;
  reloaded.begin();
  TEST_ASSERT_FALSE(reloaded.isTypeEnabled(NOTIFY_TPA_START));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_disabled_when_no_key);
  RUN_TEST(test_enabled_when_key_set);
  RUN_TEST(test_can_send_when_enabled);
  RUN_TEST(test_type_toggle_disables_notification);
  RUN_TEST(test_daily_report_config);
  RUN_TEST(test_default_daily_report_time);
  RUN_TEST(test_daily_count_starts_at_zero);
  RUN_TEST(test_all_types_enabled_by_default);
  RUN_TEST(test_invalid_type_returns_false);
  RUN_TEST(test_key_cleared);
  RUN_TEST(test_new_type_enabled_under_old_mask);
  RUN_TEST(test_new_type_toggle_persists);

  UNITY_END();
  return 0;
}
