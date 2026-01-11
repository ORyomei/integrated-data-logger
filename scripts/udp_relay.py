#!/usr/bin/env python3
"""
UDP Relay: Windows -> WSL
Windowsで実行してTeensyからのパケットをWSLに転送
"""

import socket
import sys

LISTEN_PORT = 8888
WSL_IP = "172.25.194.32"  # ← WSLのIPアドレスに変更してください
WSL_PORT = 8888

def main():
    # Windows側で受信
    listen_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listen_sock.bind(('0.0.0.0', LISTEN_PORT))
    
    # WSLに転送
    forward_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    print(f"UDP Relay started: 0.0.0.0:{LISTEN_PORT} -> {WSL_IP}:{WSL_PORT}")
    print("Waiting for packets...")
    
    packet_count = 0
    try:
        while True:
            data, addr = listen_sock.recvfrom(65535)
            forward_sock.sendto(data, (WSL_IP, WSL_PORT))
            packet_count += 1
            if packet_count % 10 == 0:
                print(f"Forwarded {packet_count} packets")
    except KeyboardInterrupt:
        print(f"\nTotal forwarded: {packet_count} packets")

if __name__ == '__main__':
    main()
