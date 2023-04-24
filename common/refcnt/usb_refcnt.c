/*******************************************************************************
 * Includes
*******************************************************************************/
#include "common/refcnt/usb_refcnt.h"
#include "common/err/usb_err.h"

/*******************************************************************************
 * Static
*******************************************************************************/
#if USB_OS_EN
/* \brief 引用计数互斥锁*/
static usb_mutex_handle_t __g_usb_ref_cnt_lock = NULL;
#endif
/* \brief 是否初始化完成互斥锁标志*/
static usb_bool_t         __g_is_lock_init     = USB_FALSE;

/*******************************************************************************
 * Code
*******************************************************************************/
/**
 * \brief 初始化引用计数
 *
 * \param[in]  p_ref 引用计数对象指针
 */
void usb_refcnt_init(int *p_ref){

    if(__g_is_lock_init == USB_FALSE){
#if USB_OS_EN
        __g_usb_ref_cnt_lock = usb_mutex_create();
        if (__g_usb_ref_cnt_lock == NULL) {
            __USB_ERR_TRACE(MutexCreateErr, "\r\n");
            return;
        }
#endif
        __g_is_lock_init = USB_TRUE;
    }

    if(p_ref == NULL){
        return;
    }

    *p_ref = 1;
}

/**
 * \brief 检查引用计数是否有效。
 *
 * \param[in] p_ref 引用计数对象指针
 */
usb_bool_t usb_refcnt_valid(int *p_ref){
	if (p_ref == NULL) {
        return USB_FALSE;
	}

    return (*p_ref != 0);
}

/**
 * \brief 引用计数加。
 *
 * \param[in] p_ref 引用计数对象指针
 *
 * \retval 成功返回USB_OK
 */
int usb_refcnt_get(int *p_ref){
    int ret = USB_OK;

    if (p_ref == NULL) {
        return -USB_EINVAL;
	}
    if (__g_is_lock_init == USB_FALSE) {
        return -USB_ENOINIT;
	}

#if USB_OS_EN
    ret = usb_mutex_lock(__g_usb_ref_cnt_lock, USB_WAIT_FOREVER);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (*p_ref == 0) {
#if USB_OS_EN
        ret = usb_mutex_unlock(__g_usb_ref_cnt_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
        return -USB_EPERM;
    }
    *p_ref += 1;
#if USB_OS_EN
    ret = usb_mutex_unlock(__g_usb_ref_cnt_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 引用计数减
 *
 * \param[in]  p_ref        引用计数对象指针
 * \param[in]  p_fn_release 回调函数，引用计数释放后执行
 *
 * \retval 成功返回 USB_OK
 */
int usb_refcnt_put(int   *p_ref,
                   void (*p_fn_release)(int *p_ref)){
    int ret = USB_OK;

    if (p_ref == NULL) {
        return -USB_EINVAL;
	}
    if (__g_is_lock_init == USB_FALSE) {
        return -USB_ENOINIT;
	}

#if USB_OS_EN
    ret = usb_mutex_lock(__g_usb_ref_cnt_lock, USB_WAIT_FOREVER);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (*p_ref == 0) {
#if USB_OS_EN
        ret = usb_mutex_unlock(__g_usb_ref_cnt_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
        return -USB_EPERM;
    }
    *p_ref -= 1;
    if ((*p_ref == 0) && (p_fn_release)) {
#if USB_OS_EN
        ret = usb_mutex_unlock(__g_usb_ref_cnt_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
        p_fn_release(p_ref);
        return USB_OK;
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(__g_usb_ref_cnt_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}


