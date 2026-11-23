#ifndef __ESP8266_MQTT_H__
#define __ESP8266_MQTT_H__

#include <stdint.h>
#include <stdbool.h>

/**
  * @brief 初始化 ESP8266 并连接云端 MQTT Broker
  * @retval true 成功, false 失败
  */
bool ESP8266_MQTT_Init(void);

/**
  * @brief 向指定 MQTT 主题推送数据报文 (JSON格式)
  * @param topic 主题名称
  * @param payload JSON 格式字符串
  * @retval true 发布成功, false 失败
  */
bool ESP8266_MQTT_Publish(const char *topic, const char *payload);

#endif /* __ESP8266_MQTT_H__ */
