/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/net/usbh_net.h"

/*******************************************************************************
 * Static
 ******************************************************************************/
/* \brief 声明一个 USB 主机网络驱动库结构体*/
struct usbh_net_lib __g_usbh_net_lib;

static void __net_rx_trps_deinit(struct usbh_net *p_net);
static int __net_rx_trps_cancel(struct usbh_net *p_net);
static int __net_deinit(struct usbh_net *p_net);

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief USB 网络驱动库释放函数
 */
static void __usbh_net_lib_release(int *p_ref){
    usb_lib_deinit(&__g_usbh_net_lib.lib);
}

/**
 * \brief 初始化一个随机的以太网地址
 */
static int __net_random_addr_init(uint8_t *p_addr){
    uint8_t i;

    for(i = 0; i < USB_NET_HWADDR_MAX_LEN; i++) {
        p_addr[i] = 0;
    }
    /* 清除多播位*/
    p_addr[0] &= 0xfe;
    /* 设置本地分配位(IEEE802) */
    p_addr[0] |= 0x02;

    return USB_OK;
}

/**
 * \brief USB 主机网络设备释放函数
 */
static void __drv_release(usb_refcnt *p_ref){
    struct usbh_net *p_net = NULL;
    int              ret;

    p_net = USB_CONTAINER_OF(p_ref, struct usbh_net, ref_cnt);

    /* 删除节点*/
    ret = usb_lib_dev_del(&__g_usbh_net_lib.lib, &p_net->node);
    if (ret != USB_OK) {
        return;
    }

    ret = usb_refcnt_put(&__g_usbh_net_lib.ref_cnt, __usbh_net_lib_release);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
        return;
    }

    ret = __net_rx_trps_cancel(p_net);
    if (ret != USB_OK) {
        return;
    }

    __net_rx_trps_deinit(p_net);

    ret = __net_deinit(p_net);
    if (ret != USB_OK) {
        return;
    }

    usb_lib_mfree(&__g_usbh_net_lib.lib, p_net);

    /* 释放 USB 设备*/
    ret = usbh_dev_ref_put(p_net->p_usb_fun->p_usb_dev);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
    }
}

/**
 * \brief USB 主机网络设备的引用增加
 */
static int __net_ref_get(struct usbh_net *p_net){
    return usb_refcnt_get(&p_net->ref_cnt);
}

/**
 * \brief USB 主机网络设备引用减少
 */
static int __net_ref_put(struct usbh_net *p_net){
    return usb_refcnt_put(&p_net->ref_cnt, __drv_release);
}

/**
 * \brief USB 主机网络设备事件设置
 */
static void __net_event_set(struct usbh_net *p_net, uint32_t event_flag){
    p_net->event_flags |= event_flag;
}

/**
 * \brief USB 主机网络设备事件获取
 */
static uint32_t __net_event_get(struct usbh_net *p_net){
    return p_net->event_flags;
}

/**
 * \brief USB 主机网络设备事件清除
 */
static void __net_event_clr(struct usbh_net *p_net, uint32_t event_flag){
    p_net->event_flags &= ~event_flag;
}

/**
 * \brief USB 主机网络设备最大队列长度更新
 */
static void __net_max_qlen_update(struct usbh_net *p_net){
    uint8_t speed = USBH_DEV_SPEED_GET(p_net->p_usb_fun);

    switch (speed) {
        case USB_SPEED_HIGH:
            p_net->rx_qlen = __g_usbh_net_lib.queue_max / p_net->rx_trp_size;
            p_net->tx_qlen = __g_usbh_net_lib.queue_max  / p_net->hard_mtu;
            break;
        case USB_SPEED_SUPER:
            p_net->rx_qlen = 5 * __g_usbh_net_lib.queue_max  / p_net->rx_trp_size;
            p_net->tx_qlen = 5 * __g_usbh_net_lib.queue_max  / p_net->hard_mtu;
           break;
        default:
            p_net->rx_qlen = p_net->tx_qlen = 4;
    }
}

/**
 * \brief USB 主机网络设备链接改变处理
 */
static void __net_link_change_handle(struct usbh_net *p_net){
//    aw_bool_t is_up;
//
//
//    /* 如果当前网络断开*/
//    aw_netif_is_link_up(&dev->ethif, &is_up);
//    if (is_up == AW_FALSE){
//        /* 停止读取数据包的USB传输请求包，以节省总线带宽*/
//        awbl_usbnet_unlink_trps(dev, &dev->rxq);
//
//        /* tx_timeout 将解除发送数据包的 USB传输请求包链接，并且在链接关闭后，发送队列将被网络核心停止*/
//    } else {
//        /* 提交读取数据包的USB传输请求包*/
//        //aw_usb_sem_give(dev->bh_sem);
//    }
//
    /* 硬件最大传输单元或接收的 USB 传输请求包大小可能会因为链接改变而改变 */
    __net_max_qlen_update(p_net);
    /* 清除链接改变事件标志*/
    __net_event_clr(p_net, USB_NET_EVENT_LINK_CHANGE);
}

/**
 * \brief USB 主机网络设备设置接收模式处理
 */
static void __net_set_rx_mode_handle(struct usbh_net *p_net){
    if (p_net->p_drv_info->p_fn_rx_mode_set) {
        p_net->p_drv_info->p_fn_rx_mode_set(p_net);
    }
    /* 清除设置接收模式事件标志*/
    __net_event_clr(p_net, USB_NET_EVENT_SET_RX_MODE);
}

/**
 * \brief USB 主机网络设备端点获取
 *
 * \param[in] p_net  USB 主机网络结构体
 * \param[in] p_intf USB 接口结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_endpoints_get(struct usbh_net       *p_net,
                           struct usbh_interface *p_intf){
    uint8_t                intf_num, j;
    int                    ret, i, intr = 0;
    struct usb_list_node  *p_node       = NULL;
    struct usbh_config    *p_cfg        = p_net->p_cfg;
    struct usbh_interface *p_intf_tmp   = NULL;
    struct usbh_endpoint  *p_ep_tmp     = NULL;
    struct usbh_endpoint  *p_in         = NULL, *p_out = NULL;
    struct usbh_endpoint  *p_status     = NULL;

    /* 获取接口编号*/
    intf_num = USBH_INTF_NUM_GET(p_intf);

    /* 遍历配置所有接口 */
    for (i = 0; i < p_cfg->n_funs; i++) {
        usb_list_for_each_node(p_node, &p_cfg->p_funs[i].intf_list) {
            p_intf_tmp = usb_container_of(p_node, struct usbh_interface, node);

            p_in = p_out = p_status = NULL;
            /* 遍历特定的接口编号的接口*/
            if (USBH_INTF_NUM_GET(p_intf_tmp) == intf_num) {
                /* 遍历接口所有端点*/
                for (j = 0; j < USBH_INTF_NEP_GET(p_intf_tmp); j++) {
                    p_ep_tmp = &p_intf_tmp->p_eps[j];
                    switch (USBH_EP_TYPE_GET(p_ep_tmp)) {
                        /* 中断端点*/
                       case USB_EP_TYPE_INT:
                           /* 中断端点是输出端点，继续遍历*/
                           if (USBH_EP_DIR_GET(p_ep_tmp) != USB_DIR_IN) {
                               continue;
                           }
                           intr = 1;
                           break;
                       /* 批量端点*/
                       case USB_EP_TYPE_BULK:
                           break;
                       default:
                           continue;
                    }
                    /* 端点是输入方向*/
                    if (USBH_EP_DIR_GET(p_ep_tmp) == USB_DIR_IN) {
                        if ((intr == 0) && (p_in == NULL)) {
                            p_in = p_ep_tmp;
                        } else if ((intr == 1) && (p_status == NULL)) {
                            p_status = p_ep_tmp;
                        }
                    } else {
                        if (p_out == NULL) {
                            p_out = p_ep_tmp;
                        }
                     }
                }
            }
            /* 获取到输入输出端点，退出遍历*/
            if (p_in && p_out) {
                goto __eps_find;
            }
        }
    }
__eps_find:
    if ((p_in == NULL) || (p_out == NULL)) {
        return -USB_EAGAIN;
    }
    /* 接口描述符拥有备用接口且该设备允许设置接口 */
    if (USBH_INTF_ALT_NUM_GET(p_intf_tmp) != 0 &&
          !(p_net->p_drv_info->flags & USB_NET_NO_SETINTF)) {
        ret = usbh_func_intf_set(p_net->p_usb_fun, intf_num, USBH_INTF_ALT_NUM_GET(p_intf_tmp));
        if (ret != USB_OK) {
            return ret;
        }
        p_net->p_intf = p_intf_tmp;
    }

    p_net->p_ep_in     = p_in;
    p_net->p_ep_out    = p_out;
    p_net->p_ep_status = p_status;

    return USB_OK;
}

/**
 * \brief USB 主机网络设备以太网地址获取
 *
 * \param[in] p_net       USB 主机网络设备
 * \param[in] mac_address 字符串索引
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_ethernet_addr_get(struct usbh_net *p_net, int mac_address){
    int  ret, tmp = -1;
    char *p_buf = NULL;

    ret = usbh_dev_string_get(p_net->p_usb_fun->p_usb_dev, mac_address, &p_buf);
    if (ret == USB_OK) {
        if (strlen(p_buf) == 12) {
            tmp = usb_hex_to_bin(p_net->dev_addr, p_buf, 6);
            if (tmp < 0) {
                __USB_ERR_INFO("USB host net device bad mac string %d fetch, %d\r\n", mac_address, tmp);
                ret = -USB_EBADF;
            }
        }
        usbh_dev_string_put(p_buf);
    }
    return ret;
}

/**
 * \brief USB 主机网络设备链接改变函数
 *
 * \param[in] p_net    USB 主机网络设备
 * \param[in] is_link  是否连接
 * \param[in] is_reset 是否复位
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_link_change(struct usbh_net *p_net,
                         usb_bool_t       is_link,
                         usb_bool_t       is_reset){
    if (p_net == NULL) {
        return -USB_EINVAL;
    }
    if ((is_reset == USB_TRUE) && (is_link == USB_TRUE)) {
        /* 发送连接复位事件*/
        __net_event_set(p_net, USB_NET_EVENT_LINK_RESET);
    }
    /* 发送连接改变事件*/
    __net_event_set(p_net, USB_NET_EVENT_LINK_CHANGE);

    return USB_OK;
}

/**
 * \brief USB 主机网络设备状态中断完成回调函数
 */
static void __net_status_intr_done(void *p_arg){
    struct usbh_net *p_net = (struct usbh_net *)p_arg;
    struct usbh_trp *p_trp = &p_net->trp_int;
    int ret, status        =  p_trp->status;

    switch (status) {
        case 0:
            p_net->p_drv_info->p_fn_status(p_net, p_trp);
            break;
        default:
            __USB_ERR_INFO("USB host net device status TRP error(%d)\r\n", status);
            break;
    }

    ret = usbh_trp_submit(p_trp);
    if (ret != 0){
        __USB_ERR_INFO("USB host net device status TRP submit failed(%d)\r\n", ret);
//        dev->interrupt_count = 0;
    }
}

/**
 * \brief USB 主机网络设备状态请求包初始化
 */
static int __net_status_trp_init(struct usbh_net *p_net){
    uint8_t  *p_buf = NULL;
    uint16_t  maxp;
    uint32_t  period;

    if (p_net->p_drv_info->p_fn_status == NULL) {
        return USB_OK;
    }

    /* 获取端点最大包大小*/
    maxp = USBH_EP_MPS_GET(p_net->p_ep_status);
    /* 避免 1 毫秒间隔:  最小 8 毫秒 轮询间隔*/
    period = max (USBH_EP_INTVAL_GET(p_net->p_ep_status),
        (USBH_DEV_SPEED_GET(p_net->p_usb_fun) == USB_SPEED_HIGH) ? 7 : 3);

    p_buf =  usb_lib_malloc(&__g_usbh_net_lib.lib, maxp);
    if (p_buf == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_buf ,0, maxp);
    memset(&p_net->trp_int, 0, sizeof(struct usbh_trp));

    p_net->trp_int.p_data    = p_buf;
    p_net->trp_int.len       = maxp;
    p_net->trp_int.p_fn_done = __net_status_intr_done;
    p_net->trp_int.p_arg     = p_net;
    p_net->trp_int.act_len   = 0;
    p_net->trp_int.status    = -USB_EINPROGRESS;;
    p_net->trp_int.flag      = 0;
    p_net->trp_int.p_ep      = p_net->p_ep_status;

    __USB_INFO("USB host net device status EP %d ,bytes %d, period %d\r\n",
            USBH_EP_ADDR_GET(p_net->p_ep_status), maxp, period);

    return USB_OK;
}

/**
 * \brief USB 主机网络设备状态请求包反初始化
 */
static void __net_status_trp_deinit(struct usbh_net *p_net){
    if (p_net->trp_int.p_data) {
        usb_lib_mfree(&__g_usbh_net_lib.lib, p_net->trp_int.p_data);
    }
}

/**
 * \brief USB 主机转串口设备端口接收缓存放入数据
 */
static int __net_rx_buf_put(struct usbh_net *p_net,
                            uint8_t         *p_buf,
                            uint32_t         buf_len,
                            uint32_t        *p_act_len){
    int      ret = USB_OK;
    uint32_t len;

#if USB_OS_EN
    ret = usb_mutex_lock(p_net->p_lock, USB_NET_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_net->rx_buf.rd_pos > p_net->rx_buf.data_pos) {
        if (buf_len > (p_net->rx_buf.rd_pos - p_net->rx_buf.data_pos - 1)) {
            len = p_net->rx_buf.rd_pos - p_net->rx_buf.data_pos;
        } else {
            len = buf_len;
        }
        memcpy(p_net->rx_buf.p_buf + p_net->rx_buf.data_pos, p_buf, len);

        *p_act_len = len;
    } else {
        /* 计算可以放入数据的总大小*/
        len = p_net->rx_buf.buf_size + p_net->rx_buf.rd_pos - p_net->rx_buf.data_pos - 1;

        if (len > buf_len) {
            len = buf_len;
        }
        /* 回环*/
        if ((len + p_net->rx_buf.data_pos) > p_net->rx_buf.buf_size) {
            len = p_net->rx_buf.buf_size - p_net->rx_buf.data_pos;
        }
        memcpy(p_net->rx_buf.p_buf + p_net->rx_buf.data_pos, p_buf, len);
        *p_act_len = len;

        len = buf_len - *p_act_len;

        if (p_net->rx_buf.rd_pos != 0) {
            if (len > (p_net->rx_buf.rd_pos - 1)) {
                len = p_net->rx_buf.rd_pos - 1;
            }
            memcpy(p_net->rx_buf.p_buf, p_buf + *p_act_len, len);
            *p_act_len += len;
        }
    }
    /* 重新设置写标记*/
    p_net->rx_buf.data_pos = (p_net->rx_buf.data_pos + *p_act_len);
    /* 到了环形末尾，回到环形头*/
    if (p_net->rx_buf.data_pos >= p_net->rx_buf.buf_size) {
        p_net->rx_buf.data_pos -= p_net->rx_buf.buf_size;
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_net->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif

    return ret;
}

/**
 * \brief USB 主机网络设备接收完成函数
 */
static void __net_rx_done(void *p_arg){
    int                  ret;
    struct usbh_net_trp *p_net_trp = (struct usbh_net_trp *)p_arg;
    struct usbh_net     *p_net     = (struct usbh_net *)(p_net_trp->trp.p_usr_priv);
    int                  status    = p_net_trp->trp.status;
    uint32_t             act_len, data_act_len, event;
    uint8_t             *p_buf     = NULL;

    event = __net_event_get(p_net);

    if ((p_net_trp->trp.status == -USB_ECANCEL) ||
            (p_net->is_removed == USB_TRUE) ||
            (event & USB_NET_EVENT_RX_KILL)) {
        return;
    }

    if (p_net_trp->trp.act_len <= 0) {
        goto __exit;
    }

    switch (status) {
        case USB_OK:
            act_len = p_net_trp->trp.act_len;
            p_buf   = p_net_trp->trp.p_data;
            __net_rx_buf_put(p_net, p_buf, act_len, &data_act_len);
            break;
        default:
            break;
    }

__exit:
    ret = usbh_trp_submit(&p_net_trp->trp);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB host net TRP submit failed(%d)\r\n", ret);
#if USB_OS_EN
        ret = usb_mutex_lock(p_net->p_lock, USB_NET_LOCK_TIMEOUT);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
            return;
        }
#endif
        usb_list_node_del(&p_net_trp->node);
        usb_list_node_add_tail(&p_net_trp->node, &p_net->rx_trp_hdr.trp_free);
        p_net->rx_trp_hdr.n_trp_start--;
#if USB_OS_EN
        ret = usb_mutex_unlock(p_net->p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        }
#endif
    }
}

/**
 * \brief USB 主机网络设备接收请求包分配函数
 */
static struct usbh_net_trp *__net_rx_trp_alloc(struct usbh_net *p_net){
    struct usbh_net_trp *p_net_trp = NULL;

    p_net_trp = usb_lib_malloc(&__g_usbh_net_lib.lib, sizeof(struct usbh_net_trp));
    if (p_net_trp == NULL) {
        return NULL;
    }
    memset(p_net_trp, 0, sizeof(struct usbh_net_trp));

    p_net_trp->trp.p_data = usb_lib_malloc(&__g_usbh_net_lib.lib, p_net->rx_trp_size);
    if (p_net_trp->trp.p_data == NULL) {
        usb_lib_mfree(&__g_usbh_net_lib.lib, p_net_trp);
        return NULL;
    }
    memset(p_net_trp->trp.p_data, 0, p_net->rx_trp_size);

    p_net_trp->trp.p_ep       = p_net->p_ep_in;
    p_net_trp->trp.len        = p_net->rx_trp_size;
    p_net_trp->trp.p_fn_done  = __net_rx_done;
    p_net_trp->trp.p_arg      = (void *)p_net_trp;
    p_net_trp->trp.p_usr_priv = (void *)p_net;
    p_net_trp->trp.status     = -USB_EINPROGRESS;

    usb_list_node_add_tail(&p_net_trp->node, &p_net->rx_trp_hdr.trp_free);

    p_net->rx_trp_hdr.n_trp_total++;

    return p_net_trp;
}

/**
 * \brief USB 主机网络设备接收请求包释放函数
 */
static void __net_rx_trp_free(struct usbh_net *p_net, struct usbh_net_trp *p_net_trp){
    if (p_net_trp) {
        usb_list_node_del(&p_net_trp->node);
        p_net->rx_trp_hdr.n_trp_total--;

        if (p_net_trp->trp.p_data) {
            usb_lib_mfree(&__g_usbh_net_lib.lib, p_net_trp->trp.p_data);
        }
        usb_lib_mfree(&__g_usbh_net_lib.lib, p_net_trp);
    }
}

/**
 * \brief USB 主机网络设备发送传输请求包初始化函数
 */
static int __net_rx_trps_init(struct usbh_net *p_net){
    uint16_t             i;
    struct usbh_net_trp *p_net_trp = NULL;

    for (i = 0; i < p_net->rx_qlen; i++) {
        p_net_trp = __net_rx_trp_alloc(p_net);
        if (p_net_trp == NULL) {
            return -USB_ENOMEM;
        }
    }

    return USB_OK;
}

/**
 * \brief  USB 主机网络设备发送传输请求包反初始化函数
 */
static void __net_rx_trps_deinit(struct usbh_net *p_net){
    struct usb_list_node *p_node     = NULL;
    struct usb_list_node *p_node_tmp = NULL;
    struct usbh_net_trp  *p_net_trp  = NULL;

    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_net->rx_trp_hdr.trp_free){
        p_net_trp = usb_container_of(p_node, struct usbh_net_trp, node);
        __net_rx_trp_free(p_net, p_net_trp);
    }
}

/**
 * \brief USB 主机网络设备发送传输请求包调整函数
 */
static int __net_rx_trps_adjust(struct usbh_net *p_net){
    uint32_t             i, num;
    struct usbh_net_trp *p_net_trp = NULL;

    if (p_net->rx_trp_hdr.n_trp_total <= p_net->rx_qlen) {
        num = p_net->rx_qlen - p_net->rx_trp_hdr.n_trp_total;

        for (i = 0; i < num; i++) {
            p_net_trp = __net_rx_trp_alloc(p_net);
            if (p_net_trp == NULL) {
                return -USB_ENOMEM;
            }
        }
    } else {
        //todo
    }
    return USB_OK;
}

/**
 * \brief USB 主机网络设备发送传输请求包提交函数
 */
static int __net_rx_trps_submit(struct usbh_net *p_net){
    struct usbh_net_trp *p_net_trp = NULL;
    uint32_t             i;
    int                  ret       = 0;

    /* 不要一次性马上重新填充队列*/
    for (i = 0; (i < 10) && (p_net->rx_trp_hdr.n_trp_start < p_net->rx_qlen); i++) {
        if (!usb_list_head_is_empty(&p_net->rx_trp_hdr.trp_free)) {
            p_net_trp = usb_container_of(p_net->rx_trp_hdr.trp_free.p_next, struct usbh_net_trp, node);

            if ((p_net_trp != NULL) &&
                    !(p_net->event_flags & USB_NET_EVENT_DEV_ASLEEP) &&
                    !(p_net->event_flags & USB_NET_EVENT_RX_HALT)) {
                usb_list_node_del(&p_net_trp->node);

                ret = usbh_trp_submit(&p_net_trp->trp);
                if (ret != USB_OK) {
                    //todo
                    return ret;
                }
                usb_list_node_add_tail(&p_net_trp->node, &p_net->rx_trp_hdr.trp_start);
                p_net->rx_trp_hdr.n_trp_start++;
            }
        }
    }
    return USB_OK;
}

/**
 * \brief USB 主机网络设备发送传输请求包取消函数
 */
static int __net_rx_trps_cancel(struct usbh_net *p_net){
    int                   ret;
    struct usb_list_node *p_node     = NULL;
    struct usb_list_node *p_node_tmp = NULL;
    struct usbh_net_trp  *p_net_trp  = NULL;

    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_net->rx_trp_hdr.trp_start){
        p_net_trp = usb_container_of(p_node, struct usbh_net_trp, node);

        ret = usbh_trp_xfer_cancel(&p_net_trp->trp);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host net device RX TRP cancel failed(%d)\r\n", ret);
            continue;
        }
        usb_list_node_del(&p_net_trp->node);
        p_net->rx_trp_hdr.n_trp_start--;

        usb_list_node_add_tail(&p_net_trp->node, &p_net->rx_trp_hdr.trp_free);
    }

    return USB_OK;
}

/**
 * \brief USB 网络设备初始化
 */
static int __net_init(struct usbh_net                *p_net,
                      struct usbh_function           *p_usb_fun,
                      char                           *p_name,
                      const struct usbh_net_drv_info *p_drv_info){
    struct usbh_interface *p_intf = NULL;
    int                    ret;

    usb_refcnt_init(&p_net->ref_cnt);
#if USB_OS_EN
    p_net->p_lock = usb_lib_mutex_create(&__g_usbh_net_lib.lib);
    if (p_net->p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        return -USB_EPERM;
    }
#endif

    p_intf = usbh_func_intf_get(p_usb_fun, p_usb_fun->first_intf_num, 0);
    if (p_intf == NULL) {
        return -USB_ENODEV;
    }

    /* 初始化一个随机的以太网地址*/
    __net_random_addr_init(p_net->dev_addr);
    strncpy(p_net->name, p_name, (USB_NAME_LEN - 1));

    p_net->mtu             = 1500;
    p_net->p_intf          = p_intf;
    p_net->p_cfg           = &p_usb_fun->p_usb_dev->cfg;
    p_net->p_usb_fun       = p_usb_fun;
    p_net->hard_header_len = 14;
    p_net->is_removed      = USB_FALSE;
    p_net->p_drv_info      = p_drv_info;
    p_net->rx_buf.buf_size = 4096;

    p_net->rx_buf.p_buf = usb_lib_malloc(&__g_usbh_net_lib.lib, 4096);
    if (p_net->rx_buf.p_buf == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_net->rx_buf.p_buf, 0, 4096);

    usb_list_head_init(&p_net->rx_trp_hdr.trp_start);
    usb_list_head_init(&p_net->rx_trp_hdr.trp_free);

    if (p_drv_info->p_fn_bind) {
        ret = p_drv_info->p_fn_bind(p_net);
        if (ret == 0) {
            /* 以太网设备（非 PPP 模式）*/
            if ((p_drv_info->flags & USB_NET_ETHER) != 0 &&
                    ((p_drv_info->flags & USB_NET_POINTTOPOINT) == 0 ||
                            (p_net->dev_addr[0] & 0x02) == 0)) {
                p_net->dev_type |= USB_NET_ETHER;
            }
            if ((p_drv_info->flags & USB_NET_WLAN) != 0) {
                p_net->dev_type |= USB_NET_WLAN;
            }
            if ((p_drv_info->flags & USB_NET_WWAN) != 0) {
                p_net->dev_type |= USB_NET_WWAN;
            }

            /* 可能远程不能接收一个以太网最大传输单元*/
            if (p_net->mtu  > (p_net->hard_mtu - p_net->hard_header_len)) {
                p_net->mtu  = p_net->hard_mtu - p_net->hard_header_len;
            }
        }
    } else if ((p_drv_info->p_ep_in == NULL) || (p_drv_info->p_ep_out == NULL)) {
        /* 获取 USB 网络设备端点*/
        ret = usbh_net_endpoints_get(p_net, p_intf);
    } else {
        p_net->p_ep_in  = p_drv_info->p_ep_in;
        p_net->p_ep_out = p_drv_info->p_ep_out;
    }
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB host net device EP get failed(%d)\r\n", ret);
        return ret;
    }
    /* 初始化状态端点*/
    if (p_net->p_ep_status) {
        ret = __net_status_trp_init(p_net);
        if (ret != USB_OK) {
            return ret;
        }
    }
    if (p_net->rx_trp_size == 0) {
        p_net->rx_trp_size = p_net->hard_mtu;
    }
    /* 获取输出传输请求包最大包大小*/
    p_net->max_packet = USBH_EP_MPS_GET(p_net->p_ep_out);

    /* 初始化最大接收队列长度和最大发送长度*/
    __net_max_qlen_update(p_net);

    return USB_OK;
}

/**
 * \brief USB 网络设备反初始化
 */
static int __net_deinit(struct usbh_net *p_net){
    int                             ret;
    const struct usbh_net_drv_info *p_drv_info = p_net->p_drv_info;

    __net_status_trp_deinit(p_net);

    if (p_drv_info->p_fn_unbind) {
        ret = p_drv_info->p_fn_unbind(p_net);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host net device unbind failed(%d)\r\n", ret);
            return ret;
        }
    }
    if (p_net->rx_buf.p_buf) {
        usb_lib_mfree(&__g_usbh_net_lib.lib, p_net->rx_buf.p_buf);
    }

#if USB_OS_EN
    if (p_net->p_lock) {
        ret = usb_lib_mutex_destroy(&__g_usbh_net_lib.lib, p_net->p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
        }
    }
#endif
    return ret;
}

/**
 * \brief USB 主机网络设备创建
 *
 * \param[in] p_usb_fun  USB 功能接口结构体
 * \param[in] p_name     USB 主机网络设备名字
 * \param[in] p_drv_info USB 网络驱动信息
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_create(struct usbh_function           *p_usb_fun,
                    char                           *p_name,
                    const struct usbh_net_drv_info *p_drv_info){
    struct usbh_net *p_net  = NULL;
    int              ret, ret_tmp;

    if ((usb_lib_is_init(&__g_usbh_net_lib.lib) == USB_FALSE) ||
            (__g_usbh_net_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    ret = usbh_dev_ref_get(p_usb_fun->p_usb_dev);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
        return ret;
    }

    p_net = usb_lib_malloc(&__g_usbh_net_lib.lib, sizeof(struct usbh_net));
    if (p_net == NULL) {
        ret = -USB_ENOMEM;
        goto __failed1;
    }
    memset(p_net, 0, sizeof(struct usbh_net));

    ret = __net_init(p_net, p_usb_fun, p_name, p_drv_info);
    if (ret != USB_OK) {
        return ret;
    }
    /* 往库中添加一个设备*/
    ret = usb_lib_dev_add(&__g_usbh_net_lib.lib, &p_net->node);
    if (ret != USB_OK) {
        goto __failed2;
    }

    ret = usb_refcnt_get(&__g_usbh_net_lib.ref_cnt);
    if (ret != USB_OK) {
        goto __failed3;
    }

    /* 设置 USB 设备类型*/
    usbh_dev_type_set(p_usb_fun->p_usb_dev, p_usb_fun->func_type);

    /* 设置 USB 功能驱动私有数据*/
    usbh_func_drvdata_set(p_usb_fun, p_net);

    return USB_OK;
__failed3:
    usb_lib_dev_del(&__g_usbh_net_lib.lib, &p_net->node);
__failed2:
    __net_deinit(p_net);
__failed1:
    if (p_net) {
        usb_lib_mfree(&__g_usbh_net_lib.lib, p_net);
    }

    ret_tmp = usbh_dev_ref_put(p_usb_fun->p_usb_dev);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret_tmp);
    }

    return ret;
}

/**
 * \brief USB 主机网络设备销毁
 *
 * \param[in] p_usb_fun USB 功能接口结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_destroy(struct usbh_function *p_usb_fun){
    int              ret;
    struct usbh_net *p_net = NULL;

    if (p_usb_fun == NULL) {
        return -USB_EINVAL;
    }
    if (p_usb_fun->func_type != USBH_FUNC_UNIC) {
        return -USB_EILLEGAL;
    }

    ret = usbh_func_drvdata_get(p_usb_fun, (void **)&p_net);
    if (ret != USB_OK) {
        return ret;
    } else if (p_net == NULL) {
        return -USB_ENODEV;
    }

    /* 设置移除标志*/
    p_net->is_removed = USB_TRUE;

    return __net_ref_put(p_net);
}

