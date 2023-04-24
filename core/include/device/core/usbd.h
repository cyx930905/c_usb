#ifndef __USBD_H
#define __USBD_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "adapter/usb_adapter.h"
#include "common/list/usb_list.h"
#include "core/include/usb_lib.h"
#include "core/include/specs/usb_specs.h"
#include "core/include/device/core/usbd_dev.h"

#if USB_OS_EN
/* \brief USB 从机互斥锁超时时间*/
#define USB_DC_MUTEX_TIMEOUT   5000
#endif

/* \brief 控制器状态*/
#define USBD_ST_NOTATTACHED        0           /* 断开*/
#define USBD_ST_ATTACHED           1           /* 连接但还没有枚举*/
#define USBD_ST_POWERED            2           /* 上电*/
#define USBD_ST_DEFAULT            3           /* 默认*/
#define USBD_ST_ADDRESS            4           /* 设置地址*/
#define USBD_ST_CONFIGURED         5           /* 配置*/
#define USBD_ST_SUSPENDED          6           /* 挂起*/

/* \brief 控制器速度*/
#define USBD_SPEED_UNKNOWN         0
#define USBD_SPEED_LOW             1           /* usb 1.1 */
#define USBD_SPEED_FULL            2           /* usb 1.1 */
#define USBD_SPEED_HIGH            3           /* usb 2.0 */
#define USBD_SPEED_WIRELESS        4           /* 无线 (usb 2.5) */
#define USBD_SPEED_SUPER           5           /* usb 3.0 */

/* \brief 传输标志*/
#define USBD_SHORT_NOT_OK          0x00000001     /* 短包错误标志*/
#define USBD_ZERO_PACKET           0x00000002     /* 用一个短包来结束一次传输*/
#define USBD_NO_INTERRUPT          0x00000004     /* 不需要中断，除非错误*/

/* \brief 端点类型支持标志*/
#define USBD_EP_SUPPORT_CTRL       0x01
#define USBD_EP_SUPPORT_ISO        0x02
#define USBD_EP_SUPPORT_BULK       0x04
#define USBD_EP_SUPPORT_INT        0x08
#define USBD_EP_SUPPORT_ALL        0x0F

/* \brief 端点最大包大小限制*/
#define USBD_EP_MPS_NO_LIMT        0x00
#define USBD_EP0_MAX_PKT_SIZE      64
#define USBD_EP0_REQ_BUF_SIZE      512

/* \brief 控制器配置标志*/
#define USBD_CFG_SELF_POWERED      0x0200
#define USBD_CFG_REMOTE_WAKEUP     0x0400
#define USBD_CFG_MAX_POWER         0x01FF
#define USBD_CFG_MAX_POWER_MA(v)  (v & 0x1FF)

#define USB_WBVAL(x) (x & 0xFF),((x >> 8) & 0xFF)

/* \brief USB 从机库结构体*/
struct usbd_core_lib {
    struct usb_lib_base lib;                /* USB 库*/
    usb_bool_t          is_lib_deiniting;   /* 是否移除库*/
    int                 ref_cnt;            /* 引用计数*/
};

struct usb_dc;

/* \brief USB 从机端点0*/
struct usbd_ep0 {
    struct usb_dc     *p_dc;
    struct usbd_ep    *p_hw;
    struct usbd_trans  req;
    void             (*p_fn_ep0_cb)(void *p_arg);
    void              *p_arg;
};

/* \brief 从机控制器驱动函数集*/
struct usb_dc_drv {
    /* 控制器复位*/
    void (*p_fn_reset)(void *p_dc_drv_arg);
    /* 控制器启动*/
    int (*p_fn_run)(void *p_dc_drv_arg);
    /* 控制器停止*/
    int (*p_fn_stop)(void *p_dc_drv_arg);
    /* 传输请求*/
    int (*p_fn_xfer_req)(void *p_dc_drv_arg, struct usbd_trans *p_trans);
    /* 传输请求取消*/
    int (*p_fn_xfer_cancel)(void *p_dc_drv_arg, struct usbd_trans *p_trans);
    /* 端点使能*/
    int (*p_fn_ep_enable)(void *p_dc_drv_arg, struct usbd_ep *p_ep);
    /* 端点禁能*/
    int (*p_fn_ep_disable)(void *p_dc_drv_arg, struct usbd_ep *p_ep);
    /* 端点暂停设置 */
    int (*p_fn_ep_halt_set)(void *p_dc_drv_arg, struct usbd_ep *p_ep, usb_bool_t is_set);
    /* 复位端点，清除所有端点的状态和配置，调用之后必须再使能一次端点*/
    int (*p_fn_ep_reset)(void *p_dc_drv_arg, struct usbd_ep *p_ep);
    /* 设置设备地址*/
    int (*p_fn_addr_set)(void *p_dc_drv_arg, uint8_t addr);
    /* 设置配置*/
    int (*p_fn_config_set)(void *p_dc_drv_arg, usb_bool_t is_set);
    /* 唤醒控制器*/
    int (*p_fn_wakeup)(void *p_dc_drv_arg);
    /* 上拉或断开 D+/D-上的电阻*/
    int (*p_fn_pullup)(void *p_dc_drv_arg, usb_bool_t is_on);
};

/* \brief 从机控制器设备操作函数集*/
struct usb_dc_opts {
    /* 控制 SETUP 包处理*/
    int (*p_fn_ctrl_setup)(struct usb_dc      *p_dc,
                           struct usb_ctrlreq *p_setup,
                           void               *p_buf,
                           int                 buf_len);
    /* 总线复位*/
    void (*p_fn_reset)(struct usb_dc *p_dc);
    /* 总线挂起*/
    void (*p_fn_suspend)(struct usb_dc *p_dc);
    /* 总线恢复*/
    void (*p_fn_resume)(struct usb_dc *p_dc);
    /* 总线断开*/
    void (*p_fn_disconnect)(struct usb_dc *p_dc);
};

/* \brief USB 从机控制器结构体 */
struct usb_dc {
    uint8_t                   idx;            /* 索引*/
#if USB_OS_EN
    usb_mutex_handle_t        p_mutex;        /* USB 从机互斥锁*/
#endif
    struct usbd_ep0           ep0_in;         /* 端点 0 输入*/
    struct usbd_ep0           ep0_out;        /* 端点 0 输出*/
    uint32_t                  ep0_req_size;   /* 端点 0 请求最大大小*/
    struct usb_ctrlreq        ep0_setup;      /* 端点 0 SETUP 包*/
    struct usb_list_head      ep_list;        /* 端点链表*/
    struct usb_dc_drv        *p_dc_drv;       /* 从机驱动函数集*/
    void                     *p_dc_drv_arg;   /* 从机驱动参数*/
    struct usbd_dev          *p_usbd_dev;     /* 具体的 USB 从机设备*/
    void                     *p_usr_data;     /* 用户数据*/
    uint8_t                   speed;          /* 控制器速度*/
    usb_bool_t                is_run;         /* 是否在运行中*/
    usb_bool_t                need_sta;       /* 是否需要接收或者发送控制传输的状态 */
    int                       ref_cnt;        /* 引用计数*/
    uint8_t                   state;          /* 控制器状态*/
    uint8_t                   resume_state;   /* 记录挂起时的控制器状态*/
    struct usb_list_head      dev_list;       /* 从机控制器设备链表*/
    struct usb_list_node      node;           /* 当前从机控制器节点*/
    uint32_t                  dc_feature;     /* 控制器特性*/
    uint8_t                   device_is_hs;   /* 是否是高速设备 */
};

/**
 * \brief 初始化 USB 设备库
 *
 * \retval 成功返回 USB_OK
 */
int usbd_core_lib_init(void);
/**
 * \brief 反初始化 USB 从机库
 *
 * \retval 成功返回 USB_OK
 */
int usbd_core_lib_deinit(void);
/**
 * \brief 设置 USB 从机控制器用户数据
 *
 * \param[in] p_dc       USB 从机控制器
 * \param[in] p_usr_data 要设置的用户数据
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_usr_data_set(struct usb_dc *p_dc, void *p_usr_data);
/**
 * \brief 获取 USB 从机控制器用户数据
 *
 * \param[in]  p_dc       USB 从机控制器
 * \param[out] p_usr_data 返回的用户数据
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_usr_data_get(struct usb_dc *p_dc, void **p_usr_data);
/**
 * \brief USB 从机控制器传输完成函数
 *
 * \param[in] p_trans 传输事务
 * \param[in] status  传输状态
 * \param[in] len_act 实际传输的长度
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_trans_complete(struct usbd_trans *p_trans,
                          int                status,
                          uint32_t           len_act);
/**
 * \brief 通过索引获取 USB 从机控制器
 *
 * \param[in]  idx  USB 从机控制器索引
 * \param[out] p_dc 返回的 USB 从机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_get(uint8_t idx, struct usb_dc **p_dc);
/**
 * \brief 创建一个 USB 从机控制器
 *
 * \param[in]  idx           USB 从机控制器索引
 * \param[in]  ep0_req_size  端点 0 请求大小
 * \param[in]  p_drv         从机驱动函数集
 * \param[in]  p_dc_drv_arg  从机驱动参数
 * \param[out] p_dc          返回创建成功的 USB 从机控制机
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_create(uint8_t             idx,
                  uint32_t            ep0_req_size,
                  struct usb_dc_drv  *p_drv,
                  void               *p_dc_drv_arg,
                  struct usb_dc     **p_dc);
/**
 * \brief USB 从机控制器销毁
 *
 * \param[in] p_dc 要销毁的 USB 从机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_destroy(struct usb_dc *p_dc);
/**
 * \brief 启动 USB 从机控制器
 *
 * \param[in] p_dc USB 从机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_start(struct usb_dc *p_dc);
/**
 * \brief 停止 USB 从机控制器
 *
 * \param[in] p_dc 要停止的 USB 从机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_stop(struct usb_dc *p_dc);
/**
 * \brief 注册一个 USB 从机控制器端点
 *
 * \param[in] p_dc         相关的 USB 从机控制器
 * \param[in] ep_addr      端点地址
 * \param[in] type_support 可支持的类型
 * \param[in] mps_limt     最大包大小限制
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_ep_register(struct usb_dc  *p_dc,
                       uint8_t         ep_addr,
                       uint8_t         type_support,
                       uint16_t        mps_limt);
/**
 * \brief 注销一个 USB 从机控制器端点
 *
 * \param[in] p_dc    相关的 USB 从机控制器
 * \param[in] ep_addr 端点地址
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_ep_deregister(struct usb_dc *p_dc, uint8_t ep_addr);
/**
 * \brief USB 从机控制器端点 0 请求
 *
 * \param[in] p_dc            USB 从机控制器
 * \param[in] dir             请求方向
 * \param[in] p_buf           数据缓存
 * \param[in] buf_size        缓存大小
 * \param[in] p_fn_ep0_out_cb 回调函数
 * \param[in] p_arg           回调函数参数
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_ep0_req(struct usb_dc *p_dc,
                   uint8_t        dir,
                   uint8_t       *p_buf,
                   uint32_t       buf_size,
                   void         (*p_fn_ep0_cb)(void *p_arg),
                   void          *p_arg);
/**
 * \brie USB 从机传输请求函数
 *
 * \param[in] p_dc    USB 从机控制器
 * \param[in] p_trans USB 传输请求包
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_trans_req(struct usb_dc     *p_dc,
                     struct usbd_trans *p_trans);
/**
 * \brie USB 从机传输取消请求函数
 *
 * \param[in] p_dc    USB 从机控制器
 * \param[in] p_trans USB 传输请求包
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_trans_cancel(struct usb_dc     *p_dc,
                        struct usbd_trans *p_trans);
/**
 * \brief USB 从机控制器控制传输处理
 *
 * \param[in] p_dc USB 从机控制器
 * \param[in] p_setup  Setup 包
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_setup_handle(struct usb_dc      *p_dc,
                        struct usb_ctrlreq *p_setup);
/**
 * \brief USB 从机控制器速度更新
 *
 * \param[in] p_dc   USB 从机控制器
 * \param[in] speed  要更新的速度
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_speed_update(struct usb_dc *p_dc, uint8_t speed);
/**
 * \brief USB 从机控制器总线复位处理
 *
 * \param[in] p_dc USB 从机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_bus_reset_handle(struct usb_dc *p_dc);
/**
 * \brief USB 从机控制器恢复处理
 *
 * \param[in] p_dc USB 从机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_resume_handle(struct usb_dc *p_dc);
/**
 * \brief USB 从机控制器挂起处理
 *
 * \param[in] p_dc USB 从机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_suspend_handle(struct usb_dc *p_dc);
/**
 * \brief USB 从机控制器断开处理
 *
 * \param[in] p_dc USB 从机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_disconnect_handle(struct usb_dc *p_dc);
#if USB_MEM_RECORD_EN
/**
 * \brief 获取 USB 从机内存记录
 *
 * \param[out] p_mem_record 返回内存记录
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_mem_record_get(struct usb_mem_record *p_mem_record);
#endif
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif /* __USBD_H */

