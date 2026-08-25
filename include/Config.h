#pragma once

#include <Arduino.h>

// ============================================================================
// FIRMWARE VERSION
// ============================================================================
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "3.2.1"
#endif

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
// The A02YYUW's control lead is wired here. Nothing writes to Serial2 — the
// sensor streams unprompted — but keeping it as the UART TX pin holds the line
// at the idle-high level the sensor needs to stay in continuous mode.
constexpr uint8_t PIN_US_TX = 18; // Ultrasonic A02 control lead (idle high)
constexpr uint8_t PIN_US_RX = 34; // Ultrasonic A02 UART RX (from TX on sensor)
// NOTE: GPIO4 was the XKC-Y25 capacitive max-level sensor. That sensor was
// never installed and has been dropped: max-level protection is now a physical
// reed switch wired in series with the refill MOSFET gate signal (GPIO33 ->
// IN7), which cuts the pump independently of the firmware. GPIO4 is free.
constexpr uint8_t PIN_FLOAT =
    19; // Horizontal float switch reservoir (Moved from 5 to 19 for stability)
// --- TFT Display (ST7735, SPI) ---
// Board pins: VCC, GND, CS, RESET, A0, SDA, SCK, LED
constexpr uint8_t PIN_TFT_CS = 15;   // CS   — Chip Select
constexpr uint8_t PIN_TFT_DC = 17;   // A0   — Data/Command
constexpr uint8_t PIN_TFT_MOSI = 23; // SDA  — SPI Data
constexpr uint8_t PIN_TFT_SCK = 16;  // SCK  — SPI Clock
constexpr int8_t PIN_TFT_RST = -1;   // RESET — Tied to ESP32 EN

// --- Navigation Button ---
// Panel button. GPIO5 is a strapping pin, so it must not be held down through
// power-on; a momentary press at any other time is harmless.
constexpr uint8_t PIN_BTN = 5; // Panel button (INPUT_PULLUP, active LOW)

// --- Display ---
// Blank the TFT after this long without a button press. Only clears pixels —
// the backlight is hardwired to 3.3V on this module and cannot be switched.
constexpr unsigned long DISPLAY_TIMEOUT_MS = 30UL * 1000; // 30s auto-off

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

// Pump timeouts
constexpr unsigned long TIMEOUT_DRAIN_MS = 5UL * 60 * 1000;         // 5 min
constexpr unsigned long TIMEOUT_FILL_MS = 10UL * 60 * 1000;         // 10 min
// Backstop for filling the reservoir from the mains solenoid. The float switch
// is the real stop; this only bounds a stuck float. A ~18 L reservoir on a
// typical ~5 L/min solenoid fills in about 4 minutes, so 20 minutes is generous
// without leaving mains water running for hours.
constexpr unsigned long TIMEOUT_RESERVOIR_FILL_MS = 20UL * 60 * 1000; // 20 min
constexpr unsigned long TIMEOUT_REFILL_MS = 10UL * 60 * 1000;       // 10 min
constexpr unsigned long TIMEOUT_PRIME_MS = 60UL * 1000;             // 1 min
constexpr unsigned long TIMEOUT_FERT_MS = 30UL * 1000;              // 30 sec
constexpr unsigned long TIMEOUT_EMERGENCY_MS = 3UL * 60 * 1000;     // 3 min
// Hard ceiling for a manual pump run. The real limit is dynamic — twice the
// expected duration once a flow rate is known — this is only the fallback for
// an uncalibrated pump.
constexpr unsigned long MANUAL_PUMP_MAX_MS = 10UL * 60 * 1000;      // 10 min
// Floor for that dynamic budget, so a tiny goal still gets time to start up.
constexpr unsigned long MANUAL_PUMP_MIN_MS = 30UL * 1000;           // 30 s

// Overflow trips when the level rises this far above the calibrated 100% mark,
// expressed as a percentage of aquarium height. The 100% line is meant to sit
// 2–3 cm below the rim, so this fires while there is still headroom to react.
constexpr float OVERFLOW_TOLERANCE_PCT = 3.0f;

// Largest level change (cm) that can plausibly happen between two safety checks
// 500 ms apart. Even a refill pump moves the surface by a fraction of a
// millimetre in that time, so anything beyond this is the sensor being moved,
// knocked or lying — never the water. Such readings must not open the drain.
constexpr float MAX_PLAUSIBLE_LEVEL_STEP_CM = 3.0f;
constexpr unsigned long MAINTENANCE_DURATION_MS = 30UL * 60 * 1000; // 30 min

// Volumes and flow
constexpr float DEFAULT_DOSE_ML = 5.0f;      // Default dose per fertilizer
constexpr float DEFAULT_PRIME_ML = 10.0f;    // Default Prime dose
constexpr float DEFAULT_STOCK_ML = 500.0f;   // Default bottle size
constexpr float FLOW_RATE_ML_PER_SEC = 1.5f; // Peristaltic pump flow rate
constexpr float DEFAULT_DRAIN_PCT = 30.0f;   // Drain 30% of tank

// -- NTP sync interval --
constexpr unsigned long NTP_SYNC_INTERVAL_MS = 24UL * 3600 * 1000; // 24 h

// -- Ultrasonic --
constexpr float ULTRASONIC_MAX_DISTANCE_CM = 400.0f;
constexpr uint8_t ULTRASONIC_SAMPLES = 5; // Median filter samples
constexpr unsigned long ULTRASONIC_PULSE_TIMEOUT_US =
    30000; // 30 ms echo timeout

// -- Water levels (distance from sensor in cm — lower distance = higher water)
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

// -- Unit conversions --
// mL/s to L/min: multiply mL/s by 0.06 (= 60 / 1000)
constexpr float ML_PER_SEC_TO_LPM = 0.06f;
constexpr float LPM_TO_ML_PER_SEC = 1.0f / ML_PER_SEC_TO_LPM;

// -- Calibration pulse duration --
constexpr unsigned long CALIBRATION_PULSE_MS = 3000; // 3 seconds

// One-shot flow calibration: run a TPA pump for this long, then derive L/min
// from how far the level moved. Longer is more accurate, because the ultrasonic
// noise floor is a fixed number of millimetres regardless of run length.
// A flow rate is only trustworthy once the level has moved far enough to dwarf
// the sensor noise. Below this the measurement is mostly noise, so it is
// discarded rather than allowed to overwrite a good calibration.
constexpr float CALIBRATION_MIN_DELTA_PCT = 5.0f;

// Upper bound for a calibration run. It normally ends earlier, as soon as the
// level has moved CALIBRATION_MIN_DELTA_PCT.
constexpr unsigned long PUMP_CALIBRATION_MAX_MS = 5UL * 60 * 1000; // 5 min
