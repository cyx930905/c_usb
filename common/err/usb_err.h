#ifndef __USB_ERR_H
#define __USB_ERR_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "config/usb_config.h"
#include "common/usb_common.h"
#include "adapter/usb_adapter.h"

/* \brief 打印函数类型 */
typedef int (*p_fn_printf)(const char *fmt, ...);

extern p_fn_printf __g_usb_printf;

/* \brief 没定义调试等级，默认最高调试等级 */
#if !(defined(USB_DEBUG_LEVEL1) || defined(USB_DEBUG_LEVEL2) || defined(USB_DEBUG_LEVEL3))
#define USB_DEBUG_LEVEL3
#endif

#define USB_OK                0
#define USB_EPERM           501          /* 操作不允许 */
#define USB_ENOINIT         502          /* 无初始化*/
#define USB_END             504          /* 到末尾了*/
#define USB_EBADF           509          /* 描述符损坏 */
#define USB_EAGAIN          511          /* 资源不可用，需重试 */
#define USB_ENOMEM          512          /* 空间（内存）不足 */
#define USB_EFAULT          514          /* 地址错误 */
#define USB_EBUSY           516          /* 设备或资源忙 */
#define USB_EEXIST          517          /* 资源已经存在 */
#define USB_ECANCEL         518          /* 操作取消 */
#define USB_ENODEV          519          /* 设备不存在 */
#define USB_EINVAL          522          /* 无效参数 */
#define USB_EILLEGAL        523          /* 参数非法 */
#define USB_EPIPE           532          /* 损坏的管道 */
#define USB_ENOTSUP         535          /* 不支持 */
#define USB_EINPROGRESS     568          /* 操作正在进行中 */
#define USB_EPROTO          576          /* 协议错误 */
#define USB_ETIME           579          /* 超时 */
#define USB_EDATA           590          /* 数据错误*/
#define USB_EWRITE          591          /* 写错误*/
#define USB_EREAD           592          /* 读错误*/
#define USB_ESIZE           611          /* 大小错误 */
#define USB_EOVERFLOW       612          /* 过流*/

/* \brief 错误信息字符串数组*/
extern char *__g_p_err_str[];

/* \brief 错误编号*/
enum {
    ErrorTrace,
#if USB_OS_EN
    MutexCreateErr,
    MutexDelErr,
    MutexLockErr,
    MutexUnLockErr,
    SemCreateErr,
    SemDelErr,
    SemTakeErr,
    SemGiveErr,
    TaskCreateErr,
    TaskStartErr,
    TaskDelErr,
#endif
};

/* \brief 错误打印追踪函数*/
#if defined(USB_DEBUG_LEVEL2) || defined(USB_DEBUG_LEVEL3)
#define __USB_ERR_INFO(info, ...)                                                                       \
            do {                                                                                        \
                __g_usb_printf("USB: [%s:%d] "info,__FUNCTION__, __LINE__, ##__VA_ARGS__); \
            } while (0)
#else
#define __USB_ERR_INFO(info, ...)                           \
            do {                                            \
                __g_usb_printf("USB: "info, ##__VA_ARGS__); \
            } while (0)
#endif

/* \brief 打印函数*/
#define __USB_INFO(info, ...)                               \
            do {                                            \
                __g_usb_printf("USB: "info, ##__VA_ARGS__); \
            } while (0)
/* \brief 换行*/
#define __USB_INFO_NEW_LINE() __g_usb_printf("\r\n")

/* \brief 错误追踪打印函数*/
#define __USB_ERR_TRACE(code, info, ...)                    \
    __USB_ERR_INFO("%s"info, __g_p_err_str[code], ##__VA_ARGS__);

/**
 * \brief USB 打印注册函数
 */
void usb_printf_register(p_fn_printf p_fn_printf);

#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif
