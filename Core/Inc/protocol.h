#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>

#define FRAME_HEADER_1  0xAAU
#define FRAME_HEADER_2  0x55U
#define FRAME_TAIL      0x0DU
#define TYPE_ENV_DATA   0x01U

void Protocol_SendEnvData(float temp, float humi);
void Protocol_SetEnabled(bool enable);
bool Protocol_IsEnabled(void);

#endif /* __PROTOCOL_H */
