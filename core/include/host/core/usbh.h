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
#ifndef __USBH_H
#define __USBH_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "adapter/usb_adapter.h"
#include "common/usb_common.h"
#include "common/list/usb_list.h"
#include "config/usb_config.h"
#include "core/include/specs/usb_specs.h"
#include "core/include/usb_lib.h"
#include "core/include/host/core/usbh_dev.h"

#if USB_OS_EN
/* \brief USB 主机库互斥锁超时时间*/
#define USBH_LIB_MUTEX_TIMEOUT 5000
/* \brief USB 主机互斥锁超时时间*/
#define USB_HC_MUTEX_TIMEOUT   5000
#endif

struct usb_hc;
struct usbh_hub_basic;

/* \brief USB 主机库结构体*/
struct usbh_core_lib {
    struct usb_lib_base lib;                /* USB 库*/
    usb_bool_t          is_lib_deiniting;   /* 是否移除库*/
    int                 ref_cnt;            /* 引用计数*/
};

/* \brief 集线器状态*/
enum {
    USBH_HUB_HALTED = 0,
    USBH_HUB_SUSPEND,
    USBH_HUB_RUNNING,
    USBH_HUB_STOP,
};

/**
 * \brief USB TT(Transaction Translators)
 *        当低/全速设备接入高速集线器时，就会使用事务转换器
 */
struct usbh_tt {
    struct usbh_function *p_hub;      /* 上游高速集线器*/
    int                   multi;      /* 是否每个端口都有一个事物转换器*/
    uint32_t              think_time; /* 思考时间(纳秒)*/
};

/* \brief USB根集线器结构体*/
struct usbh_hub_basic {
    struct usbh_function *p_usb_fun;  /* 相关的USB功能结构体*/
    struct usbh_hub_evt   evt;
    uint8_t               n_ports;    /* 端口数量*/
    uint32_t              pwr_time;   /* 上电稳定时间*/
    uint8_t               speed;      /* 集线器速度*/
    struct usbh_tt        tt;         /* 事务转换器(低/全速设备接到高速集线器)*/
    struct usbh_device  **p_ports;    /* 集线器端口设备*/
    uint8_t              *p_port_sta; /* 端口状态*/
    uint8_t               hub_status; /* 集线器状态*/
    /* 集线器处理函数*/
    int (*p_fn_hub_handle)(struct usbh_hub_basic *p_hub_basic,
                           struct usbh_hub_evt   *p_hub_evt);
    /* 获取集线器复位操作*/
    int (*p_fn_hub_reset)(struct usbh_hub_basic *p_hub_basic,
                          usb_bool_t             is_port,
                          uint8_t                port_mum);
    /* 获取集线器上电操作*/
    int (*p_fn_power)(struct usbh_hub_basic *p_hub_basic,
                      usb_bool_t             is_port,
                      uint8_t                port_mum,
                      usb_bool_t             is_power);
    /* 检查根集线器端口是否连接*/
    int (*p_fn_hub_pc_check)(struct usbh_hub_basic *p_hub_basic,
                             uint8_t                port_num);
    /* 获取集线器端口速度*/
    int (*p_fn_hub_ps_get)(struct usbh_hub_basic *p_hub_basic,
                           uint8_t                port_num,
                           uint8_t               *p_speed);
};

/* \brief USB主机控制器类型*/
enum{
    UNKNOW = 0,
    OHCI   = 1,
    EHCI   = 2,
    XHCI   = 3,
    CUSTOM = 4,
};

/* \brief USB主机驱动回调函数*/
struct usb_hc_drv {
    /* 初始化根集线器*/
    int (*p_fn_rh_init)(struct usb_hc *p_hc);

    /* 启动主机控制器*/
    int (*p_fn_hc_start)(struct usb_hc *p_hc);
    /* 停止主机控制器*/
    int (*p_fn_hc_stop)(struct usb_hc *p_hc);
    /* 挂起主机控制器*/
    int (*p_fn_hc_suspend)(struct usb_hc *p_hc);
    /* 恢复主机控制器*/
    int (*p_fn_hc_resume)(struct usb_hc *p_hc);

    /* 请求传输*/
    int (*p_fn_xfer_request)(struct usb_hc   *p_hc,
                             struct usbh_trp *p_trp);
    /* 取消传输*/
    int (*p_fn_xfer_cancel)(struct usb_hc   *p_hc,
                            struct usbh_trp *p_trp);
    /* 使能端点*/
    int (*p_fn_ep_enable)(struct usb_hc        *p_hc,
                          struct usbh_endpoint *p_ep);
    /* 禁能端点*/
    int (*p_fn_ep_disable)(struct usb_hc        *p_hc,
                           struct usbh_endpoint *p_ep);
    /* 获取当前帧编号*/
    int (*p_fn_frame_num_get)(struct usb_hc *p_hc);
#if USB_MEM_RECORD_EN
    /* 获取控制器数据结构体使用情况*/
    int (*p_fn_controller_mem_get)(struct usb_hc *p_hc);
#endif
};

/* \brief USB主机控制器结构体头 */
struct usb_hc_head {
    uint8_t            controller_type;  /* 控制器类型*/
    struct usb_hc_drv *p_controller_drv; /* 控制器驱动函数集*/
    struct usb_hc     *p_hc;             /* USB主机*/
    usb_bool_t         is_init;          /* 控制器是否已经初始化*/
};

/* \brief USB主机结构体*/
struct usb_hc {
    uint8_t               host_idx;      /* 主机索引*/
    uint32_t              map[4];        /* 设备地址表*/
    struct usbh_hub_basic root_hub;      /* 根集线器*/
    uint8_t               speed;         /* 集线器速度*/
#if USB_OS_EN
    usb_mutex_handle_t    p_lock;        /* 互斥锁，只用于OS模式*/
#endif
    struct usb_list_node  node;          /* 当前主机节点*/
    void                 *p_controller;  /* 主机控制器*/
    usb_bool_t            is_init;       /* 是否初始化*/
    uint8_t              *p_usr_priv;    /* 用户私有数据*/
};

/**
 * \brief USB 主机库初始化
 *
 * \retval 成功返回 USB_OK
 */
int usbh_core_lib_init(void);
/**
 * \brief USB 主机库反初始化
 *
 * \retval 成功返回 USB_OK
 */
int usbh_core_lib_deinit(void);
/**
 * \brief 通过索引获取 USB 主机
 *
 * \param[in]  host_idx USB 主机索引
 * \param[out] p_hc     USB 主机地址
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_get(uint8_t host_idx, struct usb_hc **p_hc);
/**
 * \brief 设置 USB 主机用户私有数据
 *
 * \param[in] p_hc       USB 主机结构体
 * \param[in] p_usr_data 用户私有数据
 *
 * \retval 成功返回 BL_OK
 */
int usb_hc_usrdata_set(struct usb_hc *p_hc, void *p_usr_data);
/**
 * \brief 获取 USB 主机用户私有数据
 *
 * \param[in]  p_hc       USB 主机结构体
 * \param[out] p_usr_priv 获取到的用户私有数据
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_usrdata_get(struct usb_hc *p_hc, void **p_usr_priv);
/**
 * \brief 设置 USB 主机控制器
 *
 * \param[in] p_hc         USB 主机结构体
 * \param[in] p_controller 具体的主机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_host_controller_set(struct usb_hc *p_hc, void *p_controller);
/**
 * \brief 获取 USB 主机控制器
 *
 * \param[in]  p_hc         USB 主机结构体
 * \param[out] p_controller 返回的 USB 主机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_host_controller_get(struct usb_hc *p_hc, void **p_controller);
/**
 * \brief 创建 USB 主机控制器
 *
 * \param[in]  host_idx 主机结构体索引
 * \param[out] p_hc     创建成功的 USB 主机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_create(uint8_t host_idx, struct usb_hc **p_hc);
/**
 * \brief 销毁 USB 主机控制器
 *
 * \param[in] p_hc 要销毁的主机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_destory(struct usb_hc *p_hc);
/**
 * \brief 启动 USB 主机控制器
 *
 * \param[in] p_hc 要启动的主机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_start(struct usb_hc *p_hc);
/**
 * \brief 停止 USB 主机控制器
 *
 * \param[in] p_hc 要停止的主机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_stop(struct usb_hc *p_hc);
/**
 * \brief 挂起 USB 主机控制器
 *
 * \param[in] p_hc 要挂起的主机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_suspend(struct usb_hc *p_hc);
/**
 * \brief 恢复 USB 主机控制器
 *
 * \param[in] p_hc 要恢复的主机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_resume(struct usb_hc *p_hc);
/**
 * \brief 获取 USB 主机当前帧编号
 *
 * \param[in] p_hc USB 主机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_frame_num_get(struct usb_hc *p_hc);
/**
 * \brief USB 根集线器初始化
 *
 * \param[in]  p_hc       USB 主机结构体
 * \param[out] p_n_ports  返回根集线器端口数量
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_rh_init(struct usb_hc *p_hc, uint8_t *p_n_ports);
/**
 * \brief 复位根集线器
 *
 * \param[in] p_hc     USB主机控制器结构体
 * \param[in] is_port  是否是端口
 * \param[in] port_num 端口号
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_rh_reset(struct usb_hc *p_hc, usb_bool_t is_port, uint8_t port_num);
#if USB_MEM_RECORD_EN
/**
 * \brief 获取 USB 主机内存记录
 *
 * \param[out] p_mem_record 返回内存记录
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_mem_record_get(struct usb_mem_record *p_mem_record);
#endif
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif /* __USBH_H */
