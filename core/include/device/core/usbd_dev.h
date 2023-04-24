#ifndef __USBD_DEV_H
#define __USBD_DEV_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus  */
#include "common/list/usb_list.h"
#include "core/include/device/core/usbd.h"
#include <stdio.h>
#include <stdint.h>

#if USB_OS_EN
/* \brief USB 从机设备互斥锁超时时间*/
#define USBD_DEV_MUTEX_TIMEOUT 5000
#endif

/* \brief USB 配置数量最大值 */
#define USBD_CONFIG_NUM_MAX        3
/* \brief USB 接口数量最大值 */
#define USBD_INTF_NUM_MAX          10
/* \brief USB 接口备用设置数量最大值 */
#define USBD_INTF_ALT_NUM_MAX      10

/* \brief USB 设备字符串长度 */
#define USBD_STRING_LENGTH         32
/* \brief USB 设备字符串数量最大值 */
#define USBD_STRING_NUM_MAX        32

/* \brief USB 设备接口类型 */
#define USBD_INTERFACE_NORMAL      1
#define USBD_INTERFACE_ASSOC_FIRST 2
#define USBD_INTERFACE_ASSOC       3

/* \brief USB 设备接口功能获取 USB 从机控制器 */
#define USB_DC_GET_FROM_FUNC(p_func)       (p_func->p_cfg->p_dev->p_dc)
/* \brief USB 设备接口功能获取 USB 设备 */
#define USBD_DEV_GET_FROM_FUNC(p_func)     (p_func->p_cfg->p_dev)
/* \brief USB 设备接口功能获取接口号 */
#define USBD_FUNC_INTF_NUM_GET(p_func)     (p_func->intf_desc.interface_number)
/* \brief USB 设备接口功能获取接口备用设置号 */
#define USBD_FUNC_INTF_ALT_NUM_GET(p_func) (p_func->intf_desc.alternate_setting)
/* \brief USB 设备获取端点地址 */
#define USBD_EP_ADDR_GET(p_ep)             (p_ep->ep_addr)
/* \brief USB 设备获取端点最大包大小 */
#define USBD_EP_MPS_GET(p_ep)              (p_ep->cur_mps)
/* \brief USB 设备获取端点类型 */
#define USBD_EP_TYPE_GET(p_ep)             (p_ep->cur_type)

/* \brief 中断回调函数*/
typedef void (*p_fn_trans_cb)(void *p_arg);

/* \brief USB 设备状态 */
typedef enum {
    USB_DEV_STATE_NOTATTACHED = 0,
    USB_DEV_STATE_ATTACHED,
    USB_DEV_STATE_POWERED,
    USB_DEV_STATE_RECONNECTING,
    USB_DEV_STATE_UNAUTHENTICATED,
    USB_DEV_STATE_DEFAULT,
    USB_DEV_STATE_ADDRESS,
    USB_DEV_STATE_CONFIGURED,
    USB_DEV_STATE_SUSPENDED
} udevice_state_t;

struct usbd_func;
/* \brief USB 设备功能操作函数 */
struct usbd_func_opts {
    int (*p_fn_shutdown)(struct usbd_func *p_func, void *p_arg);
    int (*p_fn_setup)(struct usbd_func *p_func, struct usb_ctrlreq *p_setup, void *p_arg);
    int (*p_fn_alt_set)(struct usbd_func *p_func, void *p_arg);
    int (*p_fn_alt_get)(struct usbd_func *p_func, void *p_arg);
    int (*p_fn_suspend)(struct usbd_func *p_func, void *p_arg);
    int (*p_fn_resume)(struct usbd_func *p_func, void *p_arg);
};

#pragma pack(push)
#pragma pack(1)
/* \brief USB 设备字符串描述符 */
struct usb_string_desc {
    uint8_t length;
    uint8_t type;
    uint8_t string[USBD_STRING_LENGTH * 2];
};
#pragma pack(pop)

/* \brief USB 从机端点*/
struct usbd_ep {
    uint8_t              ep_addr;       /* 端点地址*/
    uint8_t              type_support;  /* 支持类型*/
    uint8_t              cur_type;      /* 当前类型*/
    uint16_t             mps_limt;      /* 最大包大小限制*/
    uint16_t             cur_mps;       /* 当前最大包大小*/
    struct usb_list_node node;          /* 端点节点*/
    usb_bool_t           is_enable;     /* 使能标志位*/
    usb_bool_t           is_stalled;    /* 停止标志位*/
};

/* \brief USB 设备传输结构体*/
struct usbd_trans {
    struct usbd_ep *p_hw;
    uint8_t        *p_buf;
    void           *p_buf_dma;
    size_t          len;
    void          (*p_fn_complete)(void *p_arg);
    void           *p_arg;
    size_t          act_len;            /* 实际传输的字节数*/
    int             status;             /* 传输状态*/
    int             flag;
};

/* \brief USB 从机设备管道信息*/
struct usbd_pipe_info {
    uint8_t  num;       /* 端点编号，为 0 则自动分配可用端点 */
    uint8_t  dir;       /* 端点方向，USB_DIR_IN ：输入，从机到主机
                                   USB_DIR_OUT：输出 ，主机到从机 */
    uint8_t  attr;      /* 传输类型和补充信息 */
    uint8_t  inter;     /* 中断和等时端点的服务时距 */
    uint16_t mps;       /* 端点每个事务允许最大数据字节数 */
};

/* \brief USBD对象管道类型定义 */
struct usbd_pipe {
    uint8_t                  ref_cnt;    /* 引用计数 */
    struct usbd_ep          *p_hw;       /* 端点指针 */
    struct usb_endpoint_desc ep_desc;    /* 接口描述符*/
    struct usbd_func_hdr    *p_func_hdr; /* 功能接口 */
#if USB_OS_EN
    usb_mutex_handle_t       p_lock;     /* 设备互斥锁*/
#endif
    struct usb_list_node     node;       /* 节点 */
    union {
        void                *p_sync;
        uint32_t             sync;
    };
};

/* \brief USB 从机设备接口功能信息 */
struct usbd_func_info {
    uint8_t    intf_num;        /* 接口编号*/
    uint8_t    alt_setting_num; /* 备用接口编号*/
    uint8_t    class;           /* 类代码 */
    uint8_t    sub_class;       /* 子类代码 */
    uint8_t    protocol;        /* 协议代码 */
    char      *p_intf_str;      /* 接口功能字符串，可为空*/
    uint8_t   *p_spec_desc;     /* 特殊描述符*/
    uint32_t   spec_desc_size;  /* 特殊描述符大小*/
};

/* \brief USB 从机设备联合接口功能信息 */
struct usbd_func_assoc_info {
    uint8_t  intf_num_first;  /* 功能联合的第一个接口的编号 */
    uint8_t  intf_count;      /* 功能联合的接口的数量 */
    uint8_t  class;           /* 类代码 */
    uint8_t  sub_class;       /* 子类代码 */
    uint8_t  protocol;        /* 协议代码 */
    char    *p_intf_str;      /* 联合接口功能字符串，可为空*/
};

/* \brief USB 设备功能结构体 */
struct usbd_func_hdr {
    uint8_t             type;                           /* 类型*/
    uint8_t             func_num;                       /* 接口功能编号*/
    uint8_t             n_func_alt;                     /* 备用设置数量*/
    struct usbd_func   *p_func[USBD_INTF_ALT_NUM_MAX];  /* 备用设置*/
};

/* \brief USB 设备功能结构体 */
struct usbd_func {
    struct usbd_config       *p_cfg;                          /* 所属配置*/
    struct usb_interface_desc intf_desc;                      /* 接口描述符*/
    char                      intf_str[USBD_STRING_LENGTH];   /* 接口字符串 */
    struct usbd_func_opts    *p_opts;                         /* 功能操作函数 */
    void                     *p_func_arg;                     /* 功能特定的操作函数参数*/
    usb_bool_t                is_enabled;                     /* 是否使能*/
    char                     *p_intf_str;                     /* 接口功能字符串，可为空*/
    uint8_t                  *p_spec_desc;                    /* 特殊描述符*/
    uint32_t                  spec_desc_size;                 /* 特殊描述符大小*/
};

/* \brief USB 从机设备联合接口功能结构体 */
struct usbd_func_assoc {
	struct usbd_func                func;
	struct usb_interface_assoc_desc intf_assoc_desc;                    /* 接口描述符*/
	char                            intf_assoc_str[USBD_STRING_LENGTH]; /* 接口字符串 */
};

/* \brief USB 配置结构体 */
struct usbd_config {
    struct usbd_dev        *p_dev;
    struct usb_config_desc  config_desc;                     /* 配置描述符*/
    char                    config_str[USBD_STRING_LENGTH];  /* 配置字符串 */
    struct usbd_func_hdr   *p_func_hdr[USBD_INTF_NUM_MAX];   /* 接口功能*/
    uint8_t                 n_func;                          /* 接口功能数量，不包括备用设置*/
    struct usb_list_head    pipe_list;
    /* 当前配置支持速度 */
    uint8_t                 supper_speed:1;
    uint8_t                 high_speed  :1;
    uint8_t                 full_speed  :1;
};

/* \brief USB 从机设备信息结构定义 */
struct usbd_dev_desc {
    uint8_t     clss;                 /* 类代码 */
    uint8_t     sub_clss;             /* 子类代码 */
    uint8_t     protocol;             /* 协议代码 */
    uint8_t     ep0_mps;              /* 端点 0 包最大尺寸 */
    uint16_t    bcd;                  /* 设备版本号 */
    uint16_t    vid;                  /* 厂商 ID */
    uint16_t    pid;                  /* 产品 ID */
    const char *p_manufacturer;       /* 制造商字符串描述 */
    const char *p_product;            /* 产品字符串描述 */
    const char *p_serial;             /* 序列号字符串描述 */
    const char *p_mircosoft_os_str;   /* 序列号字符串描述 */
};

/* \brief USB 从机设备 */
struct usbd_dev {
    char                          name[USB_NAME_LEN];                     /* 设备名字*/
    struct usb_dc                *p_dc;                                   /* 所属 USB 从机控制器 */
    struct usb_device_desc        dev_desc;                               /* 设备描述符 */
    struct usbd_config           *p_cur_cfg;                              /* 当前配置结构体 */
    struct usbd_config            def_cfg;                                /* 默认配置 */
    struct usbd_config           *p_cfgs[USBD_CONFIG_NUM_MAX];            /* 配置 */
    uint8_t                       addr;                                   /* 主机分配的设备地址*/
    void                         *p_drv_data;                             /* 驱动数据*/
    struct usb_list_node          node;                                   /* 当前设备节点 */
    struct usb_mircosoft_os_desc *p_mircosoft_os_desc;                    /* Mircosoft OS 描述符*/
#if USB_OS_EN
    usb_mutex_handle_t            p_lock;                                 /* 设备互斥锁*/
#endif
    uint8_t                       status;                                 /* 设备状态 */
    char                         *p_str[USBD_STRING_NUM_MAX];             /* 保存所有字符串的地址*/
    char                          mf_str[USBD_STRING_LENGTH];             /* 厂商字符串 */
    char                          pd_str[USBD_STRING_LENGTH];             /* 产品字符串 */
    char                          sn_str[USBD_STRING_LENGTH];             /* 序列号字符串 */
    char                          mircosoft_os_str[USBD_STRING_LENGTH];   /* Mircosoft OS 字符串 */
    uint8_t                       i_mircosoft_os;                         /* Mircosoft OS 字符串索引 */
    uint8_t                       vendor_code;
};

/**
 * \brief 创建一个 USB 从机设备
 *
 * \param[in]  p_dc     USB 从机控制器
 * \param[in]  p_name   USB 设备名字
 * \param[in]  dev_desc USB 设备描述符
 * \param[out] p_dev    返回创建成功的 USB 从机设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_create(struct usb_dc         *p_dc,
                    char                  *p_name,
                    struct usbd_dev_desc   dev_desc,
                    struct usbd_dev      **p_dev);
/**
 * \brief 销毁 USB 从机设备
 *
 * \param[in] p_dev 要销毁的 USB 从机设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_destroy(struct usbd_dev *p_dev);
/**
 * \brief 获取 USB 从机设备驱动数据
 *
 * \param[in]  p_dev      USB 从机控制器设备
 * \param[out] p_drv_data 返回获取到的驱动数据
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_drv_data_get(struct usbd_dev *p_dev, void **p_drv_data);
/**
 * \brief 设置 USB 从机设备驱动数据
 *
 * \param[in] p_dev      USB 从机控制器设备
 * \param[in] p_drv_data 要设置的驱动数据
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_drv_data_set(struct usbd_dev *p_dev, void *p_drv_data);
/**
 * \brief USB 从机设备添加一个配置
 *
 * \param[in] p_dev      要添加配置的 USB 从机设备
 * \param[in] p_cfg_str  配置字符串，可为空
 * \param[in] p_cfg_num  存储添加成功的配置编号
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_cfg_add(struct usbd_dev *p_dev, char *p_cfg_str, uint8_t *p_cfg_num);
/**
 * \brief USB 从机设备删除一个配置
 *
 * \param[in] p_dev        要删除配置的 USB 从机设备
 * \param[in] config_num   要删除的配置的编号
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_config_del(struct usbd_dev *p_dev, uint8_t config_num);
/**
 * \brief USB 从机设备添加一个功能接口
 *
 * \param[in] p_dev        要添加接口功能的 USB 从机设备
 * \param[in] config_num   要添加的接口功能的配置编号
 * \param[in] func_info    功能接口信息
 * \param[in] p_func_opts  功能接口操作函数
 * \param[in] p_func_arg   功能接口操作函数参数
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_func_add(struct usbd_dev       *p_dev,
                      uint8_t                config_num,
                      struct usbd_func_info  func_info,
                      struct usbd_func_opts *p_func_opts,
                      void                  *p_func_arg);
/**
 * \brief USB 从机设备删除一个功能接口
 *
 * \param[in] p_dev        要删除接口功能的 USB 从机设备
 * \param[in] config_num   要删除的接口功能的配置编号
 * \param[in] intf_num     功能接口编号
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_func_del(struct usbd_dev *p_dev, uint8_t config_num, uint8_t intf_num);
/**
 * \brief USB 从机设备添加联合功能接口
 *
 * \param[in] p_dev            要添加联合接口功能的 USB 从机设备
 * \param[in] config_num       要添加联合接口功能的配置编号
 * \param[in] func_assoc_info  联合功能接口信息
 * \param[in] p_func_info      功能接口信息
 * \param[in] p_func_opts      功能接口操作函数
 * \param[in] p_func_arg       功能接口操作函数参数
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_func_assoc_add(struct usbd_dev             *p_dev,
                            uint8_t                      config_num,
                            struct usbd_func_assoc_info  func_assoc_info,
                            struct usbd_func_info       *p_func_info,
                            struct usbd_func_opts       *p_func_opts,
                            void                        *p_func_arg);
/**
 * \brief 获取对应端点地址的 USB 设备管道结构体
 *
 * \param[in]  p_func  USB 接口功能结构体
 * \param[in]  ep_num  端点号
 * \param[in]  ep_dir  端点方向
 * \param[out] p_pipe  返回找到的管道结构体地址
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_func_pipe_find(struct usbd_func  *p_func,
                            uint8_t            ep_num,
                            uint8_t            ep_dir,
                            struct usbd_pipe **p_pipe);
/**
 * \brief USB 从机设备删除联合功能接口
 *
 * \param[in] p_dev          要删除联合接口功能的 USB 从机设备
 * \param[in] cfg_num        要删除联合接口功能的配置编号
 * \param[in] intf_num_first 联合接口功能的第一个接口编号
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_func_assoc_del(struct usbd_dev *p_dev,
                            uint8_t          cfg_num,
                            uint8_t          intf_num_first);
/**
 * \brief 添加 USB 从机设备管道
 *
 * \param[in]  p_dev           USB 从机设备
 * \param[in]  cfg_num         相关的配置编号
 * \param[in]  intf_num        相关的接口编号
 * \param[in]  alt_setting_num 相关的接口备用设置编号
 * \param[in]  p_info          管道信息
 * \param[out] p_ep_addr       返回添加的管道端点地址
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_pipe_add(struct usbd_dev      *p_dev,
                      uint8_t               cfg_num,
                      uint8_t               intf_num,
                      uint8_t               alt_setting_num,
                      struct usbd_pipe_info p_info,
                      uint8_t              *p_ep_addr);
/**
 * \brief 删除 USB 从机设备管道
 *
 * \param[in] p_dev           USB 从机设备
 * \param[in] cfg_num         相关的配置编号
 * \param[in] intf_num        相关的接口编号
 * \param[in] alt_setting_num 相关的接口备用设置编号
 * \param[in] pipe_dir        管道方向
 * \param[in] pipe_addr       管道地址
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_pipe_del(struct usbd_dev *p_dev,
                      uint8_t          cfg_num,
                      uint8_t          intf_num,
                      uint8_t          alt_setting_num,
                      uint8_t          pipe_dir,
                      uint8_t          pipe_addr);
/**
 * \brief 获取 USB 从机设备通讯管道
 *
 * \param[in]  p_dev   USB 从机设备
 * \param[in]  ep_num  端点号
 * \param[in]  ep_dir  端点方向
 * \param[out] p_pipe  返回找到的管道结构体地址
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_pipe_get(struct usbd_dev   *p_dev,
                      uint8_t            ep_num,
                      uint8_t            ep_dir,
                      struct usbd_pipe **p_pipe);
/**
 * \brief USB 设备异步传输函数
 *
 * \param[in] p_dev   USB 从机设备
 * \param[in] p_trans USB 传输请求
 *
 * \return[in] 成功返回 USB_OK
 */
int usbd_dev_trans_async(struct usbd_dev   *p_dev,
                         struct usbd_trans *p_trans);
/**
 * \brief USB 设备同步传输函数
 *
 * \param[in] p_dev     USB 从机设备
 * \param[in] p_pipe    使用的 USB 管道
 * \param[in] p_buf     数据缓存
 * \param[in] len       数据长度
 * \param[in] flag      标志
 * \param[in] timeout   超时时间
 * \param[in] p_act_len 存储实际传输的数据长度缓存
 *
 * \retval 成功返回 USB_OK
 */
int usbd_dev_trans_sync(struct usbd_dev  *p_dev,
                        struct usbd_pipe *p_pipe,
                        uint8_t          *p_buf,
                        uint32_t          len,
                        int               flag,
                        int               timeout,
                        uint32_t         *p_act_len);
#ifdef __cplusplus
}
#endif  /* __cplusplus  */

#endif /* __USBD_DEV_H */
