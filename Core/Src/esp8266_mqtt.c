#include "esp8266_mqtt.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

#define AT_BUF_SIZE 256
static char s_tx_buf[AT_BUF_SIZE];
static char s_rx_buf[AT_BUF_SIZE];

/**
  * @brief  发送 AT 指令并等待匹配的应答字符串
  */
static bool ESP8266_SendCmd(const char *cmd, const char *expected, uint32_t timeout_ms)
{
    // 1. 发送指令
    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 100);

    // 2. 超时等待接收匹配
    uint32_t start = HAL_GetTick();
    uint16_t rx_cnt = 0;
    memset(s_rx_buf, 0, sizeof(s_rx_buf));

    while (HAL_GetTick() - start < timeout_ms)
    {
        uint8_t ch = 0;
        if (HAL_UART_Receive(&huart1, &ch, 1, 10) == HAL_OK)
        {
            if (rx_cnt < sizeof(s_rx_buf) - 1)
            {
                s_rx_buf[rx_cnt++] = (char)ch;
                s_rx_buf[rx_cnt] = '\0';

                // 检查是否包含期望关键字
                if (strstr(s_rx_buf, expected) != NULL)
                {
                    return true;
                }
            }
        }
    }

    return false; // 等待超时
}

/**
  * @brief  初始化 ESP8266 并连接云端 MQTT Broker
  */
bool ESP8266_MQTT_Init(void)
{
    // 步骤 1: AT 连通性测试 (握手)
    if (!ESP8266_SendCmd("AT\r\n", "OK", 500))
    {
        return false;
    }

    // 步骤 2: 连接 WiFi
    if (!ESP8266_SendCmd("AT+CWJAP=\"Virtual_WiFi\",\"12345678\"\r\n", "OK", 1000))
    {
        return false;
    }

    // 步骤 3: 建立 MQTT 云端连接 (连接 broker.emqx.io)
    if (!ESP8266_SendCmd("AT+MQTTCONN=0,\"broker.emqx.io\",1883,1\r\n", "OK", 1000))
    {
        return false;
    }

    return true;
}

/**
  * @brief  向指定主题发布 MQTT JSON 报文
  */
bool ESP8266_MQTT_Publish(const char *topic, const char *payload)
{
    // 组装标准 MQTT 发布指令: AT+MQTTPUB=0,"topic","payload",0,0
    snprintf(s_tx_buf, sizeof(s_tx_buf), "AT+MQTTPUB=0,\"%s\",\"%s\",0,0\r\n", topic, payload);

    return ESP8266_SendCmd(s_tx_buf, "OK", 1000);
}
