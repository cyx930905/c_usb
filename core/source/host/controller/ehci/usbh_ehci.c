/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/controller/ehci/usbh_ehci.h"
#include "core/include/host/controller/ehci/usbh_ehci_reg.h"
#include <string.h>

/*******************************************************************************
 * Macro operate
 ******************************************************************************/
/* \brief EHCI 事件类型*/
#define __EHCI_EVT_IRQ       0x01   /* 中断事件*/
#define __EHCI_EVT_GUARD     0x02   /* 守护事件*/

/**
 * \brief 中断
 * __REG_STS_FATAL:系统错误中断
 * __REG_STS_PCD  :端口变化检测中断
 * __REG_STS_ERR  :USB错误中断
 * __REG_STS_INT  :USB中断使能
 */
#define __EHCI_IRQS         (__REG_STS_FATAL | __REG_STS_PCD | __REG_STS_ERR | __REG_STS_INT)

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbh_core_lib __g_usb_host_lib;

#if USB_MEM_RECORD_EN
extern int usbh_ehci_mem_sta_get(struct usb_hc *p_hc);
#endif
extern int usbh_ehci_mem_init(struct usbh_ehci *p_ehci,
                              uint8_t           mem_type,
                              uint32_t          n_qhs,
                              uint32_t          n_qtds,
                              uint32_t          n_itds,
                              uint32_t          n_sitds);
extern int usbh_ehci_mem_deinit(struct usbh_ehci *p_ehci);
extern struct usbh_ehci_qh *usbh_ehci_qh_alloc(struct usbh_ehci *p_ehci);
extern int usbh_ehci_qh_free(struct usbh_ehci    *p_ehci,
                             struct usbh_ehci_qh *p_qh);
extern int usbh_ehci_rh_init(struct usb_hc *p_hc);
extern void usbh_ehci_rh_port_change(struct usbh_ehci *p_ehci);

extern void usbh_ehci_periodic_disable(struct usbh_ehci *p_ehci);
extern int usbh_ehci_qtds_make(struct usbh_ehci     *p_ehci,
                               struct usbh_trp      *p_trp,
                               struct usb_list_head *p_qtds);
extern int usbh_ehci_qh_init(struct usbh_ehci     *p_ehci,
                             struct usbh_endpoint *p_ep,
                             struct usbh_ehci_qh **p_qh);
extern int usbh_ehci_qh_deinit(struct usbh_ehci    *p_ehci,
                               struct usbh_ehci_qh *p_qh);
extern int usbh_ehci_async_unlink(struct usbh_ehci    *p_ehci,
                                  struct usbh_ehci_qh *p_qh);
extern void usbh_ehci_async_link(struct usbh_ehci     *p_ehci,
                                 struct usbh_ehci_qh  *p_qh,
                                 struct usb_list_head *p_qtds);
extern int usbh_ehci_qh_handle(struct usbh_ehci    *p_ehci,
                               struct usbh_ehci_qh *p_qh);
extern int usbh_ehci_intr_req(struct usbh_ehci     *p_ehci,
                              struct usbh_ehci_qh  *p_qh,
                              struct usbh_trp      *p_trp,
                              struct usb_list_head *p_qtd_list);
extern int usbh_ehci_intr_cancel(struct usbh_ehci    *p_ehci,
                                 struct usbh_ehci_qh *p_qh);
extern int usbh_ehci_iso_stream_init(struct usbh_ehci             *p_ehci,
                                     struct usbh_endpoint         *p_ep,
                                     struct usbh_ehci_iso_stream **p_stream);
extern int usbh_ehci_iso_stream_deinit(struct usbh_ehci            *p_ehci,
                                       struct usbh_ehci_iso_stream *p_stream);
extern int usbh_iso_sched_make(struct usbh_ehci            *p_ehci,
                               struct usbh_trp             *p_trp,
                               struct usbh_ehci_iso_stream *p_stream);
extern int usbh_ehci_iso_req(struct usbh_ehci            *p_ehci,
                             struct usbh_trp             *p_trp,
                             struct usbh_ehci_iso_stream *p_stream);
extern void usbh_ehci_async_scan(struct usbh_ehci *p_ehci);
extern void usbh_ehci_intr_scan(struct usbh_ehci *p_ehci);
extern void usbh_ehci_isoc_scan(struct usbh_ehci *p_ehci);
extern void usbh_trp_done(struct usbh_trp *p_trp);

/*******************************************************************************
 * Code
 ******************************************************************************/
#if USB_OS_EN
/**
 * \brief EHCI 释放信号量
 */
static int __evt_give(struct usbh_ehci *p_ehci,
                      uint8_t           evt){
    /* 赋值事件类型*/
    p_ehci->evt |= evt;
    /* 释放信号量*/
    return usb_sem_give(p_ehci->p_sem);
}

/**
 * \brief EHCI 等待信号量
 */
static int __evt_take(struct usbh_ehci *p_ehci,
                      uint8_t          *p_evt,
                      int               time_out){
    int ret;
    /* 等待 EHCI 信号量*/
    ret = usb_sem_take(p_ehci->p_sem, time_out);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(SemTakeErr, "(%d)\r\n", ret);
        return ret;
    }
    /* 获取事件类型*/
    *p_evt = p_ehci->evt;
    /* 清除 EHCI 事件类型*/
    p_ehci->evt = 0;

    return USB_OK;
}
#endif

/**
 * \brief 初始化主机控制器私有数据域
 */
int usbh_ep_hcpriv_init(struct usbh_endpoint *p_ep){
    if (p_ep == NULL) {
        return -USB_EINVAL;
    }
    if (p_ep->p_hc_priv != NULL) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    p_ep->p_hc_priv = usb_lib_sem_create(&__g_usb_host_lib.lib);
    if (p_ep->p_hc_priv == NULL) {
        __USB_ERR_TRACE(SemTakeErr, "\r\n");
        return -USB_EPERM;
    }
#endif
    return USB_OK;
}

/**
 * \brief 反初始化主机控制器私有数据域
 */
int usbh_ep_hcpriv_deinit(struct usbh_endpoint *p_ep){
    int ret = USB_OK;

    if (p_ep == NULL) {
        return -USB_EINVAL;
    }

    if (p_ep->p_hc_priv == NULL) {
        return USB_OK;
    }
#if USB_OS_EN
    ret = usb_lib_sem_destroy(&__g_usb_host_lib.lib, p_ep->p_hc_priv);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(SemDelErr, "(%d)\r\n", ret);
    }
#endif
    p_ep->p_hc_priv = NULL;

    return ret;
}

/**
 * \brief 同步传输完成函数
 */
void usbh_trp_sync_xfer_done(void *p_arg){
#if USB_OS_EN
    int                   ret;
#endif
    struct usbh_endpoint *p_ep = (struct usbh_endpoint *)p_arg;

#if USB_OS_EN
    ret = usb_sem_give((usb_sem_handle_t)p_ep->p_hc_priv);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(SemGiveErr, "(%d)\r\n", ret);
    }
#endif
}

/**
 * \brief 同步传输等待函数
 */
int usbh_trp_sync_xfer_wait(struct usbh_endpoint *p_ep, int time_out){
    if (p_ep == NULL) {
        return -USB_EINVAL;
    }
    if ((p_ep->p_hc_priv == NULL) ||
            ((time_out < 0) && (time_out != USB_WAIT_FOREVER))) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    return usb_sem_take((usb_sem_handle_t)p_ep->p_hc_priv, time_out);
#endif
}

/**
 * \brief EHCI 传输请求包完成处理函数
 */
static void __ehci_trp_done_handle(struct usbh_ehci *p_ehci){
#if USB_OS_EN
    int                   ret;
#endif
    struct usb_list_node *p_node     = NULL;
    struct usb_list_node *p_node_tmp = NULL;
    struct usbh_trp      *p_trp      = NULL;

#if USB_OS_EN
    ret = usb_mutex_lock(p_ehci->p_trp_done_lock, USBH_EHCI_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return;
    }
#endif
    usb_list_for_each_node_safe(p_node, p_node_tmp, &p_ehci->trp_done_list){
        p_trp = usb_container_of(p_node, struct usbh_trp, node);

        usb_list_node_del(&p_trp->node);
        usbh_trp_done(p_trp);
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_ehci->p_trp_done_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
}

/**
 * \brief 启动 EHCI 控制器
 */
static int __ehci_start(struct usb_hc *p_hc){
    uint32_t tmp;
    int      ret = USB_OK;
#if USB_OS_EN
    int      ret_tmp;
#endif

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }

    /* 获取EHCI控制器*/
    struct usbh_ehci *p_ehci = USBH_GET_EHCI_FROM_HC(p_hc);
    if (p_ehci == NULL) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_ehci->p_lock, USBH_EHCI_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 启动 EHCI 控制器*/
    tmp = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_CMD);
    tmp |= __REG_CMD_RUN;
    USB_LE_REG_WRITE32(tmp, p_ehci->opt_reg + __OPT_REG_CMD);

#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_ehci->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 停止 EHCI 控制器
 */
static int __ehci_stop(struct usb_hc *p_hc){
    return USB_OK;
}

/**
 * \brief 恢复 EHCI 控制器
 */
static int __ehci_resume(struct usb_hc *p_hc){
    int      ret              = USB_OK;
    uint32_t cmd              = 0;
    uint32_t tmp              = 0;

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }

    /* 获取EHCI控制器*/
    struct usbh_ehci *p_ehci = USBH_GET_EHCI_FROM_HC(p_hc);
    if (p_ehci == NULL) {
        return -USB_EILLEGAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_ehci->p_lock, USBH_EHCI_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    /* 关闭中断*/
    USB_LE_REG_WRITE32(0, p_ehci->opt_reg + __OPT_REG_INTR);

    cmd = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_CMD);
    cmd |= __REG_CMD_RUN;
    USB_LE_REG_WRITE32(cmd, p_ehci->opt_reg + __OPT_REG_CMD);

    tmp = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_PSC0);
    /* 清除 PHY 低功耗状态 */
    USB_LE_REG_WRITE32(tmp & ~(1 << 23), p_ehci->opt_reg + __OPT_REG_PSC0);

    tmp = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_PSC0);

    tmp &= ~(__REG_PORTSC_CSC |
             __REG_PORTSC_PEC |
             __REG_PORTSC_OCC |
             __REG_PORTSC_SUSPEND |
             __REG_PORTSC_RESUME);
    USB_LE_REG_WRITE32(tmp | (1 << 6), p_ehci->opt_reg + __OPT_REG_PSC0);

    /* 使能中断(USB错误中断+端口变化检测中断+系统错误中断)*/
    USB_LE_REG_WRITE32(__EHCI_IRQS, p_ehci->opt_reg + __OPT_REG_INTR);

    /* 唤醒后要延时等待USB主机稳定 */
    usb_mdelay(100);
    USB_LE_REG_WRITE32((uint32_t)p_ehci->p_periodic, p_ehci->opt_reg + __OPT_REG_PERIOD);
    USB_LE_REG_WRITE32((uint32_t)p_ehci->p_async, p_ehci->opt_reg + __OPT_REG_ASYNC);

    tmp = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_CMD);
    tmp |= (__REG_CMD_ASE | __REG_CMD_PSE) ;
    USB_LE_REG_WRITE32(tmp, p_ehci->opt_reg + __OPT_REG_CMD);

#if USB_OS_EN
    ret = usb_mutex_unlock(p_ehci->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif

    return ret;
}

/**
 * \brief 挂起 EHCI 控制器
 */
static int __ehci_suspend(struct usb_hc *p_hc){
    int      ret              = USB_OK;
    uint32_t cmd              = 0;
    uint32_t portsc           = 0;
    uint32_t tmp              = 0;
    uint8_t  phy_is_suspended = 0;

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }

    /* 获取EHCI控制器*/
    struct usbh_ehci *p_ehci = USBH_GET_EHCI_FROM_HC(p_hc);
    if (p_ehci == NULL) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_ehci->p_lock, USBH_EHCI_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 关闭异步调度和周期调度*/
    cmd = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_CMD);
    cmd &= ~(__REG_CMD_ASE | __REG_CMD_PSE);
    /* 等待关闭成功*/
    do {
        USB_LE_REG_WRITE32(cmd, p_ehci->opt_reg + __OPT_REG_CMD);
    } while (USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_CMD) & (3 << 4));

    portsc = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_PSC0);
    portsc &= ~(__REG_PORTSC_CSC | __REG_PORTSC_PEC | __REG_PORTSC_OCC);

    tmp = portsc & ~(__REG_PORTSC_WKOC_E | __REG_PORTSC_WKDISC_E | __REG_PORTSC_WKCONN_E);
    /* 挂起主机 */
    tmp |= __REG_PORTSC_SUSPEND;

    if (portsc & __REG_PORTSC_CONNECT) {
        /* 有设备连接中，使能断开唤醒*/
        tmp |= __REG_PORTSC_WKOC_E | __REG_PORTSC_WKDISC_E;
    } else {
        /* 无设备连接，使能连接唤醒*/
        tmp |= __REG_PORTSC_WKOC_E | __REG_PORTSC_WKCONN_E;
    }

    USB_LE_REG_WRITE32(tmp, p_ehci->opt_reg + __OPT_REG_PSC0);

    if (((tmp & __REG_PORTSC_WKDISC_E) && (((tmp & __REG_PORTSC_SSTS) >> 26)) > 1)) {
        usb_mdelay(100);
    }
    /* 停止 PHY 时钟*/
    portsc = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_PSC0);
    USB_LE_REG_WRITE32(portsc | (1 << 23), p_ehci->opt_reg + __OPT_REG_PSC0);

    usb_mdelay(200);
    /* 检查 PHY 时钟是否已经停止*/
    portsc = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_PSC0);
    phy_is_suspended = ((portsc & __REG_PORTSC_SSTS) >> 23);

    usb_mdelay(5);
    /* 关闭中断*/
    USB_LE_REG_WRITE32(0, p_ehci->opt_reg + __OPT_REG_INTR);

    cmd = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_CMD);
    cmd &= ~(__REG_CMD_RUN | __REG_CMD_IAAD);
    USB_LE_REG_WRITE32(cmd, p_ehci->opt_reg + __OPT_REG_CMD);

#if USB_OS_EN
    ret = usb_mutex_unlock(p_ehci->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (phy_is_suspended == 0) {
        __ehci_resume(p_hc);
        return -USB_EPERM;
    }
    return ret;
}

/**
 * \brief EHCI 端点使能函数
 */
static int __ehci_ep_enable(struct usb_hc        *p_hc,
                            struct usbh_endpoint *p_ep){
    int ret = USB_OK;
#if USB_OS_EN
    int ret_tmp;
#endif
    if ((p_hc == NULL) || (p_ep == NULL)) {
        return -USB_EINVAL;
    }

    /* 获取 EHCI 控制器*/
    struct usbh_ehci *p_ehci = USBH_GET_EHCI_FROM_HC(p_hc);
    if (p_ehci == NULL) {
        return -USB_EILLEGAL;
    }

    /* 端点是否已经使能(使能的端点的 p_hw_priv 域会有对应的数据结构数据)*/
    if (p_ep->p_hw_priv) {
        return USB_OK;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_ehci->p_lock, USBH_EHCI_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 如果是等时端点*/
    if (USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_ISO) {
        /* 生成一个等时数据流*/
        ret = usbh_ehci_iso_stream_init(p_ehci,
                                        p_ep,
                                       (struct usbh_ehci_iso_stream **)&p_ep->p_hw_priv);
        if (ret != USB_OK) {
            __USB_ERR_INFO("usb host ehci stream init failed(%d)\r\n", ret);
        }
    } else {
        /* 生成一个 QH(队列头) 放进端点结构体的 p_hw_priv 域*/
        ret = usbh_ehci_qh_init(p_ehci, p_ep, (struct usbh_ehci_qh **)&p_ep->p_hw_priv);
        if (ret != USB_OK) {
            __USB_ERR_INFO("usb host ehci queue head init failed(%d)\r\n", ret);
        }
    }
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_ehci->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief EHCI 端点禁用函数
 */
static int __ehci_ep_disable(struct usb_hc        *p_hc,
                             struct usbh_endpoint *p_ep){
    int                  ret  = USB_OK;
    struct usbh_ehci_qh *p_qh = NULL;
#if USB_OS_EN
    int                  ret_tmp;
#endif

    if ((p_hc == NULL) || (p_ep == NULL)) {
        return -USB_EINVAL;
    }
    if (p_ep->p_hw_priv == NULL) {
        return USB_OK;
    }

    /* 获取 EHCI 控制器 */
    struct usbh_ehci *p_ehci = USBH_GET_EHCI_FROM_HC(p_hc);
    if (p_ehci == NULL) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_ehci->p_lock, USBH_EHCI_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    if (USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_ISO) {
        ret = usbh_ehci_iso_stream_deinit(p_ehci, p_ep->p_hw_priv);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
            goto __exit;
        }
        p_ep->p_hw_priv = NULL;
    } else {

        p_qh = (struct usbh_ehci_qh *)p_ep->p_hw_priv;

        if (p_qh->state == __QH_ST_LINKED) {
            if (USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_INT) {
                /* 取消周期中断链接*/
                ret = usbh_ehci_intr_cancel(p_qh->p_ehci, p_qh);
            } else {
                /* 异步调度取消 QH(队列头) 链接*/
                usbh_ehci_async_unlink(p_qh->p_ehci, p_qh);
            }
            if (ret != USB_OK) {
                __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
                goto __exit;
            }
        }

        if (ret == USB_OK) {
            /* 处理取消链接了的端点 QH（队列头）*/
            ret = usbh_ehci_qh_handle(p_qh->p_ehci, p_qh);
            if (ret != USB_OK) {
                __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
                goto __exit;
            }
        }
    }

    if (ret == USB_OK) {
        if (USBH_EP_TYPE_GET(p_ep) != USB_EP_TYPE_ISO){
            ret = usbh_ehci_qh_deinit(p_ehci, p_qh);
            p_ep->p_hw_priv = NULL;
        }
    }
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_ehci->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief EHCI 请求传输函数
 */
static int __ehci_xfer_request(struct usb_hc   *p_hc,
                               struct usbh_trp *p_trp){
    struct usbh_ehci_qh         *p_qh     = NULL;
    struct usbh_ehci_iso_stream *p_stream = NULL;
    struct usb_list_head         td_list;
    int                          ret      = USB_OK;
#if USB_OS_EN
    int                          ret_tmp;
#endif
    struct usbh_ehci            *p_ehci   = USBH_GET_EHCI_FROM_HC(p_hc);

    /* 端点是否有链接数据链表，没有则返回错误*/
    /* 检查是否是控制/批量传输端点*/
    if ((p_ehci == NULL) || (p_trp->p_ep->p_hw_priv == NULL) ||
           ((USBH_EP_TYPE_GET(p_trp->p_ep) != USB_EP_TYPE_CTRL) &&
            (USBH_EP_TYPE_GET(p_trp->p_ep) != USB_EP_TYPE_BULK) &&
            (USBH_EP_TYPE_GET(p_trp->p_ep) != USB_EP_TYPE_INT) &&
            (USBH_EP_TYPE_GET(p_trp->p_ep) != USB_EP_TYPE_ISO))) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_ehci->p_lock, USBH_EHCI_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
    	__USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 初始化传输描述符链表*/
    usb_list_head_init(&td_list);
    /* 清除传输请求包实际传输长度和状态*/
    p_trp->act_len = 0;
    p_trp->status  = 0;

    /* 根据端点类型进行相应的处理*/
    switch (USBH_EP_TYPE_GET(p_trp->p_ep)) {
        /* 等时端点*/
        case USB_EP_TYPE_ISO:
            p_stream = (struct usbh_ehci_iso_stream *)(p_trp->p_ep->p_hw_priv);

            ret = usbh_iso_sched_make(p_ehci, p_trp, p_stream);
            if (ret != USB_OK) {
                __USB_ERR_INFO("USB host EHCI ISO schedule make failed(%d)", ret);
                goto __exit;
            }
            /* 等时传输请求*/
            ret = usbh_ehci_iso_req(p_ehci, p_trp, p_stream);
            break;
        /* 中断端点*/
        case USB_EP_TYPE_INT:
            p_qh = (struct usbh_ehci_qh *)(p_trp->p_ep->p_hw_priv);
            /* 把传输请求包的数据拆分为一个个 qTD 结构体，并生成一个 qTD 链表*/
            ret = usbh_ehci_qtds_make(p_ehci, p_trp, &td_list);
            if (ret != USB_OK) {
                __USB_ERR_INFO("USB host EHCI QTD list make failed(%d)", ret);
                goto __exit;
            }

            /* 周期中断请求*/
            ret = usbh_ehci_intr_req(p_ehci,
                                     p_qh,
                                     p_trp,
                                    &td_list);
            break;
        /* 控制/批量传输端点*/
        case USB_EP_TYPE_CTRL:
        case USB_EP_TYPE_BULK:
            p_qh = (struct usbh_ehci_qh *)(p_trp->p_ep->p_hw_priv);
            /* 把传输请求包的数据拆分为一个个 qTD 结构体，并生成一个 qTD 链表*/
            ret = usbh_ehci_qtds_make(p_ehci, p_trp, &td_list);
            if (ret != USB_OK) {
                __USB_ERR_INFO("USB host EHCI QTD list make failed(%d)\r\n", ret);
                goto __exit;
            }

            /* 异步调度请求*/
            usbh_ehci_async_link(p_ehci, p_qh, &td_list);

            break;
    }

__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_ehci->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief EHCI 取消传输函数
 *
 * \param[in] p_hc  USB 主机结构体
 * \param[in] p_trp 要取消的传输请求包
 *
 * \retval 成功返回 USB_OK
 */
static int __ehci_xfer_cancel(struct usb_hc   *p_hc,
                              struct usbh_trp *p_trp){
    struct usbh_ehci_qh *p_qh = NULL;
    int                  ret;
#if USB_OS_EN
    int                  ret_tmp;
#endif
    if ((p_hc == NULL) || (p_trp == NULL)) {
        return -USB_EINVAL;
    }

    /* 获取 EHCI 控制器*/
    struct usbh_ehci *p_ehci = USBH_GET_EHCI_FROM_HC(p_hc);
    if (p_ehci == NULL) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_ehci->p_lock, USBH_EHCI_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (USBH_EP_TYPE_GET(p_trp->p_ep) != USB_EP_TYPE_ISO){
        /* 如果端点的私有数据域为空(没有 QH )，则说明端点未使能*/
        if (p_trp->p_ep->p_hw_priv == NULL) {
            ret = -USB_EPERM;
            __USB_ERR_INFO("endpoint don't enable\r\n");
            goto __exit;
        }
    }

    p_trp->status = -USB_ECANCEL;

    /* 判断是不是同步端点*/
    if (USBH_EP_TYPE_GET(p_trp->p_ep) == USB_EP_TYPE_ISO) {
        /* 没有周期调度需求了，关闭周期调度*/
        if ((p_ehci->isoc_count == 0) && (p_ehci->intr_count == 0)) {
            usbh_ehci_periodic_disable(p_ehci);
        }

        usbh_ehci_isoc_scan(p_ehci);
    } else {
        /* 获取绑定端点的 QH(队列头)*/
        p_qh = (struct usbh_ehci_qh *)(p_trp->p_ep->p_hw_priv);
        /* 如果 QH(队列头) 是已链接状态*/
        if (p_qh->state == __QH_ST_LINKED) {
            /* 中断端点*/
            if (USBH_EP_TYPE_GET(p_qh->p_ep) == USB_EP_TYPE_INT) {
                /* 取消周期调度的链接*/
                ret = usbh_ehci_intr_cancel(p_qh->p_ehci, p_qh);
            } else {
                /* 取消异步调度的链接*/
                ret = usbh_ehci_async_unlink(p_qh->p_ehci, p_qh);
            }
            if (ret != USB_OK) {
                goto __exit;
            }
            /* 处理取消链接了的端点QH(队列头)*/
            ret = usbh_ehci_qh_handle(p_qh->p_ehci, p_qh);
            if (ret != USB_OK) {
                __USB_ERR_INFO("ehci qh handle failed(%d)\r\n", ret);
            }
        }
    }
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_ehci->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    __ehci_trp_done_handle(p_ehci);
    return ret;
}

/**
 * \brief 获取 USB 主机控制器周期帧列表的帧索引(精确到微帧)
 *
 * \param[in] p_ehci EHCI 控制器结构体
 *
 * \retval 成功返回读取到的微帧索引
 */
uint32_t usbh_ehci_uframe_idx_get(struct usbh_ehci *p_ehci){
    return USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_FDIX);
}

/**
 * \brief 获取 USB 主机控制器周期帧列表的帧索引
 */
static int __ehci_frame_idx_get(struct usb_hc *p_hc){
    struct usbh_ehci *p_ehci = NULL;

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }

    /* 获取 EHCI 控制器*/
    p_ehci = USBH_GET_EHCI_FROM_HC(p_hc);
    if (p_ehci == NULL) {
        return -USB_EILLEGAL;
    }

    return (usbh_ehci_uframe_idx_get(p_ehci) >> 3) % p_ehci->frame_list_size;
}

/* \brief EHCI 驱动函数集 */
static struct usb_hc_drv __g_ehci_drv = {
        .p_fn_rh_init       = usbh_ehci_rh_init,

        .p_fn_hc_start      = __ehci_start,
        .p_fn_hc_stop       = __ehci_stop,
        .p_fn_hc_suspend    = __ehci_suspend,
        .p_fn_hc_resume     = __ehci_resume,

        .p_fn_ep_enable     = __ehci_ep_enable,
        .p_fn_ep_disable    = __ehci_ep_disable,

        .p_fn_xfer_request  = __ehci_xfer_request,
        .p_fn_xfer_cancel   = __ehci_xfer_cancel,

        .p_fn_frame_num_get = __ehci_frame_idx_get,
#if USB_MEM_RECORD_EN
        .p_fn_controller_mem_get = usbh_ehci_mem_sta_get,
#endif
};

/**
 * \brief 初始化 EHCI 控制器
 */
static int __ehci_hw_init(struct usbh_ehci *p_ehci, struct usbh_ehci_mem_cfg mem_cfg){
    uint32_t  tmp, cmd, time_out;
    int       ret;

    /* 复位 EHCI 控制器，设置产生中断的最大速率为 1 微帧*/
    USB_LE_REG_WRITE32((__REG_CMD_RESET | (1 << 16)), p_ehci->opt_reg + __OPT_REG_CMD);
    time_out = 500;
    /* 等待复位完成*/
    do {
        tmp = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_CMD);
        time_out--;
    } while((tmp & __REG_CMD_RESET) && (time_out > 0));

    if (time_out == 0) {
        return -USB_ETIME;
    }

    /* 设置为主机控制器模式*/
    USB_LE_REG_WRITE32(__REG_MODE_CM_HC, p_ehci->opt_reg + __OPT_REG_MODE);

    /* 获取主机控制器结构体参数*/
    p_ehci->hcs = USB_LE_REG_READ32(p_ehci->cap_reg + __CAP_REG_HCS);
    /* 获取主机控制器性能参数*/
    tmp = USB_LE_REG_READ32(p_ehci->cap_reg + __CAP_REG_HCC);

    /* 主机控制器可能会缓存一些周期调度*/
    if(__CAP_HCC_ISOC_CACHE(tmp)){
        /* 全帧缓存*/
        p_ehci->i_thresh = 0;
    } else {
        /* 部分微帧缓存*/
        p_ehci->i_thresh = 2 + (((tmp) >> 4) & 0x7);
    }

    /* 设置中断间隔为 1 微帧*/
    cmd = 1 << 16;

    /* 禁用 “park” 模式*/
    cmd &= ~__REG_CMD_PARK;

    /* 如果周期帧列表大小可编程*/
    if (__CAP_HCC_PGM_FRAME_LIST_FLAG(tmp)) {
        uint8_t tune_fls;

        p_ehci->frame_list_size = USBH_EHCI_FRAME_LIST_SIZE;

        switch (USBH_EHCI_FRAME_LIST_SIZE) {
            case 1024:  tune_fls = __REG_CMD_FLS_1024; break;
            case 512:   tune_fls = __REG_CMD_FLS_512;  break;
            case 256:   tune_fls = __REG_CMD_FLS_256;  break;
            default:    tune_fls = __REG_CMD_FLS_1024; break;
        }
        /* 设置周期帧列表大小*/
        cmd &= ~(3 << 2);
        cmd |= tune_fls;
    } else {
        /* 不可编程则默认 1024 */
        p_ehci->frame_list_size = 1024;
    }

    /* 初始化 EHCI 数据结构内存空间*/
    ret = usbh_ehci_mem_init(p_ehci,
                             mem_cfg.mem_type,
                             mem_cfg.nqhs,
                             mem_cfg.nqtds,
                             mem_cfg.nitds,
                             mem_cfg.nsitds);
    if (ret != USB_OK) {
        __USB_ERR_INFO("ehci mem init failed(%d)", ret);
        return ret;
    }
    /* 是否能使用 64 位地址*/
    if (__CAP_HCC_64BIT_ADDR(tmp)) {
        USB_LE_REG_WRITE32(0, p_ehci->opt_reg + __OPT_REG_CTRL);
    }

    /* 设置周期帧列表地址*/
    USB_LE_REG_WRITE32((uint32_t)p_ehci->p_periodic, p_ehci->opt_reg + __OPT_REG_PERIOD);

    /* 为 EHCI 异步调度申请空间*/
    p_ehci->p_async = usbh_ehci_qh_alloc(p_ehci);
    if (p_ehci->p_async == NULL) {
        return -USB_ENOMEM;
    }

    /* qTD(队列传输描述符) 中的令牌寄存器设置为错误计数为3和输入令牌*/
    USB_LE_REG_WRITE32((3 << 10) | (__QTD_PID_IN << 8),
                      &p_ehci->p_async->p_dummy->hw_token);
    /* 填充 QH(队列头) 水平链表中下一个要处理的数据结构地址*/
    USB_LE_REG_WRITE32(((uint32_t)p_ehci->p_async & 0xFFFFFFE0) | __Q_TYPE_QH,
                      &p_ehci->p_async->hw_next);
    /* QH(队列头) 的下一个 qTD 指针地址为无效*/
    USB_LE_REG_WRITE32(1u, &p_ehci->p_async->hw_next_qtd);
    /* QH(队列头) 的 Altemate Next qTD Pointer 域写入 qTD 结构体的地址*/
    USB_LE_REG_WRITE32((uint32_t)&p_ehci->p_async->p_dummy,
                       &p_ehci->p_async->hw_alt_next_qtd);
    /* 设置异步起始链表头为 QH(队列头)*/
    USB_LE_REG_WRITE32(__QH_HEAD, &p_ehci->p_async->hw_info1);
    /* QH(队列头) 的 qTD Toke n寄存器设置为 halted on error */
    USB_LE_REG_WRITE32(__QTD_STS_HALT, &p_ehci->p_async->hw_token);
    /* 下一个异步调度 QH(队列头) 为空*/
    p_ehci->p_async->p_next.p_qh = NULL;
    /* 异步调度 QH(队列头) 设置为已链接*/
    p_ehci->p_async->state  = __QH_ST_LINKED;

    /* 设置异步调度的地址*/
    USB_LE_REG_WRITE32((uint32_t)p_ehci->p_async, p_ehci->opt_reg + __OPT_REG_ASYNC);

    /* 使能异步调度*/
    cmd |= __REG_CMD_ASE;

    /* 端口路由到 EHCI 控制器 */
    USB_LE_REG_WRITE32(1u, p_ehci->opt_reg + __OPT_REG_CFG);

    /* 如果支持端口变化检测位，EHCI1.1*/
    tmp = USB_LE_REG_READ32(p_ehci->cap_reg + __CAP_REG_HCC);
    if (__CAP_HCC_PER_PORT_CHANGE_EVENT(tmp)) {
        p_ehci->ppcd = USB_TRUE;
        cmd |= __REG_CMD_PPCEE;
    }

    /* 写入 USB 命令寄存器*/
    USB_LE_REG_WRITE32(cmd, p_ehci->opt_reg + __OPT_REG_CMD);

    return USB_OK;
}

/**
 * \brief 反初始化 EHCI 控制器
 */
static int __ehci_hw_deinit(struct usbh_ehci *p_ehci){

    usbh_ehci_qh_free(p_ehci, p_ehci->p_async);

    return usbh_ehci_mem_deinit(p_ehci);
}

/**
 * \brief EHCI 工作函数
 *
 * \param[in] p_ehci EHCI 结构体
 */
void usbh_ehci_work(struct usbh_ehci *p_ehci){
#if USB_OS_EN
    int ret;

    ret = usb_mutex_lock(p_ehci->p_lock, USBH_EHCI_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return;
    }
#endif
    /* 异步调度*/
    if (p_ehci->async_count) {
        usbh_ehci_async_scan(p_ehci);
    }
    /* 中断传输*/
    if (p_ehci->intr_count > 0) {
        usbh_ehci_intr_scan(p_ehci);
    }
    /* 等时传输*/
    if (p_ehci->isoc_count > 0) {
        usbh_ehci_isoc_scan(p_ehci);
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_ehci->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    __ehci_trp_done_handle(p_ehci);
}

/**
 * \brief EHCI 守护工作
 */
void usbh_ehci_guard_work(struct usbh_ehci *p_ehci){
    /* 赋值事件类型*/
    p_ehci->evt |= __EHCI_EVT_GUARD;
#if USB_OS_EN
    /* 释放信号量*/
    usb_sem_give(p_ehci->p_sem);
#endif
}

#if USB_OS_EN
/**
 * \brief EHCI 任务函数
 */
static void __ehci_task(void *p_arg){
    struct usbh_ehci *p_ehci = NULL;
    int               ret;
    uint8_t           evt;
    uint32_t          status;

    p_ehci = (struct usbh_ehci *)p_arg;

    while(1) {
        /* 等待 EHCI 信号量*/
        ret = __evt_take(p_ehci, &evt, USB_WAIT_FOREVER);
        if (ret != USB_OK) {
            break;
        }

        if (evt & __EHCI_EVT_IRQ){
            ret = usb_mutex_lock(p_ehci->p_lock, USBH_EHCI_MUTEX_TIMEOUT);
            if (ret != USB_OK) {
            	__USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
            	return;
            }

            /* 获取 USB 中断状态*/
            status = p_ehci->status;
            /* 状态清 0 */
            p_ehci->status = 0;

            ret = usb_mutex_unlock(p_ehci->p_lock);
            if (ret != USB_OK) {
                __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
                return;
            }
        }

        /* 使能 USB 中断*/
        USB_LE_REG_WRITE32(__EHCI_IRQS, p_ehci->opt_reg + __OPT_REG_INTR);
        /* 中断事件*/
        if (evt & __EHCI_EVT_IRQ) {
            /* 完成传输或者错误*/
            if (status & (__REG_STS_ERR | __REG_STS_INT)) {
                /* 调用 EHCI 处理函数*/
                usbh_ehci_work(p_ehci);
            }

            /* 移除 QH */
            if (status & __REG_STS_IAA) {
                uint32_t temp;
                /* 读取 USB 命令寄存器*/
                temp  = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_CMD);
                /* 清除“门铃”*/
                temp &= ~__REG_CMD_IAAD;
                USB_LE_REG_WRITE32(temp, p_ehci->opt_reg + __OPT_REG_CMD);
                //TODO
            }

            /* 端口变化中断*/
            if (status & __REG_STS_PCD) {
                /* EHCI 端口变化函数*/
                usbh_ehci_rh_port_change(p_ehci);
            }

            /* 主机错误中断，未做*/
            if (status & __REG_STS_FATAL) {
                //TODO
            }
        }
        /* 守护定时器事件*/
        if (evt & __EHCI_EVT_GUARD) {
            /* 调用 EHCI 处理函数*/
            usbh_ehci_work(p_ehci);
        }
    }
}
#endif

