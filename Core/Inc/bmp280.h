#ifndef __BMP280_H
#define __BMP280_H

#include "stm32f1xx_hal.h"

// BMP280 7位 I2C 从机候选地址
#define BMP280_ADDR_L           0x76  // SDO 接地 (GND)
#define BMP280_ADDR_H           0x77  // SDO 接高电平或上拉 (VCC)

// 关键寄存器映射
#define BMP280_CHIP_ID_REG      0xD0
#define BMP280_RESET_REG        0xE0
#define BMP280_STATUS_REG       0xF3
#define BMP280_CTRL_MEAS_REG    0xF4
#define BMP280_CONFIG_REG       0xF5
#define BMP280_DATA_START_REG   0xF7

// 初始化 BMP280 (自动双地址探测 0x76/0x77 与多型号 ID 识别，返回 0: 成功, 1: 失败)
uint8_t BMP280_Init(void);

// 读取物理气压值 (单位: Pa) 与温度 (单位: ℃)
uint8_t BMP280_ReadData(float *temperature, float *pressure);

// 根据标准大气压估算海拔高度 (单位: 米)
float BMP280_CalcAltitude(float pressure);

// 获取探测到的 I2C 设备地址 (0x76 或 0x77)
uint8_t BMP280_GetAddr(void);

// 获取读取到的芯片 ID (例如 0x58, 0x60, 0x56 等)
uint8_t BMP280_GetChipID(void);

// 查询 BMP280 是否已就绪可用
uint8_t BMP280_IsReady(void);

#endif
