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
#include "core/include/device/class/virtual_printer/usbd_virtual_printer.h"

/*******************************************************************************
 * Static
 ******************************************************************************/
/* \brief USB 接口功能信息定义 */
static struct usbd_func_info __g_vp_func_info = {
        .intf_num        = USBD_VP_INTF_NUM,
        .alt_setting_num = USBD_VP_INTF_ALT_NUM,
        .class           = USB_CLASS_PRINTER,
        .sub_class       = 1,
        .protocol        = 2,
        .p_intf_str      = "",
        .spec_desc_size  = 0,
};

/* \brief 数据接口功能输出管道信息定义 */
static struct usbd_pipe_info __g_vp_data_pipe_out_info = {
        .num   = 0,
        .mps   = 512,
        .dir   = USB_DIR_OUT,
        .attr  = USB_EP_TYPE_BULK,
        .inter = 0
};

/* \brief 数据接口功能输入管道信息定义 */
static struct usbd_pipe_info __g_vp_data_pipe_in_info = {
        .num   = 0,
        .mps   = 512,
        .dir   = USB_DIR_IN,
        .attr  = USB_EP_TYPE_BULK,
        .inter = 0
};

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbd_core_lib __g_usb_device_lib;

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief USB 虚拟打印机命令设置函数
 */
static int __vp_cmd_setup(struct usbd_func   *p_func,
                          struct usb_ctrlreq *p_setup,
                          void               *p_arg){
    int             ret;
#if USB_OS_EN
    int             ret_tmp;
#endif
    uint32_t        len;
    uint16_t        index  = USB_CPU_TO_LE16(p_setup->index);
    uint16_t        value  = USB_CPU_TO_LE16(p_setup->value);
    uint16_t        length = USB_CPU_TO_LE16(p_setup->length);
    struct usbd_vp *p_vp   = (struct usbd_vp *)p_arg;

    /* 类请求*/
    if ((p_setup->request_type & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS) {
        /* 具体请求*/
        switch (p_setup->request) {
            /* 获取设备ID*/
            case USB_VP_GET_DEVICE_ID:
#if USB_OS_EN
                ret = usb_mutex_lock(p_vp->p_lock, USBD_DEV_MUTEX_TIMEOUT);
                if (ret != USB_OK) {
                    __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
                    return ret;
                }
#endif
                /* 只支持一个接口编号*/
                if ((index >> 8) != USBD_VP_INTF_NUM) {
                    break;
                }
                memset(p_vp->p_buf, 0, USB_VP_PNP_STRING_LEN);
                for (len = 0; len < strlen(p_vp->p_printer_id); len++) {
                	p_vp->p_buf[len + 2] = p_vp->p_printer_id[len];
                }
                len = strlen(p_vp->p_printer_id) - 1;
                p_vp->p_buf[0] = ((uint8_t)(len >> 8));
                p_vp->p_buf[1] = (uint8_t)len;
                ret            = len;
#if USB_OS_EN
                ret_tmp = usb_mutex_unlock(p_vp->p_lock);
                if (ret_tmp != USB_OK) {
                    __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
                    return ret_tmp;
                }
#endif
                __USB_INFO("1284 PNP String: %s\r\n", &p_vp->p_buf[2]);

                return usb_dc_ep0_req(USB_DC_GET_FROM_FUNC(p_func),
                                      USB_DIR_IN,
                                      p_vp->p_buf,
                                      ret,
                                      NULL,
                                      NULL);
            /* 获取设备状态*/
            case USB_VP_GET_PORT_STATUS:
                /* 只支持一个接口编号*/
                if (index != USBD_VP_INTF_NUM) {
                    break;
                }
                *(uint8_t *)p_vp->p_buf = p_vp->printer_sta;
                ret = min(length, 1);

                return usb_dc_ep0_req(USB_DC_GET_FROM_FUNC(p_func),
                                      USB_DIR_IN,
                                      p_vp->p_buf,
                                      ret,
                                      NULL,
                                      NULL);
            /* 软件复位*/
            case USB_VP_SOFT_RESET:
                /* 只支持一个接口编号*/
                if (index != USBD_VP_INTF_NUM) {
                    break;
                }
                /* 打印机复位*/
                //todo

                ret = 0;
                break;

            default:
                goto __unknown;
        }
    } else {
__unknown:
        __USB_ERR_INFO("unknown ctrl req%02x.%02x v%04x i%04x l%d\r\n",
                    p_setup->request_type, p_setup->request, value, index, length);
        return -USB_ENOTSUP;
    }
    return ret;
}

/**
 * \brief 关闭接口功能
 */
static int __vp_shutdown(struct usbd_func *p_func, void *p_arg){
    struct usbd_vp *p_vp = (struct usbd_vp *)p_arg;

    p_vp->is_setup = USB_FALSE;

    return USB_OK;
}

/**
 * \brief USB 设备虚拟打印机设备接口功能设置函数
 */
static int __vp_alt_set(struct usbd_func *p_func, void *p_arg){
    int             ret;
    struct usbd_vp *p_vp = (struct usbd_vp *)p_arg;

    ret = usbd_dev_pipe_get(USBD_DEV_GET_FROM_FUNC(p_func),
                            p_vp->data_out_ep_addr & 0x0F,
                            USB_DIR_OUT,
                           &p_vp->p_data_out);
    if (ret != USB_OK) {
        return ret;
    }

    ret = usbd_dev_pipe_get(USBD_DEV_GET_FROM_FUNC(p_func),
                            p_vp->data_in_ep_addr & 0x0F,
                            USB_DIR_IN,
                           &p_vp->p_data_in);
    if (ret != USB_OK) {
        return ret;
    }
    p_vp->is_setup = USB_TRUE;

    return USB_OK;
}

/* \brief USB 虚拟打印机设备接口功能操作函数  */
static struct usbd_func_opts __g_vp_func_opts = {
    __vp_shutdown,
    __vp_cmd_setup,
    __vp_alt_set,
    NULL,
    NULL,
    NULL
};

/**
 * \brief 创建一个 USB 虚拟打印机设备
 *
 * \param[in]  p_dc     所属的 USB 从机控制器
 * \param[in]  p_name   设备名字
 * \param[in]  dev_desc 设备描述符
 * \param[out] p_vp_ret 返回创建成功的 USB 虚拟打印机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbd_virtual_printer_create(struct usb_dc        *p_dc,
                                char                 *p_name,
                                struct usbd_dev_desc  dev_desc,
                                struct usbd_vp      **p_vp_ret){
    int             ret;
#if USB_OS_EN
    int             ret_tmp;
#endif
    struct usbd_vp *p_vp = NULL;

    if ((p_name == NULL) || (p_dc == NULL)) {
        return -USB_EINVAL;
    }

    p_vp = usb_lib_malloc(&__g_usb_device_lib.lib, sizeof(struct usbd_vp));
    if (p_vp == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_vp, 0, sizeof(struct usbd_vp));

    p_vp->printer_sta = USB_VP_NOT_ERROR;

    p_vp->p_buf = usb_lib_malloc(&__g_usb_device_lib.lib, USB_VP_PNP_STRING_LEN);
    if (p_vp->p_buf  == NULL) {
        ret = -USB_ENOMEM;
        goto __failed1;
    }
    memset(p_vp->p_buf , 0, USB_VP_PNP_STRING_LEN);

    p_vp->p_printer_id = usb_lib_malloc(&__g_usb_device_lib.lib, USB_VP_PNP_STRING_LEN);
    if (p_vp->p_printer_id  == NULL) {
        ret = -USB_ENOMEM;
        goto __failed1;
    }
    memset(p_vp->p_printer_id, 0, USB_VP_PNP_STRING_LEN);
    strcpy(p_vp->p_printer_id, "USB virtual printer");

#if USB_OS_EN
    p_vp->p_lock = usb_lib_mutex_create(&__g_usb_device_lib.lib);
    if (p_vp->p_lock == NULL) {
        ret = -USB_EPERM;
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        goto __failed1;
    }
#endif

    /* 创建一个 USB 从机设备*/
    ret = usbd_dev_create(p_dc, p_name, dev_desc, &p_vp->p_dc_dev);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device create failed(%d)\r\n", ret);
        goto __failed1;
    }

    /* 添加配置*/
    ret = usbd_dev_cfg_add(p_vp->p_dc_dev, NULL, &p_vp->cfg_num);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device config add failed(%d)\r\n", ret);
        goto __failed2;
    }

    /* 创建一个功能接口*/
    ret = usbd_dev_func_add(p_vp->p_dc_dev,
                            p_vp->cfg_num,
                          __g_vp_func_info,
                         &__g_vp_func_opts,
                            p_vp);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device function add failed(%d)\r\n", ret);
        goto __failed2;
    }

    /* 添加数据输出管道 */
    ret = usbd_dev_pipe_add(p_vp->p_dc_dev,
                            p_vp->cfg_num,
                            USBD_VP_INTF_NUM,
                            USBD_VP_INTF_ALT_NUM,
                          __g_vp_data_pipe_out_info,
                           &p_vp->data_out_ep_addr);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device pipe add failed(%d)\r\n", ret);
        goto __failed2;
    }

    /* 添加数据输入管道 */
    ret = usbd_dev_pipe_add(p_vp->p_dc_dev,
                            p_vp->cfg_num,
                            USBD_VP_INTF_NUM,
                            USBD_VP_INTF_ALT_NUM,
                          __g_vp_data_pipe_in_info,
                           &p_vp->data_in_ep_addr);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device pipe add failed(%d)\r\n", ret);
        goto __failed2;
    }

    *p_vp_ret = p_vp;

    return USB_OK;
__failed2:
    usbd_dev_destroy(p_vp->p_dc_dev);
__failed1:
    if (p_vp->p_buf) {
        usb_lib_mfree(&__g_usb_device_lib.lib, p_vp->p_buf);
    }
    if (p_vp->p_printer_id) {
        usb_lib_mfree(&__g_usb_device_lib.lib, p_vp->p_printer_id);
    }
#if USB_OS_EN
    if (p_vp->p_lock) {
        ret_tmp = usb_lib_mutex_destroy(&__g_usb_device_lib.lib, p_vp->p_lock);
        if (ret_tmp != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret_tmp);
        }
    }
#endif
    usb_lib_mfree(&__g_usb_device_lib.lib, p_vp);

    return ret;
}

/**
 * \brief 销毁 USB 虚拟打印机
 *
 * \param[in] p_vp USB 虚拟打印机设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_virtual_printer_destroy(struct usbd_vp *p_vp){
    int ret;

    if (p_vp == NULL) {
        return USB_OK;
    }

    ret = usbd_dev_destroy(p_vp->p_dc_dev);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device \"%s\" destroy failed(%d)\r\n", p_vp->p_dc_dev->name, ret);
        return ret;
    }
    if (p_vp->p_buf) {
        usb_lib_mfree(&__g_usb_device_lib.lib, p_vp->p_buf);
    }
    if (p_vp->p_printer_id) {
        usb_lib_mfree(&__g_usb_device_lib.lib, p_vp->p_printer_id);
    }
#if USB_OS_EN
    if (p_vp->p_lock) {
        ret = usb_lib_mutex_destroy(&__g_usb_device_lib.lib, p_vp->p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
            return ret;
        }
    }
#endif
    usb_lib_mfree(&__g_usb_device_lib.lib, p_vp);

    return USB_OK;
}

/**
 * \brief 检查 USB 虚拟打印机设备是否设置
 *
 * \param[in] p_vp USB 虚拟打印机设备
 *
 * \retval 设置返回 USB_TRUE，否则返回 USB_FALSE
 */
usb_bool_t usbd_virtual_printer_is_setup(struct usbd_vp *p_vp){
    if (p_vp == NULL) {
        return USB_FALSE;
    }
    return p_vp->is_setup;
}

/**
 * \brief 设置 USB 虚拟打印机字符串 ID
 *
 * \param[in] p_vp USB 虚拟打印机设备
 * \param[in] p_id 要设置的字符串 ID
 *
 * \retval 成功返回 USB_OK
 */
int usbd_virtual_printer_id_set(struct usbd_vp *p_vp, char *p_id){
    int ret = USB_OK;

    if ((p_vp == NULL) || (p_id == NULL)) {
        return -USB_EINVAL;
    }
    if (strlen(p_id) > USB_VP_PNP_STRING_LEN - 1) {
        return -USB_EILLEGAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_vp->p_lock, USBD_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    strcpy(p_vp->p_printer_id, p_id);
#if USB_OS_EN
    ret = usb_mutex_unlock(p_vp->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 虚拟打印机设备写数据
 *
 * \param[in]  p_vp      USB 虚拟打印机设备
 * \param[in]  p_buf     要发送的数据缓存
 * \param[in]  len       数据长度
 * \param[in]  timeout   超时时间
 * \param[out] p_act_len 返回实际发送的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbd_virtual_printer_write(struct usbd_vp  *p_vp,
                               uint8_t         *p_buf,
                               uint32_t         len,
                               uint32_t         timeout,
                               uint32_t        *p_act_len){

    /* 检查参数合法性*/
    if (p_vp == NULL) {
        return -USB_EINVAL;
    }

    return usbd_dev_trans_sync(p_vp->p_dc_dev,
                               p_vp->p_data_in,
                               p_buf,
                               len,
                               0,
                               timeout,
                               p_act_len);
}

/**
 * \brief USB 虚拟打印机设备同步读数据
 *
 * \param[in]  p_vp      USB 虚拟打印机设备
 * \param[in]  p_buf     要读取的数据缓存
 * \param[in]  len       数据长度
 * \param[in]  timeout   超时时间
 * \param[out] p_act_len 返回读到的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbd_virtual_printer_read_sync(struct usbd_vp  *p_vp,
                                   uint8_t         *p_buf,
                                   uint32_t         len,
                                   int              timeout,
                                   uint32_t        *p_act_len){

    /* 检查参数合法性*/
    if (p_vp == NULL) {
        return -USB_EINVAL;
    }

    return usbd_dev_trans_sync(p_vp->p_dc_dev,
                               p_vp->p_data_out,
                               p_buf,
                               len,
                               0,
                               timeout,
                               p_act_len);
}




