#include "host/class/uac/usbh_uac_operation.h"
#include "host/class/uac/uac_hw_rule.h"
#include "host/class/uac/uac_format.h"
#include "usb_config.h"
#include "stdio.h"

/** UAC设备打开函数*/
uac_handle usbh_uac_open(int audio_index, int pcm_index, int stream){
    usb_err_t ret;
    struct uac_pcm_substream *substream = NULL;
    struct usbh_audio *chip = NULL;
    struct usbh_uac_stream *as = NULL;
    struct uac_pcm *pcm = NULL;

    /* 获取USB音频设备*/
    chip = usbh_uac_dev_find(audio_index);

    if(chip != NULL){
        ret = usb_mutex_lock(chip->stream_mutex, 5000);
        if (ret != USB_OK) {
            return NULL;
        }
        /* 默认PCM*/
        if(pcm_index == DEFAULT_PCM){
            as = usb_list_first_entry(&chip->stream_list, struct usbh_uac_stream, list);
            if((as != NULL) && (as->pcm != NULL)){
                pcm = as->pcm;
            }
        }
        else{
            /* 特定PCM*/
            usb_list_for_each_entry(as, &chip->stream_list, struct usbh_uac_stream, list) {
                if((as->pcm_index == pcm_index) && (as->pcm != NULL)){
                    pcm = as->pcm;
                    break;
                }
            }
        }
        usb_mutex_unlock(chip->stream_mutex);
        if(pcm != NULL){
            /* 打开PCM子数据流*/
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
/** UAC设备关闭函数*/
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


///** UAC设备释放函数*/
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
//        /* 释放硬件设置*/
//        if (substream->ops->hw_free != NULL)
//            substream->ops->hw_free(substream);
//        /* 关闭数据流*/
//        substream->ops->close(substream);
//        substream->hw_opened = 0;
//    }
//    /* 释放PCM流*/
//    if (substream->pcm_release) {
//        substream->pcm_release(substream);
//        substream->pcm_release = NULL;
//    }
//    /* 断开链接子流*/
//    uac_pcm_detach_substream(substream);
//}


/** UAC设备设置硬件参数*/
usb_err_t usbh_uac_params_set(uac_handle handle,
                              struct usbh_uac_params_usr *params_usr){
    usb_err_t ret;
    uint32_t bits;
    uint32_t frames;
    struct uac_pcm_runtime *runtime;
    struct uac_pcm_hw_params *hw_params;
    struct uac_pcm_substream *substream = (struct uac_pcm_substream *)handle;
    /* 合法性检查*/
    if((substream == NULL) || (params_usr == NULL) || (substream->runtime == NULL)){
        return -USB_EINVAL;
    }

    ret = usb_mutex_lock(substream->mutex, 5000);
    if (ret != USB_OK) {
        return ret;
    }

    runtime = substream->runtime;
    /* 检查当前状态*/
    if((runtime->status.state != UAC_PCM_STATE_OPEN) &&
            (runtime->status.state != UAC_PCM_STATE_SETUP) &&
            (runtime->status.state != UAC_PCM_STATE_PREPARED)){
        return -USB_EPERM;
    }
    /* 获取硬件参数*/
    hw_params = usbh_uac_get_hw_params(params_usr);
    if(hw_params == NULL){
        return -USB_EPERM;
    }

    /* 优化硬件参数*/
    ret = uac_pcm_hw_refine(substream, hw_params);
    if (ret < 0)
        goto _error;
    /* 选择硬件参数的值*/
    ret = uac_pcm_hw_params_choose(substream, hw_params);
    if (ret != USB_OK){
        goto _error;
    }
    /* 调用PCM硬件设置回调函数*/
    if (substream->ops->hw_params != NULL) {
        ret = substream->ops->hw_params(substream, hw_params);
        if (ret < 0){
            goto _error;
        }
    }

    runtime->format_size = uac_get_pcm_format_size_bit(params_usr->pcm_format_size);
    runtime->channels    = params_channels(hw_params);
    /* 获取格式的位数*/
    bits = uac_pcm_format_physical_width(runtime->format_size);
    runtime->sample_bits = bits;
    /* 获取一帧数据的位数*/
    bits *= runtime->channels;
    runtime->frame_bits = bits;

    runtime->rate              = params_rate(hw_params);
    runtime->period_size       = params_period_size(hw_params);
    runtime->periods           = params_periods(hw_params);
    /* 至少有一个周期的帧缓存才可激活传输*/
    runtime->control.avail_min = runtime->period_size;
    runtime->buffer_frame_size = params_buffer_size(hw_params);

    frames = 1;
    while (bits % 8 != 0) {
        bits *= 2;
        frames *= 2;
    }
    /* 设置对齐*/
    runtime->byte_align = bits / 8;
    runtime->min_align = frames;
    /* 时间戳模式*/
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
    /* 设置状态*/
    if(runtime->status.state != UAC_PCM_STATE_DISCONNECTED){
        runtime->status.state = UAC_PCM_STATE_SETUP;
    }
    /* 重新定义参数*/
    /* 使能时间戳*/
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

/** UAC设备准备函数*/
usb_err_t usbh_uac_prepare(uac_handle handle){
    usb_err_t ret;
    struct uac_pcm_substream *substream = (struct uac_pcm_substream *)handle;
    struct uac_pcm_runtime *runtime = substream->runtime;

    if(substream == NULL){
        return -USB_EINVAL;
    }
    /* 检查数据流当前状态*/
    if ((runtime->status.state == UAC_PCM_STATE_OPEN) ||
        (runtime->status.state == UAC_PCM_STATE_DISCONNECTED)){
        return -USB_EPERM;
    }
    if (uac_pcm_running(substream)){
        return -USB_EBUSY;
    }
    /* 检查PCM 子流是否打开*/
    if(substream->hw_opened == 0){
        return -USB_EPERM;
    }

    ret = usb_mutex_lock(substream->mutex, 5000);
    if (ret != USB_OK) {
        return ret;
    }

    /* 调用PCM准备回调函数*/
    if(substream->ops->prepare){
        ret = substream->ops->prepare(substream);
        if(ret != USB_OK){
            goto __error;
        }
    }
    /* 复位PCM*/
    ret = uac_pcm_reset(substream, 0, USB_FALSE);
    if(ret != USB_OK){
        goto __error;
    }

    runtime->control.appl_ptr = runtime->status.hw_ptr;
    /* 设置准备状态*/
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
 * USB音频设备写函数
 *
 * @param handle      USB音频设备句柄
 * @param buffer      数据缓存
 * @param buffer_size 缓存大小
 *
 * @return 成功返回成功传输的字节数
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
    /* 检查状态*/
    if (runtime->status.state == UAC_PCM_STATE_OPEN){
        return -USB_EPERM;
    }
    /* 检查是否字节对齐*/
//    if (!frame_aligned(runtime, buffer_size)){
//        return -USB_EPERM;
//    }
    /* 转换为帧数*/
    frame_size = bytes_to_frames(runtime, buffer_size);
    /* 调用底层写函数*/
    ret = uac_pcm_write(substream, buffer, frame_size);
    if (ret > 0){
        /* 返回成功传输的字节数*/
        ret = frames_to_bytes(runtime, ret);
    }
    return ret;
}

/** 停止USB音频*/
usb_err_t usbh_uac_stop(uac_handle handle, uac_pcm_state_t state){
    struct uac_pcm_substream *substream = (struct uac_pcm_substream *)handle;
    return uac_pcm_stop(substream, state);
}



