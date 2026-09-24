/**
  ******************************************************************************
  * @file    lvgl_port.h
  * @brief   LVGL ↔ 本板 BSP 的移植层（Phase 6，ADR-013）
  *
  * LVGL 移植只需要四个接缝，本层占三个（第四个 tick 在 lv_conf.h
  * 用 LV_TICK_CUSTOM 挂 FreeRTOS，不用代码）：
  *   ① lv_init() 之后的显示注册（draw_buf + flush_cb）
  *   ② flush_cb：LVGL 画好一块 → LCD_WriteArea 写进 ST7789
  *   ③ read_cb：LVGL 每 30ms 来问触摸状态 → Touch_Read
  ******************************************************************************
  */

#ifndef __LVGL_PORT_H
#define __LVGL_PORT_H

/* 初始化绘制缓冲 + 注册显示设备 + 注册触摸输入设备。
   必须在 lv_init() 之后、ui_create() 之前调用（任务上下文中）。 */
void lvgl_port_init(void);

#endif /* __LVGL_PORT_H */
