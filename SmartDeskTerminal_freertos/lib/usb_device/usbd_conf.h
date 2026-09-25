/**
  ******************************************************************************
  * @file    usbd_conf.h
  * @brief   USB Device 配置宏（替代框架的 usbd_conf_template.h）
  *
  * 中间件源码（usbd_core.c 等）#include "usbd_conf.h" —— 本文件就是它。
  * 内容：USB 设备能力上限 + 包大小 + 是否支持 LPM 等编译期常量。
  * 数值取 STM32CubeMX 生成 USB CDC 工程的标准默认（F407 OTG_FS）。
  ******************************************************************************
  */

#ifndef __USBD_CONF__H__
#define __USBD_CONF__H__

#include <stdint.h>
#include <stddef.h>
#include <string.h>    /* memset/memcpy 宏映射用 */
#include <stdlib.h>    /* malloc/free 宏映射用 */
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_pwr.h"

/* ---- USB 设备能力上限 ---- */
#define USBD_MAX_NUM_INTERFACES               1U    /* CDC 只用 2 个接口（通信+数据），但 USBD 用 1 表示单 class */
#define USBD_MAX_NUM_CONFIGURATION            1U
#define USBD_MAX_STR_DESC_SIZ                 512U
#define USBD_SELF_POWERED                     1U
#define USBD_DEBUG_LEVEL                      0U    /* 关 USB 库内部 printf */
#define USBD_LPM_ENABLED                      0U    /* 不用 LPM（Link Power Management） */
#define USBD_KEEP_CFG_DESCRIPTOR              1U
#define USBD_KEEP_DEVICE_DESC                 0U

/* ---- 包大小：OTG_FS 全速 = 64 字节 ---- */
#define USBD_EP0_MAX_PACKET_SIZE              64U

/* ---- 数据包大小（CDC 数据端点 OUT/IN）---- */
#define USB_CDC_DATA_PACKET_MAX_SIZE          64U   /* 全速批量大包 */

/* 给 USB Device Library 用：选 OTG_FS（不是 HS） */
#define USE_USB_FS                            1U

/* ---- USB 协议库的工具宏 ---- */
#define USBD_memset                           memset
#define USBD_memcpy                           memcpy

/* ⚠️⚠️ 不要映射到 newlib 的 malloc/free（2026-09-25 修）⚠️⚠️
 *
 * 原实现 `#define USBD_malloc malloc` 有三个致命问题：
 *   ① malloc 用 newlib 堆（靠链接脚本的 _Min_Heap_Size + _sbrk 增长），
 *      与 FreeRTOS 的 ucHeap、LVGL 的静态池**不是同一套内存管理**；
 *      在带 RTOS 的工程里这是"两套堆并存"的隐患，边界全靠链接脚本运气。
 *   ② malloc/free **不是线程安全的**，本工程有 Task_LVGL / Task_Agent
 *      两个任务上下文，虽然 USB 只在启动期分配一次，但这个约束不该靠"恰好"。
 *   ③ 一旦 _sbrk 未正确实现或堆耗尽，malloc 返回 NULL 或野指针，
 *      USB 协议库会拿它当 class data 反复解引用 → **任意地址读写**。
 *
 * 改为**静态 arena 分配**（定义在 usbd_conf.c）：
 *   - 永不失败在"堆边界"上（arena 是 .bss 里的固定数组，编译器/链接器管）
 *   - 零碎片、零依赖、零线程安全问题
 *   - USB CDC 生命周期只分配一次（约 540 字节），单槽位足够
 */
void  *USBD_StaticMalloc(uint32_t size);
void   USBD_StaticFree(void *p);

#define USBD_malloc                           USBD_StaticMalloc
#define USBD_free                             USBD_StaticFree
#define USBD_Delay                            HAL_Delay

#endif /* __USBD_CONF__H__ */
