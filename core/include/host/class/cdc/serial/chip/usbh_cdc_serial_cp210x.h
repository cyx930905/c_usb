#ifndef __USBH_CDC_SERIAL_CP210X_H
#define __USBH_CDC_SERIAL_CP210X_H

#include "core/include/host/class/cdc/serial/usbh_cdc_serial_drv.h"

#define CP210X_N_TRP                8

#define CP210X_SET_LINE_CTL         0x03     /* 设置配置*/
#define CP210X_GET_LINE_CTL         0x04     /* 获取配置*/
#define CP210X_SET_FLOW             0x13     /* 设置控制流*/
#define CP210X_GET_FLOW             0x14     /* 获取控制流*/
#define CP210X_SET_BAUDRATE         0x1E     /* 设置波特率*/

/* \brief 配置请求类型*/
#define REQTYPE_HOST_TO_INTERFACE   0x41
#define REQTYPE_INTERFACE_TO_HOST   0xc1

/* \brief CP210X_(设置|获取)_配置 */
/* \brief 数据位*/
#define BITS_DATA_MASK              0X0f00
#define BITS_DATA_5                 0X0500
#define BITS_DATA_6                 0X0600
#define BITS_DATA_7                 0X0700
#define BITS_DATA_8                 0X0800
#define BITS_DATA_9                 0X0900

/* \brief 数据校验*/
#define BITS_PARITY_MASK            0x00f0
#define BITS_PARITY_NONE            0x0000
#define BITS_PARITY_ODD             0x0010
#define BITS_PARITY_EVEN            0x0020
#define BITS_PARITY_MARK            0x0030
#define BITS_PARITY_SPACE           0x0040

/* \brief 停止位*/
#define BITS_STOP_MASK              0x000f
#define BITS_STOP_1                 0x0000
#define BITS_STOP_1_5               0x0001
#define BITS_STOP_2                 0x0002

#define PL2303_VENDOR_WRITE_REQUEST_TYPE (0x40)
#define PL2303_VENDOR_WRITE_REQUEST      (0x01)
#define PL2303_VENDOR_READ_REQUEST_TYPE  (0xc0)
#define PL2303_VENDOR_READ_REQUEST       (0x01)

/* \brief USB 转串口 cp210x 串口结构体*/
struct usbh_serial_cp210x {
    struct usbh_serial   *p_userial;
    struct usbh_endpoint *p_data_ep_rd;
    struct usbh_endpoint *p_data_ep_wr;
    uint32_t              rd_buf_size;
    uint8_t              *p_rd_buf[CP210X_N_TRP];
    struct usbh_trp       trp_rd[CP210X_N_TRP];
};

/**
 * \brief USB 转串口 cp210x 芯片初始化
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_cp210x_init(struct usbh_serial *p_userial);
/**
 * \brief USB 转串口 cp210x 芯片反初始化函数
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_cp210x_deinit(struct usbh_serial *p_userial);

#endif
