#ifndef __USB_HUB_SPECS_H
#define __USB_HUB_SPECS_H


/* \brief 集线器端口特性号*/
#define HUB_PORT_FEAT_CONNECTION           0
#define HUB_PORT_FEAT_ENABLE               1
#define HUB_PORT_FEAT_SUSPEND              2       /* L2 suspend */
#define HUB_PORT_FEAT_OVER_CURRENT         3
#define HUB_PORT_FEAT_RESET                4
#define HUB_PORT_FEAT_L1                   5        /* L1 suspend */
#define HUB_PORT_FEAT_POWER                8        /* 端口上电*/
#define HUB_PORT_FEAT_LOWSPEED             9        /* Should never be used */
#define HUB_PORT_FEAT_C_CONNECTION         16       /* 端口有设备连接特性*/
#define HUB_PORT_FEAT_C_ENABLE             17       /* 端口使能特性*/
#define HUB_PORT_FEAT_C_SUSPEND            18       /* 端口挂起特性*/
#define HUB_PORT_FEAT_C_OVER_CURRENT       19       /* 端口过流特性*/
#define HUB_PORT_FEAT_C_RESET              20       /* 端口复位特性*/
#define HUB_PORT_FEAT_TEST                 21
#define HUB_PORT_FEAT_INDICATOR            22
#define HUB_PORT_FEAT_C_PORT_L1            23
#define HUB_PORT_FEAT_LINK_STATE           5
#define HUB_PORT_FEAT_U1_TIMEOUT           23
#define HUB_PORT_FEAT_U2_TIMEOUT           24
#define HUB_PORT_FEAT_C_PORT_LINK_STATE    25
#define HUB_PORT_FEAT_C_PORT_CONFIG_ERROR  26
#define HUB_PORT_FEAT_REMOTE_WAKE_MASK     27
#define HUB_PORT_FEAT_BH_PORT_RESET        28
#define HUB_PORT_FEAT_C_BH_PORT_RESET      29
#define HUB_PORT_FEAT_FORCE_LINKPM_ACCEPT  30

/* \brief 集线器状态-wHubStatus */
#define HUB_STAT_LOCAL_POWER               0x0001    /* 集线器上电状态*/
#define HUB_STAT_OVERCURRENT               0x0002    /* 集线器过流状态*/
/* \brief 集线器变化-wHubChange */
#define HUB_CHANGE_LOCAL_POWER             0x0001    /* 集线器发生电源变化*/
#define HUB_CHANGE_OVERCURRENT             0x0002    /* 集线器发生过流变化*/

/* \brief 集线器类特性编号*/
#define HUB_LOCAL_POWER                    0         /* 集线器电源变化*/
#define HUB_OVER_CURRENT                   1         /* 集线器过流*/

/* \brief 端口变化位 -wPortChange*/
#define HUB_PORT_STAT_C_CONNECTION         0x0001    /* 端口已连接的状态发生变化*/
#define HUB_PORT_STAT_C_ENABLE             0x0002    /* 端口使能状态发生变化*/
#define HUB_PORT_STAT_C_SUSPEND            0x0004    /* 端口挂起状态发生变化*/
#define HUB_PORT_STAT_C_OVERCURRENT        0x0008    /* 端口过流状态发生变化*/
#define HUB_PORT_STAT_C_RESET              0x0010    /* 端口复位状态发生变化*/

/* 端口状态-wPortStatus  */
#define HUB_PORT_STAT_CONNECTION           0x0001    /* 端口有设备连接状态*/
#define HUB_PORT_STAT_ENABLE               0x0002
#define HUB_PORT_STAT_SUSPEND              0x0004
#define HUB_PORT_STAT_OVERCURRENT          0x0008    /* 端口过流状态*/
#define HUB_PORT_STAT_RESET                0x0010
#define HUB_PORT_STAT_L1                   0x0020
#define HUB_PORT_STAT_POWER                0x0100
#define HUB_PORT_STAT_LOW_SPEED            0x0200    /* 端口低速状态*/
#define HUB_PORT_STAT_HIGH_SPEED           0x0400    /* 端口高速状态*/
#define HUB_PORT_STAT_TEST                 0x0800
#define HUB_PORT_STAT_INDICATOR            0x1000

#endif

