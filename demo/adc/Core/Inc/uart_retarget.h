/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    uart_retarget.h
  * @brief   标准输出 printf 串口重定向接口（兼容 Keil AC5 / AC6 与 GCC）
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __UART_RETARGET_H
#define __UART_RETARGET_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdio.h>

/**
  * @brief  绑定用于 printf 输出的 UART 句柄
  * @param  huart : 目标串口句柄指针（例如 &huart1）
  */
void uartRetargetInit(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* __UART_RETARGET_H */
