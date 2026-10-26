/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    tim.c
  * @brief   This file provides code for the configuration
  *          of the TIM instances.
  ******************************************************************************
  *
  * 说明（本工程的计算依据）：
  *   TIM2 挂在 APB1 上。F1 的规则是：APB1 预分频 != 1 时，定时器时钟 = PCLK1 x2。
  *   本工程 APB1 = 36MHz（2 分频），所以 TIM2CLK = 72MHz。
  *
  *   目标：500ms 触发一次更新中断
  *   总计数 = 72MHz x 0.5s = 36,000,000
  *   拆成两个 16 位因子：7200 x 5000 = 36,000,000
  *   寄存器填值（都是"减一"后的数）：
  *     PSC = 7200 - 1 = 7199   -> 计数频率 72MHz / 7200 = 10kHz
  *     ARR = 5000 - 1 = 4999   -> 从 0 数到 4999 共 5000 次 = 500ms
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "tim.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

TIM_HandleTypeDef htim2;

/* TIM2 init function */
void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7199;                                  /* 7200 分频 -> 10kHz */
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;                  /* 向上计数 */
  htim2.Init.Period = 4999;                                     /* 5000 次 -> 500ms */
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;     /* 内部时钟，不占引脚 */
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* tim_baseHandle)
{
  if(tim_baseHandle->Instance==TIM2)
  {
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* TIM2 interrupt Init：NVIC 里勾了 "TIM2 global interrupt" 才会生成这两行 */
    HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
