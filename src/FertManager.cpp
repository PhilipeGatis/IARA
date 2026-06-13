#include "FertManager.h"

FertManager::FertManager() {
  for (uint8_t i = 0; i < NUM_FERTS + 1; i++) {
    for (uint8_t d = 0; d < 7; d++) {
      _doseML[i][d] =
          0.0f; // Default 0 for all days to prevent accidental dosing
      _schedHour[i][d] = DEFAULT_FERT_HOUR;
      _schedMinute[i][d] = DEFAULT_FERT_MINUTE;
    }
    _stockML[i] = DEFAULT_STOCK_ML;
    _names[i] = (i < NUM_FERTS) ? String("CH") + String(i + 1) : "Prime";
    _lastDoseKey[i] = 0;
    _flowRateMLps[i] = FLOW_RATE_ML_PER_SEC; // Default 1.5 mL/s
    _pwm[i] = 255;
    _lowStockThreshold[i] = 50.0f; // Default low stock warning at 50 mL
    _enabled[i] = true;
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
          if (doseChannel(i, ds)) {
            _stockML[i] -= ds;
            if (_stockML[i] < 0)
              _stockML[i] = 0;
            _markDosed(i, now);
            saveState();
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

bool FertManager::doseChannel(uint8_t ch, float ml) {
  if (!_isValidChannel(ch))
    return false;
  if (ml <= 0)
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

  unsigned long start = millis();
  while ((millis() - start) < durationMs) {
    delay(10); // Yield to watchdog
  }

  ledcWrite(ch, 0);
  return true;
}

void FertManager::manualPump(uint8_t ch, bool state) {
  if (!_isValidChannel(ch))
    return;
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

void FertManager::saveState() {
  for (uint8_t i = 0; i < NUM_FERTS + 1; i++) {
    char key[16];

    for (uint8_t d = 0; d < 7; d++) {
      snprintf(key, sizeof(key), "d%d_%d", i, d); // e.g. d0_0, d0_6
      _prefs.putFloat(key, _doseML[i][d]);
    }

    snprintf(key, sizeof(key), "stock%d", i);
    _prefs.putFloat(key, _stockML[i]);

    snprintf(key, sizeof(key), "name%d", i);
    _prefs.putString(key, _names[i]);

    snprintf(key, sizeof(key), "lk%d", i); // Last key
    _prefs.putUInt(key, _lastDoseKey[i]);

    // Schedule Hours/Minutes per day of week
    for (uint8_t d = 0; d < 7; d++) {
      snprintf(key, sizeof(key), "sH%d_%d", i, d);
      _prefs.putUChar(key, _schedHour[i][d]);
      snprintf(key, sizeof(key), "sM%d_%d", i, d);
      _prefs.putUChar(key, _schedMinute[i][d]);
    }

    snprintf(key, sizeof(key), "fR%d", i); // Flow Rate
    _prefs.putFloat(key, _flowRateMLps[i]);

    snprintf(key, sizeof(key), "pwm%d", i); // PWM Config
    _prefs.putUChar(key, _pwm[i]);

    snprintf(key, sizeof(key), "lt%d", i); // Low stock Threshold
    _prefs.putFloat(key, _lowStockThreshold[i]);

    snprintf(key, sizeof(key), "en%d", i); // Enabled toggle
    _prefs.putBool(key, _enabled[i]);
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

void FertManager::_loadState() {
  for (uint8_t i = 0; i < NUM_FERTS + 1; i++) {
    char key[16];

    // Backwards Compatibility: Read legacy single-dose to use as fallback
    snprintf(key, sizeof(key), "dose%d", i);
    float legacyDose = _prefs.getFloat(key, (i == NUM_FERTS) ? DEFAULT_PRIME_ML
                                                             : DEFAULT_DOSE_ML);

    // If a channel was previously disabled via schedule bitmask, respect it
    snprintf(key, sizeof(key), "sD%d", i);
    uint8_t legacyMask = _prefs.getUChar(key, 127);

    for (uint8_t d = 0; d < 7; d++) {
      snprintf(key, sizeof(key), "d%d_%d", i, d);
      float defaultDose = ((legacyMask & (1 << d)) != 0) ? legacyDose : 0.0f;
      _doseML[i][d] = _prefs.getFloat(key, defaultDose);
    }

    snprintf(key, sizeof(key), "stock%d", i);
    _stockML[i] = _prefs.getFloat(key, DEFAULT_STOCK_ML);

    snprintf(key, sizeof(key), "name%d", i);
    String defaultName =
        (i < NUM_FERTS) ? String("CH") + String(i + 1) : "Prime";
    _names[i] = _prefs.getString(key, defaultName);

    snprintf(key, sizeof(key), "lk%d", i);
    _lastDoseKey[i] = _prefs.getUInt(key, 0);

    // Per-day schedule times with backward compat from legacy single keys
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
  }
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
