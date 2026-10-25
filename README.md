# STM32 HAL 外设练习

STM32F103C8T6 + STM32Cube HAL（FW_F1 V1.8.7）+ Keil MDK5。每个外设一个最小可运行的工程，各自独立的 CubeMX 配置、独立编译、独立下载。

## Demo 一览

| 目录 | 内容 | 状态 |
|---|---|---|
| [`demo/led`](demo/led) | PC13 板载 LED，500ms 非阻塞翻转 | ✅ 实测编译 0 Error / 0 Warning |
| [`demo/key-led`](demo/key-led) | PA0 按键消抖翻转 PC13，带一键烧录脚本 | ✅ 实测编译 0 Error / 0 Warning，已上板 |
| [`USART_Test`](USART_Test) | USART1 中断回显，验证 `HAL_UART_Receive_IT` 必须重挂 | ✅ 实测通过（含反例现象） |
| `demo/tim` | TIM1 PWM 输出 | 计划中 |

## demo/led

| 项目 | 说明 |
|---|---|
| MCU | STM32F103C8T6（64KB Flash / 20KB RAM） |
| 时钟 | HSE 8MHz → PLL ×9 → 72MHz，`FLASH_LATENCY_2` |
| LED | PC13，板载，低电平点亮 |
| 周期 | 500ms 翻转一次 |
| 定时方式 | `HAL_GetTick()` 轮询，**不使用 `HAL_Delay()`**，主循环不被阻塞 |

核心逻辑（`Core/Src/main.c`）：

```c
uint32_t last_flash_tick = HAL_GetTick();

while (1)
{
    uint32_t now = HAL_GetTick();
    if ((now - last_flash_tick) >= 500u)
    {
        last_flash_tick += 500u;              /* 累加而非赋值，避免抖动累积 */
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
}
```

用非阻塞定时而不是 `HAL_Delay(500)`，是为了后面往这个工程里加按键、串口收发时，主循环还能继续跑。

### 编译与下载

1. 打开 `demo/led/MDK-ARM/Basic_GPIO.uvprojx`
2. F7 编译，F8 用 ST-Link 下载

命令行编译（需已装 Keil）：

```sh
cd demo/led/MDK-ARM
"<Keil安装路径>/UV4/UV4.exe" -r Basic_GPIO.uvprojx -t Basic_GPIO -o build.log
```

实测结果：`Code=2752  RO-data=304  RW-data=16  ZI-data=1632`，0 Error / 0 Warning。

## demo/key-led（PA0 按键控制 PC13）

| 项目 | 说明 |
|---|---|
| MCU | STM32F103C8T6 |
| LED | PC13，板载，低电平点亮（初值 `GPIO_PIN_SET` = 灭） |
| 按键 | PA0，内部上拉，按下接地即为低电平 |
| 消抖 | 20ms 延时 + 二次确认 + 等松手 |

核心逻辑（`Core/Src/main.c`）：

```c
if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET)
{
  HAL_Delay(20);                                            /* 等抖动过去 */
  if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) /* 再确认 */
  {
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);                 /* 只翻转一次 */
    while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET)
    {
      /* 等松手：按住期间不再翻转 */
    }
  }
}
```

两点注意：

- 这是**阻塞式**消抖。`HAL_Delay(20)` 和「等松手」的死循环都会占住主循环，只点一个灯够用；以后加串口、ADC 时要改成 `HAL_GetTick()` 记时间戳的状态机。
- 四脚轻触开关**同侧两脚内部相通**，PA0 与 GND 必须接**对角**，接同侧会变成常通或常断。

### 一键烧录（不依赖 Keil 的 Reset and Run）

双击 `demo/key-led/flash.bat`：自动定位工程 → 编译 → 用 STM32CubeProgrammer 下载 → 校验 → **复位并运行**。

脚本会自动探测 Keil（`D:\K5\UV4` 或 Program Files）和 `STM32_Programmer_CLI.exe`（`%LOCALAPPDATA%\stm32cube\bundles` 或 CubeProgrammer 安装目录），换机器只需改这两处路径。

实测：`Code=2836  RO-data=304  RW-data=16  ZI-data=1632`，0 Error / 0 Warning。

## USART_Test（USART1 中断回显）

| 项目 | 说明 |
|---|---|
| MCU | STM32F103C8T6 |
| 串口 | USART1，PA9 = TX / PA10 = RX |
| 参数 | 115200，8-N-1 |
| 中断 | `USART1 global interrupt`（CubeMX 中勾选） |
| 接线 | PA9 → USB转TTL 的 RXD，PA10 → TXD，必须共 GND |

核心逻辑（`Core/Src/main.c`，USER CODE 2 与 USER CODE 4）：

```c
HAL_UART_Receive_IT(&huart1, &rx_byte, 1);   /* 上电武装一次 */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    HAL_UART_Transmit(&huart1, &rx_byte, 1, 100);   /* 原样回发 */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);      /* 必须重挂，否则只收一个字节 */
  }
}
```

**这个 demo 的重点是反例**：把回调里的 `HAL_UART_Receive_IT` 删掉后，只有第 1 个字节能回显，之后就彻底没反应——因为 HAL 在收完后自己关了 `RXNEIE`（源码 3628 行）并把 `RxState` 置回 `READY`（3637 行），再调回调（3675 行），**没有任何机制会自动重新打开中断**。

完整实验记录（两种现象的对照表、源码行号对应）见 [`USART_Test/README.md`](USART_Test/README.md)。

## 目录结构

```
stm32-hal-demos/
├── demo/
│   ├── led/                      PC13 非阻塞闪烁
│   │   ├── Basic_GPIO.ioc        CubeMX 工程
│   │   ├── Core/                 Inc + Src（main / gpio / it / msp / system）
│   │   ├── Drivers/
│   │   │   ├── CMSIS/            Include + Device/ST/STM32F1xx/Include
│   │   │   └── STM32F1xx_HAL_Driver/   Inc + Src
│   │   └── MDK-ARM/              uvprojx / uvoptx / startup
│   └── key-led/                  PA0 按键消抖翻转 PC13
│       ├── GPIO_Key_LED.ioc      CubeMX 工程
│       ├── Core/                 Inc + Src（main / gpio / it / msp / system）
│       ├── Drivers/              同上
│       ├── MDK-ARM/              GPIO_Key_LED.uvprojx
│       └── flash.bat             一键编译 + 烧录 + 复位
├── USART_Test/                   USART1 中断回显
│   ├── USART_Test.ioc            CubeMX 工程
│   ├── Core/                     Inc + Src（main / gpio / usart / it / msp）
│   ├── Drivers/                  同上
│   ├── MDK-ARM/                  USART_Test.uvprojx
│   └── README.md                 实验记录：重挂 vs 不重挂的两种现象
└── README.md
```

`Drivers` 只保留编译必需的部分，已去掉 CMSIS 的 DSP、NN、RTOS 和 ARM 数学库（86MB → 17MB）。每个 demo 自带一份，方便单独打开工程。

## 约定

- 一个 demo 只演示一个外设，尽量不加无关功能。
- 定时一律用 `HAL_GetTick()` 或定时器，不用 `HAL_Delay()` 阻塞主循环。
- 中断式外设的回调里，**必须重新武装**（如 `HAL_UART_Receive_IT`），并检查返回值。
- 编译产物不入库，见 `.gitignore`。
- 每个 demo 的深度笔记写在它自己的 `README.md` 里，根 README 只放索引和核心片段。
