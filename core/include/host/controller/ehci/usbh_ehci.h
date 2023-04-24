/*******************************************************************************
 *                                    ZLG
 *                         ----------------------------
 *                         innovating embedded platform
 *
 * Copyright (c) 2001-present Guangzhou ZHIYUAN Electronics Co., Ltd.
 * All rights reserved.
 *
 * Contact information:
 * web site:    https://www.zlg.cn
 *******************************************************************************/
#ifndef __USBH_EHCI_H
#define __USBH_EHCI_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "core/include/host/core/usbh_dev.h"
#include "core/include/host/core/usbh.h"
#include "adapter/usb_adapter.h"
#include "common/pool/usb_pool.h"
#include "common/list/usb_list.h"
#include "common/usb_common.h"
#include "core/include/host/controller/ehci/usbh_ehci_xfer.h"

#if USB_OS_EN
/* \brief EHCI 互斥锁超时时间*/
#define USBH_EHCI_MUTEX_TIMEOUT 5000
#endif

/* \brief EHCI 数据结构内存分配方式*/
#define USBH_EHCI_MEM_POOL 1    /* 内存池方式*/
#define USBH_EHCI_MEM      2    /* 直接分配方式*/

/* \brief EHCI 任务棧大小*/
#define USBH_EHCI_TASK_STACK_SIZE  (8192)
/* \brief 帧列表大小：1024(4K), 512(2K), 256(1K) */
#define USBH_EHCI_FRAME_LIST_SIZE  (256)
/* \brief 一个微帧的带宽，125*80% us */
#define USBH_EHCI_UFRAME_BANDWIDTH (100)
/* \brief 存储微帧带宽的数量，同时也是最小的周期 */
#define USBH_EHCI_BANDWIDTH_SIZE   (80)

#define USBH_EHCI_TUNE_RL_HS       4       /* nak throttle; see 4.9 */
/* \brief 0-3 qTD 重试; 0 为不停止 */
#define USBH_EHCI_TUNE_CERR        3
#define USBH_EHCI_TUNE_RL_TT       0
/* \brief 1-3事务每微帧 */
#define USBH_EHCI_TUNE_MULT_HS     1
/* \brief 端点每一微帧产生一个事务*/
#define USBH_EHCI_TUNE_MULT_TT     1

/* \brief 获取下一个数据结构的类型 */
#define Q_NEXT_TYPE(data)   ((data) & USB_CPU_TO_LE32(3 << 1))

/* \brief 从  USB 主机获取  EHCI 控制器*/
#define USBH_GET_EHCI_FROM_HC(hc) ((struct usbh_ehci *)(hc->p_controller))

/* \brief EHCI 数据结构内存配置结构体*/
struct usbh_ehci_mem_cfg {
    uint8_t      mem_type;   /* 内存类型*/
    uint32_t     nqhs;       /* QH 数量*/
    uint32_t     nqtds;      /* QTD 数量*/
    uint32_t     nitds;      /* ITD 数量*/
    uint32_t     nsitds;     /* SITD 数量*/
};

/* \brief EHCI 事务转换器*/
struct usbh_ehci_tt {

};

/* \brief EHCI 结构体*/
struct usbh_ehci {
    struct usb_hc_head          hc_head;
    uint32_t                    cap_reg;               /* USB 性能寄存器*/
    uint32_t                    opt_reg;               /* EHCI 操作寄存器*/

    uint8_t                     mem_type;              /* 内存类型*/
    struct usb_pool             qh_pool;               /* QH(队列头)内存池*/
    struct usb_pool             qtd_pool;              /* qtd(队列传输描述符)内存池*/
    struct usb_pool             itd_pool;              /* itd(同步传输描述符)内存池*/
    struct usb_pool             sitd_pool;             /* sitd(分割事务同步描述符)内存池*/
    uint32_t                    ds_size;               /* 数据结构大小*/

    uint32_t                    isoc_count;            /* 等时调度激活的次数*/
    uint32_t                    async_count;           /* 异步调度激活的次数*/
    uint32_t                    intr_count;            /* 中断传输激活的次数*/

    uint32_t                    i_thresh;
    int                         bd_width_allocated;
    int                         u_frame_next;          /* 下一微帧索引*/
    uint32_t                    frame_now;             /* 目前的帧索引*/
    uint32_t                    iso_frame_last;        /* 周期扫描的最新帧索引*/
#if USB_OS_EN
    usb_sem_handle_t            p_sem;                 /* EHCI 信号量*/
    usb_mutex_handle_t          p_lock;                /* EHCI 互斥锁*/
    usb_task_handle_t           p_task;                /* EHCI 任务*/
#endif
    uint8_t                     evt;                   /* EHCI事件*/

    uint32_t                    hcs;                   /* 主机控制器结构体参数*/
    usb_bool_t                  ppcd;                  /* 是否支持端口变化检测*/
    uint8_t                     port_speed_bit;        /* 端口速度位*/
    uint8_t                     n_ports;               /* 端口数量*/
    uint32_t                    status;                /* EHCI 状态*/

    struct usbh_ehci_qh        *p_async;               /* 异步调度*/
    uint32_t                   *p_periodic;            /* 周期帧列表，用来存放数据结构的地址*/
    union usbh_ehci_struct_ptr *p_shadow;              /* 硬件周期表镜像，用来存放周期帧列表中地址对应的数据结构*/
    uint32_t                    frame_list_size;       /* 帧列表大小*/
    uint8_t                     BdWidth[USBH_EHCI_BANDWIDTH_SIZE];   /* 总线带宽*/
    int                         random;

    struct usb_list_head        intr_qh_list;          /* 中断 QH */
    struct usb_list_head        trp_done_list;         /* 完成的请求包列表*/
#if USB_OS_EN
    usb_mutex_handle_t          p_trp_done_lock;
#endif
    /* 根集线器，第31位是复位变化状态位，第30位是挂起变化状态位，29~0位和USB端口状态控制寄存器值一样*/
    uint32_t                   *p_psta;                /* 储存端口状态寄存器的值*/
};

/* \brief EHCI FSTN */
struct usbh_ehci_fstn {
    uint32_t                   hw_next;
    union usbh_ehci_struct_ptr p_next;
};

/**
 * \brief EHCI 中断处理函数
 */
int usbh_ehci_irq_handle(struct usbh_ehci *p_ehci);
/**
 * \brief EHCI 守护工作
 */
void usbh_ehci_guard_work(struct usbh_ehci *p_ehci);
/**
 * \brief USB EHCI 初始化函数
 *
 * \param[in]  p_hc           USB 主机结构体
 * \param[in]  reg_base       USB 性能寄存器基地址
 * \param[in]  task_prio      EHCI 任务优先级
 * \param[in]  port_speed_bit 端口速度位
 * \param[in]  mem_cfg        内存配置
 * \param[out] p_ehci         成功返回的 EHCI 结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_create(struct usb_hc           *p_hc,
                     uint32_t                 reg_base,
                     int                      task_prio,
                     uint8_t                  port_speed_bit,
                     struct usbh_ehci_mem_cfg mem_cfg,
                     struct usbh_ehci       **p_ehci);
/**
 * \brief 销毁 EHCI 控制器
 *
 * \param[in] p_hc   USB 主机结构体
 * \param[in] p_ehci EHCI 结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_destory(struct usb_hc *p_hc, struct usbh_ehci *p_ehci);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif /* __USBH_EHCI_H */
