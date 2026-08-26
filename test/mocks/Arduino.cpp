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
  if (pin < NUM_MOCK_PINS) {
    mock_pin_state[pin] = val;
    mock_pin_read_value[pin] = val; // Also update read value for output pins
  }
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

// ---- Serial ----
MockSerial Serial;
MockSerial Serial2;

// ---- A02YYUW UART Frame Helper ----
void mock_inject_a02_distance(float distanceCm) {
  uint16_t distMm = (uint16_t)(distanceCm * 10.0f);
  uint8_t header = 0xFF;
  uint8_t dataH = (distMm >> 8) & 0xFF;
  uint8_t dataL = distMm & 0xFF;
  uint8_t checksum = (header + dataH + dataL) & 0xFF;
  uint8_t frame[] = { header, dataH, dataL, checksum };
  Serial2.mock_inject(frame, 4);
}

// ---- WiFi ----
MockWiFiClass WiFi;

// ---- LEDC (PWM) stubs ----
// Track which pin each LEDC channel drives, so ledcWrite() moves mock_pin_state
// the way the real peripheral moves the pad. Without this the mock cannot tell
// a stopped dosing pump from a running one — and stopping them is exactly what
// the emergency paths have to prove.
static int mock_ledc_pin[NUM_MOCK_PINS];
static bool mock_ledc_init = false;

void ledcSetup(uint8_t channel, double freq, uint8_t resolution) {
  if (!mock_ledc_init) {
    for (int i = 0; i < NUM_MOCK_PINS; i++) mock_ledc_pin[i] = -1;
    mock_ledc_init = true;
  }
}

void ledcAttachPin(uint8_t pin, uint8_t channel) {
  if (channel < NUM_MOCK_PINS) mock_ledc_pin[channel] = pin;
}

void ledcWrite(uint8_t channel, uint32_t duty) {
  if (channel < NUM_MOCK_PINS && mock_ledc_pin[channel] >= 0) {
    mock_pin_state[mock_ledc_pin[channel]] = duty > 0 ? HIGH : LOW;
  }
}

void ledcDetachPin(uint8_t pin) {
  for (int i = 0; i < NUM_MOCK_PINS; i++) {
    if (mock_ledc_pin[i] == pin) mock_ledc_pin[i] = -1;
  }
}
