#include "NotifyManager.h"
#include <Preferences.h>

#ifdef USE_WEBSERVER
#include <WiFi.h>
#include <WiFiClientSecure.h>
#endif

static Preferences _nPrefs;

// CONSTRUCTOR

NotifyManager::NotifyManager()
    : _lang(LANG_PT), _dailyReportHour(8), _dailyReportMinute(0),
      _dailyReportSent(false), _dailyCount(0), _lastResetDay(0) {
  for (uint8_t i = 0; i < NOTIFY_TYPE_COUNT; i++) {
    _typeEnabled[i] = true; // All enabled by default
    _lastNotifyMs[i] = 0;
  }
#ifdef UNIT_TEST
  _lastSendResult = false;
#endif
}

// BEGIN

void NotifyManager::begin() {
  _loadConfig();
  
#ifdef USE_WEBSERVER
  // Auto-generate a topic based on MAC address if none is set
  if (!isEnabled()) {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    _topic = "iara_" + mac;
    _saveConfig();
  }
#endif

  Serial.printf("[Notify] ntfy.sh %s (Topic: %s). Daily report at %02d:%02d\n",
                isEnabled() ? "ENABLED" : "DISABLED", _topic.c_str(), _dailyReportHour,
                _dailyReportMinute);
}

// UPDATE (called from loop — checks daily report schedule)

void NotifyManager::update(uint8_t currentHour, uint8_t currentMinute,
                           float levelCm) {
  // Re-arm at midnight. This must not be conditional on anything: gating it on
  // _dailyCount meant a quiet day left the flag stuck and the daily report was
  // never sent again.
  if (currentHour == 0 && currentMinute == 0) {
    _dailyCount = 0;
    _dailyReportSent = false;
  }

  if (currentHour != _dailyReportHour || currentMinute != _dailyReportMinute) {
    return;
  }

  // Report time. The flag keeps it to one send even though the loop runs at
  // ~20 Hz for the whole minute.
  if (!_dailyReportSent) {
    _dailyReportSent = true;
    notifyDailyLevel(levelCm);
  }
}

// TYPED NOTIFICATIONS

void NotifyManager::notifyTPAComplete() {
  if (!_canSend(NOTIFY_TPA_COMPLETE))
    return;
  const auto &s = NOTIFY_STRINGS[_lang];
  _send(NOTIFY_TPA_COMPLETE, s.tpaCompleteTitle, s.tpaCompleteMsg, "default", "droplet,white_check_mark");
}

void NotifyManager::notifyTPAError(const char *reason) {
  if (!_canSend(NOTIFY_TPA_ERROR))
    return;
  const auto &s = NOTIFY_STRINGS[_lang];
  char msg[128];
  snprintf(msg, sizeof(msg), s.tpaErrorFmt, reason);
  _send(NOTIFY_TPA_ERROR, s.tpaErrorTitle, msg, "high", "warning,droplet");
}

void NotifyManager::notifyFertLowStock(uint8_t channel, float remainingML,
                                       float thresholdML) {
  if (!_canSend(NOTIFY_FERT_LOW_STOCK))
    return;
  const auto &s = NOTIFY_STRINGS[_lang];
  char msg[128];
  snprintf(msg, sizeof(msg), s.fertLowStockFmt, channel + 1, remainingML,
           thresholdML);
  _send(NOTIFY_FERT_LOW_STOCK, s.fertLowStockTitle, msg, "default", "test_tube,warning");
}

void NotifyManager::notifyEmergency(const char *reason) {
  if (!_canSend(NOTIFY_EMERGENCY))
    return;
  const auto &s = NOTIFY_STRINGS[_lang];
  char msg[128];
  snprintf(msg, sizeof(msg), s.emergencyFmt, reason);
  _send(NOTIFY_EMERGENCY, s.emergencyTitle, msg, "urgent", "rotating_light");
}

void NotifyManager::notifyFertComplete(uint8_t channel, float doseML) {
  if (!_canSend(NOTIFY_FERT_COMPLETE))
    return;
  const auto &s = NOTIFY_STRINGS[_lang];
  char msg[128];
  snprintf(msg, sizeof(msg), s.fertCompleteFmt, channel + 1, doseML);
  _send(NOTIFY_FERT_COMPLETE, s.fertCompleteTitle, msg, "low", "test_tube,white_check_mark");
}

void NotifyManager::notifyDailyLevel(float levelCm) {
  if (!_canSend(NOTIFY_DAILY_LEVEL))
    return;
  const auto &s = NOTIFY_STRINGS[_lang];
  char msg[128];
  snprintf(msg, sizeof(msg), s.dailyLevelFmt, levelCm);
  _send(NOTIFY_DAILY_LEVEL, s.dailyLevelTitle, msg, "low", "bar_chart,ocean");
}

void NotifyManager::sendTest() {
  if (!isEnabled()) {
    Serial.println("[Notify] Cannot send test: no ntfy.sh topic configured.");
    return;
  }
  const auto &s = NOTIFY_STRINGS[_lang];
  bool ok = _send(NOTIFY_TYPE_COUNT, s.testTitle, s.testMsg, "default", "partying_face");
  Serial.printf("[Notify] Test notification %s.\n", ok ? "SENT" : "FAILED");
}

// RATE LIMITING

bool NotifyManager::_canSend(NotifyType type) {
  if (!isEnabled()) {
    return false;
  }

  if (type >= NOTIFY_TYPE_COUNT) {
    return false;
  }

  // Check per-type toggle
  if (!_typeEnabled[type]) {
    return false;
  }

  // Check daily limit
  if (_dailyCount >= MAX_DAILY_NOTIFICATIONS) {
    Serial.println("[Notify] Daily limit reached, skipping.");
    return false;
  }

  // Check cooldown
  unsigned long now = millis();
  if (_lastNotifyMs[type] > 0 &&
      (now - _lastNotifyMs[type]) < NOTIFY_COOLDOWN_MS) {
    return false;
  }

  return true;
}

// HTTPS SEND

bool NotifyManager::_send(NotifyType type, const char *title,
                          const char *message, const char *priority,
                          const char *tags) {
#ifdef UNIT_TEST
  // In test mode, just record the attempt
  _lastSendResult = true;
  _dailyCount++;
  if (type < NOTIFY_TYPE_COUNT) {
    _lastNotifyMs[type] = millis();
  }
  return true;
#endif

#ifdef USE_WEBSERVER
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Notify] WiFi not connected, skipping notification.");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure(); // ntfy.sh uses Let's Encrypt, safe enough for this purpose
  client.setTimeout(10);

  if (!client.connect("ntfy.sh", 443)) {
    Serial.println("[Notify] HTTPS connection failed.");
    return false;
  }

  String msgStr = String(message);

  // Send HTTP POST
  client.print("POST /");
  client.print(_topic);
  client.println(" HTTP/1.1");
  client.println("Host: ntfy.sh");
  
  // Encode title appropriately (for simplicity, we assume safe ascii, but real world might need RFC 2047)
  // Actually, ntfy supports "Title" header
  client.print("Title: ");
  client.println(title);
  
  if (priority && strlen(priority) > 0) {
    client.print("X-Priority: ");
    client.println(priority);
  }
  if (tags && strlen(tags) > 0) {
    client.print("X-Tags: ");
    client.println(tags);
  }

  client.print("Content-Length: ");
  client.println(msgStr.length());
  client.println("Connection: close");
  client.println();
  client.print(msgStr);

  // Read response status
  unsigned long start = millis();
  while (!client.available() && (millis() - start) < 5000) {
    delay(50);
  }

  bool success = false;
  if (client.available()) {
    String statusLine = client.readStringUntil('\n');
    success = statusLine.indexOf("200") >= 0;
    Serial.printf("[Notify] ntfy.sh response: %s\n", statusLine.c_str());
  }

  client.stop();

  if (success) {
    _dailyCount++;
    // Set cooldown so this type won't fire again for NOTIFY_COOLDOWN_MS
    if (type < NOTIFY_TYPE_COUNT) {
      _lastNotifyMs[type] = millis();
    }
    Serial.printf("[Notify] Sent: \"%s\" (%d/%d today)\n", title, _dailyCount,
                  MAX_DAILY_NOTIFICATIONS);
  }

  return success;
#else
  Serial.printf("[Notify] (no WiFi) Would send: %s — %s\n", title, message);
  return false;
#endif
}

// CONFIGURATION

void NotifyManager::setTopic(const String &topic) {
  _topic = topic;
  _saveConfig();
  Serial.printf("[Notify] Topic %s.\n",
                topic.length() > 0 ? topic.c_str() : "cleared");
}

void NotifyManager::setTypeEnabled(NotifyType type, bool on) {
  if (type < NOTIFY_TYPE_COUNT) {
    _typeEnabled[type] = on;
    _saveConfig();
  }
}

bool NotifyManager::isTypeEnabled(NotifyType type) const {
  if (type < NOTIFY_TYPE_COUNT) {
    return _typeEnabled[type];
  }
  return false;
}

void NotifyManager::setDailyReportHour(uint8_t h, uint8_t m) {
  _dailyReportHour = h;
  _dailyReportMinute = m;
  _saveConfig();
  Serial.printf("[Notify] Daily report set to %02d:%02d\n", h, m);
}

// NVS PERSISTENCE

void NotifyManager::_loadConfig() {
  _nPrefs.begin("notify", true); // readonly
  _topic = _nPrefs.getString("key", "");

  // Load per-type toggles (stored as a bitmask in a single byte)
  uint8_t mask = _nPrefs.getUChar("mask", 0xFF); // all enabled by default
  for (uint8_t i = 0; i < NOTIFY_TYPE_COUNT; i++) {
    _typeEnabled[i] = (mask >> i) & 1;
  }

  _dailyReportHour = _nPrefs.getUChar("repH", 8);
  _dailyReportMinute = _nPrefs.getUChar("repM", 0);
  _nPrefs.end();
}

void NotifyManager::_saveConfig() {
  _nPrefs.begin("notify", false);
  _nPrefs.putString("key", _topic);

  // Store toggles as bitmask
  uint8_t mask = 0;
  for (uint8_t i = 0; i < NOTIFY_TYPE_COUNT; i++) {
    if (_typeEnabled[i])
      mask |= (1 << i);
  }
  _nPrefs.putUChar("mask", mask);

  _nPrefs.putUChar("repH", _dailyReportHour);
  _nPrefs.putUChar("repM", _dailyReportMinute);
  _nPrefs.end();
}
