#ifndef __USBH_HID_DRV_H
#define __USBH_HID_DRV_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "core/include/specs/usb_hid_specs.h"
#include "core/include/host/core/usbh.h"
#include "core/include/usb_lib.h"
#include "adapter/usb_adapter.h"
#include "common/usb_common.h"
#include "utils/list/usb_list.h"

#if USB_OS_EN
#define UHID_LOCK_TIMEOUT            5000
#endif

/* \brief USB 人脸接口设备传输超时时间*/
#define UHID_XFER_TIMEOUT            5000

/* \brief USB 人体接口设备类描述类型 */
#define HID_DT_HID                  (USB_REQ_TYPE_CLASS | 0x01)
#define HID_DT_REPORT               (USB_REQ_TYPE_CLASS | 0x02)
#define HID_DT_PHYSICAL             (USB_REQ_TYPE_CLASS | 0x03)

/* \brief USB 人体接口设备描述符最大大小 */
#define HID_MAX_DESCRIPTOR_SIZE	     4096
/* \brief USB 人体接口设备最小的缓存大小*/
#define HID_MIN_BUFFER_SIZE	         64
/* \brief USB 人体接口设备最大的缓存大小*/
#define HID_MAX_BUFFER_SIZE	         4096

/* \brief USB 人体接口设备兼容标志 */
#define HID_QUIRK_INVERT                        0x00000001
#define HID_QUIRK_NOTOUCH                       0x00000002
#define HID_QUIRK_IGNORE                        0x00000004
#define HID_QUIRK_NOGET                         0x00000008
#define HID_QUIRK_HIDDEV_FORCE                  0x00000010
#define HID_QUIRK_BADPAD                        0x00000020
#define HID_QUIRK_MULTI_INPUT                   0x00000040
#define HID_QUIRK_HIDINPUT_FORCE                0x00000080
#define HID_QUIRK_NO_EMPTY_INPUT                0x00000100
#define HID_QUIRK_NO_INIT_INPUT_REPORTS         0x00000200
#define HID_QUIRK_ALWAYS_POLL                   0x00000400
#define HID_QUIRK_SKIP_OUTPUT_REPORTS           0x00010000
#define HID_QUIRK_SKIP_OUTPUT_REPORT_ID         0x00020000
#define HID_QUIRK_NO_OUTPUT_REPORTS_ON_INTR_EP  0x00040000
#define HID_QUIRK_FULLSPEED_INTERVAL            0x10000000
#define HID_QUIRK_NO_INIT_REPORTS               0x20000000
#define HID_QUIRK_NO_IGNORE                     0x40000000
#define HID_QUIRK_NO_INPUT_SYNC                 0x80000000

#define HID_GROUP_GENERIC                       0x0001
#define HID_GROUP_MULTITOUCH                    0x0002
#define HID_GROUP_SENSOR_HUB                    0x0003
#define HID_GROUP_MULTITOUCH_WIN_8              0x0004

/* \brief 特殊 VID 的人体接口设备组*/
#define HID_GROUP_RMI                           0x0100
#define HID_GROUP_WACOM                         0x0101
#define HID_GROUP_LOGITECH_DJ_DEVICE            0x0102

#define USB_VENDOR_ID_WACOM		                0x056a
#define USB_VENDOR_ID_SYNAPTICS		            0x06cb
#define USB_VENDOR_ID_MICROSOFT                 0x045e
#define USB_DEVICE_ID_SIDEWINDER_GV             0x003b
#define USB_DEVICE_ID_MS_OFFICE_KB              0x0048
#define USB_DEVICE_ID_WIRELESS_OPTICAL_DESKTOP_3_0 0x009d
#define USB_DEVICE_ID_MS_NE4K                   0x00db
#define USB_DEVICE_ID_MS_NE4K_JP                0x00dc
#define USB_DEVICE_ID_MS_LK6K                   0x00f9
#define USB_DEVICE_ID_MS_PRESENTER_8K_BT        0x0701
#define USB_DEVICE_ID_MS_PRESENTER_8K_USB       0x0713
#define USB_DEVICE_ID_MS_NE7K                   0x071d
#define USB_DEVICE_ID_MS_DIGITAL_MEDIA_3K       0x0730
#define USB_DEVICE_ID_MS_COMFORT_MOUSE_4500     0x076c
#define USB_DEVICE_ID_MS_SURFACE_PRO_2          0x0799
#define USB_DEVICE_ID_MS_TOUCH_COVER_2          0x07a7
#define USB_DEVICE_ID_MS_TYPE_COVER_2           0x07a9
#define USB_DEVICE_ID_MS_TYPE_COVER_3           0x07dc
#define USB_DEVICE_ID_MS_TYPE_COVER_3_JP        0x07dd

/* \brief 指定人体接口设备长条目标记 */
#define HID_ITEM_TAG_LONG                       15

/* \brief 人体接口设备全局栈大小 */
#define HID_GLOBAL_STACK_SIZE                   4
#define HID_COLLECTION_STACK_SIZE               4

#define HID_MAX_IDS                             256
#define HID_MAX_USAGES                          12288
#define HID_MAX_FIELDS                          256

#define HID_SCAN_FLAG_MT_WIN_8                 (1 << 0)
#define HID_SCAN_FLAG_VENDOR_SPECIFIC          (1 << 1)
#define HID_SCAN_FLAG_GD_POINTER               (1 << 2)
/* \brief USB 人体接口设备集合默认数量*/
#define HID_DEFAULT_NUM_COLLECTIONS	            16

/* \brief USB 人体接口设备库结构体*/
struct uhid_lib {
    struct usb_lib_base  lib;                 /* USB 库*/
    usb_bool_t           is_lib_deiniting;    /* 是否移除库*/
    int                  xfer_time_out;       /* 传输超时时间*/
    int                  ref_cnt;             /* 引用计数*/
};

struct usbh_hid_usage {
    uint32_t  hid;			    /* hid usage code */
    uint32_t  collection_index;	/* index into collection array */
    uint32_t  usage_index;		/* index into usage array */
	/* hidinput data */
    uint16_t  code;			    /* input driver code */
    uint8_t   type;			    /* input driver type */
    int8_t	  hat_min;		    /* hat switch fun */
    int8_t	  hat_max;		    /* ditto */
    int8_t	  hat_dir;		    /* ditto */
};

/* \brief USB 主机人体接口设备域结构体*/
struct usbh_hid_field {
//	unsigned  physical;		/* physical usage for this field */
//	unsigned  logical;		/* logical usage for this field */
//	unsigned  application;		/* application usage for this field */
    struct usbh_hid_usage *p_usage;	/* 这个功能的用法表 */
//	unsigned  maxusage;		/* maximum usage index */
//	unsigned  flags;		/* main-item flags (i.e. volatile,array,constant) */
//	unsigned  report_offset;	/* bit offset in the report */
//	unsigned  report_size;		/* size of this field in the report */
//	unsigned  report_count;		/* number of this field in the report */
//	unsigned  report_type;		/* (input,output,feature) */
    int32_t                *p_value;		/* last known value(s) */
//	__s32     logical_minimum;
//	__s32     logical_maximum;
//	__s32     physical_minimum;
//	__s32     physical_maximum;
//	__s32     unit_exponent;
//	unsigned  unit;
    struct usbh_hid_report *p_report;    /* 关联的报告 */
	uint32_t                idx;        /* 报告中的索引 */
//	/* hidinput data */
//	struct hid_input *hidinput;	/* associated input structure */
//	__u16 dpad;			/* dpad input code */
};

/* \brief USB 主机人体接口设备报告结构体*/
struct usbh_hid_report {
    struct usb_list_node   node;                    /* 当前节点*/
    uint32_t               id;                      /* 报告ID */
    uint32_t               type;					/* 报告类型 */
    struct usbh_hid_field *p_field[HID_MAX_FIELDS];	/* 报告域 */
    uint32_t               field_max;               /* 最大的有效域索引 */
    uint32_t               size;                    /* 报告大小 （位数） */
    struct usbh_hid       *p_hid;                   /* 关联的 USB 人体接口设备 */
};

/* \brief USB 主机人体接口设备报告枚举结构体*/
struct usbh_hid_report_enum {
    uint32_t                numbered;
    struct usb_list_head    report_list;
    struct usbh_hid_report *p_report_id_hash[HID_MAX_IDS];
};

/* \brief USB 人体接口设备集合结构体*/
struct usbh_hid_collection {
	uint32_t type;
	uint32_t usage;
	uint32_t level;
};

/* \brief USB 主机人体接口设备*/
struct usbh_hid {
    struct usbh_function       *p_usb_fun;               /* 相关的功能接口*/
    char                        name[USB_NAME_LEN];      /* 人体接口设备名字*/
#if USB_OS_EN
    usb_mutex_handle_t          p_lock;
#endif
    uint32_t                    quirks;                  /* 兼容*/
    uint32_t                    version;                 /* 版本*/
    uint32_t                    country;
    uint8_t                    *p_report_desc;           /* 报告描述符*/
    uint32_t                    report_desc_size;        /* 报告描述符大小*/
    uint16_t                    group;                   /* 报告组 */
    uint32_t                    buf_size;                /* 缓存大小*/
	struct usbh_hid_collection *p_collection;            /* 集合链表 */
    uint32_t                    collection_size;         /* 集合数量 */
    uint32_t                    collection_max;          /* 已分析的集合数量 */
    uint32_t                    application_max;         /* 应用数量 */
    struct usbh_hid_report_enum report_enum[HID_REPORT_TYPES];
    int                         ref_cnt;                 /* 引用计数*/
    uint8_t                    *p_buf_in;                /* 输入缓存 */
    struct usbh_trp            *p_trp_in;                /* 输入传输请求包*/
    struct usb_list_node        node;                    /* 设备节点*/
    usb_bool_t                  is_removed;              /* 移除标志*/
};

/* \brief USB 人体接口设备全局条目结构体*/
struct usbh_hid_global {
    uint32_t usage_page;
    int32_t  logical_minimum;
    int32_t  logical_maximum;
    int32_t  physical_minimum;
    int32_t  physical_maximum;
    int32_t  unit_exponent;
    uint32_t unit;
    uint32_t report_id;
    uint32_t report_size;
    uint32_t report_count;
};

/* \brief USB 人体接口设备本地条目结构体*/
struct usbh_hid_local {
    uint32_t usage[HID_MAX_USAGES];            /* usage array */
    uint32_t collection_index[HID_MAX_USAGES]; /* collection index array */
    uint32_t usage_index;
    uint32_t usage_minimum;
    uint32_t delimiter_depth;
    uint32_t delimiter_branch;
};

/* \brief USB 人体接口设备条目结构体*/
struct usbh_hid_item {
    uint32_t     format;
    uint8_t      size;
    uint8_t      type;
    uint8_t      tag;
    union {
        uint8_t  u8;
        int8_t   s8;
        uint16_t u16;
        int16_t  s16;
        uint32_t u32;
        int32_t  s32;
        uint8_t *p_longdata;
    } data;
};

/* \brief USB 人类接口设备分析器结构体 */
struct usbh_hid_parser {
    struct usbh_hid_global global;
    struct usbh_hid_global global_stack[HID_GLOBAL_STACK_SIZE];
    uint32_t               global_stack_ptr;
    struct usbh_hid_local  local;
    uint32_t               collection_stack[HID_COLLECTION_STACK_SIZE];
    uint32_t               collection_stack_ptr;
    struct usbh_hid       *p_hid;
    unsigned               scan_flags;
};

/**
 * \brief USB 人类接口设备探测函数
 *
 * \param[in] p_usb_fun USB 功能结构体
 *
 * \retval 是 USB 人类接口设备返回 USB_OK
 */
int usbh_hid_probe(struct usbh_function *p_usb_fun);
/**
 * \brief 获取 USB 人类接口设备名字
 *
 * \param[in] p_hub     USB 人类接口设备结构体
 * \param[in] p_name    存储返回的名字缓存
 * \param[in] name_size 名字缓存大小
 *
 * \retval 成功获取返回 USB_OK
 */
int usbh_hid_name_get(struct usbh_hid *p_hid,
                      char            *p_name,
                      uint32_t         name_size);
/**
 * \brief 设置 USB 人类接口设备名字
 *
 * \param[in] p_hub     USB 人类接口设备结构体
 * \param[in] p_name_new 要设置的新的名字
 *
 * \retval 成功设置返回 USB_OK
 */
int usbh_hid_name_set(struct usbh_hid *p_hid, char *p_name_new);
/**
 * \brief 创建一个 USB 人体交互设备
 *
 * \param[in] p_usb_fun 相关的 USB 功能接口
 * \param[in] p_name    USB 人体交互设备的名字
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_create(struct usbh_function *p_usb_fun,
                    char                 *p_name);
/**
 * \brief 销毁  USB 人体交互设备
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_destroy(struct usbh_function *p_usb_fun);
/**
 * \brief 打开一个 USB 人体接口设备
 *
 * \param[in]  name  设备名字
 * \param[in]  flag  打开标志，本接口支持三种打开方式：
 *                   USBH_DEV_OPEN_BY_NAME 是通过名字打开设备
 *                   USBH_DEV_OPEN_BY_FUN 是通过 USB 功能结构体打开设备
 * \param[out] p_hid 返回打开的 USB 人体接口设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_open(void *p_handle, uint8_t flag, struct usbh_hid **p_hid);
/**
 * \brief 关闭 USB 人体接口设备
 *
 * \param[in] p_hid USB 人体接口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_close(struct usbh_hid *p_hid);
/**
 * \brief 启动 USB 人体接口设备
 *
 * \param[in] p_hid 要启动的 USB 人体接口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_start(struct usbh_hid *p_hid);
/**
 * \brief 停止 USB 人体接口设备
 *
 * \param[in] p_hid 要停止的 USB 人体接口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_stop(struct usbh_hid *p_hid);

/**
 * \brief 初始化 USB 人体接口设备库
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_lib_init(void);
/**
 * \brief 反初始化 USB 人体接口设备库
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_lib_deinit(void);
/**
 * \brief 获取 USB 人体接口设备驱动内存记录
 *
 * \param[out] p_mem_record 返回内存记录
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_lib_mem_record_get(struct usb_mem_record *p_mem_record);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif

