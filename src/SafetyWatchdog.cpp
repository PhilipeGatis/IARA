#include "SafetyWatchdog.h"
#include "PumpLog.h"
#include <algorithm> // std::sort

SafetyWatchdog::SafetyWatchdog()
    : _lastDistance(-1), _emergency(false), _sensorsConnected(false),
      _ultrasonicFailCount(0), _overflowConsecutiveCount(0), _overflowFlag(false), _maintenance(false),
      _maintenanceStart(0), _lastCheckMs(0), _emergencyDraining(false),
      _emergencyDrainStart(0) {}

void SafetyWatchdog::begin() {
  // Ultrasonic
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  // Optical level sensor (active LOW, pulled up)
  pinMode(PIN_OPTICAL, INPUT_PULLUP);

  // Float switch (active HIGH, pulled down, connected to 3.3V)
  pinMode(PIN_FLOAT, INPUT_PULLDOWN);

  // Initial sensor probe — detect if ultrasonic is connected
  readUltrasonic();

  Serial.printf("[Safety] Watchdog initialized. Sensors: %s\n",
                _sensorsConnected ? "CONNECTED" : "NOT CONNECTED");
}

// ============================================================================
// SENSOR READS
// ============================================================================

float SafetyWatchdog::readUltrasonic() {
  float samples[ULTRASONIC_SAMPLES];
  uint8_t validCount = 0;

  for (uint8_t i = 0; i < ULTRASONIC_SAMPLES; i++) {
    // Send trigger pulse
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    // Measure echo pulse duration
    unsigned long duration =
        pulseIn(PIN_ECHO, HIGH, ULTRASONIC_PULSE_TIMEOUT_US);

    if (duration > 0) {
      float distance = (duration * 0.0343f) / 2.0f;
      if (distance > 0 && distance < ULTRASONIC_MAX_DISTANCE_CM) {
        samples[validCount++] = distance;
      }
    }
    delay(30); // JSN-SR04T needs ~30ms between measurements
    yield();   // Let FreeRTOS IDLE task run (prevents task WDT trigger)
  }

  if (validCount == 0) {
    _ultrasonicFailCount++;
    if (_ultrasonicFailCount >= 10 && _sensorsConnected) {
      _sensorsConnected = false;
      Serial.println(
          "[Safety] Ultrasonic sensor disconnected — safety checks disabled.");
    } else if (_ultrasonicFailCount < 10) {
      Serial.println("[Safety] Ultrasonic: no valid readings!");
    }
    return _lastDistance; // Return last known good value
  }

  // Sensor is working
  _ultrasonicFailCount = 0;
  if (!_sensorsConnected) {
    _sensorsConnected = true;
    Serial.println(
        "[Safety] Ultrasonic sensor connected — safety checks enabled.");
  }

  // If we have enough samples, use median; otherwise use average
  if (validCount >= 3) {
    // Pad remaining with valid values for median
    float sorted[ULTRASONIC_SAMPLES];
    for (uint8_t i = 0; i < validCount; i++)
      sorted[i] = samples[i];
    std::sort(sorted, sorted + validCount);
    _lastDistance = sorted[validCount / 2];
  } else {
    float sum = 0;
    for (uint8_t i = 0; i < validCount; i++)
      sum += samples[i];
    _lastDistance = sum / validCount;
  }

  return _lastDistance;
}

bool SafetyWatchdog::isOpticalHigh() {
  // Active LOW with pullup: LOW = water detected = HIGH level
  return digitalRead(PIN_OPTICAL) == LOW;
}

bool SafetyWatchdog::isReservoirFull() {
  // Active HIGH (3.3V): HIGH = Switch closed = Reservoir full
  return digitalRead(PIN_FLOAT) == HIGH;
}

// ============================================================================
// EMERGENCY ACTIONS
// ============================================================================

void SafetyWatchdog::emergencyShutdown() {
  allPumpsOff(PumpReason::EMERGENCY_SHUTDOWN);
  _emergency = true;
  _emergencyDraining = false;
}

void SafetyWatchdog::emergencyDrain() {
  // Shut everything off first
  allPumpsOff(PumpReason::EMERGENCY_DRAIN);

  // Open drain valve
  pumpOn(PIN_DRAIN, PumpReason::EMERGENCY_DRAIN);

  _emergency = true;
  _emergencyDraining = true;
  _emergencyDrainStart = millis();
}

// ============================================================================
// MAINTENANCE MODE
// ============================================================================

void SafetyWatchdog::enterMaintenance() {
  Serial.println("[Safety] Maintenance mode ENABLED (30 min timer).");
  _maintenance = true;
  _maintenanceStart = millis();
}

void SafetyWatchdog::exitMaintenance() {
  Serial.println("[Safety] Maintenance mode DISABLED.");
  _maintenance = false;
}

// ============================================================================
// UPDATE (called every loop)
// ============================================================================

void SafetyWatchdog::update() {
  unsigned long now = millis();

  // Rate-limit safety checks
  if ((now - _lastCheckMs) < SAFETY_CHECK_INTERVAL_MS)
    return;
  _lastCheckMs = now;

  // -- Maintenance auto-expire --
  if (_maintenance && (now - _maintenanceStart >= MAINTENANCE_DURATION_MS)) {
    Serial.println("[Safety] Maintenance timer expired.");
    exitMaintenance();
  }

  // Skip sensor-based safety during maintenance
  if (_maintenance)
    return;

  // Skip sensor-based safety if sensors not connected
  if (!_sensorsConnected) {
    _overflowFlag = false;
    return;
  }

  // -- Emergency drain timeout --
  _updateEmergencyDrain();

  // -- Optical sensor: immediate stop if water at max --
  if (isOpticalHigh()) {
    // Always stop refill/solenoid when optical is triggered
    pumpOff(PIN_REFILL, PumpReason::SAFETY_STOP);
    pumpOff(PIN_SOLENOID, PumpReason::SAFETY_STOP);
    _overflowFlag = true;
  } else {
    _overflowFlag = false;
  }

  // -- Ultrasonic overflow check --
  _checkOverflow();
}

void SafetyWatchdog::_checkOverflow() {
  float dist = readUltrasonic();
  if (dist < 0)
    return; // No valid reading

  // Lower distance = higher water level
  if (dist < LEVEL_SAFETY_MIN_CM && !_emergencyDraining) {
    _overflowConsecutiveCount++;
    Serial.printf(
        "[Safety] OVERFLOW WARNING! Distance=%.1f cm < %.1f cm (Count: %d/10)\n", dist,
        LEVEL_SAFETY_MIN_CM, _overflowConsecutiveCount);
    
    if (_overflowConsecutiveCount >= 10) {
      Serial.println("[Safety] CONFIRMED OVERFLOW (10 consecutive readings). Drain temporarily disabled!");
      // emergencyDrain();
    }
  } else {
    _overflowConsecutiveCount = 0;
  }
}

void SafetyWatchdog::_updateEmergencyDrain() {
  if (!_emergencyDraining)
    return;

  unsigned long elapsed = millis() - _emergencyDrainStart;

  // Check if water is now at safe level
  float dist = _lastDistance;
  if (dist > LEVEL_SAFETY_MIN_CM + 5.0f) {
    pumpOff(PIN_DRAIN, PumpReason::SAFETY_STOP);
    _emergencyDraining = false;
    _emergency = false;
    return;
  }

  // Timeout — stop even if water isn't safe (prevent running forever)
  if (elapsed >= TIMEOUT_EMERGENCY_MS) {
    Serial.println("[EMERGENCY] Drain timeout reached. FULL SHUTDOWN.");
    emergencyShutdown();
  }
}

float SafetyWatchdog::_medianOfFive(float *arr) {
  std::sort(arr, arr + 5);
  return arr[2];
}
