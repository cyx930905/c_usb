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

/* ϵͳ��Ƶ�豸�ṹ������*/
static struct usbh_audio *__g_usbh_audio[USBH_SND_CARDS] = {NULL};
/* ��Ƶ�豸ע�ụ����*/
static usb_mutex_handle_t uac_register_mutex = NULL;
static usb_bool_t ignore_ctl_error = USB_FALSE;

/** �ϲ��ֽڲ��õ�һ������ֵ*/
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
 * �������������棬���ظ��������������͵���ʼָ��
 *
 * @param descstart ��������ʼ��ַ
 * @param desclen   ����������
 * @param after
 * @param dtype     ҪѰ�ҵ�����������
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

/** ͨ�������ҵ����ض��Ľӿ�������*/
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

///** ��ȡ�˵����ѯ���*/
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
 * Ѱ����Ƶ�豸
 *
 * @param audio_index ��Ƶ�豸����
 *
 * @return �ɹ�����USB��Ƶ�ṹ��
 */
struct usbh_audio *usbh_uac_dev_find(int audio_index){
    int i;
    usb_err_t ret;
    struct usbh_audio *p_audio = NULL;
    /* Ĭ������*/
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
    /* ʹ���ض�����*/
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
 * UAC���ÿ��ƴ���
 *
 * @param p_fun       ��صĹ��ܽӿ�
 * @param request     �����USB����
 * @param requesttype ���䷽��|��������|����Ŀ��
 * @param value       ����
 * @param index       ����
 * @param data        ����(����Ϊ��)
 * @param size        ���ݳ���
 *
 * @return ��������ݴ��䣬�ɹ����سɹ�������ֽ���
 *         ���û�����ݴ��䣬�ɹ�����USB_OK
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
    /* ������ƴ���*/
    ret = usbh_ctrl_sync(&p_fun->p_udev->ep0, requesttype, request,
                         value, index, size, buf, timeout, 0);
    if (ret > 0) {
        memcpy(data, buf, ret);
    }
    usb_mem_free(buf);
    /* ���ƴ�����ݴ���*/
    usbh_uac_ctl_msg_quirk(p_fun, request, requesttype, value, index, data, size);

    return ret;
}

/** ��ʼ����������(UAC�淶V1)*/
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

/** ��ʼ����������*/
usb_err_t usbh_uac_init_pitch(struct usbh_audio *chip, int iface,
                              struct usbh_interface *alts,
                              struct audioformat *fmt)
{
    /* ����˵�û���������ƣ�����*/
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
 * ����USB��Ƶ�豸(����)������
 *
 * @param p_audio   USB��Ƶ�豸
 * @param ctrlif    ���ƽӿڱ��
 * @param interface ���ݽӿڱ��
 *
 * @return �ɹ�����USB_OK
 */
static usb_err_t usbh_uac_create_stream(struct usbh_audio *p_audio,
                                        uint32_t ctrlif, int interface){
    struct usbh_interface *p_intf = NULL;
    struct usbh_function  *p_func = NULL;

    if(p_audio->p_fun == NULL){
        return -USB_EINVAL;
    }
    /* ��ȡ���ýӿ�0*/
    p_intf = usbh_ifnum_to_if(p_audio->p_fun->p_udev, interface, 0);
    if(p_intf == NULL){
        __UAC_TRACE("%u:%d : does not exist\r\n", ctrlif, interface);
        return -USB_EINVAL;
    }
    /* ��ȡ�ӿڹ��ܽṹ��*/
    p_func = usbh_ifnum_to_func(p_audio->p_fun->p_udev, interface);
    if(p_func == NULL){
        return -USB_EINVAL;
    }

    if ((p_audio->usb_id == UAC_ID(0x18d1, 0x2d04) ||
         p_audio->usb_id == UAC_ID(0x18d1, 0x2d05)) &&
        (interface == 0) &&
        (p_intf->p_desc->bInterfaceClass == USB_CLASS_VENDOR_SPEC) &&
        (p_intf->p_desc->bInterfaceClass == USB_SUBCLASS_VENDOR_SPEC)) {
        //todo �豸���ݴ�����ʱ��ʵ��
    }

    if ((p_intf->p_desc->bInterfaceClass == USB_CLASS_AUDIO ||
            p_intf->p_desc->bInterfaceClass == USB_CLASS_VENDOR_SPEC) &&
            (p_intf->p_desc->bInterfaceSubClass == USB_SUBCLASS_MIDISTREAMING)) {
        //todo �豸���ݴ�����ʱ��ʵ��
    }
    /* ������֧�ֵ���*/
    if ((p_intf->p_desc->bInterfaceClass != USB_CLASS_AUDIO &&
            p_intf->p_desc->bInterfaceClass != USB_CLASS_VENDOR_SPEC) ||
            p_intf->p_desc->bInterfaceSubClass != USB_SUBCLASS_AUDIOSTREAMING) {
        __UAC_TRACE("%u:%d: skipping non-supported interface %d\r\n",
            ctrlif, interface, p_intf->p_desc->bInterfaceClass);
        return -USB_EINVAL;
    }
    /* ��֧�ֵ����豸*/
    if (uac_get_speed(p_audio->p_fun) == USB_SPEED_LOW) {
        __UAC_TRACE("low speed audio streaming not supported\r\n");
        return -USB_EINVAL;
    }
    /* ������Ƶ�ӿ�*/
    if (usbh_uac_parse_audio_interface(p_audio, interface) != USB_OK) {
        /* ��λ��ǰ�Ľӿ�*/
        usbh_set_interface(p_func, interface, 0);
        return -USB_EINVAL;
    }

    return USB_OK;
}

/**
 * ����USB��Ƶ�豸������
 *
 * @param p_audio USB��Ƶ�豸
 * @param ctrlif  ���ƽӿڱ��
 *
 * @return �ɹ�����USB_OK
 */
static usb_err_t usbh_uac_create_streams(struct usbh_audio *p_audio, uint32_t ctrlif){
    void *control_header;
    struct usbh_interface *p_intf = NULL;
    int i, protocol;

    if(p_audio->p_fun == NULL){
        return -USB_EINVAL;
    }

    p_intf = usbh_ifnum_to_if(p_audio->p_fun->p_udev, ctrlif, 0);
    /* Ѱ��UAC header������*/
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
        /* ��鳤��*/
        if (h1->bLength < sizeof(*h1) + h1->bInCollection) {
            __UAC_TRACE("invalid UAC_HEADER (v1)\r\n");
            return -USB_EINVAL;
        }
        /* ����������*/
        for (i = 0; i < h1->bInCollection; i++){
            usbh_uac_create_stream(p_audio, ctrlif, h1->baInterfaceNr[i]);
        }
        break;
    }
    case UAC_VERSION_2: {
        //todo ��ʱûʵ��
        break;
    }
    }

    return USB_OK;
}

/** �Ͽ�������*/
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
 * ����һ��USB��Ƶ�豸
 *
 * @param p_fun    USB���ܽӿ�
 * @param idx      USB��Ƶ�豸����
 * @param rp_audio Ҫ���ص�USB��Ƶ�豸��ַ
 *
 * @return �ɹ�����USB_OK
 */
static usb_err_t usbh_uac_dev_init(struct usbh_function *p_fun, int idx,
                                   struct usbh_audio **rp_audio){
    struct usbh_audio *p_audio;
    *rp_audio = NULL;

    /* ����豸�ٶ�*/
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
    /* ����UAC�豸�ڴ�*/
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
    /* ��ʼ��PCM����*/
    USB_INIT_LIST_HEAD(&p_audio->stream_list);
    /* ��ʼ����Ƶ�˵�����*/
    USB_INIT_LIST_HEAD(&p_audio->ep_list);
    /* ��������*/
    sprintf(p_audio->name, "USB Device %#04x:%#04x",
        (unsigned int)UAC_ID_VENDOR(p_audio->usb_id),
        (unsigned int)UAC_ID_PRODUCT(p_audio->usb_id));

    *rp_audio = p_audio;
    return USB_OK;
}

/**
 * ����һ��UAC�豸
 *
 * @param p_fun USB�ӿڹ���
 *
 * @return �ɹ�����USB_OK
 */
