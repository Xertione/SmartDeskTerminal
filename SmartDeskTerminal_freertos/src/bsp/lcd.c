/**
  ******************************************************************************
  * @file    bsp/lcd.c
  * @brief   BSP - ST7789V LCD 模块实现（SPI3 接口）
  *
 * 本文件含：① SPI 通信层（GPIO + SPI3 初始化 + 收发命令/数据）
 *           ② ST7789 初始化序列 LCD_ST7789_Init()（STEP2）
 *           ③ 全屏单色填充 LCD_FillScreen()（STEP2）
 *
 * 验证目标（T-008 STEP2）：
 *   烧录后屏幕红→绿→蓝每秒交替 = 初始化序列正确 + 显示数据通路全通
  ******************************************************************************
  */

#include "bsp/lcd.h"

/* SPI 句柄：模块内部 static，外部通过 LCD_Write_Cmd/Data 间接访问 */
static SPI_HandleTypeDef hspi3;

/**
  * @brief  LCD 相关 GPIO 初始化
  * @note   PB3/PB5 配 AF6（SPI3_SCK/MOSI）；PA15/PD13/PC2/PD12 配推挽输出
  */
static void LCD_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 开各端口时钟（GPIOA/B/C/D） */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* ---- SPI3 信号：PB3(SCK) + PB5(MOSI) → 复用推挽 AF6 ---- */
    GPIO_InitStruct.Pin       = LCD_SPI_SCK_PIN | LCD_SPI_MOSI_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;       /* 复用推挽 */
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;          /* F407 的 SPI3 复用号 = 6 */
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* ---- 控制信号：CS/DC/RST/BL → 普通推挽输出 ---- */
    GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = 0;                      /* 输出模式不用 Alternate */

    /* CS 默认高（不选中屏幕） */
    GPIO_InitStruct.Pin = LCD_CS_PIN;
    HAL_GPIO_Init(LCD_CS_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);

    /* DC 默认低（命令模式） */
    GPIO_InitStruct.Pin = LCD_DC_PIN;
    HAL_GPIO_Init(LCD_DC_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET);

    /* RST 默认高（不复位） */
    GPIO_InitStruct.Pin = LCD_RST_PIN;
    HAL_GPIO_Init(LCD_RST_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET);

    /* BL 拉高（背光常亮） */
    GPIO_InitStruct.Pin = LCD_BL_PIN;
    HAL_GPIO_Init(LCD_BL_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_SET);
}

/**
  * @brief  SPI3 外设初始化
  * @note   SPI3 在 APB1 总线（42MHz）。BaudRate=/16 → 2.625MHz（调试慢速，后续提速）
  *         ST7789 写时序最高 15MHz，2.6MHz 远在安全范围。
  *         用全双工配置但只发不收（MISO 脚悬空不影响）。
  */
void LCD_SPI_Init(void)
{
    __HAL_RCC_SPI3_CLK_ENABLE();

    hspi3.Instance               = SPI3;
    hspi3.Init.Mode               = SPI_MODE_MASTER;             /* 主机 */
    hspi3.Init.Direction          = SPI_DIRECTION_2LINES;        /* 全双工配置（只用发送） */
    hspi3.Init.DataSize           = SPI_DATASIZE_8BIT;            /* 8 位帧 */
    hspi3.Init.CLKPolarity        = SPI_POLARITY_LOW;            /* CPOL=0：空闲低 */
    hspi3.Init.CLKPhase           = SPI_PHASE_1EDGE;              /* CPHA=0：第一边沿采样 */
    hspi3.Init.NSS                = SPI_NSS_SOFT;                 /* 软件控制 CS */
    hspi3.Init.BaudRatePrescaler  = SPI_BAUDRATEPRESCALER_16;     /* 42M/16 = 2.625MHz */
    hspi3.Init.FirstBit           = SPI_FIRSTBIT_MSB;            /* 高位先发 */
    hspi3.Init.TIMode             = SPI_TIMODE_DISABLE;           /* 不用 TI 模式 */
    hspi3.Init.CRCCalculation      = SPI_CRCCALCULATION_DISABLE;   /* 不用 CRC */
    HAL_SPI_Init(&hspi3);
}

/**
  * @brief  LCD 初始化：GPIO + SPI + 硬件复位
  * @note   硬件复位时序：RST 拉低 ≥10ms → 拉高 → 等 ≥120ms（ST7789 规格书要求）
  *         必须在 SystemClock_Config 之后调用（用到 HAL_Delay）
  */
void LCD_Init(void)
{
    LCD_GPIO_Init();
    LCD_SPI_Init();

    /* 硬件复位 */
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);   /* 拉低 10ms */
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(120);  /* 等 120ms 让屏内部稳定 */
}

/**
  * @brief  发命令（DC=0）
  * @note   CS 拉低 → DC=0 → SPI 发 → CS 拉高
  *         CS 的拉低/拉高包裹整个传输，确保 DC 在传输期间稳定
  */
void LCD_Write_Cmd(uint8_t cmd)
{
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);   /* 选中 */
    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET);   /* DC=0 命令 */
    HAL_SPI_Transmit(&hspi3, &cmd, 1, 100);                       /* 发送，超时 100ms */
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);     /* 取消选中 */
}

/**
  * @brief  发数据（DC=1）
  */
void LCD_Write_Data(uint8_t data)
{
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);   /* 选中 */
    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);     /* DC=1 数据 */
    HAL_SPI_Transmit(&hspi3, &data, 1, 100);                      /* 发送 */
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);     /* 取消选中 */
}

/**
  * @brief  发 16 位数据（RGB565）
  * @note   ST7789 的 GRAM 是 16bit/pixel。发数据时高字节先发（MSB First）。
  *         用缓冲区一次发 2 字节，比发两次单字节快。
  */
void LCD_Write_Data16(uint16_t data)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(data >> 8);    /* 高字节 */
    buf[1] = (uint8_t)(data & 0xFF);  /* 低字节 */

    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);     /* DC=1 数据 */
    HAL_SPI_Transmit(&hspi3, buf, 2, 100);
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}

/**
  * @brief  设置显示窗口（后续写 GRAM 只写这个区域）
  * @note   CASET(0x2A) 设列范围，RASET(0x2B) 设行范围。
  *         240×320 屏无需偏移，全屏 = (0,0)~(239,319)。
  */
void LCD_SetAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    /* CASET: 列地址 */
    LCD_Write_Cmd(0x2A);
    LCD_Write_Data16(x0);
    LCD_Write_Data16(x1);

    /* RASET: 行地址 */
    LCD_Write_Cmd(0x2B);
    LCD_Write_Data16(y0);
    LCD_Write_Data16(y1);
}

/**
  * @brief  填充全屏单色
  * @note   设窗口全屏 → RAMWR(0x2C) → 连续写 240×320=76800 个像素。
  *         2.6MHz SPI 下约 0.47s/帧，验证够用（后续可改 DMA 提速）。
  *         RAMWR 后连续写数据即可，不需要每次重发命令。
  */
void LCD_FillScreen(uint16_t color)
{
    LCD_SetAddrWindow(0, 0, 239, 319);  /* 全屏 */
    LCD_Write_Cmd(0x2C);                /* RAMWR: 写 GRAM */

    for (uint32_t i = 0; i < 240UL * 320UL; i++)
    {
        LCD_Write_Data16(color);
    }
}

/**
  * @brief  ST7789 初始化序列（15 步，厂方 TN Code）
  * @note   序列来源：doc/datasheets_md/04_ST7789V_初始化序列与源码索引.md §2
  *         厂方 TN Code 与 LCD.c 逐条一致，参数针对本屏调校。
  *         必须在 LCD_Init()（GPIO+SPI+硬件复位）之后调用。
  */
void LCD_ST7789_Init(void)
{
    /* 1. Sleep Out + 120ms（退出睡眠） */
    LCD_Write_Cmd(0x11);
    HAL_Delay(120);

    /* 2. MADCTL: 正常方向，RGB 顺序（非 BGR） */
    LCD_Write_Cmd(0x36);
    LCD_Write_Data(0x00);

    /* 3. COLMOD: 16bit/pixel RGB565 */
    LCD_Write_Cmd(0x3A);
    LCD_Write_Data(0x05);

    /* 4. PORCTRL: 与手册默认值一致 */
    LCD_Write_Cmd(0xB2);
    LCD_Write_Data(0x0C); LCD_Write_Data(0x0C); LCD_Write_Data(0x00);
    LCD_Write_Data(0x33); LCD_Write_Data(0x33);

    /* 5. GCTRL: VGH=14.06V / VGL=-10.43V */
    LCD_Write_Cmd(0xB7);
    LCD_Write_Data(0x35);

    /* 6. VCOMS: 1.1V */
    LCD_Write_Cmd(0xBB);
    LCD_Write_Data(0x28);

    /* 7. LCMCTRL: 手册默认值 */
    LCD_Write_Cmd(0xC0);
    LCD_Write_Data(0x2C);

    /* 8. VDVVRHEN: VDV/VRH 由命令写入决定 */
    LCD_Write_Cmd(0xC2);
    LCD_Write_Data(0x01);

    /* 9. VRHS */
    LCD_Write_Cmd(0xC3);
    LCD_Write_Data(0x0B);

    /* 10. VDVS: VDV=0V */
    LCD_Write_Cmd(0xC4);
    LCD_Write_Data(0x20);

    /* 11. FRCTRL2: 60Hz 正常帧率 */
    LCD_Write_Cmd(0xC6);
    LCD_Write_Data(0x0F);

    /* 12. PWCTRL1: AVDD=6.8V / AVCL=-4.8V / VDS=2.3V */
    LCD_Write_Cmd(0xD0);
    LCD_Write_Data(0xA4); LCD_Write_Data(0xA1);

    /* 13. PVGAMCTRL: 正电压 gamma（厂方调校 14 参数） */
    LCD_Write_Cmd(0xE0);
    LCD_Write_Data(0xD0); LCD_Write_Data(0x01); LCD_Write_Data(0x08); LCD_Write_Data(0x0F);
    LCD_Write_Data(0x11); LCD_Write_Data(0x2A); LCD_Write_Data(0x36); LCD_Write_Data(0x55);
    LCD_Write_Data(0x44); LCD_Write_Data(0x3A); LCD_Write_Data(0x0B); LCD_Write_Data(0x06);
    LCD_Write_Data(0x11); LCD_Write_Data(0x20);

    /* 14. NVGAMCTRL: 负电压 gamma（厂方调校 14 参数） */
    LCD_Write_Cmd(0xE1);
    LCD_Write_Data(0xD0); LCD_Write_Data(0x02); LCD_Write_Data(0x07); LCD_Write_Data(0x0A);
    LCD_Write_Data(0x0B); LCD_Write_Data(0x18); LCD_Write_Data(0x34); LCD_Write_Data(0x43);
    LCD_Write_Data(0x4A); LCD_Write_Data(0x2B); LCD_Write_Data(0x1B); LCD_Write_Data(0x1C);
    LCD_Write_Data(0x22); LCD_Write_Data(0x1F);

    /* 15. Display On */
    LCD_Write_Cmd(0x29);
    HAL_Delay(10);  /* 等 10ms 让显示稳定 */
}
