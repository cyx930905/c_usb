#ifndef __USBH_CDC_ECM_DRV_H
#define __USBH_CDC_ECM_DRV_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "common/list/usb_list.h"
#include "common/refcnt/usb_refcnt.h"
#include "core/include/host/core/usbh.h"
#include "core/include/host/core/usbh_dev.h"
#include "core/include/specs/usb_cdc_specs.h"
#include "core/include/host/class/net/usbh_net.h"
#include <string.h>
#include <stdio.h>

/**
 * \brief USB 主机 ECM 设备探测函数
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ecm_probe(struct usbh_function *p_usb_fun);
/**
 * \brief USB 主机 ECM 设备创建函数
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 * \param[in] p_name    USB 主机 ECM 设备名字
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ecm_create(struct usbh_function *p_usb_fun, char *p_name);
/**
 * \brief  USB 主机 ECM 设备销毁函数
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ecm_destroy(struct usbh_function *p_usb_fun);
/**
 * \brief USB 主机 ECM 设备打开函数
 *
 * \param[in]  p_handle  打开句柄
 * \param[in]  flag      打开标志，本接口支持两种打开方式：
 *                       USBH_DEV_OPEN_BY_NAME 是通过名字打开设备
 *                       USBH_DEV_OPEN_BY_UFUN 是通过 USB 功能结构体打开设备
 * \param[out] p_net_ret 成功返回 USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ecm_open(void             *p_handle,
                  uint8_t           flag,
                  struct usbh_net **p_net_ret);
/**
 * \brief USB 主机 ECM 设备关闭函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ecm_close(struct usbh_net *p_net);
/**
 * \brief USB 主机 ECM 设备启动函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ecm_start(struct usbh_net *p_net);
/**
 * \brief USB 主机 ECM 设备停止函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ecm_stop(struct usbh_net *p_net);
/**
 * \brief USB 主机 ECM 设备写函数
 *
 * \param[in] p_net   USB 主机网络设备
 * \param[in] p_buf   写缓存
 * \param[in] buf_len 写缓存长度
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ecm_write(struct usbh_net *p_net, uint8_t *p_buf, uint32_t buf_len);
/**
 * \brief USB 主机 ECM 设备读函数
 *
 * \param[in] p_net   USB 主机网络设备
 * \param[in] p_buf   读缓存
 * \param[in] buf_len 读缓存长度
 *
 * \retval 成功返回读到的数据长度
 */
int usbh_ecm_read(struct usbh_net *p_net, uint8_t *p_buf, uint32_t buf_len);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif /* __USBH_CDC_ECM_DRV_H */

