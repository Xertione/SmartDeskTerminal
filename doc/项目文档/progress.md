# 进度

- 当前阶段：Phase 1 工程初始化与基础运行环境
- 状态：进行中（**MCU 侧已全部验证正常**；仅剩"用户蓝灯不亮"这一硬件悬案）
- 最后更新：2026-09-18

## 已完成

- [x] PlatformIO 工程骨架（`framework = stm32cube`，board = black_f407ve）
- [x] 硬件齐备（F407VE 黑板 / ST-Link / ST7789 屏 / USB-C 线）
- [x] 路线决策：PIO + stm32cube HAL，Phase 3 引入 FreeRTOS（见 [decision-log.md](decision-log.md) ADR-002）
- [x] 文档骨架初始化（迁至 `doc/项目文档/`）
- [x] LED 引脚查实：**PC0**，且**低电平点亮**（原理图 + 官方 `Drivers/User/Inc/led.h` 双验证）
- [x] LED 闪烁代码（[main.c](../../SmartDeskTerminal_freertos/src/main.c)）+ 时钟决策（ADR-003 PLLQ=7 / ADR-004 HSE_VALUE 宏）
- [x] 工作区根统一 repo（ADR-005）
- [x] 首次烧录成功（日志有 `** Verified OK **`）
- [x] **SWD 在线诊断：MCU 侧全部通过**（详证见 [troubleshooting.md](troubleshooting.md) T-005）
  - HSE 8MHz 起振 + PLL 锁定；`SystemCoreClock = 168000000`（精确）
  - `uwTick` 相隔 2000ms 涨 2003 / 2013 → SysTick 每 1ms 精准
  - PC0 实测 **1Hz / 50% 占空比方波**；无 HardFault；PC 落在 `HAL_Delay` 中
- [x] 屏幕接线资料核实：屏的 18-pin FPC **已由商家预先插好**在 28005 模块上；模块 14-pin 排针丝印与手册**逐脚一致**（见 `05_转接板_28005_SPI模块.md` 第 7 节）

## 正在做

用**屏幕背光**作为替代可见指示，验证"板子能正常驱动外部显示器件"。

接线（只需 3 根母对母杜邦线，其余针脚全部悬空）：28005 模块 `VCC→3V3`、`GND→GND`、`LED→3V3`。

## 下一步计划

1. 接好 3 根线（**先拍照给执行方确认再上电**）→ 预期背光整片亮起
2. 若背光亮 → 板子对外驱动能力确认 OK，Phase 1 可收尾，蓝灯问题降级为"已知硬件异常"记录在案
3. 蓝灯最终定性（用户有万用表后）：量 PC0 引脚对 GND，闪烁时应 0V↔3.3V 跳变
4. **Phase 5 前置**：解决 `02` 与 `06` 号文件关于 "U4 pin4 是否 `LCD_BL_PD12`" 的冲突

## 阻塞问题

- **PC0 蓝灯不亮**：已确认**不是软件问题**，故障在 `PC0 引脚 → 板上 LED2` 硬件段。不阻塞后续 Phase，但 Phase 1 的可见指示改用屏幕背光。
- **Phase 5 前置项**：`02_屏幕引脚与接线图.md` 与 `06_核心板_原理图与引脚映射.md` 对 U4 是否有背光脚说法冲突，接 SPI 前需回原始原理图定论。

## 最近一次可运行状态

- 命令：`pio run` / `pio run -t upload`（在 `SmartDeskTerminal_freertos/` 下）
- 结果：编译 SUCCESS（Flash 3476B / RAM 44B）；烧录 `** Verified OK **` + `Resetting Target`
- 现象：**PC0 蓝灯不亮**（预期 500ms 闪烁）；但 SWD 读出 PC0 实际输出为精确 1Hz 方波
- 代码：`SmartDeskTerminal_freertos/src/main.c`（168MHz / PLLQ=7 / `-DHSE_VALUE=8000000U` / `SysTick_Handler` 到位）
