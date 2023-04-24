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
 * �Ż�PCMӲ��������Ϣ
 *
 * @param i Ҫ�������õ�������Ϣ
 * @param v ���õ�������Ϣ
 *
 * @return ֵ�ı��˷��ط�0��û�ı䷵��0
 */
int uac_pcm_hw_interval_refine(struct uac_pcm_hw_interval *i, const struct uac_pcm_hw_interval *v)
{
    int changed = 0;

    /* ���Ӳ��������Ϣ�Ƿ�Ϊ��*/
    if (uac_pcm_hw_interval_empty(i)){
        return -USB_EINVAL;
    }
    /* �����������ֵ*/
    if (i->min < v->min) {
        i->min = v->min;
        i->openmin = v->openmin;
        changed = 1;
    } else if ((i->min == v->min) && (!i->openmin && v->openmin)) {
        /* ���ÿ�������Сֵ*/
        i->openmin = 1;
        changed = 1;
    }
    /* �����������ֵ*/
    if (i->max > v->max) {
        i->max = v->max;
        i->openmax = v->openmax;
        changed = 1;
    } else if ((i->max == v->max) && (!i->openmax && v->openmax)) {
        /* ���ÿ��������ֵ*/
        i->openmax = 1;
        changed = 1;
    }
    /* �Ƿ��ǹ̶�����ֵ*/
    if ((!i->integer) && (v->integer)) {
        i->integer = 1;
        changed = 1;
    }
    if (i->integer) {
        /* ����ǹ̶�����ֵ���Ƿ�Χ�ǿ�����*/
        if (i->openmin) {
            i->min++;
            i->openmin = 0;
        }
        if (i->openmax) {
            i->max--;
            i->openmax = 0;
        }
    } else if ((!i->openmin) && (!i->openmax) && (i->min == i->max)){
        /* �����䷶Χ�������ֵ������Сֵ�����ǹ̶�����ֵ*/
        i->integer = 1;
    }
    /* �����������Ϣ�Ƿ���Ч*/
    if (uac_pcm_hw_interval_checkempty(i)) {
        /* ��Ч������Ϊ��Ч*/
        uac_pcm_hw_interval_none(i);
        return -USB_EINVAL;
    }
    return changed;
}

/** ��������Ӳ��������Ϣ�ĳ˻�*/
void uac_pcm_hw_mul(const struct uac_pcm_hw_interval *a, const struct uac_pcm_hw_interval *b,
                    struct uac_pcm_hw_interval *c)
{
    /* ������һ������Ч��Ϣ��������Ϊ��Ч*/
    if ((a->empty) || (b->empty)) {
        uac_pcm_hw_interval_none(c);
        return;
    }
    c->empty = 0;
    /* ����������Сֵ�ĳ˻�*/
    c->min = mul(a->min, b->min);
    c->openmin = (a->openmin || b->openmin);
    /* �����������ֵ�ĳ˻�*/
    c->max = mul(a->max,  b->max);
    c->openmax = (a->openmax || b->openmax);
    c->integer = (a->integer && b->integer);
}

/**
 * �ó����Ż�����ֵ
 *
 * @param a ������
 * @param b ����
 * @param c ��
 *
 * c = a / b
 *
 * @return ֵ�ı��˷��ط�0��û�ı䷵��0
 */
void uac_pcm_hw_div(const struct uac_pcm_hw_interval *a, const struct uac_pcm_hw_interval *b,
                    struct uac_pcm_hw_interval *c){
    uint32_t r;
    /* ������һ������Ч��Ϣ��������Ϊ��Ч*/
    if ((a->empty) || (b->empty)) {
        uac_pcm_hw_interval_none(c);
        return;
    }
    c->empty = 0;
    /* ��Сֵ����������ֵ�ĳ���*/
    c->min = div32(a->min, b->max, &r);
    c->openmin = (r || a->openmin || b->openmax);
    if (b->min > 0) {
        /* ���ֵ����������ֵ�ĳ���*/
        c->max = div32(a->max, b->min, &r);
        /* �����������ֵ��1����Ϊ������*/
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
 * �Ż����ֵ
 *
 * @param a ������1
 * @param k ������2(��Ϊ����)
 * @param b ����
 * @param c ���
 *
 * c = a * k / b
 *
 * @return ֵ�ı��˷��ط�0��û�ı䷵��0
 */
void uac_pcm_hw_mulkdiv(const struct uac_pcm_hw_interval *a, uint32_t k,
                        const struct uac_pcm_hw_interval *b, struct uac_pcm_hw_interval *c){
    uint32_t r;
    /* ������һ������Ч��Ϣ��������Ϊ��Ч*/
    if ((a->empty) || (b->empty)) {
        uac_pcm_hw_interval_none(c);
        return;
    }
    c->empty = 0;
    c->min = muldiv32(a->min, k, b->max, &r);
    c->openmin = (r || a->openmin || b->openmax);
    if (b->min > 0) {
        c->max = muldiv32(a->max, k, b->min, &r);
        /* �����������ֵ��1����Ϊ������*/
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
 * �Ż����ֵ
 *
 * @param a ������1
 * @param b ������2
 * @param k ����(��Ϊ����)
 * @param c ���
 *
 * c = a * b / k
 *
 * @return ֵ�ı��˷��ط�0��û�ı䷵��0
 */
void uac_pcm_hw_muldivk(const struct uac_pcm_hw_interval *a, const struct uac_pcm_hw_interval *b,
                        uint32_t k, struct uac_pcm_hw_interval *c)
{
    uint32_t r;
    /* ������һ������Ч��Ϣ��������Ϊ��Ч*/
    if ((a->empty) || (b->empty)) {
        uac_pcm_hw_interval_none(c);
        return;
    }
    c->empty = 0;
    c->min = muldiv32(a->min, b->min, k, &r);
    c->openmin = (r || a->openmin || b->openmin);
    c->max = muldiv32(a->max, b->max, k, &r);
    /* �����������ֵ��1����Ϊ������*/
    if (r) {
        c->max++;
        c->openmax = 1;
    } else
        c->openmax = (a->openmax || b->openmax);
    c->integer = 0;
}

/* �������������ĳ˻�*/
static int uac_pcm_hw_rule_mul(struct uac_pcm_hw_params *params,
                               struct uac_pcm_hw_rule *rule)
{
    struct uac_pcm_hw_interval t;
    uac_pcm_hw_mul(hw_param_hw_interval_c(params, rule->deps[0]),
                   hw_param_hw_interval_c(params, rule->deps[1]), &t);
    return uac_pcm_hw_interval_refine(hw_param_hw_interval(params, rule->var), &t);
}

/* ���������������*/
static int uac_pcm_hw_rule_div(struct uac_pcm_hw_params *params,
                               struct uac_pcm_hw_rule *rule)
{
    struct uac_pcm_hw_interval t;
    uac_pcm_hw_div(hw_param_hw_interval_c(params, rule->deps[0]),
                   hw_param_hw_interval_c(params, rule->deps[1]), &t);
    return uac_pcm_hw_interval_refine(hw_param_hw_interval(params, rule->var), &t);
}

/** �������0��rule->private���Բ���1*/
static int uac_pcm_hw_rule_mulkdiv(struct uac_pcm_hw_params *params,
                                   struct uac_pcm_hw_rule *rule)
{
    struct uac_pcm_hw_interval t;
    uac_pcm_hw_mulkdiv(hw_param_hw_interval_c(params, rule->deps[0]),
                       (unsigned long) rule->private,
                       hw_param_hw_interval_c(params, rule->deps[1]), &t);
    return uac_pcm_hw_interval_refine(hw_param_hw_interval(params, rule->var), &t);
}

/** �������0�˲���1����rule->private*/
static int uac_pcm_hw_rule_muldivk(struct uac_pcm_hw_params *params,
                                   struct uac_pcm_hw_rule *rule)
{
    struct uac_pcm_hw_interval t;
    uac_pcm_hw_muldivk(hw_param_hw_interval_c(params, rule->deps[0]),
                       hw_param_hw_interval_c(params, rule->deps[1]),
                       (unsigned long) rule->private, &t);
    return uac_pcm_hw_interval_refine(hw_param_hw_interval(params, rule->var), &t);
}

/** ��һ��������Ϣ����һ����������*/
int uac_pcm_hw_constraint_integer(struct uac_pcm_runtime *runtime, uac_pcm_hw_param_t var)
{
    struct uac_pcm_hw_constraints *constrs = &runtime->hw_constraints;
    return uac_pcm_hw_interval_setinteger(constrs_hw_interval(constrs, var));
}

/**
 * ���б����Ż�����ֵ
 *
 * @param i     Ҫ�Ż�������ֵ
 * @param count �б�Ԫ�ص�����
 * @param list  �б�
 * @param mask
 *
 * @return ����1��ʾֵ�ı䣬����0��ʾ�޸ı�
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

/** ��������Ϣ�����ֵ����Ϊ��Сֵ*/
static int uac_interval_refine_first(struct uac_pcm_hw_interval *i)
{
    /* ��Ч��������Ϣ*/
    if (uac_pcm_hw_interval_empty(i)){
        return -USB_EINVAL;
    }
    /* ������������Ϣ�Ƿ�ֻ��Ψһֵ*/
    if (uac_pcm_hw_interval_single(i)){
        return 0;
    }
    i->max = i->min;
    i->openmax = i->openmin;
    /* ������*/
    if (i->openmax){
        i->max++;
    }
    return 1;
}

/** ��������Ϣ����Сֵ����Ϊ���ֵ*/
static int uac_interval_refine_last(struct uac_pcm_hw_interval *i)
{
    /* ��Ч��������Ϣ*/
    if (uac_pcm_hw_interval_empty(i)){
        return -USB_EINVAL;
    }
    /* ������������Ϣ�Ƿ�ֻ��Ψһֵ*/
    if (uac_pcm_hw_interval_single(i)){
        return 0;
    }
    i->min = i->max;
    i->openmin = i->openmax;
    /* ������*/
    if (i->openmin){
        i->min--;
    }
    return 1;
}

/**
 * ����PCMӲ��������С���ֵ
 *
 * @param runtime USB��Ƶ����ʱ��ṹ��
 * @param var     ����Ӳ������
 * @param min     ��Сֵ
 * @param max     ���ֵ
 *
 * @return ����1��ʾֵ�ı䣬����0��ʾֵû�ı�
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
 * USB��ƵPCMӲ����ʽ����
 *
 * ����ʽ��λ���
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
        /* ���Բ����õĸ�ʽ*/
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
 * USB��ƵPCMӲ������λ�����
 *
 * @param params  PCMӲ������
 * @param rule    PCMӲ������ṹ��
 *
 * @return ����1��ʾֵ�ı䣬����0��ʾֵû�ı�
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
        /* ��ȡ��ʽλ��*/
        bits = uac_pcm_format_physical_width(k);
        /* ���Բ����õĸ�ʽ*/
        if (bits <= 0){
            continue;
        }
        if (t.min > (uint32_t)bits)
            t.min = bits;
        if (t.max < (uint32_t)bits)
            t.max = bits;
    }
    /* ��ʽλ���ǹ̶�����*/
    t.integer = 1;
    return uac_pcm_hw_interval_refine(hw_param_hw_interval(params, rule->var), &t);
}

/** ����ʽ�Ƿ���Ч*/
static usb_bool_t hw_check_valid_format(struct usbh_uac_substream *subs,
                                        struct uac_pcm_hw_params *params,
                                        struct audioformat *fp){
    struct uac_pcm_hw_interval *it = hw_param_hw_interval(params, UAC_PCM_HW_PARAM_RATE);
    struct uac_pcm_hw_interval *ct = hw_param_hw_interval(params, UAC_PCM_HW_PARAM_CHANNELS);
    struct uac_pcm_hw_interval *pt = hw_param_hw_interval(params, UAC_PCM_HW_PARAM_PERIOD_TIME);
    uint32_t ptime;

    /* ����ʽ*/
    if(params->format != fp->formats){
        __UAC_RULE_SPACE("> check: no supported format %d\n", fp->formats);
        return USB_FALSE;
    }
    /* ���ͨ��*/
    if ((fp->channels < ct->min) || (fp->channels > ct->max)) {
        __UAC_RULE_SPACE("> check: no valid channels %d (%d/%d)\r\n", fp->channels, ct->min, ct->max);
        return USB_FALSE;
    }
    /* ���������Ƿ��ڷ�Χ��*/
    if ((fp->rate_min > it->max) || (fp->rate_min == it->max && it->openmax)) {
        __UAC_RULE_SPACE("> check: rate_min %d > max %d\n", fp->rate_min, it->max);
        return USB_FALSE;
    }
    if ((fp->rate_max < it->min) || (fp->rate_max == it->min && it->openmin)) {
        __UAC_RULE_SPACE("> check: rate_max %d < min %d\n", fp->rate_max, it->min);
        return USB_FALSE;
    }
    /* ����Ƿ�����ʱ����ڵ������ݰ����ʱ��*/
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
 * �����ʹ�����
 *
 * @param params PCMӲ������
 * @param rule   ����ṹ��
 *
 * @return ����1��ʾֵ�ı䣬����0��ʾֵû�ı�
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
    /* ��ȡ���и�ʽ����Сֵ�����ֵ*/
    usb_list_for_each_entry(fp, &subs->fmt_list, struct audioformat, list) {
        /* ����Ƿ���Ч�ĸ�ʽ*/
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
    /* ���¹淶����������*/
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
    /* ���Ӳ��������Ϣ�Ƿ���Ч*/
    if (uac_pcm_hw_interval_checkempty(it)) {
        it->empty = 1;
        return -USB_EINVAL;
    }
    __UAC_RULE_SPACE("--> (%d, %d) (changed = %d)\r\n", it->min, it->max, changed);
    return changed;
}

/**
 * ͨ����������
 *
 * @param params PCMӲ������
 * @param rule   ����ṹ��
 *
 * @return ����1��ʾֵ�ı䣬����0��ʾֵû�ı�
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
    /* ��ȡ���и�ʽ����Сֵ�����ֵ*/
    usb_list_for_each_entry(fp, &subs->fmt_list, struct audioformat, list) {
        /* ����Ƿ���Ч�ĸ�ʽ*/
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
    /* ���¹淶ͨ������*/
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
    /* ���Ӳ��������Ϣ�Ƿ���Ч*/
    if (uac_pcm_hw_interval_checkempty(it)) {
        it->empty = 1;
        return -USB_EINVAL;
    }
    __UAC_RULE_SPACE("--> (%d, %d) (changed = %d)\r\n", it->min, it->max, changed);
    return changed;
}

/**
 * ��Ƶ��ʽ������
 *
 * @param params PCMӲ������
 * @param rule   ����ṹ��
 *
 * @return ����1��ʾֵ�ı䣬����0��ʾֵû�ı�
 */
int hw_rule_format(struct uac_pcm_hw_params *params,
                   struct uac_pcm_hw_rule *rule)
{
    //todo
    return 0;
}

/**
 * ����ʱ�������
 *
 * @param params PCMӲ������
 * @param rule   ����ṹ��
 *
 * @return ����1��ʾֵ�ı䣬����0��ʾֵû�ı�
 */
int hw_rule_period_time(struct uac_pcm_hw_params *params,
                        struct uac_pcm_hw_rule *rule)
{
    //todo
    return 0;
}

/* ������*/
static uint32_t rates[] = { 5512, 8000, 11025, 16000, 22050, 32000, 44100,
                            48000, 64000, 88200, 96000, 176400, 192000 };

const struct uac_pcm_hw_constraint_list uac_pcm_known_rates1 = {
    .count = USB_NELEMENTS(rates),
    .list = rates,
};

/** �����ʹ�����*/
static int uac_pcm_hw_rule_rate(struct uac_pcm_hw_params *params,
                                struct uac_pcm_hw_rule *rule)
{
    struct uac_pcm_hardware *hw = rule->private;
    return uac_interval_list(hw_param_hw_interval(params, rule->var),
                             uac_pcm_known_rates1.count,
                             uac_pcm_known_rates1.list, hw->rates);
}

/** ��󻺴��ֽڴ�С����*/
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
 * �Ż�PCMӲ������
 *
 * @param substream PCM���������ṹ��
 * @param params    Ӳ������
 *
 * @return �ɹ�����USB_OK
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

    /* ��������Ӳ����������*/
    for (k = UAC_PCM_HW_PARAM_FIRST_INFO; k <= UAC_PCM_HW_PARAM_LAST_INFO; k++) {
        /* ��ȡ�����������Ϣ*/
        i = hw_param_hw_interval(params, k);
        /* ���������Ϣ�Ƿ���Ч*/
        if (uac_pcm_hw_interval_empty(i)){
            return -USB_EINVAL;
        }
        /* �����û�б仯����*/
        if (!(params->rmask & (1 << k)))
            continue;
        /* �Ż����ò���*/
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

            /* ���*/
            for (d = 0; r->deps[d] >= 0; d++) {
                if (vstamps[r->deps[d]] > rstamps[k]) {
                    doit = 1;
                    break;
                }
            }
            if (!doit)
                continue;
            /* ִ�й�����*/
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
 * ��������Ϣ�����ֵ����Ϊ��Сֵ
 *
 * @return �б仯����1��û�仯����0
 */
static int _uac_pcm_hw_param_first(struct uac_pcm_hw_params *params,
                                   uac_pcm_hw_param_t var)
{
    int changed;

    /** ��������Ϣ�����ֵ����Ϊ��Сֵ*/
    if((var >= UAC_PCM_HW_PARAM_FIRST_INFO) &&
            (var <= UAC_PCM_HW_PARAM_LAST_INFO)){
        changed = uac_interval_refine_first(hw_param_hw_interval(params, var));
    }
    else{
        return -USB_EINVAL;
    }
    /* ���ñ仯λ*/
    if (changed) {
        params->cmask |= 1 << var;
        params->rmask |= 1 << var;
    }
    return changed;
}

/**
 * ��������Ϣ����Сֵ����Ϊ���ֵ
 *
 * @return �б仯����1��û�仯����0
 */
static int _uac_pcm_hw_param_last(struct uac_pcm_hw_params *params,
                                  uac_pcm_hw_param_t var)
{
    int changed;
    /** ��������Ϣ����Сֵ����Ϊ���ֵ*/
    if((var >= UAC_PCM_HW_PARAM_FIRST_INFO) &&
            (var <= UAC_PCM_HW_PARAM_LAST_INFO)){
        changed = uac_interval_refine_last(hw_param_hw_interval(params, var));
    }else{
        return -USB_EINVAL;
    }
    /* ���ñ仯λ*/
    if (changed) {
        params->cmask |= 1 << var;
        params->rmask |= 1 << var;
    }
    return changed;
}

/**
 * ��ȡ�����Ӳ����Ϣ����Сֵ
 *
 * @param params PCMӲ������
 * @param var    Ӳ��������־
 * @param dir    �����仹�ǿ�����
 *
 * @return �ɹ�����Ӳ����Ϣ��Сֵ
 */
int uac_pcm_hw_param_value(const struct uac_pcm_hw_params *params,
                           uac_pcm_hw_param_t var, int *dir)
{
    if((var >= UAC_PCM_HW_PARAM_FIRST_INFO) &&
            (var <= UAC_PCM_HW_PARAM_LAST_INFO)){
        const struct uac_pcm_hw_interval *i = hw_param_hw_interval_c(params, var);
        /* ֻ��Ψһֵ*/
        if (!uac_pcm_hw_interval_single(i)){
            return -USB_EINVAL;
        }
        /* ��ȡ��Сֵ�����仹�Ǳ�����*/
        if (dir){
            *dir = i->openmin;
        }
        /* ����������Ϣ��Сֵ*/
        return uac_pcm_hw_interval_value(i);
    }
    return -USB_EINVAL;
}

/**
 * �Ż����ò��ҷ�����Сֵ
 *
 * @return �ɹ�������Сֵ��ʧ�ܷ��ظ�ֵ
 */
int uac_pcm_hw_param_first(struct uac_pcm_substream *pcm,
                           struct uac_pcm_hw_params *params,
                           uac_pcm_hw_param_t var, int *dir)
{
    /* ��������Ϣ�����ֵ����Ϊ��Сֵ*/
    int changed = _uac_pcm_hw_param_first(params, var);
    if (changed < 0){
        return changed;
    }
    /* �б仯���Ż�Ӳ������*/
    if (params->rmask) {
        usb_err_t ret = uac_pcm_hw_refine(pcm, params);
        if (ret != USB_OK){
            return ret;
        }
    }
    /* ����������Ϣ��Сֵ*/
    return uac_pcm_hw_param_value(params, var, dir);
}

/**
 * �Ż����ò��ҷ������ֵ
 *
 * @return �ɹ��������ֵ��ʧ�ܷ��ظ�ֵ
 */
int uac_pcm_hw_param_last(struct uac_pcm_substream *pcm,
                          struct uac_pcm_hw_params *params,
                          uac_pcm_hw_param_t var, int *dir)
{
    /* ��������Ϣ����Сֵ����Ϊ���ֵ*/
    int changed = _uac_pcm_hw_param_last(params, var);
    if (changed < 0){
        return changed;
    }
    /* �б仯���Ż�Ӳ������*/
    if (params->rmask) {
        usb_err_t ret = uac_pcm_hw_refine(pcm, params);
        if (ret != USB_OK){
            return ret;
        }
    }
    /* ����������Ϣ��Сֵ*/
    return uac_pcm_hw_param_value(params, var, dir);
}

/** ѡ��һ��PCMӲ�������ṹ�嶨�������*/
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
            /* �Ż�Ӳ��������Ϣ����Сֵ����������Сֵ*/
            ret = uac_pcm_hw_param_first(pcm, params, *v, NULL);
        }else{
            /* �Ż�Ӳ��������Ϣ�����ֵ�����������ֵ*/
            ret = uac_pcm_hw_param_last(pcm, params, *v, NULL);
        }
        if (ret < 0){
            return ret;
        }
    }
    return USB_OK;
}

/**
 * ���һ��USB��ƵPCMӲ�������ƹ���
 *
 * @param runtime PCM����ʱ��ṹ��
 * @param cond    ����λ
 * @param var     �����Ӳ������
 * @param func    ������
 * @param private ���ݸ��������Ĳ���
 * @param dep     ��������
 *
 * @return �ɹ�����USB_OK
 */
usb_err_t uac_pcm_hw_rule_add(struct uac_pcm_runtime *runtime, uint32_t cond,
                        int var,
                        uac_pcm_hw_rule_func_t func, void *private,
                        int dep1, int dep2, int dep3)
{
    struct uac_pcm_hw_constraints *constrs = &runtime->hw_constraints;
    struct uac_pcm_hw_rule *c;

    /* ����µĹ���*/
    if (constrs->rules_num >= constrs->rules_all) {
        struct uac_pcm_hw_rule *new;
        /* һ����ƵPCM������16��*/
        uint32_t new_rules = constrs->rules_all + 16;
        new = usb_mem_alloc(new_rules * sizeof(struct uac_pcm_hw_rule));
        if (new == NULL) {
            return -USB_ENOMEM;
        }
        memset(new, 0, new_rules * sizeof(struct uac_pcm_hw_rule));
        /* ���ڱ�Ĺ����¹�����ӵ��͹������*/
        if (constrs->rules) {
            memcpy(new, constrs->rules, constrs->rules_num * sizeof(struct uac_pcm_hw_rule));
            usb_mem_free(constrs->rules);
        }
        constrs->rules = new;
        constrs->rules_all = new_rules;
    }
    /* ���ù���*/
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
 * ��ʼ��USB��ƵPCMӲ������
 *
 * @param substream PCM���������ṹ��
 *
 * @return �ɹ�����USB_OK
 */
usb_err_t uac_pcm_hw_constraints_init(struct uac_pcm_substream *substream){
    struct uac_pcm_runtime *runtime = substream->runtime;
    struct uac_pcm_hw_constraints *constrs = &runtime->hw_constraints;
    int k;
    usb_err_t ret;

    /* ��ʼ��PCMӲ����Ϣ*/
    for (k = UAC_PCM_HW_PARAM_FIRST_INFO; k <= UAC_PCM_HW_PARAM_LAST_INFO; k++) {
        uac_pcm_hw_interval_any(constrs_hw_interval(constrs, k));
    }
    /* ���ù̶�������־*/
    uac_pcm_hw_interval_setinteger(constrs_hw_interval(constrs, UAC_PCM_HW_PARAM_CHANNELS));
    uac_pcm_hw_interval_setinteger(constrs_hw_interval(constrs, UAC_PCM_HW_PARAM_BUFFER_SIZE));
    uac_pcm_hw_interval_setinteger(constrs_hw_interval(constrs, UAC_PCM_HW_PARAM_BUFFER_BYTES));
    uac_pcm_hw_interval_setinteger(constrs_hw_interval(constrs, UAC_PCM_HW_PARAM_SAMPLE_BITS));
    uac_pcm_hw_interval_setinteger(constrs_hw_interval(constrs, UAC_PCM_HW_PARAM_FRAME_BITS));
    /* ��Ӹ�ʽ����*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_FORMAT,
                              uac_pcm_hw_rule_format, NULL,
                              UAC_PCM_HW_PARAM_SAMPLE_BITS, -1, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* ��Ӳ���λ����*/
    ret = uac_pcm_hw_rule_add(runtime, 0, SNDRV_PCM_HW_PARAM_SAMPLE_BITS,
                              uac_pcm_hw_rule_sample_bits, NULL,
                              UAC_PCM_HW_PARAM_FORMAT,
                              UAC_PCM_HW_PARAM_SAMPLE_BITS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 1 ����λ�� = ֡λ��     / ͨ����*/
    ret = uac_pcm_hw_rule_add(runtime, 0, SNDRV_PCM_HW_PARAM_SAMPLE_BITS,
                              uac_pcm_hw_rule_div, NULL,
                              UAC_PCM_HW_PARAM_FRAME_BITS, UAC_PCM_HW_PARAM_CHANNELS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 2 ֡λ��   = ����λ��   * ͨ����*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_FRAME_BITS,
                              uac_pcm_hw_rule_mul, NULL,
                              UAC_PCM_HW_PARAM_SAMPLE_BITS, UAC_PCM_HW_PARAM_CHANNELS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 3 ֡λ��   = �����ֽ��� * 8 / ����֡��*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_FRAME_BITS,
                              uac_pcm_hw_rule_mulkdiv, (void*) 8,
                              UAC_PCM_HW_PARAM_PERIOD_BYTES, UAC_PCM_HW_PARAM_PERIOD_SIZE, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 4 ֡λ��   = �����ֽ��� * 8 / ����֡��*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_FRAME_BITS,
                              uac_pcm_hw_rule_mulkdiv, (void*) 8,
                              UAC_PCM_HW_PARAM_BUFFER_BYTES, UAC_PCM_HW_PARAM_BUFFER_SIZE, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 5 ͨ����   = ֡λ��     / ����λ��*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_CHANNELS,
                              uac_pcm_hw_rule_div, NULL,
                              UAC_PCM_HW_PARAM_FRAME_BITS, UAC_PCM_HW_PARAM_SAMPLE_BITS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 6 ������   = ����֡��   * 1000000 / ����ʱ��*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_RATE,
                              uac_pcm_hw_rule_mulkdiv, (void*) 1000000,
                              UAC_PCM_HW_PARAM_PERIOD_SIZE, UAC_PCM_HW_PARAM_PERIOD_TIME, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 7 ������   = ����֡��   * 1000000 / ����ʱ�� */
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_RATE,
                              uac_pcm_hw_rule_mulkdiv, (void*) 1000000,
                              UAC_PCM_HW_PARAM_BUFFER_SIZE, UAC_PCM_HW_PARAM_BUFFER_TIME, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 8 ������   = ����֡��   / ����֡�� */
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_PERIODS,
                              uac_pcm_hw_rule_div, NULL,
                              UAC_PCM_HW_PARAM_BUFFER_SIZE, UAC_PCM_HW_PARAM_PERIOD_SIZE, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 9 ����֡��   = ����֡�� / ����������*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_PERIOD_SIZE,
                              uac_pcm_hw_rule_div, NULL,
                              UAC_PCM_HW_PARAM_BUFFER_SIZE, UAC_PCM_HW_PARAM_PERIODS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 10 ����֡��   = �����ֽ��� * 8 / ֡λ��*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_PERIOD_SIZE,
                              uac_pcm_hw_rule_mulkdiv, (void*) 8,
                              UAC_PCM_HW_PARAM_PERIOD_BYTES, UAC_PCM_HW_PARAM_FRAME_BITS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 11 ����֡��   = ����ʱ��  * ������  / 1000000*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_PERIOD_SIZE,
                              uac_pcm_hw_rule_muldivk, (void*) 1000000,
                              UAC_PCM_HW_PARAM_PERIOD_TIME, UAC_PCM_HW_PARAM_RATE, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 12 ����֡�� = ����֡�� * ��������*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_BUFFER_SIZE,
                              uac_pcm_hw_rule_mul, NULL,
                              UAC_PCM_HW_PARAM_PERIOD_SIZE, UAC_PCM_HW_PARAM_PERIODS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 13 ����֡�� = �����ֽ��� * 8 / ֡λ��*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_BUFFER_SIZE,
                              uac_pcm_hw_rule_mulkdiv, (void*) 8,
                              UAC_PCM_HW_PARAM_BUFFER_BYTES, UAC_PCM_HW_PARAM_FRAME_BITS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 14 ����֡�� = ����ʱ�� * 1000000 / ������*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_BUFFER_SIZE,
                              uac_pcm_hw_rule_muldivk, (void*) 1000000,
                              UAC_PCM_HW_PARAM_BUFFER_TIME, UAC_PCM_HW_PARAM_RATE, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 15 �����ֽ��� = ����֡�� * 8 / ֡λ��*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_PERIOD_BYTES,
                              uac_pcm_hw_rule_muldivk, (void*) 8,
                              UAC_PCM_HW_PARAM_PERIOD_SIZE, UAC_PCM_HW_PARAM_FRAME_BITS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 16 �����ֽ��� = ����֡�� * 8 / ֡λ��*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_BUFFER_BYTES,
                              uac_pcm_hw_rule_muldivk, (void*) 8,
                              UAC_PCM_HW_PARAM_BUFFER_SIZE, UAC_PCM_HW_PARAM_FRAME_BITS, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 17 ����ʱ�� = ����֡�� * 1000000 / ������*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_PERIOD_TIME,
                              uac_pcm_hw_rule_mulkdiv, (void*) 1000000,
                              UAC_PCM_HW_PARAM_PERIOD_SIZE, UAC_PCM_HW_PARAM_RATE, -1);
    if (ret != USB_OK){
        return ret;
    }
    /* 18 ����ʱ�� = ����֡�� * 1000000 / ������*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_BUFFER_TIME,
                              uac_pcm_hw_rule_mulkdiv, (void*) 1000000,
                              UAC_PCM_HW_PARAM_BUFFER_SIZE, UAC_PCM_HW_PARAM_RATE, -1);
    if (ret != USB_OK){
        return ret;
    }
    return USB_OK;
}

/** ����PCMӲ����������*/
usb_err_t uac_pcm_hw_constraints_complete(struct uac_pcm_substream *substream)
{
    struct uac_pcm_runtime *runtime = substream->runtime;
    struct uac_pcm_hardware *hw = &runtime->hw;
    usb_err_t ret;

    /* ����ͨ����С���ֵ*/
    ret = uac_pcm_hw_constraint_minmax(runtime, UAC_PCM_HW_PARAM_CHANNELS,
                                       hw->channels_min, hw->channels_max);
    if (ret < 0){
        return ret;
    }
    /* ���ò�������С���ֵ*/
    ret = uac_pcm_hw_constraint_minmax(runtime, UAC_PCM_HW_PARAM_RATE,
                                       hw->rate_min, hw->rate_max);
    if (ret < 0){
        return ret;
    }
    /* ���������ֽ���С���ֵ*/
    ret = uac_pcm_hw_constraint_minmax(runtime, UAC_PCM_HW_PARAM_PERIOD_BYTES,
                                       hw->period_bytes_min, hw->period_bytes_max);
    if (ret < 0){
        return ret;
    }
    /* ��������������С���ֵ*/
    ret = uac_pcm_hw_constraint_minmax(runtime, UAC_PCM_HW_PARAM_PERIODS,
                                       hw->periods_min, hw->periods_max);
    if (ret < 0){
        return ret;
    }
    /* ���û����С��С���ֵ*/
    ret = uac_pcm_hw_constraint_minmax(runtime, UAC_PCM_HW_PARAM_BUFFER_BYTES,
                                       hw->period_bytes_min, hw->buffer_bytes_max);
    if (ret < 0){
        return ret;
    }
    /* ��ӻ����ֽ����ֵ����*/
    ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_BUFFER_BYTES,
                              uac_pcm_hw_rule_buffer_bytes_max, substream,
                              UAC_PCM_HW_PARAM_BUFFER_BYTES, -1, -1);
    if (ret < 0){
        return ret;
    }
    /* ��Ӳ����ʹ���*/
    if (!(hw->rates & (SNDRV_PCM_RATE_KNOT | SNDRV_PCM_RATE_CONTINUOUS))) {
        ret = uac_pcm_hw_rule_add(runtime, 0, UAC_PCM_HW_PARAM_RATE,
                                  uac_pcm_hw_rule_rate, hw,
                                  UAC_PCM_HW_PARAM_RATE, -1, -1);
        if (ret < 0){
            return ret;
        }
    }

    /* ��������֡����СΪ�̶�ֵ*/
    uac_pcm_hw_constraint_integer(runtime, UAC_PCM_HW_PARAM_PERIOD_SIZE);

    return 0;
}

/** ����usb��ƵӲ������*/
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

    /* �����ڴ�*/
    hw_params = usb_mem_alloc(sizeof(struct uac_pcm_hw_params));
    if(hw_params == NULL){
        return NULL;
    }
    memset(hw_params, 0, sizeof(struct uac_pcm_hw_params));
    /* ������֤���в���*/
    hw_params->rmask = ~0U;
    /* ���ø�ʽλ��͸�ʽ*/
    hw_params->pcm_format_size_bit = uac_get_pcm_format_size_bit(pcm_format_size);
    if(hw_params->pcm_format_size_bit >= 0){
        hw_params->format = (1 << hw_params->pcm_format_size_bit);
    }
    else{
        usb_mem_free(hw_params);
        return NULL;
    }
    /* ��������λ��*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_SAMPLE_BITS);
    hw_interval->integer = 1;
    hw_interval->min = params->bits_per_sample;
    hw_interval->max = params->bits_per_sample;
    /* ����ͨ����*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_CHANNELS);
    hw_interval->integer = 1;
    hw_interval->min = channels;
    hw_interval->max = channels;
    /* ����֡λ��*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_FRAME_BITS);
    hw_interval->integer = 1;
    hw_interval->min = params->pcm_format_size * channels;
    hw_interval->max = params->pcm_format_size * channels;
    /* ���ò�����*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_RATE);
    hw_interval->integer = 1;
    hw_interval->min = rate;
    hw_interval->max = rate;
    /* ����һ�����ڵ�֡��(һ������1024֡)*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_PERIOD_SIZE);
    hw_interval->integer = 1;
    hw_interval->min = 1024;
    hw_interval->max = 1024;
    /* �������ڵ��ֽ���*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_PERIOD_BYTES);
    hw_interval->integer = 1;
    hw_interval->min = 1024 * params->pcm_format_size * channels / 8;
    hw_interval->max = 1024 * params->pcm_format_size * channels / 8;
    /* ���û������������(һ������16������)*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_PERIODS);
    hw_interval->integer = 1;
    hw_interval->min = 16;
    hw_interval->max = 16;
    /* ���û���֡��(һ������16K֡)*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_BUFFER_SIZE);
    hw_interval->integer = 1;
    hw_interval->min = 16 * 1024;
    hw_interval->max = 16 * 1024;
    /* ���û����ֽڴ�С*/
    buffer_bytes_size = 16 * 1024 * params->pcm_format_size * channels / 8;
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_BUFFER_BYTES);
    hw_interval->integer = 1;
    hw_interval->min = buffer_bytes_size;
    hw_interval->max = buffer_bytes_size;
    /* ��������ʱ��(������ʱ����Ϊ��Сֵ�����ֵ)*/
    hw_interval = hw_param_hw_interval(hw_params, UAC_PCM_HW_PARAM_PERIOD_TIME);
    hw_interval->integer = 0;
    hw_interval->min = 0;
    hw_interval->max = USB_UINT_MAX;
    /* ���û���ʱ��*/
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



