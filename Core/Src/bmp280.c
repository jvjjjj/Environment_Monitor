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
static int32_t t_fine = 0; // 温度精密补偿中间变量，气压换算强制依赖它

static uint8_t s_bmp280_addr = BMP280_ADDR_L; // 动态探测到的有效地址
static uint8_t s_bmp280_chip_id = 0x00;        // 读到的芯片 ID
static uint8_t s_bmp280_ready = 0;            // 传感器是否初始化成功

/* --- 底层寄存器读写辅助函数 --- */
static uint8_t BMP280_ReadRegAddr(uint8_t dev_addr, uint8_t reg, uint8_t *p_val) {
    I2C_Start();
    I2C_SendByte((dev_addr << 1) | 0x00);
    if (I2C_WaitAck() != 0) {
        I2C_Stop();
        return 1;
    }
    I2C_SendByte(reg);
    if (I2C_WaitAck() != 0) {
        I2C_Stop();
        return 1;
    }
    
    I2C_Start();
    I2C_SendByte((dev_addr << 1) | 0x01);
    if (I2C_WaitAck() != 0) {
        I2C_Stop();
        return 1;
    }
    *p_val = I2C_ReadByte(0);
    I2C_Stop();
    return 0;
}

static uint8_t BMP280_WriteReg(uint8_t reg, uint8_t val) {
    I2C_Start();
    I2C_SendByte((s_bmp280_addr << 1) | 0x00);
    if (I2C_WaitAck() != 0) {
        I2C_Stop();
        return 1;
    }
    I2C_SendByte(reg);
    if (I2C_WaitAck() != 0) {
        I2C_Stop();
        return 1;
    }
    I2C_SendByte(val);
    if (I2C_WaitAck() != 0) {
        I2C_Stop();
        return 1;
    }
    I2C_Stop();
    return 0;
}

static uint8_t BMP280_ReadMulti(uint8_t reg, uint8_t *buf, uint8_t len) {
    I2C_Start();
    I2C_SendByte((s_bmp280_addr << 1) | 0x00);
    if (I2C_WaitAck() != 0) {
        I2C_Stop();
        return 1;
    }
    I2C_SendByte(reg);
    if (I2C_WaitAck() != 0) {
        I2C_Stop();
        return 1;
    }

    I2C_Start();
    I2C_SendByte((s_bmp280_addr << 1) | 0x01);
    if (I2C_WaitAck() != 0) {
        I2C_Stop();
        return 1;
    }
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = I2C_ReadByte(i < (len - 1) ? 1 : 0);
    }
    I2C_Stop();
    return 0;
}

/* --- 诊断接口 --- */
uint8_t BMP280_GetAddr(void) {
    return s_bmp280_addr;
}

uint8_t BMP280_GetChipID(void) {
    return s_bmp280_chip_id;
}

uint8_t BMP280_IsReady(void) {
    return s_bmp280_ready;
}

/* --- 初始化与校准参数载入 --- */
uint8_t BMP280_Init(void) {
    s_bmp280_ready = 0;

    // 探测候选地址: 0x76 与 0x77
    const uint8_t addr_list[2] = {BMP280_ADDR_L, BMP280_ADDR_H};
    uint8_t found = 0;
    uint8_t id = 0;

    for (uint8_t i = 0; i < 2; i++) {
        if (BMP280_ReadRegAddr(addr_list[i], BMP280_CHIP_ID_REG, &id) == 0) {
            s_bmp280_chip_id = id;
            // 兼容 BMP280(0x58), BME280(0x60), 以及常见兼容替代芯片(0x56, 0x57)
            if (id == 0x58 || id == 0x60 || id == 0x56 || id == 0x57) {
                s_bmp280_addr = addr_list[i];
                found = 1;
                break;
            }
        }
    }

    if (!found) {
        return 1; // 未探测到可用芯片
    }

    // 从 0x88 一次性连续读出 24 字节出厂标定多项式参数
    uint8_t c_buf[24];
    if (BMP280_ReadMulti(0x88, c_buf, 24) != 0) {
        return 1;
    }

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

    if (calib.dig_P1 == 0) {
        return 1; // 校准参数异常
    }

    // 配置寄存器 0xF5: 待机时间 62.5ms, IIR 滤波系数 16
    if (BMP280_WriteReg(BMP280_CONFIG_REG, (0x01 << 5) | (0x04 << 2)) != 0) {
        return 1;
    }

    // 配置寄存器 0xF4: 温度 2x 过采样, 气压 16x 过采样, Normal 连续工作模式
    if (BMP280_WriteReg(BMP280_CTRL_MEAS_REG, (0x02 << 5) | (0x05 << 2) | 0x03) != 0) {
        return 1;
    }

    s_bmp280_ready = 1;
    return 0;
}

/* --- 数据采集与博世官方补偿算法 --- */
uint8_t BMP280_ReadData(float *temperature, float *pressure) {
    if (!s_bmp280_ready) {
        return 1;
    }

    uint8_t raw[6];
    if (BMP280_ReadMulti(BMP280_DATA_START_REG, raw, 6) != 0) {
        return 1;
    }

    int32_t adc_P = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | ((int32_t)raw[2] >> 4);
    int32_t adc_T = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | ((int32_t)raw[5] >> 4);

    // 1. 温度补偿算法 (计算 t_fine，气压多项式计算强依赖此变量)
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)calib.dig_T1 << 1))) * ((int32_t)calib.dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)calib.dig_T1))) >> 12) * ((int32_t)calib.dig_T3)) >> 14;
    t_fine = var1 + var2;
    *temperature = (float)((t_fine * 5 + 128) >> 8) / 100.0f;

    // 2. 64-bit 气压高精度补偿算法 (Bosch Datasheet 公式)
    int64_t p_var1 = ((int64_t)t_fine) - 128000;
    int64_t p_var2 = p_var1 * p_var1 * (int64_t)calib.dig_P6;
    p_var2 = p_var2 + ((p_var1 * (int64_t)calib.dig_P5) << 17);
    p_var2 = p_var2 + (((int64_t)calib.dig_P4) << 35);
    p_var1 = ((p_var1 * p_var1 * (int64_t)calib.dig_P3) >> 8) + ((p_var1 * (int64_t)calib.dig_P2) << 12);
    p_var1 = (((((int64_t)1) << 47) + p_var1)) * ((int64_t)calib.dig_P1) >> 33;

    if (p_var1 == 0) {
        *pressure = 0.0f; // 避免除零错误
        return 2;
    }

    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - p_var2) * 3125) / p_var1;
    p_var1 = (((int64_t)calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    p_var2 = (((int64_t)calib.dig_P8) * p) >> 19;
    p = ((p + p_var1 + p_var2) >> 8) + (((int64_t)calib.dig_P7) << 4);

    *pressure = (float)p / 256.0f; // 换算为标准国际单位 Pa (帕斯卡)

    return 0;
}

// 国际标准大气压公式估算相对海拔高度 (单位: 米)
float BMP280_CalcAltitude(float pressure) {
    if (pressure <= 0.0f) {
        return 0.0f;
    }
    // P0 = 101325 Pa (海平面标准大气压)
    return 44330.0f * (1.0f - powf(pressure / 101325.0f, 0.190295f));
}
