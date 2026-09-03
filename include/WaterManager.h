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
  MANUAL_PUMP_REFILL
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
  void startTPA(bool manual = false);

  /// Abort TPA cycle immediately (emergency or user cancel)
  void abortTPA();

  /// Manual Operations
  void startManualReservoirFill();
  void startManualPump(const String &pump, float goalLiters);

  /// Run a pump for PUMP_CALIBRATION_RUN_MS and derive its flow rate from the
  /// level change. This is the bootstrap: a goal cannot be honoured before a
  /// flow rate exists, and a flow rate cannot be measured without running.
  void startPumpCalibration(const String &pump);

  /// Calibrate both pumps back to back: drain until the level has moved enough
  /// to measure, then refill the same amount. Neither can be calibrated alone
  /// on a full tank — the drain needs water to remove and the refill needs
  /// headroom to add — so running them as a pair leaves the level where it
  /// started and measures both.
  void startPairedCalibration();

  void stopManual();

  /// Pump Progress Tracking (used for UI/Display during Auto and Manual TPA)
  float getPumpGoalLiters() const;
  float getPumpProgressLiters() const;
  unsigned long getPumpElapsedMs() const;

  /// Current state
  TPAState getState() const { return _state; }
  const char *getStateName() const { return tpaStateName(_state); }

  /// Is the current TPA manually triggered?
  bool isManualTPA() const { return _isManualTPA; }

  /// True when the run that just ended was a full TPA cycle rather than a
  /// manual pump run or a calibration. Both land in COMPLETE, but only the
  /// first one should consume the schedule interval.
  bool wasFullCycle() const { return _wasFullCycle; }

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
  /// Window the solenoid may stay open waiting for the reservoir to read full.
  ///
  /// Zero disables it: the valve stays open until the float reports full, with
  /// nothing in the firmware stopping it. That is only defensible when a
  /// mechanical float valve bounds the volume — the timeout is otherwise the
  /// sole limit on a mains feed. It also means a float that never closes leaves
  /// the cycle in FILLING_RESERVOIR indefinitely rather than moving on.
  void setTimeoutReservoirFillMs(unsigned long ms) {
    _timeoutReservoirFillMs = ms;
  }
  unsigned long getTimeoutReservoirFillMs() const {
    return _timeoutReservoirFillMs;
  }

  void setTimeoutDrainMs(unsigned long ms) { _timeoutDrainMs = ms; }
  void setTimeoutRefillMs(unsigned long ms) { _timeoutRefillMs = ms; }

  // ---- Calibration / Flow rates ----
  void setLitersPerCm(float lpc) { _litersPerCm = lpc; }
  void setAqEffectiveHeightCm(float h) { _aqEffectiveHeightCm = h; }

  /// The calibrated 100% mark, as sensor distance. Percentages are meaningless
  /// without it — see TpaPlan.h.
  void setSensorFullCm(float cm) { _sensorFullCm = cm; }
  void setPrimeEnabled(bool enabled) { _primeEnabled = enabled; }

  /// Declares that a mechanical float valve closes the reservoir's inlet at the
  /// full level, independently of any electronics.
  ///
  /// With it, running out the fill timeout stops being a failure: the water has
  /// been held back by the valve, so the reservoir is full whether or not the
  /// electrical float said so. Without it, the timeout is the only thing
  /// standing between a stuck valve and an unbounded mains feed, and it errors.
  ///
  /// The cost is real and worth stating: the electrical float is the only way
  /// to tell a full reservoir from an empty one, so in this mode a fill that
  /// failed for want of mains pressure looks exactly like one that succeeded,
  /// and the cycle will drain the aquarium before discovering it.
  void setReservoirHasMechanicalFloat(bool has) { _reservoirMechFloat = has; }
  /// Store a measured flow rate. Rejected, and left at zero, when the pump has
  /// a declared nominal rate and the measurement exceeds it: see
  /// NOMINAL_FLOW_TOLERANCE.
  void setDrainFlowLPM(float lpm);
  void setRefillFlowLPM(float lpm);
  float getDrainFlowLPM() const { return _drainFlowLPM; }
  float getRefillFlowLPM() const { return _refillFlowLPM; }
  bool isCalibrated() const { return _drainFlowLPM > 0 && _refillFlowLPM > 0; }

  /// The pump's datasheet rating, in L/min. 0 = not declared.
  ///
  /// Declaring it re-tests whatever measurement is already stored, so a rate
  /// calibrated against a broken sensor is discarded the moment the real
  /// capability of the pump is written down, rather than waiting for the next
  /// calibration run that nobody remembers to do.
  void setDrainNominalLPM(float lpm);
  void setRefillNominalLPM(float lpm);
  float getDrainNominalLPM() const { return _drainNominalLPM; }
  float getRefillNominalLPM() const { return _refillNominalLPM; }

  /// The rate to size a timeout or an expectation from: the SLOWER of the
  /// measured and the nominal one, falling back to whichever exists alone.
  ///
  /// Slower, because every consumer of these is asking "how long may this
  /// legitimately take" — and being wrong in the fast direction is what aborts
  /// an honest water change half-finished.
  float planningDrainLPM() const;
  float planningRefillLPM() const;

  /// Save calibrated flow rates to NVS
  void saveCalibration();
  /// Load calibrated flow rates from NVS
  void loadCalibration();

  /// Turn the canister back on, but only if the water is deep enough for its
  /// intake. Returns false and leaves it off otherwise. Every path that
  /// restores the filter goes through here, so the safe-level rule cannot be
  /// bypassed by whichever caller forgets it.
  bool restoreCanisterIfSafe(PumpReason reason);

  /// Canister filter state
  bool isCanisterOn() const {
    return digitalRead(PIN_CANISTER) == LOW;
  } // SSR: LOW = ON

  // ---- Feeding pause ----
  /// Switch the canister off so food is not pulled into the intake, and
  /// schedule it back on. Refused during a water change or an emergency: both
  /// already decide what the filter does. A pause never outlives its timer —
  /// the filter coming back on is the point, not a courtesy.
  void startFeedingPause(uint16_t minutes);

  /// Restore the canister now and drop the pause.
  void endFeedingPause();

  bool isFeedingPause() const { return _feedingPause; }

  /// Seconds left in the pause, 0 when none is running.
  uint32_t feedingSecondsLeft() const;

  /// Get last TPA completion timestamp (for telemetry)
  String getLastTPATime() const { return _lastTPATime; }
  void setLastTPATime(const String &t) { _lastTPATime = t; }

  /// Get last TPA error message (for notifications)
  String getLastErrorMsg() const { return _lastErrorMsg; }

private:
  TPAState _state;
  SafetyWatchdog *_safety;
  FertManager *_fert;
  bool _isManualTPA;
  bool _wasFullCycle = false;
  bool _primeDoseStarted = false;
  // Level the drain leg of a paired calibration started from; the refill leg
  // stops there so the pair is level-neutral by construction.
  float _pairedRefillTargetCm = -1;
  unsigned long _primeWaitStartedMs = 0;

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
  unsigned long _timeoutReservoirFillMs = TIMEOUT_RESERVOIR_FILL_MS;
  unsigned long _timeoutDrainMs;
  unsigned long _timeoutRefillMs;

  // Inline calibration (measured during TPA)
  float _litersPerCm;         // Aquarium litersPerCm (set by main before TPA)
  float _aqEffectiveHeightCm; // Effective water height (for cm→% conversion)
  float _calStartLevel;       // Ultrasonic level at state entry (cm)
  unsigned long _calStartMs;  // millis() at state entry
  float _drainFlowLPM;        // Calibrated drain flow rate (L/min)
  float _refillFlowLPM;       // Calibrated refill flow rate (L/min)
  float _drainNominalLPM = 0; // Datasheet rating (L/min), 0 = not declared
  float _refillNominalLPM = 0;

  /// Returns `measured` when the pump could plausibly have produced it, 0 when
  /// it could not. Logs the rejection: a silently discarded sample is how the
  /// bad rate survived every manual run that should have replaced it.
  float _plausibleFlow(float measured, float nominal, const char *what) const;

  // Refill flow verification. The expected rate is snapshotted when the pump
  // starts, BEFORE the live recalibration below can touch _refillFlowLPM --
  // otherwise a stalled refill would drag the expectation down with it and the
  // check would agree that nothing is wrong.
  float _refillProgressCmMin;      // Expected level movement (cm/min), 0 = off
  float _refillProgressLevel;      // Level at the start of the window (cm)
  unsigned long _refillProgressMs; // millis() at the start of the window

  // Telemetry
  String _lastTPATime;
  String _lastErrorMsg;

  // ---- State handlers ----
  void _enterState(TPAState newState);
  /// Ends a feeding pause when its time is up, or when something with a better
  /// claim on the canister takes over. Runs every loop, including while idle.
  void _tickFeedingPause();

  void _handleCanisterOff();
  void _handleDraining();
  void _handleFillingReservoir();
  void _handleDosingPrime();
  void _handleRefilling();
  void _handleCanisterOn();
  void _handleManualReservoirFill();
  void _handleManualPump(uint8_t pin, float flowLPM);

  /// Switch the canister off for a manual run and start the settle wait.
  void _stopCanisterForManualRun();

  /// Enter a calibration run. Shared by the single and paired entry points;
  /// unlike them it does not re-check isRunning(), so the paired sequence can
  /// chain its second leg directly out of the first.
  void _beginPumpCalibration(const String &pump);

  /// Clear every per-run flag. Called wherever a run begins or ends.
  void _resetCycleState();

  /// Stop all TPA actuators (DRY helper)
  void _stopAllTpaActuators(PumpReason reason);

  /// Capture inline calibration flow rate
  void _captureDrainCalibration();
  void _captureRefillCalibration();

  /// Explain on the serial log why a capture produced nothing.
  void _reportRejectedSample(const char *what, float startLevel,
                             float endLevel) const;

  /// Progress window sized so its threshold clears the sensor's noise.
  unsigned long _refillProgressWindowMs() const;

  /// Calculate flow rate from calibration data (DRY helper)
  float _calcFlowRate(float startLevel, float endLevel, unsigned long startMs) const;

  /// Elapsed time in current state (ms)
  unsigned long _stateElapsed() const { return millis() - _stateStartMs; }

  /// Is waiting for a non-blocking delay to expire?
  bool _isWaiting() const {
    // Signed difference, not `millis() < _waitUntilMs`. Around the ~49-day
    // rollover the raw comparison flips, and a wait that should have ended
    // instead lasts another 49 days — with the canister off and the state
    // machine parked mid-cycle.
    return _waitUntilMs > 0 && (long)(millis() - _waitUntilMs) < 0;
  }

  /// Abort with error message
  void _error(const char *msg);

  // Manual pump state
  String _manualPumpTarget;
  float _manualPumpGoalLiters;
  /// True while a manual run has the canister switched off on its behalf.
  bool _canisterOffForManual = false;
  /// True while a feeding pause has the canister switched off on its behalf.
  bool _feedingPause = false;
  unsigned long _feedingStartMs = 0;
  unsigned long _feedingDurationMs = 0;
  /// True while the refill is paused, waiting for the surface to settle before
  /// accepting that the setpoint was really reached.
  bool _refillConfirming = false;
  /// True while the drain leg of a paired calibration is running, so its
  /// completion chains straight into the refill leg.
  bool _pairedCalibration = false;
  /// When non-zero, the manual run stops cleanly after this long to calibrate.
  unsigned long _calibrationRunMs = 0;
  /// Ultrasonic level (cm) that satisfies the manual goal. -1 = track by flow only.
  float _manualTargetLevelCm = -1;
  /// Timeout for the current manual run, sized from the goal and flow rate.
  unsigned long _manualTimeoutMs = MANUAL_PUMP_MAX_MS;
  bool _primeEnabled = true;
  bool _reservoirMechFloat = false;
  float _sensorFullCm = 0;

  // Float sensor debounce (requires N consecutive "full" reads to confirm)
  static constexpr uint8_t FLOAT_DEBOUNCE_COUNT = 5; // 5 × 50ms loop = 250ms
  uint8_t _floatFullCount = 0;
  bool _isReservoirFullDebounced();

};
