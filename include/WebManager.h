#pragma once

#include "Config.h"
#include <Arduino.h>

// Forward declarations
class TimeManager;
class WaterManager;
class FertManager;
class SafetyWatchdog;
class NotifyManager;

#ifdef USE_WEBSERVER
#include <ESPAsyncWebServer.h>
#endif

/// @brief Manages embedded web dashboard, REST API, and serial commands.
class WebManager {
public:
  WebManager();

  /// Initialize web server and serial UI
  void begin(TimeManager *time, WaterManager *water, FertManager *fert,
             SafetyWatchdog *safety, NotifyManager *notify);

  /// Run web server + update telemetry (call from loop)
  void update();

  /// Trigger TPA manually from dashboard or schedule
  bool triggerTPA(bool manual = true);

  // ---- Schedule parameters (read by main loop) ----
  uint16_t getTpaInterval() const { return _tpaInterval; }
  bool getTpaAutoEnabled() const { return _tpaAutoEnabled; }
  uint8_t getTpaHour() const { return _tpaHour; }
  uint8_t getTpaMinute() const { return _tpaMinute; }
  uint32_t getTpaLastRun() const { return _tpaLastRun; }
  void setTpaLastRun(uint32_t epoch);
  uint8_t getTpaPercent() const { return _tpaPercent; }
  uint8_t getCanisterSafePct() const { return _canisterSafePct; }
  uint8_t getLanguage() const { return _language; }

  // ---- Aquarium config ----
  uint32_t getAquariumVolume() const {
    float effH = (float)_aqHeight - (_aqMarginMm / 10.0f);
    int32_t vol = (int32_t)(effH * _aqLength * _aqWidth) / 1000;
    return vol > 0 ? (uint32_t)vol : 0;
  }
  float getLitersPerCm() const { return (float)_aqLength * _aqWidth / 1000.0f; }
  uint16_t getAqHeight() const { return _aqHeight; }
  uint16_t getSensorFullDistanceMm() const { return _sensorFullDistanceMm; }
  /// Distance (cm) at or below which the water counts as overflowing.
  /// Returns 0 — meaning "no overflow detection" — until the level sensor has
  /// been calibrated. A freshly flashed board must never act on a placeholder
  /// threshold: that turns a healthy tank into an emergency drain.
  float getOverflowThresholdCm() const {
    if (_sensorFullDistanceMm == 0 || _aqHeight == 0)
      return 0.0f;
    const float sfCm = _sensorFullDistanceMm / 10.0f;
    const float toleranceCm = _aqHeight * (OVERFLOW_TOLERANCE_PCT / 100.0f);
    const float t = sfCm - toleranceCm;
    return t > 0.0f ? t : 0.0f;
  }
  float getReservoirSafetyML() const { return _reservoirSafetyML; }
  uint16_t getReservoirVolume() const { return _reservoirVolume; }
  /// Pull freshly measured flow rates out of WaterManager into this manager's
  /// copies. WaterManager measures them; WebManager is what isTpaConfigReady()
  /// and /api/status read, so without this a manual calibration is invisible
  /// until the next boot.
  void syncFlowRatesFromWater();

  /// Push the aquarium geometry into WaterManager. It used to happen only
  /// inside triggerTPA(), which left litersPerCm at zero until a water change
  /// actually started — and flow calibration needs it to convert a level change
  /// into litres. That made calibration impossible outside a TPA, and a TPA
  /// impossible without calibration.
  void syncAquariumGeometryToWater();

  bool isTpaConfigReady() const {
    return _aqHeight > 0 && _aqLength > 0 && _aqWidth > 0 &&
           _sensorFullDistanceMm > 0 && _drainFlowRate > 0 && _refillFlowRate > 0 &&
           _reservoirVolume > 0 && _reservoirSafetyML > 0 &&
           // Without this, canisterSafeLevelCm equals the full tank height and
           // restoreCanisterIfSafe() waves every level through — so a scheduled,
           // unattended TPA would run with the one guard that keeps the canister
           // from running dry switched off. The README already called it
           // mandatory; now it is.
           _canisterSafePct > 0;
  }
  bool getPrimeEnabled() const { return _primeEnabled; }
  bool getReservoirMechFloat() const { return _reservoirMechFloat; }

  /// Process serial commands (always active)
  void processSerialCommands();

private:
  // Manager pointers
  TimeManager *_time;
  WaterManager *_water;
  FertManager *_fert;
  SafetyWatchdog *_safety;
  NotifyManager *_notify;

  // Schedule parameters
  uint16_t _tpaInterval;
  bool _tpaAutoEnabled;
  uint8_t _tpaHour;
  uint8_t _tpaMinute;
  uint32_t _tpaLastRun;
  uint8_t _tpaPercent;      // % of aquarium volume to change
  uint8_t _canisterSafePct; // Min water level % for safe canister operation
  uint8_t _language;        // 0=PT, 1=EN, 2=JA

  float _primeML;

  // Aquarium dimensions (cm)
  uint16_t _aqHeight;        // Altura (cm)
  uint16_t _aqMarginMm;      // Margem do topo (mm)
  uint16_t _aqLength;        // Comprimento (cm)
  uint16_t _aqWidth;         // Largura (cm)
  uint16_t _sensorFullDistanceMm; // Distância do sensor até a água 100% cheia (mm)
  float _drainFlowRate;      // mL/s
  float _refillFlowRate;     // mL/s
  float _primeRatio;         // mL per liter (manufacturer ratio)
  bool _primeEnabled;
  bool _reservoirMechFloat;        // se true, o canal 5 faz Prime na TPA. se false, é um fertilizante genérico

  // ---- Reservoir config ----
  uint16_t _reservoirVolume; // reservoir liters
  float _reservoirSafetyML;  // min mL to keep in reservoir for pump

  // Telemetry timing
  unsigned long _lastTelemetryMs;
  unsigned long _lastSSEMs;
  unsigned long _lastSSECleanupMs;

  // Reboot flag
  bool _rebootPending = false;
  unsigned long _rebootMs = 0;

  // Calibration state
  int8_t _calibratingFertChannel = -1;
  uint32_t _calibrationStartMs = 0;

  // NVS persistence
  void _loadParams();
  void _saveParams();

  // Telemetry
  void _updateTelemetry();
  String _buildStatusJSON();

  // Serial UI
  void _printStatus();
  void _printHelp();

  // JSON helpers
  /// Rejects a mutating request that did not come from the dashboard.
  ///
  /// The controller has no authentication and sits on the home LAN. A browser
  /// on that LAN, visiting any page anywhere, can be made to POST at
  /// http://iara.local/api/tpa/start — a form submission or an <img> needs no
  /// permission from us, and the response being unreadable does not matter when
  /// the effect is starting a water change or writing flash.
  ///
  /// What a page cannot do is attach a custom header cross-origin without first
  /// obtaining permission through a CORS preflight, and this firmware answers no
  /// preflight. So requiring the header is enough to separate "the dashboard
  /// asked" from "something else asked on the owner's behalf".
  ///
  /// Returns true and answers with 403 when the request should not proceed.
  bool _rejectForgedRequest(AsyncWebServerRequest *request);

  /// Reassembles a POST body from the async body handler's chunks.
  ///
  /// Two hazards it exists to close. The `data` buffer is NOT NUL-terminated,
  /// so `String((char*)data)` reads past its end until it happens to find a
  /// zero: a heap overread that pulls whatever follows in memory into the JSON
  /// parse, and can fault outright. And a body arrives in TCP-sized chunks, so
  /// parsing the first one alone silently acts on a truncated request.
  ///
  /// Returns true (and fills `out`) only once the whole body has arrived.
  /// Answers oversized requests itself, so callers just return on false.
  bool _collectBody(AsyncWebServerRequest *request, uint8_t *data, size_t len,
                    size_t index, size_t total, String &out);

  static int _extractInt(const String &json, const char *key);
  static float _extractFloat(const String &json, const char *key);
  static String _extractString(const String &json, const char *key);
  static bool _extractFloatArray(const String &json, const char *key,
                                 float *outArray, uint8_t expectedSize);

  /// Build notify status JSON fragment (DRY #5)
  String _buildNotifyJSON() const;

  static unsigned long _clampTimeoutMs(float minutes);

  // Reassembly buffer for _collectBody(). One is enough: the async server runs
  // handlers on a single task, and _bodyOwner catches the case where a second
  // request interleaves anyway by discarding the abandoned partial body.
  // Set when an OTA upload was refused on its first chunk, so the remaining
  // chunks are discarded and the completion handler does not report success.
  bool _otaForged = false;

  String _bodyBuf;
  const AsyncWebServerRequest *_bodyOwner = nullptr;

  // Litres the last triggerTPA() actually planned to move, which is not
  // necessarily aqVolume x tpaPercent: the reservoir and the 50% ceiling both
  // cap it. Surfaced so a capped cycle is visible rather than silent.
  float _tpaPlannedLiters = 0;
  // Why the last trigger was refused, for the UI to show instead of nothing.
  String _tpaBlockedReason;

#ifdef USE_WEBSERVER
  AsyncWebServer _server;
  AsyncEventSource _events;
  void _setupRoutes();
#endif
};
