// ============================================================================
// SafetyWatchdog Unit Tests
// Tests: sensor reads, emergency actions, maintenance mode, GPIO state
// ============================================================================

#include "Arduino.h"
#include "SafetyWatchdog.h"
#include <unity.h>

void setUp() {
  mock_reset_pins();
  mock_millis_value = 0;
  mock_pulseIn_value = 0;
}

void tearDown() {}

// ----------------------------------------------------------------------------
// Initialization
// ----------------------------------------------------------------------------

void test_begin_sets_pin_modes() {
  SafetyWatchdog sw;
  sw.begin();

  TEST_ASSERT_EQUAL(OUTPUT, mock_pin_mode[PIN_TRIG]);
  TEST_ASSERT_EQUAL(INPUT, mock_pin_mode[PIN_ECHO]);
  TEST_ASSERT_EQUAL(INPUT_PULLUP, mock_pin_mode[PIN_OPTICAL]);
  TEST_ASSERT_EQUAL(INPUT_PULLDOWN, mock_pin_mode[PIN_FLOAT]);
}

// ----------------------------------------------------------------------------
// Optical Sensor
// ----------------------------------------------------------------------------

void test_optical_low_means_water_detected() {
  SafetyWatchdog sw;
  sw.begin();

  // Active LOW: LOW = water at max level
  mock_pin_read_value[PIN_OPTICAL] = LOW;
  TEST_ASSERT_TRUE(sw.isOpticalHigh());
}

void test_optical_high_means_normal() {
  SafetyWatchdog sw;
  sw.begin();

  mock_pin_read_value[PIN_OPTICAL] = HIGH;
  TEST_ASSERT_FALSE(sw.isOpticalHigh());
}

// ----------------------------------------------------------------------------
// Float Switch
// ----------------------------------------------------------------------------

void test_float_high_means_reservoir_full() {
  SafetyWatchdog sw;
  sw.begin();

  mock_pin_read_value[PIN_FLOAT] = HIGH;
  TEST_ASSERT_TRUE(sw.isReservoirFull());
}

void test_float_low_means_reservoir_empty() {
  SafetyWatchdog sw;
  sw.begin();

  mock_pin_read_value[PIN_FLOAT] = LOW;
  TEST_ASSERT_FALSE(sw.isReservoirFull());
}

// ----------------------------------------------------------------------------
// Emergency Shutdown
// ----------------------------------------------------------------------------

void test_emergency_shutdown_all_pins_low() {
  SafetyWatchdog sw;
  sw.begin();

  // Set some pins HIGH first
  digitalWrite(PIN_DRAIN, HIGH);
  digitalWrite(PIN_REFILL, HIGH);
  digitalWrite(PIN_SOLENOID, HIGH);
  digitalWrite(PIN_CANISTER, HIGH);

  sw.emergencyShutdown();

  // All output pins must be LOW
  for (uint8_t i = 0; i < NUM_OUTPUT_PINS; i++) {
    TEST_ASSERT_EQUAL_MESSAGE(LOW, mock_pin_state[OUTPUT_PINS[i]],
                              "Pin not LOW after emergency shutdown");
  }
  TEST_ASSERT_TRUE(sw.isEmergency());
}

// ----------------------------------------------------------------------------
// Emergency Drain
// ----------------------------------------------------------------------------

void test_emergency_drain_opens_drain_only() {
  SafetyWatchdog sw;
  sw.begin();

  // Set everything HIGH
  for (uint8_t i = 0; i < NUM_OUTPUT_PINS; i++) {
    digitalWrite(OUTPUT_PINS[i], HIGH);
  }

  sw.emergencyDrain();

  // Only drain should be HIGH
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_DRAIN]);

  // Everything else should be LOW
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_REFILL]);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_SOLENOID]);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_CANISTER]);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_FERT1]);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_PRIME]);

  TEST_ASSERT_TRUE(sw.isEmergency());
}

// ----------------------------------------------------------------------------
// Maintenance Mode
// ----------------------------------------------------------------------------

void test_maintenance_mode_toggles() {
  SafetyWatchdog sw;
  sw.begin();

  TEST_ASSERT_FALSE(sw.isMaintenanceMode());

  sw.enterMaintenance();
  TEST_ASSERT_TRUE(sw.isMaintenanceMode());

  sw.exitMaintenance();
  TEST_ASSERT_FALSE(sw.isMaintenanceMode());
}

void test_maintenance_auto_expires() {
  SafetyWatchdog sw;
  sw.begin();

  sw.enterMaintenance();
  TEST_ASSERT_TRUE(sw.isMaintenanceMode());

  // Advance past 30 minutes + safety check interval
  mock_millis_value = MAINTENANCE_DURATION_MS + SAFETY_CHECK_INTERVAL_MS + 1;

  sw.update(); // Should auto-expire

  TEST_ASSERT_FALSE(sw.isMaintenanceMode());
}

void test_maintenance_persists_within_duration() {
  SafetyWatchdog sw;
  sw.begin();

  sw.enterMaintenance();

  // Advance only 15 minutes
  mock_millis_value = 15UL * 60 * 1000 + SAFETY_CHECK_INTERVAL_MS + 1;

  sw.update();

  TEST_ASSERT_TRUE(sw.isMaintenanceMode()); // Still active
}

// ----------------------------------------------------------------------------
// Ultrasonic (with mock pulseIn)
// ----------------------------------------------------------------------------

void test_ultrasonic_valid_reading() {
  SafetyWatchdog sw;
  sw.begin();

  // Simulate 15cm distance: duration = (15 * 2) / 0.0343 ≈ 875 us
  mock_pulseIn_value = 875;

  float dist = sw.readUltrasonic();
  // Should be approximately 15 cm
  TEST_ASSERT_FLOAT_WITHIN(2.0f, 15.0f, dist);
}

void test_ultrasonic_no_reading_returns_last() {
  SafetyWatchdog sw;
  sw.begin();

  // First: valid reading
  mock_pulseIn_value = 875; // ~15cm
  sw.readUltrasonic();

  // Then: no echo
  mock_pulseIn_value = 0;
  float dist = sw.readUltrasonic();

  // Should return last valid
  TEST_ASSERT_FLOAT_WITHIN(2.0f, 15.0f, dist);
}

// ----------------------------------------------------------------------------
// Optical Overflow Flag
// ----------------------------------------------------------------------------

void test_optical_flag_set_on_update() {
  SafetyWatchdog sw;
  mock_pulseIn_value = 1750; // Ensure sensor appears connected on begin()
  sw.begin();

  // Set ultrasonic to a safe distance (so overflow check doesn't trigger)
  mock_pulseIn_value = 1750; // ~30cm — safe

  // Optical sensor triggered
  mock_pin_read_value[PIN_OPTICAL] = LOW;

  mock_millis_value = SAFETY_CHECK_INTERVAL_MS + 1;
  sw.update();

  TEST_ASSERT_TRUE(sw.overflowDetected());

  // Refill and solenoid should be forced LOW
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_REFILL]);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_SOLENOID]);
}

void test_no_overflow_when_optical_clear() {
  SafetyWatchdog sw;
  mock_pulseIn_value = 1750; // Ensure sensor appears connected on begin()
  sw.begin();

  mock_pulseIn_value = 1750;
  mock_pin_read_value[PIN_OPTICAL] = HIGH; // Normal

  mock_millis_value = SAFETY_CHECK_INTERVAL_MS + 1;
  sw.update();

  TEST_ASSERT_FALSE(sw.overflowDetected());
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
  UNITY_BEGIN();

  // Initialization
  RUN_TEST(test_begin_sets_pin_modes);

  // Optical sensor
  RUN_TEST(test_optical_low_means_water_detected);
  RUN_TEST(test_optical_high_means_normal);

  // Float switch
  RUN_TEST(test_float_high_means_reservoir_full);
  RUN_TEST(test_float_low_means_reservoir_empty);

  // Emergency
  RUN_TEST(test_emergency_shutdown_all_pins_low);
  RUN_TEST(test_emergency_drain_opens_drain_only);

  // Maintenance mode
  RUN_TEST(test_maintenance_mode_toggles);
  RUN_TEST(test_maintenance_auto_expires);
  RUN_TEST(test_maintenance_persists_within_duration);

  // Ultrasonic
  RUN_TEST(test_ultrasonic_valid_reading);
  RUN_TEST(test_ultrasonic_no_reading_returns_last);

  // Overflow flags
  RUN_TEST(test_optical_flag_set_on_update);
  RUN_TEST(test_no_overflow_when_optical_clear);

  UNITY_END();
  return 0;
}
