# Troubleshooting

> 已发生坑详写；预期坑按 Phase 预填预警（标"待踩"）。每条带 T-NNN 编号 + 日期。

## 已发生

### T-001：Zephyr 与 HAL 工程混淆

- 日期：2026-09-17
- 现象：plan.md Phase 1 写 `STM32 Framework / HAL`，但实际 `platformio.ini` 写 `framework = zephyr`；main.c 是 `#include <zephyr/kernel.h>` 空模板。
- 排查：1) 对比 plan 与实际工程配置 2) 确认 Zephyr 是 RTOS、HAL 是驱动库、CubeMX 是代码生成器，三者不同层面
- 根因：建工程时 framework 字段含义未分清，CubeMX/HAL/Zephyr/FreeRTOS 四个名字混用
- 解决：废弃 `SmartDeskTerminal/`（Zephyr 工程），新建 `SmartDeskTerminal_freertos/`（`framework = stm32cube`）
- 规则固化：建 PlatformIO 工程前，先确认 `framework` 字段对应的实际栈，别凭名字猜

### T-005：烧录成功但 PC0 蓝灯不亮 —— 软件侧已完全排除，判定为硬件段故障

- 日期：2026-09-18
- 现象：`pio run -t upload` 成功（日志有 `** Verified OK **` + `Resetting Target`），但 PC0 蓝灯**完全不亮**，闪烁也没有。
- **排查过程（用 openocd 直连 SWD 读寄存器 + 反汇编，非推测）**：

  | 读数 | 值 | 结论 |
  |---|---|---|
  | `RCC->CR` | `0x03037F83`（HSERDY=1 / PLLRDY=1） | HSE 8MHz 起振成功、PLL 锁定成功 |
  | `RCC->CFGR` | `0x0000940A`（SW=10 / SWS=10） | 系统时钟**已实际**切到 PLL |
  | `SystemCoreClock`（HAL 自算） | **168000000**（精确） | 主频 168MHz 正确，`-DHSE_VALUE=8000000U` 生效 |
  | `uwTick` 相隔 2000ms 三次 | +2003 / +2013 | SysTick 每 1ms 精准加一、中断正常 |
  | `GPIOC->MODER` | `0x...01` | PC0 = 输出模式 |
  | `GPIOC->ODR` 每 250ms 采 20 次 | `0 1 1 0 0 1 1 0 0 1 1 0 1 1 0 0 1 1 0 0` | **精确 1Hz、50% 占空比方波** |
  | `SCB->CFSR` / `HFSR` | `0` / `0` | 无 HardFault |
  | PC 多次采样（`nm` 对照） | 落在 `HAL_GetTick`(0x08000560) / `HAL_Delay`(0x0800056C~94) | main 的 while 循环正在正常跑，**没进 Error_Handler** |
  | 官方例程 `Drivers/User/Inc/led.h` | `LED1_PIN=GPIO_PIN_0` / `LED1_PORT=GPIOC`；`LED1_ON → BRR`（注释"LED1亮，此时IO口是低电平"） | 引脚确认 PC0、**极性为低电平点亮**，与我们代码逐项一致 |

- 追加验证（排除极性误判）：用调试器把 PC0 **恒低电平**保持 5 分钟 → 灯仍不亮；再 **恒高电平**保持 10 分钟 → 灯仍不亮。
- 根因：**MCU 侧全部正常**（时钟 / 中断 / GPIO 输出级 / 下载调试链路均已证明）。故障落在 **`PC0` 引脚 → 板上用户蓝灯 LED2 之间**（LED 本体坏 / 焊点虚焊 / 走线断）。
- 解决：**挂起**，等用户用万用表量 PC0 引脚（闪烁时应在 0V↔3.3V 跳变）做最终定性。Phase 1 的"可见指示"改用**屏幕背光**替代（28005 转接板 `LED` 脚接 3.3V，高电平点亮）。
- 规则固化：
  1. **"灯不亮"不要靠猜。** 先用 SWD 读 `RCC->CR/CFGR`、`GPIO->MODER/ODR`、`SCB->CFSR`，一次就能分清"没跑起来 / 跑在别的时钟 / 跑了但没配好 / 软件全对是硬件坏"。
  2. `ODR` / `IDR` 只能证明**芯片内部输出级**正常，**证明不了**引脚焊点到外部负载是否连通 —— 这是本次的关键边界。
  3. 给 `Error_Handler` 加闪灯指示时，循环次数必须按**失败分支的真实主频**校准：`for(i=0;i<1000000;i++)` 在 HSI 16MHz 下是 **~1 秒半周期（慢闪）**，会和正常 500ms 闪烁混淆，不是"快闪"。
  4. 查 LED 引脚/极性，一定要连官方例程的 `Drivers/User/Inc/led.h` 一起看，别只看 main.c。

### T-006：XPT2046 触摸坐标只有应有值的一半 —— 读位少了一个"转换"时钟

- 日期：2026-09-22
- 现象：触摸能读出坐标，但按遍全屏 raw 极值只有 `X 100~1872 / Y 169~1874`（约为 12 位满量程 4095 的 **44%**）。表现是**按钮永远点不中** —— hit-test 收到的坐标只有实际位置的一半，落在按钮框外。
- 排查路径（数据流切片法）：

  | 环节 | 观测方式 | 结论 |
  |---|---|---|
  | 手指按压 → 面板分压 | 无法直测 | 假定正常 |
  | ADC 采样 + **SPI 读位** | 读 raw 原始值 | ❌ **最早出错点** |
  | raw → 屏幕坐标（映射） | 读映射输出 | 错，但只是上游错误的后果 |
  | 屏幕坐标 → hit-test | 看 IN/OUT | ✅ 逻辑本身正确 |

- 根因：`touch_read_raw()` 在控制字发完后**少发 1 个"转换/BUSY"时钟**就直接读 12 位，于是把第一个无效位当成 D11 收了进来，**12 位数据整体错位 → 数值约减半**。
  代码里那句注释写着"转换所需 1 个额外时钟，**已在上面**"，但上面只调了 `touch_delay()`、**根本没产生时钟脉冲** —— 注释与代码不符，bug 就藏在这道缝里。
- 定位手段（**可复用**）：加"双读法实时对比" —— 同一次按压把两种读法（补 0 个 / 补 1 个时钟）的结果并排显示在屏上，**按住不动**读两个数：
  - `B ≈ A × 2` → 存在一位错位（本次即此）
  - `B ≈ A` → 读法本来就对

  实测 `A=100~1872 / B=200~3800`，且 `200~3800` 正是 2.8" 电阻屏的典型量程 → 一锤定音。
- 解决：补上该时钟（`touch_read_raw_ex(ctrl, 1)`）。
- 规则固化：
  1. **注释声称做了某事时，回头核对代码是否真做了** —— 本次 bug 就藏在"注释说已在上面、代码里没有"的缝隙里。
  2. **读数呈 2 的幂次偏差（减半 / 1/4 / 1/8）时，优先怀疑读位错位或时钟数目不对**，不要去调校准区间。
  3. **区分"校准区间"与"读数正确性"** —— 校准只做线性缩放，治不了读数本身丢位。本次起初误在映射层找原因，根因其实在读位层。
  4. 排查必须配一个**能证伪的观测手段**（本次的双读法对比），否则改完只是自我安慰。

## 预期坑预警与回填（按 Phase 预填；已踩的原地补成实际记录）

> T-002 已踩并回填（2026-09-23）。T-003 触摸侧命中的变体见 **T-006**。T-003 / T-004 仍未踩。

### T-002（已踩，2026-09-23）：FreeRTOS 集成配置 —— 中断优先级宏未左移，启动第一个任务即 HardFault

> 原预期里的 **②「`configMAX_SYSCALL_INTERRUPT_PRIORITY` 配错致临界区失效」** 与 **④「内核中断优先级未设最低」** —— **两条全中**，且是同一个根因。
> 详细推理训练见 [training-invpc.md](training-invpc.md)。

- 实际 Phase：3
- 实际现象：屏静态文字正常、`pre-Sched` 显示、**一个任务都没跑起来**；接管故障向量后屏上：`FAULT: HARD` / `CFSR=0x00040000` / `HFSR=0x40000000`
- 实际根因：`configKERNEL_INTERRUPT_PRIORITY` / `configMAX_SYSCALL_INTERRUPT_PRIORITY` **未按 `(8 - configPRIO_BITS)` 左移 4 位**（写成 `15` / `5`，应为 **`0xF0` / `0x50`**）。STM32 的 NVIC 优先级寄存器只实现高 4 位，写入 `0x0F` 会被丢成**优先级 0（最高）**，而不是 15（最低）。
  - 后果 ①：PendSV / SysTick 落到最高优先级，违背"内核中断必须最低"；
  - 后果 ②：`BASEPRI = 5` 屏蔽不掉优先级 0 → **临界区挡不住 PendSV / SysTick**，内核链表可能被中断中途改写。
- 定位链（**每一步都用二进制取证，未靠猜**）：
  1. `CFSR` bit18 = **INVPC（用非法 EXC_RETURN 装载 PC）** → 这类动作只可能出现在 `bx lr` / `bx r14` 上 → 全工程仅 `port.c` 的 `vPortSVCHandler`（启动第一个任务）与 `xPortPendSVHandler`（任务切换）两处；
  2. 读 `firmware.bin` 前 64 字节核对向量表：[3]HardFault、[11]SVCall、[14]PendSV、[15]SysTick 均已正确指向目标函数 → **排除"软中断落到 `Default_Handler` 死循环"**；
  3. 反汇编 `SVC_Handler`：`ldr r0,[pxCurrentTCB,#0]` → `ldmia r0!,{r4-r11,lr}` → `msr PSP,r0` → `bx lr`，与 `pxPortInitialiseStack()` 的帧布局（`[0..7]=R4~R11`、`[8]=EXC_RETURN=0xFFFFFFFD`、`[9..16]=R0~xPSR`）**严格一致** → **排除"帧布局不匹配"**；
  4. `prvPortStartFirstTask` 内含 `msr control,#0` 清 FPCA → **排除 FPU 懒加载/帧类型不匹配**；
  5. 反汇编 `vPortEnterCritical`，看到 `mov.w r3, #5; msr BASEPRI, r3` —— 常量是 `5` 而不是 `0x50`，**配置未左移在此暴露**。
- 实际解决：两个宏改为 `<< ( 8 - configPRIO_BITS )`。
- 验证：修后反汇编 `vPortEnterCritical` = `mov.w r3,#80 @0x50`；`xPortStartScheduler` 写 SHPR3 的常量由 `0x000F0000 / 0x0F000000` 变为 `0xF00000 / 0xF0000000`；烧录后**三任务全部正常调度**。
- 规则固化：
  1. **凡是要写进硬件寄存器的"编号"类配置，先确认硬件实现了多少位** —— "优先级编号"与"寄存器值"不是一回事（NVIC 只实现高 4 位，必须 `<< 4`）。
  2. **移植 RTOS 时，优先级宏一律从官方 STM32 模板抄，不要自己推**。
  3. **"什么都没发生"的故障，第一步先把静默变可见**：接管 weak 故障向量并打 `CFSR` / `HFSR`；本板 PC0 LED 是坏件 → 提示只能走屏。
  4. **反汇编 ELF（向量表 / 符号地址 / 机器码里的常量）是不烧录就能排除一半假设的手段** —— 本次 4 条假设全靠它排除。
     ⚠️ **但它是"核武器"，不是日常技能** —— 只在"芯片已死、连屏都画不出、SWD 也连不上"时才开始用（本次正是这种情况）。**不要把它列为团队成员的学习门槛**：日常排查靠"读自己的代码 + SWD 断点看变量"，反汇编这类工具型操作应交给 AI 代跑。
  5. 故障处理器若要取真实现场，**异常向量必须用 `naked`**（普通 C 函数序言会改写 SP/LR）；解引用前先判地址合法性，防 double fault。
- 相关隐患（未阻塞）：`SystemInit()` 只开 FPU、**不设 `SCB->VTOR`**（保持复位值 0），目前靠 F4 的 flash 别名侥幸工作；建议后续在 `main()` 里显式 `SCB->VTOR = 0x08000000;`。

### T-003（LCD 侧未踩，触摸侧踩到同类变体）：SPI 驱动时序

- 预期 Phase：5
- 预期现象：屏不亮 / 花屏 / 颜色反
- 预期根因：SPI 时钟极性/相位（CPOL/CPHA）错；DC/RES 时序错；数据/命令未区分
- **实际情况**：LCD 侧**一次通过**（CPOL=0 / CPHA=0、DC/RES 时序均正确）。但**触摸侧踩到了同类问题的另一种变体** —— XPT2046 读位少一个时钟、数值减半，见 **T-006**。
  教训：SPI 类外设"**能通信**"≠"**数据正确**"，读位数量/错位要靠**数值量级**来判断。
- 预防：先抄 doc/ 里对应分辨率的官方 ST7789 例程，别从零写
- 待踩后补：见上

### T-004（已踩，2026-09-25）：LVGL 与 FreeRTOS 的 tick 绑定

- 预期 Phase：6
- 预期现象：LVGL 界面不刷新 / 抖动
- 预期根因：`lv_tick_inc()` 没在稳定节拍里调；`lv_timer_handler()` 调用频率不对
- **实际踩到（编译期变体）**：选了 `LV_TICK_CUSTOM=1` 直接挂 `xTaskGetTickCount()`（比 SysTick 里调 `lv_tick_inc` 更优雅，不动中断），结果 `lv_hal_tick.c` 编译炸出几十个错：
  ```
  task.h:34: #error "include FreeRTOS.h must appear in source files before include task.h"
  ```
- 实际根因：**FreeRTOS 规定 `task.h` 之前必须先 include `FreeRTOS.h`**（task.h 里有显式 `#error` 检查，它自己不带这条 include）；而 LVGL 的 `LV_TICK_CUSTOM_INCLUDE` 宏**只有一个文件槽**，单塞 `task.h` 违反顺序。
- 实际解决：写包装头 `src/lvgl_tick_source.h`（内容就两行：先 `FreeRTOS.h` 后 `task.h`），`lv_conf.h` 引用它。
- 顺带踩到（同一次编译）：`lib_deps` 写 `lvgl/lvgl@^8.3.11`，`^` 语义是"≥8.3.11 的任意 8.x"，PIO 实际拉了 **8.4.0**。要锁 8.3 系列必须写精确版本 `@8.3.11`。
- 预防 / 规则固化：
  1. **FreeRTOS 全家头文件有 include 顺序要求：`FreeRTOS.h` 永远第一个**，`task.h`/`queue.h`/`semphr.h` 都在其后。
  2. 第三方库的"单文件 include 槽"宏需要多条 include 时，**用包装头**，别指望库会自己带依赖。
  3. semver 前缀（`^`/`~`）在 `lib_deps` 里是**范围**不是版本号，锁定要写全。
- 验证：修后编译 SUCCESS（RAM 47.3% / Flash 24.0%），运行时行为待实机验证后补记。

### T-005（2026-09-25）：safe-delete 表现④ —— SCons 跳过构建报假 SUCCESS

- Phase：8
- 现象：`pio run` 18s 报 SUCCESS，**零 Compiling/Linking 行**，firmware.elf 0 字节或不存在
- 根因：多次改名 `.sconsign311.dblite`（→ .zz → .zz2）触发 safe-delete 计数器累积到阈值 50。此后 PIO 每次启动都尝试清理这些改名残留，全部 `SAFE_DELETE_BULK_REJECTED` → SCons 无法初始化/写入 `.sconsign` 数据库 → 跳过所有构建步骤直接报 SUCCESS
- 与表现①②③的区别：
  - ① 阻塞挂起（pio 卡死无 gcc 子进程）
  - ② 软拒绝（构建照常跑完）
  - ③ FAIL_CLOSED 卡 elf（报 FAILED 但实际编译完成）
  - **④ 本次：SCons 无法初始化 → 零编译零链接直接 SUCCESS（最危险：看着成功但 elf 无效）**
- 绕过方法：在 `platformio.ini` 加 `build_dir = .pio/build_new`（新目录无历史包袱，safe-delete 不触发）。编译成功后可去掉该配置
- 规则固化：
  1. **不要再改名 .sconsign**——多次改名会累积 safe-delete 计数器，触发表现④
  2. **判据升级**：不只看 SUCCESS/FAILED，必须确认日志有 Compiling/Linking 行 + elf 大小>0 + mtime 新鲜
  3. **safe-delete 计数器是按"轮"累积的**——同一轮里反复触发文件操作会累积；换新 build_dir 是最干净的绕过
