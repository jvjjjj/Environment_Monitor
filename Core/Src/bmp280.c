#include "bmp280.h"
#include "bsp_i2c.h"
#include <math.h>

// 官方出厂校准参数结构体
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
} BMP280_Calib_t;

static BMP280_Calib_t calib;
static int32_t t_fine; // 温度精密补偿中间变量，气压换算强制依赖它

/* --- 底层寄存器读写辅助函数 --- */
static uint8_t BMP280_ReadReg(uint8_t reg) {
    uint8_t val = 0;
    I2C_Start();
    I2C_SendByte((BMP280_ADDR << 1) | 0x00);
    I2C_WaitAck();
    I2C_SendByte(reg);
    I2C_WaitAck();
    
    I2C_Start();
    I2C_SendByte((BMP280_ADDR << 1) | 0x01);
    I2C_WaitAck();
    val = I2C_ReadByte(0);
    I2C_Stop();
    return val;
}

static void BMP280_WriteReg(uint8_t reg, uint8_t val) {
    I2C_Start();
    I2C_SendByte((BMP280_ADDR << 1) | 0x00);
    I2C_WaitAck();
    I2C_SendByte(reg);
    I2C_WaitAck();
    I2C_SendByte(val);
    I2C_WaitAck();
    I2C_Stop();
}

static void BMP280_ReadMulti(uint8_t reg, uint8_t *buf, uint8_t len) {
    I2C_Start();
    I2C_SendByte((BMP280_ADDR << 1) | 0x00);
    I2C_WaitAck();
    I2C_SendByte(reg);
    I2C_WaitAck();

    I2C_Start();
    I2C_SendByte((BMP280_ADDR << 1) | 0x01);
    I2C_WaitAck();
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = I2C_ReadByte(i < (len - 1) ? 1 : 0);
    }
    I2C_Stop();
}

/* --- 初始化与校准参数载入 --- */
uint8_t BMP280_Init(void) {
    uint8_t id = BMP280_ReadReg(BMP280_CHIP_ID_REG);
    if (id != 0x58) {
        return 1; // 芯片 ID 校验失败 (器件离线或地址有误)
    }

    // 一次性从 0x88 读出 24 字节出厂标定参数
    uint8_t c_buf[24];
    BMP280_ReadMulti(0x88, c_buf, 24);

    // 小端模式重组校准多项式系数
    calib.dig_T1 = (uint16_t)(c_buf[1] << 8) | c_buf[0];
    calib.dig_T2 = (int16_t)(c_buf[3] << 8)  | c_buf[2];
    calib.dig_T3 = (int16_t)(c_buf[5] << 8)  | c_buf[4];
    calib.dig_P1 = (uint16_t)(c_buf[7] << 8) | c_buf[6];
    calib.dig_P2 = (int16_t)(c_buf[9] << 8)  | c_buf[8];
    calib.dig_P3 = (int16_t)(c_buf[11] << 8) | c_buf[10];
    calib.dig_P4 = (int16_t)(c_buf[13] << 8) | c_buf[12];
    calib.dig_P5 = (int16_t)(c_buf[15] << 8) | c_buf[14];
    calib.dig_P6 = (int16_t)(c_buf[17] << 8) | c_buf[16];
    calib.dig_P7 = (int16_t)(c_buf[19] << 8) | c_buf[18];
    calib.dig_P8 = (int16_t)(c_buf[21] << 8) | c_buf[20];
    calib.dig_P9 = (int16_t)(c_buf[23] << 8) | c_buf[22];

    // 配置寄存器 0xF5: 待机 62.5ms, IIR 滤波 16x
    BMP280_WriteReg(BMP280_CONFIG_REG, (0x01 << 5) | (0x04 << 2));

    // 配置寄存器 0xF4: 温度 2x, 气压 16x 过采样, Normal 模式
    BMP280_WriteReg(BMP280_CTRL_MEAS_REG, (0x02 << 5) | (0x05 << 2) | 0x03);

    return 0;
}

/* --- 数据采集与官方补偿算法 --- */
uint8_t BMP280_ReadData(float *temperature, float *pressure) {
    uint8_t raw[6];
    BMP280_ReadMulti(BMP280_DATA_START_REG, raw, 6);

    int32_t adc_P = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | ((int32_t)raw[2] >> 4);
    int32_t adc_T = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | ((int32_t)raw[5] >> 4);

    // 1. 温度补偿算法 (计算 t_fine，必须优先计算)
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)calib.dig_T1 << 1))) * ((int32_t)calib.dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)calib.dig_T1))) >> 12) * ((int32_t)calib.dig_T3)) >> 14;
    t_fine = var1 + var2;
    *temperature = (float)((t_fine * 5 + 128) >> 8) / 100.0f;

    // 2. 64-bit 气压补偿算法 (直接引用博世官方手册多项式)
    int64_t p_var1 = ((int64_t)t_fine) - 128000;
    int64_t p_var2 = p_var1 * p_var1 * (int64_t)calib.dig_P6;
    p_var2 = p_var2 + ((p_var1 * (int64_t)calib.dig_P5) << 17);
    p_var2 = p_var2 + (((int64_t)calib.dig_P4) << 35);
    p_var1 = ((p_var1 * p_var1 * (int64_t)calib.dig_P3) >> 8) + ((p_var1 * (int64_t)calib.dig_P2) << 12);
    p_var1 = (((((int64_t)1) << 47) + p_var1)) * ((int64_t)calib.dig_P1) >> 33;

    if (p_var1 == 0) {
        *pressure = 0.0f; // 避免除零异常
        return 2;
    }

    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - p_var2) * 3125) / p_var1;
    p_var1 = (((int64_t)calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    p_var2 = (((int64_t)calib.dig_P8) * p) >> 19;
    p = ((p + p_var1 + p_var2) >> 8) + (((int64_t)calib.dig_P7) << 4);

    *pressure = (float)p / 256.0f; // 得到真实 Pa (帕斯卡)

    return 0;
}

// 国际标准大气压公式估算相对海拔高度
float BMP280_CalcAltitude(float pressure) {
    // P0 = 101325 Pa (标准海平面气压)
    return 44330.0f * (1.0f - powf(pressure / 101325.0f, 0.190295f));
}
