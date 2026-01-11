# Integrated Data Logger

Teensy 4.1 + ADS8688 を使用した高速データロガー

## 機能

- ADS8688 による 8ch アナログ入力
  - 入力範囲: ±10V
  - サンプリング周波数: 10kHz
  - 16bit 分解能
- 60Hz 矩形波出力 (GPIO)
- Ethernet (UDP) によるデータ転送
  - 100Hz でデータ送信
  - CSV フォーマット

## ハードウェア構成

### 必要な機材

- Teensy 4.1
- ADS8688 ADC モジュール
- SparkFun Ethernet Adapter for Teensy (または Teensy 4.1 内蔵イーサネット)

### ピンアサイン

#### ADS8688 (SPI接続)

| 機能 | Teensy 4.1 ピン | ADS8688 ピン | 備考 |
|------|----------------|--------------|------|
| MISO | 39 (LPSPI3) | SDO | データ入力 |
| MOSI | 26 (LPSPI3) | SDI | データ出力 |
| SCK  | 27 (LPSPI3) | SCLK | クロック |
| CS   | 38 (GPIO2_11) | CS | チップセレクト |

#### GPIO出力

| 機能 | Teensy 4.1 ピン | 備考 |
|------|----------------|------|
| 60Hz 矩形波 | 19 (GPIO1_0) | 測定用トリガー信号 |

#### イーサネット

Teensy 4.1 は内蔵イーサネット PHY を搭載しているため、RJ45 コネクタを接続するだけで使用できます。
SparkFun Ethernet Adapter を使用する場合も、Teensy 4.1 の内蔵 ENET ポートが使用されます。

## ソフトウェア構成

### ファームウェア

Zephyr RTOS 4.3.0 ベースのファームウェア

#### ビルド方法

```bash
cd firmware
west build -b teensy41 apps/integrated-data-logger
```

#### 書き込み方法

```bash
# Teensy Loader を使用 (GUI)
# または teensy_loader_cli を使用
teensy_loader_cli --mcu=TEENSY41 -w build/zephyr/zephyr.hex
```

### PC側受信プログラム

UDPでデータを受信するPythonスクリプト

```python
#!/usr/bin/env python3
import socket
import csv
from datetime import datetime

# UDP設定
UDP_IP = "0.0.0.0"  # 全てのインターフェースで受信
UDP_PORT = 8888

# CSVファイル名
csv_filename = f"data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"

# ソケット作成
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

print(f"Listening on {UDP_IP}:{UDP_PORT}")
print(f"Saving to {csv_filename}")

# CSVファイルをオープン
with open(csv_filename, 'w', newline='') as csvfile:
    csv_writer = csv.writer(csvfile)
    # ヘッダー書き込み
    csv_writer.writerow(['Timestamp(ms)', 'CH0', 'CH1', 'CH2', 'CH3', 
                        'CH4', 'CH5', 'CH6', 'CH7'])
    
    try:
        packet_count = 0
        while True:
            data, addr = sock.recvfrom(1024)
            
            # デコードしてCSVに書き込み
            line = data.decode('utf-8').strip()
            values = line.split(',')
            
            if len(values) == 9:  # timestamp + 8 channels
                csv_writer.writerow(values)
                
                packet_count += 1
                if packet_count % 100 == 0:  # 1秒ごとに表示
                    print(f"Received {packet_count} packets, last: {values}")
            
    except KeyboardInterrupt:
        print(f"\nStopped. Total packets received: {packet_count}")
        sock.close()
```

### ネットワーク設定

1. Teensy 4.1 と PC を同じネットワークに接続
2. PC の IP アドレスを確認 (例: 192.168.1.100)
3. ファームウェアの `src/main.c` で送信先 IP を設定

```c
#define UDP_DEST_ADDR       "192.168.1.100"  // PC のIPアドレス
```

4. ファームウェアは DHCP で自動的に IP アドレスを取得します

### データフォーマット

CSV 形式:
```
timestamp,ch0,ch1,ch2,ch3,ch4,ch5,ch6,ch7
1234,100,-200,300,-400,500,-600,700,-800
...
```

- timestamp: システム起動からのミリ秒
- ch0-ch7: 各チャンネルの ADC 値 (16bit signed, ±10V を ±32768 にマッピング)

### 電圧変換

ADC値を電圧に変換する式:
```
voltage = (adc_value / 32768.0) * 10.0
```

例:
- ADC値 = 32767 → +10V
- ADC値 = 0 → 0V
- ADC値 = -32768 → -10V

## リポジトリ構成

```bash
integrated-data-logger/.
├── README.md
└── firmware # Zephyr RTOS ファームウェア
    └── apps
       ├── integrated-data-logger # data-logger ファームウェア
       ├── ...
       └── west.yml
```
