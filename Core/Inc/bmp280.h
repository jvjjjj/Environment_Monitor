#ifndef __BMP280_H
#define __BMP280_H

#include "stm32f1xx_hal.h"

#define BMP280_ADDR             0x76  // 若无应答可尝试 0x77
#define BMP280_CHIP_ID_REG      0xD0
#define BMP280_RESET_REG        0xE0
#define BMP280_STATUS_REG       0xF3
#define BMP280_CTRL_MEAS_REG    0xF4
#define BMP280_CONFIG_REG       0xF5
#define BMP280_DATA_START_REG   0xF7

// 初始化 BMP280 (返回 0: 成功, 1: 失败)
uint8_t BMP280_Init(void);

// 读取物理气压值 (单位: Pa) 与温度 (单位: ℃)
uint8_t BMP280_ReadData(float *temperature, float *pressure);

// 根据标准大气压估算海拔高度 (单位: 米)
float BMP280_CalcAltitude(float pressure);

#endif
