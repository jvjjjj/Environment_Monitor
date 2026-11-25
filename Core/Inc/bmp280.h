#ifndef __BMP280_H
#define __BMP280_H

#include "stm32f1xx_hal.h"

/* BMP280 7位 I2C 从机候选地址 */
#define BMP280_ADDR_L           0x76U  /* SDO 接地 (GND) */
#define BMP280_ADDR_H           0x77U  /* SDO 接高电平 (VCC) */

/* 关键寄存器映射 */
#define BMP280_CHIP_ID_REG      0xD0U
#define BMP280_RESET_REG        0xE0U
#define BMP280_STATUS_REG       0xF3U
#define BMP280_CTRL_MEAS_REG    0xF4U
#define BMP280_CONFIG_REG       0xF5U
#define BMP280_DATA_START_REG   0xF7U

uint8_t BMP280_Init(void);
uint8_t BMP280_ReadData(float *temperature, float *pressure_pa);
float   BMP280_CalcAltitude(float pressure_pa);

uint8_t BMP280_GetAddr(void);
uint8_t BMP280_GetChipID(void);
uint8_t BMP280_IsReady(void);

#endif /* __BMP280_H */
