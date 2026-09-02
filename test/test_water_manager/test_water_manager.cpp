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
  wm.update(); // setpoint seen → pump off, settling
  mock_millis_value += REFILL_SETTLE_MS + 1;
  setDistance(8.6f); // still there once the surface is calm
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
  wm.update(); // setpoint seen → pump off, settling
  mock_millis_value += REFILL_SETTLE_MS + 1;
  setDistance(8.6f); // still there once the surface is calm
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
  wm.setAqEffectiveHeightCm(40.0f); // 5% floor = 2cm of level change
  wm.setTimeoutRefillMs(300000);

  goToRefilling(wm);

  // First REFILLING tick: pump on, records the start level.
  setDistance(20.4f);
  wm.update();

  mock_millis_value += 30000;

  // Level back at the 10cm target — but the reading is taken with water still
  // pouring in, so it only pauses the pump.
  setDistance(9.9f);
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::REFILLING, wm.getState());
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_REFILL]); // paused, not finished

  // Once the surface settles and still reads at target, it is believed.
  mock_millis_value += REFILL_SETTLE_MS + 1;
  setDistance(9.9f);
  wm.update();

  TEST_ASSERT_EQUAL(TPAState::CANISTER_ON, wm.getState());
  TEST_ASSERT_TRUE(wm.getRefillFlowLPM() > 0);
}

void test_refill_resumes_when_settled_reading_is_short() {
  WaterManager wm = makeWM();
  wm.setLitersPerCm(3.2f);
  wm.setTimeoutRefillMs(300000);

  goToRefilling(wm);
  setDistance(20.4f);
  wm.update(); // pump on

  // Inflow beside the sensor makes it look like the target was reached.
  setDistance(9.9f);
  wm.update();
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_REFILL]); // paused

  // With the surface calm the tank is really still short, so it carries on
  // instead of ending the refill early — the failure seen on the first real
  // water change, where the cycle reported success with the level low.
  mock_millis_value += REFILL_SETTLE_MS + 1;
  setDistance(14.0f);
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::REFILLING, wm.getState());
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_REFILL]); // pumping again
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

// A refill that is commanded but not filling has to be caught long before the
// timeout, and the reason is the case areSensorsConnected() cannot see: a
// sensor that keeps answering with a frozen number. From the firmware's side
// that is indistinguishable from the reed having cut the pump, a dead pump or
// a kinked hose -- all of them stop the level from moving.
void test_refill_errors_when_level_stops_moving() {
  WaterManager wm = makeWM();
  wm.setLitersPerCm(2.0f);
  wm.setRefillFlowLPM(5.0f); // 2.5 cm/min expected
  wm.setTimeoutRefillMs(600000);

  goToRefilling(wm);
  setDistance(24.0f);
  wm.update(); // pump on, expected rate snapshotted

  mock_millis_value += 21000; // past the grace period
  wm.update();                // opens the progress window

  mock_millis_value += 31000; // window elapses with the level unchanged
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::ERROR, wm.getState());
}

// The mirror of the test above: a refill that is actually filling must survive
// the same window. Guards against the check aborting legitimate water changes.
void test_refill_survives_progress_check_while_filling() {
  WaterManager wm = makeWM();
  wm.setLitersPerCm(2.0f);
  wm.setRefillFlowLPM(5.0f); // needs 1.25 cm in 30 s, 0.44 cm to pass
  wm.setTimeoutRefillMs(600000);

  goToRefilling(wm);
  setDistance(24.0f);
  wm.update();

  mock_millis_value += 21000;
  wm.update();

  mock_millis_value += 31000;
  setDistance(21.0f); // 3 cm of real progress, well past the threshold
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::REFILLING, wm.getState());
}

// The live recalibration rewrites _refillFlowLPM as the run goes. If the check
// read that value instead of the snapshot, a stalling refill would drag its own
// expectation down to zero and always agree that nothing is wrong.
void test_refill_stall_check_uses_snapshot_not_live_rate() {
  WaterManager wm = makeWM();
  wm.setLitersPerCm(2.0f);
  wm.setRefillFlowLPM(5.0f);
  wm.setTimeoutRefillMs(600000);

  goToRefilling(wm);
  setDistance(24.0f);
  wm.update();

  // Creep: enough movement to keep the live rate alive, far short of expected.
  mock_millis_value += 21000;
  setDistance(23.9f);
  wm.update();

  mock_millis_value += 31000;
  setDistance(23.8f); // 0.1 cm in 31 s against 1.29 cm expected
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::ERROR, wm.getState());
}

// The expected rate is only as trustworthy as the calibration behind it, and a
// calibration taken against a level sensor that was not tracking comes out far
// too high. One real device reported 19 L/min from a pump capable of 5. The
// absolute floor is what stops that number from failing an honest refill.
void test_refill_survives_a_wildly_overstated_calibration() {
  WaterManager wm = makeWM();
  wm.setLitersPerCm(1.8f);
  wm.setRefillFlowLPM(19.19f); // ~10.7 cm/min claimed, 4x the real pump
  wm.setTimeoutRefillMs(600000);

  goToRefilling(wm);
  setDistance(24.0f);
  wm.update();

  mock_millis_value += 21000;
  wm.update();

  // What the pump can really do: ~2.8 cm/min, so ~1.4 cm in the window. The
  // fraction of the claimed rate would demand 1.87 cm and abort a good run.
  mock_millis_value += 31000;
  setDistance(22.6f);
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::REFILLING, wm.getState());
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

  // Actually get to DRAINING. This used to jump straight from startTPA() to a
  // long wait and assert ERROR, which passed only because it was tripping the
  // *reservoir fill* timeout on the way — it never exercised the drain timeout
  // it is named for, and it broke the moment that unrelated constant changed.
  setDistance(7.0f);
  goToDraining(wm);

  // With no calibration the drain falls back to _timeoutDrainMs's constructor
  // default of 20 minutes. Note that is not TIMEOUT_DRAIN_MS, which says five —
  // the named constant is not what initialises the member. Left alone here, but
  // the two disagreeing is a trap for the next reader.
  mock_millis_value += 1200001;
  setDistance(7.0f);
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::ERROR, wm.getState());
}

// --- Nominal (datasheet) pump ratings ---

// The rating is measured at zero head, so the real pump can only ever be
// slower. A measurement above it is a broken measurement, and storing it is
// what sized a refill timeout at three minutes for a four-minute job.
void test_measurement_above_the_rating_is_refused() {
  WaterManager wm = makeWM();
  wm.setRefillNominalLPM(5.0f); // 300 L/h pump

  wm.setRefillFlowLPM(19.19f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, wm.getRefillFlowLPM());

  // Just inside the noise tolerance is still accepted.
  wm.setRefillFlowLPM(5.5f);
  TEST_ASSERT_EQUAL_FLOAT(5.5f, wm.getRefillFlowLPM());
}

// Order at boot is loadCalibration() first, ratings second, so declaring one
// has to reach backwards and re-test what is already stored.
void test_declaring_the_rating_discards_a_stored_impossible_rate() {
  WaterManager wm = makeWM();
  wm.setRefillFlowLPM(19.19f); // no rating yet, so it is accepted
  TEST_ASSERT_EQUAL_FLOAT(19.19f, wm.getRefillFlowLPM());

  wm.setRefillNominalLPM(5.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, wm.getRefillFlowLPM());
}

// Zero means "not declared" and must not start refusing every measurement.
void test_no_rating_accepts_any_measurement() {
  WaterManager wm = makeWM();
  wm.setRefillFlowLPM(19.19f);
  TEST_ASSERT_EQUAL_FLOAT(19.19f, wm.getRefillFlowLPM());
  TEST_ASSERT_EQUAL_FLOAT(19.19f, wm.planningRefillLPM());
}

// Planning takes the slower of the two: being wrong in the fast direction is
// what aborts an honest water change half-finished.
void test_planning_rate_takes_the_slower_of_the_two() {
  WaterManager wm = makeWM();

  wm.setRefillNominalLPM(5.0f);
  wm.setRefillFlowLPM(3.0f); // head loss: below the rating, so believed
  TEST_ASSERT_EQUAL_FLOAT(3.0f, wm.planningRefillLPM());

  // With nothing measured, the rating alone still sizes a timeout.
  WaterManager wm2 = makeWM();
  wm2.setDrainNominalLPM(4.0f);
  TEST_ASSERT_EQUAL_FLOAT(4.0f, wm2.planningDrainLPM());

  // And with neither, callers get zero and fall back to their own defaults.
  WaterManager wm3 = makeWM();
  TEST_ASSERT_EQUAL_FLOAT(0.0f, wm3.planningRefillLPM());
}

// The stall check expects the level to move at the planning rate. Sized from
// the overstated measurement it demanded more than the pump can deliver; the
// rating brings the expectation back to something physical.
void test_stall_check_expects_no_more_than_the_rating() {
  WaterManager wm = makeWM();
  wm.setLitersPerCm(1.8f);
  wm.setRefillNominalLPM(5.0f);
  wm.setRefillFlowLPM(19.19f); // refused, so planning falls back to 5.0
  TEST_ASSERT_EQUAL_FLOAT(5.0f, wm.planningRefillLPM());
  wm.setTimeoutRefillMs(600000);

  goToRefilling(wm);
  setDistance(24.0f);
  wm.update();

  mock_millis_value += 21000;
  wm.update();

  // 2.8 cm/min of real movement against an expectation built on 5 L/min.
  mock_millis_value += 31000;
  setDistance(22.6f);
  wm.update();
  TEST_ASSERT_EQUAL(TPAState::REFILLING, wm.getState());
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

// ----------------------------------------------------------------------------
// Feeding pause
// ----------------------------------------------------------------------------

// The pause only ever restores the filter through restoreCanisterIfSafe(), and
// that refuses on a level below the safe mark. A generous mark plus a shallow
// reading is what "the water is fine" looks like to these tests.
static void allowCanisterRestore(WaterManager &wm) {
  wm.setCanisterSafeLevelCm(20.0f);
  setDistance(10.0f);
}

void test_feeding_pause_switches_the_canister_off() {
  WaterManager wm = makeWM();
  digitalWrite(PIN_CANISTER, LOW); // ON

  wm.startFeedingPause(10);

  TEST_ASSERT_TRUE(wm.isFeedingPause());
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_CANISTER]); // SSR: HIGH = OFF
  TEST_ASSERT_EQUAL_UINT32(600, wm.feedingSecondsLeft());
}

