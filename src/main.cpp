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
#include "PumpLog.h"
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
#include <ESPmDNS.h>
#include <driver/gpio.h>

// ============================================================================
// PRE-BOOT SAFETY: Force all output GPIOs LOW before Arduino setup()
// This eliminates the ~100-200ms window where pins float during ESP32 boot,
// which can briefly activate relays and trigger siphon effects.
// Runs automatically before app_main() via GCC constructor attribute.
// ============================================================================
static void __attribute__((constructor)) earlyPinInit() {
  const gpio_num_t pins[] = {
      (gpio_num_t)PIN_FERT1,    (gpio_num_t)PIN_FERT2,
      (gpio_num_t)PIN_FERT3,    (gpio_num_t)PIN_FERT4,
      (gpio_num_t)PIN_PRIME,    (gpio_num_t)PIN_DRAIN,
      (gpio_num_t)PIN_REFILL,   (gpio_num_t)PIN_SOLENOID,
      (gpio_num_t)PIN_CANISTER,
  };
  for (auto pin : pins) {
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0); // LOW — relay OFF
  }
}

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
// COMPLETE holds for many loop iterations, so persisting and propagating the
// calibration must happen once per cycle — not at 20 Hz against the flash.
bool calibrationSettled = false;
bool tpaErrorNotified = false;    // Prevent repeated TPA error notifications

// ---- WiFi Retry State ----
unsigned long lastWiFiRetryTime = 0;
const unsigned long WIFI_RETRY_INTERVAL_MS = 30000; // 30 seconds
bool wifiWasConnected = false; // Track WiFi state transitions for mDNS restoration

// ---- Boot diagnostics (exposed via /api/status) ----
const char *bootResetReason = "UNKNOWN";
unsigned long bootTimeMs = 0; // millis() at end of setup

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  // --- Step 1: Initialize all output pins LOW FIRST (safety critical) ---
  for (uint8_t i = 0; i < NUM_OUTPUT_PINS; i++) {
    pinMode(OUTPUT_PINS[i], OUTPUT);
  }
  allPumpsOff(PumpReason::BOOT_INIT);

  // --- Step 2: Serial ---
  Serial.begin(115200);
  delay(2000);

  // --- Step 2a: Log reset reason (diagnostic) ---
  esp_reset_reason_t resetReason = esp_reset_reason();
  const char *resetStr = "UNKNOWN";
  switch (resetReason) {
    case ESP_RST_POWERON:  resetStr = "POWER_ON"; break;
    case ESP_RST_EXT:      resetStr = "EXTERNAL"; break;
    case ESP_RST_SW:       resetStr = "SOFTWARE"; break;
    case ESP_RST_PANIC:    resetStr = "PANIC_EXCEPTION"; break;
    case ESP_RST_INT_WDT:  resetStr = "INTERRUPT_WATCHDOG"; break;
    case ESP_RST_TASK_WDT: resetStr = "TASK_WATCHDOG"; break;
    case ESP_RST_WDT:      resetStr = "OTHER_WATCHDOG"; break;
    case ESP_RST_DEEPSLEEP:resetStr = "DEEP_SLEEP"; break;
    case ESP_RST_BROWNOUT: resetStr = "BROWNOUT"; break;
    case ESP_RST_SDIO:     resetStr = "SDIO"; break;
    default:               resetStr = "UNKNOWN"; break;
  }
  bootResetReason = resetStr;

  Serial.println("\n==========================================");
  Serial.println("  AQUARIUM AUTOMATION - ESP32 Firmware");
  Serial.printf("  v%s - Pump Log + Safety\n", FIRMWARE_VERSION);
  Serial.printf("  Reset reason: %s\n", resetStr);
  Serial.println("==========================================\n");

  // --- Step 2b: I2C bus scan (diagnostic) ---
  Wire.begin(); // SDA=21, SCL=22
  Serial.println("[I2C] Scanning bus...");
  uint8_t devCount = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[I2C] Device found at 0x%02X", addr);
      if (addr == 0x3C || addr == 0x3D)
        Serial.print(" (SSD1306 OLED)");
      if (addr == 0x68)
        Serial.print(" (DS3231 RTC)");
      if (addr == 0x57)
        Serial.print(" (DS3231 EEPROM)");
      Serial.println();
      devCount++;
    }
  }
  Serial.printf("[I2C] Scan complete: %d device(s) found.\n", devCount);

  // --- Step 2c: OLED Display (early init for boot screen) ---
  displayMgr.initHardware();

  // --- Step 2c: Filesystem ---
  displayMgr.showBootStatus("LittleFS");
  if (!LittleFS.begin(true)) {
    Serial.println("[LittleFS] Mount Failed. Formatting...");
  } else {
    Serial.println("[LittleFS] Mounted successfully.");
  }

  // Load persisted pump log from flash
  pumpLogLoad();

  // --- Step 3: WiFi (must be before NTP/WebServer) ---
  displayMgr.showBootStatus("WiFi scan");
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
    char ipBuf[22];
    snprintf(ipBuf, sizeof(ipBuf), "IP: %s", WiFi.localIP().toString().c_str());
    displayMgr.showBootStatus(ipBuf);

    // Start mDNS Responder
    if (MDNS.begin("iara")) {
      Serial.println("[mDNS] Responder started at http://iara.local");
      MDNS.addService("http", "tcp", 80);
    } else {
      Serial.println("[mDNS] Error setting up mDNS responder!");
    }
    wifiWasConnected = true;
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
    displayMgr.showBootStatus("WiFi: AP Mode");
    
    // Start mDNS Responder for AP mode
    if (MDNS.begin("iara")) {
      Serial.println("[mDNS] Responder started at http://iara.local (AP Mode)");
      MDNS.addService("http", "tcp", 80);
    }
    // Removed immediate WiFi.begin() because it disrupts the SoftAP network.
  }

  // --- Step 4: Safety Watchdog (sensors) ---
  displayMgr.showBootStatus("Sensors");
  safety.begin();

  // --- Step 5: Time Manager (RTC + NTP — needs WiFi) ---
  displayMgr.showBootStatus("RTC + NTP");
  timeMgr.begin();
  pumpLogInit([]() -> String { return timeMgr.getFormattedTime(); });

  // --- Step 5: Fertilizer Manager (NVS state) ---
  fertMgr.begin();

  // --- Step 6: Water Manager (TPA state machine) ---
  waterMgr.begin(&safety, &fertMgr);

  // Load calibrated pump flow rates from NVS
  waterMgr.loadCalibration();

  // --- Step 7: Web Dashboard + Serial UI ---
  displayMgr.showBootStatus("Web server");
  webMgr.begin(&timeMgr, &waterMgr, &fertMgr, &safety, &notifyMgr);

  // --- Step 7b: OLED Display (full init with managers) ---
  displayMgr.begin(&timeMgr, &waterMgr, &fertMgr, &safety, &webMgr, &notifyMgr);
  displayMgr.showBootStatus("System ready!");
  delay(1000); // pause to show final boot log

  // --- Step 8: Canister filter ON by default ---
  pumpOff(PIN_CANISTER, PumpReason::BOOT_INIT); // SSR: LOW = relay ON
  Serial.println("[Main] Canister filter ON (default).\n");

  // --- Step 9: Notifications ---
  notifyMgr.begin();
  notifyMgr.setLanguage(webMgr.getLanguage());

  // --- Step 10: Disable Task Watchdog ---
  // The Arduino framework's task WDT conflicts with our long-running loop
  // (ultrasonic pulseIn blocks ~300ms, I2C transfers, etc). The system already
  // has a comprehensive SafetyWatchdog (sensors, emergency shutdown, overflow
  // detection) so the task WDT is redundant. Disable it to prevent false
  // reboots.
  disableLoopWDT();
  disableCore0WDT();
  Serial.println("[WDT] Task watchdog disabled (SafetyWatchdog active).");

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
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);

  if (!wifiConnected) {
    wifiWasConnected = false;
    if (millis() - lastWiFiRetryTime >= WIFI_RETRY_INTERVAL_MS) {
      Serial.println("\n[WiFi] Connection lost/failed. Retrying connection...");

      // If AP is active, we don't want to kill it, just ask STA to reconnect
      WiFi.reconnect();

      lastWiFiRetryTime = millis();
    }
  } else if (!wifiWasConnected) {
    // WiFi just reconnected — restore mDNS so http://iara.local works again
    wifiWasConnected = true;
    Serial.println("[WiFi] Reconnected! IP: " + WiFi.localIP().toString());

    // mDNS does not survive WiFi disconnection; must be restarted
    MDNS.end();
    if (MDNS.begin("iara")) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("[mDNS] Restored: http://iara.local");
    } else {
      Serial.println("[mDNS] ERROR: Failed to restart mDNS!");
    }
  }

  // ---- 5. SCHEDULING (only if not in maintenance and not running TPA) ----
  if (!safety.isMaintenanceMode()) {

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
    notifyMgr.update(now.hour(), now.minute(), safety.getLastDistance());

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
          if (!webMgr.getTpaAutoEnabled()) {
            Serial.println("[Main] TPA schedule triggered but Auto-TPA is DISABLED - skipping.");
          } else if (!webMgr.isTpaConfigReady()) {
            Serial.println("[Main] TPA schedule triggered but config "
                           "incomplete - skipping.");
          } else {
            if (webMgr.triggerTPA(false)) { // Scheduled = not manual
              tpaDoneThisMinute = true;
            }
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

    // Persist and publish the calibration once. A timed calibration run ends
    // here rather than through stopManual(), so this is the only place that
    // sees it — without the sync the measured rate stays inside WaterManager
    // and never reaches /api/status or isTpaConfigReady().
    if (!calibrationSettled) {
      calibrationSettled = true;
      if (waterMgr.getDrainFlowLPM() > 0 || waterMgr.getRefillFlowLPM() > 0) {
        waterMgr.saveCalibration();
        webMgr.syncFlowRatesFromWater();
      }
    }

    if (!tpaCompleteNotified) {
      if (!waterMgr.isManualTPA()) {
        notifyMgr.notifyTPAComplete();
      }
      tpaCompleteNotified = true;
    }
  } else if (waterMgr.getState() == TPAState::ERROR) {
    if (!tpaErrorNotified) {
      if (!waterMgr.isManualTPA()) {
        notifyMgr.notifyTPAError(waterMgr.getLastErrorMsg().c_str());
      }
      tpaErrorNotified = true;
    }
  } else if (waterMgr.isRunning()) {
    // Reset flags while TPA is actively running
    tpaCompleteNotified = false;
    tpaErrorNotified = false;
    calibrationSettled = false;
  }

  // ---- 5b. CANISTER AUTO-ON TIMER ----
  static unsigned long canisterOffSince = 0;
  if (waterMgr.isCanisterOn()) {
    canisterOffSince = 0;
  } else {
    if (canisterOffSince == 0) {
      canisterOffSince = millis();
    } else if (millis() - canisterOffSince > 3600000UL) { // 1 hour
      if (!waterMgr.isRunning() && !safety.isEmergency() && !safety.isMaintenanceMode()) {
        Serial.println("[Main] Canister off for >1h. Restoring if the level allows.");
        // This used to switch the filter on unconditionally. After a drain that
        // meant starting it with the intake above the water, which is exactly
        // what the safe-level setting exists to prevent.
        if (!waterMgr.restoreCanisterIfSafe(PumpReason::MANUAL_PUMP)) {
          // Still too low — check again after another hour rather than spin.
          canisterOffSince = millis();
        } else {
          canisterOffSince = 0;
        }
      }
    }
  }

  // ---- 6. WEB DASHBOARD + TELEMETRY ----
  // (webMgr.update() already called in step 3 above — no duplicate needed)

  // ---- 7. OLED DISPLAY ----
  displayMgr.update();

  // ---- 8. PUMP LOG PERSISTENCE ----
  pumpLogFlush(); // Write to flash every 10s if dirty

  // ---- 9. YIELD ----
  delay(50); // ~20 Hz loop: enough for safety, yields to FreeRTOS IDLE
}
