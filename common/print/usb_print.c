/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <stdio.h>
#include "config/usb_config.h"
#include "common/err/usb_err.h"

/*******************************************************************************
 * Statement
 ******************************************************************************/
int usb_printf_none(const char *fmt, ...);

/*******************************************************************************
 * Global
 ******************************************************************************/
/* \brief 错误信息字符串数组*/
char *__g_p_err_str[] = {
    "error trace",
#if USB_OS_EN
    "mutex create failed",
    "mutex delete failed",
    "mutex lock failed",
    "mutex unlock failed",
    "semaphore create failed",
    "semaphore delete failed",
    "semaphore take failed",
    "semaphore give failed",
    "task create failed",
    "task start failed",
    "task delete failed",
#endif
};

/* \brief USB 打印函数 */
p_fn_printf __g_usb_printf = usb_printf_none;

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief USB 无打印函数
 */
int usb_printf_none(const char *fmt, ...){
    return USB_OK;
}

/**
 * \brief USB 打印注册函数
 */
void usb_printf_register(p_fn_printf p_fn_printf){
    __g_usb_printf = p_fn_printf;
}

