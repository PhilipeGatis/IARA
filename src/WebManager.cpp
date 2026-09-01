#include "nvs_flash.h"
#include "WebManager.h"
#include "TpaPlan.h"
#include "PumpLog.h"
#include "FertManager.h"
#include "NotifyManager.h"
#include "SafetyWatchdog.h"
#include "TimeManager.h"
#include "WaterManager.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <Update.h>

#ifdef USE_WEBSERVER
#include <WiFi.h>
#endif

static Preferences _prefs;

// ============================================================================
// CONSTRUCTOR
// ============================================================================

WebManager::WebManager()
#ifdef USE_WEBSERVER
    : _server(80), _events("/events"),
#else
    :
#endif
      _time(nullptr), _water(nullptr), _fert(nullptr), _safety(nullptr),
      _tpaInterval(7), _tpaAutoEnabled(false), _tpaHour(10), _tpaMinute(0), _tpaLastRun(0),
      _tpaPercent(20), _canisterSafePct(0),
      _feedPauseMin(DEFAULT_FEED_PAUSE_MIN), _language(0),
      _primeML(DEFAULT_PRIME_ML), _aqHeight(0), _aqLength(0), _aqWidth(0),
      _sensorFullDistanceMm(0),
      _ultrasonicMinMm((uint16_t)(ULTRASONIC_MIN_DISTANCE_DEFAULT_CM * 10)),
      _drainFlowRate(0), _refillFlowRate(0),
      _primeEnabled(true), _reservoirMechFloat(false), _resFillTimeoutMin(40),
      _reservoirVolume(0), _reservoirSafetyML(0), _lastTelemetryMs(0),
      _lastSSEMs(0), _lastSSECleanupMs(0), _rebootPending(false), _rebootMs(0) {
}

// ============================================================================
// BEGIN
// ============================================================================

void WebManager::syncAquariumGeometryToWater() {
  if (!_water)
    return;

  const float lPerCm = getLitersPerCm();
  if (lPerCm <= 0)
    return; // dimensions not set yet

  _water->setLitersPerCm(lPerCm);

  const float effH = (float)getAquariumVolume() / lPerCm;
  _water->setAqEffectiveHeightCm(effH);
  // The offset matters: without it, a "safe level" of 80% resolves to a
  // distance above the calibrated full mark, which the tank only reaches by
  // overflowing — so the canister could never be allowed on.
  const float fullCm = (float)_sensorFullDistanceMm / 10.0f;
  _water->setSensorFullCm(fullCm);
  _water->setCanisterSafeLevelCm(
      distanceForLevelPercent(getCanisterSafePct(), fullCm, effH));
}

void WebManager::syncFlowRatesFromWater() {
  if (!_water)
    return;

  // WaterManager measures the flow; WebManager holds the copy that
  // isTpaConfigReady() and /api/status read. Whichever side has a value wins,
  // so this also migrates an older install where only WebManager had one.
  if (_water->getDrainFlowLPM() > 0) {
    _drainFlowRate = _water->getDrainFlowLPM() * LPM_TO_ML_PER_SEC;
  } else if (_drainFlowRate > 0) {
    _water->setDrainFlowLPM(_drainFlowRate * ML_PER_SEC_TO_LPM);
  }

  if (_water->getRefillFlowLPM() > 0) {
    _refillFlowRate = _water->getRefillFlowLPM() * LPM_TO_ML_PER_SEC;
  } else if (_refillFlowRate > 0) {
    _water->setRefillFlowLPM(_refillFlowRate * ML_PER_SEC_TO_LPM);
  }

  _saveParams();
}

void WebManager::begin(TimeManager *time, WaterManager *water,
                       FertManager *fert, SafetyWatchdog *safety,
                       NotifyManager *notify) {
  _time = time;
  _water = water;
  _fert = fert;
  _safety = safety;
  _notify = notify;

#ifndef UNIT_TEST
  _statusMutex = xSemaphoreCreateMutex();
#endif

  _loadParams();

  if (_water) {
    _water->setPrimeML(_primeML);
    _water->setPrimeEnabled(_primeEnabled);
    _water->setReservoirHasMechanicalFloat(_reservoirMechFloat);
    _water->setTimeoutReservoirFillMs((unsigned long)_resFillTimeoutMin * 60000UL);

    syncFlowRatesFromWater();
    syncAquariumGeometryToWater();

    Serial.println("[Config] ====== WebManager <-> WaterManager SYNC ======");
    Serial.printf("[Config]   WaterMgr drain: %.2f LPM, refill: %.2f LPM\n",
      _water->getDrainFlowLPM(), _water->getRefillFlowLPM());
    Serial.printf("[Config]   WebMgr   drain: %.2f mL/s, refill: %.2f mL/s\n",
      _drainFlowRate, _refillFlowRate);
    Serial.printf("[Config]   Prime: %.1f mL, enabled=%s\n",
      _primeML, _primeEnabled ? "YES" : "NO");
  }

#ifdef USE_WEBSERVER
  _setupRoutes();
  _server.begin();
  String ipStr = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString()
                                               : WiFi.softAPIP().toString();
  Serial.println("[Web] Dashboard at http://" + ipStr);
#else
  Serial.println("[Web] Web server disabled.");
#endif

  _printHelp();
  Serial.printf("[Web] TPA Schedule: Every %d days at %02d:%02d\n",
                _tpaInterval, _tpaHour, _tpaMinute);
}

void WebManager::setTpaLastRun(uint32_t epoch) {
  _tpaLastRun = epoch;
  _saveParams();
}

// ============================================================================
// UPDATE (call from loop)
// ============================================================================

void WebManager::update() {
#ifdef USE_WEBSERVER
  unsigned long now = millis();
  if (_rebootPending && (now - _rebootMs > 2000)) {
    Serial.println("[Web] Rebooting now...");
    ESP.restart();
  }

  // Handle non-blocking fert calibration pulse
  if (_calibratingFertChannel >= 0 && (now - _calibrationStartMs >= CALIBRATION_PULSE_MS)) {
    if (_fert) {
      _fert->manualPump(_calibratingFertChannel, false);
    }
    _calibratingFertChannel = -1;
    Serial.println("[Web] Fert calibration pulse finished.");
  }

  // Periodic heap and SSE diagnostics every 30s
  if ((now - _lastSSECleanupMs) >= 30000) {
    _lastSSECleanupMs = now;
    uint32_t freeHeap = ESP.getFreeHeap();
    size_t clients = _events.count();

    Serial.printf("[HEAP] free=%u min=%u maxBlock=%u sse=%u%s\n", freeHeap,
                  ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(),
                  (unsigned)clients, freeHeap < 20000 ? "  <-- LOW" : "");

    // Heap that stays on the floor for five straight minutes does not come
    // back: the SSE sends are already being skipped and new connections fail,
    // so the board looks dead from the network while it keeps dosing happily.
    // Reboot, but never across a running pump — an interrupted drain or refill
    // puts water on the floor, and the schedule survives a restart.
    if (freeHeap < 12000) {
      _lowHeapStreak++;
      if (_lowHeapStreak >= 10 && !_rebootPending &&
          (!_water || !_water->isRunning()) &&
          (!_fert || !_fert->isDosing()) &&
          (!_safety || (!_safety->isMaintenanceMode() && !_safety->isEmergency()))) {
        Serial.println("[HEAP] Exhausted for 5 min with nothing running — rebooting.");
        Serial.flush();
        ESP.restart();
      }
    } else {
      _lowHeapStreak = 0;
    }
  }

  // The payload is rebuilt here, on the loop task, and published for the async
  // server to copy. Building it walks every manager, and the async task pays
  // for that with the whole web server: a callback that sits for five seconds
  // trips the task watchdog on async_tcp and reboots the board. That is exactly
  // what an SSE client connecting used to do while the radio was thrashing.
  if ((now - _statusJSONMs) >= 1000 || _statusJSONMs == 0) {
    _refreshStatusCache();
  }

  // Send SSE telemetry every 3 seconds to reduce network congestion
  // The send() call itself cleans up disconnected clients internally.
  // Skip if heap is critically low to prevent crash.
  if ((now - _lastSSEMs) >= 3000 && _events.count() > 0) {
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap > 15000) {  // Guard: need ~1.5KB for JSON + overhead
      _lastSSEMs = now;
      String json;
      if (_copyStatusCache(json)) {
        _events.send(json.c_str(), "status", millis());
      }
    } else {
      Serial.println("[SSE] Skipped send — heap too low");
    }
  }
#endif
  _updateTelemetry();
}

// ============================================================================
// STATUS JSON
// ============================================================================

void WebManager::_refreshStatusCache() {
  String fresh = _buildStatusJSON();
#ifndef UNIT_TEST
  if (_statusMutex && xSemaphoreTake(_statusMutex, pdMS_TO_TICKS(50)) != pdTRUE)
    return; // a reader is mid-copy; the next tick publishes this anyway
#endif
  _statusJSON = fresh;
  _statusJSONMs = millis();
#ifndef UNIT_TEST
  if (_statusMutex) xSemaphoreGive(_statusMutex);
#endif
}

bool WebManager::_copyStatusCache(String &out) {
#ifndef UNIT_TEST
  if (_statusMutex && xSemaphoreTake(_statusMutex, pdMS_TO_TICKS(20)) != pdTRUE)
    return false;
#endif
  const bool ready = _statusJSON.length() > 0;
  if (ready) out = _statusJSON;
#ifndef UNIT_TEST
  if (_statusMutex) xSemaphoreGive(_statusMutex);
#endif
  return ready;
}

