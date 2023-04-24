/*******************************************************************************
 *                                    ZLG
 *                         ----------------------------
 *                         innovating embedded platform
 *
 * Copyright (c) 2001-present Guangzhou ZHIYUAN Electronics Co., Ltd.
 * All rights reserved.
 *
 * Contact information:
 * web site:    https://www.zlg.cn
 *******************************************************************************/
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/device/class/hid/usbd_hid_mouse.h"

/*******************************************************************************
 * Static
 ******************************************************************************/
/* \brief 鼠标报告设备 */
static const char __g_mouse_report[52] = {
    //通用桌面设备
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    //鼠标
    0x09, 0x02,                    // USAGE (Mouse)
    //集合
    0xa1, 0x01,                    // COLLECTION (Application)
    //指针设备
    0x09, 0x01,                    //   USAGE (Pointer)
    //集合
    0xa1, 0x00,                    //   COLLECTION (Physical)
    //按键
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    //使用最小值1
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    //使用最大值3。1表示左键，2表示右键，3表示中键
    0x29, 0x03,                    //     USAGE_MAXIMUM (Button 3)
    //逻辑最小值0
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    //逻辑最大值1
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    //数量为3
    0x95, 0x03,                    //     REPORT_COUNT (3)
    //大小为1bit
    0x75, 0x01,                    //     REPORT_SIZE (1)
    //输入，变量，数值，绝对值
    //以上3个bit分别表示鼠标的三个按键情况，最低位（bit-0）为左键
    //bit-1为右键，bit-2为中键，按下时对应的位值为1，释放时对应的值为0
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)

    //填充5个bit，补足一个字节
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x05,                    //     REPORT_SIZE (5)
    0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)

    //用途页为通用桌面
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    //用途为X
    0x09, 0x30,                    //     USAGE (X)
    //用途为Y
    0x09, 0x31,                    //     USAGE (Y)
    //用途为滚轮
    0x09, 0x38,                    //     USAGE (Wheel)
    //逻辑最小值为-127
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    //逻辑最大值为+127
    0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
    //大小为8个bits
    0x75, 0x08,                    //     REPORT_SIZE (8)
    //数量为3个，即分别代表x,y,滚轮
    0x95, 0x03,                    //     REPORT_COUNT (3)
    //输入，变量，值，相对值
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)

    //关集合
    0xc0,                          //   END_COLLECTION
    0xc0                           // END_COLLECTION
};

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 创建一个 USB 从机鼠标设备
 *
 * \param[in]  p_dc      所属的 USB 从机控制器
 * \param[in]  p_name    设备名字
 * \param[in]  dev_desc  设备描述符
 * \param[out] p_hid_ret 返回创建成功的 USB 从机人体接口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_mouse_create(struct usb_dc        *p_dc,
                          char                 *p_name,
                          struct usbd_dev_desc  dev_desc,
                          struct usbd_hid     **p_hid_ret){
    return usbd_hid_create(p_dc, p_name, dev_desc, (char *)__g_mouse_report, sizeof(__g_mouse_report), p_hid_ret);
}

/**
 * \brief 销毁一个 USB 从机鼠标设备
 *
 * \param[in] p_hid 要销毁的  USB 从机人体接口设
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_mouse_destroy(struct usbd_hid *p_hid){
    return usbd_hid_destroy(p_hid);
}

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
                         uint32_t                 *p_act_len){
    return usbd_hid_write(p_hid,
                         (uint8_t *)p_report,
                          sizeof(struct usbd_mouse_report),
                          timeout,
                          p_act_len);
}

