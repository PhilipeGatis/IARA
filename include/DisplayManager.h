#pragma once

#include "Config.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Arduino.h>
#include <SPI.h>

// Forward declarations
class TimeManager;
class WaterManager;
class FertManager;
class SafetyWatchdog;
class SafetyWatchdog;
class WebManager;
class NotifyManager;

/// @brief Manages the ST7735 TFT display with auto-cycling pages.
class DisplayManager {
public:
  DisplayManager();

  /// Early hardware init — call before WiFi to show boot screen immediately
  bool initHardware();

  /// Show a boot progress line on the display (call during setup steps)
  void showBootStatus(const char *line1, const char *line2 = nullptr);

  /// Full initialization with manager pointers (call after all managers ready)
  void begin(TimeManager *time, WaterManager *water, FertManager *fert,
             SafetyWatchdog *safety, WebManager *web, NotifyManager *notify);

  /// Update display — call from loop(). Cycles pages every PAGE_CYCLE_MS.
  void update();

private:
  Adafruit_ST7735 _display;

  static constexpr uint8_t SCREEN_WIDTH = 128;
  static constexpr uint8_t SCREEN_HEIGHT = 160;

  // ── Color Palette ──
  static constexpr uint16_t COL_BG = 0x0000;     // Black
  static constexpr uint16_t COL_TEXT = 0xFFFF;   // White
  static constexpr uint16_t COL_DIM = 0xAD55;    // Light gray
  static constexpr uint16_t COL_ACCENT = 0x04FF; // Cyan-blue
  static constexpr uint16_t COL_GOOD = 0x07E0;   // Green
  static constexpr uint16_t COL_WARN = 0xFFE0;   // Yellow
  static constexpr uint16_t COL_ERR = 0xF800;    // Red
  static constexpr uint16_t COL_BAR_BG = 0x4208; // Dark gray
  static constexpr uint16_t COL_SEL = 0x2104;    // Selection highlight bg

  // Manager pointers
  TimeManager *_time;
  WaterManager *_water;
  FertManager *_fert;
  SafetyWatchdog *_safety;
  WebManager *_web;
  NotifyManager *_notify;

  // Page cycling
  uint8_t _currentPage;
  unsigned long _lastPageSwitch;
  unsigned long _lastRedraw;
  static constexpr uint8_t NUM_PAGES = 4;
  static constexpr unsigned long PAGE_CYCLE_MS = 5000; // switch page every 5s
  static constexpr unsigned long REDRAW_MS = 1000; // refresh content every 1s
  uint8_t _bootLine;                               // boot log line counter

  // ── Button state ──
  bool _btnLastState;        // previous digitalRead (true=released)
  unsigned long _btnPressTs; // millis when button was pressed
  bool _btnHandled;          // true if current press already acted on
  static constexpr unsigned long BTN_DEBOUNCE_MS = 50;
  static constexpr unsigned long BTN_LONG_MS = 3000;

  // ── Display on/off ──
  bool _displayOn;
  unsigned long _lastInteraction; // millis of last button press

  // ── Menu state ──
  bool _inMenu;
  uint8_t _menuItem;
  static constexpr uint8_t MENU_ITEMS = 2;

  // Button handling
  void _readButton();

  /// True when a real GPIO is assigned to the navigation button.
  /// PIN_BTN == 255 means disabled (D19 is used by the reservoir float switch).
  static constexpr bool _btnEnabled() { return PIN_BTN != 255; }

  // Page drawing methods
  void _drawNetworkPage();
  void _drawAquariumPage();
  void _drawAquariumPageLive(); // flicker-free partial redraw
  void _drawStockPage();
  void _drawSchedulePage();
  void _drawMenuPage();
  void _switchToPage(uint8_t page);
  void _executeMenuItem();
  void _displayOff();

  // Helper
  void _drawHeader(const char *title);
  void _drawHeaderLevelBar();
  void _drawHeaderTitle(const char *title);

  /// Calculate water level percentage from ultrasonic distance (DRY helper)
  float _calcWaterPct(float distCm) const;
};
