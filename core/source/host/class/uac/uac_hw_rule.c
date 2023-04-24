#include "host/class/uac/uac_pcm.h"
#include "host/class/uac/usbh_uac_driver.h"
#include "host/class/uac/uac_quirks.h"
#include "host/class/uac/uac_endpoint.h"
#include "host/class/uac/uac_clock.h"
#include "host/class/uac/uac_hw_rule.h"
#include "host/class/uac/usbh_uac_operation.h"
#include "usb_config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * 优化PCM硬件区间信息
 *
 * @param i 要重新设置的区间信息
 * @param v 引用的区间信息
 *
 * @return 值改变了返回非0，没改变返回0
 */
int uac_pcm_hw_interval_refine(struct uac_pcm_hw_interval *i, const struct uac_pcm_hw_interval *v)
{
    int changed = 0;

    /* 检查硬件区间信息是否为空*/
    if (uac_pcm_hw_interval_empty(i)){
        return -USB_EINVAL;
    }
    /* 重新设置最低值*/
    if (i->min < v->min) {
        i->min = v->min;
        i->openmin = v->openmin;
        changed = 1;
    } else if ((i->min == v->min) && (!i->openmin && v->openmin)) {
        /* 设置开区间最小值*/
        i->openmin = 1;
        changed = 1;
    }
    /* 重新设置最大值*/
    if (i->max > v->max) {
        i->max = v->max;
        i->openmax = v->openmax;
        changed = 1;
    } else if ((i->max == v->max) && (!i->openmax && v->openmax)) {
        /* 设置开区间最大值*/
        i->openmax = 1;
        changed = 1;
    }
    /* 是否是固定整数值*/
    if ((!i->integer) && (v->integer)) {
        i->integer = 1;
        changed = 1;
    }
    if (i->integer) {
        /* 如果是固定整数值且是范围是开区间*/
        if (i->openmin) {
            i->min++;
            i->openmin = 0;
        }
        if (i->openmax) {
            i->max--;
            i->openmax = 0;
        }
    } else if ((!i->openmin) && (!i->openmax) && (i->min == i->max)){
        /* 闭区间范围，且最大值等于最小值，则是固定整数值*/
        i->integer = 1;
    }
    /* 最后检查区间信息是否有效*/
    if (uac_pcm_hw_interval_checkempty(i)) {
        /* 无效，设置为无效*/
        uac_pcm_hw_interval_none(i);
        return -USB_EINVAL;
    }
    return changed;
}

/** 计算两个硬件区间信息的乘积*/
void uac_pcm_hw_mul(const struct uac_pcm_hw_interval *a, const struct uac_pcm_hw_interval *b,
                    struct uac_pcm_hw_interval *c)
{
    /* 至少有一个是无效信息，则设置为无效*/
    if ((a->empty) || (b->empty)) {
        uac_pcm_hw_interval_none(c);
        return;
    }
    c->empty = 0;
    /* 计算两个最小值的乘积*/
    c->min = mul(a->min, b->min);
    c->openmin = (a->openmin || b->openmin);
    /* 计算两个最大值的乘积*/
    c->max = mul(a->max,  b->max);
    c->openmax = (a->openmax || b->openmax);
    c->integer = (a->integer && b->integer);
}

/**
 * 用除法优化区间值
 *
 * @param a 被除数
 * @param b 除数
 * @param c 商
 *
 * c = a / b
 *
 * @return 值改变了返回非0，没改变返回0
 */
void uac_pcm_hw_div(const struct uac_pcm_hw_interval *a, const struct uac_pcm_hw_interval *b,
                    struct uac_pcm_hw_interval *c){
    uint32_t r;
    /* 至少有一个是无效信息，则设置为无效*/
    if ((a->empty) || (b->empty)) {
        uac_pcm_hw_interval_none(c);
        return;
    }
    c->empty = 0;
    /* 最小值，计算两个值的除数*/
    c->min = div32(a->min, b->max, &r);
    c->openmin = (r || a->openmin || b->openmax);
    if (b->min > 0) {
        /* 最大值，计算两个值的除数*/
        c->max = div32(a->max, b->min, &r);
        /* 有余数，最大值加1，且为开区间*/
        if (r) {
            c->max++;
            c->openmax = 1;
        } else{
            c->openmax = (a->openmax || b->openmin);
        }
    } else {
        c->max = USB_UINT_MAX;
        c->openmax = 0;
    }
    c->integer = 0;
}

/**
 * 优化间隔值
 *
 * @param a 被除数1
 * @param k 被除数2(作为整数)
 * @param b 除数
 * @param c 结果
 *
 * c = a * k / b
 *
 * @return 值改变了返回非0，没改变返回0
 */
void uac_pcm_hw_mulkdiv(const struct uac_pcm_hw_interval *a, uint32_t k,
                        const struct uac_pcm_hw_interval *b, struct uac_pcm_hw_interval *c){
    uint32_t r;
    /* 至少有一个是无效信息，则设置为无效*/
    if ((a->empty) || (b->empty)) {
        uac_pcm_hw_interval_none(c);
        return;
    }
    c->empty = 0;
    c->min = muldiv32(a->min, k, b->max, &r);
    c->openmin = (r || a->openmin || b->openmax);
    if (b->min > 0) {
        c->max = muldiv32(a->max, k, b->min, &r);
        /* 有余数，最大值加1，且为开区间*/
        if (r) {
            c->max++;
            c->openmax = 1;
        } else
            c->openmax = (a->openmax || b->openmin);
    } else {
        c->max = USB_UINT_MAX;
        c->openmax = 0;
    }
    c->integer = 0;
}

/**
 * 优化间隔值
 *
 * @param a 被除数1
 * @param b 被除数2
 * @param k 除数(作为整数)
 * @param c 结果
 *
 * c = a * b / k
 *
 * @return 值改变了返回非0，没改变返回0
 */
void uac_pcm_hw_muldivk(const struct uac_pcm_hw_interval *a, const struct uac_pcm_hw_interval *b,
                        uint32_t k, struct uac_pcm_hw_interval *c)
{
    uint32_t r;
    /* 至少有一个是无效信息，则设置为无效*/
    if ((a->empty) || (b->empty)) {
        uac_pcm_hw_interval_none(c);
        return;
    }
    c->empty = 0;
    c->min = muldiv32(a->min, b->min, k, &r);
    c->openmin = (r || a->openmin || b->openmin);
    c->max = muldiv32(a->max, b->max, k, &r);
    /* 有余数，最大值加1，且为开区间*/
    if (r) {
        c->max++;
        c->openmax = 1;
    } else
        c->openmax = (a->openmax || b->openmax);
    c->integer = 0;
}

/* 计算两个参数的乘积*/
static int uac_pcm_hw_rule_mul(struct uac_pcm_hw_params *params,
                               struct uac_pcm_hw_rule *rule)
{
    struct uac_pcm_hw_interval t;
    uac_pcm_hw_mul(hw_param_hw_interval_c(params, rule->deps[0]),
                   hw_param_hw_interval_c(params, rule->deps[1]), &t);
    return uac_pcm_hw_interval_refine(hw_param_hw_interval(params, rule->var), &t);
}

/* 计算两个参数相除*/
static int uac_pcm_hw_rule_div(struct uac_pcm_hw_params *params,
                               struct uac_pcm_hw_rule *rule)
{
    struct uac_pcm_hw_interval t;
    uac_pcm_hw_div(hw_param_hw_interval_c(params, rule->deps[0]),
                   hw_param_hw_interval_c(params, rule->deps[1]), &t);
    return uac_pcm_hw_interval_refine(hw_param_hw_interval(params, rule->var), &t);
}

/** 计算参数0乘rule->private除以参数1*/
static int uac_pcm_hw_rule_mulkdiv(struct uac_pcm_hw_params *params,
                                   struct uac_pcm_hw_rule *rule)
{
    struct uac_pcm_hw_interval t;
    uac_pcm_hw_mulkdiv(hw_param_hw_interval_c(params, rule->deps[0]),
                       (unsigned long) rule->private,
                       hw_param_hw_interval_c(params, rule->deps[1]), &t);
    return uac_pcm_hw_interval_refine(hw_param_hw_interval(params, rule->var), &t);
}

/** 计算参数0乘参数1除以rule->private*/
static int uac_pcm_hw_rule_muldivk(struct uac_pcm_hw_params *params,
                                   struct uac_pcm_hw_rule *rule)
{
    struct uac_pcm_hw_interval t;
    uac_pcm_hw_muldivk(hw_param_hw_interval_c(params, rule->deps[0]),
                       hw_param_hw_interval_c(params, rule->deps[1]),
                       (unsigned long) rule->private, &t);
    return uac_pcm_hw_interval_refine(hw_param_hw_interval(params, rule->var), &t);
}

/** 给一个区间信息申请一个整数限制*/
int uac_pcm_hw_constraint_integer(struct uac_pcm_runtime *runtime, uac_pcm_hw_param_t var)
{
    struct uac_pcm_hw_constraints *constrs = &runtime->hw_constraints;
    return uac_pcm_hw_interval_setinteger(constrs_hw_interval(constrs, var));
}

/**
 * 从列表中优化区间值
 *
 * @param i     要优化的区间值
 * @param count 列表元素的数量
 * @param list  列表
 * @param mask
 *
 * @return 返回1表示值改变，返回0表示无改变
 */
int uac_interval_list(struct uac_pcm_hw_interval *i, uint32_t count,
                      const uint32_t *list, uint32_t mask){
    uint32_t k;
    struct uac_pcm_hw_interval list_range;

    if (!count) {
        i->empty = 1;
        return -USB_EINVAL;
    }
    uac_pcm_hw_interval_any(&list_range);
    list_range.min = USB_UINT_MAX;
    list_range.max = 0;
    for (k = 0; k < count; k++) {
        if (mask && !(mask & (1 << k)))
            continue;
        if (!uac_pcm_hw_interval_test(i, list[k]))
            continue;
        list_range.min = min(list_range.min, list[k]);
        list_range.max = max(list_range.max, list[k]);
    }
    return uac_pcm_hw_interval_refine(i, &list_range);
}

/** 把区间信息的最大值设置为最小值*/
static int uac_interval_refine_first(struct uac_pcm_hw_interval *i)
{
    /* 无效的区间信息*/
    if (uac_pcm_hw_interval_empty(i)){
        return -USB_EINVAL;
    }
    /* 检查这个区间信息是否只有唯一值*/
    if (uac_pcm_hw_interval_single(i)){
        return 0;
    }
    i->max = i->min;
    i->openmax = i->openmin;
    /* 开区间*/
    if (i->openmax){
        i->max++;
    }
    return 1;
}

/** 把区间信息的最小值设置为最大值*/
static int uac_interval_refine_last(struct uac_pcm_hw_interval *i)
{
    /* 无效的区间信息*/
    if (uac_pcm_hw_interval_empty(i)){
        return -USB_EINVAL;
    }
    /* 检查这个区间信息是否只有唯一值*/
    if (uac_pcm_hw_interval_single(i)){
        return 0;
    }
    i->min = i->max;
    i->openmin = i->openmax;
    /* 开区间*/
    if (i->openmin){
        i->min--;
    }
    return 1;
}

/**
 * 设置PCM硬件容器最小最大值
 *
 * @param runtime USB音频运行时间结构体
 * @param var     具体硬件参数
 * @param min     最小值
 * @param max     最大值
 *
 * @return 返回1表示值改变，返回0表示值没改变
 */
int uac_pcm_hw_constraint_minmax(struct uac_pcm_runtime *runtime, uac_pcm_hw_param_t var,
                                 uint32_t min, uint32_t max){
    struct uac_pcm_hw_constraints *constrs = &runtime->hw_constraints;
    struct uac_pcm_hw_interval t;
    t.min = min;
    t.max = max;
    t.openmin = t.openmax = 0;
    t.integer = 0;
    return uac_pcm_hw_interval_refine(constrs_hw_interval(constrs, var), &t);
}

/**
 * USB音频PCM硬件格式规则
 *
 * 检查格式的位宽度
 */
static int uac_pcm_hw_rule_format(struct uac_pcm_hw_params *params,
                                  struct uac_pcm_hw_rule *rule)
{
    uint32_t k;
    struct uac_pcm_hw_interval *i = hw_param_hw_interval(params, rule->deps[0]);

    for (k = 0; k <= SNDRV_PCM_FORMAT_LAST; ++k) {
        int bits;
        if (params->pcm_format_size_bit != k){
            continue;
        }
        bits = uac_pcm_format_physical_width(k);
        /* 忽略不可用的格式*/
        if (bits <= 0){
            continue;
        }
        if (((uint32_t)bits < i->min) || ((uint32_t)bits > i->max)){
            params->pcm_format_size_bit = 0;
        }
    }
    return 0;
}
/**
 * USB音频PCM硬件采样位宽规则
 *
 * @param params  PCM硬件参数
 * @param rule    PCM硬件规则结构体
 *
 * @return 返回1表示值改变，返回0表示值没改变
 */
static int uac_pcm_hw_rule_sample_bits(struct uac_pcm_hw_params *params,
                                       struct uac_pcm_hw_rule *rule)
{
    struct uac_pcm_hw_interval t;
    uint32_t k;
    t.min = USB_UINT_MAX;
    t.max = 0;
    t.openmin = 0;
    t.openmax = 0;
    for (k = 0; k <= SNDRV_PCM_FORMAT_LAST; ++k) {
        int bits;
        if (params->pcm_format_size_bit != k)
            continue;
        /* 获取格式位宽*/
        bits = uac_pcm_format_physical_width(k);
        /* 忽略不可用的格式*/
        if (bits <= 0){
            continue;
        }
        if (t.min > (uint32_t)bits)
            t.min = bits;
        if (t.max < (uint32_t)bits)
            t.max = bits;
    }
    /* 格式位宽是固定整数*/
    t.integer = 1;
    return uac_pcm_hw_interval_refine(hw_param_hw_interval(params, rule->var), &t);
}

/** 检查格式是否有效*/
static usb_bool_t hw_check_valid_format(struct usbh_uac_substream *subs,
                                        struct uac_pcm_hw_params *params,
                                        struct audioformat *fp){
    struct uac_pcm_hw_interval *it = hw_param_hw_interval(params, UAC_PCM_HW_PARAM_RATE);
    struct uac_pcm_hw_interval *ct = hw_param_hw_interval(params, UAC_PCM_HW_PARAM_CHANNELS);
    struct uac_pcm_hw_interval *pt = hw_param_hw_interval(params, UAC_PCM_HW_PARAM_PERIOD_TIME);
    uint32_t ptime;

    /* 检查格式*/
    if(params->format != fp->formats){
        __UAC_RULE_SPACE("> check: no supported format %d\n", fp->formats);
        return USB_FALSE;
    }
    /* 检查通道*/
    if ((fp->channels < ct->min) || (fp->channels > ct->max)) {
        __UAC_RULE_SPACE("> check: no valid channels %d (%d/%d)\r\n", fp->channels, ct->min, ct->max);
        return USB_FALSE;
    }
    /* 检查采样率是否在范围内*/
    if ((fp->rate_min > it->max) || (fp->rate_min == it->max && it->openmax)) {
        __UAC_RULE_SPACE("> check: rate_min %d > max %d\n", fp->rate_min, it->max);
        return USB_FALSE;
    }
    if ((fp->rate_max < it->min) || (fp->rate_max == it->min && it->openmin)) {
        __UAC_RULE_SPACE("> check: rate_max %d < min %d\n", fp->rate_max, it->min);
        return USB_FALSE;
    }
    /* 检查是否周期时间大于等于数据包间隔时间*/
    if (subs->speed != USB_SPEED_FULL) {
        ptime = 125 * (1 << fp->datainterval);
        if ((ptime > pt->max) || (ptime == pt->max && pt->openmax)) {
            __UAC_RULE_SPACE("> check: ptime %u > max %u\r\n", ptime, pt->max);
            return USB_FALSE;
        }
    }
    return USB_TRUE;
}

/**
 * 采样率规则函数
 *
 * @param params PCM硬件参数
 * @param rule   规则结构体
 *
 * @return 返回1表示值改变，返回0表示值没改变
 */
int hw_rule_rate(struct uac_pcm_hw_params *params,
                 struct uac_pcm_hw_rule *rule)
{
    struct usbh_uac_substream *subs = rule->private;
    struct audioformat *fp;
    struct uac_pcm_hw_interval *it = hw_param_hw_interval(params, UAC_PCM_HW_PARAM_RATE);
    uint32_t rmin, rmax;
    int changed;

    __UAC_RULE_TRACE("hw_rule_rate: (%d,%d)\r\n", it->min, it->max);
    changed = 0;
    rmin = rmax = 0;
    /* 获取所有格式的最小值和最大值*/
    usb_list_for_each_entry(fp, &subs->fmt_list, struct audioformat, list) {
        /* 检查是否有效的格式*/
        if (!hw_check_valid_format(subs, params, fp))
            continue;
        if (changed++) {
            if (rmin > fp->rate_min)
                rmin = fp->rate_min;
            if (rmax < fp->rate_max)
                rmax = fp->rate_max;
        } else {
            rmin = fp->rate_min;
            rmax = fp->rate_max;
        }
    }

    if (!changed) {
        __UAC_RULE_SPACE("--> not changed\r\n");
        it->empty = 1;
        return -USB_EINVAL;
    }

    changed = 0;
    /* 重新规范采样率区间*/
    if (it->min < rmin) {
        it->min = rmin;
        it->openmin = 0;
        changed = 1;
    }
    if (it->max > rmax) {
        it->max = rmax;
        it->openmax = 0;
        changed = 1;
    }
    /* 检查硬件区间信息是否有效*/
    if (uac_pcm_hw_interval_checkempty(it)) {
        it->empty = 1;
        return -USB_EINVAL;
    }
    __UAC_RULE_SPACE("--> (%d, %d) (changed = %d)\r\n", it->min, it->max, changed);
    return changed;
}

/**
 * 通道数规则函数
 *
 * @param params PCM硬件参数
 * @param rule   规则结构体
 *
 * @return 返回1表示值改变，返回0表示值没改变
 */
int hw_rule_channels(struct uac_pcm_hw_params *params,
                     struct uac_pcm_hw_rule *rule)
{
    struct usbh_uac_substream *subs = rule->private;
    struct audioformat *fp;
    struct uac_pcm_hw_interval *it = hw_param_hw_interval(params, UAC_PCM_HW_PARAM_CHANNELS);
    uint32_t rmin, rmax;
    int changed;

    __UAC_RULE_TRACE("hw_rule_channels: (%d,%d)\r\n", it->min, it->max);
    changed = 0;
    rmin = rmax = 0;
    /* 获取所有格式的最小值和最大值*/
    usb_list_for_each_entry(fp, &subs->fmt_list, struct audioformat, list) {
        /* 检查是否有效的格式*/
        if (!hw_check_valid_format(subs, params, fp))
            continue;
        if (changed++) {
            if (rmin > fp->channels)
                rmin = fp->channels;
            if (rmax < fp->channels)
                rmax = fp->channels;
        } else {
            rmin = fp->channels;
            rmax = fp->channels;
        }
    }

    if (!changed) {
        __UAC_RULE_SPACE("--> not changed\r\n");
        it->empty = 1;
        return -USB_EINVAL;
    }
    /* 重新规范通道区间*/
    changed = 0;
    if (it->min < rmin) {
        it->min = rmin;
        it->openmin = 0;
        changed = 1;
    }
    if (it->max > rmax) {
        it->max = rmax;
        it->openmax = 0;
        changed = 1;
    }
    /* 检查硬件区间信息是否有效*/
    if (uac_pcm_hw_interval_checkempty(it)) {
        it->empty = 1;
        return -USB_EINVAL;
    }
    __UAC_RULE_SPACE("--> (%d, %d) (changed = %d)\r\n", it->min, it->max, changed);
    return changed;
}

/**
 * 音频格式规则函数
 *
 * @param params PCM硬件参数
 * @param rule   规则结构体
 *
 * @return 返回1表示值改变，返回0表示值没改变
 */
int hw_rule_format(struct uac_pcm_hw_params *params,
                   struct uac_pcm_hw_rule *rule)
{
    //todo
    return 0;
}

/**
 * 周期时间规则函数
 *
 * @param params PCM硬件参数
 * @param rule   规则结构体
 *
 * @return 返回1表示值改变，返回0表示值没改变
 */
int hw_rule_period_time(struct uac_pcm_hw_params *params,
                        struct uac_pcm_hw_rule *rule)
{
    //todo
    return 0;
}

/* 采样率*/
static uint32_t rates[] = { 5512, 8000, 11025, 16000, 22050, 32000, 44100,
                            48000, 64000, 88200, 96000, 176400, 192000 };

const struct uac_pcm_hw_constraint_list uac_pcm_known_rates1 = {
    .count = USB_NELEMENTS(rates),
    .list = rates,
};

/** 采样率规则函数*/
static int uac_pcm_hw_rule_rate(struct uac_pcm_hw_params *params,
                                struct uac_pcm_hw_rule *rule)
{
    struct uac_pcm_hardware *hw = rule->private;
    return uac_interval_list(hw_param_hw_interval(params, rule->var),
                             uac_pcm_known_rates1.count,
                             uac_pcm_known_rates1.list, hw->rates);
}

/** 最大缓存字节大小规则*/
static int uac_pcm_hw_rule_buffer_bytes_max(struct uac_pcm_hw_params *params,
                        struct uac_pcm_hw_rule *rule)
{
    struct uac_pcm_hw_interval t;
    struct uac_pcm_substream *substream = rule->private;
    t.min = 0;
    t.max = substream->buffer_bytes_max;
    t.openmin = 0;
    t.openmax = 0;
    t.integer = 1;
    return uac_pcm_hw_interval_refine(hw_param_hw_interval(params, rule->var), &t);
}

/**
 * 优化PCM硬件参数
 *
 * @param substream PCM子数据流结构体
 * @param params    硬件参数
 *
 * @return 成功返回USB_OK
 */
usb_err_t uac_pcm_hw_refine(struct uac_pcm_substream *substream,
                            struct uac_pcm_hw_params *params)
{
    uint32_t k;
    //struct uac_pcm_hardware *hw;
    struct uac_pcm_hw_interval *i = NULL;
    struct uac_pcm_hw_constraints *constrs = &substream->runtime->hw_constraints;
    uint32_t rstamps[constrs->rules_num];
    uint32_t vstamps[UAC_PCM_HW_PARAM_LAST_INFO + 1];
    uint32_t stamp = 2;
    int changed, again = 0;

    if (params->rmask & (1 << UAC_PCM_HW_PARAM_SAMPLE_BITS)){
        params->msbits = 0;
    }
    if (params->rmask & (1 << UAC_PCM_HW_PARAM_RATE)) {
        params->rate_num = 0;
        params->rate_den = 0;
    }

    /* 遍历所有硬件参数条件*/
    for (k = UAC_PCM_HW_PARAM_FIRST_INFO; k <= UAC_PCM_HW_PARAM_LAST_INFO; k++) {
        /* 获取具体的区间信息*/
        i = hw_param_hw_interval(params, k);
        /* 检查区间信息是否有效*/
        if (uac_pcm_hw_interval_empty(i)){
            return -USB_EINVAL;
        }
        /* 检查有没有变化请求*/
        if (!(params->rmask & (1 << k)))
            continue;
        /* 优化配置参数*/
        changed = uac_pcm_hw_interval_refine(i, constrs_hw_interval(constrs, k));
        if (changed > 0){
            params->cmask |= 1 << k;
        }
        if (changed < 0){
            return changed;
        }
    }

    for (k = 0; k < constrs->rules_num; k++){
        rstamps[k] = 0;
    }
    for (k = 0; k <= UAC_PCM_HW_PARAM_LAST_INFO; k++){
        vstamps[k] = (params->rmask & (1 << k)) ? 1 : 0;
    }
    do {
        again = 0;
        int i = 0;
        for (k = 0; k < constrs->rules_num; k++) {
            struct uac_pcm_hw_rule *r = &constrs->rules[k];
            uint32_t d;
            int doit = 0;

            /* 检查*/
            for (d = 0; r->deps[d] >= 0; d++) {
                if (vstamps[r->deps[d]] > rstamps[k]) {
                    doit = 1;
                    break;
                }
            }
            if (!doit)
                continue;
            /* 执行规则函数*/
            changed = r->func(params, r);
            i++;
//            rstamps[k] = stamp;
            if ((changed > 0) && (r->var >= 0)) {
                params->cmask |= (1 << r->var);
                vstamps[r->var] = stamp;
                again = 1;
            }
            if (changed < 0){
                usb_printf("%d\r\n",i);
                return changed;
            }
//            stamp++;
        }
    } while (again);

    if (!params->msbits) {
        i = hw_param_hw_interval(params, SNDRV_PCM_HW_PARAM_SAMPLE_BITS);
        if (uac_pcm_hw_interval_single(i)){
            params->msbits = uac_pcm_hw_interval_value(i);
        }
    }

    if (!params->rate_den) {
        i = hw_param_hw_interval(params, UAC_PCM_HW_PARAM_RATE);
        if (uac_pcm_hw_interval_single(i)) {
            params->rate_num = uac_pcm_hw_interval_value(i);
            params->rate_den = 1;
        }
    }
//
//    //hw = &substream->runtime->hw;
//    if (!params->fifo_size) {
//        i = hw_param_hw_interval(params, UAC_PCM_HW_PARAM_CHANNELS);
//        if (uac_pcm_hw_interval_min(i) == uac_pcm_hw_interval_max(i)) {
//            changed = substream->ops->fifo_size(substream, params);
//            if (changed < 0){
//                return changed;
//            }
//        }
//    }
    params->rmask = 0;
    return USB_OK;
}

/**
 * 把区间信息的最大值设置为最小值
 *
 * @return 有变化返回1，没变化返回0
 */
static int _uac_pcm_hw_param_first(struct uac_pcm_hw_params *params,
                                   uac_pcm_hw_param_t var)
{
    int changed;

    /** 把区间信息的最大值设置为最小值*/
    if((var >= UAC_PCM_HW_PARAM_FIRST_INFO) &&
            (var <= UAC_PCM_HW_PARAM_LAST_INFO)){
        changed = uac_interval_refine_first(hw_param_hw_interval(params, var));
    }
    else{
        return -USB_EINVAL;
    }
    /* 设置变化位*/
    if (changed) {
        params->cmask |= 1 << var;
        params->rmask |= 1 << var;
    }
    return changed;
}

/**
 * 把区间信息的最小值设置为最大值
 *
 * @return 有变化返回1，没变化返回0
 */
static int _uac_pcm_hw_param_last(struct uac_pcm_hw_params *params,
                                  uac_pcm_hw_param_t var)
{
    int changed;
    /** 把区间信息的最小值设置为最大值*/
    if((var >= UAC_PCM_HW_PARAM_FIRST_INFO) &&
            (var <= UAC_PCM_HW_PARAM_LAST_INFO)){
        changed = uac_interval_refine_last(hw_param_hw_interval(params, var));
    }else{
        return -USB_EINVAL;
    }
    /* 设置变化位*/
    if (changed) {
        params->cmask |= 1 << var;
        params->rmask |= 1 << var;
    }
    return changed;
}

/**
 * 获取具体的硬件信息的最小值
 *
 * @param params PCM硬件参数
 * @param var    硬件参数标志
 * @param dir    闭区间还是开区间
 *
 * @return 成功返回硬件信息最小值
 */
int uac_pcm_hw_param_value(const struct uac_pcm_hw_params *params,
                           uac_pcm_hw_param_t var, int *dir)
{
    if((var >= UAC_PCM_HW_PARAM_FIRST_INFO) &&
            (var <= UAC_PCM_HW_PARAM_LAST_INFO)){
        const struct uac_pcm_hw_interval *i = hw_param_hw_interval_c(params, var);
        /* 只有唯一值*/
        if (!uac_pcm_hw_interval_single(i)){
            return -USB_EINVAL;
        }
        /* 获取最小值开区间还是闭区间*/
        if (dir){
            *dir = i->openmin;
        }
        /* 返回区间信息最小值*/
        return uac_pcm_hw_interval_value(i);
    }
    return -USB_EINVAL;
}

/**
 * 优化配置并且返回最小值
 *
 * @return 成功返回最小值，失败返回负值
 */
int uac_pcm_hw_param_first(struct uac_pcm_substream *pcm,
                           struct uac_pcm_hw_params *params,
                           uac_pcm_hw_param_t var, int *dir)
{
    /* 把区间信息的最大值设置为最小值*/
    int changed = _uac_pcm_hw_param_first(params, var);
    if (changed < 0){
        return changed;
    }
    /* 有变化，优化硬件参数*/
    if (params->rmask) {
        usb_err_t ret = uac_pcm_hw_refine(pcm, params);
        if (ret != USB_OK){
            return ret;
        }
    }
    /* 返回区间信息最小值*/
    return uac_pcm_hw_param_value(params, var, dir);
}

/**
 * 优化配置并且返回最大值
 *
 * @return 成功返回最大值，失败返回负值
 */
