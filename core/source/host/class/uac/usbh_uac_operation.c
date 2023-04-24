#include "host/class/uac/usbh_uac_operation.h"
#include "host/class/uac/uac_hw_rule.h"
#include "host/class/uac/uac_format.h"
#include "usb_config.h"
#include "stdio.h"

/** UAC�豸�򿪺���*/
uac_handle usbh_uac_open(int audio_index, int pcm_index, int stream){
    usb_err_t ret;
    struct uac_pcm_substream *substream = NULL;
    struct usbh_audio *chip = NULL;
    struct usbh_uac_stream *as = NULL;
    struct uac_pcm *pcm = NULL;

    /* ��ȡUSB��Ƶ�豸*/
    chip = usbh_uac_dev_find(audio_index);

    if(chip != NULL){
        ret = usb_mutex_lock(chip->stream_mutex, 5000);
        if (ret != USB_OK) {
            return NULL;
        }
        /* Ĭ��PCM*/
        if(pcm_index == DEFAULT_PCM){
            as = usb_list_first_entry(&chip->stream_list, struct usbh_uac_stream, list);
            if((as != NULL) && (as->pcm != NULL)){
                pcm = as->pcm;
            }
        }
        else{
            /* �ض�PCM*/
            usb_list_for_each_entry(as, &chip->stream_list, struct usbh_uac_stream, list) {
                if((as->pcm_index == pcm_index) && (as->pcm != NULL)){
                    pcm = as->pcm;
                    break;
                }
            }
        }
        usb_mutex_unlock(chip->stream_mutex);
        if(pcm != NULL){
            /* ��PCM��������*/
            ret = uac_pcm_open_substream(pcm, stream, &substream);
            if((ret != USB_OK) || (substream == NULL)){
                __UAC_TRACE("UAC open failed\r\n");
                return NULL;
            }
            return substream;
        }
    }

    return NULL;
}

extern void uac_pcm_release_substream(usb_refcnt *ref);
/** UAC�豸�رպ���*/
usb_err_t usbh_uac_close(uac_handle handle){
    struct uac_pcm_substream *substream = (struct uac_pcm_substream *)handle;

    if(handle == NULL){
        return -USB_EINVAL;
    }

    if(substream->refcnt > 0){
        usb_refcnt_put(&substream->refcnt, uac_pcm_release_substream);
    }
    return USB_OK;
}


///** UAC�豸�ͷź���*/
//void usbh_uac_release(uac_handle handle){
//    struct uac_pcm_substream *substream = (struct uac_pcm_substream *)handle;
//    if((substream == NULL) || (substream->runtime == NULL)){
//        return;
//    }
//
//    substream->refcnt--;
//    if (substream->refcnt > 0){
//        return;
//    }
//
//    uac_pcm_drop(substream);
//    if (substream->hw_opened) {
//        /* �ͷ�Ӳ������*/
//        if (substream->ops->hw_free != NULL)
//            substream->ops->hw_free(substream);
//        /* �ر�������*/
//        substream->ops->close(substream);
//        substream->hw_opened = 0;
//    }
//    /* �ͷ�PCM��*/
//    if (substream->pcm_release) {
//        substream->pcm_release(substream);
//        substream->pcm_release = NULL;
//    }
//    /* �Ͽ���������*/
//    uac_pcm_detach_substream(substream);
//}


/** UAC�豸����Ӳ������*/
usb_err_t usbh_uac_params_set(uac_handle handle,
                              struct usbh_uac_params_usr *params_usr){
    usb_err_t ret;
    uint32_t bits;
    uint32_t frames;
    struct uac_pcm_runtime *runtime;
    struct uac_pcm_hw_params *hw_params;
    struct uac_pcm_substream *substream = (struct uac_pcm_substream *)handle;
    /* �Ϸ��Լ��*/
    if((substream == NULL) || (params_usr == NULL) || (substream->runtime == NULL)){
        return -USB_EINVAL;
    }

    ret = usb_mutex_lock(substream->mutex, 5000);
    if (ret != USB_OK) {
        return ret;
    }

    runtime = substream->runtime;
    /* ��鵱ǰ״̬*/
    if((runtime->status.state != UAC_PCM_STATE_OPEN) &&
            (runtime->status.state != UAC_PCM_STATE_SETUP) &&
            (runtime->status.state != UAC_PCM_STATE_PREPARED)){
        return -USB_EPERM;
    }
    /* ��ȡӲ������*/
    hw_params = usbh_uac_get_hw_params(params_usr);
    if(hw_params == NULL){
        return -USB_EPERM;
    }

    /* �Ż�Ӳ������*/
    ret = uac_pcm_hw_refine(substream, hw_params);
    if (ret < 0)
        goto _error;
    /* ѡ��Ӳ��������ֵ*/
    ret = uac_pcm_hw_params_choose(substream, hw_params);
    if (ret != USB_OK){
        goto _error;
    }
    /* ����PCMӲ�����ûص�����*/
    if (substream->ops->hw_params != NULL) {
        ret = substream->ops->hw_params(substream, hw_params);
        if (ret < 0){
            goto _error;
        }
    }

    runtime->format_size = uac_get_pcm_format_size_bit(params_usr->pcm_format_size);
    runtime->channels    = params_channels(hw_params);
    /* ��ȡ��ʽ��λ��*/
    bits = uac_pcm_format_physical_width(runtime->format_size);
    runtime->sample_bits = bits;
    /* ��ȡһ֡���ݵ�λ��*/
    bits *= runtime->channels;
    runtime->frame_bits = bits;

    runtime->rate              = params_rate(hw_params);
    runtime->period_size       = params_period_size(hw_params);
    runtime->periods           = params_periods(hw_params);
    /* ������һ�����ڵ�֡����ſɼ����*/
    runtime->control.avail_min = runtime->period_size;
    runtime->buffer_frame_size = params_buffer_size(hw_params);

    frames = 1;
    while (bits % 8 != 0) {
        bits *= 2;
        frames *= 2;
    }
    /* ���ö���*/
    runtime->byte_align = bits / 8;
    runtime->min_align = frames;
    /* ʱ���ģʽ*/
    runtime->tstamp_mode = USBH_SND_PCM_TSTAMP_NONE;
    runtime->period_step = 1;
    runtime->start_threshold = 1;
    runtime->stop_threshold = runtime->buffer_frame_size;
    runtime->silence_threshold = 0;
    runtime->silence_size = 0;
    runtime->boundary = runtime->buffer_frame_size;
    while (runtime->boundary * 2 <= USB_LONG_MAX - runtime->buffer_frame_size){
        runtime->boundary *= 2;
    }
    /* ����״̬*/
    if(runtime->status.state != UAC_PCM_STATE_DISCONNECTED){
        runtime->status.state = UAC_PCM_STATE_SETUP;
    }
    /* ���¶������*/
    /* ʹ��ʱ���*/
    runtime->tstamp_mode = USBH_SND_PCM_TSTAMP_ENABLE;
    runtime->stop_threshold = runtime->boundary;
    runtime->silence_threshold = 0;
    runtime->silence_size = runtime->boundary;

    usb_mutex_unlock(substream->mutex);

    usb_mem_free(hw_params);

    substream->hw_param_set = 1;

    return USB_OK;
_error:
    if (substream->ops->hw_free != NULL){
        substream->ops->hw_free(substream);
    }
    usb_mutex_unlock(substream->mutex);
    return ret;
}

/** UAC�豸׼������*/
usb_err_t usbh_uac_prepare(uac_handle handle){
    usb_err_t ret;
    struct uac_pcm_substream *substream = (struct uac_pcm_substream *)handle;
    struct uac_pcm_runtime *runtime = substream->runtime;

    if(substream == NULL){
        return -USB_EINVAL;
    }
    /* �����������ǰ״̬*/
    if ((runtime->status.state == UAC_PCM_STATE_OPEN) ||
        (runtime->status.state == UAC_PCM_STATE_DISCONNECTED)){
        return -USB_EPERM;
    }
    if (uac_pcm_running(substream)){
        return -USB_EBUSY;
    }
    /* ���PCM �����Ƿ��*/
    if(substream->hw_opened == 0){
        return -USB_EPERM;
    }

    ret = usb_mutex_lock(substream->mutex, 5000);
    if (ret != USB_OK) {
        return ret;
    }

    /* ����PCM׼���ص�����*/
    if(substream->ops->prepare){
        ret = substream->ops->prepare(substream);
        if(ret != USB_OK){
            goto __error;
        }
    }
    /* ��λPCM*/
    ret = uac_pcm_reset(substream, 0, USB_FALSE);
    if(ret != USB_OK){
        goto __error;
    }

    runtime->control.appl_ptr = runtime->status.hw_ptr;
    /* ����׼��״̬*/
    if (substream->runtime->status.state != UAC_PCM_STATE_DISCONNECTED){
        substream->runtime->status.state = UAC_PCM_STATE_PREPARED;
    }

    usb_mutex_unlock(substream->mutex);
    return USB_OK;
__error:
    usb_mutex_unlock(substream->mutex);
    return ret;
}

/**
 * USB��Ƶ�豸д����
 *
 * @param handle      USB��Ƶ�豸���
 * @param buffer      ���ݻ���
 * @param buffer_size �����С
 *
 * @return �ɹ����سɹ�������ֽ���
 */
usb_err_t usbh_uac_write(uac_handle handle, uint8_t *buffer, uint32_t buffer_size){
    usb_err_t ret;
    struct uac_pcm_substream *substream = (struct uac_pcm_substream *)handle;
    struct uac_pcm_runtime *runtime;
    uint32_t frame_size;

    if((substream == NULL) || (substream->runtime == NULL)){
        return -USB_EINVAL;
    }
    runtime = substream->runtime;
    /* ���״̬*/
    if (runtime->status.state == UAC_PCM_STATE_OPEN){
        return -USB_EPERM;
    }
    /* ����Ƿ��ֽڶ���*/
//    if (!frame_aligned(runtime, buffer_size)){
//        return -USB_EPERM;
//    }
    /* ת��Ϊ֡��*/
    frame_size = bytes_to_frames(runtime, buffer_size);
    /* ���õײ�д����*/
    ret = uac_pcm_write(substream, buffer, frame_size);
    if (ret > 0){
        /* ���سɹ�������ֽ���*/
        ret = frames_to_bytes(runtime, ret);
    }
    return ret;
}

/** ֹͣUSB��Ƶ*/
usb_err_t usbh_uac_stop(uac_handle handle, uac_pcm_state_t state){
    struct uac_pcm_substream *substream = (struct uac_pcm_substream *)handle;
    return uac_pcm_stop(substream, state);
}



