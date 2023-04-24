#ifndef __USB_REFCNT_H
#define __USB_REFCNT_H

#include "common/usb_common.h"
#include "common/err/usb_err.h"
#include "adapter/usb_adapter.h"

/**
 * \brief 初始化引用计数
 *
 * \param[in] p_ref 引用计数对象指针
 */
void usb_refcnt_init(int *p_ref);
/**
 * \brief 检查引用计数是否有效。
 *
 * \param[in] p_ref 引用计数对象指针
 */
usb_bool_t usb_refcnt_valid(int *p_ref);
/**
 * \brief 引用计数加。
 *
 * \param[in] p_ref 引用计数对象指针
 *
 * \retval 成功返回 USB_OK
 */
int usb_refcnt_get(int *p_ref);
/**
 * \brief 引用计数减
 *
 * \param[in]  p_ref        引用计数对象指针
 * \param[in]  p_fn_release 回调函数，引用计数释放后执行
 *
 * \retval 成功返回 USB_OK
 */
int usb_refcnt_put(int   *p_ref,
                   void (*p_fn_release)(int *p_ref));


#endif

