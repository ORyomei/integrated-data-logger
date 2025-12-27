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
sudo apt install -y python3-pip cmake ninja-build device-tree-compiler dfu-util
sudo install --break-system-packages west pyelftools  # python は venv をつかってもよい
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

```bash
cd firmware
sudo west flash
```
