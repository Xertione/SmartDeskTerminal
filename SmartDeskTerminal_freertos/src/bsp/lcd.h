/**
  ******************************************************************************
  * @file    bsp/lcd.h
  * @brief   BSP - ST7789V LCD 模块（SPI3 接口，4 线写）
  *
  * 硬件链路（核实自 doc/datasheets_md/05_转接板_28005_SPI模块.md §5 + 06 §3）：
  *   屏幕 CL28CK230-18A（18-pin FPC，ST7789V，240×320，2.8"，电阻触摸 XPT2046）
  *     → 28005 转接板（14-pin 排针）→ 母对母杜邦线 → 核心板 GPIO
  *
  * 接线表（本模块只管显示，触摸另建模块）：
  *   排针 1 VCC  → 核心板 3V3（⚠️ 不可接 5V0，面板 VDD 绝对最大 4.2V）
  *   排针 2 GND  → 核心板 GND
  *   排针 3 CS   → PA15（软件控制，不用硬件 NSS——ST7789 的 DC 要在 CS 拉低期间设好）
  *   排针 4 RST  → PC2（datasheets 说"需自指定 IO"，本模块取 PC2，可改宏）
  *   排针 5 DC   → PD13（0=命令 / 1=数据）
  *   排针 6 MOSI → PB5（SPI3_MOSI，AF6）
  *   排针 7 SCK  → PB3（SPI3_SCK，AF6）
  *   排针 8 LED  → PD12（高点亮背光，已验证点亮）
  *   排针 9 MISO → 不接（屏只写不读）
  *
  * ⚠️ PB3/PB5 默认是 JTAG 调试脚（SWO/JTDI）。本工程用 SWD 调试（SWCLK=PA14/SWDIO=PA13），
  *    不用 JTAG，把 PB3/PB5 配成 AF6=SPI3 不影响 SWD 调试。
  ******************************************************************************
  */

#ifndef __BSP_LCD_H
#define __BSP_LCD_H

#include "stm32f4xx_hal.h"

/* ---------------------------- 引脚定义 ---------------------------- */
/* SPI3 信号（配成复用功能 AF6） */
#define LCD_SPI            SPI3
#define LCD_SPI_SCK_PORT   GPIOB
#define LCD_SPI_SCK_PIN    GPIO_PIN_3      /* PB3 = SPI3_SCK */
#define LCD_SPI_MOSI_PORT  GPIOB
#define LCD_SPI_MOSI_PIN   GPIO_PIN_5      /* PB5 = SPI3_MOSI */

/* 控制信号（配成推挽输出，软件控制） */
#define LCD_CS_PORT        GPIOA
#define LCD_CS_PIN         GPIO_PIN_15     /* PA15 = 片选，低有效 */
#define LCD_DC_PORT        GPIOD
#define LCD_DC_PIN         GPIO_PIN_13     /* PD13 = 数据/命令，0=命令/1=数据 */
#define LCD_RST_PORT       GPIOC
#define LCD_RST_PIN        GPIO_PIN_2      /* PC2 = 复位，低有效（可改） */
#define LCD_BL_PORT        GPIOD
#define LCD_BL_PIN         GPIO_PIN_12    /* PD12 = 背光，高点亮 */

/* ---------------------------- 函数声明 ---------------------------- */
void LCD_Init(void);                 /* GPIO + SPI + 硬件复位（背光常亮） */
void LCD_Write_Cmd(uint8_t cmd);     /* 发命令：DC=0 */
void LCD_Write_Data(uint8_t data);   /* 发数据：DC=1 */

#endif /* __BSP_LCD_H */
