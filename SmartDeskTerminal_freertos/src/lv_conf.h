/**
  ******************************************************************************
  * @file    lv_conf.h
  * @brief   LVGL v8.3 配置（Phase 6，ADR-013）
  *
  * 查找方式：platformio.ini 的 -DLV_CONF_INCLUDE_SIMPLE 让 lvgl.h
  *           #include "lv_conf.h"（配合 -Isrc 找到本文件）。
  *
  * 只写"非默认/有决策含义"的项，未写的项走 lv_conf_internal.h 默认值。
  * 本文件三个核心决策（为什么这么配见 decision-log.md ADR-013）：
  *   1. LV_COLOR_16_SWAP=1 —— SPI 屏大端序：LVGL 直接把像素按
  *      "高字节在前"存进绘制缓冲，flush 时整块字节流发 SPI，零换序开销
  *   2. LV_MEM_CUSTOM=0 + 32KB —— LVGL 自带静态数组池，与 FreeRTOS
  *      heap_4（8KB）两池分离，出问题好定位
  *   3. LV_TICK_CUSTOM=1 —— 时基直接挂 FreeRTOS tick（1ms），
  *      不需要在 SysTick 里调 lv_tick_inc()
  ******************************************************************************
  */

#ifndef LV_CONF_H
#define LV_CONF_H

/* ========================= 颜色 ========================= */
#define LV_COLOR_DEPTH     16     /* RGB565，与 ST7789 COLMOD=0x05 一致 */

/* SPI 是 MSB first 逐字节发送，RGB565 的 uint16 在小端 MCU 内存里
 * 低字节在前 —— 不交换的话屏幕先收到低字节，颜色错乱。
 * SWAP=1 后 LVGL 内部就按交换序存，flush 可以直接把缓冲当字节流发。 */
#define LV_COLOR_16_SWAP   1

/* ========================= 内存 ========================= */
/* 静态数组池（lv_mem 内部 static uint8_t池[32K]），不从 FreeRTOS 堆拿 */
#define LV_MEM_CUSTOM      0
#define LV_MEM_SIZE        (32U * 1024U)

/* ========================= 时基 ========================= */
/* 挂 FreeRTOS tick：configTICK_RATE_HZ=1000 → 1 tick = 1ms，恰好是
 * LVGL 期望的毫秒单位。调度器没启动时 xTaskGetTickCount() 返回 0，无害。
 * ⚠️ include 用包装头 lvgl_tick_source.h：FreeRTOS 要求 task.h 前必须
 * 先有 FreeRTOS.h，单塞 task.h 会 #error（实测）。 */
#define LV_TICK_CUSTOM              1
#define LV_TICK_CUSTOM_INCLUDE      "lvgl_tick_source.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (xTaskGetTickCount())

/* ========================= 显示/输入节拍 ========================= */
#define LV_DISP_DEF_REFR_PERIOD  30     /* 显示刷新周期 ms */
#define LV_INDEV_DEF_READ_PERIOD 30     /* 触摸读取周期 ms（read_cb 被调频率）*/
#define LV_DPI_DEF               130    /* 2.8" 240×320 的典型值 */

/* ========================= 日志：关（省 flash） ========================= */
#define LV_USE_LOG    0

/* ========================= 字体 ========================= */
/* montserrat_16：14~18px 里选 16，240 宽屏上清晰；只编一个字号省 flash。
 * 中文（lv_font_simsun_16_cjk）体积 ~170KB，Phase 9/10 有需要再开。 */
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_DEFAULT         &lv_font_montserrat_16

/* ========================= 主题 ========================= */
#define LV_USE_THEME_DEFAULT    1
#define LV_THEME_DEFAULT_DARK   0      /* 亮色主题（黑底屏配深色背景由 UI 自己设） */
#define LV_USE_THEME_BASIC      0      /* 以下两个备用主题关掉省 flash */
#define LV_USE_THEME_MONO       0

/* ========================= 性能监视器 ========================= */
/* 屏幕右下角自动显示 FPS + CPU 占用 —— 验证 LVGL 刷新性能的第一手数据 */
#define LV_USE_PERF_MONITOR     1

/* ========================= 布局 ========================= */
#define LV_USE_FLEX             1      /* 纵向流式布局用 */
#define LV_USE_GRID             0

/* ========================= 控件裁剪 ========================= */
/* 只留本项目用得到的，其余全关（每个控件是独立编译开关，关 = 代码不进固件）。
 * Phase 9/10（PC Agent 状态页）需要新控件时来这里开。 */
#define LV_USE_ARC          0
#define LV_USE_ANIMIMG      0
#define LV_USE_BAR          1      /* 进度条备用（Agent 状态指示） */
#define LV_USE_BTN          1      /* ★ 主角：PRESS ME 按钮 */
#define LV_USE_BTNMATRIX    0
#define LV_USE_CANVAS       0
#define LV_USE_CALENDAR     0
#define LV_USE_CHART        0      /* Phase 10 数据可视化时再开 */
#define LV_USE_CHECKBOX     0
#define LV_USE_COLORWHEEL   0
#define LV_USE_DROPDOWN     0
#define LV_USE_IMG          0
#define LV_USE_IMGBTN       0
#define LV_USE_KEYBOARD     0
#define LV_USE_LABEL        1      /* ★ 主角：所有文字 */
#define LV_USE_LED          0
#define LV_USE_LINE         0
#define LV_USE_LIST         0
#define LV_USE_MENU         0
#define LV_USE_METER        0
#define LV_USE_MSGBOX       0
#define LV_USE_SPAN         0
#define LV_USE_SPINBOX      0
#define LV_USE_SPINNER      0
#define LV_USE_SWITCH       0
#define LV_USE_TEXTAREA     0
#define LV_USE_TILEVIEW     0
#define LV_USE_TABVIEW      0
#define LV_USE_WIN          0

#endif /* LV_CONF_H */
