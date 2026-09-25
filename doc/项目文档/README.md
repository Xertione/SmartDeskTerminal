# SmartDeskTerminal

> STM32F407 智能桌面协同终端 MVP：MCU 侧采集/显示，PC 侧 Python Agent 推送事件，USB CDC 互连。

## 项目状态

**Phase 1/2/5/7 + Phase 3+4 + Phase 6(LVGL) 已完成实机验证** —— LVGL 界面/触摸/FPS/心跳全部正常。

**Phase 8（USB CDC）代码已写完并修掉 3 个缺陷，待实机复验**（详见 [progress.md](progress.md) 与
[troubleshooting.md](troubleshooting.md) T-007）。

下一个要做的：**Phase 8 实机复验 → 通信协议落地 → Phase 9 PC Agent**（hooks 触发机制待定）。

> ⚠️ 历史遗留问题（2026-09-25 发现并已修）：`lib/usb_device/` 曾被 `.gitignore` 整目录排除，
> 导致 USB 中间件从未入库、fresh clone 无法编译。现已入库。**新增 vendor 中间件时务必 git status 核对。**

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
| [troubleshooting.md](troubleshooting.md) | 已踩的坑（T-NNN）+ **T-008 案例复盘：为什么这个 bug 查了这么久**（六条观测通道同时失效 / 差分调试法） |
| [wiring.md](wiring.md) | **物理接线施工单**（拿着杜邦线照着插） |
| [debug-manual.md](debug-manual.md) | SWD 调试与自助烧录 |
| [training-invpc.md](training-invpc.md) | **调试训练任务书**（T0~T11，不含答案；含项目结构总览 + 完整调用链表；一次训练材料，做完可删） |
| [AGENTS.md](AGENTS.md) | AI 协作规范 + 文档更新规则 + 提交前检查清单 |
