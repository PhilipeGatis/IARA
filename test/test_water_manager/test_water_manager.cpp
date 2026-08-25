// ============================================================================
// WaterManager Unit Tests
// Tests: state machine transitions, safety aborts, timeout handling,
//        auto-calibration
// ============================================================================

#include "Arduino.h"
#include "FertManager.h"
#include "SafetyWatchdog.h"
#include "WaterManager.h"
#include <unity.h>

static SafetyWatchdog safety;
static FertManager fert;

void setUp() {
  Preferences::mock_clearAll();
  mock_reset_pins();
  mock_millis_value = 0;
  Serial2.mock_clear();
  mock_pin_read_value[PIN_FLOAT] = HIGH; // Reservoir empty (HIGH = not triggered)

  safety = SafetyWatchdog();
  safety.begin();
  
  mock_inject_a02_distance(10.0f); // Default: 10cm
  safety.update(); // Process the initial distance frame
  
  fert = FertManager();
  fert.begin();
}

void tearDown() {}

WaterManager makeWM() {
  WaterManager wm;
  wm.begin(&safety, &fert);
  wm.setDrainTargetCm(20.0f);
  wm.setRefillTargetCm(10.0f);
  return wm;
}

// Helper: force a distance value by filling the median buffer
void setDistance(float dist) {
  // Must overwrite the whole median window, otherwise older samples still
  // dominate the median and the level never actually becomes `dist`.
  for (int i = 0; i < 32; i++) {
    mock_inject_a02_distance(dist);
    safety.readUltrasonic();
  }
}

// Helper: simulate float sensor triggering (debounced — needs N consecutive reads)
void simulateFloatFull(WaterManager &wm) {
  mock_pin_read_value[PIN_FLOAT] = LOW; // LOW = triggered (active LOW with pullup)
  for (int i = 0; i < 5; i++) { wm.update(); }
}

// The cycle now starts at the reservoir, so everything that can fail there
// fails while the aquarium is still full and the canister still running.
// Order: FILLING_RESERVOIR -> DOSING_PRIME -> CANISTER_OFF -> DRAINING
//        -> REFILLING -> CANISTER_ON -> COMPLETE

// Helper: advance to FILLING_RESERVOIR (the first state of the cycle)
void goToFilling(WaterManager &wm) {
  mock_pin_read_value[PIN_FLOAT] = HIGH; // reservoir not full yet
  setDistance(7.0f);                     // water high, far from the 20cm target
  wm.startTPA();
  wm.update(); // opens the solenoid
  TEST_ASSERT_EQUAL(TPAState::FILLING_RESERVOIR, wm.getState());
}

// Helper: advance to DOSING_PRIME
void goToDosingPrime(WaterManager &wm) {
  goToFilling(wm);
  simulateFloatFull(wm); // debounced float → DOSING_PRIME
  TEST_ASSERT_EQUAL(TPAState::DOSING_PRIME, wm.getState());
}

// Helper: advance to CANISTER_OFF
void goToCanisterOff(WaterManager &wm) {
  goToDosingPrime(wm);
  wm.update();               // doses prime, sets the 2s mixing wait
  mock_millis_value += 2001;
  wm.update();               // wait elapsed → CANISTER_OFF
  TEST_ASSERT_EQUAL(TPAState::CANISTER_OFF, wm.getState());
}

// Helper: advance to DRAINING
void goToDraining(WaterManager &wm) {
  goToCanisterOff(wm);
  wm.update();               // canister off, sets the 3s settle wait
  mock_millis_value += 3001;
  wm.update();               // wait elapsed → DRAINING
  TEST_ASSERT_EQUAL(TPAState::DRAINING, wm.getState());
}

// Helper: advance to REFILLING
void goToRefilling(WaterManager &wm) {
  goToDraining(wm);
  setDistance(7.0f);
  wm.update();               // drain pump on, still above target
  setDistance(24.0f);        // past the 20cm drain target
  wm.update();               // → REFILLING
  TEST_ASSERT_EQUAL(TPAState::REFILLING, wm.getState());
}

// --- Initial State ---

void test_initial_state_is_idle() {
  WaterManager wm = makeWM();
  TEST_ASSERT_EQUAL(TPAState::IDLE, wm.getState());
  TEST_ASSERT_FALSE(wm.isRunning());
}

// --- Start TPA ---

void test_start_tpa_transitions_to_filling_reservoir() {
  WaterManager wm = makeWM();
  wm.startTPA();
  // The reservoir is filled first so a failure there leaves the aquarium full.
  TEST_ASSERT_EQUAL(TPAState::FILLING_RESERVOIR, wm.getState());
  TEST_ASSERT_TRUE(wm.isRunning());
}

void test_start_tpa_blocked_during_emergency() {
  WaterManager wm = makeWM();
  safety.emergencyShutdown();
  wm.startTPA();
  TEST_ASSERT_EQUAL(TPAState::IDLE, wm.getState());
}

void test_double_start_ignored() {
  WaterManager wm = makeWM();
  wm.startTPA();
  wm.update();
  TPAState s = wm.getState();
  wm.startTPA(); // Ignored
  TEST_ASSERT_EQUAL(s, wm.getState());
}

// --- Canister OFF ---

void test_canister_off_disables_relay() {
  WaterManager wm = makeWM();
  digitalWrite(PIN_CANISTER, LOW); // Start with canister ON (LOW = ON)
  goToCanisterOff(wm);
  wm.update(); // First call: sets canister HIGH (OFF) and starts 3s wait
  // SSR relay: HIGH = OFF (canister disabled)
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_CANISTER]);
  TEST_ASSERT_EQUAL(TPAState::CANISTER_OFF, wm.getState()); // Still waiting

  mock_millis_value += 3001;
  wm.update(); // Wait elapsed → DRAINING
  TEST_ASSERT_EQUAL(TPAState::DRAINING, wm.getState());
}

// --- Draining ---

void test_draining_activates_drain_pump() {
  WaterManager wm = makeWM();
  goToDraining(wm);
  setDistance(7.0f);
  wm.update();
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_DRAIN]);
  TEST_ASSERT_EQUAL(TPAState::DRAINING, wm.getState());
}

void test_draining_stops_at_target() {
  WaterManager wm = makeWM();
  goToDraining(wm);
  setDistance(7.0f);
  wm.update(); // Pump on, reads ~7cm → stays DRAINING

  setDistance(24.0f); // >= 20cm target
  wm.update();

  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_DRAIN]);
  TEST_ASSERT_EQUAL(TPAState::REFILLING, wm.getState());
}

void test_draining_timeout_causes_error() {
  WaterManager wm = makeWM();
  goToDraining(wm);
  mock_millis_value += 1800000 + 1;
  setDistance(7.0f); // Still not at target
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::ERROR, wm.getState());
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_DRAIN]);
  // SSR relay: LOW = ON (canister restored on error)
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_CANISTER]);
}

// --- Filling Reservoir ---

void test_fill_opens_solenoid() {
  WaterManager wm = makeWM();
  goToFilling(wm);
  wm.update();
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_SOLENOID]);
  TEST_ASSERT_EQUAL(TPAState::FILLING_RESERVOIR, wm.getState());
}

void test_fill_stops_on_float_switch() {
  WaterManager wm = makeWM();
  goToFilling(wm);
  wm.update(); // Opens solenoid
  simulateFloatFull(wm);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_SOLENOID]);
  TEST_ASSERT_EQUAL(TPAState::DOSING_PRIME, wm.getState());
}

void test_fill_timeout_causes_error() {
  WaterManager wm = makeWM();
  goToFilling(wm);
  wm.update();
  unsigned long t = mock_millis_value;
  mock_millis_value = t + (2UL * 60 * 60 * 1000) + 1; // 2 hours hard limit
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::ERROR, wm.getState());
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_SOLENOID]);
}

// --- Abort ---

void test_abort_stops_all_and_restores_canister() {
  WaterManager wm = makeWM();
  goToDraining(wm);
  setDistance(7.0f);
  wm.update();
  wm.abortTPA();
  TEST_ASSERT_EQUAL(TPAState::ERROR, wm.getState());
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_DRAIN]);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_REFILL]);
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_SOLENOID]);
  // SSR relay: LOW = ON (canister restored on abort)
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_CANISTER]);
}

// --- Emergency ---

void test_emergency_during_tpa_aborts() {
  WaterManager wm = makeWM();
  goToDraining(wm);
  safety.emergencyShutdown();
  setDistance(7.0f);
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::ERROR, wm.getState());
}

// --- Refilling ---

