/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "common/err/usb_err.h"
#include "common/usb_common.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 计算 2 的 (v - 1) 次幂
 */
int usb_ilog2(unsigned long v){
    int l = 0;
    while ((1UL << l) < v) {
        l++;
    }
    return l;
}

/**
 * \brief 无符号 64 位除法
 *
 * \param[in] dividend 被除数
 * \param[in] divisor  除数
 *
 * \retval 返回商
 */
uint64_t usb_div_u64(uint64_t dividend, uint32_t divisor){
    return dividend / divisor;
}

/**
 * \brief 有符号 64 位除法，且返回余数
 *
 * \param[in]  dividend  被除数
 * \param[in]  divisor   除数
 * \param[out] remainder 存储余数
 *
 * \retval 返回商
 */
int64_t usb_div_s64_rem(int64_t dividend, int32_t divisor, int32_t *remainder){
    *remainder = dividend % divisor;
    return dividend / divisor;
}

/**
 * \brief 从传入的地址开始，返回连续两个字节的值
 *
 * \param[in] p 起始地址
 */
uint16_t usb_get_unaligned_le16(uint8_t *p){
    if (p != NULL) {
        return ((*(p + 1) << 8) | *p);
    }
    return 0;
}

/**
 * \brief 从传入的地址开始，返回连续四个字节的值
 *
 * \param[in] p 起始地址
 */
uint32_t usb_get_unaligned_le32(uint8_t *p){
    if (p != NULL) {
        return ((*(p + 3) << 24) | (*(p + 2) << 16) | (*(p + 1) << 8) | *p);
    }
    return 0;
}

/**
 * \brief 检查内存区域指定的位是不是为 1
 *
 * \param[in] p_data 内存区域起始地址
 * \param[in] bit    指定位
 *
 * \retval 是返回 1，否则为 0
 */
int usb_test_bit(const uint8_t *p_data, int bit){
    return (p_data[bit >> 3] >> (bit & 7)) & 1;
}

/**
 * \brief 清除内存区域指定的位
 *
 * \param[in] p_data 内存区域起始地址
 * \param[in] bit    指定位
 */
void usb_clear_bit(uint8_t *p_data, int bit){
	p_data[bit >> 3] &= ~(1 << (bit & 7));
}

/* \brief 掩码定义*/
#define __UNICODE_MAX       0x0010ffff
#define __PLANE_SIZE        0x00010000

#define __SURROGATE_MASK    0xfffff800
#define __SURROGATE_PAIR    0x0000d800
#define __SURROGATE_LOW     0x00000400
#define __SURROGATE_BITS    0x000003ff

/* \brief UTF8 表结构体*/
struct __usb_utf8_tab {
    int     cmask;
    int     cval;
    int     shift;
    long    lmask;
    long    lval;
};

static const struct __usb_utf8_tab __gp_usb_utf8_tab[] = {
    {0x80,  0x00,   0*6,    0x7F,           0,         /* 1 byte sequence */},
    {0xE0,  0xC0,   1*6,    0x7FF,          0x80,      /* 2 byte sequence */},
    {0xF0,  0xE0,   2*6,    0xFFFF,         0x800,     /* 3 byte sequence */},
    {0xF8,  0xF0,   3*6,    0x1FFFFF,       0x10000,   /* 4 byte sequence */},
    {0xFC,  0xF8,   4*6,    0x3FFFFFF,      0x200000,  /* 5 byte sequence */},
    {0xFE,  0xFC,   5*6,    0x7FFFFFFF,     0x4000000, /* 6 byte sequence */},
    {0,                                                /* end of table    */}
};

/**
 * \brief UTF8 转换 UTF32
 */
static int __utf8_to_utf32(const uint8_t *s,
                           int            len,
                           uint32_t      *pu){
    unsigned long                 l;
    int                           c0, c, nc;
    const struct __usb_utf8_tab  *t;

    nc = 0;
    c0 = *s;
    l = c0;

    for (t = __gp_usb_utf8_tab; t->cmask; t++) {
        nc++;
        if ((c0 & t->cmask) == t->cval) {

            l &= t->lmask;

            if (l < t->lval || l > __UNICODE_MAX ||
                (l & __SURROGATE_MASK) == __SURROGATE_PAIR) {
                return -1;
            }

            *pu = (uint32_t)l;
            return nc;
        }

        if (len <= nc) {
            return -1;
        }

        s++;
        c = (*s ^ 0x80) & 0xFF;

        if (c & 0xC0) {
            return -1;
        }

        l = (l << 6) | c;
    }
    return -1;
}

/**
 * \brief UTF32 转换 UTF8
 */
static int __utf32s_to_utf8s(uint32_t  u,
                             uint8_t  *s,
                             int       maxout){
    unsigned long                 l;
    int                           c, nc;
    const struct __usb_utf8_tab  *t;

    if (!s) {
        return 0;
    }

    l = u;
    if (l > __UNICODE_MAX ||
        (l & __SURROGATE_MASK) == __SURROGATE_PAIR) {
        return -1;
    }

    nc = 0;
    for (t = __gp_usb_utf8_tab; t->cmask && maxout; t++, maxout--) {
        nc++;
        if (l <= t->lmask) {

            c   =   t->shift;
            *s  =   (uint8_t) (t->cval | (l >> c));

            while (c > 0) {
                c -= 6;
                s++;
                *s = (uint8_t) (0x80 | ((l >> c) & 0x3F));
            }
            return nc;
        }
    }
    return -1;
}

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
                        int             maxout){
    uint8_t        *p_op;
    int             size;
    unsigned long   u, v;

    p_op = p_str;
    while (in_len > 0 && maxout > 0) {
        u = USB_CPU_TO_LE16(*p_wcs);
        if (!u) {
            break;
        }
        p_wcs++;
        in_len--;
        if (u > 0x7f) {
            if ((u & __SURROGATE_MASK) == __SURROGATE_PAIR) {
                if (u & __SURROGATE_LOW) {
                    /* Ignore character and move on */
                    continue;
                }

                if (in_len <= 0) {
                    break;
                }

                v = USB_CPU_TO_LE16(*p_wcs);
                if ((v & __SURROGATE_MASK) != __SURROGATE_PAIR ||
                        !(v & __SURROGATE_LOW)) {
                    /* Ignore character and move on */
                    continue;
                }
                u = __PLANE_SIZE + ((u & __SURROGATE_BITS) << 10)
                        + (v & __SURROGATE_BITS);
                p_wcs++;
                in_len--;
            }

            size = __utf32s_to_utf8s(u, p_op, maxout);
            if (size == -1) {
                /* Ignore character and move on */
            } else {
            	p_op += size;
                maxout -= size;
            }

        } else {
            *p_op++ = (uint8_t) u;
            maxout--;
        }
    }
    return p_op - p_str;
}

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
                        uint16_t      *p_wcs){
    uint16_t  *p_op;
    int        size;
    uint32_t   u;

    p_op = p_wcs;

    while (*p_str && len > 0) {
        if (*p_str & 0x80) {

            size = __utf8_to_utf32(p_str, len, &u);

            if (size < 0) {
                return -USB_EINVAL;
            }

            if (u >= __PLANE_SIZE) {
                u -= __PLANE_SIZE;
                *p_op++ = (uint16_t) (__SURROGATE_PAIR | ((u >> 10) & __SURROGATE_BITS));
                *p_op++ = (uint16_t) (__SURROGATE_PAIR | __SURROGATE_LOW | (u & __SURROGATE_BITS));
            } else {
                *p_op++ = (uint16_t) u;
            }
            p_str += size;
            len -= size;
        } else {
            *p_op++ = *p_str++;
            len--;
        }
    }
    return p_op - p_wcs;
}

/**
 * \brief 将 ASCII 字符转换为十进制
 *
 * \param[in] ch ASCII字符
 *
 * \retval 成功返回对应十进制值
 */
int usb_hex_to_bin_byte(char ch){
    if ((ch >= '0') && (ch <= '9')) {
        return ch - '0';
    }
    if ((ch >= 'a') && (ch <= 'f')) {
        return ch - 'a' + 10;
    }
    if ((ch >= 'A') && (ch <= 'F')) {
        return ch - 'A' + 10;
    }
    return -1;
}

/**
 * \brief ASCII 字符转换为十六进制的值
 *
 * \param[out] p_dst 转换成十六进制的缓存
 * \param[in]  p_src ASCII 字符串
 * \param[in]  count 转换为十六进制数的长度
 *
 * \retval 成功返回0
 */
int usb_hex_to_bin(uint8_t *p_dst, const char *p_src, size_t count){
    while (count--) {
        int hi = usb_hex_to_bin_byte(*p_src++);
        int lo = usb_hex_to_bin_byte(*p_src++);

        if ((hi < 0) || (lo < 0)) {
            return -1;
        }

        *p_dst++ = (hi << 4) | lo;
    }
    return 0;
}

/**
 * \brief 把一个 n 位的有符号整数转换为一个 有符号的 32 位整数
 *
 * \param[in] value 要转换的值
 * \param[in] n     值的位数
 *
 * \retval 成功返回转换成功的值
 */
int32_t usb_sn_to_s32(uint32_t value, uint32_t n){
    switch (n) {
        case 8:  return ((int8_t)value);
        case 16: return ((int16_t)value);
        case 32: return ((int32_t)value);
	}
    return value & (1 << (n - 1)) ? value | (-1 << n) : value;
}

/**
 * \brief 把 2 个 1 字节数组转成一个小端 2 字节数值
 *
 * \param[in] p 要转换的数组
 *
 * \retval 返回转换的 16 位值
 */
uint16_t usb_be16_load(uint8_t *p){
    return (((uint16_t)p[0] << 8) | ((uint16_t)p[1] << 0));
}

/**
 * \brief 把 4 个 1 字节数组转成一个小端 4 字节数值
 *
 * \param[in] p 要转换的数组
 *
 * \retval 返回转换的 32 位值
 */
uint32_t usb_le32_load(uint8_t *p){
    return ((uint32_t)p[3] << 24) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[0] << 0);
}

/**
 * \brief 把 4 个 1 字节数组转成一个大端 4 字节数值
 *
 * \param[in] p 要转换的数组
 *
 * \retval 返回转换的 32 位值
 */
uint32_t usb_be32_load(uint8_t *p){
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           ((uint32_t)p[3] << 0);
}

/**
 * \brief 把一个 2 字节数值转换成大端 2 个 1 字节数组
 *
 * \param[in] v 2 字节值
 * \param[in] p 转成的数组
 */
void usb_be16_stor(uint16_t v, uint8_t *p){
    p[0] = (v >> 8)  & 0xFF;
    p[1] = (v >> 0)  & 0xFF;
}

/**
 * \brief 把一个 4 字节数值转换成小端 4 个 1 字节数组
 *
 * \param[in] v 4 字节值
 * \param[in] p 转成的数组
 */
void usb_le32_stor(uint32_t v, uint8_t *p){
    p[0] = (v >> 0)  & 0xFF;
    p[1] = (v >> 8)  & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

/**
 * \brief 把一个 4 字节数值转换成大端 4 个 1 字节数组
 *
 * \param[in] v 4 字节值
 * \param[in] p 转成的数组
 */
void usb_be32_stor(uint32_t v, uint8_t *p){
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8)  & 0xFF;
    p[3] = (v >> 0)  & 0xFF;
}


