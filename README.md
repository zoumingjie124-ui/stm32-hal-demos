# STM32 HAL 外设练习

STM32F103C8T6 + STM32Cube HAL（FW_F1 V1.8.7）+ Keil MDK5。每个外设对应 `demo/` 下的一个最小可运行独立工程，自带 `.ioc` 配置、精简后的独立 `Drivers/` 与详细实验笔记。

## Demo 一览

| 目录 | 内容 | 详细文档 | 编译体积（ARMCC V5.06） | 状态 |
|---|---|---|---|---|
| [`demo/led`](demo/led) | PC13 板载 LED，500ms 非阻塞闪烁 | [README](demo/led/README.md) | `Code=2752 RO=304 RW=16 ZI=1632` | ✅ 已上板 SWD 实测 500ms 翻转 |
| [`demo/key-led`](demo/key-led) | PA0 按键 4 状态非阻塞消抖翻转 PC13 | [README](demo/key-led/README.md) | `Code=2848 RO=304 RW=24 ZI=1632` | ⚠️ 已上板确认上拉与空闲运行态（按键触发需手动短接 PA0-GND） |
| [`demo/exti-key`](demo/exti-key) | EXTI 下降沿中断按键 + 30ms 去抖 | [README](demo/exti-key/README.md) | `Code=2852 RO=304 RW=20 ZI=1636` | ⚠️ 已上板确认 `EXTI_FTSR` 下降沿使能（触发去抖需手动短接 PA0-GND） |
| [`demo/tim`](demo/tim) | TIM2 更新中断 500ms 翻转 PC13 | [README](demo/tim/README.md) | `Code=4012 RO=304 RW=16 ZI=1704` | ✅ 已上板 SWD 实测初值与 500ms 翻转 |
| [`demo/adc`](demo/adc) | PA1 单通道 ADC 非阻塞采样 + 串口打印 | [README](demo/adc/README.md) | `Code=6484 RO=328 RW=40 ZI=1752` | ⚠️ 已上板 SWD 实测非阻塞采样与换算（串口打印待接 USB-TTL 复核） |
| [`demo/usart`](demo/usart) | USART1 中断环形缓冲分帧 + 跨编译器 `printf` | [README](demo/usart/README.md) | `Code=5992 RO=312 RW=40 ZI=1896` | ⚠️ 已上板确认波特率/无半主机卡死/`BUSY_RX`（实验 C 收发待接 USB-TTL） |

## 硬件与全局引脚规划（STM32F103C8T6）

| 引脚 | 功能分配 | 使用工程 | 注意事项 |
|---|---|---|---|
| `PA0` | 按键输入 / `EXTI0`（内部上拉，按下接地） | `key-led`、`exti-key` | 四脚按键接对角；`exti-key` 须用下降沿触发 |
| `PA1` | `ADC1_IN1` 模拟输入 | `adc` | 悬空读数会拾取干扰乱跳，属正常高阻现象 |
| `PA9` / `PA10` | `USART1_TX` / `USART1_RX`（115200 8N1） | `adc`、`usart` | 与 `TIM1` 默认 `CH2`/`CH3` 复用同引脚，拓展同时使用时建议使用 `TIM1_CH1 (PA8)` 或重映射 `USART1` |
| `PA13` / `PA14` | `SWDIO` / `SWCLK`（SWD 调试下载） | 全部工程 | CubeMX `SYS → Debug` 必须选 `Serial Wire`，切勿选 `No Debug` |
| `PC13` | 板载 LED（低电平点亮，初值 `GPIO_PIN_SET`） | `led`、`key-led`、`exti-key`、`tim` | 上电默认灭灯 |
| `PD0` / `PD1` | `OSC_IN` / `OSC_OUT`（8MHz HSE） | 全部工程 | 倍频 ×9 得 72MHz 系统时钟 |

## 构建与一键烧录脚本

仓库根目录提供了统一脚本（基于 Keil `UV4.exe` 与 `STM32_Programmer_CLI.exe`）：

```bat
:: 全量重编所有 demo（支持 --fast 增量编译）
scripts\build_all.bat

:: 编译并一键烧录、校验、复位指定 demo
scripts\flash.bat adc
```

`demo/key-led/flash.bat` 也保留了可单独双击执行的独立烧录脚本。

## 目录结构

```text
stm32-hal-demos/
├── demo/
│   ├── led/          PC13 非阻塞闪烁（Basic_GPIO.ioc / Basic_GPIO.uvprojx）
│   ├── key-led/      PA0 非阻塞消抖状态机（GPIO_Key_LED.ioc / flash.bat）
│   ├── exti-key/     PA0 下降沿 EXTI 中断（EXTI_Key_LED.ioc）
│   ├── tim/          TIM2 500ms 更新中断（TIM2.ioc）
│   ├── adc/          PA1 单通道 ADC + 串口打印（ADC.ioc）
│   └── usart/        USART1 环形缓冲 + printf 重定向（USART_Test.ioc）
├── scripts/
│   ├── build_all.bat 全工程批量编译校验脚本
│   └── flash.bat     通用编译 + 烧录 + 复位脚本
├── .gitignore        忽略编译产物、日志、.mxproject 与 RTE 缓存
├── LICENSE           MIT 开源协议
└── README.md         仓库总索引与规范说明
```

- 每个 demo 的 `Drivers/` 均已按独立编译所需的最小集合裁剪（仅保留 `STM32F103xB` 相关头文件与本工程实际编译的 HAL `.c` 文件），单目录可直接拷走编译，单个工程 `Drivers/` 约 2.0 ~ 2.4 MB。
  > **CubeMX 重新生成提示**：若后续在 CubeMX 中打开 `.ioc` 并重新点击 `Generate Code`，CubeMX 会把完整 HAL/CMSIS 驱动再次铺回工程目录；重新生成后请注意核对差异或再次裁剪未用到的驱动文件，避免冗余驱动重新入库。
- 需要 `printf` 的工程（`demo/adc`、`demo/usart`）已统一抽离出 `Core/Src/uart_retarget.c`，通过 `#if defined(__CC_ARM)` / `__ARMCC_VERSION` / `__GNUC__` 同时兼容 **Keil AC5、Keil AC6 与 STM32CubeIDE（GCC）**。

## 仓库约定

1. **单一职责**：一个 demo 聚焦一个外设主题，深度笔记与踩坑记录写在各自目录的 `README.md` 中。
2. **严禁主循环阻塞**：所有定时节拍与按键消抖统一使用 `HAL_GetTick()` 或硬件定时器，不在主循环使用 `HAL_Delay()` 阻塞。
3. **中断边界**：中断回调内只做登记状态、标记位或重挂外设（如 `HAL_UART_Receive_IT()`），且**必须检查返回值**；不放 `HAL_Delay()` 或阻塞发送。
4. **CubeMX 同步**：引脚与外设参数先改 `.ioc` 再生成代码，不直接手改 `USER CODE` 保护区外的生成文件。
5. **命名与文件卫生**：新增 C 变量采用 `camelCase`，宏采用全大写；源码统一 UTF-8 无 BOM 编码；编译产物、`.log`、`.mxproject` 与 `RTE/` 缓存不入库。

## 协议

本项目示例代码采用 [MIT License](LICENSE) 开源；`Drivers/` 目录下 CMSIS 与 STM32Cube HAL 驱动遵循 ST / Arm 原有许可条款（见各 `Drivers/*/LICENSE.txt`）。
