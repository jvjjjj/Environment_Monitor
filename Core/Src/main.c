/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
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

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* Diagnostic checkpoint 1: main entry reached */
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;
  GPIOA->CRL = (GPIOA->CRL & 0xF000000F) | 0x03333330;
  GPIOA->BSRR = GPIO_PIN_1; // PA1 = HIGH (main entry)
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  GPIOA->BSRR = GPIO_PIN_2; // PA2 = HIGH (HAL_Init passed)

  /* Configure the system clock */
  SystemClock_Config();
  GPIOA->BSRR = GPIO_PIN_3; // PA3 = HIGH (SystemClock_Config passed)

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  GPIOA->BSRR = GPIO_PIN_4; // PA4 = HIGH (MX_GPIO_Init passed)

  /* USER CODE BEGIN 2 */
  // 1. 初始化 OLED 屏幕
  OLED_Init();
  GPIOA->BSRR = GPIO_PIN_5; // PA5 = HIGH (OLED_Init passed)
  OLED_Clear();

  // 绘制屏幕边框 (0,0) ~ (127,63)
  for (uint8_t i = 0; i < 128; i++) {
    OLED_DrawPoint(i, 0, 1);
    OLED_DrawPoint(i, 63, 1);
  }
  for (uint8_t i = 0; i < 64; i++) {
    OLED_DrawPoint(0, i, 1);
    OLED_DrawPoint(127, i, 1);
  }

  // 2. 显示 "Hello World!" 字符串
  OLED_ShowString(16, 24, "Hello World!");

  // 3. 将显存数据刷新到 OLED 屏幕
  OLED_UpdateScreen();
  GPIOA->BSRR = GPIO_PIN_6; // PA6 = HIGH (OLED_UpdateScreen passed)
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_1);
    for (volatile int d = 0; d < 50000; d++) __NOP();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration (使用内部 HSI 8MHz，无需外部晶振，杜绝死锁)
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */