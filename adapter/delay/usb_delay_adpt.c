/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "stdio.h"
#include "stdint.h"

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief USB 延时毫秒函数适配
 */
__attribute__((weak)) void usb_mdelay(uint32_t ms){
}