#include "WaterManager.h"
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
  _isManualTPA = manual;
  _enterState(TPAState::CANISTER_OFF);
}

void WaterManager::abortTPA() {
  Serial.println("[TPA] !!! TPA ABORTED !!!");
  if (_state == TPAState::DRAINING || _state == TPAState::MANUAL_PUMP_DRAIN) {
    _captureDrainCalibration();
  } else if (_state == TPAState::REFILLING || _state == TPAState::MANUAL_PUMP_REFILL) {
    _captureRefillCalibration();
  }
  _stopAllTpaActuators(PumpReason::ABORT);
  // Canister back on for safety (SSR: LOW = ON)
  pumpOff(PIN_CANISTER, PumpReason::ABORT);
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

  _manualPumpTarget = pump;
  _manualPumpGoalLiters = goalLiters;
  _calibrationRunMs = 0;

  Serial.printf("[TPA] ====== MANUAL PUMP STARTED: %s (Goal: %.1f L) ======\n", pump.c_str(), goalLiters);

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

  _manualPumpTarget = pump;
  _manualPumpGoalLiters = 0; // no goal: the clock ends this run
  _calibrationRunMs = PUMP_CALIBRATION_RUN_MS;

  Serial.printf("[TPA] ====== FLOW CALIBRATION: %s for %lus ======\n",
                pump.c_str(), PUMP_CALIBRATION_RUN_MS / 1000);

  if (pump == "drain") {
    _enterState(TPAState::MANUAL_PUMP_DRAIN);
  } else if (pump == "refill") {
    _enterState(TPAState::MANUAL_PUMP_REFILL);
  }
}

void WaterManager::stopManual() {
  Serial.println("[TPA] Manual operation stopped.");
  _calibrationRunMs = 0;
  if (_state == TPAState::MANUAL_PUMP_DRAIN) {
    _captureDrainCalibration();
    saveCalibration(); // a manual run is how the flow rate gets measured in
                       // the first place, so it must outlive the reboot
  } else if (_state == TPAState::MANUAL_PUMP_REFILL) {
    _captureRefillCalibration();
    saveCalibration();
  }
  _stopAllTpaActuators(PumpReason::MANUAL_PUMP);
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
  // Use real sensor data for accurate progress when available
  if (_safety && _litersPerCm > 0 && _calStartLevel > 0) {
    float currentLevel = _safety->getLastDistance();
    if (currentLevel > 0) {
      float deltaCm = (_state == TPAState::DRAINING || _state == TPAState::MANUAL_PUMP_DRAIN)
        ? (currentLevel - _calStartLevel)    // draining: distance increases
        : (_calStartLevel - currentLevel);   // refilling: distance decreases
      if (deltaCm > 0) return deltaCm * _litersPerCm;
    }
  }
  // Fallback: time-based estimation
  if (_state == TPAState::MANUAL_PUMP_DRAIN || _state == TPAState::DRAINING) {
    return (_stateElapsed() / 60000.0f) * _drainFlowLPM;
  } else if (_state == TPAState::MANUAL_PUMP_REFILL || _state == TPAState::REFILLING) {
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
  _floatFullCount = 0; // Reset debounce on state transition
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

      _enterState(TPAState::FILLING_RESERVOIR);
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
  // Step 3: Open solenoid until float switch indicates reservoir full
  if (digitalRead(PIN_SOLENOID) == LOW) {
    pumpOn(PIN_SOLENOID, PumpReason::TPA_SOLENOID);
    Serial.println("[TPA] Solenoid OPEN. Filling reservoir...");
  }

  if (_isReservoirFullDebounced()) {
    Serial.println("[TPA] Reservoir FULL (float switch triggered).");
    pumpOff(PIN_SOLENOID, PumpReason::TPA_TARGET_REACHED);
    _enterState(TPAState::DOSING_PRIME);
    return;
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
    _enterState(TPAState::REFILLING);
    return;
  }

  if (!_doseCompleted) {
    // First call: perform dosing
    if (_fert && _primeML > 0) {
      Serial.printf("[TPA] Dosing Prime: %.1f ml\n", _primeML);
      bool ok = _fert->doseChannel(NUM_FERTS, _primeML); // Channel 4 = Prime
      if (!ok) {
        Serial.println("[TPA] WARNING: Prime dosing may have timed out.");
      }
      // Deduct from Prime stock
      float stock = _fert->getStockML(NUM_FERTS);
      _fert->setStockML(NUM_FERTS, stock - _primeML);
      _fert->saveState();
    }
    _doseCompleted = true;
    _waitUntilMs = millis() + 2000; // Let Prime mix
    return;
  }

  // Subsequent calls: check if mixing wait has elapsed
  if (!_isWaiting()) {
    _waitUntilMs = 0;
    _doseCompleted = false;
    _enterState(TPAState::REFILLING);
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
      Serial.printf("[TPA] Refill setpoint reached: %.1f cm\n", dist);
      pumpOff(PIN_REFILL, PumpReason::TPA_TARGET_REACHED);
      Serial.printf("[TPA] Refill calibrated: %.2f L/min\n", _refillFlowLPM);
      _enterState(TPAState::CANISTER_ON);
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
  // Step 6: Turn canister filter back on (SSR: LOW = ON)
  pumpOff(PIN_CANISTER, PumpReason::TPA_CANISTER);
  Serial.println("[TPA] Canister ON. TPA cycle COMPLETE.");

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
    if (_calibrationRunMs > 0) {
      // Give the calibration window room to finish before the timeout bites.
      _manualTimeoutMs = _calibrationRunMs * 2;
    } else if (_manualPumpGoalLiters > 0 && flowLPM > 0) {
      unsigned long budget =
          (unsigned long)((_manualPumpGoalLiters / flowLPM) * 60000.0f) * 2;
      if (budget < MANUAL_PUMP_MIN_MS) budget = MANUAL_PUMP_MIN_MS;
      if (budget < _manualTimeoutMs) _manualTimeoutMs = budget;
    }
  }

  bool reached = false;

  // Calibration run: the clock ends it, and the level change measured over that
  // window becomes the flow rate.
  if (_calibrationRunMs > 0 && _stateElapsed() >= _calibrationRunMs) {
    Serial.printf("[TPA] Calibration window elapsed for %s.\n", pinName(pin));
    reached = true;
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

  // Backstop: flow x time, for when the sensor goes silent mid-run.
  if (!reached && _manualPumpGoalLiters > 0 && flowLPM > 0) {
    const float pumpedLiters = (_stateElapsed() / 60000.0f) * flowLPM;
    if (pumpedLiters >= _manualPumpGoalLiters) {
      Serial.printf("[TPA] Manual %s goal reached (flow estimate): %.1f L\n",
                    pinName(pin), pumpedLiters);
      reached = true;
    }
  }

  if (reached) {
    if (isDrain) _captureDrainCalibration();
    else _captureRefillCalibration();
    pumpOff(pin, PumpReason::TPA_TARGET_REACHED);
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
  float deltaLevel = (endLevel > startLevel) ? (endLevel - startLevel) : (startLevel - endLevel);
  float deltaLiters = deltaLevel * _litersPerCm;
  float deltaMinutes = (float)(millis() - startMs) / 60000.0f;
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

void WaterManager::_error(const char *msg) {
  Serial.printf("[TPA] ERROR: %s\n", msg);
  // Safety: turn off all TPA actuators
  _stopAllTpaActuators(PumpReason::ERROR_STOP);

  // Build detailed error message for notifications
  _lastErrorMsg = String(msg);

  // Only turn canister back on if water level is safe
  // (low distance = high water = safe for canister intake)
  if (_safety) {
    float dist = _safety->readUltrasonic();
    char buf[100];
    // Convert to percentage (low dist = high water = high %)
    float waterPct =
        (_aqEffectiveHeightCm > 0)
            ? ((_aqEffectiveHeightCm - dist) / _aqEffectiveHeightCm * 100.0f)
            : 0;
    float safePct = (_aqEffectiveHeightCm > 0)
                        ? ((_aqEffectiveHeightCm - _canisterSafeLevelCm) /
                           _aqEffectiveHeightCm * 100.0f)
                        : 0;
    if (waterPct < 0)
      waterPct = 0;
    if (dist > 0 && dist <= _canisterSafeLevelCm) {
      pumpOff(PIN_CANISTER, PumpReason::ERROR_STOP); // SSR: LOW = ON
      Serial.printf(
          "[TPA] Canister ON (water level %.0f%% is safe, limit: %.0f%%).\n",
          waterPct, safePct);
      snprintf(buf, sizeof(buf), " | Canister: ON (nivel %.0f%%)", waterPct);
    } else {
      Serial.printf("[TPA] WARNING: Canister stays OFF — water level %.0f%% "
                    "too low (need >= %.0f%%).\n",
                    waterPct, safePct);
      snprintf(buf, sizeof(buf), " | Canister: OFF (nivel %.0f%%, min: %.0f%%)",
              waterPct, safePct);
    }
    _lastErrorMsg += buf;
  } else {
    // No safety sensor — leave canister off for safety
    Serial.println("[TPA] WARNING: Canister stays OFF — no sensor to verify "
                   "water level.");
    _lastErrorMsg += " | Canister: OFF (sem sensor)";
  }

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

