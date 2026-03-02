// ============================================================================
// WaterManager Unit Tests
// Tests: state machine transitions, safety aborts, timeout handling
// ============================================================================

#include "Arduino.h"
#include "FertManager.h"
#include "SafetyWatchdog.h"
#include "WaterManager.h"
#include <unity.h>

static SafetyWatchdog safety;
static FertManager fert;

void setUp() {
  mock_reset_pins();
  mock_millis_value = 0;
  // Default: water too high for drain target — 10cm (pulseIn ~583us)
  mock_pulseIn_value = 583;
  mock_pin_read_value[PIN_OPTICAL] = HIGH; // Normal
  mock_pin_read_value[PIN_FLOAT] = HIGH;   // Reservoir empty
  Preferences::mock_clearAll();

  safety = SafetyWatchdog();
  safety.begin();
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

// Helper: advance to DRAINING (water stays high, won't finish draining)
void goToDraining(WaterManager &wm) {
  mock_pulseIn_value = 400; // ~6.9cm — water very high, far from 20cm target
  wm.startTPA();
  wm.update(); // CANISTER_OFF: first call sets _waitUntilMs = millis() + 3000
  mock_millis_value += 3001; // Advance past the 3s wait
  wm.update();               // CANISTER_OFF: wait elapsed → DRAINING
}

// Helper: advance to FILLING_RESERVOIR
void goToFilling(WaterManager &wm) {
  goToDraining(wm);
  // Drain runs, ultrasonic reads ~6.9cm (< 20), stays DRAINING
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::DRAINING, wm.getState());

  // Now ultrasonic shows level past target (>= 20cm → 1400us ≈ 24cm)
  mock_pulseIn_value = 1400;
  wm.update(); // Target reached → FILLING_RESERVOIR
  TEST_ASSERT_EQUAL(TPAState::FILLING_RESERVOIR, wm.getState());
}

// Helper: advance to DOSING_PRIME
void goToDosingPrime(WaterManager &wm) {
  goToFilling(wm);
  wm.update();                          // Opens solenoid
  mock_pin_read_value[PIN_FLOAT] = LOW; // Reservoir full
  wm.update();                          // Float triggers → DOSING_PRIME
  TEST_ASSERT_EQUAL(TPAState::DOSING_PRIME, wm.getState());
}

// Helper: advance to REFILLING
void goToRefilling(WaterManager &wm) {
  goToDosingPrime(wm);
  wm.update();               // Doses prime, sets _waitUntilMs = millis() + 2000
  mock_millis_value += 2001; // Advance past the 2s mixing wait
  wm.update();               // Wait elapsed → REFILLING
  TEST_ASSERT_EQUAL(TPAState::REFILLING, wm.getState());
}

// --- Initial State ---

void test_initial_state_is_idle() {
  WaterManager wm = makeWM();
  TEST_ASSERT_EQUAL(TPAState::IDLE, wm.getState());
  TEST_ASSERT_FALSE(wm.isRunning());
}

// --- Start TPA ---

void test_start_tpa_transitions_to_canister_off() {
  WaterManager wm = makeWM();
  wm.startTPA();
  TEST_ASSERT_EQUAL(TPAState::CANISTER_OFF, wm.getState());
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
  mock_pulseIn_value = 400;
  wm.startTPA();
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
  wm.update();
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_DRAIN]);
  TEST_ASSERT_EQUAL(TPAState::DRAINING, wm.getState());
}

void test_draining_stops_at_target() {
  WaterManager wm = makeWM();
  goToDraining(wm);
  wm.update(); // Pump on, reads ~6.9cm → stays DRAINING

  mock_pulseIn_value = 1400; // ~24cm → >= 20 target
  wm.update();

  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_DRAIN]);
  TEST_ASSERT_EQUAL(TPAState::FILLING_RESERVOIR, wm.getState());
}

void test_draining_timeout_causes_error() {
  WaterManager wm = makeWM();
  goToDraining(wm);
  mock_millis_value += TIMEOUT_DRAIN_MS + 1;
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
  mock_pin_read_value[PIN_FLOAT] = LOW;
  wm.update();
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_SOLENOID]);
  TEST_ASSERT_EQUAL(TPAState::DOSING_PRIME, wm.getState());
}

void test_fill_timeout_causes_error() {
  WaterManager wm = makeWM();
  goToFilling(wm);
  wm.update();
  unsigned long t = mock_millis_value;
  mock_millis_value = t + TIMEOUT_FILL_MS + 1;
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::ERROR, wm.getState());
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_SOLENOID]);
}

// --- Abort ---

void test_abort_stops_all_and_restores_canister() {
  WaterManager wm = makeWM();
  goToDraining(wm);
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
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::ERROR, wm.getState());
}

// --- Refilling ---

void test_refill_stops_on_optical_sensor() {
  WaterManager wm = makeWM();
  goToRefilling(wm);

  mock_pulseIn_value = 1400; // 24cm — far from 10cm target
  wm.update();               // Pump ON
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_REFILL]);

  mock_pin_read_value[PIN_OPTICAL] = LOW; // Max level!
  wm.update();
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_REFILL]);
  TEST_ASSERT_EQUAL(TPAState::CANISTER_ON, wm.getState());
}

void test_refill_stops_at_setpoint() {
  WaterManager wm = makeWM();
  goToRefilling(wm);

  mock_pulseIn_value = 1400; // 24cm
  wm.update();               // Pump ON

  mock_pulseIn_value = 500; // ~8.6cm → <= 10cm setpoint
  wm.update();
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_REFILL]);
  TEST_ASSERT_EQUAL(TPAState::CANISTER_ON, wm.getState());
}

// --- Complete Cycle ---

void test_complete_cycle_restores_canister() {
  WaterManager wm = makeWM();
  goToRefilling(wm);

  mock_pin_read_value[PIN_OPTICAL] = LOW;
  wm.update(); // REFILLING → CANISTER_ON
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

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_initial_state_is_idle);
  RUN_TEST(test_start_tpa_transitions_to_canister_off);
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
  RUN_TEST(test_refill_stops_on_optical_sensor);
  RUN_TEST(test_refill_stops_at_setpoint);
  RUN_TEST(test_complete_cycle_restores_canister);
  RUN_TEST(test_state_names);

  UNITY_END();
  return 0;
}
