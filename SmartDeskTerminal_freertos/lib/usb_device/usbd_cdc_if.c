/**
  ******************************************************************************
  * @file    usbd_cdc_if.c
  * @brief   CDC 应用层接口（接收回调 + 发送函数）
  *
  * 协议库在收到 USB 数据时调 CDC_Receive_FS 把字节推进来。
  * 这里把它们存到环形缓冲，主循环 USB_CDC_Poll 再取出解析 ——
  * 不在 USB 中断上下文做命令解析（一来中断里不能调 RTOS API，
  * 二来解析耗时会让 USB 端点丢包）。
  ******************************************************************************
  */

#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "stm32f4xx_hal.h"
#include <string.h>     /* memcpy（CDC_Control_FS 回应主机查询用） */

/* 全局 USB 设备句柄（usbd_conf.c 的 hUsbDeviceFS 通过 USBD_Init 装入这里） */
extern USBD_HandleTypeDef hUsbDeviceFS;

/* 接收环形缓冲（CDC 单包最大 64 字节，给 256 让出余量） */
#define CDC_RX_BUF_SIZE 256
static volatile uint8_t  cdc_rx_buf[CDC_RX_BUF_SIZE];
static volatile uint16_t cdc_rx_head;
static volatile uint16_t cdc_rx_tail;

/* 发送状态标志（HAL_PCD_DataInStageCallback 设置后我们才能发下一包） */
static volatile uint8_t  cdc_tx_ready;

/* HAL PCD 直接写入这里的接收缓冲（被 CDC_Receive_FS 用作参数） */
static uint8_t cdc_rx_buf_raw[64];

/* ---- 接收回调（协议库在 USB 中断里调） ---- */
static int8_t CDC_Init_FS(void)
{
    cdc_rx_head = cdc_rx_tail = 0;
    cdc_tx_ready = 1;
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, NULL, 0);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, cdc_rx_buf_raw);
    return USBD_OK;
}

static int8_t CDC_DeInit_FS(void) { return USBD_OK; }

/* 行编码（波特率等）。CDC 虚拟串口不限速，但**必须能正确回应主机查询**，
   否则某些 Windows 版本/驱动会因"设备未按规范应答"而不创建 COM 口。 */
static USBD_CDC_LineCodingTypeDef cdc_line_coding = {
    115200,      /* baudrate */
    0x00,        /* stop bits: 1 */
    0x00,        /* parity: none */
    0x08         /* data bits: 8 */
};

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    switch (cmd)
    {
    case CDC_SET_LINE_CODING:      /* 主机下发（0x20）：存下来，不回数据 */
        if (length >= sizeof(cdc_line_coding))
        {
            memcpy(&cdc_line_coding, pbuf, sizeof(cdc_line_coding));
        }
        break;

    case CDC_GET_LINE_CODING:      /* 主机查询（0x21）：必须回 7 字节，否则枚举可能失败 */
        memcpy(pbuf, &cdc_line_coding, sizeof(cdc_line_coding));
        break;

    case CDC_SET_CONTROL_LINE_STATE:   /* DTR/RTS，本工程不关心 */
    case CDC_SEND_ENCAPSULATED_COMMAND:
    case CDC_GET_ENCAPSULATED_RESPONSE:
    default:
        break;
    }
    return USBD_OK;
}

/* 接收数据回调：协议库把数据放进 cdc_rx_buf_raw，本函数把它搬到环形缓冲 */
/* static uint8_t cdc_rx_buf_raw[64]; 已挪到文件上方 CDC_Init_FS 之前 */
static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
    uint32_t n = *Len;
    if (n > sizeof(cdc_rx_buf_raw)) n = sizeof(cdc_rx_buf_raw);

    /* 把接收到的字节推进环形缓冲（防止溢出：满就丢新的） */
    for (uint32_t i = 0; i < n; i++)
    {
        uint16_t next = (cdc_rx_head + 1) % CDC_RX_BUF_SIZE;
        if (next == cdc_rx_tail) break;   /* 满：丢 */
        cdc_rx_buf[cdc_rx_head] = Buf[i];
        cdc_rx_head = next;
    }

    /* 准备接收下一包（USB 协议要求显式 re-arm） */
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, cdc_rx_buf_raw);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}

/* CDC_Itf 结构（协议库通过它知道我们的回调在哪） */
static USBD_CDC_ItfTypeDef USBD_CDC_Interface = {
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS,
};

/* ---- 对外接口 ---- */

/* 主程序调用一次：注册 CDC 接口 */
void USBD_CDC_If_Init(void)
{
    USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_CDC_Interface);
}

/* 从环形缓冲取一个字节（0 表示空） */
uint16_t CDC_ReadAvailable(void)
{
    int32_t n = (int32_t)cdc_rx_head - (int32_t)cdc_rx_tail;
    if (n < 0) n += CDC_RX_BUF_SIZE;
    return (uint16_t)n;
}

uint8_t CDC_ReadByte(void)
{
    if (cdc_rx_head == cdc_rx_tail) return 0;
    uint8_t b = cdc_rx_buf[cdc_rx_tail];
    cdc_rx_tail = (cdc_rx_tail + 1) % CDC_RX_BUF_SIZE;
    return b;
}

/* 发送一串字节（阻塞直到 USB 端点空闲） */
uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len)
{
    if (Len == 0) return USBD_OK;
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
    return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}
