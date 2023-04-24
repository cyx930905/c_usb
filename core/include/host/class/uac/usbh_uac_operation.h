#ifndef __USBH_UAC_OPERATION_H_
#define __USBH_UAC_OPERATION_H_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "usb_adapter_inter.h"
#include "usb_errno.h"
#include "host/core/usbh.h"
#include "host/class/uac/uac_pcm.h"
#include "host/class/uac/usbh_uac_driver.h"

/* USB音频设备句柄*/
typedef void* uac_handle;
/* 默认网卡*/
#define USB_AUDIO_DEFAULT  -1
/* 默认PCM*/
#define DEFAULT_PCM    -1


/* 用户硬件参数结构体*/
struct usbh_uac_params_usr {
    int      pcm_format_size;  /* PCM格式长度*/
    uint32_t channels;         /* 通道数量*/
    uint32_t rate;             /* 采样率*/
    uint32_t period_frames;    /* 一个周期中帧数量*/
    uint32_t buffer_periods;   /* 一个缓存的周期数量*/
    uint32_t bits_per_sample;  /* 样本的位数*/
};

/** UAC设备打开函数*/
uac_handle usbh_uac_open(int audio_index, int pcm_index, int stream);
/** UAC设备设置硬件参数*/
usb_err_t usbh_uac_params_set(uac_handle handle,
                              struct usbh_uac_params_usr *hw_params);
/** UAC设备准备函数*/
usb_err_t usbh_uac_prepare(uac_handle handle);
/**
 * USB音频设备写函数
 *
 * @param handle      USB音频设备句柄
 * @param buffer      数据缓存
 * @param buffer_size 缓存大小
 *
 * @return 成功返回成功传输的字节数
 */
usb_err_t usbh_uac_write(uac_handle handle, uint8_t *buffer, uint32_t buffer_size);
/** 停止USB音频*/
usb_err_t usbh_uac_stop(uac_handle handle, uac_pcm_state_t state);
/** UAC设备释放函数*/
void usbh_uac_release(uac_handle handle);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif

