/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/cdc/serial/chip/usbh_cdc_serial_common.h"

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern int usbh_serial_quirk_init(struct usbh_function *p_usb_fun);

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief USB 转串口通用终端设置函数
 */
static int __common_termios_set(struct usbh_serial_port    *p_port,
                                struct usb_serial_port_cfg *p_cfg){
    return USB_OK;
}

/**
 * \brief USB 转串口设备通用写函数
 */
static int __common_write(struct usbh_serial_port *p_port,
                          uint8_t                 *p_buf,
                          uint32_t                 buf_len,
                          uint32_t                *p_act_len){
    int                        ret, tmp;
    uint8_t                   *p_buf_tmp = p_buf;
    uint32_t                   len_tmp   = buf_len;
    struct usbh_serial_common *p_common  = USBH_SERIAL_DRV_HANDLE_GET(p_port->p_userial);

    while (len_tmp) {
        /* 调整数据发送大小*/
        tmp = min(len_tmp, p_port->tx_pipe.pipe.buf_size);
        memcpy(p_port->tx_pipe.pipe.p_buf, p_buf_tmp, tmp);
        /* 提交传输请求包*/
        ret = usbh_trp_sync_xfer(p_common->p_data_ep_wr,
                                 NULL,
                                 p_port->tx_pipe.pipe.p_buf,
                                 tmp,
                                 p_port->tx_pipe.time_out,
                                 0);
        if (ret < 0) {
            return ret;
        }
        len_tmp   -= ret;
        p_buf_tmp += ret;

        if (ret < tmp) {
            break;
        }
    }
    *p_act_len = buf_len - len_tmp;

    return 0;
}

/**
 * \brief 读请求完成回调函数
 */
static void __read_bulk_cb(void *p_arg){
    struct usbh_trp    *p_trp     = (struct usbh_trp *)p_arg;
    struct usbh_serial *p_userial = (struct usbh_serial *)p_trp->p_usr_priv;
    int                 ret;
    uint32_t            act_len, data_act_len;
    uint8_t            *p_buf     = NULL;

    if ((p_trp->status == -USB_ECANCEL) ||
            (p_userial->is_removed == USB_TRUE)) {
        return;
    }

    if (p_trp->act_len <= 0) {
        goto __exit;
    }
    act_len = p_trp->act_len;
    p_buf   = p_trp->p_data;

    usbh_serial_port_rx_buf_put(p_userial->p_ports, p_buf, act_len, &data_act_len);

__exit:
    ret = usbh_trp_submit(p_trp);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB host serial TRP submit failed(%d)\r\n", ret);
    }
    return;
}

/**
 * \brief 读请求包初始化
 */
static int __read_trp_init(struct usbh_serial_common *p_common){
    int i, ret;

    for (i = 0;i < COMMON_N_TRP; i++) {
        p_common->p_rd_buf[i] = usbh_serial_mem_alloc(p_common->rd_buf_size);
        if (p_common->p_rd_buf[i] == NULL) {
            return -USB_ENOMEM;
        }
        memset(p_common->p_rd_buf[i], 0, p_common->rd_buf_size);

        p_common->trp_rd[i].p_ep       = p_common->p_data_ep_rd;
        p_common->trp_rd[i].p_ctrl     = NULL;
        p_common->trp_rd[i].p_data     = p_common->p_rd_buf[i];
        p_common->trp_rd[i].len        = p_common->rd_buf_size;
        p_common->trp_rd[i].p_fn_done  = __read_bulk_cb;
        p_common->trp_rd[i].p_arg      = (void *)&p_common->trp_rd[i];
        p_common->trp_rd[i].act_len    = 0;
        p_common->trp_rd[i].status     = -USB_EINPROGRESS;
        p_common->trp_rd[i].flag       = 0;
        p_common->trp_rd[i].p_usr_priv = p_common->p_userial;

        /* 提交传输请求包*/
        ret = usbh_trp_submit(&p_common->trp_rd[i]);
        if (ret != USB_OK) {
            return ret;
        }
    }
    return USB_OK;
}

/**
 * \brief 读请求包反初始化
 */
static int __read_trp_deinit(struct usbh_serial_common *p_common){
    int i, ret;

    for (i = 0; i < COMMON_N_TRP; i++) {
        ret = usbh_trp_xfer_cancel(&p_common->trp_rd[i]);
        if (ret != USB_OK) {
            return ret;
        }
        if (p_common->p_rd_buf[i]) {
            usbh_serial_mem_free(p_common->p_rd_buf[i]);
        }
    }
    return USB_OK;
}

/* \brief 操作函数集 */
static struct usbh_serial_port_opts __g_common_opts = {
    .p_fn_port_cfg_set = __common_termios_set,
    .p_fn_port_write   = __common_write,
};

/**
 * \brief USB 转串口通用设备初始化
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_common_init(struct usbh_serial *p_userial){
    int                        ret       = -USB_ENODEV;
    struct usbh_interface     *p_intf    = NULL;
    struct usb_list_node      *p_node    = NULL;
    struct usbh_function      *p_usb_fun = p_userial->p_usb_fun;
    struct usbh_serial_common *p_common = NULL;
    uint8_t                    i, n_eps;

    /* 联合接口*/
    if (p_usb_fun->n_intf_type > 1) {
        usb_list_for_each_node(p_node, &p_usb_fun->intf_list){
            p_intf = usb_container_of(p_node, struct usbh_interface, node);

            if (USBH_INTF_CLASS_GET(p_intf) == USB_CLASS_CDC_DATA) {
                ret = usbh_func_intf_set(p_usb_fun, USBH_INTF_NUM_GET(p_intf), USBH_INTF_ALT_NUM_GET(p_intf));
                break;
            }
        }
        if (ret != USB_OK) {
            return ret;
        }
    } else {
        /* 获取 USB 功能设备的第一个接口*/
        p_intf = usbh_func_intf_get(p_usb_fun, p_usb_fun->first_intf_num, 0);
        if (p_intf == NULL) {
            return -USB_ENODEV;
        }
    }

    n_eps = USBH_INTF_NEP_GET(p_intf);
    if (n_eps < 2) {
        return -USB_EILLEGAL;
    }

    p_common = usbh_serial_mem_alloc(sizeof(struct usbh_serial_common));
    if (p_common == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_common, 0, sizeof(struct usbh_serial_common));
    p_common->p_userial   = p_userial;
    p_common->rd_buf_size = 512;

    for (i = 0; i < n_eps; i++) {
        if (USBH_EP_TYPE_GET(&p_intf->p_eps[i]) == USB_EP_TYPE_BULK) {
            if (USBH_EP_DIR_GET(&p_intf->p_eps[i]) == USB_DIR_IN) {
                p_common->p_data_ep_rd = &p_intf->p_eps[i];
            } else {
                p_common->p_data_ep_wr = &p_intf->p_eps[i];
            }
        }
    }
    if ((p_common->p_data_ep_wr == NULL) || (p_common->p_data_ep_rd == NULL)) {
        ret = -USB_EILLEGAL;
        goto __failed;
    }

    ret = usbh_serial_ports_create(p_userial, 1);
    if (ret != USB_OK) {
        goto __failed;
    }

    ret = usbh_serial_port_init(p_userial,
                                0,
                                p_common->p_data_ep_wr,
                                2048,
                                1000,
                                p_common->p_data_ep_rd,
                                2048);
    if (ret != USB_OK) {
        goto __failed;
    }

    /* 设置 USB 转串口结构体私有数据*/
    USBH_SERIAL_DRV_HANDLE_SET(p_userial, p_common);

    /* 设置 USB 串口回调函数*/
    usbh_serial_port_opts_set(p_userial, &__g_common_opts);

    ret = __read_trp_init(p_common);
    if (ret != USB_OK) {
        goto __failed;
    }

    /* 兼容初始化*/
    return usbh_serial_quirk_init(p_usb_fun);
__failed:
    usbh_serial_port_deinit(p_userial, 0);

    usbh_serial_ports_destroy(p_userial);

    usbh_serial_mem_free(p_common);

    USBH_SERIAL_DRV_HANDLE_SET(p_userial, NULL);

    return ret;
}

/**
 * \brief USB 转串口通用设备反初始化
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_common_deinit(struct usbh_serial *p_userial){
    int                        ret;
    struct usbh_serial_common *p_common =
            (struct usbh_serial_common *)USBH_SERIAL_DRV_HANDLE_GET(p_userial);

    if (p_common == NULL) {
        return -USB_EINVAL;
    }
    ret = __read_trp_deinit(p_common);
    if (ret != USB_OK) {
        return ret;
    }

    ret = usbh_serial_port_deinit(p_userial, 0);
    if (ret != USB_OK) {
        return ret;
    }
    ret = usbh_serial_ports_destroy(p_userial);
    if (ret != USB_OK) {
        return ret;
    }

    usbh_serial_mem_free(p_common);

    USBH_SERIAL_DRV_HANDLE_SET(p_userial, NULL);

    return USB_OK;
}

