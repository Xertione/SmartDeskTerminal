目标：

30天完成一个基于 STM32F407 + FreeRTOS + LVGL + USB CDC + Python Agent 的智能桌面协同终端 MVP。

总体开发路线

⚠️ 实际执行顺序已按 ADR-009 / ADR-010 调整，与下方 Phase 编号顺序不同：

    1 → 2 → 5(LCD) → 7(触摸) → 3(FreeRTOS) → 4(任务通信) → 6(LVGL) → 8(USB CDC) → 9(PC Agent) → 10(整合)

其中 Phase 5 / Phase 7 已于 2026-09-22 完成（裸机）。下一个要做的 = **Phase 3**。

本文件只描述各 Phase 的目标与产物；**当前进度以 `progress.md` 为准**。

最终工程：

SmartDeskTerminal

├── STM32 Firmware
│
│   ├── BSP
│   │   ├── LCD
│   │   ├── Touch
│   │   ├── Key
│   │   └── EEPROM
│   │
│   ├── Application
│   │   ├── GUI Task
│   │   ├── Communication Task
│   │   ├── Event Task
│   │   └── Storage Task
│   │
│   ├── Middleware
│   │   ├── FreeRTOS
│   │   ├── LVGL
│   │   └── USB CDC
│
└── PC Agent

    ├── monitor.py
    ├── event_hook.py
    ├── protocol.py
Phase 1：工程初始化与基础运行环境

目标：

STM32工程可以编译、下载、运行。

技术：

PlatformIO
STM32 Framework
HAL
GPIO

完成：

创建项目
配置F407VET6
ST-Link下载
LED闪烁

你理解：

代码
 ↓
编译
 ↓
烧录
 ↓
MCU执行
Phase 2：STM32基础外设层建立

目标：

建立自己的BSP层。

目录：

BSP

lcd
key
uart

完成：

GPIO:

LED
Button

UART:

printf重定向
串口日志

理解：

GPIO寄存器
HAL封装
外设初始化

产物：

[INFO]
System Init OK
UART Ready
Phase 3：引入FreeRTOS

目标：

从裸机进入实时系统。

架构：

main

 |
 |
Scheduler

 |
 +---LED Task

 +---UART Task

 +---System Task

学习：

Task
Priority
Delay
Tick

代码结构：

task_led.c

task_uart.c

task_system.c
Phase 4：FreeRTOS任务通信

目标：

理解RTOS真正价值。

加入：

Queue

例如：

UART收到：

HELLO

发送：

Queue
 ↓
GUI Task

学习：

Queue
Semaphore
Mutex

最终：

Communication Task

        |
        |
      Queue

        |
        ↓

Display Task
Phase 5：LCD驱动移植

目标：

点亮ST7789。

加入：

SPI

STM32

SPI1

ST7789

完成：

显示：

Hello SmartDesk

学习：

SPI协议
DMA概念
LCD初始化流程

目录：

BSP

lcd

 st7789.c

 st7789.h
Phase 6：LVGL图形系统接入

目标：

替代手写LCD绘制。

加入：

LVGL

完成：

页面：

Home

----------------

Time

CPU

Memory

Event

学习：

Widget
Label
Button
Screen

结构：

GUI Task

   |
   |
 LVGL
   |
 LCD Driver
Phase 7：触摸与交互系统

目标：

让设备可以操作。

加入：

XPT2046（电阻触摸，SPI 软件时序）
     ↑ 原规划写的是 FT6336U（电容触摸，I2C），与实际屏（CL28CK230-18A，电阻触摸）
       不符。已按实物更正，见 ADR-009。

完成：

点击：

页面切换
按钮确认

学习：

SPI 主机模式 + 控制字 / 读位时序
     （原写 I2C —— 那是 FT6336U 的接口；XPT2046 走 SPI，已于 2026-09-22 完成驱动）
Touch Driver
LVGL Input Device
Phase 8：USB CDC通信链路

目标：

STM32和PC通信。

架构：

Python

 |
 USB Serial

 |
STM32

STM32：

接收：

JSON

例如：

{
"type":"event",
"msg":"compile success"
}

学习：

USB Device
CDC
Buffer
Protocol
Phase 9：PC Agent开发

目标：

电脑主动发送信息。

Python：

模块：

agent

├── monitor.py

├── serial.py

├── json.py

└── hook.py

功能：

CPU:

psutil

任务：

模拟：

build success

发送：

{
"cpu":35,
"memory":60
}
Phase 10：MVP系统整合

目标：

完成第一个版本。

最终效果：

开机：

SMART DESK


10:30

CPU 35%

MEM 60%


Latest Event

Build Success


PC：

运行：

python agent.py

STM32：

实时更新。