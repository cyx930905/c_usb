#ifndef __USB_MEM_H
#define __USB_MEM_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include <stdio.h>
#include <stdint.h>

/* \brief USB DMA方向 */
#define USB_DMA_TO_DEVICE    0x01
#define USB_DMA_FROM_DEVICE  0x02

/**
 * \brief 分配 USB 使用的对齐的内存
 *
 * \param[in] size  分配字节长度
 * \param[in] align 对齐字节长度
 *
 * \retval 分配的内存起始地址或  NULL
 */
void *usb_mem_align_alloc(size_t size, size_t align);
/**
 * \brief 分配 USB 使用的内存
 *
 * \param[in] size  分配字节长度
 *
 * \retval 分配的内存起始地址 或 NULL
 */
void *usb_mem_alloc(size_t size);
/**
 * \brief 释放 USB 使用的内存
 *
 * \param[in] p 内存起始地址
 */
void usb_mem_free(void *p);
/**
 * \brief 数据收发前 DMA 映射操作
 *
 * \param[in] p_mem 内存起始地址
 * \param[in] size  内存大小
 * \param[in] dir   数据传输方向
 *
 * \retval 内存起始地址
 */
void *usb_dma_map(void *p_mem, size_t size, uint8_t dir);
/**
 * \brief 数据收发完成后 DMA 取消映射操作
 *
 * \param[in] p_dma 内存起始地址
 * \param[in] size  内存大小
 * \param[in] dir   数据传输方向
 *
 * \retval 内存起始地址
 */
void *usb_dma_unmap(void *p_dma, size_t size, uint8_t dir);
/**
 * \brief 申请 DMA 内存对齐
 *
 * \param[in] n_bytes 内存大小
 * \param[in] align   对齐数
 *
 * \retval 内存起始地址
 */
void *usb_cache_dma_align(size_t size, size_t align);
/**
 * \brief 释放 DMA 内存
 *
 * \param[in] ptr  内存起始地址
 * \param[in] size 内存大小
 *
 * \retval 成功返回 USB_OK
 */
int usb_cache_dma_free(void *p, uint32_t size);


#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif /* __USB_MEM_H */

/* end of file */
