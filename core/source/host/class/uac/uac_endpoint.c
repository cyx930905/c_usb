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

/** 转换一个采样率到全速格式(fs/1000 in Q16.16)*/
static uint32_t get_usb_full_speed_rate(uint32_t rate)
{
    return ((rate << 13) + 62) / 125;
}

/** 转换一个采样率到高速格式(fs/8000 in Q16.16)*/
static uint32_t get_usb_high_speed_rate(uint32_t rate)
{
    return ((rate << 10) + 62) / 125;
}

/** 释放端点的传输请求包*/
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

/** 准备一个要提交到总线的回放USB传输请求包*/
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
            /* 如果没有数据，发送静音数据*/
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
/** 音频传输完成回调函数*/
static void uac_complete_trp(void *arg)
{
    struct usbh_trp *p_trp = (struct usbh_trp *)arg;
    struct uac_trp_ctx *ctx = p_trp->priv;
    struct usbh_uac_endpoint *ep = ctx->ep;
    //struct uac_pcm_substream *substream;

    usb_err_t ret;

    if ((p_trp->status == -USB_ENOENT) ||          /* 未链接*/
            (p_trp->status == -USB_ENODEV) ||      /* 设备移除*/
            (p_trp->status == -USB_ECONNRESET) ||  /* 未链接 */
            (p_trp->status == -USB_ESHUTDOWN) ||   /* 设备禁能*/
            (ep->chip->shutdown)){                 /* 设备移除*/
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

    /* 重新提交传输请求包*/
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
/** 音频传输完成回调函数*/
static void uac_complete_trp(void *arg)
{
    struct usbh_trp *p_trp = (struct usbh_trp *)arg;
    struct uac_trp_ctx *ctx = p_trp->priv;
    struct usbh_uac_endpoint *ep = ctx->ep;
    //struct uac_pcm_substream *substream;

    usb_err_t ret;

    if ((p_trp->status == -USB_ENOENT) ||          /* 未链接*/
            (p_trp->status == -USB_ENODEV) ||      /* 设备移除*/
            (p_trp->status == -USB_ECONNRESET) ||  /* 未链接 */
            (p_trp->status == -USB_ESHUTDOWN) ||   /* 设备禁能*/
            (ep->chip->shutdown)){                 /* 设备移除*/
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

/** 配置一个数据端点(不完整)*/
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
    /* 帧物理位宽(每一帧有多少位)*/
    int frame_bits = uac_pcm_format_physical_width(pcm_format) * channels;

    if ((pcm_format == SNDRV_PCM_FORMAT_DSD_U16_LE) && (fmt->dsd_dop)) {
        /* 当操作在DSD DOP模式时，硬件上的一个采样帧的大小不同于实际物理格式的位宽，因为我们需要
         * 额外的空间存储DOP标记*/
        frame_bits += channels << 3;
    }
    /* 设置数据间隔*/
    ep->datainterval = fmt->datainterval;
    /* 帧物理字节*/
    ep->stride = frame_bits >> 3;
    /* 设置静音数据*/
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

    /* 假设最大的频率比正常的高25%*/
    ep->freqmax = ep->freqn + (ep->freqn >> 2);
    maxsize = ((ep->freqmax + 0xffff) * (frame_bits >> 3))
                >> (16 - ep->datainterval);

    /* wMaxPacketSize 可能会减少这个最大包大小*/
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
    /* 高速设备*/
    if (uac_get_speed(ep->chip->p_fun) != USB_SPEED_FULL) {
        /* USB等时传输，高速模式，1ms=8微帧，8/数据间隔=1ms多少个数据包*/
        packs_per_ms = 8 >> ep->datainterval;
        max_packs_per_trp = UAC_MAX_PACKS_HS;
    } else {
        /* 全速/低速模式，1帧1个数据包*/
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
        /* 决定一个数据包可以多小*/
        minsize = (ep->freqn >> (16 - ep->datainterval)) *
                (frame_bits >> 3);

        if (sync_ep){
            minsize -= minsize >> 3;
        }
        minsize = max(minsize, 1u);

        /* 计算多少个数据包可以包含一整个周期*/
        max_packs_per_period = USB_DIV_ROUND_UP(period_bytes, minsize);
        /* 计算一个周期要多少个USB传输请求包*/
        trps_per_period = USB_DIV_ROUND_UP(max_packs_per_period,
                max_packs_per_trp);

        /* 每一个USB传输请求包需要多少个数据包*/
        trp_packs = USB_DIV_ROUND_UP(max_packs_per_period, trps_per_period);
        /* 限制一个USB传输请求包的音频帧的数量*/
        ep->max_trp_frames = USB_DIV_ROUND_UP(frames_per_period,
                    trps_per_period);
        /* 尝试使用足够的USB传输请求包去包含一个完成的音频缓存*/
        max_trps = min((unsigned) UAC_MAX_TRPS,
                UAC_MAX_QUEUE * packs_per_ms / trp_packs);
        ep->ntrps = min(max_trps, trps_per_period * periods_per_buffer);
    }
    /* 分配和初始化数据传输请求包*/
    for (i = 0; i < ep->ntrps; i++) {
        struct uac_trp_ctx *u = &ep->trp[i];
        u->index = i;
        u->ep = ep;
        u->packets = trp_packs;
        u->buffer_size = maxsize * u->packets;
        /* 用于传输分隔符*/
        if (fmt->fmt_type == UAC_FORMAT_TYPE_II){
            u->packets++;
        }

        u->trp = usb_mem_alloc(sizeof(struct usbh_trp));
        if (u->trp == NULL){
            goto out_of_memory;
        }
        memset(u->trp, 0, sizeof(struct usbh_trp));
        /* 申请等时帧描述符内存*/
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

/** 决定下一个包要发送的样本数量*/
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
 * 配置音频设备端点
 *
 * @param ep              要配置的端点
 * @param pcm_format      音频PCM格式
 * @param channels        音频通道数量
 * @param period_bytes    一个周期的字节数
 * @param period_frames   一个周期有多少帧
 * @param buffer_periods  一个缓存里的周期数量
 * @param rate            帧率
 * @param fmt             USB音频格式信息
 * @param sync_ep         要使用的同步端点
 *
 * @return 成功返回USB_OK
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

    /* 端点使用中*/
    if (ep->use_count != 0) {
        __UAC_EP_TRACE("Unable to change format on ep #%x: already in use\r\n",
             ep->ep_num);
        return -USB_EBUSY;
    }

    /* 如果有，释放旧的缓存*/
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
    /* 以16.16格式计算频率*/
    ep->freqm = ep->freqn;
    ep->freqshift = USB_INT_MIN;

    ep->phase = 0;

    switch (ep->type) {
    case  SND_USB_ENDPOINT_TYPE_DATA:
        /* 设置数据端点参数*/
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
 * 添加一个端点到USB音频设备
 *
 * @param chip      USB音频设备结构体
 * @param alts      USB接口
 * @param ep_num    要使用的端点号
 * @param direction PCM数据流方向(回放或者捕获)
 * @param type      音频端点类型
 *
 * @return 成功返回USB音频端点结构体
 */
struct usbh_uac_endpoint *usbh_uac_add_endpoint(struct usbh_audio *chip,
                          struct usbh_interface *alts,
                          int ep_num, int direction, int type){
    struct usbh_uac_endpoint *ep = NULL;
    int is_playback = 0;
    /* 是否是回放*/
    if(direction == USBH_SND_PCM_STREAM_PLAYBACK){
        is_playback = 1;
    }

    if (alts == NULL){
        return NULL;
    }

    usb_mutex_lock(chip->ep_mutex, 5000);

    /* 寻找端点是否已经存在*/
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
 * 启动一个USB音频设备端点
 *
 * @param ep        要启动的端点
 * @param can_sleep
 *
 * @return 成功返回USB_OK
 */
usb_err_t usbh_uac_endpoint_start(struct usbh_uac_endpoint *ep, usb_bool_t can_sleep){
    usb_err_t ret;
    unsigned int i;

    if (ep->chip->shutdown){
        return -USB_ENODEV;
    }
    /* 是否在运行中*/
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
        /* 提交USB传输请求包*/
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

/** 停止端点*/
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
 * 释放一个端点
 *
 * @param ep 音频设备端点
 */
void usbh_uac_endpoint_release(struct usbh_uac_endpoint *ep)
{
    int i;
    if(ep == NULL){
        return;
    }



    ep->retire_data_trp = NULL;
    ep->prepare_data_trp = NULL;
    /* 释放端点缓存*/
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
 * 释放一个音频端点缓存
 *
 * @param ep 音频设备端点
 */
void usbh_uac_endpoint_free(struct usbh_uac_endpoint *ep){
    if(ep == NULL){
        return;
    }
    usb_mem_free(ep);
}

/** 停止端点*/
void uac_stop_endpoints(struct usbh_uac_substream *subs, usb_bool_t wait)
{
    /* 停止同步端点*/
    if(subs->flags & (1 << SUBSTREAM_FLAG_SYNC_EP_STARTED)){
        usbh_uac_endpoint_stop(subs->sync_endpoint);
        subs->flags &= ~(1 << SUBSTREAM_FLAG_SYNC_EP_STARTED);
    }
    /* 停止数据端点*/
    if(subs->flags & (1 << SUBSTREAM_FLAG_DATA_EP_STARTED)){
        usbh_uac_endpoint_stop(subs->data_endpoint);
        subs->flags &= ~(1 << SUBSTREAM_FLAG_DATA_EP_STARTED);
    }
    /* 等待停止传输*/
    if (wait) {
//        usbh_uac_endpoint_sync_pending_stop(subs->sync_endpoint);
//        usbh_uac_endpoint_sync_pending_stop(subs->data_endpoint);
    }
}

/** 配置端点参数*/
usb_err_t uac_configure_endpoint(struct usbh_uac_substream *subs)
{
    usb_err_t ret;

    /* format changed */
    //todo stop_endpoints(subs, true);
    /* 设置端点参数*/
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
    /* 如果有同步端点，配置同步端点*/
    if (subs->sync_endpoint){
        //todo ret = configure_sync_endpoint(subs);
    }
    return ret;
}

/**
 * 设置同步端点
 *
 * @param subs USB音频子数据流
 * @param fmt  音频格式
 * @param alts USB接口结构体
 *
 * @return 成功返回USB_OK
 */
usb_err_t set_sync_endpoint(struct usbh_uac_substream *subs,
                                   struct audioformat *fmt,
                                   struct usbh_interface *alts)
{
    int is_playback = subs->direction == USBH_SND_PCM_STREAM_PLAYBACK;
    uint32_t attr;
    //bool implicit_fb;
    usb_err_t ret;

    /* 我们需要一个同步管道在异步输出或自适应输入模式
     * 检查端点数量。有一些设备的描述符有问题，如果只有一个端点，假设是自适应输出模式或同步输入*/
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

/** 启动端点*/
usb_err_t uac_start_endpoints(struct usbh_uac_substream *subs,
                                     usb_bool_t can_sleep){
    usb_err_t ret;

    if (subs->data_endpoint == NULL){
        return -USB_EINVAL;
    }
    /* 是否已经启动数据端点*/
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




