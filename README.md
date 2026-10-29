# STM32 HAL 外设练习

STM32F103C8T6 + STM32Cube HAL（FW_F1 V1.8.7）+ Keil MDK5。每个外设一个最小可运行的工程，各自独立的 CubeMX 配置、独立编译、独立下载。

## Demo 一览

| 目录 | 内容 | 状态 |
|---|---|---|
| [`demo/led`](demo/led) | PC13 板载 LED，500ms 非阻塞翻转 | ✅ 实测编译 0 Error / 0 Warning |
| [`demo/key-led`](demo/key-led) | PA0 按键消抖翻转 PC13，带一键烧录脚本 | ✅ 实测编译 0 Error / 0 Warning，已上板 |
| [`demo/exti-key`](demo/exti-key) | EXTI 外部中断版按键：下降沿触发 + 中断内 30ms 去抖 | ✅ 实测编译 0 Error / 0 Warning |
| [`demo/tim`](demo/tim) | TIM2 更新中断每 500ms 翻转 PC13（替代 `HAL_GetTick()` 轮询） | ✅ 实测编译 0 Error / 0 Warning |
| [`demo/adc`](demo/adc) | PA1 单通道 ADC 采样，`printf` 输出 raw / mV，已用电位器验证线性 | ✅ 实测编译 0 Error / 0 Warning，已上板 |
| [`USART_Test`](USART_Test) | USART1 中断回显 + printf 重定向，验证 `HAL_UART_Receive_IT` 必须重挂 | ✅ 实测通过（含反例现象；printf 已在 COM4 收到 `hello`） |
| `demo/pwm` | TIM1 PWM 输出 | 计划中 |

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

## demo/exti-key（EXTI 外部中断版按键）

和 `demo/key-led` 同一个硬件、同一颗灯，区别是**不再轮询，靠 EXTI 中断触发**。

| | key-led（轮询） | exti-key（中断） |
|---|---|---|
| 触发方式 | `while(1)` 里反复读 PA0 | PA0 电平跳变触发硬件中断 |
| CPU 占用 | 主循环一直在转 | 平时 0，事件来了才进 |
| 去抖 | `HAL_Delay(20)` + 二次确认（**阻塞**） | 中断里比较 `HAL_GetTick()` 时间戳，30ms 内重复触发直接丢 |
| 按住时 | `while` 卡住整个主循环 | 不阻塞主循环，但**中断本身必须短** |

### 代码位置

| 区域 | 内容 |
|---|---|
| `main.c` → `USER CODE 4` | `HAL_GPIO_EXTI_Callback()`：30ms 去抖 + 翻转 PC13 |
| `stm32f1xx_it.c` | `EXTI0_IRQHandler()` → `HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0)` |
| `gpio.c` | PA0 = `GPIO_MODE_IT_FALLING` + `GPIO_PULLUP`；NVIC `EXTI0_IRQn` 已使能，优先级 0 |

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  static uint32_t last_tick = 0;

  if (GPIO_Pin == GPIO_PIN_0)
  {
    uint32_t now = HAL_GetTick();
    if ((now - last_tick) >= 30u)      /* 30ms 内的重复触发当作抖动忽略 */
    {
      last_tick = now;
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
  }
}
```

### 三个坑

1. **边沿选反**：按键一端接 GND、内部上拉，所以**按下是下降沿**。配成 `IT_RISING` 的现象是「按下没反应，松手才翻灯」——很容易误判成按键坏了。
2. **改 `gpio.c` 会被覆盖**：它是 CubeMX 生成物，重新 `GENERATE CODE` 会把边沿改回去。正解是在 `.ioc` 里把 PA0 设成 `External Interrupt Mode with Falling edge trigger detection`。
3. **中断里不能阻塞**：不要放 `HAL_Delay()` 或串口阻塞发送。另外 EXTI0 优先级是 **0**，高于 SysTick 的 **15**，所以在中断里调 `HAL_GetTick()` 时 tick 可能还没推进（同一毫秒内）——这也意味着去抖阈值不能设成 1~2ms 那种量级。

编译：`Code=2852 RO=304 RW=20 ZI=1636`，0 Error / 0 Warning。

## demo/tim（TIM2 中断 500ms 翻转 PC13）

把 `demo/led` 里 `HAL_GetTick()` 轮询翻转的做法，换成**硬件定时器更新中断**驱动。这个工程是用 CubeMX 图形界面配置生成的，`.ioc` 里 TIM2 配置完整。

### 参数怎么来的

```
TIM2 挂 APB1。APB1 = 36MHz（2 分频）时，定时器时钟 = PCLK1 × 2 = 72MHz
目标 500ms：总计数 = 72,000,000 × 0.5 = 36,000,000
拆成两个 16 位因子：7200 × 5000 = 36,000,000
```

| 寄存器 | CubeMX 字段 | 意义 | 填值 |
|---|---|---|---|
| `PSC` | Prescaler | 分频系数 − 1 | **7199**（→ 计数频率 10kHz） |
| `ARR` | Counter Period | 计数上限 − 1 | **4999**（0~4999 共 5000 次） |

两者都 ≤ 65535。注意寄存器存的是「减一」后的值：PSC 填 0 才是「不分频」，ARR 填 0 才是「每计 1 次就溢出」。

### 代码位置

| 文件 / 区域 | 内容 |
|---|---|
| `Core/Src/tim.c` | `MX_TIM2_Init()`（PSC/ARR/内部时钟）+ `HAL_TIM_Base_MspInit()`（开 TIM2 时钟、NVIC 使能） |
| `Core/Src/main.c` → `USER CODE 2` | `HAL_TIM_Base_Start_IT(&htim2);` |
| `Core/Src/main.c` → `USER CODE 4` | `HAL_TIM_PeriodElapsedCallback()`：判断 `Instance == TIM2` 后翻转 PC13 |
| `Core/Src/stm32f1xx_it.c` | `TIM2_IRQHandler()` → `HAL_TIM_IRQHandler(&htim2)` |

### 中断的完整调用链

```
CNT 计到 4999 → 归零并置 SR.UIF
   → DIER.UIE=1 且 NVIC 已使能 → 内核按向量表跳 TIM2_IRQHandler()
   → HAL_TIM_IRQHandler()：查标志、清 UIF、分发
   → HAL_TIM_PeriodElapsedCallback()（HAL 里是 __weak 空实现，被 main.c 同名函数覆盖）
   → HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13)
```

### 踩过的坑（真实记录）

1. **Counter Period 默认 65535** —— 只在 CubeMX 里填了 Prescaler，`Period` 没写进 `.ioc`（`TIM2.IPParameters=Prescaler`），生成出来是 `Period = 65535` → 实际周期 **6.55 秒**而不是 500ms。已把 `Period` 加入 `IPParameters` 并设为 4999。
2. **漏 `HAL_TIM_Base_Start_IT()`** —— 计数器在跑、`UIF` 也置位，但永远不进中断（`HAL_TIM_Base_Start()` 不使能更新中断）。
3. **回调必须判断 `Instance`** —— 所有定时器共用一个回调，不判断的话多个定时器都会触发同一段逻辑。
4. **PC13 初值是 `GPIO_PIN_RESET`（低电平）** —— 板载 LED 低电平点亮，所以上电先亮 500ms 再进入正常闪烁。
5. **`SYS → Debug = No Debug` 会把调试口关死（本工程最初就踩了这个）** —— CubeMX 在这个选项下会生成
   `__HAL_AFIO_REMAP_SWJ_DISABLE()`，程序一启动 SWD 和 JTAG 全部关闭，之后**任何工程都烧不进去**，
   Keil 报 `No target connected` / `Flash Download failed`，CubeProgrammer 报 `Unable to get core ID`。
   救回来的办法：**BOOT0 跳 1 + 复位**（用户程序不执行，SWD 恢复），烧完再跳回 0；或者给 ST-Link 接 NRST 用 connect-under-reset。
   已改为 `Serial Wire`（生成 `__HAL_AFIO_REMAP_SWJ_NOJTAG()`，保留 SWD）。
   **新建工程时第一件事就是确认这一项。**

### 编译

`Code=3996 RO=304 RW=16 ZI=1704`，0 Error / 0 Warning。

**上板实测待补**：曾因 `SYS = No Debug` 导致 SWD 被关闭而无法连接（已修复，见上面第 5 条），
灯的实际翻转周期尚未用仪器确认。

## demo/adc（PA1 单通道 ADC + 串口打印）

电位器分压 → PA1 采样 → 换算成 mV → 每 500ms 从 USART1 打印一行。

### 配置

| 项 | 值 |
|---|---|
| ADC | ADC1，通道 **IN1（PA1）** |
| 转换模式 | 单次、**软件触发**、不扫描 |
| 采样时间 | **55.5 cycles** |
| ADC 时钟 | **PCLK2 ÷ 6 = 12MHz**（F103 的 ADC 上限是 14MHz，36MHz 会超） |
| 触发节奏 | 主循环每 500ms 一次 |

单次转换耗时 = (55.5 + 12.5) / 12MHz ≈ **5.67 µs**，采样周期远小于节奏，不会有堆积。

### 代码位置（都在 USER CODE 段，重新生成不会被覆盖）

| 区域 | 内容 |
|---|---|
| `USER CODE PD` | `ADC_USE_PRINTF` 开关（没有串口时置 0，避免 printf 走 semihosting 卡死） |
| `USER CODE 0` | `fputc` 重定向 + `__use_no_semihosting` + `_sys_exit/_ttywrch/__FILE` |
| `USER CODE 2` | `HAL_ADCEx_Calibration_Start(&hadc1)` —— 第一次转换前自校准 |
| `USER CODE 3` | `HAL_ADC_Start → PollForConversion(10ms) → GetValue → 换算 → printf → HAL_Delay(500)` |

### 换算公式与那个 `uint32_t`

```c
uint32_t raw = HAL_ADC_GetValue(&hadc1);   /* 12 位右对齐：0 ~ 4095 */
uint32_t mv  = raw * 3300u / 4095u;        /* 满量程 3300mV 对应 4095 */
```

`raw` 必须是 32 位：`4095 × 3300 = 13,513,500`，**远超 uint16_t 的 65535**。若写成 `uint16_t mv = raw * 3300 / 4095`，乘法先溢出，结果错得没有规律——而且小数值时是对的，接近满量程才炸。

### 实测记录（SWD 直接读 RAM，与固件输出交叉验证）

| `raw` | 固件算出的 mV | 手算 `raw*3300/4095` | 一致 |
|---|---|---|---|
| 1806 | 1455 | 1455 | ✅ |
| 843 | 679 | 679 | ✅ |
| 587 | 473 | 473 | ✅ |
| 483 | 389 | 389 | ✅ |

接上电位器后，同一位置连续 6 次读数稳定在 **481~483（波动 2 LSB ≈ 1.6mV）**；未接时（PA1 悬空）读数在 587~1806 之间乱跳 —— 这个对比本身就证明输入确实接到了低阻信号源。

### 精度分析（哪些误差修了、哪些没修）

| 误差项 | 量级 | 校准能修吗 |
|---|---|---|
| 零点偏移 | 校准后 < 1 LSB（~0.8mV） | ✅ 已修 |
| 噪声 | ±2 LSB（~1.6mV） | ❌ 需多次采样平均 |
| **VDDA 实际值** | 实测 **3.27V**，代码按 3300 算 → 系统偏高约 **0.9%（~30mV）** | ❌ 需读内部 `Vrefint` 或外部基准 |

**当前最大的一项误差不是 ADC，是“假设 3.3V 就是 3.3V”。** 要提高绝对精度，下一步应该读 `Vrefint` 反推真实 VDDA。

### 两个坑

1. **没有串口时不要开 `printf`** —— 标准库会走 semihosting（`SVC 0xAB`），脱机运行时没人应答，程序直接卡死。所以本工程用 `ADC_USE_PRINTF` 开关控制，配好 USART1 并加上 `fputc` 后才置 1。
2. **PA1 悬空时读数乱跳是正常的** —— 高阻输入在拾取环境噪声。判断“ADC 坏没坏”不能看数值大小，要看**接上确定电压后是否稳定、是否随输入线性变化**。

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
│   ├── key-led/                  PA0 按键消抖翻转 PC13
│   │   ├── GPIO_Key_LED.ioc      CubeMX 工程
│   │   ├── Core/                 Inc + Src（main / gpio / it / msp / system）
│   │   ├── Drivers/              同上
│   │   ├── MDK-ARM/              GPIO_Key_LED.uvprojx
│   │   └── flash.bat             一键编译 + 烧录 + 复位
│   ├── exti-key/                 EXTI 外部中断版按键
│   │   ├── EXTI_Key_LED.ioc      CubeMX 工程（PA0 = Falling edge trigger）
│   │   ├── Core/                 Inc + Src（中断回调在 main.c 的 USER CODE 4）
│   │   ├── Drivers/              同上
│   │   └── MDK-ARM/              EXTI_Key_LED.uvprojx
│   ├── tim/                      TIM2 中断 500ms 翻转 PC13
│   │   ├── TIM2.ioc              CubeMX 工程（TIM2 = Internal Clock, PSC 7199 / ARR 4999）
│   │   ├── Core/                 Inc + Src（tim.c = TIM2 配置）
│   │   ├── Drivers/              同上
│   │   └── MDK-ARM/              TIM2.uvprojx
│   └── adc/                      PA1 单通道 ADC + 串口打印
│       ├── ADC.ioc               CubeMX 工程（ADC1_IN1 / PCLK2÷6 / USART1）
│       ├── Core/                 Inc + Src（adc.c / usart.c / main.c）
│       ├── Drivers/              同上
│       └── MDK-ARM/              ADC.uvprojx
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
