/**
 * PCM(Pulse-code modulation)脉冲编码调制
 * 声音是模拟信号，PCM就是把声音从模拟转换成数字信号
 * 它的原理是利用一个固定的频率对模拟信号进行采样，采样后的信号在波形上看就像
 * 一串连续的幅值不一的的脉冲，把这些脉冲的幅值进行一定精度的量化，这些量化的
 * 值被连续的输出，传输，处理或记录到存储介质，所有这些组成了数字音频。
 */
#ifndef __UAC_PCM_H_
#define __UAC_PCM_H_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "host/class/uac/uac_hw_rule.h"
#include "usb_adapter_inter.h"
#include "usb_errno.h"
#include "list/usb_list.h"

/* 打印函数*/
#define __UAC_PCM_TRACE(info, ...) \
            do { \
                usb_printf("USBH-UAC-PCM:"info, ##__VA_ARGS__); \
            } while (0)

#define _SNDRV_PCM_FMTBIT(fmt)       (1 << (int)fmt)
#define SNDRV_PCM_FMTBIT_S8         _SNDRV_PCM_FMTBIT(SNDRV_PCM_FORMAT_S8)
#define SNDRV_PCM_FMTBIT_U8         _SNDRV_PCM_FMTBIT(SNDRV_PCM_FORMAT_U8)
#define SNDRV_PCM_FMTBIT_S16_LE     _SNDRV_PCM_FMTBIT(SNDRV_PCM_FORMAT_S16_LE)
#define SNDRV_PCM_FMTBIT_S16_BE     _SNDRV_PCM_FMTBIT(SNDRV_PCM_FORMAT_S16_BE)
#define SNDRV_PCM_FMTBIT_S32_LE     _SNDRV_PCM_FMTBIT(SNDRV_PCM_FORMAT_S32_LE)
#define SNDRV_PCM_FMTBIT_FLOAT_LE   _SNDRV_PCM_FMTBIT(SNDRV_PCM_FORMAT_FLOAT_LE)
#define SNDRV_PCM_FMTBIT_MU_LAW     _SNDRV_PCM_FMTBIT(SNDRV_PCM_FORMAT_MU_LAW)
#define SNDRV_PCM_FMTBIT_A_LAW      _SNDRV_PCM_FMTBIT(SNDRV_PCM_FORMAT_A_LAW)
#define SNDRV_PCM_FMTBIT_MPEG       _SNDRV_PCM_FMTBIT(SNDRV_PCM_FORMAT_MPEG)
#define SNDRV_PCM_FMTBIT_SPECIAL    _SNDRV_PCM_FMTBIT(SNDRV_PCM_FORMAT_SPECIAL)

/* USB音频PCM状态*/
typedef int uac_pcm_state_t;
#define UAC_PCM_STATE_OPEN          0  /* 数据流打开状态*/
#define UAC_PCM_STATE_SETUP         1  /* 数据流初始化状态*/
#define UAC_PCM_STATE_PREPARED      2  /* 数据流可以启动状态*/
#define UAC_PCM_STATE_RUNNING       3  /* 数据流正在运行扎un给他*/
#define UAC_PCM_STATE_XRUN          4  /* stream reached an xrun */
#define UAC_PCM_STATE_DRAINING      5  /* stream is draining */
#define UAC_PCM_STATE_PAUSED        6  /* 数据流停止状态*/
#define UAC_PCM_STATE_SUSPENDED     7  /* 硬件挂起*/
#define UAC_PCM_STATE_DISCONNECTED  8  /* 硬件未连接*/
#define UAC_PCM_STATE_LAST          SNDRV_PCM_STATE_DISCONNECTED

/* USB音频触发选项*/
#define UAC_PCM_TRIGGER_STOP          0
#define UAC_PCM_TRIGGER_START         1
#define UAC_PCM_TRIGGER_PAUSE_PUSH    3
#define UAC_PCM_TRIGGER_PAUSE_RELEASE 4
#define UAC_PCM_TRIGGER_SUSPEND       5
#define UAC_PCM_TRIGGER_RESUME        6
#define UAC_PCM_TRIGGER_DRAIN         7

/**
 * USB音频采样率
 *
 * 音频采样率是指设备在一秒中内对声音信号的采样次数
 *
 * 采集一帧音频数据所需要的时间是1000(ms)/采样率毫秒
 */
#define SNDRV_PCM_RATE_5512         (1<<0)      /* 5512Hz */
#define SNDRV_PCM_RATE_8000         (1<<1)      /* 8000Hz */
#define SNDRV_PCM_RATE_11025        (1<<2)      /* 11025Hz */
#define SNDRV_PCM_RATE_16000        (1<<3)      /* 16000Hz */
#define SNDRV_PCM_RATE_22050        (1<<4)      /* 22050Hz */
#define SNDRV_PCM_RATE_32000        (1<<5)      /* 32000Hz */
#define SNDRV_PCM_RATE_44100        (1<<6)      /* 44100Hz */
#define SNDRV_PCM_RATE_48000        (1<<7)      /* 48000Hz */
#define SNDRV_PCM_RATE_64000        (1<<8)      /* 64000Hz */
#define SNDRV_PCM_RATE_88200        (1<<9)      /* 88200Hz */
#define SNDRV_PCM_RATE_96000        (1<<10)     /* 96000Hz */
#define SNDRV_PCM_RATE_176400       (1<<11)     /* 176400Hz */
#define SNDRV_PCM_RATE_192000       (1<<12)     /* 192000Hz */

#define SNDRV_PCM_RATE_CONTINUOUS   (1<<30)     /* 连续的采样率*/
#define SNDRV_PCM_RATE_KNOT         (1<<31)     /* supports more non-continuos rates */

#define SNDRV_PCM_HW_PARAM_SAMPLE_BITS       8  /* Bits per sample */
#define SNDRV_PCM_HW_PARAM_TICK_TIME        19  /* Approx tick duration in us */
#define SNDRV_PCM_HW_PARAM_FIRST_INTERVAL   SNDRV_PCM_HW_PARAM_SAMPLE_BITS
#define SNDRV_PCM_HW_PARAM_LAST_INTERVAL    SNDRV_PCM_HW_PARAM_TICK_TIME

#define SNDRV_PCM_INFO_HALF_DUPLEX  0x00100000  /* only half duplex */

#define UAC_PCM_RUNTIME_CHECK(sub)        ((sub == NULL) || ((sub)->runtime == NULL))
#define uac_pcm_substream_chip(substream) ((substream)->private_data)

#define SND_USB_ENDPOINT_TYPE_DATA     0
#define SND_USB_ENDPOINT_TYPE_SYNC     1

/* PCM格式数据结构体*/
struct pcm_format_data {
    uint8_t width;      /* 位宽*/
    uint8_t phys;       /* 物理位宽*/
    char    le;         /* 0 = 大端, 1 = 小端, -1 = 其他*/
    char    signd;      /* 0 = 无符号, 1 = 有符号, -1 = 其他 */
    uint8_t silence[8]; /* 要填充的静音数据 */
};

struct uac_pcm;
struct uac_pcm_group {
    usb_mutex_handle_t   mutex;
    struct usb_list_head substreams;
    int count;
};

/* PCM硬件信息结构*/
struct uac_pcm_hardware {
    uint32_t formats;           /* 格式位数*/
    uint32_t rates;             /* 采样率*/
    uint32_t rate_min;          /* 最小采样率*/
    uint32_t rate_max;          /* 最大采样率*/
    uint32_t channels_min;      /* 最小通道数*/
    uint32_t channels_max;      /* 最大通道数*/
    size_t   buffer_bytes_max;  /* 最大缓存大小(字节)*/
    size_t   period_bytes_min;  /* 最小周期字节数*/
    size_t   period_bytes_max;  /* 最大周期字节数*/
    uint32_t periods_min;       /* 最小周期*/
    uint32_t periods_max;       /* 最大周期*/
    size_t   fifo_size;         /* 缓存大小 */
};

/* USB音频PCM状态结构体*/
struct uac_pcm_status {
    uac_pcm_state_t     state;           /* PCM状态*/
    //int pad1;                          /* Needed for 64 bit alignment */
    int                 hw_ptr;          /* RO: hw ptr (0...boundary-1) */
    struct usb_timespec tstamp;          /* 时间戳*/
    uac_pcm_state_t     suspended_state; /* 数据流挂起状态*/
    struct usb_timespec audio_tstamp;    /* from sample counter or wall clock */
};

/* USB音频PCM控制结构体*/
struct uac_pcm_control {
    uint32_t appl_ptr;    /* 用户缓存指针*/
    uint32_t avail_min;   /* 用于唤醒的最小有效帧数*/
};

/* PCM运行时间结构体*/
struct uac_pcm_runtime {
    /* 硬件描述*/
    struct uac_pcm_hardware       hw;                /* PCM硬件描述*/
    struct uac_pcm_hw_constraints hw_constraints;    /* PCM硬件参数容器*/
    /* 音频格式描述*/
    uint32_t                      channels;          /* 通道*/
    int                           format_size;       /* 格式位大小*/
    uint32_t                      rate;              /* 采样率*/
    uint32_t                      periods;           /* 周期数*/
    uint32_t                      frame_bits;        /* 帧位数*/
    uint32_t                      sample_bits;       /* 样本位数*/
    uint32_t                      period_size;       /* 周期大小(以帧为单位)*/
    uint32_t                      buffer_frame_size; /* 缓存大小(以帧为单位)*/
    uint8_t                      *buffer;            /* 缓存*/
    uint32_t                      buffer_size;       /* 缓存大小(以字节为单位)*/
    /* 传输机制相关成员*/
    uint32_t                      min_align;         /* 格式最小对齐*/
    uint32_t                      byte_align;        /* 字节对齐*/
    uint32_t                      period_step;
    uint32_t                      start_threshold;   /* 启动点(以帧为单位)*/
    uint32_t                      stop_threshold;    /* 停止点(以帧为单位)*/
    uint32_t                      boundary;          /* 边界，在最大限度内，buffer_frame_size的最大整数倍的值*/
    struct uac_pcm_status         status;            /* 状态结构体*/
    struct uac_pcm_control        control;           /* 控制结构体*/
    uint32_t                      hw_ptr_base;       /* 缓存重新启动的位置*/
    uint32_t                      hw_ptr_interrupt;  /* 中断时间的位置*/
    uint32_t                      hw_ptr_wrap;
    uint32_t                      twake;             /* 唤醒帧数*/
    uint32_t                      avail_max;         /* 最大可用帧数*/
    /* 时间戳相关*/
    int                           tstamp_mode;       /* 时间戳模式*/
    struct usb_timespec           trigger_tstamp;    /* 触发时间戳*/
    struct usb_timespec           driver_tstamp;     /* 驱动时间戳*/
    int                           delay;             /* 延迟的帧数*/
    /* 静音相关变量*/
    uint32_t                      silence_start;     /* 静音数据开始的帧指针*/
    uint32_t                      silence_filled;    /* 已经填充静音数据的大小*/
    uint32_t                      silence_size;      /* 需要填充静音数据大小(以帧为单位)*/
    uint32_t                      silence_threshold; /* 静音点(以帧为单位)*/

    void                         *private_data;      /* 私有数据*/
};

struct uac_pcm_hw_constraint_list {
    uint32_t  count;
    uint32_t *list;
    uint32_t  mask;
};

struct uac_interval {
    uint32_t min, max;
    uint32_t openmin:1,
             openmax:1,
             integer:1,
             empty:1;
};

struct uac_pcm_substream;
/* PCM操作函数集*/
struct uac_pcm_ops {
    usb_err_t (*open)(struct uac_pcm_substream *substream);
    usb_err_t (*close)(struct uac_pcm_substream *substream);
    usb_err_t (*prepare)(struct uac_pcm_substream *substream);
    usb_err_t (*hw_params)(struct uac_pcm_substream *substream,
                          struct uac_pcm_hw_params *hw_params);
    usb_err_t (*hw_free)(struct uac_pcm_substream *substream);
    usb_err_t (*trigger)(struct uac_pcm_substream *substream, int cmd);
    int       (*pointer)(struct uac_pcm_substream *substream);
    usb_err_t (*fifo_size)(struct uac_pcm_substream * substream, void *arg);
    int       (*silence)(struct uac_pcm_substream *substream, int channel,
                         uint32_t pos, uint32_t count);
};

/* PCM子流结构体*/
struct uac_pcm_substream {
    struct uac_pcm           *pcm;                 /* PCM实体*/
    struct uac_pcm_str       *pstr;                /* 所属的流结构体*/
    void                     *private_data;        /* 私有数据*/
    usb_mutex_handle_t        mutex;               /* 互斥锁*/
    int                       number;              /* 子流编号*/
    char                      name[32];            /* 子流名字*/
    int                       stream;              /* 流(方向) */
    size_t                    buffer_bytes_max;    /* 最大环形缓冲期大小*/
    const struct uac_pcm_ops *ops;                 /* PCM操作函数集*/
    struct uac_pcm_runtime   *runtime;             /* 运行时间*/
    struct uac_pcm_substream *next;                /* 下一个子流 */

    int                       refcnt;              /* 引用计数*/
    uint32_t                  hw_opened: 1;        /* 是否是打开状态*/
    uint32_t                  hw_param_set: 1;     /* 是否设置了硬件参数*/
};

/* PCM流结构体*/
struct uac_pcm_str {
    int                       stream;            /* 流(方向) */
    struct uac_pcm           *pcm;               /* PCM实体*/
    uint32_t                  substream_count;   /* 子流数量*/
    uint32_t                  substream_opened;  /* 子流打开的数量*/
    struct uac_pcm_substream *substream;         /* 子流指针*/
};

/* PCM实体结构体*/
struct uac_pcm {
    //struct usb_list_head list;          /* PCM节点*/
    int                  device;        /* 设备号*/
    uint32_t             info_flags;
    char                 id[64];        /* PCM ID*/
    char                 name[80];      /* PCM 名字*/
    struct uac_pcm_str   streams[2];    /* PCM数据流*/
    //usb_mutex_handle_t   open_mutex;
    void                *private_data;
    void               (*private_free) (struct uac_pcm *pcm);
};


/** 释放音频PCM*/
void usbh_uac_pcm_free(struct uac_pcm *pcm);
/** 创建一个新的PCM*/
usb_err_t uac_pcm_new(const char *id, int device, int playback_count,
                      int capture_count, struct uac_pcm **rpcm);
/** 设置PCM操作函数集*/
void usbh_uac_set_pcm_ops(struct uac_pcm *pcm, int stream);










/** 获取PCM格式宽度*/
int uac_get_pcm_format_size_bit(uint32_t format_size);
/** 复位PCM*/
usb_err_t uac_pcm_reset(struct uac_pcm_substream *substream, int state, usb_bool_t check_state);
/** 停止PCM*/
usb_err_t uac_pcm_stop(struct uac_pcm_substream *substream, uac_pcm_state_t state);
/**
 * 暂停/恢复数据流
 *
 * @param substream USB音频PCM子数据流
 * @param push      1：暂停；0：恢复
 *
 * @return 成功返回USB_OK
 */
usb_err_t uac_pcm_pause(struct uac_pcm_substream *substream, int push);
usb_err_t uac_pcm_drop(struct uac_pcm_substream *substream);
usb_err_t uac_pcm_unlink(struct uac_pcm_substream *substream);
/**
 * 创建一个新的PCM数据流
 *
 * @param pcm             PCM实体
 * @param stream          数据流方向
 * @param substream_count 子数据流的数量
 *
 * @return 成功返回USB_OK
 */
usb_err_t uac_pcm_new_stream(struct uac_pcm *pcm,
                             int stream, int substream_count);
/** 设置PCM操作函数集*/
void usbh_uac_set_pcm_ops(struct uac_pcm *pcm, int stream);
/** 打开一个PCM的子流*/
usb_err_t uac_pcm_open_substream(struct uac_pcm *pcm, int stream,
                                 struct uac_pcm_substream **rsubstream);
/** 断开子流连接*/
void uac_pcm_detach_substream(struct uac_pcm_substream *substream);
/**
 * USB音频PCM写函数
 *
 * @param substream   PCM子数据流结构体
 * @param buffer      缓存
 * @param buffer_size 缓存的帧大小
 *
 * @param 成功返回成功传输的帧数
 */
int uac_pcm_write(struct uac_pcm_substream *substream,
                  uint8_t *buffer, uint32_t buffer_frame_size);



/**
 * 采样率规则函数
 *
 * @param params PCM硬件参数
 * @param rule   规则结构体
 *
 * @return 返回1表示值改变，返回0表示值没改变
 */
int hw_rule_rate(struct uac_pcm_hw_params *params,
                 struct uac_pcm_hw_rule *rule);
/**
 * 通道数规则函数
 *
 * @param params PCM硬件参数
 * @param rule   规则结构体
 *
 * @return 返回1表示值改变，返回0表示值没改变
 */
int hw_rule_channels(struct uac_pcm_hw_params *params,
                     struct uac_pcm_hw_rule *rule);
/**
 * 音频格式规则函数
 *
 * @param params PCM硬件参数
 * @param rule   规则结构体
 *
 * @return 返回1表示值改变，返回0表示值没改变
 */
int hw_rule_format(struct uac_pcm_hw_params *params,
                   struct uac_pcm_hw_rule *rule);
/**
 * 周期时间规则函数
 *
 * @param params PCM硬件参数
 * @param rule   规则结构体
 *
 * @return 返回1表示值改变，返回0表示值没改变
 */
int hw_rule_period_time(struct uac_pcm_hw_params *params,
                        struct uac_pcm_hw_rule *rule);
/**
 * 设置PCM硬件容器最小最大值
 *
 * @param runtime USB音频运行时间结构体
 * @param var     具体硬件参数
 * @param min     最小值
 * @param max     最大值
 *
 * @return 返回1表示值改变，返回0表示值没改变
 */
int uac_pcm_hw_constraint_minmax(struct uac_pcm_runtime *runtime, uac_pcm_hw_param_t var,
                                 uint32_t min, uint32_t max);
/**
 * 重新定义PCM硬件参数
 *
 * @param substream PCM子数据流结构体
 * @param params    硬件参数
 *
 * @return 成功返回USB_OK
 */
usb_err_t uac_pcm_hw_refine(struct uac_pcm_substream *substream,
                            struct uac_pcm_hw_params *params);
/** 选择一个PCM硬件参数结构体定义的配置*/
usb_err_t uac_pcm_hw_params_choose(struct uac_pcm_substream *pcm,
                                   struct uac_pcm_hw_params *params);
/**
 * 添加一个USB音频PCM硬件啊限制规则
 *
 * @param runtime PCM运行时间结构体
 * @param cond    条件位
 * @param var     具体的硬件参数
 * @param func    规则函数
 * @param private 传递给规则函数的参数
 * @param dep     独立变量
 *
 * @return 成功返回USB_OK
 */
usb_err_t uac_pcm_hw_rule_add(struct uac_pcm_runtime *runtime, uint32_t cond,
                        int var,
                        uac_pcm_hw_rule_func_t func, void *private,
                        int dep1, int dep2, int dep3);
/**
 * 初始化USB音频PCM硬件限制
 *
 * @param substream PCM子数据流结构体
 *
 * @return 成功返回USB_OK
 */
usb_err_t uac_pcm_hw_constraints_init(struct uac_pcm_substream *substream);
/** 设置PCM硬件限制条件*/
usb_err_t uac_pcm_hw_constraints_complete(struct uac_pcm_substream *substream);
/** 设置usb音频硬件参数*/
struct uac_pcm_hw_params *usbh_uac_get_hw_params(void *params_usr);


static inline uint32_t pcm_format_to_bits(int pcm_format)
{
    return 1ULL << (int) pcm_format;
}

/** 字节转换为帧*/
static inline uint32_t bytes_to_frames(struct uac_pcm_runtime *runtime, uint32_t size)
{
    return size * 8 / runtime->frame_bits;
}

/** 帧转换为字节*/
static inline int frames_to_bytes(struct uac_pcm_runtime *runtime, int size)
{
    return size * runtime->frame_bits / 8;
}

/** 采样率转换为字节*/
static inline int samples_to_bytes(struct uac_pcm_runtime *runtime, int size)
{
    return size * runtime->sample_bits / 8;
}

/** 检查是否字节对齐*/
static inline int frame_aligned(struct uac_pcm_runtime *runtime, int bytes)
{
    return bytes % runtime->byte_align == 0;
}

/** 获取可用(可写)的回放缓存空间*/
static inline uint32_t uac_pcm_playback_avail(struct uac_pcm_runtime *runtime)
{
    int avail = runtime->status.hw_ptr + runtime->buffer_frame_size - runtime->control.appl_ptr;
    if (avail < 0){
        avail += runtime->boundary;
    }
    else if ((uint32_t) avail >= runtime->boundary){
        avail -= runtime->boundary;
    }
    return avail;
}

/** 获取可用(可读)的回放缓存空间*/
static inline uint32_t uac_pcm_capture_avail(struct uac_pcm_runtime *runtime)
{
    int avail = runtime->status.hw_ptr - runtime->control.appl_ptr;
    if (avail < 0){
        avail += runtime->boundary;
    }
    return avail;
}

/** 获取运行缓存指针到用户指针的帧数*/
static inline int uac_pcm_playback_hw_avail(struct uac_pcm_runtime *runtime)
{
    return runtime->buffer_frame_size - uac_pcm_playback_avail(runtime);
}


/**
 * 检查是否有数据存在在回放缓存中
 *
 * @return 返回0表示没有数据在缓存中
 */
static inline int uac_pcm_playback_data(struct uac_pcm_substream *substream)
{
    struct uac_pcm_runtime *runtime = substream->runtime;

    if (runtime->stop_threshold >= runtime->boundary){
        return 1;
    }
    return uac_pcm_playback_avail(runtime) < runtime->buffer_frame_size;
}

/** 获取时间*/
static inline void uac_pcm_gettime(struct uac_pcm_runtime *runtime,
                                   struct usb_timespec *tv)
{
    usb_get_timespec(tv);
}

#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif
