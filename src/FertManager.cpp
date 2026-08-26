#include "FertManager.h"

FertManager::FertManager() {
  for (uint8_t i = 0; i < NUM_FERTS + 1; i++) {
    _applyDefaults(i);
  }
}

void FertManager::begin() {
  _prefs.begin("fert", false); // RW mode
  _loadState();

  // Initialize LEDC (hardware PWM) for the pump pins
  for (uint8_t i = 0; i < NUM_FERTS + 1; i++) {
    ledcSetup(i, 5000, 8); // Channel i, 5kHz, 8-bit resolution (0-255)
    ledcAttachPin(_pinForChannel(i), i);
    ledcWrite(i, 0); // Initialize OFF
  }

  Serial.println("[Fert] Manager initialized.");
  Serial.printf("[Fert] Last dose key: %u\n", _lastDoseKey[0]);
  for (uint8_t i = 0; i < NUM_FERTS; i++) {
    Serial.printf("[Fert] CH%d ('%s'): stock=%.1f ml\n", i + 1,
                  _names[i].c_str(), _stockML[i]);
  }
  Serial.printf("[Fert] Prime ('%s'): stock=%.1f ml\n",
                _names[NUM_FERTS].c_str(), _stockML[NUM_FERTS]);
}

void FertManager::update(DateTime now) {
  // A dose in progress owns the channel until it ends. Starting a second one
  // would overlap two pumps against a single elapsed-time measurement.
  if (_doseActive)
    return;

  uint32_t todayKey = _dateKey(now);
  uint8_t currentDow = now.dayOfTheWeek();
  uint8_t currentHour = now.hour();
  uint8_t currentMinute = now.minute();

  for (uint8_t i = 0; i < NUM_FERTS + 1; i++) {
    if (!_enabled[i]) continue;

    // Check if it's the right time for today's day-of-week (minute precision)
    if (currentHour == _schedHour[i][currentDow] &&
        currentMinute == _schedMinute[i][currentDow]) {
      // Check if it was already dosed today
      if (_lastDoseKey[i] != todayKey) {
        float ds = _doseML[i][currentDow];
        if (ds > 0 && _stockML[i] >= ds) {
          Serial.printf("[Fert] Scheduled auto-dose CH%d: %.1f ml\n", i + 1,
                        ds);
          if (startDose(i, ds)) {
            // Booked at the start, not on completion. If the dose is cut short
            // the stock figure is pessimistic, which is the safe direction —
            // whereas booking it at the end leaves a window in which the same
            // channel could be scheduled again, and a double dose is the one
            // failure here that kills livestock.
            _stockML[i] -= ds;
            if (_stockML[i] < 0)
              _stockML[i] = 0;
            _markDosed(i, now);
            saveState();
            // One channel per pass; the next update() picks up the next one
            // once this dose has finished.
            return;
          }
        } else if (ds > 0) {
          Serial.printf(
              "[Fert] Skipping CH%d: Insufficient stock (%.1f < %.1f)\n", i + 1,
              _stockML[i], ds);
        } else if (ds <= 0) {
          _markDosed(
              i,
              now); // Volume is 0 for today, just mark as checked to prevent
                    // loop repeats if someone sets it via UI during the minute
        }
      }
    }
  }
}

bool FertManager::startDose(uint8_t ch, float ml) {
  if (!_isValidChannel(ch))
    return false;
  if (ml <= 0)
    return false;
  if (_doseActive)
    return false;

  uint8_t pin = _pinForChannel(ch);
  float rate = _flowRateMLps[ch];
  if (rate <= 0)
    rate = FLOW_RATE_ML_PER_SEC; // Fallback safety

  unsigned long durationMs = (unsigned long)((ml / rate) * 1000.0f);
  unsigned long timeout =
      (ch == NUM_FERTS) ? TIMEOUT_PRIME_MS : TIMEOUT_FERT_MS;

  // Cap to timeout
  if (durationMs > timeout) {
    Serial.printf("[Fert] WARNING: dose duration %lu ms exceeds timeout %lu "
                  "ms. Capping.\n",
                  durationMs, timeout);
    durationMs = timeout;
  }

  Serial.printf("[Fert] Activating pin %d for %lu ms (Rate: %.2f mL/s)\n", pin,
                durationMs, rate);
  ledcWrite(ch, _pwm[ch]);

  _doseActive = true;
  _doseChannel = ch;
  _doseEndMs = millis() + durationMs;
  return true;
}

