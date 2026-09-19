#ifndef __OLED_H
#define __OLED_H

#include "main.h"

/* SSD1306 7位 I2C 地址 0x3C，写地址为 0x78 */
#define OLED_I2C_ADDR   0x78

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Update(void);
void OLED_UpdateScreen(void);
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color);
void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size);
void OLED_ShowString(uint8_t x, uint8_t y, char *str, uint8_t size);

#endif