/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/core/usbh.h"

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbh_dev_lib  __g_usbh_dev_lib;
extern struct usbh_core_lib __g_usb_host_lib;

extern int usb_hc_dev_addr_alloc(struct usb_hc *p_hc, uint8_t *p_addr);
extern int usb_hc_dev_addr_free(struct usb_hc *p_hc, uint8_t addr);
extern int usbh_ep_hcpriv_init(struct usbh_endpoint *p_ep);
extern int usbh_ep_hcpriv_deinit(struct usbh_endpoint *p_ep);
extern int usb_hc_ep_enable(struct usb_hc        *p_hc,
                            struct usbh_endpoint *p_ep);
extern int usb_hc_ep_disable(struct usb_hc        *p_hc,
                             struct usbh_endpoint *p_ep);

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 获取特定的描述符
 */
static uint32_t __next_desc_get(uint8_t  *p_buf,
                                uint32_t  buf_size,
                                uint8_t   desc_type1,
                                uint8_t   desc_type2,
                                uint8_t   desc_type3,
                                uint32_t *p_n_desc_skip){
    struct usb_desc_head *p_desc_hd   = NULL;
    uint32_t              n_desc_skip = 0;
    uint8_t              *p_buf_tmp   = p_buf;

    /* 寻找类型的描述符*/
    while (buf_size > 0) {
        p_desc_hd = (struct usb_desc_head *)p_buf;
        if ((p_desc_hd->descriptor_type == desc_type1) ||
                (p_desc_hd->descriptor_type == desc_type2) ||
                (p_desc_hd->descriptor_type == desc_type3)) {
            break;
        }
        if (p_desc_hd->length == 0) {
            break;
        }
        /* 移动到下一个描述符*/
        p_buf    += p_desc_hd->length;
        buf_size -= p_desc_hd->length;
        ++n_desc_skip;
    }
    /* 存储跳过的描述符数量和返回跳过的数据字节大小*/
    if (p_n_desc_skip != NULL) {
        *p_n_desc_skip = n_desc_skip;
    }
    return p_buf - p_buf_tmp;
}

/**
 * \brief 计算端点带宽
 */
static long __bandwidth_calc(struct usbh_endpoint *p_ep){
    int            max_pkt;
    unsigned long  tmp;

#define __BIT_TIME(bytecount)   (7 * 8 * bytecount / 6)
#define __BW_HOST_DELAY          1000L
#define __BW_HUB_LS_SETUP        333L
#define __USB2_HOST_DELAY        5

    if (p_ep->p_usb_dev->speed == USB_SPEED_HIGH) {
        max_pkt = (1 + (((USBH_EP_MPS_GET(p_ep)) >> 11) & 0x03)) *
                  (USBH_EP_MPS_GET(p_ep) & 0x7ff);
    } else {
        max_pkt = (USBH_EP_MPS_GET(p_ep) & 0x7ff);
    }

    switch (p_ep->p_usb_dev->speed) {
    case USB_SPEED_LOW:
        if (USBH_EP_DIR_GET(p_ep) == USB_DIR_IN) {
            tmp = (67667L * (31L + 10L * __BIT_TIME(max_pkt))) / 1000L;
            return 64060L + (2 * __BW_HUB_LS_SETUP) + __BW_HOST_DELAY + tmp;
        } else {
            tmp = (66700L * (31L + 10L * __BIT_TIME(max_pkt))) / 1000L;
            return 64107L + (2 * __BW_HUB_LS_SETUP) + __BW_HOST_DELAY + tmp;
        }
    case USB_SPEED_FULL:
        if (USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_ISO) {
            tmp = (8354L * (31L + 10L * __BIT_TIME(max_pkt))) / 1000L;
            return ((USBH_EP_DIR_GET(p_ep) == USB_DIR_IN) ? 7268L : 6265L)
                    + __BW_HOST_DELAY + tmp;
        } else {
            tmp = (8354L * (31L + 10L * __BIT_TIME(max_pkt))) / 1000L;
            return 9107L + __BW_HOST_DELAY + tmp;
        }
    case USB_SPEED_HIGH:
        if (USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_ISO) {
            tmp = (((38 * 8 * 2083) +
                    (2083UL * (3 + __BIT_TIME(max_pkt))))/1000 + __USB2_HOST_DELAY);

        } else {
            tmp = (((55 * 8 * 2083) +
                    (2083UL * (3 + __BIT_TIME(max_pkt))))/1000 + __USB2_HOST_DELAY);
        }
        return tmp;
    default:
        return -1;
    }
}

/**
 * \brief USB 功能接口初始化
 */
static void __dev_funs_init(struct usbh_device   *p_usb_dev,
                            struct usbh_function *p_fun,
                            uint8_t               n_funs){
    uint8_t i, host_idx = p_usb_dev->p_hc->host_idx;

    for (i = 0; i < n_funs; i++) {
        usb_list_head_init(&p_fun[i].intf_list);

        p_fun[i].p_usb_dev = p_usb_dev;
        sprintf(p_fun[i].name,
               "usb%03d_%03d_%03d",
                host_idx,
                p_usb_dev->addr, i);
        p_fun[i].func_type = USBH_FUNC_UNKNOWN;
    }
}

/**
 * \brief USB 主机设备获取描述符函数
 */
static int __dev_desc_get(struct usbh_device *p_usb_dev,
                          uint16_t            type,
                          uint16_t            value,
                          uint16_t            idx,
                          uint16_t            len,
                          void               *p_buf){
    /* 控制传输
     * 主机输入|标准请求|请求目标为设备
     * 获取描述符请求*/
    int ret = usbh_ctrl_trp_sync_xfer(&p_usb_dev->ep0,
                                       USB_DIR_IN | USB_REQ_TYPE_STANDARD | USB_REQ_TAG_DEVICE,
                                       USB_REQ_GET_DESCRIPTOR,
                                      (type << 8) + value,
                                       idx,
                                       len,
                                       p_buf,
                                     __g_usbh_dev_lib.xfer_time_out,
                                       0);
    if (ret < 0) {
        __USB_ERR_INFO("USB host device desc %x get failed(%d)\r\n", type, ret);
        return ret;
    } else if (((uint8_t *)p_buf)[1] != type) {
        __USB_ERR_INFO("USB host device desc %x type illegal\r\n", ((uint8_t *)p_buf)[1]);
        ret = -USB_EDATA;
    }

    return ret;
}

/**
 * \brief USB 主机设备地址分配设置
 */
static int __dev_addr_alloc_set(struct usbh_device *p_usb_dev){
    int     ret, ret_tmp;
    uint8_t addr;

    /* 分配一个地址*/
    ret = usb_hc_dev_addr_alloc(p_usb_dev->p_hc, &addr);
    if (ret != USB_OK) {
        __USB_ERR_INFO("USB host device addr alloc failed(%d)\r\n", addr);
        return ret;
    }

    ret = usbh_ctrl_trp_sync_xfer(&p_usb_dev->ep0,
                                   USB_DIR_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_TAG_DEVICE,
                                   USB_REQ_SET_ADDRESS,
                                   addr & 0xFF,
                                   0,
                                   0,
                                   NULL,
                                 __g_usbh_dev_lib.xfer_time_out,
                                   0);
    if (ret < 0) {
        __USB_ERR_INFO("USB host device addr set failed(%d)\r\n", ret);
        goto __exit;
    }

    /* 填充设备结构体的地址域*/
    p_usb_dev->addr = addr;

    return USB_OK;
__exit:
    ret_tmp = usb_hc_dev_addr_free(p_usb_dev->p_hc, addr);
    if (ret_tmp != USB_OK) {
        return ret_tmp;
    }

    return ret;
}

/**
 * \brief USB 主机设备端点初始化函数
 */
static int __dev_ep_init(struct usbh_device       *p_usb_dev,
                         struct usbh_endpoint     *p_ep,
                         struct usb_endpoint_desc *p_ep_desc,
                         int                       size){
    int      num, len, ret, i = 0;
    uint32_t n_desc_skip      = 0;

    /* 检查端点描述符是否合法*/
    if (p_ep_desc->length < 7) {
        __USB_ERR_INFO("USB host device EP desc length illegal(%d)\r\n", p_ep_desc->length);
        return -USB_EILLEGAL;
    }

    /* 获取端点地址*/
    num = p_ep_desc->endpoint_address & 0x0F;
    if ((num >= 16) || (num == 0)) {
        __USB_ERR_INFO("USB host device EP number illegal(%d)\r\n", num);
        return -USB_EILLEGAL;
    }

    /* 初始化端点结构体成员*/
    p_ep->p_usb_dev  = p_usb_dev;
    p_ep->p_ep_desc  = p_ep_desc;
    /* 端点使能标志位*/
    p_ep->is_enabled = USB_FALSE;
    /* 计算端点所需要的时间带宽*/
    p_ep->band_width = USB_DIV_ROUND_UP(__bandwidth_calc(p_ep), 1000L);
    p_ep->p_hw_priv  = NULL;

    ret = usbh_ep_hcpriv_init(p_ep);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
        return ret;
    }

    len = size - p_ep_desc->length;
    /* 获取端点额外的描述符*/
    /* 寻找下一个接口描述符或端点描述符*/
    i = __next_desc_get((uint8_t *)p_ep_desc + p_ep_desc->length,
                         len,
                         USB_DT_INTERFACE,
                         USB_DT_ENDPOINT,
                         USB_DT_INTERFACE_ASSOCIATION,
                        &n_desc_skip);
    if (i) {
        p_ep->p_extra   = (uint8_t *)p_ep_desc + p_ep_desc->length;
        p_ep->extra_len = i;
    }

    return p_ep_desc->length + i;
}

/**
 * \brief USB 主机设备端点反初始化函数
 */
static int __dev_ep_deinit(struct usbh_endpoint *p_ep){
    int ret = usbh_ep_hcpriv_deinit(p_ep);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
    }
    return ret;
}

/**
 * \brief USB 主机设备接口描述符分析函数
 */
static int __dev_intf_parse(struct usbh_device        *p_usb_dev,
                            struct usbh_interface     *p_intf,
                            struct usb_interface_desc *p_intf_desc,
                            int                        size){
    struct usb_desc_head  *p_hd    = NULL;
    struct usbh_endpoint  *p_eps   = NULL;
    int                    len, i  = 0;
    int                    ret;
    uint8_t                n_eps   = 0;
    uint32_t               n_desc_skip;

    /* 接口描述符已经有了设置，退出*/
    if ((p_intf->p_desc) &&
       ((p_intf->p_desc->alternate_setting == 0) ||
        (p_intf_desc->alternate_setting != 0))) {
        return p_intf_desc->length;
    }

    /* 调整接口端点数量并分配端点内存*/
    p_intf_desc->num_endpoints = min(p_intf_desc->num_endpoints, USBH_MAX_EP_NUM);
    if (p_intf_desc->num_endpoints != 0) {
        p_eps = usb_lib_malloc(&__g_usb_host_lib.lib, p_intf_desc->num_endpoints * sizeof(*p_eps));
        if (p_eps == NULL) {
            return -USB_ENOMEM;
        }
        memset(p_eps, 0, p_intf_desc->num_endpoints * sizeof(*p_eps));
    }
    /* 获取剩下的描述符长度*/
    len  = size - p_intf_desc->length;
    p_hd = (struct usb_desc_head *)(((uint8_t *)p_intf_desc) + p_intf_desc->length);

    /* 获取接口额外的描述符*/
    /* 寻找下一个接口描述符或端点描述符*/
    i = __next_desc_get((uint8_t *)p_hd,
                         len,
                         USB_DT_INTERFACE,
                         USB_DT_ENDPOINT,
                         USB_DT_INTERFACE_ASSOCIATION,
                        &n_desc_skip);
    if (i) {
        p_intf->p_extra   = (uint8_t *)p_hd;
        p_intf->extra_len = i;
    }

    p_hd = (struct usb_desc_head *)(((uint8_t *)p_intf_desc) + p_intf_desc->length + i);
    len  = size - p_intf_desc->length - i;

    while (len) {
        /* 没有足够的描述符，退出*/
        if (p_hd->length > len) {
            break;
        }
        /* 遇到下一个接口描述符，说明当前接口描述符设置结束，退出*/
        if ((p_hd->descriptor_type == USB_DT_INTERFACE) ||
                (p_hd->descriptor_type == USB_DT_INTERFACE_ASSOCIATION)) {
            break;
        }
        /* 端点描述符*/
        if (p_hd->descriptor_type == USB_DT_ENDPOINT) {
            if (n_eps >= p_intf_desc->num_endpoints) {
                __USB_ERR_INFO("USB host device EP number illegal(%d)\r\n", n_eps);
                break;
            }

            /* 初始化端点*/
            ret = __dev_ep_init(p_usb_dev,
                               &p_eps[n_eps],
                               (struct usb_endpoint_desc *)p_hd,
                                len);
            if (ret < 0) {
                return ret;
            }

            n_eps++;
        } else {
            ret = p_hd->length;
        }
        /* 定位下一个描述符地址*/
        len  = len - ret;
        p_hd = (struct usb_desc_head *)(((uint8_t *)p_hd) + ret);
    }

    p_intf_desc->num_endpoints = n_eps;
    p_intf->p_desc = p_intf_desc;
    /* 如果给接口存在端点，释放所有端点*/
    if (p_intf->p_eps) {
        usb_lib_mfree(&__g_usb_host_lib.lib, p_intf->p_eps);
    }
    p_intf->p_eps = p_eps;

    return size - len;
}

/**
 * \brief USB 主机设备接口初始化
 */
static int __dev_intf_init(struct usbh_device        *p_usb_dev,
                           struct usb_list_head      *p_intfs_list,
                           struct usb_interface_desc *p_intf_desc,
                           int                        size){
    struct usbh_interface *p_intf = NULL;
    int                    ret;

    if (p_intf_desc->length < sizeof(struct usb_interface_desc)) {
        __USB_ERR_INFO("USB host device interface descriptor length illegal(%d)\r\n", p_intf_desc->length);
        return -USB_EILLEGAL;
    }

    p_intf = (struct usbh_interface *)usb_lib_malloc(&__g_usb_host_lib.lib, sizeof(struct usbh_interface));
    if (p_intf == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_intf, 0, sizeof(struct usbh_interface));

    usb_list_node_init(&p_intf->node);

    /* 分析接口描述符 */
    ret = __dev_intf_parse(p_usb_dev, p_intf, p_intf_desc, size);

    if (p_intf->p_desc == NULL) {
        usb_lib_mfree(&__g_usb_host_lib.lib, p_intf);
    } else if (ret > 0) {
        if (usb_list_node_is_empty(&p_intf->node)) {
            /* 把新的接口插入链表中*/
            usb_list_node_add_tail(&p_intf->node, p_intfs_list);
        }
    }
    return ret;
}

/**
 * \brief USB 主机设备接口反初始化
 */
static int __dev_intf_deinit(struct usbh_device    *p_usb_dev,
                             struct usbh_interface *p_intf){
    uint8_t i;
    int     ret;

    for (i = 0; i < p_intf->p_desc->num_endpoints; i++) {
        if (p_intf->p_eps) {
            if (USBH_EP_DIR_GET(&p_intf->p_eps[i]) == USB_DIR_IN) {
                p_usb_dev->p_ep_in[USBH_EP_ADDR_GET(&p_intf->p_eps[i])] = NULL;
            } else {
                p_usb_dev->p_ep_out[USBH_EP_ADDR_GET(&p_intf->p_eps[i])] = NULL;
            }

            ret = __dev_ep_deinit(&p_intf->p_eps[i]);
            if (ret != USB_OK) {
                return ret;
            }
        }
    }
    if (p_intf->p_eps) {
        usb_lib_mfree(&__g_usb_host_lib.lib, p_intf->p_eps);
        p_intf->p_eps = NULL;
    }
    usb_lib_mfree(&__g_usb_host_lib.lib, p_intf);

    return USB_OK;
}

/**
 * \brief USB 主机设备配置分析函数
 */
