#ifndef __USB_HID_SPECS_H
#define __USB_HID_SPECS_H
#include <stdio.h>
#include <stdint.h>

/* \brief HID 类描述符*/
struct hid_class_descriptor {
    uint8_t descriptor_type;
    int16_t descriptor_length;
} __attribute__ ((packed));

/* \brief HID 描述符*/
struct hid_descriptor {
    uint8_t                     length;
    uint8_t                     descriptor_type;
    int16_t                     bcd_hid;
    uint8_t                     country_code;
    uint8_t                     num_descriptors;
    struct hid_class_descriptor desc[1];
} __attribute__ ((packed));

/* \brief USB 人体接口设备接口子类*/
#define USB_INTERFACE_SUBCLASS_BOOT             1

/* \brief USB 人体接口设备协议*/
#define USB_INTERFACE_PROTOCOL_KEYBOARD         1
#define USB_INTERFACE_PROTOCOL_MOUSE            2

/* HID 描述符类型 */
#define HID_HID_DESCRIPTOR_TYPE                 0x21
#define HID_REPORT_DESCRIPTOR_TYPE              0x22
#define HID_PHYSICAL_DESCRIPTOR_TYPE            0x23

/* \brief HID 类请求*/
#define HID_REQ_GET_REPORT                      0x01
#define HID_REQ_GET_IDLE                        0x02
#define HID_REQ_GET_PROTOCOL                    0x03
#define HID_REQ_SET_REPORT                      0x09
#define HID_REQ_SET_IDLE                        0x0A
#define HID_REQ_SET_PROTOCOL                    0x0B

#define HID_INPUT_REPORT                        0
#define HID_OUTPUT_REPORT                       1
#define HID_FEATURE_REPORT                      2
/* \brief HID 报告类型*/
#define HID_REPORT_TYPES                        3

/* \brief HID 报告条目格式 */
#define HID_ITEM_FORMAT_SHORT                   0
#define HID_ITEM_FORMAT_LONG                    1

/* \brief HID 报告描述符主条目标记 */
#define HID_MAIN_ITEM_TAG_INPUT                 8
#define HID_MAIN_ITEM_TAG_OUTPUT                9
#define HID_MAIN_ITEM_TAG_FEATURE               11
#define HID_MAIN_ITEM_TAG_BEGIN_COLLECTION      10
#define HID_MAIN_ITEM_TAG_END_COLLECTION        12

/* \brief HID 报告描述符主条目录 */
#define HID_MAIN_ITEM_CONSTANT                  0x001
#define HID_MAIN_ITEM_VARIABLE                  0x002
#define HID_MAIN_ITEM_RELATIVE                  0x004
#define HID_MAIN_ITEM_WRAP                      0x008
#define HID_MAIN_ITEM_NONLINEAR                 0x010
#define HID_MAIN_ITEM_NO_PREFERRED              0x020
#define HID_MAIN_ITEM_NULL_STATE                0x040
#define HID_MAIN_ITEM_VOLATILE                  0x080
#define HID_MAIN_ITEM_BUFFERED_BYTE             0x100

/* \brief HID 报告描述符全局条目标记 */
#define HID_GLOBAL_ITEM_TAG_USAGE_PAGE          0
#define HID_GLOBAL_ITEM_TAG_LOGICAL_MINIMUM     1
#define HID_GLOBAL_ITEM_TAG_LOGICAL_MAXIMUM     2
#define HID_GLOBAL_ITEM_TAG_PHYSICAL_MINIMUM    3
#define HID_GLOBAL_ITEM_TAG_PHYSICAL_MAXIMUM    4
#define HID_GLOBAL_ITEM_TAG_UNIT_EXPONENT       5
#define HID_GLOBAL_ITEM_TAG_UNIT                6
#define HID_GLOBAL_ITEM_TAG_REPORT_SIZE         7
#define HID_GLOBAL_ITEM_TAG_REPORT_ID           8
#define HID_GLOBAL_ITEM_TAG_REPORT_COUNT        9
#define HID_GLOBAL_ITEM_TAG_PUSH                10
#define HID_GLOBAL_ITEM_TAG_POP                 11

/* \brief HID 报告描述符本地条目标记*/
#define HID_LOCAL_ITEM_TAG_USAGE                0
#define HID_LOCAL_ITEM_TAG_USAGE_MINIMUM        1
#define HID_LOCAL_ITEM_TAG_USAGE_MAXIMUM        2
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_INDEX     3
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_MINIMUM   4
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_MAXIMUM   5
#define HID_LOCAL_ITEM_TAG_STRING_INDEX         7
#define HID_LOCAL_ITEM_TAG_STRING_MINIMUM       8
#define HID_LOCAL_ITEM_TAG_STRING_MAXIMUM       9
#define HID_LOCAL_ITEM_TAG_DELIMITER            10

/* \brief HID 用法表*/
#define HID_USAGE_PAGE                          0xffff0000
#define HID_UP_UNDEFINED                        0x00000000
#define HID_UP_GENDESK                          0x00010000
#define HID_UP_SIMULATION                       0x00020000
#define HID_UP_GENDEVCTRLS                      0x00060000
#define HID_UP_KEYBOARD                         0x00070000
#define HID_UP_LED                              0x00080000
#define HID_UP_BUTTON                           0x00090000
#define HID_UP_ORDINAL                          0x000a0000
#define HID_UP_TELEPHONY                        0x000b0000
#define HID_UP_CONSUMER                         0x000c0000
#define HID_UP_DIGITIZER                        0x000d0000
#define HID_UP_PID                              0x000f0000
#define HID_UP_HPVENDOR                         0xff7f0000
#define HID_UP_HPVENDOR2                        0xff010000
#define HID_UP_MSVENDOR                         0xff000000
#define HID_UP_CUSTOM                           0x00ff0000
#define HID_UP_LOGIVENDOR                       0xffbc0000
#define HID_UP_LNVENDOR                         0xffa00000
#define HID_UP_SENSOR                           0x00200000

#define HID_GD_POINTER                          0x00010001
#define HID_GD_MOUSE                            0x00010002
#define HID_GD_JOYSTICK                         0x00010004
#define HID_GD_GAMEPAD                          0x00010005
#define HID_GD_KEYBOARD                         0x00010006
#define HID_GD_KEYPAD                           0x00010007
#define HID_GD_MULTIAXIS                        0x00010008
#define HID_GD_X                                0x00010030
#define HID_GD_Y                                0x00010031
#define HID_GD_Z                                0x00010032
#define HID_GD_RX                               0x00010033
#define HID_GD_RY                               0x00010034
#define HID_GD_RZ                               0x00010035
#define HID_GD_SLIDER                           0x00010036
#define HID_GD_DIAL                             0x00010037
#define HID_GD_WHEEL                            0x00010038
#define HID_GD_HATSWITCH                        0x00010039
#define HID_GD_BUFFER                           0x0001003a
#define HID_GD_BYTECOUNT                        0x0001003b
#define HID_GD_MOTION                           0x0001003c
#define HID_GD_START                            0x0001003d
#define HID_GD_SELECT                           0x0001003e
#define HID_GD_VX                               0x00010040
#define HID_GD_VY                               0x00010041
#define HID_GD_VZ                               0x00010042
#define HID_GD_VBRX                             0x00010043
#define HID_GD_VBRY                             0x00010044
#define HID_GD_VBRZ                             0x00010045
#define HID_GD_VNO                              0x00010046
#define HID_GD_FEATURE                          0x00010047
#define HID_GD_SYSTEM_CONTROL                   0x00010080
#define HID_GD_UP                               0x00010090
#define HID_GD_DOWN                             0x00010091
#define HID_GD_RIGHT                            0x00010092
#define HID_GD_LEFT                             0x00010093

#define HID_DG_CONFIDENCE                       0x000d0047
#define HID_DG_WIDTH                            0x000d0048
#define HID_DG_HEIGHT                           0x000d0049
#define HID_DG_CONTACTID                        0x000d0051
#define HID_DG_INPUTMODE                        0x000d0052
#define HID_DG_DEVICEINDEX                      0x000d0053
#define HID_DG_CONTACTCOUNT                     0x000d0054
#define HID_DG_CONTACTMAX                       0x000d0055
#define HID_DG_BUTTONTYPE                       0x000d0059
#define HID_DG_BARRELSWITCH2                    0x000d005a
#define HID_DG_TOOLSERIALNUMBER                 0x000d005b

/* \brief HID 报告描述符收集条目类型*/
#define HID_COLLECTION_PHYSICAL                 0
#define HID_COLLECTION_APPLICATION              1
#define HID_COLLECTION_LOGICAL                  2

#endif

