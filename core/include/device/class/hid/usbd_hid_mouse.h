#ifndef __USBD_HID_MOUSE_H__
#define __USBD_HID_MOUSE_H__

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "core/include/device/class/hid/usbd_hid.h"

/* \brief USB HID 鼠标输入结构体*/
struct usbd_mouse_report {
    uint8_t     left     :1;    /* 左键，bits[0]*/
    uint8_t     right    :1;    /* 右键，bits[1]*/
    uint8_t     center   :1;    /* 中间键， bits[2]*/
    uint8_t     padding  :5;    /* bits[7:3]*/
    int8_t      x;              /* 相对坐标x*/
    int8_t      y;              /* 相对坐标y*/
    int8_t      wheel;          /* 滚轮*/
};

/**
 * \brief 创建一个 USB 从机鼠标设备
 *
 * \param[in]  p_dc      所属的 USB 从机控制器
 * \param[in]  p_name    设备名字
 * \param[in]  dev_desc  设备描述符
 * \param[out] p_hid_ret 返回创建成功的 USB 从机人体接口设
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_mouse_create(struct usb_dc        *p_dc,
                          char                 *p_name,
                          struct usbd_dev_desc  dev_desc,
                          struct usbd_hid     **p_hid_ret);
/**
 * \brief 销毁一个 USB 从机鼠标设备
 *
 * \param[in] p_hid 要销毁的  USB 从机人体接口设
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_mouse_destroy(struct usbd_hid *p_hid);
/**
 * \brief USB 从机鼠标设备发送数据
 *
 * \param[in]  p_hid      USB 从机鼠标设备
 * \param[in]  p_report   鼠标报告数据
 * \param[in]  timeout    发送超时时间
 * \param[out] p_act_len  返回实际发送的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_mouse_write(struct usbd_hid          *p_hid,
                         struct usbd_mouse_report *p_report,
                         int                       timeout,
                         uint32_t                 *p_act_len);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif



