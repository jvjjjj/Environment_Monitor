#ifndef __PROTOCOL_H
#define __PROTOCOL_H
#define FRAME_HEADER_1  0x5A  // 改为 'Z' (ASCII 90)
#define FRAME_HEADER_2  0xA5  // 或者改为 0x24 '$'
#include "stm32f1xx_hal.h"

#define FRAME_HEADER_1  0xAA
#define FRAME_HEADER_2  0x55
#define FRAME_TAIL      0x0D

#define TYPE_ENV_DATA   0x01

// 发送一包标准环境数据
void Protocol_SendEnvData(float temp, float humi);

#endif
