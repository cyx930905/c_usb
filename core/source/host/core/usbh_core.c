/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "common/err/usb_err.h"
#include "common/refcnt/usb_refcnt.h"
#include "core/include/host/core/usbh.h"
#include "core/include/specs/usb_specs.h"
#include <string.h>
#include <stdio.h>

/*******************************************************************************
 * Global
 ******************************************************************************/
/* \brief 声明一个 USB 主机库结构体*/
struct usbh_core_lib __g_usb_host_lib;

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 缓存 DMA 映射
 */
static int __trp_buf_map(struct usbh_trp *p_trp){
    uint8_t dir;

    p_trp->p_ctrl_dma = NULL;
    p_trp->p_data_dma = NULL;

    if (p_trp->p_ctrl) {
        /* 把控制请求包回写到 DMA 传输的内存中(发送 SETUP 令牌包)*/
        p_trp->p_ctrl_dma = usb_dma_map(p_trp->p_ctrl,
                                        sizeof(struct usb_ctrlreq),
                                        USB_DMA_TO_DEVICE);
        if (p_trp->p_ctrl_dma == NULL) {
            __USB_ERR_INFO("control dma map failed\r\n");
            return -USB_EAGAIN;
        }

        /* 如果控制请求包为输入端点，设置DMA数据方向为从设备接收*/
        if (p_trp->p_ctrl->request_type & USB_DIR_IN) {
            dir = USB_DMA_FROM_DEVICE;
        } else {
            dir = USB_DMA_TO_DEVICE;
        }
    } else {
        if ((p_trp->p_ep == NULL) || (p_trp->p_ep->p_ep_desc == NULL)) {
            return -USB_EILLEGAL;
        }

        /* 没有控制请求包，判断传输请求包的端点方向*/
        if (USBH_EP_DIR_GET(p_trp->p_ep) == USB_DIR_IN) {
            dir = USB_DMA_FROM_DEVICE;
        } else {
            dir = USB_DMA_TO_DEVICE;
        }
    }
    /* 检查是否有异常*/
    if (((p_trp->p_data == NULL) && (p_trp->len)) ||
            ((p_trp->p_data != NULL) && (p_trp->len == 0))) {
        return -USB_EILLEGAL;
    }

    /* 映射数据过程*/
    if ((p_trp->p_data) && (p_trp->len)) {
        p_trp->p_data_dma = usb_dma_map(p_trp->p_data,
                                        p_trp->len,
                                        dir);
        if (p_trp->p_data_dma == NULL) {
            __USB_ERR_INFO("data dma map failed\r\n");
            return -USB_EAGAIN;
        }
    }

    return USB_OK;
}

/**
 * \brief 取消 DMA 映射
 */
static void __trp_buf_unmap(struct usbh_trp *p_trp){
    uint8_t dir;

    if (p_trp->p_ctrl) {
        if (p_trp->p_ctrl->request_type & USB_DIR_IN) {
            dir = USB_DMA_FROM_DEVICE;
        } else {
            dir = USB_DMA_TO_DEVICE;
        }
    } else {
        if ((p_trp->p_ep != NULL) && (p_trp->p_ep->p_ep_desc != NULL)) {
            if (USBH_EP_DIR_GET(p_trp->p_ep) == USB_DIR_IN) {
                dir = USB_DMA_FROM_DEVICE;
            } else {
                dir = USB_DMA_TO_DEVICE;
            }
        }
    }
    /* 取消映射控制传输*/
    if (p_trp->p_ctrl_dma) {
        usb_dma_unmap(p_trp->p_ctrl_dma,
                      sizeof(struct usb_ctrlreq),
                      USB_DMA_TO_DEVICE);
    }
    /* 取消映射数据缓存*/
    if (p_trp->p_data_dma) {
        usb_dma_unmap(p_trp->p_data_dma,
                      p_trp->len,
                      dir);
    }
}

/**
 * \brief USB 主机库释放函数
 */
static void __lib_release(int *p_ref){
    usb_lib_deinit(&__g_usb_host_lib.lib);
    usbh_dev_lib_deinit();
}

/**
 * \brief USB 主机库初始化
 *
 * \retval 成功返回 USB_OK
 */
int usbh_core_lib_init(void){
    int ret;

    if (usb_lib_is_init(&__g_usb_host_lib.lib) == USB_TRUE) {
        return USB_OK;
    }

#if USB_MEM_RECORD_EN
    ret = usb_lib_init(&__g_usb_host_lib.lib, "usb_hc_mem");
#else
    ret = usb_lib_init(&__g_usb_host_lib.lib);
#endif
    if (ret != USB_OK) {
        return ret;
    }

    ret = usbh_dev_lib_init();
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB host device lib init failed(%d)", ret);
        goto __failed;
    }

    /* 初始化引用计数*/
    usb_refcnt_init(&__g_usb_host_lib.ref_cnt);

    __g_usb_host_lib.is_lib_deiniting = USB_FALSE;

    return USB_OK;
__failed:
    usb_lib_deinit(&__g_usb_host_lib.lib);
    return ret;
}

/**
 * \brief USB 主机库反初始化
 *
 * \retval 成功返回 USB_OK
 */
int usbh_core_lib_deinit(void){
    if (usb_lib_is_init(&__g_usb_host_lib.lib) == USB_FALSE) {
        return USB_OK;
    }
    __g_usb_host_lib.is_lib_deiniting = USB_TRUE;
    return usb_refcnt_put(&__g_usb_host_lib.ref_cnt, __lib_release);
}

/**
 * \brief 传输请求包完成回调函数
 */
void usbh_trp_done(struct usbh_trp *p_trp){
    __trp_buf_unmap(p_trp);

    if (p_trp->p_fn_done) {
        p_trp->p_fn_done(p_trp->p_arg);
    }
}

/**
 * \brief 通过索引获取 USB 主机
 *
 * \param[in]  host_idx USB 主机索引
 * \param[out] p_hc     USB 主机地址
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_get(uint8_t host_idx, struct usb_hc **p_hc){
#if USB_OS_EN
    int                   ret;
#endif
    struct usb_list_node *p_node     = NULL;
    struct usb_list_node *p_node_tmp = NULL;
    struct usb_hc        *p_hc_find  = NULL;

    if (usb_lib_is_init(&__g_usb_host_lib.lib) == USB_FALSE) {
        return -USB_ENOINIT;
    }

#if USB_OS_EN
    ret = usb_lib_mutex_lock(&__g_usb_host_lib.lib);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    USB_LIB_LIST_FOR_EACH_NODE(p_node, p_node_tmp, &__g_usb_host_lib.lib){
        p_hc_find = usb_container_of(p_node, struct usb_hc, node);

        if (p_hc_find->host_idx == host_idx) {
#if USB_OS_EN
            ret = usb_lib_mutex_unlock(&__g_usb_host_lib.lib);
            if (ret != USB_OK) {
                __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
                return ret;
            }
#endif
            *p_hc = p_hc_find;
            return USB_OK;
        }
    }
#if USB_OS_EN
    ret = usb_lib_mutex_unlock(&__g_usb_host_lib.lib);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    return -USB_ENODEV;
}

/**
 * \brief 设置 USB 主机用户私有数据
 *
 * \param[in] p_hc       USB 主机结构体
 * \param[in] p_usr_data 用户私有数据
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_usrdata_set(struct usb_hc *p_hc, void *p_usr_data){
    int ret = USB_OK;

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_hc->p_usr_priv = p_usr_data;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_hc->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 获取 USB 主机用户私有数据
 *
 * \param[in]  p_hc       USB 主机结构体
 * \param[out] p_usr_priv 获取到的用户私有数据
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_usrdata_get(struct usb_hc *p_hc, void **p_usr_priv){
    int ret = USB_OK;

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_usr_priv = p_hc->p_usr_priv;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_hc->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 设置 USB 主机控制器
 *
 * \param[in] p_hc         USB 主机结构体
 * \param[in] p_controller 具体的主机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_host_controller_set(struct usb_hc *p_hc, void *p_controller){
    int ret = USB_OK;

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }
    if (p_hc->is_init != USB_TRUE) {
        return -USB_ENOINIT;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_controller != NULL) {
        struct usb_hc_head *p_hc_head = (struct usb_hc_head *)p_controller;
        /* 检查控制器是否合法*/
        if (p_hc_head->controller_type > CUSTOM) {
#if USB_OS_EN
            ret = usb_mutex_unlock(p_hc->p_lock);
            if (ret != USB_OK) {
                 __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            }
#endif
            return -USB_EILLEGAL;
        }
    }
    p_hc->p_controller = p_controller;

#if USB_OS_EN
    ret = usb_mutex_unlock(p_hc->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 获取 USB 主机控制器
 *
 * \param[in]  p_hc         USB 主机结构体
 * \param[out] p_controller 返回的 USB 主机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_host_controller_get(struct usb_hc *p_hc, void **p_controller){
    int ret = USB_OK;

    if ((p_hc == NULL) || (p_controller == NULL)) {
        return -USB_EINVAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_controller = p_hc->p_controller;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_hc->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机初始化控制器
 */
static int __hc_init(struct usb_hc *p_hc, uint8_t host_idx){
    p_hc->host_idx = host_idx;
    usb_list_node_init(&p_hc->node);
#if USB_OS_EN
    p_hc->p_lock = usb_lib_mutex_create(&__g_usb_host_lib.lib);
    if (p_hc->p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        return -USB_EPERM;
    }
#endif
    /* 设置已初始化标志*/
    p_hc->is_init = USB_TRUE;

    return USB_OK;
}

/**
 * \brief USB 主机反初始化控制器
 */
static int __hc_deinit(struct usb_hc *p_hc){
#if USB_OS_EN
    int ret = usb_lib_mutex_destroy(&__g_usb_host_lib.lib, p_hc->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 设置已初始化标志*/
    p_hc->is_init = USB_FALSE;

    return USB_OK;
}

/**
 * \brief 创建 USB 主机控制器
 *
 * \param[in]  host_idx 主机结构体索引
 * \param[out] p_hc     创建成功的 USB 主机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_create(uint8_t host_idx, struct usb_hc **p_hc){
    int            ret;
    struct usb_hc *p_hc_tmp = NULL;

    /* USB 主机库没有初始化或者 USB 主机库正在被卸载*/
    if ((usb_lib_is_init(&__g_usb_host_lib.lib) == USB_FALSE) ||
            (__g_usb_host_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }

    /* 是否有相同索引的主机控制器*/
    ret = usb_hc_get(host_idx, &p_hc_tmp);
    if ((ret != USB_OK) && (ret != -USB_ENODEV)) {
        return ret;
    }

    if (ret == USB_OK) {
        __USB_ERR_INFO("have same usb host index\r\n");
        return -USB_EPERM;
    }

    p_hc_tmp = usb_lib_malloc(&__g_usb_host_lib.lib, sizeof(struct usb_hc));
    if (p_hc == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_hc_tmp, 0, sizeof(struct usb_hc));

    ret = __hc_init(p_hc_tmp, host_idx);
    if (ret != USB_OK) {
        goto __failed1;
    }

    /* 往库中添加一个设备*/
    ret = usb_lib_dev_add(&__g_usb_host_lib.lib, &p_hc_tmp->node);
    if (ret != USB_OK) {
        goto __failed2;
    }

    ret = usb_refcnt_get(&__g_usb_host_lib.ref_cnt);
    if (ret != USB_OK) {
        goto __failed3;
    }

    *p_hc = p_hc_tmp;

    return USB_OK;
__failed3:
    usb_lib_dev_del(&__g_usb_host_lib.lib, &p_hc_tmp->node);
__failed2:
    __hc_deinit(p_hc_tmp);
__failed1:
    usb_lib_mfree(&__g_usb_host_lib.lib, p_hc_tmp);

    return ret;
}

/**
 * \brief 销毁 USB 主机控制器
 *
 * \param[in] p_hc 要销毁的主机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_destory(struct usb_hc *p_hc){
    int ret;

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }

    ret = usb_lib_dev_del(&__g_usb_host_lib.lib, &p_hc->node);
    if (ret != USB_OK) {
        return ret;
    }

    ret = __hc_deinit(p_hc);
    if (ret != USB_OK) {
        return ret;
    }
    usb_lib_mfree(&__g_usb_host_lib.lib, p_hc);

    ret = usb_refcnt_put(&__g_usb_host_lib.ref_cnt, __lib_release);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
    }

    return ret;
}

/**
 * \brief 启动 USB 主机控制器
 *
 * \param[in] p_hc 要启动的主机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_start(struct usb_hc *p_hc){
    int                 ret;
    struct usb_hc_head *p_hc_head = NULL;
#if USB_OS_EN
    int                 ret_tmp;
#endif

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }
    if (p_hc->is_init != USB_TRUE) {
        return -USB_ENOINIT;
    }

    /* 获取主机控制器头*/
    ret = usb_host_controller_get(p_hc, (void **)&p_hc_head);
    if (ret != USB_OK) {
        return ret;
    }

    if ((p_hc_head->p_controller_drv == NULL) ||
            (p_hc_head->p_controller_drv->p_fn_hc_start == NULL)) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    ret = p_hc_head->p_controller_drv->p_fn_hc_start(p_hc);

#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hc->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 停止 USB 主机控制器
 *
 * \param[in] p_hc 要停止的主机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_stop(struct usb_hc *p_hc){
    int                 ret;
    struct usb_hc_head *p_hc_head = NULL;
#if USB_OS_EN
    int                 ret_tmp;
#endif

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }
    if (p_hc->is_init != USB_TRUE) {
        return -USB_ENOINIT;
    }

    /* 获取主机控制器头*/
    ret = usb_host_controller_get(p_hc, (void **)&p_hc_head);
    if (ret != USB_OK) {
        return ret;
    }

    if ((p_hc_head->p_controller_drv == NULL) ||
            (p_hc_head->p_controller_drv->p_fn_hc_stop == NULL)) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    ret = p_hc_head->p_controller_drv->p_fn_hc_stop(p_hc);

#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hc->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 挂起 USB 主机控制器
 *
 * \param[in] p_hc 要挂起的主机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_suspend(struct usb_hc *p_hc){
    int                 ret;
    struct usb_hc_head *p_hc_head = NULL;
#if USB_OS_EN
    int                 ret_tmp;
#endif

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }
    if (p_hc->is_init != USB_TRUE) {
        return -USB_ENOINIT;
    }

    /* 获取主机控制器头*/
    ret = usb_host_controller_get(p_hc, (void **)&p_hc_head);
    if (ret != USB_OK) {
        return ret;
    }

    if ((p_hc_head->p_controller_drv == NULL) ||
            (p_hc_head->p_controller_drv->p_fn_hc_suspend == NULL)) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    ret = p_hc_head->p_controller_drv->p_fn_hc_suspend(p_hc);

#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hc->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 恢复 USB 主机控制器
 *
 * \param[in] p_hc 要恢复的主机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_resume(struct usb_hc *p_hc){
    int                 ret;
    struct usb_hc_head *p_hc_head = NULL;
#if USB_OS_EN
    int                 ret_tmp;
#endif

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }
    if (p_hc->is_init != USB_TRUE) {
        return -USB_ENOINIT;
    }

    /* 获取主机控制器头*/
    ret = usb_host_controller_get(p_hc, (void **)&p_hc_head);
    if (ret != USB_OK) {
        return ret;
    }

    if ((p_hc_head->p_controller_drv == NULL) ||
            (p_hc_head->p_controller_drv->p_fn_hc_resume == NULL)) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    ret = p_hc_head->p_controller_drv->p_fn_hc_resume(p_hc);

#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hc->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 根集线器初始化
 *
 * \param[in]  p_hc       USB 主机结构体
 * \param[out] p_n_ports  返回根集线器端口数量
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_rh_init(struct usb_hc *p_hc, uint8_t *p_n_ports){
    int                 ret;
    struct usb_hc_head *p_hc_head = NULL;
#if USB_OS_EN
    int                 ret_tmp;
#endif

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }
    if (p_hc->is_init != USB_TRUE) {
        return -USB_ENOINIT;
    }

    /* 获取主机控制器头*/
    ret = usb_host_controller_get(p_hc, (void **)&p_hc_head);
    if (ret != USB_OK) {
        return ret;
    }

    if ((p_hc_head->p_controller_drv == NULL) ||
            (p_hc_head->p_controller_drv->p_fn_rh_init == NULL)) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    ret = p_hc_head->p_controller_drv->p_fn_rh_init(p_hc);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb host %d root hub init failed(%d)\r\n", p_hc->host_idx, ret);
        goto __exit;
    }

    *p_n_ports = p_hc->root_hub.n_ports;

    usb_list_node_init(&p_hc->root_hub.evt.evt_node);

    p_hc->root_hub.p_ports = usb_lib_malloc(&__g_usb_host_lib.lib, sizeof(void *) * p_hc->root_hub.n_ports);
    if (p_hc->root_hub.p_ports == NULL) {
        ret = -USB_ENOMEM;
        goto __exit;
    }
    memset(p_hc->root_hub.p_ports, 0, sizeof(void *) * p_hc->root_hub.n_ports);

    p_hc->root_hub.hub_status = USBH_HUB_RUNNING;

__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hc->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 复位根集线器
 *
 * \param[in] p_hc     USB 主机控制器结构体
 * \param[in] is_port  是否是端口
 * \param[in] port_num 端口号
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_rh_reset(struct usb_hc *p_hc, usb_bool_t is_port, uint8_t port_num){
    int ret = USB_OK;
#if USB_OS_EN
    int ret_tmp;
#endif

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }
    if (p_hc->is_init != USB_TRUE) {
        return -USB_ENOINIT;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_hc->root_hub.p_fn_hub_reset) {
        ret = p_hc->root_hub.p_fn_hub_reset(&p_hc->root_hub, is_port, port_num);
    }
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hc->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;

}

/**
 * \brief USB 主机设备地址分配
 *
 * \param[in]  p_hc   USB 主机
 * \param[out] p_addr 返回分配的地址
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_dev_addr_alloc(struct usb_hc *p_hc, uint8_t *p_addr){
    uint8_t     mutl, offs;
    uint32_t   *p_map = p_hc->map;
    int         ret;

#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    /* 分配机制：4 个 32 位的数组，总共有 128 个位，每一个位代表一个设备，
     * 设备地址范围为 1~128 */
    for (mutl = 0; mutl < 4; mutl++) {
        offs = 0;
        while ((p_map[mutl] & (1u << offs)) && (offs < 32)) {
            offs++;
        }

        if (offs < 32) {
            p_map[mutl] |= (1u << offs);
            *p_addr = mutl * 32 + offs + 1;
            break;
        }
    }

    if ((mutl == 4) && (offs == 32)) {
        ret = -USB_ENOMEM;
    }

#if USB_OS_EN
    ret = usb_mutex_unlock(p_hc->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);

        p_hc->map[mutl] &= ~(1u << offs);
        *p_addr = 0;
    }
#endif
    return ret;
}

