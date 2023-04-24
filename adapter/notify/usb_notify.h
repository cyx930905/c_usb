#ifndef __USB_NOTIFY_H
#define __USB_NOTIFY_H
#include "core/include/host/core/usbh_dev.h"
#include "common/err/usb_err.h"

/**
 * \brief USB 设备通知函数
 *
 * \param[in] p_usb_func USB 功能结构体
 * \param[in] type       通知类型 
 *                       USBH_DEV_INJECT 代表有设备插入
 *                       USBH_DEV_EJECT  代表有设备拔出
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_func_sta_notify(struct usbh_function *p_usb_func, uint8_t status);


#endif
