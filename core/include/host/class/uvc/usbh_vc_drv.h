#ifndef __USBH_VC_DRV_H
#define __USBH_VC_DRV_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "core/include/host/core/usbh_dev.h"
#include "core/include/host/core/usbh.h"
#include "core/include/usb_lib.h"
#include "adapter/usb_adapter.h"
#include "common/usb_common.h"
#include "common/list/usb_list.h"
#include "core/include/host/class/uvc/usbh_vc_queue.h"
#include "core/include/specs/usb_vc_specs.h"

#if USB_OS_EN
#define UVC_LOCK_TIMEOUT                 5000
#endif

/* \brief 获取 USB 视频类设备 VID*/
#define USBH_VC_DEV_VID_GET(p_vc)     USBH_DEV_VID_GET(p_vc->p_ufun)
/* \brief 获取 USB 视频类设备 PID*/
#define USBH_VC_DEV_PID_GET(p_vc)     USBH_DEV_PID_GET(p_vc->p_ufun)

/* \brief 中断包的最大状态缓存字节大小 */
#define UVC_MAX_STATUS_SIZE              16

/* \brief 设备兼容*/
#define UVC_QUIRK_FIX_BANDWIDTH          0x00000080
#define UVC_QUIRK_STREAM_NO_FID          0x00000010

/* \brief USB 视频类设备库结构体*/
struct usbh_vc_lib {
    struct usb_lib_base    lib;                    /* USB 库*/
    usb_bool_t             is_lib_deiniting;       /* 是否移除库*/
    int                    xfer_time_out;          /* 传输超时时间*/
    uint8_t                n_stream_max;           /* 可存在 USB 视频类设备数据流的最大数量*/
    uint8_t                n_stream;               /* 当前存在 USB 视频类设备数据流数量*/
    uint32_t               stream_idx;             /* USB 视频类设备数据流索引*/
    uint8_t                n_dev;                  /* 当前存在 USB 视频类设备的数量*/
    struct usbh_vc_queue  *p_queue;                /* USB 视频类缓存队列*/
    int                    ref_cnt;                /* 引用计数*/
};

/* \brief USB 视频类设备结构体*/
struct usbh_vc {
	char                   name[USB_NAME_LEN];     /* 设备名字*/
    struct usbh_function  *p_ufun;                 /* 相关的 UBS 功能结构体(描述一个 USB 设备功能)*/
    struct usbh_interface *p_ctrl_intf;            /* USB 视频类控制接口链表(根据UVC规范1.5，每个视频功能只有一个控制接口)*/
#if USB_OS_EN
    usb_mutex_handle_t     p_lock;                 /* 互斥锁*/
#endif
    uint16_t               uvc_version;            /* USB 视频类规范版本*/
    uint32_t               clk_freq;               /* 时钟频率*/
    uint32_t               n_stream_intf_altset;   /* UVC流接口备用设置的数量*/
    struct usb_list_head   streams;                /* 视频流链表*/
    struct usb_list_head   entities;               /* USB 视频类设备实体链表(描述单元和终端)*/
    struct usb_list_head   chains;                 /* 视频设备链表*/
    struct usb_list_node   node;                   /* 当前视频设备链表*/
    uint16_t               n_streams;              /* 视频流数量*/
    uint16_t               n_entities;             /* 终端数量*/
    struct usbh_trp        int_trp;                /* 中断传输请求包*/
    struct usbh_endpoint  *p_int_ep;               /* 中断端点*/
    uint8_t               *p_status;               /* UVC设备中断端点缓存*/
    uint32_t               quirks;                 /* 设备兼容*/
    usb_bool_t             is_removed;             /* 是否移除*/
    int                    ref_cnt;                /* 引用计数*/
};

/**
 * \brief 获取 USB 视频类设备视频流的个数
 *
 * \param[in] p_vc       USB 视频类设备
 * \param[in] p_n_stream 存储视频流缓存
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_nstream_get(struct usbh_vc *p_vc, uint32_t *p_n_stream);
/**
 * \brief 获取 USB 视频类设备名字
 *
 * \param[in] p_vc      USB 视频类设备
 * \param[in] p_name    存储设备名字的缓存
 * \param[in] name_size 名字缓存大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_name_get(struct usbh_vc *p_vc,
                     char           *p_name,
                     uint32_t        name_size);
/**
 * \brief 设置 USB 视频类设备名字
 *
 * \param[in] p_vc   USB 视频类设备
 * \param[in] p_name 要设置的名字
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_name_set(struct usbh_vc *p_vc, char *p_name);
/**
 * \brief USB 视频类设备探测函数
 *
 * \param[in] p_usb_fun 相关的 USB 功能接口
 *
 * \retval 探测成功返回 USB_OK
 */
int usbh_vc_probe(struct usbh_function *p_usb_fun);
/**
 * \brief 创建一个 USB 视频类设备
 *
 * \param[in] p_usb_fun 相关的 USB 功能接口
 * \param[in] p_name    USB 视频类设备的名字
 *
 * \retval 成功返回 0
 */
int usbh_vc_create(struct usbh_function *p_usb_fun, char *p_name);
/**
 * \brief 销毁 USB 视频类设备
 *
 * \param[in] p_usb_fun USB 功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_destroy(struct usbh_function *p_usb_fun);
/**
 * \brief 复位 USB 视频类设备遍历
 */
void usbh_vc_traverse_reset(void);
/**
 * \brief USB 视频类设备遍历
 *
 * \param[out] p_name    返回的名字缓存
 * \param[in]  name_size 名字缓存的大小
 * \param[out] p_vid     返回的设备 VID
 * \param[out] p_pid     返回的设备 PID
 *
 * \retval 成功返回 USB_OK，到末尾返回 -USB_END
 */
int usbh_vc_traverse(char     *p_name,
                     uint32_t  name_size,
                     uint16_t *p_vid,
                     uint16_t *p_pid);
/**
 * \brief 打开一个 USB 视频类设备
 *
 * \param[in]  p_handle 打开句柄
 * \param[in]  flag     打开标志，本接口支持两种种打开方式：
 *                      USBH_DEV_OPEN_BY_NAME 是通过名字打开设备
 *                      USBH_DEV_OPEN_BY_UFUN 是通过 USB 功能结构体打开设备
 * \param[out] p_vc     返回获取到的 USB 视频类设备地址
 *
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_open(void *p_handle, uint8_t flag, struct usbh_vc **p_vc);
/**
 * \brief 关闭 USB 视频类设备
 *
 * \param[in] p_vc USB 视频类设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_close(struct usbh_vc *p_vc);

/**
 * \brief 获取当前存在的 USB 视频类驱动设备数量
 *
 * \param[in] p_n_dev 存储返回的设备数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_lib_ndev_get(uint8_t *p_n_dev);
/**
 * \brief USB 视频类驱动库相关初始化
 *
 * \param[in] stream_num_max 最大支持 USB 视频类设备数据流数量
 * \param[in] stream_buf_num 设备数据流缓存个数
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_lib_init(uint8_t stream_num_max,
                     uint8_t stream_buf_num);
/**
 * \brief 反初始化 USB 视频类驱动库
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_lib_deinit(void);
#if USB_MEM_RECORD_EN
/**
 * \brief 获取 USB 视频类驱动库内存记录
 *
 * \param[out] p_mem_record 返回内存记录
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_lib_mem_record_get(struct usb_mem_record *p_mem_record);
#endif
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif



