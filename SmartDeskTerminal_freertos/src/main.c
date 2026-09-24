/**
  ******************************************************************************
  * @file    main.c
  * @brief   SmartDeskTerminal - Phase 6 LVGL 移植（v0.9.0-lvgl）
  *
  * 架构演进（Phase 3+4 → Phase 6）：
  *   旧：Task_LED(p1) + Task_Touch(p3) + Task_LCD(p2) + 队列
  *       （手搓 hit-test + DrawString 整行重画）
  *   新：Task_LVGL 单任务 —— lv_init + 移植层 + UI，循环跑 lv_timer_handler
  *       LVGL 的 lv_timer（软定时器）+ 事件回调接管了原来的多任务+队列分工：
  *         触摸轮询  → LVGL indev read_cb（30ms，内部巡检）
  *         按钮判定  → LVGL 事件系统（CLICKED）
  *         界面刷新  → LVGL 脏区渲染（只重画变化的小块）
  *         心跳/内存 → lv_timer 1s 定时回调（ui.c）
  *       队列暂时退役 —— Phase 8 USB CDC 的 Agent→UI 消息通道会重新启用。
  *
  * SysTick 共存（沿用 ADR-001 方案）：
  *   SysTick_Handler 先调 HAL_IncTick（保 HAL 时基），
  *   再调 xPortSysTickHandler（FreeRTOS tick，调度器启动后）
  *   LVGL 时基不走 SysTick —— lv_conf.h 用 LV_TICK_CUSTOM 直接挂
  *   xTaskGetTickCount()。
  *
  * 引脚：LED_PC0（坏件，仅留 Error_Handler 兜底）/ 见 wiring.md
  ******************************************************************************
  */

#include "stm32f4xx_hal.h"
#include "bsp/key.h"
#include "bsp/lcd.h"
#include "bsp/touch.h"

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"

/* LVGL */
#include "lvgl.h"
#include "lvgl_port.h"
#include "ui.h"

/* ---------------------------- 全局变量（供 SWD 监视） ---------------------------- */
volatile uint8_t  spi_test_done = 0;   /* LCD 就绪标志（错误路径画屏前必须检查） */

/* FreeRTOS 当前任务控制块（定义在 tasks.c）。
   fault_dump_c 里读它取当前任务栈顶。 */
extern void *volatile pxCurrentTCB;

/* FreeRTOS 的 SysTick 处理函数（port.c 定义但头文件没声明，显式声明消 UB） */
extern void xPortSysTickHandler(void);

/* ---------------------------- 函数声明 ---------------------------- */
static void SystemClock_Config(void);
static void LED_GPIO_Init(void);
static void Error_Handler(void);
static void int_to_str(uint32_t val, char *buf);
static void u32_to_hex(uint32_t v, char *buf);
static uint8_t ram_ok(uint32_t a);
static void Task_LVGL(void *arg);

/* ====================================================================== */
/*                              主程序入口                                 */
/* ====================================================================== */
int main(void)
{
    /* 悬挂项清账（原靠 0x0 flash 别名侥幸）：显式把向量表指到真实 flash 地址 */
    SCB->VTOR = FLASH_BASE;             /* FLASH_BASE = 0x08000000 */

    HAL_Init();
    LED_GPIO_Init();
    SystemClock_Config();

    /* BSP 初始化（HAL_Delay 在调度器启动前用，合法） */
    Key_Init();
    LCD_Init();
    LCD_ST7789_Init();
    Touch_Init();

    spi_test_done = 1;                  /* LCD 就绪：错误路径可以画屏了 */

    /* 单任务架构：LVGL 全部逻辑跑在一个任务里（串行化 SPI 天然成立）。
       栈 768 字 = 3KB：lv_timer_handler 渲染路径 + flush 调用链，
       后续用 uxTaskGetStackHighWaterMark 验证余量。 */
    if (xTaskCreate(Task_LVGL, "LVGL", 768, NULL, 3, NULL) != pdPASS)
        Error_Handler();

    /* 启动调度器。正常情况下此函数永不返回。 */
    vTaskStartScheduler();

    Error_Handler();                    /* 能走到这里 = 调度器异常返回 */
}

/* ====================================================================== */
/*                              任务实现                                    */
/* ====================================================================== */

/**
  * @brief  Task_LVGL：LVGL 唯一宿主任务（官方推荐 RTOS 集成模式）
  * @note   lv_timer_handler 每轮处理到期的 timer（indev 巡检 / 界面刷新 /
  *         用户 lv_timer），处理完睡 5ms 让出 CPU。
  *         ⚠️ 所有 LVGL API（含 ui.c 回调里的）都在本任务上下文执行，
  *         不需要加锁 —— Phase 8 若别的任务要碰 UI，必须走队列中转。
  */
static void Task_LVGL(void *arg)
{
    (void)arg;

    lv_init();              /* LVGL 内核（内存池就在这时挂上 32KB 静态数组） */
    lvgl_port_init();       /* 显示 flush + 触摸 read 两个接缝（lvgl_port.c） */
    ui_create();            /* 界面控件 + 1s 刷新定时器（ui.c） */

    while (1)
    {
        lv_timer_handler();                  /* 跑到期的所有 timer */
        vTaskDelay(pdMS_TO_TICKS(5));        /* 睡 5ms，不吃满 CPU */
    }
}

/* ====================================================================== */
/*                              辅助函数                                    */
/* ====================================================================== */

/**
  * @brief  整数转字符串（错误路径屏显用，不用 sprintf 省 Flash）
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
  * @note   PC0 实物是坏件（T-005）—— 只作为 Error_Handler 最后兜底输出
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
  *         LVGL 不占 SysTick（LV_TICK_CUSTOM 挂 xTaskGetTickCount）
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
  * @note   ⚠️⚠️ 本板 PC0 用户 LED 是**已知坏件**（T-005）——
  *         "闪灯报错"在本板等于没有提示，错误输出**必须走屏幕**。
  *         只有 LCD 初始化完成（spi_test_done=1）才能画屏，否则 hspi3 句柄
  *         未初始化，调 HAL_SPI_Transmit 会解引用空指针 → HardFault。
  */
static void Error_Handler(void)
{
    __disable_irq();                      /* 注意：中断已关，不能用 HAL_Delay/vTaskDelay */

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
  * @brief  configASSERT 失败时的出口（FreeRTOSConfig.h 的宏调用）
  * @note   默认 configASSERT 是**空宏** —— 配置错误完全静默。
  *         这里接到屏幕：**直接显示断言所在行号**（本板灯坏，只能靠屏）。
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
    /* FreeRTOS 堆分配失败，进 Error_Handler */
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
/*  硬件故障探针：把"静默死机"变成屏上可见（T-001~T-004 排查体系的遗产，       */
/*  常驻保留 —— LVGL 崩溃时这里直接写 SPI，不经过 LVGL，天然无冲突）          */
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
