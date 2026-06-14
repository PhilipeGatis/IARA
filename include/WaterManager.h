#pragma once

#include "Config.h"
#include "PumpLog.h"
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
  ERROR,
  MANUAL_RESERVOIR_FILL,
  MANUAL_PUMP_DRAIN,
  MANUAL_PUMP_REFILL,
  CALIBRATING_RESERVOIR  // Calibrating solenoid fill rate
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

  /// Manual Operations
  void startManualReservoirFill();
  void startManualPump(const String &pump, float goalLiters);
  void stopManual();

  /// Pump Progress Tracking (used for UI/Display during Auto and Manual TPA)
  float getPumpGoalLiters() const;
  float getPumpProgressLiters() const;
  unsigned long getPumpElapsedMs() const;

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
  void setAqEffectiveHeightCm(float h) { _aqEffectiveHeightCm = h; }
  void setPrimeEnabled(bool enabled) { _primeEnabled = enabled; }
  void setDrainFlowLPM(float lpm) { _drainFlowLPM = lpm; }
  void setRefillFlowLPM(float lpm) { _refillFlowLPM = lpm; }
  float getDrainFlowLPM() const { return _drainFlowLPM; }
  float getRefillFlowLPM() const { return _refillFlowLPM; }
  bool isCalibrated() const { return _drainFlowLPM > 0 && _refillFlowLPM > 0; }

  /// Save calibrated flow rates to NVS
  void saveCalibration();
  /// Load calibrated flow rates from NVS
  void loadCalibration();

  // ---- Reservoir solenoid calibration ----
  void startReservoirCalibration();
  float getSolenoidFillTimeSec() const { return _solenoidFillTimeSec; }
  float getReservoirVolume() const;
  bool isReservoirCalibrated() const { return _solenoidFillTimeSec > 0; }

  /// Canister filter state
  bool isCanisterOn() const {
    return digitalRead(PIN_CANISTER) == LOW;
  } // SSR: LOW = ON

  /// Get last TPA completion timestamp (for telemetry)
  String getLastTPATime() const { return _lastTPATime; }
  void setLastTPATime(const String &t) { _lastTPATime = t; }

  /// Get last TPA error message (for notifications)
  String getLastErrorMsg() const { return _lastErrorMsg; }

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
  float _litersPerCm;         // Aquarium litersPerCm (set by main before TPA)
  float _aqEffectiveHeightCm; // Effective water height (for cm→% conversion)
  float _calStartLevel;       // Ultrasonic level at state entry (cm)
  unsigned long _calStartMs;  // millis() at state entry
  float _drainFlowLPM;        // Calibrated drain flow rate (L/min)
  float _refillFlowLPM;       // Calibrated refill flow rate (L/min)

  // Telemetry
  String _lastTPATime;
  String _lastErrorMsg;

  // ---- State handlers ----
  void _enterState(TPAState newState);
  void _handleCanisterOff();
  void _handleDraining();
  void _handleFillingReservoir();
  void _handleDosingPrime();
  void _handleRefilling();
  void _handleCanisterOn();
  void _handleManualReservoirFill();
  void _handleManualPump(uint8_t pin, float flowLPM, bool checkOptical);

  /// Stop all TPA actuators (DRY helper)
  void _stopAllTpaActuators(PumpReason reason);

  /// Capture inline calibration flow rate
  void _captureRefillCalibration();

  /// Calculate flow rate from calibration data (DRY helper)
  float _calcFlowRate(float startLevel, float endLevel, unsigned long startMs) const;

  /// Elapsed time in current state (ms)
  unsigned long _stateElapsed() const { return millis() - _stateStartMs; }

  /// Is waiting for a non-blocking delay to expire?
  bool _isWaiting() const {
    return _waitUntilMs > 0 && millis() < _waitUntilMs;
  }

  /// Abort with error message
  void _error(const char *msg);

  // Manual pump state
  String _manualPumpTarget;
  float _manualPumpGoalLiters;
  bool _primeEnabled = true;

  // Float sensor debounce (requires N consecutive "full" reads to confirm)
  static constexpr uint8_t FLOAT_DEBOUNCE_COUNT = 5; // 5 × 50ms loop = 250ms
  uint8_t _floatFullCount = 0;
  bool _isReservoirFullDebounced();

  // Reservoir solenoid calibration
  float _solenoidFillTimeSec = 0; // Time to fill reservoir (0 = uncalibrated)
  void _handleCalibratingReservoir();
};
