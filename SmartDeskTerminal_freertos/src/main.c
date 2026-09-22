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
  * Phase 7（T-009，提前，ADR-009）追加：Touch 驱动 + 触摸按钮交互验证，
  *       屏画按钮，触摸按钮变色 + 计数 + 显示触摸坐标。
  *
  * 参考：doc/核心板资料/.../【1】参考例程/HAL库/1.LED闪烁（官方例程，时钟参数照抄）
  * 引脚：LED_PC0（见 doc/datasheets_md/06_核心板_原理图与引脚映射.md 第 2 节）
  ******************************************************************************
  */

#include "stm32f4xx_hal.h"
#include "bsp/key.h"
#include "bsp/lcd.h"
#include "bsp/touch.h"

/* ---------------------------- 固件版本标识 ---------------------------- */
/* FW_VERSION：手工维护，有实质功能改动就递增（人可能忘，所以它只是辅助）。
   __DATE__ / __TIME__：编译器自动填充，**每次重新编译必然变化** ——
   这才是判断"手上跑的是不是最新固件"的可靠依据（防烧错/烧旧）。
   两者都会显示在屏幕顶部，烧录后一眼可核对。 */
#define FW_VERSION  "v0.7.1"

/* ---------------------------- 全局变量 ---------------------------- */
/* 按键当前状态：1 = 松开（上拉高电平）/ 0 = 按下（PC1 接 GND）。
   加 volatile 是为了让调试器能读到主循环里的最新值，不被编译器优化掉。 */
volatile uint8_t key_state = KEY_RELEASED;

/* Phase 5：LCD_Write_Cmd 执行后置 1，供 SWD 监视验证 SPI 通信跑过。
   作用：断点停在 while 循环时，读这个变量=1 表示 SPI3 发送链路执行了。 */
volatile uint8_t spi_test_done = 0;

/* Phase 7：触摸状态（供 SWD 监视） */
volatile uint16_t touch_x = 0xFFFF;
volatile uint16_t touch_y = 0xFFFF;
volatile uint8_t  touch_pressed = 0;
volatile uint8_t  button_hit_count = 0;  /* 按钮被点中次数 */

/* 按钮区域定义（屏幕坐标） */
#define BTN_X 60
#define BTN_Y 200
#define BTN_W 120
#define BTN_H 40

/* ---------------------------- 函数声明 ---------------------------- */
static void SystemClock_Config(void);
static void LED_GPIO_Init(void);
static void Error_Handler(void);
static void int_to_str(uint32_t val, char *buf);  /* 整数转字符串 */

/**
  * @brief  主程序入口
  */
int main(void)
{
    HAL_Init();
    LED_GPIO_Init();
    SystemClock_Config();

    Key_Init();
    LCD_Init();
    LCD_ST7789_Init();
    Touch_Init();

    /* 界面：黑底标题 + 版本标识 + 信息行 */
    LCD_FillScreen(LCD_BLACK);
    LCD_DrawString(8,  10, "SmartDesk " FW_VERSION, LCD_WHITE, LCD_BLACK);
    /* 编译时间：编译器自动填充，每次重编译必然变化 ——
       烧录后先看这一行，就能确认手上跑的是不是最新构建 */
    LCD_DrawString(8,  30, "Build " __DATE__ " " __TIME__, LCD_GRAY, LCD_BLACK);
    LCD_DrawString(8,  50, "LCD: ST7789V 240x320",  LCD_GREEN,  LCD_BLACK);
    LCD_DrawString(8,  70, "Touch: XPT2046 RTP",    LCD_GREEN,  LCD_BLACK);

    /* SYSCLK 数值：先拼成一整行再一次性画。
       ⚠️ 别拆成多段续画 —— 每段的 x 必须按"已画字符数×8"递增，算错就会互相覆盖
       （旧版把 " Hz" 的 x 写成与数字相同的 64，屏上显示成 "SYSCLK: Hz000000"）。 */
    {
        char line[32];
        int k = 0;
        const char *pfx = "SYSCLK: ";
        const char *sfx = " Hz";

        for (int m = 0; pfx[m]; m++) line[k++] = pfx[m];   /* "SYSCLK: " */
        int_to_str(SystemCoreClock, line + k);             /* 追数字      */
        while (line[k]) k++;                               /* 移到数字末尾 */
        for (int m = 0; sfx[m]; m++) line[k++] = sfx[m];   /* " Hz"       */
        line[k] = 0;

        LCD_DrawString(8, 90, line, LCD_CYAN, LCD_BLACK);
    }

    /* 画触摸按钮（绿底白字）—— 画矩形 + 写字 */
    {
        uint16_t by;
        for (by = BTN_Y; by < BTN_Y + BTN_H; by++)
        {
            LCD_SetAddrWindow(BTN_X, by, BTN_X + BTN_W - 1, by);
            LCD_Write_Cmd(0x2C);
            for (uint16_t bx = 0; bx < BTN_W; bx++) LCD_Write_Data16(LCD_GREEN);
        }
        LCD_DrawString(BTN_X + 16, BTN_Y + 12, "PRESS ME", LCD_WHITE, LCD_GREEN);
    }

    LCD_DrawString(8, 150, "Touch:", LCD_YELLOW, LCD_BLACK);
    LCD_DrawString(8, 170, "Hits:",  LCD_YELLOW, LCD_BLACK);

    spi_test_done = 1;
    uint8_t hit_latched = 0;   /* 锁存标志：一次按压只触发一次按钮 */
    char numbuf[12];

    while (1)
    {
        key_state = (uint8_t)Key_Read();

        /* 读触摸 */
        TouchPoint tp = Touch_Read();
        touch_x = tp.x;
        touch_y = tp.y;
        touch_pressed = tp.pressed;

        /* 触摸坐标显示 */
        int_to_str(touch_x, numbuf);
        LCD_DrawString(8 + 8*6, 150, "      ", LCD_BLACK, LCD_BLACK);
        if (tp.pressed) LCD_DrawString(8 + 8*6, 150, numbuf, LCD_WHITE, LCD_BLACK);

        int_to_str(touch_y, numbuf);
        LCD_DrawString(8 + 8*6 + 8*7, 150, "      ", LCD_BLACK, LCD_BLACK);
        if (tp.pressed) LCD_DrawString(8 + 8*6 + 8*7, 150, numbuf, LCD_WHITE, LCD_BLACK);

        /* 当前触点是否落在按钮矩形内（供 hit-test 与诊断行共用） */
        uint8_t in_btn = (tp.pressed &&
                          tp.x >= BTN_X && tp.x < BTN_X + BTN_W &&
                          tp.y >= BTN_Y && tp.y < BTN_Y + BTN_H) ? 1u : 0u;

        /* 按钮触发：锁存式（一次按压只算一次）
           ⚠️ 不要用"按下瞬间那一帧"判定 —— 按下第一帧的坐标最容易失准
           （XPT2046 首次转换未稳 + 手指刚接触时受压面积还在变）。一旦这一帧
           偏出按钮区，边沿检测就把唯一的机会用掉了：之后手指稳稳按在按钮上、
           坐标也回到区内，却永远不会再触发，表现就是"按了没反应"。
           锁存式：一次按压期间只要有一帧落在按钮区内就触发，松开才解锁。 */
        if (tp.pressed)
        {
            if (!hit_latched && in_btn)
            {
                hit_latched = 1;   /* 锁定：按住期间不重复计数 */
                button_hit_count++;

                /* 闪红反馈 */
                uint16_t by;
                for (by = BTN_Y; by < BTN_Y + BTN_H; by++)
                {
                    LCD_SetAddrWindow(BTN_X, by, BTN_X + BTN_W - 1, by);
                    LCD_Write_Cmd(0x2C);
                    for (uint16_t bx = 0; bx < BTN_W; bx++) LCD_Write_Data16(LCD_RED);
                }
                LCD_DrawString(BTN_X + 16, BTN_Y + 12, "HIT!   ", LCD_WHITE, LCD_RED);
                HAL_Delay(200);
                /* 恢复绿色 */
                for (by = BTN_Y; by < BTN_Y + BTN_H; by++)
                {
                    LCD_SetAddrWindow(BTN_X, by, BTN_X + BTN_W - 1, by);
                    LCD_Write_Cmd(0x2C);
                    for (uint16_t bx = 0; bx < BTN_W; bx++) LCD_Write_Data16(LCD_GREEN);
                }
                LCD_DrawString(BTN_X + 16, BTN_Y + 12, "PRESS ME", LCD_WHITE, LCD_GREEN);
            }
        }
        else
        {
            hit_latched = 0;       /* 松开解锁，允许下一次触发 */
        }

        /* Hits 计数 */
        int_to_str(button_hit_count, numbuf);
        LCD_DrawString(8 + 8*6, 170, "    ", LCD_BLACK, LCD_BLACK);
        LCD_DrawString(8 + 8*6, 170, numbuf, LCD_YELLOW, LCD_BLACK);

        /* 触摸调试诊断显示（y=260 的 raw 极值行、y=280 的 P/IN-OUT 行）
           已于 2026-09-22 排查结束后移除，屏面恢复干净。
           若日后需要重新校准或排查，可临时恢复显示，数据源仍在
           touch.c 的 touch_raw_x/y_min/max（说明见 touch.h）。 */

        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0);
        HAL_Delay(50);
    }
}

/**
  * @brief  整数转字符串（不用 sprintf 省 Flash）
  * @retval 写入 buf，返回长度（buf 里是 \0 结尾字符串）
  */
static void int_to_str(uint32_t val, char *buf)
{
    if (val == 0) { buf[0] = '0'; buf[1] = 0; return; }
    char tmp[12];
    int t = 0;
    while (val > 0) { tmp[t++] = '0' + (val % 10); val /= 10; }
    int i = 0;
    while (t > 0) { buf[i++] = tmp[--t]; }
    buf[i] = 0;
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
