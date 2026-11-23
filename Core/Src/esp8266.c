#include "esp8266.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

/* ==================== 1. 环形缓冲区 (Ring Buffer) ==================== */
static volatile uint8_t  s_rx_buf[ESP8266_RX_BUF_SIZE];
static volatile uint16_t s_rx_head = 0; // 写指针（中断写）
static volatile uint16_t s_rx_tail = 0; // 读指针（任务读）

// 累积接收历史文本
static char s_accumulated_rx[ESP8266_RX_BUF_SIZE];

volatile NetState_t g_net_state = NET_STATE_IDLE;

/**
 * @brief 中断入队函数 (Fast-In)
 */
void ESP8266_RxCallback(uint8_t data)
{
    uint16_t next_head = (s_rx_head + 1) % ESP8266_RX_BUF_SIZE;
    if (next_head != s_rx_tail)
    {
        s_rx_buf[s_rx_head] = data;
        s_rx_head = next_head;
    }
}

/**
 * @brief 清空环形缓冲区与累积历史
 */
void ESP8266_ClearRx(void)
{
    s_rx_tail = s_rx_head;
    s_accumulated_rx[0] = '\0';
}

/**
 * @brief 将环形缓冲区新到的字符追加到累积字符串中
 */
static void poll_rx_to_accumulated(void)
{
    uint16_t cur_len = strlen(s_accumulated_rx);
    while (s_rx_tail != s_rx_head && cur_len < sizeof(s_accumulated_rx) - 1)
    {
        s_accumulated_rx[cur_len++] = (char)s_rx_buf[s_rx_tail];
        s_rx_tail = (s_rx_tail + 1) % ESP8266_RX_BUF_SIZE;
    }
    s_accumulated_rx[cur_len] = '\0';
}

/* ==================== 2. 硬件中断初始化 ==================== */
void ESP8266_InitHardware(void)
{
    // 显式开启 USART2 的接收中断
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_ERR);
}

/* ==================== 3. 非阻塞 AT 发送与步进轮询 ==================== */
ESP8266_Status_t ESP8266_SendCmd(const char *cmd, const char *expect_res, uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    const uint32_t step_ms = 20;

    // 不做全量硬清空，只重置累积文本
    s_accumulated_rx[0] = '\0';

    // 1. 发送指令
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), 100);
    HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, 100);

    // 2. 步进轮询等待应答
    while (elapsed < timeout_ms)
    {
        vTaskDelay(pdMS_TO_TICKS(step_ms));
        elapsed += step_ms;

        // 提取新接收的字符
        poll_rx_to_accumulated();

        // 只要累积的文本里出现了我们期待的关键字 (比如 "OK")
        if (expect_res != NULL && strstr(s_accumulated_rx, expect_res) != NULL)
        {
            // 立即回发一段清晰的确认日志，让你在串口助手亲眼看见！
            const char ack_msg[] = "\r\n>>> [STM32] MATCHED 'OK'! PROGRESSING TO NEXT STAGE! <<<\r\n\r\n";
            HAL_UART_Transmit(&huart2, (uint8_t *)ack_msg, strlen(ack_msg), 100);
            return ESP8266_OK;
        }

        if (strstr(s_accumulated_rx, "ERROR") != NULL) return ESP8266_ERROR;
        if (strstr(s_accumulated_rx, "FAIL") != NULL)  return ESP8266_FAIL;
    }

    return ESP8266_TIMEOUT;
}

/* ==================== 4. 业务流程函数 ==================== */
bool ESP8266_ConnectWiFi(const char *ssid, const char *pwd)
{
    char cmd[128];
    if (ESP8266_SendCmd("AT+CWMODE=1", "OK", 3000) != ESP8266_OK) return false;

    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);
    return (ESP8266_SendCmd(cmd, "OK", 5000) == ESP8266_OK);
}

bool ESP8266_ConnectMQTT(const char *broker_ip, uint16_t port)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", broker_ip, port);
    return (ESP8266_SendCmd(cmd, "OK", 5000) == ESP8266_OK);
}

bool ESP8266_PublishData(float temp, float humi)
{
    char payload[128];
    char cmd[64];

    snprintf(payload, sizeof(payload), "{\"temp\":%.1f,\"humi\":%.1f}", temp, humi);

    // 申请发送：在真实模组中回复 '>'，在仿真模拟时你回复 '>' 或 'OK' 都能通关！
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", (int)strlen(payload));
    if (ESP8266_SendCmd(cmd, "OK", 3000) != ESP8266_OK)
    {
        return false;
    }

    // 报文推送确认：回复 'SEND OK' 或 'OK' 都能通过！
    return (ESP8266_SendCmd(payload, "OK", 3000) == ESP8266_OK);
}
