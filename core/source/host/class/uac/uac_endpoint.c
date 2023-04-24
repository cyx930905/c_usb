#include "host/class/uac/uac_pcm.h"
#include "host/class/uac/usbh_uac_driver.h"
#include "host/class/uac/uac_quirks.h"
#include "host/class/uac/uac_endpoint.h"
#include <string.h>
#include <stdio.h>

#define EP_FLAG_RUNNING     1
#define EP_FLAG_STOPPING    2

#define SUBSTREAM_FLAG_DATA_EP_STARTED  0
#define SUBSTREAM_FLAG_SYNC_EP_STARTED  1

/** ת��һ�������ʵ�ȫ�ٸ�ʽ(fs/1000 in Q16.16)*/
static uint32_t get_usb_full_speed_rate(uint32_t rate)
{
    return ((rate << 13) + 62) / 125;
}

/** ת��һ�������ʵ����ٸ�ʽ(fs/8000 in Q16.16)*/
static uint32_t get_usb_high_speed_rate(uint32_t rate)
{
    return ((rate << 10) + 62) / 125;
}

/** �ͷŶ˵�Ĵ��������*/
static void release_trps(struct usbh_uac_endpoint *ep, int force)
{
    int i;
    if(ep == NULL){
        return;
    }
    for (i = 0; i < ep->ntrps; i++) {


    }
}


int usbh_uac_endpoint_next_packet_size(struct usbh_uac_endpoint *ep);

/*
 * Prepare a CAPTURE or SYNC urb for submission to the bus.
 */
static inline void prepare_inbound_urb(struct usbh_uac_endpoint *ep,
                                       struct uac_trp_ctx       *trp_ctx)
{
    int i, offs;
    struct usbh_trp *p_trp = trp_ctx->trp;

    //urb->dev = ep->chip->dev; /* we need to set this at each time */

    switch (ep->type) {
    case SND_USB_ENDPOINT_TYPE_DATA:
        offs = 0;
        for (i = 0; i < trp_ctx->packets; i++) {
            p_trp->iso_frame_desc[i].offset = offs;
            p_trp->iso_frame_desc[i].length = ep->curpacksize;
            offs += ep->curpacksize;
        }

        p_trp->len = offs;
        p_trp->number_of_packets = trp_ctx->packets;
        break;

    case SND_USB_ENDPOINT_TYPE_SYNC:
        p_trp->iso_frame_desc[0].length = min(4u, ep->syncmaxsize);
        p_trp->iso_frame_desc[0].offset = 0;
        break;
    }
}

/** ׼��һ��Ҫ�ύ�����ߵĻط�USB���������*/
static void prepare_outbound_trp(struct usbh_uac_endpoint *ep,
                                 struct uac_trp_ctx *ctx){
    int i;
    struct usbh_trp *trp = ctx->trp;
    //uint8_t *cp = trp->p_data;

    switch (ep->type) {
    case SND_USB_ENDPOINT_TYPE_DATA:
        if (ep->prepare_data_trp) {
            ep->prepare_data_trp(ep->data_subs, trp);
        } else {
            /* ���û�����ݣ����;�������*/
            uint32_t offs = 0;
            for (i = 0; i < ctx->packets; ++i) {
                int counts;

                if (ctx->packet_size[i]){
                    counts = ctx->packet_size[i];
                }else{
                    counts = usbh_uac_endpoint_next_packet_size(ep);
                }
                trp->iso_frame_desc[i].offset = offs * ep->stride;
                trp->iso_frame_desc[i].length = counts * ep->stride;
                offs += counts;
            }

            trp->number_of_packets = ctx->packets;
            trp->len = offs * ep->stride;
            memset(trp->p_data, ep->silence_value,
                   offs * ep->stride);
        }
        break;
    case SND_USB_ENDPOINT_TYPE_SYNC:
        break;
    }
}

static void retire_outbound_trp(struct usbh_uac_endpoint *ep,
                                struct uac_trp_ctx       *trp_ctx)
{
    if (ep->retire_data_trp){
        ep->retire_data_trp(ep->data_subs, trp_ctx->trp);
    }
}

static void retire_inbound_trp(struct usbh_uac_endpoint *ep,
                               struct uac_trp_ctx       *trp_ctx)
{
    struct usbh_trp *p_trp = trp_ctx->trp;

    if (ep->skip_packets > 0) {
        ep->skip_packets--;
        return;
    }

//    if (ep->sync_slave)
//        snd_usb_handle_sync_urb(ep->sync_slave, ep, urb);

    if (ep->retire_data_trp){
        ep->retire_data_trp(ep->data_subs, p_trp);
    }
}

#if 0
/** ��Ƶ������ɻص�����*/
static void uac_complete_trp(void *arg)
{
    struct usbh_trp *p_trp = (struct usbh_trp *)arg;
    struct uac_trp_ctx *ctx = p_trp->priv;
    struct usbh_uac_endpoint *ep = ctx->ep;
    //struct uac_pcm_substream *substream;

    usb_err_t ret;

    if ((p_trp->status == -USB_ENOENT) ||          /* δ����*/
            (p_trp->status == -USB_ENODEV) ||      /* �豸�Ƴ�*/
            (p_trp->status == -USB_ECONNRESET) ||  /* δ���� */
            (p_trp->status == -USB_ESHUTDOWN) ||   /* �豸����*/
            (ep->chip->shutdown)){                 /* �豸�Ƴ�*/
        return;
    }

    if((p_trp->status == USB_OK) || (p_trp->status == -USB_EXDEV)){
        if (!USBH_GET_EP_DIR(ep->p_ep)) {
            retire_outbound_trp(ep, ctx);
            if (!(ep->flags & (1 << EP_FLAG_RUNNING))){
                return;
            }

            prepare_outbound_trp(ep, ctx);
        } else {
            retire_inbound_trp(ep, ctx);
            /* can be stopped during retire callback */
            if (!(ep->flags & (1 << EP_FLAG_RUNNING))){
                return;
            }

            prepare_inbound_urb(ep, ctx);
        }
    }

    /* �����ύ���������*/
    if((p_trp->status == USB_OK) || (p_trp->status == -USB_EPROTO)){
        ret = usbh_trp_submit(p_trp);
        if (ret == USB_OK){
            //__UAC_EP_TRACE("submit trp successful %d\r\n", p_trp->start_frame);
            return;
        }
        __UAC_EP_TRACE("cannot submit trp (err = %d)\r\n", ret);
    }
}
#else
/** ��Ƶ������ɻص�����*/
static void uac_complete_trp(void *arg)
{
    struct usbh_trp *p_trp = (struct usbh_trp *)arg;
    struct uac_trp_ctx *ctx = p_trp->priv;
    struct usbh_uac_endpoint *ep = ctx->ep;
    //struct uac_pcm_substream *substream;

    usb_err_t ret;

    if ((p_trp->status == -USB_ENOENT) ||          /* δ����*/
            (p_trp->status == -USB_ENODEV) ||      /* �豸�Ƴ�*/
            (p_trp->status == -USB_ECONNRESET) ||  /* δ���� */
            (p_trp->status == -USB_ESHUTDOWN) ||   /* �豸����*/
            (ep->chip->shutdown)){                 /* �豸�Ƴ�*/
        return;
    }

    if(p_trp->status == USB_OK){
        if (!USBH_GET_EP_DIR(ep->p_ep)) {
            retire_outbound_trp(ep, ctx);
            if (!(ep->flags & (1 << EP_FLAG_RUNNING))){
                return;
            }

            prepare_outbound_trp(ep, ctx);
        } else {
            retire_inbound_trp(ep, ctx);
            /* can be stopped during retire callback */
            if (!(ep->flags & (1 << EP_FLAG_RUNNING))){
                return;
            }

            prepare_inbound_urb(ep, ctx);
        }
    }

    ret = usbh_trp_submit(p_trp);
    if (ret == USB_OK){
        return;
    }
    __UAC_EP_TRACE("cannot submit trp (err = %d)\r\n", ret);
}

#endif

/** ����һ�����ݶ˵�(������)*/
static usb_err_t data_ep_set_params(struct usbh_uac_endpoint *ep,
                                    uac_pcm_format_t pcm_format,
                                    uint32_t channels,
                                    uint32_t period_bytes,
                                    uint32_t frames_per_period,
                                    uint32_t periods_per_buffer,
                                    struct audioformat *fmt,
                                    struct usbh_uac_endpoint *sync_ep){
    uint32_t maxsize, minsize, packs_per_ms, max_packs_per_trp;
    uint32_t max_packs_per_period, trps_per_period, trp_packs;
    uint32_t i, max_trps;
    /* ֡����λ��(ÿһ֡�ж���λ)*/
    int frame_bits = uac_pcm_format_physical_width(pcm_format) * channels;

    if ((pcm_format == SNDRV_PCM_FORMAT_DSD_U16_LE) && (fmt->dsd_dop)) {
        /* ��������DSD DOPģʽʱ��Ӳ���ϵ�һ������֡�Ĵ�С��ͬ��ʵ�������ʽ��λ����Ϊ������Ҫ
         * ����Ŀռ�洢DOP���*/
        frame_bits += channels << 3;
    }
    /* �������ݼ��*/
    ep->datainterval = fmt->datainterval;
    /* ֡�����ֽ�*/
    ep->stride = frame_bits >> 3;
    /* ���þ�������*/
    if(pcm_format == SNDRV_PCM_FORMAT_U8){
        ep->silence_value = 0x80;
    }
    else{
        ep->silence_value = 0;
    }

//    if(USBH_GET_EP_DIR(ep->p_ep) || ()){
//
//    }
//    if (usb_pipein(ep->pipe) ||
//            snd_usb_endpoint_implicit_feedback_sink(ep)) {
//
//

    /* ��������Ƶ�ʱ������ĸ�25%*/
    ep->freqmax = ep->freqn + (ep->freqn >> 2);
    maxsize = ((ep->freqmax + 0xffff) * (frame_bits >> 3))
                >> (16 - ep->datainterval);

    /* wMaxPacketSize ���ܻ�������������С*/
    if (ep->maxpacksize && (ep->maxpacksize < maxsize)) {
        maxsize = ep->maxpacksize;
        ep->freqmax = (maxsize / (frame_bits >> 3))
                << (16 - ep->datainterval);
    }

    if (ep->fill_max){
        ep->curpacksize = ep->maxpacksize;
    }else{
        ep->curpacksize = maxsize;
    }
    /* �����豸*/
    if (uac_get_speed(ep->chip->p_fun) != USB_SPEED_FULL) {
        /* USB��ʱ���䣬����ģʽ��1ms=8΢֡��8/���ݼ��=1ms���ٸ����ݰ�*/
        packs_per_ms = 8 >> ep->datainterval;
        max_packs_per_trp = UAC_MAX_PACKS_HS;
    } else {
        /* ȫ��/����ģʽ��1֡1�����ݰ�*/
        packs_per_ms = 1;
        max_packs_per_trp = UAC_MAX_PACKS;
    }


//    if (sync_ep && !snd_usb_endpoint_implicit_feedback_sink(ep))
//        max_packs_per_urb = min(max_packs_per_urb,
//                    1U << sync_ep->syncinterval);
    if (sync_ep){

    }
    else{
        max_packs_per_trp = max(1u, max_packs_per_trp >> ep->datainterval);
        /* ����һ�����ݰ����Զ�С*/
        minsize = (ep->freqn >> (16 - ep->datainterval)) *
                (frame_bits >> 3);

        if (sync_ep){
            minsize -= minsize >> 3;
        }
        minsize = max(minsize, 1u);

        /* ������ٸ����ݰ����԰���һ��������*/
        max_packs_per_period = USB_DIV_ROUND_UP(period_bytes, minsize);
        /* ����һ������Ҫ���ٸ�USB���������*/
        trps_per_period = USB_DIV_ROUND_UP(max_packs_per_period,
                max_packs_per_trp);

        /* ÿһ��USB�����������Ҫ���ٸ����ݰ�*/
        trp_packs = USB_DIV_ROUND_UP(max_packs_per_period, trps_per_period);
        /* ����һ��USB�������������Ƶ֡������*/
        ep->max_trp_frames = USB_DIV_ROUND_UP(frames_per_period,
                    trps_per_period);
        /* ����ʹ���㹻��USB���������ȥ����һ����ɵ���Ƶ����*/
        max_trps = min((unsigned) UAC_MAX_TRPS,
                UAC_MAX_QUEUE * packs_per_ms / trp_packs);
        ep->ntrps = min(max_trps, trps_per_period * periods_per_buffer);
    }
    /* ����ͳ�ʼ�����ݴ��������*/
    for (i = 0; i < ep->ntrps; i++) {
        struct uac_trp_ctx *u = &ep->trp[i];
        u->index = i;
        u->ep = ep;
        u->packets = trp_packs;
        u->buffer_size = maxsize * u->packets;
        /* ���ڴ���ָ���*/
        if (fmt->fmt_type == UAC_FORMAT_TYPE_II){
            u->packets++;
        }

        u->trp = usb_mem_alloc(sizeof(struct usbh_trp));
        if (u->trp == NULL){
            goto out_of_memory;
        }
        memset(u->trp, 0, sizeof(struct usbh_trp));
        /* �����ʱ֡�������ڴ�*/
        u->trp->iso_frame_desc = usb_mem_alloc(u->packets *
                                               sizeof(struct usb_iso_packet_descriptor));
        if(u->trp->iso_frame_desc == NULL){
            goto out_of_memory;
        }
        memset(u->trp->iso_frame_desc, 0, u->packets * sizeof(struct usb_iso_packet_descriptor));

        u->trp->p_data = usb_mem_alloc(u->buffer_size);
        if (u->trp->p_data == NULL){
            goto out_of_memory;
        }
        memset(u->trp->p_data, 0, u->buffer_size);

        u->trp->p_ep = ep->p_ep;
        u->trp->interval = 1 << ep->datainterval;
        //u->trp->interval = 2;
        u->trp->priv = u;
        u->trp->p_arg = u->trp;
        u->trp->flag = USBH_TRP_ISO_ASAP;
        u->trp->pfn_done = uac_complete_trp;
        USB_INIT_LIST_HEAD(&u->ready_list);
    }
    return USB_OK;
out_of_memory:
    release_trps(ep, 0);
    return -USB_ENOMEM;
}

/** ������һ����Ҫ���͵���������*/
int usbh_uac_endpoint_next_packet_size(struct usbh_uac_endpoint *ep)
{
    int ret;

    if (ep->fill_max){
        return ep->maxframesize;
    }
    ep->phase = (ep->phase & 0xffff)
        + (ep->freqm << ep->datainterval);
    ret = min(ep->phase >> 16, ep->maxframesize);

    return ret;
}

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
                                       struct usbh_uac_endpoint *sync_ep){
    usb_err_t err;

    /* �˵�ʹ����*/
    if (ep->use_count != 0) {
        __UAC_EP_TRACE("Unable to change format on ep #%x: already in use\r\n",
             ep->ep_num);
        return -USB_EBUSY;
    }

    /* ����У��ͷžɵĻ���*/
    release_trps(ep, 0);

    ep->datainterval = fmt->datainterval;
    ep->maxpacksize = fmt->maxpacksize;
    ep->fill_max = !!(fmt->attributes & UAC_EP_CS_ATTR_FILL_MAX);

    if (uac_get_speed(ep->chip->p_fun) == USB_SPEED_FULL){
        ep->freqn = get_usb_full_speed_rate(rate);
    }
    else{
        ep->freqn = get_usb_high_speed_rate(rate);
    }
    /* ��16.16��ʽ����Ƶ��*/
    ep->freqm = ep->freqn;
    ep->freqshift = USB_INT_MIN;

    ep->phase = 0;

    switch (ep->type) {
    case  SND_USB_ENDPOINT_TYPE_DATA:
        /* �������ݶ˵����*/
        err = data_ep_set_params(ep, pcm_format, channels,
                     period_bytes, period_frames,
                     buffer_periods, fmt, sync_ep);
        break;
    case  SND_USB_ENDPOINT_TYPE_SYNC:
        //todo err = sync_ep_set_params(ep);
        break;
    default:
        err = -USB_EINVAL;
    }

    __UAC_EP_TRACE("Setting params for ep #%x (type %d, %d trps), ret=%d\r\n",
        ep->ep_num, ep->type, ep->ntrps, err);

    return err;
}

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
                          int ep_num, int direction, int type){
    struct usbh_uac_endpoint *ep = NULL;
    int is_playback = 0;
    /* �Ƿ��ǻط�*/
    if(direction == USBH_SND_PCM_STREAM_PLAYBACK){
        is_playback = 1;
    }

    if (alts == NULL){
        return NULL;
    }

    usb_mutex_lock(chip->ep_mutex, 5000);

    /* Ѱ�Ҷ˵��Ƿ��Ѿ�����*/
    usb_list_for_each_entry(ep, &chip->ep_list, struct usbh_uac_endpoint, list) {
        if ((ep->ep_num == ep_num) &&
            (ep->iface == alts->p_desc->bInterfaceNumber) &&
            (ep->altsetting == alts->p_desc->bAlternateSetting)) {
            __UAC_EP_TRACE("Re-using EP %x in iface %d,%d @%p\r\n",
                    ep_num, ep->iface, ep->altsetting, ep);
            goto __exit_unlock;
        }
    }

    __UAC_EP_TRACE("Creating new %s %s endpoint #%x\r\n",
            is_playback ? "playback" : "capture",
            type == SND_USB_ENDPOINT_TYPE_DATA ? "data" : "sync",
            ep_num);

    ep = usb_mem_alloc(sizeof(struct usbh_uac_endpoint));
    if (ep == NULL){
        goto __exit_unlock;
    }
    memset(ep, 0, sizeof(struct usbh_uac_endpoint));

    ep->chip = chip;
    ep->type = type;
    ep->ep_num = ep_num;
    ep->iface = alts->p_desc->bInterfaceNumber;
    ep->altsetting = alts->p_desc->bAlternateSetting;
    USB_INIT_LIST_HEAD(&ep->ready_playback_trps);

    ep_num &= USB_EP_NUM_MASK;

    if(is_playback){
        ep->p_ep = chip->p_fun->p_udev->ep_out[ep_num];
    }
    else{
        ep->p_ep = chip->p_fun->p_udev->ep_in[ep_num];
    }

    if (type == SND_USB_ENDPOINT_TYPE_SYNC) {
        if((alts->eps[1].p_desc->bLength >= USB_EP_AUDIO_SIZE) &&
                (alts->eps[1].p_desc->bRefresh >= 1) &&
                (alts->eps[1].p_desc->bRefresh <= 9)){
            ep->syncinterval = alts->eps[1].p_desc->bRefresh;
        }
        else if(uac_get_speed(chip->p_fun) == USB_SPEED_FULL){
            ep->syncinterval = 1;
        }
        else if((alts->eps[1].p_desc->bInterval >= 1) &&
                (alts->eps[1].p_desc->bInterval <= 16)){
            ep->syncinterval = alts->eps[1].p_desc->bInterval - 1;
        }
        else{
            ep->syncinterval = 3;
        }

        ep->syncmaxsize = USB_CPU_TO_LE16(alts->eps[1].p_desc->wMaxPacketSize);

        if ((chip->usb_id == UAC_ID(0x0644, 0x8038)) /* TEAC UD-H01 */ &&
            (ep->syncmaxsize == 4)){
            //todo
        }
    }

    usb_list_add_tail(&ep->list, &chip->ep_list);

__exit_unlock:
    usb_mutex_unlock(chip->ep_mutex);

    return ep;
}

