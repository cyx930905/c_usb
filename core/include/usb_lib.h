#ifndef __USB_LIB_H
#define __USB_LIB_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "common/usb_common.h"
#include "config/usb_config.h"
#include "adapter/usb_adapter.h"
#include "common/list/usb_list.h"

#if USB_OS_EN
/* \brief USB 库互斥锁上锁超时时间*/
#define USB_LIB_LOCK_TIMEOUT  5000
#endif

#define USB_LIB_LIST_FOR_EACH_NODE(pos, n, p_lib) usb_list_for_each_node_safe(pos, n, &((p_lib)->lib_dev_list))

#if USB_MEM_RECORD_EN
/* \brief USB 内存记录结构体*/
struct usb_mem_record {
    char     mem_name[USB_NAME_LEN];
    uint32_t mem_total;
#if USB_OS_EN
    uint32_t mutex_total;
    uint32_t sem_total;
    uint32_t task_total;
#endif
};
#endif

/* \brief USB 环形缓冲区结构体*/
struct usb_ringbuf {
#if USB_OS_EN
    usb_mutex_handle_t p_lock;          /* 互斥锁*/
#endif
    uint8_t           *p_buf;
    uint32_t           buf_size;
    uint32_t           data_pos;        /* 数据位置*/
    uint32_t           rd_pos;          /* 已读位置*/
};

/* \brief USB 基本库结构体*/
struct usb_lib_base {
    struct usb_list_head   lib_dev_list;      /* 管理所有设备链表*/
    struct usb_list_node  *p_dev_loca;        /* 遍历时用来定位设备位置*/
    usb_bool_t             is_lib_init;       /* 是否初始化库*/
    uint8_t                n_dev;             /* 当前存在设备的数量*/
#if USB_OS_EN
    usb_mutex_handle_t     p_lock;            /* 互斥锁*/
#endif
#if USB_MEM_RECORD_EN
    struct usb_mem_record  mem_record;        /* 内存记录*/
#endif
};

/**
 * \brief 检查 USB 库是否初始化
 *
 * \param[in] p_lib USB 基本库结构体
 *
 * \retval 初始化返回 USB_TRUE，否则返回 USB_FALSE
 */
usb_bool_t usb_lib_is_init(struct usb_lib_base *p_lib);
/**
 * \brief USB 库内存分配函数
 *
 * \param[in] p_lib USB 基本库结构体
 * \param[in] size  要分配的内存大小
 *
 * \retval 成功返回分配的内存的地址
 */
void *usb_lib_malloc(struct usb_lib_base *p_lib, size_t size);
/**
 * \brief USB 库内存释放函数
 *
 * \param[in] p_lib USB 基本库结构体
 * \param[in] p_mem 要释放的内存
 */
void usb_lib_mfree(struct usb_lib_base *p_lib, void *p_mem);
/**
 * \brief USB DMA 内存申请函数
 *
 * \param[in] p_lib  USB 基本库结构体
 * \param[in] size   要分配的内存大小
 * \param[in] align  对齐数
 *
 * \retval 成功返回分配的内存的地址
 */
void *usb_lib_dma_align_malloc(struct usb_lib_base *p_lib, size_t size, size_t align);
/**
 * \brief USB DMA 内存释放函数
 *
 * \param[in] p_lib USB 基本库结构体
 * \param[in] p_mem 要释放的内存
 * \param[in] size  要释放的内存大小
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_dma_mfree(struct usb_lib_base *p_lib, void *p_mem, uint32_t size);
/**
 * \brief USB 库环形缓冲区创建函数
 *
 * \param[in] p_lib    USB 基本库结构体
 * \param[in] buf_size 环形缓冲区缓存大小
 *
 * \retval 成功返回 USB 环形缓冲区结构体
 */
struct usb_ringbuf *usb_lib_rb_create(struct usb_lib_base *p_lib, uint32_t buf_size);
/**
 * \brief USB 库环形缓冲区销毁函数
 *
 * \param[in] p_lib USB 基本库结构体
 * \param[in] p_rb  要销毁的环形缓冲区
 */
void usb_lib_rb_destroy(struct usb_lib_base *p_lib, struct usb_ringbuf *p_rb);
/**
 * \brief USB 库环形缓冲区数据放入函数
 *
 * \param[in]  p_rb      环形缓冲区
 * \param[in]  p_buf     数据缓存
 * \param[in]  buf_len   缓存大小
 * \param[out] p_act_len 返回实际放入的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_rb_put(struct usb_ringbuf *p_rb,
                   uint8_t            *p_buf,
                   uint32_t            buf_len,
                   uint32_t           *p_act_len);
/**
 * \brief USB 库环形缓冲区数据获取函数
 *
 * \param[in]  p_rb      环形缓冲区
 * \param[in]  p_buf     数据缓存
 * \param[in]  buf_len   缓存大小
 * \param[out] p_act_len 返回实际获取的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_rb_get(struct usb_ringbuf *p_rb,
                   uint8_t            *p_buf,
                   uint32_t            buf_len,
                   uint32_t           *p_act_len);
#if USB_OS_EN
/**
 * \brief USB 库互斥锁创建函数
 *
 * \param[in] p_lib USB 基本库结构体
 *
 * \retval 成功返回 USB 互斥锁句柄
 */
usb_mutex_handle_t usb_lib_mutex_create(struct usb_lib_base *p_lib);
/**
 * \brief USB 库互斥锁销毁函数
 *
 * \param[in] p_lib   USB 基本库结构体
 * \param[in] p_mutex USB 互斥锁句柄
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_mutex_destroy(struct usb_lib_base *p_lib, usb_mutex_handle_t p_mutex);
/**
 * \brief USB 库互斥锁锁
 *
 * \param[in] p_lib USB 基本库结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_mutex_lock(struct usb_lib_base *p_lib);
/**
 * \brief USB 库互斥锁解锁
 *
 * \param[in] p_lib USB 基本库结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_mutex_unlock(struct usb_lib_base *p_lib);
/**
 * \brief USB 库信号量创建函数
 *
 * \param[in] p_lib USB 基本库结构体
 *
 * \retval 成功返回 USB 信号量句柄
 */
usb_sem_handle_t usb_lib_sem_create(struct usb_lib_base *p_lib);
/**
 * \brief USB 库信号量销毁函数
 *
 * \param[in] p_lib USB 基本库结构体
 * \param[in] p_sem 要销毁的信号量
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_sem_destroy(struct usb_lib_base *p_lib, usb_sem_handle_t p_sem);
/**
 * \brief USB 库任务创建函数
 *
 * \param[in] p_lib  USB 基本库结构体
 * \param[in] p_name 任务名字
 * \param[in] prio   任务优先级
 * \param[in] stk_s  任务栈大小
 * \param[in] p_func 任务函数
 * \param[in] p_arg  任务函数参数
 *
 * \retval 成功返回 USB 任务句柄
 */

usb_task_handle_t usb_lib_task_create(struct usb_lib_base *p_lib,
                                      const char          *p_name,
                                      int                  prio,
                                      size_t               stk_s,
                                      void               (*p_func) (void *p_arg),
                                      void                *p_arg);
/**
 * \brief USB 库任务销毁函数
 *
 * \param[in] p_lib  USB 基本库结构体
 * \param[in] p_task 要销毁的任务
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_task_destroy(struct usb_lib_base *p_lib, usb_task_handle_t p_task);
#endif
/**
 * \brief 获取 USB 库设备数量
 *
 * \param[in] p_lib   USB 基本库结构体
 * \param[in] p_n_dev 返回的设备数量
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_ndev_get(struct usb_lib_base *p_lib, uint8_t *p_n_dev);
/**
 * \brief 往 USB 库中添加一个设备
 *
 * \param[in] p_lib  USB 基本库结构体
 * \param[in] p_node 要添加的设备节点
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_dev_add(struct usb_lib_base *p_lib, struct usb_list_node *p_node);
/**
 * \brief 往 USB 库中删除一个设备
 *
 * \param[in] p_lib  USB 基本库结构体
 * \param[in] p_node 要删除的设备节点
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_dev_del(struct usb_lib_base *p_lib, struct usb_list_node *p_node);
/**
 * \brief 复位 USB 库设备遍历
 *
 * \param[in] p_lib USB 基本库结构体
 */
void usb_lib_dev_traverse_reset(struct usb_lib_base *p_lib);
/**
 * \brief USB 库设备遍历
 *
 * \param[in]  p_lib  USB 基本库结构体
 * \param[out] p_node 返回设备节点结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_dev_traverse(struct usb_lib_base *p_lib, struct usb_list_node **p_node);
#if USB_MEM_RECORD_EN
/**
 * \brief 初始化 USB 库
 *
 * \param[in] p_lib      USB 基本库结构体
 * \param[in] p_mem_name 内存记录名字
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_init(struct usb_lib_base *p_lib, char *p_mem_name);
/**
 * \brief 获取 USB 库内存记录
 *
 * \param[in] p_lib        USB 基本库结构体
 * \param[in] p_mem_record 内存记录结构体缓存
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_mem_record_get(struct usb_lib_base *p_lib, struct usb_mem_record *p_mem_record);
#else
/**
 * \brief 初始化 USB 库
 *
 * \param[in] p_lib USB 基本库结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_init(struct usb_lib_base *p_lib);
#endif
/**
 * \brief 反初始化 USB 库
 *
 * \param[in] p_lib USB 基本库结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_deinit(struct usb_lib_base *p_lib);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif
