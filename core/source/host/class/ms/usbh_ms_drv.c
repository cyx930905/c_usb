/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/ms/usbh_ms_drv.h"
#include "common/refcnt/usb_refcnt.h"
#include <string.h>
#include <stdio.h>

/*******************************************************************************
 * Macro operate
 ******************************************************************************/
#define __MS_CBI_ADSC             0x00

#define __MS_CBW_SIGNATURE        0x43425355
#define __MS_CSW_SIGNATURE        0x53425355

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern int usbh_ms_scsi_init(struct usbh_ms_lu *p_lu);
extern int usbh_ms_scsi_read(struct usbh_ms_lu *p_lu,
                             uint32_t           blk,
                             uint32_t           n_blks,
                             void              *p_buf);
extern int usbh_ms_scsi_write(struct usbh_ms_lu *p_lu,
                              uint32_t           blk,
                              uint32_t           n_blks,
                              void              *p_buf);

/*******************************************************************************
 * Statement
 ******************************************************************************/
/* \brief 声明一个 USB 大容量存储库结构体*/
static struct ums_lib __g_ums_lib;

/* \brief CBW(Command Block Wrapper 命令块包)*/
struct __bulk_only_cbw {
    uint32_t sig;
    uint32_t tag;
    uint32_t d_len;
    uint8_t  flags;
    uint8_t  lun;
    uint8_t  c_len;
    uint8_t  cb[16];
};

/* \brief CSW(Command Status Wrapper 命令状态包) */
struct __bulk_only_csw {
    uint32_t sig;
    uint32_t tag;
    uint32_t res;
    uint8_t  sta;
};

/* \brief USB 大容量存储设备类*/
static struct usbh_ms_sclass __g_sclass[] = {
    {
        USB_MS_SC_SCSI_TRANSPARENT,
        usbh_ms_scsi_init,
        usbh_ms_scsi_read,
        usbh_ms_scsi_write
    },
    {0}
};

/*******************************************************************************
 * Code
 ******************************************************************************/
static int __tag_get(struct usbh_ms *p_ms){
    p_ms->tag += 1;
    return p_ms->tag;
}

/**
 * \brief 块复位
 */
static int __bulk_reset(struct usbh_ms *p_ms){
    return usbh_ctrl_trp_sync_xfer(&p_ms->p_usb_fun->p_usb_dev->ep0,
                                    USB_DIR_OUT |
                                    USB_REQ_TYPE_CLASS |
                                    USB_REQ_TAG_INTERFACE,
                                    USB_BBB_REQ_RESET,
                                    0,
                                    p_ms->p_usb_fun->first_intf_num,
                                    0,
                                    NULL,
                                    5000,
                                    0);

}

/**
 * \brief 初始化大容量存储设备
 */
static int __ms_init(struct usbh_ms       *p_ms,
                     struct usbh_function *p_usb_fun,
                     char                 *p_name,
                     uint8_t               n_lu,
                     uint32_t              lu_buf_size){
    int                    i, ret;
    struct usbh_interface *p_intf = NULL;
    struct usb_hc         *p_hc   = NULL;

    /* 获取第一个接口*/
    p_intf = usbh_func_intf_get(p_usb_fun, p_usb_fun->first_intf_num, 0);
    if (p_intf == NULL) {
        return -USB_EAGAIN;
    }
    p_hc = p_usb_fun->p_usb_dev->p_hc;

    /* 设置设备名字*/
    strncpy(p_ms->name, p_name, (USB_NAME_LEN - 1));

    /* 初始化引用计数*/
    usb_refcnt_init(&p_ms->ref_cnt);
#if USB_OS_EN
    /* 创建互斥锁*/
    p_ms->p_lock = usb_lib_mutex_create(&__g_ums_lib.lib);
    if (p_ms->p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        return -USB_EPERM;
    }
#endif
    p_ms->tag        = 0;
    p_ms->is_removed = USB_FALSE;
    p_ms->p_usb_fun  = p_usb_fun;
    p_ms->p_lus      = (struct usbh_ms_lu *)(p_ms + 1);
    p_ms->pro        = p_intf->p_desc->interface_protocol;

    p_ms->p_cbw = usb_lib_malloc(&__g_ums_lib.lib, sizeof(struct __bulk_only_cbw));
    if (p_ms->p_cbw == NULL) {
        return -USB_EPERM;
    }
    memset(p_ms->p_cbw, 0, sizeof(struct __bulk_only_cbw));

    p_ms->p_csw = usb_lib_malloc(&__g_ums_lib.lib, sizeof(struct __bulk_only_csw));
    if (p_ms->p_csw == NULL) {
        return -USB_EPERM;
    }
    memset(p_ms->p_csw, 0, sizeof(struct __bulk_only_csw));

    /* 获取端点*/
    for (i = 0; i < p_intf->p_desc->num_endpoints; i++) {
        if ((USBH_EP_TYPE_GET(&p_intf->p_eps[i]) == USB_EP_TYPE_INT) &&
            (p_intf->p_desc->interface_protocol == USB_MS_PRO_CBI_CCI)) {
            if (p_ms->p_ep_intr == NULL) {
                p_ms->p_ep_intr = &p_intf->p_eps[i];
            }
        }

        if (USBH_EP_TYPE_GET(&p_intf->p_eps[i]) == USB_EP_TYPE_BULK) {
            if (USBH_EP_DIR_GET(&p_intf->p_eps[i]) == USB_DIR_IN) {
                if (p_ms->p_ep_in == NULL) {
                    p_ms->p_ep_in = &p_intf->p_eps[i];
                }
            } else {
                if (p_ms->p_ep_out == NULL) {
                    p_ms->p_ep_out = &p_intf->p_eps[i];
                }
            }
        }
    }

    /* 检查接口协议*/
    if (((p_intf->p_desc->interface_protocol == USB_MS_PRO_CBI_CCI) && (p_ms->p_ep_intr == NULL)) ||
            ((p_ms->p_ep_out == NULL) || (p_ms->p_ep_in == NULL))) {
        return -USB_ENOTSUP;
    }

    /* 检查接口子协议*/
    p_ms->p_sclass = __g_sclass;
    if (((p_ms->p_sclass->id != 0) && (p_ms->p_sclass->id != p_intf->p_desc->interface_sub_class)) ||
            (p_ms->p_sclass->id == 0)) {
        return -USB_EPERM;
    }

    /* 等待上电*/
    usb_mdelay(200);

    for (i = 0; i < n_lu; i++) {
        p_ms->p_lus[i].is_init    = USB_FALSE;
        p_ms->p_lus[i].p_ms       = p_ms;
        p_ms->p_lus[i].lun_num    = i;
        p_ms->p_lus[i].p_usr_priv = NULL;
        sprintf(p_ms->p_lus[i].name, "/dev/h%d-d%d-%d", p_hc->host_idx,
                                                        p_usb_fun->p_usb_dev->addr,
                                                        i);
        p_ms->p_lus[i].p_buf = usb_lib_malloc(&__g_ums_lib.lib, lu_buf_size);
        if (p_ms->p_lus[i].p_buf == NULL) {
            return -USB_ENOMEM;
        }

        memset(p_ms->p_lus[i].p_buf, 0, lu_buf_size);

        p_ms->p_lus[i].buf_size = lu_buf_size;

        /* 初始化逻辑单元*/
        if (p_ms->p_sclass->p_fn_init) {
            ret = p_ms->p_sclass->p_fn_init(&p_ms->p_lus[i]);
            if (ret != USB_OK) {
                usb_lib_mfree(&__g_ums_lib.lib, p_ms->p_lus[i].p_buf);
                p_ms->p_lus[i].p_buf = NULL;
            }
        }
        if (ret == USB_OK) {
            p_ms->p_lus[i].is_init = USB_TRUE;
            p_ms->n_lu++;
        }
    }
    return USB_OK;
}

/**
 * \brief 反初始化大容量存储设备
 */
static int __ms_deinit(struct usbh_ms *p_ms){
#if USB_OS_EN
    int     ret;
#endif
    uint8_t i;

#if USB_OS_EN
    if (p_ms->p_lock) {
        ret = usb_lib_mutex_destroy(&__g_ums_lib.lib, p_ms->p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
            return ret;
        }
    }
#endif
    if (p_ms->p_cbw) {
        usb_lib_mfree(&__g_ums_lib.lib, p_ms->p_cbw);
    }

    if (p_ms->p_csw) {
        usb_lib_mfree(&__g_ums_lib.lib, p_ms->p_csw);
    }
    /* 释放分区缓存和销毁块设备*/
    i = p_ms->n_lu;
    for (i = 0; i < p_ms->n_lu; i++) {
        if (p_ms->p_lus[i].p_buf) {
            usb_lib_mfree(&__g_ums_lib.lib, p_ms->p_lus[i].p_buf);
        }
    }
    return USB_OK;
}

/**
 * \brief 检查逻辑单元是否写保护
 *
 * \param[in] p_lu    逻辑单元结构域
 * \param[in] p_is_wp 写保护标志缓存
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_is_wp(struct usbh_ms_lu *p_lu, usb_bool_t *p_is_wp){
    int ret = USB_OK;

    if ((p_lu == NULL) || (p_is_wp == NULL)) {
        return -USB_EINVAL;
    }
    if (p_lu->is_init == USB_FALSE) {
        return -USB_ENOINIT;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_lu->p_ms->p_lock, UMS_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_is_wp = p_lu->is_wp;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_lu->p_ms->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 获取最大逻辑单元数量
 */
static int __ms_max_lun_get(struct usbh_function *p_usb_fun, uint8_t *p_nlus){
    struct usbh_interface *p_intf = NULL;
    uint8_t                data;
    int                    ret;

    /* 获取第一个接口*/
    p_intf = usbh_func_intf_get(p_usb_fun, p_usb_fun->first_intf_num, 0);
    if (p_intf == NULL) {
        return -USB_EPERM;
    }

    /* 获取逻辑单元数量*/
    ret = usbh_ctrl_trp_sync_xfer(&p_usb_fun->p_usb_dev->ep0,
                                   USB_DIR_IN |
                                   USB_REQ_TYPE_CLASS |
                                   USB_REQ_TAG_INTERFACE,
                                   USB_BBB_REQ_GET_MLUN,
                                   0,
                                   p_intf->p_desc->interface_number,
                                   1,
                                  &data,
                                   5000,
                                   0);
    if (ret < 1) {
        return ret;
    }

    *p_nlus = (data & 0x0F) + 1;

    return USB_OK;
}

/**
 * \brief 释放 USB 大容量存储设备
 */
static void __ms_release(usb_refcnt *p_ref){
    struct usbh_ms *p_ms = NULL;
    int             ret;

    p_ms = USB_CONTAINER_OF(p_ref, struct usbh_ms, ref_cnt);
    /* 删除节点*/
    ret = usb_lib_dev_del(&__g_ums_lib.lib, &p_ms->node);
    if (ret != USB_OK) {
        return;
    }
    /* 反初始化大容量存储设备*/
    __ms_deinit(p_ms);

    usb_lib_mfree(&__g_ums_lib.lib, p_ms);

    /* 释放对 USB 设备的引用*/
    ret = usbh_dev_ref_put(p_ms->p_usb_fun->p_usb_dev);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
    }
}

/**
 * \brief 增加 USB 大容量存储设备的引用
 */
static int __ms_ref_get(struct usbh_ms *p_ms){
    return usb_refcnt_get(&p_ms->ref_cnt);
}

/**
 * \brief 取消 USB 大容量存储设备引用
 */
static int __ms_ref_put(struct usbh_ms *p_ms){
    return usb_refcnt_put(&p_ms->ref_cnt, __ms_release);
}

/**
 * \brief 复位 USB 大容量存储设备遍历
 */
void usbh_ms_traverse_reset(void){
    usb_lib_dev_traverse_reset(&__g_ums_lib.lib);
}

/**
 * \brief USB 大容量存储设备遍历
 *
 * \param[out] p_name    返回的名字缓存
 * \param[in]  name_size 名字缓存的大小
 * \param[out] p_vid     存储设备 VID 的缓存
 * \param[out] p_pid     存储设备 PID 的缓存
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_traverse(char     *p_name,
                     uint32_t  name_size,
                     uint16_t *p_vid,
                     uint16_t *p_pid){
    int                   ret;
    struct usb_list_node *p_node = NULL;
    struct usbh_ms       *p_ms  = NULL;

    ret = usb_lib_dev_traverse(&__g_ums_lib.lib, &p_node);
    if (ret == USB_OK) {
        p_ms = usb_container_of(p_node, struct usbh_ms, node);
        if (p_name != NULL) {
            strncpy(p_name, p_ms->name, name_size);
        }
        if (p_vid != NULL) {
            *p_vid = USBH_MS_DEV_VID_GET(p_ms);
        }
        if (p_pid != NULL) {
            *p_pid = USBH_MS_DEV_PID_GET(p_ms);
        }
    }
    return ret;
}

/**
 * \brief USB 大容量块传输
 *
 * \param[in] p_ms     USB 大容量存储设备结构体
 * \param[in] lun_num  逻辑单元号
 * \param[in] p_cmd    相关命令
 * \param[in] cmd_len  命令长度
 * \param[in] p_data   数据缓存
 * \param[in] data_len 数据长度
 * \param[in] dir      端点方向
 *
 * \retval 成功返回发送/接收数据的长度
 */
static int __ms_bulk_transport(struct usbh_ms *p_ms,
                               uint8_t         lun_num,
                               void           *p_cmd,
                               uint8_t         cmd_len,
                               void           *p_data,
                               uint32_t        data_len,
                               uint8_t         dir){
    int                     ret;
    int                     ret_tmp;

    struct __bulk_only_cbw *p_cbw = NULL;
    struct __bulk_only_csw *p_csw = NULL;
    uint8_t                 retry;

#if USB_OS_EN
    ret = usb_mutex_lock(p_ms->p_lock, UMS_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    p_cbw = (struct __bulk_only_cbw *)p_ms->p_cbw;
    p_csw = (struct __bulk_only_csw *)p_ms->p_csw;

    /* 填充命令块包*/
    p_cbw->sig   = USB_CPU_TO_LE32(__MS_CBW_SIGNATURE);
    p_cbw->tag   = USB_CPU_TO_LE32(__tag_get(p_ms));
    p_cbw->d_len = USB_CPU_TO_LE32(data_len);
    p_cbw->flags = dir;
    p_cbw->lun   = lun_num & 0x0F;
    p_cbw->c_len = cmd_len;

    memset(p_cbw->cb, 0, 16);
    memcpy(p_cbw->cb, p_cmd, cmd_len);
    memset(p_csw, 0, sizeof(struct __bulk_only_csw));

    /* 发送命令，重试3次*/
    retry = 3;
    do {
        ret = usbh_trp_sync_xfer(p_ms->p_ep_out,
                                 NULL,
                                 p_cbw,
                                 31,
                                 5000,
                                 0);
        if ((ret == -USB_EPIPE) || (ret == -USB_ETIME)) {
            ret_tmp = __bulk_reset(p_ms);
            if (ret_tmp != USB_OK) {
                __USB_ERR_INFO("bulk reset failed(%d)\r\n", ret_tmp);
                goto __end;
            }

            ret_tmp = usbh_dev_ep_halt_clr(p_ms->p_ep_out);
            if (ret_tmp != USB_OK) {
                __USB_ERR_INFO("device endpoint halt clear failed(%d)\r\n", ret_tmp);
                goto __end;
            }
        }
    } while ((--retry) && (ret < 31) && usbh_dev_is_connect(p_ms->p_ep_out->p_usb_dev));

    if (ret < 31) {
        if (ret >= 0) {
            ret = -USB_EDATA;
        }
#if USB_OS_EN
        ret_tmp = usb_mutex_unlock(p_ms->p_lock);
        if (ret_tmp != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
            return ret_tmp;
        }
#endif
        return ret;
    }

    /* 发送/接收数据*/
    if (data_len) {
        struct usbh_endpoint *p_ep = NULL;

        if (dir == USB_DIR_IN) {
            p_ep = p_ms->p_ep_in;
        } else {
            p_ep = p_ms->p_ep_out;
        }
        /* 发送/接收数据，重试3次*/
        retry = 3;
        do {
            ret = usbh_trp_sync_xfer(p_ep,
                                     NULL,
                                     p_data,
                                     data_len,
                                     5000,
                                     0);
            if ((ret == -USB_EPIPE) || (ret == -USB_ETIME)) {

                ret_tmp = usbh_dev_ep_halt_clr(p_ms->p_ep_out);
                if (ret_tmp != USB_OK) {
                    __USB_ERR_INFO("device endpoint halt clear failed(%d)\r\n", ret_tmp);
                    goto __end;
                }
            }
        } while ((--retry) && (ret < 0) && usbh_dev_is_connect(p_ep->p_usb_dev));

        if (ret < 0) {
            if (ret == -USB_EPIPE) {
                usbh_trp_sync_xfer(p_ms->p_ep_in,
                                   NULL,
                                   p_csw,
                                   13,
                                   5000,
                                   0);
            } else {
                ret_tmp = __bulk_reset(p_ms);
                if (ret_tmp != USB_OK) {
                    __USB_ERR_INFO("bulk reset failed(%d)\r\n", ret_tmp);
                    goto __end;
                }
            }
#if USB_OS_EN
            ret_tmp = usb_mutex_unlock(p_ms->p_lock);
            if (ret_tmp != USB_OK) {
                __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
                return ret_tmp;
            }
#endif
            return ret;
        }
        data_len = ret;
    }

    /* 获取命令状态包，重试3次*/
    retry = 3;
    do {
        ret = usbh_trp_sync_xfer(p_ms->p_ep_in,
                                 NULL,
                                 p_csw,
                                 13,
                                 5000,
                                 0);
        if ((ret == -USB_EPIPE) ||
            (ret == -USB_ETIME)) {
            ret_tmp = usbh_dev_ep_halt_clr(p_ms->p_ep_in);
            if (ret_tmp != USB_OK) {
                __USB_ERR_INFO("device endpoint halt clear failed(%d)\r\n", ret_tmp);
                goto __end;
            }
        }
    } while ((--retry) && (ret < 13) && usbh_dev_is_connect(p_ms->p_ep_in->p_usb_dev));

    if (ret < 13) {
        if (ret >= 0) {
            ret = -USB_EDATA;
        }
#if USB_OS_EN
        ret_tmp = usb_mutex_unlock(p_ms->p_lock);
        if (ret_tmp != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
            return ret_tmp;
        }
#endif
        return ret;
    }

    /* 检查命令状态包是否正确*/
    if ((USB_CPU_TO_LE32(p_csw->sig) != __MS_CSW_SIGNATURE) ||
        (USB_CPU_TO_LE32(p_csw->tag) != p_csw->tag) ||
        (p_csw->sta > 0x01)) {
#if USB_OS_EN
        ret_tmp = usb_mutex_unlock(p_ms->p_lock);
        if (ret_tmp != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
            return ret_tmp;
        }
#endif
        return -USB_EDATA;
    }

    ret = data_len;
__end:
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
 * \brief USB 大容量存储设备传输函数
 *
 * \param[in] p_lun    逻辑单元结构体
 * \param[in] p_cmd    相关命令代码
 * \param[in] cmd_len  命令长度
 * \param[in] p_data   数据缓存
 * \param[in] data_len 数据长度
 * \param[in] dir      数据方向
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_transport(struct usbh_ms_lu *p_lu,
                      void              *p_cmd,
                      uint8_t            cmd_len,
                      void              *p_data,
                      uint32_t           data_len,
                      uint8_t            dir){

    /* 支持批量传输*/
    switch (p_lu->p_ms->pro) {
        case USB_MS_PRO_CBI_CCI:
            return -USB_ENOTSUP;

        case USB_MS_PRO_CBI_NCCI:
            return -USB_ENOTSUP;

        case USB_MS_PRO_BBB:
            return __ms_bulk_transport(p_lu->p_ms,
                                       p_lu->lun_num,
                                       p_cmd,
                                       cmd_len,
                                       p_data,
                                       data_len,
                                       dir);

        default:
            return -USB_ENOTSUP;
    }
}

/**
 * \brief USB 大容量存储设备块读函数
 *
 * \param[in] p_lu    逻辑单元结构体
 * \param[in] blk_num 起始块编号
 * \param[in] n_blks  块数量
 * \param[in] p_buf   数据缓存(为NULL则用逻辑分区的缓存)
 *
 * \retval 成功返回实际读写成功的字节数
 */
int usbh_ms_blk_read(struct usbh_ms_lu *p_lu,
                     uint32_t           blk_num,
                     uint32_t           n_blks,
                     void              *p_buf){
    int ret;

    if (p_lu == NULL) {
        return -USB_EINVAL;
    }
    if ((p_lu->p_ms == NULL) || (p_lu->p_ms->p_sclass == NULL) ||
            (p_lu->p_ms->p_sclass->p_fn_read == NULL)) {
        return -USB_EILLEGAL;
    }
    if (p_lu->p_ms->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

    if (p_buf == NULL) {
        ret = p_lu->p_ms->p_sclass->p_fn_read(p_lu,
                                              blk_num,
                                              n_blks,
                                              p_lu->p_buf);
    } else {
        ret = p_lu->p_ms->p_sclass->p_fn_read(p_lu,
                                              blk_num,
                                              n_blks,
                                              p_buf);
    }

    return ret;
}

/**
 * \brief USB 大容量存储设备块写函数
 *
 * \param[in] p_lu    逻辑单元结构体
 * \param[in] blk_num 起始块编号
 * \param[in] n_blks  块数量
 * \param[in] p_buf   数据缓存(为 NULL 则用逻辑分区的缓存)
 *
 * \retval 成功返回实际读写成功的字节数
 */
int usbh_ms_blk_write(struct usbh_ms_lu *p_lu,
                      uint32_t           blk_num,
                      uint32_t           n_blks,
                      void              *p_buf){
    int ret;

    if (p_lu == NULL) {
        return -USB_EINVAL;
    }
    if ((p_lu->p_ms == NULL) || (p_lu->p_ms->p_sclass == NULL) ||
            (p_lu->p_ms->p_sclass->p_fn_write == NULL)) {
        return -USB_EILLEGAL;
    }
    if (p_lu->p_ms->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

    if (p_buf == NULL) {
        ret = p_lu->p_ms->p_sclass->p_fn_write(p_lu,
                                               blk_num,
                                               n_blks,
                                               p_lu->p_buf);
    } else {
        ret = p_lu->p_ms->p_sclass->p_fn_write(p_lu,
                                               blk_num,
                                               n_blks,
                                               p_buf);
    }

    return ret;
}

/**
 * \brief 获取 USB 大容量存储设备支持的最大逻辑单元数量
 *
 * \param[in] p_ms    USB 大容量存储设备结构体
 * \param[in] p_nluns 存储最大逻辑单元数量的缓存
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_max_nlu_get(struct usbh_ms *p_ms, uint8_t *p_nlus){
    int ret = USB_OK;
#if USB_OS_EN
    int ret_tmp;
#endif
    if ((p_ms == NULL) || (p_nlus == NULL)) {
        return -USB_EINVAL;
    }
    if (p_ms->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_ms->p_lock, UMS_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    ret = __ms_max_lun_get(p_ms->p_usb_fun, p_nlus);
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
 * \brief 获取 USB 大容量存储设备当前的逻辑单元数量
 *
 * \param[in] p_ms    USB 大容量存储设备结构体
 * \param[in] p_nlus  存储逻辑单元数量的缓存
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_valid_nlu_get(struct usbh_ms *p_ms, uint8_t *p_nlus){
    int ret = USB_OK;

    if ((p_ms == NULL) || (p_nlus == NULL)) {
        return -USB_EINVAL;
    }
    if (p_ms->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_ms->p_lock, UMS_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_nlus = p_ms->n_lu;

#if USB_OS_EN
    ret = usb_mutex_unlock(p_ms->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 获取 USB 大容量存储设备名字
 *
 * \param[in]  p_ms      USB 大容量存储设备结构体
 * \param[out] p_name    存储名字的缓存
 * \param[in]  name_size 名字缓存大小
 *
 * \retval 成功获取返回 USB_OK
 */
int usbh_ms_name_get(struct usbh_ms *p_ms, char *p_name, uint32_t name_size){
    int ret = USB_OK;

    if ((p_ms == NULL) || (p_name == NULL)) {
        return -USB_EINVAL;
    }

    if ((strlen(p_ms->name) + 1) > name_size) {
        return -USB_EILLEGAL;
    }

    if (p_ms->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_ms->p_lock, UMS_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    strcpy(p_name, p_ms->name);

#if USB_OS_EN
    ret = usb_mutex_unlock(p_ms->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 设置 USB 大容量存储设备名字
 *
 * \param[in] p_ms       USB 大容量存储设备结构体
 * \param[in] p_name_new 要设置的新的名字
 *
 * \retval 成功设置返回 USB_OK
 */
int usbh_ms_name_set(struct usbh_ms *p_ms, char *p_name_new){
    int ret = USB_OK;

    if ((p_ms == NULL) || (p_name_new == NULL)) {
        return -USB_EINVAL;
    }
    if (p_ms->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_ms->p_lock, UMS_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    memset(p_ms->name, 0, USB_NAME_LEN);
    strncpy(p_ms->name, p_name_new, (USB_NAME_LEN - 1));

#if USB_OS_EN
    ret = usb_mutex_unlock(p_ms->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 打开一个 USB 大容量存储设备
 *
 * \param[in]  p_handle 打开句柄
 * \param[in]  flag     打开标志，本接口支持两种种打开方式：
 *                      USBH_DEV_OPEN_BY_NAME 是通过名字打开设备
 *                      USBH_DEV_OPEN_BY_UFUN 是通过 USB 功能结构体打开设备
 * \param[out] p_ms     返回获取到的 USB 大容量存储设备地址
 *
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_open(void *p_handle, uint8_t flag, struct usbh_ms **p_ms){
    int             ret;
    struct usbh_ms *p_ms_tmp = NULL;

    if (p_handle == NULL) {
        return -USB_EINVAL;
    }
    if ((flag != USBH_DEV_OPEN_BY_NAME) &&
            (flag != USBH_DEV_OPEN_BY_UFUN)) {
        return -USB_EILLEGAL;
    }
    if ((usb_lib_is_init(&__g_ums_lib.lib) == USB_FALSE) ||
            (__g_ums_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    if (flag == USBH_DEV_OPEN_BY_NAME) {
        struct usb_list_node *p_node     = NULL;
        struct usb_list_node *p_node_tmp = NULL;
        char                 *p_name     = (char *)p_handle;

#if USB_OS_EN
        ret = usb_lib_mutex_lock(&__g_ums_lib.lib);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
        USB_LIB_LIST_FOR_EACH_NODE(p_node, p_node_tmp, &__g_ums_lib.lib){
        	p_ms_tmp = usb_container_of(p_node, struct usbh_ms, node);
            if (strcmp(p_ms_tmp->name, p_name) == 0) {
                break;
            }
            p_ms_tmp = NULL;
        }
#if USB_OS_EN
        ret = usb_lib_mutex_unlock(&__g_ums_lib.lib);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
    } else if (flag == USBH_DEV_OPEN_BY_UFUN) {
        struct usbh_function *p_usb_fun = (struct usbh_function *)p_handle;

        usbh_func_drvdata_get(p_usb_fun, (void **)&p_ms_tmp);
    }

    if (p_ms_tmp) {
        ret = __ms_ref_get(p_ms_tmp);
        if (ret != USB_OK) {
            return ret;
        }
        *p_ms = p_ms_tmp;

        return USB_OK;
    }
    return -USB_ENODEV;
}

/**
 * \brief 关闭 USB 大容量存储设备
 *
 * \param[out] p_ms 要关闭的 USB 大容量存储设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_close(struct usbh_ms *p_ms){
    if (p_ms == NULL) {
        return -USB_EINVAL;
    }
    return __ms_ref_put(p_ms);
}

/**
 * \brief 打开 USB 大容量存储设备逻辑单元
 *
 * \param[in]  p_ms  USB 大容量存储设备
 * \param[in]  lun   逻辑单元号
 * \param[out] p_lu  返回逻辑单元结构体地址
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_open(struct usbh_ms     *p_ms,
                    uint8_t             lun,
                    struct usbh_ms_lu **p_lu){
    int ret;

    if (p_ms == NULL) {
        return -USB_EINVAL;
    }
    if (p_ms->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
    if (lun > (p_ms->n_lu - 1)) {
        return -USB_EILLEGAL;
    }

    if (p_ms->p_lus != NULL) {
        if (p_ms->p_lus[lun].is_init == USB_TRUE) {
            ret = __ms_ref_get(p_ms);
            if (ret != USB_OK) {
                __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
                return ret;
            }

            *p_lu = &p_ms->p_lus[lun];

            return USB_OK;
        }
    }
    return -USB_ENODEV;
}

/**
 * \brief 关闭逻辑单元
 *
 * \param[in] p_lu 要关闭的逻辑单元结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_close(struct usbh_ms_lu *p_lu){
    if (p_lu == NULL) {
        return -USB_EINVAL;
    }

    return __ms_ref_put(p_lu->p_ms);
}

/**
 * \brief 获取逻辑单元名字
 *
 * \param[in]  p_lu      逻辑单元结构体
 * \param[out] p_name    存储逻辑单元名字的缓存
 * \param[in]  name_size 名字缓存大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_name_get(struct usbh_ms_lu *p_lu, char *p_name, uint32_t name_size){
    int ret = USB_OK;

    if ((p_lu == NULL) || (p_name == NULL)) {
        return -USB_EINVAL;
    }
    if ((strlen(p_lu->name) + 1) > name_size) {
        return -USB_EILLEGAL;
    }
    if (p_lu->p_ms->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
    if (p_lu->is_init == USB_FALSE) {
        return -USB_ENOINIT;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_lu->p_ms->p_lock, UMS_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    strcpy(p_name, p_lu->name);
#if USB_OS_EN
    ret = usb_mutex_unlock(p_lu->p_ms->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 设置逻辑单元名字
 *
 * \param[in] p_lu       逻辑单元结构体
 * \param[in] p_name_new 要设置的名字
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_name_set(struct usbh_ms_lu *p_lu, char *p_name_new){
    int ret = USB_OK;

    if ((p_lu == NULL) || (p_name_new == NULL) || (strlen(p_name_new) >= USB_NAME_LEN)) {
        return -USB_EINVAL;
    }
    if (p_lu->p_ms->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
    if (p_lu->is_init == USB_FALSE) {
        return -USB_ENOINIT;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_lu->p_ms->p_lock, UMS_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    memset(p_lu->name, 0, USB_NAME_LEN);
    strncpy(p_lu->name, p_name_new, (USB_NAME_LEN - 1));
#if USB_OS_EN
    ret = usb_mutex_unlock(p_lu->p_ms->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 设置逻辑单元用户私有数据
 *
 * \param[in] p_lu       逻辑单元结构体
 * \param[in] p_usr_priv 用户私有数据
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_usrdata_set(struct usbh_ms_lu *p_lu, void *p_usr_priv){
    int ret = USB_OK;

    if (p_lu == NULL) {
        return -USB_EINVAL;
    }
    if (p_lu->p_ms->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
    if (p_lu->is_init == USB_FALSE) {
        return -USB_ENOINIT;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_lu->p_ms->p_lock, UMS_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_lu->p_usr_priv = p_usr_priv;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_lu->p_ms->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 获取逻辑单元用户私有数据
 *
 * \param[in]  p_lu       逻辑单元结构体
 * \param[out] p_usr_priv 返回的用户私有数据
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_usrdata_get(struct usbh_ms_lu *p_lu, void **p_usr_priv){
    int ret = USB_OK;

    if (p_lu == NULL) {
        return -USB_EINVAL;
    }
    if (p_lu->p_ms->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
    if (p_lu->is_init == USB_FALSE) {
        return -USB_ENOINIT;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_lu->p_ms->p_lock, UMS_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_usr_priv = p_lu->p_usr_priv;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_lu->p_ms->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 逻辑单元缓冲读数据
 *        当调用 usbh_ms_blk_read 函数是最后一个参数设置为空时
 *        可用这个函数读数据
 *
 * \param[in] p_lu  逻辑单元结构体
 * \param[in] p_buf 读缓冲
 * \param[in] size  读数据大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_buf_read(struct usbh_ms_lu *p_lu, void *p_buf, uint32_t size){
    int ret = USB_OK;

    if ((p_lu == NULL) || (p_buf == NULL) || (size == 0)) {
        return -USB_EINVAL;
    }
    if (p_lu->p_buf == NULL) {
        return -USB_EILLEGAL;
    }
    if (p_lu->p_ms->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
    if (p_lu->is_init == USB_FALSE) {
        return -USB_ENOINIT;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_lu->p_ms->p_lock, UMS_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    memcpy(p_buf, p_lu->p_buf, size > p_lu->buf_size? p_lu->buf_size : size);
#if USB_OS_EN
    ret = usb_mutex_unlock(p_lu->p_ms->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 逻辑单元缓冲写数据
 *        当调用 usbh_ms_blk_write 函数是最后一个参数设置为空时
 *        可用这个函数写数据
 *
 * \param[in] p_lun 逻辑单元结构体
 * \param[in] p_buf 写缓冲
 * \param[in] size  写大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_buf_write(struct usbh_ms_lu *p_lu, void *p_buf, uint32_t size){
    int ret = USB_OK;

    if ((p_lu == NULL) || (p_buf == NULL) || (size == 0)) {
        return -USB_EINVAL;
    }
    if (p_lu->p_buf == NULL) {
        return -USB_EILLEGAL;
    }
    if (p_lu->p_ms->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
    if (p_lu->is_init == USB_FALSE) {
        return -USB_ENOINIT;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_lu->p_ms->p_lock, UMS_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    memcpy(p_lu->p_buf, p_buf, size > p_lu->buf_size? p_lu->buf_size : size);
#if USB_OS_EN
    ret = usb_mutex_unlock(p_lu->p_ms->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 获取 USB 大容量存储设备逻辑单元块大小
 *
 * \param[in] p_lu       逻辑单元结构体
 * \param[in] p_blk_size 存储块大小数据
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_blk_size_get(struct usbh_ms_lu *p_lu, uint32_t *p_blk_size){
    int ret = USB_OK;

    if ((p_lu == NULL) || (p_blk_size == NULL)) {
        return -USB_EINVAL;
    }
    if (p_lu->p_ms->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
    if (p_lu->is_init == USB_FALSE) {
        return -USB_ENOINIT;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_lu->p_ms->p_lock, UMS_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_blk_size = p_lu->blk_size;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_lu->p_ms->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 获取 USB 大容量存储设备逻辑单元块数量
 *
 * \param[in] p_lu   逻辑单元结构体
 * \param[in] p_nblk 存储块数量数据
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_nblks_get(struct usbh_ms_lu *p_lu, uint32_t *p_nblk){
    int ret = USB_OK;

    if ((p_lu == NULL) || (p_nblk == NULL)) {
        return -USB_EINVAL;
    }
    if (p_lu->p_ms->is_removed == USB_TRUE) {
        return -USB_ENODEV;
    }
    if (p_lu->is_init == USB_FALSE) {
        return -USB_ENOINIT;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_lu->p_ms->p_lock, UMS_LOCK_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_nblk = p_lu->n_blks;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_lu->p_ms->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 大容量存储设备探测函数
 */
int usbh_ms_probe(struct usbh_function *p_usb_fun){
    int proto, sub_class, class;

    /* 检查功能类*/
    class = USBH_FUNC_CLASS_GET(p_usb_fun);
    if (class != USB_CLASS_MASS_STORAGE) {
        return -USB_ENOTSUP;
    }

    /* 检查协议*/
    proto = USBH_FUNC_PROTO_GET(p_usb_fun);
    if ((proto != USB_MS_PRO_BBB) &&
            (proto != USB_MS_PRO_CBI_NCCI) &&
            (proto != USB_MS_PRO_CBI_CCI)) {
        return -USB_ENOTSUP;
    }

    /* 检查子协议*/
    sub_class = USBH_FUNC_SUBCLASS_GET(p_usb_fun);
    if (sub_class != USB_MS_SC_SCSI_TRANSPARENT) {
        return -USB_ENOTSUP;
    }

    p_usb_fun->func_type = USBH_FUNC_UMS;

    return USB_OK;
}

/**
 * \brief 创建一个 USB 大容量存储设备
 *
 * \param[in] p_usb_fun   相关的 USB 功能接口
 * \param[in] p_name      USB 大容量存储设备的名字
 * \param[in] lu_buf_size U盘数据交互缓存，影响U盘读写性能，推荐16k
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_create(struct usbh_function *p_usb_fun,
                   char                 *p_name,
                   uint32_t              lu_buf_size){
    int             ret_tmp, ret = -USB_ENOTSUP;
    uint8_t         n_lu         = 0;
    uint32_t        n_lu_mem     = 0;
    struct usbh_ms *p_ms         = NULL;

    /* 检查 USB 库是否正常*/
    if ((usb_lib_is_init(&__g_ums_lib.lib) == USB_FALSE) ||
            (__g_ums_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    if ((p_usb_fun == NULL) || (p_name == NULL) ||
            (lu_buf_size == 0) || (strlen(p_name) >= USB_NAME_LEN)) {
        return -USB_EINVAL;
    }

    /* 功能类型未确定，检查一下是否是 USB 大容量存储设备*/
    if (p_usb_fun->func_type != USBH_FUNC_UMS) {
        if (p_usb_fun->func_type == USBH_FUNC_UNKNOWN) {
            ret = usbh_ms_probe(p_usb_fun);
        }
        if (ret != USB_OK) {
            __USB_ERR_INFO("usb function is not mass storage\r\n");
            return ret;
        }
    }

    /* 引用 USB 设备*/
    ret = usbh_dev_ref_get(p_usb_fun->p_usb_dev);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
        return ret;
    }

    /* 获取最大的逻辑单元数*/
    ret = __ms_max_lun_get(p_usb_fun, &n_lu);
    if (ret != USB_OK) {
        __USB_ERR_INFO("usb mass storage logical unit numeber get failed(%d)\r\n", ret);
        goto __failed1;
    } else if (n_lu == 0) {
        __USB_ERR_INFO("usb mass storage have no logical unit\r\n");
        ret = -USB_ENODEV;
        goto __failed1;
    }

    /* 计算要申请的内存*/
    n_lu_mem = sizeof(struct usbh_ms) + (sizeof(struct usbh_ms_lu) * n_lu);
    p_ms = usb_lib_malloc(&__g_ums_lib.lib, n_lu_mem);
    if (p_ms == NULL) {
        ret = -USB_ENOMEM;
        goto __failed1;
    }
    memset(p_ms, 0, n_lu_mem);

    /* 初始化大容量存储设备*/
    ret = __ms_init(p_ms, p_usb_fun, p_name, n_lu, lu_buf_size);
    if (ret != USB_OK) {
        goto __failed1;
    }

    /* 往库中添加一个设备*/
    ret = usb_lib_dev_add(&__g_ums_lib.lib, &p_ms->node);
    if (ret != USB_OK) {
        goto __failed1;
    }

    ret = usb_refcnt_get(&__g_ums_lib.ref_cnt);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
        goto __failed2;
    }

    /* 设置 USB 设备类型*/
    usbh_dev_type_set(p_usb_fun->p_usb_dev, p_usb_fun->func_type);

    /* 设置 USB 功能结构体驱动数据*/
    usbh_func_drvdata_set(p_usb_fun, p_ms);

    return USB_OK;
__failed2:
    usb_lib_dev_del(&__g_ums_lib.lib, &p_ms->node);
__failed1:
    if (p_ms) {
        __ms_deinit(p_ms);
        usb_lib_mfree(&__g_ums_lib.lib, p_ms);
    }

    ret_tmp = usbh_dev_ref_put(p_usb_fun->p_usb_dev);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret_tmp);
    }
    return ret;
}

/**
 * \brief 销毁 USB 大容量存储设备
 *
 * \param[in] p_usb_fun USB 功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_destory(struct usbh_function *p_usb_fun){
    int             ret;
    struct usbh_ms *p_ms = NULL;

    if (p_usb_fun == NULL) {
        return -USB_EINVAL;
    }
    if (p_usb_fun->func_type != USBH_FUNC_UMS) {
        return -USB_EILLEGAL;
    }

    ret = usbh_func_drvdata_get(p_usb_fun, (void **)&p_ms);
    if (ret != USB_OK) {
        return ret;
    } else if (p_ms == NULL) {
        return -USB_ENODEV;
    }
    p_ms->is_removed = USB_TRUE;

    return __ms_ref_put(p_ms);
}

///**
// * \brief 获取USB大容量存储设备传输超时时间
// */
//int usbh_ms_xfer_timeout_get(int *pTimeOut){
//#if USB_OS_EN
//    int Ret;
//#endif
//    USB_ERR_RETVAL(ParaInvalid, (pTimeOut == NULL), -USB_EINVAL);
//    USB_ERR_RETVAL(NoInit, ((__gUmsLib.IsLibInit != USB_TRUE) ||
//                            (__gUmsLib.IsLibUniniting == USB_TRUE)), -USB_EPERM);
//
//#if USB_OS_EN
//    Ret = usb_mutex_lock(__gUmsLib.pLock, USBH_DEV_MUTEX_TIMEOUT);
//    USB_ERR_PE_RETVAL(MutexLockErr, (Ret != USB_OK), Ret, Ret);
//#endif
//    *pTimeOut = __gUmsLib.XferTimeOut;
//#if USB_OS_EN
//    Ret = usb_mutex_unlock(__gUmsLib.pLock);
//    USB_ERR_PE_RETVAL(MutexUnLockErr, (Ret != USB_OK), Ret, Ret);
//#endif
//
//    return USB_OK;
//}
//
///**
// * \brief 设置 USB 大容量存储设备传输超时时间
// *
// * \param[in] time_out 要设置的超时时间
// *
// * \retval 成功返回 USB_OK
// */
//int usbh_ms_xfer_timeout_set(int time_out){
//    int ret = USB_OK;
//
//    USB_RETVAL(((__g_ums_lib.is_lib_init != USB_TRUE) ||
//                (__g_ums_lib.is_lib_uniniting == USB_TRUE)), -USB_ENOINIT);
//    USB_RETVAL(((time_out < 0) && (time_out != USB_WAIT_FOREVER)), -USB_EINVAL);
//
//#if USB_OS_EN
//    ret = usb_mutex_lock(__g_ums_lib.p_lock, UMS_LOCK_TIMEOUT);
//    USB_ERR_TRACE_RETVAL(MutexLockErr, (ret != USB_OK), ret, ret);
//#endif
//    __g_ums_lib.xfer_time_out = time_out;
//#if USB_OS_EN
//    ret = usb_mutex_unlock(__g_ums_lib.p_lock);
//    USB_ERR_TRACE(MutexUnLockErr, (ret != USB_OK), ret);
//#endif
//    return ret;
//}
/**
 * \brief 获取当前存在的 USB 大容量存储设备数量
 *
 * \param[out] p_n_dev 存储返回的设备数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lib_ndev_get(uint8_t *p_n_dev){
    if (__g_ums_lib.is_lib_deiniting == USB_TRUE) {
        return -USB_ENOINIT;
    }
    return usb_lib_ndev_get(&__g_ums_lib.lib, p_n_dev);
}

/**
 * \brief 初始化 USB 大容量存储设备库
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lib_init(void){
    if (usb_lib_is_init(&__g_ums_lib.lib) == USB_TRUE) {
        return USB_OK;
    }

#if USB_MEM_RECORD_EN
    int ret = usb_lib_init(&__g_ums_lib.lib, "ums_mem");
#else
    int ret = usb_lib_init(&__g_ums_lib.lib);
#endif
    if (ret != USB_OK) {
        return ret;
    }

    /* 初始化引用计数*/
    usb_refcnt_init(&__g_ums_lib.ref_cnt);

    __g_ums_lib.xfer_time_out    = 5000;
    __g_ums_lib.is_lib_deiniting = USB_FALSE;

    return USB_OK;
}

/**
 * \brief USB 大容量存储库释放函数
 */
static void __ums_lib_release(int *p_ref){
    usb_lib_deinit(&__g_ums_lib.lib);
}

/**
 * \brief 反初始化 USB 大容量存储设备库
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lib_deinit(void){
    /* 检查 USB 库是否正常*/
    if (usb_lib_is_init(&__g_ums_lib.lib) == USB_FALSE) {
        return USB_OK;
    }
    __g_ums_lib.is_lib_deiniting = USB_TRUE;

    return usb_refcnt_put(&__g_ums_lib.ref_cnt, __ums_lib_release);
}

#if USB_MEM_RECORD_EN
/**
 * \brief 获取 USB 大容量存储驱动内存记录
 *
 * \param[out] p_mem_record 返回内存记录
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_mem_record_get(struct usb_mem_record *p_mem_record){
    return usb_lib_mem_record_get(&__g_ums_lib.lib, p_mem_record);
}
#endif
