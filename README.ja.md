<p align="center">
  <img src="docs/logo.png" alt="IARA Logo" width="128" />
</p>

# IARA

**水槽用自動換水・施肥・ろ過システム** — ESP32ファームウェア。

*ブラジル民話の淡水の精霊「イアラ」にちなんで名付けられました。*

![PlatformIO CI](https://github.com/PhilipeGatis/IARA/actions/workflows/test.yml/badge.svg)

> 🇺🇸 Read in English: [README.md](README.md)
>
> 🇧🇷 Leia em português: [README.pt-BR.md](README.pt-BR.md)

---

## 📖 概要

**IARA**は水草水槽のための完全な組み込みオートメーションシステムです。以下を管理します：

- **換水（TPA）** — 6ステートのステートマシンによる自動排水・給水。
- **施肥** — ペリスタルティックポンプによるスケジュール投与・在庫追跡。
- **ろ過** — SSRリレー（AC電源）によるキャニスターのON/OFF制御。
- **安全** — 連続センサー監視、ウォッチドッグタイマー、緊急シャットダウンモード。

ファームウェアは**ESP32 DevKit V1**上で動作し、**組み込みWebダッシュボード**（React + Vite、LittleFSで配信）、**カラーTFTディスプレイ（ST7735）**、**DS3231 RTCクロック**、**シリアルコマンドインターフェース**を搭載しています。
ESP32のIPアドレスを調べる必要はなく、ローカルネットワーク上の **http://iara.local** (mDNS経由) にアクセスするだけで、簡単にWebインターフェースを利用できます。

---

## 📚 追加ドキュメント

プロジェクトには以下の詳細なハードウェアドキュメントが含まれています：

| ドキュメント | 説明 |
|---|---|
| [`BOM.md`](BOM.md) / [`BOM.ja.md`](BOM.ja.md) | **部品表** — AC、DC、アクチュエーター、センサー、保護、コネクタのレイヤー別全コンポーネントリスト。 |
| [`HARDWARE.md`](HARDWARE.md) / [`HARDWARE.ja.md`](HARDWARE.ja.md) | **ハードウェアアーキテクチャ** — 各レイヤーの配線図、ノイズ保護、超音波UART線の10kΩプルアップ、安全実装ノートを含む詳細技術資料。 |
| [`3d_models/`](3d_models/) | **3Dモデル (OpenSCAD)** — パラメトリック電子機器筐体 (`electronics_enclosure.scad`)、ドーシングポンプサポート (`dosing_pump_support.scad` / `dosing_pump_single.scad`)、ドリルテストプレート。 |

---

## 🏗️ ファームウェアアーキテクチャ

```
main.cpp               ← メインオーケストレーター
├── SafetyWatchdog      ← センサー + 緊急 + メンテナンスモード
├── TimeManager         ← DS3231 RTC + NTP同期
├── FertManager         ← 投与 + NVS重複排除 + 在庫追跡
├── WaterManager        ← 換水ステートマシン（6ステート）
├── DisplayManager      ← TFT ST7735 128×160 (SPI)
└── WebManager          ← 組み込みWebダッシュボード + シリアルI/F
```

### セーフティファーストループ

```cpp
loop() {
    safety.update();          // 🔴 最高優先度
    if (emergency) return;
    timeMgr.update();
    commands.process();
    schedules.check();
    waterMgr.update();        // 換水ステートマシン
    telemetry.send();
}
```

### 🔄 換水（TPA）フロー

TPAはメインループ内でノンブロッキングに動作する6ステートのステートマシンです。各ステートには安全のため設定可能なタイムアウトがあります。

```mermaid
stateDiagram-v2
    [*] --> FILLING_RESERVOIR: startTPA()
    FILLING_RESERVOIR --> DOSING_PRIME: フロートスイッチ
    DOSING_PRIME --> CANISTER_OFF: 2秒混合
    CANISTER_OFF --> DRAINING: 3秒安定化
    DRAINING --> REFILLING: レベル到達
    REFILLING --> CANISTER_ON: レベル到達
    CANISTER_ON --> [*]: 完了

    DRAINING --> ERROR: タイムアウト
    FILLING_RESERVOIR --> ERROR: タイムアウト
    REFILLING --> ERROR: タイムアウト
    ERROR --> [*]: レベル確認 → キャニスターON/OFF
```

| ステップ | ステート | 動作内容 |
|---|---|---|
| 1 | **FILLING_RESERVOIR** | ソレノイドバルブを開いてリザーバーに水道水を供給。満水になるとフロートスイッチがバルブを閉じます。すでに満水の場合は、バルブを開かずにこのステップをスキップします。 |
| 2 | **DOSING_PRIME** | ペリスタルティックポンプがリザーバーに脱塩素剤（Prime）を投与。2秒間混合を待機。在庫が減算されNVSに保存。 |
| 3 | **CANISTER_OFF** | キャニスターフィルターをOFF（SSRリレーHIGH）。超音波センサーが安定した読み取りを得られるよう3秒待機。 |
| 4 | **DRAINING** | 排水ポンプON。超音波センサーが水位を監視し、目標レベルに到達するまでポンプ稼働。自動キャリブレーション用に流量を測定。 |
| 5 | **REFILLING** | 給水ポンプON、処理済みの水をリザーバーから水槽へ送水。元のレベルに到達したように見えた時点でポンプを**3秒間停止**し、水面が落ち着いた状態で確認します。センサー直下への注水は、実際より高い水位として読み取られるためです。落ち着いた読み取りでもまだ不足していれば給水を再開します。最大水位に達した場合は、MOSFETチャンネルの信号線に直列接続されたリードスイッチがハードウェアでポンプを遮断します。キャリブレーション用に流量を測定。 |
| 6 | **CANISTER_ON** | キャニスターフィルターを再びON。**TPAサイクル完了。**キャリブレーション済み流量がNVSに保存。 |

> **リザーバーを最初に満たす理由：** この段階で起こりうる失敗（水道圧の不足、フロートの固着、ソレノイドの故障）は、水槽がまだ満水でフィルターも稼働している状態で発生します。先に排水すると、同じ失敗が「水位の低い水槽・停止したキャニスター・補充する処理済みの水がない」という状態を招きます。

**各ステップの安全性：**
- 各ステートにはキャリブレーション済み流量から計算される**動的タイムアウト**あり（`容量 / 流量 × 1.5`）。初回TPAは安全なデフォルト値を使用：**排水30秒、給水15秒**。
- GPIO33とMOSFETモジュールの入力IN7を結ぶ信号線に直列接続された**リードスイッチ**が、最大水位で給水ポンプを**ファームウェアとは独立して**遮断します。ESP32がハングしていても超音波センサーが沈黙していても機能します。[`HARDWARE.ja.md`](HARDWARE.ja.md) を参照。
- **緊急中止**はどの時点でも全アクチュエーターをOFFにし、キャニスターフィルターを復元。
- **エラー時**、超音波センサーで水位を確認してからキャニスターを再起動。水位が低すぎる場合（例：排水中のエラー）、キャニスターは**OFFのまま**維持し、空運転によるポンプ損傷を防止。

---

## 🔌 ハードウェア概要

### 主要接続

```mermaid
graph LR
  subgraph Power [⚡ 12V & 5V]
    PSU[12V電源] -->|12V| MOSFET[8chMOSFETモジュール]
    PSU -->|12V| LM2596[LM2596ステップダウン]
    LM2596 -->|5V| Cap[電解コンデンサ]
    Cap -->|5V| ESP32[ESP32 VIN]
    Cap -->|5V VCC| SENS[センサー]
  end

  subgraph Signals [🎮 ESP32信号]
    ESP32 -->|D21 SDA, D22 SCL| I2C[RTC DS3231]
    ESP32 -->|SPI D15,16,17,23| TFT[TFTディスプレイ ST7735]
    ESP32 -->|D12, D13, D14, D25-D27, D32, D33| MOSFET
    ESP32 -->|D2| SSR[Omron SSR]
    ESP32 ---|D34 RX| Ultra[超音波 A02YYUW UART]
    ESP32 ---|D5| Button[パネルボタン]
    ESP32 ---|D19| Float[フロートスイッチ]
  end

  MOSFET -->|OUT 1–5| Peristaltic[5xペリスタルティックポンプ]
  MOSFET -->|OUT 6| Drain[排水ポンプ]
  MOSFET -->|OUT 7| Refill[給水ポンプ]
  MOSFET -->|OUT 8| Solenoid[ソレノイドバルブ]
```

> 詳細な配線図と保護ノート（フライバックダイオード、スターGND）は [`HARDWARE.ja.md`](HARDWARE.ja.md) を参照してください。
>
> 全コンポーネントリストと数量は [`BOM.ja.md`](BOM.ja.md) を参照してください。

### ESP32 GPIO接続図

| GPIO | 機能 | コンポーネント | 方向 | プロトコル |
|------|------|----------------|------|------------|
| **D2** | キャニスターON/OFF | Omron SSRリレー | 出力 | デジタル |
| **D12** | 肥料CH2 | MOSFETチャンネル2 | 出力 | デジタル |
| **D13** | 肥料CH1 | MOSFETチャンネル1 | 出力 | デジタル |
| **D14** | 肥料CH3 | MOSFETチャンネル3 | 出力 | デジタル |
| **D15** | TFT CS | ST7735ディスプレイ (CS) | 出力 | SPI (CS) |
| **D16** | TFT SCK | ST7735ディスプレイ (SCK) | 出力 | SPI (SCK) |
| **D17** | TFT A0 (データ/コマンド) | ST7735ディスプレイ (A0) | 出力 | SPI (DC) |
| **D5** | ナビゲーションボタン | プッシュ/タクタイルボタン（パネル） | 入力 (PULLUP) | デジタル |
| **D18** | 超音波センサー制御線 | A02YYUW（制御線） | 出力 | Highで保持 |
| **D19** | リザーバーフロート | 水平フロートスイッチ | 入力 (PULLUP) | デジタル |
| **D21** | SDA | DS3231 RTC | 双方向 | I2C |
| **D22** | SCL | DS3231 RTC | 双方向 | I2C |
| **D23** | TFT SDA (データ) | ST7735ディスプレイ (SDA) | 出力 | SPI (MOSI) |
| **D25** | 排水ポンプ | MOSFETチャンネル6 | 出力 | デジタル |
| **D26** | プライム（カルキ抜き） | MOSFETチャンネル5 | 出力 | デジタル |
| **D27** | 肥料CH4 | MOSFETチャンネル4 | 出力 | デジタル |
| **D32** | ソレノイドバルブ | MOSFETチャンネル8 | 出力 | デジタル |
| **D33** | 給水ポンプ | MOSFETチャンネル7 | 出力 | デジタル |
| **D34** | 超音波 UART RX | A02YYUW | 入力 | デジタル (センサーのTXに接続) |
| **VIN** | 5V電源 | LM2596ステップダウン | — | 電源 |
| **EN** | 共有リセット | ST7735ディスプレイ (RESET) | — | リセット |
| **3.3V** | バックライト | ST7735ディスプレイ (LED) | — | 電源 |

---

## 🛡️ 安全性と信頼性

浸水、機器損傷、魚の損失を防止するため、**安全第一**のアプローチで設計されています。

### ソフトウェア保護

| 保護機能 | 説明 |
|---|---|
| **最大水位インターロック（ハードウェア）** | 給水チャンネルの信号線に直列接続されたリードスイッチ。ファームウェアを介さずにポンプを遮断します。ESP32がハングしても機能する唯一の保護です。 |
| **SafetyWatchdog** | ループの各イテレーションで最高優先度で実行。超音波センサーを監視し、緊急事態を検出して全アクチュエーターを停止します。誤検出を防ぐため、オーバーフローは10回連続の読み取りを経てから通知します。 |
| **ノンブロッキングループ** | すべての待機状態（キャニスター落ち着き、プライム混合）は`delay()`ではなく`millis()`を使用し、安全ウォッチドッグが待機中も動作し続けます。 |
| **ステートマシンタイムアウト** | TPAの各ステート（`DRAINING`、`FILLING`、`REFILLING`）には設定可能なタイムアウトがあります。超過するとエラー状態に入り、全アクチュエーターを停止します。 |
| **NVS重複排除** | 予期しない再起動後も同日の肥料二重投与を防止します。 |
| **緊急シャットダウン** | `emergency_stop`コマンドで全アクチュエーターを即座に停止します。 |
| **CPUスロットル** | メインループは約100Hz（`delay(10)`）で動作し、過熱を防止しWiFi/TCPスタックにCPUリソースを確保します。 |
| **ポンプ自動キャリブレーション** | TPA中に流量をインラインで測定（Δレベル × リットル/cm / Δ時間）。動的タイムアウト = `(容量 / 流量) × 1.5`。初回TPAは安全なデフォルト（排水30秒/給水15秒）を使用。 |
| **TPA必須設定** | すべての必須パラメータが設定されるまでTPAは開始不可：水槽寸法、リザーバー容量、交換%、キャニスター安全レベル%。無効な値での実行を防止。 |
| **キャニスター安全レベル（%）** | TPAエラー後にキャニスターを安全に再起動するために必要な最小水位（水槽高さの%）。水位がこの閾値を下回る場合（例：排水中のエラー）、キャニスターはOFFのまま維持し、空運転を防止。 |

### ハードウェア推奨事項

| 保護機能 | 説明 |
|---|---|
| **最大水位リードスイッチ** | 最大水位にEVAフロートで保持した磁石とNO（ノーマルオープン）リードスイッチを設置し、GPIO33とMOSFET入力IN7を結ぶ**信号線**に直列接続します（ポンプの電源に直列に入れてはいけません）。水位が上がりすぎた場合にファームウェアに依存せず物理的にポンプを遮断。[`HARDWARE.ja.md`](HARDWARE.ja.md) を参照。 |
| **フライバックダイオード** | ポンプにFR154、ソレノイドに1N5822 — 誘導性負荷の電圧スパイクを吸収。 |
| **デカップリングコンデンサ** | ESP32近傍に1000µF、MOSFETモジュール近傍に470µF — ブラウンアウト過渡現象を吸収。 |
| **直接3.3V UART** | 3.3V電源のA02YYUWは分圧回路なしで直接ESP32に接続。 |
| **サイフォン防止（排水）** | 排水ポンプと並列にソレノイドバルブを配線する（専用のフライバックダイオードが必要）か、水槽内のホースに小さな空気穴を開け、ポンプ停止後に重力で水が排出され続けるのを防ぎます。 |

---

## 📲 プッシュ通知

IARAは[ntfy.sh](https://ntfy.sh/)を通じてリアルタイムのプッシュ通知をスマートフォンに送信します。各通知タイプはダッシュボードで個別に有効/無効にできます。

### 通知タイプ

| タイプ | トリガー | メッセージ例 |
|---|---|---|
| **TPA完了** | 換水が正常に完了 | `✅ TPA completed successfully` |
| **TPAエラー** | TPA中のタイムアウトまたは障害 | `⚠️ Drain timeout \| Canister: OFF (nivel 45%, min: 60%)` |
| **肥料残量低下** | 在庫が閾値を下回る | `⚠️ CH1 Potássio: 15 mL remaining` |
| **緊急事態** | 緊急シャットダウン発動 | `🚨 Emergency shutdown activated!` |
| **投与完了** | 肥料の投与完了 | `💧 CH2 dosed 5.0 mL` |
| **日次水位** | 定期的な水位レポート | `📊 Water level: 12.3 cm` |

### レート制限

| パラメータ | 値 |
|---|---|
| タイプ別クールダウン | 5分（同一タイプは連続発火しない） |
| 1日の最大数 | 20通知 |
| カウンターリセット | 深夜（自動） |

### セットアップ

1. スマートフォンに**ntfy**アプリをダウンロードします（iOS/Android）
2. 「+」をタップして「トピックを購読」します
3. IARAが生成したトピック（水槽のディスプレイまたはダッシュボードに表示されます）を入力すれば完了です！
3. または、IARAダッシュボード（`通知 → ntfy.shトピック`）かAPIでトピックを設定します：
   ```bash
   curl -X POST http://<ESP32_IP>/api/notify/key -d '{"topic":"YOUR_TOPIC"}'
   ```
4. ダッシュボードで各通知タイプの有効/無効を設定

---

## ⚙️ システム設定

最初のTPAを実行する前に、すべての安全パラメータの設定が必要です。これにより、お使いの水槽に合わせた安全な換水が保証されます。

### 必須パラメータ（TPA実行に必要）

| パラメータ | APIフィールド | 説明 |
|---|---|---|
| **水槽の高さ** | `aqHeight` | 内部の高さ（cm） |
| **水槽の長さ** | `aqLength` | 内部の長さ（cm） |
| **水槽の幅** | `aqWidth` | 内部の幅（cm） |
| **リザーバー容量** | `reservoirVolume` | リザーバーの容量（リットル） |
| **交換率** | `tpaPercent` | 排水する水量の割合（例：30%） |
| **キャニスター安全レベル %** | `canisterSafePct` | キャニスターをONにするための最小水位%（例：60%） |

> [!IMPORTANT]
> 上記6つのパラメータがすべて設定されるまでTPAは**開始しません**。ダッシュボードで`tpaConfigReady: true/false`で準備状況を確認できます。

### オプションパラメータ

| パラメータ | APIフィールド | 説明 |
|---|---|---|
| **水面マージン** | `aqMarginMm` | 上端から溢水限界までの距離（mm） |
| **プライム比率** | `primeRatio` | 水1リットルあたりのカルキ抜き量（mL） |
| **リザーバー安全量** | `reservoirSafetyML` | ポンプ安全のためのリザーバー最低量（mL） |
| **TPA間隔** | `tpaInterval` | 自動TPA間の日数（例：7） |
| **TPAスケジュール** | `tpaHour`, `tpaMinute` | 自動TPAの実行時刻 |

### APIによる設定

**水槽寸法：**
```bash
curl -X POST http://<ESP32_IP>/api/config/aquarium \
  -H "Content-Type: application/json" \
  -d '{"aqHeight":45, "aqLength":90, "aqWidth":40, "aqMarginMm":30, "reservoirVolume":20, "primeRatio":0.025}'
```

**TPAスケジュール + キャニスター安全設定：**
```bash
curl -X POST http://<ESP32_IP>/api/schedule \
  -H "Content-Type: application/json" \
  -d '{"tpaInterval":7, "tpaHour":10, "tpaMinute":0, "tpaPercent":30, "canisterSafePct":60}'
```

### 設定の保存方法

すべてのパラメータは**NVS（不揮発性ストレージ）**に保存され、リブートや電源断でも維持されます。設定方法：

1. **Webダッシュボード** — 全パラメータのフォームを備えたReact SPA
2. **REST API** — プログラムアクセス用JSONエンドポイント
3. **シリアルコマンド** — デバッグと初期セットアップ用USB

### 派生値（自動計算）

| 値 | 計算式 | 用途 |
|---|---|---|
| **水槽容量** | `(高さ - マージン) × 長さ × 幅 / 1000` | 総水量（リットル） |
| **cm当たりリットル** | `長さ × 幅 / 1000` | 水位1cmあたりの容量変化 |
| **プライム投与量** | `リザーバー容量 × プライム比率` | 自動計算のカルキ抜き量 |
| **排水cm** | `(容量 × TPA%) / cm当たりリットル` | TPA開始時に計算 |
| **キャニスター安全cm** | `有効高さ × (100 - SafePct) / 100` | 超音波距離の閾値 |
| **動的タイムアウト** | `(容量 / 流量) × 1.5` | キャリブレーション流量から計算 |

---

---

## 🧪 テスト

### ユニットテスト実行（ハードウェア不要）

```bash
pio test -e native
```

### カバレッジレポート生成

```bash
pio test -e coverage && ./scripts/coverage.sh
open coverage/index.html
```

### テストスイート

| スイート | テスト数 | カバレッジ |
|---|---|---|
| `test_fert_manager` | 13 | NVS重複排除、在庫、GPIO、永続化 |
| `test_safety_watchdog` | 14 | センサー、緊急事態、メンテナンス |
| `test_water_manager` | 23 | 換水ステートマシン全体 + キャリブレーション |
| `test_time_manager` | 15 | DateTime、スケジューリング、フォーマット |
| `test_notify_manager` | 10 | 通知、フォーマット |

### 📊 コードカバレッジ

| ファイル | カバレッジ |
|---|---|
| `WaterManager.cpp` | 92% |
| `SafetyWatchdog.cpp` | 81% |
| `FertManager.cpp` | 75% |
| `NotifyManager.cpp` | 43% |
| **合計** | **75%** |

> 75のネイティブユニットテストがコミットごとにCIで実行されます。

---

## 🚀 ビルド・デプロイ

### 初回セットアップ — WiFi認証情報

認証情報はリポジトリの外に置きます。初回ビルドの前に：

```bash
cp secrets.ini.example secrets.ini
# その後、secrets.ini を自分のネットワークのSSIDとパスワードに書き換えます
```

`secrets.ini` は gitignore されているため、リポジトリに入ることはありません。
このファイルがないとビルドは失敗します。

### ファームウェアのビルドとフラッシュ

```bash
# ファームウェアビルド
pio run

# ESP32にフラッシュ
pio run --target upload

# シリアルモニター
pio device monitor
```

### フルデプロイ（フロントエンド + ファームウェア）

`Makefile`でワークフロー全体を自動化：

```bash
# 一括：React ビルド → LittleFS アップロード → ファームウェアフラッシュ
make all

# または個別ステップ：
make build-front     # Reactアプリビルド（Vite）
make upload-fs       # 静的ファイルをLittleFSにアップロード
make upload-fw       # C++ファームウェアのビルドとフラッシュ
make monitor         # シリアルモニターを開く
make clean           # 全ビルドをクリーン
```

### フロントエンド（Webダッシュボード）

Webダッシュボードは`frontend/`にあるReact + Vite + Tailwind CSS SPAです。ビルド後、静的ファイルは`data/`にコピーされ、LittleFS経由でESP32にアップロードされます。

```bash
cd frontend && npm install && npm run build
```

---

## ⌨️ シリアルコマンド

| コマンド | 説明 |
|---|---|
| `help` | 利用可能なコマンド一覧 |
| `status` | 現在のシステム状態 |
| `tpa` | 手動換水を開始 |
| `abort` | 実行中の換水を中止 |
| `maint` | メンテナンスモード切替（30分） |
| `fert_time HH MM` | 施肥時刻を設定 |
| `tpa_interval DAYS` | 自動換水の間隔日数を設定（0 = 無効） |
| `dose CH ML` | チャンネルCH（1–5）の投与量を設定 |
| `reset_stock CH ML` | チャンネルCHの在庫をリセット |
| `set_drain CM` | 排水目標レベルを設定 |
| `set_refill CM` | 給水目標レベルを設定 |
| `emergency_stop` | 全アクチュエーターを停止 |

---

## 📁 プロジェクト構成

```
├── BOM.md                    # 部品表（pt-BR）
├── BOM.en.md                 # 部品表（en-US）
├── BOM.ja.md                 # 部品表（ja-JP）
├── HARDWARE.md               # ハードウェアアーキテクチャ（pt-BR）
├── HARDWARE.en.md            # ハードウェアアーキテクチャ（en-US）
├── HARDWARE.ja.md            # ハードウェアアーキテクチャ（ja-JP）
├── 3d_models/                # 3Dモデル（3D印刷・レーザーカット）
│   ├── electronics_enclosure.scad  # 電子機器筐体（ベース＋蓋）
│   ├── dosing_pump_support.scad    # 8ポンプドーシングサポート
│   └── dosing_pump_single.scad     # 単体ポンプサポート
├── README.md                 # ドキュメント（en-US） — デフォルト
├── README.pt-BR.md           # ドキュメント（pt-BR）
├── README.ja.md              # ドキュメント（ja-JP）
├── Makefile                  # 自動化：フロントエンドビルド + アップロード
├── platformio.ini            # 環境：esp32dev、native、coverage
├── include/
│   ├── Config.h              # ピン、タイムアウト、定数
│   ├── SafetyWatchdog.h
│   ├── TimeManager.h
│   ├── FertManager.h
│   ├── WaterManager.h
│   ├── WebManager.h
│   └── web_dashboard.h       # ダッシュボードHTML/CSS/JS（フォールバック）
├── src/
│   ├── main.cpp              # セットアップ + ループ
│   ├── SafetyWatchdog.cpp
│   ├── TimeManager.cpp
│   ├── FertManager.cpp
│   ├── WaterManager.cpp
│   └── WebManager.cpp
├── frontend/                 # React + Vite + Tailwindダッシュボード
│   ├── src/
│   └── vite.config.ts
├── data/                     # 静的ファイル（フロントエンドビルド → LittleFS）
├── test/
│   ├── mocks/                # Arduinoモックレイヤー（GPIO、NVS、RTC）
│   ├── test_fert_manager/
│   ├── test_safety_watchdog/
│   ├── test_water_manager/
│   └── test_time_manager/
├── scripts/
│   └── coverage.sh           # lcovカバレッジレポートジェネレーター
└── .github/workflows/
    └── test.yml              # CI：コミットごとにテスト実行
```

---

## ☕ サポート

このプロジェクトが役に立った場合は、コーヒーをおごってください！

<a href="https://buymeacoffee.com/iara.automatedwaterchange" target="_blank">
  <img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" width="180" />
</a>

---

## 📜 ライセンス

MIT
