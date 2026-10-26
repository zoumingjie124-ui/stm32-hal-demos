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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>          /* printf / fputc / FILE 的声明 */
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
uint8_t rx_byte;                    /* 中断接收缓存：回调函数也要用它，必须是全局变量 */

/* ---- 环形缓冲：中断只负责往里塞，主循环负责取 ---- */
#define RX_RING_SIZE   128u         /* 必须是 2 的幂会更省，这里用取模写法更直观 */
#define LINE_MAX       64u          /* 单行（一帧）最大长度，含结尾 '\0' */

static volatile uint8_t  rx_ring[RX_RING_SIZE];
static volatile uint16_t rx_head = 0u;      /* 只由中断写 */
static volatile uint16_t rx_tail = 0u;      /* 只由主循环写 */

static char     line_buf[LINE_MAX];         /* 攒出一整行的地方 */
static uint16_t line_len = 0u;

/* 诊断计数：环形缓冲满导致丢弃的字节数（volatile：中断里加，主循环里读并清零） */
static volatile uint32_t rx_drop = 0u;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
  * @brief  printf 重定向：printf 的底层输出最终会调用 fputc
  * @note   本工程未勾选 MicroLIB（useUlib=0），用的是标准 C 库。
  *         标准库的 printf 底层默认走 semihosting（靠调试器转发），
  *         脱机运行时没人应答，会直接卡死。所以必须自己实现 fputc 顶掉它。
  * @param  ch : 要输出的字符（int 是标准库的约定，只有低 8 位有效）
  * @param  f  : 目标流（stdout/stderr），这里不用区分，一律从 USART1 发
  * @retval 返回写出的字符，表示成功
  */
int fputc(int ch, FILE *f)
{
  uint8_t c = (uint8_t)ch;                    /* 只取低 8 位，避免把 int 高位也发出去 */
  HAL_UART_Transmit(&huart1, &c, 1, 0xFFFF);  /* 阻塞发送 1 字节，超时给足（约 87us 就发完） */
  return ch;
}

/* ---- 以下三个桩是"不用 semihosting"时必须提供的，否则链接会报未定义符号 ---- */
/* Keil AC5 语法：告诉链接器不要把 semihosting 打进来 */
#pragma import(__use_no_semihosting)

struct __FILE { int handle; };   /* 标准库内部用来描述流的类型 */
FILE __stdout;                   /* printf 默认写的就是这个流 */
FILE __stderr;

void _sys_exit(int x)            /* 库在异常退出时会调用它 */
{
  x = x;
}

void _ttywrch(int ch)            /* 库在某些调试输出时会调用它 */
{
  ch = ch;
}

/* ---------------- 环形缓冲：单生产者 / 单消费者 ----------------
   生产者 = USART 中断（只写 rx_head）
   消费者 = 主循环（只写 rx_tail）
   两者各改各的指针，所以不需要关中断，也不会互相破坏。
   代价：满的时候必须丢数据（这里丢新字节），用 rx_drop 记录丢了多少。 */

/**
  * @brief  中断里调用：把一个字节放进环形缓冲
  * @note   耗时约 1~2us，远小于 115200 下 86.8us 的字节间隔
  */
static void rx_push(uint8_t b)
{
  uint16_t next = (uint16_t)((rx_head + 1u) % RX_RING_SIZE);

  if (next != rx_tail)              /* 还有空位 */
  {
    rx_ring[rx_head] = b;
    rx_head = next;
  }
  else                              /* 满了：丢弃这个新字节并计数 */
  {
    rx_drop++;
  }
}

/**
  * @brief  主循环里调用：取出一个字节
  * @retval 1 = 取到了，0 = 缓冲空
  */
static int rx_pop(uint8_t *b)
{
  if (rx_head == rx_tail)           /* 空 */
  {
    return 0;
  }

  *b = rx_ring[rx_tail];
  rx_tail = (uint16_t)((rx_tail + 1u) % RX_RING_SIZE);
  return 1;
}
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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  printf("hello\r\n");                         /* 串口重定向自检：串口助手应收到 hello */
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);   /* 启动第一次中断接收：收满1字节触发回调 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* 主循环只做两件事：把缓冲里的字节取出来、攒成一行、整行处理。
       这里慢一点没关系（最多是"回显晚几毫秒"），不像中断里会丢数据。 */
    {
      uint8_t b;

      while (rx_pop(&b))
      {
        if (b == (uint8_t)'\n')                       /* 一帧结束 */
        {
          if ((line_len > 0u) && (line_buf[line_len - 1u] == '\r'))
          {
            line_len--;                                /* 去掉 \r，只留内容 */
          }
          line_buf[line_len] = '\0';

          printf("ECHO[%u]: %s\r\n", (unsigned)line_len, line_buf);
          line_len = 0u;                               /* 准备下一帧 */
        }
        else if (b == (uint8_t)'\r')
        {
          /* 单独出现的 \r 先忽略，等后面的 \n 再收尾 */
        }
        else if (line_len < (LINE_MAX - 1u))
        {
          line_buf[line_len] = (char)b;
          line_len++;
        }
        else                                           /* 单行超长：丢掉这一帧 */
        {
          line_len = 0u;
          printf("ERR: line longer than %u, dropped\r\n", (unsigned)(LINE_MAX - 1u));
        }
      }

      /* 环形缓冲满过就把计数报出来（读完清零），这就是"丢字节"的证据 */
      if (rx_drop != 0u)
      {
        uint32_t d = rx_drop;
        rx_drop = 0u;
        printf("WARN: ring full, dropped %lu byte(s)\r\n", (unsigned long)d);
      }
    }
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
}

/* USER CODE BEGIN 4 */
/**
  * @brief  UART 接收完成中断回调：每收到 1 字节进入一次
  * @note   这里只做两件事：塞进环形缓冲 + 重新武装接收。
  *         绝不在中断里做阻塞发送 —— 那会把 RXNE 中断关掉 87us，
  *         而 115200 下字节间隔就是 86.8us，连发必然溢出丢字节。
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    rx_push(rx_byte);                            /* 只登记，不处理 */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);   /* 一次性武装，必须重挂 */
  }
}

/**
  * @brief  UART 错误回调（主要是 ORE 溢出、帧错误、噪声）
  * @note   F1 的接收寄存器只有 1 字节，来不及读就会 ORE。
  *         发生错误后 HAL 会把 RxState 打回 READY 并不再收数据，
  *         所以必须清标志 + 重新武装，否则串口从此"失聪"。
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    __HAL_UART_CLEAR_OREFLAG(huart);             /* 读 SR 再读 DR，硬件才会清 ORE */
    (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
}
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
