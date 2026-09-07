#pragma once
// ============================================================================
// esp_system.h Mock for Native Unit Tests
// ============================================================================
// Only the reset-reason enum, which BootLog persists as a raw byte. The
// numbering is fixed by the IDF ABI and must match esp_system.h exactly: a
// mismatch here would silently relabel every entry already stored on a device.

typedef enum {
  ESP_RST_UNKNOWN = 0,
  ESP_RST_POWERON = 1,
  ESP_RST_EXT = 2,
  ESP_RST_SW = 3,
  ESP_RST_PANIC = 4,
  ESP_RST_INT_WDT = 5,
  ESP_RST_TASK_WDT = 6,
  ESP_RST_WDT = 7,
  ESP_RST_DEEPSLEEP = 8,
  ESP_RST_BROWNOUT = 9,
  ESP_RST_SDIO = 10,
} esp_reset_reason_t;
