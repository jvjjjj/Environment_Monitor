/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : FreeRTOS v4.0 物联网多任务协同管理 (v3 + v4 全栈融合版)
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "i2c.h"
#include "aht20.h"
#include "bmp280.h"
#include "virtual_sensor.h"
#include "env_analyzer.h"
#include "debug_console.h"
#include "protocol.h"
#include "oled.h"
#include "esp8266.h"
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define IOT_WIFI_SSID           "Virtual_WiFi"
#define IOT_WIFI_PASS           "12345678"
#define IOT_MQTT_BROKER         "broker.emqx.io"
#define IOT_MQTT_PORT           1883U
#define IOT_MQTT_CLIENT_ID      "STM32_EnvMonitor_v4"
#define IOT_MQTT_PUB_TOPIC      "sensor/env_monitor_25087"

#define IOT_BACKOFF_MIN_MS      1000U
#define IOT_BACKOFF_MAX_MS      30000U
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
/* 深度为 1 的最新帧共享队列：
 * 生产者 Task_Sensor 使用 xQueueOverwrite 覆盖写入；
 * 消费者 Task_OLED / Task_Comm / Task_IoT 均使用 xQueuePeek 非破坏性读取。 */
static QueueHandle_t q_env_data = NULL;

osThreadId Task_SensorHandle;
osThreadId Task_OLEDHandle;
osThreadId Task_CommHandle;
osThreadId Task_IoTHandle;
/* USER CODE END Variables */

osThreadId defaultTaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartSensorTask(void const * argument);
void StartOledTask(void const * argument);
void StartCommTask(void const * argument);
void StartIoTTask(void const * argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize);

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t  xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer   = &xIdleTaskTCBBuffer;
    *ppxIdleTaskStackBuffer = &xIdleStack[0];
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

void MX_FREERTOS_Init(void)
{
    /* USER CODE BEGIN RTOS_QUEUES */
    q_env_data = xQueueCreate(1U, sizeof(EnvData_t));
    /* USER CODE END RTOS_QUEUES */

    osThreadDef(defaultTask, StartDefaultTask, osPriorityIdle, 0, 128);
    defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

    /* USER CODE BEGIN RTOS_THREADS */
    /* 1. 双传感器融合采集任务 (1000ms 严格周期, 优先级 AboveNormal) */
    osThreadDef(Task_Sensor, StartSensorTask, osPriorityAboveNormal, 0, 256);
    Task_SensorHandle = osThreadCreate(osThread(Task_Sensor), NULL);

    /* 2. OLED 双模显示刷新任务 (200ms 周期, 优先级 BelowNormal) */
    osThreadDef(Task_OLED, StartOledTask, osPriorityBelowNormal, 0, 256);
    Task_OLEDHandle = osThreadCreate(osThread(Task_OLED), NULL);

    /* 3. 上位机 9 字节二进制协议 + CLI 调试台 + 心跳任务 (优先级 High) */
    osThreadDef(Task_Comm, StartCommTask, osPriorityHigh, 0, 256);
    Task_CommHandle = osThreadCreate(osThread(Task_Comm), NULL);

    /* 4. 物联网 ESP8266 非阻塞状态机与 MQTT 上云任务 (3000ms 周期, 优先级 Normal) */
    osThreadDef(Task_IoT, StartIoTTask, osPriorityNormal, 0, 320);
    Task_IoTHandle = osThreadCreate(osThread(Task_IoT), NULL);
    /* USER CODE END RTOS_THREADS */
}

void StartDefaultTask(void const * argument)
{
    (void)argument;
    for (;;)
    {
        osDelay(500);
    }
}

/* ==================== 任务 1: 传感器融合采集任务 (1000ms) ==================== */
void StartSensorTask(void const * argument)
{
    (void)argument;
    EnvData_t env;
    memset(&env, 0, sizeof(env));
    env.pressure = 1013.2f;
    env.altitude = 0.0f;

    uint8_t i2c_err_cnt = 0U;
    VirtualSensor_Init();

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000);

    for (;;)
    {
        VirtualSensor_Update();
        VirtualSensor_t vdata = VirtualSensor_Get();

#if (ENV_FORCE_VIRTUAL_SENSOR == 1)
        env.temperature = vdata.temperature;
        env.humidity    = vdata.humidity;
        env.pressure    = 1013.2f;
        env.altitude    = BMP280_CalcAltitude(env.pressure * 100.0f);
        env.bmp_ready   = 0U;
#else
        /* 若通过 CLI 进入手动故障注入模式，优先响应手动篡改值 */
        if (vdata.manual_mode)
        {
            env.temperature = vdata.temperature;
            env.humidity    = vdata.humidity;
        }
        else
        {
            float t = 0.0f, h = 0.0f;
            if (!AHT20_IsReady())
            {
                (void)AHT20_Init();
            }

            if (AHT20_Read(&t, &h) == AHT20_OK)
            {
                env.temperature = t;
                env.humidity    = h;
                i2c_err_cnt     = 0U;
            }
            else
            {
                /* 硬件缺失或总线故障时优雅降级虚拟传感器桩，并触发 9 脉冲总线自愈 */
                env.temperature = vdata.temperature;
                env.humidity    = vdata.humidity;
                if (++i2c_err_cnt >= 3U)
                {
                    I2C1_BusRecovery_9Pulse();
                    i2c_err_cnt = 0U;
                }
            }
        }

        /* BMP280 双地址探测与离线自愈 */
        if (!BMP280_IsReady())
        {
            (void)BMP280_Init();
        }

        float bmp_temp = 0.0f;
        float raw_press_pa = 0.0f;
        if (BMP280_ReadData(&bmp_temp, &raw_press_pa) == 0U)
        {
            env.pressure  = raw_press_pa / 100.0f;
            env.altitude  = BMP280_CalcAltitude(raw_press_pa);
            env.bmp_ready = 1U;
            env.bmp_addr  = BMP280_GetAddr();
            env.bmp_id    = BMP280_GetChipID();
        }
        else
        {
            env.bmp_ready = 0U;
        }
#endif

        env.timestamp = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        /* 写入滑动窗口统计分析器 (供 CLI `stat` / `hist` 命令查询) */
        EnvAnalyzer_Push(env.temperature, env.humidity);

        /* 覆盖写入最新帧队列，所有消费者通过 xQueuePeek 无损共享 */
        if (q_env_data != NULL)
        {
            (void)xQueueOverwrite(q_env_data, &env);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/* ==================== 任务 2: OLED 双模显示任务 (200ms) ==================== */
void StartOledTask(void const * argument)
{
    (void)argument;
    EnvData_t oled_env;
    char disp_buf[24];
    memset(&oled_env, 0, sizeof(oled_env));

    vTaskDelay(pdMS_TO_TICKS(200));

    for (;;)
    {
        OLED_ShowString(0, 0, "== ENV MONITOR ==");

        if (q_env_data != NULL && xQueuePeek(q_env_data, &oled_env, pdMS_TO_TICKS(50)) == pdPASS)
        {
            snprintf(disp_buf, sizeof(disp_buf), "T:%4.1fC H:%4.1f%%",
                     oled_env.temperature, oled_env.humidity);
            OLED_ShowString(0, 16, disp_buf);

            if (oled_env.bmp_ready)
            {
                snprintf(disp_buf, sizeof(disp_buf), "P:%6.1f A:%3.0fm",
                         oled_env.pressure, oled_env.altitude);
            }
            else
            {
                snprintf(disp_buf, sizeof(disp_buf), "P:1013.2 [SIM] ");
            }
            OLED_ShowString(0, 32, disp_buf);
        }
        else
        {
            OLED_ShowString(0, 16, "T:--.-C H:--.-%");
            OLED_ShowString(0, 32, "P:----.- hPa   ");
        }

        switch (g_net_state)
        {
            case NET_STATE_AT_TEST:
                OLED_ShowString(0, 48, "NET: WAIT AT... ");
                break;
            case NET_STATE_WIFI_CONNECT:
                OLED_ShowString(0, 48, "NET: WIFI CON...");
                break;
            case NET_STATE_MQTT_CONNECT:
                OLED_ShowString(0, 48, "NET: MQTT CON...");
                break;
            case NET_STATE_ONLINE:
                OLED_ShowString(0, 48, "NET: MQTT ONLINE");
                break;
            case NET_STATE_FAIL:
                OLED_ShowString(0, 48, "NET: RETRYING...");
                break;
            default:
                OLED_ShowString(0, 48, "NET: BOOTING... ");
                break;
        }

        OLED_UpdateScreen();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* ==================== 任务 3: 串口协议上传 + CLI 调试台 (1000ms) ==================== */
void StartCommTask(void const * argument)
{
    (void)argument;
    EnvData_t comm_env;
    uint8_t sub_ticks = 0U;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xStep = pdMS_TO_TICKS(50); /* 50ms 轮询 CLI，20 次 = 1000ms 上传一帧 */

    for (;;)
    {
        /* 响应上位机/串口助手 CLI 调试指令 (temp / hum / auto / stat / hist / proto) */
        DebugConsole_Process();

        if (++sub_ticks >= 20U)
        {
            sub_ticks = 0U;
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);

            if (q_env_data != NULL && xQueuePeek(q_env_data, &comm_env, 0) == pdPASS)
            {
                /* 按 v2.0 规范发送 9 字节定长二进制协议帧至 PC 上位机 */
                Protocol_SendEnvData(comm_env.temperature, comm_env.humidity);
            }
        }

        vTaskDelayUntil(&xLastWakeTime, xStep);
    }
}

/* ==================== 任务 4: 物联网非阻塞状态机与 MQTT 上云 (3000ms) ==================== */
void StartIoTTask(void const * argument)
{
    (void)argument;
    EnvData_t iot_env;
    uint32_t backoff_ms = IOT_BACKOFF_MIN_MS;
    uint8_t  pub_fail_cnt = 0U;

    vTaskDelay(pdMS_TO_TICKS(1000));

    for (;;)
    {
        /* 阶段 1: AT 握手与回显关闭 (ATE0) */
        g_net_state = NET_STATE_AT_TEST;
        if (!ESP8266_TestAT())
        {
            g_net_state = NET_STATE_FAIL;
            vTaskDelay(pdMS_TO_TICKS(backoff_ms));
            backoff_ms = (backoff_ms * 2U > IOT_BACKOFF_MAX_MS) ? IOT_BACKOFF_MAX_MS : (backoff_ms * 2U);
            continue;
        }

        /* 阶段 2: 连接 WiFi */
        g_net_state = NET_STATE_WIFI_CONNECT;
        if (!ESP8266_ConnectWiFi(IOT_WIFI_SSID, IOT_WIFI_PASS))
        {
            g_net_state = NET_STATE_FAIL;
            vTaskDelay(pdMS_TO_TICKS(backoff_ms));
            backoff_ms = (backoff_ms * 2U > IOT_BACKOFF_MAX_MS) ? IOT_BACKOFF_MAX_MS : (backoff_ms * 2U);
            continue;
        }

        /* 阶段 3: 连接 MQTT Broker */
        g_net_state = NET_STATE_MQTT_CONNECT;
        if (!ESP8266_ConnectMQTT(IOT_MQTT_BROKER, IOT_MQTT_PORT, IOT_MQTT_CLIENT_ID))
        {
            g_net_state = NET_STATE_FAIL;
            vTaskDelay(pdMS_TO_TICKS(backoff_ms));
            backoff_ms = (backoff_ms * 2U > IOT_BACKOFF_MAX_MS) ? IOT_BACKOFF_MAX_MS : (backoff_ms * 2U);
            continue;
        }

        /* 阶段 4: 上线成功，指数退避时间复位回 1s，进入 3000ms 周期遥测推流 */
        g_net_state  = NET_STATE_ONLINE;
        backoff_ms   = IOT_BACKOFF_MIN_MS;
        pub_fail_cnt = 0U;

        while (g_net_state == NET_STATE_ONLINE)
        {
            if (q_env_data != NULL && xQueuePeek(q_env_data, &iot_env, 0) == pdPASS)
            {
                if (ESP8266_PublishEnv(IOT_MQTT_PUB_TOPIC, &iot_env))
                {
                    pub_fail_cnt = 0U;
                }
                else if (++pub_fail_cnt >= 3U)
                {
                    /* 连续 3 次发布失败，触发网络容灾回退自愈重连 */
                    g_net_state = NET_STATE_FAIL;
                    break;
                }
            }

            vTaskDelay(pdMS_TO_TICKS(3000));
        }

        vTaskDelay(pdMS_TO_TICKS(backoff_ms));
        backoff_ms = (backoff_ms * 2U > IOT_BACKOFF_MAX_MS) ? IOT_BACKOFF_MAX_MS : (backoff_ms * 2U);
    }
}
