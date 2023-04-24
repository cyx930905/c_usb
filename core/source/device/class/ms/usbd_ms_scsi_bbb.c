/*******************************************************************************
 *                                    ZLG
 *                         ----------------------------
 *                         innovating embedded platform
 *
 * Copyright (c) 2001-present Guangzhou ZHIYUAN Electronics Co., Ltd.
 * All rights reserved.
 *
 * Contact information:
 * web site:    https://www.zlg.cn
 *******************************************************************************/
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/device/class/ms/usbd_ms.h"

/*******************************************************************************
 * Static
 ******************************************************************************/
/* \brief USB 接口功能信息定义 */
static struct usbd_func_info __g_ms_func_info = {
        .intf_num        = USBD_MS_INTF_NUM,
        .alt_setting_num = USBD_MS_INTF_ALT_NUM,
        .class           = USB_CLASS_MASS_STORAGE,
        .sub_class       = USB_MS_SC_SCSI_TRANSPARENT,
        .protocol        = USB_MS_PRO_BBB,
        .p_intf_str      = "",
        .spec_desc_size  = 0,
};

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 寻找对应的逻辑分区
 */
static struct usbd_ms_lu *__lu_find(struct usbd_ms *p_ms,
                                    uint8_t         lu_num){
    struct usbd_ms_lu    *p_lu   = NULL;
    struct usb_list_node *p_node = NULL;

    usb_list_for_each_node(p_node, &p_ms->lu_list){
        p_lu = usb_container_of(p_node, struct usbd_ms_lu, node);

        if (p_lu->num == (int)lu_num) {
            return p_lu;
        }
    }
    return NULL;
}

/**
 * \brief USB 大容量存储设备获取命令
 */
static int __ms_scsi_bbb_cmd_get(struct usbd_ms *p_ms){
    int        ret;
    uint8_t   *p_buf = NULL;
    uint32_t   len_ret, tmp;
    usb_bool_t is_connect;

    is_connect = usbd_ms_is_setup(p_ms);
    if (is_connect == USB_FALSE) {
        return -USB_EPERM;
    }

    /* 获取命令块打包长度*/
    ret = usbd_ms_read(p_ms, USB_BBB_CBW_LEN, USB_WAIT_FOREVER, &len_ret);
    if (ret != USB_OK) {
        return ret;
    }
    p_buf = (uint8_t *)p_ms->p_buf;

    /* 获取命令块包签名*/
    tmp = USB_CPU_TO_LE32(usb_le32_load(&p_buf[0]));
    /* 检查是否合法*/
    if ((len_ret != USB_BBB_CBW_LEN) || (tmp != USB_BBB_CBW_SIG)) {
        /* bulk-only 规范(6.6.1) 必须停止输入管道直到下一次复位*/
        //todo
        usbd_ms_pipe_stall_set(p_ms, USB_DIR_IN, USB_TRUE);
        return -USB_EBADF;
    }

    /* 获取其他信息*/
    p_ms->info.info.tag  = USB_CPU_TO_LE32(usb_le32_load(&p_buf[4]));
    p_ms->info.info.dlen = USB_CPU_TO_LE32(usb_le32_load(&p_buf[8]));
    p_ms->info.info.dir  = p_buf[12];
    tmp                  = p_buf[13];
    p_ms->info.info.clen = p_buf[14];

    /* 检查命令包是否合法*/
    if ((tmp >= p_ms->n_lu) ||
        (p_ms->info.info.dir & ~USB_BBB_CBW_FLAG_IN) ||
        (p_ms->info.info.clen < 1) ||
        (p_ms->info.info.clen > 16)) {
        /* 停止管道*/
        usbd_ms_pipe_stall_set(p_ms, USB_DIR_IN, USB_TRUE);
        usbd_ms_pipe_stall_set(p_ms, USB_DIR_OUT, USB_TRUE);
        return -USB_EINVAL;
    }

    memcpy(p_ms->info.info.cb, &p_buf[15], p_ms->info.info.clen);
    /* 寻找逻辑分区*/
    p_ms->p_lu_select = __lu_find(p_ms, tmp);
    if (p_ms->p_lu_select == NULL) {
        return -USB_ENODEV;
    }

    return USB_OK;
}

/**
 * \brief 检查命令
 */
static usb_bool_t __cmd_check(struct usbd_ms *p_ms,
                              uint8_t         clen,
                              uint32_t        cdlen,
                              uint8_t         dir){
    p_ms->info.info.cdlen = cdlen;

    if ((p_ms->info.info.dlen < p_ms->info.info.cdlen) ||
        ((p_ms->info.info.cdlen) && (p_ms->info.info.dir != dir)) ||
        (p_ms->info.info.clen < clen)) {
        p_ms->info.info.sta = USB_BBB_CSW_STA_PHASE_ERROR;
        return USB_FALSE;
    }

    if (p_ms->p_lu_select) {
        if (p_ms->info.info.cb[0] != SCSI_CMD_REQUEST_SENSE) {
            /* 清除sense数据*/
            p_ms->p_lu_select->lu.lu.sd           = SCSI_SK_NO_SENSE;
            p_ms->p_lu_select->lu.lu.sd_info      = 0;
            p_ms->p_lu_select->lu.lu.is_inf_valid = USB_FALSE;
        }
    } else {
        /* 只有查询和请求命令允许没有逻辑单元*/
        if ((p_ms->info.info.cb[0] != SCSI_CMD_INQUIRY) &&
            (p_ms->info.info.cb[0] != SCSI_CMD_REQUEST_SENSE)) {
            p_ms->info.info.sta = USB_BBB_CSW_STA_FAIL;
            return USB_FALSE;
        }
    }

    return USB_TRUE;
}

/**
 * \brief 测试单元是否已经准备好
 */
static int __unity_ready_test(struct usbd_ms *p_ms){
    usb_bool_t is_valid;

    is_valid = __cmd_check(p_ms, 6, 0, USB_BBB_CBW_FLAG_OUT);
    if (is_valid == USB_FALSE) {
        return -USB_EILLEGAL;
    }
    return 0;
}

static int __request_sense(struct usbd_ms *p_ms){
    uint32_t    data, info;
    uint8_t     valid;
    uint8_t    *p_buf = (uint8_t*)p_ms->p_buf;
    usb_bool_t  is_valid;

    is_valid = __cmd_check(p_ms, 6, p_ms->info.info.cb[4], USB_BBB_CBW_FLAG_IN);
    if (is_valid == USB_FALSE) {
        return -USB_EILLEGAL;
    }

    if (p_ms->p_lu_select == NULL) {
        data  = SCSI_SK_LOGICAL_UNIT_NOT_SUPPORTED;
        info  = 0;
        valid = 0;
    } else {
        data  = p_ms->p_lu_select->lu.lu.sd;
        info  = p_ms->p_lu_select->lu.lu.sd_info;
        valid = p_ms->p_lu_select->lu.lu.is_inf_valid ? 0x80 : 0;

        p_ms->p_lu_select->lu.lu.sd           = SCSI_SK_NO_SENSE;
        p_ms->p_lu_select->lu.lu.sd_info      = 0;
        p_ms->p_lu_select->lu.lu.is_inf_valid = USB_FALSE;
    }

    memset(p_buf, 0, 18);

    /* valid, current error */
    p_buf[0] = valid | 0x70;
    p_buf[2] = SCSI_SK(data);

    /* sense information */
    usb_be32_stor(info, &p_buf[3]);

    /* additional sense length */
    p_buf[7]  = 18 - 8;
    p_buf[12] = SCSI_ASC(data);
    p_buf[13] = SCSI_ASCQ(data);

    return 18;
}

/**
 * \brief 查询命令
 */
static int __inquiry(struct usbd_ms *p_ms){
    uint8_t    *p_buf = (uint8_t *)p_ms->p_buf;
    usb_bool_t  is_valid;

    is_valid = __cmd_check(p_ms, 6, p_ms->info.info.cb[4], USB_BBB_CBW_FLAG_IN);
    if (is_valid == USB_FALSE) {
        return -USB_EILLEGAL;
    }

    if (p_ms->p_lu_select == NULL) {
        memset(p_buf, 0, 36);
        p_buf[0] = 0x7f;      /* 不支持，没有设备类型 */
        p_buf[4] = 31;
    } else {
        p_buf[0] = p_ms->p_lu_select->lu.lu.is_rdonly ? SCSI_DEV_TYPE_CDROM : SCSI_DEV_TYPE_DISK;
        p_buf[1] = p_ms->p_lu_select->lu.lu.is_rm ? 0x80 : 0;
        p_buf[2] = 0;
        p_buf[3] = 0x01;
        p_buf[4] = 31;        /* 额外的长度*/
        p_buf[5] = 0;
        p_buf[6] = 0;
        p_buf[7] = 0;
        p_buf += 8;
        memset(p_buf, 0, 28);
    }

    return 36;
}

/**
 * \brief 读取格式化容量
 */
static int __format_capacities_read(struct usbd_ms *p_ms){
    uint8_t    *p_buf = (uint8_t *)p_ms->p_buf;
    usb_bool_t  is_valid;

    is_valid = __cmd_check(p_ms, 10, usb_be16_load(&p_ms->info.info.cb[7]), USB_BBB_CBW_FLAG_IN);
    if (is_valid == USB_FALSE) {
        return -USB_EILLEGAL;
    }

    p_buf[0] = 0;
    p_buf[1] = 0;
    p_buf[2] = 0;
    p_buf[3] = 8;

    /* 最大逻辑块数量*/
    usb_be32_stor(p_ms->p_lu_select->n_blks, &p_buf[4]);
    /* 块大小*/
    usb_be32_stor(p_ms->p_lu_select->blk_size, &p_buf[8]);
    /* 当前容量*/
    p_buf[8] = 0x02;

    return 12;
}

/**
 * \brief 读取容量
 */
static int __capacity_read(struct usbd_ms *p_ms){
    uint32_t    temp;
    int         pmi;
    uint8_t    *p_buf = NULL;
    usb_bool_t  is_valid;

    is_valid = __cmd_check(p_ms, 10, 8, USB_BBB_CBW_FLAG_IN);
    if (is_valid == USB_FALSE) {
        return -USB_EILLEGAL;
    }

    pmi  = p_ms->info.info.cb[8];
    temp = usb_be32_load(&p_ms->info.info.cb[2]);

    /* Check the PMI and LBA fields */
    if (pmi > 1 || (pmi == 0 && temp != 0)) {
        p_ms->p_lu_select->lu.lu.sd = SCSI_SK_INVALID_FIELD_IN_CDB;
        p_ms->info.info.sta         = USB_BBB_CSW_STA_FAIL;
        return 0;
    }

    p_buf = (uint8_t *)p_ms->p_buf;

    /* 最大逻辑块数量*/
    usb_be32_stor(p_ms->p_lu_select->n_blks, p_buf);
    /* 块大小*/
    usb_be32_stor(p_ms->p_lu_select->blk_size, &p_buf[4]);

    return 8;
}

/**
 * \brief 读
 */
static int __read(struct usbd_ms *p_ms){
    uint32_t  lba, n_blks, n_blks_act, blk_rd, tmp;
    uint32_t  wr_len;
    int       ret;

    /* get the starting LBA and check it */
    if (p_ms->info.info.cb[0] == SCSI_CMD_READ_6) {
        lba = usb_be32_load(&p_ms->info.info.cb[0]) & 0x1FFFFF;
    } else {
        lba = usb_be32_load(&p_ms->info.info.cb[2]);

        /* we allow DPO (Disable Page Out = don't save data in the
         * cache) and FUA (Force Unit Access = don't read from the
         * cache), but we don't implement them. */
         if ((p_ms->info.info.cb[1] & ~0x18) != 0) {
             p_ms->p_lu_select->lu.lu.sd = SCSI_SK_INVALID_FIELD_IN_CDB;
             p_ms->info.info.sta         = USB_BBB_CSW_STA_FAIL;
             return 0;
         }
    }

    if (lba >= p_ms->p_lu_select->n_blks) {
        p_ms->p_lu_select->lu.lu.sd = SCSI_SK_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
        p_ms->info.info.sta         = USB_BBB_CSW_STA_FAIL;
        return 0;
    }

    if (p_ms->info.info.cdlen == 0) {
        return -USB_EILLEGAL;
    }

    /* 要读的块数*/
    n_blks     = p_ms->info.info.cdlen / p_ms->p_lu_select->blk_size;
    /* 实际要读的块数*/
    n_blks_act = p_ms->buf_size / p_ms->p_lu_select->blk_size;

    for (;;) {
        tmp = min(n_blks, n_blks_act);
        /* 检查要读的块数量有没有超过最大数量*/
        tmp = min(tmp, (p_ms->p_lu_select->n_blks - lba));

        /* 无数据*/
        if (tmp == 0) {
            p_ms->p_lu_select->lu.lu.sd           = SCSI_SK_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
            p_ms->p_lu_select->lu.lu.sd_info      = lba;
            p_ms->p_lu_select->lu.lu.is_inf_valid = USB_TRUE;
            p_ms->info.info.sta                   = USB_BBB_CSW_STA_FAIL;
            return 0;
        }

        /* 从媒体中读取数据*/
        ret = p_ms->p_opts->p_fn_blk_read(p_ms->p_lu_select, p_ms->p_buf, lba, tmp, &blk_rd);
        if (ret != USB_OK) {
            blk_rd = 0;
            __USB_ERR_INFO("usb mass storage device read failed(%d)\r\n", ret);
        }

        n_blks               -=  blk_rd;
        lba                  +=  blk_rd;
        p_ms->info.info.dlen -= (blk_rd * p_ms->p_lu_select->blk_size);

        /* 如果有错误产生，上报它和它的位置*/
        if (blk_rd < tmp) {
            p_ms->p_lu_select->lu.lu.sd           = SCSI_SK_UNRECOVERED_READ_ERROR;
            p_ms->p_lu_select->lu.lu.sd_info      = lba;
            p_ms->p_lu_select->lu.lu.is_inf_valid = USB_TRUE;
            p_ms->info.info.sta                   = USB_BBB_CSW_STA_FAIL;
            return 0;
        }

        ret = usbd_ms_write(p_ms,
                            blk_rd * p_ms->p_lu_select->blk_size,
                            USBD_MS_WAIT_TIMEOUT,
                           &wr_len);

        if (ret != USB_OK) {
            return ret;
        } else if (wr_len != (blk_rd * p_ms->p_lu_select->blk_size)) {
            return -USB_EWRITE;
        }
        if (n_blks == 0) {
            break;
        }
    };

    return 0;
}

/**
 * \brief 读
 */
static int __read6(struct usbd_ms *p_ms){
    usb_bool_t is_valid;
    uint32_t   tmp;

    tmp = p_ms->info.info.cb[4] ? p_ms->info.info.cb[4] : 256;
    if (p_ms->p_lu_select) {
        tmp *= p_ms->p_lu_select->blk_size;
    }

    is_valid = __cmd_check(p_ms, 6, tmp, USB_BBB_CBW_FLAG_IN);
    if (is_valid == USB_FALSE) {
        return -USB_EILLEGAL;
    }
    return __read(p_ms);
}

/**
 * \brief 读
 */
static int __read10(struct usbd_ms *p_ms){
    usb_bool_t is_valid;
    uint32_t   tmp;

    tmp = (uint32_t)usb_be16_load(&p_ms->info.info.cb[7]);
    if (p_ms->p_lu_select) {
        tmp *= p_ms->p_lu_select->blk_size;
    }

    is_valid = __cmd_check(p_ms, 10, tmp, USB_BBB_CBW_FLAG_IN);
    if (is_valid == USB_FALSE) {
        return -USB_EILLEGAL;
    }
    return __read(p_ms);
}

/**
 * \brief 写
 */
static int __write(struct usbd_ms *p_ms){
    struct usbd_ms_lu *p_lu = p_ms->p_lu_select;
    uint32_t           uoff, lba, nout, n_blks, n_blks_act;
    uint32_t           rd_len, blk_wr, blk_wr_ret;
    int                ret;

    /* 只读*/
    if (p_lu->lu.lu.is_rdonly == USB_TRUE) {
    	p_lu->lu.lu.sd      = SCSI_SK_WRITE_PROTECTED;
        p_ms->info.info.sta = USB_BBB_CSW_STA_FAIL;
        return -USB_EPERM;
    }

    /* get LBA and check it */
    if (p_ms->info.info.cb[0] == SCSI_CMD_WRITE_6) {
        lba = usb_be32_load(&p_ms->info.info.cb[0]) & 0x1FFFFF;
    } else {
        lba = usb_be32_load(&p_ms->info.info.cb[2]);

        /* we allow DPO (Disable Page Out = don't save data in the
         * cache) and FUA (Force Unit Access = write directly to the
         * medium).  We don't implement DPO; we implement FUA by
         * performing synchronous output. */
        if (p_ms->info.info.cb[1] & ~0x18) {
        	p_lu->lu.lu.sd      = SCSI_SK_INVALID_FIELD_IN_CDB;
            p_ms->info.info.sta = USB_BBB_CSW_STA_FAIL;
            return -USB_EILLEGAL;
        }
        if (p_ms->info.info.cb[1] & 0x08) {  /* FUA */
            //SYNC PUSH TODO
        }
    }

    if (lba >= p_lu->n_blks) {
        /* 超过范围 */
        p_lu->lu.lu.sd      = SCSI_SK_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
        p_ms->info.info.sta = USB_BBB_CSW_STA_FAIL;
        return -USB_EILLEGAL;
    }

    if (p_ms->info.info.cdlen == 0) {
        /* 无数据 */
        return -USB_EPERM;
    }

    n_blks     = p_ms->info.info.cdlen / p_lu->blk_size;
    n_blks_act = p_ms->buf_size / p_lu->blk_size;
    uoff       = lba;

    while (n_blks > 0) {
        if (uoff >= p_lu->n_blks) {
            p_lu->lu.lu.sd           = SCSI_SK_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
            p_lu->lu.lu.sd_info      = uoff;
            p_lu->lu.lu.is_inf_valid = USB_TRUE;
            p_ms->info.info.sta      = USB_BBB_CSW_STA_FAIL;
            return -USB_EILLEGAL;
        }

        nout    = min(n_blks, n_blks_act);
        uoff   += nout;
        n_blks -= nout;

        ret = usbd_ms_read(p_ms, nout * p_lu->blk_size, USBD_MS_WAIT_TIMEOUT, &rd_len);
        if (ret != USB_OK) {
            p_lu->lu.lu.sd           = SCSI_SK_COMMUNICATION_FAILURE;
            p_lu->lu.lu.sd_info      = lba;
            p_lu->lu.lu.is_inf_valid = USB_TRUE;
            p_ms->info.info.sta      = USB_BBB_CSW_STA_FAIL;
            return ret;
        } else if (rd_len != (nout * p_lu->blk_size)) {
            return -USB_EREAD;
        }
        blk_wr = nout;

        ret = p_ms->p_opts->p_fn_blk_write(p_lu, p_ms->p_buf, lba, blk_wr, &blk_wr_ret);
        if (ret != USB_OK) {
            __USB_ERR_INFO("usb mass storage device write failed(%d)\r\n", ret);
            return ret;
        }
        if (blk_wr_ret != blk_wr) {
             p_lu->lu.lu.sd           = SCSI_SK_WRITE_ERROR;
             p_lu->lu.lu.sd_info      = lba;
             p_lu->lu.lu.is_inf_valid = USB_TRUE;
             p_ms->info.info.sta      = USB_BBB_CSW_STA_FAIL;
             return -USB_EWRITE;
        }

        lba                  +=  blk_wr_ret;
        p_ms->info.info.dlen -= (blk_wr_ret * p_lu->blk_size);
    }
    return 0;
}

/**
 * \brief 写
 */
static int __write10(struct usbd_ms *p_ms){
    usb_bool_t is_valid;
    uint32_t   tmp;

    tmp = (uint32_t)usb_be16_load(&p_ms->info.info.cb[7]);
    if (p_ms->p_lu_select) {
        tmp *= p_ms->p_lu_select->blk_size;
    }

    is_valid = __cmd_check(p_ms, 10, tmp, USB_BBB_CBW_FLAG_OUT);
    if (is_valid == USB_FALSE) {
        return -USB_EILLEGAL;
    }
    return __write(p_ms);
}

static void __store_cdrom_addr(uint8_t *p_dest,
                               int      msf,
                               uint32_t addr){
    if (msf) {
        /* Convert to Minutes-Seconds-Frames */
        addr    >>= 2;          /* Convert to 2048-byte frames */
        addr     += 2*75;       /* Lead-in occupies 2 seconds */
        p_dest[3] = addr % 75;  /* Frames */
        addr     /= 75;
        p_dest[2] = addr % 60;  /* Seconds */
        addr     /= 60;
        p_dest[1] = addr;       /* Minutes */
        p_dest[0] = 0;          /* Reserved */
    } else {
        /* Absolute sector */
        usb_be32_stor(addr, p_dest);
    }
}

static int __prevent_allow(struct usbd_ms *p_ms){
    int        prevent;
    usb_bool_t is_valid;

    is_valid = __cmd_check(p_ms, 6, 0, USB_BBB_CBW_FLAG_OUT);
    if (is_valid == USB_FALSE) {
        return -USB_EILLEGAL;
    }

    if (p_ms->p_lu_select->lu.lu.is_rm == USB_FALSE) {
        p_ms->p_lu_select->lu.lu.sd = SCSI_SK_INVALID_COMMAND;
        p_ms->info.info.sta         = USB_BBB_CSW_STA_FAIL;
        return 0;
    }

    prevent = p_ms->info.info.cb[4] & 0x01;
    if ((p_ms->info.info.cb[4] & ~0x01) != 0) {   /* mask away Prevent */
        p_ms->p_lu_select->lu.lu.sd = SCSI_SK_INVALID_FIELD_IN_CDB;
        p_ms->info.info.sta         = USB_BBB_CSW_STA_FAIL;
        return 0;
    }

    if (p_ms->p_lu_select->lu.lu.is_pv && !prevent) {
        /* todo */
    }
    p_ms->p_lu_select->lu.lu.is_pv = prevent;

    return 0;
}

static int __mode_sense(struct usbd_ms *p_ms){
    uint8_t *ptr0   = (uint8_t *)p_ms->p_buf;
    uint8_t *ptr    = (uint8_t *)p_ms->p_buf;
    int      pc, page_code;
    int      changeable_values, all_pages;
    int      valid_page = 0;
    int      len, limit;

    /* Mask away DBD */
    if ((p_ms->info.info.cb[1] & ~0x08) != 0) {
        p_ms->p_lu_select->lu.lu.sd = SCSI_SK_INVALID_FIELD_IN_CDB;
        p_ms->info.info.sta         = USB_BBB_CSW_STA_FAIL;
        return 0;
    }

    pc        = p_ms->info.info.cb[2] >> 6;
    page_code = p_ms->info.info.cb[2] & 0x3f;
    if (pc == 3) {
        p_ms->p_lu_select->lu.lu.sd = SCSI_SK_SAVING_PARAMETERS_NOT_SUPPORTED;
        p_ms->info.info.sta         = USB_BBB_CSW_STA_FAIL;
        return 0;
    }

    changeable_values = (pc == 1);
    all_pages         = (page_code == 0x3f);

    /* Write the mode parameter header.  Fixed values are: default
     * medium type, no cache control (DPOFUA), and no block descriptors.
     * The only variable value is the WriteProtect bit.  We will fill in
     * the mode data length later. */
    memset(ptr, 0, 8);
    if (p_ms->info.info.cb[0] == SCSI_CMD_MODE_SENSE_6) {
        ptr[2] = (p_ms->p_lu_select->lu.lu.is_rdonly) ? 0x80 : 0x00;     /* WP, DPOFUA */
        ptr   += 4;
        limit  = 255;
    } else {
        /* SC_MODE_SENSE_10 */
        ptr[3] = (p_ms->p_lu_select->lu.lu.is_rdonly) ? 0x80 : 0x00;     /* WP, DPOFUA */
        ptr += 8;
        limit = 65535;      /* Should really be FSG_BUFLEN */
    }

    /* The mode pages, in numerical order.  The only page we support
     * is the Caching page. */
    if (page_code == 0x08 || all_pages) {
        valid_page = 1;
        ptr[0] = 0x08;      /* Page code */
        ptr[1] = 10;        /* Page length */
        memset(ptr + 2, 0, 10); /* None of the fields are changeable */

        if (!changeable_values) {
            ptr[2] = 0x04;  /* Write cache enable, */
                            /* Read cache not disabled */
                            /* No cache retention priorities */
            usb_be16_stor(0xffff, &ptr[4]);
                    /* Don't disable prefetch */
                    /* Minimum prefetch = 0 */
            usb_be16_stor(0xffff, &ptr[8]);
                    /* Maximum prefetch */
            usb_be16_stor(0xffff, &ptr[10]);
                    /* Maximum prefetch ceiling */
        }
        ptr += 12;
    }

    /* Check that a valid page was requested and the mode data length
     * isn't too long. */
    len = ptr - ptr0;
    if (!valid_page || len > limit) {
        p_ms->p_lu_select->lu.lu.sd = SCSI_SK_INVALID_FIELD_IN_CDB;
        p_ms->info.info.sta         = USB_BBB_CSW_STA_FAIL;
        return 0;
    }

    /*  Store the mode data length */
    if (p_ms->info.info.cb[0] == SCSI_CMD_MODE_SENSE_6) {
        ptr0[0] = len - 1;
    } else {
        usb_be16_stor(len - 2, ptr0);
    }
    return len;
}

static int __mode_sense6(struct usbd_ms *p_ms){
    usb_bool_t is_valid;

    is_valid = __cmd_check(p_ms, 6, p_ms->info.info.cb[4], USB_BBB_CBW_FLAG_IN);
    if (is_valid == USB_FALSE) {
        return -USB_EILLEGAL;
    }
    return __mode_sense(p_ms);
}

static int __mode_sense10(struct usbd_ms *p_ms){
    usb_bool_t is_valid;

    is_valid = __cmd_check(p_ms, 10, usb_be16_load(&p_ms->info.info.cb[7]), USB_BBB_CBW_FLAG_IN);
    if (is_valid == USB_FALSE) {
        return -USB_EILLEGAL;
    }
    return __mode_sense(p_ms);
}

static int __start_stop(struct usbd_ms  *p_ms){
    struct usbd_ms_lu *p_lu = p_ms->p_lu_select;
    int                loej, start;
    usb_bool_t         is_valid;

    is_valid = __cmd_check(p_ms, 6, 0, USB_BBB_CBW_FLAG_OUT);
    if (is_valid == USB_FALSE) {
        return -USB_EILLEGAL;
    }

    if (p_lu->lu.lu.is_rm == USB_FALSE) {
        p_lu->lu.lu.sd      = SCSI_SK_INVALID_COMMAND;
        p_ms->info.info.sta = USB_BBB_CSW_STA_FAIL;
        return 0;
    }

    if ((p_ms->info.info.cb[1] & ~0x01) != 0 ||      /* Mask away Immed */
        (p_ms->info.info.cb[4] & ~0x03) != 0) {      /* Mask LoEj, Start */
        p_lu->lu.lu.sd = SCSI_SK_INVALID_FIELD_IN_CDB;
        p_ms->info.info.sta = USB_BBB_CSW_STA_FAIL;
        return 0;
    }

    loej  = p_ms->info.info.cb[4] & 0x02;
    start = p_ms->info.info.cb[4] & 0x01;

    if (!start) {
        if (loej) { /* Simulate an unload/eject */
            //todo
        }
    } else {
        /* Our emulation doesn't support mounting; the medium is
         * available for use as soon as it is loaded. */
        //todo
    }
    return 0;
}

static int __toc_read(struct usbd_ms *p_ms){
    struct usbd_ms_lu *p_lu        = p_ms->p_lu_select;
    int                msf         = p_ms->info.info.cb[1] & 0x02;
    int                start_track = p_ms->info.info.cb[6];
    uint8_t           *p_buf       = (uint8_t *)p_ms->p_buf;
    usb_bool_t         is_valid;

    if ((p_ms->p_lu_select == NULL) || (p_ms->p_lu_select->lu.lu.is_rdonly == USB_FALSE)) {
        return -USB_EILLEGAL;
    }

    is_valid = __cmd_check(p_ms, 10, usb_be16_load(&p_ms->info.info.cb[7]), USB_BBB_CBW_FLAG_IN);
    if (is_valid == USB_FALSE) {
        return -USB_EILLEGAL;
    }

    if ((p_ms->info.info.cb[1] & ~0x02) != 0 || start_track > 1) {
        p_lu->lu.lu.sd      = SCSI_SK_INVALID_FIELD_IN_CDB;
        p_ms->info.info.sta = USB_BBB_CSW_STA_FAIL;
        return USB_OK;
    }

    memset(p_buf, 0, 20);
    p_buf[1] = (20-2);        /* TOC data length */
    p_buf[2] = 1;             /* First track number */
    p_buf[3] = 1;             /* Last track number */
    p_buf[5] = 0x16;          /* Data track, copying allowed */
    p_buf[6] = 0x01;          /* Only track is number 1 */
    __store_cdrom_addr(&p_buf[8], msf, 0);

    p_buf[13] = 0x16;         /* Lead-out track is data */
    p_buf[14] = 0xAA;         /* Lead-out track number */
    __store_cdrom_addr(&p_buf[16], msf, p_lu->n_blks);

    return 20;
}

static int __synchronize_cache(struct usbd_ms *p_ms){
    usb_bool_t is_valid;

    is_valid = __cmd_check(p_ms, 10, 0, USB_BBB_CBW_FLAG_OUT);
    if (is_valid == USB_FALSE) {
        return -USB_EILLEGAL;
    }

    //todo
    return 0;
}

static int __header_read(struct usbd_ms *p_ms){
    struct usbd_ms_lu *p_lu  = p_ms->p_lu_select;
    int                msf   = p_ms->info.info.cb[1] & 0x02;
    uint32_t           lba   = usb_be32_load(&p_ms->info.info.cb[2]);
    uint8_t           *p_buf = (uint8_t *)p_ms->p_buf;
    usb_bool_t         is_valid;

    is_valid = __cmd_check(p_ms, 10, usb_be16_load(&p_ms->info.info.cb[7]), USB_BBB_CBW_FLAG_IN);
    if (is_valid == USB_FALSE) {
        return -USB_EILLEGAL;
    }

    if (p_ms->info.info.cb[1] & ~0x02) {     /* Mask away MSF */
        p_lu->lu.lu.sd = SCSI_SK_INVALID_FIELD_IN_CDB;
        return -USB_EILLEGAL;
    }
    if (lba >= p_lu->n_blks) {
        p_lu->lu.lu.sd      = SCSI_SK_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
        p_ms->info.info.sta = USB_BBB_CSW_STA_FAIL;
        return USB_OK;
    }

    memset(p_buf, 0, 8);
    p_buf[0] = 0x01;      /* 2048 bytes of user data, rest is EC */

    __store_cdrom_addr(&p_buf[4], msf, lba);

    return 8;
}

static int __unknown_cmd(struct usbd_ms *p_ms){
    usb_bool_t is_valid;

    is_valid = __cmd_check(p_ms, p_ms->info.info.clen, 0, USB_BBB_CBW_FLAG_OUT);
    if (is_valid == USB_FALSE) {
        return -USB_EILLEGAL;
    }
    p_ms->p_lu_select->lu.lu.sd = SCSI_SK_INVALID_COMMAND;
    p_ms->info.info.sta         = USB_BBB_CSW_STA_FAIL;

    return 0;
}

/* \brief USB 大容量存储设备 SCSI 命令信息表*/
static struct usbd_ms_scsi_cmd_info __g_cmd_info_tab[] = {
        {SCSI_CMD_TEST_UNIT_READY,              __unity_ready_test},
        {SCSI_CMD_REQUEST_SENSE,                __request_sense},
        {SCSI_CMD_INQUIRY,                      __inquiry},
        {SCSI_CMD_READ_FORMAT_CAPACITIES,       __format_capacities_read},
        {SCSI_CMD_READ_CAPACITY,                __capacity_read},
        {SCSI_CMD_READ_6,                       __read6},
        {SCSI_CMD_READ_10,                      __read10},
        {SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL, __prevent_allow},
        {SCSI_CMD_MODE_SENSE_6,                 __mode_sense6},
        {SCSI_CMD_MODE_SENSE_10,                __mode_sense10},
        {SCSI_CMD_WRITE_10,                     __write10},
        {SCSI_CMD_START_STOP_UNIT,              __start_stop},
        {SCSI_CMD_READ_TOC,                     __toc_read},
        {SCSI_CMD_SYNCHRONIZE_CACHE,            __synchronize_cache},
        {SCSI_CMD_READ_HEADER,                  __header_read},
};

/**
 * \brief 通过命令获取命令信息
 */
static struct usbd_ms_scsi_cmd_info *__cmd_info_get(uint8_t cmd){
    for (uint32_t i = 0; i < USB_NELEMENTS(__g_cmd_info_tab); i++) {
        if (__g_cmd_info_tab[i].cmd == cmd) {
            return &__g_cmd_info_tab[i];
        }
    }
    return NULL;
}

#if 0
/**
 * \brief USB 大容量存储设备执行命令
 */
static int __ms_scsi_bbb_cmd_exec(struct usbd_ms *p_ms){
    int        ret = USB_OK;
    uint32_t   tmp, wr_len;
    usb_bool_t is_connect, is_valid;

    is_connect = usbd_ms_is_setup(p_ms);
    if (is_connect == USB_FALSE) {
        return -USB_EPERM;
    }

    p_ms->info.info.sta = USB_BBB_CSW_STA_PASS;

    switch (p_ms->info.info.cb[0]) {
        /* 检查单元是否准备好*/
        case SCSI_CMD_TEST_UNIT_READY:
            is_valid = __cmd_check(p_ms, 6, 0, 0);
            if (is_valid == USB_FALSE) {
                return -USB_EILLEGAL;
            }
            break;
        case SCSI_CMD_REQUEST_SENSE:
            is_valid = __cmd_check(p_ms, 6, p_ms->info.info.cb[4], USB_BBB_CBW_FLAG_IN);
            if (is_valid == USB_TRUE) {
                ret = __request_sense(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        /* 查询命令*/
        case SCSI_CMD_INQUIRY:
            is_valid = __cmd_check(p_ms, 6, p_ms->info.info.cb[4], USB_BBB_CBW_FLAG_IN);
            if (is_valid == USB_TRUE) {
                ret = __inquiry(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        /* 读取格式容量*/
        case SCSI_CMD_READ_FORMAT_CAPACITIES:
            is_valid = __cmd_check(p_ms, 10, usb_be16_load(&p_ms->info.info.cb[7]), USB_BBB_CBW_FLAG_IN);
            if (is_valid == USB_TRUE) {
                ret = __format_capacities_read(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        /* 读取容量*/
        case SCSI_CMD_READ_CAPACITY:
            is_valid = __cmd_check(p_ms, 10, 8, USB_BBB_CBW_FLAG_IN);
            if (is_valid == USB_TRUE) {
                ret = __capacity_read(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        case SCSI_CMD_READ_6:
            tmp = p_ms->info.info.cb[4] ? p_ms->info.info.cb[4] : 256;
            if (p_ms->p_lu_select) {
                tmp *= p_ms->p_lu_select->blk_size;
            }
            is_valid = __cmd_check(p_ms, 6, tmp, USB_BBB_CBW_FLAG_IN);
            if (is_valid == USB_TRUE) {
                ret = __read(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        case SCSI_CMD_READ_10:
            tmp = (uint32_t)usb_be16_load(&p_ms->info.info.cb[7]);
            if (p_ms->p_lu_select) {
                tmp *= p_ms->p_lu_select->blk_size;
            }
            is_valid = __cmd_check(p_ms, 10, tmp, USB_BBB_CBW_FLAG_IN);
            if (is_valid == USB_TRUE) {
                ret = __read(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        case SCSI_CMD_READ_12:
            tmp = (uint32_t)usb_be32_load(&p_ms->info.info.cb[6]);
            if (p_ms->p_lu_select) {
                tmp *= p_ms->p_lu_select->blk_size;
            }
            is_valid = __cmd_check(p_ms, 12, tmp, USB_BBB_CBW_FLAG_IN);
            if (is_valid == USB_TRUE) {
                ret = __read(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
            is_valid = __cmd_check(p_ms, 6, 0, 0);
            if (is_valid == USB_TRUE) {
                ret = __prevent_allow(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        case SCSI_CMD_MODE_SENSE_6:
            is_valid = __cmd_check(p_ms, 6, p_ms->info.info.cb[4], USB_BBB_CBW_FLAG_IN);
            if (is_valid == USB_TRUE) {
                ret = __mode_sense(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        case SCSI_CMD_MODE_SENSE_10:
            is_valid = __cmd_check(p_ms, 10, usb_be16_load(&p_ms->info.info.cb[7]), USB_BBB_CBW_FLAG_IN);
            if (is_valid == USB_TRUE) {
                ret = __mode_sense(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        case SCSI_CMD_WRITE_6:
            tmp = p_ms->info.info.cb[4] ? p_ms->info.info.cb[4] : 256;
            if (p_ms->p_lu_select) {
                tmp *= p_ms->p_lu_select->blk_size;
            }
            is_valid = __cmd_check(p_ms, 6, tmp, USB_BBB_CBW_FLAG_OUT);
            if (is_valid == USB_TRUE) {
                ret = __write(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        case SCSI_CMD_WRITE_10:
            tmp = (uint32_t)usb_be16_load(&p_ms->info.info.cb[7]);
            if (p_ms->p_lu_select) {
                tmp *= p_ms->p_lu_select->blk_size;
            }
            is_valid = __cmd_check(p_ms, 10, tmp, USB_BBB_CBW_FLAG_OUT);
            if (is_valid == USB_TRUE) {
                ret = __write(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        case SCSI_CMD_WRITE_12:
            tmp = (uint32_t)usb_be32_load(&p_ms->info.info.cb[6]);
            if (p_ms->p_lu_select) {
                tmp *= p_ms->p_lu_select->blk_size;
            }
            is_valid = __cmd_check(p_ms, 12, tmp, USB_BBB_CBW_FLAG_OUT);
            if (is_valid == USB_TRUE) {
                ret = __write(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        case SCSI_CMD_START_STOP_UNIT:
            is_valid = __cmd_check(p_ms, 6, 0, 0);
            if (is_valid == USB_TRUE) {
                ret = __start_stop(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        case SCSI_CMD_READ_TOC:
            if ((p_ms->p_lu_select == NULL) || (p_ms->p_lu_select->lu.lu.is_rdonly == USB_FALSE)) {
                goto __unknown;
            }
            is_valid = __cmd_check(p_ms, 10, usb_be16_load(&p_ms->info.info.cb[7]), USB_BBB_CBW_FLAG_IN);
            if (is_valid == USB_TRUE) {
                ret = __toc_read(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        case SCSI_CMD_SYNCHRONIZE_CACHE:
            is_valid = __cmd_check(p_ms, 10, 0, 0);
            if (is_valid == USB_TRUE) {
                ret = __synchronize_cache(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        case SCSI_CMD_READ_HEADER:
            is_valid = __cmd_check(p_ms, 10, usb_be16_load(&p_ms->info.info.cb[7]), USB_BBB_CBW_FLAG_IN);
            if (is_valid == USB_TRUE) {
                ret = __header_read(p_ms);
            } else {
                return -USB_EILLEGAL;
            }
            break;
        default:
__unknown:
            is_valid = __cmd_check(p_ms, p_ms->info.info.clen, 0, 0);
            if (is_valid == USB_TRUE) {
                p_ms->p_lu_select->lu.lu.sd = SCSI_SK_INVALID_COMMAND;
                p_ms->info.info.sta         = USB_BBB_CSW_STA_FAIL;
            }
            break;
            //return -USB_EILLEGAL;
    }

    if (ret < 0) {
        return ret;
    }

    /* 完成数据阶段*/
    if (p_ms->info.info.dlen) {
        if (p_ms->info.info.dir & USB_BBB_CBW_FLAG_IN) {
            /* 只发送数据到主机*/
            tmp = min(p_ms->info.info.dlen, min(p_ms->info.info.cdlen, (uint32_t)ret));
            ret = usbd_ms_write(p_ms, tmp, USBD_MS_WAIT_TIMEOUT, &wr_len);
            if (ret != USB_OK) {
                return ret;
            }
            if (wr_len < tmp) {
                return -USB_EWRITE;
            }
            p_ms->info.info.dlen -= tmp;
        } else {
            usbd_ms_pipe_stall(p_ms, USB_DIR_OUT);
        }
    }

    return USB_OK;
}
#else
/**
 * \brief USB 大容量存储设备执行命令
 */
static int __ms_scsi_bbb_cmd_exec(struct usbd_ms *p_ms){
    int                           ret = USB_OK;
    uint32_t                      tmp, wr_len;
    usb_bool_t                    is_connect;
    struct usbd_ms_scsi_cmd_info *p_cmd_info = NULL;

    is_connect = usbd_ms_is_setup(p_ms);
    if (is_connect == USB_FALSE) {
        return -USB_EPERM;
    }

    p_ms->info.info.sta = USB_BBB_CSW_STA_PASS;

    p_cmd_info = __cmd_info_get(p_ms->info.info.cb[0]);

    __USB_INFO("scsi cmd: %x\r\n", p_ms->info.info.cb[0]);

    if (p_cmd_info != NULL) {
        ret = p_cmd_info->p_fn_handle(p_ms);
    } else {
        ret = __unknown_cmd(p_ms);
    }

    if (ret < 0) {
        return ret;
    }

    /* 完成数据阶段*/
    if (p_ms->info.info.dlen) {
        if (p_ms->info.info.dir & USB_BBB_CBW_FLAG_IN) {
            /* 只发送数据到主机*/
            tmp = min(p_ms->info.info.dlen, min(p_ms->info.info.cdlen, (uint32_t)ret));
            ret = usbd_ms_write(p_ms, tmp, USBD_MS_WAIT_TIMEOUT, &wr_len);
            if (ret != USB_OK) {
                return ret;
            }
            if (wr_len < tmp) {
                return -USB_EWRITE;
            }
            p_ms->info.info.dlen -= tmp;
        } else {
            usbd_ms_pipe_stall_set(p_ms, USB_DIR_OUT, USB_TRUE);
            usb_mdelay(10);
            usbd_ms_pipe_stall_set(p_ms, USB_DIR_OUT, USB_FALSE);
        }
    }
    return USB_OK;
}

#endif

/**
 * \brief 发送状态
 */
static int __ms_scsi_bbb_cmd_sta(struct usbd_ms *p_ms){
    int       ret;
    uint8_t  *p_buf = (uint8_t *)p_ms->p_buf;
    uint32_t  wr_len;

    usb_le32_stor(USB_BBB_CSW_SIG, &p_buf[0]);
    usb_le32_stor(p_ms->info.info.tag, &p_buf[4]);
    usb_le32_stor(p_ms->info.info.dlen, &p_buf[8]);

    p_buf[12] = p_ms->info.info.sta;

    ret = usbd_ms_write(p_ms, USB_BBB_CSW_LEN, USBD_MS_WAIT_TIMEOUT, &wr_len);
    if (ret != USB_OK) {
        return ret;
    } else if (wr_len != USB_BBB_CSW_LEN) {
        return -USB_EWRITE;
    }
    return USB_OK;
}

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
                            struct usbd_ms       **p_ms_ret){
    return usbd_ms_create(p_dc,
                          p_name,
                          dev_desc,
                        __g_ms_func_info,
                          buf_size,
                          p_ms_ret);
}

/**
 * \brief 销毁 USB 从机大容量存储设备（SCSI，BBB）
 *
 * \param[in] p_ms 要销毁的 USB 从机大容量存储设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_scsi_bbb_destroy(struct usbd_ms *p_ms){
    return usbd_ms_destroy(p_ms);
}

/**
 * \brief USB 从机大容量存储设备处理（SCSI，BBB）
 *
 * \param[in] p_ms 相关的 USB 从机大容量存储设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_ms_scsi_bbb_process(struct usbd_ms *p_ms){
    int ret;
#if USB_OS_EN
    int ret_tmp;
#endif

    if ((p_ms == NULL) ||
            (p_ms->p_opts == NULL) ||
            (p_ms->p_opts->p_fn_blk_read == NULL) ||
            (p_ms->p_opts->p_fn_blk_write == NULL)) {
        return -USB_EINVAL;
    }

    if (p_ms->is_max_lun_get == USB_FALSE) {
        return -USB_EPERM;
    }

#if USB_OS_EN
    ret = usb_mutex_lock(p_ms->p_lock, USBD_MS_MUTEX_TIMEOUT);
    if (ret != USB_OK) {
        __USB_ERR_TRACE(MutexLockErr, "(%d)\r\n", ret);
        return ret;
    }
#endif

    /* 获取命令*/
    ret = __ms_scsi_bbb_cmd_get(p_ms);
    if (ret != USB_OK) {
        goto __exit;
    }
    /* 执行命令*/
    ret = __ms_scsi_bbb_cmd_exec(p_ms);
    if (ret != USB_OK) {
        goto __exit;
    }
    ret = __ms_scsi_bbb_cmd_sta(p_ms);
__exit:
#if USB_OS_EN
    ret_tmp = usb_mutex_unlock(p_ms->p_lock);
    if (ret_tmp != USB_OK) {
        __USB_ERR_TRACE(MutexUnLockErr, "(%d)\r\n", ret_tmp);
        return ret_tmp;
    }
#endif
    return ret;
}
