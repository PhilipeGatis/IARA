// ============================================================================
// FertManager Unit Tests
// Tests: dosing, NVS deduplication, stock tracking, timeout limits
// ============================================================================

#include "Arduino.h"
#include "FertManager.h"
#include <unity.h>

// Helper: create a FertManager with schedule set to 09:00 for all channels
static FertManager createFM(uint8_t schedHour = 9, uint8_t schedMin = 0) {
  FertManager fm;
  fm.begin();
  for (uint8_t ch = 0; ch <= NUM_FERTS; ch++) {
    for (uint8_t dow = 0; dow < 7; dow++) {
      fm.setScheduleTime(ch, dow, schedHour, schedMin);
      fm.setDoseML(ch, dow, DEFAULT_DOSE_ML);
    }
  }
  return fm;
}

// Reset all state before each test
void setUp() {
  Preferences::mock_clearAll();
  mock_reset_pins();
  mock_millis_value = 0;
  Preferences::mock_clearAll();
}

void tearDown() {}

// ----------------------------------------------------------------------------
// Date Key Deduplication
// ----------------------------------------------------------------------------

void test_not_dosed_initially() {
  FertManager fm = createFM();
  DateTime dt(2026, 2, 24, 9, 0, 0);
  TEST_ASSERT_FALSE(fm.wasDosedToday(dt));
}

void test_dose_marks_day_as_done() {
  FertManager fm = createFM();
  DateTime dt(2026, 2, 24, 9, 0, 0); // 09:00 matches schedule

  fm.update(dt); // Should dose all channels

  TEST_ASSERT_TRUE(fm.wasDosedToday(dt));
}

void test_no_double_dose_same_day() {
  FertManager fm = createFM();
  DateTime dt(2026, 2, 24, 9, 0, 0);

  fm.update(dt); // First call — should dose
  TEST_ASSERT_TRUE(fm.wasDosedToday(dt));

  // Reset millis to simulate the pump running again
  mock_millis_value = 0;
  fm.update(dt); // Second call same day — should NOT dose again
  // wasDosedToday should still be true (already dosed)
  TEST_ASSERT_TRUE(fm.wasDosedToday(dt));
}

void test_doses_on_different_day() {
  FertManager fm = createFM();
  DateTime day1(2026, 2, 24, 9, 0, 0);
  DateTime day2(2026, 2, 25, 9, 0, 0);

  fm.update(day1);
  TEST_ASSERT_TRUE(fm.wasDosedToday(day1));
  TEST_ASSERT_FALSE(fm.wasDosedToday(day2));
}

void test_dedup_survives_reboot() {
  // Simulate first boot: dose and save
  {
    FertManager fm = createFM();
    DateTime dt(2026, 2, 24, 9, 0, 0);
    fm.update(dt);
    fm.saveState();
  }

  // Simulate reboot: new FertManager instance loads from NVS
  {
    FertManager fm = createFM();
    DateTime dt(2026, 2, 24, 9, 0, 0);
    TEST_ASSERT_TRUE(fm.wasDosedToday(dt));
  }
}

// ----------------------------------------------------------------------------
// Schedule Matching
// ----------------------------------------------------------------------------

void test_no_dose_outside_schedule() {
  FertManager fm = createFM(9, 0);     // Schedule at 09:00
  DateTime dt(2026, 2, 24, 10, 30, 0); // 10:30 != 09:00
  fm.update(dt);
  TEST_ASSERT_FALSE(fm.wasDosedToday(dt)); // Should NOT have dosed
}

// ----------------------------------------------------------------------------
// Stock Tracking
// ----------------------------------------------------------------------------

void test_stock_decrements_after_dosing() {
  FertManager fm = createFM();

  float initialStock = fm.getStockML(0);
  uint8_t dow = 1; // Monday (2026-02-24 is a Tuesday=2, but let's use day1)
  DateTime dt(2026, 2, 24, 9, 0, 0); // Tuesday = dow 2
  float dose = fm.getDoseML(0, dt.dayOfTheWeek());

  fm.update(dt);

  float remaining = fm.getStockML(0);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, initialStock - dose, remaining);
}

void test_stock_reset() {
  FertManager fm = createFM();

  fm.setStockML(0, 10.0f); // Low stock
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 10.0f, fm.getStockML(0));

  fm.resetStock(0, 500.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 500.0f, fm.getStockML(0));
}

void test_skip_dose_when_empty_stock() {
  FertManager fm = createFM();

  // Empty stock for all channels
  for (int i = 0; i < 4; i++) {
    fm.setStockML(i, 0.0f);
  }

  DateTime dt(2026, 2, 24, 9, 0, 0);
  fm.update(dt);

  // Current behavior: insufficient stock → skipped, NOT marked as dosed
  // (only zero-dose days are marked as dosed to prevent retries)
  TEST_ASSERT_FALSE(fm.wasDosedToday(dt));
}

void test_stock_persists_across_reboot() {
  {
    FertManager fm = createFM();
    fm.setStockML(0, 42.0f);
    fm.saveState();
  }
  {
    FertManager fm = createFM();
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 42.0f, fm.getStockML(0));
  }
}

// ----------------------------------------------------------------------------
// Dose Volume Configuration
// ----------------------------------------------------------------------------

void test_set_and_get_dose() {
  FertManager fm;
  fm.begin();

  // Set dose for CH0 on Sunday (0)
  fm.setDoseML(0, 0, 7.5f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 7.5f, fm.getDoseML(0, 0));

  // Set dose for CH3 on Wednesday (3)
  fm.setDoseML(3, 3, 12.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.0f, fm.getDoseML(3, 3));
}

// ----------------------------------------------------------------------------
// Dosing GPIO Behavior
// ----------------------------------------------------------------------------

void test_dose_channel_activates_correct_pin() {
  FertManager fm = createFM();
  Preferences::mock_clearAll();
  mock_reset_pins();

  // Dose CH1 (PIN_FERT1 = GPIO 13)
  TEST_ASSERT_TRUE(fm.startDose(0, 1.0f)); // 1 ml
  TEST_ASSERT_TRUE(fm.isDosing());
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[13]); // pump running

  // The dose ends on the clock, not by blocking. Nothing switches the pump off
  // until tickDose() sees the duration elapse.
  mock_millis_value += 60UL * 1000;
  fm.tickDose();
  TEST_ASSERT_FALSE(fm.isDosing());
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[13]);
}

void test_dose_does_not_block_the_loop() {
  FertManager fm = createFM();
  Preferences::mock_clearAll();
  mock_reset_pins();

  const unsigned long before = mock_millis_value;
  fm.startDose(0, 30.0f); // a long dose

  // startDose() must return with the clock untouched. The blocking version sat
  // in delay() for the whole duration, and loop() — with it the overflow
  // watchdog and the emergency drain — did not run for up to 30 seconds while
  // a pump was live.
  TEST_ASSERT_EQUAL(before, mock_millis_value);
  TEST_ASSERT_TRUE(fm.isDosing());
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[13]);
}

void test_second_dose_on_same_channel_refused() {
  FertManager fm = createFM();
  Preferences::mock_clearAll();
  mock_reset_pins();

  TEST_ASSERT_TRUE(fm.startDose(0, 5.0f));
  // One channel, one dose: a second would run the same pump against a second
  // end time and deliver an unknown volume.
  TEST_ASSERT_FALSE(fm.startDose(0, 5.0f));

  fm.abortDose();
  TEST_ASSERT_FALSE(fm.isDosing());
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[13]);
  TEST_ASSERT_TRUE(fm.startDose(0, 5.0f));
}

void test_channels_dose_in_parallel() {
  FertManager fm = createFM();
  Preferences::mock_clearAll();
  mock_reset_pins();

  // Separate pins, separate LEDC channels, separate end times — nothing about
  // one channel's dose has any bearing on another's.
  TEST_ASSERT_TRUE(fm.startDose(0, 3.0f));  // 2 s at the default 1.5 mL/s
  TEST_ASSERT_TRUE(fm.startDose(1, 15.0f)); // 10 s
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_FERT1]);
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_FERT2]);

  // The short one ends on its own schedule and leaves the long one running.
  mock_millis_value += 2500;
  fm.tickDose();
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_FERT1]);
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_FERT2]);
  TEST_ASSERT_FALSE(fm.isDosing(0));
  TEST_ASSERT_TRUE(fm.isDosing(1));
  TEST_ASSERT_TRUE(fm.isDosing()); // something is still running

  mock_millis_value += 8000;
  fm.tickDose();
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_FERT2]);
  TEST_ASSERT_FALSE(fm.isDosing());
}

void test_schedule_staggers_the_pump_starts() {
  FertManager fm = createFM();
  Preferences::mock_clearAll();
  mock_reset_pins();

  // Every channel is due on the same minute. They run together, but they must
  // not leave stall together: five motors starting at the same instant is a
  // spike on the 12 V rail, not a scheduling requirement.
  DateTime dt(2026, 2, 24, 9, 0, 0);
  fm.update(dt);
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_FERT1]);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_FERT2]);

  // Still inside the window: nothing else starts, however often loop() calls.
  mock_millis_value += FERT_START_STAGGER_MS - 1;
  fm.update(dt);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_FERT2]);

  mock_millis_value += 2;
  fm.update(dt);
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_FERT2]);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_FERT3]);
  // The first one is still running while the second starts — staggered, not
  // serialised.
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_FERT1]);

  mock_millis_value += FERT_START_STAGGER_MS;
  fm.update(dt);
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_FERT3]);

  mock_millis_value += FERT_START_STAGGER_MS;
  fm.update(dt);
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_FERT4]);
}

void test_dose_now_fires_todays_schedule_off_hour() {
  FertManager fm = createFM(); // scheduled for 09:00
  Preferences::mock_clearAll();
  mock_reset_pins();

  DateTime dt(2026, 2, 24, 14, 30, 0); // nowhere near the scheduled hour

  // Nothing happens on its own at 14:30.
  fm.update(dt);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_FERT1]);

  // 4 ferts + Prime, all enabled with a volume for today.
  TEST_ASSERT_EQUAL(NUM_FERTS + 1, fm.doseTodayNow(dt));

  fm.update(dt);
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_FERT1]);
  TEST_ASSERT_TRUE(fm.wasDosedToday(dt));
}

void test_dose_now_does_not_double_a_dose_already_given() {
  FertManager fm = createFM();
  Preferences::mock_clearAll();
  mock_reset_pins();

  DateTime dt(2026, 2, 24, 9, 0, 0);
  // Let the whole schedule run, one stagger window at a time.
  for (uint8_t i = 0; i < NUM_FERTS + 1; i++) {
    fm.update(dt);
    mock_millis_value += FERT_START_STAGGER_MS;
  }
  const float stockAfterSchedule = fm.getStockML(0);

  // The button is pressed afterwards. Nothing is owing, so nothing runs.
  DateTime later(2026, 2, 24, 14, 0, 0);
  TEST_ASSERT_EQUAL(0, fm.doseTodayNow(later));
  fm.update(later);
  TEST_ASSERT_EQUAL_FLOAT(stockAfterSchedule, fm.getStockML(0));
}

void test_dose_now_all_repeats_a_channel_already_dosed() {
  FertManager fm = createFM();
  Preferences::mock_clearAll();
  mock_reset_pins();

  // The whole schedule runs at its hour.
  DateTime dt(2026, 2, 24, 9, 0, 0);
  for (uint8_t i = 0; i < NUM_FERTS + 1; i++) {
    fm.update(dt);
    mock_millis_value += FERT_START_STAGGER_MS;
  }
  mock_millis_value += 5000; // every dose runs out
  fm.tickDose();
  TEST_ASSERT_FALSE(fm.isDosing());
  const float stockAfterSchedule = fm.getStockML(0);
  mock_reset_pins();

  // The doses ran and delivered nothing — an unprimed line, a flow rate that is
  // wrong. Asked explicitly, the button repeats them despite the day's stamp.
  DateTime later(2026, 2, 24, 14, 0, 0);
  TEST_ASSERT_EQUAL(NUM_FERTS + 1, fm.doseTodayNow(later, true));
  fm.update(later);
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_FERT1]);
  TEST_ASSERT_EQUAL_FLOAT(stockAfterSchedule - DEFAULT_DOSE_ML,
                          fm.getStockML(0));

  // And it is spent afterwards: the repeat does not repeat itself.
  mock_millis_value += 10000;
  fm.tickDose();
  mock_reset_pins();
  fm.update(later);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_FERT1]);
}

void test_dose_now_does_not_survive_into_the_next_day() {
  FertManager fm = createFM();
  Preferences::mock_clearAll();
  mock_reset_pins();

  DateTime today(2026, 2, 24, 14, 0, 0);
  TEST_ASSERT_TRUE(fm.doseTodayNow(today) > 0);

  // The board was busy — a water change holds fertiliser dosing off — and the
  // request never got a pass before midnight. It must not fire tomorrow's
  // schedule at 00:00.
  DateTime tomorrow(2026, 2, 25, 0, 0, 0);
  fm.update(tomorrow);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_FERT1]);
  TEST_ASSERT_FALSE(fm.wasDosedToday(tomorrow));
}

void test_abort_stops_every_running_channel() {
  FertManager fm = createFM();
  Preferences::mock_clearAll();
  mock_reset_pins();

  TEST_ASSERT_TRUE(fm.startDose(0, 20.0f));
  TEST_ASSERT_TRUE(fm.startDose(2, 20.0f));
  fm.manualPump(3, true);

  // A stop that leaves the other pumps running is not a stop.
  fm.abortDose();
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_FERT1]);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_FERT3]);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_FERT4]);
  TEST_ASSERT_FALSE(fm.isDosing());
}

void test_manual_pump_stops_at_its_ceiling() {
  FertManager fm = createFM();
  Preferences::mock_clearAll();
  mock_reset_pins();

  fm.manualPump(0, true);
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[13]);

  // Still running well inside the ceiling.
  mock_millis_value += MANUAL_FERT_MAX_MS - 1000;
  fm.tickDose();
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[13]);

  // The browser that turned it on never sent the OFF request.
  mock_millis_value += 2000;
  fm.tickDose();
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[13]);
}

void test_dose_channel_rejects_invalid() {
  FertManager fm = createFM();

  // Channel > 4 should fail
  bool ok = fm.startDose(10, 5.0f);
  TEST_ASSERT_FALSE(ok);

  // Zero ml should fail
  ok = fm.startDose(0, 0.0f);
  TEST_ASSERT_FALSE(ok);
}

// ----------------------------------------------------------------------------
// Legacy NVS Migration
// ----------------------------------------------------------------------------

// Every channel carried its own settings in the pre-blob key layout, so every
// channel must come back after the upgrade — not just the first one.
void test_legacy_settings_migrate_on_every_channel() {
  // Seed the old per-key layout for CH2 (index 1): 7.5 mL every day at 14:30.
  Preferences legacy;
  legacy.begin("fert", false);
  for (uint8_t d = 0; d < 7; d++) {
    char key[16];
    snprintf(key, sizeof(key), "d1_%d", d);
    legacy.putFloat(key, 7.5f);
    snprintf(key, sizeof(key), "sH1_%d", d);
    legacy.putUChar(key, 14);
    snprintf(key, sizeof(key), "sM1_%d", d);
    legacy.putUChar(key, 30);
  }
  legacy.putFloat("stock1", 250.0f);
  legacy.putFloat("fR1", 2.0f);
  legacy.putUChar("pwm1", 200);

  FertManager fm;
  fm.begin(); // migrates the legacy keys into the per-channel blob

  TEST_ASSERT_EQUAL_FLOAT(7.5f, fm.getDoseML(1, 3));
  TEST_ASSERT_EQUAL_UINT8(14, fm.getSchedHour(1, 3));
  TEST_ASSERT_EQUAL_UINT8(30, fm.getSchedMinute(1, 3));
  TEST_ASSERT_EQUAL_FLOAT(250.0f, fm.getStockML(1));
  TEST_ASSERT_EQUAL_FLOAT(2.0f, fm.getFlowRate(1));
  TEST_ASSERT_EQUAL_UINT8(200, fm.getPWM(1));
}

// ----------------------------------------------------------------------------
// Bottle Capacity
// ----------------------------------------------------------------------------

void test_bottle_size_defaults_and_persists() {
  {
    FertManager fm = createFM();
    TEST_ASSERT_EQUAL_FLOAT(DEFAULT_STOCK_ML, fm.getCapacityML(1));
    fm.setCapacityML(1, 450.0f);
    TEST_ASSERT_EQUAL_FLOAT(450.0f, fm.getCapacityML(1));
  }

  FertManager reloaded;
  reloaded.begin();
  TEST_ASSERT_EQUAL_FLOAT(450.0f, reloaded.getCapacityML(1));
  TEST_ASSERT_EQUAL_FLOAT(DEFAULT_STOCK_ML, reloaded.getCapacityML(2));
}

void test_bottle_size_refuses_zero() {
  FertManager fm = createFM();
  fm.setCapacityML(1, 450.0f);
  fm.setCapacityML(1, 0.0f);
  // A zero would divide the stock bar by nothing, so the last real size stands.
  TEST_ASSERT_EQUAL_FLOAT(450.0f, fm.getCapacityML(1));
}

void test_settings_survive_a_blob_missing_the_bottle_size() {
  // What a device flashed before the field existed has in NVS: the same blob,
  // shorter by the appended value. Read strictly it would count as no blob at
  // all, and the channel would fall through to a migration whose keys are gone.
  {
    FertManager fm = createFM();
    fm.setStockML(1, 123.0f);
    fm.setFlowRate(1, 2.5f);
    fm.setPWM(1, 180);
    fm.setDoseML(1, 3, 4.5f);
    fm.saveState();
  }

  Preferences nvs;
  nvs.begin("fert", false);
  char blob[256];
  const size_t len = nvs.getBytes("ch1", blob, sizeof(blob));
  TEST_ASSERT_TRUE(len > sizeof(float));
  nvs.putBytes("ch1", blob, len - sizeof(float));

  FertManager fm;
  fm.begin();

  TEST_ASSERT_EQUAL_FLOAT(123.0f, fm.getStockML(1));
  TEST_ASSERT_EQUAL_FLOAT(2.5f, fm.getFlowRate(1));
  TEST_ASSERT_EQUAL_UINT8(180, fm.getPWM(1));
  TEST_ASSERT_EQUAL_FLOAT(4.5f, fm.getDoseML(1, 3));
  // Nothing stored means the volume the firmware used to assume.
  TEST_ASSERT_EQUAL_FLOAT(DEFAULT_STOCK_ML, fm.getCapacityML(1));
}

// ----------------------------------------------------------------------------
// Per-Channel Reset
// ----------------------------------------------------------------------------

void test_reset_channel_restores_defaults() {
  FertManager fm = createFM(14, 30);
  fm.setStockML(1, 42.0f);
  fm.setFlowRate(1, 3.3f);
  fm.setPWM(1, 90);
  fm.setEnabled(1, false);
  fm.setName(1, "Ferro");

  fm.resetChannel(1);

  TEST_ASSERT_EQUAL_FLOAT(0.0f, fm.getDoseML(1, 3));
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_FERT_HOUR, fm.getSchedHour(1, 3));
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_FERT_MINUTE, fm.getSchedMinute(1, 3));
  TEST_ASSERT_EQUAL_FLOAT(DEFAULT_STOCK_ML, fm.getStockML(1));
  TEST_ASSERT_EQUAL_FLOAT(FLOW_RATE_ML_PER_SEC, fm.getFlowRate(1));
  TEST_ASSERT_EQUAL_UINT8(255, fm.getPWM(1));
  TEST_ASSERT_TRUE(fm.isEnabled(1));
  TEST_ASSERT_EQUAL_STRING("CH2", fm.getName(1).c_str());
}

// Resetting one channel must not disturb its neighbours.
void test_reset_channel_leaves_others_alone() {
  FertManager fm = createFM(14, 30);
  fm.setStockML(0, 42.0f);
  fm.setPWM(0, 90);

  fm.resetChannel(1);

  TEST_ASSERT_EQUAL_FLOAT(DEFAULT_DOSE_ML, fm.getDoseML(0, 3));
  TEST_ASSERT_EQUAL_UINT8(14, fm.getSchedHour(0, 3));
  TEST_ASSERT_EQUAL_FLOAT(42.0f, fm.getStockML(0));
  TEST_ASSERT_EQUAL_UINT8(90, fm.getPWM(0));
}

