# Decision Log

> 每条必写"后果"段。面试官必追问"这方案有什么代价"，缺这段的 ADR 是半成品。

## ADR-001：framework 路线选 C（PIO + HAL + FreeRTOS 库混合）

- 日期：2026-09-17
- 背景：plan.md 原写 HAL 路线，但误建了 `framework = zephyr` 工程（Zephyr 本身是 RTOS，与 plan 的"引入 FreeRTOS"冲突）。废弃后重建为 `framework = stm32cube`（HAL）。FreeRTOS 怎么引入有三条路：
  - A：纯 PIO + HAL，FreeRTOS 留到 Phase 3 用 `platformio lib install` 手加
  - B：CubeMX GUI 生成 F407+FreeRTOS+外设，一步到位（生成 .ioc + Core/Drivers/Middlewares）
  - C：PIO + HAL + FreeRTOS 库混合（保留 PIO 工作流，手加 FreeRTOS package）
- 决定：选 **C**
- 原因：用户希望"AI 快速写好代码验证 → 自己再学习复刻"，保留 VS Code + PlatformIO 工作流，不另学 CubeMX GUI；同时手写 HAL 练寄存器理解。
- 后果：
  - **优点**：保留单一 PIO 工作流；HAL 手写练底层；FreeRTOS 在 Phase 3 用库形式加，可控
  - **代价（关键风险）**：FreeRTOS 在 PIO 里集成需手动配置——`configTOTAL_HEAP_SIZE`（堆大小）、`configMAX_SYSCALL_INTERRUPT_PRIORITY`（可屏蔽中断优先级）、SysTick 与 HAL 时基冲突、PendSV/SVC 优先级。**这些是配置层坑，AI 写代码绕不过去，必须自己理解并配对**。Zest 之前明确标 C 为"新手坑最多、不推荐"，用户已知风险仍选 C，理由是优先保留 PIO 工作流 + 学习复刻策略。
  - **后续触发**：到 Phase 3 真正引入 FreeRTOS 时，若集成受阻 >2 小时，考虑回退到 B（迁 CubeMX）。

## ADR-002：A 与 C 路线趋同，不再纠结标签

- 日期：2026-09-17
- 背景：用户指出 A（PIO+HAL，FreeRTOS 后期手加）与 C（PIO+HAL+FreeRTOS 库混合）实操趋同——A 起步到 Phase 3 加 FreeRTOS 后就是 C，区别只在引入时机的标签，对 Phase 1-2 实操无影响。
- 决定：不再用 A/C 标签，统一表述为「PIO + stm32cube HAL，Phase 3 用库方式引入 FreeRTOS」。
- 原因：用户判断正确，Phase 1-2 阶段两者完全相同（纯 HAL），标签之争无意义。
- 后果：
  - **优点**：路线表述简化，不再为 A/C 标签纠结
  - **代价（不变）**：FreeRTOS 集成配置坑（堆/中断优先级/SysTick）依然存在，与叫 A 还是 C 无关——Phase 3 必然遇到。ADR-001 的退路（受阻 >2h 迁 CubeMX）仍有效。

## ADR-003：PLLQ 取 7（48MHz）而非官方例程的 4（84MHz）

- 日期：2026-09-18
- 背景：官方例程 `1.LED闪烁/Core/Src/main.c` 写 PLLQ=4（→84MHz），但同目录 `0.CubeMX配置参考/407VE.ioc` 写 PLLQ=7（→48MHz）。参考手册要求 PLLQ 输出 ≤48MHz 才能供 USB。
- 决定：取 **PLLQ=7**（48MHz）
- 原因：Phase 8 要用 USB CDC，需 48MHz 时钟；Phase 1 不受影响（PLLQ 不参与 SYSCLK 计算），提前取 7 避免 Phase 8 再改时钟。
- 后果：
  - **优点**：时钟一次配对，Phase 8 直接用 48MHz
  - **代价**：与官方 LED 例程不一致（例程 4），对照例程时要注意 PLLQ 差异；48MHz 精度依赖 HSE 8MHz 准确（见 ADR-004）

## ADR-004：HSE_VALUE 编译宏覆盖为 8MHz

- 日期：2026-09-18
- 背景：PlatformIO `framework-stm32cubef4` 默认 `HSE_VALUE = 25000000U`（25MHz），与本板 8MHz 晶振冲突。会导致 HAL_Delay / 串口波特率偏 3.125 倍。
- 决定：`platformio.ini` 加 `build_flags = -DHSE_VALUE=8000000U`
- 原因：实现既定 168MHz 时钟的必要条件（PLLM=8 除 8MHz = 1MHz VCO 输入，符合手册 1~2MHz）；不改架构决策，只是修正默认宏。
- 后果：
  - **优点**：HAL_Delay / 串口波特率正确；PLL 配置计算准确
  - **代价**：每次改 board 都要记得带这个宏；Phase 8 USB 的 48MHz 精度依赖此宏 + HSE 物理起振

## ADR-005：工作区根统一 repo（方案一）

- 日期：2026-09-18
- 背景：文档迁到 `doc/项目文档/` 后，工程 repo（`SmartDeskTerminal_freertos/.git`）与新文档位置分离。三个选项：① 工作区根统一 repo ② 文档独立 repo ③ 不入 git。
- 决定：选 **①**——删工程 `.git`，工作区根 `git init`，plan + doc + 工程一个 repo。
- 原因：一个 repo 看全貌，项目文档/硬件资料/代码统一管理。
- 后果：
  - **优点**：单一 repo，历史统一，跨文件改动一次 commit
  - **代价**：入库 6260 文件（doc/ 硬件资料占 6201），repo 体积大；固件工程不再独立 repo，日后单独分享固件需另建子 repo 或 sparse-checkout

## ADR-006：Phase 1 收尾裁决（蓝灯挂起 + 背光验证 + 进 Phase 2）

- 日期：2026-09-18
- 背景：首次烧录成功但 PC0 蓝灯不亮。用户完成排查：① 直读芯片寄存器（GPIOC->ODR）确认 PC0 在 toggle = 代码在跑 ② 点屏幕背光（28005 模块接 3.3V）确认供电 + 背光回路正常。结论：芯片/时钟/工具链/代码全正常，故障仅限板载蓝灯硬件。
- 三个待裁决项：
  1. **蓝灯悬案挂起** —— 是。根因 = 板载 LED 硬件问题（LED 坏/虚焊/走线断），代码验证通过（寄存器级）。不影响项目推进。待有万用表/示波器时再查硬件。
  2. **U4 背光脚冲突何时定论** —— 挂起。当前走 28005 转接板路线（18-pin FPC → 28005 → 排针），不走核心板 U4 接口。U4 pin4 是否为 LCD_BL_PD12 的文档矛盾（`02_屏幕引脚与接线图.md` vs `06_核心板_原理图与引脚映射.md`）只在 Phase 5 若直插 U4 时才需要定论。当前不影响。
  3. **背光点亮是否算 Phase 1 收尾** —— 是。plan.md Phase 1 目标"编译/下载/运行"三项全满足：编译✓、下载✓（ST-Link 烧录成功）、运行✓（寄存器验证代码在跑）；背光额外验证供电回路正常。Phase 1 收尾。
- 决定：Phase 1 收尾，进入 Phase 2。蓝灯 + U4 冲突两项挂起。
- 原因：验证目标已达成（代码能跑进 MCU），蓝灯是硬件故障不是代码问题，不应阻塞项目推进。
- 后果：
  - **优点**：项目推进不被硬件单点故障卡死；验证方法论升级（寄存器级 > 看灯）
  - **代价**：蓝灯硬件问题未修，后续 GPIO 输出实验缺少"看得见"的反馈；Phase 2 起需优先建立 UART printf 调试通道，不再依赖灯

## ADR-007：repo 精简为方案 X（只留 datasheets_md + 项目文档）

- 日期：2026-09-18
- 背景：ADR-005 入库 6260 文件（原厂硬件资料占 6201，581M）。用户要求精简：只留整理好的简洁版 md，最多 10 个，不放太大源文件。
- 决定：选方案 X——repo 只跟踪 `doc/datasheets_md/`（11 个 md，136K）+ `doc/项目文档/`（7 个 md）。原厂资料（核心板资料/ + 屏幕资料/）`git rm --cached` 从索引移除，磁盘保留但不入 repo。
- 原因：datasheets_md 是用户翻译整理的精简版，覆盖全 Phase 关键信息；原厂 6000+ C/H 源码 + PDF/ZIP/EXE 体积大且 PIO 编译时自动下载 HAL 库，不需要在 repo 存。
- 后果：
  - **优点**：repo 27 文件，轻量；clone 快；focus 在项目代码 + 精简文档
  - **代价**：原厂例程源码不在 repo，查例程细节要翻磁盘 `doc/核心板资料/`（不入 git，换机丢失）；需在 README 标注"原厂资料磁盘有，repo 不跟踪"

## ADR-008：T-007 UART printf 跳过 + Phase 2 部分收尾

- 日期：2026-09-21
- 背景：Phase 2 两个目标——T-006 Button（KEY_PC1）+ T-007 UART printf 重定向。用户无 CH340/USB-TTL 转换器，串口打印通道不可用。ST-Link V2 克隆棒（0483:3748）通常无 VCP。
- T-006 实际状态：
  - 代码完成（key.c/key.h，对照官方例程，引脚/极性/寄存器配置正确）
  - 编译烧录 Verified OK（Flash 3588B）
  - 基线验证通过（不接线=1，1001 次采样零抖动）
  - **功能验证挂起**（短接 PC1↔GND=0 未执行，因 ST-Link 未连接）
- T-007 决定：**跳过**（整个 UART 模块不建立，不只跳过 printf 重定向）
- 原因：用户无 CH340，printf 通道物理不可用；用户选择跳过而非购买/自发自收验证
- 后果：
  - **优点**：不阻塞 Phase 3 推进；省去等硬件的时间
  - **代价（关键风险）**：
    1. **FreeRTOS 调试缺 printf 通道**——Phase 3 任务切换/队列/信号量是时序相关，SWD 断点会冻结调度器，断点现场≠真实运行时序。没有 printf 看"任务调度历史"，FreeRTOS 时序 bug 极难定位
    2. **Phase 3-7 调试全靠 SWD 读变量**——能查"卡住那一刻"的值，查不到"时间序列上的变化"
    3. **plan.md Phase 2 产物 `[INFO] System Init OK / UART Ready` 未达成**——Phase 2 不算完整收尾，记"部分完成"
    4. **退路**：Phase 8 USB CDC 实现后，可用 USB 虚拟串口替代物理 UART 做 printf 通道——但 Phase 8 在 Phase 3-7 之后
  - **后续触发**：若 Phase 3 FreeRTOS 集成受阻且根因属时序问题，回头补 UART（买 CH340 或做自发自收）再继续
  - **挂起项**：T-006 功能验证（短接=0）5 分钟可补，建议进 Phase 3 前补完，让 GPIO 那半干净收尾

## ADR-NNN：<待追加>

> 做下一个取舍时在此追加。
