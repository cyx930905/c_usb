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
#include "core/include/device/class/hid/usbd_hid_keyboard.h"

/*******************************************************************************
 * Static
 ******************************************************************************/
/* \brief 键盘报告设备 */
static const char __g_keyboard_report[63] = {
    //表示用途页为通用桌面设备
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)

    //表示用途为键盘
    0x09, 0x06,                    // USAGE (Keyboard)

    //表示应用集合，必须要以END_COLLECTION来结束它，见最后的END_COLLECTION
    0xa1, 0x01,                    // COLLECTION (Application)

    //表示用途页为按键
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)

    //用途最小值，这里为左ctrl键
    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
    //用途最大值，这里为右GUI键，即window键
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
    //逻辑最小值为0
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    //逻辑最大值为1
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    //报告大小（即这个字段的宽度）为1bit，所以前面的逻辑最小值为0，逻辑最大值为1
    0x75, 0x01,                    //   REPORT_SIZE (1)
    //报告的个数为8，即总共有8个bits
    0x95, 0x08,                    //   REPORT_COUNT (8)
    //输入用，变量，值，绝对值。像键盘这类一般报告绝对值，
    //而鼠标移动这样的则报告相对值，表示鼠标移动多少
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    //上面这这几项描述了一个输入用的字段，总共为8个bits，每个bit表示一个按键
    //分别从左ctrl键到右GUI键。这8个bits刚好构成一个字节，它位于报告的第一个字节。
    //它的最低位，即bit-0对应着左ctrl键，如果返回的数据该位为1，则表示左ctrl键被按下，
    //否则，左ctrl键没有按下。最高位，即bit-7表示右GUI键的按下情况。中间的几个位，
    //需要根据HID协议中规定的用途页表（HID Usage Tables）来确定。这里通常用来表示
    //特殊键，例如ctrl，shift，del键等

    //这样的数据段个数为1
    0x95, 0x01,                    //   REPORT_COUNT (1)
    //每个段长度为8bits
    0x75, 0x08,                    //   REPORT_SIZE (8)
    //输入用，常量，值，绝对值
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)

    //上面这8个bit是常量，设备必须返回0

    //这样的数据段个数为5
    0x95, 0x05,                    //   REPORT_COUNT (5)
    //每个段大小为1bit
    0x75, 0x01,                    //   REPORT_SIZE (1)
    //用途是LED，即用来控制键盘上的LED用的，因此下面会说明它是输出用
    0x05, 0x08,                    //   USAGE_PAGE (LEDs)
    //用途最小值是Num Lock，即数字键锁定灯
    0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
    //用途最大值是Kana，这个是什么灯我也不清楚^_^
    0x29, 0x05,                    //   USAGE_MAXIMUM (Kana)
    //如前面所说，这个字段是输出用的，用来控制LED。变量，值，绝对值。
    //1表示灯亮，0表示灯灭
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)

    //这样的数据段个数为1
    0x95, 0x01,                    //   REPORT_COUNT (1)
    //每个段大小为3bits
    0x75, 0x03,                    //   REPORT_SIZE (3)
    //输出用，常量，值，绝对
    0x91, 0x03,                    //   OUTPUT (Cnst,Var,Abs)
    //由于要按字节对齐，而前面控制LED的只用了5个bit，
    //所以后面需要附加3个不用bit，设置为常量。

    //报告个数为6
    0x95, 0x06,                    //   REPORT_COUNT (6)
    //每个段大小为8bits
    0x75, 0x08,                    //   REPORT_SIZE (8)
    //逻辑最小值0
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    //逻辑最大值255
    0x25, 0xFF,                    //   LOGICAL_MAXIMUM (255)
    //用途页为按键
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    //使用最小值为0
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
    //使用最大值为0x65
    0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)
    //输入用，变量，数组，绝对值
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    //以上定义了6个8bit宽的数组，每个8bit（即一个字节）用来表示一个按键，所以可以同时
    //有6个按键按下。没有按键按下时，全部返回0。如果按下的键太多，导致键盘扫描系统
    //无法区分按键时，则全部返回0x01，即6个0x01。如果有一个键按下，则这6个字节中的第一
    //个字节为相应的键值（具体的值参看HID Usage Tables），如果两个键按下，则第1、2两个
    //字节分别为相应的键值，以次类推。

    //关集合，跟上面的对应
    0xc0                           // END_COLLECTION
};

/* \brief 键盘对应表*/
static const unsigned char _g_kbd_keycode[256] = {
      0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
     50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
      4,  5,  6,  7,  8,  9, 10, 11, 28,  1, 14, 15, 57, 12, 13, 26,
     27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64,
     65, 66, 67, 68, 87, 88, 99, 70,119,110,102,104,111,107,109,106,
    105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
     72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,
    191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
    115,114,  0,  0,  0,121,  0, 89, 93,124, 92, 94, 95,  0,  0,  0,
    122,123, 90, 91, 85,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     29, 42, 56,125, 97, 54,100,126,164,166,165,163,161,115,114,113,
    150,158,159,128,136,177,178,176,142,152,173,140
};

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 创建一个 USB 从机键盘设备
 *
 * \param[in]  p_dc      所属的 USB 从机控制器
 * \param[in]  p_name    设备名字
 * \param[in]  dev_desc  设备描述符
 * \param[out] p_hid_ret 返回创建成功的 USB 从机人体接口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_keyboard_create(struct usb_dc        *p_dc,
                             char                 *p_name,
                             struct usbd_dev_desc  dev_desc,
                             struct usbd_hid     **p_hid_ret){
    return usbd_hid_create(p_dc,
                           p_name,
                           dev_desc,
                          (char *)__g_keyboard_report,
                           sizeof(__g_keyboard_report),
                           p_hid_ret);
}

/**
 * \brief 销毁一个 USB 从机键盘设备
 *
 * \param[in] p_hid 要销毁的  USB 从机人体接口设
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_keyboard_destroy(struct usbd_hid *p_hid){
    return usbd_hid_destroy(p_hid);
}

/**
 * \brief USB 从机键盘设备发送数据
 *
 * \param[in]  p_hid      USB 从机键盘设备
 * \param[in]  p_report   键盘报告数据
 * \param[in]  timeout    发送超时时间
 * \param[out] p_act_len  返回实际发送的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_keyboard_write(struct usbd_hid                *p_hid,
                            struct usbd_keyboard_in_report *p_report,
                            int                             timeout,
                            uint32_t                       *p_act_len){
    return usbd_hid_write(p_hid,
                         (uint8_t *)p_report,
                          sizeof(struct usbd_keyboard_in_report),
                          timeout,
                          p_act_len);
}

/**
 * \brief USB 从机键盘设备发送数据
 *
 * \param[in]  p_hid      USB 从机键盘设备
 * \param[in]  p_report   键盘报告数据缓存
 * \param[in]  timeout    接收超时时间
 * \param[out] p_act_len  返回实际接收的数据长度
 *
 * \retval 成功返回 USB_OK
 */
int usbd_hid_keyboard_read_sync(struct usbd_hid                 *p_hid,
                                struct usbd_keyboard_out_report *p_report,
                                int                              timeout,
                                uint32_t                        *p_act_len){
    return usbd_hid_read_sync(p_hid,
                             (uint8_t *)p_report,
                              sizeof(struct usbd_keyboard_out_report),
                              timeout,
                              p_act_len);
}

/**
 * \brief 键盘键值转换
 *
 * \param[in] key 键值
 *
 * \retval 成功返回转换的索引
 */
uint8_t usbd_hid_keyboayd_key_to_code(uint8_t key){
    for (int i = 0; i < 256; i++) {
        if (_g_kbd_keycode[i] == key) {
            return i;
        }
    }
    return 0;
}
