# SmartDeskTerminal

> STM32F407 智能桌面协同终端 MVP：MCU 侧采集/显示，PC 侧 Python Agent 推送事件，USB CDC 互连。

## 快速开始

前置：
- PlatformIO（VS Code 扩展）
- ST-Link 驱动
- 板子：F407VET6 黑板 + ST7789 屏 + ST-Link + USB-C 线

最小命令（目标态，当前 src 尚空，还跑不起来）：

```bash
pio run              # 编译
pio run -t upload    # 烧录到板子
pio device monitor   # 看串口输出
```

> 当前无可运行状态，见 [progress.md](progress.md)。

## 技术栈

- MCU：STM32F407VET6（Cortex-M4，168MHz）
- 框架：PlatformIO + `stm32cube` HAL
- RTOS：FreeRTOS（Phase 3 引入，PIO 库形式手加；A/C 路线趋同，见 [decision-log.md](decision-log.md) ADR-002）
- GUI：LVGL（Phase 6）
- 通信：USB CDC（Phase 8）
- PC 端：Python Agent（Phase 9，psutil + 串口）

## 我为什么做它

补齐自己对寄存器操作和 RTOS 的理解——会 C、用过 STM32，但底层和实时系统不熟。用一个真实闭环（硬件采集 → 显示 → PC 互连）把这些知识钉死，而不是停留在"会用 AI 写代码"层面。

## 当前状态

见 [progress.md](progress.md)。

## 项目规划

总规划见工作区根 [../../plan.md](../../plan.md)（10 个 Phase）。
