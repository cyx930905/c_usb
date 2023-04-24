/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/cdc/serial/chip/usbh_cdc_serial_ch341.h"

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief USB 转串口 ch341 控制设置
 */
static int __control_out(struct usbh_serial_ch341 *p_ch341,
                         uint8_t                   request,
                         uint16_t                  value,
                         uint16_t                  index){
    return usbh_ctrl_trp_sync_xfer(p_ch341->p_cmd_ep,
                                  (USB_REQ_TYPE_VENDOR | USB_REQ_TAG_DEVICE | USB_DIR_OUT),
                                   request,
                                   value,
                                   index,
                                   0,
                                   NULL,
                                   3000,
                                   0);
}

/**
 * \brief USB 转串口 ch341 读控制
 */
static int __control_in(struct usbh_serial_ch341 *p_ch341,
                        uint8_t                   request,
                        uint16_t                  value,
                        uint16_t                  index,
                        char                     *p_buf,
                        uint32_t                  buf_size){
    return usbh_ctrl_trp_sync_xfer(p_ch341->p_cmd_ep,
                                  (USB_REQ_TYPE_VENDOR | USB_REQ_TAG_DEVICE | USB_DIR_IN),
                                   request,
                                   value,
                                   index,
                                   buf_size,
                                   p_buf,
                                   3000,
                                   0);
}

/**
 * \brief USB 转串口 ch341 握手设置
 */
static int __handshake_set(struct usbh_serial_ch341 *p_ch341,
                           uint8_t                   control){
    int ret = __control_out(p_ch341, 0xa4, ~control, 0);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB host serial handshake set failed(%d)\r\n",  ret);
    }
    return ret;
}

/**
 * \brief USB 转串口 ch341 设置波特率
 */
static int __baudrate_set(struct usbh_serial_ch341 *p_ch341,
                          uint32_t                  baudrate){
    short    a, b;
    int      ret;
    uint32_t factor;
    short    divisor;

    factor = (CH341_BAUDBASE_FACTOR / baudrate);
    divisor = CH341_BAUDBASE_DIVMAX;

    while ((factor > 0xfff0) && divisor) {
        factor >>= 3;
        divisor--;
    }

    if (factor > 0xfff0) {
        return -USB_EILLEGAL;
    }
    factor = 0x10000 - factor;
    a = (factor & 0xff00) | divisor;
    b = factor & 0xff;

    ret = __control_out(p_ch341, 0x9a, 0x1312, a);
    if (ret != USB_OK) {
        __control_out(p_ch341, 0x9a, 0x0f2c, b);
    }
    return ret;

}

/**
 * \brief USB 转串口 ch341 获取状态
 */
static int __status_get(struct usbh_serial_ch341 *p_ch341){
    char           buf[8] = {0};
    int            len;

    len = __control_in(p_ch341, 0x95, 0x0706, 0, buf, 8);
    if (len < 0){
        return len;
    }
    /* 如果没问题则设置私有状态*/
    if (len == 2) {
    	p_ch341->line_status = (~(*buf)) & CH341_BITS_MODEM_STAT;

        return USB_OK;
    }
    return -USB_EPROTO;
}

/**
 * \brief USB 转串口 ch34x 配置函数
 */
static int __ch341_cfg(struct usbh_serial_ch341 *p_ch341){
    char           buf[8] = {0};
    int            ret;

    /* 需要两个字节 0x27 0x00 */
    ret = __control_in(p_ch341, 0x5f, 0, 0, buf, 8);
    if (ret < 0) {
        return ret;
    }

    ret = __control_out(p_ch341, 0xa1, 0, 0);
    if (ret < 0) {
        return ret;
    }

    ret = __baudrate_set(p_ch341, 115200);
    if (ret < 0) {
        return ret;
    }

    /* 需要两个字节 0x56 0x00 */
    ret = __control_in(p_ch341, 0x95, 0x2518, 0, buf, 8);
    if (ret < 0) {
        return ret;
    }

    ret = __control_out(p_ch341, 0x9a, 0x2518, 0x0050);
    if (ret < 0) {
        return ret;
    }

    /* 需要 0xff 0xee */
    ret = __status_get(p_ch341);
    if (ret < 0) {
        return ret;
    }

    ret = __control_out(p_ch341, 0xa1, 0x501f, 0xd90a);
    if (ret < 0) {
        return ret;
    }

    ret = __baudrate_set(p_ch341, 115200);
    if (ret < 0) {
        return ret;
    }

    ret = __handshake_set(p_ch341, p_ch341->line_control);
    if (ret < 0) {
        return ret;
    }

    /* 需要 0x9f 0xee */
    return __status_get(p_ch341);
}

/**
 * \brief USB 转串口 ch341 串口终端设置函数
 */
static int __ch341_termios_set(struct usbh_serial_port    *p_port,
                               struct usb_serial_port_cfg *p_cfg){
    int                       ret;
    uint8_t                   control;
    struct usbh_serial       *p_userial = p_port->p_userial;
    struct usbh_serial_ch341 *p_ch341   = (struct usbh_serial_ch341 *)USBH_SERIAL_DRV_HANDLE_GET(p_userial);

    /* 数据位，停止位，校验方式，ixoff，crtscts 不能设置*/
    if ((p_cfg->byte_size != p_port->cfg.byte_size) ||
            (p_cfg->stop_bits != p_port->cfg.stop_bits) ||
            (p_cfg->parity != p_port->cfg.parity) ||
            (p_cfg->ixoff != p_port->cfg.ixoff) ||
            (p_cfg->crtscts != p_port->cfg.crtscts)) {
        return -USB_EILLEGAL;
    }
    /* 修改波特率*/
    if (p_cfg->baud_rate != p_port->cfg.baud_rate) {

        control = p_ch341->line_control;
        control |= (CH341_BIT_DTR | CH341_BIT_RTS);

        ret = __baudrate_set(p_ch341, p_cfg->baud_rate);
        if(ret != USB_OK){
            __USB_ERR_INFO("USB host serial baudrate set failed(%d)\r\n",  ret);
            return ret;
        }
        ret = __handshake_set(p_ch341, p_ch341->line_control);
        if (ret != USB_OK) {
            return ret;
        }
        p_ch341->line_control = control;
        p_port->cfg.baud_rate = p_cfg->baud_rate;
    }
    /* 修改 DTR */
    if (p_cfg->dtr != p_port->cfg.dtr) {
        control = p_ch341->line_control;
        if (p_cfg->dtr == 1) {
            control |= CH341_BIT_DTR;
        } else {
            control &= ~CH341_BIT_DTR;
        }
        ret = __handshake_set(p_ch341, p_ch341->line_control);
        if (ret != USB_OK) {
            return ret;
        }
        p_ch341->line_control = control;
        p_port->cfg.dtr       = p_cfg->dtr;
    }
    /* 修改 RTS */
    if (p_cfg->rts != p_port->cfg.rts) {
        control = p_ch341->line_control;
        if (p_cfg->rts == 1) {
            control |= CH341_BIT_RTS;
        } else {
            control &= ~CH341_BIT_RTS;
        }
        ret = __handshake_set(p_ch341, p_ch341->line_control);
        if (ret != USB_OK) {
            return ret;
        }
        p_ch341->line_control = control;
        p_port->cfg.rts       = p_cfg->rts;
    }
    return USB_OK;
}

/**
 * \brief USB 转串口 ch341 同步写函数
 *
 * \param[in] p_userial  USB 转串口设备
 * \param[in] p_buf      要写的缓存
 * \param[in] len        要写的数据长度
 *
 * \retval 成功返回写的数据长度
 */
static int __ch341_write(struct usbh_serial_port *p_port,
                         uint8_t                 *p_buf,
                         uint32_t                 buf_len,
                         uint32_t                *p_act_len){
    int                       ret, tmp;
    uint8_t                  *p_buf_tmp = p_buf;
    uint32_t                  len_tmp   = buf_len;
    struct usbh_serial_ch341 *p_ch341   = USBH_SERIAL_DRV_HANDLE_GET(p_port->p_userial);

    while (len_tmp) {
        /* 调整数据发送大小*/
        tmp = min(len_tmp, p_port->tx_pipe.pipe.buf_size);
        memcpy(p_port->tx_pipe.pipe.p_buf, p_buf_tmp, tmp);
        /* 提交传输请求包*/
        ret = usbh_trp_sync_xfer(p_ch341->p_data_ep_wr,
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
static int __read_trp_init(struct usbh_serial_ch341 *p_ch341){
    int i, ret;

    for (i = 0; i < CH341_N_TRP; i++) {
        p_ch341->p_rd_buf[i] = usbh_serial_mem_alloc(p_ch341->rd_buf_size);
        if (p_ch341->p_rd_buf[i] == NULL) {
            return -USB_ENOMEM;
        }
        memset(p_ch341->p_rd_buf[i], 0, p_ch341->rd_buf_size);

        p_ch341->trp_rd[i].p_ep       = p_ch341->p_data_ep_rd;
        p_ch341->trp_rd[i].p_ctrl     = NULL;
        p_ch341->trp_rd[i].p_data     = p_ch341->p_rd_buf[i];
        p_ch341->trp_rd[i].len        = p_ch341->rd_buf_size;
        p_ch341->trp_rd[i].p_fn_done  = __read_bulk_cb;
        p_ch341->trp_rd[i].p_arg      = (void *)&p_ch341->trp_rd[i];
        p_ch341->trp_rd[i].act_len    = 0;
        p_ch341->trp_rd[i].status     = -USB_EINPROGRESS;
        p_ch341->trp_rd[i].flag       = 0;
        p_ch341->trp_rd[i].p_usr_priv = p_ch341->p_userial;

        /* 提交传输请求包*/
        ret = usbh_trp_submit(&p_ch341->trp_rd[i]);
        if (ret != USB_OK) {
            return ret;
        }
    }
    return USB_OK;
}

/**
 * \brief 读请求包反初始化
 */
static int __read_trp_deinit(struct usbh_serial_ch341 *p_ch341){
    int i, ret;

    for (i = 0; i < CH341_N_TRP; i++) {
        ret = usbh_trp_xfer_cancel(&p_ch341->trp_rd[i]);
        if (ret != USB_OK) {
            return ret;
        }
        if (p_ch341->p_rd_buf[i]) {
            usbh_serial_mem_free(p_ch341->p_rd_buf[i]);
        }
    }
    return USB_OK;
}

/* \brief 操作函数集 */
static struct usbh_serial_port_opts __g_ch341_opts = {
    .p_fn_port_cfg_set = __ch341_termios_set,
    .p_fn_port_write   = __ch341_write,
};

/**
 * \brief USB 转串口 ch341 芯片初始化
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_ch341_init(struct usbh_serial *p_userial){
    int                       ret;
    struct usbh_interface    *p_intf    = NULL;
    struct usbh_function     *p_usb_fun = p_userial->p_usb_fun;
    struct usbh_serial_ch341 *p_ch341   = NULL;
    uint8_t                   i, n_eps;

    /* 获取 USB 功能设备的第一个接口*/
    p_intf = usbh_func_intf_get(p_usb_fun, p_usb_fun->first_intf_num, 0);
    if (p_intf == NULL) {
        return -USB_ENODEV;
    }

    n_eps = USBH_INTF_NEP_GET(p_intf);
    if (n_eps < 3) {
        return -USB_EILLEGAL;
    }

    p_ch341 = usbh_serial_mem_alloc(sizeof(struct usbh_serial_ch341));
    if (p_ch341 == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_ch341, 0, sizeof(struct usbh_serial_ch341));

    for (i = 0; i < n_eps; i++) {
        if (USBH_EP_TYPE_GET(&p_intf->p_eps[i]) == USB_EP_TYPE_BULK) {
            if (USBH_EP_DIR_GET(&p_intf->p_eps[i]) == USB_DIR_IN) {
                p_ch341->p_data_ep_rd = &p_intf->p_eps[i];
            } else {
                p_ch341->p_data_ep_wr = &p_intf->p_eps[i];
            }
        }
    }
    if ((p_ch341->p_data_ep_wr == NULL) || (p_ch341->p_data_ep_rd == NULL)) {
        ret = -USB_EILLEGAL;
        goto __failed;
    }
    p_ch341->p_cmd_ep    = &p_usb_fun->p_usb_dev->ep0;
    p_ch341->rd_buf_size = USBH_EP_MPS_GET(p_ch341->p_data_ep_rd);
    p_ch341->p_userial   = p_userial;

    /* 设置 DTR 和 RTS */
    p_ch341->line_control |= (CH341_BIT_DTR | CH341_BIT_RTS);

    ret = usbh_serial_ports_create(p_userial, 1);
    if (ret != USB_OK) {
        goto __failed;
    }

    ret = usbh_serial_port_init(p_userial,
                                0,
                                p_ch341->p_data_ep_wr,
                                2048,
                                1000,
                                p_ch341->p_data_ep_rd,
                                2048);
    if (ret != USB_OK) {
        goto __failed;
    }

    /* 设置 USB 转串口结构体私有数据*/
    USBH_SERIAL_DRV_HANDLE_SET(p_userial, p_ch341);

    /* 设置 USB 串口回调函数*/
    usbh_serial_port_opts_set(p_userial, &__g_ch341_opts);

    p_userial->p_ports->cfg.byte_size = 8;
    p_userial->p_ports->cfg.stop_bits = USB_SERIAL_ONE_STOPBIT;
    p_userial->p_ports->cfg.parity    = USB_SERIAL_NONE_PARITY;
#if 0
    /* 打开 DTR/RTS */
    p_userial->p_ports->cfg.dtr       = 1;
    p_userial->p_ports->cfg.rts       = 1;
#endif

    /* 初始串口配置(波特率115200，8位数据位，1位停止位，无校验，无控制流)*/
    /* ch341 无法设置数据位，停止位和校验*/
    ret = __ch341_cfg(p_ch341);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB host serial config failed(%d)\r\n", ret);
        goto __failed;
    }

    /* 初始化数据读请求包*/
    ret = __read_trp_init(p_ch341);
    if (ret != USB_OK) {
        goto __failed;
    }

    return USB_OK;
__failed:
    usbh_serial_ch341_deinit(p_userial);
    return ret;
}

/**
 * \brief USB 转串口 ch341 芯片反初始化函数
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_ch341_deinit(struct usbh_serial *p_userial){
    int                       ret;
    struct usbh_serial_ch341 *p_ch341 =
            (struct usbh_serial_ch341 *)USBH_SERIAL_DRV_HANDLE_GET(p_userial);

    if (p_ch341 == NULL) {
        return -USB_EINVAL;
    }
    ret = __read_trp_deinit(p_ch341);
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

    usbh_serial_mem_free(p_ch341);

    USBH_SERIAL_DRV_HANDLE_SET(p_userial, NULL);

    return USB_OK;
}

