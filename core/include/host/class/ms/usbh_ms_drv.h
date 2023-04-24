#ifndef __USBH_MS_DRV_H
#define __USBH_MS_DRV_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "core/include/host/core/usbh_dev.h"
#include "core/include/host/core/usbh.h"
#include "core/include/specs/usb_ms_specs.h"
#include "core/include/usb_lib.h"
#include "adapter/usb_adapter.h"
#include "common/usb_common.h"
#include "common/list/usb_list.h"

#if USB_OS_EN
#define UMS_LOCK_TIMEOUT             5000
#endif

///* \brief USB 大容量存储设备设备类型*/
//#define USBH_MS_SC_RBC               0x01    /* flash 设备*/
//#define USBH_MS_SC_8020              0x02    /* CD's DVD's*/
//#define USBH_MS_SC_QIC               0x03    /* 磁带*/
//#define USBH_MS_SC_UFI               0x04    /* Floppy*/
//#define USBH_MS_SC_8070              0x05    /* Floppies(mostly)*/
//#define USBH_MS_SC_SCSI              0x06    /* SCSI 设备*/
//#define USBH_MS_SC_RAMD              0x13    /* Ram 磁盘设备*/

/* \brief 获取USB大容量存储设备VID*/
#define USBH_MS_DEV_VID_GET(p_ms)     USBH_DEV_VID_GET(p_ms->p_usb_fun)
/* \brief 获取USB大容量存储设备PID*/
#define USBH_MS_DEV_PID_GET(p_ms)     USBH_DEV_PID_GET(p_ms->p_usb_fun)

/* \brief USB 大容量存储设备库结构体*/
struct ums_lib {
    struct usb_lib_base lib;               /* USB 库*/
    usb_bool_t          is_lib_deiniting;  /* 是否移除库*/
    int                 xfer_time_out;     /* 传输超时时间*/
    int                 ref_cnt;           /* 引用计数*/
};

/* \brief USB 大容量存储设备逻辑单元结构体 */
struct usbh_ms_lu {
    struct usbh_ms   *p_ms;                   /* 相关的大容量存储设备*/
    char              name[USB_NAME_LEN];     /* 逻辑单元名字*/
    uint8_t           lun_num;                /* 逻辑单元号*/
    uint32_t          n_blks;                 /* 块数量*/
    uint32_t          blk_size;               /* 块大小*/
    usb_bool_t        is_wp;                  /* 是否有写保护*/
    void             *p_buf;                  /* 数据缓存*/
    uint32_t          buf_size;               /* 数据缓存大小*/
    usb_bool_t        is_init;                /* 是否初始化成功*/
    void             *p_usr_priv;             /* 用户私有数据*/
};

/* \brief 子类操作函数集*/
struct usbh_ms_sclass {
    uint8_t     id;
    int       (*p_fn_init)(struct usbh_ms_lu *p_lun);

    int       (*p_fn_read)(struct usbh_ms_lu *p_lun,
                           uint32_t           blk_num,
                           uint32_t           n_blks,
                           void              *p_buf);

    int       (*p_fn_write)(struct usbh_ms_lu *p_lun,
                            uint32_t           blk_num,
                            uint32_t           n_blks,
                            void              *p_buf);

};

/* \brief USB 主机大容量存储设备*/
struct usbh_ms {
    struct usbh_function   *p_usb_fun;           /* 相关的功能接口*/
    char                    name[USB_NAME_LEN];  /* 大容量存储设备名字*/
#if USB_OS_EN
    usb_mutex_handle_t      p_lock;
#endif
    struct usbh_endpoint   *p_ep_in;
    struct usbh_endpoint   *p_ep_out;
    struct usbh_endpoint   *p_ep_intr;           /* 中断端点*/
    struct usbh_ms_sclass  *p_sclass;            /* 大容量存储设备类别*/
    uint8_t                 n_lu;                /* 逻辑分区数量*/
    int                     ref_cnt;             /* 引用计数*/
    uint8_t                 pro;                 /* 接口协议*/
    int                     tag;
    struct usb_list_node    node;                /* 大容量存储设备节点*/
    struct usbh_ms_lu      *p_lus;               /* 逻辑单元*/
    usb_bool_t              is_removed;          /* 移除标志*/
    void                   *p_cbw;               /* 命令块包*/
    void                   *p_csw;               /* 命令状态包*/
};

///**
// * \brief 设置 USB 大容量存储设备传输超时时间
// *
// * \param[in] time_out 要设置的超时时间
// *
// * \retval 成功返回 USB_OK
// */
//int usbh_ms_xfer_timeout_set(int time_out);
///**
// * \brief 获取USB大容量存储设备传输超时时间
// */
//int usbh_ms_xfer_timeout_get(int *pTimeOut);
/**
 * \brief 复位 USB 大容量存储设备遍历
 */
void usbh_ms_traverse_reset(void);
/**
 * \brief USB 大容量存储设备遍历
 *
 * \param[out] p_name    返回的名字缓存
 * \param[in]  name_size 名字缓存的大小
 * \param[out] p_vid     存储设备 VID 的缓存
 * \param[out] p_pid     存储设备 PID 的缓存
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_traverse(char     *p_name,
                     uint32_t  name_size,
                     uint16_t *p_vid,
                     uint16_t *p_pid);
/**
 * \brief USB 大容量存储设备探测函数
 */
int usbh_ms_probe(struct usbh_function *p_usb_fun);
/**
 * \brief 创建一个 USB 大容量存储设备
 *
 * \param[in] p_usb_fun    相关的 USB 功能接口
 * \param[in] p_name       USB 大容量存储设备的名字
 * \param[in] lu_buf_size  U盘数据交互缓存，影响U盘读写性能，推荐16k
 *
 * \retval 成功返回 USB_OK;
 */
int usbh_ms_create(struct usbh_function *p_usb_fun,
                   char                 *p_name,
                   uint32_t              lu_buf_size);
/**
 * \brief 销毁 USB 大容量存储设备
 *
 * \param[in] p_usb_fun USB 功能结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_destory(struct usbh_function *p_usb_fun);
/**
 * \brief 打开一个 USB 大容量存储设备
 *
 * \param[in]  p_handle 打开句柄
 * \param[in]  flag     打开标志，本接口支持两种种打开方式：
 *                      USBH_DEV_OPEN_BY_NAME 是通过名字打开设备
 *                      USBH_DEV_OPEN_BY_FUN  是通过 USB 功能结构体打开设备
 * \param[out] p_ms     返回获取到的 USB 大容量存储设备地址
 *
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_open(void *p_handle, uint8_t flag, struct usbh_ms **p_ms);
/**
 * \brief 关闭 USB 大容量存储设备
 *
 * \param[out] p_ms 要关闭的 USB 大容量存储设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_close(struct usbh_ms *p_ms);
/**
 * \brief USB 大容量存储设备块写函数
 *
 * \param[in] p_lun   逻辑单元结构体
 * \param[in] blk_num 起始块编号
 * \param[in] n_blks  块数量
 * \param[in] p_buf   数据缓存(为 NULL 则用逻辑分区的缓存)
 *
 * \retval 成功返回实际读写成功的字节数
 */
int usbh_ms_blk_write(struct usbh_ms_lu *p_lu,
                      uint32_t           blk_num,
                      uint32_t           n_blks,
                      void              *p_buf);
/**
 * \brief USB 大容量存储设备块读函数
 *
 * \param[in] p_lun   逻辑单元结构体
 * \param[in] blk_num 起始块编号
 * \param[in] n_blks  块数量
 * \param[in] p_buf   数据缓存(为NULL则用逻辑分区的缓存)
 *
 * \retval 成功返回实际读写成功的字节数
 */
int usbh_ms_blk_read(struct usbh_ms_lu *p_lu,
                     uint32_t           blk_num,
                     uint32_t           n_blks,
                     void              *p_buf);
/**
 * \brief 获取 USB 大容量存储设备支持的最大逻辑单元数量
 *
 * \param[in] p_ms   USB 大容量存储设备结构体
 * \param[in] p_nlus 存储最大逻辑单元数量的缓存
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_max_nlu_get(struct usbh_ms *p_ms, uint8_t *p_nlus);
/**
 * \brief 获取 USB 大容量存储设备当前的逻辑单元数量
 *
 * \param[in] p_ms   USB 大容量存储设备结构体
 * \param[in] p_nlus 存储逻辑单元数量的缓存
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_valid_nlu_get(struct usbh_ms *p_ms, uint8_t *p_nlus);
/**
 * \brief 获取 USB 大容量存储设备名字
 *
 * \param[in]  p_ms      USB 大容量存储设备结构体
 * \param[out] p_name    存储名字的缓存
 * \param[in]  name_size 名字缓存大小
 *
 * \retval 成功获取返回 USB_OK
 */
int usbh_ms_name_get(struct usbh_ms *p_ms, char *p_name, uint32_t name_size);
/**
 * \brief 设置 USB 大容量存储设备名字
 *
 * \param[in]  p_ms      USB 大容量存储设备结构体
 * \param[in]  p_name_new 要设置的新的名字
 *
 * \retval 成功设置返回 USB_OK
 */
int usbh_ms_name_set(struct usbh_ms *p_ms, char *p_name_new);
/**
 * \brief 打开 USB 大容量存储设备逻辑单元
 *
 * \param[in]  p_ms USB 大容量存储设备
 * \param[in]  lun  逻辑单元号
 * \param[out] p_lu 返回逻辑单元结构体地址
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_open(struct usbh_ms     *p_ms,
                    uint8_t             lun,
                    struct usbh_ms_lu **p_lu);
/**
 * \brief 关闭逻辑单元
 *
 * \param[in] p_lu 要关闭的逻辑单元结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_close(struct usbh_ms_lu *p_lu);
/**
 * \brief 检查逻辑单元是否写保护
 *
 * \param[in] p_lu    逻辑单元结构域
 * \param[in] p_is_wp 写保护标志缓存
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_is_wp(struct usbh_ms_lu *p_lu, usb_bool_t *p_is_wp);
/**
 * \brief 获取逻辑单元名字
 *
 * \param[in]  p_lu      逻辑单元结构体
 * \param[out] p_name    存储逻辑单元名字的缓存
 * \param[in]  name_size 名字缓存大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_name_get(struct usbh_ms_lu *p_lu, char *p_name, uint32_t name_size);
/**
 * \brief 设置逻辑单元名字
 *
 * \param[in] p_lu       逻辑单元结构体
 * \param[in] p_name_new 要设置的名字
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_name_set(struct usbh_ms_lu *p_lu, char *p_name_new);
/**
 * \brief 逻辑单元缓冲读数据
 *        当调用 usbh_ms_blk_read 函数是最后一个参数设置为空时
 *        可用这个函数读数据
 *
 * \param[in] p_lu  USB 大容量存储设备单元结构体
 * \param[in] p_buf 读缓冲
 * \param[in] size  读数据大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_buf_read(struct usbh_ms_lu *p_lu, void *p_buf, uint32_t size);
/**
 * \brief 逻辑单元缓冲写数据
 *        当调用 usbh_ms_blk_write 函数是最后一个参数设置为空时
 *        可用这个函数写数据
 *
 *
 * \param[in] p_lu      逻辑单元结构体
 * \param[in] p_src_buf 写缓冲
 * \param[in] size      写大小
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_buf_write(struct usbh_ms_lu *p_lu, void *p_buf, uint32_t size);
/**
 * \brief 设置逻辑单元用户私有数据
 *
 * \param[in] p_lu       逻辑单元结构体
 * \param[in] p_usr_priv 用户私有数据
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_usrdata_set(struct usbh_ms_lu *p_lu, void *p_usr_priv);
/**
 * \brief 获取逻辑单元用户私有数据
 *
 * \param[in]  p_lu       逻辑单元结构体
 * \param[out] p_usr_priv 返回的用户私有数据
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_usrdata_get(struct usbh_ms_lu *p_lu, void **p_usr_priv);
/**
 * \brief 获取 USB 大容量存储设备逻辑单元块大小
 *
 * \param[in] p_lu       逻辑单元结构体
 * \param[in] p_blk_size 存储块大小数据
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_blk_size_get(struct usbh_ms_lu *p_lu, uint32_t *p_blk_size);
/**
 * \brief 获取 USB 大容量存储设备逻辑单元块数量
 *
 * \param[in] p_lu   逻辑单元结构体
 * \param[in] p_nblk 存储块数量数据
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lu_nblks_get(struct usbh_ms_lu *p_lu, uint32_t *p_nblk);

/**
 * \brief 获取当前存在的 USB 大容量存储设备数量
 *
 * \param[out] p_n_dev 存储返回的设备数量
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lib_ndev_get(uint8_t *p_n_dev);
/**
 * \brief 初始化 USB 大容量存储设备库
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lib_init(void);
/**
 * \brief 反初始化 USB 大容量存储设备库
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_lib_deinit(void);
#if USB_MEM_RECORD_EN
/**
 * \brief 获取 USB 大容量存储驱动内存记录
 *
 * \param[out] p_mem_record 返回内存记录
 *
 * \retval 成功返回 USB_OK
 */
int usbh_ms_mem_record_get(struct usb_mem_record *p_mem_record);
#endif
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif

