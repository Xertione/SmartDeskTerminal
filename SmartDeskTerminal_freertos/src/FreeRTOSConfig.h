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
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15  /* 最低优先级 */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5  /* 可调 API 的最低优先级 */
#define configKERNEL_INTERRUPT_PRIORITY         configLIBRARY_LOWEST_INTERRUPT_PRIORITY
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY

/* -----------------------------------------------------------
 * 时钟与 PendSV/SVC（ADR-001 坑 4：中断优先级）
 * xPortSysTickHandler / xPortPendSVHandler / vPortSVCHandler
 * 的优先级由 port.c 自动设，这里只声明映射
 * ----------------------------------------------------------- */
#define configKERNEL_INTERRUPT_PRIORITY         configLIBRARY_LOWEST_INTERRUPT_PRIORITY

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
