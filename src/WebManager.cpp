#include "WebManager.h"
#include "FertManager.h"
#include "NotifyManager.h"
#include "SafetyWatchdog.h"
#include "TimeManager.h"
#include "WaterManager.h"
#include <LittleFS.h>
#include <Preferences.h>

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
      _tpaInterval(7), _tpaHour(10), _tpaMinute(0), _tpaLastRun(0),
      _tpaPercent(20), _canisterSafePct(0), _primeML(DEFAULT_PRIME_ML),
      _aqHeight(0), _aqLength(0), _aqWidth(0), _aqMarginCm(0),
      _drainFlowRate(0), _refillFlowRate(0), _reservoirVolume(0),
      _reservoirSafetyML(0), _lastTelemetryMs(0), _lastSSEMs(0) {
}

// ============================================================================
// BEGIN
// ============================================================================

void WebManager::begin(TimeManager *time, WaterManager *water,
                       FertManager *fert, SafetyWatchdog *safety,
                       NotifyManager *notify) {
  _time = time;
  _water = water;
  _fert = fert;
  _safety = safety;
  _notify = notify;

  _loadParams();

  if (_water) {
    _water->setPrimeML(_primeML);
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
  // Send SSE telemetry every 3 seconds to reduce network congestion
  unsigned long now = millis();
  if ((now - _lastSSEMs) >= 3000 && _events.count() > 0) {
    _lastSSEMs = now;
    String json = _buildStatusJSON();
    _events.send(json.c_str(), "status", millis());
  }
#endif
  _updateTelemetry();
}

// ============================================================================
// STATUS JSON
// ============================================================================

String WebManager::_buildStatusJSON() {
  String json;
  json.reserve(1200); // Prevent heap fragmentation and speed up concatenation
  json += "{";

  // WiFi Connection Status
  json += "\"wifiConnected\":" +
          String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";

  if (_time) {
    json += "\"time\":\"" + _time->getFormattedTime() + "\",";
  }
  if (_safety) {
    json += "\"waterLevel\":" + String(_safety->getLastDistance(), 1) + ",";
    json +=
        "\"optical\":" + String(_safety->isOpticalHigh() ? "true" : "false") +
        ",";
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
  json += "\"tpaHour\":" + String(_tpaHour) + ",";
  json += "\"tpaMinute\":" + String(_tpaMinute) + ",";
  json += "\"tpaPercent\":" + String(_tpaPercent) + ",";
  json += "\"canisterSafePct\":" + String(_canisterSafePct) + ",";
  json += "\"primeMl\":" + String(_primeML, 1) + ",";
  uint32_t aqVol = (uint32_t)_aqHeight * _aqLength * _aqWidth / 1000;
  float lPerCm = (float)_aqLength * _aqWidth / 1000.0;
  json += "\"aqHeight\":" + String(_aqHeight) + ",";
  json += "\"aqLength\":" + String(_aqLength) + ",";
  json += "\"aqWidth\":" + String(_aqWidth) + ",";
  json += "\"aqMarginCm\":" + String(_aqMarginCm) + ",";
  json += "\"aquariumVolume\":" + String(aqVol) + ",";
  json += "\"litersPerCm\":" + String(lPerCm, 2) + ",";
  json += "\"drainFlowRate\":" + String(_drainFlowRate, 2) + ",";
  json += "\"refillFlowRate\":" + String(_refillFlowRate, 2) + ",";
  json += "\"primeRatio\":" + String(_primeRatio, 4) + ",";
  json += "\"reservoirVolume\":" + String(_reservoirVolume) + ",";
  json += "\"reservoirSafetyML\":" + String(_reservoirSafetyML, 0) + ",";
  json += "\"tpaConfigReady\":";
  json += (isTpaConfigReady() ? "true" : "false");
  json += ",";
  // Stocks
  json += "\"stocks\":[";
  if (_fert) {
    for (uint8_t i = 0; i < NUM_FERTS + 1; i++) {
      if (i > 0)
        json += ",";
      json += "{\"stock\":" + String(_fert->getStockML(i), 0) + ",\"name\":\"" +
              _fert->getName(i) + "\"" + ",\"doses\":[" +
              String(_fert->getDoseML(i, 0), 1) + "," +
              String(_fert->getDoseML(i, 1), 1) + "," +
              String(_fert->getDoseML(i, 2), 1) + "," +
              String(_fert->getDoseML(i, 3), 1) + "," +
              String(_fert->getDoseML(i, 4), 1) + "," +
              String(_fert->getDoseML(i, 5), 1) + "," +
              String(_fert->getDoseML(i, 6), 1) + "]" +
              ",\"sH\":" + String(_fert->getSchedHour(i)) +
              ",\"sM\":" + String(_fert->getSchedMinute(i)) +
              ",\"fR\":" + String(_fert->getFlowRate(i), 2) +
              ",\"pwm\":" + String(_fert->getPWM(i)) + "}";
    }
  }
  json += "]";

  // Notify status
  if (_notify) {
    json += ",\"notify\":{";
    json +=
        "\"enabled\":" + String(_notify->isEnabled() ? "true" : "false") + ",";
    json += "\"dailyCount\":" + String(_notify->getDailyCount()) + ",";
    json += "\"reportHour\":" + String(_notify->getDailyReportHour()) + ",";
    json += "\"reportMinute\":" + String(_notify->getDailyReportMinute()) + ",";
    json += "\"types\":[";
    for (uint8_t i = 0; i < NOTIFY_TYPE_COUNT; i++) {
      if (i > 0)
        json += ",";
      json += _notify->isTypeEnabled((NotifyType)i) ? "true" : "false";
    }
    json += "]}";
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
  _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // ---- SSE Events ----
  _events.onConnect([this](AsyncEventSourceClient *client) {
    Serial.println("[Web] SSE client connected");
    client->send(_buildStatusJSON().c_str(), "status", millis());
  });
  _server.addHandler(&_events);

  // ---- GET /api/status ----
  _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send(200, "application/json", _buildStatusJSON());
  });

  // ---- POST /api/tpa/start ----
  _server.on("/api/tpa/start", HTTP_POST,
             [this](AsyncWebServerRequest *request) {
               if (_water)
                 _water->startTPA();
               Serial.println("[Web] TPA started via dashboard");
               request->send(200, "application/json", "{\"ok\":true}");
             });

  // ---- POST /api/tpa/abort ----
  _server.on("/api/tpa/abort", HTTP_POST,
             [this](AsyncWebServerRequest *request) {
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
        String body = String((char *)data).substring(0, len);
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

  // ---- POST /api/tpa/pump (Manual Drain/Refill Trigger) ----
  _server.on(
      "/api/tpa/pump", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body = String((char *)data).substring(0, len);
        String pStr = _extractString(body, "pump");
        int st = _extractInt(body, "state");

        if (pStr == "drain") {
          digitalWrite(PIN_DRAIN, st == 1 ? HIGH : LOW);
        } else if (pStr == "refill") {
          digitalWrite(PIN_REFILL, st == 1 ? HIGH : LOW);
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- POST /api/config/aquarium (JSON body) ----
  _server.on(
      "/api/config/aquarium", HTTP_POST, [](AsyncWebServerRequest *request) {},
      NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body = String((char *)data).substring(0, len);
        bool changed = false;

        int h = _extractInt(body, "aqHeight");
        if (h > 0) {
          _aqHeight = h;
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
          _primeRatio = ratio;
          changed = true;
        }
        int rv = _extractInt(body, "reservoirVolume");
        if (rv >= 0) {
          _reservoirVolume = rv;
          changed = true;
        }
        int mg = _extractInt(body, "aqMarginCm");
        if (mg >= 0) {
          _aqMarginCm = mg;
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
          uint32_t vol = getAquariumVolume();
          Serial.printf(
              "[Web] Aquarium dims: %dx%dx%d cm (margin %d) = %lu L\n",
              _aqHeight, _aqLength, _aqWidth, _aqMarginCm, vol);
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- POST /api/tpa/run3s (JSON body: {"pump": "drain" | "refill"}) ----
  _server.on(
      "/api/tpa/run3s", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body = String((char *)data).substring(0, len);
        String pStr = _extractString(body, "pump");
        uint8_t pin = 0;
        if (pStr == "drain")
          pin = PIN_DRAIN;
        else if (pStr == "refill")
          pin = PIN_REFILL;

        if (pin > 0) {
          digitalWrite(pin, HIGH);
          unsigned long start = millis();
          while ((millis() - start) < 3000) {
            delay(10);
          }
          digitalWrite(pin, LOW);
          Serial.printf("[Web] %s pump ran for 3s\n", pStr.c_str());
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- POST /api/tpa/calibrate (JSON: {"pump":"drain"|"refill","ml":150}) --
  _server.on(
      "/api/tpa/calibrate", HTTP_POST, [](AsyncWebServerRequest *request) {},
      NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body = String((char *)data).substring(0, len);
        String pStr = _extractString(body, "pump");
        float ml = _extractFloat(body, "ml");
        if (ml > 0.1f) {
          float rate = ml / 3.0f;
          if (pStr == "drain") {
            _drainFlowRate = rate;
            Serial.printf("[Web] Drain flow rate calibrated: %.2f mL/s\n",
                          rate);
          } else if (pStr == "refill") {
            _refillFlowRate = rate;
            Serial.printf("[Web] Refill flow rate calibrated: %.2f mL/s\n",
                          rate);
          }
          _saveParams();
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- POST /api/maintenance/toggle ----
  _server.on("/api/maintenance/toggle", HTTP_POST,
             [this](AsyncWebServerRequest *request) {
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

  // ---- POST /api/emergency/stop ----
  _server.on("/api/emergency/stop", HTTP_POST,
             [this](AsyncWebServerRequest *request) {
               if (_safety)
                 _safety->emergencyShutdown();
               Serial.println("[Web] EMERGENCY STOP via dashboard!");
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
  _server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
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
      delay(500);
      ESP.restart();
    } else {
      request->send(400, "application/json", "{\"error\":\"Missing params\"}");
    }
  });

  // ---- POST /api/schedule (JSON body - only TPA now) ----
  _server.on(
      "/api/schedule", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body = String((char *)data).substring(0, len);
        bool changed = false;

        int inv = _extractInt(body, "tpaInterval");
        if (inv >= 0) {
          _tpaInterval = inv;
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
        int csp = _extractInt(body, "canisterSafePct");
        if (csp > 0 && csp <= 100) {
          _canisterSafePct = csp;
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
        String body = String((char *)data).substring(0, len);
        int ch = _extractInt(body, "channel");
        int h = _extractInt(body, "hour");
        int m = _extractInt(body, "minute");
        float doses[7] = {0};
        bool hasDoses = _extractFloatArray(body, "doses", doses, 7);

        if (ch >= 0 && ch <= 4 && h >= 0 && h <= 23 && m >= 0 && m <= 59 &&
            hasDoses && _fert) {
          _fert->setScheduleTime(ch, h, m);
          for (uint8_t d = 0; d < 7; d++) {
            _fert->setDoseML(ch, d, doses[d]);
          }
          _fert->saveState();
          Serial.printf("[Web] CH%d Schedule updated -> time: %02d:%02d\n",
                        ch + 1, h, m);
        }

        // Low stock threshold (optional)
        float lt = _extractFloat(body, "lowStockThreshold");
        if (lt >= 0 && ch >= 0 && ch <= 4 && _fert) {
          _fert->setLowStockThreshold(ch, lt);
        }

        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- Pump Calibration: Toggle Prime ----
  _server.on(
      "/api/fert/pump", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body = String((char *)data).substring(0, len);
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
        String body = String((char *)data).substring(0, len);
        int ch = _extractInt(body, "channel");

        if (ch >= 0 && ch <= 4 && _fert) {
          // Block and pulse
          _fert->manualPump(ch, true);
          unsigned long start = millis();
          while ((millis() - start) < 3000) {
            delay(10);
          }
          _fert->manualPump(ch, false);
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- Pump Calibration: Save Flow Rate ----
  _server.on(
      "/api/fert/calibrate", HTTP_POST, [](AsyncWebServerRequest *request) {},
      NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body = String((char *)data).substring(0, len);
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
      });

  // Obsolete: /api/dose replaced by full /api/fert/schedule usage.

  // ---- POST /api/stock/reset (JSON body) ----
  _server.on(
      "/api/stock/reset", HTTP_POST, [](AsyncWebServerRequest *request) {},
      NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body = String((char *)data).substring(0, len);
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
        String body = String((char *)data).substring(0, len);
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
        String body = String((char *)data).substring(0, len);
        int ch = _extractInt(body, "channel");
        int pwmValue = _extractInt(body, "pwm");

        if (ch >= 0 && ch <= 4 && pwmValue >= 0 && pwmValue <= 255 && _fert) {
          _fert->setPWM(ch, pwmValue);
          Serial.printf("[Web] CH%d PWM set to %d\n", ch + 1, pwmValue);
        }
        request->send(200, "application/json", "{\"ok\":true}");
      });

  // ---- GET /api/notify/status ----
  _server.on(
      "/api/notify/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!_notify) {
          request->send(200, "application/json", "{\"enabled\":false}");
          return;
        }
        String json = "{";
        json +=
            "\"enabled\":" + String(_notify->isEnabled() ? "true" : "false") +
            ",";
        // Mask key for security
        String key = _notify->getPrivateKey();
        if (key.length() > 4) {
          key = key.substring(0, 4) + "****";
        }
        json += "\"key\":\"" + key + "\",";
        json += "\"dailyCount\":" + String(_notify->getDailyCount()) + ",";
        json += "\"reportHour\":" + String(_notify->getDailyReportHour()) + ",";
        json +=
            "\"reportMinute\":" + String(_notify->getDailyReportMinute()) + ",";
        json += "\"types\":[";
        for (uint8_t i = 0; i < NOTIFY_TYPE_COUNT; i++) {
          if (i > 0)
            json += ",";
          json += _notify->isTypeEnabled((NotifyType)i) ? "true" : "false";
        }
        json += "]}";
        request->send(200, "application/json", json);
      });

  // ---- POST /api/notify/key ----
  _server.on(
      "/api/notify/key", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len,
             size_t index, size_t total) {
        String body = String((char *)data).substring(0, len);
        String key = _extractString(body, "key");
        if (_notify) {
          _notify->setPrivateKey(key);
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
        String body = String((char *)data).substring(0, len);

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
               if (_notify)
                 _notify->sendTest();
               request->send(200, "application/json", "{\"ok\":true}");
             });
}
#endif

// ============================================================================
// SIMPLE JSON EXTRACTORS (avoid ArduinoJson dependency)
// ============================================================================

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

  Serial.println("--- Telemetry ---");
  if (_time) {
    Serial.printf("  Time: %s\n", _time->getFormattedTime().c_str());
  }
  if (_safety) {
    Serial.printf("  Water Level: %.1f cm\n", _safety->getLastDistance());
    Serial.printf("  Optical: %s | Float: %s\n",
                  _safety->isOpticalHigh() ? "HIGH" : "low",
                  _safety->isReservoirFull() ? "FULL" : "empty");
    Serial.printf("  Emergency: %s | Maintenance: %s\n",
                  _safety->isEmergency() ? "YES" : "no",
                  _safety->isMaintenanceMode() ? "YES" : "no");
  }
  if (_water) {
    Serial.printf("  TPA State: %s | Canister: %s\n", _water->getStateName(),
                  _water->isCanisterOn() ? "ON" : "OFF");
  }
  if (_fert) {
    for (uint8_t i = 0; i < NUM_FERTS; i++) {
      Serial.printf("  Fert CH%d: stock=%.0f ml\n", i + 1,
                    _fert->getStockML(i));
    }
    Serial.printf("  Prime: stock=%.0f ml\n", _fert->getStockML(NUM_FERTS));
  }
  Serial.println("-----------------");
}

// ============================================================================
// NVS PERSISTENCE
// ============================================================================

void WebManager::_loadParams() {
  _prefs.begin("aqua", true);
  _tpaInterval = _prefs.getUShort("tpaInt", 7);
  _tpaHour = _prefs.getUChar("tpaH", 10);
  _tpaMinute = _prefs.getUChar("tpaM", 0);
  _tpaLastRun = _prefs.getUInt("tpaRun", 0);
  _tpaPercent = _prefs.getUChar("tpaPct", 20);
  _canisterSafePct = _prefs.getUChar("canSf", 0);
  _primeML = _prefs.getFloat("tpaPr", DEFAULT_PRIME_ML);
  _aqHeight = _prefs.getUShort("aqH", 0);
  _aqLength = _prefs.getUShort("aqL", 0);
  _aqWidth = _prefs.getUShort("aqW", 0);
  _aqMarginCm = _prefs.getUShort("aqMg", 0);
  _drainFlowRate = _prefs.getFloat("drFR", 0);
  _refillFlowRate = _prefs.getFloat("rfFR", 0);
  _primeRatio = _prefs.getFloat("pRat", 0);
  _reservoirVolume = _prefs.getUShort("resVol", 0);
  _reservoirSafetyML = _prefs.getFloat("resSf", 0);
  _prefs.end();

  // Auto-calculate primeML from reservoirVolume × ratio if both are set
  if (_reservoirVolume > 0 && _primeRatio > 0) {
    _primeML = _reservoirVolume * _primeRatio;
  }
}

void WebManager::_saveParams() {
  _prefs.begin("aqua", false);
  _prefs.putUShort("tpaInt", _tpaInterval);
  _prefs.putUChar("tpaH", _tpaHour);
  _prefs.putUChar("tpaM", _tpaMinute);
  _prefs.putUInt("tpaRun", _tpaLastRun);
  _prefs.putUChar("tpaPct", _tpaPercent);
  _prefs.putUChar("canSf", _canisterSafePct);
  _prefs.putFloat("tpaPr", _primeML);
  _prefs.putUShort("aqH", _aqHeight);
  _prefs.putUShort("aqL", _aqLength);
  _prefs.putUShort("aqW", _aqWidth);
  _prefs.putUShort("aqMg", _aqMarginCm);
  _prefs.putFloat("drFR", _drainFlowRate);
  _prefs.putFloat("rfFR", _refillFlowRate);
  _prefs.putFloat("pRat", _primeRatio);
  _prefs.putUShort("resVol", _reservoirVolume);
  _prefs.putFloat("resSf", _reservoirSafetyML);
  _prefs.end();
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
    digitalWrite(PIN_CANISTER, LOW); // SSR: LOW = ON
    Serial.println("[CMD] Canister ON.");
  } else if (cmd == "canister_off") {
    digitalWrite(PIN_CANISTER, HIGH); // SSR: HIGH = OFF
    Serial.println("[CMD] Canister OFF.");
  } else if (cmd == "emergency_stop") {
    if (_safety)
      _safety->emergencyShutdown();
  } else if (cmd.startsWith("pushsafer_key ")) {
    String key = cmd.substring(14);
    key.trim();
    if (_notify) {
      _notify->setPrivateKey(key);
      Serial.printf("[CMD] Pushsafer key %s.\n",
                    key.length() > 0 ? "set" : "cleared");
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
      Serial.printf("  Pushsafer: %s\n",
                    _notify->isEnabled() ? "ENABLED" : "DISABLED");
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
  Serial.println("  pushsafer_key KEY — Set Pushsafer key");
  Serial.println("  test_notify   — Send test notification");
  Serial.println("  notify_config — Show notification config");
  Serial.println("========================\n");
}

void WebManager::_printStatus() {
  Serial.println("\n=== System Status ===");
  if (_time) {
    Serial.printf("Time: %s\n", _time->getFormattedTime().c_str());
  }
  if (_safety) {
    Serial.printf("Water: %.1f cm | Emergency: %s | Maintenance: %s\n",
                  _safety->getLastDistance(),
                  _safety->isEmergency() ? "YES" : "no",
                  _safety->isMaintenanceMode() ? "YES" : "no");
  }
  if (_water) {
    Serial.printf("TPA: %s | Canister: %s\n", _water->getStateName(),
                  _water->isCanisterOn() ? "ON" : "OFF");
  }
  Serial.printf("Schedule: TPA=Every %d days at %02d:%02d\n", _tpaInterval,
                _tpaHour, _tpaMinute);
  if (_fert) {
    for (uint8_t i = 0; i < NUM_FERTS; i++) {
      Serial.printf("CH%d: dose[Sun]=%.1f ml, stock=%.0f ml, rate=%.2f mL/s\n",
                    i + 1, _fert->getDoseML(i, 0), _fert->getStockML(i),
                    _fert->getFlowRate(i));
    }
    Serial.printf("Prime: dose[Sun]=%.1f ml, stock=%.0f ml, rate=%.2f mL/s\n",
                  _fert->getDoseML(NUM_FERTS, 0), _fert->getStockML(NUM_FERTS),
                  _fert->getFlowRate(NUM_FERTS));
  }
  Serial.println("=====================\n");
}
