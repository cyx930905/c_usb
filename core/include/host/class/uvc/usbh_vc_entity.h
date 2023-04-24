#ifndef __USBH_VC_ENTITY_H
#define __USBH_VC_ENTITY_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "core/include/host/core/usbh.h"
#include "core/include/host/class/uvc/usbh_vc_stream.h"
#include "adapter/usb_adapter.h"
#include "common/usb_common.h"
#include "common/list/usb_list.h"

#define MEDIA_PAD_FL_SINK               (1 << 0)
#define MEDIA_PAD_FL_SOURCE             (1 << 1)
#define MEDIA_PAD_FL_MUST_CONNECT       (1 << 2)

#define UVC_ENTITY_FLAG_DEFAULT         (1 << 0)

/* \brief 检查终端方向*/
#define UVC_TERM_DIRECTION(p_term)     ((p_term)->type & 0x8000)
/* \brief 检查实体类型*/
#define UVC_ENTITY_TYPE(p_entity)      ((p_entity)->type & 0x7fff)
/* \brief 检查实体是不是一个终端*/
#define UVC_ENTITY_IS_TERM(p_entity)  (((p_entity)->type & 0xff00) != 0)
/* 判断实体是不是输入终端类型*/
#define UVC_ENTITY_IS_ITERM(p_entity) \
    (UVC_ENTITY_IS_TERM(p_entity) && ((p_entity)->type & 0x8000) == UVC_TERM_INPUT)
/* \brief 判断实体是不是输出终端类型*/
#define UVC_ENTITY_IS_OTERM(p_entity) \
    (UVC_ENTITY_IS_TERM(p_entity) && ((p_entity)->type & 0x8000) == UVC_TERM_OUTPUT)

/* \brief 全局唯一标识符(Globally Unique Identifier,GUIDs) */
#define UVC_GUID_UVC_CAMERA \
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}
#define UVC_GUID_UVC_OUTPUT \
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02}
#define UVC_GUID_UVC_MEDIA_TRANSPORT_INPUT \
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03}
#define UVC_GUID_UVC_PROCESSING \
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01}
#define UVC_GUID_UVC_SELECTOR \
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02}

#define UVC_GUID_FORMAT_MJPEG \
    { 'M',  'J',  'P',  'G', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_YUY2 \
    { 'Y',  'U',  'Y',  '2', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_YUY2_ISIGHT \
    { 'Y',  'U',  'Y',  '2', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0x00, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_NV12 \
    { 'N',  'V',  '1',  '2', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_YV12 \
    { 'Y',  'V',  '1',  '2', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_I420 \
    { 'I',  '4',  '2',  '0', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_UYVY \
    { 'U',  'Y',  'V',  'Y', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_Y800 \
    { 'Y',  '8',  '0',  '0', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_Y8 \
    { 'Y',  '8',  ' ',  ' ', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_Y10 \
    { 'Y',  '1',  '0',  ' ', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_Y12 \
    { 'Y',  '1',  '2',  ' ', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_Y16 \
    { 'Y',  '1',  '6',  ' ', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_BY8 \
    { 'B',  'Y',  '8',  ' ', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_BA81 \
    { 'B',  'A',  '8',  '1', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_GBRG \
    { 'G',  'B',  'R',  'G', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_GRBG \
    { 'G',  'R',  'B',  'G', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_RGGB \
    { 'R',  'G',  'G',  'B', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_RGBP \
    { 'R',  'G',  'B',  'P', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_BGR3 \
    { 0x7d, 0xeb, 0x36, 0xe4, 0x4f, 0x52, 0xce, 0x11, \
     0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}
#define UVC_GUID_FORMAT_M420 \
    { 'M',  '4',  '2',  '0', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}

#define UVC_GUID_FORMAT_H264 \
    { 'H',  '2',  '6',  '4', 0x00, 0x00, 0x10, 0x00, \
     0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}

/* \brief 控件标志*/
#define UVC_CTRL_FLAG_SET_CUR       (1 << 0)     /* 设置当前值*/
#define UVC_CTRL_FLAG_GET_CUR       (1 << 1)     /* 获取当前值*/
#define UVC_CTRL_FLAG_GET_MIN       (1 << 2)     /* 获取最小值*/
#define UVC_CTRL_FLAG_GET_MAX       (1 << 3)     /* 获取最大值*/
#define UVC_CTRL_FLAG_GET_RES       (1 << 4)
#define UVC_CTRL_FLAG_GET_DEF       (1 << 5)
/* \brief 控件应该在挂起时保存，在恢复时还原*/
#define UVC_CTRL_FLAG_RESTORE       (1 << 6)
/* \brief 控件可以通过摄像头更新 */
#define UVC_CTRL_FLAG_AUTO_UPDATE   (1 << 7)
/* \brief 获取控件范围值*/
#define UVC_CTRL_FLAG_GET_RANGE \
    (UVC_CTRL_FLAG_GET_CUR | UVC_CTRL_FLAG_GET_MIN | \
     UVC_CTRL_FLAG_GET_MAX | UVC_CTRL_FLAG_GET_RES | UVC_CTRL_FLAG_GET_DEF)

/* \brief 控件类*/
#define UVC_CTRL_CLASS_USER          0x00980000         /* 旧类型的“用户”控制*/
#define UVC_CTRL_CLASS_MPEG          0x00990000         /* MPEG压缩控制*/
#define UVC_CTRL_CLASS_CAMERA        0x009a0000         /* 摄像头类控制*/
#define UVC_CTRL_CLASS_FM_TX         0x009b0000         /* FM调制器控制*/
#define UVC_CTRL_CLASS_FLASH         0x009c0000         /* 摄像头闪光灯控制*/
#define UVC_CTRL_CLASS_JPEG          0x009d0000         /* JPEG压缩控制*/
#define UVC_CTRL_CLASS_IMAGE_SOURCE  0x009e0000         /* 图像源控制*/
#define UVC_CTRL_CLASS_IMAGE_PROC    0x009f0000         /* 图像处理控制*/
#define UVC_CTRL_CLASS_DV            0x00a00000         /* 数字视频控制*/
#define UVC_CTRL_CLASS_FM_RX         0x00a10000         /* FM接收器控制*/
#define UVC_CTRL_CLASS_RF_TUNER      0x00a20000         /* RF调谐器控制*/
#define UVC_CTRL_CLASS_DETECT        0x00a30000         /* 检测控制*/

/* \brief 用户类控制 ID*/
#define UVC_CID_BASE                        (UVC_CTRL_CLASS_USER | 0x900)
#define UVC_CID_USER_BASE                    UVC_CID_BASE
#define UVC_CID_USER_CLASS                  (UVC_CTRL_CLASS_USER | 1)
#define UVC_CID_BRIGHTNESS                  (UVC_CID_BASE + 0)
#define UVC_CID_CONTRAST                    (UVC_CID_BASE + 1)
#define UVC_CID_SATURATION                  (UVC_CID_BASE + 2)
#define UVC_CID_HUE                         (UVC_CID_BASE + 3)
#define UVC_CID_AUDIO_VOLUME                (UVC_CID_BASE + 5)
#define UVC_CID_AUDIO_BALANCE               (UVC_CID_BASE + 6)
#define UVC_CID_AUDIO_BASS                  (UVC_CID_BASE + 7)
#define UVC_CID_AUDIO_TREBLE                (UVC_CID_BASE + 8)
#define UVC_CID_AUDIO_MUTE                  (UVC_CID_BASE + 9)
#define UVC_CID_AUDIO_LOUDNESS              (UVC_CID_BASE + 10)
#define UVC_CID_BLACK_LEVEL                 (UVC_CID_BASE + 11) /* Deprecated */
#define UVC_CID_AUTO_WHITE_BALANCE          (UVC_CID_BASE + 12)
#define UVC_CID_DO_WHITE_BALANCE            (UVC_CID_BASE + 13)
#define UVC_CID_RED_BALANCE                 (UVC_CID_BASE + 14)
#define UVC_CID_BLUE_BALANCE                (UVC_CID_BASE + 15)
#define UVC_CID_GAMMA                       (UVC_CID_BASE + 16)
#define UVC_CID_WHITENESS                   (UVC_CID_GAMMA) /* Deprecated */
#define UVC_CID_EXPOSURE                    (UVC_CID_BASE + 17)
#define UVC_CID_AUTOGAIN                    (UVC_CID_BASE + 18)
#define UVC_CID_GAIN                        (UVC_CID_BASE + 19)
#define UVC_CID_HFLIP                       (UVC_CID_BASE + 20)
#define UVC_CID_VFLIP                       (UVC_CID_BASE + 21)

#define UVC_CID_POWER_LINE_FREQUENCY        (UVC_CID_BASE + 24)

#define UVC_CID_HUE_AUTO                    (UVC_CID_BASE + 25)
#define UVC_CID_WHITE_BALANCE_TEMPERATURE   (UVC_CID_BASE + 26)
#define UVC_CID_SHARPNESS                   (UVC_CID_BASE + 27)
#define UVC_CID_BACKLIGHT_COMPENSATION      (UVC_CID_BASE + 28)
#define UVC_CID_CHROMA_AGC                  (UVC_CID_BASE + 29)
#define UVC_CID_COLOR_KILLER                (UVC_CID_BASE + 30)
#define UVC_CID_COLORFX                     (UVC_CID_BASE + 31)

/* \brief 摄像头类控制 ID */
#define UVC_CID_CAMERA_CLASS_BASE           (UVC_CTRL_CLASS_CAMERA | 0x900)
#define UVC_CID_CAMERA_CLASS                (UVC_CTRL_CLASS_CAMERA | 1)

#define UVC_CID_EXPOSURE_AUTO               (UVC_CID_CAMERA_CLASS_BASE + 1)
#define UVC_CID_EXPOSURE_ABSOLUTE           (UVC_CID_CAMERA_CLASS_BASE + 2)
#define UVC_CID_EXPOSURE_AUTO_PRIORITY      (UVC_CID_CAMERA_CLASS_BASE + 3)
#define UVC_CID_PAN_RELATIVE                (UVC_CID_CAMERA_CLASS_BASE + 4)
#define UVC_CID_TILT_RELATIVE               (UVC_CID_CAMERA_CLASS_BASE + 5)
#define UVC_CID_PAN_RESET                   (UVC_CID_CAMERA_CLASS_BASE + 6)
#define UVC_CID_TILT_RESET                  (UVC_CID_CAMERA_CLASS_BASE + 7)
#define UVC_CID_PAN_ABSOLUTE                (UVC_CID_CAMERA_CLASS_BASE + 8)
#define UVC_CID_TILT_ABSOLUTE               (UVC_CID_CAMERA_CLASS_BASE + 9)
#define UVC_CID_FOCUS_ABSOLUTE              (UVC_CID_CAMERA_CLASS_BASE + 10)
#define UVC_CID_FOCUS_RELATIVE              (UVC_CID_CAMERA_CLASS_BASE + 11)
#define UVC_CID_FOCUS_AUTO                  (UVC_CID_CAMERA_CLASS_BASE + 12)
#define UVC_CID_ZOOM_ABSOLUTE               (UVC_CID_CAMERA_CLASS_BASE + 13)
#define UVC_CID_ZOOM_RELATIVE               (UVC_CID_CAMERA_CLASS_BASE + 14)
#define UVC_CID_ZOOM_CONTINUOUS             (UVC_CID_CAMERA_CLASS_BASE + 15)
#define UVC_CID_PRIVACY                     (UVC_CID_CAMERA_CLASS_BASE + 16)
#define UVC_CID_IRIS_ABSOLUTE               (UVC_CID_CAMERA_CLASS_BASE + 17)
#define UVC_CID_IRIS_RELATIVE               (UVC_CID_CAMERA_CLASS_BASE + 18)
#define UVC_CID_AUTO_EXPOSURE_BIAS          (UVC_CID_CAMERA_CLASS_BASE + 19)
#define UVC_CID_AUTO_N_PRESET_WHITE_BALANCE (UVC_CID_CAMERA_CLASS_BASE + 20)
#define UVC_CID_PAN_SPEED                   (UVC_CID_CAMERA_CLASS_BASE + 32)
#define UVC_CID_TILT_SPEED                  (UVC_CID_CAMERA_CLASS_BASE + 33)

/* \brief USB 视频类控件数据的数据类型*/
#define UVC_CTRL_DATA_TYPE_RAW      0
#define UVC_CTRL_DATA_TYPE_SIGNED   1
#define UVC_CTRL_DATA_TYPE_UNSIGNED 2
#define UVC_CTRL_DATA_TYPE_BOOLEAN  3
#define UVC_CTRL_DATA_TYPE_ENUM     4
#define UVC_CTRL_DATA_TYPE_BITMASK  5
/* 控件索引掩码*/
#define UVC_CTRL_ID_MASK           (0x0fffffff)
/* \brief 寻找下一个控件标志*/
#define UVC_CTRL_FLAG_NEXT_CTRL     0x80000000

enum uvc_exposure_auto_type {
    UVC_EXPOSURE_AUTO              = 0,
    UVC_EXPOSURE_MANUAL            = 1,
    UVC_EXPOSURE_SHUTTER_PRIORITY  = 2,
    UVC_EXPOSURE_APERTURE_PRIORITY = 3
};

/* \brief USB 视频类控件类型*/
enum uvc_ctrl_type {
    UVC_CTRL_TYPE_INTEGER       = 1,
    UVC_CTRL_TYPE_BOOLEAN       = 2,
    UVC_CTRL_TYPE_MENU          = 3,
    UVC_CTRL_TYPE_BUTTON        = 4,
    UVC_CTRL_TYPE_INTEGER64     = 5,
    UVC_CTRL_TYPE_CTRL_CLASS    = 6,
    UVC_CTRL_TYPE_STRING        = 7,
    UVC_CTRL_TYPE_BITMASK       = 8,
    UVC_CTRL_TYPE_INTEGER_MENU  = 9,

    /* 命令类型是 >= 0x0100*/
    UVC_CTRL_COMPOUND_TYPES = 0x0100,
    UVC_CTRL_TYPE_U8        = 0x0100,
    UVC_CTRL_TYPE_U16       = 0x0101,
    UVC_CTRL_TYPE_U32       = 0x0102,
};

struct uvc_menu_info {
    uint32_t value;
    uint8_t  name[32];
};

/* \brief 媒体 PAD 结构体*/
struct media_pad {
    uint16_t      idx;            /* 实体 PAD 数组的 PAD 索引*/
    unsigned long flags;          /* PAD 索引 (MEDIA_PAD_FL_*) */
};

struct usbh_vc_ctrl;
/* \brief USB 视频类实体结构体*/
struct usbh_vc_entity {
    struct usb_list_node node;                     /* 当前 USB 视频类实体节点*/
    struct usb_list_node chain;                    /* 视频设备链节点*/

    uint32_t             flags;                    /* USB 视频类实体标志*/

    uint8_t              id;                       /* USB 视频类实体 ID */
    uint16_t             type;                     /* USB 视频类实体类型*/
    char                 name[64];                 /* USB 视频类实体名字*/

    uint32_t             num_pads;
    uint32_t             num_links;
    struct media_pad    *p_pads;

    union {
        /* 摄像头信息*/
        struct {
            uint16_t     objective_focal_length_min; /* L(min)的值，如果不支持光学变焦，则为0*/
            uint16_t     objective_focal_length_max; /* L(max)的值，如果不支持光学变焦，则为0*/
            uint16_t     ocular_focal_length;        /* L(ocular)的值，如果不支持光学变焦，则为0*/
            uint8_t      control_size;               /* 位图大小*/
            uint8_t     *p_controls;                 /* p_controls 是一个位图，表示视频流的某些摄像头控件的可用性*/
        } camera;
        /* 媒体*/
        struct {
            uint8_t      control_size;
            uint8_t     *p_controls;
            uint8_t      transport_mode_size;
            uint8_t     *p_transport_modes;
        } media;
        /* 输出终端*/
        struct {
        } output;
        /* 处理单元*/
        struct {
            uint16_t     max_multiplier;            /* 数字倍增(此字段指示最大数字倍增率乘以100)*/
            uint8_t      control_size;              /* 控件位图的大小*/
            uint8_t     *p_controls;                /* 控件位图地址*/
            uint8_t      video_standards;           /* 处理单元支持的所有模拟视频标准的位图*/
        } processing;
        /* 选择器单元*/
        struct {
        } selector;
        /* 扩展单元*/
        struct {
            uint8_t      guid_extension_code[16];   /* 特定供应商的代码*/
            uint8_t      num_controls;              /* 扩展单元的控件数量*/
            uint8_t      control_size;              /* 控件位图的大小*/
            uint8_t     *p_controls;                /* 控件位图的地址*/
            uint8_t     *p_controls_type;
        } extension;
    };
    uint8_t              n_in_pins;                 /* 输入引脚个数*/
    uint8_t             *p_source_id;               /* 连接点 ID*/

    uint32_t             n_controls;                /* 控件数量*/
    struct usbh_vc_ctrl *p_controls;                /* USB 视频类控件结构体*/
};

/* \brief USB 视频类控件信息结构体*/
struct usbh_vc_ctrl_info {
    struct usb_list_head mappings;   /* 控件图链表*/

    uint8_t              entity[16]; /* USB 视频类实体GUID(全局唯一标识符)*/
    uint8_t              index;      /* p_controls 的位索引 */
    uint8_t              selector;   /* 选择项*/

    uint16_t             size;       /* 大小*/
    uint32_t             flags;      /* 标志*/
};

/* \brief USB 视频类控件映射结构体*/
struct usbh_vc_ctrl_mapping {
    struct usb_list_node  node;
    struct usb_list_head  ev_subs;

    uint32_t              id;
    uint8_t               name[32];
    uint8_t               entity[16];
    uint8_t               selector;

    uint8_t               size;
    uint8_t               offset;
    enum uvc_ctrl_type    ctrl_type;
    uint32_t              data_type;

    struct uvc_menu_info *p_menu_info;
    uint32_t              menu_count;

    uint32_t              master_id;
    int                   master_manual;
    uint32_t              slave_ids[2];

    int (*p_fn_get) (struct usbh_vc_ctrl_mapping *p_mapping, uint8_t query,
                     const uint8_t *p_data);
    void (*p_fn_set) (struct usbh_vc_ctrl_mapping *p_mapping, int value,
                      uint8_t *p_data);
};

/* \brief USB 视频类控件结构体*/
struct usbh_vc_ctrl {
    struct usbh_vc_entity    *p_entity;             /* 所属的 USB 视频类实体*/
    struct usbh_vc_ctrl_info  info;                 /* 控件信息结构体*/

    uint8_t                   index;                /* 用 来匹配 usbh_vc_ctrl 入口和 usbh_vc_ctrl_info*/
    uint8_t                   dirty      :1,
                              loaded     :1,
                              modified   :1,
                              cached     :1,
                              initialized:1;        /* 初始化完成标志*/

    uint8_t                  *p_uvc_data;
};

/* \brief USB 视频类视频链结构体*/
struct usbh_vc_video_chain {
    struct usbh_vc        *p_vc;          /* 所属的 USB 视频类设备结构体*/
    struct usb_list_node   node;          /* 当前视频链节点*/

    struct usb_list_head   entities;      /* 所有实体 */
    struct usbh_vc_entity *p_processing;  /* 处理单元*/
    struct usbh_vc_entity *p_selector;    /* 选择单元*/
#if USB_OS_EN
    usb_mutex_handle_t     p_lock;        /* 保护控件信息*/
#endif
    uint32_t               caps;          /* chain-wide caps */
};

/**
 * \brief USB 视频类实体申请函数
 *
 * \param[in]  type       实体类型
 * \param[in]  id         实体 ID
 * \param[in]  n_pads     引脚点数量
 * \param[in]  extra_size 额外数据的大小
 * \param[out] p_entity   返回的实体结构体地址
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_entity_alloc(uint16_t                type,
                         uint8_t                 id,
                         uint32_t                n_pads,
                         uint32_t                extra_size,
					 	 struct usbh_vc_entity **p_entity);
/**
 * \brief USB 视频类释放实体
 *
 * \param[in] p_vc USB 视频类设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_entity_free(struct usbh_vc *p_vc);
/**
 * \brief 为视频链扫描设备和注册视频设备
 *        从输出终端开始扫描链并向后遍历
 *
 * \param[in] p_vc USB 视频类设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_video_chain_scan(struct usbh_vc *p_vc);
/**
 * \brief 为视频链扫描设备和注册视频设备
 *        从输出终端开始扫描链并向后遍历
 *
 * \param[in] p_vc USB 视频类设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_video_chain_clr(struct usbh_vc *p_vc);
/**
 * \brief USB 视频类注册视频链
 *
 * \param[in] p_vc USB 视频类设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_video_chain_register(struct usbh_vc *p_vc);
/**
 * \brief USB 视频类注销视频链
 *
 * \param[in] p_vc USB 视频类设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_video_chain_deregister(struct usbh_vc *p_vc);
/**
 * \brief 设置视频链控件值
 *
 * \param[in] p_chain 视频链
 * \param[in] id      控件ID
 * \param[in] value   控件值
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_chain_ctrl_set(struct usbh_vc_video_chain *p_chain,
                           int                         id,
                           int                         value);
/**
 * \brief 获取视频链控件值
 *
 * \param[in] p_chain 视频链
 * \param[in] id      控件 ID
 * \param[in] p_value 储存控制值的缓存
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_chain_ctrl_get(struct usbh_vc_video_chain *p_chain,
                           int                         id,
                           int                        *p_value);
/**
 * \brief USB 视频类设备控件初始化
 *
 * \param[in] p_vc USB 视频类设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_ctrl_init(struct usbh_vc *p_vc);
/**
 * \brief USB 视频类设备控件反初始化
 *
 * \param[in] p_vc USB 视频类设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_ctrl_deinit(struct usbh_vc *p_vc);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif


