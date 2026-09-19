#include "app_tasks.h"
#include "app_data.h"
#include "aht20.h"
#include "bmp280.h"
#include "oled.h"
#include "protocol.h"
#include <stdio.h>

static QueueHandle_t q_oled_data = NULL;
static QueueHandle_t q_comm_data = NULL;

/* ================= 任务 1: 传感器采集任务 (生产者) ================= */
static void Task_Sensor(void *pvParameters)
{
    EnvData_t sensor_env;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000);

    // 硬件双传感器初始化
    AHT20_Init();
    BMP280_Init();

    for (;;)
    {
        float dummy_temp = 0.0f;
        float raw_press_pa = 0.0f;

        // 1. 采集 AHT20 温湿度
        AHT20_ReadData(&sensor_env.temperature, &sensor_env.humidity);

        // 2. 采集 BMP280 气压与海拔
        if (BMP280_ReadData(&dummy_temp, &raw_press_pa) == 0) {
            sensor_env.pressure = raw_press_pa / 100.0f; // 转换为 hPa (百帕)
            sensor_env.altitude = BMP280_CalcAltitude(raw_press_pa);
        }

        sensor_env.timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // 3. 将融合后的环境全集推入队列 (覆盖旧值)
        xQueueOverwrite(q_oled_data, &sensor_env);
        xQueueOverwrite(q_comm_data, &sensor_env);

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/* ================= 任务 2: OLED 界面刷新任务 (四行全显) ================= */
static void Task_OLED(void *pvParameters)
{
    EnvData_t oled_env;
    char str_buf[20];

    OLED_Init();

    for (;;)
    {
        if (xQueueReceive(q_oled_data, &oled_env, pdMS_TO_TICKS(500)) == pdPASS)
        {
            OLED_Clear();
            
            // 第 0 页: 温湿度 (拼接为紧凑格式)
            sprintf(str_buf, "T:%.1fC H:%.1f%%", oled_env.temperature, oled_env.humidity);
            OLED_ShowString(0, 0, str_buf, 16);

            // 第 2 页: 大气压强 (hPa)
            sprintf(str_buf, "P: %.1fhPa", oled_env.pressure);
            OLED_ShowString(0, 2, str_buf, 16);

            // 第 4 页: 估算海拔 (Alt: xx.xm)
            sprintf(str_buf, "Alt: %.1fm", oled_env.altitude);
            OLED_ShowString(0, 4, str_buf, 16);

            // 第 6 页: 系统状态
            OLED_ShowString(0, 6, "RTOS Bus: OK", 16);

            OLED_Update();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ================= 任务 3: 串口通信封包任务 (消费者 2) ================= */
static void Task_Comm(void *pvParameters)
{
    EnvData_t comm_env;

    for (;;)
    {
        // 挂起等待传感器产生数据 (portMAX_DELAY: 无限阻塞，不耗费任何 CPU)
        if (xQueueReceive(q_comm_data, &comm_env, portMAX_DELAY) == pdPASS)
        {
            // 按照 v2.0 定义的 9 字节协议帧发送给 PC
            Protocol_SendEnvData(comm_env.temperature, comm_env.humidity);
        }
    }
}

/* ================= 统一任务与队列初始化接口 ================= */
void App_Tasks_Init(void)
{
    // 1. 创建队列 (长度 1，保证始终保存最新帧)
    q_oled_data = xQueueCreate(1, sizeof(EnvData_t));
    q_comm_data = xQueueCreate(1, sizeof(EnvData_t));

    // 2. 创建任务 (栈大小单位为 Word，在 Cortex-M 上 128 Word = 512 Bytes)
    xTaskCreate(Task_Sensor, "Task_Sensor", 128, NULL, 3, NULL); // 优先级 3 (中)
    xTaskCreate(Task_OLED,   "Task_OLED",   256, NULL, 1, NULL); // 优先级 1 (低, OLED刷新吃栈深)
    xTaskCreate(Task_Comm,   "Task_Comm",   128, NULL, 4, NULL); // 优先级 4 (高, 通信优先)
}
