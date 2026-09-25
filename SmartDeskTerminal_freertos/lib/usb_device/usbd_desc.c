/**
  ******************************************************************************
  * @file    usbd_desc.c
  * @brief   USB 设备描述符（VID/PID/字符串）
  *
  * PC 插上 USB 后识别的设备身份：
  *   VID=0x1234 PID=0x5678（ST 官方例程测试值，正式产品需替换）
  *   Windows 端用 usbser.sys 自动识别为 COM 口（PID 0x5678 已被 INF 适配）
  ******************************************************************************
  */

#include "usbd_def.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"   /* 库自带 USBD_GetString 签名：(uint8_t*desc, uint8_t*unicode, uint16_t*len) */
#include "usbd_desc.h"

/* ---- 设备标识 ---- */
#define USBD_VID                    0x1234
#define USBD_PID_FS                 0x5678
#define USBD_LANGID_STRING          1033    /* 美式英语 */
#define USBD_MANUFACTURER_STRING    "SmartDesk"
#define USBD_PRODUCTSTRING_FS       "SmartDesk CDC Port"
#define USBD_SERIALNUMBER_STRING_FS "00000000001A"
#define USBD_CONFIGURATION_STRING_FS "CDC Config"
#define USBD_INTERFACE_STRING_FS   "CDC Interface"

/* ---- 描述符句柄（外部用） ---- */
uint8_t *USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);

USBD_DescriptorsTypeDef FS_Desc = {
    USBD_FS_DeviceDescriptor,
    USBD_FS_LangIDStrDescriptor,
    USBD_FS_ManufacturerStrDescriptor,
    USBD_FS_ProductStrDescriptor,
    USBD_FS_SerialStrDescriptor,
    USBD_FS_ConfigStrDescriptor,
    USBD_FS_InterfaceStrDescriptor,
};

/* ---- 设备描述符（18 字节固定结构）---- */
__ALIGN_BEGIN uint8_t USBD_FS_DeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END = {
    0x12,                       /* bLength */
    USB_DESC_TYPE_DEVICE,       /* bDescriptorType */
    0x00, 0x02,                 /* bcdUSB = 2.00 */
    0x02,                       /* bDeviceClass = CDC */
    0x00,                       /* bDeviceSubClass */
    0x00,                       /* bDeviceProtocol */
    USB_MAX_EP0_SIZE,           /* bMaxPacketSize = 64 */
    LOBYTE(USBD_VID), HIBYTE(USBD_VID),
    LOBYTE(USBD_PID_FS), HIBYTE(USBD_PID_FS),
    0x00, 0x02,                 /* bcdDevice = 2.00 */
    1,                          /* iManufacturer */
    2,                          /* iProduct */
    3,                          /* iSerialNumber */
    1                           /* bNumConfigurations */
};

/* ---- 语言 ID 字符串描述符 ---- */
__ALIGN_BEGIN uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END = {
    USB_LEN_LANGID_STR_DESC,
    USB_DESC_TYPE_STRING,
    LOBYTE(USBD_LANGID_STRING),
    HIBYTE(USBD_LANGID_STRING),
};

/* ---- 字符串描述符实现（用库自带的 USBD_GetString 把 ASCII 转 USB 字符串描述符） ---- */
/* 库签名：void USBD_GetString(uint8_t *desc, uint8_t *unicode, uint16_t *len)
   desc=ASCII 源串；unicode=输出缓冲（每字符 2 字节 + 头 2 字节）；len=输出长度 */
#define MAKE_STR_DESC(NAME, UTF8)                                              \
uint8_t *NAME(USBD_SpeedTypeDef speed, uint16_t *length)                       \
{                                                                              \
    (void)speed;                                                               \
    static __ALIGN_BEGIN uint8_t buf[USBD_MAX_STR_DESC_SIZ+1] __ALIGN_END;     \
    USBD_GetString((uint8_t *)(UTF8), buf, length);                            \
    return buf;                                                                \
}

uint8_t *USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = USB_LEN_DEV_DESC;
    return USBD_FS_DeviceDesc;
}

uint8_t *USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = USB_LEN_LANGID_STR_DESC;
    return USBD_LangIDDesc;
}

MAKE_STR_DESC(USBD_FS_ManufacturerStrDescriptor, USBD_MANUFACTURER_STRING)
MAKE_STR_DESC(USBD_FS_ProductStrDescriptor,      USBD_PRODUCTSTRING_FS)
MAKE_STR_DESC(USBD_FS_SerialStrDescriptor,       USBD_SERIALNUMBER_STRING_FS)
MAKE_STR_DESC(USBD_FS_ConfigStrDescriptor,       USBD_CONFIGURATION_STRING_FS)
MAKE_STR_DESC(USBD_FS_InterfaceStrDescriptor,    USBD_INTERFACE_STRING_FS)
