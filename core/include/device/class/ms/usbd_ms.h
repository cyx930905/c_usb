#ifndef __USBD_MS_H__
#define __USBD_MS_H__

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "common/err/usb_err.h"
#include "core/include/specs/usb_specs.h"
#include "core/include/specs/usb_ms_specs.h"
#include "core/include/device/core/usbd.h"

struct usbd_ms;

#include "core/include/device/class/ms/usbd_ms_scsi_bbb.h"

/* \brief 接口号定义*/
#define USBD_MS_INTF_NUM              0           /* USB 大容量存储设备接口编号*/
#define USBD_MS_INTF_ALT_NUM          0           /* USB 大容量存储设备备用设置编号*/

/* \brief 读写超时时间*/
#define USBD_MS_WAIT_TIMEOUT          5000
#if USB_OS_EN
/* \brief 互斥锁超时时间*/
#define USBD_MS_MUTEX_TIMEOUT         5000
#endif

#define USBD_MS_LU_USR_DATA_GET(p_lu) (p_lu->p_usr_data)
#define USBD_MS_LU_BLK_SIZE_GET(p_lu) (p_lu->blk_size)

/* \brief USB 从机大容量存储设备逻辑单元结构体 */
struct usbd_ms_lu {
    int                            num;        /* 逻辑单元号 */
    uint32_t                       blk_size;   /* 块大小 */
    uint32_t                       n_blks;     /* 块数量 */
    union {
    	struct usbd_ms_scsi_bbb_lu lu;
    } lu;
    struct usb_list_node           node;
    void                          *p_usr_data;  /* 用户私有数据*/
};

/* \brief 操作函数集*/
struct usbd_ms_opts {
    int (*p_fn_blk_read)(struct usbd_ms_lu *p_lu,
                         uint8_t           *p_buf,
                         uint32_t           blk_num,
                         uint32_t           n_blks,
                         uint32_t          *p_blks_act);
    int (*p_fn_blk_write)(struct usbd_ms_lu *p_lu,
                          uint8_t           *p_buf,
                          uint32_t           blk_num,
                          uint32_t           n_blks,
                          uint32_t          *p_blks_act);
};

/* \brief USB 从机大容量存储设备结构体 */
struct usbd_ms {
    uint8_t                          n_lu;             /* 逻辑单元个数 */
    uint8_t                          cfg_num;          /* 配置编号*/
    uint8_t                         *p_buf;            /* 传输缓存*/
    uint32_t                         buf_size;         /* 缓存大小 */
    struct usbd_dev                 *p_dc_dev;         /* USB 从机设备结构体*/
#if USB_OS_EN
    usb_mutex_handle_t               p_lock;
#endif
    struct usbd_pipe                *p_data_in;        /* 数据输入管道*/
    struct usbd_pipe                *p_data_out;       /* 数据输出管道*/
    uint8_t                          data_in_ep_addr;  /* 数据输入端点地址*/
    uint8_t                          data_out_ep_addr; /* 数据输出端点地址*/
    usb_bool_t                       is_setup;         /* 是否设置*/
    usb_bool_t                       is_max_lun_get;   /* 是否被获取最大逻辑单元号*/
    struct usbd_ms_opts             *p_opts;           /* 操作函数集*/

    union {
    	struct usbd_ms_scsi_bbb_info info;
    } info;

    struct usb_list_head             lu_list;        /* 逻辑单元链表*/
    struct usbd_ms_lu               *p_lu_select;      /* 选择操作的逻辑分区*/
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
                   struct usbd_ms       **p_ms_ret);
/**
 * \brief 销毁 USB 从机大容量存储设备
 *
 * \param[in] p_ms 要销毁的 USB 从机大容量存储设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_destroy(struct usbd_ms *p_ms);
/**
 * \brief 检查 USB 从机大容量存储设备是否设置
 *
 * \param[in] p_ms USB 从机大容量存储设备
 *
 * \retval 设置返回 USB_TRUE，否则返回 USB_FALSE
 */
usb_bool_t usbd_ms_is_setup(struct usbd_ms *p_ms);
/**
 * \brief USB 从机大容量存储设备添加分区信息
 *
 * \param[in] p_ms USB 从机大容量存储设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_lu_info_add(struct usbd_ms *p_ms,
                        uint32_t        blk_size,
                        uint32_t        n_blks,
                        uint8_t        *p_num_ret);
/**
 * \brief USB 从机大容量存储设备用户操作函数设置
 *
 * \param[in] p_ms USB 从机大容量存储设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_usr_opts_set(struct usbd_ms *p_ms, struct usbd_ms_opts *p_opts);
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
                   uint8_t        *p_num_ret);
/**
 * \brief USB 从机大容量存储设备删除逻辑单元信息
 *
 * \param[in] p_ms   USB 从机大容量存储设备
 * \param[in] lu_num 逻辑单元号
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_lu_del(struct usbd_ms *p_ms, uint8_t lu_num);
/**
 * \brief USB 从大容量存储设备管道停止设置
 *
 * \param[in] p_ms    USB 从机大容量存储设备
 * \param[in] dir     要停止的管道方向
 * \param[in] is_set  是否停止
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_pipe_stall_set(struct usbd_ms *p_ms, uint8_t dir, usb_bool_t is_set);
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
                 uint32_t       *p_act_len);
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
                  uint32_t       *p_act_len);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif

