#include "host/class/uac/uac_pcm.h"
#include "host/class/uac/usbh_uac_driver.h"
#include "host/class/uac/uac_quirks.h"
#include "host/class/uac/uac_endpoint.h"
#include "host/class/uac/uac_clock.h"
#include "host/class/uac/uac_hw_rule.h"
#include "usb_config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//
//
//
//
//
//
//
///**
// * 计算一个周期有多少字节
// *
// * @param format   PCM格式深度
// * @param channels 通道数量
// * @param frames   一个周期帧的数量
// *
// * @return 成功返回一个周期的字节数
// */
//uint32_t uac_pcm_bytes_to_periods(uac_pcm_format_t format, uint32_t channels, uint32_t frames){
//    int val;
//
//    if ((format < 0) || (format > SNDRV_PCM_FORMAT_LAST) || (frames == 0)){
//        return -USB_EINVAL;
//    }
//    if ((val = pcm_formats[format].width) == 0){
//        return -USB_EINVAL;
//    }
//
//    return (frames * (channels * val / 8));
//}
//
//
///** 根据给定的格式位宽获取样本字节大小*/
//uint32_t uac_pcm_format_size(uint32_t format, uint32_t samples)
//{
//    int phys_width = uac_pcm_format_physical_width(format);
//    if (phys_width < 0){
//        return -USB_EINVAL;
//    }
//    return samples * phys_width / 8;
//}


extern usb_err_t uac_start_endpoints(struct usbh_uac_substream *subs,
                                     usb_bool_t can_sleep);
extern usb_err_t uac_configure_endpoint(struct usbh_uac_substream *subs);
extern void uac_stop_endpoints(struct usbh_uac_substream *subs, usb_bool_t wait);

/** 基于USB帧计数计算延时的音频帧数*/
int usbh_uac_pcm_delay(struct usbh_uac_substream *subs, uint32_t rate)
{
    int current_frame_number;
    int frame_diff;
    int est_delay;

    if (subs->last_delay == 0){
        return 0;
    }
    /* 获取当前周期调度的帧索引*/
    current_frame_number = usbh_host_get_frame_num(subs->p_fun->p_udev->p_hc);
    /* 计算相差的周期帧间隔*/
    frame_diff = (current_frame_number - subs->last_frame_number) & 0xff;
    /* 计算相差多少音频帧，一周期帧是1ms*/
    est_delay =  frame_diff * rate / 1000;
    if (subs->direction == USBH_SND_PCM_STREAM_PLAYBACK){
        est_delay = subs->last_delay - est_delay;
    }else{
        est_delay = subs->last_delay + est_delay;
    }
    if (est_delay < 0){
        est_delay = 0;
    }
    return est_delay;
}

/**
 * PCM连接子流
 *
 * @param pcm        PCM实体
 * @param stream     数据流方向
 * @param rsubstream 要返回的子流结构体地址
 *
 * @return 成功返回USB_OK
 */
static usb_err_t uac_pcm_attach_substream(struct uac_pcm *pcm, int stream,
                                          struct uac_pcm_substream **rsubstream)
{
    struct uac_pcm_str * pstr;
    struct uac_pcm_substream *substream;
    struct uac_pcm_runtime *runtime;

    /* 合法性检查*/
    if ((pcm == NULL) || (rsubstream == NULL)){
        return -USB_ENXIO;
    }
    /* 检查PCM类型*/
    if (stream != USBH_SND_PCM_STREAM_PLAYBACK &&
               stream != USBH_SND_PCM_STREAM_CAPTURE){
        return -USB_EINVAL;
    }

    *rsubstream = NULL;
    /* 获取PCM具体的数据流*/
    pstr = &pcm->streams[stream];
    /* 数据流是否存在子流*/
    if (pstr->substream == NULL || pstr->substream_count == 0){
        return -USB_ENODEV;
    }

    /* 遍历没有使用过的数据子流*/
    for (substream = pstr->substream; substream; substream = substream->next) {
        if (substream->refcnt == 1){
            break;
        }
    }

    if(substream == NULL){
        return -USB_EAGAIN;
    }

    /* 分配运行时间结构体*/
    runtime = usb_mem_alloc(sizeof(struct uac_pcm_runtime));
    if (runtime == NULL){
        return -USB_ENOMEM;
    }
    memset(runtime, 0, sizeof(struct uac_pcm_runtime));
    /* 设置PDM打开状态*/
    runtime->status.state = UAC_PCM_STATE_OPEN;

    substream->runtime = runtime;
    substream->private_data = pcm->private_data;
    usb_refcnt_get(&substream->refcnt);
    pstr->substream_opened++;
    *rsubstream = substream;
    return USB_OK;
}

/** 断开子数据流*/
void uac_pcm_detach_substream(struct uac_pcm_substream *substream)
{
    struct uac_pcm_runtime *runtime;

    /* 合法性检查*/
    if (UAC_PCM_RUNTIME_CHECK(substream)){
        return;
    }
    runtime = substream->runtime;
    /* 释放硬件限制规则*/
    if (runtime->hw_constraints.rules != NULL){
        usb_mem_free(runtime->hw_constraints.rules);
    }
    usb_mem_free(runtime);
    substream->runtime = NULL;
    substream->pstr->substream_opened--;
}

/** 释放子数据流*/
void uac_pcm_release_substream(usb_refcnt *ref)
{
    usb_err_t ret;
    struct uac_pcm_substream *p_substream;

    p_substream = USB_CONTAINER_OF(ref, struct uac_pcm_substream, refcnt);
    ret = usb_mutex_lock(p_substream->mutex, 5000);
    if (ret != USB_OK) {
        return;
    }
    /* 设置了硬件参数，释放掉*/
    if (p_substream->hw_param_set) {
        if (p_substream->ops->hw_free != NULL){
            p_substream->ops->hw_free(p_substream);
        }
        p_substream->hw_param_set = 0;
    }
    /* 打开状态，关闭*/
    if (p_substream->hw_opened){
        p_substream->ops->close(p_substream);
        p_substream->hw_opened = 0;
    }
    /* 断开链接*/
    uac_pcm_detach_substream(p_substream);

    usb_mutex_unlock(p_substream->mutex);
    /* 删除互斥锁*/
    if (p_substream->mutex) {
        usb_mutex_delete(p_substream->mutex);
    }
    usb_mem_free(p_substream);
}

/** 打开一个PCM的子流*/
usb_err_t uac_pcm_open_substream(struct uac_pcm *pcm, int stream,
                                 struct uac_pcm_substream **rsubstream){
    struct uac_pcm_substream *substream;
    usb_err_t ret;

    /* 寻找可用的数据子流*/
    ret = uac_pcm_attach_substream(pcm, stream, &substream);
    if (ret != USB_OK){
        return ret;
    }
    /* 保护pcm子数据流*/
    ret = usb_mutex_lock(substream->mutex, 5000);
    if (ret != USB_OK) {
        return ret;
    }

    /* 初始化硬件限制*/
    ret = uac_pcm_hw_constraints_init(substream);
    if (ret != USB_OK) {
        goto error;
    }

    /* 调用PCM打开回调函数*/
    if ((ret = substream->ops->open(substream)) != USB_OK){
        goto error;
    }
    /* 设置已打开标志位*/
    substream->hw_opened = 1;

    ret = uac_pcm_hw_constraints_complete(substream);
    if (ret != USB_OK) {
        goto error;
    }
    *rsubstream = substream;
    usb_mutex_unlock(substream->mutex);
    return USB_OK;

error:
    usb_refcnt_put(&substream->refcnt, uac_pcm_release_substream);
    usb_mutex_unlock(substream->mutex);
    return ret;
}

/**
 * 创建一个新的PCM数据流
 *
 * @param pcm             PCM实体
 * @param stream          数据流方向
 * @param substream_count 子数据流的数量
 *
 * @return 成功返回USB_OK
 */
usb_err_t uac_pcm_new_stream(struct uac_pcm *pcm,
                             int stream, int substream_count)
{
    int idx;
    struct uac_pcm_str *pstr = &pcm->streams[stream];
    struct uac_pcm_substream *substream, *prev;

    pstr->stream = stream;
    pstr->pcm = pcm;
    pstr->substream_count = substream_count;
    if (substream_count == 0){
        return USB_OK;
    }

    prev = NULL;
    for (idx = 0, prev = NULL; idx < substream_count; idx++) {
        substream = usb_mem_alloc(sizeof(struct uac_pcm_substream));
        if (substream == NULL){
            return -USB_ENOMEM;
        }
        memset(substream, 0, sizeof(struct uac_pcm_substream));

        substream->refcnt = 1;
        substream->pcm = pcm;
        substream->pstr = pstr;
        substream->number = idx;
        substream->stream = stream;
        substream->next = NULL;
        sprintf(substream->name, "subdevice #%i", idx);
        substream->buffer_bytes_max = USB_UINT_MAX;
        if (prev == NULL){
            pstr->substream = substream;
        }
        else{
            prev->next = substream;
        }
        /* 创建互斥锁*/
        substream->mutex = usb_mutex_create();
        if(substream->mutex == NULL){
            usb_mem_free(substream);
            return -USB_ENOMEM;
        }
        prev = substream;
    }
    return USB_OK;
}

void uac_pcm_period_elapsed(struct uac_pcm_substream *substream);
/** 回放传输请求包准备*/
static void prepare_playback_trp(struct usbh_uac_substream *subs,
                 struct usbh_trp *trp){
    uint32_t counts, frames, bytes;
    struct usbh_uac_endpoint *ep = subs->data_endpoint;
    struct uac_trp_ctx *ctx = trp->priv;
    struct uac_pcm_runtime *runtime = subs->pcm_substream->runtime;
    int i, stride, period_elapsed = 0;
    /* 获取1帧的位数*/
    int frame_bits = uac_pcm_format_physical_width(subs->pcm_format_size) * subs->channels;
    /* 1帧的字节数*/
    stride = frame_bits >> 3;

    frames = 0;
    trp->number_of_packets = 0;
    /* 获取USB传输请求包最大的帧数*/
    subs->frame_limit += ep->max_trp_frames;
    for (i = 0; i < ctx->packets; i++) {
        if (ctx->packet_size[i]){
            counts = ctx->packet_size[i];
        }else{
            /* 决定下一个包要发送的帧数量*/
            counts = usbh_uac_endpoint_next_packet_size(ep);
        }

        /* 设置偏移*/
        trp->iso_frame_desc[i].offset = frames * ep->stride;
        /* 设置数据长度*/
        trp->iso_frame_desc[i].length = counts * ep->stride;
        /* 计算帧数*/
        frames += counts;
        /* 包的数量增加*/
        trp->number_of_packets++;
        /* 计算传输完成的帧数*/
        subs->transfer_done += counts;
        /* 传输完成的帧数超过或等于一个周期*/
        if (subs->transfer_done >= runtime->period_size) {
            subs->transfer_done -= runtime->period_size;
            subs->frame_limit = 0;
            period_elapsed = 1;
            if (subs->fmt_type == UAC_FORMAT_TYPE_II) {
                //todo
            }
        }
        if ((period_elapsed) ||
                (subs->transfer_done >= subs->frame_limit)){
            break;
        }
    }
    /* 总共要处理的字节数*/
    bytes = frames * ep->stride;
    /* 通用的PCM */
    /* 传输字节超过数据缓存边界*/
    if (subs->hwptr_done + bytes > runtime->buffer_frame_size * stride) {
        uint32_t bytes1 = runtime->buffer_frame_size * stride - subs->hwptr_done;
        memcpy(trp->p_data, runtime->buffer + subs->hwptr_done, bytes1);
        memcpy(trp->p_data + bytes1, runtime->buffer, bytes - bytes1);
    }
    else {
        memcpy(trp->p_data, runtime->buffer + subs->hwptr_done, bytes);
    }
    /* 重新计算要传输完成字节指针*/
    subs->hwptr_done += bytes;
    /* 字节数超过缓存字节数*/
    if (subs->hwptr_done >= runtime->buffer_frame_size * stride){
        subs->hwptr_done -= runtime->buffer_frame_size * stride;
    }
    /* 更新延迟的音频帧数 */
    runtime->delay = subs->last_delay;
    runtime->delay += frames;
    subs->last_delay = runtime->delay;
    /* 获取当前的周期帧号*/
    subs->last_frame_number = usbh_host_get_frame_num(subs->p_fun->p_udev->p_hc);
    /* 获取地8位*/
    subs->last_frame_number &= 0xFF;
    /* 记录启动时间*/
    if (subs->trigger_tstamp_pending_update) {
        /* 获取时间*/
        uac_pcm_gettime(runtime, &runtime->trigger_tstamp);
        subs->trigger_tstamp_pending_update = USB_FALSE;
    }
    /* 设置下一个USB传输请求包要传输的字节数*/
    trp->len = bytes;
    /* 每传输完一个周期需要更新下状态*/
    if (period_elapsed){
        uac_pcm_period_elapsed(subs->pcm_substream);
    }
}

/* 处理完成传输的数据，减少延迟计数*/
static void retire_playback_trp(struct usbh_uac_substream *subs,
                                struct usbh_trp *p_trp)
{
    struct uac_pcm_runtime *runtime = subs->pcm_substream->runtime;
    struct usbh_uac_endpoint *ep = subs->data_endpoint;
    int processed = p_trp->len / ep->stride;
    int est_delay;

    /* 在processed=0时忽略延迟计算，即在处理实际数据之前处理装载静音数据*/
    if (processed == 0){
        return;
    }
    if (subs->last_delay == 0){
        return;
    }
    /* 获取延迟的帧数*/
    est_delay = usbh_uac_pcm_delay(subs, runtime->rate);
    /* 更新延迟的帧数*/
    if (processed > subs->last_delay){
        subs->last_delay = 0;
    }else{
        subs->last_delay -= processed;
    }
    runtime->delay = subs->last_delay;

    /* 延迟估计值超过2ms时上报。由于估计值依赖每毫秒更新一次的计数器的两次读取，因此误差应小于2ms*/
    if (abs(est_delay - subs->last_delay) * 1000 > runtime->rate * 2){
//        __UAC_PCM_TRACE("delay: estimated %d, actual %d\r\n",
//                est_delay, subs->last_delay);
    }

    if (subs->running == 0) {
        /* 更新最新的周期帧号*/
        subs->last_frame_number =
                usbh_host_get_frame_num(subs->p_fun->p_udev->p_hc) & 0xff;
    }
}

/** 更新PCM状态*/
usb_err_t uac_pcm_update_state(struct uac_pcm_substream *substream,
                               struct uac_pcm_runtime *runtime){
    uint32_t avail;

    /* 计算可用的帧数*/
    if (substream->stream == USBH_SND_PCM_STREAM_PLAYBACK){
        avail = uac_pcm_playback_avail(runtime);
    }else{
        avail = uac_pcm_capture_avail(runtime);
    }
    /* 设置限制*/
    if (avail > runtime->avail_max){
        runtime->avail_max = avail;
    }
    if (runtime->status.state == UAC_PCM_STATE_DRAINING) {
        if (avail >= runtime->buffer_frame_size) {
            //snd_pcm_drain_done(substream);
            return -USB_EPIPE;
        }
    } else {
        if (avail >= runtime->stop_threshold) {
            //xrun(substream);
            return -USB_EPIPE;
        }
    }
//    if (runtime->twake) {
//        if (avail >= runtime->twake)
//            wake_up(&runtime->tsleep);
//    } else if (avail >= runtime->control->avail_min)
//        wake_up(&runtime->sleep);
    return USB_OK;
}

/** 更新音频时间戳*/
static void update_audio_tstamp(struct uac_pcm_substream *substream,
                struct usb_timespec *curr_tstamp,
                struct usb_timespec *audio_tstamp){
    struct uac_pcm_runtime *runtime = substream->runtime;
    uint64_t audio_frames, audio_nsecs;
    struct usb_timespec driver_tstamp;

    if ((substream == NULL) || (curr_tstamp == NULL) || (audio_tstamp == NULL)){
        return;
    }
    if (runtime->tstamp_mode != USBH_SND_PCM_TSTAMP_ENABLE){
        return;
    }
    /* 获取音频帧字节数*/
    audio_frames = runtime->hw_ptr_wrap + runtime->status.hw_ptr;

    audio_nsecs = usb_div_u64(audio_frames * 1000000000LL, runtime->rate);
    //*audio_tstamp = ns_to_timespec(audio_nsecs);

    runtime->status.audio_tstamp = *audio_tstamp;
    runtime->status.tstamp = *curr_tstamp;
    /*
     * re-take a driver timestamp to let apps detect if the reference tstamp
     * read by low-level hardware was provided with a delay
     */
    uac_pcm_gettime(substream->runtime, (struct usb_timespec *)&driver_tstamp);
    runtime->driver_tstamp = driver_tstamp;
}

extern struct pcm_format_data pcm_formats[SNDRV_PCM_FORMAT_LAST + 1];
/**
 * 用给定的采样率在缓存中设置静音数据
 *
 * @param PCM格式
 * @param 缓存指针
 * @param 要设置静音的采样率数量
 *
 * @return 成功返回USB_OK
 */
usb_err_t uac_pcm_format_set_silence(uac_pcm_format_t format, void *data, uint32_t samples)
{
    int width;
    uint8_t *dst, *pat;

    if ((format < 0) || (format > SNDRV_PCM_FORMAT_LAST)){
        return -USB_EINVAL;
    }
    if (samples == 0){
        return USB_OK;
    }
    /* 物理宽度*/
    width = pcm_formats[format].phys;
    pat = pcm_formats[format].silence;
    if (! width){
        return -USB_EINVAL;
    }
    /* 有符号或1字节数据 */
    if (pcm_formats[format].signd == 1 || width <= 8) {
        uint32_t bytes = samples * width / 8;
        memset(data, *pat, bytes);
        return USB_OK;
    }
    /* non-zero samples, fill using a loop */
    width /= 8;
    dst = data;
    /* a bit optimization for constant width */
    switch (width) {
    case 2:
        while (samples--) {
            memcpy(dst, pat, 2);
            dst += 2;
        }
        break;
    case 3:
        while (samples--) {
            memcpy(dst, pat, 3);
            dst += 3;
        }
        break;
    case 4:
        while (samples--) {
            memcpy(dst, pat, 4);
            dst += 4;
        }
        break;
    case 8:
        while (samples--) {
            memcpy(dst, pat, 8);
            dst += 8;
        }
        break;
    }
    return USB_OK;
}

/** 在环形缓冲中填充静音数据*/
void uac_pcm_playback_silence(struct uac_pcm_substream *substream, uint32_t new_hw_ptr)
{
   struct uac_pcm_runtime *runtime = substream->runtime;
   uint32_t frames, ofs, transfer;

   if (runtime->silence_size < runtime->boundary) {
       int noise_dist, n;
       if (runtime->silence_start != runtime->control.appl_ptr) {
           n = runtime->control.appl_ptr - runtime->silence_start;
           if (n < 0){
               n += runtime->boundary;
           }
           if ((int)n < runtime->silence_filled){
               runtime->silence_filled -= n;
           }else{
               runtime->silence_filled = 0;
           }
           runtime->silence_start = runtime->control.appl_ptr;
       }
       if (runtime->silence_filled >= runtime->buffer_size){
           return;
       }
       noise_dist = uac_pcm_playback_hw_avail(runtime) + runtime->silence_filled;
       if (noise_dist >= (int) runtime->silence_threshold){
           return;
       }
       frames = runtime->silence_threshold - noise_dist;
       if (frames > runtime->silence_size){
           frames = runtime->silence_size;
       }
   } else {
       /* 初始化阶段*/
       if (new_hw_ptr == USB_ULONG_MAX) {
           /* 获取可用帧数*/
           int avail = uac_pcm_playback_hw_avail(runtime);
           if (avail > runtime->buffer_frame_size){
               avail = runtime->buffer_frame_size;
           }
           /* 计算不用设置静音数据的帧数*/
           runtime->silence_filled = avail > 0 ? avail : 0;
           /* 计算起始静音数据起始位置*/
           runtime->silence_start = (runtime->status.hw_ptr +
                                     runtime->silence_filled) %
                                     runtime->boundary;
       } else {
           /* 计算新完成的帧数*/
           ofs = runtime->status.hw_ptr;
           frames = new_hw_ptr - ofs;
           if ((int)frames < 0){
               frames += runtime->boundary;
           }
           /* 增加需要填充静音的帧数*/
           runtime->silence_filled -= frames;
           /* 说明最新完成的帧数已经超过已填充的帧数*/
           if ((int)runtime->silence_filled < 0) {
               /* 重新设置静音数据填充信息*/
               runtime->silence_filled = 0;
               runtime->silence_start = new_hw_ptr;
           } else {
               /* 更新静音起始位置*/
               runtime->silence_start = ofs;
           }
       }
       /* 计算要填充静音数据的帧数*/
       frames = runtime->buffer_frame_size - runtime->silence_filled;
   }
   if (frames > runtime->buffer_frame_size){
       return;
   }
   if (frames == 0){
       return;
   }
   /* 获取静音数据起始位置*/
   ofs = runtime->silence_start % runtime->buffer_frame_size;
   while (frames > 0) {
       /* 计算传输的帧数*/
       transfer = ofs + frames > runtime->buffer_frame_size ?
               runtime->buffer_frame_size - ofs : frames;
       if (substream->ops->silence) {
           int err;
           err = substream->ops->silence(substream, -1, ofs, transfer);
           if(err < 0){
               return;
           }
       } else {
           uint8_t *hwbuf = runtime->buffer + frames_to_bytes(runtime, ofs);
           /* 设置静音数据*/
           uac_pcm_format_set_silence(runtime->format_size, hwbuf, transfer * runtime->channels);
       }
       /* 更新填充静音数据的位置*/
       runtime->silence_filled += transfer;
       frames -= transfer;
       ofs = 0;
   }
}

/** 更新硬件指针*/
static usb_err_t uac_pcm_update_hw_ptr(struct uac_pcm_substream *substream, usb_bool_t in_interrupt){
    struct uac_pcm_runtime *runtime = substream->runtime;
    uint32_t old_hw_ptr, new_hw_ptr, pos, hw_base;
    int crossed_boundary = 0;
    int hdelta, delta;
    struct usb_timespec curr_tstamp;
    struct usb_timespec audio_tstamp;

    if(substream == NULL){
        return -USB_EINVAL;
    }

    /* 获取之前的硬件指针位置*/
    old_hw_ptr = runtime->status.hw_ptr;
    /* 获取当前已完成的帧的数量*/
    pos = substream->ops->pointer(substream);
    /* 获取当前时间*/
    if (runtime->tstamp_mode == USBH_SND_PCM_TSTAMP_ENABLE) {
        uac_pcm_gettime(runtime, (struct usb_timespec *)&curr_tstamp);
    }

    /* 检查是否大于缓存的最大帧数*/
    if (pos >= runtime->buffer_frame_size) {
        pos = 0;
    }
    /* 帧对齐*/
    pos -= pos % runtime->min_align;
    /* 获取重新开始的位置*/
    hw_base = runtime->hw_ptr_base;
    /* 计算新的缓存指针位置*/
    new_hw_ptr = hw_base + pos;
    if (in_interrupt == USB_TRUE) {
        /* 我们知道一个周期被处理完成*/
        /* 在中断环境中，delta为“下一个预期的缓存指针”*/
        delta = runtime->hw_ptr_interrupt + runtime->period_size;
        if (delta > new_hw_ptr) {
            /* check for double acknowledged interrupts */
//            hdelta = curr_jiffies - runtime->hw_ptr_jiffies;
//            if (hdelta > runtime->hw_ptr_buffer_jiffies/2 + 1) {
//                hw_base += runtime->buffer_size;
//                if (hw_base >= runtime->boundary) {
//                    hw_base = 0;
//                    crossed_boundary++;
//                }
//                new_hw_ptr = hw_base + pos;
//                goto __delta;
//            }
        }
    }
    /* 当跨过环形缓冲时，新的缓存指针可能小于旧的缓存指针*/
    if (new_hw_ptr < old_hw_ptr) {
        hw_base += runtime->buffer_frame_size;
        /* 跨过边界了*/
        if (hw_base >= runtime->boundary) {
            hw_base = 0;
            crossed_boundary++;
        }
        new_hw_ptr = hw_base + pos;
    }
__delta:
    /* 计算新完成的帧数*/
    delta = new_hw_ptr - old_hw_ptr;
    /* 跨过边界*/
    if (delta < 0){
        delta += runtime->boundary;
    }
    if ((delta >= runtime->buffer_frame_size + runtime->period_size)) {
        __UAC_PCM_TRACE("Unexpected hw_ptr"
                "(stream=%i, pos=%ld, new_hw_ptr=%ld, old_hw_ptr=%ld)\r\n",
                substream->stream, (long)pos,
                (long)new_hw_ptr, (long)old_hw_ptr);
        return -USB_EPERM;
    }
    /* 更新音频时间戳*/
    if(runtime->status.hw_ptr == new_hw_ptr){
        update_audio_tstamp(substream, &curr_tstamp, &audio_tstamp);
        return USB_OK;
    }
    /* 填充静音数据*/
    if ((substream->stream == USBH_SND_PCM_STREAM_PLAYBACK) &&
        (runtime->silence_size > 0)){
        uac_pcm_playback_silence(substream, new_hw_ptr);
    }
    if (in_interrupt) {
        delta = new_hw_ptr - runtime->hw_ptr_interrupt;
        if (delta < 0){
            delta += runtime->boundary;
        }
        delta -= (uint32_t)delta % runtime->period_size;
        runtime->hw_ptr_interrupt += delta;
        if (runtime->hw_ptr_interrupt >= runtime->boundary){
            runtime->hw_ptr_interrupt -= runtime->boundary;
        }
    }
    /* 更新缓存指针位置*/
    runtime->hw_ptr_base = hw_base;
    runtime->status.hw_ptr = new_hw_ptr;
    /* 是否跨过边界*/
    if (crossed_boundary) {
        /* 跨边界次数大于1，非法*/
        if(crossed_boundary != 1){
            __UAC_PCM_TRACE("cross boundary != 1\r\n");
        }
        runtime->hw_ptr_wrap += runtime->boundary;
    }
    /* 更新时间戳*/
    update_audio_tstamp(substream, &curr_tstamp, &audio_tstamp);
    /* 更新PCM状态*/
    return uac_pcm_update_state(substream, runtime);
}

/**
 * 为下一个周期更新PCM状态
 *
 * 当PCM处理完一个周期大小的数据时，更新当前的指针
 */
void uac_pcm_period_elapsed(struct uac_pcm_substream *substream){
    if(substream == NULL){
        return;
    }
    /* 检查是否还在运行中*/
    if(!uac_pcm_running(substream)){
        return;
    }
    /* 更新硬件指针*/
    uac_pcm_update_hw_ptr(substream, USB_TRUE);
}

/** 复位PCM*/
usb_err_t uac_pcm_reset(struct uac_pcm_substream *substream, int state, usb_bool_t complete_reset)
{
    struct uac_pcm_runtime *runtime = substream->runtime;
    /* 检查状态*/
    if(complete_reset == USB_TRUE){
        switch (runtime->status.state) {
        case UAC_PCM_STATE_RUNNING:
        case UAC_PCM_STATE_PREPARED:
        case UAC_PCM_STATE_PAUSED:
        case UAC_PCM_STATE_SUSPENDED:
            break;
        default:
            return -USB_EPERM;
        }
    }
    if (uac_pcm_running(substream) &&
        uac_pcm_update_hw_ptr(substream, USB_FALSE) >= 0){
        runtime->status.hw_ptr %= runtime->buffer_size;
    }else {
        runtime->status.hw_ptr = 0;
        runtime->hw_ptr_wrap = 0;
    }
    /* 启动帧指针位置*/
    runtime->hw_ptr_base = 0;
    runtime->hw_ptr_interrupt = runtime->status.hw_ptr -
        runtime->status.hw_ptr % runtime->period_size;
    /* 静音起始帧指针*/
    runtime->silence_start = runtime->status.hw_ptr;
    /* 已填充静音数据大小*/
    runtime->silence_filled = 0;

    if(complete_reset == USB_TRUE){
        /* 设置用户帧指针*/
        runtime->control.appl_ptr = runtime->status.hw_ptr;
        if ((substream->stream == USBH_SND_PCM_STREAM_PLAYBACK) &&
                (runtime->silence_size > 0)){
            uac_pcm_playback_silence(substream, USB_ULONG_MAX);
        }
    }
    return USB_OK;
}

/** 停止PCM*/
usb_err_t uac_pcm_stop(struct uac_pcm_substream *substream, uac_pcm_state_t state){
    if((substream == NULL) || (substream->runtime == NULL)){
        return -USB_EINVAL;
    }
    struct uac_pcm_runtime *runtime = substream->runtime;

    if (runtime->status.state == UAC_PCM_STATE_OPEN){
        return -USB_EPERM;
    }
    if (uac_pcm_running(substream)){
        substream->ops->trigger(substream, UAC_PCM_TRIGGER_STOP);
    }

    return USB_OK;
}

/**
 * 暂停/恢复数据流
 *
 * @param substream USB音频PCM子数据流
 * @param push      1：暂停；0：恢复
 *
 * @return 成功返回USB_OK
 */
usb_err_t uac_pcm_pause(struct uac_pcm_substream *substream, int push){
    usb_err_t ret;
    struct uac_pcm_runtime *runtime = substream->runtime;

    /* 检查数据流当前状态*/
    if (push) {
        if (runtime->status.state != UAC_PCM_STATE_RUNNING){
            return -USB_EPERM;
        }
    } else if (runtime->status.state != UAC_PCM_STATE_PAUSED){
        return -USB_EPERM;
    }
    /* 暂停操作，更新硬件指针*/
    if (push){
        uac_pcm_update_hw_ptr(substream, USB_FALSE);
    }

    ret = substream->ops->trigger(substream, push ? UAC_PCM_TRIGGER_PAUSE_PUSH :
                                                    UAC_PCM_TRIGGER_PAUSE_RELEASE);
    if(ret != USB_OK){
        substream->ops->trigger(substream, push ? UAC_PCM_TRIGGER_PAUSE_RELEASE :
                                                  UAC_PCM_TRIGGER_PAUSE_PUSH);
        return ret;
    }

    if (push){
        runtime->status.state = UAC_PCM_STATE_PAUSED;
    }
    else{
        runtime->status.state = UAC_PCM_STATE_RUNNING;
    }
    return USB_OK;
}

usb_err_t uac_pcm_drop(struct uac_pcm_substream *substream)
{
    struct uac_pcm_runtime *runtime;

    if ((substream == NULL) || (substream->runtime == NULL)){
        return -USB_EINVAL;
    }
    runtime = substream->runtime;

    if ((runtime->status.state == UAC_PCM_STATE_OPEN) ||
        (runtime->status.state == UAC_PCM_STATE_DISCONNECTED) ||
        (runtime->status.state == UAC_PCM_STATE_SUSPENDED)){
        return -USB_EPERM;
    }

    /* 恢复暂停*/
    if (runtime->status.state == UAC_PCM_STATE_PAUSED){
        uac_pcm_pause(substream, 0);
    }
    /* 停止PCM数据流*/
    uac_pcm_stop(substream, UAC_PCM_STATE_SETUP);

    return USB_OK;
}
/** 启动PCM*/
usb_err_t uac_pcm_start(struct uac_pcm_substream *substream){
    usb_err_t ret;

    struct uac_pcm_runtime *runtime = substream->runtime;
    if (runtime->status.state != UAC_PCM_STATE_PREPARED){
        return -USB_EPERM;
    }
    /* 检查运行缓存里是否有数据*/
    if ((substream->stream == USBH_SND_PCM_STREAM_PLAYBACK) &&
        !uac_pcm_playback_data(substream)){
        return -USB_EPIPE;
    }
    /* 调用触发函数*/
    ret = substream->ops->trigger(substream, UAC_PCM_TRIGGER_START);
    if(ret != USB_OK){
        substream->ops->trigger(substream, UAC_PCM_TRIGGER_STOP);
        return ret;
    }
    runtime->status.state = UAC_PCM_STATE_RUNNING;
    /* 填充静音数据*/
    if ((substream->stream == USBH_SND_PCM_STREAM_PLAYBACK) &&
        (runtime->silence_size > 0)){
        uac_pcm_playback_silence(substream, USB_ULONG_MAX);
    }
    return USB_OK;
}

/**
 * 等待avail_min数量的帧可用
 *
 * @return 返回的帧储存在availp中
 */
static usb_err_t wait_for_avail(struct uac_pcm_substream *substream,
                                uint32_t *availp){
    struct uac_pcm_runtime *runtime = substream->runtime;
    int is_playback = substream->stream == USBH_SND_PCM_STREAM_PLAYBACK;
    usb_err_t ret = 0;
    uint32_t avail = 0;
    int wait_time;

    wait_time = 10;
    if (runtime->rate) {
        int t = runtime->period_size * 2 / runtime->rate;
        wait_time = max(t, wait_time);
    }

    usb_mutex_lock(substream->mutex, USB_WAIT_FOREVER);
    while(1){
        /* 检查是否已经有可用的帧*/
        if (is_playback){
            avail = uac_pcm_playback_avail(runtime);
        }else{
            avail = uac_pcm_capture_avail(runtime);
        }
        if (avail >= runtime->twake)
            break;
        usb_mutex_unlock(substream->mutex);
        usb_mdelay(wait_time);
        usb_mutex_lock(substream->mutex, USB_WAIT_FOREVER);
        /* 检查状态*/
        switch (runtime->status.state) {
        case UAC_PCM_STATE_SUSPENDED:
            ret = -USB_EPERM;
            goto __endloop;
        case UAC_PCM_STATE_XRUN:
            ret = -USB_EPIPE;
            goto __endloop;
        case UAC_PCM_STATE_DRAINING:
            if (is_playback){
                ret = -USB_EPIPE;
            }else{
                avail = 0; /* indicate draining */
            }
            goto __endloop;
        case UAC_PCM_STATE_OPEN:
        case UAC_PCM_STATE_SETUP:
        case UAC_PCM_STATE_DISCONNECTED:
            ret = -USB_ENODEV;
            goto __endloop;
        case UAC_PCM_STATE_PAUSED:
            continue;
        }
    }
__endloop:
    usb_mutex_unlock(substream->mutex);
    *availp = avail;
    return ret;
}

/**
 * 复制用户数据
 *
 * @param substream PCM子数据流结构体
 * @param hwoff     运行内存的帧偏移
 * @param data      用户数据缓存
 * @param off       用户数据的帧偏移
 * @param frames    要复制的帧数
 */
static void uac_pcm_write_transfer(struct uac_pcm_substream *substream,
                                       uint32_t hwoff,
                                       uint8_t *data, uint32_t off,
                                       uint32_t frames){
    struct uac_pcm_runtime *runtime = substream->runtime;
    /* 用户缓存偏移*/
    uint8_t *buf = (uint8_t *) data + frames_to_bytes(runtime, off);
    /* 运行时间缓存偏移*/
    uint8_t *hwbuf = runtime->buffer + frames_to_bytes(runtime, hwoff);
    memcpy(hwbuf, buf, frames_to_bytes(runtime, frames));
}

/**
 * USB音频PCM写函数
 *
 * @param substream   PCM子数据流结构体
 * @param buffer      缓存
 * @param buffer_size 缓存的帧大小
 *
 * @param 成功返回成功传输的帧数
 */
int uac_pcm_write(struct uac_pcm_substream *substream,
                  uint8_t *buffer, uint32_t buffer_frame_size){
    struct uac_pcm_runtime *runtime = substream->runtime;
    usb_err_t ret = USB_OK;
    uint32_t avail;
    uint32_t xfer = 0;
    uint32_t offset = 0;

    if (buffer_frame_size == 0){
        return 0;
    }
    /* 检查状态*/
    if((runtime->status.state != UAC_PCM_STATE_PREPARED) &&
            (runtime->status.state != UAC_PCM_STATE_RUNNING) &&
            (runtime->status.state != UAC_PCM_STATE_PAUSED)){
        ret = -USB_EPERM;
        goto __end;
    }

    /* 设置唤醒帧数*/
    runtime->twake = runtime->control.avail_min ? : 1;
    /* 运行过程中，需要更新硬件指针*/
    if (runtime->status.state == UAC_PCM_STATE_RUNNING){
        uac_pcm_update_hw_ptr(substream, USB_FALSE);
    }
    avail = uac_pcm_playback_avail(runtime);
    while (buffer_frame_size > 0) {
        uint32_t frames, appl_ptr, appl_ofs;;
        uint32_t cont;
        /* 没有可用的帧*/
        if (avail == 0) {
            runtime->twake = min(buffer_frame_size, runtime->control.avail_min ? : 1);
            /* 等待有可用的帧*/
            ret = wait_for_avail(substream, &avail);
            if (ret < 0){
                goto __end;
            }
        }
        /* 计算要发送的帧数*/
        frames = buffer_frame_size > avail ? avail : buffer_frame_size;
        /* 计算可用的帧数*/
        cont = runtime->buffer_frame_size -
                (runtime->control.appl_ptr % runtime->buffer_frame_size);
        if (frames > cont){
            frames = cont;
        }
        if (frames == 0){
            runtime->twake = 0;
            return -USB_EPERM;
        }
        /* 用户指针*/
        appl_ptr = runtime->control.appl_ptr;
        /* 用户指针在运行内存里的偏移*/
        appl_ofs = appl_ptr % runtime->buffer_frame_size;
        /* 复制用户数据*/
        usb_mutex_lock(substream->mutex, 5000);
        uac_pcm_write_transfer(substream, appl_ofs, buffer, offset, frames);
        usb_mutex_unlock(substream->mutex);
        /* 设置用户缓存指针*/
        appl_ptr += frames;
        /* 到达边界*/
        if (appl_ptr >= runtime->boundary){
            appl_ptr -= runtime->boundary;
        }
        runtime->control.appl_ptr = appl_ptr;
        /* 更新用户缓存偏移*/
        offset += frames;
        /* 更新用户帧缓存大小*/
        buffer_frame_size -= frames;
        /* 更新成功传输的帧大小*/
        xfer += frames;
        /* 更新可用帧的数量*/
        avail -= frames;
        /* 没启动传输，调用启动函数*/
        if ((runtime->status.state == UAC_PCM_STATE_PREPARED) &&
            uac_pcm_playback_hw_avail(runtime) >= (int)runtime->start_threshold) {
            ret = uac_pcm_start(substream);
            if (ret != USB_OK){
                goto __end;
            }
        }
    }
__end:
    runtime->twake = 0;
    if ((xfer > 0) && (ret >= 0)){
        uac_pcm_update_state(substream, runtime);
    }
    return xfer > 0 ? (int)xfer : ret;
}

/** 设置运行时间硬件信息*/
static usb_err_t setup_hw_info(struct uac_pcm_runtime *runtime,
                               struct usbh_uac_substream *subs)
{
    struct audioformat *fp;
    uint32_t pt, ptmin;
    int param_period_time_if_needed;
    usb_err_t ret;

    runtime->hw.formats = subs->formats;

    runtime->hw.rate_min = 0x7fffffff;
    runtime->hw.rate_max = 0;
    runtime->hw.channels_min = 256;
    runtime->hw.channels_max = 0;
    runtime->hw.rates = 0;
    ptmin = USB_UINT_MAX;

    /* 检查最小/最大采样率和通道数*/
    usb_list_for_each_entry(fp, &subs->fmt_list, struct audioformat, list) {
        runtime->hw.rates |= fp->rates;
        if (runtime->hw.rate_min > fp->rate_min)
            runtime->hw.rate_min = fp->rate_min;
        if (runtime->hw.rate_max < fp->rate_max)
            runtime->hw.rate_max = fp->rate_max;
        if (runtime->hw.channels_min > fp->channels)
            runtime->hw.channels_min = fp->channels;
        if (runtime->hw.channels_max < fp->channels)
            runtime->hw.channels_max = fp->channels;
        if ((fp->fmt_type == UAC_FORMAT_TYPE_II) && (fp->frame_size > 0)) {
            /* FIXME: there might be more than one audio formats... */
            runtime->hw.period_bytes_min = runtime->hw.period_bytes_max =
                fp->frame_size;
        }
        pt = 125 * (1 << fp->datainterval);
        ptmin = min(ptmin, pt);
    }
    param_period_time_if_needed = UAC_PCM_HW_PARAM_PERIOD_TIME;
    /* 全速设备需要修改数据包间隔*/
    if (subs->speed == USB_SPEED_FULL){
        ptmin = 1000;
    }
    /* 如果周期时间不低于1ms，不需要规则*/
    if (ptmin == 1000){
        param_period_time_if_needed = -1;
    }
    /* 设置周期时间最小最大值*/
    uac_pcm_hw_constraint_minmax(runtime, UAC_PCM_HW_PARAM_PERIOD_TIME,
                                 ptmin, USB_UINT_MAX);
    /* 添加采样率规则*/
    if ((ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_RATE,
                                   hw_rule_rate, subs,
                                   UAC_PCM_HW_PARAM_FORMAT,
                                   UAC_PCM_HW_PARAM_CHANNELS,
                                   param_period_time_if_needed)) != USB_OK){
        return ret;
    }
    /* 添加通道数规则*/
    if ((ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_CHANNELS,
                                   hw_rule_channels, subs,
                                   UAC_PCM_HW_PARAM_FORMAT,
                                   UAC_PCM_HW_PARAM_RATE,
                                   param_period_time_if_needed)) != USB_OK){
        return ret;
    }
    /* 添加格式规则*/
    if ((ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_FORMAT,
                                   hw_rule_format, subs,
                                   UAC_PCM_HW_PARAM_RATE,
                                   UAC_PCM_HW_PARAM_CHANNELS,
                                   param_period_time_if_needed)) != USB_OK){
        return ret;
    }
    if (param_period_time_if_needed >= 0) {
        /* 添加周期时间规则*/
        ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_PERIOD_TIME,
                                  hw_rule_period_time, subs,
                                  UAC_PCM_HW_PARAM_FORMAT,
                                  UAC_PCM_HW_PARAM_CHANNELS,
                                  UAC_PCM_HW_PARAM_RATE);
        if (ret != USB_OK){
            return ret;
        }
    }
    return USB_OK;
}

/* PCM 硬件信息*/
static struct uac_pcm_hardware usbh_uac_hardware =
{
    .buffer_bytes_max = 1024 * 1024,
    .period_bytes_min = 64,
    .period_bytes_max = 512 * 1024,
    .periods_min =      2,
    .periods_max =      1024,
};

/**
 * PCM打开(初始化)函数
 *
 * @param substream PCM子流结构体
 * @param direction 方向(回放或捕获)
 *
 * @return 成功返回USB_OK
 */
static usb_err_t usbh_uac_pcm_open(struct uac_pcm_substream *substream, int direction)
{
    struct usbh_uac_stream *as = uac_pcm_substream_chip(substream);
    struct uac_pcm_runtime *runtime = substream->runtime;
    struct usbh_uac_substream *subs = &as->substream[direction];

    subs->interface = -1;
    subs->altsetting = 0;
    runtime->hw = usbh_uac_hardware;
    runtime->private_data = subs;
    subs->pcm_substream = substream;

    /* 初始化 DSD/DOP 环境*/
    subs->dsd_dop.byte_idx = 0;
    subs->dsd_dop.channel = 0;
    subs->dsd_dop.marker = 1;
    /* 设置硬件信息*/
    return setup_hw_info(runtime, subs);
}

/** 关闭一个PCM*/
static usb_err_t usbh_uac_pcm_close(struct uac_pcm_substream *substream, int direction)
{
    struct usbh_uac_stream *as = uac_pcm_substream_chip(substream);
    struct usbh_uac_substream *subs = &as->substream[direction];

    /* 停止端点*/
    uac_stop_endpoints(subs, USB_TRUE);

    if (!as->chip->shutdown && subs->interface >= 0) {
        usbh_set_interface(subs->p_fun, subs->interface, 0);
        subs->interface = -1;
    }

    subs->pcm_substream = NULL;

    return USB_OK;
}

/** 打开一个回放数据流*/
static usb_err_t usbh_uac_playback_open(struct uac_pcm_substream *substream)
{
    return usbh_uac_pcm_open(substream, USBH_SND_PCM_STREAM_PLAYBACK);
}

/** 打开一个捕获数据流*/
static usb_err_t usbh_uac_capture_open(struct uac_pcm_substream *substream)
{
    return usbh_uac_pcm_open(substream, USBH_SND_PCM_STREAM_CAPTURE);
}

/** 关闭一个回放数据流*/
static usb_err_t usbh_uac_playback_close(struct uac_pcm_substream *substream)
{
    return usbh_uac_pcm_close(substream, USBH_SND_PCM_STREAM_PLAYBACK);
}

extern struct audioformat *find_format(struct usbh_uac_substream *subs);
extern usb_err_t set_format(struct usbh_uac_substream *subs, struct audioformat *fmt);
/**
 * USB音频设备硬件参数设置
 *
 * @param substream PCM子流结构体
 * @param hw_params 硬件参数结构体
 *
 * @return 成功返回USB_OK
 */
static usb_err_t usbh_uac_hw_params(struct uac_pcm_substream *substream,
                                    struct uac_pcm_hw_params *hw_params){
    struct usbh_uac_substream *subs = substream->runtime->private_data;
    struct uac_pcm_runtime *runtime = NULL;
    struct audioformat *fmt;
    usb_err_t ret;

    if((hw_params == NULL) || (substream == NULL) || (substream->runtime == NULL)){
        return -USB_EINVAL;
    }

    runtime = substream->runtime;
    /* 初始化缓存*/
    if(runtime->buffer == NULL){
        runtime->buffer = usb_mem_alloc(params_buffer_bytes(hw_params));
        if(runtime->buffer == NULL){
            return -USB_ENOMEM;
        }
        memset(runtime->buffer ,0, params_buffer_bytes(hw_params));
        runtime->buffer_size = params_buffer_bytes(hw_params);
    }

    /* 获取格式位宽*/
    subs->pcm_format_size = hw_params->pcm_format_size_bit;
    if(subs->pcm_format_size < 0){
        return -USB_EINVAL;
    }

    /* 设置通道数*/
    subs->channels = params_channels(hw_params);
    /* 设置采样率*/
    subs->cur_rate = params_rate(hw_params);
    /* 一个缓存里的周期数*/
    subs->buffer_periods = params_periods(hw_params);
    /* 一个周期的帧数*/
    subs->period_frames = params_period_size(hw_params);
    /* 一个周期有多少字节*/
    subs->period_bytes = params_period_bytes(hw_params);

    /* 寻找格式*/
    fmt = find_format(subs);
    if (fmt == NULL) {
        __UAC_PCM_TRACE("cannot set format: format size = %#x, rate = %d, channels = %d\r\n",
               subs->pcm_format_size, subs->cur_rate, subs->channels);
        return -USB_EINVAL;
    }
    /* 设置格式*/
    if (subs->stream->chip->shutdown){
        ret = -USB_ENODEV;
    }else{
        ret = set_format(subs, fmt);
    }

    if (ret != USB_OK){
        return ret;
    }
    subs->interface = fmt->iface;
    subs->altsetting = fmt->altsetting;
    subs->need_setup_ep = USB_TRUE;

    return USB_OK;
}

/** 释放硬件参数，复位音频格式和释放缓存*/
static usb_err_t usbh_uac_hw_free(struct uac_pcm_substream *substream)
{
    struct usbh_uac_substream *subs = substream->runtime->private_data;
    struct uac_pcm_runtime *runtime;

    if((substream == NULL) || (substream->runtime == NULL)){
        return -USB_EINVAL;
    }

    runtime = substream->runtime;
    subs->cur_audiofmt = NULL;
    subs->cur_rate = 0;
    subs->period_bytes = 0;

    /* 释放缓存*/
    if(runtime->buffer){
        usb_mem_free(runtime->buffer);
    }
    runtime->buffer = NULL;
    return USB_OK;
}

/** PCM准备函数*/
static usb_err_t usbh_uac_pcm_prepare(struct uac_pcm_substream *substream)
{
    struct uac_pcm_runtime *runtime = substream->runtime;
    struct usbh_uac_substream *subs = runtime->private_data;
    struct usbh_interface *iface = NULL;
    usb_err_t ret;

    /* 检查是否存在格式*/
    if (subs->cur_audiofmt == NULL) {
        __UAC_PCM_TRACE("no format is specified!\r\n");
        return -USB_ENXIO;
    }
    if (subs->stream->chip->shutdown) {
        return -USB_ENODEV;
    }
    if (subs->data_endpoint == NULL) {
        return -USB_EIO;
    }
    /* 设置格式*/
    ret = set_format(subs, subs->cur_audiofmt);
    if (ret != USB_OK){
        return ret;
    }

    /* 获取需要的接口设置*/
    iface = usbh_ifnum_to_if(subs->p_fun->p_udev, subs->cur_audiofmt->iface,
                             subs->cur_audiofmt->altsetting);
    /* 初始化采样率*/
    ret = usbh_uac_init_sample_rate(subs->stream->chip,
                                    subs,
                                    subs->cur_audiofmt->iface,
                                    iface,
                                    subs->cur_audiofmt,
                                    subs->cur_rate);
    if (ret != USB_OK){
        return ret;
    }
    /* 配置端点*/
    if (subs->need_setup_ep == USB_TRUE) {
        ret = uac_configure_endpoint(subs);
        if (ret != USB_OK){
            return ret;
        }
        subs->need_setup_ep = USB_FALSE;
    }

    subs->data_endpoint->maxframesize =
        bytes_to_frames(runtime, subs->data_endpoint->maxpacksize);
    subs->data_endpoint->curframesize =
        bytes_to_frames(runtime, subs->data_endpoint->curpacksize);

    /* 复位指针*/
    subs->hwptr_done = 0;
    subs->transfer_done = 0;
    subs->last_delay = 0;
    subs->last_frame_number = 0;
    runtime->delay = 0;

    /* 回放模式，现在提交传输请求包*/
    if (subs->direction == USBH_SND_PCM_STREAM_PLAYBACK){
        ret = uac_start_endpoints(subs, USB_TRUE);
    }
    return USB_OK;
}

/**
 * USB音频设备回放触发函数
 *
 * @param substream PCM子流结构体
 * @param cmd       触发命令
 *
 * @return 成功返回USB_OK
 */
static usb_err_t usbh_uac_substream_playback_trigger(struct uac_pcm_substream *substream,
                                                     int cmd)
{
    struct usbh_uac_substream *subs = substream->runtime->private_data;

    switch (cmd) {
    case UAC_PCM_TRIGGER_START:
        subs->trigger_tstamp_pending_update = USB_TRUE;
    case UAC_PCM_TRIGGER_PAUSE_RELEASE:
        subs->data_endpoint->prepare_data_trp = prepare_playback_trp;
        subs->data_endpoint->retire_data_trp = retire_playback_trp;
        subs->running = USB_TRUE;
        return USB_OK;
    case UAC_PCM_TRIGGER_STOP:
        uac_stop_endpoints(subs, USB_FALSE);
        subs->running = USB_FALSE;
        return USB_OK;
    case UAC_PCM_TRIGGER_PAUSE_PUSH:
        subs->data_endpoint->prepare_data_trp = NULL;
        subs->data_endpoint->retire_data_trp = retire_playback_trp;
        subs->running = USB_FALSE;
        return USB_OK;
    }
    return -USB_EINVAL;
}

/** 获取当前PCM指针*/
static int usbh_uac_pcm_pointer(struct uac_pcm_substream *substream)
{
    struct usbh_uac_substream *subs;
    uint32_t hwptr_done;

    subs = (struct usbh_uac_substream *)substream->runtime->private_data;
    if (subs->stream->chip->shutdown){
        return -1;
    }

    hwptr_done = subs->hwptr_done;
    /* 计算延迟*/
    substream->runtime->delay = usbh_uac_pcm_delay(subs, substream->runtime->rate);
    /* 转换为帧*/
    return hwptr_done / (substream->runtime->frame_bits >> 3);
}

///** 设置帧缓冲大小*/
//static usb_err_t uac_pcm_lib_fifo_size(struct uac_pcm_substream *substream,
//                                       void *arg)
//{
//    struct uac_pcm_hw_params *params = arg;
//    uac_pcm_format_t format_size_bit;
//    int channels;
//    uint32_t frame_size;
//
//    params->fifo_size = substream->runtime->hw.fifo_size;
//    format_size_bit = params->pcm_format_size_bit;
//    channels = params_channels(params);
//    frame_size = uac_pcm_format_size(format_size_bit, channels);
//    if (frame_size > 0){
//        params->fifo_size /= (uint32_t)frame_size;
//    }
//    return USB_OK;
//}

/* 捕获音频操作函数集*/
static struct uac_pcm_ops usbh_uac_capture_ops = {
        .open      = usbh_uac_capture_open,
};

/* 回放音频操作函数集*/
static struct uac_pcm_ops usbh_uac_playback_ops = {
        .open      = usbh_uac_playback_open,
        .close     = usbh_uac_playback_close,
        .hw_params = usbh_uac_hw_params,
        .hw_free   = usbh_uac_hw_free,
        .prepare   = usbh_uac_pcm_prepare,
        .trigger   = usbh_uac_substream_playback_trigger,
        .pointer   = usbh_uac_pcm_pointer,
        //.fifo_size = uac_pcm_lib_fifo_size,
};

/**
 * 设置PCM操作函数集
 *
 * @param pcm       PCM实体
 * @param direction 流方向
 * @param ops       操作函数集
 */
static void uac_pcm_set_ops(struct uac_pcm *pcm, int direction,
                            const struct uac_pcm_ops *ops)
{
    struct uac_pcm_str *stream = &pcm->streams[direction];
    struct uac_pcm_substream *substream;

    for (substream = stream->substream; substream != NULL;
            substream = substream->next){
        substream->ops = ops;
    }
}

/** 设置PCM操作函数集*/
void usbh_uac_set_pcm_ops(struct uac_pcm *pcm, int stream)
{
  uac_pcm_set_ops(pcm, stream,
          stream == USBH_SND_PCM_STREAM_PLAYBACK ?
          &usbh_uac_playback_ops : &usbh_uac_capture_ops);
}

/** 释放音频PCM*/
void usbh_uac_pcm_free(struct uac_pcm *pcm)
{
    int i;
    struct uac_pcm_substream *substream;

    if (pcm == NULL){
        return;
    }

    /* 释放PCM子数据流*/
    for(i = 0;i < 2;i++){
        substream = pcm->streams[i].substream;
        while(substream != NULL){
            if(substream->refcnt > 0){
                usb_refcnt_put(&substream->refcnt, uac_pcm_release_substream);
            }
            substream = substream->next;
        }
    }
    usb_mem_free(pcm);
}

/** 创建一个新的PCM*/
usb_err_t uac_pcm_new(const char *id, int device, int playback_count,
                    int capture_count, struct uac_pcm **rpcm){
  struct uac_pcm *pcm = NULL;
  usb_err_t err;

  if (rpcm){
      *rpcm = NULL;
  }
  pcm = usb_mem_alloc(sizeof(struct uac_pcm));
  if (pcm == NULL){
      return -USB_ENOMEM;
  }
  memset(pcm, 0, sizeof(struct uac_pcm));

  pcm->device = device;
//    pcm->open_mutex = usb_mutex_create();
//    if(pcm->open_mutex == NULL){
//        return -USB_ENOMEM;
//    }
//    USB_INIT_LIST_HEAD(&pcm->list);
  /* 复制ID*/
  if (id){
      strlcpy(pcm->id, id, sizeof(pcm->id));
  }
  /* 创建回放PCM数据流*/
  if ((err = uac_pcm_new_stream(pcm, USBH_SND_PCM_STREAM_PLAYBACK, playback_count)) < 0) {
      usbh_uac_pcm_free(pcm);
      return err;
  }
  /* 创建捕获PCM数据流*/
  if ((err = uac_pcm_new_stream(pcm, USBH_SND_PCM_STREAM_CAPTURE, capture_count)) < 0) {
      usbh_uac_pcm_free(pcm);
      return err;
  }
  if (rpcm){
      *rpcm = pcm;
  }
  return USB_OK;
}


