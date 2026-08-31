#pragma once

#include "Config.h"
#include "PumpLog.h"
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

  /// Runs today's scheduled doses now, without waiting for their hour.
  ///
  /// What the schedule would have delivered anyway: the volume configured for
  /// today's day of week, on enabled channels that have the stock for it.
  ///
  /// @param includeDosed also re-dose channels that already ran today. Off by
  ///        default, because a repeated button press must not be able to double
  ///        a dose by accident — it is only ever true when the person asking
  ///        said so explicitly, which is a legitimate thing to want after a
  ///        dose that visibly delivered nothing.
  ///
  /// The doses do not start here. They are handed to update(), which owns the
  /// stagger between pump starts and the one path that books stock and stamps
  /// the day, so a hand-fired dose behaves exactly like a scheduled one.
  ///
  /// @return how many channels are going to dose
  uint8_t doseTodayNow(DateTime now, bool includeDosed = false);

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
  /// Returns false if the channel is invalid, the volume is not positive, or
  /// that same channel is already dosing. Other channels are free to run at the
  /// same time: each drives its own LEDC channel and its own pin, and each
  /// tracks its own end time.
  /// @param reason What to record in the pump log for this dose.
  bool startDose(uint8_t ch, float ml,
                 PumpReason reason = PumpReason::FERT_MANUAL);

  /// Switches the pump off once the dose duration has elapsed. Must be called
  /// every loop, including in maintenance mode and during a TPA — it is the
  /// only thing that ends a dose.
  void tickDose();

  /// Is any channel dosing?
  bool isDosing() const;
  /// Is this particular channel dosing?
  bool isDosing(uint8_t ch) const {
    return _isValidChannel(ch) && _doseActive[ch];
  }

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

  // ---- Bottle capacity (mL, NVS) ----
  /// How much the channel's bottle holds when full. Only the stock bar and the
  /// run-out estimate read it — dosing never does — but a wrong value makes a
  /// 450 mL bottle report 90 % the moment it is opened.
  float getCapacityML(uint8_t ch) const;
  void setCapacityML(uint8_t ch, float ml);

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

  /// Wipe one channel's configuration back to factory defaults and persist it.
  /// Clears doses, per-day times, stock, flow rate, PWM, name, threshold, the
  /// enable flag and the "already dosed today" stamp, and drops any leftover
  /// pre-blob NVS keys for the channel. Stops the channel's pump first.
  void resetChannel(uint8_t ch);

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

  // Full-bottle volume per channel
  float _capacityML[NUM_FERTS + 1];

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

  /// Put one channel's settings back to their factory values (RAM only).
  void _applyDefaults(uint8_t ch);

  /// Drop every pre-blob NVS key belonging to a channel.
  void _removeLegacyKeys(uint8_t ch);

  /// Mark today as dosed for a channel in NVS
  void _markDosed(uint8_t ch, DateTime now);

  /// Check if a channel index is valid (0-NUM_FERTS inclusive) (DRY #7)
  bool _isValidChannel(uint8_t ch) const { return ch <= NUM_FERTS; }

  // Today's schedule, released early by doseTodayNow(). Holds the date key so
  // a request that survives midnight — the board was mid-water-change, say —
  // dies with the day it belonged to instead of firing into the next one.
  uint32_t _forceDoseKey = 0;

  // Which channels that request covers, one bit each. A set of channels rather
  // than a flag because "dose everything again" has to reach channels whose
  // day is already stamped, and that stamp is the only thing standing between
  // a schedule and a double dose — so it is bypassed per channel, once, for
  // the channels someone deliberately named, and the bit is cleared the moment
  // the dose starts.
  uint8_t _forceMask = 0;

  // When the last pump was switched on, for the stagger. Separate flag because
  // millis() is legitimately near zero right after boot.
  unsigned long _lastPumpStartMs = 0;
  bool _pumpStarted = false;

  // A manual run has no scheduled end, so it gets a hard ceiling instead.
  bool _manualActive[NUM_FERTS + 1] = {};
  unsigned long _manualEndMs[NUM_FERTS + 1] = {};

  // In-progress doses, one slot per channel.
  //
  // These were single values, which made the whole dosing side serial: three
  // channels scheduled for the same minute ran one after another, and any that
  // did not get its turn before the minute elapsed was skipped for the day
  // without a word. Nothing in the hardware asked for that — the pumps sit on
  // separate LEDC channels behind separate MOSFETs.
  bool _doseActive[NUM_FERTS + 1] = {};
  unsigned long _doseEndMs[NUM_FERTS + 1] = {};
  PumpReason _doseReason[NUM_FERTS + 1] = {}; ///< what to log on the OFF

  /// Get the GPIO pin for a channel (0-3 = fert, 4 = prime)
  uint8_t _pinForChannel(uint8_t ch) const;
};
