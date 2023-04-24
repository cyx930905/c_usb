/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/cdc/serial/chip/usbh_cdc_serial_cp210x.h"

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief USB 转串口 cp210x 波特率获取
 */
static unsigned int __cp210x_baudrate_quantise(uint32_t baudrate){
    if (baudrate <= 300) {
        baudrate = 300;
    } else if (baudrate <= 600)  {
        baudrate = 600;
    } else if (baudrate <= 1200) {
        baudrate = 1200;
    } else if (baudrate <= 1800) {
        baudrate = 1800;
    } else if (baudrate <= 2400) {
        baudrate = 2400;
    } else if (baudrate <= 4000) {
        baudrate = 4000;
    } else if (baudrate <= 4803) {
        baudrate = 4800;
    } else if (baudrate <= 7207) {
        baudrate = 7200;
    } else if (baudrate <= 9612) {
        baudrate = 9600;
    } else if (baudrate <= 14428) {
        baudrate = 14400;
    } else if (baudrate <= 16062) {
        baudrate = 16000;
    } else if (baudrate <= 19250) {
        baudrate = 19200;
    } else if (baudrate <= 28912) {
        baudrate = 28800;
    } else if (baudrate <= 38601) {
        baudrate = 38400;
    } else if (baudrate <= 51558) {
        baudrate = 51200;
    } else if (baudrate <= 56280) {
        baudrate = 56000;
    } else if (baudrate <= 58053) {
        baudrate = 57600;
    } else if (baudrate <= 64111) {
        baudrate = 64000;
    } else if (baudrate <= 77608) {
        baudrate = 76800;
    } else if (baudrate <= 117028) {
        baudrate = 115200;
    } else if (baudrate <= 129347) {
        baudrate = 128000;
    } else if (baudrate <= 156868) {
        baudrate = 153600;
    } else if (baudrate <= 237832) {
        baudrate = 230400;
    } else if (baudrate <= 254234) {
        baudrate = 250000;
    } else if (baudrate <= 273066) {
        baudrate = 256000;
    } else if (baudrate <= 491520) {
        baudrate = 460800;
    } else if (baudrate <= 567138) {
        baudrate = 500000;
    } else if (baudrate <= 670254) {
        baudrate = 576000;
    } else if (baudrate < 1000000) {
        baudrate = 921600;
    } else if (baudrate > 2000000) {
        baudrate = 2000000;
    }
    return baudrate;
}

/**
 * \brief USB 转串口 cp210x 配置获取
 */
static int __cp210x_cfg_get(struct usbh_function *p_usb_fun,
                            uint8_t               request,
                            uint32_t             *p_data,
                            uint32_t              data_size){
    uint32_t *p_buf = NULL;
    uint32_t  i, length;
    int       ret;

    /* 包含数组所需要的整数数量*/
    length = (((data_size - 1) | 3) + 1) / 4;

    p_buf = usbh_serial_mem_alloc(length * sizeof(uint32_t));
    if (p_buf == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_buf, 0, length * sizeof(uint32_t));

    ret = usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                   REQTYPE_INTERFACE_TO_HOST,
                                   request,
                                   0,
                                   p_usb_fun->first_intf_num,
                                   data_size,
                                   p_buf,
                                   5000,
                                   0);
    if (ret != (int)data_size) {
        usbh_serial_mem_free(p_buf);
        return -USB_EPIPE;
    }

    /* 将数据转换为整数数组*/
    for (i = 0; i < length; i++) {
        p_data[i] = USB_CPU_TO_LE32(p_buf[i]);
    }

    usbh_serial_mem_free(p_buf);

    return USB_OK;
}

/**
 * \brief USB 转串口 cp210x 配置设置
 */
