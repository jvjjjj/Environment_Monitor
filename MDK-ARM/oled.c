#include "oled.h"
#include "oledfont.h"
static uint8_t OLED_Buffer[8][128];

// ??????
static void I2C_Delay(void) {
    for (volatile uint32_t i = 0; i < 50; i++) {
        __NOP();
    }
}

// ?? I2C ????
static void I2C_Start(void) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);   // SDA=1
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // SCL=1
    I2C_Delay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET); // SDA=0
    I2C_Delay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // SCL=0
    I2C_Delay();
}

// ?? I2C ??
static void I2C_Stop(void) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
    I2C_Delay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    I2C_Delay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    I2C_Delay();
}

// ??????
static void I2C_SendByte(uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        if (byte & 0x80) {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
        }
        I2C_Delay();
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        I2C_Delay();
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
        I2C_Delay();
        byte <<= 1;
    }
    // ??? 9 ?????????
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET); // ?? SDA
    I2C_Delay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    I2C_Delay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    I2C_Delay();
}

// ?????? (?? 0x78 ?? 8 ?????)
static void OLED_WriteCmd(uint8_t cmd) {
    I2C_Start();
    I2C_SendByte(0x78);
    I2C_SendByte(0x00);
    I2C_SendByte(cmd);
    I2C_Stop();
}

// ????
static void OLED_WriteData(uint8_t data) {
    I2C_Start();
    I2C_SendByte(0x78);
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

void OLED_UpdateScreen(void) {
    for (uint8_t page = 0; page < 8; page++) {
        OLED_WriteCmd(0xB0 + page);
        OLED_WriteCmd(0x00);
        OLED_WriteCmd(0x10);
        for (uint8_t col = 0; col < 128; col++) {
            OLED_WriteData(OLED_Buffer[page][col]);
        }
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
    // ???????,??? HAL_Delay ?? SysTick ??
    for (volatile int i = 0; i < 50000; i++) __NOP();

    OLED_WriteCmd(0xAE); // ????
    OLED_WriteCmd(0x20); // ????
    OLED_WriteCmd(0x02); // ?????
    OLED_WriteCmd(0x8D); // ???
    OLED_WriteCmd(0x14);
    OLED_WriteCmd(0xA1); // ????????????
    OLED_WriteCmd(0xC8); // ????
    OLED_WriteCmd(0x81); // ???
    OLED_WriteCmd(0xCF);
    OLED_WriteCmd(0xA4); // ???? RAM
    OLED_WriteCmd(0xA6); // ???? (1 ??,0 ??)
    
    // ????
    OLED_Clear();
    OLED_UpdateScreen();
    
    OLED_WriteCmd(0xAF); // ????
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