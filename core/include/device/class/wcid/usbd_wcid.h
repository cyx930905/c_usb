#ifndef __USBD_WICD_H__
#define __USBD_WICD_H__

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "common/err/usb_err.h"
#include "core/include/specs/usb_specs.h"
#include "core/include/device/core/usbd.h"

/* \brief 接口号定义*/
#define USBD_WCID_INTF_NUM           0   /* USB Windows 兼容 ID 设备接口编号*/
#define USBD_WCID_INTF_ALT_NUM       0   /* USB Windows 兼容 ID 设备接口备用设置编号*/

struct usbd_wcid {
    uint8_t            cfg_num;          /* 配置编号*/
    uint8_t            data_in_ep_addr;  /* 数据输入端点地址*/
    uint8_t            data_out_ep_addr; /* 数据输出端点地址*/
    struct usbd_dev   *p_dc_dev;         /* USB 从机设备结构体*/
    struct usbd_pipe  *p_data_in;        /* 数据输入管道*/
    struct usbd_pipe  *p_data_out;       /* 数据输出管道*/
    usb_bool_t         is_setup;         /* 是否设置*/
};

/**
 * \brief 创建一个 USB Windows 兼容 ID 设备
 *
 * \param[in]  p_dc       所属的 USB 从机控制器
 * \param[in]  p_name     设备名字
 * \param[in]  dev_desc   设备描述符
 * \param[out] p_wcid_ret 返回创建成功的 USB Windows 兼容 ID 设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbd_wcid_create(struct usb_dc        *p_dc,
                     char                 *p_name,
                     uint8_t               vendor_code,
                     struct usbd_dev_desc  dev_desc,
                     struct usbd_wcid    **p_wcid_ret);
/**
 * \brief 销毁 USB Windows 兼容 ID 设备
 *
 * \param[in] p_wcid USB Windows 兼容 ID 设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_wcid_destroy(struct usbd_wcid *p_wcid);
/**
 * \brief 检查 USB Windows 兼容 ID 设备是否设置
 *
 * \param[in] p_wcid USB Windows 兼容 ID 设备
 *
 * \retval 设置返回 USB_TRUE，否则返回 USB_FALSE
 */
usb_bool_t usbd_wcid_is_setup(struct usbd_wcid *p_wcid);
/**
 * \brief USB Windows 兼容 ID 设备写数据
 *
 * \param[in]  p_wcid    USB Windows 兼容 ID 设备
 * \param[in]  p_buf     写缓存
 * \param[in]  len       要写的长度
 * \param[in]  timeout   写超时时间
 * \param[out] p_act_len 返回实际写的数据
 *
 * \retval 成功返回 USB_OK
 */
int usbd_wcid_write(struct usbd_wcid *p_wcid,
                    uint8_t          *p_buf,
                    uint32_t          len,
                    int               timeout,
                    uint32_t         *p_act_len);
/**
 * \brief USB Windows 兼容 ID 设备同步读数据
 *
 * \param[in]  p_wcid    USB Windows 兼容 ID 设备
 * \param[in]  p_buf     读缓存
 * \param[in]  len       要读的长度
 * \param[in]  timeout   读超时时间
 * \param[out] p_act_len 返回实际读的数据
 *
 * \retval 成功返回 USB_OK
 */
int usbd_wcid_read_sync(struct usbd_wcid *p_wcid,
                        uint8_t          *p_buf,
                        uint32_t          len,
                        int               timeout,
                        uint32_t         *p_act_len);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif

