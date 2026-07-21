#include "SafetyWatchdog.h"
#include "PumpLog.h"
#include <algorithm> // std::sort

SafetyWatchdog::SafetyWatchdog()
    : _lastDistance(-1), _overflowThresholdCm(5.0f), _emergency(false), _sensorsConnected(false),
      _ultrasonicFailCount(0), _overflowConsecutiveCount(0), _overflowFlag(false), _maintenance(false),
      _maintenanceStart(0), _lastCheckMs(0), _emergencyDraining(false),
      _emergencyDrainStart(0), _medianIndex(0), _medianCount(0) {}

void SafetyWatchdog::begin() {
  // Ultrasonic A02 UART
  Serial2.begin(9600, SERIAL_8N1, PIN_US_RX, PIN_US_TX);

  // Optical level sensor (active LOW, pulled up)
  pinMode(PIN_OPTICAL, INPUT_PULLUP);

  // Float switch (active HIGH, pulled down, connected to 3.3V)
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

bool SafetyWatchdog::isOpticalHigh() {
  // Active LOW with pullup: LOW = water detected = HIGH level
  return digitalRead(PIN_OPTICAL) == LOW;
}

bool SafetyWatchdog::isReservoirFull() {
  // Pin 19 is currently burned/reading false LOW.
  // Returning false temporarily so manual fill can work without sensor lock.
  // return digitalRead(PIN_FLOAT) == LOW;
  return false;
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
    _overflowFlag = false;
    return;
  }

  // -- Optical sensor: immediate stop if water at max --
  if (isOpticalHigh()) {
    // Always stop refill when optical is triggered
    pumpOff(PIN_REFILL, PumpReason::SAFETY_STOP);
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

  // 1. OVERFLOW PROTECTION
  if (dist < _overflowThresholdCm && !_emergencyDraining) {
    _overflowConsecutiveCount++;
        Serial.printf("[Safety] Overflow warning: dist=%.1f cm < %.1f cm (Count: %d/3)\n", dist,
                      _overflowThresholdCm, _overflowConsecutiveCount);
    
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
