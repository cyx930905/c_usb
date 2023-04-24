/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/hid/usbh_hid_drv.h"
#include "core/include/usb_mem.h"
#include "utils/refcnt/usb_refcnt.h"
#include <string.h>
#include <stdio.h>

/*******************************************************************************
 * Static
 ******************************************************************************/
/* \brief 声明一个 USB 人体接口库结构体*/
struct uhid_lib __g_uhid_lib;

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern void usbh_hid_report_init(struct usbh_hid *p_hid);
extern int usbh_hid_report_scan(struct usbh_hid *p_hid);
extern int usbh_hid_report_parse(struct usbh_hid *p_hid);

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief USB 人体接口设备输入中断回调函数
 */
static void __hid_irq_in(void *p_arg){
    //todo
}

/**
 * \brief USB 人体接口设备空闲设置
 */
static int __hid_idle_set(struct usbh_function *p_usb_fun, int intf_num, int report, int idle){
    return usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                    USB_REQ_TAG_INTERFACE | USB_REQ_TYPE_CLASS,
                                    HID_REQ_SET_IDLE,
                                   (idle << 8) | report,
                                    intf_num,
                                    0,
                                    NULL,
                                  __g_uhid_lib.xfer_time_out,
                                    0);
}

/**
 * \brief USB 人体接口设备获取类描述符
 */
static int __hid_class_desc_get(struct usbh_function *p_usb_fun,
                                int                   intf_num,
                                unsigned char         type,
                                void                 *p_buf,
                                int                   size){
    int ret, retry = 4;

    memset(p_buf, 0, size);

    do {
    	ret = usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                       USB_REQ_TAG_INTERFACE | USB_DIR_IN,
                                       USB_REQ_GET_DESCRIPTOR,
                                      (type << 8),
                                       intf_num,
                                       size,
                                       p_buf,
                                     __g_uhid_lib.xfer_time_out,
                                       0);
        retry--;
    } while ((ret < size) && retry);

    if (ret == size) {
    	return USB_OK;
    }
    return ret;
}

static int __hid_max_report_get(struct usbh_hid *p_hid,
                                uint32_t         type,
								uint32_t        *p_max){
	struct hid_report *report;
	unsigned int size;

//	usb_list_for_each_node(report, &hid->report_enum[type].report_list, list) {
//		size = ((report->size - 1) >> 3) + 1 + hid->report_enum[type].numbered;
//		if (*max < size)
//			*max = size;
//	}
}

//static int __hid_raw_req(struct usbh_hid *p_hid,
//                         uint8_t          report_num,
//                         uint8_t         *p_buf,
//                         uint32_t         len,
//                         uint8_t          report_type,
//                         int              req_type){
//    switch (req_type) {
//        case HID_REQ_GET_REPORT:
//            return __hid_raw_report_get(hid, reportnum, buf, len, rtype);
//        case HID_REQ_SET_REPORT:
//            return __hid_raw_report_set(hid, reportnum, buf, len, rtype);
//        default:
//            return -USB_ENOTSUP;
//    }
//    return -USB_ENOTSUP;
//}

/**
 * \brief USB 人体接口设备分析
 */
static int __hid_parse(struct usbh_hid *p_hid, struct usbh_interface *p_intf){
    int                    ret;
    uint8_t               *p_rdesc       = NULL;
    uint8_t                i             = 0;
    uint32_t               rsize, quirks = 0;
    struct usbh_function  *p_usb_fun     = p_hid->p_usb_fun;
    struct hid_descriptor *p_hid_desc    = NULL;

    if (USBH_FUNC_SUBCLASS_GET(p_usb_fun) == USB_INTERFACE_SUBCLASS_BOOT) {
        if ((USBH_FUNC_PROTO_GET(p_usb_fun) == USB_INTERFACE_PROTOCOL_KEYBOARD) ||
                (USBH_FUNC_PROTO_GET(p_usb_fun) == USB_INTERFACE_PROTOCOL_MOUSE)) {
            quirks |= HID_QUIRK_NOGET;
        }
	}
    /* 获取额外描述符*/
    ret = usb_extra_desc_get(p_intf->p_extra,
                             p_intf->extra_len,
                             HID_DT_HID,
                            (void **)&p_hid_desc);
    if (ret != USB_OK) {
        if ((USBH_INTF_NEP_GET(p_intf) == 0) ||
                (usb_extra_desc_get(p_intf->p_eps[0].p_extra,
                                    p_intf->p_eps[0].extra_len,
                                    HID_DT_HID,
                                   (void **)&p_hid_desc) == USB_OK)) {
        	__USB_ERR_INFO("can't find human interface device class descriptor\r\n");
        	return -USB_ENOTSUP;
        }
    }
    p_hid->version = USB_CPU_TO_LE16(p_hid_desc->bcd_hid);
    p_hid->country = p_hid_desc->country_code;

    for (i = 0; i < p_hid_desc->num_descriptors; i++) {
        if (p_hid_desc->desc[i].descriptor_type == HID_DT_REPORT) {
            rsize = USB_CPU_TO_LE16(p_hid_desc->desc[i].descriptor_length);
        }
    }
    if ((rsize == 0) || (rsize > HID_MAX_DESCRIPTOR_SIZE)) {
        __USB_ERR_INFO("human interface device report descriptor length %d illegal\r\n", rsize);
		return -USB_EILLEGAL;
    }

    p_rdesc = usb_lib_malloc(&__g_uhid_lib.lib, rsize);
    if (p_rdesc == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_rdesc, 0, rsize);

    ret = __hid_idle_set(p_usb_fun, USBH_INTF_NUM_GET(p_intf), 0, 0);
    if (ret != USB_OK) {
        __USB_ERR_INFO("human interface device idle set failed(%d)\r\n", ret);
        goto __failed;
    }
    /* 获取类描述符*/
    ret = __hid_class_desc_get(p_usb_fun,
                               USBH_INTF_NUM_GET(p_intf),
                               HID_DT_REPORT,
                               p_rdesc,
                               rsize);
    if (ret != USB_OK) {
        __USB_ERR_INFO("human interface device class descriptor get failed(%d)\r\n", ret);
        goto __failed;
    }

    p_hid->p_report_desc    = p_rdesc;
    p_hid->report_desc_size = rsize;
	p_hid->quirks          |= quirks;

	return USB_OK;
__failed:
    usb_lib_mfree(&__g_uhid_lib.lib, p_rdesc);

    return ret;
}

/**
 * \brief 初始化 USB 人类接口设备
 */
static int __hid_init(struct usbh_hid      *p_hid,
                      struct usbh_function *p_usb_fun,
                      char                 *p_name){
	int                    ret;
    struct usbh_interface *p_intf = NULL;

    /* 获取第一个接口*/
    p_intf = usbh_func_first_intf_get(p_usb_fun);
    if (p_intf == NULL) {
        return -USB_ENODEV;
    }

#if USB_OS_EN
    /* 创建互斥锁*/
    p_hid->p_lock = usb_lib_mutex_create(&__g_uhid_lib.lib);
    if (p_hid->p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        return -USB_EPERM;
    }
#endif

	p_hid->p_usb_fun = p_usb_fun;

    /* 设置设备名字*/
    strncpy(p_hid->name, p_name, (USB_NAME_LEN - 1));
    /* 初始化引用计数*/
    usb_refcnt_init(&p_hid->ref_cnt);
    /* 分析人体接口设备*/
    ret = __hid_parse(p_hid, p_intf);
    if (ret != USB_OK) {
        __USB_ERR_INFO("human interface device parse failed(%d)\r\n", ret);
        return ret;
    }
    /* 报告初始化*/
    usbh_hid_report_init(p_hid);

    /* 扫描人体接口设备报告描述符*/
    ret = usbh_hid_report_scan(p_hid);
    if (ret != USB_OK) {
        __USB_ERR_INFO("human interface device report descriptor scan failed(%d)\r\n", ret);
        return ret;
    }
    /* 分析人体接口设备报告描述符*/
    ret = usbh_hid_report_parse(p_hid);
//    ret = __hid_xfer_init(p_hid);
//    if (ret != USB_OK) {
//        goto __failed1;
//    }
}

/**
 * \brief 反初始化 USB 人类接口设备
 */
static int __hid_deinit(struct usbh_hid *p_hid){
    return USB_OK;
}

/**
 * \brief USB 人类接口设备释放函数
 */
static void __hid_release(int *p_ref){

}

/**
 * \brief 增加 USB 人类接口设备的引用
 */
static int __hid_ref_get(struct usbh_hid *p_hid){
    return usb_refcnt_get(&p_hid->ref_cnt);
}

/**
 * \brief 取消 USB 人类接口设备引用
 */
static int __hid_ref_put(struct usbh_hid *p_hid){
    return usb_refcnt_put(&p_hid->ref_cnt, __hid_release);
}

/**
 * \brief USB 人类接口设备探测函数
 *
 * \param[in] p_usb_fun USB 功能结构体
 *
 * \retval 是 USB 人类接口设备返回 USB_OK
 */
int usbh_hid_probe(struct usbh_function *p_usb_fun){
    int                    class;
    uint8_t                n_eps, i, n_eps_int;
    struct usbh_interface *p_intf = NULL;

    /* 检查功能类*/
    class = USBH_FUNC_CLASS_GET(p_usb_fun);
    if (class != USB_CLASS_HID) {
        return -USB_ENOTSUP;
    }

    /* 获取第一个接口*/
    p_intf = usbh_func_first_intf_get(p_usb_fun);
    if (p_intf == NULL) {
        return -USB_ENODEV;
    }

    /* 寻找输入中断端点*/
    n_eps = USBH_INTF_NEP_GET(p_intf);
    for (i = 0; i < n_eps; i++) {
        if (USBH_EP_DIR_GET(&p_intf->p_eps[i]) &&
                USBH_EP_TYPE_GET(&p_intf->p_eps[i]) == USB_EP_TYPE_INT) {
            n_eps_int++;
        }
    }
    if (n_eps_int == 0) {
        __USB_ERR_INFO("can't find an input interrupt endpoint\n");
        return -USB_ENOTSUP;
    }

    p_usb_fun->func_type = USBH_FUNC_UHID;

    return USB_OK;
}

/**
 * \brief 获取 USB 人类接口设备名字
 *
 * \param[in] p_hub     USB 人类接口设备结构体
 * \param[in] p_name    存储返回的名字缓存
 * \param[in] name_size 名字缓存大小
 *
 * \retval 成功获取返回 USB_OK
 */
int usbh_hid_name_get(struct usbh_hid *p_hid,
                      char            *p_name,
                      uint32_t         name_size){
    int ret = USB_OK;

    if ((p_hid == NULL) || (p_name == NULL)) {
        return -USB_EINVAL;
    }
    if ((strlen(p_hid->name) + 1) > name_size) {
        return -USB_EILLEGAL;
    }

    /* 检查是否允许操作*/
    if (p_hid->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_hid->p_lock, UHID_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    strcpy(p_name, p_hid->name);

#if USB_OS_EN
    ret = usb_mutex_unlock(p_hid->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 设置 USB 人类接口设备名字
 *
 * \param[in] p_hub     USB 人类接口设备结构体
 * \param[in] p_name_new 要设置的新的名字
 *
 * \retval 成功设置返回 USB_OK
 */
int usbh_hid_name_set(struct usbh_hid *p_hid, char *p_name_new){
    int ret = USB_OK;

    if ((p_hid == NULL) || (p_name_new == NULL)) {
        return -USB_EINVAL;
    }

    /* 检查是否允许操作*/
    if (p_hid->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hid->p_lock, UHID_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    memset(p_hid->name, 0, USB_NAME_LEN);
    strncpy(p_hid->name, p_name_new, (USB_NAME_LEN - 1));
#if USB_OS_EN
    ret = usb_mutex_unlock(p_hid->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 创建一个 USB 人体交互设备
 *
 * \param[in] p_usb_fun 相关的 USB 功能接口
 * \param[in] p_name    USB 人体交互设备的名字
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_create(struct usbh_function *p_usb_fun,
                    char                 *p_name){
	int              ret, ret_tmp;
    struct usbh_hid *p_hid = NULL;

    if ((p_usb_fun == NULL) || (p_name == NULL)) {
        return -USB_EINVAL;
    }

    /* 检查 USB 库是否正常*/
    if ((usb_lib_is_init(&__g_uhid_lib.lib) == USB_FALSE) ||
            (__g_uhid_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    /* 功能类型未确定，检查一下是否是 USB 人体交互设备*/
    if (p_usb_fun->func_type != USBH_FUNC_UHID) {
        if (p_usb_fun->func_type == USBH_FUNC_UNKNOWN) {
            ret = usbh_hid_probe(p_usb_fun);
        }
        if (ret != USB_OK) {
        	__USB_ERR_INFO("usb function is not human interface device\r\n");
        	return ret;
        }
    }

    ret = usbh_dev_ref_get(p_usb_fun->p_usb_dev);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
        goto __failed1;
    }

    p_hid = usb_lib_malloc(&__g_uhid_lib.lib, sizeof(struct usbh_hid));
    if (p_hid == NULL) {
        ret = -USB_ENOMEM;
        goto __failed1;
    }
    memset(p_hid, 0, sizeof(struct usbh_hid));

    /* 初始化人体接口设备*/
    ret = __hid_init(p_hid, p_usb_fun, p_name);
    if (ret != USB_OK) {
        goto __failed1;
    }

    /* 往库中添加一个设备*/
    ret = usb_lib_dev_add(&__g_uhid_lib.lib, &p_hid->node);
    if (ret != USB_OK) {
        goto __failed1;
    }

    ret = usb_refcnt_get(&__g_uhid_lib.ref_cnt);
    if (ret != USB_OK) {
        goto __failed2;
    }
    /* 设置 USB 设备类型*/
    usbh_dev_type_set(p_usb_fun->p_usb_dev, p_usb_fun->func_type);
    /* 设置 USB 功能驱动私有数据*/
    usbh_func_drvdata_set(p_usb_fun, p_hid);

    return USB_OK;
__failed2:
    usb_lib_dev_del(&__g_uhid_lib.lib, &p_hid->node);
__failed1:
    if (p_hid) {
        __hid_deinit(p_hid);

        usb_lib_mfree(&__g_uhid_lib.lib, p_hid);
    }
    ret_tmp = usbh_dev_ref_put(p_usb_fun->p_usb_dev);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret_tmp);
    }
    return ret;
}

/**
 * \brief 销毁  USB 人体交互设备
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_destroy(struct usbh_function *p_usb_fun){
    int              ret;
    struct usbh_hid *p_hid = NULL;

    if (p_usb_fun == NULL) {
        return -USB_EINVAL;
    }
    if (p_usb_fun->func_type != USBH_FUNC_UHUB) {
        return -USB_EILLEGAL;
    }

    ret = usbh_func_drvdata_get(p_usb_fun, (void **)&p_hid);
    if (ret != USB_OK) {
        return ret;
    } else if (p_hid == NULL) {
        return -USB_ENODEV;
    }
    p_hid->is_removed = USB_TRUE;

    return __hid_ref_put(p_hid);
}

/**
 * \brief 打开一个 USB 人体接口设备
 *
 * \param[in]  name  设备名字
 * \param[in]  flag  打开标志，本接口支持三种打开方式：
 *                   USBH_DEV_OPEN_BY_NAME 是通过名字打开设备
 *                   USBH_DEV_OPEN_BY_FUN 是通过 USB 功能结构体打开设备
 * \param[out] p_hid 返回打开的 USB 人体接口设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_open(void *p_handle, uint8_t flag, struct usbh_hid **p_hid){
    int              ret;
    struct usbh_hid *p_hid_tmp = NULL;

    if (p_handle == NULL) {
        return -USB_EINVAL;
    }
    if ((flag != USBH_DEV_OPEN_BY_NAME) &&
            (flag != USBH_DEV_OPEN_BY_UFUN)) {
        return -USB_EILLEGAL;
    }
    if ((usb_lib_is_init(&__g_uhid_lib.lib) == USB_FALSE) ||
            (__g_uhid_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    if (flag == USBH_DEV_OPEN_BY_NAME) {
        struct usb_list_node *p_node     = NULL;
        struct usb_list_node *p_node_tmp = NULL;
        char                 *p_name     = (char *)p_handle;

#if USB_OS_EN
        ret = usb_lib_mutex_lock(&__g_uhid_lib.lib);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
        USB_LIB_LIST_FOR_EACH_NODE(p_node, p_node_tmp, &__g_uhid_lib.lib){
        	p_hid_tmp = usb_list_node_get(p_node, struct usbh_hid, node);
            if (strcmp(p_hid_tmp->name, p_name) == 0) {
                break;
            }
            p_hid_tmp = NULL;
        }
#if USB_OS_EN
        ret = usb_lib_mutex_unlock(&__g_uhid_lib.lib);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
    } else if (flag == USBH_DEV_OPEN_BY_UFUN) {
        struct usbh_function *p_usb_fun = (struct usbh_function *)p_handle;

        usbh_func_drvdata_get(p_usb_fun, (void **)&p_hid_tmp);
    }

    if (p_hid_tmp) {
        ret = __hid_ref_get(p_hid_tmp);
        if (ret != USB_OK) {
            return ret;
        }
        *p_hid = p_hid_tmp;

        return USB_OK;
    }
    return -USB_ENODEV;
}

/**
 * \brief 关闭 USB 人体接口设备
 *
 * \param[in] p_hid USB 人体接口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_close(struct usbh_hid *p_hid){
    if (p_hid == NULL) {
        return -USB_EINVAL;
    }
    return __hid_ref_put(p_hid);
}

/**
 * \brief 启动 USB 人体接口设备
 *
 * \param[in] p_hid 要启动的 USB 人体接口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_start(struct usbh_hid *p_hid){
    uint8_t                i;
    uint32_t               in_size   = 0;
    struct usbh_function  *p_usb_fun = NULL;
    struct usbh_interface *p_intf    = NULL;

    if (p_hid == NULL) {
        return -USB_EINVAL;
    }
    /* 确保缓存的最小值*/
    p_hid->buf_size = HID_MIN_BUFFER_SIZE;

    if (p_hid->buf_size > HID_MAX_BUFFER_SIZE) {
        p_hid->buf_size = HID_MAX_BUFFER_SIZE;
	}

    p_usb_fun = p_hid->p_usb_fun;
    /* 获取第一个接口*/
    p_intf = usbh_func_first_intf_get(p_usb_fun);
    if (p_intf == NULL) {
        return -USB_ENODEV;
    }

    for (i = 0; i < USBH_INTF_NEP_GET(p_intf); i++) {
        uint8_t                   interval;
        struct usb_endpoint_desc *p_ep_desc = NULL;

        if (!(USBH_EP_TYPE_GET(&p_intf->p_eps[i]) == USB_EP_TYPE_INT)) {
            continue;
        }
        p_ep_desc = p_intf->p_eps[i].p_ep_desc;

        interval = p_ep_desc->interval;

        if ((p_hid->quirks & HID_QUIRK_FULLSPEED_INTERVAL) &&
                (USBH_DEV_SPEED_GET(p_usb_fun) == USB_SPEED_HIGH)) {
			interval = fls(p_ep_desc->interval * 8);
            __USB_INFO("human interface device \"%s\" fixing fullspeed to highspeed interval: %d -> %d\r\n",
                        p_hid->name, p_ep_desc->interval, interval);
		}

		/* Change the polling interval of mice. */
//		if (p_hid->collection->usage == HID_GD_MOUSE && hid_mousepoll_interval > 0)
//			interval = hid_mousepoll_interval;
        if (USBH_EP_DIR_GET(&p_intf->p_eps[i]) == USB_DIR_IN) {
            if (p_hid->p_trp_in) {
                continue;
            }
            p_hid->p_trp_in = usb_lib_malloc(&__g_uhid_lib.lib, sizeof(struct usbh_trp));
            if (p_hid->p_trp_in == NULL) {
                return -USB_ENOMEM;
            }
            memset(p_hid->p_trp_in, 0, sizeof(struct usbh_trp));

            p_hid->p_trp_in->p_fn_done  = __hid_irq_in;
            p_hid->p_trp_in->p_arg      = p_hid->p_trp_in;
            p_hid->p_trp_in->p_data     = p_hid->p_buf_in;
            p_hid->p_trp_in->interval   = interval;
            p_hid->p_trp_in->p_usr_priv = p_hid;
            p_hid->p_trp_in->p_ep       = &p_intf->p_eps[i];
            p_hid->p_trp_in->len        = in_size;
        } else {

        }
    }

    if (p_hid->quirks & HID_QUIRK_ALWAYS_POLL) {

    }
    return USB_OK;
}

/**
 * \brief 停止 USB 人体接口设备
 *
 * \param[in] p_hid 要停止的 USB 人体接口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_stop(struct usbh_hid *p_hid){
    return USB_OK;
}

/**
 * \brief 初始化 USB 人体接口设备库
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_lib_init(void){
    if (usb_lib_is_init(&__g_uhid_lib.lib) == USB_TRUE) {
        return USB_OK;
    }

#if USB_MEM_RECORD_EN
    int ret = usb_lib_init(&__g_uhid_lib.lib, "uhid_mem");
#else
    int ret = usb_lib_init(&__g_uhid_lib.lib);
#endif
    if (ret != USB_OK) {
        return ret;
    }
    /* 初始化引用计数*/
    usb_refcnt_init(&__g_uhid_lib.ref_cnt);

    __g_uhid_lib.xfer_time_out    = UHID_XFER_TIMEOUT;
    __g_uhid_lib.is_lib_deiniting = USB_FALSE;

    return USB_OK;
}

/**
 * \brief USB 人体接口设备库释放函数
 */
static void __uhid_lib_release(int *p_ref){
    usb_lib_deinit(&__g_uhid_lib.lib);
}

/**
 * \brief 反初始化 USB 人体接口设备库
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_lib_deinit(void){
    /* 检查 USB 库是否正常*/
    if (usb_lib_is_init(&__g_uhid_lib.lib) == USB_FALSE) {
        return USB_OK;
    }
    __g_uhid_lib.is_lib_deiniting = USB_TRUE;

    return usb_refcnt_put(&__g_uhid_lib.ref_cnt, __uhid_lib_release);
}

/**
 * \brief 获取 USB 人体接口设备驱动内存记录
 *
 * \param[out] p_mem_record 返回内存记录
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_lib_mem_record_get(struct usb_mem_record *p_mem_record){
    return usb_lib_mem_record_get(&__g_uhid_lib.lib, p_mem_record);
}

