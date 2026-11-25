/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* 0: 优先硬件传感器 (AHT20 + BMP280), 未接入物理传感器时自动回退至虚拟传感器桩
 * 1: 强制虚拟传感器桩模式 (无物理传感器台架 / HIL 自动化测试) */
#define ENV_FORCE_VIRTUAL_SENSOR    0

typedef struct {
    float    temperature; // 换算后的摄氏度 (-40.0 ~ 85.0)
    float    humidity;    // 换算后的相对湿度 (0.0 ~ 100.0%)
    float    pressure;    // 大气压强 (hPa)
    float    altitude;    // 国际标准气压公式估算海拔 (m)
    uint32_t timestamp;   // 采集时标 (毫秒 Tick)
    uint8_t  bmp_ready;   // 1: BMP280 在线, 0: 未接入/降级模式
    uint8_t  bmp_addr;    // 动态探测到的 BMP280 地址 (0x76 / 0x77)
    uint8_t  bmp_id;      // 读取到的 BMP280 Chip ID (0x58 等)
} EnvData_t;
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
