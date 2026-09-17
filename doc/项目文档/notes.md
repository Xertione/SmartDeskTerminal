# Notes（学习留痕）

> 必须用自己的话写，禁止粘贴 AI 输出。这是「会用 AI 写代码」和「真的会」之间的分界线。

## 1. CubeMX / HAL / FreeRTOS 是三个不同层面的东西

- 我原来以为：CubeMX 是"框架"，FreeRTOS 是框架里的一部分，建工程时选了 CubeMX 就等于有了 FreeRTOS。
- 实际上是：三者是独立的东西——
  - **CubeMX** 是 STM32 官方的 GUI 配置工具，画图配引脚/时钟/外设，**生成代码**（产出 .ioc 文件 + Core/Src/main.c）。它是"代码生成器"，不是框架。
  - **HAL**（`stm32cube` framework）是 STM32 硬件抽象库，提供操作 GPIO/串口/SPI 的函数集（比如 `HAL_GPIO_WritePin`）。它是"驱动框架"。
  - **FreeRTOS** 是实时操作系统，管任务调度/队列/信号量。它是"RTOS"，叠在 HAL 之上。
- 为什么会混：因为 STM32Cube 生态把三者打包在一起推（CubeMX + HAL + 可选 FreeRTOS），名字都带 Cube/STM32，新手容易以为是同一个东西的不同叫法。
- 用我自己的话讲一遍：CubeMX 是"画图生成代码的画板"，HAL 是"操控硬件的函数库"，FreeRTOS 是"管多任务的调度器"。我现在这个工程里只有 HAL（`framework=stm32cube`），没有 CubeMX 生成的 .ioc，也没有 FreeRTOS——它俩都得单独再引入。
- 如果面试官追问，我会说：选 framework 时不能只看名字带不带 Cube，要看 `platformio.ini` 的 `framework` 字段实际对应哪个栈；`stm32cube` = HAL 库，`zephyr` = Zephyr RTOS，两者不兼容；FreeRTOS 在 HAL 之上是另一层，要单独配。

## <待追加>

> 下一个"原来是这样"的顿悟在此追加。
