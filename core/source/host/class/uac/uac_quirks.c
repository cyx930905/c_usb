#include "host/class/uac/uac_format.h"
#include "host/class/uac/uac_clock.h"
#include "host/class/uac/uac_mixer.h"
#include <string.h>
#include <stdio.h>

/** ���UAC�豸��������*/
usb_err_t usbh_uac_apply_boot_quirk(struct usbh_function  *p_fun,
                                    struct usbh_interface *p_intf){
    //todo
    return USB_OK;
}

/** ���UAC�豸�ӿڼ���*/
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

/** ���ýӿڼ���*/
void usbh_uac_set_interface_quirk(struct usbh_device *p_dev)
{
    /* "Playback Design"��Ʒ���ýӿں���Ҫһ��50ms����ʱ*/
    if (USB_CPU_TO_LE16(p_dev->p_desc->idVendor) == 0x23ba){
        usb_mdelay(50);
    }
}

/** ���ø�ʽ����*/
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

/** �˵���������*/
void usbh_uac_endpoint_start_quirk(struct usbh_uac_endpoint *ep){
    //todo
}
/**
 * Marantz/Denon USB DACs��Ҫһ���Զ���
 * ����ȥ��PCM�ͱ��� DSDģʽ���л�
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

/** ����豸�Ƿ�ʹ�ô�˲���*/
usb_bool_t usbh_uac_is_big_endian_format(struct usbh_audio *chip, struct audioformat *fp)
{
    /* ȡ���ڱ��������豸�Ƿ��Ǵ���豸*/
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
 * ����������DSD��PCM��ʽλ�ֶ����ͣ�UAC��׼û��ָ��λ�ֶ�����ʾ֧�ֵ�DSD�ӿڡ�
 * ��ˣ�������֪֧�ִ˸�ʽ��Ӳ���������ڴ��г���
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

/** ���ƴ������*/
void usbh_uac_ctl_msg_quirk(struct usbh_function *p_fun, uint8_t request,
                            uint8_t requesttype, uint16_t value, uint16_t index, void *data,
                            uint16_t size)
{
    /* "Playback Design"��Ʒ��ÿһ�κϹ���������Ҫһ��20ms����ʱ*/
    if ((USBH_GET_DEV_VID(p_fun) == 0x23ba) &&
        (requesttype & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS){
        usb_mdelay(20);
    }
    /* ��USB DAC ���ܵ�Marantz/Denon�豸��ÿһ�κϹ���������Ҫһ��20ms����ʱ*/
    if (is_marantz_denon_dac(UAC_ID(USBH_GET_DEV_VID(p_fun), USBH_GET_DEV_PID(p_fun)))
        && (requesttype & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS){
        usb_mdelay(20);
    }
    /* Zoom R16/24��Ҫһ����ʱ���������ƻ�ȡ/����Ƶ�ʵ�����᷵��ʧ�ܣ������ǳɹ�*/
    if ((USBH_GET_DEV_VID(p_fun) == 0x1686) &&
        (USBH_GET_DEV_PID(p_fun) == 0x00dd) &&
        (requesttype & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS){
        usb_mdelay(1);
    }
}

/** �豸��ȡ�����ʼ���*/
usb_bool_t usbh_uac_get_sample_rate_quirk(struct usbh_audio *chip)
{
	/* �豸��֧�ֶ�������*/
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

/** ����ͬ���˵����*/
usb_err_t set_sync_ep_implicit_fb_quirk(struct usbh_uac_substream *subs,
                                        uint32_t attr){
    //todo
    return USB_OK;
}

/** �����ӿڴ�������*/
usb_err_t usbh_uac_mixer_apply_create_quirk(struct usb_mixer_interface *mixer){
    //todo
    return USB_OK;
}

