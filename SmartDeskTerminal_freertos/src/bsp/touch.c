/**
  ******************************************************************************
  * @file    bsp/touch.c
  * @brief   BSP - XPT2046 电阻触摸驱动实现（软件 SPI）
  ******************************************************************************
  */

#include "bsp/touch.h"

/* ---------------------------- 触摸校准值 ---------------------------- */
/* XPT2046 12bit ADC 值 → 屏幕坐标的映射。
   实际值需触摸屏四角校准后确定，这里先给经验值，后续校准再调。
   屏 240×320，XPT2046 典型范围：X 约 300~3900，Y 约 200~3800。 */
#define XPT_MIN_X   300
#define XPT_MAX_X   3900
#define XPT_MIN_Y   200
#define XPT_MAX_Y   3800

/* ---------------------------- 软件延时（SCK 脉冲宽度） ---------------------------- */
/* XPT2046 时钟最高 2.5MHz，软件 GPIO 翻转够慢，不需精确延时。
   168MHz 主频下几次空操作约几十 ns，足够。 */
static inline void touch_delay(void)
{
    for (volatile int i = 0; i < 5; i++);
}

/* ---------------------------- GPIO 位操作 ---------------------------- */
static inline void t_clk_high(void) { T_CLK_PORT->BSRR = T_CLK_PIN; }
static inline void t_clk_low(void)  { T_CLK_PORT->BSRR = T_CLK_PIN << 16; }
static inline void t_cs_high(void)  { T_CS_PORT->BSRR  = T_CS_PIN; }
static inline void t_cs_low(void)   { T_CS_PORT->BSRR  = T_CS_PIN << 16; }
static inline void t_sdi_high(void) { T_SDI_PORT->BSRR = T_SDI_PIN; }
static inline void t_sdi_low(void)  { T_SDI_PORT->BSRR = T_SDI_PIN << 16; }
static inline uint8_t t_sdo_read(void) { return (T_SDO_PORT->IDR & T_SDO_PIN) ? 1 : 0; }

/**
  * @brief  触摸 GPIO 初始化：CLK/CS/SDI 推挽输出，SDO/IRQ 上拉输入
  */
void Touch_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 推挽输出：CLK/CS/SDI */
    GPIO_InitStruct.Pin   = T_CLK_PIN | T_SDI_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = T_CS_PIN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* 上拉输入：SDO（XPT2046 输出）+ IRQ（中断，有触摸拉低） */
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Pin  = T_SDO_PIN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = T_IRQ_PIN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* 初始状态：CS 高（不选中），CLK 低 */
    t_cs_high();
    t_clk_low();
}

/**
  * @brief  读 XPT2046 一个通道的 12bit 原始 ADC 值
  * @param  ctrl  控制字节：0xD0=X通道, 0x90=Y通道
  * @retval 12bit 原始值 (0~4095)
  */
static uint16_t touch_read_raw(uint8_t ctrl)
{
    uint16_t value = 0;

    t_cs_low();
    touch_delay();

    /* 发送 8bit 控制字（MSB 先发） */
    for (int i = 7; i >= 0; i--)
    {
        if (ctrl & (1 << i)) t_sdi_high();
        else                 t_sdi_low();
        touch_delay();
        t_clk_high();
        touch_delay();
        t_clk_low();
    }

    /* 读 12bit 数据（XPT2046 在最后一个时钟后开始输出） */
    touch_delay();  /* 转换所需 1 个额外时钟，已在上面 */
    for (int i = 11; i >= 0; i--)
    {
        t_clk_high();
        touch_delay();
        if (t_sdo_read()) value |= (1 << i);
        t_clk_low();
        touch_delay();
    }

    t_cs_high();
    return value;
}

/**
  * @brief  ADC 原始值 → 屏幕坐标
  */
static uint16_t map_to_screen(uint16_t raw, uint16_t raw_min, uint16_t raw_max, uint16_t screen_max)
{
    if (raw < raw_min) raw = raw_min;
    if (raw > raw_max) raw = raw_max;
    return (uint16_t)((uint32_t)(raw - raw_min) * screen_max / (raw_max - raw_min));
}

/**
  * @brief  仅查 IRQ 引脚，快判断有没有触摸（不读坐标）
  * @retval 1=有触摸（IRQ 低）/ 0=无触摸（IRQ 高）
  */
uint8_t Touch_IsPressed(void)
{
    return (T_IRQ_PORT->IDR & T_IRQ_PIN) ? 0 : 1;
}

/**
  * @brief  读触摸点（含简单去抖 + 坐标映射）
  * @retval TouchPoint：pressed=1 时 x/y 有效，pressed=0 时 x=y=0xFFFF
  */
TouchPoint Touch_Read(void)
{
    TouchPoint p = {0xFFFF, 0xFFFF, 0};

    /* 先查 IRQ，无触摸直接返回（省 SPI 通信） */
    if (!Touch_IsPressed()) return p;

    /* 读 X 和 Y（各读一次，简单实现不做多次平均）
       连续读 2 次去抖：两次差值 > 阈值视为抖动丢弃 */
    uint16_t x1 = touch_read_raw(0xD0);
    uint16_t y1 = touch_read_raw(0x90);
    HAL_Delay(5);  /* 5ms 间隔 */
    if (!Touch_IsPressed()) return p;  /* 5ms 后松开了，抖动 */

    uint16_t x2 = touch_read_raw(0xD0);
    uint16_t y2 = touch_read_raw(0x90);

    /* 去抖：两次差值 < 200 视为有效 */
    int xdiff = (x1 > x2) ? (x1 - x2) : (x2 - x1);
    int ydiff = (y1 > y2) ? (y1 - y2) : (y2 - y1);
    if (xdiff > 200 || ydiff > 200) return p;  /* 抖动 */

    /* 映射到屏幕坐标 */
    p.x = map_to_screen((x1 + x2) / 2, XPT_MIN_X, XPT_MAX_X, 239);
    p.y = map_to_screen((y1 + y2) / 2, XPT_MIN_Y, XPT_MAX_Y, 319);
    p.pressed = 1;
    return p;
}