static int __dev_cfg_parse(struct usbh_device     *p_usb_dev,
                           struct usbh_config     *p_cfg,
                           struct usb_config_desc *p_cfg_desc){
    struct usb_desc_head  *p_desc_hd;
    struct usb_list_node  *p_node       = NULL;
    struct usb_list_node  *p_node_tmp   = NULL;
    struct usbh_interface *p_intf       = NULL;
    int                    ret;
    struct usb_list_head   intf_list;
    int                    j, i         = 0;
    uint16_t               len;
    uint32_t               n_desc_skip  = 0;
    uint8_t                n_assoc_intf = 0;

    usb_list_head_init(&intf_list);

    p_cfg->n_funs = 0;
    p_cfg->p_funs = NULL;
    /* 获取描述符头部*/
    p_desc_hd = (struct usb_desc_head *)p_cfg_desc;
    /* 获取配置描述符及其全部附属描述符的字节数*/
    len       = USB_CPU_TO_LE16(p_cfg_desc->total_length);

    /* 获取配置额外的描述符 */
    i = __next_desc_get((uint8_t *)p_desc_hd + p_desc_hd->length,
                         len,
                         USB_DT_INTERFACE_ASSOCIATION,
                         USB_DT_INTERFACE,
                         0,
                        &n_desc_skip);
    if (i) {
        p_cfg->p_extra   = (uint8_t *)p_desc_hd + p_desc_hd->length;
        p_cfg->extra_len = i;
    }

    /* 寻找接口描述符*/
    while (len) {
        if (p_desc_hd->length > len) {
            break;
        }
        /* 接口联合描述符*/
        if (p_desc_hd->descriptor_type == USB_DT_INTERFACE_ASSOCIATION) {
            struct usb_interface_assoc_desc *p_iass_tmp = NULL;
            p_iass_tmp    = (struct usb_interface_assoc_desc *)p_desc_hd;
            /* 计算要减去的接口数量 */
            n_assoc_intf += p_iass_tmp->interface_count - 1;
            ret = p_desc_hd->length;
        } else if (p_desc_hd->descriptor_type == USB_DT_INTERFACE) {
            /* 初始化接口*/
            ret = __dev_intf_init(p_usb_dev,
                                 &intf_list,
                                 (struct usb_interface_desc *)p_desc_hd,
                                  len);
            if (ret < 0) {
                return ret;
            }

            if (ret == 0) {
                return -USB_EBADF;
            }
        } else {
            ret = p_desc_hd->length;
        }

        len       = len - ret;
        p_desc_hd = (struct usb_desc_head *)(((uint8_t *)p_desc_hd) + ret);
    }

    /* 为设备添加端点*/
    usb_list_for_each_node(p_node, &intf_list) {
        p_intf = usb_container_of(p_node, struct usbh_interface, node);

        for (i = 0; i < p_intf->p_desc->num_endpoints; i++) {
            if(p_intf->p_desc->alternate_setting == 0){
                if (USBH_EP_DIR_GET(&p_intf->p_eps[i]) == USB_DIR_IN) {
                    p_usb_dev->p_ep_in[USBH_EP_ADDR_GET(&p_intf->p_eps[i])] = &p_intf->p_eps[i];
                } else {
                    p_usb_dev->p_ep_out[USBH_EP_ADDR_GET(&p_intf->p_eps[i])] = &p_intf->p_eps[i];
                }
            }
        }
    }

    /* 获取有效接口的数量*/
    p_cfg->n_funs = p_cfg_desc->num_interfaces - n_assoc_intf;

    p_cfg->p_funs = (struct usbh_function *)usb_lib_malloc(&__g_usb_host_lib.lib,
                                                              sizeof(*p_cfg->p_funs) * p_cfg->n_funs);
    if (p_cfg->p_funs == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_cfg->p_funs, 0, sizeof(*p_cfg->p_funs) * p_cfg->n_funs);

    __dev_funs_init(p_usb_dev, p_cfg->p_funs, p_cfg->n_funs);

    p_desc_hd = (struct usb_desc_head *)p_cfg_desc;
    len       = USB_CPU_TO_LE16(p_cfg_desc->total_length);
    i         = 0;

    while (len) {
        if (p_desc_hd->length > len) {
            /* 没有足够的描述符*/
            break;
        }
        /* 超过配置功能数量*/
        if (i >= p_cfg->n_funs) {
            break;
        }
        /* 接口联合描述符*/
        if (p_desc_hd->descriptor_type == USB_DT_INTERFACE_ASSOCIATION) {
            struct usb_interface_assoc_desc *p_iass = NULL;

            p_iass = (struct usb_interface_assoc_desc *)p_desc_hd;

            if (i > p_cfg->n_funs - 1) {
                return -USB_EPERM;
            }

            /* 获取信息*/
            p_cfg->p_funs[i].type           = USB_DT_INTERFACE_ASSOCIATION;
            p_cfg->p_funs[i].first_intf_num = p_iass->first_interface;    /* 接口联合的第一个接口的编号 */
            p_cfg->p_funs[i].clss           = p_iass->function_class;     /* 类代码*/
            p_cfg->p_funs[i].sub_clss       = p_iass->function_sub_class; /* 子类代码*/
            p_cfg->p_funs[i].proto          = p_iass->function_protocol;  /* 协议代码*/
            p_cfg->p_funs[i].n_intf_type    = p_iass->interface_count;    /* 接口联合的接口的数量 */

            /* 获取联合结构体相关的接口*/
            usb_list_for_each_node_safe(p_node,
                                        p_node_tmp,
                                       &intf_list) {
                p_intf = usb_container_of(p_node, struct usbh_interface, node);

                if ((p_intf->p_desc->interface_number >= p_cfg->p_funs[i].first_intf_num) &&
                    (p_intf->p_desc->interface_number < (p_cfg->p_funs[i].first_intf_num + p_cfg->p_funs[i].n_intf_type))) {
                    usb_list_node_del(&p_intf->node);
                    usb_list_node_add_tail(&p_intf->node, &p_cfg->p_funs[i].intf_list);
                    p_cfg->p_funs[i].n_intf++;
                }
            }
            i++;
        }

        len  = len - p_desc_hd->length;
        p_desc_hd = (struct usb_desc_head *)(((uint8_t *)p_desc_hd) + p_desc_hd->length);
    }
    /* 遍历其他的接口节点*/
    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &intf_list) {
        p_intf = usb_container_of(p_node, struct usbh_interface, node);

        for (j = i; j < p_cfg->n_funs; j++) {

            if (p_cfg->p_funs[j].n_intf > 0) {
                if (p_cfg->p_funs[j].first_intf_num == p_intf->p_desc->interface_number) {
                    p_cfg->p_funs[j].n_intf++;
                    usb_list_node_del(&p_intf->node);
                    usb_list_node_add_tail(&p_intf->node, &p_cfg->p_funs[j].intf_list);
                    break;
                }
            } else if (p_cfg->p_funs[j].n_intf == 0) {
                p_cfg->p_funs[j].type           = USB_DT_INTERFACE;
                p_cfg->p_funs[j].first_intf_num = p_intf->p_desc->interface_number;    /* 接口编号*/
                p_cfg->p_funs[j].clss           = p_intf->p_desc->interface_class;     /* 类代码*/
                p_cfg->p_funs[j].sub_clss       = p_intf->p_desc->interface_sub_class; /* 子类代码*/
                p_cfg->p_funs[j].proto          = p_intf->p_desc->interface_protocol;  /* 协议代码*/
                p_cfg->p_funs[j].n_intf_type    = 1;
                p_cfg->p_funs[j].n_intf         = 1;

                usb_list_node_del(&p_intf->node);
                usb_list_node_add_tail(&p_intf->node, &p_cfg->p_funs[j].intf_list);
                break;
            }
        }
    }

    return USB_OK;
}

/**
 * \brief USB 主机设备配置初始化
 */
static int __dev_cfg_init(struct usbh_device *p_usb_dev,
                          struct usbh_config *p_cfg,
                          uint8_t             config_num){
    struct usb_config_desc *p_cfg_desc = NULL;
    uint16_t                len;
    int                     ret;
#if USB_OS_EN
    int                     ret_tmp;
#endif
    memset(p_cfg, 0, sizeof(*p_cfg));

    p_cfg_desc = usb_lib_malloc(&__g_usb_host_lib.lib, sizeof(*p_cfg_desc));
    if (p_cfg_desc == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_cfg_desc, 0, sizeof(struct usb_config_desc));

    /* 启动控制传输，第一次获取配置描述符*/
    ret = __dev_desc_get(p_usb_dev,
                         USB_DT_CONFIG,
                        (config_num - 1),
                         0,
                         sizeof(*p_cfg_desc),
                         p_cfg_desc);
    if (ret < 0) {
        return ret;
    } else if (ret < (int)sizeof(struct usb_config_desc)) {
        __USB_ERR_INFO("USB host device config desc length illegal(%d)\r\n", ret);
        return -USB_EDATA;
    }

    /* 配置描述符及其全部附属描述符的字节数*/
    len = USB_CPU_TO_LE16(p_cfg_desc->total_length);
    usb_lib_mfree(&__g_usb_host_lib.lib, p_cfg_desc);

    if (len <= p_cfg_desc->length) {
        __USB_ERR_INFO("USB host device config desc length illegal(%d)\r\n", len);
        return -USB_EDATA;
    }

    /* 申请配置描述符及其全部附属描述符的内存空间*/
    p_cfg_desc = usb_lib_malloc(&__g_usb_host_lib.lib, len);
    if (p_cfg_desc == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_cfg_desc, 0, len);

    /* 获取完整的配置描述符*/
    ret = __dev_desc_get(p_usb_dev,
                         USB_DT_CONFIG,
                        (config_num - 1),
                         0,
                         len,
                         p_cfg_desc);
    if ((ret < len) ||
            (p_cfg_desc->length < sizeof(struct usb_config_desc)) ||
            (p_cfg_desc->length > len)) {
        __USB_ERR_INFO("USB host device config desc length illegal\r\n");
        return -USB_EDATA;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_usb_dev->p_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "%d\r\n", ret);
        return ret;
    }
#endif

    ret = __dev_cfg_parse(p_usb_dev, p_cfg, p_cfg_desc);

#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_usb_dev->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "%d\r\n", ret_tmp);
        return ret_tmp;
    }
#endif

    if (ret == USB_OK) {
        p_cfg->p_desc = p_cfg_desc;
    }
    return ret;
}

/**
 * \brief USB 主机设备配置反初始化
 */
static int __dev_cfg_deinit(struct usbh_device *p_usb_dev, struct usbh_config *p_cfg){
    int                    ret;
    uint8_t                i;
    struct usb_list_node  *p_node     = NULL;
    struct usb_list_node  *p_node_tmp = NULL;
    struct usbh_interface *p_intf     = NULL;

    if (p_cfg->p_funs) {
        for (i = 0; i < p_cfg->n_funs; i++) {
            /* 遍历所有接口*/
            usb_list_for_each_node_safe(p_node,
                                        p_node_tmp,
                                       &p_cfg->p_funs[i].intf_list) {
                p_intf = usb_container_of(p_node, struct usbh_interface, node);

                usb_list_node_del(&p_intf->node);

                ret = __dev_intf_deinit(p_usb_dev, p_intf);
                if (ret != USB_OK) {
                    return ret;
                }
            }
        }
        usb_lib_mfree(&__g_usb_host_lib.lib, p_cfg->p_funs);
        p_cfg->p_funs = NULL;
        p_cfg->n_funs = 0;
    }

    if (p_cfg->p_desc) {
        usb_lib_mfree(&__g_usb_host_lib.lib, p_cfg->p_desc);
        p_cfg->p_desc = NULL;
    }

    p_usb_dev->status &= ~USBH_DEV_CFG;

    return USB_OK;
}

/**
 * \brief USB 主机设置设备配置设置
 */
static int __dev_cfg_set(struct usbh_device *p_usb_dev,
                         uint8_t             cfg_num){
    int ret;

    /* 设备已存在配置描述符*/
    if (p_usb_dev->cfg.p_desc) {
        /* 存在的配置描述符的配置编号与要设置的编号相同，说明已经配置，返回*/
        if (p_usb_dev->cfg.p_desc->configuration_value == cfg_num) {
            return -USB_EEXIST;
        }

        ret = __dev_cfg_deinit(p_usb_dev, &p_usb_dev->cfg);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host device config deinit failed(%d)\r\n", ret);
            return ret;
        }
    }

    /* 设备还没有配置，进行配置初始化*/
    ret = __dev_cfg_init(p_usb_dev, &p_usb_dev->cfg, cfg_num);
    if (ret != USB_OK) {
        goto __failed;
    }

    /* 发起控制传输，设置设备配置*/
    ret = usbh_ctrl_trp_sync_xfer(&p_usb_dev->ep0,
                                   USB_DIR_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_TAG_DEVICE,
                                   USB_REQ_SET_CONFIGURATION,
                                   cfg_num,
                                   0,
                                   0,
                                   NULL,
                                 __g_usbh_dev_lib.xfer_time_out,
                                   0);
    if (ret < 0) {
        __USB_ERR_INFO("USB host device config %d set failed(%d)\r\n", ret, cfg_num);
        goto __failed;
    }

    /* 更新设备状态*/
    p_usb_dev->status |= USBH_DEV_CFG;

    return USB_OK;

__failed:
    __dev_cfg_deinit(p_usb_dev, &p_usb_dev->cfg);
    return ret;
}

/**
 * \brief 检查设备兼容性
 */
static int __dev_quirks_detect(struct usbh_device *p_usb_dev){
    //todo
    return USB_OK;
}

/**
 * \brief USB 主机设备枚举函数
 *
 * \param[in] p_usb_dev USB 主机设备
 * \param[in] scheme
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_enumerate(struct usbh_device *p_usb_dev,
                       int                 scheme) {
    struct usbh_device *p_hub_dev  = NULL;
    int                 ret;
    uint8_t             i, cfg_num = 1;
    usb_bool_t          is_port_connect;

   /* 设备上面不是根集线器*/
    if (p_usb_dev->p_hub_basic->p_usb_fun != NULL) {
        p_hub_dev = p_usb_dev->p_hub_basic->p_usb_fun->p_usb_dev;
    }
    /* 复位端口*/
    ret = usbh_basic_hub_reset(p_usb_dev->p_hub_basic, USB_TRUE, p_usb_dev->port);
    if ((ret != USB_OK) && (ret != USB_ENOTSUP)) {
        return ret;
    }

    /* 获取操作的端口的速度*/
    ret = usbh_basic_hub_ps_get(p_usb_dev->p_hub_basic, p_usb_dev->port, &p_usb_dev->speed);
    if ((ret != USB_OK) && (ret != USB_ENOTSUP)) {
        return ret;
    }

    /* 根据设备速度设置设备端点 0 的最大包大小*/
    switch (p_usb_dev->speed) {
        case USB_SPEED_SUPER:
        case USB_SPEED_WIRELESS:
            p_usb_dev->ep0_desc.max_packet_size = USB_CPU_TO_LE16(512);
            break;
        case USB_SPEED_HIGH:
        case USB_SPEED_FULL:
            p_usb_dev->ep0_desc.max_packet_size = USB_CPU_TO_LE16(64);
            break;
        case USB_SPEED_LOW:
            p_usb_dev->ep0_desc.max_packet_size = USB_CPU_TO_LE16(8);
            break;
        default:
            __USB_ERR_INFO("USB host hub port speed illegal(%d)\r\n",
                    p_usb_dev->ep0_desc.max_packet_size);
            return -USB_ENOTSUP;
    }

    /* 不是根集线器，设置事务转换器*/
    if ((p_hub_dev != NULL) && (p_hub_dev->p_tt != NULL)) {
        p_usb_dev->p_tt    = p_hub_dev->p_tt;
        p_usb_dev->tt_port = p_hub_dev->tt_port;
    }
    /* 根集线器，直接获取根集线器的事务转换器*/
    else if ((p_usb_dev->speed != USB_SPEED_HIGH)
               && (p_usb_dev->p_hub_basic->speed == USB_SPEED_HIGH)) {
        p_usb_dev->p_tt    = &p_usb_dev->p_hub_basic->tt;
        p_usb_dev->tt_port = p_usb_dev->port;
    }

    usb_mdelay(100);

    /* 获取设备描述符*/
    for (i = 0; i < 3; i++) {
        /* 获取设备描述符，把获取到的数据放到设备结构体的设备描述符定义里
         * 第一次获取只获取端点 0 大小的设备描述符（8 字节）*/
        ret = __dev_desc_get(p_usb_dev,
                             USB_DT_DEVICE,
                             0,
                             0,
                             scheme ? sizeof(*p_usb_dev->p_dev_desc) : 8,
                             p_usb_dev->p_dev_desc);

        if (ret < 0) {
            ret = usbh_dev_ep_reset(&p_usb_dev->ep0);
            if (ret != USB_OK) {
                return ret;
            }

            ret = usbh_basic_hub_port_connect_chk(p_usb_dev->p_hub_basic, p_usb_dev->port, &is_port_connect);
            if (ret == USB_OK) {
                if (is_port_connect != USB_TRUE) {
                    __USB_ERR_INFO("USB host hub port disconnect\r\n");
                    return -USB_ENODEV;
                }
            }

            /* 复位端口*/
            ret = usbh_basic_hub_reset(p_usb_dev->p_hub_basic, USB_TRUE, p_usb_dev->port);
            if ((ret != USB_OK) && (ret != USB_ENOTSUP)) {
                return ret;
            }
            usb_mdelay(100);
            continue;
        } else if (ret < 8) {
            __USB_ERR_INFO("USB host device desc length illegal(%d)\r\n", ret);
            return -USB_EPIPE;
        }

        /* 设备端点 0 包最大尺寸是否合法*/
        if ((p_usb_dev->p_dev_desc->max_packet_size0 != 8)  &&
            (p_usb_dev->p_dev_desc->max_packet_size0 != 16) &&
            (p_usb_dev->p_dev_desc->max_packet_size0 != 32) &&
            (p_usb_dev->p_dev_desc->max_packet_size0 != 64) &&
            (p_usb_dev->p_dev_desc->max_packet_size0 != 255)) {
            __USB_ERR_INFO("USB host device MPS illegal(%d)\r\n", p_usb_dev->p_dev_desc->max_packet_size0);
            return -USB_EPROTO;
        }
        ret = USB_OK;
        break;
    }

    if (ret != USB_OK) {
        return ret;
    }

    /* 再一次 复位端口*/
    ret = usbh_basic_hub_reset(p_usb_dev->p_hub_basic, USB_TRUE, p_usb_dev->port);
    if ((ret != USB_OK) && (ret != USB_ENOTSUP)) {
        return ret;
    }

    usb_mdelay(100);

    for (i = 0; i < 3; i++) {
        /* 设置设备地址*/
        ret = __dev_addr_alloc_set(p_usb_dev);
        if (ret == USB_OK) {
            break;
        }
        /* 若设备已断开，退出*/
        if (p_usb_dev->p_hub_basic->p_fn_hub_pc_check) {
            usb_bool_t is_port_connect;

            is_port_connect = p_usb_dev->p_hub_basic->p_fn_hub_pc_check(p_usb_dev->p_hub_basic, p_usb_dev->port);
            if (is_port_connect != USB_TRUE) {
                __USB_ERR_INFO("USB host hub port disconnect\r\n");
                return ret;
            }
        }
        /* 延时*/
        usb_mdelay(100);
    }

    if (ret != USB_OK) {
        return ret;
    }
    usb_mdelay(10);

    /* 更新设备端点 0 的最大包大小*/
    p_usb_dev->ep0_desc.max_packet_size = p_usb_dev->p_dev_desc->max_packet_size0;

    /* 设备信息改变，复位设备端点 0 */
    ret = usbh_dev_ep_reset(&p_usb_dev->ep0);
    if (ret != USB_OK) {
        return ret;
    }

    /* 第二次获取设备描述符，则此获取完整的设备描述符*/
    ret = __dev_desc_get(p_usb_dev,
                         USB_DT_DEVICE,
                         0,
                         0,
                         sizeof(*p_usb_dev->p_dev_desc),
                         p_usb_dev->p_dev_desc);
    /* 完整的设备描述符是18个字节*/
    if (ret < 18) {
        if (ret == 0) {
            ret = -USB_EPIPE;
        }
        return ret;
    }

    /* 检测设备兼容*/
    ret = __dev_quirks_detect(p_usb_dev);
    if (ret != USB_OK) {
        return ret;
    }

    /* USB规范版本号*/
    switch (USB_CPU_TO_LE16(p_usb_dev->p_dev_desc->bcdUSB)) {
        case 0x0100: ret = USB_SPEED_LOW;      break;  /* USB规范V1.00*/
        case 0x0110: ret = USB_SPEED_FULL;     break;  /* USB规范V1.10*/
        case 0x0200:                                   /* USB规范V2.00*/
        case 0x0210: ret = USB_SPEED_HIGH;     break;  /* USB规范V2.10*/
        case 0x0250: ret = USB_SPEED_WIRELESS; break;  /* USB规范V2.50*/
        case 0x0300: ret = USB_SPEED_SUPER;    break;  /* USB规范V3.00*/
    }
    /* 调整设备速度*/
    if (p_usb_dev->speed >= ret) {
        p_usb_dev->speed = ret;
    }
    /* 设备的配置数小于  1，即该设备无配置*/
    if (p_usb_dev->p_dev_desc->num_configurations < 1) {
        __USB_ERR_INFO("USB host device config number illegal(%d)\r\n",
                        p_usb_dev->p_dev_desc->num_configurations);
        return -USB_EDATA;
    }

    /* 设备的配置数目超过最大的配置数目，调整设备的配置数目*/
    if (p_usb_dev->p_dev_desc->num_configurations > USBH_CONFIG_MAX) {
        p_usb_dev->p_dev_desc->num_configurations = USBH_CONFIG_MAX;
    }

    /* 设置配置描述符*/
    ret = __dev_cfg_set(p_usb_dev, cfg_num);
    if (ret != USB_OK) {
        return ret;
    }

    /* 获取字符串描述符，这里失败暂不返回错误，因为有一些设备获取字符串描述符会失败，但不影响其他功能*/
    /* 制造商*/
    if (p_usb_dev->p_dev_desc->i_manufacturer) {
        ret = usbh_dev_string_get(p_usb_dev, p_usb_dev->p_dev_desc->i_manufacturer, &p_usb_dev->p_mft);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host device manufacturer string get failed(%d)\r\n", ret);
        }
    }
    /* 产商*/
    if (p_usb_dev->p_dev_desc->i_product) {
        ret = usbh_dev_string_get(p_usb_dev, p_usb_dev->p_dev_desc->i_product, &p_usb_dev->p_pdt);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host device product string get failed(%d)\r\n", ret);
        }
    }
    /* 序列号*/
    if (p_usb_dev->p_dev_desc->i_serial_number) {
        ret = usbh_dev_string_get(p_usb_dev, p_usb_dev->p_dev_desc->i_serial_number, &p_usb_dev->p_snum);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host device serial number string get failed(%d)\r\n", ret);
        }
    }

    /* 填充 USB 设备名 --> usb000_001_0x2526_0x1245_012*/
    sprintf(p_usb_dev->name,
            "usb%03d_%03d_0x%04x_0x%04x_%03d",
            p_usb_dev->p_hc->host_idx,
            p_usb_dev->port,
            p_usb_dev->p_dev_desc->id_vendor,
            p_usb_dev->p_dev_desc->id_product,
            p_usb_dev->addr);

    return USB_OK;
}

/**
 * \brief USB 主机设备取消枚举
 *
 * \param[in] p_usb_dev USB 主机设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_unenumerate(struct usbh_device *p_usb_dev){
    /* 释放设备地址*/
    if (p_usb_dev->addr) {
        usb_hc_dev_addr_free(p_usb_dev->p_hc, p_usb_dev->addr);
    }
    usbh_dev_string_put(p_usb_dev->p_mft);
    usbh_dev_string_put(p_usb_dev->p_pdt);
    usbh_dev_string_put(p_usb_dev->p_snum);

    /* 反初始化配置*/
    return __dev_cfg_deinit(p_usb_dev, &p_usb_dev->cfg);
}

/**
 * \brief USB 主机设备字符串获取
 *
 * \param[in]  p_usb_dev USB 主机设备
 * \param[in]  idx       字符描述符索引
 * \param[out] p_srting  返回的设备字符串描述符的字符串
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_string_get(struct usbh_device *p_usb_dev,
                        uint8_t             idx,
                        char              **p_string){
    int       ret;
    uint8_t  *p_buf        = NULL;
    char      str[127 * 3 + 1];
    char     *p_string_tmp = NULL;

    if (p_usb_dev == NULL) {
        return -USB_EINVAL;
    }

    /* 申请数据缓存内存空间*/
    p_buf = usb_lib_malloc(&__g_usb_host_lib.lib, 256);
    if (p_buf == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_buf, 0, 256);

    if (p_usb_dev->lang_id == 0) {
        /* 获取设备语言 ID */
        ret = __dev_desc_get(p_usb_dev,
                             USB_DT_STRING,
                             0,
                             0,
                             255,
                             p_buf);
        if (ret < 0) {
            goto __exit;
        }

        p_usb_dev->lang_id = *(uint16_t*)USB_CPU_TO_LE16(&p_buf[2]);
    }

    /* 根据描述符索引获取指定的字符串描述符*/
    ret = __dev_desc_get(p_usb_dev,
                         USB_DT_STRING,
                         idx,
                         p_usb_dev->lang_id,
                         255,
                         p_buf);
    if (ret < 0) {
        goto __exit;
    }

    /* utf16 转换成 utf8*/
    ret = usb_utf16s_to_utf8s((uint16_t *)&p_buf[2],
                              (ret - 2) / 2,
                              (uint8_t*)str,
                              sizeof(str) - 1);
    str[ret] = 0;
    ret++;

    p_string_tmp = usb_lib_malloc(&__g_usb_host_lib.lib, ret);
    if (p_string_tmp == NULL) {
        ret = -USB_ENOMEM;
        goto __exit;
    }

    memset(p_string_tmp, 0, ret);
    memcpy(p_string_tmp, str, ret);

    *p_string = p_string_tmp;

    ret = USB_OK;
