#ifndef __UAC_MIXER_H_
#define __UAC_MIXER_H_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */

#include "host/class/uac/usbh_uac_driver.h"
#include "host/class/uac/uac.h"
#include "list/usb_list.h"

/* 打印函数*/
#define __UAC_MIXER_TRACE(info, ...) \
            do { \
                usb_printf("USBH-UAC-MIXER:"info, ##__VA_ARGS__); \
            } while (0)

/** USB混音接口结构体*/
struct usb_mixer_interface {
    struct usbh_audio     *chip;        /* 相关的USB音频设备*/
    struct usbh_interface *itf;         /* 接口结构体*/
    int                    protocol;    /* 协议版本*/
    struct usb_list_head   list;        /* 当前节点*/
    struct usbh_trp       *p_trp;       /* USB传输请求包*/
};

/** 创建混音设备*/
usb_err_t usbh_uac_create_mixer(struct usbh_audio *chip, int ctrlif,
			                    usb_bool_t ignore_error);


static inline uint8_t uac_mixer_unit_bNrChannels(struct uac_mixer_unit_descriptor *desc)
{
    return desc->baSourceID[desc->bNrInPins];
}
static inline uint32_t uac_mixer_unit_wChannelConfig(struct uac_mixer_unit_descriptor *desc,
                          int protocol)
{
    if (protocol == UAC_VERSION_1){
        return (desc->baSourceID[desc->bNrInPins + 2] << 8) |
            desc->baSourceID[desc->bNrInPins + 1];
    }
    else{
        return  (desc->baSourceID[desc->bNrInPins + 4] << 24) |
            (desc->baSourceID[desc->bNrInPins + 3] << 16) |
            (desc->baSourceID[desc->bNrInPins + 2] << 8)  |
            (desc->baSourceID[desc->bNrInPins + 1]);
    }
}
static inline uint8_t *uac_mixer_unit_bmControls(struct uac_mixer_unit_descriptor *desc,
                          int protocol)
{
    return (protocol == UAC_VERSION_1) ?
        &desc->baSourceID[desc->bNrInPins + 4] :
        &desc->baSourceID[desc->bNrInPins + 6];
}
static inline uint8_t uac_mixer_unit_iMixer(struct uac_mixer_unit_descriptor *desc)
{
    uint8_t *raw = (uint8_t *) desc;
    return raw[desc->bLength - 1];
}
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif

