/*******************************************************************************
 *                                    ZLG
 *                         ----------------------------
 *                         innovating embedded platform
 *
 * Copyright (c) 2001-present Guangzhou ZHIYUAN Electronics Co., Ltd.
 * All rights reserved.
 *
 * Contact information:
 * web site:    https://www.zlg.cn
 *******************************************************************************/
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/device/class/ms/usbd_ms.h"

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbd_core_lib __g_usb_device_lib;
extern int usb_dc_ep_halt_set(struct usb_dc *p_dc, struct usbd_ep *p_ep, usb_bool_t is_halt);

/*******************************************************************************
 * Static
 ******************************************************************************/
/* \brief 数据接口功能输出管道信息定义 */
static struct usbd_pipe_info __g_ms_data_pipe_out_info = {
        .num   = 0,
        .mps   = 64, //512,
        .dir   = USB_DIR_OUT,
        .attr  = USB_EP_TYPE_BULK,
};

/* \brief 数据接口功能输入管道信息定义 */
static struct usbd_pipe_info __g_ms_data_pipe_in_info = {
        .num   = 0,
        .mps   = 64, //512,
        .dir   = USB_DIR_IN,
        .attr  = USB_EP_TYPE_BULK,
};

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 关闭接口功能
 */
static int __ms_shutdown(struct usbd_func *p_func, void *p_arg){
    struct usbd_ms *p_ms = (struct usbd_ms *)p_arg;

    p_ms->is_setup    = USB_FALSE;
    p_ms->p_data_in   = NULL;
    p_ms->p_data_out  = NULL;
    p_ms->p_lu_select = NULL;

    return USB_OK;
}

/**
 * \brief 获取最大逻辑单元号完成回调函数
 */
static void __ms_max_lun_get_done(void *p_arg){
    struct usbd_ms *p_ms = (struct usbd_ms *)p_arg;

    p_ms->is_max_lun_get = USB_TRUE;
}

/**
 * \brief USB 大容量存储设备设置函数
 */
static int __ms_cmd_setup(struct usbd_func   *p_func,
                          struct usb_ctrlreq *p_setup,
                          void               *p_arg){
    struct usbd_ms *p_ms = (struct usbd_ms *)p_arg;

    if ((p_setup->request_type & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS) {
        switch (p_setup->request) {
            case USB_BBB_REQ_RESET:
                __USB_INFO("USB ms reset\r\n");
                break;
            case USB_BBB_REQ_GET_MLUN:
                __USB_INFO("get max lun\r\n");

                if (p_ms->n_lu) {
                    *(uint8_t*)p_ms->p_buf = p_ms->n_lu - 1;
                } else {
                    *(uint8_t*)p_ms->p_buf = 0;
                }
                return usb_dc_ep0_req(USB_DC_GET_FROM_FUNC(p_func),
                                      USB_DIR_IN,
                                     (uint8_t *)p_ms->p_buf,
                                      1,
                                    __ms_max_lun_get_done,
                                      p_ms);
            default:
                return -USB_ENOTSUP;
        }
        return USB_OK;
    }

    return -USB_ENOTSUP;
}

/**
 * \brief USB 大容量存储设备接口功能设置函数
 */
static int __ms_data_func_set(struct usbd_func *p_func, void *p_arg){
    int             ret;
    struct usbd_ms *p_ms = NULL;

    p_ms = (struct usbd_ms *)p_arg;

    ret = usbd_dev_pipe_get(USBD_DEV_GET_FROM_FUNC(p_func),
                            p_ms->data_out_ep_addr & 0x0F,
                            USB_DIR_OUT,
                           &p_ms->p_data_out);
    if (ret != USB_OK) {
        return ret;
    }

    ret = usbd_dev_pipe_get(USBD_DEV_GET_FROM_FUNC(p_func),
                            p_ms->data_in_ep_addr & 0x0F,
                            USB_DIR_IN,
                           &p_ms->p_data_in);
    if (ret != USB_OK) {
        return ret;
    }
    p_ms->is_setup = USB_TRUE;

    return USB_OK;
}

/* \brief USB 设备大容量存储设备接口功能操作函数  */
static struct usbd_func_opts __g_ms_func_opts = {
    __ms_shutdown,
    __ms_cmd_setup,
    __ms_data_func_set,
    NULL,
    NULL,
    NULL
};

/**
 * \brief 创建一个 USB 从机大容量存储设备
 *
 * \param[in]  p_dc      所属的 USB 从机控制器
 * \param[in]  p_name    设备名字
 * \param[in]  dev_desc  设备描述符
 * \param[in]  func_info 接口功能信息
 * \param[in]  buf_size  传输缓存大小
 * \param[out] p_ms_ret  返回创建成功的 USB 从机大容量存储设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_create(struct usb_dc         *p_dc,
                   char                  *p_name,
                   struct usbd_dev_desc   dev_desc,
                   struct usbd_func_info  func_info,
                   uint32_t               buf_size,
                   struct usbd_ms       **p_ms_ret){
    int             ret;
#if USB_OS_EN
    int             ret_tmp;
#endif
    struct usbd_ms *p_ms = NULL;

    if ((p_name == NULL) ||
            (p_dc == NULL) ||
            (buf_size == 0)) {
        return -USB_EINVAL;
    }

    p_ms = usb_lib_malloc(&__g_usb_device_lib.lib, sizeof(struct usbd_ms));
    if (p_ms == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_ms, 0, sizeof(struct usbd_ms));

    p_ms->p_buf = usb_lib_malloc(&__g_usb_device_lib.lib, buf_size);
    if (p_ms->p_buf == NULL) {
        ret = -USB_ENOMEM;
        goto __failed1;
    }
    memset(p_ms->p_buf, 0, buf_size);

#if USB_OS_EN
    p_ms->p_lock = usb_lib_mutex_create(&__g_usb_device_lib.lib);
    if (p_ms->p_lock == NULL) {
        ret = -USB_EPERM;
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        goto __failed1;
    }
#endif
    usb_list_head_init(&p_ms->lu_list);
    p_ms->buf_size = buf_size;

    /* 创建一个 USB 从机设备*/
    ret = usbd_dev_create(p_dc, p_name, dev_desc, &p_ms->p_dc_dev);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device create failed(%d)\r\n", ret);
        goto __failed1;
    }

    /* 添加配置*/
    ret = usbd_dev_cfg_add(p_ms->p_dc_dev, NULL, &p_ms->cfg_num);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device config add failed(%d)\r\n", ret);
        goto __failed2;
    }
    /* 创建一个功能接口*/
    ret = usbd_dev_func_add(p_ms->p_dc_dev,
                            p_ms->cfg_num,
                            func_info,
                         &__g_ms_func_opts,
                            p_ms);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device function add failed(%d)\r\n", ret);
        goto __failed2;
    }
    /* 添加数据输入管道 */
    ret = usbd_dev_pipe_add(p_ms->p_dc_dev,
                            p_ms->cfg_num,
                            USBD_MS_INTF_NUM,
                            USBD_MS_INTF_ALT_NUM,
                          __g_ms_data_pipe_in_info,
                           &p_ms->data_in_ep_addr);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device pipe add failed(%d)\r\n", ret);
        goto __failed2;
    }
    /* 添加数据输出管道 */
    ret = usbd_dev_pipe_add(p_ms->p_dc_dev,
                            p_ms->cfg_num,
                            USBD_MS_INTF_NUM,
                            USBD_MS_INTF_ALT_NUM,
                          __g_ms_data_pipe_out_info,
                           &p_ms->data_out_ep_addr);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device pipe add failed(%d)\r\n", ret);
        goto __failed2;
    }

    *p_ms_ret = p_ms;

    return USB_OK;
__failed2:
    usbd_dev_destroy(p_ms->p_dc_dev);
__failed1:
    if (p_ms->p_buf) {
        usb_lib_mfree(&__g_usb_device_lib.lib, p_ms->p_buf);
    }
#if USB_OS_EN
    if (p_ms->p_lock) {
        ret_tmp= usb_lib_mutex_destroy(&__g_usb_device_lib.lib, p_ms->p_lock);
        if (ret_tmp != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret_tmp);
        }
    }
#endif
    usb_lib_mfree(&__g_usb_device_lib.lib, p_ms);
    return ret;
}

/**
 * \brief 销毁 USB 从机大容量存储设备
 *
 * \param[in] p_ms 要销毁的 USB 从机大容量存储设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_destroy(struct usbd_ms *p_ms){
    int                   ret;
    struct usb_list_node *p_node     = NULL;
    struct usb_list_node *p_node_tmp = NULL;
    struct usbd_ms_lu    *p_lu       = NULL;

    if (p_ms == NULL) {
        return USB_OK;
    }

    ret = usbd_dev_destroy(p_ms->p_dc_dev);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb device \"%s\" destroy failed(%d)\r\n", p_ms->p_dc_dev->name, ret);
        return ret;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_ms->p_lock, USBD_MS_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    usb_list_for_each_node_safe(p_node, p_node_tmp, &p_ms->lu_list){
        p_lu = usb_container_of(p_node, struct usbd_ms_lu, node);

        usb_list_node_del(&p_lu->node);
        usb_lib_mfree(&__g_usb_device_lib.lib, p_lu);
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_ms->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_ms->p_buf) {
        usb_lib_mfree(&__g_usb_device_lib.lib, p_ms->p_buf);
    }
#if USB_OS_EN
    if (p_ms->p_lock) {
        ret= usb_lib_mutex_destroy(&__g_usb_device_lib.lib, p_ms->p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
            return ret;
        }
    }
#endif
    usb_lib_mfree(&__g_usb_device_lib.lib, p_ms);

    return USB_OK;
}

/**
 * \brief 检查 USB 从机大容量存储设备是否设置
 *
 * \param[in] p_ms USB 从机大容量存储设备
 *
 * \retval 设置返回 USB_TRUE，否则返回 USB_FALSE
 */
usb_bool_t usbd_ms_is_setup(struct usbd_ms *p_ms){
    if (p_ms == NULL) {
        return USB_FALSE;
    }
    return p_ms->is_setup;
}

/**
 * \brief USB 从机大容量存储设备用户操作函数设置
 *
 * \param[in] p_ms USB 从机大容量存储设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_usr_opts_set(struct usbd_ms *p_ms, struct usbd_ms_opts *p_opts){
    int ret = USB_OK;

    if (p_ms == NULL) {
        return -USB_EINVAL;
    }
    if (p_ms->is_setup == USB_TRUE) {
        return -USB_EPERM;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_ms->p_lock, USBD_MS_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    p_ms->p_opts = p_opts;

#if USB_OS_EN
    ret = usb_mutex_unlock(p_ms->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 从机大容量存储设备添加逻辑单元
 *
 * \param[in]  p_ms       USB 从机大容量存储设备
 * \param[in]  blk_size   逻辑单元块大小
 * \param[in]  n_blks     逻辑单元块数量
 * \param[in]  p_usr_data 用户数据（可为空）
 * \param[out] p_num_ret  返回逻辑单元号
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_lu_add(struct usbd_ms *p_ms,
                   uint32_t        blk_size,
                   uint32_t        n_blks,
                   usb_bool_t      is_rdonly,
                   usb_bool_t      is_removed,
                   void           *p_usr_data,
                   uint8_t        *p_num_ret){
    int                ret  = USB_OK;
    struct usbd_ms_lu *p_lu = NULL;
#if USB_OS_EN
    int                ret_tmp;
#endif

    if ((p_ms == NULL) || (p_num_ret == NULL)) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_ms->p_lock, USBD_MS_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_lu = usb_lib_malloc(&__g_usb_device_lib.lib, sizeof(struct usbd_ms_lu));
    if (p_lu == NULL) {
        ret = -USB_ENOMEM;
        goto __exit;
    }
    memset(p_lu, 0, sizeof(struct usbd_ms_lu));

    p_lu->blk_size        = blk_size;
    p_lu->n_blks          = n_blks;
    p_lu->num             = (int)p_ms->n_lu;
    p_lu->lu.lu.is_rdonly = is_rdonly;
    p_lu->lu.lu.is_rm     = is_removed;
    p_lu->p_usr_data      = p_usr_data;
    *p_num_ret            = p_ms->n_lu;

    usb_list_node_add_tail(&p_lu->node, &p_ms->lu_list);

    p_ms->n_lu++;
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_ms->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif

    return ret;
}

/**
 * \brief USB 从机大容量存储设备删除逻辑单元信息
 *
 * \param[in] p_ms   USB 从机大容量存储设备
 * \param[in] lu_num 逻辑单元号
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_lu_del(struct usbd_ms *p_ms, uint8_t lu_num){
    int                   ret        = USB_OK;
#if USB_OS_EN
    int                   ret_tmp;
#endif
    struct usbd_ms_lu    *p_lu       = NULL;
    struct usb_list_node *p_node     = NULL;
    struct usb_list_node *p_node_tmp = NULL;

    if (p_ms == NULL) {
        return -USB_EINVAL;
    }

    if (lu_num >= p_ms->n_lu) {
        return -USB_EILLEGAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_ms->p_lock, USBD_MS_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_ms->p_lu_select->num == (int)lu_num) {
        ret = -USB_EBUSY;
        goto __exit;
    } else {

    }
    usb_list_for_each_node_safe(p_node, p_node_tmp, &p_ms->lu_list){
        p_lu = usb_container_of(p_node, struct usbd_ms_lu, node);

        if (p_lu->num == (int)lu_num) {
            usb_list_node_del(&p_lu->node);

            usb_lib_mfree(&__g_usb_device_lib.lib, p_lu);
        }
    }
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_ms->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 从大容量存储设备管道停止设置
 *
 * \param[in] p_ms    USB 从机大容量存储设备
 * \param[in] dir     要停止的管道方向
 * \param[in] is_set  是否停止
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_pipe_stall_set(struct usbd_ms *p_ms, uint8_t dir, usb_bool_t is_set){
    if (p_ms == NULL) {
        return -USB_EINVAL;
    }
    if ((p_ms->p_dc_dev == NULL) ||
            (p_ms->p_data_in == NULL) ||
            (p_ms->p_data_out == NULL)) {
        return -USB_EILLEGAL;
    }

    if (dir == USB_DIR_IN) {
        return usb_dc_ep_halt_set(p_ms->p_dc_dev->p_dc, p_ms->p_data_in->p_hw, is_set);
    }
    return usb_dc_ep_halt_set(p_ms->p_dc_dev->p_dc, p_ms->p_data_out->p_hw, is_set);
}

/**
 * \brief USB 大容量存储设备读数据
 *
 * \param[in]  p_ms      USB 从机大容量存储设备
 * \param[in]  len       要读的长度（不可超过 USB 从机大容量存储设备 buf_size）
 * \param[in]  timeout   读超时时间
 * \param[out] p_act_len 返回实际读的数据
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_read(struct usbd_ms *p_ms,
                 uint32_t        len,
                 int             timeout,
                 uint32_t       *p_act_len){
    if ((p_ms == NULL) || (p_act_len == NULL)) {
        return -USB_EINVAL;
    }
    if (len > p_ms->buf_size) {
        return -USB_EILLEGAL;
    }

    return usbd_dev_trans_sync(p_ms->p_dc_dev,
                               p_ms->p_data_out,
                               p_ms->p_buf,
                               len,
                               0,
                               timeout,
                               p_act_len);
}

/**
 * \brief USB 大容量存储设备写数据
 *
 * \param[in]  p_ms      USB 从机大容量存储设备
 * \param[in]  len       要写的长度（不可超过 USB 从机大容量存储设备 buf_size）
 * \param[in]  timeout   写超时时间
 * \param[out] p_act_len 返回实际写的数据
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_write(struct usbd_ms *p_ms,
                  uint32_t        len,
                  int             timeout,
                  uint32_t       *p_act_len){
    if ((p_ms == NULL) || (p_act_len == NULL)) {
        return -USB_EINVAL;
    }
    if (len > p_ms->buf_size) {
        return -USB_EILLEGAL;
    }

    return usbd_dev_trans_sync(p_ms->p_dc_dev,
                               p_ms->p_data_in,
                               p_ms->p_buf,
                               len,
                               0,
                               timeout,
                               p_act_len);
}

