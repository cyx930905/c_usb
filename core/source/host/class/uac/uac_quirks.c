#include "host/class/uac/uac_format.h"
#include "host/class/uac/uac_clock.h"
#include "host/class/uac/uac_mixer.h"
#include <string.h>
#include <stdio.h>

/** 检查UAC设备启动兼容*/
usb_err_t usbh_uac_apply_boot_quirk(struct usbh_function  *p_fun,
                                    struct usbh_interface *p_intf){
    //todo
    return USB_OK;
}

/** 检查UAC设备接口兼容*/
usb_err_t usbh_uac_apply_interface_quirk(struct usbh_audio *chip, int iface,
                                         int altno){
    //todo
    return USB_OK;
}


usb_err_t usbh_uac_select_mode_quirk(struct usbh_uac_substream *subs,
                                     struct audioformat *fmt){
    //todo
    return USB_OK;
}

/** 设置接口兼容*/
void usbh_uac_set_interface_quirk(struct usbh_device *p_dev)
{
    /* "Playback Design"产品设置接口后需要一个50ms的延时*/
    if (USB_CPU_TO_LE16(p_dev->p_desc->idVendor) == 0x23ba){
        usb_mdelay(50);
    }
}

/** 设置格式兼容*/
void usbh_uac_set_format_quirk(struct usbh_uac_substream *subs,
                               struct audioformat *fmt)
{
    switch (subs->stream->chip->usb_id) {
    case UAC_ID(0x041e, 0x3f02): /* E-Mu 0202 USB */
    case UAC_ID(0x041e, 0x3f04): /* E-Mu 0404 USB */
    case UAC_ID(0x041e, 0x3f0a): /* E-Mu Tracker Pre */
    case UAC_ID(0x041e, 0x3f19): /* E-Mu 0204 USB */
        //todo
        break;
    }
}

/** 端点启动兼容*/
void usbh_uac_endpoint_start_quirk(struct usbh_uac_endpoint *ep){
    //todo
}
/**
 * Marantz/Denon USB DACs需要一个自定义
 * 命令去在PCM和本地 DSD模式间切换
 */
static usb_bool_t is_marantz_denon_dac(uint32_t id)
{
    switch (id) {
    case UAC_ID(0x154e, 0x1003): /* Denon DA-300USB */
    case UAC_ID(0x154e, 0x3005): /* Marantz HD-DAC1 */
    case UAC_ID(0x154e, 0x3006): /* Marantz SA-14S1 */
        return USB_TRUE;
    }
    return USB_FALSE;
}

/** 检查设备是否使用大端采样*/
usb_bool_t usbh_uac_is_big_endian_format(struct usbh_audio *chip, struct audioformat *fp)
{
    /* 取决于备用设置设备是否是大端设备*/
    switch (chip->usb_id) {
    /* M-Audio Quattro: captured data only */
    case UAC_ID(0x0763, 0x2001):
        if ((fp->altsetting == 2) || (fp->altsetting == 3) ||
            (fp->altsetting == 5) || (fp->altsetting == 6)){
            return USB_TRUE;
        }
        break;
    /* M-Audio Audiophile USB */
    case UAC_ID(0x0763, 0x2003):
		//todo
        break;
    /* M-Audio Fast Track Pro */
    case UAC_ID(0x0763, 0x2012):
        if ((fp->altsetting == 2) || (fp->altsetting == 3) ||
            (fp->altsetting == 5) || (fp->altsetting == 6)){
            return USB_TRUE;
        }
        break;
    }
    return USB_FALSE;
}

/**
 * 调用以扩充DSD的PCM格式位字段类型，UAC标准没有指定位字段来表示支持的DSD接口。
 * 因此，所有已知支持此格式的硬件都必须在此列出。
 */
uint64_t usbh_uac_interface_dsd_format_quirks(struct usbh_audio *chip,
                                              struct audioformat *fp,
                                              uint32_t sample_bytes)
{
    /* Playback Designs */
    if (USBH_GET_DEV_VID(chip->p_fun) == 0x23ba) {
    	//todo
    }

    /* XMOS based USB DACs */
    switch (chip->usb_id) {
    case UAC_ID(0x20b1, 0x3008): /* iFi Audio micro/nano iDSD */
    case UAC_ID(0x20b1, 0x2008): /* Matrix Audio X-Sabre */
    case UAC_ID(0x20b1, 0x300a): /* Matrix Audio Mini-i Pro */
		//todo
        break;

    case UAC_ID(0x20b1, 0x2009): /* DIYINHK DSD DXD 384kHz USB to I2S/DSD */
    case UAC_ID(0x20b1, 0x2023): /* JLsounds I2SoverUSB */
		//todo
        break;
    default:
        break;
    }

    /* Denon/Marantz devices with USB DAC functionality */
    if (is_marantz_denon_dac(chip->usb_id)) {
        if (fp->altsetting == 2){
        	//todo
        }
    }

    return 0;
}

/** 控制传输兼容*/
void usbh_uac_ctl_msg_quirk(struct usbh_function *p_fun, uint8_t request,
                            uint8_t requesttype, uint16_t value, uint16_t index, void *data,
                            uint16_t size)
{
    /* "Playback Design"产品在每一次合规的类请求后要一个20ms的延时*/
    if ((USBH_GET_DEV_VID(p_fun) == 0x23ba) &&
        (requesttype & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS){
        usb_mdelay(20);
    }
    /* 有USB DAC 功能的Marantz/Denon设备在每一次合规的类请求后要一个20ms的延时*/
    if (is_marantz_denon_dac(UAC_ID(USBH_GET_DEV_VID(p_fun), USBH_GET_DEV_PID(p_fun)))
        && (requesttype & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS){
        usb_mdelay(20);
    }
    /* Zoom R16/24需要一点延时，否则类似获取/设置频率的请求会返回失败，尽管是成功*/
    if ((USBH_GET_DEV_VID(p_fun) == 0x1686) &&
        (USBH_GET_DEV_PID(p_fun) == 0x00dd) &&
        (requesttype & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS){
        usb_mdelay(1);
    }
}

/** 设备获取采样率兼容*/
usb_bool_t usbh_uac_get_sample_rate_quirk(struct usbh_audio *chip)
{
	/* 设备不支持读采样率*/
	switch (chip->usb_id) {
	case UAC_ID(0x045E, 0x075D): /* MS Lifecam Cinema  */
	case UAC_ID(0x045E, 0x076D): /* MS Lifecam HD-5000 */
	case UAC_ID(0x045E, 0x0772): /* MS Lifecam Studio */
	case UAC_ID(0x045E, 0x0779): /* MS Lifecam HD-3000 */
	case UAC_ID(0x04D8, 0xFEEA): /* Benchmark DAC1 Pre */
	case UAC_ID(0x074D, 0x3553): /* Outlaw RR2150 (Micronas UAC3553B) */
		return USB_TRUE;
	}
	return USB_FALSE;
}

/** 设置同步端点兼容*/
usb_err_t set_sync_ep_implicit_fb_quirk(struct usbh_uac_substream *subs,
                                        uint32_t attr){
    //todo
    return USB_OK;
}

/** 混音接口创建兼容*/
usb_err_t usbh_uac_mixer_apply_create_quirk(struct usb_mixer_interface *mixer){
    //todo
    return USB_OK;
}

