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

/* 任务心跳计数器（排查用）：哪个数字在涨 = 哪个任务在跑。
   ⚠️ 只有 Task_LCD 会显示它们 —— 所以**两个都不出现**本身就说明 Task_LCD 没运行。 */
volatile uint32_t hb_touch = 0;
volatile uint32_t hb_lcd   = 0;

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
static void u32_to_hex(uint32_t v, char *buf);
static uint8_t ram_ok(uint32_t a);
static void draw_button(uint16_t color, const char *label);
static void Task_LED(void *arg);
static void Task_Touch(void *arg);
static void Task_LCD(void *arg);

/* FreeRTOS 当前任务控制块（定义在 tasks.c）。
   TCB 的**第一个成员是 pxTopOfStack**，启动第一个任务时 port.c 的
   vPortSVCHandler 就是从它指向的位置按固定布局取寄存器的 —— 见下方"打点 B"。 */
extern void *volatile pxCurrentTCB;

/* FreeRTOS 的 SysTick 处理函数（FreeRTOS tick 中断）。
   它定义在 portable/GCC/ARM_CM4F/port.c，但 portmacro.h / task.h **都没有声明它**，
   直接调用会触发 "implicit declaration" 警告 —— 而隐式声明在 C 里是未定义行为
   （编译器默认按返回 int 处理）。这里显式声明，消除 UB。
   ⚠️ ARM_CM3 等不含 FPU 保存的 port 同样有这个名字，可通用。 */
extern void xPortSysTickHandler(void);

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
    /* 编译时间：编译器自动填充，每次重编译必变 —— 烧录后先看这行，
       确认手上跑的是不是刚编出来的固件（版本号是人手维护的，会忘） */
    LCD_DrawString(8, 130, "Build " __DATE__ " " __TIME__, LCD_GRAY, LCD_BLACK);
    LCD_DrawString(8, 150, "Touch:", LCD_YELLOW, LCD_BLACK);
    LCD_DrawString(8, 170, "Hits:",  LCD_YELLOW, LCD_BLACK);
    LCD_DrawString(8, 250, "Heap free:", LCD_GRAY, LCD_BLACK);

    /* 画初始按钮 */
    draw_button(LCD_GREEN, "PRESS ME");

    spi_test_done = 1;

    /* 创建队列：**深度 1**，配合 xQueueOverwrite 使用。
       ⚠️ xQueueOverwrite 按 FreeRTOS 官方语义【只能用于长度为 1 的队列】——
       它的设计意图是"始终只保留最新一条"，深度 >1 时行为不符合文档约定。
       触摸消息正是"只关心最新状态"的场景，深度 1 天然契合。 */
    xTouchQueue = xQueueCreate(1, sizeof(TouchMsg));
    if (xTouchQueue == NULL) Error_Handler();

    /* 创建任务。
       ⚠️ 必须检查返回值：堆不足时 xTaskCreate 返回
       errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY，**任务根本不会被创建**，
       但主流程会继续走下去 —— 现象就是"屏不刷新 / 触摸毫无反应"，
       而且极难排查（看起来像是驱动坏了）。 */
    if (xTaskCreate(Task_LED,   "LED",   configMINIMAL_STACK_SIZE,     NULL, 1, &hTaskLED)   != pdPASS) Error_Handler();
    if (xTaskCreate(Task_Touch, "Touch", configMINIMAL_STACK_SIZE * 2, NULL, 3, &hTaskTouch) != pdPASS) Error_Handler();
    if (xTaskCreate(Task_LCD,   "LCD",   configMINIMAL_STACK_SIZE * 2, NULL, 2, &hTaskLCD)   != pdPASS) Error_Handler();

    /* ── 打点 A：调度器启动**之前** ──
       用途：区分"卡在调度器内部"和"调度器返回了"两种情况。
       若屏上一直显示 pre-Sched → vTaskStartScheduler() 没返回过
         （正常情况它就该永不返回，所以这说明**任务启动那一步卡住了**，
          最常见是 vPortStartFirstTask 的 `svc 0` 落到了启动文件的
          Default_Handler 死循环 —— 那种卡法不会进 Error_Handler，
          所以屏上既没红屏、也没任务输出，完全静默）。
       若屏上变成 post-Sched! → 它**返回了**，说明连空闲/定时器任务都没建起来。 */
    LCD_DrawString(8, 270, "pre-Sched ", LCD_CYAN, LCD_BLACK);

    /* ── 打点 B：**调度器启动前**，检查"首任务栈帧"是否已经建好 ──────────
     *
     * 这是本次 HardFault 排查的关键判据，原理如下：
     *
     * 启动第一个任务时，port.c 的 vPortSVCHandler 做的事是（反汇编已核对）：
     *     ldr  r0, [pxCurrentTCB, #0]      ; r0 = TCB->pxTopOfStack
     *     ldmia r0!, {r4-r11, lr}          ; 从该地址连取 9 个字，第 9 个装进 LR
     *     msr  PSP, r0                     ; PSP = 该地址 + 36
     *     bx   lr                          ; ← 用 LR 做**异常返回**
     *
     * 而 pxPortInitialiseStack() 建帧时的布局是固定的：
     *     [0..7] = R4~R11                （内容无关，会被覆盖）
     *     [8]    = EXC_RETURN  ← **必须是 0xFFFFFFFD**
     *     [9..16] = R0,R1,R2,R3,R12,LR,PC,xPSR
     *
     * `bx lr` 时如果 LR 不是合法 EXC_RETURN（高位不是全 1 的固定格式），
     * CPU 直接报 INVPC（CFSR bit18 = "attempt to load PC with a bad EXC_RETURN"）
     * 并升级为 HardFault —— 这正好就是屏上抓到的 CFSR=0x00040000。
     *
     * 所以：**在进调度器之前先把这个字读出来看一眼**。
     *   显示 OK  → 帧是好的，故障是"进入调度器之后被谁破坏的"
     *   显示 BAD → 帧在建的时候就没对，问题在 xTaskCreate/堆/对齐
     * 两种情况指向完全不同的方向，一行字就能分开。
     * ------------------------------------------------------------------ */
    {
        uint32_t tos = 0, exc = 0;
        uint8_t  ok  = 0;

        if (pxCurrentTCB != NULL)
        {
            tos = ((uint32_t *)pxCurrentTCB)[0];       /* TCB 首成员 = pxTopOfStack */
            if (ram_ok(tos) && ram_ok(tos + 8 * 4))
            {
                exc = ((uint32_t *)tos)[8];            /* 第 9 个字 = EXC_RETURN */
                ok  = (exc == 0xFFFFFFFDu) ? 1 : 0;
            }
        }

        char l[36];
        int  k = 0;
        const char *p = "TOS:";
        for (int m = 0; p[m]; m++) l[k++] = p[m];
        u32_to_hex(tos, l + k); k += 8;
        l[k++] = ' ';
        p = "E:";
        for (int m = 0; p[m]; m++) l[k++] = p[m];
        u32_to_hex(exc, l + k); k += 8;
        l[k++] = ' ';
        p = ok ? "OK" : "BAD";
        for (int m = 0; p[m]; m++) l[k++] = p[m];
        l[k] = 0;

        LCD_DrawString(8, 290, l, ok ? LCD_GREEN : LCD_RED, LCD_BLACK);

        if (!ok)
        {
            /* 帧在进调度器之前就坏了 → 不进调度器，保留证据（进去必 Fault） */
            LCD_FillScreen(LCD_RED);
            LCD_DrawString(8, 10, "FRAME BAD (pre-sched)", LCD_WHITE, LCD_RED);
            LCD_DrawString(8, 30, "TOS:", LCD_WHITE, LCD_RED);
            u32_to_hex(tos, l); l[8] = 0;
            LCD_DrawString(8 + 8 * 4, 30, l, LCD_WHITE, LCD_RED);
            LCD_DrawString(8, 50, "EXC[8]:", LCD_WHITE, LCD_RED);
            u32_to_hex(exc, l); l[8] = 0;
            LCD_DrawString(8 + 8 * 7, 50, l, LCD_WHITE, LCD_RED);
            LCD_DrawString(8, 70, "expect: FFFFFFFD", LCD_WHITE, LCD_RED);
            while (1) { __asm volatile ("nop"); }
        }
    }

    /* 启动调度器（FreeRTOS 接管 SysTick）。正常情况下此函数永不返回。 */
    vTaskStartScheduler();

    /* ⚠️ 能走到这里 = vTaskStartScheduler() 返回了（异常路径） */
    LCD_DrawString(8, 270, "post-Sched!", LCD_RED, LCD_BLACK);
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
        hb_touch++;                    /* 心跳：证明本任务在跑 */
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
        hb_lcd++;                      /* 心跳：证明本任务在跑 */

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

        /* Heap free + 任务心跳显示（每 2 秒刷新一次） */
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

                /* 任务心跳（y=290）：T=Task_Touch 计数，L=Task_LCD 计数。
                   两个都在涨 = 调度正常；T 涨 L 不涨 = LCD 任务卡住；
                   两个都不出现 = Task_LCD 根本没运行（结合 Error_Handler 红屏判断）。 */
                char hb[40];
                int k = 0;
                const char *p1 = "HB T:";
                const char *p2 = " L:";
                for (int m = 0; p1[m]; m++) hb[k++] = p1[m];
                int_to_str(hb_touch, hb + k); while (hb[k]) k++;
                for (int m = 0; p2[m]; m++) hb[k++] = p2[m];
                int_to_str(hb_lcd, hb + k); while (hb[k]) k++;
                hb[k] = 0;

                LCD_DrawString(8, 290, "                      ", LCD_BLACK, LCD_BLACK);
                LCD_DrawString(8, 290, hb, LCD_GRAY, LCD_BLACK);
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
  * @brief  出错死循环
  * @note   ⚠️⚠️ 本板 PC0 用户 LED 是**已知坏件**（见 troubleshooting T-005）——
  *         原来的"闪灯报错"在本板等于**没有任何提示**，出了错完全看不出来。
  *         这里改用**屏幕**作为错误输出（屏幕已实证可靠）。
  *         只有 LCD 初始化完成（spi_test_done=1）才能画屏，否则 hspi3 句柄
  *         还是未初始化状态，调 HAL_SPI_Transmit 会解引用空指针 → HardFault。
  */
static void Error_Handler(void)
{
    __disable_irq();                      /* 注意：中断已关，不能再用 HAL_Delay/vTaskDelay */

    uint8_t lcd_ok = spi_test_done;       /* LCD 是否已就绪 */

    while (1)
    {
        if (lcd_ok)
        {
            /* 红底白字 "!! ERROR !" —— 一眼可见的"我卡住了" */
            LCD_FillScreen(LCD_RED);
            LCD_DrawString(8,  10, "!! ERROR !",  LCD_WHITE, LCD_RED);
            LCD_DrawString(8,  30, "check SWD",   LCD_WHITE, LCD_RED);
            /* 保持约 0.4s（空循环延时；中断已关，HAL_Delay 会死等） */
            for (volatile uint32_t i = 0; i < 20000000; i++) {}
            /* 黑屏约 0.7s（FillScreen 本身耗时），形成闪烁节奏 */
            LCD_FillScreen(LCD_BLACK);
        }
        else
        {
            /* LCD 还没就绪（例如时钟配置阶段就失败）：只能退回灯闪 */
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0);
            for (volatile uint32_t i = 0; i < 100000; i++) {}
        }
    }
}

/* FreeRTOS 钩子函数 */

/**
  * @brief  configASSERT 失败时的出口（由 FreeRTOSConfig.h 的宏调用）
  * @note   默认的 configASSERT 是个**空宏** —— 断言失败时什么都不做，
  *         于是"优先级分组配错 / 队列参数非法 / API 用法错"这类问题
  *         会完全静默，只表现为"任务莫名不跑/卡死"。
  *         这里把它接到屏幕上：**直接把断言所在行号显示出来**，
  *         比对着源码猜快得多（本板 PC0 灯是坏件，只能靠屏）。
  */
void vApplicationAssertFailed(const char *file, int line)
{
    (void)file;                 /* 文件路径太长，屏上放不下，只显示行号 */
    __disable_irq();            /* 中断已关，下面不能用 HAL_Delay/vTaskDelay */

    if (spi_test_done)          /* LCD 已就绪才能画；否则句柄未初始化会 HardFault */
    {
        char buf[16];
        int_to_str((uint32_t)line, buf);
        LCD_FillScreen(LCD_RED);
        LCD_DrawString(8,       10, "ASSERT FAIL",  LCD_WHITE, LCD_RED);
        LCD_DrawString(8,       30, "line:",        LCD_WHITE, LCD_RED);
        LCD_DrawString(8 + 40,  30, buf,            LCD_WHITE, LCD_RED);
        while (1) { __asm volatile ("nop"); }   /* 停在这里，屏上信息保留 */
    }

    while (1)                   /* LCD 未就绪：退回灯闪 */
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0);
        for (volatile uint32_t i = 0; i < 100000; i++) {}
    }
}

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

/* ====================================================================== */
/*  硬件故障探针：把"静默死机"变成屏上可见                                   */
/* ====================================================================== */
/**
  * @brief  32 位整数转十六进制字符串（故障码用十进制看不懂）
  */
static void u32_to_hex(uint32_t v, char *buf)
{
    const char *d = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--)
    {
        buf[7 - i] = d[(v >> (i * 4)) & 0xFU];
    }
    buf[8] = 0;
}

/**
  * @brief  地址是否落在本芯片 RAM 内（0x20000000~0x2001FFFF，F407VE 128KB）
  * @note   故障时寄存器可能是垃圾值，**解引用前必须先判断**，
  *         否则故障处理程序自己会再触发一次故障（double fault，什么都看不到）。
  */
static uint8_t ram_ok(uint32_t a)
{
    return (a >= 0x20000000u && a < 0x20020000u) ? 1u : 0u;
}

/**
  * @brief  故障现场处理器（C 部分）：把证据打到屏上
  * @param  frame       出错那一刻的栈帧指针。ARMv7-M 异常入栈顺序固定：
  *                       frame[0..3]=R0~R3  frame[4]=R12  frame[5]=LR
  *                       frame[6]=PC（**出错的那条指令**）  frame[7]=xPSR
  * @param  exc_return  出错时的 LR（EXC_RETURN），用它可以判断"故障发生在哪":
  *                       0xFFFFFFF9 = Thread 模式 / MSP 栈（普通代码里出错）
  *                       0xFFFFFFFD = Thread 模式 / PSP 栈（任务代码里出错）
  *                       0xFFFFFFF1 = **Handler 模式**（在某个中断处理程序里出错！）
  *                       0xFFFFFFE9/ED = 带 FPU 扩展帧
  * @param  kind        故障类别字符：H=HardFault M=MemManage B=BusFault U=UsageFault
  */
void fault_dump_c(uint32_t *frame, uint32_t exc_return, uint32_t kind)
{
    __disable_irq();

    if (!spi_test_done)     /* LCD 还没就绪，画屏会解引用空句柄 → 只能灯闪 */
    {
        while (1)
        {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0);
            for (volatile uint32_t i = 0; i < 100000; i++) {}
        }
    }

    char l[40];
    int  k;
    const char *p;

    LCD_FillScreen(LCD_RED);

#define FLINE(y, label)                       \
    do {                                      \
        k = 0;                                \
        for (int m = 0; (label)[m]; m++) l[k++] = (label)[m]; \
        u32_to_hex(v, l + k); k += 8;         \
        LCD_DrawString(8, (y), l, LCD_WHITE, LCD_RED); \
    } while (0)

    uint32_t v;

    /* 哪一类故障 */
    k = 0;
    p = "FAULT: ";
    for (int m = 0; p[m]; m++) l[k++] = p[m];
    l[k++] = (char)kind;
    l[k] = 0;
    LCD_DrawString(8, 10, l, LCD_WHITE, LCD_RED);

    /* 故障状态寄存器 */
    v = SCB->CFSR; FLINE(30,  "CFSR:");
    v = SCB->HFSR; FLINE(50,  "HFSR:");

    /* ★ 故障发生在哪种模式 / 哪个栈（决定下一步往哪查） */
    v = exc_return; FLINE(70, "LR:");

    /* ★ 出错的那条指令地址 —— 拿它去反汇编就能精确到行 */
    v = (frame != NULL && ram_ok((uint32_t)frame)) ? frame[6] : 0xDEADBEEFu;
    FLINE(90,  "PC:");

    /* 出错前的 LR（通常指向调用它的那条下一条指令） */
    v = (frame != NULL && ram_ok((uint32_t)frame)) ? frame[5] : 0xDEADBEEFu;
    FLINE(110, "LR2:");

    /* 出错时的栈帧地址（判断 PSP 有没有被初始化过） */
    v = (uint32_t)frame; FLINE(130, "SP:");

    /* ★ FreeRTOS 当前任务 + 它的栈顶指针 */
    uint32_t tcb = (uint32_t)pxCurrentTCB;
    v = tcb; FLINE(150, "TCB:");

    uint32_t tos = 0, exc_slot = 0, entry = 0;
    if (ram_ok(tcb))
    {
        tos = ((uint32_t *)tcb)[0];
        if (ram_ok(tos) && ram_ok(tos + 16 * 4))
        {
            exc_slot = ((uint32_t *)tos)[8];    /* 必须是 FFFFFFFD */
            entry    = ((uint32_t *)tos)[15];   /* 任务入口 PC（应为 Flash 地址） */
        }
    }
    v = tos;      FLINE(170, "TOS:");
    v = exc_slot; FLINE(190, "EXC:");
    v = entry;    FLINE(210, "ENTRY:");

    while (1) { __asm volatile ("nop"); }
}

/* 接管四个硬件异常向量（覆盖启动文件里的 weak Default_Handler）。
 *
 * ⚠️ 必须用 `naked`：普通 C 函数的序言会先压栈、改写 SP 和 LR，
 *    那样就拿不到"出错那一刻"的真实 LR 和栈帧了。
 *
 * 汇编做的事（三条）：
 *   tst lr, #4      EXC_RETURN 的 bit2 = 1 表示出错时用的是 PSP（任务栈）
 *   mrseq/mrsne r0  → r0 = 出错时的栈帧指针（MSP 或 PSP）
 *   mov r1, lr      → r1 = EXC_RETURN（判断故障发生在哪种模式）
 *   b fault_dump_c  → 交给 C 打印（r2 已在前面装好类别字符）
 */
#define FAULT_STUB(name, kindchar)                       \
    void __attribute__((naked)) name(void)               \
    {                                                    \
        __asm volatile (                                 \
            "mov   r2, #" #kindchar "   \n"              \
            "tst   lr, #4               \n"              \
            "ite   eq                   \n"              \
            "mrseq r0, msp              \n"              \
            "mrsne r0, psp              \n"              \
            "mov   r1, lr               \n"              \
            "b     fault_dump_c         \n"              \
        );                                               \
    }

FAULT_STUB(HardFault_Handler,  72)   /* 'H' */
FAULT_STUB(MemManage_Handler,  77)   /* 'M' */
FAULT_STUB(BusFault_Handler,   66)   /* 'B' */
FAULT_STUB(UsageFault_Handler, 85)   /* 'U' */