static int __cp210x_cfg_set(struct usbh_function *p_usb_fun,
                            uint8_t               request,
                            uint32_t             *p_data,
                            uint32_t              data_size){
    uint32_t *p_buf = NULL, length, i;
    int       ret;

    /* 包含数组所需要的整数数量*/
    length = (((data_size - 1) | 3) + 1) / 4;

    p_buf = usbh_serial_mem_alloc(length * sizeof(uint32_t));
    if (p_buf == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_buf, 0, length * sizeof(uint32_t));

    /* 字节整数数组*/
    for (i = 0; i < length; i++) {
        p_buf[i] = USB_CPU_TO_LE32(p_data[i]);
    }
    if (data_size > 2) {
        ret = usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                       REQTYPE_HOST_TO_INTERFACE,
                                       request,
                                       0,
                                       p_usb_fun->first_intf_num,
                                       data_size,
                                       p_buf,
                                       5000,
                                       0);
        if (ret == (int)data_size) {
            ret = USB_OK;
        }
    } else {
        ret = usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                       REQTYPE_HOST_TO_INTERFACE,
                                       request,
                                       p_data[0],
                                       p_usb_fun->first_intf_num,
                                       0,
                                       NULL,
                                       5000,
                                       0);
    }
    usbh_serial_mem_free(p_buf);

    return ret;
}

/**
 * \brief USB 转串口 cp210x 波特率设置
 */
static int __cp210x_speed_change(struct usbh_function *p_usb_fun, uint32_t baudrate){
    baudrate = __cp210x_baudrate_quantise(baudrate);

    /* 设置波特率*/
    return __cp210x_cfg_set(p_usb_fun, CP210X_SET_BAUDRATE, &baudrate, sizeof(baudrate));
}

/**
 * \brief USB 转串口 cp210x 终端设置函数
 */
static int __cp210x_termios_set(struct usbh_serial_port    *p_port,
                                struct usb_serial_port_cfg *p_cfg){
    int                   ret;
    uint32_t              bits, modem_ctl[4];
    struct usbh_function *p_usb_fun = p_port->p_userial->p_usb_fun;

    /* 修改数据位*/
    if (p_cfg->byte_size != p_port->cfg.byte_size) {
        ret = __cp210x_cfg_get(p_usb_fun, CP210X_GET_LINE_CTL, &bits, 2);
        if (ret != USB_OK) {
            return ret;
        }
        bits &= ~BITS_DATA_MASK;
        switch (p_cfg->byte_size) {
            case USB_SERIAL_CS5:
                bits |= BITS_DATA_5;
                break;
            case USB_SERIAL_CS6:
                bits |= BITS_DATA_6;
                break;
            case USB_SERIAL_CS7:
                bits |= BITS_DATA_7;
                break;
            case USB_SERIAL_CS8:
                bits |= BITS_DATA_8;
                break;
            default:
                bits |= BITS_DATA_8;
                break;
        }
        ret = __cp210x_cfg_set(p_usb_fun, CP210X_SET_LINE_CTL, &bits, 2);
        if (ret != USB_OK) {
             __USB_ERR_INFO("USB host serial byte size set failed(%d)\r\n", ret);
            return ret;
        }
        p_port->cfg.byte_size = p_cfg->byte_size;
    }
    /* 修改校验*/
    if (p_cfg->parity != p_port->cfg.parity) {
        ret = __cp210x_cfg_get(p_usb_fun, CP210X_GET_LINE_CTL, &bits, 2);
        if (ret != USB_OK) {
            return ret;
        }
        bits &= ~BITS_PARITY_MASK;

        if (p_cfg->parity == USB_SERIAL_EVEN_PARITY) {
            bits |= BITS_PARITY_EVEN;
        } else if(p_cfg->parity == USB_SERIAL_ODD_PARITY) {
            bits |= BITS_PARITY_ODD;
        } else {
            bits |= BITS_PARITY_NONE;
        }
        ret = __cp210x_cfg_set(p_usb_fun, CP210X_SET_LINE_CTL, &bits, 2);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host serial parity set failed(%d)\r\n", ret);
            return ret;
        }

        p_port->cfg.parity = p_cfg->parity;
    }
    /* 修改停止位*/
    if (p_cfg->stop_bits != p_port->cfg.stop_bits) {
        ret = __cp210x_cfg_get(p_usb_fun, CP210X_GET_LINE_CTL, &bits, 2);
        if (ret != USB_OK) {
            return ret;
        }
        bits &= ~BITS_STOP_MASK;
        if (p_cfg->stop_bits == USB_SERIAL_ONE_STOPBIT) {
            bits |= BITS_STOP_1;
        } else if (p_cfg->stop_bits == USB_SERIAL_TWO_STOPBITS) {
            bits |= BITS_STOP_2;
        } else {
            __USB_ERR_INFO("USB host serial do not suporrt this stop bits\r\n");
            return -USB_ENOTSUP;
        }

        ret = __cp210x_cfg_set(p_usb_fun, CP210X_SET_LINE_CTL, &bits, 2);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host serial stop bits set failed(%d)\r\n", ret);
            return ret;
        }

        p_port->cfg.stop_bits = p_cfg->stop_bits;
    }
    /* 修改波特率*/
    if (p_cfg->baud_rate != p_port->cfg.baud_rate) {
        ret = __cp210x_speed_change(p_usb_fun, p_cfg->baud_rate);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host serial baudrate set failed(%d)\r\n", ret);
            return ret;
        }
        p_port->cfg.baud_rate = p_cfg->baud_rate;
    }
    /* 修改控制流*/
    if (p_cfg->crtscts != p_port->cfg.crtscts) {
        ret = __cp210x_cfg_get(p_usb_fun, CP210X_GET_FLOW, modem_ctl, 16);
        if (ret != USB_OK) {
            return ret;
        }

        if (p_cfg->crtscts == 1) {
            modem_ctl[0] &= ~0x7B;
            modem_ctl[0] |= 0x09;
            modem_ctl[1] = 0x80;
        } else {
            modem_ctl[0] &= ~0x7B;
            modem_ctl[0] |= 0x01;
            modem_ctl[1] |= 0x40;
        }

        ret = __cp210x_cfg_set(p_usb_fun, CP210X_SET_FLOW, modem_ctl, 16);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host serial flow set failed(%d)\r\n", ret);
            return ret;
        }
        p_port->cfg.crtscts = p_cfg->crtscts;
    }

    return USB_OK;
}

/**
 * \brief USB 转串口 cp210x 写函数
 */
static int __cp210x_write(struct usbh_serial_port *p_port,
                          uint8_t                 *p_buf,
                          uint32_t                 buf_len,
                          uint32_t                *p_act_len){
    int                        ret, tmp;
    uint8_t                   *p_buf_tmp = p_buf;
    uint32_t                   len_tmp   = buf_len;
    struct usbh_serial_cp210x *p_cp210x   = USBH_SERIAL_DRV_HANDLE_GET(p_port->p_userial);

    while (len_tmp) {
        /* 调整数据发送大小*/
        tmp = min(len_tmp, p_port->tx_pipe.pipe.buf_size);
        memcpy(p_port->tx_pipe.pipe.p_buf, p_buf_tmp, tmp);
        /* 提交传输请求包*/
        ret = usbh_trp_sync_xfer(p_cp210x->p_data_ep_wr,
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
static int __read_trp_init(struct usbh_serial_cp210x *p_cp210x){
    int i, ret;

    for (i = 0;i < CP210X_N_TRP; i++) {
        p_cp210x->p_rd_buf[i] = usbh_serial_mem_alloc(p_cp210x->rd_buf_size);
        if (p_cp210x->p_rd_buf[i] == NULL) {
            return -USB_ENOMEM;
        }
        memset(p_cp210x->p_rd_buf[i], 0, p_cp210x->rd_buf_size);

        p_cp210x->trp_rd[i].p_ep       = p_cp210x->p_data_ep_rd;
        p_cp210x->trp_rd[i].p_ctrl     = NULL;
        p_cp210x->trp_rd[i].p_data     = p_cp210x->p_rd_buf[i];
        p_cp210x->trp_rd[i].len        = p_cp210x->rd_buf_size;
        p_cp210x->trp_rd[i].p_fn_done  = __read_bulk_cb;
        p_cp210x->trp_rd[i].p_arg      = (void *)&p_cp210x->trp_rd[i];
        p_cp210x->trp_rd[i].act_len    = 0;
        p_cp210x->trp_rd[i].status     = -USB_EINPROGRESS;
        p_cp210x->trp_rd[i].flag       = 0;
        p_cp210x->trp_rd[i].p_usr_priv = p_cp210x->p_userial;

        /* 提交传输请求包*/
        ret = usbh_trp_submit(&p_cp210x->trp_rd[i]);
        if (ret != USB_OK) {
            return ret;
        }
    }
    return USB_OK;
}

/**
 * \brief 读请求包反初始化
 */
static int __read_trp_deinit(struct usbh_serial_cp210x *p_cp210x){
    int i, ret;

    for (i = 0; i < CP210X_N_TRP; i++) {
        ret = usbh_trp_xfer_cancel(&p_cp210x->trp_rd[i]);
        if (ret != USB_OK) {
            return ret;
        }
        if (p_cp210x->p_rd_buf[i]) {
            usbh_serial_mem_free(p_cp210x->p_rd_buf[i]);
        }
    }
    return USB_OK;
}

/* \brief 操作函数集 */
static struct usbh_serial_port_opts __g_cp210x_opts = {
    .p_fn_port_cfg_set = __cp210x_termios_set,
    .p_fn_port_write   = __cp210x_write,
};

/**
 * \brief USB 转串口 cp210x 芯片初始化
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_cp210x_init(struct usbh_serial *p_userial){
    int                        ret;
    struct usbh_serial_cp210x *p_cp210x  = NULL;
    struct usbh_interface     *p_intf    = NULL;
    struct usbh_function      *p_usb_fun = p_userial->p_usb_fun;
    uint8_t                    i, n_eps;

    p_intf = usbh_func_intf_get(p_usb_fun, p_usb_fun->first_intf_num, 0);
    if (p_intf == NULL) {
        return -USB_ENODEV;
    }

    n_eps = USBH_INTF_NEP_GET(p_intf);
    if (n_eps < 2) {
        return -USB_EILLEGAL;
    }

    p_cp210x = usbh_serial_mem_alloc(sizeof(struct usbh_serial_cp210x));
    if (p_cp210x == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_cp210x, 0, sizeof(struct usbh_serial_cp210x));

    for (i = 0; i < n_eps; i++) {
        if (USBH_EP_TYPE_GET(&p_intf->p_eps[i]) == USB_EP_TYPE_BULK) {
            if (USBH_EP_DIR_GET(&p_intf->p_eps[i]) == USB_DIR_IN) {
                p_cp210x->p_data_ep_rd = &p_intf->p_eps[i];
            } else {
                p_cp210x->p_data_ep_wr = &p_intf->p_eps[i];
            }
        }
    }
    if ((p_cp210x->p_data_ep_wr == NULL) || (p_cp210x->p_data_ep_rd == NULL)) {
        ret = -USB_EILLEGAL;
        goto __failed;
    }
    p_cp210x->rd_buf_size = 512;
    p_cp210x->p_userial   = p_userial;

    ret = usbh_serial_ports_create(p_userial, 1);
    if (ret != USB_OK) {
        goto __failed;
    }

    ret = usbh_serial_port_init(p_userial,
                                0,
                                p_cp210x->p_data_ep_wr,
                                2048,
                                1000,
                                p_cp210x->p_data_ep_rd,
                                2048);
    if (ret != USB_OK) {
        goto __failed;
    }

    usbh_serial_port_opts_set(p_userial, &__g_cp210x_opts);

    /* 设置 USB 转串口结构体私有数据*/
    USBH_SERIAL_DRV_HANDLE_SET(p_userial, p_cp210x);

    /* 初始化数据读请求包*/
    ret = __read_trp_init(p_cp210x);
    if (ret != USB_OK) {
        goto __failed;
    }

    return USB_OK;

__failed:
    usbh_serial_cp210x_deinit(p_userial);

    return ret;
}

/**
 * \brief USB 转串口 cp210x 芯片反初始化函数
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_cp210x_deinit(struct usbh_serial *p_userial){
    int                        ret;
    struct usbh_serial_cp210x *p_cp210x =
            (struct usbh_serial_cp210x *)USBH_SERIAL_DRV_HANDLE_GET(p_userial);

    if (p_cp210x == NULL) {
        return -USB_EINVAL;
    }
    ret = __read_trp_deinit(p_cp210x);
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

    usbh_serial_mem_free(p_cp210x);

    USBH_SERIAL_DRV_HANDLE_SET(p_userial, NULL);

    return USB_OK;
}
