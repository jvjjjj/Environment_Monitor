# 基于 STM32F103 的嵌入式多任务智能环境监测系统

[![Release](https://img.shields.io/badge/Release-v3.0-blue.svg)](https://github.com/jvjjjj/Environment_Monitor/releases)
[![MCU](https://img.shields.io/badge/MCU-STM32F103C8T6-green.svg)](https://www.st.com)
[![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.3-orange.svg)](https://www.freertos.org)
[![Compiler](https://img.shields.io/badge/Compiler-ARMCLANG%20(AC6)-purple.svg)](https://developer.arm.com)

一套基于 **STM32F103C8T6** 微控制器的工业级环境监测终端与软硬协同系统。从底层单总线多从机 I2C 驱动、自定义抗干扰二进制串口通信协议，到移植 **FreeRTOS** 实时内核实现多任务解耦与传感器离线自愈，并配套开发 Python (Matplotlib + PySerial) 动态上位机进行实时数据流可视化。

---

## 📌 版本演进与分支归档 (Releases & Branches)

| 版本 Tag | 对应分支 | 核心架构与功能特性 |
| :--- | :--- | :--- |
| **`v1.0`** | - | **裸机基础版**：软件模拟 I2C（Timeout 防死锁机制），单总线挂载 SSD1306 OLED (1KB 双缓冲显存) 与 AHT20 温湿度传感器。 |
| **`v2.0`** | [`v2.0`](https://github.com/jvjjjj/Environment_Monitor/tree/v2.0) | **通信与上位机**：设计定长二进制帧协议（帧头校验和防乱码），USART1 高速上传，配套开发 Python 多线程上位机动态监控曲线。 |
| **`v3.0`** | [`v3.0`](https://github.com/jvjjjj/Environment_Monitor/tree/v3.0) / [`master`](https://github.com/jvjjjj/Environment_Monitor) | **实时操作系统重构**：移植 FreeRTOS，任务解耦（生产者-消费者模型与 Queue 消息队列），集成 BMP280 气压/相对海拔融合，支持 I2C 双物理地址自动探测（0x76/0x77）与传感器离线自愈。 |

---

## 🛠️ 硬件清单 (BOM)

* **主控芯片**：STM32F103C8T6 (ARM Cortex-M3 内核, 72MHz 主频, 64KB Flash, 20KB SRAM)
* **显示外设**：0.96 寸 4-Pin I2C OLED 屏幕 (SSD1306, 128x64 分辨率)
* **复合环境传感器**：AHT20 + BMP280 二合一模块
  * **AHT20**：高精度相对温湿度测量（I2C 地址 `0x38`）
  * **BMP280**：工业级大气压强与海拔高度测量（自适应探测 I2C 地址 `0x76` / `0x77`）
* **通信与调试**：
  * ST-Link V2 (SWD 硬件调试下载)
  * CH340 USB-TTL 串口模块 (波特率 115200bps, N-8-1)
* **实验平台**：830 孔双电源轨面包板 + 杜邦线

---

## 🏗️ FreeRTOS 软件架构与多任务设计 (v3.0)

本项目彻底摒弃了裸机 `while(1)` 线性阻塞轮询模式，重构为典型的**基于 FreeRTOS 队列通信的生产者-消费者异步并发架构**：

```
                    ┌─────────────────────────┐
                    │  硬件定时器硬件时基 TIM4 │ (提供 HAL 1ms 时钟，规避 SysTick 抢占)
                    └───────────┬─────────────┘
                                │
   ┌────────────────────────────┼────────────────────────────┐
   │                            │                            │
┌──▼────────────────┐   ┌───────▼───────────┐   ┌────────────▼──────────┐
│ Task_Sensor (PRI 3)│   │  Task_OLED (PRI 1)│   │   Task_Comm (PRI 4)   │
│ 传感器采集(生产者)  │   │  屏幕渲染(消费者) │   │   串口封包(消费者)    │
└──┬────────────────┘   └───────▲───────────┘   └────────────▲──────────┘
   │                            │                            │
   │  xQueueOverwrite           │ xQueueReceive(500ms)       │ xQueueReceive(Block)
   ├────────────────────────────┼────────────────────────────┤
   │                     [ q_oled_data ] 队列                │
   │                                                         │
   └─────────────────────────────────────────────────────────┼───────┐
                         [ q_comm_data ] 队列                        │
                                                                     │
                                                       (USART1 二进制协议帧)
                                                                     ▼
                                                             Python 上位机程序
```

### 任务职责与优先级分配表

| 任务名称 | 优先级 | 栈深 (Word) | 运行周期 | 职责描述 |
| :--- | :---: | :---: | :---: | :--- |
| **`Task_Sensor`** | 3 (中) | 128 (512B) | 1000ms | 触发 AHT20/BMP280 物理量采集，气压公式估算海拔高度，BMP280 掉线自愈检测，将融合数据分发写入队列。 |
| **`Task_OLED`** | 1 (低) | 256 (1024B)| 事件唤醒/500ms | 挂起等待传感器队列。接收后在 1KB 显存完成 4 行文字排版（温湿度/气压/海拔/总线诊断码），I2C 整屏刷新。 |
| **`Task_Comm`** | 4 (高) | 128 (512B) | `portMAX_DELAY` | 无限挂起，零 CPU 占用。一旦产生最新有效数据立即唤醒，组装 9 字节二进制协议帧送入串口 FIFO。 |

---

## 📡 二进制通信协议定义 (v2.0 & v3.0)

针对传统 `printf` 文本传输解析开销大、容易断包粘包及抗干扰差的痛点，设计了轻量级二进制定长数据帧：

| 字节序号 | 字段名称 | 长度 (Byte) | 取值范围 / 说明 |
| :---: | :---: | :---: | :--- |
| **Byte 0** | 帧头 1 (Header 1) | 1 | 固定 `0xAA` |
| **Byte 1** | 帧头 2 (Header 2) | 1 | 固定 `0x55` |
| **Byte 2** | 温度高字节 (Temp\_H) | 1 | 温度以放大 10 倍的有符号整型传输 (`(int16_t)(Temp * 10)`) |
| **Byte 3** | 温度低字节 (Temp\_L) | 1 | 小端/大端规约传输 |
| **Byte 4** | 湿度高字节 (Humi\_H) | 1 | 湿度以放大 10 倍的无符号整型传输 (`(uint16_t)(Humi * 10)`) |
| **Byte 5** | 湿度低字节 (Humi\_L) | 1 | 分辨率 0.1%RH |
| **Byte 6** | 保留字段 (Reserved)  | 1 | 预留可扩展气压标识 |
| **Byte 7** | 校验和 (Checksum)    | 1 | Byte 2 至 Byte 6 的单字节累加和低 8 位 (`Sum & 0xFF`) |
| **Byte 8** | 帧尾 (Tail)          | 1 | 固定 `0x0D` (`\r`) |

---

## 💻 Python 上位机运行指南

位于项目根目录的 [`monitor_host.py`](monitor_host.py) 采用 PySerial 流式滑动窗口解包校验，并利用 Matplotlib 动态绘制曲线：

```bash
# 安装依赖
pip install pyserial matplotlib

# 启动上位机
python monitor_host.py
```

---

## 🔧 核心排障与关键技术点 (Interview Highlights)

1. **Keil ARM Compiler 6 (AC6 / ARMCLANG) 兼容性解决**：
   * 在使用 AC6 编译 FreeRTOS 内核时，RVDS 移植层中的 `__forceinline` 和内联汇编语法会导致报错。工程将 portable 层切换为 `portable/GCC/ARM_CM3`，完美消除兼容性错误，实现 0 Error 0 Warning 编译。
2. **HAL 与 FreeRTOS 时基解耦**：
   * FreeRTOS 调度器独占 `SysTick`（滴答定时器）。为了防止 `HAL_Delay` 与 RTOS 产生节拍死锁，配置 STM32 硬件通用定时器 **TIM4** 作为 HAL 库全局独立时基（1ms 中断）。
3. **BMP280 I2C 物理地址自适应探测 (0x76 / 0x77)**：
   * 部分复合模块 SDO 引脚通过 10k 电阻上拉至 VCC（地址为 `0x77`），部分接地（地址为 `0x76`）。驱动实现双地址自动扫描和 Chip ID 校验（兼容 BMP280 `0x58` 与 BME280 `0x60` 及替代料），并在 OLED 第 6 行提供实时自诊断码（如 `RTOS OK [77:58]`）。
