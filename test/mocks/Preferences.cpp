#include "Preferences.h"
#include "Arduino.h"

std::map<std::string, Preferences::MockValue> Preferences::_store;
std::map<std::string, std::string> Preferences::_strStore;

void Preferences::putString(const char *key, const String &val) {
  _strStore[_makeKey(key)] = val.c_str();
}

String Preferences::getString(const char *key, const String &defaultVal) {
  auto it = _strStore.find(_makeKey(key));
  return (it != _strStore.end()) ? String(it->second.c_str()) : defaultVal;
}

String Preferences::getString(const char *key, const char *defaultVal) {
  auto it = _strStore.find(_makeKey(key));
  return (it != _strStore.end()) ? String(it->second.c_str())
                                 : String(defaultVal ? defaultVal : "");
}
std::map<std::string, std::string> Preferences::_bytesStore;
