# 基于 STM32F103 的嵌入式多任务智能环境监测系统 (v3.0 FreeRTOS 终极版)

[![Release](https://img.shields.io/badge/Release-v3.0-blue.svg)](https://github.com/jvjjjj/Environment_Monitor/releases/tag/v3.0)
[![MCU](https://img.shields.io/badge/MCU-STM32F103C8T6-green.svg)](https://www.st.com)
[![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20v10.3-orange.svg)](https://www.freertos.org)
[![Sensors](https://img.shields.io/badge/Sensors-AHT20%20%2B%20BMP280-blueviolet.svg)]()
[![Compiler](https://img.shields.io/badge/Compiler-ARMCLANG%20(AC6)-purple.svg)](https://developer.arm.com)

> 💡 **项目演进导读**：本项目是一套从零构建的嵌入式环境监测终端，分为三个递进演进阶段：
> * **v1.0**：单片机裸机驱动与总线复用（软件模拟 I2C、OLED 1KB 显存双缓冲、AHT20 采集，见分支 [`v1.0`](https://github.com/jvjjjj/Environment_Monitor/tree/v1.0) / [`v1`](https://github.com/jvjjjj/Environment_Monitor/tree/v1)）。
> * **v2.0**：通信协议设计与上位机交互系统（定长二进制协议帧、USART1 校验传输、Python 动态波形监控，见分支 [`v2.0`](https://github.com/jvjjjj/Environment_Monitor/tree/v2.0)）。
> * **v3.0（当前版本 / 主分支）**：**实时操作系统重构与高精传感器融合**。彻底告别裸机阻塞轮询，移植 **FreeRTOS** 抢占式实时内核，采用生产者-消费者队列通信模型解耦三大独立任务；集成 **BMP280** 大气压强与高精海拔测算；实现硬件时基解耦、I2C 双物理地址（`0x76`/`0x77`）自适应探测与掉线自愈。

---

## 目录 (Table of Contents)

1. [项目目标与核心解决的问题](#1-项目目标与核心解决的问题)
2. [【核心必读】RTOS 实时操作系统与高精传感专业名词通俗大白话词典](#2-核心必读rtos-实时操作系统与高精传感专业名词通俗大白话词典)
   * [2.1 为什么裸机代码必须被重构？裸机系统的两大致命痛点](#21-为什么裸机代码必须被重构裸机系统的两大致命痛点)
   * [2.2 什么是 RTOS (实时操作系统)？](#22-什么是-rtos-实时操作系统)
   * [2.3 任务 (Task)、任务栈 (Stack) 与栈深单位 (Words vs Bytes)](#23-任务-task任务栈-stack-与栈深单位-words-vs-bytes)
   * [2.4 抢占式调度 (Preemptive) 与上下文切换 (Context Switch)](#24-抢占式调度-preemptive-与上下文切换-context-switch)
   * [2.5 系统节拍 (SysTick) 与独立硬件时基 (TIM4) 解耦机制](#25-系统节拍-systick-与独立硬件时基-tim4-解耦机制)
   * [2.6 跨任务通信：全局变量数据撕裂 vs 消息队列 (Queue)](#26-跨任务通信全局变量数据撕裂-vs-消息队列-queue)
   * [2.7 阻塞挂起唤醒机制与 portMAX_DELAY (0% CPU 占用)](#27-阻塞挂起唤醒机制与-portmax_delay-0-cpu-占用)
   * [2.8 生产者-消费者并发模型 (Producer-Consumer)](#28-生产者-消费者并发模型-producer-consumer)
   * [2.9 BMP280 气压传感器：MEMS 压阻、过采样、IIR 滤波与出厂标定](#29-bmp280-气压传感器mems-压阻过采样iir-滤波与出厂标定)
   * [2.10 海平面气压与气压测高公式](#210-海平面气压与气压测高公式)
3. [硬件清单与多外设并联系统接线拓扑](#3-硬件清单与多外设并联系统接线拓扑)
4. [FreeRTOS 软件并发架构与任务拓扑图](#4-freertos-软件并发架构与任务拓扑图)
5. [核心源码深度剖析：代码是怎么写的？用意何在？](#5-核心源码深度剖析代码是怎么写的用意何在)
   * [5.1 编译器适配：FreeRTOS portable 层切换至 GCC/ARM_CM3](#51-编译器适配freertos-portable-层切换至-gccarm_cm3)
   * [5.2 时基解耦：`stm32f1xx_hal_timebase_tim.c` 硬件时钟配置](#52-时基解耦stm32f1xx_hal_timebase_timc-硬件时钟配置)
   * [5.3 任务实现：`app_tasks.c` 三大并发任务与队列流转](#53-任务实现app_tasksc-三大并发任务与队列流转)
   * [5.4 传感器驱动：`bmp280.c` 双地址探测、标定补偿与海拔计算](#54-传感器驱动bmp280c-双地址探测标定补偿与海拔计算)
6. [快速上手：编译构建、固件烧录与运行验证](#6-快速上手编译构建固件烧录与运行验证)
7. [初学者高频排障笔记 (Troubleshooting)](#7-初学者高频排障笔记-troubleshooting)

---

## 1. 项目目标与核心解决的问题

在 v1.0 与 v2.0 中，我们的系统运行在“裸机”架构下。主函数依靠一个大 `while(1)` 死循环按顺序调用：
```c
while (1) {
    AHT20_ReadData(&temp, &humi); // 内部 HAL_Delay(80) 堵死 CPU 整整 80 毫秒！
    OLED_Update();                // I2C 慢速刷新屏幕，占满总线好几十毫秒！
    Protocol_SendEnvData();       // 串口发送...
    HAL_Delay(1000);              // 又硬生生睡死 1 秒！
}
```
这种代码在高校实验室或者简单 Demo 中很常见，但在工业级产品中是绝对不能接受的。

**v3.0 要实现的核心技术跃升**：
1. **多任务异步解耦**：引入工业级开源实时操作系统 **FreeRTOS**，将臃肿的主循环拆分为 3 个独立的抢占式任务（传感器采集、屏幕渲染、串口上传）。
2. **零 CPU 浪费与高实时性**：当没有数据或处于延时等待时，任务主动进入阻塞冬眠，将 CPU 算力 100% 释放出来调度其他任务。
3. **数据安全传输**：摒弃有竞争风险的全局变量，全面使用 FreeRTOS **消息队列 (`QueueHandle_t`)** 进行跨任务线程安全通信。
4. **环境全参数融合**：新增 **BMP280** 大气压强与高精海拔传感器，解决硬件 SDO 引脚电平陷阱，实现双地址自适应动态探测与传感器离线自愈。
5. **硬件时基解耦**：解决 FreeRTOS 调度器与 STM32 HAL 库抢占 `SysTick` 的死锁冲突，构建高可靠时钟树。

---

## 2. 【核心必读】RTOS 实时操作系统与高精传感专业名词通俗大白话词典

### 2.1 为什么裸机代码必须被重构？裸机系统的两大致命痛点
* **痛点 1：实时性为零（容易丢包与失聪）**
  * 如果在单片机执行 `HAL_Delay(80)` 或者 `HAL_Delay(1000)` 期间，PC 上位机发来一条紧急关机指令，或者外部紧急报警按键被按下，单片机因为正在“死等睡眠”而完全无法响应，极易造成严重丢包甚至安全事故！
* **痛点 2：CPU 资源极度浪费**
  * 在 72MHz 主频下，CPU 每秒钟可以执行 72,000,000 条机器指令。而裸机代码中有超过 90% 的时间都在执行毫无意义的 `while` 空转等待延时，白白消耗大量电量。

---

### 2.2 什么是 RTOS (实时操作系统)？
* **RTOS (Real-Time Operating System)**：专为对时间响应要求严苛的嵌入式微控制器设计的微型操作系统。
* **为什么叫“实时”？**
  * Windows、Android 这类通用操作系统更看重“整体吞吐量”，有时候鼠标卡顿半秒我们感觉不出来；
  * 而在工业控制、汽车刹车或无人机姿态控制中，如果传感器数据晚了 10 毫秒处理，飞机可能就会坠毁。RTOS 的核心承诺是：**在确定的、可预测的时间限制内，立即响应并执行任务**！

---

### 2.3 任务 (Task)、任务栈 (Stack) 与栈深单位 (Words vs Bytes)
* **任务 (Task / Thread)**：
  * 在 RTOS 中，每个功能模块都是一个独立的无限死循环函数：
    ```c
    void Task_Demo(void *pvParameters) {
        for (;;) {
            // 做业务...
            vTaskDelay(pdMS_TO_TICKS(100)); // 礼貌休眠，把 CPU 让给别人
        }
    }
    ```
* **任务私有栈 (Task Stack)**：
  * 每个任务在创建时，操作系统都会从 RAM 内存中为它划出一块专属的私人抽屉（栈）。
  * 任务内部定义的局部变量、函数调用的返回地址、任务被中断打断时 CPU 的现场寄存器，全部保存在这个私人栈里。任务之间互不干扰。
* **致命细节：栈深单位是字 (Words)，不是字节 (Bytes)！**
  * 在 Cortex-M3（32位芯片）上，**1 个 Word（字）= 4 个 Bytes（字节）**！
  * 如果你调用 `xTaskCreate(..., 128, ...)`，你分配的栈内存大小实际是 $128 \times 4 = 512$ 字节！如果初学者误以为是 128 字节而盲目填入 32，任务一运行就会瞬间**栈溢出 (Stack Overflow)** 导致单片机崩溃死机。

---

### 2.4 抢占式调度 (Preemptive) 与上下文切换 (Context Switch)
* **抢占式调度 (Preemptive Scheduling)**：
  * 操作系统根据我们为每个任务指定的**优先级 (Priority)** 来决定谁用 CPU。
  * **法则**：只要高优先级的任务就绪了（比如数据到达被唤醒），调度器会**立刻强行剥夺当前低优先级任务的 CPU 使用权**，打断它并立即运行高优先级任务！
* **上下文切换 (Context Switch)**：
  * 当任务 A 正在跑，突然要切换到任务 B 时：
    1. CPU 会在几个时钟周期内，把当前正在使用的寄存器（R0~R15、PC 计数器、xPSR 状态寄存器等）一口气打包压入任务 A 的私有栈顶保存（保存现场）；
    2. 把 CPU 的栈指针寄存器（SP）切换指向任务 B 的栈顶；
    3. 把任务 B 上次保存的寄存器弹回到 CPU 中（恢复现场）；
    4. CPU 就像什么都没发生过一样，从任务 B 上次被打断的地方接着往下跑！
  * 整个上下文切换过程在微秒级完成，快到人眼根本无法察觉。

---

### 2.5 系统节拍 (SysTick) 与独立硬件时基 (TIM4) 解耦机制
* **SysTick (滴答定时器)**：ARM Cortex-M 内核自带的一个 24 位向下计数定时器。它是 FreeRTOS 的“脉搏”，通常配置为每 $1\text{ms}$ 产生一次中断（Tick）。每次中断到来，调度器就会清点各个任务的延时倒计时、轮换时间片。
* **致命硬件冲突：为什么必须引入 TIM4？（面试核心亮点）**
  * STM32 的 HAL 官方库内部也自带一套延时体系（`HAL_Delay`），它在默认 CubeMX 配置下也是依靠 `SysTick` 维持 1ms 计数的。
  * 当移植了 FreeRTOS 后，FreeRTOS 将 `SysTick` 的中断优先级设置得非常低（为了不影响其他硬件中断）。如果某个硬件中断服务程序中调用了依赖 `SysTick` 的 `HAL_Delay`，就会产生**相互等待死锁**，单片机瞬间卡死！
  * **工业级解法（时基解耦）**：我们重写了时钟驱动，将 HAL 库的时钟基准改用片上通用的硬件定时器 **TIM4**（开启 1ms 硬件更新中断专门喂饱 HAL 库），把 `SysTick` 完完整整、干干净净地留给 FreeRTOS 调度器，彻底根除了死锁隐患！

---

### 2.6 跨任务通信：全局变量数据撕裂 vs 消息队列 (Queue)
在裸机中，我们习惯定义一个全局变量 `float global_temp`，大家随便读写。但在多任务并发系统中，这是极度危险的禁忌！

* **数据撕裂（Data Tearing）生活化大比喻**：
  * 假设温度是 `float`（占 4 个字节）。任务 A 正在向 `global_temp` 写入新数据，刚写完前 2 个字节，突然时间片到了，高优先级的任务 B 强行抢占 CPU 并读取了 `global_temp`。
  * 任务 B 读出的这个数字，**前 2 字节是新数据，后 2 字节是旧数据**！这种拼装出来的怪胎浮点数会导致数值瞬间突变崩溃！
* **FreeRTOS 消息队列 (`QueueHandle_t`)**：
  * 相当于操作系统在内存中搭建的带安全防线的高速邮筒。
  * **先进先出 (FIFO)**：数据包按放入顺序依次被取走。
  * **原子操作保障**：队列在放入和取出时，操作系统会自动关闭中断（进入临界区保护），保证整块结构体数据的拷贝绝对不可被打断，彻底杜绝数据撕裂。

---

### 2.7 阻塞挂起唤醒机制与 portMAX_DELAY (0% CPU 占用)
* 在裸机中，等待数据只能靠 `while (!data_ready);`，CPU 占有率持续 100%。
* **RTOS 的阻塞休眠**：
  * 当任务调用 `xQueueReceive(queue, &data, portMAX_DELAY)` 时，如果邮筒里没有信件，任务会立刻进入**阻塞挂起状态 (Blocked)**，主动交出 CPU 睡觉去。此时该任务占用的 CPU 算力是绝对的 **0%**！
  * 另一个任务一旦把信件投入该邮筒，操作系统调度器会瞬间把休眠的任务唤醒推入就绪列表，实现**零延迟事件驱动**。

---

### 2.8 生产者-消费者并发模型 (Producer-Consumer)
本项目构建了嵌入式领域最经典的并发解耦设计模式：
* **生产者 (Producer)**：`Task_Sensor`
  * 它的唯一职责就是周期性与 AHT20、BMP280 进行物理硬件通信，测量数据，打包成结构体，往队列里投递。
* **消费者 1 (Consumer 1)**：`Task_OLED`
  * 专门负责从队列取出数据，排版并刷新屏幕显示。
* **消费者 2 (Consumer 2)**：`Task_Comm`
  * 专门负责从队列取出数据，组装 9 字节协议帧推向串口送给 PC。
* **解耦优势**：如果哪天 OLED 屏通信变慢了，完全不会拖慢串口发送；如果串口堵住了，也丝毫不影响屏幕刷新与传感器采样的节奏。

---

### 2.9 BMP280 气压传感器：MEMS 压阻、过采样、IIR 滤波与出厂标定
* **MEMS 压阻式感压膜片**：芯片内部有一块微米级的超薄硅膜，外界大气压强挤压膜片发生微小形变，导致硅材料电阻率改变，进而输出微伏级电压变化。
* **过采样 (Oversampling)**：内部模数转换器对外界物理量连续采样 16 次求平均值，大幅降低物理热噪声。
* **IIR 滤波器 (Infinite Impulse Response)**：数字低通滤波算法。在室内开门、关窗或一阵风吹过时，会产生瞬态气压波动。开启 IIR 滤波（16x）可以平滑这些尖刺毛刺，使气压读数稳定平滑。
* **出厂激光标定参数 (Factory Calibration)**：
  * 因为微观制造公差，世界上没有两片一模一样的 MEMS 芯片。
  * 博世在出厂测试时，将每颗芯片的独有补偿多项式系数（24 个字节，`dig_T1 ~ dig_P9`）烧录在芯片内部的 OTP 寄存器中。
  * 驱动必须先读取出这 24 字节系数，代入博世官方提供的 **64 位整数多项式算法** 中，才能还原出真实帕斯卡（Pa）气压值。
* **从机物理地址陷阱 (0x76 vs 0x77)**：
  * BMP280 的地址由其 `SDO` 引脚的硬件电平决定：接地为 `0x76`，接高电平为 `0x77`。
  * 市售的二合一模块为了避免总线冲突，板载自带了 10k 上拉电阻将 SDO 拉高到了 3.3V（即真实地址为 **`0x77`**！）。如果驱动盲目写死 `0x76`，就会引发 NACK 离线级联雪崩！

---

### 2.10 海平面气压与气压测高公式
根据国际民航组织 (ICAO) 标准大气模型，海拔高度每上升 100 米，大气压强约下降 12 hPa。基于压高公式：
$$\text{Altitude (m)} = 44330 \times \left(1 - \left(\frac{P}{P_0}\right)^{0.190295}\right)$$
* 其中 $P_0 = 101325 \ \text{Pa}$（海平面标准大气压）；
* $P$ 为当前传感器测出的实际环境大气压强（单位：Pa）。通过这个数学公式，系统即可实时估算出当地的相对海拔高度。

---

## 3. 硬件清单与多外设并联系统接线拓扑

| 器件名称 | 规格参数 | 数量 | 作用说明 |
| :--- | :--- | :---: | :--- |
| **STM32F103C8T6** | ARM Cortex-M3, 72MHz, 64KB Flash, 20KB RAM | 1 | 系统运行 FreeRTOS 内核的主控芯片 |
| **0.96 寸 OLED 屏** | SSD1306 驱动，128×64 分辨率，I2C 接口 | 1 | 本地实时显示四行环境全集数据 |
| **AHT20+BMP280 模块**| 标准 I2C 总线并联模块 | 1 | 复合测量温湿度、气压与海拔高度 |
| **CH340 USB-TTL** | USB 转串口转接板 | 1 | 负责将协议数据帧上传电脑上位机 |
| **ST-Link V2** | SWD 调试下载器 | 1 | 固件烧录与 FreeRTOS 任务调试 |

```text
                                STM32F103C8T6
                   ┌──────────────────────────────────────┐
                   │                                      │
                   │  PB6(SCL)  PB7(SDA)  PA9(TX)  PA10(RX│
                   └───┬──────────┬─────────┬────────┬────┘
                       │          │         │        │
      I2C 物理并联总线 │          │         │        │  USART1
    ┌──────────────────┴──┐       │         ▼        ▼  异步串口
    │                     │       │        RX        TX
    ▼                     ▼       │     ┌─────────────────┐
┌─────────────┐       ┌───────────┴─┐   │   CH340 模块    │
│   SSD1306   │       │ AHT20+BMP280│   │ (USB-TTL 转接)  │
│  OLED 显示屏│       │ 复合传感器  │   └────────┬────────┘
│ (从机: 0x3C)│       │(0x38 & 0x77)│            │ USB
└─────────────┘       └─────────────┘            ▼
                                          PC 电脑上位机程序
```

---

## 4. FreeRTOS 软件并发架构与任务拓扑图

```text
                    ┌─────────────────────────┐
                    │  硬件定时器硬件时基 TIM4 │ (提供 HAL 1ms 时钟，彻底规避 SysTick 死锁)
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

### 任务配置参数对照表

| 任务名称 | 优先级 | 栈深大小 (Words) | 实际分配内存 (Bytes) | 执行周期 / 唤醒条件 | 核心职责 |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **`Task_Sensor`** | **3 (中)** | 128 | 512 B | 1000ms 周期唤醒 | AHT20/BMP280 物理采集、气压算海拔、BMP280 掉线自愈检测、分发队列。 |
| **`Task_OLED`** | **1 (低)** | 256 | 1024 B | 事件驱动 / 500ms 超时 | 等待显存数据，格式化拼接 4 行内容（温湿度/气压/海拔/总线诊断码），I2C 整屏刷新。 |
| **`Task_Comm`** | **4 (高)** | 128 | 512 B | `portMAX_DELAY` 无限阻塞 | 零 CPU 占用休眠。一旦传感器产生新帧立刻被光速唤醒，打包推向串口。 |

---

## 5. 核心源码深度剖析：代码是怎么写的？用意何在？

### 5.1 编译器适配：FreeRTOS portable 层切换至 GCC/ARM_CM3
* **痛点**：Keil 高版本全面废弃 AC5，强制采用基于 Clang 内核的 **ARM Compiler 6 (AC6)**。如果使用老旧的 RVDS 移植层，`portmacro.h` 中的 `__forceinline` 以及 `port.c` 中的 `__asm` 汇编内联语法会导致 AC6 抛出多达数十项编译错误。
* **工程解法**：在工程文件结构中，将 portable 移植层定向到 `portable/GCC/ARM_CM3`。GCC 移植层采用符合标准 C99 和 GNU 内联汇编语法的纯正实现，与基于 LLVM/Clang 的 ARMCLANG 编译器具有完美的血缘兼容性，实现 **0 Error, 0 Warning** 构建。

---

### 5.2 时基解耦：`stm32f1xx_hal_timebase_tim.c` 硬件时钟配置

打开 [`Core/Src/stm32f1xx_hal_timebase_tim.c`](Core/Src/stm32f1xx_hal_timebase_tim.c)：
```c
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority) {
    RCC_ClkInitTypeDef clkconfig;
    uint32_t uwTimclock, uwPrescalerValue;
    uint32_t pFLatency;

    // 1. 使能 TIM4 外设时钟
    __HAL_RCC_TIM4_CLK_ENABLE();

    // 2. 计算预分频系数，使计数频率精准对齐 1MHz (每计数1次耗时 1微秒)
    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);
    uwTimclock = HAL_RCC_GetPCLK1Freq();
    uwPrescalerValue = (uint32_t)((uwTimclock / 1000000U) - 1U);

    // 3. 配置定时周期为 1000 微秒 (即 1ms 中断一次)
    htim4.Instance = TIM4;
    htim4.Init.Period = (1000000U / 1000U) - 1U;
    htim4.Init.Prescaler = uwPrescalerValue;
    htim4.Init.ClockDivision = 0;
    htim4.Init.CounterMode = TIM_COUNTERMODE_UP;

    HAL_TIM_Base_Init(&htim4);
    HAL_TIM_Base_Start_IT(&htim4); // 开启定时器中断

    // 4. 配置中断优先级为最高 (0)
    HAL_NVIC_SetPriority(TIM4_IRQn, TickPriority, 0U);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);

    return HAL_OK;
}

// TIM4 中断服务回调函数
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM4) {
        HAL_IncTick(); // 专门给 HAL 库喂时钟节拍！
    }
}
```
* **为什么这样写？用意何在？**
  * 我们重写了弱定义的 `HAL_InitTick` 函数，用芯片内部的硬件定时器 **TIM4** 替代了 SysTick。
  * 每当 TIM4 走过 1ms，硬件中断触发 `HAL_IncTick()`，HAL 库的 `HAL_GetTick()` 和延时体系平稳工作；
  * 而系统的 `SysTick` 硬件中断则被完全解放，专供 FreeRTOS 内核作为任务调度的节拍发生器，两者井水不犯河水，从根源上杜绝了调度死锁。

---

### 5.3 任务实现：`app_tasks.c` 三大并发任务与队列流转

打开 [`Core/Src/app_tasks.c`](Core/Src/app_tasks.c)：

#### 1. 生产者任务：`Task_Sensor`
```c
static void Task_Sensor(void *pvParameters) {
    EnvData_t sensor_env;
    memset(&sensor_env, 0, sizeof(EnvData_t)); // 栈内存清零，防野值

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); // 1000ms 绝对周期

    AHT20_Init();
    BMP280_Init();

    for (;;) {
        // 1. 采集 AHT20 温湿度
        AHT20_ReadData(&sensor_env.temperature, &sensor_env.humidity);

        // 2. BMP280 离线自愈检测
        if (!BMP280_IsReady()) {
            BMP280_Init(); // 若之前掉线或没通电，后台周期性自愈重试
        }

        // 3. 采集 BMP280 气压与海拔
        float dummy_temp = 0.0f, raw_press_pa = 0.0f;
        if (BMP280_ReadData(&dummy_temp, &raw_press_pa) == 0) {
            sensor_env.pressure = raw_press_pa / 100.0f; // 换算为 hPa
            sensor_env.altitude = BMP280_CalcAltitude(raw_press_pa); // 计算海拔
        }

        sensor_env.timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // 4. 广播推入两个独立队列 (覆盖旧值，保证队列中永远是最新的)
        xQueueOverwrite(q_oled_data, &sensor_env);
        xQueueOverwrite(q_comm_data, &sensor_env);

        // 5. 绝对精确周期延时
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
```
* **为什么用 `vTaskDelayUntil` 而不是普通 `vTaskDelay`？**
  * 普通的 `vTaskDelay(1000)` 表示“当前任务执行完后，再睡 1000ms”。但任务本身的执行代码也是要消耗几毫秒时间的，日积月累会导致测量周期的严重漂移；
  * `vTaskDelayUntil` 会自动把任务本身的执行耗时扣除，确保不管任务执行了多久，每两次采集之间的**绝对时间间隔严格恒等于 1000ms**！
* **为什么用 `xQueueOverwrite`？**
  * 我们的环境监测只需要最新的一帧数据。如果屏幕或者串口稍微耽搁了一下，覆盖写入能保证消费者拿到的永远是最鲜活的实时物理量，避免老旧数据在队列里积压滞后。

#### 2. OLED 屏幕渲染任务：`Task_OLED`
```c
static void Task_OLED(void *pvParameters) {
    EnvData_t oled_env;
    char str_buf[20];
    OLED_Init();

    for (;;) {
        // 挂起等待新数据，最多等待 500ms
        if (xQueueReceive(q_oled_data, &oled_env, pdMS_TO_TICKS(500)) == pdPASS) {
            OLED_Clear();
            
            // 第 0 页: 温湿度紧凑格式
            sprintf(str_buf, "T:%.1fC H:%.1f%%", oled_env.temperature, oled_env.humidity);
            OLED_ShowString(0, 0, str_buf, 16);

            // 第 2 页: 大气压强 (hPa)
            sprintf(str_buf, "P: %.1fhPa", oled_env.pressure);
            OLED_ShowString(0, 2, str_buf, 16);

            // 第 4 页: 估算海拔高度 (m)
            sprintf(str_buf, "Alt: %.1fm", oled_env.altitude);
            OLED_ShowString(0, 4, str_buf, 16);

            // 第 6 页: 动态自诊断硬件状态码
            if (BMP280_IsReady()) {
                sprintf(str_buf, "RTOS OK [%02X:%02X]", BMP280_GetAddr(), BMP280_GetChipID());
            } else {
                sprintf(str_buf, "BMP Err! ID:%02X", BMP280_GetChipID());
            }
            OLED_ShowString(0, 6, str_buf, 16);

            OLED_Update(); // 毫秒级整屏推送
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```
* **第 6 行动态诊断码设计用意**：
  * 传统代码即使传感器坏了也盲目显示 `OK`。我们改为了动态自诊断指示：
  * 若探测到物理地址 `0x77`、芯片 ID 为 `0x58`，屏幕第 6 行显示：`RTOS OK [77:58]`，硬件在线一目了然；
  * 若杜邦线松了，显示 `BMP Err! ID:FF`，极大降低现场排障难度。

#### 3. 串口通信任务：`Task_Comm`
```c
static void Task_Comm(void *pvParameters) {
    EnvData_t comm_env;
    for (;;) {
        // 无限期阻塞等待队列！没数据时绝对 0% 占用 CPU
        if (xQueueReceive(q_comm_data, &comm_env, portMAX_DELAY) == pdPASS) {
            Protocol_SendEnvData(comm_env.temperature, comm_env.humidity);
        }
    }
}
```
* **用意**：优先级设为最高的 4。一旦传感器采到了数据，操作系统光速唤醒它，优先发出串口数据帧，发完立刻再度冬眠，实时性达到极致。

---

### 5.4 传感器驱动：`bmp280.c` 双地址探测、标定补偿与海拔计算

打开 [`Core/Src/bmp280.c`](Core/Src/bmp280.c)：

#### 1. 双物理地址自适应探测
```c
uint8_t BMP280_Init(void) {
    s_bmp280_ready = 0;
    // 候选地址列表: 0x76 (SDO接GND), 0x77 (SDO接VCC)
    const uint8_t addr_list[2] = {0x76, 0x77};
    uint8_t found = 0, id = 0;

    for (uint8_t i = 0; i < 2; i++) {
        if (BMP280_ReadRegAddr(addr_list[i], BMP280_CHIP_ID_REG, &id) == 0) {
            s_bmp280_chip_id = id;
            // 兼容 BMP280(0x58), BME280(0x60), 及常见兼容替代料(0x56, 0x57)
            if (id == 0x58 || id == 0x60 || id == 0x56 || id == 0x57) {
                s_bmp280_addr = addr_list[i]; // 动态锁定有效地址！
                found = 1;
                break;
            }
        }
    }
    if (!found) return 1; // 两个地址均未响应
```
* **为什么这样写？用意何在？**
  * 市场上二合一模块的 SDO 引脚有的接地（`0x76`），有的上拉（`0x77`）。写死单一地址会经常出现“买来模块读不到”的尴尬。双地址自动轮询彻底消除了硬件跳线差异。

#### 2. 64 位整数出厂标定精密气压多项式补偿
```c
uint8_t BMP280_ReadData(float *temperature, float *pressure) {
    // ... 读取原始 20位 ADC 数据 adc_P 和 adc_T ...

    // 步骤 1: 必须先计算精密温度补偿中间变量 t_fine
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)calib.dig_T1 << 1))) * ((int32_t)calib.dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)calib.dig_T1))) >> 12) * ((int32_t)calib.dig_T3)) >> 14;
    t_fine = var1 + var2; // 关键：气压计算严重依赖 t_fine！

    // 步骤 2: 64位整型多项式补偿 (直接引用博世手册算法)
    int64_t p_var1 = ((int64_t)t_fine) - 128000;
    int64_t p_var2 = p_var1 * p_var1 * (int64_t)calib.dig_P6;
    p_var2 = p_var2 + ((p_var1 * (int64_t)calib.dig_P5) << 17);
    p_var2 = p_var2 + (((int64_t)calib.dig_P4) << 35);
    p_var1 = ((p_var1 * p_var1 * (int64_t)calib.dig_P3) >> 8) + ((p_var1 * (int64_t)calib.dig_P2) << 12);
    p_var1 = (((((int64_t)1) << 47) + p_var1)) * ((int64_t)calib.dig_P1) >> 33;

    if (p_var1 == 0) {
        *pressure = 0.0f; // 避免除零异常
        return 2;
    }

    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - p_var2) * 3125) / p_var1;
    p_var1 = (((int64_t)calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    p_var2 = (((int64_t)calib.dig_P8) * p) >> 19;
    p = ((p + p_var1 + p_var2) >> 8) + (((int64_t)calib.dig_P7) << 4);

    *pressure = (float)p / 256.0f; // 最终得到标准国际单位 Pa (帕斯卡)
    return 0;
}
```
* **用意何在？**
  * 压阻式感压膜片的电阻受温度影响极大。因此博世官方算法强制要求**先算温度得到 `t_fine`**，再将 `t_fine` 注入气压公式中消除温漂影响。64 位整型多项式避免了浮点运算的累计误差，精度可达极其惊人的 $\pm 0.12 \ \text{hPa}$（相当于仅 1 米的高度分辨率）。

---

## 6. 快速上手：编译构建、固件烧录与运行验证

### 6.1 编译与烧录
1. 使用 Keil 打开工程：`MDK-ARM/Environment_Monitor.uvprojx`。
2. 键盘按下 **F7**（Build），确认编译结果为 **`0 Error(s), 0 Warning(s)`**。
3. 连接 ST-Link 仿真器，按下 **F8**（Download）一键烧录。

### 6.2 预期运行现象
* **单片机端 (OLED)**：屏幕清晰显示 4 行信息：
  ```text
  ┌──────────────────┐
  │T:25.4C H:58.2%   │  <- 第 0 行：温湿度全集
  │P: 1013.2hPa      │  <- 第 2 行：大气压强
  │Alt: 45.3m        │  <- 第 4 行：当地估算海拔
  │RTOS OK [77:58]   │  <- 第 6 行：FreeRTOS总线与BMP280在线诊断
  └──────────────────┘
  ```
* **PC 端 (Python 上位机)**：
  在电脑上运行 `python monitor_host.py`，依然能顺畅接收到 9 字节协议帧，绘制出实时的动态温湿度变化波形！

---

## 7. 初学者高频排障笔记 (Troubleshooting)

### 踩坑 1：单片机跑一会儿就卡死在 `HardFault_Handler`
* **根因**：**任务栈溢出 (Stack Overflow)**！通常是因为在某个任务函数内部定义了巨大的局部数组（如 `char buffer[1024]`），直接把任务私有的几百字节栈空间打穿了。
* **解法**：加大 `xTaskCreate` 里的栈深参数，或在 `FreeRTOSConfig.h` 中开启栈溢出钩子检测：`#define configCHECK_FOR_STACK_OVERFLOW 2`。

### 踩坑 2：屏幕上的气压和海拔始终显示为 `0.0`
* **根因**：BMP280 初始化失败。由于模块 SDO 上拉，物理地址是 `0x77`，若代码写死 `0x76`，出厂标定参数全为 0。多项式计算时 `calib.dig_P1 == 0` 导致 `p_var1 == 0`，触发除零保护退出，结构体从未被赋值。
* **解法**：使用本工程实现的自动双地址扫描与掉线自愈驱动即可彻底根治。

### 踩坑 3：烧录后程序完全不运行，卡在启动阶段
* **根因**：没有为 HAL 库配置独立硬件定时器，HAL 库的 `HAL_Delay` 正在等待尚未启动的 FreeRTOS `SysTick` 节拍，导致永久死锁。
* **解法**：检查 `stm32f1xx_hal_timebase_tim.c`，确保启用了硬件定时器 TIM4 作为独立时钟源。
