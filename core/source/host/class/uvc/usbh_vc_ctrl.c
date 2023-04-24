/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/uvc/usbh_vc_drv.h"
#include "core/include/host/class/uvc/usbh_vc_entity.h"
#include "core/include/host/class/uvc/usbh_vc_stream.h"
#include "common/refcnt/usb_refcnt.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*******************************************************************************
 * Statement
 ******************************************************************************/
/* \brief USB 视频类设备 ID 匹配*/
struct uvc_device_id {
    uint16_t match_flags;     /* 匹配标志*/
    uint16_t vendor_id;       /* VID*/
    uint16_t product_id;      /* PID*/
};

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct usbh_vc_lib __g_uvc_lib;

/*******************************************************************************
 * Marco operate
 ******************************************************************************/
#define UVC_MATCH_VENDOR          0x0001  /* 匹配VID*/
#define UVC_MATCH_PRODUCT         0x0002  /* 匹配PID*/

#define UVC_DEVICE_MATCH(vend, prod)                      \
    .match_flags =  UVC_MATCH_VENDOR | UVC_MATCH_PRODUCT, \
    .vendor_id   = (vend),                                \
    .product_id  = (prod)

#define UVC_CTRL_DATA_CURRENT   0
#define UVC_CTRL_DATA_BACKUP    1
#define UVC_CTRL_DATA_MIN       2
#define UVC_CTRL_DATA_MAX       3
#define UVC_CTRL_DATA_RES       4
#define UVC_CTRL_DATA_DEF       5
#define UVC_CTRL_DATA_LAST      6

/*******************************************************************************
 * Static
 ******************************************************************************/
static int __vc_ctrl_zoom_get(struct usbh_vc_ctrl_mapping *p_mapping,
                              uint8_t                      query,
                              const uint8_t               *p_data);
static void __vc_ctrl_zoom_set(struct usbh_vc_ctrl_mapping *p_mapping,
                               int                          value,
                               uint8_t                     *p_data);
static int __vc_ctrl_rel_speed_get(struct usbh_vc_ctrl_mapping *p_mapping,
                                   uint8_t                      query,
                                   const uint8_t               *p_data);
static void __vc_ctrl_rel_speed_set(struct usbh_vc_ctrl_mapping *p_mapping,
                                   int                           value,
                                   uint8_t                      *p_data);

static struct uvc_menu_info power_line_frequency_controls[] = {
    { 0, "Disabled" },
    { 1, "50 Hz" },
    { 2, "60 Hz" },
};

static struct uvc_menu_info exposure_auto_controls[] = {
    { 2, "Auto Mode" },
    { 1, "Manual Mode" },
    { 4, "Shutter Priority Mode" },
    { 8, "Aperture Priority Mode" },
};

/* \brief USB 视频类控件信息*/
static struct usbh_vc_ctrl_info uvc_ctrls_info[] = {
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_BRIGHTNESS_CONTROL,
        .index      = 0,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_CONTRAST_CONTROL,
        .index      = 1,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_HUE_CONTROL,
        .index      = 2,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE
                | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_SATURATION_CONTROL,
        .index      = 3,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_SHARPNESS_CONTROL,
        .index      = 4,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_GAMMA_CONTROL,
        .index      = 5,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL,
        .index      = 6,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE
                | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL,
        .index      = 7,
        .size       = 4,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE
                | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_BACKLIGHT_COMPENSATION_CONTROL,
        .index      = 8,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_GAIN_CONTROL,
        .index      = 9,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_POWER_LINE_FREQUENCY_CONTROL,
        .index      = 10,
        .size       = 1,
        .flags      = UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
                | UVC_CTRL_FLAG_GET_DEF | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_HUE_AUTO_CONTROL,
        .index      = 11,
        .size       = 1,
        .flags      = UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
                | UVC_CTRL_FLAG_GET_DEF | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL,
        .index      = 12,
        .size       = 1,
        .flags      = UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
                | UVC_CTRL_FLAG_GET_DEF | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL,
        .index      = 13,
        .size       = 1,
        .flags      = UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
                | UVC_CTRL_FLAG_GET_DEF | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_DIGITAL_MULTIPLIER_CONTROL,
        .index      = 14,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL,
        .index      = 15,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL,
        .index      = 16,
        .size       = 1,
        .flags      = UVC_CTRL_FLAG_GET_CUR,
    },
    {
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_ANALOG_LOCK_STATUS_CONTROL,
        .index      = 17,
        .size       = 1,
        .flags      = UVC_CTRL_FLAG_GET_CUR,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_SCANNING_MODE_CONTROL,
        .index      = 0,
        .size       = 1,
        .flags      = UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
                | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_AE_MODE_CONTROL,
        .index      = 1,
        .size       = 1,
        .flags      = UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
                | UVC_CTRL_FLAG_GET_DEF | UVC_CTRL_FLAG_GET_RES
                | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_AE_PRIORITY_CONTROL,
        .index      = 2,
        .size       = 1,
        .flags      = UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
                | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL,
        .index      = 3,
        .size       = 4,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL,
        .index      = 4,
        .size       = 1,
        .flags      = UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_FOCUS_ABSOLUTE_CONTROL,
        .index      = 5,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE
                | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_FOCUS_RELATIVE_CONTROL,
        .index      = 6,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_MIN
                | UVC_CTRL_FLAG_GET_MAX | UVC_CTRL_FLAG_GET_RES
                | UVC_CTRL_FLAG_GET_DEF
                | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_IRIS_ABSOLUTE_CONTROL,
        .index      = 7,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE
                | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_IRIS_RELATIVE_CONTROL,
        .index      = 8,
        .size       = 1,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_ZOOM_ABSOLUTE_CONTROL,
        .index      = 9,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE
                | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_ZOOM_RELATIVE_CONTROL,
        .index      = 10,
        .size       = 3,
        .flags      = UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_MIN
                | UVC_CTRL_FLAG_GET_MAX | UVC_CTRL_FLAG_GET_RES
                | UVC_CTRL_FLAG_GET_DEF
                | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_PANTILT_ABSOLUTE_CONTROL,
        .index      = 11,
        .size       = 8,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE
                | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_PANTILT_RELATIVE_CONTROL,
        .index      = 12,
        .size       = 4,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_ROLL_ABSOLUTE_CONTROL,
        .index      = 13,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR
                | UVC_CTRL_FLAG_GET_RANGE
                | UVC_CTRL_FLAG_RESTORE
                | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_ROLL_RELATIVE_CONTROL,
        .index      = 14,
        .size       = 2,
        .flags      = UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_MIN
                | UVC_CTRL_FLAG_GET_MAX | UVC_CTRL_FLAG_GET_RES
                | UVC_CTRL_FLAG_GET_DEF
                | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_FOCUS_AUTO_CONTROL,
        .index      = 17,
        .size       = 1,
        .flags      = UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
                | UVC_CTRL_FLAG_GET_DEF | UVC_CTRL_FLAG_RESTORE,
    },
    {
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_PRIVACY_CONTROL,
        .index      = 18,
        .size       = 1,
        .flags      = UVC_CTRL_FLAG_SET_CUR | UVC_CTRL_FLAG_GET_CUR
                | UVC_CTRL_FLAG_RESTORE
                | UVC_CTRL_FLAG_AUTO_UPDATE,
    },
};

/* \brief USB 视频类控件映射*/
static struct usbh_vc_ctrl_mapping uvc_ctrl_mappings[] = {
    {
        .id         = UVC_CID_BRIGHTNESS,
        .name       = "Brightness",
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_BRIGHTNESS_CONTROL,
        .size       = 16,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_SIGNED,
    },
    {
        .id         = UVC_CID_CONTRAST,
        .name       = "Contrast",
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_CONTRAST_CONTROL,
        .size       = 16,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_UNSIGNED,
    },
    {
        .id         = UVC_CID_HUE,
        .name       = "Hue",
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_HUE_CONTROL,
        .size       = 16,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_SIGNED,
        .master_id  = UVC_CID_HUE_AUTO,
        .master_manual  = 0,
    },
    {
        .id         = UVC_CID_SATURATION,
        .name       = "Saturation",
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_SATURATION_CONTROL,
        .size       = 16,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_UNSIGNED,
    },
    {
        .id         = UVC_CID_SHARPNESS,
        .name       = "Sharpness",
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_SHARPNESS_CONTROL,
        .size       = 16,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_UNSIGNED,
    },
    {
        .id         = UVC_CID_GAMMA,
        .name       = "Gamma",
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_GAMMA_CONTROL,
        .size       = 16,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_UNSIGNED,
    },
    {
        .id         = UVC_CID_BACKLIGHT_COMPENSATION,
        .name       = "Backlight Compensation",
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_BACKLIGHT_COMPENSATION_CONTROL,
        .size       = 16,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_UNSIGNED,
    },
    {
        .id         = UVC_CID_GAIN,
        .name       = "Gain",
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_GAIN_CONTROL,
        .size       = 16,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_UNSIGNED,
    },
    {
        .id          = UVC_CID_POWER_LINE_FREQUENCY,
        .name        = "Power Line Frequency",
        .entity      = UVC_GUID_UVC_PROCESSING,
        .selector    = UVC_PU_POWER_LINE_FREQUENCY_CONTROL,
        .size        = 2,
        .offset      = 0,
        .ctrl_type   = UVC_CTRL_TYPE_MENU,
        .data_type   = UVC_CTRL_DATA_TYPE_ENUM,
        .p_menu_info = power_line_frequency_controls,
        .menu_count  = USB_NELEMENTS(power_line_frequency_controls),
    },
    {
        .id         = UVC_CID_HUE_AUTO,
        .name       = "Hue, Auto",
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_HUE_AUTO_CONTROL,
        .size       = 1,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_BOOLEAN,
        .data_type  = UVC_CTRL_DATA_TYPE_BOOLEAN,
        .slave_ids  = { UVC_CID_HUE, },
    },
    {
        .id          = UVC_CID_EXPOSURE_AUTO,
        .name        = "Exposure, Auto",
        .entity      = UVC_GUID_UVC_CAMERA,
        .selector    = UVC_CT_AE_MODE_CONTROL,
        .size        = 4,
        .offset      = 0,
        .ctrl_type   = UVC_CTRL_TYPE_MENU,
        .data_type   = UVC_CTRL_DATA_TYPE_BITMASK,
        .p_menu_info = exposure_auto_controls,
        .menu_count  = USB_NELEMENTS(exposure_auto_controls),
        .slave_ids   = { UVC_CID_EXPOSURE_ABSOLUTE, },
    },
    {
        .id         = UVC_CID_EXPOSURE_AUTO_PRIORITY,
        .name       = "Exposure, Auto Priority",
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_AE_PRIORITY_CONTROL,
        .size       = 1,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_BOOLEAN,
        .data_type  = UVC_CTRL_DATA_TYPE_BOOLEAN,
    },
    {
        .id         = UVC_CID_EXPOSURE_ABSOLUTE,
        .name       = "Exposure (Absolute)",
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL,
        .size       = 32,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_UNSIGNED,
        .master_id  = UVC_CID_EXPOSURE_AUTO,
        .master_manual  = UVC_EXPOSURE_MANUAL,
    },
    {
        .id         = UVC_CID_AUTO_WHITE_BALANCE,
        .name       = "White Balance Temperature, Auto",
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL,
        .size       = 1,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_BOOLEAN,
        .data_type  = UVC_CTRL_DATA_TYPE_BOOLEAN,
        .slave_ids  = { UVC_CID_WHITE_BALANCE_TEMPERATURE, },
    },
    {
        .id         = UVC_CID_WHITE_BALANCE_TEMPERATURE,
        .name       = "White Balance Temperature",
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL,
        .size       = 16,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_UNSIGNED,
        .master_id  = UVC_CID_AUTO_WHITE_BALANCE,
        .master_manual  = 0,
    },
    {
        .id         = UVC_CID_AUTO_WHITE_BALANCE,
        .name       = "White Balance Component, Auto",
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL,
        .size       = 1,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_BOOLEAN,
        .data_type  = UVC_CTRL_DATA_TYPE_BOOLEAN,
        .slave_ids  = { UVC_CID_BLUE_BALANCE,
                        UVC_CID_RED_BALANCE },
    },
    {
        .id         = UVC_CID_BLUE_BALANCE,
        .name       = "White Balance Blue Component",
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL,
        .size       = 16,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_SIGNED,
        .master_id  = UVC_CID_AUTO_WHITE_BALANCE,
        .master_manual  = 0,
    },
    {
        .id         = UVC_CID_RED_BALANCE,
        .name       = "White Balance Red Component",
        .entity     = UVC_GUID_UVC_PROCESSING,
        .selector   = UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL,
        .size       = 16,
        .offset     = 16,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_SIGNED,
        .master_id  = UVC_CID_AUTO_WHITE_BALANCE,
        .master_manual  = 0,
    },
    {
        .id         = UVC_CID_FOCUS_ABSOLUTE,
        .name       = "Focus (absolute)",
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_FOCUS_ABSOLUTE_CONTROL,
        .size       = 16,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_UNSIGNED,
        .master_id  = UVC_CID_FOCUS_AUTO,
        .master_manual  = 0,
    },
    {
        .id         = UVC_CID_FOCUS_AUTO,
        .name       = "Focus, Auto",
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_FOCUS_AUTO_CONTROL,
        .size       = 1,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_BOOLEAN,
        .data_type  = UVC_CTRL_DATA_TYPE_BOOLEAN,
        .slave_ids  = { UVC_CID_FOCUS_ABSOLUTE, },
    },
    {
        .id         = UVC_CID_IRIS_ABSOLUTE,
        .name       = "Iris, Absolute",
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_IRIS_ABSOLUTE_CONTROL,
        .size       = 16,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_UNSIGNED,
    },
    {
        .id         = UVC_CID_IRIS_RELATIVE,
        .name       = "Iris, Relative",
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_IRIS_RELATIVE_CONTROL,
        .size       = 8,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_SIGNED,
    },
    {
        .id         = UVC_CID_ZOOM_ABSOLUTE,
        .name       = "Zoom, Absolute",
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_ZOOM_ABSOLUTE_CONTROL,
        .size       = 16,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_UNSIGNED,
    },
    {
        .id         = UVC_CID_ZOOM_CONTINUOUS,
        .name       = "Zoom, Continuous",
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_ZOOM_RELATIVE_CONTROL,
        .size       = 0,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_SIGNED,
        .p_fn_get   = __vc_ctrl_zoom_get,
        .p_fn_set   = __vc_ctrl_zoom_set,
    },
    {
        .id         = UVC_CID_PAN_ABSOLUTE,
        .name       = "Pan (Absolute)",
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_PANTILT_ABSOLUTE_CONTROL,
        .size       = 32,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_SIGNED,
    },
    {
        .id         = UVC_CID_TILT_ABSOLUTE,
        .name       = "Tilt (Absolute)",
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_PANTILT_ABSOLUTE_CONTROL,
        .size       = 32,
        .offset     = 32,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_SIGNED,
    },
    {
        .id         = UVC_CID_PAN_SPEED,
        .name       = "Pan (Speed)",
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_PANTILT_RELATIVE_CONTROL,
        .size       = 16,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_SIGNED,
        .p_fn_get   = __vc_ctrl_rel_speed_get,
        .p_fn_set   = __vc_ctrl_rel_speed_set,
    },
    {
        .id         = UVC_CID_TILT_SPEED,
        .name       = "Tilt (Speed)",
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_PANTILT_RELATIVE_CONTROL,
        .size       = 16,
        .offset     = 16,
        .ctrl_type  = UVC_CTRL_TYPE_INTEGER,
        .data_type  = UVC_CTRL_DATA_TYPE_SIGNED,
        .p_fn_get   = __vc_ctrl_rel_speed_get,
        .p_fn_set   = __vc_ctrl_rel_speed_set,
    },
    {
        .id         = UVC_CID_PRIVACY,
        .name       = "Privacy",
        .entity     = UVC_GUID_UVC_CAMERA,
        .selector   = UVC_CT_PRIVACY_CONTROL,
        .size       = 1,
        .offset     = 0,
        .ctrl_type  = UVC_CTRL_TYPE_BOOLEAN,
        .data_type  = UVC_CTRL_DATA_TYPE_BOOLEAN,
    },
};

static const uint8_t uvc_processing_guid[16]            = UVC_GUID_UVC_PROCESSING;
static const uint8_t uvc_camera_guid[16]                = UVC_GUID_UVC_CAMERA;
static const uint8_t uvc_media_transport_input_guid[16] = UVC_GUID_UVC_MEDIA_TRANSPORT_INPUT;

/*******************************************************************************
 * Code
 ******************************************************************************/
static int __vc_memweight(const void *ptr, size_t bytes){
    int            ret      = 0;
    const uint8_t *p_bitmap = ptr;

    for (; bytes > 0; bytes--, p_bitmap++)
        ret += usb_hweight8(*p_bitmap);
    return ret;
}

/**
 * \brief 匹配 USB 设备 ID
 */
static usb_bool_t __vc_dev_id_match(struct usbh_vc             *p_vc,
                                    const struct uvc_device_id *p_id){
    /* USB 设备 ID 匹配标志：匹配供应商*/
    if ((p_id->match_flags & UVC_MATCH_VENDOR) &&
            p_id->vendor_id != USBH_DEV_VID_GET(p_vc->p_ufun)) {
        return USB_FALSE;
    }
    /* USB 设备 ID 匹配标志：匹配产商*/
    if ((p_id->match_flags & UVC_MATCH_PRODUCT) &&
            p_id->product_id != USBH_DEV_PID_GET(p_vc->p_ufun)) {
        return USB_FALSE;
    }
    return USB_TRUE;
}

/**
 * \brief USB 视频类实体匹配 GUID
 */
static int __vc_entity_match_guid(const struct usbh_vc_entity *p_entity,
                                  const uint8_t                guid[16]){
    /* 检查UVC实体标志*/
    switch (UVC_ENTITY_TYPE(p_entity)) {
        /* 摄像头终端*/
        case UVC_ITT_CAMERA:
            return memcmp(uvc_camera_guid, guid, 16) == 0;
        /* 媒体*/
        case UVC_ITT_MEDIA_TRANSPORT_INPUT:
            return memcmp(uvc_media_transport_input_guid, guid, 16) == 0;
        /* 处理单元*/
        case UVC_VC_PROCESSING_UNIT:
            return memcmp(uvc_processing_guid, guid, 16) == 0;
        /* 扩展单元*/
        case UVC_VC_EXTENSION_UNIT:
            return memcmp(p_entity->extension.guid_extension_code, guid, 16) == 0;
        default:
            return 0;
    }
}

/**
 * \brief 获取控件
 */
static void __vc_ctrl_get(struct usbh_vc_entity        *p_entity,
                          uint32_t                      idx,
                          struct usbh_vc_ctrl_mapping **p_mapping,
                          struct usbh_vc_ctrl         **p_control,
                          int                           next){
    struct usb_list_node        *p_node = NULL;
    struct usbh_vc_ctrl         *p_ctrl = NULL;
    struct usbh_vc_ctrl_mapping *p_map  = NULL;
    unsigned int                 i;

    /* 遍历控件*/
    for (i = 0; i < p_entity->n_controls; ++i) {

    	p_ctrl = &p_entity->p_controls[i];
        /* 检查是否初始化*/
        if (!p_ctrl->initialized){
            continue;
        }
        usb_list_for_each_node(p_node, &p_ctrl->info.mappings) {
            p_map = usb_container_of(p_node, struct usbh_vc_ctrl_mapping, node);

            if ((p_map->id == idx) && !next) {
                *p_control = p_ctrl;
                *p_mapping = p_map;
                return;
            }

            if ((*p_mapping == NULL || (*p_mapping)->id > p_map->id) &&
                (p_map->id > idx) && next) {
                *p_control = p_ctrl;
                *p_mapping = p_map;
            }
        }
    }
}

/**
 * \brief 获取控件数据
 */
static uint8_t *__vc_ctrl_data_get(struct usbh_vc_ctrl *p_ctrl, int id){
    return p_ctrl->p_uvc_data + id * p_ctrl->info.size;
}

/**
 * \brief 使用黑名单修剪其伪控件的实体。伪控件是当前是摄像头崩溃的控件，当被查询到时无条件返回错误。
 */
static void __vc_ctrl_entity_prune(struct usbh_vc        *p_vc,
                                   struct usbh_vc_entity *p_entity){
    /* 黑名单控件结构体*/
    struct uvc_ctrl_blacklist {
        struct uvc_device_id id;
        uint8_t              index;
    };
    /* 处理单元黑名单*/
    static const struct uvc_ctrl_blacklist processing_blacklist[] = {
        { { UVC_DEVICE_MATCH(0x13d3, 0x509b) }, 9 }, /* Gain */
        { { UVC_DEVICE_MATCH(0x1c4f, 0x3000) }, 6 }, /* WB Temperature */
        { { UVC_DEVICE_MATCH(0x5986, 0x0241) }, 2 }, /* Hue */
    };
    /* 摄像头黑名单*/
    static const struct uvc_ctrl_blacklist camera_blacklist[] = {
        { { UVC_DEVICE_MATCH(0x06f8, 0x3005) }, 9 }, /* Zoom, Absolute */
    };

    const struct uvc_ctrl_blacklist *p_blacklist = NULL;
    uint32_t                         size, count, i;
    uint8_t                         *p_controls;
    /* 检查实体类型*/
    switch (UVC_ENTITY_TYPE(p_entity)) {
        /* 处理单元*/
        case UVC_VC_PROCESSING_UNIT:
            /* 获取处理单元控件黑名单*/
        	p_blacklist = processing_blacklist;
            /* 黑名单大小*/
            count       = USB_NELEMENTS(processing_blacklist);
            /* 获取处理单元的控件位图*/
            p_controls  = p_entity->processing.p_controls;
            /* 获取处理单元的控件位图大小*/
            size        = p_entity->processing.control_size;
            break;
        /* 摄像头*/
        case UVC_ITT_CAMERA:
            /* 获取摄像头黑名单及大小*/
        	p_blacklist = camera_blacklist;
            count       = USB_NELEMENTS(camera_blacklist);
            /* 获取摄像头控制位图及大小*/
            p_controls  = p_entity->camera.p_controls;
            size        = p_entity->camera.control_size;
            break;
        default:
            return;
    }
    for (i = 0; i < count; ++i) {
        /* 匹配黑名单设备的 ID */
        if (!__vc_dev_id_match(p_vc, &p_blacklist[i].id)) {
            continue;
        }
        /* 如果匹配到黑名单控件，判断实体该控件的位是否1*/
        if (p_blacklist[i].index >= 8 * size ||
            !usb_test_bit(p_controls, p_blacklist[i].index))
            continue;

        __USB_INFO("%u/%u control is black listed, removing it\r\n",
                       p_entity->id, p_blacklist[i].index);
        /* 清除该位*/
        usb_clear_bit(p_controls, p_blacklist[i].index);
    }
}

/**
 * \brief 从“data”中存储的小端数据中提取 mapping->offset 和 mapping->size 指定的
 *        字符串，并将结果作为带符号的32位整数返回。如果映射引用带符号的数据类型， 则带执行符号扩展1。
 */
static int __vc_le_value_get(struct usbh_vc_ctrl_mapping *p_mapping,
                             uint8_t                      query,
                             const uint8_t               *p_data){
    int     bits   = p_mapping->size;
    int     offset = p_mapping->offset;
    int     value  = 0;
    uint8_t mask;

    p_data += offset / 8;
    offset &= 7;
    mask = ((1LL << bits) - 1) << offset;

    for (; bits > 0; p_data++) {
        uint8_t byte = *p_data & mask;
        value  |= offset > 0 ? (byte >> offset) : (byte << (-offset));
        bits   -= 8 - (offset > 0 ? offset : 0);
        offset -= 8;
        mask = (1 << bits) - 1;
    }

    /* 如果需要，可以对值进行符号扩展*/
    if (p_mapping->data_type == UVC_CTRL_DATA_TYPE_SIGNED){
        value |= -(value & (1 << (p_mapping->size - 1)));
    }
    return value;
}

/**
 * \brief 将“data”中存储的小端数据中的 mapping->offset 和 mapping->size 指定
 *        的位字符串设置为值“value”
 */
static void __vc_le_value_set(struct usbh_vc_ctrl_mapping *p_mapping,
                              int                          value,
                              uint8_t                     *p_data){
    int     bits   = p_mapping->size;
    int     offset = p_mapping->offset;
    uint8_t mask;

    if (p_mapping->ctrl_type == UVC_CTRL_TYPE_BUTTON) {
        value = -1;
    }

    p_data += offset / 8;
    offset &= 7;

    for (; bits > 0; p_data++) {
        mask = ((1LL << bits) - 1) << offset;
        *p_data = (*p_data & ~mask) | ((value << offset) & mask);
        value >>= offset ? offset : 8;
        bits -= 8 - offset;
        offset = 0;
    }
}

static int __vc_ctrl_zoom_get(struct usbh_vc_ctrl_mapping *p_mapping,
                              uint8_t                      query,
                              const uint8_t               *p_data){
    char zoom = (char)p_data[0];

    switch (query) {
        case UVC_GET_CUR:
            return (zoom == 0) ? 0 : (zoom > 0 ? p_data[2] : -p_data[2]);

        case UVC_GET_MIN:
        case UVC_GET_MAX:
        case UVC_GET_RES:
        case UVC_GET_DEF:
        default:
            return p_data[2];
    }
}

static void __vc_ctrl_zoom_set(struct usbh_vc_ctrl_mapping *p_mapping,
                               int                          value,
                               uint8_t                     *p_data){
	p_data[0] = value == 0 ? 0 : (value > 0) ? 1 : 0xff;
	p_data[2] = min((int)abs(value), 0xff);
}

static int __vc_ctrl_rel_speed_get(struct usbh_vc_ctrl_mapping *p_mapping,
                                   uint8_t                      query,
                                   const uint8_t               *p_data){
    unsigned int first = p_mapping->offset / 8;
    char rel           = (char)p_data[first];

    switch (query) {
        case UVC_GET_CUR:
            return (rel == 0) ? 0 : (rel > 0 ? p_data[first + 1] : -p_data[first + 1]);
        case UVC_GET_MIN:
            return -p_data[first + 1];
        case UVC_GET_MAX:
        case UVC_GET_RES:
        case UVC_GET_DEF:
        default:
            return p_data[first + 1];
    }
}

static void __vc_ctrl_rel_speed_set(struct usbh_vc_ctrl_mapping *p_mapping,
                                   int                           value,
                                   uint8_t                      *p_data){
    unsigned int first = p_mapping->offset / 8;

    p_data[first] = value == 0 ? 0 : (value > 0) ? 1 : 0xff;
    p_data[first + 1] = min((int)abs(value), 0xff);
}

/**
 * \brief 在一个给定的控制里添加控制信息
 */
static int __vc_ctrl_info_add(struct usbh_vc                 *p_vc,
                              struct usbh_vc_ctrl            *p_ctrl,
                              const struct usbh_vc_ctrl_info *p_info){
    p_ctrl->info = *p_info;
    usb_list_head_init(&p_ctrl->info.mappings);

    /* 分配一个数组来保存控制变量 */
    p_ctrl->p_uvc_data = usb_lib_malloc(&__g_uvc_lib.lib, p_ctrl->info.size * UVC_CTRL_DATA_LAST + 1);
    if (p_ctrl->p_uvc_data == NULL) {
        return -USB_ENOMEM;
    }
    memset(p_ctrl->p_uvc_data, 0, p_ctrl->info.size * UVC_CTRL_DATA_LAST + 1);
    /* 设置控件已初始化*/
    p_ctrl->initialized = 1;

    return USB_OK;
}

/**
 * \brief 在一个给定而定控制里添加控件映射
 */
static int __vc_ctrl_mapping_add(struct usbh_vc                    *p_vc,
                                 struct usbh_vc_ctrl               *p_ctrl,
                                 const struct usbh_vc_ctrl_mapping *p_mapping){
    struct usbh_vc_ctrl_mapping *p_map = NULL;
    unsigned int                 size;

    p_map = usb_lib_malloc(&__g_uvc_lib.lib, sizeof(struct usbh_vc_ctrl_mapping));
    if (p_map == NULL) {
        return -USB_ENOMEM;
    }
    memcpy(p_map, p_mapping, sizeof(struct usbh_vc_ctrl_mapping));

    usb_list_head_init(&p_map->ev_subs);

    size = sizeof(*p_mapping->p_menu_info) * p_mapping->menu_count;

    p_map->p_menu_info = usb_lib_malloc(&__g_uvc_lib.lib, size);
    if (p_map->p_menu_info == NULL) {
        return -USB_ENOMEM;
    }
    memcpy(p_map->p_menu_info, p_mapping->p_menu_info, size);
    if (p_map->p_menu_info == NULL) {
    	usb_lib_mfree(&__g_uvc_lib.lib, p_map);
        return -USB_ENOMEM;
    }

    if (p_map->p_fn_get == NULL) {
    	p_map->p_fn_get = __vc_le_value_get;
    }
    if (p_map->p_fn_set == NULL) {
    	p_map->p_fn_set = __vc_le_value_set;
    }
    usb_list_node_add_tail(&p_map->node, &p_ctrl->info.mappings);

    return USB_OK;
}

/**
 * \brief USB 视频类控件查询
 */
static int __vc_ctrl_query(struct usbh_vc *p_vc,
                           uint8_t         query,
                           uint8_t         unit,
                           uint8_t         intf_num,
                           uint8_t         cs,
                           void           *p_data,
                           uint16_t        size,
                           int             time_out){
    struct usbh_device *p_usb_dev = p_vc->p_ufun->p_usb_dev;
    /* 设置 USB 请求类型*/
    uint8_t             type      = USB_REQ_TYPE_CLASS | USB_REQ_TAG_INTERFACE;

    /* 判断请求方向*/
    type |= (query & 0x80) ? USB_DIR_IN : USB_DIR_OUT;
    /* 发送请求*/
    return usbh_ctrl_trp_sync_xfer(&p_usb_dev->ep0,
                                    type,
                                    query,
                                    cs << 8,
                                    unit << 8 | intf_num,
                                    size,
                                    p_data,
                                    time_out,
                                    0);
}

/**
 * \brief 获取当前控件的缓存的值
 */
static int __vc_ctrl_populate_cache(struct usbh_vc_video_chain *p_chain,
                                    struct usbh_vc_ctrl        *p_ctrl){
    int ret;

    if (p_ctrl->info.flags & UVC_CTRL_FLAG_GET_DEF) {
        ret = __vc_ctrl_query(p_chain->p_vc,
                              UVC_GET_DEF,
							  p_ctrl->p_entity->id,
							  USBH_INTF_NUM_GET(p_chain->p_vc->p_ctrl_intf),
                              p_ctrl->info.selector,
							  __vc_ctrl_data_get(p_ctrl, UVC_CTRL_DATA_DEF),
                              p_ctrl->info.size,
                              300);
        if (ret < 0) {
            return ret;
        }
    }

    if (p_ctrl->info.flags & UVC_CTRL_FLAG_GET_MIN) {
        ret = __vc_ctrl_query(p_chain->p_vc,
                              UVC_GET_MIN,
                              p_ctrl->p_entity->id,
                              USBH_INTF_NUM_GET(p_chain->p_vc->p_ctrl_intf),
                              p_ctrl->info.selector,
                              __vc_ctrl_data_get(p_ctrl, UVC_CTRL_DATA_MIN),
                              p_ctrl->info.size,
                              300);
        if (ret < 0) {
            return ret;
        }
    }
    if (p_ctrl->info.flags & UVC_CTRL_FLAG_GET_MAX) {
        ret = __vc_ctrl_query(p_chain->p_vc,
                              UVC_GET_MAX,
                              p_ctrl->p_entity->id,
                              USBH_INTF_NUM_GET(p_chain->p_vc->p_ctrl_intf),
                              p_ctrl->info.selector,
                              __vc_ctrl_data_get(p_ctrl, UVC_CTRL_DATA_MAX),
                              p_ctrl->info.size,
                              300);
        if (ret < 0) {
            return ret;
        }
    }
    if (p_ctrl->info.flags & UVC_CTRL_FLAG_GET_RES) {
        ret = __vc_ctrl_query(p_chain->p_vc,
                              UVC_GET_RES,
                              p_ctrl->p_entity->id,
                              USBH_INTF_NUM_GET(p_chain->p_vc->p_ctrl_intf),
                              p_ctrl->info.selector,
                              __vc_ctrl_data_get(p_ctrl, UVC_CTRL_DATA_RES),
                              p_ctrl->info.size,
                              300);
        if (ret < 0) {
            if (UVC_ENTITY_TYPE(p_ctrl->p_entity) != UVC_VC_EXTENSION_UNIT){
                return ret;
            }
            memset(__vc_ctrl_data_get(p_ctrl, UVC_CTRL_DATA_RES), 0, p_ctrl->info.size);
        }
    }
    /* 设置已缓存标记*/
    p_ctrl->cached = 1;
    return USB_OK;
}

/**
 * \brief USB 视频类修复视频控件
 */
static void __vc_vedio_ctrl_fixup(struct usbh_vc_stream  *p_stream,
                                  struct uvc_stream_ctrl *p_ctrl){

    struct uvc_format *p_format = NULL;
    struct uvc_frame  *p_frame = NULL;
    uint32_t           i;

    /* 确定视频流的格式*/
    for (i = 0; i < p_stream->n_formats; ++i) {
        if (p_stream->p_format[i].index == p_ctrl->format_index) {
        	p_format = &p_stream->p_format[i];
            break;
        }
    }

    if (p_format == NULL) {
        return;
    }
    /* 确定格式的帧*/
    for (i = 0; i < p_format->n_frames; ++i) {
        if (p_format->p_frame[i].frame_index == p_ctrl->frame_index) {
        	p_frame = &p_format->p_frame[i];
            break;
        }
    }
    if (p_frame == NULL) {
        return;
    }
    /* 视频格式为压缩格式，设置编解码最大段大小*/
    if (!(p_format->flags & UVC_FMT_FLAG_COMPRESSED) ||
          (p_ctrl->max_video_frame_size == 0 && p_stream->p_vc->uvc_version < 0x0110)) {
        p_ctrl->max_video_frame_size = p_frame->max_video_frame_buffer_size;
    }

    if (!(p_format->flags & UVC_FMT_FLAG_COMPRESSED) &&
            (p_stream->p_vc->quirks & UVC_QUIRK_FIX_BANDWIDTH) &&
            (p_stream->p_vc->n_stream_intf_altset > 1)) {
        uint32_t interval;
        uint32_t bandwidth;

        interval = (p_ctrl->frame_interval > 100000) ? p_ctrl->frame_interval : p_frame->p_frame_interval[0];
        /*通过将帧大小乘以每秒视频帧的数目来计算带宽估计，将结果除以每秒USB帧(或高速设备的微帧)的数目
                              并添加UVC头大小(假设为12字节长)*/
        /* 带宽：帧宽 x 帧高 x 一个像素多少位 /1个字节8位*/
        bandwidth  = p_frame->width * p_frame->height / 8 * p_format->bpp;
        bandwidth *= 10000000 / interval + 1;
        bandwidth /= 1000;
        /* 高速设备，1帧等于8微帧*/
        if (p_stream->p_vc->p_ufun->p_usb_dev->speed == USB_SPEED_HIGH) {
            bandwidth /= 8;
        }
        bandwidth += 12;

        /*对于摄像头来说，带宽估计值太低。不要使用小于1024字节的最大数据包大小来尝试解决此问题。
         * 根据在两种不同摄像头模型上进行测量，该值足够高，可以使大多数分辨率工作，同时不会以15fps
         * 的速度阻止两个同时出现的VGA流。*/
        bandwidth = max((uint32_t)bandwidth, 1024);

        p_ctrl->max_payload_transfer_size = bandwidth;
    }
}

/**
 * \brief 初始化 USB 视频类控件结构体，向给定设备添加控件信息和硬编码的库存控制映射
 */
static void __vc_ctrl_init(struct usbh_vc *p_vc, struct usbh_vc_ctrl *p_ctrl){
    /* UVC 控件信息表*/
    const struct usbh_vc_ctrl_info    *p_info    = uvc_ctrls_info;
    /* UVC 控件信息表末尾*/
    const struct usbh_vc_ctrl_info    *p_iend    = p_info + USB_NELEMENTS(uvc_ctrls_info);
    /* UVC 控件映射*/
    const struct usbh_vc_ctrl_mapping *p_mapping = uvc_ctrl_mappings;
    /* UVC 控件映射末尾*/
    const struct usbh_vc_ctrl_mapping *p_mend    = p_mapping + USB_NELEMENTS(uvc_ctrl_mappings);

    /* 扩展单元 控件初始化需要查询设备以获取控件信息。由于某些有缺陷的UVC设备在紧循环
     * 中重复查询时会崩溃，因此将 扩展单元控件初始化延迟到首次使用。
     */
    if (UVC_ENTITY_TYPE(p_ctrl->p_entity) == UVC_VC_EXTENSION_UNIT) {
        return;
    }
    /* 遍历控件信息表*/
    for (; p_info < p_iend; ++p_info) {
        /* 匹配实体的 GUID 和控件索引*/
        if (__vc_entity_match_guid(p_ctrl->p_entity, p_info->entity) &&
                p_ctrl->index == p_info->index) {
            /* 匹配了添加控件信息*/
            __vc_ctrl_info_add(p_vc, p_ctrl, p_info);
            break;
        }
    }
    /* 检查控件结构体初始化位*/
    if (!p_ctrl->initialized) {
        return;
    }
    /* 遍历控件映射*/
    for (; p_mapping < p_mend; ++p_mapping) {
        /* 匹配实体的 GUID 和控件选择器*/
        if (__vc_entity_match_guid(p_ctrl->p_entity, p_mapping->entity) &&
        		p_ctrl->info.selector == p_mapping->selector) {
            /* 匹配了添加控件映射*/
            __vc_ctrl_mapping_add(p_vc, p_ctrl, p_mapping);
        }
    }
}

/**
 * \brief 寻找视频链控件
 */
static struct usbh_vc_ctrl *__vc_chain_ctrl_get(struct usbh_vc_video_chain   *p_chain,
                                                uint32_t                      idx,
                                                struct usbh_vc_ctrl_mapping **p_mapping){
    struct usb_list_node  *p_node   = NULL;
    struct usbh_vc_ctrl   *p_ctrl   = NULL;
    struct usbh_vc_entity *p_entity = NULL;
    int                    next     = idx & UVC_CTRL_FLAG_NEXT_CTRL;

    *p_mapping = NULL;
    idx &= UVC_CTRL_ID_MASK;

    /* 寻找控件*/
    usb_list_for_each_node(p_node, &p_chain->entities) {
        p_entity = usb_container_of(p_node, struct usbh_vc_entity, chain);

        __vc_ctrl_get(p_entity, idx, p_mapping, &p_ctrl, next);
        if (p_ctrl && !next) {
            return p_ctrl;
        }
    }

    if (p_ctrl == NULL && !next) {
        return NULL;
    }
    return p_ctrl;
}

/**
 * \brief 获取控件值
 */
static int __vc_ctrl_value_get(struct usbh_vc_video_chain  *p_chain,
                               struct usbh_vc_ctrl         *p_ctrl,
                               struct usbh_vc_ctrl_mapping *p_mapping,
                               int                         *p_value){
    struct uvc_menu_info *p_menu;
    unsigned int          i;
    int                   ret;

    /* 检查控件是否有获取当前值的功能*/
    if ((p_ctrl->info.flags & UVC_CTRL_FLAG_GET_CUR) == 0){
        return -USB_ENOTSUP;
    }
    if (!p_ctrl->loaded) {
        ret = __vc_ctrl_query(p_chain->p_vc,
                              UVC_GET_CUR,
                              p_ctrl->p_entity->id,
                              USBH_INTF_NUM_GET(p_chain->p_vc->p_ctrl_intf),
                              p_ctrl->info.selector,
                            __vc_ctrl_data_get(p_ctrl, UVC_CTRL_DATA_CURRENT),
                              p_ctrl->info.size, 300);
        if (ret < 0) {
            return ret;
        }

        p_ctrl->loaded = 1;
    }

    *p_value = p_mapping->p_fn_get(p_mapping, UVC_GET_CUR, __vc_ctrl_data_get(p_ctrl, UVC_CTRL_DATA_CURRENT));

    if (p_mapping->ctrl_type == UVC_CTRL_TYPE_MENU) {
    	p_menu = p_mapping->p_menu_info;
        for (i = 0; i < p_mapping->menu_count; ++i, ++p_menu) {
            if (p_menu->value == (uint32_t)*p_value) {
                *p_value = i;
                break;
            }
        }
    }

    return USB_OK;
}

/**
 * \brief 设置视频链控件值
 *
 * \param[in] p_chain 视频链
 * \param[in] id      控件ID
 * \param[in] value   控件值
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_chain_ctrl_set(struct usbh_vc_video_chain *p_chain,
                           int                         id,
                           int                         value){
    struct usbh_vc_ctrl         *p_ctrl;
    struct usbh_vc_ctrl_mapping *p_mapping;
    uint32_t                     step;
    int                          ret, min, max;

    /* 寻找控件*/
    p_ctrl = __vc_chain_ctrl_get(p_chain, id, &p_mapping);
    if (p_ctrl == NULL){
        return -USB_ENODEV;
    }
    /* 是否可以设置值*/
    if (!(p_ctrl->info.flags & UVC_CTRL_FLAG_SET_CUR)) {
        return -USB_EPERM;
    }
    /* 挑出超出范围的值*/
    switch (p_mapping->ctrl_type) {
        case UVC_CTRL_TYPE_INTEGER:
            if (!p_ctrl->cached) {
                ret = __vc_ctrl_populate_cache(p_chain, p_ctrl);
                if (ret != USB_OK) {
                    return ret;
                }
            }

            min  = p_mapping->p_fn_get(p_mapping, UVC_GET_MIN, __vc_ctrl_data_get(p_ctrl, UVC_CTRL_DATA_MIN));
            max  = p_mapping->p_fn_get(p_mapping, UVC_GET_MAX, __vc_ctrl_data_get(p_ctrl, UVC_CTRL_DATA_MAX));
            step = p_mapping->p_fn_get(p_mapping, UVC_GET_RES, __vc_ctrl_data_get(p_ctrl, UVC_CTRL_DATA_RES));

            if (step == 0) {
                step = 1;
            }
            value = min + ((uint32_t)(value - min) + step / 2) / step * step;

            if (p_mapping->data_type == UVC_CTRL_DATA_TYPE_SIGNED) {
                value = usb_clamp(value, min, max);
            } else {
                value = usb_clamp(value, min, max);
            }
            break;

        case UVC_CTRL_TYPE_BOOLEAN:
            value = usb_clamp(value, 0, 1);
            break;

        case UVC_CTRL_TYPE_MENU:
            if ((value < 0) || ((uint32_t)value >= p_mapping->menu_count)){
                return -USB_ESIZE;
            }
            value = p_mapping->p_menu_info[value].value;

            /* Valid menu indices are reported by the GET_RES request for
             * UVC controls that support it.*/
            if ((p_mapping->data_type == UVC_CTRL_DATA_TYPE_BITMASK) &&
                        (p_ctrl->info.flags & UVC_CTRL_FLAG_GET_RES)) {
                if (!p_ctrl->cached) {
                    ret = __vc_ctrl_populate_cache(p_chain, p_ctrl);
                    if (ret != USB_OK) {
                        return ret;
                    }
                }

                step = p_mapping->p_fn_get(p_mapping, UVC_GET_RES, __vc_ctrl_data_get(p_ctrl, UVC_CTRL_DATA_RES));
                if (!(step & value)) {
                    return -USB_EILLEGAL;
                }
            }

            break;

        default:
            break;
    }

    /* If the mapping doesn't span the whole UVC control, the current value
     * needs to be loaded from the device to perform the read-modify-write
     * operation.*/
    if (!p_ctrl->loaded && (p_ctrl->info.size * 8) != p_mapping->size) {
        if ((p_ctrl->info.flags & UVC_CTRL_FLAG_GET_CUR) == 0) {
            memset(__vc_ctrl_data_get(p_ctrl, UVC_CTRL_DATA_CURRENT), 0, p_ctrl->info.size);
        } else {
            ret = __vc_ctrl_query(p_chain->p_vc,
                                  UVC_GET_CUR,
								  p_ctrl->p_entity->id,
								  USBH_INTF_NUM_GET(p_chain->p_vc->p_ctrl_intf),
                                  p_ctrl->info.selector,
                                  __vc_ctrl_data_get(p_ctrl, UVC_CTRL_DATA_CURRENT),
                                  p_ctrl->info.size,
                                  300);
            if (ret < 0) {
                return ret;
            }
        }

        p_ctrl->loaded = 1;
    }

    /* 备份旧的值，以防我们要设置回去*/
    if (!p_ctrl->dirty) {
        memcpy(__vc_ctrl_data_get(p_ctrl, UVC_CTRL_DATA_BACKUP),
               __vc_ctrl_data_get(p_ctrl, UVC_CTRL_DATA_CURRENT), p_ctrl->info.size);
    }
    /* 设置值*/
    p_mapping->p_fn_set(p_mapping, value, __vc_ctrl_data_get(p_ctrl, UVC_CTRL_DATA_CURRENT));
    /* 设置已被修改标记*/
    p_ctrl->dirty = 1;
    p_ctrl->modified = 1;
    return USB_OK;
}

/**
 * \brief 获取视频链控件值
 *
 * \param[in] p_chain 视频链
 * \param[in] id      控件 ID
 * \param[in] p_value 储存控制值的缓存
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_chain_ctrl_get(struct usbh_vc_video_chain *p_chain,
                           int                         id,
                           int                        *p_value){
    struct usbh_vc_ctrl         *p_ctrl    = NULL;
    struct usbh_vc_ctrl_mapping *p_mapping = NULL;

    p_ctrl = __vc_chain_ctrl_get(p_chain, id, &p_mapping);
    if (p_ctrl == NULL) {
        return -USB_ENODEV;
    }
    return __vc_ctrl_value_get(p_chain, p_ctrl, p_mapping, p_value);
}

/**
 * \brief USB 视频类视频控件获取
 *
 * \param[in] p_stream USB 视频类视频流结构体
 * \param[in] p_ctrl   视频流控制结构体
 * \param[in] probe    是否是探测
 * \param[in] query    查询代码
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_video_ctrl_get(struct usbh_vc_stream  *p_stream,
                           struct uvc_stream_ctrl *p_ctrl,
                           int                     probe,
                           uint8_t                 query){

    uint8_t  data[34] = {0};
    uint16_t size;
    int      ret;

    /* 根据 USB 视频类规范版本申请不同内存大小的缓存*/
    size = p_stream->p_vc->uvc_version >= 0x0110 ? 34 : 26;

    /* 查询控件请求*/
    ret = __vc_ctrl_query(p_stream->p_vc, query, 0,
                          p_stream->intf_num,
                          probe ? UVC_VS_PROBE_CONTROL : UVC_VS_COMMIT_CONTROL,
                          data,
                          size,
                          5000);
    if (ret < 0) {
        return ret;
    }

    if ((query == UVC_GET_MIN || query == UVC_GET_MAX) && (ret == 2)) {
        /* 一些摄像头主要基于 Bison 电子芯片组，仅用 comp_quality 字段回答 GET_MAX 请求。*/
    	__USB_INFO("usb vedio class non compliance - GET_MIN/MAX(PROBE) incorrectly "
                          "supported. enabling workaround\r\n");
        memset(p_ctrl, 0, sizeof(struct uvc_stream_ctrl));
        p_ctrl->comp_quality = *((uint16_t *)data);

        return USB_OK;
    } else if ((query == UVC_GET_DEF) && (probe == 1) && (ret != size)) {
         /* 许多摄像头不支持其视频探测控件上的 GET_DEF 请求。警告一次并且返回
         * 调用这将返回到 GET_CUR。*/
    	__USB_ERR_INFO("usb vedio class non compliance - GET_DEF(PROBE) not supported. "
                          "enabling workaround\r\n");
        return -USB_EILLEGAL;
    } else if (ret != size) {
        __USB_ERR_INFO("query (%u) usb vedio class %s control %d (exp. %u) failed\r\n",
                         query, probe ? "probe" : "commit", ret, size);
        return -USB_EILLEGAL;
    }

    p_ctrl->hint                      = *((uint16_t *)&data[0]);
    p_ctrl->format_index              = data[2];
    p_ctrl->frame_index               = data[3];
    p_ctrl->frame_interval            = *((uint32_t *)&data[4]);
    p_ctrl->key_frame_rate            = *((uint16_t *)&data[8]);
    p_ctrl->frame_rate                = *((uint16_t *)&data[10]);
    p_ctrl->comp_quality              = *((uint16_t *)&data[12]);
    p_ctrl->comp_window_size          = *((uint16_t *)&data[14]);
    p_ctrl->delay                     = *((uint16_t *)&data[16]);
    p_ctrl->max_video_frame_size      = usb_get_unaligned_le32(&data[18]);
    p_ctrl->max_payload_transfer_size = usb_get_unaligned_le32(&data[22]);

    if (size == 34) {
    	p_ctrl->clock_frequency  = usb_get_unaligned_le32(&data[26]);
    	p_ctrl->framing_info     = data[30];
    	p_ctrl->prefered_version = data[31];
    	p_ctrl->min_version      = data[32];
    	p_ctrl->max_version      = data[33];
    } else {
    	p_ctrl->clock_frequency  = p_stream->p_vc->clk_freq;
    	p_ctrl->framing_info     = 0;
    	p_ctrl->prefered_version = 0;
    	p_ctrl->min_version      = 0;
    	p_ctrl->max_version      = 0;
    }
    /* 一些损坏的设备返回空或错误的 max_video_frame_size 和 max_payload_transfer_size 字段。
     * 尝试从格式和帧描述符中获取值。*/
    __vc_vedio_ctrl_fixup(p_stream, p_ctrl);

    return USB_OK;
}

/**
 * \brief USB 视频类设置视频控件
 *
 * \param[in] p_stream USB 视频类视频流结构体
 * \param[in] p_ctrl   视频流控制结构体
 * \param[in] probe    是否是探测
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_video_ctrl_set(struct usbh_vc_stream  *p_stream,
                           struct uvc_stream_ctrl *p_ctrl,
                           int                     probe){

    uint8_t  data[34] = {0};
    uint16_t size;
    int      ret;

    size = p_stream->p_vc->uvc_version >= 0x0110 ? 34 : 26;

    *(uint16_t *)&data[0]  = USB_CPU_TO_LE16(p_ctrl->hint);
    data[2]                = p_ctrl->format_index;
    data[3]                = p_ctrl->frame_index;
    *(uint32_t *)&data[4]  = USB_CPU_TO_LE32(p_ctrl->frame_interval);
    *(uint16_t *)&data[8]  = USB_CPU_TO_LE16(p_ctrl->key_frame_rate);
    *(uint16_t *)&data[10] = USB_CPU_TO_LE16(p_ctrl->frame_rate);
    *(uint16_t *)&data[12] = USB_CPU_TO_LE16(p_ctrl->comp_quality);
    *(uint16_t *)&data[14] = USB_CPU_TO_LE16(p_ctrl->comp_window_size);
    *(uint16_t *)&data[16] = USB_CPU_TO_LE16(p_ctrl->delay);
    *(uint32_t *)&data[18] = USB_CPU_TO_LE32(p_ctrl->max_video_frame_size);
    *(uint32_t *)&data[22] = USB_CPU_TO_LE32(p_ctrl->max_payload_transfer_size);

    if (size == 34) {
        *(uint32_t *)&data[26] = USB_CPU_TO_LE32(p_ctrl->clock_frequency);
        data[30] = p_ctrl->framing_info;
        data[31] = p_ctrl->prefered_version;
        data[32] = p_ctrl->min_version;
        data[33] = p_ctrl->max_version;
    }
    /* 查询控件请求*/
    ret = __vc_ctrl_query(p_stream->p_vc,
                          UVC_SET_CUR,
                          0,
                          p_stream->intf_num,
                          probe ? UVC_VS_PROBE_CONTROL : UVC_VS_COMMIT_CONTROL,
                          data,
                          size,
                          5000);
    if (ret != size) {
        return ret;
    }

    return USB_OK;
}

/**
 * \brief USB 视频类设备控件初始化
 *
 * \param[in] p_vc USB 视频类设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_ctrl_init(struct usbh_vc *p_vc){
    struct usb_list_node  *p_node     = NULL;
    struct usb_list_node  *p_node_tmp = NULL;
    struct usbh_vc_entity *p_entity   = NULL;
    uint32_t               i;

    if (p_vc == NULL) {
        return -USB_EINVAL;
    }

    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_vc->entities){
        struct usbh_vc_ctrl *p_ctrl       = NULL;
        uint32_t             control_size = 0, n_controls;
        uint8_t             *p_controls   = NULL;

        p_entity = usb_container_of(p_node, struct usbh_vc_entity, node);

        /* 检查实体类型*/
        if (UVC_ENTITY_TYPE(p_entity) == UVC_VC_EXTENSION_UNIT) {      /* 扩展单元*/
            /* 获取扩展单元控件位图及位图大小*/
            p_controls   = p_entity->extension.p_controls;
            control_size = p_entity->extension.control_size;
        } else if (UVC_ENTITY_TYPE(p_entity) == UVC_VC_PROCESSING_UNIT) {   /* 处理单元*/
            /* 获取处理单元控件位图及位图大小*/
            p_controls   = p_entity->processing.p_controls;
            control_size = p_entity->processing.control_size;
        } else if (UVC_ENTITY_TYPE(p_entity) == UVC_ITT_CAMERA) {      /* 摄像头终端*/
            /* 获取摄像头终端控件位图及位图大小*/
            p_controls   = p_entity->camera.p_controls;
            control_size = p_entity->camera.control_size;
        }
        /* 移除假/黑名单控件*/
        __vc_ctrl_entity_prune(p_vc, p_entity);

        /* 计算支持的控件的数量和分配控件矩阵*/
        n_controls = __vc_memweight(p_controls, control_size);
        if (n_controls == 0){
            continue;
        }

        p_entity->p_controls = usb_lib_malloc(&__g_uvc_lib.lib, n_controls * sizeof(struct usbh_vc_ctrl));
        if (p_entity->p_controls == NULL) {
            return -USB_ENOMEM;
        }
        memset(p_entity->p_controls, 0, n_controls * sizeof(struct usbh_vc_ctrl));

        /* 设置控件数量*/
        p_entity->n_controls = n_controls;

        /* 初始化所有可支持的控件*/
        p_ctrl = p_entity->p_controls;
        for (i = 0; i < control_size * 8; ++i) {
            if (usb_test_bit(p_controls, i) == 0) {
                continue;
            }
            p_ctrl->p_entity = p_entity;
            /* 设置控件索引*/
            p_ctrl->index = i;
            /* 初始化控件*/
            __vc_ctrl_init(p_vc, p_ctrl);
            /* 移动到下一个控件结构体*/
            p_ctrl++;
        }
    }
    return USB_OK;
}

/**
 * \brief 清除控件映射
 */
static void __vc_ctrl_mappings_cleanup(struct usbh_vc      *p_vc,
                                       struct usbh_vc_ctrl *p_ctrl){
    struct usb_list_node        *p_node     = NULL;
    struct usb_list_node        *p_node_tmp = NULL;
    struct usbh_vc_ctrl_mapping *p_mapping  = NULL;

    usb_list_for_each_node_safe(p_node,
                                p_node_tmp,
                               &p_ctrl->info.mappings) {
        p_mapping = usb_container_of(p_node, struct usbh_vc_ctrl_mapping, node);

        usb_list_node_del(&p_mapping->node);
        usb_lib_mfree(&__g_uvc_lib.lib, p_mapping->p_menu_info);
        usb_lib_mfree(&__g_uvc_lib.lib, p_mapping);
    }
}

/**
 * \brief USB 视频类设备控件反初始化
 *
 * \param[in] p_vc USB 视频类设备结构体
 *
 * \retval 成功返回 USB_OK
 */
int usbh_vc_ctrl_deinit(struct usbh_vc *p_vc){
    struct usb_list_node  *p_node   = NULL;
    struct usbh_vc_entity *p_entity = NULL;
    unsigned int           i;

    if (p_vc == NULL) {
        return -USB_EINVAL;
    }

    /* 释放所有实体的控件和控件映射 */
    usb_list_for_each_node(p_node, &p_vc->entities) {
        p_entity = usb_container_of(p_node, struct usbh_vc_entity, node);

        for (i = 0; i < p_entity->n_controls; ++i) {
            struct usbh_vc_ctrl *p_ctrl = &p_entity->p_controls[i];
            /* 检查控件初始化标志*/
            if (!p_ctrl->initialized) {
                continue;
            }
            /* 清除控件映射*/
            __vc_ctrl_mappings_cleanup(p_vc, p_ctrl);
            usb_lib_mfree(&__g_uvc_lib.lib, p_ctrl->p_uvc_data);
        }
        if (p_entity->p_controls) {
        	usb_lib_mfree(&__g_uvc_lib.lib, p_entity->p_controls);
        }
    }
    return USB_OK;
}
