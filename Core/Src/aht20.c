#include "aht20.h"
#include "i2c.h"
#include <stdio.h>

#define AHT20_ADDR (0x38 << 1) // 7????? 0x38,??1???????

AHT20_Status_t AHT20_Init(void)
{
    uint8_t read_status = 0;
    
    HAL_Delay(40); // ?????? 40ms ????
    
    // 1. ?????,????????????
    if (HAL_I2C_Master_Receive(&hi2c1, AHT20_ADDR | 0x01, &read_status, 1, 100) != HAL_OK) {
        printf("[AHT20] Init Fail: No I2C ACK!\r\n");
        return AHT20_ERR_BUS_BUSY;
    }
    
    // ?? bit[3] ??? 1 (1 ?????)
    if ((read_status & 0x08) == 0) {
        // ???,???????????? 0xBE
        uint8_t init_cmd[3] = {0xBE, 0x08, 0x00};
        HAL_I2C_Master_Transmit(&hi2c1, AHT20_ADDR, init_cmd, 3, 100);
        HAL_Delay(10);
    }
    
    printf("[AHT20] Init OK. Status Byte: 0x%02X\r\n", read_status);
    return AHT20_OK;
}

AHT20_Status_t AHT20_Read(float *temperature, float *humidity)
{
    uint8_t trigger_cmd[3] = {0xAC, 0x33, 0x00}; // ??????
    uint8_t raw_data[6] = {0};
    
    // 1. ????????
    if (HAL_I2C_Master_Transmit(&hi2c1, AHT20_ADDR, trigger_cmd, 3, 100) != HAL_OK) {
        return AHT20_ERR_BUS_BUSY;
    }
    
    // 2. ??????????? (????????? 80ms)
    HAL_Delay(80);
    
    // 3. ?? 6 ????????
    if (HAL_I2C_Master_Receive(&hi2c1, AHT20_ADDR | 0x01, raw_data, 6, 100) != HAL_OK) {
        return AHT20_ERR_BUS_BUSY;
    }
    
    // 4. ??? 0 ?????? (bit 7: 1????????,0??????)
    if ((raw_data[0] & 0x80) != 0) {
        return AHT20_ERR_TIMEOUT;
    }
    
    // 5. ???? 20 ??? (?????????)
    uint32_t hum_raw = (((uint32_t)raw_data[1] << 16) | ((uint32_t)raw_data[2] << 8) | raw_data[3]) >> 4;
    uint32_t temp_raw = (((uint32_t)(raw_data[3] & 0x0F) << 16) | ((uint32_t)raw_data[4] << 8) | raw_data[5]);
    
    // 6. ?????????
    *humidity = ((float)hum_raw * 100.0f) / 1048576.0f;           // 1048576 = 2^20
    *temperature = (((float)temp_raw * 200.0f) / 1048576.0f) - 50.0f;
    
    return AHT20_OK;
}