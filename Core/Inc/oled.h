#ifndef __OLED_H
#define __OLED_H

#include "main.h"

// SSD1306 默认 7 位从机地址为 0x3C，左移 1 位得到 8 位写地址 0x78
#define OLED_I2C_ADDR   0x78

// 核心功能 API
void OLED_Init(void);
void OLED_Clear(void);
void OLED_UpdateScreen(void);
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color);
void OLED_ShowChar(uint8_t x, uint8_t y, char chr);
void OLED_ShowString(uint8_t x, uint8_t y, char *str);

#endif /* __OLED_H */