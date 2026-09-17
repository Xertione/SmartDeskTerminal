# 进度

- 当前阶段：Phase 1 工程初始化与基础运行环境
- 状态：进行中
- 最后更新：2026-09-17

## 已完成

- [x] PlatformIO 工程骨架（`framework = stm32cube`，board = black_f407ve）
- [x] 硬件齐备（F407VE 黑板 / ST-Link / ST7789 屏 / USB-C 线）
- [x] 路线决策：PIO + stm32cube HAL，Phase 3 引入 FreeRTOS（A/C 趋同，见 [decision-log.md](decision-log.md) ADR-002）
- [x] 文档骨架初始化（迁至 `doc/项目文档/`，6 文件）
- [x] LED 引脚查实：**PC0**（用户 LED 蓝光），原理图 + 官方例程双验证

## 正在做

准备做板子可靠性检查（烧 LED 闪烁，验证板子无虚焊/物理错误）。

## 下一步计划

1. ~~查 LED 引脚~~ ✅ 已查实：PC0，见 `doc/datasheets_md/06_核心板_原理图与引脚映射.md` 第 2 节
2. AI 写最小 LED 闪烁代码（`HAL_GPIO_TogglePin` 操作 PC0；参考官方例程 `SystemClock_Config`：HSE 8MHz→PLL→168MHz），放进 `SmartDeskTerminal_freertos/src/main.c`
3. `pio run` 编译，过则进下一步，错则查 [troubleshooting.md](troubleshooting.md)
4. `pio run -t upload` 经 ST-Link 烧录
5. 看 PC0 蓝灯按预期闪烁 = 板子硬件 OK，Phase 1 收尾

## 阻塞问题

无。

## 最近一次可运行状态

暂无（尚未烧录过任何程序，src/main.c 为空）。首次跑通后在此填精确命令 + 现象。
