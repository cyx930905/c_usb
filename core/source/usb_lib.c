/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/usb_lib.h"
#include "common/refcnt/usb_refcnt.h"
#include "adapter/usb_adapter.h"
#include <string.h>
#include <stdio.h>

/*******************************************************************************
 * Statement
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 检查 USB 库是否初始化
 *
 * \param[in] p_lib USB 基本库结构体
 *
 * \retval 初始化返回 USB_TRUE，否则返回 USB_FALSE
 */
usb_bool_t usb_lib_is_init(struct usb_lib_base *p_lib){
    return p_lib->is_lib_init;
}

/**
 * \brief USB 库内存分配函数
 *
 * \param[in] p_lib USB 基本库结构体
 * \param[in] size  要分配的内存大小
 *
 * \retval 成功返回分配的内存的地址
 */
void *usb_lib_malloc(struct usb_lib_base *p_lib, size_t size){
    void *p_mem = NULL;

    p_mem = usb_mem_alloc(size);
    if (p_mem != NULL) {
#if USB_MEM_RECORD_EN
        p_lib->mem_record.mem_total++;
#endif
    }
    return p_mem;
}

/**
 * \brief USB 库内存释放函数
 *
 * \param[in] p_lib USB 基本库结构体
 * \param[in] p_mem 要释放的内存
 */
void usb_lib_mfree(struct usb_lib_base *p_lib, void *p_mem){
    if (p_mem == NULL) {
        return;
    }
#if USB_MEM_RECORD_EN
    p_lib->mem_record.mem_total--;
#endif
    usb_mem_free(p_mem);
}

/**
 * \brief USB DMA 内存申请函数
 *
 * \param[in] p_lib  USB 基本库结构体
 * \param[in] size   要分配的内存大小
 * \param[in] align  对齐数
 *
 * \retval 成功返回分配的内存的地址
 */
void *usb_lib_dma_align_malloc(struct usb_lib_base *p_lib, size_t size, size_t align){
    void *p_mem = NULL;

    p_mem = usb_cache_dma_align(size, align);
    if (p_mem != NULL) {
#if USB_MEM_RECORD_EN
        p_lib->mem_record.mem_total++;
#endif
    }
    return p_mem;
}

/**
 * \brief USB DMA 内存释放函数
 *
 * \param[in] p_lib USB 基本库结构体
 * \param[in] p_mem 要释放的内存
 * \param[in] size  要释放的内存大小
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_dma_mfree(struct usb_lib_base *p_lib, void *p_mem, uint32_t size){
    if (p_mem == NULL) {
        return USB_OK;
    }
#if USB_MEM_RECORD_EN
    p_lib->mem_record.mem_total--;
#endif
    return usb_cache_dma_free(p_mem, size);
}

/**
 * \brief USB 库环形缓冲区创建函数
 *
 * \param[in] p_lib    USB 基本库结构体
 * \param[in] buf_size 环形缓冲区缓存大小
 *
 * \retval 成功返回 USB 环形缓冲区结构体
 */
struct usb_ringbuf *usb_lib_rb_create(struct usb_lib_base *p_lib, uint32_t buf_size){
    struct usb_ringbuf *p_rb = NULL;

    p_rb = usb_lib_malloc(p_lib, sizeof(struct usb_ringbuf));
    if (p_rb == NULL) {
        return NULL;
    }
    memset(p_rb, 0, sizeof(struct usb_ringbuf));

    p_rb->p_buf = usb_lib_malloc(p_lib, buf_size);
    if (p_rb->p_buf == NULL) {
        goto __failed;
    }
#if USB_OS_EN
    /* 创建互斥锁*/
    p_rb->p_lock = usb_lib_mutex_create(p_lib);
    if (p_rb->p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        goto __failed;
    }
#endif

    p_rb->buf_size = buf_size;

    return p_rb;
__failed:
#if USB_OS_EN
    if (p_rb->p_lock) {
        usb_lib_task_destroy(p_lib, p_rb->p_lock);
    }
#endif
    if (p_rb->p_buf) {
        usb_lib_mfree(p_lib, p_rb->p_buf);
    }
    return NULL;
}

/**
 * \brief USB 库环形缓冲区销毁函数
 *
 * \param[in] p_lib USB 基本库结构体
 * \param[in] p_rb  要销毁的环形缓冲区
 */
void usb_lib_rb_destroy(struct usb_lib_base *p_lib, struct usb_ringbuf *p_rb){
    if (p_rb == NULL) {
        return;
    }
#if USB_OS_EN
    if (p_rb->p_lock) {
        usb_lib_task_destroy(p_lib, p_rb->p_lock);
    }
#endif
    if (p_rb->p_buf) {
        usb_lib_mfree(p_lib, p_rb->p_buf);
    }
    usb_lib_mfree(p_lib, p_rb);
}

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
                   uint32_t           *p_act_len){
    int      ret = USB_OK;
    uint32_t len;

#if USB_OS_EN
    ret = usb_mutex_lock(p_rb->p_lock, 5000);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_rb->rd_pos > p_rb->data_pos) {
        if (buf_len > (p_rb->rd_pos - p_rb->data_pos - 1)) {
            len = p_rb->rd_pos - p_rb->data_pos;
        } else {
            len = buf_len;
        }
        memcpy(p_rb->p_buf + p_rb->data_pos, p_buf, len);

        *p_act_len = len;
    } else {
        /* 计算可以放入数据的总大小*/
        len = p_rb->buf_size + p_rb->rd_pos - p_rb->data_pos - 1;

        if (len > buf_len) {
            len = buf_len;
        }
        /* 回环*/
        if ((len + p_rb->data_pos) > p_rb->buf_size) {
            len = p_rb->buf_size - p_rb->data_pos;
        }
        memcpy(p_rb->p_buf + p_rb->data_pos, p_buf, len);
        *p_act_len = len;

        len = buf_len - *p_act_len;

        if (p_rb->rd_pos != 0) {
            if (len > (p_rb->rd_pos - 1)) {
                len = p_rb->rd_pos - 1;
            }
            memcpy(p_rb->p_buf, p_buf + *p_act_len, len);
            *p_act_len += len;
        }
    }
    /* 重新设置写标记*/
    p_rb->data_pos = (p_rb->data_pos + *p_act_len);
    /* 到了环形末尾，回到环形头*/
    if (p_rb->data_pos >= p_rb->buf_size) {
        p_rb->data_pos -= p_rb->buf_size;
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_rb->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif

    return ret;
}

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
                   uint32_t           *p_act_len){
    int      ret = USB_OK;
    uint32_t len;

#if USB_OS_EN
    ret = usb_mutex_lock(p_rb->p_lock, 5000);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (p_rb->rd_pos <= p_rb->data_pos) {
        len = p_rb->data_pos - p_rb->rd_pos;

        if (len > buf_len) {
            len = buf_len;
        }
        memcpy(p_buf, p_rb->p_buf + p_rb->rd_pos, len);
        *p_act_len = len;
    } else {
        len = p_rb->buf_size - p_rb->rd_pos;

        if (len > buf_len) {
            len = buf_len;
        }
        memcpy(p_buf, p_rb->p_buf + p_rb->rd_pos, len);
        *p_act_len = len;

        len = p_rb->data_pos;

        if (len > (buf_len - *p_act_len)) {
            len = buf_len - *p_act_len;
        }
        if (len > 0) {
            memcpy(p_buf + *p_act_len, p_rb->p_buf, len);
        }
        *p_act_len += len;
    }
    p_rb->rd_pos = p_rb->rd_pos + *p_act_len;
    if (p_rb->rd_pos >= p_rb->buf_size) {
        p_rb->rd_pos -= p_rb->buf_size;
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(p_rb->p_lock);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif

    return ret;
}

#if USB_OS_EN
/**
 * \brief USB 库互斥锁创建函数
 *
 * \param[in] p_lib USB 基本库结构体
 *
 * \retval 成功返回 USB 互斥锁句柄
 */
usb_mutex_handle_t usb_lib_mutex_create(struct usb_lib_base *p_lib){
#if USB_MEM_RECORD_EN
    p_lib->mem_record.mutex_total++;
#endif
    return usb_mutex_create();
}

/**
 * \brief USB 库互斥锁销毁函数
 *
 * \param[in] p_lib   USB 基本库结构体
 * \param[in] p_mutex USB 互斥锁句柄
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_mutex_destroy(struct usb_lib_base *p_lib, usb_mutex_handle_t p_mutex){
#if USB_MEM_RECORD_EN
    p_lib->mem_record.mutex_total--;
#endif
    return usb_mutex_delete(p_mutex);
}

/**
 * \brief USB 库互斥锁锁
 *
 * \param[in] p_lib USB 基本库结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_mutex_lock(struct usb_lib_base *p_lib){
    return usb_mutex_lock(p_lib->p_lock, USB_LIB_LOCK_TIMEOUT);
}

/**
 * \brief USB 库互斥锁解锁
 *
 * \param[in] p_lib USB 基本库结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_mutex_unlock(struct usb_lib_base *p_lib){
    return usb_mutex_unlock(p_lib->p_lock);
}

/**
 * \brief USB 库信号量创建函数
 *
 * \param[in] p_lib USB 基本库结构体
 *
 * \retval 成功返回 USB 信号量句柄
 */
usb_sem_handle_t usb_lib_sem_create(struct usb_lib_base *p_lib){
#if USB_MEM_RECORD_EN
    p_lib->mem_record.sem_total++;
#endif
    return usb_sem_create();
}

/**
 * \brief USB 库信号量销毁函数
 *
 * \param[in] p_lib USB 基本库结构体
 * \param[in] p_sem 要销毁的信号量
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_sem_destroy(struct usb_lib_base *p_lib, usb_sem_handle_t p_sem){
#if USB_MEM_RECORD_EN
    p_lib->mem_record.sem_total--;
#endif
    return usb_sem_delete(p_sem);
}

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
                                      void                *p_arg){
#if USB_MEM_RECORD_EN
    p_lib->mem_record.task_total++;
#endif
    return usb_task_create(p_name, prio, stk_s, p_func, p_arg);
}

/**
 * \brief USB 库任务销毁函数
 *
 * \param[in] p_lib  USB 基本库结构体
 * \param[in] p_task 要销毁的任务
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_task_destroy(struct usb_lib_base *p_lib, usb_task_handle_t p_task){
#if USB_MEM_RECORD_EN
    p_lib->mem_record.task_total--;
#endif
    return usb_task_delete(p_task);
}

#endif

/**
 * \brief 获取 USB 库设备数量
 *
 * \param[in] p_lib   USB 基本库结构体
 * \param[in] p_n_dev 返回的设备数量
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_ndev_get(struct usb_lib_base *p_lib, uint8_t *p_n_dev){
    int ret = USB_OK;

    if (p_n_dev == NULL) {
        return -USB_EINVAL;
    }

    /* 检查 USB 库是否正常*/
    if (usb_lib_is_init(p_lib) == USB_FALSE) {
        return -USB_ENOINIT;
    }

#if USB_OS_EN
    ret = usb_lib_mutex_lock(p_lib);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    *p_n_dev = p_lib->n_dev;
#if USB_OS_EN
    ret = usb_lib_mutex_unlock(p_lib);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 往 USB 库中添加一个设备
 *
 * \param[in] p_lib  USB 基本库结构体
 * \param[in] p_node 要添加的设备节点
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_dev_add(struct usb_lib_base *p_lib, struct usb_list_node *p_node){
    int ret = USB_OK;

    if (p_node == NULL) {
        return -USB_EINVAL;
    }

    /* 检查 USB 库是否正常*/
    if (usb_lib_is_init(p_lib) == USB_FALSE) {
        return -USB_ENOINIT;
    }
#if USB_OS_EN
    ret = usb_lib_mutex_lock(p_lib);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_lib->n_dev++;
    usb_list_node_add_tail(p_node, &p_lib->lib_dev_list);
#if USB_OS_EN
    ret = usb_lib_mutex_unlock(p_lib);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 往 USB 库中删除一个设备
 *
 * \param[in] p_lib  USB 基本库结构体
 * \param[in] p_node 要删除的设备节点
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_dev_del(struct usb_lib_base *p_lib, struct usb_list_node *p_node){
    int ret = USB_OK;

    if (p_node == NULL) {
        return -USB_EINVAL;
    }

#if USB_OS_EN
    ret = usb_lib_mutex_lock(p_lib);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    p_lib->n_dev--;
    usb_list_node_del(p_node);
#if USB_OS_EN
    ret = usb_lib_mutex_unlock(p_lib);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 复位 USB 库设备遍历
 *
 * \param[in] p_lib USB 基本库结构体
 */
void usb_lib_dev_traverse_reset(struct usb_lib_base *p_lib){
#if USB_OS_EN
    int ret;
#endif

    /* 检查 USB 库是否正常*/
    if (usb_lib_is_init(p_lib) == USB_FALSE) {
        return;
    }

#if USB_OS_EN
    ret = usb_lib_mutex_lock(p_lib);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
    }
#endif

    p_lib->p_dev_loca = (struct usb_list_node *)&p_lib->lib_dev_list;

#if USB_OS_EN
    ret = usb_lib_mutex_unlock(p_lib);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
}

/**
 * \brief USB 库设备遍历
 *
 * \param[in]  p_lib  USB 基本库结构体
 * \param[out] p_node 返回设备节点结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_dev_traverse(struct usb_lib_base *p_lib, struct usb_list_node **p_node){
    int ret = USB_OK;

    /* 检查 USB 库是否正常*/
    if (usb_lib_is_init(p_lib) == USB_FALSE) {
        return -USB_ENOINIT;
    }

#if USB_OS_EN
    ret = usb_lib_mutex_lock(p_lib);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    if (!usb_list_head_is_empty(&p_lib->lib_dev_list)){
        if (p_lib->p_dev_loca->p_next == (struct usb_list_node *)&p_lib->lib_dev_list) {
            goto __exit;
        }

        *p_node = p_lib->p_dev_loca->p_next;

        p_lib->p_dev_loca = p_lib->p_dev_loca->p_next;

#if USB_OS_EN
        ret = usb_lib_mutex_unlock(p_lib);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            *p_node = NULL;
        }
#endif
        return ret;
    }
__exit:
#if USB_OS_EN
        ret = usb_lib_mutex_unlock(p_lib);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
    return -USB_END;
}

#if USB_MEM_RECORD_EN
/**
 * \brief 初始化 USB 库
 *
 * \param[in] p_lib      USB 基本库结构体
 * \param[in] p_mem_name 内存记录名字
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_init(struct usb_lib_base *p_lib, char *p_mem_name){
#else
/**
 * \brief 初始化 USB 库
 *
 * \param[in] p_lib USB 基本库结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_init(struct usb_lib_base *p_lib){
#endif
    if (p_lib->is_lib_init == USB_TRUE) {
        return USB_OK;
    }

    if (p_mem_name == NULL) {
        return -USB_EINVAL;
    }

#if USB_MEM_RECORD_EN
    /* 初始化内存记录*/
    memset(&p_lib->mem_record, 0, sizeof(struct usb_mem_record));
    strncpy(p_lib->mem_record.mem_name, p_mem_name, (USB_NAME_LEN - 1));
#endif
#if USB_OS_EN
    /* 创建互斥锁*/
    p_lib->p_lock = usb_lib_mutex_create(p_lib);
    if (p_lib->p_lock == NULL) {
        __USB_ERR_TRACE(MutexCreateErr, "\r\n");
        return -USB_EPERM;
    }
#endif
    /* 初始化 USB 库设备链表*/
    usb_list_head_init(&p_lib->lib_dev_list);

    p_lib->n_dev       = 0;
    p_lib->is_lib_init = USB_TRUE;
    p_lib->p_dev_loca  = (struct usb_list_node *)&p_lib->lib_dev_list;

    return USB_OK;
}

/**
 * \brief 反初始化 USB 库
 *
 * \param[in] p_lib USB 基本库结构体
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_deinit(struct usb_lib_base *p_lib){
#if USB_OS_EN
    int ret;
    if (p_lib->p_lock) {
        ret = usb_mutex_delete(p_lib->p_lock);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexDelErr, "(%d)\r\n", ret);
            return ret;
        }
    }
#endif
#if USB_MEM_RECORD_EN
    memset(&p_lib->mem_record, 0, sizeof(struct usb_mem_record));
#endif
    p_lib->n_dev       = 0;
    p_lib->is_lib_init = USB_FALSE;

    return USB_OK;
}

#if USB_MEM_RECORD_EN
/**
 * \brief 获取 USB 库内存记录
 *
 * \param[in] p_lib        USB 基本库结构体
 * \param[in] p_mem_record 内存记录结构体缓存
 *
 * \retval 成功返回 USB_OK
 */
int usb_lib_mem_record_get(struct usb_lib_base *p_lib, struct usb_mem_record *p_mem_record){
    if (p_mem_record == NULL) {
        return -USB_EINVAL;
    }
    /* 检查 USB 库是否正常*/
    if (usb_lib_is_init(p_lib) == USB_FALSE) {
        return -USB_ENOINIT;
    }

    memcpy(p_mem_record, &p_lib->mem_record, sizeof(struct usb_mem_record));

    return USB_OK;
}
#endif
