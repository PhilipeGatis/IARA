#pragma once

#include "Config.h"
#include <Arduino.h>

/// @brief Safety-first watchdog: sensor reads, overflow detection, emergency
/// actions.
class SafetyWatchdog {
public:
  SafetyWatchdog();

  /// Initialize sensor pins
  void begin();

  /// Run all safety checks — call every loop iteration
  void update();

  // ---- Sensor reads ----

  /// Ultrasonic distance (cm). Uses median filter. Returns -1 on error.
  float readUltrasonic();

  /// Optical max-level sensor: true = water at max level (STOP pumps!)
  bool isOpticalHigh();

  /// Reservoir float switch: true = reservoir is full
  bool isReservoirFull();

  /// Last valid ultrasonic reading (cm)
  float getLastDistance() const { return _lastDistance; }

  /// True if ultrasonic sensor is producing valid readings
  bool areSensorsConnected() const { return _sensorsConnected; }

  // ---- Emergency actions ----

  /// Immediately set ALL output pins LOW
  void emergencyShutdown();

  /// Open drain, close everything else. Runs for TIMEOUT_EMERGENCY_MS.
  void emergencyDrain();

  /// Returns true if currently in emergency state
  bool isEmergency() const { return _emergency; }

  /// Clear the emergency state
  void clearEmergency();

  // ---- Maintenance mode ----

  void enterMaintenance();
  void exitMaintenance();
  bool isMaintenanceMode() const { return _maintenance; }

  // ---- Flags for other managers ----

  /// True if optical sensor triggered overflow during last update
  bool overflowDetected() const { return _overflowFlag; }

private:
  float _lastDistance;
  bool _emergency;
  bool _sensorsConnected;
  uint8_t _ultrasonicFailCount;
  uint8_t _overflowConsecutiveCount;
  bool _overflowFlag;

  // Maintenance
  bool _maintenance;
  unsigned long _maintenanceStart;

  // Timing
  unsigned long _lastCheckMs;

  // Emergency drain tracking
  bool _emergencyDraining;
  unsigned long _emergencyDrainStart;

  /// Median filter helper
  float _medianOfFive(float *arr);

  /// Check if water level is dangerously high
  void _checkOverflow();

  /// Update emergency drain timeout
  void _updateEmergencyDrain();
};
