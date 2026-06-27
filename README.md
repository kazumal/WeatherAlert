# KasaAlert（カサアラート）

ESP32 で天気 API を取得し、雨の日だけ目立つ表示で「傘を忘れない」を玄関で支援するデバイス。

## 概要

玄関に常設する ESP32 デバイスが、Open-Meteo などの天気 API から今日の予報を取得する。雨の予報がある日だけ OLED 表示と LED で注意喚起し、晴れの日は目立たない状態に保つ。

```
普段（晴れ）  : OLED 薄表示 or 消灯、LED 消灯
雨の日        : OLED「☔ 傘！」+ LED 点滅
曇り          : OLED「曇り」+ 黄 LED（弱め）
```

## ドキュメント

| ファイル | 内容 |
|----------|------|
| [docs/PROJECT.md](docs/PROJECT.md) | プロジェクト設計・部品・API・実装ステップの詳細 |

## ハードウェア

- ESP32 SuperKit 系（ESP32 DevKit + OLED + LED + 抵抗 + ブレッドボード等）
- USB 給電（玄関常設用）

## ソフトウェア（予定）

- PlatformIO または Arduino IDE
- WiFi / HTTPClient / ArduinoJson / Adafruit SSD1306

## ステータス

🚧 設計・ドキュメント段階（コード未実装）

## ライセンス

未定
