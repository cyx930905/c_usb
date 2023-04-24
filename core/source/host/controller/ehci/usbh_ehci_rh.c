/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/controller/ehci/usbh_ehci.h"
#include "core/include/host/controller/ehci/usbh_ehci_reg.h"

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbh_core_lib __g_usb_host_lib;
extern int usbh_hub_evt_add(struct usbh_hub_evt *p_evt);
extern int usbh_basic_hub_dev_connect(struct usbh_hub_basic *p_hub_basic, uint8_t port);
extern int usbh_basic_hub_dev_disconnect(struct usbh_hub_basic *p_hub_basic, uint8_t port);

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief USB 主机 EHCI 端口变化函数
 */
void usbh_ehci_rh_port_change(struct usbh_ehci *p_ehci){
    uint32_t       ppcd = 0xFFFFFFFF;
    uint32_t       tmp;
    uint8_t        i;
    int            ret;
    struct usb_hc *p_hc = p_ehci->hc_head.p_hc;

    if (p_ehci->ppcd) {
        ppcd = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_STS)  >> 16;
    }

    for (i = 0; i < p_ehci->n_ports; i++) {

        if ((ppcd & (1 << i)) == 0) {
            continue;
        }
        /* 获取端口的状态和控制*/
        tmp = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_PSC0);
        /* 该控制器不拥有这个端口*/
        if (tmp & __REG_PORTSC_PO) {
            continue;
        }
        /* 寄存器的值与之前的不一样*/
        if (tmp != (p_ehci->p_psta[i] & 0x3FFFFFFF)) {
            /* 更新变化后的状态值*/
            p_ehci->p_psta[i] = tmp | (p_ehci->p_psta[i] & 0xC0000000);
            p_hc->root_hub.evt.port_num = i;

            ret = usbh_hub_evt_add(&p_hc->root_hub.evt);
            if (ret != USB_OK) {
                __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
            }
        }
    }
}

/**
 * \brief EHCI 根集线器获取状态
 *
 * \param[in]  p_hub_basic USB 基本集线器结构体
 * \param[in]  is_port     是否是面向端口
 * \param[in]  port_num    对应的端口号
 * \param[out] p_sta       返回的根集线器状态值
 *
 * \retval 成功返回 USB_OK
 */
static int usbh_ehci_rh_sta_get(struct usbh_hub_basic *p_hub_basic,
                                usb_bool_t             is_port,
                                int                    port_num,
                                uint32_t              *p_sta){
    uint32_t       tmp;
    struct usb_hc *p_hc = NULL;

    if ((p_hub_basic == NULL) || (p_sta == NULL)) {
        return -USB_EINVAL;
    }

    if (is_port == USB_FALSE){
        *p_sta = 0x0001;
        return USB_OK;
    }

    if ((port_num < 0) || (port_num > 15)) {
        return -USB_EILLEGAL;
    }

    p_hc = usb_container_of(p_hub_basic, struct usb_hc, root_hub);

    /* 获取EHCI控制器*/
    struct usbh_ehci *p_ehci = USBH_GET_EHCI_FROM_HC(p_hc);

    tmp =  USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_PSC0 + port_num);

    /* 端口不属于控制器*/
    if (tmp & __REG_PORTSC_PO) {
        return -USB_EPERM;
    }

    *p_sta = tmp;

    return USB_OK;
}

/**
 * \brief EHCI 根集线器特性设置函数
 *
 * \param[in] p_hub_basic USB基本集线器结构体
 * \param[in] is_port     是否是面向端口
 * \param[in] port_num    对应的端口号
 * \param[in] feat        要设置的特性
 *
 * \retval 成功返回 USB_OK
 */
static int usbh_ehci_rh_feature_set(struct usbh_hub_basic *p_hub_basic,
                                    usb_bool_t             is_port,
                                    int                    port_num,
                                    uint16_t               feat){
    uint32_t       tmp;
    struct usb_hc *p_hc = NULL;

    if (p_hub_basic == NULL) {
        return -USB_EINVAL;
    }
    if ((is_port == USB_FALSE) ||
            ((port_num < 0) || (port_num > 15))) {
        return -USB_EILLEGAL;
    }
    if ((feat != __REG_PORTSC_PP) &&
            (feat != __REG_PORTSC_SUSPEND) &&
            (feat != __REG_PORTSC_RESET)) {
        return -USB_EILLEGAL;
    }

    p_hc = usb_container_of(p_hub_basic, struct usb_hc, root_hub);

    /* 获取EHCI控制器*/
    struct usbh_ehci *p_ehci = USBH_GET_EHCI_FROM_HC(p_hc);

    tmp = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_PSC0 + port_num);

    /* 端口不属于控制器*/
    if (tmp & __REG_PORTSC_PO) {
        return -USB_EPERM;
    }

    /* 设置集线器端口上电*/
    if (feat == __REG_PORTSC_PP) {
        tmp |= __REG_PORTSC_PP;
        USB_LE_REG_WRITE32(tmp, p_ehci->opt_reg + __OPT_REG_PSC0 + port_num);
    }
    /* 设置集线器端口挂起*/
    else if (feat == __REG_PORTSC_SUSPEND) {
        tmp |= __REG_PORTSC_SUSPEND;
        USB_LE_REG_WRITE32(tmp, p_ehci->opt_reg + __OPT_REG_PSC0 + port_num);
        usb_mdelay(2);
    }
    /* 设置集线器端口复位*/
    else if (feat == __REG_PORTSC_RESET) {
        tmp |= __REG_PORTSC_RESET;
        USB_LE_REG_WRITE32(tmp, p_ehci->opt_reg + __OPT_REG_PSC0 + port_num);
        /* 复位端口状态变化 */
        p_ehci->p_psta[port_num] = 0x0;
        usb_mdelay(200);
    }

    return USB_OK;
}

/**
 * \brief EHCI 根集线器特性清除函数
 *
 * \param[in] p_hub_basic USB 基本集线器结构体
 * \param[in] is_port     是否是面向端口
 * \param[in] port_num    对应的端口号
 * \param[in] feat        要清除的特性
 *
 * \retval 成功返回 USB_OK
 */
static int usbh_ehci_rh_feature_clr(struct usbh_hub_basic *p_hub_basic,
                                    usb_bool_t             is_port,
                                    int                    port_num,
                                    uint16_t               feat){
    uint32_t       tmp;
    struct usb_hc *p_hc = NULL;

    if (p_hub_basic == NULL) {
        return -USB_EINVAL;
    }
    if ((is_port == USB_FALSE) ||
            ((port_num < 0) || (port_num > 15))) {
        return -USB_EILLEGAL;
    }

    p_hc = usb_container_of(p_hub_basic, struct usb_hc, root_hub);

    /* 获取 EHCI 控制器*/
    struct usbh_ehci *p_ehci = USBH_GET_EHCI_FROM_HC(p_hc);

    tmp = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_PSC0 + port_num);

    /* 端口不属于控制器*/
    if (tmp & __REG_PORTSC_PO) {
        return -USB_EPERM;
    }

    /* 清除端口上电和使能状态*/
    if ((feat == __REG_PORTSC_PP) || (feat == __REG_PORTSC_PE) || (feat == __REG_PORTSC_RESET)) {
        tmp &= ~(feat);
        USB_LE_REG_WRITE32(tmp, p_ehci->opt_reg + __OPT_REG_PSC0 + port_num);
    }
    /* 清除端口连接、使能和过流状态变化*/
    else if ((feat == __REG_PORTSC_CSC) || (feat == __REG_PORTSC_PEC) || (feat == __REG_PORTSC_OCC)) {
        tmp |= (feat);
        USB_LE_REG_WRITE32(tmp, p_ehci->opt_reg + __OPT_REG_PSC0 + port_num);
    } else {
        return -USB_ENOTSUP;
    }

    return USB_OK;
}

/**
 * \brief 检查 EHCI 根集线器端口是否有设备连接
 *
 * \param[in] p_hub_basic USB 主机结构体
 * \param[in] port_num    端口号
 *
 * \retval 有设备返回  USB_TRUE，没有返回 USB_FALSE
 */
static usb_bool_t usbh_ehci_rh_pc_check(struct usbh_hub_basic *p_hub_basic,
                                        uint8_t                port_num){
    int      ret;
    uint32_t tmp_rd;

    if (p_hub_basic == NULL) {
        return -USB_EINVAL;
    }
    if (port_num > 15) {
        return -USB_EILLEGAL;
    }

    ret = usbh_ehci_rh_sta_get(p_hub_basic, USB_TRUE, port_num, &tmp_rd);
    if (ret != USB_OK) {
        __USB_ERR_INFO("ehci root hub status get failed(%d)\r\n", ret);
        return ret;
    }

    if (tmp_rd & __REG_PORTSC_CSC) {
        return USB_FALSE;
    }

    if (tmp_rd & __REG_PORTSC_CONNECT) {
        return USB_TRUE;
    }

    return USB_FALSE;
}

/**
 * \brief EHCI 根集线器处理函数
 *
 * \param[in] p_hub_basic USB 基本集线器结构体
 * \param[in] port_num    对应的端口号
 *
 * \retval 成功返回 USB_OK
 */
static int usbh_ehci_rh_handle(struct usbh_hub_basic *p_hub_basic,
                               struct usbh_hub_evt   *p_hub_evt){
    uint32_t       tmp_rd;
    int            ret;
    usb_bool_t     is_connect;
    int            i;

    if ((p_hub_basic == NULL) || (p_hub_evt == NULL)) {
        return -USB_EINVAL;
    }
    if (p_hub_evt->port_num > 15) {
        return -USB_EILLEGAL;
    }

    /* 获取集线器状态*/
    ret = usbh_ehci_rh_sta_get(p_hub_basic, USB_TRUE, p_hub_evt->port_num, &tmp_rd);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB EHCI root hub status get failed(%d)\r\n", ret);
        return ret;
    }

    /* 连接发生变化*/
       if (tmp_rd & __REG_PORTSC_CSC) {
        /* 清除连接变化特性*/
           ret = usbh_ehci_rh_feature_clr(p_hub_basic, USB_TRUE, p_hub_evt->port_num, __REG_PORTSC_CSC);
           if (ret != USB_OK) {
               __USB_ERR_INFO("USB EHCI root hub feature clear failed(%d)\r\n", ret);
               return ret;
           }

           /* 有设备连接*/
           if (tmp_rd & __REG_PORTSC_CONNECT) {
               /* 确保连接稳定*/
               for (i = 0; i < 5; i++) {
                   usb_mdelay(50);
                   is_connect = usbh_ehci_rh_pc_check(p_hub_basic, p_hub_evt->port_num);
                   if (is_connect == USB_TRUE) {
                       continue;
                   } else {
                       break;
                   }
               }
            /* 设备枚举*/
            if (is_connect == USB_TRUE) {
                ret = usbh_basic_hub_dev_connect(p_hub_basic, p_hub_evt->port_num);
                if (ret != USB_OK) {
                    return ret;
                }
            }
        } else {
            /* 设备端断开*/
            ret = usbh_basic_hub_dev_disconnect(p_hub_basic, p_hub_evt->port_num);
            if (ret != USB_OK) {
                return ret;
            }
        }
    }
    /* 端口使能状态发生变化*/
    if (tmp_rd & __REG_PORTSC_PEC) {
        ret = usbh_ehci_rh_feature_clr(p_hub_basic, USB_TRUE, p_hub_evt->port_num, __REG_PORTSC_PEC);
        if (ret != USB_OK) {
            __USB_ERR_INFO("ehci root hub feature clear failed(%d)\r\n", ret);
            return ret;
        }
    }
    /* 端口过流状态发生变化*/
    if (tmp_rd & __REG_PORTSC_OCC) {
        ret = usbh_ehci_rh_feature_clr(p_hub_basic, USB_TRUE, p_hub_evt->port_num, __REG_PORTSC_OCC);
        if (ret == USB_OK){
            /* 端口重新上电*/
            if (!(tmp_rd & __REG_PORTSC_OC)) {
                ret = usbh_ehci_rh_feature_set(p_hub_basic, USB_TRUE, p_hub_evt->port_num, __REG_PORTSC_PP);
                if (ret != USB_OK) {
                    __USB_ERR_INFO("ehci root hub feature set failed(%d)\r\n", ret);
                    return ret;
                }
                usb_mdelay(p_hub_basic->pwr_time * 2);
            }
        }
    }

    return USB_OK;
}

/**
 * \brief 复位 EHCI 根集线器
 *
 * \param[in] p_hub_basic USB基本集线器结构体
 * \param[in] is_port     是否复位端口
 * \param[in] port_num    端口号
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_rh_reset(struct usbh_hub_basic *p_hub_basic,
                       usb_bool_t             is_port,
                       uint8_t                port_num){
    int      ret;
    uint32_t tmp_rd;
    uint32_t time_out = 1000;

    if (p_hub_basic == NULL) {
        return -USB_EINVAL;
    }
    if ((is_port == USB_FALSE) || (port_num > 15)) {
        return -USB_EILLEGAL;
    }

    ret = usbh_ehci_rh_feature_set(p_hub_basic, USB_TRUE, port_num, __REG_PORTSC_RESET);
    if (ret != USB_OK) {
        __USB_ERR_INFO("ehci root hub feature set failed(%d)\r\n", ret);
        return ret;
    }

    do {
        usb_mdelay(1);

        ret = usbh_ehci_rh_sta_get(p_hub_basic, USB_TRUE, port_num, &tmp_rd);
        if (ret != USB_OK) {
            __USB_ERR_INFO("ehci root hub status get failed(%d)\r\n", ret);
            return ret;
        }
        /* 直到复位完成*/
    } while ((tmp_rd & __REG_PORTSC_RESET) && (--time_out));
    /* 超时*/
    if (time_out == 0) {
        return -USB_ETIME;
    }

    /* 取消复位*/
    ret = usbh_ehci_rh_feature_clr(p_hub_basic, USB_TRUE, port_num, __REG_PORTSC_RESET);
    if (ret != USB_OK) {
        __USB_ERR_INFO("ehci root hub feature clear failed(%d)\r\n", ret);
        return ret;
    }

    ret = usbh_ehci_rh_sta_get(p_hub_basic, USB_TRUE, port_num, &tmp_rd);
    if (ret != USB_OK) {
        __USB_ERR_INFO("ehci root hub status get failed(%d)\r\n", ret);
        return ret;
    }

    /* 端口使能*/
    if (!(tmp_rd & __REG_PORTSC_PE)) {
        /* 端口上电*/
        ret = usbh_ehci_rh_feature_set(p_hub_basic, USB_TRUE, port_num, __REG_PORTSC_PP);
        if (ret != USB_OK) {
            __USB_ERR_INFO("ehci root hub feature set failed(%d)\r\n", ret);
            return ret;
        }
    }

    return USB_OK;
}

/**
 * \brief 获取 EHCI 根集线器端口的操作速度
 *
 * \param[in]  p_hub_basic USB 主机结构体
 * \param[in]  port_num    端口号
 * \param[out] p_speed     返回的速度
 *
 * \retval 成功返回 USB_OK
 */
static int usbh_ehci_rh_ps_get(struct usbh_hub_basic *p_hub_basic,
                               uint8_t                port_num,
                               uint8_t               *p_speed){
    int      ret;
    uint32_t tmp_rd;

    if ((p_hub_basic == NULL) || (p_speed == NULL)) {
        return -USB_EINVAL;
    }
    if (port_num > 15) {
        return -USB_EILLEGAL;
    }

    ret = usbh_ehci_rh_sta_get(p_hub_basic, USB_TRUE, port_num, &tmp_rd);
    if (ret != USB_OK) {
        __USB_ERR_INFO("ehci root hub status get failed(%d)\r\n", ret);
        return ret;
    }

    if ((tmp_rd & __REG_PORTSC_PS_UNKNOWN) == __REG_PORTSC_PS_LS){
        *p_speed = USB_SPEED_LOW;
    } else if ((tmp_rd & __REG_PORTSC_PS_UNKNOWN) == __REG_PORTSC_PS_FS) {
        *p_speed = USB_SPEED_FULL;
    } else if ((tmp_rd & __REG_PORTSC_PS_UNKNOWN) == __REG_PORTSC_PS_HS) {
        *p_speed = USB_SPEED_HIGH;
    } else {
        *p_speed = USB_SPEED_UNKNOWN;
    }

    return USB_OK;
}

/**
 * \brief EHCI 内部根集线器初始化，一般USB主机只有一个端口，这个端口就是连接根集线器
 *
 * \param[in] p_hc USB 主机结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_rh_init(struct usb_hc *p_hc){
    int      i;
    uint32_t tmp;
    uint32_t time_out = 5000;

    if (p_hc == NULL) {
        return -USB_EINVAL;
    }

    /* 获取 EHCI 控制器*/
    struct usbh_ehci *p_ehci = USBH_GET_EHCI_FROM_HC(p_hc);
    if (p_ehci == NULL) {
        return -USB_EILLEGAL;
    }

    /* 获取主机控制器端口数量*/
    p_ehci->n_ports = __CAP_HCS_N_PORTS(p_ehci->hcs);
    p_hc->root_hub.n_ports = p_ehci->n_ports;

    p_ehci->p_psta = usb_lib_malloc(&__g_usb_host_lib.lib, p_ehci->n_ports * sizeof(uint32_t));
    if (p_ehci->p_psta == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_ehci->p_psta, 0, p_ehci->n_ports * sizeof(uint32_t));

    /* 上电稳定时间*/
    p_hc->root_hub.pwr_time          = 10;
    p_hc->root_hub.p_fn_hub_handle   = usbh_ehci_rh_handle;
    p_hc->root_hub.p_fn_hub_reset    = usbh_ehci_rh_reset;
    p_hc->root_hub.p_fn_hub_ps_get   = usbh_ehci_rh_ps_get;
    p_hc->root_hub.p_fn_hub_pc_check = usbh_ehci_rh_pc_check;
    p_hc->root_hub.speed             = USB_SPEED_HIGH;

    /* 遍历 EHCI 的所有端口*/
    for (i = 0; i < p_ehci->n_ports; i++) {
        /* 复位端口*/
        tmp  = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_PSC0 + (i << 2));
        tmp |= __REG_PORTSC_RESET;
        USB_LE_REG_WRITE32(tmp, p_ehci->opt_reg + __OPT_REG_PSC0 + (i << 2));
        /* 等待复位完成*/
        do {
            tmp = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_PSC0 + (i << 2));
            usb_mdelay(1);
        } while ((tmp & __REG_PORTSC_RESET) && (--time_out));

        if (time_out == 0) {
            return -USB_ETIME;
        }

        /* 获取主机控制器结构体参数*/
        tmp = USB_LE_REG_READ32(p_ehci->cap_reg + __CAP_REG_HCS);
        /* 检查是否拥有端口电源控制*/
        if (__CAP_HCS_PPC(tmp)) {
            /* 设置端口上电灯颜色及端口上电*/
            tmp  = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_PSC0 + (i << 2));
            tmp |= (__REG_PORTSC_PP | (0x1 << 14));
        } else {
            /* 设置端口上电灯颜色*/
            tmp  = USB_LE_REG_READ32(p_ehci->opt_reg + __OPT_REG_PSC0 + (i << 2));
            tmp |= (0x1 << 14);
        }
        USB_LE_REG_WRITE32(tmp, p_ehci->opt_reg + __OPT_REG_PSC0 + (i << 2));
        usb_mdelay(100);
    }

    return USB_OK;
}

