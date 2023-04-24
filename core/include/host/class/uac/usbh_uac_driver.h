#ifndef __USBH_UAC_DRIVER_H_
#define __USBH_UAC_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "usb_adapter_inter.h"
#include "usb_errno.h"
#include "host/core/usbh.h"
#include "host/class/uac/uac_pcm.h"
#include "host/class/uac/uac.h"

/* 打印函数*/
#define __UAC_TRACE(info, ...) \
            do { \
                usb_printf("USBH-UAC:"info, ##__VA_ARGS__); \
            } while (0)

#define MAX_NR_RATES    1024
/* UAC设备的VID和PID*/
#define UAC_ID(vendor, product) (((vendor) << 16) | (product))
/* 获取UAC设备的VID*/
#define UAC_ID_VENDOR(id) ((id) >> 16)
/* 获取UAC设备的PID*/
#define UAC_ID_PRODUCT(id) ((uint16_t)(id))

#define combine_word(s)    ((*(s)) | ((uint32_t)(s)[1] << 8))
#define combine_triple(s)  (combine_word(s) | ((uint32_t)(s)[2] << 16))
#define combine_quad(s)    (combine_triple(s) | ((uint32_t)(s)[3] << 24))

#define UAC_CONTROL_BIT(CS) (1 << ((CS) - 1))

/* 获取设备速度*/
#define uac_get_speed(p_fun) ((p_fun)->p_udev->speed)
/* 声卡模式*/
enum {
    USBH_SND_PCM_STREAM_PLAYBACK = 0,    /* 播放模式*/
    USBH_SND_PCM_STREAM_CAPTURE,         /* 捕获模式(录音)*/
    USBH_SND_PCM_STREAM_LAST = USBH_SND_PCM_STREAM_PLAYBACK,
};

/* 时间戳模式*/
enum {
    USBH_SND_PCM_TSTAMP_NONE = 0,
    USBH_SND_PCM_TSTAMP_ENABLE,
    USBH_SND_PCM_TSTAMP_LAST = USBH_SND_PCM_TSTAMP_ENABLE,
};

typedef int uac_pcm_format_t;
#define SNDRV_PCM_FORMAT_S8         0
#define SNDRV_PCM_FORMAT_U8         1
#define SNDRV_PCM_FORMAT_S16_LE     2
#define SNDRV_PCM_FORMAT_S16_BE     3
#define SNDRV_PCM_FORMAT_U16_LE     4
#define SNDRV_PCM_FORMAT_U16_BE     5
#define SNDRV_PCM_FORMAT_S24_LE     6          /* low three bytes */
#define SNDRV_PCM_FORMAT_S24_BE     7          /* low three bytes */
#define SNDRV_PCM_FORMAT_U24_LE     8          /* low three bytes */
#define SNDRV_PCM_FORMAT_U24_BE     9          /* low three bytes */
#define SNDRV_PCM_FORMAT_S32_LE     10
#define SNDRV_PCM_FORMAT_S32_BE     11
#define SNDRV_PCM_FORMAT_U32_LE     12
#define SNDRV_PCM_FORMAT_U32_BE     13
#define SNDRV_PCM_FORMAT_FLOAT_LE   14
#define SNDRV_PCM_FORMAT_FLOAT_BE   15          /* 4-byte float, IEEE-754 32-bit, range -1.0 to 1.0 */
#define SNDRV_PCM_FORMAT_FLOAT64_LE 16          /* 8-byte float, IEEE-754 64-bit, range -1.0 to 1.0 */
#define SNDRV_PCM_FORMAT_FLOAT64_BE 17          /* 8-byte float, IEEE-754 64-bit, range -1.0 to 1.0 */
#define SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE 18  /* IEC-958 subframe, Little Endian */
#define SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE 19  /* IEC-958 subframe, Big Endian */
#define SNDRV_PCM_FORMAT_MU_LAW     20
#define SNDRV_PCM_FORMAT_A_LAW      21
#define SNDRV_PCM_FORMAT_IMA_ADPCM  22
#define SNDRV_PCM_FORMAT_MPEG       23
#define SNDRV_PCM_FORMAT_GSM        24
#define SNDRV_PCM_FORMAT_SPECIAL    31
#define SNDRV_PCM_FORMAT_S24_3LE    32          /* in three bytes */
#define SNDRV_PCM_FORMAT_S24_3BE    33          /* in three bytes */
#define SNDRV_PCM_FORMAT_U24_3LE    34          /* in three bytes */
#define SNDRV_PCM_FORMAT_U24_3BE    35          /* in three bytes */
#define SNDRV_PCM_FORMAT_S20_3LE    36          /* in three bytes */
#define SNDRV_PCM_FORMAT_S20_3BE    37          /* in three bytes */
#define SNDRV_PCM_FORMAT_U20_3LE    38          /* in three bytes */
#define SNDRV_PCM_FORMAT_U20_3BE    39          /* in three bytes */
#define SNDRV_PCM_FORMAT_S18_3LE    40          /* in three bytes */
#define SNDRV_PCM_FORMAT_S18_3BE    41          /* in three bytes */
#define SNDRV_PCM_FORMAT_U18_3LE    42          /* in three bytes */
#define SNDRV_PCM_FORMAT_U18_3BE    43          /* in three bytes */
#define SNDRV_PCM_FORMAT_G723_24    44          /* 8 samples in 3 bytes */
#define SNDRV_PCM_FORMAT_G723_24_1B 45          /* 1 sample in 1 byte */
#define SNDRV_PCM_FORMAT_G723_40    46          /* 8 Samples in 5 bytes */
#define SNDRV_PCM_FORMAT_G723_40_1B 47          /* 1 sample in 1 byte */
#define SNDRV_PCM_FORMAT_DSD_U8     48          /* DSD, 1-byte samples DSD (x8) */
#define SNDRV_PCM_FORMAT_DSD_U16_LE 49          /* DSD, 2-byte samples DSD (x16), little endian */
#define SNDRV_PCM_FORMAT_DSD_U32_LE 50          /* DSD, 4-byte samples DSD (x32), little endian */
#define SNDRV_PCM_FORMAT_DSD_U16_BE 51          /* DSD, 2-byte samples DSD (x16), big endian */
#define SNDRV_PCM_FORMAT_DSD_U32_BE 52          /* DSD, 4-byte samples DSD (x32), big endian */
#define SNDRV_PCM_FORMAT_LAST       SNDRV_PCM_FORMAT_DSD_U32_BE

/* 音频格式结构体*/
struct audioformat {
    struct usb_list_head       list;                  /* 当前格式节点*/
    uint32_t                   formats;               /* 格式位数*/
    uint32_t                   channels;              /* 通道数*/
    uint32_t                   fmt_type;              /* USB音频格式类型(1-3)*/
    uint32_t                   frame_size;            /* 每一帧的样本大小 */
    int                        iface;                 /* 接口号*/
    uint8_t                    altsetting;            /* 备用设置编号*/
    uint8_t                    attributes;            /* 音频端点属性*/
    uint8_t                    endpoint;              /* 端点号*/
    uint8_t                    ep_attr;               /* 端点属性*/
    uint8_t                    datainterval;          /* 数据包间隔 */
    uint8_t                    protocol;              /* UAC版本，1或2*/
    uint32_t                   maxpacksize;           /*最大包大小*/
    uint32_t                   rates;                 /* 采样率掩码位*/
    uint32_t                   rate_min, rate_max;    /* 最小/最大采样率*/
    uint32_t                   nr_rates;              /* 采样率表入口数量*/
    uint32_t                  *rate_table;            /* 采样率表*/
    uint8_t                    clock;                 /* 相关的时钟*/
    usb_bool_t                 dsd_dop;               /* add DOP headers in case of DSD samples */
    usb_bool_t                 dsd_bitrev;            /* reverse the bits of each DSD sample */
};

/* USB音频子流结构体*/
struct usbh_uac_substream {
    struct usbh_uac_stream           *stream;         /* 所属的UAC设备数据流*/
    struct usbh_function             *p_fun;          /* USB接口功能结构体*/
    struct uac_pcm_substream         *pcm_substream;  /* PCM子流*/
    int                               direction;      /* PCM方向：回放或捕获*/
    int                               interface;      /* 当前接口号*/
    int                               endpoint;       /* 端点号*/
    struct audioformat               *cur_audiofmt;   /* 当前的音频格式指针*/
    uac_pcm_format_t                  pcm_format_size;/* 当前音频格式深度*/
    uint32_t                          channels;       /* 当前的通道数*/
    uint32_t                          channels_max;   /* 所有音频格式的最大通道*/
    uint32_t                          cur_rate;       /* 当前采样率*/
    uint32_t                          period_bytes;   /* 当前周期字节*/
    uint32_t                          period_frames;  /* 当前每个周期的帧数 */
    uint32_t                          buffer_periods; /* 当前每个缓存的周期数*/
    uint32_t                          altsetting;     /* USB数据格式: 备用设置号*/
    uint32_t                          txfr_quirk:1;   /* allow sub-frame alignment */
    uint32_t                          fmt_type;       /* USB 音频格式类型 (1-3) */
    uint32_t                          pkt_offset_adj; /* 要从数据包开头删除的字节数(对于不合规的设备)*/
    uint32_t                          maxframesize;   /* 最大帧数*/
    uint32_t                          curframesize;   /* 当前帧数*/

    usb_bool_t                        running;        /* 运行状态 */

    uint32_t                          hwptr_done;     /* 缓存中处理字节的位置*/
    uint32_t                          transfer_done;  /* 自上次周期更新后处理的帧数*/
    uint32_t                          frame_limit;    /* USB传输请求包的帧极限数量*/

    /* 这个数据流的数据和同步端点*/
    uint32_t                          ep_num;         /* 端点号*/
    struct usbh_uac_endpoint         *data_endpoint;
    struct usbh_uac_endpoint         *sync_endpoint;
    uint32_t                          flags;
    usb_bool_t                        need_setup_ep;  /* 是否配置好端点准备收发数据*/
    uint32_t                          speed;          /* USB设备速度*/

    uint32_t                          formats;        /* 格式位数*/
    uint32_t                          num_formats;    /* 支持的音频的格式的数量(链表)*/
    struct usb_list_head              fmt_list;       /* 格式链表*/
    struct uac_pcm_hw_constraint_list rate_list;      /* 极限采样率*/

    int last_frame_number;                            /* 最新的周期帧编号*/
    int last_delay;                                   /* 储存延时的音帧数量*/

    struct {
        int marker;
        int channel;
        int byte_idx;
    } dsd_dop;
    /* 是否更新启动时间*/
    usb_bool_t                         trigger_tstamp_pending_update;
};

struct usbh_audio;

/* USB音频设备数据流结构体*/
struct usbh_uac_stream {
    struct usbh_audio        *chip;        /* USB音频设备*/
    struct uac_pcm           *pcm;         /* PCM实体*/
    int                       pcm_index;   /* PCM索引*/
    uint32_t                  fmt_type;    /* USB音频格式类型(1-3)*/
    struct usbh_uac_substream substream[2];/* USB音频设备数据子流*/
    struct usb_list_head      list;        /* 数据流链表节点*/
};

/* USB音频结构体*/
struct usbh_audio{
    int                    index;          /* 音频设备索引*/
    struct usbh_function  *p_fun;          /* USB接口功能结构体*/
    uint32_t               usb_id;         /* USB设备的VID和PID*/
    char                   name[80];       /* 音频设备名字*/
    char                   mixername[80];  /* 混音名字*/
    struct usbh_interface *pm_intf;        /* 电源管理接口*/
    struct usbh_interface *ctrl_intf;      /* 控制接口*/
    usb_bool_t             shutdown;       /* 是否停止*/
    usb_bool_t             txfr_quirk;     /* 是否有传输兼容*/
    int                    pcm_devs;       /* PCM设备号*/
    usb_mutex_handle_t     stream_mutex;   /* 数据流互斥锁*/
    struct usb_list_head   stream_list;    /* 数据流链表*/
    usb_mutex_handle_t     ep_mutex;       /* 端点互斥锁*/
    struct usb_list_head   ep_list;        /* 音频相关端点的链表*/
    struct usb_list_head   mixer_list;     /* 混音接口链表*/
    int                    num_ctrl_intf;  /* 控制接口数量*/
};


/** 合并字节并得到一个整数值*/
uint32_t usbh_uac_combine_bytes(uint8_t *bytes, int size);
/** UAC驱动相关初始化*/
usb_err_t usbh_uac_lib_init(void);
/**
 * 创建一个UAC设备
 *
 * @param p_fun USB接口功能
 *
 * @return 成功返回USB_OK
 */
usb_err_t usbh_uac_dev_create(struct usbh_function *p_fun);
/**
 * 销毁一个UAC设备
 *
 * @param p_fun USB接口功能
 *
 * @return 成功返回USB_OK
 */
usb_err_t usbh_uac_dev_destory(struct usbh_function *p_fun);
/** 释放USB流实体*/
void usbh_uac_audio_stream_free(struct usbh_uac_stream *stream);
/**
 * 分析描述符缓存，返回给定的描述符类型的起始指针
 *
 * @param descstart 描述符起始地址
 * @param desclen   描述符长度
 * @param after
 * @param dtype     要寻找的描述符类型
 */
void *usbh_uac_find_desc(void *descstart, int desclen, void *after, uint8_t dtype);
/** 通过子类找到类特定的接口描述符*/
void *usbh_uac_find_csint_desc(void *buffer, int buflen, void *after, uint8_t dsubtype);
/** 获取端点的轮询间隔*/
uint8_t usbh_uac_parse_datainterval(struct usbh_audio     *chip,
                                    struct usbh_interface *p_intf);
/**
 * 寻找音频设备
 *
 * @param audio_index 音频设备索引
 *
 * @return 成功返回USB音频结构体
 */
struct usbh_audio *usbh_uac_dev_find(int audio_index);
/**
 * 分析USB音频设备数据接口
 *
 * @param p_audio  USB音频设备
 * @param iface_no 数据接口编号
 *
 * @return 成功返回USB_OK
 */
usb_err_t usbh_uac_parse_audio_interface(struct usbh_audio *p_audio, int iface_no);
/** 获取格式物理位宽*/
usb_err_t uac_pcm_format_physical_width(uac_pcm_format_t format);
/**
 * UAC设置控制传输
 *
 * @param p_fun       相关的功能接口
 * @param request     具体的USB请求
 * @param requesttype 传输方向|请求类型|请求目标
 * @param value       参数
 * @param index       索引
 * @param data        数据(可以为空)
 * @param size        数据长度
 *
 * @return 如果有数据传输，成功返回成功传输的字节数
 *         如果没有数据传输，成功返回USB_OK
 */
usb_err_t usbh_uac_ctl_msg(struct usbh_function *p_fun, uint8_t request,
                           uint8_t requesttype, uint16_t value, uint16_t index, void *data,
                           uint16_t size);
/** 初始化音量控制*/
usb_err_t usbh_uac_init_pitch(struct usbh_audio *chip, int iface,
		                      struct usbh_interface *alts,
		                      struct audioformat *fmt);

/** 检查PCM子数据流是否在运行中*/
static inline int uac_pcm_running(struct uac_pcm_substream *substream)
{
    return ((substream->runtime->status.state == UAC_PCM_STATE_RUNNING) ||
        ((substream->runtime->status.state == UAC_PCM_STATE_DRAINING) &&
         (substream->stream == USBH_SND_PCM_STREAM_PLAYBACK)));
}
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif
