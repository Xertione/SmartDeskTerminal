/**
  ******************************************************************************
  * @file    cmd.h
  * @brief   命令解析器（Phase 8 雏形，Phase 9 Agent 协议升级）
  *
  * 协议：行式文本命令，\r 或 \n 结尾（兼容 PuTTY/Arduino 串口监视器）。
  *
  * 当前命令集：
  *   hello            → "Hello SmartDesk!"
  *   version          → 固件版本 + Build 时间
  *   hits             → 当前按钮 Hits 计数
  *   clear            → 通过队列请 LVGL 重置 Hits 计数
  *   ping             → "pong"
  *
  * 不支持的命令 → "ERR: unknown cmd"
  ******************************************************************************
  */

#ifndef __CMD_H
#define __CMD_H

#include <stdint.h>

/* 喂一个字节给解析器（\r 或 \n 触发命令分发） */
void CMD_Feed(uint8_t b);

#endif /* __CMD_H */
