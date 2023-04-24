#include "host/class/uac/usbh_uac_driver.h"
#include "host/class/uac/uac_pcm.h"
#include "host/class/uac/uac_rate.h"
#include "host/class/uac/uac_quirks.h"

/* USB音频采样率数组*/
static uint32_t rates[] = { 5512, 8000, 11025, 16000, 22050, 32000, 44100,
                            48000, 64000, 88200, 96000, 176400, 192000 };

/* USB音频采样率硬件限制表*/
const struct uac_pcm_hw_constraint_list uac_pcm_known_rates = {
    .count = USB_NELEMENTS(rates),
    .list = rates,
};

/**
 * 转换采样率为SNDRV_PCM_RATE_xxx位
 *
 * @param rate 要转换的采样率
 *
 * @return 给定的采样率转换成的SNDRV_PCM_RATE_xxx标志，或者未知采样率为SNDRV_PCM_RATE_KNOT
 */
uint32_t uac_pcm_rate_to_rate_bit(uint32_t rate)
{
    uint32_t i;

    for (i = 0; i < uac_pcm_known_rates.count; i++){
        if (uac_pcm_known_rates.list[i] == rate){
            return 1u << i;
        }
    }
    return SNDRV_PCM_RATE_KNOT;
}

/** 设置采样率(USB音频类规范版本1)*/
static usb_err_t set_sample_rate_v1(struct usbh_audio *chip,
                                    struct usbh_uac_substream *subs, int iface,
                                    struct usbh_interface *alts,
                                    struct audioformat *fmt, int rate)
{
    struct usbh_function *p_fun = chip->p_fun;
    uint32_t ep;
    uint8_t data[3];
    int crate;
    usb_err_t ret;

    ep = alts->eps[0].p_desc->bEndpointAddress;

    /* 如果短短没有采样率控制，跳出*/
    if (!(fmt->attributes & UAC_EP_CS_ATTR_SAMPLE_RATE)){
        return USB_OK;
    }
    data[0] = rate;
    data[1] = rate >> 8;
    data[2] = rate >> 16;
    if ((ret = usbh_uac_ctl_msg(p_fun, UAC_SET_CUR,
                   USB_REQ_TYPE_CLASS | USB_REQ_TAG_ENDPOINT | USB_DIR_OUT,
                   UAC_EP_CS_ATTR_SAMPLE_RATE << 8, ep,
                   data, sizeof(data))) < 0) {
        __UAC_TRACE("%d:%d: cannot set freq %d to ep %#x\r\n",
                    iface, fmt->altsetting, rate, ep);
        return ret;
    }
    /* 如果设备不支持读，不要检查设备采样率*/
    if (usbh_uac_get_sample_rate_quirk(chip)){
        return USB_OK;
    }
    if ((ret = usbh_uac_ctl_msg(p_fun, UAC_GET_CUR,
                    USB_REQ_TYPE_CLASS | USB_REQ_TAG_ENDPOINT | USB_DIR_IN,
                    UAC_EP_CS_ATTR_SAMPLE_RATE << 8, ep,
                    data, sizeof(data))) < 0) {
        __UAC_TRACE("%d:%d: cannot get freq at ep %#x\r\n",
                    iface, fmt->altsetting, ep);
        /* 有一些设备不支持读*/
        return USB_OK;
    }

    crate = data[0] | (data[1] << 8) | (data[2] << 16);
    if (crate != rate) {
        __UAC_TRACE("current rate %d is different from the "
                    "runtime rate %d\r\n", crate, rate);
    }

    return USB_OK;
}

/** 初始化采样率*/
usb_err_t usbh_uac_init_sample_rate(struct usbh_audio *chip,
                                    struct usbh_uac_substream *subs, int iface,
                                    struct usbh_interface *alts,
                                    struct audioformat *fmt, int rate)
{
    switch (fmt->protocol) {
    case UAC_VERSION_1:
    default:
        return set_sample_rate_v1(chip, subs, iface, alts, fmt, rate);

    case UAC_VERSION_2:
        //todo
        return -USB_EPERM;
    }
}
