#!/usr/bin/env python3
"""
TCP Data Receiver for Integrated Data Logger
TCPクライアント: Teensy TCPサーバーに接続してデータを受信
"""

import socket
import sys
import time
from datetime import datetime

# Teensy の IP アドレス (ルーターのDHCPリストから確認)
TEENSY_IP = "192.168.0.84"  # ← Wireshark で確認した Teensy の IP
TCP_PORT = 8888

def main():
    # ログファイル名
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"data_{timestamp}.csv"
    
    print(f"Connecting to Teensy at {TEENSY_IP}:{TCP_PORT}")
    
    # TCPソケット作成
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10.0)  # 接続タイムアウト
    
    try:
        # Teensy に接続
        sock.connect((TEENSY_IP, TCP_PORT))
        print(f"Connected to {TEENSY_IP}:{TCP_PORT}")
        print(f"Logging to: {filename}")
        
        # 受信後はタイムアウトを長めに設定
        sock.settimeout(5.0)
        
        packet_count = 0
        error_count = 0
        start_time = time.time()
        
        with open(filename, 'w') as f:
            # CSVヘッダー
            f.write("timestamp,ch0,ch1,ch2,ch3,ch4,ch5,ch6,ch7\n")
            
            buffer = b""
            
            while True:
                try:
                    # データ受信 (バッファリング対応)
                    data = sock.recv(4096)
                    
                    if not data:
                        print("Connection closed by server")
                        break
                    
                    # バッファに追加
                    buffer += data
                    
                    # 改行で分割して処理
                    while b'\n' in buffer:
                        line, buffer = buffer.split(b'\n', 1)
                        
                        try:
                            # デコードして処理
                            line_str = line.decode('utf-8').strip()
                            if line_str:
                                # ファイルに書き込み
                                f.write(line_str + '\n')
                                f.flush()
                                
                                packet_count += 1
                                
                                # 進捗表示 (10パケットごと)
                                if packet_count % 10 == 0:
                                    elapsed = time.time() - start_time
                                    rate = packet_count / elapsed if elapsed > 0 else 0
                                    print(f"Received: {packet_count} packets, "
                                          f"Rate: {rate:.1f} Hz, "
                                          f"Errors: {error_count}")
                        except UnicodeDecodeError:
                            error_count += 1
                            print(f"Decode error: {line}")
                
                except socket.timeout:
                    print("Timeout waiting for data")
                    break
                except Exception as e:
                    error_count += 1
                    print(f"Error receiving data: {e}")
                    break
        
        # 統計表示
        elapsed = time.time() - start_time
        avg_rate = packet_count / elapsed if elapsed > 0 else 0
        
        print("\n" + "="*50)
        print(f"Total packets: {packet_count}")
        print(f"Error packets: {error_count}")
        print(f"Duration: {elapsed:.1f}s")
        print(f"Average rate: {avg_rate:.1f} Hz")
        print(f"Data saved to: {filename}")
        print("="*50)
    
    except socket.timeout:
        print(f"Connection timeout - Is Teensy at {TEENSY_IP}?")
        sys.exit(1)
    except ConnectionRefusedError:
        print(f"Connection refused - Is Teensy TCP server running?")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
    finally:
        sock.close()

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("\nStopped by user")
        sys.exit(0)
