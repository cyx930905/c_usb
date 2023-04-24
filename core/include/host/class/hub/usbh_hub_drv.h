#ifndef __USBH_HUB_DRV_H
#define __USBH_HUB_DRV_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "adapter/usb_adapter.h"
#include "common/usb_common.h"
#include "common/list/usb_list.h"
#include "core/include/usb_lib.h"
#include "core/include/host/core/usbh_dev.h"
#include "core/include/host/core/usbh.h"

#if USB_OS_EN
/* \brief USB 集线器互斥锁上锁超时时间*/
#define UHUB_LOCK_TIMEOUT     5000
#endif

/* \brief USB 集线器传输超时时间*/
#define UHUB_XFER_TIMEOUT     5000
/* \brief USB 集线器最大端口数量 */
#define USBH_HUB_PORTS_MAX    15

/* \brief 普通集线器*/
#define USB_DT_HUB           (USB_REQ_TYPE_CLASS | 0x09)
/* \brief USB3.0 集线器*/
#define USB_DT_SS_HUB        (USB_REQ_TYPE_CLASS | 0x0a)

/* \brief 获取 USB 集线器设备 VID */
#define USBH_HUB_DEV_VID_GET(p_hub)   USBH_DEV_VID_GET(p_hub->hub_basic.p_usb_fun)
/* \brief 获取 USB 集线器设备 PID */
#define USBH_HUB_DEV_PID_GET(p_hub)   USBH_DEV_PID_GET(p_hub->hub_basic.p_usb_fun)

/* \brief USB 集线器库结构体*/
struct uhub_lib {
    struct usb_lib_base  lib;               /* USB 库*/
    usb_bool_t           is_lib_deiniting;  /* 是否移除库*/
    struct usb_list_head evt_list;          /* 集线器事件链表*/
    int                  xfer_time_out;     /* 传输超时时间*/
    int                  ref_cnt;           /* 引用计数*/
};

/* \brief USB 集线器结构体*/
struct usbh_hub {
    struct usbh_hub_basic    hub_basic;
    struct usbh_endpoint    *p_ep;                        /* 集线器的端点*/
    char                     name[USB_NAME_LEN];          /* 集线器名字*/
    int                      ref_cnt;                     /* 引用计数*/
    struct usbh_trp          trp;                         /* 集线器传输请求包*/
    uint32_t                 sta;                         /* 集线器状态*/
    uint8_t                  err_cnt;                     /* 错误计数*/
    struct usb_list_node     node;                        /* 当前集线器节点*/
#if USB_OS_EN
    usb_mutex_handle_t       p_lock;                      /* 互斥锁*/
#endif
    struct usbh_device      *p_ports[USBH_HUB_PORTS_MAX]; /* 集线器端口设备*/
    usb_bool_t               is_removed;                  /* 移除标志*/
};

/**
 * \brief USB 主机集线器设备探测
 *
 * \param[in] p_usb_fun USB 功能结构体
 *
 * \retval 是集线器设备返回 USB_OK
 */
int usbh_hub_probe(struct usbh_function *p_usb_fun);
/**
 * \brief 获取 USB 集线器设备名字
 *
 * \param[in] p_hub     USB 集线器设备结构体
 * \param[in] p_name    存储返回的名字缓存
 * \param[in] name_size 名字缓存大小
 *
 * \retval 成功获取返回 USB_OK
 */
int usbh_hub_name_get(struct usbh_hub *p_hub,
                      char            *p_name,
                      uint32_t         name_size);
/**
 * \brief 设置 USB 集线器设备名字
 *
 * \param[in] p_hub     USB 集线器设备结构体
 * \param[in] p_name_new 要设置的新的名字
 *
 * \retval 成功设置返回 USB_OK
 */
int usbh_hub_name_set(struct usbh_hub *p_hub, char *p_name_new);
/**
 * \brief 获取 USB 集线器状态
 *
 * \param[in] p_hub USB 集线器设备结构体
 * \param[in] p_sta 要返回的状态
 *
 * \retval 成功设置返回 USB_OK
 */
int usbh_hub_sta_get(struct usbh_hub *p_hub, uint8_t *p_sta);
/**
 * \brief 获取 USB 集线器的端口数量
 *
 * \param[in] p_hub     USB 集线器
 * \param[in] p_n_ports 返回端口数量
 *
 * \retval 是集线器设备返回 USB_OK
 */
