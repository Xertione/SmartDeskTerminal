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

/* USB 设备句柄（usbd_conf.c 通过 USBD_LL_Init 把 hUsbDeviceFS.pData 装入这里） */
USBD_HandleTypeDef hUsbDeviceFS;

/* USB 初始化（等价 STM32CubeMX 生成的 MX_USB_DEVICE_Init） */
void USB_CDC_Init(void)
{
    /* 关联描述符 + CDC class + 接口回调，然后启动设备 */
    if (USBD_Init(&hUsbDeviceFS, &FS_Desc, 0) != USBD_OK) return;
    USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC);
    USBD_CDC_If_Init();
    USBD_Start(&hUsbDeviceFS);
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
