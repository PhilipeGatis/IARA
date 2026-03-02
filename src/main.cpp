// =============================================================================
// AQUARIUM AUTOMATION FIRMWARE - ESP32
// =============================================================================
// TPA (Troca Parcial de Água), Fertilization & Filtration Controller
// Priority: Flood prevention and temporal precision
//
// Architecture: OOP with 5 manager classes
//   - SafetyWatchdog: sensor reads, overflow detection, emergency actions
//   - TimeManager:    RTC DS3231 + NTP synchronization
//   - WaterManager:   TPA state machine (6 states)
//   - FertManager:    Daily dosing with NVS deduplication
//   - WebManager:     Embedded web dashboard + Serial command interface
// =============================================================================

#include "Config.h"
#include "DisplayManager.h"
#include "FertManager.h"
#include "NotifyManager.h"
#include "SafetyWatchdog.h"
#include "TimeManager.h"
#include "WaterManager.h"
#include "WebManager.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include <esp_wifi.h>

// ---- Global instances ----
SafetyWatchdog safety;
TimeManager timeMgr;
WaterManager waterMgr;
FertManager fertMgr;
WebManager webMgr;
DisplayManager displayMgr;
NotifyManager notifyMgr;

// ---- Scheduling state ----
bool fertDoneThisMinute = false; // Prevent re-triggering within same minute
uint8_t lastFertMinute = 255;
bool tpaDoneThisMinute = false;
uint8_t lastTPAMinute = 255;
bool emergencyNotified = false;   // Prevent repeated emergency notifications
bool tpaCompleteNotified = false; // Prevent repeated TPA complete notifications
bool tpaErrorNotified = false;    // Prevent repeated TPA error notifications

// ---- WiFi Retry State ----
unsigned long lastWiFiRetryTime = 0;
const unsigned long WIFI_RETRY_INTERVAL_MS = 30000; // 30 seconds

// ---- TPA Button State ----
bool lastButtonState = HIGH; // INPUT_PULLUP: idle = HIGH
bool lastFertBtnState = HIGH;

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  // --- Step 1: Initialize all output pins LOW FIRST (safety critical) ---
  for (uint8_t i = 0; i < NUM_OUTPUT_PINS; i++) {
    pinMode(OUTPUT_PINS[i], OUTPUT);
    digitalWrite(OUTPUT_PINS[i], LOW);
  }

  // TPA button
  pinMode(PIN_TPA_BUTTON, INPUT_PULLUP);
  pinMode(PIN_FERT_BUTTON, INPUT_PULLUP);

  // --- Step 2: Serial ---
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n==========================================");
  Serial.println("  AQUARIUM AUTOMATION - ESP32 Firmware");
  Serial.println("  v3.0.0 - Web Dashboard");
  Serial.println("==========================================\n");

  // --- Step 2b: Filesystem ---
  if (!LittleFS.begin(true)) {
    Serial.println("[LittleFS] Mount Failed. Formatting...");
  } else {
    Serial.println("[LittleFS] Mounted successfully.");
  }

  // --- Step 3: WiFi (must be before NTP/WebServer) ---
  Preferences wifiPref;
  wifiPref.begin("wifi", true); // true = readonly
  String savedSSID = wifiPref.getString("ssid", String(WIFI_SSID));
  String savedPass = wifiPref.getString("pass", String(WIFI_PASSWORD));
  wifiPref.end();

  Serial.printf("[WiFi] SSID: '%s'\n", savedSSID.c_str());
  Serial.printf("[WiFi] PASS: len=%d\n", savedPass.length());
  Serial.print("[WiFi] Connecting");

  // Add Event Listener to catch specific disconnect reasons
  WiFi.onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      Serial.printf("\n[WiFi Event] Disconnected! Reason Code: %d\n",
                    info.wifi_sta_disconnected.reason);
    }
  });

  WiFi.mode(WIFI_STA);         // Set STA mode first
  WiFi.disconnect(true, true); // Clean previous connections
  delay(500);

  // Advanced Router Compatibility Fixes (for TIM / Smart Routers)
  WiFi.setSleep(
      false); // Disable sleep for better compatibility with some routers
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);

  // Active Scan Workaround: Force the radio to wake up, populate the BSSID
  // cache, and extract the exact BSSID/Channel to avoid Reason 201 (NO AP FOUND
  // / Band Steering)
  Serial.print(" Scanning...");
  int n = WiFi.scanNetworks();
  delay(100);

  uint8_t targetBSSID[6] = {0};
  int32_t targetChannel = 0;
  bool bssidFound = false;

  for (int i = 0; i < n; ++i) {
    if (WiFi.SSID(i) == savedSSID) {
      memcpy(targetBSSID, WiFi.BSSID(i), 6);
      targetChannel = WiFi.channel(i);
      bssidFound = true;
      Serial.printf("\n[WiFi] Found Target BSSID: "
                    "%02X:%02X:%02X:%02X:%02X:%02X on Channel: %d\n",
                    targetBSSID[0], targetBSSID[1], targetBSSID[2],
                    targetBSSID[3], targetBSSID[4], targetBSSID[5],
                    targetChannel);
      break;
    }
  }

  // CRITICAL FIX: To prevent Reason 15 (4-way handshake timeout) on
  // TIM routers we need to set the WiFi config explicitly to disable Protected
  // Management Frames (PMF) because the ESP32 WPA3 implementation sometimes
  // fails to negotiate with modern mixed-mode routers.

  wifi_config_t wifi_config = {};
  strlcpy((char *)wifi_config.sta.ssid, savedSSID.c_str(),
          sizeof(wifi_config.sta.ssid));
  strlcpy((char *)wifi_config.sta.password, savedPass.c_str(),
          sizeof(wifi_config.sta.password));

  // Force WPA2 and Disable PMF completely to bypass the handshake hang
  wifi_config.sta.pmf_cfg.capable = false;
  wifi_config.sta.pmf_cfg.required = false;

  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

  // Re-attempt standard connection, locking to BSSID and Channel if found
  if (bssidFound) {
    WiFi.begin(savedSSID.c_str(), savedPass.c_str(), targetChannel, targetBSSID,
               true);
  } else {
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
  }

  {
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED &&
           attempts < 60) { // Incremented timeout (Reason 39)
      delay(500);
      Serial.print(".");
      attempts++;
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" FAILED!");
    Serial.print("[WiFi] Error Code: ");
    switch (WiFi.status()) {
    case WL_NO_SHIELD:
      Serial.println("WL_NO_SHIELD");
      break;
    case WL_IDLE_STATUS:
      Serial.println("WL_IDLE_STATUS");
      break;
    case WL_NO_SSID_AVAIL:
      Serial.println("WL_NO_SSID_AVAIL");
      break;
    case WL_SCAN_COMPLETED:
      Serial.println("WL_SCAN_COMPLETED");
      break;
    case WL_CONNECT_FAILED:
      Serial.println("WL_CONNECT_FAILED");
      break;
    case WL_CONNECTION_LOST:
      Serial.println("WL_CONNECTION_LOST");
      break;
    case WL_DISCONNECTED:
      Serial.println("WL_DISCONNECTED");
      break;
    default:
      Serial.printf("UNKNOWN (%d)\n", WiFi.status());
      break;
    }

    Serial.println("[WiFi] Starting AP mode fallback");
    WiFi.mode(WIFI_AP_STA); // AP + keep trying STA in background
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.printf("[WiFi] AP started: SSID='%s' PASS='%s'\n", AP_SSID,
                  AP_PASSWORD);
    Serial.print("[WiFi] AP IP: ");
    Serial.println(WiFi.softAPIP());
    // Removed immediate WiFi.begin() because it disrupts the SoftAP network.
  }

  // --- Step 4: Safety Watchdog (sensors) ---
  safety.begin();

  // --- Step 5: Time Manager (RTC + NTP — needs WiFi) ---
  timeMgr.begin();

  // --- Step 5: Fertilizer Manager (NVS state) ---
  fertMgr.begin();

  // --- Step 6: Water Manager (TPA state machine) ---
  waterMgr.begin(&safety, &fertMgr);

  // Load calibrated pump flow rates from NVS
  {
    Preferences calPref;
    calPref.begin("pumpcal", true); // read-only
    float drainLPM = calPref.getFloat("drainLPM", 0);
    float refillLPM = calPref.getFloat("refillLPM", 0);
    calPref.end();
    if (drainLPM > 0) {
      waterMgr.setDrainFlowLPM(drainLPM);
      Serial.printf("[Main] Loaded drain calibration: %.2f L/min\n", drainLPM);
    }
    if (refillLPM > 0) {
      waterMgr.setRefillFlowLPM(refillLPM);
      Serial.printf("[Main] Loaded refill calibration: %.2f L/min\n",
                    refillLPM);
    }
    if (drainLPM <= 0 && refillLPM <= 0) {
      Serial.println(
          "[Main] No pump calibration found. Using safe defaults (30s/15s).");
    }
  }

  // --- Step 7: Web Dashboard + Serial UI ---
  webMgr.begin(&timeMgr, &waterMgr, &fertMgr, &safety, &notifyMgr);

  // --- Step 7b: OLED Display ---
  displayMgr.begin(&timeMgr, &waterMgr, &fertMgr, &safety, &webMgr);

  // --- Step 8: Canister filter ON by default ---
  digitalWrite(PIN_CANISTER, LOW); // SSR: LOW = relay ON
  Serial.println("[Main] Canister filter ON (default).\n");

  // --- Step 9: Notifications ---
  notifyMgr.begin();

  // --- Step 10: Hardware Watchdog (10s timeout, auto-reboot on freeze) ---
  esp_task_wdt_init(10, true); // 10 seconds, panic (reboot) on timeout
  esp_task_wdt_add(NULL);      // Register main task
  Serial.println("[WDT] Hardware watchdog enabled (10s timeout).");

  Serial.println("[Main] === System Ready ===\n");
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  // ---- 1. SAFETY (highest priority, runs every 500ms) ----
  safety.update();

  // If in emergency, skip all scheduling and just process commands
  if (safety.isEmergency()) {
    if (!emergencyNotified) {
      notifyMgr.notifyEmergency("Sistema em estado de emergência!");
      emergencyNotified = true;
    }
    webMgr.processSerialCommands();
    webMgr.update(); // keep web server alive
    delay(100);
    return;
  } else {
    emergencyNotified = false;
  }

  // ---- 2. TIME SYNC (periodic NTP re-sync) ----
  timeMgr.update();

  // ---- 3. SERIAL COMMANDS + WEB ----
  webMgr.processSerialCommands();
  webMgr.update(); // handle SSE and HTTP clients

  // ---- 4. WIFI RETRY LOGIC (Every 30 seconds) ----
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWiFiRetryTime >= WIFI_RETRY_INTERVAL_MS) {
      Serial.println("\n[WiFi] Connection lost/failed. Retrying connection...");

      // If AP is active, we don't want to kill it, just ask STA to reconnect
      WiFi.reconnect();

      lastWiFiRetryTime = millis();
    }
  }

  // ---- 5. SCHEDULING (only if not in maintenance and not running TPA) ----
  if (!safety.isMaintenanceMode()) {

    // ---- Manual TPA button (GPIO 15, active LOW with debounce) ----
    bool btnState = digitalRead(PIN_TPA_BUTTON);
    if (btnState == LOW && lastButtonState == HIGH && !waterMgr.isRunning()) {
      Serial.println("[BTN] TPA button pressed — starting TPA...");
      float lvl = safety.readUltrasonic();
      waterMgr.setDrainTargetCm(lvl + 5.0f);
      waterMgr.setRefillTargetCm(lvl);
      waterMgr.setPrimeML(DEFAULT_PRIME_ML);
      waterMgr.startTPA();
    }
    lastButtonState = btnState;

    // ---- Manual FERT button (GPIO 23, active LOW with debounce) ----
    bool fertBtnState = digitalRead(PIN_FERT_BUTTON);
    if (fertBtnState == LOW && lastFertBtnState == HIGH) {
      Serial.println("[BTN] Fert button pressed — dosing all channels...");
      for (uint8_t ch = 0; ch < NUM_FERTS; ch++) {
        float dose = fertMgr.getDoseML(ch, 0); // Use Sunday dose as default
        if (dose <= 0)
          dose = DEFAULT_DOSE_ML;
        Serial.printf("[BTN] CH%d: %.1f mL\n", ch + 1, dose);
        fertMgr.doseChannel(ch, dose);
        float stock = fertMgr.getStockML(ch);
        fertMgr.setStockML(ch, stock - dose);
      }
      fertMgr.saveState();
      Serial.println("[BTN] Fertilization complete.");
    }
    lastFertBtnState = fertBtnState;

    DateTime now = timeMgr.now();
    uint8_t currentMinute = now.minute();

    // --- Fertilization schedule (Independent per Channel) ---
    fertMgr.update(now);

    // --- Check low stock after fertilization ---
    for (uint8_t ch = 0; ch < NUM_FERTS + 1; ch++) {
      if (fertMgr.isLowStock(ch)) {
        notifyMgr.notifyFertLowStock(ch, fertMgr.getStockML(ch),
                                     fertMgr.getLowStockThreshold(ch));
      }
    }

    // --- Notifications: daily level report + midnight reset ---
    notifyMgr.update(now.hour(), now.minute());
    if (now.hour() == notifyMgr.getDailyReportHour() &&
        now.minute() == notifyMgr.getDailyReportMinute()) {
      float level = safety.getLastDistance();
      notifyMgr.notifyDailyLevel(level);
    }

    // --- TPA schedule ---
    if (currentMinute != lastTPAMinute) {
      tpaDoneThisMinute = false;
      lastTPAMinute = currentMinute;

      // Evaluate interval-based execution
      bool isTPADay = false;
      uint16_t interval = webMgr.getTpaInterval();
      if (interval > 0) {
        unsigned long lastRun = webMgr.getTpaLastRun();
        unsigned long nowEpoch = timeMgr.now().unixtime();

        // 43200 seconds = 12 hours. We grant a 12h leeway so that DST shifts
        // or small clock drifts don't cause it to miss a day. The precise
        // trigger happens below by strictly matching hour and minute.
        if (lastRun == 0 ||
            nowEpoch >= (lastRun + (interval * 86400) - 43200)) {
          isTPADay = true;
        }
      }

      // Determine if a TPA should start (evaluated only once per minute)
      if (!waterMgr.isRunning() && isTPADay) {
        if (timeMgr.isDailyScheduleTime(webMgr.getTpaHour(),
                                        webMgr.getTpaMinute())) {
          if (!webMgr.isTpaConfigReady()) {
            Serial.println("[Main] TPA schedule triggered but config "
                           "incomplete - skipping.");
          } else {
            // Compute dynamic drain/refill targets
            float currentLevel = safety.readUltrasonic();
            float lPerCm = webMgr.getLitersPerCm();
            float aqVol = (float)webMgr.getAquariumVolume();
            float drainLiters = aqVol * webMgr.getTpaPercent() / 100.0f;

            // Cap by reservoir available volume (minus safety margin)
            float resAvail = (float)webMgr.getReservoirVolume() -
                             webMgr.getReservoirSafetyML() / 1000.0f;
            if (resAvail > 0 && drainLiters > resAvail) {
              drainLiters = resAvail;
              Serial.printf("[Main] TPA capped to %.1f L (reservoir limit)\n",
                            drainLiters);
            }

            float cmToDrain = (lPerCm > 0) ? drainLiters / lPerCm : 0;
            waterMgr.setDrainTargetCm(currentLevel + cmToDrain);
            waterMgr.setRefillTargetCm(currentLevel);
            waterMgr.setLitersPerCm(lPerCm); // For inline calibration

            // Compute canister safe level from percentage
            // canisterSafePct = min water % for canister → convert to cm
            // (ultrasonic dist) Higher % = needs more water = lower ultrasonic
            // distance
            float effH =
                (float)webMgr.getAquariumVolume() / lPerCm; // effective height
            float canisterSafeCm =
                effH * (100.0f - webMgr.getCanisterSafePct()) / 100.0f;
            waterMgr.setCanisterSafeLevelCm(canisterSafeCm);

            // Dynamic timeouts (if calibrated)
            float drainLPM = waterMgr.getDrainFlowLPM();
            float refillLPM = waterMgr.getRefillFlowLPM();
            if (drainLPM > 0) {
              unsigned long t =
                  (unsigned long)((drainLiters / drainLPM) * 1.5f * 60000.0f);
              waterMgr.setTimeoutDrainMs(t);
              Serial.printf("[Main] Drain timeout: %lums (calibrated)\n", t);
            }
            if (refillLPM > 0) {
              unsigned long t =
                  (unsigned long)((drainLiters / refillLPM) * 1.5f * 60000.0f);
              waterMgr.setTimeoutRefillMs(t);
              Serial.printf("[Main] Refill timeout: %lums (calibrated)\n", t);
            }

            Serial.printf(
                "[Main] TPA: %.1f L = %.1f cm, drain to %.1f, refill to %.1f\n",
                drainLiters, cmToDrain, currentLevel + cmToDrain, currentLevel);
            waterMgr.startTPA();
            webMgr.setTpaLastRun(timeMgr.now().unixtime());
            tpaDoneThisMinute = true;
          }
        }
      }
    }
  }

  // ---- 5. TPA STATE MACHINE ----
  waterMgr.update();

  // If TPA just completed, record timestamp, save calibration, and notify
  if (waterMgr.getState() == TPAState::COMPLETE) {
    waterMgr.setLastTPATime(timeMgr.getFormattedTime());

    // Save calibrated flow rates for next TPA
    if (waterMgr.getDrainFlowLPM() > 0 || waterMgr.getRefillFlowLPM() > 0) {
      Preferences calPref;
      calPref.begin("pumpcal", false);
      if (waterMgr.getDrainFlowLPM() > 0)
        calPref.putFloat("drainLPM", waterMgr.getDrainFlowLPM());
      if (waterMgr.getRefillFlowLPM() > 0)
        calPref.putFloat("refillLPM", waterMgr.getRefillFlowLPM());
      calPref.end();
      Serial.printf("[Main] Calibration saved: drain=%.2f refill=%.2f L/min\n",
                    waterMgr.getDrainFlowLPM(), waterMgr.getRefillFlowLPM());
    }

    if (!tpaCompleteNotified) {
      notifyMgr.notifyTPAComplete();
      tpaCompleteNotified = true;
    }
  } else if (waterMgr.getState() == TPAState::ERROR) {
    if (!tpaErrorNotified) {
      notifyMgr.notifyTPAError("Timeout ou falha durante o ciclo TPA.");
      tpaErrorNotified = true;
    }
  } else if (waterMgr.isRunning()) {
    // Reset flags while TPA is actively running
    tpaCompleteNotified = false;
    tpaErrorNotified = false;
  }

  // ---- 6. WEB DASHBOARD + TELEMETRY ----
  webMgr.update();

  // ---- 7. OLED DISPLAY ----
  displayMgr.update();

  // ---- 8. WATCHDOG RESET + YIELD ----
  esp_task_wdt_reset(); // Feed the hardware watchdog
  delay(10);            // ~100 Hz loop: fast enough for safety, gentle on CPU
}
