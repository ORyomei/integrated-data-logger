# Integrated Data Logger — Firmware Architecture

## 概要

Teensy 4.1 上で動作する 8ch アナログデータロガー。
外付け ADC (TI ADS8688) から 1kHz で 8ch ±10V のアナログ値を取得し、CSV 形式で USB CDC (Serial) に出力する。

## ハードウェア

| 項目       | 仕様                                            |
| ---------- | ----------------------------------------------- |
| MCU        | Teensy 4.1 (i.MX RT1062, ARM Cortex-M7 600MHz)  |
| ADC        | TI ADS8688 — 16bit 8ch SAR ADC                  |
| 入力レンジ | ±10.24V (BIPOLAR_2_5xVREF)                      |
| SPI バス   | LPSPI4 (SPI0) — SCK=13, MOSI=11, MISO=12, CS=10 |
| CS 制御    | ハードウェア CS (LPSPI4_PCS0, pin 10 ALT3)      |
| SPI モード | MODE1 (CPOL=0, CPHA=1), 5MHz                    |

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
├── src/
│   └── main.cpp            # エントリポイント (setup/loop)
├── lib/
│   ├── ADS8688/
│   │   └── ADS8688.h       # 低レベル SPI ドライバ (header-only)
│   └── ADC/
│       ├── ADC.h            # 高レベル ADC クラス
│       └── ADC.cpp          # ADC クラス実装
├── include/
└── test/
```

## レイヤー構成

```
main.cpp
  └── ADC クラス           (lib/ADC/)
        └── ADS8688 クラス  (lib/ADS8688/)
              └── LPSPI4 レジスタ (Teensy 4.x ハードウェア)
```

### 1. ADS8688 クラス (`lib/ADS8688/ADS8688.h`)

ADS8688 の SPI 通信を直接扱う低レベルドライバ。Header-only。

- **`begin()`** — ハードウェア CS 設定、全チャネルのレンジ設定、AUTO_RST モード開始
- **`readAllChannels(int16_t raw[])`** — AUTO_RST モードで 8ch 一括読み取り
- **`toVoltage(int16_t raw)`** — 生値→電圧変換 (static)
- **`writeRegister(addr, value)`** — 24bit フレームでレジスタ書き込み (LPSPI4 直接制御)
- **`transferCommand32(cmd)`** — 32bit コマンドフレーム送受信 (LPSPI4 直接制御)

SPI 転送は LPSPI4 レジスタ (`IMXRT_LPSPI4_S`) を直接操作し、ハードウェア CS と可変フレームサイズ (24bit/32bit) を使用。

### 2. ADC クラス (`lib/ADC/`)

ADS8688 を抽象化した高レベルインターフェース。

- **`begin()`** — ADS8688 初期化
- **`startSampling(intervalUs)`** — IntervalTimer で周期サンプリング開始 (デフォルト 1kHz)
- **`stopSampling()`** — タイマー停止
- **`available()`** — ISR フラグで新データの有無を返す
- **`read()`** — 全チャネル読み取り (フラグクリア + readAllChannels)
- **`voltage(ch)` / `rawValue(ch)`** — チャネル単位のデータ取得
- **`printCSVHeader(out)` / `printCSVLine(out, timestamp)`** — CSV 出力

ISR コールバックには static メンバ + singleton パターンを使用。

### 3. main.cpp (`src/main.cpp`)

ADC クラスのみを使用するシンプルなエントリポイント。ADS8688 の詳細には依存しない。

```cpp
ADC adc(SPI, 10);

void setup() {
    SPI.begin();
    adc.begin();
    adc.printCSVHeader(Serial);
    adc.startSampling();  // 1kHz
}

void loop() {
    if (!adc.available()) return;
    adc.read();
    adc.printCSVLine(Serial, micros());
}
```

## データフロー

1. `IntervalTimer` が 1kHz (1000us) で ISR を発火 → `_sampleFlag = true`
2. `loop()` で `adc.available()` が true を検出
3. `adc.read()` → ADS8688 の AUTO_RST モードで 8ch 分の SPI 転送
4. `adc.printCSVLine()` → `snprintf` で CSV 行を構築、`Serial.write()` で USB CDC 出力

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
- ADS8688 の生値は `readVal >> 1` で取得（データシートの bit alignment に対応）
