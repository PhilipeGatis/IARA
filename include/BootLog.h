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

/// Entries kept. 24 x 32 bytes = 768 bytes in RAM and in NVS.
///
/// Was 32 when an entry was half this size. The snapshot doubled the entry, and
/// the NVS partition is only 16 KB with the fertiliser schedules, the pump
/// calibration and the WiFi credentials already in it, so the ring gave back
/// what the snapshot took. 24 boots is still weeks of history on a board that
/// is not resetting, and more than enough on one that is.
constexpr uint8_t BOOT_LOG_MAX = 24;

/// State of the station at the moment the snapshot was taken.
enum class BootWifiState : uint8_t {
  UNKNOWN = 0,
  CONNECTED = 1,
  DOWN = 2,      ///< Station lost, recovery ladder running.
  AP_FALLBACK = 3, ///< Soft AP up because the station never came back.
};

/// What the board was doing, carried across the reset in RTC memory.
///
/// A brownout cannot be logged as it happens — the CPU is reset immediately and
/// writing flash on a collapsing rail is how flash gets corrupted. So the loop
/// keeps this up to date in RTC RAM, which costs nothing, and the next boot
/// reads it out of the wreckage.
///
/// It answers one question: was the radio busy? A brownout is a supply event,
/// and the only load on this board that moves without anyone asking is WiFi —
/// a weak link means transmitting harder and retransmitting more. Several
/// brownouts all landing on a bad RSSI with a retry storm in flight would be an
/// accusation; several landing on a healthy idle link clear the radio and leave
/// the supply itself, which is the more expensive thing to go and measure.
struct BootSnapshot {
  int8_t rssi;             ///< dBm, 0 when not associated.
  uint8_t wifiState;       ///< BootWifiState as a raw byte.
  uint16_t wifiRetries;    ///< Reconnection attempts since the station dropped.
  int16_t wifiReason;      ///< Last disconnect reason code, -1 if none.
  uint16_t wifiQuietS;     ///< Seconds since the last disconnect event.
  uint16_t wifiDisconnects;///< Disconnect events counted this run.
  uint8_t sseClients;      ///< Dashboards streaming: constant TX every 3 s.
  uint8_t _pad;
  uint16_t outputsMask;    ///< Actuators the log believes are on, by OUTPUT_PINS index.
  uint16_t freeHeapKb;
};

/// Snapshot taken before any loop pass had a chance to fill one in.
constexpr int8_t BOOT_RSSI_UNKNOWN = 0;

/// Previous run's length could not be recovered — the RTC counter did not
/// survive the reset, which is the normal case for a real power cycle.
constexpr uint32_t BOOT_UPTIME_UNKNOWN = 0xFFFFFFFFUL;

/// One boot (32 bytes).
struct BootLogEntry {
  uint32_t seq;         ///< Monotonic boot number, never reused.
  uint32_t epoch;       ///< Unix time of the boot, 0 while the clock was unset.
  uint32_t prevUptimeS; ///< Seconds the previous run lasted, or BOOT_UPTIME_UNKNOWN.
  uint8_t reason;       ///< esp_reset_reason_t stored as a raw byte.
  uint8_t _pad[3];
  BootSnapshot snap;    ///< What the previous run was doing when it ended.
};

/// @brief Load the log from NVS and append this boot. Call once, early in
/// setup() — before WiFi, so a board that keeps dying during the 30 s connect
/// window still records every attempt.
/// @param resetReason Value from esp_reset_reason().
void bootLogBegin(uint8_t resetReason);

/// @brief Keep the uptime counter fresh. Call from loop(); touches RTC RAM
/// only, never flash.
void bootLogTick();

/// @brief Record what the board is doing right now, for the next boot to read.
///
/// Call from loop(). Cheap enough to call often, but the caller decides how
/// often — WiFi.RSSI() reaches into the driver, so once a second is plenty and
/// twenty times a second is waste. RTC RAM only, never flash.
void bootLogUpdateSnapshot(const BootSnapshot &snapshot);

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
