#!/usr/bin/env python3
"""
UDP Data Receiver for Integrated Data Logger
Receives ADC data from Teensy 4.1 via UDP and saves to CSV file
"""
import socket
import csv
from datetime import datetime
import argparse

def main():
    parser = argparse.ArgumentParser(description='Receive ADC data via UDP')
    parser.add_argument('--ip', default='0.0.0.0', help='IP address to bind (default: 0.0.0.0)')
    parser.add_argument('--port', type=int, default=8888, help='UDP port (default: 8888)')
    parser.add_argument('--output', default=None, help='Output CSV filename (default: auto-generated)')
    args = parser.parse_args()
    
    # UDP設定
    UDP_IP = args.ip
    UDP_PORT = args.port
    
    # CSVファイル名
    if args.output:
        csv_filename = args.output
    else:
        csv_filename = f"data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    
    # ソケット作成
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))
    
    print(f"Listening on {UDP_IP}:{UDP_PORT}")
    print(f"Saving to {csv_filename}")
    print("Press Ctrl+C to stop")
    
    # CSVファイルをオープン
    with open(csv_filename, 'w', newline='') as csvfile:
        csv_writer = csv.writer(csvfile)
        # ヘッダー書き込み
        csv_writer.writerow(['Timestamp(ms)', 'CH0', 'CH1', 'CH2', 'CH3', 
                            'CH4', 'CH5', 'CH6', 'CH7'])
        
        try:
            packet_count = 0
            error_count = 0
            start_time = datetime.now()
            
            while True:
                data, addr = sock.recvfrom(1024)
                
                # デコードしてCSVに書き込み
                try:
                    line = data.decode('utf-8').strip()
                    values = line.split(',')
                    
                    if len(values) == 9:  # timestamp + 8 channels
                        csv_writer.writerow(values)
                        
                        packet_count += 1
                        if packet_count % 100 == 0:  # 1秒ごとに表示
                            elapsed = (datetime.now() - start_time).total_seconds()
                            rate = packet_count / elapsed if elapsed > 0 else 0
                            print(f"Packets: {packet_count}, Rate: {rate:.1f} Hz, "
                                  f"Errors: {error_count}, Last: {values[1:4]}...")
                    else:
                        error_count += 1
                        print(f"Invalid packet length: {len(values)} (expected 9)")
                        
                except UnicodeDecodeError as e:
                    error_count += 1
                    print(f"Decode error: {e}")
                
        except KeyboardInterrupt:
            elapsed = (datetime.now() - start_time).total_seconds()
            rate = packet_count / elapsed if elapsed > 0 else 0
            print(f"\n--- Statistics ---")
            print(f"Total packets: {packet_count}")
            print(f"Error packets: {error_count}")
            print(f"Duration: {elapsed:.1f} seconds")
            print(f"Average rate: {rate:.1f} Hz")
            print(f"Data saved to: {csv_filename}")
            sock.close()

if __name__ == '__main__':
    main()
