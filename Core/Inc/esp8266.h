#ifndef __ESP8266_H
#define __ESP8266_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define ESP8266_RX_BUF_SIZE 512U

typedef enum {
    ESP8266_OK = 0,
    ESP8266_ALREADY_CONNECTED,
    ESP8266_TIMEOUT,
    ESP8266_ERROR,
    ESP8266_FAIL
} ESP8266_Status_t;

/* 网络连接状态枚举（供 OLED 屏幕显示） */
typedef enum {
    NET_STATE_IDLE = 0,     /* 初始未联网 */
    NET_STATE_AT_TEST,      /* 正在握手 AT */
    NET_STATE_WIFI_CONNECT, /* 正在连接 WiFi */
    NET_STATE_MQTT_CONNECT, /* 正在连接 MQTT Broker */
    NET_STATE_ONLINE,       /* 已在线，正常推流 */
    NET_STATE_FAIL          /* 离线/退避重连中 */
} NetState_t;

extern volatile NetState_t g_net_state;

void ESP8266_InitHardware(void);
void ESP8266_RxCallback(uint8_t data);
void ESP8266_ClearRx(void);

ESP8266_Status_t ESP8266_SendCmd(const char *cmd, const char *expect_res, uint32_t timeout_ms);
bool ESP8266_TestAT(void);
bool ESP8266_ConnectWiFi(const char *ssid, const char *pwd);
bool ESP8266_ConnectMQTT(const char *broker_host, uint16_t port, const char *client_id);
bool ESP8266_PublishEnv(const char *topic, const EnvData_t *env);

#endif /* __ESP8266_H */
