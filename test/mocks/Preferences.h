#pragma once
// ============================================================================
// Preferences.h Mock for Native Unit Tests
// Simple in-memory key-value store mimicking ESP32 NVS
// ============================================================================

#include <cstdint>
#include <cstring>
#include <map>
#include <string>

#include "Arduino.h"

class Preferences {
public:
  void begin(const char *ns, bool readOnly = false) { _namespace = ns; }
  void end() {}

  // ---- Write ----
  void putUInt(const char *key, uint32_t val) {
    _store[_makeKey(key)].u32 = val;
  }
  void putFloat(const char *key, float val) { _store[_makeKey(key)].f = val; }
  void putUChar(const char *key, uint8_t val) {
    _store[_makeKey(key)].u8 = val;
  }
  void putBool(const char *key, bool val) {
    _store[_makeKey(key)].b = val;
  }
  void putString(const char *key, const char *val) {
    _strStore[_makeKey(key)] = val ? val : "";
  }
  void putString(const char *key, const String &val);

  // ---- Read ----
  uint32_t getUInt(const char *key, uint32_t defaultVal = 0) {
    auto it = _store.find(_makeKey(key));
    return (it != _store.end()) ? it->second.u32 : defaultVal;
  }
  float getFloat(const char *key, float defaultVal = 0.0f) {
    auto it = _store.find(_makeKey(key));
    return (it != _store.end()) ? it->second.f : defaultVal;
  }
  uint8_t getUChar(const char *key, uint8_t defaultVal = 0) {
    auto it = _store.find(_makeKey(key));
    return (it != _store.end()) ? it->second.u8 : defaultVal;
  }
  bool getBool(const char *key, bool defaultVal = false) {
    auto it = _store.find(_makeKey(key));
    return (it != _store.end()) ? it->second.b : defaultVal;
  }
  String getString(const char *key, const String &defaultVal = String());
  String getString(const char *key, const char *defaultVal);

  // ---- Mock control ----
  static void mock_clearAll() {
    _store.clear();
    _strStore.clear();
  }

private:
  std::string _namespace;

  union MockValue {
    uint32_t u32;
    float f;
    uint8_t u8;
    bool b;
    MockValue() : u32(0) {}
  };

  static std::map<std::string, MockValue> _store;
  static std::map<std::string, std::string> _strStore;

  std::string _makeKey(const char *key) { return _namespace + "." + key; }
};
