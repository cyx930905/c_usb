#ifndef __UAC_QUIRKS_H_
#define __UAC_QUIRKS_H_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "host/class/uac/usbh_uac_driver.h"
#include "host/class/uac/uac.h"
#include "host/class/uac/uac_pcm.h"
#include "host/class/uac/uac_mixer.h"
#include "usb_config.h"

/** ���UAC�豸��������*/
usb_err_t usbh_uac_apply_boot_quirk(struct usbh_function  *p_fun,
                                    struct usbh_interface *p_intf);
/** ���UAC�豸�ӿڼ���*/
usb_err_t usbh_uac_apply_interface_quirk(struct usbh_audio *chip, int iface,
                                         int altno);
usb_err_t usbh_uac_select_mode_quirk(struct usbh_uac_substream *subs,
                                     struct audioformat *fmt);
/** ���ýӿڼ���*/
void usbh_uac_set_interface_quirk(struct usbh_device *p_dev);
/** ���ø�ʽ����*/
void usbh_uac_set_format_quirk(struct usbh_uac_substream *subs,
                               struct audioformat *fmt);
/** �˵���������*/
void usbh_uac_endpoint_start_quirk(struct usbh_uac_endpoint *ep);
/** ����豸�Ƿ�ʹ�ô�˲���*/
usb_bool_t usbh_uac_is_big_endian_format(struct usbh_audio *chip, struct audioformat *fp);
/**
 * ����������DSD��PCM��ʽλ�ֶ����ͣ�UAC��׼û��ָ��λ�ֶ�����ʾ֧�ֵ�DSD�ӿڡ�
 * ��ˣ�������֪֧�ִ˸�ʽ��Ӳ���������ڴ��г���
 */
uint64_t usbh_uac_interface_dsd_format_quirks(struct usbh_audio *chip,
                                              struct audioformat *fp,
                                              uint32_t sample_bytes);
/** ���ƴ������*/
void usbh_uac_ctl_msg_quirk(struct usbh_function *p_fun, uint8_t request,
                            uint8_t requesttype, uint16_t value, uint16_t index, void *data,
                            uint16_t size);
/** �豸��ȡ�����ʼ���*/
usb_bool_t usbh_uac_get_sample_rate_quirk(struct usbh_audio *chip);
/** ����ͬ���˵����*/
usb_err_t set_sync_ep_implicit_fb_quirk(struct usbh_uac_substream *subs,
                                        uint32_t attr);
/** �����ӿڴ�������*/
usb_err_t usbh_uac_mixer_apply_create_quirk(struct usb_mixer_interface *mixer);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif

