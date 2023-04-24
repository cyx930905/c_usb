/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/core/usbh.h"

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbh_dev_lib  __g_usbh_dev_lib;
extern struct usbh_core_lib __g_usb_host_lib;

extern int usbh_dev_lib_ref_get(void);
extern int usbh_dev_lib_ref_put(void);

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 检查是否存在指定的 USB 主机设备
 */
static usb_bool_t __is_dev_exsit(uint16_t vid, uint16_t pid, uint32_t dev_type){
#if USB_OS_EN
    int                   ret;
#endif
    struct usb_list_node *p_node        = NULL;
    struct usbh_device   *p_usb_dev_tmp = NULL;
    usb_bool_t            is_find       = USB_FALSE;


    /* 检查参数合法性*/
    if ((dev_type == USBH_DEV_UNKNOWN) &&
            (vid == USBH_DEV_IGONRE_VID) &&
            (pid == USBH_DEV_IGONRE_PID)) {
        return USB_FALSE;
    }
    if (dev_type > USBH_DEV_UNKNOWN) {
        return USB_FALSE;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(__g_usbh_dev_lib.p_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    usb_list_for_each_node(p_node, &__g_usbh_dev_lib.dev_list){
        p_usb_dev_tmp = usb_container_of(p_node, struct usbh_device, node);

        if (!((vid == USBH_DEV_IGONRE_VID) || (vid == p_usb_dev_tmp->p_dev_desc->id_vendor))) {
            continue;
        }
        if (!((pid == USBH_DEV_IGONRE_PID) || (pid == p_usb_dev_tmp->p_dev_desc->id_product))) {
            continue;
        }
        if ((dev_type != USBH_DEV_UNKNOWN) && (!(dev_type & p_usb_dev_tmp->dev_type))) {
            continue;
        }
        is_find = USB_TRUE;
        break;
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(__g_usbh_dev_lib.p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    return is_find;
}

/**
 * \brief USB 主机设备监控工作函数
 */
static void __udev_monitor_job(void *p_arg){
    uint8_t                  state;
    struct usbh_dev_monitor *p_dev_monitor = (struct usbh_dev_monitor *)p_arg;
#if USB_OS_EN
    int                      ret;
#endif

#if USB_OS_EN
    ret = usb_mutex_lock(__g_usbh_dev_lib.p_monitor_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return;
    }
#endif
    state = p_dev_monitor->state;
    p_dev_monitor->state = 0;

    /* 调用用户回调*/
    p_dev_monitor->p_fn_usr_cb(p_dev_monitor->p_arg, state);
#if USB_OS_EN
    ret = usb_mutex_unlock(__g_usbh_dev_lib.p_monitor_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
}

/**
 * \brief USB 主机设备监控检查
 *
 * \param[in] vid      要监控的 USB 主机设备 PID
 * \param[in] pid      要监控的 USB 主机设备 VID
 * \param[in] dev_type 要匹配的 USB 主机设备类型
 * \param[in] type     监控类型
 */
void usbh_dev_monitor_chk(uint16_t  vid,
                          uint16_t  pid,
                          uint32_t  dev_type,
                          uint8_t   type){
    struct usb_list_node    *p_node        = NULL;
    struct usb_list_node    *p_node_tmp    = NULL;
    struct usbh_dev_monitor *p_dev_monitor = NULL;
    int                      ret;

#if USB_OS_EN
    ret = usb_mutex_lock(__g_usbh_dev_lib.p_monitor_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
    }
#endif
    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                             &__g_usbh_dev_lib.monitor_list){
        p_dev_monitor = usb_container_of(p_node, struct usbh_dev_monitor, node);

        if (!((p_dev_monitor->vid == USBH_DEV_IGONRE_VID) || (p_dev_monitor->vid == vid))) {
            continue;
        }
        if (!((p_dev_monitor->pid == USBH_DEV_IGONRE_PID) || (p_dev_monitor->pid == pid))) {
            continue;
        }
        if ((p_dev_monitor->dev_type != USBH_DEV_UNKNOWN) && (!(p_dev_monitor->dev_type & dev_type))) {
            continue;
        }
        /* 匹配监控类型*/
        if ((p_dev_monitor->type & type) == type) {
            if (p_dev_monitor->p_fn_usr_cb != NULL) {
                p_dev_monitor->state = type;
                if (p_dev_monitor->p_job != NULL) {
                    ret = usb_job_start(p_dev_monitor->p_job);
                    if (ret != USB_OK) {
                        __USB_ERR_INFO("USB job start failed(%d)\r\n", ret);
                    }
                } else {
                    p_dev_monitor->p_fn_usr_cb(p_dev_monitor->p_arg, type);
                }
            }
        }
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(__g_usbh_dev_lib.p_monitor_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
}

/**
 * \brief USB 主机设备监控注册函数
 *
 * \param[in]  vid           要监控的 USB 主机设备的 PID
 * \param[in]  pid           要监控的 USB 主机设备的 VID
 * \param[in]  dev_type      要监控的 USB 主机设备的类型
 * \param[in]  monitor_type  监控类型
 * \param[in]  p_fn_usr_cb   用户回调函数
 * \param[in]  p_arg         回调函数参数
 * \param[out] p_monitor_ret 返回的监控句柄
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_monitor_register(uint16_t                  vid,
                              uint16_t                  pid,
                              uint32_t                  dev_type,
                              uint8_t                   monitor_type,
                              void                    (*p_fn_usr_cb)(void *p_arg, uint8_t type),
                              void                     *p_arg,
                              struct usbh_dev_monitor **p_monitor_ret){
    struct usbh_dev_monitor *p_monitor = NULL;
    void                    *p_job     = NULL;
    int                      ret, ret_tmp;
    usb_bool_t               is_find   = USB_FALSE;

    if (p_fn_usr_cb == NULL) {
        return -USB_EINVAL;
    }

    if (((__g_usbh_dev_lib.is_lib_init != USB_TRUE) ||
         (__g_usbh_dev_lib.is_lib_deiniting == USB_TRUE))) {
        return -USB_ENOINIT;
    }

    /* 检查监控的设备类型*/
    if (((dev_type > USBH_DEV_UNKNOWN) ||
            ((monitor_type & (USBH_DEV_INJECT | USBH_DEV_EJECT | USBH_DEV_EXIST)) == 0))) {
        return -USB_EILLEGAL;
    }

    /* 初始化 USB 设备监控结构体*/
    p_monitor = usb_lib_malloc(&__g_usb_host_lib.lib, sizeof(struct usbh_dev_monitor));
    if (p_monitor == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_monitor, 0, sizeof(struct usbh_dev_monitor));

    p_monitor->pid         = pid;
    p_monitor->vid         = vid;
    p_monitor->type        = monitor_type;
    p_monitor->p_fn_usr_cb = p_fn_usr_cb;
    p_monitor->p_arg       = p_arg;
    p_monitor->dev_type    = dev_type;
    p_monitor->state       = 0;

    /* 只查当前存在的设备，没必要插入链表*/
    if (monitor_type != USBH_DEV_EXIST) {
        /* 创建一个工作*/
        p_job = usb_job_create(__udev_monitor_job, (void *)p_monitor);
        if (p_job == NULL) {
            p_monitor->p_job = NULL;
        } else{
            p_monitor->p_job = p_job;
        }

#if USB_OS_EN
        ret = usb_mutex_lock(__g_usbh_dev_lib.p_monitor_lock, USBH_DEV_MUTEX_TIMEOUT);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
            goto __failed;
        }
#endif
        usb_list_node_add_tail(&p_monitor->node, &__g_usbh_dev_lib.monitor_list);

        usbh_dev_lib_ref_get();
#if USB_OS_EN
        ret = usb_mutex_unlock(__g_usbh_dev_lib.p_monitor_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            goto __failed1;
        }
#endif
    }

    if ((monitor_type & USBH_DEV_EXIST)) {
        is_find = __is_dev_exsit(vid, pid, dev_type);
        if (is_find == USB_TRUE) {
            p_monitor->state = USBH_DEV_EXIST;
            if ((monitor_type == USBH_DEV_EXIST) || (p_monitor->p_job == NULL)) {
                p_fn_usr_cb(p_monitor->p_arg, USBH_DEV_EXIST);
            } else {
                ret = usb_job_start(p_monitor->p_job);
                if (ret != USB_OK) {
                    __USB_ERR_INFO("USB job start failed(%d)\r\n", ret);
                    goto __failed1;
                }
            }
        }
    }

    *p_monitor_ret = p_monitor;

    return USB_OK;
__failed1:
    usb_list_node_del(&p_monitor->node);

    usbh_dev_lib_ref_put();
__failed:
    if (p_monitor) {
        usb_lib_mfree(&__g_usb_host_lib.lib, p_monitor);
    }
    if (p_monitor->p_job != NULL) {
        ret_tmp = usb_job_destory(p_monitor);
        if (ret_tmp != USB_OK) {
            __USB_ERR_INFO("USB job destroy failed(%d)\r\n", ret);
        }
    }

    return ret;
}

/**
 * \brief USB 主机设备监控注销函数
 *
 * \param[in] p_monitor 要注销的监控句柄
 *
 * \retval 成功返回 USB_OK
 */
int usbh_dev_monitor_deregister(struct usbh_dev_monitor *p_monitor){
    int ret;

    if (p_monitor == NULL) {
        return -USB_EINVAL;
    }

    if (p_monitor->p_job != NULL) {
        ret = usb_job_destory(p_monitor->p_job);
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB job destroy failed(%d)\r\n", ret);
            return ret;
        }
    }

#if USB_OS_EN
    ret = usb_mutex_lock(__g_usbh_dev_lib.p_monitor_lock, USBH_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    usb_list_node_del(&p_monitor->node);

    usb_lib_mfree(&__g_usb_host_lib.lib, p_monitor);

    usbh_dev_lib_ref_put();

#if USB_OS_EN
    ret = usb_mutex_unlock(__g_usbh_dev_lib.p_monitor_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}
