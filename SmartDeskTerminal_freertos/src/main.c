/**
  ******************************************************************************
  * @file    main.c
  * @brief   SmartDeskTerminal - Phase 1 板级可靠性验证（不引入 RTOS / LVGL / USB）
  *
  * 目标：PC0 用户 LED（蓝光）以 500ms 亮 / 500ms 灭 闪烁，
  *       用来验证「板子 + 供电 + ST-Link 下载」这条链路正常（无虚焊 / 无物理错误）。
  *
  * 参考：doc/核心板资料/.../【1】参考例程/HAL库/1.LED闪烁（官方例程，时钟参数照抄）
  * 引脚：LED_PC0（见 doc/datasheets_md/06_核心板_原理图与引脚映射.md 第 2 节）
  ******************************************************************************
  */

#include "stm32f4xx_hal.h"

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
    SystemClock_Config();   /* HSE 8MHz -> PLL -> SYSCLK 168MHz */
    LED_GPIO_Init();        /* PC0 配置为推挽输出 */

    while (1)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0);
        HAL_Delay(500);     /* 依赖下面的 SysTick_Handler 调用 HAL_IncTick() */
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
  * @brief  出错死循环（时钟配置失败时停在这里，方便用调试器定位）
  */
static void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
