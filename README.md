# STM32 HAL 外设练习

STM32F103C8T6 + STM32Cube HAL（FW_F1 V1.8.7）+ Keil MDK5。每个外设一个最小可运行的 demo，全部放在同一个仓库的 `demo/` 下，独立编译、独立下载。

## Demo 一览

| 目录 | 内容 | 状态 |
|---|---|---|
| [`demo/led`](demo/led) | PC13 板载 LED，500ms 非阻塞翻转 | ✅ 实测编译 0 Error / 0 Warning |
| `demo/uart` | USART1 收发 | 计划中 |
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

## 目录结构

```
stm32-hal-demos/
├── demo/
│   └── led/
│       ├── Basic_GPIO.ioc        CubeMX 工程
│       ├── Core/                 Inc + Src（main / gpio / it / msp / system）
│       ├── Drivers/
│       │   ├── CMSIS/            Include + Device/ST/STM32F1xx/Include
│       │   └── STM32F1xx_HAL_Driver/   Inc + Src
│       └── MDK-ARM/              uvprojx / uvoptx / startup
└── README.md
```

`Drivers` 只保留编译必需的部分，已去掉 CMSIS 的 DSP、NN、RTOS 和 ARM 数学库（86MB → 17MB）。每个 demo 自带一份，方便单独打开工程。

## 约定

- 一个 demo 只演示一个外设，尽量不加无关功能。
- 定时一律用 `HAL_GetTick()` 或定时器，不用 `HAL_Delay()` 阻塞主循环。
- 编译产物不入库，见 `.gitignore`。
