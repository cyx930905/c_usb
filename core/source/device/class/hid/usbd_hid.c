/*******************************************************************************
 *                                    ZLG
 *                         ----------------------------
 *                         innovating embedded platform
 *
 * Copyright (c) 2001-present Guangzhou ZHIYUAN Electronics Co., Ltd.
 * All rights reserved.
 *
 * Contact information:
 * web site:    https://www.zlg.cn
 *******************************************************************************/
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/device/class/hid/usbd_hid.h"

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbd_core_lib __g_usb_device_lib;

/*******************************************************************************
 * Code
 ******************************************************************************/
/* \brief USB HID 设备特定的描述符*/
static uint8_t __g_hid_spec_desc[] = {
    /* HID 描述符*/
    sizeof(struct hid_descriptor),
    HID_HID_DESCRIPTOR_TYPE,
    USB_WBVAL(0x0111),                  /* HID 版本*/
    0x00,                               /* 国家代码*/
    0x01,                               /* 描述符数量*/
    HID_REPORT_DESCRIPTOR_TYPE,         /* 描述符类型*/
    USB_WBVAL(0),                       /* 描述符长度*/
};

/* \brief USB 接口功能信息定义 */
static struct usbd_func_info __g_hid_func_info = {
        .intf_num        = USBD_HID_INTF_NUM,
        .alt_setting_num = USBD_HID_INTF_ALT_NUM,
        .class           = USB_CLASS_HID,
        .sub_class       = 0,
        .protocol        = 0,
        .p_intf_str      = "",
        .p_spec_desc     = __g_hid_spec_desc,
        .spec_desc_size  = sizeof(__g_hid_spec_desc),
};

/* \brief 数据接口功能输出管道信息定义 */
static struct usbd_pipe_info __g_hid_data_pipe_out_info = {
        .num   = 0,
        .mps   = 512,
        .dir   = USB_DIR_OUT,
        .attr  = USB_EP_TYPE_INT,
        .inter = 1
};

/* \brief 数据接口功能输入管道信息定义 */
static struct usbd_pipe_info __g_hid_data_pipe_in_info = {
        .num   = 0,
        .mps   = 512,
        .dir   = USB_DIR_IN,
        .attr  = USB_EP_TYPE_INT,
        .inter = 1
};

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 关闭接口功能
 */
static int __hid_shutdown(struct usbd_func *p_func, void *p_arg){
    struct usbd_hid *p_hid = (struct usbd_hid *)p_arg;

    p_hid->is_setup = USB_FALSE;

    return USB_OK;
}

/**
 * \brief USB 人体接口设备设置函数
 */
static int __hid_cmd_setup(struct usbd_func   *p_func,
                           struct usb_ctrlreq *p_setup,
                           void               *p_arg){
    struct usbd_hid *p_hid = (struct usbd_hid *)p_arg;

    /* 类请求*/
    if ((p_setup->request_type & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS) {
        /* 具体请求*/
        switch (p_setup->request) {
            case HID_REQ_GET_REPORT:
                __USB_INFO("HID request report get\r\n");
                return USB_OK;

            case HID_REQ_GET_IDLE:
                __USB_INFO("HID request idle get\r\n");
                return USB_OK;

            case HID_REQ_SET_IDLE:
                __USB_INFO("HID request idle set\r\n");
                return USB_OK;
            default:
                return USB_OK;
        }
    }
    /* 接口请求*/
    if ((p_setup->request_type & USB_REQ_TAG_MASK) == USB_REQ_TAG_INTERFACE) {
        /* 获取接口编号*/
        uint16_t value  = USB_CPU_TO_LE16(p_setup->value);

        switch (p_setup->request) {
            /* 获取描述符请求*/
            case USB_REQ_GET_DESCRIPTOR:
                switch (value >> 8) {
                    /* 报告描述符类型*/
                    case HID_REPORT_DESCRIPTOR_TYPE:
                        __USB_INFO("HID report descriptor type\r\n");

                        return usb_dc_ep0_req(USB_DC_GET_FROM_FUNC(p_func),
                                              USB_DIR_IN,
                                             (uint8_t *)p_hid->p_report_data,
                                              p_hid->report_data_size,
                                              NULL,
                                              NULL);
                }
                break;
            default:
                return -USB_ENOTSUP;
            }
    }
    return USB_OK;
}

/**
 * \brief USB 人体接口设备接口功能设置函数
 */
static int __hid_data_func_set(struct usbd_func *p_func, void *p_arg){
    int              ret;
    struct usbd_hid *p_hid = NULL;

    p_hid = (struct usbd_hid *)p_arg;

    ret = usbd_dev_pipe_get(USBD_DEV_GET_FROM_FUNC(p_func),
                            p_hid->data_out_ep_addr & 0x0F,
                            USB_DIR_OUT,
                           &p_hid->p_data_out);
    if (ret != USB_OK) {
        return ret;
    }

    ret = usbd_dev_pipe_get(USBD_DEV_GET_FROM_FUNC(p_func),
                            p_hid->data_in_ep_addr & 0x0F,
                            USB_DIR_IN,
                           &p_hid->p_data_in);
    if (ret != USB_OK) {
        return ret;
    }
    p_hid->is_setup = USB_TRUE;

    return USB_OK;
}

/* \brief USB 设备人体接口设备接口功能操作函数  */
static struct usbd_func_opts __g_hid_func_opts = {
    __hid_shutdown,
    __hid_cmd_setup,
    __hid_data_func_set,
    NULL,
    NULL,
    NULL
};

/**
 * \brief 创建一个 USB 从机人体接口设备
 *
 * \param[in]  p_dc             所属的 USB 从机控制器
 * \param[in]  p_name           设备名字
 * \param[in]  dev_desc         设备描述符
 * \param[in]  p_report_data    报告数据集合
 * \param[in]  report_data_size 报告数据大小
 * \param[out] p_hid_ret        返回创建成功的 USB 从机人体接口设
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_create(struct usb_dc        *p_dc,
                    char                 *p_name,
                    struct usbd_dev_desc  dev_desc,
                    char                 *p_report_data,
                    uint16_t              report_data_size,
                    struct usbd_hid     **p_hid_ret){
    int              ret;
    struct usbd_hid *p_hid = NULL;

    if ((p_name == NULL) ||
            (p_dc == NULL) ||
            (p_report_data == NULL) ||
            (report_data_size == 0)) {
        return -USB_EINVAL;
    }

    p_hid = usb_lib_malloc(&__g_usb_device_lib.lib, sizeof(struct usbd_hid));
    if (p_hid == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_hid, 0, sizeof(struct usbd_hid));

    p_hid->p_report_data    = p_report_data;
    p_hid->report_data_size = report_data_size;

    /* 设置 USB HID 特定描述符*/
    __g_hid_spec_desc[7] = p_hid->report_data_size & 0xff;
    __g_hid_spec_desc[8] = (p_hid->report_data_size >> 8) & 0xff;

    /* 创建一个 USB 从机设备*/
    ret = usbd_dev_create(p_dc, p_name, dev_desc, &p_hid->p_dc_dev);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device create failed(%d)\r\n", ret);
        goto __failed1;
    }
    /* 添加配置*/
    ret = usbd_dev_cfg_add(p_hid->p_dc_dev, NULL, &p_hid->cfg_num);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device config add failed(%d)\r\n", ret);
        goto __failed2;
    }
    /* 创建一个功能接口*/
    ret = usbd_dev_func_add(p_hid->p_dc_dev,
                            p_hid->cfg_num,
                          __g_hid_func_info,
                         &__g_hid_func_opts,
                            p_hid);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device function add failed(%d)\r\n", ret);
        goto __failed2;
    }

    /* 添加数据输入管道 */
    ret = usbd_dev_pipe_add(p_hid->p_dc_dev,
                            p_hid->cfg_num,
                            USBD_HID_INTF_NUM,
                            USBD_HID_INTF_ALT_NUM,
                          __g_hid_data_pipe_in_info,
                           &p_hid->data_in_ep_addr);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device pipe add failed(%d)\r\n", ret);
        goto __failed2;
    }
    /* 添加数据输出管道 */
    ret = usbd_dev_pipe_add(p_hid->p_dc_dev,
                            p_hid->cfg_num,
                            USBD_HID_INTF_NUM,
                            USBD_HID_INTF_ALT_NUM,
                          __g_hid_data_pipe_out_info,
                           &p_hid->data_out_ep_addr);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device pipe add failed(%d)\r\n", ret);
        goto __failed2;
    }
    *p_hid_ret = p_hid;

    return USB_OK;
__failed2:
    usbd_dev_destroy(p_hid->p_dc_dev);
__failed1:
    usb_lib_mfree(&__g_usb_device_lib.lib, p_hid);

    return ret;
}

/**
 * \brief 销毁一个 USB 从机人体接口设备
 *
 * \param[in] p_hid 要销毁的 USB 从机人体接口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_destroy(struct usbd_hid *p_hid){
    int ret;

    if (p_hid == NULL) {
        return USB_OK;
    }

    ret = usbd_dev_destroy(p_hid->p_dc_dev);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device \"%s\" destroy failed(%d)\r\n", p_hid->p_dc_dev->name, ret);
        return ret;
    }
    usb_lib_mfree(&__g_usb_device_lib.lib, p_hid);

    return USB_OK;
}

/**
 * \brief 检查 USB 从机人体接口设备是否设置
 *
 * \param[in] p_hid USB 人体接口设备
 *
 * \retval 设置返回 USB_TRUE，否则返回 USB_FALSE
 */
usb_bool_t usbd_hid_is_setup(struct usbd_hid *p_hid){
    if (p_hid == NULL) {
        return USB_FALSE;
    }
    return p_hid->is_setup;
}

/**
 * \brief USB 从机人体接口设备写数据
 *
 * \param[in]  p_hid     USB 人体接口设备
 * \param[in]  p_buf     要发送的数据缓存
 * \param[in]  len       数据长度
 * \param[in]  timeout   超时时间
 * \param[out] p_act_len 返回实际发送的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_write(struct usbd_hid *p_hid,
                   uint8_t         *p_buf,
                   uint32_t         len,
                   int              timeout,
                   uint32_t        *p_act_len){

    /* 检查参数合法性*/
    if (p_hid == NULL) {
        return -USB_EINVAL;
    }

    return usbd_dev_trans_sync(p_hid->p_dc_dev,
                               p_hid->p_data_in,
                               p_buf,
                               len,
                               0,
                               timeout,
                               p_act_len);
}

/**
 * \brief USB 从机人体接口设备同步读数据
 *
 * \param[in]  p_hid     USB 人体接口设备
 * \param[in]  p_buf     要读取的数据缓存
 * \param[in]  len       数据长度
 * \param[in]  timeout   超时时间
 * \param[out] p_act_len 返回实际读到的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_read_sync(struct usbd_hid *p_hid,
                       uint8_t         *p_buf,
                       uint32_t         len,
                       int              timeout,
                       uint32_t        *p_act_len){

    /* 检查参数合法性*/
    if (p_hid == NULL) {
        return -USB_EINVAL;
    }

    return usbd_dev_trans_sync(p_hid->p_dc_dev,
                               p_hid->p_data_out,
                               p_buf,
                               len,
                               0,
                               timeout,
                               p_act_len);
}

