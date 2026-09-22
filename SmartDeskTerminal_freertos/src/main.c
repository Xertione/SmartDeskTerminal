/**
  ******************************************************************************
  * @file    main.c
  * @brief   SmartDeskTerminal - Phase 3 FreeRTOS 集成 + Phase 4 任务通信
  *
  * 架构（裸机 → RTOS）：
  *   main: 初始化硬件 + 画静态界面 + 创建队列/任务 + vTaskStartScheduler
  *   Task_LED  (优先级1): PC0 LED 500ms 闪烁（心跳）
  *   Task_Touch(优先级3): 50ms 轮询触摸 + hit-test + 计数 → 发队列消息
  *   Task_LCD  (优先级2): 从队列收消息 → 更新显示（坐标/计数/按钮颜色）
  *
  * SysTick 共存方案（ADR-001 坑3）：
  *   SysTick_Handler 先调 HAL_IncTick（保 HAL 时基），
  *   再调 xPortSysTickHandler（FreeRTOS tick，调度器启动后）
  *
  * 参考：doc/核心板资料/.../【1】参考例程/HAL库/1.LED闪烁（时钟参数照抄）
  * 引脚：LED_PC0（见 doc/datasheets_md/06_核心板_原理图与引脚映射.md 第 2 节）
  ******************************************************************************
  */

#include "stm32f4xx_hal.h"
#include "bsp/key.h"
#include "bsp/lcd.h"
#include "bsp/touch.h"

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* ---------------------------- 固件版本标识 ---------------------------- */
#define FW_VERSION  "v0.8.0-rtos"

/* ---------------------------- 按钮区域定义 ---------------------------- */
#define BTN_X 60
#define BTN_Y 200
#define BTN_W 120
#define BTN_H 40

/* ---------------------------- 任务通信：消息结构 ---------------------------- */
/* Touch Task → Queue → LCD Task */
typedef struct {
    uint16_t x;            /* 触摸 X 坐标 */
    uint16_t y;            /* 触摸 Y 坐标 */
    uint8_t  pressed;      /* 1=按下 / 0=松开 */
    uint8_t  hit_event;    /* 1=本次产生了按钮 hit（边沿触发） */
    uint8_t  hit_count;   /* 累计 hit 次数 */
} TouchMsg;

/* ---------------------------- 全局变量（供 SWD 监视） ---------------------------- */
volatile uint8_t  key_state = KEY_RELEASED;
volatile uint8_t  spi_test_done = 0;
volatile uint16_t touch_x = 0xFFFF;
volatile uint16_t touch_y = 0xFFFF;
volatile uint8_t  touch_pressed = 0;
volatile uint8_t  button_hit_count = 0;

/* FreeRTOS 对象 */
static QueueHandle_t xTouchQueue = NULL;
static TaskHandle_t  hTaskLED   = NULL;
static TaskHandle_t  hTaskTouch  = NULL;
static TaskHandle_t  hTaskLCD   = NULL;

/* ---------------------------- 函数声明 ---------------------------- */
static void SystemClock_Config(void);
static void LED_GPIO_Init(void);
static void Error_Handler(void);
static void int_to_str(uint32_t val, char *buf);
static void draw_button(uint16_t color, const char *label);
static void Task_LED(void *arg);
static void Task_Touch(void *arg);
static void Task_LCD(void *arg);

/* ====================================================================== */
/*                              主程序入口                                 */
/* ====================================================================== */
int main(void)
{
    HAL_Init();
    LED_GPIO_Init();
    SystemClock_Config();

    /* BSP 初始化 */
    Key_Init();
    LCD_Init();
    LCD_ST7789_Init();
    Touch_Init();

    /* 画静态界面（任务启动前一次性画好） */
    LCD_FillScreen(LCD_BLACK);
    LCD_DrawString(8,  10, "SmartDesk " FW_VERSION, LCD_WHITE, LCD_BLACK);
    LCD_DrawString(8,  30, "RTOS: FreeRTOS v10",   LCD_GREEN, LCD_BLACK);
    LCD_DrawString(8,  50, "LCD: ST7789V 240x320",  LCD_GREEN, LCD_BLACK);
    LCD_DrawString(8,  70, "Touch: XPT2046 RTP",    LCD_GREEN, LCD_BLACK);

    /* SYSCLK */
    {
        char line[32];
        int k = 0;
        const char *pfx = "SYSCLK: ";
        const char *sfx = " Hz";
        for (int m = 0; pfx[m]; m++) line[k++] = pfx[m];
        int_to_str(SystemCoreClock, line + k);
        while (line[k]) k++;
        for (int m = 0; sfx[m]; m++) line[k++] = sfx[m];
        line[k] = 0;
        LCD_DrawString(8, 90, line, LCD_CYAN, LCD_BLACK);
    }

    LCD_DrawString(8, 110, "Tasks: LED+Touch+LCD", LCD_MAGENTA, LCD_BLACK);
    LCD_DrawString(8, 150, "Touch:", LCD_YELLOW, LCD_BLACK);
    LCD_DrawString(8, 170, "Hits:",  LCD_YELLOW, LCD_BLACK);
    LCD_DrawString(8, 250, "Heap free:", LCD_GRAY, LCD_BLACK);

    /* 画初始按钮 */
    draw_button(LCD_GREEN, "PRESS ME");

    spi_test_done = 1;

    /* 创建队列：深度 8 条消息 */
    xTouchQueue = xQueueCreate(8, sizeof(TouchMsg));
    if (xTouchQueue == NULL) Error_Handler();

    /* 创建任务 */
    xTaskCreate(Task_LED,   "LED",   configMINIMAL_STACK_SIZE, NULL, 1, &hTaskLED);
    xTaskCreate(Task_Touch,  "Touch", configMINIMAL_STACK_SIZE * 2, NULL, 3, &hTaskTouch);
    xTaskCreate(Task_LCD,    "LCD",   configMINIMAL_STACK_SIZE * 2, NULL, 2, &hTaskLCD);

    /* 启动调度器（FreeRTOS 接管 SysTick） */
    vTaskStartScheduler();

    /* 不会走到这里，除非堆不够创建空闲任务 */
    Error_Handler();
}

/* ====================================================================== */
/*                              任务实现                                    */
/* ====================================================================== */

/**
  * @brief  Task_LED：PC0 LED 心跳闪烁（500ms 翻转）
  * @note   最低优先级，验证调度器在跑
  */
static void Task_LED(void *arg)
{
    (void)arg;
    while (1)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0);
        vTaskDelay(pdMS_TO_TICKS(500));  /* 500ms，不用 HAL_Delay */
    }
}

/**
  * @brief  Task_Touch：轮询触摸 + hit-test + 发队列消息
  * @note   不直接画屏（SPI 访问交给 LCD Task 串行化），只读触摸 + 发消息
  *         锁存式触发：一次按压期间只要有一帧落在按钮区就触发，松开才解锁
  */
static void Task_Touch(void *arg)
{
    (void)arg;
    uint8_t hit_latched = 0;
    TouchMsg msg = {0};

    while (1)
    {
        key_state = (uint8_t)Key_Read();

        TouchPoint tp = Touch_Read();
        touch_x = tp.x;
        touch_y = tp.y;
        touch_pressed = tp.pressed;

        msg.x = tp.x;
        msg.y = tp.y;
        msg.pressed = tp.pressed;
        msg.hit_event = 0;

        /* hit-test（锁存式） */
        if (tp.pressed)
        {
            uint8_t in_btn = (tp.x >= BTN_X && tp.x < BTN_X + BTN_W &&
                              tp.y >= BTN_Y && tp.y < BTN_Y + BTN_H) ? 1 : 0;
            if (!hit_latched && in_btn)
            {
                hit_latched = 1;
                button_hit_count++;
                msg.hit_event = 1;
                msg.hit_count = button_hit_count;
            }
        }
        else
        {
            hit_latched = 0;
        }

        /* 发消息到队列（非阻塞，满了就丢——触摸是高频的，丢几条无所谓） */
        xQueueOverwrite(xTouchQueue, &msg);

        vTaskDelay(pdMS_TO_TICKS(50));  /* 50ms 轮询 */
    }
}

/**
  * @brief  Task_LCD：从队列收触摸消息 → 更新显示
  * @note   所有画屏操作集中在这个任务，避免 SPI 访问冲突
  */
