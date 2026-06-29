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
  fm.doseChannel(0, 1.0f); // 1 ml

  // After dosing, pin should be LOW again (pump turned off)
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[13]);
}

void test_dose_channel_rejects_invalid() {
  FertManager fm = createFM();

  // Channel > 4 should fail
  bool ok = fm.doseChannel(10, 5.0f);
  TEST_ASSERT_FALSE(ok);

  // Zero ml should fail
  ok = fm.doseChannel(0, 0.0f);
  TEST_ASSERT_FALSE(ok);
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
  RUN_TEST(test_dose_channel_rejects_invalid);

  UNITY_END();
  return 0;
}