usb_err_t usbh_uac_dev_create(struct usbh_function *p_fun){
    struct usbh_interface *p_intf = NULL;
    struct usbh_audio *p_audio;
    int ifnum, i;
    usb_err_t ret;

    if(p_fun == NULL){
        return -USB_EINVAL;
    }

    /* ��ȡ��һ���ӿ�*/
    p_intf = usbh_host_get_first_intf(p_fun);
    if(p_intf == NULL){
        return -USB_ENODEV;
    }
    if(p_fun->sclss == USB_SUBCLASS_AUDIOCONTROL){
        __UAC_TRACE("find USB audio control interface %d\r\n", p_fun->ifirst);

        ifnum = USBH_GET_INTF_NUM(p_intf);
        /* ����豸����*/
        ret = usbh_uac_apply_boot_quirk(p_fun, p_intf);
        if (ret != USB_OK){
            return ret;
        }
        /* ����Ƿ��Ѿ���ע��*/
        p_audio = NULL;

        ret = usb_mutex_lock(uac_register_mutex, 5000);
        if (ret != USB_OK) {
            return ret;
        }
        /* ��ʼ��һ��USB��Ƶ�豸*/
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
        /* ���ܻ����豸���ж�����ƽӿ�*/
        if (p_audio->ctrl_intf == NULL){
            p_audio->ctrl_intf = p_intf;
        }
        p_audio->txfr_quirk = 0;

        /* ����һ��USB��Ƶ���ӿ�*/
        ret = usbh_uac_create_streams(p_audio, ifnum);
        if (ret != USB_OK){
            goto __error2;
        }
        /* ��������*/
//        ret = usbh_uac_create_mixer(p_audio, ifnum, ignore_ctl_error);
//        if (ret != USB_OK){
//            goto __error2;
//        }

        __g_usbh_audio[p_audio->index] = p_audio;
        p_audio->num_ctrl_intf++;
        /* ���ýӿ���������*/
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
    /* ����USB�豸����*/
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
 * �Ͽ�����һ��UAC�豸
 *
 * @param p_fun USB�ӿڹ���
 *
 * @return �ɹ�����USB_OK
 */
static usb_err_t usbh_uac_dev_disconnect(struct usbh_audio *p_audio){
    usb_err_t ret;
    usb_bool_t was_shutdown;

    if(p_audio == NULL){
        return -USB_EINVAL;
    }
    /* ��ȡ��Ƶ�豸�Ĺر�״̬*/
    was_shutdown = p_audio->shutdown;
    p_audio->shutdown = USB_TRUE;

    ret = usb_mutex_lock(uac_register_mutex, 5000);
    if (ret != USB_OK) {
        return ret;
    }
    if (was_shutdown == USB_FALSE) {
        struct usbh_uac_stream *as = NULL;
        struct usbh_uac_endpoint *ep = NULL;
        /* �ͷ�PCM��Դ*/
        ret = usb_mutex_lock(p_audio->stream_mutex, 5000);
        if (ret != USB_OK) {
            return ret;
        }
        usb_list_for_each_entry(as, &p_audio->stream_list, struct usbh_uac_stream,list) {
            usbh_uac_stream_disconnect(as);
        }
        usb_mutex_unlock(p_audio->stream_mutex);
        /* �ͷŶ˵���Դ*/
        ret = usb_mutex_lock(p_audio->ep_mutex, 5000);
        if (ret != USB_OK) {
            return ret;
        }
        usb_list_for_each_entry(ep, &p_audio->ep_list, struct usbh_uac_endpoint, list) {
            usbh_uac_endpoint_release(ep);
        }
        usb_mutex_unlock(p_audio->ep_mutex);
        /* �ͷŻ�����Դ*/
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

/** ����USB��Ƶ�豸*/
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

    /* �ͷŶ˵�*/
    ret = usb_mutex_lock(p_audio->ep_mutex, 5000);
    if (ret != USB_OK) {
        return ret;
    }
    usb_list_for_each_entry_safe(ep, n, &p_audio->ep_list, struct usbh_uac_endpoint, list){
        usbh_uac_endpoint_free(ep);
    }
    usb_mutex_unlock(p_audio->ep_mutex);
    /* �ͷ�USB ��Ƶ������*/
    ret = usb_mutex_lock(p_audio->stream_mutex, 5000);
    if (ret != USB_OK) {
        return ret;
    }
    usb_list_for_each_entry_safe(as, as_temp, &p_audio->stream_list, struct usbh_uac_stream, list){
        usbh_uac_audio_stream_free(as);
    }
    usb_mutex_unlock(p_audio->stream_mutex);
    /* ɾ��������*/
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


/** UAC������س�ʼ��*/
usb_err_t usbh_uac_lib_init(void){
    /* ����ע�ụ����*/
	if(uac_register_mutex == NULL){
		uac_register_mutex = usb_mutex_create();
		if(uac_register_mutex == NULL){
			return -USB_ENOMEM;
		}
	}
	return USB_OK;
}







