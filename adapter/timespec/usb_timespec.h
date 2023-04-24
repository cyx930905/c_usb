#ifndef __USB_TIMESPEC_H__
#define __USB_TIMESPEC_H__
#include "common/err/usb_err.h"
#include <stdio.h>

/* 时间戳结构体*/
struct usb_timespec {
    long ts_sec;         /* 秒*/
    long ts_nsec;        /* 纳秒*/
};

/**
 * \brief 获取当前时间戳
 */
int usb_timespec_get(struct usb_timespec *p_ts);
/**
 * \brief 通过纳秒获取时间戳
 */
struct usb_timespec usb_ns_to_timespec(const int64_t nsec);
#endif