/**
 * \brief USB 主机网络设备打开函数
 *
 * \param[in]  p_handle  打开句柄
 * \param[in]  flag      打开标志，本接口支持两种打开方式：
 *                       USBH_DEV_OPEN_BY_NAME 是通过名字打开设备
 *                       USBH_DEV_OPEN_BY_UFUN 是通过 USB 功能结构体打开设备
 * \param[out] p_net_ret 成功返回 USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_open(void             *p_handle,
                  uint8_t           flag,
                  struct usbh_net **p_net_ret){
    int              ret;
    struct usbh_net *p_net = NULL;

    if (p_handle == NULL) {
        return -USB_EINVAL;
    }
    if ((flag != USBH_DEV_OPEN_BY_NAME) &&
            (flag != USBH_DEV_OPEN_BY_UFUN)) {
        return -USB_EILLEGAL;
    }
    if ((usb_lib_is_init(&__g_usbh_net_lib.lib) == USB_FALSE) ||
            (__g_usbh_net_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    if (flag == USBH_DEV_OPEN_BY_NAME) {
        struct usb_list_node *p_node     = NULL;
        struct usb_list_node *p_node_tmp = NULL;
        char                 *p_name     = (char *)p_handle;

#if USB_OS_EN
        ret = usb_lib_mutex_lock(&__g_usbh_net_lib.lib);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
        USB_LIB_LIST_FOR_EACH_NODE(p_node, p_node_tmp, &__g_usbh_net_lib.lib){
            p_net = usb_container_of(p_node, struct usbh_net, node);
            if (strcmp(p_net->name, p_name) == 0) {
                break;
            }
            p_net = NULL;
        }
#if USB_OS_EN
        ret = usb_lib_mutex_unlock(&__g_usbh_net_lib.lib);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
    } else if (flag == USBH_DEV_OPEN_BY_UFUN) {
        struct usbh_function *p_usb_fun = (struct usbh_function *)p_handle;

        if (p_usb_fun->func_type != USBH_FUNC_UNIC) {
            return -USB_EPROTO;
        }

        usbh_func_drvdata_get(p_usb_fun, (void **)&p_net);
    }

    if (p_net) {
        ret = __net_ref_get(p_net);
        if (ret != USB_OK) {
            return ret;
        }
        *p_net_ret = p_net;

        return USB_OK;
    }
    return -USB_ENODEV;
}

/**
 * \brief USB 主机网络设备关闭函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_close(struct usbh_net *p_net){
    if (p_net == NULL) {
        return -USB_EINVAL;
    }

    return __net_ref_put(p_net);
}

/**
 * \brief USB 主机网络设备启动函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_start(struct usbh_net *p_net){
    int ret;
#if USB_OS_EN
    int ret_tmp;
#endif

    if (p_net == NULL) {
        return -USB_EINVAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_net->p_lock, USB_NET_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 复位设备*/
    if (p_net->p_drv_info->p_fn_reset) {
        ret = p_net->p_drv_info->p_fn_reset(p_net);
        if (ret != USB_OK) {
            goto __exit;
        }
    }
    /* 硬件最大传输单元或接收传输请求包大小可能在复位函数中改变*/
    __net_max_qlen_update(p_net);

    /* 检查对等方是否已连接*/
    if (p_net->p_drv_info->p_fn_connect_chk) {
        ret = p_net->p_drv_info->p_fn_connect_chk(p_net);
        if (ret != USB_OK) {
            goto __exit;
        }
    }
    /* 开始状态中断传输*/
    if (p_net->p_ep_status) {
//        /* 提交中断请求包((暂时不能提交。。机制问题))*/
//        ret = usbh_trp_submit(&p_net->trp_int);
//        if (ret != USB_OK) {
//            goto __exit;
//        }
    }

    /* 电源管理*/
    if (p_net->p_drv_info->p_fn_power_manage) {
        ret = p_net->p_drv_info->p_fn_power_manage(p_net, USB_TRUE);
        if (ret != USB_OK) {
            //todo
            goto __exit;
        }
    }

    ret = __net_rx_trps_init(p_net);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB host net RX TRP init failed(%d)\r\n", ret);
        goto __exit;
    }
    ret = __net_rx_trps_submit(p_net);
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_net->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif

    return ret;
}

/**
 * \brief USB 主机网络设备停止函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_stop(struct usbh_net *p_net){
    int                             ret;
#if USB_OS_EN
    int                             ret_tmp;
#endif
    const struct usbh_net_drv_info *p_info = NULL;

    if (p_net == NULL) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_net->p_lock, USB_NET_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_info = p_net->p_drv_info;

    __net_event_set(p_net, USB_NET_EVENT_RX_KILL);

    usb_mdelay(100);

    if (p_info->p_fn_stop) {
        ret = p_info->p_fn_stop(p_net);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host net device stop failed(%d)\r\n",ret);
            goto __exit;
        }
    }

    if (!(p_info->flags & USB_NET_DRV_FLAG_AVOID_UNLINK_TRPS)) {
        ret = __net_rx_trps_cancel(p_net);
        if (ret != USB_OK) {
            goto __exit;
        }
        __net_rx_trps_deinit(p_net);
    }

    p_net->event_flags = 0;

    if (p_info->p_fn_power_manage) {
        ret = p_info->p_fn_power_manage(p_net, 0);
    }
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_net->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif

    return ret;
}
#if 1
/**
 * \brief USB 主机网络设备写函数
 *
 * \param[in] p_net   USB 主机网络设备
 * \param[in] p_buf   写缓存
 * \param[in] buf_len 写缓存长度
 *
 * \retval 成功返回实际写的长度
 */
int usbh_net_write(struct usbh_net *p_net, uint8_t *p_buf, uint32_t buf_len){
    int                             ret;
    uint32_t                        length, flags = 0;
    const struct usbh_net_drv_info *p_info        = NULL;
    uint8_t                        *p_buf_tmp     = NULL;

    if ((p_net == NULL) || (p_buf == NULL) || (buf_len == 0)) {
        return -USB_EINVAL;
    }

    p_info = p_net->p_drv_info;
    if (p_info->p_fn_tx_fixup) {
        p_buf_tmp = p_info->p_fn_tx_fixup(p_net, p_buf, buf_len);
        if (p_buf_tmp == NULL) {
            return -USB_EAGAIN;
        }
    }

    length = buf_len;

    /* 不要假设硬件会处理 USB 零数据包
     * 注意：严格一致的 CDC ETHER 设备应该在这里期望 0 长度包，但忽略一个字节包
     * 注意2：CDC-ECM 与 CDC-NCM 在处理 0 长度包/短包上有不同的规范，因此 CDC-NCM 驱动
     * 程序会根据需要自行生成短包*/
    if (length % p_net->max_packet == 0) {
        /* 硬件能处理零长度包*/
        if ((p_info->flags & USB_NET_DRV_FLAG_SEND_ZLP) == 0) {
            /* 驱动不会积累多数据包*/
            if ((p_info->flags & USB_NET_DRV_FLAG_MULTI_PACKET) == 0) {
                //FIXME
            }
        } else {
            /* 用短包结束 USB 批量传输*/
            flags |= USBH_TRP_ZERO_PACKET;
        }
    }

    /* 记录传输启动时间*/
    usb_timespec_get(&p_net->trans_start_ts);

    ret = usbh_trp_sync_xfer(p_net->p_ep_out, NULL, p_buf, length, 100, flags);
    if (ret < 0) {
        __USB_ERR_INFO("USB host net TX failed(%d)\r\n", ret);

        /* 设置发送停止事件*/
        __net_event_set(p_net, USB_NET_EVENT_TX_HALT);
    }
    return ret;
}
#else

/**
 * \brief USB 主机网络设备发送完成函数
 */
static void __net_tx_done(void *p_arg) {
    //struct usbh_net *p_net = (struct usbh_net *)p_arg;
    struct usbh_trp *p_trp = (struct usbh_trp *)p_arg;

    /* 传输成功*/
    if (p_trp->status != 0) {
        __USB_ERR_INFO("USB host net device TX failed(%d)\r\n", p_trp->status);
    } else {
        usb_lib_mfree(&__g_usbh_net_lib.lib, p_trp);
    }
}


/**
 * \brief USB 主机网络设备写函数
 *
 * \param[in] p_net   USB 主机网络设备
 * \param[in] p_buf   写缓存
 * \param[in] buf_len 写缓存长度
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_write(struct usbh_net *p_net, uint8_t *p_buf, uint32_t buf_len){
    int                             ret;
    uint32_t                        length;
    const struct usbh_net_drv_info *p_info = NULL;
    struct usbh_trp                *p_trp_tx;

    if ((p_net == NULL) || (p_buf == NULL) || (buf_len == 0)) {
        return -USB_EINVAL;
    }

    p_info = p_net->p_drv_info;

    p_trp_tx = usb_lib_malloc(&__g_usbh_net_lib.lib, sizeof(struct usbh_trp));
    if (p_trp_tx == NULL) {
        return -512;
    }
    p_trp_tx->p_ep      = p_net->p_ep_out;
    p_trp_tx->p_fn_done = __net_tx_done;
    p_trp_tx->p_data    = p_buf;
    p_trp_tx->p_arg     = p_trp_tx;

    length = buf_len;

    /* 不要假设硬件会处理 USB 零数据包
     * 注意：严格一致的 CDC ETHER 设备应该在这里期望 0 长度包，但忽略一个字节包
     * 注意2：CDC-ECM 与 CDC-NCM 在处理 0 长度包/短包上有不同的规范，因此 CDC-NCM 驱动
     * 程序会根据需要自行生成短包*/
    if (length % p_net->max_packet == 0) {
        /* 硬件能处理零长度包*/
        if ((p_info->flags & USB_NET_DRV_FLAG_SEND_ZLP) == 0) {
            /* 驱动不会积累多数据包*/
            if ((p_info->flags & USB_NET_DRV_FLAG_MULTI_PACKET) == 0) {
                //FIXME
            }
        } else {
            /* 用短包结束 USB 批量传输*/
            p_net->trp_tx.flag |= USBH_TRP_ZERO_PACKET;
        }
    }
    p_trp_tx->len = length;

    ret = usbh_trp_submit(p_trp_tx);
    switch (ret) {
        /* 提交成功*/
        case USB_OK:
            /* 记录传输启动时间*/
            usb_timespec_get(&p_net->trans_start_ts);
            return USB_OK;
        case -USB_EPIPE:
            /* 发送停止事件*/
            __net_event_set(p_net, USB_NET_EVENT_TX_HALT);
            break;
        default:
            __USB_ERR_INFO("USB host net TX TRP submit failed(%d)\r\n", ret);
            break;
    }
    return ret;
}
#endif

/**
 * \brief USB 主机网络设备读函数
 *
 * \param[in] p_net   USB 主机网络设备
 * \param[in] p_buf   读缓存
 * \param[in] buf_len 读缓存长度
 *
 * \retval 成功返回读到的长度
 */
int usbh_net_read(struct usbh_net *p_net,
                  uint8_t         *p_buf,
                  uint32_t         buf_len){
#if USB_OS_EN
    int      ret;
#endif
    uint32_t len, len_act;

    if ((p_net == NULL) || (p_buf == NULL)) {
        return -USB_EINVAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_net->p_lock, USB_NET_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_net->rx_buf.rd_pos <= p_net->rx_buf.data_pos) {
        len = p_net->rx_buf.data_pos - p_net->rx_buf.rd_pos;

        if (len > buf_len) {
            len = buf_len;
        }
        memcpy(p_buf, p_net->rx_buf.p_buf + p_net->rx_buf.rd_pos, len);
        len_act = len;
    } else {
        len = p_net->rx_buf.buf_size - p_net->rx_buf.rd_pos;

        if (len > buf_len) {
            len = buf_len;
        }
        memcpy(p_buf, p_net->rx_buf.p_buf + p_net->rx_buf.rd_pos, len);
        len_act = len;

        len = p_net->rx_buf.data_pos;

        if (len > (buf_len - len_act)) {
            len = buf_len - len_act;
        }
        if (len > 0) {
            memcpy(p_buf + len_act, p_net->rx_buf.p_buf, len);
        }
        len_act += len;
    }
    p_net->rx_buf.rd_pos = p_net->rx_buf.rd_pos + len_act;
    if (p_net->rx_buf.rd_pos >= p_net->rx_buf.buf_size) {
        p_net->rx_buf.rd_pos -= p_net->rx_buf.buf_size;
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_net->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    return len_act;
}

/**
 * \brief USB 主机网络设备接收缓存大小获取
 *
 * \param[in]  p_net  USB 主机网络设备
 * \param[out] p_size 返回的缓存大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_rx_buf_size_get(struct usbh_net *p_net, uint32_t *p_size){
    int ret = USB_OK;

    if ((p_net == NULL) || (p_size == NULL)) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_net->p_lock, USB_NET_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_size = p_net->rx_buf.buf_size;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_net->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机网络设备接收缓存大小获取
 *
 * \param[in] p_net USB 主机网络设备
 * \param[in] size  要设置的缓存大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_rx_buf_size_set(struct usbh_net *p_net, uint32_t size){
    int ret = USB_OK;
#if USB_OS_EN
    int ret_tmp;
#endif

    if ((p_net == NULL) || (size == 0)) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_net->p_lock, USB_NET_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_net->rx_buf.buf_size != size) {
        if (p_net->rx_buf.p_buf != NULL) {
            usb_lib_mfree(&__g_usbh_net_lib.lib, p_net->rx_buf.p_buf);
            p_net->rx_buf.p_buf = usb_lib_malloc(&__g_usbh_net_lib.lib, size);
            if (p_net->rx_buf.p_buf == NULL) {
                ret = -USB_ENOMEM;
                goto __exit;
            }
        }
        p_net->rx_buf.buf_size = size;
    }
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_net->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 主机网络设备事件处理函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
static int __net_event_prosess(struct usbh_net *p_net){
    uint32_t evt;
    int      ret = USB_OK;

    evt = __net_event_get(p_net);

    /* 发送停止事件*/
    if (evt & USB_NET_EVENT_TX_HALT) {
        /* 清除端点停止状态*/
        ret = usbh_dev_ep_halt_clr(p_net->p_ep_out);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host net device TX EP halt clear failed(%d)\r\n",ret);
            return ret;
        } else {
            __net_event_clr(p_net, USB_NET_EVENT_TX_HALT);
            /* 重新启动传输*/
            //todo
        }
    }
    /* 接收停止事件*/
    if (evt & USB_NET_EVENT_RX_HALT) {

        __net_rx_trps_cancel(p_net);

        ret = usbh_dev_ep_halt_clr(p_net->p_ep_in);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host net device RX EP halt clear failed(%d)\r\n",ret);
            return ret;
        } else {
            __net_event_clr(p_net, USB_NET_EVENT_RX_HALT);
        }
    }
    /* 链接复位事件*/
    if (evt & USB_NET_EVENT_LINK_RESET) {
        const struct usbh_net_drv_info *p_info = p_net->p_drv_info;

        /* 清除链接复位事件标志*/
        __net_event_clr(p_net, USB_NET_EVENT_LINK_RESET);

        if (p_info->p_fn_link_reset) {
            ret = p_info->p_fn_link_reset(p_net);
            if (ret != USB_OK) {
                __USB_ERR_INFO("USB host net device link reset failed(%d)\r\n",ret);
                return ret;
            }
        }
        /* 链接复位后要处理链接改变*/
        __net_link_change_handle(p_net);
    }
    /* 链接改变事件*/
    if (evt & USB_NET_EVENT_LINK_CHANGE){
        __net_link_change_handle(p_net);
    }
    /* 设置接收模式事件*/
    if (evt & USB_NET_EVENT_SET_RX_MODE) {
        __net_set_rx_mode_handle(p_net);
    }
    return ret;
}

/**
 * \brief USB 主机网络设备处理函数
 *
 * \param[in] p_net USB 主机网络设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_process(struct usbh_net *p_net){
    int                  ret;
#if USB_OS_EN
    int                  ret_tmp;
#endif

    if (p_net == NULL) {
        return -USB_EINVAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_net->p_lock, USB_NET_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    ret = __net_event_prosess(p_net);
    if (ret != USB_OK) {
        goto __exit;
    }

    __net_rx_trps_adjust(p_net);

    ret = __net_rx_trps_submit(p_net);
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_net->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 获取当前存在的 USB 网络设备数量
 *
 * \param[out] p_n_dev 存储返回的设备数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_lib_ndev_get(uint8_t *p_n_dev){
    if (__g_usbh_net_lib.is_lib_deiniting == USB_TRUE) {
        return -USB_ENOINIT;
    }
    return usb_lib_ndev_get(&__g_usbh_net_lib.lib, p_n_dev);
}

/**
 * \brief USB 主机网络驱动库初始化
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_lib_init(void){
    if (usb_lib_is_init(&__g_usbh_net_lib.lib) == USB_TRUE) {
        return USB_OK;
    }

#if USB_MEM_RECORD_EN
    int ret = usb_lib_init(&__g_usbh_net_lib.lib, "usbh_net_mem");
#else
    int ret = usb_lib_init(&__g_userial_lib.lib);
#endif
    if (ret != USB_OK) {
        return ret;
    }

    usb_refcnt_init(&__g_usbh_net_lib.ref_cnt);

    __g_usbh_net_lib.queue_max        = 30 * 1518;
    __g_usbh_net_lib.is_lib_deiniting = USB_FALSE;

    return USB_OK;
}

/**
 * \brief 反初始化 USB 主机网络驱动库
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_lib_deinit(void){
    if (usb_lib_is_init(&__g_usbh_net_lib.lib) == USB_FALSE) {
        return USB_OK;
    }
    __g_usbh_net_lib.is_lib_deiniting = USB_TRUE;

    return usb_refcnt_put(&__g_usbh_net_lib.ref_cnt, __usbh_net_lib_release);
}

#if USB_MEM_RECORD_EN
/**
 * \brief 获取 USB 主机网络驱动内存记录
 *
 * \param[out] p_mem_record 内存记录
 *
 * \retval 成功返回 USB_OK
 */
int usbh_net_lib_mem_record_get(struct usb_mem_record *p_mem_record){
    return usb_lib_mem_record_get(&__g_usbh_net_lib.lib, p_mem_record);
}
#endif
