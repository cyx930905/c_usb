/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/cdc/serial/chip/usbh_cdc_serial_pl2303.h"

/*******************************************************************************
 * Static
 ******************************************************************************/
/* \brief pl2303 芯片类型数据*/
static const struct pl2303_type_data __g_pl2303_type_data[TYPE_COUNT] = {
    [TYPE_01] = {
        .max_baud_rate = 1228800,
        .quirks        = PL2303_QUIRK_LEGACY,
    },
    [TYPE_HX] = {
        .max_baud_rate = 12000000,
    },
};

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief USB 转串口 pl2303 写函数
 */
static int __pl2303_vendor_write(struct usbh_serial *p_userial,
                                 uint16_t            value,
                                 uint16_t            idx){
    struct usbh_function *p_usb_fun = p_userial->p_usb_fun;

    return usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                    PL2303_VENDOR_WRITE_REQUEST_TYPE,
                                    PL2303_VENDOR_WRITE_REQUEST,
                                    value,
                                    idx,
                                    0,
                                    NULL,
                                    5000,
                                    0);
}

/**
 * \brief USB 转串口 pl2303 读函数
 */
static int __pl2303_vendor_read(struct usbh_serial *p_userial,
                                uint16_t            value,
                                uint8_t            *p_data){
    struct usbh_function *p_usb_fun = p_userial->p_usb_fun;

    return usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                    PL2303_VENDOR_READ_REQUEST_TYPE,
                                    PL2303_VENDOR_READ_REQUEST,
                                    value,
                                    0,
                                    1,
                                    p_data,
                                    5000,
                                    0);
}

/**
 * \brief USB 转串口 pl2303 获取配置信息
 */
static int __pl2303_get_line_request(struct usbh_serial *p_userial, uint8_t buf[7]){
    struct usbh_function *p_usb_fun = p_userial->p_usb_fun;

    return usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                    PL2303_GET_LINE_REQUEST_TYPE,
                                    PL2303_GET_LINE_REQUEST,
                                    0,
                                    0,
                                    7,
                                    buf,
                                    5000,
                                    0);
}

/**
 * \brief USB 转串口 pl2303 设置配置信息
 */
static int __pl2303_set_line_request(struct usbh_serial *p_userial, uint8_t buf[7]){
    struct usbh_function *p_usb_fun = p_userial->p_usb_fun;

    return usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                    PL2303_SET_LINE_REQUEST_TYPE,
                                    PL2303_SET_LINE_REQUEST,
                                    0,
                                    0,
                                    7,
                                    buf,
                                    5000,
                                    0);
}

/**
 * \brief USB 转串口 pl2303 设置控制信息
 */
static int __pl2303_control_lines_set(struct usbh_serial *p_userial, uint8_t value){
    struct usbh_function *p_usb_fun = p_userial->p_usb_fun;

    return usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                    PL2303_SET_CONTROL_REQUEST_TYPE,
                                    PL2303_SET_CONTROL_REQUEST,
                                    value,
                                    0,
                                    0,
                                    NULL,
                                    5000,
                                    0);
}

/**
 * \brief 返回最接近的支持的波特率，直接设置而不用使用因子
 */
static uint32_t __pl2303_supported_baudrate_get(uint32_t baud){
    static const uint32_t baud_sup[] = {
        75, 150, 300, 600, 1200, 1800, 2400, 3600, 4800, 7200, 9600,
        14400, 19200, 28800, 38400, 57600, 115200, 230400, 460800,
        614400, 921600, 1228800, 2457600, 3000000, 6000000
    };

    uint32_t i;

    for (i = 0; i < USB_NELEMENTS(baud_sup); ++i) {
        if (baud_sup[i] > baud) {
            break;
        }
    }

    if (i == USB_NELEMENTS(baud_sup)) {
        baud = baud_sup[i - 1];
    } else if (i > 0 && (baud_sup[i] - baud) > (baud - baud_sup[i - 1])) {
        baud = baud_sup[i - 1];
    } else {
        baud = baud_sup[i];
    }
    return baud;
}

/**
 * \brief 直接使用波特率
 */
static uint32_t __pl2303_baudrate_direct_encode(uint8_t buf[4], uint32_t baud){
    buf[0] = (uint8_t)(baud & 0x000000FF);
    buf[1] = (uint8_t)((baud >> 8) & 0x000000FF);
    buf[2] = (uint8_t)((baud >> 16) & 0x000000FF);
    buf[3] = (uint8_t)((baud >> 24) & 0x000000FF);
    return baud;
}

/**
 * \brief 解码波特率因子
 */
static uint32_t __pl2303_baudrate_divisor_encode(uint8_t buf[4], uint32_t baud){
    uint32_t baseline, mantissa, exponent;

    /*
     * 公式是:
     * 波特率 = 12M * 32 / (对数 * 4^指数)
     * 对数 = buf[8:0]
     * 指数 = buf[11:9]
     */
    baseline = 12000000 * 32;
    mantissa = baseline / baud;
    if (mantissa == 0) {
        /* 如果波特率大于 32*12M，避免除以 0 */
        mantissa = 1;
    }
    exponent = 0;
    while (mantissa >= 512) {
        if (exponent < 7) {
            /* 除以4 */
            mantissa >>= 2;
            exponent++;
        } else {
            /* 指数最大值. Trim mantissa and leave. */
            mantissa = 511;
            break;
        }
    }

    buf[3] = 0x80;
    buf[2] = 0;
    buf[1] = exponent << 1 | mantissa >> 8;
    buf[0] = mantissa & 0xff;

    /* 计算且返回确切的波特率*/
    baud = (baseline / mantissa) >> (exponent << 1);

    return baud;
}

/**
 * \brief USB 转串口 pl2303 波特率解码
 */
static void __pl2303_baudrate_encode(struct usbh_serial_pl2303 *p_pl2303,
                                     uint32_t                   baudrate,
                                     uint8_t                    buf[4]){
    struct pl2303_serial_private *p_spriv = &p_pl2303->pl2303_priv;
    uint32_t                      baud_sup;

    if (p_spriv->p_type->max_baud_rate) {
        baudrate = min(baudrate, p_spriv->p_type->max_baud_rate);
    }
    /* 使用直接的方法去支持波特率，不然使用因子*/
    baud_sup = __pl2303_supported_baudrate_get(baudrate);

    if (baudrate == baud_sup) {
        baudrate = __pl2303_baudrate_direct_encode(buf, baudrate);
    } else {
        baudrate = __pl2303_baudrate_divisor_encode(buf, baudrate);
    }
}

/**
 * \brief USB 转串口 pl2303 启动
 */
static int __pl2303_startup(struct usbh_serial_pl2303 *p_pl2303){
	struct usbh_serial *p_userial = p_pl2303->p_userial;
    enum pl2303_type    type      = TYPE_01;
    uint8_t             data;

    memset(&p_pl2303->pl2303_priv, 0, sizeof(struct pl2303_serial_private));

    if (USBH_SERIAL_DEV_CLASS_GET(p_userial) == 0x02) {
        type = TYPE_01;     /* 类型 0 */
    } else if (USBH_SERIAL_DEV_MPS0_GET(p_userial) == 0x40) {
        type = TYPE_HX;
    } else if (USBH_SERIAL_DEV_CLASS_GET(p_userial) == 0x00) {
        type = TYPE_01;     /* 类型 1 */
    } else if (USBH_SERIAL_DEV_CLASS_GET(p_userial) == 0xFF) {
        type = TYPE_01;     /* 类型 1 */
    }

    p_pl2303->pl2303_priv.p_type  = &__g_pl2303_type_data[type];
    p_pl2303->pl2303_priv.quirks |= __g_pl2303_type_data[type].quirks;

    __pl2303_vendor_read(p_userial, 0x8484, &data);
    __pl2303_vendor_write(p_userial, 0x0404, 0);
    __pl2303_vendor_read(p_userial, 0x8484, &data);
    __pl2303_vendor_read(p_userial, 0x8383, &data);
    __pl2303_vendor_read(p_userial, 0x8484, &data);
    __pl2303_vendor_write(p_userial, 0x0404, 1);
    __pl2303_vendor_read(p_userial, 0x8484, &data);
    __pl2303_vendor_read(p_userial, 0x8383, &data);
    __pl2303_vendor_write(p_userial, 0, 1);
    __pl2303_vendor_write(p_userial, 1, 0);

    if (p_pl2303->pl2303_priv.quirks & PL2303_QUIRK_LEGACY) {
        __pl2303_vendor_write(p_userial, 2, 0x24);
    } else {
        __pl2303_vendor_write(p_userial, 2, 0x44);
    }

    return USB_OK;
}

/**
 * \brief USB 转串口 pl2303 终端设置函数
 */
static int __pl2303_termios_set(struct usbh_serial_port    *p_port,
                                struct usb_serial_port_cfg *p_cfg){
    uint8_t                    control, buf[7] = {0};
    int                        ret;
    struct usbh_serial_pl2303 *p_pl2303        =
        (struct usbh_serial_pl2303 *)USBH_SERIAL_DRV_HANDLE_GET(p_port->p_userial);

    ret = __pl2303_get_line_request(p_port->p_userial, buf);
    if (ret != 7) {
        return ret;
    }
    /* 更改数据位*/
    if (p_cfg->byte_size != p_port->cfg.byte_size) {
        switch (p_cfg->byte_size) {
            case 5:
                buf[6] = 5;
                break;
            case 6:
                buf[6] = 6;
                break;
            case 7:
                buf[6] = 7;
                break;
            default:
            case 8:
                buf[6] = 8;
       }
    }
    /* 更改波特率*/
    if (p_cfg->baud_rate != p_port->cfg.baud_rate) {
        __pl2303_baudrate_encode(p_pl2303, p_cfg->baud_rate, &buf[0]);
    }
    /* 更改停止位*/
    if (p_cfg->stop_bits != p_port->cfg.stop_bits) {
        if (p_cfg->byte_size == USB_SERIAL_CS5) {
            /* 1.5位停止位*/
            buf[4] = 1;
        } else {
            /* 2位停止位*/
            buf[4] = 2;
        }
    } else {
        /* 1位停止位*/
        buf[4] = 0;
    }

    /* 更改校验位*/
    if (p_cfg->parity != p_port->cfg.parity) {
        /* buf[5] = 0 是无校验
         * buf[5] = 1 是奇校验
         * buf[5] = 2 是偶校验
         * buf[5] = 3 是 mark 校验
         * buf[5] = 4 是 space 校验  */
        if(p_cfg->parity == USB_SERIAL_EVEN_PARITY){
            /* 偶校验*/
            buf[5] = 2;
        } else if(p_cfg->parity == USB_SERIAL_ODD_PARITY) {
            /* 奇校验*/
            buf[5] = 1;
        } else if (p_cfg->parity == USB_SERIAL_NONE_PARITY) {
            /* 无校验*/
            buf[5] = 0;
        } else {
            return -USB_ENOTSUP;
        }
    }
    /* 设置新的配置*/
    ret = __pl2303_set_line_request(p_port->p_userial, buf);
    if (ret == 7) {
        memcpy(p_pl2303->line_settings, buf, 7);
    } else {
        __USB_ERR_INFO("USB host serial line set failed(%d)\r\n", ret);
        return ret;
    }
    p_port->cfg.byte_size = p_cfg->byte_size;
    p_port->cfg.parity    = p_cfg->parity;
    p_port->cfg.stop_bits = p_cfg->stop_bits;
    p_port->cfg.baud_rate = p_cfg->baud_rate;

    /* 设置控制*/
    control = p_pl2303->line_control;
    if ((p_cfg->dtr == 1) && (p_cfg->rts == 1)) {
        p_pl2303->line_control |= (PL2303_CONTROL_DTR | PL2303_CONTROL_RTS);

        if (control != p_pl2303->line_control) {
            ret = __pl2303_control_lines_set(p_port->p_userial, p_pl2303->line_control);
            if (ret < 0) {
                __USB_ERR_INFO("USB host serial DTR/RTS set failed(%d)\r\n", ret);
                return ret;
            }
        }
    } else if ((p_cfg->dtr == 0) && (p_cfg->rts == 0)) {
        p_pl2303->line_control &= ~(PL2303_CONTROL_DTR | PL2303_CONTROL_RTS);

        if (control != p_pl2303->line_control) {
            ret = __pl2303_control_lines_set(p_port->p_userial, p_pl2303->line_control);
            if (ret < 0) {
                __USB_ERR_INFO("USB host serial DTR/RTS set failed(%d)\r\n", ret);
                return ret;
            }
        }
    }

    if (p_cfg->crtscts == 1) {
        if (p_pl2303->pl2303_priv.quirks & PL2303_QUIRK_LEGACY) {
            __pl2303_vendor_write(p_port->p_userial, 0x0, 0x41);
        } else {
            __pl2303_vendor_write(p_port->p_userial, 0x0, 0x61);
        }
    } else {
        __pl2303_vendor_write(p_port->p_userial, 0x0, 0x0);
    }

    return USB_OK;
}

/**
 * \brief USB 转串口 pl2303 写函数
 */
static int __pl2303_write(struct usbh_serial_port *p_port,
                          uint8_t                 *p_buf,
                          uint32_t                 buf_len,
                          uint32_t                *p_act_len){
    int                        ret, tmp;
    uint8_t                   *p_buf_tmp = p_buf;
    uint32_t                   len_tmp   = buf_len;
    struct usbh_serial_pl2303 *p_pl2303   = USBH_SERIAL_DRV_HANDLE_GET(p_port->p_userial);

    while (len_tmp) {
        /* 调整数据发送大小*/
        tmp = min(len_tmp, p_port->tx_pipe.pipe.buf_size);
        memcpy(p_port->tx_pipe.pipe.p_buf, p_buf_tmp, tmp);
        /* 提交传输请求包*/
        ret = usbh_trp_sync_xfer(p_pl2303->p_data_ep_wr,
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
static int __read_trp_init(struct usbh_serial_pl2303 *p_pl2303){
    int i, ret;

    for (i = 0;i < PL2303_N_TRP; i++) {
        p_pl2303->p_rd_buf[i] = usbh_serial_mem_alloc(p_pl2303->rd_buf_size);
        if (p_pl2303->p_rd_buf[i] == NULL) {
            return -USB_ENOMEM;
        }
        memset(p_pl2303->p_rd_buf[i], 0, p_pl2303->rd_buf_size);

        p_pl2303->trp_rd[i].p_ep       = p_pl2303->p_data_ep_rd;
        p_pl2303->trp_rd[i].p_ctrl     = NULL;
        p_pl2303->trp_rd[i].p_data     = p_pl2303->p_rd_buf[i];
        p_pl2303->trp_rd[i].len        = p_pl2303->rd_buf_size;
        p_pl2303->trp_rd[i].p_fn_done  = __read_bulk_cb;
        p_pl2303->trp_rd[i].p_arg      = (void *)&p_pl2303->trp_rd[i];
        p_pl2303->trp_rd[i].act_len    = 0;
        p_pl2303->trp_rd[i].status     = -USB_EINPROGRESS;
        p_pl2303->trp_rd[i].flag       = 0;
        p_pl2303->trp_rd[i].p_usr_priv = p_pl2303->p_userial;

        /* 提交传输请求包*/
        ret = usbh_trp_submit(&p_pl2303->trp_rd[i]);
        if (ret != USB_OK) {
            return ret;
        }
    }
    return USB_OK;
}

/**
 * \brief 读请求包反初始化
 */
static int __read_trp_deinit(struct usbh_serial_pl2303 *p_pl2303){
    int i, ret;

    for (i = 0; i < PL2303_N_TRP; i++) {
        ret = usbh_trp_xfer_cancel(&p_pl2303->trp_rd[i]);
        if (ret != USB_OK) {
            return ret;
        }
        if (p_pl2303->p_rd_buf[i]) {
            usbh_serial_mem_free(p_pl2303->p_rd_buf[i]);
        }
    }
    return USB_OK;
}

/* \brief 操作函数集 */
static struct usbh_serial_port_opts __g_pl2303_opts = {
    .p_fn_port_cfg_set = __pl2303_termios_set,
    .p_fn_port_write   = __pl2303_write,
};

/**
 * \brief USB 转串口 pl2303 芯片初始化
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_pl2303_init(struct usbh_serial *p_userial){
    int                        ret;
    struct usbh_serial_pl2303 *p_pl2303  = NULL;
    struct usbh_interface     *p_intf    = NULL;
    struct usbh_function      *p_usb_fun = p_userial->p_usb_fun;
    uint8_t                    i, n_eps;

    /* 获取 USB 功能设备的第一个接口*/
    p_intf = usbh_func_intf_get(p_usb_fun, p_usb_fun->first_intf_num, 0);
    if (p_intf == NULL) {
        return -USB_ENODEV;
    }

    n_eps = USBH_INTF_NEP_GET(p_intf);
    if (n_eps < 2) {
        return -USB_EILLEGAL;
    }

    /* 分配并清0 串口结构体*/
    p_pl2303 = usbh_serial_mem_alloc(sizeof(struct usbh_serial_pl2303));
    if (p_pl2303 == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_pl2303, 0, sizeof(struct usbh_serial_pl2303));

    for (i = 0; i < n_eps; i++) {
        if (USBH_EP_TYPE_GET(&p_intf->p_eps[i]) == USB_EP_TYPE_BULK) {
            if (USBH_EP_DIR_GET(&p_intf->p_eps[i]) == USB_DIR_IN) {
                p_pl2303->p_data_ep_rd = &p_intf->p_eps[i];
            } else {
                p_pl2303->p_data_ep_wr = &p_intf->p_eps[i];
            }
        }
    }
    if ((p_pl2303->p_data_ep_wr == NULL) || (p_pl2303->p_data_ep_rd == NULL)) {
        ret = -USB_EILLEGAL;
        goto __failed;
    }

    p_pl2303->rd_buf_size = USBH_EP_MPS_GET(p_pl2303->p_data_ep_rd);
    p_pl2303->p_userial   = p_userial;

    ret = usbh_serial_ports_create(p_userial, 1);
    if (ret != USB_OK) {
        goto __failed;
    }

    ret = usbh_serial_port_init(p_userial,
                                0,
                                p_pl2303->p_data_ep_wr,
                                2048,
                                1000,
                                p_pl2303->p_data_ep_rd,
                                2048);
    if (ret != USB_OK) {
        goto __failed;
    }

    /* 启动 pl2303*/
    ret = __pl2303_startup(p_pl2303);
    if(ret != USB_OK){
        __USB_ERR_INFO("USB host serial startup failed(%d)\r\n", ret);
        goto __failed;
    }

    /* 设置兼容*/
    if (p_pl2303->pl2303_priv.quirks & PL2303_QUIRK_LEGACY) {
        ret = usbh_dev_ep_halt_clr(p_pl2303->p_data_ep_wr);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host serial out EP halt clear failed(%)\r\n", ret);
            goto __failed;
        }
        ret = usbh_dev_ep_halt_clr(p_pl2303->p_data_ep_rd);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host serial in EP halt clear failed(%)\r\n", ret);
            goto __failed;
        }
    } else {
        /* 复位上流数据管道*/
        __pl2303_vendor_write(p_userial, 8, 0);
        __pl2303_vendor_write(p_userial, 9, 0);
    }

    usbh_serial_port_opts_set(p_userial, &__g_pl2303_opts);

    /* 设置 USB 转串口结构体私有数据*/
    USBH_SERIAL_DRV_HANDLE_SET(p_userial, p_pl2303);

    /* 初始化数据读请求包*/
    ret = __read_trp_init(p_pl2303);
    if (ret != USB_OK) {
        goto __failed;
    }

    return USB_OK;

__failed:
    usbh_serial_pl2303_deinit(p_userial);

    return ret;
}

/**
 * \brief USB 转串口 pl2303 芯片反初始化函数
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_pl2303_deinit(struct usbh_serial *p_userial){
    int                        ret;
    struct usbh_serial_pl2303 *p_pl2303 =
            (struct usbh_serial_pl2303 *)USBH_SERIAL_DRV_HANDLE_GET(p_userial);

    if (p_pl2303 == NULL) {
        return -USB_EINVAL;
    }
    ret = __read_trp_deinit(p_pl2303);
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

    usbh_serial_mem_free(p_pl2303);

    USBH_SERIAL_DRV_HANDLE_SET(p_userial, NULL);

    return USB_OK;
}
