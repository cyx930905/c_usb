#ifndef __USBH_VC_STREAM_H
#define __USBH_VC_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "core/include/host/core/usbh.h"
#include "adapter/usb_adapter.h"
#include "common/usb_common.h"
#include "common/list/usb_list.h"

/* \brief RGB格式*/
#define UVC_PIX_FMT_RGB332   usb_fourcc('R', 'G', 'B', '1')    /*  8  RGB-3-3-2     */
#define UVC_PIX_FMT_RGB444   usb_fourcc('R', '4', '4', '4')    /* 16  xxxxrrrr ggggbbbb */
#define UVC_PIX_FMT_ARGB444  usb_fourcc('A', 'R', '1', '2')    /* 16  aaaarrrr ggggbbbb */
#define UVC_PIX_FMT_XRGB444  usb_fourcc('X', 'R', '1', '2')    /* 16  xxxxrrrr ggggbbbb */
#define UVC_PIX_FMT_RGB555   usb_fourcc('R', 'G', 'B', 'O')    /* 16  RGB-5-5-5     */
#define UVC_PIX_FMT_ARGB555  usb_fourcc('A', 'R', '1', '5')    /* 16  ARGB-1-5-5-5  */
#define UVC_PIX_FMT_XRGB555  usb_fourcc('X', 'R', '1', '5')    /* 16  XRGB-1-5-5-5  */
#define UVC_PIX_FMT_RGB565   usb_fourcc('R', 'G', 'B', 'P')    /* 16  RGB-5-6-5     */
#define UVC_PIX_FMT_RGB555X  usb_fourcc('R', 'G', 'B', 'Q')    /* 16  RGB-5-5-5 BE  */
#define UVC_PIX_FMT_ARGB555X usb_fourcc('A', 'R', '1', '5')    /* 16  ARGB-5-5-5 BE */
#define UVC_PIX_FMT_XRGB555X usb_fourcc('X', 'R', '1', '5')    /* 16  XRGB-5-5-5 BE */
#define UVC_PIX_FMT_RGB565X  usb_fourcc('R', 'G', 'B', 'R')    /* 16  RGB-5-6-5 BE  */
#define UVC_PIX_FMT_BGR666   usb_fourcc('B', 'G', 'R', 'H')    /* 18  BGR-6-6-6     */
#define UVC_PIX_FMT_BGR24    usb_fourcc('B', 'G', 'R', '3')    /* 24  BGR-8-8-8     */
#define UVC_PIX_FMT_RGB24    usb_fourcc('R', 'G', 'B', '3')    /* 24  RGB-8-8-8     */
#define UVC_PIX_FMT_BGR32    usb_fourcc('B', 'G', 'R', '4')    /* 32  BGR-8-8-8-8   */
#define UVC_PIX_FMT_ABGR32   usb_fourcc('A', 'R', '2', '4')    /* 32  BGRA-8-8-8-8  */
#define UVC_PIX_FMT_XBGR32   usb_fourcc('X', 'R', '2', '4')    /* 32  BGRX-8-8-8-8  */
#define UVC_PIX_FMT_RGB32    usb_fourcc('R', 'G', 'B', '4')    /* 32  RGB-8-8-8-8   */
#define UVC_PIX_FMT_ARGB32   usb_fourcc('B', 'A', '2', '4')    /* 32  ARGB-8-8-8-8  */
#define UVC_PIX_FMT_XRGB32   usb_fourcc('B', 'X', '2', '4')    /* 32  XRGB-8-8-8-8  */
/* \brief 灰色格式*/
#define UVC_PIX_FMT_GREY     usb_fourcc('G', 'R', 'E', 'Y')    /*  8  Greyscale     */
#define UVC_PIX_FMT_Y4       usb_fourcc('Y', '0', '4', ' ')    /*  4  Greyscale     */
#define UVC_PIX_FMT_Y6       usb_fourcc('Y', '0', '6', ' ')    /*  6  Greyscale     */
#define UVC_PIX_FMT_Y10      usb_fourcc('Y', '1', '0', ' ')    /* 10  Greyscale     */
#define UVC_PIX_FMT_Y12      usb_fourcc('Y', '1', '2', ' ')    /* 12  Greyscale     */
#define UVC_PIX_FMT_Y16      usb_fourcc('Y', '1', '6', ' ')    /* 16  Greyscale     */
/* \brief 亮度和色度格式 */
#define UVC_PIX_FMT_YVU410   usb_fourcc('Y', 'V', 'U', '9')    /*  9  YVU 4:1:0     */
#define UVC_PIX_FMT_YVU420   usb_fourcc('Y', 'V', '1', '2')    /* 12  YVU 4:2:0     */
#define UVC_PIX_FMT_YUYV     usb_fourcc('Y', 'U', 'Y', 'V')    /* 16  YUV 4:2:2     */
#define UVC_PIX_FMT_YYUV     usb_fourcc('Y', 'Y', 'U', 'V')    /* 16  YUV 4:2:2     */
#define UVC_PIX_FMT_YVYU     usb_fourcc('Y', 'V', 'Y', 'U')    /* 16 YVU 4:2:2 */
#define UVC_PIX_FMT_UYVY     usb_fourcc('U', 'Y', 'V', 'Y')    /* 16  YUV 4:2:2     */
#define UVC_PIX_FMT_VYUY     usb_fourcc('V', 'Y', 'U', 'Y')    /* 16  YUV 4:2:2     */
#define UVC_PIX_FMT_YUV422P  usb_fourcc('4', '2', '2', 'P')    /* 16  YVU422 planar */
#define UVC_PIX_FMT_YUV411P  usb_fourcc('4', '1', '1', 'P')    /* 16  YVU411 planar */
#define UVC_PIX_FMT_Y41P     usb_fourcc('Y', '4', '1', 'P')    /* 12  YUV 4:1:1     */
#define UVC_PIX_FMT_YUV444   usb_fourcc('Y', '4', '4', '4')    /* 16  xxxxyyyy uuuuvvvv */
#define UVC_PIX_FMT_YUV555   usb_fourcc('Y', 'U', 'V', 'O')    /* 16  YUV-5-5-5     */
#define UVC_PIX_FMT_YUV565   usb_fourcc('Y', 'U', 'V', 'P')    /* 16  YUV-5-6-5     */
#define UVC_PIX_FMT_YUV32    usb_fourcc('Y', 'U', 'V', '4')    /* 32  YUV-8-8-8-8   */
#define UVC_PIX_FMT_YUV410   usb_fourcc('Y', 'U', 'V', '9')    /*  9  YUV 4:1:0     */
#define UVC_PIX_FMT_YUV420   usb_fourcc('Y', 'U', '1', '2')    /* 12  YUV 4:2:0     */
#define UVC_PIX_FMT_HI240    usb_fourcc('H', 'I', '2', '4')    /*  8  8-bit color   */
#define UVC_PIX_FMT_HM12     usb_fourcc('H', 'M', '1', '2')    /*  8  YUV 4:2:0 16x16 macroblocks */
#define UVC_PIX_FMT_M420     usb_fourcc('M', '4', '2', '0')    /* 12  YUV 4:2:0 2 lines y, 1 line uv interleaved */
/* \brief 两个平面 -- 一个Y，一个Cr + Cb交错  */
#define UVC_PIX_FMT_NV12     usb_fourcc('N', 'V', '1', '2')    /* 12  Y/CbCr 4:2:0  */
#define UVC_PIX_FMT_NV21     usb_fourcc('N', 'V', '2', '1')    /* 12  Y/CrCb 4:2:0  */
#define UVC_PIX_FMT_NV16     usb_fourcc('N', 'V', '1', '6')    /* 16  Y/CbCr 4:2:2  */
#define UVC_PIX_FMT_NV61     usb_fourcc('N', 'V', '6', '1')    /* 16  Y/CrCb 4:2:2  */
#define UVC_PIX_FMT_NV24     usb_fourcc('N', 'V', '2', '4')    /* 24  Y/CbCr 4:4:4  */
#define UVC_PIX_FMT_NV42     usb_fourcc('N', 'V', '4', '2')    /* 24  Y/CrCb 4:4:4  */
/* \brief Bayer formats - see http://www.siliconimaging.com/RGB%20Bayer.htm */
#define UVC_PIX_FMT_SBGGR8   usb_fourcc('B', 'A', '8', '1')    /*  8  BGBG.. GRGR.. */
#define UVC_PIX_FMT_SGBRG8   usb_fourcc('G', 'B', 'R', 'G')    /*  8  GBGB.. RGRG.. */
#define UVC_PIX_FMT_SGRBG8   usb_fourcc('G', 'R', 'B', 'G')    /*  8  GRGR.. BGBG.. */
#define UVC_PIX_FMT_SRGGB8   usb_fourcc('R', 'G', 'G', 'B')    /*  8  RGRG.. GBGB.. */
#define UVC_PIX_FMT_SBGGR10  usb_fourcc('B', 'G', '1', '0')    /* 10  BGBG.. GRGR.. */
#define UVC_PIX_FMT_SGBRG10  usb_fourcc('G', 'B', '1', '0')    /* 10  GBGB.. RGRG.. */
#define UVC_PIX_FMT_SGRBG10  usb_fourcc('B', 'A', '1', '0')    /* 10  GRGR.. BGBG.. */
#define UVC_PIX_FMT_SRGGB10  usb_fourcc('R', 'G', '1', '0')    /* 10  RGRG.. GBGB.. */
/* \brief 10bit raw bayer packed, 5 bytes for every 4 pixels */
#define UVC_PIX_FMT_SBGGR10P usb_fourcc('p', 'B', 'A', 'A')
#define UVC_PIX_FMT_SGBRG10P usb_fourcc('p', 'G', 'A', 'A')
#define UVC_PIX_FMT_SGRBG10P usb_fourcc('p', 'g', 'A', 'A')
#define UVC_PIX_FMT_SRGGB10P usb_fourcc('p', 'R', 'A', 'A')
/* \brief 10bit raw bayer a-law compressed to 8 bits */
#define UVC_PIX_FMT_SBGGR10ALAW8 usb_fourcc('a', 'B', 'A', '8')
#define UVC_PIX_FMT_SGBRG10ALAW8 usb_fourcc('a', 'G', 'A', '8')
#define UVC_PIX_FMT_SGRBG10ALAW8 usb_fourcc('a', 'g', 'A', '8')
#define UVC_PIX_FMT_SRGGB10ALAW8 usb_fourcc('a', 'R', 'A', '8')
/* \brief 10bit raw bayer DPCM compressed to 8 bits */
#define UVC_PIX_FMT_SBGGR10DPCM8 usb_fourcc('b', 'B', 'A', '8')
#define UVC_PIX_FMT_SGBRG10DPCM8 usb_fourcc('b', 'G', 'A', '8')
#define UVC_PIX_FMT_SGRBG10DPCM8 usb_fourcc('B', 'D', '1', '0')
#define UVC_PIX_FMT_SRGGB10DPCM8 usb_fourcc('b', 'R', 'A', '8')
#define UVC_PIX_FMT_SBGGR12      usb_fourcc('B', 'G', '1', '2') /* 12  BGBG.. GRGR.. */
#define UVC_PIX_FMT_SGBRG12      usb_fourcc('G', 'B', '1', '2') /* 12  GBGB.. RGRG.. */
#define UVC_PIX_FMT_SGRBG12      usb_fourcc('B', 'A', '1', '2') /* 12  GRGR.. BGBG.. */
#define UVC_PIX_FMT_SRGGB12      usb_fourcc('R', 'G', '1', '2') /* 12  RGRG.. GBGB.. */
#define UVC_PIX_FMT_SBGGR16      usb_fourcc('B', 'Y', 'R', '2') /* 16  BGBG.. GRGR.. */
/* \brief 压缩格式 */
#define UVC_PIX_FMT_MJPEG        usb_fourcc('M', 'J', 'P', 'G') /* Motion-JPEG   */
#define UVC_PIX_FMT_JPEG         usb_fourcc('J', 'P', 'E', 'G') /* JFIF JPEG     */
#define UVC_PIX_FMT_DV           usb_fourcc('d', 'v', 's', 'd') /* 1394          */
#define UVC_PIX_FMT_MPEG         usb_fourcc('M', 'P', 'E', 'G') /* MPEG-1/2/4 Multiplexed */
#define UVC_PIX_FMT_H264         usb_fourcc('H', '2', '6', '4') /* H264 with start codes */
#define UVC_PIX_FMT_H264_NO_SC   usb_fourcc('A', 'V', 'C', '1') /* H264 without start codes */
#define UVC_PIX_FMT_H264_MVC     usb_fourcc('M', '2', '6', '4') /* H264 MVC */
#define UVC_PIX_FMT_H263         usb_fourcc('H', '2', '6', '3') /* H263          */
#define UVC_PIX_FMT_MPEG1        usb_fourcc('M', 'P', 'G', '1') /* MPEG-1 ES     */
#define UVC_PIX_FMT_MPEG2        usb_fourcc('M', 'P', 'G', '2') /* MPEG-2 ES     */
#define UVC_PIX_FMT_MPEG4        usb_fourcc('M', 'P', 'G', '4') /* MPEG-4 part 2 ES */
#define UVC_PIX_FMT_XVID         usb_fourcc('X', 'V', 'I', 'D') /* Xvid           */
#define UVC_PIX_FMT_VC1_ANNEX_G  usb_fourcc('V', 'C', '1', 'G') /* SMPTE 421M Annex G compliant stream */
#define UVC_PIX_FMT_VC1_ANNEX_L  usb_fourcc('V', 'C', '1', 'L') /* SMPTE 421M Annex L compliant stream */
#define UVC_PIX_FMT_VP8          usb_fourcc('V', 'P', '8', '0') /* VP8 */

/* \brief "性能"字段的值*/
#define UVC_CAP_VIDEO_CAPTURE        0x00000001  /* 视频捕获设备*/
#define UVC_CAP_VIDEO_OUTPUT         0x00000002  /* 视频输出设备*/
#define UVC_CAP_VIDEO_OVERLAY        0x00000004  /* Can do video overlay */
#define UVC_CAP_VBI_CAPTURE          0x00000010  /* Is a raw VBI capture device */
#define UVC_CAP_VBI_OUTPUT           0x00000020  /* Is a raw VBI output device */
#define UVC_CAP_SLICED_VBI_CAPTURE   0x00000040  /* Is a sliced VBI capture device */
#define UVC_CAP_SLICED_VBI_OUTPUT    0x00000080  /* Is a sliced VBI output device */
#define UVC_CAP_RDS_CAPTURE          0x00000100  /* RDS data capture */
#define UVC_CAP_VIDEO_OUTPUT_OVERLAY 0x00000200  /* Can do video output overlay */
#define UVC_CAP_HW_FREQ_SEEK         0x00000400  /* Can do hardware frequency seek  */
#define UVC_CAP_RDS_OUTPUT           0x00000800  /* Is an RDS encoder */

/* \brief 格式标志 */
#define UVC_FMT_FLAG_COMPRESSED     0x00000001    /* 压缩标志*/
#define UVC_FMT_FLAG_STREAM         0x00000002    /* 流标志*/

/* \brief 可选配置标志*/
#define UVC_CFG_BRIGHTNESS         (1 << 0)
#define UVC_CFG_CONTRAST           (1 << 1)
#define UVC_CFG_HUE                (1 << 2)
#define UVC_CFG_SATURARION         (1 << 3)
#define UVC_CFG_SHARPNESS          (1 << 4)
#define UVC_CFG_GAMA               (1 << 5)
#define UVC_CFG_WB                 (1 << 6)
#define UVC_CFG_PLF                (1 << 7)

/* see also http://vektor.theorem.ca/graphics/ycbcr/ */
enum uvc_colorspace {
    /* SMPTE 170M: used for broadcast NTSC/PAL SDTV */
    UVC_COLORSPACE_SMPTE170M     = 1,

    /* Obsolete pre-1998 SMPTE 240M HDTV standard, superseded by Rec 709 */
    UVC_COLORSPACE_SMPTE240M     = 2,

    /* Rec.709: used for HDTV */
    UVC_COLORSPACE_REC709        = 3,

    /*
     * Deprecated, do not use. No driver will ever return this. This was
     * based on a misunderstanding of the bt878 datasheet.
     */
    UVC_COLORSPACE_BT878         = 4,

    /*
     * NTSC 1953 colorspace. This only makes sense when dealing with
     * really, really old NTSC recordings. Superseded by SMPTE 170M.
     */
    UVC_COLORSPACE_470_SYSTEM_M  = 5,

    /*
     * EBU Tech 3213 PAL/SECAM colorspace. This only makes sense when
     * dealing with really old PAL/SECAM recordings. Superseded by
     * SMPTE 170M.
     */
    UVC_COLORSPACE_470_SYSTEM_BG = 6,

    /*
     * Effectively shorthand for V4L2_COLORSPACE_SRGB, V4L2_YCBCR_ENC_601
     * and V4L2_QUANTIZATION_FULL_RANGE. To be used for (Motion-)JPEG.
     */
    UVC_COLORSPACE_JPEG          = 7,

    /* For RGB colorspaces such as produces by most webcams. */
    UVC_COLORSPACE_SRGB          = 8,

    /* AdobeRGB colorspace */
    UVC_COLORSPACE_ADOBERGB      = 9,

    /* BT.2020 colorspace, used for UHDTV. */
    UVC_COLORSPACE_BT2020        = 10,
};

enum uvc_field {
    UVC_FIELD_ANY           = 0, /* driver can choose from none,
                                     top, bottom, interlaced
                                     depending on whatever it thinks
                                     is approximate ... */
    UVC_FIELD_NONE          = 1, /* this device has no fields ... */
    UVC_FIELD_TOP           = 2, /* top field only */
    UVC_FIELD_BOTTOM        = 3, /* bottom field only */
    UVC_FIELD_INTERLACED    = 4, /* both fields interlaced */
    UVC_FIELD_SEQ_TB        = 5, /* both fields sequential into one
                                    buffer, top-bottom order */
    UVC_FIELD_SEQ_BT        = 6, /* same as above + bottom-top order */
    UVC_FIELD_ALTERNATE     = 7, /* both fields alternating into
                                    separate buffers */
    UVC_FIELD_INTERLACED_TB = 8, /* both fields interlaced, top field
                                    first and the top field is
                                    transmitted first */
    UVC_FIELD_INTERLACED_BT = 9, /* both fields interlaced, top field
                                    first and the bottom field is
                                    transmitted first */
};

/* \brief USB 视频类缓存类型*/
enum uvc_buf_type {
    UVC_BUF_TYPE_VIDEO_CAPTURE        = 1,
    UVC_BUF_TYPE_VIDEO_OUTPUT         = 2,
    UVC_BUF_TYPE_VIDEO_OVERLAY        = 3,
    UVC_BUF_TYPE_VBI_CAPTURE          = 4,
    UVC_BUF_TYPE_VBI_OUTPUT           = 5,
    UVC_BUF_TYPE_SLICED_VBI_CAPTURE   = 6,
    UVC_BUF_TYPE_SLICED_VBI_OUTPUT    = 7,
#if 1
    /* 实验的 */
    UVC_BUF_TYPE_VIDEO_OUTPUT_OVERLAY = 8,
#endif
    UVC_BUF_TYPE_VIDEO_CAPTURE_MPLANE = 9,
    UVC_BUF_TYPE_VIDEO_OUTPUT_MPLANE  = 10,
    UVC_BUF_TYPE_SDR_CAPTURE          = 11,
    /* 已弃用，不要用 */
    UVC_BUF_TYPE_PRIVATE              = 0x80,
};

/* \brief USB 视频类视频流状态 */
typedef enum{
    UVC_STREAM_CLOSE = 0,
    UVC_STREAM_OPEN,
    UVC_STREAM_CONFIG,
    UVC_STREAM_START,
    UVC_STREAM_STOP,
    UVC_STREAM_SUSPEND,
} uvc_stream_status;

/* \brief 摄像头支持的输出像素格式   */
typedef enum __uvc_pix_format {
    UVC_RGB565,
    UVC_RGB555,
    UVC_RGB444,
    UVC_RGB888,
    UVC_YUV420,
    UVC_YUV422,
    UVC_YUV444,
    UVC_JPEG,
    UVC_MJPEG,
    UVC_BAYER_RAW,
} uvc_pix_format_t;

/* \brief 像素点格式结构体*/
struct uvc_pix_format {
    uint32_t width;          /* 宽度*/
    uint32_t height;         /* 高度*/
    uint32_t pixelformat;    /* 像素点格式*/
    uint32_t field;          /* enum v4l2_field */
    uint32_t bytesperline;   /* for padding, zero if unused */
    uint32_t sizeimage;
    uint32_t colorspace;     /* 色域*/
    uint32_t priv;           /* private data, depends on pixelformat */
    uint32_t flags;          /* format flags (V4L2_PIX_FMT_FLAG_*) */
    uint32_t ycbcr_enc;      /* enum v4l2_ycbcr_encoding */
    uint32_t quantization;   /* enum v4l2_quantization */
};

struct uvc_frame_info {
    uint16_t  wWidth;
    uint16_t  wHeight;
};

/* \brief USB 视频类帧结构体 */
struct uvc_frame {
    uint8_t   frame_index;
    uint8_t   capabilities;
    uint16_t  width;
    uint16_t  height;
    uint32_t  min_bit_rate;
    uint32_t  max_bit_rate;
    uint32_t  max_video_frame_buffer_size;
    uint8_t   frame_interval_type;
    uint32_t  default_frame_interval;
    uint32_t *p_frame_interval;
};

/* \brief USB 视频类帧状态结构体*/
struct uvc_frame_stats {
    uint32_t   size;            /* 捕获到的字节数*/
    uint32_t   first_data;      /* 第一个非空包的索引*/

    uint32_t   nb_packets;      /* 包的数量*/
    uint32_t   nb_empty;        /* 空包的数量*/
    uint32_t   nb_invalid;      /* 不可用头部包的数量*/
    uint32_t   nb_errors;       /* 设置了错误位的包的数量*/

    uint32_t   nb_pts;          /* Number of packets with a PTS timestamp */
    uint32_t   nb_pts_diffs;    /* Number of PTS differences inside a frame */
    uint32_t   last_pts_diff;   /* Index of the last PTS difference */
    usb_bool_t has_initial_pts; /* Whether the first non-empty packet has a PTS */
    usb_bool_t has_early_pts;   /* Whether a PTS is present before the first non-empty packet */
    uint32_t   pts;             /* PTS of the last packet */

    uint32_t   nb_scr;          /* Number of packets with a SCR timestamp */
    uint32_t   nb_scr_diffs;    /* Number of SCR.STC differences inside a frame */
    uint16_t   scr_sof;         /* SCR.SOF of the last packet */
    uint32_t   scr_stc;         /* SCR.STC of the last packet */
};

/* \brief USB 视频类格式描述符*/
struct uvc_format_desc {
    char    *p_name;       /* 格式名字*/
    uint8_t  guid[16];     /* 全局唯一标识符*/
    uint32_t fcc;
};

/* \brief USB 视频类格式配置结构体*/
struct uvc_format_cfg {
    uint32_t              type;  /* 格式类型*/
    struct uvc_pix_format pix;   /* 像素点格式*/
};

/* \brief USB 视频类格式结构体*/
struct uvc_format {
    uint8_t           type;         /* 格式类型*/
    uint8_t           index;        /* 格式索引*/
    uint8_t           bpp;          /* 像素深度*/
    uint8_t           colorspace;   /* 色域*/
    uint32_t          fcc;
    uint32_t          flags;        /* 格式标志*/

    char              name[32];     /* 格式名字*/

    unsigned int      n_frames;     /* 帧结构体数量*/
    struct uvc_frame *p_frame;      /* USB 视频类帧结构体*/
};

/* \brief USB 视频类视频流头结构体 */
struct uvc_stream_hdr {
    uint8_t  num_formats;
    uint8_t  endpoint_address;
    uint8_t  terminal_link;
    uint8_t  control_size;
    uint8_t *p_controls;
    /* 以下字段只用于输入头描述符*/
    uint8_t  info;
    uint8_t  still_capture_method;
    uint8_t  trigger_support;
    uint8_t  trigger_usage;
};

/* \brief 视频探测和提交控件 */
struct uvc_stream_ctrl {
    uint16_t hint;                      /* 向功能只是哪些字段应保持固定的位字段控制*/
    uint8_t  format_index;              /* 来自此视频接口的格式描述符的视频格式索引*/
    uint8_t  frame_index;               /* 来自帧描述符的视频帧索引*/
    uint32_t frame_interval;            /* 此字段为选定的视频流和帧索引设置所需的视频帧间隔(100ns为单位的帧间隔)*/
    uint16_t key_frame_rate;            /* 每视频帧单位的关键帧速率*/
    uint16_t frame_rate;                /* 以帧/关键帧单位表示的帧速率*/
    uint16_t comp_quality;              /* 抽象单元压缩质量控制1(最低)到10000(最高)*/
    uint16_t comp_window_size;          /* 平均比特率控制的窗口大小*/
    uint16_t delay;                     /* 内部视频流接口延迟(毫秒)*/
    uint32_t max_video_frame_size;      /* 视频帧或编解码其特定的最大段大小(字节)*/
    uint32_t max_payload_transfer_size; /* 指定设备在单个有效负载传输中可以发送或接受的最大字节数*/
    uint32_t clock_frequency;           /* 指定合适的设备时钟频率*/
    uint8_t  framing_info;              /* 此控件向功能指示有效负载传输是否将在视频有效负载头中包含外带帧信息*/
    uint8_t  prefered_version;          /* 主机或设备为指定的bFormatIndex值支持的首选负载格式版本*/
    uint8_t  min_version;               /* 主机或设备为指定的bFormatIndex值支持的最小负载格式版本*/
    uint8_t  max_version;               /* 主机或设备为指定的bFormatIndex值支持的最大负载格式版本*/
} __attribute__((__packed__));

/* \brief USB 视频类流状态结构体*/
struct uvc_stream_stats {
    struct usb_timespec start_ts;        /* 流起始时间戳*/
    struct usb_timespec stop_ts;         /* 流停止时间戳*/

    uint32_t            nb_frames;       /* 帧的数量*/

    uint32_t            nb_packets;      /* 包的数量*/
    uint32_t            nb_empty;        /* 空包的数量*/
    uint32_t            nb_invalid;      /* 不可用头部包的数量*/
    uint32_t            nb_errors;       /* 设置了错误位的包的数量*/

    uint32_t            nb_pts_constant; /* 包含显示时间戳的帧的数量*/
    uint32_t            nb_pts_early;    /* Number of frames with early PTS */
    uint32_t            nb_pts_initial;  /* Number of frames with initial PTS */

    uint32_t            nb_scr_count_ok; /* Number of frames with at least one SCR per non empty packet */
    uint32_t            nb_scr_diffs_ok; /* Number of frames with varying SCR.STC */
    uint32_t            scr_sof_count;   /* STC.SOF counter accumulated since stream start */
    uint32_t            scr_sof;         /* STC.SOF of the last packet */
    uint32_t            min_sof;         /* 最小 STC.SOF 值*/
    uint32_t            max_sof;         /* 最大 STC.SOF 值 */
};

/* \brief USB 视频类视频流结构体*/
struct usbh_vc_stream {
    struct usb_list_node          node;               /* 视频流节点*/
    struct usbh_vc               *p_vc;               /* USB 视频类设备结构体*/
    struct usbh_interface        *p_intf;             /* USB 接口结构体*/
    int                           intf_num;           /* 接口编号*/
    struct uvc_stream_hdr         header;             /* USB 视频类视频流头结构体  */
#if USB_OS_EN
    usb_mutex_handle_t            p_lock;             /* 设备互斥锁*/
#endif
    uint32_t                      n_formats;          /* 格式的数量*/
    struct uvc_format            *p_format;           /* USB 视频类格式结构体*/
    enum uvc_buf_type             type;               /* 缓存类型*/
    uint16_t                      mps;                /* 端点最大包大小*/
    struct usbh_vc_video_chain   *p_chain;            /* 视频链链表*/
    struct uvc_stream_ctrl        ctrl;               /* USB 视频类流控制结构体*/
    struct uvc_format            *p_format_def;       /* 默认格式*/
    struct uvc_format            *p_format_cur;       /* 当前格式*/
    struct uvc_frame             *p_frame_cur;        /* 当前帧*/
    uint32_t                      sequence;           /* 序列号*/
    uint8_t                       last_fid;           /* 最后帧 ID */
    uint8_t                       is_frozen;          /* 是否挂起*/
    struct usbh_trp             **p_trp;              /* USB 请求传输包 */
    uint32_t                      trp_size;           /* USB 请求传输包大小*/
    char                        **p_trp_buffer;       /* USB 请求传输包缓存*/
    uint8_t                       n_trp;              /* USB 请求传输包数量*/
    uint8_t                       n_trp_act;          /* USB 请求传输包激活数据*/
    usb_bool_t                    is_init;            /* 是否已经初始化*/
    uint8_t                       opt_status;         /* 操作状态*/
    uint8_t                       open_cnt;           /* 打开次数*/
    void                         *p_queue;            /* 缓存队列*/
    void                         *p_buf_tmp;          /* 临时缓存*/

    /* 编码回调函数*/
    void (*p_fn_decode) (struct usbh_trp       *p_trp,
                         struct usbh_vc_stream *p_stream,
                         struct usbh_vc_buffer *p_buf);

    struct {
        struct uvc_frame_stats   frame_stats;
        struct uvc_stream_stats  stream_stats;
    } stats;

    /* 批量完成处理程序使用的上下文数据*/
    struct {
        uint8_t                  header[256];
        uint32_t                 header_size;
        int                      skip_payload;
        uint32_t                 payload_size;
        uint32_t                 max_payload_size;
    } bulk;

    /* 时间戳支持 */
    struct uvc_clock {                                /* UVC时钟结构体*/
        struct uvc_clock_sample {                     /* UVC时钟采样结构体*/
            uint32_t             dev_stc;
            uint16_t             dev_sof;
            struct usb_timespec  host_ts;             /* 主机时间戳*/
            uint16_t             host_sof;
        } *p_samples;

        uint32_t                 head;
        uint32_t                 count;

        int16_t                  last_sof;
        int16_t                  sof_offset;
        uint32_t                 size;                /* */
        usb_mutex_handle_t       p_lock;              /* UVC时钟互斥锁*/
    } clock;
};

/* \brief USB 视频类视频流配置结构体*/
struct usbh_vc_stream_cfg {
    /* 以下为必选配置*/
    uint16_t         width;              /* 图像宽度*/
    uint16_t         height;             /* 图像高度*/
    uint32_t         type;               /* USB 摄像头类型*/
    uvc_pix_format_t format;             /* 输出格式*/
    /* 以下为可选配置*/
    int              brightness;         /* 亮度*/
    int              contrast;           /* 对比度*/
    int              hue;                /* 色调*/
    int              saturation;         /* 饱和度*/
    int              sharpness;          /* 锐度*/
    int              gama;               /* 伽马*/
    int              wb;                 /* 白平衡*/
    int              plf;                /* Power Line Frequency，灯管频率*/
};

/* \brief USB 视频类视频流格式结构体*/
struct usbh_vc_stream_frame {
    uint16_t width;       /* 宽*/
    uint16_t height;      /* 高*/
    float    fps;         /* 帧率*/
};

/* \brief USB 视频类视频流格式结构体*/
struct usbh_vc_stream_format {
    char                         format_name[USB_NAME_LEN];
    uint32_t                     n_frame;
    struct usbh_vc_stream_frame *p_frame;
};

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
                        struct usbh_vc_stream **p_stream);
/**
 * \brief USB 视频类设备数据流关闭函数
 *
 * \param[in] p_stream USB 视频类设备数据流
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_close(struct usbh_vc_stream *p_stream);
/**
 * \brief 启动 USB 视频类设备数据流
 *
 * \param[in] p_stream USB 视频类设备数据流
 * \param[in] n_trp    传输请求包数量(2~5)
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_start(struct usbh_vc_stream *p_stream, uint8_t n_trp);
/**
 * \brief 停止 USB 视频类设备数据流
 *
 * \param[in] p_stream USB 视频类设备数据流
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_stop(struct usbh_vc_stream *p_stream);
/**
 * \brief 挂起 USB 视频类设备数据流
 *
 * \param[in] p_stream USB 视频类设备数据流
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_suspend(struct usbh_vc_stream *p_stream);
/**
 * \brief 恢复 USB 视频类设备数据流
 *
 * \param[in] p_stream USB 视频类设备数据流
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_resume(struct usbh_vc_stream *p_stream);
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
                           uint32_t                   flag);
/**
 * \brief 获取 USB 视频类数据流配置
 *
 * \param[in] p_stream USB 视频类数据流
 * \param[in] p_cfg    数据流配置
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_cfg_get(struct usbh_vc_stream     *p_stream,
                           struct usbh_vc_stream_cfg *p_cfg);
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
                              uint32_t                      *p_n_format);
/**
 * \brief 回收 USB 视频类设备视频流支持的格式
 *
 * \param[in] p_format USB 视频类格式结构体
 * \param[in] n_format USB 视频类格式结构体数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_format_put(struct usbh_vc_stream_format *p_format,
                              uint32_t                      n_format);
/**
 * \brief USB 视频类数据流获取图片函数
 *
 * \param[in]  p_stream USB 视频类数据流
 * \param[out] p_buf    返回的图片数据缓存
 *
 * \return 成功返回 USB_OK
 */
int usbh_vc_stream_photo_get(struct usbh_vc_stream  *p_stream,
                             uint32_t              **p_buf);
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
                             struct uvc_timeval    *p_timeval);
/**
 * \brief USB 视频类设备数据流释放图像
 *
 * \param[in] p_stream USB 视频类数据流
 * \param[in] p_buf    要释放的一帧数据缓存
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_stream_video_put(struct usbh_vc_stream *p_stream, void* p_buf);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif


