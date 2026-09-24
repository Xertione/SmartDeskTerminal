/**
  ******************************************************************************
  * @file    ui.c
  * @brief   LVGL 界面（Phase 6，复刻 v0.8 的手搓 UI 并升级）
  *
  * 与旧版（main.c 手搓 DrawString + hit-test）的对照：
  *   旧：Task_Touch 轮询 + 锁存 hit-test + 队列 → Task_LCD 收消息重画整行
  *   新：LVGL 按钮的 CLICKED 事件回调里直接改 label → LVGL 自己算脏区只刷那块
  *   旧：心跳行 = Task_LCD 每 2s 手动重画
  *   新：lv_timer_create(1000ms) 定时回调更新 —— lv_timer 是 LVGL 内置的
  *       "软定时器"，在 lv_timer_handler 里巡检执行，替代了手搓任务+队列
  *
  * 控件树（flex 纵向排列）：
  *   screen
  *   ├─ label  标题  "SmartDesk v0.9.0-lvgl"
  *   ├─ label  环境  RTOS/LCD/Touch/SYSCLK/Build
  *   ├─ btn    "PRESS ME" ── CLICKED ──▶ Hits 计数 +1
  *   ├─ label  "Hits: n"
  *   ├─ label  "FRTh: n  LVMem: n%"   （1s 刷新）
  *   └─ label  "HB n"                  （1s 刷新 = lv_timer 活着的证据）
  *   另：LV_USE_PERF_MONITOR=1 → 右下角自动叠 FPS/CPU 显示
  ******************************************************************************
  */

#include "lvgl.h"
#include "ui.h"
#include "agent_msg.h"
#include "bsp/usb_cdc.h"
#include "FreeRTOS.h"
#include "task.h"      /* xTaskGetTickCount */

/* ---------------------------- 固件版本标识 ---------------------------- */
/* 人维护的版本号 + 编译器自动填的 Build 时间 —— 烧录后先核对这两行，
   确认跑的是刚编出来的固件（沿用 v0.8 的验证习惯） */
#define FW_VERSION  "v0.9.0-lvgl"

/* ---------------------------- 控件引用 ---------------------------- */
static lv_obj_t *label_hits;   /* Hits 计数 */
static lv_obj_t *label_mem;    /* 两个内存池余量 */
static lv_obj_t *label_hb;     /* 心跳（1s 跳一次 = lv_timer_handler 在跑） */

static uint32_t hit_count = 0;
static uint32_t hb_count  = 0;

/* ---------------------------- 回调 ---------------------------- */

/** 按钮 CLICKED（完整按下+松开，替代旧版手搓的边沿锁存 hit-test） */
static void btn_event_cb(lv_event_t *e)
{
    (void)e;
    hit_count++;
    lv_label_set_text_fmt(label_hits, "Hits: %lu", (unsigned long)hit_count);
}

/** 1s 定时刷新：内存余量 + 心跳计数（替代旧版 Task_LCD 的 2s 手动重画） */
static void info_timer_cb(lv_timer_t *t)
{
    (void)t;
    hb_count++;

    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);   /* LVGL 池：used_pct = 已用百分比 */

    lv_label_set_text_fmt(label_mem, "FRTh: %u  LVMem: %u%%",
                          (unsigned)xPortGetFreeHeapSize(),
                          (unsigned)mon.used_pct);
    lv_label_set_text_fmt(label_hb, "HB %lu  Uptime %lus",
                          (unsigned long)hb_count,
                          (unsigned long)(xTaskGetTickCount() / 1000U));
}

/* ---------------------------- 工具 ---------------------------- */

/** 快捷：建一个 label 设好颜色放进 flex 流 */
static lv_obj_t *add_label(lv_obj_t *parent, const char *text, uint32_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, text);
    return l;
}

/* ---------------------------- 界面搭建 ---------------------------- */

void ui_create(void)
{
    lv_obj_t *scr = lv_scr_act();

    /* 深色背景（不依赖主题，和旧版黑底观感一致） */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101010), 0);

    /* flex 纵向流布局：子控件从上往下排，间距 6px */
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scr, 10, 0);
    lv_obj_set_style_pad_row(scr, 6, 0);

    /* ---- 信息区（复刻 v0.8 屏上的每一行）---- */
    add_label(scr, "SmartDesk " FW_VERSION, 0xFFFFFF);
    add_label(scr, "RTOS: FreeRTOS v10",    0x00C853);
    add_label(scr, "UI: LVGL 8.3",          0x00C853);
    add_label(scr, "LCD: ST7789V 240x320",  0x00C853);
    add_label(scr, "Touch: XPT2046 RTP",    0x00C853);
    add_label(scr, "SYSCLK: 168 MHz",       0x00E5FF);
    add_label(scr, "Build " __DATE__ " " __TIME__, 0x9E9E9E);

    /* ---- 按钮（点击默认主题自带按下变色反馈，替代旧版手搓闪红）---- */
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 150, 45);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, "PRESS ME");
    lv_obj_center(bl);

    /* ---- 计数 / 内存 / 心跳 ---- */
    label_hits = add_label(scr, "Hits: 0",  0xFFD54F);
    label_mem  = add_label(scr, "FRTh: -  LVMem: -", 0x9E9E9E);
    label_hb   = add_label(scr, "HB 0", 0x9E9E9E);

    /* 1s 刷新定时器（lv_timer = LVGL 内置软定时器） */
    lv_timer_create(info_timer_cb, 1000, NULL);
}

/* ---------------------------- Agent 队列应用 ---------------------------- */

/**
  * @brief  收 Agent 任务发来的队列消息，应用到 UI（在 Task_LVGL 上下文调）
  * @note   必须在 Task_LVGL 里调 —— LVGL 非线程安全，只有这个任务能改控件。
  *         Agent 任务通过 agent_send 把意图丢过来，本函数落地执行。
  */
void ui_poll_agent(void)
{
    AgentMsg msg;
    /* 不阻塞：没消息立即返回（Task_LVGL 主循环里高频调用） */
    while (agent_recv(&msg, 0))
    {
        switch (msg.type)
        {
        case AGENT_MSG_CLEAR_HITS:
            hit_count = 0;
            lv_label_set_text_fmt(label_hits, "Hits: 0");
            break;

        case AGENT_MSG_REPORT_HITS:
            USB_CDC_Printf("hits=%lu\r\n", (unsigned long)hit_count);
            break;

        case AGENT_MSG_SHOW_TEXT:
            /* 预留：Phase 9 Agent 推送消息到屏（暂未用到） */
            break;

        default:
            break;
        }
    }
}
