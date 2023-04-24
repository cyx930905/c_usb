#ifndef __USBH_VC_QUEUE_H
#define __USBH_VC_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "core/include/host/core/usbh.h"
#include "adapter/usb_adapter.h"
#include "common/usb_common.h"
#include "common/list/usb_list.h"

/* \brief 每个 USB 请求包的最大包数量*/
#define UVC_MAX_PACKETS              32

#define UVC_QUEUE_DISCONNECTED      (1 << 0)
#define UVC_QUEUE_DROP_CORRUPTED    (1 << 1)

/* \brief 缓存状态*/
enum uvc_buf_state {
    UVC_BUF_STATE_IDLE   = 0,
    UVC_BUF_STATE_QUEUED = 1,
    UVC_BUF_STATE_ACTIVE = 2,
    UVC_BUF_STATE_READY  = 3,
    UVC_BUF_STATE_DONE   = 4,
    UVC_BUF_STATE_ERROR  = 5,
};

/* \brief 缓存时间戳*/
struct uvc_timeval {
    long ts_sec;         /* 秒*/
    long ts_usec;        /* 微秒*/
};

/* \brief USB 视频类队列结构体 */
struct usbh_vc_queue {
#if USB_OS_EN
    usb_mutex_handle_t   p_lock;       /* 保护队列*/
#endif
    usb_bool_t           is_used;      /* 是否被使用*/

    uint32_t             flags;
    uint32_t             n_bufs_used;  /* 缓存使用的个数*/

    uint32_t             n_bufs_total; /* 被分配/使用的缓存的数量*/
    struct usb_list_head irqqueue;     /* 中断队列链表*/
};

/* \brief USB 视频类缓存结构体 */
struct usbh_vc_buffer {
    struct usb_list_node node;       /* 缓存节点*/

    struct uvc_timeval   timeval;     /* 时间值*/
    enum uvc_buf_state   state;       /* 缓存状态*/
    uint32_t             error_no;    /* 错误代码*/

    void                *p_mem;       /* 缓存指针*/
    uint32_t             length;      /* 缓存长度*/
    uint32_t             bytes_used;  /* 缓存已填充字节数*/
    uint8_t              is_used;     /* 是否被使用标志*/

    uint32_t             pts;
};

/**
 * \brief 获取可用的缓存队列
 *
 * \retval 成功返回可用的缓存队列
 */
struct usbh_vc_queue *usbn_vc_queue_get(void);
/**
 * \brief 释放缓存队列
 *
 * \param[in] 要释放可用的缓存队列
 */
void usbh_vc_queue_put(struct usbh_vc_queue *p_queue);
/**
 * \brief USB 视频类队列缓存初始化
 *
 * \param[in] p_queue  视频缓存队列
 * \param[in] buf_size 要申请的缓存大小
 * \param[in] n_buf    要申请的缓存个数
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_queue_buf_alloc(struct usbh_vc_queue *p_queue,
                            uint32_t              buf_size,
                            uint8_t               n_buf);
/**
 * \brief USB 视频类设备队列释放缓存
 *
 * \param[in] p_queue  USB 视频类设备缓存队列
 *
 * \retval 成功返回 UBS_OK
 */
int usbh_vc_queue_buf_free(struct usbh_vc_queue *p_queue);
/**
 * \brief 创建 USB 视频类驱动缓存队列
 *
 * \param[in]  n_stream_max 最大支持的 USB 视频类设备数据流数量
 * \param[in]  n_stream_buf 每个 USB 视频类设备数据流的缓存个数
 * \param[out] p_queue      返回成功创建的队列结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_queue_create(uint8_t                n_stream_max,
                         uint8_t                n_stream_buf,
                         struct usbh_vc_queue **p_queue);
/**
 * \brief 销毁 USB 视频类驱动缓存队列
 *
 * \param[in] p_queue      要销毁的 USB 视频类驱动缓存队列
 * \param[in] n_stream_max 最大支持的 USB 视频类设备数据流数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_queue_destory(struct usbh_vc_queue *p_queue, uint8_t n_stream_max);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif


