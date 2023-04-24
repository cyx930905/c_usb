#ifndef __USB_MS_SPECS_H
#define __USB_MS_SPECS_H
#include <stdio.h>
#include <stdint.h>


/* \brief USB 大容量存储设备子类代码*/
#define USB_MS_SC_SCSI_NO_REPORTED        0x00
#define USB_MS_SC_RBC                     0x01
#define USB_MS_SC_MMC5                    0x02
#define USB_MS_SC_OBSOLETE_QIC157         0x03
#define USB_MS_SC_UFI                     0x04
#define USB_MS_SC_OBSOLETE_SFF8070I       0x05
#define USB_MS_SC_SCSI_TRANSPARENT        0x06
#define USB_MS_SC_LSD_FS                  0x07
#define USB_MS_SC_IEEE1667                0x08
#define USB_MS_SC_RAMD                    0x13    /* Ram 磁盘设备*/
#define USB_MS_SC_VENDOR_SPECIFIC         0xFF

/* \brief USB 大容量存储设备协议代码*/
#define USB_MS_PRO_CBI_CCI                0x00   /* CBI(Control/Bulk/Interrupt) 传输，CCI(Commad Completion Interrupt)*/
#define USB_MS_PRO_CBI_NCCI               0x01   /* CBI(Control/Bulk/Interrupt) 传输，NCCI(No Commad Completion Interrupt)*/
#define USB_MS_PRO_OBSOLETE               0x02
#define USB_MS_PRO_BBB                    0x50   /* BBB(Bulk/Bulk/Bulk)，Bulk-Only 传输*/
#define USB_MS_PRO_UAS                    0x62
#define USB_MS_PRO_VENDOR_SPECIFIC        0xFF

/* \brief BBB，Bulk-Only 类定义 */
#define USB_BBB_REQ_RESET                 0xFF        /* 复位请求*/
#define USB_BBB_REQ_GET_MLUN              0xFE        /* 最大逻辑单元请求*/




//#define USB_BBB_REQ_CSW_SIG               0x53425355  /* 命令状态包签名*/
//#define USB_BBB_REQ_CBW_FLAG_OUT          0x00        /* 命令块包输出标志*/
//#define USB_BBB_REQ_CSW_LEN               13          /* 命令状态包长度*/
//#define USB_BBB_REQ_CSW_STA_PASS          0           /* 命令状态包状态阶段通过*/
//#define USB_BBB_REQ_CSW_STA_FAIL          1           /* 命令状态包状态阶段失败*/
//#define USB_BBB_REQ_CSW_STA_PHASE_ERROR   2           /* 命令状态包状态阶段错误*/


#endif