int usbh_hub_nport_get(struct usbh_hub *p_hub, uint8_t *p_n_ports);
/**
 * \brief 检查 USB 集线器端口是否有设备连接
 *
 * \param[in] p_hub        USB 集线器
 * \param[in] port_num     端口号
 * \param[in] p_is_connect 返回连接状态
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_port_is_connect(struct usbh_hub *p_hub,
                             uint8_t          port_num,
                             usb_bool_t      *p_is_connect);
/**
 * \brief 获取 USB 集线器端口状态
 *
 * \param[in] p_hub    USB 集线器
 * \param[in] port_num 端口号
 * \param[in] p_sta    返回的状态
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_port_sta_get(struct usbh_hub *p_hub, int port_num, uint8_t *p_sta);
/**
 * \brief 创建一个 USB 集线器设备
 *
 * \param[in] p_ufun USB 功能结构体
 * \param[in] p_name 要创建的集线器的名字
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_create(struct usbh_function *p_usb_fun, char *p_name);
/**
 * \brief 销毁 USB 集线器
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_destroy(struct usbh_function *p_usb_fun);
/**
 * \brief 复位 USB 集线器设备遍历
 */
void usbh_hub_traverse_reset(void);
/**
 * \brief USB 集线器设备遍历
 *
 * \param[out] p_name    返回的名字缓存
 * \param[in]  name_size 名字缓存的大小
 * \param[out] p_vid     返回的设备 VID
 * \param[out] p_pid     返回的设备 PID
 *
 * \retval 成功返回 USB_OK，到末尾返回 -USB_END
 */
int usbh_hub_traverse(char     *p_name,
                      uint32_t  name_size,
                      uint16_t *p_vid,
                      uint16_t *p_pid);
/**
 * \brief 打开一个 USB 集线器
 *
 * \param[in]  name  设备名字
 * \param[in]  flag  打开标志，本接口支持三种打开方式：
 *                   USBH_HUB_OPEN_BY_NAME是通过名字打开设备
 *                   USBH_HUB_OPEN_BY_FUN是通过USB功能结构体打开设备
 * \param[out] p_hub 返回打开的 USB 集线器结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_open(void *p_handle, uint8_t flag, struct usbh_hub **p_hub);
/**
 * \brief 关闭 USB 集线器
 *
 * \param[in] p_hub USB 集线器结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_close(struct usbh_hub *p_hub);
/**
 * \brief 使能 USB 集线器端口
 *
 * \param[in] p_hub     集线器结构体
 * \param[in] port_num  要使能的端口号
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_port_enable(struct usbh_hub *p_hub, int port_num);
/**
 * \brief 禁能 USB 集线器端口
 *
 * \param[in] p_hub    USB 集线器结构体
 * \param[in] port_num 要禁能的端口号
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_port_disable(struct usbh_hub *p_hub, int port_num);
/**
 * \brief USB 集线器端口复位
 *
 * \param[in] p_hub    USB 集线器结构体
 * \param[in] port_num 要复位的端口号
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_port_reset(struct usbh_hub *p_hub, int port_num);
/**
 * \brief 获取 USB 集线器端口状态
 *
 * \param[in]  p_hub    USB 集线器结构体
 * \param[in]  port_num 要获取状态的端口号
 * \param[out] p_sta    返回获取到的端口状态
 *
 * \retval 成功返回 USB_OK，状态 1 表示使能，0 表示禁能
 */
int usbh_hub_port_sta_get(struct usbh_hub *p_hub, int port_num, uint8_t *p_sta);
/**
 * \brief 启动 USB 集线器
 *
 * \param[in] p_hub USB 集线器结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_start(struct usbh_hub *p_hub);
/**
 * \brief 停止 USB 集线器
 *
 * \param[in] p_hub USB 集线器结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_stop(struct usbh_hub *p_hub);
/**
 * \brief 获取 USB 集线器状态
 *
 * \param[in]  p_hub USB 集线器结构体
 * \param[out] p_sta 返回获取到的集线器状态
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_sta_get(struct usbh_hub *p_hub, uint8_t *p_sta);

/**
 * \brief 获取当前存在的 USB 集线器设备数量
 *
 * \param[in] p_n_dev 存储返回的设备数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_lib_ndev_get(uint8_t *p_n_dev);
/**
 * \brief 初始化 USB 集线器设备库
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_lib_init(void);
/**
 * \brief 反初始化 USB 集线器设备库
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_lib_deinit(void);
#if USB_MEM_RECORD_EN
/**
 * \brief 获取 USB 集线器驱动内存记录
 *
 * \param[out] p_mem_record 返回内存记录
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_lib_mem_record_get(struct usb_mem_record *p_mem_record);
#endif
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif

