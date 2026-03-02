#include "WaterManager.h"
#include "FertManager.h"
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
  default:
    return "UNKNOWN";
  }
}

WaterManager::WaterManager()
    : _state(TPAState::IDLE), _safety(nullptr), _fert(nullptr),
      _stateStartMs(0), _waitUntilMs(0), _doseCompleted(false),
      _drainTargetCm(LEVEL_DRAIN_TARGET_CM),
      _refillTargetCm(LEVEL_REFILL_TARGET_CM), _primeML(DEFAULT_PRIME_ML) {}

void WaterManager::begin(SafetyWatchdog *safety, FertManager *fert) {
  _safety = safety;
  _fert = fert;
  Serial.println("[TPA] WaterManager initialized.");
}

void WaterManager::startTPA() {
  if (isRunning()) {
    Serial.println("[TPA] Already running, ignoring startTPA().");
    return;
  }
  if (_safety && !_safety->areSensorsConnected()) {
    Serial.println("[TPA] Cannot start: ultrasonic sensor not connected.");
    return;
  }
  if (_safety && _safety->isEmergency()) {
    Serial.println("[TPA] Cannot start: system in emergency state.");
    return;
  }

  Serial.println("[TPA] ====== TPA CYCLE STARTED ======");
  _enterState(TPAState::CANISTER_OFF);
}

void WaterManager::abortTPA() {
  Serial.println("[TPA] !!! TPA ABORTED !!!");
  // Turn off all TPA-related actuators
  digitalWrite(PIN_DRAIN, LOW);
  digitalWrite(PIN_REFILL, LOW);
  digitalWrite(PIN_SOLENOID, LOW);
  digitalWrite(PIN_PRIME, LOW);
  // Canister back on for safety (SSR: LOW = ON)
  digitalWrite(PIN_CANISTER, LOW);
  _state = TPAState::ERROR;
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
  Serial.printf("[TPA] -> State: %s\n", tpaStateName(newState));
}

// ============================================================================
// STATE HANDLERS
// ============================================================================

void WaterManager::_handleCanisterOff() {
  // Step 1: Turn off canister filter (SSR: HIGH = OFF)
  if (_waitUntilMs == 0) {
    // First call: turn off canister and start the non-blocking wait
    digitalWrite(PIN_CANISTER, HIGH);
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
    // Start drain pump on first tick
    digitalWrite(PIN_DRAIN, HIGH);
    Serial.printf("[TPA] Drain pump ON. Target: %.1f cm\n", _drainTargetCm);
  }

  // Read ultrasonic
  if (_safety) {
    float dist = _safety->readUltrasonic();
    if (dist >= _drainTargetCm) {
      // Target reached (higher distance = lower water)
      Serial.printf("[TPA] Drain target reached: %.1f cm\n", dist);
      digitalWrite(PIN_DRAIN, LOW);
      _enterState(TPAState::FILLING_RESERVOIR);
      return;
    }
  }

  // Timeout check
  if (_stateElapsed() >= TIMEOUT_DRAIN_MS) {
    _error("Drain timeout exceeded!");
    return;
  }
}

void WaterManager::_handleFillingReservoir() {
  // Step 3: Open solenoid until float switch indicates reservoir full
  if (digitalRead(PIN_SOLENOID) == LOW) {
    digitalWrite(PIN_SOLENOID, HIGH);
    Serial.println("[TPA] Solenoid OPEN. Filling reservoir...");
  }

  if (_safety && _safety->isReservoirFull()) {
    Serial.println("[TPA] Reservoir FULL (float switch triggered).");
    digitalWrite(PIN_SOLENOID, LOW);
    _enterState(TPAState::DOSING_PRIME);
    return;
  }

  // Timeout check
  if (_stateElapsed() >= TIMEOUT_FILL_MS) {
    digitalWrite(PIN_SOLENOID, LOW);
    _error("Reservoir fill timeout exceeded!");
    return;
  }
}

void WaterManager::_handleDosingPrime() {
  // Step 4: Dose Prime (dechlorinator) into reservoir
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
    digitalWrite(PIN_REFILL, HIGH);
    Serial.printf("[TPA] Refill pump ON. Target: %.1f cm\n", _refillTargetCm);
  }

  // CRITICAL SAFETY: Optical sensor = immediate stop
  if (_safety && _safety->isOpticalHigh()) {
    Serial.println("[TPA] Optical sensor HIGH — refill STOPPED (max level).");
    digitalWrite(PIN_REFILL, LOW);
    _enterState(TPAState::CANISTER_ON);
    return;
  }

  // Ultrasonic setpoint check
  if (_safety) {
    float dist = _safety->readUltrasonic();
    if (dist > 0 && dist <= _refillTargetCm) {
      Serial.printf("[TPA] Refill setpoint reached: %.1f cm\n", dist);
      digitalWrite(PIN_REFILL, LOW);
      _enterState(TPAState::CANISTER_ON);
      return;
    }
  }

  // Timeout check
  if (_stateElapsed() >= TIMEOUT_REFILL_MS) {
    digitalWrite(PIN_REFILL, LOW);
    _error("Refill timeout exceeded!");
    return;
  }
}

void WaterManager::_handleCanisterOn() {
  // Step 6: Turn canister filter back on (SSR: LOW = ON)
  digitalWrite(PIN_CANISTER, LOW);
  Serial.println("[TPA] Canister ON. TPA cycle COMPLETE.");

  _state = TPAState::COMPLETE;
}

void WaterManager::_error(const char *msg) {
  Serial.printf("[TPA] ERROR: %s\n", msg);
  // Safety: turn off all TPA actuators
  digitalWrite(PIN_DRAIN, LOW);
  digitalWrite(PIN_REFILL, LOW);
  digitalWrite(PIN_SOLENOID, LOW);
  // Turn canister back on (SSR: LOW = ON)
  digitalWrite(PIN_CANISTER, LOW);
  _state = TPAState::ERROR;
}
