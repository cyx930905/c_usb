#ifndef __USB_REG_H
#define __USB_REG_H
#include "common/usb_common.h"

/**
 * \brief 32位寄存器写函数
 */
void usb_reg_writel(uint32_t val, volatile void *p_addr);
/**
 * \brief 32位寄存器读函数
 */
uint32_t usb_reg_readl(volatile void *p_addr);
/* 小端寄存器读写*/
#define USB_LE_REG_WRITE32(val, addr)    usb_reg_writel(USB_CPU_TO_LE32(val), (volatile void *)addr)
#define USB_LE_REG_READ32(addr)          USB_CPU_TO_LE32(usb_reg_readl((volatile void *)addr))

#endif


