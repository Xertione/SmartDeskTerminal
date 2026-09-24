/**
  ******************************************************************************
  * @file    bsp/touch.c
  * @brief   BSP - XPT2046 电阻触摸驱动实现（软件 SPI）
  ******************************************************************************
  */

#include "bsp/touch.h"

/* Phase 6 起 BSP 只在 RTOS 上下文运行（Touch_Read 由 LVGL read_cb 调用）。
 * 去抖等待用 vTaskDelay 让出 CPU —— 原 HAL_Delay(5) 是忙等，
 * 按住不放时每 30ms 白烧 5ms CPU（约 17%），已列入待修清单，本次顺手修掉。
 * ⚠️ 副作用约束：Touch_Read 不再能在调度器启动前调用（vTaskDelay 会断言）。 */
#include "FreeRTOS.h"
#include "task.h"

/* ---------------------------- 触摸校准值 ---------------------------- */
/* XPT2046 12bit ADC 值 → 屏幕坐标的映射区间。
   **固定值**：运行时不改动，不做任何"自适应校准"（不该要求用户上电点四角）。
   X 来源：**双读法对比实测**确认改用"补 1 个时钟"的读法后，X 实测 200~3800
           （正是 2.8" 电阻屏的典型量程）。
   Y 来源：暂沿用同一典型范围，待用户按遍四角后以实测值替换。
   ⚠️ 本组值绑定"补 1 个时钟"的读法（见 touch_read_raw）。
      历史上曾用不补时钟的读法，那时量程只有一半（190~1845），已废弃。 */
#define XPT_MIN_X   200
#define XPT_MAX_X   3800
#define XPT_MIN_Y   200
#define XPT_MAX_Y   3800

/* 最近一次 ADC 原始值（供校准 / 调试观察，见 touch.h 说明） */
volatile uint16_t touch_raw_x = 0;
volatile uint16_t touch_raw_y = 0;

/* 开机以来的 raw 极值（自动累积），用于一次性测出真实量程 */
volatile uint16_t touch_raw_x_min = 4095;
volatile uint16_t touch_raw_x_max = 0;
volatile uint16_t touch_raw_y_min = 4095;
volatile uint16_t touch_raw_y_max = 0;

/* 清零极值记录（重新测一轮时调用） */
void Touch_ResetRange(void)
{
    touch_raw_x_min = 4095;
    touch_raw_x_max = 0;
    touch_raw_y_min = 4095;
    touch_raw_y_max = 0;
}

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
  * @param  ctrl         控制字节：0xD0 = X 通道, 0x90 = Y 通道
  * @param  skip_clocks  控制字发完后补发的时钟数
  *                      0 = 控制字后直接读（少读一位，数值只有一半）
  *                      1 = 先过一个"转换/BUSY"时钟再读（**正确读法**）
  * @retval 12bit 原始值
  * @note   2026-09-22 双读法对比实测结论：skip=0 时 X 得 100~1872，
  *         skip=1 时 X 得 200~3800（恰为前者的 2 倍，且符合 2.8" 屏典型量程）
  *         → 采用 skip=1。参数保留是为了记录这段结论、便于日后复现对比。
  */
static uint16_t touch_read_raw_ex(uint8_t ctrl, uint8_t skip_clocks)
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

    /* 可选：补发若干个时钟（用于验证是否有多余位） */
    for (uint8_t k = 0; k < skip_clocks; k++)
    {
        t_clk_high();
        touch_delay();
        t_clk_low();
        touch_delay();
    }

    /* 读 12bit 数据（MSB 先出） */
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

/* 工作读法：控制字后先补 1 个"转换/BUSY"时钟，再读 12 位数据。
   —— 2026-09-22 用双读法对比实测确认的结论：
        A（不补时钟）= X 100~1872   ← 数值只有一半，且最小值也偏小
        B（补 1 个时钟）= X 200~3800  ← 正好是 A 的 2 倍
      且 200~3800 正是 2.8" 电阻屏的典型量程 → 确认 B 正确、A 少读了一位。 */
static uint16_t touch_read_raw(uint8_t ctrl)
{
    return touch_read_raw_ex(ctrl, 1);
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
       连续读 2 次去抖：两次差值 > 阈值视为抖动丢弃
       （间隔 5ms 用 vTaskDelay 让出 CPU，见文件头注释） */
    uint16_t x1 = touch_read_raw(0xD0);
    uint16_t y1 = touch_read_raw(0x90);
    vTaskDelay(pdMS_TO_TICKS(5));  /* 5ms 间隔（不忙等） */
    if (!Touch_IsPressed()) return p;  /* 5ms 后松开了，抖动 */

    uint16_t x2 = touch_read_raw(0xD0);
    uint16_t y2 = touch_read_raw(0x90);

    /* 去抖：两次差值 < 200 视为有效 */
    int xdiff = (x1 > x2) ? (x1 - x2) : (x2 - x1);
    int ydiff = (y1 > y2) ? (y1 - y2) : (y2 - y1);
    if (xdiff > 200 || ydiff > 200) return p;  /* 抖动 */

    /* 记录原始值（校准 / 调试观察用），再做映射 */
    uint16_t rawx = (uint16_t)((x1 + x2) / 2);
    uint16_t rawy = (uint16_t)((y1 + y2) / 2);
    touch_raw_x = rawx;
    touch_raw_y = rawy;

    /* 累积极值：在屏上按遍四角即可得到真实量程 */
    if (rawx < touch_raw_x_min) touch_raw_x_min = rawx;
    if (rawx > touch_raw_x_max) touch_raw_x_max = rawx;
    if (rawy < touch_raw_y_min) touch_raw_y_min = rawy;
    if (rawy > touch_raw_y_max) touch_raw_y_max = rawy;

    /* 映射到屏幕坐标：使用文件头的固定校准区间。
       不做运行时"自适应校准" —— 产品不该要求用户每次上电先点四个角；
       区间靠一次性实测确定后写死。 */
    p.x = map_to_screen(rawx, XPT_MIN_X, XPT_MAX_X, 239);
    p.y = map_to_screen(rawy, XPT_MIN_Y, XPT_MAX_Y, 319);
    p.pressed = 1;
    return p;
}
