/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include "usb_notify.h"

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief USB 主机设备接口功能状态通知函数
 *
 * \param[in] p_usb_func USB 主机接口功能
 * \param[in] status     状态 
 *                       USBH_DEV_INJECT 设备插入
 *                       USBH_DEV_EJECT  设备拔出
 *
 * \retval 成功返回 USB_OK
 */
__attribute__((weak)) int usbh_dev_func_sta_notify(struct usbh_function *p_usb_func, uint8_t status){
    return USB_OK;
}

