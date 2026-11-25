# demo/key-led — PA0 按键非阻塞消抖翻转 PC13

轮询读取 `PA0` 按键输入，通过 4 状态非阻塞软件消抖状态机实现“每按下一次，PC13 稳定翻转一次”。

## 硬件与配置

| 项目 | 说明 |
|---|---|
| MCU | STM32F103C8T6 |
| LED | `PC13`，板载，低电平点亮（初值 `GPIO_PIN_SET` = 灭） |
| 按键 | `PA0`，`GPIO_MODE_INPUT` + `GPIO_PULLUP`（内部上拉），按下接地为 `GPIO_PIN_RESET` |
| 消抖方式 | `HAL_GetTick()` 4 状态机，按下/松手各 20ms 消抖（**无阻塞**） |

> 接线注意：四脚轻触开关同侧两脚内部导通，`PA0` 与 `GND` 必须接**对角**引脚。

## 状态机执行流程（`Core/Src/main.c`）

```
KEY_STATE_IDLE ──(PA0==LOW, 记时间)──▶ KEY_STATE_DEBOUNCE_PRESS
      ▲                                        │
      │ (毛刺回弹: PA0==HIGH)                  │ (持续 LOW ≥ 20ms: 翻转 PC13)
      └────────────────────────────────────────▼
KEY_STATE_DEBOUNCE_RELEASE ◀──(PA0==HIGH)── KEY_STATE_PRESSED
      │
      └──(持续 HIGH ≥ 20ms)──▶ KEY_STATE_IDLE
```

- 相比 `HAL_Delay(20)` + `while(...)` 死等松手的写法，状态机在按下等待消抖和长按不松手期间都**立即返回主循环**，不占用其他外设的 CPU 时间。
- 增加了松手消抖阶段（`KEY_STATE_DEBOUNCE_RELEASE`），防止松开按键瞬间触点回弹误触发下一次翻转。

## 编译与烧录

- 可双击本目录 `flash.bat` 一键编译并调用 `STM32_Programmer_CLI.exe` 烧录复位，或在仓库根目录执行：

```bat
scripts\flash.bat key-led
```

编译结果（ARMCC V5.06）：`Code=2848 RO-data=304 RW-data=24 ZI-data=1632`，0 Error(s) / 0 Warning(s)。
> 上板检查（ST-Link SWD）：实测 `GPIOA->CRL=0x44444448`、`GPIOA->ODR=0xA001`（PA0 内部上拉已使能）、`GPIOA->IDR=0x9EFD`（空闲高电平）、`GPIOC->ODR=0x2000`（PC13 初始灭灯）、`keyState=0`（`KEY_STATE_IDLE`）、`uwTick` 连续推进不阻塞。实际按键翻转表现待物理短接 `PA0` 与 `GND` 复测。
