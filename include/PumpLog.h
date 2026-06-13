#pragma once

#include "Config.h"
#include <Arduino.h>

/// @brief Callback type for getting formatted time string.
/// Returns a formatted timestamp (e.g., "HH:MM:SS" or "YYYY/MM/DD HH:MM:SS").
typedef String (*TimeFormatCallback)();

// ============================================================================
// PUMP ACTIVATION REASONS
// ============================================================================

/// @brief Standardized reasons for pump/actuator activation.
/// Every call to pumpOn/pumpOff must specify a reason for diagnostics.
enum class PumpReason : uint8_t {
  TPA_DRAINING,        // TPA state machine: DRAINING state
  TPA_REFILLING,       // TPA state machine: REFILLING state
  TPA_CANISTER,        // TPA state machine: canister on/off
  TPA_SOLENOID,        // TPA state machine: filling reservoir
  TPA_PRIME,           // TPA state machine: dosing Prime
  TPA_TARGET_REACHED,  // TPA: target level reached, stopping pump
  MANUAL_PUMP,         // Manual pump activation via UI/API
  MANUAL_SOLENOID,     // Manual solenoid activation via UI/API
  CALIBRATION,         // Run3s calibration pulse
  EMERGENCY_DRAIN,     // Emergency drain (overflow detected)
  EMERGENCY_SHUTDOWN,  // Emergency shutdown (all outputs OFF)
  SAFETY_STOP,         // Safety sensor triggered (optical/overflow)
  ERROR_STOP,          // Error/timeout stop
  ABORT,               // User-initiated abort
  BOOT_INIT,           // Initialization during setup()
};

// ============================================================================
// PUMP LOG API
// ============================================================================

/// @brief Initialize PumpLog with a time formatting callback.
/// Call once in setup() after TimeManager is initialized.
/// If not called or callback is nullptr, falls back to millis().
void pumpLogInit(TimeFormatCallback cb);

/// @brief Activate an actuator pin (HIGH) with logging.
/// Logs: [PUMP] HH:MM:SS | PIN_NAME | ON | REASON
void pumpOn(uint8_t pin, PumpReason reason);

/// @brief Deactivate an actuator pin (LOW) with logging.
/// Logs: [PUMP] HH:MM:SS | PIN_NAME | OFF | REASON
void pumpOff(uint8_t pin, PumpReason reason);

/// @brief Deactivate ALL output pins with logging.
/// Iterates OUTPUT_PINS[] and calls pumpOff() for each.
void allPumpsOff(PumpReason reason);

/// @brief Get human-readable name for a pin.
const char *pinName(uint8_t pin);

/// @brief Get human-readable name for a reason.
const char *reasonName(PumpReason r);
