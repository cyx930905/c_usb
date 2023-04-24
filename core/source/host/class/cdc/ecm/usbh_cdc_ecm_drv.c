/**
 * \brief ECM(Ethernet Networking Control Model) 以太网网络控制模型
 */
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/cdc/ecm/usbh_cdc_ecm_drv.h"

/*******************************************************************************
 * Static
 ******************************************************************************/
/* \brief 声明一个 USB 主机网络驱动信息*/
static const struct usbh_net_drv_info  *__g_p_cdc_info = NULL;

static const uint8_t __g_mbm_guid[16] = {
    0xa3, 0x17, 0xa8, 0x8b, 0x04, 0x5e, 0x4f, 0x01,
    0xa6, 0x07, 0xc0, 0xff, 0xcb, 0x7e, 0x39, 0x2a,
};

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 检查 USB 设备是否符合远程网络驱动接口规范
 */
static usb_bool_t __is_rndis(struct usb_interface_desc *p_desc){
    return ((p_desc->interface_class == USB_CLASS_COMM) &&
            (p_desc->interface_sub_class == USB_CDC_SUBCLASS_ACM) &&
            (p_desc->interface_protocol == USB_CDC_PROTOCOL_VENDOR));
}

/**
 * \brief 检查 USB 设备 是否是 activesync
 */
static usb_bool_t __is_activesync(struct usb_interface_desc *p_desc){
    return ((p_desc->interface_class == USB_CLASS_MISC) &&
            (p_desc->interface_sub_class == USB_CDC_SUBCLASS_DLCM) &&
            (p_desc->interface_protocol == USB_CDC_PROTOCOL_V25TER));
}

/**
 * \brief 检查 USB 设备是否符合无线远程网络驱动接口规范
 */
static usb_bool_t __is_wireless_rndis(struct usb_interface_desc *p_desc){
    return ((p_desc->interface_class == USB_CLASS_WIRELESS_CONTROLLER) &&
            (p_desc->interface_sub_class == USB_CDC_SUBCLASS_DLCM) &&
            (p_desc->interface_protocol == 3));
}

/**
 * \brief 更新滤波器
 */
static int __net_cdc_filter_update(struct usbh_net *p_net){
    struct usbh_cdc_state *p_state = (void *)&p_net->data;
    struct usbh_interface *p_intf  = p_state->p_control;

    uint16_t cdc_filter = USB_CDC_PACKET_TYPE_DIRECTED | USB_CDC_PACKET_TYPE_BROADCAST;
    /* 在设备上过滤是一个可选的功能，不值得麻烦，所以我们只是粗略地关注监听，如果有任何多播请求，我们采取每一个多播*/
//    if (dev->net_flags & AW_IFF_PROMISC) {
//        cdc_filter |= USB_CDC_PACKET_TYPE_PROMISCUOUS;
//    }
//    cdc_filter |= USB_CDC_PACKET_TYPE_ALL_MULTICAST;

    return usbh_ctrl_trp_sync_xfer(&p_net->p_usb_fun->p_usb_dev->ep0,
                                    USB_REQ_TYPE_CLASS | USB_REQ_TAG_INTERFACE,
                                    USB_CDC_SET_ETHERNET_PACKET_FILTER,
                                    cdc_filter,
                                    p_intf->p_desc->interface_number,
                                    0,
                                    NULL,
                                    5000,
                                    0);
}

/**
 * \brief USB 主机网络 CDC 通用绑定函数
 *
 * \param[in] p_net  USB 主机网络设备
 * \param[in] p_intf 相关接口
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_generic_cdc_bind(struct usbh_net *p_net, struct usbh_interface *p_intf){
    uint8_t                         *p_buf              = p_intf->p_extra;
    int                              ret, len           = p_intf->extra_len;
    struct usb_interface_desc       *p_intf_desc        = NULL;
    struct usb_cdc_mdlm_desc        *p_mdlm_desc        = NULL;
    struct usb_cdc_mdlm_detail_desc *p_mdlm_detail_desc = NULL;

    struct usbh_cdc_state           *p_state            = (void *)&p_net->data;
    int                              rndis;

    if (sizeof(p_net->data) < sizeof(struct usbh_cdc_state)) {
        return -USB_ESIZE;
    }

    /* 除了描述符的严格规范一致性之外，还要处理将它们存储在错误位置的固件。*/
    if ((len == 0) && p_net->p_cfg->extra_len) {
        /* 摩托罗拉 SB4100 (以及其他：Brad Hards 说这是 Broadcom 设计)
         * 把  CDC 描述符放在这*/
        p_buf = p_net->p_cfg->p_extra;
        len   = p_net->p_cfg->extra_len;

        __USB_INFO("usb net cdc descriptors in config\r\n");
    }
    /* 可能  CDC 描述符在端点之后？这个 bug 在 2 线 Inc RNDIS-ish 产品见过。*/
    if (len == 0) {
        //todo
        if (len) {
            __USB_INFO("usb net cdc descriptors in endpoint\r\n");
        }
    }
    /* 这假设如果存在 cdc-acm 的非 RNDIS 的不同的产商，它将彻底失去 RNDIS 请求。*/
    rndis = (__is_rndis(p_intf->p_desc) ||
             __is_activesync(p_intf->p_desc) ||
             __is_wireless_rndis(p_intf->p_desc));

    memset(p_state, 0, sizeof(struct usbh_cdc_state));
    p_state->p_control = p_intf;

    while (len > 3) {
        /* 不是接口类请求*/
        if (p_buf[1] != (USB_REQ_TYPE_CLASS | USB_DT_INTERFACE)) {
            return -USB_EAGAIN;
        }
        /* 使用 descriptor_sub_type 来标识通讯设备类（CDC）描述符。我们
         * 期望设备使用  CDC header 和 union 描述符。对于
         * CDC 以太网我们需要以太网描述符。对于 RNDIS ，忽略两个(无意义)
         * 的 CDC modem 描述符，而选择一个复杂额基于 OID 的 RPC 方案，用一个
         * 简单的描述符来实现 CD C以太网所实现的功能。*/

        switch (p_buf[2]) {
            /* USB CDC 头类型*/
            case USB_CDC_HEADER_TYPE:
                if (p_state->p_hdr) {
                    __USB_ERR_INFO("usb host net device extra cdc header find\r\n");
                    return -USB_EAGAIN;
                }
                p_state->p_hdr = (void *)p_buf;
                /* 检查描述符合法性*/
                if (p_state->p_hdr->length != sizeof(struct usb_cdc_header_desc)) {
                    __USB_ERR_INFO("usb host net device cdc header length %d illgeal\r\n", p_state->p_hdr->length);
                    return -USB_EAGAIN;
                }
                break;
            /* USB CDC ACM 类型*/
            case USB_CDC_ACM_TYPE:
                /*  paranoia:  从 RNDIS非调制解调器中消除“真实”供应商特定调制解调器接口的歧义。*/
                if (rndis) {
                    struct usb_cdc_acm_descriptor *p_acm = NULL;

                    p_acm = (void *)p_buf;
                    if (p_acm->capabilities) {
                        __USB_ERR_INFO("usb host net device acm capabilities %02x, not really RNDIS?\r\n", p_acm->capabilities);
                        return -USB_EAGAIN;
                    }
                }
                break;
            /* USB CDC 联合类型*/
            case USB_CDC_UNION_TYPE:
                if (p_state->p_u) {
                    __USB_ERR_INFO("usb host net device extra cdc union find\r\n");
                    return -USB_EAGAIN;
                }
                p_state->p_u = (void *)p_buf;
                /* 检查描述符合法性*/
                if (p_state->p_u->length != sizeof(struct usb_cdc_union_desc)) {
                    __USB_ERR_INFO("usb host net device cdc union length %d illgeal\r\n", p_state->p_u->length);
                    return -USB_EAGAIN;
                }

                /* 我们需要一个主/控制接口(我们正在探测的东西)和一个从/数据接口；联合描述符将所有这些分类出来。*/
                p_state->p_control = usbh_dev_intf_get(p_net->p_usb_fun->p_usb_dev, p_state->p_u->master_interface0, 0);
                p_state->p_data    = usbh_dev_intf_get(p_net->p_usb_fun->p_usb_dev, p_state->p_u->slave_interface0, 0);

                if ((p_state->p_control == NULL) || (p_state->p_data == NULL)) {
                    __USB_INFO("usb host net device ecm master %d slave %d\r\n",
                                p_state->p_u->master_interface0,
                                p_state->p_u->slave_interface0);
                    /* 回到 RNDIS 的硬件接线*/
                    if (rndis) {
                        goto __next_desc;
                    }
                    return -USB_EAGAIN;
                }
                if (p_state->p_control != p_intf) {
                    /* Ambit USB 电缆调制解调器（或其他调制解调器）交换主从接口。*/
                    if (p_state->p_data == p_intf) {
                        p_state->p_data    = p_state->p_control;
                        p_state->p_control = p_intf;
                    } else {
                        return -USB_EAGAIN;
                    }
                }

                /* 一些设备合并这些 - 跳过类检查*/
                if (p_state->p_control == p_state->p_data) {
                    goto __next_desc;
                }

                /* 数据接口备用设置执行真正的 I/O */
                p_intf_desc = p_state->p_data->p_desc;
                /* 检查数据接口描述符是否合法*/
                if (p_intf_desc->interface_class != USB_CLASS_CDC_DATA) {
                    __USB_ERR_INFO("usb host net device slave class %x illgeal\r\n", p_intf_desc->interface_class);
                    return -USB_EAGAIN;
                }
                break;
            /* USB CDC 以太网类型*/
            case USB_CDC_ETHERNET_TYPE:
                 if (p_state->p_ether) {
                     __USB_ERR_INFO("usb host net device extra cdc ether find\r\n");
                     return -USB_EAGAIN;
                 }
                 p_state->p_ether = (void *)p_buf;
                 /* 检查描述符合法性*/
                 if (p_state->p_ether->length != sizeof(struct usb_cdc_ether_desc)) {
                     __USB_ERR_INFO("usb host net device cdc ether length %d illegal\r\n", p_state->p_ether->length);
                     return -USB_EAGAIN;
                 }
                 /* 获取设备硬件最大传输单元*/
                 p_net->hard_mtu = USB_CPU_TO_LE16(p_state->p_ether->max_segment_size);
                 /* 由于 Zaurus的原因，我们可能忽略了主机端的我们给定的链接地址。*/
                 break;
            /* USB CDC 移动直行模型类型*/
            case USB_CDC_MDLM_TYPE:
                if (p_mdlm_desc) {
                    __USB_ERR_INFO("usb host net device extra mdlm descriptor find\r\n");
                    return -USB_EAGAIN;
                }

                p_mdlm_desc = (void *)p_buf;
                /* 检查描述符合法性*/
                if (p_mdlm_desc->length != sizeof(struct usb_cdc_mdlm_desc)) {
                    return -USB_EAGAIN;
                }

                if (memcmp(&p_mdlm_desc->guid, __g_mbm_guid, 16)) {
                    return -USB_EAGAIN;
                }
                break;
            /* USB CDC MDLM 细节类型*/
            case USB_CDC_MDLM_DETAIL_TYPE:
                if (p_mdlm_detail_desc) {
                    __USB_ERR_INFO("usb host net device extra mdlm detail descriptor find\r\n");
                    return -USB_EAGAIN;
                }

                p_mdlm_detail_desc = (void *)p_buf;
                      /* 检查描述符合法性*/
                if (p_mdlm_detail_desc->guid_descriptor_type == 0) {
                    if (p_mdlm_detail_desc->length < (sizeof(struct usb_cdc_mdlm_detail_desc) + 1)) {
                        return -USB_EAGAIN;
                    }
                } else {
                    return -USB_EAGAIN;
                }
                break;
        }
__next_desc:
        len   -= p_buf[0];
        p_buf += p_buf[0];
    }
    if ((p_state->p_hdr == NULL) ||
            (p_state->p_u == NULL) ||
            (!rndis && (p_state->p_ether == NULL))) {
        __USB_ERR_INFO("usb host net device missing cdc %s%s%sdescriptor\r\n", p_state->p_hdr ? "" : "header ",
                        p_state->p_u ? "" : "union ", p_state->p_ether ? "" : "ether ");
        return -USB_EAGAIN;
    }
    /* 获取接口端点*/
    ret = usbh_net_endpoints_get(p_net, p_state->p_data);
    if (ret != USB_OK) {
        return ret;
    }
    /* 状态端点：CDC 以太网可选，非 RNDIS（或 ACM）*/
    if (p_state->p_data != p_state->p_control) {
        p_net->p_ep_status = NULL;
    }
    if (p_state->p_control->p_desc->num_endpoints == 1) {
        struct usb_endpoint_desc *p_ep_desc = NULL;

        p_net->p_ep_status = &p_state->p_control->p_eps[0];
        p_ep_desc = p_net->p_ep_status->p_ep_desc;

        if ((USBH_EP_DIR_GET(p_net->p_ep_status) == USB_DIR_OUT) ||
                (USB_CPU_TO_LE16(p_ep_desc->max_packet_size) < sizeof(struct usb_cdc_notification)) ||
                (p_ep_desc->interval == 0)) {
            __USB_ERR_INFO("usb host net device have bad notification endpoint\r\n");
            p_net->p_ep_status = NULL;
        }
    }
    if (rndis && (p_net->p_ep_status == NULL)) {
        __USB_ERR_INFO("usb host net device missing rndis status endpoint\r\n");
        return -USB_ENODEV;
    }

    /* 有些设备初始化不正确。 尤其是包滤波器没有复位。有些设备不会一直重置。 所以包过滤器应该设置为一个正常的初始值。*/
    return __net_cdc_filter_update(p_net);
}

/**
 * \brief USB 主机网络通讯设备类绑定
 */
static int __net_cdc_bind(struct usbh_net *p_net){
    int                    ret;
    struct usbh_interface *p_intf  = p_net->p_intf;
    struct usbh_cdc_state *p_state = (void *)&p_net->data;

    ret  = usbh_net_generic_cdc_bind(p_net, p_intf);
    if (ret != USB_OK) {
        return ret;
    }
    /* USB 网络获取以太网地址*/
    return usbh_net_ethernet_addr_get(p_net, p_state->p_ether->mac_address);
}

/**
 * \brief USB 主机网络通讯设备类解除绑定
 */
int usbh_net_cdc_unbind(struct usbh_net *p_net){
    struct usbh_cdc_state *p_state = (void *)&p_net->data;
    struct usbh_interface *p_intf  = p_net->p_intf;

    /* 共同的接口 ，直接退出*/
    if (p_state->p_data == p_state->p_control) {
        return USB_OK;
    }
    if ((p_intf == p_state->p_control) && p_state->p_data) {
        p_state->p_data = NULL;
    } else if ((p_intf == p_state->p_data) && p_state->p_control) {
        p_state->p_control = NULL;
    }
    return USB_OK;
}

/**
 * \brief USB 主机网络状态处理函数
 */
static void __net_cdc_status(struct usbh_net *p_net, struct usbh_trp *p_trp){
    struct usb_cdc_notification *p_event = NULL;

    if (p_trp->act_len < sizeof(struct usb_cdc_notification)) {
        return;
    }

    p_event = p_trp->p_data_dma;

    switch (p_event->notification_type) {
        /* 网络连接*/
        case USB_CDC_NOTIFY_NETWORK_CONNECTION:
//            __USB_ECM_TRACE("CDC: carrier %s\r\n", event->wValue ? "on" : "off");
            usbh_net_link_change(p_net, !!p_event->value, 0);
            break;
        /* 发送/接收速率改变*/
        case USB_CDC_NOTIFY_SPEED_CHANGE:
//            __USB_ECM_TRACE("CDC: speed change (len %d)\r\n", trp->act_len);
            if (p_trp->act_len != (sizeof(struct usb_cdc_notification) + 8)) {

            } else {

            }
//                dev->dev_flags |= (1 << AW_EVENT_STS_SPLIT);
//            else
//                dumpspeed((uint32_t *) &event[1]);
            break;
        /* USB_CDC_NOTIFY_RESPONSE_AVAILABLE 也可以发生(例如 RNDIS),
         * 但响应数据没有标准的格式*/
        default:
            __USB_ERR_INFO("usb host net device cdc notification %02x unexpected\r\n", p_event->notification_type);
            break;
    }
}

/* \brief CDC 以太网设备*/
static const struct usbh_net_drv_info __g_cdc_info = {
    .p_desc            = "CDC Ethernet Device",
    .flags             =  USB_NET_ETHER | USB_NET_POINTTOPOINT,
    .p_fn_bind         =  __net_cdc_bind,
    .p_fn_unbind       =  usbh_net_cdc_unbind,
    .p_fn_status       =  __net_cdc_status,
    .p_fn_rx_mode_set  =  __net_cdc_filter_update,
    .p_fn_power_manage = NULL,
};

/* \brief 无线广域网设备*/
static const struct usbh_net_drv_info __g_wwan_info  = {
    .p_desc            = "Mobile Broadband Network Device",
    .flags             =  USB_NET_WWAN,
    .p_fn_bind         =  __net_cdc_bind,
    .p_fn_unbind       =  usbh_net_cdc_unbind,
    .p_fn_status       =  __net_cdc_status,
    .p_fn_rx_mode_set  =  __net_cdc_filter_update,
    .p_fn_power_manage =  NULL,
};

/**
 * \brief USB 主机 ECM 设备探测函数
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ecm_probe(struct usbh_function *p_usb_fun){
    if ((USBH_FUNC_CLASS_GET(p_usb_fun) == USB_CLASS_COMM) &&
            (USBH_FUNC_SUBCLASS_GET(p_usb_fun) == USB_CDC_SUBCLASS_ETHERNET) &&
            (USBH_FUNC_PROTO_GET(p_usb_fun) == 0)) {
        __g_p_cdc_info = &__g_cdc_info;
        goto __exit;
    }
    if ((USBH_FUNC_CLASS_GET(p_usb_fun) == USB_CLASS_COMM) &&
            (USBH_FUNC_SUBCLASS_GET(p_usb_fun) == USB_CDC_SUBCLASS_MDLM) &&
            (USBH_FUNC_PROTO_GET(p_usb_fun) == 0)) {
        __g_p_cdc_info = &__g_wwan_info;
        goto __exit;
    }

    return -USB_ENOTSUP;

__exit:
    p_usb_fun->func_type = USBH_FUNC_UNIC;
    return USB_OK;
}

/**
 * \brief USB 主机 ECM 设备创建函数
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 * \param[in] p_name    USB 主机 ECM 设备名字
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ecm_create(struct usbh_function *p_usb_fun, char *p_name){
    int ret;

    if ((p_usb_fun == NULL) || (p_name == NULL)) {
        return -USB_EINVAL;
    }

    ret = usbh_ecm_probe(p_usb_fun);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb function is not ECM\r\n");
        return ret;
    }

    ret = usbh_net_create(p_usb_fun, p_name, __g_p_cdc_info);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb host net create failed(%d)\r\n", ret);
    }

    return ret;
}

/**
 * \brief  USB 主机 ECM 设备销毁函数
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ecm_destroy(struct usbh_function *p_usb_fun){
    return usbh_net_destroy(p_usb_fun);
}

/**
 * \brief USB 主机 ECM 设备打开函数
 *
 * \param[in]  p_handle  打开句柄
 * \param[in]  flag      打开标志，本接口支持两种打开方式：
 *                       USBH_DEV_OPEN_BY_NAME 是通过名字打开设备
 *                       USBH_DEV_OPEN_BY_UFUN 是通过 USB 功能结构体打开设备
 * \param[out] p_net_ret 成功返回 USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ecm_open(void             *p_handle,
                  uint8_t           flag,
                  struct usbh_net **p_net_ret){
    return usbh_net_open(p_handle, flag, p_net_ret);
}

/**
 * \brief USB 主机 ECM 设备关闭函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ecm_close(struct usbh_net *p_net){
    return usbh_net_close(p_net);
}

/**
 * \brief USB 主机 ECM 设备启动函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ecm_start(struct usbh_net *p_net){
    return usbh_net_start(p_net);
}

/**
 * \brief USB 主机 ECM 设备停止函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ecm_stop(struct usbh_net *p_net){
    return usbh_net_stop(p_net);
}

/**
 * \brief USB 主机 ECM 设备写函数
 *
 * \param[in] p_net   USB 主机网络设备
 * \param[in] p_buf   写缓存
 * \param[in] buf_len 写缓存长度
 *
 * \retval 成功返回实际写的长度
 */
int usbh_ecm_write(struct usbh_net *p_net, uint8_t *p_buf, uint32_t buf_len){
    return usbh_net_write(p_net, p_buf, buf_len);
}

/**
 * \brief USB 主机 ECM 设备读函数
 *
 * \param[in] p_net   USB 主机网络设备
 * \param[in] p_buf   读缓存
 * \param[in] buf_len 读缓存长度
 *
 * \retval 成功返回读到的数据长度
 */
int usbh_ecm_read(struct usbh_net *p_net, uint8_t *p_buf, uint32_t buf_len){
    return usbh_net_read(p_net, p_buf, buf_len);
}

