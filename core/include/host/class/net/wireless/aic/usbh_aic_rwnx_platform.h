#ifndef __USBH_AIC_RWNX_PLATFORM_H
#define __USBH_AIC_RWNX_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "common/list/usb_list.h"
#include "common/refcnt/usb_refcnt.h"
#include "core/include/host/core/usbh.h"
#include "core/include/host/core/usbh_dev.h"
#include "core/include/host/class/net/wireless/aic/usbh_aic_rwnx_cmd.h"
#include "core/include/host/class/net/wireless/usbh_wireless_physical.h"
#include "core/include/host/class/net/wireless/aic/usbh_aic_wifi_drv.h"

struct usbh_aic_rwnx_hw {
    struct usbh_aic_wifi      *p_aic_wifi;
    struct usbh_wireless_phy  *p_wi_phy;
    struct usbh_aic_rwnx_plat *p_rwnx_plat;
    struct rwnx_cmd_mgr       *p_cmd_mgr;
};


struct usbh_aic_rwnx_plat {
    struct usbh_aic_wifi *p_aic_wifi;
    usb_bool_t            is_enabled;
};







#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif /* __USBH_AIC_RWNX_PLATFORM_H */


