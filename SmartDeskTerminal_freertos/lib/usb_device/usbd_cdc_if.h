/**
  ******************************************************************************
  * @file    usbd_cdc_if.h
  * @brief   CDC 应用层接口声明
  ******************************************************************************
  */

#ifndef __USBD_CDC_IF_H__
#define __USBD_CDC_IF_H__

#include "usbd_def.h"
#include "usbd_cdc.h"

void     USBD_CDC_If_Init(void);
uint8_t  CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);
uint16_t CDC_ReadAvailable(void);
uint8_t  CDC_ReadByte(void);

#endif /* __USBD_CDC_IF_H__ */
