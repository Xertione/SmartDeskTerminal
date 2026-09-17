# SmartDeskTerminal 项目长期记忆

## 项目概况
- 目标：30 天 MVP，STM32F407 + FreeRTOS + LVGL + USB CDC + Python Agent 智能桌面协同终端。
- 路线：10 个 Phase（工程初始化 → BSP → FreeRTOS → 任务通信 → LCD → LVGL → 触摸 → USB CDC → PC Agent → 整合）。详见 `plan.md`。
- 与用户 Java 后端目标**独立**，本项目的会话不要扯 Java/DataAgent。

## 技术栈/路线
- 当前工程：`SmartDeskTerminal_freertos/`（PlatformIO + `framework=stm32cube`，HAL 路线）。
- 旧工程 `SmartDeskTerminal/`（Zephyr）已废弃，别再看它。
- 核心板预设：`board = black_f407ve`（野火/通用 F407VET6 黑板）。

## 硬件清单（已到手）
- F407VE 黑板、ST-Link 调试器、ST7789 屏、USB Type-C 线。

## doc/ 资料结构
- `doc/核心板资料/.../HAL库/` 下有官方参考例程：LED 闪烁、按键、串口打印、RTC、USB 虚拟串口，以及多分辨率 ST7789 屏驱动例程。**可直接参考**。
- **`doc/datasheets_md/`（已整理的硬件手册 md，优先查这里，不要重复解析 PDF）**：
  - `README.md` 索引；`00_无法自动读取清单.md` 记录未读到/未搬运的内容。
  - `06_核心板_原理图与引脚映射.md` 是最常用的一份（排针/屏接口/SWD 全映射）。
  - 整理方法与证据：常规 PDF 提文字层；图片型 PDF 渲染读图；原理图用**矢量坐标重建连线**；核心板映射已与背面丝印照片交叉验证。
  - 中间产物在 `.workbuddy/tmp/dump/`（临时，可清）。

## 硬件关键事实（已核实，勿再推测）
- 核心板型号 `LXB407VE-P1`（鹿小班科技），主控 STM32F407VET6。
- 两组 2×21P 排针：**U5** 引出 +5V/3.3V 与 GPIO，**U6** 前 4 脚为 GND；合计引出 76 个 GPIO（仅 PA13/PA14/PH0/PH1/PC14/PC15 未引出）。
- **SPI 屏接口 U4（12pin FPC）**：SCK=PB3、MOSI=PB5、CS=PA15、DC=PD13、BL=PD12；**无 MISO、无屏 RESET**。
- **FSMC 屏接口 FPC1（32pin FPC）**：含触摸 T_SCK_PA5/T_MISO_PA6/T_MOSI_PA7/T_PEN_PB8/T_CS_PB9 与 FSMC_D0~D15、LCD_RST_PC2。
- `LCD_BL_PD12` 被两个屏接口**共用** → 同一时刻只能用一种屏。
- SWD/串口接口 H1（2×4P）：1 SWCLK_PA14 / 2 SWDIO_PA13 / 3 GND / 4 +5V / 5 USART1_RX_PA10 / 6 USART1_TX_PA9 / 7 RST(经1K) / 8 3.3V。
- 板载 3V3 对外输出**上限 500mA**；屏背光（4 颗白光 LED 并联）典型 **100mA@3.1V**。
- **doc 里没有 STM32F407 数据手册（DS8626）**，只有参考手册中文版和勘误 ES0182 Rev14。查 IO 复用表需另补。

## 工程 / 工具链事实（已核实）
- PIO 核心目录在 **`C:\.platformio`**（不在 `~/.platformio`）。CLI 路径 `C:/.platformio/penv/Scripts/pio.exe`，需带 `PLATFORMIO_CORE_DIR=C:\.platformio` 才能跑。
- **PIO `framework-stm32cubef4` 默认 `HSE_VALUE = 25000000U`（25MHz），与 8MHz 晶振不符** → 必须在 `platformio.ini` 里 `build_flags = -DHSE_VALUE=8000000U`，否则 HAL_Delay / 串口波特率全部偏 3.125 倍。
- **PIO 启动文件里 `SysTick_Handler` 是弱符号**（指向 Default_Handler），必须自己实现 `void SysTick_Handler(void){ HAL_IncTick(); }`，否则 `HAL_Delay()` 永久卡死。
- `board = black_f407ve` 默认 `upload_protocol = stlink`，`pio run -t upload` 可直接烧。
- 官方例程 main.c 的 `PLLQ=4`（84MHz）与 `.ioc` 的 `RTL.PLLQ=7`（48MHz）不一致；USB 需 48MHz，取 7。

## 用户基础与偏好（本项目语境）
- 会 C，用过 STM32；寄存器和 RTOS 不熟，要补齐。
- 偏好：纯文字 + markdown 表格；先讲清再考；操作要具体到软件/步骤/预期结果；不替他拍板，给对比选项 + 依据。
