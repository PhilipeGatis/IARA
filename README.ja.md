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

ファームウェアは**ESP32 DevKit V1**上で動作し、**組み込みWebダッシュボード**（React + Vite、LittleFSで配信）、**OLEDディスプレイ**、**DS3231 RTCクロック**、**シリアルコマンドインターフェース**を搭載しています。

---

## 📚 追加ドキュメント

プロジェクトには以下の詳細なハードウェアドキュメントが含まれています：

| ドキュメント | 説明 |
|---|---|
| [`BOM.md`](BOM.md) / [`BOM.ja.md`](BOM.ja.md) | **部品表** — AC、DC、アクチュエーター、センサー、保護、コネクタのレイヤー別全コンポーネントリスト。 |
| [`HARDWARE.md`](HARDWARE.md) / [`HARDWARE.ja.md`](HARDWARE.ja.md) | **ハードウェアアーキテクチャ** — 各レイヤーの配線図、ノイズ保護、センサー用分圧回路、安全実装ノートを含む詳細技術資料。 |
| [`3d_models/`](3d_models/) | **3Dモデル (OpenSCAD)** — パラメトリック電子機器筐体 (`electronics_enclosure.scad`)、ドーシングポンプサポート (`dosing_pump_support.scad` / `dosing_pump_single.scad`)、ドリルテストプレート。 |

---

## 🏗️ ファームウェアアーキテクチャ

```
main.cpp               ← メインオーケストレーター
├── SafetyWatchdog      ← センサー + 緊急 + メンテナンスモード
├── TimeManager         ← DS3231 RTC + NTP同期
├── FertManager         ← 投与 + NVS重複排除 + 在庫追跡
├── WaterManager        ← 換水ステートマシン（6ステート）
├── DisplayManager      ← OLED SSD1306 128×64
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
    [*] --> CANISTER_OFF: startTPA()
    CANISTER_OFF --> DRAINING: 3秒安定化
    DRAINING --> FILLING_RESERVOIR: レベル到達
    FILLING_RESERVOIR --> DOSING_PRIME: フロートスイッチ
    DOSING_PRIME --> REFILLING: 2秒混合
    REFILLING --> CANISTER_ON: レベル到達
    CANISTER_ON --> [*]: 完了

    DRAINING --> ERROR: タイムアウト
    FILLING_RESERVOIR --> ERROR: タイムアウト
    REFILLING --> ERROR: タイムアウト / 光学センサー
    ERROR --> [*]: レベル確認 → キャニスターON/OFF
```

| ステップ | ステート | 動作内容 |
|---|---|---|
| 1 | **CANISTER_OFF** | キャニスターフィルターをOFF（SSRリレーHIGH）。超音波センサーが安定した読み取りを得られるよう3秒待機。 |
| 2 | **DRAINING** | 排水ポンプON。超音波センサーが水位を監視。目標レベルに到達するまでポンプ稼働（例：10cm低下）。自動キャリブレーション用に流量を測定。 |
| 3 | **FILLING_RESERVOIR** | ソレノイドバルブを開いてリザーバーに水道水を供給。フロートスイッチがリザーバー水位を監視。満水でバルブを閉鎖。 |
| 4 | **DOSING_PRIME** | ペリスタルティックポンプがリザーバーに脱塩素剤（Prime）を投与。2秒間混合を待機。在庫が減算されNVSに保存。 |
| 5 | **REFILLING** | 給水ポンプON、処理済みの水をリザーバーから水槽へ送水。超音波センサーが元のレベルに到達するか、光学センサーが最大レベルを検出（安全遮断）で停止。キャリブレーション用に流量を測定。 |
| 6 | **CANISTER_ON** | キャニスターフィルターを再びON。**TPAサイクル完了。**キャリブレーション済み流量がNVSに保存。 |

**各ステップの安全性：**
- 各ステートにはキャリブレーション済み流量から計算される**動的タイムアウト**あり（`容量 / 流量 × 1.5`）。初回TPAは安全なデフォルト値を使用：**排水30秒、給水15秒**。
- **光学センサー**がリフィル中のハードウェアレベル安全遮断として機能 — 超音波読み取りに関係なく即時停止。
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
    ESP32 -->|D21 SDA, D22 SCL| I2C[RTC + OLED]
    ESP32 -->|D12, D13, D14, D25-D27, D32, D33| MOSFET
    ESP32 -->|D2| SSR[Omron SSR]
    ESP32 ---|D18 Trig, D19 Echo| Ultra[超音波JSN]
    ESP32 ---|D4| Water[静電容量センサー]
    ESP32 ---|D5| Float[フロートスイッチ]
  end

  MOSFET -->|OUT 1–5| Peristaltic[5xペリスタルティックポンプ]
  MOSFET -->|OUT 6| Drain[排水ポンプ]
  MOSFET -->|OUT 7| Refill[給水ポンプ]
  MOSFET -->|OUT 8| Solenoid[ソレノイドバルブ]
```

> 詳細な配線図、分圧回路、保護ノート（フライバックダイオード、スターGND）は [`HARDWARE.ja.md`](HARDWARE.ja.md) を参照してください。
>
> 全コンポーネントリストと数量は [`BOM.ja.md`](BOM.ja.md) を参照してください。

---

## 🛡️ 安全性と信頼性

浸水、機器損傷、魚の損失を防止するため、**安全第一**のアプローチで設計されています。

### ソフトウェア保護

| 保護機能 | 説明 |
|---|---|
| **ハードウェアウォッチドッグ（WDT）** | ESP32タスクWDT、10秒タイムアウト。メインループがフリーズした場合、ESP32が自動的に再起動します。 |
| **SafetyWatchdog** | ループの各イテレーションで最高優先度で実行。オーバーフロー（光学センサー）、緊急事態を検出し、全アクチュエーターを停止します。 |
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
| **オーバーフローフロートスイッチ** | 最大水位にNC（常閉）フロートスイッチを設置し、給水ポンプの電源に直列接続。水位が上がりすぎた場合にファームウェアに依存せず物理的にポンプを遮断。 |
| **フライバックダイオード** | ポンプにFR154、ソレノイドに1N5822 — 誘導性負荷の電圧スパイクを吸収。 |
| **デカップリングコンデンサ** | ESP32近傍に1000µF、MOSFETモジュール近傍に470µF — ブラウンアウト過渡現象を吸収。 |
| **分圧回路（ECHO）** | JSN-SR04Tのechoピン用5V → 3.3V — ESP32 GPIOを保護。 |

---

## 📲 プッシュ通知

IARAは[Pushsafer](https://www.pushsafer.com/)を通じてリアルタイムのプッシュ通知をスマートフォンに送信します。各通知タイプはダッシュボードで個別に有効/無効にできます。

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

1. [pushsafer.com](https://www.pushsafer.com/)で無料アカウントを作成
2. Pushsaferダッシュボードから**Private Key**をコピー
3. IARAダッシュボード（`通知 → API Key`）またはAPIでキーを入力：
   ```bash
   curl -X POST http://<ESP32_IP>/api/notify/key -d '{"key":"YOUR_KEY"}'
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
| **水面マージン** | `aqMarginCm` | 上端から水面までの距離（cm） |
| **プライム比率** | `primeRatio` | 水1リットルあたりのカルキ抜き量（mL） |
| **リザーバー安全量** | `reservoirSafetyML` | ポンプ安全のためのリザーバー最低量（mL） |
| **TPA間隔** | `tpaInterval` | 自動TPA間の日数（例：7） |
| **TPAスケジュール** | `tpaHour`, `tpaMinute` | 自動TPAの実行時刻 |

### APIによる設定

**水槽寸法：**
```bash
curl -X POST http://<ESP32_IP>/api/config/aquarium \
  -H "Content-Type: application/json" \
  -d '{"aqHeight":45, "aqLength":90, "aqWidth":40, "aqMarginCm":3, "reservoirVolume":20, "primeRatio":0.5}'
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

## 🖥️ Wokwiシミュレーション

プロジェクトは[Wokwi](https://wokwi.com)シミュレーションを完全サポートしており、**物理ハードウェアなし**でファームウェアをテストできます。

### 前提条件

1. **Wokwi Simulator**拡張機能がインストールされた[VS Code](https://code.visualstudio.com/)。
2. VS Codeにインストールされた[PlatformIO](https://platformio.org/)。

### 手順

1. **Wokwi環境用ファームウェアをビルド：**

   ```bash
   pio run -e wokwi
   ```

2. **シミュレーションを開始：**
   - プロジェクトディレクトリでVS Codeを開きます。
   - `F1` → **Wokwi: Start Simulator** を押します。
   - シミュレーターが`diagram.json`とコンパイル済みファームウェアを読み込みます。

3. **シミュレーションとの対話：**
   - **換水ボタン**（GPIO 15） — 押して換水サイクルを開始。
   - **施肥ボタン**（GPIO 23） — 押して肥料投与を実行。
   - シリアルモニターにリアルタイムでシステムログが表示されます。
   - 仮想WiFiが有効（`wokwi.toml`で`[net] enable = true`）で、Webダッシュボードにアクセス可能。

### Wokwi環境の違い

`wokwi`環境は`-D WOKWI_TEST`フラグを設定し、自動的に以下を調整します：

- **高速タイミング** — 換水サイクルが分ではなく秒で動作。
- **小容量** — クイックテスト用にパラメータを縮小。

### 関連ファイル

| ファイル | 用途 |
|---|---|
| `diagram.json` | 仮想回路の定義（ESP32、ボタン、LED、センサー） |
| `wokwi.toml` | シミュレーター設定（ファームウェアパス、仮想ネットワーク） |
| `platformio.ini`（`[env:wokwi]`） | シミュレーションフラグ付きビルド環境 |

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
# 一括：React ビルド → LittlFS アップロード → ファームウェアフラッシュ
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
| `tpa_time DOW HH MM` | 換水スケジュールを設定 |
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
├── diagram.json              # Wokwi仮想回路
├── wokwi.toml                # Wokwiシミュレーター設定
├── platformio.ini            # 環境：esp32dev、wokwi、native、coverage
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
