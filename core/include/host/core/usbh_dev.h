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
#ifndef __USBH_DEV_H
#define __USBH_DEV_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "common/list/usb_list.h"
#include "adapter/usb_adapter.h"
#include "core/include/specs/usb_specs.h"
#include <string.h>
#include <stdio.h>

#if USB_OS_EN
/* \brief USB主机设备互斥锁超时时间*/
#define USBH_DEV_MUTEX_TIMEOUT 5000
#endif

/* \brief USB设备最大配置数量*/
#define USBH_CONFIG_MAX       5
/* \brief USB设备最大接口数量*/
#define USBH_MAX_INTERFACES   32
/* \brief USB设备最大端点数目*/
#define USBH_MAX_EP_NUM       32

/* \brief 设备枚举状态*/
#define USBH_DEV_IGONRE_PID  0        /* 忽略使用 PID*/
#define USBH_DEV_IGONRE_VID  0        /* 忽略使用 VID*/
#define USBH_DEV_INJECT      0x01     /* 设备插入标志*/
#define USBH_DEV_EJECT       0x02     /* 设备拔出标志*/
#define USBH_DEV_EXIST       0x04     /* 设备当前是否存在标志*/
#define USBH_DEV_CFG         0x08     /* 设备已配置状态*/

/* \brief 判断设备是否是插入状态*/
#define USBH_IS_DEV_INJECT(p_dev)  ((p_dev)->status & USBH_DEV_INJECT)

#define USBH_TRP_SHORT_NOT_OK  0x00000001    /* 短包错误*/
#define USBH_TRP_ISO_ASAP      0x00000002    /* Start ISO transfer at the earliest */
#define USBH_TRP_ZERO_PACKET   0x00000004    /* 用 0 包结束批量传输*/
#define USBH_TRP_NO_INTERRUPT  0x00000008    /* 无需中断，除非传输错误*/

/* \brief 获取端点类型*/
#define USBH_EP_TYPE_GET(p_ep)             ((p_ep)->p_ep_desc->attributes & 0x03)
/* \brief 获取端点方向*/
#define USBH_EP_DIR_GET(p_ep)              ((p_ep)->p_ep_desc->endpoint_address & 0x80)
/* \brief 获取端点最大包大小*/
#define USBH_EP_MPS_GET(p_ep)              (USB_CPU_TO_LE16((p_ep)->p_ep_desc->max_packet_size))
/* \brief 获取端点地址*/
#define USBH_EP_ADDR_GET(p_ep)             ((p_ep)->p_ep_desc->endpoint_address & 0x0F)
/* \brief 获取端点最大包大小*/
#define USBH_EP_MPS_GET(p_ep)              (USB_CPU_TO_LE16((p_ep)->p_ep_desc->max_packet_size))
/* \brief 获取端点轮询周期*/
#define USBH_EP_INTVAL_GET(p_ep)           ((p_ep)->p_ep_desc->interval)

/* \brief 获取设备名*/
#define USBH_DEV_NAME_GET(p_fun)           ((p_fun)->p_usb_dev->name)
/* \brief 获取版本*/
#define USBH_DEV_BCD_GET(p_fun)            (USB_CPU_TO_LE16((p_fun)->p_usb_dev->p_dev_desc->bcdUSB))
/* \brief 获取设备 VID*/
#define USBH_DEV_VID_GET(p_fun)            (USB_CPU_TO_LE16((p_fun)->p_usb_dev->p_dev_desc->id_vendor))
/* \brief 获取设备 PID*/
#define USBH_DEV_PID_GET(p_fun)            (USB_CPU_TO_LE16((p_fun)->p_usb_dev->p_dev_desc->id_product))
/* \brief 获取设备类*/
#define USBH_DEV_CLASS_GET(p_fun)          ((p_fun)->p_usb_dev->p_dev_desc->device_class)
/* \brief 获取设备子类*/
#define USBH_DEV_SUB_CLASS_GET(p_fun)      ((p_fun)->p_usb_dev->p_dev_desc->device_sub_class)
/* \brief 获取设备协议*/
#define USBH_DEV_PROTOCOL_GET(p_fun)       ((p_fun)->p_usb_dev->p_dev_desc->device_protocol)
/* \brief 获取设备端点 0 最大包大小*/
#define USBH_DEV_MPS0_GET(p_fun)           ((p_fun)->p_usb_dev->p_dev_desc->max_packet_size0)
/* \brief 获取设备地址*/
#define USBH_DEV_ADDR_GET(p_fun)           ((p_fun)->p_usb_dev->addr)
/* \brief 获取设备产商描述字符串*/
#define USBH_DEV_PDTSTR_GET(p_fun)         ((p_fun)->p_usb_dev->p_pdt)
/* \brief 获取设备速度*/
#define USBH_DEV_SPEED_GET(p_fun)          ((p_fun)->p_usb_dev->speed)

/* \brief 获取功能第一个接口号*/
#define USBH_FUNC_FIRST_INTFNUM_GET(p_fun) ((p_fun)->first_intf_num)
/* \brief 获取功能接口数量*/
#define USBH_FUNC_NINTF_GET(p_fun)         ((p_fun)->n_intf)
/* \brief 获取功能接口协议*/
#define USBH_FUNC_PROTO_GET(p_fun)         ((p_fun)->proto)
/* \brief 获取功能接口类*/
#define USBH_FUNC_CLASS_GET(p_fun)         ((p_fun)->clss)
/* \brief 获取功能接口子类*/
#define USBH_FUNC_SUBCLASS_GET(p_fun)      ((p_fun)->sub_clss)
/* \brief 获取功能 USB 主机*/
#define USBH_FUNC_HC_GET(p_fun)            ((p_fun)->p_usb_dev->p_hc)

/* \brief 获取接口类*/
#define USBH_INTF_CLASS_GET(p_intf)        ((p_intf)->p_desc->interface_class)
/* \brief 获取接口子类*/
#define USBH_INTF_SUB_CLASS_GET(p_intf)    ((p_intf)->p_desc->interface_sub_class)
/* \brief 获取接口协议*/
#define USBH_INTF_PROTO_GET(p_intf)        ((p_intf)->p_desc->interface_protocol)
/* \brief 获取接口编号*/
#define USBH_INTF_NUM_GET(p_intf)          ((p_intf)->p_desc->interface_number)
/* \brief 获取接口备用设置编号*/
#define USBH_INTF_ALT_NUM_GET(p_intf)      ((p_intf)->p_desc->alternate_setting)
/* \brief 获取接口端点数量*/
#define USBH_INTF_NEP_GET(p_intf)          ((p_intf)->p_desc->num_endpoints)
/* \brief 获取接口第一个端点*/
#define USBH_INTF_FIRST_EP_GET(p_intf)     ((p_intf)->p_eps)

/* \brief USB 设备匹配标志 */
#define USB_DEV_PID_MATCH                  (1 << 0)
#define USB_DEV_VID_MATCH                  (1 << 1)
#define USB_INTF_CLASS_MATCH               (1 << 2)
#define USB_INTF_SUB_CLASS_MATCH           (1 << 3)
#define USB_INTF_PROCOTOL_MATCH            (1 << 4)
#define USB_DEV_MATCH_END                 \
        {                                 \
                .flags          = 0,      \
                .pid            = 0,      \
                .vid            = 0,      \
                .intf_class     = 0,      \
                .intf_sub_class = 0,      \
                .intf_protocol  = 0,      \
        }

/* \brief USB 设备类型 */
#define USBH_DEV_HUB      (1 << 0)  /* USB集线器设备*/
#define USBH_DEV_UMS      (1 << 1)  /* USB大容量存储设备*/
#define USBH_DEV_UVC      (1 << 2)  /* USB摄像头设备*/
#define USBH_DEV_UNIC     (1 << 4)  /* USB网卡*/
#define USBH_DEV_UHID     (1 << 5)  /* USB人体接口设备*/
#define USBH_DEV_UAC      (1 << 7)  /* USB音频设备*/
#define USBH_DEV_UNKNOWN  (1 << 8)  /* 未知类型*/

#define USBH_FUNC_UHUB    (1 << 0)     /* USB 集线器设备*/
#define USBH_FUNC_UMS     (1 << 1)     /* USB 大容量存储设备*/
#define USBH_FUNC_UVC     (1 << 2)     /* USB 视频类设备*/
#define USBH_FUNC_USERIAL (1 << 3)     /* USB 转串口设备*/
#define USBH_FUNC_UNIC    (1 << 4)     /* USB 网卡*/
#define USBH_FUNC_UHID    (1 << 5)     /* USB 人体接口设备*/
#define USBH_FUNC_UNKNOWN  0xFFFFFFFF  /* 未知类型*/

/* \brief USB设备打开及类型*/
enum{
    USBH_DEV_OPEN_BY_NAME = 0,
    USBH_DEV_OPEN_BY_UFUN,
};

/* \brief USB 接口匹配信息结构体 */
struct usb_dev_match_info {
    uint8_t  flags;
    uint16_t pid;
    uint16_t vid;
    uint8_t  intf_class;
    uint8_t  intf_sub_class;
    uint8_t  intf_protocol;
};

/* \brief USB 主机设备库结构体*/
struct usbh_dev_lib {
    struct usb_list_head  dev_list;        /* 设备链表*/
#if USB_OS_EN
    usb_mutex_handle_t    p_lock;          /* 互斥锁*/
    usb_mutex_handle_t    p_monitor_lock;  /* 监控器互斥锁*/
    usb_sem_handle_t      p_hub_evt_sem;   /* 集线器事件信号量*/
#endif
    usb_bool_t            is_lib_init;     /* 是否初始化库*/
    usb_bool_t            is_lib_deiniting;/* 是否移除库*/
    struct usb_list_head  monitor_list;    /* 监控器链表*/
    struct usb_list_head  hub_evt_list;    /* 集线器事件链表*/
    uint8_t               n_dev;           /* 当前存在设备的数量*/
    int                   ref_cnt;         /* 引用计数*/
    int                   xfer_time_out;   /* 设备传输超时时间*/
};

/* \brief USB 集线器事件结构体*/
struct usbh_hub_evt {
    struct usb_list_node evt_node;   /* 当前节点*/
    uint8_t              port_num;   /* 相关端口*/
    void                *p_evt_arg;  /* 事件参数*/
};

/* \brief USB设备监视结构体*/
struct usbh_dev_monitor {
    uint16_t             pid;                                            /* 监控的设备的 PID */
    uint16_t             vid;                                            /* 监控的设备的 VID */
    uint8_t              type;                                           /* 监控类型*/
    void               (*p_fn_usr_cb) (void *p_arg, uint8_t EvtType);    /* 用户回调函数*/
    void                *p_arg;                                          /* 回调函数参数*/
    struct usb_list_node node;                                           /* 节点*/
    uint32_t             dev_type;                                       /* 设备类型*/
    void                *p_job;                                          /* 用户数据*/
    uint8_t              state;                                          /* 状态*/
};

/* \brief USB 等时包描述符*/
struct usb_iso_pkt_desc {
    uint32_t  offset;     /* 偏移*/
    uint32_t  len;        /* 期望传输的长度*/
    uint32_t  act_len;    /* 实际传输的长度*/
    int       status;     /* 状态*/
};

/* \brief USB 传输请求包*/
struct usbh_trp {
    struct usbh_endpoint    *p_ep;                    /* 传输请求包相关端点*/
    struct usb_ctrlreq      *p_ctrl;                  /* SETUP包*/
    void                    *p_data;                  /* 数据缓存*/
    size_t                   len;                     /* 要传输的长度*/
    void                    *p_ctrl_dma;              /* SETUP包映射的DMA内存*/
    void                    *p_data_dma;              /* 数据过程映射的DMA内存*/
    void                   (*p_fn_done)(void *p_arg); /* 传输完成回调函数*/
    void                    *p_arg;                   /* 传输完成回调函数参数*/
    size_t                   act_len;                 /* 实际传输长度*/
    int                      status;                  /* 传输状态*/
    int                      flag;                    /* 传输标志*/
    void                    *p_usr_priv;              /* 用户私有数据*/
    int                      interval;                /* 传输时间周期(中断/等时)*/
    void                    *p_hc_priv;               /* 主机控制器的私有数据*/
    int                      err_cnt;                 /* (返回)等时传输的错误数量*/
    int                      start_frame;             /* 等时起始帧*/
    int                      n_iso_packets;           /* (输入)同步包的数量(add by CYX at 9/17-2019)*/
    struct usb_iso_pkt_desc *p_iso_frame_desc;        /* 等时包描述符(add by CYX at 9/17-2019)*/
    struct usb_list_node     node;                    /* 当前USB传输请求包的节点*/
};

/* \brief USB端点结构体*/
struct usbh_endpoint {
    struct usbh_device       *p_usb_dev;  /* 所属设备结构体*/
    struct usb_endpoint_desc *p_ep_desc;  /* USB端点描述符定义*/
    usb_bool_t                is_enabled; /* 使能标志位*/
    int                       band_width; /* 端点所需带宽*/
    int                       interval;   /* 传输时间周期(中断/等时)*/
    struct usb_list_node      Node;       /* 本端点结构体节点*/
    void                     *p_hc_priv;  /* 主机控制器私有数据域*/
    void                     *p_hw_priv;  /* 端点私有数据域*/
    int                       extra_len;  /* 额外的描述符的长度*/
    uint8_t                  *p_extra;    /* 额外的描述符(例如，特定类描述符或特定产商描述符) */
};

/* \brief USB 接口结构体*/
struct usbh_interface {
    struct usb_list_node        node;      /* 当前接口节点*/
    struct usb_interface_desc  *p_desc;    /* 接口描述符*/
    struct usbh_endpoint       *p_eps;     /* 接口下的端点*/
    int                         extra_len; /* 额外的描述符的长度*/
    uint8_t                    *p_extra;   /* 额外的描述符(例如，特定类描述符或特定产商描述符) */
};

/* \brief USB配置结构体*/
struct usbh_config {
    struct usb_config_desc  *p_desc;    /* 配置描述符*/
    char                    *p_string;  /* 配置描述*/
    struct usbh_function    *p_funs;    /* 接口功能结构体*/
    uint8_t                  n_funs;    /* 接口功能数*/
    int                      extra_len; /* 额外的描述符的长度*/
    uint8_t                 *p_extra;   /* 额外的描述符(例如，特定类描述符或特定产商描述符) */
};

/* \brief USB 设备结构体*/
struct usbh_device {
    struct usb_hc                  *p_hc;        /* USB主机结构体*/
    struct usbh_hub_basic          *p_hub_basic; /* 所属集线器*/
    uint8_t                         port;        /* 所属集线器端口号*/
    struct usb_ctrlreq             *p_ctrl;      /* 控制请求(SETUP)*/
    char                            name[32];    /* 设备名字*/
    uint8_t                         addr;        /* 设备地址*/
    uint8_t                         status;      /* 设备状态*/
    uint8_t                         speed;       /* 设备速度*/
    int                             ref_cnt;     /* 设备引用计数*/
    struct usb_device_desc         *p_dev_desc;  /* USB设备描述符定义*/
#if USB_OS_EN
    usb_mutex_handle_t              p_lock;      /* 设备互斥锁*/
#endif
    struct usb_list_node            node;        /* 当前设备结构体节点*/
    struct usbh_config              cfg;         /* USB设备的配置结构体*/
    struct usbh_endpoint            ep0;         /* 端点0*/
    struct usb_endpoint_desc        ep0_desc;    /* 端点0描述符定义*/
    struct usbh_endpoint           *p_ep_in[16]; /* 设备输入端点*/
    struct usbh_endpoint           *p_ep_out[16];/* 设备输出端点*/

    char                           *p_mft;       /* 设备制造商*/
    char                           *p_pdt;       /* 设备产商*/
    char                           *p_snum;      /* 设备序列号*/
    uint16_t                        lang_id;     /* 语言ID*/
    uint32_t                        quirks;      /* 设备兼容*/
    uint32_t                        dev_type;    /* 设备类型*/
    struct usbh_tt                 *p_tt;        /* 事务转换器(低/全速设备接到高速集线器)*/
    int                             tt_port;     /* 设备在事物转换器集线器的端口号*/
};

/* \brief USB功能结构体*/
struct usbh_function {
    char                  name[16];       /* 设备名字*/
    struct usbh_device   *p_usb_dev;      /* 相关的USB设备结构体*/
    void                 *p_usr_priv;     /* 用户私有信息*/

    uint8_t               first_intf_num; /* 第一个接口的编号*/
    uint8_t               n_intf;         /* 接口的数量*/
    uint8_t               type;           /* 接口类型（普通接口或联合接口）*/
    uint8_t               n_intf_type;    /* 接口类型的数量（普通接口该值为 1，联合接口该值为联合接口包含的接口类型数量）*/
    uint8_t               clss;           /* 类*/
    uint8_t               sub_clss;       /* 子类*/
    uint8_t               proto;          /* 协议*/
    uint32_t              func_type;      /* 功能类型*/

    struct usb_list_head  intf_list;      /* 接口链表*/
    void                 *p_drv_priv;     /* 驱动私有信息*/
};

/**
 * \brief USB 获取额外的描述符
 *
 * \param[in]  p_buf   数据缓存
 * \param[in]  size    数据缓存大小
 * \param[in]  type    要找的描述符类型
 * \param[out] p_desc  返回找到的描述符
 *
 * \retval 成功返回 USB_OK
 */
int usb_extra_desc_get(uint8_t  *p_buf,
                       uint32_t  size,
                       uint8_t   type,
                       void    **p_desc);
/**
 * \brief USB 设备功能匹配函数
 *
 * \param[in] p_usb_fun USB 功能接口结构体
 * \param[in] p_info    匹配信息
 *
 * \retval 匹配成功返回 USB_OK
 */
int usb_dev_func_match(struct usbh_function            *p_usb_fun,
                       const struct usb_dev_match_info *p_info);
/**
 * \brief USB 主机功能接口用户数据设备
 *
 * \param[in] p_fun      USB 主机功能接口
 * \param[in] p_usr_data 用户私有数据
 *
 * \retval 成功返回 USB_OK
 */
int usbh_func_usrdata_set(struct usbh_function *p_fun, void *p_usr_data);
/**
 * \brief USB 主机功能接口用户数据获取
 *
 * \param[in]  p_fun      USB 主机功能接口
 * \param[out] p_usr_data 获取到的用户数据
 *
 * \retval 成功返回 USB_OK
 */
int usbh_func_usrdata_get(struct usbh_function *p_fun, void **p_usr_data);
/**
 * \brief USB 主机功能接口驱动数据设置
 *
 * \param[in] p_fun      USB 主机功能接口
 * \param[in] p_drv_data 要设置的驱动数据
 *
 * \retval 成功返回 USB_OK
 */
int usbh_func_drvdata_set(struct usbh_function *p_usb_fun, void *p_drv_data);
/**
 * \brief USB 主机功能接口驱动数据获取
 *
 * \param[in]  p_fun      USB 主机功能接口
 * \param[out] p_drv_data 获取到的驱动数据
 *
 * \retval 成功返回 USB_OK
 */
int usbh_func_drvdata_get(struct usbh_function *p_usb_fun, void **p_drv_data);
/**
 * \brief USB 主机传输请求包提交
 *
 * \param[in] p_trp 要提交的传输请求包
 *
 * \retval 成功返回 USB_OK
 */
int usbh_trp_submit(struct usbh_trp *p_trp);
/**
 * \brief USB 主机传输请求包取消
 *
 * \param[in] p_trp 要取消的传输请求包
 *
 * \retval 成功返回 USB_OK
 */
int usbh_trp_xfer_cancel(struct usbh_trp *p_trp);
/**
 * \brief USB 主机提交传输请求包
 *
 * \param[in] p_ep     使用的端点
 * \param[in] p_ctrl   控制传输请求结构体
 * \param[in] p_data   要写/读的数据缓存
 * \param[in] len      要写/读的数据长度
 * \param[in] time_out 超时时间
 * \param[in] flag     标志
 *
 * \retval 成功返回实际传输长度，失败返回传输请求包状态
 */
int usbh_trp_sync_xfer(struct usbh_endpoint  *p_ep,
                       struct usb_ctrlreq    *p_ctrl,
                       void                  *p_data,
                       int                    len,
                       int                    time_out,
                       int                    flag);
/**
 * \brief USB 主机同步控制传输
 *
 * \param[in] p_ep     使用控制传输的端点
 * \param[in] type     传输方向 | 请求类型 | 请求目标
 * \param[in] req      具体的 USB 请求
 * \param[in] val      参数
 * \param[in] idx      索引
 * \param[in] len      要写/读的数据长度
 * \param[in] p_data   要写/读的数据缓存
 * \param[in] time_out 超时时间
 * \param[in] flag     标志
 *
 * \retval 如果没有数据，成功返回 0，如果有数据，成功返回传输的数据大小
 */
int usbh_ctrl_trp_sync_xfer(struct usbh_endpoint *p_ep,
                            uint8_t               type,
                            uint8_t               req,
                            uint16_t              val,
                            uint16_t              idx,
                            uint16_t              len,
                            void                 *p_data,
                            int                   time_out,
                            int                   flag);
/**
 * \brief USB 主机设备备用接口设置
 *
 * \param[in] p_ufun   USB 接口功能结构体
 * \param[in] intf_num 接口编号
 * \param[in] alt_num  接口的备用设置号
 *
 * \retval 成功返回 USB_OK
 */
int usbh_func_intf_set(struct usbh_function *p_usb_fun,
                       uint8_t               intf_num,
                       uint8_t               alt_num);
/**
 * \brief USB 主机功能接口获取
 *
 * \param[in] p_usb_fun USB 主机功能接口
 * \param[in] intf_num  接口编号
 * \param[in] alt_num   备用设置编号
 *
 * \retval 成功返回 USB 主机接口结构体
 */
struct usbh_interface *usbh_func_intf_get(struct usbh_function *p_usb_fun,
                                          uint8_t               intf_num,
                                          uint8_t               alt_num);
/**
 * \brief 通过接口号和备用设置号获取 USB 主机设备对应的接口结构体
 *
 * \param[in] p_usb_dev USB 主机设备
 * \param[in] intf_num  接口编号
 * \param[in] alt_num   接口的备用设置号
 *
 * \retval 成功返回找到的 USB 主机接口结构体。
 */
struct usbh_interface *usbh_dev_intf_get(struct usbh_device *p_usb_dev,
                                         uint8_t             intf_num,
                                         uint8_t             alt_num);
/**
 * \brief 根据端点地址获取接口端点
 *
 * \param[in]  p_usb_fun USB 主机功能接口
 * \param[in]  ep_addr   端点地址
 * \param[out] p_ep_ret  返回找到的端点结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_intf_ep_get(struct usbh_interface *p_intf,
                     uint8_t                ep_addr,
                     struct usbh_endpoint **p_ep_ret);



/**
 * \brief USB 主机设备的引用
 *
 * \param[in] p_usb_dev USB 主机设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_ref_get(struct usbh_device *p_usb_dev);
/**
 * \brief USB 主机设备取消引用
 *
 * \param[in] p_usb_dev USB 主机设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_ref_put(struct usbh_device *p_usb_dev);
/**
 * \brief USB 主机设备类型设置
 *
 * \param[in] p_usb_dev USB 主机设备
 * \param[in] dev_type  要设置的设备类型
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_type_set(struct usbh_device *p_usb_dev, uint32_t dev_type);
/**
 * \brief 检查 USB 主机设备是否在连接状态
 *
 * \param[in] p_usb_dev 要检查的 USB 主机设备
 *
 * \retval 连接返回 USB_TRUE，没连接返回 USB_FALSE
 */
usb_bool_t usbh_dev_is_connect(struct usbh_device *p_usb_dev);


/**
 * \brief USB 主机设备字符串获取
 *
 * \param[in]  p_usb_dev USB 主机设备
 * \param[in]  idx       字符描述符索引
 * \param[out] p_srting  返回的设备字符串描述符的字符串
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_string_get(struct usbh_device *p_usb_dev,
                        uint8_t             idx,
                        char              **p_string);
/**
 * \brief USB 主机设备字符串释放
 */
void usbh_dev_string_put(char *p_string);
/**
 * \brief USB 主机设备端点复位
 *
 * \param[in] p_ep 要复位的点
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_ep_reset(struct usbh_endpoint *p_ep);
/**
 * \brief USB 主机设备端点停止清除
 *
 * \param[in] p_ep 要清除停止状态的端点
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_ep_halt_clr(struct usbh_endpoint *p_ep);



/**
 * \brief USB 主机集线器事件添加函数
 *
 * \param[in] p_evt USB 主机集线器事件
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_evt_add(struct usbh_hub_evt *p_evt);
/**
 * \brief USB 主机集线器事件删除函数
 *
 * \param[in] p_evt USB 主机集线器事件
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_evt_del(struct usbh_hub_evt *p_evt);
/**
 * \brief USB 主机集线器事件处理函数
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_evt_process(void);
/**
 * \brief USB 主机基础集线器端口设备连接
 *
 * \param[in] p_hub_basic 基本集线器
 * \param[in] port_num    集线器端口号
 *
 * \retval 成功返回 USB_OK
 */
int usbh_basic_hub_dev_connect(struct usbh_hub_basic *p_hub_basic,
                               uint8_t                port_num);
/**
 * \brief USB 主机基础集线器端口设备断开
 *
 * \param[in] p_hub_basic 基本集线器
 * \param[in] port_num    集线器端口号
 *
 * \retval 成功返回 USB_OK
 */
int usbh_basic_hub_dev_disconnect(struct usbh_hub_basic *p_hub_basic,
                                  uint8_t                port_num);
/**
 * \brief USB 主机基础集线器端口连接检查
 *
 * \param[in]  p_hub_basic       USB 主机基础集线器
 * \param[in]  port_num          集线器端口号
 * \param[out] p_is_port_connect 返回连接的结果
 *
 * \retval 成功返回 USB_OK
 */
int usbh_basic_hub_port_connect_chk(struct usbh_hub_basic *p_hub_basic,
                                    uint8_t                port_num,
                                    usb_bool_t            *p_is_port_connect);
/**
 * \brief USB 主机基础集线器端口速度获取函数
 *
 * \param[in]  p_hub_basic USB 主机基础集线器
 * \param[in]  port_num    端口号
 * \param[out] p_speed     返回获取到端口速度
 *
 * \retval 成功返回 USB_OK
 */
int usbh_basic_hub_ps_get(struct usbh_hub_basic *p_hub_basic,
                          uint8_t                port_num,
                          uint8_t               *p_speed);
/**
 * \brief USB 主机基础集线器复位操作函数
 *
 * \param[in] p_hub_basic USB 主机基础集线器
 * \param[in] is_port     是否复位端口
 * \param[in] port_mum    要复位端口号
 *
 * \retval 成功返回 USB_OK
 */
int usbh_basic_hub_reset(struct usbh_hub_basic *p_hub_basic,
                         usb_bool_t             is_port,
                         uint8_t                port_mum);



/**
 * \brief USB 主机设备传输超时时间获取
 *
 * \param[out] p_time_out 返回的超时时间
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_xfer_timeout_get(int *p_time_out);
/**
 * \brief USB 主机设备传输超时时间设置
 *
 * \param[in] time_out 要设置的超时时间
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_xfer_timeout_set(int time_out);

/**
 * \brief USB 主机设备处理函数
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_process(void);
/**
 * \brief 获取当前存在的 USB 主机设备数量
 *
 * \param[out] p_n_dev 返回设备数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_lib_ndev_get(uint32_t *p_n_dev);
/**
 * \brief USB 主机设备库初始化
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_lib_init(void);
/**
 * \brief USB 主机设备库反初始化
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_lib_deinit(void);



/**
 * \brief USB 主机设备监控注册函数
 *
 * \param[in]  vid           要监控的 USB 主机设备的 PID
 * \param[in]  pid           要监控的 USB 主机设备的 VID
 * \param[in]  dev_type      要监控的 USB 主机设备的类型
 * \param[in]  monitor_type  监控类型
 * \param[in]  p_fn_usr_cb   用户回调函数
 * \param[in]  p_arg         回调函数参数
 * \param[out] p_monitor_ret 返回的监控句柄
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_monitor_register(uint16_t                  vid,
                              uint16_t                  pid,
                              uint32_t                  dev_type,
                              uint8_t                   monitor_type,
                              void                    (*p_fn_usr_cb)(void *p_arg, uint8_t type),
                              void                     *p_arg,
                              struct usbh_dev_monitor **p_monitor_ret);
/**
 * \brief USB 主机设备监控注销函数
 *
 * \param[in] p_monitor 要注销的监控句柄
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_monitor_deregister(struct usbh_dev_monitor *p_monitor);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif /* __USBH_DEV_H */