__exit:
    usb_lib_mfree(&__g_usb_host_lib.lib, p_buf);
    return ret;
}

/**
 * \brief USB 主机设备字符串释放
 */
void usbh_dev_string_put(char *p_string){
    if (p_string != NULL) {
        usb_lib_mfree(&__g_usb_host_lib.lib, p_string);
    }
}

/**
 * \brief USB 主机设备端点使能函数
 */
int usbh_dev_ep_enable(struct usbh_endpoint *p_ep){
    int ret = USB_OK;

    if (!p_ep->is_enabled) {
        ret = usb_hc_ep_enable(p_ep->p_usb_dev->p_hc, p_ep);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host EP %x enable failed(%d)\r\n", USBH_EP_ADDR_GET(p_ep), ret);
            return ret;
        }

        p_ep->is_enabled = USB_TRUE;
    }

    return ret;
}

/**
 * \brief USB 主机设备端点禁能函数
 */
int usbh_dev_ep_disable(struct usbh_endpoint *p_ep){
    int ret = USB_OK;

    if ((p_ep->is_enabled) ||
        (USBH_EP_DIR_GET(p_ep) && (USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_ISO))) {

        ret = usb_hc_ep_disable(p_ep->p_usb_dev->p_hc, p_ep);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host EP %x disable failed(%d)\r\n", USBH_EP_ADDR_GET(p_ep), ret);
            return ret;
        }

        /* 更新状态*/
        p_ep->is_enabled = USB_FALSE;
    }

    return ret;
}

/**
 * \brief 端点复位函数
 */
static int __ep_reset(struct usbh_endpoint *p_ep){
    usb_bool_t enable;
    int        ret;

    enable = p_ep->is_enabled;

    ret = usbh_dev_ep_disable(p_ep);
    if (ret != USB_OK) {
        return ret;
    }

    if (enable) {
        ret = usbh_dev_ep_enable(p_ep);
    }
    return ret;
}

/**
 * \brief USB 主机设备端点复位
 *
 * \param[in] p_ep 要复位的点
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_ep_reset(struct usbh_endpoint *p_ep){
    int        ret = USB_OK;
    usb_bool_t enable;

    if (p_ep == NULL) {
        return -USB_EINVAL;
    }
    enable = p_ep->is_enabled;

    ret = usbh_dev_ep_disable(p_ep);
    if (ret != USB_OK) {
        return ret;
    }

    if (enable) {
        ret = usbh_dev_ep_enable(p_ep);
    }
    return ret;
}

/**
 * \brief USB 主机设备端点停止清除
 *
 * \param[in] p_ep 要清除停止状态的端点
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_ep_halt_clr(struct usbh_endpoint *p_ep){
    int  ret;

    if (p_ep == NULL) {
        return -USB_EINVAL;
    }

    ret = usbh_ctrl_trp_sync_xfer(&p_ep->p_usb_dev->ep0,
                                   USB_DIR_OUT |
                                   USB_REQ_TYPE_STANDARD |
                                   USB_REQ_TAG_ENDPOINT,
                                   USB_REQ_CLEAR_FEATURE,
                                   0,
                                   USBH_EP_ADDR_GET(p_ep) |
                                   USBH_EP_DIR_GET(p_ep),
                                   0,
                                   NULL,
                                 __g_usbh_dev_lib.xfer_time_out,
                                   0);
    if (ret < 0) {
        __USB_ERR_INFO("USB host device EP halt clear failed(%d)\r\n", ret);
        return ret;
    }

    return __ep_reset(p_ep);
}
