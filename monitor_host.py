"""
智能环境监测终端 - PC上位机监控程序 (v2.0)
功能：
1. 通过 pyserial 读取 COM 串口的二进制协议流
2. 按照自定义协议帧（0xAA 0x55 引导，校验和与 0x0D 尾标）解包
3. 使用 matplotlib 实时绘制温湿度动态曲线
"""

import sys
import time
import struct
import threading
import argparse
from collections import deque
from datetime import datetime

import serial
import serial.tools.list_ports
import matplotlib.pyplot as plt
import matplotlib.animation as animation

# 设置 Matplotlib 中文字体兼容（Windows 系统首选微黑或黑体）
plt.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'SimHei', 'Arial']
plt.rcParams['axes.unicode_minus'] = False

# 协议常量定义 (与 STM32 端 protocol.h 严格对应)
FRAME_HEADER_1 = 0xAA
FRAME_HEADER_2 = 0x55
FRAME_TAIL     = 0x0D
TYPE_ENV_DATA  = 0x01
FRAME_LEN      = 9


class ProtocolParser:
    """协议解析与帧校验器"""
    def __init__(self):
        self.buffer = bytearray()

    def parse_stream(self, new_bytes):
        """
        向缓冲区追加字节流，并尝试提取所有合法的数据帧
        返回列表: [(temp, humi), ...]
        """
        self.buffer.extend(new_bytes)
        parsed_data = []

        while len(self.buffer) >= FRAME_LEN:
            # 1. 寻找帧头 0xAA 0x55
            if self.buffer[0] != FRAME_HEADER_1 or self.buffer[1] != FRAME_HEADER_2:
                # 丢弃非帧头字节，滑动窗口继续寻找
                self.buffer.pop(0)
                continue

            # 2. 检查帧尾 0x0D
            if self.buffer[8] != FRAME_TAIL:
                # 帧尾不匹配，说明不是正确帧，滑动 1 字节继续找
                self.buffer.pop(0)
                continue

            # 3. 截取整帧候选
            frame = self.buffer[:FRAME_LEN]

            # 4. 校验类型码
            pkg_type = frame[2]
            if pkg_type != TYPE_ENV_DATA:
                self.buffer.pop(0)
                continue

            # 5. 校验和验证 (累加 Type 到 Data，低8位)
            calc_checksum = sum(frame[2:7]) & 0xFF
            recv_checksum = frame[7]

            if calc_checksum != recv_checksum:
                print(f"[警告] 校验和错误! 计算值: 0x{calc_checksum:02X}, 接收值: 0x{recv_checksum:02X}")
                self.buffer.pop(0)
                continue

            # 6. 数据解包 (大端序 int16 温度，uint16 湿度)
            # frame[3:5] 为温度高低位，frame[5:7] 为湿度高低位
            raw_temp = struct.unpack('>h', frame[3:5])[0]
            raw_humi = struct.unpack('>H', frame[5:7])[0]

            temp = raw_temp / 10.0
            humi = raw_humi / 10.0

            parsed_data.append((temp, humi))

            # 成功解析一整帧，移出已处理的 9 字节
            del self.buffer[:FRAME_LEN]

        return parsed_data


class SerialMonitor:
    """串口后台采集与数据维护"""
    def __init__(self, port="COM3", baudrate=115200, max_points=100):
        self.port = port
        self.baudrate = baudrate
        self.max_points = max_points
        self.running = False
        self.ser = None

        # 双端队列用于存储近期时序数据
        self.times = deque(maxlen=max_points)
        self.temps = deque(maxlen=max_points)
        self.humis = deque(maxlen=max_points)
        self.lock = threading.Lock()

        self.parser = ProtocolParser()
        self.rx_count = 0
        self.last_temp = None
        self.last_humi = None

    def start(self):
        """开启后台读取线程"""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=0.2)
            print(f"[OK] 成功打开串口: {self.port}, 波特率: {self.baudrate}")
        except Exception as e:
            print(f"[错误] 无法打开串口 {self.port}: {e}")
            print("当前系统可用串口如下：")
            for p in serial.tools.list_ports.comports():
                print(f"  - {p.device}: {p.description}")
            return False

        self.running = True
        self.thread = threading.Thread(target=self._worker, daemon=True)
        self.thread.start()
        return True

    def _worker(self):
        """串口接收工作线程"""
        while self.running:
            try:
                if self.ser.in_waiting > 0:
                    raw = self.ser.read(self.ser.in_waiting)
                    results = self.parser.parse_stream(raw)
                    if results:
                        now_str = datetime.now().strftime("%H:%M:%S")
                        with self.lock:
                            for temp, humi in results:
                                self.times.append(now_str)
                                self.temps.append(temp)
                                self.humis.append(humi)
                                self.last_temp = temp
                                self.last_humi = humi
                                self.rx_count += 1
                                print(f"[{now_str}] 接收成功 (包 #{self.rx_count}) -> 温度: {temp:5.1f} ℃ | 湿度: {humi:5.1f} %")
                else:
                    time.sleep(0.05)
            except Exception as e:
                print(f"[异常] 串口读取异常: {e}")
                break

    def stop(self):
        self.running = False
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("[OK] 串口已安全关闭")

    def get_data(self):
        with self.lock:
            return list(self.times), list(self.temps), list(self.humis)


def setup_plot(monitor):
    """配置 Matplotlib 交互界面与实时绘图"""
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 7), sharex=True)
    fig.canvas.manager.set_window_title(f"智能环境监测终端 - 上位机系统 ({monitor.port})")

    # 1. 温度子图配置
    line_temp, = ax1.plot([], [], color='#FF5722', linewidth=2, marker='o', markersize=4, label='温度 (℃)')
    ax1.set_ylabel('温度 (℃)', fontsize=11, fontweight='bold', color='#D84315')
    ax1.set_title('智能环境监测终端 (STM32 + AHT20) 实时遥测数据', fontsize=14, pad=12)
    ax1.grid(True, linestyle='--', alpha=0.6)
    ax1.legend(loc='upper left')
    text_temp = ax1.text(0.98, 0.90, '', transform=ax1.transAxes, ha='right', va='top',
                          fontsize=13, fontweight='bold', color='#D84315',
                          bbox=dict(boxstyle='round,pad=0.4', facecolor='#FFF3E0', edgecolor='#FF8A65'))

    # 2. 湿度子图配置
    line_humi, = ax2.plot([], [], color='#0288D1', linewidth=2, marker='s', markersize=4, label='湿度 (%RH)')
    ax2.set_xlabel('采集时间 (最近数据)', fontsize=11, fontweight='bold')
    ax2.set_ylabel('湿度 (%RH)', fontsize=11, fontweight='bold', color='#0277BD')
    ax2.grid(True, linestyle='--', alpha=0.6)
    ax2.legend(loc='upper left')
    text_humi = ax2.text(0.98, 0.90, '', transform=ax2.transAxes, ha='right', va='top',
                          fontsize=13, fontweight='bold', color='#0277BD',
                          bbox=dict(boxstyle='round,pad=0.4', facecolor='#E1F5FE', edgecolor='#4FC3F7'))

    fig.tight_layout()

    def update_frame(frame_num):
        times, temps, humis = monitor.get_data()
        if not times:
            return line_temp, line_humi, text_temp, text_humi

        x_indices = list(range(len(times)))

        # 更新温度曲线
        line_temp.set_data(x_indices, temps)
        ax1.set_xlim(0, max(len(times) - 1, 10))
        t_min, t_max = min(temps), max(temps)
        margin_t = max(1.0, (t_max - t_min) * 0.2)
        ax1.set_ylim(t_min - margin_t, t_max + margin_t)
        text_temp.set_text(f"当前温度: {temps[-1]:.1f} ℃")

        # 更新湿度曲线
        line_humi.set_data(x_indices, humis)
        ax2.set_xlim(0, max(len(times) - 1, 10))
        h_min, h_max = min(humis), max(humis)
        margin_h = max(2.0, (h_max - h_min) * 0.2)
        ax2.set_ylim(max(0, h_min - margin_h), min(100, h_max + margin_h))
        text_humi.set_text(f"当前湿度: {humis[-1]:.1f} %")

        # 仅在 X 轴稀疏标注时间戳，防止密集重叠
        step = max(1, len(times) // 8)
        selected_idx = list(range(0, len(times), step))
        if selected_idx[-1] != len(times) - 1:
            selected_idx.append(len(times) - 1)
        ax2.set_xticks(selected_idx)
        ax2.set_xticklabels([times[i] for i in selected_idx], rotation=20, ha='right', fontsize=9)

        return line_temp, line_humi, text_temp, text_humi

    ani = animation.FuncAnimation(fig, update_frame, interval=300, blit=False)
    return fig, ani


def main():
    parser = argparse.ArgumentParser(description="STM32 智能环境监测终端 - 上位机监控")
    parser.add_argument("--port", type=str, default="COM3", help="串口号 (默认: COM3)")
    parser.add_argument("--baud", type=int, default=115200, help="波特率 (默认: 115200)")
    parser.add_argument("--points", type=int, default=60, help="画幅保留最大数据点数 (默认: 60)")
    args = parser.parse_args()

    print("==================================================")
    print("      STM32 智能环境监测终端 - 上位机遥测系统       ")
    print(f"      串口: {args.port} | 波特率: {args.baud}      ")
    print("==================================================")

    monitor = SerialMonitor(port=args.port, baudrate=args.baud, max_points=args.points)
    if not monitor.start():
        sys.exit(1)

    fig, ani = setup_plot(monitor)

    try:
        plt.show()
    except KeyboardInterrupt:
        pass
    finally:
        monitor.stop()


if __name__ == "__main__":
    main()
