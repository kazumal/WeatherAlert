# WeatherAlert

ESP32 が Open-Meteo から今日の天気を取得し、OLED で玄関に天気と傘の要否を表示する IoT デバイス。

```
晴れ  → Sunny / No umbrella
曇り  → Cloudy / Check later
雨    → Rain / Take umbrella
```

## 動作環境

| 項目 | 内容 |
|------|------|
| ボード | ESP32 Dev Module（ELEGOO Super Starter Kit 想定） |
| 表示 | 0.96" SSD1306 I2C OLED（128×64） |
| 開発 | PlatformIO CLI |
| 天気 API | [Open-Meteo](https://open-meteo.com/)（API キー不要） |

## クイックスタート

### 1. WiFi 設定

```bash
cp include/secrets.h.example include/secrets.h
```

`include/secrets.h` を編集:

```cpp
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
```

### 2. 位置（任意）

デフォルトは東京（35.6812, 139.7671）。変更する場合は `include/secrets.h` に追記:

```cpp
#define LATITUDE  33.5861
#define LONGITUDE 130.3956
```

### 3. ESP32 + OLED を配線

**ブレッドボード推奨**（ESP32 はピンが下向きのため）。

| OLED ピン | ESP32 |
|-----------|-------|
| GND | GND |
| VCC | **3.3V**（5V / VIN 不可） |
| SDA | **GPIO 21**（D21） |
| SCL | **GPIO 22**（D22） |

```
OLED          ESP32（例）
 GND  ────────  GND
 VCC  ────────  3V3
 SCL  ────────  D22
 SDA  ────────  D21
```

- ジャンパー線: **Female-Male** または **Male-Male + ブレッドボード**
- I2C アドレス: `0x3C`（ELEGOO キット標準）

### 4. USB 接続 & ポート確認

ESP32 を Mac に USB 接続し、ポートを確認:

```bash
pio device list
```

`platformio.ini` の `upload_port` / `monitor_port` を自分の環境に合わせて変更するか、書き込み時に `--upload-port` を指定。

例: `/dev/cu.usbserial-0001`（CP2102）

### 5. ビルド・書き込み・ログ

```bash
cd WeatherAlert
pio run                  # ビルド
pio run -t upload        # ESP32 に書き込み
pio device monitor       # Serial Monitor（115200）
```

初回 `pio run` は ESP32 ツールチェーンのダウンロードで **5〜15 分** かかることがあります（2 回目以降は数秒）。

---

## 正常動作の確認

Serial Monitor に次が出れば OK:

```
=== WeatherAlert ===
OLED ready
Connected. IP: 192.168.x.x
Fetching weather from Open-Meteo...
Today weathercode (WMO): 51
RAIN: true
```

OLED（128×64、上部に `WeatherAlert` ヘッダー + アイコン + 天気ラベル）:

| 天気 | 表示 |
|------|------|
| 雨 | 雨雲アイコン + `Rain` / `Take umbrella` |
| 曇り | 雲アイコン + `Cloudy` / `Check later` |
| 晴れ | 太陽アイコン + `Sunny` / `No umbrella` |

---

## Serial テストコマンド

API を待たずに OLED 表示を試せます。Monitor を開いたまま **Enter 付き** で送信。

| コマンド | 効果 |
|----------|------|
| `51` または `code 51` | WMO code を指定（0〜99） |
| `rain` | 雨（code 51） |
| `cloudy` | 曇り（code 3） |
| `clear` | 晴れ（code 0） |
| `live` | テスト終了 → API の本番天気に戻す |
| `refresh` | 今すぐ API 再取得 |
| `help` | コマンド一覧 |

テスト中（`51` など送信後）は 3 時間ごとの自動更新を止め、表示を固定。`live` で API モードに戻る。

---

## プロジェクト構成

```
WeatherAlert/
├── platformio.ini
├── src/main.cpp           # メインロジック
├── include/
│   ├── config.h           # ピン・天気更新間隔（位置のデフォルト）
│   ├── secrets.h.example  # WiFi・位置テンプレート
│   └── secrets.h          # 各自作成（.gitignore）
```

## トラブルシュート

| 症状 | 対処 |
|------|------|
| ポートが見えない | USB ケーブル差し直し、CP2102 ドライバ確認 |
| 書き込み失敗 | `--upload-port` 指定、BOOT 押しながら upload |
| WiFi タイムアウト | `secrets.h` の SSID/パス、2.4GHz WiFi か確認 |
| `OLED init failed` | SDA/SCL の配線、3.3V 給電を確認 |
| JSON パース失敗 | ファームを最新に（`getString()` 版） |
