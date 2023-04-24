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
#include "common/err/usb_err.h"
#include "common/refcnt/usb_refcnt.h"
#include "common/list/usb_list.h"
#include "core/include/device/core/usbd.h"

/*******************************************************************************
 * Global
 ******************************************************************************/
/* \brief 声明一个 USB 从机库结构体*/
struct usbd_core_lib __g_usb_device_lib;

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern int usbd_dev_standard_req_handle(struct usbd_dev    *p_dev,
                                        struct usbd_ep0    *p_ep0,
                                        struct usb_ctrlreq *p_setup);
extern int usbd_dev_func_req_handle(struct usbd_dev    *p_dev,
                                    struct usb_ctrlreq  *p_setup);
extern int usbd_dev_vendor_req_handle(struct usbd_dev    *p_dev,
                                      struct usbd_ep0    *p_ep0,
                                      struct usb_ctrlreq *p_setup);
extern int usbd_dev_disconnect_handle(struct usbd_dev *p_dev);

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief USB 从机库释放函数
 */
static void __usb_core_lib_release(int *p_ref){
    usb_lib_deinit(&__g_usb_device_lib.lib);
}

/**
 * \brief USB 从机控制器释放函数
 */
static void __usb_dc_release(int *p_ref){
    //todo
}

/**
 * \brief 初始化 USB 从机库
 *
 * \retval 成功返回 USB_OK
 */
int usbd_core_lib_init(void){
    if (usb_lib_is_init(&__g_usb_device_lib.lib) == USB_TRUE) {
        return USB_OK;
    }

#if USB_MEM_RECORD_EN
    int ret = usb_lib_init(&__g_usb_device_lib.lib, "usb_dc_mem");
#else
    int ret = usb_lib_init(&__g_uhub_lib.lib);
#endif
    if (ret != USB_OK) {
        return ret;
    }
    /* 初始化引用计数*/
    usb_refcnt_init(&__g_usb_device_lib.ref_cnt);

    __g_usb_device_lib.is_lib_deiniting = USB_FALSE;

    return USB_OK;
}

/**
 * \brief 反初始化 USB 从机库
 *
 * \retval 成功返回 USB_OK
 */
int usbd_core_lib_deinit(void){
    /* 检查 USB 库是否正常*/
    if (usb_lib_is_init(&__g_usb_device_lib.lib) == USB_FALSE) {
        return USB_OK;
    }
    __g_usb_device_lib.is_lib_deiniting = USB_TRUE;

    return usb_refcnt_put(&__g_usb_device_lib.ref_cnt, __usb_core_lib_release);
}

/**
 * \brief 缓存映射
 */
static int __buf_map(struct usbd_trans *p_trans){
    uint8_t  dir;

    p_trans->p_buf_dma = NULL;

    if (p_trans->p_hw->ep_addr & USB_DIR_IN) {
        dir = USB_DMA_TO_DEVICE;
    } else {
        dir = USB_DMA_FROM_DEVICE;
    }

    if (p_trans->p_buf && p_trans->len) {
        p_trans->p_buf_dma = usb_dma_map(p_trans->p_buf,
                                         p_trans->len,
                                         dir);
        if (p_trans->p_buf_dma == NULL) {
            return -USB_EAGAIN;
        }
    }

    return USB_OK;
}

/**
 * \brief 缓存取消映射
 */
static void __buf_unmap(struct usbd_trans *p_trans){
    uint8_t    dir;

    if (p_trans->p_hw->ep_addr & USB_DIR_IN) {
        dir = USB_DMA_TO_DEVICE;
    } else {
        dir = USB_DMA_FROM_DEVICE;
    }

    if (p_trans->p_buf_dma) {
        usb_dma_unmap(p_trans->p_buf_dma,
                      p_trans->len,
                      dir);
    }
}

/**
 * \brief 寻找具体端点地址的端点
 *
 * \param[in]  p_dc USB 从机控制器
 * \param[in]  ep_addr  要找的端点地址
 * \param[out] p_ep     返回找到的端点地址
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_ep_get(struct usb_dc *p_dc, uint8_t ep_addr, struct usbd_ep **p_ep){
    struct usb_list_node *p_node   = NULL;
    struct usbd_ep       *p_ep_tmp = NULL;

    if (p_dc == NULL) {
        return -USB_EINVAL;
    }

    /* 遍历USB设备控制器所有端点*/
    usb_list_for_each_node(p_node, &p_dc->ep_list) {
        p_ep_tmp = usb_container_of(p_node, struct usbd_ep, node);

        /* 端点被使用，继续寻找*/
        //if (p_ep_tmp->is_used) {
        if (p_ep_tmp->ep_addr == ep_addr) {
            *p_ep = p_ep_tmp;
            return USB_OK;
        }
        //}
    }

    return -USB_ENODEV;
}

