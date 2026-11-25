# demo/led — PC13 板载 LED 非阻塞闪烁

使用 `HAL_GetTick()` 毫秒节拍在主循环实现 500ms 周期翻转，避免 `HAL_Delay()` 阻塞 CPU。

## 硬件与配置

| 项目 | 说明 |
|---|---|
| MCU | STM32F103C8T6（64KB Flash / 20KB RAM） |
| 时钟 | HSE 8MHz → PLL ×9 → 72MHz，`FLASH_LATENCY_2` |
| LED | `PC13`，板载，推挽输出，初值 `GPIO_PIN_SET`（上电默认灭，低电平点亮） |
| 周期 | 500ms 翻转一次 |
| 定时方式 | `HAL_GetTick()` 非阻塞时间差轮询 |

## 核心逻辑（`Core/Src/main.c`）

```c
uint32_t last_flash_tick = HAL_GetTick();

while (1)
{
  uint32_t now = HAL_GetTick();
  if ((now - last_flash_tick) >= 500u)
  {
    last_flash_tick += 500u;              /* 累加而非直接赋值 now，避免节拍漂移累积 */
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
  }
}
```

- 采用无符号减法 `(now - last_flash_tick) >= 500u`，在 `SysTick` 32 位计数约 49.7 天溢出回绕时依然成立。
- 主循环无阻塞延时，后续可直接叠加按键、串口或 ADC 等并行任务。

## 编译与烧录

- Keil 打开 `MDK-ARM/Basic_GPIO.uvprojx` 编译下载，或在仓库根目录执行：

```bat
scripts\build_all.bat
scripts\flash.bat led
```

编译结果（ARMCC V5.06）：`Code=2752 RO-data=304 RW-data=16 ZI-data=1632`，0 Error(s) / 0 Warning(s)。
> 状态：本轮仅统一 `main.c` 缩进格式，已完成编译校验（未重烧上板）。
