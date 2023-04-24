#include "host/class/uac/usbh_uac_driver.h"
#include "host/class/uac/usbh_uac_operation.h"
#include "host/class/uac/uac.h"
#include "host/class/uac/uac_pcm.h"
#include "host/class/uac/uac_quirks.h"
#include "host/class/uac/uac_mixer.h"
#include "host/class/uac/uac_endpoint.h"
#include "usb_config.h"
#include <string.h>
#include <stdio.h>

/* 系统音频设备结构体数组*/
static struct usbh_audio *__g_usbh_audio[USBH_SND_CARDS] = {NULL};
/* 音频设备注册互斥锁*/
static usb_mutex_handle_t uac_register_mutex = NULL;
static usb_bool_t ignore_ctl_error = USB_FALSE;

/** 合并字节并得到一个整数值*/
uint32_t usbh_uac_combine_bytes(uint8_t *bytes, int size)
{
    switch (size) {
    case 1:  return *bytes;
    case 2:  return combine_word(bytes);
    case 3:  return combine_triple(bytes);
    case 4:  return combine_quad(bytes);
    default: return 0;
    }
}

/**
 * 分析描述符缓存，返回给定的描述符类型的起始指针
 *
 * @param descstart 描述符起始地址
 * @param desclen   描述符长度
 * @param after
 * @param dtype     要寻找的描述符类型
 */
void *usbh_uac_find_desc(void *descstart, int desclen, void *after, uint8_t dtype)
{
    uint8_t *p, *end, *next;

    p = descstart;
    end = p + desclen;
    for (; p < end;) {
        if (p[0] < 2){
            return NULL;
        }
        next = p + p[0];
        if (next > end){
            return NULL;
        }
        if (p[1] == dtype && (!after || (void *)p > after)) {
            return p;
        }
        p = next;
    }
    return NULL;
}

/** 通过子类找到类特定的接口描述符*/
void *usbh_uac_find_csint_desc(void *buffer, int buflen, void *after, uint8_t dsubtype)
{
    uint8_t *p = after;

    while ((p = usbh_uac_find_desc(buffer, buflen, p,
               (USB_REQ_TYPE_CLASS | USB_DT_INTERFACE))) != NULL) {
        if (p[0] >= 3 && p[2] == dsubtype){
            return p;
        }
    }
    return NULL;
}

///** 获取端点的轮询间隔*/
//uint8_t usbh_uac_parse_datainterval(struct usbh_audio     *p_audio,
//                                    struct usbh_interface *p_intf)
//{
//    switch (uac_get_speed(p_audio->p_fun)) {
//    case USB_SPEED_HIGH:
//    case USB_SPEED_WIRELESS:
//    case USB_SPEED_SUPER:
//        if ((USBH_GET_EP_INTVAL(&p_intf->eps[0]) >= 1) &&
//                (USBH_GET_EP_INTVAL(&p_intf->eps[0]) <= 4)){
//            return USBH_GET_EP_INTVAL(&p_intf->eps[0]) - 1;
//        }
//        break;
//    default:
//        break;
//    }
//    return 0;
//}

/**
 * 寻找音频设备
 *
 * @param audio_index 音频设备索引
 *
 * @return 成功返回USB音频结构体
 */
struct usbh_audio *usbh_uac_dev_find(int audio_index){
    int i;
    usb_err_t ret;
    struct usbh_audio *p_audio = NULL;
    /* 默认声卡*/
    if(audio_index == USB_AUDIO_DEFAULT){
        ret = usb_mutex_lock(uac_register_mutex, 5000);
        if (ret != USB_OK) {
            return NULL;
        }
        for(i = 0; i < USBH_SND_CARDS; i++){
            if(__g_usbh_audio[i] != NULL){
                p_audio = __g_usbh_audio[i];
                usb_mutex_unlock(uac_register_mutex);
                return p_audio;
            }
        }
        usb_mutex_unlock(uac_register_mutex);
    }
    /* 使用特定声卡*/
    else if(audio_index >= 0){
        if(__g_usbh_audio[audio_index] != NULL){
            ret = usb_mutex_lock(uac_register_mutex, 5000);
            if (ret != USB_OK) {
                return NULL;
            }
            p_audio = __g_usbh_audio[audio_index];
            usb_mutex_unlock(uac_register_mutex);
            return p_audio;
        }
    }
    return NULL;
}

/**
 * UAC设置控制传输
 *
 * @param p_fun       相关的功能接口
 * @param request     具体的USB请求
 * @param requesttype 传输方向|请求类型|请求目标
 * @param value       参数
 * @param index       索引
 * @param data        数据(可以为空)
 * @param size        数据长度
 *
 * @return 如果有数据传输，成功返回成功传输的字节数
 *         如果没有数据传输，成功返回USB_OK
 */
usb_err_t usbh_uac_ctl_msg(struct usbh_function *p_fun, uint8_t request,
                           uint8_t requesttype, uint16_t value, uint16_t index, void *data,
                           uint16_t size)
{
    usb_err_t ret;
    void *buf = NULL;
    int timeout;

    if (size > 0) {
        buf = usb_mem_alloc(size);
        if (buf == NULL){
            return -USB_ENOMEM;
        }
        memset(buf, 0, size);
    }

    if (requesttype & USB_DIR_IN){
        timeout = USB_CTRL_GET_TIMEOUT;
    }else{
        timeout = USB_CTRL_SET_TIMEOUT;
    }
    /* 发起控制传输*/
    ret = usbh_ctrl_sync(&p_fun->p_udev->ep0, requesttype, request,
                         value, index, size, buf, timeout, 0);
    if (ret > 0) {
        memcpy(data, buf, ret);
    }
    usb_mem_free(buf);
    /* 控制传输兼容处理*/
    usbh_uac_ctl_msg_quirk(p_fun, request, requesttype, value, index, data, size);

    return ret;
}

/** 初始化音量控制(UAC规范V1)*/
static usb_err_t init_pitch_v1(struct usbh_audio *chip, int iface,
                               struct usbh_interface *alts,
                               struct audioformat *fmt)
{
    struct usbh_function *p_fun = chip->p_fun;
    uint8_t data[1];
    uint32_t ep;
    usb_err_t err;

    ep = alts->eps[0].p_desc->bEndpointAddress;
    data[0] = 1;

    if ((err = usbh_uac_ctl_msg(p_fun, UAC_SET_CUR,
                   USB_REQ_TYPE_CLASS | USB_REQ_TAG_ENDPOINT | USB_DIR_OUT,
                   UAC_EP_CS_ATTR_PITCH_CONTROL << 8, ep,
                   data, sizeof(data))) < 0) {
        __UAC_TRACE("%d:%d cannot set enable PITCH\r\n",
                    iface, ep);
        return err;
    }

    return USB_OK;
}

/** 初始化音量控制*/
usb_err_t usbh_uac_init_pitch(struct usbh_audio *chip, int iface,
                              struct usbh_interface *alts,
                              struct audioformat *fmt)
{
    /* 如果端点没有音量控制，跳出*/
    if (!(fmt->attributes & UAC_EP_CS_ATTR_PITCH_CONTROL)){
        return USB_OK;
    }
    switch (fmt->protocol) {
    case UAC_VERSION_1:
    default:
        return init_pitch_v1(chip, iface, alts, fmt);

    case UAC_VERSION_2:
        //todo
        return -USB_EPERM;
    }
}

/**
 * 创建USB音频设备(单个)数据流
 *
 * @param p_audio   USB音频设备
 * @param ctrlif    控制接口编号
 * @param interface 数据接口编号
 *
 * @return 成功返回USB_OK
 */
static usb_err_t usbh_uac_create_stream(struct usbh_audio *p_audio,
                                        uint32_t ctrlif, int interface){
    struct usbh_interface *p_intf = NULL;
    struct usbh_function  *p_func = NULL;

    if(p_audio->p_fun == NULL){
        return -USB_EINVAL;
    }
    /* 获取备用接口0*/
    p_intf = usbh_ifnum_to_if(p_audio->p_fun->p_udev, interface, 0);
    if(p_intf == NULL){
        __UAC_TRACE("%u:%d : does not exist\r\n", ctrlif, interface);
        return -USB_EINVAL;
    }
    /* 获取接口功能结构体*/
    p_func = usbh_ifnum_to_func(p_audio->p_fun->p_udev, interface);
    if(p_func == NULL){
        return -USB_EINVAL;
    }

    if ((p_audio->usb_id == UAC_ID(0x18d1, 0x2d04) ||
         p_audio->usb_id == UAC_ID(0x18d1, 0x2d05)) &&
        (interface == 0) &&
        (p_intf->p_desc->bInterfaceClass == USB_CLASS_VENDOR_SPEC) &&
        (p_intf->p_desc->bInterfaceClass == USB_SUBCLASS_VENDOR_SPEC)) {
        //todo 设备兼容处理，暂时不实现
    }

    if ((p_intf->p_desc->bInterfaceClass == USB_CLASS_AUDIO ||
            p_intf->p_desc->bInterfaceClass == USB_CLASS_VENDOR_SPEC) &&
            (p_intf->p_desc->bInterfaceSubClass == USB_SUBCLASS_MIDISTREAMING)) {
        //todo 设备兼容处理，暂时不实现
    }
    /* 跳过不支持的类*/
    if ((p_intf->p_desc->bInterfaceClass != USB_CLASS_AUDIO &&
            p_intf->p_desc->bInterfaceClass != USB_CLASS_VENDOR_SPEC) ||
            p_intf->p_desc->bInterfaceSubClass != USB_SUBCLASS_AUDIOSTREAMING) {
        __UAC_TRACE("%u:%d: skipping non-supported interface %d\r\n",
            ctrlif, interface, p_intf->p_desc->bInterfaceClass);
        return -USB_EINVAL;
    }
    /* 不支持低速设备*/
    if (uac_get_speed(p_audio->p_fun) == USB_SPEED_LOW) {
        __UAC_TRACE("low speed audio streaming not supported\r\n");
        return -USB_EINVAL;
    }
    /* 分析音频接口*/
    if (usbh_uac_parse_audio_interface(p_audio, interface) != USB_OK) {
        /* 复位当前的接口*/
        usbh_set_interface(p_func, interface, 0);
        return -USB_EINVAL;
    }

    return USB_OK;
}

/**
 * 创建USB音频设备数据流
 *
 * @param p_audio USB音频设备
 * @param ctrlif  控制接口编号
 *
 * @return 成功返回USB_OK
 */
static usb_err_t usbh_uac_create_streams(struct usbh_audio *p_audio, uint32_t ctrlif){
    void *control_header;
    struct usbh_interface *p_intf = NULL;
    int i, protocol;

    if(p_audio->p_fun == NULL){
        return -USB_EINVAL;
    }

    p_intf = usbh_ifnum_to_if(p_audio->p_fun->p_udev, ctrlif, 0);
    /* 寻找UAC header描述符*/
    control_header = usbh_uac_find_csint_desc(p_intf->extra,
                                              p_intf->extralen,
                                              NULL, UAC_HEADER);
    protocol = p_intf->p_desc->bInterfaceProtocol;
    if (control_header == NULL) {
        __UAC_TRACE("cannot find UAC_HEADER\r\n");
        return -USB_EINVAL;
    }

    switch (protocol) {
    default:
        __UAC_TRACE("unknown interface protocol %#02x, assuming v1\r\n", protocol);
        break;

    case UAC_VERSION_1: {
        struct uac1_ac_header_descriptor *h1 = control_header;

        if (h1->bInCollection == 0) {
            __UAC_TRACE("skipping empty audio interface (v1)\r\n");
            return -USB_EINVAL;
        }
        /* 检查长度*/
        if (h1->bLength < sizeof(*h1) + h1->bInCollection) {
            __UAC_TRACE("invalid UAC_HEADER (v1)\r\n");
            return -USB_EINVAL;
        }
        /* 创建数据流*/
        for (i = 0; i < h1->bInCollection; i++){
            usbh_uac_create_stream(p_audio, ctrlif, h1->baInterfaceNr[i]);
        }
        break;
    }
    case UAC_VERSION_2: {
        //todo 暂时没实现
        break;
    }
    }

    return USB_OK;
}

/** 断开数据流*/
static void usbh_uac_stream_disconnect(struct usbh_uac_stream *as)
{
    int idx;
    struct usbh_uac_substream *subs;

    if(as == NULL){
        return;
    }

    for (idx = 0; idx < 2; idx++) {
        subs = &as->substream[idx];
        if (!subs->num_formats){
            continue;
        }
        subs->interface = -1;
        subs->data_endpoint = NULL;
        subs->sync_endpoint = NULL;
    }
}

/**
 * 创建一个USB音频设备
 *
 * @param p_fun    USB功能接口
 * @param idx      USB音频设备索引
 * @param rp_audio 要返回的USB音频设备地址
 *
 * @return 成功返回USB_OK
 */
static usb_err_t usbh_uac_dev_init(struct usbh_function *p_fun, int idx,
                                   struct usbh_audio **rp_audio){
    struct usbh_audio *p_audio;
    *rp_audio = NULL;

    /* 检查设备速度*/
    switch (uac_get_speed(p_fun)) {
    case USB_SPEED_LOW:
    case USB_SPEED_FULL:
    case USB_SPEED_HIGH:
    case USB_SPEED_WIRELESS:
    case USB_SPEED_SUPER:
        break;
    default:
        __UAC_TRACE("unknown device speed %d\r\n", uac_get_speed(p_fun));
        return -USB_ENXIO;
    }
    /* 申请UAC设备内存*/
    p_audio = usb_mem_alloc(sizeof(struct usbh_audio));
    if (p_audio == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_audio, 0, sizeof(struct usbh_audio));

    p_audio->p_fun   = p_fun;
    p_audio->index   = idx;
    p_audio->usb_id  = UAC_ID(USBH_GET_DEV_VID(p_fun), USBH_GET_DEV_PID(p_fun));
    p_audio->ep_mutex = usb_mutex_create();
    if(p_audio->ep_mutex == NULL){
        return -USB_ENOMEM;
    }
    p_audio->stream_mutex = usb_mutex_create();
    if(p_audio->stream_mutex == NULL){
        return -USB_ENOMEM;
    }
    /* 初始化PCM链表*/
    USB_INIT_LIST_HEAD(&p_audio->stream_list);
    /* 初始化音频端点链表*/
    USB_INIT_LIST_HEAD(&p_audio->ep_list);
    /* 设置名字*/
    sprintf(p_audio->name, "USB Device %#04x:%#04x",
        (unsigned int)UAC_ID_VENDOR(p_audio->usb_id),
        (unsigned int)UAC_ID_PRODUCT(p_audio->usb_id));

    *rp_audio = p_audio;
    return USB_OK;
}

/**
 * 创建一个UAC设备
 *
 * @param p_fun USB接口功能
 *
 * @return 成功返回USB_OK
 */
usb_err_t usbh_uac_dev_create(struct usbh_function *p_fun){
    struct usbh_interface *p_intf = NULL;
    struct usbh_audio *p_audio;
    int ifnum, i;
    usb_err_t ret;

    if(p_fun == NULL){
        return -USB_EINVAL;
    }

    /* 获取第一个接口*/
    p_intf = usbh_host_get_first_intf(p_fun);
    if(p_intf == NULL){
        return -USB_ENODEV;
    }
    if(p_fun->sclss == USB_SUBCLASS_AUDIOCONTROL){
        __UAC_TRACE("find USB audio control interface %d\r\n", p_fun->ifirst);

        ifnum = USBH_GET_INTF_NUM(p_intf);
        /* 检查设备兼容*/
        ret = usbh_uac_apply_boot_quirk(p_fun, p_intf);
        if (ret != USB_OK){
            return ret;
        }
        /* 检查是否已经被注册*/
        p_audio = NULL;

        ret = usb_mutex_lock(uac_register_mutex, 5000);
        if (ret != USB_OK) {
            return ret;
        }
        /* 初始化一个USB音频设备*/
        if (p_audio == NULL) {
            for (i = 0; i < USBH_SND_CARDS; i++){
                if (__g_usbh_audio[i] == NULL) {
                    ret = usbh_uac_dev_init(p_fun, i, &p_audio);
                    if (ret < 0){
                        goto __error1;
                    }
                    p_audio->pm_intf = p_intf;
                    break;
                }
            }
            if (p_audio == NULL) {
                __UAC_TRACE("no available usb audio device\r\n");
                ret = -USB_ENODEV;
                goto __error1;
            }
        }
        /* 可能会有设备会有多个控制接口*/
        if (p_audio->ctrl_intf == NULL){
            p_audio->ctrl_intf = p_intf;
        }
        p_audio->txfr_quirk = 0;

        /* 创建一个USB音频流接口*/
        ret = usbh_uac_create_streams(p_audio, ifnum);
        if (ret != USB_OK){
            goto __error2;
        }
        /* 创建混音*/
//        ret = usbh_uac_create_mixer(p_audio, ifnum, ignore_ctl_error);
//        if (ret != USB_OK){
//            goto __error2;
//        }

        __g_usbh_audio[p_audio->index] = p_audio;
        p_audio->num_ctrl_intf++;
        /* 设置接口驱动数据*/
        p_fun->driver_priv = p_audio;
        usb_mutex_unlock(uac_register_mutex);
    }
    else if (p_fun->sclss == USB_SUBCLASS_AUDIOSTREAMING){
        __UAC_TRACE("find USB audio streaming interface %d\r\n", p_fun->ifirst);
        return USB_OK;
    }
    else if (p_fun->sclss == USB_SUBCLASS_MIDISTREAMING){
        __UAC_TRACE("find USB midi streaming interface %d\r\n", p_fun->ifirst);
        return USB_OK;
    }
    /* 设置USB设备类型*/
    p_fun->p_udev->dev_type = USBH_DEV_UAC;

    __UAC_TRACE("create a USB Audio Device, index %d\r\n", p_audio->index);

	return USB_OK;
__error2:
    usb_mem_free(p_audio);
__error1:
	usb_mutex_unlock(uac_register_mutex);
    return ret;
}

/**
 * 断开连接一个UAC设备
 *
 * @param p_fun USB接口功能
 *
 * @return 成功返回USB_OK
 */
static usb_err_t usbh_uac_dev_disconnect(struct usbh_audio *p_audio){
    usb_err_t ret;
    usb_bool_t was_shutdown;

    if(p_audio == NULL){
        return -USB_EINVAL;
    }
    /* 获取音频设备的关闭状态*/
    was_shutdown = p_audio->shutdown;
    p_audio->shutdown = USB_TRUE;

    ret = usb_mutex_lock(uac_register_mutex, 5000);
    if (ret != USB_OK) {
        return ret;
    }
    if (was_shutdown == USB_FALSE) {
        struct usbh_uac_stream *as = NULL;
        struct usbh_uac_endpoint *ep = NULL;
        /* 释放PCM资源*/
        ret = usb_mutex_lock(p_audio->stream_mutex, 5000);
        if (ret != USB_OK) {
            return ret;
        }
        usb_list_for_each_entry(as, &p_audio->stream_list, struct usbh_uac_stream,list) {
            usbh_uac_stream_disconnect(as);
        }
        usb_mutex_unlock(p_audio->stream_mutex);
        /* 释放端点资源*/
        ret = usb_mutex_lock(p_audio->ep_mutex, 5000);
        if (ret != USB_OK) {
            return ret;
        }
        usb_list_for_each_entry(ep, &p_audio->ep_list, struct usbh_uac_endpoint, list) {
            usbh_uac_endpoint_release(ep);
        }
        usb_mutex_unlock(p_audio->ep_mutex);
        /* 释放混音资源*/
        //todo
    }
    p_audio->num_ctrl_intf--;
    if (p_audio->num_ctrl_intf <= 0) {
        __g_usbh_audio[p_audio->index] = NULL;
        usb_mutex_unlock(uac_register_mutex);
    } else {
        usb_mutex_unlock(uac_register_mutex);
    }

    return USB_OK;
}

/** 销毁USB音频设备*/
usb_err_t usbh_uac_dev_destory(struct usbh_function *p_func)
{
    usb_err_t ret;
    struct usbh_audio *p_audio;
    struct usbh_uac_endpoint *ep, *n;
    struct usbh_uac_stream *as, *as_temp;
    if(p_func == NULL){
        return -USB_EINVAL;
    }

    if(p_func->driver_priv == NULL){
        return USB_OK;
    }

    p_audio = (struct usbh_audio *)p_func->driver_priv;

    usbh_uac_dev_disconnect(p_audio);

    /* 释放端点*/
    ret = usb_mutex_lock(p_audio->ep_mutex, 5000);
    if (ret != USB_OK) {
        return ret;
    }
    usb_list_for_each_entry_safe(ep, n, &p_audio->ep_list, struct usbh_uac_endpoint, list){
        usbh_uac_endpoint_free(ep);
    }
    usb_mutex_unlock(p_audio->ep_mutex);
    /* 释放USB 音频数据流*/
    ret = usb_mutex_lock(p_audio->stream_mutex, 5000);
    if (ret != USB_OK) {
        return ret;
    }
    usb_list_for_each_entry_safe(as, as_temp, &p_audio->stream_list, struct usbh_uac_stream, list){
        usbh_uac_audio_stream_free(as);
    }
    usb_mutex_unlock(p_audio->stream_mutex);
    /* 删除互斥锁*/
    if(p_audio->ep_mutex){
        usb_mutex_delete(p_audio->ep_mutex);
    }
    if(p_audio->stream_mutex){
        usb_mutex_delete(p_audio->stream_mutex);
    }
    usb_mem_free(p_audio);
    p_func->driver_priv = NULL;

    return USB_OK;
}


/** UAC驱动相关初始化*/
usb_err_t usbh_uac_lib_init(void){
    /* 创建注册互斥锁*/
	if(uac_register_mutex == NULL){
		uac_register_mutex = usb_mutex_create();
		if(uac_register_mutex == NULL){
			return -USB_ENOMEM;
		}
	}
	return USB_OK;
}







