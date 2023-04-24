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
#include "core/include/specs/usb_specs.h"
#include "core/include/device/core/usbd.h"

/*******************************************************************************
 * Static
 ******************************************************************************/
/* \brief 设备字符串索引*/
static uint32_t __g_dev_str_idx = 0;

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbd_core_lib __g_usb_device_lib;

extern int usb_dc_ep_halt_set(struct usb_dc *p_dc, struct usbd_ep *p_ep, usb_bool_t is_halt);
extern int usb_dc_ep0_stall_set(struct usb_dc *p_dc, usb_bool_t is_halt);
extern int usb_dc_ep_get(struct usb_dc *p_dc, uint8_t ep_addr, struct usbd_ep **p_ep);
extern int usb_dc_ref_get(struct usb_dc *p_dc);
extern int usb_dc_dev_add(struct usb_dc *p_dc, struct usbd_dev *p_dev);
extern int usb_dc_dev_del(struct usb_dc *p_dc, struct usbd_dev *p_dev);
extern int usb_dc_ep_alloc(struct usb_dc   *p_dc,
                           uint8_t          addr,
                           uint8_t          type,
                           uint16_t         mps,
                           struct usbd_ep **p_ep);
extern int usb_dc_ep_enable(struct usb_dc *p_dc, struct usbd_ep *p_ep);
extern int usb_dc_ep_disable(struct usb_dc *p_dc, struct usbd_ep *p_ep);

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 分配字符串的索引
 *
 * \param[in] p_idx 存储分配的索引的缓存
 *
 * \retval 成功返回 USB_OK
 */
int __dev_str_idx_alloc(uint8_t *p_idx){
    int i;

    for (i = 1; i < 32; i++) {
        if ((__g_dev_str_idx & (1 << i)) == 0) {
            *p_idx = i;
            __g_dev_str_idx |= (1 << i);
            return USB_OK;
        }
    }
    return -USB_ENOMEM;
}

/**
 * \brief 释放字符串的索引
 */
void __dev_str_idx_free(uint8_t idx){
    if (idx > 31) {
        return;
    }
    __g_dev_str_idx &= ~(1 << idx);
}

/**
 * \brief 检查索引是否被分配
 */
int __dev_str_idx_chk(uint8_t idx){
    if ((idx > 31) || (idx == 0)) {
        return -USB_EINVAL;
    }
    if (__g_dev_str_idx & (1 << idx)) {
        return USB_OK;
    }
    return -USB_EPERM;
}

/**
 * \brief 设备功能接口初始化
 */
static int __dev_func_init(struct usbd_dev       *p_dev,
                           struct usbd_config    *p_cfg,
                           struct usbd_func      *p_func,
                           uint8_t                type,
                           struct usbd_func_info *p_func_info,
                           struct usbd_func_opts *p_func_opts,
                           void                  *p_func_arg){
    int                   ret;
    uint8_t               is_malloc       = 0;
    uint8_t               intf_num        = p_func_info->intf_num;
    uint8_t               alt_setting_num = p_func_info->alt_setting_num;
    struct usbd_func_hdr *p_func_hdr      = NULL;

    if ((intf_num > USBD_INTF_NUM_MAX) || (alt_setting_num > USBD_INTF_ALT_NUM_MAX)) {
        return -USB_EILLEGAL;
    }

    memset(p_func, 0, sizeof(struct usbd_func));

    if (p_cfg->p_func_hdr[intf_num] == NULL) {
        p_func_hdr = usb_lib_malloc(&__g_usb_device_lib.lib, sizeof(struct usbd_func_hdr));
        if (p_func_hdr == NULL) {
            return -USB_ENOMEM;
        }
        memset(p_func_hdr, 0, sizeof(struct usbd_func_hdr));

        is_malloc = 1;
        p_cfg->n_func++;
        p_cfg->p_func_hdr[intf_num] = p_func_hdr;
    } else {
        if (p_cfg->p_func_hdr[intf_num]->p_func[alt_setting_num] != NULL) {
            return -USB_EEXIST;
        }
    }

    /* 有特殊的描述符*/
    if ((p_func_info->p_spec_desc != NULL) && (p_func_info->spec_desc_size != 0)) {
        p_func->p_spec_desc = usb_lib_malloc(&__g_usb_device_lib.lib, p_func_info->spec_desc_size);
        if (p_func->p_spec_desc == NULL) {
            ret = -USB_ENOMEM;
            goto __failed;
        }
        p_func->spec_desc_size = p_func_info->spec_desc_size;

        memset(p_func->p_spec_desc, 0, p_func->spec_desc_size);
        memcpy(p_func->p_spec_desc, p_func_info->p_spec_desc, p_func_info->spec_desc_size);
    }
    /* 分配获取设备配置接口功能描述符的字符串信息 */
    ret = __dev_str_idx_alloc(&p_func->intf_desc.i_interface);
    if (ret == USB_OK) {
        if ((p_func_info->p_intf_str != NULL) && (strlen(p_func_info->p_intf_str) != 0)) {
            strncpy(p_func->intf_str, p_func_info->p_intf_str, USBD_STRING_LENGTH - 1);
        }
        p_dev->p_str[p_func->intf_desc.i_interface] = p_func->intf_str;
    } else {
        goto __failed;
    }

    p_func->p_opts                        = p_func_opts;
    p_func->p_cfg                         = p_cfg;
    p_func->p_func_arg                    = p_func_arg;
    p_func->intf_desc.length              = sizeof(struct usb_interface_desc);
    p_func->intf_desc.descriptor_type     = USB_DT_INTERFACE;
    p_func->intf_desc.num_endpoints       = 0;
    p_func->intf_desc.alternate_setting   = p_func_info->alt_setting_num;
    p_func->intf_desc.interface_protocol  = p_func_info->protocol;
    p_func->intf_desc.interface_class     = p_func_info->class;
    p_func->intf_desc.interface_sub_class = p_func_info->sub_class;
    p_func->intf_desc.interface_number    = p_func_info->intf_num;

    p_cfg->p_func_hdr[intf_num]->type     = type;
    p_cfg->p_func_hdr[intf_num]->func_num = intf_num;
    p_cfg->p_func_hdr[intf_num]->n_func_alt++;
    p_cfg->p_func_hdr[intf_num]->p_func[alt_setting_num] = p_func;
    p_cfg->config_desc.num_interfaces++;
    p_cfg->config_desc.total_length += (sizeof(struct usb_interface_desc) + p_func_info->spec_desc_size);

    return USB_OK;
__failed:
    if (is_malloc == 1) {
        usb_lib_mfree(&__g_usb_device_lib.lib, p_func_hdr);
        p_cfg->p_func_hdr[p_func_info->intf_num] = NULL;
    }
    if (p_func->p_spec_desc != NULL) {
        usb_lib_mfree(&__g_usb_device_lib.lib, p_func->p_spec_desc);
    }
    return ret;
}

/**
 * \brief 设备功能接口反初始化
 */
static int __dev_func_deinit(struct usbd_dev    *p_dev,
                             struct usbd_config *p_cfg,
                             struct usbd_func   *p_func){
    uint8_t intf_num = p_func->intf_desc.interface_number;

    if ((p_cfg->p_func_hdr[intf_num] == NULL) || (p_cfg->p_func_hdr[intf_num]->n_func_alt == 0)) {
        return -USB_EILLEGAL;
    }

    p_cfg->config_desc.total_length -= sizeof(struct usb_interface_desc) + p_func->spec_desc_size;

    __dev_str_idx_free(p_func->intf_desc.i_interface);

    if (p_func->p_spec_desc != NULL) {
        usb_lib_mfree(&__g_usb_device_lib.lib, p_func->p_spec_desc);
    }
    usb_lib_mfree(&__g_usb_device_lib.lib, p_func);

    p_cfg->p_func_hdr[intf_num]->n_func_alt--;

    if (p_cfg->p_func_hdr[intf_num]->n_func_alt == 0) {
        usb_lib_mfree(&__g_usb_device_lib.lib, p_cfg->p_func_hdr[intf_num]);
        p_cfg->p_func_hdr[intf_num] = NULL;
    }
    return USB_OK;
}

/**
 * \brief 创建设备功能接口
 */
static int __dev_func_create(struct usbd_dev       *p_dev,
                             struct usbd_config    *p_cfg,
                             uint8_t                type,
                             struct usbd_func_info *p_func_info,
                             struct usbd_func_opts *p_func_opts,
                             void                  *p_func_arg){
    int               ret;
    struct usbd_func *p_func = NULL;

    p_func = usb_lib_malloc(&__g_usb_device_lib.lib, sizeof(struct usbd_func));
    if (p_func == NULL) {
        return -USB_ENOMEM;
    }
    /* 初始化功能接口*/
    ret = __dev_func_init(p_dev,
                          p_cfg,
                          p_func,
                          type,
                          p_func_info,
                          p_func_opts,
                          p_func_arg);
    if (ret != USB_OK) {
        usb_lib_mfree(&__g_usb_device_lib.lib, p_func);
    }
    return ret;
}

/**
 * \brief 创建设备功能接口
 */
static int __dev_func_destroy(struct usbd_dev    *p_dev,
                              struct usbd_config *p_cfg,
	                          struct usbd_func   *p_func){
    int ret = __dev_func_deinit(p_dev, p_cfg, p_func);
    if (ret != USB_OK) {
        return ret;
    }
    usb_lib_mfree(&__g_usb_device_lib.lib, p_func);

    return USB_OK;
}

/**
 * \brief 创建设备联合功能接口
 */
static int __dev_func_assoc_create(struct usbd_dev             *p_dev,
                                   struct usbd_config          *p_cfg,
								   struct usbd_func_assoc_info *p_func_assoc_info,
                                   struct usbd_func_info       *p_func_info,
						           struct usbd_func_opts       *p_func_opts,
						           void                        *p_func_arg){
    int                     ret;
    struct usbd_func_assoc *p_func_assoc = NULL;

    p_func_assoc = usb_lib_malloc(&__g_usb_device_lib.lib, sizeof(struct usbd_func_assoc));
    if (p_func_assoc == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_func_assoc, 0, sizeof(struct usbd_func_assoc));

    /* 分配获取设备配置接口功能描述符的字符串信息 */
    ret = __dev_str_idx_alloc(&p_func_assoc->intf_assoc_desc.i_function);
    if (ret == USB_OK) {
        if ((p_func_assoc_info->p_intf_str != NULL) && (strlen(p_func_assoc_info->p_intf_str) != 0)) {
            strncpy(p_func_assoc->intf_assoc_str, p_func_assoc_info->p_intf_str, USBD_STRING_LENGTH - 1);
        }
        p_dev->p_str[p_func_assoc->intf_assoc_desc.i_function] = p_func_assoc->intf_assoc_str;
    } else {
    	goto __failed1;
    }

    p_func_assoc->intf_assoc_desc.length             = sizeof(struct usb_interface_assoc_desc);
    p_func_assoc->intf_assoc_desc.descriptor_type    = USB_DT_INTERFACE_ASSOCIATION;
    p_func_assoc->intf_assoc_desc.interface_count    = p_func_assoc_info->intf_count;
    p_func_assoc->intf_assoc_desc.function_class     = p_func_assoc_info->class;
    p_func_assoc->intf_assoc_desc.function_sub_class = p_func_assoc_info->sub_class;
    p_func_assoc->intf_assoc_desc.function_protocol  = p_func_assoc_info->protocol;
    p_func_assoc->intf_assoc_desc.first_interface    = p_func_assoc_info->intf_num_first;

    /* 初始化功能接口*/
    ret = __dev_func_init(p_dev,
                          p_cfg,
						 &p_func_assoc->func,
						  USBD_INTERFACE_ASSOC,
                          p_func_info,
                          p_func_opts,
                          p_func_arg);
    if (ret != USB_OK) {
    	goto __failed2;
    }

    p_cfg->config_desc.total_length += sizeof(struct usb_interface_assoc_desc);

    return USB_OK;
__failed2:
    __dev_str_idx_free(p_func_assoc->intf_assoc_desc.i_function);
__failed1:
    usb_lib_mfree(&__g_usb_device_lib.lib, p_func_assoc);

	return ret;
}

/**
 * \brief 创建设备联合功能接口
 */
static int __dev_func_assoc_destroy(struct usbd_dev        *p_dev,
                                    struct usbd_config     *p_cfg,
                                    struct usbd_func_assoc *p_func_assoc){
    int ret = __dev_func_deinit(p_dev, p_cfg, &p_func_assoc->func);
    if (ret != USB_OK) {
        return ret;
    }
    p_cfg->config_desc.total_length -= sizeof(struct usb_interface_assoc_desc);

	__dev_str_idx_free(p_func_assoc->intf_assoc_desc.i_function);

	usb_lib_mfree(&__g_usb_device_lib.lib, p_func_assoc);

    return USB_OK;
}

/**
 * \brief USB 从机设备接口功能使能
 */
static int __dev_func_enable(struct usbd_func *p_func){
    int                   ret    = USB_OK;
    struct usbd_pipe     *p_pipe = NULL;
    struct usb_list_node *p_node = NULL;
    struct usbd_config   *p_cfg  = p_func->p_cfg;
    struct usb_dc        *p_dc   = p_cfg->p_dev->p_dc;

    /* 使能所有的端点*/
    usb_list_for_each_node(p_node, &p_cfg->pipe_list) {
        p_pipe = usb_container_of(p_node, struct usbd_pipe, node);

        if (p_pipe->p_func_hdr->func_num == p_func->intf_desc.interface_number) {
            ret = usb_dc_ep_enable(p_dc, p_pipe->p_hw);
            if (ret != USB_OK) {
                __USB_ERR_INFO("endpoint 0x%x enable failed(%d)\r\n",  p_pipe->p_hw->ep_addr, ret);
                return ret;
            }
    	}
    }

    /* 调用驱动的接口设置回调函数*/
    if ((p_func->p_opts != NULL) && (p_func->p_opts->p_fn_alt_set != NULL)) {
        ret = p_func->p_opts->p_fn_alt_set(p_func, p_func->p_func_arg);
        if (ret != USB_OK) {
            __USB_ERR_INFO("config %d interface %d alternate setting %d set failed(%d)\r\n",
                    p_cfg->config_desc.configuration_value,
                    p_func->intf_desc.interface_number,
                    p_func->intf_desc.alternate_setting, ret);
            return ret;
        }
    }

    p_func->is_enabled = USB_TRUE;

    return USB_OK;
}

/**
 * \brief 获取 USB 从机设备通讯管道
 *
 * \param[in]  p_dev   USB 从机设备
 * \param[in]  ep_num  端点号
 * \param[in]  ep_dir  端点方向
 * \param[out] p_pipe  返回找到的管道结构体地址
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_pipe_get(struct usbd_dev   *p_dev,
                      uint8_t            ep_num,
                      uint8_t            ep_dir,
                      struct usbd_pipe **p_pipe){
    int                   ret;
#if USB_OS_EN
    int                   ret_tmp;
#endif
    struct usbd_config   *p_cfg      = NULL;
    struct usbd_pipe     *p_pipe_tmp = NULL;
    struct usb_list_node *p_node     = NULL;

    if (p_dev == NULL) {
        return -USB_EINVAL;
    }
    if (p_dev->p_cur_cfg == NULL) {
        return -USB_ENODEV;
    }

    p_cfg = p_dev->p_cur_cfg;

#if USB_OS_EN
    ret = usb_mutex_lock(p_dev->p_lock, USBD_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
	ret = -USB_ENODEV;

    usb_list_for_each_node(p_node, &p_cfg->pipe_list) {
        p_pipe_tmp = usb_container_of(p_node, struct usbd_pipe, node);

        if (p_pipe_tmp->p_hw->ep_addr == (ep_dir | (ep_num & (0xF)))) {
            *p_pipe = p_pipe_tmp;
            ret     = USB_OK;
        }
    }

#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_dev->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 添加 USB 从机设备管道
 *
 * \param[in]  p_dev           USB 从机设备
 * \param[in]  cfg_num         相关的配置编号
 * \param[in]  intf_num        相关的接口编号
 * \param[in]  alt_setting_num 相关的接口备用设置编号
 * \param[in]  p_info          管道信息
 * \param[out] p_ep_addr       返回添加的管道端点地址
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_pipe_add(struct usbd_dev      *p_dev,
                      uint8_t               cfg_num,
                      uint8_t               intf_num,
                      uint8_t               alt_setting_num,
                      struct usbd_pipe_info p_info,
                      uint8_t              *p_ep_addr){
    uint8_t               ep_num;
    int                   ret    = USB_OK;
#if USB_OS_EN
    int                   ret_tmp;
#endif
    struct usbd_func     *p_func = NULL;
    struct usbd_config   *p_cfg  = NULL;
    struct usbd_pipe     *p_pipe = NULL;
    struct usb_list_node *p_node = NULL;

    if ((p_dev == NULL) || (cfg_num == 0) || (cfg_num > USBD_CONFIG_NUM_MAX)) {
        return -USB_EINVAL;
    }
    if ((p_dev->p_dc->is_run == USB_TRUE)  ||
            (p_dev->p_cfgs[cfg_num - 1] == NULL) ||
            (p_dev->p_cfgs[cfg_num - 1]->p_func_hdr[intf_num] == NULL) ||
            (p_dev->p_cfgs[cfg_num - 1]->p_func_hdr[intf_num]->p_func[alt_setting_num] == NULL)) {
        return -USB_EPERM;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_dev->p_lock, USBD_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    p_cfg  = p_dev->p_cfgs[cfg_num - 1];
    ep_num = (p_info.num == 0)? 1 : p_info.num;

    /* 检查端点是否有别的接口在使用*/
    usb_list_for_each_node(p_node, &p_cfg->pipe_list) {
        p_pipe = usb_container_of(p_node, struct usbd_pipe, node);

        if (p_info.num == 0) {
            if (p_pipe->p_hw->ep_addr == ((p_info.dir) | ep_num)) {
                ep_num++;
            }
        } else {
            if (p_pipe->p_hw->ep_addr == ((p_info.dir) | (p_info.num & 0x0F))) {
                /* 同一个配置的不同功能接口使用了同一个端点，返回失败*/
                if (p_pipe->p_func_hdr->func_num != intf_num) {
                    ret = -USB_EAGAIN;
                }
                goto __exit;
            }
        }
    }

    p_pipe = usb_lib_malloc(&__g_usb_device_lib.lib, sizeof(struct usbd_pipe));
    if (p_pipe == NULL) {
        ret = -USB_ENOMEM;
        goto __exit;
    }
    memset(p_pipe, 0, sizeof(struct usbd_pipe));

    /* 分配端点*/
    ret = usb_dc_ep_alloc(p_dev->p_dc,
                        ((p_info.dir) | (ep_num & 0x0F)),
                          p_info.attr & 0x03,
                          p_info.mps,
                         &p_pipe->p_hw);
    if (ret != USB_OK) {
        goto __failed;
    }

#if USB_OS_EN
    /* 创建一个管道信号量*/
    p_pipe->p_sync = usb_lib_sem_create(&__g_usb_device_lib.lib);
    if (p_pipe->p_sync == NULL) {
        __USB_ERR_TRACE(SemCreateErr, "\r\n");
        ret = -USB_EPERM;
        goto __failed;
    }

    /* 创建一个管道信号量*/
    p_pipe->p_lock = usb_lib_mutex_create(&__g_usb_device_lib.lib);
    if (p_pipe->p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        ret = -USB_EPERM;
        goto __failed;
    }
#endif

    /* 添加到 USB 设备配置中*/
    usb_list_node_add_tail(&p_pipe->node, &p_cfg->pipe_list);

    p_func = p_dev->p_cfgs[cfg_num - 1]->p_func_hdr[intf_num]->p_func[alt_setting_num];

    p_pipe->p_func_hdr               = p_dev->p_cfgs[cfg_num - 1]->p_func_hdr[intf_num];
    p_pipe->ep_desc.length           = sizeof(struct usb_endpoint_desc);
    p_pipe->ep_desc.descriptor_type  = USB_DT_ENDPOINT;
    p_pipe->ep_desc.endpoint_address = p_pipe->p_hw->ep_addr;
    p_pipe->ep_desc.max_packet_size  = p_info.mps;
    p_pipe->ep_desc.attributes       = p_info.attr;
    p_pipe->ep_desc.interval         = p_info.inter;
    /* 有可能同一个接口不同的备用设置会用到同一个端点*/
    p_pipe->ref_cnt++;

    p_func->intf_desc.num_endpoints++;
    p_cfg->config_desc.total_length += sizeof(struct usb_endpoint_desc);

    if (p_ep_addr != NULL) {
        *p_ep_addr = p_pipe->p_hw->ep_addr;
    }

    goto __exit;

__failed:
#if USB_OS_EN
    if (p_pipe->p_lock) {
        ret_tmp = usb_lib_mutex_destroy(&__g_usb_device_lib.lib, p_pipe->p_lock);
        if (ret_tmp != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret_tmp);
        }
    }
    if (p_pipe->p_sync) {
        ret_tmp = usb_lib_sem_destroy(&__g_usb_device_lib.lib, p_pipe->p_sync);
        if (ret_tmp != USB_OK) {
            __USB_ERR_TRACE(SemDelErr, "(%d)\r\n", ret_tmp);
        }
    }
#endif
    usb_lib_mfree(&__g_usb_device_lib.lib, p_pipe);

__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_dev->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 删除 USB 从机设备管道
 *
 * \param[in] p_dev           USB 从机设备
 * \param[in] cfg_num         相关的配置编号
 * \param[in] intf_num        相关的接口编号
 * \param[in] alt_setting_num 相关的接口备用设置编号
 * \param[in] pipe_dir        管道方向
 * \param[in] pipe_addr       管道地址
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_pipe_del(struct usbd_dev *p_dev,
                      uint8_t          cfg_num,
                      uint8_t          intf_num,
                      uint8_t          alt_setting_num,
                      uint8_t          pipe_dir,
                      uint8_t          pipe_addr){
	int                   ret;
    struct usbd_func     *p_func = NULL;
    struct usbd_config   *p_cfg  = NULL;
    struct usbd_pipe     *p_pipe = NULL;
    struct usb_list_node *p_node = NULL;
#if USB_OS_EN
    int                   ret_tmp;
#endif

    if ((p_dev == NULL) || (cfg_num == 0) || (cfg_num > USBD_CONFIG_NUM_MAX)) {
        return -USB_EINVAL;
    }

    if ((p_dev->p_dc->is_run == USB_TRUE)  ||
            (p_dev->p_cfgs[cfg_num - 1] == NULL) ||
            (p_dev->p_cfgs[cfg_num - 1]->p_func_hdr[intf_num] == NULL) ||
            (p_dev->p_cfgs[cfg_num - 1]->p_func_hdr[intf_num]->p_func[alt_setting_num] == NULL)) {
        return -USB_EPERM;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_dev->p_lock, USBD_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    p_cfg = p_dev->p_cfgs[cfg_num - 1];

    /* 检查端点是否已经存在*/
    usb_list_for_each_node(p_node, &p_cfg->pipe_list) {
        p_pipe = usb_container_of(p_node, struct usbd_pipe, node);

        if (p_pipe->p_hw->ep_addr == ((pipe_dir) | (pipe_addr & 0x0F))) {
            if (p_pipe->p_func_hdr->func_num == intf_num) {
                goto __find_ep;
            }
        }
    }
    ret = -USB_ENODEV;

    goto __exit;
__find_ep:
    p_pipe->ref_cnt--;

    /* 没有别的接口备用设置用到这个端点了，可以释放了*/
    if (p_pipe->ref_cnt == 0) {
        usb_list_node_del(&p_pipe->node);
#if USB_OS_EN
        ret = usb_lib_sem_destroy(&__g_usb_device_lib.lib, p_pipe->p_sync);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(SemDelErr, "(%d)\r\n", ret);
            goto __exit;
        }
        ret = usb_lib_mutex_destroy(&__g_usb_device_lib.lib, p_pipe->p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
            goto __exit;
        }
#endif
        usb_lib_mfree(&__g_usb_device_lib.lib, p_pipe);
    }

    p_func = p_dev->p_cfgs[cfg_num - 1]->p_func_hdr[intf_num]->p_func[alt_setting_num];

    p_func->intf_desc.num_endpoints--;
    p_cfg->config_desc.total_length -= sizeof(struct usb_endpoint_desc);

__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_dev->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 从机设备添加一个功能接口
 *
 * \param[in] p_dev        要添加接口功能的 USB 从机设备
 * \param[in] config_num   要添加的接口功能的配置编号
 * \param[in] func_info    功能接口信息
 * \param[in] p_func_opts  功能接口操作函数
 * \param[in] p_func_arg   功能接口操作函数参数
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_func_add(struct usbd_dev       *p_dev,
                      uint8_t                config_num,
                      struct usbd_func_info  func_info,
                      struct usbd_func_opts *p_func_opts,
                      void                  *p_func_arg){
    int                 ret;
#if USB_OS_EN
    int                 ret_tmp;
#endif
    struct usbd_config *p_cfg = NULL;

    if ((p_dev == NULL) ||
    		(config_num == 0) ||
			(config_num > USBD_CONFIG_NUM_MAX)) {
        return -USB_EINVAL;
    }
    if ((p_dev->p_dc->is_run == USB_TRUE)  ||
    		(p_dev->p_cfgs[config_num - 1] == NULL)) {
        return -USB_EPERM;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_dev->p_lock, USBD_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_cfg = p_dev->p_cfgs[config_num - 1];

    /* 初始化接口功能*/
    ret = __dev_func_create(p_dev,
                            p_cfg,
							USBD_INTERFACE_NORMAL,
                           &func_info,
                            p_func_opts,
                            p_func_arg);
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_dev->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 从机设备删除一个功能接口
 *
 * \param[in] p_dev        要删除接口功能的 USB 从机设备
 * \param[in] config_num   要删除的接口功能的配置编号
 * \param[in] intf_num     功能接口编号
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_func_del(struct usbd_dev *p_dev, uint8_t config_num, uint8_t intf_num){
    //todo
    return USB_OK;
}

/**
 * \brief USB 从机设备添加联合功能接口
 *
 * \param[in] p_dev           要添加联合接口功能的 USB 从机设备
 * \param[in] cfg_num         要添加联合接口功能的配置编号
 * \param[in] func_assoc_info 联合功能接口信息
 * \param[in] p_func_info     功能接口信息
 * \param[in] p_func_opts     功能接口操作函数
 * \param[in] p_func_arg      功能接口操作函数参数
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_func_assoc_add(struct usbd_dev             *p_dev,
                            uint8_t                      cfg_num,
                            struct usbd_func_assoc_info  func_assoc_info,
                            struct usbd_func_info       *p_func_info,
						    struct usbd_func_opts       *p_func_opts,
						    void                        *p_func_arg){
    int                  i, ret   = USB_OK;
#if USB_OS_EN
    int                  ret_tmp;
#endif
    struct usbd_config  *p_cfg    = NULL;

    if ((p_dev == NULL) ||
    		(cfg_num == 0) ||
			(cfg_num > USBD_CONFIG_NUM_MAX) ||
			(p_func_info == NULL)) {
        return -USB_EINVAL;
    }
    if ((p_dev->p_dc->is_run == USB_TRUE)  ||
    		(p_dev->p_cfgs[cfg_num - 1] == NULL)) {
        return -USB_EPERM;
    }

    p_cfg = p_dev->p_cfgs[cfg_num - 1];

    /* 接口编号必须连续*/
    if (func_assoc_info.intf_num_first > 0) {
        if (p_cfg->p_func_hdr[func_assoc_info.intf_num_first - 1] == NULL) {
        	__USB_ERR_INFO("usb config %d interface %d don't exist\r\n",
        			cfg_num, func_assoc_info.intf_num_first - 1);
            return -USB_EPERM;
        }
    }

    /* 检查是否有存在接口*/
    for (i = 0; i < func_assoc_info.intf_count; i++) {
        if (p_cfg->p_func_hdr[i + func_assoc_info.intf_num_first] != NULL) {
            return -USB_EEXIST;
        }
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_dev->p_lock, USBD_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 初始化所有功能接口*/
    for (i = 0; i < func_assoc_info.intf_count; i++) {
        if (p_func_info[i].intf_num == func_assoc_info.intf_num_first) {
        	/* 创建联合接口的第一个接口*/
            ret = __dev_func_assoc_create(p_dev,
            		                      p_cfg,
									     &func_assoc_info,
                                         &p_func_info[i],
                                          p_func_opts,
                                          p_func_arg);
        } else {
            ret = __dev_func_create(p_dev,
            		                p_cfg,
									USBD_INTERFACE_NORMAL,
                                   &p_func_info[i],
                                    p_func_opts,
                                    p_func_arg);
        }
    }
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_dev->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 从机设备删除联合功能接口
 *
 * \param[in] p_dev          要删除联合接口功能的 USB 从机设备
 * \param[in] cfg_num        要删除联合接口功能的配置编号
 * \param[in] intf_num_first 联合接口功能的第一个接口编号
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_func_assoc_del(struct usbd_dev *p_dev,
                            uint8_t          cfg_num,
                            uint8_t          intf_num_first){
    int                     i, ret        = USB_OK;
    struct usbd_func       *p_func        = NULL;
    struct usbd_func_hdr   *p_func_hdr    = NULL;
    struct usbd_config     *p_cfg         = NULL;
    struct usbd_func_assoc *p_func_assoc  = NULL;
    uint8_t                 n_intf        = 0;
    uint8_t                 is_assoc_find = 0;

    if ((p_dev == NULL) ||
    		(cfg_num == 0) ||
			(cfg_num > USBD_CONFIG_NUM_MAX)) {
        return -USB_EINVAL;
    }
    if ((p_dev->p_dc->is_run == USB_TRUE)  ||
    		(p_dev->p_cfgs[cfg_num - 1] == NULL)) {
        return -USB_EPERM;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_dev->p_lock, USBD_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_cfg = p_dev->p_cfgs[cfg_num - 1];

    /* 删除联合功能接口*/
    for (i = 0; i < p_cfg->n_func; i++) {
        p_func_hdr = p_cfg->p_func_hdr[i];

        if (p_func_hdr->func_num == intf_num_first) {
            p_func_assoc = (struct usbd_func_assoc *)(p_func_hdr->p_func[0]);

            n_intf = p_func_assoc->intf_assoc_desc.interface_count;

            ret = __dev_func_assoc_destroy(p_dev, p_cfg, p_func_assoc);
            if (ret != USB_OK) {
                return ret;
            }
            p_func_hdr->p_func[0] = NULL;

            is_assoc_find = 1;
        }
    }
    /* 删除联合的其他接口*/
    if (is_assoc_find == 1) {
        for (i = 1; i < n_intf; i++) {
            p_func = p_func_hdr->p_func[intf_num_first + i];

            ret = __dev_func_destroy(p_dev, p_cfg, p_func);
            if (ret != USB_OK) {
                return ret;
            }
            p_func_hdr->p_func[intf_num_first + i] = NULL;
        }
    } else {
    	ret = -USB_ENODEV;
    }
    return ret;
}

/**
 * \brief USB 从机设备添加一个配置
 *
 * \param[in] p_dev      要添加配置的 USB 从机设备
 * \param[in] p_cfg_str  配置字符串，可为空
 * \param[in] p_cfg_num  存储添加成功的配置编号
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_cfg_add(struct usbd_dev *p_dev, char *p_cfg_str, uint8_t *p_cfg_num){
    int                 ret   = USB_OK;
#if USB_OS_EN
    int                 ret_tmp;
#endif
    struct usbd_config *p_cfg = NULL;

    if ((p_dev == NULL) || (p_cfg_num == NULL)) {
        return -USB_EINVAL;
    }
    /* 控制器正在运行，或配置已存在*/
    if ((p_dev->p_dc->is_run == USB_TRUE) ||
            (p_dev->dev_desc.num_configurations == USBD_CONFIG_NUM_MAX)) {
        return -USB_EPERM;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_dev->p_lock, USBD_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_cfg = usb_lib_malloc(&__g_usb_device_lib.lib, sizeof(struct usbd_config));
    if (p_cfg == NULL) {
        ret = -USB_ENOMEM;
        goto __exit;
    }
    memset(p_cfg, 0, sizeof(struct usbd_config));

    /* 分配获取设备配置描述符的字符串信息 */
    ret = __dev_str_idx_alloc(&p_cfg->config_desc.i_configuration);
    if (ret == USB_OK) {
        if (p_cfg_str != NULL) {
            strncpy(p_cfg->config_str, p_cfg_str, USBD_STRING_LENGTH - 1);
        }
        p_dev->p_str[p_cfg->config_desc.i_configuration] = p_cfg->config_str;
    } else {
    	usb_lib_mfree(&__g_usb_device_lib.lib, p_cfg);
        goto __exit;
    }

    p_dev->p_cfgs[p_dev->dev_desc.num_configurations] = p_cfg;
    p_cfg->p_dev                           = p_dev;
    /* 填充配置描述符*/
    p_cfg->config_desc.descriptor_type     = USB_DT_CONFIG;
    p_cfg->config_desc.length              = sizeof(struct usb_config_desc);
    p_cfg->config_desc.configuration_value = p_dev->dev_desc.num_configurations + 1;
    p_cfg->config_desc.num_interfaces      = 0;
    p_cfg->config_desc.total_length        = sizeof(struct usb_config_desc);
    p_cfg->config_desc.attributes          = (1 << 7) | (1 << 6);
    p_cfg->config_desc.max_power           = 0x32;

    /* 初始化端点管道链表*/
    usb_list_head_init(&p_cfg->pipe_list);
    /* 修改配置数量*/
    p_cfg->p_dev->dev_desc.num_configurations++;

    *p_cfg_num = p_cfg->p_dev->dev_desc.num_configurations;

__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_dev->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 从机设备删除一个配置
 *
 * \param[in] p_dev   要删除配置的 USB 从机设备
 * \param[in] cfg_num 要删除的配置的编号
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_config_del(struct usbd_dev *p_dev, uint8_t cfg_num){
    int                   ret        = USB_OK;
    uint8_t               i, j;
    struct usbd_config   *p_cfg      = NULL;
    struct usbd_pipe     *p_pipe     = NULL;
    struct usb_list_node *p_node     = NULL;
    struct usb_list_node *p_node_tmp = NULL;
#if USB_OS_EN
    int                   ret_tmp;
#endif

    if ((cfg_num == 0) || (cfg_num > USBD_CONFIG_NUM_MAX)) {
        return -USB_EINVAL;
    }
    if (p_dev->p_dc->is_run == USB_TRUE) {
        return -USB_EPERM;
    }
    if (p_dev->p_cfgs[cfg_num - 1] == NULL) {
        return -USB_ENODEV;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_dev->p_lock, USBD_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_cfg = p_dev->p_cfgs[cfg_num - 1];
    for (i = 0; i < USBD_INTF_NUM_MAX; i++) {
        if (p_cfg->p_func_hdr[i] != NULL) {
            struct usbd_func_hdr *p_func_hdr = p_cfg->p_func_hdr[i];

            for (j = 0; j < USBD_INTF_ALT_NUM_MAX; j++) {
                if (p_func_hdr->p_func[j] != NULL) {
                    ret = __dev_func_deinit(p_dev, p_cfg, p_func_hdr->p_func[j]);
                    if (ret != USB_OK) {
                        goto __exit;
                    }
                }
            }
            /* 删除关联的所有的管道*/
            usb_list_for_each_node_safe(p_node, p_node_tmp, &p_cfg->pipe_list) {
                p_pipe = usb_container_of(p_node, struct usbd_pipe, node);

                usb_list_node_del(&p_pipe->node);
#if USB_OS_EN
                ret = usb_lib_sem_destroy(&__g_usb_device_lib.lib, p_pipe->p_sync);
                if (ret != USB_OK) {
                    __USB_ERR_TRACE(SemDelErr, "(%d)\r\n", ret);
                    goto __exit;
                }
                ret = usb_lib_mutex_destroy(&__g_usb_device_lib.lib, p_pipe->p_lock);
                if (ret != USB_OK) {
                    __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
                    goto __exit;
                }
#endif
                usb_lib_mfree(&__g_usb_device_lib.lib, p_pipe);
            }
    	}
    }
    p_dev->p_cfgs[cfg_num - 1] = NULL;

    usb_lib_mfree(&__g_usb_device_lib.lib, p_cfg);
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_dev->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 释放 USB 从机设备
 */
static int __dev_release(struct usbd_dev *p_dev){
#if USB_OS_EN
    int ret;
#endif
    if (p_dev->dev_desc.i_product != 0xFF) {
        __dev_str_idx_free(p_dev->dev_desc.i_product);
    }
    if (p_dev->dev_desc.i_manufacturer != 0xFF) {
        __dev_str_idx_free(p_dev->dev_desc.i_manufacturer);
    }
    if (p_dev->dev_desc.i_serial_number != 0xFF) {
        __dev_str_idx_free(p_dev->dev_desc.i_serial_number);
    }
    if (p_dev->i_mircosoft_os != 0xFF) {
        __dev_str_idx_free(p_dev->i_mircosoft_os);
    }
#if USB_OS_EN
    if (p_dev->p_lock) {
        ret = usb_lib_mutex_destroy(&__g_usb_device_lib.lib, p_dev->p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
            return ret;
        }
    }
#endif
    usb_lib_mfree(&__g_usb_device_lib.lib, p_dev);

    return USB_OK;
}

/**
 * \brief 创建一个 USB 从机设备
 *
 * \param[in]  p_dc     USB 从机控制器
 * \param[in]  p_name   USB 设备名字
 * \param[in]  dev_desc USB 设备描述符
 * \param[out] p_dev    返回创建成功的 USB 从机设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_create(struct usb_dc         *p_dc,
                    char                  *p_name,
                    struct usbd_dev_desc   dev_desc,
                    struct usbd_dev      **p_dev){
    int              ret;
    struct usbd_dev *p_dev_tmp = NULL;

    if ((p_dc == NULL) || (p_name == NULL)) {
        return -USB_EINVAL;
    }

    if (strlen(p_name) > (USB_NAME_LEN - 1)) {
        return -USB_EILLEGAL;
    }

    if (p_dc->is_run == USB_TRUE) {
        return -USB_EPERM;
    }

    p_dev_tmp = usb_lib_malloc(&__g_usb_device_lib.lib, sizeof(struct usbd_dev));
    if (p_dev_tmp == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_dev_tmp, 0, sizeof(struct usb_dc));

    p_dev_tmp->dev_desc.i_product       = 0xFF;
    p_dev_tmp->dev_desc.i_manufacturer  = 0xFF;
    p_dev_tmp->dev_desc.i_serial_number = 0xFF;
    p_dev_tmp->i_mircosoft_os           = 0xFF;
#if USB_OS_EN
    p_dev_tmp->p_lock = usb_lib_mutex_create(&__g_usb_device_lib.lib);
    if (p_dev_tmp->p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        ret = -USB_EPERM;
        goto __failed1;
    }
#endif

    strcpy(p_dev_tmp->name, p_name);
    /* 分配获取设备描述符的字符串信息 */
    ret = __dev_str_idx_alloc(&p_dev_tmp->dev_desc.i_product);
    if (ret == USB_OK) {
        if (dev_desc.p_product != NULL) {
            strncpy(p_dev_tmp->pd_str, dev_desc.p_product, sizeof(p_dev_tmp->pd_str) - 1);
        }
        p_dev_tmp->p_str[p_dev_tmp->dev_desc.i_product] = p_dev_tmp->pd_str;
    } else {
        goto __failed1;
    }

    ret = __dev_str_idx_alloc(&p_dev_tmp->dev_desc.i_manufacturer);
    if (ret == USB_OK) {
        if (dev_desc.p_manufacturer != NULL) {
            strncpy(p_dev_tmp->mf_str, dev_desc.p_manufacturer, sizeof(p_dev_tmp->mf_str) - 1);
        }
        p_dev_tmp->p_str[p_dev_tmp->dev_desc.i_manufacturer] = p_dev_tmp->mf_str;
    } else {
        goto __failed1;
    }

    ret = __dev_str_idx_alloc(&p_dev_tmp->dev_desc.i_serial_number);
    if (ret == USB_OK) {
        if (dev_desc.p_serial != NULL) {
            strncpy(p_dev_tmp->sn_str, dev_desc.p_serial, sizeof(p_dev_tmp->sn_str) - 1);
        }
        p_dev_tmp->p_str[p_dev_tmp->dev_desc.i_serial_number] = p_dev_tmp->sn_str;
    } else {
        goto __failed1;
    }

    ret = __dev_str_idx_alloc(&p_dev_tmp->i_mircosoft_os);
    if (ret == USB_OK) {
        if (dev_desc.p_mircosoft_os_str != NULL) {
            strncpy(p_dev_tmp->mircosoft_os_str, dev_desc.p_mircosoft_os_str, sizeof(p_dev_tmp->mircosoft_os_str) - 1);
        }
        p_dev_tmp->p_str[p_dev_tmp->i_mircosoft_os] = p_dev_tmp->mircosoft_os_str;
    } else {
        goto __failed1;
    }

    /* 填充设备描述符*/
    p_dev_tmp->p_dc = p_dc;
    p_dev_tmp->dev_desc.length           = sizeof(struct usb_device_desc);
    p_dev_tmp->dev_desc.descriptor_type  = USB_DT_DEVICE;
    p_dev_tmp->dev_desc.device_class     = dev_desc.clss;
    p_dev_tmp->dev_desc.device_sub_class = dev_desc.sub_clss;
    p_dev_tmp->dev_desc.device_protocol  = dev_desc.protocol;
    p_dev_tmp->dev_desc.id_product       = dev_desc.pid;
    p_dev_tmp->dev_desc.id_vendor        = dev_desc.vid;
    p_dev_tmp->dev_desc.bcd_device       = dev_desc.bcd;
    p_dev_tmp->dev_desc.max_packet_size0 = dev_desc.ep0_mps;
    p_dev_tmp->dev_desc.bcdUSB           = USB_BCD_VERSION;

    ret = usb_dc_dev_add(p_dc, p_dev_tmp);
    if (ret != USB_OK) {
    	goto __failed1;
    }
    /* 引用 USB 从机控制器*/
    ret = usb_dc_ref_get(p_dc);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
    	goto __failed2;
    }

    p_dc->p_usbd_dev = p_dev_tmp;
    *p_dev           = p_dev_tmp;

    return USB_OK;
__failed2:
    usb_dc_dev_del(p_dc, p_dev_tmp);
__failed1:
    __dev_release(p_dev_tmp);

    return ret;
}

/**
 * \brief 销毁 USB 从机设备
 *
 * \param[in] p_dev 要销毁的 USB 从机设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_destroy(struct usbd_dev *p_dev){
    int     ret;
    uint8_t i, n_cfg;

    if (p_dev == NULL) {
        return USB_OK;
    }
    /* 控制器运行中，不允许这个时候销毁设备*/
    if (p_dev->p_dc->is_run == USB_TRUE) {
        return -USB_EPERM;
    }

    n_cfg = p_dev->dev_desc.num_configurations;
    for (i = 1; i <= n_cfg; i++) {
    	ret = usbd_dev_config_del(p_dev, i);
        if (ret != USB_OK) {
            __USB_ERR_INFO("usb device \"%s\" configuration %d delete failed(%d)\r\n", p_dev->name, i, ret);
            return ret;
    	}
    }

    usb_dc_dev_del(p_dev->p_dc, p_dev);

    __dev_release(p_dev);

    return USB_OK;
}

/**
 * \brief 设置 USB 从机设备驱动数据
 *
 * \param[in] p_dev      USB 从机控制器设备
 * \param[in] p_drv_data 要设置的驱动数据
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_drv_data_set(struct usbd_dev *p_dev, void *p_drv_data){
    int ret = USB_OK;

    if (p_dev == NULL) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_dev->p_lock, USB_DC_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_dev->p_drv_data = p_drv_data;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_dev->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 获取 USB 从机设备驱动数据
 *
 * \param[in]  p_dev      USB 从机控制器设备
 * \param[out] p_drv_data 返回获取到的驱动数据
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_drv_data_get(struct usbd_dev *p_dev, void **p_drv_data){
    int ret = USB_OK;

    if (p_dev == NULL) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_dev->p_lock, USBD_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_drv_data = p_dev->p_drv_data;
#if USB_OS_EN
    ret = usb_mutex_unlock(p_dev->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 填充请求包
 */
static void __dev_trans_fill(struct usbd_trans *p_trans,
                             struct usbd_pipe  *p_pipe,
                             uint8_t           *p_buf,
                             uint32_t           len,
                             int                flag,
                             void             (*p_fn_complete)(void *p_arg),
                             void              *p_arg){
    p_trans->p_hw          = p_pipe->p_hw;
    p_trans->p_buf         = p_buf;
    p_trans->len           = len;
    p_trans->flag          = flag;
    p_trans->p_fn_complete = p_fn_complete;
    p_trans->p_arg         = p_arg;
    p_trans->act_len       = 0;
    p_trans->status        = 0;
}

/**
 * \brief 同步传输完成回调函数
 */
static void __trans_complete(void *p_arg){
	struct usbd_pipe *p_pipe = (struct usbd_pipe *)p_arg;

#if USB_OS_EN
    usb_sem_give((usb_sem_handle_t)(p_pipe->p_sync));
#else
    p_pipe->sync = 1;
#endif
}

/**
 * \brief USB 设备异步传输函数
 *
 * \param[in] p_dev   USB 从机设备
 * \param[in] p_trans USB 传输请求
 *
 * \return[in] 成功返回 USB_OK
 */
int usbd_dev_trans_async(struct usbd_dev   *p_dev,
                         struct usbd_trans *p_trans){
    int  ret;
#if USB_OS_EN
    int  ret_tmp;
#endif

    /* 检查参数数据结构合法性*/
    if ((p_dev == NULL) || (p_trans == NULL)) {
        return -USB_EINVAL;
    }
#if USB_OS_EN
    ret = usb_mutex_lock(p_dev->p_lock, USBD_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_dev->p_dc->is_run == USB_FALSE) {
        ret = -USB_EPERM;
        goto __exit;
    }

    /* 调用 USB 设备传输请求*/
    ret = usb_dc_trans_req(p_dev->p_dc, p_trans);
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_dev->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief USB 设备同步传输函数
 *
 * \param[in] p_dev     USB 从机设备
 * \param[in] p_pipe    使用的 USB 管道
 * \param[in] p_buf     数据缓存
 * \param[in] len       数据长度
 * \param[in] flag      标志
 * \param[in] timeout   超时时间
 * \param[in] p_act_len 存储实际传输的数据长度缓存
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_trans_sync(struct usbd_dev  *p_dev,
                        struct usbd_pipe *p_pipe,
                        uint8_t          *p_buf,
                        uint32_t          len,
                        int               flag,
                        int               timeout,
                        uint32_t         *p_act_len){
    struct usbd_trans trans;
    int               ret;
#if !USB_OS_EN
    int               timeout_tmp = timeout;
#else
    int               ret_tmp;
#endif

    /* 检查参数合法性*/
    if ((p_buf == NULL) || (p_pipe == NULL) || (p_act_len == NULL)) {
        return -USB_EINVAL;
    }

    *p_act_len = 0;
#if USB_OS_EN
    ret = usb_mutex_lock(p_pipe->p_lock, USBD_DEV_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 填充传输请求*/
    __dev_trans_fill(&trans,
                      p_pipe,
                      p_buf,
                      len,
                      flag,
                    __trans_complete,
                      p_pipe);

#if USB_OS_EN
    usb_sem_take(p_pipe->p_sync, USB_NO_WAIT);
#else
    p_pipe->sync = 0;
#endif
    /* 调用 USB 设备异步传输*/
    ret = usbd_dev_trans_async(p_dev, &trans);
    if (ret != USB_OK) {
        goto __exit;
    }

    /* 等待传输完成*/
    while (1) {
#if USB_OS_EN
        /* 等待信号量*/
        ret = usb_sem_take(p_pipe->p_sync, timeout);
#else
        while (p_pipe->sync != 1) {
            if (timeout_tmp > 0) {
                timeout_tmp--;
                usb_mdelay(1);
            }

            if (timeout_tmp == 0) {
                ret = -USB_ETIME;
                break;
            }
        }
#endif
        if (ret == USB_OK) {
            if (trans.status == USB_OK) {
                /* 传输完成返回实际传输的数据长度*/
                *p_act_len = trans.act_len;
                ret        = USB_OK;
            } else {
                /* 传输失败返回错误状态*/
                ret =  trans.status;
            }
            goto __exit;
        } else {
            /* 取消 USB 设备传输*/
            usb_dc_trans_cancel(p_dev->p_dc, &trans);
            goto __exit;
        }
    }
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_pipe->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret_tmp;
    }
#endif
    return ret;
}

/**
 * \brief 清除特性
 */
static int __feature_clr(struct usbd_dev *p_dev,
                         uint16_t         value,
                         uint16_t         index){
    struct usbd_ep *p_ep = NULL;
    int             ret  = USB_OK;

    /* 寻找对应的端点*/
    ret = usb_dc_ep_get(p_dev->p_dc, index, &p_ep);
    if (ret != USB_OK) {
        return ret;
    }

    if (value == USB_DEV_REMOTE_WAKEUP) {
        //todo
    } else if (value == USB_ENDPOINT_HALT) {
        /* 清除端点停止状态*/
        ret = usb_dc_ep_halt_set(p_dev->p_dc, p_ep, USB_FALSE);
    }

    return ret;
}

/**
 * \brief 设置特性
 */
static int __feature_set(struct usbd_dev *p_dev,
                         uint16_t         value,
                         uint16_t         index){
    struct usbd_ep *p_ep = NULL;
    int             ret  = USB_OK;

    ret = usb_dc_ep_get(p_dev->p_dc, index, &p_ep);
    if (ret != USB_OK) {
        return ret;
    }

    if (value == USB_DEV_REMOTE_WAKEUP) {
        //todo
    } else if (value == USB_ENDPOINT_HALT) {
        ret = usb_dc_ep_halt_set(p_dev->p_dc, p_ep, USB_TRUE);
    }

    return ret;
}

/**
 * \brief 设置设备地址
 *
 * \param[in] p_dc   USB 从机控制器
 * \param[in] value  要设置的地址值
 *
 * \retval 成功返回 USB_OK
 */
static int __address_set(struct usb_dc *p_dc, uint16_t value){
    if (p_dc->p_dc_drv->p_fn_addr_set == NULL) {
        return -USB_ENOTSUP;
    }

    return p_dc->p_dc_drv->p_fn_addr_set(p_dc->p_dc_drv_arg, value);
}

/**
 * \brief \brief 填充接口及端点描述符
 */
static int __usb_intf_desc_fill(struct usbd_func *p_func,
                                uint8_t          *p_buf,
                                uint32_t          buf_len,
                                uint32_t         *p_len){
    struct usbd_pipe     *p_pipe        = NULL;
    struct usb_list_node *p_node        = NULL;
    struct usbd_config   *p_cfg         = p_func->p_cfg;
    uint8_t              *p_buf_tmp     = p_buf;
    uint32_t              len           = 0;
    uint8_t               intf_desc_len = sizeof(struct usb_interface_desc);
    uint8_t               ep_desc_len   = sizeof(struct usb_endpoint_desc);

    memcpy(p_buf_tmp, (uint8_t *)&p_func->intf_desc, intf_desc_len);

    p_buf_tmp = p_buf_tmp + intf_desc_len;
    len      += intf_desc_len;

    if ((p_func->spec_desc_size > 0) &&
            (p_func->p_spec_desc != NULL) &&
            ((buf_len - len) >= p_func->spec_desc_size)) {
        memcpy(p_buf_tmp, p_func->p_spec_desc, p_func->spec_desc_size);
        p_buf_tmp = p_buf_tmp + p_func->spec_desc_size;
        len += p_func->spec_desc_size;
    }

    usb_list_for_each_node(p_node, &p_cfg->pipe_list) {
        p_pipe = usb_container_of(p_node, struct usbd_pipe, node);

        if (p_pipe->p_func_hdr->func_num != p_func->intf_desc.interface_number) {
            continue;
        }
        if ((buf_len - len) < ep_desc_len) {
            return -USB_ESIZE;
        }

        memcpy(p_buf_tmp, (uint8_t *)&p_pipe->ep_desc, ep_desc_len);

        len       += ep_desc_len;
        p_buf_tmp += ep_desc_len;
    }

    *p_len = len;

    return USB_OK;
}

/**
 * \brief 获取设备描述符
 *
 * \param[in] p_dev   相关的 USB 从机设备
 * \param[in] p_setup Setup 包
 * \param[in] p_req   传输清求
 *
 * \retval 成功返回 USB_OK
 */
static int __device_descriptor_get(struct usbd_dev    *p_dev,
                                   struct usb_ctrlreq *p_setup,
                                   struct usbd_trans  *p_req){
    uint32_t size;
    uint32_t desc_len = sizeof(struct usb_device_desc);

    /* 检查长度是否合法*/
    size = (p_setup->length > desc_len) ? desc_len : p_setup->length;
    if (size > p_dev->p_dc->ep0_req_size) {
        return -USB_ESIZE;
    }
    p_req->len = size;

    memcpy(p_req->p_buf, &p_dev->dev_desc, p_req->len);

    return USB_OK;
}

/**
 * \brief 制作完整的配置描述符
 *
 * \param[in] p_cur_cfg 相关的 USB 从机设备配置
 * \param[in] p_buf     存放配置描述符的缓存
 *
 * \retval 成功返回 USB_OK
 */
static int __cfg_desc_make(struct usbd_config *p_cur_cfg,
                           uint8_t             speed,
                           void               *p_buf){
    uint8_t  *p_buf_tmp  = p_buf;
    int       i, j, ret     = USB_OK;
    uint32_t  len_use, len;

    /* 复制配置描述符*/
    memcpy(p_buf_tmp, (uint8_t *)&p_cur_cfg->config_desc, sizeof(struct usb_config_desc));

    p_buf_tmp += sizeof(struct usb_config_desc);
    /* 计算缓存剩余长度*/
    len = p_cur_cfg->p_dev->p_dc->ep0_req_size - sizeof(struct usb_config_desc);

    for (i = 0; i < p_cur_cfg->n_func; i++) {
    	uint8_t desc_len = sizeof(struct usb_interface_assoc_desc);
        /* 剩余长度至少要一个接收描述符的长度*/
        if (len < sizeof(struct usb_interface_desc)) {
            return -USB_ESIZE;
        }

        if (p_cur_cfg->p_func_hdr[i]->type == USBD_INTERFACE_ASSOC) {
        	struct usbd_func_assoc *p_func_assoc =
        			(struct usbd_func_assoc *)p_cur_cfg->p_func_hdr[i]->p_func[0];

        	/* 先填充联合接口描述符*/
        	memcpy(p_buf_tmp, (uint8_t *)&p_func_assoc->intf_assoc_desc, desc_len);
        	len       -= desc_len;
        	p_buf_tmp += desc_len;
        }

    	for (j = 0;j < p_cur_cfg->p_func_hdr[i]->n_func_alt; j++) {
        	/* 填充接口描述符*/
        	ret = __usb_intf_desc_fill(p_cur_cfg->p_func_hdr[i]->p_func[j],
        			                   p_buf_tmp,
									   len,
                                      &len_use);
            if (ret < 0) {
                return ret;
            }

            len       -= len_use;
            p_buf_tmp += len_use;

    	}
    }

    return USB_OK;
}

/**
 * \brief 获取配置描述符
 *
 * \param[in] p_dev   相关的 USB 从机设备
 * \param[in] p_setup Setup 包
 * \param[in] p_req   传输清求
 *
 * \retval 成功返回 USB_OK
 */
static int __cfg_descriptor_get(struct usbd_dev    *p_dev,
                                struct usb_ctrlreq *p_setup,
                                struct usbd_trans  *p_req){
    uint8_t                 speed      = p_dev->p_dc->speed;
    uint8_t                 type       = p_setup->value >> 8;
    uint8_t                 cfg_value  = 0;
    struct usb_config_desc *p_cfg_desc = NULL;
    uint32_t                size       = 0;
    int                     ret        = -USB_EBADF;

    if ((type == USB_DT_OTHER_SPEED_CONFIG) && (speed == USB_SPEED_HIGH)) {
        speed = USB_SPEED_FULL;
    }

    cfg_value = (p_setup->value & 0xff);

    if (cfg_value >= USBD_CONFIG_NUM_MAX) {
         return -USB_EFAULT;
    }

    ret = __cfg_desc_make(p_dev->p_cfgs[cfg_value], speed, p_req->p_buf);
    if (ret == USB_OK) {
        p_cfg_desc = (struct usb_config_desc *)p_req->p_buf;

        size = (p_setup->length > p_cfg_desc->total_length) ?
                p_cfg_desc->total_length : p_setup->length;

        p_req->len     = size;
        p_req->act_len = 0;
    }

    return ret;
}

/**
 * \brief 获取字符串描述符
 */
static int __string_descriptor_get(struct usbd_dev    *p_dev,
                                   struct usb_ctrlreq *p_setup,
                                   struct usbd_trans  *p_req){

    struct usb_string_desc str_desc;
    uint8_t                idx;
    uint32_t               len, len_ret;

    str_desc.type = USB_DT_STRING;
    idx = p_setup->value & 0xFF;

    /* 获取语言 ID*/
    if (idx == 0) {
        str_desc.length    = 4;
        str_desc.string[0] = 0x09;
        str_desc.string[1] = 0x04;
    } else {
        if (idx == 0xEE) {
            idx = p_dev->i_mircosoft_os;
        }

        if (__dev_str_idx_chk(idx) != USB_OK) {
            __USB_ERR_INFO("unknown string index\r\n");
            usb_dc_ep0_stall_set(p_dev->p_dc, USB_TRUE);
        } else {
            len = min((p_req->len - 2) >> 1, strlen(p_dev->p_str[idx]));

            len_ret = usb_utf8s_to_utf16s((const uint8_t*)(p_dev->p_str[idx]),
                                           len,
                                          (uint16_t *)&str_desc.string);

            if (len_ret == 0) {
            	len_ret = 2;
            }

            str_desc.length = len_ret * 2 + 2;

            if (idx == p_dev->i_mircosoft_os) {
                str_desc.string[len * 2]     = p_dev->vendor_code;
                str_desc.string[len * 2 + 1] = 0x00;
                str_desc.length += 2;
            }
        }
    }

    if (p_setup->length > str_desc.length) {
        len = str_desc.length;
    } else {
        len = p_setup->length;
    }

    if (len > p_dev->p_dc->ep0_req_size) {
        __USB_ERR_INFO("ep0 buffer not enough space\n");
        return -USB_ENOMEM;
    }

    p_req->len = len;

    memcpy(p_req->p_buf, &str_desc, p_req->len);

    return USB_OK;
}

/**
 * \brief
 */
static int __qualifier_descriptor_get(struct usbd_dev    *p_dev,
                                      struct usb_ctrlreq *p_setup,
                                      struct usbd_trans  *p_req){

    int ret = USB_OK;

    if (p_dev->p_dc->device_is_hs) {
        p_req->len = sizeof(struct usb_qualifier_desc);

        //__config_qualifier(p_dev, &__g_dev_qualifier, p_req->p_buf);
    } else {
        usb_dc_ep0_stall_set(p_dev->p_dc, USB_TRUE);
        return -USB_ENOTSUP;
    }

    return ret;
}

/**
 * \brief 获取描述符
 *
 * \param[in] p_dev   相关的 USB 从机设备
 * \param[in] p_setup Setup 包
 * \param[in] p_req   传输清求
 *
 * \retval 成功返回 USB_OK
 */
static int __descriptor_get(struct usbd_dev    *p_dev,
                            struct usb_ctrlreq *p_setup,
                            struct usbd_trans  *p_req){
    int ret = USB_OK;

    if (p_setup->request_type == USB_DIR_IN) {
        switch(p_setup->value >> 8) {
            /* 获取设备描述符*/
            case USB_DT_DEVICE:
                __USB_INFO("get usb device descriptor\r\n");
                ret = __device_descriptor_get(p_dev, p_setup, p_req);
                break;
            case USB_DT_CONFIG:
                __USB_INFO("get usb configure descriptor\r\n");
                ret = __cfg_descriptor_get(p_dev, p_setup, p_req);
                break;
            case USB_DT_STRING:
                __USB_INFO("get usb device srting %d descriptor\r\n", (p_setup->value & 0xFF));
                ret = __string_descriptor_get(p_dev, p_setup, p_req);
                break;
            case USB_DT_DEVICE_QUALIFIER:
                /**
                 * 如果一个只支持全速的设备（设备描述符版本号等于 0200H），接收一个“获取设备限定描述符（device_qualifier）”请求，它会响应错误请求。
                 * 主机不能设置一个 other_speed_configuration 请求，除非它成功检索到“设备限定描述符（device_qualifier）”
                 */
                if (p_dev->p_dc->device_is_hs) {
                    ret = __qualifier_descriptor_get(p_dev, p_setup, p_req);
                } else {
                    /* 如果是全速则不支持限定描述符 */
                    usb_dc_ep0_stall_set(p_dev->p_dc, USB_TRUE);
                    ret = -USB_ENOTSUP;
                }
                break;
            case USB_DT_OTHER_SPEED_CONFIG:
                /* 如果要获取其它速度，则当前控制器必须支持高速模式及以上 */
                ret = __cfg_descriptor_get(p_dev, p_setup, p_req);
                break;
            case USB_DT_BOS:
                p_req->len = p_dev->p_mircosoft_os_desc->p_compat_desc[2] | (p_dev->p_mircosoft_os_desc->p_compat_desc[3] << 8);
                p_req->len = p_req->len > p_setup->length  ? p_setup->length : p_req->len;
                memcpy(p_req->p_buf, p_dev->p_mircosoft_os_desc->p_compat_desc, p_req->len);
                break;
            default:
                __USB_ERR_INFO("setup value %d unknown\r\n", (p_setup->value >> 8));
                usb_dc_ep0_stall_set(p_dev->p_dc, USB_TRUE);
                ret = -USB_ENOTSUP;
                break;
        }
    } else {
        usb_dc_ep0_stall_set(p_dev->p_dc, USB_TRUE);
        ret = -USB_ENOTSUP;
    }

    return ret;
}

/**
 * \brief 获取当前配置编号
 */
static int __cur_cfg_value_get(struct usbd_dev    *p_dev,
                               struct usb_ctrlreq *p_setup,
                               struct usbd_trans  *p_req){
    if (p_dev->p_cur_cfg == NULL) {
        return -USB_EILLEGAL;
    }

    /* 检查设备状态*/
    if (p_dev->status == USB_DEV_STATE_CONFIGURED) {
        /* 获取当前的配置编号 */
        p_req->p_buf[0] = p_dev->p_cur_cfg->config_desc.configuration_value;
        p_req->len      = 1;
    } else {
        return -USB_ENODEV;
    }

    return USB_OK;
}

/**
 * \brief 设置当前配置编号
 */
static int __cur_cfg_value_set(struct usbd_dev *p_dev, uint8_t value){
    if (p_dev->p_dc->p_dc_drv->p_fn_config_set == NULL) {
        return -USB_EILLEGAL;
    }

    return p_dev->p_dc->p_dc_drv->p_fn_config_set(p_dev->p_dc->p_dc_drv_arg, value);
}

/**
 * \brief 设置配置
 */
static int __setup_cfg_set(struct usbd_dev    *p_dev,
                           struct usb_ctrlreq *p_setup){
    int                   ret, i;
    struct usbd_config   *p_cfg      = NULL;


    if ((p_setup->value > p_dev->dev_desc.num_configurations) || (p_setup->value == 0)) {
        usb_dc_ep0_stall_set(p_dev->p_dc, USB_TRUE);
        return -USB_EILLEGAL;
    }

    p_cfg = p_dev->p_cfgs[p_setup->value - 1];

    /* 设置当前配置 */
    ret = __cur_cfg_value_set(p_dev, p_setup->value);
    if (ret != USB_OK) {
        __USB_ERR_INFO("config %d set failed(%d)\r\n", p_setup->value, ret);
        return ret;
    }
    p_dev->p_cur_cfg = p_cfg;

    /* 使能当前配置的所有接口的备用设置 0 */
    for (i = 0; i < p_cfg->n_func; i++) {
        ret = __dev_func_enable(p_cfg->p_func_hdr[i]->p_func[0]);
        if (ret != USB_OK) {
            __USB_ERR_INFO("config %d func %d enable failed(%d)\r\n",
                    p_setup->value, i, ret);
            return ret;
        }
#if 0
        if (p_cfg->p_func_hdr[i]->type == USBD_INTERFACE_ASSOC) {
            struct usbd_func_assoc *p_func_assoc =
                    (struct usbd_func_assoc *)p_cfg->p_func_hdr[i]->p_func[0];

            i += p_func_assoc->intf_assoc_desc.interface_count - 1;
        }
#endif
    }

    p_dev->status = USB_DEV_STATE_CONFIGURED;

    return USB_OK;
}

/**
 * \brief 获取接口编号
 */
static int __interface_get(struct usbd_dev    *p_dev,
                           struct usb_ctrlreq *p_setup,
                           struct usbd_ep0    *p_ep0){
    int               ret;
    struct usbd_func *p_func = NULL;

    if (p_dev->status != USB_DEV_STATE_CONFIGURED) {
        usb_dc_ep0_stall_set(p_dev->p_dc, USB_TRUE);
        return -USB_EPERM;
    }

    /* 寻找相应的功能接口*/
	p_func = p_dev->p_cur_cfg->p_func_hdr[(p_setup->index & 0xFF)]->p_func[0];

    if ((p_func != NULL) && (p_func->p_opts != NULL) && (p_func->p_opts->p_fn_alt_get != NULL)) {
        ret = p_func->p_opts->p_fn_alt_get(p_func, p_func->p_func_arg);
        if (ret != USB_OK) {
            return ret;
        }
    }

    p_ep0->req.p_buf[0] = 0;
    p_ep0->req.len      = 1;

    return USB_OK;
}

/**
 * \brief 设置接口
 */
static int __interface_set(struct usbd_dev    *p_dev,
                           struct usb_ctrlreq *p_setup){
    struct usbd_func *p_func = NULL;

    if (p_dev->status != USB_DEV_STATE_CONFIGURED) {
        usb_dc_ep0_stall_set(p_dev->p_dc, USB_TRUE);
        return -USB_EPERM;
    }
    /* 寻找相应的功能接口*/
	p_func = p_dev->p_cur_cfg->p_func_hdr[(p_setup->index & 0xFF)]->p_func[0];

    if ((p_func == NULL) ||
            (p_func->p_opts == NULL) ||
            (p_func->p_opts->p_fn_alt_set == NULL)) {
        //return -USB_EILLEGAL;
        return USB_OK;
    }

    return p_func->p_opts->p_fn_alt_set(p_func,
                                        p_func->p_func_arg);
}

/**
 * \brief 设置接口
 */
static int __interface_setup(struct usbd_dev    *p_dev,
                             struct usb_ctrlreq *p_setup){
    struct usbd_func *p_func = NULL;
    /* 寻找相应的功能接口*/
	p_func = p_dev->p_cur_cfg->p_func_hdr[(p_setup->index & 0xFF)]->p_func[0];

    if ((p_func == NULL) ||
            (p_func->p_opts == NULL) ||
            (p_func->p_opts->p_fn_setup == NULL)) {
        //return -USB_EILLEGAL;
        return USB_OK;
    }

    return p_func->p_opts->p_fn_setup(p_func, p_setup, p_func->p_func_arg);
}

/**
 * \brief 标准请求
 *
 * \param[in] p_dev   具体的 USB 从机设备
 * \param[in] p_ep0   具体使用的端点 0（输入或输出）
 * \param[in] p_setup 控制请求包
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_standard_req_handle(struct usbd_dev    *p_dev,
                                 struct usbd_ep0    *p_ep0,
                                 struct usb_ctrlreq *p_setup){
    uint16_t        value = 0;
    int             ret   = 0;
    struct usbd_ep *p_ep  = NULL;

    if ((p_dev == NULL) || (p_setup == NULL) || (p_ep0 == NULL)) {
        return -USB_EINVAL;
    }

    //__USB_INFO("%x\r\n", p_setup->request);

    switch (p_setup->request_type & USB_REQ_TAG_MASK) {
        case USB_REQ_TAG_DEVICE:
            switch (p_setup->request) {
                case USB_REQ_GET_STATUS:
                    p_ep0->req.len  = 2;
                    memcpy(p_ep0->req.p_buf, &value, p_ep0->req.len);
                    break;
                case USB_REQ_CLEAR_FEATURE:
                    ret = __feature_clr(p_dev, p_setup->value, p_setup->index);
                    break;
                case USB_REQ_SET_FEATURE:
                    ret = __feature_set(p_dev, p_setup->value, p_setup->index);
                    break;
                /* 设置地址*/
                case USB_REQ_SET_ADDRESS:
                    __USB_INFO("set usb device address: 0x%x\r\n", p_setup->value);
                    ret = __address_set(p_dev->p_dc, p_setup->value);
                    if (ret == USB_OK) {
                        p_dev->status = USB_DEV_STATE_ADDRESS;
                    }
                    break;
                /* 获取描述符*/
                case USB_REQ_GET_DESCRIPTOR:
                    ret = __descriptor_get(p_dev, p_setup, &p_ep0->req);
                    break;
                case USB_REQ_SET_DESCRIPTOR:
                    /* 不支持设置描述符 */
                    usb_dc_ep0_stall_set(p_dev->p_dc, USB_TRUE);
                    ret = -USB_ENOTSUP;
                    break;

                case USB_REQ_GET_CONFIGURATION:
                    ret = __cur_cfg_value_get(p_dev, p_setup, &p_ep0->req);
                    break;
                /* 设置配置*/
                case USB_REQ_SET_CONFIGURATION:
                    __USB_INFO("set usb device config: %d\r\n", p_setup->value);
                    ret = __setup_cfg_set(p_dev, p_setup);
                    break;
                default:
                    __USB_ERR_INFO("device request %d unknown\r\n", p_setup->request);
                    usb_dc_ep0_stall_set(p_dev->p_dc, USB_TRUE);
                    ret = -USB_ENOTSUP;
                    break;
            }
            break;
        case USB_REQ_TAG_INTERFACE:
            switch(p_setup->request) {
                case USB_REQ_GET_INTERFACE:
                    ret = __interface_get(p_dev, p_setup, p_ep0);
                    break;
                case USB_REQ_SET_INTERFACE:
                    ret = __interface_set(p_dev, p_setup);
                    break;
                default:
                    if (__interface_setup(p_dev, p_setup) != USB_OK) {
                        __USB_ERR_INFO("interface request %d unknown\r\n", p_setup->request);
                        usb_dc_ep0_stall_set(p_dev->p_dc, USB_TRUE);
                        return -USB_ENOTSUP;
                    } else
                        break;
                    }
                 break;
        case USB_REQ_TAG_ENDPOINT:
            switch (p_setup->request) {
                case USB_REQ_GET_STATUS:

                    ret = usb_dc_ep_get(p_dev->p_dc, p_setup->index, &p_ep);
                    if (ret != USB_OK) {
                        break;
                    }

                    p_ep0->req.p_buf[0] = p_ep->is_stalled;
                    p_ep0->req.p_buf[1] = 0;
                    p_ep0->req.len      = 2;

                    break;
                case USB_REQ_CLEAR_FEATURE:

                    /* 如果是端点0的stall清除，未测试 */
                    if ((p_setup->index == 0) || (p_setup->index == 0x80)) {
                        if(USB_ENDPOINT_HALT == p_setup->value) {
                            usb_dc_ep0_stall_set(p_dev->p_dc, USB_FALSE);
                            /* 等待验证，查看是否进入此分支,Release下会进入 */
                        }
                        break;
                    }
                    /* p_setup->wIndex 不能是 0,如果是 0 要做特殊处理，因为 0 端点没有加入链表 */
                    ret = usb_dc_ep_get(p_dev->p_dc, p_setup->index, &p_ep);
                    if (ret != USB_OK) {
                        break;
                    }
                    if ((USB_ENDPOINT_HALT == p_setup->value) && (p_ep->is_stalled == USB_TRUE)) {
                        ret = __feature_clr(p_dev, p_setup->value, p_setup->index);
                        p_ep->is_stalled = USB_FALSE;
                    }
                    break;
                case USB_REQ_SET_FEATURE:
                    if (USB_ENDPOINT_HALT == p_setup->value) {
                        ret = usb_dc_ep_get(p_dev->p_dc, p_setup->index, &p_ep);
                        if (ret != USB_OK) {
                            break;
                        }
                        p_ep->is_stalled = USB_TRUE;
                        __feature_set(p_dev, p_setup->value, p_setup->index);
                    }
                    break;
                case USB_REQ_SYNCH_FRAME:
                    break;
                default:
                    __USB_ERR_INFO("endpoint request %d unknown\r\n", p_setup->request);
                    usb_dc_ep0_stall_set(p_dev->p_dc, USB_TRUE);
                    ret = -USB_ENOTSUP;
                    break;
            }
            break;
        case USB_REQ_TAG_OTHER:
            default:
            __USB_ERR_INFO("other type request %d unknown\n", p_setup->request);
            usb_dc_ep0_stall_set(p_dev->p_dc, USB_TRUE);
            ret = -USB_ENOTSUP;
            break;
    }

    return ret;
}

/**
 * \brief 功能请求
 *
 * \param[in] p_dev   具体的 USB 从机设备
 * \param[in] p_setup 控制请求包
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_func_req_handle(struct usbd_dev    *p_dev,
                             struct usb_ctrlreq *p_setup){
    struct usbd_func *p_func = NULL;

    if ((p_dev == NULL) || (p_setup == NULL)) {
        return -USB_EINVAL;
    }
    /* 请求的接口编号大于接口数量*/
    if (p_setup->index > p_dev->p_cur_cfg->config_desc.num_interfaces) {
        usb_dc_ep0_stall_set(p_dev->p_dc, USB_TRUE);
        return -USB_EILLEGAL;
    }

    switch (p_setup->request_type & USB_REQ_TAG_MASK) {
        case USB_REQ_TAG_INTERFACE:
            p_func = p_dev->p_cur_cfg->p_func_hdr[(p_setup->index & 0xFF)]->p_func[0];

            if ((p_func != NULL) &&
                    (p_func->p_opts != NULL) &&
                    (p_func->p_opts->p_fn_setup != NULL)) {
                return p_func->p_opts->p_fn_setup(p_func, p_setup, p_func->p_func_arg);
            }
            return USB_OK;
        case USB_REQ_TAG_ENDPOINT:
            break;
        default:
            __USB_ERR_INFO("function request type %d unknown\r\n", p_setup->request_type & USB_REQ_TAG_MASK);
            usb_dc_ep0_stall_set(p_dev->p_dc, USB_TRUE);
            return -USB_ENOTSUP;
    }

    return USB_OK;
}

/**
 * \brief 自定义请求
 *
 * \param[in] p_dev   具体的 USB 从机设备
 * \param[in] p_ep0   具体使用的端点 0（输入或输出）
 * \param[in] p_setup 控制请求包
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_vendor_req_handle(struct usbd_dev    *p_dev,
                               struct usbd_ep0    *p_ep0,
                               struct usb_ctrlreq *p_setup){
    const uint8_t *p_desc = NULL;
    uint32_t       len    = 0;
    int            ret    = -USB_ENOTSUP;

    if (p_setup->request == p_dev->vendor_code) {
        switch(p_setup->index) {
            case 4:
                if (p_dev->p_mircosoft_os_desc && p_dev->p_mircosoft_os_desc->p_compat_desc) {
                    p_desc = p_dev->p_mircosoft_os_desc->p_compat_desc;
                    len    = p_desc[0] + (p_desc[1] << 8) + (p_desc[2] << 16) + (p_desc[3] << 24);
                } else {
                    goto __failed;
                }
                break;
            case 5:
                if (p_dev->p_mircosoft_os_desc && p_dev->p_mircosoft_os_desc->p_prop_desc) {
                    p_desc = p_dev->p_mircosoft_os_desc->p_prop_desc;
                    len    = p_desc[0] + (p_desc[1] << 8) + (p_desc[2] << 16) + (p_desc[3] << 24);

                    /* wValue 的高 8 位指定的是接口 */
                    for (int i = 0; i < (p_setup->value >> 8); i++) {
                        p_desc = p_desc + len;
                        len    = p_desc[0] + (p_desc[1] << 8) + (p_desc[2] << 16) + (p_desc[3] << 24);
                    }
                } else {
                    goto __failed;
                }
                break;
            case 7:
                if(p_dev->p_mircosoft_os_desc && p_dev->p_mircosoft_os_desc->p_prop_desc) {
                    p_desc = p_dev->p_mircosoft_os_desc->p_prop_desc;
                    len    = *(uint16_t *)(p_desc + 8);
                } else {
                    goto __failed;
                }
                break;
        }

        len = p_setup->length > len ? len : p_setup->length;
        if (len > p_dev->p_dc->ep0_req_size) {
            ret = -USB_ENOMEM;
            goto __failed;
        }

        p_ep0->req.len = len;

        if ((len > 0) && (p_desc != NULL)) {
            memcpy(p_ep0->req.p_buf, p_desc, p_ep0->req.len);
            return USB_OK;
        }
    }
