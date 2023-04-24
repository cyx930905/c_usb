/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/hub/usbh_hub_drv.h"
#include "core/include/specs/usb_hub_specs.h"
#include "common/refcnt/usb_refcnt.h"
#include <string.h>
#include <stdio.h>

/*******************************************************************************
 * Statement
 ******************************************************************************/
/* \brief 声明一个 USB 集线器库结构体*/
static struct uhub_lib __g_uhub_lib;
static void __uhub_lib_release(int *p_ref);

/*******************************************************************************
 * Macro operate
 ******************************************************************************/
/* \brief 设置集线器端口特性*/
#define __HUB_PORT_FEATURE_SET(p_hub, feature, pnum) \
            __hub_feature_req((p_hub), (feature), USB_TRUE, (pnum), USB_TRUE)
/* \brief 清除集线器特性*/
#define __HUB_DEV_FEATURE_CLR(p_hub, feature) \
            __hub_feature_req((p_hub), (feature), USB_FALSE, 0, USB_FALSE)
/* \brief 获取集线器状态*/
#define __HUB_DEV_STATUS_GET(p_hub, p_sta, p_change) \
            __hub_status_get((p_hub), (p_sta), (p_change), USB_FALSE, 0);
/* \brief 设置集线器端口特性*/
#define __HUB_PORT_FEATURE_SET(p_hub, feature, pnum) \
            __hub_feature_req((p_hub), (feature), USB_TRUE, (pnum), USB_TRUE)
/* \brief 清除集线器端口特性*/
#define __HUB_PORT_FEATURE_CLR(p_hub, feature, pnum) \
            __hub_feature_req((p_hub), (feature), USB_TRUE, (pnum), USB_FALSE)
/* \brief 获取集线器状态*/
#define __HUB_PORT_STATUS_GET(p_hub, p_sta, p_change, pnum) \
            __hub_status_get((p_hub), (p_sta), (p_change), USB_TRUE, (pnum))

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 获取集线器信息
 */
static int __hub_info_get(struct usbh_endpoint *p_ep,
                          uint8_t              *p_n_ports,
                          uint32_t             *p_pwr_time){
    uint16_t len, type;
    uint8_t  buf[32] = {0};
    int      ret;

    if (p_ep->p_usb_dev->speed == USB_SPEED_SUPER) {
        type = USB_DT_SS_HUB;
        len = USB_CPU_TO_LE16(12);
    } else {
        type = USB_DT_HUB;
        len = USB_CPU_TO_LE16(32);
    }

    ret = usbh_ctrl_trp_sync_xfer(p_ep,
                                  USB_DIR_IN |
                                  USB_REQ_TYPE_CLASS |
                                  USB_REQ_TAG_DEVICE,
                                  USB_REQ_GET_DESCRIPTOR,
                                  type << 8,
                                  0,
                                  len,
                                  buf,
                                __g_uhub_lib.xfer_time_out,
                                  0);
    if (ret < 0) {
        return ret;
    }

    if (ret < 7) {
        return -USB_EBADF;
    }

    *p_n_ports  = buf[2];
    *p_pwr_time = buf[5];

    return USB_OK;
}

/**
 * \brief USB 集线器特性请求
 */
static int __hub_feature_req(struct usbh_hub *p_hub,
                             uint8_t          feature,
                             usb_bool_t       is_port,
                             uint8_t          port_num,
                             usb_bool_t       is_set){
    uint8_t   type, req;
    uint16_t  idx, val;
    int       ret;
#if USB_OS_EN
    int       ret_tmp;
#endif

    if (is_set) {
        /* 设置特性*/
        req = USB_REQ_SET_FEATURE;
    } else {
        /* 清除特性*/
        req = USB_REQ_CLEAR_FEATURE;
    }

    val  = USB_CPU_TO_LE16(feature);
    type = USB_DIR_OUT | USB_REQ_TYPE_CLASS;
    if (is_port) {
        type |= USB_REQ_TAG_OTHER;
        idx   = USB_CPU_TO_LE16(port_num);
    } else {
        type |= USB_REQ_TAG_DEVICE;
        idx   = USB_CPU_TO_LE16(0);
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hub->p_lock, UHUB_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    ret = usbh_ctrl_trp_sync_xfer(&p_hub->hub_basic.p_usb_fun->p_usb_dev->ep0,
                                   type,
                                   req,
                                   val,
                                   idx,
                                   0,
                                   NULL,
                                 __g_uhub_lib.xfer_time_out,
                                   0);
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hub->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 集线器获取状态
 */
static int __hub_status_get(struct usbh_hub *p_hub,
                            uint16_t        *p_status,
                            uint16_t        *p_change,
                            usb_bool_t       is_port,
                            uint8_t          port_num){
    uint8_t  type;
    uint16_t idx;
    int      ret;
#if USB_OS_EN
    int      ret_tmp;
#endif

    type = USB_DIR_IN | USB_REQ_TYPE_CLASS;
    if (is_port) {
        type |= USB_REQ_TAG_OTHER;
        idx   = USB_CPU_TO_LE16(port_num);
    } else {
        type |= USB_REQ_TAG_DEVICE;
        idx   = USB_CPU_TO_LE16(0);
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_hub->p_lock, UHUB_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    ret = usbh_ctrl_trp_sync_xfer(&p_hub->hub_basic.p_usb_fun->p_usb_dev->ep0,
                                   type,
                                   USB_REQ_GET_STATUS,
                                   0,
                                   idx,
                                   4,
                                  &p_hub->sta,
                                 __g_uhub_lib.xfer_time_out,
                                   0);
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hub->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif

    if (ret >= 0) {
        if (ret == 4) {
            p_hub->sta  = USB_CPU_TO_LE32(p_hub->sta);
            *p_status   = (uint16_t)(p_hub->sta & 0x00FFFF);
            *p_change   = (uint16_t)((p_hub->sta >> 16) & 0x00FFFF);

            ret = USB_OK;
        } else {
            ret = -USB_EPIPE;
        }
    }

    return ret;
}

/**
 * \brief USB 集线器上电操作
 */
static int __hub_power_set(struct usbh_hub_basic *p_hub_basic,
                           usb_bool_t             is_port,
                           uint8_t                    port_num,
                           usb_bool_t             is_power){
    int              ret   = USB_OK;
    struct usbh_hub *p_hub = NULL;

    p_hub = (struct usbh_hub *)p_hub_basic;

    if (is_port == USB_TRUE) {
        if (is_power == USB_FALSE) {
            ret = __HUB_PORT_FEATURE_CLR(p_hub,
                                         HUB_PORT_FEAT_POWER,
                                         port_num);
        } else if (is_power == USB_TRUE) {
            ret = __HUB_PORT_FEATURE_SET(p_hub,
                                         HUB_PORT_FEAT_POWER,
                                         port_num);
        }
    }
    return ret;
}

/**
 * \brief 获取 USB 集线器端口速度
 */
static int __hub_port_speed_get(struct usbh_hub_basic *p_hub_basic,
                                uint8_t                port_num,
                                uint8_t               *p_speed){
    int              ret;
    uint16_t         sta, change;
    struct usbh_hub *p_hub = NULL;

    p_hub = (struct usbh_hub *)p_hub_basic;

    ret = __HUB_PORT_STATUS_GET(p_hub, &sta, &change, port_num);
    if (ret != USB_OK) {
        return ret;
    }

    if (sta & HUB_PORT_STAT_LOW_SPEED) {
        *p_speed = USB_SPEED_LOW;
    } else if (sta & HUB_PORT_STAT_HIGH_SPEED) {
        *p_speed = USB_SPEED_HIGH;
    } else {
        *p_speed = USB_SPEED_FULL;
    }

    return USB_OK;
}

/**
 * \brief 检查 USB 集线器端口是否有设备连接
 */
static int __hub_port_is_connect(struct usbh_hub_basic *p_hub_basic,
                                 uint8_t                port_num){
    uint16_t         sta, change;
    int              ret;
    struct usbh_hub *p_hub= NULL;

    p_hub = (struct usbh_hub *)p_hub_basic;

    ret = __HUB_PORT_STATUS_GET(p_hub, &sta, &change, port_num);
    if (ret != USB_OK) {
        return ret;
    }

    if (change & HUB_PORT_STAT_C_CONNECTION) {
        return USB_FALSE;
    }

    if (sta & HUB_PORT_STAT_CONNECTION) {
        return USB_TRUE;
    }

    return USB_FALSE;
}

/**
 * \brief USB 集线器端口复位
 */
static int __hub_port_reset(struct usbh_hub_basic *p_hub_basic,
                            usb_bool_t             is_port,
                            uint8_t                port_num){
    uint16_t         sta, change;
    uint8_t          i,j;
    int              ret;
    struct usbh_hub *p_hub= NULL;

    p_hub = (struct usbh_hub *)p_hub_basic;

    for (i = 0; i < 10; i++) {
        /* 端口复位*/
        ret = __HUB_PORT_FEATURE_SET(p_hub, HUB_PORT_FEAT_RESET, port_num);
        if (ret != USB_OK) {
            /* 失败重试*/
            usb_mdelay(5);
            continue;
        }

        /* 等待完成*/
        usb_mdelay(20);
        for (j = 0; j < 10; j++) {
            /* 获取端口状态*/
            ret = __HUB_PORT_STATUS_GET(p_hub, &sta, &change, port_num);
            if (ret != USB_OK) {
                usb_mdelay(50);
                continue;
            }
            /* 设备断开，返回*/
            if (!(sta & HUB_PORT_STAT_CONNECTION)) {
                return USB_OK;
            }
            /* 发生复位状态变化和端口状态为使能，复位完成*/
            if ((change & HUB_PORT_STAT_C_RESET) &&
                (sta & HUB_PORT_STAT_ENABLE)) {
                /* 复位完成*/
                return __HUB_PORT_FEATURE_CLR(p_hub, HUB_PORT_FEAT_C_RESET, port_num);
            }
            usb_mdelay(100);
        }
    }

    /* 复位失败，设置端口挂起*/
    __HUB_PORT_FEATURE_SET(p_hub, HUB_PORT_FEAT_SUSPEND, port_num);
    /* 清除端口挂起特性*/
    __HUB_PORT_FEATURE_CLR(p_hub, HUB_PORT_FEAT_SUSPEND, port_num);

    return -USB_EPERM;
}

/**
 * \brief USB 集线器处理函数
 */
static int __hub_handle(struct usbh_hub_basic *p_hub_basic,
                        struct usbh_hub_evt   *p_hub_evt){
    struct usbh_hub *p_hub = NULL;
    uint16_t         data, sta, change;
    int              ret   = USB_OK, port_num;

    p_hub = (struct usbh_hub *)p_hub_basic;

    /* 集线器的中断传输请求包传输错误*/
    if (p_hub->trp.status != USB_OK) {
        p_hub->err_cnt++;
        /* 达到最大错误次数，复位集线器*/
        if (p_hub->err_cnt >= 10) {
            struct usbh_device *p_usb_dev_tmp = NULL;

            p_usb_dev_tmp = p_hub->hub_basic.p_usb_fun->p_usb_dev;
            /* 复位这个集线器*/
            //todo reset this hub
            /* 清除上一级集线器连接当前集线器的端口的上电状态*/
            if (p_usb_dev_tmp->p_hub_basic->p_fn_power) {
                ret = p_usb_dev_tmp->p_hub_basic->p_fn_power(p_usb_dev_tmp->p_hub_basic,
                                                             USB_TRUE,
                                                             p_usb_dev_tmp->port,
                                                             USB_FALSE);
                if (ret != USB_OK) {
                    return ret;
                }

                usb_mdelay(100);

                ret = p_usb_dev_tmp->p_hub_basic->p_fn_power(p_usb_dev_tmp->p_hub_basic,
                                                             USB_TRUE,
                                                             p_usb_dev_tmp->port,
                                                             USB_TRUE);
                if (ret != USB_OK) {
                    return ret;
                }
            }
        }
        data = 0;
    } else {    /* 集线器的中断传输请求包传输完成*/
        p_hub->err_cnt = 0;

        /* 获取集线器状态*/
        data = USB_CPU_TO_LE32(*(uint16_t *)(p_hub->trp.p_data));
    }

    if (p_hub->hub_basic.hub_status != USBH_HUB_RUNNING) {
        return ret;
    }
    ret = usbh_trp_submit(&p_hub->trp);
    if (ret != USB_OK) {
        return ret;
    }

    if (data & 0x01) {
        /* 获取集线器状态*/
        ret = __HUB_DEV_STATUS_GET(p_hub, &sta, &change);
        if (ret != USB_OK) {
            return ret;
        }

        /* 本地电源发生变化*/
        if (change & HUB_CHANGE_LOCAL_POWER) {
            /* 清除电源特性*/
            ret = __HUB_DEV_FEATURE_CLR(p_hub, HUB_LOCAL_POWER);
            if (ret != USB_OK) {
                return ret;
            }
        }
        /* 发生过流变化*/
        if (change & HUB_CHANGE_OVERCURRENT) {
            /* 清除过流特性*/
            ret = __HUB_DEV_FEATURE_CLR(p_hub, HUB_OVER_CURRENT);
            if (ret != USB_OK) {
                return ret;
            }

            if (sta & HUB_STAT_OVERCURRENT) {
                for (port_num = 0; port_num < p_hub->hub_basic.n_ports; port_num++) {
                    /* 集线器所有端口上电*/
                    ret = __HUB_PORT_FEATURE_SET(p_hub, HUB_PORT_FEAT_POWER, port_num);
                    if (ret != USB_OK) {
                        return ret;
                    }
                }
            }
        }
    }
    data >>= 1;
    if (data != 0) {
        /* 有一些控制器必须延时等待状态*/
        usb_mdelay(100);

        /* 集线器端口状态变化*/
        for (port_num = 1; port_num <= p_hub->hub_basic.n_ports; port_num++, data >>= 1) {
            if ((data & 0x01) == 0) {
                continue;
            }
            /* 获取集线器端口状态*/
            ret = __HUB_PORT_STATUS_GET(p_hub, &sta, &change, port_num);
            if (ret != USB_OK) {
                continue;
            }
            /* 连接状态产生变化或该端口上的设备断开*/
            if ((change & HUB_PORT_STAT_C_CONNECTION) ||
                ((p_hub->hub_basic.p_ports[port_num - 1]) && (!(sta & HUB_PORT_STAT_CONNECTION)))) {

                /* 清除集线器端口连接状态变化位*/
                ret = __HUB_PORT_FEATURE_CLR(p_hub, HUB_PORT_FEAT_C_CONNECTION, port_num);
                if (ret != USB_OK) {
                    return ret;
                }

                /* 端口上存在设备数据*/
                ret = usbh_basic_hub_dev_disconnect(&p_hub->hub_basic, port_num);
                if (ret != USB_OK) {
                    __USB_ERR_INFO("hub device disconnect failed(%d)\r\n", ret);
                    return ret;
                }
            }

            /* 有新的设备连接*/
            if ((sta & HUB_PORT_STAT_CONNECTION) && (p_hub->hub_basic.p_ports[port_num - 1] == NULL)) {
                /* 调用集线器端口连接函数*/
                ret = usbh_basic_hub_dev_connect(&p_hub->hub_basic, port_num);
                if (ret != USB_OK) {
                    __USB_ERR_INFO("hub device connect failed(%d)\r\n", ret);
                    return ret;
                }
            }

            /* 端口的使能状态发生变化，清除集线器端口使能特性*/
            if (change & HUB_PORT_STAT_C_ENABLE) {
                ret = __HUB_PORT_FEATURE_CLR(p_hub, HUB_PORT_FEAT_C_ENABLE, port_num);
                if (ret != USB_OK) {
                    return ret;
                }
            }

            /* 端口的挂起状态发生变化，清除集线器端口挂起特性*/
            if (change & HUB_PORT_STAT_C_SUSPEND) {
                ret = __HUB_PORT_FEATURE_CLR(p_hub, HUB_PORT_FEAT_C_SUSPEND, port_num);
                if (ret != USB_OK) {
                    return ret;
                }
            }

            /* 端口的过流状态发生变化，清除集线器端口过流特性*/
            if (change & HUB_PORT_STAT_C_OVERCURRENT) {
                ret = __HUB_PORT_FEATURE_CLR(p_hub, HUB_PORT_FEAT_C_OVER_CURRENT, port_num);
                if (ret != USB_OK) {
                    return ret;
                }
                /* 如果端口无过流状态*/
                if (!(sta & HUB_PORT_STAT_OVERCURRENT)) {
                    /* 设置端口重新上电*/
                    ret = __HUB_PORT_FEATURE_SET(p_hub, HUB_PORT_FEAT_POWER, port_num);
                    if (ret != USB_OK) {
                        return ret;
                    }
                    usb_mdelay(p_hub->hub_basic.pwr_time * 2);
                }
            }
            /* 端口的复位状态发生变化，清除集线器端口复位特性*/
            if (change & HUB_PORT_STAT_C_RESET) {
                ret = __HUB_PORT_FEATURE_CLR(p_hub, HUB_PORT_STAT_C_RESET, port_num);
                if (ret != USB_OK) {
                    return ret;
                }
            }
        }
    }
    return ret;
}

/**
 * \brief 初始化集线器
 */
static int __hub_init(struct usbh_hub      *p_hub,
                      struct usbh_function *p_usb_fun,
                      char                 *p_name){

    int                    ret;
    struct usbh_interface *p_intf = NULL;

    /* 获取第一个接口*/
    p_intf = usbh_func_intf_get(p_usb_fun, p_usb_fun->first_intf_num, 0);
    if (p_intf == NULL) {
        return -USB_ENODEV;
    }
    p_hub->p_ep = USBH_INTF_FIRST_EP_GET(p_intf);
#if USB_OS_EN
    /* 创建互斥锁*/
    p_hub->p_lock = usb_lib_mutex_create(&__g_uhub_lib.lib);
    if (p_hub->p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        return -USB_EPERM;
    }
#endif

    usb_refcnt_init(&p_hub->ref_cnt);

    strncpy(p_hub->name, p_name, (USB_NAME_LEN - 1));
    /* 分配集线器传输请求包的数据缓存*/
    p_hub->trp.p_data = usb_lib_malloc(&__g_uhub_lib.lib, sizeof(uint16_t));
    if (p_hub->trp.p_data == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_hub->trp.p_data, 0, sizeof(uint16_t));

    p_hub->hub_basic.p_usb_fun = p_usb_fun;
    p_hub->err_cnt             = 0;

    /* 初始化集线器事件链表*/
    usb_list_node_init(&p_hub->hub_basic.evt.evt_node);
    /* 获取集线器信息*/
    ret = __hub_info_get(&p_usb_fun->p_usb_dev->ep0,
                         &p_hub->hub_basic.n_ports,
                         &p_hub->hub_basic.pwr_time);
    if (ret != USB_OK) {
        return ret;
    }
    /* 检查端口数量*/
    if ((p_hub->hub_basic.n_ports > USBH_HUB_PORTS_MAX) || (p_hub->hub_basic.n_ports == 0)) {
        __USB_ERR_INFO("hub ports number %d illegal\r\n", p_hub->hub_basic.n_ports);
        return -USB_EILLEGAL;
    }

    p_hub->hub_basic.p_fn_hub_reset    = __hub_port_reset;
    p_hub->hub_basic.p_fn_hub_ps_get   = __hub_port_speed_get;
    p_hub->hub_basic.p_fn_hub_pc_check = __hub_port_is_connect;
    p_hub->hub_basic.p_fn_power        = __hub_power_set;
    p_hub->hub_basic.p_fn_hub_handle   = __hub_handle;

    p_hub->hub_basic.p_ports = usb_lib_malloc(&__g_uhub_lib.lib, sizeof(void *) * p_hub->hub_basic.n_ports);
    if (p_hub->hub_basic.p_ports == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_hub->hub_basic.p_ports, 0, sizeof(void *) * p_hub->hub_basic.n_ports);

    p_hub->hub_basic.p_port_sta = usb_lib_malloc(&__g_uhub_lib.lib, p_hub->hub_basic.n_ports);
    if (p_hub->hub_basic.p_port_sta == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_hub->hub_basic.p_port_sta, 0, p_hub->hub_basic.n_ports);

    /* 设置状态*/
    p_hub->hub_basic.hub_status = USBH_HUB_HALTED;

    return USB_OK;
}

/**
 * \brief 反初始化集线器
 */
static int __hub_deinit(struct usbh_hub *p_hub){
#if USB_OS_EN
    if (p_hub->p_lock) {
        int ret = usb_lib_mutex_destroy(&__g_uhub_lib.lib, p_hub->p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
            return ret;
        }
    }
#endif
    if (p_hub->trp.p_data) {
        usb_lib_mfree(&__g_uhub_lib.lib, p_hub->trp.p_data);
    }
    if (p_hub->hub_basic.p_ports) {
        usb_lib_mfree(&__g_uhub_lib.lib, p_hub->hub_basic.p_ports);
    }
    if (p_hub->hub_basic.p_port_sta) {
        usb_lib_mfree(&__g_uhub_lib.lib, p_hub->hub_basic.p_port_sta);
    }

    return USB_OK;
}

/**
 * \brief 内部集线器中断请求包完成回调函数
 */
static void __hub_intr_done(void *p_arg){
    int ret;

    struct usbh_hub *p_hub = (struct usbh_hub *)p_arg;
    /* 设备已经被移除*/
    if (p_hub->is_removed == USB_TRUE) {
        return;
    }
    ret = usbh_hub_evt_add(&p_hub->hub_basic.evt);
    if (ret != USB_OK) {
        __USB_ERR_INFO("hub event add failed(%d)\r\n", ret);
    }
}

/**
 * \brief 初始化集线器传输请求包
 */
static void __hub_trp_init(struct usbh_hub *p_hub){
    p_hub->trp.p_ctrl    = NULL;
    p_hub->trp.len       = 2;
    p_hub->trp.p_fn_done = __hub_intr_done;
    p_hub->trp.p_arg     = p_hub;
    p_hub->trp.act_len   = 0;
    p_hub->trp.status    = 0;
    p_hub->trp.flag      = 0;
    p_hub->trp.p_ep      = p_hub->p_ep;
}

/**
 * \brief USB 集线器释放函数
 */
static void __hub_release(int *p_ref){
    struct usbh_hub *p_hub = NULL;
    int              ret, i;

    p_hub = USB_CONTAINER_OF(p_ref, struct usbh_hub, ref_cnt);

    /* 删除节点*/
    ret = usb_lib_dev_del(&__g_uhub_lib.lib, &p_hub->node);
    if (ret != USB_OK) {
        return;
    }

    /* 取消 USB 集线器中断包*/
    if (p_hub->hub_basic.hub_status == USBH_HUB_RUNNING) {
        usbh_trp_xfer_cancel(&p_hub->trp);
    }
    /* 检查端口上有没有设备没有被移除*/
    for (i = 1; i <= p_hub->hub_basic.n_ports; i++) {
        ret = usbh_basic_hub_dev_disconnect(&p_hub->hub_basic, i);
        if (ret != USB_OK) {
            return;
        }
    }

    /* 清除残留的事件*/
    if (usb_list_node_is_empty(&p_hub->hub_basic.evt.evt_node) == 0) {
        usb_list_node_del(&p_hub->hub_basic.evt.evt_node);
    }

    __hub_deinit(p_hub);

    usb_lib_mfree(&__g_uhub_lib.lib, p_hub);

    /* 释放对 USB 设备的引用*/
    usbh_dev_ref_put(p_hub->hub_basic.p_usb_fun->p_usb_dev);
}

/**
 * \brief 增加 USB 集线器设备的引用
 */
static int __hub_ref_get(struct usbh_hub *p_hub){
    return usb_refcnt_get(&p_hub->ref_cnt);
}

/**
 * \brief 取消 USB 集线器设备引用
 */
static int __hub_ref_put(struct usbh_hub *p_hub){
    return usb_refcnt_put(&p_hub->ref_cnt, __hub_release);
}

/**
 * \brief USB 主机集线器设备探测
 *
 * \param[in] p_usb_fun USB 功能结构体
 *
 * \retval 是集线器设备返回 USB_OK
 */
int usbh_hub_probe(struct usbh_function *p_usb_fun){
    struct usbh_interface *p_intf = NULL;

    if (p_usb_fun == NULL) {
        return -USB_EINVAL;
    }

    /* 获取第一个接口*/
    p_intf = usbh_func_intf_get(p_usb_fun, p_usb_fun->first_intf_num, 0);
    if (p_intf == NULL) {
        return -USB_ENODEV;
    }

    /* 判断接口类，集线器的接口子类必须是 0 或 1 ，集线器必须只有一个端点*/
    if ((USBH_INTF_CLASS_GET(p_intf) != USB_CLASS_HUB) ||
            ((USBH_INTF_SUB_CLASS_GET(p_intf) > 1) ||
            (USBH_INTF_NEP_GET(p_intf) != 1))) {
        return -USB_ENOTSUP;
    }

    /* 端点必须是中断端点*/
    if ((USBH_EP_TYPE_GET(USBH_INTF_FIRST_EP_GET(p_intf)) != USB_EP_TYPE_INT) ||
            (USBH_EP_DIR_GET(USBH_INTF_FIRST_EP_GET(p_intf)) != USB_DIR_IN)) {
        return -USB_ENOTSUP;
    }

    p_usb_fun->func_type = USBH_FUNC_UHUB;

    return USB_OK;
}

/**
 * \brief 获取 USB 集线器设备名字
 *
 * \param[in] p_hub     USB 集线器设备结构体
 * \param[in] p_name    存储返回的名字缓存
 * \param[in] name_size 名字缓存大小
 *
 * \retval 成功获取返回 USB_OK
 */
int usbh_hub_name_get(struct usbh_hub *p_hub,
                      char            *p_name,
                      uint32_t         name_size){
    int ret = USB_OK;

    if ((p_hub == NULL) || (p_name == NULL)) {
        return -USB_EINVAL;
    }

    if ((strlen(p_hub->name) + 1) > name_size) {
        return -USB_EILLEGAL;
    }

    /* 检查是否允许操作*/
    if (p_hub->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_hub->p_lock, UHUB_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    strcpy(p_name, p_hub->name);

#if USB_OS_EN
    ret = usb_mutex_unlock(p_hub->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 设置 USB 集线器设备名字
 *
 * \param[in] p_hub     USB 集线器设备结构体
 * \param[in] p_name_new 要设置的新的名字
 *
 * \retval 成功设置返回 USB_OK
 */
int usbh_hub_name_set(struct usbh_hub *p_hub, char *p_name_new){
    int ret = USB_OK;

    if ((p_hub == NULL) || (p_name_new == NULL)) {
        return -USB_EINVAL;
    }

    /* 检查是否允许操作*/
    if (p_hub->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hub->p_lock, UHUB_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    memset(p_hub->name, 0, USB_NAME_LEN);
    strncpy(p_hub->name, p_name_new, (USB_NAME_LEN - 1));
#if USB_OS_EN
    ret = usb_mutex_unlock(p_hub->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 获取 USB 集线器状态
 *
 * \param[in]  p_hub USB 集线器结构体
 * \param[out] p_sta 返回获取到的集线器状态
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_sta_get(struct usbh_hub *p_hub, uint8_t *p_sta){
    int ret = USB_OK;

    if((p_hub == NULL) || (p_sta == NULL)){
        return -USB_EINVAL;
    }

    /* 检查是否允许操作*/
    if (p_hub->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hub->p_lock, UHUB_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_sta = p_hub->hub_basic.hub_status;

#if USB_OS_EN
    ret = usb_mutex_unlock(p_hub->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 获取 USB 集线器的端口数量
 *
 * \param[in] p_hub     USB 集线器
 * \param[in] p_n_ports 返回端口数量
 *
 * \retval 是集线器设备返回 USB_OK
 */
int usbh_hub_nport_get(struct usbh_hub *p_hub, uint8_t *p_n_ports){
    int ret = USB_OK;

    if ((p_hub == NULL) || (p_n_ports == NULL)) {
        return -USB_EINVAL;
    }
    if (p_hub->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_hub->p_lock, UHUB_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_n_ports = p_hub->hub_basic.n_ports;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_hub->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 检查 USB 集线器端口是否有设备连接
 *
 * \param[in] p_hub        USB 集线器
 * \param[in] port_num     端口号
 * \param[in] p_is_connect 返回连接状态
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_port_is_connect(struct usbh_hub *p_hub,
                             uint8_t          port_num,
                             usb_bool_t      *p_is_connect){
    int ret = USB_OK;

    if ((p_hub == NULL) || (p_is_connect == NULL)) {
        return -USB_EINVAL;
    }
    if ((port_num <= 0) || (port_num > p_hub->hub_basic.n_ports)) {
        return -USB_EILLEGAL;
    }
    if (p_hub->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hub->p_lock, UHUB_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_is_connect = __hub_port_is_connect(&p_hub->hub_basic, port_num);
#if USB_OS_EN
    ret = usb_mutex_unlock(p_hub->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 获取 USB 集线器端口状态
 *
 * \param[in]  p_hub    USB 集线器结构体
 * \param[in]  port_num 要获取状态的端口号
 * \param[out] p_sta    返回获取到的端口状态
 *
 * \retval 成功返回 USB_OK，状态 1 表示使能，0 表示禁能
 */
int usbh_hub_port_sta_get(struct usbh_hub *p_hub, int port_num, uint8_t *p_sta){
    int ret = USB_OK;

    if ((p_hub == NULL) || (p_sta == NULL)) {
        return -USB_EINVAL;
    }
    if ((port_num <= 0) || (port_num > p_hub->hub_basic.n_ports)) {
        return -USB_EILLEGAL;
    }
    if (p_hub->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_hub->p_lock, UHUB_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_sta = p_hub->hub_basic.p_port_sta[port_num - 1];

#if USB_OS_EN
    ret = usb_mutex_unlock(p_hub->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 创建一个 USB 集线器设备
 *
 * \param[in] p_usb_fun USB 功能结构体
 * \param[in] p_name    要创建的集线器的名字
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_create(struct usbh_function *p_usb_fun, char *p_name){
    int              ret, ret_tmp;
    struct usbh_hub *p_hub  = NULL;

    /* 检查 USB 库是否正常*/
    if ((usb_lib_is_init(&__g_uhub_lib.lib) == USB_FALSE) ||
            (__g_uhub_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    if ((p_usb_fun == NULL) || (p_name == NULL)) {
        return -USB_EINVAL;
    }

    /* 功能类型未确定，检查一下是否是 USB 集线器设备*/
    if (p_usb_fun->func_type != USBH_FUNC_UHUB) {
        if (p_usb_fun->func_type == USBH_FUNC_UNKNOWN) {
            ret = usbh_hub_probe(p_usb_fun);
        }
        if (ret != USB_OK) {
            __USB_ERR_INFO("usb function is not hub\r\n");
            return ret;
        }
    }

    ret = usbh_dev_ref_get(p_usb_fun->p_usb_dev);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
        goto __failed1;
    }

    p_hub = usb_lib_malloc(&__g_uhub_lib.lib, sizeof(struct usbh_hub));
    if (p_hub == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_hub, 0, sizeof(struct usbh_hub));

    /* 初始化集线器*/
    ret = __hub_init(p_hub, p_usb_fun, p_name);
    if (ret != USB_OK) {
        goto __failed1;
    }

    /* 往库中添加一个设备*/
    ret = usb_lib_dev_add(&__g_uhub_lib.lib, &p_hub->node);
    if (ret != USB_OK) {
        goto __failed1;
    }

    ret = usb_refcnt_get(&__g_uhub_lib.ref_cnt);
    if (ret != USB_OK) {
        goto __failed2;
    }
    /* 设置 USB 设备类型*/
    usbh_dev_type_set(p_usb_fun->p_usb_dev, p_usb_fun->func_type);
    /* 设置 USB 功能驱动私有数据*/
    usbh_func_drvdata_set(p_usb_fun, p_hub);

    return USB_OK;
__failed2:
    usb_lib_dev_del(&__g_uhub_lib.lib, &p_hub->node);
__failed1:
    if (p_hub) {
        __hub_deinit(p_hub);

        usb_lib_mfree(&__g_uhub_lib.lib, p_hub);
    }

    ret_tmp = usbh_dev_ref_put(p_usb_fun->p_usb_dev);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret_tmp);
    }
    return ret;
}

/**
 * \brief 销毁 USB 集线器
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_destroy(struct usbh_function *p_usb_fun){
    int              ret;
    struct usbh_hub *p_hub = NULL;

    if (p_usb_fun == NULL) {
        return -USB_EINVAL;
    }
    if (p_usb_fun->func_type != USBH_FUNC_UHUB) {
        return -USB_EILLEGAL;
    }

    ret = usbh_func_drvdata_get(p_usb_fun, (void **)&p_hub);
    if (ret != USB_OK) {
        return ret;
    } else if (p_hub == NULL) {
        return -USB_ENODEV;
    }
    p_hub->is_removed = USB_TRUE;

    return __hub_ref_put(p_hub);
}

/**
 * \brief 复位 USB 集线器设备遍历
 */
void usbh_hub_traverse_reset(void){
    usb_lib_dev_traverse_reset(&__g_uhub_lib.lib);
}

/**
 * \brief USB 集线器设备遍历
 *
 * \param[out] p_name    返回的名字缓存
 * \param[in]  name_size 名字缓存的大小
 * \param[out] p_vid     返回的设备 VID
 * \param[out] p_pid     返回的设备 PID
 *
 * \retval 成功返回 USB_OK，到末尾返回 -USB_END
 */
int usbh_hub_traverse(char     *p_name,
                      uint32_t  name_size,
                      uint16_t *p_vid,
                      uint16_t *p_pid){
    int                   ret;
    struct usb_list_node *p_node = NULL;
    struct usbh_hub      *p_hub  = NULL;

    ret = usb_lib_dev_traverse(&__g_uhub_lib.lib, &p_node);
    if (ret == USB_OK) {
        p_hub = usb_container_of(p_node, struct usbh_hub, node);
        if (p_name != NULL) {
            strncpy(p_name, p_hub->name, name_size);
        }
        if (p_vid != NULL) {
            *p_vid = USBH_HUB_DEV_VID_GET(p_hub);
        }
        if (p_pid != NULL) {
            *p_pid = USBH_HUB_DEV_PID_GET(p_hub);
        }
    }
    return ret;
}

/**
 * \brief 打开一个 USB 集线器
 *
 * \param[in]  name  设备名字
 * \param[in]  flag  打开标志，本接口支持三种打开方式：
 *                   USBH_DEV_OPEN_BY_NAME 是通过名字打开设备
 *                   USBH_DEV_OPEN_BY_FUN 是通过 USB 功能结构体打开设备
 * \param[out] p_hub 返回打开的 USB 集线器结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_open(void *p_handle, uint8_t flag, struct usbh_hub **p_hub){
    int              ret;
    struct usbh_hub *p_hub_tmp = NULL;

    if (p_handle == NULL) {
        return -USB_EINVAL;
    }
    if ((flag != USBH_DEV_OPEN_BY_NAME) &&
            (flag != USBH_DEV_OPEN_BY_UFUN)) {
        return -USB_EILLEGAL;
    }
    if ((usb_lib_is_init(&__g_uhub_lib.lib) == USB_FALSE) ||
            (__g_uhub_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    if (flag == USBH_DEV_OPEN_BY_NAME) {
        struct usb_list_node *p_node     = NULL;
        struct usb_list_node *p_node_tmp = NULL;
        char                 *p_name     = (char *)p_handle;

#if USB_OS_EN
        ret = usb_lib_mutex_lock(&__g_uhub_lib.lib);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
        USB_LIB_LIST_FOR_EACH_NODE(p_node, p_node_tmp, &__g_uhub_lib.lib){
            p_hub_tmp = usb_container_of(p_node, struct usbh_hub, node);
            if (strcmp(p_hub_tmp->name, p_name) == 0) {
                break;
            }
            p_hub_tmp = NULL;
        }
#if USB_OS_EN
        ret = usb_lib_mutex_unlock(&__g_uhub_lib.lib);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
    } else if (flag == USBH_DEV_OPEN_BY_UFUN) {
        struct usbh_function *p_usb_fun = (struct usbh_function *)p_handle;

        if (p_usb_fun->func_type != USBH_FUNC_UHUB) {
            return -USB_EPROTO;
        }

        usbh_func_drvdata_get(p_usb_fun, (void **)&p_hub_tmp);
    }

    if (p_hub_tmp) {
        ret = __hub_ref_get(p_hub_tmp);
        if (ret != USB_OK) {
            return ret;
        }
        *p_hub = p_hub_tmp;

        return USB_OK;
    }
    return -USB_ENODEV;
}

/**
 * \brief 关闭 USB 集线器
 *
 * \param[in] p_hub USB 集线器结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_close(struct usbh_hub *p_hub){
    if (p_hub == NULL) {
        return -USB_EINVAL;
    }
    return __hub_ref_put(p_hub);
}

/**
 * \brief 使能 USB 集线器端口
 *
 * \param[in] p_hub    集线器结构体
 * \param[in] port_num 要使能的端口号
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_port_enable(struct usbh_hub *p_hub, int port_num){
    int ret;
#if USB_OS_EN
    int ret_tmp;
#endif

    if (p_hub == NULL) {
        return -USB_EINVAL;
    }
    if ((port_num <= 0) || (port_num > p_hub->hub_basic.n_ports)) {
        return -USB_EILLEGAL;
    }
    if (p_hub->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_hub->p_lock, UHUB_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_hub->hub_basic.p_port_sta[port_num - 1] == 0) {
        ret = __HUB_PORT_FEATURE_SET(p_hub, HUB_PORT_FEAT_POWER, port_num);
        if (ret != USB_OK) {
            __USB_ERR_INFO("hub port power up failed(%d)\r\n", ret);
            goto __exit;
        }
        p_hub->hub_basic.p_port_sta[port_num - 1] = 1;
    }
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hub->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 禁能 USB 集线器端口
 *
 * \param[in] p_hub    USB 集线器结构体
 * \param[in] port_num 要禁能的端口号
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_port_disable(struct usbh_hub *p_hub, int port_num){
    int ret;
#if USB_OS_EN
    int ret_tmp;
#endif

    if (p_hub == NULL) {
        return -USB_EINVAL;
    }
    if ((port_num <= 0) || (port_num > p_hub->hub_basic.n_ports)) {
        return -USB_EILLEGAL;
    }
    if (p_hub->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_hub->p_lock, UHUB_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_hub->hub_basic.p_port_sta[port_num - 1] == 1) {
        ret = usbh_basic_hub_dev_disconnect(&p_hub->hub_basic, port_num);
        if (ret != USB_OK) {
            __USB_ERR_INFO("hub port device disconnect failed(%d)\r\n", ret);
            goto __exit;
        }

        /* 端口断电*/
        ret = __HUB_PORT_FEATURE_CLR(p_hub, HUB_PORT_FEAT_POWER, port_num);
        if (ret != USB_OK) {
            __USB_ERR_INFO("hub port power down failed(%d)\r\n", ret);
            goto __exit;
        }
        p_hub->hub_basic.p_port_sta[port_num - 1] = 0;
    }
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hub->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 集线器端口复位
 *
 * \param[in] p_hub    USB 集线器结构体
 * \param[in] port_num 要复位的端口号
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_port_reset(struct usbh_hub *p_hub, int port_num){
    int ret;
#if USB_OS_EN
    int ret_tmp;
#endif

    if (p_hub == NULL) {
        return -USB_EINVAL;
    }
    if ((port_num <= 0) || (port_num > p_hub->hub_basic.n_ports)) {
        return -USB_EILLEGAL;
    }
    if (p_hub->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_hub->p_lock, UHUB_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    ret = __hub_port_reset(&p_hub->hub_basic, USB_TRUE, port_num);
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hub->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 启动 USB 集线器
 *
 * \param[in] p_hub USB 集线器结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_start(struct usbh_hub *p_hub){
    int ret = USB_OK;
#if USB_OS_EN
    int ret_tmp;
#endif

    if (p_hub == NULL) {
        return -USB_EINVAL;
    }
    if (p_hub->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_hub->p_lock, UHUB_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_hub->hub_basic.hub_status == USBH_HUB_RUNNING) {
        goto __exit;
    }

    /* 初始化集线器的传输请求包*/
    __hub_trp_init(p_hub);
    /* 提交传输请求包*/
    ret = usbh_trp_submit(&p_hub->trp);
    if (ret == USB_OK) {
        p_hub->hub_basic.hub_status = USBH_HUB_RUNNING;
    }

__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hub->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 停止 USB 集线器
 *
 * \param[in] p_hub USB 集线器结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_stop(struct usbh_hub *p_hub){
    int ret = USB_OK;
#if USB_OS_EN
    int ret_tmp;
#endif

    if (p_hub == NULL) {
        return -USB_EINVAL;
    }
    if (p_hub->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_hub->p_lock, UHUB_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_hub->hub_basic.hub_status == USBH_HUB_STOP) {
        goto __exit;
    }

    ret = usbh_trp_xfer_cancel(&p_hub->trp);
    if (ret == USB_OK) {
        p_hub->hub_basic.hub_status = USBH_HUB_STOP;
    }
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_hub->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 获取当前存在的 USB 集线器设备数量
 *
 * \param[in] p_n_dev 存储返回的设备数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_lib_ndev_get(uint8_t *p_n_dev){
    if (__g_uhub_lib.is_lib_deiniting == USB_TRUE) {
        return -USB_ENOINIT;
    }
    return usb_lib_ndev_get(&__g_uhub_lib.lib, p_n_dev);
}

/**
 * \brief 初始化 USB 集线器设备库
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_lib_init(void){
    if (usb_lib_is_init(&__g_uhub_lib.lib) == USB_TRUE) {
        return USB_OK;
    }

#if USB_MEM_RECORD_EN
    int ret = usb_lib_init(&__g_uhub_lib.lib, "uhub_mem");
#else
    int ret = usb_lib_init(&__g_uhub_lib.lib);
#endif
    if (ret != USB_OK) {
        return ret;
    }
    /* 初始化引用计数*/
    usb_refcnt_init(&__g_uhub_lib.ref_cnt);
    /* 初始化 USB 集线器事件链表*/
    usb_list_head_init(&__g_uhub_lib.evt_list);

    __g_uhub_lib.is_lib_deiniting = USB_FALSE;
    __g_uhub_lib.xfer_time_out    = UHUB_XFER_TIMEOUT;

    return USB_OK;
}

/**
 * \brief USB 集线器库释放函数
 */
static void __uhub_lib_release(int *p_ref){
    usb_lib_deinit(&__g_uhub_lib.lib);
}

/**
 * \brief 反初始化 USB 集线器设备库
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_lib_deinit(void){
    /* 检查 USB 库是否正常*/
    if (usb_lib_is_init(&__g_uhub_lib.lib) == USB_FALSE) {
        return USB_OK;
    }
    __g_uhub_lib.is_lib_deiniting = USB_TRUE;

    return usb_refcnt_put(&__g_uhub_lib.ref_cnt, __uhub_lib_release);
}

#if USB_MEM_RECORD_EN
/**
 * \brief 获取 USB 集线器驱动内存记录
 *
 * \param[out] p_mem_record 返回内存记录
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hub_lib_mem_record_get(struct usb_mem_record *p_mem_record){
    return usb_lib_mem_record_get(&__g_uhub_lib.lib, p_mem_record);
}
#endif
