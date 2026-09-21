/**
  ******************************************************************************
  * @file    bsp/key.c
  * @brief   BSP - 按键模块实现（KEY_PC1）
  *
  * 参考：doc/核心板资料/.../【1】参考例程/HAL库/2.按键测试/Drivers/User/Src/key.c
  * 说明：本模块只做「电平读取」，不含消抖 / 不含按下检测状态机（后续单独实现）。
  ******************************************************************************
  */

#include "bsp/key.h"

/**
  * @brief  按键引脚初始化：PC1 上拉输入
  * @note   上拉保证松开时引脚不会被悬空成不定电平
  */
void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_KEY_CLK_ENABLE();          /* 开 GPIOC 时钟，否则寄存器写入无效 */

    GPIO_InitStruct.Pin  = KEY_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;   /* 输入模式 */
    GPIO_InitStruct.Pull = GPIO_PULLUP;       /* 内部上拉 */

    HAL_GPIO_Init(KEY_PORT, &GPIO_InitStruct);
}

/**
  * @brief  读按键电平
  * @retval GPIO_PIN_SET(1)   = 松开（上拉高电平）
  *         GPIO_PIN_RESET(0) = 按下（接 GND 低电平）
  */
GPIO_PinState Key_Read(void)
{
    return HAL_GPIO_ReadPin(KEY_PORT, KEY_PIN);
}
