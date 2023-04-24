#ifndef __USBH_CDC_SERIAL_CH341_H
#define __USBH_CDC_SERIAL_CH341_H

#include "common/err/usb_err.h"
#include "core/include/host/class/cdc/serial/usbh_cdc_serial_drv.h"

#define CH341_N_TRP 4

/* \brief 波特率计算因子*/
#define CH341_BAUDBASE_FACTOR  1532620800
#define CH341_BAUDBASE_DIVMAX  3

#define CH341_BIT_RTS         (1 << 6)
#define CH341_BIT_DTR         (1 << 5)

#define CH341_BITS_MODEM_STAT  0x0f /* 所有位*/

/* \brief USB 转串口 ch341 串口结构体*/
struct usbh_serial_ch341 {
    struct usbh_serial   *p_userial;
    struct usbh_endpoint *p_cmd_ep;
    struct usbh_endpoint *p_data_ep_rd;
    struct usbh_endpoint *p_data_ep_wr;
    uint32_t              rd_buf_size;
    uint8_t              *p_rd_buf[CH341_N_TRP];
    struct usbh_trp       trp_rd[CH341_N_TRP];
    uint8_t               line_control;   /* 设置 line control value RTS/DTR */
    uint8_t               line_status;    /* modem 控制输入的激活的位*/
};

/**
 * \brief USB 转串口 ch341 芯片初始化
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_ch341_init(struct usbh_serial *p_userial);
/**
 * \brief USB 转串口 ch341 芯片反初始化函数
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_ch341_deinit(struct usbh_serial *p_userial);

#endif
