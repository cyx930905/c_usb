#ifndef __USBD_VIRTUAL_PRINTER_H__
#define __USBD_VIRTUAL_PRINTER_H__

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "common/err/usb_err.h"
#include "core/include/specs/usb_specs.h"
#include "core/include/device/core/usbd.h"

/* \brief 接口号定义*/
#define USBD_VP_INTF_NUM           0   /* USB 虚拟打印机设备接口编号*/
#define USBD_VP_INTF_ALT_NUM       0   /* USB 虚拟打印机设备接口备用设置编号*/

/* \brief USB 虚拟打印设备 ID 获取 */
#define USB_VP_GET_DEVICE_ID       0
/* \brief USB 虚拟打印设备端口状态获取 */
#define USB_VP_GET_PORT_STATUS     1
/* \brief USB 虚拟打印设备软复位 */
#define USB_VP_SOFT_RESET          2

#define USB_VP_NOT_ERROR           0x08
#define USB_VP_SELECTED            0x10
#define USB_VP_PAPER_EMPTY         0x20

#define USB_VP_PNP_STRING_LEN      1024

/* \brief USB 从机虚拟打印机结构体 */
struct usbd_vp {
    uint8_t            cfg_num;          /* 配置编号*/
    uint8_t            printer_sta;      /* 打印机状态*/
    uint8_t            data_in_ep_addr;  /* 数据输入端点地址*/
    uint8_t            data_out_ep_addr; /* 数据输出端点地址*/
    struct usbd_dev   *p_dc_dev;         /* USB 从机设备结构体*/
    struct usbd_pipe  *p_data_in;        /* 数据输入管道*/
    struct usbd_pipe  *p_data_out;       /* 数据输出管道*/
    uint8_t           *p_buf;
    char              *p_printer_id;     /* 打印机 ID 字符串*/
#if USB_OS_EN
    usb_mutex_handle_t p_lock;
#endif
    usb_bool_t         is_setup;         /* 是否设置*/
};

/**
 * \brief 创建一个 USB 虚拟打印机设备
 *
 * \param[in] p_dc     所属的 USB 从机控制器
 * \param[in] p_name   设备名字
 * \param[in] dev_desc 设备描述符
 * \param[in] p_vp_ret 返回创建成功的 USB 虚拟打印机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbd_virtual_printer_create(struct usb_dc        *p_dc,
                                char                 *p_name,
                                struct usbd_dev_desc  dev_desc,
                                struct usbd_vp      **p_vp_ret);
/**
 * \brief 销毁 USB 虚拟打印机
 *
 * \param[in] p_vp USB 虚拟打印机设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_virtual_printer_destroy(struct usbd_vp *p_vp);
/**
 * \brief 检查 USB 虚拟打印机设备是否设置
 *
 * \param[in] p_vp USB 虚拟打印机设备
 *
 * \retval 设置返回 USB_TRUE，否则返回 USB_FALSE
 */
usb_bool_t usbd_virtual_printer_is_setup(struct usbd_vp *p_vp);
/**
 * \brief 设置 USB 虚拟打印机字符串 ID
 *
 * \param[in] p_vp USB 虚拟打印机设备
 * \param[in] p_id 要设置的字符串 ID
 *
 * \retval 成功返回 USB_OK
 */
int usbd_virtual_printer_id_set(struct usbd_vp *p_vp, char *p_id);
/**
 * \brief USB 虚拟打印机设备写数据
 *
 * \param[in]  p_vp      USB 虚拟打印机设备
 * \param[in]  p_buf     要发送的数据缓存
 * \param[in]  len       数据长度
 * \param[in]  timeout   超时时间
 * \param[out] p_act_len 返回实际发送的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbd_virtual_printer_write(struct usbd_vp  *p_vp,
                               uint8_t         *p_buf,
                               uint32_t         len,
                               uint32_t         timeout,
                               uint32_t        *p_act_len);
/**
 * \brief USB 虚拟打印机设备同步读数据
 *
 * \param[in]  p_vp      USB 虚拟打印机设备
 * \param[in]  p_buf     要读取的数据缓存
 * \param[in]  len       数据长度
 * \param[in]  timeout   超时时间
 * \param[out] p_act_len 返回读到的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbd_virtual_printer_read_sync(struct usbd_vp  *p_vp,
                                   uint8_t         *p_buf,
                                   uint32_t         len,
                                   int              timeout,
                                   uint32_t        *p_act_len);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif

