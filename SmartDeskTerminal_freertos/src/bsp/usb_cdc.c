/**
  ******************************************************************************
  * @file    bsp/usb_cdc.c
  * @brief   USB CDC 顶层接口实现
  *
  * 数据流：
  *   发：USB_CDC_Printf → vsnprintf 进临时缓冲 → CDC_Transmit_FS → USB 端点
  *   收：USB 中断回调 → 环形缓冲 → USB_CDC_Poll → CMD_Feed → 命令分发
  *
  * printf 重定向策略：USB_CDC_Printf 不覆盖系统 printf（避免 USB 未就绪时
  *   printf 卡死）。错误路径（Error_Handler / fault_dump_c）继续走屏幕，
  *   不依赖 USB —— USB 是软通道，启动期/故障期不可靠。
  ******************************************************************************
  */

#include "bsp/usb_cdc.h"
#include "cmd.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc_if.h"
#include "FreeRTOS.h"
#include "task.h"       /* taskENTER_CRITICAL / taskEXIT_CRITICAL */
#include <stdarg.h>
#include <string.h>
#include <stdio.h>      /* vsnprintf */

/* USB 设备句柄（usbd_conf.c 通过 USBD_LL_Init 把 hpcd_USB_OTG_FS.pData 装入这里） */
USBD_HandleTypeDef hUsbDeviceFS;

/* 初始化结果：0 = 成功，非 0 = 见 usb_cdc.h 的 USB_INIT_ERR_*
   ⚠️ 供 UI 与屏显读取 —— 这个量存在的意义就是"让失败可见"。 */
volatile uint8_t usb_init_err = USB_INIT_ERR_NONE;

/* USB 初始化
 *
 * ⚠️⚠️ 绝对不要写 "失败就 while(1) 死循环"（2026-09-26 血的教训）：
 *   本函数在 main() 里、**调度器启动之前**被调用。而此刻 BASEPRI 已被
 *   前面的 FreeRTOS 临界区设成 0x50（`uxCriticalNesting` 初值 0xAAAAAAAA，
 *   `vPortExitCritical()` 不会还原 BASEPRI，要等第一个任务启动才清），
 *   ⇒ **SysTick 已被屏蔽**。若在此死循环：
 *      · HAL_Delay 永久卡死（uwTick 不涨，超时永不触发）
 *      · 调度器永远起不来 → LVGL 不跑 → **花屏**
 *      · 屏上无任何提示（Error_Handler 的坏灯分支也是静默的）
 *   正确做法：记录错误码 + 正常返回，让 LVGL 照常跑起来，
 *   再把结果画在屏幕上 —— **可选外设失败不该拖死整个系统。**
 */
uint8_t USB_CDC_Init(void)
{
    usb_init_err = USB_INIT_ERR_NONE;

    /* 关联描述符 + CDC class + 接口回调，然后启动设备 */
    if (USBD_Init(&hUsbDeviceFS, &FS_Desc, 0) != USBD_OK)
    {
        /* 注意：底层 HAL_PCD_Init 失败也会走到这里（USBD_LL_Init 返回 USBD_FAIL）。
           用 g_usbd_pcd_init_failed 区分"是 PCD 没起来"还是"协议库自身出错"。 */
        usb_init_err = g_usbd_pcd_init_failed ? USB_INIT_ERR_PCD : USB_INIT_ERR_USBD_INIT;
        return usb_init_err;
    }

    if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK)
    {
        usb_init_err = USB_INIT_ERR_REG_CLASS;
        return usb_init_err;
    }

    USBD_CDC_If_Init();

    if (USBD_Start(&hUsbDeviceFS) != USBD_OK)
    {
        usb_init_err = USB_INIT_ERR_START;
        return usb_init_err;
    }

    return USB_INIT_ERR_NONE;
}

/* 发原始字节（CDC 不就绪时直接丢 —— 防止启动期阻塞） */
void USB_CDC_Send(const char *s, uint16_t len)
{
    if (!USB_CDC_IsConfigured()) return;
    /* CDC_Transmit_FS 内部会拷贝到 USB 端点缓冲，s 可复用 */
    CDC_Transmit_FS((uint8_t *)s, len);
}

/* printf 风格：用 vsnprintf 进持久缓冲再发（避免 USB 端点大小限制）
 *
 * ⚠️ 两个必须注意的点（2026-09-25 修）：
 *  ① 缓冲**必须持久**（不能放栈上）——CDC_Transmit_FS 只是把指针交给端点，
 *     真正的搬运发生在后续的 USB 中断（DataIn 阶段）。栈缓冲一返回就失效，
 *     中断读到的是被覆盖的栈 → 线上出现随机字节。
 *  ② 缓冲是 static 的，而本函数会被 **两个任务**调用：
 *       Task_Agent（上电欢迎语，main.c）
 *       Task_LVGL （`hits` 命令回复，走 ui_poll_agent → 本函数）
 *     Task_LVGL 优先级(3) > Task_Agent(2)，能在 vsnprintf 中途抢占它，
 *     导致输出串字节。原注释写的"不并发调用，安全"**不成立**。
 *     这里用临界区把"格式化 + 装载端点"变成一个原子段。
 */
void USB_CDC_Printf(const char *fmt, ...)
{
    if (!USB_CDC_IsConfigured()) return;

    static char buf[160];

    taskENTER_CRITICAL();      /* 关中断：原子化"格式化+装载"，防跨任务串字节 */
    {
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        if (n > 0)
        {
            /* vsnprintf 返回"本该写入的长度"，截断时会 > sizeof(buf)。
               ⚠️ 原实现写 `n = sizeof(buf)` 会多发 1 个字节（含结尾 '\0'），
                 这里夹到 sizeof(buf)-1，只发真实字符。 */
            if (n > (int)sizeof(buf) - 1) n = (int)sizeof(buf) - 1;
            USB_CDC_Send(buf, (uint16_t)n);
        }
    }
    taskEXIT_CRITICAL();
}

/* 轮询：取环形缓冲字节喂给命令解析器 */
void USB_CDC_Poll(void)
{
    while (CDC_ReadAvailable() > 0)
    {
        uint8_t b = CDC_ReadByte();
        CMD_Feed(b);
    }
}

/* USB 设备已配置（PC 端已枚举成 COM 口） */
uint8_t USB_CDC_IsConfigured(void)
{
    return (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED) ? 1 : 0;
}
