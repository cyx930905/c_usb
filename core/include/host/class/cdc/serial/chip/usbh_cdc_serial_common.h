#ifndef __USBH_CDC_SERIAL_DEFAULT_H
#define __USBH_CDC_SERIAL_DEFAULT_H

#include "core/include/host/class/cdc/serial/usbh_cdc_serial_drv.h"

#define COMMON_N_TRP 4

/* \brief USB 主机转串口通用设备*/
struct usbh_serial_common {
    struct usbh_serial   *p_userial;
    struct usbh_endpoint *p_data_ep_rd;
    struct usbh_endpoint *p_data_ep_wr;
    uint32_t              rd_buf_size;
    uint8_t              *p_rd_buf[COMMON_N_TRP];
    struct usbh_trp       trp_rd[COMMON_N_TRP];
};

/**
 * \brief USB 转串口通用设备初始化
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_common_init(struct usbh_serial *p_userial);
/**
 * \brief USB 转串口通用设备反初始化
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_common_deinit(struct usbh_serial *p_userial);
#endif
