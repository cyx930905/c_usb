/*******************************************************************************
*                                    ZLG
*                         ----------------------------
*                         innovating embedded platform
*
* Copyright (c) 2001-2021 Guangzhou ZHIYUAN Electronics Co., Ltd.
* All rights reserved.
*
* Contact information:
* web site:    https://www.zlg.cn
*******************************************************************************/
#ifndef __USBH_EHCI_REG_H
#define __USBH_EHCI_REG_H

/* \brief 性能寄存器偏移地址*/
#define __CAP_REG_LENGTH    (0x00000000)   /* 储存操作寄存器偏移值寄存器偏移*/
#define __CAP_REG_HCS       (0x00000004)   /* 主机控制器结构参数寄存器偏移*/
#define __CAP_REG_HCC       (0x00000008)   /* 主机控制器性能参数寄存器偏移*/

/* \brief 操作寄存器偏移地址*/
#define __OPT_REG_CMD       (0x00000000)   /* 命令寄存器偏移*/
#define __OPT_REG_STS       (0x00000004)   /* 状态寄存器偏移*/
#define __OPT_REG_INTR      (0x00000008)   /* 中断使能寄存器偏移*/
#define __OPT_REG_FDIX      (0x0000000C)   /* 帧索引寄存器偏移*/
#define __OPT_REG_CTRL      (0x00000010)
#define __OPT_REG_PERIOD    (0x00000014)   /* 周期帧列表基地址寄存器偏移*/
#define __OPT_REG_ASYNC     (0x00000018)   /* 异步调度基地址寄存器偏移*/
#define __OPT_REG_TTSTS     (0x0000001C)
#define __OPT_REG_CFG       (0x00000040)   /* 配置标志寄存器偏移*/
#define __OPT_REG_PSC0      (0x00000044)   /* 端口状态和控制寄存器偏移*/
#define __OPT_REG_PSC1      (0x00000048)   /* 端口1状态和控制寄存器偏移(如果有)*/
#define __OPT_REG_PSC2      (0x0000004C)   /* 端口2状态和控制寄存器偏移(如果有)*/
#define __OPT_REG_PSC3      (0x00000050)   /* 端口3状态和控制寄存器偏移(如果有)*/
#define __OPT_REG_PSC4      (0x00000054)   /* 端口4状态和控制寄存器偏移(如果有)*/
#define __OPT_REG_PSC5      (0x00000058)   /* 端口5状态和控制寄存器偏移(如果有)*/
#define __OPT_REG_PSC6      (0x0000005C)   /* 端口6状态和控制寄存器偏移(如果有)*/
#define __OPT_REG_PSC7      (0x00000060)   /* 端口7状态和控制寄存器偏移(如果有)*/
#define __OPT_REG_PSC8      (0x00000064)   /* 端口8状态和控制寄存器偏移(如果有)*/
#define __OPT_REG_MODE      (0x00000068)   /* 控制器模式寄存器偏移*/

/* \brief 性能寄存器位操作*/
#define __CAP_HCS_PI(p)                       ((p) & (1 << 16))
#define __CAP_HCS_PPC(p)                      ((p) & (1 << 4))      /* 端口电源控制 */
#define __CAP_HCS_N_PORTS(p)                  (((p) >> 0) & 0xf)    /* 位3:0, 主机控制器的端口数量*/
#define __CAP_HCC_32FRAME_PERIODIC_LIST(p)    ((p) & (1 << 19))
#define __CAP_HCC_PER_PORT_CHANGE_EVENT(p)    ((p) & (1 << 18))
#define __CAP_HCC_LPM(p)                      ((p) & (1 << 17))
#define __CAP_HCC_HW_PREFETCH(p)              ((p) & (1 << 16))
#define __CAP_HCC_EXT_CAPS(p)                 (((p) >> 8) & 0xff)   /* for pci extended caps */
#define __CAP_HCC_ISOC_CACHE(p)               ((p) & (1 << 7))      /* 是否可以缓存等时帧*/
#define __CAP_HCC_ISOC_THRES(p)               (((p) >> 4) & 0x7)    /* 位 6:4, 微帧缓存*/
#define __CAP_HCC_CANPARK(p)                  ((p) & (1 << 2))      /* true: can park on async qh */
#define __CAP_HCC_PGM_FRAME_LIST_FLAG(p)      ((p) & (1 << 1))      /* 是否周期帧链表大小可变*/
#define __CAP_HCC_64BIT_ADDR(p)               ((p) & (1))           /* 是否可以使用64位地址*/

/* \brief 命令操作寄存器位操作*/
#define __REG_CMD_PPCEE       (1 << 15)             /* 每一个端口变化事件使能*/
#define __REG_CMD_PARK        (1 << 11)             /* enable "park" on async qh */
#define __REG_CMD_PARK_CNT(c) (((c) >> 8) & 3)      /* how many transfers to park for */
#define __REG_CMD_LRESET      (1 << 7)              /* partial reset (no ports, etc) */
#define __REG_CMD_IAAD        (1 << 6)              /* "doorbell" interrupt async advance */
#define __REG_CMD_ASE         (1 << 5)              /* 使能异步调度*/
#define __REG_CMD_PSE         (1 << 4)              /* 使能周期调度*/
#define __REG_CMD_FLS_1024    (0x00000000)          /* 1024帧一周期 */
#define __REG_CMD_FLS_512     (0x00000004)          /*  512帧一周期 */
#define __REG_CMD_FLS_256     (0x00000008)          /*  256帧一周期 */
#define __REG_CMD_FLS_128     (0x0000000C)          /*  128帧一周期 */
#define __REG_CMD_FLS_64      (0x00008000)          /*   64帧一周期 */
#define __REG_CMD_FLS_32      (0x00008004)          /*   32帧一周期 */
#define __REG_CMD_FLS_16      (0x00008008)          /*   16帧一周期 */
#define __REG_CMD_FLS_8       (0x0000800C)          /*    8帧一周期 */
#define __REG_CMD_RESET       (1 << 1)              /* 复位主机控制器(非总线) */
#define __REG_CMD_RUN         (1 << 0)              /* 启动/停止主机控制器*/

/* \brief 状态操作寄存器位操作*/
#define __REG_STS_ASS           (1 << 15)           /* 异步调度状态*/
#define __REG_STS_PSS           (1 << 14)           /* 周期调度状态*/
#define __REG_STS_RECL          (1 << 13)           /* Reclamation */
#define __REG_STS_HALT          (1 << 12)           /* Not running (any reason) */
#define __REG_STS_IAA           (1 << 5)            /* 异步推进中断*/
#define __REG_STS_FATAL         (1 << 4)            /* such as some PCI access errors */
#define __REG_STS_FLR           (1 << 3)            /* frame list rolled over */
#define __REG_STS_PCD           (1 << 2)            /* 端口变化检测*/
#define __REG_STS_ERR           (1 << 1)            /* "error" completion (overflow, ...) */
#define __REG_STS_INT           (1 << 0)            /* "normal" completion (short, ...) */

/* \brief 模式操作寄存器位操作*/
#define __REG_MODE_SDIS          (1 << 3)           /* 流禁能*/
#define __REG_MODE_BE            (1 << 2)           /* 大/小端选择*/
#define __REG_MODE_CM_HC         (3 << 0)           /* 主机控制器模式*/
#define __REG_MODE_CM_IDLE       (0 << 0)           /* 空闲状态*/

/* \brief 端口速度位*/
#define __REG_PORTSC_PS_FS       (0 << 26)          /* 全速端口*/
#define __REG_PORTSC_PS_LS       (1 << 26)          /* 低速端口*/
#define __REG_PORTSC_PS_HS       (2 << 26)          /* 高速端口*/
#define __REG_PORTSC_PS_UNKNOWN  (3 << 26)          /* 未知*/

/* \brief 端口状态和变化寄存器位操作*/
#define __REG_PORTSC_PSPD        (3 << 26)          /* 端口速度位*/
#define __REG_PORTSC_SSTS        (3 << 23)          /* 挂起状态 */
#define __REG_PORTSC_WKOC_E      (1 << 22)          /* 过流唤醒 */
#define __REG_PORTSC_WKDISC_E    (1 << 21)          /* 断开唤醒 */
#define __REG_PORTSC_WKCONN_E    (1 << 20)          /* 连接唤醒 */
#define __REG_PORTSC_PO          (1 << 13)          /* 配套主机控制器拥有这个端口*/
#define __REG_PORTSC_PP          (1 << 12)          /* 端口在上电*/
#define __REG_PORTSC_USB11(x)   (((x) & (3 << 10)) == (1 << 10))    /* USB 1.1 设备*/
#define __REG_PORTSC_RESET       (1 << 8)           /* 端口复位*/
#define __REG_PORTSC_SUSPEND     (1 << 7)           /* 端口挂起*/
#define __REG_PORTSC_RESUME      (1 << 6)           /* 端口恢复*/
#define __REG_PORTSC_OCC         (1 << 5)           /* 过流变化 */
#define __REG_PORTSC_OC          (1 << 4)           /* 过流激活 */
#define __REG_PORTSC_PEC         (1 << 3)           /* 端口使能变化 */
#define __REG_PORTSC_PE          (1 << 2)           /* 端口使能 */
#define __REG_PORTSC_CSC         (1 << 1)           /* 端口状态变化 */
#define __REG_PORTSC_CONNECT     (1 << 0)           /* 端口连接 */

/* \brief EHCI 传输数据结构类型定义*/
#define __Q_TYPE_ITD  (0 << 1)
#define __Q_TYPE_QH   (1 << 1)
#define __Q_TYPE_SITD (2 << 1)
#define __Q_TYPE_FSTN (3 << 1)

/* \brief 队列头 XactErr 重试限制*/
#define __QH_XACTERR_MAX        32

#define __QH_CONTROL_EP   (1 << 27)   /* 控制端点*/
#define __QH_HEAD         (1 << 15)   /* Head of async reclamation list */
#define __QH_TOGGLE_CTL   (1 << 14)   /* 数据切换控制*/
#define __QH_HIGH_SPEED   (2 << 12)   /* 高速端点*/
#define __QH_LOW_SPEED    (1 << 12)   /* 低速端点*/
//#define __QH_FULL_SPEED   (0 << 12)
//#define __QH_INACTIVATE   (1 << 7)  /* Inactivate on next transaction */

#define __QH_SMASK    0x000000ff
#define __QH_CMASK    0x0000ff00
#define __QH_HUBADDR  0x007f0000
#define __QH_HUBPORT  0x3f800000
#define __QH_MULT     0xc0000000

/* \brief QTD PID*/
#define __QTD_PID_OUT     0
#define __QTD_PID_IN      1
#define __QTD_PID_SETUP   2


#define __QTD_TOGGLE      (1u << 31)               /* 数据切换*/
#define __QTD_LENGTH(tok) (((tok) >> 16) & 0x7fffu)
#define __QTD_IOC         (1u << 15)               /* 完成中断*/
#define __QTD_CERR(tok)   (((tok) >> 10) & 0x3u)
#define __QTD_PID(tok)    (((tok) >> 8) & 0x3u)
#define __QTD_STS_ACTIVE  (1u << 7)                /* HC may execute this */
#define __QTD_STS_HALT    (1u << 6)                /* 错误停止*/
#define __QTD_STS_DBE     (1u << 5)                /* 数据缓存错误 (主机控制器) */
#define __QTD_STS_BABBLE  (1u << 4)                /* device was babbling (qtd halted) */
#define __QTD_STS_XACT    (1u << 3)                /* 设备返回非法响应 */
#define __QTD_STS_MMF     (1u << 2)                /* 分割传输未完成*/
#define __QTD_STS_STS     (1u << 1)                /* 分割事务状态 */
#define __QTD_STS_PING    (1u << 0)                /* issue PING? */

#define IS_SHORT_READ(tok) ((__QTD_LENGTH(tok) != 0) && (__QTD_PID(tok) == 1))

#endif /* __USBH_EHCI_REG_H */
