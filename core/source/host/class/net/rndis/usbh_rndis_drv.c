/**
 * \brief RNDIS(Remote Network Driver Interface Specification) 远程网络驱动接口规范
 */
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/net/rndis/usbh_rndis_drv.h"

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbh_net_lib __g_usbh_net_lib;
extern int usbh_net_generic_cdc_bind(struct usbh_net *p_net, struct usbh_interface *p_intf);
extern int usbh_net_cdc_unbind(struct usbh_net *p_net);

/*******************************************************************************
 * Code
 ******************************************************************************/
/*
 * \brief USB 主机 RNDIS 设备信息表明
 */
static void __rndis_msg_indicate(struct usbh_net       *p_net,
                                 struct rndis_indicate *p_msg,
                                 uint32_t               msg_len){
    if (p_net->p_drv_info->p_fn_indication) {
        p_net->p_drv_info->p_fn_indication(p_net, p_net, msg_len);
    } else {
        uint32_t status = USB_CPU_TO_LE32(p_msg->status);

        switch (status) {
            case RNDIS_STATUS_MEDIA_CONNECT:
                __USB_INFO("USB host net RNDIS device media connect\r\n");
                break;
            case RNDIS_STATUS_MEDIA_DISCONNECT:
                __USB_INFO("USB host net RNDIS device media disconnect\r\n");
                break;
            default:
                __USB_INFO("USB host net RNDIS device indication: 0x%08x\r\n", status);
        }
    }
}

/*
 * \brief USB 主机 RNDIS 设备命令处理
 */
static int __rndis_cmd(struct usbh_net      *p_net,
                       struct rndis_msg_hdr *p_buf,
                       uint32_t              buf_len){
    struct usbh_cdc_state       *p_info = (void *)&p_net->data;
    //struct usb_cdc_notification  notification;
    uint32_t                     req_id, msg_type, msg_len, rsp, status;
    uint32_t                     xid    = 0;
    uint8_t                      master_intf_num;
    int                          ret, cnt;
    struct rndis_keepalive_c    *p_msg  = NULL;

    msg_type = USB_CPU_TO_LE32(p_buf->msg_type);

    if ((msg_type != RNDIS_MSG_HALT) && (msg_type != RNDIS_MSG_RESET)) {
        xid = p_net->xid++;
        if (xid == 0) {
            xid = p_net->xid++;
        }
        p_buf->request_id = xid;
    }

    master_intf_num = USBH_INTF_NUM_GET(p_info->p_control);
    ret = usbh_ctrl_trp_sync_xfer(&p_net->p_usb_fun->p_usb_dev->ep0,
                                   USB_REQ_TYPE_CLASS | USB_REQ_TAG_INTERFACE,
                                   USB_CDC_SEND_ENCAPSULATED_COMMAND,
                                   0,
                                   master_intf_num,
                                   USB_CPU_TO_LE32(p_buf->msg_len),
                                   p_buf,
                                   RNDIS_CONTROL_TIMEOUT,
                                   0);
    if ((ret != (int)USB_CPU_TO_LE32(p_buf->msg_len)) || (xid == 0)) {
        return ret;
    }

    /* 有一些设备在轮询状态通道之前，不响应控制通道*/
    if (p_net->p_drv_info->data & RNDIS_DRIVER_DATA_POLL_STATUS) {
        __USB_ERR_INFO("USB host net RNDIS devicestatus poll implememted\r\n");
        return -USB_ENOTSUP;
//        ret = usb_interrupt_msg(dev->udev,
//                                usb_rcvintpipe(dev->udev,
//                                dev->status->desc.bEndpointAddress),
//                               &notification, sizeof(notification), &partial,
//                                RNDIS_CONTROL_TIMEOUT_MS);
//        if (unlikely(retval < 0))
//           return retval;
    }

    /* 轮询控制通道，请求可能马上完成 */
    rsp = USB_CPU_TO_LE32(p_buf->msg_type) | RNDIS_MSG_COMPLETION;
    for (cnt = 0; cnt < 10; cnt++) {
        memset(p_buf, 0, RNDIS_CONTROL_BUF_SIZE);

        ret = usbh_ctrl_trp_sync_xfer(&p_net->p_usb_fun->p_usb_dev->ep0,
                                       USB_DIR_IN | USB_REQ_TYPE_CLASS | USB_REQ_TAG_INTERFACE,
                                       USB_CDC_GET_ENCAPSULATED_RESPONSE,
                                       0,
                                       master_intf_num,
                                       buf_len,
                                       p_buf,
                                       RNDIS_CONTROL_TIMEOUT,
                                       0);
        if (ret >= 8) {
            msg_type = USB_CPU_TO_LE32(p_buf->msg_type);
            msg_len  = USB_CPU_TO_LE32(p_buf->msg_len);
            status   = USB_CPU_TO_LE32(p_buf->status);
            req_id   = p_buf->request_id;

            if (msg_type == rsp) {
                if (req_id == xid) {
                    if ((rsp == RNDIS_MSG_RESET_C) || (RNDIS_STATUS_SUCCESS == status)) {
                        return USB_OK;
                    }
                    __USB_ERR_INFO("USB host net RNDIS device reply status %08x\r\n", status);
                    return -USB_EPERM;
                }
                __USB_INFO("USB host net RNDIS device reply id %d expected %d\r\n", req_id, xid);
            } else {
                switch (msg_type) {
                    /* 错误/事件 */
                    case RNDIS_MSG_INDICATE:
                        __rndis_msg_indicate(p_net, (void *)p_buf, buf_len);
                        break;
                    /* ping */
                    case RNDIS_MSG_KEEPALIVE:
                        p_msg = (void *)p_buf;

                        p_msg->msg_type = USB_CPU_TO_LE32(RNDIS_MSG_KEEPALIVE_C);
                        p_msg->msg_len  = USB_CPU_TO_LE32(sizeof(struct rndis_keepalive_c));
                        p_msg->status   = USB_CPU_TO_LE32(RNDIS_STATUS_SUCCESS);

                        ret = usbh_ctrl_trp_sync_xfer(&p_net->p_usb_fun->p_usb_dev->ep0,
                                                       USB_REQ_TYPE_CLASS | USB_REQ_TAG_INTERFACE,
                                                       USB_CDC_SEND_ENCAPSULATED_COMMAND,
                                                       0,
                                                       master_intf_num,
                                                       sizeof(struct rndis_keepalive_c),
                                                       p_msg,
                                                       RNDIS_CONTROL_TIMEOUT,
                                                       0);
                        if (ret < 0) {
                            __USB_ERR_INFO("USB host net RNDIS keepalive failed(%d)\r\n", ret);
                        }
                        break;
                    default:
                        __USB_ERR_INFO("USB host net RNDIS device msg %08x len %d unexpected\r\n",
                                USB_CPU_TO_LE32(p_buf->msg_type), msg_len);
                }
            }
        } else {
            __USB_ERR_INFO("USB host net RNDIS device response failed(%d)\r\n", ret);
        }
        usb_mdelay(20);
    }
    return -USB_ETIME;
}

/*
 * \brief USB 主机 RNDIS 设备查询
 */
static int __rndis_query(struct usbh_net  *p_net,
                         void             *p_buf,
                         uint32_t          oid,
                         uint32_t          in_len,
                         void            **p_reply,
                         int              *p_reply_len){
    int      ret;
    union {
        void                 *p_buf;
        struct rndis_msg_hdr *p_header;
        struct rndis_query   *p_get;
        struct rndis_query_c *p_get_c;
    } u;
    uint32_t off, len;

    u.p_buf = p_buf;

    memset(u.p_get, 0, sizeof(struct rndis_query) + in_len);
    u.p_get->msg_type = USB_CPU_TO_LE32(RNDIS_MSG_QUERY);
    u.p_get->msg_len  = USB_CPU_TO_LE32(sizeof(struct rndis_query) + in_len);
    u.p_get->oid      = USB_CPU_TO_LE32(oid);
    u.p_get->len      = USB_CPU_TO_LE32(in_len);
    u.p_get->offset   = USB_CPU_TO_LE32(20);

    ret = __rndis_cmd(p_net, u.p_header, RNDIS_CONTROL_BUF_SIZE);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB host net RNDIS device msg 0x%08x query failed(%d)\r\n", oid, ret);
        return ret;
    }

    off = USB_CPU_TO_LE32(u.p_get_c->offset);
    len = USB_CPU_TO_LE32(u.p_get_c->len);
    if (((8 + off + len) > RNDIS_CONTROL_BUF_SIZE)) {
        goto __failed;
    }

    if ((*p_reply_len != -1) && ((int)len != *p_reply_len)) {
        goto __failed;
    }
    *p_reply     = (unsigned char *) &u.p_get_c->request_id + off;
    *p_reply_len = (int)len;

    return USB_OK;
__failed:
    __USB_ERR_INFO("USB host net RNDIS device msg 0x%08x "
                   "response invalid, off %d len %d\r\n", oid, off, len);
    return -USB_EILLEGAL;
}

/**
 * \brief USB 主机  RNDIS 设备通用绑定
 *
 * \param[in] p_net USB 主机网络设备
 * \param[in] flags 标志
 *
 * \retval 成功返回 USB_OK
 */
int usbh_rndis_generic_bind(struct usbh_net *p_net, uint32_t flags){
    int                       ret, reply_len;
    union {
        void                 *p_buf;
        struct rndis_msg_hdr *p_header;
        struct rndis_init    *p_init;
        struct rndis_init_c  *p_init_c;
        struct rndis_query   *p_get;
        struct rndis_query_c *p_get_c;
        struct rndis_set     *p_set;
        struct rndis_set_c   *p_set_c;
        struct rndis_halt    *p_halt;
    } u;
    uint8_t                  *p_bp;
    struct usbh_interface    *p_intf = p_net->p_intf;
    uint32_t                  tmp, phym_unspec;
    uint32_t                 *p_phym = NULL;

    u.p_buf = usb_lib_malloc(&__g_usbh_net_lib.lib, RNDIS_CONTROL_BUF_SIZE);
    if (u.p_buf == NULL) {
        return -USB_ENOMEM;
    }
    memset(u.p_buf, 0, RNDIS_CONTROL_BUF_SIZE);

    ret = usbh_net_generic_cdc_bind(p_net, p_intf);
    if (ret != USB_OK) {
        return ret;
    }

    u.p_init->msg_type      = USB_CPU_TO_LE32(RNDIS_MSG_INIT);
    u.p_init->msg_len       = USB_CPU_TO_LE32(sizeof(struct rndis_init));
    u.p_init->major_version = USB_CPU_TO_LE32(1);
    u.p_init->minor_version = USB_CPU_TO_LE32(0);

    p_net->hard_header_len += sizeof(struct rndis_data_hdr);
    p_net->hard_mtu         = p_net->mtu + p_net->hard_header_len;

    /* 获取输出传输请求包最大包大小*/
    p_net->max_packet           = USBH_EP_MPS_GET(p_net->p_ep_out);
    p_net->rx_trp_size          = p_net->hard_mtu + (p_net->max_packet + 1);
    p_net->rx_trp_size         &= ~(p_net->max_packet - 1);
    u.p_init->max_transfer_size = USB_CPU_TO_LE32(p_net->rx_trp_size);

    ret = __rndis_cmd(p_net, u.p_header, RNDIS_CONTROL_BUF_SIZE);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB host net RNDIS device init failed(%d)\n", ret);
        goto __failed1;
    }
    tmp = USB_CPU_TO_LE32(u.p_init_c->max_transfer_size);
    if (tmp < p_net->hard_mtu) {
        if (tmp <= p_net->hard_header_len) {
            __USB_ERR_INFO("USB host net RNDIS device can't "
                           "take %d byte packets(max %d)\r\n", p_net->hard_mtu, tmp);
            ret = -USB_EILLEGAL;
            goto __failed1;
        }
        __USB_INFO("USB host net RNDIS device can't take %d byte "
                   "packets(max %d), adjusting mtu to %d\r\n", p_net->hard_mtu, tmp, tmp - p_net->hard_header_len);
        p_net->hard_mtu = tmp;
        p_net->mtu      = p_net->hard_mtu - p_net->hard_header_len;
    }

    __USB_INFO("USB host net RNDIS device hard mtu %d (%d from dev), "
               "rx buflen %d, align %d\r\n",p_net->hard_mtu, tmp,
                p_net->rx_trp_size, 1 << USB_CPU_TO_LE32(u.p_init_c->packet_alignment));

    /* 有一些模块在 RNDIS_INIT 之后需要完成一些初始化*/
    if (p_net->p_drv_info->p_fn_early_init) {
        ret = p_net->p_drv_info->p_fn_early_init(p_net);
        if (ret != USB_OK) {
            goto __failed2;
        }
    }
    /* 检查物理媒介*/
    reply_len = sizeof(uint32_t *);
    ret = __rndis_query(p_net, u.p_buf, RNDIS_OID_GEN_PHYSICAL_MEDIUM, 0, (void **)&p_phym, &reply_len);
    if ((ret != USB_OK) || (p_phym == NULL)) {
         /* OID 是可选的，所以不要在这失败  */
        phym_unspec = USB_CPU_TO_LE32(RNDIS_PHYSICAL_MEDIUM_UNSPECIFIED);
        p_phym      = &phym_unspec;
    }

    if ((flags & RNDIS_PHYM_WIRELESS_FLAG) &&
        ((uint32_t)(*p_phym) != RNDIS_PHYSICAL_MEDIUM_WIRELESS_LAN)) {

        __USB_ERR_INFO("USB host net RNDIS device requires wireless physical medium, but device is not\r\n");
        ret = -USB_ENOTSUP;
        goto __failed2;
    }
    if ((flags & RNDIS_PHYM_NOT_WIRELESS_FLAG) &&
        ((uint32_t)(*p_phym)  == RNDIS_PHYSICAL_MEDIUM_WIRELESS_LAN)) {

        __USB_ERR_INFO("USB host net RNDIS device requires non-wireless physical medium, but device is wireless\r\n");
        ret = -USB_ENOTSUP;
        goto __failed2;
    }

    /* 获取主机以太网地址 */
    reply_len = USB_NET_HWADDR_MAX_LEN;
    ret = __rndis_query(p_net, u.p_buf, RNDIS_OID_802_3_PERMANENT_ADDRESS, 48, (void **)&p_bp, &reply_len);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB host net RNDIS device ethernet addr get failed(%d)\r\n", ret);
        goto __failed2;
    }
    memcpy(p_net->dev_addr, p_bp, USB_NET_HWADDR_MAX_LEN);

    /* 设置一个非 0 滤波去使能数据传输*/
    memset(u.p_set, 0, sizeof(struct rndis_set));
    u.p_set->msg_type = USB_CPU_TO_LE32(RNDIS_MSG_SET);
    u.p_set->msg_len  = USB_CPU_TO_LE32(4 + sizeof(struct rndis_set));
    u.p_set->oid      = USB_CPU_TO_LE32(RNDIS_OID_GEN_CURRENT_PACKET_FILTER);
    u.p_set->len      = USB_CPU_TO_LE32(4);
    u.p_set->offset   = USB_CPU_TO_LE32((sizeof(struct rndis_set)) - 8);
    *(uint32_t *)(u.p_buf + sizeof(struct rndis_set)) = USB_CPU_TO_LE32(RNDIS_DEFAULT_FILTER);

    ret = __rndis_cmd(p_net, u.p_header, RNDIS_CONTROL_BUF_SIZE);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB host net RNDIS device set packet filter failed(%d)\n", ret);
        goto __failed2;
    }

    usb_lib_mfree(&__g_usbh_net_lib.lib, u.p_buf);

    return USB_OK;

__failed2:
    memset(u.p_halt, 0, sizeof(struct rndis_halt));
    u.p_halt->msg_type = USB_CPU_TO_LE32(RNDIS_MSG_HALT);
    u.p_halt->msg_len  = USB_CPU_TO_LE32(sizeof(struct rndis_halt));
    __rndis_cmd(p_net, (void *)u.p_halt, RNDIS_CONTROL_BUF_SIZE);
__failed1:
    usb_lib_mfree(&__g_usbh_net_lib.lib, u.p_buf);

    return ret;
}

/**
 * \brief USB 主机  RNDIS 设备绑定
 */
static int __rndis_bind(struct usbh_net *p_net){
    return usbh_rndis_generic_bind(p_net, RNDIS_PHYM_NOT_WIRELESS_FLAG);
}

/**
 * \brief USB 主机 RNDIS 设备解除绑定
 */
static int __rndis_unbind(struct usbh_net *p_net){
    struct rndis_halt halt;

    memset(&halt, 0, sizeof(struct rndis_halt));
    halt.msg_type = USB_CPU_TO_LE32(RNDIS_MSG_HALT);
    halt.msg_len  = USB_CPU_TO_LE32(sizeof(struct rndis_halt));
    __rndis_cmd(p_net, (void *)&halt, RNDIS_CONTROL_BUF_SIZE);

    return usbh_net_cdc_unbind(p_net);
}

/**
 * \brief USB 主机  RNDIS 设备状态通知
 */
void __rndis_status(struct usbh_net *p_net, struct usbh_trp *p_trp){

}

static uint8_t *__rndis_tx_fixup(struct usbh_net *p_net, uint8_t *p_buf, uint32_t buf_len){
    return NULL;
}

static int __rndis_rx_fixup(struct usbh_net *p_net, uint8_t *p_buf, uint32_t buf_len){
    return -1;
}

/* \brief 远程网络驱动接口规范设备*/
static const struct usbh_net_drv_info  __g_rndis_info = {
    .p_desc        = "RNDIS device",
    .flags         =  USB_NET_ETHER | USB_NET_POINTTOPOINT | USB_NET_FRAMING_RN | USB_NET_NO_SETINTF,
    .p_fn_bind     =  __rndis_bind,
    .p_fn_unbind   =  __rndis_unbind,
    .p_fn_status   =  __rndis_status,
    .p_fn_rx_fixup =  __rndis_rx_fixup,
    .p_fn_tx_fixup =  __rndis_tx_fixup,
};

/**
 * \brief USB 主机 RNDIS 设备探测函数
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_rndis_probe(struct usbh_function *p_usb_fun){
    if ((USBH_FUNC_CLASS_GET(p_usb_fun) == USB_CLASS_WIRELESS_CONTROLLER) &&
            (USBH_FUNC_SUBCLASS_GET(p_usb_fun) == 1) &&
            (USBH_FUNC_PROTO_GET(p_usb_fun) == 3)) {
        p_usb_fun->func_type = USBH_FUNC_UNIC;
        return USB_OK;
    }
    return -USB_ENOTSUP;
}

/**
 * \brief USB 主机 RNDIS 设备创建函数
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 * \param[in] p_name    USB 主机 RNDIS 设备名字
 *
 * \retval 成功返回 USB_OK
 */
int usbh_rndis_create(struct usbh_function *p_usb_fun, char *p_name){
    int ret;

    ret = usbh_net_create(p_usb_fun, p_name, &__g_rndis_info);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb host net create failed(%d)\r\n", ret);
    }

    return ret;
}

/**
 * \brief USB 主机 RNDIS 设备销毁函数
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_rndis_destroy(struct usbh_function *p_usb_fun){
    return usbh_net_destroy(p_usb_fun);
}

/**
 * \brief USB 主机 RNDIS 设备打开函数
 *
 * \param[in]  p_handle  打开句柄
 * \param[in]  flag      打开标志，本接口支持两种打开方式：
 *                       USBH_DEV_OPEN_BY_NAME 是通过名字打开设备
 *                       USBH_DEV_OPEN_BY_UFUN 是通过 USB 功能结构体打开设备
 * \param[out] p_net_ret 成功返回 USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_rndis_open(void             *p_handle,
                    uint8_t           flag,
                    struct usbh_net **p_net_ret){
    return usbh_net_open(p_handle, flag, p_net_ret);
}

/**
 * \brief USB 主机 RNDIS 设备关闭函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_rndis_close(struct usbh_net *p_net){
    return usbh_net_close(p_net);
}

/**
 * \brief USB 主机 RNDIS 设备启动函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_rndis_start(struct usbh_net *p_net){
    return usbh_net_start(p_net);
}

/**
 * \brief USB 主机 RNDIS 设备停止函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_rndis_stop(struct usbh_net *p_net){
    return usbh_net_stop(p_net);
}

/**
 * \brief USB 主机 RNDIS 设备写函数
 *
 * \param[in] p_net   USB 主机网络设备
 * \param[in] p_buf   写缓存
 * \param[in] buf_len 写缓存长度
 *
 * \retval 成功返回实际写的长度
 */
int usbh_rndis_write(struct usbh_net *p_net, uint8_t *p_buf, uint32_t buf_len){
    return usbh_net_write(p_net, p_buf, buf_len);
}

/**
 * \brief USB 主机 RNDIS 设备读函数
 *
 * \param[in] p_net   USB 主机网络设备
 * \param[in] p_buf   读缓存
 * \param[in] buf_len 读缓存长度
 *
 * \retval 成功返回读到的数据长度
 */
int usbh_rndis_read(struct usbh_net *p_net, uint8_t *p_buf, uint32_t buf_len){
    return usbh_net_read(p_net, p_buf, buf_len);
}
