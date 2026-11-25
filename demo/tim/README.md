# demo/tim — TIM2 硬件更新中断 500ms 翻转 PC13

使用 `TIM2` 基本计数更新中断替代 `HAL_GetTick()` 轮询，实现精确周期的 `PC13` 闪烁。

## 参数推导

```
TIM2 挂载在 APB1 上。当 APB1 = 36MHz（HCLK 2 分频）时，APB1 定时器时钟 = PCLK1 × 2 = 72MHz
目标周期 500ms（2Hz）：总计数值 = 72,000,000 × 0.5 = 36,000,000
拆分为两个 16 位整型因子：7200 × 5000 = 36,000,000
```

| 寄存器 | CubeMX 参数 | 含义 | 配置值 |
|---|---|---|---|
| `PSC` | Prescaler | 分频系数 − 1 | `7199`（计数频率 10kHz） |
| `ARR` | Counter Period | 自动重装载值 − 1 | `4999`（计满 5000 次即 500ms 溢出） |

## 中断调用链

```
CNT 计到 4999 → 硬件归零并置位 SR.UIF
  → DIER.UIE=1 且 NVIC 已使能 → 进入 TIM2_IRQHandler()（stm32f1xx_it.c）
  → HAL_TIM_IRQHandler(&htim2)：检查并清除 UIF 标志
  → HAL_TIM_PeriodElapsedCallback(&htim2)（main.c USER CODE 4 覆盖 __weak 空实现）
  → 判断 htim->Instance == TIM2 后翻转 PC13
```

## 踩坑记录

1. **Counter Period 默认 65535**：在 CubeMX 修改定时器参数时需确认 `Period` 已写入 `.ioc`（`TIM2.IPParameters=Prescaler,Period`），否则默认 `65535` 会导致实际周期变成 6.55 秒。
2. **启动函数必须用 `HAL_TIM_Base_Start_IT(&htim2)`**：`HAL_TIM_Base_Start()` 只开计数不开更新中断。
3. **回调判断 `Instance`**：所有定时器共享 `HAL_TIM_PeriodElapsedCallback()`，多定时器工程不判断会误触发。
4. **`PC13` 上电初值**：已补全 `TIM2.ioc` 的 `PC13-TAMPER-RTC.PinState=GPIO_PIN_SET`，上电默认灭灯，与前几个 LED 例程保持一致。
5. **`SYS → Debug` 必须选 `Serial Wire`**：若保持默认 `No Debug`，CubeMX 会生成 `__HAL_AFIO_REMAP_SWJ_DISABLE()` 关死 SWD 调试口，后续无法直接烧录（需拉高 `BOOT0=1` 复位或接 NRST 使用 connect-under-reset 救回）。

## 编译与验证

编译结果（ARMCC V5.06）：`Code=3996 RO-data=304 RW-data=16 ZI-data=1704`，0 Error(s) / 0 Warning(s)。
> 状态：已统一 `PC13` 初值并编译通过，尚未重烧上板验证实际翻转周期。