/**
 * \brief USB 从机控制器端点分配
 *
 * \param[in]  p_dc  USB 从机控制器
 * \param[in]  addr  端点地址，地址低 4 位为 0 则代表自动分配端点
 * \param[in]  type  端点类型
 * \param[in]  mps   端点最大包长度
 * \param[out] p_ep  返回分配成功的端点结构体地址
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_ep_alloc(struct usb_dc   *p_dc,
                    uint8_t          addr,
                    uint8_t          type,
                    uint16_t         mps,
                    struct usbd_ep **p_ep){
    struct usb_list_node *p_node   = NULL;
    struct usbd_ep       *p_ep_tmp = NULL;
    usb_bool_t            is_auto_alloc;
    int                   ret      = USB_OK;
#if USB_OS_EN
    int                   ret_tmp;
#endif

    if (p_dc == NULL) {
        return -USB_EINVAL;
    }

    /* 判断端点地址是否是自动分配(地址为 0 为自动分配)*/
    is_auto_alloc = (addr & 0x0F) ? USB_FALSE : USB_TRUE;

#if USB_OS_EN
    ret = usb_mutex_lock(p_dc->p_mutex, USB_DC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    /* 遍历 USB 从机控制器所有端点*/
    usb_list_for_each_node(p_node, &p_dc->ep_list) {
        p_ep_tmp = usb_container_of(p_node, struct usbd_ep, node);

        /* 排除端点0*/
        if (!(p_ep_tmp->ep_addr & 0x0F)) {
            continue;
        }
        /* 自动分配地址*/
        if (is_auto_alloc) {
            /* 检查端点方向是否一致*/
            if ((p_ep_tmp->ep_addr & 0x80) != (addr & 0x80)) {
                continue;
            }
        } else { /* 非自动分配地址*/
            /* 检查地址是否匹配*/
            if (p_ep_tmp->ep_addr != addr) {
                continue;
            }
        }

        /* 检查端点的最大包长度限制*/
        if ((p_ep_tmp->mps_limt) && (p_ep_tmp->mps_limt < mps)) {
            continue;
        }

        /* 检查端点是否匹配指定类型*/
        switch (type) {
            /* 控制端点*/
            case USB_EP_TYPE_CTRL:
                if(p_ep_tmp->type_support & USBD_EP_SUPPORT_CTRL) {
                    goto __end;
                }
                break;
            /* 等时端点*/
            case USB_EP_TYPE_ISO:
                if(p_ep_tmp->type_support & USBD_EP_SUPPORT_ISO) {
                    goto __end;
                }
                break;
            /* 批量端点*/
            case USB_EP_TYPE_BULK:
                if(p_ep_tmp->type_support & USBD_EP_SUPPORT_BULK) {
                    goto __end;
                }
                break;
            /* 中断端点*/
            case USB_EP_TYPE_INT:
                if(p_ep_tmp->type_support & USBD_EP_SUPPORT_INT) {
                    goto __end;
                }
                break;

            default:break;
        }
    }
    p_ep_tmp = NULL;

    ret = -USB_ENODEV;
__end:
    /* 更新端点信息*/
    if (p_ep_tmp) {
        p_ep_tmp->cur_type = type;
        p_ep_tmp->cur_mps  = mps;
        //p_ep_tmp->is_used  = USB_TRUE;

        *p_ep = p_ep_tmp;
    }

#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_dc->p_mutex);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 从机控制器端点使能
 *
 * \param[in] p_dc USB 从机控制器
 * \param[in] p_ep     要使能的端点结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_ep_enable(struct usb_dc *p_dc, struct usbd_ep *p_ep){
    int ret;

    if ((p_dc == NULL) || (p_ep == NULL)) {
        return -USB_EINVAL;
    }

    if ((p_ep->is_enable != USB_TRUE) && (p_dc->p_dc_drv->p_fn_ep_enable)) {
        ret = p_dc->p_dc_drv->p_fn_ep_enable(p_dc->p_dc_drv_arg, p_ep);
        if (ret != USB_OK) {
            return ret;
        }
    }

    p_ep->is_enable = USB_TRUE;

    return USB_OK;
}

/**
 * \brief USB 从机控制器端点禁能
 *
 * \param[in] p_dc USB 从机控制器
 * \param[in] p_ep     要禁能的端点结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_ep_disable(struct usb_dc *p_dc, struct usbd_ep *p_ep){
    int ret;

    if ((p_dc == NULL) || (p_ep == NULL)) {
        return -USB_EINVAL;
    }

    if ((p_ep->is_enable == USB_TRUE) && (p_dc->p_dc_drv->p_fn_ep_disable)) {

        ret = p_dc->p_dc_drv->p_fn_ep_disable(p_dc->p_dc_drv_arg, p_ep);
        if (ret != USB_OK) {
            return ret;
        }
    }

    p_ep->is_enable = USB_FALSE;

    return USB_OK;
}

/**
 * \brief USB 从机控制器端点复位
 */
static int __dc_ep_reset(struct usb_dc *p_dc, struct usbd_ep *p_ep){
    int ret = USB_OK;
#if USB_OS_EN
    int ret_tmp;

    ret = usb_mutex_lock(p_dc->p_mutex, USB_DC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_dc->p_dc_drv->p_fn_ep_reset) {
        /* 复位控制器端点*/
        ret = p_dc->p_dc_drv->p_fn_ep_reset(p_dc->p_dc_drv_arg, p_ep);
    } else {
    	if (p_dc->p_dc_drv->p_fn_ep_disable) {
            ret = p_dc->p_dc_drv->p_fn_ep_disable(p_dc->p_dc_drv_arg, p_ep);
            if ((ret == USB_OK) && (p_ep->is_enable == USB_TRUE)) {
                if (p_dc->p_dc_drv->p_fn_ep_enable) {
                    ret = p_dc->p_dc_drv->p_fn_ep_enable(p_dc->p_dc_drv_arg, p_ep);
                }
            }
    	}
    }

#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_dc->p_mutex);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 从机控制器端点复位
 */
int usb_dc_ep_halt_set(struct usb_dc *p_dc, struct usbd_ep *p_ep, usb_bool_t is_halt){
    int ret = USB_OK;
#if USB_OS_EN
    int ret_tmp;
#endif

    if ((p_dc == NULL) || (p_ep == NULL)) {
        return -USB_EINVAL;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_dc->p_mutex, USB_DC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_dc->p_dc_drv->p_fn_ep_halt_set) {
        /* 停止控制器端点*/
        ret = p_dc->p_dc_drv->p_fn_ep_halt_set(p_dc->p_dc_drv_arg, p_ep, is_halt);
    }
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_dc->p_mutex);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 端点 0 控制完成传输回调函数
 */
static void __ep0_ctrl_complete(void *p_arg){
    struct usbd_ep0 *p_ep0 = (struct usbd_ep0 *)p_arg;
    int              ret;

    /* 有数据阶段，要进入状态阶段*/
    if (p_ep0->p_dc->need_sta) {
        p_ep0->req.len        = 0;
        p_ep0->p_dc->need_sta = USB_FALSE;

        if (p_ep0->p_hw->ep_addr & USB_DIR_IN) {
            p_ep0 = &p_ep0->p_dc->ep0_out;
        } else {
            p_ep0 = &p_ep0->p_dc->ep0_in;
        }
        p_ep0->req.len     = 0;
        p_ep0->req.act_len = 0;

        ret = usb_dc_trans_req(p_ep0->p_dc, &p_ep0->req);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
        }
    } else {
        if (p_ep0->req.len == 0) {
            if (p_ep0->p_fn_ep0_cb) {
                p_ep0->p_fn_ep0_cb(p_ep0->p_arg);
                p_ep0->p_arg       = NULL;
                p_ep0->p_fn_ep0_cb = NULL;
            }
        }
    }
}

/**
 * \brief 分配一个端点0
 */
static struct usbd_ep *__dc_ep0_alloc(struct usb_dc *p_dc,
                                      uint8_t        addr){
   struct usb_list_node *p_node  = NULL;
   struct usbd_ep       *p_ep    = NULL;
   uint8_t               ep0_mps = p_dc->p_usbd_dev->dev_desc.max_packet_size0;

   /* 遍历所有的端点*/
   usb_list_for_each_node(p_node, &p_dc->ep_list) {
       p_ep = usb_container_of(p_node, struct usbd_ep, node);

       /* 匹配地址*/
       if (p_ep->ep_addr != addr) {
           continue;
       }
       /* 匹配最大包大小*/
       if ((p_ep->mps_limt) && (p_ep->mps_limt < ep0_mps)) {
           continue;
       }
       /* 匹配类型*/
       if(!(p_ep->type_support & USBD_EP_SUPPORT_CTRL)) {
           continue;
       }

       p_ep->cur_type = USB_EP_TYPE_CTRL;
       p_ep->cur_mps  = ep0_mps;

       return p_ep;
   }

   return NULL;
}

/**
 * \brief 释放 USB 从机控制器端点 0
 */
static void __dc_ep0_free(struct usbd_ep *p_ep){
    /* 清除端点信息*/
    p_ep->cur_type = 0;
    p_ep->cur_mps  = 0;
}

/**
 * \brief 初始化从机控制器端点 0
 *
 * \param[in] p_dc    USB 从机控制器
 * \param[in] p_ep0   端点 0 结构体
 * \param[in] ep_addr 端点地址
 *
 * \retcal 成功返回 USB_OK
 */
static int __dc_ep0_init(struct usb_dc   *p_dc,
                         struct usbd_ep0 *p_ep0,
                         uint8_t          ep_addr){
    int ret;

    p_ep0->p_hw = __dc_ep0_alloc(p_dc, ep_addr);
    if (p_ep0->p_hw == NULL) {
        return -USB_ENOMEM;
    }
    if (ep_addr == USB_DIR_IN) {
        p_ep0->req.p_buf = usb_lib_malloc(&__g_usb_device_lib.lib, p_dc->ep0_req_size);
        if (p_ep0->req.p_buf == NULL) {
            return -USB_ENOMEM;
        }
    }
    p_ep0->req.p_hw          = p_ep0->p_hw;
    p_ep0->req.p_fn_complete = __ep0_ctrl_complete;
    p_ep0->req.p_arg         = p_ep0;
    p_ep0->p_dc              = p_dc;

    if (p_dc->p_dc_drv->p_fn_ep_enable) {
        ret = p_dc->p_dc_drv->p_fn_ep_enable(p_dc->p_dc_drv_arg, p_ep0->p_hw);
        if (ret != USB_OK) {
        	usb_lib_mfree(&__g_usb_device_lib.lib, p_ep0->req.p_buf);
            return ret;
        }
    }

    p_ep0->p_hw->is_enable = USB_TRUE;

    return USB_OK;
}

/**
 * \brief 反初始化从机控制器端点 0
 */
static int __dc_ep0_deinit(struct usb_dc   *p_dc,
                           struct usbd_ep0 *p_ep0){
    int ret;

    if (p_dc->p_dc_drv->p_fn_ep_disable) {
        ret = p_dc->p_dc_drv->p_fn_ep_disable(p_dc, p_ep0->p_hw);
        if (ret != USB_OK) {
            return ret;
        } else {
            p_ep0->p_hw->is_enable = USB_FALSE;
        }
    }
    __dc_ep0_free(p_ep0->p_hw);

    if (p_ep0->req.p_buf) {
        usb_lib_mfree(&__g_usb_device_lib.lib, p_ep0->req.p_buf);
        p_ep0->req.p_buf = NULL;
    }

    return USB_OK;
}

/**
 * \brief USB 从机控制器端点 0 请求
 *
 * \param[in] p_dc            USB 从机控制器
 * \param[in] dir             请求方向
 * \param[in] p_buf           数据缓存
 * \param[in] buf_size        缓存大小
 * \param[in] p_fn_ep0_out_cb 回调函数
 * \param[in] p_arg           回调函数参数
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_ep0_req(struct usb_dc *p_dc,
                   uint8_t        dir,
                   uint8_t       *p_buf,
                   uint32_t       buf_size,
                   void         (*p_fn_ep0_cb)(void *p_arg),
                   void          *p_arg){
    if ((p_dc == NULL) || (p_buf == NULL) || (buf_size == 0)) {
        return -USB_EINVAL;
    }

    if (buf_size > p_dc->ep0_req_size) {
        return -USB_ESIZE;
    }

    if (dir == USB_DIR_IN) {
        memcpy(p_dc->ep0_in.req.p_buf, p_buf, buf_size);
        p_dc->ep0_in.req.len      = buf_size;
        p_dc->ep0_out.p_fn_ep0_cb = p_fn_ep0_cb;
        p_dc->ep0_out.p_arg       = p_arg;
        //p_dc->ep0_in.p_fn_ep0_cb = p_fn_ep0_cb;
        //p_dc->ep0_in.p_arg       = p_arg;
    } else if (dir == USB_DIR_OUT) {
        p_dc->ep0_out.req.p_buf  = p_buf;
        p_dc->ep0_out.req.len    = buf_size;
        p_dc->ep0_in.p_fn_ep0_cb = p_fn_ep0_cb;
        p_dc->ep0_in.p_arg       = p_arg;
        //p_dc->ep0_out.p_fn_ep0_cb = p_fn_ep0_cb;
        //p_dc->ep0_out.p_arg       = p_arg;
    } else {
        return -USB_EILLEGAL;
    }

    return USB_OK;
}

/**
 * \brief 设置端点 0 停止
 */
int usb_dc_ep0_stall_set(struct usb_dc *p_dc, usb_bool_t is_halt){

    int ret = usb_dc_ep_halt_set(p_dc, p_dc->ep0_out.p_hw, is_halt);
    if (ret != USB_OK) {
        return ret;
    }
    return usb_dc_ep_halt_set(p_dc, p_dc->ep0_in.p_hw, is_halt);
}

/**
 * \brie USB 从机传输请求函数
 *
 * \param[in] p_dc    USB 从机控制器
 * \param[in] p_trans USB 传输请求包
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_trans_req(struct usb_dc     *p_dc,
                     struct usbd_trans *p_trans){
    int ret;
#if USB_OS_EN
    int ret_tmp;
#endif

    if ((p_dc == NULL) || (p_trans == NULL)) {
        return -USB_EINVAL;
    }

    /* 该传输正在进行中，返回错误*/
    if (p_trans->status == -USB_EINPROGRESS) {
        return -USB_EBUSY;
    }
    /* 如果是同步传输*/
    if (p_trans->p_hw->cur_type == USB_EP_TYPE_ISO) {
        /* 传输数据长度大于端点的最大包长度，返回错误*/
        if (p_trans->len > p_trans->p_hw->cur_mps) {
            return -USB_ESIZE;
        }
    }

    if (p_trans->p_hw->is_stalled == USB_TRUE) {
        return -USB_EAGAIN;
    }

    /* 传输请求地址映射*/
    ret = __buf_map(p_trans);
    if (ret != USB_OK) {
        return ret;
    }
    /* 更新传输状态*/
    p_trans->status = -USB_EINPROGRESS;
#if USB_OS_EN
    ret = usb_mutex_lock(p_dc->p_mutex, USB_DC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 调用 USB 设备驱动传输请求回调函数*/
    ret = p_dc->p_dc_drv->p_fn_xfer_req(p_dc->p_dc_drv_arg, p_trans);
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_dc->p_mutex);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    if (ret != USB_OK) {
        /* 取消映射*/
        __buf_unmap(p_trans);
    }

    return ret;
}

/**
 * \brie USB 从机传输取消请求函数
 *
 * \param[in] p_dc    USB 从机控制器
 * \param[in] p_trans USB 传输请求包
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_trans_cancel(struct usb_dc     *p_dc,
                        struct usbd_trans *p_trans){
    int ret;
#if USB_OS_EN
    int ret_tmp;
#endif

    /* 更新传输状态*/
    p_trans->status = -USB_ECANCEL;
#if USB_OS_EN
    ret = usb_mutex_lock(p_dc->p_mutex, USB_DC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 调用 USB 设备驱动传输请求取消回调函数*/
    ret = p_dc->p_dc_drv->p_fn_xfer_cancel(p_dc->p_dc_drv_arg, p_trans);
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_dc->p_mutex);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 从机控制器传输完成函数
 *
 * \param[in] p_trans 传输事务
 * \param[in] status  传输状态
 * \param[in] len_act 实际传输的长度
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_trans_complete(struct usbd_trans *p_trans,
                          int                status,
                          uint32_t           len_act){
    if (p_trans == NULL) {
        return -USB_EINVAL;
    }
    p_trans->status  = status;
    p_trans->act_len = len_act;

    __buf_unmap(p_trans);

    if (p_trans->p_fn_complete) {
        p_trans->p_fn_complete(p_trans->p_arg);
    }

    return USB_OK;
}

