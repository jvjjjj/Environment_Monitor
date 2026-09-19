#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include "stm32f1xx_hal.h"

#define FRAME_HEADER_1  0xAA
#define FRAME_HEADER_2  0x55
#define FRAME_TAIL      0x0D

#define TYPE_ENV_DATA   0x01

// 发送一包标准环境数据
void Protocol_SendEnvData(float temp, float humi);

#endif
