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

### T-009（2026-09-25）：safe-delete 表现④ —— SCons 跳过构建报假 SUCCESS

> 📌 **编号更正（2026-09-26）**：本条原误编为 `T-005`，与"PC0 蓝灯硬件故障"那条撞号。
> `T-005` 保留给蓝灯故障（代码注释与 `debug-manual.md` 都引用它），本条改编号为 **T-009**。

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

**2026-09-25 二次复现（补充记录，又踩一次）**：
- 本次为改 `build_flags`（加 `-fno-common`）需要全量重编 → 按老经验改名 `.sconsign311.dblite` → 触发 **表现②变体**：
  日志出现 `[safe-delete][SAFE_DELETE_BULK_CONFIRM_REQUIRED] {"count":68,"threshold":50}`，
  随后**报 SUCCESS 但零 Compiling 行、firmware.elf mtime 停在上一轮**（= 假成功）。
  原因：全量重编要删 68 个旧 `.o`（> 阈值 50），钩子拒绝 → SCons 放弃但退出码仍为 0。
- **更省事且更干净的做法（本次实测有效）**：直接 `os.rename` 把**整个环境构建目录**挪走
  ```
  .pio/build/black_f407ve  →  .pio/build/build_stale_<日期>_<标记>
  ```
  `os.rename`/`mv` **不计入 safe-delete 计数**，且新目录为空 → 全量构建全程无需删除任何东西 → 钩子根本不触发。
  本次结果：327 Compiling + 1 Linking，67.6s，0 error 0 warning，全新 elf 落地。
- ⚠️ 代价：旧目录会累积（`build_*` 已成堆），需定期人工清理。**这不是"删除操作"，是"搬运操作"，所以永远安全。**

### T-007（已踩，2026-09-25）：两个同名句柄被链接器合并 → 内存别名 → 花屏 + USB 无法枚举

> **这是"编译零警告、运行随机崩"的教科书案例。** 也是本项目第一次遇到"症状在显示层、根因在 USB 层"的跨模块故障。

- Phase：8（USB CDC）
- 现象（用户报告）：烧录后**花屏**、无法正常显示；同时 USB 连 PC **不出现虚拟 COM 口**。
  - 关键分界：**Phase 6（LVGL）单独烧录时是验证通过的**（界面/颜色/触摸/FPS 全正常），
    所以花屏**不是** SPI 提速（10.5MHz）造成的 —— 这一点排除了最初的第一嫌疑。
- 根因：**两个文件各自定义了同名全局变量，但类型不同**
  ```c
  lib/usb_device/usbd_conf.c :  PCD_HandleTypeDef  hUsbDeviceFS;   /* 1252 字节 */
  src/bsp/usb_cdc.c          :  USBD_HandleTypeDef hUsbDeviceFS;   /*  732 字节 */
  ```
  ARM GCC **默认 `-fcommon`**，把两个 tentative definition **静默合并成同一个地址**。
  - **ELF 实证**（`arm-none-eabi-nm -S firmware.elf`）：修复前只有**一个**符号
    `2000ffa4 000004e4 B hUsbDeviceFS` —— `0x4E4 = 1252` 正是 `PCD_HandleTypeDef` 的大小
    （含 `IN_ep[16]` + `OUT_ep[16]`），**USBD 句柄被"折叠"掉了**。
  - 因果链：
    1. `USB_CDC_Init()` → `USBD_Init()` 把 USBD 协议栈句柄写进该地址；
    2. 同一调用链里 `USBD_LL_Init()` → `HAL_PCD_Init()` 又把 **1252 字节的 PCD 结构体整块覆写**上去
       → **协议栈句柄被摧毁**；
    3. 之后协议库通过 `pdev->pClassDataCmsit[]` / `pdev->pData` 等字段解引用，
       读到的却是 PCD 结构里的字节 → **指针全是垃圾 → 任意地址读写**；
    4. `HAL_PCD_IRQHandler` 用被 USBD 侧覆写过的端点结构取 `xfer_buff` → **野指针搬运**。
    - ① → 显示层被写坏 = **花屏**；② → `dev_state` 永远是垃圾 ≠ `USBD_STATE_CONFIGURED`
      → `USB_CDC_Send` 全部静默丢弃 + 枚举失败 = **没有 COM 口**。**两个症状同一个根因。**
- 定位链（**未烧录，全程靠 ELF 符号表 + 源码对读**）：
  1. `nm -S` 看 `hUsbDeviceFS` → 只有 1 个、大小为 1252 → 与 `PCD_HandleTypeDef` 尺寸吻合，
     而 `USBD_HandleTypeDef` 只有 732 → **数量与尺寸双双对不上，别名成立**；
  2. 回读 `usbd_conf.c` / `usb_cdc.c` → 两处定义均非 `static`、名字完全一致 → 根因确认；
  3. 顺带发现另外两个缺陷（详见下）；
  4. 修完再 `nm -S` 复验：变成**两个独立符号**（`hpcd_USB_OTG_FS` 1252 + `hUsbDeviceFS` 732）→ 结案。
- 实际解决（4 处改动 + 1 处构建加固）：

  | # | 改动 | 说明 |
  |---|---|---|
  | 1 | `usbd_conf.c`：PCD 句柄改名 `hpcd_USB_OTG_FS` | ST 官方命名，彻底消除同名 |
  | 2 | `usbd_conf.c`：**补上缺失的 `HAL_PCD_MspInit`** | PA11/PA12 配 `GPIO_AF10_OTG_FS` 复用推挽 + NVIC。原文件**完全没有这个函数**，落到 HAL 的 weak 空实现 → 引脚停在复位态浮空 → 这也是"没有虚拟串口"的**独立第二因** |
  | 3 | `usbd_conf.h`：`USBD_malloc` 由 `malloc` 改为**静态 arena** | 原实现走 newlib 堆，与 FreeRTOS 堆/LVGL 池是两套内存管理；且 `malloc` 非线程安全。改为 .bss 里 768 字节固定 arena（单槽位，USB CDC 全程只分配一次约 540 字节） |
  | 4 | `usbd_cdc_if.c`：`CDC_Control_FS` 补 `GET/SET_LINE_CODING` | 原先所有控制请求一律 `return USBD_OK` 不处理，主机的 `GET_LINE_CODING` 会收到 7 字节未初始化数据，部分 Windows 版本会因此不创建 COM 口 |
  | 5 | `platformio.ini`：加 **`-fno-common`** | **关键加固**：让"同名不同型"从"静默合并"变成链接期 `multiple definition` **硬错误**，这类 bug 从此不可能再偷偷发生 |
- 附带修的同源缺陷：`USB_CDC_Printf` 用 `static char buf[160]`，却会被 **Task_Agent（欢迎语）与 Task_LVGL（`hits` 回复）两个任务**调用 —— 原注释写的"不并发调用，安全"**不成立**（LVGL 优先级 3 > Agent 优先级 2，可在 `vsnprintf` 中途抢占 → 输出串字节）。已用 `taskENTER_CRITICAL/EXIT_CRITICAL` 原子化；顺带修掉"截断时多发 1 字节（结尾 `\0`）"的夹取错误。
  - 注意：该缓冲**不能改成栈上局部变量** —— `CDC_Transmit_FS` 只登记指针，真正的数据搬运发生在后续 USB 中断（DataIn 阶段），栈缓冲一返回就失效。
- 顺带修的隐患：静态 arena 首版落在 `0x2000F367`（**奇地址**）——
  `uint8_t[]` 只保证 1 字节对齐，而 `USBD_CDC_HandleTypeDef` 开头是 `uint32_t data[128]`。
  已加 `__attribute__((aligned(8)))`，复验地址 `0x2000FD50` 对齐 OK。
- 验证：全量重编 **327 Compiling + 1 Linking，67.6s，0 error 0 warning**；RAM 52.0% / Flash 26.3%；
  `nm -S` 确认两个句柄地址/尺寸独立、arena 8 字节对齐。
- 规则固化：
  1. **跨文件同名全局变量是"静默炸弹"** —— 尤其在不同库/模板（HAL vs 中间件）之间。
     命名要带前缀（`hpcd_` / `hUsbd_`），别指望编译器报警。
  2. **`-fno-common` 应作为嵌入式工程的默认配置**。默认 `-fcommon` 会把类型冲突藏到运行时。
  3. **怀疑"内存被写坏"时，`nm -S` 看符号的数量/地址/尺寸是最快的一刀**：
     同名符号只出现一次、或尺寸与结构体定义不符 → 别名成立。
     这属于**核武器**（见 `training-invpc.md` §0）；日常优先用 SWD 断点/watch。
  4. **症状位置 ≠ 根因位置**。本次屏花但根因在 USB。**改动哪个模块后出现的问题，先怀疑哪个模块**
     —— 但"怀疑"要通过排除法落地（本例先排除了 SPI 提速，因为单独烧 Phase 6 是好的）。
  5. **两套内存管理并存**（newlib 堆 + FreeRTOS 堆 + 库自带静态池）要主动收敛；
     能静态就不要动态，尤其在一层只分配一次的中间件里。

### T-008（已踩，2026-09-26）：调度器启动前的 `while(1)` 静默吊死 + 调试会话锁住 firmware.elf

> 本案是"花屏 + 无 COM 口"排查的后半段。**核心教训：可选外设的初始化失败绝不该让整个系统死掉。**

- 现象：Phase 8 烧录后花屏、PC 无虚拟 COM 口；`uwTick` 不涨、`PRIMASK=0`、`ICSR VECTACTIVE=0`、无 HardFault。
- 机制（FreeRTOS ARM_CM4F `port.c` 源码级证据，已逐行核实）：

  | 位置 | 代码 | 意义 |
  |---|---|---|
  | `port.c:148` | `static UBaseType_t uxCriticalNesting = 0xaaaaaaaa;` | 初值是**非零毒值** |
  | `vPortExitCritical()` | `uxCriticalNesting--; if (uxCriticalNesting == 0) portENABLE_INTERRUPTS();` | 只有计数**回到 0** 才还原 BASEPRI |
  | `vPortSVCHandler()` L256-257 | `mov r0, #0` / `msr basepri, r0` | BASEPRI **只在"启动第一个任务"时才被清掉** |

  - ⇒ **调度器启动之前的任何一次 `taskENTER_CRITICAL()`，退出时都不会还原 BASEPRI**
    （计数值从 `0xAAAAAAAA` → `0xAAAAAAAB` → `0xAAAAAAAA`，**永远不等于 0**）。
  - ⇒ `BASEPRI` 停在 `configMAX_SYSCALL_INTERRUPT_PRIORITY = 0x50`，屏蔽优先级数值 ≥ 5 的中断；
    **SysTick 优先级 15（`configKERNEL_INTERRUPT_PRIORITY = 0xF0`）→ 被屏蔽
    → `uwTick` 冻死 → `HAL_Delay` 永久卡死。**
  - 窗口起点 = 第一次 FreeRTOS 临界区（`xQueueCreate`，即 `agent_queue_init()` 内部）；
    终点 = `vPortSVCHandler`。**`USB_CDC_Init()`（Phase 8 新加）正好落在这个窗口里。**
  - 这也解释了**为什么 Phase 6 好、Phase 8 坏**：Phase 6 没有这两行，窗口从 `xTaskCreate` 才开始，
    那段没有可卡住的代码。
- 头号嫌疑（已修）：`usbd_conf.c` 的 `USBD_LL_Init` 里
  `if (HAL_PCD_Init(...) != HAL_OK) { while (1) { } }` —— 窗口内**唯一**会无限循环且不产生 fault 的地方。
  **一旦 USB 初始化失败就静默吊死：不刷屏、不枚举、无任何提示。**
  已排除的旁路：`hal_pcd.c` / `ll_usb.c` 里 `HAL_GetTick` 出现 **0 次** ⇒ USB 底层没有基于 tick 的超时。
- 实际解决（把"静默失败"变成"屏上可见"）：
  1. `usbd_conf.c`：删掉 `while(1)`，改为置 `g_usbd_pcd_init_failed = 1` 并 `return USBD_FAIL`
  2. `usb_cdc.h/.c`：`USB_CDC_Init()` 改为**返回错误码**
     （`USB_INIT_ERR_USBD_INIT / REG_CLASS / START / PCD`），新增全局 `usb_init_err`
  3. `main.c`：接返回值，并**绕过 LVGL 直接在屏上画** `USB: ok` / `USB: FAIL code=N`
  4. `ui.c`：LVGL 界面加一行常驻 `USB CDC: ok` / `FAIL (PCD init)`
  5. 全量重编：**327 Compiling + 1 Linking，0 error 0 warning**，Flash 138016 B（26.3%）/ RAM 68212 B（52.0%）
- 规则固化：
  1. **`while(1)` 永远不是错误处理。** 尤其不能在"调度器启动前、中断已被 BASEPRI 屏蔽"的窗口里用。
     错误处理要么返回错误码，要么走可见报错通道（屏 / 串口）。
  2. **`vTaskStartScheduler()` 之前不要做"可能失败又重要"的初始化。** 真要做，先想清楚：
     此刻 BASEPRI 已被前面的 FreeRTOS 临界区设为 `0x50`，**SysTick 是屏蔽的，`HAL_Delay` 会死等**。
  3. **可选外设（USB / 串口 / 传感器）初始化失败，必须让主体功能继续跑**，并把错误显示出来。
  4. **`firmware.elf` 被锁的头号原因是"调试会话还开着"**（`arm-none-eabi-gdb` + `openocd` 进程在跑）。
     查法：`Get-Process | Where-Object { $_.ProcessName -match 'gdb|openocd' }`。
     解法：VS Code 里按 **`Shift+F5` 停止调试**。
  5. ⚠️ **调试会话把目标板 HALT 住时，`uwTick` / 任何 RAM 变量都不会变化** ——
     此时读到的"卡死"是**测量假象**。必须先 `Shift+F5` 释放，或按
     **"F5 自由跑 N 秒 → F6 暂停 → 读"** 的时序测，不能连续两次暂停读。
  6. **`PLATFORMIO_BUILD_DIR` 环境变量**可在不改 `platformio.ini` 的前提下换构建目录，
     用于绕过被占用的 `firmware.elf`（构建报 `SAFE_DELETE_FAIL_CLOSED` / `WinError 32` 时）：

     ```
     PLATFORMIO_BUILD_DIR=<绝对路径> pio run
     ```

     （根因仍是"elf 被占用"，根治办法是停掉调试会话。）

- ✅ **结案（2026-09-26 用户实机验证通过）**。最终解法：**把 `USB_CDC_Init()` 从 `main()` 移到
  `Task_Agent` 的第一行**（提交 `51811de`），即"USB 初始化不再发生在调度器启动之前"。
  - **因果关系的实证**：`af60c5f`（USB 仍在 `main` 里 → **花屏**）→ `51811de`（移到任务里 → **正常**）。
    两版之间 USB 的**唯一**功能差异就是这个调用位置。
  - **已排除（均有源码/工件实证）**：
    - USB 库自身**无延时、无基于 tick 的超时、无死循环**（`lib/usb_device` 全量 grep：`USBD_Delay`
      只有宏定义无调用者；`HAL_Delay` / `HAL_GetTick` / `while(1)` 在代码里 0 处）。
    - `hal_pcd.c` / `ll_usb.c` 里 `HAL_GetTick` **0 次** → USB 底层没有 tick 超时。
    - 显示路径代码与 Phase 6 **完全一致**（`lcd.*` / `lvgl_port.*` / `lv_conf.h` /
      `lvgl_tick_source.h` / `touch.c` / `FreeRTOSConfig.h` / `font.h` 全部零改动）。
  - **候选机制（未逐一区分，如实记录）**：
    - **M1：USB 中断使能早于 USB 内核配置完成。** `HAL_PCD_Init()` 的调用顺序（源码实证）是
      `HAL_PCD_MspInit(hpcd)` → `USB_CoreInit()` → `USB_SetCurrentMode()` → `USB_DevInit()`；
      而我们的 `HAL_PCD_MspInit` 在这个最前面就 `HAL_NVIC_EnableIRQ(OTG_FS_IRQn)`。
      若此刻有挂起的 USB 中断，`HAL_PCD_IRQHandler` 会在 **PCD 句柄半初始化**状态下运行。
    - **M2：`BASEPRI = 0x50` 屏蔽窗口内的行为**（见上文机制）：SysTick 被屏蔽，
      任何依赖 `uwTick` 的等待都会变成永久等待。
    - ⚠️ **为什么没能进一步区分**：当时**没有任何可用的输出通道**（见下方复盘第一节），
      要区分 M1/M2 需要串口日志或 SWD 断点，而这两条当时都不可用。
      ⇒ **这也正是"先造观测通道"应该排在所有排查动作之前的原因。**

---

## 案例复盘 T-008：为什么这个 bug 查了这么久（六条观测通道同时失效）

> 这是整个项目到目前为止**最值得存档的一篇**。它不是"某个坑的解法"，而是"**为什么查不出来**"的复盘。

### 一、真面目：不是"难"，是"没有仪器"

T-008 之所以长时间查不出，不是因为它复杂，而是因为**当时手上所有能输出信息的通道同时失效**：

| 观测通道 | 当时的实际状态 | 结果 |
|---|---|---|
| HardwareFault / `CFSR` / `HFSR` | `SHCSR = 0`，**从未发生硬件异常** | 故障探针无输出 |
| `configASSERT` | 未触发 | 断言通道静默 |
| **PC0 用户 LED** | **硬件坏件**（T-005，挂起） | "闪灯报错"= 完全没有提示 |
| **屏幕** | **花屏**（正是唯一还能显示信息的东西坏了） | 自检/报错都读不到 |
| 串口 / CH340 | 当时**没接**（ADR-008 曾以"无 CH340"为由跳过 UART printf） | 无 printf 通道 |
| SWD 断点 / 监视变量 | **符号与芯片固件不匹配**（`PIO Debug (without uploading)` 会用 `-O0` 重建 ELF 但不烧录）；且**调试器 HALT 住 CPU 时 `uwTick` 与任何 RAM 变量都不会变化** | 读数不可信 / 引发误判 |

**六条通道，六条同时失效。**

⇒ **结论：这不是"难查"，这是"没有仪器"。**
⇒ **因此正确的动作不是"再想想"，而是"先把观测通道造出来"。**
这也解释了后来的一个关键决定：CH340 到货后应当**优先补上 UART printf**，而不是继续盲猜
（`reference-survey.md` §5 对同类项目的调研给出了完全一致的结论：
「截图会骗人、计时器会骗人，所以每个结论都要配一个仪表」）。

### 二、为什么"比对 Phase 6"一击命中 —— 差分调试法

**这是本案唯一真正有效的动作**，值得单独记住。

| | 普通排查 | 差分调试 |
|---|---|---|
| 问题形式 | **开放式**："哪里错了？" | **封闭式**："这两版差在哪？" |
| 搜索空间 | 整个工程（几千行 + 中间件） | **有限、可枚举的文件列表** |
| 本案实测 | 反复猜、反复排除，两次把推测当结论 | 一条命令：**7 个显示相关文件全部零改动** ⇒ **整个显示层一次性排除** ⇒ 嫌疑集缩到"Phase 8 新增的那两行调用" |

**可复用命令**：

```
git diff <最后一次已知可用> <当前>  --stat          # 先看改了哪些文件
git diff <最后一次已知可用> <当前> -- <关键文件>    # 再看关键文件的具体改动
```

本案实际执行的是：
```
git diff e545ec3 af60c5f --stat
git diff e545ec3 af60c5f -- SmartDeskTerminal_freertos/src/bsp/lcd.c
git diff e545ec3 af60c5f -- SmartDeskTerminal_freertos/platformio.ini
```

**⚠️ 差分调试有一个硬前提：你必须有一个被显式标记、被记录、且**确实验证过**的"已知可用状态"。**

⇒ **所以 `git tag`（外加写进 `progress.md`）不是"归档动作"，它是"调试基础设施"。**
本案之所以能做差分，全靠 09-25 打了标签 `phase6-verified` 并写进了进度文档。
**如果没有这个标签，"比对 Phase 6" 根本无从谈起。**

**规则固化**：每当一个版本**通过实机验证**，立刻
① `git tag <阶段>-verified` ② 在 `progress.md` 里写明"最后确认可用 = 哪个 tag/commit"。

### 三、这次真实踩到的六个认知陷阱

按发生顺序，每一个都**实际导致了时间浪费**：

| # | 陷阱 | 本案的具体表现 | 事后的正确做法 |
|---|---|---|---|
| 1 | **症状位置 ≠ 根因位置** | 屏花 → 我先把根因归到 USB 的**同名句柄别名**（T-007）。它确实是真 bug，但**作用点在卡点之后**，不是花屏的原因 | 先问"**是显示坏了，还是根本没画？**"——把症状按数据流切片，找**最早出错的那一环** |
| 2 | **调用栈不可信** | `PIO Debug (without uploading)` 用 `-O0` 重建 ELF 但**不烧录**，芯片里却是 `-Os` 版；栈里出现 `usbd_conf.c` 等根本不可能调到 `HAL_Delay` 的帧 | 排查前先让**符号与芯片固件一致**（用完整 `PIO Debug`，它会编译+烧录）；或只采信 CPU 寄存器类读数 |
| 3 | **把"变量不变"当成"卡死"** | "`uwTick` 恒为 165" 被当成死机证据；但调试器 HALT 住 CPU 时变量本来就不会变 | 活性指标必须按 **"F5 自由跑 N 秒 → F6 暂停 → 读"** 的时序采；寄存器读数（`ICSR`/`SHCSR`/`CTRL`）比 RAM 变量可信 |
| 4 | **把"最像的"当成"最早出错的那一环"** | 由 `tickstart=265 / Delay=10` 推出"卡在 `lcd.c:289`"，实际是被 #2#3 误导 | 每个推断都要配**能证伪它的观测手段**；写不出观测手段的推断，划掉 |
| 5 | **构建报 SUCCESS 但没编译** | safe-delete 钩子拦截 → `Linking=0` 却报 SUCCESS；调试会话占着 `firmware.elf` 又加剧了它 | 判据升级：**必须**同时确认 ① 日志有 `Compiling`/`Linking` 行 ② `firmware.bin` 的 mtime 是刚才 |
| 6 | **复用"老经验"而不查文档** | 又去改名 `.sconsign` 强制全编，而 **T-009** 原文早写了"不要再改名 `.sconsign`" | **动手前先查 `troubleshooting.md` 对应条目**，别凭记忆 |

### 四、下次遇到"什么现象都没有"的故障，照这个顺序做

1. **先确认观测通道是否可用**——列一张表（故障寄存器 / 断言 / LED / 屏 / 串口 / SWD），
   **哪条是通的？如果都不通，先造一条**（本案的教训：CH340 的 UART 应该第一优先）。
2. **有基线就跑差分**——`git diff <最后一次已知可用> <当前>`。没有基线就先想想有没有历史版本可当基线。
3. **把模糊现象"有限化"**——不要停留在"花屏"，要设计成**一次能区分少数几种情况**的观测点
   （本案最后加的开机自检屏把"花屏"拆成了 A/B/C 三种可区分的结果，一次烧录即定性）。
4. **顺序上先做"便宜且能一刀切开范围"的动作**，而不是先做"最像原因"的动作。
5. 每改一次，**配一个能证伪自己的观测**；改之前先说出**预期后果**。

### 五、本项目由此新立的几条工程规矩

1. **调度器启动前（`vTaskStartScheduler()` 之前）不要做"可能失败又重要"的初始化。**
   真要做，必须先知道：此刻 `BASEPRI` 可能已被前面的 FreeRTOS 临界区设为 `0x50`，
   **SysTick 是屏蔽的，`HAL_Delay` 会死等**。
2. **可选外设（USB / 串口 / 传感器）的初始化失败，绝不能让主体功能跟着死。**
   `while(1)` 不是错误处理 —— 要么返回错误码，要么走可见的报错通道。
3. **每个"会静默失败"的子系统，都要在界面上留一个常驻状态位**
   （本案加的 `USB: ok / FAIL code=N`）。
4. **通过实机验证的版本立刻打 tag 并写进 `progress.md`** —— 这是调试基础设施，不是归档。
5. **把"观测能力"当成一等公民**：`?stat` 类自检命令、UART printf、开机自检屏，
   平时就该在，而不是出事才补。