int uac_pcm_hw_param_last(struct uac_pcm_substream *pcm,
                          struct uac_pcm_hw_params *params,
                          uac_pcm_hw_param_t var, int *dir)
{
    /* 把区间信息的最小值设置为最大值*/
    int changed = _uac_pcm_hw_param_last(params, var);
    if (changed < 0){
        return changed;
    }
    /* 有变化，优化硬件参数*/
    if (params->rmask) {
        usb_err_t ret = uac_pcm_hw_refine(pcm, params);
        if (ret != USB_OK){
            return ret;
        }
    }
    /* 返回区间信息最小值*/
    return uac_pcm_hw_param_value(params, var, dir);
}

/** 选择一个PCM硬件参数结构体定义的配置*/
usb_err_t uac_pcm_hw_params_choose(struct uac_pcm_substream *pcm,
                                   struct uac_pcm_hw_params *params)
{
    static int vars[] = {
        UAC_PCM_HW_PARAM_CHANNELS,
        UAC_PCM_HW_PARAM_RATE,
        UAC_PCM_HW_PARAM_PERIOD_TIME,
        UAC_PCM_HW_PARAM_BUFFER_SIZE,
        SNDRV_PCM_HW_PARAM_TICK_TIME,
        -1
    };
    usb_err_t ret;
    int *v;

    for (v = vars; *v != -1; v++) {
        if (*v != UAC_PCM_HW_PARAM_BUFFER_SIZE){
            /* 优化硬件区间信息的最小值，并返回最小值*/
            ret = uac_pcm_hw_param_first(pcm, params, *v, NULL);
        }else{
            /* 优化硬件区间信息的最大值，并返回最大值*/
            ret = uac_pcm_hw_param_last(pcm, params, *v, NULL);
        }
        if (ret < 0){
            return ret;
        }
    }
    return USB_OK;
}

/**
 * 添加一个USB音频PCM硬件啊限制规则
 *
 * @param runtime PCM运行时间结构体
 * @param cond    条件位
 * @param var     具体的硬件参数
 * @param func    规则函数
 * @param private 传递给规则函数的参数
 * @param dep     独立变量
 *
 * @return 成功返回USB_OK
 */
usb_err_t uac_pcm_hw_rule_add(struct uac_pcm_runtime *runtime, uint32_t cond,
                        int var,
                        uac_pcm_hw_rule_func_t func, void *private,
                        int dep1, int dep2, int dep3)
{
    struct uac_pcm_hw_constraints *constrs = &runtime->hw_constraints;
    struct uac_pcm_hw_rule *c;

    /* 添加新的规则*/
    if (constrs->rules_num >= constrs->rules_all) {
        struct uac_pcm_hw_rule *new;
        /* 一个音频PCM规则是16个*/
        uint32_t new_rules = constrs->rules_all + 16;
        new = usb_mem_alloc(new_rules * sizeof(struct uac_pcm_hw_rule));
        if (new == NULL) {
            return -USB_ENOMEM;
        }
        memset(new, 0, new_rules * sizeof(struct uac_pcm_hw_rule));
        /* 存在别的规则，新规则添加到就规则后面*/
        if (constrs->rules) {
            memcpy(new, constrs->rules, constrs->rules_num * sizeof(struct uac_pcm_hw_rule));
            usb_mem_free(constrs->rules);
        }
        constrs->rules = new;
        constrs->rules_all = new_rules;
    }
    /* 设置规则*/
    c = &constrs->rules[constrs->rules_num];
    c->cond = cond;
    c->func = func;
    c->var = var;
    c->private = private;
    c->deps[0] = dep1;
    c->deps[1] = dep2;
    c->deps[2] = dep3;

    constrs->rules_num++;
    return USB_OK;
}

/**
 * 初始化USB音频PCM硬件限制
 *
 * @param substream PCM子数据流结构体
 *
 * @return 成功返回USB_OK
 */
usb_err_t uac_pcm_hw_constraints_init(struct uac_pcm_substream *substream){
    struct uac_pcm_runtime *runtime = substream->runtime;
    struct uac_pcm_hw_constraints *constrs = &runtime->hw_constraints;
    int k;
    usb_err_t ret;

    /* 初始化PCM硬件信息*/
    for (k = UAC_PCM_HW_PARAM_FIRST_INFO; k <= UAC_PCM_HW_PARAM_LAST_INFO; k++) {
        uac_pcm_hw_interval_any(constrs_hw_interval(constrs, k));
    }
    /* 设置固定整数标志*/
    uac_pcm_hw_interval_setinteger(constrs_hw_interval(constrs, UAC_PCM_HW_PARAM_CHANNELS));
    uac_pcm_hw_interval_setinteger(constrs_hw_interval(constrs, UAC_PCM_HW_PARAM_BUFFER_SIZE));
    uac_pcm_hw_interval_setinteger(constrs_hw_interval(constrs, UAC_PCM_HW_PARAM_BUFFER_BYTES));
    uac_pcm_hw_interval_setinteger(constrs_hw_interval(constrs, UAC_PCM_HW_PARAM_SAMPLE_BITS));
    uac_pcm_hw_interval_setinteger(constrs_hw_interval(constrs, UAC_PCM_HW_PARAM_FRAME_BITS));
    /* 添加格式规则*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_FORMAT,
                              uac_pcm_hw_rule_format, NULL,
                              UAC_PCM_HW_PARAM_SAMPLE_BITS, -1, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 添加采样位规则*/
    ret = uac_pcm_hw_rule_add(runtime, 0, SNDRV_PCM_HW_PARAM_SAMPLE_BITS,
                              uac_pcm_hw_rule_sample_bits, NULL,
                              UAC_PCM_HW_PARAM_FORMAT,
                              UAC_PCM_HW_PARAM_SAMPLE_BITS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 1 采样位数 = 帧位数     / 通道数*/
    ret = uac_pcm_hw_rule_add(runtime, 0, SNDRV_PCM_HW_PARAM_SAMPLE_BITS,
                              uac_pcm_hw_rule_div, NULL,
                              UAC_PCM_HW_PARAM_FRAME_BITS, UAC_PCM_HW_PARAM_CHANNELS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 2 帧位数   = 采样位数   * 通道数*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_FRAME_BITS,
                              uac_pcm_hw_rule_mul, NULL,
                              UAC_PCM_HW_PARAM_SAMPLE_BITS, UAC_PCM_HW_PARAM_CHANNELS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 3 帧位数   = 周期字节数 * 8 / 周期帧数*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_FRAME_BITS,
                              uac_pcm_hw_rule_mulkdiv, (void*) 8,
                              UAC_PCM_HW_PARAM_PERIOD_BYTES, UAC_PCM_HW_PARAM_PERIOD_SIZE, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 4 帧位数   = 缓存字节数 * 8 / 缓存帧数*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_FRAME_BITS,
                              uac_pcm_hw_rule_mulkdiv, (void*) 8,
                              UAC_PCM_HW_PARAM_BUFFER_BYTES, UAC_PCM_HW_PARAM_BUFFER_SIZE, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 5 通道数   = 帧位数     / 采样位数*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_CHANNELS,
                              uac_pcm_hw_rule_div, NULL,
                              UAC_PCM_HW_PARAM_FRAME_BITS, UAC_PCM_HW_PARAM_SAMPLE_BITS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 6 采样率   = 周期帧数   * 1000000 / 周期时间*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_RATE,
                              uac_pcm_hw_rule_mulkdiv, (void*) 1000000,
                              UAC_PCM_HW_PARAM_PERIOD_SIZE, UAC_PCM_HW_PARAM_PERIOD_TIME, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 7 采样率   = 缓存帧数   * 1000000 / 缓存时间 */
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_RATE,
                              uac_pcm_hw_rule_mulkdiv, (void*) 1000000,
                              UAC_PCM_HW_PARAM_BUFFER_SIZE, UAC_PCM_HW_PARAM_BUFFER_TIME, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 8 周期数   = 缓存帧数   / 周期帧数 */
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_PERIODS,
                              uac_pcm_hw_rule_div, NULL,
                              UAC_PCM_HW_PARAM_BUFFER_SIZE, UAC_PCM_HW_PARAM_PERIOD_SIZE, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 9 周期帧数   = 缓存帧数 / 缓存周期数*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_PERIOD_SIZE,
                              uac_pcm_hw_rule_div, NULL,
                              UAC_PCM_HW_PARAM_BUFFER_SIZE, UAC_PCM_HW_PARAM_PERIODS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 10 周期帧数   = 周期字节数 * 8 / 帧位数*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_PERIOD_SIZE,
                              uac_pcm_hw_rule_mulkdiv, (void*) 8,
                              UAC_PCM_HW_PARAM_PERIOD_BYTES, UAC_PCM_HW_PARAM_FRAME_BITS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 11 周期帧数   = 周期时间  * 采样率  / 1000000*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_PERIOD_SIZE,
                              uac_pcm_hw_rule_muldivk, (void*) 1000000,
                              UAC_PCM_HW_PARAM_PERIOD_TIME, UAC_PCM_HW_PARAM_RATE, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 12 缓存帧数 = 周期帧数 * 周期数量*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_BUFFER_SIZE,
                              uac_pcm_hw_rule_mul, NULL,
                              UAC_PCM_HW_PARAM_PERIOD_SIZE, UAC_PCM_HW_PARAM_PERIODS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 13 缓存帧数 = 缓存字节数 * 8 / 帧位数*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_BUFFER_SIZE,
                              uac_pcm_hw_rule_mulkdiv, (void*) 8,
                              UAC_PCM_HW_PARAM_BUFFER_BYTES, UAC_PCM_HW_PARAM_FRAME_BITS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 14 缓存帧数 = 缓存时间 * 1000000 / 采样率*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_BUFFER_SIZE,
                              uac_pcm_hw_rule_muldivk, (void*) 1000000,
                              UAC_PCM_HW_PARAM_BUFFER_TIME, UAC_PCM_HW_PARAM_RATE, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 15 周期字节数 = 周期帧数 * 8 / 帧位数*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_PERIOD_BYTES,
                              uac_pcm_hw_rule_muldivk, (void*) 8,
                              UAC_PCM_HW_PARAM_PERIOD_SIZE, UAC_PCM_HW_PARAM_FRAME_BITS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 16 缓存字节数 = 缓存帧数 * 8 / 帧位数*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_BUFFER_BYTES,
                              uac_pcm_hw_rule_muldivk, (void*) 8,
                              UAC_PCM_HW_PARAM_BUFFER_SIZE, UAC_PCM_HW_PARAM_FRAME_BITS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 17 周期时间 = 周期帧数 * 1000000 / 采样率*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_PERIOD_TIME,
                              uac_pcm_hw_rule_mulkdiv, (void*) 1000000,
                              UAC_PCM_HW_PARAM_PERIOD_SIZE, UAC_PCM_HW_PARAM_RATE, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 18 缓存时间 = 缓存帧数 * 1000000 / 采样率*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_BUFFER_TIME,
                              uac_pcm_hw_rule_mulkdiv, (void*) 1000000,
                              UAC_PCM_HW_PARAM_BUFFER_SIZE, UAC_PCM_HW_PARAM_RATE, -1);
    if (ret != USB_OK){
        return ret;
    }
    return USB_OK;
}

/** 设置PCM硬件限制条件*/
usb_err_t uac_pcm_hw_constraints_complete(struct uac_pcm_substream *substream)
{
    struct uac_pcm_runtime *runtime = substream->runtime;
    struct uac_pcm_hardware *hw = &runtime->hw;
    usb_err_t ret;

    /* 设置通道最小最大值*/
    ret = uac_pcm_hw_constraint_minmax(runtime, UAC_PCM_HW_PARAM_CHANNELS,
                                       hw->channels_min, hw->channels_max);
    if (ret < 0){
        return ret;
    }
    /* 设置采样率最小最大值*/
    ret = uac_pcm_hw_constraint_minmax(runtime, UAC_PCM_HW_PARAM_RATE,
                                       hw->rate_min, hw->rate_max);
    if (ret < 0){
        return ret;
    }
    /* 设置周期字节最小最大值*/
    ret = uac_pcm_hw_constraint_minmax(runtime, UAC_PCM_HW_PARAM_PERIOD_BYTES,
                                       hw->period_bytes_min, hw->period_bytes_max);
    if (ret < 0){
        return ret;
    }
    /* 设置周期数量最小最大值*/
    ret = uac_pcm_hw_constraint_minmax(runtime, UAC_PCM_HW_PARAM_PERIODS,
                                       hw->periods_min, hw->periods_max);
    if (ret < 0){
        return ret;
    }
    /* 设置缓存大小最小最大值*/
    ret = uac_pcm_hw_constraint_minmax(runtime, UAC_PCM_HW_PARAM_BUFFER_BYTES,
                                       hw->period_bytes_min, hw->buffer_bytes_max);
    if (ret < 0){
        return ret;
    }
    /* 添加缓存字节最大值规则*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_BUFFER_BYTES,
                              uac_pcm_hw_rule_buffer_bytes_max, substream,
                              UAC_PCM_HW_PARAM_BUFFER_BYTES, -1, -1);
    if (ret < 0){
        return ret;
    }
    /* 添加采样率规则*/
    if (!(hw->rates & (SNDRV_PCM_RATE_KNOT | SNDRV_PCM_RATE_CONTINUOUS))) {
        ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_RATE,
                                  uac_pcm_hw_rule_rate, hw,
                                  UAC_PCM_HW_PARAM_RATE, -1, -1);
        if (ret < 0){
            return ret;
        }
    }

    /* 设置周期帧数大小为固定值*/
    uac_pcm_hw_constraint_integer(runtime, UAC_PCM_HW_PARAM_PERIOD_SIZE);

    return 0;
}

/** 设置usb音频硬件参数*/
struct uac_pcm_hw_params *usbh_uac_get_hw_params(void *params_usr){
    struct uac_pcm_hw_params *hw_params = NULL;
    struct usbh_uac_params_usr *params = (struct usbh_uac_params_usr *)params_usr;
    struct uac_pcm_hw_interval *hw_interval = NULL;
    uint32_t channels = 0;
    uint32_t rate = 0;
    int pcm_format_size = 0;
    uint32_t buffer_bytes_size = 0;

    if(params_usr == NULL){
        return NULL;
    }
    channels = params->channels;
    rate     = params->rate;
    pcm_format_size = params->pcm_format_size;

    /* 申请内存*/
    hw_params = usb_mem_alloc(sizeof(struct uac_pcm_hw_params));
    if(hw_params == NULL){
        return NULL;
    }
    memset(hw_params, 0, sizeof(struct uac_pcm_hw_params));
    /* 请求验证所有参数*/
    hw_params->rmask = ~0U;
    /* 设置格式位宽和格式*/
    hw_params->pcm_format_size_bit = uac_get_pcm_format_size_bit(pcm_format_size);
    if(hw_params->pcm_format_size_bit >= 0){
        hw_params->format = (1 << hw_params->pcm_format_size_bit);
    }
    else{
        usb_mem_free(hw_params);
        return NULL;
    }
    /* 设置样本位数*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_SAMPLE_BITS);
    hw_interval->integer = 1;
    hw_interval->min = params->bits_per_sample;
    hw_interval->max = params->bits_per_sample;
    /* 设置通道数*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_CHANNELS);
    hw_interval->integer = 1;
    hw_interval->min = channels;
    hw_interval->max = channels;
    /* 设置帧位数*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_FRAME_BITS);
    hw_interval->integer = 1;
    hw_interval->min = params->pcm_format_size * channels;
    hw_interval->max = params->pcm_format_size * channels;
    /* 设置采样率*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_RATE);
    hw_interval->integer = 1;
    hw_interval->min = rate;
    hw_interval->max = rate;
    /* 设置一个周期的帧数(一个周期1024帧)*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_PERIOD_SIZE);
    hw_interval->integer = 1;
    hw_interval->min = 1024;
    hw_interval->max = 1024;
    /* 设置周期的字节数*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_PERIOD_BYTES);
    hw_interval->integer = 1;
    hw_interval->min = 1024 * params->pcm_format_size * channels / 8;
    hw_interval->max = 1024 * params->pcm_format_size * channels / 8;
    /* 设置缓存的周期数量(一个缓存16个周期)*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_PERIODS);
    hw_interval->integer = 1;
    hw_interval->min = 16;
    hw_interval->max = 16;
    /* 设置缓存帧数(一个缓存16K帧)*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_BUFFER_SIZE);
    hw_interval->integer = 1;
    hw_interval->min = 16 * 1024;
    hw_interval->max = 16 * 1024;
    /* 设置缓存字节大小*/
    buffer_bytes_size = 16 * 1024 * params->pcm_format_size * channels / 8;
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_BUFFER_BYTES);
    hw_interval->integer = 1;
    hw_interval->min = buffer_bytes_size;
    hw_interval->max = buffer_bytes_size;
    /* 设置周期时间(这里暂时设置为最小值和最大值)*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_PERIOD_TIME);
    hw_interval->integer = 0;
    hw_interval->min = 0;
    hw_interval->max = USB_UINT_MAX;
    /* 设置缓存时间*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_BUFFER_TIME);
    hw_interval->integer = 0;
    //if(((16 * 1024 * 1000000) % rate) != 0){
    if(((16 * 1024 * 100000) % rate) != 0){
        hw_interval->min = 16 * 1024 * 100000 / rate * 10 + 3;
        hw_interval->max = 16 * 1024 * 100000 / rate * 10 + 4;
    }
    else{
        hw_interval->min = 16 * 1024 * 100000 / rate * 10;
        hw_interval->max = 16 * 1024 * 100000 / rate * 10 + 1;
    }

    return hw_params;

}



