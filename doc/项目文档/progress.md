# 进度

> 维护语义：**覆盖**。永远只反映当前状态，过期内容直接删掉，不往下堆历史。
> 历史决策看 `decision-log.md`，已踩的坑看 `troubleshooting.md`。

- 当前阶段：**Phase 7 触摸驱动 ✅ 完成** → 下一步 Phase 3 FreeRTOS
- 状态：LCD + 触摸全链路实机验证通过，裸机 UI（按钮 + 坐标 + 计数）可用
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
- [x] T-006 基线验证（不接线=1）：1001 次采样零抖动
- [x] T-006 功能验证（短接=0）：**用户明确放弃** → 按「GPIO 半通过 / UART 半跳过」结账
- [x] T-007 UART printf：**跳过**（无 CH340，ADR-008）

### Phase 5：LCD 驱动 ✅ 实机验证通过
- [x] 硬件接线（显示 8 根）—— 施工单见 `wiring.md` §4
- [x] T-008 STEP1：SPI3 通信层 + 背光点亮（判据 1/2/4 通过）
- [x] T-008 STEP2：ST7789 初始化序列（15 步，与厂方 TN Code 逐条一致）+ 全屏填充 → **红→绿→蓝交替正常**
- [x] T-008 STEP3：8×16 字库（`bsp/font.h`，96 字形）+ `LCD_DrawChar/DrawString` → **文字清晰可读**

### Phase 7：触摸驱动 ✅ 实机验证通过
- [x] 硬件接线（触摸 5 根）—— 施工单见 `wiring.md` §5
- [x] T-009 XPT2046 软件 SPI 驱动（`bsp/touch.c|h`）+ 按钮 UI
      （绿色 `PRESS ME` → 点中闪红 `HIT!` 200ms → 恢复绿 + Hits 计数 + 实时坐标）
- [x] 读位时序修复：控制字后补 1 个"转换/BUSY"时钟（ADR-011，修复前数值减半）
- [x] 校准区间固定为实测值 `200~3800`（不做运行时自适应，ADR-011）
- [x] 按钮 hit-test 改「锁存式」（一次按压只触发一次，ADR-011）
- [x] 固件版本标识：`FW_VERSION` + 编译时间（`__DATE__`/`__TIME__`）显示在屏顶部

---

## 悬挂项

- 🔴 **蓝灯硬件故障** —— 代码侧已完全排除（寄存器级验证通过），待万用表定性
- 🟡 **U4 背光脚文档冲突** —— 走 28005 转接板路线不受影响，仅在直插 U4 时才需定论（ADR-006）
- 🟡 **Y 轴校准沿用 X 的 `200~3800`** —— Y 未单独实测，约 20 像素误差，当前已接受
- 🟡 **UART printf 通道** —— 跳过（ADR-008）。FreeRTOS 时序 bug 查不动时用 CH340 补
- ⚪ `.pio/build_stale_0921`、`.pio/build_stale_s2` 废弃构建目录待手动删

---

## 下一步计划

按 ADR-009 调整后的顺序：

1. **Phase 3 FreeRTOS 引入** —— 任务创建 / 调度 / SysTick 交接（**下一个要做的**）
2. Phase 4 任务通信 —— 队列 / 信号量
3. Phase 6 LVGL + 触摸输入设备
4. Phase 8 USB CDC（顺带可解决 printf 通道）

---

## 最近一次可运行状态

- 命令：`pio run -t upload`（在 `SmartDeskTerminal_freertos/` 下）
- 构建：SUCCESS，**0 warning**，Flash **8768 B** / RAM **156 B**
- 屏上可见：`SmartDesk v0.7.1` + 编译时间 / 设备信息 / SYSCLK / 实时坐标 / Hits
  / 绿色 `PRESS ME` 按钮（可点中并计数）
- 代码：`src/main.c` + `src/bsp/{key,lcd,touch}.c|h` + `src/bsp/font.h`
- 对应提交：`c67d8fc`
