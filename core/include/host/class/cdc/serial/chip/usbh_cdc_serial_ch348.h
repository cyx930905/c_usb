#ifndef __USBH_CDC_SERIAL_CHX_H
#define __USBH_CDC_SERIAL_CHX_H

#include "common/err/usb_err.h"
#include "core/include/host/class/cdc/serial/usbh_cdc_serial_drv.h"

#define CH348_NR       8

#define CH348_R_C1     0x01
#define CH348_R_C2     0x02
#define CH348_R_C3     0x03
#define CH348_R_C4     0x04
#define CH348_R_C5     0x06

#define CH348_M_NOR    0x00
#define CH348_M_RS     0x01
#define CH348_M_IO     0x02
#define CH348_M_HF     0x03

#define CH348_CTO_D    0x01

#define CH348_CMD_WB_E 0x90
#define CH348_CMD_VER  0x96

#define CH348_R_INIT   0xA1

#define CH348_CMD_W_BR 0x80
#define CH348_CMD_W_R  0xC0

typedef enum {
    CHIP_CH9344 = 1,
    CHIP_CH348L = 2,
    CHIP_CH348Q = 3,
} chip_type_t;

/* \brief USB 转串口 ch348 串口结构体*/
struct usbh_serial_ch348 {
    struct usbh_serial   *p_userial;
    chip_type_t           chip_type;
    uint8_t               n_ports;
    uint8_t               port_offset;
    struct usbh_trp       trp_cmd_rd;
    struct usbh_trp       trp_data_rd[CH348_NR];
    uint8_t              *p_rd_buf[CH348_NR];
    uint32_t              rd_buf_size;
    uint32_t              wr_size_max;
    usb_bool_t            is_modeline9;
    uint32_t              buf[512];
    struct usbh_endpoint *p_cmd_ep_rd;
    struct usbh_endpoint *p_cmd_ep_wr;
    struct usbh_endpoint *p_data_ep_rd;
    struct usbh_endpoint *p_data_ep_wr;
};

/**
 * \brief USB 转串口 ch348 初始化
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_ch348_init(struct usbh_serial *p_userial);
/**
 * \brief USB 转串口 ch348 芯片反初始化
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_ch348_deinit(struct usbh_serial *p_userial);


#endif

