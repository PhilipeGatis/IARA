#include "WaterManager.h"
#include <cmath> // fabsf
#include "FertManager.h"
#include "PumpLog.h"
#include "SafetyWatchdog.h"

const char *tpaStateName(TPAState s) {
  switch (s) {
  case TPAState::IDLE:
    return "IDLE";
  case TPAState::CANISTER_OFF:
    return "CANISTER_OFF";
  case TPAState::DRAINING:
    return "DRAINING";
  case TPAState::FILLING_RESERVOIR:
    return "FILLING_RESERVOIR";
  case TPAState::DOSING_PRIME:
    return "DOSING_PRIME";
  case TPAState::REFILLING:
    return "REFILLING";
  case TPAState::CANISTER_ON:
    return "CANISTER_ON";
  case TPAState::COMPLETE:
    return "COMPLETE";
  case TPAState::ERROR:
    return "ERROR";
  case TPAState::MANUAL_RESERVOIR_FILL:
    return "MANUAL_RESERVOIR_FILL";
  case TPAState::MANUAL_PUMP_DRAIN:
    return "MANUAL_PUMP_DRAIN";
  case TPAState::MANUAL_PUMP_REFILL:
    return "MANUAL_PUMP_REFILL";
  default:
    return "UNKNOWN";
  }
}

WaterManager::WaterManager()
    : _state(TPAState::IDLE), _safety(nullptr), _fert(nullptr), _isManualTPA(false),
      _stateStartMs(0), _waitUntilMs(0), _doseCompleted(false),
      _drainTargetCm(0), _refillTargetCm(0),
      _canisterSafeLevelCm(15.0f), // Default: 15cm (safe for most canisters)
      _primeML(0), _timeoutDrainMs(1200000),   // Default: 20 min
      _timeoutRefillMs(1200000), // Default: 20 min
      _litersPerCm(0), _aqEffectiveHeightCm(0), _calStartLevel(0),
      _calStartMs(0), _drainFlowLPM(0), _refillFlowLPM(0), _lastTPATime("N/A"),
      _lastErrorMsg(""), _manualPumpTarget(""), _manualPumpGoalLiters(0) {}

void WaterManager::begin(SafetyWatchdog *safety, FertManager *fert) {
  _safety = safety;
  _fert = fert;
  Serial.println("[TPA] WaterManager initialized.");
}

// ============================================================================
// START / ABORT
// ============================================================================

void WaterManager::_resetCycleState() {
  // Everything that is scoped to one run. Left stale, each of these silently
  // changes what the next cycle does: a non-zero _waitUntilMs makes
  // _handleCanisterOff() skip switching the filter off, _doseCompleted makes
  // DOSING_PRIME believe it already dosed, and _pairedCalibration chains an
  // unrequested refill onto the end of an ordinary manual drain.
  _waitUntilMs = 0;
  _doseCompleted = false;
  _pairedCalibration = false;
  _calibrationRunMs = 0;
  _canisterOffForManual = false;
  _refillConfirming = false;
  _manualTargetLevelCm = -1;
  _wasFullCycle = false;
  _primeDoseStarted = false;
  _primeWaitStartedMs = 0;
  _pairedRefillTargetCm = -1;
}

void WaterManager::startTPA(bool manual) {
  if (isRunning()) {
    Serial.println("[Water] Cannot start TPA: already running.");
    return;
  }
  if (_safety && _safety->isEmergency()) {
    Serial.println("[Water] Cannot start TPA: emergency mode.");
    return;
  }
  if (_safety && _safety->isMaintenanceMode()) {
    Serial.println("[Water] Cannot start TPA: maintenance mode.");
    return;
  }

  Serial.println("[Water] === TPA CYCLE STARTED ===");
  _resetCycleState();
  _isManualTPA = manual;
  _wasFullCycle = true;
  // The reservoir comes first on purpose. Everything that can fail here — no
  // mains pressure, a stuck float, a dead solenoid — fails while the aquarium
  // is still full and the canister still running. Draining first would leave a
  // low tank, a stopped filter and no treated water to put back.
  _enterState(TPAState::FILLING_RESERVOIR);
}

void WaterManager::abortTPA() {
  Serial.println("[TPA] !!! TPA ABORTED !!!");
  if (_state == TPAState::DRAINING || _state == TPAState::MANUAL_PUMP_DRAIN) {
    _captureDrainCalibration();
  } else if (_state == TPAState::REFILLING || _state == TPAState::MANUAL_PUMP_REFILL) {
    _captureRefillCalibration();
  }
  _stopAllTpaActuators(PumpReason::ABORT);
  // Abort is most likely pressed mid-drain, which is the worst moment to start
  // the filter: its intake may be above the surface. Same rule as every other
  // restore path.
  restoreCanisterIfSafe(PumpReason::ABORT);
  _resetCycleState();
  _state = TPAState::ERROR;
}

void WaterManager::startManualReservoirFill() {
  if (isRunning()) {
    Serial.println("[TPA] Already running, ignoring startManualReservoirFill().");
    return;
  }
  Serial.println("[TPA] ====== MANUAL RESERVOIR FILL STARTED ======");
  _enterState(TPAState::MANUAL_RESERVOIR_FILL);
}

void WaterManager::startManualPump(const String &pump, float goalLiters) {
  if (isRunning()) {
    Serial.println("[TPA] Already running, ignoring startManualPump().");
    return;
  }

  // A volume goal can only be honoured with something to measure it against:
  // a working level sensor (with litersPerCm known) or a calibrated flow rate.
  // With neither, the pump would just run to the timeout, which is how an
  // uncalibrated manual run used to become half an hour of pumping.
  if (goalLiters > 0) {
    const bool sensorOk =
        _safety && _safety->areSensorsConnected() && _litersPerCm > 0;
    const float flow = (pump == "drain") ? _drainFlowLPM : _refillFlowLPM;
    if (!sensorOk && flow <= 0) {
      Serial.println("[TPA] Manual pump refused: no level sensor and no "
                     "calibrated flow rate to track the goal.");
      _lastErrorMsg = "Manual pump needs a level sensor or a calibrated flow";
      return;
    }
  }

  // Starting is the only moment guaranteed to happen. Clearing per-run state
  // only when a run *ends* misses the run that ends in COMPLETE, which goes
  // through neither stopManual() nor abortTPA() nor _error() — so a paired
  // calibration that finished normally left _pairedRefillTargetCm behind, and
  // the next plain manual refill stopped on the previous run's setpoint, in
  // under a second, reporting its goal as reached.
  _resetCycleState();

  _manualPumpTarget = pump;
  _manualPumpGoalLiters = goalLiters;
  _calibrationRunMs = 0;
  _stopCanisterForManualRun();

  Serial.printf("[TPA] ====== MANUAL PUMP STARTED: %s (Goal: %.1f L) ======\n", pump.c_str(), goalLiters);

  if (pump == "drain") {
    _enterState(TPAState::MANUAL_PUMP_DRAIN);
  } else if (pump == "refill") {
    _enterState(TPAState::MANUAL_PUMP_REFILL);
  }
}

void WaterManager::_beginPumpCalibration(const String &pump) {
  _manualPumpTarget = pump;
  _manualPumpGoalLiters = 0; // no goal: the level change ends this run
  _calibrationRunMs = PUMP_CALIBRATION_MAX_MS;
  _stopCanisterForManualRun();

  Serial.printf("[TPA] ====== FLOW CALIBRATION: %s until the level moves "
                "%.1f%% (max %lus) ======\n",
                pump.c_str(), CALIBRATION_MIN_DELTA_PCT,
                PUMP_CALIBRATION_MAX_MS / 1000);

  if (pump == "drain") {
    _enterState(TPAState::MANUAL_PUMP_DRAIN);
  } else if (pump == "refill") {
    _enterState(TPAState::MANUAL_PUMP_REFILL);
  }
}

void WaterManager::startPumpCalibration(const String &pump) {
  if (isRunning()) {
    Serial.println("[TPA] Already running, ignoring startPumpCalibration().");
    return;
  }
  if (_litersPerCm <= 0) {
    Serial.println("[TPA] Calibration refused: aquarium dimensions unknown, "
                   "so a level change cannot be converted to litres.");
    _lastErrorMsg = "Set the aquarium dimensions before calibrating";
    return;
  }
  _pairedCalibration = false;
  _beginPumpCalibration(pump);
}

void WaterManager::startPairedCalibration() {
  if (isRunning()) {
    Serial.println("[TPA] Already running, ignoring startPairedCalibration().");
    return;
  }
  if (_litersPerCm <= 0) {
    Serial.println("[TPA] Calibration refused: aquarium dimensions unknown.");
    _lastErrorMsg = "Set the aquarium dimensions before calibrating";
    return;
  }

  Serial.println("[TPA] ====== PAIRED CALIBRATION: drain, then refill ======");
  _resetCycleState();
  _pairedCalibration = true;
  _beginPumpCalibration("drain");
}

void WaterManager::stopManual() {
  Serial.println("[TPA] Manual operation stopped.");
  _calibrationRunMs = 0;
  _pairedCalibration = false;
  if (_state == TPAState::MANUAL_PUMP_DRAIN) {
    _captureDrainCalibration();
    saveCalibration(); // a manual run is how the flow rate gets measured in
                       // the first place, so it must outlive the reboot
  } else if (_state == TPAState::MANUAL_PUMP_REFILL) {
    _captureRefillCalibration();
    saveCalibration();
  }
  _stopAllTpaActuators(PumpReason::MANUAL_PUMP);
  if (_canisterOffForManual) {
    restoreCanisterIfSafe(PumpReason::MANUAL_PUMP);
  }
  _resetCycleState();
  _state = TPAState::IDLE;
}

float WaterManager::getPumpGoalLiters() const {
  if (_state == TPAState::MANUAL_PUMP_DRAIN || _state == TPAState::MANUAL_PUMP_REFILL) {
    return _manualPumpGoalLiters;
  } else if (_state == TPAState::DRAINING) {
    return (_drainTargetCm > _calStartLevel && _litersPerCm > 0) ? (_drainTargetCm - _calStartLevel) * _litersPerCm : 0.0f;
  } else if (_state == TPAState::REFILLING) {
    return (_calStartLevel > _refillTargetCm && _litersPerCm > 0) ? (_calStartLevel - _refillTargetCm) * _litersPerCm : 0.0f;
  }
  return 0.0f;
}

float WaterManager::getPumpProgressLiters() const {
  const bool draining =
      (_state == TPAState::DRAINING || _state == TPAState::MANUAL_PUMP_DRAIN);

  // Measured progress: how far the surface has actually moved. Once the sensor
  // is usable this is the only answer given — falling through to the time
  // estimate whenever the delta was not yet positive made the reading flip
  // between a small real number and a large estimated one, which is what made
  // the progress bar jump around.
  if (_safety && _safety->areSensorsConnected() && _litersPerCm > 0 &&
      _calStartLevel > 0) {
    const float currentLevel = _safety->getLastDistance();
    if (currentLevel > 0) {
      const float deltaCm = draining ? (currentLevel - _calStartLevel)  // level falls, distance grows
                                     : (_calStartLevel - currentLevel); // level rises, distance shrinks
      return deltaCm > 0 ? deltaCm * _litersPerCm : 0.0f;
    }
  }

  // No sensor: fall back to the flow estimate, which is all that is left.
  if (draining) {
    return (_stateElapsed() / 60000.0f) * _drainFlowLPM;
  }
  if (_state == TPAState::MANUAL_PUMP_REFILL || _state == TPAState::REFILLING) {
    return (_stateElapsed() / 60000.0f) * _refillFlowLPM;
  }
  return 0.0f;
}

unsigned long WaterManager::getPumpElapsedMs() const {
  if (_state == TPAState::IDLE || _state == TPAState::COMPLETE || _state == TPAState::ERROR) {
    return 0;
  }
  return _stateElapsed();
}

void WaterManager::update() {
  if (_state == TPAState::IDLE || _state == TPAState::COMPLETE ||
      _state == TPAState::ERROR) {
    return;
  }

  // Check emergency state from watchdog
  if (_safety && _safety->isEmergency()) {
    Serial.println("[TPA] Emergency detected during TPA — aborting.");
    abortTPA();
    return;
  }

  // Dispatch to current state handler
  switch (_state) {
  case TPAState::CANISTER_OFF:
    _handleCanisterOff();
    break;
  case TPAState::DRAINING:
    _handleDraining();
    break;
  case TPAState::FILLING_RESERVOIR:
    _handleFillingReservoir();
    break;
  case TPAState::DOSING_PRIME:
    _handleDosingPrime();
    break;
  case TPAState::REFILLING:
    _handleRefilling();
    break;
  case TPAState::CANISTER_ON:
    _handleCanisterOn();
    break;
  case TPAState::MANUAL_RESERVOIR_FILL:
    _handleManualReservoirFill();
    break;
  case TPAState::MANUAL_PUMP_DRAIN:
    _handleManualPump(PIN_DRAIN, _drainFlowLPM);
    break;
  case TPAState::MANUAL_PUMP_REFILL:
    _handleManualPump(PIN_REFILL, _refillFlowLPM);
    break;
  default:
    break;
  }
}

// ============================================================================
// STATE TRANSITIONS
// ============================================================================

void WaterManager::_enterState(TPAState newState) {
  _state = newState;
  _stateStartMs = millis();
  _floatFullCount = 0;       // Reset debounce on state transition
  _refillConfirming = false; // and any pending refill confirmation
  Serial.printf("[TPA] -> State: %s\n", tpaStateName(newState));
}

// ============================================================================
// STATE HANDLERS
// ============================================================================

void WaterManager::_handleCanisterOff() {
  // Step 1: Turn off canister filter (SSR: HIGH = OFF)
  if (_waitUntilMs == 0) {
    // First call: turn off canister and start the non-blocking wait
    pumpOn(PIN_CANISTER, PumpReason::TPA_CANISTER); // HIGH = OFF for SSR
    Serial.println("[TPA] Canister OFF. Waiting 3s for water to settle...");
    _waitUntilMs = millis() + 3000;
    return;
  }

  // Subsequent calls: check if wait has elapsed
  if (!_isWaiting()) {
    _waitUntilMs = 0;
    _enterState(TPAState::DRAINING);
  }
}

void WaterManager::_handleDraining() {
  // Step 2: Drain until ultrasonic shows target level
  // Ensure drain pump is ON at the start of this state
  if (digitalRead(PIN_DRAIN) == LOW) {
    pumpOn(PIN_DRAIN, PumpReason::TPA_DRAINING);
    Serial.printf("[TPA] Drain pump ON. Target: %.1f cm\n", _drainTargetCm);
    // Record calibration start point
    if (_safety && _litersPerCm > 0) {
      _calStartLevel = _safety->readUltrasonic();
      _calStartMs = millis();
    }
  }

  // A stale reading is worse than no reading: readUltrasonic() keeps returning
  // the last good value when the sensor goes quiet, so the setpoint would never
  // be met and the pump would run to the timeout against a frozen number.
  if (_safety && !_safety->areSensorsConnected()) {
    pumpOff(PIN_DRAIN, PumpReason::ERROR_STOP);
    _error("Ultrasonic sensor lost during drain!");
    return;
  }

  // Read ultrasonic
  if (_safety) {
    float dist = _safety->readUltrasonic();

    // Live flow rate recalibration during operation
    if (dist > 0 && _calStartMs > 0 && _litersPerCm > 0) {
      float liveRate = _calcFlowRate(_calStartLevel, dist, _calStartMs);
      if (liveRate > 0.01f) _drainFlowLPM = liveRate;
    }

    if (dist >= _drainTargetCm) {
      // Target reached (higher distance = lower water)
      Serial.printf("[TPA] Drain target reached: %.1f cm\n", dist);
      pumpOff(PIN_DRAIN, PumpReason::TPA_TARGET_REACHED);
      Serial.printf("[TPA] Drain calibrated: %.2f L/min\n", _drainFlowLPM);

      _enterState(TPAState::REFILLING);
      return;
    }
  }

  // Timeout check (uses dynamic timeout)
  if (_stateElapsed() >= _timeoutDrainMs) {
    // Even on timeout, capture partial calibration data
    if (_safety && _calStartMs > 0 && _litersPerCm > 0) {
      float dist = _safety->readUltrasonic();
      float flowRate = _calcFlowRate(_calStartLevel, dist, _calStartMs);
      if (flowRate > 0) {
        _drainFlowLPM = flowRate;
        Serial.printf("[TPA] Drain calibrated on timeout: %.2f L/min\n", _drainFlowLPM);
      }
    }
    _error("Drain timeout exceeded!");
    return;
  }
}

void WaterManager::_handleFillingReservoir() {
  // Step 1: top the reservoir up, unless it is already full. Check before
  // opening, so an already-full reservoir does not get a pointless pulse of
  // mains water.
  if (_isReservoirFullDebounced()) {
    Serial.println("[TPA] Reservoir FULL (float switch triggered).");
    pumpOff(PIN_SOLENOID, PumpReason::TPA_TARGET_REACHED);
    _enterState(TPAState::DOSING_PRIME);
    return;
  }

  if (digitalRead(PIN_SOLENOID) == LOW) {
    pumpOn(PIN_SOLENOID, PumpReason::TPA_SOLENOID);
    Serial.println("[TPA] Solenoid OPEN. Filling reservoir...");
  }

  if (_stateElapsed() >= TIMEOUT_RESERVOIR_FILL_MS) {
    pumpOff(PIN_SOLENOID, PumpReason::ERROR_STOP);
    _error("Reservoir fill timeout exceeded!");
    return;
  }
}

void WaterManager::_handleDosingPrime() {
  // Step 4: Dose Prime (dechlorinator) into reservoir
  if (!_primeEnabled) {
    _enterState(TPAState::CANISTER_OFF);
    return;
  }

  if (!_doseCompleted) {
    if (_fert && _primeML > 0) {
      if (!_primeDoseStarted) {
        // Dosing no longer blocks, so the pump runs while the rest of the
        // system keeps ticking. Something else holding the channel is the only
        // reason this can fail, and it should clear within a dose duration.
        if (!_fert->startDose(NUM_FERTS, _primeML)) { // Channel 4 = Prime
          if (_primeWaitStartedMs == 0)
            _primeWaitStartedMs = millis();
          if (millis() - _primeWaitStartedMs > TIMEOUT_PRIME_MS) {
            _error("Prime channel busy");
          }
          return;
        }
        Serial.printf("[TPA] Dosing Prime: %.1f ml\n", _primeML);
        _primeDoseStarted = true;
        // Deduct from Prime stock
        float stock = _fert->getStockML(NUM_FERTS);
        _fert->setStockML(NUM_FERTS, stock - _primeML);
        _fert->saveState();
        return;
      }
      if (_fert->isDosing())
        return; // pump still running
    }
    _doseCompleted = true;
    _waitUntilMs = millis() + 2000; // Let Prime mix
    return;
  }

  // Subsequent calls: check if mixing wait has elapsed
  if (!_isWaiting()) {
    _waitUntilMs = 0;
    _doseCompleted = false;
    _enterState(TPAState::CANISTER_OFF);
  }
}

void WaterManager::_handleRefilling() {
  // NOTE: max-level cutoff is a hardware reed switch in series with the refill
  // MOSFET gate signal. It kills the pump without the firmware being involved,
  // so there is no max-level sensor to poll here.

  // Same reasoning as DRAINING: acting on a frozen level while pumping water
  // *into* the aquarium is the direction that overflows.
  if (_safety && !_safety->areSensorsConnected()) {
    pumpOff(PIN_REFILL, PumpReason::ERROR_STOP);
    _error("Ultrasonic sensor lost during refill!");
    return;
  }

  // The setpoint looked reached, so the pump was stopped. Give the surface
  // time to settle, then decide on a calm reading instead of on one taken with
  // water pouring in beside the sensor.
  if (_refillConfirming) {
    if (_isWaiting())
      return;
    _waitUntilMs = 0;

    const float settled = _safety ? _safety->readUltrasonic() : -1;
    if (settled > 0 && settled <= _refillTargetCm) {
      Serial.printf("[TPA] Refill confirmed at %.1f cm after settling.\n",
                    settled);
      _captureRefillCalibration();
      Serial.printf("[TPA] Refill calibrated: %.2f L/min\n", _refillFlowLPM);
      _enterState(TPAState::CANISTER_ON);
      return;
    }

    Serial.printf("[TPA] Settled at %.1f cm, still short of %.1f. Resuming.\n",
                  settled, _refillTargetCm);
    _refillConfirming = false; // fall through and restart the pump
  }

  // Step 5: Refill tank until the ultrasonic setpoint is reached
  if (digitalRead(PIN_REFILL) == LOW) {
    pumpOn(PIN_REFILL, PumpReason::TPA_REFILLING);
    Serial.printf("[TPA] Refill pump ON. Target: %.1f cm\n", _refillTargetCm);
    // Record calibration start point
    if (_safety && _litersPerCm > 0) {
      _calStartLevel = _safety->readUltrasonic();
      _calStartMs = millis();
    }
  }

  // Ultrasonic setpoint check + live recalibration
  if (_safety) {
    float dist = _safety->readUltrasonic();

    // Live flow rate recalibration during operation
    if (dist > 0 && _calStartMs > 0 && _litersPerCm > 0) {
      float liveRate = _calcFlowRate(_calStartLevel, dist, _calStartMs);
      if (liveRate > 0.01f) _refillFlowLPM = liveRate;
    }

    if (dist > 0 && dist <= _refillTargetCm) {
      Serial.printf("[TPA] Refill setpoint seen at %.1f cm. Settling %lums "
                    "before confirming.\n",
                    dist, REFILL_SETTLE_MS);
      pumpOff(PIN_REFILL, PumpReason::TPA_TARGET_REACHED);
      _refillConfirming = true;
      _waitUntilMs = millis() + REFILL_SETTLE_MS;
      return;
    }
  }

  // Timeout check (uses dynamic timeout)
  if (_stateElapsed() >= _timeoutRefillMs) {
    pumpOff(PIN_REFILL, PumpReason::ERROR_STOP);
    _captureRefillCalibration();
    _error("Refill timeout exceeded!");
    return;
  }
}

void WaterManager::_handleCanisterOn() {
  // Step 6: bring the filter back, subject to the same level check as every
  // other restore. A refill confirmed on a bad reading would otherwise end the
  // cycle by starting the canister below its safe mark.
  restoreCanisterIfSafe(PumpReason::TPA_CANISTER);
  Serial.println("[TPA] TPA cycle COMPLETE.");

  _state = TPAState::COMPLETE;
}

void WaterManager::_handleManualReservoirFill() {
  if (digitalRead(PIN_SOLENOID) == LOW) {
    pumpOn(PIN_SOLENOID, PumpReason::MANUAL_SOLENOID);
    Serial.println("[TPA] Solenoid OPEN. Manual filling reservoir...");
  }

  if (_isReservoirFullDebounced()) {
    Serial.println("[TPA] Reservoir FULL. Manual fill complete.");
    pumpOff(PIN_SOLENOID, PumpReason::TPA_TARGET_REACHED);
    _state = TPAState::COMPLETE;
    return;
  }

  if (_stateElapsed() >= TIMEOUT_RESERVOIR_FILL_MS) {
    pumpOff(PIN_SOLENOID, PumpReason::ERROR_STOP);
    _error("Manual reservoir fill timeout exceeded!");
    return;
  }
}

