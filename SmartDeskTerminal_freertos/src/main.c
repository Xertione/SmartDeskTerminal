/**
  ******************************************************************************
  * @file    main.c
  * @brief   SmartDeskTerminal - Phase 1 板级可靠性验证（不引入 RTOS / LVGL / USB）
  *
  * 目标：PC0 用户 LED（蓝光）以 500ms 亮 / 500ms 灭 闪烁，
  *       用来验证「板子 + 供电 + ST-Link 下载」这条链路正常（无虚焊 / 无物理错误）。
  *
  * Phase 2（T-006）追加：Key_Init() + key_state 轮询（BSP 模块 1 = PC1 按键输入），
  *       验证按键输入通路；LED 闪烁保留作为运行心跳。
  *
  * Phase 5（T-008 STEP3，提前，ADR-009）追加：LCD_DrawString 字符显示，
  *       验证屏亮文字（Hello SmartDesk + SystemCoreClock + Key 状态）= 字模渲染通路通。
  *
  * 参考：doc/核心板资料/.../【1】参考例程/HAL库/1.LED闪烁（官方例程，时钟参数照抄）
  * 引脚：LED_PC0（见 doc/datasheets_md/06_核心板_原理图与引脚映射.md 第 2 节）
  ******************************************************************************
  */

#include "stm32f4xx_hal.h"
#include "bsp/key.h"
#include "bsp/lcd.h"

/* ---------------------------- 全局变量 ---------------------------- */
/* 按键当前状态：1 = 松开（上拉高电平）/ 0 = 按下（PC1 接 GND）。
   加 volatile 是为了让调试器能读到主循环里的最新值，不被编译器优化掉。 */
volatile uint8_t key_state = KEY_RELEASED;

/* Phase 5：LCD_Write_Cmd 执行后置 1，供 SWD 监视验证 SPI 通信跑过。
   作用：断点停在 while 循环时，读这个变量=1 表示 SPI3 发送链路执行了。 */
volatile uint8_t spi_test_done = 0;

/* ---------------------------- 函数声明 ---------------------------- */
static void SystemClock_Config(void);
static void LED_GPIO_Init(void);
static void Error_Handler(void);

/**
  * @brief  主程序入口
  */
int main(void)
{
    HAL_Init();             /* 初始化 HAL：Flash 预取、NVIC 优先级分组、SysTick 时基 */

    /* 先初始化 LED（诊断用）：此时仍跑在 HSI 16MHz 默认时钟上，不依赖 PLL。
       这样即使后面 SystemClock_Config() 里 HSE 起振失败进了 Error_Handler，
       PC0 也已经被配置成输出，Error_Handler 里的快闪才能被肉眼看到。 */
    LED_GPIO_Init();        /* PC0 配置为推挽输出 */

    SystemClock_Config();   /* HSE 8MHz -> PLL -> SYSCLK 168MHz */

    Key_Init();             /* BSP 模块 1：PC1 上拉输入 */
    LCD_Init();             /* BSP 模块 2：屏幕 GPIO + SPI3 + 硬件复位（背光常亮） */
    LCD_ST7789_Init();      /* ST7789 初始化序列（15步，厂方 TN Code） */

    /* STEP3：字符显示验证
       清屏黑色 → 画标题（白字）+ 画主频信息（绿字）+ 画按键状态行（黄字） */
    LCD_FillScreen(LCD_BLACK);
    LCD_DrawString(8,  10, "Hello SmartDesk",  LCD_WHITE, LCD_BLACK);
    LCD_DrawString(8,  40, "LCD: ST7789V 240x320", LCD_GREEN, LCD_BLACK);
    LCD_DrawString(8,  60, "SPI3 @ 2.6MHz",    LCD_GREEN, LCD_BLACK);

    /* 画 SystemCoreClock 数值（把数字转成字符串） */
    {
        char buf[24];
        uint32_t clk = SystemCoreClock;
        /* 简单整数转字符串（不用 sprintf，省库） */
        int i = 0;
        if (clk == 0) { buf[i++] = '0'; }
        else {
            char tmp[12];
            int t = 0;
            while (clk > 0) { tmp[t++] = '0' + (clk % 10); clk /= 10; }
            while (t > 0) { buf[i++] = tmp[--t]; }
        }
        buf[i] = 0;
        LCD_DrawString(8,  80, "SYSCLK:", LCD_CYAN, LCD_BLACK);
        LCD_DrawString(8 + 8*7, 80, buf, LCD_CYAN, LCD_BLACK);
        LCD_DrawString(8 + 8*7, 80, " Hz", LCD_CYAN, LCD_BLACK);
    }

    spi_test_done = 1;      /* 标记初始化 + 字符显示完成 */

    while (1)
    {
        key_state = (uint8_t)Key_Read();   /* 1 = 松开 / 0 = 按下 */

        /* 在第 5 行实时刷新按键状态。
           先用背景色把旧字擦掉（覆盖写一遍黑底黑字），再写新状态。 */
        if (key_state)
        {
            LCD_DrawString(8, 100, "Key: RELEASED", LCD_GREEN, LCD_BLACK);
        }
        else
        {
            LCD_DrawString(8, 100, "Key: PRESSED ", LCD_YELLOW, LCD_BLACK);
        }

        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0);
        HAL_Delay(200);     /* 200ms 刷新一次按键显示 */
    }
}

/**
  * @brief  系统时钟配置：HSE 8MHz -> PLL -> 168MHz
  * @note   参数与官方例程一致：PLLM=8 / PLLN=336 / PLLP=DIV2 / PLLQ=7
  *         8MHz / 8 * 336 / 2 = 168MHz；VCO 输入 1MHz（手册要求 1~2MHz）
  */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* 使能电源时钟并设置调压器为 Scale1（168MHz 必须） */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* 振荡器配置 */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 8;
    RCC_OscInitStruct.PLL.PLLN       = 336;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /* 总线时钟配置：SYSCLK=PLLCLK，AHB=DIV1(168M)，APB1=DIV4(42M)，APB2=DIV2(84M) */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    /* 168MHz 下 Flash 等待周期必须为 5 */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief  LED 引脚初始化：PC0 推挽输出
  */
static void LED_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitStruct.Pin   = GPIO_PIN_0;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* 起始电平：亮 / 灭取决于 LED 是低电平点亮还是高电平点亮（本板极性暂未核实） */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);
}

/**
  * @brief  SysTick 中断服务函数
  * @note   必须自己实现！启动文件里 SysTick_Handler 是 weak 弱符号，
  *         默认指向 Default_Handler（死循环），不实现的话 HAL_Delay() 会永久卡死。
  */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/**
  * @brief  出错死循环（改成闪灯，用灯的节奏报告"我卡在这里了"）
  * @note   这里用「软件空循环」延时，不能用 HAL_Delay()：
  *         HAL_Delay 依赖 SysTick 中断累加计数，而本函数先 __disable_irq() 关了中断，
  *         用 HAL_Delay 会直接死等。
  */
static void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0);
        for (volatile uint32_t i = 0; i < 100000; i++)
        {
        }
    }
}
