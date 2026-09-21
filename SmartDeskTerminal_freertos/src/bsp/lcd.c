/**
  ******************************************************************************
  * @file    bsp/lcd.c
  * @brief   BSP - ST7789V LCD 模块实现（SPI3 接口）
  *
  * 本文件只做「SPI 通信层」：GPIO 配置 + SPI 外设初始化 + 收发命令/数据。
  * 不含 ST7789 初始化序列、不含显示逻辑（后续 STEP2+ 补）。
  *
  * 验证目标（T-008 STEP1）：
  *   编译烧录后，背光常亮 + SWD 监视 SPI3->SR 的 TXE 位 = 1（表示 SPI 发过数据且缓冲区空）
  ******************************************************************************
  */

#include "bsp/lcd.h"

/* SPI 句柄：模块内部 static，外部通过 LCD_Write_Cmd/Data 间接访问 */
static SPI_HandleTypeDef hspi3;

/**
  * @brief  LCD 相关 GPIO 初始化
  * @note   PB3/PB5 配 AF6（SPI3_SCK/MOSI）；PA15/PD13/PC2/PD12 配推挽输出
  */
void LCD_GPIO_Init(void)
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
