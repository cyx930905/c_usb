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
#include "core/include/device/class/cdc/virtual_serial/usbd_cdc_virtual_serial.h"

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbd_core_lib __g_usb_device_lib;

/*******************************************************************************
 * Static
 ******************************************************************************/
/* \brief 通讯设备类功能描述符定义  */
static uint8_t __g_cdc_priv_buf[] = {
    /** 标头功能  1个 */
    0x05,                                   /* 描述符长度*/
    USB_CDC_CS_INTERFACE,                   /* 描述符类型：接口*/
    USB_CDC_HEADER,                         /* 描述符子类型：头功能描述符*/
    USB_WBVAL(USB_CDC_V1_10),               /* 规范版本*/

    /** 功能专属描述字  多个 */
    /** Call Management Functional Descriptor */
    0x05,                                   /* 描述符长度*/
    USB_CDC_CS_INTERFACE,                   /* 描述符类型：接口*/
    USB_CDC_CALL_MANAGEMENT,                /* 描述符子类型： Call Management Func Desc */
    0x00,                                   /* bmCapabilities: device handles call management */
    0x02,                                   /* 数据接口: CDC 数据接口ID */
    /** Abstract Control Management Functional Descriptor */
    0x04,                                   /* 描述符长度*/
    USB_CDC_CS_INTERFACE,                   /* 描述符类型：接口*/
    USB_CDC_ABSTRACT_CONTROL_MANAGEMENT,    /* bDescriptorSubtype */
    0x02,                                   /* bmCapabilities */
    /** 联合功能描述符*/
    0x05,                                   /* 描述符长度*/
    USB_CDC_CS_INTERFACE,                   /* 描述符类型*/
    USB_CDC_UNION,                          /* 描述符子类型*/
    0,                                      /* 主接口 0 */
    0,                                      /* 从接口 1 */
};

/* \brief 联合接口功能信息定义 */
static struct usbd_func_assoc_info __g_vs_func_assoc_info = {
        .intf_num_first = 0,
        .intf_count     = 2,
        .class          = USB_CLASS_COMM,
        .sub_class      = USB_CDC_SUBCLASS_ACM,
        .protocol       = USB_CDC_PROTOCOL_V25TER,
        .p_intf_str     = "",
};

/* \brief 通讯接口功能信息定义 */
static struct usbd_func_info __g_vs_func_info[2] = {
        /* \brief 通讯接口功能信息定义 */
        {
                .intf_num        = 0,
                .alt_setting_num = 0,
                .class           = USB_CLASS_COMM,
                .sub_class       = USB_CDC_SUBCLASS_ACM,
                .protocol        = USB_CDC_PROTOCOL_V25TER,
                .p_intf_str      = "",
                .p_spec_desc     = __g_cdc_priv_buf,
                .spec_desc_size  = sizeof(__g_cdc_priv_buf),
        },
        /* \brief 数据接口功能信息定义 */
        {
                .intf_num        = 0,
                .alt_setting_num = 0,
                .class           = USB_CLASS_CDC_DATA,
                .sub_class       = 0x00,
                .protocol        = 0x00,
                .p_intf_str      = "",
                .p_spec_desc     = NULL,
                .spec_desc_size  = 0,
        }
};

/* \brief 通讯接口功能管道信息定义 */
static struct usbd_pipe_info __g_vs_cmd_pipe_info = {
        .num   = 0,
        .mps   = 0x08,
        .dir   = USB_DIR_IN,
        .attr  = USB_EP_TYPE_INT,
        .inter = 0xFF,
};

/* \brief 数据接口功能输入管道信息定义 */
static struct usbd_pipe_info __g_vs_data_pipe_in_info = {
        .num   = 0,
        .mps   = 512,
        .dir   = USB_DIR_IN,
        .attr  = USB_EP_TYPE_BULK,
        .inter = 0
};

/* \brief 数据接口功能输出管道信息定义 */
static struct usbd_pipe_info __g_vs_data_pipe_out_info = {
        .num   = 0,
        .mps   = 512,
        .dir   = USB_DIR_OUT,
        .attr  = USB_EP_TYPE_BULK,
        .inter = 0
};

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 设置 line coding 完成回调函数
 */
static void __cdc_line_coding_set_cb(void *p_arg){
    struct usbd_cdc_vs_port *p_vs_port = (struct usbd_cdc_vs_port *)p_arg;

    __USB_INFO("USB device virtual serial port %d cdc line coding set\r\n", p_vs_port->port_num);
    __USB_INFO("   baudrate    : %d\r\n", p_vs_port->line_control.dte_rate);
    __USB_INFO("   char format : %d\r\n", p_vs_port->line_control.char_format);
    __USB_INFO("   data bits   : %d\r\n", p_vs_port->line_control.data_bits);
    __USB_INFO("   parity type : %d\r\n", p_vs_port->line_control.parity_type);
}

/**
 * \brief 设置 line coding
 */
static int __cdc_line_coding_set(struct usbd_func        *p_func,
                                 struct usb_ctrlreq      *p_setup,
                                 struct usbd_cdc_vs_port *p_vs_port){
    uint16_t size;

    size = (p_setup->length > sizeof(struct usb_cdc_line_coding)) ? sizeof(struct usb_cdc_line_coding) : p_setup->length;

    return usb_dc_ep0_req(USB_DC_GET_FROM_FUNC(p_func),
                          USB_DIR_OUT,
                         (uint8_t *)&p_vs_port->line_control,
                          size,
                        __cdc_line_coding_set_cb,
                          p_vs_port);
}

/**
 * \brief 获取 line coding
 */
static int __cdc_line_coding_get(struct usbd_func        *p_func,
                                 struct usb_ctrlreq      *p_setup,
                                 struct usbd_cdc_vs_port *p_vs_port){
    uint16_t size;

    __USB_INFO("USB device virtual serial port %d cdc line coding get\r\n", p_vs_port->port_num);

    size = (p_setup->length > sizeof(struct usb_cdc_line_coding)) ? sizeof(struct usb_cdc_line_coding) : p_setup->length;

    return usb_dc_ep0_req(USB_DC_GET_FROM_FUNC(p_func),
                          USB_DIR_IN,
                         (uint8_t *)&p_vs_port->line_control,
                          size,
                          NULL,
                          NULL);
}

/**
 * \brief USB 虚拟串口命令设置函数
 */
static int __cdc_vs_cmd_setup(struct usbd_func   *p_func,
                              struct usb_ctrlreq *p_setup,
                              void               *p_arg){
    struct usbd_cdc_vs_port *p_vs_port = (struct usbd_cdc_vs_port *)p_arg;

    if ((USBD_FUNC_INTF_NUM_GET(p_func) == p_vs_port->cmd_intf_num) &&
            (USBD_FUNC_INTF_ALT_NUM_GET(p_func) == 0)) {
        switch(p_setup->request) {
            case USB_CDC_SEND_ENCAPSULATED_COMMAND:
                break;
            case USB_CDC_GET_ENCAPSULATED_RESPONSE:
                break;
            case USB_CDC_SET_COMM_FEATURE:
                break;
            case USB_CDC_GET_COMM_FEATURE:
                break;
            case USB_CDC_CLEAR_COMM_FEATURE:
                break;
            case USB_CDC_SET_LINE_CODING:
                return __cdc_line_coding_set(p_func, p_setup, p_vs_port);
            case USB_CDC_GET_LINE_CODING:
                return __cdc_line_coding_get(p_func, p_setup, p_vs_port);
            case USB_CDC_SET_CONTROL_LINE_STATE:
                //p_vs->is_connected = (p_setup->value & 0x01) > 0 ? USB_TRUE : USB_FALSE;
                break;
            case USB_CDC_SEND_BREAK:
                break;
            default:
                __USB_ERR_INFO("cdc request %d unknown\r\n", p_setup->request_type);
                return -USB_ENOTSUP;
        }
    }

    return USB_OK;
}

/**
 * \brief 关闭接口功能
 */
static int __cdc_vs_shutdown(struct usbd_func *p_func, void *p_arg){
    struct usbd_cdc_vs_port *p_vs_port = (struct usbd_cdc_vs_port *)p_arg;

    p_vs_port->is_setup = USB_FALSE;

    return USB_OK;
}

/**
 * \brief USB 从机虚拟串口数据接口功能设置函数
 */
static int __cdc_vs_data_func_set(struct usbd_func *p_func, void *p_arg){
    int                      ret;
    struct usbd_cdc_vs_port *p_vs_port = (struct usbd_cdc_vs_port *)p_arg;;

    if ((USBD_FUNC_INTF_NUM_GET(p_func) == p_vs_port->data_intf_num) &&
            (USBD_FUNC_INTF_ALT_NUM_GET(p_func) == 0)) {

        ret = usbd_dev_pipe_get(USBD_DEV_GET_FROM_FUNC(p_func),
                                p_vs_port->data_out_ep_addr & 0x0F,
                                USB_DIR_OUT,
                               &p_vs_port->p_data_out);
        if (ret != USB_OK) {
            return ret;
        }

        ret =  usbd_dev_pipe_get(USBD_DEV_GET_FROM_FUNC(p_func),
                                 p_vs_port->data_in_ep_addr & 0x0F,
                                 USB_DIR_IN,
                                &p_vs_port->p_data_in);
        if (ret != USB_OK) {
            return ret;
        }
        p_vs_port->is_setup = USB_TRUE;
    }
    return USB_OK;
}

/* \brief USB 设备虚拟串口接口功能操作函数  */
static struct usbd_func_opts __g_vs_func_opts = {
    __cdc_vs_shutdown,
    __cdc_vs_cmd_setup,
    __cdc_vs_data_func_set,
    NULL,
    NULL,
    NULL
};

static void __vs_trans_done(void *p_arg){
    uint32_t                 len_act;
    struct usbd_cdc_vs_port *p_port = (struct usbd_cdc_vs_port *)p_arg;

    usb_lib_rb_put(p_port->p_rb, p_port->trans.p_buf, p_port->trans.act_len, &len_act);

    usbd_dev_trans_async(p_port->p_vs->p_dc_dev, &p_port->trans);
}

/**
 * \brief USB 从机虚拟串口设备创建
 *
 * \param[in]  p_dc     所属的 USB 从机控制器
 * \param[in]  p_name   设备名字
 * \param[in]  dev_desc 设备描述符
 * \param[out] p_vs_ret 返回创建成功的 USB 虚拟串口结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbd_cdc_vs_create(struct usb_dc        *p_dc,
                       char                 *p_name,
                       struct usbd_dev_desc  dev_desc,
                       uint8_t               n_ports,
                       struct usbd_cdc_vs  **p_vs_ret){
    int                 ret;
    struct usbd_cdc_vs *p_vs          = NULL;
    uint8_t             i, intf_num   = 0;
    uint32_t            priv_buf_size = 0;

    if ((p_name == NULL) || (p_dc == NULL)) {
        return -USB_EINVAL;
    }
    if (n_ports > 3) {
        return -USB_EILLEGAL;
    }

    priv_buf_size = sizeof(__g_cdc_priv_buf);

    p_vs = usb_lib_malloc(&__g_usb_device_lib.lib, sizeof(struct usbd_cdc_vs));
    if (p_vs == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_vs, 0, sizeof(struct usbd_cdc_vs));

    p_vs->p_port = usb_lib_malloc(&__g_usb_device_lib.lib, n_ports * sizeof(struct usbd_cdc_vs_port));
    if (p_vs->p_port == NULL) {
        ret = -USB_ENOMEM;
        goto __failed1;
    }
    memset(p_vs->p_port, 0, n_ports * sizeof(struct usbd_cdc_vs_port));

    /* 创建一个 USB 从机设备*/
    ret = usbd_dev_create(p_dc, p_name, dev_desc, &p_vs->p_dc_dev);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB device virtual serial create failed(%d)\r\n", ret);
        goto __failed1;
    }

    /* 添加配置*/
    ret = usbd_dev_cfg_add(p_vs->p_dc_dev, NULL, &p_vs->cfg_num);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB device virtual serial config add failed(%d)\r\n", ret);
        goto __failed2;
    }

    for (i = 0; i < n_ports; i++) {
        p_vs->p_port[i].port_num                 = i;
        p_vs->p_port[i].cmd_intf_num             = intf_num;
        p_vs->p_port[i].data_intf_num            = intf_num + 1;
        p_vs->p_port[i].line_control.dte_rate    = 9600;
        p_vs->p_port[i].line_control.char_format = 0;
        p_vs->p_port[i].line_control.data_bits   = 8;
        p_vs->p_port[i].line_control.parity_type = 0;
        p_vs->p_port[i].p_vs                     = p_vs;

        p_vs->p_port[i].p_rb = usb_lib_rb_create(&__g_usb_device_lib.lib, USBD_VS_RB_SIZE);
        if (p_vs->p_port[i].p_rb == NULL) {
            goto __failed2;
        }

        __g_cdc_priv_buf[priv_buf_size - 2]   = intf_num;
        __g_cdc_priv_buf[priv_buf_size - 1]   = intf_num + 1;
        __g_vs_func_assoc_info.intf_num_first = intf_num;
        __g_vs_func_info[0].intf_num          = intf_num;
        __g_vs_func_info[1].intf_num          = intf_num + 1;

        /* 创建联合接口*/
        ret = usbd_dev_func_assoc_add(p_vs->p_dc_dev,
                                      p_vs->cfg_num,
                                    __g_vs_func_assoc_info,
                                    __g_vs_func_info,
                                   &__g_vs_func_opts,
                                     &p_vs->p_port[i]);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB device virtual serial port %d association function add failed(%d)\r\n", i, ret);
            goto __failed2;
        }

        /* 添加命令中断管道 */
        ret = usbd_dev_pipe_add(p_vs->p_dc_dev,
                                p_vs->cfg_num,
                                intf_num,
                                0,
                              __g_vs_cmd_pipe_info,
                                NULL);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB device virtual serial pipe add failed(%d)\r\n", ret);
            goto __failed2;
        }

        /* 添加数据输入管道 */
        ret = usbd_dev_pipe_add(p_vs->p_dc_dev,
                                p_vs->cfg_num,
                                intf_num + 1,
                                0,
                              __g_vs_data_pipe_in_info,
                               &p_vs->p_port[i].data_in_ep_addr);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB device virtual serial pipe add failed(%d)\r\n", ret);
            goto __failed2;
        }
        /* 添加数据输出管道 */
        ret = usbd_dev_pipe_add(p_vs->p_dc_dev,
                                p_vs->cfg_num,
                                intf_num + 1,
                                0,
                              __g_vs_data_pipe_out_info,
                               &p_vs->p_port[i].data_out_ep_addr);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB device virtual serial pipe add failed(%d)\r\n", ret);
            goto __failed2;
        }

        p_vs->n_ports++;
        intf_num += 2;
    }

    *p_vs_ret = p_vs;

    return USB_OK;
__failed2:
    usbd_dev_destroy(p_vs->p_dc_dev);
__failed1:
    if (p_vs->p_port) {
        for (i = 0; i < p_vs->n_ports; i++) {
            if (p_vs->p_port[i].p_rb) {
                usb_lib_rb_destroy(&__g_usb_device_lib.lib, p_vs->p_port[i].p_rb);
            }
        }
        usb_lib_mfree(&__g_usb_device_lib.lib, p_vs->p_port);
    }
    usb_lib_mfree(&__g_usb_device_lib.lib, p_vs);

    return ret;
}

/**
 * \brief 销毁 USB 虚拟串口
 *
 * \param[in] p_vs USB 虚拟串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_cdc_vs_destroy(struct usbd_cdc_vs *p_vs){
    int ret, i;

    if (p_vs == NULL) {
        return USB_OK;
    }

    ret = usbd_dev_destroy(p_vs->p_dc_dev);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB device \"%s\" destroy failed(%d)\r\n", p_vs->p_dc_dev->name, ret);
        return ret;
    }
    if (p_vs->p_port) {
        for (i = 0; i < p_vs->n_ports; i++) {
            if (p_vs->p_port[i].p_rb) {
                usb_lib_rb_destroy(&__g_usb_device_lib.lib, p_vs->p_port[i].p_rb);
            }
        }
        usb_lib_mfree(&__g_usb_device_lib.lib, p_vs->p_port);
    }
    usb_lib_mfree(&__g_usb_device_lib.lib, p_vs);

    return USB_OK;
}

/**
 * \brief 检查 USB 从机虚拟串口端口是否设置
 *
 * \param[in] p_vs     USB 从机虚拟串口设备
 * \param[in] port_num 端口号
 *
 * \retval 设置返回 USB_TRUE，否则返回 USB_FALSE
 */
usb_bool_t usbd_cdc_vs_port_is_setup(struct usbd_cdc_vs *p_vs, uint8_t port_num){
    if (p_vs == NULL) {
        return USB_FALSE;
    }
    if (port_num > (p_vs->n_ports - 1)) {
        return USB_FALSE;
    }
    return p_vs->p_port[port_num].is_setup;
}

/**
 * \brief USB 从机虚拟串口设备端口启动
 *
 * \param[in]  p_vs      USB 从机虚拟串口设备
 * \param[in]  port_num  端口号
 *
 * \retval 成功返回 USB_OK
 */
int usbd_cdc_vs_port_start(struct usbd_cdc_vs *p_vs, uint8_t port_num){
    struct usbd_cdc_vs_port *p_port = NULL;
    int                      ret;

    if (p_vs == NULL) {
        return -USB_EINVAL;
    }
    if (port_num > (p_vs->n_ports - 1)) {
        return -USB_EILLEGAL;
    }
    p_port = &p_vs->p_port[port_num];

    if (p_port->is_start == USB_TRUE) {
        return -USB_EPERM;
    }

    memset(&p_port->trans, 0, sizeof(struct usbd_trans));

    p_port->trans.p_buf = usb_lib_malloc(&__g_usb_device_lib.lib, 512);
    if (p_port->trans.p_buf == NULL) {
        return -USB_ENOMEM;
    }

    p_port->trans.p_hw          = p_vs->p_port[port_num].p_data_out->p_hw;
    p_port->trans.len           = 512;
    p_port->trans.flag          = 0;
    p_port->trans.p_fn_complete = __vs_trans_done;
    p_port->trans.p_arg         = p_port;
    p_port->trans.act_len       = 0;
    p_port->trans.status        = 0;

    ret = usbd_dev_trans_async(p_vs->p_dc_dev, &p_port->trans);
    if (ret == USB_OK) {
        p_port->is_start = USB_TRUE;
    }
    return ret;
}

/**
 * \brief USB 从机虚拟串口设备端口写数据
 *
 * \param[in]  p_vs      USB 从机虚拟串口设备
 * \param[in]  port_num  端口号
 * \param[in]  p_buf     要发送的数据缓存
 * \param[in]  len       数据长度
 * \param[in]  timeout   超时时间
 * \param[out] p_act_len 返回实际发送的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbd_cdc_vs_port_write(struct usbd_cdc_vs  *p_vs,
                           uint8_t              port_num,
                           uint8_t             *p_buf,
                           uint32_t             len,
                           int                  timeout,
                           uint32_t            *p_act_len){

    /* 检查参数合法性*/
    if (p_vs == NULL) {
        return -USB_EINVAL;
    }
    if (port_num > (p_vs->n_ports - 1)) {
        return -USB_EILLEGAL;
    }
    return usbd_dev_trans_sync(p_vs->p_dc_dev,
                               p_vs->p_port[port_num].p_data_in,
                               p_buf,
                               len,
                               0,
                               timeout,
                               p_act_len);
}

/**
 * \brief USB 从机虚拟串口设备端口同步读数据
 *
 * \param[in]  p_vs      USB 从机虚拟串口设备
 * \param[in]  port_num  端口号
 * \param[in]  p_buf     要读取的数据缓存
 * \param[in]  len       数据长度
 * \param[out] p_act_len 返回读到的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbd_cdc_vs_port_read(struct usbd_cdc_vs  *p_vs,
                          uint8_t              port_num,
                          uint8_t             *p_buf,
                          uint32_t             len,
                          uint32_t            *p_act_len){
    struct usbd_cdc_vs_port *p_port = NULL;

    if ((p_vs == NULL) || (p_act_len == NULL) || (p_buf == NULL)) {
        return -USB_EINVAL;
    }
    if (port_num > (p_vs->n_ports - 1)) {
        return -USB_EILLEGAL;
    }
    p_port = &p_vs->p_port[port_num];

    return usb_lib_rb_get(p_port->p_rb, p_buf, len, p_act_len);
}

///**
// * \brief USB 从机虚拟串口设备端口同步读数据
// *
// * \param[in]  p_vs      USB 从机虚拟串口设备
// * \param[in]  port_num  端口号
// * \param[in]  p_buf     要读取的数据缓存
// * \param[in]  len       数据长度
// * \param[in]  timeout   超时时间
// * \param[out] p_act_len 返回读到的数据长度
// *
// * \retval 成功返回 USB_OK
// */
//int usbd_cdc_vs_port_read_sync(struct usbd_cdc_vs  *p_vs,
//                               uint8_t              port_num,
//                               uint8_t             *p_buf,
//                               uint32_t             len,
//                               int                  timeout,
//                               uint32_t            *p_act_len){
//
//    /* 检查参数合法性*/
//    if (p_vs == NULL) {
//        return -USB_EINVAL;
//    }
//    if (port_num > (p_vs->n_ports - 1)) {
//        return -USB_EILLEGAL;
//    }
//    return usbd_dev_trans_sync(p_vs->p_dc_dev,
//                               p_vs->p_port[port_num].p_data_out,
//                               p_buf,
//                               len,
//                               0,
//                               timeout,
//                               p_act_len);
//}

///**
// * \brief USB 虚拟串口设备异步读数据
// *
// * \param[in]  p_vs      USB 虚拟串口设备
// * \param[in]  p_buf     要读取的数据缓存
// * \param[in]  len       数据长度
// * \param[out] p_act_len 返回读到的数据长度
// *
// * \retval 成功返回 USB_OK
// */
//int usbd_cdc_vs_port_read_async(struct usbd_cdc_vs  *p_vs,
//                                uint8_t              port_num,
//                                uint8_t             *p_buf,
//                                uint32_t             len,
//                                p_fn_trans_cb       *p_cb,
//                                void                *p_arg){
//    struct usbd_trans *p_trans = NULL;
//
//    /* 检查参数合法性*/
//    if (p_vs == NULL) {
//        return -USB_EINVAL;
//    }
//    if (port_num > (p_vs->n_ports - 1)) {
//        return -USB_EILLEGAL;
//    }
//    p_trans = usb_lib_malloc(&__g_usb_device_lib.lib, sizeof(struct usbd_trans));
//    if (p_trans == NULL) {
//        return -USB_ENOMEM;
//    }
//    memset(p_trans, 0, sizeof(struct usbd_trans));
//
//    p_trans->p_hw          = p_vs->p_port[port_num].p_data_out->p_hw;
//    p_trans->p_buf         = p_buf;
//    p_trans->len           = len;
//    p_trans->flag          = 0;
//    p_trans->p_fn_complete = p_fn_complete;
//    p_trans->p_arg         = p_arg;
//    p_trans->act_len       = 0;
//    p_trans->status        = 0;
//
//    return usbd_dev_trans_async(p_vs->p_dc_dev, p_trans);
//}
