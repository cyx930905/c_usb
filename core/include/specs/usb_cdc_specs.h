#ifndef __USB_CDC_SPECS_H
#define __USB_CDC_SPECS_H


/* \brief 通信设备类规范版本 1.1.0 */
#define USB_CDC_V1_10                   0x0110

/* 功能描述符的描述符类型字段的类型值
 * (usbcdc11.pdf, 5.2.3, Table 24) */
#define USB_CDC_CS_INTERFACE            0x24
#define USB_CDC_CS_ENDPOINT             0x25

/* \brief USB 通讯设备类子类定义 */
#define USB_CDC_SUBCLASS_NONE           0x00
#define USB_CDC_SUBCLASS_DLCM           0x01
#define USB_CDC_SUBCLASS_ACM            0x02
#define USB_CDC_SUBCLASS_TCM            0x03
#define USB_CDC_SUBCLASS_MCCM           0x04
#define USB_CDC_SUBCLASS_CCM            0x05
#define USB_CDC_SUBCLASS_ETHERNET       0x06
#define USB_CDC_SUBCLASS_ATM            0x07
#define USB_CDC_SUBCLASS_WHC            0x08
#define USB_CDC_SUBCLASS_DMM            0x09
#define USB_CDC_SUBCLASS_MDLM           0x0a
#define USB_CDC_SUBCLASS_OBEX           0x0b
#define USB_CDC_SUBCLASS_EEM            0x0c
#define USB_CDC_SUBCLASS_NCM            0x0d
#define USB_CDC_SUBCLASS_MBIM           0x0e

/* \brief USB 通讯设备类协议定义 */
#define USB_CDC_PROTOCOL_NONE           0x00
#define USB_CDC_PROTOCOL_V25TER         0x01
#define USB_CDC_PROTOCOL_I430           0x30
#define USB_CDC_PROTOCOL_HDLC           0x31
#define USB_CDC_PROTOCOL_TRANS          0x32
#define USB_CDC_PROTOCOL_Q921M          0x50
#define USB_CDC_PROTOCOL_Q921           0x51
#define USB_CDC_PROTOCOL_Q921TM         0x52
#define USB_CDC_PROTOCOL_V42BIS         0x90
#define USB_CDC_PROTOCOL_Q931           0x91
#define USB_CDC_PROTOCOL_V120           0x92
#define USB_CDC_PROTOCOL_CAPI20         0x93
#define USB_CDC_PROTOCOL_HOST           0xFD
#define USB_CDC_PROTOCOL_PUFD           0xFE
#define USB_CDC_PROTOCOL_VENDOR         0xFF
#define USB_CDC_PROTOCOL_EEM            0x07

/* \brief 功能描述符的描述符子类型字段的类型值
 *        (usbcdc11.pdf, 5.2.3, Table 25) */
#define USB_CDC_HEADER                              0x00
#define USB_CDC_CALL_MANAGEMENT                     0x01
#define USB_CDC_ABSTRACT_CONTROL_MANAGEMENT         0x02
#define USB_CDC_DIRECT_LINE_MANAGEMENT              0x03
#define USB_CDC_TELEPHONE_RINGER                    0x04
#define USB_CDC_REPORTING_CAPABILITIES              0x05
#define USB_CDC_UNION                               0x06
#define USB_CDC_COUNTRY_SELECTION                   0x07
#define USB_CDC_TELEPHONE_OPERATIONAL_MODES         0x08
#define USB_CDC_USB_TERMINAL                        0x09
#define USB_CDC_NETWORK_CHANNEL                     0x0A
#define USB_CDC_PROTOCOL_UNIT                       0x0B
#define USB_CDC_EXTENSION_UNIT                      0x0C
#define USB_CDC_MULTI_CHANNEL_MANAGEMENT            0x0D
#define USB_CDC_CAPI_CONTROL_MANAGEMENT             0x0E
#define USB_CDC_ETHERNET_NETWORKING                 0x0F
#define USB_CDC_ATM_NETWORKING                      0x10

/* \brief USB 通讯设备类命令定义 */
#define USB_CDC_SEND_ENCAPSULATED_COMMAND   0x00
#define USB_CDC_GET_ENCAPSULATED_RESPONSE   0x01
#define USB_CDC_SET_COMM_FEATURE            0x02
#define USB_CDC_GET_COMM_FEATURE            0x03
#define USB_CDC_CLEAR_COMM_FEATURE          0x04
#define USB_CDC_SET_AUX_LINE_STATE          0x10
#define USB_CDC_SET_HOOK_STATE              0x11
#define USB_CDC_PULSE_SETUP                 0x12
#define USB_CDC_SEND_PULSE                  0x13
#define USB_CDC_SET_PULSE_TIME              0x14
#define USB_CDC_RING_AUX_JACK               0x15
#define USB_CDC_SET_LINE_CODING             0x20
#define USB_CDC_GET_LINE_CODING             0x21
#define USB_CDC_SET_CONTROL_LINE_STATE      0x22
#define USB_CDC_SEND_BREAK                  0x23
#define USB_CDC_SET_RINGER_PARMS            0x30
#define USB_CDC_GET_RINGER_PARMS            0x31
#define USB_CDC_SET_OPERATION_PARMS         0x32
#define USB_CDC_GET_OPERATION_PARMS         0x33
#define USB_CDC_SET_LINE_PARMS              0x34
#define USB_CDC_GET_LINE_PARMS              0x35
#define USB_CDC_DIAL_DIGITS                 0x36
#define USB_CDC_SET_UNIT_PARAMETER          0x37
#define USB_CDC_GET_UNIT_PARAMETER          0x38
#define USB_CDC_CLEAR_UNIT_PARAMETER        0x39
#define USB_CDC_GET_PROFILE                 0x3A
#define USB_CDC_SET_ETH_MULTICAST_FILTERS   0x40
#define USB_CDC_SET_ETH_POWER_MGMT_FILT     0x41
#define USB_CDC_GET_ETH_POWER_MGMT_FILT     0x42
#define USB_CDC_SET_ETH_PACKET_FILTER       0x43
#define USB_CDC_GET_ETH_STATISTIC           0x44
#define USB_CDC_SET_ATM_DATA_FORMAT         0x50
#define USB_CDC_GET_ATM_DEVICE_STATISTICS   0x51
#define USB_CDC_SET_ATM_DEFAULT_VC          0x52
#define USB_CDC_GET_ATM_VC_STATISTICS       0x53

/* \brief 特定类描述符*/
#define USB_CDC_HEADER_TYPE                 0x00     /* 头描述符*/
#define USB_CDC_CALL_MANAGEMENT_TYPE        0x01     /* call_mgmt_descriptor */
#define USB_CDC_ACM_TYPE                    0x02     /* ACM 描述符*/
#define USB_CDC_UNION_TYPE                  0x06     /* 联合描述符*/
#define USB_CDC_COUNTRY_TYPE                0x07
#define USB_CDC_NETWORK_TERMINAL_TYPE       0x0a     /* 网络中端描述符*/
#define USB_CDC_ETHERNET_TYPE               0x0f     /* 以太网描述符*/
#define USB_CDC_WHCM_TYPE                   0x11
#define USB_CDC_MDLM_TYPE                   0x12     /* 移动直行模型描述符 */
#define USB_CDC_MDLM_DETAIL_TYPE            0x13     /* 移动直行模型细节描述符 */
#define USB_CDC_DMM_TYPE                    0x14
#define USB_CDC_OBEX_TYPE                   0x15
#define USB_CDC_NCM_TYPE                    0x1a
#define USB_CDC_MBIM_TYPE                   0x1b
#define USB_CDC_MBIM_EXTENDED_TYPE          0x1c

/* \brief 表62； 多播滤波器位*/
#define USB_CDC_PACKET_TYPE_PROMISCUOUS    (1 << 0)
#define USB_CDC_PACKET_TYPE_ALL_MULTICAST  (1 << 1) /* 无滤波器 */
#define USB_CDC_PACKET_TYPE_DIRECTED       (1 << 2)
#define USB_CDC_PACKET_TYPE_BROADCAST      (1 << 3)
#define USB_CDC_PACKET_TYPE_MULTICAST      (1 << 4) /* 已滤波*/

/* \brief 类规范控制请求 （6.2）*/
#define USB_CDC_SEND_ENCAPSULATED_COMMAND       0x00
#define USB_CDC_GET_ENCAPSULATED_RESPONSE       0x01
#define USB_CDC_REQ_SET_LINE_CODING             0x20
#define USB_CDC_REQ_GET_LINE_CODING             0x21
#define USB_CDC_REQ_SET_CONTROL_LINE_STATE      0x22
#define USB_CDC_REQ_SEND_BREAK                  0x23
#define USB_CDC_SET_ETHERNET_MULTICAST_FILTERS  0x40
#define USB_CDC_SET_ETHERNET_PM_PATTERN_FILTER  0x41
#define USB_CDC_GET_ETHERNET_PM_PATTERN_FILTER  0x42
#define USB_CDC_SET_ETHERNET_PACKET_FILTER      0x43
#define USB_CDC_GET_ETHERNET_STATISTIC          0x44
#define USB_CDC_GET_NTB_PARAMETERS              0x80
#define USB_CDC_GET_NET_ADDRESS                 0x81
#define USB_CDC_SET_NET_ADDRESS                 0x82
#define USB_CDC_GET_NTB_FORMAT                  0x83
#define USB_CDC_SET_NTB_FORMAT                  0x84
#define USB_CDC_GET_NTB_INPUT_SIZE              0x85
#define USB_CDC_SET_NTB_INPUT_SIZE              0x86
#define USB_CDC_GET_MAX_DATAGRAM_SIZE           0x87
#define USB_CDC_SET_MAX_DATAGRAM_SIZE           0x88
#define USB_CDC_GET_CRC_MODE                    0x89
#define USB_CDC_SET_CRC_MODE                    0x8a

struct usb_cdc_line_coding {
    uint32_t  dte_rate;
    uint8_t   char_format;
    uint8_t   parity_type;
    uint8_t   data_bits;
};

/* \brief "header 功能描述符" CDC 规范 5.2.3.1*/
struct usb_cdc_header_desc {
    uint8_t    length;               /* 描述符大小(字节)*/
    uint8_t    descriptor_type;      /* 描述符类型*/
    uint8_t    descriptor_sub_type;  /* 描述符子类类型*/
    uint16_t   bcd_cdc;              /* 用于通信设备的USB类定义规范发布号(二进制编码十进制)*/
} __attribute__ ((packed));

/* \brief "union 功能描述符" CDC 规范 5.2.3.8 */
struct usb_cdc_union_desc {
    uint8_t    length;               /* 描述符大小(字节)*/
    uint8_t    descriptor_type;      /* 描述符类型*/
    uint8_t    descriptor_sub_type;  /* 描述符子类类型*/
    /* 配置中接口的索引是从0开始*/
    uint8_t    master_interface0;    /* 通信或数据类接口的接口号，指定为联合的控制接口*/
    uint8_t    slave_interface0;     /* 联合中第一个子接口的接口号*/
    /* 这里可以添加其他子接口*/
} __attribute__ ((packed));

/* \brief "以太网网络功能描述符" CDC 规范 5.2.3.16 */
struct usb_cdc_ether_desc {
    uint8_t    length;                /* 描述符大小(字节)*/
    uint8_t    descriptor_type;       /* 描述符类型*/
    uint8_t    descriptor_sub_type;   /* 描述符子类型*/

    uint8_t    mac_address;           /* 字符串描述符的索引。这个字符串描述符包含 48 位以太网 MAC 地址。
                                       * MAC 地址的 Unicode 表示如下：第一个 Unicode 字符表示网络字节序
                                       * 的 MAC 地址第一个字节的高位半字节，下一个字符表示接下来的四
                                       * 位，以此类推。mac_address 不能为 0。*/
    uint32_t  ethernet_statistics;    /* 指示设备收集的以太网统计功能。如果有一个位设置为0，则主机
                                       * 网络驱动程序将保留响应统计的计数（如果可以）。*/
    uint16_t  max_segment_size;       /* 以太网设备支持的最大段大小，一般是1514字节，但可以被扩展*/
    uint16_t  number_mc_filters;      /* 包含主机可以配置的多播滤波器的数量。*/
    uint8_t   number_power_filters;   /* 包含可用于导致主机唤醒的典型滤波器的数量*/
} __attribute__ ((packed));

/* \brief "抽象控制管理描述符" CDC 规范 5.2.3.3 */
struct usb_cdc_acm_descriptor {
    uint8_t   length;
    uint8_t   descriptor_type;
    uint8_t   descriptor_sub_type;

    uint8_t   capabilities;
} __attribute__ ((packed));

/* \brief "移动直行模型功能性描述符" CDC WMC(Wireless Mobile Communication Device无线移动通讯设备)规范  6.7.2.3 */
struct usb_cdc_mdlm_desc {
    uint8_t   length;
    uint8_t   descriptor_type;
    uint8_t   descriptor_sub_type;

    uint16_t  bcd_version;
    uint8_t   guid[16];
} __attribute__ ((packed));

/* \brief "移动直行模型细节功能性描述符" CDC WMC(无线移动通讯设备)规范  6.7.2.4 */
struct usb_cdc_mdlm_detail_desc {
    uint8_t   length;
    uint8_t   descriptor_type;
    uint8_t   descriptor_sub_type;

    /* 类型关联于 usb_cdc_mdlm_desc.guid */
    uint8_t   guid_descriptor_type;
    uint8_t   detail_data[0];
} __attribute__ ((packed));

/* \brief 通过中断传输发送的类特定通知(6.3)
 *
 *        CDC 规范的 3.8.2 节表 11 列出了以太网的通知
 *        第 3.6.2.1 节表 5 规定了 RNDIS 接受的 ACM 通知
 *        RNDIS 还定义了自己的位不兼容通知 */
#define USB_CDC_NOTIFY_NETWORK_CONNECTION   0x00    /* 网络连接*/
#define USB_CDC_NOTIFY_RESPONSE_AVAILABLE   0x01    /* 得到响应*/
#define USB_CDC_NOTIFY_SERIAL_STATE         0x20    /* 串行状态*/
#define USB_CDC_NOTIFY_SPEED_CHANGE         0x2a    /* 速度该表*/

struct usb_cdc_notification {
    uint8_t   request_type;
    uint8_t   notification_type;
    uint16_t  value;
    uint16_t  index;
    uint16_t  length;
} __attribute__ ((packed));

#endif
