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

/* 初始化 USB CDC 设备（创建描述符 + 协议库 + 接口 + 启动枚举） */
void USB_CDC_Init(void);

/* 发字符串（不含 \0，自带 \r\n 不替换 —— 上层自己加） */
void USB_CDC_Send(const char *s, uint16_t len);

/* Printf 风格发送 —— 替代旧版的 printf（USB CDC 出口，替代被跳过的 UART 通道） */
void USB_CDC_Printf(const char *fmt, ...);

/* 轮询：取环形缓冲字节喂给命令解析器（Agent 任务循环里调） */
void USB_CDC_Poll(void);

/* 设备是否已枚举成功（PC 端能看到 COM 口） */
uint8_t USB_CDC_IsConfigured(void);

#endif /* __BSP_USB_CDC_H */
