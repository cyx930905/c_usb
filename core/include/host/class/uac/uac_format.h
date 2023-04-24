#ifndef __UAC_FORMAT_H_
#define __UAC_FORMAT_H_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "host/class/uac/usbh_uac_driver.h"
#include "host/class/uac/uac.h"
#include "host/class/uac/uac_pcm.h"
#include "usb_config.h"

/* 打印函数*/
#define __UAC_FORMAT_TRACE(info, ...) \
            do { \
                usb_printf("USBH-UAC-FORMAT:"info, ##__VA_ARGS__); \
            } while (0)

/** 获取PCM格式宽度*/
int uac_get_pcm_format_size_bit(uint32_t format_size);
/** UAC 分析音频格式*/
usb_err_t usbh_uac_parse_audio_format(struct usbh_audio *chip,
                   struct audioformat *fp, unsigned int format,
                   struct uac_format_type_i_continuous_descriptor *fmt,
                   int stream);

#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif

