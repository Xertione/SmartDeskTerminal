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

/* printf 风格：用 vsnprintf 进临时缓冲再发（避免 USB 端点大小限制） */
void USB_CDC_Printf(const char *fmt, ...)
{
    if (!USB_CDC_IsConfigured()) return;

    static char buf[160];   /* 静态避免栈占用；不并发调用，安全 */
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0)
    {
        if (n > (int)sizeof(buf)) n = sizeof(buf);
        USB_CDC_Send(buf, (uint16_t)n);
    }
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
