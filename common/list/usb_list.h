#ifndef __USB_LIST_H
#define __USB_LIST_H

#ifdef __cplusplus
extern "C" {
#endif
#include <string.h>
#include <stdio.h>
#include "common/err/usb_err.h"

/**
 * \brief  定义空指针
 */
#ifndef USB_LIST_POISON1
#define USB_LIST_POISON1  NULL
#endif

#ifndef USB_LIST_POISON2
#define USB_LIST_POISON2  NULL
#endif

/* \brief 双向链表节点结构体 */
struct usb_list_node {
    struct usb_list_node *p_next, *p_prev;
};

/* \brief 双向链表头结构体 */
struct usb_list_head {
    struct usb_list_node *p_next, *p_prev;
};

/**
 * \brief 链表节点初始化
 *
 * \param[in] p_node 要初始化的链表节点
 */
static inline void usb_list_node_init(struct usb_list_node *p_node){
    p_node->p_next = USB_LIST_POISON1;
    p_node->p_prev = USB_LIST_POISON2;
}

/**
 * \brief 链表头初始化
 *
 * \param[in] p_head 要初始化的链表头
 */
static inline void usb_list_head_init(struct usb_list_head *p_head){
    p_head->p_next = (struct usb_list_node *)p_head;
    p_head->p_prev = (struct usb_list_node *)p_head;
}

/**
 * \brief 判断链表节点是否在链表中
 *
 * \param[in] p_node 目标节点
 *
 * \return 是则返回 1，不是则返回 0
 */
static inline int usb_list_node_is_empty(const struct usb_list_node *p_node){
    return ((p_node->p_next == USB_LIST_POISON1) && (p_node->p_prev == USB_LIST_POISON2));
}

/**
 * \brief 判断链表是否是空链表
 *
 * \param[in] p_head 目标链表
 *
 * \return 是则返回 1，不是则返回 0
 */
static inline int usb_list_head_is_empty(const struct usb_list_head *p_head){
    return p_head->p_next == (struct usb_list_node *)p_head;
}

/**
 * \brief 判断链表是否只有一个节点
 *
 * \param[in] p_head 目标链表
 *
 * \return 是则返回 1，不是则返回 0
 */
static inline int usb_list_head_is_singular(const struct usb_list_head *p_head){
    return !usb_list_head_is_empty(p_head) && (p_head->p_next == p_head->p_prev);
}

/**
 * \brief 添加节点到链表中
 */
static inline void __usb_list_node_add(struct usb_list_node *p_node,
                                       struct usb_list_node *p_prev,
                                       struct usb_list_node *p_next){
    p_next->p_prev = p_node;
    p_node->p_next = p_next;
    p_node->p_prev = p_prev;
    p_prev->p_next = p_node;
}

/**
 * \brief 将节点添加到链表中
 *
 * \param[in] p_node 要添加进链表的节点
 * \param[in] p_head 链表头
 */
static inline void usb_list_node_add(struct usb_list_node *p_node,
                                     struct usb_list_head *p_head){
    __usb_list_node_add(p_node, (struct usb_list_node *)p_head, p_head->p_next);
}

/**
 * \brief 将节点添加到链表末尾
 *
 * \param[in] p_node 要添加进链表的节点
 * \param[in] p_head 链表头
 */
static inline void usb_list_node_add_tail(struct usb_list_node *p_node,
                                          struct usb_list_head *p_head){

    __usb_list_node_add(p_node, p_head->p_prev, (struct usb_list_node *)p_head);
}

/**
 * \brief 删除节点
 */
static inline void __usb_list_node_del(struct usb_list_node *p_prev,
                                       struct usb_list_node *p_next){
    p_next->p_prev = p_prev;
    p_prev->p_next = p_next;
}

/**
 * \brief 将节点从链表中删除
 *
 * \param[in] p_node 要删除的节点
 */
static inline void usb_list_node_del(struct usb_list_node *p_node){
    __usb_list_node_del(p_node->p_prev, p_node->p_next);

    p_node->p_next = USB_LIST_POISON1;
    p_node->p_prev = USB_LIST_POISON2;
}

/**
 * \brief 将节点移动到另一条链表
 *
 * \param[in] p_node 目标节点
 * \param[in] p_head 目标链表
 */
static inline void usb_list_node_move(struct usb_list_node *p_node,
                                      struct usb_list_head *p_head){
    __usb_list_node_del(p_node->p_prev, p_node->p_next);
    usb_list_node_add(p_node, p_head);
}

/**
 * \brief 将节点移动到另一条链表末尾
 *
 * \param[in] list 目标节点
 * \param[in] head 目标链表
 */
static inline void usb_list_node_move_tail(struct usb_list_node *p_node,
                                           struct usb_list_head *p_head){
    __usb_list_node_del(p_node->p_prev, p_node->p_next);
    usb_list_node_add_tail(p_node, p_head);
}

/**
 * \brief 链表插入
 */
static inline void __usb_list_head_splice(const struct usb_list_head *p_head,
                                          struct usb_list_node       *p_prev,
                                          struct usb_list_node       *p_next){
    struct usb_list_node *p_first = p_head->p_next;
    struct usb_list_node *p_last  = p_head->p_prev;

    p_first->p_prev = p_prev;
    p_prev->p_next  = p_first;

    p_last->p_next  = p_next;
    p_next->p_prev  = p_last;
}

/**
 * \brief 将 p_head_src 链表剪切到 p_head_desc 链表头部
 *
 * \param[in] p_head_src  源链表
 * \param[in] p_head_desc 目的链表
 */
static inline void usb_list_head_splice(const struct usb_list_head *p_head_src,
                                        struct usb_list_head       *p_head_desc){
    if (!usb_list_head_is_empty(p_head_src)) {
        __usb_list_head_splice(p_head_src,
                              (struct usb_list_node *)p_head_desc,
                              (struct usb_list_node *)p_head_desc->p_next);
    }
}

/**
 * \brief 将 p_head_src 链表剪切到 p_head_desc 链表尾部
 *
 * \param[in] p_head_src  源链表
 * \param[in] p_head_desc 目的链表
 */
static inline void usb_list_head_splice_tail(struct usb_list_head *p_head_src,
                                             struct usb_list_head *p_head_desc){
    if (!usb_list_head_is_empty(p_head_src))
        __usb_list_head_splice(p_head_src, p_head_desc->p_prev, (struct usb_list_node *)p_head_desc);
}

/**
 * \brief 遍历一条链表
 *
 * \param[out] pos  存放节点的地址
 * \param[in]  head 链表地址
 */
#define usb_list_for_each_node(pos, head)       \
    for (pos = (head)->p_next;                  \
         pos != (struct usb_list_node *)(head); \
         pos = pos->p_next)

/**
 * \brief 遍历一条链表，带 safe 是防止删除节点造成断链的发生
 *
 * \param[out] pos  存放节点地址
 * \param[in]  n    临时存放节点地址的变量
 * \param[in]  head 链表地址
 */
#define usb_list_for_each_node_safe(pos, n, head) \
    for (pos = (head)->p_next, n = pos->p_next;   \
         pos != (struct usb_list_node *)(head);   \
         pos = n, n = pos->p_next)



#ifdef __cplusplus
}
#endif

#endif /* __USB_LIST_H */

/* end of file */
