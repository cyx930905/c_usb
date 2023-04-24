#ifndef __USBD_HID_H__
#define __USBD_HID_H__

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "common/err/usb_err.h"
#include "core/include/specs/usb_specs.h"
#include "core/include/device/core/usbd.h"
#include "core/include/specs/usb_hid_specs.h"

/* \brief 接口号定义*/
#define USBD_HID_INTF_NUM           0   /* USB 人体接口设备接口编号*/
#define USBD_HID_INTF_ALT_NUM       0   /* USB 人体接口设备接口备用设置编号*/

/* \brief USB 从机人体接口设备结构体 */
struct usbd_hid {
    uint8_t            cfg_num;           /* 配置编号*/
    uint8_t            data_in_ep_addr;   /* 数据输入端点地址*/
    uint8_t            data_out_ep_addr;  /* 数据输出端点地址*/
    struct usbd_dev   *p_dc_dev;          /* USB 从机设备结构体*/
    struct usbd_pipe  *p_data_in;         /* 数据输入管道*/
    struct usbd_pipe  *p_data_out;        /* 数据输出管道*/
    char              *p_report_data;     /* 报告数据集合*/
    uint16_t           report_data_size;  /* 报告数据大小*/
    usb_bool_t         is_setup;          /* 是否设置*/
};

/**
 * \brief 创建一个 USB 从机人体接口设备
 *
 * \param[in]  p_dc             所属的 USB 从机控制器
 * \param[in]  p_name           设备名字
 * \param[in]  dev_desc         设备描述符
 * \param[in]  p_report_data    报告数据集合
 * \param[in]  report_data_size 报告数据大小
 * \param[out] p_hid_ret        返回创建成功的 USB 从机人体接口设
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_create(struct usb_dc        *p_dc,
                    char                 *p_name,
                    struct usbd_dev_desc  dev_desc,
                    char                 *p_report_data,
                    uint16_t              report_data_size,
                    struct usbd_hid     **p_hid_ret);
/**
 * \brief 销毁一个 USB 从机人体接口设备
 *
 * \param[in] p_hid 要销毁的 USB 从机人体接口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_destroy(struct usbd_hid *p_hid);
/**
 * \brief 检查 USB 从机人体接口设备是否设置
 *
 * \param[in] p_hid USB 人体接口设备
 *
 * \retval 设置返回 USB_TRUE，否则返回 USB_FALSE
 */
usb_bool_t usbd_hid_is_setup(struct usbd_hid *p_hid);
/**
 * \brief USB 从机人体接口设备写数据
 *
 * \param[in]  p_hid     USB 人体接口设备
 * \param[in]  p_buf     要发送的数据缓存
 * \param[in]  len       数据长度
 * \param[in]  timeout   超时时间
 * \param[out] p_act_len 返回实际发送的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_write(struct usbd_hid *p_hid,
                   uint8_t         *p_buf,
                   uint32_t         len,
                   int              timeout,
                   uint32_t        *p_act_len);
/**
 * \brief USB 从机人体接口设备同步读数据
 *
 * \param[in]  p_hid     USB 人体接口设备
 * \param[in]  p_buf     要读取的数据缓存
 * \param[in]  len       数据长度
 * \param[in]  timeout   超时时间
 * \param[out] p_act_len 返回实际读到的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_read_sync(struct usbd_hid *p_hid,
                       uint8_t         *p_buf,
                       uint32_t         len,
                       int              timeout,
                       uint32_t        *p_act_len);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif

