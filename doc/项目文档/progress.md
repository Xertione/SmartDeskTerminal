# 进度

> 维护语义：**覆盖**。永远只反映当前状态，过期内容直接删掉，不往下堆历史。
> 历史决策看 `decision-log.md`，已踩的坑看 `troubleshooting.md`。

- 当前阶段：**Phase 8 USB CDC 代码完成，待实机验证**
- 状态：编译 SUCCESS（RAM 50.9% / Flash 26.2%）；USB CDC 双向通道 + 命令解析就绪
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

### Phase 8：USB CDC 双向通道 + 命令雏形（代码完成，待实机验证，ADR-014）
- [x] 集成：复制 USB Device Library CDC+Core 中间件到 `lib/usb_device/`（library.json 限制只编 CDC+Core）
- [x] 用户层：`lib/usb_device/{usbd_conf,usbd_desc,usbd_cdc_if}.{c,h}`（HAL 钩子 + 描述符 + CDC 接口）
- [x] 顶层接口 `src/bsp/usb_cdc.{c,h}`：`USB_CDC_Init/Send/Printf/Poll/IsConfigured`
- [x] 命令解析 `src/cmd.{c,h}`：hello/version/hits/clear/ping/help（行式文本，\r\n 结尾）
- [x] Agent↔UI 队列 `src/agent_msg.{c,h}`：队列回归（Phase 6 退役的队列在此重启）
- [x] 任务架构：Task_LVGL(prio3) + Task_Agent(prio2)；Agent 不直接碰 UI，走队列中转
- [x] USB 中断优先级 6（≥5 可调 RTOS API，保守做法）
- [x] 编译 SUCCESS：RAM 50.9%（66.7KB）/ Flash 26.2%（137.5KB）/ 0 错误 1 警告(unused,已修)
- [ ] **待办：实机烧录验证**（验收标准见下方）

### Phase 6：LVGL 移植 ✅ 实机验证通过（2026-09-25，ADR-013）
- [x] 集成：`lib_deps = lvgl/lvgl@8.3.11` + `-DLV_CONF_INCLUDE_SIMPLE` + `src/lv_conf.h`
- [x] 内存：LVGL 静态池 32KB + 绘制缓冲 240×40（1/8 屏）；时基 LV_TICK_CUSTOM 挂 FreeRTOS tick
- [x] 移植层 `src/lvgl_port.c|h`：flush_cb→`LCD_WriteArea`（新块写函数）+ read_cb→`Touch_Read`
- [x] UI `src/ui.c|h`：复刻 v0.8 全部信息（版本/Build/SYSCLK/按钮/Hits/内存/心跳）+ 右下角 perf monitor
- [x] 架构：三任务+队列 → **单任务** Task_LVGL（lv_timer_handler + 5ms sleep），队列 Phase 8 回归
- [x] 顺带清账：SPI3 提速 /16→/4（10.5MHz）；补 `SCB->VTOR`；`Touch_Read` 忙等改 vTaskDelay；删调试打点
- [x] 编译 SUCCESS：RAM 47.3%（62.0KB）/ Flash 24.0%（126.1KB）/ 0 警告
- [x] **实机验证通过（2026-09-25 用户确认）**：验收 5 条全过 —— 界面正常 / 按钮可按 / FPS 显示 / HB 每秒刷新 / 颜色正常（字节序 + SPI 10.5MHz 提速双双通过）

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

1. **Phase 8 实机验证**（用户烧录 + PC 端串口工具测试，验收标准见下）
2. Phase 9 PC Agent
3. Phase 10 MVP 整合

---

## 最近一次可运行状态

- 命令：`pio run -t upload`（在 `SmartDeskTerminal_freertos/` 下）
- 构建：SUCCESS，Flash **137532 B（26.2%）** / RAM **66700 B（50.9%）**
- **实机验收标准**（Phase 8，全过才算结账）：
  1. 烧录后屏上 Phase 6 界面照常显示（LVGL 不受影响）
  2. USB 数据线连核心板 Type-C 口 → PC 设备管理器出现新 COM 口（VID=1234 PID=5678）
  3. PC 串口工具（PuTTY/Arduino 串口监视器）打开 COM 口，波特率任意（CDC 虚拟串口不限速）
  4. 收到 `SmartDesk v0.9.1-cdc ready` + `Type 'help' for commands` 欢迎语
  5. 键入 `hello` → 收到 `Hello SmartDesk!`；`version` → 收到版本+Build；`ping` → 收到 `pong`
  6. 键入 `clear` → 屏上 Hits 计数归零；`hits` → 串口收到 `hits=N`
  7. 按屏上 PRESS ME 按钮 → Hits 增长 → 键入 `hits` 确认数值同步
- 代码：`src/main.c` + `src/{lv_conf,lvgl_port,ui,lvgl_tick_source}.{c,h}` + `src/bsp/{key,lcd,touch,usb_cdc}.c|h` + `src/{cmd,agent_msg}.{c,h}` + `lib/usb_device/` + `src/FreeRTOSConfig.h` + `extra_script.py`
- 对应提交：（本次 commit 后回填）
