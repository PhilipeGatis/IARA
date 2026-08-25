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

  /// Set the overflow threshold (distance from sensor, cm).
  /// 0 disables overflow detection — used while the sensor is uncalibrated.
  void setOverflowThresholdCm(float cm) { _overflowThresholdCm = cm > 0 ? cm : 0.0f; }

  /// Returns true if currently in emergency state
  bool isEmergency() const { return _emergency; }

  /// Clear the emergency state
  void clearEmergency();

  // ---- Maintenance mode ----

  void enterMaintenance();
  void exitMaintenance();
  bool isMaintenanceMode() const { return _maintenance; }

private:
  float _lastDistance;
  float _overflowThresholdCm;

  bool _emergency;
  bool _sensorsConnected;
  uint8_t _ultrasonicFailCount;
  uint8_t _overflowConsecutiveCount;
  /// Previous reading, used to reject physically impossible jumps.
  float _prevOverflowDistance = -1;

  // Maintenance
  bool _maintenance;
  unsigned long _maintenanceStart;

  // Timing
  unsigned long _lastCheckMs;

  // Emergency drain tracking
  bool _emergencyDraining;
  unsigned long _emergencyDrainStart;

  // Median filter buffer (circular)
  static constexpr uint8_t MEDIAN_BUFFER_SIZE = 5;
  float _medianBuffer[MEDIAN_BUFFER_SIZE] = {-1, -1, -1, -1, -1};
  uint8_t _medianIndex = 0;
  uint8_t _medianCount = 0;

  /// Median filter helper
  float _medianOfFive(float *arr);

  /// Check if water level is dangerously high
  void _checkOverflow();

  /// Update emergency drain timeout
  void _updateEmergencyDrain();
};
