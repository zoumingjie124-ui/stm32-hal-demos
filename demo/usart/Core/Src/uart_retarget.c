/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    uart_retarget.c
  * @brief   标准输出 printf 串口重定向实现（支持 Keil AC5 / AC6 与 GCC）
  ******************************************************************************
  */
/* USER CODE END Header */

#include "uart_retarget.h"

#if defined(HAL_UART_MODULE_ENABLED) && (!defined(ADC_USE_PRINTF) || (ADC_USE_PRINTF != 0))

#define UART_RETARGET_TIMEOUT_MS   100u   /* 单字节阻塞发送超时（ms），115200 下约 87us 完成 */

static UART_HandleTypeDef *retargetUart = NULL;

/**
  * @brief  绑定用于 printf 输出的 UART 句柄
  * @param  huart : 目标串口句柄指针
  */
void uartRetargetInit(UART_HandleTypeDef *huart)
{
  retargetUart = huart;
}

static void uartRetargetPutChar(uint8_t ch)
{
  if (retargetUart != NULL)
  {
    (void)HAL_UART_Transmit(retargetUart, &ch, 1u, UART_RETARGET_TIMEOUT_MS);
  }
}

#if defined(__CC_ARM)
/* Keil ARMCC (AC5)：关闭 semihosting 并提供标准 C 库所需的流桩函数 */
#pragma import(__use_no_semihosting)

struct __FILE
{
  int handle;
};

FILE __stdout;
FILE __stderr;

void _sys_exit(int x)
{
  (void)x;
}

void _ttywrch(int ch)
{
  (void)ch;
}

int fputc(int ch, FILE *f)
{
  (void)f;
  uartRetargetPutChar((uint8_t)ch);
  return ch;
}

#elif defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
/* Keil ARMCLANG (AC6)：标准 C 库下通过汇编符号禁用 semihosting */
#if !defined(__MICROLIB)
__asm(".global __use_no_semihosting\n\t");

struct __FILE
{
  int handle;
};

FILE __stdout;
FILE __stderr;

void _sys_exit(int x)
{
  (void)x;
}

void _ttywrch(int ch)
{
  (void)ch;
}
#endif

int fputc(int ch, FILE *f)
{
  (void)f;
  uartRetargetPutChar((uint8_t)ch);
  return ch;
}

#elif defined(__GNUC__)
/* STM32CubeIDE / arm-none-eabi-gcc (Newlib)：实现 _write 底层输出 */
int _write(int file, char *ptr, int len)
{
  int idx;

  (void)file;
  for (idx = 0; idx < len; idx++)
  {
    uartRetargetPutChar((uint8_t)ptr[idx]);
  }
  return len;
}
#endif

#endif /* HAL_UART_MODULE_ENABLED && ADC_USE_PRINTF */
