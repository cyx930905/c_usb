#ifndef __USB_VC_SPECS_H
#define __USB_VC_SPECS_H


/* \brief USB 视频类接口子类代码*/
#define UVC_VSC_UNDEFINED                             0x00         /* 未定义子类*/
#define UVC_VSC_VIDEOCONTROL                          0x01         /* 视频控制子类*/
#define UVC_VSC_VIDEOSTREAMING                        0x02         /* 视频流子类*/
#define UVC_VSC_VIDEO_INTERFACE_COLLECTION            0x03         /* 视频接口集子类*/

/* \brief 2.4.2.2. 状态包类型*/
#define UVC_STATUS_TYPE_CONTROL                       1
#define UVC_STATUS_TYPE_STREAMING                     2

/* \brief 2.4.3.3. 负载头信息*/
#define UVC_STREAM_EOH                               (1 << 7)
#define UVC_STREAM_ERR                               (1 << 6)
#define UVC_STREAM_STI                               (1 << 5)
#define UVC_STREAM_RES                               (1 << 4)
#define UVC_STREAM_SCR                               (1 << 3)
#define UVC_STREAM_PTS                               (1 << 2)
#define UVC_STREAM_EOF                               (1 << 1)
#define UVC_STREAM_FID                               (1 << 0)      /* 帧 ID */

/* \brief A.5 特定视频类VC接口描述符子类型 */
#define UVC_VC_DESCRIPTOR_UNDEFINED                   0x00         /* UVC未定义描述符类型*/
#define UVC_VC_HEADER                                 0x01         /* UVC头描述符*/
#define UVC_VC_INPUT_TERMINAL                         0x02         /* UVC输入终端描述符*/
#define UVC_VC_OUTPUT_TERMINAL                        0x03         /* UVC输出终端描述符*/
#define UVC_VC_SELECTOR_UNIT                          0x04         /* UVC选择器单元描述符*/
#define UVC_VC_PROCESSING_UNIT                        0x05         /* UVC处理单元描述符*/
#define UVC_VC_EXTENSION_UNIT                         0x06         /* UVC扩展单元描述符*/

/* A.6. 特定视频类VC接口描述符子类型 */
#define UVC_VS_UNDEFINED                              0x00         /* UVC未定义描述符*/
#define UVC_VS_INPUT_HEADER                           0x01         /* UVC输入头描述符*/
#define UVC_VS_OUTPUT_HEADER                          0x02         /* UVC输出头描述符*/
#define UVC_VS_STILL_IMAGE_FRAME                      0x03
#define UVC_VS_FORMAT_UNCOMPRESSED                    0x04         /* 无压缩视频格式*/
#define UVC_VS_FRAME_UNCOMPRESSED                     0x05         /* 无压缩帧*/
#define UVC_VS_FORMAT_MJPEG                           0x06         /* MJPEG格式*/
#define UVC_VS_FRAME_MJPEG                            0x07         /* MJPEG帧*/
#define UVC_VS_FORMAT_MPEG2TS                         0x0a         /* MPEG2-TS格式*/
#define UVC_VS_FORMAT_DV                              0x0c         /* DV格式*/
#define UVC_VS_COLORFORMAT                            0x0d
#define UVC_VS_FORMAT_FRAME_BASED                     0x10         /* 供应商自定义格式*/
#define UVC_VS_FRAME_FRAME_BASED                      0x11         /* 通用基本帧*/
#define UVC_VS_FORMAT_STREAM_BASED                    0x12         /* MPEG1-SS/MPEG2-PS格式*/

/* \brief A.8.视频特定类请求代码*/
#define UVC_RC_UNDEFINED                              0x00
#define UVC_SET_CUR                                   0x01
#define UVC_GET_CUR                                   0x81         /* 返回要协商的字段的默认值*/
#define UVC_GET_MIN                                   0x82         /* 返回要协商的字段的最小值*/
#define UVC_GET_MAX                                   0x83         /* 返回要协商的字段的最大值*/
#define UVC_GET_RES                                   0x84
#define UVC_GET_LEN                                   0x85
#define UVC_GET_INFO                                  0x86
#define UVC_GET_DEF                                   0x87

/* \brief A.9.4. 摄像头终端控制选择器*/
#define UVC_CT_CONTROL_UNDEFINED                      0x00
#define UVC_CT_SCANNING_MODE_CONTROL                  0x01
#define UVC_CT_AE_MODE_CONTROL                        0x02
#define UVC_CT_AE_PRIORITY_CONTROL                    0x03
#define UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL         0x04
#define UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL         0x05
#define UVC_CT_FOCUS_ABSOLUTE_CONTROL                 0x06
#define UVC_CT_FOCUS_RELATIVE_CONTROL                 0x07
#define UVC_CT_FOCUS_AUTO_CONTROL                     0x08
#define UVC_CT_IRIS_ABSOLUTE_CONTROL                  0x09
#define UVC_CT_IRIS_RELATIVE_CONTROL                  0x0a
#define UVC_CT_ZOOM_ABSOLUTE_CONTROL                  0x0b
#define UVC_CT_ZOOM_RELATIVE_CONTROL                  0x0c
#define UVC_CT_PANTILT_ABSOLUTE_CONTROL               0x0d
#define UVC_CT_PANTILT_RELATIVE_CONTROL               0x0e
#define UVC_CT_ROLL_ABSOLUTE_CONTROL                  0x0f
#define UVC_CT_ROLL_RELATIVE_CONTROL                  0x10
#define UVC_CT_PRIVACY_CONTROL                        0x11

/* \brief A.9.5. 处理单元控制选择器*/
#define UVC_PU_CONTROL_UNDEFINED                      0x00
#define UVC_PU_BACKLIGHT_COMPENSATION_CONTROL         0x01
#define UVC_PU_BRIGHTNESS_CONTROL                     0x02
#define UVC_PU_CONTRAST_CONTROL                       0x03
#define UVC_PU_GAIN_CONTROL                           0x04
#define UVC_PU_POWER_LINE_FREQUENCY_CONTROL           0x05
#define UVC_PU_HUE_CONTROL                            0x06
#define UVC_PU_SATURATION_CONTROL                     0x07
#define UVC_PU_SHARPNESS_CONTROL                      0x08
#define UVC_PU_GAMMA_CONTROL                          0x09
#define UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL      0x0a
#define UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL 0x0b
#define UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL        0x0c
#define UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL   0x0d
#define UVC_PU_DIGITAL_MULTIPLIER_CONTROL             0x0e
#define UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL       0x0f
#define UVC_PU_HUE_AUTO_CONTROL                       0x10
#define UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL          0x11
#define UVC_PU_ANALOG_LOCK_STATUS_CONTROL             0x12

/* A.9.7. 视频流接口控件选择器*/
#define UVC_VS_CONTROL_UNDEFINED                      0x00         /* 未定义控件*/
#define UVC_VS_PROBE_CONTROL                          0x01         /* 探测控件*/
#define UVC_VS_COMMIT_CONTROL                         0x02         /* 提交控件*/
#define UVC_VS_STILL_PROBE_CONTROL                    0x03
#define UVC_VS_STILL_COMMIT_CONTROL                   0x04
#define UVC_VS_STILL_IMAGE_TRIGGER_CONTROL            0x05
#define UVC_VS_STREAM_ERROR_CODE_CONTROL              0x06
#define UVC_VS_GENERATE_KEY_FRAME_CONTROL             0x07
#define UVC_VS_UPDATE_FRAME_SEGMENT_CONTROL           0x08
#define UVC_VS_SYNC_DELAY_CONTROL                     0x09

/* \brief B.1. USB 终端类型*/
#define UVC_TT_VENDOR_SPECIFIC                        0x0100       /* 特定供应商终端类型*/
#define UVC_TT_STREAMING                              0x0101       /* 流终端类型*/

/* \brief B.2. 输入终端类型*/
#define UVC_ITT_VENDOR_SPECIFIC                       0x0200       /* 供应商特定输入终端*/
#define UVC_ITT_CAMERA                                0x0201       /* 摄像头传感器。仅用于摄像头终端描述符*/
#define UVC_ITT_MEDIA_TRANSPORT_INPUT                 0x0202       /* 序列媒体。仅用于媒体传输终端描述符*/

/* \brief B.3. 输出终端类型*/
#define UVC_OTT_VENDOR_SPECIFIC                       0x0300       /* 特定供应商输出终端*/
#define UVC_OTT_DISPLAY                               0x0301       /* 显示输出终端*/
#define UVC_OTT_MEDIA_TRANSPORT_OUTPUT                0x0302       /* 媒体输出终端*/

#define UVC_TERM_INPUT                                0x0000       /* 输入终端*/
#define UVC_TERM_OUTPUT                               0x8000       /* 输出终端*/

#endif

