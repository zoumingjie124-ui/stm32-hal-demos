# demo/exti-key — EXTI 外部中断版按键翻转 PC13

与 `demo/key-led` 使用同一硬件引脚，但改为 `PA0` 下降沿硬件外部中断触发，在回调内做 30ms 非阻塞时间窗去抖。

## 与轮询版对比

| 对比项 | `demo/key-led`（轮询状态机） | `demo/exti-key`（EXTI 中断） |
|---|---|---|
| 触发方式 | `while(1)` 持续扫描 `PA0` | `PA0` 下降沿硬件请求中断 |
| 空闲 CPU 占用 | 每次循环都读一次寄存器 | 平时为 0，仅电平跳变时进入 ISR |
| 去抖方式 | 4 状态机（按下 + 松手各 20ms） | 中断内比较 `HAL_GetTick()`，30ms 内重复触发直接忽略 |

## 代码位置与配置

| 文件 / 区域 | 内容 |
|---|---|
| `EXTI_Key_LED.ioc` | `PA0-WKUP` 配置 `GPIO_ModeDefaultEXTI=GPIO_MODE_IT_FALLING` + `GPIO_PULLUP`，使能 `EXTI0_IRQn` |
| `Core/Src/gpio.c` | `PA0` 初始化为 `GPIO_MODE_IT_FALLING` + `GPIO_PULLUP`，`EXTI0_IRQn` 抢占优先级 0 |
| `Core/Src/stm32f1xx_it.c` | `EXTI0_IRQHandler()` → `HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0)` |
| `Core/Src/main.c` (`USER CODE 4`) | `HAL_GPIO_EXTI_Callback()`：30ms 去抖后翻转 `PC13` |

## 常见踩坑笔记

1. **边沿选反**：按键按下接地产生下降沿。若误配为 `GPIO_MODE_IT_RISING`，现象是“按下没反应，松手才翻灯”。
2. **不要直接手改 `gpio.c` 脱离 `.ioc`**：触发边沿必须在 `.ioc`（CubeMX 中 `External Interrupt Mode with Falling edge trigger detection`）里同步修改，否则重新生成代码会被覆盖。
3. **中断内严禁阻塞与优先级关系**：回调里绝不能放 `HAL_Delay()`。同时注意 CubeMX 默认 `EXTI0_IRQn` 抢占优先级为 `0`，高于 `SysTick_IRQn`（`15`），因此同一毫秒内的多次毛刺读到的 `HAL_GetTick()` 不会推进，去抖阈值应大于数毫秒（本工程为 `30u`）。

## 编译与验证

编译结果（ARMCC V5.06）：`Code=2852 RO-data=304 RW-data=20 ZI-data=1636`，0 Error(s) / 0 Warning(s)。
> 上板检查（ST-Link SWD）：实测 `GPIOA->CRL=0x44444448`、`GPIOC->ODR=0x2000`、`EXTI->IMR=0x0001`、`EXTI->RTSR=0x0000`、`EXTI->FTSR=0x0001`（确认 PA0 已配为下降沿中断触发），`uwTick` 连续推进。实际触发翻转与 `30ms` 去抖待物理短接 `PA0` 与 `GND` 复测。
