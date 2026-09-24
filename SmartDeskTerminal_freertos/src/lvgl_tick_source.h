/**
  ******************************************************************************
  * @file    lvgl_tick_source.h
  * @brief   LVGL 时基包装头（lv_conf.h 的 LV_TICK_CUSTOM_INCLUDE 用）
  *
  * 为什么需要这个文件：
  *   FreeRTOS 规定 task.h 之前必须先 include FreeRTOS.h（task.h 里有显式
  *   #error 检查，单独 include 直接编译失败 —— 2026-09-25 实测踩过）。
  *   而 LVGL 的 LV_TICK_CUSTOM_INCLUDE 宏只有一个文件槽，
  *   塞不下两条 include。所以用这个包装头一次带齐，lv_conf.h 引用它。
  ******************************************************************************
  */

#ifndef __LVGL_TICK_SOURCE_H
#define __LVGL_TICK_SOURCE_H

#include "FreeRTOS.h"
#include "task.h"

#endif /* __LVGL_TICK_SOURCE_H */
