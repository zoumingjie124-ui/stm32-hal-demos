# demo/usart — 串口中断环形缓冲接收与跨编译器 printf 重定向

验证 `HAL_UART_Receive_IT()` 的“一次性武装”机制，并通过 **ISR 环形缓冲入队 + 主循环按行分帧 + ORE 错误恢复** 解决连续高速接收丢字节问题。

## 硬件与配置

| 项目 | 配置 |
|---|---|
| MCU | STM32F103C8T6（Blue Pill） |
| 串口 | `USART1`，`PA9` = TX，`PA10` = RX |
| 参数 | 115200，8 数据位，1 停止位，无校验（CubeMX 默认） |
| 接线 | `PA9` → USB 转 TTL `RXD`；`PA10` → `TXD`；`GND` 共地 |
| 中断 | CubeMX 中开启 `USART1 global interrupt` |

## 代码位置（`Core/Src/`）

| 文件 / 区域 | 内容 |
|---|---|
| `uart_retarget.c/.h` | 独立串口重定向模块：支持 Keil AC5 / AC6 与 GCC（Newlib `_write`），发送单字节超时 `100u` ms |
| `main.c` (`USER CODE PV`) | `rx_byte`、`rx_ring[128]`、`rx_head` / `rx_tail`、`line_buf[64]`、`rx_drop`、`rxErrCount`、`rxRearmNeeded` |
| `main.c` (`USER CODE 2`) | `uartRetargetInit(&huart1)`、开机自检打印 `hello\r\n`、检查首次 `HAL_UART_Receive_IT()` 返回值 |
| `main.c` (`USER CODE 3`) | 主循环 `rx_pop()` 按 `\n` 切帧（兼容 `\r\n` 去除尾部 `\r`）、`rxRearmNeeded` 兜底重挂、`PRIMASK` 原子读取丢包/错误计数 |
| `main.c` (`USER CODE 4`) | `HAL_UART_RxCpltCallback()`（入队 + 重挂并查返回值）与 `HAL_UART_ErrorCallback()` |

## 实验 A 与反例实验 B：为什么必须重挂 `HAL_UART_Receive_IT`

- **实验 A（正确）**：回调里在接收完 1 字节后再次调用 `HAL_UART_Receive_IT()`，串口可持续收发。
- **实验 B（反例）**：删掉回调里的 `HAL_UART_Receive_IT()` 后，只有第 1 个字节能触发回调，后续字节彻底无响应。

对照 HAL 源码 `Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c`：

| 行号 | 代码 | 含义 |
|---|---|---|
| 2363 | `if ((isrflags & USART_SR_RXNE) ...)` | 收到字节且开启 `RXNEIE` 才进入 `UART_Receive_IT()` |
| 3625 | `if (--huart->RxXferCount == 0U)` | 本批接收计数归零 |
| 3628 | `__HAL_UART_DISABLE_IT(huart, UART_IT_RXNE);` | HAL **主动关闭接收中断** |
| 3637 | `huart->RxState = HAL_UART_STATE_READY;` | 接收状态复位为空闲 |
| 3675 | `HAL_UART_RxCpltCallback(huart);` | 最后才调用户回调，因此必须由用户重新武装 |

## 实验 C：环形缓冲 + 分帧 + 健壮性修复

### 1. 为什么不能在回调里直接阻塞回发

| 项 | 数值 |
|---|---|
| 115200 8-N-1 字节间隔 | `86.8 µs` |
| STM32F1 接收寄存器深度 | 1 字节（无硬件 FIFO） |
| 回调里阻塞发 1 字节 + 中断开销 | ≈ `90 µs` |

处理时间大于到达间隔且 HAL 先关了 `RXNEIE`，连续发送必然触发 `ORE` 溢出丢字节。改为两层解耦后：

```
中断（1~2µs）:  rx_push(rx_byte)  +  检查重挂 HAL_UART_Receive_IT()
主循环（不限）: rx_pop() 攒行（以 '\n' 结尾） → 整行打印 ECHO[n]: <line>
```

### 2. 错误回调与并发细节处理

- **精确处理 `HAL_UART_ErrorCallback()`**：仅在发生 `HAL_UART_ERROR_ORE` 或 `huart->RxState == HAL_UART_STATE_READY` 时才执行 `__HAL_UART_CLEAR_OREFLAG(huart)` 并重新武装，避免在普通帧/噪声错误（HAL 未中止接收、`RxCpltCallback` 已重挂）下多读一次 `DR` 误吞有效字节。
- **重挂失败兜底**：3 处 `HAL_UART_Receive_IT()` 均检查返回值；若中断中因 `HAL_BUSY` 等原因失败，置位 `rxRearmNeeded` 并递增 `rxErrCount`，主循环检测到空闲后自动恢复接收。
- **原子快照计数**：主循环通过 `__get_PRIMASK()` / `__disable_irq()` 原子读取并清零 `rx_drop` 与 `rxErrCount`，避免与 ISR 发生读-改-写丢计数。
- **帧尾说明**：仅以 `\n`（兼容 `\r\n`）作为行结束符；若串口助手只发送 `\r`，数据会停留在行缓存中直到收到 `\n` 或超长报错。

### 预期应答格式

| 发送（带换行） | 应答 |
|---|---|
| `hello` | `ECHO[5]: hello` |
| `abcdefghijklmnopqrstuvwxyz` | `ECHO[26]: abcdefghijklmnopqrstuvwxyz` |
| 超过 63 字符的一行 | `ERR: line longer than 63, dropped` |
| 缓冲满或错误事件 | `WARN: ring full, dropped N byte(s)` / `WARN: UART error/rearm count = N` |

## 编译与验证

编译结果（ARMCC V5.06）：`Code=5992 RO-data=312 RW-data=40 ZI-data=1896`，0 Error(s) / 0 Warning(s)。
> 上板检查（ST-Link SWD）：旧版实验 A/B 与 `printf` 重定向曾上板通过（COM4 收到 `hello`）；本轮重构后上板实测 `USART1->BRR=0x0271`（115200）、`CR1=0x202C`（`UE/RXNEIE/TE/RE` 全开）、`SR=0x00C0`（开机 `hello` 发送完成，未卡死于半主机）、`huart1.RxState=0x22`（`BUSY_RX`）、`rxRearmNeeded=0`、`rx_drop=0`、`rxErrCount=0`。因当前无串口设备连接，实验 C 连续收发与报错分支待接 USB 转 TTL 复测。
