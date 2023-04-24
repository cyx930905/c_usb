#ifndef __USB_COMMON_SPECS_H
#define __USB_COMMON_SPECS_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */

#include "common/usb_common.h"

/* \brief USB 版本 2.0 BCD */
#define USB_BCD_VERSION             0x0200       /* USB 2.0 */

/* \brief USB传输方向 */
#define USB_DIR_OUT                 0            /* OUT 主机端到设备端*/
#define USB_DIR_IN                  0x80         /* IN  设备端到主机端*/

/* \brief USB 请求类型 */
#define USB_REQ_TYPE_MASK           (0x03 << 5)
#define USB_REQ_TYPE_STANDARD       (0x00 << 5)  /* USB标准请求 */
#define USB_REQ_TYPE_CLASS          (0x01 << 5)  /* USB类请求 */
#define USB_REQ_TYPE_VENDOR         (0x02 << 5)  /* USB自定义请求 */
#define USB_REQ_TYPE_RESERVED       (0x03 << 5)  /* 保留 */

/* \brief USB 请求目标 */
#define USB_REQ_TAG_MASK               0x1f
#define USB_REQ_TAG_DEVICE             0x00      /* 设备 */
#define USB_REQ_TAG_INTERFACE          0x01      /* 接口 */
#define USB_REQ_TAG_ENDPOINT           0x02      /* 端点 */
#define USB_REQ_TAG_OTHER              0x03      /* 其他 */
#define USB_REQ_TAG_PORT               0x04      /* 端口 */
#define USB_REQ_TAG_RPIPE              0x05      /* 管道 */

/* \brief USB 请求 */
#define USB_REQ_GET_STATUS             0x00      /* 获取状态 */
#define USB_REQ_CLEAR_FEATURE          0x01      /* 清除特性 */
#define USB_REQ_SET_FEATURE            0x03      /* 设置特性 */
#define USB_REQ_SET_ADDRESS            0x05      /* 设置地址 */
#define USB_REQ_GET_DESCRIPTOR         0x06      /* 获取描述符 */
#define USB_REQ_SET_DESCRIPTOR         0x07      /* 设置描述符 */
#define USB_REQ_GET_CONFIGURATION      0x08      /* 获取配置 */
#define USB_REQ_SET_CONFIGURATION      0x09      /* 设置配置 */
#define USB_REQ_GET_INTERFACE          0x0A      /* 获取接口信息 */
#define USB_REQ_SET_INTERFACE          0x0B      /* 设置接口 */
#define USB_REQ_SYNCH_FRAME            0x0C      /* 同步帧 */

#define USB_REQ_SET_ENCRYPTION         0x0D      /* 设置加密 */
#define USB_REQ_GET_ENCRYPTION         0x0E      /* 获取加密 */
#define USB_REQ_RPIPE_ABORT            0x0E      /* RPipe中止 */
#define USB_REQ_SET_HANDSHAKE          0x0F      /* 设置握手 */
#define USB_REQ_RPIPE_RESET            0x0F      /* RPipe复位 */
#define USB_REQ_GET_HANDSHAKE          0x10      /* 获取握手 */
#define USB_REQ_SET_CONNECTION         0x11      /* 设置连接 */
#define USB_REQ_SET_SECURITY_DATA      0x12      /* 设置加密数据 */
#define USB_REQ_GET_SECURITY_DATA      0x13      /* 获取加密数据 */
#define USB_REQ_SET_WUSB_DATA          0x14      /* 配置无线USB数据 */
#define USB_REQ_LOOPBACK_DATA_WRITE    0x15      /* 回环数据写 */
#define USB_REQ_LOOPBACK_DATA_READ     0x16      /* 回环数据读 */
#define USB_REQ_SET_INTERFACE_DS       0x17      /* SET_INTERFACE_DS请求 */
#define USB_REQ_SET_SEL                0x30      /* SET_SEL请求 */
#define USB_REQ_SET_ISOCH_DELAY        0x31      /* SET_ISOCH_DELAY请求 */

/* \brief USB 设备速度定义 */
#define USB_SPEED_UNKNOWN           0
#define USB_SPEED_LOW               1       /* 低速(usb 1.1) */
#define USB_SPEED_FULL              2       /* 全速(usb 1.1) */
#define USB_SPEED_HIGH              3       /* 高速(usb 2.0) */
#define USB_SPEED_WIRELESS          4       /* 无线USB(usb 2.5) */
#define USB_SPEED_SUPER             5       /* 超高速(usb 3.0) */

/* \brief USB 设备类定义 */
#define USB_CLASS_PER_INTERFACE        0x00  /* 接口描述中定义 */
#define USB_CLASS_AUDIO                0x01  /* 音频设备类 */
#define USB_CLASS_COMM                 0x02  /* 通讯设备类 */
#define USB_CLASS_HID                  0x03  /* 人机交互设备类 */
#define USB_CLASS_PHYSICAL             0x05  /* 物理设备类 */
#define USB_CLASS_STILL_IMAGE          0x06  /* 图像设备类 */
#define USB_CLASS_PRINTER              0x07  /* 打印机设备类 */
#define USB_CLASS_MASS_STORAGE         0x08  /* 大容量存储类 */
#define USB_CLASS_HUB                  0x09  /* 集线器类 */
#define USB_CLASS_CDC_DATA             0x0a  /* CDC-Date通信设备类 */
#define USB_CLASS_CSCID                0x0b  /* 智能卡设备类 */
#define USB_CLASS_CONTENT_SEC          0x0d  /* 信息安全设备类 */
#define USB_CLASS_VIDEO                0x0e  /* 视频设备类 */
#define USB_CLASS_WIRELESS_CONTROLLER  0xe0  /* 无线控制器类 */
#define USB_CLASS_MISC                 0xef  /* 杂项类 */
#define USB_CLASS_APP_SPEC             0xfe  /* 应用专有规范 */
#define USB_CLASS_VENDOR_SPEC          0xff  /* 厂商自定义类 */
#define USB_SUBCLASS_VENDOR_SPEC       0xff  /* 产商特定子类*/

/* \brief USB 端点地址屏蔽位*/
#define USB_EP_NUM_MASK             0x0f
/* \brief USB 端点类型 */
#define USB_EP_TYPE_MASK            0x03
#define USB_EP_TYPE_CTRL            0x00     /* 控制端点 */
#define USB_EP_TYPE_ISO             0x01     /* 等时端点 */
#define USB_EP_TYPE_BULK            0x02     /* 批量端点 */
#define USB_EP_TYPE_INT             0x03     /* 中断端点 */

/* \brief USB 3.0 端点类型 */
#define USB_SS_EP_SYNCTYPE          0x0c
#define USB_SS_EP_SYNC_NONE        (0 << 2)
#define USB_SS_EP_SYNC_ASYNC       (1 << 2)
#define USB_SS_EP_SYNC_ADAPTIVE    (2 << 2)
#define USB_SS_EP_SYNC_SYNC        (3 << 2)

/* \brief USB 描述符类型定义 */
#define USB_DT_DEVICE                  0x01  /* 设备描述符 */
#define USB_DT_CONFIG                  0x02  /* 配置描述符 */
#define USB_DT_STRING                  0x03  /* 字符串描述符 */
#define USB_DT_INTERFACE               0x04  /* 接口描述符 */
#define USB_DT_ENDPOINT                0x05  /* 端点描述符(0端点外)*/
#define USB_DT_DEVICE_QUALIFIER        0x06  /* 设备限定描述符 */
#define USB_DT_OTHER_SPEED_CONFIG      0x07  /* 其他速率配置描述符 */
#define USB_DT_INTERFACE_POWER         0x08  /* 接口功耗描述符 */
#define USB_DT_OTG                     0x09  /* OTG描述符 */
#define USB_DT_DEBUG                   0x0a  /* 调试描述符 */
#define USB_DT_INTERFACE_ASSOCIATION   0x0b  /* 接口联合描述符 */
#define USB_DT_SECURITY                0x0c  /* 安全性描述符 */
#define USB_DT_KEY                     0x0d  /* 密码钥匙描述符 */
#define USB_DT_ENCRYPTION_TYPE         0x0e  /* 加密类型描述符 */
#define USB_DT_BOS                     0x0f  /* 二进制设备目标存储 */
#define USB_DT_DEVICE_CAPABILITY       0x10  /* 设备性能描述符 */
#define USB_DT_WIRELESS_ENDPOINT_COMP  0x11  /* 无线端点伙伴 */
#define USB_DT_WIRE_ADAPTER            0x21  /* DWA描述符 */
#define USB_DT_RPIPE                   0x22  /* RPipe描述符 */
#define USB_DT_CS_RADIO_CONTROL        0x23  /* Radio Control描述符 */
#define USB_DT_PIPE_USAGE              0x24  /* Pipe Usage描述符 */
#define USB_DT_SS_ENDPOINT_COMP        0x30  /* 超高速端点伙伴 */

/* \brief USB 从机特性定义 */
#define USB_DEV_SELF_POWERED            0    /* 自供电 */
#define USB_DEV_REMOTE_WAKEUP           1    /* 支持远程唤醒 */

#define USB_ENDPOINT_HALT               0

#pragma pack (push, 1)

/* \brief SETUP 包结构定义 */
struct usb_ctrlreq {
    uint8_t   request_type;  /* 数据流方向、请求类型、接收端等 */
    uint8_t   request;       /* 确认请求 */
    uint16_t  value;         /* 请求值 */
    uint16_t  index;         /* 请求索引*/
    uint16_t  length;        /* 数据阶段数据长度 */
};

/* \brief USB 描述符头部 */
struct usb_desc_head {
    uint8_t length;            /* 描述符字节长度 */
    uint8_t descriptor_type;   /* 描述符类型 */
};

/* \brief USB 设备描述符定义 */
struct usb_device_desc {
    uint8_t     length;             /* 描述符字节长度 */
    uint8_t     descriptor_type;    /* 常数(01h) */

    uint16_t    bcdUSB;             /* USB规范版本号 */
    uint8_t     device_class;       /* 类代码 */
    uint8_t     device_sub_class;   /* 子类代码 */
    uint8_t     device_protocol;    /* 协议代码 */
    uint8_t     max_packet_size0;   /* 端点0包最大尺寸 */
    uint16_t    id_vendor;          /* 厂商ID */
    uint16_t    id_product;         /* 产品ID */
    uint16_t    bcd_device;         /* 设备版本号 */
    uint8_t     i_manufacturer;     /* 制造商字符串描述符索引 */
    uint8_t     i_product;          /* 产品字符串描述符索引 */
    uint8_t     i_serial_number;    /* 序列号字符串描述符索引 */
    uint8_t     num_configurations; /* 配置数目 */
};

/* \brief USB 配置描述符定义 */
struct usb_config_desc {
    uint8_t     length;              /* 描述符字节长度 */
    uint8_t     descriptor_type;     /* 常数(02h) */

    uint16_t    total_length;        /* 配置描述符及其全部附属描述符字节数 */
    uint8_t     num_interfaces;      /* 配置中接口数量 */
    uint8_t     configuration_value; /* 配置编号 */
    uint8_t     i_configuration;     /* 配置字符串描述符索引 */
    uint8_t     attributes;          /* 附加特性(如：自供电、远程唤醒等)*/
    uint8_t     max_power;           /* 2mA单位的总线功耗 */
};

/* \brief USB 设备限定描述符定义 */
struct usb_qualifier_desc {
    uint8_t  bLength;               /* 描述符字节长度 */
    uint8_t  bDescriptorType;       /* 常数(06h) */

    uint16_t bcdUSB;                /* USB规范版本号 */
    uint8_t  bDeviceClass;          /* 类代码 */
    uint8_t  bDeviceSubClass;       /* 子类代码 */
    uint8_t  bDeviceProtocol;       /* 协议代码 */
    uint8_t  bMaxPacketSize0;       /* 端点0包最大尺寸 */
    uint8_t  bNumConfigurations;    /* 配置数目 */
    uint8_t  bRESERVED;             /* 保留 */
};

/* \brief USB 接口联合描述符定义 */
struct usb_interface_assoc_desc {
    uint8_t     length;              /* 描述符字节长度 */
    uint8_t     descriptor_type;     /* 常数(0Bh) */

    uint8_t     first_interface;     /* 功能联合的第一个接口的编号 */
    uint8_t     interface_count;     /* 功能联合的接口的数量 */
    uint8_t     function_class;      /* 类代码 */
    uint8_t     function_sub_class;  /* 子类代码 */
    uint8_t     function_protocol;   /* 协议代码 */
    uint8_t     i_function;          /* 功能字符串描述符索引 */
};

/* \brief USB 接口描述符定义 */
struct usb_interface_desc {
    uint8_t     length;              /* 描述符字节长度 */
    uint8_t     descriptor_type;     /* 常数(04h) */

    uint8_t     interface_number;    /* 接口编号 */
    uint8_t     alternate_setting;   /* 替代设置编号 */
    uint8_t     num_endpoints;       /* 所支持的端点数目(除端点0外) */
    uint8_t     interface_class;     /* 类代码 */
    uint8_t     interface_sub_class; /* 子类代码 */
    uint8_t     interface_protocol;  /* 协议代码 */
    uint8_t     i_interface;         /* 接口字符串描述符索引 */
};

/* \brief USB 端点描述符定义 */
struct usb_endpoint_desc {
    uint8_t     length;             /* 描述符字节长度 */
    uint8_t     descriptor_type;    /* 常数(05h) */

    uint8_t     endpoint_address;   /* 端点地址和方向 */
    uint8_t     attributes;         /* 补充信息（如传输类型） */
    uint16_t    max_packet_size;    /* 所支持的最大包尺寸
                                     * 第12，11位表明了每微帧的事务数量
                                     * 00(每微帧1个事务)
                                     * 01(每微帧2个事务)
                                     * 10(每微帧3个事务)
                                     * 11(保留)
                                     * 低11位指明了每个单独事务的有效数据字节数*/
    uint8_t     interval;           /* 轮询端点的时间间隔
                                     * 根据设备运行速度以帧(全/低速)或微帧(高速)表示单位(1ms或125us)
                                     * 全/高速同步端点，这个值必须在1~16，轮询端点的时间为2^(interval-1)微帧
                                     * 全/低速中断端点，这个值必须在1~255个帧
                                     * 高速中断端点，这个值必须在1——16，轮询端点的时间为2^(interval-1)微帧
                                     * 高速批量/控制输出端点，interval指定端点的最大NAK率，0表示没有NAKs
                                     * 其他值表示每interval微帧里最多一个NAK，这个值必选0~255*/

    /* 注意：这两个是用于音频端点*/
    uint8_t     refresh;
    uint8_t     synch_address;
};

/* \brief USB Microsoft OS 描述符*/
struct usb_mircosoft_os_desc {
    /* 暂时只支持 WCID 设备 */
    const uint8_t *p_compat_desc;  /* WCID 1.0/2.0 */
    const uint8_t *p_prop_desc;    /* WCID 1.0/2.0 */
};

#pragma pack (pop)

/************************ 集线器类规范相关定义 ****************************/
/* \brief 集线器类特性编号     USB2.0规范表11-17 */
#define USB_HUB_C_LOCAL_POWER      0
#define USB_HUB_C_OVER_CURRENT     1

/* \brief 集线器端口特性编号   USB2.0规范表11-17 */
#define USB_PORT_CONNECTION        0
#define USB_PORT_ENABLE            1
#define USB_PORT_SUSPEND           2     /* L2挂起*/
#define USB_PORT_OVER_CURRENT      3
#define USB_PORT_RESET             4
#define USB_PORT_FEAT_L1           5     /* L1 挂起*/
#define USB_PORT_POWER             8
#define USB_PORT_LOW_SPEED         9
#define USB_PORT_C_CONNECTION      16
#define USB_PORT_C_ENABLE          17
#define USB_PORT_C_SUSPEND         18
#define USB_PORT_C_OVER_CURRENT    19
#define USB_PORT_C_RESET           20
#define USB_PORT_C_TEST            21
#define USB_PORT_C_INDICATOR       22
#define USB_PORT_FEAT_C_PORT_L1    23

/* \brief wPortStatus 位域     USB2.0规范表11-21 */
#define USB_PORT_STAT_CONNECTION     0x0001
#define USB_PORT_STAT_ENABLE         0x0002
#define USB_PORT_STAT_SUSPEND        0x0004
#define USB_PORT_STAT_OVERCURRENT    0x0008
#define USB_PORT_STAT_RESET          0x0010
#define USB_PORT_STAT_L1             0x0020
/* \brief 位6和7保留*/
#define USB_PORT_STAT_POWER          0x0100
#define USB_PORT_STAT_LOW_SPEED      0x0200
#define USB_PORT_STAT_HIGH_SPEED     0x0400
#define USB_PORT_STAT_TEST           0x0800
#define USB_PORT_STAT_INDICATOR      0x1000

/* \brief wPortChange 位域     USB2.0规范表11-22和 USB2.0 LPM(链路电源管理)ECN 表4.10  位6-15保留 */
#define USB_PORT_STAT_C_CONNECTION   0x0001
#define USB_PORT_STAT_C_ENABLE       0x0002
#define USB_PORT_STAT_C_SUSPEND      0x0004
#define USB_PORT_STAT_C_OVERCURRENT  0x0008
#define USB_PORT_STAT_C_RESET        0x0010
#define USB_PORT_STAT_C_L1           0x0020

#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif /* __USB_COMMON_SPECS_H */

/* end of file */
