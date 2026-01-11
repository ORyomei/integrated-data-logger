#!/usr/bin/env python3
"""
UDP Data Viewer for Integrated Data Logger
Real-time plotting of ADC data from Teensy 4.1 via UDP
Requires: matplotlib, numpy
"""
import socket
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from collections import deque
import argparse

# UDP設定
UDP_IP = "0.0.0.0"
UDP_PORT = 8888

# データバッファ (最新1000サンプル分を保持)
BUFFER_SIZE = 1000
data_buffers = [deque(maxlen=BUFFER_SIZE) for _ in range(8)]
time_buffer = deque(maxlen=BUFFER_SIZE)

# ソケット作成
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))
sock.settimeout(0.01)  # 10ms timeout

def voltage_from_adc(adc_value):
    """ADC値を電圧に変換"""
    return (adc_value / 32768.0) * 10.0

def update_data():
    """UDPデータを受信してバッファを更新"""
    try:
        data, addr = sock.recvfrom(1024)
        line = data.decode('utf-8').strip()
        values = line.split(',')
        
        if len(values) == 9:  # timestamp + 8 channels
            timestamp = float(values[0]) / 1000.0  # ms to seconds
            time_buffer.append(timestamp)
            
            for i in range(8):
                adc_value = int(values[i+1])
                voltage = voltage_from_adc(adc_value)
                data_buffers[i].append(voltage)
                
            return True
    except socket.timeout:
        pass
    except Exception as e:
        print(f"Error: {e}")
    
    return False

def animate(frame):
    """アニメーション更新関数"""
    # データ受信 (複数パケット処理)
    for _ in range(10):
        update_data()
    
    if len(time_buffer) == 0:
        return
    
    # プロット更新
    times = np.array(time_buffer)
    times = times - times[0]  # 相対時間に変換
    
    for i, (ax, buf) in enumerate(zip(axes, data_buffers)):
        ax.clear()
        if len(buf) > 0:
            voltages = np.array(buf)
            ax.plot(times, voltages, linewidth=0.5)
            ax.set_ylabel(f'CH{i} [V]')
            ax.set_ylim(-10, 10)
            ax.grid(True, alpha=0.3)
            
            # 最新値を表示
            latest_voltage = voltages[-1]
            ax.text(0.02, 0.95, f'{latest_voltage:.3f}V', 
                   transform=ax.transAxes, 
                   verticalalignment='top',
                   bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    axes[-1].set_xlabel('Time [s]')
    plt.tight_layout()

def main():
    parser = argparse.ArgumentParser(description='Real-time ADC data viewer')
    parser.add_argument('--port', type=int, default=8888, help='UDP port (default: 8888)')
    parser.add_argument('--channels', type=int, nargs='+', default=list(range(8)), 
                       help='Channels to display (default: all)')
    args = parser.parse_args()
    
    global UDP_PORT, axes
    UDP_PORT = args.port
    
    # ソケット再作成
    global sock
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))
    sock.settimeout(0.01)
    
    print(f"Listening on {UDP_IP}:{UDP_PORT}")
    print(f"Displaying channels: {args.channels}")
    print("Close window to exit")
    
    # プロット設定
    num_channels = len(args.channels)
    fig, axes_temp = plt.subplots(num_channels, 1, figsize=(12, 2*num_channels), sharex=True)
    
    if num_channels == 1:
        axes = [axes_temp]
    else:
        axes = axes_temp
    
    fig.suptitle('Real-time ADC Data')
    
    # アニメーション開始
    ani = FuncAnimation(fig, animate, interval=50, cache_frame_data=False)
    plt.show()
    
    sock.close()

if __name__ == '__main__':
    main()
