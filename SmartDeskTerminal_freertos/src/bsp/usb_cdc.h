/**
  ******************************************************************************
  * @file    bsp/usb_cdc.h
  * @brief   顶层 USB CDC 接口（封装协议库初始化 + 发送 + printf 重定向）
  *
  * 用法：
  *   USB_CDC_Init();                  // main 启动时调一次
  *   USB_CDC_Printf("...");           // 发字符串（替代 printf，走 USB 出去）
  *   USB_CDC_Poll();                  // Agent 任务循环里调，处理接收 + 命令解析
  ******************************************************************************
  */

#ifndef __BSP_USB_CDC_H
#define __BSP_USB_CDC_H

#include <stdint.h>

/* 初始化 USB CDC 设备（创建描述符 + 协议库 + 接口 + 启动枚举）
 * 返回值：0 = 成功；非 0 = 失败（见下方 USB_INIT_ERR_* 常量）
 * ⚠️ 调用方**必须**检查返回值并把结果显示出来（屏/串口）。
 *    绝不能再有"失败就 while(1)"那种设计 —— 会导致整个系统静默吊死。 */
uint8_t USB_CDC_Init(void);

/* USB 初始化错误码（usb_cdc.c 里的全局 usb_init_err） */
#define USB_INIT_ERR_NOT_YET     0xFFU  /* 还没初始化（USB_CDC_Init 尚未被调用） */
#define USB_INIT_ERR_NONE        0U   /* 成功 */
#define USB_INIT_ERR_USBD_INIT   1U   /* USBD_Init 失败（含底层 PCD 初始化失败） */
#define USB_INIT_ERR_REG_CLASS   2U   /* USBD_RegisterClass 失败 */
#define USB_INIT_ERR_START       3U   /* USBD_Start 失败 */
#define USB_INIT_ERR_PCD         4U   /* HAL_PCD_Init 失败（usbd_conf.c 上报） */

/* 初始化结果（供 UI / 屏显读取）。
   ⚠️ 初值是 USB_INIT_ERR_NOT_YET —— 因为 USB 初始化在 Task_Agent 里做（调度器启动后），
      而 UI 在 Task_LVGL 启动时就创建了；不区分"还没做"和"成功了"会让 UI 误报。 */
extern volatile uint8_t usb_init_err;

/* 底层 PCD 初始化失败标志（定义在 usbd_conf.c） */
extern volatile uint8_t g_usbd_pcd_init_failed;

/* 发字符串（不含 \0，自带 \r\n 不替换 —— 上层自己加） */
void USB_CDC_Send(const char *s, uint16_t len);

/* Printf 风格发送 —— 替代旧版的 printf（USB CDC 出口，替代被跳过的 UART 通道） */
void USB_CDC_Printf(const char *fmt, ...);

/* 轮询：取环形缓冲字节喂给命令解析器（Agent 任务循环里调） */
void USB_CDC_Poll(void);

/* 设备是否已枚举成功（PC 端能看到 COM 口） */
uint8_t USB_CDC_IsConfigured(void);

#endif /* __BSP_USB_CDC_H */
