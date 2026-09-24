/**
  ******************************************************************************
  * @file    agent_msg.h
  * @brief   Agent ↔ UI 任务通信（队列回归，ADR-013 退役的队列在此重启）
  *
  * 为什么用队列而不是直接调 LVGL API：
  *   FreeRTOS 任务隔离 + LVGL 非线程安全 —— Agent 任务（USB CDC 轮询）
  *   不能直接碰 UI 控件，必须通过队列把"意图"丢给 LVGL 任务，
  *   由 LVGL 任务在自己的上下文里改 UI（这是 LVGL 官方推荐的多任务用法）。
  *
  * 数据流：
  *   cmd.c 解析命令 → agent_send → xQueueSend → LVGL 任务 lv_timer 回调取出 → 改 UI
  ******************************************************************************
  */

#ifndef __AGENT_MSG_H
#define __AGENT_MSG_H

#include <stdint.h>

/* 消息类型 */
typedef enum {
    AGENT_MSG_CLEAR_HITS    = 1,    /* 清 Hits 计数 */
    AGENT_MSG_REPORT_HITS   = 2,    /* 通过 USB 回报当前 Hits 数值 */
    AGENT_MSG_SHOW_TEXT     = 3,    /* 显示一段文本到屏上（字段 text） */
} AgentMsgType;

typedef struct {
    AgentMsgType type;
    char text[32];   /* SHOW_TEXT 用，限 31 字节 */
} AgentMsg;

/* 初始化队列（main 启动时调一次） */
void agent_queue_init(void);

/* 发消息（Agent 任务 → 队列 → LVGL 任务）。100ms 超时，超时丢消息 */
void agent_send(const AgentMsg *msg);

/* 收消息（LVGL 任务里调）。block_ms=0 表示不阻塞；返回 1=收到，0=超时 */
uint8_t agent_recv(AgentMsg *out, uint32_t block_ms);

#endif /* __AGENT_MSG_H */
