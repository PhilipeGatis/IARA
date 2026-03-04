#include "DisplayManager.h"
#include "FertManager.h"
#include "SafetyWatchdog.h"
#include "TimeManager.h"
#include "WaterManager.h"
#include "WebManager.h"
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
      _web(nullptr), _currentPage(0), _lastPageSwitch(0) {}

// =============================================================================
// INIT HARDWARE
// =============================================================================
bool DisplayManager::initHardware() {
  _display.initR(INITR_BLACKTAB);
  _display.setRotation(1); // Landscape: 160×128
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

  return true;
}

// =============================================================================
// SHOW BOOT STATUS
// =============================================================================
void DisplayManager::showBootStatus(const char *line1, const char *line2) {
  _display.fillScreen(COL_BG);

  // Header bar
  _display.fillRect(0, 0, 160, 20, COL_ACCENT);
  _display.setTextSize(1);
  _display.setTextColor(COL_BG);
  _display.setCursor(4, 6);
  _display.print(F("IARA > Boot"));

  // Status text
  _display.setTextColor(COL_GOOD);
  _display.setTextSize(1);
  _display.setCursor(8, 40);
  _display.print(line1);
  if (line2) {
    _display.setCursor(8, 58);
    _display.setTextColor(COL_DIM);
    _display.print(line2);
  }
}

// =============================================================================
// BEGIN
// =============================================================================
void DisplayManager::begin(TimeManager *time, WaterManager *water,
                           FertManager *fert, SafetyWatchdog *safety,
                           WebManager *web) {
  _time = time;
  _water = water;
  _fert = fert;
  _safety = safety;
  _web = web;
  _lastPageSwitch = millis();
}

// =============================================================================
// UPDATE — cycles pages, locks on TPA page when water change is running
// =============================================================================
void DisplayManager::update() {
  unsigned long now = millis();

  // Lock on aquarium page while TPA is running
  bool tpaRunning = _water->isRunning();
  if (tpaRunning) {
    // Force aquarium page, still respect timing to avoid flicker
    if (now - _lastPageSwitch < PAGE_CYCLE_MS) {
      return;
    }
    _lastPageSwitch = now;
    _currentPage = 1; // Aquarium page

    _display.fillScreen(COL_BG);
    uint8_t lang = _web->getLanguage();
    _drawHeader(STR_AQUARIUM[lang]);
    _drawAquariumPage();
    return;
  }

  if (now - _lastPageSwitch < PAGE_CYCLE_MS) {
    return;
  }
  _lastPageSwitch = now;

  _display.fillScreen(COL_BG);

  uint8_t lang = _web->getLanguage();
  const char *pageNames[] = {STR_NETWORK[lang], STR_AQUARIUM[lang],
                             STR_STOCK[lang], STR_SCHEDULE[lang]};
  _drawHeader(pageNames[_currentPage]);

  switch (_currentPage) {
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

  _currentPage = (_currentPage + 1) % NUM_PAGES;
}

// =============================================================================
// HEADER — accent bar with title + water level bar
// =============================================================================
void DisplayManager::_drawHeader(const char *title) {
  float dist = _safety->getLastDistance();
  const float maxDist = 30.0f;

  // Accent bar with title
  _display.fillRect(0, 0, 160, 22, COL_ACCENT);
  _display.setTextSize(1);
  _display.setTextColor(COL_BG);
  _display.setCursor(4, 7);
  _display.print(title);

  // Water level bar on the right side of header
  if (dist >= 0) {
    float pct = 1.0f - (dist / maxDist);
    if (pct > 1.0f)
      pct = 1.0f;
    if (pct < 0.0f)
      pct = 0.0f;

    uint8_t barX = 80;
    uint8_t barW = 70;
    uint8_t barH = 10;
    uint8_t barY = 6;
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
    _display.print(F("Nivel: --"));
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
    _display.print(dist, 1);
    _display.setTextSize(1);
    _display.print(F(" cm"));
  } else {
    _display.print(F("-- cm"));
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
