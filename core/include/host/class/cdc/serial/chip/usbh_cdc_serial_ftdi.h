#ifndef __USBH_CDC_SERIAL_FTDI_H
#define __USBH_CDC_SERIAL_FTDI_H

#include "core/include/host/class/cdc/serial/usbh_cdc_serial_drv.h"

#define FTDI_N_TRP                           8

#define INTERFACE_A                          1
#define INTERFACE_B                          2
#define INTERFACE_C                          3
#define INTERFACE_D                          4

#define FTDI_SIO_RESET                       0     /* 复位端口*/
#define FTDI_SIO_MODEM_CTRL                  1     /* Set the modem control register */
#define FTDI_SIO_SET_FLOW_CTRL               2     /* 设置流控制寄存器*/
#define FTDI_SIO_SET_BAUDRATE_REQUEST        3     /* 设置波特率*/
#define FTDI_SIO_SET_DATA                    4     /* 设置端口数据特性*/

#define FTDI_SIO_RESET_REQUEST_TYPE          0x40  /* 复位USB请求类型*/
#define FTDI_SIO_SET_DATA_REQUEST_TYPE       0x40  /* 设置数据USB请求类型*/
#define FTDI_SIO_SET_FLOW_CTRL_REQUEST_TYPE  0x40  /* 设置数据流USB请求类型*/
#define FTDI_SIO_SET_BAUDRATE_REQUEST_TYPE   0x40  /* 设置波特率USB请求类型*/
#define FTDI_SIO_SET_MODEM_CTRL_REQUEST_TYPE 0x40  /* */

/* \brief 校验设置*/
#define FTDI_SIO_SET_DATA_PARITY_NONE       (0x0 << 8)
#define FTDI_SIO_SET_DATA_PARITY_ODD        (0x1 << 8)
#define FTDI_SIO_SET_DATA_PARITY_EVEN       (0x2 << 8)
#define FTDI_SIO_SET_DATA_PARITY_MARK       (0x3 << 8)
#define FTDI_SIO_SET_DATA_PARITY_SPACE      (0x4 << 8)

/* \brief 停止位设置*/
#define FTDI_SIO_SET_DATA_STOP_BITS_1       (0x0 << 11)
#define FTDI_SIO_SET_DATA_STOP_BITS_15      (0x1 << 11)
#define FTDI_SIO_SET_DATA_STOP_BITS_2       (0x2 << 11)

/* 控制流*/
#define FTDI_SIO_RTS_CTS_HS                 (0x1 << 8)
#define FTDI_SIO_DTR_DSR_HS                 (0x2 << 8)
#define FTDI_SIO_XON_XOFF_HS                (0x4 << 8)

#define FTDI_SIO_SET_DTR_MASK                0x1
#define FTDI_SIO_SET_DTR_HIGH               (1 | (FTDI_SIO_SET_DTR_MASK  << 8))
#define FTDI_SIO_SET_DTR_LOW                (0 | (FTDI_SIO_SET_DTR_MASK  << 8))
#define FTDI_SIO_SET_RTS_MASK                0x2
#define FTDI_SIO_SET_RTS_HIGH               (2 | (FTDI_SIO_SET_RTS_MASK << 8))
#define FTDI_SIO_SET_RTS_LOW                (0 | (FTDI_SIO_SET_RTS_MASK << 8))

#define FTDI_NDI_HUC_PID                     0xDA70  /* NDI Host USB Converter */
#define FTDI_NDI_SPECTRA_SCU_PID             0xDA71  /* NDI Spectra SCU */
#define FTDI_NDI_FUTURE_2_PID                0xDA72  /* NDI future device #2 */
#define FTDI_NDI_FUTURE_3_PID                0xDA73  /* NDI future device #3 */
#define FTDI_NDI_AURORA_SCU_PID              0xDA74  /* NDI Aurora SCU */

/* \brief FTDI 芯片类型 */
enum ftdi_chip_type {
    SIO       = 1,
    FT8U232AM = 2,
    FT232BM   = 3,
    FT2232C   = 4,
    FT232RL   = 5,
    FT2232H   = 6,
    FT4232H   = 7,
    FT232H    = 8,
    FTX       = 9,
};

/* \brief 波特率*/
enum ftdi_sio_baudrate {
    ftdi_sio_b300 =    0,
    ftdi_sio_b600 =    1,
    ftdi_sio_b1200 =   2,
    ftdi_sio_b2400 =   3,
    ftdi_sio_b4800 =   4,
    ftdi_sio_b9600 =   5,
    ftdi_sio_b19200 =  6,
    ftdi_sio_b38400 =  7,
    ftdi_sio_b57600 =  8,
    ftdi_sio_b115200 = 9
};

/* \brief USB 转串口 FTDI 串口结构体*/
struct usbh_serial_ftdi {
    struct usbh_serial   *p_userial;
    struct usbh_endpoint *p_data_ep_rd;
    struct usbh_endpoint *p_data_ep_wr;
    uint32_t              rd_buf_size;
    uint8_t              *p_rd_buf[FTDI_N_TRP];
    struct usbh_trp       trp_rd[FTDI_N_TRP];
    enum ftdi_chip_type   chip_type;            /* 芯片型号*/
    int                   baud_base;            /* 波特率基数*/
    uint16_t              intf_num;             /* 接口编号*/
};

/**
 * \brief USB 转串口 FTDI 芯片初始化
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_ftdi_init(struct usbh_serial *p_userial);
/**
 * \brief USB 转串口 FTDI 芯片反初始化函数
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_ftdi_deinit(struct usbh_serial *p_userial);

#endif
