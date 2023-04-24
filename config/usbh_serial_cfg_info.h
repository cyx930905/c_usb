#ifndef __USBH_SERIAL_CFG_INFO_H
#define __USBH_SERIAL_CFG_INFO_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "core/include/host/core/usbh_dev.h"
#include "core/include/host/class/cdc/serial/usbh_cdc_serial_drv.h"
#include "core/include/host/class/cdc/serial/usbh_cdc_serial_chip.h"
#include <string.h>
#include <stdio.h>

///* \brief USB 转串口设备信息*/
//const struct usbh_serial_dev_info __g_userial_dev_tab[] = {
////    /* FTDI ����ģ��*/
////    {
////            .vid    = 0x0403,
////            .pid    = 0x6001,
////            .type   = "FTDI",
////            .init   = usbh_serial_ftdi_init,
////            .uninit = usbh_serial_ftdi_uninit,
////    },
//    /* CH341 串口模块*/
//    {
//            .vid         = 0x4348,
//            .pid         = 0x5523,
//            .p_drv_name  = "CH34x",
//            .p_fn_init   = usbh_serial_ch341_init,
//            .p_fn_deinit = usbh_serial_ch341_deinit,
//    },
//    /* CH341 串口模块*/
//    {
//            .vid         = 0x1A86,
//            .pid         = 0x7523,
//            .p_drv_name  = "CH34x",
//            .p_fn_init   = usbh_serial_ch341_init,
//            .p_fn_deinit = usbh_serial_ch341_deinit,
//    },
//    /* CH341 串口模块*/
//    {
//            .vid         = 0x1A86,
//            .pid         = 0x5523,
//            .p_drv_name  = "CH34x",
//            .p_fn_init   = usbh_serial_ch341_init,
//            .p_fn_deinit = usbh_serial_ch341_deinit,
//    },
//    {
//            .vid         = 0x1A86,
//            .pid         = 0x55d9,
//            .p_drv_name  = "CH348",
//            .p_fn_init   = usbh_serial_ch348_init,
//            .p_fn_deinit = usbh_serial_ch348_deinit,
//    },
//    /* PL2303 串口模块*/
//    {
//            .vid         = 0x067B,
//            .pid         = 0x2303,
//            .p_drv_name  = "PL2303",
//            .p_fn_init   = usbh_serial_pl2303_init,
//            .p_fn_deinit = usbh_serial_pl2303_deinit,
//    },
//    /* 移远 EC20 4G 模块*/
//    {
//            .vid         = 0x2C7C,
//            .pid         = 0x0125,
//            .p_drv_name  = "EC20",
//            .p_fn_init   = usbh_serial_default_init,
//            .p_fn_deinit = usbh_serial_default_deinit,
//    },
//    /* 移远 EC200U 4G 模块*/
//    {
//            .vid         = 0x2C7C,
//            .pid         = 0x0901,
//            .p_drv_name  = "EC200U",
//            .p_fn_init   = usbh_serial_default_init,
//            .p_fn_deinit = usbh_serial_default_deinit,
//    },
//    /* 移远 RM500U 5G 模块*/
//    {
//            .vid         = 0x2c7c,
//            .pid         = 0x0900,
//            .p_drv_name  = "RM500U",
//            .p_fn_init   = usbh_serial_default_init,
//            .p_fn_deinit = usbh_serial_default_deinit,
//    },
//    /* FIBOCOM L610 4G 模块*/
//    {
//            .vid         = 0x1782,
//            .pid         = 0x4D10,
//            .p_drv_name  = "L610",
//            .p_fn_init   = usbh_serial_default_init,
//            .p_fn_deinit = usbh_serial_default_deinit,
//    },
//    USBH_SERIAL_INFO_END
//};

/* \brief USB 主机转串口设备匹配信息*/
const struct usbh_serial_dev_match_info __g_userial_dev_tab[] = {
        {
                {
                        .flags      = USB_DEV_PID_MATCH | USB_DEV_VID_MATCH,
                        .pid        = 0x6001,
                        .vid        = 0x0403,
                },
                "FTDI",
        }, /* USB 转串口 FTDI */
        {
                {
                        .flags      = USB_DEV_PID_MATCH | USB_DEV_VID_MATCH,
                        .pid        = 0x5523,
                        .vid        = 0x4348,
                },
                "CH341",
        }, /* USB 转串口 CH341 */
        {
                {
                        .flags      = USB_DEV_PID_MATCH | USB_DEV_VID_MATCH,
                        .pid        = 0x7523,
                        .vid        = 0x1A86,
                },
                "CH341",
        }, /* USB 转串口 CH341 */
        {
                {
                        .flags      = USB_DEV_PID_MATCH | USB_DEV_VID_MATCH,
                        .pid        = 0x5523,
                        .vid        = 0x1A86,
                },
                "CH341",
        }, /* USB 转串口 CH341 */
        {
                {
                        .flags      = USB_DEV_PID_MATCH | USB_DEV_VID_MATCH,
                        .vid        = 0x1A86,
                        .pid        = 0x55d9,
                },
                "CH348",
        }, /* USB 转串口 CH348 */
        {
                {
                        .flags      = USB_DEV_PID_MATCH | USB_DEV_VID_MATCH,
                        .pid        = 0x2303,
                        .vid        = 0x067B,
                },
                "PL2303",
        }, /* USB 转串口 PL2303 */
        {
                {
                        .flags      = USB_DEV_PID_MATCH | USB_DEV_VID_MATCH,
                        .pid        = 0xEA60,
                        .vid        = 0x10C4,
                },
                "CP210X",
        }, /* USB 转串口 CP210X */
        {
                {
                        .flags      = USB_DEV_PID_MATCH | USB_DEV_VID_MATCH | USB_INTF_CLASS_MATCH,
                        .pid        = 0x0900,
                        .vid        = 0x2C7C,
                        .intf_class = 0xFF,
                },
                "Common",
        },
        {
                {
                        .flags      = USB_DEV_PID_MATCH | USB_DEV_VID_MATCH,
                        .pid        = 0x7209,
                        .vid        = 0x2F81,
                        .intf_class = 0xFF,
                },
                "Common",
        },
        {
                {
                        .flags          = USB_DEV_PID_MATCH | USB_DEV_VID_MATCH | USB_INTF_CLASS_MATCH | USB_INTF_SUB_CLASS_MATCH,
                        .pid            = 0x0001,
                        .vid            = 0x19D1,
                        .intf_class     = USB_CLASS_COMM,
                        .intf_sub_class = USB_CDC_SUBCLASS_ACM,
                },
                "Common",
        },  /* 合宙 Air780EG */
        USB_SERIAL_DEV_MATCH_END
};

#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif /* __USBH_SERIAL_CFG_INFO_H */


