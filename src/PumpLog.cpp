#include "PumpLog.h"

// ============================================================================
// INTERNAL STATE
// ============================================================================

static TimeFormatCallback _timeCb = nullptr;

// ============================================================================
// INITIALIZATION
// ============================================================================

void pumpLogInit(TimeFormatCallback cb) { _timeCb = cb; }

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
// LOGGING CORE
// ============================================================================

static void _log(uint8_t pin, bool state, PumpReason reason) {
  // Get timestamp: prefer callback, fallback to millis
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

void pumpOn(uint8_t pin, PumpReason reason) {
  digitalWrite(pin, HIGH);
  _log(pin, true, reason);
}

void pumpOff(uint8_t pin, PumpReason reason) {
  digitalWrite(pin, LOW);
  _log(pin, false, reason);
}

void allPumpsOff(PumpReason reason) {
  for (uint8_t i = 0; i < NUM_OUTPUT_PINS; i++) {
    digitalWrite(OUTPUT_PINS[i], LOW);
  }
  // Single summary log instead of one per pin
  if (_timeCb) {
    Serial.printf("[PUMP] %s | ALL       | OFF | %s\n",
                  _timeCb().c_str(), reasonName(reason));
  } else {
    Serial.printf("[PUMP] %lums | ALL       | OFF | %s\n", millis(),
                  reasonName(reason));
  }
}
