#pragma once
#include <cstdint>

/// @brief System language codes
enum Lang : uint8_t { LANG_PT = 0, LANG_EN = 1, LANG_JA = 2, LANG_COUNT = 3 };

/// @brief Notification string table for multi-language push messages
struct NotifyStrings {
  // Titles
  const char *tpaCompleteTitle;
  const char *tpaErrorTitle;
  const char *fertLowStockTitle;
  const char *emergencyTitle;
  const char *fertCompleteTitle;
  const char *dailyLevelTitle;
  const char *testTitle;

  // Message templates (use %s, %d, %.1f etc.)
  const char *tpaCompleteMsg;
  const char *tpaErrorFmt; // %s = reason
  const char
      *fertLowStockFmt;     // %d = channel, %.0f = remaining, %.0f = threshold
  const char *emergencyFmt; // %s = reason
  const char *fertCompleteFmt; // %d = channel, %.1f = dose
  const char *dailyLevelFmt;   // %.1f = level cm
  const char *testMsg;
};

// clang-format off
static const NotifyStrings NOTIFY_STRINGS[LANG_COUNT] = {
    // ---- LANG_PT (Portuguese) ----
    {
        "TPA Completa ✅", "Erro na TPA ❌", "Estoque Baixo ⚠️",
        "EMERGÊNCIA 🚨", "Fertilização OK 🧪", "Nível Diário 📊", "Teste IARA",
        "Troca parcial de água finalizada com sucesso.",
        "Erro durante TPA: %s",
        "Canal %d: %.0f mL restantes (limiar: %.0f mL). Reabasteça!",
        "ALERTA: %s",
        "Canal %d: %.1f mL dosado com sucesso.",
        "Nível atual: %.1f cm. Verifique evaporação.",
        "Notificação de teste do sistema."
    },
    // ---- LANG_EN (English) ----
    {
        "TPA Complete ✅", "TPA Error ❌", "Low Stock ⚠️",
        "EMERGENCY 🚨", "Fertilization OK 🧪", "Daily Level 📊", "IARA Test",
        "Partial water change completed successfully.",
        "TPA error: %s",
        "Channel %d: %.0f mL remaining (threshold: %.0f mL). Refill!",
        "ALERT: %s",
        "Channel %d: %.1f mL dosed successfully.",
        "Current level: %.1f cm. Check evaporation.",
        "System test notification."
    },
    // ---- LANG_JA (Japanese) ----
    {
        "TPA完了 ✅", "TPAエラー ❌", "在庫低下 ⚠️",
        "緊急 🚨", "施肥完了 🧪", "日次水位 📊", "IARAテスト",
        "換水が正常に完了しました。",
        "TPAエラー: %s",
        "CH%d: %.0f mL残 (閾値: %.0f mL)。補充してください!",
        "警告: %s",
        "CH%d: %.1f mL投与完了。",
        "現在の水位: %.1f cm。蒸発を確認。",
        "システムテスト通知。"
    },
};
// clang-format on
