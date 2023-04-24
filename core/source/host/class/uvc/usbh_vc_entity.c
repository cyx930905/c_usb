/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/uvc/usbh_vc_drv.h"
#include "core/include/host/class/uvc/usbh_vc_entity.h"
#include "core/include/host/class/uvc/usbh_vc_stream.h"

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbh_vc_lib __g_uvc_lib;
extern struct usbh_vc_stream *usbh_vc_stream_find_by_id(struct usbh_vc *p_vc, int id);
extern int usbh_vc_stream_register(struct usbh_vc        *p_vc,
                                   struct usbh_vc_stream *p_stream);

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 获取终端字符串
 */
static uint32_t __vc_terms_print(struct usb_list_head *p_term_list,
                                 uint16_t              dir,
                                 char                 *p_buf){
    struct usb_list_node  *p_node     = NULL;
    struct usb_list_node  *p_node_tmp = NULL;
    struct usbh_vc_entity *p_term     = NULL;
    uint32_t               n_terms    = 0;
    char                  *p          = p_buf;
    /* 遍历终端链表，寻找特定方向的终端*/
    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                                p_term_list) {
        p_term = usb_container_of(p_node, struct usbh_vc_entity, chain);

        if (!UVC_ENTITY_IS_TERM(p_term) || UVC_TERM_DIRECTION(p_term) != dir) {
            continue;
        }

        if (n_terms) {
            p += sprintf(p, ",");
        }
        if (++n_terms >= 4) {
            p += sprintf(p, "...");
            break;
        }
        p += sprintf(p, "%u", p_term->id);
    }

    return p - p_buf;
}

/**
 * \brief 打印视频链
 */
static const char *__vc_chain_print(struct usbh_vc_video_chain *p_chain){
    static char buf[43];
    char       *p = buf;
    /* 获得输入终端字符串*/
    p += __vc_terms_print(&p_chain->entities, UVC_TERM_INPUT, p);
    p += sprintf(p, " -> ");
    /* 获得输出终端字符串*/
    __vc_terms_print(&p_chain->entities, UVC_TERM_OUTPUT, p);

    return buf;
}

/**
 * \brief USB 视频类实体申请函数
 *
 * \param[in]  type       实体类型
 * \param[in]  id         实体 ID
 * \param[in]  n_pads     引脚点数量
 * \param[in]  extra_size 额外数据的大小
 * \param[out] p_entity   返回的实体结构体地址
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_entity_alloc(uint16_t                type,
                         uint8_t                 id,
                         uint32_t                n_pads,
                         uint32_t                extra_size,
                         struct usbh_vc_entity **p_entity){
    struct usbh_vc_entity *p_entity_tmp = NULL;
    uint32_t               n_inputs, size, i;

    extra_size = USB_ALIGN(extra_size, sizeof(*p_entity_tmp->p_pads));
    /* 输出终端必须有一个输出引脚*/
    n_inputs = (type & UVC_TERM_OUTPUT) ? n_pads : n_pads - 1;
    size = sizeof(struct usbh_vc_entity) + extra_size +
           sizeof(*p_entity_tmp->p_pads) * n_pads + n_inputs;

    p_entity_tmp = usb_lib_malloc(&__g_uvc_lib.lib, size);
    if (p_entity_tmp == NULL){
        return -USB_ENOMEM;
    }
    memset(p_entity_tmp, 0, size);

    p_entity_tmp->id        = id;
    p_entity_tmp->type      = type;
    p_entity_tmp->num_links = 0;
    /* UVC实体引脚点个数*/
    p_entity_tmp->num_pads  = n_pads;
    p_entity_tmp->p_pads    = ((void *)(p_entity_tmp + 1)) + extra_size;

    for (i = 0; i < n_inputs; ++i){
    	p_entity_tmp->p_pads[i].flags = MEDIA_PAD_FL_SINK;
    }
    if (!UVC_ENTITY_IS_OTERM(p_entity_tmp)){
    	p_entity_tmp->p_pads[n_pads - 1].flags = MEDIA_PAD_FL_SOURCE;
    }
    p_entity_tmp->n_in_pins   = n_inputs;
    p_entity_tmp->p_source_id = (uint8_t *)(&p_entity_tmp->p_pads[n_pads]);

    *p_entity = p_entity_tmp;

    return USB_OK;
}

/**
 * \brief 根据 ID 寻找特定的实体
 */
struct usbh_vc_entity *__vc_entity_get_by_id(struct usbh_vc *p_vc, int id){
    struct usb_list_node  *p_node     = NULL;
    struct usb_list_node  *p_node_tmp = NULL;
    struct usbh_vc_entity *p_entity   = NULL;
    /* 遍历实体链表*/
    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_vc->entities) {
        p_entity = usb_container_of(p_node, struct usbh_vc_entity, node);

        /* 匹配 ID */
        if (p_entity->id == id) {
            return p_entity;
        }
    }

    return NULL;
}

/**
 * \brief 根据 ID 寻找特定终端的输出端实体
 */
struct usbh_vc_entity *__vc_entity_get_by_source_id(struct usbh_vc        *p_vc,
                                                    int                    id,
                                                    struct usbh_vc_entity *p_entity){
    unsigned int          i;
    struct usb_list_node *p_node = NULL;

    /* 如果没有指定开始遍历的实体，获取 USB 视频类设备实体链表的第一个实体*/
    if (p_entity == NULL) {
        p_entity = usb_container_of(&p_vc->entities, struct usbh_vc_entity, node);
    }
    /* 开始遍历设备的实体链表*/
    p_node = p_entity->node.p_next;

    while (p_node != (struct usb_list_node *)&p_vc->entities) {
        p_entity = usb_container_of(p_node, struct usbh_vc_entity, node);

        /* 遍历实体的输入引脚*/
        for (i = 0; i < p_entity->n_in_pins; ++i) {
            /* 匹配输入端的实体 ID */
            if (p_entity->p_source_id[i] == id) {
                return p_entity;
            }
        }
        p_node = p_entity->node.p_next;
    }
    return NULL;
}


/**
 * \brief 扫描 UVC 描述符以定位从输出终端开始并包含以下单元的链：
 *
 * - 一个或多个输出终端(USB 流或显示)
 * - 0或1个处理单元
 * - 0，1或多个单输入选择器单元
 * - 0或1个多输入选择器单元，如果所有输入都连接到输入终端
 * - 0，1或多个单输入扩展单元
 * - 1或多个输入终端(摄像头，扩展或 USB 流)
 *
 * 终端与单元必须与下列结构匹配
 * 输入终端类型_*(0) -> +-----------------+    +---------------+          +---------------+ -> 终端类型流(0)
 * ...                  | 选择器单元{0,1} | -> | 处理单元{0,1} | -> | 扩展单元{0,n} |    ...
 * 输入终端类型_*(n) -> +-----------------+    +---------------+          +---------------+ -> 终端类型流(n)
 *
 *               +---------------+    +---------------+ -> 输出终端类型_*(0)
 * 终端类型流 -> | 处理单元{0,1} | -> | 扩展单元{0,n} |    ...
 *               +---------------+    +---------------+ -> 输出终端类型_*(n)
 *
 * 处理单元和扩展单元可以在任何顺序。还支持与主链相连的附加扩展单元(作为单个单元分支)
 * 忽略单输入选择器单元
 */
static int __vc_chain_entity_scan(struct usbh_vc_video_chain *p_chain,
                                  struct usbh_vc_entity      *p_entity){
    /* 检查实体类型*/
    switch (UVC_ENTITY_TYPE(p_entity)) {
        /* 扩展单元*/
        case UVC_VC_EXTENSION_UNIT:
            /* 扩展单元的输入引脚需要为 1 */
            if (p_entity->n_in_pins != 1) {
            	__USB_ERR_INFO("\r\nextension unit %d has more than 1 input pin\r\n", p_entity->id);
                return -USB_EILLEGAL;
            }
            break;
        /* 处理单元*/
        case UVC_VC_PROCESSING_UNIT:
            /* 不允许存在多个处理单元*/
            if (p_chain->p_processing != NULL) {
            	__USB_ERR_INFO("\r\nfound multiple processing units in chain\r\n");
                return -USB_EILLEGAL;
            }

            p_chain->p_processing = p_entity;
            break;
        /* 选择器单元*/
        case UVC_VC_SELECTOR_UNIT:
            /* 忽略单输入选择器单元(Single-input selector units are ignored.) */
            if (p_entity->n_in_pins == 1) {
                break;
            }
            if (p_chain->p_selector != NULL) {
            	__USB_ERR_INFO("\r\nfound multiple selector units in chain\r\n");
                return -USB_EILLEGAL;
            }

            p_chain->p_selector = p_entity;
            break;
        /* 特定供应商，摄像头，媒体输入终端类型*/
        case UVC_ITT_VENDOR_SPECIFIC:
        case UVC_ITT_CAMERA:
        case UVC_ITT_MEDIA_TRANSPORT_INPUT:
            break;
        /* 特定供应商，显示，媒体输出终端类型*/
        case UVC_OTT_VENDOR_SPECIFIC:
        case UVC_OTT_DISPLAY:
        case UVC_OTT_MEDIA_TRANSPORT_OUTPUT:
            break;
        /* 流终端类型*/
        case UVC_TT_STREAMING:
            break;

        default:
        	__USB_ERR_INFO("\r\nunsupported entity type 0x%04x found in chain\r\n", UVC_ENTITY_TYPE(p_entity));
            return -USB_ENOTSUP;
    }
    /* 插入视频链实体链表*/
    usb_list_node_add_tail(&p_entity->chain, &p_chain->entities);
    return USB_OK;
}

/**
 * \brief 向前扫描视频链
 */
static int __vc_chain_entity_scan_forward(struct usbh_vc_video_chain *p_chain,
                                          struct usbh_vc_entity      *p_entity,
                                          struct usbh_vc_entity      *p_prev){
    struct usbh_vc_entity *p_forward;

    /* 向前扫描(Forward scan) */
    p_forward = NULL;

    while (1) {
        /* 获取这个终端输出引脚的终端*/
        p_forward = __vc_entity_get_by_source_id(p_chain->p_vc, p_entity->id, p_forward);
        if (p_forward == NULL) {
            break;
        }
        if (p_forward == p_prev) {
            continue;
        }
        /* 检查实体类型*/
        switch (UVC_ENTITY_TYPE(p_forward)) {
            /* 扩展单元*/
            case UVC_VC_EXTENSION_UNIT:
                /* 扩展单元输入引脚必须为1*/
                if (p_forward->n_in_pins != 1) {
                    __USB_ERR_INFO("\r\nextension unit %d has more than 1 input pin\r\n", p_entity->id);
                    return -USB_EILLEGAL;
                }
                usb_list_node_add_tail(&p_forward->chain, &p_chain->entities);
                break;
            /* 特定供应商，显示，媒体输出终端，流终端类型*/
            case UVC_OTT_VENDOR_SPECIFIC:
            case UVC_OTT_DISPLAY:
            case UVC_OTT_MEDIA_TRANSPORT_OUTPUT:
            case UVC_TT_STREAMING:
                /* 检查是不是输入终端*/
                if (UVC_ENTITY_IS_ITERM(p_forward)) {
                    __USB_ERR_INFO("\r\nunsupported input terminal %u\r\n", p_forward->id);
                    return -USB_EILLEGAL;
                }
                usb_list_node_add_tail(&p_forward->chain, &p_chain->entities);
                break;
        }
    }

    return USB_OK;
}

/**
 * \brief 向后扫描视频链
 */
static int __vc_chain_entity_scan_backward(struct usbh_vc_video_chain *p_chain,
                                           struct usbh_vc_entity     **p_entity){
    struct usbh_vc_entity *p_entity_tmp = *p_entity;
    struct usbh_vc_entity *p_term;
    int                    id           = -USB_EINVAL, i;
    /* 检查实体类型*/
    switch (UVC_ENTITY_TYPE(p_entity_tmp)) {
        /* 扩展或处理单元*/
        case UVC_VC_EXTENSION_UNIT:
        case UVC_VC_PROCESSING_UNIT:
            /* 扩展和处理单元只有一个输入引脚*/
            id = p_entity_tmp->p_source_id[0];
            break;

        case UVC_VC_SELECTOR_UNIT:
            /* 忽略单输入选择器单元*/
            if (p_entity_tmp->n_in_pins == 1) {
                id = p_entity_tmp->p_source_id[0];
                break;
            }
            p_chain->p_selector = p_entity_tmp;
            /* 寻找选择器单元输入引脚的实体*/
            for (i = 0; i < p_entity_tmp->n_in_pins; ++i) {
                id = p_entity_tmp->p_source_id[i];
                /* 根据 ID 寻找实体*/
                p_term = __vc_entity_get_by_id(p_chain->p_vc, id);
                /* 如果是输入终端，返回错误*/
                if (p_term == NULL || !UVC_ENTITY_IS_ITERM(p_term)) {
                	__USB_ERR_INFO("\r\nselector unit %d input %d isn't connected to an "
                                       "input terminal\r\n", p_entity_tmp->id, i);
                    return -USB_EILLEGAL;
                }
                usb_list_node_add_tail(&p_term->chain, &p_chain->entities);

                __vc_chain_entity_scan_forward(p_chain, p_term, p_entity_tmp);
            }
            id = 0;
            break;
        /* 特定供应商，摄像头，媒体输入终端，特定供应商，显示，媒体输出终端，流终端*/
        case UVC_ITT_VENDOR_SPECIFIC:
        case UVC_ITT_CAMERA:
        case UVC_ITT_MEDIA_TRANSPORT_INPUT:
        case UVC_OTT_VENDOR_SPECIFIC:
        case UVC_OTT_DISPLAY:
        case UVC_OTT_MEDIA_TRANSPORT_OUTPUT:
        case UVC_TT_STREAMING:
            /* 只有输入引脚*/
            id = UVC_ENTITY_IS_OTERM(p_entity_tmp) ? p_entity_tmp->p_source_id[0] : 0;
            break;
    }

    if (id <= 0) {
        *p_entity = NULL;
        return id;
    }
    /* 根据 ID 寻找实体*/
    p_entity_tmp = __vc_entity_get_by_id(p_chain->p_vc, id);
    if (p_entity_tmp == NULL) {
    	__USB_ERR_INFO("\r\nfound reference to unknown entity %d\r\n", id);
        return -USB_EILLEGAL;
    }

    *p_entity = p_entity_tmp;

    return USB_OK;
}

/**
 * \brief USB 视频类扫描视频链
 */
static int __vc_chain_scan(struct usbh_vc_video_chain *p_chain,
                           struct usbh_vc_entity      *p_term){
	int                    ret;
    struct usbh_vc_entity *p_entity, *p_prev;

    p_entity = p_term;
    p_prev   = NULL;

    while (p_entity != NULL) {
        /* 实体一定不能是存在视频链的一部分*/
        if (p_entity->chain.p_next || p_entity->chain.p_prev) {
            __USB_ERR_INFO("found reference to entity %d already in chain\r\n", p_entity->id);
            return -USB_EILLEGAL;
        }

        /* 处理实体*/
        ret = __vc_chain_entity_scan(p_chain, p_entity);
        if (ret != USB_OK) {
            return ret;
        }

        /* 向前扫描*/
        ret = __vc_chain_entity_scan_forward(p_chain, p_entity, p_prev);
        if (ret != USB_OK) {
            return ret;
        }

        /* 向后扫描*/
        p_prev = p_entity;
        ret = __vc_chain_entity_scan_backward(p_chain, &p_entity);
        if (ret != USB_OK) {
            return ret;
        }
    }

    return USB_OK;
}

/**
 * \brief 为视频链扫描设备和注册视频设备
 *        从输出终端开始扫描链并向后遍历
 *
 * \param[in] p_vc USB 视频类设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_video_chain_scan(struct usbh_vc *p_vc){
#if USB_OS_EN
    int                         ret;
#endif
    struct usb_list_node       *p_node     = NULL;
    struct usb_list_node       *p_node_tmp = NULL;
    struct usbh_vc_video_chain *p_chain    = NULL;
    struct usbh_vc_entity      *p_term     = NULL;

    if (p_vc == NULL) {
        return -USB_EINVAL;
    }

    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_vc->entities) {
        p_term = usb_container_of(p_node, struct usbh_vc_entity, node);

        /* 如果是输出终端，跳过*/
        if (!UVC_ENTITY_IS_OTERM(p_term)) {
            continue;
        }
        /* 如果终端已经包括了一条链，跳过这个终端。这可能发生在具有
         * 多个输出终端的链上，其中除第一个输出终端外的所有输出终端
         * 将正在扫描中插入链中。*/
        if (p_term->chain.p_next || p_term->chain.p_prev) {
            continue;
        }
        /* 为视频链结构体申请内存空间*/
        p_chain = usb_lib_malloc(&__g_uvc_lib.lib, sizeof(struct usbh_vc_video_chain));
        if (p_chain == NULL) {
            return -USB_ENOMEM;
        }
        memset(p_chain, 0, sizeof(struct usbh_vc_video_chain));
        /* 初始化视频链的实体链表*/
        usb_list_head_init(&p_chain->entities);
#if USB_OS_EN
        /* 初始化视频链的互斥锁*/
        p_chain->p_lock = usb_lib_mutex_create(&__g_uvc_lib.lib);
        if (p_chain->p_lock == NULL) {
            __USB_ERR_TRACE(MutexCreateErr, "\r\n");
            usb_lib_mfree(&__g_uvc_lib.lib, p_chain);
            return -USB_EPERM;
        }
#endif
        p_chain->p_vc = p_vc;

        p_term->flags |= UVC_ENTITY_FLAG_DEFAULT;

        /* 扫描视频链*/
        if (__vc_chain_scan(p_chain, p_term) != USB_OK) {
#if USB_OS_EN
            ret = usb_lib_mutex_destroy(&__g_uvc_lib.lib, p_chain->p_lock);
            if (ret != USB_OK) {
                __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
            }
#endif
            usb_lib_mfree(&__g_uvc_lib.lib, p_chain);
            continue;
        }

        __USB_INFO("found a valid video chain(%s)\r\n", __vc_chain_print(p_chain));
        /* 插入视频链链表*/
        usb_list_node_add_tail(&p_chain->node, &p_vc->chains);
    }
    /* 检查设备结构体视频链是否为空*/
    if (usb_list_head_is_empty(&p_vc->chains)) {
        __USB_ERR_INFO("no valid video chain found\r\n");
        return -USB_ENODEV;
    }

    return USB_OK;
}

/**
 * \brief 为视频链扫描设备和注册视频设备
 *        从输出终端开始扫描链并向后遍历
 *
 * \param[in] p_vc USB 视频类设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_video_chain_clr(struct usbh_vc *p_vc){
    struct usb_list_node *p, *n;
#if USB_OS_EN
    int                   ret;
#endif

    /* 清除视频链*/
    usb_list_for_each_node_safe(p, n, &p_vc->chains) {
        struct usbh_vc_video_chain *p_chain = NULL;
        p_chain = usb_container_of(p, struct usbh_vc_video_chain, node);
#if USB_OS_EN
        ret = usb_lib_mutex_destroy(&__g_uvc_lib.lib, p_chain->p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
        }
#endif
        usb_lib_mfree(&__g_uvc_lib.lib, p_chain);
    }
    return USB_OK;
}

/**
 * \brief 注册 USB 视频类设备终端
 */
static int __vc_term_register(struct usbh_vc             *p_vc,
                              struct usbh_vc_video_chain *p_chain){
    struct usbh_vc_stream *p_stream   = NULL;
    struct usb_list_node  *p_node     = NULL;
    struct usb_list_node  *p_node_tmp = NULL;
    struct usbh_vc_entity *p_term     = NULL;
    int                    ret;

    /* 遍历视频链的实体*/
    usb_list_for_each_node_safe(p_node, p_node_tmp, &p_chain->entities) {
        p_term = usb_container_of(p_node, struct usbh_vc_entity, chain);

        /* 寻找流终端*/
        if (UVC_ENTITY_TYPE(p_term) != UVC_TT_STREAMING) {
            continue;
        }
        /* 根据 ID 获取视频流流结构体*/
        p_stream = usbh_vc_stream_find_by_id(p_vc, p_term->id);
        if (p_stream == NULL) {
            __USB_INFO("no streaming interface found for terminal %u\r\n", p_term->id);
            continue;
        }

        p_stream->p_chain = p_chain;
        /* 注册视频流*/
        ret = usbh_vc_stream_register(p_vc, p_stream);
        if (ret != USB_OK) {
            return ret;
        }
    }

    return USB_OK;
}

/**
 * \brief USB 视频类注册视频链
 *
 * \param[in] p_vc USB 视频类设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_video_chain_register(struct usbh_vc *p_vc){
    struct usb_list_node       *p_node     = NULL;
    struct usb_list_node       *p_node_tmp = NULL;
    struct usbh_vc_video_chain *p_chain    = NULL;
    int                         ret;
    /* 遍历视频链*/
    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_vc->chains) {
        p_chain = usb_container_of(p_node, struct usbh_vc_video_chain, node);

        /* 注册终端*/
        ret = __vc_term_register(p_vc, p_chain);
        if (ret != USB_OK){
            return ret;
        }
    }
    return USB_OK;
}

/**
 * \brief USB 视频类清除实体
 *
 * \param[in] p_vc USB 视频类设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_entity_free(struct usbh_vc *p_vc){
    struct usb_list_node *p, *n;

    if (p_vc == NULL) {
        return -USB_EINVAL;
    }

    /* 清除实体*/
    usb_list_for_each_node_safe(p, n, &p_vc->entities) {
        struct usbh_vc_entity *p_entity;

        p_entity = usb_container_of(p, struct usbh_vc_entity, node);

        usb_lib_mfree(&__g_uvc_lib.lib, p_entity);
    }

    return USB_OK;
}