String WebManager::_buildStatusJSON() {
  String json;
  // Sized past what the payload actually measures. usDiag and the network
  // health block pushed it well over the old 1200, and every byte past the
  // reservation is a reallocation on a string the SSE tick rebuilds every
  // three seconds — the cheapest way there is to fragment the heap.
  json.reserve(1800);
  json += "{";

  // WiFi Connection Status
  json += "\"wifiConnected\":" +
          String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";

  // Boot diagnostics
  extern const char *bootResetReason;
  json += "\"resetReason\":\"" + String(bootResetReason) + "\",";
  json += "\"uptimeMs\":" + String(millis()) + ",";

  // Network health. Without these the only symptom of a dropped station or a
  // leaking socket pool is the dashboard going silent, which says nothing
  // about why.
  extern volatile int lastWifiDisconnectReason;
  extern unsigned long wifiDownSinceMs;
  json += "\"wifiDisconnectReason\":" + String(lastWifiDisconnectReason) + ",";
  json += "\"wifiDownMs\":" +
          String(wifiDownSinceMs ? (millis() - wifiDownSinceMs) : 0) + ",";
  json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
#ifdef USE_WEBSERVER
  json += "\"sseClients\":" + String(_events.count()) + ",";
#endif
  json += "\"firmwareVersion\":\"" + String(FIRMWARE_VERSION) + "\",";
  if (_time) {
    json += "\"time\":\"" + _time->getFormattedTime() + "\",";
    json += "\"rtcConnected\":" + String(_time->isRtcConnected() ? "true" : "false") + ",";
    json += "\"rtcLostPower\":" + String(_time->hasRtcLostPower() ? "true" : "false") + ",";
    // Scheduling is suspended while this is false, and nothing else about the
    // dashboard would look any different.
    json += "\"timeValid\":" + String(_time->isTimeValid() ? "true" : "false") + ",";
  }
  if (_safety) {
    json += "\"waterLevel\":" + String(_safety->getLastDistance(), 1) + ",";
    json +=
        "\"float\":" + String(_safety->isReservoirFull() ? "true" : "false") +
        ",";
    json +=
        "\"emergency\":" + String(_safety->isEmergency() ? "true" : "false") +
        ",";
    json += "\"maintenance\":" +
            String(_safety->isMaintenanceMode() ? "true" : "false") + ",";
  }
  if (_water) {
    json += "\"tpaState\":\"" + String(_water->getStateName()) + "\",";
    json +=
        "\"canister\":" + String(_water->isCanisterOn() ? "true" : "false") +
        ",";
  }

  // Schedule
  json += "\"tpaInterval\":" + String(_tpaInterval) + ",";
  json += "\"tpaAutoEnabled\":" + String(_tpaAutoEnabled ? "true" : "false") + ",";
  json += "\"tpaHour\":" + String(_tpaHour) + ",";
  json += "\"tpaMinute\":" + String(_tpaMinute) + ",";
  json += "\"tpaPercent\":" + String(_tpaPercent) + ",";
  json += "\"canisterSafePct\":" + String(_canisterSafePct) + ",";
  json += "\"feedPauseMin\":" + String(_feedPauseMin) + ",";
  json += "\"feedingLeft\":" +
          String(_water ? _water->feedingSecondsLeft() : 0) + ",";
  json += "\"tpaLastRun\":" + String(_tpaLastRun) + ",";
  json += "\"primeML\":" + String(_primeML, 1) + ",";
  uint32_t aqVol = getAquariumVolume();
  float lPerCm = (float)_aqLength * _aqWidth / 1000.0;
  json += "\"aqHeight\":" + String(_aqHeight) + ",";
  json += "\"aqMarginMm\":" + String(_aqMarginMm) + ",";
  json += "\"aqLength\":" + String(_aqLength) + ",";
  json += "\"aqWidth\":" + String(_aqWidth) + ",";
  json += "\"sensorFullDistanceMm\":" + String(_sensorFullDistanceMm) + ",";
  json += "\"ultrasonicMinMm\":" + String(_ultrasonicMinMm) + ",";
  json += "\"aquariumVolume\":" + String(aqVol) + ",";
  json += "\"litersPerCm\":" + String(lPerCm, 2) + ",";
  json += "\"drainFlowRate\":" + String(_drainFlowRate, 2) + ",";
  json += "\"refillFlowRate\":" + String(_refillFlowRate, 2) + ",";
  json += "\"primeRatio\":" + String(_primeRatio, 5) + ",";
  json += "\"primeEnabled\":" + String(_primeEnabled ? "true" : "false") + ",";
  json += "\"reservoirMechFloat\":" + String(_reservoirMechFloat ? "true" : "false") + ",";
  json += "\"reservoirFillTimeoutMin\":" + String(_resFillTimeoutMin) + ",";
  json += "\"reservoirVolume\":" + String(_reservoirVolume) + ",";
  json += "\"reservoirSafetyML\":" + String(_reservoirSafetyML, 0) + ",";
  json += "\"tpaConfigReady\":";
  json += (isTpaConfigReady() ? "true" : "false");
  json += ",";
  // What the last trigger actually planned to move. The reservoir and the 50%
  // ceiling can both cap it well below aqVolume x tpaPercent, and until now
  // that happened silently — the UI showed the configured percentage and the
  // cycle delivered something else.
  json += "\"tpaPlannedLiters\":" + String(_tpaPlannedLiters, 2) + ",";
  // The state machine has always known why it stopped; the dashboard just never
  // asked. A cycle that failed showed "ERROR" and nothing else, leaving the one
  // question worth answering — which step, and what was it waiting for — only
  // in the serial log.
  if (_water) {
    String err = _water->getLastErrorMsg();
    err.replace("\\", "\\\\");
    err.replace("\"", "\\\"");
    json += "\"lastError\":\"" + err + "\",";
  }
  json += "\"tpaBlockedReason\":\"" + _tpaBlockedReason + "\",";
  // Without this the UI cannot tell a dead sensor from a live one: a stale
  // level and a current level look identical once they are both just numbers.
  json += "\"sensorsOk\":";
  json += (_safety && _safety->areSensorsConnected() ? "true" : "false");
  json += ",";
  // Climbing on a still tank means the sensor is seeing something other than
  // the water — a rippled surface, a wall, a hanging cable.
  json += "\"rejectedReadings\":" +
          String(_safety ? _safety->getRejectedReadings() : 0) + ",";
  // Why the sensor is quiet, not just that it is. Reading these over the
  // network beats a serial cable behind a tank full of water: zero bytes is a
  // dead wire, bytes without frames is noise or the wrong baud rate, frames
  // without accepted readings is a sensor that cannot find the surface.
  if (_safety) {
    json += "\"usDiag\":{";
    json += "\"bytes\":" + String(_safety->getUsBytes()) + ",";
    json += "\"garbage\":" + String(_safety->getUsGarbageBytes()) + ",";
    json += "\"frames\":" + String(_safety->getUsFrames()) + ",";
    json += "\"csFail\":" + String(_safety->getUsChecksumFails()) + ",";
    json += "\"rangeRej\":" + String(_safety->getUsRangeRejects()) + ",";
    json += "\"lastRaw\":" + String(_safety->getUsLastRaw(), 1) + ",";
    json += "\"pending\":" + String(_safety->getUsPending());
    json += "},";
  }
  json += "\"language\":" + String(_language) + ",";
  if (_water) {
    json += "\"pumpGoalLiters\":" + String(_water->getPumpGoalLiters(), 2) + ",";
    json += "\"pumpProgressLiters\":" + String(_water->getPumpProgressLiters(), 2) + ",";
    json += "\"pumpElapsedMs\":" + String(_water->getPumpElapsedMs()) + ",";
  }
  // Stocks
  json += "\"stocks\":[";
  if (_fert) {
    for (uint8_t i = 0; i < NUM_FERTS + 1; i++) {
      if (i > 0)
        json += ",";
      json += "{\"stock\":" + String(_fert->getStockML(i), 0) +
              ",\"cap\":" + String(_fert->getCapacityML(i), 0) + ",\"name\":\"" +
              _fert->getName(i) + "\"" + ",\"doses\":[" +
              String(_fert->getDoseML(i, 0), 1) + "," +
              String(_fert->getDoseML(i, 1), 1) + "," +
              String(_fert->getDoseML(i, 2), 1) + "," +
              String(_fert->getDoseML(i, 3), 1) + "," +
              String(_fert->getDoseML(i, 4), 1) + "," +
              String(_fert->getDoseML(i, 5), 1) + "," +
              String(_fert->getDoseML(i, 6), 1) + "]" + ",\"sH\":[";
      for (uint8_t d = 0; d < 7; d++) {
        if (d > 0)
          json += ",";
        json += String(_fert->getSchedHour(i, d));
      }
      json += "],\"sM\":[";
      for (uint8_t d = 0; d < 7; d++) {
        if (d > 0)
          json += ",";
        json += String(_fert->getSchedMinute(i, d));
      }
      json += "],\"fR\":" + String(_fert->getFlowRate(i), 2) +
              ",\"pwm\":" + String(_fert->getPWM(i)) +
              ",\"en\":" + String(_fert->isEnabled(i) ? "true" : "false") + "}";
    }
  }
  json += "]";

  // Notify status
  if (_notify) {
    json += ",\"notify\":" + _buildNotifyJSON();
  }

  // Low stock thresholds
  if (_fert) {
    json += ",\"lowStockThresholds\":[";
    for (uint8_t i = 0; i < NUM_FERTS + 1; i++) {
      if (i > 0)
        json += ",";
      json += String(_fert->getLowStockThreshold(i), 0);
    }
    json += "]";
  }

  json += "}";
  return json;
}

// ============================================================================
// WEB ROUTES
// ============================================================================

#ifdef USE_WEBSERVER
void WebManager::_setupRoutes() {
  // ---- Dashboard React App (LittleFS) ----
  _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("no-cache, no-store, must-revalidate");

  // ---- SSE Events ----
  _events.onConnect([this](AsyncEventSourceClient *client) {
    Serial.println("[Web] SSE client connected");
    // Nothing but a copy of what the loop already published. If it is not
    // ready, the client waits for the next 3 s tick rather than the server
    // task doing the work here.
    String json;
    if (_copyStatusCache(json)) {
      client->send(json.c_str(), "status", millis());
    }
  });
  _server.addHandler(&_events);

  // ---- GET /api/status ----
  _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
    String json;
    if (_copyStatusCache(json)) {
      request->send(200, "application/json", json);
    } else {
      // At most a second old normally; 503 only while the loop is mid-publish
      // or has not published yet, and the dashboard retries on its own.
      request->send(503, "application/json", "{\"error\":\"status not ready\"}");
    }
  });



  // ---- POST /api/tpa/start ----
  _server.on("/api/tpa/start", HTTP_POST,
             [this](AsyncWebServerRequest *request) {
               if (_rejectForgedRequest(request)) return;
               if (triggerTPA()) {
                 Serial.println("[Web] TPA started via dashboard");
                 request->send(200, "application/json", "{\"ok\":true}");
               } else {
                 request->send(400, "application/json", "{\"error\":\"Cannot start TPA\"}");
               }
             });

  // ---- POST /api/tpa/abort ----
  _server.on("/api/tpa/abort", HTTP_POST,
             [this](AsyncWebServerRequest *request) {
               if (_rejectForgedRequest(request)) return;
               if (_water)
                 _water->abortTPA();
               Serial.println("[Web] TPA aborted via dashboard");
               request->send(200, "application/json", "{\"ok\":true}");
             });

  // ---- POST /api/tpa/config (reservoir safety margin) ----
  _server.on(
      "/api/tpa/config", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body;
        if (!_collectBody(request, data, len, index, total, body))
          return;
        bool changed = false;

        float s = _extractFloat(body, "reservoirSafetyML");
        if (s >= 0) {
          _reservoirSafetyML = s;
          changed = true;
        }

        if (changed) {
          _saveParams();
          Serial.printf("[Web] Reservoir safety margin: %.0f mL\n",
                        _reservoirSafetyML);
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

// ---- POST /api/tpa/pump (Manual Drain/Refill/Solenoid Trigger) ----
  _server.on(
      "/api/tpa/pump", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body;
        if (!_collectBody(request, data, len, index, total, body))
          return;
        int st = _extractInt(body, "state");
        
        // Only block STARTING a pump if TPA is running. We must always allow STOPPING (st == 0)
        if (st == 1 && _water->getState() != TPAState::IDLE && _water->getState() != TPAState::COMPLETE && _water->getState() != TPAState::ERROR) {
          request->send(400, "application/json", "{\"error\":\"TPA is running\"}");
          return;
        }
        String pStr = _extractString(body, "pump");
        float liters = _extractFloat(body, "liters");
        if (liters < 0) liters = 0;

        if (st == 1) {
          if (pStr == "solenoid" && _water) {
            _water->startManualReservoirFill();
          } else if ((pStr == "drain" || pStr == "refill") && _water) {
            _water->startManualPump(pStr, liters);
          } else {
             // Fallback for direct toggle if needed
             if (pStr == "drain") pumpOn(PIN_DRAIN, PumpReason::MANUAL_PUMP);
             else if (pStr == "refill") pumpOn(PIN_REFILL, PumpReason::MANUAL_PUMP);
             else if (pStr == "solenoid") pumpOn(PIN_SOLENOID, PumpReason::MANUAL_SOLENOID);
          }
        } else {
          if (_water && _water->isRunning()) {
            _water->stopManual();
            // stopManual() is where a manual run's flow rate is measured.
            // Pull it across now, otherwise it stays invisible until reboot.
            syncFlowRatesFromWater();
          } else {
             if (pStr == "drain") pumpOff(PIN_DRAIN, PumpReason::MANUAL_PUMP);
             else if (pStr == "refill") pumpOff(PIN_REFILL, PumpReason::MANUAL_PUMP);
             else if (pStr == "solenoid") pumpOff(PIN_SOLENOID, PumpReason::MANUAL_SOLENOID);
          }
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- POST /api/tpa/calibrate-pump (timed run to measure flow rate) ----
  _server.on(
      "/api/tpa/calibrate-pump", HTTP_POST,
      [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body;
        if (!_collectBody(request, data, len, index, total, body))
          return;
        String pStr = _extractString(body, "pump");
        if (!_water || (pStr != "drain" && pStr != "refill")) {
          request->send(400, "application/json",
                        "{\"error\":\"pump must be drain or refill\"}");
          return;
        }
        if (_water->isRunning()) {
          request->send(400, "application/json", "{\"error\":\"TPA is running\"}");
          return;
        }
        _water->startPumpCalibration(pStr);
        // startPumpCalibration() refuses when it cannot measure. Reporting 200
        // regardless is how this failure stayed invisible: the dashboard saw
        // success while nothing ran.
        if (!_water->isRunning()) {
          String err = _water->getLastErrorMsg();
          if (err.length() == 0)
            err = "Calibration could not start";
          request->send(400, "application/json",
                        "{\"error\":\"" + err + "\"}");
          return;
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- POST /api/tpa/calibrate-pumps (drain then refill, net zero water) ----
  _server.on("/api/tpa/calibrate-pumps", HTTP_POST,
             [this](AsyncWebServerRequest *request) {
               if (_rejectForgedRequest(request)) return;
               if (!_water) {
                 request->send(500, "application/json",
                               "{\"error\":\"WaterManager not available\"}");
                 return;
               }
               if (_water->isRunning()) {
                 request->send(400, "application/json",
                               "{\"error\":\"TPA is running\"}");
                 return;
               }
               _water->startPairedCalibration();
               if (!_water->isRunning()) {
                 String err = _water->getLastErrorMsg();
                 if (err.length() == 0)
                   err = "Calibration could not start";
                 request->send(400, "application/json",
                               "{\"error\":\"" + err + "\"}");
                 return;
               }
               request->send(200, "application/json", "{\"ok\":true}");
             });

  // ---- POST /api/config/aquarium (JSON body) ----
  _server.on(
      "/api/config/aquarium", HTTP_POST, [](AsyncWebServerRequest *request) {},
      NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body;
        if (!_collectBody(request, data, len, index, total, body))
          return;
        bool changed = false;

        int h = _extractInt(body, "aqHeight");
        if (h > 0) {
          _aqHeight = h;
          changed = true;
        }
        int m = _extractInt(body, "aqMarginMm");
        if (m >= 0) {
          _aqMarginMm = m;
          changed = true;
        }
        int l = _extractInt(body, "aqLength");
        if (l > 0) {
          _aqLength = l;
          changed = true;
        }
        int w = _extractInt(body, "aqWidth");
        if (w > 0) {
          _aqWidth = w;
          changed = true;
        }
        float ratio = _extractFloat(body, "primeRatio");
        if (ratio >= 0) {
          if (ratio > PRIME_MAX_ML_PER_L) {
            Serial.printf("[Web] primeRatio %.4f mL/L capped to %.4f (%.0fx the "
                          "label rate)\n",
                          ratio, PRIME_MAX_ML_PER_L,
                          PRIME_MAX_ML_PER_L / PRIME_LABEL_ML_PER_L);
            ratio = PRIME_MAX_ML_PER_L;
          }
          _primeRatio = ratio;
          changed = true;
        }
        int pe = _extractInt(body, "primeEnabled");
        if (pe >= 0) {
          _primeEnabled = (pe == 1);
          if (_water) _water->setPrimeEnabled(_primeEnabled);
          changed = true;
        }
        int ft = _extractInt(body, "reservoirFillTimeoutMin");
        if (ft >= 1 && ft <= 120) {
          _resFillTimeoutMin = ft;
          if (_water)
            _water->setTimeoutReservoirFillMs((unsigned long)ft * 60000UL);
          changed = true;
        }
        int mf = _extractInt(body, "reservoirMechFloat");
        if (mf >= 0) {
          _reservoirMechFloat = (mf == 1);
          if (_water) _water->setReservoirHasMechanicalFloat(_reservoirMechFloat);
          changed = true;
        }
        int rv = _extractInt(body, "reservoirVolume");
        if (rv >= 0) {
          _reservoirVolume = rv;
          changed = true;
        }
        int mg = _extractInt(body, "sensorFullDistanceMm");
        if (mg >= 0) {
          _sensorFullDistanceMm = mg;
          changed = true;
        }
        // The blind zone belongs to the module, not the firmware. Bounds are
        // enforced here as well as in the setter: a zero would let ring-down
        // through as a level, and a huge one silently blinds the tank.
        int umm = _extractInt(body, "ultrasonicMinMm");
        if (umm >= (int)(ULTRASONIC_MIN_DISTANCE_FLOOR_CM * 10) &&
            umm <= (int)(ULTRASONIC_MIN_DISTANCE_CEIL_CM * 10)) {
          _ultrasonicMinMm = umm;
          if (_safety) _safety->setMinDistanceCm(getUltrasonicMinCm());
          changed = true;
        }
        // The config form posts this alongside the dimensions, which is where a
        // user would expect it to live. It was only ever parsed by
        // /api/tpa/schedule, so saving the form silently discarded it and the
        // canister dry-run guard stayed at whatever it was.
        int csp = _extractInt(body, "canisterSafePct");
        if (csp >= 0 && csp <= 100) {
          _canisterSafePct = csp;
          changed = true;
        }
        int fpm = _extractInt(body, "feedPauseMin");
        if (fpm > 0 && fpm <= MAX_FEED_PAUSE_MIN) {
          _feedPauseMin = fpm;
          changed = true;
        }

        if (changed) {
          // Auto-calculate primeML from reservoirVolume × ratio
          if (_reservoirVolume > 0 && _primeRatio > 0) {
            _primeML = _reservoirVolume * _primeRatio;
            if (_water)
              _water->setPrimeML(_primeML);
          }
          _saveParams();
          syncAquariumGeometryToWater();
          if (_safety) {
            _safety->setOverflowThresholdCm(getOverflowThresholdCm());
          }
          uint32_t vol = getAquariumVolume();
          Serial.printf(
              "[Web] Aquarium dims: %dx%dx%d cm, margin=%d mm, sensorFull=%d mm, vol=%lu L\n",
              _aqHeight, _aqLength, _aqWidth, _aqMarginMm, _sensorFullDistanceMm, vol);
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- POST /api/config/calibrate-sensor-full ----
  _server.on("/api/config/calibrate-sensor-full", HTTP_POST,
             [this](AsyncWebServerRequest *request) {
               if (_rejectForgedRequest(request)) return;
               if (_safety) {
                 const float dist = _safety->readUltrasonic();
                 // The A02YYUW's usable range; anything outside it is a bad
                 // frame, not a measurement. Storing one here is expensive:
                 // this reading defines where 100% is, so every level, every
                 // percentage and the overflow threshold inherit the error.
                 if (dist >= getUltrasonicMinCm() &&
                     dist <= ULTRASONIC_MAX_DISTANCE_CM) {
                   _sensorFullDistanceMm = (uint16_t)round(dist * 10.0f); // mm

                   // _prefs is opened and closed inside _loadParams/_saveParams,
                   // so the handle is closed here and putUShort() went nowhere:
                   // the calibration survived until the next reboot and no
                   // further. Go through _saveParams() like every other write.
                   _saveParams();

                   // Both of these are derived from the 100% mark. Without the
                   // push, overflow detection keeps comparing against the old
                   // reference and the canister safe level stays where it was.
                   if (_safety)
                     _safety->setOverflowThresholdCm(getOverflowThresholdCm());
                   syncAquariumGeometryToWater();

                   Serial.printf("[Web] Sensor 100%% calibrated to %d mm\n", _sensorFullDistanceMm);
                   request->send(200, "application/json", "{\"ok\":true}");
                   return;
                 }
                 Serial.printf("[Web] Sensor calibration rejected: %.1f cm out of range\n", dist);
               }
               request->send(500, "application/json", "{\"error\":\"Sensor error\"}");
             });

  // ---- POST /api/maintenance/toggle ----
  _server.on("/api/maintenance/toggle", HTTP_POST,
             [this](AsyncWebServerRequest *request) {
               if (_rejectForgedRequest(request)) return;
               if (_safety) {
                 if (_safety->isMaintenanceMode()) {
                   _safety->exitMaintenance();
                   Serial.println("[Web] Maintenance OFF");
                 } else {
                   _safety->enterMaintenance();
                   Serial.println("[Web] Maintenance ON");
                 }
               }
               request->send(200, "application/json", "{\"ok\":true}");
             });

  // ---- POST /api/canister/toggle ----
   _server.on("/api/canister/toggle", HTTP_POST,
             [this](AsyncWebServerRequest *request) {
               if (_rejectForgedRequest(request)) return;
               bool current = digitalRead(PIN_CANISTER) == LOW; // SSR LOW=ON
               if (current) {
                 pumpOn(PIN_CANISTER, PumpReason::MANUAL_PUMP); // HIGH = OFF
               } else {
                 pumpOff(PIN_CANISTER, PumpReason::MANUAL_PUMP); // LOW = ON
               }
               Serial.printf("[Web] Canister manually turned %s\n", current ? "OFF" : "ON");
               request->send(200, "application/json", "{\"ok\":true}");
             });

  // ---- POST /api/canister/feed ----
  // Toggles the feeding pause. One control rather than two: the button that
  // starts the pause is the same one that ends it early, so the dashboard
  // never has to guess which of the pair applies.
  _server.on("/api/canister/feed", HTTP_POST,
             [this](AsyncWebServerRequest *request) {
               if (_rejectForgedRequest(request)) return;
               if (_water) {
                 if (_water->isFeedingPause()) {
                   _water->endFeedingPause();
                 } else {
                   _water->startFeedingPause(_feedPauseMin);
                 }
               }
               request->send(200, "application/json",
                             String("{\"feedingLeft\":") +
                                 (_water ? _water->feedingSecondsLeft() : 0) +
                                 "}");
             });

  // ---- POST /api/emergency/stop ----
  // Toggles, mirroring the `emergency_stop` serial command. Without a way to
  // clear it from here the dashboard could enter an emergency and never leave
  // it, since main.cpp skips everything else while the flag is set.
  _server.on("/api/emergency/stop", HTTP_POST,
             [this](AsyncWebServerRequest *request) {
               if (_rejectForgedRequest(request)) return;
               if (_safety) {
                 if (_safety->isEmergency()) {
                   _safety->clearEmergency();
                   if (_water) {
                     _water->stopManual(); // back to IDLE
                     // The shutdown stopped the filter, and nothing owned
                     // restarting it. A later manual run will not: it only
                     // restores a filter *it* switched off, so an emergency
                     // left the canister stopped until the hourly sweep — an
                     // hour of no circulation nobody was told about.
                     _water->restoreCanisterIfSafe(PumpReason::MANUAL_PUMP);
                   }
                   Serial.println("[Web] Emergency CLEARED via dashboard.");
                 } else {
                   _safety->emergencyShutdown();
                   Serial.println("[Web] EMERGENCY STOP via dashboard!");
                 }
               }
               request->send(200, "application/json", "{\"ok\":true}");
             });

  // ---- GET /api/wifi/scan ----
  _server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    int n = WiFi.scanComplete();
    if (n == -2) {
      // Scan hasn't started yet, trigger it
      WiFi.scanNetworks(true); // async scan
      request->send(202, "application/json", "{\"status\":\"scanning\"}");
    } else if (n == -1) {
      // Still scanning
      request->send(202, "application/json", "{\"status\":\"scanning\"}");
    } else if (n == 0) {
      request->send(200, "application/json", "{\"networks\":[]}");
      WiFi.scanDelete();
    } else {
      String json = "{\"networks\":[";
      for (int i = 0; i < n; ++i) {
        if (i > 0)
          json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) +
                "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
      }
      json += "]}";
      request->send(200, "application/json", json);
      WiFi.scanDelete();
    }
  });

  // ---- POST /api/wifi (Form Data: ssid, pass) ----
  _server.on("/api/wifi", HTTP_POST, [this](AsyncWebServerRequest *request) {
    if (_rejectForgedRequest(request)) return;
    if (request->hasParam("ssid", true) && request->hasParam("pass", true)) {
      String ssid = request->getParam("ssid", true)->value();
      String pass = request->getParam("pass", true)->value();

      Preferences pref;
      pref.begin("wifi", false);
      pref.putString("ssid", ssid);
      pref.putString("pass", pass);
      pref.end();

      Serial.println(
          "[Web] WiFi credentials updated via dashboard. Restarting...");
      request->send(200, "application/json", "{\"ok\":true}");

      // Give the server time to send the response before rebooting
      _rebootPending = true;
      _rebootMs = millis();
    } else {
      request->send(400, "application/json", "{\"error\":\"Missing params\"}");
    }
  });

  // ---- POST /api/schedule (JSON body - only TPA now) ----
  _server.on(
      "/api/schedule", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body;
        if (!_collectBody(request, data, len, index, total, body))
          return;
        bool changed = false;

        int inv = _extractInt(body, "tpaInterval");
        if (inv >= 0) {
          _tpaInterval = inv;
          changed = true;
        }
        int tae = _extractInt(body, "tpaAutoEnabled");
        if (tae >= 0) {
          _tpaAutoEnabled = (tae == 1);
          changed = true;
        }
        int hh = _extractInt(body, "tpaHour");
        if (hh >= 0 && hh <= 23) {
          _tpaHour = hh;
          changed = true;
        }
        int mm = _extractInt(body, "tpaMinute");
        if (mm >= 0 && mm <= 59) {
          _tpaMinute = mm;
          changed = true;
        }
        int pct = _extractInt(body, "tpaPercent");
        if (pct > 0 && pct <= 100) {
          _tpaPercent = pct;
          changed = true;
        }

        // Resetting the schedule by hand. Zero is what main.cpp reads as
        // "never ran", so it makes the next hour:minute match due; any other
        // epoch restarts the interval from that moment, which is how a water
        // change done by hand gets counted. A failed cycle deliberately leaves
        // this alone, and that is exactly when the stored date needs fixing.
        int lastRun = _extractInt(body, "tpaLastRun");
        if (lastRun >= 0) {
          _tpaLastRun = (uint32_t)lastRun;
          changed = true;
        }
        int csp = _extractInt(body, "canisterSafePct");
        if (csp >= 0 && csp <= 100) {
          _canisterSafePct = csp;
          // canisterSafeLevelCm is derived from this percentage, so WaterManager
          // keeps using the old one until the geometry is pushed down again.
          syncAquariumGeometryToWater();
          changed = true;
        }
        int fpm = _extractInt(body, "feedPauseMin");
        if (fpm > 0 && fpm <= MAX_FEED_PAUSE_MIN) {
          _feedPauseMin = fpm;
          changed = true;
        }
        int lang = _extractInt(body, "language");
        if (lang >= 0 && lang < 3) {
          _language = lang;
          if (_notify)
            _notify->setLanguage(lang);
          changed = true;
        }

        if (changed) {
          _saveParams();
          Serial.printf(
              "[Web] TPA Schedule updated: Every %d days at %02d:%02d\n",
              _tpaInterval, _tpaHour, _tpaMinute);
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- Individual Fert Schedule ----
  _server.on(
      "/api/fert/schedule", HTTP_POST, [](AsyncWebServerRequest *request) {},
      NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body;
        if (!_collectBody(request, data, len, index, total, body))
          return;
        int ch = _extractInt(body, "channel");
        float doses[7] = {0};
        bool hasDoses = _extractFloatArray(body, "doses", doses, 7);

        // Per-day times (new format: hours[] and minutes[] arrays)
        float hours[7] = {-1, -1, -1, -1, -1, -1, -1};
        float minutes[7] = {-1, -1, -1, -1, -1, -1, -1};
        bool hasHours = _extractFloatArray(body, "hours", hours, 7);
        bool hasMinutes = _extractFloatArray(body, "minutes", minutes, 7);

        // Backward compat: single hour/minute applies to all days
        int singleH = _extractInt(body, "hour");
        int singleM = _extractInt(body, "minute");

        if (ch >= 0 && ch <= 4 && hasDoses && _fert) {
          for (uint8_t d = 0; d < 7; d++) {
            _fert->setDoseML(ch, d, doses[d]);
          }

          if (hasHours && hasMinutes) {
            // Per-day times
            for (uint8_t d = 0; d < 7; d++) {
              int h = (int)hours[d];
              int m = (int)minutes[d];
              if (h >= 0 && h <= 23 && m >= 0 && m <= 59) {
                _fert->setScheduleTime(ch, d, h, m);
              }
            }
          } else if (singleH >= 0 && singleH <= 23 && singleM >= 0 &&
                     singleM <= 59) {
            // Legacy: apply same time to all days
            _fert->setScheduleTimeAll(ch, singleH, singleM);
          }

          _fert->saveState();
          Serial.printf("[Web] CH%d Schedule updated\n", ch + 1);
        }

        // Low stock threshold (optional)
        float lt = _extractFloat(body, "lowStockThreshold");
        if (lt >= 0 && ch >= 0 && ch <= 4 && _fert) {
          _fert->setLowStockThreshold(ch, lt);
        }

        // Bottle size (optional). Channels hold whatever the bottle on the
        // shelf holds — 450 mL here — and the stock bar reads as a percentage
        // of it, so a default of 500 shows a full bottle as nine tenths.
        float capML = _extractFloat(body, "capacityML");
        if (capML > 0 && ch >= 0 && ch <= 4 && _fert) {
          _fert->setCapacityML(ch, capML);
        }

        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- Pump Calibration: Toggle Prime ----
  _server.on(
      "/api/fert/pump", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body;
        if (!_collectBody(request, data, len, index, total, body))
          return;
        int ch = _extractInt(body, "channel");
        int st = _extractInt(body, "state");

        if (ch >= 0 && ch <= 4 && _fert) {
          _fert->manualPump(ch, st == 1);
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- Pump Calibration: Run 3 Seconds ----
  _server.on(
      "/api/fert/run3s", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body;
        if (!_collectBody(request, data, len, index, total, body))
          return;
        int ch = _extractInt(body, "channel");

        if (ch >= 0 && ch <= 4 && _fert) {
          // Block and pulse
          _fert->manualPump(ch, true);
          _calibratingFertChannel = ch;
          _calibrationStartMs = millis();
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- Pump Calibration: Save Flow Rate ----
  _server.on(
      "/api/fert/calibrate", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (_rejectForgedRequest(request)) return;
        if (request->_tempObject) {
          String body = *(String *)request->_tempObject;
          int ch = _extractInt(body, "channel");

          int startIdx = body.indexOf("\"ml\":");
          if (startIdx != -1 && ch >= 0 && ch <= 4 && _fert) {
            startIdx += 5;
            int endIdx = body.indexOf(",", startIdx);
            if (endIdx == -1)
              endIdx = body.indexOf("}", startIdx);
            if (endIdx != -1) {
              float measuredML = body.substring(startIdx, endIdx).toFloat();
              if (measuredML > 0.1f) {
                float newRate = measuredML / 3.0f; // 3 seconds baseline
                _fert->setFlowRate(ch, newRate);
                _fert->saveState();
                Serial.printf("[Web] CH%d flow rate calibrated to %.2f mL/s\n",
                              ch + 1, newRate);
              }
            }
          }
          request->send(200, "application/json", "{\"ok\":true}");
          delete (String *)request->_tempObject;
          request->_tempObject = NULL;
        } else {
          request->send(400, "application/json", "{\"error\":\"No body\"}");
        }
      },
      NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        if (!index) {
          request->_tempObject = new String();
        }
        String *body = (String *)request->_tempObject;
        body->concat((char *)data, len);
      });

  // Obsolete: /api/dose replaced by full /api/fert/schedule usage.

  // ---- POST /api/stock/reset (JSON body) ----
  _server.on(
      "/api/stock/reset", HTTP_POST, [](AsyncWebServerRequest *request) {},
      NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body;
        if (!_collectBody(request, data, len, index, total, body))
          return;
        int ch = _extractInt(body, "channel");
        float ml = _extractFloat(body, "ml");
        if (ch >= 0 && ch <= 4 && ml > 0 && _fert) {
          _fert->resetStock(ch, ml);
          Serial.printf("[Web] Stock CH%d reset to %.0f ml\n", ch + 1, ml);
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- POST /api/fert/name (JSON body: {"channel": 0, "name": "Potássio"})
  // ----
  _server.on(
      "/api/fert/name", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body;
        if (!_collectBody(request, data, len, index, total, body))
          return;
        int ch = _extractInt(body, "channel");
        String nameStr = _extractString(body, "name");

        if (ch >= 0 && ch <= 4 && nameStr.length() > 0 && _fert) {
          _fert->setName(ch, nameStr);
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- POST /api/fert/pwm (JSON body: {"channel": 0, "pwm": 255})
  _server.on(
      "/api/fert/pwm", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body;
        if (!_collectBody(request, data, len, index, total, body))
          return;
        int ch = _extractInt(body, "channel");
        int pwmValue = _extractInt(body, "pwm");

        if (ch >= 0 && ch <= 4 && pwmValue >= 0 && pwmValue <= 255 && _fert) {
          _fert->setPWM(ch, pwmValue);
          Serial.printf("[Web] CH%d PWM set to %d\n", ch + 1, pwmValue);
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- POST /api/fert/reset (JSON body: {"channel": 0}) ----
  // Wipes one channel back to factory defaults. Scoped to a single channel on
  // purpose: it is the escape hatch for a channel whose stored settings are
  // wrong, not a factory reset of the controller.
  _server.on(
      "/api/fert/reset", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body;
        if (!_collectBody(request, data, len, index, total, body))
          return;
        int ch = _extractInt(body, "channel");

        if (ch >= 0 && ch <= NUM_FERTS && _fert) {
          _fert->resetChannel(ch);
          Serial.printf("[Web] CH%d configuration reset via dashboard\n",
                        ch + 1);
          request->send(200, "application/json", "{\"ok\":true}");
        } else {
          request->send(400, "application/json",
                        "{\"error\":\"invalid channel\"}");
        }
      });

  // ---- POST /api/fert/enable (JSON body: {"channel": 0, "enabled": 1})
  _server.on(
      "/api/fert/enable", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body;
        if (!_collectBody(request, data, len, index, total, body))
          return;
        int ch = _extractInt(body, "channel");
        int enabled = _extractInt(body, "enabled");

        if (ch >= 0 && ch <= 4 && _fert) {
          _fert->setEnabled(ch, enabled > 0);
          Serial.printf("[Web] CH%d schedule set to %s\n", ch + 1, (enabled > 0) ? "ENABLED" : "DISABLED");
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- POST /api/fert/dose-now (JSON body: {"includeDosed": 0|1}) ----
  // Fires today's schedule ahead of its hour. Not a manual pump run: it is the
  // configured volume for today's day of week, it books stock, and it stamps
  // the day, so the automatic dose does not land on top of it later.
  //
  // includeDosed repeats channels that already ran today. It exists because the
  // common reason to reach for this button is a dose that ran and delivered
  // nothing, and refusing to repeat it would leave the only fix as editing the
  // schedule.
  _server.on(
      "/api/fert/dose-now", HTTP_POST, [](AsyncWebServerRequest *request) {},
      NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body;
        if (!_collectBody(request, data, len, index, total, body))
          return;
        if (!_fert) {
          request->send(500, "application/json",
                        "{\"error\":\"no fertiliser manager\"}");
          return;
        }
        // The dose is stamped against a date. Firing one while the clock is
        // untrusted would stamp the wrong day, and the real schedule would then
        // skip or repeat.
        if (!_time || !_time->isTimeValid()) {
          request->send(409, "application/json",
                        "{\"error\":\"clock not synced yet\"}");
          return;
        }
        const bool includeDosed = _extractInt(body, "includeDosed") == 1;
        const uint8_t queued = _fert->doseTodayNow(_time->now(), includeDosed);
        request->send(200, "application/json",
                      "{\"ok\":true,\"queued\":" + String(queued) + "}");
      });

  // ---- GET /api/pump/log ----
  _server.on(
      "/api/pump/log", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", pumpLogGetJSON());
      });

  // ---- GET /api/notify/status ----
  _server.on(
      "/api/notify/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!_notify) {
          request->send(200, "application/json", "{\"enabled\":false}");
          return;
        }
        request->send(200, "application/json", _buildNotifyJSON());
      });

  // ---- POST /api/notify/key ----
  _server.on(
      "/api/notify/key", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body;
        if (!_collectBody(request, data, len, index, total, body))
          return;
        String topic = _extractString(body, "topic");
        if (_notify) {
          _notify->setTopic(topic);
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- POST /api/notify/config ----
  _server.on(
      "/api/notify/config", HTTP_POST, [](AsyncWebServerRequest *request) {},
      NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        if (!_notify) {
          request->send(200, "application/json", "{\"ok\":true}");
          return;
        }
        String body;
        if (!_collectBody(request, data, len, index, total, body))
          return;

        // Per-type toggles
        const char *typeKeys[] = {"tpaComplete", "tpaError",     "fertLowStock",
                                  "emergency",   "fertComplete", "dailyLevel"};
        for (uint8_t i = 0; i < NOTIFY_TYPE_COUNT; i++) {
          int val = _extractInt(body, typeKeys[i]);
          if (val == 0 || val == 1) {
            _notify->setTypeEnabled((NotifyType)i, val == 1);
          }
        }

        // Daily report time
        int rH = _extractInt(body, "reportHour");
        int rM = _extractInt(body, "reportMinute");
        if (rH >= 0 && rH <= 23 && rM >= 0 && rM <= 59) {
          _notify->setDailyReportHour(rH, rM);
        }

        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- POST /api/notify/test ----
  _server.on("/api/notify/test", HTTP_POST,
             [this](AsyncWebServerRequest *request) {
               if (_rejectForgedRequest(request)) return;
               if (_notify)
                 _notify->sendTest();
               request->send(200, "application/json", "{\"ok\":true}");
              });

  // ---- POST /api/ota (Web OTA Firmware Update) ----
  _server.on(
      "/api/ota", HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        // This handler is executed after the upload finishes
        if (_otaForged) {
          _otaForged = false;
          return; // the upload handler already answered with 403
        }
        bool shouldReboot = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", shouldReboot ? "{\"ok\":true}" : "{\"error\":\"Update failed\"}");
        response->addHeader("Connection", "close");
        request->send(response);
        if (shouldReboot) {
          Serial.println("[OTA] Update Success! Scheduling reboot in 2s...");
          _rebootPending = true;
          _rebootMs = millis();
        }
      },
      [this](AsyncWebServerRequest *request, String filename, size_t index,
             uint8_t *data, size_t len, bool final) {
        // Checked on the first chunk, before Update.begin(): a multipart form
        // POST is exactly what a hostile page can forge without a preflight,
        // and this endpoint writes flash.
        if (!index && _rejectForgedRequest(request)) {
          _otaForged = true;
          return;
        }
        if (_otaForged) return;
        if (!index) {
          Serial.printf("[OTA] Update Start: %s\n", filename.c_str());
          // Start update. If it's littlefs.bin we should use U_SPIFFS, else U_FLASH
          int cmd = (filename.indexOf("littlefs") > -1) ? U_SPIFFS : U_FLASH;
          // For ESP32, U_SPIFFS correctly maps to the VFS partition (LittleFS/SPIFFS)
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
            Update.printError(Serial);
          }
        }
        if (!Update.hasError()) {
          if (Update.write(data, len) != len) {
            Update.printError(Serial);
          }
        }
        if (final) {
          if (Update.end(true)) {
            Serial.printf("[OTA] Update Finished: %u B\n", index + len);
          } else {
            Update.printError(Serial);
          }
        }
      });
}
#endif

// ============================================================================
// SIMPLE JSON EXTRACTORS (avoid ArduinoJson dependency)
// ============================================================================

/// Strips the scheme and any path from an Origin or Referer, leaving host:port.
static String _authorityOf(const String &url) {
  int start = url.indexOf("://");
  start = (start < 0) ? 0 : start + 3;
  int end = url.indexOf('/', start);
  return (end < 0) ? url.substring(start) : url.substring(start, end);
}

bool WebManager::_rejectForgedRequest(AsyncWebServerRequest *request) {
  // Preferred proof: the dashboard's own marker. A page on another site cannot
  // attach a custom header without a CORS preflight, and we answer none.
  if (request->hasHeader("X-IARA-Request"))
    return false;

  // Fallback: the browser's own account of where the request came from.
  //
  // Requiring only the custom header built a trap. Flashing firmware without
  // the matching UI left a dashboard that sends no header, and the one way to
  // upload the new UI is /api/ota — which the header requirement had just
  // locked. The device refused the only request that could fix it.
  //
  // Browsers send Origin on every cross-origin POST, form submissions included,
  // so checking it blocks the forgery just as well while letting an older
  // dashboard served from this device through.
  const String host = request->host();
  const char *sources[] = {"Origin", "Referer"};
  for (const char *name : sources) {
    if (!request->hasHeader(name))
      continue;
    const String authority = _authorityOf(request->header(name));
    if (authority.length() > 0 && authority == host)
      return false; // same origin — served by us
    Serial.printf("[Web] Refused %s %s: %s is %s, not %s\n",
                  request->methodToString(), request->url().c_str(), name,
                  authority.c_str(), host.c_str());
    request->send(403, "application/json",
                  "{\"error\":\"cross-origin request refused\"}");
    return true;
  }

  // Neither header present. A browser always sends Origin on a cross-origin
  // POST, so this is not a page acting on the owner's behalf — it is a direct
  // client such as curl. Those are outside what this guard can address: anyone
  // who can reach the device on the LAN can already craft any request. This is
  // not authentication and does not pretend to be.
  return false;
}

bool WebManager::_collectBody(AsyncWebServerRequest *request, uint8_t *data,
                              size_t len, size_t index, size_t total,
                              String &out) {
  // Every body handler funnels through here, so this is the one place that has
  // to hold for all of them.
  if (index == 0 && _rejectForgedRequest(request))
    return false;

  // Every body this server accepts is a small JSON object. Anything claiming to
  // be larger is either a bug or someone trying to exhaust the heap, and the
  // ESP32 has ~200 KB of it.
  static const size_t MAX_BODY_BYTES = 4096;
  if (total > MAX_BODY_BYTES || index + len > total) {
    _bodyBuf = String();
    _bodyOwner = nullptr;
    request->send(413, "application/json", "{\"error\":\"body too large\"}");
    return false;
  }

  if (index == 0 || _bodyOwner != request) {
    _bodyBuf = String();
    _bodyBuf.reserve(total + 1);
    _bodyOwner = request;
  }
  _bodyBuf.concat((const char *)data, len);

  if (index + len < total)
    return false; // more chunks still coming

  out = _bodyBuf;
  _bodyBuf = String();
  _bodyOwner = nullptr;
  return true;
}

int WebManager::_extractInt(const String &json, const char *key) {
  String search = String("\"") + key + "\":";
  int idx = json.indexOf(search);
  if (idx < 0)
    return -1;
  idx += search.length();
  return json.substring(idx).toInt();
}

float WebManager::_extractFloat(const String &json, const char *key) {
  String search = String("\"") + key + "\":";
  int idx = json.indexOf(search);
  if (idx < 0)
    return -1;
  idx += search.length();
  return json.substring(idx).toFloat();
}

String WebManager::_extractString(const String &json, const char *key) {
  String search = String("\"") + key + "\":\"";
  int startIdx = json.indexOf(search);
  if (startIdx < 0)
    return "";
  startIdx += search.length();
  int endIdx = json.indexOf("\"", startIdx);
  if (endIdx < 0)
    return "";
  return json.substring(startIdx, endIdx);
}

bool WebManager::_extractFloatArray(const String &json, const char *key,
                                    float *outArray, uint8_t expectedSize) {
  String search = String("\"") + key + "\":[";
  int startIdx = json.indexOf(search);
  if (startIdx < 0)
    return false;
  startIdx += search.length();

  int endIdx = json.indexOf("]", startIdx);
  if (endIdx < 0)
    return false;

  String arrayStr = json.substring(startIdx, endIdx);
  int lastComma = 0;
  for (uint8_t i = 0; i < expectedSize; i++) {
    int nextComma = arrayStr.indexOf(",", lastComma);
    if (nextComma == -1) {
      outArray[i] = arrayStr.substring(lastComma).toFloat();
      break;
    } else {
      outArray[i] = arrayStr.substring(lastComma, nextComma).toFloat();
      lastComma = nextComma + 1;
    }
  }
  return true;
}

// ============================================================================
// TELEMETRY (Serial)
// ============================================================================

void WebManager::_updateTelemetry() {
  unsigned long now = millis();
  if ((now - _lastTelemetryMs) < TELEMETRY_INTERVAL_MS)
    return;
  _lastTelemetryMs = now;

  // DRY #4: Reuse _printStatus instead of duplicating the same output
  _printStatus();
}

// ============================================================================
// NOTIFY JSON BUILDER (DRY #5)
// ============================================================================

String WebManager::_buildNotifyJSON() const {
  if (!_notify) return "{}";
  String json = "{";
  json += "\"enabled\":" + String(_notify->isEnabled() ? "true" : "false") + ",";
  // Mask key for security
  String key = _notify->getTopic();
  if (key.length() > 4) {
    key = key.substring(0, 4) + "****";
  }
  json += "\"topic\":\"" + key + "\",";
  json += "\"dailyCount\":" + String(_notify->getDailyCount()) + ",";
  json += "\"reportHour\":" + String(_notify->getDailyReportHour()) + ",";
  json += "\"reportMinute\":" + String(_notify->getDailyReportMinute()) + ",";
  json += "\"types\":[";
  for (uint8_t i = 0; i < NOTIFY_TYPE_COUNT; i++) {
    if (i > 0) json += ",";
    json += _notify->isTypeEnabled((NotifyType)i) ? "true" : "false";
  }
  json += "]}";
  return json;
}

// ============================================================================
// ============================================================================

// ============================================================================
// NVS PERSISTENCE
// ============================================================================

void WebManager::_loadParams() {
  _prefs.begin("aqua", true);
  _tpaInterval = _prefs.getUShort("tpaInt", 7);
  _tpaAutoEnabled = _prefs.getBool("tpaEn", false);
  _tpaHour = _prefs.getUChar("tpaH", 10);
  _tpaMinute = _prefs.getUChar("tpaM", 0);
  _tpaLastRun = _prefs.getUInt("tpaRun", 0);
  _tpaPercent = _prefs.getUChar("tpaPct", 20);
  _canisterSafePct = _prefs.getUChar("canSf", 0);
  _feedPauseMin = _prefs.getUShort("feedMin", DEFAULT_FEED_PAUSE_MIN);
  _language = _prefs.getUChar("lang", 0);
  _primeML = _prefs.getFloat("tpaPr", DEFAULT_PRIME_ML);
  _aqHeight = _prefs.getUShort("aqH", 40);
  _aqLength = _prefs.getUShort("aqL", 60);
  _aqWidth = _prefs.getUShort("aqW", 30);
  _aqMarginMm = _prefs.getUShort("aqBord", 0);
  // 0 means "not calibrated yet". It must not default to a plausible-looking
  // distance: overflow detection is derived from it, and a placeholder value
  // makes a fresh board believe a normal water level is a flood.
  _sensorFullDistanceMm = _prefs.getUShort("aqMg", 0);
  _ultrasonicMinMm = _prefs.getUShort(
      "usMin", (uint16_t)(ULTRASONIC_MIN_DISTANCE_DEFAULT_CM * 10));
  _drainFlowRate = _prefs.getFloat("drFR", 0);
  _refillFlowRate = _prefs.getFloat("rfFR", 0);
  _primeRatio = _prefs.getFloat("pRat", PRIME_LABEL_ML_PER_L);
  _primeEnabled = _prefs.getBool("pEn", true);
  _reservoirMechFloat = _prefs.getBool("resMF", false);
  _resFillTimeoutMin = _prefs.getUShort("resFT", 40);
  _reservoirVolume = _prefs.getUShort("rVol", 0);
  _reservoirSafetyML = _prefs.getFloat("resSf", 0);
  _prefs.end();

  // Auto-calculate primeML from reservoirVolume × ratio if both are set
  if (_reservoirVolume > 0 && _primeRatio > 0) {
    _primeML = _reservoirVolume * _primeRatio;
  }

  if (_safety) {
    _safety->setOverflowThresholdCm(getOverflowThresholdCm());
    _safety->setMinDistanceCm(getUltrasonicMinCm());
  }

  Serial.println("[Config] ====== NVS LOADED (namespace: aqua) ======");
  Serial.printf("[Config]   TPA: interval=%dd, auto=%s, hour=%02d:%02d, pct=%d%%\n",
    _tpaInterval, _tpaAutoEnabled ? "ON" : "OFF", _tpaHour, _tpaMinute, _tpaPercent);
  Serial.printf("[Config]   Aquarium: %dx%dx%d cm, margin=%d mm, sensorFull=%d mm, blindZone=%d mm\n",
    _aqHeight, _aqLength, _aqWidth, _aqMarginMm, _sensorFullDistanceMm, _ultrasonicMinMm);
  Serial.printf("[Config]   Drain flow: %.2f mL/s, Refill flow: %.2f mL/s\n",
    _drainFlowRate, _refillFlowRate);
  Serial.printf("[Config]   Prime: %.1f mL, ratio=%.5f, enabled=%s\n",
    _primeML, _primeRatio, _primeEnabled ? "YES" : "NO");
  Serial.printf("[Config]   Reservoir: vol=%d mL, safetyML=%.1f\n",
    _reservoirVolume, _reservoirSafetyML);
  Serial.printf("[Config]   Canister safe: %d%%, lang=%d\n",
    _canisterSafePct, _language);
}

void WebManager::_saveParams() {
  _prefs.begin("aqua", false);
  _prefs.putUShort("tpaInt", _tpaInterval);
  _prefs.putBool("tpaEn", _tpaAutoEnabled);
  _prefs.putUChar("tpaH", _tpaHour);
  _prefs.putUChar("tpaM", _tpaMinute);
  _prefs.putUInt("tpaRun", _tpaLastRun);
  _prefs.putUChar("tpaPct", _tpaPercent);
  _prefs.putUChar("canSf", _canisterSafePct);
  _prefs.putUShort("feedMin", _feedPauseMin);
  _prefs.putUChar("lang", _language);
  _prefs.putFloat("tpaPr", _primeML);
  _prefs.putUShort("aqH", _aqHeight);
  _prefs.putUShort("aqL", _aqLength);
  _prefs.putUShort("aqW", _aqWidth);
  _prefs.putUShort("aqBord", _aqMarginMm);
  _prefs.putUShort("aqMg", _sensorFullDistanceMm);
  _prefs.putUShort("usMin", _ultrasonicMinMm);
  _prefs.putFloat("drFR", _drainFlowRate);
  _prefs.putFloat("rfFR", _refillFlowRate);
  _prefs.putFloat("pRat", _primeRatio);
  _prefs.putBool("pEn", _primeEnabled);
  _prefs.putBool("resMF", _reservoirMechFloat);
  _prefs.putUShort("resFT", _resFillTimeoutMin);
  _prefs.putUShort("rVol", _reservoirVolume);
  _prefs.putFloat("resSf", _reservoirSafetyML);
  _prefs.end();

  Serial.println("[Config] ====== NVS SAVED (namespace: aqua) ======");
  Serial.printf("[Config]   Drain flow: %.2f mL/s, Refill flow: %.2f mL/s\n",
    _drainFlowRate, _refillFlowRate);
  Serial.printf("[Config]   Prime: %.1f mL, enabled=%s\n",
    _primeML, _primeEnabled ? "YES" : "NO");
}

// ============================================================================
// SERIAL COMMANDS
// ============================================================================

void WebManager::processSerialCommands() {
  if (!Serial.available())
    return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0)
    return;

  if (cmd == "help" || cmd == "?") {
    _printHelp();
  } else if (cmd == "status") {
    _printStatus();
  } else if (cmd == "tpa") {
    Serial.println("[CMD] Starting TPA cycle...");
    if (_water)
      _water->startTPA();
  } else if (cmd == "abort") {
    Serial.println("[CMD] Aborting TPA...");
    if (_water)
      _water->abortTPA();
  } else if (cmd == "maint") {
    if (_safety) {
      if (_safety->isMaintenanceMode()) {
        _safety->exitMaintenance();
      } else {
        _safety->enterMaintenance();
      }
    }
  } else if (cmd.startsWith("fert_time ")) {
    Serial.println("[CMD] fert_time is obsolete. Use individual channel "
                   "scheduling via web.");
  } else if (cmd.startsWith("tpa_interval ")) {
    int inv = cmd.substring(13).toInt();
    if (inv >= 0) {
      _tpaInterval = inv;
      _saveParams();
      Serial.printf("[CMD] TPA schedule set to every %d days\n", inv);
    }
  } else if (cmd.startsWith("dose ")) {
    Serial.println(
        "[CMD] dose is obsolete. Use individual channel scheduling via web.");
  } else if (cmd.startsWith("reset_stock ")) {
    int ch = cmd.substring(12, 13).toInt();
    float ml = cmd.substring(14).toFloat();
    if (ch >= 1 && ch <= 5 && ml > 0) {
      _fert->resetStock(ch - 1, ml);
      Serial.printf("[CMD] Stock CH%d reset to %.0f ml\n", ch, ml);
    }
  } else if (cmd == "drain_target") {
    if (_safety) {
      float dist = _safety->readUltrasonic();
      Serial.printf("[CMD] Current ultrasonic: %.1f cm\n", dist);
    }
  } else if (cmd.startsWith("set_drain ")) {
    float cm = cmd.substring(10).toFloat();
    if (cm > 0 && _water) {
      _water->setDrainTargetCm(cm);
      Serial.printf("[CMD] Drain target set to %.1f cm\n", cm);
    }
  } else if (cmd.startsWith("set_refill ")) {
    float cm = cmd.substring(11).toFloat();
    if (cm > 0 && _water) {
      _water->setRefillTargetCm(cm);
      Serial.printf("[CMD] Refill target set to %.1f cm\n", cm);
    }
  } else if (cmd == "canister_on") {
    pumpOff(PIN_CANISTER, PumpReason::MANUAL_PUMP); // SSR: LOW = ON
    Serial.println("[CMD] Canister ON.");
  } else if (cmd == "canister_off") {
    pumpOn(PIN_CANISTER, PumpReason::MANUAL_PUMP); // SSR: HIGH = OFF
    Serial.println("[CMD] Canister OFF.");
  } else if (cmd == "emergency_stop") {
    if (_safety) {
      if (_safety->isEmergency()) {
        _safety->clearEmergency();
        if (_water) {
          _water->stopManual(); // Reset state to IDLE
          _water->restoreCanisterIfSafe(PumpReason::MANUAL_PUMP);
        }
      } else {
        _safety->emergencyShutdown();
      }
    }
  } else if (cmd.startsWith("notify_topic ")) {
    String topic = cmd.substring(13);
    topic.trim();
    if (_notify) {
      _notify->setTopic(topic);
      Serial.printf("[CMD] ntfy.sh topic %s.\n",
                    topic.length() > 0 ? "set" : "cleared");
    }
  } else if (cmd == "test_notify") {
    if (_notify) {
      _notify->sendTest();
    } else {
      Serial.println("[CMD] NotifyManager not available.");
    }
  } else if (cmd == "notify_config") {
    if (_notify) {
      Serial.println("--- Notification Config ---");
      Serial.printf("  Enabled: %s\n",
                    _notify->isEnabled() ? "YES" : "NO");
      Serial.printf("  ntfy.sh Topic: %s\n",
                    _notify->getTopic().length() > 0 ? _notify->getTopic().c_str() : "NOT SET");
      Serial.printf("  Daily report: %02d:%02d\n",
                    _notify->getDailyReportHour(),
                    _notify->getDailyReportMinute());
      Serial.printf("  Today's count: %d/%d\n", _notify->getDailyCount(), 20);
      const char *names[] = {"TPA OK",     "TPA Erro", "Estoque",
                             "Emergência", "Fert OK",  "Nível Diário"};
      for (uint8_t i = 0; i < NOTIFY_TYPE_COUNT; i++) {
        Serial.printf("  [%c] %s\n",
                      _notify->isTypeEnabled((NotifyType)i) ? 'X' : ' ',
                      names[i]);
      }
      Serial.println("---------------------------");
    }
  } else {
    Serial.printf("[CMD] Unknown: '%s'. Type 'help' for commands.\n",
                  cmd.c_str());
  }
}

// ============================================================================
// SERIAL UI
// ============================================================================

void WebManager::_printHelp() {
  Serial.println("\n=== SERIAL COMMANDS ===");
  Serial.println("  help          — This menu");
  Serial.println("  status        — System status");
  Serial.println("  tpa           — Start TPA");
  Serial.println("  abort         — Abort TPA");
  Serial.println("  maint         — Toggle maintenance mode");
  Serial.println("  tpa_interval N — Set TPA interval (days)");
  Serial.println("  reset_stock CH ML — Reset stock channel CH");
  Serial.println("  set_drain CM  — Set drain target");
  Serial.println("  set_refill CM — Set refill target");
  Serial.println("  canister_on/off — Canister relay");
  Serial.println("  emergency_stop — All outputs OFF");
  Serial.println("  notify_topic TOPIC — Set ntfy.sh topic");
  Serial.println("  test_notify   — Send test notification");
  Serial.println("  notify_config — Show notification config");
  Serial.println("========================\n");
}

void WebManager::_printStatus() {
  Serial.println("\n--- Telemetry ---");
  if (_time) {
    Serial.printf("  Time: %s\n", _time->getFormattedTime().c_str());
  }
  if (_safety) {
    Serial.printf("  Water Level: %.1f cm\n", _safety->getLastDistance());
    Serial.printf("  Float: %s\n",
                  _safety->isReservoirFull() ? "FULL" : "empty");
    Serial.printf("  Emergency: %s | Maintenance: %s\n",
                  _safety->isEmergency() ? "YES" : "no",
                  _safety->isMaintenanceMode() ? "YES" : "no");
  }
  if (_water) {
    Serial.printf("  TPA State: %s | Canister: %s\n", _water->getStateName(),
                  _water->isCanisterOn() ? "ON" : "OFF");
  }
  Serial.printf("  Schedule: TPA=Every %d days at %02d:%02d\n", _tpaInterval,
                _tpaHour, _tpaMinute);
  if (_fert) {
    for (uint8_t i = 0; i < NUM_FERTS; i++) {
      Serial.printf("  Fert CH%d: stock=%.0f ml\n", i + 1,
                    _fert->getStockML(i));
    }
    Serial.printf("  Prime: stock=%.0f ml\n", _fert->getStockML(NUM_FERTS));
  }
  Serial.println("-----------------");
}
bool WebManager::triggerTPA(bool manual) {
  if (!_water || !_safety) return false;
  if (!isTpaConfigReady()) {
    Serial.println("[Web] TPA config incomplete. Cannot trigger.");
    return false;
  }
  if (_water->isRunning()) {
    Serial.println("[Web] TPA already running.");
    return false;
  }

  const float currentLevel = _safety->readUltrasonic();
  const float fullCm = (float)_sensorFullDistanceMm / 10.0f;
  const float lPerCm = getLitersPerCm();
  const float aqVol = (float)getAquariumVolume();

  float requested = aqVol * getTpaPercent() / 100.0f;
  const float hardCap = aqVol * TPA_MAX_DRAIN_PCT / 100.0f;
  if (requested > hardCap) {
    requested = hardCap;
    Serial.printf("[Web] TPA capped to %.1f L (%.0f%% ceiling)\n", requested,
                  TPA_MAX_DRAIN_PCT);
  }

  const float resAvail =
      (float)getReservoirVolume() - getReservoirSafetyML() / 1000.0f;

  // Every setpoint below comes from planTPA(), which is a pure function and has
  // tests. This one needs the whole async web server to exist, which is why the
  // numbers deciding how much water leaves the aquarium went untested for so
  // long.
  const TpaPlan plan =
      planTPA(currentLevel, fullCm, requested, lPerCm, resAvail,
              getUltrasonicMinCm());
  if (!plan.ok) {
    Serial.printf("[Web] TPA refused: %s (level %.1f cm, mark %.1f cm)\n",
                  plan.refusal, currentLevel, fullCm);
    _tpaBlockedReason = plan.refusal;
    return false;
  }
  if (plan.refillLiters < requested - 0.05f) {
    Serial.printf("[Web] TPA capped to %.1f L (reservoir limit)\n",
                  plan.refillLiters);
  }
  _tpaBlockedReason = String();

  const float drainLiters = plan.drainLiters;
  const float refillLiters = plan.refillLiters;
  _tpaPlannedLiters = refillLiters;

  // Dose against the water actually being changed, not the reservoir's nominal
  // capacity. At a 20% change only about 13 of 18 L are drawn, so the old basis
  // over-dosed by the difference — and the residue carries its Prime into the
  // next cycle, which then gets a full dose again on top.
  if (_water && _primeRatio > 0 && refillLiters > 0) {
    const float cycleML = refillLiters * _primeRatio;
    _water->setPrimeML(cycleML);
    Serial.printf("[Web] Prime for this cycle: %.2f mL for %.1f L (%.4f mL/L)\n",
                  cycleML, refillLiters, _primeRatio);
  }

  _water->setDrainTargetCm(plan.drainTargetCm);
  // Absolute, not "wherever the level happened to be". This is what makes the
  // cycle end full instead of banking whatever was missing when it started.
  _water->setRefillTargetCm(plan.refillTargetCm);
  _water->setLitersPerCm(lPerCm);

  const float effH = aqVol / lPerCm;
  _water->setSensorFullCm(fullCm);
  _water->setCanisterSafeLevelCm(
      distanceForLevelPercent(getCanisterSafePct(), fullCm, effH));
  _water->setAqEffectiveHeightCm(effH);

  // A measured rate near zero makes this quotient enormous; assigning it to an
  // unsigned long is undefined behaviour, and the value that lands there is a
  // timeout that will never fire. Clamp before the conversion, not after.
  const float drainLPM = _water->getDrainFlowLPM();
  const float refillLPM = _water->getRefillFlowLPM();
  // The legs move different volumes now: the drain removes only what is left
  // after the shortfall, the refill puts back the whole change.
  if (drainLPM > 0)
    _water->setTimeoutDrainMs(_clampTimeoutMs(drainLiters / drainLPM));
  if (refillLPM > 0)
    _water->setTimeoutRefillMs(_clampTimeoutMs(refillLiters / refillLPM));

  _water->startTPA(manual);
  // _tpaLastRun is deliberately NOT stamped here. It gates the schedule, so
  // stamping it at the start spends the whole interval on an attempt: a cycle
  // that errors at 10:02 would not be retried for another seven days, and a
  // failure during the refill would leave the tank low for that entire week.
  // main.cpp stamps it on the COMPLETE transition instead, which leaves
  // isTPADay latched and lets the next day's scheduled minute retry.
  return true;
}

/// Converts a run time in minutes into a timeout, with margin and bounds.
unsigned long WebManager::_clampTimeoutMs(float minutes) {
  // 1.5x was too thin and a cycle failed by seven seconds. The estimate divides
  // by a measured flow rate, and that measurement is taken over a couple of
  // centimetres of a rippling surface — it reads high as often as low. A margin
  // has to absorb that error, not merely round it.
  constexpr float MARGIN = 2.0f;

  // Flat allowance on top. Pump spin-up, the canister settle and the median
  // filter's own lag cost the same handful of seconds whatever the volume, so
  // scaling the whole budget starves a short leg. Subtracting the tank's
  // shortfall from the drain made those legs much shorter, which is what
  // exposed this.
  constexpr float OVERHEAD_MINUTES = 1.0f;

  constexpr float MAX_MINUTES = 60.0f;
  if (!(minutes > 0))
    return TIMEOUT_DRAIN_MS; // fall back to the compiled-in default
  float budget = minutes * MARGIN + OVERHEAD_MINUTES;
  if (budget > MAX_MINUTES)
    budget = MAX_MINUTES;
  return (unsigned long)(budget * 60000.0f);
}