static void Task_LCD(void *arg)
{
    (void)arg;
    TouchMsg msg;
    char numbuf[12];
    uint8_t last_hit_count = 0;
    uint8_t button_state = 0;  /* 0=绿 / 1=红(闪) */
    uint32_t red_expire = 0;

    while (1)
    {
        /* 阻塞等消息（最多 100ms 超时，让闪红计时器能跑） */
        if (xQueueReceive(xTouchQueue, &msg, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            /* 触摸坐标显示 */
            int_to_str(msg.x, numbuf);
            LCD_DrawString(8 + 8*6, 150, "      ", LCD_BLACK, LCD_BLACK);
            if (msg.pressed) LCD_DrawString(8 + 8*6, 150, numbuf, LCD_WHITE, LCD_BLACK);

            int_to_str(msg.y, numbuf);
            LCD_DrawString(8 + 8*6 + 8*7, 150, "      ", LCD_BLACK, LCD_BLACK);
            if (msg.pressed) LCD_DrawString(8 + 8*6 + 8*7, 150, numbuf, LCD_WHITE, LCD_BLACK);

            /* 按钮 hit 事件：闪红 200ms 后恢复绿 */
            if (msg.hit_event)
            {
                draw_button(LCD_RED, "HIT!   ");
                button_state = 1;
                red_expire = xTaskGetTickCount() + pdMS_TO_TICKS(200);
            }

            /* Hits 计数变化时刷新 */
            if (msg.hit_count != last_hit_count)
            {
                last_hit_count = msg.hit_count;
                int_to_str(msg.hit_count, numbuf);
                LCD_DrawString(8 + 8*6, 170, "    ", LCD_BLACK, LCD_BLACK);
                LCD_DrawString(8 + 8*6, 170, numbuf, LCD_YELLOW, LCD_BLACK);
            }
        }

        /* 闪红超时恢复绿色 */
        if (button_state == 1 && xTaskGetTickCount() >= red_expire)
        {
            draw_button(LCD_GREEN, "PRESS ME");
            button_state = 0;
        }

        /* Heap free 显示（每 2 秒刷新一次） */
        {
            static uint32_t last_heap_show = 0;
            uint32_t now = xTaskGetTickCount();
            if (now - last_heap_show > pdMS_TO_TICKS(2000))
            {
                last_heap_show = now;
                size_t free = xPortGetFreeHeapSize();
                int_to_str(free, numbuf);
                LCD_DrawString(8 + 8*11, 250, "    ", LCD_BLACK, LCD_BLACK);
                LCD_DrawString(8 + 8*11, 250, numbuf, LCD_GRAY, LCD_BLACK);
            }
        }
    }
}

/* ====================================================================== */
/*                              辅助函数                                    */
/* ====================================================================== */

/**
  * @brief  画触摸按钮矩形 + 标签
  */
static void draw_button(uint16_t color, const char *label)
{
    uint16_t by;
    for (by = BTN_Y; by < BTN_Y + BTN_H; by++)
    {
        LCD_SetAddrWindow(BTN_X, by, BTN_X + BTN_W - 1, by);
        LCD_Write_Cmd(0x2C);
        for (uint16_t bx = 0; bx < BTN_W; bx++) LCD_Write_Data16(color);
    }
    LCD_DrawString(BTN_X + 16, BTN_Y + 12, label, LCD_WHITE, color);
}

/**
  * @brief  整数转字符串（不用 sprintf 省 Flash）
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
  */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 8;
    RCC_OscInitStruct.PLL.PLLN       = 336;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) Error_Handler();
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
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);
}

/**
  * @brief  SysTick 中断服务函数
  * @note   FreeRTOS 与 HAL 共存：
  *         1. HAL_IncTick() 保 HAL 时基（HAL_GetTick/HAL_Delay 仍可用）
  *         2. xPortSysTickHandler() 只在调度器启动后调用（FreeRTOS tick）
  *         两者共用 SysTick 不冲突：都是 1ms tick（168MHz/168000）
  */
void SysTick_Handler(void)
{
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        xPortSysTickHandler();
    }
}

/**
  * @brief  出错死循环（闪灯报告）
  */
static void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0);
        for (volatile uint32_t i = 0; i < 100000; i++) {}
    }
}

/* FreeRTOS 钩子函数 */
void vApplicationMallocFailedHook(void)
{
    /* 堆分配失败，进 Error_Handler */
    Error_Handler();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    /* 栈溢出，进 Error_Handler */
    Error_Handler();
}
