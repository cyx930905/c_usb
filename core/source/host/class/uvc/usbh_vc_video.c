/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/uvc/usbh_vc_drv.h"
#include "core/include/host/class/uvc/usbh_vc_stream.h"
#include "core/include/host/class/uvc/usbh_vc_entity.h"

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbh_vc_lib __g_uvc_lib;
extern int usb_hc_frame_num_get(struct usb_hc *p_hc);
extern int usbh_vc_stream_xfer_cancel(struct usbh_vc_stream *p_stream);
extern int usbh_vc_video_ctrl_get(struct usbh_vc_stream  *p_stream,
                                  struct uvc_stream_ctrl *p_ctrl,
                                  int                     probe,
                                  uint8_t                 query);
extern int usbh_vc_video_ctrl_set(struct usbh_vc_stream  *p_stream,
                                  struct uvc_stream_ctrl *p_ctrl,
                                  int                     probe);

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 检查 USB 视频类缓存
 */
static void __vc_video_buffer_validate(struct usbh_vc_stream *p_stream,
                                       struct usbh_vc_buffer *p_buf){
    /* 非压缩格式并且接收到数据量不等于帧大小，记录缓存错误*/
    if ((p_stream->ctrl.max_video_frame_size != p_buf->bytes_used) &&
        !(p_stream->p_format_cur->flags & UVC_FMT_FLAG_COMPRESSED)) {
        p_buf->error_no = 1;
    }
}

/**
 * \brief 获取缓存队列中下一个可用缓存
 */
static struct usbh_vc_buffer *__vc_queue_next_buffer(struct usbh_vc_queue  *p_queue,
                                                     struct usbh_vc_buffer *p_buf){
#if USB_OS_EN
    int                    ret;
#endif
    struct usbh_vc_buffer *p_buf_next = NULL;

    if ((p_queue->flags & UVC_QUEUE_DROP_CORRUPTED) && (p_buf->error_no)) {
        p_buf->error_no   = 0;
        p_buf->state      = UVC_BUF_STATE_QUEUED;
        p_buf->bytes_used = 0;
        return p_buf;
    }

    p_buf->state = p_buf->error_no ? UVC_BUF_STATE_ERROR : UVC_BUF_STATE_DONE;
    if (p_buf->state != UVC_BUF_STATE_DONE) {
        p_buf->error_no   = 0;
        p_buf->state      = UVC_BUF_STATE_QUEUED;
        p_buf->bytes_used = 0;
        return p_buf;
    } else {
#if USB_OS_EN
        ret = usb_mutex_lock(p_queue->p_lock, UVC_LOCK_TIMEOUT);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
            return NULL;
        }
#endif
        usb_list_node_move_tail(&p_buf->node, &p_queue->irqqueue);
        if (!usb_list_head_is_empty(&p_queue->irqqueue)) {
            p_buf_next = usb_container_of(p_queue->irqqueue.p_next, struct usbh_vc_buffer, node);
        } else {
            p_buf_next = NULL;
        }
#if USB_OS_EN
        ret = usb_mutex_unlock(p_queue->p_lock);
    	if (ret != USB_OK) {
        	__USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        	return NULL;
    	}
#endif
        p_buf_next->error_no   = 0;
        p_buf_next->bytes_used = 0;
        p_buf_next->state      = UVC_BUF_STATE_QUEUED;
    }
    return p_buf_next;
}

/**
 * \brief USB 视频类解码视频时钟
 */
static void __vc_video_clock_decode(struct usbh_vc_stream *p_stream,
                                    struct usbh_vc_buffer *p_buf,
                                    uint8_t               *p_data,
                                    int                    len){
    struct uvc_clock_sample *p_sample = NULL;
    int                      header_size;
    usb_bool_t               has_pts  = USB_FALSE;
    usb_bool_t               has_scr  = USB_FALSE;
    struct usb_timespec      ts;
    uint16_t                 host_sof;
    uint16_t                 dev_sof;
    struct usb_hc           *p_hc     = p_stream->p_vc->p_ufun->p_usb_dev->p_hc;
#if USB_OS_EN
    int                      ret;
#endif

    switch (p_data[1] & (UVC_STREAM_PTS | UVC_STREAM_SCR)) {
        case UVC_STREAM_PTS | UVC_STREAM_SCR:
            header_size = 12;
            has_pts = USB_TRUE;
            has_scr = USB_TRUE;
            break;
        case UVC_STREAM_PTS:
            header_size = 6;
            has_pts = USB_TRUE;
            break;
        case UVC_STREAM_SCR:
            header_size = 8;
            has_scr = USB_TRUE;
            break;
        default:
            header_size = 2;
            break;
    }
    /* 检查无效的头部*/
    if (len < header_size) {
        return;
    }
    /* 提取时间戳，在缓存结构体里存储帧的显示时间戳*/
    if (has_pts && p_buf != NULL) {
        p_buf->pts = usb_get_unaligned_le32(&p_data[2]);
    }
    if (!has_scr) {
        return;
    }
    /* 若要限制数据量，请删除具有与前一个相同的 SOF的 SCRs */
    dev_sof = usb_get_unaligned_le16(&p_data[header_size - 2]);
    if (dev_sof == p_stream->clock.last_sof) {
        return;
    }

    p_stream->clock.last_sof = dev_sof;

    host_sof = usb_hc_frame_num_get(p_hc);

    usb_timespec_get(&ts);

    /* USB 视频类规范允许无法获得 USB 帧号的设备实现保留自己的帧计数器，只要它们与 USB SOF 令牌相关联的帧号的大小和
     * 帧率匹配。这些设备发送的 SOF 值与 USB SOF 令牌的不同在于需要对其进行估计和说明，以使时间戳恢复尽可能准确
     *
     * 第一次接收到设备的 SOF 值作为主机和设备 SOF 值之间的差估计偏移量。 由于两个 SOF 值可能由于传输延迟而略有不同。
     * 因此如果差异不大于 10ms，则认为偏移量为空(不能出现负差异，因此被视为偏移量)。 视频提交控件计算动态阈值，而不是
     * 使用固定的 10ms 的值，但设备不会报告可靠的 delay 值。
     *
     * 有关 1 为什么只保留 delta 低 8 位的解释，请参见 uvc_video_clock_host_sof().*/
    if (p_stream->clock.sof_offset == (int16_t)-1) {
    //if (p_stream->clock.sof_offset == (uint16_t)-1) {
        uint16_t delta_sof = (host_sof - dev_sof) & 255;
        if (delta_sof >= 10) {
            p_stream->clock.sof_offset = delta_sof;
        } else {
            p_stream->clock.sof_offset = 0;
        }
    }

    dev_sof = (dev_sof + p_stream->clock.sof_offset) & 2047;
#if USB_OS_EN
    ret = usb_mutex_lock(p_stream->clock.p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return;
    }
#endif
    p_sample = &p_stream->clock.p_samples[p_stream->clock.head];
    p_sample->dev_stc  = usb_get_unaligned_le32(&p_data[header_size - 6]);
    p_sample->dev_sof  = dev_sof;
    p_sample->host_sof = host_sof;
    p_sample->host_ts  = ts;

    /* 更新滑动窗口的头和计数*/
    p_stream->clock.head = (p_stream->clock.head + 1) % p_stream->clock.size;

    if (p_stream->clock.count < p_stream->clock.size) {
        p_stream->clock.count++;
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_stream->clock.p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
}

/**
 * \brief USB 视频类设备视频时钟复位
 */
static void __vc_video_clock_reset(struct usbh_vc_stream *p_stream){
    struct uvc_clock *p_clock = &p_stream->clock;

    p_clock->head       = 0;
    p_clock->count      = 0;
    p_clock->last_sof   = -1;
    p_clock->sof_offset = -1;
}

/**
 * \brief 清除 USB 视频类设备视频时钟清除
 */
static void __vc_video_clock_cleanup(struct usbh_vc_stream *p_stream){
    /* 释放内存*/
    if(p_stream->clock.p_samples){
        usb_lib_mfree(&__g_uvc_lib.lib, p_stream->clock.p_samples);
    }
    p_stream->clock.p_samples = NULL;
}

/**
 * \brief USB 视频类设备视频时钟初始化
 */
static int __vc_video_clock_init(struct usbh_vc_stream *p_stream){
    struct uvc_clock *p_clock = &p_stream->clock;

    p_clock->size = 32;

    p_clock->p_samples = usb_lib_malloc(&__g_uvc_lib.lib, p_clock->size * sizeof(*p_clock->p_samples));
    if (p_clock->p_samples == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_clock->p_samples , 0, p_clock->size * sizeof(*p_clock->p_samples));
    /* 复位视频时钟*/
    __vc_video_clock_reset(p_stream);

    return USB_OK;
}

/**
 * \brief USB 视频类更新视频状态
 */
static void __vc_video_stats_update(struct usbh_vc_stream *p_stream){
    struct uvc_frame_stats *p_frame = &p_stream->stats.frame_stats;

//    __UVC_TRACE_DEBUG(("frame %u stats: %u/%u/%u packets, "
//          "%u/%u/%u pts (%searly %sinitial), %u/%u scr, "
//          "last pts/stc/sof %u/%u/%u\r\n",
//          stream->sequence, frame->first_data,
//          frame->nb_packets - frame->nb_empty, frame->nb_packets,
//          frame->nb_pts_diffs, frame->last_pts_diff, frame->nb_pts,
//          frame->has_early_pts ? "" : "!",
//          frame->has_initial_pts ? "" : "!",
//          frame->nb_scr_diffs, frame->nb_scr,
//          frame->pts, frame->scr_stc, frame->scr_sof));

    p_stream->stats.stream_stats.nb_frames++;
    p_stream->stats.stream_stats.nb_packets += p_stream->stats.frame_stats.nb_packets;
    p_stream->stats.stream_stats.nb_empty   += p_stream->stats.frame_stats.nb_empty;
    p_stream->stats.stream_stats.nb_errors  += p_stream->stats.frame_stats.nb_errors;
    p_stream->stats.stream_stats.nb_invalid += p_stream->stats.frame_stats.nb_invalid;

    if (p_frame->has_early_pts) {
        p_stream->stats.stream_stats.nb_pts_early++;
    }
    if (p_frame->has_initial_pts) {
        p_stream->stats.stream_stats.nb_pts_initial++;
    }
    if (p_frame->last_pts_diff <= p_frame->first_data) {
        p_stream->stats.stream_stats.nb_pts_constant++;
    }
    if (p_frame->nb_scr >= p_frame->nb_packets - p_frame->nb_empty) {
        p_stream->stats.stream_stats.nb_scr_count_ok++;
    }
    if (p_frame->nb_scr_diffs + 1 == p_frame->nb_scr) {
        p_stream->stats.stream_stats.nb_scr_diffs_ok++;
    }

    memset(&p_stream->stats.frame_stats, 0, sizeof(p_stream->stats.frame_stats));
}

/**
 * \brief USB 视频类解码视频状态
 */
static void __vc_video_stats_decode(struct usbh_vc_stream *p_stream,
                                    uint8_t               *p_data,
                                    int                    len){
    int         header_size;
    usb_bool_t  has_pts = USB_FALSE;
    usb_bool_t  has_scr = USB_FALSE;
    uint16_t    scr_sof;
    uint32_t    scr_stc;
    uint32_t    pts;

    /* 获取起始时间戳*/
    if ((p_stream->stats.stream_stats.nb_frames == 0) &&
            p_stream->stats.frame_stats.nb_packets == 0){
        usb_timespec_get(&p_stream->stats.stream_stats.start_ts);
    }

    switch (p_data[1] & (UVC_STREAM_PTS | UVC_STREAM_SCR)) {
        case UVC_STREAM_PTS | UVC_STREAM_SCR:
            header_size = 12;
            has_pts = USB_TRUE;
            has_scr = USB_TRUE;
            break;
        case UVC_STREAM_PTS:
            header_size = 6;
            has_pts = USB_TRUE;
            break;
        case UVC_STREAM_SCR:
            header_size = 8;
            has_scr = USB_TRUE;
            break;
        default:
            header_size = 2;
            break;
    }

    /* 检查无效的头*/
    if ((len < header_size) || (p_data[0] < header_size)) {
        p_stream->stats.frame_stats.nb_invalid++;
        return;
    }

    /* 提取时间戳*/
    if (has_pts) {
        pts = usb_get_unaligned_le32(&p_data[2]);
    }
    if (has_scr) {
        scr_stc = usb_get_unaligned_le32(&p_data[header_size - 6]);
        scr_sof = usb_get_unaligned_le16(&p_data[header_size - 2]);
    }

    /* 显示时间戳在整个框架内是恒定的吗？*/
    if (has_pts && p_stream->stats.frame_stats.nb_pts) {
        if (p_stream->stats.frame_stats.pts != pts) {
        	p_stream->stats.frame_stats.nb_pts_diffs++;
        	p_stream->stats.frame_stats.last_pts_diff = p_stream->stats.frame_stats.nb_packets;
        }
    }

    if (has_pts) {
    	p_stream->stats.frame_stats.nb_pts++;
    	p_stream->stats.frame_stats.pts = pts;
    }

    /* 所有帧的都一个非空数据包中或第一个空数据包之前是否都有一个显示时间戳*/
    if (p_stream->stats.frame_stats.size == 0) {
        if (len > header_size) {
        	p_stream->stats.frame_stats.has_initial_pts = has_pts;
        }
        if (len == header_size && has_pts) {
        	p_stream->stats.frame_stats.has_early_pts = USB_TRUE;
        }
    }

    /* SCR.STC 和 SCR.SOF 字段是否在整个框架中变化*/
    if (has_scr && p_stream->stats.frame_stats.nb_scr) {
        if (p_stream->stats.frame_stats.scr_stc != scr_stc) {
        	p_stream->stats.frame_stats.nb_scr_diffs++;
        }
    }

    if (has_scr) {
        /* 将 SOF 计数器扩展到 32 位并存储其值*/
        if (p_stream->stats.stream_stats.nb_frames > 0 || p_stream->stats.frame_stats.nb_scr > 0) {
            p_stream->stats.stream_stats.scr_sof_count +=
                    (scr_sof - p_stream->stats.stream_stats.scr_sof) % 2048;
        }
        p_stream->stats.stream_stats.scr_sof = scr_sof;

        p_stream->stats.frame_stats.nb_scr++;
        p_stream->stats.frame_stats.scr_stc = scr_stc;
        p_stream->stats.frame_stats.scr_sof = scr_sof;

        if (scr_sof < p_stream->stats.stream_stats.min_sof) {
            p_stream->stats.stream_stats.min_sof = scr_sof;
        }
        if (scr_sof > p_stream->stats.stream_stats.max_sof) {
            p_stream->stats.stream_stats.max_sof = scr_sof;
        }
    }

    /* 记录第一个非空包的编号*/
    if (p_stream->stats.frame_stats.size == 0 && len > header_size) {
        p_stream->stats.frame_stats.first_data = p_stream->stats.frame_stats.nb_packets;
    }
    /* 更新帧大小*/
    p_stream->stats.frame_stats.size += len - header_size;

    /* 更新包计数器*/
    p_stream->stats.frame_stats.nb_packets++;
    if (len > header_size) {
        p_stream->stats.frame_stats.nb_empty++;
    }
    if (p_data[1] & UVC_STREAM_ERR) {
        p_stream->stats.frame_stats.nb_errors++;
    }
}

/**
 * \brief 启动记录视频状态
 */
static void __vc_video_stats_start(struct usbh_vc_stream *p_stream){
    memset(&p_stream->stats, 0, sizeof(p_stream->stats));
    p_stream->stats.stream_stats.min_sof = 2048;
}

/**
 * \brief 停止记录视频状态
 */
static void __vc_video_stats_stop(struct usbh_vc_stream *p_stream){
    usb_timespec_get(&p_stream->stats.stream_stats.stop_ts);
}

/**
 * \brief USB 视频类视频负载解码 是由 __vc_video_decode_start()， __vc_video_decode_data()
 *        和 __vc_video_decode_end()处理。
 *        在批量或等时有效载荷开始时用USB请求块数据调用。它处理头数据，如果成功则返回头
 *        大小(字节)。如果一个错误发生，则返回一个负的错误代码。以下的错误代码有特殊的
 *        意义。
 *
 *        __vc_video_decode_data 是为每一个有 USB 请求块数据的 USB 请求块被调用。它复制数据到
 *        视频缓存。
 *
 *        在一个批量或等时有效负载的末尾用头数据调用 __vc_video_decode_end。它执行任何额外
 *        的头数据处理，返回 0 或者返回负的错误代码如果发送错误。由于 __vc_video_decode_start
 *        已经处理了头数据，因此不需要这个函数再次执行合法性检查。
 *
 *        对于负载总是在单个USB请求块中传输的等时传输，将连续调用这三个函数。
 *
 *        要让解码器处理头数据并更新其内部状态，即使没有可用的视频缓冲区，也必须准备好使用
 *        空缓存参数调用 __vc_video_decode_start。永远不会用空缓存调用 __vc_video_decode_data
 *        和 __vc_video_decode_end。
 */
static int __vc_video_decode_start(struct usbh_vc_stream *p_stream,
                                   struct usbh_vc_buffer *p_buf,
                                   uint8_t               *p_data,
                                   int                    len){
    uint8_t fid;

    /* 合法性检查：
     * - 包必须至少两个字节长度
     * - bHeaderLength 值至少两个字节(见上面)
     * - bHeaderLength 值不能比包大小还大*/
    if ((len < 2) || (p_data[0] < 2) || (p_data[0] > len)) {
        p_stream->stats.frame_stats.nb_invalid++;
        return -USB_EILLEGAL;
    }
    /* 获取帧 ID */
    fid = p_data[1] & UVC_STREAM_FID;

    /* 不管缓冲区状态如何，都要增加序列号，这样不连续的序列号总是表示丢失的帧。*/
    if (p_stream->last_fid != fid) {
        p_stream->sequence++;
        if (p_stream->sequence) {
            /* 更新视频状态*/
            __vc_video_stats_update(p_stream);
        }
    }

    __vc_video_clock_decode(p_stream, p_buf, p_data, len);
    __vc_video_stats_decode(p_stream, p_data, len);

    /* 存储有效负载的 FID 位和当缓存是空的则马上返回*/
    if (p_buf == NULL) {
        p_stream->last_fid = fid;
        return -USB_EDATA;
    }

    /* 如果错误位被设置了，标记为坏的缓存*/
    if (p_data[1] & UVC_STREAM_ERR) {
        //__USB_ERR_INFO("marking buffer as bad(error bit set)\r\n");
        p_buf->error_no = 1;
    }

    /* 当缓冲区的状态不是UVC_BUF_STATE_ACTIVE时，通过等待FID位被切换来同步到输入流。stream->last_fid
     * 被初始化为-1，所有第一个等时帧总是同步的。
     *
     * 如果设备不切换FID位，当EOF为被设置为强制下一个数据包同步时，反转stream->last_fid。*/
    if (p_buf->state != UVC_BUF_STATE_ACTIVE) {
        struct usb_timespec ts;

        if (fid == p_stream->last_fid) {
            if ((p_stream->p_vc->quirks & UVC_QUIRK_STREAM_NO_FID) &&
                (p_data[1] & UVC_STREAM_EOF))
                p_stream->last_fid ^= UVC_STREAM_FID;
            return -USB_EDATA;
        }

        usb_timespec_get(&ts);

        p_buf->timeval.ts_sec  = ts.ts_sec;
        p_buf->timeval.ts_usec = ts.ts_nsec / 1000L;

        /* TODO: 处理PTS和SCR*/
        p_buf->state = UVC_BUF_STATE_ACTIVE;
    }
    /* 如果我们在下一个新帧的开始处，则将缓冲区标记为已完成。通过检查 EOF 位(与 EOF 位相比，FID 位切换延迟一帧)
     * 可以更好地实现帧尾检测，但有些设备不会在帧尾设置位(最后一个有效负载可能会丢失)。
     *
     * stream->last_fid 被初始化为 -1，所以第一个等时帧不会触发帧尾检测。
     *
     * 空缓冲区(bytes_used == 0)不会触发帧尾检测，因为返回空缓冲区没有意义。如果先前的有效负载设置了 EOF 位
     * ，这也避免了在 FID 切换时检测帧结束条件。*/
    if ((fid != p_stream->last_fid) && (p_buf->bytes_used != 0)) {
        //__UVC_TRACE_DEBUG("Frame complete (FID bit toggled).\r\n");
        p_buf->state = UVC_BUF_STATE_READY;
        return -USB_EAGAIN;
    }

    p_stream->last_fid = fid;

    return p_data[0];
}

/**
 * \brief USB 视频类视频数据解码
 */
static void __vc_video_data_decode(struct usbh_vc_stream *p_stream,
                                   struct usbh_vc_buffer *p_buf,
                                   uint8_t               *p_data,
                                   int                    len){
    unsigned int maxlen, nbytes;
    void        *p_mem;

    if (len <= 0) {
        return;
    }

    /* 复制视频数据到缓存里*/
    maxlen = p_buf->length - p_buf->bytes_used;

    p_mem = p_buf->p_mem + p_buf->bytes_used;

    nbytes = min((unsigned int)len, maxlen);

    memcpy(p_mem, p_data, nbytes);

    p_buf->bytes_used += nbytes;

    /* 如果超过缓存大，完成当前帧小*/
    if (len > (int)maxlen) {
        //__UVC_TRACE_DEBUG("Frame complete (overflow).\r\n");
        p_buf->state = UVC_BUF_STATE_READY;
    }
}

/**
 * \brief USB 视频类视频解码结束
 */
static void __vc_video_decode_end(struct usbh_vc_stream *p_stream,
                                  struct usbh_vc_buffer *p_buf,
                                  uint8_t               *p_data,
                                  int                    len){
    /* 如果 EOF 标记被设置了，标记缓存为已完成*/
    if ((p_data[1] & UVC_STREAM_EOF) && (p_buf->bytes_used != 0)) {
        //__UVC_TRACE_DEBUG("Frame complete (EOF found).\r\n");
        if (p_data[0] == len) {
            //__UVC_TRACE_DEBUG("EOF in empty payload.\r\n");
        }
        p_buf->state = UVC_BUF_STATE_READY;

        if (p_stream->p_vc->quirks & UVC_QUIRK_STREAM_NO_FID) {
            p_stream->last_fid ^= UVC_STREAM_FID;
        }
    }
}

/**
 * \brief USB 视频类解码等时传输的视频数据
 */
static void __vc_video_isoc_decode(struct usbh_trp       *p_trp,
                                   struct usbh_vc_stream *p_stream,
                                   struct usbh_vc_buffer *p_buf){
    uint8_t              *p_mem   = NULL;
    int                   ret, i;
    struct usbh_vc_queue *p_queue = NULL;
    /* 获取队列*/
    p_queue = (struct usbh_vc_queue *)(p_stream->p_queue);
    if (p_queue == NULL) {
        return;
    }

    /* 检查USB请求块每个同步包的状态*/
    for (i = 0; i < p_trp->n_iso_packets; ++i) {
        if (p_trp->p_iso_frame_desc[i].status < 0) {
            //__USB_ERR_INFO("usb isochronous frame lost(%d)\r\n", p_trp->p_iso_frame_desc[i].status);
            /* 将缓冲区标记为故障*/
            if (p_buf != NULL) {
                p_buf->error_no = 1;
            }
            continue;
        }

        /* 解码有效负载头*/
        p_mem = p_trp->p_data + p_trp->p_iso_frame_desc[i].offset;
        do {
            /* 开始视频编码*/
            ret = __vc_video_decode_start(p_stream,
                                          p_buf,
                                          p_mem,
                                          p_trp->p_iso_frame_desc[i].act_len);
            if (ret == -USB_EAGAIN) {
                __vc_video_buffer_validate(p_stream, p_buf);
                p_buf = __vc_queue_next_buffer(p_queue, p_buf);
            }
        } while (ret == -USB_EAGAIN);

        if (ret < 0) {
            continue;
        }
        /* 解码负载数据*/
        __vc_video_data_decode(p_stream,
                               p_buf,
                               p_mem + ret,
                               p_trp->p_iso_frame_desc[i].act_len - ret);
        /* 再次处理头*/
        __vc_video_decode_end(p_stream,
                              p_buf,
                              p_mem,
                              p_trp->p_iso_frame_desc[i].act_len);

        if (p_buf->state == UVC_BUF_STATE_READY) {
            __vc_video_buffer_validate(p_stream, p_buf);
            p_buf = __vc_queue_next_buffer(p_queue, p_buf);
        }
    }
}

#if 0
/**
 * \brief USB 视频类视频批量编码
 */
static void __vc_video_bulk_encode(struct usbh_trp       *p_trp,
                                   struct usbh_vc_stream *p_stream,
                                   struct usbh_vc_buffer *p_buf){
//todo
}
#endif

/**
 * \brief USB 视频类视频批量解码
 */
static void __vc_video_bulk_decode(struct usbh_trp       *p_trp,
                                   struct usbh_vc_stream *p_stream,
                                   struct usbh_vc_buffer *p_buf){
//todo
}

/**
 * \brief 通过切换到备用设置 0 来初始化 USB 视频类视频设备并检索默认格式
 *        一些摄像头(即 Fuji Finepix)将格式和帧索引设置为0。 USB 视频类标准并没有
 *        明确规定这是违反规定的，所有如果可能的话静静的修改这些值。
 *        这个函数要在注册 V4L 设备之前调用
 *
 * \param[in] p_stream 视频流结构体
 *
 * \reval 成功返回 USB_OK
 */
int usbh_vc_video_init(struct usbh_vc_stream *p_stream){
    int                     ret;
    struct uvc_format      *p_format = NULL;
    struct uvc_frame       *p_frame  = NULL;
    uint32_t                i;
    struct uvc_stream_ctrl *p_probe  = &p_stream->ctrl;
    struct usbh_function   *p_ufun   = p_stream->p_vc->p_ufun;

    if (p_stream == NULL) {
        return -USB_EINVAL;
    }
    /* 检查视频流格式数量*/
    if (p_stream->n_formats == 0) {
        __USB_ERR_INFO("no supported video formats found\r\n");
        return -USB_EILLEGAL;
    }
    /* 备用设置 0 应该是默认的，但是如果 XBox Live Vision Cam(可能还有其他设备)
     * 在任何其他视频控制请求之前没有接受 SET_INTERFACE 请求，则会崩溃或者出
     * 先其他错误行为。*/
    for (i = 0; i < 3; i++) {
        ret = usbh_func_intf_set(p_ufun, p_stream->intf_num, 0);
        if (ret == USB_OK) {
            break;
        }
        usb_mdelay(100);
    }
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB video device interface %d alt 0 set failed(%d)\r\n",
                p_stream->intf_num, ret);
        return ret;
    }
    /* 使用从设备检索的默认流参数设置流探测控件。不支持探测控件的 GET_DEF 请求
     * 的网络摄像头将只保留当前流参数。
     */
    if (usbh_vc_video_ctrl_get(p_stream, p_probe, 1, UVC_GET_DEF) == 0) {
        usbh_vc_video_ctrl_set(p_stream, p_probe, 1);
    }
    /* 使用探测控制电流值初始化流参数。这确保流提交控件上的 SET_CUR 请求将
     * 始终使用根据UVC规范的要求从探测控件上成功的 GET_CUR 请求检索的值。
     */
    ret = usbh_vc_video_ctrl_get(p_stream, p_probe, 1, UVC_GET_CUR);
    if (ret != USB_OK) {
        return ret;
    }

    /* 检查是否存在默认格式描述符。否则使用第一个格式。*/
    for (i = p_stream->n_formats; i > 0; --i) {
        p_format = &p_stream->p_format[i - 1];
        if (p_format->index == p_probe->format_index) {
            break;
        }
    }
    if (p_format->n_frames == 0) {
    	__USB_ERR_INFO("no frame descriptor found for the default format\r\n");
        return -USB_EILLEGAL;
    }

    /* frame_index 为 0 可能是正确。基于流的格式(包括MPEG-2 TS 和 DV)不支持帧，
     * 但具有 frame_index 设置为 0 的虚拟帧描述符。如果找不到默认的帧描述符，
     * 请使用第一个可用的帧。*/
    for (i = p_format->n_frames; i > 0; --i) {
        p_frame = &p_format->p_frame[i - 1];
        if (p_frame->frame_index == p_probe->frame_index) {
            break;
        }
    }
    p_probe->format_index  = p_format->index;
    p_probe->frame_index   = p_frame->frame_index;

    p_stream->p_format_def = p_format;
    p_stream->p_format_cur = p_format;
    p_stream->p_frame_cur  = p_frame;

    /* 选择视频解码功能*/
    if (p_stream->type == UVC_BUF_TYPE_VIDEO_CAPTURE) {
        if (p_stream->p_vc->n_stream_intf_altset > 1) {
        	p_stream->p_fn_decode = __vc_video_isoc_decode;
        } else {
        	p_stream->p_fn_decode = __vc_video_bulk_decode;
        }
    } else {
        if (p_stream->p_vc->n_stream_intf_altset == 1) {
        	p_stream->p_fn_decode = __vc_video_bulk_decode;
        } else {
        	__USB_ERR_INFO("isochronous endpoints are not "
                              "supported for video output devices\r\n");
            return -USB_ENOTSUP;
        }
    }

    return USB_OK;
}

/**
 * \brief 计算端点每个时间间隔的最大字节数
 */
static uint32_t __vc_endpoint_max_bpi(struct usbh_device   *p_usb_dev,
                                      struct usbh_endpoint *p_ep){
    uint16_t psize;

    switch (p_usb_dev->speed) {
        case USB_SPEED_SUPER:
            //todo
            return -USB_ENOTSUP;
        case USB_SPEED_HIGH:
            /* 获取端点最大包大小*/
            psize = USBH_EP_MPS_GET(p_ep);
            /* 第12，11位表明了每微帧的事务数量，00(每微帧1个事务)
               01(每微帧2个事务)，10(每微帧3个事务)，11(保留)，低11位指明了每个单独事务的有效数据字节数*/
            return (psize & 0x07ff) * (1 + ((psize >> 11) & 3));
        case USB_SPEED_WIRELESS:
            psize = USBH_EP_MPS_GET(p_ep);
            return psize;
        default:
            psize = USBH_EP_MPS_GET(p_ep);
            return psize & 0x07ff;
    }
}

/**
 * \brief 申请同步传输请求包
 */
static struct usbh_trp *__vc_trp_alloc(int iso_packets){
    struct usbh_trp *p_trp = NULL;

    p_trp = usb_lib_malloc(&__g_uvc_lib.lib, sizeof(struct usbh_trp));
    if (p_trp == NULL) {
        return NULL;
    }
    memset(p_trp, 0, sizeof(struct usbh_trp));

    if (iso_packets > 0) {
        p_trp->p_iso_frame_desc = usb_lib_malloc(&__g_uvc_lib.lib, iso_packets * sizeof(struct usb_iso_pkt_desc));
        if (p_trp->p_iso_frame_desc == NULL) {
            usb_lib_mfree(&__g_uvc_lib.lib, p_trp);
            return NULL;
        }

        memset(p_trp->p_iso_frame_desc, 0, iso_packets * sizeof(struct usb_iso_pkt_desc));
    }
    return p_trp;
}

/**
 * \brief 释放同步传输请求包
 */
static void __vc_trp_free(struct usbh_trp *p_trp){
    if (p_trp != NULL) {
        if (p_trp->p_iso_frame_desc) {
            usb_lib_mfree(&__g_uvc_lib.lib, p_trp->p_iso_frame_desc);
        }
        usb_lib_mfree(&__g_uvc_lib.lib, p_trp);
    }
}

static void __vc_trp_buffers_free(struct usbh_vc_stream *p_stream);
/**
 * 分配传输缓存。从挂起恢复时，可以使用已分配的缓存调用这个函数，在这种情况
 * 下，它将在不接触缓存的情况下返回。
 *
 * 将缓存大小限制为 UVC_MAX_PACKETS 批量/等时数据包。如果系统内存太低，请尝试
 * 连续减少数据包的数量，直到分配成功。
 */
static int __vc_trp_buffers_alloc(struct usbh_vc_stream *p_stream,
                                  uint32_t               size,
                                  uint32_t               psize,
                                  uint8_t                n_trp){
    uint32_t n_packets;
    uint32_t i;

    /* 缓存已经被分配，返回*/
    if (p_stream->trp_size) {
        return p_stream->trp_size / psize;
    }
    /* 计算数据包的数量。批量端点可能会跨多个多个USB请求块传输UVC负载*/
    /* 以端点最大包大小为倍数，得出需要多少个包*/
    n_packets = USB_DIV_ROUND_UP(size, psize);
    if (n_packets > UVC_MAX_PACKETS) {
        n_packets = UVC_MAX_PACKETS;
    }

    /* 重试分配直到一次成功*/
    for (; n_packets > 1; n_packets /= 2) {
        for (i = 0; i < n_trp; ++i) {
            p_stream->trp_size = psize * n_packets;
            p_stream->p_trp_buffer[i] = usb_lib_malloc(&__g_uvc_lib.lib, p_stream->trp_size);
            if (p_stream->p_trp_buffer[i] == NULL) {
                __vc_trp_buffers_free(p_stream);
                break;
            }
            memset(p_stream->p_trp_buffer[i], 0, p_stream->trp_size);
        }

        if (i == n_trp) {
            __USB_INFO("allocated %u usb buffers of %ux%u bytes each\r\n", n_trp, n_packets, psize);
            return n_packets;
        }
    }
    __USB_ERR_INFO("allocate usb buffers (%u bytes per packet) failed\r\n", psize);
    return 0;
}

/**
 * \brief 释放传输缓存
 */
static void __vc_trp_buffers_free(struct usbh_vc_stream *p_stream){
    /* 释放传输请求包*/
    for (uint8_t i = 0; i < p_stream->n_trp; i++) {
        /* 释放传输请求包数据缓存*/
        if (p_stream->p_trp_buffer[i]) {
            usb_lib_mfree(&__g_uvc_lib.lib, p_stream->p_trp_buffer[i]);
            p_stream->p_trp_buffer[i] = NULL;
        }
    }
    p_stream->trp_size = 0;
}

/**
 * \brief 提交视频参数
 */
static int __vc_video_commit(struct usbh_vc_stream  *p_stream,
                             struct uvc_stream_ctrl *p_probe){
    return usbh_vc_video_ctrl_set(p_stream, p_probe, 0);
}

/**
 * \brief USB 视频类传输完成回调函数
 */
static void __vc_video_complete(void *p_arg){
    struct usbh_trp       *p_trp    = (struct usbh_trp *)p_arg;
    struct usbh_vc_stream *p_stream = p_trp->p_usr_priv;
    struct usbh_vc_queue  *p_queue  = NULL;
    struct usbh_vc_buffer *p_buf    = NULL;
    uint32_t               i;
    int                    ret;

    /* 设备已经被移除*/
    if ((p_stream->p_vc->is_removed == USB_TRUE) ||
            (p_trp->status == -USB_ECANCEL)) {
        p_stream->n_trp_act--;
        return;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_stream->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return;
    }
#endif
    /* 设备挂起*/
    if (p_stream->is_frozen == 1) {
        goto __exit;
    }

    switch (p_trp->status) {
        case 0:
            break;
        default:
            __USB_ERR_INFO("non-zero status (%d) in video completion handler\r\n", p_trp->status);
            goto __exit;
#if 0
        case -USB_ENOENT:
            if (p_stream->is_frozen) {
                return;
            } else {
                break;
            }
        case -USB_ECONNRESET:
        case -USB_ESHUTDOWN:
            /* UVC取消队列*/
            //todo
            //uvc_queue_cancel(queue, urb->status == -ESHUTDOWN);
           return;
#endif
    }

    /* 获取队列*/
    p_queue = (struct usbh_vc_queue *)p_stream->p_queue;
    if (p_queue == NULL) {
        goto __exit;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_queue->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        goto __exit;
    }
#endif
    for (i = 0; i < p_queue->n_bufs_total; i++) {
        /* 检查队列的中断队列链表是否为空*/
        if (!usb_list_head_is_empty(&p_queue->irqqueue)) {
            /* 获取中断队列链表第一个成员*/
            p_buf = usb_container_of(p_queue->irqqueue.p_next, struct usbh_vc_buffer, node);
            /* 如果拿到的缓存被使用中，把它移到队尾*/
            if (p_buf->is_used == 0) {
                break;
            }
            usb_list_node_move_tail(&p_buf->node, &p_queue->irqqueue);
        }
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_queue->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        goto __exit;
    }
#endif
    if (i == 3) {
        goto __resubmit;
    }

    /* 如果拿到的缓存里有完整的图像数据，重置缓存状态*/
    if (p_buf->state == UVC_BUF_STATE_DONE) {
        p_buf->bytes_used = 0;
        p_buf->error_no   = 0;
        p_buf->state      = UVC_BUF_STATE_IDLE;
    }

    /* 进行数据解码*/
    p_stream->p_fn_decode(p_trp, p_stream, p_buf);
__resubmit:
    /* 再次提交 USB 请求包*/
    for (i = 0; i < 5; i++) {
        ret = usbh_trp_submit(p_trp);
        if (ret == USB_OK) {
            break;
        }
    }
    if (i == 5) {
        p_stream->n_trp_act--;
    }
__exit:
#if USB_OS_EN
    ret = usb_mutex_unlock(p_stream->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return;
}

/**
 * \brief 初始化等时 USB 请求包并且分配传输缓存。最大包大小由端点提供
 */
static int __vc_video_isoc_init(struct usbh_vc_stream *p_stream,
                                struct usbh_endpoint  *p_ep,
                                uint8_t                n_trp){
    unsigned int          ep_num;
    struct usbh_endpoint *p_ep_tmp = NULL;
    struct usbh_trp      *p_trp    = NULL;
    uint32_t              n_packets, i, j;
    uint16_t              psize;
    uint32_t              size;
    struct usbh_vc       *p_vc     = p_stream->p_vc;

    /* 获取端点地址*/
    ep_num = USBH_EP_ADDR_GET(p_ep);
    /* 判断端点*/
    if (USBH_EP_DIR_GET(p_ep)) {
    	p_ep_tmp = p_ep->p_usb_dev->p_ep_in[ep_num];
    } else {
    	p_ep_tmp = p_ep->p_usb_dev->p_ep_out[ep_num];
    }
    if (p_stream->p_trp == NULL) {
    	p_stream->p_trp = usb_lib_malloc(&__g_uvc_lib.lib, sizeof(void *) * n_trp);
        if (p_stream->p_trp == NULL) {
            return -USB_ENOMEM;
        }
        memset(p_stream->p_trp, 0, sizeof(void *) * n_trp);
    }

    if (p_stream->p_trp_buffer == NULL) {
    	p_stream->p_trp_buffer = usb_lib_malloc(&__g_uvc_lib.lib, sizeof(void *) * n_trp);
        if (p_stream->p_trp_buffer  == NULL) {
            return -USB_ENOMEM;
        }
        memset(p_stream->p_trp_buffer, 0, sizeof(void *) * n_trp);
    }

    /* 获取端点最大包大小*/
    psize = __vc_endpoint_max_bpi(p_vc->p_ufun->p_usb_dev, p_ep);
    size = p_stream->ctrl.max_video_frame_size;

    n_packets = __vc_trp_buffers_alloc(p_stream, size, psize, n_trp);
    if (n_packets == 0) {
        return -USB_ENOMEM;
    }
    /* 计算总大小*/
    size = n_packets * psize;

    for (i = 0; i < n_trp; ++i) {
        p_trp = __vc_trp_alloc(n_packets);
        if (p_trp == NULL) {
            return -USB_ENOMEM;
        }
        p_trp->p_usr_priv    = p_stream;
        p_trp->p_ep          = p_ep_tmp;
        p_trp->flag          = USBH_TRP_ISO_ASAP;
        p_trp->interval      = p_ep->p_ep_desc->interval;
        /* 传输完成回调函数*/
        p_trp->p_fn_done     = __vc_video_complete;
        p_trp->p_arg         = p_trp;
        p_trp->p_data        = p_stream->p_trp_buffer[i];
        p_trp->len           = size;
        p_trp->n_iso_packets = n_packets;

        for (j = 0; j < n_packets; ++j) {
        	p_trp->p_iso_frame_desc[j].offset = j * psize;
        	p_trp->p_iso_frame_desc[j].len    = psize;
        }

        p_stream->p_trp[i] = p_trp;
    }

    p_stream->n_trp = n_trp;

    return USB_OK;
}

/**
 * \brief 初始化批量 USB 请求包
 */
static int __vc_video_bulk_init(struct usbh_vc_stream *p_stream,
                                struct usbh_endpoint  *p_ep){
    return 0;
}

/**
 * \brief 释放视频缓存
 */
void usbh_vc_video_free(struct usbh_vc_stream *p_stream, int is_buf_free){
    if (p_stream->p_trp) {
        for (uint8_t i = 0; i < p_stream->n_trp; i++) {
            __vc_trp_free(p_stream->p_trp[i]);
        }
        usb_lib_mfree(&__g_uvc_lib.lib, p_stream->p_trp);
        p_stream->p_trp = NULL;
    }

    if (is_buf_free == USB_TRUE) {
        if (p_stream->p_trp_buffer == NULL) {
            return;
        }
        __vc_trp_buffers_free(p_stream);
        usb_lib_mfree(&__g_uvc_lib.lib, p_stream->p_trp_buffer);
        p_stream->p_trp_buffer = NULL;
    }
}

/**
 * \brief 反初始化等时/批量 USB 请求包并且释放传输缓存
 */
static int __vc_video_deinit(struct usbh_vc_stream *p_stream, int is_buf_free){
    int ret = usbh_vc_stream_xfer_cancel(p_stream);
    if (ret != USB_OK) {
        return ret;
    }

#if USB_OS_EN
    ret= usb_mutex_unlock(p_stream->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 等待传输停止*/
    while (p_stream->n_trp_act != 0) {
        usb_mdelay(10);
    }
#if USB_OS_EN
    ret= usb_mutex_lock(p_stream->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    __vc_video_stats_stop(p_stream);

    usbh_vc_video_free(p_stream, is_buf_free);

    return USB_OK;
}

/**
 * \brief 初始化等时/批量 USB 请求包并且分配传输缓存
 */
static int __vc_video_start(struct usbh_vc_stream *p_stream, uint8_t n_trp){
    struct usbh_endpoint  *p_ep       = NULL;
    struct usbh_vc        *p_vc       = p_stream->p_vc;
    int                    ret;
    uint32_t               i;
    struct usb_list_node  *p_node     = NULL;
    struct usb_list_node  *p_node_tmp = NULL;
    struct usbh_interface *p_alts     = NULL;
    uint32_t               psize;
    struct usbh_vc_queue  *p_queue    = NULL;

    p_stream->sequence          = -1;
    p_stream->last_fid          = -1;
    p_stream->bulk.header_size  = 0;
    p_stream->bulk.skip_payload = 0;
    p_stream->bulk.payload_size = 0;

    __vc_video_stats_start(p_stream);

    if (p_vc->n_stream_intf_altset > 1) {
        struct usbh_endpoint *p_ep_best  = NULL;
        uint32_t              psize_best = (~0U);
        uint32_t              band_width;
        uint32_t              altsetting = 0;
        int                   intf_num   = p_stream->intf_num;

        /* 等时端点，选择备用设置*/
        band_width = p_stream->ctrl.max_payload_transfer_size;
        if (band_width == 0) {
            __USB_INFO("device requested null bandwidth, defaulting to lowest\r\n");
            band_width = 1;
        } else {
            __USB_INFO("device requested %u B/frame bandwidth\r\n", band_width);
        }

        /* 遍历设备结构体视频流链表*/
        usb_list_for_each_node_safe(p_node,
                                    p_node_tmp,
                                   &p_vc->p_ufun->intf_list) {
            p_alts = usb_container_of(p_node, struct usbh_interface, node);

            if (p_alts->p_desc->interface_sub_class == UVC_VSC_VIDEOSTREAMING) {
                /* 寻找端点*/
                ret = usbh_intf_ep_get(p_alts, p_stream->header.endpoint_address, &p_ep);
                if (ret != USB_OK) {
                    continue;
                }
                /* 检查带宽是否足够高*/
                psize = __vc_endpoint_max_bpi(p_vc->p_ufun->p_usb_dev, p_ep);
                if ((psize >= band_width) && (psize <= psize_best)) {
                    altsetting = p_alts->p_desc->alternate_setting;
                    psize_best = psize;
                    p_ep_best  = p_ep;
                }
            }
        }
        if (p_ep_best == NULL) {
            __USB_ERR_INFO("no fast enough alt setting for requested bandwidth\r\n");
            return -USB_EAGAIN;
        }

        __USB_INFO("selecting alternate setting %u (%u B/frame bandwidth)\r\n", altsetting, psize_best);

        for (i = 0; i < 3; i++) {
            ret = usbh_func_intf_set(p_vc->p_ufun, intf_num, altsetting);
            if (ret == USB_OK) {
                break;
            }
            usb_mdelay(100);
        }
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB video device interface %d alt %d set failed(%d)\r\n",
                    intf_num, altsetting, ret);
            return ret;
        }

        /* 初始化 USB 视频流视频等时传输*/
        ret = __vc_video_isoc_init(p_stream, p_ep_best, n_trp);
        if (ret != USB_OK) {
            goto __failed;
        }
    } else {
        /* 遍历设备结构体视频流链表*/
        usb_list_for_each_node_safe(p_node,
                                    p_node_tmp,
                                   &p_vc->p_ufun->intf_list) {
            p_alts = usb_container_of(p_node, struct usbh_interface, node);

            /* 批量端点，处理传输块初始化*/
            ret = usbh_intf_ep_get(p_alts, p_stream->header.endpoint_address, &p_ep);
        }
        if (ret != USB_OK) {
            return ret;
        }
        /* 初始化 USB 视频类视频批量传输*/
        ret = __vc_video_bulk_init(p_stream, p_ep);
        if (ret != USB_OK) {
            goto __failed;
        }
    }
    /* 获取全局队列*/
    p_queue = usbn_vc_queue_get();
    if (p_queue == NULL) {
        return -USB_ENODEV;
    }
    /* 初始化缓存*/
    ret = usbh_vc_queue_buf_alloc(p_queue,
                                  p_stream->ctrl.max_video_frame_size,
                                  p_queue->n_bufs_total);
    if (ret != USB_OK) {
        goto __failed;
    }
    /* 绑定缓存队列和数据流*/
    p_stream->p_queue   = p_queue;
    p_stream->n_trp_act = 0;

    for (i = 0; i < p_stream->n_trp; ++i) {
        ret = usbh_trp_submit(p_stream->p_trp[i]);
        if (ret != USB_OK) {
            __USB_ERR_INFO("usb video class device stream trp %u submit failed(%d)\r\n", i, ret);
            goto __failed;
        }
        p_stream->n_trp_act++;
    }
    return USB_OK;

__failed:
    usbh_vc_queue_put(p_queue);
    usbh_vc_queue_buf_free(p_queue);
    __vc_video_deinit(p_stream, USB_TRUE);
    return ret;
}

/**
 * \brief 使能或禁能视频流
 */
static int __vc_video_opt(struct usbh_vc_stream *p_stream, int is_enable, uint8_t n_trp){
    int             ret;
    struct usbh_vc *p_vc = p_stream->p_vc;

    /* 关闭视频流*/
    if (!is_enable) {
        /* 注销视频流*/
        ret = __vc_video_deinit(p_stream, USB_TRUE);
        if (ret != USB_OK) {
            return ret;
        }

        /* 反初始化缓存*/
        ret = usbh_vc_queue_buf_free(p_stream->p_queue);
        if (ret != USB_OK) {
            return ret;
        }

        /* 释放缓存队列*/
        ((struct usbh_vc_queue *)p_stream->p_queue)->is_used = USB_FALSE;

        /* 清除采样时钟*/
        __vc_video_clock_cleanup(p_stream);

        if ((p_vc->n_stream_intf_altset > 1) && (p_stream->p_vc->is_removed == USB_FALSE)) {
            ret = usbh_func_intf_set(p_vc->p_ufun, p_stream->intf_num, 0);
            if ((ret == -USB_ENODEV) || (ret == USB_OK)) {
                return USB_OK;
            }
            return ret;
        } else {
            /* 获取视频流接口端点地址*/
            unsigned int ep_num = p_stream->header.endpoint_address;
            /* 清除设备停止状态*/
            if (ep_num & 0x80) {
                usbh_dev_ep_halt_clr(p_vc->p_ufun->p_usb_dev->p_ep_in[ep_num & 0x0f]);
            } else {
                usbh_dev_ep_halt_clr(p_vc->p_ufun->p_usb_dev->p_ep_out[ep_num]);
            }
        }

        return USB_OK;
    }
    /* 初始化 USB 视频类设备视频时钟*/
    ret = __vc_video_clock_init(p_stream);
    if (ret != USB_OK) {
        return ret;
    }
    /* 提交视频流参数*/
    ret = __vc_video_commit(p_stream, &p_stream->ctrl);
    if (ret != USB_OK) {
        goto __failed1;
    }
    /* 启动视频*/
    ret = __vc_video_start(p_stream, n_trp);
    if (ret != USB_OK){
        goto __failed2;
    }
    return USB_OK;
__failed2:
    /* 设置为第一个接口的备用设置*/
    usbh_func_intf_set(p_vc->p_ufun, p_stream->intf_num, 0);
__failed1:
    /* 清除采样时钟*/
    __vc_video_clock_cleanup(p_stream);
    return ret;
}

/**
 * \brief USB 视频类设备使能视频流
 *
 * \param[in] p_stream USB 视频类设备视频流结构体
 * \param[in] n_trp    要提交的传输包数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_video_enable(struct usbh_vc_stream *p_stream, uint8_t n_trp){
    if (p_stream == NULL) {
        return -USB_EINVAL;
    }

    return __vc_video_opt(p_stream, USB_TRUE, n_trp);
}

/**
 * \brief USB 视频类设备关闭视频流
 *
 * \param[in] p_stream USB 视频类设备视频流结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_video_disable(struct usbh_vc_stream *p_stream){
    if (p_stream == NULL) {
        return -USB_EINVAL;
    }

    return __vc_video_opt(p_stream, USB_FALSE, 0);
}

/**
 * \brief USB 视频类设备恢复视频流
 *
 * \param[in] p_stream USB 视频类设备视频流结构体
 * \param[in] is_reset 是否复位接口
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_video_resume(struct usbh_vc_stream *p_stream, usb_bool_t is_reset){
    int ret;
#if USB_OS_EN
    int ret_tmp;
#endif

    if (p_stream == NULL) {
        return -USB_EINVAL;
    }
    /* 没在挂起状态*/
    if (p_stream->is_frozen == 0) {
        return USB_OK;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_stream->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    /* 有一些设备如果在任何其他视频控制请求前没有收到 SET_INTERFACE 请求，会崩溃或不正常*/
    if (is_reset) {
        ret = usbh_func_intf_set(p_stream->p_vc->p_ufun, p_stream->intf_num, 0);
        if (ret != USB_OK) {
            goto __exit;
        }
    }
    p_stream->is_frozen = 0;

    __vc_video_clock_reset(p_stream);
    /* 提交摄像头参数*/
    ret = __vc_video_commit(p_stream, &p_stream->ctrl);
    if (ret != USB_OK){
    	goto __exit;
    }

    ret = __vc_video_start(p_stream, p_stream->n_trp);
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_stream->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 视频类设备挂起视频流
 *
 * \param[in] p_stream USB 视频类设备视频流结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_video_suspend(struct usbh_vc_stream *p_stream){
    int ret;
#if USB_OS_EN
    int ret_tmp;
#endif

    if (p_stream == NULL) {
        return -USB_EINVAL;
    }

    /* 没在运行状态状态*/
    if (p_stream->is_frozen == 1) {
        return USB_OK;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_stream->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 设置挂起标志*/
    p_stream->is_frozen = 1;
    /* 反初始化操作*/
    ret = __vc_video_deinit(p_stream, USB_FALSE);
    if (ret != USB_OK) {
        goto __exit;
    }

    /* 反初始化缓存*/
    ret = usbh_vc_queue_buf_free(p_stream->p_queue);
    if (ret != USB_OK) {
        goto __exit;
    }

    /* 释放缓存队列*/
    usbh_vc_queue_put(p_stream->p_queue);

    ret = usbh_func_intf_set(p_stream->p_vc->p_ufun, p_stream->intf_num, 0);
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_stream->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 视频类视频流视频探测
 *
 * \param[in] p_stream 要探测 USB 视频类视频流结构体
 * \param[in] p_probe  要探测的 USB 视频类视频流空间结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_video_probe(struct usbh_vc_stream  *p_stream,
                        struct uvc_stream_ctrl *p_probe){
    struct uvc_stream_ctrl probe_min, probe_max;
    uint16_t               band_width;
    unsigned int           i;
    int                    ret;

    /* 进行探测。设备应根据其功能调整请求的值。然而，一些设备，即第一代
     * UVC Logitech 网络摄像头，并没有正确实现视频探测控件，只是返回所需要
     * 的带宽。因此，如果所需带宽超过最大可用带宽，请尝试降低质量。*/
    ret = usbh_vc_video_ctrl_set(p_stream, p_probe, 1);
    if (ret != USB_OK) {
        goto __done;
    }
    /* 获取压缩设置的最小最大值*/
    ret = usbh_vc_video_ctrl_get(p_stream, &probe_min, 1, UVC_GET_MIN);
    if (ret != USB_OK) {
        goto __done;
    }
    ret = usbh_vc_video_ctrl_get(p_stream, &probe_max, 1, UVC_GET_MAX);
    if (ret != USB_OK) {
        goto __done;
    }

    p_probe->comp_quality = probe_max.comp_quality;

    for (i = 0; i < 2; ++i) {
        ret = usbh_vc_video_ctrl_set(p_stream, p_probe, 1);
        if (ret != USB_OK) {
            goto __done;
        }
        ret = usbh_vc_video_ctrl_get(p_stream, p_probe, 1, UVC_GET_CUR);
        if (ret != USB_OK) {
            goto __done;
        }

        if (p_stream->p_vc->n_stream_intf_altset == 1) {
            break;
        }

        band_width = p_probe->max_payload_transfer_size;
        if (band_width <= p_stream->mps) {
            break;
        }
        /* TODO: 协商压缩参数*/
        p_probe->key_frame_rate   = probe_min.key_frame_rate;
        p_probe->frame_rate       = probe_min.frame_rate;
        p_probe->comp_quality     = probe_max.comp_quality;
        p_probe->comp_window_size = probe_min.comp_window_size;
    }

__done:
    return ret;
}

/**
 * \brief 获取视频缓存
 *
 * \param[out] p_buf     返回的一帧视频缓存地址
 * \param[in]  p_stream  视频数据流
 * \param[in]  p_timeval 时间戳
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_video_buf_get(uint32_t             **p_buf,
                          struct usbh_vc_stream *p_stream,
                          struct uvc_timeval    *p_timeval){
    struct usb_list_node  *p_node     = NULL;
    struct usb_list_node  *p_node_tmp = NULL;
    struct usbh_vc_buffer *p_vc_buf   = NULL;
#if USB_OS_EN
    int                    ret;
#endif
    struct usbh_vc_queue  *p_queue    = NULL;

    if (p_stream == NULL) {
        return -USB_EINVAL;
    }
    p_queue = (struct usbh_vc_queue *)p_stream->p_queue;
    if (p_queue == NULL) {
        return -USB_EILLEGAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_queue->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 遍历缓存队列，找出已完成的缓存*/
    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_queue->irqqueue){
        p_vc_buf = usb_container_of(p_node, struct usbh_vc_buffer, node);

        if (p_vc_buf->state == UVC_BUF_STATE_DONE) {
            /* 设置缓存使用标志*/
            p_vc_buf->is_used = 1;
            /* 获取缓存的数据*/
            *p_buf = (uint32_t *)(p_vc_buf->p_mem);
            if (p_timeval != NULL) {
                p_timeval->ts_sec  = p_vc_buf->timeval.ts_sec;
                p_timeval->ts_usec = p_vc_buf->timeval.ts_usec;
            }
#if USB_OS_EN
            ret = usb_mutex_unlock(p_queue->p_lock);
            if (ret != USB_OK) {
                __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
                return ret;
            }
#endif
            /* 保存正在用的缓存地址*/
            p_stream->p_buf_tmp = p_vc_buf;
            return USB_OK;
        }
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_queue->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    p_stream->p_buf_tmp = NULL;
    return -USB_EPERM;
}

/**
 * \brief 释放视频缓存
 *
 * \param[in] p_buf     要释放的一帧视频缓存地址
 * \param[in] p_stream  视频数据流
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_video_buf_put(uint8_t               *p_buf,
                          struct usbh_vc_stream *p_stream){
#if USB_OS_EN
    int                   ret;
#endif
    struct usbh_vc_queue *p_queue = NULL;

    if (p_stream == NULL) {
        return -USB_EINVAL;
    }

    p_queue = (struct usbh_vc_queue *)p_stream->p_queue;
    if (p_queue == NULL) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_queue->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    if (p_stream->p_buf_tmp) {
        struct usbh_vc_buffer *p_buf = (struct usbh_vc_buffer *)p_stream->p_buf_tmp;
        /* 重置缓存状态*/
        p_buf->bytes_used   = 0;
        p_buf->error_no     = 0;
        p_buf->state        = UVC_BUF_STATE_QUEUED;
        p_buf->is_used      = 0;
        p_stream->p_buf_tmp = NULL;
    }

#if USB_OS_EN
    ret = usb_mutex_unlock(p_queue->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    return USB_OK;
}
