// ============================================================================
// SafetyWatchdog Unit Tests
// Tests: sensor reads, emergency actions, maintenance mode, GPIO state,
//        median filter, UART A02YYUW protocol
// ============================================================================

#include "Arduino.h"
#include "SafetyWatchdog.h"
#include <unity.h>

void setUp() {
  mock_reset_pins();
  mock_millis_value = 0;
  Serial2.mock_clear();
}

void tearDown() {}

// ----------------------------------------------------------------------------
// Initialization
// ----------------------------------------------------------------------------

void test_begin_sets_pin_modes() {
  SafetyWatchdog sw;
  sw.begin();

  TEST_ASSERT_EQUAL(INPUT_PULLUP, mock_pin_mode[PIN_FLOAT]);
}

// ----------------------------------------------------------------------------
// Float Switch
// ----------------------------------------------------------------------------

void test_float_low_means_reservoir_full() {
  SafetyWatchdog sw;
  sw.begin();
  
  // Active LOW with pullup: LOW = float triggered = reservoir full
  mock_pin_read_value[PIN_FLOAT] = LOW;
  TEST_ASSERT_TRUE(sw.isReservoirFull());
}

void test_float_high_means_reservoir_empty() {
  SafetyWatchdog sw;
  sw.begin();

  mock_pin_read_value[PIN_FLOAT] = HIGH;
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

  // Every actuator must be in its INACTIVE state — which is not the same as
  // LOW for all of them. The canister runs on an active-LOW SSR, so driving it
  // LOW would start a mains pump at the moment of a shutdown.
  for (uint8_t i = 0; i < NUM_OUTPUT_PINS; i++) {
    const uint8_t pin = OUTPUT_PINS[i];
    if (pin == PIN_CANISTER) continue;
    TEST_ASSERT_EQUAL_MESSAGE(LOW, mock_pin_state[pin],
                              "Pump pin not LOW after emergency shutdown");
  }
  TEST_ASSERT_EQUAL_MESSAGE(HIGH, mock_pin_state[PIN_CANISTER],
                            "Canister must be OFF (SSR HIGH) after shutdown");
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

  // Everything else off — and the canister OFF means SSR HIGH. Draining with
  // the filter running is how its intake ends up above the water.
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_REFILL]);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_SOLENOID]);
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_CANISTER]);
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
// Ultrasonic (A02YYUW UART)
// ----------------------------------------------------------------------------

void test_ultrasonic_valid_reading() {
  SafetyWatchdog sw;
  sw.begin();
  mock_inject_a02_distance(15.0f); // 15cm

  float dist = sw.readUltrasonic();
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 15.0f, dist);
}

void test_ultrasonic_no_data_returns_last() {
  SafetyWatchdog sw;
  sw.begin();
  mock_inject_a02_distance(15.0f);
  sw.readUltrasonic(); // Consume the frame

  Serial2.mock_clear(); // No new data
  float dist = sw.readUltrasonic();

  // Should return last valid
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 15.0f, dist);
}

void test_ultrasonic_rejects_bad_checksum() {
  SafetyWatchdog sw;
  sw.begin();

  // Inject a frame with invalid checksum
  uint8_t badFrame[] = { 0xFF, 0x00, 0x64, 0x00 }; // checksum should be 0x63
  Serial2.mock_inject(badFrame, 4);

  float dist = sw.readUltrasonic();
  // Should remain at -1 (no valid reading)
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, dist);
}

// ----------------------------------------------------------------------------
// Median Filter
// ----------------------------------------------------------------------------

void test_median_filter_rejects_spike() {
  SafetyWatchdog sw;
  sw.begin();

  // Inject 5 readings: 4 normal (~15cm) + 1 spike (2cm)
  float readings[] = { 15.0f, 15.1f, 2.0f, 14.9f, 15.2f };
  for (int i = 0; i < 5; i++) {
    mock_inject_a02_distance(readings[i]);
    sw.readUltrasonic();
  }

  // Median of [2.0, 14.9, 15.0, 15.1, 15.2] sorted = 15.0
  float dist = sw.getLastDistance();
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 15.0f, dist);
}

void test_median_filter_with_partial_buffer() {
  SafetyWatchdog sw;
  sw.begin();

  // Only 2 readings
  mock_inject_a02_distance(10.0f);
  sw.readUltrasonic();
  mock_inject_a02_distance(12.0f);
  sw.readUltrasonic();

  // With 2 samples, median = sorted[1] = 12.0
  float dist = sw.getLastDistance();
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 12.0f, dist);
}

// ----------------------------------------------------------------------------
// Optical Overflow Flag
// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
  UNITY_BEGIN();

  // Initialization
  RUN_TEST(test_begin_sets_pin_modes);

  // Optical sensor

  // Float switch
  RUN_TEST(test_float_low_means_reservoir_full);
  RUN_TEST(test_float_high_means_reservoir_empty);

  // Emergency
  RUN_TEST(test_emergency_shutdown_all_pins_low);
  RUN_TEST(test_emergency_drain_opens_drain_only);

  // Maintenance mode
  RUN_TEST(test_maintenance_mode_toggles);
  RUN_TEST(test_maintenance_auto_expires);
  RUN_TEST(test_maintenance_persists_within_duration);

  // Ultrasonic (A02YYUW UART)
  RUN_TEST(test_ultrasonic_valid_reading);
  RUN_TEST(test_ultrasonic_no_data_returns_last);
  RUN_TEST(test_ultrasonic_rejects_bad_checksum);

  // Median filter
  RUN_TEST(test_median_filter_rejects_spike);
  RUN_TEST(test_median_filter_with_partial_buffer);

  // Overflow flags

  UNITY_END();
  return 0;
}
