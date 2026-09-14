#include "oled.h"
#include "oledfont.h"
static uint8_t OLED_Buffer[8][128];

// 模拟 I2C 延时 (适当微调以在 Proteus 仿真中高速且稳定通信)
static void I2C_Delay(void) {
    for (volatile uint32_t i = 0; i < 5; i++) {
        __NOP();
    }
}

// 产生 I2C 起始信号
static void I2C_Start(void) {
    HAL_GPIO_WritePin(OLED_PORT, OLED_SDA_PIN, GPIO_PIN_SET);   // SDA=1
    HAL_GPIO_WritePin(OLED_PORT, OLED_SCL_PIN, GPIO_PIN_SET);   // SCL=1
    I2C_Delay();
    HAL_GPIO_WritePin(OLED_PORT, OLED_SDA_PIN, GPIO_PIN_RESET); // SDA=0
    I2C_Delay();
    HAL_GPIO_WritePin(OLED_PORT, OLED_SCL_PIN, GPIO_PIN_RESET); // SCL=0
    I2C_Delay();
}

// 产生 I2C 停止信号
static void I2C_Stop(void) {
    HAL_GPIO_WritePin(OLED_PORT, OLED_SCL_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OLED_PORT, OLED_SDA_PIN, GPIO_PIN_RESET);
    I2C_Delay();
    HAL_GPIO_WritePin(OLED_PORT, OLED_SCL_PIN, GPIO_PIN_SET);
    I2C_Delay();
    HAL_GPIO_WritePin(OLED_PORT, OLED_SDA_PIN, GPIO_PIN_SET);
    I2C_Delay();
}

// 发送单字节
static void I2C_SendByte(uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        if (byte & 0x80) {
            HAL_GPIO_WritePin(OLED_PORT, OLED_SDA_PIN, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(OLED_PORT, OLED_SDA_PIN, GPIO_PIN_RESET);
        }
        I2C_Delay();
        HAL_GPIO_WritePin(OLED_PORT, OLED_SCL_PIN, GPIO_PIN_SET);
        I2C_Delay();
        HAL_GPIO_WritePin(OLED_PORT, OLED_SCL_PIN, GPIO_PIN_RESET);
        I2C_Delay();
        byte <<= 1;
    }
    // 第 9 个时钟脉冲：释放 SDA 并提供 ACK 时钟
    HAL_GPIO_WritePin(OLED_PORT, OLED_SDA_PIN, GPIO_PIN_SET);
    I2C_Delay();
    HAL_GPIO_WritePin(OLED_PORT, OLED_SCL_PIN, GPIO_PIN_SET);
    I2C_Delay();
    HAL_GPIO_WritePin(OLED_PORT, OLED_SCL_PIN, GPIO_PIN_RESET);
    I2C_Delay();
}

// 发送命令 (0x00 为命令控制字节)
static void OLED_WriteCmd(uint8_t cmd) {
    I2C_Start();
    I2C_SendByte(OLED_I2C_ADDR);
    I2C_SendByte(0x00);
    I2C_SendByte(cmd);
    I2C_Stop();
}

// 发送单字节数据 (0x40 为数据控制字节)
static void OLED_WriteData(uint8_t data) {
    I2C_Start();
    I2C_SendByte(OLED_I2C_ADDR);
    I2C_SendByte(0x40);
    I2C_SendByte(data);
    I2C_Stop();
}

void OLED_Clear(void) {
    for (uint8_t page = 0; page < 8; page++) {
        for (uint8_t col = 0; col < 128; col++) {
            OLED_Buffer[page][col] = 0x00;
        }
    }
}

// 将显存数据按页刷入 OLED (标准 I2C 连续数据流传输)
void OLED_UpdateScreen(void) {
    for (uint8_t page = 0; page < 8; page++) {
        OLED_WriteCmd(0xB0 + page); // 设置页地址 (0~7)
        OLED_WriteCmd(0x00);        // 设置列低地址
        OLED_WriteCmd(0x10);        // 设置列高地址
        
        I2C_Start();
        I2C_SendByte(OLED_I2C_ADDR);
        I2C_SendByte(0x40);         // 0x40: 控制字节 (Co=0, D/C=1, 之后全为显示数据)
        for (uint8_t col = 0; col < 128; col++) {
            I2C_SendByte(OLED_Buffer[page][col]);
        }
        I2C_Stop();
    }
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
    // 电源稳定延时
    for (volatile int i = 0; i < 50000; i++) __NOP();

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
    OLED_WriteCmd(0xDA); // 设置 COM 引脚硬件配置 (关键: 128x64 必须为 0x12, 否则显示错位或全白)
    OLED_WriteCmd(0x12);
    OLED_WriteCmd(0x81); // 设置对比度
    OLED_WriteCmd(0xCF);
    OLED_WriteCmd(0xD9); // 设置预充电周期
    OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDB); // 设置 VCOMH 反选电平
    OLED_WriteCmd(0x40);
    OLED_WriteCmd(0xA4); // 全局显示跟随 RAM 内容 (A4跟随, A5全亮)
    OLED_WriteCmd(0xA6); // 设置正常显示 (A6正常, A7反相)
    
    // 清空显存并同步到屏幕
    OLED_Clear();
    OLED_UpdateScreen();
    
    OLED_WriteCmd(0xAF); // 开启显示 (Display ON)
}
// ??????? (x, y) ???? 8x16 ????
// x ??: 0 ~ 120, y ??: 0 ~ 48
void OLED_ShowChar(uint8_t x, uint8_t y, char chr) {
    uint8_t c = chr - ' '; // ??????????????
    if (x > 120 || y > 48) return;

    for (uint8_t i = 0; i < 8; i++) {
        // ??? 8 ????
        uint8_t temp1 = OLED_F8x16[c][i];
        for (uint8_t m = 0; m < 8; m++) {
            if (temp1 & (1 << m)) {
                OLED_DrawPoint(x + i, y + m, 1);
            } else {
                OLED_DrawPoint(x + i, y + m, 0);
            }
        }

        // ??? 8 ????
        uint8_t temp2 = OLED_F8x16[c][i + 8];
        for (uint8_t m = 0; m < 8; m++) {
            if (temp2 & (1 << m)) {
                OLED_DrawPoint(x + i, y + 8 + m, 1);
            } else {
                OLED_DrawPoint(x + i, y + 8 + m, 0);
            }
        }
    }
}

// ??????? (x, y) ???????
void OLED_ShowString(uint8_t x, uint8_t y, char *str) {
    while (*str != '\0') {
        if (x > 120) { // ??????,????
            x = 0;
            y += 16;
        }
        if (y > 48) break; // ???????
        OLED_ShowChar(x, y, *str);
        x += 8; // ?? 8x16 ??? 8 ??
        str++;
    }
}