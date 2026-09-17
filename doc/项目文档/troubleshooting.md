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

## 预期坑预警（按 Phase 预填）

### T-002（待踩）：FreeRTOS 在 PIO 里集成配置

- 预期 Phase：3
- 预期现象：引入 FreeRTOS 后编译过但跑不起来 / HardFault / 任务不调度
- 预期根因：① `configTOTAL_HEAP_SIZE` 太小 ② `configMAX_SYSCALL_INTERRUPT_PRIORITY` 配错致临界区失效 ③ SysTick 被 HAL 和 FreeRTOS 双重占用 ④ PendSV/SVC 优先级未设最低
- 预防：引入前先读 FreeRTOS 官方《Cortex-M3/M4 porting》一节；对照 STM32Cube 的 FreeRTOSConfig.h 模板
- 待踩后补：实际现象 / 实际根因 / 实际解决

### T-003（待踩）：ST7789 SPI 驱动时序

- 预期 Phase：5
- 预期现象：屏不亮 / 花屏 / 颜色反
- 预期根因：SPI 时钟极性/相位（CPOL/CPHA）错；DC/RES 时序错；数据/命令未区分
- 预防：先抄 doc/ 里对应分辨率的官方 ST7789 例程，别从零写
- 待踩后补：见上

### T-004（待踩）：LVGL 与 FreeRTOS 的 tick 绑定

- 预期 Phase：6
- 预期现象：LVGL 界面不刷新 / 抖动
- 预期根因：`lv_tick_inc()` 没在稳定节拍里调；`lv_timer_handler()` 调用频率不对
- 预防：用 FreeRTOS 的 1ms 软定时器或 task delay 驱动 lv_tick
- 待踩后补：见上
