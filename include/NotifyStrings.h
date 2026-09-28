#pragma once
#include <cstdint>

/// @brief System language codes
enum Lang : uint8_t { LANG_PT = 0, LANG_EN = 1, LANG_JA = 2, LANG_COUNT = 3 };

/// @brief Notification string table for multi-language push messages.
///
/// Titles carry no emoji: ntfy renders each X-Tags entry that names an emoji
/// in front of the title, so an emoji here showed up twice.
struct NotifyStrings {
  // Titles
  const char *tpaStartTitle;
  const char *tpaCompleteTitle;
  const char *tpaErrorTitle;
  const char *tpaSkippedTitle;
  const char *fertLowStockTitle;
  const char *emergencyTitle;
  const char *fertCompleteTitle;
  const char *dailyLevelTitle;
  const char *testTitle;

  // Message templates (use %s, %d, %.1f etc.)
  const char *tpaStartFmt;     // %.1f = liters, %d = percent
  const char *tpaCompleteMsg;
  const char *tpaErrorFmt;     // %s = reason
  const char *tpaSkippedFmt;   // %s = reason
  const char *fertLowStockFmt; // %s = name, %.0f = remaining, %.0f = threshold
  const char *emergencyMsg;
  const char *fertCompleteFmt; // %s = list of doses
  const char *dailyLevelFmt;   // %.1f = cm below full, %.1f = liters
  const char *dailyLevelNoSensorMsg;
  const char *testMsg;
};

// clang-format off
static const NotifyStrings NOTIFY_STRINGS[LANG_COUNT] = {
    // ---- LANG_PT (Portuguese) ----
    {
        "TPA iniciada", "TPA concluída", "Erro na TPA", "TPA não iniciou",
        "Estoque baixo", "EMERGÊNCIA", "Fertilização OK", "Nível diário",
        "Teste IARA",
        "Troca parcial de água começou: %.1f L (%d%% do aquário).",
        "Troca parcial de água concluída com sucesso.",
        "A TPA parou com erro: %s",
        "A TPA agendada não iniciou: %s",
        "%s: restam %.0f mL (alerta em %.0f mL). Reabasteça!",
        "O sistema entrou em modo de emergência. Verifique o aquário agora.",
        "Dosagem concluída: %s.",
        "Água %.1f cm abaixo do nível cheio (~%.1f L para completar).",
        "O sensor de nível está sem leitura válida.",
        "Notificação de teste do sistema."
    },
    // ---- LANG_EN (English) ----
    {
        "Water change started", "Water change complete", "Water change error",
        "Water change did not start", "Low stock", "EMERGENCY",
        "Fertilization OK", "Daily level", "IARA test",
        "Partial water change started: %.1f L (%d%% of the tank).",
        "Partial water change completed successfully.",
        "The water change stopped with an error: %s",
        "The scheduled water change did not start: %s",
        "%s: %.0f mL left (alert at %.0f mL). Refill!",
        "The system entered emergency mode. Check the aquarium now.",
        "Dosing done: %s.",
        "Water is %.1f cm below the full mark (~%.1f L to top up).",
        "The level sensor has no valid reading.",
        "System test notification."
    },
    // ---- LANG_JA (Japanese) ----
    {
        "換水開始", "換水完了", "換水エラー", "換水が開始されませんでした",
        "在庫低下", "緊急", "施肥完了", "日次水位", "IARAテスト",
        "換水を開始しました: %.1f L (水槽の%d%%)。",
        "換水が正常に完了しました。",
        "換水がエラーで停止しました: %s",
        "予定の換水が開始されませんでした: %s",
        "%s: 残り%.0f mL (警告 %.0f mL)。補充してください!",
        "緊急モードに入りました。すぐに水槽を確認してください。",
        "投与完了: %s。",
        "満水位より%.1f cm低いです (補充 約%.1f L)。",
        "水位センサーの読み取り値が無効です。",
        "システムテスト通知。"
    },
};
// clang-format on
