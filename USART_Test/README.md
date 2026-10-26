# USART_Test — 串口中断回显与「重挂」实验

验证一件事：**`HAL_UART_Receive_IT()` 只武装一次**，回调里不重新启动接收，就只能收一个字节。

## 硬件与配置

| 项目 | 配置 |
|---|---|
| MCU | STM32F103C8T6（Blue Pill） |
| 串口 | USART1，PA9 = TX，PA10 = RX |
| 参数 | 115200，8 数据位，1 停止位，无校验（CubeMX 默认） |
| 接线 | PA9 → USB转TTL 的 RXD；PA10 → TXD；GND 共地 |
| 中断 | CubeMX 中已勾选 `USART1 global interrupt` |

## 代码位置（`Core/Src/main.c`）

| 区域 | 内容 |
|---|---|
| `USER CODE PV` | `uint8_t rx_byte;` —— 全局变量，回调可见 |
| `USER CODE 2` | `HAL_UART_Receive_IT(&huart1, &rx_byte, 1);` 上电挂起第一次接收 |
| `USER CODE 4` | `HAL_UART_RxCpltCallback()`：回发 + **重新启动接收** |

```c
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    HAL_UART_Transmit(&huart1, &rx_byte, 1, 100);   /* 原样回发 */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);      /* 必须重新启动接收 */
  }
}
```

## 实验 A：回调里保留 `HAL_UART_Receive_IT`（正确）

烧录后串口助手（115200）发送任意字节：

| 发送 | 接收 | 说明 |
|---|---|---|
| `A` | `A` | 正常回显 |
| `B` | `B` | 继续正常 |
| `C`、`D`… | 原样回显 | 可无限次连续收发 |

## 实验 B：删掉回调里的 `HAL_UART_Receive_IT`（错误）

只保留回发那一句，重新编译烧录：

| 发送 | 接收 | 说明 |
|---|---|---|
| 第 1 个字节 | 正常回显 | 因为 main 里那次 `Receive_IT` 还在生效 |
| 第 2 个字节 | **无任何反应** | 中断已被 HAL 关掉，且没人再打开 |
| 之后的字节 | 全部无反应 | 串口不关也没用，只有复位后能再收一个 |

现象总结：**只回一个字符，之后彻底没反应**。

## 原因（对照 HAL 源码 `stm32f1xx_hal_uart.c`）

| 行号 | 代码 | 含义 |
|---|---|---|
| 2363 | `if ((isrflags & USART_SR_RXNE) ...)` | 收到字节且接收中断使能，才进 `UART_Receive_IT` |
| 3625 | `if (--huart->RxXferCount == 0U)` | 本批数据收完（Size=1 就是收满 1 字节） |
| 3628 | `__HAL_UART_DISABLE_IT(huart, UART_IT_RXNE);` | **HAL 主动关闭接收中断** |
| 3637 | `huart->RxState = HAL_UART_STATE_READY;` | 状态回到空闲 |
| 3675 | `HAL_UART_RxCpltCallback(huart);` | 最后才调用用户回调 |

所以回调执行时，中断已经处于关闭状态。**没有任何机制会自动把它重新打开**——这就是“必须自己重挂”的根本原因。

顺带一个易被忽略的细节：3628/3637 都发生在 3675 之前，即 HAL **先**关中断、**先**把 `RxState` 置回 `READY`，**再**调用回调。正因为如此，回调里再调 `HAL_UART_Receive_IT()` 才能顺利通过 `RxState == HAL_UART_STATE_READY` 的检查（1353 行），否则会返回 `HAL_BUSY`。

## 附加实验：printf 重定向到串口

目标：让 `printf` 直接往 USART1 输出，方便调试打日志。

### 为什么不能只写 `fputc`

本工程 `<useUlib>0</useUlib>`，**没有勾选 MicroLIB**，用的是标准 C 库。两种情况差别很大：

| 情况 | printf 底层走谁 | 脱机运行后果 |
|---|---|---|
| 勾了 MicroLIB | 直接调用用户实现的 `fputc` | 只写 `fputc` 就够 |
| **本工程：标准库** | 默认走 **semihosting**：`_write` → `SVC 0xAB` | 该指令要靠调试器转发到 PC；**脱机时无人应答，程序卡死** |

所以标准库下必须同时做三件事：

1. 实现 `fputc`，把字符送到 USART1；
2. `#pragma import(__use_no_semihosting)`，让链接器不要把 semihosting 打进来；
3. 补 `_sys_exit` / `_ttywrch` / `struct __FILE` / `__stdout`，否则链接报未定义符号。

### 代码位置

| 区域 | 内容 |
|---|---|
| `USER CODE Includes` | `#include <stdio.h>` |
| `USER CODE 0` | `fputc()` 重定向 + `__use_no_semihosting` + 三个桩函数 |
| `USER CODE 2` | `printf("hello\r\n");`，位置在 `MX_USART1_UART_Init()` 之后 |

```c
int fputc(int ch, FILE *f)
{
  uint8_t c = (uint8_t)ch;                    /* 只取低 8 位 */
  HAL_UART_Transmit(&huart1, &c, 1, 0xFFFF);  /* 阻塞发送 1 字节 */
  return ch;
}

#pragma import(__use_no_semihosting)
struct __FILE { int handle; };
FILE __stdout;
FILE __stderr;
void _sys_exit(int x) { x = x; }
void _ttywrch(int ch) { ch = ch; }
```

### 怎么验证 semihosting 真的被排除了

编译后反汇编检查有没有 `SVC 0xAB`（有就说明还在走 semihosting）：

```bat
fromelf -c MDK-ARM\USART_Test\USART_Test.axf | findstr /C:"svc     0xAB"
```

实测结果：**0 次命中**，`fputc` 在固件里（Code 段 26 字节），printf 只可能走串口。

### 实测记录

| 项目 | 结果 |
|---|---|
| 编译 | 0 Error / 0 Warning，`Code=4952 RO=312 RW=24 ZI=1704` |
| 反汇编 | `svc 0xAB` 出现 **0** 次 |
| 上板 | 串口助手（COM4，CH340，115200 8N1）收到 `hello` ✅ |

### 两个容易踩的坑

1. **`hello` 只在开机时打印一次**。它写在 `while(1)` 之前，端口没打开就永远看不到——想重测就按板子 RESET，不用重新烧录。
2. **必须用 `\r\n`**，只发 `\n` 串口助手不会回行首；另外中文会按 Keil 源文件的 GBK 编码发出，接收端要设成 GBK 才不乱码。

## 结论

1. `HAL_UART_Receive_IT()` 是**一次性武装**：收满指定长度后自动解除，不自动续杯。
2. 每收完一批数据都必须重新调用一次，位置就在 `HAL_UART_RxCpltCallback()` 里。
3. 重挂时要检查返回值（`HAL_OK` / `HAL_BUSY`），否则重挂失败是静默的，症状同样是“只收一次”。
4. 需要收变长数据时改用 `HAL_UARTEx_ReceiveToIdle_IT()`，它在总线空闲时触发回调，同样需要重挂。
