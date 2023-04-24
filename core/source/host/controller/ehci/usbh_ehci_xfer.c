/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/core/usbh_dev.h"
#include "core/include/host/controller/ehci/usbh_ehci.h"
#include "core/include/host/controller/ehci/usbh_ehci_reg.h"
#include <string.h>

/*******************************************************************************
 * Macro operate
 ******************************************************************************/
/* \brief 队列头 XactErr 重试限制*/
#define QH_XACTERR_MAX        32

#define EHCI_QH_HANDLE_NEW

/* 错误标志*/
#define EHCI_SITD_ERRS (EHCI_SITD_STS_ERR | EHCI_SITD_STS_DBE | EHCI_SITD_STS_BABBLE | EHCI_SITD_STS_XACT | EHCI_SITD_STS_MMF)
#define EHCI_ITD_ERRS  (EHCI_ITD_BUF_ERR | EHCI_ITD_BABBLE | EHCI_ITD_XACTERR)

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbh_core_lib __g_usb_host_lib;

extern struct usbh_ehci_qh *usbh_ehci_qh_alloc(struct usbh_ehci *p_ehci);
extern int usbh_ehci_qh_free(struct usbh_ehci    *p_ehci,
                             struct usbh_ehci_qh *p_qh);
extern struct usbh_ehci_qtd *usbh_ehci_qtd_alloc(struct usbh_ehci *p_ehci);
extern int usbh_ehci_qtd_free(struct usbh_ehci     *p_ehci,
                              struct usbh_ehci_qtd *p_qtd);
extern struct usbh_ehci_itd *usbh_ehci_itd_alloc(struct usbh_ehci *p_ehci);
extern int usbh_ehci_itd_free(struct usbh_ehci *p_ehci, struct usbh_ehci_itd *p_itd);
extern struct usbh_ehci_sitd *usbh_ehci_sitd_alloc(struct usbh_ehci *p_ehci);
extern int usbh_ehci_sitd_free(struct usbh_ehci *p_ehci, struct usbh_ehci_sitd *p_sitd);
extern uint32_t usbh_ehci_uframe_idx_get(struct usbh_ehci *p_ehci);
extern void usbh_ehci_periodic_enable(struct usbh_ehci *p_ehci, usb_bool_t is_iso);

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 获取当前数据结构的下一个水平数据结构的地址
 */
static uint32_t *__ehci_next_hw(union usbh_ehci_struct_ptr next,
                                uint32_t                   type){
    switch (type) {
    case __Q_TYPE_QH:
        return &next.p_qh->hw_next;
    case __Q_TYPE_FSTN:
        return &next.p_fstn->hw_next;
    case __Q_TYPE_ITD:
        return &next.p_itd->hw_next;
    default:
        return &next.p_sitd->hw_next;
    }
}

/**
 * \brief 获取当前数据结构的下一个水平数据结构
 */
static union usbh_ehci_struct_ptr __ehci_next_struct(union usbh_ehci_struct_ptr pstr,
                                                     uint32_t                   type){
    switch (type) {
    case __Q_TYPE_QH:
        return pstr.p_qh->p_next;
    case __Q_TYPE_FSTN:
        return pstr.p_fstn->p_next;
    case __Q_TYPE_ITD:
        return pstr.p_itd->p_next;
    default:
        return pstr.p_sitd->p_next;
    }
}

/**
 * \brief 传输完成函数
 */
static void __ehci_trp_done(struct usbh_ehci *p_ehci, struct usbh_trp *p_trp, int status){
#if USB_OS_EN
    int ret;
#endif

    if ((status == -USB_EINPROGRESS) || (status == -USB_EPERM)) {
        status = USB_OK;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_ehci->p_trp_done_lock, USBH_EHCI_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return;
    }
#endif
    p_trp->status = status;

    usb_list_node_add_tail(&p_trp->node, &p_ehci->trp_done_list);
#if USB_OS_EN
    ret = usb_mutex_unlock(p_ehci->p_trp_done_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
    }
#endif
}

/**
 * \brief 使能周期调度
 *
 * \param[in] p_ehci EHCI 控制器结构体
 * \param[in] is_iso 是否是等时传输
 */
void usbh_ehci_periodic_enable(struct usbh_ehci *p_ehci, usb_bool_t is_iso){
    uint32_t  tmp, cmd;

    tmp = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_STS);
    if (is_iso == USB_TRUE) {
        p_ehci->u_frame_next = usbh_ehci_uframe_idx_get(p_ehci) % (p_ehci->frame_list_size << 3);
    }
    /* 检查是否打开周期调度*/
    if ((tmp & (1 << 14))) {
        return;
    } else {
        cmd = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_CMD);
        cmd |= __REG_CMD_PSE;
        /* 写入USB命令寄存器*/
        USB_LE_REG_WRITE32(cmd, p_ehci->opt_reg + __OPT_REG_CMD);
    }
}

/**
 * \brief 禁用周期调度
 *
 * \param[in] p_ehci EHCI 结构体
 */
void usbh_ehci_periodic_disable(struct usbh_ehci *p_ehci){
    uint32_t tmp, cmd;

    tmp = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_STS);
    /* 检查是否打开周期调度*/
    if ((tmp & (1 << 14))) {
        cmd = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_CMD);
        cmd &= (~__REG_CMD_PSE);
        /* 写入USB命令寄存器*/
        USB_LE_REG_WRITE32(cmd, p_ehci->opt_reg + __OPT_REG_CMD);
        p_ehci->u_frame_next = -1;
    }
}

/**
 * \brief 初始化 qTD（队列传输描述符）
 */
static uint32_t __qtd_init(struct usbh_ehci_qtd *p_qtd,
                           void                 *p_buf,
                           size_t                len,
                           uint32_t              token,
                           uint32_t              mps){
    uint8_t  i;
    uint32_t count;

#ifdef __CPU_64BITS
    uint64_t addr = (uint64_t)p_buf;
#else
    uint32_t addr = (uint32_t)p_buf;
#endif

    /* 把数据缓存地址写入 qTD 的缓冲页指针，缓冲页为 4K 对齐*/
    USB_LE_REG_WRITE32((uint32_t)addr, &p_qtd->hw_buf[0]);
#ifdef __CPU_64BITS
    USB_LE_REG_WRITE32((addr >> 32), &p_qtd->hw_buf_high[0]);
#endif

    /* 目前地址到 4K 对齐还需要多少偏移*/
    count = 0x1000 - (addr & 0x0fff);
    /* 如果数据长度不超过 count，则 qTD 的数据长度就等于要发送的 buf 数据长度*/
    if (len <= count) {
        count = len;
    } else {
        /* 把地址移到 4K 对齐的位置*/
        addr +=  0x1000;
        addr &= ~0x0fff;

        /* 填充另外 4 个缓冲页指针*/
        for (i = 1; count < len && i < 5; i++) {
            USB_LE_REG_WRITE32((uint32_t)addr, &p_qtd->hw_buf[i]);
#ifdef __CPU_64BITS
            USB_LE_REG_WRITE32((addr >> 32), &p_qtd->hw_buf_high[0]);
#endif
            /* 每次加 4K 地址*/
            addr += 0x1000;
            /* 如果要发送的 buf 的长度还大于 4K */
            if ((count + 0x1000) < len) {
                count += 0x1000;
            } else {
                count = len;
            }
        }
        /* 一个 qTD 不够*/
        /* 把当前 qTD 的数据长度对齐为端点最大包长度的整数倍*/
        if (count != len) {
            count -= (count % mps);
        }
    }

    if ((token & (3 << 8)) == (3 << 8)) {
        __USB_ERR_INFO("ehci qtd token %x illegal\r\n", token);
        return -USB_EILLEGAL;
    }

    USB_LE_REG_WRITE32(((count << 16) | token), &p_qtd->hw_token);

    p_qtd->len = count;

    return count;
}

/**
 * \brief QH（队列头）初始化
 *
 * \param[in]  p_ehci   EHCI 结构体
 * \param[in]  p_ep     和生成的 QH 绑定的端点
 * \param[out] p_qh_ret 返回的初始化完成的  QH
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_qh_init(struct usbh_ehci     *p_ehci,
                      struct usbh_endpoint *p_ep,
                      struct usbh_ehci_qh **p_qh_ret){
    struct usbh_ehci_qh *p_qh         = NULL;
    struct usbh_device  *p_usb_dev    = NULL;
    uint32_t             info1        = 0, info2 = 0;
    int                  ret_tmp, ret = -USB_EILLEGAL;

    /* 申请一个 QH(队列头) 内存空间*/
    p_qh = usbh_ehci_qh_alloc(p_ehci);
    if (p_qh == NULL) {
        return -USB_ENOMEM;
    }

    p_qh->usecs   = 0;
    p_qh->c_usecs = 0;
    p_qh->p_ep    = p_ep;

    /* 获取端点地址*/
    info1 |= ((uint32_t)USBH_EP_ADDR_GET(p_ep)) << 8;
    info1 |= (p_ep->p_usb_dev->addr & 0x7f) << 0;

    /* 检查端点最大包大小*/
    if (USBH_EP_MPS_GET(p_ep) > 1024) {
        __USB_ERR_INFO("endpoint max packet size %d illegal", USBH_EP_MPS_GET(p_ep));
        goto __failed;
    }

    /* 如果是中断端点*/
    if (USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_INT) {
        uint16_t interval = USBH_EP_INTVAL_GET(p_ep);
        p_qh->frame_period = 0xFFFF;
        /* 获取轮询中断端点的时间间隔*/
        if (p_ep->p_usb_dev->speed == USB_SPEED_HIGH) {
            /* 高速端点的 interval 值必须在 1~16 */
            interval = (interval < 1) ? 1 : ((interval > 16) ? 16 : interval);
            /* 算出轮询高速端点的时间 2 ^ (interval - 1) 个微帧)*/
            interval = 1 << (interval - 1);

            /* 调整轮询端点的微帧间隔，微帧间隔大于最大的周期（帧周期 * 8）*/
            if (interval > p_ehci->frame_list_size << 3) {
                interval = p_ehci->frame_list_size << 3;
            }
            /* 取 8 的倍数*/
            interval &= 0xFFF8;
            /* 获取端点带宽*/
            p_qh->usecs = p_ep->band_width;
        } else {    /* 所属设备不是高速设备*/
            /* 调整轮询帧间隔，帧间隔大于最大的周期*/
            if (interval > p_ehci->frame_list_size) {
                interval = p_ehci->frame_list_size;
            }
            /* 转换为微帧*/
            interval <<= 3;
        }

        /* 调整最小的帧间隔*/
        interval = (interval < USBH_EHCI_BANDWIDTH_SIZE) ?
                    interval : USBH_EHCI_BANDWIDTH_SIZE;

        /* 转换为帧单位，得到最终轮询端点帧周期*/
        p_qh->frame_period = interval >> 3;
    }

    switch (p_ep->p_usb_dev->speed) {
        /* 低速/全速设备*/
        case USB_SPEED_LOW:
        case USB_SPEED_FULL:
            if (p_ep->p_usb_dev->speed == USB_SPEED_LOW) {
                /* 设置为低速端点*/
                info1 |= __QH_LOW_SPEED;
            }
            /* 中断端点*/
            if (USBH_EP_TYPE_GET(p_ep) != USB_EP_TYPE_INT) {
                /* 设置 NAK 计数器重装载值*/
                info1 |= (USBH_EHCI_TUNE_RL_TT << 28);
            }
            /* 控制端点*/
            if (USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_CTRL) {
                /* 全速/低速控制端点*/
                info1 |= __QH_CONTROL_EP;
                /* 数据翻转控制*/
                info1 |= __QH_TOGGLE_CTL;
            }
            /* 设置端点最大包大小*/
            info1 |= (USBH_EP_MPS_GET(p_ep) & 0x07FF) << 16;
            /* 设置端点每一微帧产生一个事务*/
            info2 |= (USBH_EHCI_TUNE_MULT_TT << 30);

            /* 寻找高速集线器或者根集线器*/
            p_usb_dev = p_ep->p_usb_dev;
            while ((p_usb_dev->p_hub_basic->p_usb_fun != NULL) &&
                   (p_usb_dev->p_hub_basic->p_usb_fun->p_usb_dev->speed != USB_SPEED_HIGH)) {
                p_usb_dev = p_usb_dev->p_hub_basic->p_usb_fun->p_usb_dev;
            }
            /* 设置高速集线器或根集线器的端口号*/
            info2 |= (p_usb_dev->port) << 23;
            /* 如果找到高速集线器(不是根集线器)，设置高速集线器地址*/
            if (p_usb_dev->p_hub_basic->p_usb_fun) {
                info2 |= p_usb_dev->p_hub_basic->p_usb_fun->p_usb_dev->addr << 16;
            }
            break;
        /* 高速设备*/
        case USB_SPEED_HIGH:
            info1 |= __QH_HIGH_SPEED;
            /* 控制端点*/
            if (USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_CTRL) {
                /* 设置 NAK 计数器重装载值*/
                info1 |= (USBH_EHCI_TUNE_RL_HS << 28);
                /* 设置端点最大包大小*/
                info1 |= (USBH_EP_MPS_GET(p_ep) & 0x07FF) << 16;
                /* 数据翻转控制*/
                info1 |= __QH_TOGGLE_CTL;
                /* 设置端点每一微帧产生一个事务*/
                info2 |= (USBH_EHCI_TUNE_MULT_HS << 30);
            }
                /* 批量传输端点*/
                else if (USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_BULK) {
                /* 设置 NAK 计数器重装载值*/
                info1 |= (USBH_EHCI_TUNE_RL_HS << 28);
                /* 设置端点最大包大小*/
                info1 |= (USBH_EP_MPS_GET(p_ep) & 0x07FF) << 16;
                /* 设置端点每一微帧产生一个事务*/
                info2 |= (USBH_EHCI_TUNE_MULT_HS << 30);
            }
            /* 中断端点*/
            else {
                /* 设置端点最大包大小*/
                info1 |= (USBH_EP_MPS_GET(p_ep) & 0x07FF) << 16;
                info2 |= (1 + (((USBH_EP_MPS_GET(p_ep)) >> 11) & 0x03));
            }
            break;
        default:
            __USB_ERR_INFO("usb device speed %d illegal\r\n", p_ep->p_usb_dev->speed);
            goto __failed;
    }

    p_qh->state = __QH_ST_IDLE;
    /* 把端点的信息写入 QH 的端点静态信息域*/
    USB_LE_REG_WRITE32(info1, &p_qh->hw_info1);
    USB_LE_REG_WRITE32(info2, &p_qh->hw_info2);
    /* 所有指针域设置为无效*/
    USB_LE_REG_WRITE32(1u, &p_qh->hw_next);
    USB_LE_REG_WRITE32(1u, &p_qh->hw_next_qtd);
    USB_LE_REG_WRITE32(1u, &p_qh->hw_alt_next_qtd);

    *p_qh_ret = p_qh;

    return USB_OK;

__failed:
    ret_tmp = usbh_ehci_qh_free(p_ehci, p_qh);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret_tmp);
    }

    return ret;
}

/**
 * \brief QH（队列头）反初始化
 *
 * \param[in] p_ehci EHCI 结构体
 * \param[in] p_qh   要反初始化的 QH
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_qh_deinit(struct usbh_ehci    *p_ehci,
                        struct usbh_ehci_qh *p_qh){
    int ret = USB_OK;

    ret = usbh_ehci_qh_free(p_ehci, p_qh);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
    }
    return ret;
}

int usbh_ehci_qtds_free(struct usbh_ehci *p_ehci, struct usb_list_head *p_qtds);
/**
 * \brief 生成 qTD（队列传输描述符）链表
 *
 * \param[in] p_ehci  EHCI 结构体
 * \param[in] p_trp   请求传输包
 * \param[in] p_qtds  要生成的 qTD 链表
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_qtds_make(struct usbh_ehci     *p_ehci,
                        struct usbh_trp      *p_trp,
                        struct usb_list_head *p_qtds){
    struct usbh_ehci_qtd *p_qtd  = NULL, *p_prev_qtd = NULL;
    struct usbh_endpoint *p_ep   = NULL;
    uint32_t              token;
    usb_bool_t            is_in;
    uint8_t              *p_data = NULL;
    int                   tr_len;
    int                   ret;

    /* 获取传输请求包的端点*/
    p_ep  = p_trp->p_ep;
    /* 激活qTD和设置错误计数*/
    token = __QTD_STS_ACTIVE | (3 << 10);

    /* 控制端点*/
    if (USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_CTRL) {
        p_qtd = usbh_ehci_qtd_alloc(p_ehci);
        if (p_qtd == NULL) {
            return -USB_ENOMEM;
        }

        usb_list_node_add_tail(&p_qtd->node, p_qtds);
        p_qtd->p_trp = p_trp;

        /* 初始化 SETUP qTD，控制端点的最大包长度最小是 8 字节*/
        __qtd_init(p_qtd,
                   p_trp->p_ctrl_dma,
                   sizeof(struct usb_ctrlreq),
                   token | (__QTD_PID_SETUP << 8),
                   8);

        /* 数据包切换(USB的纠错机制)*/
        token ^= __QTD_TOGGLE;
    }
    /* 判断传输方向*/
    if (USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_CTRL) {
        is_in = (p_trp->p_ctrl->request_type & USB_DIR_IN) ? USB_TRUE : USB_FALSE;
    } else {
        is_in = (USBH_EP_DIR_GET(p_ep) == USB_DIR_IN) ? USB_TRUE : USB_FALSE;
    }

    /* 数据包*/
    if ((p_trp->len) && (p_trp->p_data)) {
        uint32_t length;
        /* 是否是输入传输*/
        if (is_in) {
            token |= (__QTD_PID_IN << 8);
        }
        /* 获取数据缓冲区*/
        p_data = (uint8_t *)p_trp->p_data_dma;
        /* 获取数据长度*/
        tr_len = p_trp->len;
        while (tr_len > 0) {
            /* 获取上一次的 qTD */
            p_prev_qtd = p_qtd;
            /* 为数据过程申请一个新的 qTD */
            p_qtd = usbh_ehci_qtd_alloc(p_ehci);
            if (p_qtd == NULL) {
                ret = -USB_ENOMEM;
                goto __failed;
            }

            /* 把传输请求包放到 qTD 里*/
            p_qtd->p_trp = p_trp;
            /* 前一个 qTD 的 Next qTD 指针设置为新的 qTD 地址*/
            if (p_prev_qtd) {
                USB_LE_REG_WRITE32((uint32_t)p_qtd, &p_prev_qtd->hw_next);
            }
            /* 如果是批量输入传输，则把异步调度的 dummy 地址写到 qTD 链表的 Alternate Next qTD Pointer*/
            if ((USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_BULK) && (is_in)) {
                USB_LE_REG_WRITE32((uint32_t)(p_ehci->p_async->p_dummy), &p_qtd->hw_alt_next);
            }
            /* 插入到 qTD 链表中*/
            usb_list_node_add_tail(&(p_qtd->node), p_qtds);

            /* 初始化 qTD*/
            length  = __qtd_init(p_qtd, p_data, tr_len, token, USBH_EP_MPS_GET(p_ep));
            /* 传输请求包数据长度减去qTD的数据长度*/
            tr_len -= length;
            p_data += length;

            /* 异或，数据包切换*/
            if ((USBH_EP_MPS_GET(p_ep) & (length + (USBH_EP_MPS_GET(p_ep) - 1))) == 0) {
                token ^= __QTD_TOGGLE;
            }
        }
    }
    /* 没有短包错误或者是个控制端点*/
    if (!(p_trp->flag & USBH_TRP_SHORT_NOT_OK) ||
        (USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_CTRL)) {
        /* 最后一个 qTD 的 Next qTD Pointer无效*/
        p_qtd->hw_alt_next = USB_CPU_TO_LE32(1u);
        //pQtd->HwNext = USB_CPU_TO_LE32(1u);
    }

    /* 控制请求状态包*/
    if (USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_CTRL) {

        if (p_trp->len == 0) {
            /* 没有数据的控制请求，是输入传输*/
            token |= (__QTD_PID_IN << 8);
        } else {
            /* 输出传输*/
            token ^= 0x0100;
        }
        /* 数据包切换，DATA1*/
        token |= __QTD_TOGGLE;
    }
    /* 控制端点 | 传输请求包长度是端点最大包长度的整数倍 | 用一个短包来终止输出传输 */
    /* 创建一个短包插入到 qTD 链表最后*/
    if ((USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_CTRL) ||
        ((p_trp->len % USBH_EP_MPS_GET(p_ep) == 0) &&
         (p_trp->flag & USBH_TRP_ZERO_PACKET) &&
         (!is_in))) {
        /* 获取上一次的 qTD */
        p_prev_qtd = p_qtd;
        /* 申请一个新的 qTD */
        p_qtd = usbh_ehci_qtd_alloc(p_ehci);
        if (p_qtd == NULL) {
            ret = -USB_ENOMEM;
            goto __failed;
        }

        /* 把传输请求包放到 qTD 里*/
        p_qtd->p_trp = p_trp;
        /* 前一个 qTD 的 Next qTD 指针设置为新的 qTD 地址*/
        USB_LE_REG_WRITE32((uint32_t)(p_qtd), &p_prev_qtd->hw_next);
        /* 插入到 qTD 链表中*/
        usb_list_node_add_tail(&p_qtd->node, p_qtds);

        /* 初始化没有数据的 qTD */
        __qtd_init(p_qtd, 0, 0, token, 0);
    }
    /* 需要中断*/
    if (!(p_trp->flag & USBH_TRP_NO_INTERRUPT)) {
        token = USB_LE_REG_READ32(&p_qtd->hw_token);
        /* 使能 qTD 完成产生中断*/
        token |= __QTD_IOC;
        USB_LE_REG_WRITE32(token, &p_qtd->hw_token);
    }
    return USB_OK;

__failed:
    ret = usbh_ehci_qtds_free(p_ehci, p_qtds);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
    }

    return -USB_ENOMEM;
}

/**
 * \brief 释放 qTD（队列传输描述符） 链表
 */
int usbh_ehci_qtds_free(struct usbh_ehci     *p_ehci,
                        struct usb_list_head *p_qtds){
    struct usb_list_node *p_entry, *p_tmp;
    int                   ret = USB_OK;

    usb_list_for_each_node_safe(p_entry, p_tmp, p_qtds) {
        struct usbh_ehci_qtd *p_qtd = NULL;

        p_qtd = usb_container_of(p_entry, struct usbh_ehci_qtd, node);
        usb_list_node_del(&p_qtd->node);
        ret = usbh_ehci_qtd_free(p_ehci, p_qtd);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
        }
    }

    return ret;
}

/**
 * \brief qTD（队列传输描述符）链接
 */
static void __qtds_link(struct usbh_ehci_qh  *p_qh,
                        struct usb_list_head *p_qtds){
    struct usbh_ehci_qtd  *p_qtd     = NULL;
    struct usbh_ehci_qtd  *p_qtd_new = NULL;

    if ((usb_list_head_is_empty(&p_qh->qtds)) && (USB_LE_REG_READ32(&p_qh->hw_next_qtd) == 1u)) {
        p_qtd = usb_container_of(p_qtds->p_next, struct usbh_ehci_qtd, node);

        /* 把新的 qTD 链表加到 QH 中原有的 qTD 链表的尾部*/
        usb_list_head_splice_tail(p_qtds, &p_qh->qtds);
        /* QH 的 Next qTD Pointer 地址设置为 qTD 链表的第一个节点地址*/
        USB_LE_REG_WRITE32((uint32_t)p_qtd, &p_qh->hw_next_qtd);

    } else {
        /* 目标 QH 的 qTD 链表不为空，获取最后的 qTD 节点*/
        p_qtd = usb_container_of(p_qh->qtds.p_prev,
                                 struct usbh_ehci_qtd,
                                 node);

        /* 把新的 qTD 链表加到 QH 中原有的 qTD 链表的尾部*/
        usb_list_head_splice_tail(p_qtds, &p_qh->qtds);

        p_qtd_new = usb_container_of(p_qtds->p_next, struct usbh_ehci_qtd, node);

        /* 目标 QH 的旧 qTD 链表的最后一个 qTD 节点中
         * 指向下一个 qTD 地址的指针设置为新的 qTD 链表的地址*/
        USB_LE_REG_WRITE32((uint32_t)p_qtd_new, &p_qtd->hw_next);
    }
}

/**
 * \brief QH（队列头）链接到 EHCI 的异步调度中
 */
static void __async_link(struct usbh_ehci    *p_ehci,
                         struct usbh_ehci_qh *p_qh){
    struct usbh_ehci_qh *p_hdr = NULL;

    /* 目标 QH 的 Altemate Next qTD Pointer 无效*/
    p_qh->hw_alt_next_qtd =  USB_CPU_TO_LE32(1u);
    /* 设置目标 QH 的令牌（Ping功能和数据包切换）*/
    p_qh->hw_token       &= USB_CPU_TO_LE32(__QTD_TOGGLE | __QTD_STS_PING);
    /* 获取异步调度 QH 链表*/
    p_hdr                 = p_ehci->p_async;
    /* 获取异步调度源 QH 的下一个 QH 地址*/
    p_qh->p_next.p_qh     = p_hdr->p_next.p_qh;
    /* 获取异步调度源 QH 的水平链表指针*/
    p_qh->hw_next         = p_hdr->hw_next;
    /* 把目标 QH 插入到 EHCI 异步调度源 QH 的下一个 QH 地址*/
    p_hdr->p_next.p_qh    = p_qh;
    /* 把目标 QH 的地址写入到异步调度中源 QH 的水平链接地址中*/
    p_hdr->hw_next        = USB_CPU_TO_LE32((uint32_t)p_qh | __Q_TYPE_QH);
    /* 把目标QH的状态设置为链接*/
    p_qh->state           = __QH_ST_LINKED;
}

/**
 * \brief  QH（队列头）异步调度链接
 *
 * \param[in] p_ehci EHCI 结构体
 * \param[in] p_qh   要链接的 QH
 * \param[in] p_qtds 传输数据的目标 qTD 链表
 */
void usbh_ehci_async_link(struct usbh_ehci     *p_ehci,
                          struct usbh_ehci_qh  *p_qh,
                          struct usb_list_head *p_qtds){
    __qtds_link(p_qh, p_qtds);

    if ((p_qh->state == __QH_ST_IDLE) ||
        (p_qh->state == __QH_ST_UNLINKED)) {
        __async_link(p_ehci, p_qh);
        p_ehci->async_count++;
    }
}

/**
 * \brief 取消 QH（队列头）在异步调度的链接
 *
 * \param[in] p_ehci EHCI 结构体
 * \param[in] p_qh   要取消链接的 QH
 */
int usbh_ehci_async_unlink(struct usbh_ehci    *p_ehci,
                           struct usbh_ehci_qh *p_qh){
    struct usbh_ehci_qh *p_qh_pre = NULL;
    uint32_t             tmp, retry;

    if (p_qh->state != __QH_ST_LINKED) {
        return -USB_EILLEGAL;
    }

    /* 找到异步调度中要取消链接的目标 QH 的上一个 QH */
    p_qh_pre = p_ehci->p_async;
    while (p_qh_pre->p_next.p_qh != p_qh) {
        p_qh_pre = p_qh_pre->p_next.p_qh;
    }

    /* 前 QH 的水平链接指针指向目标 QH 的下一个 QH 的水平链接指针*/
    p_qh_pre->hw_next     = p_qh->hw_next;
    /* 前 QH 的下一个 QH 指针指向目标 QH 的下一个 QH 的地址*/
    p_qh_pre->p_next.p_qh = p_qh->p_next.p_qh;

    /* 通知 USB 主机控制器*/
    tmp = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_STS);
    tmp &= __REG_STS_IAA;
    USB_LE_REG_WRITE32(tmp, p_ehci->opt_reg + __OPT_REG_STS);
    /* 使能异步调度推进中断*/
    tmp = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_CMD);
    tmp |= __REG_CMD_IAAD;
    USB_LE_REG_WRITE32(tmp, p_ehci->opt_reg + __OPT_REG_CMD);

    /* 等待完成*/
    retry = 10;
    do {
        usb_mdelay(1);
        /* 查看异步推进中断状态位*/
        tmp = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_STS);
        retry--;
        tmp &= __REG_STS_IAA;
    } while ((!tmp) && (retry));

    if (retry == 0) {
        //todo
        //return -USB_ETIME;
    }

    USB_LE_REG_WRITE32(tmp, p_ehci->opt_reg + __OPT_REG_STS);
    /* 目标 QH 设置为未链接*/
    p_qh->state = __QH_ST_UNLINKED;

    if (p_ehci->async_count) {
        p_ehci->async_count--;
    }

    return USB_OK;
}

#ifdef EHCI_QH_HANDLE_NEW
/**
 * \brief qTD 获取状态
 */
static int __qtd_copy_status(struct usbh_ehci *p_ehci,
                             struct usbh_trp  *p_trp,
                             size_t            length,
                             uint32_t          token){
    int    status = -USB_EINPROGRESS;

    /* 如果不是SETUP令牌包，则是输入/输出令牌包，计算传输请求包实际传输长度*/
    if (__QTD_PID (token) != __QTD_PID_SETUP) {
        p_trp->act_len += length - __QTD_LENGTH(token);
    }

    /* force cleanup after short read; not always an error */
    if (IS_SHORT_READ(token)) {
        status = -USB_EPERM;
    }

    /* 硬件报告的错误*/
    if (token & __QTD_STS_HALT) {
        if (token & __QTD_STS_BABBLE) {
            status = -USB_EOVERFLOW;
        } else if (__QTD_CERR(token)) {
            status = -USB_EPIPE;
        } else if (token & __QTD_STS_MMF) {
            /* 全速/低速中断传输忽略分割完成*/
            status = -USB_EPROTO;
        } else if (token & __QTD_STS_DBE) {
            status = (__QTD_PID(token) == 1) /* 是否是输入操作*/
                ? -USB_EREAD   /* 主机无法读数据*/
                : -USB_EWRITE; /* 主机无法写数据*/
        } else if (token & __QTD_STS_XACT) {
            /* 超时，坏的CRC，错误的PID等等*/
            status = -USB_EPROTO;
        } else {
            status = -USB_EPROTO;
        }
    }

    return status;
}

/**
 * \brief QH 处理函数
 *
 * \param[in] p_ehci EHCI 结构体
 * \param[in] p_qh   要处理 QH 结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_qh_handle(struct usbh_ehci    *p_ehci,
                        struct usbh_ehci_qh *p_qh){
    struct usb_list_node *p_entry, *p_tmp;
    struct usbh_ehci_qtd *p_qtd      = NULL;
    struct usbh_ehci_qtd *p_qtd_prev = NULL;
    uint32_t              token      = 0;
    int                   status;
    int                   ret;
    usb_bool_t            is_work;

    if (usb_list_head_is_empty(&p_qh->qtds)) {
        return USB_OK;
    }

    is_work = (p_qh->state == __QH_ST_LINKED) ? USB_TRUE : USB_FALSE;
    status  = -USB_EINPROGRESS;

    usb_list_for_each_node_safe(p_entry, p_tmp, &p_qh->qtds){
        p_qtd = usb_container_of(p_entry, struct usbh_ehci_qtd, node);

        /* 处理上一个  qTD 结构体*/
        if (p_qtd_prev) {
            /* 如果前一个  qTD 和当前  qTD 不属于同一个传输请求包，说明上一个传输请求包已经处理完成，
             * 调用传输请求包完成回调函数 */
            if (p_qtd_prev->p_trp != p_qtd->p_trp) {
                if(p_qtd_prev->p_trp->p_ep->p_ep_desc->attributes == USB_EP_TYPE_INT){
                    if (p_ehci->intr_count) {
                        p_ehci->intr_count--;
                    }
                }
                __ehci_trp_done(p_ehci, p_qtd_prev->p_trp, status);
                status = -USB_EINPROGRESS;
            }

            ret = usbh_ehci_qtd_free(p_ehci, p_qtd_prev);
            if (ret != USB_OK) {
                __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
            }
            p_qtd_prev = NULL;
        }

        /* 读取 qTD 的令牌*/
        token = USB_LE_REG_READ32(&p_qtd->hw_token);

        if ((token & (3 << 8)) == (3 << 8)) {
            __USB_ERR_INFO("ehci qtd token illegal\r\n");
            return -USB_EILLEGAL;
        }

__retry_xacterr:
        /* 当前 qTD 不在传输状态*/
        if ((token & __QTD_STS_ACTIVE) == 0) {
            /* 当前 qTD 出现了错误*/
            if ((token & __QTD_STS_HALT) != 0) {
                if ((token & __QTD_STS_XACT) &&
                        (__QTD_CERR(token) == 0) &&
                        (++p_qh->xact_errs < QH_XACTERR_MAX)) {
                    /* 取消停止状态*/
                    token &= ~__QTD_STS_HALT;
                    /* 重新激活主机对此传输的执行并设置错误计数*/
                    token |= __QTD_STS_ACTIVE | (USBH_EHCI_TUNE_CERR << 10);
                    USB_LE_REG_WRITE32(token, &p_qtd->hw_token);
                    USB_LE_REG_WRITE32(token, &p_qh->hw_token);

                    goto __retry_xacterr;
                }
                is_work = USB_FALSE;
            } else if ((IS_SHORT_READ(token)) && !(p_qtd->hw_alt_next & USB_CPU_TO_LE32(1u))) {
                is_work = USB_FALSE;
            }
        } else {
            if (is_work) {
                /* 该 qTD 还在被处理*/
                break;
            }
        }

        if (status == -USB_EINPROGRESS) {
            status = __qtd_copy_status(p_ehci, p_qtd->p_trp, p_qtd->len, token);
            if ((status == -USB_EPERM)
                    && (p_qtd->hw_alt_next & USB_CPU_TO_LE32(1u))) {
                status = -USB_EINPROGRESS;
            }

            if ((status != -USB_EINPROGRESS) && (status != -USB_EPERM)) {
                /* 当接收到一个跟着 STALL 包的请求时（停止发送等时包），有一些集线器里的事务转换器会故障
                 * 因为一个 STALL 包不能让事务转换器的缓存处于繁忙状态，这里不会清除事务转换器缓存，这违反
                 * 了规范 */
                if (status != -USB_EPIPE) {
                    //ehci_clear_tt_buffer(ehci, qh, urb, token);
                    //todo
                }
            }
        }

        /* 前面还有别的 qTD 未完成*/
        if (p_qtd->node.p_prev != (struct usb_list_node *)&p_qh->qtds) {
            p_qtd_prev = usb_container_of(p_qtd->node.p_prev, struct usbh_ehci_qtd, node);
            /* 把前节点的 qTD next qTD Pointer 指向当前 qTD 的下一个 qTD 地址*/
            p_qtd_prev->hw_next = p_qtd->hw_next;
        } else {
            USB_LE_REG_WRITE32((uint32_t)p_qtd->hw_next, &p_qh->hw_next_qtd);
        }

        usb_list_node_del(&p_qtd->node);

        p_qtd_prev      = p_qtd;
        p_qh->xact_errs = 0;
    }

    if (p_qtd_prev) {
        if (USBH_EP_TYPE_GET(p_qtd_prev->p_trp->p_ep) == USB_EP_TYPE_INT) {
            if (p_ehci->intr_count) {
                p_ehci->intr_count--;
            }
        }
        /* 最后一个 qTD */
        __ehci_trp_done(p_ehci, p_qtd_prev->p_trp, status);
        usbh_ehci_qtd_free(p_ehci, p_qtd_prev);
        p_qtd_prev = NULL;
    }

    return USB_OK;
}
#else
int usbh_ehci_qh_handle(struct usbh_ehci    *p_ehci,
                        struct usbh_ehci_qh *p_qh){
    struct usb_list_node *p_entry, *p_tmp;
    struct usbh_ehci_qtd *p_qtd = NULL;
    struct usbh_ehci_qtd *p_prev = NULL;
    uint32_t              token = 0;
    int                   status;
    usb_bool_t            is_work;
    /* 要处理的QH(队列头)的qTD(队列传输描述符)链表是否为空*/
    if (usb_list_head_empty(&p_qh->qtds)) {
        return USB_OK;
    }
    /* 要处理的QH(队列头)的状态*/
    is_work = (p_qh->state == __QH_ST_LINKED) ? USB_TRUE : USB_FALSE;

__rescan:
    status = USB_OK;

    /* 遍历QH(队列头)中的qTD(队列传输描述符)链表节点*/
    usb_list_for_each_safe(p_entry, p_tmp, &p_qh->qtds){

        /* 根据qTD(队列传输描述符)节点获取完整的qTD(队列传输描述符)结构体*/
        p_qtd = usb_list_node_get(p_entry, struct usbh_ehci_qtd, node);

        /* 处理上一个qTD(队列传输描述符)结构体*/
        if (p_prev) {
            /* 如果前一个qTD(队列传输描述符)和当前qTD(队列传输描述符)不属于同一个trp包，说明上一个trp包已经处理完成，
                                            调用传输请求包完成回调函数，状态重新置为AW_OK*/
            if (p_prev->p_trp != p_qtd->p_trp) {
                if(p_prev->p_trp->p_ep->p_ep_desc->attributes == USB_EP_TYPE_INT){
                    if (p_ehci->intr_count) {
                        p_ehci->intr_count--;
                    }
                }
                /* 调用传输请求包完成回调函数*/
                __ehci_trp_done(p_ehci, p_prev->p_trp, status);
                status = USB_OK;
            }

            /* 释放qTD(队列传输描述符)结构体*/
            usbh_ehci_qtd_free(p_ehci, p_prev);
            p_prev = NULL;
        }
        /* 读取qTD(队列传输描述符)的令牌*/
        token = USB_LE_REG_READ32(&p_qtd->hw_token);

        USB_ERR_PRINT_RETVAL(((token & (3 << 8)) == (3 << 8)),
                              -USB_EILLEGAL,
                              "ehci qtd token illegal\r\n");
        if (p_qtd->len == 12) {
            int a = 1;
        }
        if (status == USB_OK) {
            /* QH(队列头)状态为未链接*/
            if (p_qh->state == __QH_ST_UNLINKED) {
                /* 主机控制器取消对此传输的处理*/
                token &= ~__QTD_STS_ACTIVE;
                /* 重试次数置为最大*/
                p_qh->xact_errs = 32;
            }

            /* qTD(队列传输描述符)为非激活状态，该qTD(队列传输描述符)完成或者发生错误*/
            if ((token & __QTD_STS_ACTIVE) == 0) {
                /* 如果qTD(队列传输描述符)错误停止 && 收到非法响应 && 错误计数减到0 && 重试次数小于32*/
                if ((token & __QTD_STS_HALT) &&
                    (token & __QTD_STS_XACT) &&
                    (__QTD_CERR(token) == 0) &&
                    (++p_qh->xact_errs < 32)) {
                    /* 取消停止状态*/
                    token &= ~__QTD_STS_HALT;
                    /* 重新激活主机对此传输的执行并设置错误计数*/
                    token |= __QTD_STS_ACTIVE | (3 << 10);
                    USB_LE_REG_WRITE32(token, &p_qtd->hw_token);
                    USB_LE_REG_WRITE32(token, &p_qh->hw_token);

                    if (is_work ) {
                        /* 重试 */
                        break;
                    }
                }
            } else {
                if (is_work) {
                    /* 该qTD(队列传输描述符)还在被处理*/
                    break;
                }
            }

            if (p_qtd->len == 12) {
                int a = 1;
            }

            /* 如果队列头的状态为空闲或者未链接状态，把状态置为错误*/
            if ((p_qh->state == __QH_ST_IDLE) ||
                (p_qh->state == __QH_ST_UNLINKED)) {
                status = -USB_EPROTO;
            } else {
                /* 严重错误*/
                if (token & __QTD_STS_HALT) {
                    if (token & __QTD_STS_BABBLE) {
                        status = -USB_EPIPE;
                    } else if (__QTD_CERR(token)) {
                        status = -USB_EPIPE;
                    } else if (token & __QTD_STS_MMF) {
                        status = -USB_EPROTO;
                    } else if (token & __QTD_STS_DBE) {
                        status = -USB_EPROTO;
                    } else if (token & __QTD_STS_XACT) {
                        status = -USB_EPROTO;
                    } else {
                        status = -USB_EPROTO;
                    }
                }
            }

            if (status == USB_OK) {
                /* 如果不是 SETUP 令牌包，则是输入/输出令牌包，计算传输请求包实际传输长度*/
                if (__QTD_PID(token) != __QTD_PID_SETUP) {
                    p_qtd->p_trp->act_len += (p_qtd->len - __QTD_LENGTH(token));
                }
            }
        } else {
            /* 主机控制器取消对此传输的处理*/
            token &= ~__QTD_STS_ACTIVE;
            USB_LE_REG_WRITE32(token, &p_qtd->hw_token);
        }
        /* 前面还有别的qTD(队列传输描述符)*/
        if (p_qtd->node.prev != (struct usb_list_node *)&p_qh->qtds) {
            /* 获取当前节点的前一个节点的完整qTD结构体*/
            p_prev = usb_list_node_get(p_qtd->node.prev, struct usbh_ehci_qtd, node);
            /* 把前节点的qTD next qTD Pointer指向当前qTD(队列传输描述符)的下一个qTD(队列传输描述符)地址*/
            p_prev->hw_next = p_qtd->hw_next;
        }
        else{
            USB_LE_REG_WRITE32((uint32_t)p_qtd->hw_next, &p_qh->hw_next_qtd);
        }
        /* 从链表中删除当前qTD(队列传输描述符)节点*/
        usb_list_node_del(&p_qtd->node);
        /* 前指针指向当前qTD(队列传输描述符)(下一次会进行处理)*/
        p_prev = p_qtd;
        /* 重试次数清0*/
        p_qh->xact_errs = 0;
    }

    if (p_prev) {
        if(p_prev->p_trp->p_ep->p_ep_desc->attributes == USB_EP_TYPE_INT){
            if (p_ehci->intr_count) {
                p_ehci->intr_count--;
            }
        }
        /* 最后一个qTD(队列传输描述符)*/
        __ehci_trp_done(p_ehci, p_prev->p_trp, status);
        usbh_ehci_qtd_free(p_ehci, p_prev);
        p_prev = NULL;
    }

    /* 如果QH(队列头)为未链接状态*/
    if (p_qh->state == __QH_ST_UNLINKED) {
        /* 把QH(队列头)的Next qTD Pointer无效*/
        p_qh->hw_next_qtd = USB_CPU_TO_LE32(1u);
        /* 如果qTD(队列传输描述符)链表为空，则把QH(队列头)设置为空闲状态*/
        if (usb_list_head_empty(&p_qh->qtds)) {
            p_qh->state = __QH_ST_IDLE;
        } else {
            goto __rescan;
        }
    }

    return USB_OK;
}
#endif

/**
 * \brief 扫描异步调度
 */
void usbh_ehci_async_scan(struct usbh_ehci *p_ehci){
    struct usbh_ehci_qh *p_qh_next, *p_qh;
    int                  ret;

    p_qh_next = p_ehci->p_async->p_next.p_qh;
    /* 处理每一个队列头*/
    while (p_qh_next) {
        p_qh      = p_qh_next;
        p_qh_next = p_qh->p_next.p_qh;

        ret = usbh_ehci_qh_handle(p_ehci, p_qh);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
        }
        //todo
    }
}

/////////////////////////////////////////////////////////////////////////////////////


///**
// * \brief
// */
//static int __qh_schedule(struct usbh_ehci *pEhci, struct usbh_ehci_qh *pQh){
//
//}
//
///**
// * \brief 检查周期带宽
// *
// * \param[in] pEhci   EHCI控制器结构体
// * \param[in] Frame   帧号
// * \param[in] uFrame  微帧号
// * \param[in] uPeriod 中断的周期
// * \param[in] uSecs   要使用的时间(微秒)
// *
// * \retval 带宽满足返回USB_TRUE，不满足返回USB_FALSE
// */
//static usb_bool_t __period_check(struct usbh_ehci        *pEhci,
//                                 uint32_t                 Frame,
//                                 uint32_t                 uFrame,
//                                 uint32_t                 uPeriod,
//                                 uint32_t                 uSecs){
//    /* 1 帧等于 8微帧*/
//    if (uFrame >= 8) {
//        return 0;
//    }
//
//    /* 计算最大的微帧带宽减去申请的带宽的剩余*/
//    uSecs = USBH_EHCI_UFRAME_BANDWIDTH - uSecs;
//
//    for (uFrame += Frame << 3;
//         uFrame < USBH_EHCI_BANDWIDTH_SIZE;
//         uFrame += uPeriod) {
//        /* 现在微帧位置的带宽剩余不足以分配要申请的带宽*/
//        if (pEhci->BdWidth[uFrame] > uSecs) {
//            return USB_FALSE;
//        }
//    }
//    return USB_TRUE;
//}
//
//static int intr_schedule_check(struct usbh_ehci        *pEhci,
//                               uint32_t                 Frame,
//                               uint32_t                 uFrame,
//                               struct usbh_ehci_qh    *pQh,
//                               uint32_t                *pCmaskp,
//                               struct usbh_ehci_tt  *pTt){
//    int        Ret  = -USB_ENOMEM;
//    uint8_t Mask = 0;
//
//    if (qh->ps.c_usecs && uFrame >= 6)    /* FSTN territory? */
//        goto done;
//
//    if (!check_period(ehci, frame, uframe, qh->ps.bw_uperiod, qh->ps.usecs))
//        goto done;
//    if (!qh->ps.c_usecs) {
//        retval = 0;
//        *c_maskp = 0;
//        goto done;
//    }
//
//#ifdef CONFIG_USB_EHCI_TT_NEWSCHED
//    if (tt_available(ehci, &qh->ps, tt, frame, uframe)) {
//        unsigned i;
//
//        /* TODO : this may need FSTN for SSPLIT in uframe 5. */
//        for (i = uframe+2; i < 8 && i <= uframe+4; i++)
//            if (!check_period(ehci, frame, i,
//                    qh->ps.bw_uperiod, qh->ps.c_usecs))
//                goto done;
//            else
//                mask |= 1 << i;
//
//        retval = 0;
//
//        *c_maskp = mask;
//    }
//#else
//    /* Make sure this tt's buffer is also available for CSPLITs.
//     * We pessimize a bit; probably the typical full speed case
//     * doesn't need the second CSPLIT.
//     *
//     * NOTE:  both SPLIT and CSPLIT could be checked in just
//     * one smart pass...
//     */
//    mask = 0x03 << (uframe + qh->gap_uf);
//    *c_maskp = mask;
//
//    mask |= 1 << uframe;
//    if (tt_no_collision(ehci, qh->ps.bw_period, qh->ps.udev, frame, mask)) {
//        if (!check_period(ehci, frame, uframe + qh->gap_uf + 1,
//                qh->ps.bw_uperiod, qh->ps.c_usecs))
//            goto done;
//        if (!check_period(ehci, frame, uframe + qh->gap_uf,
//                qh->ps.bw_uperiod, qh->ps.c_usecs))
//            goto done;
//        retval = 0;
//    }
//#endif
//done:
//    return retval;
//}

/**
 * \brief 检查周期带宽
 */
static uint16_t __periodic_usecs(struct usbh_ehci *p_ehci, uint32_t frame, uint32_t u_frame){
    uint32_t                   *p_hw   = &p_ehci->p_periodic[frame];
    union usbh_ehci_struct_ptr *q      = &p_ehci->p_shadow[frame];
    uint32_t                    u_secs = 0;
    struct usbh_ehci_qh        *p_qh   = NULL;

    while (q->ptr) {
        switch (USB_CPU_TO_LE32(Q_NEXT_TYPE(*p_hw))) {
            /* 队列头*/
            case __Q_TYPE_QH:
                p_qh = q->p_qh;
                /* 高速设备，微帧编号在 S-mask 寄存器里*/
                if (p_qh->hw_info2 & USB_CPU_TO_LE32(1 << u_frame)) {
                    u_secs += q->p_qh->usecs;
                }
                /* 全/低速设备，完成分割的微帧编号在 C-mask 寄存器里*/
                if (p_qh->hw_info2 & USB_CPU_TO_LE32(1 << (8 + u_frame))) {
                    u_secs += q->p_qh->c_usecs;
                }
                p_hw = &p_qh->hw_next;
                q    = &q->p_qh->p_next;
                break;
            case __Q_TYPE_FSTN:
                //todo
                break;
            default:
                break;
            /* 等时传输描述符*/
            case __Q_TYPE_ITD:
                if (q->p_itd->hw_transaction[u_frame]) {
                    //uSecs += q->pItd->pStream->uSecs;
                }
                p_hw = &q->p_itd->hw_next;
                q    = &q->p_itd->p_next;
                break;
            case __Q_TYPE_SITD:
                /* is it in the S-mask?  (count SPLIT, DATA) */
                if (q->p_sitd->hw_u_frame & USB_CPU_TO_LE32(1 << u_frame)) {
                    if (q->p_sitd->hw_full_speed_ep & USB_CPU_TO_LE32(1 << 31)){
                        //uSecs += q->pSitd->pStream->uSecs;
                    } else {    /* worst case for OUT start-split */
                        //uSecs += HS_USECS_ISO (188);
                    }
                }

                /* ... C-mask?  (count CSPLIT, DATA) */
                if (q->p_sitd->hw_u_frame & USB_CPU_TO_LE32(1 << (8 + u_frame))) {
                    /* worst case for IN complete-split */
                    //uSecs += q->pSitd->pStream->c_usecs;
                }

                p_hw = &q->p_sitd->hw_next;
                q    = &q->p_sitd->p_next;
                break;
        }
    }
    return u_secs;
}

/**
 * \brief 检查周期带宽
 */
static usb_bool_t __period_check(struct usbh_ehci *p_ehci,
                                 uint32_t          frame,
                                 uint32_t          u_frame,
                                 uint32_t          period,
                                 uint32_t          u_secs){
    uint32_t  claimed;

    /* 1 帧等于 8微帧*/
    if (u_frame >= 8) {
        return USB_FALSE;
    }
    /* 计算最大的微帧带宽减去申请的带宽的剩余*/
    u_secs = USBH_EHCI_UFRAME_BANDWIDTH - u_secs;

    /* 检查每一个微帧*/
    if (period == 0) {
        do {
            for (u_frame = 0; u_frame < 7; u_frame++) {
                claimed = __periodic_usecs(p_ehci, frame, u_frame);
                if (claimed > u_secs) {
                    return 0;
                }
            }
        } while ((frame += 1) < p_ehci->frame_list_size);

    /* 就检查周期里特定微帧*/
    } else {
        do {
            claimed = __periodic_usecs(p_ehci, frame, u_frame);
            if (claimed > u_secs)
                return 0;
        } while ((frame += period) < p_ehci->frame_list_size);
    }

    /* 成功*/
    return USB_TRUE;
}

/**
 * \brief 周期调度链接 QH(队列头)
 */
static int __period_intr_link(struct usbh_ehci    *p_ehci,
                              struct usbh_ehci_qh *p_qh){
    uint16_t idx, i;
#if USB_OS_EN
    int      ret;
#endif
    /* QH(队列头)水平链接指针无效*/
    p_qh->hw_next = USB_CPU_TO_LE32(1);
    /* 获取 QH(队列头)帧周期*/
    idx = p_qh->frame_period;
    /* 调整 QH(队列头)帧周期*/
    if (idx == 0) {
        idx = 1;
    }

    for (i = p_qh->frame_phase; i < p_ehci->frame_list_size; i += idx) {
        uint32_t                   hw_cur;
        uint32_t                  *p_hw_prev;
        union usbh_ehci_struct_ptr prev;
        union usbh_ehci_struct_ptr cur;
        uint32_t                   type;

        prev.ptr  = NULL;
        /* 读出周期帧列表元素的引用对象地址*/
        cur.ptr   = (void *)(USB_LE_REG_READ32(&p_ehci->p_periodic[i]) & 0xFFFFFFE0);
        /* 周期帧列表元素地址*/
        p_hw_prev = &p_ehci->p_periodic[i];
        /* 周期帧列表元素引用的对象地址*/
        hw_cur    = p_ehci->p_periodic[i];

        /* 扫描所有等时数据结构，QH(队列头)要链接在所有等时描述符之后*/
        /* 周期帧列表元素存在引用对象*/
        while (cur.ptr != NULL) {
            /* 判断引用对象类型，如果是队列头，则退出*/
            type = USB_CPU_TO_LE32(hw_cur) & (3 << 1);
            if (type == __Q_TYPE_QH) {
                break;
            }

            prev = cur;
            /* 获取引用对象的下一个水平链接对象的地址*/
            p_hw_prev = __ehci_next_hw(cur, type);
            /* 获取引用对象的下一个水平链接对象*/
            hw_cur = *p_hw_prev;
            /* 获取下一个引用对象*/
            cur = __ehci_next_struct(cur, type);
        }

        /* 周期帧列表元素存在 QH(队列头)*/
        while ((cur.ptr != NULL) && (p_qh != cur.p_qh)) {
            /* 如果新的 QH 帧轮询率比原来的 QH 大，跳出直接插入到原来的 QH 之前*/
            if (p_qh->frame_period > cur.p_qh->frame_period) {
                break;
            }
            /* 如果新的 QH 帧轮询率比原来的 QH 小，移动到适当的位置*/
            prev      = cur;
            /* 水平链接指针地址*/
            p_hw_prev = &cur.p_qh->hw_next;
            /* 水平链接指针*/
            hw_cur    = cur.p_qh->hw_next;
            /* 移动到下一个引用对象*/
            cur       = cur.p_qh->p_next;
        }

        if (p_qh != cur.p_qh) {
            /* 设置下一个引用对象*/
            p_qh->p_next  = cur;
            /* 设置水平链接指针*/
            p_qh->hw_next = hw_cur;
            /* 如果有前一个引用对象，前一个 QH(队列头)的引用对象指向当前 QH(队列头)*/
            if (prev.ptr != NULL) {
                prev.p_qh->p_next.p_qh = p_qh;
            }
            /* 如果有其他 QH(队列头)且轮询率比其他 QH(队列头)大，则把 QH(队列头)的地址写入到周期帧列表元素里
             * 如果没有其他 QH(队列头)，则把 QH(队列头)的地址写入到周期帧列表元素里
             * 如果有其他 QH(队列头)且轮询率比其他 QH(队列头)小，则把 QH(队列头)的地址写入到前一个 QH(队列头)的水平链接指针里*/
            USB_LE_REG_WRITE32(((uint32_t)p_qh & ~0x01f) | __Q_TYPE_QH, p_hw_prev);
        }
    }
    /* QH(队列头)状态设置为已链接*/
    p_qh->state     = __QH_ST_LINKED;
    p_qh->xact_errs = 0;
#if USB_OS_EN
    ret = usb_mutex_lock(p_ehci->p_lock, USBH_EHCI_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 把QH(队列头)放入到EHCI中断QH(队列头)链表中*/
    usb_list_node_add(&p_qh->intr_node, &p_ehci->intr_qh_list);
#if USB_OS_EN
    ret = usb_mutex_unlock(p_ehci->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 启动等时传输*/
    if (!p_ehci->intr_count++) {
        usbh_ehci_periodic_enable(p_ehci, USB_FALSE);
    }
    return USB_OK;
}

/**
 * \brief 取消周期调度链接
 */
static void __period_intr_unlink(struct usbh_ehci *p_ehci,
                                 int               frame_idx,
                                 void             *ptr){
    uint32_t                    hw_cur;
    uint32_t                   *p_hw_prev = NULL;
    union usbh_ehci_struct_ptr  prev;
    union usbh_ehci_struct_ptr  cur;
    uint32_t                    type;
    /* 读取周期帧列表特定索引的元素的引用对象地址*/
    prev.ptr  = NULL;
    cur.ptr   = (void *)(USB_LE_REG_READ32(&p_ehci->p_periodic[frame_idx]) & 0xFFFFFFE0);
    /* 周期帧列表元素地址*/
    p_hw_prev = &p_ehci->p_periodic[frame_idx];
    /* 周期帧列表元素引用的对象地址*/
    hw_cur    = p_ehci->p_periodic[frame_idx];
    /* 获取引用对象类型*/
    type      = USB_CPU_TO_LE32(hw_cur) & (3 << 1);
    /* 遍历该帧列表索引的所有引用对象，找到要取消链接的引用对象*/
    while ((cur.ptr != NULL) && (cur.ptr != ptr)) {
        /* 获取引用对象类型*/
        type      = USB_CPU_TO_LE32(hw_cur) & (3 << 1);
        prev      = cur;
        /* 获取当前引用对象的水平链接指针地址*/
        p_hw_prev = __ehci_next_hw(cur, type);
        /* 获取当前引用对象的水平链接指针*/
        hw_cur    = *p_hw_prev;
        /* 获取下一个引用对象*/
        cur       = __ehci_next_struct(cur, type);
    }
    /* 没找到要取消链接的引用对象，返回*/
    if (cur.ptr == NULL) {
        return;
    }
    /* 要取消链接的用对象不是周期帧列表元素的第一个引用对象*/
    if (prev.ptr != NULL) {
        /* 把前一个引用对象与后一个引用对象链接起来*/
        prev.p_qh->p_next = __ehci_next_struct(cur, type);
    }
    /* 前一个引用对象的水平链接指针赋值当前取消链接的引用对象的水平链接指针*/
    *p_hw_prev = *__ehci_next_hw(cur, type);
    /* 如果没有周期调度请求，则关闭周期调度*/
    if (p_ehci->intr_count) {
        p_ehci->intr_count--;
    }
    if ((p_ehci->intr_count == 0) && (p_ehci->isoc_count == 0)) {
        usbh_ehci_periodic_disable(p_ehci);
    }
}

/**
 * \brief 取消中断请求
 *
 * \param[in] p_ehci EHCI 控制器结构体
 * \param[in] p_qh   要取消链接的 QH(队列头)
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_intr_cancel(struct usbh_ehci    *p_ehci,
                          struct usbh_ehci_qh *p_qh){
    uint32_t i, tmp;

    /* 检查 QH 的状态*/
    if (p_qh->state != __QH_ST_LINKED) {
        return -USB_EILLEGAL;
    }

    /* 获取 QH 帧周期*/
    tmp = p_qh->frame_period ? p_qh->frame_period : 1;
    /* 取消周期调度链接*/
    for (i = p_qh->frame_phase; i < p_ehci->frame_list_size; i += tmp) {
        __period_intr_unlink(p_ehci, i, p_qh);
    }
    /* 取消 QH 的下一个数据结构*/
    p_qh->p_next.ptr = NULL;
    /* QH 的水平链接指针为无效*/
    p_qh->hw_next    = USB_CPU_TO_LE32(1u);
    /* 从EHCI中断链表中删除当前QH节点*/
    usb_list_node_del(&p_qh->intr_node);
    /* 更新 QH 状态*/
    p_qh->state = __QH_ST_UNLINKED;
    ///* 删除端点带宽*/
    //__bdwidth_dec(p_ehci, p_qh);

    return USB_OK;
}

/**
 * \brief EHCI 周期中断请求
 *
 * \param[in] p_ehci     EHCI 控制器结构体
 * \param[in] p_qh       队列头
 * \param[in] p_trp      USB 传输请求包
 * \param[in] p_qtd_list qTD 链表
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_intr_req(struct usbh_ehci     *p_ehci,
                       struct usbh_ehci_qh  *p_qh,
                       struct usbh_trp      *p_trp,
                       struct usb_list_head *p_qtd_list){
    uint32_t  tmp, info2;
    uint16_t  frame_idx, i, j;
    uint8_t   speed;
    int       ret = USB_OK;

    if ((p_qh == NULL) ||
            (p_ehci == NULL) ||
            (p_trp == NULL) ||
            (p_qtd_list == NULL)) {
        return -USB_EINVAL;
    }

    /* QH 的状态为空闲或未链接*/
    if ((p_qh->state == __QH_ST_IDLE) ||
        (p_qh->state == __QH_ST_UNLINKED)) {
        /* 获取所属设备的速度*/
        speed = p_trp->p_ep->p_usb_dev->speed;
        /* QH 的没有帧周期*/
        if (p_qh->frame_period == 0) {
            /* 检查微帧带宽*/
            if (__period_check(p_ehci, 0, 0, 0, p_trp->p_ep->band_width)) {
                goto __got_phase;
            }
        } else {
            /* 寻找一个合适的帧和微帧*/
            for (i = 0; i < p_qh->frame_period; i++) {
                /* 在最大周期内随机获取一个帧索引号*/
                frame_idx = ++p_ehci->random & (p_qh->frame_period - 1);
                /* 1 帧 = 8 微帧，遍历微帧*/
                for (j = 0; j < 8; j++) {
                    /* 检查微帧带宽*/
                    if (__period_check(p_ehci, frame_idx, j, p_qh->frame_period, p_trp->p_ep->band_width)) {
                        /* 填充 QH 的帧索引号和微帧索引号*/
                        p_qh->frame_phase   = frame_idx;
                        p_qh->u_frame_phase = j;
                        goto __got_phase;
                    }
                }
            }
        }
        return -USB_ENOMEM;

__got_phase:
        /* s-mask，设定起始分割的微帧号 */
        info2 = (1 << p_qh->u_frame_phase);

        /* c-mask，全/低速设备，设置完成分割的微帧编号*/
        if (speed != USB_SPEED_HIGH) {
            info2 |= (1 << (p_qh->u_frame_phase + 9));
            info2 |= (1 << (p_qh->u_frame_phase + 10));
            info2 |= (1 << (p_qh->u_frame_phase + 11));
        }

        /* 设置获取端点每微帧的事务数量*/
        info2 |= (((USBH_EP_MPS_GET(p_qh->p_ep) >> 11) & 0x03) + 1) << 30;
        /* s-mask，c-mask 和 mult 位清0*/
        tmp = (USB_LE_REG_READ32(&p_qh->hw_info2)
                & ~(__QH_SMASK | __QH_CMASK | __QH_MULT));
        /* 设置s-mask，c-mask 和 mult 的值*/
        tmp |= info2;
        /* 重新设置 QH 端点信息*/
        USB_LE_REG_WRITE32(tmp, &p_qh->hw_info2);
    }
    /* 把 qtd 链表链接到端点结构体的 QH 中*/
    __qtds_link(p_qh, p_qtd_list);

    /* QH 状态为空闲或未链接*/
    if ((p_qh->state == __QH_ST_IDLE) ||
        (p_qh->state == __QH_ST_UNLINKED)) {
        /* 调用 QH 周期链接函数*/
        ret = __period_intr_link(p_ehci, p_qh);
    } else if (p_qh->state == __QH_ST_LINKED) {
        /* 启动等时传输*/
        if (!p_ehci->intr_count++) {
            usbh_ehci_periodic_enable(p_ehci, USB_FALSE);
        }
    }

    return ret;
}

/**
 * \brief 扫描中断传输
 *
 * \param[in] p_ehci EHCI 结构体
 */
void usbh_ehci_intr_scan(struct usbh_ehci *p_ehci){
    struct usb_list_node  *p_node     = NULL;
    struct usb_list_node  *p_node_tmp = NULL;
    struct usbh_ehci_qh   *p_qh       = NULL;
    int                    ret;

    /* 遍历 EHCI 中断 QH(队列头) 链表*/
    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_ehci->intr_qh_list) {
        p_qh = usb_container_of(p_node, struct usbh_ehci_qh, intr_node);

        /* 调用 QH(队列头) 处理函数*/
        ret = usbh_ehci_qh_handle(p_ehci, p_qh);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
            return;
        }
        //todo
    }
}

/**
 * \brief 以纳秒为单位的大概的周期事务时间
 */
static long __usb_bus_time_calc(int speed, int is_input, int isoc, int bytecount){
    unsigned long tmp;

    switch (speed) {
        /* 只用于中断端点*/
        case USB_SPEED_LOW:
            if (is_input) {
                tmp = (67667L * (31L + 10L * BitTime (bytecount))) / 1000L;
                return (64060L + (2 * BW_HUB_LS_SETUP) + BW_HOST_DELAY + tmp);
            } else {
                tmp = (66700L * (31L + 10L * BitTime (bytecount))) / 1000L;
                return (64107L + (2 * BW_HUB_LS_SETUP) + BW_HOST_DELAY + tmp);
            }
        /* 等时或中断端点*/
        case USB_SPEED_FULL:
            if (isoc) {
                tmp = (8354L * (31L + 10L * BitTime (bytecount))) / 1000L;
                return (((is_input) ? 7268L : 6265L) + BW_HOST_DELAY + tmp);
            } else {
                tmp = (8354L * (31L + 10L * BitTime (bytecount))) / 1000L;
                return (9107L + BW_HOST_DELAY + tmp);
            }
        /* 等时或中断端点*/
        case USB_SPEED_HIGH:
            // FIXME adjust for input vs output
            if (isoc) {
                tmp = HS_NSECS_ISO(bytecount);
            } else {
                tmp = HS_NSECS(bytecount);
            }
            return tmp;
        default:
            return -USB_ENOTSUP;
    }
}

/**
 * \brief 分配 EHCI 等时数据流结构体内存
 *        usbh_ehci_iso_stream 操作与 等时传
 *        输描述符和分割事务等时传输描述符一起工作
 */
static struct usbh_ehci_iso_stream *__iso_stream_alloc(void){
    struct usbh_ehci_iso_stream *p_stream = NULL;

    p_stream = usb_lib_malloc(&__g_usb_host_lib.lib, sizeof(struct usbh_ehci_iso_stream));
    if (p_stream == NULL) {
        return NULL;
    }
    memset(p_stream, 0, (sizeof(struct usbh_ehci_iso_stream)));

#if USB_OS_EN
    p_stream->p_lock = usb_lib_mutex_create(&__g_usb_host_lib.lib);
    if (p_stream->p_lock == NULL) {
        usb_lib_mfree(&__g_usb_host_lib.lib, p_stream);
        return NULL;
    }
#endif
    if (p_stream != NULL) {
        /* 初始化数据流的传输描述符链表*/
        usb_list_head_init(&p_stream->td_list);
        /* 初始化数据流的未使用的传输描述符链表*/
        usb_list_head_init(&p_stream->free_list);

        p_stream->uframe_next = -1;
        p_stream->ref_cnt     = 1;
    }
    return p_stream;
}

/**
 * \brief 初始化等时数据流
 */
static void __iso_stream_init(struct usbh_ehci            *p_ehci,
                              struct usbh_ehci_iso_stream *p_stream,
                              struct usbh_endpoint        *p_ep){
    static const uint8_t smask_out [] = { 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f };
    uint32_t             buf;
    unsigned int         ep_num, maxp;
    int                  is_input;
    struct usbh_device  *p_usb_dev    = NULL;
    long                 band_width;

    p_usb_dev = p_ep->p_usb_dev;
    /* 这个可能是一个“高带宽”高速端点，正如端点描述符的 max_packet 字段编码的那样。
     * 获取端点地址*/
    ep_num   = USBH_EP_ADDR_GET(p_ep);
    /* 获取端点方向*/
    is_input = USBH_EP_DIR_GET(p_ep)? USB_DIR_IN : 0;
    /* 获取端点最大包大小*/
    maxp = USBH_EP_MPS_GET(p_ep);
    if (is_input) {
        buf = (1 << 11);
    } else {
        buf = 0;
    }
    /* 等时传输描述符*/
    if (p_usb_dev->speed == USB_SPEED_HIGH) {
        /* 获取每微帧事务数量*/
        unsigned multi = hb_mult(maxp);
        /* 标记为高速传输*/
        p_stream->highspeed = 1;
        /* 获取端点最大包大小*/
        maxp = max_packet(maxp);
        buf |= maxp;
        /* 计算出每微帧的数据传输大小*/
        maxp *= multi;
        /* 设置设备地址和端点地址*/
        p_stream->buf0  = USB_CPU_TO_LE32((ep_num << 8) | p_usb_dev->addr);
        /* 设置最大包大小*/
        p_stream->buf1  = USB_CPU_TO_LE32(buf);
        /* 设置每个事务描述的事务数量*/
        p_stream->buf2  = USB_CPU_TO_LE32(multi);
        p_stream->usecs = HS_USECS_ISO(maxp);
        band_width  = p_stream->usecs * 8;
        band_width /= p_ep->interval;
        p_stream->interval = p_ep->interval;
        p_stream->f_interval = p_ep->interval >> 3;

    } else {
        /* 低速/全速等时传输*/
        uint32_t addr = 0;
        uint32_t tmp;
        int      think_time;
        int      hs_transfers;

        addr = p_usb_dev->tt_port << 24;
        /* 不是根集线器*/
        if (p_usb_dev->p_tt->p_hub != NULL) {
            addr |= p_usb_dev->p_tt->p_hub->p_usb_dev->addr << 16;
        }
        /* 获取端点地址*/
        addr |= ep_num << 8;
        /* 获取当前设备地址*/
        addr |= p_usb_dev->addr;
        p_stream->usecs = HS_USECS_ISO(maxp);
        /* 计算设备在全速/低速总线上的时间*/
        think_time = p_usb_dev->p_tt ? p_usb_dev->p_tt->think_time : 0;
        p_stream->tt_usecs = NS_TO_US(think_time +
                                    __usb_bus_time_calc(p_usb_dev->speed, is_input, 1, maxp));
        hs_transfers = max (1u, (maxp + 187) / 188);
        if (is_input) {
            addr |= 1 << 31;
            p_stream->c_usecs = p_stream->usecs;
            p_stream->usecs   = HS_USECS_ISO(1);
            p_stream->cs_mask = 1;
            /* 计算总线时间*/
            tmp = __usb_bus_time_calc(USB_SPEED_FULL, 1, 0, maxp) / (125 * 1000);
            p_stream->cs_mask |= 3 << (tmp + 9);
        }else{
            p_stream->cs_mask = smask_out[hs_transfers - 1];
        }
        band_width  = p_stream->usecs + p_stream->c_usecs;
        band_width /= 1 << (p_ep->interval - 1);

        /* stream->splits 稍后从 raw_mask 创建*/
        p_stream->address    = USB_CPU_TO_LE32 (addr);
        p_stream->interval   = p_ep->interval << 3;
        p_stream->f_interval = p_ep->interval;
    }
    p_stream->band_width = band_width;
    p_stream->p_usb_dev  = p_usb_dev;
    /* 设置等时数据流端点地址和最大包大小*/
    p_stream->ep_addr = is_input | ep_num;
    p_stream->maxp    = maxp;
}

/**
 * \brief 获取等时数据流
 */
static struct usbh_ehci_iso_stream *__iso_stream_get(struct usbh_ehci     *p_ehci,
                                                     struct usbh_endpoint *p_ep){
    unsigned int                 ep_num;
    struct usbh_endpoint        *p_ep_tmp = NULL;
    struct usbh_ehci_iso_stream *p_stream = NULL;

    p_ep_tmp = p_ep;

    /* 获取端点地址*/
    ep_num = USBH_EP_ADDR_GET(p_ep);
    /* 判断端点*/
    if (USBH_EP_DIR_GET(p_ep)) {
        p_ep_tmp = p_ep->p_usb_dev->p_ep_in[ep_num];
    } else {
        p_ep_tmp = p_ep->p_usb_dev->p_ep_out[ep_num];
    }
    /* 获取等时数据流结构体*/
    p_stream = p_ep_tmp->p_hw_priv;

    p_ep_tmp->interval = p_ep->interval;

    if (p_stream == NULL) {
        /* 为等时数据流分配空间*/
        p_stream = __iso_stream_alloc();
        if (p_stream != NULL) {
            p_ep->p_hw_priv = p_stream;
            p_stream->p_ep  = p_ep_tmp;
            /* 初始化等时数据流*/
            __iso_stream_init(p_ehci, p_stream, p_ep_tmp);
        } else {
            return NULL;
        }
    } else if (p_stream->p_hw != NULL) {
        p_stream = NULL;
    }
    /* 等时流引用计数加1*/
    //usb_atomic_inc(&stream->refcount);
    return p_stream;
}

/**
 * \brief 初始化等时数据流
 *
 * \param[in]  p_ehci   EHCI 结构体
 * \param[in]  p_ep     相关的端点
 * \param[out] p_stream 返回分配成功的等时数据流结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_iso_stream_init(struct usbh_ehci             *p_ehci,
                              struct usbh_endpoint         *p_ep,
                              struct usbh_ehci_iso_stream **p_stream){
    struct usbh_ehci_iso_stream *p_stream_tmp = NULL;
    /* 获取等时数据流的头*/
    p_stream_tmp = __iso_stream_get(p_ehci, p_ep);
    if (p_stream_tmp == NULL) {
        return -USB_ENODEV;
    }
    /* 检查等时数据流的微帧周期和USB请求块的端点时间间隔是否一致*/
    if (((p_stream_tmp->highspeed) && (p_ep->interval != p_stream_tmp->interval)) ||
            ((!p_stream_tmp->highspeed) && (p_ep->interval != p_stream_tmp->f_interval))) {
        __USB_ERR_INFO("can't change iso interval %d --> %d\r\n",
                           p_stream_tmp->uperiod, p_ep->interval);
        return -USB_EAGAIN;
    }

    *p_stream = p_stream_tmp;

    return USB_OK;
}

