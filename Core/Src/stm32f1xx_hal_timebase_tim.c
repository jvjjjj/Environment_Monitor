/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f1xx_hal_timebase_tim.c
  * @brief   HAL time base based on the hardware TIM4.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_tim.h"

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim4;

/* TIM4 init function */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  RCC_ClkInitTypeDef sClockConfig;
  uint32_t uwTimclock = 0;
  uint32_t uwPrescalerValue = 0;
  uint32_t pFLatency;

  /* Enable TIM4 clock */
  __HAL_RCC_TIM4_CLK_ENABLE();

  /* Get clock configuration */
  HAL_RCC_GetClockConfig(&sClockConfig, &pFLatency);

  /* Compute TIM4 clock: APB1 timer clock */
  uwTimclock = 2 * HAL_RCC_GetPCLK1Freq();

  /* Compute the prescaler value to have TIM4 counter clock equal to 1MHz */
  uwPrescalerValue = (uint32_t) ((uwTimclock / 1000000U) - 1U);

  /* Initialize TIM4 */
  htim4.Instance = TIM4;
  htim4.Init.Period = (1000000U / 1000U) - 1U;
  htim4.Init.Prescaler = uwPrescalerValue;
  htim4.Init.ClockDivision = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  if (HAL_TIM_Base_Init(&htim4) == HAL_OK)
  {
    /* Start TIM4 in interrupt mode */
    HAL_NVIC_SetPriority(TIM4_IRQn, TickPriority, 0U);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
    return HAL_TIM_Base_Start_IT(&htim4);
  }

  return HAL_ERROR;
}

void HAL_SuspendTick(void)
{
  __HAL_TIM_DISABLE_IT(&htim4, TIM_IT_UPDATE);
}

void HAL_ResumeTick(void)
{
  __HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE);
}

void TIM4_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim4);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM4) {
    HAL_IncTick();
  }
}
