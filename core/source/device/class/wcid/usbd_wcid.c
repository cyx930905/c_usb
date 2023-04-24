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
#include "core/include/device/class/wcid/usbd_wcid.h"

/*******************************************************************************
 * Static
 ******************************************************************************/
/* \brief USB 接口功能信息定义 */
static struct usbd_func_info __g_wcid_func_info = {
        .intf_num        = USBD_WCID_INTF_NUM,
        .alt_setting_num = USBD_WCID_INTF_ALT_NUM,
        .class           = USB_CLASS_VENDOR_SPEC,
        .sub_class       = USB_SUBCLASS_VENDOR_SPEC,
        .protocol        = 0,
        .p_intf_str      = "",
        .spec_desc_size  = 0,
};

/* \brief 数据接口功能输出管道信息定义 */
static struct usbd_pipe_info __g_wcid_data_pipe_out_info = {
        .num   = 0,
        .mps   = 512,
        .dir   = USB_DIR_OUT,
        .attr  = USB_EP_TYPE_BULK,
        .inter = 0
};

/* \brief 数据接口功能输入管道信息定义 */
static struct usbd_pipe_info __g_wcid_data_pipe_in_info = {
        .num   = 0,
        .mps   = 512,
        .dir   = USB_DIR_IN,
        .attr  = USB_EP_TYPE_BULK,
        .inter = 0
};

static const uint8_t __g_wcid_desc_1_0[40] = {
    0x28, 0x00, 0x00, 0x00,                                  /* 描述符长度*/
    0x00, 0x01,                                              /* BCD 版本*/
    0x04, 0x00,                                              /* 索引*/
    0x01,                                                    /* 接口数量*/
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00,                                                    /* 第一个接口号*/
    0x01,


    'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,                /* CID_8*/
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,          /* 子 CID_8*/
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t __g_wcid_prop_1_0[142] = {
    0x8e, 0x00, 0x00, 0x00,                                  /* 描述符长度*/
    0x00, 0x01,             // bcdVersion
    0x05, 0x00,             // wIndex
    0x01, 0x00,             // wCount

    //registry propter descriptor
    0x84, 0x00, 0x00, 0x00,  // dwSize
    0x01, 0x00, 0x00, 0x00,  // dwPropertyDataType
    0x28, 0x00,              // wPropertyNameLength

    /* DeviceInterfaceGUID */
    'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00,
    'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00,
    't', 0x00, 'e', 0x00, 'r', 0x00, 'f', 0x00,
    'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00,
    'U', 0x00, 'I', 0x00, 'D', 0x00, 0x00, 0x00,

    0x4e, 0x00, 0x00, 0x00, // dwPropertyDataLength

    /* {12345678-9012-3456-7890-123456789012} */
    '{',  0x00, '1',  0x00, '2',  0x00, '3',  0x00,
    '4',  0x00, '5',  0x00, '6',  0x00, '7',  0x00,
    '8',  0x00, '-',  0x00, '9' , 0x00, '0',  0x00,
    '1',  0x00, '2' , 0x00, '-',  0x00, '3',  0x00,
    '4',  0x00, '5',  0x00, '6',  0x00, '-',  0x00,
    '7',  0x00, '8',  0x00, '9',  0x00, '0',  0x00,
    '-',  0x00, '1',  0x00, '2',  0x00, '3',  0x00,
    '4',  0x00, '5',  0x00, '6',  0x00, '7',  0x00,
    '8',  0x00, '9',  0x00, '0',  0x00, '1',  0x00,
    '2',  0x00, '}',  0x00, 0x00,

};

/* \brief USB Microsoft OS 描述符*/
static struct usb_mircosoft_os_desc __g_wcid_mircosoft_os_1_0_desc = {
        __g_wcid_desc_1_0,
        __g_wcid_prop_1_0,
};

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbd_core_lib __g_usb_device_lib;

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief USB Windows 兼容 ID 设备命令设置函数
 */
static int __wcid_cmd_setup(struct usbd_func   *p_func,
                            struct usb_ctrlreq *p_setup,
                            void               *p_arg){
    switch(p_setup->request) {
        default:
            __USB_ERR_INFO("wcid request %x unknown\r\n", p_setup->request_type);
            return -USB_ENOTSUP;
    }

    return USB_OK;
}

/**
 * \brief 关闭接口功能
 */
static int __wcid_shutdown(struct usbd_func *p_func, void *p_arg){
    struct usbd_wcid *p_wcid = (struct usbd_wcid *)p_arg;

    p_wcid->is_setup = USB_FALSE;

    return USB_OK;
}

/**
 * \brief USB Windows 兼容 ID 设备备接口功能设置函数
 */
static int __wcid_alt_set(struct usbd_func *p_func, void *p_arg){
    int               ret;
    struct usbd_wcid *p_wcid = (struct usbd_wcid *)p_arg;

    ret = usbd_dev_pipe_get(USBD_DEV_GET_FROM_FUNC(p_func),
                            p_wcid->data_out_ep_addr & 0x0F,
                            USB_DIR_OUT,
                           &p_wcid->p_data_out);
    if (ret != USB_OK) {
        return ret;
    }

    ret = usbd_dev_pipe_get(USBD_DEV_GET_FROM_FUNC(p_func),
                            p_wcid->data_in_ep_addr & 0x0F,
                            USB_DIR_IN,
                           &p_wcid->p_data_in);
    if (ret != USB_OK) {
        return ret;
    }
    p_wcid->is_setup = USB_TRUE;

	return USB_OK;
}

/* \brief USB Windows 兼容 ID 设备接口功能操作函数  */
static struct usbd_func_opts __g_wcid_func_opts = {
    __wcid_shutdown,
    __wcid_cmd_setup,
    __wcid_alt_set,
    NULL,
    NULL,
    NULL
};

/**
 * \brief 创建一个 USB Windows 兼容 ID 设备
 *
 * \param[in]  p_dc       所属的 USB 从机控制器
 * \param[in]  p_name     设备名字
 * \param[in]  dev_desc   设备描述符
 * \param[out] p_wcid_ret 返回创建成功的 USB Windows 兼容 ID 设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbd_wcid_create(struct usb_dc        *p_dc,
                     char                 *p_name,
                     uint8_t               vendor_code,
                     struct usbd_dev_desc  dev_desc,
                     struct usbd_wcid    **p_wcid_ret){
    int               ret;
    struct usbd_wcid *p_wcid = NULL;

    if ((p_name == NULL) || (p_dc == NULL)) {
        return -USB_EINVAL;
    }

    p_wcid = usb_lib_malloc(&__g_usb_device_lib.lib, sizeof(struct usbd_wcid));
    if (p_wcid == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_wcid, 0, sizeof(struct usbd_wcid));

    /* 创建一个 USB 从机设备*/
    ret = usbd_dev_create(p_dc, p_name, dev_desc, &p_wcid->p_dc_dev);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device create failed(%d)\r\n", ret);
        goto __failed1;
    }

    p_wcid->p_dc_dev->p_mircosoft_os_desc = &__g_wcid_mircosoft_os_1_0_desc;
    p_wcid->p_dc_dev->vendor_code         = vendor_code;

    /* 添加配置*/
    ret = usbd_dev_cfg_add(p_wcid->p_dc_dev, NULL, &p_wcid->cfg_num);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device config add failed(%d)\r\n", ret);
        goto __failed2;
    }

    /* 创建一个功能接口*/
    ret = usbd_dev_func_add(p_wcid->p_dc_dev,
                            p_wcid->cfg_num,
                          __g_wcid_func_info,
                         &__g_wcid_func_opts,
                            p_wcid);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device function add failed(%d)\r\n", ret);
        goto __failed2;
    }

    /* 添加数据输出管道 */
    ret = usbd_dev_pipe_add(p_wcid->p_dc_dev,
                            p_wcid->cfg_num,
                            USBD_WCID_INTF_NUM,
                            USBD_WCID_INTF_ALT_NUM,
                          __g_wcid_data_pipe_out_info,
                           &p_wcid->data_out_ep_addr);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device pipe add failed(%d)\r\n", ret);
        goto __failed2;
    }

    /* 添加数据输入管道 */
    ret = usbd_dev_pipe_add(p_wcid->p_dc_dev,
                            p_wcid->cfg_num,
							USBD_WCID_INTF_NUM,
							USBD_WCID_INTF_ALT_NUM,
                          __g_wcid_data_pipe_in_info,
                           &p_wcid->data_in_ep_addr);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device pipe add failed(%d)\r\n", ret);
        goto __failed2;
    }

    *p_wcid_ret = p_wcid;

    return USB_OK;
__failed2:
    usbd_dev_destroy(p_wcid->p_dc_dev);
__failed1:
    usb_lib_mfree(&__g_usb_device_lib.lib, p_wcid);

    return ret;
}

/**
 * \brief 检查 USB Windows 兼容 ID 设备是否设置
 *
 * \param[in] p_wcid USB Windows 兼容 ID 设备
 *
 * \retval 设置返回 USB_TRUE，否则返回 USB_FALSE
 */
usb_bool_t usbd_wcid_is_setup(struct usbd_wcid *p_wcid){
    if (p_wcid == NULL) {
        return USB_FALSE;
    }
    return p_wcid->is_setup;
}

/**
 * \brief 销毁 USB Windows 兼容 ID 设备
 *
 * \param[in] p_wcid USB Windows 兼容 ID 设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_wcid_destroy(struct usbd_wcid *p_wcid){
    int ret;

    if (p_wcid == NULL) {
        return USB_OK;
    }

    ret = usbd_dev_destroy(p_wcid->p_dc_dev);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device \"%s\" destroy failed(%d)\r\n", p_wcid->p_dc_dev->name, ret);
        return ret;
    }

    usb_lib_mfree(&__g_usb_device_lib.lib, p_wcid);

    return USB_OK;
}

/**
 * \brief USB Windows 兼容 ID 设备同步读数据
 *
 * \param[in]  p_wcid    USB Windows 兼容 ID 设备
 * \param[in]  p_buf     读缓存
 * \param[in]  len       要读的长度
 * \param[in]  timeout   读超时时间
 * \param[out] p_act_len 返回实际读的数据
 *
 * \retval 成功返回 USB_OK
 */
int usbd_wcid_read_sync(struct usbd_wcid *p_wcid,
                        uint8_t          *p_buf,
                        uint32_t          len,
                        int               timeout,
                        uint32_t         *p_act_len){
    if ((p_wcid == NULL) || (p_act_len == NULL)) {
        return -USB_EINVAL;
    }

    return usbd_dev_trans_sync(p_wcid->p_dc_dev,
                               p_wcid->p_data_out,
                               p_buf,
                               len,
                               0,
                               timeout,
                               p_act_len);
}

/**
 * \brief USB Windows 兼容 ID 设备写数据
 *
 * \param[in]  p_wcid    USB Windows 兼容 ID 设备
 * \param[in]  p_buf     写缓存
 * \param[in]  len       要写的长度
 * \param[in]  timeout   写超时时间
 * \param[out] p_act_len 返回实际写的数据
 *
 * \retval 成功返回 USB_OK
 */
int usbd_wcid_write(struct usbd_wcid *p_wcid,
                    uint8_t          *p_buf,
                    uint32_t          len,
                    int               timeout,
                    uint32_t         *p_act_len){
    if ((p_wcid == NULL) || (p_act_len == NULL)) {
        return -USB_EINVAL;
    }

    return usbd_dev_trans_sync(p_wcid->p_dc_dev,
                               p_wcid->p_data_in,
							   p_buf,
                               len,
                               0,
                               timeout,
                               p_act_len);
}