/**
 * ����һ��USB��Ƶ�豸�˵�
 *
 * @param ep        Ҫ�����Ķ˵�
 * @param can_sleep
 *
 * @return �ɹ�����USB_OK
 */
usb_err_t usbh_uac_endpoint_start(struct usbh_uac_endpoint *ep, usb_bool_t can_sleep){
    usb_err_t ret;
    unsigned int i;

    if (ep->chip->shutdown){
        return -USB_ENODEV;
    }
    /* �Ƿ���������*/
    if (++ep->use_count != 1){
        return USB_OK;
    }

    usbh_uac_endpoint_start_quirk(ep);

    ep->flags |= (1 << EP_FLAG_RUNNING);

    for (i = 0; i < ep->ntrps; i++) {
        struct usbh_trp *trp = ep->trp[i].trp;

        if (trp == NULL){
            goto __error;
        }

        if(!USBH_GET_EP_DIR(ep->p_ep)){
            prepare_outbound_trp(ep, trp->priv);
        }
        /* �ύUSB���������*/
        ret = usbh_trp_submit(trp);
        if(ret != USB_OK){
            __UAC_EP_TRACE("cannot submit trp %d, error %d\r\n",
                    i, ret);
            goto __error;
        }
        //__UAC_EP_TRACE("submit trp successful %d\r\n", trp->start_frame);
    }
    return USB_OK;

__error:
    ep->flags &= (~(1 << EP_FLAG_RUNNING));
    ep->use_count--;
    return -USB_EPIPE;
}

/** ֹͣ�˵�*/
void usbh_uac_endpoint_stop(struct usbh_uac_endpoint *ep)
{
    if (ep == NULL){
        return;
    }
    if (ep->use_count == 0){
        return;
    }
    if (--ep->use_count == 0) {
        //deactivate_urbs(ep, false);
        ep->data_subs = NULL;
//        ep->sync_slave = NULL;
        ep->retire_data_trp = NULL;
        ep->prepare_data_trp = NULL;
        ep->flags |= (1 << EP_FLAG_STOPPING);
    }
}

/**
 * �ͷ�һ���˵�
 *
 * @param ep ��Ƶ�豸�˵�
 */
void usbh_uac_endpoint_release(struct usbh_uac_endpoint *ep)
{
    int i;
    if(ep == NULL){
        return;
    }



    ep->retire_data_trp = NULL;
    ep->prepare_data_trp = NULL;
    /* �ͷŶ˵㻺��*/
    for (i = 0; i < ep->ntrps; i++){
        if (ep->trp[i].buffer_size){
            usb_mem_free(ep->trp[i].trp->p_data);
        }
        usb_mem_free(ep->trp[i].trp);
        ep->trp[i].trp = NULL;
    }

    ep->ntrps = 0;
}

/**
 * �ͷ�һ����Ƶ�˵㻺��
 *
 * @param ep ��Ƶ�豸�˵�
 */
void usbh_uac_endpoint_free(struct usbh_uac_endpoint *ep){
    if(ep == NULL){
        return;
    }
    usb_mem_free(ep);
}

/** ֹͣ�˵�*/
void uac_stop_endpoints(struct usbh_uac_substream *subs, usb_bool_t wait)
{
    /* ֹͣͬ���˵�*/
    if(subs->flags & (1 << SUBSTREAM_FLAG_SYNC_EP_STARTED)){
        usbh_uac_endpoint_stop(subs->sync_endpoint);
        subs->flags &= ~(1 << SUBSTREAM_FLAG_SYNC_EP_STARTED);
    }
    /* ֹͣ���ݶ˵�*/
    if(subs->flags & (1 << SUBSTREAM_FLAG_DATA_EP_STARTED)){
        usbh_uac_endpoint_stop(subs->data_endpoint);
        subs->flags &= ~(1 << SUBSTREAM_FLAG_DATA_EP_STARTED);
    }
    /* �ȴ�ֹͣ����*/
    if (wait) {
//        usbh_uac_endpoint_sync_pending_stop(subs->sync_endpoint);
//        usbh_uac_endpoint_sync_pending_stop(subs->data_endpoint);
    }
}

/** ���ö˵����*/
usb_err_t uac_configure_endpoint(struct usbh_uac_substream *subs)
{
    usb_err_t ret;

    /* format changed */
    //todo stop_endpoints(subs, true);
    /* ���ö˵����*/
    ret = usbh_uac_endpoint_set_params(subs->data_endpoint,
                                       subs->pcm_format_size,
                                       subs->channels,
                                       subs->period_bytes,
                                       subs->period_frames,
                                       subs->buffer_periods,
                                       subs->cur_rate,
                                       subs->cur_audiofmt,
                                       subs->sync_endpoint);
    if (ret != USB_OK){
        return ret;
    }
    /* �����ͬ���˵㣬����ͬ���˵�*/
    if (subs->sync_endpoint){
        //todo ret = configure_sync_endpoint(subs);
    }
    return ret;
}

/**
 * ����ͬ���˵�
 *
 * @param subs USB��Ƶ��������
 * @param fmt  ��Ƶ��ʽ
 * @param alts USB�ӿڽṹ��
 *
 * @return �ɹ�����USB_OK
 */
usb_err_t set_sync_endpoint(struct usbh_uac_substream *subs,
                                   struct audioformat *fmt,
                                   struct usbh_interface *alts)
{
    int is_playback = subs->direction == USBH_SND_PCM_STREAM_PLAYBACK;
    uint32_t attr;
    //bool implicit_fb;
    usb_err_t ret;

    /* ������Ҫһ��ͬ���ܵ����첽���������Ӧ����ģʽ
     * ���˵���������һЩ�豸�������������⣬���ֻ��һ���˵㣬����������Ӧ���ģʽ��ͬ������*/
    attr = fmt->ep_attr & USB_SS_EP_SYNCTYPE;

    ret = set_sync_ep_implicit_fb_quirk(subs, attr);
    if (ret != USB_OK){
        return ret;
    }
    if (alts->p_desc->bNumEndpoints < 2){
        return USB_OK;
    }
    if ((is_playback && (attr != USB_SS_EP_SYNC_ASYNC)) ||
        (!is_playback && (attr != USB_SS_EP_SYNC_ADAPTIVE))){
        return USB_OK;
    }
//    /* check sync-pipe endpoint */
//    /* ... and check descriptor size before accessing bSynchAddress
//       because there is a version of the SB Audigy 2 NX firmware lacking
//       the audio fields in the endpoint descriptors */
//    if ((get_endpoint(alts, 1)->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) != USB_ENDPOINT_XFER_ISOC ||
//        (get_endpoint(alts, 1)->bLength >= USB_DT_ENDPOINT_AUDIO_SIZE &&
//         get_endpoint(alts, 1)->bSynchAddress != 0)) {
//        dev_err(&dev->dev,
//            "%d:%d : invalid sync pipe. bmAttributes %02x, bLength %d, bSynchAddress %02x\n",
//               fmt->iface, fmt->altsetting,
//               get_endpoint(alts, 1)->bmAttributes,
//               get_endpoint(alts, 1)->bLength,
//               get_endpoint(alts, 1)->bSynchAddress);
//        return -EINVAL;
//    }
//    ep = get_endpoint(alts, 1)->bEndpointAddress;
//    if (get_endpoint(alts, 0)->bLength >= USB_DT_ENDPOINT_AUDIO_SIZE &&
//        ((is_playback && ep != (unsigned int)(get_endpoint(alts, 0)->bSynchAddress | USB_DIR_IN)) ||
//         (!is_playback && ep != (unsigned int)(get_endpoint(alts, 0)->bSynchAddress & ~USB_DIR_IN)))) {
//        dev_err(&dev->dev,
//            "%d:%d : invalid sync pipe. is_playback %d, ep %02x, bSynchAddress %02x\n",
//               fmt->iface, fmt->altsetting,
//               is_playback, ep, get_endpoint(alts, 0)->bSynchAddress);
//        return -EINVAL;
//    }
//
//    implicit_fb = (get_endpoint(alts, 1)->bmAttributes & USB_ENDPOINT_USAGE_MASK)
//            == USB_ENDPOINT_USAGE_IMPLICIT_FB;
//
//    subs->sync_endpoint = snd_usb_add_endpoint(subs->stream->chip,
//                           alts, ep, !subs->direction,
//                           implicit_fb ?
//                            SND_USB_ENDPOINT_TYPE_DATA :
//                            SND_USB_ENDPOINT_TYPE_SYNC);
//    if (!subs->sync_endpoint)
//        return -EINVAL;
//
//    subs->data_endpoint->sync_master = subs->sync_endpoint;

    return USB_OK;
}

/** �����˵�*/
usb_err_t uac_start_endpoints(struct usbh_uac_substream *subs,
                                     usb_bool_t can_sleep){
    usb_err_t ret;

    if (subs->data_endpoint == NULL){
        return -USB_EINVAL;
    }
    /* �Ƿ��Ѿ��������ݶ˵�*/
    if(!(subs->flags & (1 << SUBSTREAM_FLAG_DATA_EP_STARTED))){

        subs->flags |= (1 << SUBSTREAM_FLAG_DATA_EP_STARTED);
        struct usbh_uac_endpoint *ep = subs->data_endpoint;

        __UAC_PCM_TRACE("Starting data EP\r\n");

        ep->data_subs = subs;
        ret = usbh_uac_endpoint_start(ep, can_sleep);
        if (ret != USB_OK) {
            subs->flags &= ~(1 << SUBSTREAM_FLAG_DATA_EP_STARTED);
            return ret;
        }
    }
    if(subs->sync_endpoint){

    }
    return USB_OK;
}