/**
 * \brief 反初始化等时数据流
 *
 * \param[in] p_ehci   EHCI 结构体
 * \param[in] p_stream 要反初始化的等时数据流结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_iso_stream_deinit(struct usbh_ehci            *p_ehci,
                                struct usbh_ehci_iso_stream *p_stream){
    int ret = USB_OK;
#if USB_OS_EN
    ret = usb_mutex_lock(p_stream->p_lock, USBH_EHCI_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    while (!usb_list_head_is_empty(&p_stream->free_list)) {
        struct usb_list_node *p_node = NULL;

        p_node = p_stream->free_list.p_next;

        usb_list_node_del(p_node);

        /* 高速设备*/
        if (p_stream->highspeed) {
            struct usbh_ehci_itd *p_itd = NULL;

            p_itd = usb_container_of(p_node, struct usbh_ehci_itd, node);

            memset(p_itd, 0, sizeof(struct usbh_ehci_itd));
            /* 释放空闲的等时传输描述符*/
            usbh_ehci_itd_free(p_ehci, p_itd);
        } else {
            struct usbh_ehci_sitd *p_sitd = NULL;

            p_sitd = usb_container_of(p_node, struct usbh_ehci_sitd, node);

            memset(p_sitd, 0, sizeof(struct usbh_ehci_sitd));
            /* 释放空闲的分割等时传输描述符*/
            usbh_ehci_sitd_free(p_ehci, p_sitd);
        }
    }

    if (p_stream->ref_cnt == 1) {
        while (!usb_list_head_is_empty(&p_stream->td_list)) {
            struct usb_list_node *p_node = NULL;

            p_node = p_stream->td_list.p_next;

            usb_list_node_del(p_node);

            /* 高速设备*/
            if (p_stream->highspeed) {
                struct usbh_ehci_itd *p_itd = NULL;

                p_itd = usb_container_of(p_node, struct usbh_ehci_itd, node);

                memset(p_itd, 0, sizeof(struct usbh_ehci_itd));
                /* 释放空闲的等时传输描述符*/
                usbh_ehci_itd_free(p_ehci, p_itd);
            } else {
                struct usbh_ehci_sitd *p_sitd = NULL;

                p_sitd = usb_container_of(p_node, struct usbh_ehci_sitd, node);

                memset(p_sitd, 0, sizeof(struct usbh_ehci_sitd));
                /* 释放空闲的分割等时传输描述符*/
                usbh_ehci_sitd_free(p_ehci, p_sitd);
            }
        }

#if USB_OS_EN
        ret = usb_mutex_unlock(p_stream->p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            return ret;
        }
        ret = usb_lib_mutex_destroy(&__g_usb_host_lib.lib, p_stream->p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
        usb_lib_mfree(&__g_usb_host_lib.lib, p_stream);

        return USB_OK;
    } else {
    	p_stream->is_release = USB_TRUE;
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_stream->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif

    return ret;
}

/**
 * \brief usbh_ehci_iso_sched 操作只能是等时传输描述符或分割事务等时传输描述符
 */
static struct usbh_ehci_iso_sched *__iso_sched_alloc(uint32_t n_packets){
    struct usbh_ehci_iso_sched *p_iso_sched = NULL;

    p_iso_sched = usb_lib_malloc(&__g_usb_host_lib.lib, sizeof(struct usbh_ehci_iso_sched));
    if (p_iso_sched != NULL) {
        memset(p_iso_sched, 0, sizeof(struct usbh_ehci_iso_sched));

        usb_list_head_init(&p_iso_sched->td_list);
    } else {
        return NULL;
    }

    p_iso_sched->p_packet = usb_lib_malloc(&__g_usb_host_lib.lib, n_packets * sizeof (struct usbh_ehci_iso_packet));
    if (p_iso_sched->p_packet == NULL) {
    	usb_lib_mfree(&__g_usb_host_lib.lib, p_iso_sched);
        return NULL;
    }
    memset(p_iso_sched->p_packet, 0, n_packets * sizeof (struct usbh_ehci_iso_packet));
    return p_iso_sched;
}

/**
 * \brief 释放等时调度
 */
static void __iso_sched_free(struct usbh_ehci_iso_stream *p_stream,
                             struct usbh_ehci_iso_sched  *p_iso_sched){
    if (p_iso_sched == NULL) {
        return;
    }
#if USB_OS_EN
    int ret = usb_mutex_lock(p_stream->p_lock, USBH_EHCI_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return;
    }
#endif
    usb_list_head_splice(&p_iso_sched->td_list, &p_stream->free_list);
#if USB_OS_EN
    ret = usb_mutex_unlock(p_stream->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return;
    }
#endif
    if (p_iso_sched->p_packet) {
    	usb_lib_mfree(&__g_usb_host_lib.lib, p_iso_sched->p_packet);
    }
    usb_lib_mfree(&__g_usb_host_lib.lib, p_iso_sched);
}

/**
 * \brief 等时调度初始化
 */
static void __itd_sched_init(struct usbh_ehci_iso_sched  *p_iso_sched,
                             struct usbh_ehci_iso_stream *p_stream,
                             struct usbh_trp             *p_trp){
    int      i;
    uint32_t dma = (uint32_t)p_trp->p_data_dma;

    /* 这次传输在帧周期中的跨度*/
    p_iso_sched->span = p_trp->n_iso_packets * p_stream->interval;

    /* 计算出每个微帧的 itd 字段，稍后当我们将新的等时传输描述符放入调度时需要该字段。*/
    for (i = 0; i < p_trp->n_iso_packets; i++) {
        struct usbh_ehci_iso_packet *p_uframe = &p_iso_sched->p_packet[i];
        uint32_t                     length;
        uint32_t                     buf;
        uint32_t                     trans;
        /* 传输数据包的长度*/
        length = p_trp->p_iso_frame_desc[i].len;
        /* 获取数据包地址偏移*/
        buf = dma + p_trp->p_iso_frame_desc[i].offset;
        /* 设置激活等时传输*/
        trans = USB_CPU_TO_LE32(EHCI_ITD_ACTIVE);
        /* 取地址低12位*/
        trans |= buf & 0x0fff;
        /* 最后一个包要设置完成中断*/
        if (((i + 1) == p_trp->n_iso_packets) && !(p_trp->flag & USBH_TRP_NO_INTERRUPT)) {
            /* 设置完成中断*/
            trans |= EHCI_ITD_IOC;
        }
       trans |= length << 16;
       /* 设置事务状态与控制寄存器*/
       p_uframe->transaction = USB_CPU_TO_LE32(trans);

       /* 设置缓存地址的低12位*/
       p_uframe->bufp = (buf & (~(uint32_t)0x0fff));
       buf           += length;
       /* 可能需要在一个微帧里跨一个缓存页，1页4K大小*/
       if ((p_uframe->bufp != (buf & (~(uint32_t)0x0fff)))) {
           /* 设置缓存跨页*/
           p_uframe->cross = 1;
       }
    }
}

/**
 * \brief 分割等时调度初始化
 */
static void __sitd_sched_init(struct usbh_ehci_iso_sched  *p_iso_sched,
                              struct usbh_ehci_iso_stream *p_stream,
                              struct usbh_trp             *p_trp){
    int      i;
    uint32_t dma = (uint32_t)p_trp->p_data_dma;

    /* 这次传输需要多少帧*/
    p_iso_sched->span = p_trp->n_iso_packets * p_stream->f_interval;

    /* 获取每一帧的 sitd 字段*/
    for (i = 0; i < p_trp->n_iso_packets; i++) {
        struct usbh_ehci_iso_packet *p_packet = &p_iso_sched->p_packet[i];
        uint32_t                     length;
        uint32_t                     buf;
        uint32_t                     trans;
        /* 传输数据包的长度*/
        length = p_trp->p_iso_frame_desc[i].len & 0x03ff;
        /* 获取数据包地址偏移*/
        buf = dma + p_trp->p_iso_frame_desc [i].offset;
        /* 设置激活分割等时传输*/
        trans = EHCI_SITD_STS_ACTIVE;
        /* 最后一个包要设置完成中断*/
        if (((i + 1) == p_trp->n_iso_packets)
                && !(p_trp->flag & USBH_TRP_NO_INTERRUPT)){
            trans |= EHCI_SITD_IOC;
        }
        trans |= length << 16;
        /* 设置事务状态与控制寄存器*/
        p_packet->transaction = USB_CPU_TO_LE32 (trans);

        p_packet->bufp = buf;
        /* 设置缓存地址的低12位*/
        p_packet->buf1 = (buf + length) & ~0x0fff;
        /* 可能需要在一个帧里跨一个缓存页，1页4K大小*/
        if (p_packet->buf1 != (buf & ~(uint32_t)0x0fff)) {
            p_packet->cross = 1;
        }
        /* 输出使用多个start-splits*/
        if (p_stream->ep_addr & USB_DIR_IN){
            continue;
        }
        length = (length + 187) / 188;
        if (length > 1) { /* BEGIN vs ALL */
            length |= 1 << 3;
        }
        p_packet->buf1 |= length;
    }
}

/**
 * \brief 初始化等时传输请求包事务
 */
static int __itd_trp_transaction(struct usbh_ehci_iso_stream *p_stream,
                                 struct usbh_ehci            *p_ehci,
                                 struct usbh_trp             *p_trp){
    struct usbh_ehci_itd       *p_itd   = NULL;
    uint32_t                    i, n_itds;
    struct usbh_ehci_iso_sched *p_sched = NULL;

    /* 申请一个等时调度结构体*/
    p_sched = __iso_sched_alloc(p_trp->n_iso_packets);
    if (p_sched == NULL) {
        return -USB_ENOMEM;
    }
    /* 初始化等时调度*/
    __itd_sched_init(p_sched, p_stream, p_trp);
    /* 计算需要多少个等时传输描述符*/
    if (p_trp->interval < 8) {
        n_itds = 1 + (p_sched->span + 7) / 8;
    } else {
        n_itds = p_trp->n_iso_packets;
    }
    /* 分配/初始化 等时传输描述符 */
    for (i = 0; i < n_itds; i++) {
        /* 检查有没有空的等时传输描述符*/
        if (!usb_list_head_is_empty(&p_stream->free_list)) {

            p_itd = usb_container_of(p_stream->free_list.p_prev, struct usbh_ehci_itd, node);

            usb_list_node_del(&p_itd->node);
        } else {
            p_itd = NULL;
        }
        if (!p_itd) {
            p_itd = usbh_ehci_itd_alloc(p_ehci);
        }

        if (NULL == p_itd) {
            __iso_sched_free(p_stream, p_sched);
            return -USB_ENOMEM;
        }
        memset(p_itd, 0, sizeof(struct usbh_ehci_itd));

        usb_list_node_add(&p_itd->node, &p_sched->td_list);
    }

    /* 先把调度信息储存起来*/
    p_trp->p_hc_priv = p_sched;
    p_trp->err_cnt   = 0;

    return USB_OK;
}

/**
 * \brief 初始化分割等时传输请求包事务
 */
static int __sitd_trp_transaction(struct usbh_ehci_iso_stream *p_stream,
                                  struct usbh_ehci            *p_ehci,
                                  struct usbh_trp             *p_trp){
    struct usbh_ehci_sitd      *p_sitd      = NULL;
    int                         i;
    struct usbh_ehci_iso_sched *p_iso_sched = NULL;

    p_iso_sched = __iso_sched_alloc(p_trp->n_iso_packets);
    if (p_iso_sched == NULL){
        return -USB_ENOMEM;
    }
    /* 初始化分割等时调度*/
    __sitd_sched_init(p_iso_sched, p_stream, p_trp);

    /* 分配/初始化 分割等时传输描述符 */
    for (i = 0; i < p_trp->n_iso_packets; i++) {
        /* 检查有没有空的分割等时传输描述符*/
        if (!usb_list_head_is_empty(&p_stream->free_list)) {

            p_sitd = usb_container_of(p_stream->free_list.p_prev, struct usbh_ehci_sitd, node);

            usb_list_node_del(&p_sitd->node);
        } else {
            p_sitd = NULL;
        }

        if (!p_sitd) {
        	p_sitd = usbh_ehci_sitd_alloc(p_ehci);
        }

        if (!p_sitd) {
            __iso_sched_free(p_stream, p_iso_sched);
            return -USB_ENOMEM;
        }
        memset(p_sitd, 0, sizeof(struct usbh_ehci_sitd));

        usb_list_node_add(&p_sitd->node, &p_iso_sched->td_list);
    }
    /* 先把调度信息储存起来*/
    p_trp->p_hc_priv = p_iso_sched;
    p_trp->err_cnt   = 0;

    return 0;
}

/**
 * \brief 生成一个等时调度
 *
 * \param[in] p_ehci   EHCI 结构体
 * \param[in] p_trp    相关的等时传输请求包
 * \param[in] p_stream 等时数据流
 *
 * \retval 成功返回 USB_OK
 */
int usbh_iso_sched_make(struct usbh_ehci            *p_ehci,
                        struct usbh_trp             *p_trp,
                        struct usbh_ehci_iso_stream *p_stream){
    int ret;

    if ((p_stream == NULL) || (p_ehci == NULL) || (p_trp == NULL)) {
        return -USB_EINVAL;
    }

    /* 分配等时传输描述符*/
    if (p_stream->p_usb_dev->speed == USB_SPEED_HIGH) {
    	ret = __itd_trp_transaction(p_stream, p_ehci, p_trp);
    } else if ((p_stream->p_usb_dev->speed == USB_SPEED_LOW) ||
               (p_stream->p_usb_dev->speed == USB_SPEED_FULL)) {
        /* 分配分割等时传输描述符*/
        ret = __sitd_trp_transaction(p_stream, p_ehci, p_trp);
    }
    return ret;
}

/**
 * \brief 检查两个设备是否用同一个事务转换器
 */
static usb_bool_t __is_same_tt(struct usbh_device *p_usb_dev1,
                               struct usbh_device *p_usb_dev2){
    /* 检查是否有事务转换器*/
    if ((p_usb_dev1->p_tt == NULL) || (p_usb_dev2->p_tt == NULL)) {
        return USB_FALSE;
    }
    /* 检查事务转换器是否一样*/
    if (p_usb_dev1->p_tt != p_usb_dev2->p_tt) {
        return USB_FALSE;
    }
    /* 检查端口属否一样*/
    if (p_usb_dev1->p_tt->multi){
        return p_usb_dev1->tt_port == p_usb_dev2->tt_port;
    }
    return USB_TRUE;
}

/**
 * \brief 如果设备的事务转换器是可用于从指定帧开始的一个周期传输，使用掩码中的所有微帧
 *        检查事务转换器是否有冲突
 */
static usb_bool_t __tt_no_collision(struct usbh_ehci   *p_ehci,
                                    uint32_t            period,
                                    struct usbh_device *p_usb_dev,
                                    uint32_t            frame,
                                    uint32_t            uf_mask){

    /* 合法性检查*/
    if (period == 0) {
        return USB_FALSE;
    }
    /* 遍历周期帧*/
    for (; frame < p_ehci->frame_list_size; frame += period) {
        union usbh_ehci_struct_ptr here;
        uint32_t                   type;

        here = p_ehci->p_shadow[frame];
        type = Q_NEXT_TYPE(p_ehci->p_periodic[frame]);
        /* 遍历周期帧上的数据结构*/
        while (here.ptr) {
            switch (USB_CPU_TO_LE32(type)) {
                 /* 等时传输描述符，跳过下一个*/
                case __Q_TYPE_ITD:
                    type = Q_NEXT_TYPE(here.p_itd->hw_next);
                    here = here.p_itd->p_next;
                    continue;
                case __Q_TYPE_QH:
                    /* 检查是否用同个事务转换器*/
                    if (__is_same_tt(p_usb_dev, here.p_qh->p_ep->p_usb_dev)) {
                        uint32_t mask;

                        /* 检查中断和分割完成事务*/
                        mask = USB_CPU_TO_LE32(here.p_qh->hw_info2);
                        mask |= mask >> 8;
                        if (mask & uf_mask){
                            break;
                        }
                    }
                    /* 移动到下一个数据结构*/
                    type = Q_NEXT_TYPE(here.p_qh->hw_next);
                    here = here.p_qh->p_next;
                    continue;
                case __Q_TYPE_SITD:
                    /* 检查是否用同个事务转换器*/
                    if (__is_same_tt(p_usb_dev, here.p_sitd->p_stream->p_usb_dev)) {
                        uint16_t mask;

                        /* 检查分割起始事务和分割完成事务*/
                        mask = USB_CPU_TO_LE32(here.p_sitd->hw_u_frame);
                        mask |= mask >> 8;
                        if (mask & uf_mask){
                            break;
                        }
                    }
                    type = Q_NEXT_TYPE(here.p_sitd->hw_next);
                    here = here.p_sitd->p_next;
                    continue;
                case __Q_TYPE_FSTN:
                default:
                    __USB_ERR_INFO("periodic frame %d bogus type %d\r\n", frame, type);
            }
            /* 冲突或错误*/
            return USB_FALSE;
        }
    }
    /* 无冲突*/
    return USB_TRUE;
}

/**
 * \brief 检查等时传输插槽是否有足够带宽
 */
static int __itd_slot_ok(struct usbh_ehci *p_ehci,
                         uint32_t          mod,
                         uint32_t          uframe,
                         uint8_t           usecs,
                         uint32_t          period){
    uframe %= period;
    do {
        /* 不能提交大于 USBH_EHCI_UFRAME_BANDWIDTH 的带宽*/
        if (__periodic_usecs(p_ehci, uframe >> 3, uframe & 0x7)
                > (USBH_EHCI_UFRAME_BANDWIDTH - usecs))
            return 0;

        /* 我们知道传输请求包的轮询周期是2^N微帧*/
        uframe += period;
    } while (uframe < mod);
    return 1;
}

/**
 * \brief 检查分割等时传输插槽是否有足够带宽
 */
static int __sitd_slot_ok(struct usbh_ehci            *p_ehci,
                          uint32_t                     mod,
                          struct usbh_ehci_iso_stream *p_stream,
                          uint32_t                     uframe,
                          struct usbh_ehci_iso_sched  *p_sched,
                          uint32_t                     period_uframes){
    uint32_t mask, tmp;
    uint32_t frame, uf;

    mask = p_stream->cs_mask << (uframe & 7);

    /* 对于输入，不要打包完成分割事务到下一帧*/
    if (mask & ~0xffff) {
        return 0;
    }

    /* 检查带宽*/
    uframe %= period_uframes;
    do {
        uint32_t max_used;

        frame = uframe >> 3;
        uf = uframe & 7;

#ifdef CONFIG_USB_EHCI_TT_NEWSCHED
        /* The tt's fullspeed bus bandwidth must be available.
         * tt_available scheduling guarantees 10+% for control/bulk.
         */
        if (!tt_available (ehci, period_uframes << 3,
                stream->udev, frame, uf, stream->tt_usecs))
            return 0;
#else
        /* 事务转换器必须空闲，假设调度时间留下 10 +% 给控制/批量*/
        if (!__tt_no_collision(p_ehci, period_uframes << 3, p_stream->p_usb_dev, frame, mask)){
            return 0;
        }
#endif

        /* check starts (OUT uses more than one) */
        max_used = USBH_EHCI_UFRAME_BANDWIDTH - p_stream->usecs;
        for (tmp = p_stream->cs_mask & 0xff; tmp; tmp >>= 1, uf++) {
            if (__periodic_usecs(p_ehci, frame, uf) > max_used){
                return 0;
            }
        }

        /* for IN, check CSPLIT */
        if (p_stream->c_usecs) {
            uf = uframe & 7;
            max_used = USBH_EHCI_UFRAME_BANDWIDTH - p_stream->c_usecs;
            do {
                tmp = 1 << uf;
                tmp <<= 8;
                if ((p_stream->cs_mask & tmp) == 0){
                    continue;
                }
                if (__periodic_usecs(p_ehci, frame, uf) > max_used){
                    return 0;
                }
            } while (++uf < 8);
        }

        /* 我们知道传输请求包的轮询周期是 2^N 微帧*/
        uframe += period_uframes;
    } while (uframe < mod);

    p_stream->splits = USB_CPU_TO_LE32(p_stream->cs_mask << (uframe & 7));
    return 1;
}

/**
 * \brief 调度等时数据流
 */
static int __iso_stream_schedule(struct usbh_ehci            *p_ehci,
                                 struct usbh_trp             *p_trp,
                                 struct usbh_ehci_iso_stream *p_stream){
    uint32_t                    now, next, start, period, span;
    int                         ret;
    /* 高速模式下周期调度的元素数量*/
    uint32_t                    mod     = p_ehci->frame_list_size << 3;
    struct usbh_ehci_iso_sched *p_sched = p_trp->p_hc_priv;

    /* 等时传输请求包的轮询间隔*/
    period = p_trp->interval;
    /* 获取调度跨度*/
    span = p_sched->span;
    if (!p_stream->highspeed) {
        period <<= 3;
        span   <<= 3;
    }
    /* 检查调度跨度是否太大*/
    if (span > mod - USBH_EHCI_BANDWIDTH_SIZE) {
        ret = -USB_ESIZE;
        goto __failed;
    }
    /* 获取现在的微帧索引*/
    now  = ((usbh_ehci_uframe_idx_get(p_ehci) >> 3) % p_ehci->frame_list_size);
    now %= mod;

    /* 典型例子：重新使用当前的调度，数据流仍在活动中。*/
    if (!usb_list_head_is_empty(&p_stream->td_list)) {
        uint32_t excess;

        /* 对于高速设备，允许在等时调度阈值内进行调度。对于全速设备和因特尔基于PCI的控制器，则不要这么做*/
        if (!p_stream->highspeed && p_ehci->i_thresh) {
            next = now + p_ehci->i_thresh;
        } else {
            next = now;
        }
        /* 落后(最多两倍微帧周期？)，我们根据最后一个当前调度的插槽时间来决定，而不是下一个可用插槽时间
         * 计算当前主机控制器微帧索引到数据流的下一微帧的差距*/
        excess = (p_stream->uframe_next - period - next) & (mod - 1);
        /* 数据流的下一微帧落后当前主机控制器微帧索引两倍的微帧周期*/
        if (excess >= mod - 2 * USBH_EHCI_BANDWIDTH_SIZE) {
            start = next + excess - mod + period * USB_DIV_ROUND_UP(mod - excess, period);
        } else {
            start = next + excess + period;
        }
        if (start - now >= mod) {
            ret = -USB_EOVERFLOW;
            goto __failed;
        }
    }
    /* 需要调度；下一(微)帧什么时候开始？*/
    else {
        /* 加上一个微帧周期*/
        start = USBH_EHCI_BANDWIDTH_SIZE + (now & ~0x07);
        /* 找一个有足够带宽的微帧插槽*/
        next = start + period;
        for (; start < next; start++) {
            /* 检查调度：有足够的带宽？*/
            if (p_stream->highspeed) {
                if (__itd_slot_ok(p_ehci, mod, start, p_stream->usecs, period)) {
                    break;
                }
            } else {
                if ((start % 8) >= 6){
                    continue;
                }
                if (__sitd_slot_ok(p_ehci, mod, p_stream, start, p_sched, period)){
                    break;
                }
            }
        }

        /* 没有空间进行调度*/
        if (start == next) {
            ret = -USB_ENOMEM;
            goto __failed;
        }
    }

    /* 尝试在未来调度太远？*/
    if ((start - now + span - period) >= mod - 2 * USBH_EHCI_BANDWIDTH_SIZE) {
        ret = -USB_ESIZE;
        //goto fail;
    }
    /* 更新数据流的下一微帧*/
    p_stream->uframe_next = start & (mod - 1);

    /* 更新传输请求包的起始微帧，全速(帧)*/
    p_trp->start_frame = p_stream->uframe_next;
    if (!p_stream->highspeed) {
        p_trp->start_frame >>= 3;
    }
    return USB_OK;

__failed:
    __iso_sched_free(p_stream, p_sched);

    p_trp->p_hc_priv = NULL;

    return ret;
}

/**
 * \brief 等时传输描述符初始化
 */
static void __itd_init(struct usbh_ehci_iso_stream *p_stream,
                       struct usbh_ehci_itd        *p_itd){
    int i;

    p_itd->hw_next    =  USB_CPU_TO_LE32(1u);
    p_itd->hw_bufp[0] = p_stream->buf0;
    p_itd->hw_bufp[1] = p_stream->buf1;
    p_itd->hw_bufp[2] = p_stream->buf2;

    for (i = 0; i < 8; i++) {
        p_itd->index[i] = -1;
    }
    /* 其他字段将在调度中被填充*/
}

/**
 * \brief 获取周期列表中数据结构的下一个数据结构
 */
static union usbh_ehci_struct_ptr *__periodic_next_shadow(union usbh_ehci_struct_ptr *p_periodic,
                                                          uint32_t                    tag){
    switch (USB_CPU_TO_LE32(tag)) {
        case __Q_TYPE_QH:
            return &p_periodic->p_qh->p_next;
        case __Q_TYPE_FSTN:
            return &p_periodic->p_fstn->p_next;
        case __Q_TYPE_ITD:
            return &p_periodic->p_itd->p_next;
        default:
            return &p_periodic->p_sitd->p_next;
    }
}

/**
 * \brief 获取数据结构的下一个数据结构地址
 */
static uint32_t *__shadow_next_periodic(union usbh_ehci_struct_ptr *p_periodic,
                                        uint32_t                    tag){
    switch (USB_CPU_TO_LE32(tag)) {
        case __Q_TYPE_QH:
             return &p_periodic->p_qh->hw_next;
        default:
            return p_periodic->p_hw_next;
    }
}

/**
 * \brief 链接等时传输描述符
 */
static void __itd_link(struct usbh_ehci     *p_ehci,
                       unsigned              frame,
                       struct usbh_ehci_itd *p_itd){
    /* 获取 EHCI 周期表镜像数据结构*/
    union usbh_ehci_struct_ptr *p_prev = &p_ehci->p_shadow[frame];
    /* 获取周期列表中数据结构地址*/
    uint32_t                   *p_hw_p = &p_ehci->p_periodic[frame];
    union usbh_ehci_struct_ptr  here   = *p_prev;
    uint32_t                    type   = 0;

    /* 跳过属于之前微帧的所有等时节点*/
    while (here.ptr) {
        /* 获取周期列表的数据类型*/
        type = Q_NEXT_TYPE(*p_hw_p);
        if (type == USB_CPU_TO_LE32(__Q_TYPE_QH)) {
            break;
        }
        /* 获取下个数据地址的地址*/
        p_prev = __periodic_next_shadow(p_prev, type);
        /* 获取下一个周期列表的数据地址的地址*/
        p_hw_p = __shadow_next_periodic(&here, type);
        /* 获取下一个周期表镜像数据结构的地址*/
        here = *p_prev;
    }
    /* 设置等时传输描述符的下一个数据结构地址*/
    p_itd->p_next  = here;
    /* 设置等时传输描述符下一个硬件地址*/
    p_itd->hw_next = *p_hw_p;
    /* 设置数据结构的地址为当前等时传输描述符*/
    p_prev->p_itd  = p_itd;
    /* 设置当前等时传输描述符的帧索引*/
    p_itd->frame   = frame;

    /* 设置硬件数据地址*/
    *p_hw_p = USB_CPU_TO_LE32((uint32_t)p_itd | __Q_TYPE_ITD);
}

/**
 * \brief 等时传输描述符打包
 */
static void __itd_patch(struct usbh_ehci_itd       *p_itd,
                        struct usbh_ehci_iso_sched *p_iso_sched,
                        unsigned                    idx,
                        uint16_t                    uframe){
    /* 获取对应的等时调度的等时包*/
    struct usbh_ehci_iso_packet *p_uf = &p_iso_sched->p_packet[idx];
    unsigned                     pg   = p_itd->page;

    uframe &= 0x07;
    p_itd->index[uframe] = idx;

    p_itd->hw_transaction[uframe] = p_uf->transaction;
    /* 设置缓存页索引*/
    p_itd->hw_transaction[uframe] |= USB_CPU_TO_LE32(pg << 12);
    /* 设置缓存页地址*/
    p_itd->hw_bufp[pg] |= USB_CPU_TO_LE32(p_uf->bufp & ~(uint32_t)0);
    /* 64位地址版本*/
    //p_itd->hw_bufp_hi[pg] |= USB_CPU_TO_LE32((uint32_t)(p_uf->bufp >> 32));

    /* 如果有跨页*/
    if (p_uf->cross) {
        /* 缓存也地址加 4K */
        uint32_t bufp = p_uf->bufp + 4096;
        /* 缓存页索引加 1 */
        p_itd->page = ++pg;
        /* 设置缓存页地址*/
        p_itd->hw_bufp[pg] |= USB_CPU_TO_LE32(bufp & ~(uint32_t)0);
        /* 64位地址版本*/
        //p_itd->hw_bufp_hi[pg] |= USB_CPU_TO_LE32((uint32_t)(bufp >> 32));
    }
}
/**
 * \brief 把 USB 传输请求块适配进选择的调度插槽中，需要的话激活
 */
static int __itd_trp_link(struct usbh_ehci            *p_ehci,
                          struct usbh_trp             *p_trp,
                          uint32_t                     mod,
                          struct usbh_ehci_iso_stream *p_stream){
    int                         packet;
    uint32_t                    next_uframe, uframe, frame;
    struct usbh_ehci_iso_sched *p_iso_sched = p_trp->p_hc_priv;
    struct usbh_ehci_itd       *p_itd       = NULL;

    /* 获取下一微帧索引*/
    next_uframe = p_stream->uframe_next & (mod - 1);
    /* 检查等时数据流的传输描述符链表是否为空，是则记录申请的带宽*/
    if (usb_list_head_is_empty(&p_stream->td_list)) {
        p_ehci->bd_width_allocated += p_stream->band_width;
    }

    /* 通过微帧填充等时传输描述符的微帧*/
    for (packet = 0, p_itd = NULL; packet < p_trp->n_iso_packets; ) {
        if (p_itd == NULL) {
            /* 获取等时调度的等时传输描述符*/
            p_itd = usb_container_of(p_iso_sched->td_list.p_next,
                                     struct usbh_ehci_itd,
                                     node);
            /* 放到数据流的等时传输描述符链表*/
            usb_list_node_move_tail(&p_itd->node, &p_stream->td_list);
            //usb_atomic_inc(&stream->refcount);
            p_stream->ref_cnt++;
            p_itd->p_stream = p_stream;
            p_itd->p_trp    = p_trp;
            /* 初始化等时传输描述符*/
            __itd_init(p_stream, p_itd);
        }
        /* 获取微帧索引*/
        uframe = next_uframe & 0x07;
        /* 获取帧索引*/
        frame = next_uframe >> 3;
        /* 继续填充等时传输描述符*/
        __itd_patch(p_itd, p_iso_sched, packet, uframe);
        /* 获取下一微帧索引*/
        next_uframe += p_stream->interval;
        next_uframe &= mod - 1;
        packet++;

        /* 跨帧或最后一个等时传输包，链接到调度中*/
        if (((next_uframe >> 3) != frame) || packet == p_trp->n_iso_packets) {
            __itd_link(p_ehci, frame & (p_ehci->frame_list_size - 1), p_itd);
            p_itd = NULL;
        }
    }
    /* 更新数据流的下一微帧索引*/
    p_stream->uframe_next = next_uframe;

    /* 不需要调度数据了*/
    __iso_sched_free(p_stream, p_iso_sched);
    p_trp->p_hc_priv = NULL;

    /* 启动等时传输*/
    p_ehci->isoc_count++;

    usbh_ehci_periodic_enable(p_ehci, 1);

    return USB_OK;
}

/**
 * \brief 分割等时传输描述符打包
 */
static void __sitd_patch(struct usbh_ehci_iso_stream *p_stream,
                         struct usbh_ehci_sitd       *p_sitd,
                         struct usbh_ehci_iso_sched  *p_iso_sched,
                         unsigned                     idx){
    struct usbh_ehci_iso_packet *p_uf = &p_iso_sched->p_packet[idx];
    uint32_t                     bufp = p_uf->bufp;

    p_sitd->hw_next          = USB_CPU_TO_LE32(1);
    p_sitd->hw_full_speed_ep = p_stream->address;
    p_sitd->hw_u_frame       = p_stream->splits;
    p_sitd->hw_results       = p_uf->transaction;
    p_sitd->hw_back_pointer  = USB_CPU_TO_LE32(1);

    bufp = p_uf->bufp;
    p_sitd->hw_buf[0]    = USB_CPU_TO_LE32(bufp);
    //sitd->hw_buf_hi[0] = USB_CPU_TO_LE32(bufp >> 32);

    p_sitd->hw_buf[1] = USB_CPU_TO_LE32(p_uf->buf1);
    if (p_uf->cross) {
        bufp += 4096;
    }
    //sitd->hw_buf_hi[1] = USB_CPU_TO_LE32(bufp >> 32);
    p_sitd->index = idx;
}

/**
 * \brief 链接分割等时传输描述符
 */
static void __sitd_link(struct usbh_ehci      *p_ehci,
                        unsigned               frame,
                        struct usbh_ehci_sitd *p_sitd){
    /* note: sitd ordering could matter (CSPLIT then SSPLIT) */
    p_sitd->p_next  = p_ehci->p_shadow[frame];
    p_sitd->hw_next = p_ehci->p_periodic[frame];
    p_ehci->p_shadow[frame].p_sitd = p_sitd;
    p_sitd->frame   = frame;
    p_ehci->p_periodic[frame] = USB_CPU_TO_LE32((uint32_t)p_sitd | __Q_TYPE_SITD);
}

/**
 * \brief 把 USB 传输请求包适配进选择的调度插槽中，需要的话激活
 */
static int __sitd_trp_link(struct usbh_ehci            *p_ehci,
                           struct usbh_trp             *p_trp,
                           uint32_t                     mod,
                           struct usbh_ehci_iso_stream *p_stream){
    int                         packet;
    uint32_t                    next_uframe;
    struct usbh_ehci_iso_sched *p_sched = p_trp->p_hc_priv;
    struct usbh_ehci_sitd      *p_sitd  = NULL;

    /* 获取下一微帧索引*/
    next_uframe = p_stream->uframe_next;
    /* 检查等时数据流的传输描述符链表是否为空，是则记录申请的带宽*/
    if (usb_list_head_is_empty(&p_stream->td_list)) {
        p_ehci->bd_width_allocated += p_stream->band_width;
    }

    /* 通过微帧填充等时传输描述符的微帧*/
    for (packet = 0, p_sitd = NULL; packet < p_trp->n_iso_packets; packet++) {
        /* 检查传输描述符是否为空*/
        if (usb_list_node_is_empty(&p_sitd->node)){
            return -USB_EPERM;
        }
        /* 获取分割等时调度的等时传输描述符*/
        p_sitd = usb_container_of(p_sitd->node.p_next, struct usbh_ehci_sitd, node);
        /* 放到数据流的分割等时传输描述符链表*/
        usb_list_node_move_tail(&p_sitd->node, &p_stream->td_list);
        //usb_atomic_inc(&stream->refcount);
        p_stream->ref_cnt++;
        p_sitd->p_stream = p_stream;
        p_sitd->p_trp    = p_trp;
        /* 继续填充分割等时传输描述符*/
        __sitd_patch(p_stream, p_sitd, p_sched, packet);
        /* 链接到调度中*/
        __sitd_link(p_ehci, (next_uframe >> 3) & (p_ehci->frame_list_size - 1), p_sitd);

        next_uframe += p_stream->f_interval << 3;
    }
    /* 更新数据流的下一微帧索引*/
    p_stream->uframe_next = next_uframe & (mod - 1);

    /* 不需要调度数据了*/
    __iso_sched_free(p_stream, p_sched);

    p_trp->p_hc_priv = NULL;

    /* 启动等时传输*/
    p_ehci->isoc_count++;

    usbh_ehci_periodic_enable(p_ehci, 1);

    return USB_OK;
}

/**
 * \brief 等时传输请求
 *
 * \param[in] p_ehci   EHCI 结构体
 * \param[in] p_trp    传输请求包
 * \param[in] p_stream EHCI 等时传输流
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_iso_req(struct usbh_ehci            *p_ehci,
                      struct usbh_trp             *p_trp,
                      struct usbh_ehci_iso_stream *p_stream){
    int ret;

    if ((p_ehci == NULL) || (p_trp == NULL) || (p_stream == NULL)) {
        return -USB_EINVAL;
    }

    /* 等时数据流调度*/
    ret = __iso_stream_schedule(p_ehci, p_trp, p_stream);
    if (ret == USB_OK) {
        p_stream->ref_cnt++;
        //usb_atomic_inc(&stream->refcount);
        if (p_stream->highspeed) {
            ret = __itd_trp_link(p_ehci, p_trp, p_ehci->frame_list_size << 3, p_stream);
        } else {
            ret = __sitd_trp_link(p_ehci, p_trp, p_ehci->frame_list_size << 3, p_stream);
        }
    }
    return ret;
}

/**
 * \brief 更新等时数据流计数
 */
static void __iso_stream_put(struct usbh_ehci            *p_ehci,
                             struct usbh_ehci_iso_stream *p_stream){
#if USB_OS_EN
    int ret = usb_mutex_lock(p_stream->p_lock, USBH_EHCI_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return;
    }
#endif
    if (p_stream->ref_cnt > 1) {
        p_stream->ref_cnt--;
       // usb_atomic_dec(&stream->refcount);
    }
    /* 数据流没有数据了*/
    if (p_stream->ref_cnt == 1) {
        /* 检查数据流的空闲链表是否为空*/
        while (!usb_list_head_is_empty(&p_stream->free_list)) {
            struct usb_list_node *p_node = NULL;

            p_node = p_stream->free_list.p_next;
            usb_list_node_del(p_node);

            /* 高速设备*/
            if (p_stream->highspeed) {
                struct usbh_ehci_itd *p_itd = NULL;

                p_itd = usb_container_of(p_node, struct usbh_ehci_itd, node);

                memset(p_itd, 0, sizeof(struct usbh_ehci_itd));
                /* 释放空闲的等时传输描述符*/
                usbh_ehci_itd_free(p_ehci, p_itd);
            } else {
                struct usbh_ehci_sitd *p_sitd = NULL;

                p_sitd = usb_container_of(p_node, struct usbh_ehci_sitd, node);
                memset(p_sitd, 0, sizeof(struct usbh_ehci_sitd));
                /* 释放空闲的分割等时传输描述符*/
                usbh_ehci_sitd_free(p_ehci, p_sitd);
            }
        }
        p_stream->ep_addr &= 0x0f;

        if (p_stream->is_release == USB_TRUE) {
#if USB_OS_EN
            ret = usb_mutex_unlock(p_stream->p_lock);
            if (ret != USB_OK) {
                __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
                return;
            }
            ret = usb_lib_mutex_destroy(&__g_usb_host_lib.lib, p_stream->p_lock);
            if (ret != USB_OK) {
                __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
                return;
            }
#endif
            usb_lib_mfree(&__g_usb_host_lib.lib, p_stream);
            return;
        }
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_stream->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
}

/**
 * \brief 把等时传输描述符移动到视频流的释放链表
 */
static void usb_list_itd_move(struct usbh_ehci_itd *p_itd, struct usbh_ehci_iso_stream *p_stream){
#if USB_OS_EN
    int ret = usb_mutex_lock(p_stream->p_lock, USBH_EHCI_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return;
    }
#endif
    usb_list_node_move(&p_itd->node, &p_stream->free_list);
#if USB_OS_EN
    ret = usb_mutex_unlock(p_stream->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
}

/**
 * \brief 等时传输完成函数
 */
static uint32_t __itd_complete(struct usbh_ehci    *p_ehci,
                               struct usbh_ehci_itd *p_itd){
    struct usbh_trp             *p_trp    = p_itd->p_trp;
    struct usb_iso_pkt_desc     *p_desc   = NULL;
    uint32_t                     t, uframe;
    int                          trp_idx  = -1;
    struct usbh_ehci_iso_stream *p_stream = p_itd->p_stream;
    uint32_t                     retval   = USB_FALSE;

    /* 遍历包的每一微帧*/
    for (uframe = 0; uframe < 8; uframe++) {
        if (p_itd->index[uframe] == (uint32_t)-1) {
            continue;
        }
        /* 获取等时包索引*/
        trp_idx = p_itd->index[uframe];
        /* 获取等时包*/
        p_desc  = &p_trp->p_iso_frame_desc[trp_idx];
        /* 获取等时传输事务的状态*/
        t       = p_itd->hw_transaction[uframe];

        p_itd->hw_transaction[uframe] = 0;

        /* 检查传输状态*/
        if (t & EHCI_ITD_ERRS) {
            /* 等时传输请求包的错误计数加1*/
            p_trp->err_cnt++;
            if (t & EHCI_ITD_BUF_ERR) {
                p_desc->status = -USB_EPIPE;
            } else if (t & EHCI_ITD_BABBLE) {
                p_desc->status = -USB_EOVERFLOW;
            } else {/* (t & EHCI_ISOC_XACTERR) */
                p_desc->status = -USB_EPROTO;
            }
            /* 这个状态主机不需要更新长度*/
            if (!(t & EHCI_ITD_BABBLE)) {
                p_desc->act_len = EHCI_ITD_LENGTH(t);
                p_trp->act_len += p_desc->act_len;
            }
        } else if ((t & EHCI_ITD_ACTIVE) == 0) {
            p_desc->status = 0;
            p_desc->act_len = EHCI_ITD_LENGTH(t);
            p_trp->act_len += p_desc->act_len;
        } else {
            p_trp->err_cnt++;
        }
    }

    /* 是否现在处理完成？*/
    if ((trp_idx + 1) != p_trp->n_iso_packets) {
        goto __done;
    }

    p_itd->p_trp    = NULL;
    p_itd->p_stream = NULL;
    usb_list_itd_move(p_itd, p_stream);

    /* 调用传输请求包完成函数*/
    __ehci_trp_done(p_ehci, p_trp, p_trp->status);

    retval = USB_TRUE;
    p_trp  = NULL;
    /* 是否还有等时调度，没有则关闭周期调度*/
    if(p_ehci->isoc_count) {
        p_ehci->isoc_count--;
    }
    if ((p_ehci->isoc_count == 0) && (p_ehci->intr_count == 0)) {
        usbh_ehci_periodic_disable(p_ehci);
    }
    /* 是否是数据流最后一个等时传输描述符*/
    if (usb_list_head_is_singular(&p_stream->td_list)) {
        p_ehci->bd_width_allocated -= p_stream->band_width;
    }
    /* 更新等时数据流计数*/
    __iso_stream_put(p_ehci, p_stream);

__done:

    if (retval == USB_FALSE) {
        p_itd->p_trp    = NULL;
        p_itd->p_stream = NULL;
        usb_list_itd_move(p_itd, p_stream);
    }

    __iso_stream_put(p_ehci, p_stream);
    return retval;
}

/**
 * \brief 分割等时传输完成函数
 */
static uint32_t __sitd_complete(struct usbh_ehci      *p_ehci,
                                struct usbh_ehci_sitd *p_sitd){
    struct usbh_trp             *p_trp    = p_sitd->p_trp;
    struct usb_iso_pkt_desc     *p_desc   = NULL;
    uint32_t                     t;
    int                          trp_idx  = -1;
    struct usbh_ehci_iso_stream *p_stream = p_sitd->p_stream;
    usb_bool_t                   retval   = USB_FALSE;

    trp_idx = p_sitd->index;
    p_desc  = &p_trp->p_iso_frame_desc[trp_idx];
    t       = p_sitd->hw_results;

    /* 获取传输状态*/
    if (t & EHCI_SITD_ERRS) {
        p_trp->err_cnt++;
        if (t & EHCI_SITD_STS_DBE) {
            p_desc->status = -USB_EPIPE;
        } else if (t & EHCI_SITD_STS_BABBLE) {
            p_desc->status = -USB_EOVERFLOW;
        } else  {/* XACT, MMF, etc */
            p_desc->status = -USB_EPROTO;
        }
    } else if (t & EHCI_SITD_STS_ACTIVE) {
        /* URB was too late */
        p_trp->err_cnt++;
    } else {
        p_desc->status  = 0;
        p_desc->act_len = p_desc->len - EHCI_SITD_LENGTH(t);
        p_trp->act_len += p_desc->act_len;
    }

    /* 是否现在处理完成？*/
    if ((trp_idx + 1) != p_trp->n_iso_packets) {
        goto __done;
    }

    p_sitd->p_trp    = NULL;
    p_sitd->p_stream = NULL;
    usb_list_node_move(&p_sitd->node, &p_stream->free_list);

    retval = USB_TRUE;

    /* 调用传输请求包完成函数*/
    __ehci_trp_done(p_ehci, p_trp, p_trp->status);
    p_trp = NULL;

    /* 是否还有等时调度，没有则关闭周期调度*/
    if(p_ehci->isoc_count) {
        p_ehci->isoc_count--;
    }
    if ((p_ehci->isoc_count == 0) && (p_ehci->intr_count == 0)) {
        usbh_ehci_periodic_disable(p_ehci);
    }
    /* 是否是数据流最后一个等时传输描述符*/
    if (usb_list_head_is_singular(&p_stream->td_list)) {
        p_ehci->bd_width_allocated -= p_stream->band_width;
    }

    /* 更新等时数据流计数*/
    __iso_stream_put(p_ehci, p_stream);
__done:
    if(retval == USB_FALSE){
        p_sitd->p_trp    = NULL;
        p_sitd->p_stream = NULL;
        usb_list_node_move(&p_sitd->node, &p_stream->free_list);
    }
    __iso_stream_put(p_ehci, p_stream);

    return retval;
}

/**
 * \brief 扫描周期帧列表
 *
 * \param[in] p_ehci EHCI 结构体
 */
void usbh_ehci_isoc_scan(struct usbh_ehci *p_ehci){
    uint32_t    uf, frame_now, frame;
    uint32_t    fmask = p_ehci->frame_list_size - 1;
    usb_bool_t  modified, live;

    /* 获取当前运行到的微帧号*/
    uf        = usbh_ehci_uframe_idx_get(p_ehci);
    /* 转成帧编号*/
    frame_now = (uf >> 3) & fmask;
    live      = USB_TRUE;

    /* 记录当前运行的帧号*/
    p_ehci->frame_now = frame_now;
    /* 获取上一次扫描到的帧号*/
    frame = p_ehci->iso_frame_last;

    for (;;) {
        union usbh_ehci_struct_ptr q, *p_q_p;
        uint32_t                   type, *p_hw_p;

__restart:
        /* 扫描帧队列中的每一个已经完成的元素*/
        p_q_p    = &p_ehci->p_shadow[frame];
        p_hw_p   = &p_ehci->p_periodic[frame];
        q.ptr    = p_q_p->ptr;
        type     = Q_NEXT_TYPE(*p_hw_p);
        modified = USB_FALSE;
        while (q.ptr != NULL) {
            switch (type) {
                case __Q_TYPE_QH:
                case __Q_TYPE_FSTN:
                    q.ptr = NULL;
                    break;
                /* 等时传输描述符*/
                case __Q_TYPE_ITD:
                    /* 跳过帧中还没完成的等时传输描述符*/
                    if ((frame == frame_now) && live) {
                        for (uf = 0; uf < 8; uf++) {
                            if (q.p_itd->hw_transaction[uf] & USB_CPU_TO_LE32(EHCI_ITD_ACTIVE)) {
                                break;
                            }
                        }
                        /* 还有事务未完成，扫描下一个等时传输描述符*/
                        if (uf < 8) {
                            p_q_p  = &q.p_itd->p_next;
                            p_hw_p = &q.p_itd->hw_next;
                            type   = Q_NEXT_TYPE(q.p_itd->hw_next);
                            q      = *p_q_p;
                            break;
                        }
                    }

                    /* 这个等时传输描述符已经完成
                     * 获取下一个等时传输描述符*/
                    *p_q_p  = q.p_itd->p_next;
                    *p_hw_p = q.p_itd->hw_next;
                    type    = Q_NEXT_TYPE(q.p_itd->hw_next);
                    /* 当前等时传输描述符调用完成回调函数*/
                    modified = __itd_complete(p_ehci, q.p_itd);
                    q = *p_q_p;
                    break;
                /* 分割等时传输描述符*/
                case __Q_TYPE_SITD:
                    /* 分割传输描述符还在传输中，获取下一个数据结构*/
                    if (((frame == frame_now) ||
                       (((frame + 1) & fmask) == frame_now)) && live &&
                         (q.p_sitd->hw_results & USB_CPU_TO_LE32(EHCI_SITD_STS_ACTIVE))) {

                        p_q_p  = &q.p_sitd->p_next;
                        p_hw_p = &q.p_sitd->hw_next;
                        type   = Q_NEXT_TYPE(q.p_sitd->hw_next);
                        q      = *p_q_p;
                        break;
                    }

                    /* 获取下一个数据结构*/
                    *p_q_p  = q.p_sitd->p_next;
                    *p_hw_p = q.p_sitd->hw_next;
                    type    = Q_NEXT_TYPE(q.p_sitd->hw_next);
                    /* 已经完成，调用完成回调函数*/
                    modified = __sitd_complete(p_ehci, q.p_sitd);
                    q = *p_q_p;
                    break;
                default:
                    __USB_ERR_INFO("corrupt type %d frame %d shadow %p\r\n", type, frame, q.ptr);
                    q.ptr = NULL;
                    break;
            }
            if ((modified) && (p_ehci->isoc_count > 0)) {
                goto __restart;
            }
        }
        /* 当到达主机控制器运行帧时停止*/
        if (frame == frame_now) {
            break;
        }
        /* 记录最新扫描的帧号*/
        p_ehci->iso_frame_last = frame;
        /* 扫描下一个帧*/
        frame = (frame + 1) & fmask;
    }
}

