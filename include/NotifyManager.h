#pragma once

#include "Config.h"
#include "NotifyStrings.h"
#include <Arduino.h>

/// @brief Notification event types (each has independent cooldown)
enum NotifyType : uint8_t {
  NOTIFY_TPA_COMPLETE = 0,
  NOTIFY_TPA_ERROR,
  NOTIFY_FERT_LOW_STOCK,
  NOTIFY_EMERGENCY,
  NOTIFY_FERT_COMPLETE,
  NOTIFY_DAILY_LEVEL,
  NOTIFY_TYPE_COUNT // = 6
};

/// @brief ntfy.sh push notification manager with rate limiting and per-type
/// toggles.
class NotifyManager {
public:
  NotifyManager();

  /// Initialize — load config from NVS
  void begin();

  /// Call from loop — checks if daily report should be sent
  void update(uint8_t currentHour, uint8_t currentMinute);

  // ---- Typed notifications ----

  void notifyTPAComplete();
  void notifyTPAError(const char *reason);
  void notifyFertLowStock(uint8_t channel, float remainingML,
                          float thresholdML);
  void notifyEmergency(const char *reason);
  void notifyFertComplete(uint8_t channel, float doseML);
  void notifyDailyLevel(float levelCm);

  // ---- Configuration (persisted in NVS namespace "notify") ----

  void setTopic(const String &topic);
  String getTopic() const { return _topic; }
  bool isEnabled() const { return _topic.length() > 0; }

  void setLanguage(uint8_t lang) {
    _lang = (lang < LANG_COUNT) ? (Lang)lang : LANG_PT;
  }

  void setTypeEnabled(NotifyType type, bool on);
  bool isTypeEnabled(NotifyType type) const;

  void setDailyReportHour(uint8_t h, uint8_t m);
  uint8_t getDailyReportHour() const { return _dailyReportHour; }
  uint8_t getDailyReportMinute() const { return _dailyReportMinute; }

  uint16_t getDailyCount() const { return _dailyCount; }

  /// Send a manual test notification
  void sendTest();

  // ---- For unit tests (native env) ----
#ifdef UNIT_TEST
  bool lastSendResult() const { return _lastSendResult; }
  void resetDailyCount() { _dailyCount = 0; }
#endif

private:
  String _topic;
  Lang _lang;
  bool _typeEnabled[NOTIFY_TYPE_COUNT];
  uint8_t _dailyReportHour;
  uint8_t _dailyReportMinute;
  bool _dailyReportSent;

  // Rate limiting
  unsigned long _lastNotifyMs[NOTIFY_TYPE_COUNT];
  uint16_t _dailyCount;
  uint32_t _lastResetDay; // day-of-year for daily counter reset

  // Cooldown: 5 minutes between same notification type
  static constexpr unsigned long NOTIFY_COOLDOWN_MS = 5UL * 60 * 1000;
  // Max notifications per day
  static constexpr uint16_t MAX_DAILY_NOTIFICATIONS = 20;

  /// Check if a notification of given type can be sent (rate limiting)
  bool _canSend(NotifyType type);

  /// Send notification via ntfy.sh HTTPS API
  /// @return true if sent successfully
  bool _send(NotifyType type, const char *title, const char *message,
             const char *priority, const char *tags);

  void _loadConfig();
  void _saveConfig();

#ifdef UNIT_TEST
  bool _lastSendResult;
#endif
};
