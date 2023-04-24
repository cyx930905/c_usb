#ifndef __USBH_WIRELESS_PHYSICAL_H
#define __USBH_WIRELESS_PHYSICAL_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
//#include "core/include/host/class/net/wireless/cfg80211.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct cfg80211_opts {

};

/* \brief USB 主机无线物理层结构体*/
struct usbh_wireless_phy {
    const struct cfg80211_opts *p_opts;
    void                       *p_priv;
};

/**
 * \brief  USB 主机无线物理层私有数据获取
 */
static inline void *usbh_wireless_phy_priv_get(struct usbh_wireless_phy *p_wi_phy) {
    return p_wi_phy->p_priv;
}

/**
 * \brief  USB 主机无线物理层私有数据设置
 */
static inline void usbh_wireless_phy_priv_set(struct usbh_wireless_phy *p_wi_phy, void *p_priv) {
    p_wi_phy->p_priv = p_priv;
}

/**
 * \brief USB 主机无线物理层创建
 *
 * \param[in] p_opts 80211 操作函数集
 * \param[in] priv_size    私有数据大小
 *
 * \retval 成功返回 USB_OK
 */
struct usbh_wireless_phy *usbh_wireless_phy_create(const struct cfg80211_opts *p_opts);

#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif

