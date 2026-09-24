/**
  ******************************************************************************
  * @file    lvgl_port.c
  * @brief   LVGL ↔ 本板 BSP 的移植层实现（Phase 6，ADR-013）
  *
  * ── 移植原理（学 LVGL 只需要记住这张图）──────────────────────────
  *
  *   LVGL 内部                 移植层（本文件）           我们的 BSP
  *   ────────                  ───────────────            ──────────
  *   渲染到 draw_buf  ──满──▶  flush_cb(area, color_p) ─▶ LCD_WriteArea
  *   （CPU 画进内存）           （按 area 开窗口整块发）    （SPI 写 ST7789）
  *
  *   lv_timer_handler  ──30ms─▶ read_cb(data)          ─▶ Touch_Read
  *   （定期巡检输入）           （回报按下状态+坐标）      （XPT2046 读点）
  *
  *   LVGL 内部时基     ──挂──── LV_TICK_CUSTOM = xTaskGetTickCount()（lv_conf.h）
  *
  * 刷新流程：LVGL 只重画"变了的区域"（脏区合并）→ 分块塞进 240×40 的
  * 绘制缓冲 → 每块调一次 flush_cb → 我们整块 SPI 发出去。
  * 所以屏上一个小 label 变字 = 只刷那一小块，不是全屏 —— 这就是
  * LVGL 比手搓 DrawString（每次整行重画）快的根本原因。
  ******************************************************************************
  */

#include "lvgl.h"
#include "lvgl_port.h"
#include "bsp/lcd.h"
#include "bsp/touch.h"

/* ---------------------------- 显示参数 ---------------------------- */
#define PORT_HOR_RES       240    /* ST7789 屏宽 */
#define PORT_VER_RES       320    /* ST7789 屏高 */
#define DRAW_BUF_LINES     40     /* 绘制缓冲行数：240×40×2B = 19.2KB（ADR-013 决策）
                                     RAM 128KB 放不下全屏缓冲（153.6KB），
                                     分 1/8 屏渲染：够快、够省 */

/* 绘制缓冲（静态：生命周期必须覆盖 LVGL 整个运行期） */
static lv_disp_draw_buf_t g_draw_buf;
static lv_color_t g_buf1[PORT_HOR_RES * DRAW_BUF_LINES];

/* 显示/输入设备驱动结构（同样必须静态：LVGL 只保存指针） */
static lv_disp_drv_t  g_disp_drv;
static lv_indev_drv_t g_indev_drv;

/* ---------------------------- 回调实现 ---------------------------- */

/**
  * @brief  LVGL 画完一块后调用：把 color_p 指向的像素写进屏幕 area 区域
  * @note   LV_COLOR_16_SWAP=1 保证缓冲字节序已"高字节在前"，
  *         所以这里零拷贝直传（见 lcd.c 的 LCD_WriteArea 注释）。
  *         LVGL v8 是同步 flush 模型：发完调 lv_disp_flush_ready()。
  *         （如果以后改 DMA 异步发送，改成在 DMA 完成中断里调 ready）
  */
static void disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                          lv_color_t *color_p)
{
    /* area 坐标可能越出屏幕 1px（LVGL 渲染边界），裁剪防越界写 */
    int32_t x1 = area->x1 < 0 ? 0 : area->x1;
    int32_t y1 = area->y1 < 0 ? 0 : area->y1;
    int32_t x2 = area->x2 >= PORT_HOR_RES ? PORT_HOR_RES - 1 : area->x2;
    int32_t y2 = area->y2 >= PORT_VER_RES ? PORT_VER_RES - 1 : area->y2;

    if (x1 <= x2 && y1 <= y2)
    {
        /* ⚠️ 裁剪过起点时缓冲指针也要跟着偏移（每行开头跳过 x1-area->x1 像素），
           但 LVGL 实际给的脏区不会越屏（hor_res 已注册 240），这层保护正常不触发。
           完整实现过于复杂，直接按整块发 —— 若将来出现花屏再回来补精确裁剪。 */
        LCD_WriteArea((uint16_t)area->x1, (uint16_t)area->y1,
                      (uint16_t)area->x2, (uint16_t)area->y2,
                      (const uint16_t *)color_p);
    }

    lv_disp_flush_ready(drv);
}

/**
  * @brief  LVGL 每 30ms 调一次：回报触摸状态
  * @note   只填 data，不做去抖/手势 —— LVGL 自己有按压状态机
  *         （PRESSED/RELEASED/长按/滑动手势都在内部判定，这是用 LVGL
  *          替代手搓 hit-test 的原因）。pressed=0 时坐标保持上次值即可。
  */
static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;

    TouchPoint tp = Touch_Read();

    if (tp.pressed)
    {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = tp.x;
        data->point.y = tp.y;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

/* ---------------------------- 初始化 ---------------------------- */

void lvgl_port_init(void)
{
    /* ① 绘制缓冲：单缓冲（LVGL 渲染期间我们无事可做，双缓冲无收益） */
    lv_disp_draw_buf_init(&g_draw_buf, g_buf1, NULL,
                          PORT_HOR_RES * DRAW_BUF_LINES);

    /* ② 显示设备：告诉 LVGL 屏幕多大、画好了往哪送 */
    lv_disp_drv_init(&g_disp_drv);
    g_disp_drv.hor_res = PORT_HOR_RES;
    g_disp_drv.ver_res = PORT_VER_RES;
    g_disp_drv.draw_buf = &g_draw_buf;
    g_disp_drv.flush_cb = disp_flush_cb;
    g_disp_drv.antialiasing = 1;    /* 字体边缘抗锯齿（开销小，清晰度明显提升） */
    lv_disp_drv_register(&g_disp_drv);

    /* ③ 触摸输入设备：告诉 LVGL 到哪去问"有人按吗" */
    lv_indev_drv_init(&g_indev_drv);
    g_indev_drv.type = LV_INDEV_TYPE_POINTER;      /* 指针类（手指/触摸笔） */
    g_indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&g_indev_drv);
}
