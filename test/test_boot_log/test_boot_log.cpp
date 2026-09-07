// ============================================================================
// BootLog Unit Tests
// Tests: ring buffer, sequence numbering, NVS round-trip, uptime carry-over,
//        wall-clock stamping, JSON shape
// ============================================================================

#include "Arduino.h"
#include "BootLog.h"
#include "Preferences.h"
#include <unity.h>

void setUp() {
  mock_millis_value = 0;
  Preferences::mock_clearAll();
  bootLogMockReset();
}

void tearDown() {}

// --- First boot on a blank device ---

void test_first_boot_starts_at_seq_one() {
  bootLogBegin(ESP_RST_POWERON);

  TEST_ASSERT_EQUAL_UINT8(1, bootLogCount());
  BootLogEntry e = bootLogLast();
  TEST_ASSERT_EQUAL_UINT32(1, e.seq);
  TEST_ASSERT_EQUAL_UINT8(ESP_RST_POWERON, e.reason);
  TEST_ASSERT_EQUAL_UINT32(0, e.epoch);
}

// --- The log survives the reset, which is the whole point ---

void test_second_boot_appends_and_keeps_history() {
  bootLogBegin(ESP_RST_POWERON);
  bootLogBegin(ESP_RST_BROWNOUT);

  TEST_ASSERT_EQUAL_UINT8(2, bootLogCount());
  BootLogEntry e = bootLogLast();
  TEST_ASSERT_EQUAL_UINT32(2, e.seq);
  TEST_ASSERT_EQUAL_UINT8(ESP_RST_BROWNOUT, e.reason);
}

// --- Oldest entries fall off, sequence numbers do not restart ---

void test_ring_wraps_without_reusing_sequence_numbers() {
  const uint16_t boots = BOOT_LOG_MAX + 8;
  for (uint16_t i = 0; i < boots; i++) {
    bootLogBegin(ESP_RST_SW);
  }

  TEST_ASSERT_EQUAL_UINT8(BOOT_LOG_MAX, bootLogCount());
  TEST_ASSERT_EQUAL_UINT32(boots, bootLogLast().seq);
}

// --- How long the previous run lasted ---

void test_previous_uptime_carried_across_reset() {
  bootLogBegin(ESP_RST_POWERON);

  mock_millis_value = 5000;
  bootLogTick();

  bootLogBegin(ESP_RST_BROWNOUT);
  TEST_ASSERT_EQUAL_UINT32(5, bootLogLast().prevUptimeS);
}

void test_uptime_accumulates_across_several_ticks() {
  bootLogBegin(ESP_RST_POWERON);

  // Sub-second calls must not be lost to integer division: ticking at loop
  // rate would otherwise report an uptime of zero forever.
  for (uint16_t i = 1; i <= 20; i++) {
    mock_millis_value = i * 300UL;
    bootLogTick();
  }

  bootLogBegin(ESP_RST_PANIC);
  TEST_ASSERT_EQUAL_UINT32(6, bootLogLast().prevUptimeS); // 20 * 300ms
}

// --- Wall-clock stamping ---

void test_stamp_sets_epoch_once() {
  bootLogBegin(ESP_RST_BROWNOUT);
  TEST_ASSERT_EQUAL_UINT32(0, bootLogLast().epoch);

  bootLogStampTime(1788439467UL);
  TEST_ASSERT_EQUAL_UINT32(1788439467UL, bootLogLast().epoch);

  // A second stamp is what loop() does on every pass. It must not move the
  // boot's timestamp forward, or the entry would report "now" instead of when
  // the board actually came up.
  bootLogStampTime(1788449999UL);
  TEST_ASSERT_EQUAL_UINT32(1788439467UL, bootLogLast().epoch);
}

void test_stamp_ignores_invalid_clock() {
  bootLogBegin(ESP_RST_BROWNOUT);
  bootLogStampTime(0);
  TEST_ASSERT_EQUAL_UINT32(0, bootLogLast().epoch);
}

void test_stamped_epoch_survives_the_next_boot() {
  bootLogBegin(ESP_RST_POWERON);
  bootLogStampTime(1788439467UL);

  bootLogBegin(ESP_RST_BROWNOUT);

  String json = bootLogGetJSON();
  TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("1788439467"));
}

// --- JSON ---

void test_json_lists_newest_first() {
  bootLogBegin(ESP_RST_POWERON);
  bootLogBegin(ESP_RST_BROWNOUT);

  String json = bootLogGetJSON();
  TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("\"count\":2"));

  const int brownout = json.indexOf("BROWNOUT");
  const int poweron = json.indexOf("POWER_ON");
  TEST_ASSERT_NOT_EQUAL(-1, brownout);
  TEST_ASSERT_NOT_EQUAL(-1, poweron);
  TEST_ASSERT_TRUE(brownout < poweron);
}

void test_json_reports_unknown_uptime_as_null() {
  bootLogBegin(ESP_RST_POWERON);

  // 4294967295 in a field named "seconds" reads as a real 136-year uptime to
  // anything consuming this.
  String json = bootLogGetJSON();
  TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("\"prevUptimeS\":null"));
  TEST_ASSERT_EQUAL(-1, json.indexOf("4294967295"));
}

void test_json_empty_when_nothing_logged() {
  String json = bootLogGetJSON();
  TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("\"count\":0"));
  TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("\"boots\":[]"));
}

// --- Reason names ---

void test_reason_names() {
  TEST_ASSERT_EQUAL_STRING("BROWNOUT", bootResetReasonName(ESP_RST_BROWNOUT));
  TEST_ASSERT_EQUAL_STRING("PANIC_EXCEPTION", bootResetReasonName(ESP_RST_PANIC));
  TEST_ASSERT_EQUAL_STRING("TASK_WATCHDOG", bootResetReasonName(ESP_RST_TASK_WDT));
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", bootResetReasonName(200));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_first_boot_starts_at_seq_one);
  RUN_TEST(test_second_boot_appends_and_keeps_history);
  RUN_TEST(test_ring_wraps_without_reusing_sequence_numbers);
  RUN_TEST(test_previous_uptime_carried_across_reset);
  RUN_TEST(test_uptime_accumulates_across_several_ticks);
  RUN_TEST(test_stamp_sets_epoch_once);
  RUN_TEST(test_stamp_ignores_invalid_clock);
  RUN_TEST(test_stamped_epoch_survives_the_next_boot);
  RUN_TEST(test_json_lists_newest_first);
  RUN_TEST(test_json_reports_unknown_uptime_as_null);
  RUN_TEST(test_json_empty_when_nothing_logged);
  RUN_TEST(test_reason_names);

  UNITY_END();
  return 0;
}
