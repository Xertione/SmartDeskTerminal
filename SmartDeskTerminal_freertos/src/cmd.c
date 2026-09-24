/**
  ******************************************************************************
  * @file    cmd.c
  * @brief   命令解析器实现
  ******************************************************************************
  */

#include "cmd.h"
#include "bsp/usb_cdc.h"
#include "agent_msg.h"      /* 通过队列给 LVGL 任务发指令 */
#include <string.h>

#define FW_VERSION  "v0.9.1-cdc"

/* 行缓冲（最长一行命令长度上限） */
#define CMD_BUF_LEN 64
static char    cmd_buf[CMD_BUF_LEN];
static uint8_t cmd_len;

/* 命令行回显提示符（让 PC 串口工具看着像个 shell） */
static void send_str(const char *s)
{
    USB_CDC_Send(s, (uint16_t)strlen(s));
}

/* 单条命令分发 */
static void cmd_dispatch(const char *line)
{
    /* 去掉行尾 \r */
    char buf[CMD_BUF_LEN + 1];
    uint8_t n = 0;
    while (line[n] && line[n] != '\r' && n < CMD_BUF_LEN) { buf[n] = line[n]; n++; }
    buf[n] = 0;

    if (n == 0) { send_str("\r\n"); return; }

    if (strcmp(buf, "hello") == 0)
    {
        send_str("Hello SmartDesk!\r\n");
    }
    else if (strcmp(buf, "version") == 0)
    {
        send_str("SmartDesk " FW_VERSION "\r\n");
        send_str("Build " __DATE__ " " __TIME__ "\r\n");
    }
    else if (strcmp(buf, "hits") == 0)
    {
        send_str("ok\r\n");   /* Hits 计数存在 ui.c 里，下面走队列让它自己回 */
        AgentMsg msg = { .type = AGENT_MSG_REPORT_HITS };
        agent_send(&msg);
    }
    else if (strcmp(buf, "clear") == 0)
    {
        send_str("cleared\r\n");
        AgentMsg msg = { .type = AGENT_MSG_CLEAR_HITS };
        agent_send(&msg);
    }
    else if (strcmp(buf, "ping") == 0)
    {
        send_str("pong\r\n");
    }
    else if (strcmp(buf, "help") == 0)
    {
        send_str("cmds: hello version hits clear ping help\r\n");
    }
    else
    {
        send_str("ERR: unknown cmd (try help)\r\n");
    }
}

void CMD_Feed(uint8_t b)
{
    /* \r 或 \n 触发分发；连续两个分隔符只发一次 */
    if (b == '\r' || b == '\n')
    {
        if (cmd_len > 0)
        {
            cmd_buf[cmd_len] = 0;
            cmd_dispatch(cmd_buf);
            cmd_len = 0;
        }
        return;
    }

    /* 回退键（PuTTY 默认）允许改最后一个字符 */
    if (b == 0x7F || b == 0x08)
    {
        if (cmd_len > 0) cmd_len--;
        return;
    }

    /* 追加字节，缓冲满就强制分发 */
    if (cmd_len < CMD_BUF_LEN - 1)
    {
        cmd_buf[cmd_len++] = (char)b;
    }
    else
    {
        cmd_buf[cmd_len] = 0;
        cmd_dispatch(cmd_buf);
        cmd_len = 0;
    }
}
