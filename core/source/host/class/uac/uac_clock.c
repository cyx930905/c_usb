#include "host/class/uac/uac_clock.h"
#include "host/class/uac/uac_quirks.h"
#include <string.h>
#include <stdio.h>

/** 寻找时钟源描述符*/
static struct uac_clock_source_descriptor *
              uac_find_clock_source(struct usbh_interface *ctrl_iface,
                                    int                    clock_id)
{
    struct uac_clock_source_descriptor *cs = NULL;

    while ((cs = usbh_uac_find_csint_desc(ctrl_iface->extra,
                                          ctrl_iface->extralen,
                                          cs, UAC2_CLOCK_SOURCE))) {
        if (cs->bClockID == clock_id)
            return cs;
    }

    return NULL;
}

/** 寻找时钟选择器描述符*/
static struct uac_clock_selector_descriptor *
        uac_find_clock_selector(struct usbh_interface *ctrl_iface,
                                int                    clock_id)
{
    struct uac_clock_selector_descriptor *cs = NULL;

    while ((cs = usbh_uac_find_csint_desc(ctrl_iface->extra,
                         ctrl_iface->extralen,
                         cs, UAC2_CLOCK_SELECTOR))) {
        if (cs->bClockID == clock_id)
            return cs;
    }

    return NULL;
}

/** 获取时钟选择器值*/
static uint8_t uac_clock_selector_get_val(struct usbh_audio *chip, int selector_id)
{
    uint8_t buf;
    usb_err_t ret;

    ret = usbh_uac_ctl_msg(chip->p_fun, UAC2_CS_CUR,
                           USB_REQ_TAG_INTERFACE | USB_REQ_TYPE_CLASS | USB_DIR_IN,
                           UAC2_CX_CLOCK_SELECTOR << 8,
                           USBH_GET_INTF_NUM(chip->ctrl_intf) | (selector_id << 8),
                           &buf, sizeof(buf));

    if (ret < 0){
        return ret;
    }
    return buf;
}

/** 设置时钟选择器值*/
static usb_err_t uac_clock_selector_set_val(struct usbh_audio *chip, int selector_id,
                                            uint8_t pin)
{
    usb_err_t ret;

    ret = usbh_uac_ctl_msg(chip->p_fun, UAC2_CS_CUR,
                  USB_REQ_TAG_INTERFACE | USB_REQ_TYPE_CLASS | USB_DIR_OUT,
                  UAC2_CX_CLOCK_SELECTOR << 8,
                  USBH_GET_INTF_NUM(chip->ctrl_intf) | (selector_id << 8),
                  &pin, sizeof(pin));
    if (ret < 0){
        return ret;
    }
    if (ret != sizeof(pin)) {
        __UAC_CLOCK_TRACE("setting selector (id %d) unexpected length %d\r\n",
            selector_id, ret);
        return -USB_EINVAL;
    }

    ret = uac_clock_selector_get_val(chip, selector_id);
    if (ret < 0){
        return ret;
    }
    if (ret != pin) {
        __UAC_CLOCK_TRACE("setting selector (id %d) to %x failed (current: %d)\r\n",
            selector_id, pin, ret);
        return -USB_EINVAL;
    }

    return ret;
}

/** 检查时钟源是否有效*/
static usb_bool_t uac_clock_source_is_valid(struct usbh_audio *chip, int source_id)
{
    usb_err_t err;
    uint8_t data = 0;
    struct uac_clock_source_descriptor *cs_desc =
            uac_find_clock_source(chip->ctrl_intf, source_id);

    if ((cs_desc == NULL) || (chip->p_fun == NULL)){
        return USB_FALSE;
    }
    /* 如果一个时钟源不能告诉我们时候有效，我们假设它有效*/
    if (uac2_control_is_readable(cs_desc->bmControls,
                      UAC2_CS_CONTROL_CLOCK_VALID - 1) == USB_FALSE){
        return USB_TRUE;
    }
    err = usbh_uac_ctl_msg(chip->p_fun, UAC2_CS_CUR,
                           USB_REQ_TYPE_CLASS | USB_REQ_TAG_INTERFACE | USB_DIR_IN,
                           UAC2_CS_CONTROL_CLOCK_VALID << 8,
                           USBH_GET_INTF_NUM(chip->ctrl_intf) | (source_id << 8),
                           &data, sizeof(data));

    if (err < 0) {
        __UAC_CLOCK_TRACE("%s(): cannot get clock validity for id %d\r\n",
               __func__, source_id);
        return USB_FALSE;
    }

    return !!data;
}

/** 获取时钟滤波器描述符*/
static struct uac_clock_multiplier_descriptor *
        uac_find_clock_multiplier(struct usbh_interface *ctrl_iface,
                                  int                    clock_id)
{
    struct uac_clock_multiplier_descriptor *cs = NULL;

    while ((cs = usbh_uac_find_csint_desc(ctrl_iface->extra,
                                          ctrl_iface->extralen,
                                           cs, UAC2_CLOCK_MULTIPLIER))) {
        if (cs->bClockID == clock_id){
            return cs;
        }
    }

    return NULL;
}

/** 寻找时钟源*/
int usbh_uac_clock_find_source(struct usbh_audio *chip,
                               int                entity_id,
                               usb_bool_t         validate){
    struct uac_clock_source_descriptor *source;
    struct uac_clock_selector_descriptor *selector;
    struct uac_clock_multiplier_descriptor *multiplier;

    entity_id &= 0xff;

    /* 检查我们查找的ID是否已经是个时钟源*/
    source = uac_find_clock_source(chip->ctrl_intf, entity_id);
    if (source) {
        entity_id = source->bClockID;
        if (validate && !uac_clock_source_is_valid(chip, entity_id)) {
            __UAC_CLOCK_TRACE("clock source %d is not valid, cannot use\r\n",
                entity_id);
            return -USB_ENXIO;
        }
        return entity_id;
    }

    selector = uac_find_clock_selector(chip->ctrl_intf, entity_id);
    if (selector) {
        int ret, i, cur;

        /* 我们找的实体ID是一个选择器。检查当前的选择*/
        ret = uac_clock_selector_get_val(chip, selector->bClockID);
        if (ret < 0){
            return ret;
        }
        /* 选择器的值基于1*/
        if ((ret > selector->bNrInPins) || (ret < 1)) {
            __UAC_CLOCK_TRACE("%s(): selector reported illegal value, id %d, ret %d\r\n",
                __func__, selector->bClockID, ret);

            return -USB_EINVAL;
        }

        cur = ret;
        ret = usbh_uac_clock_find_source(chip, selector->baCSourceID[ret - 1], validate);
        if ((validate == USB_FALSE) || (ret > 0)){
            return ret;
        }
        /* 当前的时钟源是不可用的，尝试其他的*/
        for (i = 1; i <= selector->bNrInPins; i++) {
            int err;

            if (i == cur){
                continue;
            }
            ret = usbh_uac_clock_find_source(chip, selector->baCSourceID[i - 1], USB_TRUE);
            if (ret < 0)
                continue;

            err = uac_clock_selector_set_val(chip, entity_id, i);
            if (err < 0)
                continue;

            __UAC_CLOCK_TRACE("found and selected valid clock source %d\r\n",
                 ret);
            return ret;
        }

        return -USB_ENXIO;
    }

    /* FIXME: multipliers only act as pass-thru element for now */
    multiplier = uac_find_clock_multiplier(chip->ctrl_intf, entity_id);
    if (multiplier){
        return usbh_uac_clock_find_source(chip, multiplier->bCSourceID, validate);
    }
    return -USB_EINVAL;
}

