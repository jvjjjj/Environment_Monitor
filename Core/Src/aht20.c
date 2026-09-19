#include "aht20.h"
#include "bsp_i2c.h"

extern void Delay_us(uint32_t us);

// 初始化 AHT20
uint8_t AHT20_Init(void) {
    HAL_Delay(40); // 上电等待稳定
    
    I2C_Start();
    I2C_SendByte((AHT20_ADDR << 1) | 0x01); // 读状态字
    if (I2C_WaitAck() != 0) {
        I2C_Stop();
        return 1; // 无应答
    }
    uint8_t status = I2C_ReadByte(0);
    I2C_Stop();

    // 检查 Bit[3] 校准位是否为 1
    if ((status & 0x08) == 0) {
        // 未校准，发送初始化寄存器序列
        I2C_Start();
        I2C_SendByte((AHT20_ADDR << 1) | 0x00);
        I2C_WaitAck();
        I2C_SendByte(0xBE);
        I2C_WaitAck();
        I2C_SendByte(0x08);
        I2C_WaitAck();
        I2C_SendByte(0x00);
        I2C_WaitAck();
        I2C_Stop();
        HAL_Delay(10);
    }
    return 0; // 初始化成功
}

// 采集并换算数据
uint8_t AHT20_ReadData(float *temperature, float *humidity) {
    uint8_t buf[6];
    uint32_t raw_hum = 0;
    uint32_t raw_temp = 0;

    // 1. 发送触发测量命令: 0xAC, 0x33, 0x00
    I2C_Start();
    I2C_SendByte((AHT20_ADDR << 1) | 0x00);
    if (I2C_WaitAck() != 0) { I2C_Stop(); return 1; }
    I2C_SendByte(0xAC); I2C_WaitAck();
    I2C_SendByte(0x33); I2C_WaitAck();
    I2C_SendByte(0x00); I2C_WaitAck();
    I2C_Stop();

    // 2. 必须等待测量完成 (芯片物理转换耗时约 75ms)
    HAL_Delay(80);

    // 3. 读取 6 字节测量数据
    I2C_Start();
    I2C_SendByte((AHT20_ADDR << 1) | 0x01);
    if (I2C_WaitAck() != 0) { I2C_Stop(); return 2; }
    
    // 检查状态字 Bit[7] 是否仍处于忙碌状态
    uint8_t status = I2C_ReadByte(1);
    if (status & 0x80) {
        I2C_Stop();
        return 3; // 依然忙碌，数据未完成
    }

    // 顺序读取后续 5 字节
    for (uint8_t i = 1; i < 6; i++) {
        buf[i] = I2C_ReadByte(i < 5 ? 1 : 0); // 最后一个字节给 NACK
    }
    I2C_Stop();

    // 4. 20 位原始数据拼接
    // 湿度: buf[1] (8位) + buf[2] (8位) + buf[3] 高4位
    raw_hum = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | ((uint32_t)buf[3] >> 4);
    // 温度: buf[3] 低4位 + buf[4] (8位) + buf[5] (8位)
    raw_temp = (((uint32_t)buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | (uint32_t)buf[5];

    // 5. 按照手册公式转换为物理值
    *humidity = ((float)raw_hum / 1048576.0f) * 100.0f;           // 2^20 = 1048576
    *temperature = ((float)raw_temp / 1048576.0f) * 200.0f - 50.0f;

    return 0; // 读取成功
}
