#include "debug_console.h"
#include "virtual_sensor.h"
#include "env_analyzer.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define CLI_BUFFER_SIZE 64

static char s_rx_buffer[CLI_BUFFER_SIZE];
static uint8_t s_rx_idx = 0;
static DebugConsole_Config_t s_cfg = {
    .print_menu_on_boot = true,
    .echo_enabled = true
};

/* 打印调试指令菜单 */
static void PrintHelpMenu(void)
{
    printf("\r\n=========================================\r\n");
    printf("         Virtual Sensor Debug CLI        \r\n");
    printf("=========================================\r\n");
    printf(" Commands:\r\n");
    printf("   temp <value>   : Set temperature (e.g. 'temp 35.5')\r\n");
    printf("   hum <value>    : Set humidity (e.g. 'hum 75.0')\r\n");
    printf("   auto           : Return to auto natural drift\r\n");
    printf("   read           : Read current sensor values\r\n");
    printf("   stat           : Show sliding-window statistics (MAX/MIN/AVG)\r\n");
    printf("   hist / history : Show recent samples history timeline\r\n");
    printf("   help / ?       : Show this command menu\r\n");
    printf("=========================================\r\n\r\n");
}

/**
 * @brief  初始化调试控制台模块
 */
void DebugConsole_Init(const DebugConsole_Config_t *config)
{
    if (config != NULL)
    {
        s_cfg = *config;
    }
    s_rx_idx = 0;

    if (s_cfg.print_menu_on_boot)
    {
        PrintHelpMenu();
    }
}

/* 执行解析到的具体指令 */
static void ExecuteCommand(char *cmd)
{
    // 去除开头空格
    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') return;

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0)
    {
        PrintHelpMenu();
    }
    else if (strcmp(cmd, "read") == 0 || strcmp(cmd, "status") == 0)
    {
        VirtualSensor_t data = VirtualSensor_Get();
        printf("[DEBUG] Temp: %.1f C, Hum: %.1f %%, Mode: %s, Alarm: %s\r\n",
               data.temperature,
               data.humidity,
               data.manual_mode ? "MANUAL" : "AUTO",
               data.temp_alarm ? "ALARM" : "NORMAL");
    }
    else if (strncmp(cmd, "temp ", 5) == 0 || strncmp(cmd, "set temp ", 9) == 0)
    {
        char *p_val = (strncmp(cmd, "temp ", 5) == 0) ? (cmd + 5) : (cmd + 9);
        float t = (float)atof(p_val);
        VirtualSensor_SetTemp(t);
        printf("[DEBUG OK] Manual Temp set to: %.1f C\r\n", t);
    }
    else if (strncmp(cmd, "hum ", 4) == 0 || strncmp(cmd, "set hum ", 8) == 0 ||
             strncmp(cmd, "humi ", 5) == 0 || strncmp(cmd, "set humi ", 9) == 0)
    {
        char *p_val = strpbrk(cmd, "0123456789.-");
        if (p_val != NULL)
        {
            float h = (float)atof(p_val);
            VirtualSensor_SetHumi(h);
            printf("[DEBUG OK] Manual Humidity set to: %.1f %%\r\n", h);
        }
    }
    else if (strcmp(cmd, "auto") == 0 || strcmp(cmd, "set auto") == 0)
    {
        VirtualSensor_SetAutoMode();
        printf("[DEBUG OK] Switched to AUTO drift mode.\r\n");
    }
    else if (strcmp(cmd, "stat") == 0 || strcmp(cmd, "stats") == 0)
    {
        EnvAnalyzer_PrintReport();
    }
    else if (strcmp(cmd, "hist") == 0 || strcmp(cmd, "history") == 0)
    {
        EnvAnalyzer_PrintHistory();
    }
    else
    {
        printf("[DEBUG ERR] Unknown command: '%s'. Type 'help' for available commands.\r\n", cmd);
    }
}

/**
 * @brief  调试控制台非阻塞轮询处理函数
 */
void DebugConsole_Process(void)
{
    // 非阻塞检查串口接收标志位 RXNE
    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET)
    {
        uint8_t ch = (uint8_t)(huart1.Instance->DR & 0x00FF);

        // 回车或换行表示一帧指令输入结束
        if (ch == '\r' || ch == '\n')
        {
            if (s_rx_idx > 0)
            {
                s_rx_buffer[s_rx_idx] = '\0';
                printf("\r\n"); // 换行
                ExecuteCommand(s_rx_buffer);
                s_rx_idx = 0;
            }
        }
        // 处理退格键 (Backspace)
        else if (ch == '\b' || ch == 0x7F)
        {
            if (s_rx_idx > 0)
            {
                s_rx_idx--;
                if (s_cfg.echo_enabled)
                {
                    printf("\b \b");
                }
            }
        }
        else
        {
            // 存入缓冲区
            if (s_rx_idx < (CLI_BUFFER_SIZE - 1))
            {
                s_rx_buffer[s_rx_idx++] = (char)ch;
                if (s_cfg.echo_enabled)
                {
                    HAL_UART_Transmit(&huart1, &ch, 1, 10); // 字符实时回显
                }
            }
        }
    }
}

/**
 * @brief  带实时串口指令监听的延时
 */
void DebugConsole_Delay(uint32_t delay_ms)
{
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < delay_ms)
    {
        DebugConsole_Process();
    }
}