/**
 * \brief USB 从机控制器控制传输处理
 *
 * \param[in] p_dc USB 从机控制器
 * \param[in] p_setup  Setup 包
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_setup_handle(struct usb_dc      *p_dc,
                        struct usb_ctrlreq *p_setup){
    struct usbd_ep0 *p_ep0;
    int              ret;

    if ((p_dc == NULL) || (p_setup == NULL)) {
        return -USB_EINVAL;
    }

    if (p_setup->length == 0) {
        p_ep0 = &p_dc->ep0_in;
    } else {
        if (p_setup->request_type & USB_DIR_IN) {
            /* 设备到主机 */
            p_ep0 = &p_dc->ep0_in;
        } else {
            /* 主机到设备 */
            p_ep0 = &p_dc->ep0_out;
        }
    }

    p_ep0->req.len = 0;

    switch((p_setup->request_type & USB_REQ_TYPE_MASK)) {
        case USB_REQ_TYPE_STANDARD:
            /* 标准请求*/
            ret = usbd_dev_standard_req_handle(p_dc->p_usbd_dev, p_ep0, p_setup);
            if (ret != USB_OK) {
                return ret;
            }
            break;
        case USB_REQ_TYPE_CLASS:
            ret = usbd_dev_func_req_handle(p_dc->p_usbd_dev, p_setup);
            if (ret != USB_OK) {
                return ret;
            }
            break;
        case USB_REQ_TYPE_VENDOR:
            ret = usbd_dev_vendor_req_handle(p_dc->p_usbd_dev, p_ep0, p_setup);
            if (ret != USB_OK) {
                return ret;
            }
            break;
        default:
            __USB_ERR_INFO("setup request type %d unknown\r\n", p_setup->request_type & USB_REQ_TYPE_MASK);

            usb_dc_ep0_stall_set(p_dc, USB_TRUE);

            return -USB_EILLEGAL;
    }

    /* 有数据阶段要处理 */
    if (p_setup->length) {
        p_dc->need_sta = USB_TRUE;
    } else {
        p_dc->need_sta = USB_FALSE;
    }

    return usb_dc_trans_req(p_dc, &p_ep0->req);
}

/**
 * \brief USB 从机控制器断开处理
 *
 * \param[in] p_dc USB 从机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_disconnect_handle(struct usb_dc *p_dc){
    if (p_dc == NULL) {
        return -USB_EINVAL;
    }

    __USB_INFO("usb device disconnect\r\n");

    if ((p_dc->p_usbd_dev == NULL) || (p_dc->p_usbd_dev->p_cur_cfg == NULL)) {
        return USB_OK;
    }

    return usbd_dev_disconnect_handle(p_dc->p_usbd_dev);
}

/**
 * \brief USB 从机控制器速度更新
 *
 * \param[in] p_dc   USB 从机控制器
 * \param[in] speed  要更新的速度
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_speed_update(struct usb_dc *p_dc, uint8_t speed){
    char *p_name;

    if (p_dc == NULL) {
        return -USB_EINVAL;
    }

    switch (speed) {
        case USB_SPEED_HIGH:
            p_name = "hs";
            break;
        case USB_SPEED_FULL:
            p_name = "fs";
            break;
        case USB_SPEED_LOW:
            p_name = "ls";
            break;
        default:
            p_name = "unkonwn";
            break;
    }
    __USB_INFO("usb device speed update: %s\r\n", p_name);

    p_dc->speed = speed;

    return USB_OK;
}

/**
 * \brief USB 从机控制器总线复位处理
 *
 * \param[in] p_dc USB 从机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_bus_reset_handle(struct usb_dc *p_dc){
    int ret;

    if (p_dc == NULL) {
        return -USB_EINVAL;
    }

    __USB_INFO("usb device bus reset\r\n");

    /* 总线复位*/
    p_dc->state    = USBD_ST_DEFAULT;
    p_dc->speed    = USBD_SPEED_UNKNOWN;
    p_dc->need_sta = USB_FALSE;

    /* 复位端点 0*/
    ret = __dc_ep_reset(p_dc, p_dc->ep0_in.p_hw);
    if (ret != USB_OK) {
        return ret;
    }
    ret = __dc_ep_reset(p_dc, p_dc->ep0_out.p_hw);
    if (ret != USB_OK) {
        return ret;
    }
    /* 如果设备已经配置完，则需要断开 */
    if (p_dc->p_usbd_dev != NULL) {
        if (p_dc->p_usbd_dev->status == USB_DEV_STATE_CONFIGURED) {
            ret = usb_dc_disconnect_handle(p_dc);
            if (ret != USB_OK) {
                return ret;
            }
        }
        p_dc->p_usbd_dev->status = USB_DEV_STATE_NOTATTACHED;
    }

    return USB_OK;
}

/**
 * \brief USB 从机控制器恢复处理
 *
 * \param[in] p_dc USB 从机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_resume_handle(struct usb_dc *p_dc){
    int                   ret, i, j;
    struct usbd_config   *p_cfg      = NULL;
    struct usbd_func_hdr *p_func_hdr = NULL;
    struct usbd_func     *p_func_tmp = NULL;

    if (p_dc == NULL) {
        return -USB_EINVAL;
    }

    __USB_INFO("usb device resume\r\n");

    if ((p_dc->p_usbd_dev == NULL) || (p_dc->p_usbd_dev->p_cur_cfg == NULL)) {
        return USB_OK;
    }

    p_cfg = p_dc->p_usbd_dev->p_cur_cfg;

    for (i = 0; i < p_cfg->n_func; i++) {
    	p_func_hdr = p_cfg->p_func_hdr[i];

        for (j = 0; j < p_func_hdr->n_func_alt; j++) {
        	p_func_tmp = p_func_hdr->p_func[j];

            if ((p_func_tmp->p_opts != NULL) &&
            		(p_func_tmp->p_opts->p_fn_resume != NULL)) {
                ret = p_func_tmp->p_opts->p_fn_resume(p_func_tmp, p_func_tmp->p_func_arg);
                if (ret != USB_OK) {
                    __USB_ERR_INFO("interface %d alternate setting %d resume failed(%d)\r\n",
                            p_func_tmp->intf_desc.interface_number,
                            p_func_tmp->intf_desc.alternate_setting);
                    return ret;
                }
            }
        }
    }

    return USB_OK;
}

/**
 * \brief USB 从机控制器挂起处理
 *
 * \param[in] p_dc USB 从机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_suspend_handle(struct usb_dc *p_dc){
    int                   ret, i, j;
    struct usbd_config   *p_cfg      = NULL;
    struct usbd_func_hdr *p_func_hdr = NULL;
    struct usbd_func     *p_func_tmp = NULL;

    if (p_dc == NULL) {
        return -USB_EINVAL;
    }

    __USB_INFO("usb device suspend\r\n");

    if ((p_dc->p_usbd_dev == NULL) || (p_dc->p_usbd_dev->p_cur_cfg == NULL)) {
        return USB_OK;
    }

    p_cfg = p_dc->p_usbd_dev->p_cur_cfg;

    for (i = 0; i < p_cfg->n_func; i++) {
        p_func_hdr = p_cfg->p_func_hdr[i];

        for (j = 0; j < p_func_hdr->n_func_alt; j++) {
            p_func_tmp = p_func_hdr->p_func[j];

            if ((p_func_tmp->p_opts != NULL) &&
                    (p_func_tmp->p_opts->p_fn_suspend != NULL)) {
                ret = p_func_tmp->p_opts->p_fn_suspend(p_func_tmp, p_func_tmp->p_func_arg);
                if (ret != USB_OK) {
                    __USB_ERR_INFO("interface %d alternate setting %d suspend failed(%d)\r\n",
                            p_func_tmp->intf_desc.interface_number,
                            p_func_tmp->intf_desc.alternate_setting);
                    return ret;
                }
            }
        }
    }

    return USB_OK;
}

/**
 * \brief 获取 USB 从机控制器引用
 */
int usb_dc_ref_get(struct usb_dc *p_dc){
    return usb_refcnt_get(&p_dc->ref_cnt);
}

/**
 * \brief 释放 USB 从机控制器引用
 */
int usb_dc_ref_put(struct usb_dc *p_dc){
    return usb_refcnt_put(&p_dc->ref_cnt, __usb_dc_release);
}

/**
 * \brief 设置 USB 从机控制器用户数据
 *
 * \param[in] p_dc       USB 从机控制器
 * \param[in] p_usr_data 要设置的用户数据
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_usr_data_set(struct usb_dc *p_dc, void *p_usr_data){
    int ret = USB_OK;

    if (p_dc == NULL) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_dc->p_mutex, USB_DC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_dc->p_usr_data = p_usr_data;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_dc->p_mutex);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 获取 USB 从机控制器用户数据
 *
 * \param[in]  p_dc       USB 从机控制器
 * \param[out] p_usr_data 返回的用户数据
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_usr_data_get(struct usb_dc *p_dc, void **p_usr_data){
    int ret = USB_OK;

    if (p_dc == NULL) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_dc->p_mutex, USB_DC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_usr_data = p_dc->p_usr_data;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_dc->p_mutex);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 注册一个 USB 从机控制器端点
 *
 * \param[in] p_dc         相关的 USB 从机控制器
 * \param[in] ep_addr      端点地址
 * \param[in] type_support 可支持的类型
 * \param[in] mps_limt     最大包大小限制
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_ep_register(struct usb_dc  *p_dc,
                       uint8_t         ep_addr,
                       uint8_t         type_support,
                       uint16_t        mps_limt){
    int                   ret      = USB_OK;
#if USB_OS_EN
    int                   ret_tmp;
#endif
    struct usb_list_node *p_node   = NULL;
    struct usbd_ep       *p_ep_tmp = NULL;

    if (p_dc == NULL) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_dc->p_mutex, USB_DC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    /* 检查端点是否存在*/
    usb_list_for_each_node(p_node, &p_dc->ep_list) {
        p_ep_tmp = usb_container_of(p_node, struct usbd_ep, node);

        if (p_ep_tmp->ep_addr == ep_addr) {
            ret = -USB_EEXIST;
            goto __end;
        }
    }

    p_ep_tmp = usb_lib_malloc(&__g_usb_device_lib.lib, sizeof(struct usbd_ep));
    if (p_ep_tmp == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_ep_tmp, 0, sizeof(struct usbd_ep));

    p_ep_tmp->ep_addr      = ep_addr;
    p_ep_tmp->mps_limt     = mps_limt;
    p_ep_tmp->type_support = type_support;
    p_ep_tmp->is_enable    = USB_FALSE;

    usb_list_node_add_tail(&p_ep_tmp->node, &p_dc->ep_list);

__end:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_dc->p_mutex);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 注销一个 USB 从机控制器端点
 *
 * \param[in] p_dc    相关的 USB 从机控制器
 * \param[in] ep_addr 端点地址
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_ep_deregister(struct usb_dc *p_dc, uint8_t ep_addr){
    //todo
    return USB_OK;
}

/**
 * \brief 通过索引获取 USB 从机控制器
 *
 * \param[in]  idx  USB 从机控制器索引
 * \param[out] p_dc 返回的 USB 从机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_get(uint8_t idx, struct usb_dc **p_dc){
#if USB_OS_EN
    int                   ret;
#endif
    struct usb_list_node *p_node     = NULL;
    struct usb_list_node *p_node_tmp = NULL;
    struct usb_dc        *p_dc_find  = NULL;

    if (usb_lib_is_init(&__g_usb_device_lib.lib) == USB_FALSE) {
        return -USB_ENOINIT;
    }

#if USB_OS_EN
    ret = usb_lib_mutex_lock(&__g_usb_device_lib.lib);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    USB_LIB_LIST_FOR_EACH_NODE(p_node, p_node_tmp, &__g_usb_device_lib.lib){
        p_dc_find = usb_container_of(p_node, struct usb_dc, node);

        if (p_dc_find->idx == idx) {
#if USB_OS_EN
            ret = usb_lib_mutex_unlock(&__g_usb_device_lib.lib);
            if (ret != USB_OK) {
                __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
                return ret;
            }
#endif
            *p_dc = p_dc_find;
            return USB_OK;
        }
    }
#if USB_OS_EN
    ret = usb_lib_mutex_unlock(&__g_usb_device_lib.lib);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    return -USB_ENODEV;
}

/**
 * \brief USB 从机控制器添加设备
 *
 * \param[in] p_dc  USB 从机控制器
 * \param[in] p_dev 要添加的 USB 设备
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_dev_add(struct usb_dc *p_dc, struct usbd_dev *p_dev){
    int ret = USB_OK;

#if USB_OS_EN
    ret = usb_mutex_lock(p_dc->p_mutex, USB_DC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    /* 添加到 USB 从机控制器链表中*/
    usb_list_node_add_tail(&p_dev->node, &p_dc->dev_list);
#if USB_OS_EN
    ret = usb_mutex_unlock(p_dc->p_mutex);
    if (ret != USB_OK) {
    	usb_list_node_del(&p_dev->node);
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief USB 从机控制器添加设备
 *
 * \param[in] p_dc  USB 从机控制器
 * \param[in] p_dev 要删除的 USB 设备
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_dev_del(struct usb_dc *p_dc, struct usbd_dev *p_dev){
    int ret = USB_OK;

#if USB_OS_EN
    ret = usb_mutex_lock(p_dc->p_mutex, USB_DC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
	usb_list_node_del(&p_dev->node);
    /* USB 从机控制器绑定要删除的设备*/
	if (p_dc->p_usbd_dev == p_dev) {
		p_dc->p_usbd_dev = NULL;
	}
#if USB_OS_EN
    ret = usb_mutex_unlock(p_dc->p_mutex);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 创建一个 USB 从机控制器
 *
 * \param[in]  idx           USB 从机控制器索引
 * \param[in]  ep0_req_size  端点 0 请求大小
 * \param[in]  p_drv         从机驱动函数集
 * \param[in]  p_dc_drv_arg  从机驱动参数
 * \param[out] p_dc          返回创建成功的 USB 从机控制机
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_create(uint8_t             idx,
                  uint32_t            ep0_req_size,
                  struct usb_dc_drv  *p_drv,
                  void               *p_dc_drv_arg,
                  struct usb_dc     **p_dc){
    int            ret;
    struct usb_dc *p_dc_tmp = NULL;
#if USB_OS_EN
    int            ret_tmp;
#endif

    if ((usb_lib_is_init(&__g_usb_device_lib.lib) == USB_FALSE) ||
            (__g_usb_device_lib.is_lib_deiniting == USB_TRUE)) {
        return -USB_ENOINIT;
    }

    if ((p_dc == NULL) ||
            (p_drv == NULL) ||
            (p_drv->p_fn_xfer_req == NULL) ||
            (p_drv->p_fn_xfer_cancel == NULL)) {
        return -USB_EINVAL;
    }

    p_dc_tmp = usb_lib_malloc(&__g_usb_device_lib.lib, sizeof(struct usb_dc));
    if (p_dc_tmp == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_dc_tmp, 0, sizeof(struct usb_dc));

    /* 初始化引用计数*/
    usb_refcnt_init(&p_dc_tmp->ref_cnt);

#if USB_OS_EN
    p_dc_tmp->p_mutex = usb_lib_mutex_create(&__g_usb_device_lib.lib);
    if (p_dc_tmp->p_mutex == NULL) {
        ret = -USB_EPERM;
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        goto __failed1;
    }
#endif

    p_dc_tmp->idx          = idx;
    p_dc_tmp->state        = USBD_ST_POWERED;
    p_dc_tmp->resume_state = USBD_ST_NOTATTACHED;
    p_dc_tmp->speed        = USBD_SPEED_UNKNOWN;
    p_dc_tmp->need_sta     = USB_FALSE;
    p_dc_tmp->p_usbd_dev   = NULL;
    p_dc_tmp->p_dc_drv     = p_drv;
    p_dc_tmp->p_dc_drv_arg = p_dc_drv_arg;
    p_dc_tmp->is_run       = USB_FALSE;
    p_dc_tmp->ep0_req_size = ep0_req_size;

    usb_list_head_init(&p_dc_tmp->ep_list);
    usb_list_head_init(&p_dc_tmp->dev_list);


    /* 往库中添加一个设备*/
    ret = usb_lib_dev_add(&__g_usb_device_lib.lib, &p_dc_tmp->node);
    if (ret != USB_OK) {
        goto __failed1;
    }

    ret = usb_refcnt_get(&__g_usb_device_lib.ref_cnt);
    if (ret != USB_OK) {
        goto __failed2;
    }

    *p_dc = p_dc_tmp;

    return USB_OK;
__failed2:
    usb_lib_dev_del(&__g_usb_device_lib.lib, &p_dc_tmp->node);
__failed1:
#if USB_OS_EN
    if (p_dc_tmp->p_mutex) {
        ret_tmp = usb_lib_mutex_destroy(&__g_usb_device_lib.lib, p_dc_tmp->p_mutex);
        if (ret_tmp != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret_tmp);
        }
    }
#endif
    usb_lib_mfree(&__g_usb_device_lib.lib, p_dc_tmp);

    return ret;
}

/**
 * \brief USB 从机控制器销毁
 *
 * \param[in] p_dc 要销毁的 USB 从机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_destroy(struct usb_dc *p_dc){
    int ret;
#if USB_OS_EN
    int ret_tmp;
#endif

    if (p_dc == NULL) {
        return USB_OK;
    }

#if USB_OS_EN
    ret = usb_lib_mutex_lock(&__g_usb_device_lib.lib);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    usb_list_node_del(&p_dc->node);

    ret = usb_dc_ref_put(p_dc);

#if USB_OS_EN
    ret_tmp = usb_lib_mutex_unlock(&__g_usb_device_lib.lib);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 启动 USB 从机控制器
 *
 * \param[in] p_dc USB 从机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_start(struct usb_dc *p_dc){
    int              ret;
#if USB_OS_EN
    int              ret_tmp;
#endif

    if (p_dc == NULL) {
        return -USB_EINVAL;
    }

    if (p_dc->is_run == USB_TRUE) {
        return -USB_EPERM;
    }

    if (p_dc->p_usbd_dev == NULL) {
        __USB_ERR_INFO("device controller don't register device\r\n");
        return -USB_ENODEV;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_dc->p_mutex, USB_DC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    /* 初始化控制器端点 0 */
    ret = __dc_ep0_init(p_dc, &p_dc->ep0_in, USB_DIR_IN);
    if (ret != USB_OK) {
        goto __exit;
    }
    ret = __dc_ep0_init(p_dc, &p_dc->ep0_out, USB_DIR_OUT);
    if (ret != USB_OK) {
        /* 要把初始化成功的输入端点 0 反初始化*/
        __dc_ep0_deinit(p_dc, &p_dc->ep0_in);
        goto __exit;
    }

    /* 调用USB设备控制器驱动运行回调函数*/
    if (p_dc->p_dc_drv->p_fn_run) {
        ret = p_dc->p_dc_drv->p_fn_run(p_dc->p_dc_drv_arg);
        if (ret == USB_OK) {
            if (p_dc->p_dc_drv->p_fn_pullup) {
               /* 上拉外部电阻*/
                p_dc->p_dc_drv->p_fn_pullup(p_dc->p_dc_drv_arg, USB_TRUE);
            }
        } else {
            goto __exit;
        }
    }

    /* 设置 USB 从机控制器运行状态*/
    p_dc->is_run  = USB_TRUE;

    ret = USB_OK;

__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_dc->p_mutex);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif

    return ret;
}

/**
 * \brief 停止 USB 从机控制器
 *
 * \param[in] p_dc 要停止的 USB 从机控制器
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_stop(struct usb_dc *p_dc){
    return USB_OK;
}

#if USB_MEM_RECORD_EN
/**
 * \brief 获取 USB 从机内存记录
 *
 * \param[out] p_mem_record 返回内存记录
 *
 * \retval 成功返回 USB_OK
 */
int usb_dc_mem_record_get(struct usb_mem_record *p_mem_record){
    return usb_lib_mem_record_get(&__g_usb_device_lib.lib, p_mem_record);
}
#endif
