/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "common/err/usb_err.h"
#include "config/usb_config.h"
#include "core/include/host/controller/ehci/usbh_ehci.h"
#include <string.h>

/*******************************************************************************
 * Statement
 ******************************************************************************/
struct usbh_ehci_qtd *usbh_ehci_qtd_alloc(struct usbh_ehci *p_ehci);
int usbh_ehci_qtd_free(struct usbh_ehci     *p_ehci,
                       struct usbh_ehci_qtd *p_qtd);

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbh_core_lib __g_usb_host_lib;

/*******************************************************************************
 * Marco operate
 ******************************************************************************/
/* \brief 所有数据结构都以 32 字节对齐*/
#define EHCI_QH_SIZE    (USB_DIV_ROUND_UP(sizeof(struct usbh_ehci_qh), USB_DMA_ALIGN_SIZE) * USB_DMA_ALIGN_SIZE)
#define EHCI_QTD_SIZE   (USB_DIV_ROUND_UP(sizeof(struct usbh_ehci_qtd), USB_DMA_ALIGN_SIZE) * USB_DMA_ALIGN_SIZE)
#define EHCI_ITD_SIZE   (USB_DIV_ROUND_UP(sizeof(struct usbh_ehci_itd), USB_DMA_ALIGN_SIZE) * USB_DMA_ALIGN_SIZE)
#define EHCI_SITD_SIZE  (USB_DIV_ROUND_UP(sizeof(struct usbh_ehci_sitd), USB_DMA_ALIGN_SIZE) * USB_DMA_ALIGN_SIZE)

#define EHCI_FL_SIZE    (sizeof(uint32_t) * p_ehci->frame_list_size)

/*******************************************************************************
 * Code
 ******************************************************************************/
#if (USB_MEM_RECORD_EN == 1)
/**
 * \brief EHCI 数据结构使用状况
 */
int usbh_ehci_mem_sta_get (struct usb_hc *p_hc){
    if (p_hc == NULL) {
        return -USB_EINVAL;
    }

    /* 获取EHCI控制器*/
    struct usbh_ehci *p_ehci = USBH_GET_EHCI_FROM_HC(p_hc);
    if (p_ehci == NULL) {
        return -USB_EILLEGAL;
    }

    __USB_INFO("EHCI Data Structure:\r\n");
    __USB_INFO("QH   Used: %d/%d\r\n", (p_ehci->qh_pool.item_count - p_ehci->qh_pool.n_free),
                                        p_ehci->qh_pool.item_count);
    __USB_INFO("QTD  Used: %d/%d\r\n", (p_ehci->qtd_pool.item_count - p_ehci->qtd_pool.n_free),
                                        p_ehci->qtd_pool.item_count);
    __USB_INFO("ITD  Used: %d/%d\r\n", (p_ehci->itd_pool.item_count - p_ehci->itd_pool.n_free),
                                        p_ehci->itd_pool.item_count);
    __USB_INFO("SITD Used: %d/%d\r\n", (p_ehci->sitd_pool.item_count - p_ehci->sitd_pool.n_free),
                                        p_ehci->sitd_pool.item_count);
    return USB_OK;
}
#endif

/**
 * \brief 为 EHCI 所需的所有数据结构申请内存空间
 *
 * \param[in] p_ehci EHCI 结构体
 * \param[in] n_qh   QH 的数量
 * \param[in] n_qtd  qTD 的数量
 * \param[in] n_itd  iTD 的数量
 * \param[in] n_sitd siTD 的数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ehci_mem_init(struct usbh_ehci *p_ehci,
                       uint8_t           mem_type,
                       uint32_t          n_qh,
                       uint32_t          n_qtd,
                       uint32_t          n_itd,
                       uint32_t          n_sitd){
    uint32_t i;
    int      ret      = -USB_ENOMEM;
    int      ret_tmp, mem_size;
    void    *p_handle = NULL;
    char    *p_mem    = NULL;

    if ((mem_type != USBH_EHCI_MEM_POOL) &&
            (mem_type != USBH_EHCI_MEM)) {
        return -USB_EILLEGAL;
    }

    if (mem_type == USBH_EHCI_MEM_POOL) {
        /* 申请周期调度空间，4K对齐*/
        p_ehci->p_periodic = usb_lib_dma_align_malloc(&__g_usb_host_lib.lib, EHCI_FL_SIZE, 4096);
        if (p_ehci->p_periodic == NULL) {
            return -USB_ENOMEM;
        }

        /* 周期列表所有元素地址初始化为无效*/
        for (i = 0; i < p_ehci->frame_list_size; i++) {
            p_ehci->p_periodic[i] = USB_CPU_TO_LE32(1u);
        }
        /* 计算数据结构大小*/
        mem_size = (n_qh * EHCI_QH_SIZE) +
                   (n_qtd * EHCI_QTD_SIZE) +
                   (n_itd * EHCI_ITD_SIZE) +
                   (n_sitd * EHCI_SITD_SIZE);

        /* 申请 EHCI 的所有数据结构体的内存空间*/
        p_mem = usb_lib_dma_align_malloc(&__g_usb_host_lib.lib, mem_size, USB_DMA_ALIGN_SIZE);
        if (p_mem == NULL) {
            ret = -USB_ENOMEM;
            goto __failed;
        }

        p_ehci->ds_size = mem_size;
        
        if (n_qh > 0) {            /* 初始化 QH(队列头)的内存空间，QH 内存空间中每个内存块大小是一个 QH 大小*/
            p_handle = usb_pool_init(&p_ehci->qh_pool, p_mem, (n_qh * EHCI_QH_SIZE), EHCI_QH_SIZE);
            if (p_handle == NULL) {
                ret = -USB_ENOMEM;
                __USB_ERR_INFO("usb QH pool init failed\r\n");
                goto __failed;
            }
            p_mem += (n_qh * EHCI_QH_SIZE);
        }

        if (n_qtd > 0) {
            /* 初始化 qTD (队列元素传输描述符)的内存空间，qTD 内存空间每个内存块大小是一个 qTD 大小*/
            p_handle = usb_pool_init(&p_ehci->qtd_pool, p_mem, (n_qtd * EHCI_QTD_SIZE), EHCI_QTD_SIZE);
            if (p_handle == NULL) {
                ret = -USB_ENOMEM;
                __USB_ERR_INFO("usb qTD pool init failed\r\n");
                goto __failed;
            }
            p_mem += (n_qtd * EHCI_QTD_SIZE);
        }

        if (n_itd > 0) {
            /* 初始化 iTD(同步传输描述符)内存空间，iTD 内存空间每个内存块大小是一个 iTD 大小*/
            p_handle = usb_pool_init(&p_ehci->itd_pool, p_mem, (n_itd * EHCI_ITD_SIZE), EHCI_ITD_SIZE);
            if (p_handle == NULL) {
                ret = -USB_ENOMEM;
                __USB_ERR_INFO("usb iTD pool init failed\r\n");
                goto __failed;
            }
            p_mem += (n_itd * EHCI_ITD_SIZE);
        }

        if (n_sitd > 0) {
            /* 初始化 siTD(分割事务同步传输描述符)内存空间，siTD 内存空间每个内存块大小是一个 siTD 大小*/
            p_handle = usb_pool_init(&p_ehci->sitd_pool, p_mem, (n_sitd * EHCI_SITD_SIZE), EHCI_SITD_SIZE);
            if (p_handle == NULL) {
                ret = -USB_ENOMEM;
                __USB_ERR_INFO("usb siTD pool init failed\r\n");
                goto __failed;
            }
        }
    } else if (mem_type == USBH_EHCI_MEM) {
        /* 申请周期调度空间，4K对齐*/
        p_ehci->p_periodic = (void*)usb_lib_dma_align_malloc(&__g_usb_host_lib.lib, EHCI_FL_SIZE, 4096);
        if (p_ehci->p_periodic == NULL) {
            ret = -USB_ENOMEM;
            goto __failed;
        }

        for (i = 0; i < p_ehci->frame_list_size; i++) {
            p_ehci->p_periodic[i] = USB_CPU_TO_LE32(1u);
        }
    }

    p_ehci->mem_type = mem_type;

    /* 申请周期帧列表数据结构内存，每个帧元素是一个地址*/
    p_ehci->p_shadow = usb_lib_malloc(&__g_usb_host_lib.lib, p_ehci->frame_list_size * sizeof(void *));
    if (p_ehci->p_shadow == NULL) {
        ret = -USB_ENOMEM;
        goto __failed;
    }

    memset(p_ehci->p_shadow, 0, p_ehci->frame_list_size * sizeof(void *));

    return USB_OK;
__failed:
    if (p_mem) {
        if (mem_type == USBH_EHCI_MEM_POOL) {
            ret_tmp = usb_lib_dma_mfree(&__g_usb_host_lib.lib, p_mem, mem_size);
            if (ret_tmp != USB_OK) {
            	__USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret_tmp);
            }
        }
    }
    ret_tmp = usb_lib_dma_mfree(&__g_usb_host_lib.lib, p_ehci->p_periodic, p_ehci->frame_list_size);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret_tmp);
    }

    return ret;
}

/**
 * \brief 反初始化 EHCI 内存
 */
int usbh_ehci_mem_deinit(struct usbh_ehci *p_ehci){
    usb_lib_mfree(&__g_usb_host_lib.lib, p_ehci->p_shadow);

    usb_lib_dma_mfree(&__g_usb_host_lib.lib, p_ehci->qh_pool.p_start, p_ehci->ds_size);

    usb_lib_dma_mfree(&__g_usb_host_lib.lib, p_ehci->p_periodic, EHCI_FL_SIZE);

    return USB_OK;
}

/**
 * \brief 分配 QH(队列头)
 *
 * \param[in] p_ehci EHCI 结构体
 *
 * \retval 成功返回分配的 QH 结构体
 */
struct usbh_ehci_qh *usbh_ehci_qh_alloc(struct usbh_ehci *p_ehci){
    struct usbh_ehci_qh *p_qh;
    int                  ret;

    if (p_ehci->mem_type == USBH_EHCI_MEM_POOL) {
        p_qh = (struct usbh_ehci_qh *)usb_pool_item_get(&p_ehci->qh_pool);
    }
    else if (p_ehci->mem_type == USBH_EHCI_MEM) {
        p_qh = usb_lib_dma_align_malloc(&__g_usb_host_lib.lib, sizeof(struct usbh_ehci_qh), USB_DMA_ALIGN_SIZE);
    }

    if (p_qh == NULL) {
        return NULL;
    }

    memset(p_qh, 0, sizeof(struct usbh_ehci_qh));

    p_qh->p_dummy = usbh_ehci_qtd_alloc(p_ehci);
    if (p_qh->p_dummy == NULL) {
        goto __failed;
    }

    p_qh->p_ehci = p_ehci;
    p_qh->state  = 0;
    usb_list_head_init(&p_qh->qtds);

    return p_qh;
__failed:
    if (p_ehci->mem_type == USBH_EHCI_MEM_POOL) {
        ret = usb_pool_item_return(&p_ehci->qh_pool, p_qh);
    } else {
        ret = usb_lib_dma_mfree(&__g_usb_host_lib.lib, p_qh, sizeof(struct usbh_ehci_qh));
    }
    if (ret != USB_OK) {
        __USB_ERR_TRACE(ErrorTrace, "(%d)\r\n", ret);
    }
    return NULL;
}

/**
 * \brief 释放 QH(队列头)
 */
int usbh_ehci_qh_free(struct usbh_ehci    *p_ehci,
                      struct usbh_ehci_qh *p_qh){
    int ret;

    if (!usb_list_head_is_empty(&p_qh->qtds)) {
        return -USB_EILLEGAL;
    }

    if (p_qh->p_dummy != NULL) {
        ret = usbh_ehci_qtd_free(p_ehci, p_qh->p_dummy);
        if (ret != USB_OK) {
            return ret;
        }
    }

    if (p_ehci->mem_type == USBH_EHCI_MEM_POOL) {
        return usb_pool_item_return(&p_ehci->qh_pool, p_qh);
    }
    return usb_lib_dma_mfree(&__g_usb_host_lib.lib, p_qh, sizeof(struct usbh_ehci_qh));
}

/**
 * \brief 分配 QTD(队列传输描述符)
 *
 * \param[in] p_ehci EHCI结构体
 *
 * \retval 成功返回分配的 QTD 结构体
 */
struct usbh_ehci_qtd *usbh_ehci_qtd_alloc(struct usbh_ehci *p_ehci){
    struct usbh_ehci_qtd *p_qtd;

    if (p_ehci->mem_type == USBH_EHCI_MEM_POOL) {
        p_qtd = (struct usbh_ehci_qtd *)usb_pool_item_get(&p_ehci->qtd_pool);
    } else if (p_ehci->mem_type == USBH_EHCI_MEM) {
        p_qtd = usb_lib_dma_align_malloc(&__g_usb_host_lib.lib, sizeof(struct usbh_ehci_qtd), USB_DMA_ALIGN_SIZE);
    }
    if (p_qtd == NULL) {
        return NULL;
    }

    memset(p_qtd, 0, sizeof(*p_qtd));

    USB_LE_REG_WRITE32((1u << 6), &p_qtd->hw_token);
    USB_LE_REG_WRITE32(1u, &p_qtd->hw_next);
    USB_LE_REG_WRITE32(1u, &p_qtd->hw_alt_next);

    usb_list_node_init(&p_qtd->node);

    return p_qtd;
}

/**
 * \brief 释放 QTD(队列传输描述符)
 */
int usbh_ehci_qtd_free(struct usbh_ehci     *p_ehci,
                       struct usbh_ehci_qtd *p_qtd){
    if (p_ehci->mem_type == USBH_EHCI_MEM_POOL) {
        return usb_pool_item_return(&p_ehci->qtd_pool, p_qtd);
    }
    return usb_lib_dma_mfree(&__g_usb_host_lib.lib, p_qtd, sizeof(struct usbh_ehci_qtd));
}

/**
 * \brief 分配一个 ITD(等时传输描述符)
 */
struct usbh_ehci_itd *usbh_ehci_itd_alloc(struct usbh_ehci *p_ehci){
    if (p_ehci->mem_type == USBH_EHCI_MEM_POOL) {
        return (struct usbh_ehci_itd *)usb_pool_item_get(&p_ehci->itd_pool);
    }
    return usb_lib_dma_align_malloc(&__g_usb_host_lib.lib, sizeof(struct usbh_ehci_itd), USB_DMA_ALIGN_SIZE);
}

/**
 * \brief 释放 ITD(等时传输描述符)
 */
int usbh_ehci_itd_free(struct usbh_ehci *p_ehci, struct usbh_ehci_itd *p_itd){
    if (p_ehci->mem_type == USBH_EHCI_MEM_POOL) {
        return usb_pool_item_return(&p_ehci->itd_pool, p_itd);
    }
    return usb_lib_dma_mfree(&__g_usb_host_lib.lib, p_itd, sizeof(struct usbh_ehci_itd));
}

/**
 * \brief 分配一个 SITD(分割等时传输描述符)
 */
struct usbh_ehci_sitd *usbh_ehci_sitd_alloc(struct usbh_ehci *p_ehci){
    if (p_ehci->mem_type == USBH_EHCI_MEM_POOL) {
        return (struct usbh_ehci_sitd *)usb_pool_item_get(&p_ehci->sitd_pool);
    }
    return usb_lib_dma_align_malloc(&__g_usb_host_lib.lib, sizeof(struct usbh_ehci_sitd), USB_DMA_ALIGN_SIZE);
}

/**
 * \brief 释放 SITD(分割等时传输描述符)
 */
int usbh_ehci_sitd_free(struct usbh_ehci *p_ehci, struct usbh_ehci_sitd *p_sitd){
    if (p_ehci->mem_type == USBH_EHCI_MEM_POOL) {
        return usb_pool_item_return(&p_ehci->sitd_pool, p_sitd);
    }
    return usb_lib_dma_mfree(&__g_usb_host_lib.lib, p_sitd, sizeof(struct usbh_ehci_sitd));
}
