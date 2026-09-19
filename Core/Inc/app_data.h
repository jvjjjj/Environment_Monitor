#ifndef __APP_DATA_H
#define __APP_DATA_H

#include <stdint.h>

// 跨任务传输的标准环境数据载荷
typedef struct {
    float temperature;   // 摄氏度 (℃, 综合 AHT20)
    float humidity;      // 相对湿度 (%RH, 来自 AHT20)
    float pressure;      // 大气压强 (hPa 百帕, 来自 BMP280)
    float altitude;      // 估算海拔 (米, 来自 BMP280)
    uint32_t timestamp;  // 时间戳 (ms)
} EnvData_t;

#endif
