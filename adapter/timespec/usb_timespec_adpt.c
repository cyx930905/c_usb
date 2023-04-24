/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "usb_timespec.h"
#include "config/usb_config.h"
#include "common/usb_common.h"

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 获取当前时间戳
 */
__attribute__((weak)) int usb_timespec_get(struct usb_timespec *p_ts){
    return -USB_ENOTSUP;
}

/**
 * \brief 通过纳秒获取时间戳
 */
__attribute__((weak)) struct usb_timespec usb_ns_to_timespec(const int64_t nsec){
}