void test_refill_stops_at_setpoint() {
  WaterManager wm = makeWM();
  goToRefilling(wm);

  setDistance(24.0f);
  wm.update();               // Pump ON

  setDistance(8.6f); // <= 10cm setpoint
  wm.update();
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_REFILL]);
  TEST_ASSERT_EQUAL(TPAState::CANISTER_ON, wm.getState());
}

// --- Complete Cycle ---

void test_complete_cycle_restores_canister() {
  WaterManager wm = makeWM();
  goToRefilling(wm);

  setDistance(24.0f);
  wm.update(); // Refill pump ON
  setDistance(8.6f); // <= 10cm setpoint reached
  wm.update(); // → CANISTER_ON
  wm.update(); // CANISTER_ON → COMPLETE

  TEST_ASSERT_EQUAL(TPAState::COMPLETE, wm.getState());
  // SSR relay: LOW = ON (canister restored after complete cycle)
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_CANISTER]);
  TEST_ASSERT_FALSE(wm.isRunning());
}

// --- State Names ---

void test_state_names() {
  TEST_ASSERT_EQUAL_STRING("IDLE", tpaStateName(TPAState::IDLE));
  TEST_ASSERT_EQUAL_STRING("DRAINING", tpaStateName(TPAState::DRAINING));
  TEST_ASSERT_EQUAL_STRING("REFILLING", tpaStateName(TPAState::REFILLING));
  TEST_ASSERT_EQUAL_STRING("ERROR", tpaStateName(TPAState::ERROR));
  TEST_ASSERT_EQUAL_STRING("COMPLETE", tpaStateName(TPAState::COMPLETE));
}

// --- Calibration ---

void test_drain_calibration_during_tpa() {
  WaterManager wm = makeWM();
  wm.setLitersPerCm(3.2f);      // 80cm × 40cm / 1000 = 3.2 L/cm
  wm.setTimeoutDrainMs(300000); // 5 min (so we don't hit timeout)

  goToDraining(wm);
  setDistance(10.0f); // start level the first DRAINING tick will record

  // First DRAINING tick: pump turns on, records _calStartLevel at ~10cm
  setDistance(10.0f);
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::DRAINING, wm.getState());

  // Drain pump runs for 20 seconds
  mock_millis_value += 20000;

  // Water level dropped to 20+ cm (target reached)
  setDistance(20.4f);
  wm.update();               // DRAINING → FILLING (calibration captured)

  TEST_ASSERT_EQUAL(TPAState::REFILLING, wm.getState());
  // 10.0 -> 20.4 cm is far more than CALIBRATION_MIN_DELTA_PCT of the tank,
  // so the sample is accepted.
  TEST_ASSERT_TRUE(wm.getDrainFlowLPM() > 0);
}

void test_calibration_rejects_tiny_level_change() {
  WaterManager wm = makeWM();
  wm.setLitersPerCm(3.2f);
  wm.setAqEffectiveHeightCm(40.0f); // 5% of 40cm = 2cm minimum
  wm.setTimeoutDrainMs(300000);

  goToDraining(wm);
  setDistance(10.0f);
  wm.update(); // records the start level

  mock_millis_value += 20000;
  setDistance(10.5f); // only 0.5cm — below the 2cm floor, and pure noise
  wm.update();

  // A rate derived from that would be meaningless, so none is produced.
  TEST_ASSERT_EQUAL(0.0f, wm.getDrainFlowLPM());
}

void test_refill_calibration_during_tpa() {
  WaterManager wm = makeWM();
  wm.setLitersPerCm(3.2f);
  wm.setTimeoutDrainMs(300000);
  wm.setTimeoutRefillMs(300000);

  goToDraining(wm);
  setDistance(7.0f);

  // First DRAINING tick (start pump, record start)
  setDistance(7.0f);
  wm.update();

  // Water reaches drain target
  setDistance(20.4f);
  wm.update();               // → FILLING_RESERVOIR

  // Float switch triggered (reservoir full) - debounced
  simulateFloatFull(wm); // → DOSING_PRIME

  // First DOSING_PRIME tick: doses and sets wait
  wm.update();
  mock_millis_value += 2001;
  wm.update(); // wait elapsed → REFILLING
  TEST_ASSERT_EQUAL(TPAState::REFILLING, wm.getState());

  // First REFILLING tick: pump on, records _calStartLevel
  setDistance(20.4f);
  wm.update();

  // Refill pump runs for 30 seconds
  mock_millis_value += 30000;

  // Water level back to ~10cm
  setDistance(9.9f); // <= 10cm target
  wm.update();              // → CANISTER_ON

  TEST_ASSERT_EQUAL(TPAState::CANISTER_ON, wm.getState());
  TEST_ASSERT_TRUE(wm.getRefillFlowLPM() > 0);
}

void test_dynamic_timeout_drain() {
  WaterManager wm = makeWM();
  wm.setTimeoutDrainMs(5000); // 5s custom timeout

  goToDraining(wm);

  // Advance past custom timeout (5s)
  mock_millis_value += 5001;
  setDistance(7.0f);
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::ERROR, wm.getState());
}

void test_dynamic_timeout_refill() {
  WaterManager wm = makeWM();
  wm.setTimeoutRefillMs(8000); // 8s custom timeout

  goToRefilling(wm);
  TEST_ASSERT_EQUAL(TPAState::REFILLING, wm.getState());

  // Advance past custom timeout (8s)
  mock_millis_value += 8001;
  setDistance(24.0f);
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::ERROR, wm.getState());
}

void test_uncalibrated_defaults_are_short() {
  WaterManager wm = makeWM();
  // Default: not calibrated
  TEST_ASSERT_FALSE(wm.isCalibrated());

  // Start draining
  setDistance(7.0f);
  wm.startTPA();
  wm.update();
  mock_millis_value += 3001;
  wm.update(); // → DRAINING

  // After default timeout, should error
  mock_millis_value += 1200001;
  setDistance(7.0f);
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::ERROR, wm.getState());
}

void test_is_calibrated_getter() {
  WaterManager wm = makeWM();
  TEST_ASSERT_FALSE(wm.isCalibrated());

  wm.setDrainFlowLPM(2.5f);
  TEST_ASSERT_FALSE(wm.isCalibrated()); // still missing refill

  wm.setRefillFlowLPM(3.0f);
  TEST_ASSERT_TRUE(wm.isCalibrated());
}

// --- Sensor-based progress ---

void test_progress_uses_sensor_data() {
  WaterManager wm = makeWM();
  wm.setLitersPerCm(3.2f);
  wm.setTimeoutDrainMs(300000);

  goToDraining(wm);
  setDistance(10.0f);

  // First tick records start level
  setDistance(10.0f);
  wm.update();

  // Water drops 5cm → 15cm distance
  setDistance(15.0f);
  float progress = wm.getPumpProgressLiters();
  // 5cm × 3.2 L/cm = 16 liters
  TEST_ASSERT_FLOAT_WITHIN(2.0f, 16.0f, progress);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_initial_state_is_idle);
  RUN_TEST(test_start_tpa_transitions_to_filling_reservoir);
  RUN_TEST(test_start_tpa_blocked_during_emergency);
  RUN_TEST(test_double_start_ignored);
  RUN_TEST(test_canister_off_disables_relay);
  RUN_TEST(test_draining_activates_drain_pump);
  RUN_TEST(test_draining_stops_at_target);
  RUN_TEST(test_draining_timeout_causes_error);
  RUN_TEST(test_fill_opens_solenoid);
  RUN_TEST(test_fill_stops_on_float_switch);
  RUN_TEST(test_fill_timeout_causes_error);
  RUN_TEST(test_abort_stops_all_and_restores_canister);
  RUN_TEST(test_emergency_during_tpa_aborts);
  RUN_TEST(test_refill_stops_at_setpoint);
  RUN_TEST(test_complete_cycle_restores_canister);
  RUN_TEST(test_state_names);

  // Calibration & Dynamic Timeouts
  RUN_TEST(test_drain_calibration_during_tpa);
  RUN_TEST(test_calibration_rejects_tiny_level_change);
  RUN_TEST(test_refill_calibration_during_tpa);
  RUN_TEST(test_dynamic_timeout_drain);
  RUN_TEST(test_dynamic_timeout_refill);
  RUN_TEST(test_uncalibrated_defaults_are_short);
  RUN_TEST(test_is_calibrated_getter);

  // Sensor-based progress
  RUN_TEST(test_progress_uses_sensor_data);

  UNITY_END();
  return 0;
}
