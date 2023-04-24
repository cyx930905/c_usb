/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/cdc/serial/usbh_cdc_serial_drv.h"

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief USB 转串口初始化兼容
 */
int usbh_serial_quirk_init(struct usbh_function *p_usb_fun){
    uint16_t vid, pid;

    vid = USBH_DEV_VID_GET(p_usb_fun);
    pid = USBH_DEV_PID_GET(p_usb_fun);

    if((vid == 0x1782) && (pid == 0x4D10)){
        usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0, 0x21, 0x22, 0, p_usb_fun->first_intf_num, 0, NULL, 5000, 0);
        usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0, 0x21, 0x22, 1, p_usb_fun->first_intf_num, 0, NULL, 5000, 0);
    }

    return USB_OK;
}
