#include "oled.h"
#include "oledfont.h"
#include "bsp_i2c.h"

static uint8_t OLED_Buffer[8][128];

// 发送 OLED 命令
static void OLED_WriteCmd(uint8_t cmd) {
    I2C_Start();
    I2C_SendByte(OLED_I2C_ADDR);
    I2C_WaitAck();
    I2C_SendByte(0x00);
    I2C_WaitAck();
    I2C_SendByte(cmd);
    I2C_WaitAck();
    I2C_Stop();
}

// 发送 OLED 数据
static void OLED_WriteData(uint8_t data) {
    I2C_Start();
    I2C_SendByte(OLED_I2C_ADDR);
    I2C_WaitAck();
    I2C_SendByte(0x40);
    I2C_WaitAck();
    I2C_SendByte(data);
    I2C_WaitAck();
    I2C_Stop();
}

// 清空显存
void OLED_Clear(void) {
    for (uint8_t page = 0; page < 8; page++) {
        for (uint8_t col = 0; col < 128; col++) {
            OLED_Buffer[page][col] = 0x00;
        }
    }
}

// 将显存数据刷入 OLED 屏幕
void OLED_Update(void) {
    for (uint8_t page = 0; page < 8; page++) {
        OLED_WriteCmd(0xB0 + page); // 设置页地址 (0~7)
        OLED_WriteCmd(0x00);        // 设置列低地址
        OLED_WriteCmd(0x10);        // 设置列高地址
        
        I2C_Start();
        I2C_SendByte(OLED_I2C_ADDR);
        I2C_WaitAck();
        I2C_SendByte(0x40);         // 0x40: 数据流控制字节
        I2C_WaitAck();
        for (uint8_t col = 0; col < 128; col++) {
            I2C_SendByte(OLED_Buffer[page][col]);
            I2C_WaitAck();
        }
        I2C_Stop();
    }
}

void OLED_UpdateScreen(void) {
    OLED_Update();
}

void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color) {
    if (x >= 128 || y >= 64) return;
    if (color) {
        OLED_Buffer[y / 8][x] |= (1 << (y % 8));
    } else {
        OLED_Buffer[y / 8][x] &= ~(1 << (y % 8));
    }
}

void OLED_Init(void) {
    HAL_Delay(100); // 上电等待稳定

    OLED_WriteCmd(0xAE); // 关闭显示 (Display OFF)
    OLED_WriteCmd(0xD5); // 设置显示时钟分频比/振荡器频率
    OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xA8); // 设置多路复用率 (Multiplex Ratio)
    OLED_WriteCmd(0x3F); // 1/64 duty (对应 128x64 分辨率)
    OLED_WriteCmd(0xD3); // 设置显示偏移
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x40); // 设置起始行 (0)
    OLED_WriteCmd(0x8D); // 电荷泵设置
    OLED_WriteCmd(0x14); // 启用电荷泵
    OLED_WriteCmd(0x20); // 设置内存寻址模式
    OLED_WriteCmd(0x02); // 页寻址模式
    OLED_WriteCmd(0xA1); // 段重映射 (SEG Remap)
    OLED_WriteCmd(0xC8); // 行扫描方向 (COM Scan Direction)
    OLED_WriteCmd(0xDA); // 设置 COM 引脚硬件配置
    OLED_WriteCmd(0x12);
    OLED_WriteCmd(0x81); // 设置对比度
    OLED_WriteCmd(0xCF);
    OLED_WriteCmd(0xD9); // 设置预充电周期
    OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDB); // 设置 VCOMH 反选电平
    OLED_WriteCmd(0x40);
    OLED_WriteCmd(0xA4); // 全局显示跟随 RAM 内容
    OLED_WriteCmd(0xA6); // 设置正常显示
    
    // 清空显存并同步到屏幕
    OLED_Clear();
    OLED_Update();
    
    OLED_WriteCmd(0xAF); // 开启显示 (Display ON)
}

// 显示单个 8x16 字符
void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size) {
    uint8_t c = chr - ' ';
    if (x > 120) return;
    
    // 兼容行号(0,2,4,6)与像素坐标(0,16,32,48)
    uint8_t py = (y <= 7 && size == 16) ? (y * 8) : y;
    if (py > 48) return;

    for (uint8_t i = 0; i < 8; i++) {
        uint8_t temp1 = OLED_F8x16[c][i];
        for (uint8_t m = 0; m < 8; m++) {
            if (temp1 & (1 << m)) {
                OLED_DrawPoint(x + i, py + m, 1);
            } else {
                OLED_DrawPoint(x + i, py + m, 0);
            }
        }

        uint8_t temp2 = OLED_F8x16[c][i + 8];
        for (uint8_t m = 0; m < 8; m++) {
            if (temp2 & (1 << m)) {
                OLED_DrawPoint(x + i, py + 8 + m, 1);
            } else {
                OLED_DrawPoint(x + i, py + 8 + m, 0);
            }
        }
    }
}

// 显示字符串
void OLED_ShowString(uint8_t x, uint8_t y, char *str, uint8_t size) {
    uint8_t cur_x = x;
    uint8_t cur_y = y;
    while (*str != '\0') {
        if (cur_x > 120) {
            cur_x = 0;
            if (cur_y <= 7 && size == 16) {
                cur_y += 2;
            } else {
                cur_y += size;
            }
        }
        uint8_t py = (cur_y <= 7 && size == 16) ? (cur_y * 8) : cur_y;
        if (py > 48) break;
        OLED_ShowChar(cur_x, cur_y, *str, size);
        cur_x += (size == 16) ? 8 : 6;
        str++;
    }
}