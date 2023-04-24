#include "host/class/uac/usbh_uac_driver.h"
#include "host/class/uac/uac.h"
#include "host/class/uac/uac_quirks.h"
#include "host/class/uac/uac_format.h"
#include "host/class/uac/uac_pcm.h"
#include "host/class/uac/uac_clock.h"
#include "usb_config.h"
#include <string.h>
#include <stdio.h>

/** 通过给定的终端ID寻找一个输入终端描述符(UAC1或UAC2)*/
static void *
uac_find_input_terminal_descriptor(struct usbh_interface *ctrl_iface,
                           int terminal_id)
{
    struct uac2_input_terminal_descriptor *term = NULL;

    while ((term = usbh_uac_find_csint_desc(ctrl_iface->extra,
                           ctrl_iface->extralen,
                           term, UAC_INPUT_TERMINAL))) {
        if (term->bTerminalID == terminal_id)
            return term;
    }

    return NULL;
}

/** 获取端点的轮询间隔*/
static uint8_t uac_parse_datainterval(struct usbh_audio     *p_audio,
                                      struct usbh_interface *p_intf)
{
    switch (uac_get_speed(p_audio->p_fun)) {
    case USB_SPEED_HIGH:
    case USB_SPEED_WIRELESS:
    case USB_SPEED_SUPER:
        if ((USBH_GET_EP_INTVAL(&p_intf->eps[0]) >= 1) &&
                (USBH_GET_EP_INTVAL(&p_intf->eps[0]) <= 4)){
            return USBH_GET_EP_INTVAL(&p_intf->eps[0]) - 1;
        }
        break;
    default:
        break;
    }
    return 0;
}

/** 释放一个子数据流*/
static void free_substream(struct usbh_uac_substream *subs)
{
    struct audioformat *fp, *n;
    /* 没有初始化*/
    if (!subs->num_formats){
        return;
    }
    usb_list_for_each_entry_safe(fp, n, &subs->fmt_list, struct audioformat, list) {
        usb_mem_free(fp->rate_table);
        usb_mem_free(fp);
    }
    usb_mem_free(subs->rate_list.list);
}

/** 释放USB流实体*/
void usbh_uac_audio_stream_free(struct usbh_uac_stream *stream)
{
    /* 释放子数据流*/
    free_substream(&stream->substream[0]);
    free_substream(&stream->substream[1]);
    /* 释放PCM*/
    if(stream->pcm != NULL){
        usbh_uac_pcm_free(stream->pcm);
        stream->pcm = NULL;
    }

    usb_list_del(&stream->list);
    usb_mem_free(stream);
}

/**
 * 初始化一个子数据流实体
 *
 * @param as     USB音频设备数据流
 * @param p_fun  USB接口功能
 * @param stream 数据流方向(回放或者录音)
 * @param fp     初始化数据格式
 */
static void uac_init_substream(struct usbh_uac_stream *as,
                               struct usbh_function   *p_fun,
                               int                     stream,
                               struct audioformat     *fp)
{
    struct usbh_uac_substream *subs = &as->substream[stream];

    USB_INIT_LIST_HEAD(&subs->fmt_list);

    subs->stream = as;
    subs->direction = stream;
    subs->p_fun = p_fun;
    subs->txfr_quirk = as->chip->txfr_quirk;
    subs->speed = uac_get_speed(as->chip->p_fun);
    subs->pkt_offset_adj = 0;
    /* 设置PCM操作函数集*/
    usbh_uac_set_pcm_ops(as->pcm, stream);

    usb_list_add_tail(&fp->list, &subs->fmt_list);
    subs->formats |= fp->formats;
    subs->num_formats++;
    subs->fmt_type = fp->fmt_type;
    subs->ep_num = fp->endpoint;
    if (fp->channels > subs->channels_max){
        subs->channels_max = fp->channels;
    }
}

/** 分析USB音频设备端点属性*/
static int parse_uac_endpoint_attributes(struct usbh_audio     *chip,
                                         struct usbh_interface *p_intf,
                                         int protocol, int iface_no)
{
    /* 在这里分析UAC版本1的头部。我们先分析头部信息是没问题的，因为两个版本都一样*/
    struct uac_iso_endpoint_descriptor *csep;
    int attributes = 0;

    csep = usbh_uac_find_desc(p_intf->eps[0].extra, p_intf->eps[0].extralen,
            NULL, (USB_REQ_TYPE_CLASS | USB_DT_ENDPOINT));

    /* Creamware Noah 在第2个端点后才能找到描述符*/
    if ((csep == NULL) && (USBH_GET_INTF_EP_NUM(p_intf) >= 2)){
        csep = usbh_uac_find_desc(p_intf->eps[1].extra, p_intf->eps[1].extralen,
                NULL, (USB_REQ_TYPE_CLASS | USB_DT_ENDPOINT));
    }
    /* 如果我们不能在在第一个端点的额外的字节数据中定位端点描述符，继续在整个接口中继续寻找。
     * 一些设备在标准端点之前就有*/
    if (csep == NULL){
        csep = usbh_uac_find_desc(p_intf->extra, p_intf->extralen,
                NULL, (USB_REQ_TYPE_CLASS | USB_DT_ENDPOINT));
    }
    if ((csep == NULL) || (csep->bLength < 7) ||
        (csep->bDescriptorSubtype != UAC_EP_GENERAL)) {
        __UAC_TRACE("%u:%d : no or invalid class specific endpoint descriptor\r\n",
                   iface_no, p_intf->p_desc->bAlternateSetting);
        return 0;
    }

    if (protocol == UAC_VERSION_1) {
        attributes = csep->bmAttributes;
    } else {
        struct uac2_iso_endpoint_descriptor *csep2 =
            (struct uac2_iso_endpoint_descriptor *) csep;

        attributes = csep->bmAttributes & UAC_EP_CS_ATTR_FILL_MAX;

        /* 模拟UAC版本1设备的端点属性*/
        if (csep2->bmControls & UAC2_CONTROL_PITCH){
            attributes |= UAC_EP_CS_ATTR_PITCH_CONTROL;
        }
    }

    return attributes;
}

/**
 * 添加端点到芯片实体，如果一个流已经有这个端点，则追加。没有则创建一个新的PCM流
 *
 * @param chip   USB音频设备结构体
 * @param p_fun  特定的接口功能结构体
 * @param stream 数据流方向
 * @param fp     音频格式
 *
 * @return 成功返回USB_OK
 */
static usb_err_t uac_add_audio_stream(struct usbh_audio    *chip,
                                      struct usbh_function *p_fun,
                                      int                   stream,
                                      struct audioformat   *fp)
{
    struct usbh_uac_stream *as;
    struct usbh_uac_substream *subs;
    struct uac_pcm *pcm;
    usb_err_t ret;

    /* 检查已有的数据流*/
    usb_list_for_each_entry(as, &chip->stream_list, struct usbh_uac_stream, list) {
        /* 匹配格式类型*/
        if (as->fmt_type != fp->fmt_type){
            continue;
        }
        subs = &as->substream[stream];
        /* 相同的端点，追加格式*/
        if (subs->ep_num == fp->endpoint) {
            usb_list_add_tail(&fp->list, &subs->fmt_list);
            subs->num_formats++;
            subs->formats |= fp->formats;
            return USB_OK;
        }
    }
    /* 寻找一个没有端点的流 */
    usb_list_for_each_entry(as, &chip->stream_list, struct usbh_uac_stream, list) {
        if (as->fmt_type != fp->fmt_type){
            continue;
        }
        subs = &as->substream[stream];
        if (subs->ep_num){
            continue;
        }
        /* 创建新的PCM数据流*/
        ret = uac_pcm_new_stream(as->pcm, stream, 1);
        if (ret != USB_OK){
            return ret;
        }
        /* 初始化数据子流*/
        uac_init_substream(as, p_fun, stream, fp);
        return USB_OK;
    }

    /* 创建一个新的PCM流*/
    as = usb_mem_alloc(sizeof(struct usbh_uac_stream));
    if (as == NULL){
        return -USB_ENOMEM;
    }
    memset(as, 0, sizeof(struct usbh_uac_stream));

    as->pcm_index = chip->pcm_devs;
    as->chip = chip;
    as->fmt_type = fp->fmt_type;
    /* 创建新的PCM*/
    ret = uac_pcm_new("USB Audio", chip->pcm_devs,
                      stream == USBH_SND_PCM_STREAM_PLAYBACK ? 1 : 0,
                      stream == USBH_SND_PCM_STREAM_PLAYBACK ? 0 : 1,
                      &pcm);
    if (ret != USB_OK) {
        usb_mem_free(as);
        return ret;
    }
    as->pcm = pcm;
    pcm->private_data = as;
    //pcm->private_free = usbh_uac_audio_pcm_free;
    pcm->info_flags = 0;
    if (chip->pcm_devs > 0){
        sprintf(pcm->name, "USB Audio #%d", chip->pcm_devs);
    }
    else{
        strcpy(pcm->name, "USB Audio");
    }
    uac_init_substream(as, p_fun, stream, fp);

    usb_list_add(&as->list, &chip->stream_list);
    chip->pcm_devs++;

    return USB_OK;
}

/**
 * 分析USB音频设备数据接口
 *
 * @param p_audio  USB音频设备
 * @param iface_no 数据接口编号
 *
 * @return 成功返回USB_OK
 */
usb_err_t usbh_uac_parse_audio_interface(struct usbh_audio *p_audio, int iface_no)
{
    struct usbh_interface *p_intf, *p_intf_temp;
    struct usbh_function *p_fun = NULL;
    int num, i, altno, stream, protocol, clock = 0;
    //uint32_t chconfig;
    uint32_t format = 0, num_channels = 0;
    struct audioformat *fp = NULL;
    struct uac_format_type_i_continuous_descriptor *fmt;
    usb_err_t err;

    p_fun = usbh_ifnum_to_func(p_audio->p_fun->p_udev, iface_no);
    if(p_fun == NULL){
        return -USB_EINVAL;
    }
    /* 获取接口备用设置数量*/
    num = p_fun->nitfs;
    /* Dallas DS4201解决方法：表示5个备用接口设置， 但最后缺少等时管道，
     * 并且不能产生任何声音。*/
    if (p_audio->usb_id == UAC_ID(0x04fa, 0x4201)){
        num = 4;
    }

    i = 0;
    /* 遍历所有功能*/
    usb_list_for_each_entry_safe(p_intf,
                                 p_intf_temp,
                                 &p_fun->itfs,
                                 struct usbh_interface,
                                 node){
        if(i < num){
            protocol = p_intf->p_desc->bInterfaceProtocol;
            /* 跳过不可用的*/
            if (((p_intf->p_desc->bInterfaceClass != USB_CLASS_AUDIO ||
                    (p_intf->p_desc->bInterfaceSubClass != USB_SUBCLASS_AUDIOSTREAMING &&
                            p_intf->p_desc->bInterfaceSubClass != USB_SUBCLASS_VENDOR_SPEC)) &&
                    p_intf->p_desc->bInterfaceClass != USB_CLASS_VENDOR_SPEC) ||
                    p_intf->p_desc->bNumEndpoints < 1 ||
                    USBH_GET_EP_MPS(&p_intf->eps[0]) == 0){
                i++;
                continue;
            }

            /* 必须是等时端点*/
            if (!USBH_GET_EP_ISO(&p_intf->eps[0])){
                i++;
                continue;
            }

            /* 检查端点方向*/
            stream = USBH_GET_EP_DIR(&p_intf->eps[0]) ?
                    USBH_SND_PCM_STREAM_CAPTURE : USBH_SND_PCM_STREAM_PLAYBACK;
            /* 获取备用设置编号*/
            altno = p_intf->p_desc->bAlternateSetting;

            if (usbh_uac_apply_interface_quirk(p_audio, iface_no, altno)){
                i++;
                continue;
            }

            /* Roland音频流接口协议被标记为0/1/2, 但UAC 1 兼容。*/
            if ((UAC_ID_VENDOR(p_audio->usb_id) == 0x0582) &&
                    (p_intf->p_desc->bInterfaceClass == USB_CLASS_VENDOR_SPEC) &&
                    (protocol <= 2)){
                protocol = UAC_VERSION_1;
            }

            /* 获取音频格式*/
            switch (protocol) {
            default:
                __UAC_TRACE("%u:%d: unknown interface protocol %#02x, assuming v1\r\n",
                        iface_no, altno, protocol);
                protocol = UAC_VERSION_1;
            case UAC_VERSION_1: {
                struct uac1_as_header_descriptor *as =
                        usbh_uac_find_csint_desc(p_intf->extra, p_intf->extralen, NULL, UAC_AS_GENERAL);
                struct uac_input_terminal_descriptor *iterm;

                if (!as) {
                    __UAC_TRACE("%u:%d : UAC_AS_GENERAL descriptor not found\r\n",
                            iface_no, altno);
                    continue;
                }

                if (as->bLength < sizeof(*as)) {
                    __UAC_TRACE("%u:%d : invalid UAC_AS_GENERAL desc\r\n",
                            iface_no, altno);
                    continue;
                }
                /* 记录下格式值*/
                format = USB_CPU_TO_LE16(as->wFormatTag);
                /* 寻找输入终端描述符*/
                iterm = uac_find_input_terminal_descriptor(p_audio->ctrl_intf,
                                       as->bTerminalLink);
                if (iterm) {
                    num_channels = iterm->bNrChannels;
                    //chconfig = USB_CPU_TO_LE16(iterm->wChannelConfig);
                }

                break;
            }
            case UAC_VERSION_2: {
                //todo
                break;
            }
            }
            /* 获取格式类型*/
            fmt = usbh_uac_find_csint_desc(p_intf->extra, p_intf->extralen, NULL, UAC_FORMAT_TYPE);
            if (fmt == NULL) {
                __UAC_TRACE("%u:%d : no UAC_FORMAT_TYPE desc\r\n",iface_no, altno);
                i++;
                continue;
            }
            /* 检查描述符合法性*/
            if (((protocol == UAC_VERSION_1) && (fmt->bLength < 8)) ||
                    ((protocol == UAC_VERSION_2) && (fmt->bLength < 6))) {
                __UAC_TRACE("%u:%d : invalid UAC_FORMAT_TYPE desc\r\n", iface_no, altno);
                i++;
                continue;
            }
            /* Blue 麦克风解决方案：最后的备用设置与前面的设置一样，只是包大小更大，但实际
             * 上是一个错误标记的双通道设置；忽略它。*/
            if ((fmt->bNrChannels == 1) && (fmt->bSubframeSize == 2) &&
                    (altno == 2) && (num == 3) && (fp != NULL) &&
                    (fp->altsetting == 1) && (fp->channels == 1) &&
                    (fp->formats == SNDRV_PCM_FMTBIT_S16_LE) &&
                    (protocol == UAC_VERSION_1) &&
                    (USBH_GET_EP_MPS(&p_intf->eps[0]) == fp->maxpacksize * 2)){
                i++;
                continue;
            }
            fp = usb_mem_alloc(sizeof(struct audioformat));
            if (fp == NULL) {
                __UAC_TRACE("cannot malloc\r\n");
                return -USB_ENOMEM;
            }
            memset(fp, 0, sizeof(struct audioformat));

            fp->iface = iface_no;
            fp->altsetting = altno;
            fp->endpoint = USBH_GET_EP_ADDR(&p_intf->eps[0]);
            fp->ep_attr = USBH_GET_EP_ATTR(&p_intf->eps[0]);
            fp->datainterval = uac_parse_datainterval(p_audio, p_intf);
            fp->protocol = protocol;
            fp->maxpacksize = USBH_GET_EP_MPS(&p_intf->eps[0]);
            fp->channels = num_channels;
            if (uac_get_speed(p_audio->p_fun) == USB_SPEED_HIGH){
                fp->maxpacksize = (((fp->maxpacksize >> 11) & 3) + 1)
                        * (fp->maxpacksize & 0x7ff);
            }
            fp->attributes = parse_uac_endpoint_attributes(p_audio, p_intf, protocol, iface_no);
            fp->clock = clock;
            /* 这里是一些属性的兼容*/
            switch (p_audio->usb_id) {
            /* AudioTrak Optoplay */
            case UAC_ID(0x0a92, 0x0053):
                /* Optoplay 设置采样率属性尽管它看起来事实上不支持它。*/
                fp->attributes &= ~UAC_EP_CS_ATTR_SAMPLE_RATE;
                break;
            /* Creative SB Audigy 2 NX */
            case UAC_ID(0x041e, 0x3020):
            /* M-Audio Audiophile USB */
            case UAC_ID(0x0763, 0x2003):
                /* 不设置采样率属性，但支持它 */
                fp->attributes |= UAC_EP_CS_ATTR_SAMPLE_RATE;
                break;
            /* M-Audio Quattro USB */
            case UAC_ID(0x0763, 0x2001):
            /* M-Audio Fast Track Pro USB */
            case UAC_ID(0x0763, 0x2012):
            /* plantronics headset */
            case UAC_ID(0x047f, 0x0ca1):
            /* Griffin iMic (note that there is an older model 77d:223) */
            case UAC_ID(0x077d, 0x07af):
                //todo
                break;
            }
            /* 进一步分析*/
            if (usbh_uac_parse_audio_format(p_audio, fp, format, fmt, stream) < 0) {
                if(fp->rate_table){
                    usb_mem_free(fp->rate_table);
                }
                usb_mem_free(fp);
                fp = NULL;
                i++;
                continue;
            }
            __UAC_TRACE("%u:%d: add audio endpoint %#x\r\n", iface_no, altno, fp->endpoint);
            /* 添加音频数据流*/
            err = uac_add_audio_stream(p_audio, p_fun, stream, fp);
            if (err < 0) {
                usb_mem_free(fp->rate_table);
                usb_mem_free(fp);
                return err;
            }
            /* 尝试设置接口*/
            usbh_set_interface(p_fun, iface_no, altno);
            /* 初始化音量控制和采样率*/
            usbh_uac_init_pitch(p_audio, iface_no, p_intf, fp);
            /* 初始化采样率*/
            usbh_uac_init_sample_rate(p_audio, NULL, iface_no, p_intf, fp, fp->rate_max);
            i++;
        }
    }
    return USB_OK;
}
