/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/uvc/usbh_vc_drv.h"
#include "core/include/host/class/uvc/usbh_vc_entity.h"
#include "core/include/host/class/uvc/usbh_vc_stream.h"
#include "common/refcnt/usb_refcnt.h"
#include <string.h>
#include <stdio.h>

/*******************************************************************************
 * Static
 ******************************************************************************/
/* \brief 声明一个 USB 视频类库结构体*/
struct usbh_vc_lib __g_uvc_lib;

static void __uvc_lib_release(int *p_ref);

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern int usbh_vc_stream_parse(struct usbh_vc        *p_vc,
                                struct usbh_interface *p_intf);
extern int usbh_vc_stream_clr(struct usbh_vc *p_vc);

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 分析特定供应商的描述符(暂时不实现)
 */
static int __vendor_ctrl_parse(struct usbh_vc *p_vc,
                               uint8_t        *p_buf,
                               int             buf_len){
    //todo
    return USB_OK;
}

/**
 * \brief 分析标准类的描述符
 */
static int __standard_ctrl_parse(struct usbh_vc        *p_vc,
                                 uint8_t               *p_buf,
                                 int                    buf_len,
                                 struct usbh_interface *p_intf){
    struct usbh_interface *p_intf_tmp = NULL;
    uint16_t               type;
    struct usbh_vc_entity *p_unit, *p_term;
    int                    ret, n, i, p, len;

    /* 分析描述符*/
    switch (p_buf[2]) {
        /* USB 视频类头描述符*/
        case UVC_VC_HEADER:
            /* 获取视频流接口的数量*/
            n = buf_len >= 12 ? p_buf[11] : 0;
            /* 检查缓存数据长度是否合法*/
            if (buf_len < 12 + n) {
                __USB_ERR_INFO("video control interface %d header illegal\r\n",
                                USBH_INTF_NUM_GET(p_intf));
                return -USB_EILLEGAL;
            }
            /* 获取  USB 视频类规范版本*/
            p_vc->uvc_version = usb_get_unaligned_le16(&p_buf[3]);
            /* 获取时钟频率*/
            p_vc->clk_freq = usb_get_unaligned_le32(&p_buf[7]);
            /* 分析所有 USB 视频流接口*/
            for (i = 0; i < n; ++i) {
                if (__g_uvc_lib.n_stream >= __g_uvc_lib.n_stream_max) {
                    return -USB_ENOMEM;
                }

                /* 通过接口号获取 USB 接口结构体*/
                p_intf_tmp = usbh_dev_intf_get(p_vc->p_ufun->p_usb_dev, p_buf[12 + i], 0);
                if (p_intf_tmp == NULL) {
                    continue;
                }
                /* 分析 USB 视频类视频流描述符*/
                ret = usbh_vc_stream_parse(p_vc, p_intf_tmp);
                if (ret != USB_OK) {
                    return ret;
                }
            }
            break;
        /* USB 视频类输入终端描述符 */
        case UVC_VC_INPUT_TERMINAL:
            /* 检查数据长度*/
            if (buf_len < 8) {
                __USB_ERR_INFO("video control interface %d input terminal illegal\r\n",
                                USBH_INTF_NUM_GET(p_intf));
                return -USB_EILLEGAL;
            }

            /* 确保终端类型的最高有效位不为空，否则它会跟 unit 混淆。*/
            type = usb_get_unaligned_le16(&p_buf[4]);
            if ((type & 0xff00) == 0) {
                __USB_ERR_INFO("video control interface %d input terminal %d invalid, type 0x%4x skipping\r\n",
                                USBH_INTF_NUM_GET(p_intf), p_buf[3], type);
                return USB_OK;
            }

            n   = 0;
            p   = 0;
            len = 8;
            /* 摄像头传感器类型*/
            if (type == UVC_ITT_CAMERA) {
                n   = buf_len >= 15 ? p_buf[14] : 0;
                len = 15;
            } else if (type == UVC_ITT_MEDIA_TRANSPORT_INPUT) {    /* 媒体*/
                n   = buf_len >= 9 ? p_buf[8] : 0;
                p   = buf_len >= 10 + n ? p_buf[9 + n] : 0;
                len = 10;
            }
            if (buf_len < len + n + p) {
                __USB_ERR_INFO("video control interface %d input terminal illegal\r\n",
                                USBH_INTF_NUM_GET(p_intf));
                return -USB_EILLEGAL;
            }
            /* 申请 USB 视频类实体*/
            ret = usbh_vc_entity_alloc(type | UVC_TERM_INPUT, p_buf[3], 1, n + p, &p_term);
            if (ret != USB_OK){
                return ret;
            }
            /* 摄像头*/
            if (UVC_ENTITY_TYPE(p_term) == UVC_ITT_CAMERA) {
                /* 获取控件位图大小*/
            	p_term->camera.control_size               = n;
                /* 获取控件位图地址*/
            	p_term->camera.p_controls                 = (uint8_t *)p_term + sizeof(struct usbh_vc_entity);
            	p_term->camera.objective_focal_length_min = usb_get_unaligned_le16(&p_buf[8]);
            	p_term->camera.objective_focal_length_max = usb_get_unaligned_le16(&p_buf[10]);
            	p_term->camera.ocular_focal_length        = usb_get_unaligned_le16(&p_buf[12]);
                /* 设置控件位图值*/
                memcpy(p_term->camera.p_controls, &p_buf[15], n);
            } else if (UVC_ENTITY_TYPE(p_term) == UVC_ITT_MEDIA_TRANSPORT_INPUT) {
            	p_term->media.control_size        = n;
            	p_term->media.p_controls          = (uint8_t *)p_term + sizeof(struct usbh_vc_entity);
            	p_term->media.transport_mode_size = p;
            	p_term->media.p_transport_modes   = (uint8_t *)p_term + sizeof(struct usbh_vc_entity) + n;
                memcpy(p_term->media.p_controls, &p_buf[9], n);
                memcpy(p_term->media.p_transport_modes, &p_buf[10 + n], p);
            }
            /* 描述摄像头终端的字符串描述符索引不为 0 */
            if (p_buf[7] != 0) {
                char *p_tmp;
                /* 获取字符串描述符*/
                ret = usbh_dev_string_get(p_vc->p_ufun->p_usb_dev, p_buf[7], &p_tmp);
                if (ret != USB_OK) {
                    return ret;
                }
                memcpy(p_term->name, p_tmp, sizeof(p_term->name));
                usbh_dev_string_put(p_tmp);
            }
            else if (UVC_ENTITY_TYPE(p_term) == UVC_ITT_CAMERA) {
                sprintf(p_term->name, "camera %u", p_buf[3]);
            } else if (UVC_ENTITY_TYPE(p_term) == UVC_ITT_MEDIA_TRANSPORT_INPUT) {
                sprintf(p_term->name, "media %u", p_buf[3]);
            } else {
                sprintf(p_term->name, "input %u", p_buf[3]);
            }
            /* 插入 USB 视频类设备实体链表*/
            usb_list_node_add_tail(&p_term->node, &p_vc->entities);
            p_vc->n_entities++;
            break;
        /* USB 视频类输出终端描述符*/
        case UVC_VC_OUTPUT_TERMINAL:
            if (buf_len < 9) {
                __USB_ERR_INFO("video control interface %d output terminal illegal\r\n",
                                USBH_INTF_NUM_GET(p_intf));
                return -USB_EINVAL;
            }

            /* 确保终端类型的最高有效位不为空，否则它会跟 unit 混淆。*/
            type = usb_get_unaligned_le16(&p_buf[4]);
            if ((type & 0xff00) == 0) {
                __USB_ERR_INFO("video control interface %d output terminal %d invalid, type 0x%4x skipping\r\n",
                                USBH_INTF_NUM_GET(p_intf), p_buf[3], type);
                return USB_OK;
            }
            /* 申请终端UVC实体*/
            ret = usbh_vc_entity_alloc(type | UVC_TERM_OUTPUT, p_buf[3], 1, 0, &p_term);
            if (ret != USB_OK){
                return ret;
            }
            /* 获取这个终端连接到其他终端或单元的 ID */
            memcpy(p_term->p_source_id, &p_buf[7], 1);
            /* 描述输出终端的字符串描述符索引不为 0 */
            if (p_buf[8] != 0){
                char *p_tmp;
                /* 获取字符串描述符*/
                ret = usbh_dev_string_get(p_vc->p_ufun->p_usb_dev, p_buf[8], &p_tmp);
                if (ret != USB_OK) {
                    return ret;
                }
                memcpy(p_term->name, p_tmp, sizeof(p_term->name));
                usbh_dev_string_put(p_tmp);
            } else {
                sprintf(p_term->name, "output %u", p_buf[3]);
            }
            /* 插入 USB 视频类设备实体链表*/
            usb_list_node_add_tail(&p_term->node, &p_vc->entities);
            p_vc->n_entities++;
            break;
        /* 选择器单元*/
        case UVC_VC_SELECTOR_UNIT:
            p = buf_len >= 5 ? p_buf[4] : 0;

            if (buf_len < 5 || buf_len < 6 + p) {
                __USB_ERR_INFO("video control interface %d selector unit illegal\r\n",
                                USBH_INTF_NUM_GET(p_intf));
                return -USB_EILLEGAL;
            }
            /* 申请单元 USB 视频类实体*/
            ret = usbh_vc_entity_alloc(p_buf[2], p_buf[3], p + 1, 0, &p_unit);
            if (ret != USB_OK){
                return ret;
            }
            /* 获取这个选择器单元第一个输入引脚连接到的终端或单元的ID*/
            memcpy(p_unit->p_source_id, &p_buf[5], p);
            /* 描述选择器单元的字符串描述符索引不为0*/
            if (p_buf[5 + p] != 0){
                char *p_tmp;
                /* 获取字符串描述符*/
                ret = usbh_dev_string_get(p_vc->p_ufun->p_usb_dev, p_buf[5 + p], &p_tmp);
                if (ret != USB_OK) {
                    return ret;
                }
                memcpy(p_unit->name, p_tmp, sizeof(p_unit->name));
                usbh_dev_string_put(p_tmp);
            } else {
                sprintf(p_unit->name, "selector %u", p_buf[3]);
            }
            /* 插入 USB 视频类设备实体链表*/
            usb_list_node_add_tail(&p_unit->node, &p_vc->entities);
            p_vc->n_entities++;
            break;
        /* 处理单元*/
        case UVC_VC_PROCESSING_UNIT:
            n = buf_len >= 8 ? p_buf[7] : 0;
            p = p_vc->uvc_version >= 0x0110 ? 10 : 9;

            if (buf_len < p + n) {
                __USB_ERR_INFO("video control interface %d processing unit illegal\r\n",
                                USBH_INTF_NUM_GET(p_intf));
                return -USB_EILLEGAL;
            }
            /* 申请单元 USB 视频类实体*/
            ret = usbh_vc_entity_alloc(p_buf[2], p_buf[3], 2, n, &p_unit);
            if (ret != USB_OK){
                return ret;
            }
            /* 获取连接到这个处理单元的其他终端或单元的 ID */
            memcpy(p_unit->p_source_id, &p_buf[4], 1);
            /* 获取数字倍增*/
            p_unit->processing.max_multiplier = usb_get_unaligned_le16(&p_buf[5]);
            /* 获取处理单元控件位图大小*/
            p_unit->processing.control_size = p_buf[7];
            /* 获取处理单元控件位图地址*/
            p_unit->processing.p_controls = (uint8_t *)p_unit + sizeof(struct usbh_vc_entity);
            /* 设置处理单元控件位图值*/
            memcpy(p_unit->processing.p_controls, &p_buf[8], n);
            if (p_vc->uvc_version >= 0x0110) {
            	p_unit->processing.video_standards = p_buf[9 + n];
            }
            /* 描述处理单元的字符串描述符索引不为0*/
            if (p_buf[8 + n] != 0){
                char *p_tmp;
                /* 获取字符串描述符*/
                ret = usbh_dev_string_get(p_vc->p_ufun->p_usb_dev, p_buf[8 + n], &p_tmp);
                if (ret != USB_OK) {
                    return ret;
                }
                memcpy(p_unit->name, p_tmp, sizeof(p_unit->name));
                usbh_dev_string_put(p_tmp);
            } else {
                sprintf(p_unit->name, "processing %u", p_buf[3]);
            }
            /* 插入 USB 视频类设备实体链表*/
            usb_list_node_add_tail(&p_unit->node, &p_vc->entities);
            p_vc->n_entities++;
            break;
        /* 扩展单元*/
        case UVC_VC_EXTENSION_UNIT:
            p = buf_len >= 22 ? p_buf[21] : 0;
            n = buf_len >= 24 + p ? p_buf[22 + p] : 0;

            if (buf_len < 24 + p + n) {
                __USB_ERR_INFO("video control interface %d extension unit illegal\r\n",
                                USBH_INTF_NUM_GET(p_intf));
                return -USB_EILLEGAL;
            }
            /* 申请单元 USB 视频类实体*/
            ret = usbh_vc_entity_alloc(p_buf[2], p_buf[3], p + 1, n, &p_unit);
            if (ret != USB_OK){
                return ret;
            }
            /* 获取扩展代码*/
            memcpy(p_unit->extension.guid_extension_code, &p_buf[4], 16);
            /* 获取扩展单元控件数量*/
            p_unit->extension.num_controls = p_buf[20];
            /* 获取这个扩展单元第一个输入引脚连接到的终端或单元的ID*/
            memcpy(p_unit->p_source_id, &p_buf[22], p);
            /* 获取扩展单元控件位图大小*/
            p_unit->extension.control_size = p_buf[22 + p];
            /* 获取扩展单元控件位图地址*/
            p_unit->extension.p_controls = (uint8_t *)p_unit + sizeof(struct usbh_vc_entity);
            /* 设置扩展单元控件位图值*/
            memcpy(p_unit->extension.p_controls, &p_buf[23 + p], n);
            /* 描述扩展单元的字符串描述符索引不为 0 */
            if (p_buf[23 + p + n] != 0){
                char *p_tmp;
                /* 获取字符串描述符*/
                ret = usbh_dev_string_get(p_vc->p_ufun->p_usb_dev, p_buf[23 + p + n], &p_tmp);
                if (ret != USB_OK) {
                    return ret;
                }
                memcpy(p_unit->name, p_tmp, sizeof(p_unit->name));
                usbh_dev_string_put(p_tmp);
            } else {
                sprintf(p_unit->name, "extension %u", p_buf[3]);
            }
            /* 插入 USB 视频类设备实体链表*/
            usb_list_node_add_tail(&p_unit->node, &p_vc->entities);
            p_vc->n_entities++;
            break;
        default:
        	__USB_INFO("found an unknown interface descriptor(%u)\r\n", p_buf[2]);
            break;
    }
    return USB_OK;
}

/**
 * \brief USB 视频类分析控制描述符
 *
 * \param[in] p_vc USB 视频类设备结构体
 *
 * \retval 成功返回 USB_OK
 */
static int __vc_ctrl_parse(struct usbh_vc *p_vc){
    struct usbh_interface *p_ctrl_intf = p_vc->p_ctrl_intf;
    unsigned char         *p_buf       = NULL;
    int                    buf_len     = 0;
    int                    ret;

    p_buf   = p_ctrl_intf->p_extra;
    buf_len = p_ctrl_intf->extra_len;

    while (buf_len > 2) {
        /* 分析特定供应商的描述符*/
    	ret = __vendor_ctrl_parse(p_vc, p_buf, buf_len);
        if (ret != USB_OK){
            goto __next_descriptor;
        }
        /* 分析特定类的描述符*/
        ret = __standard_ctrl_parse(p_vc, p_buf, buf_len, p_ctrl_intf);
        if (ret != USB_OK){
            return ret;
        }

__next_descriptor:
        buf_len -= p_buf[0];
        p_buf   += p_buf[0];
    }

    /* 检查是否存在可选状态端点。内置的 iSight 网络摄像头有一个中断端点，但
     * 会吐出不符合 UVC 状态端点消息的专有数据。不要试图处理这些摄像头的中
     * 断端点。*/
    if (p_ctrl_intf->p_desc->num_endpoints == 1) {
        struct usbh_endpoint *p_ep = &p_ctrl_intf->p_eps[0];

        if ((USBH_EP_DIR_GET(p_ep) && (USBH_EP_TYPE_GET(p_ep) == USB_EP_TYPE_INT)) &&
                USBH_EP_MPS_GET(p_ep) >= 8 && USBH_EP_INTVAL_GET(p_ep) != 0) {
            __USB_INFO("found a status endpoint(addr %02x)\r\n", USBH_EP_ADDR_GET(p_ep));
            p_vc->p_int_ep = p_ep;
        }
    }
    return USB_OK;
}

/**
 * \brief 初始化 USB 视频类设备
 */
static int __dev_init(struct usbh_vc       *p_vc,
                      struct usbh_function *p_usb_fun,
                      char                 *p_name){
    struct usb_list_node  *p_node     = NULL;
    struct usb_list_node  *p_node_tmp = NULL;
    struct usbh_interface *p_intf     = NULL;
    int                    ret;

    strncpy(p_vc->name, p_name, USB_NAME_LEN);
    /* 初始化链表头*/
    usb_list_head_init(&p_vc->streams);
    usb_list_head_init(&p_vc->entities);
    usb_list_head_init(&p_vc->chains);
    /* 初始化引用计数*/
    usb_refcnt_init(&p_vc->ref_cnt);
#if USB_OS_EN
    /* 创建互斥锁*/
    p_vc->p_lock = usb_lib_mutex_create(&__g_uvc_lib.lib);
    if (p_vc->p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "(%d)");
        return -USB_EPERM;
    }
#endif

    /* 遍历接口*/
    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_usb_fun->intf_list){
        p_intf = usb_container_of(p_node, struct usbh_interface, node);

        /* 接口子类是视频控制子类*/
        if (USBH_INTF_SUB_CLASS_GET(p_intf) == UVC_VSC_VIDEOCONTROL){
        	p_vc->p_ctrl_intf = p_intf;
        }
        else if (USBH_INTF_SUB_CLASS_GET(p_intf) == UVC_VSC_VIDEOSTREAMING) {
        	p_vc->n_stream_intf_altset++;
        }
    }
    /* 设置 USB 视频类设备结构体的 USB 功能结构体*/
    p_vc->p_ufun = p_usb_fun;
    /* 有些 USB 摄像头有兼容问题，这里需要根据特定牌子的摄像头来设置兼容*/
    //todo
    /* 分析视频类控制描述符*/
    ret = __vc_ctrl_parse(p_vc);
    if (ret != USB_OK) {
    	__USB_ERR_INFO("parse USB video class control descriptors failed(%d)\r\n", ret);
        return ret;
    }

    __USB_INFO("found USB video class %u.%02x device %s (%04x:%04x)\r\n",
                p_vc->uvc_version >> 8, p_vc->uvc_version & 0xff,
                USBH_DEV_PDTSTR_GET(p_usb_fun) ? USBH_DEV_PDTSTR_GET(p_usb_fun) : "<unnamed>",
                USBH_DEV_VID_GET(p_usb_fun), USBH_DEV_PID_GET(p_usb_fun));

    /* 初始化 USB 视频类设备控件*/
    ret = usbh_vc_ctrl_init(p_vc);
    if (ret != USB_OK) {
        return ret;
    }
    /* 扫描 USB 视频类设备实体，寻找视频链*/
    ret = usbh_vc_video_chain_scan(p_vc);
    if (ret != USB_OK) {
    	return ret;
    }

    /* 注册视频设备节点*/
    return usbh_vc_video_chain_register(p_vc);
}

