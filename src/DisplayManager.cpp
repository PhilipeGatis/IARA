#include "DisplayManager.h"
#include "FertManager.h"
#include "SafetyWatchdog.h"
#include "TimeManager.h"
#include "WaterManager.h"
#include "WebManager.h"
#include "NotifyManager.h"
#include <WiFi.h>

// =============================================================================
// I18N — Display strings indexed by language (0=PT, 1=EN, 2=JA)
// =============================================================================
static const char *STR_NETWORK[] = {"REDE", "NETWORK", "NETWORK"};
static const char *STR_AQUARIUM[] = {"AQUARIO", "AQUARIUM", "AQUARIUM"};
static const char *STR_STOCK[] = {"ESTOQUE", "STOCK", "STOCK"};
static const char *STR_SCHEDULE[] = {"AGENDA", "SCHEDULE", "SCHEDULE"};
static const char *STR_CONNECTED[] = {"CONECTADO", "CONNECTED", "CONNECTED"};
static const char *STR_DISCONNECTED[] = {"DESCONECTADO", "DISCONNECTED",
                                         "DISCONNECTED"};
static const char *STR_WATER_LEVEL[] = {"NIVEL DA AGUA", "WATER LEVEL",
                                        "WATER LEVEL"};
static const char *STR_NEXT_TPA[] = {"PROXIMA TPA", "NEXT WC", "NEXT WC"};
static const char *STR_DAYS[] = {"dias", "days", "days"};

// Menu strings
static const char *STR_MENU[] = {"MENU", "MENU", "MENU"};
static const char *STR_MENU_TPA[] = {"INICIAR TPA", "START WC", "START WC"};
static const char *STR_MENU_MAINT_ON[] = {"MANUTENCAO ON", "MAINT. ON",
                                          "MAINT. ON"};
static const char *STR_MENU_MAINT_OFF[] = {"MANUTENCAO OFF", "MAINT. OFF",
                                           "MAINT. OFF"};

// =============================================================================
// PER-CHANNEL BAR COLORS (unique hue per fertilizer + prime)
// =============================================================================
// Using 565 RGB colors for visual distinction
static const uint16_t CHANNEL_COLORS[] = {
    0x07FF, // CH1: Cyan
    0xF81F, // CH2: Magenta
    0xFFE0, // CH3: Yellow
    0xFD20, // CH4: Orange
    0x07E0, // Prime: Green
};

// =============================================================================
// CONSTRUCTOR
// =============================================================================
DisplayManager::DisplayManager()
    : _display(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_MOSI, PIN_TFT_SCK, PIN_TFT_RST),
      _time(nullptr), _water(nullptr), _fert(nullptr), _safety(nullptr),
      _web(nullptr), _currentPage(0), _lastPageSwitch(0), _lastRedraw(0),
      _bootLine(0), _btnLastState(true), _btnPressTs(0), _btnHandled(false),
      _displayOn(true), _lastInteraction(0), _inMenu(false), _menuItem(0) {}

// =============================================================================
// INIT HARDWARE
// =============================================================================
bool DisplayManager::initHardware() {
  // Button (skipped when disabled — D19 is used by the reservoir float switch)
  if (_btnEnabled()) {
    pinMode(PIN_BTN, INPUT_PULLUP);
  }

  _display.initR(INITR_BLACKTAB);
  _display.setRotation(3); // Landscape: 160×128 (rotated 180°)
  Serial.println("[Display] ST7735 128x160 initialized OK.");

  // Color splash screen
  _display.fillScreen(COL_BG);
  _display.setTextSize(3);
  _display.setTextColor(COL_ACCENT);
  uint8_t tw = 4 * 18; // "IARA" = 4 chars × 18px
  _display.setCursor((160 - tw) / 2, 30);
  _display.print(F("IARA"));

  _display.setTextSize(1);
  _display.setTextColor(COL_DIM);
  _display.setCursor(55, 70);
  _display.print(F("v3.0.0"));

  // Accent bar
  _display.fillRect(30, 90, 100, 2, COL_ACCENT);

  _display.setTextColor(COL_GOOD);
  _display.setCursor(35, 100);
  _display.print(F("Aquarium Control"));

  _lastInteraction = millis();

  return true;
}

// =============================================================================
// SHOW BOOT STATUS
// =============================================================================
void DisplayManager::showBootStatus(const char *msg, const char *detail) {
  const uint8_t lineH = 12;
  const uint8_t startY = 26;
  const uint8_t maxLines = 8; // (128 - 26) / 12 = 8 lines

  // First call: draw header
  if (_bootLine == 0) {
    _display.fillScreen(COL_BG);

    // Header bar
    _display.fillRect(0, 0, 160, 22, COL_ACCENT);
    _display.setTextSize(1);
    _display.setTextColor(COL_BG);
    _display.setCursor(4, 7);
    _display.print(F("IARA > Boot"));
  }

  // Mark previous line as OK (if any)
  if (_bootLine > 0 && _bootLine <= maxLines) {
    uint8_t prevY = startY + (_bootLine - 1) * lineH;
    _display.setCursor(140, prevY);
    _display.setTextSize(1);
    _display.setTextColor(COL_GOOD, COL_BG);
    _display.print(F("OK"));
  }

  // Add new log line
  if (_bootLine < maxLines) {
    uint8_t y = startY + _bootLine * lineH;
    _display.setTextSize(1);
    _display.setTextColor(COL_TEXT, COL_BG);
    _display.setCursor(4, y);
    _display.print(F("> "));
    _display.print(msg);

    if (detail) {
      _display.setTextColor(COL_DIM, COL_BG);
      _display.setCursor(16, y + lineH);
      _display.print(detail);
      _bootLine++; // detail takes an extra line
    }
  }

  _bootLine++;
}

// =============================================================================
// BEGIN
// =============================================================================
void DisplayManager::begin(TimeManager *time, WaterManager *water,
                           FertManager *fert, SafetyWatchdog *safety,
                           WebManager *web, NotifyManager *notify) {
  _time = time;
  _water = water;
  _fert = fert;
  _safety = safety;
  _web = web;
  _notify = notify;
  _lastPageSwitch = millis();
  _lastInteraction = millis();
}

// =============================================================================
// UPDATE — cycles pages, locks on TPA page when water change is running
// =============================================================================
void DisplayManager::update() {
  // --- Button handling first ---
  _readButton();

  unsigned long now = millis();

  // Blank the panel after a period with no button press. The backlight is wired
  // straight to 3.3V on this module and cannot be switched, so this only clears
  // the pixels — pressing the button in _readButton() brings the page back.
  // Never blank while a water change is running: that is exactly when someone
  // walking past needs to see the state.
  if (_displayOn && !_water->isRunning() &&
      (now - _lastInteraction >= DISPLAY_TIMEOUT_MS)) {
    _displayOff();
    return;
  }

  if (!_displayOn)
    return; // screen is off, nothing to draw

  // --- Menu mode ---
  if (_inMenu) {
    // Menu exits on 5s inactivity
    if (now - _lastInteraction >= 5000) {
      _inMenu = false;
      _lastPageSwitch = now; // force full page redraw
      _switchToPage(_currentPage);
    }
    return; // menu is static, no auto-refresh needed
  }

  // --- Lock on aquarium page while TPA is running ---
  bool tpaRunning = _water->isRunning();
  if (tpaRunning) {
    _currentPage = 1;
    if (now - _lastPageSwitch >= PAGE_CYCLE_MS) {
      _lastPageSwitch = now;
      _lastRedraw = now;
      _display.fillRect(0, 24, 160, 104, COL_BG);
      uint8_t lang = _web->getLanguage();
      _drawHeaderTitle(STR_AQUARIUM[lang]);
      _drawHeaderLevelBar();
      _drawAquariumPage();
      return;
    }
    if (now - _lastRedraw >= REDRAW_MS) {
      _lastRedraw = now;
      _drawAquariumPageLive();
    }
    return;
  }

  // --- Normal auto-cycle pages ---
  bool pageSwitch = (now - _lastPageSwitch >= PAGE_CYCLE_MS);
  if (pageSwitch) {
    _lastPageSwitch = now;
    _lastRedraw = now;
    _currentPage = (_currentPage + 1) % NUM_PAGES;
    _switchToPage(_currentPage);
    return;
  }

  // --- Partial redraw every 1s ---
  if (now - _lastRedraw >= REDRAW_MS) {
    _lastRedraw = now;
    _drawHeaderLevelBar();
    if (_currentPage == 3) {
      DateTime dt = _time->now();
      _display.setTextSize(3);
      _display.setTextColor(COL_TEXT, COL_BG);
      char timeBuf[9];
      snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", dt.hour(),
               dt.minute(), dt.second());
      uint8_t tw = 8 * 18;
      _display.setCursor((160 - tw) / 2, 32);
      _display.print(timeBuf);
    } else if (_currentPage == 1) {
      _drawAquariumPageLive();
    }
  }
}

// =============================================================================
// BUTTON — debounce + short/long press detection
// =============================================================================
void DisplayManager::_readButton() {
  // Bail out when the button is disabled: digitalRead() on an invalid GPIO
  // returns 0 (LOW), which reads as "held down" and would auto-open the menu.
  if (!_btnEnabled()) {
    return;
  }

  bool btnNow = digitalRead(PIN_BTN); // LOW = pressed (active low)
  unsigned long now = millis();

  // Detect press edge (HIGH → LOW)
  if (_btnLastState && !btnNow) {
    // Button just pressed
    _btnPressTs = now;
    _btnHandled = false;
  }

  // While held: check for long press
  if (!btnNow && !_btnHandled && (now - _btnPressTs >= BTN_LONG_MS)) {
    _btnHandled = true;
    _lastInteraction = now;

    if (!_displayOn) {
      // Wake up display
      _displayOn = true;
      _switchToPage(_currentPage);
    } else if (_inMenu) {
      // Long press in menu = execute
      _executeMenuItem();
    } else {
      // Long press on pages = open menu
      _inMenu = true;
      _menuItem = 0;
      _drawMenuPage();
    }
  }

  // Detect release edge (LOW → HIGH)
  if (!_btnLastState && btnNow) {
    unsigned long held = now - _btnPressTs;
    if (!_btnHandled && held >= BTN_DEBOUNCE_MS && held < BTN_LONG_MS) {
      // Short press
      _lastInteraction = now;

      if (!_displayOn) {
        // Wake display
        _displayOn = true;
        _switchToPage(_currentPage);
      } else if (_inMenu) {
        // Next menu item
        _menuItem = (_menuItem + 1) % MENU_ITEMS;
        _drawMenuPage();
      } else {
        // Next page
        _currentPage = (_currentPage + 1) % NUM_PAGES;
        _lastPageSwitch = now;
        _lastRedraw = now;
        _switchToPage(_currentPage);
      }
    }
  }

  _btnLastState = btnNow;
}

// =============================================================================
// SWITCH TO PAGE — full redraw of a specific page
// =============================================================================
void DisplayManager::_switchToPage(uint8_t page) {
  _display.fillRect(0, 24, 160, 104, COL_BG);
  uint8_t lang = _web->getLanguage();
  const char *pageNames[] = {STR_NETWORK[lang], STR_AQUARIUM[lang],
                             STR_STOCK[lang], STR_SCHEDULE[lang]};
  _drawHeaderTitle(pageNames[page]);
  _drawHeaderLevelBar();

  switch (page) {
  case 0:
    _drawNetworkPage();
    break;
  case 1:
    _drawAquariumPage();
    break;
  case 2:
    _drawStockPage();
    break;
  case 3:
    _drawSchedulePage();
    break;
  }
}

// =============================================================================
// DISPLAY OFF — fill black
// =============================================================================
void DisplayManager::_displayOff() {
  _displayOn = false;
  _inMenu = false;
  _display.fillScreen(COL_BG);
}

// =============================================================================
// MENU PAGE — simple list with selection highlight
// =============================================================================
void DisplayManager::_drawMenuPage() {
  uint8_t lang = _web ? _web->getLanguage() : 0;

  // Full redraw of menu
  _display.fillRect(0, 0, 160, 128, COL_BG);

  // Header
  _display.fillRect(0, 0, 160, 22, COL_WARN);
  _display.setTextSize(1);
  _display.setTextColor(COL_BG);
  _display.setCursor(4, 7);
  _display.print(STR_MENU[lang]);
  _display.drawFastHLine(0, 23, 160, COL_BAR_BG);

  const char *items[MENU_ITEMS];
  items[0] = STR_MENU_TPA[lang];
  items[1] = _safety->isMaintenanceMode() ? STR_MENU_MAINT_OFF[lang]
                                          : STR_MENU_MAINT_ON[lang];

  const uint8_t itemH = 28;
  const uint8_t startY = 34;

  for (uint8_t i = 0; i < MENU_ITEMS; i++) {
    uint8_t y = startY + i * (itemH + 4);
    bool selected = (i == _menuItem);

    if (selected) {
      _display.fillRoundRect(6, y, 148, itemH, 4, COL_SEL);
      _display.drawRoundRect(6, y, 148, itemH, 4, COL_ACCENT);
    } else {
      _display.drawRoundRect(6, y, 148, itemH, 4, COL_BAR_BG);
    }

    _display.setTextSize(1);
    _display.setTextColor(selected ? COL_ACCENT : COL_DIM);
    _display.setCursor(16, y + 10);
    _display.print(items[i]);
  }

  // Footer hint
  _display.setTextSize(1);
  _display.setTextColor(COL_DIM);
  _display.setCursor(4, 110);
  _display.print(F("Click=Next  Hold=OK"));
}

// =============================================================================
// EXECUTE MENU ITEM
// =============================================================================
void DisplayManager::_executeMenuItem() {
  switch (_menuItem) {
  case 0: // Start TPA
    _water->startTPA();
    Serial.println("[Btn] TPA started from display");
    break;
  case 1: // Toggle maintenance
    if (_safety->isMaintenanceMode()) {
      _safety->exitMaintenance();
      Serial.println("[Btn] Maintenance OFF from display");
    } else {
      _safety->enterMaintenance();
      Serial.println("[Btn] Maintenance ON from display");
    }
    break;
  }

  // Exit menu and go back to pages
  _inMenu = false;
  _lastPageSwitch = millis();
  _switchToPage(_currentPage);
}

// =============================================================================
// HEADER — accent bar with title + water level bar
// =============================================================================
void DisplayManager::_drawHeader(const char *title) {
  // Full header: accent bar + title + level bar + separator
  _display.fillRect(0, 0, 160, 22, COL_ACCENT);
  _display.setTextSize(1);
  _display.setTextColor(COL_BG);
  _display.setCursor(4, 7);
  _display.print(title);

  _drawHeaderLevelBar();
  _display.drawFastHLine(0, 23, 160, COL_BAR_BG);
}

// =============================================================================
// HEADER TITLE — partial update (overwrites on accent background)
// =============================================================================
void DisplayManager::_drawHeaderTitle(const char *title) {
  _display.setTextSize(1);
  _display.setTextColor(COL_BG, COL_ACCENT); // text on accent = no flicker
  _display.setCursor(4, 7);
  // Pad to 12 chars to clear previous longer title
  char buf[13];
  snprintf(buf, sizeof(buf), "%-12s", title);
  _display.print(buf);
}

// =============================================================================
// HEADER LEVEL BAR — partial-redraw safe (redraws on accent background)
// =============================================================================
void DisplayManager::_drawHeaderLevelBar() {
  _display.setTextSize(1); // reset — previous page may have set size 3
  float dist = _safety->getLastDistance();
  const float maxDist = 30.0f;
  uint8_t barX = 80;
  uint8_t barW = 70;
  uint8_t barH = 10;
  uint8_t barY = 6;

  // Clear bar area on accent background
  _display.fillRect(barX, barY, barW, barH, COL_ACCENT);

  if (dist >= 0) {
    float pct = _calcWaterPct(dist) / 100.0f; // header bar uses 0-1 range

    _display.drawRect(barX, barY, barW, barH, COL_BG);
    uint8_t fillW = (uint8_t)(pct * (barW - 2));
    uint16_t fillCol =
        pct > 0.5f ? COL_GOOD : (pct > 0.2f ? COL_WARN : COL_ERR);
    if (fillW > 0) {
      _display.fillRect(barX + 1, barY + 1, fillW, barH - 2, fillCol);
    }

    int pctVal = (int)(pct * 100);
    char buf[5];
    snprintf(buf, sizeof(buf), "%d%%", pctVal);
    uint8_t textW = strlen(buf) * 6;
    uint8_t tx = barX + (barW - textW) / 2;
    _display.setTextColor(COL_BG);
    _display.setCursor(tx, barY + 1);
    _display.print(buf);
  } else {
    _display.setCursor(80, 7);
    _display.setTextColor(COL_BG);
    _display.print(F("Level: --"));
  }

  // Thin separator line
  _display.drawFastHLine(0, 23, 160, COL_BAR_BG);
}

// =============================================================================
// PAGE 1 — Network Info
// =============================================================================
void DisplayManager::_drawNetworkPage() {
  uint8_t y = 30;
  uint8_t lang = _web->getLanguage();

  if (WiFi.status() == WL_CONNECTED) {
    // Status indicator
    _display.fillCircle(12, y + 4, 5, COL_GOOD);
    _display.setTextColor(COL_GOOD);
    _display.setTextSize(1);
    _display.setCursor(22, y);
    _display.print(STR_CONNECTED[lang]);

    // IP in large font
    _display.setTextSize(2);
    _display.setTextColor(COL_TEXT);
    y += 18;
    _display.setCursor(4, y);
    _display.print(WiFi.localIP());

    // SSID + RSSI
    _display.setTextSize(1);
    _display.setTextColor(COL_DIM);
    y += 22;
    _display.setCursor(4, y);
    _display.print(WiFi.SSID());

    y += 14;
    _display.setCursor(4, y);
    _display.print(F("RSSI: "));
    int rssi = WiFi.RSSI();
    _display.setTextColor(rssi > -60 ? COL_GOOD
                                     : (rssi > -80 ? COL_WARN : COL_ERR));
    _display.print(rssi);
    _display.print(F(" dBm"));
    
    // ntfy.sh topic
    if (_notify && _notify->isEnabled()) {
      y += 14;
      _display.setTextSize(1);
      _display.setTextColor(COL_DIM);
      _display.setCursor(4, y);
      _display.print(F("Topic: "));
      _display.setTextColor(COL_ACCENT);
      // Truncate if too long (max ~15 chars fits)
      String topic = _notify->getTopic();
      if (topic.length() > 14) {
        topic = topic.substring(0, 12) + "..";
      }
      _display.print(topic);
    }
  } else {
    _display.fillCircle(12, y + 4, 5, COL_ERR);
    _display.setTextColor(COL_ERR);
    _display.setTextSize(1);
    _display.setCursor(22, y);
    _display.print(STR_DISCONNECTED[lang]);

    _display.setTextSize(2);
    _display.setTextColor(COL_TEXT);
    y += 20;
    _display.setCursor(4, y);
    _display.print(F("AP Mode"));

    _display.setTextSize(1);
    _display.setTextColor(COL_DIM);
    y += 22;
    _display.setCursor(4, y);
    _display.print(WiFi.softAPIP());
  }
}

// =============================================================================
// PAGE 2 — Aquarium Status
// =============================================================================
void DisplayManager::_drawAquariumPage() {
  uint8_t y = 30;
  uint8_t lang = _web->getLanguage();
  float dist = _safety->getLastDistance();

  // Water level — large text
  _display.setTextSize(1);
  _display.setTextColor(COL_DIM);
  _display.setCursor(4, y);
  _display.print(STR_WATER_LEVEL[lang]);

  y += 12;
  _display.setTextSize(2);
  _display.setTextColor(COL_TEXT);
  _display.setCursor(4, y);
  if (dist >= 0) {
    int pctVal = (int)_calcWaterPct(dist);
    _display.print(pctVal);
    _display.setTextSize(1);
    _display.print(F(" %"));
  } else {
    _display.print(F("-- %"));
  }

  // TPA state
  y += 26;
  _display.setTextSize(1);
  _display.setTextColor(COL_DIM);
  _display.setCursor(4, y);
  _display.print(F("TPA"));

  y += 12;
  const char *state = _water->getStateName();
  // Color code by state
  uint16_t stateCol = COL_GOOD; // IDLE default
  if (_water->isRunning())
    stateCol = COL_WARN;
  if (_water->getState() == TPAState::ERROR)
    stateCol = COL_ERR;

  _display.setTextSize(2);
  _display.setTextColor(stateCol);
  _display.setCursor(4, y);
  _display.print(state);

  // Canister status
  y += 24;
  _display.setTextSize(1);
  _display.setTextColor(COL_DIM);
  _display.setCursor(4, y);
  _display.print(F("CANISTER: "));
  bool canOn = _water->isCanisterOn();
  _display.setTextColor(canOn ? COL_GOOD : COL_ERR);
  _display.print(canOn ? F("ON") : F("OFF"));
}

// =============================================================================
// PAGE 2 LIVE — Flicker-free partial redraw of dynamic values
// =============================================================================
void DisplayManager::_drawAquariumPageLive() {
  float dist = _safety->getLastDistance();

  // Water level value (y=42, size 2) — overwrite in place
  _display.setTextSize(2);
  _display.setCursor(4, 42);
  if (dist >= 0) {
    int pctVal = (int)_calcWaterPct(dist);
    char lvlBuf[8];
    snprintf(lvlBuf, sizeof(lvlBuf), "%-4d", pctVal);
    _display.setTextColor(COL_TEXT, COL_BG);
    _display.print(lvlBuf);
    _display.setTextSize(1);
    _display.setTextColor(COL_TEXT, COL_BG);
    _display.print(F("%  "));
  } else {
    _display.setTextColor(COL_TEXT, COL_BG);
    _display.print(F("-- %  "));
  }

  // TPA state (y=80, size 2)
  const char *state = _water->getStateName();
  uint16_t stateCol = COL_GOOD;
  if (_water->isRunning())
    stateCol = COL_WARN;
  if (_water->getState() == TPAState::ERROR)
    stateCol = COL_ERR;

  _display.setTextSize(2);
  _display.setTextColor(stateCol, COL_BG);
  _display.setCursor(4, 80);
  // Pad to 10 chars to clear previous longer state names
  char stateBuf[11];
  snprintf(stateBuf, sizeof(stateBuf), "%-10s", state);
  _display.print(stateBuf);

  // Canister status (y=104, size 1)
  bool canOn = _water->isCanisterOn();
  _display.setTextSize(1);
  _display.setCursor(64, 104); // after "CANISTER: " label
  _display.setTextColor(canOn ? COL_GOOD : COL_ERR, COL_BG);
  _display.print(canOn ? F("ON ") : F("OFF"));

  // Pump Progress (y=116, size 1)
  float goal = _water->getPumpGoalLiters();
  if (goal > 0) {
    float prog = _water->getPumpProgressLiters();
    unsigned long ms = _water->getPumpElapsedMs();
    uint8_t sec = (ms / 1000) % 60;
    uint8_t min = (ms / 60000);
    char buf[26];
    snprintf(buf, sizeof(buf), "%.1f/%.1fL %02d:%02d      ", prog, goal, min, sec);
    _display.setCursor(4, 116);
    _display.setTextColor(COL_ACCENT, COL_BG);
    _display.print(buf);
  } else {
    _display.setCursor(4, 116);
    _display.setTextColor(COL_BG, COL_BG);
    _display.print(F("                      ")); // clear line
  }
}

// =============================================================================
// PAGE 3 — Fertilizer & Prime Stock (unique color per channel)
// =============================================================================
void DisplayManager::_drawStockPage() {
  const uint8_t numBars = NUM_FERTS + 1;
  const uint8_t barW = 22;
  const uint8_t gap = (160 - numBars * barW) / (numBars + 1);
  const uint8_t barTop = 40; // pushed down to avoid header overlap
  const uint8_t barH = 60;

  for (uint8_t i = 0; i < numBars; i++) {
    uint8_t x = gap + i * (barW + gap);
    float stock = _fert->getStockML(i);
    float maxStock = DEFAULT_STOCK_ML;
    float pct = stock / maxStock;
    if (pct > 1.0f)
      pct = 1.0f;
    if (pct < 0.0f)
      pct = 0.0f;
    uint8_t fillH = (uint8_t)(pct * barH);

    // Unique color per channel
    uint16_t chColor = CHANNEL_COLORS[i % 5];

    // Bar outline in channel color
    _display.drawRect(x, barTop, barW, barH, chColor);

    // Fill bar with channel color (dimmed outline, bright fill)
    if (fillH > 0) {
      _display.fillRect(x + 1, barTop + barH - fillH, barW - 2, fillH, chColor);
    }

    // Percentage label above bar
    int pctVal = (int)(pct * 100);
    char buf[5];
    snprintf(buf, sizeof(buf), "%d%%", pctVal);
    uint8_t tw = strlen(buf) * 6;
    _display.setTextSize(1);
    _display.setTextColor(chColor);
    _display.setCursor(x + (barW - tw) / 2, barTop - 10);
    _display.print(buf);

    // Channel name below bar (from saved config)
    String name = _fert->getName(i);
    if (name.length() == 0) {
      if (i < NUM_FERTS) {
        name = "F" + String(i + 1);
      } else {
        name = "PR";
      }
    }
    // Truncate to fit bar width (max 3 chars at size 1)
    if (name.length() > 3) {
      name = name.substring(0, 3);
    }
    _display.setTextColor(chColor);
    uint8_t lw = name.length() * 6;
    _display.setCursor(x + (barW - lw) / 2, barTop + barH + 4);
    _display.print(name);

    // Stock mL below name
    _display.setTextColor(COL_DIM);
    char mlBuf[8];
    snprintf(mlBuf, sizeof(mlBuf), "%.0f", stock);
    uint8_t mlW = strlen(mlBuf) * 6;
    _display.setCursor(x + (barW - mlW) / 2, barTop + barH + 14);
    _display.print(mlBuf);
  }
}

// =============================================================================
// PAGE 4 — Schedules
// =============================================================================
void DisplayManager::_drawSchedulePage() {
  uint8_t y = 32;
  uint8_t lang = _web->getLanguage();

  // Current time — large
  DateTime now = _time->now();
  _display.setTextSize(3);
  _display.setTextColor(COL_TEXT);

  char timeBuf[9];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", now.hour(), now.minute(),
           now.second());
  uint8_t tw = 8 * 18; // 8 chars × 18px (size 3)
  _display.setCursor((160 - tw) / 2, y);
  _display.print(timeBuf);

  // TPA schedule
  y += 36;
  _display.setTextSize(1);
  _display.setTextColor(COL_DIM);
  _display.setCursor(4, y);
  _display.print(STR_NEXT_TPA[lang]);

  y += 14;
  uint8_t th = _web->getTpaHour();
  uint8_t tm = _web->getTpaMinute();
  uint16_t interval = _web->getTpaInterval();

  _display.setTextSize(2);
  _display.setTextColor(COL_ACCENT);
  _display.setCursor(4, y);

  char tpaBuf[12];
  snprintf(tpaBuf, sizeof(tpaBuf), "%02d:%02d", th, tm);
  _display.print(tpaBuf);

  if (interval > 0) {
    _display.setTextSize(1);
    _display.setTextColor(COL_DIM);
    _display.setCursor(70, y + 4);
    _display.print(F("/ "));
    _display.print(interval);
    _display.print(F(" "));
    _display.print(STR_DAYS[lang]);
  }
}

// =============================================================================
// WATER LEVEL PERCENTAGE (DRY #2)
// =============================================================================
float DisplayManager::_calcWaterPct(float distCm) const {
  float sf = (float)_web->getSensorFullDistanceMm() / 10.0f;
  float refCm = (_web->getAqHeight() > 0) ? (float)_web->getAqHeight() : 20.0f;
  float pct = 100.0f - (((distCm - sf) / refCm) * 100.0f);
  if (pct > 100.0f) pct = 100.0f;
  if (pct < 0.0f) pct = 0.0f;
  return pct;
}
