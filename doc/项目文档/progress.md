# 进度

> 维护语义：**覆盖**。永远只反映当前状态，过期内容直接删掉，不往下堆历史。
> 历史决策看 `decision-log.md`，已踩的坑看 `troubleshooting.md`。

- 当前阶段：**Phase 8 花屏/无 COM 口 未收敛；已把"静默失败"改成"屏上可见"，待实机验证。**
  若下一步仍不通过 → **回退到 Phase 6（`git checkout e545ec3`，标签 `phase6-verified`）**
- 状态：编译 SUCCESS（327 Compiling / 1 Linking，0 error 0 warning）；RAM 52.0% / Flash 26.3%
- 最后更新：2026-09-26

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

### Phase 8：USB CDC 双向通道 + 命令雏形（代码完成，**已修 3 缺陷，待实机复验**，ADR-014/015）
- [x] 集成：复制 USB Device Library CDC+Core 中间件到 `lib/usb_device/`（library.json 限制只编 CDC+Core）
- [x] 用户层：`lib/usb_device/{usbd_conf,usbd_desc,usbd_cdc_if}.{c,h}`（HAL 钩子 + 描述符 + CDC 接口）
- [x] 顶层接口 `src/bsp/usb_cdc.{c,h}`：`USB_CDC_Init/Send/Printf/Poll/IsConfigured`
- [x] 命令解析 `src/cmd.{c,h}`：hello/version/hits/clear/ping/help（行式文本，\r\n 结尾）
- [x] Agent↔UI 队列 `src/agent_msg.{c,h}`：队列回归（Phase 6 退役的队列在此重启）
- [x] 任务架构：Task_LVGL(prio3) + Task_Agent(prio2)；Agent 不直接碰 UI，走队列中转
- [x] USB 中断优先级 6（≥5 可调 RTOS API，保守做法）
- [x] **首轮实机失败并定位根因（T-007）**：烧录后花屏 + PC 无虚拟 COM 口
- [x] 修复①：PCD 句柄改名 `hpcd_USB_OTG_FS` —— 原与 USBD 句柄同名，被 `-fcommon` 静默合并成同一地址（ELF 实证：单符号 1252B），两结构体互相覆写 → 野指针写
- [x] 修复②：补上缺失的 `HAL_PCD_MspInit` —— PA11/PA12 从未配成 `GPIO_AF10_OTG_FS`，引脚停在浮空复位态
- [x] 修复③：`USBD_malloc` 由 newlib `malloc` 改为静态 arena（768B，8 字节对齐）；`CDC_Control_FS` 补 `GET/SET_LINE_CODING`
- [x] 加固④：`platformio.ini` 加 `-fno-common`，让同类别名 bug 变成链接期硬错误
- [x] 加固⑤：`USB_CDC_Printf` 的共享静态缓冲加临界区保护（原被两个任务并发调用）
- [x] 编译 SUCCESS：Flash 137704B（26.3%）/ RAM 68212B（52.0%）/ 0 错误 0 警告
- [ ] **待办：实机烧录复验**（验收标准见下方）

### Phase 6：LVGL 移植 ✅ 实机验证通过（2026-09-25，ADR-013）
- [x] 集成：`lib_deps = lvgl/lvgl@8.3.11` + `-DLV_CONF_INCLUDE_SIMPLE` + `src/lv_conf.h`
- [x] 内存：LVGL 静态池 32KB + 绘制缓冲 240×40（1/8 屏）；时基 LV_TICK_CUSTOM 挂 FreeRTOS tick
- [x] 移植层 `src/lvgl_port.c|h`：flush_cb→`LCD_WriteArea`（新块写函数）+ read_cb→`Touch_Read`
- [x] UI `src/ui.c|h`：复刻 v0.8 全部信息（版本/Build/SYSCLK/按钮/Hits/内存/心跳）+ 右下角 perf monitor
- [x] 架构：三任务+队列 → **单任务** Task_LVGL（lv_timer_handler + 5ms sleep），队列 Phase 8 回归
- [x] 顺带清账：SPI3 提速 /16→/4（10.5MHz）；补 `SCB->VTOR`；`Touch_Read` 忙等改 vTaskDelay；删调试打点
- [x] 编译 SUCCESS：RAM 47.3%（62.0KB）/ Flash 24.0%（126.1KB）/ 0 警告
- [x] **实机验证通过（2026-09-25 用户确认）**：验收 5 条全过 —— 界面正常 / 按钮可按 / FPS 显示 / HB 每秒刷新 / 颜色正常
  - ✅ 因此 **SPI 10.5MHz 提速本身没问题**（已实证），后续花屏不应再怀疑这里

---

## 回退与分支方案（用户 2026-09-26 授权：修不好就回退 Phase 6）

| 目标 | 命令 |
|---|---|
| **回到 Phase 6（最后确认可用的一版）** | `git checkout e545ec3`（等价于标签 `phase6-verified`） |
| 回到最新（含 Phase 8 与 USB 修复） | `git checkout master` |
| 临时留一手（不切换、只把 Phase 6 的源码取回工作区） | `git checkout e545ec3 -- SmartDeskTerminal_freertos/src SmartDeskTerminal_freertos/platformio.ini` |

- **Phase 6 那一版的能力边界**：LVGL 界面 + 触摸按钮 + Hits 计数 + 心跳 + FPS，**没有任何 USB**。
  若确认不再需要 USB，Phase 6 就是一个干净、已实机验证的基线。
- ⚠️ 用 `git checkout e545ec3` 会进入 detached HEAD，**改动前先 `git status` 确认没有未提交内容**。
- ⚠️ 回退后 `.pio/build/` 里的旧产物与源码不匹配，建议先停掉调试会话（`Shift+F5`）再重编。

---

## 悬挂项

- 🔴 **蓝灯硬件故障** —— 代码侧已完全排除，待万用表定性
- 🟡 **U4 背光脚文档冲突** —— 走 28005 转接板路线不受影响
- 🟡 **Y 轴校准沿用 X 的 `200~3800`** —— 约 20 像素误差，已接受
- 🟡 **UART printf 通道** —— 跳过（ADR-008）
- ✅ **SPI3 10.5MHz 提速** —— 已由 Phase 6 实机验证排除嫌疑（颜色正常）。后续再出花屏**不要**先怀疑这里。
- 🟡 **训练材料已降级重排**：`training-invpc.md` 原版把"读二进制 / `nm` / `objdump`"当主线，用户明确表示不可执行（打击学习积极性）。2026-09-25 已重构：**核武器类操作改由 AI 代跑，用户只做判断推理**；新增 T0-A 项目结构表 + T0-B 完整调用链表（原 T4 的内容由 AI 直接给出）。
- 🟡 **`.pio/build/build_stale_*` 目录堆积** —— safe-delete 绕过手段的副作用（`mv` 不算删除），需定期人工清理。
- 🟡 **`troubleshooting.md` 编号冲突** —— `T-005` 出现两次（"已发生"节讲 PC0 蓝灯、"预期坑预警"节讲 safe-delete 表现④）。同一文档内编号不可复用，待重编号。
- 🟡 **workbuddy 记忆随会话演进** —— 同日多会话并行时，`MEMORY.md` 可能成为陈旧副本，以 `progress.md` 为准。

---

## 下一步计划

1. **Phase 8 实机复验**（用户烧录 + PC 端串口工具测试，验收标准见下）
2. 通信协议格式定稿（当前是"行式纯文本命令"，需扩展为 JSON 事件通道）
3. Phase 9 PC Agent（**hooks 触发机制待定**）
4. Phase 10 MVP 整合

---

## 最近一次可运行状态

- 命令：`pio run -t upload`（在 `SmartDeskTerminal_freertos/` 下）
- 构建：SUCCESS（327 Compiling / 1 Linking，0 error 0 warning），Flash **137704 B（26.3%）** / RAM **68212 B（52.0%）**
- ⚠️ **烧录前必看**：`pio run` 报 SUCCESS 只代表**退出码**，不代表编译发生。
  必须同时满足 ① 日志有 `Compiling`/`Linking` 行 ② `firmware.bin` 的 mtime 是刚才。
  若 SUCCESS 但零编译 → 见 `troubleshooting.md` T-005。
- **实机验收标准**（Phase 8，全过才算结账）：
  1. **烧录后屏上 LVGL 界面正常显示**（无花屏 —— 这是 T-007 修复的第一判据）
  2. USB 数据线连核心板 Type-C 口 → PC 设备管理器出现新 COM 口（VID=1234 PID=5678）
     - ⚠️ 必须用**能传数据的线**（部分 Type-C 线只有电源线，无 D+/D-）
  3. PC 串口工具（PuTTY/Arduino 串口监视器）打开 COM 口，波特率任意（CDC 虚拟串口不限速）
  4. 收到 `SmartDesk v0.9.1-cdc ready` + `Type 'help' for commands` 欢迎语
  5. 键入 `hello` → 收到 `Hello SmartDesk!`；`version` → 收到版本+Build；`ping` → 收到 `pong`
  6. 键入 `clear` → 屏上 Hits 计数归零；`hits` → 串口收到 `hits=N`
  7. 按屏上 PRESS ME 按钮 → Hits 增长 → 键入 `hits` 确认数值同步
- 代码：`src/main.c` + `src/{lv_conf,lvgl_port,ui,lvgl_tick_source}.{c,h}` + `src/bsp/{key,lcd,touch,usb_cdc}.c|h` + `src/{cmd,agent_msg}.{c,h}` + `lib/usb_device/` + `src/FreeRTOSConfig.h` + `extra_script.py`
- 对应提交：`bd9a117`（T-007 三缺陷修复 + `-fno-common` 加固 + `lib/usb_device/` 入库）
