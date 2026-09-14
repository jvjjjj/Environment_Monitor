# 基于 STM32F103 的环境监测系统 (Environment Monitor)

本项目基于 **STM32F103C8T6** 微控制器设计并实现了一套低功耗、高可靠性的环境监测终端系统。系统集成了 I2C 接口的 **SSD1306 OLED 显示屏** 与 **AHT20 温湿度传感器**，支持硬件外设采集、显存级图形字符渲染，并在 **Proteus 9** 平台完成了全数字孪生闭环仿真与固件验证。

---

## 🛠️ 技术栈与软硬件平台

| 模块 | 规格 / 工具 | 说明 |
| :--- | :--- | :--- |
| **主控芯片** | STM32F103C8T6 (ARM Cortex-M3) | 主频 72MHz，Flash 64KB，SRAM 20KB |
| **显示模块** | 0.96 寸 OLED (SSD1306 驱动芯片) | 128×64 分辨率，I2C 接口通讯 |
| **传感模块** | AHT20 数字温湿度传感器 | 标称 I2C 从机地址 0x38，支持校准与 CRC |
| **开发环境** | Keil MDK-ARM (v5.38+) | 采用 **Arm Compiler 6 (armclang)** 构建 |
| **代码生成** | STM32CubeMX | HAL 固件库架构 |
| **数字孪生** | Proteus 9 Professional | 系统电气连接与软硬件协同仿真平台 |
| **版本控制** | Git | 规范分支与提交管理 |

---

## 📂 仓库目录结构

```text
Environment_Monitor/
├── Core/
│   ├── Inc/               # 头文件目录
│   │   ├── main.h         # 主应用定义
│   │   ├── oled.h         # OLED 显存驱动与 API 声明
│   │   └── oledfont.h     # 8x16 字符点阵字模库
│   └── Src/               # 源文件目录
│       ├── main.c         # 系统时钟、外设调用与业务循环
│       ├── oled.c         # SSD1306 模拟 I2C 与显存刷新实现
│       └── gpio.c         # GPIO 引脚初始化
├── Drivers/               # ST 官方 CMSIS 与 STM32F1xx HAL 驱动库
├── MDK-ARM/               # Keil 工程文件及输出目录
│   └── Environment_Monitor.uvprojx
├── Environment_Monitor.ioc# STM32CubeMX 配置文件
├── .gitignore             # 嵌入式工程专属 Git 忽略规则
└── README.md              # 项目技术白皮书