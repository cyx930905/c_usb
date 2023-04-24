#ifndef __USBH_RNDIS_DRV_H
#define __USBH_RNDIS_DRV_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "core/include/host/class/cdc/ecm/usbh_cdc_ecm_drv.h"

/* \brief RNDIS 控制传输超时时间*/
#define RNDIS_CONTROL_TIMEOUT              (5 * 1000)

#define RNDIS_CONTROL_BUF_SIZE              1025
/* \brief 控制传输之前轮询状态*/
#define RNDIS_DRIVER_DATA_POLL_STATUS       1

/* \brief RNDIS 消息定义*/
#define RNDIS_MSG_COMPLETION                0x80000000
#define RNDIS_MSG_INIT                      0x00000002
#define RNDIS_MSG_HALT                      0x00000003
#define RNDIS_MSG_QUERY                     0x00000004
#define RNDIS_MSG_SET                       0x00000005
#define RNDIS_MSG_RESET                     0x00000006
#define RNDIS_MSG_RESET_C                  (RNDIS_MSG_RESET | RNDIS_MSG_COMPLETION)
#define RNDIS_MSG_INDICATE                  0x00000007
#define RNDIS_MSG_KEEPALIVE                 0x00000008
#define RNDIS_MSG_KEEPALIVE_C              (RNDIS_MSG_KEEPALIVE | RNDIS_MSG_COMPLETION)

/* \brief RNDIS 状态定义*/
#define RNDIS_STATUS_SUCCESS                0x00000000
#define RNDIS_STATUS_MEDIA_CONNECT          0x4001000B
#define RNDIS_STATUS_MEDIA_DISCONNECT       0x4001000C

#define RNDIS_PHYM_NOT_WIRELESS_FLAG        0x0001
#define RNDIS_PHYM_WIRELESS_FLAG            0x0002

/* \brief RNDIS_OID_GEN_PHYSICAL_MEDIUM 代码 */
#define RNDIS_PHYSICAL_MEDIUM_UNSPECIFIED   0x00000000
#define RNDIS_PHYSICAL_MEDIUM_WIRELESS_LAN  0x00000001

/* \brief RNDIS_OID_GEN_CURRENT_PACKET_FILTER 用的包过滤位 */
#define RNDIS_PACKET_TYPE_DIRECTED          0x00000001
#define RNDIS_PACKET_TYPE_MULTICAST         0x00000002
#define RNDIS_PACKET_TYPE_ALL_MULTICAST     0x00000004
#define RNDIS_PACKET_TYPE_BROADCAST         0x00000008
#define RNDIS_PACKET_TYPE_SOURCE_ROUTING    0x00000010
#define RNDIS_PACKET_TYPE_PROMISCUOUS       0x00000020
#define RNDIS_PACKET_TYPE_SMT               0x00000040
#define RNDIS_PACKET_TYPE_ALL_LOCAL         0x00000080
#define RNDIS_PACKET_TYPE_GROUP             0x00001000
#define RNDIS_PACKET_TYPE_ALL_FUNCTIONAL    0x00002000
#define RNDIS_PACKET_TYPE_FUNCTIONAL        0x00004000
#define RNDIS_PACKET_TYPE_MAC_FRAME         0x00008000

/* \brief NDIS 查询/设置请求用到的对象 ID 信息*/
/* \brief 通用请求对象*/
#define RNDIS_OID_802_3_PERMANENT_ADDRESS   0x01010101
#define RNDIS_OID_GEN_PHYSICAL_MEDIUM       0x00010202


#define RNDIS_OID_GEN_CURRENT_PACKET_FILTER 0x0001010E

/* \brief RNDIS 设备默认过滤 */
#define RNDIS_DEFAULT_FILTER (RNDIS_PACKET_TYPE_DIRECTED | RNDIS_PACKET_TYPE_BROADCAST | \
                              RNDIS_PACKET_TYPE_ALL_MULTICAST | RNDIS_PACKET_TYPE_PROMISCUOUS)

struct rndis_msg_hdr {
    uint32_t    msg_type;
    uint32_t    msg_len;
    /* followed by data that varies between messages */
    uint32_t    request_id;
    uint32_t    status;
} __attribute__ ((packed));

struct rndis_data_hdr {
    uint32_t msg_type;           /* RNDIS_MSG_PACKET */
    uint32_t msg_len;            /* rndis_data_hdr + data_len + pad */
    uint32_t data_offset;        /* 36 -- right after header */
    uint32_t data_len;           /* ... real packet size */

    uint32_t oob_data_offset;    /* zero */
    uint32_t oob_data_len;       /* zero */
    uint32_t num_oob;            /* zero */
    uint32_t packet_data_offset; /* zero */

    uint32_t packet_data_len;    /* zero */
    uint32_t vc_handle;          /* zero */
    uint32_t reserved;           /* zero */
} __attribute__ ((packed));

struct rndis_init {
    uint32_t msg_type;           /* RNDIS_MSG_INIT */
    uint32_t msg_len;            /* 24 */
    uint32_t request_id;
    uint32_t major_version;      /* of rndis (1.0) */
    uint32_t minor_version;
    uint32_t max_transfer_size;
} __attribute__ ((packed));

struct rndis_init_c {
    uint32_t msg_type;          /* RNDIS_MSG_INIT_C */
    uint32_t msg_len;
    uint32_t request_id;
    uint32_t status;
    uint32_t major_version;     /* of rndis (1.0) */
    uint32_t minor_version;
    uint32_t device_flags;
    uint32_t medium;            /* zero == 802.3 */
    uint32_t max_packets_per_message;
    uint32_t max_transfer_size;
    uint32_t packet_alignment;  /* max 7; (1<<n) bytes */
    uint32_t af_list_offset;    /* zero */
    uint32_t af_list_size;      /* zero */
} __attribute__ ((packed));

struct rndis_halt {
    uint32_t msg_type;          /* RNDIS_MSG_HALT */
    uint32_t msg_len;
    uint32_t request_id;
} __attribute__ ((packed));

struct rndis_query {
    uint32_t msg_type;          /* RNDIS_MSG_QUERY */
    uint32_t msg_len;
    uint32_t request_id;
    uint32_t oid;
    uint32_t len;
    uint32_t offset;
    uint32_t handle;            /* zero */
} __attribute__ ((packed));

struct rndis_query_c {
    uint32_t msg_type;          /* RNDIS_MSG_QUERY_C */
    uint32_t msg_len;
    uint32_t request_id;
    uint32_t status;
    uint32_t len;
    uint32_t offset;
} __attribute__ ((packed));

struct rndis_set {
    uint32_t msg_type;          /* RNDIS_MSG_SET */
    uint32_t msg_len;
    uint32_t request_id;
    uint32_t oid;
    uint32_t len;
    uint32_t offset;
    uint32_t handle;            /* zero */
} __attribute__ ((packed));

struct rndis_set_c {
    uint32_t msg_type;          /* RNDIS_MSG_SET_C */
    uint32_t msg_len;
    uint32_t request_id;
    uint32_t status;
} __attribute__ ((packed));

struct rndis_keepalive_c {
    uint32_t msg_type;              /* RNDIS_MSG_KEEPALIVE_C */
    uint32_t msg_len;
    uint32_t request_id;
    uint32_t status;
} __attribute__ ((packed));

struct rndis_indicate {
    uint32_t msg_type;             /* RNDIS_MSG_INDICATE */
    uint32_t msg_len;
    uint32_t status;
    uint32_t length;
    uint32_t offset;
    uint32_t diag_status;
    uint32_t error_offset;
    uint32_t message;
} __attribute__ ((packed));

/**
 * \brief USB 主机 RNDIS 设备探测函数
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_rndis_probe(struct usbh_function *p_usb_fun);
/**
 * \brief USB 主机 RNDIS 设备创建函数
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 * \param[in] p_name    USB 主机 RNDIS 设备名字
 *
 * \retval 成功返回 USB_OK
 */
int usbh_rndis_create(struct usbh_function *p_usb_fun, char *p_name);
/**
 * \brief USB 主机 RNDIS 设备销毁函数
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_rndis_destroy(struct usbh_function *p_usb_fun);
/**
 * \brief USB 主机 RNDIS 设备打开函数
 *
 * \param[in]  p_handle  打开句柄
 * \param[in]  flag      打开标志，本接口支持两种打开方式：
 *                       USBH_DEV_OPEN_BY_NAME 是通过名字打开设备
 *                       USBH_DEV_OPEN_BY_UFUN 是通过 USB 功能结构体打开设备
 * \param[out] p_net_ret 成功返回 USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_rndis_open(void             *p_handle,
                    uint8_t           flag,
                    struct usbh_net **p_net_ret);
/**
 * \brief USB 主机 RNDIS 设备关闭函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_rndis_close(struct usbh_net *p_net);
/**
 * \brief USB 主机 RNDIS 设备启动函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_rndis_start(struct usbh_net *p_net);
/**
 * \brief USB 主机 RNDIS 设备停止函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_rndis_stop(struct usbh_net *p_net);
/**
 * \brief USB 主机 RNDIS 设备写函数
 *
 * \param[in] p_net   USB 主机网络设备
 * \param[in] p_buf   写缓存
 * \param[in] buf_len 写缓存长度
 *
 * \retval 成功返回实际写的长度
 */
int usbh_rndis_write(struct usbh_net *p_net, uint8_t *p_buf, uint32_t buf_len);
/**
 * \brief USB 主机 RNDIS 设备读函数
 *
 * \param[in] p_net   USB 主机网络设备
 * \param[in] p_buf   读缓存
 * \param[in] buf_len 读缓存长度
 *
 * \retval 成功返回读到的数据长度
 */
int usbh_rndis_read(struct usbh_net *p_net, uint8_t *p_buf, uint32_t buf_len);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif /* __USBH_RNDIS_DRV_H */

