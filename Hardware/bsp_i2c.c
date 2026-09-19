#include "bsp_i2c.h"

void Delay_us(uint32_t us) {
    // 8MHz HSI / 72MHz HSE 微秒延时
    for (volatile uint32_t i = 0; i < us * 2; i++) {
        __NOP();
    }
}

void I2C_Bus_Init(void) {
    // CubeMX 已经在 MX_GPIO_Init 中配置了 PB6/PB7 开漏上拉
    I2C_SCL_1();
    I2C_SDA_1();
}

void I2C_Start(void) {
    I2C_SDA_1(); I2C_SCL_1(); Delay_us(3);
    I2C_SDA_0(); Delay_us(3); I2C_SCL_0();
}

void I2C_Stop(void) {
    I2C_SCL_0(); I2C_SDA_0(); Delay_us(3);
    I2C_SCL_1(); Delay_us(3); I2C_SDA_1(); Delay_us(3);
}

// 等待从机应答（重点：加入 Timeout 计数保护）
uint8_t I2C_WaitAck(void) {
    uint16_t timeout = 0;
    I2C_SDA_1(); // 释放 SDA 线由外部或从机控制
    Delay_us(2);
    I2C_SCL_1();
    Delay_us(2);
    
    // 从机拉低 SDA 代表 ACK，保持高代表 NACK
    while (I2C_SDA_READ()) {
        timeout++;
        if (timeout > 500) {
            I2C_Stop();
            return 1; // 超时未应答，返回失败
        }
    }
    I2C_SCL_0();
    return 0; // 成功收到 ACK
}

void I2C_Ack(void) {
    I2C_SCL_0(); I2C_SDA_0(); Delay_us(2);
    I2C_SCL_1(); Delay_us(2); I2C_SCL_0();
}

void I2C_NAck(void) {
    I2C_SCL_0(); I2C_SDA_1(); Delay_us(2);
    I2C_SCL_1(); Delay_us(2); I2C_SCL_0();
}

void I2C_SendByte(uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        if (byte & 0x80) I2C_SDA_1();
        else I2C_SDA_0();
        Delay_us(2);
        I2C_SCL_1();
        Delay_us(2);
        I2C_SCL_0();
        byte <<= 1;
    }
}

uint8_t I2C_ReadByte(uint8_t ack) {
    uint8_t byte = 0;
    I2C_SDA_1(); // 释放总线
    for (uint8_t i = 0; i < 8; i++) {
        I2C_SCL_1();
        Delay_us(2);
        byte <<= 1;
        if (I2C_SDA_READ()) byte |= 0x01;
        I2C_SCL_0();
        Delay_us(2);
    }
    if (ack) I2C_Ack();
    else I2C_NAck();
    return byte;
}

// 探测指定地址是否有设备响应 (0: 存在, 1: 不存在)
uint8_t I2C_CheckDevice(uint8_t dev_addr) {
    I2C_Start();
    I2C_SendByte(dev_addr << 1); // 7位地址左移 + 写位(0)
    uint8_t ack = I2C_WaitAck();
    I2C_Stop();
    return ack;
}
