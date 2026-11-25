# demo/adc — PA1 单通道 ADC 采样与串口打印

电位器分压接入 `PA1`（`ADC1_IN1`），以 `HAL_GetTick()` 非阻塞 500ms 节拍启动软件单次采样，换算为毫伏（mV）后通过 `USART1` `printf` 打印输出。

## 硬件与外设配置

| 项目 | 配置 |
|---|---|
| ADC | `ADC1`，通道 `ADC_CHANNEL_1`（`PA1` 模拟输入） |
| 转换模式 | 单次转换、软件触发、关闭扫描 |
| 采样时间 | `55.5 cycles`（单次总耗时约 `(55.5 + 12.5) / 12MHz ≈ 5.67 µs`） |
| ADC 时钟 | `PCLK2 ÷ 6 = 12MHz`（STM32F103 ADC 上限 14MHz） |
| 串口输出 | `USART1`（`PA9` TX / `PA10` RX，115200 8-N-1），通过 `uart_retarget.c` 重定向 |
| 采样节拍 | 主循环 `HAL_GetTick()` 每 500ms 非阻塞触发一次 |

## 代码结构与关键细节

| 文件 / 区域 | 说明 |
|---|---|
| `Core/Inc/main.h` | `ADC_USE_PRINTF` 编译开关（置 `0` 时完全关闭 `printf` 与重定向编译，避免脱机触发 semihosting） |
| `Core/Src/uart_retarget.c` | 跨编译器串口重定向（支持 Keil AC5 / AC6 与 STM32CubeIDE GCC） |
| `Core/Src/main.c` (`USER CODE 2`) | 调用 `uartRetargetInit(&huart1)`、执行 `HAL_ADCEx_Calibration_Start(&hadc1)` 上电自校准 |
| `Core/Src/main.c` (`USER CODE 3`) | 检查 `HAL_ADC_Start` / `HAL_ADC_PollForConversion` 返回值并统计 `gAdcErrCount`，最后 `HAL_ADC_Stop` |

### 电压换算防溢出

```c
uint32_t raw = HAL_ADC_GetValue(&hadc1);   /* 12 位右对齐：0 ~ 4095 */
uint32_t mv  = raw * 3300u / 4095u;        /* 满量程 3300mV 对应 4095 */
```

`4095 × 3300 = 13,513,500`，远超 `uint16_t` 上限（65535），因此中间量必须使用 `uint32_t` 先乘后除。

## 实测记录与误差分析（改非阻塞前上板验证）

| `raw` | 固件换算 `mv` | 手算 `raw*3300/4095` | 结论 |
|---|---|---|---|
| 1806 | 1455 | 1455 | 一致 |
| 843 | 679 | 679 | 一致 |
| 587 | 473 | 473 | 一致 |
| 483 | 389 | 389 | 一致 |

- **零点偏移**：上电自校准已消除大部分零点偏移（< 1 LSB）。
- **噪声**：电位器稳定位置连续 6 次波动在 `481~483`（±2 LSB ≈ 1.6mV），可通过多次采样平均进一步收敛；悬空输入乱跳属于正常高阻拾取噪声。
- **基准误差**：板载实测 `VDDA ≈ 3.27V`，代码按 `3300mV` 换算存在约 +0.9% 系统偏差，更高绝对精度需采集内部 `Vrefint` 反推真实 `VDDA`。

## 编译与烧录

```bat
scripts\flash.bat adc
```

编译结果（ARMCC V5.06）：`Code=6484 RO-data=328 RW-data=40 ZI-data=1752`，0 Error(s) / 0 Warning(s)。
> 注：上表线性数据采集于非阻塞重构前；本轮改用 `HAL_GetTick()` 非阻塞节拍并重构 `uart_retarget` 后已完成编译校验，尚未重烧上板复测。
