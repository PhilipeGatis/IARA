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
  SAFETY_STOP,         // Safety sensor triggered (overflow)
  ERROR_STOP,          // Error/timeout stop
  ABORT,               // User-initiated abort
  BOOT_INIT,           // Initialization during setup()
};

// ============================================================================
// RING BUFFER — persisted to LittleFS, survives reboots
// ============================================================================

/// Max entries in ring buffer (~2.5 KB RAM + ~2.5 KB flash)
constexpr uint8_t PUMP_LOG_MAX = 100;

/// Flush interval: write to flash at most every 10 seconds (flash wear protection)
constexpr unsigned long PUMP_LOG_FLUSH_INTERVAL_MS = 10000;

/// Single pump event entry (24 bytes)
struct PumpLogEntry {
  char timestamp[20]; // "YYYY/MM/DD HH:MM:SS" or "12345ms"
  uint8_t pin;
  bool state;         // true=ON, false=OFF
  PumpReason reason;
  uint8_t _pad;       // alignment padding
};

// ============================================================================
// PUMP LOG API
// ============================================================================

/// @brief Initialize PumpLog with a time formatting callback.
/// Call once in setup() after TimeManager is initialized.
/// If not called or callback is nullptr, falls back to millis().
void pumpLogInit(TimeFormatCallback cb);

/// @brief Load pump log from LittleFS into RAM ring buffer.
/// Call once in setup() AFTER LittleFS.begin().
void pumpLogLoad();

/// @brief Flush dirty ring buffer to LittleFS (debounced).
/// Call periodically from loop(). Only writes if dirty and interval elapsed.
void pumpLogFlush();

/// @brief Activate an actuator pin (HIGH) with logging.
void pumpOn(uint8_t pin, PumpReason reason);

/// @brief Deactivate an actuator pin (LOW) with logging.
void pumpOff(uint8_t pin, PumpReason reason);

/// @brief Deactivate ALL output pins with logging.
void allPumpsOff(PumpReason reason);

/// @brief Get human-readable name for a pin.
const char *pinName(uint8_t pin);

/// @brief Get human-readable name for a reason.
const char *reasonName(PumpReason r);

/// @brief Get the ring buffer as a JSON string.
/// Entries are ordered oldest → newest.
String pumpLogGetJSON();

/// @brief Get the number of entries currently in the ring buffer.
uint8_t pumpLogCount();
