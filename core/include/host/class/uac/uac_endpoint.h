#ifndef __UAC_ENDPOIONT_H_
#define __UAC_ENDPOIONT_H_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "usb_adapter_inter.h"
#include "usb_errno.h"
#include "list/usb_list.h"
#include "host/class/uac/usbh_uac_driver.h"

/* ��ӡ����*/
#define __UAC_EP_TRACE(info, ...) \
            do { \
                usb_printf("USBH-UAC-EP:"info, ##__VA_ARGS__); \
            } while (0)

/* ��Ƶ�˵���չ����*/
#define USB_EP_AUDIO_SIZE  9
/* USB��Ƶ���Ĵ������������*/
#define UAC_MAX_TRPS       12
/* ÿ������������������ݰ���*/
#define UAC_MAX_PACKS      6
/* ����ģʽ��ÿ������������������ݰ���*/
#define UAC_MAX_PACKS_HS   (UAC_MAX_PACKS * 8)
/* 1ms�ﾡ����Ҫ����������г���*/
#define UAC_MAX_QUEUE      18

struct usbh_uac_endpoint;
/* UAC�������������*/
struct uac_trp_ctx {
    struct usbh_uac_endpoint *ep;                            /* ������ض˵�*/
    int                       index;                         /* �����������������*/
    int                       packets;                       /* ÿ��USB��������������ݰ�����*/
    int                       packet_size[UAC_MAX_PACKS_HS]; /* ��һ�ε��ύ�İ��Ĵ�С*/
    uint32_t                  buffer_size;                   /* ���ݻ���Ĵ�С*/
    struct usbh_trp          *trp;                           /* USB���������*/
    struct usb_list_head      ready_list;                    /* ׼���õĻ������Ԥ������*/
};

/* UAC��Ƶ�豸�˵�ṹ��*/
struct usbh_uac_endpoint {
    struct usbh_audio         *chip;                /* ��������Ƶ�豸*/
    struct usbh_endpoint      *p_ep;                /* ��ض˵�*/
    int                        ep_num;              /* �˵��*/
    int                        iface;               /* �����ӿں�*/
    int                        altsetting;          /* �����ӿڵı������ú�*/
    int                        type;                /* ��Ƶ�˵������*/
    int                        use_count;           /* ʹ�ü���*/
    struct usb_list_head       list;                /* ��Ƶ�˵�ڵ�*/
    struct usb_list_head       ready_playback_trps; /* ׼���طŵĴ��������*/
    uint32_t                   syncinterval;        /* P for adaptive mode, 0 otherwise */
    uint32_t                   syncmaxsize;         /* ͬ���˵����С*/
    uint32_t                   datainterval;        /* log_2�����ݰ�ʱ����*/
    uint32_t                   maxpacksize;         /* ������С(�ֽ�)*/
    uint32_t                   maxframesize;        /* ���ݰ������Ƶ֡��С*/
    uint32_t                   curpacksize;         /* ��ǰ����С(�ֽڣ����ڲ���)*/
    uint32_t                   curframesize;        /* ��ǰ�������Ƶ֡��С(���ڲ���ģʽ)*/
    uint32_t                   fill_max:1;          /* �������������С*/
    uint32_t                   freqn;               /* �� Q16.16��ʽ��fs/fps����ͨ������ */
    uint32_t                   freqm;               /* �� Q16.16��ʽ��fs/fps��˲ʱ������*/
    uint32_t                   freqmax;             /* �������ʣ����ڻ������*/
    int                        freqshift;           /* how much to shift the feedback value to get Q16.16 */
    uint32_t                   phase;               /* phase accumulator */
    uint32_t                   ntrps;               /* ���������������*/
    struct uac_trp_ctx         trp[UAC_MAX_TRPS];   /* UAC���������*/
    uint32_t                   max_trp_frames;      /* �����������Ƶ���������*/
    struct usbh_uac_substream *data_subs;           /* ��������*/
    void (*prepare_data_trp) (struct usbh_uac_substream *subs,
                              struct usbh_trp *trp);
    void (*retire_data_trp) (struct usbh_uac_substream *subs,
                             struct usbh_trp *trp);
    uint32_t                   stride;
    uint8_t                    silence_value;       /* ����������ֵ*/
    uint32_t                   flags;
    int                        skip_packets;        /* ��Щ�豸�����������ǰ��n����*/
};

/** ������һ����Ҫ���͵���������*/
int usbh_uac_endpoint_next_packet_size(struct usbh_uac_endpoint *ep);
/**
 * ���һ���˵㵽USB��Ƶ�豸
 *
 * @param chip      USB��Ƶ�豸�ṹ��
 * @param alts      USB�ӿ�
 * @param ep_num    Ҫʹ�õĶ˵��
 * @param direction PCM����������(�طŻ��߲���)
 * @param type      ��Ƶ�˵�����
 *
 * @return �ɹ�����USB��Ƶ�˵�ṹ��
 */
struct usbh_uac_endpoint *usbh_uac_add_endpoint(struct usbh_audio *chip,
                          struct usbh_interface *alts,
                          int ep_num, int direction, int type);
/**
 * ������Ƶ�豸�˵�
 *
 * @param ep              Ҫ���õĶ˵�
 * @param pcm_format      ��ƵPCM��ʽ
 * @param channels        ��Ƶͨ������
 * @param period_bytes    һ�����ڵ��ֽ���
 * @param period_frames   һ�������ж���֡
 * @param buffer_periods  һ�����������������
 * @param rate            ֡��
 * @param fmt             USB��Ƶ��ʽ��Ϣ
 * @param sync_ep         Ҫʹ�õ�ͬ���˵�
 *
 * @return �ɹ�����USB_OK
 */
usb_err_t usbh_uac_endpoint_set_params(struct usbh_uac_endpoint *ep,
                                       uac_pcm_format_t pcm_format,
                                       uint32_t channels,
                                       uint32_t period_bytes,
                                       uint32_t period_frames,
                                       uint32_t buffer_periods,
                                       uint32_t rate,
                                       struct audioformat *fmt,
                                       struct usbh_uac_endpoint *sync_ep);
/**
 * ����һ��USB��Ƶ�豸�˵�
 *
 * @param ep        Ҫ�����Ķ˵�
 * @param can_sleep
 *
 * @return �ɹ�����USB_OK
 */
usb_err_t usbh_uac_endpoint_start(struct usbh_uac_endpoint *ep, usb_bool_t can_sleep);
/** ֹͣ�˵�*/
void usbh_uac_endpoint_stop(struct usbh_uac_endpoint *ep);
/**
 * �ͷ�һ����Ƶ�˵㻺��
 *
 * @param ep ��Ƶ�豸�˵�
 */
void usbh_uac_endpoint_free(struct usbh_uac_endpoint *ep);
/**
 * �ͷ�һ���˵�
 *
 * @param ep ��Ƶ�豸�˵�
 */
void usbh_uac_endpoint_release(struct usbh_uac_endpoint *ep);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif


