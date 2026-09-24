/**
  ******************************************************************************
  * @file    agent_msg.c
  * @brief   Agent ↔ UI 队列实现
  ******************************************************************************
  */

#include "agent_msg.h"
#include "FreeRTOS.h"
#include "queue.h"

static QueueHandle_t agent_queue = NULL;

void agent_queue_init(void)
{
    agent_queue = xQueueCreate(8, sizeof(AgentMsg));
}

void agent_send(const AgentMsg *msg)
{
    if (agent_queue == NULL) return;
    xQueueSend(agent_queue, msg, pdMS_TO_TICKS(100));
}

uint8_t agent_recv(AgentMsg *out, uint32_t block_ms)
{
    if (agent_queue == NULL) return 0;
    return (xQueueReceive(agent_queue, out, pdMS_TO_TICKS(block_ms)) == pdTRUE) ? 1 : 0;
}