__failed:
    usb_dc_ep0_stall_set(p_dev->p_dc, USB_TRUE);

    return ret;
}

/**
 * \brief USB 从机设备断开处理函数
 *
 * \param[in] p_dev 具体的 USB 从机设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_disconnect_handle(struct usbd_dev *p_dev){
    int                    i, j, k, ret;
    struct usb_list_node  *p_node     = NULL;
    struct usbd_pipe      *p_pipe     = NULL;
    struct usbd_func_hdr  *p_func_hdr = NULL;
    struct usbd_func      *p_func     = NULL;
    struct usbd_config    *p_cfg      = NULL;

    p_cfg = p_dev->p_cur_cfg;

    for (i = 0; i < p_cfg->n_func; i++) {
    	p_func_hdr = p_cfg->p_func_hdr[i];

        for (j = 0; j < p_func_hdr->n_func_alt; j++) {
        	p_func = p_func_hdr->p_func[j];

            if ((p_func->p_opts != NULL) &&
                    (p_func->p_opts->p_fn_shutdown != NULL)) {
                ret = p_func->p_opts->p_fn_shutdown(p_func, p_func->p_func_arg);
                if (ret != USB_OK) {
                    __USB_ERR_INFO("interface %d alternate setting %d shutdown failed(%d)\r\n",
                            p_func->intf_desc.interface_number,
                            p_func->intf_desc.alternate_setting);
                    return ret;
                }
            }
            for (k = 0; k < p_func->intf_desc.num_endpoints; k++) {
                /* 遍历管道*/
                usb_list_for_each_node(p_node, &p_cfg->pipe_list) {
                    p_pipe = usb_container_of(p_node, struct usbd_pipe, node);

                    if (p_pipe->p_func_hdr->func_num == p_func->intf_desc.interface_number) {
                        /* 禁能节点*/
                        ret = usb_dc_ep_disable(p_dev->p_dc, p_pipe->p_hw);
                        if (ret != USB_OK) {
                            return ret;
                        }
                    }
                }
            }
        }
    }
    return USB_OK;
}
