#ifndef __USBH_NET_H
#define __USBH_NET_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "common/list/usb_list.h"
#include "common/refcnt/usb_refcnt.h"
#include "core/include/host/core/usbh.h"
#include "core/include/host/core/usbh_dev.h"
#include "core/include/specs/usb_cdc_specs.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef USB_OS_EN
#define USB_NET_LOCK_TIMEOUT               5000
#endif

#define USB_NET_HWADDR_MAX_LEN             6

#define USB_NET_FRAMING_RN                 0x0008          /* RNDIS batches, plus huge header */
#define USB_NET_NO_SETINTF                 0x0010          /* 设备不能设置接口*/
#define USB_NET_ETHER                      0x0020          /* 以太网设备 */
#define USB_NET_WLAN                       0x0080          /* 无线局域网设备*/
#define USB_NET_WWAN                       0x0400          /* 无线广域网设备*/
#define USB_NET_POINTTOPOINT               0x1000          /* 点对点设备*/

#define USB_NET_DRV_FLAG_SEND_ZLP          0x0200          /* 硬件需要发送0长度包*/
#define USB_NET_DRV_FLAG_MULTI_PACKET      0x2000          /* 指明 USB 驱动程序积累多个 IP 包*/
#define USB_NET_DRV_FLAG_AVOID_UNLINK_TRPS 0x0100          /* don't unlink urbs at usbnet_stop() */

#define USB_NET_EVENT_TX_HALT             (1 << 0)
#define USB_NET_EVENT_RX_HALT             (1 << 1)
#define USB_NET_EVENT_RX_MEMORY           (1 << 2)
#define USB_NET_EVENT_STS_SPLIT           (1 << 3)
#define USB_NET_EVENT_LINK_RESET          (1 << 4)
#define USB_NET_EVENT_RX_PAUSED           (1 << 5)
#define USB_NET_EVENT_DEV_ASLEEP          (1 << 6)
#define USB_NET_EVENT_DEV_OPEN            (1 << 7)
#define USB_NET_EVENT_DEVICE_REPORT_IDLE  (1 << 8)
#define USB_NET_EVENT_NO_RUNTIME_PM       (1 << 9)
#define USB_NET_EVENT_RX_KILL             (1 << 10)
#define USB_NET_EVENT_LINK_CHANGE         (1 << 11)
#define USB_NET_EVENT_SET_RX_MODE         (1 << 12)

/* \brief USB 主机网络设备 VID 获取 */
#define USBH_NET_DEV_VID_GET(p_net)            USBH_DEV_VID_GET(p_net->p_usb_fun)
/* \brief USB 主机网络设备 PID 获取 */
#define USBH_NET_DEV_PID_GET(p_net)            USBH_DEV_PID_GET(p_net->p_usb_fun)

/* \brief USB 主机网络驱动库结构体 */
struct usbh_net_lib {
    struct usb_lib_base lib;               /* USB 库*/
    usb_bool_t          is_lib_deiniting;  /* 是否移除库*/
    uint32_t            queue_max;
    int                 ref_cnt;           /* 引用计数*/
};

/* \brief USB CDC 基础设施的一些标准的驱动程序（特别是根据CDC联合描述符
 *        使用多个接口）会得到一些帮助程序代码 */
struct usbh_cdc_state {
    struct usb_cdc_header_desc *p_hdr;        /* CDC 头描述符*/
    struct usb_cdc_union_desc  *p_u;          /* CDC 联合描述符*/
    struct usb_cdc_ether_desc  *p_ether;      /* CDC 以太网描述符*/
    struct usbh_interface      *p_control;    /* 控制接口*/
    struct usbh_interface      *p_data;       /* 数据接口*/
};

/* \brief USB 主机网络传输请求包头结构体 */
struct usbh_net_trp_hdr {
    struct usb_list_head trp_free;
    struct usb_list_head trp_start;
    uint32_t             n_trp_start;
    uint32_t             n_trp_total;
};

/* \brief USB 主机网络传输请求包结构体 */
struct usbh_net_trp {
    struct usbh_trp      trp;
    struct usb_list_node node;
};

/* \brief USB 主机网络接收缓存结构体 */
struct usbh_net_rx_buf {
    uint8_t  *p_buf;
    uint32_t  buf_size;
    uint32_t  data_pos;          /* 数据位置*/
    uint32_t  rd_pos;            /* 已读位置*/
};

struct usbh_net;
/* \brief USB 主机网络驱动信息 */
struct usbh_net_drv_info {
    char                 *p_desc;               /* 描述信息*/
    int                   flags;
    unsigned long         data;                 /* 混杂驱动特定数据 */
    /* 对于新设备，使用描述符读取代码*/
    struct usbh_endpoint *p_ep_in;              /* 输入端点*/
    struct usbh_endpoint *p_ep_out;             /* 输出端点*/

    /* 绑定设备 */
    int                 (*p_fn_bind)(struct usbh_net *p_net);
    /* 解除绑定设备 */
    int                 (*p_fn_unbind)(struct usbh_net *p_net);
    /* 状态轮询 */
    void                (*p_fn_status)(struct usbh_net *p_net, struct usbh_trp *p_trp);
    /* 复位设备 */
    int                 (*p_fn_reset)(struct usbh_net *p_net);
    /* 接收模式改变(设备改变地址列表滤波器) */
    int                 (*p_fn_rx_mode_set)(struct usbh_net *p_net);
    /* 激活/禁用运行时电源管理*/
    int                 (*p_fn_power_manage)(struct usbh_net *p_net, usb_bool_t is_on);
    /* 查看对等方是否已连接*/
    int                 (*p_fn_connect_chk)(struct usbh_net *p_net);
    /* 链接复位处理*/
    int                 (*p_fn_link_reset)(struct usbh_net *p_net);
    /* 关闭设备*/
    int                 (*p_fn_stop)(struct usbh_net *p_net);
    int                 (*p_fn_early_init)(struct usbh_net *p_net);
    void                (*p_fn_indication)(struct usbh_net *p_net, void *p_ind, uint32_t ind_len);

    int                 (*p_fn_rx_fixup)(struct usbh_net *p_net, uint8_t *p_buf, uint32_t buf_len);
    uint8_t            *(*p_fn_tx_fixup)(struct usbh_net *p_net, uint8_t *p_buf, uint32_t buf_len);
//#define FLAG_FRAMING_NC 0x0001      /* guard against device dropouts */
//#define FLAG_FRAMING_GL 0x0002      /* genelink batches packets */
//#define FLAG_FRAMING_Z  0x0004      /* zaurus adds a trailer */
//#define FLAG_FRAMING_RN 0x0008      /* RNDIS batches, plus huge header */
//
//#define FLAG_NO_SETINT  0x0010             /* 设备不能设置接口*/
//
//#define FLAG_FRAMING_AX 0x0040             /* AX88772/178 包 */
//#define FLAG_AVOID_UNLINK_URBS 0x0100      /* don't unlink urbs at usbnet_stop() */
//#define FLAG_SEND_ZLP   0x0200             /* 硬件需要发送0长度包*/
//
//#define FLAG_LINK_INTR  0x0800             /* updates link (carrier) status */
//
//#define FLAG_POINTTOPOINT 0x1000           /* 点对点设备*/
//
//#define FLAG_MULTI_PACKET   0x2000         /* 指明USB驱动程序积累多个IP包*/
//#define FLAG_RX_ASSEMBLE    0x4000  /* rx packets may span >1 frames */
//#define FLAG_NOARP      0x8000  /* device can't do ARP */

//    /* 修复接收包*/
//    int (*rx_fixup)(struct awbl_usbh_usbnet *dev, struct aw_sk_buff *skb);
//
};

/* \brief USB 主机网络结构体 */
struct usbh_net {
    struct usbh_function           *p_usb_fun;
    struct usbh_config             *p_cfg;                   /* 当前使用的 USB 设备的配置*/
    struct usbh_interface          *p_intf;
    uint32_t                        dev_type;                /* 设备类型*/
    void                           *p_lock;
    usb_bool_t                      is_removed;
    uint16_t                        hard_header_len;
    uint16_t                        rx_qlen, tx_qlen;        /* 发送/接收队列长度*/
    uint8_t                         dev_addr[USB_NET_HWADDR_MAX_LEN];
    char                            name[USB_NAME_LEN];
    const struct usbh_net_drv_info *p_drv_info;
    int                             ref_cnt;
    uint32_t                        event_flags;             /* 时间标志*/
    uint32_t                        max_packet;              /* 最大包大小*/
    uint32_t                        rx_trp_size;             /* 接收传输请求包大小*/
    uint32_t                        mtu;                     /* 最大传输单元*/
    uint32_t                        hard_mtu;                /* 硬件最大传输单元，计数任何额外的帧*/
    uint32_t                        xid;
    uint32_t                        trans_start;             /* 记录传输启动时间*/
    struct usbh_net_rx_buf          rx_buf;
    struct usbh_endpoint           *p_ep_in;
    struct usbh_endpoint           *p_ep_out;
    struct usbh_endpoint           *p_ep_status;             /* 状态端点*/
    struct usbh_trp                 trp_int;                 /* 中断传输请求包*/
    struct usbh_trp                 trp_tx;
    unsigned long                   data[5];
    struct usb_timespec             trans_start_ts;
    struct usbh_net_trp_hdr         rx_trp_hdr;
    struct usb_list_node            node;


//    AW_MUTEX_DECL(interrupt_mutex);             /* 中断互斥锁*/
//    AW_MUTEX_DECL(phy_mutex);
//    uint32_t interrupt_count;                   /* 中断请求计数*/
//    aw_timer_t delay;                           /* 延时定时器*/

//
//    struct awbl_usbh_usbnet_trp_head rxq;       /* 接收队列*/
//    uint16_t  rx_qlen, tx_qlen;                 /* 发送/接收队列长度*/
//
//    uint8_t pkt_cnt, pkt_err;                   /* 包次数，包错误*/
//    aw_usb_task_handle_t bh;                    /* 处理接收包任务*/
//    aw_usb_task_handle_t kevent;                /* 事件任务*/
//    aw_usb_sem_handle_t bh_sem;                 /* 信号量*/
//    uint32_t net_flags;                         /* 网络相关的标志位*/
//    struct aw_pool              trp_pool;       /* USB传输请求包内存池*/
//
//
//    uint32_t trans_start;                       /* 记录传输启动时间*/
//
//    /* 以下成员只是暂时定义*/
//    ////////////////////////////////////////////////////////////////////////////////
//    struct awbl_usbh_usbnet_device_stats stats; /* 设备状态*/
    ///////////////////////////////////////////////////////////////////////////////
};

/**
 * \brief USB 主机网络设备端点获取
 *
 * \param[in] p_net  USB 主机网络结构体
 * \param[in] p_intf USB 接口结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_endpoints_get(struct usbh_net       *p_net,
                           struct usbh_interface *p_intf);
/**
 * \brief USB 主机网络设备以太网地址获取
 *
 * \param[in] p_net       USB 主机网络设备
 * \param[in] mac_address 字符串索引
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_ethernet_addr_get(struct usbh_net *p_net, int mac_address);
/**
 * \brief USB 主机网络设备链接改变函数
 *
 * \param[in] p_net    USB 主机网络设备
 * \param[in] is_link  是否连接
 * \param[in] is_reset 是否复位
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_link_change(struct usbh_net *p_net,
                         usb_bool_t       is_link,
                         usb_bool_t       is_reset);
/**
 * \brief USB 主机网络设备创建
 *
 * \param[in] p_usb_fun  USB 功能接口结构体
 * \param[in] p_name     USB 主机网络设备名字
 * \param[in] p_drv_info USB 网络驱动信息
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_create(struct usbh_function           *p_usb_fun,
                    char                           *p_name,
                    const struct usbh_net_drv_info *p_drv_info);
/**
 * \brief USB 主机网络设备销毁
 *
 * \param[in] p_usb_fun USB 功能接口结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_destroy(struct usbh_function *p_usb_fun);
/**
 * \brief USB 主机网络设备打开函数
 *
 * \param[in]  p_handle  打开句柄
 * \param[in]  flag      打开标志，本接口支持两种打开方式：
 *                       USBH_DEV_OPEN_BY_NAME 是通过名字打开设备
 *                       USBH_DEV_OPEN_BY_UFUN 是通过 USB 功能结构体打开设备
 * \param[out] p_net_ret 成功返回 USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_open(void             *p_handle,
                  uint8_t           flag,
                  struct usbh_net **p_net_ret);
/**
 * \brief USB 主机网络设备关闭函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_close(struct usbh_net *p_net);
/**
 * \brief USB 主机网络设备启动函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_start(struct usbh_net *p_net);
/**
 * \brief USB 主机网络设备停止函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_stop(struct usbh_net *p_net);
/**
 * \brief USB 主机网络设备写函数
 *
 * \param[in] p_net   USB 主机网络设备
 * \param[in] p_buf   写缓存
 * \param[in] buf_len 写缓存长度
 *
 * \retval 成功返回实际写的长度
 */
int usbh_net_write(struct usbh_net *p_net, uint8_t *p_buf, uint32_t buf_len);
/**
 * \brief USB 主机网络设备读函数
 *
 * \param[in] p_net   USB 主机网络设备
 * \param[in] p_buf   读缓存
 * \param[in] buf_len 读缓存长度
 *
 * \retval 成功返回读到的长度
 */
int usbh_net_read(struct usbh_net *p_net,
                  uint8_t         *p_buf,
                  uint32_t         buf_len);
/**
 * \brief USB 主机网络设备接收缓存大小获取
 *
 * \param[in]  p_net  USB 主机网络设备
 * \param[out] p_size 返回的缓存大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_rx_buf_size_get(struct usbh_net *p_net, uint32_t *p_size);
/**
 * \brief USB 主机网络设备接收缓存大小获取
 *
 * \param[in] p_net USB 主机网络设备
 * \param[in] size  要设置的缓存大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_rx_buf_size_set(struct usbh_net *p_net, uint32_t size);
/**
 * \brief USB 主机网络设备处理函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_process(struct usbh_net *p_net);

/**
 * \brief USB 主机网络设备事件处理函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_event_prosess(struct usbh_net *p_net);

/**
 * \brief 获取当前存在的 USB 网络设备数量
 *
 * \param[out] p_n_dev 存储返回的设备数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_lib_ndev_get(uint8_t *p_n_dev);
/**
 * \brief USB 主机网络驱动库初始化
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_lib_init(void);
/**
 * \brief 反初始化 USB 主机网络驱动库
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_lib_deinit(void);
#if USB_MEM_RECORD_EN
/**
 * \brief 获取 USB 主机网络驱动内存记录
 *
 * \param[out] p_mem_record 内存记录
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_lib_mem_record_get(struct usb_mem_record *p_mem_record);
#endif
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif /* __USBH_NET_H */
