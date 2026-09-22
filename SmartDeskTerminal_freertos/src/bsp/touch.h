/**
  ******************************************************************************
  * @file    bsp/touch.h
  * @brief   BSP - XPT2046 电阻触摸驱动（SPI 软件控制）
  *
  * 硬件接线（核实自 doc/datasheets_md/05_转接板_28005_SPI模块.md §5）：
  *   排针 10 T_CLK  → PA5（触摸 SPI 时钟，软件控制 GPIO）
  *   排针 11 T_CS   → PB9（触摸片选，低有效）
  *   排针 12 T_SDI  → PA7（触摸数据输入，MCU→XPT2046）
  *   排针 13 T_SDO  → PA6（触摸数据输出，XPT2046→MCU）
  *   排针 14 T_IRQ  → PB8（触摸中断，有触摸时拉低）
  *
  * XPT2046 协议要点：
  *   - 独立 SPI 总线（与 LCD 的 SPI3 硬件 SPI 不同，这里用软件 GPIO 模拟）
  *   - 每次：CS 拉低 → 发 1 字节控制字 → 读 2 字节(12bit ADC 值) → CS 拉高
  *   - 控制字 bit7=START，bit6-4=通道选(XP/YP)，bit3=8bit模式,bit2=0,bit1-0=电源模式
  *   - X 通道=0xD0（读 XP），Y 通道=0x90（读 YP）
  *   - 12bit 值范围 0~4095，需映射到屏幕坐标 0~239(X) / 0~319(Y)
  ******************************************************************************
  */

#ifndef __BSP_TOUCH_H
#define __BSP_TOUCH_H

#include "stm32f4xx_hal.h"

/* ---------------------------- 引脚定义 ---------------------------- */
#define T_CLK_PORT   GPIOA
#define T_CLK_PIN    GPIO_PIN_5      /* PA5 */
#define T_CS_PORT    GPIOB
#define T_CS_PIN     GPIO_PIN_9      /* PB9 */
#define T_SDI_PORT   GPIOA
#define T_SDI_PIN    GPIO_PIN_7      /* PA7 */
#define T_SDO_PORT   GPIOA
#define T_SDO_PIN    GPIO_PIN_6      /* PA6 */
#define T_IRQ_PORT   GPIOB
#define T_IRQ_PIN    GPIO_PIN_8      /* PB8 */

/* ---------------------------- 数据结构 ---------------------------- */
typedef struct {
    uint16_t x;          /* 屏幕坐标 X (0~239)，未按下时为 0xFFFF */
    uint16_t y;          /* 屏幕坐标 Y (0~319)，未按下时为 0xFFFF */
    uint8_t  pressed;    /* 1=按下 / 0=松开 */
} TouchPoint;

/* ---------------------------- 调试 / 校准观察量 ---------------------------- */
/* 最近一次读取的 12bit ADC 原始值（未经 map_to_screen 映射）。
   用途：
   1) 四角校准 —— 记录屏四个角按下时的 raw 值，即可反推真实映射范围；
   2) 排查 —— 判断读数是否恒为 0 / 饱和到 4095（接线或时序问题）。
   注意：仅当 Touch_Read() 判定 pressed=1 时才会刷新。 */
extern volatile uint16_t touch_raw_x;
extern volatile uint16_t touch_raw_y;

/* 开机以来见过的 raw 极值（自动累积，用于一次性测出真实量程）。
   在屏上把四角都按一遍，读 *_min / *_max 即可得到真实映射区间：
     XPT_MIN_X ← touch_raw_x_min + 余量，XPT_MAX_X ← touch_raw_x_max - 余量
   若 max 明显小于 4095（例如只有 1800），说明读数本身没到全量程 = 时序问题，
   此时调校准值是治不了的。 */
extern volatile uint16_t touch_raw_x_min;
extern volatile uint16_t touch_raw_x_max;
extern volatile uint16_t touch_raw_y_min;
extern volatile uint16_t touch_raw_y_max;

/* 清零极值记录（重新测一轮时调用） */
void       Touch_ResetRange(void);

/* ---------------------------- 函数声明 ---------------------------- */
void       Touch_Init(void);                    /* GPIO 初始化 */
TouchPoint Touch_Read(void);                     /* 读触摸点（含去抖，返回屏幕坐标） */
uint8_t    Touch_IsPressed(void);                /* 仅查 T_IRQ 引脚，快判断 */

#endif /* __BSP_TOUCH_H */
