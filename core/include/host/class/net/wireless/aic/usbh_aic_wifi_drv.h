#ifndef __USBH_AIC_WIFI_DRV_H
#define __USBH_AIC_WIFI_DRV_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "core/include/host/class/net/usbh_net.h"
#include "core/include/host/class/net/wireless/aic/usbh_aic_rwnx_platform.h"
#include "core/include/host/class/net/wireless/aic/usbh_aic_rwnx_cmd.h"
#include "core/include/host/class/net/wireless/usbh_wireless_physical.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define USB_VENDOR_ID_AIC         0xA69C
#define USB_PRODUCT_ID_AIC8801    0x8801
#define USB_PRODUCT_ID_AIC8800    0x8800
#define USB_PRODUCT_ID_AIC8800DC  0x88dc


/* \brief AIC WIFI 芯片定义*/
enum AIC_WIFI_IC {
    PRODUCT_ID_AIC8801 = 0,
    PRODUCT_ID_AIC8800DC,
    PRODUCT_ID_AIC8800DW,
    PRODUCT_ID_AIC8800D80
};

/* \brief USB 主机 AIC WIFI 设备结构体*/
struct usbh_aic_wifi {
    struct usbh_function      *p_usb_fun;
    char                       name[USB_NAME_LEN];
    uint16_t                   chip_id;
    int                        ref_cnt;
    usb_bool_t                 is_bt_used;    /* 是否使用蓝牙*/
    struct usbh_endpoint      *p_ep_in;
    struct usbh_endpoint      *p_ep_out;
//    struct usbh_aic_rwnx_plat *p_aic_rwnx_plat;
//    struct usbh_aic_rwnx_hw   *p_rwnx_hw;
    //struct rwnx_cmd_mgr        cmd_mgr;
};




#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif /* __USBH_AIC_WIFI_DRV_H */

