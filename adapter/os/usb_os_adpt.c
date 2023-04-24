/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "usb_os.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 创建任务
 *
 * \param[in] p_name 任务名字
 * \param[in] prio   任务优先级
 * \param[in] stk_s  任务栈大小
 * \param[in] p_fn   任务函数
 * \param[in] p_arg  任务函数参数
 *
 * \retval 成功返回创建的那任务句柄
 */
__attribute__((weak)) usb_task_handle_t usb_task_create(const char  *p_name,
                                                        int          prio,
                                                        size_t       stk_s,
                                                        void       (*p_fnc)(void *p_arg),
                                                        void        *p_arg){
    return NULL;
}

/**
 * \brief 删除任务
 *
 * \param[in] p_task 要删除的任务句柄
 *
 * \retval 成功返回 USB_OK
 */
__attribute__((weak)) int usb_task_delete(usb_task_handle_t p_tsk){
    return -USB_ENOTSUP;
}

/**
 * \brief 启动任务
 *
 * \param[in] p_task 要启动的任务句柄
 *
 * \retval 成功返回 USB_OK
 */
__attribute__((weak)) int usb_task_startup(usb_task_handle_t p_tsk){
    return -USB_ENOTSUP;
}

/**
 * \brief 挂起任务
 *
 * \param[in] p_task 要挂起的任务句柄
 *
 * \retval 成功返回 USB_OK
 */
__attribute__((weak)) int usb_task_suspend(usb_task_handle_t p_tsk){
    return -USB_ENOTSUP;
}

/**
 * \brief 恢复任务
 *
 * \param[in] p_task 要恢复的任务句柄
 *
 * \retval 成功返回 USB_OK
 */
__attribute__((weak)) int usb_task_resume(usb_task_handle_t p_tsk){
    return -USB_ENOTSUP;
}

/**
 * \brief 创建信号量
 *
 * \retval 成功返回创建的信号量句柄
 */
__attribute__((weak)) usb_sem_handle_t usb_sem_create(void){
    return NULL;
}

/**
 * \brief 删除信号量
 *
 * \param[in] p_sem 要删除的信号量句柄
 *
 * \retval 成功返回 USB_OK
 */
__attribute__((weak)) int usb_sem_delete(usb_sem_handle_t p_sem){
    return -USB_ENOTSUP;
}

/**
 * \brief 等待信号量
 *
 * \param[in] p_sem   要等待的信号量句柄
 * \param[in] timeout 等待超时时间
 *
 * \retval 成功返回 USB_OK
 */
__attribute__((weak)) int usb_sem_take(usb_sem_handle_t p_sem,
                                       int              timeout){
    return -USB_ENOTSUP;
}

/**
 * \brief 释放信号量
 *
 * \param[in] p_sem 要释放的信号量句柄
 *
 * \retval 成功返回 USB_OK
 */
__attribute__((weak)) int usb_sem_give(usb_sem_handle_t p_sem){
    return -USB_ENOTSUP;
}

/**
 * \brief 创建互斥锁
 *
 * \retval 成功返回创建的互斥锁句柄
 */
__attribute__((weak)) usb_mutex_handle_t usb_mutex_create(void){
    return NULL;
}

/**
 * \brief 删除互斥锁
 *
 * \param[in] p_mutex 要删除的互斥锁句柄
 *
 * \retval 成功返回 USB_OK
 */
__attribute__((weak)) int usb_mutex_delete(usb_mutex_handle_t p_mutex){
    return -USB_ENOTSUP;
}

/**
 * \brief 互斥锁上锁
 *
 * \param[in] p_mutex 互斥锁句柄
 * \param[in] timeout 等待上锁超时时间
 *
 * \retval 成功返回 USB_OK
 */
__attribute__((weak)) int usb_mutex_lock(usb_mutex_handle_t p_mutex,
                                         int                timeout){
    return -USB_ENOTSUP;
}

/**
 * \brief 互斥锁解锁
 *
 * \param[in] p_mutex 互斥锁句柄
 *
 * \retval 成功返回 USB_OK
 */
__attribute__((weak)) int usb_mutex_unlock(usb_mutex_handle_t p_mutex){
    return -USB_ENOTSUP;
}

