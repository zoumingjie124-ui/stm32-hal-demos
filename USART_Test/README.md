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

## 结论

1. `HAL_UART_Receive_IT()` 是**一次性武装**：收满指定长度后自动解除，不自动续杯。
2. 每收完一批数据都必须重新调用一次，位置就在 `HAL_UART_RxCpltCallback()` 里。
3. 重挂时要检查返回值（`HAL_OK` / `HAL_BUSY`），否则重挂失败是静默的，症状同样是“只收一次”。
4. 需要收变长数据时改用 `HAL_UARTEx_ReceiveToIdle_IT()`，它在总线空闲时触发回调，同样需要重挂。