// DRY #3: Unified manual pump handler for both drain and refill
void WaterManager::_handleManualPump(uint8_t pin, float flowLPM) {
  // Let the surface settle after the canister stopped, so the start level the
  // calibration is measured against is a calm reading.
  if (_isWaiting())
    return;
  _waitUntilMs = 0;

  const bool isDrain = (pin == PIN_DRAIN);

  // First tick: start the pump and snapshot the level, so the goal is tracked
  // against the sensor — the same closed loop the automatic TPA states use —
  // instead of against elapsed time alone.
  if (digitalRead(pin) == LOW) {
    pumpOn(pin, PumpReason::MANUAL_PUMP);
    _calStartMs = millis();
    _calStartLevel =
        (_safety && _litersPerCm > 0) ? _safety->readUltrasonic() : -1;

    _manualTargetLevelCm = -1;
    if (_manualPumpGoalLiters > 0 && _litersPerCm > 0 && _calStartLevel > 0) {
      // Draining lowers the water, so the measured distance grows.
      // Refilling raises it, so the distance shrinks.
      const float deltaCm = _manualPumpGoalLiters / _litersPerCm;
      _manualTargetLevelCm = isDrain ? (_calStartLevel + deltaCm)
                                     : (_calStartLevel - deltaCm);
      Serial.printf("[TPA] Manual %s target level: %.1f cm\n", pinName(pin),
                    _manualTargetLevelCm);
    }

    // Size the timeout from the job instead of using a flat half hour, so a
    // stalled or miscalibrated pump is caught in minutes.
    _manualTimeoutMs = MANUAL_PUMP_MAX_MS;
    const bool sensorWillArbitrate =
        _manualTargetLevelCm > 0 && _safety && _safety->areSensorsConnected();
    if (_calibrationRunMs > 0) {
      // Give the calibration window room to finish before the timeout bites.
      _manualTimeoutMs = _calibrationRunMs * 2;
    } else if (sensorWillArbitrate) {
      // The sensor decides when to stop, so sizing the timeout from a possibly
      // wrong flow rate would only cut a healthy run short.
      _manualTimeoutMs = MANUAL_PUMP_MAX_MS;
    } else if (_manualPumpGoalLiters > 0 && flowLPM > 0) {
      unsigned long budget =
          (unsigned long)((_manualPumpGoalLiters / flowLPM) * 60000.0f) * 2;
      if (budget < MANUAL_PUMP_MIN_MS) budget = MANUAL_PUMP_MIN_MS;
      if (budget < _manualTimeoutMs) _manualTimeoutMs = budget;
    }
  }

  bool reached = false;

  // Calibration run: it ends when the level has moved far enough to measure,
  // not after a fixed time. A slow pump then simply runs longer instead of
  // producing a number dominated by sensor noise.
  if (_calibrationRunMs > 0 && _safety && _aqEffectiveHeightCm > 0 &&
      _calStartLevel > 0) {
    const float level = _safety->readUltrasonic();
    if (level <= 0) {
      // A calibration is a measurement, and there is nothing to measure. A
      // *stale* reading needs no special case: _calStartLevel came from the
      // same frozen value, so `moved` stays at zero and the run ends on
      // _calibrationRunMs with the sample discarded.
      pumpOff(pin, PumpReason::ERROR_STOP);
      _error("Sensor lost during calibration");
      return;
    }
    const float moved = fabsf(level - _calStartLevel);
    // Overshoot the acceptance floor deliberately, so the independent reading
    // _calcFlowRate() takes clears it rather than landing either side of it.
    const float target = _aqEffectiveHeightCm *
                         (CALIBRATION_MIN_DELTA_PCT / 100.0f) *
                         CALIBRATION_STOP_MARGIN;
    if (moved >= target) {
      Serial.printf("[TPA] Calibration: %s moved %.2f cm (>= %.2f). Done.\n",
                    pinName(pin), moved, target);
      reached = true;
    } else if (_stateElapsed() >= _calibrationRunMs) {
      Serial.printf("[TPA] Calibration: %s only moved %.2f cm of %.2f in the "
                    "time allowed. Discarding.\n",
                    pinName(pin), moved, target);
      reached = true; // stop the pump; _calcFlowRate() rejects the sample
    }
  }

  // Second leg of a pair: stop at the level the drain leg started from, not
  // after an independent 5% move.
  if (!reached && _pairedCalibration && _pairedRefillTargetCm > 0 && !isDrain &&
      _safety && _safety->areSensorsConnected()) {
    const float dist = _safety->readUltrasonic();
    if (dist > 0 && dist <= _pairedRefillTargetCm) {
      Serial.printf("[TPA] Paired refill back to start level: %.1f cm\n", dist);
      reached = true;
    }
  }

  // Primary stop: the ultrasonic reaching the target level.
  if (_manualTargetLevelCm > 0 && _safety && _safety->areSensorsConnected()) {
    const float dist = _safety->readUltrasonic();
    if (dist > 0 && (isDrain ? (dist >= _manualTargetLevelCm)
                             : (dist <= _manualTargetLevelCm))) {
      Serial.printf("[TPA] Manual %s goal reached (sensor): %.1f cm\n",
                    pinName(pin), dist);
      reached = true;
    }
  }

  // Backstop: flow x time, and ONLY when there is no sensor to trust. It used
  // to race the sensor, so an over-estimated flow rate ended the run early —
  // a refill calibrated at 19 L/min against a real 2-3 L/min stopped after
  // seconds, reporting a goal it had not moved. Measured beats estimated.
  const bool sensorArbitrates =
      _manualTargetLevelCm > 0 && _safety && _safety->areSensorsConnected();
  if (!reached && !sensorArbitrates && _manualPumpGoalLiters > 0 &&
      flowLPM > 0) {
    const float pumpedLiters = (_stateElapsed() / 60000.0f) * flowLPM;
    if (pumpedLiters >= _manualPumpGoalLiters) {
      Serial.printf("[TPA] Manual %s goal reached (flow estimate, no sensor): "
                    "%.1f L\n",
                    pinName(pin), pumpedLiters);
      reached = true;
    }
  }

  if (reached) {
    if (isDrain) _captureDrainCalibration();
    else _captureRefillCalibration();
    pumpOff(pin, PumpReason::TPA_TARGET_REACHED);

    // Second leg of a paired run: put the water back, measuring the refill on
    // the way. The canister stays off across both legs.
    if (_pairedCalibration && isDrain) {
      if (_drainFlowLPM <= 0) {
        // The drain sample was rejected, so there is nothing for the refill leg
        // to be measured against, and running it would move the tank by an
        // amount nobody chose.
        _pairedCalibration = false;
        _error("Drain calibration produced no usable rate");
        return;
      }
      Serial.println("[TPA] Drain leg done. Refilling to where we started...");
      // The pair exists so the tank ends where it began. The refill leg used to
      // re-snapshot its own start level and run until the water moved another
      // 5% from there — with no reference to the drain leg at all. An
      // under-delivering drain then still got a full refill, ending the tank
      // *above* where it started, which is a shorter distance to the overflow
      // threshold than the tolerance allows for.
      _pairedRefillTargetCm = _calStartLevel;
      _beginPumpCalibration("refill");
      return;
    }
    _pairedCalibration = false;

    if (_canisterOffForManual) {
      _canisterOffForManual = false;
      restoreCanisterIfSafe(PumpReason::MANUAL_PUMP);
    }
    _state = TPAState::COMPLETE;
    return;
  }

  if (_stateElapsed() >= _manualTimeoutMs) {
    pumpOff(pin, PumpReason::ERROR_STOP);
    _error("Manual pump timeout exceeded!");
  }
}

