/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "adapter/notify/usb_notify.h"
#include "core/include/host/core/usbh.h"
#include "core/include/host/core/usbh_dev.h"
#include "common/refcnt/usb_refcnt.h"

/*******************************************************************************
 * Statement
 ******************************************************************************/
/* \brief 声明一个 USB 主机设备库结构体*/
struct usbh_dev_lib __g_usbh_dev_lib;

int usbh_dev_lib_ref_get(void);
int usbh_dev_lib_ref_put(void);
static int __dev_destroy(struct usbh_device *p_usb_dev);

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbh_core_lib __g_usb_host_lib;

extern int usbh_dev_enumerate(struct usbh_device *p_usb_dev,
                              int                 scheme);
extern int usbh_dev_unenumerate(struct usbh_device *p_usb_dev);
extern int usbh_dev_ep_enable(struct usbh_endpoint *p_ep);
extern int usbh_dev_ep_disable(struct usbh_endpoint *p_ep);
extern void usbh_dev_monitor_chk(uint16_t  vid,
                                 uint16_t  pid,
                                 uint32_t  dev_type,
                                 uint8_t   type);
extern int usbh_ep_hcpriv_init(struct usbh_endpoint *p_ep);
extern int usbh_ep_hcpriv_deinit(struct usbh_endpoint *p_ep);
extern void usbh_trp_sync_xfer_done(void *p_arg);
extern int usbh_trp_sync_xfer_wait(struct usbh_endpoint *p_ep, int time_out);
extern int usb_hc_xfer_request(struct usb_hc     *p_hc,
                               struct usbh_trp   *p_trp);
extern int usb_hc_xfer_cancel(struct usb_hc   *p_hc,
                              struct usbh_trp *p_trp);

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 填充等时传输请求包
 */
static int __iso_trp_fill(struct usbh_trp *p_trp){
    int                 is_out, max, n, len;
    struct usbh_device *p_dev = NULL;

    p_trp->status = -USB_EINPROGRESS;
    p_trp->act_len = 0;
    is_out         = USBH_EP_DIR_GET(p_trp->p_ep)? 0 : 1;

    p_dev = p_trp->p_ep->p_usb_dev;

    if (p_dev->status < USBH_DEV_CFG) {
        return -USB_ENODEV;
    }

    max = USBH_EP_MPS_GET(p_trp->p_ep);
    if (max <= 0) {
        __USB_ERR_INFO("USB host bogus EP %d %s (bad max packet %d)\r\n",
                        USBH_EP_ADDR_GET(p_trp->p_ep), is_out ? "out" : "in", max);
        return -USB_ESIZE;
    }
    /* "高带宽"模式，每微帧 1-3 个数据包*/
    if (p_dev->speed == USB_SPEED_HIGH) {
        /* 获取每微帧的事务数量*/
        int mult = 1 + ((max >> 11) & 0x03);
        /* 获取最大包大小*/
        max &= 0x07ff;
        /* 计算每微帧最大数据大小*/
        max *= mult;
    }

    if (p_trp->n_iso_packets <= 0) {
        return -USB_EILLEGAL;
    }

    for (n = 0; n < p_trp->n_iso_packets; n++) {
        /* 检查每个等时数据包的长度*/
        len = p_trp->p_iso_frame_desc[n].len;
        if (len < 0 || len > max) {
            return -USB_ESIZE;
        }

        p_trp->p_iso_frame_desc[n].status  = -USB_EBUSY;
        p_trp->p_iso_frame_desc[n].act_len = 0;
    }
    /* 如果提交者提供了假的标志，发出警告*/
    if (!(p_trp->flag & USBH_TRP_ISO_ASAP)) {
        //__USBH_DEV_TRACE("BOGUS TRP flags, %x --> 0x00000002\r\n", p_trp->flag);
    }

    if (p_trp->interval <= 0) {
        return -USB_EINVAL;
    }
    switch (p_dev->speed) {
        case USB_SPEED_HIGH:    /* 单位是微帧*/
            /* 注意 USB 处理 2^15*/
            if (p_trp->interval > (1024 * 8)) {
                p_trp->interval = 1024 * 8;
            }
            max = 1024 * 8;
        break;
        case USB_SPEED_FULL:    /* 单位是帧/毫秒*/
        case USB_SPEED_LOW:
            if (p_trp->interval > 1024) {
                p_trp->interval = 1024;
            }
            /* 注意 USB 和 OHCI 2^15*/
            max = 1024;
        break;
        default:
            return -USB_EINVAL;
    }
    /* 四舍五入为 2 的幂，不超过 max */
    p_trp->interval = min(max, 1 << usb_ilog2(p_trp->interval));
    p_trp->p_ep->interval = p_trp->interval;

    return USB_OK;
}

/**
 * \brief USB 获取额外的描述符
 *
 * \param[in]  p_buf   数据缓存
 * \param[in]  size    数据缓存大小
 * \param[in]  type    要找的描述符类型
 * \param[out] p_desc  返回找到的描述符
 *
 * \retval 成功返回 USB_OK
 */
int usb_extra_desc_get(uint8_t  *p_buf,
                       uint32_t  size,
                       uint8_t   type,
                       void    **p_desc){
    struct usb_desc_head *p_hdr = NULL;

    if ((p_buf == NULL) || (size < 2)) {
        return -USB_EINVAL;
    }

    while (size >= sizeof(struct usb_desc_head)) {
        p_hdr = (struct usb_desc_head *)p_buf;

        if (p_hdr->length < 2) {
            return -USB_EDATA;
        }

        if (p_hdr->descriptor_type == type) {
            *p_desc = p_hdr;
            return USB_OK;
        }

        p_buf += p_hdr->length;
        size  -= p_hdr->length;
    }
    return -USB_EDATA;
};

/**
 * \brief USB 设备功能匹配函数
 *
 * \param[in] p_usb_fun USB 功能接口结构体
 * \param[in] p_info    匹配信息
 *
 * \retval 匹配成功返回 USB_OK
 */
int usb_dev_func_match(struct usbh_function            *p_usb_fun,
                       const struct usb_dev_match_info *p_info){
    if ((p_usb_fun == NULL) || (p_info == NULL)) {
        return -USB_EINVAL;
    }
    if (p_info->flags & USB_DEV_PID_MATCH) {
        if (p_info->pid != USBH_DEV_PID_GET(p_usb_fun)) {
            return -USB_ENOTSUP;
        }
    }
    if (p_info->flags & USB_DEV_VID_MATCH) {
        if (p_info->vid != USBH_DEV_VID_GET(p_usb_fun)) {
            return -USB_ENOTSUP;
        }
    }
    if (p_info->flags & USB_INTF_CLASS_MATCH) {
        if (p_usb_fun->clss != p_info->intf_class) {
            return -USB_ENOTSUP;
        }
    }
    if (p_info->flags & USB_INTF_SUB_CLASS_MATCH) {
        if (p_usb_fun->sub_clss != p_info->intf_sub_class) {
            return -USB_ENOTSUP;
        }
    }
    if (p_info->flags & USB_INTF_PROCOTOL_MATCH) {
        if (p_usb_fun->proto != p_info->intf_protocol) {
            return -USB_ENOTSUP;
        }
    }
    return USB_OK;
}

/**
 * \brief USB 主机功能接口用户数据设置
 *
 * \param[in] p_fun      USB 主机功能接口
 * \param[in] p_usr_data 用户私有数据
 *
 * \retval 成功返回 USB_OK
 */
int usbh_func_usrdata_set(struct usbh_function *p_fun, void *p_usr_data){
    int ret = USB_OK;

    if ((p_fun == NULL) || (p_usr_data == NULL)) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_fun->p_usb_dev->p_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_fun->p_usr_priv = p_usr_data;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_fun->p_usb_dev->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机功能接口用户数据获取
 *
 * \param[in]  p_fun      USB 主机功能接口
 * \param[out] p_usr_data 获取到的用户数据
 *
 * \retval 成功返回 USB_OK
 */
int usbh_func_usrdata_get(struct usbh_function *p_fun, void **p_usr_data){
    int ret = USB_OK;

    if (p_fun == NULL) {
        return -USB_EINVAL;
    }

#if USB_OS_EN
    /* 等待设备空闲*/
    ret = usb_mutex_lock(p_fun->p_usb_dev->p_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_usr_data = p_fun->p_usr_priv;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_fun->p_usb_dev->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机功能接口驱动数据设置
 *
 * \param[in] p_fun      USB 主机功能接口
 * \param[in] p_drv_data 要设置的驱动数据
 *
 * \retval 成功返回 USB_OK
 */
int usbh_func_drvdata_set(struct usbh_function *p_usb_fun, void *p_drv_data){
    int ret = USB_OK;

    if ((p_usb_fun == NULL) || (p_drv_data == NULL)) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_usb_fun->p_usb_dev->p_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_usb_fun->p_drv_priv = p_drv_data;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_usb_fun->p_usb_dev->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机功能接口驱动数据获取
 *
 * \param[in]  p_fun      USB 主机功能接口
 * \param[out] p_drv_data 获取到的驱动数据
 *
 * \retval 成功返回 USB_OK
 */
int usbh_func_drvdata_get(struct usbh_function *p_usb_fun, void **p_drv_data){
    int ret = USB_OK;

    if (p_usb_fun == NULL) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_usb_fun->p_usb_dev->p_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_drv_data = p_usb_fun->p_drv_priv;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_usb_fun->p_usb_dev->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机功能接口获取
 *
 * \param[in] p_usb_fun USB 主机功能接口
 * \param[in] intf_num  接口编号
 * \param[in] alt_num   备用设置编号
 *
 * \retval 成功返回 USB 主机接口结构体
 */
struct usbh_interface *usbh_func_intf_get(struct usbh_function *p_usb_fun,
                                          uint8_t               intf_num,
                                          uint8_t               alt_num){
    struct usb_list_node  *p_node     = NULL;
    struct usb_list_node  *p_node_tmp = NULL;
    struct usbh_interface *p_intf     = NULL;

    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_usb_fun->intf_list){
        p_intf = usb_container_of(p_node, struct usbh_interface, node);

        if ((p_intf->p_desc->interface_number == intf_num) &&
                (p_intf->p_desc->alternate_setting == alt_num)) {
            return p_intf;
        }
    }
    __USB_ERR_INFO("USB host device function intf %d alt %d get failed\r\n", intf_num, alt_num);
    return NULL;
}

/**
 * \brief 通过接口号和备用设置号获取 USB 主机设备对应的接口结构体
 *
 * \param[in] p_usb_dev USB 主机设备
 * \param[in] intf_num  接口编号
 * \param[in] alt_num   接口的备用设置号
 *
 * \retval 成功返回找到的 USB 主机接口结构体。
 */
struct usbh_interface *usbh_dev_intf_get(struct usbh_device *p_usb_dev,
                                         uint8_t             intf_num,
                                         uint8_t             alt_num){
    uint8_t                i;
    struct usb_list_node  *p_node     = NULL;
    struct usb_list_node  *p_node_tmp = NULL;
    struct usbh_interface *p_intf     = NULL;

    if (p_usb_dev == NULL) {
        return NULL;
    }

    for(i = 0;i < p_usb_dev->cfg.n_funs; i++){
        usb_list_for_each_node_safe(p_node,
                                    p_node_tmp,
                                   &p_usb_dev->cfg.p_funs[i].intf_list){
            p_intf = usb_container_of(p_node, struct usbh_interface, node);

            if((p_intf->p_desc->interface_number == intf_num) &&
                    (p_intf->p_desc->alternate_setting == alt_num)){
                return p_intf;
            }
        }
    }
    __USB_ERR_INFO("USB host device intf %d alt %d get failed\r\n", intf_num, alt_num);
    return NULL;
}

/**
 * \brief 根据端点地址获取接口端点
 *
 * \param[in]  p_usb_fun USB 主机功能接口
 * \param[in]  ep_addr   端点地址
 * \param[out] p_ep_ret  返回找到的端点结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_intf_ep_get(struct usbh_interface *p_intf,
                     uint8_t                ep_addr,
                     struct usbh_endpoint **p_ep_ret){
    struct usbh_endpoint *p_ep = NULL;
    unsigned int          i;

    if (p_intf == NULL) {
        return -USB_EINVAL;
    }

    /* 遍历接口备用设置所有端点*/
    for (i = 0; i < USBH_INTF_NEP_GET(p_intf); ++i) {
        p_ep = &p_intf->p_eps[i];
        /* 匹配端点地址*/
        if (p_ep->p_ep_desc->endpoint_address == ep_addr) {
            *p_ep_ret = p_ep;
            return USB_OK;
        }
    }
    return -USB_ENODEV;
}

/**
 * \brief USB 主机传输请求包提交
 *
 * \param[in] p_trp 要提交的传输请求包
 *
 * \retval 成功返回 USB_OK
 */
int usbh_trp_submit(struct usbh_trp *p_trp){
    int ret;
#if USB_OS_EN
    int ret_tmp;
#endif

    if (p_trp == NULL) {
        return -USB_EINVAL;
    }
    if ((p_trp->p_ep == NULL) || (p_trp->p_ep->p_usb_dev == NULL)) {
        return -USB_EILLEGAL;
    }

    struct usbh_device *p_usb_dev = p_trp->p_ep->p_usb_dev;
#if USB_OS_EN
    ret = usb_mutex_lock(p_usb_dev->p_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "%d\r\n", ret);
        return ret;
    }
#endif
    /* 如果端点是控制端点且控制请求包是空的，返回错误*/
    if ((USBH_EP_TYPE_GET(p_trp->p_ep) == USB_EP_TYPE_CTRL) &&
            (p_trp->p_ctrl == NULL)) {
        ret = -USB_EILLEGAL;
        goto __exit;
    }

    /* 如果端点的最大包尺寸小于等于0，返回错误*/
    if (USBH_EP_MPS_GET(p_trp->p_ep) <= 0) {
        ret = -USB_EILLEGAL;
        __USB_ERR_INFO("endpoint max packet size illegal(%d)\r\n", USBH_EP_MPS_GET(p_trp->p_ep));
        goto __exit;
    }

    /* 如果是等时端点，填充等时传输包的特定的字段*/
    if ((USBH_EP_TYPE_GET(p_trp->p_ep) == USB_EP_TYPE_ISO)) {
        __iso_trp_fill(p_trp);
    }

    /* 如果设备不在连接状态，解锁返回错误*/
    if (!USBH_IS_DEV_INJECT(p_trp->p_ep->p_usb_dev)) {
        ret = -USB_ENODEV;
        goto __exit;
    }

    /* 使能端点*/
    ret = usbh_dev_ep_enable(p_trp->p_ep);
    if (ret != USB_OK) {
        goto __exit;
    }

    ret = usb_hc_xfer_request(p_usb_dev->p_hc, p_trp);

__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_usb_dev->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "%d\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 主机传输请求包取消
 *
 * \param[in] p_trp 要取消的传输请求包
 *
 * \retval 成功返回 USB_OK
 */
int usbh_trp_xfer_cancel(struct usbh_trp *p_trp){
    int                 ret;
    struct usbh_device *p_usb_dev = NULL;
#if USB_OS_EN
    int                 ret_tmp;
#endif

    if (p_trp == NULL) {
        return -USB_EINVAL;
    }
    if ((p_trp->p_ep == NULL) || (p_trp->p_ep->p_usb_dev == NULL)) {
        return -USB_EILLEGAL;
    }

    p_usb_dev = p_trp->p_ep->p_usb_dev;

#if USB_OS_EN
    ret = usb_mutex_lock(p_usb_dev->p_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "%d\r\n", ret);
        return ret;
    }
#endif
    ret = usb_hc_xfer_cancel(p_usb_dev->p_hc, p_trp);
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_usb_dev->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "%d\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 主机提交传输请求包
 *
 * \param[in] p_ep     使用的端点
 * \param[in] p_ctrl   控制传输请求结构体
 * \param[in] p_data   要写/读的数据缓存
 * \param[in] len      要写/读的数据长度
 * \param[in] time_out 超时时间
 * \param[in] flag     标志
 *
 * \retval 成功返回实际传输长度，失败返回传输请求包状态
 */
int usbh_trp_sync_xfer(struct usbh_endpoint  *p_ep,
                       struct usb_ctrlreq    *p_ctrl,
                       void                  *p_data,
                       int                    len,
                       int                    time_out,
                       int                    flag){
    int             ret, ret_tmp;
    struct usbh_trp trp;

    if ((p_ep == NULL) ||
            ((p_ctrl == NULL) && (p_data == NULL)) ||
             (time_out == 0)) {
        return -USB_EINVAL;
    }

    /* 填充传输请求包*/
    trp.p_ep      = p_ep;
    trp.p_ctrl    = p_ctrl;
    trp.p_data    = p_data;
    trp.len       = len;
    trp.p_fn_done = usbh_trp_sync_xfer_done;      /* 传输完成回调函数*/
    trp.p_arg     = (void *)p_ep;                 /* 传输完成回调函数参数*/
    trp.act_len   = 0;                            /* 传输请求包的实际传输长度*/
    trp.status    = -USB_EINPROGRESS;             /* 本次传输状态*/
    trp.flag      = flag;

    /* 提交传输请求包*/
    ret = usbh_trp_submit(&trp);
    if (ret != USB_OK) {
        return ret;
    }

    /* 等待传输完成*/
    ret = usbh_trp_sync_xfer_wait(p_ep, time_out);
    if (ret != USB_OK) {
        goto __failed;
    }

    if (trp.status == USB_OK) {
        ret = trp.act_len;
    } else {
        ret = trp.status;
    }

    return ret;
__failed:
    /* 取消传输请求包*/
    ret_tmp = usbh_trp_xfer_cancel(&trp);
    if (ret_tmp != USB_OK) {
        __USB_ERR_INFO("USB host TRP cancel failed(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
    /* 等待取消传输请求包完成*/
    usbh_trp_sync_xfer_wait(p_ep, time_out);

    return ret;
}

/**
 * \brief USB 主机同步控制传输
 *
 * \param[in] p_ep     使用控制传输的端点
 * \param[in] type     传输方向 | 请求类型 | 请求目标
 * \param[in] req      具体的 USB 请求
 * \param[in] val      参数
 * \param[in] idx      索引
 * \param[in] len      要写/读的数据长度
 * \param[in] p_data   要写/读的数据缓存
 * \param[in] time_out 超时时间
 * \param[in] flag     标志
 *
 * \retval 如果没有数据，成功返回 0，如果有数据，成功返回传输的数据大小
 */
int usbh_ctrl_trp_sync_xfer(struct usbh_endpoint *p_ep,
                            uint8_t               type,
                            uint8_t               req,
                            uint16_t              val,
                            uint16_t              idx,
                            uint16_t              len,
                            void                 *p_data,
                            int                   time_out,
                            int                   flag){
    if (p_ep == NULL) {
        return -USB_EINVAL;
    }
    if (p_ep->p_usb_dev == NULL) {
        return -USB_EILLEGAL;
    }

    /* 填充设备的控制传输请求结构体*/
    p_ep->p_usb_dev->p_ctrl->request      = req;                     /* 具体的USB请求*/
    p_ep->p_usb_dev->p_ctrl->request_type = type;                    /* 数据方向，数据类型，请求目标*/
    p_ep->p_usb_dev->p_ctrl->value        = USB_CPU_TO_LE16(val);    /* 参数*/
    p_ep->p_usb_dev->p_ctrl->index        = USB_CPU_TO_LE16(idx);    /* 索引*/
    p_ep->p_usb_dev->p_ctrl->length       = USB_CPU_TO_LE16(len);    /* 请求的数据长度*/
    /* 发送传输请求包*/
    return usbh_trp_sync_xfer(p_ep,
                              p_ep->p_usb_dev->p_ctrl,
                              p_data,
                              len,
                              time_out,
                              flag);
}

/**
 * \brief USB 主机设备备用接口设置
 *
 * \param[in] p_ufun   USB 接口功能结构体
 * \param[in] intf_num 接口编号
 * \param[in] alt_num  接口的备用设置号
 *
 * \retval 成功返回 USB_OK
 */
int usbh_func_intf_set(struct usbh_function *p_usb_fun,
                       uint8_t               intf_num,
                       uint8_t               alt_num){
    int                    ret;
    struct usb_list_node  *p_node     = NULL;
    struct usb_list_node  *p_node_tmp = NULL;
    struct usbh_interface *p_intf     = NULL;
    uint16_t               i;

    if (p_usb_fun == NULL) {
        return -USB_EINVAL;
    }

    /* 遍历所有接口*/
    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_usb_fun->intf_list){
        p_intf = usb_container_of(p_node, struct usbh_interface, node);

        if ((USBH_INTF_NUM_GET(p_intf) == intf_num) &&
                (USBH_INTF_ALT_NUM_GET(p_intf) == alt_num)){
            goto __find;
        }
    }
    return -USB_ENODEV;
__find:
    /* 发起控制传输，设置接口*/
    ret = usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                   USB_DIR_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_TAG_INTERFACE,
                                   USB_REQ_SET_INTERFACE,
                                   alt_num,
                                   intf_num,
                                   0,
                                   NULL,
                                 __g_usbh_dev_lib.xfer_time_out,
                                   0);
    if (ret == 0) {
#if USB_OS_EN
        ret = usb_mutex_lock(p_usb_fun->p_usb_dev->p_lock, USBH_DEV_MUTEX_TIMEOUT);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexLockErr, "%d\r\n", ret);
            return ret;
        }
#endif
        /* 把接口所有端点赋值给所属设备结构体*/
        for (i = 0; i < USBH_INTF_NEP_GET(p_intf); i++) {
            if (USBH_EP_DIR_GET(&p_intf->p_eps[i]) == USB_DIR_IN) {
                p_usb_fun->p_usb_dev->p_ep_in[USBH_EP_ADDR_GET(&p_intf->p_eps[i])] = &p_intf->p_eps[i];
            } else {
                p_usb_fun->p_usb_dev->p_ep_out[USBH_EP_ADDR_GET(&p_intf->p_eps[i])] = &p_intf->p_eps[i];
            }
        }
#if USB_OS_EN
        ret = usb_mutex_unlock(p_usb_fun->p_usb_dev->p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "%d\r\n", ret);
        }
#endif
    }
    return ret;
}

/**
 * \brief USB 主机设备释放函数
 */
static void __dev_release(usb_refcnt *p_ref_cnt){
    struct usbh_device *p_usb_dev = NULL;
    int                 ret;

    p_usb_dev = USB_CONTAINER_OF(p_ref_cnt, struct usbh_device, ref_cnt);

    usbh_dev_unenumerate(p_usb_dev);

    ret = __dev_destroy(p_usb_dev);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB host device destroy failed(%d)\r\n", ret);
    }
}

/**
 * \brief USB 主机设备引用
 *
 * \param[in] p_usb_dev USB 主机设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_ref_get(struct usbh_device *p_usb_dev){
    if (p_usb_dev == NULL) {
        return -USB_EINVAL;
    }
    if (!USBH_IS_DEV_INJECT(p_usb_dev)) {
        return -USB_ENODEV;
    }

    return usb_refcnt_get(&p_usb_dev->ref_cnt);
}

/**
 * \brief USB 主机设备取消引用
 *
 * \param[in] p_usb_dev USB 主机设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_ref_put(struct usbh_device *p_usb_dev){
    if (p_usb_dev == NULL) {
        return -USB_EINVAL;
    }

    return usb_refcnt_put(&p_usb_dev->ref_cnt, __dev_release);
}

/**
 * \brief USB 主机设备类型设置
 *
 * \param[in] p_usb_dev USB 主机设备
 * \param[in] dev_type  要设置的设备类型
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_type_set(struct usbh_device *p_usb_dev, uint32_t dev_type){
    int ret = USB_OK;

    if (p_usb_dev == NULL) {
        return -USB_EINVAL;
    }

#if USB_OS_EN
    /* 等待设备空闲*/
    ret = usb_mutex_lock(p_usb_dev->p_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    p_usb_dev->dev_type = p_usb_dev->dev_type | dev_type;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_usb_dev->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 检查 USB 主机设备是否在连接状态
 *
 * \param[in] p_usb_dev 要检查的 USB 主机设备
 *
 * \retval 连接返回 USB_TRUE，没连接返回 USB_FALSE
 */
usb_bool_t usbh_dev_is_connect(struct usbh_device *p_usb_dev){
    struct usbh_hub_basic *p_hub_basic = NULL;
    usb_bool_t             is_connect  = USB_FALSE;

    if (p_usb_dev == NULL) {
        return USB_FALSE;
    }

    p_hub_basic = p_usb_dev->p_hub_basic;

    usbh_basic_hub_port_connect_chk(p_hub_basic, p_usb_dev->port, &is_connect);

    return is_connect;
}

/**
 * \brief USB 主机设备创建
 */
static int __dev_create(struct usb_hc         *p_hc,
                        struct usbh_hub_basic *p_hub_basic,
                        uint8_t                port,
                        struct usbh_device   **p_usb_dev_ret){
    struct usbh_device *p_usb_dev = NULL;
    int                 ret;

    p_usb_dev = (struct usbh_device *)usb_lib_malloc(&__g_usb_host_lib.lib, sizeof(struct usbh_device));
    if (p_usb_dev == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_usb_dev, 0, sizeof(struct usbh_device));

    p_usb_dev->p_dev_desc = (struct usb_device_desc *)usb_lib_malloc(&__g_usb_host_lib.lib,
                                                                            sizeof(struct usb_device_desc));
    if (p_usb_dev->p_dev_desc == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_usb_dev->p_dev_desc, 0, sizeof(struct usb_device_desc));
#if USB_OS_EN
    p_usb_dev->p_lock = usb_lib_mutex_create(&__g_usb_host_lib.lib);
    if (p_usb_dev->p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        return -USB_EPERM;
    }
#endif

    p_usb_dev->p_ctrl = (struct usb_ctrlreq *)usb_lib_malloc(&__g_usb_host_lib.lib, sizeof(struct usb_ctrlreq));
    if (p_usb_dev->p_ctrl == NULL) {
        return -USB_ENOMEM;
    }

    p_usb_dev->p_hc        = p_hc;                 /* 填充USB主机结构体*/
    p_usb_dev->p_hub_basic = p_hub_basic;          /* 填充集线器结构体*/
    p_usb_dev->port        = port;                 /* 填充所属集线器端口号*/
    p_usb_dev->addr        = 0;                    /* 设备地址先设置为0*/
    p_usb_dev->speed       = USB_SPEED_UNKNOWN;    /* 设备速度未确定*/
    p_usb_dev->status      = 0;                    /* 设备状态*/

    usb_refcnt_init(&p_usb_dev->ref_cnt);

    p_usb_dev->ep0.p_usb_dev  = p_usb_dev;             /* 填充端点0结构体的USB设备结构体*/
    p_usb_dev->ep0.p_ep_desc  = &p_usb_dev->ep0_desc;  /* 填充端点0结构体中的端点0描述符定义*/
    p_usb_dev->ep0.is_enabled = USB_FALSE;             /* 设备端点0未使能*/
    p_usb_dev->ep0.band_width = 0;                     /* 设置端点0带宽*/
    p_usb_dev->ep0.p_hw_priv  = NULL;
    p_usb_dev->ep0.p_hc_priv  = NULL;

    /* 初始化私有数据域*/
    ret = usbh_ep_hcpriv_init(&p_usb_dev->ep0);
    if (ret != USB_OK) {
        return ret;
    }

    p_usb_dev->ep0_desc.descriptor_type  = USB_DT_ENDPOINT;      /* 端点描述符*/
    p_usb_dev->ep0_desc.length           = 7;                    /* 描述符字节长度*/
    p_usb_dev->ep0_desc.endpoint_address = 0;                    /* 端点地址*/
    p_usb_dev->ep0_desc.max_packet_size  = 64;                   /* 最大包长度*/
    p_usb_dev->ep0_desc.attributes       = USB_EP_TYPE_CTRL;     /* 控制端点*/
    p_usb_dev->ep0_desc.interval         = 0;                    /* 轮询该端点的时间*/
    /* 设置端点0*/
    p_usb_dev->p_ep_in[0]  = &p_usb_dev->ep0;
    p_usb_dev->p_ep_out[0] = &p_usb_dev->ep0;

    /* 设备状态设置为连接状态*/
    p_usb_dev->status |= USBH_DEV_INJECT;

    *p_usb_dev_ret = p_usb_dev;

    return USB_OK;
}

/**
 * \brief 销毁 USB 设备结构体
 *
 * \param[in] p_usb_dev 要销毁的 USB 设备结构体
 *
 * \retval 成功返回 USB_OK
 */
static int __dev_destroy(struct usbh_device *p_usb_dev){
    int ret, i;

    p_usb_dev->status |= USBH_DEV_EJECT;

    /* 停止所有端点*/
    for (i = 0; i < 16; i++) {
        /* 禁用主机输入端点*/
        if (p_usb_dev->p_ep_in[i]) {
            ret = usbh_dev_ep_disable(p_usb_dev->p_ep_in[i]);
            if (ret != USB_OK) {
                return ret;
            }

            p_usb_dev->p_ep_in[i] = NULL;
        }
        /* 禁用主机输出端点*/
        if (p_usb_dev->p_ep_out[i]) {
            ret = usbh_dev_ep_disable(p_usb_dev->p_ep_out[i]);
            if (ret != USB_OK) {
                return ret;
            }

            p_usb_dev->p_ep_out[i] = NULL;
        }
    }
    /* 端点 0 的主机输入输出端点为空*/
    p_usb_dev->p_ep_in[0]  = NULL;
    p_usb_dev->p_ep_out[0] = NULL;

    if (p_usb_dev->p_ctrl) {
        usb_lib_mfree(&__g_usb_host_lib.lib, p_usb_dev->p_ctrl);
    }

    if (p_usb_dev->p_dev_desc) {
        usb_lib_mfree(&__g_usb_host_lib.lib, p_usb_dev->p_dev_desc);
    }

    usbh_ep_hcpriv_deinit(&p_usb_dev->ep0);

#if USB_OS_EN
    if (p_usb_dev->p_lock) {
        ret = usb_lib_mutex_destroy(&__g_usb_host_lib.lib, p_usb_dev->p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
            return ret;
        }
    }
#endif

    usb_lib_mfree(&__g_usb_host_lib.lib, p_usb_dev);

    return ret;
}

/**
 * \brief USB 主机设备插入函数
 */
static int __dev_inject(struct usbh_device *p_usb_dev){
    int     ret;
    uint8_t i;

    if (p_usb_dev->cfg.p_funs) {
        for (i = 0; i < p_usb_dev->cfg.n_funs; i++) {
            ret = usbh_dev_ref_get(p_usb_dev);
            if (ret != USB_OK) {
                __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
                continue;
            }
            usbh_dev_func_sta_notify(&p_usb_dev->cfg.p_funs[i], USBH_DEV_INJECT);
        }
    }

#if USB_OS_EN
    ret = usb_mutex_lock(__g_usbh_dev_lib.p_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    usb_list_node_add_tail(&p_usb_dev->node, &__g_usbh_dev_lib.dev_list);

    __g_usbh_dev_lib.n_dev++;

    usbh_dev_lib_ref_get();

#if USB_OS_EN
    ret = usb_mutex_unlock(__g_usbh_dev_lib.p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    /* 设备监控检查 */
    usbh_dev_monitor_chk(p_usb_dev->p_dev_desc->id_vendor,
                         p_usb_dev->p_dev_desc->id_product,
                         p_usb_dev->dev_type,
                         USBH_DEV_INJECT);
    return USB_OK;
}


/**
 * \brief USB 主机设备弹出函数
 */
static int __dev_eject(struct usbh_device *p_usb_dev){
    int     ret;
    uint8_t i;

    /* 设备的状态设置为未连接*/
    p_usb_dev->status &= ~USBH_DEV_INJECT;

#if USB_OS_EN
    ret = usb_mutex_lock(__g_usbh_dev_lib.p_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 在设备链表中删除该设备*/
    usb_list_node_del(&p_usb_dev->node);
    if (__g_usbh_dev_lib.n_dev) {
        __g_usbh_dev_lib.n_dev--;
    }
    usbh_dev_lib_ref_put();

#if USB_OS_EN
    ret = usb_mutex_unlock(__g_usbh_dev_lib.p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif


    if (p_usb_dev->cfg.p_funs) {

        for (i = 0; i < p_usb_dev->cfg.n_funs; i++) {
            usbh_dev_func_sta_notify(&p_usb_dev->cfg.p_funs[i], USBH_DEV_EJECT);

            ret = usbh_dev_ref_put(p_usb_dev);
            if (ret != USB_OK) {
                __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
                return ret;
            }
        }
    }
    /* 启动设备监控*/
    usbh_dev_monitor_chk(p_usb_dev->p_dev_desc->id_vendor,
                         p_usb_dev->p_dev_desc->id_product,
                         p_usb_dev->dev_type,
                         USBH_DEV_EJECT);
    return USB_OK;
}


/**
 * \brief USB 主机基础集线器端口设备连接
 *
 * \param[in] p_hub_basic 基本集线器
 * \param[in] port_num    集线器端口号
 *
 * \retval 成功返回 USB_OK
 */
int usbh_basic_hub_dev_connect(struct usbh_hub_basic *p_hub_basic,
                               uint8_t                port_num){
    struct usbh_device *p_usb_dev = NULL;
    struct usb_hc      *p_hc      = NULL;
    int                 i, ret;
    usb_bool_t          is_port_connect;

    if (p_hub_basic == NULL) {
        return -USB_EINVAL;
    }

    /* 获取 USB 主机*/
    if (p_hub_basic->p_usb_fun != NULL){
        p_hc = p_hub_basic->p_usb_fun->p_usb_dev->p_hc;
    } else {
        p_hc = usb_container_of(p_hub_basic, struct usb_hc, root_hub);
    }

    ret = __dev_create(p_hc, p_hub_basic, port_num, &p_usb_dev);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB host device create failed(%d)\r\n", ret);
        goto __failed;
    }

    /* 设备枚举*/
    for (i = 0; i < 4; i++) {
        ret = usbh_dev_enumerate(p_usb_dev, i & 0x01);
        if (ret == USB_OK) {
            break;
        }
        /* 枚举失败，检查设备是否连接*/
        ret = usbh_basic_hub_port_connect_chk(p_usb_dev->p_hub_basic, p_usb_dev->port, &is_port_connect);
        if (ret == USB_OK) {
            if (is_port_connect == USB_FALSE) {
                ret = -USB_ENODEV;
                __USB_ERR_INFO("USB host device hub port disconnect\r\n");
                break;
            }
        }

        usbh_dev_ep_reset(&p_usb_dev->ep0);

        usb_mdelay(100);
    }

    if (ret != USB_OK) {
        goto __failed;
    }

    if (p_hub_basic->p_usb_fun != NULL){
        /* 普通集线器设备的端口号是由 1 开始的，所以赋值结构体时需要减 1 */
        p_hub_basic->p_ports[port_num - 1] = p_usb_dev;
    } else {
        /* 根集线器设备的端口号是由 0 开始的 */
        p_hub_basic->p_ports[port_num] = p_usb_dev;
    }
    __USB_INFO_NEW_LINE();

    __USB_INFO("USB host new device (vid:%04x_pid:%04x)\r\n",
                  USB_CPU_TO_LE16(p_usb_dev->p_dev_desc->id_vendor),
                  USB_CPU_TO_LE16(p_usb_dev->p_dev_desc->id_product));

    if (p_usb_dev->p_mft) {
        __USB_INFO("    manufacturer: %s\r\n", p_usb_dev->p_mft);
    }

    if (p_usb_dev->p_pdt) {
        __USB_INFO("    product: %s\r\n", p_usb_dev->p_pdt);
    }

    if (p_usb_dev->p_snum) {
        __USB_INFO("    serial: %s\r\n", p_usb_dev->p_snum);
    }

    ret = __dev_inject(p_usb_dev);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB host device inject failed(%d)\r\n", ret);
        goto __failed;
    }

    return USB_OK;
__failed:
    usbh_dev_unenumerate(p_usb_dev);

    __dev_destroy(p_usb_dev);

    return ret;
}

/**
 * \brief USB 主机基础集线器端口设备断开
 *
 * \param[in] p_hub_basic 基本集线器
 * \param[in] port_num    集线器端口号
 *
 * \retval 成功返回 USB_OK
 */
int usbh_basic_hub_dev_disconnect(struct usbh_hub_basic *p_hub_basic,
                                  uint8_t                port_num){
    int                 ret       = USB_OK;
    int                 port_tmp;
    struct usbh_device *p_usb_dev = NULL;

    if (p_hub_basic == NULL) {
        return -USB_EINVAL;
    }

    if (p_hub_basic->p_usb_fun != NULL){
        /* 普通集线器设备的端口号是由 1 开始的，获取结构体时需要减 1 */
        port_tmp = port_num - 1;
    } else {
        /* 根集线器设备的端口号是由 0 开始的 */
        port_tmp = port_num;
    }

    p_usb_dev = p_hub_basic->p_ports[port_tmp];
    p_hub_basic->p_ports[port_tmp] = NULL;

    if (p_usb_dev) {
        __USB_INFO("USB host device (vid&%04x_pid&%04x) removed\r\n",
                    p_usb_dev->p_dev_desc->id_vendor, p_usb_dev->p_dev_desc->id_product);

        ret = __dev_eject(p_usb_dev);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host device eject failed(%d)\r\n", ret);
            return ret;
        }

        usbh_dev_ref_put(p_usb_dev);
    }

    return ret;
}

/**
 * \brief USB 主机基础集线器端口连接检查
 *
 * \param[in]  p_hub_basic       USB 主机基础集线器
 * \param[in]  port_num          集线器端口号
 * \param[out] p_is_port_connect 返回连接的结果
 *
 * \retval 成功返回 USB_OK
 */
int usbh_basic_hub_port_connect_chk(struct usbh_hub_basic *p_hub_basic,
                                    uint8_t                port_num,
                                    usb_bool_t            *p_is_port_connect){
    if (p_hub_basic->p_fn_hub_pc_check) {
        *p_is_port_connect = p_hub_basic->p_fn_hub_pc_check(p_hub_basic, port_num);
        return USB_OK;
    }
    return -USB_ENOTSUP;
}

/**
 * \brief USB 主机基础集线器端口速度获取函数
 *
 * \param[in]  p_hub_basic USB 主机基础集线器
 * \param[in]  port_num    端口号
 * \param[out] p_speed     返回获取到端口速度
 *
 * \retval 成功返回 USB_OK
 */
int usbh_basic_hub_ps_get(struct usbh_hub_basic *p_hub_basic,
                          uint8_t                port_num,
                          uint8_t               *p_speed){
    int ret;

    if (p_hub_basic->p_fn_hub_ps_get != NULL) {
        ret = p_hub_basic->p_fn_hub_ps_get(p_hub_basic, port_num, p_speed);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host hub port speed get failed(%d)\r\n", ret);
        }
        return ret;
    }
    return -USB_ENOTSUP;
}

/**
 * \brief USB 主机基础集线器复位操作函数
 *
 * \param[in] p_hub_basic USB 主机基础集线器
 * \param[in] is_port     是否复位端口
 * \param[in] port_mum    要复位端口号
 *
 * \retval 成功返回 USB_OK
 */
int usbh_basic_hub_reset(struct usbh_hub_basic *p_hub_basic,
                         usb_bool_t             is_port,
                         uint8_t                port_mum){
    int ret;

    if (p_hub_basic->p_fn_hub_reset) {
        ret = p_hub_basic->p_fn_hub_reset(p_hub_basic, is_port, port_mum);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host hub reset failed(%d)\r\n", ret);
        }
        return ret;
    }
    return -USB_ENOTSUP;
}

/**
 * \brief USB 主机集线器事件添加函数
 *
 * \param[in] p_evt USB 主机集线器事件
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_evt_add(struct usbh_hub_evt *p_evt){
    int ret = USB_OK;;

    if (p_evt == NULL) {
        return -USB_EINVAL;
    }

    /* 已将在链表内了，返回*/
    if (!usb_list_node_is_empty(&p_evt->evt_node)) {
        return USB_OK;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(__g_usbh_dev_lib.p_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    usb_list_node_add_tail(&p_evt->evt_node, &__g_usbh_dev_lib.hub_evt_list);

#if USB_OS_EN
    ret = usb_mutex_unlock(__g_usbh_dev_lib.p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }

    ret = usb_sem_give(__g_usbh_dev_lib.p_hub_evt_sem);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(SemGiveErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机集线器事件删除函数
 *
 * \param[in] p_evt USB 主机集线器事件
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_evt_del(struct usbh_hub_evt *p_evt){
    int ret = USB_OK;;

    if (p_evt == NULL) {
        return -USB_EINVAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(__g_usbh_dev_lib.p_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (!usb_list_node_is_empty(&p_evt->evt_node)) {
        usb_list_node_del(&p_evt->evt_node);
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(__g_usbh_dev_lib.p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif

    return ret;
}

/**
 * \brief USB 主机集线器事件处理函数
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_evt_process(void){
    int                    ret;
    struct usbh_hub_evt   *p_port_evt  = NULL;
    struct usbh_hub_basic *p_hub_basic = NULL;

#if USB_OS_EN
    ret = usb_sem_take(__g_usbh_dev_lib.p_hub_evt_sem, USB_WAIT_FOREVER);
    if (ret != USB_OK) {
        return ret;
    }
#endif
    /* 处理链表上所有事件*/
    while (!usb_list_head_is_empty(&__g_usbh_dev_lib.hub_evt_list)) {
        p_port_evt  = usb_container_of(__g_usbh_dev_lib.hub_evt_list.p_next, struct usbh_hub_evt, evt_node);

        p_hub_basic = usb_container_of(p_port_evt, struct usbh_hub_basic, evt);
#if USB_OS_EN
        ret = usb_mutex_lock(__g_usbh_dev_lib.p_lock, USBH_DEV_MUTEX_TIMEOUT);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
        usb_list_node_del(&p_port_evt->evt_node);
#if USB_OS_EN
        ret = usb_mutex_unlock(__g_usbh_dev_lib.p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
        /* 集线器处理函数*/
        if (p_hub_basic->p_fn_hub_handle != NULL){
            ret = p_hub_basic->p_fn_hub_handle(p_hub_basic, p_port_evt);
            if (ret != USB_OK) {
                __USB_ERR_INFO("USB host hub handle failed(%d)\r\n", ret);
                return ret;
            }
        }
    }
    return USB_OK;
}

/**
 * \brief 获取当前存在的 USB 主机设备数量
 *
 * \param[out] p_n_dev 返回设备数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_lib_ndev_get(uint32_t *p_n_dev){
    int ret = USB_OK;

    if ((__g_usbh_dev_lib.is_lib_init != USB_TRUE) ||
            (__g_usbh_dev_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    if (p_n_dev == NULL) {
        return -USB_EINVAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(__g_usbh_dev_lib.p_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_n_dev = __g_usbh_dev_lib.n_dev;
#if USB_OS_EN
    ret = usb_mutex_unlock(__g_usbh_dev_lib.p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机设备传输超时时间获取
 *
 * \param[out] p_time_out 返回的超时时间
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_xfer_timeout_get(int *p_time_out){
    int ret = USB_OK;

    if ((__g_usbh_dev_lib.is_lib_init != USB_TRUE) ||
            (__g_usbh_dev_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    if (p_time_out == NULL) {
        return -USB_EINVAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(__g_usbh_dev_lib.p_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_time_out = __g_usbh_dev_lib.xfer_time_out;
#if USB_OS_EN
    ret = usb_mutex_unlock(__g_usbh_dev_lib.p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机设备传输超时时间设置
 *
 * \param[in] time_out 要设置的超时时间
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_xfer_timeout_set(int time_out){
    int ret = USB_OK;

    if ((__g_usbh_dev_lib.is_lib_init != USB_TRUE) ||
            (__g_usbh_dev_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    if ((time_out < 0) && (time_out != USB_WAIT_FOREVER)) {
        return -USB_EINVAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(__g_usbh_dev_lib.p_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    __g_usbh_dev_lib.xfer_time_out = time_out;
#if USB_OS_EN
    ret = usb_mutex_unlock(__g_usbh_dev_lib.p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机设备库释放函数
 */
static void __dev_lib_release(int *p_ref){
#if USB_OS_EN
    int ret;

    ret = usb_lib_sem_destroy(&__g_usb_host_lib.lib, __g_usbh_dev_lib.p_hub_evt_sem);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(SemDelErr, "(%d)\r\n", ret);
    }

    ret = usb_lib_mutex_destroy(&__g_usb_host_lib.lib, __g_usbh_dev_lib.p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
    }

    ret = usb_lib_mutex_destroy(&__g_usb_host_lib.lib, __g_usbh_dev_lib.p_monitor_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
    }
#endif
    __g_usbh_dev_lib.is_lib_init = USB_FALSE;
}

/**
 * \brief USB 主机设备库引用
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_lib_ref_get(void){
    if ((__g_usbh_dev_lib.is_lib_init != USB_TRUE) ||
            (__g_usbh_dev_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    return usb_refcnt_get(&__g_usbh_dev_lib.ref_cnt);
}

/**
 * \brief USB 主机设备取消引用
 *
 * \param[in] p_usb_dev USB 主机设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_lib_ref_put(void){
    return usb_refcnt_put(&__g_usbh_dev_lib.ref_cnt, __dev_lib_release);
}

/**
 * \brief USB 主机设备库初始化
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_lib_init(void){
#if USB_OS_EN
    int ret, ret_tmp;
#endif

    if (__g_usbh_dev_lib.is_lib_init == USB_TRUE) {
        return USB_OK;
    }

    memset(&__g_usbh_dev_lib, 0, sizeof(struct usbh_dev_lib));

#if USB_OS_EN
    /* 初始化 USB 设备互斥锁*/
    __g_usbh_dev_lib.p_lock = usb_lib_mutex_create(&__g_usb_host_lib.lib);
    if (__g_usbh_dev_lib.p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        return -USB_ENOMEM;
    }

    /* 初始化 USB 设备监控器互斥锁*/
    ret = -USB_ENOMEM;
    __g_usbh_dev_lib.p_monitor_lock = usb_lib_mutex_create(&__g_usb_host_lib.lib);
    if (__g_usbh_dev_lib.p_monitor_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        goto __failed;
    }

    /* 初始化 USB 设备事件信号量*/
    __g_usbh_dev_lib.p_hub_evt_sem = usb_lib_sem_create(&__g_usb_host_lib.lib);
    if (__g_usbh_dev_lib.p_hub_evt_sem == NULL) {
        __USB_ERR_TRACE(SemCreateErr, "\r\n");
        goto __failed;
    }

#endif
    /* 初始化 USB 设备链表*/
    usb_list_head_init(&__g_usbh_dev_lib.dev_list);
    /* 初始化 USB 设备监控器链表*/
    usb_list_head_init(&__g_usbh_dev_lib.monitor_list);
    /* 初始化 USB 设备事件链表*/
    usb_list_head_init(&__g_usbh_dev_lib.hub_evt_list);
    /* 枚举超时时间*/
    __g_usbh_dev_lib.xfer_time_out = 5000;
    /* 初始化引用计数*/
    usb_refcnt_init(&__g_usbh_dev_lib.ref_cnt);

    __g_usbh_dev_lib.is_lib_init      = USB_TRUE;
    __g_usbh_dev_lib.is_lib_deiniting = USB_FALSE;

    return USB_OK;
#if USB_OS_EN
__failed:
    if (__g_usbh_dev_lib.p_lock){
        ret_tmp = usb_lib_mutex_destroy(&__g_usb_host_lib.lib, __g_usbh_dev_lib.p_lock);
        if (ret_tmp != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret_tmp);
        }
    }
    if (__g_usbh_dev_lib.p_monitor_lock){
        ret_tmp = usb_lib_mutex_destroy(&__g_usb_host_lib.lib, __g_usbh_dev_lib.p_monitor_lock);
        if (ret_tmp != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret_tmp);
        }
    }
    if (__g_usbh_dev_lib.p_hub_evt_sem){
        ret_tmp = usb_lib_sem_destroy(&__g_usb_host_lib.lib, __g_usbh_dev_lib.p_hub_evt_sem);
        if (ret_tmp != USB_OK) {
            __USB_ERR_TRACE(SemDelErr, "(%d)\r\n", ret_tmp);
        }
    }
    return ret;
#endif
}

/**
 * \brief USB 主机设备库反初始化
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_lib_deinit(void){
    if (__g_usbh_dev_lib.is_lib_init == USB_FALSE) {
        return USB_OK;
    }
    __g_usbh_dev_lib.is_lib_deiniting = USB_TRUE;

    return usb_refcnt_put(&__g_usbh_dev_lib.ref_cnt, __dev_lib_release);
}
