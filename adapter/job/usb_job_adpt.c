/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <stdint.h>
#include <stdio.h>
#include "common/err/usb_err.h"

/********************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 创建一个USB工作，如果不支持，请返回NULL
 *
 * \param[in] p_func_job 工作运行的函数
 * \param[in] p_arg      函数参数
 *
 * \retval 成功返回创建的工作句柄
 */
__attribute__((weak)) void* usb_job_create(void (*p_func_job)(void *p_arg), void *p_arg){
    return NULL;
}

/**
 * \brief 销毁 USB 工作
 *
 * \param[in] p_job 要销毁的工作句柄
 *
 * \retval 成功返回 USB_OK
 */
__attribute__((weak)) int usb_job_destory(void *p_job){
    return -USB_ENOTSUP;
}

/**
 * \brief 启动 USB 工作
 *
 * \param[in] p_job 要启动的工作句柄
 *
 * \retval 成功返回 USB_OK
 */
__attribute__((weak)) int usb_job_start(void *p_job){
    return -USB_ENOTSUP;
}
