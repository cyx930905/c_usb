/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "common/err/usb_err.h"
#include "common/usb_buildinfo.h"
#include <string.h>
#include <stdio.h>

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 获取 USB 库信息
 *
 * \param[out] p_usb_info 返回的 USB 库信息
 */
void usb_lib_info_get(struct usb_lib_info *p_usb_info){
    if (p_usb_info != NULL) {
        memset(p_usb_info, 0, sizeof(struct usb_lib_info));
        strcpy(p_usb_info->version,    USBH_VERSION);
        strcpy(p_usb_info->build_date, USBH_BUILD_DATE);
    }
}
