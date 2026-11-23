#include "virtual_sensor.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* 内部状态变量 */
static VirtualSensor_t s_sensor = {
    .temperature = 25.0f,
    .humidity    = 55.0f,
    .manual_mode = false,
    .temp_alarm  = false
};

static uint32_t s_tick_counter = 0;

/* 串口命令行接收缓冲 */
#define CLI_BUF_SIZE 64
static char s_rx_buf[CLI_BUF_SIZE];
static uint8_t s_rx_index = 0;

/**
  * @brief  初始化虚拟传感器
  */
void VirtualSensor_Init(void)
{
    s_sensor.temperature = 25.0f;
    s_sensor.humidity    = 55.0f;
    s_sensor.manual_mode = false;
    s_sensor.temp_alarm  = false;
    s_tick_counter = 0;
    s_rx_index = 0;
}

/**
  * @brief  周期更新虚拟环境数据（自动微变模型）
  */
void VirtualSensor_Update(void)
{
    s_tick_counter++;

    if (!s_sensor.manual_mode)
    {
        // 使用简单的三角波微变算法，避免引入浮点三角函数库开销
        // 每 60 次更新为一个周期，温度在 23.5 ~ 26.5℃ 之间平滑变动
        int step = (int)(s_tick_counter % 60);
        float offset = (step < 30) ? (step * 0.1f) : ((60 - step) * 0.1f);
        s_sensor.temperature = 23.5f + offset;

        // 湿度在 50.0% ~ 60.0% 之间波动
        s_sensor.humidity = 50.0f + offset * 3.33f;
    }

    // 报警检测：温度 > 32℃ 或 < 16℃ 时触发报警
    if (s_sensor.temperature > 32.0f || s_sensor.temperature < 16.0f)
    {
        s_sensor.temp_alarm = true;
    }
    else
    {
        s_sensor.temp_alarm = false;
    }
}

/**
  * @brief  获取当前虚拟传感器快照
  */
VirtualSensor_t VirtualSensor_Get(void)
{
    return s_sensor;
}

/**
  * @brief  手动设置温度
  */
void VirtualSensor_SetTemp(float temp)
{
    s_sensor.temperature = temp;
    s_sensor.manual_mode = true;
    printf("[CLI OK] Set Temperature = %.1f C (Manual Mode)\r\n", temp);
}

/**
  * @brief  手动设置湿度
  */
void VirtualSensor_SetHumi(float humi)
{
    s_sensor.humidity = humi;
    s_sensor.manual_mode = true;
    printf("[CLI OK] Set Humidity = %.1f %% (Manual Mode)\r\n", humi);
}

/**
  * @brief  恢复自动波动模式
  */
void VirtualSensor_SetAutoMode(void)
{
    s_sensor.manual_mode = false;
    printf("[CLI OK] Switched to AUTO Mode (Natural drift).\r\n");
}

/* 打印指令帮助 */
static void CLI_PrintHelp(void)
{
    printf("\r\n--- Available CLI Commands ---\r\n");
    printf("  help             : Show this help message\r\n");
    printf("  read             : Print current sensor readings immediately\r\n");
    printf("  set temp <val>   : Set manual temperature, e.g., 'set temp 35.5'\r\n");
    printf("  set humi <val>   : Set manual humidity, e.g., 'set humi 80.0'\r\n");
    printf("  set auto         : Return to automatic environment drift\r\n");
    printf("------------------------------\r\n\r\n");
}

/* 执行解析到的命令行 */
static void CLI_ExecuteCommand(char *cmd)
{
    // 去除首尾空格
    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') return;

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0)
    {
        CLI_PrintHelp();
    }
    else if (strcmp(cmd, "read") == 0 || strcmp(cmd, "status") == 0)
    {
        printf("[READ] Temp: %.1f C | Hum: %.1f %% | Mode: %s | Alarm: %s\r\n",
               s_sensor.temperature,
               s_sensor.humidity,
               s_sensor.manual_mode ? "MANUAL" : "AUTO",
               s_sensor.temp_alarm ? "ALARM!" : "OK");
    }
    else if (strncmp(cmd, "set temp ", 9) == 0)
    {
        float t = (float)atof(cmd + 9);
        VirtualSensor_SetTemp(t);
    }
    else if (strncmp(cmd, "set humi ", 9) == 0)
    {
        float h = (float)atof(cmd + 9);
        VirtualSensor_SetHumi(h);
    }
    else if (strcmp(cmd, "set auto") == 0)
    {
        VirtualSensor_SetAutoMode();
    }
    else
    {
        printf("[CLI ERR] Unknown command: '%s'. Type 'help' for instructions.\r\n", cmd);
    }
}

/**
  * @brief  串口非阻塞接收并解析 CLI 指令
  * @note   在 main 函数的 while(1) 中高频调用
  */
void VirtualSensor_CLI_Process(void)
{
    // 检查串口是否有新字符到达 (非阻塞轮询)
    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET)
    {
        uint8_t ch = (uint8_t)(huart1.Instance->DR & 0x00FF);

        // 回车或换行表示一帧指令输入结束
        if (ch == '\r' || ch == '\n')
        {
            if (s_rx_index > 0)
            {
                s_rx_buf[s_rx_index] = '\0';
                printf("\r\n"); // 终端换行
                CLI_ExecuteCommand(s_rx_buf);
                s_rx_index = 0;
            }
        }
        // 处理退格键 (Backspace)
        else if (ch == '\b' || ch == 0x7F)
        {
            if (s_rx_index > 0)
            {
                s_rx_index--;
                // 终端擦除前一个字符
                printf("\b \b");
            }
        }
        else
        {
            // 存入字符并回显
            if (s_rx_index < (CLI_BUF_SIZE - 1))
            {
                s_rx_buf[s_rx_index++] = (char)ch;
                HAL_UART_Transmit(&huart1, &ch, 1, 10); // 字符实时回显
            }
        }
    }
}