void FertManager::tickDose() {
  // Also ends a manual run that outlived its ceiling. Both live here because
  // this is the one hook loop() calls unconditionally, above every early return.
  if (_manualActive && (long)(millis() - _manualEndMs) >= 0) {
    ledcWrite(_manualChannel, 0);
    _manualActive = false;
    Serial.printf("[Fert] CH%d manual run hit its %lu ms ceiling — stopped\n",
                  _manualChannel + 1, MANUAL_FERT_MAX_MS);
  }

  if (!_doseActive)
    return;
  // Signed difference so the comparison stays correct across the millis()
  // rollover at ~49 days, which this board will reach.
  if ((long)(millis() - _doseEndMs) < 0)
    return;
  ledcWrite(_doseChannel, 0);
  _doseActive = false;
  Serial.printf("[Fert] CH%d dose finished\n", _doseChannel + 1);
}

void FertManager::abortDose() {
  if (_manualActive) {
    ledcWrite(_manualChannel, 0);
    _manualActive = false;
  }
  if (!_doseActive)
    return;
  ledcWrite(_doseChannel, 0);
  _doseActive = false;
  Serial.printf("[Fert] CH%d dose aborted mid-volume\n", _doseChannel + 1);
}

void FertManager::manualPump(uint8_t ch, bool state) {
  if (!_isValidChannel(ch))
    return;
  // The caller is a browser, and the OFF request is not guaranteed to arrive:
  // a closed tab, a dropped link or a crashed page between ON and OFF used to
  // leave the pump running indefinitely. tickDose() enforces the ceiling.
  _manualActive = state;
  _manualChannel = ch;
  _manualEndMs = millis() + MANUAL_FERT_MAX_MS;
  ledcWrite(ch, state ? _pwm[ch] : 0);
  Serial.printf("[Fert] Manual pump CH%d set to %s (PWM: %d)\n", ch + 1,
                state ? "ON" : "OFF", state ? _pwm[ch] : 0);
}

void FertManager::setPWM(uint8_t ch, uint8_t pwm) {
  if (_isValidChannel(ch)) {
    _pwm[ch] = pwm;
    saveState();
  }
}

void FertManager::setDoseML(uint8_t ch, uint8_t dayOfWeek, float ml) {
  if (_isValidChannel(ch) && dayOfWeek < 7) {
    _doseML[ch][dayOfWeek] = ml;
  }
}

float FertManager::getDoseML(uint8_t ch, uint8_t dayOfWeek) const {
  return (_isValidChannel(ch) && dayOfWeek < 7) ? _doseML[ch][dayOfWeek] : 0.0f;
}

void FertManager::setScheduleTime(uint8_t ch, uint8_t day, uint8_t hour,
                                  uint8_t minute) {
  if (_isValidChannel(ch) && day < 7) {
    _schedHour[ch][day] = hour;
    _schedMinute[ch][day] = minute;
  }
}

void FertManager::setScheduleTimeAll(uint8_t ch, uint8_t hour, uint8_t minute) {
  if (_isValidChannel(ch)) {
    for (uint8_t d = 0; d < 7; d++) {
      _schedHour[ch][d] = hour;
      _schedMinute[ch][d] = minute;
    }
  }
}

void FertManager::setFlowRate(uint8_t ch, float mlPerSec) {
  if (_isValidChannel(ch) && mlPerSec > 0.01f) {
    _flowRateMLps[ch] = mlPerSec;
  }
}

float FertManager::getStockML(uint8_t ch) const {
  return _isValidChannel(ch) ? _stockML[ch] : 0;
}

void FertManager::setStockML(uint8_t ch, float ml) {
  if (_isValidChannel(ch)) {
    _stockML[ch] = ml;
  }
}

void FertManager::resetStock(uint8_t ch, float ml) {
  if (_isValidChannel(ch)) {
    _stockML[ch] = ml;
    saveState();
    Serial.printf("[Fert] Stock CH%d reset to %.1f ml\n", ch + 1, ml);
  }
}

void FertManager::setLowStockThreshold(uint8_t ch, float ml) {
  if (_isValidChannel(ch) && ml >= 0) {
    _lowStockThreshold[ch] = ml;
    saveState();
    Serial.printf("[Fert] CH%d low stock threshold set to %.0f mL\n", ch + 1,
                  ml);
  }
}

float FertManager::getLowStockThreshold(uint8_t ch) const {
  return _isValidChannel(ch) ? _lowStockThreshold[ch] : 50.0f;
}

bool FertManager::isLowStock(uint8_t ch) const {
  if (!_isValidChannel(ch))
    return false;
  return _stockML[ch] < _lowStockThreshold[ch] && _lowStockThreshold[ch] > 0;
}

String FertManager::getName(uint8_t ch) const {
  if (_isValidChannel(ch)) {
    return _names[ch];
  }
  return "";
}

void FertManager::setName(uint8_t ch, const String &name) {
  if (_isValidChannel(ch)) {
    // Truncate name to save NVS space (max 15 chars)
    String safeName = name.substring(0, 15);
    _names[ch] = safeName;
    saveState();
    Serial.printf("[Fert] CH%d renamed to '%s'\n", ch + 1, safeName.c_str());
  }
}

bool FertManager::isEnabled(uint8_t ch) const {
  return _isValidChannel(ch) ? _enabled[ch] : false;
}

void FertManager::setEnabled(uint8_t ch, bool enabled) {
  if (_isValidChannel(ch)) {
    _enabled[ch] = enabled;
    saveState();
    Serial.printf("[Fert] CH%d schedule set to %s\n", ch + 1, enabled ? "ENABLED" : "DISABLED");
  }
}

struct FertChannelData {
  float doseML[7];
  uint8_t schedHour[7];
  uint8_t schedMinute[7];
  float stockML;
  float flowRateMLps;
  float lowStockThreshold;
  uint32_t lastDoseKey;
  uint8_t pwm;
  bool enabled;
};

void FertManager::resetChannel(uint8_t ch) {
  if (!_isValidChannel(ch))
    return;

  // Stop this channel's pump before the numbers behind it are erased. Only this
  // channel: abortDose() would also cut a manual run on a different one, and a
  // reset of CH2 has no business stopping CH3.
  if (_doseActive && _doseChannel == ch) {
    ledcWrite(ch, 0);
    _doseActive = false;
    Serial.printf("[Fert] CH%d dose stopped by config reset\n", ch + 1);
  }
  if (_manualActive && _manualChannel == ch) {
    ledcWrite(ch, 0);
    _manualActive = false;
  }

  _applyDefaults(ch);

  // The pre-blob keys are what made a reset necessary in the first place, so a
  // reset that left them behind would not be one.
  _removeLegacyKeys(ch);

  saveState();
  Serial.printf("[Fert] CH%d configuration reset to defaults\n", ch + 1);
}

void FertManager::saveState() {
  for (uint8_t i = 0; i < NUM_FERTS + 1; i++) {
    char key[16];
    snprintf(key, sizeof(key), "name%d", i);
    _prefs.putString(key, _names[i]);

    FertChannelData data;
    for (uint8_t d = 0; d < 7; d++) {
      data.doseML[d] = _doseML[i][d];
      data.schedHour[d] = _schedHour[i][d];
      data.schedMinute[d] = _schedMinute[i][d];
    }
    data.stockML = _stockML[i];
    data.flowRateMLps = _flowRateMLps[i];
    data.lowStockThreshold = _lowStockThreshold[i];
    data.lastDoseKey = _lastDoseKey[i];
    data.pwm = _pwm[i];
    data.enabled = _enabled[i];

    snprintf(key, sizeof(key), "ch%d", i);
    _prefs.putBytes(key, &data, sizeof(FertChannelData));
  }
}

bool FertManager::wasDosedToday(DateTime now) const {
  // Simplification: return true if CH1 was dosed today (telemetry mostly)
  return _dateKey(now) == _lastDoseKey[0];
}

// ============================================================================
// PRIVATE
// ============================================================================

uint32_t FertManager::_dateKey(DateTime dt) const {
  // Unique key per day: year * 1000 + dayOfYear
  // dayOfYear approximation using month*31+day (good enough for dedup)
  return (uint32_t)dt.year() * 1000 + (uint32_t)dt.month() * 31 +
         (uint32_t)dt.day();
}

void FertManager::_applyDefaults(uint8_t ch) {
  for (uint8_t d = 0; d < 7; d++) {
    _doseML[ch][d] = 0.0f; // Default 0 for all days to prevent accidental dosing
    _schedHour[ch][d] = DEFAULT_FERT_HOUR;
    _schedMinute[ch][d] = DEFAULT_FERT_MINUTE;
  }
  _stockML[ch] = DEFAULT_STOCK_ML;
  _names[ch] = (ch < NUM_FERTS) ? String("CH") + String(ch + 1) : "Prime";
  _lastDoseKey[ch] = 0;
  _flowRateMLps[ch] = FLOW_RATE_ML_PER_SEC; // Default 1.5 mL/s
  _pwm[ch] = 255;
  _lowStockThreshold[ch] = 50.0f; // Default low stock warning at 50 mL
  _enabled[ch] = true;
}

void FertManager::_removeLegacyKeys(uint8_t ch) {
  char key[16];
  const char *scalarKeys[] = {"dose%d", "sD%d",  "stock%d", "lk%d", "sH%d",
                              "sM%d",   "fR%d",  "pwm%d",   "lt%d", "en%d"};
  for (const char *fmt : scalarKeys) {
    snprintf(key, sizeof(key), fmt, ch);
    _prefs.remove(key);
  }
  for (uint8_t d = 0; d < 7; d++) {
    snprintf(key, sizeof(key), "d%d_%d", ch, d);
    _prefs.remove(key);
    snprintf(key, sizeof(key), "sH%d_%d", ch, d);
    _prefs.remove(key);
    snprintf(key, sizeof(key), "sM%d_%d", ch, d);
    _prefs.remove(key);
  }
}

void FertManager::_loadState() {
  bool migrated = false;

  for (uint8_t i = 0; i < NUM_FERTS + 1; i++) {
    char key[16];
    
    snprintf(key, sizeof(key), "name%d", i);
    String defaultName = (i < NUM_FERTS) ? String("CH") + String(i + 1) : "Prime";
    _names[i] = _prefs.getString(key, defaultName);

    snprintf(key, sizeof(key), "ch%d", i);
    FertChannelData data;
    if (_prefs.getBytes(key, &data, sizeof(FertChannelData)) == sizeof(FertChannelData)) {
      for (uint8_t d = 0; d < 7; d++) {
        _doseML[i][d] = data.doseML[d];
        _schedHour[i][d] = data.schedHour[d];
        _schedMinute[i][d] = data.schedMinute[d];
      }
      _stockML[i] = data.stockML;
      _flowRateMLps[i] = data.flowRateMLps;
      _lowStockThreshold[i] = data.lowStockThreshold;
      _lastDoseKey[i] = data.lastDoseKey;
      _pwm[i] = data.pwm;
      _enabled[i] = data.enabled;
    } else {
      // Backwards Compatibility: read the pre-blob keys, then clear them in one
      // pass at the end. Reading first matters — the per-day keys fall back on
      // the single-value ones, which have to still be there when they do.
      snprintf(key, sizeof(key), "dose%d", i);
      float legacyDose = _prefs.getFloat(key, (i == NUM_FERTS) ? DEFAULT_PRIME_ML : DEFAULT_DOSE_ML);

      snprintf(key, sizeof(key), "sD%d", i);
      uint8_t legacyMask = _prefs.getUChar(key, 127);

      for (uint8_t d = 0; d < 7; d++) {
        snprintf(key, sizeof(key), "d%d_%d", i, d);
        float defaultDose = ((legacyMask & (1 << d)) != 0) ? legacyDose : 0.0f;
        _doseML[i][d] = _prefs.getFloat(key, defaultDose);
      }

      snprintf(key, sizeof(key), "stock%d", i);
      _stockML[i] = _prefs.getFloat(key, DEFAULT_STOCK_ML);

      snprintf(key, sizeof(key), "lk%d", i);
      _lastDoseKey[i] = _prefs.getUInt(key, 0);

      snprintf(key, sizeof(key), "sH%d", i);
      uint8_t legacyHour = _prefs.getUChar(key, DEFAULT_FERT_HOUR);

      snprintf(key, sizeof(key), "sM%d", i);
      uint8_t legacyMin = _prefs.getUChar(key, DEFAULT_FERT_MINUTE);

      for (uint8_t d = 0; d < 7; d++) {
        snprintf(key, sizeof(key), "sH%d_%d", i, d);
        _schedHour[i][d] = _prefs.getUChar(key, legacyHour);
        snprintf(key, sizeof(key), "sM%d_%d", i, d);
        _schedMinute[i][d] = _prefs.getUChar(key, legacyMin);
      }

      snprintf(key, sizeof(key), "fR%d", i);
      _flowRateMLps[i] = _prefs.getFloat(key, FLOW_RATE_ML_PER_SEC);

      snprintf(key, sizeof(key), "pwm%d", i);
      _pwm[i] = _prefs.getUChar(key, 255);

      snprintf(key, sizeof(key), "lt%d", i);
      _lowStockThreshold[i] = _prefs.getFloat(key, 50.0f);

      snprintf(key, sizeof(key), "en%d", i);
      _enabled[i] = _prefs.getBool(key, true);

      _removeLegacyKeys(i);

      // Saving here would write a blob for every channel, including the ones
      // this loop has not read yet — their next getBytes() would then find that
      // blob, take the constructor defaults as real settings and never look at
      // the legacy keys. Only channel 0 would survive the upgrade. Flag it and
      // save once, after every channel has been migrated.
      migrated = true;
    }
  }

  // Force save the new struct blobs now that we've migrated and cleared keys
  if (migrated)
    saveState();
}

void FertManager::_markDosed(uint8_t ch, DateTime now) {
  if (_isValidChannel(ch)) {
    _lastDoseKey[ch] = _dateKey(now);

    char key[16];
    snprintf(key, sizeof(key), "lk%d", ch);
    _prefs.putUInt(key, _lastDoseKey[ch]);
  }
}

uint8_t FertManager::_pinForChannel(uint8_t ch) const {
  if (ch < NUM_FERTS) {
    return FERT_PINS[ch];
  }
  if (ch == NUM_FERTS) {
    return PIN_PRIME;
  }
  return 0; // Invalid
}
