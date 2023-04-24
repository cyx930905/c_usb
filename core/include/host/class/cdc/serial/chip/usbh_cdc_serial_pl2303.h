#ifndef __USBH_CDC_SERIAL_PL2303_H
#define __USBH_CDC_SERIAL_PL2303_H

#include "core/include/host/class/cdc/serial/usbh_cdc_serial_drv.h"

#define PL2303_N_TRP                      8

#define PL2303_QUIRK_UART_STATE_IDX0     (1 << 0)
#define PL2303_QUIRK_LEGACY              (1 << 1)

#define PL2303_CONTROL_DTR               (0x01)
#define PL2303_CONTROL_RTS               (0x02)

#define PL2303_SET_CONTROL_REQUEST_TYPE  (0x21)
#define PL2303_SET_CONTROL_REQUEST       (0x22)
#define PL2303_SET_LINE_REQUEST_TYPE     (0x21)
#define PL2303_SET_LINE_REQUEST          (0x20)
#define PL2303_GET_LINE_REQUEST_TYPE     (0xa1)
#define PL2303_GET_LINE_REQUEST          (0x21)

#define PL2303_VENDOR_WRITE_REQUEST_TYPE (0x40)
#define PL2303_VENDOR_WRITE_REQUEST      (0x01)
#define PL2303_VENDOR_READ_REQUEST_TYPE  (0xc0)
#define PL2303_VENDOR_READ_REQUEST       (0x01)

/* \brief USB 转串口 pl2303 芯片类型*/
enum pl2303_type {
    TYPE_01,    /* 类型 0 和 1 (区别未知) */
    TYPE_HX,    /* pl2303 芯片的 HX 版本*/
    TYPE_COUNT
};

/* \brief USB 转串口 pl2303 类型数据*/
struct pl2303_type_data {
    uint32_t      max_baud_rate;
    unsigned long quirks;
};

/* \brief USB 转串口 pl2303 串口私有数据*/
struct pl2303_serial_private {
    const struct pl2303_type_data *p_type;
    unsigned long                  quirks;
};

/* \brief USB 转串口 pl2303 串口结构体*/
struct usbh_serial_pl2303 {
    struct usbh_serial           *p_userial;
    struct usbh_endpoint         *p_data_ep_rd;
    struct usbh_endpoint         *p_data_ep_wr;
    uint32_t                      rd_buf_size;
    uint8_t                      *p_rd_buf[PL2303_N_TRP];
    struct usbh_trp               trp_rd[PL2303_N_TRP];
    struct pl2303_serial_private  pl2303_priv;
    uint8_t                       line_settings[7];
    uint8_t                       line_control;
};

/**
 * \brief USB 转串口 pl2303 芯片初始化
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_pl2303_init(struct usbh_serial *p_userial);
/**
 * \brief USB 转串口 pl2303 芯片反初始化函数
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_pl2303_deinit(struct usbh_serial *p_userial);

#endif
