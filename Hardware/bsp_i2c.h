#ifndef __BSP_I2C_H
#define __BSP_I2C_H

#include "stm32f1xx_hal.h"

// 引脚定义 (PB6: SCL, PB7: SDA)
#define I2C_SCL_PIN        GPIO_PIN_6
#define I2C_SDA_PIN        GPIO_PIN_7
#define I2C_PORT           GPIOB

#define I2C_SCL_1()        HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_SET)
#define I2C_SCL_0()        HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_RESET)
#define I2C_SDA_1()        HAL_GPIO_WritePin(I2C_PORT, I2C_SDA_PIN, GPIO_PIN_SET)
#define I2C_SDA_0()        HAL_GPIO_WritePin(I2C_PORT, I2C_SDA_PIN, GPIO_PIN_RESET)
#define I2C_SDA_READ()     HAL_GPIO_ReadPin(I2C_PORT, I2C_SDA_PIN)

void Delay_us(uint32_t us);
void I2C_Bus_Init(void);
void I2C_Start(void);
void I2C_Stop(void);
void I2C_SendByte(uint8_t byte);
uint8_t I2C_ReadByte(uint8_t ack);
uint8_t I2C_WaitAck(void);
void I2C_Ack(void);
void I2C_NAck(void);

// 核心调试诊断：总线扫描函数
uint8_t I2C_CheckDevice(uint8_t dev_addr);

#endif