void test_reset_channel_persists_across_reboot() {
  {
    FertManager fm = createFM(14, 30);
    fm.saveState();
    fm.resetChannel(1);
  }
  {
    FertManager fm;
    fm.begin(); // reload from NVS, no schedule re-applied
    TEST_ASSERT_EQUAL_FLOAT(0.0f, fm.getDoseML(1, 3));
    TEST_ASSERT_EQUAL_UINT8(DEFAULT_FERT_HOUR, fm.getSchedHour(1, 3));
    // Channel 0 kept what it had
    TEST_ASSERT_EQUAL_UINT8(14, fm.getSchedHour(0, 3));
  }
}

// A reset while the channel is pumping has to stop the pump: the volume being
// delivered was just erased from the books.
void test_reset_channel_stops_its_own_pump() {
  FertManager fm = createFM();
  TEST_ASSERT_TRUE(fm.startDose(1, 5.0f));
  TEST_ASSERT_TRUE(fm.isDosing());

  fm.resetChannel(1);

  TEST_ASSERT_FALSE(fm.isDosing());
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_FERT2]);
}

// ...but not a dose running on a different channel.
void test_reset_channel_does_not_stop_another_channels_dose() {
  FertManager fm = createFM();
  TEST_ASSERT_TRUE(fm.startDose(0, 5.0f));

  fm.resetChannel(1);

  TEST_ASSERT_TRUE(fm.isDosing());
}

// The stamp has to go too, otherwise a channel reset on a day it already dosed
// stays blocked until tomorrow.
void test_reset_channel_clears_dosed_today_stamp() {
  FertManager fm = createFM();
  DateTime dt(2026, 2, 24, 9, 0, 0);
  fm.update(dt);
  TEST_ASSERT_TRUE(fm.wasDosedToday(dt));

  fm.resetChannel(0);

  TEST_ASSERT_FALSE(fm.wasDosedToday(dt));
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
  UNITY_BEGIN();

  // Deduplication
  RUN_TEST(test_not_dosed_initially);
  RUN_TEST(test_dose_marks_day_as_done);
  RUN_TEST(test_no_double_dose_same_day);
  RUN_TEST(test_doses_on_different_day);
  RUN_TEST(test_dedup_survives_reboot);

  // Schedule matching
  RUN_TEST(test_no_dose_outside_schedule);

  // Stock tracking
  RUN_TEST(test_stock_decrements_after_dosing);
  RUN_TEST(test_stock_reset);
  RUN_TEST(test_skip_dose_when_empty_stock);
  RUN_TEST(test_stock_persists_across_reboot);

  // Dose volume config
  RUN_TEST(test_set_and_get_dose);

  // GPIO behavior
  RUN_TEST(test_dose_channel_activates_correct_pin);
  RUN_TEST(test_dose_does_not_block_the_loop);
  RUN_TEST(test_second_dose_on_same_channel_refused);
  RUN_TEST(test_channels_dose_in_parallel);
  RUN_TEST(test_schedule_staggers_the_pump_starts);
  RUN_TEST(test_dose_now_fires_todays_schedule_off_hour);
  RUN_TEST(test_dose_now_does_not_double_a_dose_already_given);
  RUN_TEST(test_dose_now_all_repeats_a_channel_already_dosed);
  RUN_TEST(test_dose_now_does_not_survive_into_the_next_day);
  RUN_TEST(test_abort_stops_every_running_channel);
  RUN_TEST(test_manual_pump_stops_at_its_ceiling);
  RUN_TEST(test_dose_channel_rejects_invalid);

  // Legacy NVS migration
  RUN_TEST(test_legacy_settings_migrate_on_every_channel);

  // Per-channel reset
  // Bottle capacity
  RUN_TEST(test_bottle_size_defaults_and_persists);
  RUN_TEST(test_bottle_size_refuses_zero);
  RUN_TEST(test_settings_survive_a_blob_missing_the_bottle_size);

  RUN_TEST(test_reset_channel_restores_defaults);
  RUN_TEST(test_reset_channel_leaves_others_alone);
  RUN_TEST(test_reset_channel_persists_across_reboot);
  RUN_TEST(test_reset_channel_stops_its_own_pump);
  RUN_TEST(test_reset_channel_does_not_stop_another_channels_dose);
  RUN_TEST(test_reset_channel_clears_dosed_today_stamp);

  UNITY_END();
  return 0;
}
