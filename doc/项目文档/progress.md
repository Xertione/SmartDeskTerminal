# 进度

> 维护语义：**覆盖**。永远只反映当前状态，过期内容直接删掉，不往下堆历史。
> 历史决策看 `decision-log.md`，已踩的坑看 `troubleshooting.md`。

- 当前阶段：**Phase 3 FreeRTOS 集成完成（编译通过）→ 待实机验证**
- 状态：FreeRTOS (ARM_CM4F + heap_4) 集成编译 SUCCESS，三任务架构（LED/Touch/LCD + 队列通信）代码就绪
- 最后更新：2026-09-22

---

## 已完成

### Phase 1：工程初始化与基础运行环境 ✅
- [x] PlatformIO 工程骨架（`framework = stm32cube`，board = black_f407ve）
- [x] 路线决策：PIO + stm32cube HAL（ADR-001/002）
- [x] LED 引脚查实：PC0；时钟：PLLQ=7（ADR-003）/ `HSE_VALUE=8MHz` 宏（ADR-004）
- [x] 工作区根统一 repo（ADR-005）+ repo 精简方案 X（ADR-007）
- [x] LED 闪烁编译烧录成功；SWD 直读寄存器确认时钟/GPIO/中断全正常（T-005）
- [x] 屏幕背光点亮验证（28005 模块接 3.3V）
- [x] 收尾裁决（ADR-006）：蓝灯硬件故障挂起，验证路径升级为寄存器级

### Phase 2：BSP 基础外设层（结账，ADR-010）
- [x] T-006 按键 BSP：`key.c|h`（PC1 上拉输入）完成，编译烧录 Verified OK
- [x] T-006 功能验证：**用户明确放弃** → 按「GPIO 半通过 / UART 半跳过」结账
- [x] T-007 UART printf：**跳过**（无 CH340，ADR-008）

### Phase 5：LCD 驱动 ✅ 实机验证通过
- [x] T-008 STEP1/2/3：SPI3 通信层 + ST7789 初始化序列 + 字符渲染

### Phase 7：触摸驱动 ✅ 实机验证通过
- [x] T-009 XPT2046 软件 SPI 驱动 + 按钮 UI（锁存式 hit-test，ADR-011）

### Phase 3+4：FreeRTOS 集成 + 任务通信（编译通过，待实机）
- [x] FreeRTOS 集成方案决策（ADR-012：PIO lib_deps + ARM_CM4F + heap_4）
- [x] FreeRTOSConfig.h 配置（168MHz / 1ms tick / 8KB 堆 / 4bit 优先级 / SysTick 共存）
- [x] extra_script.py 强制 LINKFLAGS `-mfloat-abi=hard`（解决 board 默认 softfp 链接冲突）
- [x] 三任务架构：Task_LED（优先级1）/ Task_Touch（优先级3）/ Task_LCD（优先级2）
- [x] 队列通信：xQueueOverwrite（Touch→LCD，非阻塞）
- [x] SysTick 共存：HAL_IncTick + xPortSysTickHandler（调度器启动后才调 FreeRTOS）
- [x] 编译 SUCCESS：Flash 14296B / RAM 8772B（含 8KB 堆）
- [ ] 实机验证：烧录 + 看三任务调度正常

---

## 悬挂项

- 🔴 **蓝灯硬件故障** —— 代码侧已完全排除，待万用表定性
- 🟡 **U4 背光脚文档冲突** —— 走 28005 转接板路线不受影响
- 🟡 **Y 轴校准沿用 X 的 `200~3800`** —— 约 20 像素误差，已接受
- 🟡 **UART printf 通道** —— 跳过（ADR-008），FreeRTOS 时序 bug 查不动时用 CH340 补

---

## 下一步计划

1. **Phase 3+4 实机验证**（T-010）—— 烧录 + 看三任务调度正常
2. Phase 6 LVGL + 触摸输入设备
3. Phase 8 USB CDC（顺带可解决 printf 通道）
4. Phase 9 PC Agent
5. Phase 10 MVP 整合

---

## 最近一次可运行状态

- 命令：`pio run -t upload`（在 `SmartDeskTerminal_freertos/` 下）
- 构建：SUCCESS，0 warning，Flash **14296 B** / RAM **8772 B**（含 FreeRTOS + 8KB 堆）
- 屏上可见：`SmartDesk v0.8.0-rtos` + RTOS 信息 / Tasks 行 / 触摸按钮 / Heap free
- 代码：`src/main.c` + `src/bsp/{key,lcd,touch}.c|h` + `src/bsp/font.h` + `src/FreeRTOSConfig.h` + `extra_script.py`
- 对应提交：`2b7cbd4`
