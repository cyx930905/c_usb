/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/ms/usbh_ms_drv.h"
#include <string.h>

/*******************************************************************************
 * Macro operate
 ******************************************************************************/
/* \brief SCSI 命令代码 */
#define __TEST_UNIT_READY           0x00
#define __REQUEST_SENSE             0x03
#define __INQUIRY                   0x12
#define __MODE_SENSE                0x1a
#define __READ_FORMAT_CAPACITIES    0x23
#define __READ_CAPACITY             0x25
#define __READ_10                   0x28
#define __WRITE_10                  0x2a

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern int usbh_ms_transport(struct usbh_ms_lu *p_lu,
                             void              *p_cmd,
                             uint8_t            cmd_len,
                             void              *p_data,
                             uint32_t           data_len,
                             uint8_t            dir);

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 查询逻辑单元信息
 */
static int __inquiry(struct usbh_ms_lu *p_lu,
                     void              *p_buf,
                     uint8_t            len){
    uint8_t cmd[6];

    memset(cmd, 0, 6);

    cmd[0] = __INQUIRY;
    cmd[4] = len;

    return usbh_ms_transport(p_lu,
                             cmd,
                             6,
                             p_buf,
                             len,
                             USB_DIR_IN);
}

/**
 * \brief 检查逻辑单元是否准备好
 */
static int __unit_ready(struct usbh_ms_lu *p_lu){
    uint8_t cmd[6];

    memset(cmd, 0, 6);

    cmd[0] = __TEST_UNIT_READY;

    return usbh_ms_transport(p_lu,
                             cmd,
                             6,
                             NULL,
                             0,
                             USB_DIR_OUT);
}

static int __request_sense(struct usbh_ms_lu *p_lu,
                           void              *p_buf,
                           uint8_t            len){
    uint8_t cmd[6];

    memset(cmd, 0, 6);

    cmd[0] = __REQUEST_SENSE;
    cmd[4] = len;

    return usbh_ms_transport(p_lu,
                             cmd,
                             6,
                             p_buf,
                             len,
                             USB_DIR_IN);
}

/**
 * \brief 读取逻辑单元容量
 */
static int __read_capacity(struct usbh_ms_lu *p_lu,
                           uint32_t          *p_nblks,
                           uint32_t          *p_blk_size){
    int     ret;
    uint8_t cmd[10];

    memset(cmd, 0, 10);

    cmd[0] = __READ_CAPACITY;

    ret = usbh_ms_transport(p_lu,
                            cmd,
                            10,
                            p_lu->p_buf,
                            8,
                            USB_DIR_IN);
    if (ret == 8) {
        uint8_t *ptr = (uint8_t *)p_lu->p_buf;
        if (p_nblks) {
            *p_nblks = 1 + ((ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3]);
        }
        if (p_blk_size) {
            *p_blk_size = (ptr[4] << 24) | (ptr[5] << 16) | (ptr[6] << 8) | ptr[7];
        }
        ret = USB_OK;
    } else if (ret >= 0) {
        ret = -USB_EDATA;
    }

    return ret;
}

static int __mode_sense(struct usbh_ms_lu *p_lu,
                        usb_bool_t        *p_is_wp){

    int      ret;
    uint8_t  cmd[6];

    cmd[0] = __MODE_SENSE;
    cmd[1] = 0;
    cmd[2] = 0x1C;
    cmd[3] = 0;
    cmd[4] = 192;
    cmd[5] = 0;

    ret = usbh_ms_transport(p_lu,
                            cmd,
                            6,
                            p_lu->p_buf,
                            192,
                            USB_DIR_IN);
    if (ret > 2) {
        uint8_t  *ptr = (uint8_t *)p_lu->p_buf;
        if (p_is_wp) {
            if (ptr[2] & 0x80) {
                *p_is_wp = USB_TRUE;
            } else {
                *p_is_wp = USB_FALSE;
            }
        }
        return USB_OK;
    } else if (ret > 0) {
        ret = -USB_EDATA;
    }

    return ret;
}

