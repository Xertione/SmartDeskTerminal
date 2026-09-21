# 进度

- 当前阶段：Phase 2 STM32 基础外设层建立（**部分完成，待入口决策**）
- 状态：Phase 2 代码已写完烧录，但两项验证挂起；Phase 3 入口决策待定
- 最后更新：2026-09-21

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

### Phase 2：BSP 层建立（部分完成 🟡）

#### T-006 Button（KEY_PC1）—— 代码完成，功能验证挂起
- [x] BSP 模块 1 建立：`src/bsp/key.h` + `src/bsp/key.c`（Key_Init / Key_Read）
- [x] main.c 集成：`#include "bsp/key.h"` + `Key_Init()` 在 while 前 + `key_state` 轮询
- [x] 编译 SUCCESS **0 warning**，Flash 3588B / RAM 48B；烧录 `Verified OK`
- [x] **基线验证（不接线=1）通过**：1001 次采样全 `key_state=1`，`GPIOC->IDR bit1=1`，零抖动
- [ ] **功能验证（短接 PC1↔GND=0）挂起**：09:56 查无 `VID_0483`，ST-Link 未连接，6 轮 openocd 全 `open failed`。等用户重插 ST-Link + 短接测试

#### T-007 UART printf —— 跳过（ADR-008）
- 用户无 CH340 / USB-TTL 转换器，串口打印通道不可用，决定跳过
- UART 模块（`src/bsp/uart.c|h`）未建立
- 代价见 ADR-008：FreeRTOS 调试将缺 printf 通道，靠 SWD 读变量

### 悬挂项
- 🔴 蓝灯硬件故障（LED 坏/虚焊/走线断）—— 代码已验证正确（ODR 1Hz 方波），待万用表定性
- 🟡 U4 背光脚文档冲突 —— 当前走 28005 路线不影响，Phase 5 若直插 U4 再定论
- 🟡 T-006 功能验证（短接=0）—— 5 分钟可补，插 ST-Link + 短接 PC1↔GND 读 key_state
- 🟡 UART printf 通道缺失 —— T-007 跳过，Phase 3-7 FreeRTOS/LVGL 调试缺打印通道（ADR-008）

## 正在做

Phase 2 入口决策。待用户三选一：
- A. 硬进 Phase 3 FreeRTOS（接受无 printf 调试痛苦）
- B. 先 5 分钟补完 T-006 功能验证，再进 Phase 3
- C. 补做 UART 自发自收验证（不依赖 CH340，H1 pin5/6 短接读计数器），再进 Phase 3

## 下一步计划

Phase 3 目标（plan.md）：从裸机进入实时系统
- main → Scheduler → LED Task / UART Task / System Task
- 学习：Task / Priority / Delay / Tick
- 代码结构：task_led.c / task_uart.c / task_system.c

⚠️ **Phase 3 关键风险（ADR-001 后果段预言兑现）**：
- FreeRTOS 在 PIO 集成需手动配：configTOTAL_HEAP_SIZE / configMAX_SYSCALL_INTERRUPT_PRIORITY / SysTick 与 HAL 时基冲突 / PendSV-SVC 优先级
- 这些是配置层坑，AI 写代码绕不过，必须自己理解配对
- 无 UART printf 通道，FreeRTOS 时序问题调试会非常痛苦（SWD 断点冻结调度器，看到的现场≠真实运行时序）
- 退路（ADR-001）：若集成受阻 >2 小时，考虑回退 CubeMX（路线 B）

## 阻塞问题

无（Phase 2 部分完成，Phase 3 入口待决策）。

## 最近一次可运行状态

- 命令：`pio run -t upload`（在 `SmartDeskTerminal_freertos/` 下）
- 结果：烧录成功（Verified OK）
- 代码：`SmartDeskTerminal_freertos/src/main.c` + `src/bsp/key.c|h`
- 现象：PC0 蓝灯不亮（硬件故障，代码正确）；`key_state` 基线=1 已证，短接=0 待验证
