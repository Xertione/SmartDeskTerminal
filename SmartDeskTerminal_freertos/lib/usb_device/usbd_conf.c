/**
  ******************************************************************************
  * @file    usbd_conf.c
  * @brief   USB Device Library 底层钩子 —— 把 USB Device Library 的抽象调用
  *          映射到 STM32 HAL PCD（Peripheral Controller Driver）的具体操作。
  *
  * 这一层是"USB 协议库 ↔ 硬件驱动"的接缝：
  *   协议库说"打开端点 1"   → USBD_LL_OpenEP → HAL_PCD_EP_Open
  *   协议库说"发送这 64 字节" → USBD_LL_Transmit → HAL_PCD_EP_Transmit
  *
  * ── 2026-09-25 修复的三个缺陷（Phase 8 回归排查）──────────────────
  *  ① 【严重】句柄符号重名 → 内存别名
  *     `USBD_HandleTypeDef hUsbDeviceFS`（usb_cdc.c）与
  *     `PCD_HandleTypeDef  hUsbDeviceFS`（本文件）**同名不同型**。
  *     本工程工具链默认 -fcommon，两个 tentative definition 被链接器
  *     **静默合并成同一个地址**（ELF 实证：单个符号 size=0x4E4=1252B，
  *     正好等于 PCD_HandleTypeDef 的大小 —— 即 USBD 句柄被"折叠"掉了）。
  *     后果：USBD_Init 建好的协议栈句柄被随后的 HAL_PCD_Init 整块覆写；
  *     协议库再通过 pClassData 等字段解引用，拿到的是 PCD 结构里的字节
  *     → **任意地址读写** → 外部表现即"烧录后花屏 / 随机崩溃"。
  *     修法：PCD 句柄改名 `hpcd_USB_OTG_FS`（ST 官方命名），彻底分离。
  *  ② 【严重】缺 HAL_PCD_MspInit → PA11/PA12 从未配成 USB 功能
  *     HAL_PCD_Init 内部会回调 HAL_PCD_MspInit；本文件原先没有实现，
  *     落到 HAL 的 weak 空函数 → 引脚停在复位态（浮空输入）
  *     → **USB 永远无法枚举 → PC 看不到 COM 口**。
  *  ③ USBD_malloc 原先映射 newlib malloc（见 usbd_conf.h 注释）
  *     → 改成静态 arena（本文件实现）。
  ******************************************************************************
  */

#include "usbd_core.h"
#include "usbd_ctlreq.h"
#include "stm32f4xx_hal_pcd.h"

/* 全局 PCD 句柄（OTG_FS）。
   ⚠️ 名字必须与 usb_cdc.c 的 `USBD_HandleTypeDef hUsbDeviceFS` **不同**！
      两个句柄是**两套完全不同的结构体**，同名会被链接器合并成同一块内存
      互相覆写 —— 这正是本次花屏的根因。改名后彻底隔离。 */
PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* =========================== 静态分配器 =========================== */
/* USB 协议库需要一块持久内存放 class data（CDC 约 540 字节）。
   这里用 .bss 里的固定 arena，不碰 newlib 堆、也不碰 FreeRTOS 堆。 */
#define USB_ARENA_SIZE   768U                 /* 540 字节需求 + 余量 */
/* ⚠️ 必须显式 8 字节对齐：USBD_CDC_HandleTypeDef 开头是 `uint32_t data[128]`，
   而 `uint8_t[]` 默认只保证 1 字节对齐（实测首版落在 0x2000F367 这种奇地址上）。
   虽然 Cortex-M4 允许非对齐的 LDR/STR，但 LDM/STM 与库函数（memcpy/memset）
   的成块访问会因此变慢甚至触发 UNALIGNED 用法错误。 */
static uint8_t  usbd_arena[USB_ARENA_SIZE] __attribute__((aligned(8)));
static uint8_t  usbd_arena_live;              /* 0=空闲 / 1=已分配 */

void *USBD_StaticMalloc(uint32_t size)
{
    if (usbd_arena_live) return NULL;          /* 单槽位：重复分配视为错误 */
    if (size == 0U || size > USB_ARENA_SIZE) return NULL;
    usbd_arena_live = 1U;
    return usbd_arena;
}

void USBD_StaticFree(void *p)
{
    if (p != NULL && (uint8_t *)p == usbd_arena)
    {
        usbd_arena_live = 0U;
    }
}

/* =========================== LL 钩子实现 =========================== */

/* 只启用 OTG_FS 的 AHB 时钟。USB 需要的 48MHz 来自 PLLQ=7（ADR-003 已配）。 */
static void SystemClock_Config_USB(void)
{
    __HAL_RCC_USB_OTG_FS_CLK_ENABLE();
}

/**
  * @brief  PCD 底层初始化（HAL 在 HAL_PCD_Init 内部回调本函数）
  * @note   ⚠️ 必须实现！缺了它 PA11/PA12 就是浮空输入，USB 不可能枚举成功。
  *         原版本文件里没有这个函数 —— 这是"没有虚拟串口"的第一层原因。
  *
  * 引脚（RM0090 表 9 / `doc/datasheets_md/06_核心板_原理图与引脚映射.md` §USB）：
  *   PA11 = USB_OTG_FS_DM
  *   PA12 = USB_OTG_FS_DP
  *   两者都必须配成 AF10（GPIO_AF10_OTG_FS）+ 推挽复用。
  *
  * VBUS：本工程 vbus_sensing_enable = DISABLE（Type-C 座直接给 5V，
  *       不做 VBUS 检测），因此**不配 PA9**，避免与 H1 排针的
  *       USART1_TX(PA9) 争夺同一引脚。
  */
void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hpcd->Instance == USB_OTG_FS)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA11 = DM, PA12 = DP —— 一起配，避免只配一根导致枚举不稳定 */
        GPIO_InitStruct.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;          /* 复用推挽 */
        GPIO_InitStruct.Pull      = GPIO_NOPULL;              /* USB 收发器内部自带上下拉 */
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;         /* AF10 = OTG_FS */
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* 中断优先级 6：数值上 >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY(5)，
           所以 USB 中断里也能安全调用 FreeRTOS 的 FromISR API。
           ⚠️ HAL_NVIC_SetPriority 内部会做 <<4 移位（T-002 的教训已封装在库里）。 */
        HAL_NVIC_SetPriority(OTG_FS_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
    }
}

/** 反初始化（HAL_PCD_DeInit 回调）：把引脚还给默认态，便于复用/低功耗 */
void HAL_PCD_MspDeInit(PCD_HandleTypeDef *hpcd)
{
    if (hpcd->Instance == USB_OTG_FS)
    {
        __HAL_RCC_USB_OTG_FS_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);
        HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
    }
}

/* USBD_LL_Init 由协议库在初始化路径里调用，把 hpcd 装进 USBD_HandleTypeDef */
USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
    SystemClock_Config_USB();

    /* 关联两个句柄：USBD 句柄 ↔ PCD 句柄，互相记住对方地址。
       ⚠️ 这里用的是**两个不同的变量**，不再是同一个名字。 */
    hpcd_USB_OTG_FS.pData = pdev;
    pdev->pData = &hpcd_USB_OTG_FS;

    hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
    hpcd_USB_OTG_FS.Init.dev_endpoints = 4U;          /* F407 OTG_FS 4 个 OUT + 4 个 IN 端点 */
    hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;  /* F407 FS 内置 PHY */
    hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
    hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;        /* FS 不支持 DMA */
    hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;  /* 本板不接 VBUS 检测，简化 */
    hpcd_USB_OTG_FS.Init.Sof_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.battery_charging_enable = DISABLE;

    /* ⚠️ HAL_PCD_Init 内部会回调 HAL_PCD_MspInit（本文件上方实现）
       —— 引脚 AF 配置与 NVIC 都在那里面完成，这里不需要重复设置。 */
    if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK)
    {
        while (1) { /* 静默死循环：此前屏已初始化，上层会因 USB 不响应立即看到 */ }
    }

    return USBD_OK;
}

/* 其余 LL 钩子：转发到 HAL_PCD_xxx —— 全部是模板转发 */
USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)         { return (USBD_StatusTypeDef)HAL_PCD_DeInit(pdev->pData); }
USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)          { return (USBD_StatusTypeDef)HAL_PCD_Start(pdev->pData); }
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)           { return (USBD_StatusTypeDef)HAL_PCD_Stop(pdev->pData); }
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
                                                          { return (USBD_StatusTypeDef)HAL_PCD_EP_Flush(pdev->pData, ep_addr); }
USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                       uint8_t ep_type, uint16_t ep_mps)
                                                          { return (USBD_StatusTypeDef)HAL_PCD_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type); }
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
                                                          { return (USBD_StatusTypeDef)HAL_PCD_EP_Close(pdev->pData, ep_addr); }
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                         uint8_t *pbuf, uint32_t size)
                                                          { return (USBD_StatusTypeDef)HAL_PCD_EP_Transmit(pdev->pData, ep_addr, pbuf, size); }
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                               uint8_t *pbuf, uint32_t size)
                                                          { return (USBD_StatusTypeDef)HAL_PCD_EP_Receive(pdev->pData, ep_addr, pbuf, size); }
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
                                                          { return (USBD_StatusTypeDef)HAL_PCD_EP_SetStall(pdev->pData, ep_addr); }
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
                                                          { return (USBD_StatusTypeDef)HAL_PCD_EP_ClrStall(pdev->pData, ep_addr); }
USBD_StatusTypeDef USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    PCD_HandleTypeDef *hpcd = pdev->pData;
    if ((ep_addr & 0x80U) == 0x80U) return (USBD_StatusTypeDef)hpcd->IN_ep[ep_addr & 0x7FU].is_stall;
    return (USBD_StatusTypeDef)hpcd->OUT_ep[ep_addr & 0x7FU].is_stall;
}
USBD_StatusTypeDef USBD_LL_SetDevAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)
                                                          { return (USBD_StatusTypeDef)HAL_PCD_SetAddress(pdev->pData, dev_addr); }
USBD_StatusTypeDef USBD_LL_GetUSBStatus(USBD_HandleTypeDef *pdev)
{
    /* USB 协议库自己维护 dev_state（DEFAULT/ADDRESSED/CONFIGURED），
       LL 钩子直接回报它即可 —— HAL PCD 状态机用 READY/RESET/BUSY，
       与 USB 协议状态是两套，不能直接映射。 */
    return (USBD_StatusTypeDef)pdev->dev_state;
}
USBD_StatusTypeDef USBD_LL_StartOfFrame(USBD_HandleTypeDef *pdev)   { (void)pdev; return USBD_OK; }  /* SOF 关 */

/* 接收端点当前收到的字节数（CDC DataOut 回调用它知道这次收了多少字节） */
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return HAL_PCD_EP_GetRxCount(pdev->pData, ep_addr);
}

void USBD_LL_Delay(uint32_t Delay)                        { HAL_Delay(Delay); }

/* USB 中断服务函数 —— 启动文件里 OTG_FS_IRQHandler 是 weak，这里覆盖
   ⚠️ 必须传 **PCD** 句柄（HAL_PCD_IRQHandler 要的是 PCD_HandleTypeDef*），
      不是 USBD 句柄。改名后编译器会直接挡住这类错误。 */
void OTG_FS_IRQHandler(void)
{
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}