static int __read10(struct usbh_ms_lu *p_lu,
                    uint32_t           blk_num,
                    uint32_t           n_blks,
                    void              *p_buf){
    uint8_t cmd[10];

    memset(cmd, 0, 10);

    cmd[0] = __READ_10;

    cmd[2] = (uint8_t)(blk_num >> 24);
    cmd[3] = (uint8_t)(blk_num >> 16);
    cmd[4] = (uint8_t)(blk_num >> 8);
    cmd[5] = (uint8_t)(blk_num >> 0);

    cmd[7] = (uint8_t)(n_blks >> 8);
    cmd[8] = (uint8_t)(n_blks >> 0);

    return usbh_ms_transport(p_lu,
                             cmd,
                             10,
                             p_buf,
                             n_blks * p_lu->blk_size,
                             USB_DIR_IN);
}

static int __write10(struct usbh_ms_lu *p_lu,
                     uint32_t           blk_num,
                     uint32_t           n_blks,
                     void              *p_data) {
    uint8_t  cmd[10];

    memset(cmd, 0, 10);
    cmd[0] = __WRITE_10;

    cmd[2] = (uint8_t)(blk_num >> 24);
    cmd[3] = (uint8_t)(blk_num >> 16);
    cmd[4] = (uint8_t)(blk_num >> 8);
    cmd[5] = (uint8_t)(blk_num >> 0);

    cmd[7] = (uint8_t)(n_blks >> 8);
    cmd[8] = (uint8_t)(n_blks >> 0);

    return usbh_ms_transport(p_lu,
                             cmd,
                             10,
                             p_data,
                             n_blks * p_lu->blk_size,
                             USB_DIR_OUT);
}

/**
 * \brief 初始化 USB大容量存储 SCSI 设备
 */
int usbh_ms_scsi_init(struct usbh_ms_lu *p_lu){
    int ret, retry;

    /* 查询逻辑单元信息*/
    ret = __inquiry(p_lu, p_lu->p_buf, 36);
    if (ret < 0) {
        __USB_ERR_INFO("scsi inquiry failed(%d)\r\n", ret);
        return ret;
    }

    /* 检查逻辑单元是否准备好*/
    ret = __unit_ready(p_lu);
    if (ret < 0) {
        __USB_ERR_INFO("scsi unit ready failed(%d)\r\n", ret);
        return ret;
    }

    ret = __request_sense(p_lu, p_lu->p_buf, 18);
    if (ret < 0) {
        __USB_ERR_INFO("scsi request sense failed(%d)\r\n", ret);
        return ret;
    }

    ret = __unit_ready(p_lu);
    if (ret < 0) {
        __USB_ERR_INFO("scsi unit ready failed(%d)\r\n", ret);
        return ret;
    }

    /* 读取逻辑分区容量*/
    retry = 3;
    do {
        ret = __read_capacity(p_lu, &p_lu->n_blks, &p_lu->blk_size);
        if (ret < 0) {
            __USB_ERR_INFO("scsi capacity read failed(%d)\r\n", ret);
            return ret;
        }
    } while ((ret != USB_OK) && (--retry > 0) && USBH_IS_DEV_INJECT(p_lu->p_ms->p_usb_fun->p_usb_dev));

    if (ret < 0) {
        return ret;
    }

    retry = 3;
    do {
        ret = __mode_sense(p_lu, &p_lu->is_wp);
        if (ret < 0) {
            __USB_ERR_INFO("scsi mode sense failed(%d)\r\n", ret);
            return ret;
        }
    } while ((ret != USB_OK) && (--retry > 0) && USBH_IS_DEV_INJECT(p_lu->p_ms->p_usb_fun->p_usb_dev));

    return USB_OK;
}

/**
 * \brief SCSI读函数
 */
int usbh_ms_scsi_read (struct usbh_ms_lu *p_lu,
                       uint32_t           blk_num,
                       uint32_t           n_blks,
                       void              *p_buf) {
    return __read10(p_lu, blk_num, n_blks, p_buf);
}

/**
 * \brief SCSI写函数
 */
int usbh_ms_scsi_write (struct usbh_ms_lu *p_lu,
                        uint32_t           blk_num,
                        uint32_t           n_blks,
                        void              *p_buf) {
    return __write10(p_lu, blk_num, n_blks, p_buf);
}

