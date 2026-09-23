/*
 * FreeRTOSConfig.h
 * SmartDeskTerminal - STM32F407VET6 @ 168MHz, Cortex-M4F
 *
 * 关键配置项（ADR-001 预言的坑，这里一一配对）：
 *   1. configCPU_CLOCK_HZ = 168000000（与 SystemClock_Config 一致）
 *   2. configTICK_RATE_HZ = 1000（1ms tick，与 HAL 的 SysTick 一致）
 *   3. configMAX_SYSCALL_INTERRUPT_PRIORITY = 0x50（Cortex-M4 的 BASEPRI 阈值，
 *      优先级数值 >= 5 的中断才能调用 FreeRTOS API）
 *   4. SysTick 冲突解决：SysTick_Handler 先调 HAL_IncTick 再调 xPortSysTickHandler
 *      （在调度器启动后才调 FreeRTOS 的），两者共用 SysTick 不冲突。
 *   5. configTOTAL_HEAP_SIZE = 8192（8KB 堆，够 3 个任务 + 队列）
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------
 * 基本配置
 * ----------------------------------------------------------- */
#define configUSE_PREEMPTION            1      /* 抢占式调度 */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1  /* 用 CLZ 指令优化任务选择 */
#define configUSE_TICKLESS_IDLE         0      /* 不用 tickless 省电（调试期关掉） */
#define configCPU_CLOCK_HZ              ( 168000000UL )  /* 主频 168MHz */
#define configTICK_RATE_HZ              ( ( TickType_t ) 1000 )  /* 1ms 一个 tick */
#define configMAX_PRIORITIES            ( 7 )  /* 优先级数 0~6 */
#define configMINIMAL_STACK_SIZE        ( ( uint16_t ) 128 )  /* 最小栈 128 字（512 字节） */
#define configUSE_16_BIT_TICKS          0      /* 32bit tick 计数 */
#define configIDLE_SHOULD_YIELD         1      /* 空闲任务让出 */
#define configUSE_TASK_NOTIFICATIONS    1
#define configUSE_MUTEXES               1
#define configUSE_RECURSIVE_MUTEXES     1
#define configUSE_COUNTING_SEMAPHORES   1
#define configUSE_QUEUE_SETS            0
#define configUSE_TIME_SLICING          1      /* 同优先级时间片轮转 */
#define configUSE_NEWLIB_REENTRANT      0
#define configENABLE_BACKWARD_COMPATIBILITY 1
#define configSUPPORT_STATIC_ALLOCATION 0  /* 暂用动态分配，省静态回调函数 */
#define configSUPPORT_DYNAMIC_ALLOCATION 1

/* -----------------------------------------------------------
 * ARM_CM4F 移植层专用配置（ADR-012）
 * ----------------------------------------------------------- */
/* 必须定义 configUSE_TASK_FPU_SUPPORT 或让 port.c 自动检测 */
/* ARM_CM4F port.c 默认会保存 FPU 寄存器，无需额外配置 */

/* -----------------------------------------------------------
 * 内存配置（ADR-001 坑 1：堆大小）
 * ----------------------------------------------------------- */
#define configTOTAL_HEAP_SIZE           ( ( size_t ) 8192 )  /* 8KB 堆 */
#define configSTACK_ALLOCATION_FROM_SEPARATE_HEAP 0

/* -----------------------------------------------------------
 * 中断优先级配置（ADR-001 坑 2：可屏蔽中断阈值）
 * F407 用 4bit 抢占优先级（NVIC_SetPriorityGrouping 设的）。
 * 数值越大优先级越低。configMAX_SYSCALL_INTERRUPT_PRIORITY = 0x50
 * 意味着：优先级数值 5~15 的中断可以调 FreeRTOS API，0~4 不行。
 * ----------------------------------------------------------- */
#define configPRIO_BITS                 4    /* F407 用 4 位抢占优先级 */
/* ⚠️⚠️ 必须左移 (8 - configPRIO_BITS) = 4 位！⚠️⚠️
 *
 * STM32 的 NVIC 优先级寄存器是 8 位，但只实现高 4 位（bit7:4），
 * 低 4 位丢弃。所以写「优先级 15」必须写成 0xF0，不能写 0x0F：
 *   写 0x0F → 实现位 = 0x0F >> 4 = 0  → 实际优先级 **0（最高）**
 *   写 0xF0 → 实现位 = 0xF0 >> 4 = 15 → 实际优先级 15（最低）
 *
 * 这两个宏都是**直接写进硬件寄存器**的值，不是"优先级编号"：
 *   configKERNEL_INTERRUPT_PRIORITY → port.c 写进 SHPR3（PendSV/SysTick 优先级）
 *   configMAX_SYSCALL_INTERRUPT_PRIORITY → 写进 BASEPRI（临界区屏蔽阈值）
 *
 * 历史坑（2026-09-23 修复）：此前写成未左移的 15 / 5，后果是
 *   ① PendSV 与 SysTick 落到**最高优先级 0**，违背"内核中断必须最低"的铁律；
 *   ② BASEPRI = 5 只能屏蔽 1~15，屏蔽不掉优先级 0 →
 *      **临界区挡不住 PendSV/SysTick**，内核链表可能被中断中途改写。
 * 反汇编证据：vPortEnterCritical 里是 `mov.w r3, #5; msr BASEPRI, r3`。
 * ----------------------------------------------------------- */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15  /* 最低优先级（编号） */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5  /* 可调 API 的最低优先级（编号） */
#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )        /* = 0xF0 */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )   /* = 0x50 */

/* -----------------------------------------------------------
 * 可选 API
 * ----------------------------------------------------------- */
#define configUSE_CO_ROUTINES           0
#define configMAX_CO_ROUTINE_PRIORITIES 2
#define configUSE_TIMERS                1      /* timers.c 需要 */
#define configTIMER_TASK_PRIORITY      3
#define configTIMER_QUEUE_LENGTH       10
#define configTIMER_TASK_STACK_DEPTH   (configMINIMAL_STACK_SIZE * 2)
#define INCLUDE_vTaskDelete             1
#define INCLUDE_vTaskDelay              1
#define INCLUDE_xTaskGetSchedulerState  1
#define INCLUDE_vTaskPrioritySet        1
#define INCLUDE_uxTaskPriorityGet       1
#define INCLUDE_vTaskSuspend            1
#define INCLUDE_xTaskDelayUntil         1
#define INCLUDE_xTaskGetTickCount       1
#define INCLUDE_xTaskAbortDelay         1
#define INCLUDE_xSemaphoreCreateMutex   1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_xTaskGetHandle          0
#define INCLUDE_eTaskGetState          1
#define INCLUDE_xEventGroupWaitBits    1
#define INCLUDE_xTimerPendFunctionCall 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1

/* -----------------------------------------------------------
 * 断言（ASSERT）：让 FreeRTOS 内部的配置/参数检查变得可见
 * -----------------------------------------------------------
 * 不定义 configASSERT 时，它在 FreeRTOS.h 里是个**空宏**：
 * 所有 configASSERT(...) 检查全部失效，配置写错（优先级分组不符、
 * 队列参数非法、API 用法不当）都会**完全静默**，只表现为
 * "任务不跑 / 卡死"而没有任何提示 —— 极难排查。
 *
 * 这里接一个在**屏幕**上报错的钩子（本板 PC0 LED 是坏件，只能靠屏）：
 * 断言失败时屏上显示红底白字 + 断言所在行号。
 * 实现见 main.c 的 vApplicationAssertFailed()。
 * ----------------------------------------------------------- */
void vApplicationAssertFailed(const char *file, int line);
#define configASSERT( x )    if( ( x ) == 0 ) vApplicationAssertFailed( __FILE__, __LINE__ )

/* -----------------------------------------------------------
 * FreeRTOS 与 HAL 的 SysTick 共存
 * SysTick_Handler 先调 HAL_IncTick() 再调 xPortSysTickHandler()
 * （在 main.c 的 SysTick_Handler 实现）
 * ----------------------------------------------------------- */
#define configUSE_IDLE_HOOK             0
#define configUSE_TICK_HOOK             0
#define configCHECK_FOR_STACK_OVERFLOW  2  /* 栈溢出检测方式 2（最严） */
#define configUSE_MALLOC_FAILED_HOOK    1  /* 堆分配失败钩子 */

/* -----------------------------------------------------------
 * DMA / 内存保护（暂不用）
 * ----------------------------------------------------------- */
#define configUSE_RECURSIVE_MUTEXES     1

/* -----------------------------------------------------------
 * 中断函数映射（port.c 里的中断处理函数名）
 * ----------------------------------------------------------- */
#define vPortSVCHandler         SVC_Handler
#define xPortPendSVHandler       PendSV_Handler
/* SysTick_Handler 在 main.c 手动实现（HAL_IncTick + xPortSysTickHandler） */

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_CONFIG_H */
