/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "core/include/host/class/hid/usbh_hid_drv.h"
#include "core/include/usb_mem.h"
#include "utils/refcnt/usb_refcnt.h"
#include <string.h>
#include <stdio.h>

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern struct uhid_lib __g_uhid_lib;

/*******************************************************************************
 * Code
 ******************************************************************************/
/**
 * \brief 获取条目无符号数据
 */
static uint32_t __item_udata_get(struct usbh_hid_item *p_item){
    switch (p_item->size) {
        case 1: return p_item->data.u8;
        case 2: return p_item->data.u16;
        case 4: return p_item->data.u32;
    }
    return 0;
}

/**
 * \brief 获取条目有符号数据
 */
static int32_t __item_sdata_get(struct usbh_hid_item *p_item){
    switch (p_item->size) {
        case 1: return p_item->data.s8;
        case 2: return p_item->data.s16;
        case 4: return p_item->data.s32;
    }
    return 0;
}

/**
 * \brief 获取条目信息
 */
static uint8_t *__item_fetch(uint8_t              *p_start,
                             uint8_t              *p_end,
                             struct usbh_hid_item *p_item){
    uint8_t b;

    if ((p_end - p_start) <= 0) {
        return NULL;
    }

    b = *p_start++;

    p_item->type = (b >> 2) & 3;
    p_item->tag  = (b >> 4) & 15;

    if (p_item->tag == HID_ITEM_TAG_LONG) {
        p_item->format = HID_ITEM_FORMAT_LONG;

        if ((p_end - p_start) < 2) {
            return NULL;
        }
        p_item->size = *p_start++;
        p_item->tag  = *p_start++;

        if ((p_end - p_start) < p_item->size) {
            return NULL;
        }

        p_item->data.p_longdata = p_start;

        p_start += p_item->size;

        return p_start;
	}

    p_item->format = HID_ITEM_FORMAT_SHORT;
    p_item->size   = b & 3;

    switch (p_item->size) {
        case 0:
            return p_start;
        case 1:
            if ((p_end - p_start) < 1) {
                return NULL;
            }
            p_item->data.u8 = *p_start++;
            return p_start;
        case 2:
            if ((p_end - p_start) < 2) {
                return NULL;
            }
            p_item->data.u16 = usb_get_unaligned_le16(p_start);
		    p_start = (uint8_t *)((uint16_t *)p_start + 1);
		    return p_start;
        case 3:
        	p_item->size++;
            if ((p_end - p_start) < 4) {
                return NULL;
            }
            p_item->data.u32 = usb_get_unaligned_le32(p_start);
            p_start = (uint8_t *)((uint32_t *)p_start + 1);
            return p_start;
	}

    return NULL;
}

/**
 * \brief 往临时的分析表里添加一个用法
 */
static int __hid_usage_add(struct usbh_hid_parser *p_parser, uint32_t usage){
    if (p_parser->local.usage_index >= HID_MAX_USAGES) {
        __USB_ERR_INFO("human interface device usage index exceeded\r\n");
        return -USB_EPERM;
    }
    p_parser->local.usage[p_parser->local.usage_index] = usage;
    p_parser->local.collection_index[p_parser->local.usage_index] =
    p_parser->collection_stack_ptr ? p_parser->collection_stack[p_parser->collection_stack_ptr - 1] : 0;
    p_parser->local.usage_index++;
	return USB_OK;
}

/**
 * \brief USB 人体接口设备集合扫描
 */
static void __hid_collection_scan(struct usbh_hid_parser *p_parser, uint32_t type){
    struct usbh_hid *p_hid = p_parser->p_hid;
    int              i;

    if (((p_parser->global.usage_page << 16) == HID_UP_SENSOR) && type == HID_COLLECTION_PHYSICAL) {
        p_hid->group = HID_GROUP_SENSOR_HUB;
	}
    if ((USBH_DEV_VID_GET(p_hid->p_usb_fun) == USB_VENDOR_ID_MICROSOFT) &&
            ((USBH_DEV_PID_GET(p_hid->p_usb_fun) == USB_DEVICE_ID_MS_TYPE_COVER_3) ||
             (USBH_DEV_PID_GET(p_hid->p_usb_fun) == USB_DEVICE_ID_MS_TYPE_COVER_3_JP)) &&
             (p_hid->group == HID_GROUP_MULTITOUCH)) {
        p_hid->group = HID_GROUP_GENERIC;
    }
    if ((p_parser->global.usage_page << 16) == HID_UP_GENDESK) {
        for (i = 0; i < p_parser->local.usage_index; i++) {
            if (p_parser->local.usage[i] == HID_GD_POINTER) {
				p_parser->scan_flags |= HID_SCAN_FLAG_GD_POINTER;
            }
        }
    }
    if ((p_parser->global.usage_page << 16) >= HID_UP_MSVENDOR) {
        p_parser->scan_flags |= HID_SCAN_FLAG_VENDOR_SPECIFIC;
    }
}

/**
 * \brief USB 人体接口设备打开一个集合
 */
static int __hid_collection_open(struct usbh_hid_parser *p_parser, uint32_t type){
    struct usbh_hid_collection *p_collection = NULL;
    uint32_t                    usage;

    usage = p_parser->local.usage[0];

    if (p_parser->collection_stack_ptr == HID_COLLECTION_STACK_SIZE) {
        __USB_ERR_INFO("human interface device collection stack overflow\r\n");
        return -USB_EPERM;
    }

    if (p_parser->p_hid->collection_max == p_parser->p_hid->collection_size) {
        p_collection = usb_lib_malloc(&__g_uhid_lib.lib,
                                      sizeof(struct usbh_hid_collection) * p_parser->p_hid->collection_size * 2);
        if (p_collection == NULL) {
            return -USB_ENOMEM;
        }
		memcpy(p_collection, p_parser->p_hid->p_collection,
               sizeof(struct usbh_hid_collection) * p_parser->p_hid->collection_size);
        memset(p_collection + p_parser->p_hid->collection_size, 0,
               sizeof(struct usbh_hid_collection) * p_parser->p_hid->collection_size);

        usb_lib_mfree(&__g_uhid_lib.lib, p_parser->p_hid->p_collection);

        p_parser->p_hid->p_collection     = p_collection;
        p_parser->p_hid->collection_size *= 2;
	}

    p_parser->collection_stack[p_parser->collection_stack_ptr++] = p_parser->p_hid->collection_max;

    p_collection = p_parser->p_hid->p_collection + p_parser->p_hid->collection_max++;
    p_collection->type  = type;
    p_collection->usage = usage;
    p_collection->level = p_parser->collection_stack_ptr - 1;

    if (type == HID_COLLECTION_APPLICATION) {
    	p_parser->p_hid->application_max++;
    }
    return USB_OK;
}

/**
 * \brief USB 人体接口设备关闭一个集合
 */
static int __hid_collection_close(struct usbh_hid_parser *p_parser){
    if (!p_parser->collection_stack_ptr) {
        __USB_ERR_INFO("human interface device collection stack overflow\r\n");
        return -USB_EPERM;
	}
    p_parser->collection_stack_ptr--;
    return USB_OK;
}

/**
 * \brief 往 USB 人体接口设备中注册一个新的报告
 */
static struct usbh_hid_report *__hid_report_register(struct usbh_hid *p_hid,
                                                     uint32_t         type,
                                                     uint32_t         id){
    struct usbh_hid_report_enum *p_report_enum = p_hid->report_enum + type;
    struct usbh_hid_report      *p_report      = NULL;

    if (id >= HID_MAX_IDS) {
        return NULL;
    }
    if (p_report_enum->p_report_id_hash[id]) {
        return p_report_enum->p_report_id_hash[id];
    }
    p_report = usb_lib_malloc(&__g_uhid_lib.lib, sizeof(struct usbh_hid_report));
    if (p_report == NULL) {
        return NULL;
    }
    if (id != 0) {
    	p_report_enum->numbered = 1;
	}
    p_report->id    = id;
    p_report->type  = type;
    p_report->size  = 0;
    p_report->p_hid = p_hid;
    p_report_enum->p_report_id_hash[id] = p_report;

    usb_list_add_tail(&p_report->node, &p_report_enum->report_list);

    return p_report;
}

/**
 * \brief 往一个报告里注册一个新的域
 */
static struct usbh_hid_field *__hid_field_register(struct usbh_hid_report *p_report,
                                                   uint32_t                usages,
                                                   uint32_t                values){
    struct usbh_hid_field *p_field = NULL;

    if (p_report->field_max == HID_MAX_FIELDS) {
        __USB_ERR_INFO("human interface device have too many fields in report\r\n");
		return NULL;
	}

    p_field = usb_lib_malloc(&__g_uhid_lib.lib,
                             (sizeof(struct usbh_hid_field) + usages * sizeof(struct usbh_hid_usage) + values * sizeof(unsigned)));
    if (p_field == NULL) {
        return NULL;
    }
    p_field->idx                    = p_report->field_max++;
    p_report->p_field[p_field->idx] = p_field;
    p_field->p_usage                = (struct usbh_hid_usage *)(p_field + 1);
    p_field->p_value                = (int32_t *)(p_field->p_usage + usages);
    p_field->p_report               = p_report;

    return p_field;
}

/**
 * \brief 往报告中添加一个新的域
 */
static int __hid_field_add(struct usbh_hid_parser *p_parser,
                           uint32_t                report_type,
                           uint32_t                flags){
    struct usbh_hid_report *p_report = NULL;
    struct usbh_hid_field  *p_field  = NULL;
    uint32_t                usages;
    uint32_t                offset;
    uint32_t                i;

    p_report = __hid_report_register(p_parser->p_hid, report_type, p_parser->global.report_id);
    if (p_report != NULL) {
        __USB_ERR_INFO("human interface device report register failed\r\n");
        return -USB_EPERM;
	}

	/* Handle both signed and unsigned cases properly */
    if ((p_parser->global.logical_minimum < 0 &&
            p_parser->global.logical_maximum < p_parser->global.logical_minimum) ||
		   (p_parser->global.logical_minimum >= 0 &&
                   (uint32_t)p_parser->global.logical_maximum < (uint32_t)p_parser->global.logical_minimum)) {
        __USB_ERR_INFO("human interface device logical range invalid 0x%x 0x%x\r\n",
                    p_parser->global.logical_minimum, p_parser->global.logical_maximum);
        return -USB_EILLEGAL;
	}

    offset = p_report->size;
    p_report->size += p_parser->global.report_size * p_parser->global.report_count;
    /* Ignore padding fields */
    if (!p_parser->local.usage_index) {
        return USB_OK;
    }

    usages = max(p_parser->local.usage_index, p_parser->global.report_count);
    /* 注册一个域*/
    p_field = __hid_field_register(p_report, usages, p_parser->global.report_count);
    if (p_field == NULL) {
        return -USB_EPERM;
    }
//
//	field->physical = hid_lookup_collection(parser, HID_COLLECTION_PHYSICAL);
//	field->logical = hid_lookup_collection(parser, HID_COLLECTION_LOGICAL);
//	field->application = hid_lookup_collection(parser, HID_COLLECTION_APPLICATION);
//
//	for (i = 0; i < usages; i++) {
//		unsigned j = i;
//		/* Duplicate the last usage we parsed if we have excess values */
//		if (i >= parser->local.usage_index)
//			j = parser->local.usage_index - 1;
//		field->usage[i].hid = parser->local.usage[j];
//		field->usage[i].collection_index =
//			parser->local.collection_index[j];
//		field->usage[i].usage_index = i;
//	}
//
//	field->maxusage = usages;
//	field->flags = flags;
//	field->report_offset = offset;
//	field->report_type = report_type;
//	field->report_size = parser->global.report_size;
//	field->report_count = parser->global.report_count;
//	field->logical_minimum = parser->global.logical_minimum;
//	field->logical_maximum = parser->global.logical_maximum;
//	field->physical_minimum = parser->global.physical_minimum;
//	field->physical_maximum = parser->global.physical_maximum;
//	field->unit_exponent = parser->global.unit_exponent;
//	field->unit = parser->global.unit;

	return 0;
}

static void __hid_input_usage_scan(struct usbh_hid_parser *p_parser, uint32_t usage){
    struct usbh_hid *p_hid = p_parser->p_hid;

    if (usage == HID_DG_CONTACTID) {
    	p_hid->group = HID_GROUP_MULTITOUCH;
    }
}

static void __hid_feature_usage_scan(struct usbh_hid_parser *p_parser, uint32_t usage){
    if ((usage == 0xff0000c5) && (p_parser->global.report_count == 256) &&
            (p_parser->global.report_size == 8)) {
    	p_parser->scan_flags |= HID_SCAN_FLAG_MT_WIN_8;
    }
}

/**
 * \brief USB 人体接口设备扫描主条目
 */
static int __hid_main_item_scan(struct usbh_hid_parser *p_parser,
                                struct usbh_hid_item   *p_item){
    uint32_t data;
    int      i;

    data = __item_udata_get(p_item);

    switch (p_item->tag) {
        case HID_MAIN_ITEM_TAG_BEGIN_COLLECTION:
            __hid_collection_scan(p_parser, data & 0xff);
            break;
        case HID_MAIN_ITEM_TAG_END_COLLECTION:
            break;
        case HID_MAIN_ITEM_TAG_INPUT:
            /* ignore constant inputs, they will be ignored by hid-input */
           if (data & HID_MAIN_ITEM_CONSTANT) {
               break;
           }
           for (i = 0; i < p_parser->local.usage_index; i++) {
        	   __hid_input_usage_scan(p_parser, p_parser->local.usage[i]);
           }
           break;
       case HID_MAIN_ITEM_TAG_OUTPUT:
           break;
       case HID_MAIN_ITEM_TAG_FEATURE:
           for (i = 0; i < p_parser->local.usage_index; i++) {
               __hid_feature_usage_scan(p_parser, p_parser->local.usage[i]);
           }
           break;
    }
    /* 复位本地分析器环境 */
    memset(&p_parser->local, 0, sizeof(p_parser->local));

	return USB_OK;
}

/**
 * \brief 处理一个主条目
 */
static int __hid_parser_main_item_process(struct usbh_hid_parser *p_parser,
                                          struct usbh_hid_item   *p_item){
    uint32_t data;
    int      ret;

    data = __item_udata_get(p_item);

    switch (p_item->tag) {
        case HID_MAIN_ITEM_TAG_BEGIN_COLLECTION:
            ret = __hid_collection_open(p_parser, data & 0xff);
            break;
        case HID_MAIN_ITEM_TAG_END_COLLECTION:
            ret = __hid_collection_close(p_parser);
            break;
        case HID_MAIN_ITEM_TAG_INPUT:
            ret = __hid_field_add(p_parser, HID_INPUT_REPORT, data);
            break;
        case HID_MAIN_ITEM_TAG_OUTPUT:
            ret = __hid_field_add(p_parser, HID_OUTPUT_REPORT, data);
            break;
        case HID_MAIN_ITEM_TAG_FEATURE:
            ret = __hid_field_add(p_parser, HID_FEATURE_REPORT, data);
            break;
        default:
        	__USB_INFO("human interface device main item tag 0x%x unknown\r\n", p_item->tag);
		    ret = USB_OK;
	}

	memset(&p_parser->local, 0, sizeof(p_parser->local));	/* Reset the local parser environment */

	return ret;
}


/**
 * \brief 处理 USB 人体接口设备全局条目
 */
static int __hid_parser_global_item_process(struct usbh_hid_parser *p_parser,
                                            struct usbh_hid_item   *p_item){

    uint32_t raw_value;

    switch (p_item->tag) {
        case HID_GLOBAL_ITEM_TAG_PUSH:

            if (p_parser->global_stack_ptr == HID_GLOBAL_STACK_SIZE) {
                __USB_ERR_INFO("human interface device global environment stack overflow\r\n");
                return -USB_EPERM;
		    }
            memcpy(p_parser->global_stack + p_parser->global_stack_ptr++,
                  &p_parser->global, sizeof(struct usbh_hid_global));
		    return USB_OK;

        case HID_GLOBAL_ITEM_TAG_POP:

		    if (p_parser->global_stack_ptr == 0) {
                __USB_ERR_INFO("human interface device global environment stack underflow\r\n");
                return -USB_EPERM;
		    }

            memcpy(&p_parser->global, p_parser->global_stack +
                  --p_parser->global_stack_ptr, sizeof(struct usbh_hid_global));
		    return USB_OK;

        case HID_GLOBAL_ITEM_TAG_USAGE_PAGE:
            p_parser->global.usage_page = __item_udata_get(p_item);
		    return USB_OK;

	    case HID_GLOBAL_ITEM_TAG_LOGICAL_MINIMUM:
	    	p_parser->global.logical_minimum = __item_sdata_get(p_item);
            return USB_OK;

        case HID_GLOBAL_ITEM_TAG_LOGICAL_MAXIMUM:
            if (p_parser->global.logical_minimum < 0) {
            	p_parser->global.logical_maximum = __item_sdata_get(p_item);
            } else {
            	p_parser->global.logical_maximum = __item_udata_get(p_item);
            }
            return USB_OK;

        case HID_GLOBAL_ITEM_TAG_PHYSICAL_MINIMUM:
        	p_parser->global.physical_minimum = __item_sdata_get(p_item);
		    return USB_OK;

	    case HID_GLOBAL_ITEM_TAG_PHYSICAL_MAXIMUM:
            if (p_parser->global.physical_minimum < 0) {
                p_parser->global.physical_maximum = __item_sdata_get(p_item);
            } else {
                p_parser->global.physical_maximum = __item_udata_get(p_item);
            }
            return USB_OK;

        case HID_GLOBAL_ITEM_TAG_UNIT_EXPONENT:
		    /* Many devices provide unit exponent as a two's complement
		     * nibble due to the common misunderstanding of HID
		     * specification 1.11, 6.2.2.7 Global Items. Attempt to handle
		     * both this and the standard encoding. */
            raw_value = __item_sdata_get(p_item);
            if (!(raw_value & 0xfffffff0)) {
                p_parser->global.unit_exponent = usb_sn_to_s32(raw_value, 4);
            } else {
            	p_parser->global.unit_exponent = raw_value;
            }
            return USB_OK;

        case HID_GLOBAL_ITEM_TAG_UNIT:
            p_parser->global.unit = __item_udata_get(p_item);
            return USB_OK;

        case HID_GLOBAL_ITEM_TAG_REPORT_SIZE:
            p_parser->global.report_size = __item_udata_get(p_item);
            if (p_parser->global.report_size > 128) {
                __USB_ERR_INFO("human interface device report size %d invalid\r\n", p_parser->global.report_size);
                return -USB_EPERM;
            }
            return USB_OK;

        case HID_GLOBAL_ITEM_TAG_REPORT_COUNT:
            p_parser->global.report_count = __item_udata_get(p_item);
            if (p_parser->global.report_count > HID_MAX_USAGES) {
            	 __USB_ERR_INFO("human interface device report count %d invalid\r\n", p_parser->global.report_count);
                return -USB_EPERM;
            }
            return USB_OK;

        case HID_GLOBAL_ITEM_TAG_REPORT_ID:
            p_parser->global.report_id = __item_udata_get(p_item);
            if ((p_parser->global.report_id == 0) ||
                (p_parser->global.report_id >= HID_MAX_IDS)) {
            	__USB_ERR_INFO("human interface device report id %d invalid\r\n", p_parser->global.report_id);
                return -USB_EPERM;
            }
            return USB_OK;

	    default:
            __USB_ERR_INFO("human interface device global tag 0x%x unknown\r\n", p_item->tag);
            return -USB_ENOTSUP;
	}
    return -USB_ENOTSUP;
}

/**
 * \brief 处理 USB 人体接口设备本地条目
 */
static int __hid_parser_local_item_process(struct usbh_hid_parser *p_parser,
                                           struct usbh_hid_item   *p_item){
    int      ret;
    uint32_t data, n;

    data = __item_udata_get(p_item);

    switch (p_item->tag) {
        case HID_LOCAL_ITEM_TAG_DELIMITER:
            if (data) {
                /* We treat items before the first delimiter
                 * as global to all usage sets (branch 0).
                 * In the moment we process only these global
                 * items and the first delimiter set.*/
                if (p_parser->local.delimiter_depth != 0) {
                    __USB_ERR_INFO("human interface device nested delimiters\r\n");
                    return -USB_EPERM;
                }
			    p_parser->local.delimiter_depth++;
			    p_parser->local.delimiter_branch++;
		    } else {
			    if (p_parser->local.delimiter_depth < 1) {
                    __USB_ERR_INFO("human interface device bogus close delimiter\r\n");
				    return -USB_EPERM;
			    }
			    p_parser->local.delimiter_depth--;
		    }
		    return USB_OK;

	    case HID_LOCAL_ITEM_TAG_USAGE:
            if (p_parser->local.delimiter_branch > 1) {
            	__USB_INFO("human interface device alternative usage ignored\r\n");
                return USB_OK;
            }

		    if (p_item->size <= 2) {
                data = (p_parser->global.usage_page << 16) + data;
		    }
		    return __hid_usage_add(p_parser, data);

	    case HID_LOCAL_ITEM_TAG_USAGE_MINIMUM:
		    if (p_parser->local.delimiter_branch > 1) {
                __USB_INFO("human interface device alternative usage ignored\r\n");
                return USB_OK;
		    }

            if (p_item->size <= 2) {
                data = (p_parser->global.usage_page << 16) + data;
            }
            p_parser->local.usage_minimum = data;
            return USB_OK;

	    case HID_LOCAL_ITEM_TAG_USAGE_MAXIMUM:

		    if (p_parser->local.delimiter_branch > 1) {
		    	__USB_INFO("human interface device alternative usage ignored\r\n");
			    return USB_OK;
		    }

            if (p_item->size <= 2) {
                data = (p_parser->global.usage_page << 16) + data;
		    }
		    for (n = p_parser->local.usage_minimum; n <= data; n++) {
                ret = __hid_usage_add(p_parser, n);
                if (ret != USB_OK) {
                	__USB_ERR_INFO("human interface device usage add failed(%d)\r\n");
                    return -USB_EPERM;
                }
            }
		    return USB_OK;

	    default:
            __USB_INFO("human interface device local item tag 0x%x unknown\r\n", p_item->tag);
		    return USB_OK;
	}
	return USB_OK;
}

/**
 * \brief 处理 USB 人体接口设备保留条目
 */
static int __hid_parser_reserved_item_process(struct usbh_hid_parser *p_parser,
                                              struct usbh_hid_item   *p_item){
	__USB_INFO("human interface device reserved item type, tag 0x%x\r\n", p_item->tag);
    return USB_OK;
}

/**
 * \brief USB 人体接口设备扫描报告描述符
 *
 * \param[in] p_hid USB 人体接口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_report_scan(struct usbh_hid *p_hid){
    struct usbh_hid_parser parser;
    struct usbh_hid_item   item;
    uint8_t               *p_start  = p_hid->p_report_desc;
    uint8_t               *p_end    = p_start + p_hid->report_desc_size;
	static int           (*p_dispatch_type[])(struct usbh_hid_parser *p_parser,
                                               struct usbh_hid_item   *p_item) = {
        __hid_main_item_scan,
		__hid_parser_global_item_process,
        __hid_parser_local_item_process,
		__hid_parser_reserved_item_process
	};

    parser.p_hid = p_hid;
    p_hid->group = HID_GROUP_GENERIC;

    /* 分析报告描述符每一个条目*/
    while ((p_start = __item_fetch(p_start, p_end, &item)) != NULL) {
        p_dispatch_type[item.type](&parser, &item);
    }

    if ((parser.scan_flags & HID_SCAN_FLAG_MT_WIN_8) &&
            (p_hid->group == HID_GROUP_MULTITOUCH)) {
        p_hid->group = HID_GROUP_MULTITOUCH_WIN_8;
    }
    /* 特殊的 VID 处理*/
    switch (USBH_DEV_VID_GET(p_hid->p_usb_fun)) {
        case USB_VENDOR_ID_WACOM:
        	p_hid->group = HID_GROUP_WACOM;
		    break;
        case USB_VENDOR_ID_SYNAPTICS:
            if (p_hid->group == HID_GROUP_GENERIC) {
                if ((parser.scan_flags & HID_SCAN_FLAG_VENDOR_SPECIFIC) &&
                        (parser.scan_flags & HID_SCAN_FLAG_GD_POINTER)) {
                	/* HID-RMI 类型*/
                    p_hid->group = HID_GROUP_RMI;
                }
            }
            break;
	}
	return USB_OK;
}

/**
 * \brief USB 人体接口设备分析报告描述符
 *
 * \param[in] p_hid USB 人体接口设备
 *
 * \retval 成功返回 USB_OK
 */
int usbh_hid_report_parse(struct usbh_hid *p_hid){
    struct usbh_hid_parser parser;
    struct usbh_hid_item   item;
    uint32_t               size;
	uint8_t               *p_start  = NULL;
    //uint8_t               *p_buf    = NULL;
    uint8_t               *p_end    = NULL;
	static int           (*p_dispatch_type[])(struct usbh_hid_parser *p_parser,
                                              struct usbh_hid_item   *p_item) = {
        __hid_parser_main_item_process,
        __hid_parser_global_item_process,
        __hid_parser_local_item_process,
        __hid_parser_reserved_item_process
	};

    p_start = p_hid->p_report_desc;
    if (p_start == NULL) {
        return -USB_EILLEGAL;
    }
    size = p_hid->report_desc_size;

    //todo 有些设备的报告描述符需要修复，这里暂时不做
    memset(&parser, 0, sizeof(struct usbh_hid_parser));

    parser.p_hid = p_hid;

    p_end = p_start + size;

    p_hid->p_collection = usb_lib_malloc(&__g_uhid_lib.lib,
                                          HID_DEFAULT_NUM_COLLECTIONS * sizeof(struct usbh_hid_collection));
    if (p_hid->p_collection == NULL) {
		return -USB_ENOMEM;
	}
    p_hid->collection_size = HID_DEFAULT_NUM_COLLECTIONS;

    while ((p_start = __item_fetch(p_start, p_end, &item)) != NULL) {

        if (item.format != HID_ITEM_FORMAT_SHORT) {
            __USB_ERR_INFO("human interface device unexpect long global item\r\n");
            return -USB_EILLEGAL;
        }

        if (p_dispatch_type[item.type](&parser, &item)) {
            __USB_ERR_INFO("human interface device item %u %u %u %u parse failed\r\n",
                            item.format, (unsigned)item.size, (unsigned)item.type, (unsigned)item.tag);
            return -USB_EPERM;
		}

        if (p_start == p_end) {
            if (parser.collection_stack_ptr) {
                __USB_ERR_INFO("human interface device unbalanced collection at end of report description\r\n");
                return -USB_EPERM;
			}
			if (parser.local.delimiter_depth) {
                __USB_ERR_INFO("human interface device unbalanced delimiter at end of report description\r\n");
                return -USB_EPERM;
			}
			//device->status |= HID_STAT_PARSED;

			return USB_OK;
		}
	}

    __USB_ERR_INFO("human interface device item fetching failed at offset %d\r\n", (int)(p_end - p_start));

	return -USB_EPERM;
}

/**
 * \brief USB 人体接口设备报告初始化
 *
 * \param[in] p_hid USB 人体接口设备
 *
 * \retval 成功返回 USB_OK
 */
void usbh_hid_report_init(struct usbh_hid *p_hid){
    uint32_t i, j;

    for (i = 0; i < HID_REPORT_TYPES; i++) {
        struct usbh_hid_report_enum *p_report_enum = p_hid->report_enum + i;

        for (j = 0; j < HID_MAX_IDS; j++) {
            memset(p_report_enum, 0, sizeof(struct usbh_hid_report_enum));
            usb_list_head_init(&p_report_enum->report_list);
        }
    }
}
