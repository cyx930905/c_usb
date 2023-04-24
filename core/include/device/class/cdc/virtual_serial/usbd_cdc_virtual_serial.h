#ifndef __USBD_CDC_VIRTUAL_SERIAL_H__
#define __USBD_CDC_VIRTUAL_SERIAL_H__

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "common/err/usb_err.h"
#include "core/include/specs/usb_cdc_specs.h"
#include "core/include/device/core/usbd.h"

#define USBD_VS_RB_SIZE 2048

/* \brief USB 从机虚拟串口端口结构体*/
struct usbd_cdc_vs_port {
    struct usbd_cdc_vs         *p_vs;
    uint8_t                     port_num;
    uint8_t                     cmd_intf_num;
    uint8_t                     data_intf_num;
    struct usbd_pipe           *p_data_in;        /* 数据输入管道*/
    struct usbd_pipe           *p_data_out;       /* 数据输出管道*/
    uint8_t                     data_in_ep_addr;  /* 数据输入端点地址*/
    uint8_t                     data_out_ep_addr; /* 数据输出端点地址*/
    struct usb_cdc_line_coding  line_control;     /* 串口配置*/
    usb_bool_t                  is_setup;         /* 是否设置*/
    usb_bool_t                  is_start;         /* 是否启动*/
    struct usb_ringbuf         *p_rb;             /* 环形缓冲区*/
    struct usbd_trans           trans;            /* USB 传输事务*/
};

/* \brief USB 从机虚拟串口结构体 */
struct usbd_cdc_vs {
    uint8_t                  cfg_num;          /* 配置编号*/
    uint8_t                  n_ports;          /* 虚拟串口的数量*/
    struct usbd_dev         *p_dc_dev;         /* USB 从机设备结构体*/
    struct usbd_cdc_vs_port *p_port;
};

/**
 * \brief USB 从机虚拟串口设备创建
 *
 * \param[in]  p_dc     所属的 USB 从机控制器
 * \param[in]  p_name   设备名字
 * \param[in]  dev_desc 设备描述符
 * \param[out] p_vs_ret 返回创建成功的 USB 虚拟串口结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbd_cdc_vs_create(struct usb_dc        *p_dc,
                       char                 *p_name,
                       struct usbd_dev_desc  dev_desc,
                       uint8_t               n_ports,
                       struct usbd_cdc_vs  **p_vs_ret);
/**
 * \brief 销毁 USB 虚拟串口
 *
 * \param[in] p_vs USB 虚拟串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_cdc_vs_destroy(struct usbd_cdc_vs *p_vs);
/**
 * \brief 检查 USB 从机虚拟串口端口是否设置
 *
 * \param[in] p_vs     USB 从机虚拟串口设备
 * \param[in] port_num 端口号
 *
 * \retval 设置返回 USB_TRUE，否则返回 USB_FALSE
 */
usb_bool_t usbd_cdc_vs_port_is_setup(struct usbd_cdc_vs *p_vs, uint8_t port_num);
/**
 * \brief USB 从机虚拟串口设备端口启动
 *
 * \param[in]  p_vs      USB 从机虚拟串口设备
 * \param[in]  port_num  端口号
 *
 * \retval 成功返回 USB_OK
 */
int usbd_cdc_vs_port_start(struct usbd_cdc_vs *p_vs, uint8_t port_num);
/**
 * \brief USB 从机虚拟串口设备端口写数据
 *
 * \param[in]  p_vs      USB 从机虚拟串口设备
 * \param[in]  port_num  端口号
 * \param[in]  p_buf     要发送的数据缓存
 * \param[in]  len       数据长度
 * \param[in]  timeout   超时时间
 * \param[out] p_act_len 返回实际发送的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbd_cdc_vs_port_write(struct usbd_cdc_vs  *p_vs,
                           uint8_t              port_num,
                           uint8_t             *p_buf,
                           uint32_t             len,
                           int                  timeout,
                           uint32_t            *p_act_len);
/**
 * \brief USB 从机虚拟串口设备端口同步读数据
 *
 * \param[in]  p_vs      USB 从机虚拟串口设备
 * \param[in]  port_num  端口号
 * \param[in]  p_buf     要读取的数据缓存
 * \param[in]  len       数据长度
 * \param[out] p_act_len 返回读到的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbd_cdc_vs_port_read(struct usbd_cdc_vs  *p_vs,
                          uint8_t              port_num,
                          uint8_t             *p_buf,
                          uint32_t             len,
                          uint32_t            *p_act_len);
///**
// * \brief USB 从机虚拟串口设备端口同步读数据
// *
// * \param[in]  p_vs      USB 从机虚拟串口设备
// * \param[in]  port_num  端口号
// * \param[in]  p_buf     要读取的数据缓存
// * \param[in]  len       数据长度
// * \param[in]  timeout   超时时间
// * \param[out] p_act_len 返回读到的数据长度
// *
// * \retval 成功返回 USB_OK
// */
//int usbd_cdc_vs_port_read_sync(struct usbd_cdc_vs  *p_vs,
//                               uint8_t              port_num,
//                               uint8_t             *p_buf,
//                               uint32_t             len,
//                               int                  timeout,
//                               uint32_t            *p_act_len);
///**
// * \brief USB 虚拟串口设备异步读数据
// *
// * \param[in]  p_vs      USB 虚拟串口设备
// * \param[in]  p_buf     要读取的数据缓存
// * \param[in]  len       数据长度
// * \param[out] p_act_len 返回读到的数据长度
// *
// * \retval 成功返回 USB_OK
// */
//int usbd_cdc_vs_read_async(struct usbd_cdc_vs  *p_vs,
//                           uint8_t             *p_buf,
//                           uint32_t             len,
//                           uint32_t            *p_act_len);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif

