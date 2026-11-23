#ifndef __VIRTUAL_SENSOR_H__
#define __VIRTUAL_SENSOR_H__

#include <stdint.h>
#include <stdbool.h>

/**
  * @brief 虚拟传感器数据结构体
  */
typedef struct {
    float temperature;   // 当前温度 (°C)
    float humidity;      // 当前湿度 (%)
    bool  manual_mode;   // 模式 (false: 自动波动模式, true: 串口手动固定模式)
    bool  temp_alarm;    // 温度超标报警标志 (true: 报警)
} VirtualSensor_t;

/* 初始化与周期更新 */
void VirtualSensor_Init(void);
void VirtualSensor_Update(void);
VirtualSensor_t VirtualSensor_Get(void);

/* 人工篡改与控制接口 */
void VirtualSensor_SetTemp(float temp);
void VirtualSensor_SetHumi(float humi);
void VirtualSensor_SetAutoMode(void);

/* 串口交互命令行(CLI)轮询处理函数 */
void VirtualSensor_CLI_Process(void);

#endif /* __VIRTUAL_SENSOR_H__ */
