#include "Arduino.h"

// ---- GPIO mock state ----
uint8_t mock_pin_mode[NUM_MOCK_PINS] = {0};
uint8_t mock_pin_state[NUM_MOCK_PINS] = {0};
uint8_t mock_pin_read_value[NUM_MOCK_PINS] = {0};

void mock_reset_pins() {
  memset(mock_pin_mode, 0, sizeof(mock_pin_mode));
  memset(mock_pin_state, 0, sizeof(mock_pin_state));
  memset(mock_pin_read_value, 0, sizeof(mock_pin_read_value));
}

void pinMode(uint8_t pin, uint8_t mode) {
  if (pin < NUM_MOCK_PINS)
    mock_pin_mode[pin] = mode;
}

void digitalWrite(uint8_t pin, uint8_t val) {
  if (pin < NUM_MOCK_PINS)
    mock_pin_state[pin] = val;
}

uint8_t digitalRead(uint8_t pin) {
  if (pin < NUM_MOCK_PINS)
    return mock_pin_read_value[pin];
  return LOW;
}

// ---- Timing ----
unsigned long mock_millis_value = 0;
unsigned long millis() { return mock_millis_value; }
void delay(unsigned long ms) { mock_millis_value += ms; }
void delayMicroseconds(unsigned int us) { /* no-op */ }
void yield() { /* no-op */ }

// ---- pulseIn ----
unsigned long mock_pulseIn_value = 0;
unsigned long pulseIn(uint8_t pin, uint8_t state, unsigned long timeout) {
  return mock_pulseIn_value;
}

// ---- Serial ----
MockSerial Serial;

// ---- WiFi ----
MockWiFiClass WiFi;

// ---- LEDC (PWM) stubs ----
void ledcSetup(uint8_t channel, double freq, uint8_t resolution) {}
void ledcAttachPin(uint8_t pin, uint8_t channel) {}
void ledcWrite(uint8_t channel, uint32_t duty) {}
void ledcDetachPin(uint8_t pin) {}
