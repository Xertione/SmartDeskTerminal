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

### T-005：烧录成功但 PC0 蓝灯不亮

- 日期：2026-09-18
- 现象：`pio run -t upload` 烧录成功（ST-LinkV2 闪三次 + PIO 终端提示成功），但 PC0 蓝灯完全不亮，无闪烁。
- 代码：`SmartDeskTerminal_freertos/src/main.c` 完整（PLLQ=7 / HSE_VALUE 宏 / SysTick_Handler 都到位），逻辑无明显错误。
- 排查方向（按可能性）：
  1. **时钟配置失败进 Error_Handler 死循环**——HSE 8MHz 物理未起振（晶振虚焊/坏）→ `HAL_RCC_OscConfig` 返回 != HAL_OK → 进 `Error_Handler` while(1)，`LED_GPIO_Init` 根本没执行
  2. LED 极性 / 硬件坏——但 `HAL_GPIO_TogglePin` 应至少看到微光或反相闪烁，完全不亮可能性低
  3. HSE_VALUE 宏未传到编译——不影响 PLL 配置（PLLM 是除数），但影响 HAL_Delay 精度
- 待验证：改 `Error_Handler` 加 LED 快闪指示，若快闪 = 时钟失败，定位到方向 1
- 根因：待填
- 解决：待填
- 规则固化：待填

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
