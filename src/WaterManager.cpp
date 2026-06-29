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
  case TPAState::CALIBRATING_RESERVOIR:
    return "CALIBRATING_RESERVOIR";
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
  _manualPumpTarget = pump;
  _manualPumpGoalLiters = goalLiters;

  Serial.printf("[TPA] ====== MANUAL PUMP STARTED: %s (Goal: %.1f L) ======\n", pump.c_str(), goalLiters);

  if (pump == "drain") {
    _enterState(TPAState::MANUAL_PUMP_DRAIN);
  } else if (pump == "refill") {
    _enterState(TPAState::MANUAL_PUMP_REFILL);
  }
}

void WaterManager::stopManual() {
  Serial.println("[TPA] Manual operation stopped.");
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
    _handleManualPump(PIN_DRAIN, _drainFlowLPM, false);
    break;
  case TPAState::MANUAL_PUMP_REFILL:
    _handleManualPump(PIN_REFILL, _refillFlowLPM, true);
    break;
  case TPAState::CALIBRATING_RESERVOIR:
    _handleCalibratingReservoir();
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

  // Read ultrasonic
  if (_safety) {
    float dist = _safety->readUltrasonic();
    if (dist >= _drainTargetCm) {
      // Target reached (higher distance = lower water)
      Serial.printf("[TPA] Drain target reached: %.1f cm\n", dist);
      pumpOff(PIN_DRAIN, PumpReason::TPA_TARGET_REACHED);

      // Inline calibration: calculate drain flow rate
      float flowRate = _calcFlowRate(_calStartLevel, dist, _calStartMs);
      if (flowRate > 0) {
        _drainFlowLPM = flowRate;
        Serial.printf("[TPA] Drain calibrated: %.2f L/min\n", _drainFlowLPM);
      }

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

  // Safety timeout: use calibrated time × 1.5, or 2h hard limit
  unsigned long timeout = (_solenoidFillTimeSec > 0)
    ? (unsigned long)(_solenoidFillTimeSec * 1500) // 1.5× calibrated time
    : 2UL * 60 * 60 * 1000; // 2h fallback

  if (_stateElapsed() >= timeout) {
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
  // Step 5: Refill tank until optical sensor or ultrasonic setpoint
  if (digitalRead(PIN_REFILL) == LOW) {
    pumpOn(PIN_REFILL, PumpReason::TPA_REFILLING);
    Serial.printf("[TPA] Refill pump ON. Target: %.1f cm\n", _refillTargetCm);
    // Record calibration start point
    if (_safety && _litersPerCm > 0) {
      _calStartLevel = _safety->readUltrasonic();
      _calStartMs = millis();
    }
  }

  // CRITICAL SAFETY: Optical sensor = immediate stop
  if (_safety && _safety->isOpticalHigh()) {
    Serial.println("[TPA] Optical sensor HIGH — refill STOPPED (max level).");
    pumpOff(PIN_REFILL, PumpReason::SAFETY_STOP);
    _captureRefillCalibration();
    _enterState(TPAState::CANISTER_ON);
    return;
  }

  // Ultrasonic setpoint check
  if (_safety) {
    float dist = _safety->readUltrasonic();
    if (dist > 0 && dist <= _refillTargetCm) {
      Serial.printf("[TPA] Refill setpoint reached: %.1f cm\n", dist);
      pumpOff(PIN_REFILL, PumpReason::TPA_TARGET_REACHED);
      _captureRefillCalibration();
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

  // Safety timeout: calibrated × 1.5, or 30 min fallback
  unsigned long timeout = (_solenoidFillTimeSec > 0)
    ? (unsigned long)(_solenoidFillTimeSec * 1500)
    : 30UL * 60 * 1000;

  if (_stateElapsed() >= timeout) {
    pumpOff(PIN_SOLENOID, PumpReason::ERROR_STOP);
    _error("Manual reservoir fill timeout exceeded!");
    return;
  }
}

// DRY #3: Unified manual pump handler for both drain and refill
void WaterManager::_handleManualPump(uint8_t pin, float flowLPM, bool checkOptical) {
  if (digitalRead(pin) == LOW) {
    pumpOn(pin, PumpReason::MANUAL_PUMP);
  }

  // CRITICAL SAFETY: Optical sensor = immediate stop (refill only)
  if (checkOptical && _safety && _safety->isOpticalHigh()) {
    Serial.printf("[TPA] Optical sensor HIGH — manual %s STOPPED (max level).\n", pinName(pin));
    pumpOff(pin, PumpReason::SAFETY_STOP);
    _state = TPAState::COMPLETE;
    return;
  }

  if (_manualPumpGoalLiters > 0 && flowLPM > 0) {
    float pumpedLiters = (_stateElapsed() / 60000.0f) * flowLPM;
    if (pumpedLiters >= _manualPumpGoalLiters) {
      Serial.printf("[TPA] Manual %s goal reached: %.1f L\n", pinName(pin), pumpedLiters);
      pumpOff(pin, PumpReason::TPA_TARGET_REACHED);
      _state = TPAState::COMPLETE;
      return;
    }
  }

  // Safety: maximum timeout (30 mins)
  if (_stateElapsed() >= 30UL * 60 * 1000) {
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
  _solenoidFillTimeSec = calPref.getFloat("solFillS", 0);
  calPref.end();
  if (drainLPM > 0) {
    _drainFlowLPM = drainLPM;
    Serial.printf("[TPA] Loaded drain calibration: %.2f L/min\n", drainLPM);
  }
  if (refillLPM > 0) {
    _refillFlowLPM = refillLPM;
    Serial.printf("[TPA] Loaded refill calibration: %.2f L/min\n", refillLPM);
  }
  if (_solenoidFillTimeSec > 0) {
    Serial.printf("[TPA] Loaded solenoid fill time: %.0f sec\n", _solenoidFillTimeSec);
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

void WaterManager::startReservoirCalibration() {
  if (isRunning()) {
    Serial.println("[TPA] Already running, ignoring startReservoirCalibration().");
    return;
  }
  _floatFullCount = 0;
  Serial.println("[TPA] ====== RESERVOIR CALIBRATION STARTED ======");
  _enterState(TPAState::CALIBRATING_RESERVOIR);
}

void WaterManager::_handleCalibratingReservoir() {
  // Open solenoid
  if (digitalRead(PIN_SOLENOID) == LOW) {
    pumpOn(PIN_SOLENOID, PumpReason::TPA_SOLENOID);
    Serial.println("[TPA] Calibration: Solenoid OPEN. Timing fill...");
  }

  // Wait for float sensor (debounced)
  if (_isReservoirFullDebounced()) {
    float fillTimeSec = _stateElapsed() / 1000.0f;
    _solenoidFillTimeSec = fillTimeSec;
    pumpOff(PIN_SOLENOID, PumpReason::TPA_TARGET_REACHED);

    // Save to NVS
    Preferences calPref;
    calPref.begin("pumpcal", false);
    calPref.putFloat("solFillS", _solenoidFillTimeSec);
    calPref.end();

    Serial.printf("[TPA] ====== RESERVOIR CALIBRATION COMPLETE ======\n");
    Serial.printf("[TPA] Fill time: %.1f seconds\n", fillTimeSec);
    _state = TPAState::COMPLETE;
    return;
  }

  // Hard timeout: 1 hour (safety)
  if (_stateElapsed() >= 3600000UL) {
    pumpOff(PIN_SOLENOID, PumpReason::ERROR_STOP);
    _error("Reservoir calibration timeout (1h)!");
  }
}

float WaterManager::getReservoirVolume() const {
  // Returns estimated reservoir volume based on calibrated fill time
  // Assumes typical residential solenoid flow of ~5 L/min
  // User can override via config
  return 0; // TODO: calculate from fill time + known flow rate
}
