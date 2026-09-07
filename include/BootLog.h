#pragma once

#include <Arduino.h>
#include <esp_system.h> // esp_reset_reason_t (mocked under test/mocks on native)
#include <stdint.h>

// ============================================================================
// BOOT LOG — every reset the board survives, persisted to NVS
// ============================================================================
//
// `esp_reset_reason()` only ever describes the *previous* run, and it lives in
// RAM, so the answer to "was that brownout a one-off or has it been happening
// for weeks?" disappeared at the next reset. This keeps one entry per boot in
// a ring buffer in NVS.
//
// NVS rather than LittleFS on purpose: LittleFS writes deadlock the loop
// against the async web server task, which is why PumpLog stopped persisting
// (see PUMP_LOG_PERSIST in PumpLog.cpp). NVS has no such problem, and the
// write pattern here is two per boot at most, so flash wear is not a concern.

/// Entries kept. 32 x 16 bytes = 512 bytes in RAM and in NVS.
constexpr uint8_t BOOT_LOG_MAX = 32;

/// Previous run's length could not be recovered — the RTC counter did not
/// survive the reset, which is the normal case for a real power cycle.
constexpr uint32_t BOOT_UPTIME_UNKNOWN = 0xFFFFFFFFUL;

/// One boot (16 bytes).
struct BootLogEntry {
  uint32_t seq;         ///< Monotonic boot number, never reused.
  uint32_t epoch;       ///< Unix time of the boot, 0 while the clock was unset.
  uint32_t prevUptimeS; ///< Seconds the previous run lasted, or BOOT_UPTIME_UNKNOWN.
  uint8_t reason;       ///< esp_reset_reason_t stored as a raw byte.
  uint8_t _pad[3];
};

/// @brief Load the log from NVS and append this boot. Call once, early in
/// setup() — before WiFi, so a board that keeps dying during the 30 s connect
/// window still records every attempt.
/// @param resetReason Value from esp_reset_reason().
void bootLogBegin(uint8_t resetReason);

/// @brief Keep the uptime counter fresh. Call from loop(); touches RTC RAM
/// only, never flash.
void bootLogTick();

/// @brief Stamp this boot's entry with wall-clock time, once, the first time
/// the clock can be trusted. Later calls are ignored, so it is safe to call
/// from loop() unconditionally.
void bootLogStampTime(uint32_t epoch);

/// @brief Human-readable name for an esp_reset_reason_t byte.
const char *bootResetReasonName(uint8_t reason);

/// @brief Number of entries currently held.
uint8_t bootLogCount();

/// @brief The newest entry, or a zeroed entry when the log is empty.
BootLogEntry bootLogLast();

/// @brief The ring buffer as JSON, newest entry first.
String bootLogGetJSON();

#ifdef UNIT_TEST
/// @brief Test seam: drop the in-RAM ring and the uptime carry-over, the way a
/// power cycle does. Every test shares one process, so without this each test
/// would inherit the previous one's counter and entries.
void bootLogMockReset();
#endif
