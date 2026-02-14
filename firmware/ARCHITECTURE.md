# Integrated Data Logger — Firmware Architecture

## 概要

Teensy 4.1 上で動作する 8ch アナログデータロガー。
外付け ADC (TI ADS8688) から 1kHz で 8ch ±10V のアナログ値を取得し、CSV 形式で USB CDC (Serial) に出力する。
同時に GPIO pin 2 から 60Hz の矩形波をデジタル出力する。

## ハードウェア

| 項目       | 仕様                                            |
| ---------- | ----------------------------------------------- |
| MCU        | Teensy 4.1 (i.MX RT1062, ARM Cortex-M7 600MHz)  |
| ADC        | TI ADS8688 — 16bit 8ch SAR ADC                  |
| 入力レンジ | ±10.24V (BIPOLAR_2_5xVREF)                      |
| SPI バス   | LPSPI4 (SPI0) — SCK=13, MOSI=11, MISO=12, CS=10 |
| CS 制御    | ハードウェア CS (LPSPI4_PCS0, pin 10 ALT3)      |
| SPI モード | MODE1 (CPOL=0, CPHA=1), 5MHz                    |
| 矩形波出力 | pin 2, 60Hz, PIT タイマーによるデジタルトグル   |

## ビルド環境

- **PlatformIO** (`platformio.ini`)
- ボード: `teensy41`
- フレームワーク: Arduino
- ビルドフラグ: `-DUSB_SERIAL -O2`
- ビルド・書き込みは VS Code の PlatformIO GUI タスクで行う（ターミナルコマンドではない）

## ディレクトリ構成

```
firmware/
├── platformio.ini
├── ARCHITECTURE.md          # 本ドキュメント
├── src/
│   └── main.cpp             # エントリポイント (setup/loop)
├── include/
│   └── constants.h          # ピン番号・システム定数
├── lib/
│   ├── ADS8688/
│   │   └── ADS8688.h        # 低レベル SPI ドライバ (header-only)
│   ├── ADC/
│   │   ├── ADC.h            # 高レベル ADC クラス
│   │   └── ADC.cpp          # ADC クラス実装
│   └── Pulse/
│       ├── pulse.h          # 矩形波出力クラス
│       └── pulse.cpp        # Pulse クラス実装
└── test/
```

## レイヤー構成

```
main.cpp
  ├── constants.h            (include/)
  ├── ADC クラス             (lib/ADC/)
  │     └── ADS8688 クラス   (lib/ADS8688/)
  │           └── LPSPI4 レジスタ (Teensy 4.x ハードウェア)
  └── Pulse クラス           (lib/Pulse/)
        └── PIT タイマー (IntervalTimer)
```

### 1. constants.h (`include/constants.h`)

ピン番号やシステム定数を一元管理するヘッダ。

- **`pin::ADC_CS`** — ADC チップセレクト (pin 10)
- **`pin::SQUARE_WAVE`** — 矩形波出力 (pin 2)
- **`config::SERIAL_BAUD`** — USB CDC ボーレート (115200)
- **`config::SERIAL_WAIT_MS`** — Serial 接続待ちタイムアウト (3000ms)
- **`config::ADC_SAMPLE_INTERVAL_US`** — ADC サンプリング間隔 (1000us = 1kHz)
- **`config::SQUARE_WAVE_FREQ_HZ`** — 矩形波周波数 (60Hz)

### 2. ADS8688 クラス (`lib/ADS8688/ADS8688.h`)

ADS8688 の SPI 通信を直接扱う低レベルドライバ。Header-only。レンジ設定や電圧変換は行わず、生の uint16_t 値を返す。

- **`begin()`** — ハードウェア CS 設定、全チャネル電源オン、AUTO_RST モード開始（レンジ設定は行わない）
- **`readAllChannels(uint16_t raw[])`** — AUTO_RST モードで 8ch 一括読み取り（オフセットバイナリ形式の uint16_t）
- **`writeRegister(addr, value)`** — 24bit フレームでレジスタ書き込み (LPSPI4 直接制御)
- **`transferCommand32(cmd)`** — 32bit コマンドフレーム送受信 (LPSPI4 直接制御)

SPI 転送は LPSPI4 レジスタ (`IMXRT_LPSPI4_S`) を直接操作し、ハードウェア CS と可変フレームサイズ (24bit/32bit) を使用。

### 3. ADC クラス (`lib/ADC/`)

ADS8688 を抽象化した高レベルインターフェース。レンジ設定と電圧変換を担当。

- **`begin(range)`** — ADS8688 初期化 + 全チャネルのレンジ設定 (デフォルト: BIPOLAR_2_5xVREF)
- **`startSampling(intervalUs)`** — IntervalTimer で周期サンプリング開始 (デフォルト 1kHz)
- **`stopSampling()`** — タイマー停止
- **`available()`** — ISR フラグで新データの有無を返す
- **`read()`** — 全チャネル読み取り (フラグクリア + readAllChannels)
- **`voltage(ch)` / `rawValue(ch)`** — チャネル単位のデータ取得 (rawValue は uint16_t)
- **`printCSVHeader(out)` / `printCSVLine(out, timestamp)`** — CSV 出力
- **`toVoltage(raw)`** — オフセットバイナリの uint16_t → 電圧変換 (private, レンジ対応)

電圧変換: ADS8688 の出力はオフセットバイナリ形式 (Bipolar: 0x8000=0V, Unipolar: 0x0000=0V)。
ISR コールバックには static メンバ + singleton パターンを使用。

### 4. Pulse クラス (`lib/Pulse/`)

PIT タイマー (IntervalTimer) による正確な矩形波デジタル出力。

- **`Pulse(pin, freqHz)`** — 出力ピンと周波数を指定して構築
- **`begin()`** — `pinMode` 設定 + PIT タイマー開始 (半周期ごとに `digitalToggleFast`)
- **`stop()`** — タイマー停止、ピンを LOW に
- **`setFrequency(freqHz)`** — 動作中に周波数を変更

ISR コールバックには static メンバ + singleton パターンを使用。

### 5. main.cpp (`src/main.cpp`)

ADC / Pulse クラスと constants.h のみを使用するシンプルなエントリポイント。

```cpp
#include <ADC.h>
#include <pulse.h>
#include "constants.h"

ADC adc(SPI, pin::ADC_CS);
Pulse squareWave(pin::SQUARE_WAVE, config::SQUARE_WAVE_FREQ_HZ);

void setup() {
    Serial.begin(config::SERIAL_BAUD);
    squareWave.begin();
    SPI.begin();
    adc.begin();
    adc.printCSVHeader(Serial);
    adc.startSampling(config::ADC_SAMPLE_INTERVAL_US);
}

void loop() {
    if (!adc.available()) return;
    adc.read();
    adc.printCSVLine(Serial, micros());
}
```

## データフロー

1. `IntervalTimer` (PIT ch0) が 1kHz (1000us) で ISR を発火 → `_sampleFlag = true`
2. `IntervalTimer` (PIT ch1) が 120Hz (8333us) で ISR を発火 → `digitalToggleFast` で 60Hz 矩形波生成
3. `loop()` で `adc.available()` が true を検出
4. `adc.read()` → ADS8688 の AUTO_RST モードで 8ch 分の SPI 転送
5. `adc.printCSVLine()` → `snprintf` で CSV 行を構築、`Serial.write()` で USB CDC 出力

## 出力フォーマット (CSV)

```
time_us,ch0,ch1,ch2,ch3,ch4,ch5,ch6,ch7
123456,0.0031,-0.0015,1.2345,...
```

- `time_us`: `micros()` のタイムスタンプ
- `ch0`–`ch7`: 電圧値 [V]、小数点以下 4 桁

## 注意事項

- CS ピンは pin 10 固定（LPSPI4_PCS0 のハードウェアマッピング）
- SPI クロックは 5MHz（ADS8688 の最大 17MHz に対して十分なマージン）
- `delayMicroseconds(2)` は各 SPI 転送後の CS 非アクティブ時間確保用
- ADS8688 の生値はオフセットバイナリ形式の uint16_t（Bipolar: 0x8000=0V）
- レンジ設定と電圧変換は ADC クラスが担当（ADS8688 クラスは生データのみ）
- Teensy 4.1 は PIT タイマーを 4 本持つ（ADC 用 + Pulse 用 で 2 本使用）
- ピン番号・定数の変更は `include/constants.h` で一元管理
