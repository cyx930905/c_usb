/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/uvc/usbh_vc_drv.h"

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbh_vc_lib __g_uvc_lib;

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 获取可用的缓存队列
 *
 * \retval 成功返回可用的缓存队列
 */
struct usbh_vc_queue *usbn_vc_queue_get(void){
    int i;

    for (i = 0; i < __g_uvc_lib.n_stream_max; i++) {
        if (__g_uvc_lib.p_queue[i].is_used == USB_FALSE) {
            __g_uvc_lib.p_queue[i].is_used = USB_TRUE;
            return &__g_uvc_lib.p_queue[i];
        }
    }
    return NULL;
}

/**
 * \brief 释放缓存队列
 *
 * \param[in] 要释放可用的缓存队列
 */
void usbh_vc_queue_put(struct usbh_vc_queue *p_queue){
	p_queue->is_used = USB_FALSE;
}

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
                            uint8_t               n_buf){
    uint8_t                i;
    struct usbh_vc_buffer *p_buf = NULL;

    /* 合法性判断*/
    if ((n_buf <= 0) || (p_queue == NULL)) {
        return -USB_EINVAL;
    }

    for (i = 0;i < n_buf; i++) {
    	p_buf = usb_lib_malloc(&__g_uvc_lib.lib, sizeof(struct usbh_vc_buffer));
        if (p_buf == NULL) {
            return -USB_ENOMEM;
        }
        memset(p_buf, 0, sizeof(struct usbh_vc_buffer));
        /* 申请视频缓存*/
        p_buf->p_mem = usb_lib_malloc(&__g_uvc_lib.lib, buf_size);
        if (p_buf->p_mem == NULL) {
        	usb_lib_mfree(&__g_uvc_lib.lib, p_buf);
            return -USB_ENOMEM;
        }
        /* 缓存大小*/
        p_buf->length = buf_size;

        usb_list_node_add_tail(&p_buf->node, &p_queue->irqqueue);

        p_buf = NULL;
    }
    return USB_OK;
}

/**
 * \brief USB 视频类设备队列释放缓存
 *
 * \param[in] p_queue  USB 视频类设备缓存队列
 *
 * \retval 成功返回 UBS_OK
 */
int usbh_vc_queue_buf_free(struct usbh_vc_queue *p_queue){
    struct usbh_vc_buffer *p_buf = NULL;
    int                    ret   = USB_OK;

    if (p_queue == NULL) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_queue->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
	    __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 删除队列缓存*/
    while (!usb_list_head_is_empty(&p_queue->irqqueue)) {
        p_buf = usb_container_of(p_queue->irqqueue.p_next, struct usbh_vc_buffer, node);
        usb_list_node_del(&p_buf->node);
        usb_lib_mfree(&__g_uvc_lib.lib, p_buf->p_mem);
        usb_lib_mfree(&__g_uvc_lib.lib, p_buf);
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_queue->p_lock);
    if (ret != USB_OK) {
	    __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

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
                         struct usbh_vc_queue **p_queue){
    struct usbh_vc_queue *p_queue_tmp = NULL;
    int                   i, ret;

    p_queue_tmp = usb_lib_malloc(&__g_uvc_lib.lib, sizeof(struct usbh_vc_queue) * n_stream_max);
    if (p_queue_tmp == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_queue_tmp, 0, sizeof(struct usbh_vc_queue) * n_stream_max);

    for (i = 0;i < n_stream_max; i++) {
    	p_queue_tmp[i].n_bufs_used  = n_stream_buf;
    	p_queue_tmp[i].n_bufs_total = n_stream_buf;
    	p_queue_tmp[i].is_used      = USB_FALSE;
#if USB_OS_EN
    	p_queue_tmp[i].p_lock = usb_lib_mutex_create(&__g_uvc_lib.lib);
        if (p_queue_tmp[i].p_lock == NULL) {
        	__USB_ERR_TRACE(MutexCreateErr, "\r\n");
        	ret = -USB_EPERM;
            goto __failed;
        }
#endif
        usb_list_head_init(&p_queue_tmp[i].irqqueue);
        p_queue_tmp->flags = UVC_QUEUE_DROP_CORRUPTED;
    }

    *p_queue = p_queue_tmp;

    return USB_OK;
__failed:
    usbh_vc_queue_destory(p_queue_tmp, n_stream_max);
    return ret;
}

/**
 * \brief 销毁 USB 视频类驱动缓存队列
 *
 * \param[in] p_queue      要销毁的 USB 视频类驱动缓存队列
 * \param[in] n_stream_max 最大支持的 USB 视频类设备数据流数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_queue_destory(struct usbh_vc_queue *p_queue, uint8_t n_stream_max){
#if USB_OS_EN
    int ret;
#endif

    if (p_queue == NULL) {
        return -USB_EINVAL;
    }

    for (int i = 0;i < n_stream_max;i++) {
#if USB_OS_EN
        if (p_queue[i].p_lock) {
            ret = usb_lib_mutex_destroy(&__g_uvc_lib.lib, p_queue[i].p_lock);
            if (ret != USB_OK) {
                __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
                return ret;
            }
        }
#endif
    }
    usb_lib_mfree(&__g_uvc_lib.lib, p_queue);
    return USB_OK;
}
