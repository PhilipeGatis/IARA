#include "PumpLog.h"

#ifndef UNIT_TEST
#include <LittleFS.h>
#endif

// ============================================================================
// INTERNAL STATE
// ============================================================================

static TimeFormatCallback _timeCb = nullptr;

// Ring buffer
static PumpLogEntry _logBuffer[PUMP_LOG_MAX];
static uint8_t _logHead = 0;  // Next write position
static uint8_t _logCount = 0; // Current number of entries

// Persistence state
static bool _dirty = false;
static unsigned long _lastFlushMs = 0;

#ifndef UNIT_TEST

static const char *LOG_FILE = "/pumplog.bin";

// File header magic to detect valid log files
static const uint32_t LOG_MAGIC = 0x504C4F47; // "PLOG"

struct PumpLogHeader {
  uint32_t magic;
  uint8_t count;
  uint8_t head;
  uint8_t version; // for future format changes
  uint8_t _pad;
};

// ============================================================================
// INITIALIZATION
// ============================================================================

void pumpLogInit(TimeFormatCallback cb) { _timeCb = cb; }

// ============================================================================
// PERSISTENCE — LittleFS
// ============================================================================

void pumpLogLoad() {
  File f = LittleFS.open(LOG_FILE, "r");
  if (!f) {
    Serial.println("[PumpLog] No saved log file found. Starting fresh.");
    return;
  }

  PumpLogHeader hdr;
  if (f.read((uint8_t *)&hdr, sizeof(hdr)) != sizeof(hdr) ||
      hdr.magic != LOG_MAGIC || hdr.count > PUMP_LOG_MAX) {
    Serial.println("[PumpLog] Invalid log file. Starting fresh.");
    f.close();
    return;
  }

  size_t dataSize = PUMP_LOG_MAX * sizeof(PumpLogEntry);
  size_t readSize = f.read((uint8_t *)_logBuffer, dataSize);
  f.close();

  if (readSize != dataSize) {
    Serial.println("[PumpLog] Corrupt log file. Starting fresh.");
    memset(_logBuffer, 0, dataSize);
    _logCount = 0;
    _logHead = 0;
    return;
  }

  _logCount = hdr.count;
  _logHead = hdr.head;
  Serial.printf("[PumpLog] Loaded %d events from flash.\n", _logCount);
}

void pumpLogFlush() {
  // DISABLING FLUSH TEMPORARILY: LittleFS writes are deadlocking the loop.
  // The system will only keep logs in RAM until we fix the file system issue.
  return;
  if (!_dirty) return;

  unsigned long now = millis();
  if (now - _lastFlushMs < PUMP_LOG_FLUSH_INTERVAL_MS) return;

  File f = LittleFS.open(LOG_FILE, "w");
  if (!f) {
    Serial.println("[PumpLog] Failed to open log file for writing!");
    return;
  }

  PumpLogHeader hdr;
  hdr.magic = LOG_MAGIC;
  hdr.count = _logCount;
  hdr.head = _logHead;
  hdr.version = 1;
  hdr._pad = 0;

  f.write((uint8_t *)&hdr, sizeof(hdr));
  f.write((uint8_t *)_logBuffer, PUMP_LOG_MAX * sizeof(PumpLogEntry));
  f.close();

  _dirty = false;
  _lastFlushMs = now;
}

#else // UNIT_TEST stubs

void pumpLogInit(TimeFormatCallback cb) { _timeCb = cb; }
void pumpLogLoad() {}
void pumpLogFlush() {}

#endif // UNIT_TEST

// ============================================================================
// NAME LOOKUPS
// ============================================================================

const char *pinName(uint8_t pin) {
  if (pin == PIN_FERT1) return "FERT1";
  if (pin == PIN_FERT2) return "FERT2";
  if (pin == PIN_FERT3) return "FERT3";
  if (pin == PIN_FERT4) return "FERT4";
  if (pin == PIN_PRIME) return "PRIME";
  if (pin == PIN_DRAIN) return "DRAIN";
  if (pin == PIN_REFILL) return "REFILL";
  if (pin == PIN_SOLENOID) return "SOLENOID";
  if (pin == PIN_CANISTER) return "CANISTER";
  return "UNKNOWN";
}

const char *reasonName(PumpReason r) {
  switch (r) {
  case PumpReason::TPA_DRAINING:       return "TPA_DRAINING";
  case PumpReason::TPA_REFILLING:      return "TPA_REFILLING";
  case PumpReason::TPA_CANISTER:       return "TPA_CANISTER";
  case PumpReason::TPA_SOLENOID:       return "TPA_SOLENOID";
  case PumpReason::TPA_PRIME:          return "TPA_PRIME";
  case PumpReason::TPA_TARGET_REACHED: return "TPA_TARGET_REACHED";
  case PumpReason::MANUAL_PUMP:        return "MANUAL_PUMP";
  case PumpReason::MANUAL_SOLENOID:    return "MANUAL_SOLENOID";
  case PumpReason::CALIBRATION:        return "CALIBRATION";
  case PumpReason::EMERGENCY_DRAIN:    return "EMERGENCY_DRAIN";
  case PumpReason::EMERGENCY_SHUTDOWN: return "EMERGENCY_SHUTDOWN";
  case PumpReason::SAFETY_STOP:        return "SAFETY_STOP";
  case PumpReason::ERROR_STOP:         return "ERROR_STOP";
  case PumpReason::ABORT:              return "ABORT";
  case PumpReason::BOOT_INIT:          return "BOOT_INIT";
  default:                             return "UNKNOWN";
  }
}

// ============================================================================
// RING BUFFER
// ============================================================================

static void _addEntry(uint8_t pin, bool state, PumpReason reason) {
  PumpLogEntry &entry = _logBuffer[_logHead];

  // Timestamp
  if (_timeCb) {
    String ts = _timeCb();
    strncpy(entry.timestamp, ts.c_str(), sizeof(entry.timestamp) - 1);
    entry.timestamp[sizeof(entry.timestamp) - 1] = '\0';
  } else {
    snprintf(entry.timestamp, sizeof(entry.timestamp), "%lums", millis());
  }

  entry.pin = pin;
  entry.state = state;
  entry.reason = reason;
  entry._pad = 0;

  _logHead = (_logHead + 1) % PUMP_LOG_MAX;
  if (_logCount < PUMP_LOG_MAX) {
    _logCount++;
  }
  _dirty = true;
}

uint8_t pumpLogCount() { return _logCount; }

String pumpLogGetJSON() {
  String json = "{\"count\":";
  json += String(_logCount);
  json += ",\"log\":[";

  if (_logCount > 0) {
    // Start index: oldest entry in the ring buffer
    uint8_t start =
        (_logCount < PUMP_LOG_MAX) ? 0 : _logHead;

    for (uint8_t i = 0; i < _logCount; i++) {
      uint8_t idx = (start + i) % PUMP_LOG_MAX;
      const PumpLogEntry &e = _logBuffer[idx];

      if (i > 0) json += ",";
      json += "{\"t\":\"";
      json += e.timestamp;
      json += "\",\"pin\":\"";
      json += pinName(e.pin);
      json += "\",\"state\":\"";
      json += e.state ? "ON" : "OFF";
      json += "\",\"reason\":\"";
      json += reasonName(e.reason);
      json += "\"}";
    }
  }

  json += "]}";
  return json;
}

// ============================================================================
// LOGGING CORE
// ============================================================================

static void _log(uint8_t pin, bool state, PumpReason reason) {
  // Store in ring buffer (+ mark dirty for flash persistence)
  _addEntry(pin, state, reason);

  // Also print to Serial for USB debugging
  if (_timeCb) {
    Serial.printf("[PUMP] %s | %-9s | %-3s | %s\n",
                  _timeCb().c_str(), pinName(pin),
                  state ? "ON" : "OFF", reasonName(reason));
  } else {
    Serial.printf("[PUMP] %lums | %-9s | %-3s | %s\n", millis(), pinName(pin),
                  state ? "ON" : "OFF", reasonName(reason));
  }
}

// ============================================================================
// PUBLIC API
// ============================================================================

/// LEDC channel driving this pin, or -1 if it is a plain GPIO.
/// FertManager routes CH1-CH4 and Prime through the LEDC peripheral, and once
/// a pad is switched to LEDC in the GPIO matrix, digitalWrite() no longer
/// controls it — the PWM peripheral keeps driving the pin. Without this the
/// emergency stop silently fails to stop five of the nine outputs.
static int _ledcChannelFor(uint8_t pin) {
  for (uint8_t i = 0; i < NUM_FERTS; i++) {
    if (FERT_PINS[i] == pin) return i;
  }
  if (pin == PIN_PRIME) return NUM_FERTS;
  return -1;
}

/// Drive an actuator pin to its inactive state, whatever mechanism owns it.
static void _driveOff(uint8_t pin) {
  // Do both rather than choose. Whether a dosing pin is currently owned by LEDC
  // depends on FertManager::begin() having run, and a shutdown must not depend
  // on start-up ordering. Writing LOW to a LEDC-owned pad is ignored, and
  // writing to an unattached channel is ignored — so both are always safe.
  const int ch = _ledcChannelFor(pin);
  if (ch >= 0) ledcWrite(ch, 0);

  // The canister runs on an active-LOW SSR, so "off" is HIGH for that one pin.
  // A blanket LOW across every output would switch the filter ON — the exact
  // opposite of what a shutdown means, and at the worst possible moment.
  digitalWrite(pin, pin == PIN_CANISTER ? HIGH : LOW);
}

void pumpOn(uint8_t pin, PumpReason reason) {
  digitalWrite(pin, HIGH);
  _log(pin, true, reason);
}

void pumpOff(uint8_t pin, PumpReason reason) {
  // pumpOff(PIN_CANISTER) means "SSR LOW", which turns the filter ON — that is
  // the existing convention and callers rely on it, so it is left alone here.
  // _driveOff() is the one that knows about safe states.
  const int ch = _ledcChannelFor(pin);
  if (ch >= 0) ledcWrite(ch, 0);
  digitalWrite(pin, LOW);
  _log(pin, false, reason);
}

void allPumpsOff(PumpReason reason) {
  for (uint8_t i = 0; i < NUM_OUTPUT_PINS; i++) {
    _driveOff(OUTPUT_PINS[i]);
  }
  // Log each pin individually in the ring buffer for traceability
  for (uint8_t i = 0; i < NUM_OUTPUT_PINS; i++) {
    _addEntry(OUTPUT_PINS[i], false, reason);
  }
  // Single summary log to Serial
  if (_timeCb) {
    Serial.printf("[PUMP] %s | ALL       | OFF | %s\n",
                  _timeCb().c_str(), reasonName(reason));
  } else {
    Serial.printf("[PUMP] %lums | ALL       | OFF | %s\n", millis(),
                  reasonName(reason));
  }
}
