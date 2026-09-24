# 进度

> 维护语义：**覆盖**。永远只反映当前状态，过期内容直接删掉，不往下堆历史。
> 历史决策看 `decision-log.md`，已踩的坑看 `troubleshooting.md`。

- 当前阶段：**Phase 6 LVGL 移植代码完成，待实机验证**
- 状态：编译 SUCCESS（RAM 47.3% / Flash 24.0%）；LVGL 8.3.11 + 单任务架构就绪
- 最后更新：2026-09-25

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

### Phase 3+4：FreeRTOS 集成 + 任务通信 ✅ 实机验证通过（2026-09-23）
- [x] FreeRTOS 集成方案决策（ADR-012：PIO lib_deps + ARM_CM4F + heap_4）
- [x] FreeRTOSConfig.h 配置（168MHz / 1ms tick / 8KB 堆 / HSE 宏 / SysTick 共存）
- [x] extra_script.py 强制 LINKFLAGS `-mfloat-abi=hard`（解决 board 默认 softfp 链接冲突）
- [x] 三任务架构：Task_LED（优先级1）/ Task_Touch（优先级3）/ Task_LCD（优先级2）
- [x] 队列通信：xQueueOverwrite（Touch→LCD，非阻塞）
- [x] SysTick 共存：HAL_IncTick + xPortSysTickHandler（调度器启动后才调 FreeRTOS）
- [x] **中断优先级宏未左移导致"启动第一个任务即 HardFault"—— 已定位并修复，见 T-002**
- [x] 实机验证：烧录后三任务正常调度（LED 心跳 / 触摸 / 屏幕刷新）

### Phase 6：LVGL 移植（代码完成，待实机验证，ADR-013）
- [x] 集成：`lib_deps = lvgl/lvgl@8.3.11` + `-DLV_CONF_INCLUDE_SIMPLE` + `src/lv_conf.h`
- [x] 内存：LVGL 静态池 32KB + 绘制缓冲 240×40（1/8 屏）；时基 LV_TICK_CUSTOM 挂 FreeRTOS tick
- [x] 移植层 `src/lvgl_port.c|h`：flush_cb→`LCD_WriteArea`（新块写函数）+ read_cb→`Touch_Read`
- [x] UI `src/ui.c|h`：复刻 v0.8 全部信息（版本/Build/SYSCLK/按钮/Hits/内存/心跳）+ 右下角 perf monitor
- [x] 架构：三任务+队列 → **单任务** Task_LVGL（lv_timer_handler + 5ms sleep），队列 Phase 8 回归
- [x] 顺带清账：SPI3 提速 /16→/4（10.5MHz）；补 `SCB->VTOR`；`Touch_Read` 忙等改 vTaskDelay；删调试打点
- [x] 编译 SUCCESS：RAM 47.3%（62.0KB）/ Flash 24.0%（126.1KB）/ 0 警告
- [ ] **待办：实机烧录验证**（验收标准见下方"最近一次可运行状态"）

---

## 悬挂项

- 🔴 **蓝灯硬件故障** —— 代码侧已完全排除，待万用表定性
- 🟡 **U4 背光脚文档冲突** —— 走 28005 转接板路线不受影响
- 🟡 **Y 轴校准沿用 X 的 `200~3800`** —— 约 20 像素误差，已接受
- 🟡 **UART printf 通道** —— 跳过（ADR-008）
- 🟡 **SPI3 提速到 10.5MHz 未经长时验证** —— 实机若花屏/偏色，第一怀疑对象（回退 /8=5.25MHz 对照，见 ADR-013）
- 🟡 **训练材料已降级重排**：`training-invpc.md` 原版把"读二进制 / `nm` / `objdump`"当主线，用户明确表示不可执行（打击学习积极性）。2026-09-25 已重构：**核武器类操作改由 AI 代跑，用户只做判断推理**；新增 T0-A 项目结构表 + T0-B 完整调用链表（原 T4 的内容由 AI 直接给出）。

---

## 下一步计划

1. **Phase 6 实机验证**（用户烧录，验收标准见下）
2. Phase 8 USB CDC（顺带可解决 printf 通道；Agent 任务 + 队列回归）
3. Phase 9 PC Agent
4. Phase 10 MVP 整合

---

## 最近一次可运行状态

- 命令：`pio run -t upload`（在 `SmartDeskTerminal_freertos/` 下）
- 构建：SUCCESS，0 warning，Flash **126068 B（24.0%）** / RAM **61988 B（47.3%）**
- **实机验收标准**（Phase 6，全过才算结账）：
  1. 屏上出现深色背景界面：`SmartDesk v0.9.0-lvgl` + Build 时间 + 环境/系统信息行
  2. `PRESS ME` 按钮可按（按下有变色反馈），点击后 `Hits: n` 计数增长
  3. 右下角 perf monitor 显示 FPS/CPU（30fps 左右为正常）
  4. `FRTh` / `LVMem` / `HB` 行每秒刷新（HB 数字递增 = lv_timer 活着）
  5. 颜色正常（无整体反相 / 无花屏 → 字节序与 SPI 提速双双通过）
- 代码：`src/main.c` + `src/{lv_conf,lvgl_port,ui,lvgl_tick_source}.{c,h}` + `src/bsp/{key,lcd,touch}.c|h` + `src/FreeRTOSConfig.h` + `extra_script.py`
- 对应提交：（本次 commit 后回填）
