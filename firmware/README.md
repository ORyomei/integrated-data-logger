# Firmware (Zephyr RTOS)

## 開発環境

- OS: Ubuntu / WSL2 (Ubuntu)
- Python: 3.10 以上
- ボード: **Teensy 4.1**
- 開発ツール: **[West](https://docs.zephyrproject.org/latest/develop/west/index.html)**

## 環境構築

### 必要パッケージ、West のインストール

```bash
# Ubuntu
sudo apt install -y python3-pip cmake ninja-build device-tree-compiler dfu-util teensy-loader-cli
sudo pip install --break-system-packages west pyelftools  # python は venv をつかってもよい
```

### Zephyr SDK のインストール

```bash
cd ~
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.5/zephyr-sdk-0.16.5_linux-x86_64.tar.xz
tar xf zephyr-sdk-0.16.5_linux-x86_64.tar.xz
cd zephyr-sdk-0.16.5
./setup.sh
```

`export ZEPHYR_SDK_INSTALL_DIR=$HOME/zephyr-sdk-0.16.5` が必要。`.bashrc` に書く場合は、

```bash
echo 'export PATH=$HOME/.local/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

### 初回セットアップ

注）`firmware/apps` で実行すること

これにより .west / zephyr / modules が firmware/ 配下に作られる

```bash
cd firmware/apps
west init -l .
cd ..
west update
west zephyr-export
```

確認：

```bash
west topdir
# → firmware を指していれば OK
```

### ビルド

```bash
cd firmware
west build -b teensy41 apps/integrated-data-logger -p auto -DCMAKE_EXPORT_COMPILE_COMMANDS=1
```

成功すると： `firmware/build/zephyr/zephyr.hex` が生成される。

### 書き込み（Teensy 4.1）

#### WSL を使用する場合の準備

1. Windows に `usbipd-win` をインストールする。
2. Windows 側： USB デバイスの確認

   **管理者 PowerShell** を起動して以下を実行

   ```powershell
   > usbipd list
   BUSID  VID:PID    DEVICE
   3-5    16c0:0478  USB Input Device
   ```

   `VID:PID = 16c0:0478` が Teensy（HalfKay Bootloader）である。

3. Windows -> WSL に USB をアタッチ

   ```powershell
   usbipd attach --wsl --busid 3-5 --auto-attach # BUSID は usbipd list の結果に合わせる
   ```

   を実行して、 Teensy の boot ボタンを一回押す。

   WSL 側で

   ```bash
   lsusb
   ```

   を実行して、以下のように表示されれば、WSL から書き込める。

   ```bash
    ID 16c0:0478 Van Ooijen Technische Informatica Teensy Halfkay Bootloader
   ```

#### 書き込み

```bash
cd firmware
sudo west flash
```
