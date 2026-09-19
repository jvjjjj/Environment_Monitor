#ifndef __AHT20_H
#define __AHT20_H

#include "stm32f1xx_hal.h"

#define AHT20_ADDR   0x38

uint8_t AHT20_Init(void);
uint8_t AHT20_ReadData(float *temperature, float *humidity);

#endif
