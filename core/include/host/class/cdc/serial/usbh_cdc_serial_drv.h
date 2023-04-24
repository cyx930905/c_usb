#ifndef __USBH_CDC_SERIAL_DRV_H
#define __USBH_CDC_SERIAL_DRV_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "core/include/host/core/usbh_dev.h"
#include "core/include/host/core/usbh.h"
#include "core/include/specs/usb_cdc_specs.h"
#include "common/list/usb_list.h"
#include "common/refcnt/usb_refcnt.h"
#include "common/err/usb_err.h"
#include <stdio.h>
#include <string.h>

#if USB_OS_EN
#define USERIAL_LOCK_TIMEOUT         5000
#endif

/* \brief USB 转串口设备管道方向定义 */
#define USBH_SERIAL_PIPE_RX          1
#define USBH_SERIAL_PIPE_TX          2
#define USBH_SERIAL_PIPE_BOTH        3

/* \brief USB 转串口使用的停止位数取值 */
#define USB_SERIAL_ONE_STOPBIT       (1)    /* 1 位停止位  */
#define USB_SERIAL_ONE5_STOPBITS     (2)    /* 1.5 位停止位  */
#define USB_SERIAL_TWO_STOPBITS      (3)    /* 2   位停止位  */

/* \brief USB 转串口校验方法取值*/
#define USB_SERIAL_EVEN_PARITY       (1)    /* 偶效验  */
#define USB_SERIAL_ODD_PARITY        (2)    /* 奇效验  */
#define USB_SERIAL_NONE_PARITY       (3)    /* 无效验  */

/* \brief USB 转串口数据位*/
#define USB_SERIAL_CSIZE            (0xc)  /* bits 3 and 4 encode the character size */
#define USB_SERIAL_CS5              (0x5)  /* 5 bits */
#define USB_SERIAL_CS6              (0x6)  /* 6 bits */
#define USB_SERIAL_CS7              (0x7)  /* 7 bits */
#define USB_SERIAL_CS8              (0x8)  /* 8 bits */

/* \brief USB 主机转串口设备 VID 获取 */
#define USBH_SERIAL_DEV_VID_GET(p_serial)            USBH_DEV_VID_GET(p_serial->p_usb_fun)
/* \brief USB 主机转串口设备 PID 获取 */
#define USBH_SERIAL_DEV_PID_GET(p_serial)            USBH_DEV_PID_GET(p_serial->p_usb_fun)
/* \brief USB 主机转串口设备类获取 */
#define USBH_SERIAL_DEV_CLASS_GET(p_serial)          USBH_DEV_CLASS_GET(p_serial->p_usb_fun)
/* \brief USB 主机转串口设备端点 0 最大包大小获取  */
#define USBH_SERIAL_DEV_MPS0_GET(p_serial)           USBH_DEV_MPS0_GET(p_serial->p_usb_fun)
/* \brief USB 主机转串口设备第一个接口的接口编号获取  */
#define USBH_SERIAL_DEV_INTF_NUM_GET(p_serial)       USBH_FUNC_FIRST_INTFNUM_GET(p_serial->p_usb_fun)

/* \brief USB 主机转串口驱动信息句柄设置 */
#define USBH_SERIAL_DRV_HANDLE_SET(p_serial, handle) (p_serial->p_drv_handle = handle)
/* \brief USB 主机转串口驱动信息句柄获取 */
#define USBH_SERIAL_DRV_HANDLE_GET(p_serial)         (p_serial->p_drv_handle)

#define USB_SERIAL_DEV_MATCH_END                 \
        {                                        \
                USB_DEV_MATCH_END,               \
                "",                              \
        }

/* \brief USB 主机转串口设备匹配信息 */
struct usbh_serial_dev_match_info {
    struct usb_dev_match_info  info;
    char                      *p_drv_name;
};

/* \brief USB 主机转串口库结构体 */
struct usbh_serial_lib {
    struct usb_lib_base lib;               /* USB 库*/
    usb_bool_t          is_lib_deiniting;  /* 是否移除库*/
    int                 ref_cnt;           /* 引用计数*/
};

struct usbh_serial;
struct usbh_serial_port;

/* \brief USB 主机转串口管道结构体 */
struct usbh_serial_pipe {
    struct usbh_endpoint *p_ep;
    uint8_t              *p_buf;
    uint32_t              buf_size;
#if USB_OS_EN
    usb_mutex_handle_t    p_lock;
#endif
};

/* \brief USB 主机转串口接收管道结构体 */
struct usbh_serial_rx_pipe {
    struct usbh_serial_pipe pipe;
    uint32_t                data_pos;          /* 数据位置*/
    uint32_t                rd_pos;            /* 已读位置*/
};

/* \brief USB 主机转串口发送管道结构体 */
struct usbh_serial_tx_pipe {
    struct usbh_serial_pipe pipe;
    int                     time_out;
};

/* \brief USB 转串口端口配置结构体 */
struct usb_serial_port_cfg {
    uint32_t baud_rate;      /* 波特率*/
    uint32_t byte_size:4;    /* 指定当前端口使用的数据位(范围5~8)*/
    uint32_t parity   :2;    /* 指定端口数据传输的校验方法*/
    uint32_t stop_bits:2;    /* 指定端口当前使用的停止位数*/
    uint32_t dtr      :1;    /* 数据终端准备*/
    uint32_t rts      :1;    /* 请求发送*/
    uint32_t crtscts  :1;    /* 流控制*/
    uint32_t ixoff    :1;
    uint32_t f_dummy  :24;
};

/* \brief USB 主机转串口驱动信息 */
struct usbh_serial_drv_info {
    char *p_drv_name;
    int (*p_fn_init)(struct usbh_serial *p_userial);
    int (*p_fn_deinit)(struct usbh_serial *p_userial);
};

/* \brief USB 主机转串口端口操作函数集 */
struct usbh_serial_port_opts {
    /* 同步写回调函数 */
    int (*p_fn_port_write)(struct usbh_serial_port *p_port,
                           uint8_t                 *p_buf,
                           uint32_t                 buf_len,
                           uint32_t                *p_act_len);
    /* 设置串口配置回调函数*/
    int (*p_fn_port_cfg_set)(struct usbh_serial_port *p_port, struct usb_serial_port_cfg *p_cfg);
};

/* \brief USB 主机转串口设备端口结构体*/
struct usbh_serial_port {
    uint8_t                     idx;
    struct usbh_serial         *p_userial;
    struct usbh_serial_tx_pipe  tx_pipe;
    struct usbh_serial_rx_pipe  rx_pipe;
    struct usb_serial_port_cfg  cfg;
    usb_bool_t                  is_init;
};

/* \brief USB 主机转串口设备结构体*/
struct usbh_serial {
    char                          name[USB_NAME_LEN];  /* USB 转串口设备名字*/
    struct usbh_function         *p_usb_fun;           /* 相应的功能设备结构体*/
    uint8_t                       n_ports;             /* 端口数量*/
    void                         *p_drv_handle;        /* 驱动私有数据（放相关芯片类型的结构体）*/
    struct usbh_serial_drv_info  *p_drv_info;
    int                           ref_cnt;             /* USB 转串口设备引用计数*/
    struct usbh_serial_port      *p_ports;             /* 端口结构体*/
    struct usbh_serial_port_opts *p_opts;              /* 操作函数集*/
    usb_bool_t                    is_removed;          /* 移除状态*/
#if USB_OS_EN
    usb_mutex_handle_t            p_lock;
#endif
    struct usb_list_node          node;
};

/**
 * \brief USB 主机转串口内存分配函数
 */
void *usbh_serial_mem_alloc(uint32_t size);
/**
 * \brief USB 主机转串口内存释放函数
 */
void usbh_serial_mem_free(void *p_mem);
/**
 * \brief USB 主机转串口设备端口操作函数集设置
 *
 * \param[in] p_userial USB 转串口设备
 * \param[in] p_opts    要设置的操作函数集
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_opts_set(struct usbh_serial           *p_userial,
                              struct usbh_serial_port_opts *p_opts);
/**
 * \brief USB 主机转串口设备的端口数量获取
 *
 * \param[in]  p_userial USB 转串口设备
 * \param[out] p_n_ports 返回的端口数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_nport_get(struct usbh_serial *p_userial, uint8_t *p_n_ports);
/**
 * \brief USB 主机转串口设备名字获取
 *
 * \param[in]  p_userial USB 转串口设备结构体
 * \param[out] p_name    返回的名字缓存
 * \param[in]  name_size 名字缓存大小
 *
 * \retval 成功获取返回 USB_OK
 */
int usbh_serial_name_get(struct usbh_serial *p_userial, char *p_name, uint32_t name_size);
/**
 * \brief USB 主机转串口设备名字设置
 *
 * \param[in] p_userial  USB 转串口设备结构体
 * \param[in] p_name_new 要设置的新的名字
 *
 * \retval 成功设置返回 USB_OK
 */
int usbh_serial_name_set(struct usbh_serial *p_userial, char *p_name_new);
/**
 * \brief USB 主机转串口设备探测函数
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_probe(struct usbh_function *p_usb_fun);
/**
 * \brief USB 主机转串口设备创建
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 * \param[in] p_name    USB 转串口设备名字
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_create(struct usbh_function *p_usb_fun, char *p_name);
/**
 * \brief USB 主机转串口设备销毁
 *
 * \param[in] p_usb_fun 要销毁的 USB 转串口功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_destroy(struct usbh_function *p_usb_fun);
/**
 * \brief USB 主机转串口设备端口创建
 *
 * \param[in] p_userial USB 转串口设备
 * \param[in] n_ports   端口数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_ports_create(struct usbh_serial *p_userial, uint8_t n_ports);
/**
 * \brief USB 主机转串口设备端口销毁
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_ports_destroy(struct usbh_serial *p_userial);
/**
 * \brief USB 主机转串口设备端口初始化
 *
 * \param[in] p_userial    USB 转串口设备
 * \param[in] idx          端口索引
 * \param[in] p_ep_tx      发送端点
 * \param[in] tx_buf_size  发送天缓存大小
 * \param[in] tx_time_out  发送超时时间
 * \param[in] p_ep_rx      接收端点
 * \param[in] rx_buf_size  接收缓存大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_init(struct usbh_serial   *p_userial,
                          uint8_t               idx,
                          struct usbh_endpoint *p_ep_tx,
                          uint32_t              tx_buf_size,
                          int                   tx_time_out,
                          struct usbh_endpoint *p_ep_rx,
                          uint32_t              rx_buf_size);
/**
 * \brief USB 主机转串口设备端口反初始化
 *
 * \param[in] p_userial    USB 转串口设备
 * \param[in] idx          端口索引
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_deinit(struct usbh_serial *p_userial,
                            uint8_t             idx);
/**
 * \brief USB 主机转串口设备遍历复位
 */
void usbh_serial_traverse_reset(void);
/**
 * \brief USB 主机转串口设备遍历
 *
 * \param[out] p_name    返回的设备名字
 * \param[in]  name_size 名字缓存的大小
 * \param[out] p_vid     返回的 VID，
 * \param[out] p_pid     返回的 PID，
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_traverse(char     *p_name,
                         uint32_t  name_size,
                         uint16_t *p_vid,
                         uint16_t *p_pid);
/**
 * \brief USB 主机转串口设备打开函数
 *
 * \param[in]  p_handle      打开句柄
 * \param[in]  flag          打开标志，本接口支持两种打开方式：
 *                           USBH_DEV_OPEN_BY_NAME 是通过名字打开设备
 *                           USBH_DEV_OPEN_BY_UFUN 是通过 USB 功能结构体打开设备
 * \param[out] p_userial_ret 成功返回 USB 转串口结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_open(void                *p_handle,
                     uint8_t              flag,
                     struct usbh_serial **p_userial_ret);
/**
 * \brief USB 主机转串口设备关闭函数
 *
 * \param[in] p_userial 相关的 USB 转串口设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_close(struct usbh_serial *p_userial);
/**
 * \brief USB 主机转串口设备端口打开函数
 *
 * \param[in]  p_userial  USB 转串口设备
 * \param[in]  idx        要打开的端口索引
 * \param[out] p_port_ret 成功返回 USB 转串口端口结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_open(struct usbh_serial       *p_userial,
                          uint8_t                   idx,
                          struct usbh_serial_port **p_port_ret);
/**
 * \brief USB 主机转串口设备端口关闭函数
 *
 * \param[in] p_port 要关闭的 USB 转串口设备端口
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_close(struct usbh_serial_port *p_port);
/**
 * \brief USB 主机转串口设备端口发送数据(同步)
 *
 * \param[in]  p_port    USB 转串口设备端口
 * \param[in]  p_buf     要写的数据的缓存
 * \param[in]  buf_size  要写的数据缓存的长度
 * \param[out] p_act_len 返回实际写的长度
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_write(struct usbh_serial_port *p_port,
                           uint8_t                 *p_buf,
                           uint32_t                 buf_len,
                           uint32_t                *p_act_len);
/**
 * \brief USB 主机转串口设备端口读数据
 *
 * \param[in]  p_port    USB 转串口设备端口
 * \param[in]  p_buf     要读的数据的缓存
 * \param[in]  buf_len   要读的数据的长度
 * \param[out] p_act_len 返回实际读的长度
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_read(struct usbh_serial_port *p_port,
                          uint8_t                 *p_buf,
                          uint32_t                 buf_len,
                          uint32_t                *p_act_len);
/**
 * \brief USB 主机转串口设备端口配置获取
 *
 * \param[in]  p_port USB 转串口设备端口
 * \param[out] p_cfg  返回读到的配置
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_cfg_get(struct usbh_serial_port    *p_port,
                             struct usb_serial_port_cfg *p_cfg);
/**
 * \brief USB 主机转串口端口配置设置
 *
 * \param[in] p_port USB 转串口设备端口
 * \param[in] p_cfg  要设置的配置
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_cfg_set(struct usbh_serial_port    *p_port,
                             struct usb_serial_port_cfg *p_cfg);
/**
 * \brief USB 主机转串口设备端口接收缓存获取数据
 *
 * \param[in]  p_port    USB 转串口设备端口
 * \param[in]  p_buf     缓存
 * \param[in]  buf_len   要获取的长度
 * \param[out] p_act_len 实际获取到的长度
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_rx_buf_get(struct usbh_serial_port *p_port,
                                uint8_t                 *p_buf,
                                uint32_t                 buf_len,
                                uint32_t                *p_act_len);
/**
 * \brief USB 主机转串口设备端口接收缓存放入数据
 *
 * \param[in]  p_port    USB 转串口设备端口
 * \param[in]  p_buf     数据缓存
 * \param[in]  buf_size  数据缓存大小
 * \param[out] p_act_len 返回实际放入的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_rx_buf_put(struct usbh_serial_port *p_port,
                                uint8_t                 *p_buf,
                                uint32_t                 buf_len,
                                uint32_t                *p_act_len);
/**
 * \brief USB 主机转串口设备端口管道缓存大小获取
 *
 * \param[in]  p_port     USB 转串口设备端口结构体
 * \param[in]  pipe_dir   管道方向
 * \param[out] p_buf_size 要返回的缓存大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_buf_size_get(struct usbh_serial_port *p_port,
                                  uint8_t                  pipe_dir,
                                  uint32_t                *p_buf_size);
/**
 * \brief USB 主机转串口设备端口管道缓存大小设置
 *
 * \param[in] p_port   USB 转串口端口结构体
 * \param[in] pipe_dir 管道方向
 * \param[in] buf_size 缓存大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_buf_size_set(struct usbh_serial_port *p_port,
                                  uint8_t                  pipe_dir,
                                  uint32_t                 buf_size);
/**
 * \brief USB 主机转串口设备端口发送超时时间获取
 *
 * \param[in]  p_port     USB 转串口设备端口结构体
 * \param[out] p_time_out 返回的超时时间
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_tx_timeout_get(struct usbh_serial_port *p_port,
                                    int                     *p_time_out);
/**
 * \brief USB 主机转串口设备端口超时时间设置
 *
 * \param[in] p_port   USB 转串口设备端口结构体
 * \param[in] time_out 要设置的超时时间
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_tx_timeout_set(struct usbh_serial_port *p_port,
                                    int                      time_out);

/**
 * \brief USB 主机转串口设备数量获取
 *
 * \param[out] p_n_dev 存储返回的设备数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_lib_ndev_get(uint8_t *p_n_dev);
/**
 * \brief USB 主机转串口设备库相关初始化
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_lib_init(void);
/**
 * \brief USB 主机转串口设备库反初始化
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_lib_deinit(void);
#if USB_MEM_RECORD_EN
/**
 * \brief USB 主机转串口驱动内存记录获取
 *
 * \param[out] p_mem_record 内存记录
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_lib_mem_record_get(struct usb_mem_record *p_mem_record);
#endif
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif /* __USBH_CDC_SERIAL_DRV_H */

