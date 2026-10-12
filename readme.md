# 基于 STM32F103 的环境监测系统 (v2.0 通信与上位机版)

[![Release](https://img.shields.io/badge/Release-v2.0-blue.svg)](https://github.com/jvjjjj/Environment_Monitor/releases/tag/v2.0)
[![MCU](https://img.shields.io/badge/MCU-STM32F103C8T6-green.svg)](https://www.st.com)
[![Protocol](https://img.shields.io/badge/Protocol-Binary%20Frame-orange.svg)]()
[![Host](https://img.shields.io/badge/Host-Python%20(PySerial%20%2B%20Matplotlib)-brightgreen.svg)]()
[![Compiler](https://img.shields.io/badge/Compiler-ARMCLANG%20(AC6)-purple.svg)](https://developer.arm.com)

> 💡 **项目演进导读**：本项目是一套从零构建的嵌入式环境监测终端，分为三个递进演进阶段：
> * **v1.0**：单片机裸机驱动与总线复用（软件模拟 I2C、OLED 1KB 显存双缓冲、AHT20 采集，见分支 [`v1.0`](https://github.com/jvjjjj/Environment_Monitor/tree/v1.0) / [`v1`](https://github.com/jvjjjj/Environment_Monitor/tree/v1)）。
> * **v2.0（当前版本）**：**串口通信协议设计与上位机交互系统**。告别低效的 `printf` 纯文本，自主设计工业级抗干扰定长二进制协议帧，搭配 Python (PySerial + Matplotlib) 动态波形上位机，实现端到端软硬件闭环。
> * **v3.0**：重构为 FreeRTOS 实时多任务系统，并融合 BMP280 气压海拔传感器（见分支 [`v3.0`](https://github.com/jvjjjj/Environment_Monitor/tree/v3.0) / [`master`](https://github.com/jvjjjj/Environment_Monitor)）。

---

## 目录 (Table of Contents)

1. [项目目标与核心解决的问题](#1-项目目标与核心解决的问题)
2. [【核心必读】通信协议与上位机专业名词通俗大白话词典](#2-核心必读通信协议与上位机专业名词通俗大白话词典)
   * [2.1 为什么工业界严禁用 printf("temp=...") 传数据？](#21-为什么工业界严禁用-printftemp-传数据)
   * [2.2 UART / USART、波特率与 TX/RX 交叉接线](#22-uart--usart波特率与-txrx-交叉接线)
   * [2.3 通信帧 (Frame)、帧头 (Header) 与帧尾 (Tail)](#23-通信帧-frame帧头-header-与帧尾-tail)
   * [2.4 校验和 (Checksum) 与抗电磁干扰原理](#24-校验和-checksum-与抗电磁干扰原理)
   * [2.5 流式传输 (Stream) 与粘包/断包现象](#25-流式传输-stream-与粘包断包现象)
   * [2.6 定点数放大传输法 (Fixed-Point Scaling)](#26-定点数放大传输法-fixed-point-scaling)
   * [2.7 大端序 (Big-Endian) 与小端序 (Little-Endian)](#27-大端序-big-endian-与小端序-little-endian)
   * [2.8 Python 上位机：PySerial、线程锁 (Lock) 与双端队列 (deque)](#28-python-上位机pyserial线程锁-lock-与双端队列-deque)
3. [硬件清单与接线实战指南 (CH340 USB-TTL)](#3-硬件清单与接线实战指南-ch340-usb-ttl)
4. [核心源码深度剖析：代码是怎么写的？用意何在？](#4-核心源码深度剖析代码是怎么写的用意何在)
   * [4.1 STM32 端：二进制协议封包 (`protocol.c`) 逐行拆解](#41-stm32-端二进制协议封包-protocolc-逐行拆解)
   * [4.2 PC 端：滑动窗口帧同步状态机 (`ProtocolParser`) 源码解密](#42-pc-端滑动窗口帧同步状态机-protocolparser-源码解密)
   * [4.3 PC 端：后台多线程采集与数据管道 (`SerialMonitor`)](#43-pc-端后台多线程采集与数据管道-serialmonitor)
   * [4.4 PC 端：Matplotlib 动态波形平滑渲染动画](#44-pc-端matplotlib-动态波形平滑渲染动画)
   * [4.5 STM32 端：主程序循环调度 (`main.c`)](#45-stm32-端主程序循环调度-mainc)
5. [快速上手：单片机烧录与 Python 上位机运行指南](#5-快速上手单片机烧录与-python-上位机运行指南)
6. [初学者高频排障笔记 (Troubleshooting)](#6-初学者高频排障笔记-troubleshooting)

---

## 1. 项目目标与核心解决的问题

很多初学者在简历上写“熟练掌握串口通信”，但一写代码就是简单粗暴地打一行：
```c
printf("Temp: %.1f C, Humi: %.1f %%\r\n", temp, humi); // 学生级纯文本写法
```
在真正的工业级嵌入式与 IoT 产品中，**这种做法是绝对禁止的**！

**v2.0 要解决的 4 大核心痛点**：
1. **解析开销巨大**：上位机如果要从一串长文本里用正则表达式提取温度数字，不仅极其消耗 CPU 算力，而且文本格式一旦变动（比如多了一个空格），上位机立刻解析崩溃。
2. **缺乏防篡改与完整性校验**：物理通信导线如果受到电机启停、静电打火干扰，电压瞬时翻转，`25.4` 可能会变成 `85.4`。纯文本无法识别这种错码，而二进制帧通过校验和能精准过滤掉一切误码。
3. **断包与粘包（边界模糊）**：串口接收是流式的，一串字节接踵而至。如果缺乏明确的帧头引导与长度规约，接收端根本不知道从哪个字节开始算一包完整的传感器数据。
4. **PC 端多线程可视化**：单片机把数据发出来后，PC 端如何通过 Python 建立后台监听线程、如何用线程安全队列解耦数据流、如何利用 Matplotlib 绘制出 60FPS 丝滑刷新的实时双轴动态温湿度波形。

---

## 2. 【核心必读】通信协议与上位机专业名词通俗大白话词典

### 2.1 为什么工业界严禁用 printf("temp=...") 传数据？
* **通信协议是给“机器”看的，不是给“人”看的**！
  * 纯文本字符串（如 `printf("Temp=25.4")`）包含多个 ASCII 字符，总共占了 9 个字节；
  * 如果以二进制整型传输数值 `254`，只需要 **2 个字节**！传输效率提升了整整 **4.5 倍**！
  * 在远距离通信（如无线 LoRa、4G、NB-IoT 或长总线）中，数据包越小，丢包概率越低，电量消耗越少。

---

### 2.2 UART / USART、波特率与 TX/RX 交叉接线
* **UART (Universal Asynchronous Receiver/Transmitter，通用异步收发器)**：
  * **异步通信 (Asynchronous)**：不需要时钟同步线（SCL），通信双方只用两根信号线（一根发、一根收）。
* **波特率 (Baud Rate)**：
  * 既然没有时钟线来打节拍，双方怎么知道对方发过来的一位高电平持续了多久？
  * 答案是事先约定好统一的“说话语速”，也就是**波特率（每秒传输的二进制比特位数）**。
  * 本工程设定为 **115200 bps**：意味着传输 1 位（Bit）的时间约为 $\frac{1}{115200} \approx 8.68 \ \mu\text{s}$。发送方按这个时间间隔改变电平，接收方也按这个时间间隔取样，双方必须完全一致，否则收到的全是乱码！
* **TX 与 RX 交叉连接规则**：
  * **TX (Transmit)**：数据发送引脚。
  * **RX (Receive)**：数据接收引脚。
  * **核心接线法则**：**我的嘴巴对着你的耳朵！** 即 STM32 的 TX 必须接电脑的 RX，STM32 的 RX 必须接电脑的 TX。如果 TX 接 TX，就像两张嘴巴互相喊话，什么都收不到。

```text
    ┌─────────────┐             ┌─────────────┐
    │ STM32F103   │             │ USB-TTL 转接│
    │             │   TX ───► RX│ (CH340模块) │ ───► 电脑 USB 接口
    │ PA9  (USART1)             │             │
    │ PA10 (USART1)   RX ◄─── TX│             │
    │ GND         │   GND ── GND│             │
    └─────────────┘             └─────────────┘
```

---

### 2.3 通信帧 (Frame)、帧头 (Header) 与帧尾 (Tail)
* **通信帧 (Frame)**：为了保证数据完整有序，把一组有业务关联的字节按照特定格式捆绑打包成一个整体，这个整体就叫做“一帧”。
* **帧头 (Frame Header，本协议采用 `0xAA 0x55`)**：
  * 相当于一封信信封上的“特快专递”印章。在杂乱无章的字节流中，只要接收方连续看到 `0xAA 0x55`，就知道紧接着有一包新数据来了！
  * **为什么选 `0xAA 0x55`？**
    * `0xAA` 的二进制是 `10101010`；
    * `0x55` 的二进制是 `01010101`；
    * 这两个字节具有极致对称、高低电平交替跳变的特性。外界电磁噪音很难凑巧产生这种有规律的跳变波形，因此抗噪性极佳，极难被外界干扰误触发！
* **帧尾 (Frame Tail，本协议采用 `0x0D`)**：
  * 相当于一句话句末的句号（`0x0D` 是 ASCII 码中的回车符 `\r`）。接收端只有同时对齐了帧头和帧尾，才承认这是一包没被截断的完整数据。

---

### 2.4 校验和 (Checksum) 与抗电磁干扰原理
* **为什么需要校验和？**
  * 工业现场如果旁边有大功率电机启动，导线上可能产生数伏的感应毛刺，导致某个字节的某个位从 0 突变成 1。
* **累加校验和原理 (Additive Checksum)**：
  * 发送方把要传的所有有效数据（Type、Temp、Humi 等）按字节全部加在一起，取结果的最低 8 位（`Sum & 0xFF`），存放在帧尾部的校验位中。
  * 接收方收到后，用同样的方法重新算一遍累加和。如果自己算的和收到的校验码一模一样，说明数据在路上没有被篡改；只要有任何 1 个 bit 发生错误，两边校验和就会对不上，上位机果断丢弃这包坏数据，绝不上屏误导用户。

---

### 2.5 流式传输 (Stream) 与粘包/断包现象
* **流式传输 (Stream)**：串口硬件底层就像一根自来水管，字节像水滴一样一个接一个流出。
* **粘包 (Packet Sticking)**：如果发送方发得很快，或者操作系统的串口缓冲区没有及时读走，两次发送的数据可能会粘在一起（例如 `[包1][包2]` 连续到达），一次性读出。
* **断包 (Packet Fragmentation)**：如果一包 9 字节的数据刚传到一半（比如传了 5 字节），上位机程序就执行了一次读取，导致这包数据被硬生生切成了两半。
* **解法**：上位机绝不能假设“读一次串口就刚好能得到一包数据”，必须在内存中建立一个动态累积缓冲区，采用**滑动窗口状态机**逐字节扫描对齐帧头帧尾。

---

### 2.6 定点数放大传输法 (Fixed-Point Scaling)
* 很多传感器数据都是带小数的浮点数（例如温度 `25.4 ℃`，湿度 `58.2 %`）。
* **传统浮点传输**：单片机如果直接传 32 位标准 IEEE 754 float，要占 4 字节，且在低端芯片上做浮点运算不仅耗电而且极慢。
* **工业定点数做法**：
  $$\text{整数传输值} = \text{round}(\text{物理浮点数} \times 10)$$
  * $25.4^\circ\text{C} \times 10 \rightarrow 254$（只需占 2 字节 `int16_t`，还能天然支持负温，如 $-12.3^\circ\text{C} \rightarrow -123$）。
  * 上位机收到整数 `254` 后，除以 $10.0$，瞬间无损还原出浮点数 `25.4`。既节省了一半的带宽，又完全避免了跨芯片架构时浮点数大小端解析兼容性风险。

---

### 2.7 大端序 (Big-Endian) 与小端序 (Little-Endian)
一个 16 位的两字节整数（比如 `254`，十六进制为 `0x00FE`），在内存里有两个字节：高字节 `0x00` 和低字节 `0xFE`。传输时谁先走？
* **大端模式 (Big-Endian)**：**高位字节排在前面先传**（先传 `0x00`，再传 `0xFE`）。这最符合人类阅读数字的习惯，也是网络通信协议的通用标准。
* **小端模式 (Little-Endian)**：低位字节排在前面先传（先传 `0xFE`，再传 `0x00`）。ARM 单片机内部内存默认采用小端序。
* **本协议规范**：网络大端序规范。单片机发送时通过 `>> 8` 先抽出高字节，上位机用 Python 的 `struct.unpack('>h')` 按大端解析。

---

### 2.8 Python 上位机：PySerial、线程锁 (Lock) 与双端队列 (deque)
* **PySerial**：Python 官方著名的底层串口操作驱动库，负责在 Windows 上打开 `COM3` 等虚拟串口设备，配置波特率并收发二进制字节流。
* **双端队列 (`collections.deque`)**：
  * 动态波形图只显示最近的 100 个历史数据点。如果用普通列表 `list`，每次从头部删除老数据需要把后面几万个元素整体平移，速度极慢。
  * `deque(maxlen=100)` 具有固定容量。当塞入第 101 个数据时，最老的数据自动从另一端瞬间弹出，时间复杂度为严格的 $O(1)$，效率极高。
* **线程锁 (`threading.Lock`) 与竞态条件 (Race Condition)**：
  * 在上位机中，**后台串口采集线程**（拼命往队列里写数据）和 **GUI 绘图主线程**（拼命从队列里读数据画曲线）是并发同时运行的！
  * 如果绘图线程正在画第 50 个点，采集线程突然插入清空了队列，程序就会瞬间抛出 `IndexError` 崩溃闪退。
  * **解法**：通过 `threading.Lock()`。任何一方访问共享队列前，必须先“加锁”；操作完毕后“解锁”。保证同一时刻只能有一个线程读写队列，确保程序永不闪退。

---

## 3. 硬件清单与接线实战指南 (CH340 USB-TTL)

在 v1.0 的硬件基础上（OLED + AHT20 继续保留），我们新增了 **CH340 USB-TTL 串口模块**：

```text
               ┌───────────────────────┐
               │    STM32F103C8T6      │
               │                       │
               │   PA9   PA10   GND    │
               └───┬──────┬──────┬─────┘
                   │      │      │
            USART1 │      │      │
                TX │   RX │      │
                   ▼      ▲      │
                   RX     TX     │
               ┌───┴──────┴──────┴─────┐
               │      CH340 模块       │
               │  (USB 转 TTL 串口)    │
               └───────────┬───────────┘
                           │ USB 插头
                           ▼
                    电脑 PC (COM3)
```

### 引脚接线对照表

| STM32 物理引脚 | 外设引脚 (CH340 模块) | 信号定义 | 核心注意细节 |
| :--- | :--- | :--- | :--- |
| **PA9** | CH340 **`RX`** | STM32 串口发送端 (USART1_TX) | **交叉接线！** 单片机的 TX 接模块的 RX。 |
| **PA10** | CH340 **`TX`** | STM32 串口接收端 (USART1_RX) | **交叉接线！** 单片机的 RX 接模块的 TX。 |
| **GND** | CH340 **`GND`** | 电源参考地 | **必须共地！** 若不接地，信号没有参考电压，全是乱码。 |
| *(不接)* | CH340 **`5V/3V3`** | 模块电源引脚 | **悬空不接！** 单片机由 ST-Link 或独立电源供电，切勿双重供电。 |

---

## 4. 核心源码深度剖析：代码是怎么写的？用意何在？

本章对 v2.0 的自定义二进制协议封包及 Python 上位机流式解包状态机进行**逐行拆解与工程意图剖析**。

### 协议物理帧结构表 (9 字节定长帧)

| 字节偏移 | 字段名称 | 长度 | 取值 / 规约 | 功能说明 |
| :---: | :--- | :---: | :--- | :--- |
| **Byte 0** | **帧头 1 (Header 1)** | 1 | 固定 `0xAA` | 标识一帧的开始（交替电平抗噪） |
| **Byte 1** | **帧头 2 (Header 2)** | 1 | 固定 `0x55` | 双重引导头防误触发 |
| **Byte 2** | **数据类型 (Type)**   | 1 | 固定 `0x01` | 功能码：`0x01` 代表环境温湿度数据包 |
| **Byte 3** | **温度高字节 (Temp_H)** | 1 | `int16_t >> 8` | 温度放大 10 倍的大端高 8 位 |
| **Byte 4** | **温度低字节 (Temp_L)** | 1 | `int16_t & 0xFF` | 温度放大 10 倍的大端低 8 位 |
| **Byte 5** | **湿度高字节 (Humi_H)** | 1 | `uint16_t >> 8` | 湿度放大 10 倍的大端高 8 位 |
| **Byte 6** | **湿度低字节 (Humi_L)** | 1 | `uint16_t & 0xFF` | 湿度放大 10 倍的大端低 8 位 |
| **Byte 7** | **校验和 (Checksum)** | 1 | `uint8_t` | Byte 2 至 Byte 6 的累加和低 8 位 |
| **Byte 8** | **帧尾 (Tail)**       | 1 | 固定 `0x0D` | 结束标志（ASCII `\r`），确认封包未断裂 |

---

### 4.1 STM32 端：二进制协议封包 (`protocol.c`) 逐行拆解

打开 [`Hardware/protocol.c`](Hardware/protocol.c)：

```c
#include "protocol.h"
#include "usart.h"

void Protocol_SendEnvData(float temp, float humi)
{
    uint8_t frame[9];
    uint8_t checksum = 0;
    
    // 步骤 1: 物理量放大 10 倍转为整型
    int16_t t_int = (int16_t)(temp * 10.0f);
    uint16_t h_int = (uint16_t)(humi * 10.0f);

    // 步骤 2: 填充引导头与类型码
    frame[0] = FRAME_HEADER_1; // 0xAA
    frame[1] = FRAME_HEADER_2; // 0x55
    frame[2] = TYPE_ENV_DATA;  // 0x01
    
    // 步骤 3: 大端模式拆分高低字节并载入负载
    frame[3] = (uint8_t)(t_int >> 8);   // 温度高8位
    frame[4] = (uint8_t)(t_int & 0xFF); // 温度低8位
    frame[5] = (uint8_t)(h_int >> 8);   // 湿度高8位
    frame[6] = (uint8_t)(h_int & 0xFF); // 湿度低8位

    // 步骤 4: 累加有效载荷计算校验和
    for (uint8_t i = 2; i <= 6; i++) {
        checksum += frame[i];
    }
    frame[7] = checksum;
    
    // 步骤 5: 封上帧尾
    frame[8] = FRAME_TAIL; // 0x0D

    // 步骤 6: 调用 HAL 库串口阻塞发送 9 个字节
    HAL_UART_Transmit(&huart1, frame, 9, 100);
}
```

* **为什么这样写？用意何在？**
  * `(int16_t)(temp * 10.0f)`：把浮点数 `25.4f` 乘 10 得到整数 `254`。强制转换为有符号的 `int16_t`，这样哪怕在严寒冬天下温度达到 $-15.6^\circ\text{C}$，变成 $-156$，通过补码也能完美传输。
  * `t_int >> 8` 与 `t_int & 0xFF`：把一个 16 位整数拆成两个 8 位单字节。先右移 8 位把高 8 位推到底部存入 `frame[3]`，低 8 位直接掩码提取存入 `frame[4]`。严格按照大端序存放。
  * `checksum += frame[i]`：在 C 语言中，`uint8_t` 的变量发生加法溢出时会自动对 256 取模（即自然保留低 8 位），省去了显式的取模指令，执行极快。
  * `HAL_UART_Transmit(&huart1, frame, 9, 100)`：通过 STM32 硬件 USART1 发送，超时阈值设为 100ms。115200 波特率下发送 9 字节耗时仅需约 $0.78\text{ms}$，绝对不会产生超时。

---

### 4.2 PC 端：滑动窗口帧同步状态机 (`ProtocolParser`) 源码解密

打开 [`monitor_host.py`](monitor_host.py)，这是上位机最核心的算法类：

```python
class ProtocolParser:
    def __init__(self):
        self.buffer = bytearray() # 动态流式字节缓冲区

    def parse_stream(self, new_bytes):
        self.buffer.extend(new_bytes) # 新到的水流注入池子
        parsed_data = []

        # 只要池子里的字节数够一包(9字节)，就尝试解包
        while len(self.buffer) >= 9:
            # 步骤 1: 寻找帧头 0xAA 0x55
            if self.buffer[0] != 0xAA or self.buffer[1] != 0x55:
                self.buffer.pop(0) # 不是帧头，弹出最前1个垃圾字节，继续往后滑
                continue

            # 步骤 2: 检查帧尾 0x0D
            if self.buffer[8] != 0x0D:
                self.buffer.pop(0) # 帧尾不对，说明这是巧合产生的伪帧头，滑动继续找
                continue

            # 步骤 3: 提取 9 字节候选包
            frame = self.buffer[:9]

            # 步骤 4: 校验和验证 (累加 Type 到 Data)
            calc_checksum = sum(frame[2:7]) & 0xFF
            recv_checksum = frame[7]

            if calc_checksum != recv_checksum:
                print(f"[警告] 校验和不匹配! 计算值: {calc_checksum}, 收到值: {recv_checksum}")
                self.buffer.pop(0)
                continue

            # 步骤 5: 二进制逆向大端解包
            raw_temp = struct.unpack('>h', frame[3:5])[0] # > 代表大端, h 代表 int16
            raw_humi = struct.unpack('>H', frame[5:7])[0] # > 代表大端, H 代表 uint16

            temp = raw_temp / 10.0 # 还原为带一位小数的浮点数
            humi = raw_humi / 10.0

            parsed_data.append((temp, humi))

            # 步骤 6: 成功消费完整一帧，从缓冲区切除前 9 字节
            del self.buffer[:9]

        return parsed_data
```

* **为什么这样写？用意何在？（流式容错算法哲学）**
  * **滑动窗口机制 (`buffer.pop(0)`)**：
    * 现实中的物理串口，插拔瞬间可能会产生随机电平干扰，生成一串无意义的杂乱垃圾字节。
    * 如果代码写成傻瓜式的 `data = ser.read(9)`，一旦错开了 1 个字节，后面读取的每一包就会永远错位（把上一包的尾巴当成下一包的头），全盘报废！
    * 我们通过 `while` 循环和 `pop(0)`，像筛子一样一字节一字节往前滑动扫描。无论前面混入了多少垃圾字符，只要一碰到 `0xAA 0x55` 并且第 8 个字节是 `0x0D`，状态机立刻自动锁定并实现**相位同步**，具备极强的抗噪自愈能力。
  * `struct.unpack('>h', frame[3:5])`：Python 官方二进制解包函数。`>` 强制以网络大端模式解析，`h` 自动处理补码有符号数，即使单片机送来负温度，Python 也能分毫不差地解析出负浮点数。

---

### 4.3 PC 端：后台多线程采集与数据管道 (`SerialMonitor`)

```python
class SerialMonitor:
    def __init__(self, port="COM3", baudrate=115200, max_points=100):
        self.times = deque(maxlen=max_points) # 存时间戳
        self.temps = deque(maxlen=max_points) # 存温度
        self.humis = deque(maxlen=max_points) # 存湿度
        self.lock = threading.Lock()          # 线程安全互斥锁
        self.parser = ProtocolParser()

    def _worker(self):
        """后台独立常驻读取线程"""
        while self.running:
            if self.ser.in_waiting > 0:
                raw = self.ser.read(self.ser.in_waiting) # 一次性全读出
                results = self.parser.parse_stream(raw)  # 状态机解包
                if results:
                    now_str = datetime.now().strftime("%H:%M:%S")
                    with self.lock: # 加锁保护共享数据队列
                        for temp, humi in results:
                            self.times.append(now_str)
                            self.temps.append(temp)
                            self.humis.append(humi)
                            print(f"[{now_str}] 接收成功 -> 温度: {temp:.1f} ℃ | 湿度: {humi:.1f} %")
            else:
                time.sleep(0.05) # 没数据时睡 50ms 释放 CPU
```

* **为什么这样写？用意何在？**
  * **GUI 与 IO 分离**：图形界面如果和串口读取写在同一个线程里，一旦串口等待数据或者串口卡顿，整个 Matplotlib 绘图窗口就会立刻无响应、弹窗未响应假死！
  * 我们开启了一个后台 `daemon` 守护线程，全天候静默抓取数据，抓到后用 `with self.lock:` 安全写进队列，主线程负责专心画图，界面丝滑流畅。

---

### 4.4 PC 端：Matplotlib 动态波形平滑渲染动画

```python
fig, (ax_temp, ax_humi) = plt.subplots(2, 1, figsize=(10, 6), sharex=True)
line_temp, = ax_temp.plot([], [], 'r-o', label='温度 (℃)', linewidth=1.5)
line_humi, = ax_humi.plot([], [], 'b-s', label='湿度 (%RH)', linewidth=1.5)

def update_plot(frame):
    with monitor.lock: # 线程安全读取
        if not monitor.times: return line_temp, line_humi
        t_list = list(monitor.times)
        temp_list = list(monitor.temps)
        humi_list = list(monitor.humis)

    # 动态更新曲线数据点
    line_temp.set_data(range(len(temp_list)), temp_list)
    line_humi.set_data(range(len(humi_list)), humi_list)
    
    # 动态适应坐标轴上下限
    ax_temp.set_ylim(min(temp_list) - 2, max(temp_list) + 2)
    ax_humi.set_ylim(max(0, min(humi_list) - 5), min(100, max(humi_list) + 5))
    return line_temp, line_humi

ani = animation.FuncAnimation(fig, update_plot, interval=500, blit=False)
plt.show()
```

* **为什么这样写？用意何在？**
  * `FuncAnimation` 定时器每 500ms 触发一次刷新。
  * `set_data` 只更新曲线的顶点坐标，而不需要把整个图表画布清空重画，极大降低了显卡和 CPU 占用率。
  * 上下两个子图分别用红色圆点（温度）和蓝色方块（湿度）呈现，带自动坐标轴缩放，直观清晰。

---

### 4.5 STM32 端：主程序循环调度 (`main.c`)

在 [`Core/Src/main.c`](Core/Src/main.c) 中：
```c
while (1)
{
    // 1. 本地采集温湿度
    if (AHT20_ReadData(&temp, &humi) == 0) {
        // 2. 本地 OLED 双缓冲排版刷新
        OLED_Clear();
        OLED_ShowString(0, 0, "Env Monitor v2.0", 16);
        sprintf(display_buf, "T:%.1fC H:%.1f%%", temp, humi);
        OLED_ShowString(0, 2, display_buf, 16);
        OLED_Update();
        
        // 3. 打包定长二进制协议帧，通过 USART1 发送给上位机
        Protocol_SendEnvData(temp, humi);
    }

    HAL_Delay(1000); // 1 秒业务周期
}
```
* **逻辑闭环**：单片机每秒采集一次环境物理量，一边送到本地 OLED 显示，一边组装 9 字节协议帧推向串口送达上位机，软硬协同全链路打通。

---

## 5. 快速上手：单片机烧录与 Python 上位机运行指南

### 5.1 第一步：STM32 固件编译与烧录
1. 使用 Keil 打开 `MDK-ARM/Environment_Monitor.uvprojx`。
2. 键盘按下 **F7**（Build）编译，确认编译输出为 **`0 Error(s), 0 Warning(s)`**。
3. 连接 ST-Link，按下 **F8**（Download）烧录固件。
4. 观察 OLED 屏幕第二行显示出当前温湿度。

### 5.2 第二步：连接 USB-TTL 串口
1. 将 CH340 模块的 TX 接 PA10、RX 接 PA9、GND 接 STM32 的 GND。
2. 将 CH340 模块插入电脑 USB 接口。
3. 打开 Windows 的 **设备管理器 $\rightarrow$ 端口 (COM 和 LPT)**，确认分配的端口号（例如 `COM3` 或 `COM4`）。

### 5.3 第三步：启动 Python 上位机
在项目根目录下打开命令行终端（PowerShell 或 CMD）：

```bash
# 1. 安装上位机所依赖的第三方库
pip install pyserial matplotlib

# 2. 运行上位机程序 (默认监听 COM3，波特率 115200)
python monitor_host.py

# 若设备管理器中你的串口是 COM4，可通过参数指定：
python monitor_host.py --port COM4
```

### 5.4 预期运行现象
* 命令行窗口开始实时打印解包日志：
  ```text
  [OK] 成功打开串口: COM3, 波特率: 115200
  [14:20:01] 接收成功 (包 #1) -> 温度:  25.4 ℃ | 湿度:  58.2 %
  [14:20:02] 接收成功 (包 #2) -> 温度:  25.4 ℃ | 湿度:  58.1 %
  ```
* 随后弹出一个高颜值的 Matplotlib 绘图窗口，红蓝两条曲线随着单片机数据的到达平滑向前滚动推进！

---

## 6. 初学者高频排障笔记 (Troubleshooting)

### 踩坑 1：Python 报 `PermissionError: [Errno 13] Access is denied`
* **根因**：Windows 下串口具有**独占排他性**。如果你的电脑上同时开着“串口调试助手”、“伏特加”、“VOFA+”或者 Keil 的串口终端，Python 就无法再次打开这个串口。
* **解决**：关闭所有正在占用该 COM 口的第三方调试软件，然后重新运行 Python 脚本。

### 踩坑 2：上位机有打印，但全是一串黄色 `[警告] 校验和错误`
* **根因**：
  * 最常见原因：**波特率不匹配**！单片机代码配的是 115200，上位机误设成了 9600。
  * 其次原因：单片机与 CH340 **没有共地**，地电平漂移导致信号电平判定错乱。
* **解决**：检查接线，确保 CH340 的 GND 与单片机的 GND 牢固相连。

### 踩坑 3：上位机打开后没有任何反应，一直没有数据打印
* **根因**：TX 和 RX 接反了！
* **解决**：对调 PA9 与 PA10 上的杜邦线（PA9 是 TX 必须插在模块的 RX；PA10 是 RX 必须插在模块的 TX）。

### 踩坑 4：设备管理器中出现带黄色感叹号的 `USB2.0-Serial`
* **根因**：电脑缺少 CH340 驱动程序。
* **解决**：下载并安装“CH341SER.EXE”驱动程序，重新插拔模块即可识别出正确的 COM 口。
