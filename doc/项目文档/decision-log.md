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

## ADR-009：Phase 5 LCD + 触摸驱动提前到 FreeRTOS 之前（A 路线）

- 日期：2026-09-21
- 背景：用户无 CH340（ADR-008 跳过 UART printf），提出"屏幕做调试输出通道 + 触摸替代物理按钮 + 硬件闭环"。经分析屏幕做不了高频时序日志（刷新率跟不上 FreeRTOS 调度），但可做稳态状态显示。用户选 A：提前跑通屏幕（裸机），再做 FreeRTOS。
- 决定：
  1. Phase 5 LCD（裸机点屏）+ Phase 7 触摸驱动（裸机）提前到 Phase 3 FreeRTOS 之前
  2. 触摸替代物理按钮（XPT2046 电阻触摸，**非** plan.md 原写的 FT6336U 电容——到 Phase 7 驱动按 XPT2046 写）
  3. 硬件闭环：开发板 + 屏幕 = 完整硬件，不额外做 PCB（元器件全在开发板+转接板上）
  4. 购买：万用表 + 逻辑分析仪 + CH340（到货中），杜邦线已有，外壳铜柱 PCB 暂不
- 顺序调整：原 plan.md 3→4→5→6→7，新顺序 5(裸机LCD)→7(裸机触摸驱动)→3(FreeRTOS)→4(任务通信)→6(LVGL+触摸输入设备)
- 原因：
  1. 屏幕是产品最终形态，提前跑通不浪费
  2. 触摸替代按钮后硬件闭环，无需额外 PCB
  3. 用户已有杜邦线，接线零成本
  4. SWD 调试（用户 Phase 1 已会）不依赖 CH340，FreeRTOS 期稳态调试够用
- 后果：
  - **优点**：硬件闭环早成型；屏幕跑通后 FreeRTOS 有可视化状态显示通道；触摸替代按钮省物理按钮
  - **代价（关键风险）**：
    1. FreeRTOS 调试期缺 printf 时序日志（ADR-008 代价延续），遇时序 bug 时靠 SWD 监视变量，查不动再补 UART
    2. 顺序倒置：plan.md Phase 衔接需重新安排（原 3→4 依赖 UART 通道做任务通信调试，现缺通道）
    3. 屏幕驱动（SPI+ST7789 初始化+字符绘制）是 Phase 5 最难内容，新手先爬这个大坡
    4. XPT2046 电阻触摸需按压力度（非电容轻触），用户体验略差但驱动简单
  - **后续触发**：plan.md 待更新顺序；若 FreeRTOS 期遇时序 bug 查不动，回头用 CH340 补 printf

## ADR-010：Phase 2 结账（T-006 正式放弃）+ Phase 5 LCD 完成

- 日期：2026-09-22
- 背景：
  - Phase 5 LCD 三步全部实机通过（SPI3 通信层 → ST7789 初始化序列 → 字符渲染）
  - T-006 的功能验证判据（短接 PC1↔GND 读到 0）经用户**二次确认放弃**
- 决定：
  1. Phase 5 结账：STEP1/2/3 判据全部通过
  2. T-006 功能验证**正式放弃**，Phase 2 按「GPIO 半通过 / UART 半跳过」结账
- 原因：触摸已成为主要输入手段（ADR-009），物理按钮的验证价值随之下降；继续投入时间做短接验证对后续 Phase 无实际影响。
- 后果：
  - **优点**：Phase 2 干净结账、不留"待补"尾巴；注意力集中到真正会用的输入通道
  - **代价**：PC1 按键「按下=0」这一半**从未实测**（只有基线=1 已证）。若日后真要启用物理按键，需补测
  - **影响面**：无。`key.c|h` 代码保留且编译通过，随时可用

## ADR-011：触摸驱动方案定稿（读位时序修复 + 固定校准 + 锁存式 hit-test）

- 日期：2026-09-22
- 背景：T-009 触摸驱动实机调试，连续暴露两个问题 —— 坐标数值只有应有值的一半、按钮点不中。
- 决定：
  1. **读位时序**：控制字发完后补 1 个"转换/BUSY"时钟，再读 12 位数据
  2. **校准区间固定为 `XPT_MIN/MAX_X = 200~3800`**（Y 暂同），**不做运行时自适应**
  3. **按钮 hit-test 用「锁存式」**，不用"按下瞬间"的边沿检测
  4. 固件加入版本标识（`FW_VERSION` + `__DATE__`/`__TIME__` 编译时间）
- 原因：
  1. A/B 双读法对比实测：不补时钟 X = 100~1872，补 1 个时钟 X = **200~3800**（恰为 2 倍，且符合 2.8" 电阻屏典型量程）→ 确认原实现**少读一位、数值减半**
  2. 自适应校准要求用户**每次上电先点屏幕四角** —— 作为产品方案不可接受（用户明确否决："不可能让用户每次都点一遍"）
  3. 锁存式同时解决两个问题：按下**首帧**坐标最易失准（首帧偏出按钮区后边沿即被用掉、永不再触发）、按住期间重复计数
  4. 手工版本号人会忘记维护，**编译时间由编译器自动填充、每次重编译必变**，才是判断"是否烧了最新固件"的可靠依据
- 后果：
  - **优点**：触摸精度恢复正常；校准值一次写死、运行时零开销；按钮触发可靠；烧录后可自助核对版本
  - **代价（关键）**：
    1. **校准区间与读法强绑定** —— 换触摸屏型号必须重新实测并替换那 4 个宏，无法自动适配
    2. **Y 轴沿用 X 的量程**（未单独实测），存在约 20 像素误差；做滑动/手写等精度敏感功能前需补测
    3. 放弃自适应换来了"开机即可用"的产品体验，代价是失去对面板个体差异的容忍度
  - **后续触发**：Phase 6 LVGL 触摸输入若需更高精度 → 实测 Y 轴范围替换 `XPT_MIN_Y/MAX_Y`

> 做下一个取舍时在下方追加，格式同上（**每条必须写"后果"段**）。

## ADR-012：FreeRTOS 集成方案（PIO lib_deps + ARM_CM4F + 接受现有代码改）

- 日期：2026-09-22
- 背景：FreeRTOS 集成有三个方案（clone GitHub / PIO lib_deps / 手写最小文件集），FPU 有两个方案（ARM_CM3 无 FPU / ARM_CM4F 有 FPU）。AI 未经决策直接 clone + 用 ARM_CM3 一步到位写完三任务，违反协作规则（方案未商量 + 无学习空间）。用户复盘后定方案。
- 决定：
  1. **集成方式**：PIO lib_deps（方案 B），不用 clone 的 GitHub 仓库
  2. **移植层**：ARM_CM4F（用 FPU，方案 B），不用 ARM_CM3
  3. **现有代码**：接受，在此基础上改（删 clone 仓库，换 lib_deps + CM4F，解决 FPU 链接问题）
  4. **执行模式**：一步到位（用户明确，不分步）
- 原因：
  1. lib_deps 最省事，PIO 管版本，不污染 repo（clone 的内嵌 git 是麻烦）
  2. F407 有 FPU 该用，ARM_CM4F 是正确的移植层
  3. 现有 main.c 三任务架构可复用，只改集成层
- 后果：
  - **优点**：PIO 管理依赖，repo 干净；FPU 性能不浪费；代码复用不浪费
  - **代价（关键风险）**：
    1. ARM_CM4F 的 FPU ABI 冲突需解决——之前 build_unflags 没解决链接问题，可能要改 ldscript 或用 extra_script
    2. PIO lib_deps 的 FreeRTOS 包版本/质量需验证（PIO 注册表可能不是官方最新）
    3. 原有 clone 仓库要删除（git rm）+ .gitignore 防止重新 clone
  - **后续触发**：如果 ARM_CM4F FPU 链接问题仍无法解决（试完 extra_script 后），回退到 ARM_CM3（ADR-001 预言的退路）

## ADR-013：Phase 6 LVGL 集成（v8.3.11 + lib_deps + 静态内存池 + 单任务架构）

- 日期：2026-09-25
- 背景：Phase 6 LVGL 移植，四个决策点（用户四项全部选推荐项）：版本、集成方式、内存布局、首步范围。RAM 128KB 是硬约束（全屏缓冲 153.6KB 放不下）。
- 决定：
  1. **版本**：LVGL **v8.3.11 精确锁定**（`lib_deps` 写 `@8.3.11`，不写 `^8.3.11`——`^` 会拉到 8.4.0，T-004 实测）
  2. **集成方式**：PIO lib_deps（与 FreeRTOS 同路线，ADR-012 先例）
  3. **内存布局**：`LV_MEM_CUSTOM=0` + 32KB 静态数组池；绘制缓冲 240×40×2B=19.2KB 单缓冲（1/8 屏分块渲染）
  4. **架构**：**单任务**（Task_LVGL prio3/栈768字）跑 `lv_timer_handler`，官方推荐 RTOS 模式；旧三任务+队列退役（队列 Phase 8 USB Agent 回归）
  5. 顺带三件事：SPI3 提速 /16→/4（2.625→10.5MHz）；`SCB->VTOR=FLASH_BASE` 补显式设置；`Touch_Read` 的 `HAL_Delay(5)` 忙等改 `vTaskDelay`
- 原因：
  1. v8.3 中文资料/教程最全，卡住时好搜；8.x API 稳定
  2. lib_deps 保持依赖管理一致，repo 干净
  3. 两池分离（LVGL 32K / FreeRTOS 8K）出问题好定位；单缓冲足够（LVGL 渲染期间 CPU 无事可做，双缓冲无收益）
  4. LVGL 的 lv_timer + 事件回调天然替代"轮询任务+队列+手搓 hit-test"，强行保留三任务反而画蛇添足
- 后果：
  - **优点**：RAM 47.3%（62KB/128KB）Flash 24.0%（126KB/512KB）都有余量；UI 开发效率从"手搓字模+整行重画"升级到"声明式控件+脏区渲染"；`LV_USE_PERF_MONITOR=1` 右下角自带 FPS 显示，性能可见
  - **代价（关键风险）**：
    1. **RAM 已用近半**——Phase 8 USB CDC（栈+缓冲）+ Phase 9 Agent 若再吃 20KB+ 就逼近上限，届时优先考虑缩绘制缓冲（240×40→240×20 省 9.6KB）或砍 LVGL 池
    2. SPI 提速到 10.5MHz 未经长时验证，**花屏/偏色第一怀疑对象**（回退 /8=5.25MHz 对照）
    3. `touch.c` 从此依赖 FreeRTOS（vTaskDelay），不能在调度器启动前调用 `Touch_Read`
    4. 编译文件数 299（原 ~120），全量编译 ~36s；字号/控件全在 `src/lv_conf.h` 裁剪，加控件要记得改配置
  - **后续触发**：实机若花屏 → 先降 SPI 到 /8；若 RAM 吃紧 → 缩缓冲；Phase 9 需要中文 → 开 `LV_FONT_SIMSUN_16_CJK`（约 170KB Flash，预算内）

## ADR-014：Phase 8 USB CDC 集成（HAL USB Device + 库文件复制 + 双向+命令雏形）

- 日期：2026-09-25
- 背景：Phase 8 USB CDC，三个决策点（用户全选推荐）：USB 线材（有数据线）、协议栈（HAL USB Device）、应用范围（双向+命令雏形）。F407VET6 有 OTG_FS（PA11/PA12 直连 Type-C），PLLQ=7→48MHz 在 ADR-003 已提前配好。
- 决定：
  1. **协议栈**：STM32 官方 USB Device Library（HAL 路线一致）
  2. **集成方式**：复制 CDC+Core 中间件 4 个 .c + 头文件到 `lib/usb_device/`（PIO 默认不编 Middleware，复制最干净）
  3. **应用范围**：双向通道 + 命令解析（hello/version/hits/clear/ping/help），直接做到 Agent 雏形
  4. **任务架构**：Task_LVGL(prio3) + Task_Agent(prio2)；Agent 不直接碰 UI，走队列中转（LVGL 非线程安全）
  5. **队列回归**：Phase 6 退役的队列在此重启（Agent→UI 消息通道）
  6. **USB 中断优先级**：6（≥5 可调 RTOS API，保守做法）
- 原因：
  1. HAL USB Device 与项目路线一致，资料最全
  2. 复制中间件比 extra_script 配置简单，离线可编，repo 干净（只加 11 个文件）
  3. 双向+命令雏形为 Phase 9 Agent 打好地基，只剩协议升级
  4. LVGL 非线程安全 → Agent 改 UI 必须走队列中转（LVGL 官方推荐多任务用法）
- 后果：
  - **优点**：printf 通道终于通了（USB_CDC_Printf 替代被跳过的 UART printf，ADR-008 间接解决）；PC 端能发命令到板子（Phase 9 地基）；RAM 50.9%（66.7KB）Flash 26.2%（137.5KB）都有余量
  - **代价（关键风险）**：
    1. **RAM 已用过半**——Phase 9 Agent + Phase 10 整合若再吃 20KB 就逼近上限，届时缩绘制缓冲或砍 LVGL 池
    2. USB CDC VID=1234/PID=5678 是 ST 测试值，正式产品需替换 + INF 驱动
    3. CDC 接收环形缓冲 256 字节，超长命令（>64B 单包）会被截断
    4. USB 中断优先级 6 与 FreeRTOS 临界区（BASEPRI=5）兼容，但 USB 中断里不能调 RTOS API（当前没调，安全）
    5. **safe-delete 表现④（新发现）**：PIO 启动时反复尝试清理 .sconsign 改名残留文件，全部被 REJECTED → SCons 无法初始化数据库 → 跳过构建报假 SUCCESS。绕过方法：换新 build_dir。
  - **后续触发**：Phase 9 需要更丰富命令协议 → 扩展 cmd.c；PC Agent 需要结构化数据 → 改 CDC 传输格式

## ADR-015：T-007 修复策略 —— 句柄改名 + 补 MSP + 静态分配 + `-fno-common`

- 日期：2026-09-25
- 背景：ADR-014 落地后首次实机**失败**：烧录后花屏 + PC 无虚拟 COM 口。
  用户明确澄清 **Phase 6（LVGL）单独烧录时验证通过**，因此花屏是 Phase 8 引入的回归，
  **SPI 10.5MHz 提速被排除嫌疑**。排查后定位到 3 个确定性缺陷（详见 `troubleshooting.md` **T-007**）：
  ① `hUsbDeviceFS` 同名不同型被链接器 `-fcommon` 静默合并 → 内存别名 → 野指针任意写；
  ② `HAL_PCD_MspInit` 全工程缺失 → PA11/PA12 从未配成 AF10；
  ③ `USBD_malloc` 走 newlib 堆（与 FreeRTOS 堆/LVGL 池两套并存，且非线程安全）。
- 决定：
  1. **句柄命名规范**：PCD 句柄改 `hpcd_USB_OTG_FS`，USBD 句柄保留 `hUsbDeviceFS`（ST 官方命名约定）。
     今后所有句柄加类型前缀（`hpcd_` / `husbd_` / `hspi_` …）。
  2. **补 `HAL_PCD_MspInit` / `HAL_PCD_MspDeInit`**，执行 `GPIO_AF10_OTG_FS` 的 PA11/PA12 配置 + NVIC；
     不配 PA9（VBUS 检测关闭，避免与 H1 的 USART1_TX 抢脚）。
  3. **`USBD_malloc` 改静态 arena**（768B，`aligned(8)`，单槽位），彻底不碰 newlib 堆。
  4. **构建加固：`-fno-common`**（写进 `platformio.ini` 并加注释锁死）。
  5. **补 `CDC_Control_FS` 的 `GET/SET_LINE_CODING`**（部分 Windows 版本依赖它才创建 COM 口）。
  6. **`USB_CDC_Printf` 的共享静态缓冲加临界区**（原被 Task_Agent / Task_LVGL 并发调用）。
- 原因：
  1. ①是**症状的全部来源**（花屏与无 COM 口同源），且属"编译零警告、运行才崩"的最难查类别；
  2. ②是"没有虚拟串口"的**独立第二因**，即便修了①也枚举不出来；
  3. ③④⑤⑥属同一模块内的同源隐患，一次改干净，避免下轮再来；
  4. **`-fno-common` 是本次最有价值的长期收益**：把这类 bug 从"运行期随机崩"提前到"链接期硬报错"。
- 后果：
  - **RAM 52.0%（68212B）/ Flash 26.3%（137704B）**，0 error 0 warning（全量 327 Compiling + 1 Linking）。
  - `nm -S` 复验：`hpcd_USB_OTG_FS`（1252B @0x2000F868，8 字节对齐）与 `hUsbDeviceFS`（732B @0x200002A4）
    成为**两个独立符号**；`usbd_arena` 768B @0x2000FD50 对齐 OK。
  - **代价**：`-fno-common` 一旦将来引入有真实重复定义的第三方库，会直接链接失败（这是**期望行为**，
    早失败早发现）；RAM 再涨 1.5KB，Phase 9/10 的余量进一步收窄。
  - **后续触发**：若 Phase 9 引入更多中间件，注意同类别名；若 RAM 逼近上限，先砍 LVGL 绘制缓冲。

## ADR-016：PC ↔ MCU 通信协议格式定稿（行式类型化文本，非 JSON）

- 日期：2026-09-25
- 背景：`plan.md` 里只有一个 JSON 雏形（`{"type":"event","msg":"compile success"}`），从未敲定。
  需在 Phase 9 前定稿，因为两边（`cmd.c` / Python agent）都要按它写。
  约束：MCU 侧 RAM 余量已过半、无 JSON 解析库、FreeRTOS 堆只有 8KB、用户是嵌入式初学者。
- 决定：**行式类型化文本（line-delimited typed text）**，不做 JSON。
  - 帧：每行一条消息，`\r\n` 结尾（兼容一切串口终端）
  - 语法：`<TYPE> <key>=<value> [<key>=<value> ...]`
  - 类型表：

    | TYPE | 方向 | 语义 |
    |---|---|---|
    | `CMD` | PC → MCU | 命令（有响应） |
    | `EVT` | PC → MCU | 事件推送（无响应） |
    | `ACK` | MCU → PC | 命令成功响应 |
    | `ERR` | MCU → PC | 命令失败响应（带 `code=`/`msg=`） |
    | `DAT` | MCU → PC | 主动上报数据 |
  - 示例：
    ```
    PC→MCU   CMD name=hello
    MCU→PC   ACK name=hello
    PC→MCU   CMD name=hits
    MCU→PC   DAT hits=3
    PC→MCU   EVT kind=build status=success proj=SmartDesk
    MCU→PC   DAT kind=build status=success
    ```
  - **向后兼容**：不以 `CMD`/`EVT`/`ACK`/`ERR`/`DAT` 开头的行，按**原裸命令**解析
    （`hello` / `version` / `hits` / `clear` / `ping` / `help`）→ 人肉调试体验不变，现有 `cmd.c` 不用推翻。
  - 文本值中的空格用 `+` 代替（一条 `strchr` 就能还原，零转义复杂度）。
- 原因：
  1. **JSON 在 MCU 上没有收益只有成本**：需要引入/自写解析器（内存 + 攻击面），而本项目消息 schema 极窄
     （就几个 key），手写 JSON 解析器是 bug 温床；user 是初学者，调试一个手写 parser 会拖垮进度。
  2. **行式文本天然可调试**：任何串口终端都能直接读、直接敲，不需要 host 端脚本就能手工验证 ——
     这与本项目"每步都要有可观测判据"的方法论一致。
  3. **key=value 的扩展性足够**：加字段不破坏老解析器（未知 key 忽略即可），升级成本近乎为零。
  4. **保留裸命令通道**：现有 `cmd.c` 已验证可用，不推翻；`CMD name=` 只是它的"带标识版本"。
  5. **将来要上 JSON 也不阻塞**：帧层（行 + `\r\n` + TYPE 前缀）不变，只换 `key=value` 的载荷编码即可，
     属于**可平滑演进**的设计。
- 后果：
  - `cmd.c` 只需加一层"剥 TYPE 前缀 → 若为 `CMD` 则再解析 key=value"，裸命令路径原样保留。
  - PC 端 Python agent 用 `serial.readline()` + `str.split()` 即可，零依赖（不需要 `pyserial` 之外的东西）。
  - **代价**：不支持嵌套结构（本项目不需要）；长文本需 `+` 编码（当前消息都不长）。
  - **待定**：是否需要**校验和/序号**（当前 USB CDC 有硬件级重传，暂不加；若将来改 UART 裸线再议）。

