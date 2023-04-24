#ifndef __UAC_CLOCK_H_
#define __UAC_CLOCK_H_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "host/class/uac/usbh_uac_driver.h"
#include "host/class/uac/uac.h"
#include "host/class/uac/uac_pcm.h"
#include "usb_config.h"
/* 打印函数*/
#define __UAC_CLOCK_TRACE(info, ...) \
            do { \
                usb_printf("USBH-UAC-CLOCK:"info, ##__VA_ARGS__); \
            } while (0)

/** 寻找时钟源*/
int usbh_uac_clock_find_source(struct usbh_audio *chip,
                               int                entity_id,
                               usb_bool_t         validate);
/** 初始化采样率*/
usb_err_t usbh_uac_init_sample_rate(struct usbh_audio *chip,
                                    struct usbh_uac_substream *subs, int iface,
                                    struct usbh_interface *alts,
                                    struct audioformat *fmt, int rate);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif

