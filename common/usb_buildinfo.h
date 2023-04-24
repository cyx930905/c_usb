#ifndef __USB_BUILDTIME_H__
#define __USB_BUILDTIME_H__

#ifdef __cplusplus
extern "C" {
#endif

#define USBH_BUILD_DATE    "UNKNOWN"
#define USBH_VERSION       "UNKNOWN"


/* \brief USB 库信息结构体*/
struct usb_lib_info {
    char version[32];
    char build_date[32];
};

/**
 * \brief 获取 USB 库信息
 *
 * \param[out] p_usb_info 返回的 USB 库信息
 */
void usb_lib_info_get(struct usb_lib_info *p_usb_info);

#ifdef __cplusplus
}
#endif



#endif
