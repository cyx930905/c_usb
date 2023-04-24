#ifndef __USBD_MS_SCSI_BBB_H__
#define __USBD_MS_SCSI_BBB_H__

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include <stdio.h>
#include <stdint.h>

#define USB_BBB_CBW_LEN               31          /* 命令块打包长度*/
#define USB_BBB_CSW_LEN               13          /* 命令状态包长度*/
#define USB_BBB_CBW_SIG               0x43425355  /* 命令块打包签名 */
#define USB_BBB_CSW_SIG               0x53425355  /* 命令状态包签名*/
#define USB_BBB_CBW_FLAG_IN           0x80        /* 命令块包输入标志*/
#define USB_BBB_CBW_FLAG_OUT          0x00        /* 命令块包输出标志*/
#define USB_BBB_CSW_STA_PASS          0           /* 命令状态包状态阶段通过*/
#define USB_BBB_CSW_STA_FAIL          1           /* 命令状态包状态阶段失败*/
#define USB_BBB_CSW_STA_PHASE_ERROR   2           /* 命令状态包状态阶段错误*/

/* \brief SCSI 命令 */
#define SCSI_CMD_INQUIRY                        0x12
#define SCSI_CMD_MODE_SELECT_6                  0x15
#define SCSI_CMD_MODE_SELECT_10                 0x55
#define SCSI_CMD_MODE_SENSE_6                   0x1a
#define SCSI_CMD_MODE_SENSE_10                  0x5a
#define SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL   0x1e
#define SCSI_CMD_READ_6                         0x08
#define SCSI_CMD_READ_10                        0x28
#define SCSI_CMD_READ_12                        0xa8
#define SCSI_CMD_READ_CAPACITY                  0x25
#define SCSI_CMD_READ_FORMAT_CAPACITIES         0x23
#define SCSI_CMD_READ_HEADER                    0x44
#define SCSI_CMD_READ_TOC                       0x43
#define SCSI_CMD_REQUEST_SENSE                  0x03
#define SCSI_CMD_START_STOP_UNIT                0x1b
#define SCSI_CMD_SYNCHRONIZE_CACHE              0x35
#define SCSI_CMD_TEST_UNIT_READY                0x00
#define SCSI_CMD_VERIFY                         0x2f
#define SCSI_CMD_WRITE_6                        0x0a
#define SCSI_CMD_WRITE_10                       0x2a
#define SCSI_CMD_WRITE_12                       0xaa

/* \brief SCSI Sense Key */
#define SCSI_SK_NO_SENSE                           0
#define SCSI_SK_COMMUNICATION_FAILURE              0x040800
#define SCSI_SK_INVALID_COMMAND                    0x052000
#define SCSI_SK_INVALID_FIELD_IN_CDB               0x052400
#define SCSI_SK_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE 0x052100
#define SCSI_SK_LOGICAL_UNIT_NOT_SUPPORTED         0x052500
#define SCSI_SK_SAVING_PARAMETERS_NOT_SUPPORTED    0x053900
#define SCSI_SK_UNRECOVERED_READ_ERROR             0x031100
#define SCSI_SK_WRITE_ERROR                        0x030c02
#define SCSI_SK_WRITE_PROTECTED                    0x072700

/* \brief SCSI 设备类型 */
#define SCSI_DEV_TYPE_DISK                      0x00    /* 磁盘 */
#define SCSI_DEV_TYPE_CDROM                     0x05    /* CDROM */

/* ASC Qualifier values */
#define SCSI_SK(x)                             ((uint8_t) ((x) >> 16))
#define SCSI_ASC(x)                            ((uint8_t) ((x) >> 8))
#define SCSI_ASCQ(x)                           ((uint8_t) (x))

/* \brief USB 大容量存储设备 SCSI 命令信息结构体*/
struct usbd_ms_scsi_cmd_info {
    uint8_t            cmd;
    int (*p_fn_handle)(struct usbd_ms *p_ms);
};

/* \brief USB 大容量存储设备 SCSI 信息结构体*/
struct usbd_ms_scsi_bbb_info {
    uint32_t tag;              /* CBW/CSW 标签 */
    uint32_t dlen;             /* CBW 数据长度 */
    uint32_t cdlen;            /* CBW 有效字节长度 */
    uint8_t  cb[16];           /* CBW 命令 */
    uint8_t  dir;              /* CBW 标志 */
    uint8_t  clen;             /* CBW 命令长度 */
    uint8_t  sta;              /* CSW 状态 */
};

/* \brief USB 大容量存储设备 SCSI 逻辑单元结构体 */
struct usbd_ms_scsi_bbb_lu {
    uint32_t    sd;             /* SCSI sense key */
    uint32_t    sd_info;        /* SCSI sense 信息 */
    usb_bool_t  is_inf_valid;   /* sense info 是否有效 */
    usb_bool_t  is_rdonly;      /* 是否只读 */
    usb_bool_t  is_rm;          /* 是否可移除 */
    usb_bool_t  is_pv;          /* prevent标志 */
};

/**
 * \brief 创建一个 USB 从机大容量存储设备（SCSI，BBB）
 *
 * \param[in]  p_dc     所属的 USB 从机控制器
 * \param[in]  p_name   设备名字
 * \param[in]  dev_desc 设备描述符
 * \param[in]  buf_size 传输缓存大小
 * \param[out] p_ms_ret 返回创建成功的 USB 从机大容量存储设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_scsi_bbb_create(struct usb_dc         *p_dc,
                            char                  *p_name,
                            struct usbd_dev_desc   dev_desc,
                            uint32_t               buf_size,
                            struct usbd_ms       **p_ms_ret);
/**
 * \brief 销毁 USB 从机大容量存储设备（SCSI，BBB）
 *
 * \param[in] p_ms 要销毁的 USB 从机大容量存储设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_scsi_bbb_destroy(struct usbd_ms *p_ms);
/**
 * \brief USB 从机大容量存储设备处理（SCSI，BBB）
 *
 * \param[in] p_ms 相关的 USB 从机大容量存储设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_scsi_bbb_process(struct usbd_ms *p_ms);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif

