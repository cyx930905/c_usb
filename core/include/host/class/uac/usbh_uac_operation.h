#ifndef __USBH_UAC_OPERATION_H_
#define __USBH_UAC_OPERATION_H_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "usb_adapter_inter.h"
#include "usb_errno.h"
#include "host/core/usbh.h"
#include "host/class/uac/uac_pcm.h"
#include "host/class/uac/usbh_uac_driver.h"

/* USB��Ƶ�豸���*/
typedef void* uac_handle;
/* Ĭ������*/
#define USB_AUDIO_DEFAULT  -1
/* Ĭ��PCM*/
#define DEFAULT_PCM    -1


/* �û�Ӳ�������ṹ��*/
struct usbh_uac_params_usr {
    int      pcm_format_size;  /* PCM��ʽ����*/
    uint32_t channels;         /* ͨ������*/
    uint32_t rate;             /* ������*/
    uint32_t period_frames;    /* һ��������֡����*/
    uint32_t buffer_periods;   /* һ���������������*/
    uint32_t bits_per_sample;  /* ������λ��*/
};

/** UAC�豸�򿪺���*/
uac_handle usbh_uac_open(int audio_index, int pcm_index, int stream);
/** UAC�豸����Ӳ������*/
usb_err_t usbh_uac_params_set(uac_handle handle,
                              struct usbh_uac_params_usr *hw_params);
/** UAC�豸׼������*/
usb_err_t usbh_uac_prepare(uac_handle handle);
/**
 * USB��Ƶ�豸д����
 *
 * @param handle      USB��Ƶ�豸���
 * @param buffer      ���ݻ���
 * @param buffer_size �����С
 *
 * @return �ɹ����سɹ�������ֽ���
 */
usb_err_t usbh_uac_write(uac_handle handle, uint8_t *buffer, uint32_t buffer_size);
/** ֹͣUSB��Ƶ*/
usb_err_t usbh_uac_stop(uac_handle handle, uac_pcm_state_t state);
/** UAC�豸�ͷź���*/
void usbh_uac_release(uac_handle handle);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif

