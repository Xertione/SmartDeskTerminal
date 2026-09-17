# SmartDeskTerminal

> STM32F407 智能桌面协同终端 MVP：MCU 侧采集/显示，PC 侧 Python Agent 推送事件，USB CDC 互连。

## 项目状态

Phase 1 已收尾（编译/烧录/运行全通），进 Phase 2。详见 [progress.md](progress.md)。

## 快速开始

前置：
- PlatformIO（VS Code 扩展）
- ST-Link 驱动
- 板子：F407VET6 黑板 + ST7789 屏 + ST-Link + USB-C 线

```bash
cd SmartDeskTerminal_freertos
pio run              # 编译
pio run -t upload    # 烧录到板子（ST-Link）
```

## 技术栈

- MCU：STM32F407VET6（Cortex-M4，168MHz）
- 框架：PlatformIO + `stm32cube` HAL
- RTOS：FreeRTOS（Phase 3 引入，PIO 库形式手加；A/C 路线趋同，见 [decision-log.md](decision-log.md) ADR-002）
- GUI：LVGL（Phase 6）
- 通信：USB CDC（Phase 8）
- PC 端：Python Agent（Phase 9，psutil + 串口）

## 硬件资料

- `doc/datasheets_md/` —— 翻译整理的精简版数据手册（11 个 md，repo 跟踪）
- `doc/核心板资料/` + `doc/屏幕资料/` —— 原厂完整资料（PDF/例程源码/工具，**磁盘保留但 repo 不跟踪**，见 ADR-007）

## 项目规划

总规划见 [plan.md](plan.md)（同目录，10 个 Phase）。
