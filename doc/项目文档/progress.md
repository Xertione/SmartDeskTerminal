# 进度

- 当前阶段：Phase 1 工程初始化与基础运行环境
- 状态：进行中（阻塞：烧录成功但 LED 不亮）
- 最后更新：2026-09-18

## 已完成

- [x] PlatformIO 工程骨架（`framework = stm32cube`，board = black_f407ve）
- [x] 硬件齐备（F407VE 黑板 / ST-Link / ST7789 屏 / USB-C 线）
- [x] 路线决策：PIO + stm32cube HAL，Phase 3 引入 FreeRTOS（A/C 趋同，见 [decision-log.md](decision-log.md) ADR-002）
- [x] 文档骨架初始化（迁至 `doc/项目文档/`，6 文件）
- [x] LED 引脚查实：**PC0**（用户 LED 蓝光），原理图 + 官方例程双验证
- [x] LED 闪烁代码（[main.c](../../SmartDeskTerminal_freertos/src/main.c)）+ 时钟决策（ADR-003 PLLQ=7 / ADR-004 HSE_VALUE 宏）
- [x] 工作区根统一 repo（方案一，见 ADR-005）
- [x] 首次烧录：`pio run -t upload` 成功（ST-LinkV2 闪三次 + PIO 提示成功）

## 正在做

排查"烧录成功但 PC0 蓝灯不亮"（见 [troubleshooting.md](troubleshooting.md) T-005）。最可能：时钟配置失败进 Error_Handler 死循环（HSE 物理未起振）。

## 下一步计划

1. 改 `Error_Handler` 加 LED 快闪指示（区分"时钟失败"vs"其他"）
2. 重烧，观察现象
3. 若快闪 → 时钟失败 → 查 HSE 起振 / 改用 HSI 验证
4. 定位根因后修，再烧到灯正常 500ms 闪烁 = Phase 1 收尾

## 阻塞问题

PC0 蓝灯不亮。烧录链路 OK（ST-Link 闪三次），代码已入板，但运行现象不符预期。

## 最近一次可运行状态

- 命令：`pio run -t upload`（在 `SmartDeskTerminal_freertos/` 下）
- 结果：烧录成功（ST-LinkV2 闪三次 + PIO 终端提示 success）
- 现象：**PC0 蓝灯不亮**（预期应 500ms 闪烁）
- 代码：`SmartDeskTerminal_freertos/src/main.c`（PLLQ=7 / HSE_VALUE 宏 / SysTick_Handler 到位）