// DRY #4: Extract flow rate calculation
float WaterManager::_calcFlowRate(float startLevel, float endLevel, unsigned long startMs) const {
  if (_litersPerCm <= 0 || startMs == 0) return 0;

  const float deltaLevel = fabsf(endLevel - startLevel);

  // Refuse to derive a rate from a level change small enough to be noise. The
  // ultrasonic's spread is a fixed number of millimetres, so a short run mostly
  // measures that spread — and the TPA recalibrates on every tick, which would
  // otherwise let the first seconds of a drain overwrite a good calibration.
  if (_aqEffectiveHeightCm > 0) {
    const float minDeltaCm =
        _aqEffectiveHeightCm * (CALIBRATION_MIN_DELTA_PCT / 100.0f);
    if (deltaLevel < minDeltaCm)
      return 0;
  }

  const float deltaLiters = deltaLevel * _litersPerCm;
  const float deltaMinutes = (float)(millis() - startMs) / 60000.0f;
  if (deltaMinutes > 0.1f && deltaLiters > 0.1f) {
    return deltaLiters / deltaMinutes;
  }
  return 0;
}

void WaterManager::_captureDrainCalibration() {
  if (_calStartMs > 0 && _litersPerCm > 0 && _safety) {
    float dist = _safety->readUltrasonic();
    float flowRate = _calcFlowRate(_calStartLevel, dist, _calStartMs);
    if (flowRate > 0) {
      _drainFlowLPM = flowRate;
      Serial.printf("[TPA] Drain calibrated: %.2f L/min\n", _drainFlowLPM);
    }
  }
}

void WaterManager::_captureRefillCalibration() {
  if (_calStartMs > 0 && _litersPerCm > 0 && _safety) {
    float dist = _safety->readUltrasonic();
    float flowRate = _calcFlowRate(_calStartLevel, dist, _calStartMs);
    if (flowRate > 0) {
      _refillFlowLPM = flowRate;
      Serial.printf("[TPA] Refill calibrated: %.2f L/min\n", _refillFlowLPM);
    }
  }
}

void WaterManager::_stopCanisterForManualRun() {
  // The canister's outflow ripples the surface, and the ultrasonic reads that
  // ripple as level noise — which is exactly what a manual run or a flow
  // calibration is trying to measure. Stop it, and let the water settle before
  // the pump starts.
  if (isCanisterOn()) {
    pumpOn(PIN_CANISTER, PumpReason::TPA_CANISTER); // SSR: HIGH = OFF
    _canisterOffForManual = true;
    Serial.println("[TPA] Canister OFF for the manual run. Settling 3s...");
  }
  _waitUntilMs = millis() + 3000;
}

bool WaterManager::restoreCanisterIfSafe(PumpReason reason) {
  if (!_safety) {
    Serial.println("[TPA] Canister stays OFF — no sensor to verify the level.");
    return false;
  }

  const float dist = _safety->readUltrasonic();
  const float waterPct =
      (_aqEffectiveHeightCm > 0)
          ? ((_aqEffectiveHeightCm - dist) / _aqEffectiveHeightCm * 100.0f)
          : 0;
  const float safePct =
      (_aqEffectiveHeightCm > 0)
          ? ((_aqEffectiveHeightCm - _canisterSafeLevelCm) /
             _aqEffectiveHeightCm * 100.0f)
          : 0;

  // Low distance means high water. Running the canister with its intake above
  // the surface pulls air and damages the pump, so it stays off until the
  // level is back over the configured safe mark.
  if (dist > 0 && dist <= _canisterSafeLevelCm) {
    pumpOff(PIN_CANISTER, reason); // SSR: LOW = ON
    Serial.printf("[TPA] Canister ON (level %.0f%% is safe, limit %.0f%%).\n",
                  waterPct < 0 ? 0 : waterPct, safePct);
    return true;
  }

  Serial.printf("[TPA] Canister stays OFF — level %.0f%% too low (need >= "
                "%.0f%%).\n",
                waterPct < 0 ? 0 : waterPct, safePct);
  return false;
}

void WaterManager::_error(const char *msg) {
  Serial.printf("[TPA] ERROR: %s\n", msg);
  // Safety: turn off all TPA actuators
  _stopAllTpaActuators(PumpReason::ERROR_STOP);

  // Build detailed error message for notifications
  _lastErrorMsg = String(msg);

  // The canister only comes back if the level allows it.
  if (restoreCanisterIfSafe(PumpReason::ERROR_STOP)) {
    _lastErrorMsg += " | Canister: ON";
  } else {
    _lastErrorMsg += " | Canister: OFF (nivel baixo)";
  }
  _resetCycleState();

  _state = TPAState::ERROR;
}

// DRY #2: Stop all TPA-related actuators
void WaterManager::_stopAllTpaActuators(PumpReason reason) {
  pumpOff(PIN_DRAIN, reason);
  pumpOff(PIN_REFILL, reason);
  pumpOff(PIN_SOLENOID, reason);
  pumpOff(PIN_PRIME, reason);
}

// DRY #3: Centralized calibration persistence
void WaterManager::saveCalibration() {
  Preferences calPref;
  calPref.begin("pumpcal", false);
  if (_drainFlowLPM > 0)
    calPref.putFloat("drainLPM", _drainFlowLPM);
  if (_refillFlowLPM > 0)
    calPref.putFloat("refillLPM", _refillFlowLPM);
  calPref.end();
  Serial.printf("[TPA] Calibration saved: drain=%.2f refill=%.2f L/min\n",
                _drainFlowLPM, _refillFlowLPM);
}

void WaterManager::loadCalibration() {
  Preferences calPref;
  calPref.begin("pumpcal", true); // read-only
  float drainLPM = calPref.getFloat("drainLPM", 0);
  float refillLPM = calPref.getFloat("refillLPM", 0);
  calPref.end();
  if (drainLPM > 0) {
    _drainFlowLPM = drainLPM;
    Serial.printf("[TPA] Loaded drain calibration: %.2f L/min\n", drainLPM);
  }
  if (refillLPM > 0) {
    _refillFlowLPM = refillLPM;
    Serial.printf("[TPA] Loaded refill calibration: %.2f L/min\n", refillLPM);
  }
  if (drainLPM <= 0 && refillLPM <= 0) {
    Serial.println("[TPA] No pump calibration found. Using safe defaults.");
  }
}

// ============================================================================
// FLOAT SENSOR DEBOUNCE
// ============================================================================

bool WaterManager::_isReservoirFullDebounced() {
  if (_safety && _safety->isReservoirFull()) {
    _floatFullCount++;
    if (_floatFullCount >= FLOAT_DEBOUNCE_COUNT) {
      return true;
    }
  } else {
    _floatFullCount = 0;
  }
  return false;
}

// ============================================================================
// RESERVOIR SOLENOID CALIBRATION
// ============================================================================

