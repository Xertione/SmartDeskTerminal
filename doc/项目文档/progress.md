# 进度

- 当前阶段：Phase 2 STM32 基础外设层建立
- 状态：Phase 1 已收尾，准备进 Phase 2
- 最后更新：2026-09-18

## 已完成

### Phase 1：工程初始化与基础运行环境 ✅
- [x] PlatformIO 工程骨架（`framework = stm32cube`，board = black_f407ve）
- [x] 硬件齐备（F407VE 黑板 / ST-Link / ST7789 屏 / USB-C 线）
- [x] 路线决策：PIO + stm32cube HAL，Phase 3 引入 FreeRTOS（ADR-001/002）
- [x] 文档骨架初始化（doc/项目文档/，6 文件 + plan.md）
- [x] LED 引脚查实：PC0（用户 LED 蓝光），原理图 + 官方例程双验证
- [x] 时钟决策：PLLQ=7（ADR-003）/ HSE_VALUE=8MHz 宏（ADR-004）
- [x] 工作区根统一 repo（ADR-005）+ repo 精简方案 X（ADR-007，27 文件）
- [x] LED 闪烁代码 + 编译 + 烧录成功
- [x] 板子可靠性验证：SWD 直读寄存器确认时钟/GPIO/中断全正常（见 T-005）
- [x] 屏幕背光点亮验证（28005 模块接 3.3V，供电回路正常）
- [x] Phase 1 收尾裁决（ADR-006）：蓝灯硬件故障挂起，验证路径升级为寄存器级

### 悬挂项
- 🔴 蓝灯硬件故障（LED 坏/虚焊/走线断）—— 代码已验证正确（ODR 1Hz 方波），待万用表定性
- 🟡 U4 背光脚文档冲突 —— 当前走 28005 路线不影响，Phase 5 若直插 U4 再定论

## 正在做

Phase 2 准备中。待决策：Phase 2 第一步选哪个（见下方决策项）。

## 下一步计划

Phase 2 目标（plan.md）：
- GPIO: LED（已验证，硬件挂起）/ Button（KEY_PC1）
- UART: printf 重定向 + 串口日志
- 理解: GPIO 寄存器 / HAL 封装 / 外设初始化
- 产物: `[INFO] System Init OK / UART Ready`

待用户确认：① Phase 2 第一步选 Button 还是 UART ② 有没有 USB-TTL 转换器（UART 需要）

## 阻塞问题

无（Phase 1 已收尾，Phase 2 待选第一步）。

## 最近一次可运行状态

- 命令：`pio run -t upload`（在 `SmartDeskTerminal_freertos/` 下）
- 结果：烧录成功 + SWD 寄存器验证全正常
- 现象：PC0 蓝灯不亮（硬件故障，代码正确）；屏幕背光点亮正常
- 代码：`SmartDeskTerminal_freertos/src/main.c`（PLLQ=7 / HSE_VALUE 宏 / SysTick_Handler / Error_Handler 快闪）
