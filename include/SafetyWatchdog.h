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

  /// Blind zone: readings closer than this are ring-down, not distance.
  /// Clamped to the sensor-plausible bounds; a value outside them is a typo,
  /// and either extreme silently disables level sensing.
  void setMinDistanceCm(float cm) {
    if (cm < ULTRASONIC_MIN_DISTANCE_FLOOR_CM)
      cm = ULTRASONIC_MIN_DISTANCE_FLOOR_CM;
    if (cm > ULTRASONIC_MIN_DISTANCE_CEIL_CM)
      cm = ULTRASONIC_MIN_DISTANCE_CEIL_CM;
    _minDistanceCm = cm;
  }
  float getMinDistanceCm() const { return _minDistanceCm; }

  /// Readings refused as physically impossible since boot. A climbing count on
  /// a still tank means the sensor is seeing something other than the water.
  uint32_t getRejectedReadings() const { return _rejectedTotal; }

  // ---- Ultrasonic wire diagnostics ----
  // areSensorsConnected() collapses every way the sensor can fail into one
  // false, which is what the dashboard needs and what debugging cannot use:
  // a cut data line, a sensor answering 0 mm and a bad checksum all look the
  // same. These counters split them apart without needing a serial cable.

  /// Bytes read off Serial2 since boot. Zero means nothing reaches the pin.
  uint32_t getUsBytes() const { return _bytesSeen; }
  /// Bytes discarded while hunting for the 0xFF frame header.
  uint32_t getUsGarbageBytes() const { return _garbageBytes; }
  /// Frames that started with 0xFF and passed the checksum.
  uint32_t getUsFrames() const { return _framesSeen; }
  /// Frames that started with 0xFF but failed the checksum — noise or a
  /// baud-rate mismatch rather than a silent wire.
  uint32_t getUsChecksumFails() const { return _checksumFails; }
  /// Frames decoded cleanly but thrown out by the range filter. A sensor
  /// reporting 0 mm because it cannot find the surface lands here.
  uint32_t getUsRangeRejects() const { return _rangeRejects; }
  /// Last distance decoded from a valid frame, in cm, before any filtering.
  /// -1 until one arrives.
  float getUsLastRaw() const { return _lastRawCm; }
  /// Bytes sitting unread in the UART buffer. A number stuck between 1 and 3
  /// means a partial frame the 4-byte read loop can never consume.
  int getUsPending() const { return Serial2.available(); }

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
  /// Configured blind zone, cm. Overwritten from NVS at boot.
  float _minDistanceCm = ULTRASONIC_MIN_DISTANCE_DEFAULT_CM;

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
  // The A02YYUW emits a frame roughly every 100 ms, so 20 samples is about a
  // 2 s window. Median rather than average: it discards the extremes outright,
  // which is what surface ripple produces. A wider window costs almost nothing
  // in tracking lag — pumping moves the level ~0.03 cm/s — while roughly
  // halving the noise compared with 5 samples.
  static constexpr uint8_t MEDIAN_BUFFER_SIZE = 20;
  // Entries beyond _medianCount are never read, so zero-init is fine.
  float _medianBuffer[MEDIAN_BUFFER_SIZE] = {};
  uint8_t _slewRejects = 0;   ///< consecutive readings refused as implausible
  uint32_t _rejectedTotal = 0; ///< lifetime count, for diagnosing a noisy sensor
  /// Frames the sensor sent that fell outside the valid range — a 0 mm frame
  /// from a sensor that cannot see the surface counts here, and telling that
  /// apart from a dead UART is the whole point of keeping it.
  uint32_t _rangeRejects = 0;
  unsigned long _lastRangeLogMs = 0; ///< rate limiter for the log line above
  uint32_t _bytesSeen = 0;     ///< every byte pulled off Serial2
  uint32_t _garbageBytes = 0;  ///< bytes dropped while resyncing to a header
  uint32_t _framesSeen = 0;    ///< header + checksum both good
  uint32_t _checksumFails = 0; ///< header good, checksum bad
  float _lastRawCm = -1;       ///< last decoded distance, pre-filter
  static constexpr unsigned long RANGE_LOG_INTERVAL_MS = 2000;
  float _rejectMin = 0;       ///< spread of the refused run, to tell a real
  float _rejectMax = 0;       ///< move from a burst of bad echoes
  uint8_t _medianIndex = 0;
  uint8_t _medianCount = 0;

#ifndef UNIT_TEST
  /// Serialises readUltrasonic(). Several web handlers ask for a level, and
  /// those run on the AsyncTCP task, not the loop — see readUltrasonic().
  SemaphoreHandle_t _sensorMutex = nullptr;
#endif

  /// Check if water level is dangerously high
  void _checkOverflow();

  /// Update emergency drain timeout
  void _updateEmergencyDrain();
};
