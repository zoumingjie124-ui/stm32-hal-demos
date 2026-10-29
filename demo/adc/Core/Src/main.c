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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>      /* printf 的声明（ADC_USE_PRINTF 打开时才真正用到） */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 串口已经配好（USART1, PA9/PA10, 115200），printf 通过下面的 fputc 输出。
   置 1 才会真正调用 printf；置 0 时整段被编译掉，固件里连 printf 都不会链接。 */
#define ADC_USE_PRINTF   1
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* 放在全局 + volatile：方便在 Keil 的 Watch 窗口里直接观察，
   调试器读内存不会因为编译器优化而看到"过期值"。 */
volatile uint32_t g_adc_raw = 0u;   /* 原始转换值：0 ~ 4095（12 位右对齐） */
volatile uint32_t g_adc_mv  = 0u;   /* 换算后的电压：0 ~ 3300 mV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
  * @brief  printf 重定向：printf 格式化完成后逐字符调用 fputc
  * @note   本工程未勾 MicroLIB（useUlib=0），走标准 C 库。
  *         标准库的 printf 默认用 semihosting（靠调试器转发到 PC），
  *         脱机运行时没人应答会直接卡死，所以必须自己实现 fputc 顶掉它。
  */
int fputc(int ch, FILE *f)
{
  uint8_t c = (uint8_t)ch;                    /* 只取低 8 位 */
  HAL_UART_Transmit(&huart1, &c, 1u, 100u);   /* 阻塞发送，超时 100ms（1 字节约 87us） */
  return ch;
}

/* 不用 semihosting：告诉链接器别把它打进来，并补齐库会引用的桩函数 */
#pragma import(__use_no_semihosting)

struct __FILE { int handle; };
FILE __stdout;
FILE __stderr;

void _sys_exit(int x) { x = x; }
void _ttywrch(int ch) { ch = ch; }
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* ADC 自校准：F1 的 ADC 上电有几十 LSB 的零点偏移，不校准的话
     0V 输入可能读到 20~40 而不是 0，换算出来的 mV 也整体偏高。
     校准必须在第一次转换之前、ADC 关闭状态下做（HAL 内部会处理）。 */
  if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* ---- 一次完整的单次转换：启动 → 等完成 → 取结果 ---- */
    HAL_ADC_Start(&hadc1);                                  /* 软件触发，启动一次转换 */

    if (HAL_ADC_PollForConversion(&hadc1, 10u) == HAL_OK)   /* 轮询等待，超时给 10ms（实际约 5.7us） */
    {
      /* 为什么 raw 必须用 uint32_t 而不是 uint16_t：
         raw 最大 4095，raw * 3300 = 13,513,500，已经远超 uint16_t 的 65535。
         如果写成 uint16_t mv = raw * 3300 / 4095，乘法那一步先溢出（结果被截断到 16 位），
         再除出来的数就是错的，而且错得没有规律。
         另外 C 的整数运算顺序是先乘后除，所以"先除再乘"会丢掉小数精度。 */
      uint32_t raw = HAL_ADC_GetValue(&hadc1);              /* 12 位右对齐：0 ~ 4095 */
      uint32_t mv  = raw * 3300u / 4095u;                   /* 满量程 3.3V = 3300mV 对应 4095 */

      g_adc_raw = raw;                                      /* 存全局，Watch 窗口可见 */
      g_adc_mv  = mv;

#if ADC_USE_PRINTF
      /* %lu 对应 unsigned long，显式转换一下，避免 32 位类型别名不一致的告警 */
      printf("raw=%lu, %lu mV\r\n", (unsigned long)raw, (unsigned long)mv);
#endif
    }

    HAL_ADC_Stop(&hadc1);                                   /* 单次模式用完就关，省电也避免残留状态 */

    HAL_Delay(500);                                         /* 500ms 采一次，肉眼能看清变化 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
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
