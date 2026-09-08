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

// --- Migration from the format already on the device ---
//
// These structs deliberately restate the v1 layout instead of sharing it with
// the implementation: they are what is actually sitting in NVS on a deployed
// board, so a test that moved with the code would prove nothing.

struct V1Entry {
    uint32_t seq;
    uint32_t epoch;
    uint32_t prevUptimeS;
    uint8_t reason;
    uint8_t pad[3];
};

struct V1Store {
    uint32_t magic;
    uint8_t version;
    uint8_t head;
    uint8_t count;
    uint8_t pad;
    V1Entry entries[32];
};

static void writeV1Store(uint8_t count) {
    V1Store v1;
    memset(&v1, 0, sizeof(v1));
    v1.magic = 0x424C4F47; // "BLOG"
    v1.version = 1;
    v1.count = count;
    v1.head = (uint8_t)(count % 32);
    for (uint8_t i = 0; i < count; i++) {
        v1.entries[i].seq = i + 1;
        v1.entries[i].epoch = 1788779563UL + i;
        v1.entries[i].prevUptimeS = 100 + i;
        v1.entries[i].reason = ESP_RST_BROWNOUT;
    }
    Preferences prefs;
    prefs.begin("bootlog", false);
    prefs.putBytes("ring", &v1, sizeof(v1));
    prefs.end();
}

void test_v1_history_survives_the_upgrade() {
    writeV1Store(5); // what the deployed board is carrying

    bootLogBegin(ESP_RST_BROWNOUT);

    TEST_ASSERT_EQUAL_UINT8(6, bootLogCount());
    TEST_ASSERT_EQUAL_UINT32(6, bootLogLast().seq);

    // The old entries kept their identity, and gained an empty snapshot rather
    // than a fabricated one.
    String json = bootLogGetJSON();
    TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("\"seq\":1"));
    TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("\"prevUptimeS\":104"));
    TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("\"rssi\":null"));
}

void test_v1_migration_keeps_the_newest_when_the_ring_shrinks() {
    writeV1Store(32); // v1 held 32; v2 holds BOOT_LOG_MAX

    bootLogBegin(ESP_RST_SW);

    TEST_ASSERT_EQUAL_UINT8(BOOT_LOG_MAX, bootLogCount());
    TEST_ASSERT_EQUAL_UINT32(33, bootLogLast().seq);

    // Oldest boots are the ones that fall off, not the newest.
    String json = bootLogGetJSON();
    TEST_ASSERT_EQUAL(-1, json.indexOf("\"seq\":1,"));
    TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("\"seq\":32,"));
}

void test_corrupt_store_starts_fresh_without_crashing() {
    Preferences prefs;
    prefs.begin("bootlog", false);
    const uint8_t junk[64] = {0xDE, 0xAD, 0xBE, 0xEF};
    prefs.putBytes("ring", junk, sizeof(junk));
    prefs.end();

    bootLogBegin(ESP_RST_PANIC);

    TEST_ASSERT_EQUAL_UINT8(1, bootLogCount());
    TEST_ASSERT_EQUAL_UINT32(1, bootLogLast().seq);
}

// --- Snapshot ---

void test_snapshot_carried_across_reset() {
  bootLogBegin(ESP_RST_POWERON);

  BootSnapshot snap{};
  snap.wifiState = (uint8_t)BootWifiState::CONNECTED;
  snap.rssi = -84;
  snap.wifiRetries = 7;
  snap.wifiReason = 201;
  snap.wifiDisconnects = 3;
  snap.wifiQuietS = 12;
  snap.sseClients = 2;
  snap.outputsMask = 0x0104;
  snap.freeHeapKb = 185;
  bootLogUpdateSnapshot(snap);

  bootLogBegin(ESP_RST_BROWNOUT);

  BootLogEntry e = bootLogLast();
  TEST_ASSERT_EQUAL_UINT8(ESP_RST_BROWNOUT, e.reason);
  TEST_ASSERT_EQUAL_INT8(-84, e.snap.rssi);
  TEST_ASSERT_EQUAL_UINT16(7, e.snap.wifiRetries);
  TEST_ASSERT_EQUAL_INT16(201, e.snap.wifiReason);
  TEST_ASSERT_EQUAL_UINT16(3, e.snap.wifiDisconnects);
  TEST_ASSERT_EQUAL_UINT8(2, e.snap.sseClients);
  TEST_ASSERT_EQUAL_UINT16(0x0104, e.snap.outputsMask);
  TEST_ASSERT_EQUAL_UINT16(185, e.snap.freeHeapKb);
}

void test_snapshot_reset_for_the_new_run() {
  bootLogBegin(ESP_RST_POWERON);
  BootSnapshot snap{};
  snap.rssi = -70;
  snap.sseClients = 4;
  bootLogUpdateSnapshot(snap);

  bootLogBegin(ESP_RST_BROWNOUT);
  // The second boot consumed it. A third must not inherit the same values, or
  // every entry after a busy run would blame the radio for it.
  bootLogBegin(ESP_RST_BROWNOUT);

  TEST_ASSERT_EQUAL_INT8(0, bootLogLast().snap.rssi);
  TEST_ASSERT_EQUAL_UINT8(0, bootLogLast().snap.sseClients);
}

void test_json_reports_missing_rssi_as_null() {
  bootLogBegin(ESP_RST_POWERON);
  String json = bootLogGetJSON();
  TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("\"rssi\":null"));
}

void test_json_carries_snapshot_fields() {
  bootLogBegin(ESP_RST_POWERON);
  BootSnapshot snap{};
  snap.rssi = -84;
  snap.sseClients = 2;
  bootLogUpdateSnapshot(snap);
  bootLogBegin(ESP_RST_BROWNOUT);

  String json = bootLogGetJSON();
  TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("\"rssi\":-84"));
  TEST_ASSERT_NOT_EQUAL(-1, json.indexOf("\"sse\":2"));
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
  RUN_TEST(test_v1_history_survives_the_upgrade);
  RUN_TEST(test_v1_migration_keeps_the_newest_when_the_ring_shrinks);
  RUN_TEST(test_corrupt_store_starts_fresh_without_crashing);
  RUN_TEST(test_snapshot_carried_across_reset);
  RUN_TEST(test_snapshot_reset_for_the_new_run);
  RUN_TEST(test_json_reports_missing_rssi_as_null);
  RUN_TEST(test_json_carries_snapshot_fields);
  RUN_TEST(test_reason_names);

  UNITY_END();
  return 0;
}
