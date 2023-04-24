/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/cdc/serial/chip/usbh_cdc_serial_ch348.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*******************************************************************************
 * Code
 ******************************************************************************/
static uint8_t __cal_recv_tmt(uint32_t bd){
    int dly = 1000000 * 15 / bd;

    if (bd >= 921600) {
        return 5;
    }
    return (dly / 100 + 1);
}

static void __cal_outdata(uint8_t *p_buf, uint8_t rol, uint8_t xor){
    uint8_t en_status, i;

    for (i = 0; i < rol; i++) {
        en_status = p_buf[0];
        p_buf[0]  = p_buf[0] << 1;
        p_buf[0]  = p_buf[0] | ((p_buf[1] & 0x80) ? 1 : 0);
        p_buf[1]  = p_buf[1] << 1;
        p_buf[1]  = p_buf[1] | ((p_buf[2] & 0x80) ? 1 : 0);
        p_buf[2]  = p_buf[2] << 1;
        p_buf[2]  = p_buf[2] | ((p_buf[3] & 0x80) ? 1 : 0);
        p_buf[3]  = p_buf[3] << 1;
        p_buf[3]  = p_buf[3] | ((p_buf[4] & 0x80) ? 1 : 0);
        p_buf[4]  = p_buf[4] << 1;
        p_buf[4]  = p_buf[4] | ((p_buf[5] & 0x80) ? 1 : 0);
        p_buf[5]  = p_buf[5] << 1;
        p_buf[5]  = p_buf[5] | ((p_buf[6] & 0x80) ? 1 : 0);
        p_buf[6]  = p_buf[6] << 1;
        p_buf[6]  = p_buf[6] | ((p_buf[7] & 0x80) ? 1 : 0);
        p_buf[7]  = p_buf[7] << 1;
        p_buf[7]  = p_buf[7] | ((en_status & 0x80) ? 1 : 0);
    }
    for (i = 0; i < 8; i++) {
        p_buf[i] = p_buf[i] ^ xor;
    }
}

/**
 * \brief USB 转串口 ch348 读控制
 */
static int __control_in(struct usbh_serial       *p_userial,
                        uint8_t                   request,
                        uint16_t                  value,
                        uint16_t                  index,
                        uint8_t                  *p_buf,
                        uint32_t                  buf_size){
    struct usbh_function *p_usb_fun = p_userial->p_usb_fun;

    return usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                   (USB_REQ_TYPE_VENDOR | USB_REQ_TAG_DEVICE | USB_DIR_IN),
                                    request,
                                    value,
                                    index,
                                    buf_size,
                                    p_buf,
                                    3000,
                                    0);
}

/*
 * \brief USB 转串口 ch348 命令发送函数
 */
static int __cmd_out(struct usbh_serial_ch348 *p_ch348,
                     uint8_t                  *p_buf,
                     uint32_t                  buf_size){
    return usbh_trp_sync_xfer(p_ch348->p_cmd_ep_wr,
                              NULL,
                              p_buf,
                              buf_size,
                              3000,
                              0);
}

#if 0
/*
 * \brief USB 转串口 ch348 命令发送函数
 */
static int __cmd_in(struct usbh_serial_ch348 *p_ch348,
                    uint8_t                    *p_buf,
                    uint32_t                    buf_size){
    return usbh_trp_sync_xfer(p_ch348->p_cmd_ep_rd,
                              NULL,
                              p_buf,
                              buf_size,
                              3000,
                              0);
}
#endif

/*
 * \brief USB 转串口 ch348 控制设置函数
 */
static int __control_set(struct usbh_serial_ch348 *p_ch348, int port_num, int control){
    int     ret;
    uint8_t buf[3];
    uint8_t rgadd = 0;

    if (p_ch348->chip_type == CHIP_CH9344) {
        rgadd = 0x10 * port_num + 0x08;
    } else if ((p_ch348->chip_type == CHIP_CH348L) || (p_ch348->chip_type == CHIP_CH348Q)) {
        if (port_num < 4) {
            rgadd = 0x10 * port_num;
        } else {
            rgadd = 0x10 * (port_num - 4) + 0x08;
        }
    }
    buf[0] = CH348_CMD_W_BR;
    buf[1] = rgadd + CH348_R_C4;
    buf[2] = control;

    ret = __cmd_out(p_ch348, buf, 3);
    if (ret == 3) {
        return USB_OK;
    }
    return ret;
}


/**
 * \brief USB 转串口 ch348 芯片类型枚举
 */
static int __chip_type_enum(struct usbh_serial_ch348 *p_ch348){
    int     ret;
    uint8_t buf[4] = {0};

    ret = __control_in(p_ch348->p_userial, CH348_CMD_VER, 0x0000, 0, buf, 4);
    if (ret < 0) {
        return ret;
    }

    if (ret == 4) {
        if (buf[1] & (0x02 << 6)) {
            p_ch348->chip_type = CHIP_CH348Q;
        } else {
           p_ch348->chip_type = CHIP_CH348L;
        }
        p_ch348->n_ports      = 8;
        p_ch348->is_modeline9 = USB_TRUE;
        p_ch348->port_offset  = 0;
    } else {
        return -USB_ENOTSUP;
    }
    return USB_OK;
}

/**
 * \brief 波特率参数获取
 */
static int __baud_rate_para_get(int clockRate, int bval, uint8_t *p_bd1, uint8_t *p_bd2){
    int dis, x2;

    if ((bval < 0) || (bval > 12000000)) {
        return -USB_EPERM;
    }
    /* caculate dis from bval */
    if (bval == 2000000) {
        *p_bd1 = 2;
        *p_bd2 = 0;
    } else {
        dis = 10 * clockRate / 16 / bval;
        x2 = dis % 10;
        dis /= 10;
        if (x2 >= 5) {
            dis++;
        }
        *p_bd1 = dis;
        *p_bd2 = (uint8_t)(dis >> 8);
    }

    return USB_OK;
}

/**
 * \brief USB 转串口 ch348 端口配置设置函数
 */
int __ch348_termios_set(struct usbh_serial_port *p_port, struct usb_serial_port_cfg *p_cfg){
    uint8_t                   parity_type, pedt    = 0x00;
    uint8_t                   bd1 = 0, bd2         = 0;
    uint8_t                   bd3                  = 0;
    uint8_t                   dbit = 0, pbit       = 0;
    uint8_t                   buf[128];
    uint8_t                   xor, rol;
    int                       ret, rgadd = 0, clrt = 1843200;
    struct usbh_serial_ch348 *p_ch348              = USBH_SERIAL_DRV_HANDLE_GET(p_port->p_userial);

    if (p_cfg->baud_rate == 0) {
        p_cfg->baud_rate = 9600;
    }
    if (p_cfg->baud_rate > 115200) {
        pedt = 0x01;
        clrt = 44236800;
    }
    ret = __baud_rate_para_get(clrt, p_cfg->baud_rate, &bd1, &bd2);
    if (ret != USB_OK) {
        return ret;
    }
    switch (p_cfg->baud_rate) {
        case 250000:
            bd3 = 1;
            break;
        case 500000:
            bd3 = 2;
            break;
        case 1000000:
            bd3 = 3;
            break;
        case 1500000:
            bd3 = 4;
            break;
        case 3000000:
            bd3 = 5;
            break;
        case 12000000:
            bd3 = 6;
            break;
        default:
            bd3 = 0;
            break;
    }

    if (p_cfg->parity == USB_SERIAL_EVEN_PARITY) {
        parity_type = 2;
        pbit        = (0x01 << 4) + 0x08;
    } else if (p_cfg->parity == USB_SERIAL_ODD_PARITY) {
        parity_type = 1;
        pbit        = 0x08;
    } else if (p_cfg->parity == USB_SERIAL_NONE_PARITY) {
        parity_type = 0;
        pbit        = 0x00;
    } else {
        return -USB_ENOTSUP;
    }

    switch (p_cfg->byte_size) {
        case USB_SERIAL_CS5:
            dbit = 0x00;
            break;
        case USB_SERIAL_CS6:
            dbit = 0x01;
            break;
        case USB_SERIAL_CS7:
            dbit = 0x02;
            break;
        case USB_SERIAL_CS8:
        default:
            dbit = 0x03;
            break;
    }

    if (p_ch348->chip_type == CHIP_CH9344) {
        //todo
    } else if ((p_ch348->chip_type == CHIP_CH348L) || (p_ch348->chip_type == CHIP_CH348Q)) {
        rol    = 0;
        xor    = 0;
        buf[0] = CH348_CMD_WB_E | (p_port->idx & 0x0F);
        buf[1] = CH348_R_INIT;
        buf[2] = (uint8_t)((p_port->idx  & 0x0F) | ((rol << 4) & 0xf0));
        buf[3] = (char)(p_cfg->baud_rate >> 24);
        buf[4] = (char)(p_cfg->baud_rate >> 16);
        buf[5] = (char)(p_cfg->baud_rate >> 8);
        buf[6] = (char)(p_cfg->baud_rate);

        if (p_cfg->stop_bits == USB_SERIAL_TWO_STOPBITS) {
            buf[7] = 0x02;
        } else if (p_cfg->stop_bits == USB_SERIAL_ONE_STOPBIT) {
            buf[7] = 0x00;
        }

        buf[8]  = parity_type;
        buf[9]  = p_cfg->byte_size;
        buf[10] = __cal_recv_tmt(p_cfg->baud_rate);
        buf[11] = xor;

        __cal_outdata(buf + 3, rol & 0x0f, xor);

        ret = __cmd_out(p_ch348, buf, 0x0c);
        if (ret < 0) {
            return ret;
        }

        if (p_port->idx  < 4) {
            rgadd = 0x10 * p_port->idx;
        } else {
            rgadd = 0x10 * (p_port->idx - 4) + 0x08;
        }
    }
    buf[0] = CH348_CMD_W_R;
    buf[1] = rgadd + CH348_R_C1;

    if (p_ch348->is_modeline9) {
        buf[2] = 0x0F;
    } else {
        buf[2] = 0x07;
    }
    ret = __cmd_out(p_ch348, buf, 0x03);
    if (ret < 0) {
        return ret;
    }
    ret = __control_set(p_ch348, p_port->idx, 0);
    if (ret == USB_OK) {
        p_port->cfg.parity    = p_cfg->parity;
        p_port->cfg.stop_bits = p_cfg->stop_bits;
        p_port->cfg.byte_size = p_cfg->byte_size;
        p_port->cfg.baud_rate = p_cfg->baud_rate;
    }

    return ret;
}

/**
 * \brief USB 转串口 ch348 写同步函数
 */
int __ch348_write(struct usbh_serial_port *p_port,
                  uint8_t                 *p_buf,
                  uint32_t                 buf_len,
                  uint32_t                *p_act_len){
    int                       ret, tmp;
    uint8_t                  *p_buf_tmp = p_buf;
    uint32_t                  len_tmp   = buf_len;
    uint32_t                  size;
    struct usbh_serial_ch348 *p_ch348   = USBH_SERIAL_DRV_HANDLE_GET(p_port->p_userial);

    *p_act_len = 0;

    p_port->tx_pipe.pipe.p_buf[0] = p_ch348->port_offset + p_port->idx;

    if (p_port->tx_pipe.pipe.buf_size > p_ch348->wr_size_max) {
        size = p_ch348->wr_size_max;
    } else {
        size = p_port->tx_pipe.pipe.buf_size;
    }

    while (len_tmp) {
        /* 调整数据发送大小*/
        tmp = min(len_tmp, size - 3);
        memcpy(p_port->tx_pipe.pipe.p_buf + 3, p_buf_tmp, tmp);

        p_port->tx_pipe.pipe.p_buf[1] = tmp;
        p_port->tx_pipe.pipe.p_buf[2] = tmp >> 8;

        ret = usbh_trp_sync_xfer(p_port->tx_pipe.pipe.p_ep,
                                 NULL,
                                 p_port->tx_pipe.pipe.p_buf,
                                 tmp + 3,
                                 p_port->tx_pipe.time_out,
                                 0);
        if (ret < 0) {
            return ret;
        }
        len_tmp    -= tmp;
        p_buf_tmp  += tmp;
        *p_act_len += tmp;

        if (ret != (tmp + 3)) {
            break;
        }
    }
    return (len_tmp - buf_len);
}

/* \brief 操作函数集 */
static struct usbh_serial_port_opts __g_chx_opts = {
    .p_fn_port_cfg_set = __ch348_termios_set,
    .p_fn_port_write   = __ch348_write,
};

/**
 * \brief 读请求完成回调函数
 */
static void __read_bulk_cb(void *p_arg){
    struct usbh_trp          *p_trp   = (struct usbh_trp *)p_arg;
    struct usbh_serial_ch348 *p_ch348 = p_trp->p_usr_priv;
    struct usbh_serial_port  *p_port  = NULL;
    int                       ret;
    uint8_t                   port_num_min, port_num_max, port_num;
    uint32_t                  i, act_len, data_len, data_act_len;
    uint8_t                  *p_buf   = NULL;

    if ((p_trp->status == -USB_ECANCEL) ||
            (p_ch348->p_userial->is_removed == USB_TRUE)) {
        return;
    }
    if (p_trp->act_len <= 0) {
        goto __exit;
    }

    if (p_ch348->chip_type == CHIP_CH9344) {
        port_num_min = 4;
        port_num_max = 8;
    } else if ((p_ch348->chip_type == CHIP_CH348L) || (p_ch348->chip_type == CHIP_CH348Q)) {
        port_num_min = 0;
        port_num_max = 8;
    }

    act_len = p_trp->act_len;
    p_buf   = p_trp->p_data;

    for (i = 0; i < act_len; i += 32) {
        port_num = *(p_buf + i);
        if ((port_num < port_num_min) || (port_num >= port_num_max)) {
            break;
        }
        port_num -= p_ch348->port_offset;
        data_len  = *(p_buf + i + 1);
        if (data_len > 30) {
            break;
        }
        p_port = &p_ch348->p_userial->p_ports[port_num];

        usbh_serial_port_rx_buf_put(p_port, p_buf + i + 2, data_len, &data_act_len);
    }

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
static int __read_trp_init(struct usbh_serial_ch348 *p_ch348){
    int i, ret;

    for (i = 0; i < CH348_NR; i++) {
        p_ch348->p_rd_buf[i] = usbh_serial_mem_alloc(p_ch348->rd_buf_size);
        if (p_ch348->p_rd_buf[i] == NULL) {
            return -USB_ENOMEM;
        }
        memset(p_ch348->p_rd_buf[i], 0, p_ch348->rd_buf_size);

        p_ch348->trp_data_rd[i].p_ep       = p_ch348->p_data_ep_rd;
        p_ch348->trp_data_rd[i].p_ctrl     = NULL;
        p_ch348->trp_data_rd[i].p_data     = p_ch348->p_rd_buf[i];
        p_ch348->trp_data_rd[i].len        = p_ch348->rd_buf_size;
        p_ch348->trp_data_rd[i].p_fn_done  = __read_bulk_cb;
        p_ch348->trp_data_rd[i].p_arg      = (void *)&p_ch348->trp_data_rd[i];
        p_ch348->trp_data_rd[i].act_len    = 0;
        p_ch348->trp_data_rd[i].status     = -USB_EINPROGRESS;
        p_ch348->trp_data_rd[i].flag       = 0;
        p_ch348->trp_data_rd[i].p_usr_priv = p_ch348;

        /* 提交传输请求包*/
        ret = usbh_trp_submit(&p_ch348->trp_data_rd[i]);
        if (ret != USB_OK) {
            return ret;
        }
    }
    return USB_OK;
}

/**
 * \brief 读请求包反初始化
 */
static int __read_trp_deinit(struct usbh_serial_ch348 *p_ch348){
    int i, ret;

    for (i = 0;i < CH348_NR; i++) {
        ret = usbh_trp_xfer_cancel(&p_ch348->trp_data_rd[i]);
        if (ret != USB_OK) {
            return ret;
        }
        if (p_ch348->p_rd_buf[i]) {
            usbh_serial_mem_free(p_ch348->p_rd_buf[i]);
        }
    }
    return USB_OK;
}

/**
 * \brief USB 转串口 ch348 初始化
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_ch348_init(struct usbh_serial *p_userial){
    struct usbh_interface    *p_intf     = NULL;
    int                       ret;
    struct usbh_serial_ch348 *p_ch348     = NULL;
    struct usbh_function     *p_usb_fun = p_userial->p_usb_fun;
    uint8_t                   n_eps, i;

    p_intf = usbh_func_intf_get(p_usb_fun, p_usb_fun->first_intf_num, 0);
    if (p_intf == NULL) {
        return -USB_ENODEV;
    }

    n_eps = USBH_INTF_NEP_GET(p_intf);
    if ((n_eps < 3) || (n_eps == 0)) {
        return -USB_EILLEGAL;
    }

    p_ch348 = usbh_serial_mem_alloc(sizeof(struct usbh_serial_ch348));
    if (p_ch348 == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_ch348, 0, sizeof(struct usbh_serial_ch348));

    p_ch348->p_userial = p_userial;

    if (USBH_EP_DIR_GET(&p_intf->p_eps[0]) == USB_DIR_IN) {
        p_ch348->p_data_ep_rd = &p_intf->p_eps[0];
        p_ch348->p_data_ep_wr = &p_intf->p_eps[1];
    } else {
        p_ch348->p_data_ep_rd = &p_intf->p_eps[1];
        p_ch348->p_data_ep_wr = &p_intf->p_eps[0];
    }

    if (USBH_EP_DIR_GET(&p_intf->p_eps[2]) == USB_DIR_IN) {
        p_ch348->p_cmd_ep_rd = &p_intf->p_eps[2];
        p_ch348->p_cmd_ep_wr = &p_intf->p_eps[3];
    } else {
        p_ch348->p_cmd_ep_rd = &p_intf->p_eps[3];
        p_ch348->p_cmd_ep_wr = &p_intf->p_eps[2];
    }

    p_ch348->rd_buf_size = USBH_EP_MPS_GET(p_ch348->p_data_ep_rd);
    p_ch348->wr_size_max = USBH_EP_MPS_GET(p_ch348->p_data_ep_wr);

    ret = __chip_type_enum(p_ch348);
    if (ret != USB_OK) {
        goto __failed1;
    }

    ret = usbh_serial_ports_create(p_userial, p_ch348->n_ports);
    if (ret != USB_OK) {
        goto __failed2;
    }
    for (i = 0; i < p_ch348->n_ports; i++) {
        ret = usbh_serial_port_init(p_userial,
                                    i,
                                    p_ch348->p_data_ep_wr,
                                    512,
                                    1000,
                                    p_ch348->p_data_ep_rd,
                                    2048);
        if (ret != USB_OK) {
            goto __failed2;
        }
    }

    /* 设置 USB 转串口结构体私有数据*/
    USBH_SERIAL_DRV_HANDLE_SET(p_userial, p_ch348);

    /* 设置 USB 转串口端口操作函数集*/
    usbh_serial_port_opts_set(p_userial, &__g_chx_opts);

    /* 初始化数据读请求包*/
    ret = __read_trp_init(p_ch348);
    if (ret != USB_OK) {
        goto __failed2;
    }

    return USB_OK;
__failed2:
    for (i = 0; i < p_ch348->n_ports; i++) {
        usbh_serial_port_deinit(p_userial, i);
    }
    usbh_serial_ports_destroy(p_userial);
__failed1:
    if (p_ch348) {
        usbh_serial_mem_free(p_ch348);
    }
    USBH_SERIAL_DRV_HANDLE_SET(p_userial, NULL);

    return ret;
}

/**
 * \brief USB 转串口 ch348 芯片反初始化
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_ch348_deinit(struct usbh_serial *p_userial){
    uint8_t                   i;
    int                       ret;
    struct usbh_serial_ch348 *p_ch348 = NULL;

    p_ch348 = USBH_SERIAL_DRV_HANDLE_GET(p_userial);
    if (p_ch348 == NULL) {
        return -USB_EINVAL;
    }

    ret = __read_trp_deinit(p_ch348);
    if (ret != USB_OK) {
        return ret;
    }

    for (i = 0; i < p_ch348->n_ports; i++) {
        ret = usbh_serial_port_deinit(p_userial, i);
        if (ret != USB_OK) {
            return ret;
        }
    }
    usbh_serial_ports_destroy(p_userial);

    usbh_serial_mem_free(p_ch348);

    return USB_OK;
}





