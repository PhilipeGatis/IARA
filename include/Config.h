#pragma once

#include <Arduino.h>

// ============================================================================
// HARDWARE PIN MAPPING
// ============================================================================

// --- MOSFET Channels (Active HIGH) ---
// Fertilizers CH1-CH4
constexpr uint8_t PIN_FERT1 = 13; // CH1 - Fertilizer 1
constexpr uint8_t PIN_FERT2 = 12; // CH2 - Fertilizer 2
constexpr uint8_t PIN_FERT3 = 14; // CH3 - Fertilizer 3
constexpr uint8_t PIN_FERT4 = 27; // CH4 - Fertilizer 4
constexpr uint8_t PIN_PRIME = 26; // CH5 - Prime (dechlorinator)

// TPA Actuators
constexpr uint8_t PIN_DRAIN = 25;    // CH6 - Drain pump
constexpr uint8_t PIN_REFILL = 33;   // CH7 - Refill pump (recalque)
constexpr uint8_t PIN_SOLENOID = 32; // CH8 - Solenoid valve

// Filtration
constexpr uint8_t PIN_CANISTER = 2; // Relay SSR for canister filter

// --- Sensors ---
constexpr uint8_t PIN_TRIG = 18; // Ultrasonic JSN-SR04T trigger
constexpr uint8_t PIN_ECHO = 19; // Ultrasonic JSN-SR04T echo
constexpr uint8_t PIN_OPTICAL =
    4; // Capacitive max-level sensor XKC-Y25-NPN (INPUT_PULLUP, active LOW)
constexpr uint8_t PIN_FLOAT =
    5; // Horizontal float switch reservoir (INPUT_PULLUP, active LOW)
constexpr uint8_t PIN_TPA_BUTTON =
    15; // Manual TPA start button (INPUT_PULLUP, active LOW)
constexpr uint8_t PIN_FERT_BUTTON =
    23; // Manual fertilization button (INPUT_PULLUP, active LOW)

// --- I2C (DS3231 RTC) ---
// Using ESP32 default I2C: SDA=21, SCL=22

// ============================================================================
// ALL OUTPUT PINS (for batch initialization)
// ============================================================================
constexpr uint8_t OUTPUT_PINS[] = {PIN_FERT1,  PIN_FERT2,    PIN_FERT3,
                                   PIN_FERT4,  PIN_PRIME,    PIN_DRAIN,
                                   PIN_REFILL, PIN_SOLENOID, PIN_CANISTER};
constexpr uint8_t NUM_OUTPUT_PINS =
    sizeof(OUTPUT_PINS) / sizeof(OUTPUT_PINS[0]);

// Fertilizer pin array for indexed access
constexpr uint8_t FERT_PINS[] = {PIN_FERT1, PIN_FERT2, PIN_FERT3, PIN_FERT4};
constexpr uint8_t NUM_FERTS = 4;

// ============================================================================
// TIMING & SAFETY CONSTANTS
// ============================================================================

#ifdef WOKWI_TEST
// ── Wokwi fast-simulation overrides (seconds instead of minutes) ─────────────

// Pump timeouts
constexpr unsigned long TIMEOUT_DRAIN_MS = 15UL * 1000;        // 15s
constexpr unsigned long TIMEOUT_FILL_MS = 15UL * 1000;         // 15s
constexpr unsigned long TIMEOUT_REFILL_MS = 15UL * 1000;       // 15s
constexpr unsigned long TIMEOUT_PRIME_MS = 5UL * 1000;         // 5s
constexpr unsigned long TIMEOUT_FERT_MS = 5UL * 1000;          // 5s
constexpr unsigned long TIMEOUT_EMERGENCY_MS = 10UL * 1000;    // 10s
constexpr unsigned long MAINTENANCE_DURATION_MS = 60UL * 1000; // 1 min

// Volumes and flow
constexpr float DEFAULT_DOSE_ML = 1.0f;       // 1 mL (fast)
constexpr float DEFAULT_PRIME_ML = 1.0f;      // 1 mL
constexpr float DEFAULT_STOCK_ML = 50.0f;     // 50 mL
constexpr float FLOW_RATE_ML_PER_SEC = 10.0f; // 10 mL/s (fast pump)
constexpr float DEFAULT_DRAIN_PCT = 20.0f;    // 20% drain

#else
// ── Production values ────────────────────────────────────────────────────────

// Pump timeouts
constexpr unsigned long TIMEOUT_DRAIN_MS = 5UL * 60 * 1000;         // 5 min
constexpr unsigned long TIMEOUT_FILL_MS = 10UL * 60 * 1000;         // 10 min
constexpr unsigned long TIMEOUT_REFILL_MS = 10UL * 60 * 1000;       // 10 min
constexpr unsigned long TIMEOUT_PRIME_MS = 60UL * 1000;             // 1 min
constexpr unsigned long TIMEOUT_FERT_MS = 30UL * 1000;              // 30 sec
constexpr unsigned long TIMEOUT_EMERGENCY_MS = 3UL * 60 * 1000;     // 3 min
constexpr unsigned long MAINTENANCE_DURATION_MS = 30UL * 60 * 1000; // 30 min

// Volumes and flow
constexpr float DEFAULT_DOSE_ML = 5.0f;      // Default dose per fertilizer
constexpr float DEFAULT_PRIME_ML = 10.0f;    // Default Prime dose
constexpr float DEFAULT_STOCK_ML = 500.0f;   // Default bottle size
constexpr float FLOW_RATE_ML_PER_SEC = 1.5f; // Peristaltic pump flow rate
constexpr float DEFAULT_DRAIN_PCT = 30.0f;   // Drain 30% of tank

#endif // WOKWI_TEST

// -- NTP sync interval --
constexpr unsigned long NTP_SYNC_INTERVAL_MS = 24UL * 3600 * 1000; // 24 h

// -- Ultrasonic --
constexpr float ULTRASONIC_MAX_DISTANCE_CM = 400.0f;
constexpr uint8_t ULTRASONIC_SAMPLES = 5; // Median filter samples
constexpr unsigned long ULTRASONIC_PULSE_TIMEOUT_US =
    30000; // 30 ms echo timeout

// -- Water levels (distance from sensor in cm — lower distance = higher water)
constexpr float LEVEL_SAFETY_MIN_CM = 5.0f;     // Overflow alert
constexpr float LEVEL_DRAIN_TARGET_CM = 20.0f;  // Default TPA drain target
constexpr float LEVEL_REFILL_TARGET_CM = 10.0f; // Default refill setpoint

// -- TPA defaults --
constexpr uint8_t DEFAULT_TPA_DAY = 0; // Sunday
constexpr uint8_t DEFAULT_TPA_HOUR = 10;
constexpr uint8_t DEFAULT_TPA_MINUTE = 0;
constexpr uint8_t DEFAULT_FERT_HOUR = 9;
constexpr uint8_t DEFAULT_FERT_MINUTE = 0;

// -- Loop timing --
constexpr unsigned long TELEMETRY_INTERVAL_MS = 10000;  // 10s
constexpr unsigned long SAFETY_CHECK_INTERVAL_MS = 500; // 500ms
