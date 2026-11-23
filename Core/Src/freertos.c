/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : FreeRTOS v4.0 物联网多任务协同管理
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "virtual_sensor.h"
#include "oled.h"
#include "esp8266.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
// 全局环境数据状态缓存 (临界区保护)
static EnvData_t g_latest_env = {0};
static volatile uint8_t g_data_valid = 0;

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

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityIdle, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  // 1. 虚拟传感器更新任务 (周期 1000ms, 优先级 Normal)
  osThreadDef(Task_Sensor, StartSensorTask, osPriorityNormal, 0, 128);
  Task_SensorHandle = osThreadCreate(osThread(Task_Sensor), NULL);

  // 2. OLED 屏幕刷新任务 (周期 500ms, 优先级 BelowNormal)
  osThreadDef(Task_OLED, StartOledTask, osPriorityBelowNormal, 0, 128);
  Task_OLEDHandle = osThreadCreate(osThread(Task_OLED), NULL);

  // 3. 本地串口调试与心跳任务 (周期 1000ms, 优先级 BelowNormal)
  osThreadDef(Task_Comm, StartCommTask, osPriorityBelowNormal, 0, 128);
  Task_CommHandle = osThreadCreate(osThread(Task_Comm), NULL);

  // 4. 物联网上云交互任务 (优先级 Normal, 栈分配 160=640字节)
  osThreadDef(Task_IoT, StartIoTTask, osPriorityNormal, 0, 160);
  Task_IoTHandle = osThreadCreate(osThread(Task_IoT), NULL);
  /* USER CODE END RTOS_THREADS */
}

/* USER CODE BEGIN Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  for(;;)
  {
    osDelay(100);
  }
}
/* USER CODE END Header_StartDefaultTask */

/* ==================== 任务 1: 虚拟传感器采集任务 ==================== */
void StartSensorTask(void const * argument)
{
  VirtualSensor_Init();
  vTaskDelay(pdMS_TO_TICKS(100));

  for(;;)
  {
    VirtualSensor_Update();
    VirtualSensor_t vdata = VirtualSensor_Get();

    taskENTER_CRITICAL();
    g_latest_env.temperature = vdata.temperature;
    g_latest_env.humidity = vdata.humidity;
    g_latest_env.timestamp = (uint32_t)xTaskGetTickCount();
    g_data_valid = 1;
    taskEXIT_CRITICAL();

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

/* ==================== 任务 2: OLED 屏幕显示任务 ==================== */
void StartOledTask(void const * argument)
{
  EnvData_t oled_env = {0};
  char disp_buf[20];
  vTaskDelay(pdMS_TO_TICKS(200));

  for(;;)
  {
    OLED_ShowString(0, 0, "== ENV MONITOR ==");

    if (g_data_valid)
    {
        taskENTER_CRITICAL();
        oled_env = g_latest_env;
        taskEXIT_CRITICAL();

        sprintf(disp_buf, "Temp: %5.1f C", oled_env.temperature);
        OLED_ShowString(0, 16, disp_buf);

        sprintf(disp_buf, "Hum : %5.1f %%", oled_env.humidity);
        OLED_ShowString(0, 32, disp_buf);
    }
    else
    {
        OLED_ShowString(0, 16, "Temp: --.- C");
        OLED_ShowString(0, 32, "Hum : --.- %");
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
            OLED_ShowString(0, 48, "NET: TIMEOUT/ERR");
            break;
        default:
            OLED_ShowString(0, 48, "NET: BOOTING... ");
            break;
    }

    OLED_UpdateScreen();
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

/* ==================== 任务 3: 心跳任务 ==================== */
void StartCommTask(void const * argument)
{
  vTaskDelay(pdMS_TO_TICKS(1000));
  for(;;)
  {
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

/* ==================== 任务 4: 物联网上云交互任务 (Mock ESP8266) ==================== */
void StartIoTTask(void const * argument)
{
  vTaskDelay(pdMS_TO_TICKS(1000));

  for(;;)
  {
    /* ---------------- 阶段 1: AT 握手测试 ---------------- */
    g_net_state = NET_STATE_AT_TEST;
    while (ESP8266_SendCmd("AT", "OK", 1000) != ESP8266_OK)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* ---------------- 阶段 2: 模拟连接 WiFi ---------------- */
    g_net_state = NET_STATE_WIFI_CONNECT;
    while (!ESP8266_ConnectWiFi("Virtual_WiFi", "12345678"))
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* ---------------- 阶段 3: 模拟连接 MQTT Broker ---------------- */
    g_net_state = NET_STATE_MQTT_CONNECT;
    while (!ESP8266_ConnectMQTT("broker.emqx.io", 1883))
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* ---------------- 阶段 4: 进入永久在线发布状态！ ---------------- */
    g_net_state = NET_STATE_ONLINE; // ★ 屏幕永久锁定在 NET: MQTT ONLINE！

    extern UART_HandleTypeDef huart2;
    char mqtt_payload[128];

    // 进入平稳推流循环：单片机每 3 秒主动把真实 JSON 发到你的串口屏幕上，无需你再回任何 OK！
    while (1)
    {
        if (g_data_valid)
        {
            taskENTER_CRITICAL();
            float t = g_latest_env.temperature;
            float h = g_latest_env.humidity;
            taskEXIT_CRITICAL();

            // 组装并直接推送物模型 JSON 数据
            snprintf(mqtt_payload, sizeof(mqtt_payload), 
                     "[MQTT PUB -> broker.emqx.io/env/data] {\"temp\":%.1f, \"humi\":%.1f}\r\n", t, h);
            HAL_UART_Transmit(&huart2, (uint8_t *)mqtt_payload, strlen(mqtt_payload), 100);
        }

        vTaskDelay(pdMS_TO_TICKS(3000)); // 每 3 秒自动推送一次，静静欣赏即可！
    }
  }
}
