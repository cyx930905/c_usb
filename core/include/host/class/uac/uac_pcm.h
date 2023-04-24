/**
 * PCM(Pulse-code modulation)����������
 * ������ģ���źţ�PCM���ǰ�������ģ��ת���������ź�
 * ����ԭ��������һ���̶���Ƶ�ʶ�ģ���źŽ��в�������������ź��ڲ����Ͽ�����
 * һ�������ķ�ֵ��һ�ĵ����壬����Щ����ķ�ֵ����һ�����ȵ���������Щ������
 * ֵ����������������䣬������¼���洢���ʣ�������Щ�����������Ƶ��
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

/* ��ӡ����*/
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

/* USB��ƵPCM״̬*/
typedef int uac_pcm_state_t;
#define UAC_PCM_STATE_OPEN          0  /* ��������״̬*/
#define UAC_PCM_STATE_SETUP         1  /* ��������ʼ��״̬*/
#define UAC_PCM_STATE_PREPARED      2  /* ��������������״̬*/
#define UAC_PCM_STATE_RUNNING       3  /* ����������������un����*/
#define UAC_PCM_STATE_XRUN          4  /* stream reached an xrun */
#define UAC_PCM_STATE_DRAINING      5  /* stream is draining */
#define UAC_PCM_STATE_PAUSED        6  /* ������ֹͣ״̬*/
#define UAC_PCM_STATE_SUSPENDED     7  /* Ӳ������*/
#define UAC_PCM_STATE_DISCONNECTED  8  /* Ӳ��δ����*/
#define UAC_PCM_STATE_LAST          SNDRV_PCM_STATE_DISCONNECTED

/* USB��Ƶ����ѡ��*/
#define UAC_PCM_TRIGGER_STOP          0
#define UAC_PCM_TRIGGER_START         1
#define UAC_PCM_TRIGGER_PAUSE_PUSH    3
#define UAC_PCM_TRIGGER_PAUSE_RELEASE 4
#define UAC_PCM_TRIGGER_SUSPEND       5
#define UAC_PCM_TRIGGER_RESUME        6
#define UAC_PCM_TRIGGER_DRAIN         7

/**
 * USB��Ƶ������
 *
 * ��Ƶ��������ָ�豸��һ�����ڶ������źŵĲ�������
 *
 * �ɼ�һ֡��Ƶ��������Ҫ��ʱ����1000(ms)/�����ʺ���
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

#define SNDRV_PCM_RATE_CONTINUOUS   (1<<30)     /* �����Ĳ�����*/
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

/* PCM��ʽ���ݽṹ��*/
struct pcm_format_data {
    uint8_t width;      /* λ��*/
    uint8_t phys;       /* ����λ��*/
    char    le;         /* 0 = ���, 1 = С��, -1 = ����*/
    char    signd;      /* 0 = �޷���, 1 = �з���, -1 = ���� */
    uint8_t silence[8]; /* Ҫ���ľ������� */
};

struct uac_pcm;
struct uac_pcm_group {
    usb_mutex_handle_t   mutex;
    struct usb_list_head substreams;
    int count;
};

/* PCMӲ����Ϣ�ṹ*/
struct uac_pcm_hardware {
    uint32_t formats;           /* ��ʽλ��*/
    uint32_t rates;             /* ������*/
    uint32_t rate_min;          /* ��С������*/
    uint32_t rate_max;          /* ��������*/
    uint32_t channels_min;      /* ��Сͨ����*/
    uint32_t channels_max;      /* ���ͨ����*/
    size_t   buffer_bytes_max;  /* ��󻺴��С(�ֽ�)*/
    size_t   period_bytes_min;  /* ��С�����ֽ���*/
    size_t   period_bytes_max;  /* ��������ֽ���*/
    uint32_t periods_min;       /* ��С����*/
    uint32_t periods_max;       /* �������*/
    size_t   fifo_size;         /* �����С */
};

/* USB��ƵPCM״̬�ṹ��*/
struct uac_pcm_status {
    uac_pcm_state_t     state;           /* PCM״̬*/
    //int pad1;                          /* Needed for 64 bit alignment */
    int                 hw_ptr;          /* RO: hw ptr (0...boundary-1) */
    struct usb_timespec tstamp;          /* ʱ���*/
    uac_pcm_state_t     suspended_state; /* ����������״̬*/
    struct usb_timespec audio_tstamp;    /* from sample counter or wall clock */
};

/* USB��ƵPCM���ƽṹ��*/
struct uac_pcm_control {
    uint32_t appl_ptr;    /* �û�����ָ��*/
    uint32_t avail_min;   /* ���ڻ��ѵ���С��Ч֡��*/
};

/* PCM����ʱ��ṹ��*/
struct uac_pcm_runtime {
    /* Ӳ������*/
    struct uac_pcm_hardware       hw;                /* PCMӲ������*/
    struct uac_pcm_hw_constraints hw_constraints;    /* PCMӲ����������*/
    /* ��Ƶ��ʽ����*/
    uint32_t                      channels;          /* ͨ��*/
    int                           format_size;       /* ��ʽλ��С*/
    uint32_t                      rate;              /* ������*/
    uint32_t                      periods;           /* ������*/
    uint32_t                      frame_bits;        /* ֡λ��*/
    uint32_t                      sample_bits;       /* ����λ��*/
    uint32_t                      period_size;       /* ���ڴ�С(��֡Ϊ��λ)*/
    uint32_t                      buffer_frame_size; /* �����С(��֡Ϊ��λ)*/
    uint8_t                      *buffer;            /* ����*/
    uint32_t                      buffer_size;       /* �����С(���ֽ�Ϊ��λ)*/
    /* ���������س�Ա*/
    uint32_t                      min_align;         /* ��ʽ��С����*/
    uint32_t                      byte_align;        /* �ֽڶ���*/
    uint32_t                      period_step;
    uint32_t                      start_threshold;   /* ������(��֡Ϊ��λ)*/
    uint32_t                      stop_threshold;    /* ֹͣ��(��֡Ϊ��λ)*/
    uint32_t                      boundary;          /* �߽磬������޶��ڣ�buffer_frame_size�������������ֵ*/
    struct uac_pcm_status         status;            /* ״̬�ṹ��*/
    struct uac_pcm_control        control;           /* ���ƽṹ��*/
    uint32_t                      hw_ptr_base;       /* ��������������λ��*/
    uint32_t                      hw_ptr_interrupt;  /* �ж�ʱ���λ��*/
    uint32_t                      hw_ptr_wrap;
    uint32_t                      twake;             /* ����֡��*/
    uint32_t                      avail_max;         /* ������֡��*/
    /* ʱ������*/
    int                           tstamp_mode;       /* ʱ���ģʽ*/
    struct usb_timespec           trigger_tstamp;    /* ����ʱ���*/
    struct usb_timespec           driver_tstamp;     /* ����ʱ���*/
    int                           delay;             /* �ӳٵ�֡��*/
    /* ������ر���*/
    uint32_t                      silence_start;     /* �������ݿ�ʼ��ָ֡��*/
    uint32_t                      silence_filled;    /* �Ѿ���侲�����ݵĴ�С*/
    uint32_t                      silence_size;      /* ��Ҫ��侲�����ݴ�С(��֡Ϊ��λ)*/
    uint32_t                      silence_threshold; /* ������(��֡Ϊ��λ)*/

    void                         *private_data;      /* ˽������*/
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
/* PCM����������*/
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

/* PCM�����ṹ��*/
struct uac_pcm_substream {
    struct uac_pcm           *pcm;                 /* PCMʵ��*/
    struct uac_pcm_str       *pstr;                /* ���������ṹ��*/
    void                     *private_data;        /* ˽������*/
    usb_mutex_handle_t        mutex;               /* ������*/
    int                       number;              /* �������*/
    char                      name[32];            /* ��������*/
    int                       stream;              /* ��(����) */
    size_t                    buffer_bytes_max;    /* ����λ����ڴ�С*/
    const struct uac_pcm_ops *ops;                 /* PCM����������*/
    struct uac_pcm_runtime   *runtime;             /* ����ʱ��*/
    struct uac_pcm_substream *next;                /* ��һ������ */

    int                       refcnt;              /* ���ü���*/
    uint32_t                  hw_opened: 1;        /* �Ƿ��Ǵ�״̬*/
    uint32_t                  hw_param_set: 1;     /* �Ƿ�������Ӳ������*/
};

/* PCM���ṹ��*/
struct uac_pcm_str {
    int                       stream;            /* ��(����) */
    struct uac_pcm           *pcm;               /* PCMʵ��*/
    uint32_t                  substream_count;   /* ��������*/
    uint32_t                  substream_opened;  /* �����򿪵�����*/
    struct uac_pcm_substream *substream;         /* ����ָ��*/
};

/* PCMʵ��ṹ��*/
struct uac_pcm {
    //struct usb_list_head list;          /* PCM�ڵ�*/
    int                  device;        /* �豸��*/
    uint32_t             info_flags;
    char                 id[64];        /* PCM ID*/
    char                 name[80];      /* PCM ����*/
    struct uac_pcm_str   streams[2];    /* PCM������*/
    //usb_mutex_handle_t   open_mutex;
    void                *private_data;
    void               (*private_free) (struct uac_pcm *pcm);
};


/** �ͷ���ƵPCM*/
void usbh_uac_pcm_free(struct uac_pcm *pcm);
/** ����һ���µ�PCM*/
usb_err_t uac_pcm_new(const char *id, int device, int playback_count,
                      int capture_count, struct uac_pcm **rpcm);
/** ����PCM����������*/
void usbh_uac_set_pcm_ops(struct uac_pcm *pcm, int stream);










/** ��ȡPCM��ʽ���*/
int uac_get_pcm_format_size_bit(uint32_t format_size);
/** ��λPCM*/
usb_err_t uac_pcm_reset(struct uac_pcm_substream *substream, int state, usb_bool_t check_state);
/** ֹͣPCM*/
usb_err_t uac_pcm_stop(struct uac_pcm_substream *substream, uac_pcm_state_t state);
/**
 * ��ͣ/�ָ�������
 *
 * @param substream USB��ƵPCM��������
 * @param push      1����ͣ��0���ָ�
 *
 * @return �ɹ�����USB_OK
 */
usb_err_t uac_pcm_pause(struct uac_pcm_substream *substream, int push);
usb_err_t uac_pcm_drop(struct uac_pcm_substream *substream);
usb_err_t uac_pcm_unlink(struct uac_pcm_substream *substream);
/**
 * ����һ���µ�PCM������
 *
 * @param pcm             PCMʵ��
 * @param stream          ����������
 * @param substream_count ��������������
 *
 * @return �ɹ�����USB_OK
 */
usb_err_t uac_pcm_new_stream(struct uac_pcm *pcm,
                             int stream, int substream_count);
/** ����PCM����������*/
void usbh_uac_set_pcm_ops(struct uac_pcm *pcm, int stream);
/** ��һ��PCM������*/
usb_err_t uac_pcm_open_substream(struct uac_pcm *pcm, int stream,
                                 struct uac_pcm_substream **rsubstream);
/** �Ͽ���������*/
void uac_pcm_detach_substream(struct uac_pcm_substream *substream);
/**
 * USB��ƵPCMд����
 *
 * @param substream   PCM���������ṹ��
 * @param buffer      ����
 * @param buffer_size �����֡��С
 *
 * @param �ɹ����سɹ������֡��
 */
int uac_pcm_write(struct uac_pcm_substream *substream,
                  uint8_t *buffer, uint32_t buffer_frame_size);



/**
 * �����ʹ�����
 *
 * @param params PCMӲ������
 * @param rule   ����ṹ��
 *
 * @return ����1��ʾֵ�ı䣬����0��ʾֵû�ı�
 */
int hw_rule_rate(struct uac_pcm_hw_params *params,
                 struct uac_pcm_hw_rule *rule);
/**
 * ͨ����������
 *
 * @param params PCMӲ������
 * @param rule   ����ṹ��
 *
 * @return ����1��ʾֵ�ı䣬����0��ʾֵû�ı�
 */
int hw_rule_channels(struct uac_pcm_hw_params *params,
                     struct uac_pcm_hw_rule *rule);
/**
 * ��Ƶ��ʽ������
 *
 * @param params PCMӲ������
 * @param rule   ����ṹ��
 *
 * @return ����1��ʾֵ�ı䣬����0��ʾֵû�ı�
 */
int hw_rule_format(struct uac_pcm_hw_params *params,
                   struct uac_pcm_hw_rule *rule);
/**
 * ����ʱ�������
 *
 * @param params PCMӲ������
 * @param rule   ����ṹ��
 *
 * @return ����1��ʾֵ�ı䣬����0��ʾֵû�ı�
 */
int hw_rule_period_time(struct uac_pcm_hw_params *params,
                        struct uac_pcm_hw_rule *rule);
/**
 * ����PCMӲ��������С���ֵ
 *
 * @param runtime USB��Ƶ����ʱ��ṹ��
 * @param var     ����Ӳ������
 * @param min     ��Сֵ
 * @param max     ���ֵ
 *
 * @return ����1��ʾֵ�ı䣬����0��ʾֵû�ı�
 */
int uac_pcm_hw_constraint_minmax(struct uac_pcm_runtime *runtime, uac_pcm_hw_param_t var,
                                 uint32_t min, uint32_t max);
/**
 * ���¶���PCMӲ������
 *
 * @param substream PCM���������ṹ��
 * @param params    Ӳ������
 *
 * @return �ɹ�����USB_OK
 */
usb_err_t uac_pcm_hw_refine(struct uac_pcm_substream *substream,
                            struct uac_pcm_hw_params *params);
/** ѡ��һ��PCMӲ�������ṹ�嶨�������*/
usb_err_t uac_pcm_hw_params_choose(struct uac_pcm_substream *pcm,
                                   struct uac_pcm_hw_params *params);
/**
 * ���һ��USB��ƵPCMӲ�������ƹ���
 *
 * @param runtime PCM����ʱ��ṹ��
 * @param cond    ����λ
 * @param var     �����Ӳ������
 * @param func    ������
 * @param private ���ݸ��������Ĳ���
 * @param dep     ��������
 *
 * @return �ɹ�����USB_OK
 */
usb_err_t uac_pcm_hw_rule_add(struct uac_pcm_runtime *runtime, uint32_t cond,
                        int var,
                        uac_pcm_hw_rule_func_t func, void *private,
                        int dep1, int dep2, int dep3);
/**
 * ��ʼ��USB��ƵPCMӲ������
 *
 * @param substream PCM���������ṹ��
 *
 * @return �ɹ�����USB_OK
 */
usb_err_t uac_pcm_hw_constraints_init(struct uac_pcm_substream *substream);
/** ����PCMӲ����������*/
usb_err_t uac_pcm_hw_constraints_complete(struct uac_pcm_substream *substream);
/** ����usb��ƵӲ������*/
struct uac_pcm_hw_params *usbh_uac_get_hw_params(void *params_usr);


static inline uint32_t pcm_format_to_bits(int pcm_format)
{
    return 1ULL << (int) pcm_format;
}

/** �ֽ�ת��Ϊ֡*/
static inline uint32_t bytes_to_frames(struct uac_pcm_runtime *runtime, uint32_t size)
{
    return size * 8 / runtime->frame_bits;
}

/** ֡ת��Ϊ�ֽ�*/
static inline int frames_to_bytes(struct uac_pcm_runtime *runtime, int size)
{
    return size * runtime->frame_bits / 8;
}

/** ������ת��Ϊ�ֽ�*/
static inline int samples_to_bytes(struct uac_pcm_runtime *runtime, int size)
{
    return size * runtime->sample_bits / 8;
}

/** ����Ƿ��ֽڶ���*/
static inline int frame_aligned(struct uac_pcm_runtime *runtime, int bytes)
{
    return bytes % runtime->byte_align == 0;
}

/** ��ȡ����(��д)�ĻطŻ���ռ�*/
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

/** ��ȡ����(�ɶ�)�ĻطŻ���ռ�*/
static inline uint32_t uac_pcm_capture_avail(struct uac_pcm_runtime *runtime)
{
    int avail = runtime->status.hw_ptr - runtime->control.appl_ptr;
    if (avail < 0){
        avail += runtime->boundary;
    }
    return avail;
}

/** ��ȡ���л���ָ�뵽�û�ָ���֡��*/
static inline int uac_pcm_playback_hw_avail(struct uac_pcm_runtime *runtime)
{
    return runtime->buffer_frame_size - uac_pcm_playback_avail(runtime);
}


/**
 * ����Ƿ������ݴ����ڻطŻ�����
 *
 * @return ����0��ʾû�������ڻ�����
 */
static inline int uac_pcm_playback_data(struct uac_pcm_substream *substream)
{
    struct uac_pcm_runtime *runtime = substream->runtime;

    if (runtime->stop_threshold >= runtime->boundary){
        return 1;
    }
    return uac_pcm_playback_avail(runtime) < runtime->buffer_frame_size;
}

/** ��ȡʱ��*/
static inline void uac_pcm_gettime(struct uac_pcm_runtime *runtime,
                                   struct usb_timespec *tv)
{
    usb_get_timespec(tv);
}

#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif
