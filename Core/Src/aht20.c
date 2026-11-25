#include "aht20.h"
#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

#define AHT20_ADDR (0x38 << 1) /* 7位地址 0x38 左移1位适配 HAL 接口 */

static uint8_t s_aht20_ready = 0;

static void aht20_delay_ms(uint32_t ms)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
    else
    {
        HAL_Delay(ms);
    }
}

uint8_t AHT20_IsReady(void)
{
    return s_aht20_ready;
}

AHT20_Status_t AHT20_Init(void)
{
    uint8_t read_status = 0;
    s_aht20_ready = 0;

    aht20_delay_ms(40); /* 上电等待 40ms 就绪 */

    if (HAL_I2C_Master_Receive(&hi2c1, AHT20_ADDR | 0x01, &read_status, 1, 50) != HAL_OK)
    {
        return AHT20_ERR_BUS_BUSY;
    }

    /* 检查 bit[3] 校准使能位，未校准时发送 0xBE 初始化指令 */
    if ((read_status & 0x08U) == 0U)
    {
        uint8_t init_cmd[3] = {0xBE, 0x08, 0x00};
        if (HAL_I2C_Master_Transmit(&hi2c1, AHT20_ADDR, init_cmd, 3, 50) != HAL_OK)
        {
            return AHT20_ERR_BUS_BUSY;
        }
        aht20_delay_ms(10);
    }

    s_aht20_ready = 1;
    return AHT20_OK;
}

AHT20_Status_t AHT20_Read(float *temperature, float *humidity)
{
    uint8_t trigger_cmd[3] = {0xAC, 0x33, 0x00};
    uint8_t raw_data[6] = {0};

    if (temperature == NULL || humidity == NULL)
    {
        return AHT20_ERR_DATA;
    }

    if (HAL_I2C_Master_Transmit(&hi2c1, AHT20_ADDR, trigger_cmd, 3, 50) != HAL_OK)
    {
        s_aht20_ready = 0;
        return AHT20_ERR_BUS_BUSY;
    }

    /* 等待测量转换完成 (约 80ms，RTOS 运行态自动让出 CPU) */
    aht20_delay_ms(80);

    if (HAL_I2C_Master_Receive(&hi2c1, AHT20_ADDR | 0x01, raw_data, 6, 50) != HAL_OK)
    {
        s_aht20_ready = 0;
        return AHT20_ERR_BUS_BUSY;
    }

    /* bit[7] = 1 表示芯片仍处于忙状态 */
    if ((raw_data[0] & 0x80U) != 0U)
    {
        return AHT20_ERR_TIMEOUT;
    }

    uint32_t hum_raw  = (((uint32_t)raw_data[1] << 16) | ((uint32_t)raw_data[2] << 8) | raw_data[3]) >> 4;
    uint32_t temp_raw = (((uint32_t)(raw_data[3] & 0x0FU) << 16) | ((uint32_t)raw_data[4] << 8) | raw_data[5]);

    *humidity    = ((float)hum_raw * 100.0f) / 1048576.0f;
    *temperature = (((float)temp_raw * 200.0f) / 1048576.0f) - 50.0f;

    s_aht20_ready = 1;
    return AHT20_OK;
}
