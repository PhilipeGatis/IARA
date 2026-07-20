// ============================================================================
// Drain Pump Safety Tests
// Verifies PIN_DRAIN (bomba de expurgo) NEVER activates without explicit reason.
// Also covers previously untested WaterManager code paths.
// ============================================================================

#include "Arduino.h"
#include "FertManager.h"
#include "PumpLog.h"
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
  mock_pin_read_value[PIN_OPTICAL] = HIGH; // Normal (not triggered)
  mock_pin_read_value[PIN_FLOAT] = HIGH; // Reservoir empty (HIGH = not triggered)
  Preferences::mock_clearAll();

  safety = SafetyWatchdog();
  safety.begin();
  
  mock_inject_a02_distance(10.0f); // ~10cm default
  safety.update(); // Process initial distance frame
  
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
  for (int i = 0; i < 5; i++) {
    mock_inject_a02_distance(dist);
    safety.readUltrasonic();
  }
}

// Helper: assert PIN_DRAIN is LOW (OFF)
void assertDrainOff(const char *ctx) {
  char msg[80];
  snprintf(msg, sizeof(msg), "DRAIN should be OFF during %s", ctx);
  TEST_ASSERT_EQUAL_MESSAGE(LOW, mock_pin_state[PIN_DRAIN], msg);
}

// Helper: advance to DRAINING (water stays high)
void goToDraining(WaterManager &wm) {
  setDistance(7.0f); // ~7cm — far from 20cm target
  wm.startTPA();
  wm.update(); // CANISTER_OFF: sets _waitUntilMs
  mock_millis_value += 3001;
  wm.update(); // CANISTER_OFF → DRAINING
}

// Helper: advance to FILLING_RESERVOIR
void goToFilling(WaterManager &wm) {
  goToDraining(wm);
  setDistance(7.0f);
  wm.update(); // Drain pump on
  setDistance(24.0f); // ~24cm → past 20cm target
  wm.update(); // → FILLING_RESERVOIR
}

// Helper: simulate float sensor triggering (debounced — needs N consecutive reads)
void simulateFloatFull(WaterManager &wm) {
  mock_pin_read_value[PIN_FLOAT] = LOW; // LOW = triggered (active LOW with pullup)
  for (int i = 0; i < 5; i++) { wm.update(); }
}

// Helper: advance to DOSING_PRIME
void goToDosingPrime(WaterManager &wm) {
  goToFilling(wm);
  wm.update(); // Opens solenoid
  simulateFloatFull(wm); // Debounced → DOSING_PRIME
}

// Helper: advance to REFILLING
void goToRefilling(WaterManager &wm) {
  goToDosingPrime(wm);
  wm.update();               // Doses prime
  mock_millis_value += 2001;
  wm.update();               // → REFILLING
}

// ============================================================================
// SECTION 1: Drain pump must be OFF in every non-drain state
// ============================================================================

void test_drain_off_during_idle() {
  WaterManager wm = makeWM();
  for (int i = 0; i < 10; i++) {
    mock_millis_value += 1000;
    wm.update();
  }
  assertDrainOff("IDLE (10 update cycles)");
  TEST_ASSERT_EQUAL(TPAState::IDLE, wm.getState());
}

void test_drain_off_during_canister_off() {
  WaterManager wm = makeWM();
  setDistance(7.0f);
  wm.startTPA();
  wm.update(); // CANISTER_OFF: first tick
  assertDrainOff("CANISTER_OFF state");
  TEST_ASSERT_EQUAL(TPAState::CANISTER_OFF, wm.getState());
}

void test_drain_off_during_filling_reservoir() {
  WaterManager wm = makeWM();
  goToFilling(wm);
  assertDrainOff("FILLING_RESERVOIR entry");
  wm.update(); // Opens solenoid
  assertDrainOff("FILLING_RESERVOIR solenoid open");
  TEST_ASSERT_EQUAL(TPAState::FILLING_RESERVOIR, wm.getState());
}

void test_drain_off_during_dosing_prime() {
  WaterManager wm = makeWM();
  goToDosingPrime(wm);
  assertDrainOff("DOSING_PRIME entry");
  wm.update(); // Prime dosed
  assertDrainOff("DOSING_PRIME after dose");
  TEST_ASSERT_EQUAL(TPAState::DOSING_PRIME, wm.getState());
}

void test_drain_off_during_refilling() {
  WaterManager wm = makeWM();
  goToRefilling(wm);
  setDistance(24.0f); // 24cm — refill still needed
  wm.update(); // Refill pump on
  assertDrainOff("REFILLING (refill pump should be on, NOT drain)");
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_REFILL]); // Refill ON, not drain
}

void test_drain_off_during_canister_on() {
  WaterManager wm = makeWM();
  goToRefilling(wm);
  mock_pin_read_value[PIN_OPTICAL] = LOW; // Max level → stop refill
  setDistance(24.0f);
  wm.update(); // → CANISTER_ON
  TEST_ASSERT_EQUAL(TPAState::CANISTER_ON, wm.getState());
  assertDrainOff("CANISTER_ON state");
}

void test_drain_off_during_complete() {
  WaterManager wm = makeWM();
  goToRefilling(wm);
  mock_pin_read_value[PIN_OPTICAL] = LOW;
  setDistance(24.0f);
  wm.update(); // → CANISTER_ON
  wm.update(); // → COMPLETE
  TEST_ASSERT_EQUAL(TPAState::COMPLETE, wm.getState());
  assertDrainOff("COMPLETE state");
  // Additional: many update cycles in COMPLETE should not touch drain
  for (int i = 0; i < 5; i++) {
    mock_millis_value += 1000;
    wm.update();
    assertDrainOff("COMPLETE repeated updates");
  }
}

void test_drain_off_during_error() {
  WaterManager wm = makeWM();
  goToDraining(wm);
  setDistance(7.0f);
  wm.update(); // Drain ON
  // Simulate timeout
  mock_millis_value += 1800000 + 1;
  setDistance(7.0f);
  wm.update(); // → ERROR (all actuators off)
  TEST_ASSERT_EQUAL(TPAState::ERROR, wm.getState());
  assertDrainOff("ERROR state");
  // Repeated updates in ERROR should not re-activate
  for (int i = 0; i < 5; i++) {
    mock_millis_value += 1000;
    wm.update();
    assertDrainOff("ERROR repeated updates");
  }
}

void test_drain_off_during_manual_reservoir_fill() {
  WaterManager wm = makeWM();
  wm.startManualReservoirFill();
  TEST_ASSERT_EQUAL(TPAState::MANUAL_RESERVOIR_FILL, wm.getState());
  wm.update(); // Opens solenoid
  assertDrainOff("MANUAL_RESERVOIR_FILL");
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_SOLENOID]);
}

void test_drain_off_during_manual_pump_refill() {
  WaterManager wm = makeWM();
  wm.setRefillFlowLPM(2.0f);
  wm.startManualPump("refill", 5.0f);
  TEST_ASSERT_EQUAL(TPAState::MANUAL_PUMP_REFILL, wm.getState());
  wm.update();
  assertDrainOff("MANUAL_PUMP_REFILL");
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_REFILL]); // Refill ON, not drain
}

// ============================================================================
// SECTION 2: Drain pump activates ONLY when explicitly commanded
// ============================================================================

void test_drain_on_only_during_draining() {
  WaterManager wm = makeWM();
  goToDraining(wm);
  // First DRAINING tick: pump should turn ON
  setDistance(7.0f);
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::DRAINING, wm.getState());
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_DRAIN]);
}

void test_drain_on_during_manual_pump_drain() {
  WaterManager wm = makeWM();
  wm.setDrainFlowLPM(2.0f);
  wm.startManualPump("drain", 5.0f);
  TEST_ASSERT_EQUAL(TPAState::MANUAL_PUMP_DRAIN, wm.getState());
  wm.update(); // Drain pump ON
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_DRAIN]);
}

void test_drain_stops_after_manual_stop() {
  WaterManager wm = makeWM();
  wm.setDrainFlowLPM(2.0f);
  wm.startManualPump("drain", 5.0f);
  wm.update(); // Drain ON
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_DRAIN]);

  wm.stopManual();
  TEST_ASSERT_EQUAL(TPAState::IDLE, wm.getState());
  assertDrainOff("after stopManual");
}

// ============================================================================
// SECTION 3: Manual pump coverage (previously untested)
// ============================================================================

void test_manual_drain_stops_on_goal_reached() {
  WaterManager wm = makeWM();
  wm.setDrainFlowLPM(6.0f); // 6 L/min = 0.1 L/s
  wm.startManualPump("drain", 1.0f); // Goal: 1 liter
  wm.update(); // Drain ON
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_DRAIN]);

  // At 6 L/min, 1L takes 10 seconds (10000ms)
  mock_millis_value += 10001;
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::COMPLETE, wm.getState());
  assertDrainOff("manual drain goal reached");
}

void test_manual_drain_timeout_30min() {
  WaterManager wm = makeWM();
  wm.setDrainFlowLPM(0.001f); // Almost zero flow — will timeout
  wm.startManualPump("drain", 999.0f); // Unreachable goal
  wm.update(); // Drain ON

  // 30 minutes timeout
  mock_millis_value += 30UL * 60 * 1000 + 1;
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::ERROR, wm.getState());
  assertDrainOff("manual drain timeout");
}

void test_manual_refill_stops_on_optical() {
  WaterManager wm = makeWM();
  wm.setRefillFlowLPM(2.0f);
  wm.startManualPump("refill", 10.0f);
  wm.update(); // Refill ON
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_REFILL]);

  // Optical sensor triggers
  mock_pin_read_value[PIN_OPTICAL] = LOW;
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::COMPLETE, wm.getState());
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_REFILL]);
}

void test_manual_refill_stops_on_goal() {
  WaterManager wm = makeWM();
  wm.setRefillFlowLPM(6.0f); // 6 L/min
  wm.startManualPump("refill", 1.0f); // Goal: 1L
  wm.update(); // Refill ON

  mock_millis_value += 10001; // 10s at 6 L/min = 1L
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::COMPLETE, wm.getState());
}

void test_manual_reservoir_fill_completes_on_float() {
  WaterManager wm = makeWM();
  wm.startManualReservoirFill();
  wm.update(); // Opens solenoid
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_SOLENOID]);

  mock_pin_read_value[PIN_FLOAT] = LOW; // Reservoir full (LOW = triggered)
  simulateFloatFull(wm);
  TEST_ASSERT_EQUAL(TPAState::COMPLETE, wm.getState());
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_SOLENOID]);
}

void test_manual_operations_blocked_while_running() {
  WaterManager wm = makeWM();
  wm.startTPA();
  TEST_ASSERT_TRUE(wm.isRunning());

  // All manual operations should be ignored
  wm.startManualReservoirFill();
  TEST_ASSERT_EQUAL(TPAState::CANISTER_OFF, wm.getState()); // Unchanged

  wm.startManualPump("drain", 5.0f);
  TEST_ASSERT_EQUAL(TPAState::CANISTER_OFF, wm.getState()); // Unchanged
}

// ============================================================================
// SECTION 4: Pump progress/goal getters (previously untested)
// ============================================================================

void test_pump_elapsed_ms_zero_when_idle() {
  WaterManager wm = makeWM();
  TEST_ASSERT_EQUAL(0UL, wm.getPumpElapsedMs());
}

void test_pump_elapsed_ms_during_draining() {
  WaterManager wm = makeWM();
  goToDraining(wm);
  setDistance(7.0f);
  wm.update(); // DRAINING active
  mock_millis_value += 5000;
  TEST_ASSERT_TRUE(wm.getPumpElapsedMs() > 0);
}

void test_pump_goal_liters_manual_drain() {
  WaterManager wm = makeWM();
  wm.setDrainFlowLPM(2.0f);
  wm.startManualPump("drain", 7.5f);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 7.5f, wm.getPumpGoalLiters());
}

void test_pump_goal_liters_manual_refill() {
  WaterManager wm = makeWM();
  wm.setRefillFlowLPM(2.0f);
  wm.startManualPump("refill", 3.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 3.0f, wm.getPumpGoalLiters());
}

void test_pump_progress_liters_manual_drain() {
  WaterManager wm = makeWM();
  wm.setDrainFlowLPM(6.0f); // 6 L/min
  wm.startManualPump("drain", 10.0f);
  wm.update(); // Activate
  mock_millis_value += 60000; // 1 min at 6 L/min = 6L
  float progress = wm.getPumpProgressLiters();
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 6.0f, progress);
}

// ============================================================================
// SECTION 5: Emergency drain scenarios (SafetyWatchdog)
// ============================================================================

void test_emergency_drain_activates_drain_pump() {
  SafetyWatchdog sw;
  sw.begin();
  sw.emergencyDrain();
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_DRAIN]);
  TEST_ASSERT_TRUE(sw.isEmergency());
}

void test_emergency_drain_stops_when_water_safe() {
  setDistance(15.0f); // ~15cm (valid sensor)

  // Start emergency drain
  safety.emergencyDrain();
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_DRAIN]);

  // Water level drops to safe (distance > LEVEL_SAFETY_MIN_CM + 5)
  setDistance(34.0f); // ~34cm — very safe
  mock_millis_value += SAFETY_CHECK_INTERVAL_MS + 1;
  safety.update(); // Updates _lastDistance and checks emergency drain

  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_DRAIN]);
  TEST_ASSERT_FALSE(safety.isEmergency());
}

void test_emergency_drain_timeout_causes_full_shutdown() {
  setDistance(3.0f); // ~3cm — water still dangerously high

  safety.emergencyDrain();
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_DRAIN]);

  // Advance past emergency timeout (3 min)
  mock_millis_value += TIMEOUT_EMERGENCY_MS + 1;
  mock_millis_value += SAFETY_CHECK_INTERVAL_MS; // Ensure update runs
  setDistance(3.0f);
  safety.update();

  // Should have triggered full shutdown
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_DRAIN]);
  TEST_ASSERT_TRUE(safety.isEmergency());
}

void test_emergency_shutdown_clears_drain() {
  SafetyWatchdog sw;
  sw.begin();
  // Manually set drain HIGH to simulate it being on
  digitalWrite(PIN_DRAIN, HIGH);
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_DRAIN]);

  sw.emergencyShutdown();
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_DRAIN]);
}

// ============================================================================
// SECTION 6: Edge cases — drain pump should NEVER spontaneously activate
// ============================================================================

void test_drain_stays_off_through_full_idle_cycle() {
  // Simulate 60 seconds of IDLE — drain should never go HIGH
  WaterManager wm = makeWM();
  for (int i = 0; i < 60; i++) {
    mock_millis_value += 1000;
    wm.update();
    assertDrainOff("IDLE long cycle");
  }
}

void test_drain_stays_off_after_complete_with_many_updates() {
  // Full TPA cycle → COMPLETE → many updates — drain should stay LOW
  WaterManager wm = makeWM();
  goToRefilling(wm);
  mock_pin_read_value[PIN_OPTICAL] = LOW;
  setDistance(24.0f);
  wm.update(); // → CANISTER_ON
  wm.update(); // → COMPLETE

  for (int i = 0; i < 100; i++) {
    mock_millis_value += 500;
    wm.update();
    assertDrainOff("COMPLETE long soak");
  }
  TEST_ASSERT_EQUAL(TPAState::COMPLETE, wm.getState());
}

void test_drain_off_after_abort() {
  WaterManager wm = makeWM();
  goToDraining(wm);
  setDistance(7.0f);
  wm.update(); // Drain ON
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_DRAIN]);

  wm.abortTPA();
  assertDrainOff("immediately after abort");

  // Many updates after abort — drain stays off
  for (int i = 0; i < 20; i++) {
    mock_millis_value += 1000;
    wm.update();
    assertDrainOff("ERROR after abort repeated updates");
  }
}

void test_drain_off_safety_update_normal_conditions() {
  // SafetyWatchdog update cycle with normal conditions should never touch drain
  setDistance(30.0f); // ~30cm — sensor connected and safe

  mock_pin_read_value[PIN_OPTICAL] = HIGH; // Normal

  for (int i = 0; i < 30; i++) {
    mock_millis_value += SAFETY_CHECK_INTERVAL_MS + 1;
    setDistance(30.0f);
    safety.update();
    assertDrainOff("SafetyWatchdog normal update");
  }
  TEST_ASSERT_FALSE(safety.isEmergency());
}

void test_drain_off_overflow_detection_disabled() {
  // Overflow detection is currently commented out (line 204 of SafetyWatchdog.cpp)
  // Even with low ultrasonic readings, drain should NOT activate
  setDistance(3.0f); // ~3cm — below LEVEL_SAFETY_MIN_CM

  mock_pin_read_value[PIN_OPTICAL] = HIGH; // Normal

  // Run 15 update cycles to trigger overflow counter (needs 10 consecutive)
  for (int i = 0; i < 15; i++) {
    mock_millis_value += SAFETY_CHECK_INTERVAL_MS + 1;
    setDistance(3.0f);
    safety.update();
    assertDrainOff("overflow detection should be disabled");
  }
}

// ============================================================================
// SECTION 7: Prime skip coverage
// ============================================================================

void test_prime_disabled_skips_dosing() {
  WaterManager wm = makeWM();
  wm.setPrimeEnabled(false);
  goToFilling(wm);
  wm.update(); // Opens solenoid
  mock_pin_read_value[PIN_FLOAT] = LOW; // Reservoir full (LOW = triggered)
  simulateFloatFull(wm);
  TEST_ASSERT_EQUAL(TPAState::DOSING_PRIME, wm.getState());
  wm.update(); // DOSING_PRIME handler: prime disabled → skip to REFILLING
  TEST_ASSERT_EQUAL(TPAState::REFILLING, wm.getState());
  assertDrainOff("REFILLING after prime skip");
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
  UNITY_BEGIN();

  // Section 1: Drain OFF in non-drain states
  RUN_TEST(test_drain_off_during_idle);
  RUN_TEST(test_drain_off_during_canister_off);
  RUN_TEST(test_drain_off_during_filling_reservoir);
  RUN_TEST(test_drain_off_during_dosing_prime);
  RUN_TEST(test_drain_off_during_refilling);
  RUN_TEST(test_drain_off_during_canister_on);
  RUN_TEST(test_drain_off_during_complete);
  RUN_TEST(test_drain_off_during_error);
  RUN_TEST(test_drain_off_during_manual_reservoir_fill);
  RUN_TEST(test_drain_off_during_manual_pump_refill);

  // Section 2: Drain ON only when commanded
  RUN_TEST(test_drain_on_only_during_draining);
  RUN_TEST(test_drain_on_during_manual_pump_drain);
  RUN_TEST(test_drain_stops_after_manual_stop);

  // Section 3: Manual pump operations (new coverage)
  RUN_TEST(test_manual_drain_stops_on_goal_reached);
  RUN_TEST(test_manual_drain_timeout_30min);
  RUN_TEST(test_manual_refill_stops_on_optical);
  RUN_TEST(test_manual_refill_stops_on_goal);
  RUN_TEST(test_manual_reservoir_fill_completes_on_float);
  RUN_TEST(test_manual_operations_blocked_while_running);

  // Section 4: Pump progress/goal getters (new coverage)
  RUN_TEST(test_pump_elapsed_ms_zero_when_idle);
  RUN_TEST(test_pump_elapsed_ms_during_draining);
  RUN_TEST(test_pump_goal_liters_manual_drain);
  RUN_TEST(test_pump_goal_liters_manual_refill);
  RUN_TEST(test_pump_progress_liters_manual_drain);

  // Section 5: Emergency drain (SafetyWatchdog)
  RUN_TEST(test_emergency_drain_activates_drain_pump);
  RUN_TEST(test_emergency_drain_stops_when_water_safe);
  RUN_TEST(test_emergency_drain_timeout_causes_full_shutdown);
  RUN_TEST(test_emergency_shutdown_clears_drain);

  // Section 6: Edge cases — never spontaneous
  RUN_TEST(test_drain_stays_off_through_full_idle_cycle);
  RUN_TEST(test_drain_stays_off_after_complete_with_many_updates);
  RUN_TEST(test_drain_off_after_abort);
  RUN_TEST(test_drain_off_safety_update_normal_conditions);
  RUN_TEST(test_drain_off_overflow_detection_disabled);

  // Section 7: Prime skip
  RUN_TEST(test_prime_disabled_skips_dosing);

  UNITY_END();
  return 0;
}
