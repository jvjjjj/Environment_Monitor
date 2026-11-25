#include "bmp280.h"
#include "i2c.h"

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

static BMP280_Calib_t s_calib;
static int32_t s_t_fine = 0;

static uint8_t s_bmp280_addr    = BMP280_ADDR_L;
static uint8_t s_bmp280_chip_id = 0x00U;
static uint8_t s_bmp280_ready   = 0U;

static uint8_t bmp280_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *p_val)
{
    if (HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(dev_addr << 1), reg,
                         I2C_MEMADD_SIZE_8BIT, p_val, 1, 50) != HAL_OK)
    {
        return 1U;
    }
    return 0U;
}

static uint8_t bmp280_write_reg(uint8_t reg, uint8_t val)
{
    if (HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(s_bmp280_addr << 1), reg,
                          I2C_MEMADD_SIZE_8BIT, &val, 1, 50) != HAL_OK)
    {
        return 1U;
    }
    return 0U;
}

static uint8_t bmp280_read_multi(uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(s_bmp280_addr << 1), reg,
                         I2C_MEMADD_SIZE_8BIT, buf, len, 50) != HAL_OK)
    {
        return 1U;
    }
    return 0U;
}

uint8_t BMP280_GetAddr(void)    { return s_bmp280_addr; }
uint8_t BMP280_GetChipID(void)  { return s_bmp280_chip_id; }
uint8_t BMP280_IsReady(void)    { return s_bmp280_ready; }

uint8_t BMP280_Init(void)
{
    const uint8_t addr_list[2] = {BMP280_ADDR_L, BMP280_ADDR_H};
    uint8_t found = 0U;
    uint8_t id = 0U;

    s_bmp280_ready = 0U;

    /* 动态双地址探测 (0x76 / 0x77) 与兼容芯片 ID 识别 */
    for (uint8_t i = 0U; i < 2U; i++)
    {
        if (bmp280_read_reg(addr_list[i], BMP280_CHIP_ID_REG, &id) == 0U)
        {
            s_bmp280_chip_id = id;
            if (id == 0x58U || id == 0x60U || id == 0x56U || id == 0x57U)
            {
                s_bmp280_addr = addr_list[i];
                found = 1U;
                break;
            }
        }
    }

    if (!found)
    {
        return 1U;
    }

    uint8_t c_buf[24];
    if (bmp280_read_multi(0x88U, c_buf, 24U) != 0U)
    {
        return 1U;
    }

    s_calib.dig_T1 = (uint16_t)((uint16_t)c_buf[1] << 8)  | c_buf[0];
    s_calib.dig_T2 = (int16_t)(((uint16_t)c_buf[3] << 8)  | c_buf[2]);
    s_calib.dig_T3 = (int16_t)(((uint16_t)c_buf[5] << 8)  | c_buf[4]);
    s_calib.dig_P1 = (uint16_t)((uint16_t)c_buf[7] << 8)  | c_buf[6];
    s_calib.dig_P2 = (int16_t)(((uint16_t)c_buf[9] << 8)  | c_buf[8]);
    s_calib.dig_P3 = (int16_t)(((uint16_t)c_buf[11] << 8) | c_buf[10]);
    s_calib.dig_P4 = (int16_t)(((uint16_t)c_buf[13] << 8) | c_buf[12]);
    s_calib.dig_P5 = (int16_t)(((uint16_t)c_buf[15] << 8) | c_buf[14]);
    s_calib.dig_P6 = (int16_t)(((uint16_t)c_buf[17] << 8) | c_buf[16]);
    s_calib.dig_P7 = (int16_t)(((uint16_t)c_buf[19] << 8) | c_buf[18]);
    s_calib.dig_P8 = (int16_t)(((uint16_t)c_buf[21] << 8) | c_buf[20]);
    s_calib.dig_P9 = (int16_t)(((uint16_t)c_buf[23] << 8) | c_buf[22]);

    if (s_calib.dig_P1 == 0U)
    {
        return 1U;
    }

    /* 配置 0xF5: 待机 62.5ms, IIR 滤波系数 16 */
    if (bmp280_write_reg(BMP280_CONFIG_REG, (0x01U << 5) | (0x04U << 2)) != 0U)
    {
        return 1U;
    }

    /* 配置 0xF4: 温度 2x 过采样, 气压 16x 过采样, Normal 连续模式 */
    if (bmp280_write_reg(BMP280_CTRL_MEAS_REG, (0x02U << 5) | (0x05U << 2) | 0x03U) != 0U)
    {
        return 1U;
    }

    s_bmp280_ready = 1U;
    return 0U;
}

uint8_t BMP280_ReadData(float *temperature, float *pressure_pa)
{
    if (!s_bmp280_ready || temperature == NULL || pressure_pa == NULL)
    {
        return 1U;
    }

    uint8_t raw[6];
    if (bmp280_read_multi(BMP280_DATA_START_REG, raw, 6U) != 0U)
    {
        s_bmp280_ready = 0U;
        return 1U;
    }

    int32_t adc_P = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | ((int32_t)raw[2] >> 4);
    int32_t adc_T = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | ((int32_t)raw[5] >> 4);

    /* 1. 温度精密补偿 */
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)s_calib.dig_T1 << 1))) * ((int32_t)s_calib.dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)s_calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)s_calib.dig_T1))) >> 12) * ((int32_t)s_calib.dig_T3)) >> 14;
    s_t_fine = var1 + var2;
    *temperature = (float)((s_t_fine * 5 + 128) >> 8) / 100.0f;

    /* 2. Bosch 64-bit 气压补偿公式 */
    int64_t p_var1 = ((int64_t)s_t_fine) - 128000;
    int64_t p_var2 = p_var1 * p_var1 * (int64_t)s_calib.dig_P6;
    p_var2 = p_var2 + ((p_var1 * (int64_t)s_calib.dig_P5) << 17);
    p_var2 = p_var2 + (((int64_t)s_calib.dig_P4) << 35);
    p_var1 = ((p_var1 * p_var1 * (int64_t)s_calib.dig_P3) >> 8) + ((p_var1 * (int64_t)s_calib.dig_P2) << 12);
    p_var1 = (((((int64_t)1) << 47) + p_var1)) * ((int64_t)s_calib.dig_P1) >> 33;

    if (p_var1 == 0)
    {
        *pressure_pa = 0.0f;
        return 2U;
    }

    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - p_var2) * 3125) / p_var1;
    p_var1 = (((int64_t)s_calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    p_var2 = (((int64_t)s_calib.dig_P8) * p) >> 19;
    p = ((p + p_var1 + p_var2) >> 8) + (((int64_t)s_calib.dig_P7) << 4);

    *pressure_pa = (float)p / 256.0f;
    return 0U;
}

float BMP280_CalcAltitude(float pressure_pa)
{
    if (pressure_pa <= 0.0f)
    {
        return 0.0f;
    }
    /* 基于国际标准气压公式 h = 44330 * (1 - (P/P0)^0.190295) 的 3 阶泰勒展开：
     * 令 x = 1 - P/P0，在 0~3000m 范围内误差 < 0.3m，且免去 Cortex-M3 无 FPU 引入 powf() 带来的 ~4.5KB Flash 膨胀 */
    float x = 1.0f - (pressure_pa / 101325.0f);
    return 44330.0f * (0.190295f * x + 0.077041f * x * x + 0.046461f * x * x * x);
}
