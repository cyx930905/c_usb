/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "config/usbh_serial_cfg_info.h"
#include "core/include/host/class/cdc/serial/usbh_cdc_serial_drv.h"
#include "core/include/host/class/cdc/serial/usbh_cdc_serial_chip.h"

/*******************************************************************************
 * Static
 ******************************************************************************/
/* \brief 声明一个 USB 主机转设备库结构体*/
static struct usbh_serial_lib __g_userial_lib;

static const struct usbh_serial_drv_info __g_userial_drv_info[] = {
        {
                .p_drv_name  = "FTDI",
                .p_fn_init   = usbh_serial_ftdi_init,
                .p_fn_deinit = usbh_serial_ftdi_deinit,
        },
        {
                .p_drv_name  = "CH341",
                .p_fn_init   = usbh_serial_ch341_init,
                .p_fn_deinit = usbh_serial_ch341_deinit,
        },
        {
                .p_drv_name  = "CH348",
                .p_fn_init   = usbh_serial_ch348_init,
                .p_fn_deinit = usbh_serial_ch348_deinit,
        },
        {
                .p_drv_name  = "PL2303",
                .p_fn_init   = usbh_serial_pl2303_init,
                .p_fn_deinit = usbh_serial_pl2303_deinit,
        },
        {
                .p_drv_name  = "CP210X",
                .p_fn_init   = usbh_serial_cp210x_init,
                .p_fn_deinit = usbh_serial_cp210x_deinit,
        },
        {
                .p_drv_name  = "Common",
                .p_fn_init   = usbh_serial_common_init,
                .p_fn_deinit = usbh_serial_common_deinit,
        },
};

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief USB 主机转串口设备库释放函数
 */
static void __userial_lib_release(int *p_ref){
    usb_lib_deinit(&__g_userial_lib.lib);
}

/**
 * \brief USB 转串口设备芯片初始化
 */
static int __serial_chip_init(struct usbh_serial *p_userial){
    int                          ret        = -USB_EPERM;
    struct usbh_serial_drv_info *p_drv_info = NULL;

    usbh_func_drvdata_get(p_userial->p_usb_fun, (void **)&p_drv_info);
    if (p_drv_info != NULL) {
        p_userial->p_drv_info = p_drv_info;
        ret = p_drv_info->p_fn_init(p_userial);
        if (ret == USB_OK) {
            __USB_INFO("USB host serial \"%s\" init success\r\n", p_drv_info->p_drv_name);
        } else {
            __USB_ERR_INFO("USB host serial \"%s\" init failed(%d)\r\n", p_drv_info->p_drv_name, ret);
        }
    }
    return ret;
}

/**
 * \brief USB 主机转串口设备芯片反初始化
 */
static int __serial_chip_deinit(struct usbh_serial *p_userial){
    if (p_userial->p_drv_info) {
        return p_userial->p_drv_info->p_fn_deinit(p_userial);
    }
    return USB_OK;
}

/**
 * \brief 初始化 USB 主机转串口设备
 */
static int __serial_init(struct usbh_serial  *p_userial,
                         char                 *p_name,
                         struct usbh_function *p_usb_fun){
    p_userial->p_usb_fun = p_usb_fun;

    strncpy(p_userial->name, p_name, (USB_NAME_LEN - 1));

    usb_refcnt_init(&p_userial->ref_cnt);
#if USB_OS_EN
    p_userial->p_lock = usb_lib_mutex_create(&__g_userial_lib.lib);
    if (p_userial->p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        return -USB_EPERM;
    }
#endif
    return USB_OK;
}

/**
 * \brief USB 主机转串口设备反初始化
 */
static int __serial_deinit(struct usbh_serial *p_userial){
    int ret = USB_OK;

#if USB_OS_EN
    if (p_userial->p_lock) {
        ret = usb_lib_mutex_destroy(&__g_userial_lib.lib, p_userial->p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
        }
    }
#endif
    return ret;
}

/**
 * \brief USB 主机转串口管道缓存大小获取
 */
static int __serial_pipe_buf_size_get(struct usbh_serial_pipe *p_pipe, uint32_t *p_buf_size){
    int ret = USB_OK;

#if USB_OS_EN
    ret = usb_mutex_lock(p_pipe->p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_buf_size = p_pipe->buf_size;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_pipe->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机转串口管道缓存大小设置
 */
static int __serial_pipe_buf_size_set(struct usbh_serial_pipe *p_pipe, uint32_t buf_size){
    int ret;
#if USB_OS_EN
    int ret_tmp;
#endif

    if (p_pipe->buf_size == buf_size) {
        return USB_OK;
    }

    if (buf_size < USBH_EP_MPS_GET(p_pipe->p_ep)) {
        buf_size = USBH_EP_MPS_GET(p_pipe->p_ep);
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_pipe->p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_pipe->p_buf) {
        usb_lib_mfree(&__g_userial_lib.lib, p_pipe->p_buf);
    }
    p_pipe->buf_size = 0;

    /* 为管道输入缓存申请内存空间*/
    p_pipe->p_buf = (uint8_t *)usb_lib_malloc(&__g_userial_lib.lib, buf_size);
    if (p_pipe->p_buf == NULL) {
        ret = -USB_ENOMEM;
        goto __exit;
    }
    memset(p_pipe->p_buf, 0, buf_size);

    p_pipe->buf_size = buf_size;
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_pipe->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 主机转串口设备释放函数
 */
static void __drv_release(usb_refcnt *p_ref){
    struct usbh_serial *p_userial = NULL;
    int                 ret;

    p_userial = USB_CONTAINER_OF(p_ref, struct usbh_serial, ref_cnt);

    /* 删除节点*/
    ret = usb_lib_dev_del(&__g_userial_lib.lib, &p_userial->node);
    if (ret != USB_OK) {
        return;
    }

    ret = usb_refcnt_put(&__g_userial_lib.ref_cnt, __userial_lib_release);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
        return;
    }

    ret = __serial_chip_deinit(p_userial);
    if (ret != USB_OK) {
        return;
    }

    ret = __serial_deinit(p_userial);
    if (ret != USB_OK) {
        return;
    }

    usb_lib_mfree(&__g_userial_lib.lib, p_userial);
    /* 释放 USB 设备*/
    ret = usbh_dev_ref_put(p_userial->p_usb_fun->p_usb_dev);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
    }
}

/**
 * \brief USB 主机转串口设备的引用增加
 */
static int __serial_ref_get(struct usbh_serial *p_userial){
    return usb_refcnt_get(&p_userial->ref_cnt);
}

/**
 * \brief USB 主机转串口设备引用减少
 */
static int __serial_ref_put(struct usbh_serial *p_userial){
    return usb_refcnt_put(&p_userial->ref_cnt, __drv_release);
}

/**
 * \brief USB 主机转串口内存分配函数
 */
void *usbh_serial_mem_alloc(uint32_t size){
    return usb_lib_malloc(&__g_userial_lib.lib, size);
}

/**
 * \brief USB 主机转串口内存释放函数
 */
void usbh_serial_mem_free(void *p_mem){
    usb_lib_mfree(&__g_userial_lib.lib, p_mem);
}

/**
 * \brief USB 主机转串口设备端口操作函数集设置
 *
 * \param[in] p_userial USB 转串口设备
 * \param[in] p_opts    要设置的操作函数集
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_opts_set(struct usbh_serial           *p_userial,
                              struct usbh_serial_port_opts *p_opts){
    int ret = USB_OK;

    if (p_userial == NULL) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_userial->p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_userial->p_opts = p_opts;

#if USB_OS_EN
    ret = usb_mutex_unlock(p_userial->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机转串口设备的端口数量获取
 *
 * \param[in]  p_userial USB 转串口设备
 * \param[out] p_n_ports 返回的端口数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_nport_get(struct usbh_serial *p_userial, uint8_t *p_n_ports){
    int ret = USB_OK;

    if ((p_userial == NULL) || (p_n_ports == NULL)) {
        return -USB_EINVAL;
    }
    if (p_userial->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_userial->p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    *p_n_ports = p_userial->n_ports;

#if USB_OS_EN
    ret = usb_mutex_unlock(p_userial->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机转串口设备名字获取
 *
 * \param[in]  p_userial USB 转串口设备结构体
 * \param[out] p_name    返回的名字缓存
 * \param[in]  name_size 名字缓存大小
 *
 * \retval 成功获取返回 USB_OK
 */
int usbh_serial_name_get(struct usbh_serial *p_userial, char *p_name, uint32_t name_size){
    int ret = USB_OK;

    if ((p_userial == NULL) || (p_name == NULL)) {
        return -USB_EINVAL;
    }
    if ((strlen(p_userial->name) + 1) > name_size) {
        return -USB_EILLEGAL;
    }
    /* 检查是否允许操作*/
    if (p_userial->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_userial->p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    strcpy(p_name, p_userial->name);
#if USB_OS_EN
    ret = usb_mutex_unlock(p_userial->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机转串口设备名字设置
 *
 * \param[in] p_userial  USB 转串口设备结构体
 * \param[in] p_name_new 要设置的新的名字
 *
 * \retval 成功设置返回 USB_OK
 */
int usbh_serial_name_set(struct usbh_serial *p_userial, char *p_name_new){
    int ret = USB_OK;

    if ((p_userial == NULL) || (p_name_new == NULL)) {
        return -USB_EINVAL;
    }
    /* 检查是否允许操作*/
    if (p_userial->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_userial->p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    memset(p_userial->name, 0, USB_NAME_LEN);
    strncpy(p_userial->name, p_name_new, (USB_NAME_LEN - 1));

#if USB_OS_EN
    ret = usb_mutex_unlock(p_userial->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机转串口设备探测函数
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_probe(struct usbh_function *p_usb_fun){
    const struct usbh_serial_dev_match_info *p_info = NULL;
    int                                      ret;
    uint32_t                                 i;
    uint32_t                                 num = USB_NELEMENTS(__g_userial_drv_info);

    for (p_info = __g_userial_dev_tab; (p_info->info.flags != 0); p_info++) {
        ret = usb_dev_func_match(p_usb_fun, &p_info->info);
        if (ret == USB_OK) {
            for (i = 0; i < num; i++) {
                if (strcmp(p_info->p_drv_name, __g_userial_drv_info[i].p_drv_name) == 0) {
                    usbh_func_drvdata_set(p_usb_fun, (void *)&__g_userial_drv_info[i]);
                    break;
                }
            }
            if (i == num) {
                usbh_func_drvdata_set(p_usb_fun, (void *)&__g_userial_drv_info[num - 1]);
            }
            p_usb_fun->func_type = USBH_FUNC_USERIAL;
            return USB_OK;
        }
    }
    return -USB_ENOTSUP;
}

/**
 * \brief USB 主机转串口设备创建
 *
 * \param[in] p_usb_fun USB 接口功能结构体
 * \param[in] p_name    USB 转串口设备名字
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_create(struct usbh_function *p_usb_fun, char *p_name){
    struct usbh_serial *p_userial    = NULL;
    int                 ret_tmp, ret = -USB_ENOTSUP;

    /* 检查 USB 库是否正常*/
    if ((usb_lib_is_init(&__g_userial_lib.lib) == USB_FALSE) ||
            (__g_userial_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }
    if ((p_usb_fun == NULL) || (p_name == NULL)) {
        return -USB_EINVAL;
    }

    /* 功能类型未确定，检查一下是否是 USB 转串口设备*/
    if (p_usb_fun->func_type != USBH_FUNC_USERIAL) {
        if (p_usb_fun->func_type == USBH_FUNC_UNKNOWN) {
            ret = usbh_serial_probe(p_usb_fun);
        }
        if (ret != USB_OK) {
            __USB_ERR_INFO("USB host function is not serial\r\n");
            return ret;
        }
    }

    /* 引用 USB 设备*/
    ret = usbh_dev_ref_get(p_usb_fun->p_usb_dev);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
        return ret;
    }

    p_userial = usb_lib_malloc(&__g_userial_lib.lib, sizeof(struct usbh_serial));
    if (p_userial == NULL) {
        ret = -USB_ENOMEM;
        goto __failed1;
    }
    memset(p_userial, 0, sizeof(struct usbh_serial));

    ret = __serial_init(p_userial, p_name, p_usb_fun);
    if (ret != USB_OK) {
        goto __failed1;
    }
    /* 具体芯片初始化*/
    ret = __serial_chip_init(p_userial);
    if (ret != USB_OK) {
        goto __failed2;
    }

    /* 往库中添加一个设备*/
    ret = usb_lib_dev_add(&__g_userial_lib.lib, &p_userial->node);
    if (ret != USB_OK) {
        goto __failed2;
    }

    ret = usb_refcnt_get(&__g_userial_lib.ref_cnt);
    if (ret != USB_OK) {
        goto __failed3;
    }

    /* 设置 USB 设备类型*/
    usbh_dev_type_set(p_usb_fun->p_usb_dev, p_usb_fun->func_type);

    /* 设置 USB 功能驱动私有数据*/
    usbh_func_drvdata_set(p_usb_fun, p_userial);

    return USB_OK;
__failed3:
    usb_lib_dev_del(&__g_userial_lib.lib, &p_userial->node);

__failed2:
    __serial_chip_deinit(p_userial);

__failed1:
#if USB_OS_EN
    if (p_userial->p_lock) {
        ret_tmp = usb_lib_mutex_destroy(&__g_userial_lib.lib, p_userial->p_lock);
        if (ret_tmp != USB_OK) {
            __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret_tmp);
        }
    }
#endif
    if (p_userial) {
        usb_lib_mfree(&__g_userial_lib.lib, p_userial);
    }

    ret_tmp = usbh_dev_ref_put(p_usb_fun->p_usb_dev);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret_tmp);
    }

    return ret;
}

/**
 * \brief USB 主机转串口设备销毁
 *
 * \param[in] p_usb_fun 要销毁的 USB 转串口功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_destroy(struct usbh_function *p_usb_fun){
    int                 ret;
    struct usbh_serial *p_userial = NULL;

    if (p_usb_fun == NULL) {
        return -USB_EINVAL;
    }
    if (p_usb_fun->func_type != USBH_FUNC_USERIAL) {
        return -USB_EILLEGAL;
    }

    ret = usbh_func_drvdata_get(p_usb_fun, (void **)&p_userial);
    if (ret != USB_OK) {
        return ret;
    } else if (p_userial == NULL) {
        return -USB_ENODEV;
    }

    /* 设置移除标志*/
    p_userial->is_removed = USB_TRUE;

    return __serial_ref_put(p_userial);
}

/**
 * \brief USB 主机转串口设备端口创建
 *
 * \param[in] p_userial USB 转串口设备
 * \param[in] n_ports   端口数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_ports_create(struct usbh_serial *p_userial, uint8_t n_ports){
    int      ret = USB_OK;
    uint32_t size;
#if USB_OS_EN
    int ret_tmp;
#endif

    if ((p_userial == NULL) || (n_ports == 0)) {
        return -USB_EINVAL;
    }
    if ((p_userial->p_ports != 0) || (p_userial->p_ports != NULL)) {
        return -USB_EILLEGAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_userial->p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    size = sizeof(struct usbh_serial_port) * n_ports;

    p_userial->p_ports = usb_lib_malloc(&__g_userial_lib.lib, size);
    if (p_userial->p_ports == NULL) {
        ret = -USB_ENOMEM;
        goto __exit;
    }
    memset(p_userial->p_ports, 0, size);

    p_userial->n_ports = n_ports;
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_userial->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 主机转串口设备端口销毁
 *
 * \param[in] p_userial USB 转串口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_ports_destroy(struct usbh_serial *p_userial){
    int ret = USB_OK;

    if (p_userial == NULL) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_userial->p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_userial->p_ports) {
        usb_lib_mfree(&__g_userial_lib.lib, p_userial->p_ports);
    }
    p_userial->n_ports = 0;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_userial->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif

    return ret;
}

/**
 * \brief USB 主机转串口设备端口初始化
 *
 * \param[in] p_userial    USB 转串口设备
 * \param[in] idx          端口索引
 * \param[in] p_ep_tx      发送端点
 * \param[in] tx_buf_size  发送天缓存大小
 * \param[in] tx_time_out  发送超时时间
 * \param[in] p_ep_rx      接收端点
 * \param[in] rx_buf_size  接收缓存大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_init(struct usbh_serial   *p_userial,
                          uint8_t               idx,
                          struct usbh_endpoint *p_ep_tx,
                          uint32_t              tx_buf_size,
                          int                   tx_time_out,
                          struct usbh_endpoint *p_ep_rx,
                          uint32_t              rx_buf_size){
    int                      ret    = USB_OK;
#if USB_OS_EN
    int                      ret_tmp;
#endif
    struct usbh_serial_port *p_port = NULL;

    if ((p_userial == NULL) ||
            (p_ep_tx == NULL) ||
            (p_ep_rx == NULL) ||
            (tx_buf_size == 0)) {
        return -USB_EINVAL;
    }
    if ((idx > (p_userial->n_ports - 1)) || (p_userial->p_ports == NULL)) {
        return -USB_EILLEGAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_userial->p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_port = &p_userial->p_ports[idx];

    if ((p_port->rx_pipe.pipe.p_buf != NULL) || (p_port->tx_pipe.pipe.p_buf != NULL)) {
        ret = -USB_EILLEGAL;
        goto __exit;
    }

    p_port->rx_pipe.pipe.p_buf = usb_lib_malloc(&__g_userial_lib.lib, rx_buf_size);
    if (p_port->rx_pipe.pipe.p_buf == NULL) {
        ret = -USB_ENOMEM;
        goto __exit;
    }
    memset(p_port->rx_pipe.pipe.p_buf, 0, rx_buf_size);

    p_port->tx_pipe.pipe.p_buf = usb_lib_malloc(&__g_userial_lib.lib, tx_buf_size);
    if (p_port->tx_pipe.pipe.p_buf == NULL) {
        ret = -USB_ENOMEM;
        goto __exit;
    }
    memset(p_port->tx_pipe.pipe.p_buf, 0, tx_buf_size);

#if USB_OS_EN
    p_port->rx_pipe.pipe.p_lock = usb_lib_mutex_create(&__g_userial_lib.lib);
    if (p_port->rx_pipe.pipe.p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        ret = -USB_EPERM;
        goto __exit;
    }
    p_port->tx_pipe.pipe.p_lock = usb_lib_mutex_create(&__g_userial_lib.lib);
    if (p_port->tx_pipe.pipe.p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        ret = -USB_EPERM;
        goto __exit;
    }
#endif

    p_port->idx                   = idx;
    p_port->p_userial             = p_userial;
    p_port->rx_pipe.pipe.p_ep     = p_ep_rx;
    p_port->rx_pipe.pipe.buf_size = rx_buf_size;
    p_port->tx_pipe.pipe.p_ep     = p_ep_tx;
    p_port->tx_pipe.pipe.buf_size = tx_buf_size;
    p_port->tx_pipe.time_out      = tx_time_out;
    p_port->is_init               = USB_TRUE;

__exit:
    if (ret != USB_OK) {
        if (p_port->tx_pipe.pipe.p_buf) {
            usb_lib_mfree(&__g_userial_lib.lib, p_port->tx_pipe.pipe.p_buf);
        }
        if (p_port->rx_pipe.pipe.p_buf) {
            usb_lib_mfree(&__g_userial_lib.lib, p_port->rx_pipe.pipe.p_buf);
        }
#if USB_OS_EN
        if (p_port->tx_pipe.pipe.p_lock) {
            ret_tmp = usb_lib_mutex_destroy(&__g_userial_lib.lib, p_port->tx_pipe.pipe.p_lock);
            if (ret_tmp != USB_OK) {
                __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret_tmp);
            }
        }
        if (p_port->rx_pipe.pipe.p_lock) {
            ret_tmp = usb_lib_mutex_destroy(&__g_userial_lib.lib, p_port->rx_pipe.pipe.p_lock);
            if (ret_tmp != USB_OK) {
                __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret_tmp);
            }
        }
#endif
    }
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_userial->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 主机转串口设备端口反初始化
 *
 * \param[in] p_userial    USB 转串口设备
 * \param[in] idx          端口索引
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_deinit(struct usbh_serial *p_userial,
                            uint8_t             idx){
#if USB_OS_EN
    int                      ret;
#endif
    struct usbh_serial_port *p_port = NULL;

    if (p_userial == NULL) {
        return -USB_EINVAL;
    }
    if ((idx > (p_userial->n_ports - 1)) || (p_userial->p_ports == NULL)) {
        return -USB_EILLEGAL;
    }
    p_port = &p_userial->p_ports[idx];

    if (p_port->tx_pipe.pipe.p_buf) {
        usb_lib_mfree(&__g_userial_lib.lib, p_port->tx_pipe.pipe.p_buf);
    }
    if (p_port->rx_pipe.pipe.p_buf) {
        usb_lib_mfree(&__g_userial_lib.lib, p_port->rx_pipe.pipe.p_buf);
    }
#if USB_OS_EN
    if (p_port->tx_pipe.pipe.p_lock) {
        ret = usb_lib_mutex_destroy(&__g_userial_lib.lib, p_port->tx_pipe.pipe.p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
            return ret;
        }
    }
    if (p_port->rx_pipe.pipe.p_lock) {
        ret = usb_lib_mutex_destroy(&__g_userial_lib.lib, p_port->rx_pipe.pipe.p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
            return ret;
        }
    }
#endif
    memset(p_port, 0, sizeof(struct usbh_serial_port));

    return USB_OK;
}

/**
 * \brief USB 主机转串口设备遍历复位
 */
void usbh_serial_traverse_reset(void){
    usb_lib_dev_traverse_reset(&__g_userial_lib.lib);
}

/**
 * \brief USB 主机转串口设备遍历
 *
 * \param[out] p_name    返回的设备名字
 * \param[in]  name_size 名字缓存的大小
 * \param[out] p_vid     返回的 VID，
 * \param[out] p_pid     返回的 PID，
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_traverse(char     *p_name,
                         uint32_t  name_size,
                         uint16_t *p_vid,
                         uint16_t *p_pid){
    int                   ret;
    struct usb_list_node *p_node    = NULL;
    struct usbh_serial   *p_userial = NULL;

    ret = usb_lib_dev_traverse(&__g_userial_lib.lib, &p_node);
    if (ret == USB_OK) {
        p_userial = usb_container_of(p_node, struct usbh_serial, node);
        if (p_name != NULL) {
            strncpy(p_name, p_userial->name, name_size);
        }
        if (p_vid != NULL) {
            *p_vid = USBH_SERIAL_DEV_VID_GET(p_userial);
        }
        if (p_pid != NULL) {
            *p_pid = USBH_SERIAL_DEV_PID_GET(p_userial);
        }
    }
    return ret;
}

/**
 * \brief USB 主机转串口设备打开函数
 *
 * \param[in]  p_handle      打开句柄
 * \param[in]  flag          打开标志，本接口支持两种打开方式：
 *                           USBH_DEV_OPEN_BY_NAME 是通过名字打开设备
 *                           USBH_DEV_OPEN_BY_UFUN 是通过 USB 功能结构体打开设备
 * \param[out] p_userial_ret 成功返回 USB 转串口结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_open(void                *p_handle,
                     uint8_t              flag,
                     struct usbh_serial **p_userial_ret){
    int                 ret;
    struct usbh_serial *p_userial = NULL;

    if (p_handle == NULL) {
        return -USB_EINVAL;
    }
    if ((flag != USBH_DEV_OPEN_BY_NAME) &&
            (flag != USBH_DEV_OPEN_BY_UFUN)) {
        return -USB_EILLEGAL;
    }
    if ((usb_lib_is_init(&__g_userial_lib.lib) == USB_FALSE) ||
            (__g_userial_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    if (flag == USBH_DEV_OPEN_BY_NAME) {
        struct usb_list_node *p_node     = NULL;
        struct usb_list_node *p_node_tmp = NULL;
        char                 *p_name     = (char *)p_handle;

#if USB_OS_EN
        ret = usb_lib_mutex_lock(&__g_userial_lib.lib);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
        USB_LIB_LIST_FOR_EACH_NODE(p_node, p_node_tmp, &__g_userial_lib.lib){
            p_userial = usb_container_of(p_node, struct usbh_serial, node);
            if (strcmp(p_userial->name, p_name) == 0) {
                break;
            }
            p_userial = NULL;
        }
#if USB_OS_EN
        ret = usb_lib_mutex_unlock(&__g_userial_lib.lib);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
    } else if (flag == USBH_DEV_OPEN_BY_UFUN) {
        struct usbh_function *p_usb_fun = (struct usbh_function *)p_handle;

        if (p_usb_fun->func_type != USBH_FUNC_USERIAL) {
            return -USB_EPROTO;
        }

        usbh_func_drvdata_get(p_usb_fun, (void **)&p_userial);
    }

    if (p_userial) {
        ret = __serial_ref_get(p_userial);
        if (ret != USB_OK) {
            return ret;
        }
        *p_userial_ret = p_userial;

        return USB_OK;
    }
    return -USB_ENODEV;
}

/**
 * \brief USB 主机转串口设备关闭函数
 *
 * \param[in] p_userial 相关的 USB 转串口设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_close(struct usbh_serial *p_userial){
    if (p_userial == NULL) {
        return -USB_EINVAL;
    }

    return __serial_ref_put(p_userial);
}

/**
 * \brief USB 主机转串口设备端口打开函数
 *
 * \param[in]  p_userial  USB 转串口设备
 * \param[in]  idx        要打开的端口索引
 * \param[out] p_port_ret 成功返回 USB 转串口端口结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_open(struct usbh_serial       *p_userial,
                          uint8_t                   idx,
                          struct usbh_serial_port **p_port_ret){
    int                      ret    = USB_OK;
#if USB_OS_EN
    int                      ret_tmp;
#endif
    struct usbh_serial_port *p_port = NULL;

    if (p_userial == NULL) {
        return -USB_EINVAL;
    }
    if (idx > (p_userial->n_ports - 1)) {
        return -USB_ENODEV;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_userial->p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_port = &p_userial->p_ports[idx];
    if (p_port->is_init == USB_FALSE) {
        ret = -USB_ENOINIT;
    } else {
        ret = __serial_ref_get(p_userial);
        if (ret == USB_OK) {
            *p_port_ret = p_port;
        }
    }
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_userial->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif

    return ret;
}

/**
 * \brief USB 主机转串口设备端口关闭函数
 *
 * \param[in] p_port 要关闭的 USB 转串口设备端口
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_close(struct usbh_serial_port *p_port){
    if (p_port == NULL) {
        return -USB_EINVAL;
    }

    return __serial_ref_put(p_port->p_userial);
}

/**
 * \brief USB 主机转串口设备端口接收缓存放入数据
 *
 * \param[in]  p_port    USB 转串口设备端口
 * \param[in]  p_buf     数据缓存
 * \param[in]  buf_size  数据缓存大小
 * \param[out] p_act_len 返回实际放入的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_rx_buf_put(struct usbh_serial_port *p_port,
                                uint8_t                 *p_buf,
                                uint32_t                 buf_len,
                                uint32_t                *p_act_len){
    int      ret = USB_OK;
    uint32_t len;

    if ((p_port == NULL) || (p_buf == NULL) || (p_act_len == NULL)) {
        return -USB_EINVAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_port->rx_pipe.pipe.p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_port->rx_pipe.rd_pos > p_port->rx_pipe.data_pos) {
        if (buf_len > (p_port->rx_pipe.rd_pos - p_port->rx_pipe.data_pos - 1)) {
            len = p_port->rx_pipe.rd_pos - p_port->rx_pipe.data_pos;
        } else {
            len = buf_len;
        }
        memcpy(p_port->rx_pipe.pipe.p_buf + p_port->rx_pipe.data_pos, p_buf, len);

        *p_act_len = len;
    } else {
        /* 计算可以放入数据的总大小*/
        len = p_port->rx_pipe.pipe.buf_size + p_port->rx_pipe.rd_pos - p_port->rx_pipe.data_pos - 1;

        if (len > buf_len) {
            len = buf_len;
        }
        /* 回环*/
        if ((len + p_port->rx_pipe.data_pos) > p_port->rx_pipe.pipe.buf_size) {
            len = p_port->rx_pipe.pipe.buf_size - p_port->rx_pipe.data_pos;
        }
        memcpy(p_port->rx_pipe.pipe.p_buf + p_port->rx_pipe.data_pos, p_buf, len);
        *p_act_len = len;

        len = buf_len - *p_act_len;

        if (p_port->rx_pipe.rd_pos != 0) {
            if (len > (p_port->rx_pipe.rd_pos - 1)) {
                len = p_port->rx_pipe.rd_pos - 1;
            }
            memcpy(p_port->rx_pipe.pipe.p_buf, p_buf + *p_act_len, len);
            *p_act_len += len;
        }
    }
    /* 重新设置写标记*/
    p_port->rx_pipe.data_pos = (p_port->rx_pipe.data_pos + *p_act_len);
    /* 到了环形末尾，回到环形头*/
    if (p_port->rx_pipe.data_pos >= p_port->rx_pipe.pipe.buf_size) {
        p_port->rx_pipe.data_pos -= p_port->rx_pipe.pipe.buf_size;
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_port->rx_pipe.pipe.p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif

    return ret;
}

/**
 * \brief USB 主机转串口设备端口接收缓存获取数据
 *
 * \param[in]  p_port    USB 转串口设备端口
 * \param[in]  p_buf     缓存
 * \param[in]  buf_len   要获取的长度
 * \param[out] p_act_len 实际获取到的长度
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_rx_buf_get(struct usbh_serial_port *p_port,
                                uint8_t                 *p_buf,
                                uint32_t                 buf_len,
                                uint32_t                *p_act_len){
    int      ret = USB_OK;
    uint32_t len;

    if ((p_port == NULL) || (p_buf == NULL) || (p_act_len == NULL)) {
        return -USB_EINVAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_port->rx_pipe.pipe.p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_port->rx_pipe.rd_pos <= p_port->rx_pipe.data_pos) {
        len = p_port->rx_pipe.data_pos - p_port->rx_pipe.rd_pos;

        if (len > buf_len) {
            len = buf_len;
        }
        memcpy(p_buf, p_port->rx_pipe.pipe.p_buf + p_port->rx_pipe.rd_pos, len);
        *p_act_len = len;
    } else {
        len = p_port->rx_pipe.pipe.buf_size - p_port->rx_pipe.rd_pos;

        if (len > buf_len) {
            len = buf_len;
        }
        memcpy(p_buf, p_port->rx_pipe.pipe.p_buf + p_port->rx_pipe.rd_pos, len);
        *p_act_len = len;

        len = p_port->rx_pipe.data_pos;

        if (len > (buf_len - *p_act_len)) {
            len = buf_len - *p_act_len;
        }
        if (len > 0) {
            memcpy(p_buf + *p_act_len, p_port->rx_pipe.pipe.p_buf, len);
        }
        *p_act_len += len;
    }
    p_port->rx_pipe.rd_pos = p_port->rx_pipe.rd_pos + *p_act_len;
    if (p_port->rx_pipe.rd_pos >= p_port->rx_pipe.pipe.buf_size) {
        p_port->rx_pipe.rd_pos -= p_port->rx_pipe.pipe.buf_size;
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_port->rx_pipe.pipe.p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif

    return ret;
}

/**
 * \brief USB 主机转串口设备端口发送数据
 *
 * \param[in]  p_port    USB 转串口设备端口
 * \param[in]  p_buf     要写的数据的缓存
 * \param[in]  buf_size  要写的数据缓存的长度
 * \param[out] p_act_len 返回实际写的长度
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_write(struct usbh_serial_port *p_port,
                           uint8_t                 *p_buf,
                           uint32_t                 buf_len,
                           uint32_t                *p_act_len){
    int ret = USB_OK;
#if USB_OS_EN
    int ret_tmp;
#endif

    if ((p_port == NULL) ||
            (p_buf == NULL) ||
            (buf_len == 0) ||
            (p_act_len == NULL)) {
        return -USB_EINVAL;
    }
    if (p_port->p_userial->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_port->tx_pipe.pipe.p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if ((p_port->p_userial->p_opts != NULL) &&
            (p_port->p_userial->p_opts->p_fn_port_write != NULL)) {
        ret = p_port->p_userial->p_opts->p_fn_port_write(p_port, p_buf, buf_len, p_act_len);
    } else {
        ret = -USB_ENOTSUP;
    }
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_port->tx_pipe.pipe.p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 主机转串口设备端口读数据
 *
 * \param[in]  p_port    USB 转串口设备端口
 * \param[in]  p_buf     要读的数据的缓存
 * \param[in]  buf_len   要读的数据的长度
 * \param[out] p_act_len 返回实际读的长度
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_read(struct usbh_serial_port *p_port,
                          uint8_t                 *p_buf,
                          uint32_t                 buf_len,
                          uint32_t                *p_act_len){
    if ((p_port == NULL) ||
            (p_buf == NULL) ||
            (buf_len == 0) ||
            (p_act_len == NULL)) {
        return -USB_EINVAL;
    }
    if (p_port->p_userial->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

    return usbh_serial_port_rx_buf_get(p_port, p_buf, buf_len, p_act_len);
}

/**
 * \brief USB 主机转串口设备端口配置获取
 *
 * \param[in]  p_port USB 转串口设备端口
 * \param[out] p_cfg  返回读到的配置
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_cfg_get(struct usbh_serial_port    *p_port,
                             struct usb_serial_port_cfg *p_cfg){
    int ret = USB_OK;

    if ((p_port == NULL) || (p_cfg == NULL)) {
        return -USB_EINVAL;
    }
    if (p_port->p_userial->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_port->p_userial->p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    memcpy(p_cfg, &p_port->cfg, sizeof(struct usb_serial_port_cfg));

#if USB_OS_EN
    ret = usb_mutex_unlock(p_port->p_userial->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机转串口端口配置设置
 *
 * \param[in] p_port USB 转串口设备端口
 * \param[in] p_cfg  要设置的配置
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_cfg_set(struct usbh_serial_port    *p_port,
                             struct usb_serial_port_cfg *p_cfg){
    int ret = USB_OK;
#if USB_OS_EN
    int ret_tmp;
#endif

    if ((p_port == NULL) || (p_cfg == NULL)) {
        return -USB_EINVAL;
    }
    if (p_port->p_userial->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_port->p_userial->p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    if ((p_port->p_userial->p_opts != NULL) && (p_port->p_userial->p_opts->p_fn_port_cfg_set != NULL)) {
        ret = p_port->p_userial->p_opts->p_fn_port_cfg_set(p_port, p_cfg);
    } else {
        ret = -USB_ENOTSUP;
    }
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_port->p_userial->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 主机转串口设备端口管道缓存大小获取
 *
 * \param[in]  p_port     USB 转串口设备端口结构体
 * \param[in]  pipe_dir   管道方向
 * \param[out] p_buf_size 要返回的缓存大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_buf_size_get(struct usbh_serial_port *p_port,
                                  uint8_t                  pipe_dir,
                                  uint32_t                *p_buf_size){
    struct usbh_serial_pipe *p_pipe = NULL;

    if ((p_port == NULL) || (p_buf_size == NULL)) {
        return -USB_EINVAL;
    }
    if (p_port->p_userial->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

    if (pipe_dir == USBH_SERIAL_PIPE_RX) {
        p_pipe = &p_port->rx_pipe.pipe;
    } else if (pipe_dir == USBH_SERIAL_PIPE_TX) {
        p_pipe = &p_port->tx_pipe.pipe;
    } else {
        return -USB_EPERM;
    }

    return __serial_pipe_buf_size_get(p_pipe, p_buf_size);
}

/**
 * \brief USB 主机转串口设备端口管道缓存大小设置
 *
 * \param[in] p_port   USB 转串口端口结构体
 * \param[in] pipe_dir 管道方向
 * \param[in] buf_size 缓存大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_buf_size_set(struct usbh_serial_port *p_port,
                                  uint8_t                  pipe_dir,
                                  uint32_t                 buf_size){
    int                      ret    = -USB_EPERM;
    struct usbh_serial_pipe *p_pipe = NULL;

    if ((p_port == NULL) || (buf_size == 0)) {
        return -USB_EINVAL;
    }
    if (p_port->p_userial->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

    buf_size = USB_ROUND_UP(buf_size, 4);

    if (pipe_dir & USBH_SERIAL_PIPE_RX) {
        p_pipe = &p_port->rx_pipe.pipe;
        ret = __serial_pipe_buf_size_set(p_pipe, buf_size);
        if (ret != USB_OK) {
            return ret;
        }
    }
    if (pipe_dir & USBH_SERIAL_PIPE_TX) {
        p_pipe = &p_port->tx_pipe.pipe;
        ret = __serial_pipe_buf_size_set(p_pipe, buf_size);

    }
    return ret;
}

/**
 * \brief USB 主机转串口设备端口发送超时时间获取
 *
 * \param[in]  p_port     USB 转串口设备端口结构体
 * \param[out] p_time_out 返回的超时时间
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_tx_timeout_get(struct usbh_serial_port *p_port,
                                    int                     *p_time_out){
    int                         ret       = USB_OK;
    struct usbh_serial_tx_pipe *p_tx_pipe = NULL;

    if ((p_port == NULL) || (p_time_out == NULL)) {
        return -USB_EINVAL;
    }
    if (p_port->p_userial->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

    p_tx_pipe = &p_port->tx_pipe;
#if USB_OS_EN
    ret = usb_mutex_lock(p_tx_pipe->pipe.p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_time_out = p_tx_pipe->time_out;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_tx_pipe->pipe.p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机转串口设备端口超时时间设置
 *
 * \param[in] p_port   USB 转串口设备端口结构体
 * \param[in] time_out 要设置的超时时间
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_port_tx_timeout_set(struct usbh_serial_port *p_port,
                                    int                      time_out){
    int                         ret       = USB_OK;
    struct usbh_serial_tx_pipe *p_tx_pipe = NULL;

    if ((p_port == NULL) || (time_out < 100)) {
        return -USB_EINVAL;
    }
    if (p_port->p_userial->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

    p_tx_pipe = &p_port->tx_pipe;
#if USB_OS_EN
    ret = usb_mutex_lock(p_tx_pipe->pipe.p_lock, USERIAL_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_tx_pipe->time_out = time_out;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_tx_pipe->pipe.p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 主机转串口设备数量获取
 *
 * \param[in] p_n_dev 存储返回的设备数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_lib_ndev_get(uint8_t *p_n_dev){
    if (__g_userial_lib.is_lib_deiniting == USB_TRUE) {
        return -USB_ENOINIT;
    }
    return usb_lib_ndev_get(&__g_userial_lib.lib, p_n_dev);
}

/**
 * \brief USB 主机转串口设备库相关初始化
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_lib_init(void){
    if (usb_lib_is_init(&__g_userial_lib.lib) == USB_TRUE) {
        return USB_OK;
    }

#if USB_MEM_RECORD_EN
    int ret = usb_lib_init(&__g_userial_lib.lib, "userial_mem");
#else
    int ret = usb_lib_init(&__g_userial_lib.lib);
#endif
    if (ret != USB_OK) {
        return ret;
    }
    /* 初始化引用计数*/
    usb_refcnt_init(&__g_userial_lib.ref_cnt);

    __g_userial_lib.is_lib_deiniting = USB_FALSE;
    return USB_OK;
}

/**
 * \brief USB 主机转串口设备库反初始化
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_lib_deinit(void){
    /* 检查 USB 库是否正常*/
    if (usb_lib_is_init(&__g_userial_lib.lib) == USB_FALSE) {
        return USB_OK;
    }
    __g_userial_lib.is_lib_deiniting = USB_TRUE;

    return usb_refcnt_put(&__g_userial_lib.ref_cnt, __userial_lib_release);
}

#if USB_MEM_RECORD_EN
/**
 * \brief USB 主机转串口驱动内存记录获取
 *
 * \param[out] p_mem_record 内存记录
 *
 * \retval 成功返回 USB_OK
 */
int usbh_serial_lib_mem_record_get(struct usb_mem_record *p_mem_record){
    return usb_lib_mem_record_get(&__g_userial_lib.lib, p_mem_record);
}
#endif


