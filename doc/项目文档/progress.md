# 进度

- 当前阶段：Phase 5 LCD 驱动移植（提前，ADR-009）
- 状态：Phase 5 STEP1 进行中（接线完成，待编译烧录验证 SPI3 通信）
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

### Phase 2：BSP 基础外设层（部分完成，ADR-008）
- [x] T-006 Button：key.c/key.h 代码完成（PC1 上拉输入），编译烧录 Verified OK
- [x] T-006 基线验证（不接线=1）：1001 次采样零抖动
- [ ] T-006 功能验证（短接=0）：**挂起**（ST-Link 未连接时未测）
- [ ] T-007 UART printf：**跳过**（无 CH340，ADR-008），整个 uart.c/h 不建

### 悬挂项
- 🔴 蓝灯硬件故障（LED 坏/虚焊/走线断）—— 代码已验证正确（ODR 1Hz 方波），待万用表定性（万用表已购，到货中）
- 🟡 U4 背光脚文档冲突 —— 当前走 28005 路线不影响，Phase 5 若直插 U4 再定论
- 🟡 T-006 按键功能验证（短接=0）—— 5 分钟可补，触摸替代按钮后可能不再需要
- 🟡 UART printf 通道 —— 跳过（ADR-008），FreeRTOS 时序 bug 查不动时用 CH340 补（已购，到货中）

## 正在做

### Phase 5：LCD 驱动移植（提前，ADR-009）
- [x] 硬件接线完成（屏幕 SPI 7~8 根 + 触摸 5 根，母对母杜邦线）
- [ ] T-008 STEP1：SPI3 初始化 + SWD 验证 SPI 能发数据
- [ ] T-008 STEP2：ST7789 初始化序列
- [ ] T-008 STEP3：点亮第一个像素/矩形
- [ ] T-008 STEP4：字符显示
- [ ] T-008 STEP5：显示 Hello SmartDesk

## 下一步计划

Phase 5 LCD 跑通后顺序（ADR-009 调整）：
1. Phase 7 触摸驱动（裸机 XPT2046，替代物理按钮）
2. Phase 3 FreeRTOS 引入
3. Phase 4 任务通信
4. Phase 6 LVGL + 触摸输入设备

## 阻塞问题

无（接线完成，待编译烧录验证）。

## 最近一次可运行状态

- 命令：`pio run -t upload`（在 `SmartDeskTerminal_freertos/` 下）
- 结果：Phase 2 T-006 编译烧录 Verified OK（Flash 3588B），key_state 基线=1
- 代码：`SmartDeskTerminal_freertos/src/main.c` + `src/bsp/key.c|h`
- 待验证：Phase 5 LCD SPI3 通信（T-008 STEP1）
