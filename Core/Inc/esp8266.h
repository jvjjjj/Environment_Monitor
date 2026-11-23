#ifndef __ESP8266_H
#define __ESP8266_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define ESP8266_RX_BUF_SIZE 256

typedef enum {
    ESP8266_OK = 0,
    ESP8266_TIMEOUT,
    ESP8266_ERROR,
    ESP8266_FAIL
} ESP8266_Status_t;

// 网络连接状态枚举（供 OLED 屏幕显示）
typedef enum {
    NET_STATE_IDLE = 0,     // 初始未联网
    NET_STATE_AT_TEST,      // 正在握手 AT
    NET_STATE_WIFI_CONNECT, // 正在连接 WiFi
    NET_STATE_MQTT_CONNECT, // 正在连接 MQTT Broker
    NET_STATE_ONLINE,       // 已在线，正常上传
    NET_STATE_FAIL          // 离线/失败
} NetState_t;

extern volatile NetState_t g_net_state;

// 函数声明
void ESP8266_InitHardware(void);
void ESP8266_RxCallback(uint8_t data);
void ESP8266_ClearRx(void);
ESP8266_Status_t ESP8266_SendCmd(const char *cmd, const char *expect_res, uint32_t timeout_ms);
bool ESP8266_ConnectWiFi(const char *ssid, const char *pwd);
bool ESP8266_ConnectMQTT(const char *broker_ip, uint16_t port);
bool ESP8266_PublishData(float temp, float humi);

#endif /* __ESP8266_H */
