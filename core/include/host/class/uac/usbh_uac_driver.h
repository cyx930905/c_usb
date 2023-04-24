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

/* ��ӡ����*/
#define __UAC_TRACE(info, ...) \
            do { \
                usb_printf("USBH-UAC:"info, ##__VA_ARGS__); \
            } while (0)

#define MAX_NR_RATES    1024
/* UAC�豸��VID��PID*/
#define UAC_ID(vendor, product) (((vendor) << 16) | (product))
/* ��ȡUAC�豸��VID*/
#define UAC_ID_VENDOR(id) ((id) >> 16)
/* ��ȡUAC�豸��PID*/
#define UAC_ID_PRODUCT(id) ((uint16_t)(id))

#define combine_word(s)    ((*(s)) | ((uint32_t)(s)[1] << 8))
#define combine_triple(s)  (combine_word(s) | ((uint32_t)(s)[2] << 16))
#define combine_quad(s)    (combine_triple(s) | ((uint32_t)(s)[3] << 24))

#define UAC_CONTROL_BIT(CS) (1 << ((CS) - 1))

/* ��ȡ�豸�ٶ�*/
#define uac_get_speed(p_fun) ((p_fun)->p_udev->speed)
/* ����ģʽ*/
enum {
    USBH_SND_PCM_STREAM_PLAYBACK = 0,    /* ����ģʽ*/
    USBH_SND_PCM_STREAM_CAPTURE,         /* ����ģʽ(¼��)*/
    USBH_SND_PCM_STREAM_LAST = USBH_SND_PCM_STREAM_PLAYBACK,
};

/* ʱ���ģʽ*/
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

/* ��Ƶ��ʽ�ṹ��*/
struct audioformat {
    struct usb_list_head       list;                  /* ��ǰ��ʽ�ڵ�*/
    uint32_t                   formats;               /* ��ʽλ��*/
    uint32_t                   channels;              /* ͨ����*/
    uint32_t                   fmt_type;              /* USB��Ƶ��ʽ����(1-3)*/
    uint32_t                   frame_size;            /* ÿһ֡��������С */
    int                        iface;                 /* �ӿں�*/
    uint8_t                    altsetting;            /* �������ñ��*/
    uint8_t                    attributes;            /* ��Ƶ�˵�����*/
    uint8_t                    endpoint;              /* �˵��*/
    uint8_t                    ep_attr;               /* �˵�����*/
    uint8_t                    datainterval;          /* ���ݰ���� */
    uint8_t                    protocol;              /* UAC�汾��1��2*/
    uint32_t                   maxpacksize;           /*������С*/
    uint32_t                   rates;                 /* ����������λ*/
    uint32_t                   rate_min, rate_max;    /* ��С/��������*/
    uint32_t                   nr_rates;              /* �����ʱ��������*/
    uint32_t                  *rate_table;            /* �����ʱ�*/
    uint8_t                    clock;                 /* ��ص�ʱ��*/
    usb_bool_t                 dsd_dop;               /* add DOP headers in case of DSD samples */
    usb_bool_t                 dsd_bitrev;            /* reverse the bits of each DSD sample */
};

/* USB��Ƶ�����ṹ��*/
struct usbh_uac_substream {
    struct usbh_uac_stream           *stream;         /* ������UAC�豸������*/
    struct usbh_function             *p_fun;          /* USB�ӿڹ��ܽṹ��*/
    struct uac_pcm_substream         *pcm_substream;  /* PCM����*/
    int                               direction;      /* PCM���򣺻طŻ򲶻�*/
    int                               interface;      /* ��ǰ�ӿں�*/
    int                               endpoint;       /* �˵��*/
    struct audioformat               *cur_audiofmt;   /* ��ǰ����Ƶ��ʽָ��*/
    uac_pcm_format_t                  pcm_format_size;/* ��ǰ��Ƶ��ʽ���*/
    uint32_t                          channels;       /* ��ǰ��ͨ����*/
    uint32_t                          channels_max;   /* ������Ƶ��ʽ�����ͨ��*/
    uint32_t                          cur_rate;       /* ��ǰ������*/
    uint32_t                          period_bytes;   /* ��ǰ�����ֽ�*/
    uint32_t                          period_frames;  /* ��ǰÿ�����ڵ�֡�� */
    uint32_t                          buffer_periods; /* ��ǰÿ�������������*/
    uint32_t                          altsetting;     /* USB���ݸ�ʽ: �������ú�*/
    uint32_t                          txfr_quirk:1;   /* allow sub-frame alignment */
    uint32_t                          fmt_type;       /* USB ��Ƶ��ʽ���� (1-3) */
    uint32_t                          pkt_offset_adj; /* Ҫ�����ݰ���ͷɾ�����ֽ���(���ڲ��Ϲ���豸)*/
    uint32_t                          maxframesize;   /* ���֡��*/
    uint32_t                          curframesize;   /* ��ǰ֡��*/

    usb_bool_t                        running;        /* ����״̬ */

    uint32_t                          hwptr_done;     /* �����д����ֽڵ�λ��*/
    uint32_t                          transfer_done;  /* ���ϴ����ڸ��º����֡��*/
    uint32_t                          frame_limit;    /* USB�����������֡��������*/

    /* ��������������ݺ�ͬ���˵�*/
    uint32_t                          ep_num;         /* �˵��*/
    struct usbh_uac_endpoint         *data_endpoint;
    struct usbh_uac_endpoint         *sync_endpoint;
    uint32_t                          flags;
    usb_bool_t                        need_setup_ep;  /* �Ƿ����úö˵�׼���շ�����*/
    uint32_t                          speed;          /* USB�豸�ٶ�*/

    uint32_t                          formats;        /* ��ʽλ��*/
    uint32_t                          num_formats;    /* ֧�ֵ���Ƶ�ĸ�ʽ������(����)*/
    struct usb_list_head              fmt_list;       /* ��ʽ����*/
    struct uac_pcm_hw_constraint_list rate_list;      /* ���޲�����*/

    int last_frame_number;                            /* ���µ�����֡���*/
    int last_delay;                                   /* ������ʱ����֡����*/

    struct {
        int marker;
        int channel;
        int byte_idx;
    } dsd_dop;
    /* �Ƿ��������ʱ��*/
    usb_bool_t                         trigger_tstamp_pending_update;
};

struct usbh_audio;

/* USB��Ƶ�豸�������ṹ��*/
struct usbh_uac_stream {
    struct usbh_audio        *chip;        /* USB��Ƶ�豸*/
    struct uac_pcm           *pcm;         /* PCMʵ��*/
    int                       pcm_index;   /* PCM����*/
    uint32_t                  fmt_type;    /* USB��Ƶ��ʽ����(1-3)*/
    struct usbh_uac_substream substream[2];/* USB��Ƶ�豸��������*/
    struct usb_list_head      list;        /* ����������ڵ�*/
};

/* USB��Ƶ�ṹ��*/
struct usbh_audio{
    int                    index;          /* ��Ƶ�豸����*/
    struct usbh_function  *p_fun;          /* USB�ӿڹ��ܽṹ��*/
    uint32_t               usb_id;         /* USB�豸��VID��PID*/
    char                   name[80];       /* ��Ƶ�豸����*/
    char                   mixername[80];  /* ��������*/
    struct usbh_interface *pm_intf;        /* ��Դ����ӿ�*/
    struct usbh_interface *ctrl_intf;      /* ���ƽӿ�*/
    usb_bool_t             shutdown;       /* �Ƿ�ֹͣ*/
    usb_bool_t             txfr_quirk;     /* �Ƿ��д������*/
    int                    pcm_devs;       /* PCM�豸��*/
    usb_mutex_handle_t     stream_mutex;   /* ������������*/
    struct usb_list_head   stream_list;    /* ����������*/
    usb_mutex_handle_t     ep_mutex;       /* �˵㻥����*/
    struct usb_list_head   ep_list;        /* ��Ƶ��ض˵������*/
    struct usb_list_head   mixer_list;     /* �����ӿ�����*/
    int                    num_ctrl_intf;  /* ���ƽӿ�����*/
};


/** �ϲ��ֽڲ��õ�һ������ֵ*/
uint32_t usbh_uac_combine_bytes(uint8_t *bytes, int size);
/** UAC������س�ʼ��*/
usb_err_t usbh_uac_lib_init(void);
/**
 * ����һ��UAC�豸
 *
 * @param p_fun USB�ӿڹ���
 *
 * @return �ɹ�����USB_OK
 */
usb_err_t usbh_uac_dev_create(struct usbh_function *p_fun);
/**
 * ����һ��UAC�豸
 *
 * @param p_fun USB�ӿڹ���
 *
 * @return �ɹ�����USB_OK
 */
usb_err_t usbh_uac_dev_destory(struct usbh_function *p_fun);
/** �ͷ�USB��ʵ��*/
void usbh_uac_audio_stream_free(struct usbh_uac_stream *stream);
/**
 * �������������棬���ظ��������������͵���ʼָ��
 *
 * @param descstart ��������ʼ��ַ
 * @param desclen   ����������
 * @param after
 * @param dtype     ҪѰ�ҵ�����������
 */
void *usbh_uac_find_desc(void *descstart, int desclen, void *after, uint8_t dtype);
/** ͨ�������ҵ����ض��Ľӿ�������*/
void *usbh_uac_find_csint_desc(void *buffer, int buflen, void *after, uint8_t dsubtype);
/** ��ȡ�˵����ѯ���*/
uint8_t usbh_uac_parse_datainterval(struct usbh_audio     *chip,
                                    struct usbh_interface *p_intf);
/**
 * Ѱ����Ƶ�豸
 *
 * @param audio_index ��Ƶ�豸����
 *
 * @return �ɹ�����USB��Ƶ�ṹ��
 */
struct usbh_audio *usbh_uac_dev_find(int audio_index);
/**
 * ����USB��Ƶ�豸���ݽӿ�
 *
 * @param p_audio  USB��Ƶ�豸
 * @param iface_no ���ݽӿڱ��
 *
 * @return �ɹ�����USB_OK
 */
usb_err_t usbh_uac_parse_audio_interface(struct usbh_audio *p_audio, int iface_no);
/** ��ȡ��ʽ����λ��*/
usb_err_t uac_pcm_format_physical_width(uac_pcm_format_t format);
/**
 * UAC���ÿ��ƴ���
 *
 * @param p_fun       ��صĹ��ܽӿ�
 * @param request     �����USB����
 * @param requesttype ���䷽��|��������|����Ŀ��
 * @param value       ����
 * @param index       ����
 * @param data        ����(����Ϊ��)
 * @param size        ���ݳ���
 *
 * @return ��������ݴ��䣬�ɹ����سɹ�������ֽ���
 *         ���û�����ݴ��䣬�ɹ�����USB_OK
 */
usb_err_t usbh_uac_ctl_msg(struct usbh_function *p_fun, uint8_t request,
                           uint8_t requesttype, uint16_t value, uint16_t index, void *data,
                           uint16_t size);
/** ��ʼ����������*/
usb_err_t usbh_uac_init_pitch(struct usbh_audio *chip, int iface,
		                      struct usbh_interface *alts,
		                      struct audioformat *fmt);

/** ���PCM���������Ƿ���������*/
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
