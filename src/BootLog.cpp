#include "BootLog.h"

#include <Preferences.h>

#ifndef UNIT_TEST
#include <nvs.h>
#include <nvs_flash.h>
#endif

// RTC slow memory keeps its contents across a reset but is not zeroed at
// start-up, which is exactly what the uptime counter needs. The attribute only
// exists on the ESP32 toolchain; on the host the variables are ordinary
// statics, so the magic below simply never matches and the previous uptime
// reads as unknown.
#ifndef UNIT_TEST
#define BOOTLOG_NOINIT RTC_NOINIT_ATTR
#else
#define BOOTLOG_NOINIT
#endif

// ============================================================================
// PERSISTENT STORE
// ============================================================================

static const char *NVS_NAMESPACE = "bootlog";
static const char *NVS_KEY = "ring";

static const uint32_t STORE_MAGIC = 0x424C4F47; // "BLOG"
static const uint8_t STORE_VERSION = 1;

/// Header and entries in one blob, so a boot is added with a single NVS write
/// and can never be half-committed.
struct BootLogStore {
  uint32_t magic;
  uint8_t version;
  uint8_t head;  // next write position
  uint8_t count; // entries held, <= BOOT_LOG_MAX
  uint8_t _pad;
  BootLogEntry entries[BOOT_LOG_MAX];
};

static BootLogStore _store;
static bool _loaded = false;

// ============================================================================
// UPTIME CARRIED ACROSS THE RESET
// ============================================================================

static const uint32_t RTC_MAGIC = 0x55505449; // "UPTI"

static BOOTLOG_NOINIT uint32_t _rtcMagic;
static BOOTLOG_NOINIT uint32_t _rtcUptimeS;

// Anchor for the tick accumulator. Ordinary RAM: it means nothing after a
// reset, and millis() restarts from zero anyway.
static uint32_t _tickAnchorMs = 0;

// At most one wall-clock stamp per boot.
static bool _stamped = false;

// ============================================================================
// INTERNALS
// ============================================================================

static void _persist() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) {
    Serial.println("[BootLog] NVS open failed — this boot will not be logged.");
    return;
  }
  const size_t written = prefs.putBytes(NVS_KEY, &_store, sizeof(_store));
  prefs.end();

  // The NVS partition is 16 KB and already carries the fertiliser schedules,
  // the pump calibration and the WiFi credentials. A silent failure here would
  // leave the log looking healthy in RAM and empty after the next reset —
  // which is the one moment it is consulted.
  if (written != sizeof(_store)) {
    Serial.printf("[BootLog] NVS write failed (%u of %u bytes). Log will not "
                  "survive this reset.\n",
                  (unsigned)written, (unsigned)sizeof(_store));
  }
}

/// Headroom left in the NVS partition, so a store that is filling up shows up
/// in the boot log itself rather than as a write failure months later.
static void _logNvsHeadroom() {
#ifndef UNIT_TEST
  nvs_stats_t stats;
  if (nvs_get_stats(nullptr, &stats) == ESP_OK) {
    Serial.printf("[BootLog] NVS entries: %u used, %u free of %u.\n",
                  (unsigned)stats.used_entries, (unsigned)stats.free_entries,
                  (unsigned)stats.total_entries);
  }
#endif
}

static void _resetStore() {
  memset(&_store, 0, sizeof(_store));
  _store.magic = STORE_MAGIC;
  _store.version = STORE_VERSION;
}

static void _load() {
  _resetStore();

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) {
    Serial.println("[BootLog] No saved log. Starting fresh.");
    return;
  }

  BootLogStore disk;
  const size_t read = prefs.getBytes(NVS_KEY, &disk, sizeof(disk));
  prefs.end();

  // A short read is an absent key or a store written by an older, smaller
  // format. Either way there is nothing safe to salvage, and the entries that
  // matter are the ones from here on.
  if (read != sizeof(disk) || disk.magic != STORE_MAGIC ||
      disk.version != STORE_VERSION || disk.count > BOOT_LOG_MAX ||
      disk.head >= BOOT_LOG_MAX) {
    Serial.println("[BootLog] No usable saved log. Starting fresh.");
    return;
  }

  _store = disk;
}

/// Newest entry's index, valid only when count > 0.
static uint8_t _newestIndex() {
  return (uint8_t)((_store.head + BOOT_LOG_MAX - 1) % BOOT_LOG_MAX);
}

// ============================================================================
// PUBLIC API
// ============================================================================

const char *bootResetReasonName(uint8_t reason) {
  switch (reason) {
  case ESP_RST_POWERON:   return "POWER_ON";
  case ESP_RST_EXT:       return "EXTERNAL";
  case ESP_RST_SW:        return "SOFTWARE";
  case ESP_RST_PANIC:     return "PANIC_EXCEPTION";
  case ESP_RST_INT_WDT:   return "INTERRUPT_WATCHDOG";
  case ESP_RST_TASK_WDT:  return "TASK_WATCHDOG";
  case ESP_RST_WDT:       return "OTHER_WATCHDOG";
  case ESP_RST_DEEPSLEEP: return "DEEP_SLEEP";
  case ESP_RST_BROWNOUT:  return "BROWNOUT";
  case ESP_RST_SDIO:      return "SDIO";
  default:                return "UNKNOWN";
  }
}

void bootLogBegin(uint8_t resetReason) {
  _load();
  _loaded = true;
  _stamped = false;

  // How long the run that just ended lasted. The RTC domain usually keeps its
  // power through a brownout reset — the detector fires well before the rail
  // collapses — but a real power cut wipes it, and a deep enough sag can too.
  // The magic is what tells those apart; without it a garbage value would read
  // as a plausible uptime.
  uint32_t prevUptime = BOOT_UPTIME_UNKNOWN;
  if (_rtcMagic == RTC_MAGIC) {
    prevUptime = _rtcUptimeS;
  }

  const uint32_t seq = (_store.count > 0) ? _store.entries[_newestIndex()].seq + 1 : 1;

  BootLogEntry &e = _store.entries[_store.head];
  e.seq = seq;
  e.epoch = 0; // filled in by bootLogStampTime() once the clock is trustworthy
  e.prevUptimeS = prevUptime;
  e.reason = resetReason;
  memset(e._pad, 0, sizeof(e._pad));

  _store.head = (uint8_t)((_store.head + 1) % BOOT_LOG_MAX);
  if (_store.count < BOOT_LOG_MAX) {
    _store.count++;
  }

  _persist();

  // Start this run's counter only after the previous value has been consumed.
  _rtcMagic = RTC_MAGIC;
  _rtcUptimeS = 0;
  _tickAnchorMs = millis();

  _logNvsHeadroom();

  if (prevUptime == BOOT_UPTIME_UNKNOWN) {
    Serial.printf("[BootLog] Boot #%lu, reason %s, previous uptime unknown.\n",
                  (unsigned long)seq, bootResetReasonName(resetReason));
  } else {
    Serial.printf("[BootLog] Boot #%lu, reason %s, previous run lasted %lus.\n",
                  (unsigned long)seq, bootResetReasonName(resetReason),
                  (unsigned long)prevUptime);
  }
}

void bootLogTick() {
  // Unsigned arithmetic, so this stays correct across the millis() rollover at
  // 49.7 days — the point at which a counter built on millis() alone would
  // report the board as having just started.
  const uint32_t elapsed = millis() - _tickAnchorMs;
  if (elapsed < 1000) return;

  const uint32_t whole = elapsed / 1000;
  _rtcUptimeS += whole;
  _tickAnchorMs += whole * 1000;
}

void bootLogStampTime(uint32_t epoch) {
  if (_stamped || !_loaded || _store.count == 0 || epoch == 0) return;

  BootLogEntry &e = _store.entries[_newestIndex()];
  if (e.epoch != 0) {
    _stamped = true;
    return;
  }

  e.epoch = epoch;
  _stamped = true;
  _persist();
  Serial.printf("[BootLog] Boot #%lu stamped at epoch %lu.\n",
                (unsigned long)e.seq, (unsigned long)epoch);
}

uint8_t bootLogCount() { return _store.count; }

BootLogEntry bootLogLast() {
  if (_store.count == 0) return BootLogEntry{};
  return _store.entries[_newestIndex()];
}

#ifdef UNIT_TEST
void bootLogMockReset() {
  _resetStore();
  _loaded = false;
  _stamped = false;
  _rtcMagic = 0;
  _rtcUptimeS = 0;
  _tickAnchorMs = 0;
}
#endif

String bootLogGetJSON() {
  String json;
  json.reserve(96 + (size_t)_store.count * 96);
  json += "{\"count\":";
  json += String(_store.count);
  json += ",\"boots\":[";

  // Newest first: the reason you open this page is the reset that just
  // happened, not the one from a fortnight ago.
  for (uint8_t i = 0; i < _store.count; i++) {
    const uint8_t idx =
        (uint8_t)((_store.head + BOOT_LOG_MAX - 1 - i) % BOOT_LOG_MAX);
    const BootLogEntry &e = _store.entries[idx];

    if (i > 0) json += ",";
    json += "{\"seq\":";
    json += String(e.seq);
    json += ",\"reason\":\"";
    json += bootResetReasonName(e.reason);
    json += "\",\"epoch\":";
    json += String(e.epoch);
    json += ",\"prevUptimeS\":";
    // JSON has no sentinel, and 4294967295 in a "seconds" field reads as a
    // real number to anything consuming this. null says "not recorded".
    json += (e.prevUptimeS == BOOT_UPTIME_UNKNOWN) ? "null"
                                                   : String(e.prevUptimeS);
    json += "}";
  }

  json += "]}";
  return json;
}
