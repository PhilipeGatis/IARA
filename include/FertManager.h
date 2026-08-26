#pragma once

#include "Config.h"
#include <Arduino.h>
#include <Preferences.h> // ESP32 NVS
#include <RTClib.h>

/// @brief Manages fertilizer dosing with NVS deduplication and stock tracking.
#pragma once

#include "Config.h"
#include <Arduino.h>
#include <Preferences.h> // ESP32 NVS
#include <RTClib.h>      // DateTime

/// @brief Manages fertilizer dosing with NVS deduplication and stock tracking.
class FertManager {
public:
  FertManager();

  /// Initialize NVS and load saved state
  void begin();

  /// Check schedule and dose if needed. Should be called periodically.
  /// @param now Current DateTime from TimeManager
  void update(DateTime now);

  /// Manually dose a specific channel
  /// @param ch Channel index 0-3 (fertilizers) or 4 (prime)
  /// @param ml Volume in mL
  /// @return true if dosing completed without timeout
  /// Starts a dose and returns immediately; the pump is switched off by
  /// tickDose(). The blocking form this replaced sat in a delay() loop for the
  /// dose duration — up to 30 s for a fertiliser and 60 s for Prime — during
  /// which loop() did not run, so the overflow watchdog, the emergency drain
  /// and the TPA state machine were all frozen while a pump was running.
  ///
  /// Returns false if the channel is invalid, the volume is not positive, or a
  /// dose is already in progress: the channels share one measurement of
  /// elapsed time, and two at once is a dosing error waiting to happen.
  bool startDose(uint8_t ch, float ml);

  /// Switches the pump off once the dose duration has elapsed. Must be called
  /// every loop, including in maintenance mode and during a TPA — it is the
  /// only thing that ends a dose.
  void tickDose();

  bool isDosing() const { return _doseActive; }

  /// Stops an in-progress dose immediately, mid-volume.
  void abortDose();

  /// Manually turn the pump ON or OFF for priming the line
  void manualPump(uint8_t ch, bool state);

  // ---- Dose volumes (per day of week, 0=Sun..6=Sat) ----
  void setDoseML(uint8_t ch, uint8_t dayOfWeek, float ml);
  float getDoseML(uint8_t ch, uint8_t dayOfWeek) const;

  // ---- Scheduling Config per day of week (NVS) ----
  void setScheduleTime(uint8_t ch, uint8_t day, uint8_t hour, uint8_t minute);
  /// Set same time for all 7 days (convenience)
  void setScheduleTimeAll(uint8_t ch, uint8_t hour, uint8_t minute);
  uint8_t getSchedHour(uint8_t ch, uint8_t day) const {
    return (_isValidChannel(ch) && day < 7) ? _schedHour[ch][day] : 0;
  }
  uint8_t getSchedMinute(uint8_t ch, uint8_t day) const {
    return (_isValidChannel(ch) && day < 7) ? _schedMinute[ch][day] : 0;
  }

  // ---- Flow Rate Calibration (NVS) ----
  void setFlowRate(uint8_t ch, float mlPerSec);
  float getFlowRate(uint8_t ch) const {
    return _isValidChannel(ch) ? _flowRateMLps[ch] : 1.5f;
  }

  // ---- Stock tracking ----
  float getStockML(uint8_t ch) const;
  void setStockML(uint8_t ch, float ml);
  void resetStock(uint8_t ch, float ml);

  // Enable/Disable Schedule
  bool isEnabled(uint8_t ch) const;
  void setEnabled(uint8_t ch, bool enabled);

  // ---- Low stock threshold (per channel, NVS) ----
  void setLowStockThreshold(uint8_t ch, float ml);
  float getLowStockThreshold(uint8_t ch) const;
  bool isLowStock(uint8_t ch) const;

  // ---- PWM Control (NVS) ----
  void setPWM(uint8_t ch, uint8_t pwm);
  uint8_t getPWM(uint8_t ch) const {
    return _isValidChannel(ch) ? _pwm[ch] : 255;
  }

  // ---- Custom Names (NVS) ----
  String getName(uint8_t ch) const;
  void setName(uint8_t ch, const String &name);

  /// Save stock levels and names to NVS
  void saveState();

  /// Was today's dose already applied?
  bool wasDosedToday(DateTime now) const;

private:
  Preferences _prefs;

  // Dose volumes per channel (CH1-CH4 ferts + CH5 prime)
  // [Channel][DayOfWeek] where 0 = Sunday, 6 = Saturday
  float _doseML[NUM_FERTS + 1][7];

  // Remaining stock per channel
  float _stockML[NUM_FERTS + 1];

  // Custom names per channel
  String _names[NUM_FERTS + 1];

  // Last dose date (day of year * 1000 + year) for dedup (per channel)
  uint32_t _lastDoseKey[NUM_FERTS + 1];
  bool _enabled[NUM_FERTS + 1];

  // Low stock warning threshold per channel (mL)
  float _lowStockThreshold[NUM_FERTS + 1];

  // Schedule config (per day of week: 0=Sun..6=Sat)
  uint8_t _schedHour[NUM_FERTS + 1][7];
  uint8_t _schedMinute[NUM_FERTS + 1][7];

  // Calibrated flow rate (mL per second)
  float _flowRateMLps[NUM_FERTS + 1];

  // PWM Configuration (0-255)
  uint8_t _pwm[NUM_FERTS + 1];

  /// Compute unique key for a date (for NVS dedup)
  uint32_t _dateKey(DateTime dt) const;

  /// Load state from NVS
  void _loadState();

  /// Mark today as dosed for a channel in NVS
  void _markDosed(uint8_t ch, DateTime now);

  /// Check if a channel index is valid (0-NUM_FERTS inclusive) (DRY #7)
  bool _isValidChannel(uint8_t ch) const { return ch <= NUM_FERTS; }

  // In-progress dose. Only one at a time; see startDose().
  bool _doseActive = false;
  uint8_t _doseChannel = 0;
  unsigned long _doseEndMs = 0;

  /// Get the GPIO pin for a channel (0-3 = fert, 4 = prime)
  uint8_t _pinForChannel(uint8_t ch) const;
};
