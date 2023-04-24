/*******************************************************************************
*                                    ZLG
*                         ----------------------------
*                         innovating embedded platform
*
* Copyright (c) 2001-2021 Guangzhou ZHIYUAN Electronics Co., Ltd.
* All rights reserved.
*
* Contact information:
* web site:    https://www.zlg.cn
*******************************************************************************/
#ifndef __USBH_EHCI_XFER_H
#define __USBH_EHCI_XFER_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "common/list/usb_list.h"

#define USB_HOST_DELAY                5
#define BitTime(bytecount)           (7 * 8 * bytecount / 6)
#define NS_TO_US(ns)                  USB_DIV_ROUND_UP(ns, 1000L)
#define HS_NSECS(bytes)            (((55 * 8 * 2083)/1000) + ((2083UL * (3167 + BitTime (bytes)))/1000) + USB_HOST_DELAY)
#define HS_NSECS_ISO(bytes)        (((38 * 8 * 2083) + (2083UL * (3 + BitTime(bytes)))) / 1000 + USB_HOST_DELAY)
#define HS_USECS(bytes)               NS_TO_US(HS_NSECS(bytes))
#define HS_USECS_ISO(bytes)           NS_TO_US(HS_NSECS_ISO(bytes))

/* \brief 高速端点描述符中编码的高带宽乘法器*/
#define hb_mult(max_packet_size)     (1 + (((max_packet_size) >> 11) & 0x03))
/* \brief 包大小，用于任何类型的端点描述符*/
#define max_packet(max_packet_size) ((max_packet_size) & 0x07ff)

/* \brief 全/低速带宽分配常数/支持*/
#define BW_HOST_DELAY   1000L       /* 纳秒*/
#define BW_HUB_LS_SETUP 333L        /* 纳秒*/

#define EHCI_ITD_ACTIVE              (1 << 31)                /* 这个插槽激活传输中 */
#define EHCI_ITD_BUF_ERR             (1 << 30)                /* 数据缓存错误*/
#define EHCI_ITD_BABBLE              (1 << 29)                /* babble 检测*/
#define EHCI_ITD_XACTERR             (1 << 28)                /* XactErr - 传输错误*/
#define EHCI_ITD_LENGTH(tok)       (((tok) >> 16) & 0x0fff)
#define EHCI_ITD_IOC                 (1 << 15)                /* 完成中断*/

#define EHCI_SITD_IOC                (1 << 31)               /* 完成中断*/
#define EHCI_SITD_PAGE               (1 << 30)               /* 缓存 0/1 */
#define EHCI_SITD_LENGTH(tok)        (0x3ff & ((tok)>>16))
#define EHCI_SITD_STS_ACTIVE         (1 << 7)                /* 激活*/
#define EHCI_SITD_STS_ERR            (1 << 6)                /* 事物转换器错误*/
#define EHCI_SITD_STS_DBE            (1 << 5)                /* 数据缓存错误(主机)*/
#define EHCI_SITD_STS_BABBLE         (1 << 4)                /* device was babbling */
#define EHCI_SITD_STS_XACT           (1 << 3)                /* 非法的输入相应*/
#define EHCI_SITD_STS_MMF            (1 << 2)                /* 未完成分割事务*/
#define EHCI_SITD_STS_STS            (1 << 1)                /* 分割事务状态*/

/* \brief 队列头状态*/
enum {
    __QH_ST_IDLE = 0,  /* 空闲(无数据)状态*/
    __QH_ST_LINKED,    /* 有数据已链接状态*/
    __QH_ST_UNLINKED,  /* 有数据未链接状态*/
};

/* \brief EHCI 数据结构联合体*/
union usbh_ehci_struct_ptr {
    struct usbh_ehci_qh    *p_qh;       /* QH(队列头)结构，Q_TYPE_QH */
    struct usbh_ehci_itd   *p_itd;      /* 等时传输描述符结构，Q_TYPE_ITD */
    struct usbh_ehci_sitd  *p_sitd;     /* 分割等时传输描述符结构， Q_TYPE_SITD */
    struct usbh_ehci_fstn  *p_fstn;     /* FSTN结构，Q_TYPE_FSTN */
    uint32_t               *p_hw_next;  /* 所有类型适用*/
    void                   *ptr;
};

/**
 * \brief QH(队列头)结构体
 *
 * hw_next[31: 5]     队列头水平链接指针，存放下一个数据结构的地址
 * hw_next[ 2: 1]     指明下一个数据结构的类型
 *                    00b ITD
 *                    01b QH
 *                    10b sITD
 *                    11b FSTN
 * hw_next[0]         这个为仅用于QH在周期列表中，1表示这是周期帧列表的末尾
 *                    当QH位于异步调度中时，这个为会被忽略
 *
 * hw_info1[31 : 28]  重载Nak计数器字段
 * hw_info1[27]       控制端点标志(非高速控制端点设置为1，否则为0)
 * hw_info1[26 : 16]  最大包长度
 * hw_info1[15]
 * hw_info1[14]       数据翻转控制
 * hw_info1[13 : 12]  端点速度
 * hw_info1[11 :  8]  端点号
 * hw_info1[7]        请求主机控制器取消激活位(只用于周期调度的低/全速设备)
 * hw_info1[ 6 :  0]  设备地址
 *
 * hw_info2[31 : 30]  高带宽管道乘法器
 *                    01b 端点每一微帧产生一个事务
 *                    10b 端点每一微帧产生两个事务
 *                    11b 端点每一微帧产生三个事务
 * hw_info2[29 : 23]  端口号(用于低/全速设备的分割事务)
 * hw_info2[22 : 16]  集线器地址(用于低/全速设备的分割事务)
 * hw_info2[15 :  8]  分割完成掩码(uFrame C-mask，只用于周期调度的低/全速设备)
 *                    当 FRIMDEX 寄存器位解码到一个 uFrame C-mask 为 1 的位置
 *                    则这个队列头是事务执行的候选
 * hw_info2[ 7 : 0]   中断调度掩码(uFrame S-mask，如果是异步调度则为 0，中断周期调度为非 0 )
 *                    当 FRIMDEX 寄存器位解码到一个 uFrame S-mask 为 1 的位置
 *                    则这个队列头是事务执行的候选
 *
 * hw_cur_qtd[31 : 5] 指明当前正在被处理的qTD地址
 *
 * hw_token[31]       数据翻转位，
 */
struct usbh_ehci_qh {
    /* EHCI QH寄存器，于EHCI规范 3.3.6*/
    uint32_t                    hw_next;        /* 这个 QH 处理完成后的下一个数据结构*/
    uint32_t                    hw_info1;       /* QH 信息寄存器1*/
    uint32_t                    hw_info2;       /* QH 信息寄存器2*/
    uint32_t                    hw_cur_qtd;
    uint32_t                    hw_next_qtd;
    uint32_t                    hw_alt_next_qtd;
    uint32_t                    hw_token;
    uint32_t                    hw_buf[5];      /* 传输数据*/
    uint32_t                    hw_buf_high[5];

    /* 用于 EHCI 驱动*/
    union usbh_ehci_struct_ptr  p_next;         /* 下一个 EHCI 数据结构*/
    struct usb_list_head        qtds;           /* 当前 QH 下的 qtd 链表*/
    struct usbh_ehci_qtd       *p_dummy;        /* dummy */
    struct usbh_ehci           *p_ehci;         /* EHCI 控制器 */
    struct usbh_endpoint       *p_ep;           /* 所属端点*/
    uint8_t                     state;          /* 状态*/
    uint8_t                     xact_errs;      /* XactErr 重试次数*/

    /* 用于 EHCI 驱动中断传输及等时传输*/
    uint16_t                    frame_phase;    /* frame index */
    uint8_t                     u_frame_phase;  /* uframe index */
    uint16_t                    frame_period;   /* 帧周期*/
    uint8_t                     usecs;          /* 中断端点带宽*/
    uint8_t                     c_usecs;        /* 分割完成带宽*/
    struct usb_list_node        intr_node;      /* 中断包节点*/

};

/* \brief EHCI QTD(队列传输描述符) 结构体*/
struct usbh_ehci_qtd {
    /* EHCI qTD数据结构规范*/
    uint32_t                hw_next;       /* 指向下一个 qTD 的指针*/
    uint32_t                hw_alt_next;   /* Aletmate Next qTD Pointer*/
    uint32_t                hw_token;      /* 令牌*/
    uint32_t                hw_buf[5];     /* qTD 缓冲区页指针*/
    uint32_t                hw_buf_high[5];

    /* 用于 EHCI 驱动*/
    struct usb_list_node    node;          /* 当前qTD节点*/
    struct usbh_trp        *p_trp;         /* 传输请求包 */
    size_t                  len;           /* 当前 qTD 数据长度*/
};

/* \brief EHCI ITD(等时传输描述符)结构体*/
struct usbh_ehci_itd {
    uint32_t                     hw_next;
    uint32_t                     hw_transaction[8];      /* 等时传输描述符事务状态和控制列表(EHCI 3.3.2)*/
    uint32_t                     hw_bufp[7];             /* 等时传输描述符缓存页指针链表(EHCI 3.3.3)*/
    uint32_t                     hw_bufp_hi[7];          /* 64位版本的数据结构(附录 B)*/
    union usbh_ehci_struct_ptr   p_next;                 /* 下一个数据结构地址 */
    uint32_t                     frame;                  /* 从哪里调度*/
    struct usb_list_node         node;                   /* 等时传输描述符节点*/
    struct usbh_trp             *p_trp;                  /* USB 传输请求包*/
    struct usbh_ehci_iso_stream *p_stream;               /* 等时传输数据流*/
    uint32_t                     index[8];               /* 在 trp->p_iso_frame_desc 的索引*/
    uint32_t                     page;                   /* 页选择*/
    uint8_t                      u_secs[8];
};

/**
 * \brief EHCI SITD(分割等时传输描述符)结构体
 *
 * hw_full_speed_ep[22 : 16] 事务传输集线器的地址
 * hw_full_speed_ep[11 :  8] 端点地址
 * hw_full_speed_ep[ 6 :  0] 设备地址
 *
 * hw_u_frame[15 : 8]        分割完成掩码(uFrame C-Mask)
 *                           当 FRIMDEX 寄存器位解码到一个 uFrame C-mask 为 1 的位置
 *                           则这个分割等时传输描述符是事务执行的候选
 * hw_u_frame[ 7 : 0]        分割起始掩码(uFrame S-Mask)
 *                           当 FRIMDEX 寄存器位解码到一个 uFrame S-mask 为 1 的位置
 *                           则这个分割等时传输描述符是事务执行的候选
 *
 * hw_results[31]            完成中断
 * hw_results[30]            数据缓存页指针选择
 * hw_results[25 : 16]       要传输的全部字节
 * hw_results[15 :  8]       记录被执行的分割完成事务
 * hw_results[ 7 :  0]       记录被执行的事务的状态
 *                           7 Active
 *                           6 ERR
 *                           5 Data Buffer Error
 *                           4 Babble Detected
 *                           3 Transaction Error
 *                           2 Missed Micro-Frame
 *                           1 Split Transaction State(0b Do Start Split, 1b Do Complete Split)
 */
struct usbh_ehci_sitd {
    uint32_t                     hw_next;
    uint32_t                     hw_full_speed_ep;
    uint32_t                     hw_u_frame;
    uint32_t                     hw_results;
    uint32_t                     hw_buf[2];
    uint32_t                     hw_back_pointer;
    uint32_t                     HwBufHi [2];

    union usbh_ehci_struct_ptr   p_next;         /* 下一个数据结构地址 */
    struct usbh_trp             *p_trp;
    struct usbh_ehci_iso_stream *p_stream;       /* 端点队列*/
    struct usb_list_node         node;           /* 数据流的分割等时传输描述符节点*/
    uint32_t                     frame;          /* 调度位置*/
    uint32_t                     index;
};

/* \brief 一个等时传输事务的描述符(如果是高速最多 3KB)*/
struct usbh_ehci_iso_packet {
    /* These will be copied to iTD when scheduling */
    uint64_t         bufp;               /* itd->hw_bufp{,_hi}[pg] |= */
    uint32_t         transaction;        /* itd->hw_transaction[i] |= */
    uint8_t          cross;              /* 缓存跨页*/
    /* 用于全速输出分割*/
    uint32_t         buf1;
};

/* \brief 来自等时 USB 请求块的数据包临时调度数据(两种速度)。每个数据包是设备的
 *        一个逻辑 USB 事务(不是分割事务)，从 stream->uframe_next 开始*/
struct usbh_ehci_iso_sched {
    struct usb_list_head         td_list;         /* 传输描述符链表*/
    unsigned                     span;            /* 这次调度在帧周期中的帧/微帧跨度*/
    unsigned                     first_packet;
    struct usbh_ehci_iso_packet *p_packet;
};

/* \brief 为分割事务的高速设备和全/低速设备的周期传输调度和预算信息*/
struct usbh_ehci_per_sched {
    struct usbh_device    *p_usb_dev;       /* 访问事务转换器*/
    struct usbh_endpoint  *p_ep;

    uint16_t               cs_mask;         /* C-mask和 S-mask 字节*/
    uint16_t               phase;           /* 真实时期，帧部分*/
    uint8_t                bw_phase;        /* 一样，为带宽保留*/
    uint8_t                phase_uf;        /* uframe part of the phase */
    uint8_t                usecs, c_usecs;  /* 高速总线上的时间*/
    uint8_t                bw_uperiod;      /* 带宽预留的微帧周期*/
    uint16_t               period;          /* 帧的真实周期*/
    uint8_t                bw_period;       /* 带宽帧周期*/
};

/* \brief EHCI 等时流结构体 */
struct usbh_ehci_iso_stream {
    struct usbh_ehci_qh       *p_hw;
    struct usbh_device        *p_usb_dev;         /* 访问事务转换器*/
#if USB_OS_EN
    usb_mutex_handle_t         p_lock;
#endif
    struct usb_list_head       td_list;           /* 入队列的等时传输描述符/分割事务等时传输描述符*/
    struct usb_list_head       free_list;         /* 空闲的等时传输描述符/分割事务等时传输描述符*/
    uint32_t                   uframe_next;

    usb_bool_t                 is_release;        /* 是否释放*/
    uint8_t                    ep_addr;           /* 端点地址*/
    uint8_t                    highspeed;         /* 高速传输*/
    uint16_t                   uperiod;           /* 微帧周期*/
    uint32_t                   band_width;        /* 带宽*/
    uint16_t                   maxp;              /* 最大包大小*/
    uint32_t                   splits;            /* 分割事务标志*/
    int                        ref_cnt;           /* 2.6*/
    struct usbh_endpoint      *p_ep;              /* 2.6*/
    uint8_t                    usecs, c_usecs;    /* 2.6*/
    uint16_t                   cs_mask;           /* 2.6*/
    uint8_t                    interval;          /* 2.6 微帧周期*/
    uint8_t                    f_interval;        /* 2.6 帧周期*/
    uint16_t                   depth;             /* 2.6*/
    uint16_t                   tt_usecs;          /* 2.6 在全速/低速总线上的时间*/

    struct usbh_ehci_per_sched ps;                /* 调度信息 4.x.y*/

    /* 这是用于初始化等时传输描述符的 hw_bufp 字段*/
    uint32_t                   buf0;
    uint32_t                   buf1;
    uint32_t                   buf2;

    /* 用于初始化分割等时传输描述符的事物转换器信息*/
    uint32_t                   address;
};









#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif /* __USBH_EHCI_XFER_H */
