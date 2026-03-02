#pragma once
// ============================================================================
// Arduino.h Mock for Native Unit Tests
// Provides minimal stubs of Arduino functions so firmware classes compile on
// x86
// ============================================================================

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// ---- Types ----
typedef uint8_t byte;
typedef bool boolean;

// ---- Constants ----
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

// ---- Math helpers ----
#ifndef min
template <typename T> T min(T a, T b) { return (a < b) ? a : b; }
#endif
#ifndef max
template <typename T> T max(T a, T b) { return (a > b) ? a : b; }
#endif

// ---- String class (Arduino-compatible) — MUST be before MockSerial ----
class String {
public:
  String() : _str() {}
  String(const char *s) : _str(s ? s : "") {}
  String(const std::string &s) : _str(s) {}
  String(int val) : _str(std::to_string(val)) {}
  String(float val) : _str(std::to_string(val)) {}

  const char *c_str() const { return _str.c_str(); }
  int length() const { return (int)_str.length(); }
  void trim() {
    auto start = _str.find_first_not_of(" \t\n\r");
    auto end = _str.find_last_not_of(" \t\n\r");
    if (start == std::string::npos)
      _str.clear();
    else
      _str = _str.substr(start, end - start + 1);
  }
  bool startsWith(const char *prefix) const {
    return _str.rfind(prefix, 0) == 0;
  }
  String substring(int from, int to = -1) const {
    if (to < 0)
      return String(_str.substr(from).c_str());
    return String(_str.substr(from, to - from).c_str());
  }
  bool endsWith(const char *suffix) const {
    std::string s(suffix);
    if (s.size() > _str.size())
      return false;
    return _str.compare(_str.size() - s.size(), s.size(), s) == 0;
  }
  int toInt() const { return atoi(_str.c_str()); }
  float toFloat() const { return (float)atof(_str.c_str()); }

  bool operator==(const char *other) const { return _str == other; }
  bool operator==(const String &other) const { return _str == other._str; }
  bool operator!=(const char *other) const { return _str != other; }
  bool operator!=(const String &other) const { return _str != other._str; }

  String operator+(const String &rhs) const {
    return String((_str + rhs._str).c_str());
  }
  String operator+(const char *rhs) const {
    return String((_str + std::string(rhs ? rhs : "")).c_str());
  }
  String operator+(int rhs) const {
    return String((_str + std::to_string(rhs)).c_str());
  }
  friend String operator+(const char *lhs, const String &rhs) {
    return String((std::string(lhs ? lhs : "") + rhs._str).c_str());
  }

private:
  std::string _str;
};

// ---- GPIO mock state ----
#define NUM_MOCK_PINS 40

extern uint8_t mock_pin_mode[NUM_MOCK_PINS];
extern uint8_t mock_pin_state[NUM_MOCK_PINS];
extern uint8_t mock_pin_read_value[NUM_MOCK_PINS];

void mock_reset_pins();

// ---- Arduino function stubs ----
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
uint8_t digitalRead(uint8_t pin);

// ---- Timing ----
extern unsigned long mock_millis_value;
unsigned long millis();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
void yield();

// ---- LEDC (PWM) stubs ----
void ledcSetup(uint8_t channel, double freq, uint8_t resolution);
void ledcAttachPin(uint8_t pin, uint8_t channel);
void ledcWrite(uint8_t channel, uint32_t duty);
void ledcDetachPin(uint8_t pin);

// ---- pulseIn ----
extern unsigned long mock_pulseIn_value;
unsigned long pulseIn(uint8_t pin, uint8_t state, unsigned long timeout);

// ---- Serial Mock ----
class MockSerial {
public:
  void begin(unsigned long baud) {}
  void print(const char *s) {}
  void print(int v, int base = 10) {}
  void print(unsigned int v, int base = 10) {}
  void print(float v, int decimals = 2) {}
  void print(char c) {}
  void println(const char *s = "") {}
  void println(int v, int base = 10) {}
  void println(unsigned int v, int base = 10) {}
  void printf(const char *fmt, ...) {}
  bool available() { return false; }
  String readStringUntil(char c) { return String(""); }
};

extern MockSerial Serial;

// ---- WiFi Mock ----
#define WL_CONNECTED 3

class MockWiFiClass {
public:
  int status() { return _status; }
  void begin(const char *ssid, const char *pass) { _status = WL_CONNECTED; }
  String localIP() { return String("192.168.1.100"); }

  int _status = WL_CONNECTED;
};

extern MockWiFiClass WiFi;