/**
 * \brief USB 主机设备地址释放
 *
 * \param[in] p_hc USB 主机
 * \param[in] addr 要释放的地址
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_dev_addr_free(struct usb_hc *p_hc, uint8_t addr){
    uint8_t mutl, offs;
    int     ret = USB_OK;

#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 地址减 1（申请时加了 1）*/
    addr--;
    /* 确定数组元素*/
    mutl = addr / 32;
    /* 确定位偏移*/
    offs = addr % 32;
    /* 相应位清0*/
    p_hc->map[mutl] &= ~(1u << offs);
#if USB_OS_EN
    ret = usb_mutex_unlock(p_hc->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 控制器设备端点使能
 *
 * \param[in] p_hc USB 主机结构体
 * \param[in] p_ep USB 设备端点
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_ep_enable(struct usb_hc        *p_hc,
                     struct usbh_endpoint *p_ep){
    int                 ret;
    struct usb_hc_head *p_hc_head = NULL;
#if USB_OS_EN
    int                 ret_tmp;
#endif

    /* 获取主机控制器头*/
    ret = usb_host_controller_get(p_hc, (void **)&p_hc_head);
    if (ret != USB_OK) {
        return ret;
    }

    if ((p_hc_head->p_controller_drv == NULL) ||
            (p_hc_head->p_controller_drv->p_fn_ep_enable == NULL)) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    ret = p_hc_head->p_controller_drv->p_fn_ep_enable(p_hc, p_ep);

#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hc->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 控制器设备取消端点使能
 *
 * \param[in] p_hc USB 主机结构体
 * \param[in] p_ep USB 设备端点
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_ep_disable(struct usb_hc        *p_hc,
                      struct usbh_endpoint *p_ep){
    int                 ret;
    struct usb_hc_head *p_hc_head = NULL;
#if USB_OS_EN
    int                 ret_tmp;
#endif

    /* 获取主机控制器头*/
    ret = usb_host_controller_get(p_hc, (void **)&p_hc_head);
    if (ret != USB_OK) {
        return ret;
    }

    if ((p_hc_head->p_controller_drv == NULL) ||
            (p_hc_head->p_controller_drv->p_fn_ep_disable == NULL)) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    ret = p_hc_head->p_controller_drv->p_fn_ep_disable(p_hc, p_ep);

#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hc->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 控制器传输请求
 *
 * \param[in] p_hc  USB 主机结构体
 * \param[in] p_trp USB 传输请求包
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_xfer_request(struct usb_hc   *p_hc,
                        struct usbh_trp *p_trp){
    int                 ret       = USB_OK;
    struct usb_hc_head *p_hc_head = NULL;
#if USB_OS_EN
    int                 ret_tmp;
#endif

    /* 获取主机控制器头*/
    ret = usb_host_controller_get(p_hc, (void **)&p_hc_head);
    if (ret != USB_OK) {
        return ret;
    }

    if ((p_hc_head->p_controller_drv == NULL) ||
            (p_hc_head->p_controller_drv->p_fn_xfer_request == NULL)) {
        return -USB_EILLEGAL;
    }

    /* 内存 DMA 映射*/
    ret = __trp_buf_map(p_trp);
    if (ret != USB_OK) {
        __USB_ERR_INFO("trp dma map failed(%d)", ret);
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    ret = p_hc_head->p_controller_drv->p_fn_xfer_request(p_hc, p_trp);

#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hc->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 主机取消函数
 *
 * \param[in] p_hc  USB 主机结构体
 * \param[in] p_trp 要取消的传输请求包
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_xfer_cancel(struct usb_hc   *p_hc,
                       struct usbh_trp *p_trp){
    int                 ret       = USB_OK;
    struct usb_hc_head *p_hc_head = NULL;
#if USB_OS_EN
    int                 ret_tmp;
#endif

    /* 获取主机控制器头*/
    ret = usb_host_controller_get(p_hc, (void **)&p_hc_head);
    if (ret != USB_OK) {
        return ret;
    }

    if ((p_hc_head->p_controller_drv == NULL) ||
            (p_hc_head->p_controller_drv->p_fn_xfer_cancel == NULL)) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 调用控制器传输取消函数*/
    ret = p_hc_head->p_controller_drv->p_fn_xfer_cancel(p_hc, p_trp);

#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hc->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 获取 USB 主机当前帧编号
 *
 * \param[in] p_hc USB 主机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_frame_num_get(struct usb_hc *p_hc){
    int                 ret;
#if USB_OS_EN
    int                 ret_tmp;
#endif
    struct usb_hc_head *p_hc_head = NULL;

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }
    if (p_hc->is_init != USB_TRUE) {
        return -USB_ENOINIT;
    }
    if (p_hc->p_controller == NULL) {
        return -USB_EILLEGAL;
    }

    /* 获取主机控制器头*/
    ret = usb_host_controller_get(p_hc, (void **)&p_hc_head);
    if (ret != USB_OK) {
        return ret;
    }

    if ((p_hc_head->p_controller_drv == NULL) ||
            (p_hc_head->p_controller_drv->p_fn_frame_num_get == NULL)) {
        return -USB_EILLEGAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_hc->p_lock, USB_HC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 调用控制器驱动函数*/
    ret = p_hc_head->p_controller_drv->p_fn_frame_num_get(p_hc);
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hc->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret_tmp;
    }
#endif

    return ret;
}

/**
 * \brief 获取 USB 主机内存记录
 *
 * \param[out] p_mem_record 返回内存记录
 *
 * \retval 成功返回 USB_OK
 */
int usb_hc_mem_record_get(struct usb_mem_record *p_mem_record){
    return usb_lib_mem_record_get(&__g_usb_host_lib.lib, p_mem_record);
}

