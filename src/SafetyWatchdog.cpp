#include "SafetyWatchdog.h"
#include "PumpLog.h"
#include <algorithm> // std::sort
#include <cmath>     // fabsf

SafetyWatchdog::SafetyWatchdog()
    : _lastDistance(-1), _overflowThresholdCm(0.0f), _emergency(false), _sensorsConnected(false),
      _ultrasonicFailCount(0), _overflowConsecutiveCount(0), _maintenance(false),
      _maintenanceStart(0), _lastCheckMs(0), _emergencyDraining(false),
      _emergencyDrainStart(0), _medianIndex(0), _medianCount(0) {}

void SafetyWatchdog::begin() {
  // Ultrasonic A02YYUW UART. Nothing here ever writes to Serial2 — the sensor
  // streams frames on its own — but PIN_US_TX stays assigned so the control
  // lead wired to it is held at the idle-high level continuous mode needs.
  Serial2.begin(9600, SERIAL_8N1, PIN_US_RX, PIN_US_TX);

  // Float switch on D19 (active LOW, internal pullup, switch wired to GND)
  pinMode(PIN_FLOAT, INPUT_PULLUP);

  // Give the A02YYUW sensor time to power up and start transmitting.
  // Without this delay, Serial2 reads garbage on boot and the sensor
  // appears disconnected until physically re-plugged.
  delay(500);
  while (Serial2.available()) Serial2.read(); // Flush any startup noise

  // Initial sensor probe — detect if ultrasonic is connected
  readUltrasonic();

  Serial.printf("[Safety] Watchdog initialized. Sensors: %s\n",
                _sensorsConnected ? "CONNECTED" : "NOT CONNECTED");
}

// ============================================================================
// SENSOR READS
// ============================================================================

float SafetyWatchdog::readUltrasonic() {
  bool newData = false;
  static unsigned long lastValidMs = millis();

  // Read all available UART frames (each frame is 4 bytes: 0xFF, High, Low, Checksum)
  while (Serial2.available() >= 4) {
    if (Serial2.peek() == 0xFF) {
      uint8_t header = Serial2.read();
      uint8_t dataH = Serial2.read();
      uint8_t dataL = Serial2.read();
      uint8_t sum = Serial2.read();
      
      if (((header + dataH + dataL) & 0xFF) == sum) {
        float distance = ((dataH << 8) | dataL) / 10.0f; // mm to cm
        if (distance > 0 && distance <= ULTRASONIC_MAX_DISTANCE_CM) {
          // Store in median buffer (circular)
          _medianBuffer[_medianIndex] = distance;
          _medianIndex = (_medianIndex + 1) % MEDIAN_BUFFER_SIZE;
          if (_medianCount < MEDIAN_BUFFER_SIZE) _medianCount++;

          // Calculate median of available samples
          float sorted[MEDIAN_BUFFER_SIZE];
          memcpy(sorted, _medianBuffer, _medianCount * sizeof(float));
          std::sort(sorted, sorted + _medianCount);
          _lastDistance = sorted[_medianCount / 2];

          newData = true;
          lastValidMs = millis();
        }
      }
    } else {
      Serial2.read(); // Discard garbage byte and try again
    }
  }

  // Handle connection status tracking (2 seconds timeout)
  if (newData) {
    if (!_sensorsConnected) {
      _sensorsConnected = true;
      Serial.println("[Safety] Ultrasonic A02 connected — safety checks enabled.");
    }
  } else if (millis() - lastValidMs > 2000) {
    if (_sensorsConnected) {
      _sensorsConnected = false;
      Serial.println("[Safety] Ultrasonic A02 disconnected (timeout) — safety checks disabled.");
    }
  }

  return _lastDistance;
}

bool SafetyWatchdog::isReservoirFull() {
  // Active LOW with pullup: LOW = float triggered = reservoir full
  return digitalRead(PIN_FLOAT) == LOW;
}

// ============================================================================
// EMERGENCY ACTIONS
// ============================================================================

void SafetyWatchdog::emergencyShutdown() {
  Serial.println("[EMERGENCY] >>> SHUTDOWN: All outputs OFF <<<");
  for (uint8_t i = 0; i < NUM_OUTPUT_PINS; i++) {
    digitalWrite(OUTPUT_PINS[i], LOW);
  }
  _emergency = true;
  _emergencyDraining = false;
}

void SafetyWatchdog::clearEmergency() {
  Serial.println("[EMERGENCY] Emergency state cleared manually.");
  _emergency = false;
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

  // -- Emergency drain timeout --
  // CRITICAL: This MUST run even if sensors are disconnected, 
  // otherwise an active emergency will never time out!
  _updateEmergencyDrain();

  // Always read the ultrasonic sensor so it can reconnect after boot.
  // Without this, if the sensor misses the initial probe in begin(),
  // _sensorsConnected stays false forever (deadlock).
  readUltrasonic();

  // Skip sensor-based safety during maintenance
  if (_maintenance)
    return;

  // Skip sensor-based safety if sensors not connected
  if (!_sensorsConnected) {
    return;
  }

  // NOTE: max-level cutoff is handled in hardware by a reed switch in series
  // with the refill MOSFET gate signal. It cuts the pump without firmware.

  // -- Ultrasonic overflow check --
  _checkOverflow();
}

void SafetyWatchdog::_checkOverflow() {
  // Disabled until the level sensor is calibrated. Acting on a threshold
  // derived from placeholder config is how a fresh board drains a healthy tank.
  if (_overflowThresholdCm <= 0)
    return;

  float dist = readUltrasonic();
  if (dist < 0)
    return; // No valid reading

  // Reject physically impossible jumps. Water cannot move several centimetres
  // in half a second, so a step this large means the sensor was moved, knocked
  // or is misreading — the exact situation where draining the tank would be
  // the wrong answer. Restart the streak and wait for readings to settle.
  if (_prevOverflowDistance >= 0 &&
      fabsf(dist - _prevOverflowDistance) > MAX_PLAUSIBLE_LEVEL_STEP_CM) {
    Serial.printf("[Safety] Implausible level step: %.1f -> %.1f cm. "
                  "Ignoring (sensor moved?).\n",
                  _prevOverflowDistance, dist);
    _prevOverflowDistance = dist;
    _overflowConsecutiveCount = 0;
    return;
  }
  _prevOverflowDistance = dist;

  // 1. OVERFLOW PROTECTION
  if (dist < _overflowThresholdCm && !_emergencyDraining) {
    _overflowConsecutiveCount++;
    Serial.printf("[Safety] Overflow warning: dist=%.1f cm < %.1f cm (%d/10)\n",
                  dist, _overflowThresholdCm, _overflowConsecutiveCount);
    
    if (_overflowConsecutiveCount >= 10) {
      // Reached only with a live sensor: update() returns early when the
      // ultrasonic is disconnected, readings are median-filtered over 5
      // samples and checksum-validated, and 10 consecutive hits at the 500 ms
      // check interval means the level has been over the threshold for ~5 s.
      Serial.println("[Safety] CONFIRMED OVERFLOW (10 consecutive readings). "
                     "Starting emergency drain.");
      emergencyDrain();
    }
  } else {
    _overflowConsecutiveCount = 0;
  }
}

void SafetyWatchdog::_updateEmergencyDrain() {
  if (!_emergencyDraining)
    return;

  unsigned long elapsed = millis() - _emergencyDrainStart;

  // Hysteresis: only clear emergency drain if level falls 5cm below threshold
  float dist = _lastDistance;
  if (dist > _overflowThresholdCm + 5.0f) {
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
