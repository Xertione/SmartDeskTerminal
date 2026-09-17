# Decision Log

> 每条必写"后果"段。面试官必追问"这方案有什么代价"，缺这段的 ADR 是半成品。

## ADR-001：framework 路线选 C（PIO + HAL + FreeRTOS 库混合）

- 日期：2026-09-17
- 背景：plan.md 原写 HAL 路线，但误建了 `framework = zephyr` 工程（Zephyr 本身是 RTOS，与 plan 的"引入 FreeRTOS"冲突）。废弃后重建为 `framework = stm32cube`（HAL）。FreeRTOS 怎么引入有三条路：
  - A：纯 PIO + HAL，FreeRTOS 留到 Phase 3 用 `platformio lib install` 手加
  - B：CubeMX GUI 生成 F407+FreeRTOS+外设，一步到位（生成 .ioc + Core/Drivers/Middlewares）
  - C：PIO + HAL + FreeRTOS 库混合（保留 PIO 工作流，手加 FreeRTOS package）
- 决定：选 **C**
- 原因：用户希望"AI 快速写好代码验证 → 自己再学习复刻"，保留 VS Code + PlatformIO 工作流，不另学 CubeMX GUI；同时手写 HAL 练寄存器理解。
- 后果：
  - **优点**：保留单一 PIO 工作流；HAL 手写练底层；FreeRTOS 在 Phase 3 用库形式加，可控
  - **代价（关键风险）**：FreeRTOS 在 PIO 里集成需手动配置——`configTOTAL_HEAP_SIZE`（堆大小）、`configMAX_SYSCALL_INTERRUPT_PRIORITY`（可屏蔽中断优先级）、SysTick 与 HAL 时基冲突、PendSV/SVC 优先级。**这些是配置层坑，AI 写代码绕不过去，必须自己理解并配对**。Zest 之前明确标 C 为"新手坑最多、不推荐"，用户已知风险仍选 C，理由是优先保留 PIO 工作流 + 学习复刻策略。
  - **后续触发**：到 Phase 3 真正引入 FreeRTOS 时，若集成受阻 >2 小时，考虑回退到 B（迁 CubeMX）。

## ADR-002：A 与 C 路线趋同，不再纠结标签

- 日期：2026-09-17
- 背景：用户指出 A（PIO+HAL，FreeRTOS 后期手加）与 C（PIO+HAL+FreeRTOS 库混合）实操趋同——A 起步到 Phase 3 加 FreeRTOS 后就是 C，区别只在引入时机的标签，对 Phase 1-2 实操无影响。
- 决定：不再用 A/C 标签，统一表述为「PIO + stm32cube HAL，Phase 3 用库方式引入 FreeRTOS」。
- 原因：用户判断正确，Phase 1-2 阶段两者完全相同（纯 HAL），标签之争无意义。
- 后果：
  - **优点**：路线表述简化，不再为 A/C 标签纠结
  - **代价（不变）**：FreeRTOS 集成配置坑（堆/中断优先级/SysTick）依然存在，与叫 A 还是 C 无关——Phase 3 必然遇到。ADR-001 的退路（受阻 >2h 迁 CubeMX）仍有效。

## ADR-NNN：<待追加>

> 做下一个取舍时在此追加。
