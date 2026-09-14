#ifndef __OLED_H
#define __OLED_H

#include "main.h"

// SSD1306 7 位 I2C 地址为 0x3C,左移 1 位后写入地址为 0x78
#define OLED_I2C_ADDR   0x78

#define OLED_PORT       GPIOB
#define OLED_SCL_PIN    GPIO_PIN_5
#define OLED_SDA_PIN    GPIO_PIN_6

// 函数声明
void OLED_Init(void);
void OLED_Clear(void);
void OLED_UpdateScreen(void);
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color);
void OLED_ShowChar(uint8_t x, uint8_t y, char chr);
void OLED_ShowString(uint8_t x, uint8_t y, char *str);

#endif