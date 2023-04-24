/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/uvc/usbh_vc_drv.h"
#include "core/include/host/class/uvc/usbh_vc_stream.h"
#include "core/include/host/class/uvc/usbh_vc_entity.h"
#include <stdlib.h>

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbh_vc_lib __g_uvc_lib;

extern int usbh_vc_video_init(struct usbh_vc_stream *p_stream);
extern int usbh_vc_video_enable(struct usbh_vc_stream *p_stream, uint8_t n_trp);
extern int usbh_vc_video_disable(struct usbh_vc_stream *p_stream);
extern int usbh_vc_video_resume(struct usbh_vc_stream *p_stream, usb_bool_t is_reset);
extern int usbh_vc_video_suspend(struct usbh_vc_stream *p_stream);
extern int usbh_vc_video_probe(struct usbh_vc_stream  *p_stream,
                               struct uvc_stream_ctrl *p_probe);
extern int usbh_trp_xfer_cancel(struct usbh_trp *p_trp);
extern void usbh_vc_video_free(struct usbh_vc_stream *p_stream, int is_buf_free);
extern int usbh_vc_ref_get(struct usbh_vc *p_vc);
extern int usbh_vc_ref_put(struct usbh_vc *p_vc);
extern int usbh_vc_video_buf_get(uint32_t             **p_buf,
                                 struct usbh_vc_stream *p_stream,
                                 struct uvc_timeval    *p_timeval);
extern int usbh_vc_video_buf_put(uint8_t               *p_buf,
                                 struct usbh_vc_stream *p_stream);

/*******************************************************************************
 * Static
 ******************************************************************************/
/* \brief 视频格式*/
static struct uvc_format_desc uvc_fmts[] = {
    {
        .p_name     = "YUV 4:2:2 (YUYV)",
        .guid       = UVC_GUID_FORMAT_YUY2,
        .fcc        = UVC_PIX_FMT_YUYV,
    },
    {
        .p_name     = "YUV 4:2:2 (YUYV)",
        .guid       = UVC_GUID_FORMAT_YUY2_ISIGHT,
        .fcc        = UVC_PIX_FMT_YUYV,
    },
    {
        .p_name     = "YUV 4:2:0 (NV12)",
        .guid       = UVC_GUID_FORMAT_NV12,
        .fcc        = UVC_PIX_FMT_NV12,
    },
    {
        .p_name     = "MJPEG",
        .guid       = UVC_GUID_FORMAT_MJPEG,
        .fcc        = UVC_PIX_FMT_MJPEG,
    },
    {
        .p_name     = "YVU 4:2:0 (YV12)",
        .guid       = UVC_GUID_FORMAT_YV12,
        .fcc        = UVC_PIX_FMT_YVU420,
    },
    {
        .p_name     = "YUV 4:2:0 (I420)",
        .guid       = UVC_GUID_FORMAT_I420,
        .fcc        = UVC_PIX_FMT_YUV420,
    },
    {
        .p_name     = "YUV 4:2:0 (M420)",
        .guid       = UVC_GUID_FORMAT_M420,
        .fcc        = UVC_PIX_FMT_M420,
    },
    {
        .p_name     = "YUV 4:2:2 (UYVY)",
        .guid       = UVC_GUID_FORMAT_UYVY,
        .fcc        = UVC_PIX_FMT_UYVY,
    },
    {
        .p_name     = "Greyscale 8-bit (Y800)",
        .guid       = UVC_GUID_FORMAT_Y800,
        .fcc        = UVC_PIX_FMT_GREY,
    },
    {
        .p_name     = "Greyscale 8-bit (Y8  )",
        .guid       = UVC_GUID_FORMAT_Y8,
        .fcc        = UVC_PIX_FMT_GREY,
    },
    {
        .p_name     = "Greyscale 10-bit (Y10 )",
        .guid       = UVC_GUID_FORMAT_Y10,
        .fcc        = UVC_PIX_FMT_Y10,
    },
    {
        .p_name     = "Greyscale 12-bit (Y12 )",
        .guid       = UVC_GUID_FORMAT_Y12,
        .fcc        = UVC_PIX_FMT_Y12,
    },
    {
        .p_name     = "Greyscale 16-bit (Y16 )",
        .guid       = UVC_GUID_FORMAT_Y16,
        .fcc        = UVC_PIX_FMT_Y16,
    },
    {
        .p_name     = "BGGR Bayer (BY8 )",
        .guid       = UVC_GUID_FORMAT_BY8,
        .fcc        = UVC_PIX_FMT_SBGGR8,
    },
    {
        .p_name     = "BGGR Bayer (BA81)",
        .guid       = UVC_GUID_FORMAT_BA81,
        .fcc        = UVC_PIX_FMT_SBGGR8,
    },
    {
        .p_name     = "GBRG Bayer (GBRG)",
        .guid       = UVC_GUID_FORMAT_GBRG,
        .fcc        = UVC_PIX_FMT_SGBRG8,
    },
    {
        .p_name     = "GRBG Bayer (GRBG)",
        .guid       = UVC_GUID_FORMAT_GRBG,
        .fcc        = UVC_PIX_FMT_SGRBG8,
    },
    {
        .p_name     = "RGGB Bayer (RGGB)",
        .guid       = UVC_GUID_FORMAT_RGGB,
        .fcc        = UVC_PIX_FMT_SRGGB8,
    },
    {
        .p_name     = "RGB565",
        .guid       = UVC_GUID_FORMAT_RGBP,
        .fcc        = UVC_PIX_FMT_RGB565,
    },
    {
        .p_name     = "BGR 8:8:8 (BGR3)",
        .guid       = UVC_GUID_FORMAT_BGR3,
        .fcc        = UVC_PIX_FMT_BGR24,
    },
    {
        .p_name     = "H.264",
        .guid       = UVC_GUID_FORMAT_H264,
        .fcc        = UVC_PIX_FMT_H264,
    },
};

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 通过 ID 寻找视频流
 *
 * \param[in] dev USB 视频类设备结构体
 * \param[in] id  视频流 ID
 *
 * \retval 成功返回找到的流结构体
 */
struct usbh_vc_stream *usbh_vc_stream_find_by_id(struct usbh_vc *p_vc, int id){
    struct usb_list_node  *p_node     = NULL;
    struct usb_list_node  *p_node_tmp = NULL;
    struct usbh_vc_stream *p_stream   = NULL;

    if (p_vc == NULL) {
        return NULL;
    }

    /* 遍历视频流链表*/
    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_vc->streams) {
        p_stream = usb_container_of(p_node, struct usbh_vc_stream, node);

        /* 匹配ID */
        if (p_stream->header.terminal_link == id){
            return p_stream;
        }
    }
    return NULL;
}

/**
 * \brief 获取色域
 */
static uint32_t __vc_colorspace(const uint8_t primaries){
    static const uint8_t colorprimaries[] = {
        0,
        UVC_COLORSPACE_SRGB,
        UVC_COLORSPACE_470_SYSTEM_M,
        UVC_COLORSPACE_470_SYSTEM_BG,
        UVC_COLORSPACE_SMPTE170M,
        UVC_COLORSPACE_SMPTE240M,
    };

    if (primaries < USB_NELEMENTS(colorprimaries)) {
        return colorprimaries[primaries];
    }
    return 0;
}

/**
 * \brief 通过全局唯一标识符(Globally Unique Identifier) 寻找对应的 USB 视频类格式
 *
 * \param[in] guid 全局唯一标识符
 *
 * \retval 成功返回找到的 USB 视频类格式
 */
static struct uvc_format_desc *__vc_format_get_by_guid(const uint8_t guid[16]){
    /* 获取格式表长度*/
    unsigned int len = USB_NELEMENTS(uvc_fmts);
    unsigned int i;

    for (i = 0; i < len; ++i) {
        /* 比对 GUID*/
        if (memcmp(guid, uvc_fmts[i].guid, 16) == 0)
            return &uvc_fmts[i];
    }

    return NULL;
}

/**
 * \brief USB 视频类分析格式描述符
 */
static int __vc_format_parse(struct usbh_vc        *p_vc,
                             struct usbh_vc_stream *p_stream,
                             struct uvc_format     *p_format,
                             uint32_t             **p_intervals,
                             unsigned char         *p_buf,
                             int                    buf_len){

    struct usbh_interface  *p_intf           = p_stream->p_intf;
    struct uvc_format_desc *p_fmt_desc       = NULL;
    struct uvc_frame       *p_frame          = NULL;
    const uint8_t          *p_start          = p_buf;
    uint32_t                width_multiplier = 1;
    uint32_t                interval;
    int                     i, n;
    uint8_t                 ftype;

    /* 获取格式类型*/
    p_format->type  = p_buf[2];
    /* 获取格式索引*/
    p_format->index = p_buf[3];

    switch (p_buf[2]) {
        case UVC_VS_FORMAT_UNCOMPRESSED:
        case UVC_VS_FORMAT_FRAME_BASED:
            n = p_buf[2] == UVC_VS_FORMAT_UNCOMPRESSED ? 27 : 28;
            if (buf_len < n) {
                __USB_ERR_INFO("device video streaming interface %d format illegal\r\n", USBH_INTF_NUM_GET(p_intf));
                return -USB_EILLEGAL;
            }
            /* 在这个 GUID 中寻找对应的格式描述符(Find the format descriptor from its GUID.) */
            p_fmt_desc = __vc_format_get_by_guid(&p_buf[5]);
            if (p_fmt_desc != NULL) {
                strlcpy(p_format->name, p_fmt_desc->p_name, sizeof(p_format->name));
                p_format->fcc = p_fmt_desc->fcc;
            } else {
            	__USB_INFO("vedio format %pUl unknown\r\n", &p_buf[5]);
                snprintf(p_format->name, sizeof(p_format->name), "%pUl\n", &p_buf[5]);
                p_format->fcc = 0;
            }
            p_format->bpp = p_buf[21];
            /*设置兼容，有些设备报告的格式与实际发送的格式不匹配*/
            //todo
            if (p_buf[2] == UVC_VS_FORMAT_UNCOMPRESSED) {
                ftype = UVC_VS_FRAME_UNCOMPRESSED;
            } else {
                ftype = UVC_VS_FRAME_FRAME_BASED;
                if (p_buf[27]) {
                	p_format->flags = UVC_FMT_FLAG_COMPRESSED;
                }
            }
            break;
        /* MJPEG格式*/
        case UVC_VS_FORMAT_MJPEG:
            if (buf_len < 11) {
            	__USB_ERR_INFO("device video streaming interface %d format illegal\r\n", USBH_INTF_NUM_GET(p_intf));
                return -USB_EILLEGAL;
            }
            /* 设置格式名字*/
            strlcpy(p_format->name, "MJPEG", sizeof(p_format->name));
            /* 格式压缩类型*/
            p_format->fcc   = UVC_PIX_FMT_MJPEG;
            /* 格式标志*/
            p_format->flags = UVC_FMT_FLAG_COMPRESSED;
            p_format->bpp   = 0;
            /* 帧类型*/
            ftype = UVC_VS_FRAME_MJPEG;
            break;
        /* DV格式*/
        case UVC_VS_FORMAT_DV:
            if (buf_len < 9) {
            	__USB_ERR_INFO("device video streaming interface %d format illegal\r\n", USBH_INTF_NUM_GET(p_intf));
                return -USB_EILLEGAL;
            }
            switch (p_buf[8] & 0x7f) {
                case 0:
                    strlcpy(p_format->name, "SD-DV", sizeof(p_format->name));
                    break;
                case 1:
                    strlcpy(p_format->name, "SDL-DV", sizeof(p_format->name));
                    break;
                case 2:
                    strlcpy(p_format->name, "HD-DV", sizeof(p_format->name));
                    break;
                default:
                	__USB_ERR_INFO("device video streaming interface %d: unknown DV format %u\r\n",
                                    USBH_INTF_NUM_GET(p_intf), p_buf[8]);
                    return -USB_EILLEGAL;
            }
            strlcat(p_format->name, p_buf[8] & (1 << 7) ? " 60Hz" : " 50Hz", sizeof(p_format->name));

            p_format->fcc   = UVC_PIX_FMT_DV;
            p_format->flags = UVC_FMT_FLAG_COMPRESSED | UVC_FMT_FLAG_STREAM;
            p_format->bpp   = 0;
            ftype           = 0;

            /* 创建一个假的帧描述符 */
            p_frame = &p_format->p_frame[0];
            memset(&p_format->p_frame[0], 0, sizeof(p_format->p_frame[0]));
            p_frame->frame_interval_type    = 1;
            p_frame->default_frame_interval = 1;
            p_frame->p_frame_interval       = *p_intervals;
            *(*p_intervals)++  = 1;
            p_format->n_frames = 1;
            break;
        /* MPEG2-TS 格式或 MPEG1-SS/MPEG2-PS 格式*/
        case UVC_VS_FORMAT_MPEG2TS:
        case UVC_VS_FORMAT_STREAM_BASED:
            /* 还不支持*/
        default:
        	__USB_ERR_INFO("device video streaming interface %d unsupported format %u\r\n",
                            USBH_INTF_NUM_GET(p_intf), p_buf[2]);
            return -USB_EILLEGAL;
    }
    __USB_INFO("found format %s\r\n", p_format->name);
    /* 跳过格式描述符*/
    buf_len -= p_buf[0];
    p_buf   += p_buf[0];
    /* 分析帧描述符。只有无压缩，MJPEG 和 通用基本帧有帧描述符*/
    while (buf_len > 2 && p_buf[1] == (USB_REQ_TYPE_CLASS | USB_DT_INTERFACE) && p_buf[2] == ftype) {
        p_frame = &p_format->p_frame[p_format->n_frames];

        if (ftype != UVC_VS_FRAME_FRAME_BASED) {
            n = buf_len > 25 ? p_buf[25] : 0;
        } else {
            n = buf_len > 21 ? p_buf[21] : 0;
        }
        n = n ? n : 3;

        if (buf_len < 26 + 4 * n) {
        	__USB_ERR_INFO("device %d video streaming interface %d frame illegal\r\n",
                            USBH_INTF_NUM_GET(p_intf));
            return -USB_EILLEGAL;
        }

        p_frame->frame_index  = p_buf[3];
        p_frame->capabilities = p_buf[4];
        p_frame->width        = usb_get_unaligned_le16(&p_buf[5]) * width_multiplier;
        p_frame->height       = usb_get_unaligned_le16(&p_buf[7]);
        p_frame->min_bit_rate = usb_get_unaligned_le16(&p_buf[9]);
        p_frame->max_bit_rate = usb_get_unaligned_le16(&p_buf[13]);

        if (ftype != UVC_VS_FRAME_FRAME_BASED) {
        	p_frame->max_video_frame_buffer_size = usb_get_unaligned_le32(&p_buf[17]);
        	p_frame->default_frame_interval      = usb_get_unaligned_le32(&p_buf[21]);
        	p_frame->frame_interval_type         = p_buf[25];
        } else {
        	p_frame->max_video_frame_buffer_size = 0;
        	p_frame->default_frame_interval      = usb_get_unaligned_le32(&p_buf[17]);
        	p_frame->frame_interval_type         = p_buf[21];
        }
        p_frame->p_frame_interval = *p_intervals;

        if (!(p_format->flags & UVC_FMT_FLAG_COMPRESSED)) {
        	p_frame->max_video_frame_buffer_size = p_format->bpp * p_frame->width * p_frame->height / 8;
        }
        for (i = 0; i < n; ++i) {
            interval = usb_get_unaligned_le32(&p_buf[26 + 4 * i]);
            *(*p_intervals)++ = interval ? interval : 1;
        }
        /* 确保默认的帧间隔保持在边界之间*/
        n -= p_frame->frame_interval_type ? 1 : 2;
        p_frame->default_frame_interval = min(p_frame->p_frame_interval[n],
                                          max(p_frame->p_frame_interval[0],
                                              p_frame->default_frame_interval));
        /* 限制帧速率兼容*/
        //todo
//        __USB_INFO("- %ux%u (%u.%u fps)\r\n", p_frame->width, p_frame->height,
//                                              10000000 / p_frame->default_frame_interval,
//                                             (100000000 / p_frame->default_frame_interval) % 10);

        p_format->n_frames++;
        buf_len -= p_buf[0];
        p_buf   += p_buf[0];
    }
    if (buf_len > 2 && p_buf[1] == (USB_REQ_TYPE_CLASS|USB_DT_INTERFACE) &&
             p_buf[2] == UVC_VS_STILL_IMAGE_FRAME) {
    	buf_len -= p_buf[0];
    	p_buf   += p_buf[0];
    }

    if (buf_len > 2 && p_buf[1] == (USB_REQ_TYPE_CLASS|USB_DT_INTERFACE) &&
    		p_buf[2] == UVC_VS_COLORFORMAT) {
        if (buf_len < 6) {
        	__USB_ERR_INFO("device video streaming interface %d color format illegal\r\n", USBH_INTF_NUM_GET(p_intf));
            return -USB_EILLEGAL;
        }
        /* 获取色域*/
        p_format->colorspace = __vc_colorspace(p_buf[3]);

        buf_len -= p_buf[0];
        p_buf   += p_buf[0];
    }
    return p_buf - p_start;
}

#if 0
/**
 * \brief 根据名字找到对应 USB 摄像类设备数据流
 */
static int __vc_stream_get(char *p_name, struct usbh_vc_stream **p_stream){
    int                    ret          = -USB_ENODEV;
    struct usbh_vc        *p_vc         = NULL;
    struct usb_list_node  *p_node       = NULL;
    struct usb_list_node  *p_node_tmp   = NULL;
    struct usbh_vc_stream *p_stream_tmp = NULL;
#if USB_OS_EN
    int                    ret_tmp;
#endif

#if USB_OS_EN
    ret = usb_lib_mutex_lock(&__g_uvc_lib.lib);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    USB_LIB_LIST_FOR_EACH_NODE(p_node, p_node_tmp, __g_uvc_lib.lib){
    	p_vc = usb_list_node_get(p_node, struct usbh_vc, node);

    	if (p_vc->is_removed == USB_TRUE) {
            continue;
    	}

#if USB_OS_EN
    	ret_tmp = usb_mutex_lock(p_vc->p_lock, UVC_LOCK_TIMEOUT);
        if (ret_tmp != USB_OK) {
    	    __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret_tmp);
    	    ret = ret_tmp;
            goto __exit;
        }
#endif
        usb_list_for_each_node(p_stream_tmp,
                              &p_vc->streams,
                               struct usbh_vc_stream,
                               node){
            if ((strcmp(p_stream_tmp->name, p_name) == 0) &&
                    (p_stream_tmp->is_init == USB_TRUE)){

                ret = usbh_vc_ref_get(p_stream_tmp->p_vc);
#if USB_OS_EN
            	ret_tmp = usb_mutex_unlock(p_vc->p_lock);
                if (ret_tmp != USB_OK) {
            	    __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
            	    if (ret != USB_OK) {
                	    ret = ret_tmp;
                        goto __exit;
            	    }
                }
#endif
                if (ret != USB_OK) {
                	__USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
                    goto __exit;
                }

                *p_stream = p_stream_tmp;

                ret = USB_OK;

                goto __exit;
            }
        }
    }
__exit:
#if USB_OS_EN
    ret_tmp = usb_lib_mutex_unlock(&__g_uvc_lib.lib);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}
#endif

/**
 * \brief USB 视频类分析视频流描述符
 *
 * \param[in] p_vc   USB 视频类设备结构体
 * \param[in] p_intf 相关的视频流接口
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_parse(struct usbh_vc        *p_vc,
                         struct usbh_interface *p_intf){
    uint8_t               *p_buf_tmp, *p_buf;
    int                    buf_len_tmp, buf_len;
    struct usbh_vc_stream *p_stream    = NULL;
    int                    ret, size, n, p;
    struct uvc_format     *p_format;
    struct uvc_frame      *p_frame;
    uint32_t               n_formats   = 0;
    uint32_t               n_frames    = 0;
    uint32_t               n_intervals = 0;
    uint8_t               *p_t         = NULL;
    uint32_t              *p_interval;
    uint16_t               mps;
    struct usb_list_node  *p_node      = NULL;
    struct usb_list_node  *p_node_tmp  = NULL;
    struct usbh_interface *p_intf_tmp  = NULL;

    /* 检查 USB 接口的子类*/
    if (USBH_INTF_SUB_CLASS_GET(p_intf) != UVC_VSC_VIDEOSTREAMING) {
        __USB_ERR_INFO("usb vedio device interface %d isn't a video streaming interface\r\n", USBH_INTF_NUM_GET(p_intf));
        return -USB_EILLEGAL;
    }

    p_buf   = p_intf->p_extra;
    buf_len = p_intf->extra_len;

    p_stream = usb_lib_malloc(&__g_uvc_lib.lib, sizeof(struct usbh_vc_stream));
    if (p_stream == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_stream, 0, sizeof(struct usbh_vc_stream));
    /* 设置挂起标志*/
    p_stream->is_frozen = 1;
#if USB_OS_EN
    p_stream->clock.p_lock = usb_lib_mutex_create(&__g_uvc_lib.lib);
    if (p_stream->clock.p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        usb_lib_mfree(&__g_uvc_lib.lib, p_stream);
        return -USB_EPERM;
    }
    p_stream->p_lock = usb_lib_mutex_create(&__g_uvc_lib.lib);
    if (p_stream->p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");

        ret = usb_lib_mutex_destroy(&__g_uvc_lib.lib, p_stream->clock.p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
        }
        usb_lib_mfree(&__g_uvc_lib.lib, p_stream);
        return -USB_EPERM;
    }
#endif

    p_stream->p_vc     = p_vc;
    p_stream->p_intf   = p_intf;
    p_stream->intf_num = p_intf->p_desc->interface_number;
    /* Pico iMage 网络摄像机在端点描述符之后有其特定于类的接口描述符*/
    //todo
    /* 跳过标准接口描述符 */
    while (buf_len > 2 && p_buf[1] != (USB_REQ_TYPE_CLASS|USB_DT_INTERFACE)) {
        buf_len -= p_buf[0];
        p_buf += p_buf[0];
    }
    if (buf_len <= 2) {
        __USB_ERR_INFO("no class-specific streaming interface descriptors found\r\n");
        goto __failed;
    }
    switch (p_buf[2]) {
        /* USB 视频类输出头描述符*/
        case UVC_VS_OUTPUT_HEADER:
            p_stream->type = UVC_BUF_TYPE_VIDEO_OUTPUT;
            size = 9;
        break;
        /* USB 视频类输入头描述符*/
        case UVC_VS_INPUT_HEADER:
            p_stream->type = UVC_BUF_TYPE_VIDEO_CAPTURE;
            size = 13;
        break;
        default:
            __USB_ERR_INFO("device video streaming interface %d hander descriptor not found\r\n", USBH_INTF_NUM_GET(p_intf));
            goto __failed;
    }
    /* 获取视频有效负载格式描述符的数量*/
    p = buf_len >= 4 ? p_buf[3] : 0;
    /* 每个 p_ontrols(x) 字段的大小*/
    n = buf_len >= size ? p_buf[size - 1] : 0;
    /* 检查描述符数据长度*/
    if (buf_len < size + p * n) {
        __USB_ERR_INFO("device video streaming interface %d HEADER descriptor is invalid\r\n", USBH_INTF_NUM_GET(p_intf));
        goto __failed;
    }
    /* 设置 USB 视频类视频流头的格式数量*/
    p_stream->header.num_formats = p;
    /* 设置 USB 视频类视频流的端点地址*/
    p_stream->header.endpoint_address = p_buf[6];
    /* 如果是 USB 视频类输入头描述符*/
    if (p_buf[2] == UVC_VS_INPUT_HEADER) {
        /* 设置此视频流接口的功能：D0：支持动态格式更改；D7-D1：保留，设置为0。*/
        p_stream->header.info = p_buf[7];
        /* 此接口的视频端点连接到输出终端的中断ID。*/
        p_stream->header.terminal_link = p_buf[8];
        /* 支持静止图像捕获的方法*/
        p_stream->header.still_capture_method = p_buf[9];
        /* 指示此接口是否支持硬件触发*/
        p_stream->header.trigger_support = p_buf[10];
        /* 指示主机软件如何响应来自此接口的硬件触发中断事件*/
        p_stream->header.trigger_usage   = p_buf[11];
    } else {
        /* 设置此视频流接口的功能：D0：支持动态格式更改；D7-D1：保留，设置为0。*/
        p_stream->header.terminal_link = p_buf[7];
    }
    /* 设置 p_controls(x) 的大小*/
    p_stream->header.control_size = n;
    /* 把 p_controls(x) 的缓存复制一份赋值给视频流头结构体*/
    p_t = usb_lib_malloc(&__g_uvc_lib.lib, (p * n));
    if (p_t == NULL) {
        ret = -USB_ENOMEM;
        goto __failed;
    }
    memset(p_t, 0, p * n);
    memcpy(p_t, &p_buf[size], p * n);

    p_stream->header.p_controls = p_t;

    /* 更新额外描述符缓存的长度*/
    buf_len -= p_buf[0];
    /* 移动额外描述符缓存位置*/
    p_buf += p_buf[0];

    p_buf_tmp   = p_buf;
    buf_len_tmp = buf_len;
    /* 计算格式和帧描述符 */
    while (buf_len_tmp > 2 && p_buf_tmp[1] == (USB_REQ_TYPE_CLASS | USB_DT_INTERFACE)) {
        switch (p_buf_tmp[2]) {
            case UVC_VS_FORMAT_UNCOMPRESSED:
            case UVC_VS_FORMAT_MJPEG:
            case UVC_VS_FORMAT_FRAME_BASED:
                n_formats++;
                break;
            case UVC_VS_FORMAT_DV:
                /* DV 格式没有帧描述符。我们将创建一个具有伪帧间隔的伪帧描述符 */
                n_formats++;
                n_frames++;
                n_intervals++;
                break;
           case UVC_VS_FORMAT_MPEG2TS:
           case UVC_VS_FORMAT_STREAM_BASED:
                __USB_ERR_INFO("device video streaming interface %d format %u is not supported\r\n",
                               USBH_INTF_NUM_GET(p_intf), p_buf_tmp[2]);
                break;
           case UVC_VS_FRAME_UNCOMPRESSED:
           case UVC_VS_FRAME_MJPEG:
                n_frames++;
                if (buf_len_tmp > 25) {
                    n_intervals += p_buf_tmp[25] ? p_buf_tmp[25] : 3;
                }
                break;
           case UVC_VS_FRAME_FRAME_BASED:
                n_frames++;
                if (buf_len_tmp > 21) {
                    n_intervals += p_buf_tmp[21] ? p_buf_tmp[21] : 3;
                }
                break;
        }
        /* 移动到下一个描述符*/
        buf_len_tmp -= p_buf_tmp[0];
        p_buf_tmp   += p_buf_tmp[0];
    }
    if (n_formats == 0) {
        __USB_ERR_INFO("device video streaming interface %d has no supported formats defined\r\n",
                        USBH_INTF_NUM_GET(p_intf));
        goto __failed;
    }
    /* 计算 USB 视频类格式结构体的大小*/
    size = n_formats * sizeof(*p_format) + n_frames * sizeof(*p_frame)
         + n_intervals * sizeof(*p_interval);

    p_format = usb_lib_malloc(&__g_uvc_lib.lib, size);
    if (p_format == NULL) {
        ret = -USB_ENOMEM;
        goto __failed;
    }
    memset(p_format, 0, size);
    /* 获取帧结构体地址*/
    p_frame = (struct uvc_frame *)&p_format[n_formats];
    /* 获取服务间隔的地址*/
    p_interval = (uint32_t *)&p_frame[n_frames];

    p_stream->p_format  = p_format;
    p_stream->n_formats = n_formats;

    /* 分析格式描述符*/
    while (buf_len > 2 && p_buf[1] == (USB_REQ_TYPE_CLASS | USB_DT_INTERFACE)) {
        switch (p_buf[2]) {
            case UVC_VS_FORMAT_UNCOMPRESSED:
            case UVC_VS_FORMAT_MJPEG:
            case UVC_VS_FORMAT_DV:
            case UVC_VS_FORMAT_FRAME_BASED:
                p_format->p_frame = p_frame;

                ret = __vc_format_parse(p_vc,
                                        p_stream,
                                        p_format,
                                       &p_interval,
                                        p_buf,
                                        buf_len);
                if (ret < 0) {
                    goto __failed;
                }
                p_frame += p_format->n_frames;
                p_format++;

                buf_len -= ret;
                p_buf   += ret;
                continue;

            default:
                break;
        }
        /* 移动到下一个描述符*/
        buf_len -= p_buf[0];
        p_buf   += p_buf[0];
    }

    if (buf_len > 0){
        __USB_ERR_INFO("device video streaming interface %d has %u bytes "
                       "of trailing descriptor garbage\r\n",
                        USBH_INTF_NUM_GET(p_intf), buf_len);
    }
    /* 分析备用设置找到最大的带宽*/
    /* 遍历其他接口*/
    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_vc->p_ufun->intf_list){
        struct usbh_endpoint *p_ep;

        p_intf_tmp = usb_container_of(p_node, struct usbh_interface, node);

        if (p_intf_tmp->p_desc->interface_sub_class == UVC_VSC_VIDEOSTREAMING) {
            /* 寻找端点*/
            ret = usbh_intf_ep_get(p_intf_tmp, p_stream->header.endpoint_address, &p_ep);
            if (ret != USB_OK) {
                continue;
            }
            /* 获取端点最大包大小*/
            mps = USBH_EP_MPS_GET(p_ep);
            mps = (mps & 0x07ff) * (1 + ((mps >> 11) & 3));
            /* 检查端点最大包大小，寻找最大的端点包大小的端点*/
            if (mps > p_stream->mps){
                p_stream->mps = mps;
            }
        }
    }
    usb_list_node_add_tail(&p_stream->node, &p_vc->streams);

    __g_uvc_lib.n_stream++;

    return USB_OK;
__failed:
    if (p_stream->p_format) {
    	usb_lib_mfree(&__g_uvc_lib.lib, p_stream->p_format);
    }
    if (p_stream->header.p_controls) {
    	usb_lib_mfree(&__g_uvc_lib.lib, p_stream->header.p_controls);
    }
    if (p_stream) {
    	usb_lib_mfree(&__g_uvc_lib.lib, p_stream);
    }
    return ret;
}

/**
 * \brief USB 视频类视频流注册
 *
 * \param[in] dev USB   视频类设备结构体
 * \param[in] p_stream  要注册的视频流
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_register(struct usbh_vc        *p_vc,
                            struct usbh_vc_stream *p_stream){
    int ret;

    if ((p_vc == NULL) || (p_stream == NULL)) {
        return -USB_EINVAL;
    }

    /* 用默认流参数初始化流接口*/
    ret = usbh_vc_video_init(p_stream);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb video initialize failed(%d)\r\n", ret);
        return ret;
    }

    if (p_stream->type == UVC_BUF_TYPE_VIDEO_CAPTURE) {
        p_stream->p_chain->caps |= UVC_CAP_VIDEO_CAPTURE;
    } else {
        p_stream->p_chain->caps |= UVC_CAP_VIDEO_OUTPUT;
    }

    p_vc->n_streams++;

    p_stream->is_init = USB_TRUE;

    return USB_OK;
}

/**
 * \brief USB 视频类设备启动视频流
 */
static int __vc_stream_start(struct usbh_vc_stream *p_stream, uint8_t n_trp){
    int ret;

#if 0
    /* 设置非挂起标志*/
    if(stream->frozen == 0){
        return USB_OK;
    }
#endif
#if USB_OS_EN
    int ret_tmp;

    /* 启动视频流*/
    ret = usb_mutex_lock(p_stream->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 使能视频流*/
    ret = usbh_vc_video_enable(p_stream, n_trp);
    if (ret == USB_OK) {
        p_stream->is_frozen = 0;
    }
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
 * \brief USB 视频类设备停止视频流
 */
static int __vc_stream_stop(struct usbh_vc_stream *p_stream){
    int ret;
#if USB_OS_EN
    int ret_tmp;

    ret = usb_mutex_lock(p_stream->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 设置挂起标志*/
//    if(stream->frozen == 1){
//        return USB_OK;
//    }
//    stream->frozen = 1;
    /* 关闭视频流*/
    ret = usbh_vc_video_disable(p_stream);
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
 * \brief 查找与给定帧格式和大小的请求帧间隔最接近的帧间隔。这应该作为视频探测和提交协商
 *        的一部分，这部分应该由设备完成，但是一些硬件没有实现该功能。
 */
static uint32_t __vc_frame_interval_try(struct uvc_frame *p_frame,
                                        uint32_t          interval){
    uint32_t i;

    if (p_frame->frame_interval_type) {
        uint32_t best = -1, dist;
        /* 遍历所有设备支持的帧间隔类型*/
        for (i = 0; i < p_frame->frame_interval_type; ++i) {
            /* 与请求的帧间隔求差*/
            dist = interval > p_frame->p_frame_interval[i] ?
                   interval - p_frame->p_frame_interval[i] : p_frame->p_frame_interval[i] - interval;

            if (dist > best) {
                break;
            }
            best = dist;
        }
        /* 获取差值最小的帧间隔*/
        interval = p_frame->p_frame_interval[i - 1];
    } else {
        const uint32_t min  = p_frame->p_frame_interval[0];
        const uint32_t max  = p_frame->p_frame_interval[1];
        const uint32_t step = p_frame->p_frame_interval[2];

        interval = min + (interval - min + step / 2) / step * step;
        if (interval > max) {
            interval = max;
        }
    }

    return interval;
}

/**
 * \brief 检查 USB 视频类视频流是否支持格式
 */
static int __vc_stream_format_try(struct usbh_vc_stream   *p_stream,
                                  struct uvc_format_cfg   *p_fmt,
                                  struct uvc_stream_ctrl  *p_probe,
                                  struct uvc_format      **p_format_ret,
                                  struct uvc_frame       **p_frame_ret){
    struct uvc_format *p_format = NULL;
    struct uvc_frame  *p_frame  = NULL;
    uint16_t           rw, rh;
    uint32_t           d, maxd, i, interval;
    int                ret      = USB_OK;
    uint8_t           *p_fcc    = NULL;

    /* 检查格式类型与视频流类似是否匹配*/
    if (p_fmt->type != p_stream->type) {
        return -USB_EILLEGAL;
    }
    /* 获取像素点格式*/
    p_fcc = (uint8_t *)&p_fmt->pix.pixelformat;

    __USB_INFO("trying format 0x%08x (%c%c%c%c): %ux%u\r\n",
                   p_fmt->pix.pixelformat, p_fcc[0], p_fcc[1], p_fcc[2], p_fcc[3],
                   p_fmt->pix.width, p_fmt->pix.height);

    /* 检查硬件是否支持请求的格式，否则使用默认格式 */
    for (i = 0; i < p_stream->n_formats; ++i) {
        p_format = &p_stream->p_format[i];
        if (p_format->fcc == p_fmt->pix.pixelformat) {
            break;
        }
    }
    /* 不支持格式*/
    if (i == p_stream->n_formats) {
        /* 使用默认格式*/
        p_format = p_stream->p_format_def;
        p_fmt->pix.pixelformat = p_format->fcc;
    }
    /* 寻找最接近的图像大小。图像大小之间的距离是请求大小和帧指定大小之间不重叠区域的像素大小。*/
    rw = p_fmt->pix.width;
    rh = p_fmt->pix.height;
    /* 不重叠区域大小先设置为最大*/
    maxd = (unsigned int)-1;

    for (i = 0; i < p_format->n_frames; ++i) {
        uint16_t w = p_format->p_frame[i].width;
        uint16_t h = p_format->p_frame[i].height;
        /* 算出重叠区域面积*/
        d = min(w, rw) * min(h, rh);
        /* 算出不重叠区域面积*/
        d = w*h + rw*rh - 2*d;
        /* 选择不重叠区域最小的帧*/
        if (d < maxd) {
            maxd    = d;
            p_frame = &p_format->p_frame[i];
        }

        if (maxd == 0) {
            break;
        }
    }

    if (p_frame == NULL) {
        __USB_ERR_INFO("unsupported size %ux%u\r\n", p_fmt->pix.width, p_fmt->pix.height);
        return -USB_EINVAL;
    }

    /* 使用默认的帧间隔*/
    interval = p_frame->default_frame_interval;
    __USB_INFO("using default frame interval %u.%u us (%u.%u fps)\r\n",
                   interval / 10, interval % 10, 10000000 / interval, (100000000 / interval) % 10);

    /* 设置格式索引，帧索引和帧间隔*/
    memset(p_probe, 0, sizeof(struct uvc_stream_ctrl));
    p_probe->hint         = 1;
    p_probe->format_index = p_format->index;
    p_probe->frame_index  = p_frame->frame_index;
    /* 检查帧间隔是否支持*/
    p_probe->frame_interval = __vc_frame_interval_try(p_frame, interval);

    /* 探测设置*/
    ret = usbh_vc_video_probe(p_stream, p_probe);
    if (ret != USB_OK) {
        goto __done;
    }

    /* 重新更新请求格式的值*/
    p_fmt->pix.width        = p_frame->width;
    p_fmt->pix.height       = p_frame->height;
    p_fmt->pix.field        = UVC_FIELD_NONE;
    p_fmt->pix.bytesperline = p_format->bpp * p_frame->width / 8;
    p_fmt->pix.sizeimage    = p_probe->max_video_frame_size;
    p_fmt->pix.colorspace   = p_format->colorspace;
    p_fmt->pix.priv         = 0;
    /* 返回格式和帧结构体*/
    if (p_format_ret != NULL) {
        *p_format_ret = p_format;
    }
    if (p_frame_ret != NULL) {
        *p_frame_ret = p_frame;
    }
__done:
    return ret;
}

/**
 * \brief USB 视频类数据流打开函数
 *
 * \param[in]  p_vc     USB 视频类设备
 * \param[in]  idx      视频流索引，最小是 1
 * \param[out] p_stream 返回打开成功的USB 视频类数据流地址
 *
 * \return 成功返回 USB_OK
 */
int usbh_vc_stream_open(struct usbh_vc         *p_vc,
                        uint8_t                 idx,
                        struct usbh_vc_stream **p_stream){
    int                    ret, i       = 1;
    struct usb_list_node  *p_node       = NULL;
    struct usb_list_node  *p_node_tmp   = NULL;
    struct usbh_vc_stream *p_stream_tmp = NULL;

    if ((p_vc == NULL) || (idx == 0) || (idx > p_vc->n_streams)) {
        return -USB_EINVAL;
    }

    if ((usb_lib_is_init(&__g_uvc_lib.lib) == USB_FALSE) ||
            (__g_uvc_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    if (p_vc->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

    *p_stream = NULL;

#if USB_OS_EN
    ret = usb_mutex_lock(p_vc->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_vc->streams){
        p_stream_tmp = usb_container_of(p_node, struct usbh_vc_stream, node);

        if (i == idx) {
            break;
        }
        i++;
    }

#if USB_OS_EN
    ret = usb_mutex_unlock(p_vc->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    ret = usbh_vc_ref_get(p_stream_tmp->p_vc);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
        return ret;
    }

    if (p_stream_tmp->opt_status == UVC_STREAM_CLOSE) {
        p_stream_tmp->opt_status = UVC_STREAM_OPEN;
    }

    p_stream_tmp->open_cnt++;

    *p_stream = p_stream_tmp;

    return USB_OK;
    //return usbh_trp_submit(&stream->dev->int_trp);
}

/**
 * \brief USB 视频类设备数据流关闭函数
 *
 * \param[in] p_stream USB 视频类设备数据流
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_close(struct usbh_vc_stream *p_stream){
    int ret;

    if (p_stream == NULL) {
        return -USB_EINVAL;
    }
    if ((p_stream->opt_status == UVC_STREAM_CLOSE) || (p_stream->open_cnt == 0)) {
        return USB_OK;
    }
    if (((p_stream->opt_status == UVC_STREAM_START) || (p_stream->opt_status == UVC_STREAM_SUSPEND)) \
            && (p_stream->open_cnt == 1)) {
        ret = usbh_vc_stream_stop(p_stream);
        if (ret != USB_OK) {
            return ret;
        }
    }

    if (!(--p_stream->open_cnt)) {
        p_stream->opt_status = UVC_STREAM_CLOSE;
    }

    return usbh_vc_ref_put(p_stream->p_vc);
}

/**
 * \brief 启动 USB 视频类设备数据流
 *
 * \param[in] p_stream USB 视频类设备数据流
 * \param[in] n_trp    传输请求包数量(2~5)
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_start(struct usbh_vc_stream *p_stream, uint8_t n_trp){
    int ret;

    if (p_stream == NULL) {
        return -USB_EINVAL;
    }
    if (p_stream->p_vc->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
    if ((p_stream->opt_status != UVC_STREAM_CONFIG) &&
            (p_stream->opt_status != UVC_STREAM_STOP)) {
        return -USB_EPERM;
    }

    /* 限制传输请求包数量*/
    if (n_trp > 5) {
        n_trp = 5;
    }
    if (n_trp < 2) {
        n_trp = 2;
    }

    ret = __vc_stream_start(p_stream, n_trp);
    if (ret == USB_OK) {
        p_stream->opt_status = UVC_STREAM_START;
    }
    return ret;
}

/**
 * \brief 停止 USB 视频类设备数据流
 *
 * \param[in] p_stream USB 视频类设备数据流
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_stop(struct usbh_vc_stream *p_stream){
    int ret;

    if (p_stream == NULL) {
        return -USB_EINVAL;
    }
    if ((p_stream->opt_status != UVC_STREAM_START) && (p_stream->opt_status != UVC_STREAM_SUSPEND)) {
        return -USB_EPERM;
    }

    ret = __vc_stream_stop(p_stream);
    if (ret == USB_OK) {
        p_stream->opt_status = UVC_STREAM_STOP;
    }
    return ret;
}

/**
 * \brief USB 视频类数据流配置
 *
 * \param[in] p_stream USB 视频类数据流
 * \param[in] p_cfg    数据流配置
 * \param[in] flag     可选配置标志(只配置必选项时，设置为 0 即可)
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_cfg_set(struct usbh_vc_stream     *p_stream,
                           struct usbh_vc_stream_cfg *p_cfg,
                           uint32_t                   flag){
    struct uvc_format_cfg       fmt;
    struct uvc_stream_ctrl      probe;
    struct uvc_format          *p_format = NULL;
    struct uvc_frame           *p_frame  = NULL;
    struct usbh_vc_video_chain *p_chain  = NULL;
    int                         ret;
#if USB_OS_EN
    int                         ret_tmp;
#endif

    if ((p_stream == NULL) || (p_cfg == NULL)) {
        return -USB_EINVAL;
    }
    if (p_stream->p_vc->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
    /* 检查状态*/
    if ((p_stream->opt_status != UVC_STREAM_OPEN) &&
            (p_stream->opt_status != UVC_STREAM_CONFIG) &&
            (p_stream->opt_status != UVC_STREAM_STOP)) {
        return -USB_EPERM;
    }
#if USB_OS_EN
    /* 锁住数据流*/
    ret = usb_mutex_lock(p_stream->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* USB 摄像头类型*/
    fmt.type       = p_cfg->type;
    /* 设置图像分辨率*/
    fmt.pix.width  = p_cfg->width;
    fmt.pix.height = p_cfg->height;
    /* 设置输出图像格式*/
    if (p_cfg->format == UVC_YUV422) {
        fmt.pix.pixelformat = UVC_PIX_FMT_YUYV;
    } else if (p_cfg->format == UVC_MJPEG) {
        fmt.pix.pixelformat = UVC_PIX_FMT_MJPEG;
    }
    /* 检查格式类型和视频流类型是否匹配*/
    if (fmt.type != p_stream->type) {
#if USB_OS_EN
        ret = usb_mutex_unlock(p_stream->p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
        return -USB_EILLEGAL;
    }

    /* 检查是否支持请求的格式*/
    ret = __vc_stream_format_try(p_stream, &fmt, &probe, &p_format, &p_frame);
    if (ret != USB_OK) {
#if USB_OS_EN
        ret_tmp = usb_mutex_unlock(p_stream->p_lock);
        if (ret_tmp != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
            return ret_tmp;
        }
#endif
        return ret;
    }

    /* 设置视频流控件*/
    p_stream->ctrl         = probe;
    /* 设置视频流当前格式*/
    p_stream->p_format_cur = p_format;
    /* 设置视频当前帧*/
    p_stream->p_frame_cur  = p_frame;

    /* 获取当前数据流视频链*/
    p_chain = usb_container_of(p_stream->p_vc->chains.p_next, struct usbh_vc_video_chain, node);
    if (p_chain == NULL) {
        return -USB_ENODEV;
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_stream->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 设置亮度*/
    if (flag & UVC_CFG_BRIGHTNESS) {
        ret = usbh_vc_chain_ctrl_set(p_chain, UVC_CID_BRIGHTNESS, p_cfg->brightness);
        if(ret != USB_OK){
            __USB_ERR_INFO("usb video class brightness set failed(%d)\r\n", ret);
        }
    }
    /* 设置对比度*/
    if (flag & UVC_CFG_CONTRAST) {
        ret = usbh_vc_chain_ctrl_set(p_chain, UVC_CID_CONTRAST, p_cfg->contrast);
        if (ret != USB_OK) {
            __USB_ERR_INFO("usb video class contrast set failed(%d)\r\n", ret);
        }
    }
    /* 设置色调*/
    if (flag & UVC_CFG_HUE) {
        ret = usbh_vc_chain_ctrl_set(p_chain, UVC_CID_HUE, p_cfg->hue);
        if (ret != USB_OK) {
            __USB_ERR_INFO("usb video class hue set failed(%d)\r\n", ret);
        }
    }
    /* 设置饱和度*/
    if (flag & UVC_CFG_SATURARION) {
        ret = usbh_vc_chain_ctrl_set(p_chain, UVC_CID_SATURATION, p_cfg->saturation);
        if (ret != USB_OK) {
            __USB_ERR_INFO("usb video class saturation set failed(%d)\r\n", ret);
        }
    }
    /* 设置锐度*/
    if (flag & UVC_CFG_SHARPNESS) {
        ret = usbh_vc_chain_ctrl_set(p_chain, UVC_CID_SHARPNESS, p_cfg->sharpness);
        if (ret != USB_OK) {
            __USB_ERR_INFO("usb video class sharpness set failed(%d)\r\n", ret);
        }
    }
    /* 设置伽马*/
    if (flag & UVC_CFG_GAMA) {
        ret = usbh_vc_chain_ctrl_set(p_chain, UVC_CID_GAMMA, p_cfg->gama);
        if (ret != USB_OK) {
            __USB_ERR_INFO("usb video class gama set failed(%d)\r\n", ret);
        }
    }
    /* 设置白平衡*/
    if (flag & UVC_CFG_WB) {
        ret = usbh_vc_chain_ctrl_set(p_chain, UVC_CID_WHITE_BALANCE_TEMPERATURE, p_cfg->wb);
        if (ret != USB_OK) {
            __USB_ERR_INFO("usb video class white balance set failed(%d)\r\n", ret);
        }
    }
    /* 设置灯管频率*/
    if (flag & UVC_CFG_PLF) {
        ret = usbh_vc_chain_ctrl_set(p_chain, UVC_CID_POWER_LINE_FREQUENCY, p_cfg->plf);
        if (ret != USB_OK) {
            __USB_ERR_INFO("usb video class power line frequenc set failed(%d)\r\n", ret);
        }
    }
    p_stream->opt_status = UVC_STREAM_CONFIG;

    return ret;
}

/**
 * \brief 获取 USB 视频类数据流配置
 *
 * \param[in] p_stream USB 视频类数据流
 * \param[in] p_cfg    数据流配置
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_cfg_get(struct usbh_vc_stream     *p_stream,
                           struct usbh_vc_stream_cfg *p_cfg){
    struct uvc_format          *p_format    = NULL;
    struct uvc_frame           *p_frame     = NULL;
    struct usbh_vc_video_chain *p_chain     = NULL;
    int                         ret, value  = 0;

    if ((p_stream == NULL) || (p_cfg == NULL)) {
        return -USB_EINVAL;
    }
    if (p_stream->p_vc->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
    /* 在关闭阶段*/
    if (p_stream->opt_status == UVC_STREAM_CLOSE) {
        return -USB_EPERM;
    };

    /* 锁住数据流*/
#if USB_OS_EN
    /* 锁住数据流*/
    ret = usb_mutex_lock(p_stream->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 获取当前数据流格式*/
    p_format = p_stream->p_format_cur;
    if (p_format == NULL) {
        return -USB_EILLEGAL;
    }
    /* 获取当前数据流帧*/
    p_frame = p_stream->p_frame_cur;
    if (p_frame == NULL) {
        return -USB_EILLEGAL;
    }

    if (p_format->fcc == UVC_PIX_FMT_YUYV) {
        p_cfg->format = UVC_YUV422;
    } else if(p_format->fcc == UVC_PIX_FMT_MJPEG) {
        p_cfg->format = UVC_MJPEG;
    }

    p_cfg->type   = p_stream->type;
    p_cfg->width  = p_frame->width;
    p_cfg->height = p_frame->height;
    /* 获取当前数据流视频链*/
    p_chain = usb_container_of(p_stream->p_vc->chains.p_next, struct usbh_vc_video_chain, node);
    if (p_chain == NULL) {
        return -USB_ENODEV;
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_stream->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 获取亮度值*/
    ret = usbh_vc_chain_ctrl_get(p_chain, UVC_CID_BRIGHTNESS, &value);
    if (ret == USB_OK) {
        p_cfg->brightness = value;
    }
    /* 获取对比度*/
    ret = usbh_vc_chain_ctrl_get(p_chain, UVC_CID_CONTRAST, &value);
    if (ret == USB_OK) {
        p_cfg->contrast = value;
    }
    /* 获取饱和度*/
    ret = usbh_vc_chain_ctrl_get(p_chain, UVC_CID_SATURATION, &value);
    if (ret == USB_OK) {
        p_cfg->saturation = value;
    }
    /* 获取色调*/
    ret = usbh_vc_chain_ctrl_get(p_chain, UVC_CID_HUE, &value);
    if (ret == USB_OK) {
        p_cfg->hue = value;
    }
    /* 获取伽马*/
    ret = usbh_vc_chain_ctrl_get(p_chain, UVC_CID_GAMMA, &value);
    if (ret == USB_OK) {
        p_cfg->gama = value;
    }
    /* 灯管频率*/
    ret = usbh_vc_chain_ctrl_get(p_chain, UVC_CID_POWER_LINE_FREQUENCY, &value);
    if (ret == USB_OK) {
        p_cfg->plf = value;
    }
    /* 获取锐度*/
    ret = usbh_vc_chain_ctrl_get(p_chain, UVC_CID_SHARPNESS, &value);
    if (ret == USB_OK) {
        p_cfg->sharpness = value;
    }
    /* 获取白平衡*/
    ret = usbh_vc_chain_ctrl_get(p_chain, UVC_CID_WHITE_BALANCE_TEMPERATURE, &value);
    if (ret == USB_OK) {
        p_cfg->wb = value;
    }

    return USB_OK;
}

/**
 * \brief 获取 USB 视频类设备视频流支持的格式
 *
 * \param[in]  p_stream USB 视频类设备视频流
 * \param[out] p_format 返回的 USB 视频类格式结构体
 * \param[out] n_format 返回的 USB 视频类格式结构体数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_format_get(struct usbh_vc_stream         *p_stream,
                              struct usbh_vc_stream_format **p_format,
                              uint32_t                      *p_n_format){
    int                           ret;
#if USB_OS_EN
    int                           ret_tmp;
#endif
    uint32_t                      i, j;
    struct uvc_frame             *p_stream_frame  = NULL;
    struct usbh_vc_stream_frame  *p_frame         = NULL;
    struct usbh_vc_stream_format *p_format_tmp    = NULL;

    if ((usb_lib_is_init(&__g_uvc_lib.lib) == USB_FALSE) ||
            (__g_uvc_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    if (p_stream == NULL) {
        return -USB_EINVAL;
    }

    if (p_stream->p_vc->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_stream->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_format_tmp = usb_lib_malloc(&__g_uvc_lib.lib,
                                     p_stream->n_formats * sizeof(struct usbh_vc_stream_format));
    if (p_format_tmp == NULL) {
        ret = -USB_ENOMEM;
        goto __exit;
    }
    memset(p_format_tmp, 0, p_stream->n_formats * sizeof(struct usbh_vc_stream_format));

    *p_n_format = p_stream->n_formats;

    for (i = 0; i < p_stream->n_formats; i++) {
        strcpy(p_format_tmp[i].format_name, p_stream->p_format[i].name);

        p_format_tmp[i].p_frame = usb_lib_malloc(&__g_uvc_lib.lib,
                                                    p_stream->p_format[i].n_frames * sizeof(struct usbh_vc_stream_frame));
        if (p_format_tmp[i].p_frame == NULL) {
            ret = -USB_ENOMEM;
            goto __exit;
        }
        memset(p_format_tmp[i].p_frame, 0, p_stream->p_format[i].n_frames * sizeof(struct usbh_vc_stream_frame));

        p_format_tmp[i].n_frame = p_stream->p_format[i].n_frames;
        p_frame                 = p_format_tmp[i].p_frame;
        p_stream_frame          = p_stream->p_format[i].p_frame;

        for (j = 0; j < p_stream->p_format[i].n_frames; j++) {
            p_frame[j].width  = p_stream_frame[j].width;
            p_frame[j].height = p_stream_frame[j].height;
            p_frame[j].fps    = (float)10000000 / p_stream_frame[j].default_frame_interval;
    	}
    }
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_stream->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret_tmp;
    }
#endif
    if (ret != USB_OK) {
         usbh_vc_stream_format_put(p_format_tmp, *p_n_format);
    } else {
        *p_format = p_format_tmp;
    }

    return ret;
}

/**
 * \brief 回收 USB 视频类设备视频流支持的格式
 *
 * \param[in] p_format USB 视频类格式结构体
 * \param[in] n_format USB 视频类格式结构体数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_format_put(struct usbh_vc_stream_format *p_format,
                              uint32_t                      n_format){
	uint32_t i;

    if ((p_format == NULL) || (n_format == 0)) {
        return -USB_EINVAL;
    }
    for (i = 0; i < n_format; i++) {
        if (p_format[i].n_frame > 0) {
        	usb_lib_mfree(&__g_uvc_lib.lib, p_format[i].p_frame);
        }
    }
    usb_lib_mfree(&__g_uvc_lib.lib, p_format);

    return USB_OK;
}

/**
 * \brief 挂起 USB 视频类设备数据流
 *
 * \param[in] p_stream USB 视频类设备数据流
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_suspend(struct usbh_vc_stream *p_stream){
    int ret;

    if (p_stream == NULL) {
        return -USB_EINVAL;
    }
    if (p_stream->p_vc->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
    if (p_stream->opt_status != UVC_STREAM_START) {
        return -USB_EPERM;
    }

    ret = usbh_vc_video_suspend(p_stream);
    if(ret == USB_OK){
        p_stream->opt_status = UVC_STREAM_SUSPEND;
    }
    return ret;
}

/**
 * \brief 恢复 USB 视频类设备数据流
 *
 * \param[in] p_stream USB 视频类设备数据流
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_resume(struct usbh_vc_stream *p_stream){
    int ret;

    if (p_stream == NULL) {
        return -USB_EINVAL;
    }
    if (p_stream->p_vc->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
    if (p_stream->opt_status != UVC_STREAM_SUSPEND) {
        return -USB_EPERM;
    }

    ret = usbh_vc_video_resume(p_stream, USB_TRUE);
    if (ret == USB_OK) {
        p_stream->opt_status = UVC_STREAM_START;
    }
    return ret;
}

/**
 * \brief USB 视频类数据流获取图片函数
 *
 * \param[in]  p_stream USB 视频类数据流
 * \param[out] p_buf    返回的图片数据缓存
 *
 * \return 成功返回 USB_OK
 */
int usbh_vc_stream_photo_get(struct usbh_vc_stream  *p_stream,
                             uint32_t              **p_buf){
    return -USB_ENOTSUP;
}

/**
 * \brief USB 视频类设备数据流获取图像
 *
 * \param[in]  p_stream  USB 视频类数据流
 * \param[out] p_buf     返回的一帧视频数据缓存
 * \param[in]  p_timeval 时间戳（可为空）
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_video_get(struct usbh_vc_stream *p_stream,
                             void                 **p_buf,
                             struct uvc_timeval    *p_timeval){
    int ret;
#if USB_OS_EN
    int ret_tmp;
#endif

    if (p_stream == NULL) {
        return -USB_EINVAL;
    }
    if (p_stream->p_vc->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
    /* 不是在启动阶段*/
    if (p_stream->opt_status != UVC_STREAM_START) {
        return -USB_EPERM;
    };
#if USB_OS_EN
    /* 锁住数据流*/
    ret = usb_mutex_lock(p_stream->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 获取图像*/
    ret = usbh_vc_video_buf_get((uint32_t **)p_buf, p_stream, p_timeval);
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
 * \brief USB 视频类设备数据流释放图像
 *
 * \param[in] p_stream USB 视频类数据流
 * \param[in] p_buf    要释放的一帧数据缓存
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_video_put(struct usbh_vc_stream *p_stream, void* p_buf){
    int ret;
#if USB_OS_EN
    int ret_tmp;
#endif

    if (p_stream == NULL) {
        return -USB_EINVAL;
    }
    if (p_stream->p_vc->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
#if USB_OS_EN
    /* 锁住数据流*/
    ret = usb_mutex_lock(p_stream->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    ret = usbh_vc_video_buf_put((uint8_t *)p_buf, p_stream);
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
 * \brief 取消视频流传输
 */
int usbh_vc_stream_xfer_cancel(struct usbh_vc_stream *p_stream){
    int i, ret;

    if (p_stream->p_trp) {
        /* 停止传输请求包*/
        for (i = 0; i < p_stream->n_trp; i++) {
            if (p_stream->p_trp[i]) {
                /* 取消传输请求包*/
                ret = usbh_trp_xfer_cancel(p_stream->p_trp[i]);
                if (ret != USB_OK) {
                    return ret;
                }
                usb_mdelay(200);
            }
        }
    }
    return USB_OK;
}


///**
// * \brief 取消视频流传输
// *
// * \param[in] p_stream    USB 视频类数据流
// * \param[in] is_buf_free 是否释放缓存
// *
// * \retval 成功返回 USB_OK
// */
//int usbh_vc_stream_xfer_cancel(struct usbh_vc_stream *p_stream, int is_buf_free){
//    int i, ret = USB_OK;
//
//#if USB_OS_EN
//    ret= usb_mutex_lock(p_stream->p_lock, UVC_LOCK_TIMEOUT);
//    if (ret != USB_OK) {
//        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
//        return ret;
//    }
//#endif
//
//    if (p_stream->p_trp) {
//        /* 停止传输请求包*/
//        for (i = 0; i < p_stream->n_trp; i++) {
//            if (p_stream->p_trp[i]) {
//                /* 取消传输请求包*/
//                ret = usbh_trp_xfer_cancel(p_stream->p_trp[i]);
//                if (ret != USB_OK) {
//                    return ret;
//                }
//            }
//        }
//        /* 等待等时传输结束*/
//        if ((p_stream->n_trp > 0) && (p_stream->n_trp_free < p_stream->n_trp)) {
//#if USB_OS_EN
//            ret= usb_mutex_unlock(p_stream->p_lock);
//            if (ret != USB_OK) {
//                __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
//                return ret;
//            }
//#endif
//            while (p_stream->n_trp_free < p_stream->n_trp) {
//                usb_mdelay_block(10);
//            }
//#if USB_OS_EN
//            ret= usb_mutex_lock(p_stream->p_lock, UVC_LOCK_TIMEOUT);
//            if (ret != USB_OK) {
//                __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
//                return ret;
//            }
//#endif
//        }
//        for (i = 0; i < p_stream->n_trp; i++) {
//            if (p_stream->p_trp[i]->p_iso_frame_desc) {
//                usb_lib_mfree(&__g_uvc_lib.lib, p_stream->p_trp[i]->p_iso_frame_desc);
//            }
//            /* 释放传输请求包内存*/
//            usb_lib_mfree(&__g_uvc_lib.lib, p_stream->p_trp[i]);
//            p_stream->p_trp[i] = NULL;
//        }
//        usb_lib_mfree(&__g_uvc_lib.lib, p_stream->p_trp);
//        p_stream->p_trp = NULL;
//    }
//    if ((p_stream->p_trp_buffer) && (is_buf_free)) {
//        /* 释放传输请求包*/
//        for (i = 0; i < p_stream->n_trp; i++) {
//            /* 释放传输请求包数据缓存*/
//            if (p_stream->p_trp_buffer[i]) {
//            	usb_lib_mfree(&__g_uvc_lib.lib, p_stream->p_trp_buffer[i]);
//                p_stream->p_trp_buffer[i] = NULL;
//            }
//        }
//        p_stream->trp_size = 0;
//        usb_lib_mfree(&__g_uvc_lib.lib, p_stream->p_trp_buffer);
//        p_stream->p_trp_buffer = NULL;
//    }
//
//#if USB_OS_EN
//    ret = usb_mutex_unlock(p_stream->p_lock);
//    if (ret != USB_OK) {
//        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
//    }
//#endif
//
//    return ret;
//}

/**
 * \brief 释放 USB 摄像头流
 */
static int __vc_stream_free(struct usbh_vc_stream *p_stream){
    struct usbh_vc_buffer *p_buf_tmp = NULL;
    struct usbh_vc_queue  *p_queue   = NULL;
    int                    ret;

    /* 取消传输*/
    ret = usbh_vc_stream_xfer_cancel(p_stream);
    if (ret != USB_OK) {
        return ret;
    }

    /* 等待传输停止*/
    while (p_stream->n_trp_act != 0) {
        usb_mdelay(10);
    }

    usbh_vc_video_free(p_stream, USB_TRUE);

    p_stream->p_intf = NULL;
    p_stream->p_vc   = NULL;

    /* 获取队列*/
    p_queue = (struct usbh_vc_queue *)p_stream->p_queue;
    if (p_queue == NULL) {
        goto __queue_end;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_queue->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 释放图片数据缓存*/
    while (!usb_list_head_is_empty(&p_queue->irqqueue)) {
        p_buf_tmp = usb_container_of(p_queue->irqqueue.p_next, struct usbh_vc_buffer, node);
        usb_lib_mfree(&__g_uvc_lib.lib, p_buf_tmp->p_mem);
        usb_list_node_del(&p_buf_tmp->node);
        usb_lib_mfree(&__g_uvc_lib.lib, p_buf_tmp);
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_queue->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_queue->is_used = USB_FALSE;

__queue_end:
    usb_lib_mfree(&__g_uvc_lib.lib, p_stream->p_format);
    usb_lib_mfree(&__g_uvc_lib.lib, p_stream->header.p_controls);
#if USB_OS_EN
    ret = usb_lib_mutex_destroy(&__g_uvc_lib.lib, p_stream->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 释放视频流时钟采样*/
    if (p_stream->clock.p_samples) {
        usb_lib_mfree(&__g_uvc_lib.lib, p_stream->clock.p_samples);
    }
#if USB_OS_EN
    /* 删除互斥锁*/
    if (p_stream->clock.p_lock) {
        ret = usb_lib_mutex_destroy(&__g_uvc_lib.lib, p_stream->clock.p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
            return ret;
        }
    }
#endif

    usb_lib_mfree(&__g_uvc_lib.lib, p_stream);

    return USB_OK;
}

/**
 * \brief USB 视频类清除视频流
 *
 * \param[in] p_vc USB 视频类设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_clr(struct usbh_vc *p_vc){
    int                    ret;
#if USB_OS_EN
    int                    ret_tmp;
#endif
    struct usb_list_node  *p_node     = NULL;
    struct usb_list_node  *p_node_tmp = NULL;
    struct usbh_vc_stream *p_stream   = NULL;

#if USB_OS_EN
    ret = usb_mutex_lock(p_vc->p_lock, UVC_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    /* 遍历所有视频流*/
    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_vc->streams){
        p_stream = usb_container_of(p_node, struct usbh_vc_stream, node);

        /* 删除节点*/
        usb_list_node_del(&p_stream->node);

        __g_uvc_lib.n_stream--;

        /* 释放视频流*/
        ret = __vc_stream_free(p_stream);
        if (ret != USB_OK) {
            goto __exit;
        }
    }
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_vc->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}


