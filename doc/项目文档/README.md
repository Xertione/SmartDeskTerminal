# SmartDeskTerminal

> STM32F407 智能桌面协同终端 MVP：MCU 侧采集/显示，PC 侧 Python Agent 推送事件，USB CDC 互连。

## 项目状态

**Phase 1/2/5/7 + Phase 3+4 已完成实机验证** —— 三任务 FreeRTOS 调度正常，屏上文字/触摸按钮/计数/心跳齐全。

下一个要做的：**Phase 6 LVGL**。详见 [progress.md](progress.md)。

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

## 项目文档导航（`doc/项目文档/`）

| 文件 | 用途 |
|---|---|
| [progress.md](progress.md) | **当前进度（以它为准）** |
| [plan.md](plan.md) | 10 个 Phase 的目标与产物 |
| [decision-log.md](decision-log.md) | 为什么这样选而不是那样选（ADR） |
| [troubleshooting.md](troubleshooting.md) | 已踩的坑（T-NNN） |
| [wiring.md](wiring.md) | **物理接线施工单**（拿着杜邦线照着插） |
| [debug-manual.md](debug-manual.md) | SWD 调试与自助烧录 |
| [training-invpc.md](training-invpc.md) | **调试训练任务书**（T1~T13，不含答案；一次训练材料，做完可删） |
| [AGENTS.md](AGENTS.md) | AI 协作规范 + 文档更新规则 + 提交前检查清单 |
