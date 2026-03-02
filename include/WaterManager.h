#pragma once

#include "Config.h"
#include <Arduino.h>

// Forward declarations
class SafetyWatchdog;
class FertManager;

/// @brief TPA state machine states
enum class TPAState : uint8_t {
  IDLE = 0,
  CANISTER_OFF,
  DRAINING,
  FILLING_RESERVOIR,
  DOSING_PRIME,
  REFILLING,
  CANISTER_ON,
  COMPLETE,
  ERROR
};

/// @brief Returns human-readable name for a TPA state
const char *tpaStateName(TPAState s);

/// @brief Manages the TPA (Troca Parcial de Água) state machine.
class WaterManager {
public:
  WaterManager();

  /// Initialize (references to safety watchdog and fert manager for Prime)
  void begin(SafetyWatchdog *safety, FertManager *fert);

  /// Run state machine tick — call every loop iteration
  void update();

  /// Start TPA cycle
  void startTPA();

  /// Abort TPA cycle immediately (emergency or user cancel)
  void abortTPA();

  /// Current state
  TPAState getState() const { return _state; }
  const char *getStateName() const { return tpaStateName(_state); }

  /// Is a TPA cycle currently running?
  bool isRunning() const {
    return _state != TPAState::IDLE && _state != TPAState::COMPLETE &&
           _state != TPAState::ERROR;
  }

  // ---- TPA parameters (set via WebManager) ----
  void setDrainTargetCm(float cm) { _drainTargetCm = cm; }
  void setRefillTargetCm(float cm) { _refillTargetCm = cm; }
  void setCanisterSafeLevelCm(float cm) { _canisterSafeLevelCm = cm; }
  void setPrimeML(float ml) { _primeML = ml; }
  float getDrainTargetCm() const { return _drainTargetCm; }
  float getRefillTargetCm() const { return _refillTargetCm; }
  float getCanisterSafeLevelCm() const { return _canisterSafeLevelCm; }
  float getPrimeML() const { return _primeML; }

  // ---- Dynamic timeouts ----
  void setTimeoutDrainMs(unsigned long ms) { _timeoutDrainMs = ms; }
  void setTimeoutRefillMs(unsigned long ms) { _timeoutRefillMs = ms; }

  // ---- Calibration / Flow rates ----
  void setLitersPerCm(float lpc) { _litersPerCm = lpc; }
  void setDrainFlowLPM(float lpm) { _drainFlowLPM = lpm; }
  void setRefillFlowLPM(float lpm) { _refillFlowLPM = lpm; }
  float getDrainFlowLPM() const { return _drainFlowLPM; }
  float getRefillFlowLPM() const { return _refillFlowLPM; }
  bool isCalibrated() const { return _drainFlowLPM > 0 && _refillFlowLPM > 0; }

  /// Canister filter state
  bool isCanisterOn() const {
    return digitalRead(PIN_CANISTER) == LOW;
  } // SSR: LOW = ON

  /// Get last TPA completion timestamp (for telemetry)
  String getLastTPATime() const { return _lastTPATime; }
  void setLastTPATime(const String &t) { _lastTPATime = t; }

private:
  TPAState _state;
  SafetyWatchdog *_safety;
  FertManager *_fert;

  // State timing
  unsigned long _stateStartMs;
  unsigned long _waitUntilMs; // Non-blocking delay target (0 = not waiting)

  // Dosing state
  bool
      _doseCompleted; // Tracks if Prime dosing already happened in DOSING_PRIME

  // Parameters
  float _drainTargetCm;
  float _refillTargetCm;
  float
      _canisterSafeLevelCm; // Min water level (cm) for safe canister operation
  float _primeML;

  // Dynamic timeouts (initialized from Config.h, updated after calibration)
  unsigned long _timeoutDrainMs;
  unsigned long _timeoutRefillMs;

  // Inline calibration (measured during TPA)
  float _litersPerCm;        // Aquarium litersPerCm (set by main before TPA)
  float _calStartLevel;      // Ultrasonic level at state entry (cm)
  unsigned long _calStartMs; // millis() at state entry
  float _drainFlowLPM;       // Calibrated drain flow rate (L/min)
  float _refillFlowLPM;      // Calibrated refill flow rate (L/min)

  // Telemetry
  String _lastTPATime;

  // ---- State handlers ----
  void _enterState(TPAState newState);
  void _handleCanisterOff();
  void _handleDraining();
  void _handleFillingReservoir();
  void _handleDosingPrime();
  void _handleRefilling();
  void _handleCanisterOn();

  /// Capture refill flow rate from inline calibration data
  void _captureRefillCalibration();

  /// Elapsed time in current state (ms)
  unsigned long _stateElapsed() const { return millis() - _stateStartMs; }

  /// Is waiting for a non-blocking delay to expire?
  bool _isWaiting() const {
    return _waitUntilMs > 0 && millis() < _waitUntilMs;
  }

  /// Abort with error message
  void _error(const char *msg);
};
