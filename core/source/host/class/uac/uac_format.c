#include "host/class/uac/uac_format.h"
#include "host/class/uac/uac_clock.h"
#include "host/class/uac/uac_quirks.h"
#include "host/class/uac/uac_endpoint.h"
#include <string.h>
#include <stdio.h>

extern uint32_t uac_pcm_rate_to_rate_bit(uint32_t rate);
extern usb_err_t set_sync_endpoint(struct usbh_uac_substream *subs,
                                   struct audioformat *fmt,
                                   struct usbh_interface *alts);

struct pcm_format_data pcm_formats[SNDRV_PCM_FORMAT_LAST + 1] = {
    [SNDRV_PCM_FORMAT_S8] = {
        .width = 8, .phys = 8, .le = -1, .signd = 1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_U8] = {
        .width = 8, .phys = 8, .le = -1, .signd = 0,
        .silence = { 0x80 },
    },
    [SNDRV_PCM_FORMAT_S16_LE] = {
        .width = 16, .phys = 16, .le = 1, .signd = 1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_S16_BE] = {
        .width = 16, .phys = 16, .le = 0, .signd = 1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_U16_LE] = {
        .width = 16, .phys = 16, .le = 1, .signd = 0,
        .silence = { 0x00, 0x80 },
    },
    [SNDRV_PCM_FORMAT_U16_BE] = {
        .width = 16, .phys = 16, .le = 0, .signd = 0,
        .silence = { 0x80, 0x00 },
    },
    [SNDRV_PCM_FORMAT_S24_LE] = {
        .width = 24, .phys = 32, .le = 1, .signd = 1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_S24_BE] = {
        .width = 24, .phys = 32, .le = 0, .signd = 1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_U24_LE] = {
        .width = 24, .phys = 32, .le = 1, .signd = 0,
        .silence = { 0x00, 0x00, 0x80 },
    },
    [SNDRV_PCM_FORMAT_U24_BE] = {
        .width = 24, .phys = 32, .le = 0, .signd = 0,
        .silence = { 0x00, 0x80, 0x00, 0x00 },
    },
    [SNDRV_PCM_FORMAT_S32_LE] = {
        .width = 32, .phys = 32, .le = 1, .signd = 1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_S32_BE] = {
        .width = 32, .phys = 32, .le = 0, .signd = 1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_U32_LE] = {
        .width = 32, .phys = 32, .le = 1, .signd = 0,
        .silence = { 0x00, 0x00, 0x00, 0x80 },
    },
    [SNDRV_PCM_FORMAT_U32_BE] = {
        .width = 32, .phys = 32, .le = 0, .signd = 0,
        .silence = { 0x80, 0x00, 0x00, 0x00 },
    },
    [SNDRV_PCM_FORMAT_FLOAT_LE] = {
        .width = 32, .phys = 32, .le = 1, .signd = -1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_FLOAT_BE] = {
        .width = 32, .phys = 32, .le = 0, .signd = -1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_FLOAT64_LE] = {
        .width = 64, .phys = 64, .le = 1, .signd = -1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_FLOAT64_BE] = {
        .width = 64, .phys = 64, .le = 0, .signd = -1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE] = {
        .width = 32, .phys = 32, .le = 1, .signd = -1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE] = {
        .width = 32, .phys = 32, .le = 0, .signd = -1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_MU_LAW] = {
        .width = 8, .phys = 8, .le = -1, .signd = -1,
        .silence = { 0x7f },
    },
    [SNDRV_PCM_FORMAT_A_LAW] = {
        .width = 8, .phys = 8, .le = -1, .signd = -1,
        .silence = { 0x55 },
    },
    [SNDRV_PCM_FORMAT_IMA_ADPCM] = {
        .width = 4, .phys = 4, .le = -1, .signd = -1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_G723_24] = {
        .width = 3, .phys = 3, .le = -1, .signd = -1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_G723_40] = {
        .width = 5, .phys = 5, .le = -1, .signd = -1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_DSD_U8] = {
        .width = 8, .phys = 8, .le = 1, .signd = 0,
        .silence = { 0x69 },
    },
    [SNDRV_PCM_FORMAT_DSD_U16_LE] = {
        .width = 16, .phys = 16, .le = 1, .signd = 0,
        .silence = { 0x69, 0x69 },
    },
    [SNDRV_PCM_FORMAT_DSD_U32_LE] = {
        .width = 32, .phys = 32, .le = 1, .signd = 0,
        .silence = { 0x69, 0x69, 0x69, 0x69 },
    },
    [SNDRV_PCM_FORMAT_DSD_U16_BE] = {
        .width = 16, .phys = 16, .le = 0, .signd = 0,
        .silence = { 0x69, 0x69 },
    },
    [SNDRV_PCM_FORMAT_DSD_U32_BE] = {
        .width = 32, .phys = 32, .le = 0, .signd = 0,
        .silence = { 0x69, 0x69, 0x69, 0x69 },
    },
    /* FIXME: the following three formats are not defined properly yet */
    [SNDRV_PCM_FORMAT_MPEG] = {
        .le = -1, .signd = -1,
    },
    [SNDRV_PCM_FORMAT_GSM] = {
        .le = -1, .signd = -1,
    },
    [SNDRV_PCM_FORMAT_SPECIAL] = {
        .le = -1, .signd = -1,
    },
    [SNDRV_PCM_FORMAT_S24_3LE] = {
        .width = 24, .phys = 24, .le = 1, .signd = 1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_S24_3BE] = {
        .width = 24, .phys = 24, .le = 0, .signd = 1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_U24_3LE] = {
        .width = 24, .phys = 24, .le = 1, .signd = 0,
        .silence = { 0x00, 0x00, 0x80 },
    },
    [SNDRV_PCM_FORMAT_U24_3BE] = {
        .width = 24, .phys = 24, .le = 0, .signd = 0,
        .silence = { 0x80, 0x00, 0x00 },
    },
    [SNDRV_PCM_FORMAT_S20_3LE] = {
        .width = 20, .phys = 24, .le = 1, .signd = 1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_S20_3BE] = {
        .width = 20, .phys = 24, .le = 0, .signd = 1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_U20_3LE] = {
        .width = 20, .phys = 24, .le = 1, .signd = 0,
        .silence = { 0x00, 0x00, 0x08 },
    },
    [SNDRV_PCM_FORMAT_U20_3BE] = {
        .width = 20, .phys = 24, .le = 0, .signd = 0,
        .silence = { 0x08, 0x00, 0x00 },
    },
    [SNDRV_PCM_FORMAT_S18_3LE] = {
        .width = 18, .phys = 24, .le = 1, .signd = 1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_S18_3BE] = {
        .width = 18, .phys = 24, .le = 0, .signd = 1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_U18_3LE] = {
        .width = 18, .phys = 24, .le = 1, .signd = 0,
        .silence = { 0x00, 0x00, 0x02 },
    },
    [SNDRV_PCM_FORMAT_U18_3BE] = {
        .width = 18, .phys = 24, .le = 0, .signd = 0,
        .silence = { 0x02, 0x00, 0x00 },
    },
    [SNDRV_PCM_FORMAT_G723_24_1B] = {
        .width = 3, .phys = 8, .le = -1, .signd = -1,
        .silence = {},
    },
    [SNDRV_PCM_FORMAT_G723_40_1B] = {
        .width = 5, .phys = 8, .le = -1, .signd = -1,
        .silence = {},
    },
};

/** 获取PCM格式宽度*/
int uac_get_pcm_format_size_bit(uint32_t format_size){
    /* 获取格式位宽*/
    if(format_size == 8){
        return SNDRV_PCM_FORMAT_S8;
    }
    else if(format_size == 16){
        return SNDRV_PCM_FORMAT_S16_LE;
    }
    else if(format_size == 24){
        return SNDRV_PCM_FORMAT_S24_LE;
    }
    else if(format_size == 32){
        return SNDRV_PCM_FORMAT_S32_LE;
    }
    return -1;
}

/** 获取格式物理位宽*/
usb_err_t uac_pcm_format_physical_width(uac_pcm_format_t format)
{
    int val;
    if ((format < 0) || (format > SNDRV_PCM_FORMAT_LAST)){
        return -USB_EINVAL;
    }
    if ((val = pcm_formats[format].phys) == 0){
        return -USB_EINVAL;
    }
    return val;
}

/** 寻找一个匹配的音频格式*/
struct audioformat *find_format(struct usbh_uac_substream *subs)
{
    struct audioformat *fp;
    struct audioformat *found = NULL;
    int cur_attr = 0, attr;

    usb_list_for_each_entry(fp, &subs->fmt_list, struct audioformat, list) {
        if (!(fp->formats & pcm_format_to_bits(subs->pcm_format_size))){
            continue;
        }
        if (fp->channels != subs->channels){
            continue;
        }
        if (subs->cur_rate < fp->rate_min ||
            subs->cur_rate > fp->rate_max){
            continue;
        }
        if (! (fp->rates & SNDRV_PCM_RATE_CONTINUOUS)) {
            uint32_t i;
            for (i = 0; i < fp->nr_rates; i++){
                if (fp->rate_table[i] == subs->cur_rate){
                    break;
                }
            }
            if (i >= fp->nr_rates){
                continue;
            }
        }
        attr = fp->ep_attr & USB_SS_EP_SYNCTYPE;
        if (! found) {
            found = fp;
            cur_attr = attr;
            continue;
        }
        /* avoid async out and adaptive in if the other method
         * supports the same format.
         * this is a workaround for the case like
         * M-audio audiophile USB.*/
        if (attr != cur_attr) {
            if ((attr == USB_SS_EP_SYNC_ASYNC &&
                 subs->direction == USBH_SND_PCM_STREAM_PLAYBACK) ||
                (attr == USB_SS_EP_SYNC_ADAPTIVE &&
                 subs->direction == USBH_SND_PCM_STREAM_CAPTURE)){
                continue;
            }
            if ((cur_attr == USB_SS_EP_SYNC_ASYNC &&
                 subs->direction == USBH_SND_PCM_STREAM_PLAYBACK) ||
                (cur_attr == USB_SS_EP_SYNC_ADAPTIVE &&
                 subs->direction == USBH_SND_PCM_STREAM_CAPTURE)) {
                found = fp;
                cur_attr = attr;
                continue;
            }
        }
        /* 寻找有最大的包大小的格式*/
        if (fp->maxpacksize > found->maxpacksize) {
            found = fp;
            cur_attr = attr;
        }
    }
    return found;
}

/** 寻找匹配的格式和设置接口*/
usb_err_t set_format(struct usbh_uac_substream *subs, struct audioformat *fmt)
{
    struct usbh_function  *p_fun = NULL;
    struct usbh_interface *alts  = NULL;
    usb_err_t ret;

    /* 获取特定的功能接口*/
    p_fun = usbh_ifnum_to_func(subs->p_fun->p_udev, fmt->iface);
    if (p_fun == NULL){
        return -USB_EINVAL;
    }
    /* 获取特定的备用设备接口*/
    alts = usbh_ifnum_to_if(subs->p_fun->p_udev, fmt->iface, fmt->altsetting);
    if(alts == NULL){
        return -USB_EINVAL;
    }
    /* 格式已经设置好了，直接返回*/
    if (fmt == subs->cur_audiofmt){
        return USB_OK;
    }
    /* 关闭旧的接口*/
    if ((subs->interface >= 0) && (subs->interface != fmt->iface)) {
        ret = usbh_set_interface(subs->p_fun, subs->interface, 0);
        if (ret != USB_OK) {
            __UAC_PCM_TRACE("%d:%d: return to setting 0 failed (%d)\r\n",
                fmt->iface, fmt->altsetting, ret);
            return -USB_EIO;
        }
        subs->interface = -1;
        subs->altsetting = 0;
    }
    /* 设置接口*/
    if ((subs->interface != fmt->iface) ||
        (subs->altsetting != fmt->altsetting)) {
        ret = usbh_uac_select_mode_quirk(subs, fmt);
        if (ret != USB_OK){
            return -USB_EIO;
        }

        ret = usbh_set_interface(subs->p_fun, fmt->iface, fmt->altsetting);
        if (ret != USB_OK) {
            __UAC_PCM_TRACE("%d:%d: usb_set_interface failed (%d)\r\n",
                fmt->iface, fmt->altsetting, ret);
            return -USB_EIO;
        }
        __UAC_PCM_TRACE("setting usb interface %d:%d\r\n", fmt->iface, fmt->altsetting);
        subs->interface = fmt->iface;
        subs->altsetting = fmt->altsetting;

        usbh_uac_set_interface_quirk(subs->p_fun->p_udev);
    }
    /* 添加端点*/
    subs->data_endpoint = usbh_uac_add_endpoint(subs->stream->chip,
                           alts, fmt->endpoint, subs->direction,
                           SND_USB_ENDPOINT_TYPE_DATA);

    if (subs->data_endpoint == NULL){
        return -USB_EINVAL;
    }
    /* 设置同步端点*/
    ret = set_sync_endpoint(subs, fmt, alts);
    if (ret != USB_OK){
        return ret;
    }
    /* 初始化音量控制*/
    ret = usbh_uac_init_pitch(subs->stream->chip, fmt->iface, alts, fmt);
    if (ret != USB_OK){
        return ret;
    }
    subs->cur_audiofmt = fmt;

    usbh_uac_set_format_quirk(subs, fmt);

    return USB_OK;
}

/**
 * 分心音频合适类型I描述符
 *
 * @param chip   USB音频结构体
 * @param fp     音频格式记录
 * @param format 格式tag(wFormatTag)
 * @param _fmt   格式类型描述符
 *
 * @return 成功返回相应的PCM格式
 */
static uint64_t parse_audio_format_i_type(struct usbh_audio *chip,
                                          struct audioformat *fp,
                                          uint32_t format, void *_fmt)
{
    int sample_width, sample_bytes;
    uint32_t pcm_formats = 0;

    switch (fp->protocol) {
    case UAC_VERSION_1:
    default: {
        struct uac_format_type_i_discrete_descriptor *fmt = _fmt;
        sample_width = fmt->bBitResolution;
        sample_bytes = fmt->bSubframeSize;
        format = 1 << format;
        break;
    }

    case UAC_VERSION_2: {
        struct uac_format_type_i_ext_descriptor *fmt = _fmt;
        sample_width = fmt->bBitResolution;
        sample_bytes = fmt->bSubslotSize;

        if (format & UAC2_FORMAT_TYPE_I_RAW_DATA){
            pcm_formats |= SNDRV_PCM_FMTBIT_SPECIAL;
        }
        format <<= 1;
        break;
    }
    }

    if ((pcm_formats == 0) &&
        (format == 0 || format == (1 << UAC_FORMAT_TYPE_I_UNDEFINED))) {
        /* some devices don't define this correctly... */
        __UAC_FORMAT_TRACE("%u:%d : format type 0 is detected, processed as PCM\r\n",
                            fp->iface, fp->altsetting);
        format = 1 << UAC_FORMAT_TYPE_I_PCM;
    }
    if (format & (1 << UAC_FORMAT_TYPE_I_PCM)) {
        if (((chip->usb_id == UAC_ID(0x0582, 0x0016)) ||
             /* Edirol SD-90 */
             (chip->usb_id == UAC_ID(0x0582, 0x000c))) &&
             /* Roland SC-D70 */
            sample_width == 24 && sample_bytes == 2)
            sample_bytes = 3;
        else if (sample_width > sample_bytes * 8) {
            __UAC_FORMAT_TRACE("%u:%d : sample bitwidth %d in over sample bytes %d\r\n",
                                fp->iface, fp->altsetting,
                                sample_width, sample_bytes);
        }
        /* 检查格式类型字节大小*/
        switch (sample_bytes) {
        case 1:
            pcm_formats |= SNDRV_PCM_FMTBIT_S8;
            break;
        case 2:
            if (usbh_uac_is_big_endian_format(chip, fp))
                pcm_formats |= SNDRV_PCM_FMTBIT_S16_BE; /* grrr, big endian!! */
            else
                pcm_formats |= SNDRV_PCM_FMTBIT_S16_LE;
            break;
        case 3:
        	//todo
            break;
        case 4:
            pcm_formats |= SNDRV_PCM_FMTBIT_S32_LE;
            break;
        default:
            __UAC_FORMAT_TRACE("%u:%d : unsupported sample bitwidth %d in %d bytes\r\n",
                                fp->iface, fp->altsetting,
                                sample_width, sample_bytes);
            break;
        }
    }
    if (format & (1 << UAC_FORMAT_TYPE_I_PCM8)) {
        /* Dallas DS4201 workaround: it advertises U8 format, but really
           supports S8. */
        if (chip->usb_id == UAC_ID(0x04fa, 0x4201))
            pcm_formats |= SNDRV_PCM_FMTBIT_S8;
        else
            pcm_formats |= SNDRV_PCM_FMTBIT_U8;
    }
    if (format & (1 << UAC_FORMAT_TYPE_I_IEEE_FLOAT)) {
        pcm_formats |= SNDRV_PCM_FMTBIT_FLOAT_LE;
    }
    if (format & (1 << UAC_FORMAT_TYPE_I_ALAW)) {
        pcm_formats |= SNDRV_PCM_FMTBIT_A_LAW;
    }
    if (format & (1 << UAC_FORMAT_TYPE_I_MULAW)) {
        pcm_formats |= SNDRV_PCM_FMTBIT_MU_LAW;
    }
    if (format & ~0x3f) {
        __UAC_FORMAT_TRACE("%u:%d : unsupported format bits %#x\r\n",
                            fp->iface, fp->altsetting, format);
    }

    pcm_formats |= usbh_uac_interface_dsd_format_quirks(chip, fp, sample_bytes);

    return pcm_formats;
}


/**
 * 分析格式描述符和存储audioformat表上可能的采样率(音频类版本V1)
 *
 * @param chip   USB音频结构体
 * @param fp     音频格式记录
 * @param fmt    格式描述符
 * @param offset 描述符指向采样率类型的偏移(类型I和III是7，类型II是8)
 *
 * @return 成功返回USB_OK
 */
static usb_err_t parse_audio_format_rates_v1(struct usbh_audio *chip, struct audioformat *fp,
                                             unsigned char *fmt, int offset)
{
    int nr_rates = fmt[offset];

    if (fmt[0] < offset + 1 + 3 * (nr_rates ? nr_rates : 2)) {
        __UAC_FORMAT_TRACE("%u:%d : invalid UAC_FORMAT_TYPE desc\r\n",
            fp->iface, fp->altsetting);
        return -USB_EINVAL;
    }

    if (nr_rates) {
        /* 建立采样率表和位图标志*/
        int r, idx;

        fp->rate_table = usb_mem_alloc(sizeof(int) * nr_rates);
        if (fp->rate_table == NULL) {
            __UAC_FORMAT_TRACE("cannot malloc\r\n");
            return -USB_ENOMEM;
        }
        memset(fp->rate_table, 0, sizeof(int) * nr_rates);

        fp->nr_rates = 0;
        fp->rate_min = fp->rate_max = 0;
        for (r = 0, idx = offset + 1; r < nr_rates; r++, idx += 3) {
            uint32_t rate = combine_triple(&fmt[idx]);
            if (rate == 0){
                continue;
            }
            /* C-Media CM6501 mislabels its 96 kHz altsetting */
            /* Terratec Aureon 7.1 USB C-Media 6206, too */
            if (rate == 48000 && nr_rates == 1 &&
                (chip->usb_id == UAC_ID(0x0d8c, 0x0201) ||
                 chip->usb_id == UAC_ID(0x0d8c, 0x0102) ||
                 chip->usb_id == UAC_ID(0x0ccd, 0x00b1)) &&
                fp->altsetting == 5 && fp->maxpacksize == 392){
                rate = 96000;
            }
            /* Creative VF0420/VF0470 Live Cams report 16 kHz instead of 8kHz */
            if (rate == 16000 && (chip->usb_id == UAC_ID(0x041e, 0x4064) ||
                 chip->usb_id == UAC_ID(0x041e, 0x4068))){
                rate = 8000;
            }

            fp->rate_table[fp->nr_rates] = rate;
            if ((fp->rate_min == 0) || (rate < fp->rate_min)){
                fp->rate_min = rate;
            }
            if ((fp->rate_max == 0) || (rate > fp->rate_max)){
                fp->rate_max = rate;
            }
            fp->rates |= uac_pcm_rate_to_rate_bit(rate);
            fp->nr_rates++;
        }
        if (!fp->nr_rates) {
            __UAC_FORMAT_TRACE("All rates were zero. Skipping format!\r\n");
            return -USB_EINVAL;
        }
    } else {
        /* 连续的采样率*/
        fp->rates = SNDRV_PCM_RATE_CONTINUOUS;
        fp->rate_min = combine_triple(&fmt[offset + 1]);
        fp->rate_max = combine_triple(&fmt[offset + 4]);
    }
    return USB_OK;
}

/** 分析格式类型I和III描述符*/
static usb_err_t parse_audio_format_i(struct usbh_audio *chip,
                                      struct audioformat *fp, uint32_t format,
                                      struct uac_format_type_i_continuous_descriptor *fmt)
{
	usb_err_t ret;
    /* 格式类型III*/
    if (fmt->bFormatType == UAC_FORMAT_TYPE_III) {
    	//todo
    } else {
        fp->formats = parse_audio_format_i_type(chip, fp, format, fmt);
        if (!fp->formats){
            return -USB_EINVAL;
        }
    }

    /* 获取采样率*/
    switch (fp->protocol) {
    default:
    case UAC_VERSION_1:
        fp->channels = fmt->bNrChannels;
        ret = parse_audio_format_rates_v1(chip, fp, (unsigned char *) fmt, 7);
        break;
    case UAC_VERSION_2:
    	//todo
        break;
    }

    if (fp->channels < 1) {
        __UAC_FORMAT_TRACE("%u:%d : invalid channels %d\r\n",
                            fp->iface, fp->altsetting, fp->channels);
        return -USB_EINVAL;
    }

    return ret;
}

/** UAC 分析音频格式*/
usb_err_t usbh_uac_parse_audio_format(struct usbh_audio *chip,
                   struct audioformat *fp, unsigned int format,
                   struct uac_format_type_i_continuous_descriptor *fmt,
                   int stream)
{
    int err;

    switch (fmt->bFormatType) {
    case UAC_FORMAT_TYPE_I:
    case UAC_FORMAT_TYPE_III:
        err = parse_audio_format_i(chip, fp, format, fmt);
        break;
    case UAC_FORMAT_TYPE_II:
        //todo
        break;
    default:
        __UAC_FORMAT_TRACE("%u:%d : format type %d is not supported yet\r\n",
                             fp->iface, fp->altsetting,
                             fmt->bFormatType);
        return -USB_EPERM;
    }
    fp->fmt_type = fmt->bFormatType;
    if (err < 0){
        return err;
    }
#if 1
    /* FIXME: temporary hack for extigy/audigy 2 nx/zs */
    /* extigy apparently supports sample rates other than 48k
     * but not in ordinary way.  so we enable only 48k atm.*/
    if (chip->usb_id == UAC_ID(0x041e, 0x3000) ||
        chip->usb_id == UAC_ID(0x041e, 0x3020) ||
        chip->usb_id == UAC_ID(0x041e, 0x3061)) {
        if (fmt->bFormatType == UAC_FORMAT_TYPE_I &&
            fp->rates != SNDRV_PCM_RATE_48000 &&
            fp->rates != SNDRV_PCM_RATE_96000){
            return -USB_EPERM;
        }
    }
#endif
    return USB_OK;
}