/**
 * \brief EHCI 中断处理函数
 *
 * \param[in] p_ehci EHCI 结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_irq_handle(struct usbh_ehci *p_ehci){
    uint32_t tmp;
    int      ret = USB_OK;

    if (p_ehci == NULL) {
        return -USB_EINVAL;
    }

    /* 先关闭 USB 中断*/
    tmp  = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_INTR);
    tmp &= ~__EHCI_IRQS;
    USB_LE_REG_WRITE32(tmp, p_ehci->opt_reg + __OPT_REG_INTR);

    /* 获取 USB 状态然后清除状态*/
    tmp = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_STS);
    p_ehci->status |= tmp;
    USB_LE_REG_WRITE32(tmp, p_ehci->opt_reg + __OPT_REG_STS);
#if USB_OS_EN
    /* 释放 EHCI 信号量*/
    ret = __evt_give(p_ehci, __EHCI_EVT_IRQ);
    if (ret != USB_OK) {
         __USB_ERR_TRACE(SemGiveErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB EHCI 初始化函数
 *
 * \param[in]  p_hc           USB 主机结构体
 * \param[in]  reg_base       USB 性能寄存器基地址
 * \param[in]  task_prio      EHCI 任务优先级
 * \param[in]  port_speed_bit 端口速度位
 * \param[in]  mem_cfg        内存配置
 * \param[out] p_ehci         成功返回的 EHCI 结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_create(struct usb_hc           *p_hc,
                     uint32_t                 reg_base,
                     int                      task_prio,
                     uint8_t                  port_speed_bit,
                     struct usbh_ehci_mem_cfg mem_cfg,
                     struct usbh_ehci       **p_ehci){
    struct usbh_ehci *p_ehci_tmp = NULL;
    uint32_t          tmp;
    int               ret;
#if USB_OS_EN
    int               ret_tmp;
#endif
    if ((p_hc == NULL) || (p_ehci == NULL)) {
        return -USB_EINVAL;
    }

    p_ehci_tmp = usb_lib_malloc(&__g_usb_host_lib.lib, sizeof(struct usbh_ehci));
    if (p_ehci_tmp == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_ehci_tmp, 0, sizeof(struct usbh_ehci));

    usb_list_head_init(&p_ehci_tmp->intr_qh_list);
    usb_list_head_init(&p_ehci_tmp->trp_done_list);

    p_ehci_tmp->hc_head.controller_type = EHCI;
    p_ehci_tmp->isoc_count  = 0;
    p_ehci_tmp->async_count = 0;
    p_ehci_tmp->intr_count  = 0;
    /* 设置根集线器变化回调函数*/
    p_ehci_tmp->hc_head.p_controller_drv = &__g_ehci_drv;

    /* 获取 EHCI 操作寄存器地址偏移*/
    tmp = USB_LE_REG_READ32(reg_base + __CAP_REG_LENGTH) & 0xFF;
    p_ehci_tmp->cap_reg        = reg_base;
    /* 获取 EHCI 操作寄存器地址*/
    p_ehci_tmp->opt_reg        = reg_base + tmp;
    /* 赋值端口速度位*/
    p_ehci_tmp->port_speed_bit = port_speed_bit;
    p_ehci_tmp->u_frame_next   = -1;
#if USB_OS_EN
    p_ehci_tmp->p_sem = usb_lib_sem_create(&__g_usb_host_lib.lib);
    if (p_ehci_tmp->p_sem == NULL) {
        ret = -USB_EPERM;
        __USB_ERR_TRACE(SemCreateErr, "\r\n");
        goto __failed1;
    }

    p_ehci_tmp->p_lock = usb_lib_mutex_create(&__g_usb_host_lib.lib);
    if (p_ehci_tmp->p_lock == NULL) {
        ret = -USB_EPERM;
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        goto __failed2;
    }

    p_ehci_tmp->p_trp_done_lock = usb_lib_mutex_create(&__g_usb_host_lib.lib);
    if (p_ehci_tmp->p_lock == NULL) {
        ret = -USB_EPERM;
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        goto __failed2;
    }

    p_ehci_tmp->p_task = usb_lib_task_create(&__g_usb_host_lib.lib,
                                               "ehci task",
                                                 task_prio,
                                                 USBH_EHCI_TASK_STACK_SIZE,
                                               __ehci_task,
                                                (void *)p_ehci_tmp);
    if (p_ehci_tmp->p_task == NULL) {
        ret = -USB_EPERM;
        __USB_ERR_TRACE(TaskCreateErr, "\r\n");
        goto __failed2;
    }
#endif
    /* 初始化数据结构内存和硬件*/
    ret = __ehci_hw_init(p_ehci_tmp, mem_cfg);
    if (ret != USB_OK) {
        goto __failed3;
    }

    /* 关联USB主机*/
    p_ehci_tmp->hc_head.p_hc = p_hc;

    /* 使能中断(USB错误中断+端口变化检测中断+系统错误中断)*/
    USB_LE_REG_WRITE32(__EHCI_IRQS, p_ehci_tmp->opt_reg + __OPT_REG_INTR);

#if USB_OS_EN
    /* 启动 EHCI 任务*/
    ret = usb_task_startup(p_ehci_tmp->p_task);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(TaskStartErr, "(%d)\r\n", ret);
        goto __failed2;
    }
#endif

    p_ehci_tmp->hc_head.is_init = USB_TRUE;

    *p_ehci = p_ehci_tmp;

    usb_host_controller_set(p_hc, *p_ehci);

    return USB_OK;
__failed3:
#if USB_OS_EN
    ret_tmp = usb_lib_task_destroy(&__g_usb_host_lib.lib, p_ehci_tmp->p_task);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(TaskDelErr, "(%d)\r\n", ret_tmp);
    }

__failed2:
    if (p_ehci_tmp->p_trp_done_lock) {
        ret_tmp = usb_lib_mutex_destroy(&__g_usb_host_lib.lib, p_ehci_tmp->p_trp_done_lock);
        if (ret_tmp != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret_tmp);
        }
    }

    if (p_ehci_tmp->p_lock) {
        ret_tmp = usb_lib_mutex_destroy(&__g_usb_host_lib.lib, p_ehci_tmp->p_lock);
        if (ret_tmp != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret_tmp);
        }
    }

    if (p_ehci_tmp->p_sem) {
        ret_tmp = usb_lib_sem_destroy(&__g_usb_host_lib.lib, p_ehci_tmp->p_sem);
        if (ret_tmp != USB_OK) {
            __USB_ERR_TRACE(SemDelErr, "(%d)\r\n", ret_tmp);
        }
    }
__failed1:
#endif
    usb_lib_mfree(&__g_usb_host_lib.lib, p_ehci_tmp);

    return ret;
}

/**
 * \brief 销毁 EHCI 控制器
 *
 * \param[in] p_hc   USB 主机结构体
 * \param[in] p_ehci EHCI 结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_destory(struct usb_hc *p_hc, struct usbh_ehci *p_ehci){
#if USB_OS_EN
    int      ret;
#endif
    uint32_t tmp;

    if ((p_hc == NULL) || (p_ehci == NULL)) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_ehci->p_lock, USBH_EHCI_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 控制器运行着，停止控制器*/
    tmp = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_CMD);
    if (tmp & __REG_CMD_RUN) {
        tmp &= ~__REG_CMD_RUN;
        USB_LE_REG_WRITE32(tmp, p_ehci->opt_reg + __OPT_REG_CMD);
    }

#if USB_OS_EN
    ret = usb_mutex_unlock(p_ehci->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

#if USB_OS_EN
    /* 删除信号量*/
    ret = usb_lib_sem_destroy(&__g_usb_host_lib.lib, p_ehci->p_sem);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(SemDelErr, "(%d)\r\n", ret);
        return ret;
    }
    /* 删除任务*/
    ret = usb_lib_task_destroy(&__g_usb_host_lib.lib, p_ehci->p_task);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(TaskDelErr, "(%d)\r\n", ret);
        return ret;
    }
    /* 删除互斥锁*/
    ret = usb_lib_mutex_destroy(&__g_usb_host_lib.lib, p_ehci->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    __ehci_hw_deinit(p_ehci);

    usb_lib_mfree(&__g_usb_host_lib.lib, p_ehci);

    usb_host_controller_set(p_hc, NULL);

    return USB_OK;
}