void test_feeding_pause_restores_the_canister_when_time_is_up() {
  WaterManager wm = makeWM();
  allowCanisterRestore(wm);
  digitalWrite(PIN_CANISTER, LOW);

  wm.startFeedingPause(10);
  mock_millis_value += 9UL * 60UL * 1000UL;
  wm.update();
  TEST_ASSERT_TRUE(wm.isFeedingPause());
  TEST_ASSERT_EQUAL(HIGH, mock_pin_state[PIN_CANISTER]); // still off

  mock_millis_value += 2UL * 60UL * 1000UL;
  wm.update();

  TEST_ASSERT_FALSE(wm.isFeedingPause());
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_CANISTER]); // back ON
  TEST_ASSERT_EQUAL_UINT32(0, wm.feedingSecondsLeft());
}

void test_feeding_pause_ends_early() {
  WaterManager wm = makeWM();
  allowCanisterRestore(wm);
  digitalWrite(PIN_CANISTER, LOW);

  wm.startFeedingPause(10);
  wm.endFeedingPause();

  TEST_ASSERT_FALSE(wm.isFeedingPause());
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_CANISTER]);
}

void test_feeding_pause_refused_during_a_cycle() {
  WaterManager wm = makeWM();
  goToCanisterOff(wm);
  wm.update(); // the cycle owns the canister from here

  wm.startFeedingPause(10);

  // Refusing matters more than the relay position: the cycle switches the
  // filter off too, so the pin alone cannot tell the two apart. What must not
  // happen is a pause that later switches the filter on mid-cycle.
  TEST_ASSERT_FALSE(wm.isFeedingPause());
}

void test_feeding_pause_lets_go_when_the_filter_is_switched_on_by_hand() {
  WaterManager wm = makeWM();
  allowCanisterRestore(wm);
  digitalWrite(PIN_CANISTER, LOW);
  wm.startFeedingPause(10);

  digitalWrite(PIN_CANISTER, LOW); // the header toggle, mid-pause
  wm.update();

  // The pause is over because someone else decided, and it leaves the relay
  // exactly where that someone put it.
  TEST_ASSERT_FALSE(wm.isFeedingPause());
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_CANISTER]);
}

void test_feeding_pause_of_zero_minutes_does_nothing() {
  WaterManager wm = makeWM();
  digitalWrite(PIN_CANISTER, LOW);

  wm.startFeedingPause(0);

  TEST_ASSERT_FALSE(wm.isFeedingPause());
  TEST_ASSERT_EQUAL(LOW, mock_pin_state[PIN_CANISTER]);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_initial_state_is_idle);
  RUN_TEST(test_start_tpa_transitions_to_filling_reservoir);
  RUN_TEST(test_start_tpa_blocked_during_emergency);
  RUN_TEST(test_double_start_ignored);
  RUN_TEST(test_canister_off_disables_relay);

  // Feeding pause
  RUN_TEST(test_feeding_pause_switches_the_canister_off);
  RUN_TEST(test_feeding_pause_restores_the_canister_when_time_is_up);
  RUN_TEST(test_feeding_pause_ends_early);
  RUN_TEST(test_feeding_pause_refused_during_a_cycle);
  RUN_TEST(test_feeding_pause_lets_go_when_the_filter_is_switched_on_by_hand);
  RUN_TEST(test_feeding_pause_of_zero_minutes_does_nothing);
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
  RUN_TEST(test_refill_resumes_when_settled_reading_is_short);
  RUN_TEST(test_dynamic_timeout_drain);
  RUN_TEST(test_refill_errors_when_level_stops_moving);
  RUN_TEST(test_refill_survives_progress_check_while_filling);
  RUN_TEST(test_refill_stall_check_uses_snapshot_not_live_rate);
  RUN_TEST(test_refill_survives_a_wildly_overstated_calibration);
  RUN_TEST(test_dynamic_timeout_refill);
  RUN_TEST(test_uncalibrated_defaults_are_short);
  RUN_TEST(test_measurement_above_the_rating_is_refused);
  RUN_TEST(test_declaring_the_rating_discards_a_stored_impossible_rate);
  RUN_TEST(test_no_rating_accepts_any_measurement);
  RUN_TEST(test_planning_rate_takes_the_slower_of_the_two);
  RUN_TEST(test_stall_check_expects_no_more_than_the_rating);
  RUN_TEST(test_is_calibrated_getter);

  // Sensor-based progress
  RUN_TEST(test_progress_uses_sensor_data);

  UNITY_END();
  return 0;
}
