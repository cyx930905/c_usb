#ifndef __USB_COMMON_H
#define __USB_COMMON_H

#include <stddef.h>
#include <stdint.h>

/* \brief USB 名字长度*/
#define USB_NAME_LEN       32

#define USB_WAIT_FOREVER  -1
#define USB_NO_WAIT        0

/* \brief 空指针 */
#ifndef NULL
#define NULL       ((void *)0)
#endif
/* \brief 布尔类型*/
typedef int             usb_bool_t;
#define USB_TRUE         (1)
#define USB_FALSE        (0)
/* \brief 引用计数类型*/
typedef int             usb_refcnt;

#define USB_UINT_MAX     (~0U)
#define USB_INT_MAX      ((int)(~0U >> 1))
#define USB_LONG_MAX     ((long)(~0UL >> 1))
#define USB_INT_MIN      (-USB_INT_MAX - 1)
#define USB_ULONG_MAX    (~0UL)

/* \brief 倍数向上舍入 */
#define USB_DIV_ROUND_UP(n, d)   (((n) + (d) - 1) / (d))
/* \brief 倍数向上舍入 */
#define USB_ROUND_UP(x,n) (((x) + (n) - 1u) & ~((n) - 1u))

/**
 * \brief 获取2个数中的较大的数值
 *
 * \param[in] x 数字1
 * \param[in] y 数字2
 *
 * \retval 2个数中的较大的数值
 */
#ifndef max
#define max(x, y)   (((x) < (y)) ? (y) : (x))
#endif

/**
 * \brief 获取2个数中的较小的数值
 *
 * \param[in] x 数字1
 * \param[in] y 数字2
 *
 * \retval 2个数中的较小数值
 */
#ifndef min
#define min(x, y)   (((x) < (y)) ? (x) : (y))
#endif

/* \brief 1秒等于纳秒数*/
#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC           1000000000ULL
#endif

/* 大小端定义*/
#define USB_LITTLE_ENDIAN 1234       /* \brief 目标机器小端 */
#define USB_BIG_ENDIAN    3412       /* \brief 目标机器大端 */
#define CPU_ENDIAN USB_LITTLE_ENDIAN

/**
 * \brief USB字节序转换
 */
#if (CPU_ENDIAN == USB_BIG_ENDIAN)
#define USB_CPU_TO_LE32(x) \
            x = ((((x) & 0xff000000) >> 24) | \
             (((x) & 0x00ff0000) >> 8) | \
             (((x) & 0x0000ff00) << 8) | (((x) & 0x000000ff) << 24))

#define USB_CPU_TO_LE16(x) \
            x =(uint16_t) ((((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8))
#else
#define USB_CPU_TO_LE32(x) (x)
#define USB_CPU_TO_LE16(x) (x)
#endif

/* \brief 计算数组元素个数*/
#define USB_NELEMENTS(array) (sizeof (array) / sizeof ((array) [0]))
/* \brief 对齐*/
#define USB_ALIGN(x, a)    (((x) + ((a) - 1)) & ~((a) - 1))

#define USB_CONTAINER_OF(ptr, type, member) \
        ((type *)((char *)(ptr) - offsetof(type,member)))

/**
 * \brief 通过结构体成员指针获取包含该结构体成员的结构体，
 *        同 \ref CONTAINER_OF 一样
 */
#ifndef usb_container_of
#define usb_container_of(ptr, type, member)     USB_CONTAINER_OF(ptr, type, member)
#endif


#ifndef prefetch
#define prefetch(x) (void)0
#endif

/* \brief 检查一个字节里有多少个1*/
#define usb_hweight8(w)         \
     ((unsigned int)         \
     ((!!((w) & (1ULL << 0))) + \
      (!!((w) & (1ULL << 1))) + \
      (!!((w) & (1ULL << 2))) + \
      (!!((w) & (1ULL << 3))) + \
      (!!((w) & (1ULL << 4))) + \
      (!!((w) & (1ULL << 5))) + \
      (!!((w) & (1ULL << 6))) + \
      (!!((w) & (1ULL << 7)))))

/* \brief 四字符代码 (FOURCC) */
#define usb_fourcc(a, b, c, d)\
    ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#define usb_fourcc_be(a, b, c, d)  (v4l2_fourcc(a, b, c, d) | (1 << 31))

/* \brief 约束边界值*/
#define usb_clamp(val, lo, hi)   min(max(val, lo), hi)

/**
 * \brief 计算 2 的 (v - 1) 次幂
 */
int usb_ilog2(unsigned long v);
/**
 * \brief 无符号 64 位除法
 *
 * \param[in] dividend 被除数
 * \param[in] divisor  除数
 *
 * \retval 返回商
 */
uint64_t usb_div_u64(uint64_t dividend, uint32_t divisor);
/**
 * \brief 有符号 64 位除法，且返回余数
 *
 * \param[in]  dividend  被除数
 * \param[in]  divisor   除数
 * \param[out] remainder 存储余数
 *
 * \retval 返回商
 */
int64_t usb_div_s64_rem(int64_t dividend, int32_t divisor, int32_t *remainder);
/**
 * \brief 从传入的地址开始，返回连续两个字节的值
 *
 * \param[in] p 起始地址
 */
uint16_t usb_get_unaligned_le16(uint8_t *p);

/**
 * \brief 从传入的地址开始，返回连续四个字节的值
 *
 * \param[in] p 起始地址
 */
uint32_t usb_get_unaligned_le32(uint8_t *p);
/**
 * \brief 检查内存区域指定的位是不是为 1
 *
 * \param[in] p_data 内存区域起始地址
 * \param[in] bit    指定位
 *
 * \retval 是返回 1，否则为 0
 */
int usb_test_bit(const uint8_t *p_data, int bit);
/**
 * \brief 清除内存区域指定的位
 *
 * \param[in] p_data 内存区域起始地址
 * \param[in] bit    指定位
 */
void usb_clear_bit(uint8_t *p_data, int bit);
/**
 * \brief utf16 编码字符串转 utf8 编码字符串
 *
 * \param[in]  p_wcs   utf16 编码源字符串
 * \param[in]  in_len  字符串长度
 * \param[out] p_str   转化的 utf8 编码字符串目标缓存
 * \param[in]  maxout  最大转换长度
 *
 * \retval 实际转换完成长度
 */
int usb_utf16s_to_utf8s(const uint16_t *p_wcs,
                        int             in_len,
                        uint8_t        *p_str,
                        int             maxout);
/**
 * \brief utf8 编码字符串转 utf16 编码字符串
 *
 * \param[in]  p_str  utf8 编码源字符串
 * \param[in]  len    字符串字节长度
 * \param[out] p_wcs  转化的 utf16 编码字符串目标缓存
 *
 * \retval 实际转换的字节长度
 */
int usb_utf8s_to_utf16s(const uint8_t *p_str,
                        int            len,
                        uint16_t      *p_wcs);
/**
 * \brief 将 ASCII 字符转换为十进制
 *
 * \param[in] ch ASCII字符
 *
 * \retval 成功返回对应十进制值
 */
int usb_hex_to_bin_byte(char ch);
/**
 * \brief ASCII 字符转换为十六进制的值
 *
 * \param[out] p_dst 转换成十六进制的缓存
 * \param[in]  p_src ASCII 字符串
 * \param[in]  count 转换为十六进制数的长度
 *
 * \retval 成功返回 0
 */
int usb_hex_to_bin(uint8_t *p_dst, const char *p_src, size_t count);
/**
 * \brief 把一个 n 位的有符号整数转换为一个 有符号的 32 位整数
 *
 * \param[in] value 要转换的值
 * \param[in] n     值的位数
 *
 * \retval 成功返回转换成功的值
 */
int32_t usb_sn_to_s32(uint32_t value, uint32_t n);
/**
 * \brief 把 2 个 1 字节数组转成一个小端 2 字节数值
 *
 * \param[in] p 要转换的数组
 *
 * \retval 返回转换的 16 位值
 */
uint16_t usb_be16_load(uint8_t *p);
/**
 * \brief 把 4 个 1 字节数组转成一个小端 4 字节数值
 *
 * \param[in] p 要转换的数组
 *
 * \retval 返回转换的 32 位值
 */
uint32_t usb_le32_load(uint8_t *p);
/**
 * \brief 把 4 个 1 字节数组转成一个大端 4 字节数值
 *
 * \param[in] p 要转换的数组
 *
 * \retval 返回转换的 32 位值
 */
uint32_t usb_be32_load(uint8_t *p);
/**
 * \brief 把一个 2 字节数值转换成大端 2 个 1 字节数组
 *
 * \param[in] v 2 字节值
 * \param[in] p 转成的数组
 */
void usb_be16_stor(uint16_t v, uint8_t *p);
/**
 * \brief 把一个 4 字节数值转换成小端 4 个 1 字节数组
 *
 * \param[in] v 4 字节值
 * \param[in] p 转成的数组
 */
void usb_le32_stor(uint32_t v, uint8_t *p);
/**
 * \brief 把一个 4 字节数值转换成大端 4 个 1 字节数组
 *
 * \param[in] v 4 字节值
 * \param[in] p 转成的数组
 */
void usb_be32_stor(uint32_t v, uint8_t *p);
#endif






