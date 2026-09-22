# SmartDeskTerminal：AI 协作规范

## 先读什么

处理本项目的开发 / 排错 / 评审任务前，先读：

1. [progress.md](progress.md) —— 当前进度
2. [decision-log.md](decision-log.md) —— 已定的取舍
3. [troubleshooting.md](troubleshooting.md) —— 已踩/预期坑
4. [wiring.md](wiring.md) —— **物理接线施工单**（拿着杜邦线照着插；模块针脚 → 核心板排针 + 电气红线）
5. [plan.md](plan.md) —— 总规划与 10 Phase（同目录）

> spec.md 暂未建（需求未冻结，由 plan.md 充当）。

## 工作边界

- 中文沟通、中文注释
- 一次只做一个可验证的小功能；需求不明确先提问，不替用户拍板
- 不擅自引入未批准的技术（FreeRTOS/LVGL/USB CDC 各自到对应 Phase 才引入）
- 不删除 / 重命名 / 移动现有文件，除非明确要求
- 受保护目录约束：桌面下已存在文件不可改/删，新文件可建；改 doc/ 下硬件资料前先确认

## 文档更新规则

| 场景 | 更新文件 | 方式 |
|---|---|---|
| 做完一个功能 / Phase 推进 | progress.md | 覆盖（删过期，只留一屏） |
| 遇到并解决问题（排查 >20 分钟） | troubleshooting.md | 追加（带 T-NNN 编号） |
| 改变重要方案 / 选型 | decision-log.md | 追加（带 ADR-NNN，必写后果） |
| 顿悟 / 概念理清 | notes.md | 追加（必须用自己的话） |
| 硬件引脚 / 时钟 / 参数查实 | 对应文件待填位 | 原地填 |
| **接线变更（新接/改接/拆线）** | wiring.md | 覆盖对应小节 + 更新 §1 进度表 |

禁止：同一事实写在两个文件里。发现两处描述同一事实 = 删掉一处。

## 环境坑

统一放 `~/.workbuddy/ENV-NOTES.md`，本工程内只留指针，不复述。

## 上下文恢复模板

我正在开发 SmartDeskTerminal（STM32F407 智能桌面协同终端）。
请先读 progress.md / decision-log.md / troubleshooting.md / wiring.md / plan.md。
当前做到：<填，例如 Phase 1 LED 闪烁已烧录成功>
本次只完成：<一个可验证的小目标，例如让 LED 以 500ms 闪烁>
先不要写代码，先分析：涉及哪些模块、改哪些文件、风险点、我先该理解什么。
