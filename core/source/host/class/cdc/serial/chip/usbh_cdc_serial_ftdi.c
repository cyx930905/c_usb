/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/cdc/serial/chip/usbh_cdc_serial_ftdi.h"

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 确定 FTDI 芯片的具体型号
 */
static void __ftdi_determine_type(struct usbh_serial_ftdi *p_ftdi){
    uint32_t              version;
    uint32_t              n_intf;
    struct usbh_function *p_usb_fun = p_ftdi->p_userial->p_usb_fun;

    if ((USBH_DEV_PID_GET(p_usb_fun) == 0x6001) &&
            (USBH_DEV_VID_GET(p_usb_fun) == 0x0403)) {
        p_ftdi->chip_type = FT8U232AM;
        return;
    }

    /* 获取版本号*/
    version = USBH_DEV_BCD_GET(p_usb_fun);
    /* 获取接口数量*/
    n_intf = p_usb_fun->p_usb_dev->cfg.p_desc->num_interfaces;

    /* 多于一个接口*/
    if (n_intf > 1) {
        uint8_t intf_num;
        /* 多接口*/
        if (version == 0x0800) {
            p_ftdi->chip_type = FT4232H;
            /* 高速模式-波特率时钟运行在 120 MHZ */
            p_ftdi->baud_base = 120000000 / 2;
        } else if (version == 0x0700) {
            p_ftdi->chip_type = FT2232H;
            /* 高速模式-波特率时钟运行在 120 MHZ*/
            p_ftdi->baud_base = 120000000 / 2;
        } else {
            p_ftdi->chip_type = FT2232C;
        }
        /* 决定接口编号*/
        intf_num = USBH_FUNC_FIRST_INTFNUM_GET(p_usb_fun);
        if (intf_num == 0) {
            p_ftdi->intf_num = INTERFACE_A;
        } else  if (intf_num == 1) {
            p_ftdi->intf_num = INTERFACE_B;
        } else  if (intf_num == 2) {
            p_ftdi->intf_num = INTERFACE_C;
        } else  if (intf_num == 3) {
            p_ftdi->intf_num = INTERFACE_D;
        }
        /* BM 类型的设备有一个 bug，当序列号是 0 时  bcdDevice 设置为 0x200 */
        if (version < 0x500) {
            __USB_ERR_INFO("FTDI something fishy - bcdDevice too low for multi-interface device\r\n");
        }
    } else if (version < 0x200) {
        /* 旧设备，假设它是原始的 SIO */
        p_ftdi->chip_type = SIO;
        p_ftdi->baud_base = 12000000 / 16;
    } else if (version < 0x400) {
        /* 假设是一个 FT8U232AM（或  FT8U245AM）
         * 由于序列号 bug，这可能是一个 BM，但它仍可以作为一个 AM 设备工作 */
        p_ftdi->chip_type = FT8U232AM;
    } else if (version < 0x600) {
        /* 假设是一个FT232BM(或 FT245BM)*/
        p_ftdi->chip_type = FT232BM;
    } else if (version < 0x900) {
        /* 假设是一个FT232RL*/
        p_ftdi->chip_type = FT232RL;
    } else if (version < 0x1000) {
        /* 假设是一个FT232H*/
        p_ftdi->chip_type = FT232H;
    } else {
        /* 假设是一个FT-X系列设备*/
        p_ftdi->chip_type = FTX;
    }
}

static unsigned short int __ftdi_232am_baud_base_to_divisor(int baudrate, int base){
	unsigned short int divisor;
    /* divisor shifted 3 bits to the left */
    int                divisor3 = base / 2 / baudrate;

    if ((divisor3 & 0x7) == 7) {
        /* round x.7/8 up to x+1 */
        divisor3++;
    }
    divisor   = divisor3 >> 3;
    divisor3 &= 0x7;

    if (divisor3 == 1) {
        divisor |= 0xc000;
    } else if (divisor3 >= 4) {
        divisor |= 0x4000;
    } else if (divisor3 != 0) {
        divisor |= 0x8000;
    } else if (divisor == 1) {
        /* special case for maximum baud rate */
        divisor = 0;
    }
    return divisor;
}

static uint32_t __ftdi_232bm_baud_base_to_divisor(int baudrate, int base){
    static const unsigned char divfrac[8] = { 0, 3, 2, 4, 1, 5, 6, 7 };
    uint32_t                   divisor;
    /* divisor shifted 3 bits to the left */
    int                        divisor3   = base / 2 / baudrate;

    divisor  = divisor3 >> 3;
    divisor |= (uint32_t)divfrac[divisor3 & 0x7] << 14;
    /* Deal with special cases for highest baud rates. */
    if (divisor == 1) {
        divisor = 0;
    } else if (divisor == 0x4001) {
        divisor = 1;
    }
    return divisor;
}

static uint32_t __ftdi_2232h_baud_base_to_divisor(int baudrate, uint32_t base){
    static const unsigned char divfrac[8] = { 0, 3, 2, 4, 1, 5, 6, 7 };
    uint32_t                   divisor;
    int                        divisor3;

    /* hi-speed baud rate is 10-bit sampling instead of 16-bit */
    divisor3 = base * 8 / (baudrate * 10);

    divisor  = divisor3 >> 3;
    divisor |= (uint32_t)divfrac[divisor3 & 0x7] << 14;

    /* Deal with special cases for highest baud rates. */
    if (divisor == 1) {
        divisor = 0;
    } else if (divisor == 0x4001) {
        divisor = 1;
    }
    /*
     * Set this bit to turn off a divide by 2.5 on baud rate generator
     * This enables baud rates up to 12Mbaud but cannot reach below 1200
     * baud with this bit set
     */
    divisor |= 0x00020000;
    return divisor;
}


static uint32_t __ftdi_232am_baud_to_divisor(int baudrate){
     return __ftdi_232am_baud_base_to_divisor(baudrate, 48000000);
}

static uint32_t __ftdi_232bm_baud_to_divisor(int baudrate){
     return __ftdi_232bm_baud_base_to_divisor(baudrate, 48000000);
}

static uint32_t __ftdi_2232h_baud_to_divisor(int baudrate){
     return __ftdi_2232h_baud_base_to_divisor(baudrate, 120000000);
}

/**
 * \brief FTDI 获取除数
 */
static uint32_t __ftdi_divisor_get(int                      baudrate,
                                   struct usbh_serial_ftdi *p_ftdi){
    uint32_t              div_value = 0;
    int                   div_okay  = 1;
    struct usbh_function *p_usb_fun = p_ftdi->p_userial->p_usb_fun;

    if (baudrate == 0) {
        baudrate = 9600;
    }
    switch (p_ftdi->chip_type) {
        case SIO: /* SIO 芯片*/
            switch (baudrate) {
                case 300:    div_value = ftdi_sio_b300; break;
                case 600:    div_value = ftdi_sio_b600; break;
                case 1200:   div_value = ftdi_sio_b1200; break;
                case 2400:   div_value = ftdi_sio_b2400; break;
                case 4800:   div_value = ftdi_sio_b4800; break;
                case 9600:   div_value = ftdi_sio_b9600; break;
                case 19200:  div_value = ftdi_sio_b19200; break;
                case 38400:  div_value = ftdi_sio_b38400; break;
                case 57600:  div_value = ftdi_sio_b57600;  break;
                case 115200: div_value = ftdi_sio_b115200; break;
            } /* 波特率*/
            if (div_value == 0) {
                div_value = ftdi_sio_b9600;
                baudrate  = 9600;
                div_okay  = 0;
            }
            break;
        case FT8U232AM: /* 8U232AM 芯片*/
            if (baudrate <= 3000000) {
                div_value = __ftdi_232am_baud_to_divisor(baudrate);
            } else {
                baudrate  = 9600;
                div_value = __ftdi_232am_baud_to_divisor(9600);
                div_okay  = 0;
            }
            break;
        case FT232BM: /* FT232BM 芯片 */
        case FT2232C: /* FT2232C 芯片 */
        case FT232RL: /* FT232RL 芯片 */
        case FTX:     /* FT-X 系列 */
            if (baudrate <= 3000000) {
                uint16_t product_id = USBH_DEV_PID_GET(p_usb_fun);

                if (((FTDI_NDI_HUC_PID == product_id) || (FTDI_NDI_SPECTRA_SCU_PID == product_id) ||
                     (FTDI_NDI_FUTURE_2_PID == product_id) || (FTDI_NDI_FUTURE_3_PID == product_id) ||
                     (FTDI_NDI_AURORA_SCU_PID == product_id)) && (baudrate == 19200)) {
                    baudrate = 1200000;
                }
                div_value = __ftdi_232bm_baud_to_divisor(baudrate);
            } else {
                div_value = __ftdi_232bm_baud_to_divisor(9600);
                div_okay  = 0;
                baudrate  = 9600;
            }
            break;
        case FT2232H: /* FT2232H 芯片*/
        case FT4232H: /* FT4232H 芯片 */
        case FT232H:  /* FT232H  芯片*/
            if ((baudrate <= 12000000) && (baudrate >= 1200)) {
                div_value = __ftdi_2232h_baud_to_divisor(baudrate);
            } else if (baudrate < 1200) {
                div_value = __ftdi_232bm_baud_to_divisor(baudrate);
            } else {
                div_value = __ftdi_232bm_baud_to_divisor(9600);
                div_okay  = 0;
                baudrate  = 9600;
            }
            break;
    }

    return div_value;
}

/**
 * \brief FTDI 波特率改变
 */
static int __ftdi_speed_change(struct usbh_serial_ftdi *p_ftdi,
                               int                      baudrate){
    uint16_t              trp_value;
    uint16_t              trp_idx;
    uint32_t              trp_idx_value;
    struct usbh_function *p_usb_fun = p_ftdi->p_userial->p_usb_fun;

    trp_idx_value = __ftdi_divisor_get(baudrate, p_ftdi);

    trp_value = (uint16_t)trp_idx_value;
    trp_idx   = (uint16_t)(trp_idx_value >> 16);

    if ((p_ftdi->chip_type == FT2232C) || (p_ftdi->chip_type == FT2232H) ||
        (p_ftdi->chip_type == FT4232H) || (p_ftdi->chip_type == FT232H)) {
        /* Probably the BM type needs the MSB of the encoded fractional
         * divider also moved like for the chips above. Any infos? */
        trp_idx = (uint16_t)((trp_idx << 8) | USBH_FUNC_FIRST_INTFNUM_GET(p_usb_fun));
    }

    /* 设置波特率*/
    return usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                    FTDI_SIO_SET_BAUDRATE_REQUEST_TYPE,
                                    FTDI_SIO_SET_BAUDRATE_REQUEST,
                                    trp_value,
                                    trp_idx,
                                    0,
                                    NULL,
                                    3000,
                                    0);
}

/**
 * \brief USB 转串口 FTDI 终端设置函数
 */
static int __ftdi_termios_set(struct usbh_serial_port    *p_port,
                              struct usb_serial_port_cfg *p_cfg){
    uint16_t                 value     = 0;
    int                      ret;
    struct usbh_function    *p_usb_fun = p_port->p_userial->p_usb_fun;
    struct usbh_serial_ftdi *p_ftdi    = (struct usbh_serial_ftdi *)USBH_SERIAL_DRV_HANDLE_GET(p_port->p_userial);

    if (memcmp(p_cfg, &p_port->cfg, sizeof(struct usb_serial_port_cfg)) == 0) {
        return USB_OK;
    }
    /* 复位*/
    ret = usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                   FTDI_SIO_RESET_REQUEST_TYPE,
                                   FTDI_SIO_RESET,
                                   0,
                                   USBH_FUNC_FIRST_INTFNUM_GET(p_usb_fun),
                                   0,
                                   NULL,
                                   5000,
                                   0);
    if (ret < 0) {
        __USB_ERR_INFO("USB host serial reset failed(%d)\r\n", ret);
        return ret;
    }

    if (p_cfg->byte_size != p_port->cfg.byte_size) {
        /* 设置数据位*/
        switch(p_cfg->byte_size){
            case USB_SERIAL_CS5:
                __USB_ERR_INFO("USB host serial setting CS5 quirk\r\n");
                break;
            case USB_SERIAL_CS7:
                value |= 7;
                break;
            case USB_SERIAL_CS8:
                value |= 8;
                break;
            default:
                __USB_ERR_INFO("USB host serial do not support the byte size\r\n");
                return -USB_ENOTSUP;
        }
    }
    /* 修改校验*/
    if (p_cfg->parity != p_port->cfg.parity) {
        if (p_cfg->parity == USB_SERIAL_EVEN_PARITY) {
            value |= FTDI_SIO_SET_DATA_PARITY_EVEN;
        } else if (p_cfg->parity == USB_SERIAL_ODD_PARITY) {
            value |= FTDI_SIO_SET_DATA_PARITY_ODD;
        } else if (p_cfg->parity == USB_SERIAL_NONE_PARITY) {
            value |= FTDI_SIO_SET_DATA_PARITY_NONE;
        } else {
            __USB_ERR_INFO("USB host serial do not support the parity\r\n");
            return -USB_ENOTSUP;
        }
    }
    /* 修改停止位*/
    if (p_cfg->stop_bits != p_port->cfg.stop_bits) {
        if (p_cfg->stop_bits == USB_SERIAL_ONE_STOPBIT) {
            value |= FTDI_SIO_SET_DATA_STOP_BITS_1;
        } else if (p_cfg->stop_bits == USB_SERIAL_TWO_STOPBITS) {
            value |= FTDI_SIO_SET_DATA_STOP_BITS_2;
        } else {
            __USB_ERR_INFO("USB host serial do not suporrt 1.5 stop bits\r\n");
            return -USB_ENOTSUP;
        }
    }

    /* 设置停止位/数据位/校验*/
    ret = usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                   FTDI_SIO_SET_DATA_REQUEST_TYPE,
                                   FTDI_SIO_SET_DATA,
                                   value,
                                   USBH_FUNC_FIRST_INTFNUM_GET(p_usb_fun),
                                   0,
                                   NULL,
                                   5000,
                                   0);
    if (ret < 0) {
        __USB_ERR_INFO("USB host serial config set failed(%d)\r\n", ret);
        return ret;
    }

    p_port->cfg.byte_size = p_cfg->byte_size;
    p_port->cfg.parity    = p_cfg->parity;
    p_port->cfg.stop_bits = p_cfg->stop_bits;

    /* 修改波特率*/
    if (p_cfg->baud_rate != p_port->cfg.baud_rate) {
        ret = __ftdi_speed_change(p_ftdi, p_cfg->baud_rate);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host serial baudrate set failed(%d)\r\n", ret);
            return -ret;
        }
        p_port->cfg.baud_rate = p_cfg->baud_rate;
    }

    if ((p_cfg->rts != p_port->cfg.rts) || (p_cfg->dtr != p_port->cfg.dtr)) {
         value = 0;

        if (p_cfg->rts) {
            value |= FTDI_SIO_SET_RTS_HIGH;
        } else {
            value |= FTDI_SIO_SET_RTS_LOW;
        }
        if (p_cfg->dtr) {
            value |= FTDI_SIO_SET_DTR_HIGH;
        } else {
            value |= FTDI_SIO_SET_DTR_LOW;
        }
        ret = usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                       FTDI_SIO_SET_MODEM_CTRL_REQUEST_TYPE,
                                       FTDI_SIO_MODEM_CTRL,
                                       value,
                                       USBH_FUNC_FIRST_INTFNUM_GET(p_usb_fun),
                                       0,
                                       NULL,
                                       5000,
                                       0);
        if (ret < 0) {
            __USB_ERR_INFO("USB host serial RTS/DTR set failed(%d)\r\n", ret);
            return ret;
        }
    }

    /* 设置控制流*/
    if ((p_cfg->crtscts != p_port->cfg.crtscts) && (p_cfg->crtscts == 1)) {
        ret = usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                       FTDI_SIO_SET_FLOW_CTRL_REQUEST_TYPE,
                                       FTDI_SIO_SET_FLOW_CTRL,
                                       0,
                                      (FTDI_SIO_RTS_CTS_HS | USBH_FUNC_FIRST_INTFNUM_GET(p_usb_fun)),
                                       0,
                                       NULL,
                                       3000,
                                       0);
        if (ret < 0) {
            __USB_ERR_INFO("USB host serial control flow set failed(%d)\r\n", ret);
            return ret;
        }
        p_port->cfg.crtscts = p_cfg->crtscts;
    } else {
        if (p_cfg->ixoff) {
            //todo
        } else {
            /* 无流控制*/
            ret = usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                           FTDI_SIO_SET_FLOW_CTRL_REQUEST_TYPE,
                                           FTDI_SIO_SET_FLOW_CTRL,
                                           0,
                                           USBH_FUNC_FIRST_INTFNUM_GET(p_usb_fun),
                                           0,
                                           NULL,
                                           5000,
                                           0);
            if (ret < 0) {
                __USB_ERR_INFO("USB host serial flow control set failed(%d)\r\n", ret);
                return ret;
            }
        }
        p_port->cfg.ixoff = p_cfg->ixoff;
    }
    return USB_OK;
}

/**
 * \brief USB 转串口 FTDI 写函数
 */
static int __ftdi_write(struct usbh_serial_port *p_port,
                          uint8_t               *p_buf,
                          uint32_t               buf_len,
                          uint32_t              *p_act_len){
    int                      ret, tmp;
    uint8_t                 *p_buf_tmp = p_buf;
    uint32_t                 len_tmp   = buf_len;
    struct usbh_serial_ftdi *p_ftdi     = USBH_SERIAL_DRV_HANDLE_GET(p_port->p_userial);

    while (len_tmp) {
        /* 调整数据发送大小*/
        tmp = min(len_tmp, p_port->tx_pipe.pipe.buf_size);
        memcpy(p_port->tx_pipe.pipe.p_buf, p_buf_tmp, tmp);
        /* 提交传输请求包*/
        ret = usbh_trp_sync_xfer(p_ftdi->p_data_ep_wr,
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

    if (p_trp->act_len <= 2) {
        goto __exit;
    }

    act_len = p_trp->act_len - 2;
    p_buf   = p_trp->p_data;

    usbh_serial_port_rx_buf_put(p_userial->p_ports, p_buf + 2, act_len, &data_act_len);

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
static int __read_trp_init(struct usbh_serial_ftdi *p_ftdi){
    int i, ret;

    for (i = 0;i < FTDI_N_TRP; i++) {
        p_ftdi->p_rd_buf[i] = usbh_serial_mem_alloc(p_ftdi->rd_buf_size);
        if (p_ftdi->p_rd_buf[i] == NULL) {
            return -USB_ENOMEM;
        }
        memset(p_ftdi->p_rd_buf[i], 0, p_ftdi->rd_buf_size);

        p_ftdi->trp_rd[i].p_ep       = p_ftdi->p_data_ep_rd;
        p_ftdi->trp_rd[i].p_ctrl     = NULL;
        p_ftdi->trp_rd[i].p_data     = p_ftdi->p_rd_buf[i];
        p_ftdi->trp_rd[i].len        = p_ftdi->rd_buf_size;
        p_ftdi->trp_rd[i].p_fn_done  = __read_bulk_cb;
        p_ftdi->trp_rd[i].p_arg      = (void *)&p_ftdi->trp_rd[i];
        p_ftdi->trp_rd[i].act_len    = 0;
        p_ftdi->trp_rd[i].status     = -USB_EINPROGRESS;
        p_ftdi->trp_rd[i].flag       = 0;
        p_ftdi->trp_rd[i].p_usr_priv = p_ftdi->p_userial;

        /* 提交传输请求包*/
        ret = usbh_trp_submit(&p_ftdi->trp_rd[i]);
        if (ret != USB_OK) {
            return ret;
        }
    }
    return USB_OK;
}

/**
 * \brief 读请求包反初始化
 */
static int __read_trp_deinit(struct usbh_serial_ftdi *p_ftdi){
    int i, ret;

    for (i = 0; i < FTDI_N_TRP; i++) {
        ret = usbh_trp_xfer_cancel(&p_ftdi->trp_rd[i]);
        if (ret != USB_OK) {
            return ret;
        }
        if (p_ftdi->p_rd_buf[i]) {
            usbh_serial_mem_free(p_ftdi->p_rd_buf[i]);
        }
    }
    return USB_OK;
}

/* \brief 操作函数集 */
static struct usbh_serial_port_opts __g_ftdi_opts = {
    .p_fn_port_cfg_set = __ftdi_termios_set,
    .p_fn_port_write   = __ftdi_write,
};

/**
 * \brief USB 转串口 FTDI 芯片初始化函数
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_ftdi_init(struct usbh_serial *p_userial){
    int                      ret;
    struct usbh_serial_ftdi *p_ftdi    = NULL;
    struct usbh_interface   *p_intf    = NULL;
    struct usbh_function    *p_usb_fun = p_userial->p_usb_fun;
    uint8_t                  i, n_eps;

    p_intf = usbh_func_intf_get(p_usb_fun, p_usb_fun->first_intf_num, 0);
    if (p_intf == NULL) {
        return -USB_ENODEV;
    }

    n_eps = USBH_INTF_NEP_GET(p_intf);
    if (n_eps < 2) {
        return -USB_EILLEGAL;
    }

    p_ftdi = usbh_serial_mem_alloc(sizeof(struct usbh_serial_ftdi));
    if (p_ftdi == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_ftdi, 0, sizeof(struct usbh_serial_ftdi));

    for (i = 0; i < n_eps; i++) {
        if (USBH_EP_TYPE_GET(&p_intf->p_eps[i]) == USB_EP_TYPE_BULK) {
            if (USBH_EP_DIR_GET(&p_intf->p_eps[i]) == USB_DIR_IN) {
                p_ftdi->p_data_ep_rd = &p_intf->p_eps[i];
            } else {
                p_ftdi->p_data_ep_wr = &p_intf->p_eps[i];
            }
        }
    }
    if ((p_ftdi->p_data_ep_wr == NULL) || (p_ftdi->p_data_ep_rd == NULL)) {
        ret = -USB_EILLEGAL;
        goto __failed;
    }

    p_ftdi->rd_buf_size = USBH_EP_MPS_GET(p_ftdi->p_data_ep_rd);
    p_ftdi->p_userial   = p_userial;

    /* 确定 FTDI 芯片的具体类型*/
    __ftdi_determine_type(p_ftdi);

    /* 复位*/
    ret = usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                   FTDI_SIO_RESET_REQUEST_TYPE,
                                   FTDI_SIO_RESET,
                                   0,
                                   USBH_FUNC_FIRST_INTFNUM_GET(p_usb_fun),
                                   0,
                                   NULL,
                                   5000,
                                   0);
    if (ret < 0) {
        __USB_ERR_INFO("USB host serial reset failed(%d)\r\n", ret);
        goto __failed;
    }

    ret = usbh_serial_ports_create(p_userial, 1);
    if (ret != USB_OK) {
        goto __failed;
    }

    ret = usbh_serial_port_init(p_userial,
                                0,
                                p_ftdi->p_data_ep_wr,
                                2048,
                                1000,
                                p_ftdi->p_data_ep_rd,
                                2048);
    if (ret != USB_OK) {
        goto __failed;
    }

    usbh_serial_port_opts_set(p_userial, &__g_ftdi_opts);

    /* 设置 USB 转串口结构体私有数据*/
    USBH_SERIAL_DRV_HANDLE_SET(p_userial, p_ftdi);

    /* 初始化数据读请求包*/
    ret = __read_trp_init(p_ftdi);
    if (ret != USB_OK) {
        goto __failed;
    }

    return USB_OK;

__failed:
    usbh_serial_ftdi_deinit(p_userial);

    return ret;
}

/**
 * \brief USB 转串口 FTDI 芯片反初始化函数
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_ftdi_deinit(struct usbh_serial *p_userial){
    int                      ret;
    struct usbh_serial_ftdi *p_ftdi =
            (struct usbh_serial_ftdi *)USBH_SERIAL_DRV_HANDLE_GET(p_userial);

    if (p_ftdi == NULL) {
        return -USB_EINVAL;
    }
    ret = __read_trp_deinit(p_ftdi);
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

    usbh_serial_mem_free(p_ftdi);

    USBH_SERIAL_DRV_HANDLE_SET(p_userial, NULL);

    return USB_OK;
}
