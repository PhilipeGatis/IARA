#pragma once
#include <Arduino.h>

/// Trying new WiFi credentials without a reboot.
///
/// The dashboard used to save the credentials and restart, so a typo in the
/// password left the board on its AP until someone fixed it from there. Now
/// the new network is tried first, from loop(), and only saved once the
/// station has an IP. On failure the board goes back to the network it had.
///
/// The radio work runs in loop() (wifiTestTick) and never in the async web
/// handler, which must not block.
enum class WifiTestState : uint8_t { IDLE, SCANNING, CONNECTING, OK, FAILED };

/// Queues a try of these credentials. False if one is already running.
bool wifiTestRequest(const String &ssid, const String &pass);

/// True while a try owns the radio; the recovery ladder stands down meanwhile.
bool wifiTestBusy();

/// Advances the try. Call once per loop().
void wifiTestTick();

/// {"state":"idle|scanning|connecting|ok|failed","ssid":..,"ip":..,
///  "reason":N,"elapsedMs":N}
String wifiTestJson();
