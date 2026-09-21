/**
  ******************************************************************************
  * @file    bsp/key.h
  * @brief   BSP - 按键模块（KEY_PC1）
  *
  * 硬件：核心板无独立用户按键，KEY 只引到排针 PC1（U6 排针，与 PC0 成对）。
  * 接法：PC1 与 GND 短接即视为「按下」。
  *
  * 极性与官方例程一致（doc/核心板资料/.../【1】参考例程/HAL库/2.按键测试）：
  *   上拉输入 -> 松开时读到高电平，接 GND 时读到低电平
  ******************************************************************************
  */

#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#include "stm32f4xx_hal.h"

/* ---------------------------- 引脚定义（官方例程原值） ---------------------------- */
#define KEY_PIN               GPIO_PIN_1              /* 引脚：PC1 */
#define KEY_PORT              GPIOC                   /* 端口：GPIOC */
#define __HAL_RCC_KEY_CLK_ENABLE()  __HAL_RCC_GPIOC_CLK_ENABLE()

/* ---------------------------- 状态约定 ---------------------------- */
/* Key_Read() 直接返回引脚电平，不做任何取反： */
#define KEY_RELEASED          1U   /* 松开：上拉，读到高电平 */
#define KEY_PRESSED           0U   /* 按下：接 GND，读到低电平 */

/* ---------------------------- 函数声明 ---------------------------- */
void          Key_Init(void);   /* 按键引脚初始化：PC1 上拉输入 */
GPIO_PinState Key_Read(void);   /* 读按键电平：GPIO_PIN_SET=松开 / GPIO_PIN_RESET=按下 */

#endif /* __BSP_KEY_H */
