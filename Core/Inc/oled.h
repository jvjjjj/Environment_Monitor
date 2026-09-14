#ifndef __OLED_H
#define __OLED_H

#include "main.h"

// SSD1306 7位 I2C 地址为 0x3C，左移 1 位后写入地址为 0x78
#define OLED_I2C_ADDR   0x78

/* 
 * 硬件引脚定义说明：
 * 在 Proteus 原理图 (Environment_Monitor.pdsprj) 中：
 *   - OLED Pin 3 (SCL) 实际连接到了 STM32 的 Pin 41 (PB5)
 *   - OLED Pin 4 (SDA) 实际连接到了 STM32 的 Pin 42 (PB6)
 *
 * 默认使用当前 Proteus 原理图连接：PB5=SCL, PB6=SDA。
 * 若在 Proteus 中修改接线为标准引脚（Pin 42=PB6, Pin 43=PB7），可将宏切换为 PB6/PB7。
 */
#define OLED_PORT       GPIOB
#define OLED_SCL_PIN    GPIO_PIN_5
#define OLED_SDA_PIN    GPIO_PIN_6

void OLED_Init(void);
void OLED_Clear(void);
void OLED_UpdateScreen(void);
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color);
void OLED_ShowChar(uint8_t x, uint8_t y, char chr);
void OLED_ShowString(uint8_t x, uint8_t y, char *str);

#endif