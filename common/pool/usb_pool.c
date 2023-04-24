/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "adapter/usb_adapter.h"
#include "common/usb_common.h"
#include "common/err/usb_err.h"
#include "common/pool/usb_pool.h"

/*******************************************************************************
 * Static
 ******************************************************************************/
#if USB_OS_EN
static usb_mutex_handle_t __g_usb_pool_mutex;
#endif
static usb_bool_t         __g_usb_pool_init = USB_FALSE;

/*******************************************************************************
 * Statement
 ******************************************************************************/
/* \brief 内存池空闲块结构体 */
typedef struct __free_item {
    struct __free_item *p_next;
} __free_item_t;

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 内存池实例初始化
 *
 * \param[in] p_pool      内存池实例指针
 * \param[in] p_pool_mem  内存池单元使用空间的起始地址
 * \param[in] pool_size   内存池单元空间总大小
 * \param[in] item_size   单个内存池单元空间大小
 *
 * \return 若成功，返回内存池句柄；当失败时，返回NULL
 */
usb_pool_id_t usb_pool_init(usb_pool_t *p_pool,
                            void       *p_pool_mem,
                            size_t      pool_size,
                            size_t      item_size){
    __free_item_t  *p_fb;
    size_t          corr;
    size_t          n_blocks;

    /* 初始化互斥锁*/
    if(__g_usb_pool_init == USB_FALSE){
#if USB_OS_EN
        __g_usb_pool_mutex = usb_mutex_create();
        if (__g_usb_pool_mutex == NULL) {
            __USB_ERR_TRACE(MutexCreateErr, "\r\n");
            return NULL;
        }
#endif
        __g_usb_pool_init = USB_TRUE;
    }

    /* 内存块必须有效
     * 内存池的大小至少是一个空闲块的大小*/
    if ((!((p_pool_mem != NULL) &&
            (pool_size >= (uint32_t)sizeof(__free_item_t)) &&
            (item_size + sizeof(__free_item_t) > item_size)))) {
        return NULL;
    }

    /* 检查内存池起始地址是否需要对齐*/
    corr = (size_t)p_pool_mem & (sizeof(__free_item_t) - 1);
    if (corr != 0) {
        corr = sizeof(__free_item_t) - corr;
        /* 减小可用的内存池的大小*/
        pool_size -= corr;
    }
    /* 设置空闲内存池起始地址*/
    p_pool->p_free = (void *)((uint8_t *)p_pool_mem + corr);

    p_pool->item_size = sizeof(__free_item_t);
    /* 计算一个内存池条目需要多少个空闲块*/
    n_blocks = 1;
    while (p_pool->item_size < item_size) {
        p_pool->item_size += sizeof(__free_item_t);
        ++n_blocks;
    }
    /* 重新设置内存池条目大小（有可能比实际的大，因为最小单位是空闲块）*/
    item_size = p_pool->item_size;

    /* 检查内存大小是否小于一个条目大小*/
    if (pool_size < item_size) {
        return NULL;
    }

    pool_size -= item_size;
    p_pool->item_count = 1;
    /* 把所有的内存池条目串成链表*/
    p_fb = (__free_item_t *)p_pool->p_free;
    while (pool_size >= item_size) {
        p_fb->p_next = &p_fb[n_blocks];
        p_fb = p_fb->p_next;
        pool_size -= (uint32_t)item_size;
        ++p_pool->item_count;
    }
    /* 最后一个条目*/
    p_fb->p_next    = NULL;
    /* 所有条目空闲 */
    p_pool->n_free  = p_pool->item_count;
    /* 空闲条目的数量的最小值*/
    p_pool->n_min   = p_pool->item_count;
    /* 内存池实际起始地址（有可能被对齐）*/
    p_pool->p_start = p_pool_mem;
    /* 最后一个条目*/
    p_pool->p_end   = p_fb;

    return p_pool;
}

/**
 * \brief 从内存池中获取一个单元
 *
 * \param[in] pool 内存池句柄
 *
 * \return 若成功，返回一个内存池单元指针；当失败时，返回NULL
 */
void *usb_pool_item_get(usb_pool_id_t pool){
    usb_pool_t    *p_pool = (usb_pool_t *)pool;
    __free_item_t *p_fb;
#if USB_OS_EN
    int            ret;
    ret = usb_mutex_lock(__g_usb_pool_mutex, USB_WAIT_FOREVER);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return NULL;
    }
#endif
    /* 获取空闲的条目*/
    p_fb = (__free_item_t *)p_pool->p_free;
    if (p_fb != NULL) {
        p_pool->p_free = p_fb->p_next;
        --p_pool->n_free;
        /* 记录历史最小值*/
        if (p_pool->n_min > p_pool->n_free) {
            p_pool->n_min = p_pool->n_free;
        }
    }
#if USB_OS_EN
    ret = usb_mutex_unlock(__g_usb_pool_mutex);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return NULL;
    }
#endif
    return p_fb;
}

/**
 * \brief 释放一个内存池单元
 *
 * \param[in] pool   内存池句柄
 * \param[in] p_item 内存池单元指针
 *
 * \retval 成功返回 USB_OK
 */
int usb_pool_item_return(usb_pool_id_t pool, void *p_item){
    usb_pool_t *p_pool = (usb_pool_t *)pool;
    int         ret    = USB_OK;

#if USB_OS_EN
    ret = usb_mutex_lock(__g_usb_pool_mutex, USB_WAIT_FOREVER);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    /* 检查释放的内存条目的地址是否在内存池的范围内*/
    if (!((p_pool->p_start <= p_item) && (p_item <= p_pool->p_end)
              && (p_pool->n_free <= p_pool->item_count))) {
        __USB_ERR_INFO("item is not belong to pool\r\n");
#if USB_OS_EN
        ret = usb_mutex_unlock(__g_usb_pool_mutex);
        if (ret != USB_OK) {
            __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
            return ret;
        }
#endif
        return -USB_EFAULT;
    }

    /* 链接回空闲链表中*/
    ((__free_item_t *)p_item)->p_next = (__free_item_t *)p_pool->p_free;
    p_pool->p_free = p_item;
    ++p_pool->n_free;
#if USB_OS_EN
    ret = usb_mutex_unlock(__g_usb_pool_mutex);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
    }
#endif
    return ret;
}

/**
 * \brief 获取当前内存池，分配余量的下界
 *
 *  用来查看内存池的使用情况，分配的峰值。
 *
 * \param[in] pool   内存池句柄
 *
 * \return 返回内存池的使用情况，分配的峰值。
 */
size_t usb_pool_margin_get(usb_pool_id_t pool){
    usb_pool_t *p_pool = (usb_pool_t *)pool;
    size_t      margin;
#if USB_OS_EN
    int         ret;
    ret = usb_mutex_lock(__g_usb_pool_mutex, USB_WAIT_FOREVER);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    margin = p_pool->n_min;
#if USB_OS_EN
    ret = usb_mutex_unlock(__g_usb_pool_mutex);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif
    return margin;
}