/**
 * \brief 反初始化 USB 视频类设备
 */
static int __vc_dev_deinit(struct usbh_vc *p_vc){
    int ret = USB_OK;

    /* 清除视频流*/
    usbh_vc_stream_clr(p_vc);
    /* 清除实体*/
    usbh_vc_entity_free(p_vc);
    /* 清除视频链*/
    usbh_vc_video_chain_clr(p_vc);
    /* 清除设备控件*/
    usbh_vc_ctrl_deinit(p_vc);
#if USB_OS_EN
    if (p_vc->p_lock) {
        ret = usb_lib_mutex_destroy(&__g_uvc_lib.lib, p_vc->p_lock);
        if(ret != USB_OK){
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
        }
    }
#endif
    return ret;
}

/**
 * \breif USB 视频类控制事件处理函数
 */
static void __vc_control_event(struct usbh_vc *p_vc, uint8_t *p_data, int len){
    char *p_attrs[3] = { "value", "info", "failure" };

    if ((len < 6) || (p_data[2] != 0) || (p_data[4] > 2)) {
        //__UVC_STATUS_TRACE("Invalid control status event received\r\n");
        return;
    }

    __USB_INFO("control %u/%u %s change len %d\r\n", p_data[1], p_data[3], p_attrs[p_data[4]], len);
}

/**
 * \brief USB 视频类视频流事件处理函数
 */
static void __vc_stream_event(struct usbh_vc *p_vc, uint8_t *p_data, int len){
    if (len < 3) {
        //__UVC_STATUS_TRACE("Invalid streaming status event received\r\n");
        return;
    }

    if (p_data[2] == 0) {
        if (len < 4) {
            return;
        }
        __USB_INFO("button (intf %u) %s len %d\r\n", p_data[1], p_data[3] ? "pressed" : "released", len);
    } else {
    	__USB_INFO("stream %u error event %02x %02x len %d\r\n", p_data[1], p_data[2], p_data[3], len);
    }
}

/**
 * \brief USB 视频类获取状态完成回调函数
 */
static void __vc_status_complete(void *arg){
    struct usbh_vc  *p_vc  = (struct usbh_vc *)arg;
    struct usbh_trp *p_trp = &p_vc->int_trp;
    int              len, ret;

    /* 检查状态*/
    switch (p_trp->status) {
        case 0:
            break;
        case -USB_EPIPE:
        case -USB_EPROTO:
            return;
        default:
            return;
    }
    /* 获取传输的实际长度*/
    len = p_trp->act_len;
    if (len > 0) {
        switch (p_vc->p_status[0] & 0x0f) {
            /* 控制类型状态*/
            case UVC_STATUS_TYPE_CONTROL:
            	__vc_control_event(p_vc, p_vc->p_status, len);
                break;
            /* 视频流类型状态*/
            case UVC_STATUS_TYPE_STREAMING:
            	__vc_stream_event(p_vc, p_vc->p_status, len);
                break;

            default:
            	__USB_ERR_INFO("unknown status event type %u\r\n", p_vc->p_status[0]);
                break;
        }
    }
    ret = usbh_trp_submit(&p_vc->int_trp);
    if (ret != USB_OK) {
        __USB_ERR_INFO("submit status trp failed(%d)\r\n", ret);
    }
}

/**
 * \brief 初始化状态端点请求包
 */
static int __vc_status_trp_init(struct usbh_vc *p_vc){
    struct usbh_endpoint *p_ep = p_vc->p_int_ep;

    if (p_ep == NULL) {
        return USB_OK;
    }

    p_vc->p_status = usb_lib_malloc(&__g_uvc_lib.lib, UVC_MAX_STATUS_SIZE);
    if (p_vc->p_status == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_vc->p_status, 0, UVC_MAX_STATUS_SIZE);

    p_vc->int_trp.p_data = p_vc->p_status;
    /* 填充一个中断传输请求包*/
    p_vc->int_trp.p_ctrl    = NULL;
    p_vc->int_trp.len       = UVC_MAX_STATUS_SIZE;
    p_vc->int_trp.p_fn_done = __vc_status_complete;
    p_vc->int_trp.p_arg     = p_vc;
    p_vc->int_trp.act_len   = 0;
    p_vc->int_trp.status    = 0;
    p_vc->int_trp.flag      = 0;
    p_vc->int_trp.p_ep      = p_ep;

    return USB_OK;
}

/**
 * \brief USB 视频类中断传输包反初始化
 */
void __vc_status_trp_deinit(struct usbh_vc *p_vc){
    /* 取消传输请求包*/
    //usbh_trp_cancel(&dev->int_trp);
    /* 释放传输请求包的数据缓存*/
    if (p_vc->p_status) {
    	usb_lib_mfree(&__g_uvc_lib.lib, p_vc->p_status);
    }
    p_vc->p_int_ep = NULL;
}

/**
 * \brief USB 视频类设备释放函数
 */
static void __drv_release(int *p_ref){
    int             ret;
    struct usbh_vc *p_vc = NULL;

    p_vc = USB_CONTAINER_OF(p_ref, struct usbh_vc, ref_cnt);

    /* 删除节点*/
    ret = usb_lib_dev_del(&__g_uvc_lib.lib, &p_vc->node);
    if (ret != USB_OK) {
        return;
    }

    /* 反初始化状态请求包*/
    __vc_status_trp_deinit(p_vc);
    /* 反初始*/
    __vc_dev_deinit(p_vc);
    /* 释放 USB 设备*/
    ret = usbh_dev_ref_put(p_vc->p_ufun->p_usb_dev);
    if (ret != USB_OK) {
    	__USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
    }
    usb_lib_mfree(&__g_uvc_lib.lib, p_vc);
}

/**
 * \brief 增加 USB 视频类设备的引用
 */
int usbh_vc_ref_get(struct usbh_vc *p_vc){
    return usb_refcnt_get(&p_vc->ref_cnt);
}

/**
 * \brief 取消 USB 视频类设备引用
 */
int usbh_vc_ref_put(struct usbh_vc *p_vc){
    return usb_refcnt_put(&p_vc->ref_cnt, __drv_release);
}

/**
 * \brief 获取 USB 视频类设备视频流的个数
 *
 * \param[in] p_vc       USB 视频类设备
 * \param[in] p_n_stream 存储视频流缓存
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_nstream_get(struct usbh_vc *p_vc, uint32_t *p_n_stream){
    int ret = USB_OK;

    if ((p_vc == NULL) || (p_n_stream == NULL)) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_vc->p_lock, UVC_LOCK_TIMEOUT);
    if(ret != USB_OK){
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_n_stream = p_vc->n_streams;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_vc->p_lock);
    if(ret != USB_OK){
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 获取 USB 视频类设备名字
 *
 * \param[in] p_vc      USB 视频类设备
 * \param[in] p_name    存储设备名字的缓存
 * \param[in] name_size 名字缓存大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_name_get(struct usbh_vc *p_vc,
                     char           *p_name,
                     uint32_t        name_size){
    int ret = USB_OK;

    if ((p_vc == NULL) || (p_name == NULL)) {
        return -USB_EINVAL;
    }
    if ((strlen(p_vc->name) + 1) > name_size) {
        return -USB_EILLEGAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_vc->p_lock, UVC_LOCK_TIMEOUT);
    if(ret != USB_OK){
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    strcpy(p_name, p_vc->name);
#if USB_OS_EN
    ret = usb_mutex_unlock(p_vc->p_lock);
    if(ret != USB_OK){
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 设置 USB 视频类设备名字
 *
 * \param[in] p_vc   USB 视频类设备
 * \param[in] p_name 要设置的名字
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_name_set(struct usbh_vc *p_vc, char *p_name){
    int ret = USB_OK;

    if ((p_vc == NULL) || (p_name == NULL)) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_vc->p_lock, UVC_LOCK_TIMEOUT);
    if(ret != USB_OK){
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    strcpy(p_vc->name, p_name);
#if USB_OS_EN
    ret = usb_mutex_unlock(p_vc->p_lock);
    if(ret != USB_OK){
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 视频类设备探测函数
 *
 * \param[in] p_usb_fun 相关的 USB 功能接口
 *
 * \retval 探测成功返回 USB_OK
 */
int usbh_vc_probe(struct usbh_function *p_usb_fun){
    struct usb_list_node  *p_node             = NULL;
    struct usb_list_node  *p_node_tmp         = NULL;
    struct usbh_interface *p_intf             = NULL;
    uint8_t                find_video_control = 0;
    uint8_t                find_video_stream  = 0;

    /* 遍历接口*/
    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_usb_fun->intf_list){
        p_intf = usb_container_of(p_node, struct usbh_interface, node);

        /* 接口类代码不是视频类*/
        if (USBH_INTF_CLASS_GET(p_intf) != USB_CLASS_VIDEO) {
            return -USB_ENOTSUP;
        }

        /* 接口子类是视频控制子类*/
        if (USBH_INTF_SUB_CLASS_GET(p_intf) == UVC_VSC_VIDEOCONTROL) {
            find_video_control = 1;
        } else if (USBH_INTF_SUB_CLASS_GET(p_intf) == UVC_VSC_VIDEOSTREAMING) { /* 接口子类是视频流子类*/
            find_video_stream  = 1;
        }
    }
    /* 需要同时存在频控制子类和视频流子类*/
    if (find_video_stream && find_video_control) {

        p_usb_fun->func_type = USBH_FUNC_UVC;

        return USB_OK;
    }

    return -USB_ENOTSUP;
}

/**
 * \brief 创建一个 USB 视频类设备
 *
 * \param[in] p_usb_fun 相关的 USB 功能接口
 * \param[in] p_name    USB 视频类设备的名字
 *
 * \retval 成功返回 0
 */
int usbh_vc_create(struct usbh_function *p_usb_fun, char *p_name){
    struct usbh_vc *p_vc = NULL;
    int             ret  = -USB_ENOTSUP;
    int             ret_tmp;

    if ((p_usb_fun == NULL) || (p_name == NULL)) {
        return -USB_EINVAL;
    }

    /* 检查 USB 库是否正常*/
    if ((usb_lib_is_init(&__g_uvc_lib.lib) == USB_FALSE) ||
            (__g_uvc_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    /* 功能类型未确定，检查一下是否是 USB 视频类设备*/
    if (p_usb_fun->func_type != USBH_FUNC_UVC) {
        if (p_usb_fun->func_type == USBH_FUNC_UNKNOWN) {
            ret = usbh_vc_probe(p_usb_fun);
        }
        if (ret != USB_OK) {
            __USB_ERR_INFO("usb function is not video class\r\n");
            return ret;
        }
    }

    /* 引用 USB 设备*/
    ret = usbh_dev_ref_get(p_usb_fun->p_usb_dev);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
        return ret;
    }

    p_vc = usb_lib_malloc(&__g_uvc_lib.lib, sizeof(struct usbh_vc));
    if (p_vc == NULL) {
        ret = -USB_ENOMEM;
        goto __failed;
    }
    memset(p_vc, 0, sizeof(struct usbh_vc));

    /* 初始化 USB 视频类设备*/
    ret = __dev_init(p_vc, p_usb_fun, p_name);
    if (ret != USB_OK) {
        goto __failed;
    }
    /* 初始化中断包*/
    ret = __vc_status_trp_init(p_vc);
    if (ret != USB_OK) {
        goto __failed;
    }

    /* 往库中添加一个设备*/
    ret = usb_lib_dev_add(&__g_uvc_lib.lib, &p_vc->node);
    if (ret != USB_OK) {
        goto __failed;
    }

    ret = usb_refcnt_get(&__g_uvc_lib.ref_cnt);
    if (ret != USB_OK) {
        goto __failed;
    }
    /* 设置 USB 设备类型*/
    usbh_dev_type_set(p_usb_fun->p_usb_dev, p_usb_fun->func_type);

    /* 设置 USB 功能结构体驱动数据*/
    usbh_func_drvdata_set(p_usb_fun, p_vc);

    return USB_OK;

__failed:
    if (p_vc != NULL) {
    	__vc_dev_deinit(p_vc);

        /* 反初始化状态请求包*/
        __vc_status_trp_deinit(p_vc);

        usb_lib_mfree(&__g_uvc_lib.lib, p_vc);
    }

    ret_tmp = usbh_dev_ref_put(p_usb_fun->p_usb_dev);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret_tmp);
    }

    return ret;
}

/**
 * \brief 销毁 USB 视频类设备
 *
 * \param[in] p_usb_fun USB 功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_destroy(struct usbh_function *p_usb_fun){
    int             ret  = USB_OK;
    struct usbh_vc *p_vc = NULL;

    if (p_usb_fun == NULL) {
        return -USB_EINVAL;
    }
    if (p_usb_fun->func_type != USBH_FUNC_UVC) {
        return -USB_EILLEGAL;
    }

    ret = usbh_func_drvdata_get(p_usb_fun, (void **)&p_vc);
    if ((ret != USB_OK) || (p_vc == NULL)) {
        return ret;
    }
    p_vc->is_removed = USB_TRUE;

    return usbh_vc_ref_put(p_vc);
}

/**
 * \brief 复位 USB 视频类设备遍历
 */
void usbh_vc_traverse_reset(void){
    usb_lib_dev_traverse_reset(&__g_uvc_lib.lib);
}

/**
 * \brief USB 视频类设备遍历
 *
 * \param[out] p_name    返回的名字缓存
 * \param[in]  name_size 名字缓存的大小
 * \param[out] p_vid     返回的设备 VID
 * \param[out] p_pid     返回的设备 PID
 *
 * \retval 成功返回 USB_OK，到末尾返回 -USB_END
 */
int usbh_vc_traverse(char     *p_name,
                     uint32_t  name_size,
                     uint16_t *p_vid,
                     uint16_t *p_pid){
    int                   ret;
    struct usb_list_node *p_node = NULL;
    struct usbh_vc       *p_vc  = NULL;

    ret = usb_lib_dev_traverse(&__g_uvc_lib.lib, &p_node);
    if (ret == USB_OK) {
    	p_vc = usb_container_of(p_node, struct usbh_vc, node);
        if (p_name != NULL) {
            strncpy(p_name, p_vc->name, name_size);
        }
        if (p_vid != NULL) {
            *p_vid = USBH_VC_DEV_VID_GET(p_vc);
        }
        if (p_pid != NULL) {
            *p_pid = USBH_VC_DEV_PID_GET(p_vc);
        }
    }
    return ret;
}

/**
 * \brief 打开一个 USB 视频类设备
 *
 * \param[in]  p_handle 打开句柄
 * \param[in]  flag     打开标志，本接口支持两种种打开方式：
 *                      USBH_DEV_OPEN_BY_NAME 是通过名字打开设备
 *                      USBH_DEV_OPEN_BY_UFUN 是通过 USB 功能结构体打开设备
 * \param[out] p_vc     返回获取到的 USB 视频类设备地址
 *
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_open(void *p_handle, uint8_t flag, struct usbh_vc **p_vc){
    int             ret;
    struct usbh_vc *p_vc_tmp = NULL;

    if (p_handle == NULL) {
        return -USB_EINVAL;
    }
    if ((flag != USBH_DEV_OPEN_BY_NAME) && (flag != USBH_DEV_OPEN_BY_UFUN)) {
        return -USB_EILLEGAL;
    }
    if ((usb_lib_is_init(&__g_uvc_lib.lib) == USB_FALSE) ||
            (__g_uvc_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    if (flag == USBH_DEV_OPEN_BY_NAME) {
        struct usb_list_node *p_node     = NULL;
        struct usb_list_node *p_node_tmp = NULL;
        char                 *p_name     = (char *)p_handle;

#if USB_OS_EN
        ret = usb_lib_mutex_lock(&__g_uvc_lib.lib);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
        USB_LIB_LIST_FOR_EACH_NODE(p_node, p_node_tmp, &__g_uvc_lib.lib){
            p_vc_tmp = usb_container_of(p_node, struct usbh_vc, node);
            if (strcmp(p_vc_tmp->name, p_name) == 0) {
                break;
            }
            p_vc_tmp = NULL;
        }
#if USB_OS_EN
        ret = usb_lib_mutex_unlock(&__g_uvc_lib.lib);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
    } else if (flag == USBH_DEV_OPEN_BY_UFUN) {
        struct usbh_function *p_usb_fun = (struct usbh_function *)p_handle;

        usbh_func_drvdata_get(p_usb_fun, (void **)&p_vc_tmp);
    }
    if (p_vc_tmp) {
        ret = usbh_vc_ref_get(p_vc_tmp);
        if (ret != USB_OK) {
            return ret;
        }
        *p_vc = p_vc_tmp;

        return USB_OK;
    }
    return -USB_ENODEV;
}

/**
 * \brief 关闭 USB 视频类设备
 *
 * \param[in] p_vc USB 视频类设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_close(struct usbh_vc *p_vc){
    if (p_vc == NULL) {
        return -USB_EINVAL;
    }
    return usbh_vc_ref_put(p_vc);
}

/**
 * \brief 获取当前存在的 USB 视频类驱动设备数量
 *
 * \param[in] p_n_dev 存储返回的设备数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_lib_ndev_get(uint8_t *p_n_dev){
    if (__g_uvc_lib.is_lib_deiniting == USB_TRUE) {
        return -USB_ENOINIT;
    }

    return usb_lib_ndev_get(&__g_uvc_lib.lib, p_n_dev);
}

/**
 * \brief USB 视频类驱动库相关初始化
 *
 * \param[in] stream_num_max 最大支持 USB 视频类设备数据流数量
 * \param[in] stream_buf_num 设备数据流缓存个数
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_lib_init(uint8_t stream_num_max,
                     uint8_t stream_buf_num){
#if USB_MEM_RECORD_EN
    int ret = usb_lib_init(&__g_uvc_lib.lib, "uvc_mem");
#else
    int ret = usb_lib_init(&__g_uvc_lib.lib);
#endif
    if (ret != USB_OK) {
        return ret;
    }
    /* 创建缓存队列*/
    ret = usbh_vc_queue_create(stream_num_max, stream_buf_num, &__g_uvc_lib.p_queue);
    if(ret != USB_OK){
        return ret;
    }
    /* 初始化引用计数*/
    usb_refcnt_init(&__g_uvc_lib.ref_cnt);

    __g_uvc_lib.n_stream_max = stream_num_max;
    __g_uvc_lib.n_stream     = 0;
    __g_uvc_lib.stream_idx   = 0;
    __g_uvc_lib.n_dev        = 0;

    return USB_OK;
}

/**
 * \brief USB 视频类驱动库释放函数
 */
static void __uvc_lib_release(int *p_ref){
    struct usbh_vc_lib *p_uvc_lib = &__g_uvc_lib;

    usb_lib_deinit(&__g_uvc_lib.lib);

    usbh_vc_queue_destory(p_uvc_lib->p_queue, p_uvc_lib->n_stream_max);

    p_uvc_lib->n_stream_max = 0;
    p_uvc_lib->n_stream     = 0;
    p_uvc_lib->stream_idx   = 0;
    p_uvc_lib->n_dev        = 0;
}

/**
 * \brief 反初始化 USB 视频类驱动库
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_lib_deinit(void){
    /* 检查 USB 库是否正常*/
    if (usb_lib_is_init(&__g_uvc_lib.lib) == USB_FALSE) {
        return USB_OK;
    }
    __g_uvc_lib.is_lib_deiniting = USB_TRUE;

    return usb_refcnt_put(&__g_uvc_lib.ref_cnt, __uvc_lib_release);
}

/**
 * \brief 获取 USB 视频类驱动库内存记录
 *
 * \param[out] p_mem_record 返回内存记录
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_lib_mem_record_get(struct usb_mem_record *p_mem_record){
    return usb_lib_mem_record_get(&__g_uvc_lib.lib, p_mem_record);
}

