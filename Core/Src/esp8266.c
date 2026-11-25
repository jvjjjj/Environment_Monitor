#include "esp8266.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

/* ==================== 1. SPSC 免锁环形缓冲区 (Ring Buffer) ==================== */
/* 生产者: USART2_IRQHandler (仅写 s_rx_head)
 * 消费者: Task_IoT          (仅写 s_rx_tail)
 * Cortex-M3 上 16 位对齐读写具备指令级原子性，配合 volatile 实现 SPSC 无锁安全。 */
static volatile uint8_t  s_rx_buf[ESP8266_RX_BUF_SIZE];
static volatile uint16_t s_rx_head = 0U;
static volatile uint16_t s_rx_tail = 0U;

static char s_accumulated_rx[ESP8266_RX_BUF_SIZE];

volatile NetState_t g_net_state = NET_STATE_IDLE;

void ESP8266_RxCallback(uint8_t data)
{
    uint16_t next_head = (uint16_t)((s_rx_head + 1U) % ESP8266_RX_BUF_SIZE);
    if (next_head != s_rx_tail)
    {
        s_rx_buf[s_rx_head] = data;
        s_rx_head = next_head;
    }
}

void ESP8266_ClearRx(void)
{
    s_rx_tail = s_rx_head;
    s_accumulated_rx[0] = '\0';
}

static void poll_rx_to_accumulated(void)
{
    size_t cur_len = strlen(s_accumulated_rx);
    while (s_rx_tail != s_rx_head && cur_len < (sizeof(s_accumulated_rx) - 1U))
    {
        s_accumulated_rx[cur_len++] = (char)s_rx_buf[s_rx_tail];
        s_rx_tail = (uint16_t)((s_rx_tail + 1U) % ESP8266_RX_BUF_SIZE);
    }
    s_accumulated_rx[cur_len] = '\0';
}

/* ==================== 2. 硬件中断初始化 ==================== */
void ESP8266_InitHardware(void)
{
    ESP8266_ClearRx();
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_ERR);
}

/* ==================== 3. 非阻塞 AT 步进轮询引擎 ==================== */
ESP8266_Status_t ESP8266_SendCmd(const char *cmd, const char *expect_res, uint32_t timeout_ms)
{
    uint32_t elapsed = 0U;
    const uint32_t step_ms = 20U;

    ESP8266_ClearRx();

    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, (uint16_t)strlen(cmd), 100U);
    HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2U, 100U);

    while (elapsed < timeout_ms)
    {
        vTaskDelay(pdMS_TO_TICKS(step_ms));
        elapsed += step_ms;

        poll_rx_to_accumulated();

        if (expect_res != NULL && strstr(s_accumulated_rx, expect_res) != NULL)
        {
            return ESP8266_OK;
        }
        if (strstr(s_accumulated_rx, "ALREADY CONNECTED") != NULL)
        {
            return ESP8266_ALREADY_CONNECTED;
        }
        if (strstr(s_accumulated_rx, "ERROR") != NULL)
        {
            return ESP8266_ERROR;
        }
        if (strstr(s_accumulated_rx, "FAIL") != NULL)
        {
            return ESP8266_FAIL;
        }
    }

    return ESP8266_TIMEOUT;
}

/* ==================== 4. 乐鑫标准 AT + MQTT 业务封装 ==================== */
bool ESP8266_TestAT(void)
{
    if (ESP8266_SendCmd("AT", "OK", 1000U) != ESP8266_OK)
    {
        return false;
    }
    /* 关闭本地回显 (ATE0)，防止指令回显污染环形缓冲区匹配 */
    (void)ESP8266_SendCmd("ATE0", "OK", 500U);
    return true;
}

bool ESP8266_ConnectWiFi(const char *ssid, const char *pwd)
{
    char cmd[128];

    if (ESP8266_SendCmd("AT+CWMODE=1", "OK", 2000U) != ESP8266_OK)
    {
        return false;
    }

    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);
    ESP8266_Status_t st = ESP8266_SendCmd(cmd, "OK", 5000U);
    return (st == ESP8266_OK || st == ESP8266_ALREADY_CONNECTED);
}

bool ESP8266_ConnectMQTT(const char *broker_host, uint16_t port, const char *client_id)
{
    char cmd[160];

    /* 配置 MQTT 客户端 ID (兼容乐鑫官方 MQTT AT 固件及本地 HIL 网关) */
    snprintf(cmd, sizeof(cmd), "AT+MQTTUSERCFG=0,1,\"%s\",\"\",\"\",0,0,\"\"", client_id);
    (void)ESP8266_SendCmd(cmd, "OK", 1500U);

    snprintf(cmd, sizeof(cmd), "AT+MQTTCONN=0,\"%s\",%u,1", broker_host, (unsigned int)port);
    ESP8266_Status_t st = ESP8266_SendCmd(cmd, "OK", 5000U);
    return (st == ESP8266_OK || st == ESP8266_ALREADY_CONNECTED);
}

bool ESP8266_PublishEnv(const char *topic, const EnvData_t *env)
{
    char raw_json[128];
    char escaped_json[192];
    char cmd[256];

    if (topic == NULL || env == NULL)
    {
        return false;
    }

    /* 1. 构造标准物模型 JSON 报文 */
    snprintf(raw_json, sizeof(raw_json),
             "{\"temp\":%.1f,\"humi\":%.1f,\"press\":%.1f,\"alt\":%.1f}",
             env->temperature, env->humidity, env->pressure, env->altitude);

    /* 2. 对 JSON 内部双引号执行自动转义 (" -> \")，防止截断 AT+MQTTPUB 参数 */
    size_t j = 0U;
    for (size_t i = 0U; raw_json[i] != '\0' && j < (sizeof(escaped_json) - 2U); i++)
    {
        if (raw_json[i] == '"')
        {
            escaped_json[j++] = '\\';
        }
        escaped_json[j++] = raw_json[i];
    }
    escaped_json[j] = '\0';

    /* 3. QoS 0 轻量遥测发布 */
    snprintf(cmd, sizeof(cmd), "AT+MQTTPUB=0,\"%s\",\"%s\",0,0", topic, escaped_json);
    return (ESP8266_SendCmd(cmd, "OK", 2000U) == ESP8266_OK);
}
