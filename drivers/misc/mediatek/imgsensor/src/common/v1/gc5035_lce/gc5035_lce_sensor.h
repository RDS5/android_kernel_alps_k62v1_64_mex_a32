/*
 * gc5035_lce_sensor.h
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * gc5035_luxvisions_sensor image sensor config settings
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#ifndef __GC5035_LCE_SENSOR_H
#define __GC5035_LCE_SENSOR_H

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <securec.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "imgsensor_sensor_common.h"
#include "imgsensor_sensor_i2c.h"

#define SENSOR_CISCTRL_CAPT_VB_REG_H 0x41
#define SENSOR_CISCTRL_CAPT_VB_REG_L 0x42
#define SENSOR_PAGE_SELECT_REG 0xfe
#define SENSOR_SET_GAIN_REG 0xb6
#define SENSOR_SET_GAIN_REG_H 0xb1
#define SENSOR_SET_GAIN_REG_L 0xb2
#define SENSOR_CISCTRL_BUF_EXP_IN_REG_H 0x03
#define SENSOR_CISCTRL_BUF_EXP_IN_REG_L 0x04
#define SENSOR_SET_PATTERN_MODE 0x8c

#define GC5035_MIRROR_FLIP_ENABLE         0
#if GC5035_MIRROR_FLIP_ENABLE
#define GC5035_MIRROR                     0x83
#define GC5035_RSTDUMMY1                  0x03
#define GC5035_RSTDUMMY2                  0xfc
#else
#define GC5035_MIRROR                     0x80
#define GC5035_RSTDUMMY1                  0x02
#define GC5035_RSTDUMMY2                  0x7c
#endif

/* SENSOR PRIVATE INFO FOR GAIN SETTING */
#define GC5035_SENSOR_GAIN_BASE             256
#define GC5035_SENSOR_GAIN_MAX              (16 * GC5035_SENSOR_GAIN_BASE)
#define GC5035_SENSOR_GAIN_MAX_VALID_INDEX  17
#define GC5035_SENSOR_GAIN_MAP_SIZE         17
#define GC5035_SENSOR_DGAIN_BASE            0x100

/* OTP DPC PARAMETERS */
#define GC5035_OTP_DPC_FLAG_OFFSET         0x0068
#define GC5035_OTP_DPC_TOTAL_NUM_OFFSET    0x0070
#define GC5035_OTP_DPC_ERROR_NUM_OFFSET    0x0078
/* SENSOR PRIVATE INFO FOR OTP SETTINGS */
#define GC5035_OTP_FOR_CUSTOMER            1
#define GC5035_OTP_DEBUG                   0

/* DEBUG */
#if GC5035_OTP_DEBUG
#define GC5035_OTP_START_ADDR              0x0000
#endif

#define GC5035_OTP_DATA_LENGTH             1024

/* OTP FLAG TYPE */
#define GC5035_OTP_FLAG_EMPTY              0x00
#define GC5035_OTP_FLAG_VALID              0x01
#define GC5035_OTP_FLAG_INVALID            0x02
#define GC5035_OTP_FLAG_INVALID2           0x03
#define gc5035_otp_get_offset(size)           (size << 3)
#define gc5035_otp_get_2bit_flag(flag, bit)   ((flag >> bit) & 0x03)
#define gc5035_otp_check_1bit_flag(flag, bit) \
	(((flag >> bit) & 0x01) == GC5035_OTP_FLAG_VALID)

#define GC5035_OTP_ID_SIZE                 9
#define GC5035_OTP_ID_DATA_OFFSET          0x0020

/* OTP REGISTER UPDATE PARAMETERS */
#define GC5035_OTP_REG_FLAG_OFFSET         0x0880
#define GC5035_OTP_REG_DATA_OFFSET         0x0888
#define GC5035_OTP_REG_MAX_GROUP           5
#define GC5035_OTP_REG_BYTE_PER_GROUP      5
#define GC5035_OTP_REG_REG_PER_GROUP       2
#define GC5035_OTP_REG_BYTE_PER_REG        2
#define GC5035_OTP_REG_DATA_SIZE \
	(GC5035_OTP_REG_MAX_GROUP * GC5035_OTP_REG_BYTE_PER_GROUP)
#define GC5035_OTP_REG_REG_SIZE \
	(GC5035_OTP_REG_MAX_GROUP * GC5035_OTP_REG_REG_PER_GROUP)

#if GC5035_OTP_FOR_CUSTOMER
#define GC5035_OTP_CHECK_SUM_BYTE          1
#define GC5035_OTP_GROUP_CNT               2

/* OTP MODULE INFO PARAMETERS */
#define GC5035_OTP_MODULE_FLAG_OFFSET      0x1f10
#define GC5035_OTP_MODULE_DATA_OFFSET      0x1f18
#define GC5035_OTP_MODULE_DATA_SIZE        6

/* OTP WB PARAMETERS */
#define GC5035_OTP_WB_FLAG_OFFSET          0x1f78
#define GC5035_OTP_WB_DATA_OFFSET          0x1f80
#define GC5035_OTP_WB_DATA_SIZE            4
#define GC5035_OTP_GOLDEN_DATA_OFFSET      0x1fc0
#define GC5035_OTP_GOLDEN_DATA_SIZE        4
#define GC5035_OTP_WB_CAL_BASE             0x0800
#define GC5035_OTP_WB_GAIN_BASE            0x0400

/* WB TYPICAL VALUE 0.5 */
#define GC5035_OTP_WB_RG_TYPICAL           0x04D9
#define GC5035_OTP_WB_BG_TYPICAL           0x04AB
#define LCE_MOD_ID                         0X1E
#define GROUP1_MODULE_ID                   0x1f18
#define GROUP2_MODULE_ID                   0x1f48
#endif

/* DPC STRUCTURE */
struct gc5035_dpc_t {
	kal_uint8 flag;
	kal_uint16 total_num;
};

struct gc5035_reg_t {
	kal_uint8 page;
	kal_uint8 addr;
	kal_uint8 value;
};

/* REGISTER UPDATE STRUCTURE */
struct gc5035_reg_update_t {
	kal_uint8 flag;
	kal_uint8 cnt;
	struct gc5035_reg_t reg[GC5035_OTP_REG_REG_SIZE];
};

#if GC5035_OTP_FOR_CUSTOMER
/* MODULE INFO STRUCTURE */
struct gc5035_module_info_t {
	kal_uint8 module_id;
	kal_uint8 lens_id;
	kal_uint8 year;
	kal_uint8 month;
	kal_uint8 day;
};

/* WB STRUCTURE */
struct gc5035_wb_t {
	kal_uint8  flag;
	kal_uint16 rg;
	kal_uint16 bg;
};
#endif

/* OTP STRUCTURE */
struct gc5035_otp_t {
	kal_uint8 otp_id[GC5035_OTP_ID_SIZE];
	struct gc5035_dpc_t dpc;
	struct gc5035_reg_update_t regs;
#if GC5035_OTP_FOR_CUSTOMER
	struct gc5035_wb_t wb;
	struct gc5035_wb_t golden;
#endif
};

static kal_uint32 dgain_ratio = GC5035_SENSOR_DGAIN_BASE;

kal_uint16 g_gc5035_agc_param[GC5035_SENSOR_GAIN_MAP_SIZE][2] = {
	{ 256, 0 },
	{ 302, 1 },
	{ 358, 2 },
	{ 425, 3 },
	{ 502, 8 },
	{ 599, 9 },
	{ 717, 10 },
	{ 845, 11 },
	{ 998, 12 },
	{ 1203, 13 },
	{ 1434, 14 },
	{ 1710, 15 },
	{ 1997, 16 },
	{ 2355, 17 },
	{ 2816, 18 },
	{ 3318, 19 },
	{ 3994, 20 },
};


static struct imgsensor_i2c_reg stream_on[] = {
	{ 0xfe, 0x00, 0x00 },
	{ 0x3f, 0x91, 0x00 },
	{ 0xfe, 0x00, 0x00 },
};

static struct imgsensor_i2c_reg stream_off[] = {
	{ 0xfe, 0x00, 0x00 },
	{ 0x3f, 0x00, 0x00 },
	{ 0xfe, 0x00, 0x00 },
};

static struct imgsensor_i2c_reg init_setting[] = {
	{ 0xfc, 0x01, 0x00 },
	{ 0xf4, 0x40, 0x00 },
	{ 0xf5, 0xe9, 0x00 },
	{ 0xf6, 0x14, 0x00 },
	{ 0xf8, 0x48, 0x00 },
	{ 0xf9, 0x82, 0x00 },
	{ 0xfa, 0x00, 0x00 },
	{ 0xfc, 0x81, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0x36, 0x01, 0x00 },
	{ 0xd3, 0x87, 0x00 },
	{ 0x36, 0x00, 0x00 },
	{ 0x33, 0x00, 0x00 },
	{ 0xfe, 0x03, 0x00 },
	{ 0x01, 0xe7, 0x00 },
	{ 0xf7, 0x01, 0x00 },
	{ 0xfc, 0x8f, 0x00 },
	{ 0xfc, 0x8f, 0x00 },
	{ 0xfc, 0x8e, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xee, 0x30, 0x00 },
	{ 0x87, 0x18, 0x00 },
	{ 0xfe, 0x01, 0x00 },
	{ 0x8c, 0x90, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0x05, 0x02, 0x00 },
	{ 0x06, 0xda, 0x00 },
	{ 0x9d, 0x0c, 0x00 },
	{ 0x09, 0x00, 0x00 },
	{ 0x0a, 0x04, 0x00 },
	{ 0x0b, 0x00, 0x00 },
	{ 0x0c, 0x03, 0x00 },
	{ 0x0d, 0x07, 0x00 },
	{ 0x0e, 0xa8, 0x00 },
	{ 0x0f, 0x0a, 0x00 },
	{ 0x10, 0x30, 0x00 },
	{ 0x11, 0x02, 0x00 },
	{ 0x17, GC5035_MIRROR, 0x00 },
	{ 0x19, 0x05, 0x00 },
	{ 0xfe, 0x02, 0x00 },
	{ 0x30, 0x03, 0x00 },
	{ 0x31, 0x03, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xd9, 0xc0, 0x00 },
	{ 0x1b, 0x20, 0x00 },
	{ 0x21, 0x48, 0x00 },
	{ 0x28, 0x22, 0x00 },
	{ 0x29, 0x58, 0x00 },
	{ 0x44, 0x20, 0x00 },
	{ 0x4b, 0x10, 0x00 },
	{ 0x4e, 0x1a, 0x00 },
	{ 0x50, 0x11, 0x00 },
	{ 0x52, 0x33, 0x00 },
	{ 0x53, 0x44, 0x00 },
	{ 0x55, 0x10, 0x00 },
	{ 0x5b, 0x11, 0x00 },
	{ 0xc5, 0x02, 0x00 },
	{ 0x8c, 0x1a, 0x00 },
	{ 0xfe, 0x02, 0x00 },
	{ 0x33, 0x05, 0x00 },
	{ 0x32, 0x38, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0x91, 0x80, 0x00 },
	{ 0x92, 0x28, 0x00 },
	{ 0x93, 0x20, 0x00 },
	{ 0x95, 0xa0, 0x00 },
	{ 0x96, 0xe0, 0x00 },
	{ 0xd5, 0xfc, 0x00 },
	{ 0x97, 0x28, 0x00 },
	{ 0x16, 0x0c, 0x00 },
	{ 0x1a, 0x1a, 0x00 },
	{ 0x1f, 0x11, 0x00 },
	{ 0x20, 0x10, 0x00 },
	{ 0x46, 0x83, 0x00 },
	{ 0x4a, 0x04, 0x00 },
	{ 0x54, GC5035_RSTDUMMY1, 0x00 },
	{ 0x62, 0x00, 0x00 },
	{ 0x72, 0x8f, 0x00 },
	{ 0x73, 0x89, 0x00 },
	{ 0x7a, 0x05, 0x00 },
	{ 0x7d, 0xcc, 0x00 },
	{ 0x90, 0x00, 0x00 },
	{ 0xce, 0x18, 0x00 },
	{ 0xd0, 0xb2, 0x00 },
	{ 0xd2, 0x40, 0x00 },
	{ 0xe6, 0xe0, 0x00 },
	{ 0xfe, 0x02, 0x00 },
	{ 0x12, 0x01, 0x00 },
	{ 0x13, 0x01, 0x00 },
	{ 0x14, 0x01, 0x00 },
	{ 0x15, 0x02, 0x00 },
	{ 0x22, GC5035_RSTDUMMY2, 0x00 },
	{ 0x91, 0x00, 0x00 },
	{ 0x92, 0x00, 0x00 },
	{ 0x93, 0x00, 0x00 },
	{ 0x94, 0x00, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x88, 0x00 },
	{ 0xfe, 0x10, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x8e, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x88, 0x00 },
	{ 0xfe, 0x10, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x8e, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xb0, 0x6e, 0x00 },
	{ 0xb1, 0x01, 0x00 },
	{ 0xb2, 0x00, 0x00 },
	{ 0xb3, 0x00, 0x00 },
	{ 0xb4, 0x00, 0x00 },
	{ 0xb6, 0x00, 0x00 },
	{ 0xfe, 0x01, 0x00 },
	{ 0x53, 0x00, 0x00 },
	{ 0x89, 0x03, 0x00 },
	{ 0x60, 0x40, 0x00 },
	{ 0xfe, 0x01, 0x00 },
	{ 0x42, 0x21, 0x00 },
	{ 0x49, 0x03, 0x00 },
	{ 0x4a, 0xff, 0x00 },
	{ 0x4b, 0xc0, 0x00 },
	{ 0x55, 0x00, 0x00 },
	{ 0xfe, 0x01, 0x00 },
	{ 0x41, 0x28, 0x00 },
	{ 0x4c, 0x00, 0x00 },
	{ 0x4d, 0x00, 0x00 },
	{ 0x4e, 0x3c, 0x00 },
	{ 0x44, 0x08, 0x00 },
	{ 0x48, 0x02, 0x00 },
	{ 0xfe, 0x01, 0x00 },
	{ 0x91, 0x00, 0x00 },
	{ 0x92, 0x08, 0x00 },
	{ 0x93, 0x00, 0x00 },
	{ 0x94, 0x07, 0x00 },
	{ 0x95, 0x07, 0x00 },
	{ 0x96, 0x98, 0x00 },
	{ 0x97, 0x0a, 0x00 },
	{ 0x98, 0x20, 0x00 },
	{ 0x99, 0x00, 0x00 },
	{ 0xfe, 0x03, 0x00 },
	{ 0x02, 0x57, 0x00 },
	{ 0x03, 0xb7, 0x00 },
	{ 0x15, 0x14, 0x00 },
	{ 0x18, 0x0f, 0x00 },
	{ 0x21, 0x22, 0x00 },
	{ 0x22, 0x06, 0x00 },
	{ 0x23, 0x48, 0x00 },
	{ 0x24, 0x12, 0x00 },
	{ 0x25, 0x28, 0x00 },
	{ 0x26, 0x08, 0x00 },
	{ 0x29, 0x06, 0x00 },
	{ 0x2a, 0x58, 0x00 },
	{ 0x2b, 0x08, 0x00 },
	{ 0xfe, 0x01, 0x00 },
	{ 0x8c, 0x10, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0x3e, 0x01, 0x00 },
};


static struct imgsensor_i2c_reg preview_setting[] = {
	{ 0xfe, 0x00, 0x00 },
	{ 0x3e, 0x01, 0x00 },
	{ 0xfc, 0x01, 0x00 },
	{ 0xf4, 0x40, 0x00 },
	{ 0xf5, 0xe4, 0x00 },
	{ 0xf6, 0x14, 0x00 },
	{ 0xf8, 0x48, 0x00 },
	{ 0xf9, 0x12, 0x00 },
	{ 0xfa, 0x01, 0x00 },
	{ 0xfc, 0x81, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0x36, 0x01, 0x00 },
	{ 0xd3, 0x87, 0x00 },
	{ 0x36, 0x00, 0x00 },
	{ 0x33, 0x20, 0x00 },
	{ 0xfe, 0x03, 0x00 },
	{ 0x01, 0x87, 0x00 },
	{ 0xf7, 0x11, 0x00 },
	{ 0xfc, 0x8f, 0x00 },
	{ 0xfc, 0x8f, 0x00 },
	{ 0xfc, 0x8e, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xee, 0x30, 0x00 },
	{ 0x87, 0x18, 0x00 },
	{ 0xfe, 0x01, 0x00 },
	{ 0x8c, 0x90, 0x00 },
	{ 0xfe, 0x00, 0x00 },

	/* Analog & CISCTL */
	{ 0xfe, 0x00, 0x00 },
	{ 0x05, 0x02, 0x00 },
	{ 0x06, 0xda, 0x00 },
	{ 0x9d, 0x0c, 0x00 },
	{ 0x09, 0x00, 0x00 },
	{ 0x0a, 0x04, 0x00 },
	{ 0x0b, 0x00, 0x00 },
	{ 0x0c, 0x03, 0x00 },
	{ 0x0d, 0x07, 0x00 },
	{ 0x0e, 0xa8, 0x00 },
	{ 0x0f, 0x0a, 0x00 },
	{ 0x10, 0x30, 0x00 },
	{ 0x21, 0x60, 0x00 },
	{ 0x29, 0x30, 0x00 },
	{ 0x44, 0x18, 0x00 },
	{ 0x4e, 0x20, 0x00 },
	{ 0x8c, 0x20, 0x00 },
	{ 0x91, 0x15, 0x00 },
	{ 0x92, 0x3a, 0x00 },
	{ 0x93, 0x20, 0x00 },
	{ 0x95, 0x45, 0x00 },
	{ 0x96, 0x35, 0x00 },
	{ 0xd5, 0xf0, 0x00 },
	{ 0x97, 0x20, 0x00 },
	{ 0x1f, 0x19, 0x00 },
	{ 0xce, 0x18, 0x00 },
	{ 0xd0, 0xb3, 0x00 },
	{ 0xfe, 0x02, 0x00 },
	{ 0x14, 0x02, 0x00 },
	{ 0x15, 0x00, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x88, 0x00 },
	{ 0xfe, 0x10, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x8e, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x88, 0x00 },
	{ 0xfe, 0x10, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x8e, 0x00 },

	/* BLK */
	{ 0xfe, 0x01, 0x00 },
	{ 0x49, 0x00, 0x00 },
	{ 0x4a, 0x01, 0x00 },
	{ 0x4b, 0xf8, 0x00 },

	/* Anti_blooming */
	{ 0xfe, 0x01, 0x00 },
	{ 0x4e, 0x06, 0x00 },
	{ 0x44, 0x02, 0x00 },

	/* Crop */
	{ 0xfe, 0x01, 0x00 },
	{ 0x91, 0x00, 0x00 },
	{ 0x92, 0x04, 0x00 },
	{ 0x93, 0x00, 0x00 },
	{ 0x94, 0x03, 0x00 },
	{ 0x95, 0x03, 0x00 },
	{ 0x96, 0xcc, 0x00 },
	{ 0x97, 0x05, 0x00 },
	{ 0x98, 0x10, 0x00 },
	{ 0x99, 0x00, 0x00 },

	/* MIPI */
	{ 0xfe, 0x03, 0x00 },
	{ 0x02, 0x58, 0x00 },
	{ 0x22, 0x03, 0x00 },
	{ 0x26, 0x06, 0x00 },
	{ 0x29, 0x03, 0x00 },
	{ 0x2b, 0x06, 0x00 },
	{ 0xfe, 0x01, 0x00 },
	{ 0x8c, 0x10, 0x00 },

	{ 0xfe, 0x00, 0x00 },
	{ 0x3e, 0x91, 0x00 },
};

static struct imgsensor_i2c_reg capture_setting[] = {
	{ 0xfe, 0x00, 0x00 },
	{ 0x3e, 0x01, 0x00 },
	{ 0xfc, 0x01, 0x00 },
	{ 0xf4, 0x40, 0x00 },
	{ 0xf5, 0xe9, 0x00 },
	{ 0xf6, 0x14, 0x00 },
	{ 0xf8, 0x48, 0x00 },
	{ 0xf9, 0x82, 0x00 },
	{ 0xfa, 0x00, 0x00 },
	{ 0xfc, 0x81, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0x36, 0x01, 0x00 },
	{ 0xd3, 0x87, 0x00 },
	{ 0x36, 0x00, 0x00 },
	{ 0x33, 0x00, 0x00 },
	{ 0xfe, 0x03, 0x00 },
	{ 0x01, 0xe7, 0x00 },
	{ 0xf7, 0x01, 0x00 },
	{ 0xfc, 0x8f, 0x00 },
	{ 0xfc, 0x8f, 0x00 },
	{ 0xfc, 0x8e, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xee, 0x30, 0x00 },
	{ 0x87, 0x18, 0x00 },
	{ 0xfe, 0x01, 0x00 },
	{ 0x8c, 0x90, 0x00 },
	{ 0xfe, 0x00, 0x00 },

	/* Analog & CISCTL */
	{ 0xfe, 0x00, 0x00 },
	{ 0x05, 0x02, 0x00 },
	{ 0x06, 0xda, 0x00 },
	{ 0x9d, 0x0c, 0x00 },
	{ 0x09, 0x00, 0x00 },
	{ 0x0a, 0x04, 0x00 },
	{ 0x0b, 0x00, 0x00 },
	{ 0x0c, 0x03, 0x00 },
	{ 0x0d, 0x07, 0x00 },
	{ 0x0e, 0xa8, 0x00 },
	{ 0x0f, 0x0a, 0x00 },
	{ 0x10, 0x30, 0x00 },
	{ 0x21, 0x48, 0x00 },
	{ 0x29, 0x58, 0x00 },
	{ 0x44, 0x20, 0x00 },
	{ 0x4e, 0x1a, 0x00 },
	{ 0x8c, 0x1a, 0x00 },
	{ 0x91, 0x80, 0x00 },
	{ 0x92, 0x28, 0x00 },
	{ 0x93, 0x20, 0x00 },
	{ 0x95, 0xa0, 0x00 },
	{ 0x96, 0xe0, 0x00 },
	{ 0xd5, 0xfc, 0x00 },
	{ 0x97, 0x28, 0x00 },
	{ 0x1f, 0x11, 0x00 },
	{ 0xce, 0x18, 0x00 },
	{ 0xd0, 0xb2, 0x00 },
	{ 0xfe, 0x02, 0x00 },
	{ 0x14, 0x01, 0x00 },
	{ 0x15, 0x02, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x88, 0x00 },
	{ 0xfe, 0x10, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x8e, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x88, 0x00 },
	{ 0xfe, 0x10, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x8e, 0x00 },

	/* BLK */
	{ 0xfe, 0x01, 0x00 },
	{ 0x49, 0x03, 0x00 },
	{ 0x4a, 0xff, 0x00 },
	{ 0x4b, 0xc0, 0x00 },

	/* Anti_blooming */
	{ 0xfe, 0x01, 0x00 },
	{ 0x4e, 0x3c, 0x00 },
	{ 0x44, 0x08, 0x00 },

	/* Crop */
	{ 0xfe, 0x01, 0x00 },
	{ 0x91, 0x00, 0x00 },
	{ 0x92, 0x08, 0x00 },
	{ 0x93, 0x00, 0x00 },
	{ 0x94, 0x07, 0x00 },
	{ 0x95, 0x07, 0x00 },
	{ 0x96, 0x98, 0x00 },
	{ 0x97, 0x0a, 0x00 },
	{ 0x98, 0x20, 0x00 },
	{ 0x99, 0x00, 0x00 },

	/* MIPI */
	{ 0xfe, 0x03, 0x00 },
	{ 0x02, 0x57, 0x00 },
	{ 0x22, 0x06, 0x00 },
	{ 0x26, 0x08, 0x00 },
	{ 0x29, 0x06, 0x00 },
	{ 0x2b, 0x08, 0x00 },
	{ 0xfe, 0x01, 0x00 },
	{ 0x8c, 0x10, 0x00 },

	{ 0xfe, 0x00, 0x00 },
	{ 0x3e, 0x91, 0x00 },
};

static struct imgsensor_i2c_reg video_setting[] = {
	{ 0xfe, 0x00, 0x00 },
	{ 0x3e, 0x01, 0x00 },
	{ 0xfc, 0x01, 0x00 },
	{ 0xf4, 0x40, 0x00 },
	{ 0xf5, 0xe9, 0x00 },
	{ 0xf6, 0x14, 0x00 },
	{ 0xf8, 0x48, 0x00 },
	{ 0xf9, 0x82, 0x00 },
	{ 0xfa, 0x00, 0x00 },
	{ 0xfc, 0x81, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0x36, 0x01, 0x00 },
	{ 0xd3, 0x87, 0x00 },
	{ 0x36, 0x00, 0x00 },
	{ 0x33, 0x00, 0x00 },
	{ 0xfe, 0x03, 0x00 },
	{ 0x01, 0xe7, 0x00 },
	{ 0xf7, 0x01, 0x00 },
	{ 0xfc, 0x8f, 0x00 },
	{ 0xfc, 0x8f, 0x00 },
	{ 0xfc, 0x8e, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xee, 0x30, 0x00 },
	{ 0x87, 0x18, 0x00 },
	{ 0xfe, 0x01, 0x00 },
	{ 0x8c, 0x90, 0x00 },
	{ 0xfe, 0x00, 0x00 },

	/* Analog & CISCTL */
	{ 0xfe, 0x00, 0x00 },
	{ 0x05, 0x02, 0x00 },
	{ 0x06, 0xda, 0x00 },
	{ 0x9d, 0x0c, 0x00 },
	{ 0x09, 0x00, 0x00 },
	{ 0x0a, 0x04, 0x00 },
	{ 0x0b, 0x00, 0x00 },
	{ 0x0c, 0x03, 0x00 },
	{ 0x0d, 0x07, 0x00 },
	{ 0x0e, 0xa8, 0x00 },
	{ 0x0f, 0x0a, 0x00 },
	{ 0x10, 0x30, 0x00 },
	{ 0x21, 0x48, 0x00 },
	{ 0x29, 0x58, 0x00 },
	{ 0x44, 0x20, 0x00 },
	{ 0x4e, 0x1a, 0x00 },
	{ 0x8c, 0x1a, 0x00 },
	{ 0x91, 0x80, 0x00 },
	{ 0x92, 0x28, 0x00 },
	{ 0x93, 0x20, 0x00 },
	{ 0x95, 0xa0, 0x00 },
	{ 0x96, 0xe0, 0x00 },
	{ 0xd5, 0xfc, 0x00 },
	{ 0x97, 0x28, 0x00 },
	{ 0x1f, 0x11, 0x00 },
	{ 0xce, 0x18, 0x00 },
	{ 0xd0, 0xb2, 0x00 },
	{ 0xfe, 0x02, 0x00 },
	{ 0x14, 0x01, 0x00 },
	{ 0x15, 0x02, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x88, 0x00 },
	{ 0xfe, 0x10, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x8e, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x88, 0x00 },
	{ 0xfe, 0x10, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x8e, 0x00 },

	/* BLK */
	{ 0xfe, 0x01, 0x00 },
	{ 0x49, 0x03, 0x00 },
	{ 0x4a, 0xff, 0x00 },
	{ 0x4b, 0xc0, 0x00 },

	/* Anti_blooming */
	{ 0xfe, 0x01, 0x00 },
	{ 0x4e, 0x3c, 0x00 },
	{ 0x44, 0x08, 0x00 },

	/* Crop */
	{ 0xfe, 0x01, 0x00 },
	{ 0x91, 0x00, 0x00 },
	{ 0x92, 0x08, 0x00 },
	{ 0x93, 0x00, 0x00 },
	{ 0x94, 0x07, 0x00 },
	{ 0x95, 0x07, 0x00 },
	{ 0x96, 0x98, 0x00 },
	{ 0x97, 0x0a, 0x00 },
	{ 0x98, 0x20, 0x00 },
	{ 0x99, 0x00, 0x00 },

	/* MIPI */
	{ 0xfe, 0x03, 0x00 },
	{ 0x02, 0x57, 0x00 },
	{ 0x22, 0x06, 0x00 },
	{ 0x26, 0x08, 0x00 },
	{ 0x29, 0x06, 0x00 },
	{ 0x2b, 0x08, 0x00 },
	{ 0xfe, 0x01, 0x00 },
	{ 0x8c, 0x10, 0x00 },

	{ 0xfe, 0x00, 0x00 },
	{ 0x3e, 0x91, 0x00 },
};

static struct imgsensor_i2c_reg custom2_setting[] = {
	{ 0xfe, 0x00, 0x00 },
	{ 0x3e, 0x01, 0x00 },
	{ 0xfc, 0x01, 0x00 },
	{ 0xf4, 0x40, 0x00 },
	{ 0xf5, 0xe9, 0x00 },
	{ 0xf6, 0x14, 0x00 },
	{ 0xf8, 0x48, 0x00 },
	{ 0xf9, 0x82, 0x00 },
	{ 0xfa, 0x00, 0x00 },
	{ 0xfc, 0x81, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0x36, 0x01, 0x00 },
	{ 0xd3, 0x87, 0x00 },
	{ 0x36, 0x00, 0x00 },
	{ 0x33, 0x00, 0x00 },
	{ 0xfe, 0x03, 0x00 },
	{ 0x01, 0xe7, 0x00 },
	{ 0xf7, 0x01, 0x00 },
	{ 0xfc, 0x8f, 0x00 },
	{ 0xfc, 0x8f, 0x00 },
	{ 0xfc, 0x8e, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xee, 0x30, 0x00 },
	{ 0x87, 0x18, 0x00 },
	{ 0xfe, 0x01, 0x00 },
	{ 0x8c, 0x90, 0x00 },
	{ 0xfe, 0x00, 0x00 },

	/* Analog & CISCTL */
	{ 0xfe, 0x00, 0x00 },
	{ 0x05, 0x02, 0x00 },
	{ 0x06, 0xda, 0x00 },
	{ 0x9d, 0x0c, 0x00 },
	{ 0x09, 0x00, 0x00 },
	{ 0x0a, 0x04, 0x00 },
	{ 0x0b, 0x00, 0x00 },
	{ 0x0c, 0x03, 0x00 },
	{ 0x0d, 0x07, 0x00 },
	{ 0x0e, 0xa8, 0x00 },
	{ 0x0f, 0x0a, 0x00 },
	{ 0x10, 0x30, 0x00 },
	{ 0x21, 0x48, 0x00 },
	{ 0x29, 0x58, 0x00 },
	{ 0x44, 0x20, 0x00 },
	{ 0x4e, 0x1a, 0x00 },
	{ 0x8c, 0x1a, 0x00 },
	{ 0x91, 0x80, 0x00 },
	{ 0x92, 0x28, 0x00 },
	{ 0x93, 0x20, 0x00 },
	{ 0x95, 0xa0, 0x00 },
	{ 0x96, 0xe0, 0x00 },
	{ 0xd5, 0xfc, 0x00 },
	{ 0x97, 0x28, 0x00 },
	{ 0x1f, 0x11, 0x00 },
	{ 0xce, 0x18, 0x00 },
	{ 0xd0, 0xb2, 0x00 },
	{ 0xfe, 0x02, 0x00 },
	{ 0x14, 0x01, 0x00 },
	{ 0x15, 0x02, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x88, 0x00 },
	{ 0xfe, 0x10, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x8e, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x88, 0x00 },
	{ 0xfe, 0x10, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfc, 0x8e, 0x00 },

	/* BLK */
	{ 0xfe, 0x01, 0x00 },
	{ 0x49, 0x03, 0x00 },
	{ 0x4a, 0xff, 0x00 },
	{ 0x4b, 0xc0, 0x00 },

	/* Anti_blooming */
	{ 0xfe, 0x01, 0x00 },
	{ 0x4e, 0x3c, 0x00 },
	{ 0x44, 0x08, 0x00 },

	/* Crop */
	{ 0xfe, 0x01, 0x00 },
	{ 0x91, 0x00, 0x00 },
	{ 0x92, 0x08, 0x00 },
	{ 0x93, 0x00, 0x00 },
	{ 0x94, 0x07, 0x00 },
	{ 0x95, 0x07, 0x00 },
	{ 0x96, 0x98, 0x00 },
	{ 0x97, 0x0a, 0x00 },
	{ 0x98, 0x20, 0x00 },
	{ 0x99, 0x00, 0x00 },

	/* MIPI */
	{ 0xfe, 0x03, 0x00 },
	{ 0x02, 0x57, 0x00 },
	{ 0x22, 0x06, 0x00 },
	{ 0x26, 0x08, 0x00 },
	{ 0x29, 0x06, 0x00 },
	{ 0x2b, 0x08, 0x00 },
	{ 0xfe, 0x01, 0x00 },
	{ 0x8c, 0x10, 0x00 },

	{ 0xfe, 0x00, 0x00 },
	{ 0x3e, 0x91, 0x00 },
};

static struct imgsensor_i2c_reg_table dump_setting[] = {
	{ 0xfe, 0x00, IMGSENSOR_I2C_BYTE_DATA, IMGSENSOR_I2C_READ, 0 },
	{ 0x00, 0x00, IMGSENSOR_I2C_BYTE_DATA, IMGSENSOR_I2C_READ, 0 },
};

static struct imgsensor_info_t imgsensor_info = {
	.sensor_id_reg = 0xf0,
	.sensor_id = GC5035_LCE_SENSOR_ID,
	.checksum_value = 0xD470,

	.pre = {
		.pclk = 87600000,
		.linelength = 1460,
		.framelength = 2008,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1296,
		.grabwindow_height = 972,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 86400000,
		.max_framerate = 300,
	},

	.cap = {
		.pclk = 175200000,
		.linelength = 2920,
		.framelength = 2008,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 168400000,
		.max_framerate = 300,
	},

	.normal_video = {
		.pclk = 175200000,
		.linelength = 2920,
		.framelength = 2008,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 168400000,
		.max_framerate = 300,
	},

	.custom2 = {
		.pclk = 175200000,
		.linelength = 2920,
		.framelength = 2008,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 168400000,
		.max_framerate = 300,
	},
	.init_setting = {
		.setting = init_setting,
		.size = IMGSENSOR_ARRAY_SIZE(init_setting),
		.addr_type = IMGSENSOR_I2C_BYTE_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},
	.pre_setting = {
		.setting = preview_setting,
		.size = IMGSENSOR_ARRAY_SIZE(preview_setting),
		.addr_type = IMGSENSOR_I2C_BYTE_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},
	.cap_setting = {
		.setting = capture_setting,
		.size = IMGSENSOR_ARRAY_SIZE(capture_setting),
		.addr_type = IMGSENSOR_I2C_BYTE_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},
	.normal_video_setting = {
		.setting = video_setting,
		.size = IMGSENSOR_ARRAY_SIZE(video_setting),
		.addr_type = IMGSENSOR_I2C_BYTE_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},
	.custom2_setting = {
		.setting = custom2_setting,
		.size = IMGSENSOR_ARRAY_SIZE(custom2_setting),
		.addr_type = IMGSENSOR_I2C_BYTE_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},
	.streamon_setting = {
		.setting = stream_on,
		.size = IMGSENSOR_ARRAY_SIZE(stream_on),
		.addr_type = IMGSENSOR_I2C_BYTE_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},

	.streamoff_setting = {
		.setting = stream_off,
		.size = IMGSENSOR_ARRAY_SIZE(stream_off),
		.addr_type = IMGSENSOR_I2C_BYTE_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 0,
	},

	.dump_info = {
		.setting = dump_setting,
		.size = IMGSENSOR_ARRAY_SIZE(dump_setting),
	},

	.margin = 16,
	.min_shutter = 4,
	.max_frame_length = 0x3fff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 7,

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
	.custom2_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = 0,
#if GC5035_MIRROR_FLIP_ENABLE
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
#else
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
#endif
	.mclk = 24,  // mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
	.mipi_lane_num = SENSOR_MIPI_2_LANE,  // mipi lane num
	.i2c_addr_table = { 0x7E, 0xff },
	.i2c_speed = 400,  // i2c read/write speed
	.addr_type = IMGSENSOR_I2C_BYTE_ADDR,
};

static struct imgsensor_t imgsensor = {
	.mirror = IMAGE_HV_MIRROR,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x258,  // current shutter
	.gain = 0x40,  // current gain
	.dummy_pixel = 0,  // current dummypixel
	.dummy_line = 0,  // current dummyline
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = KAL_FALSE,
	.i2c_write_id = 0x7E,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	/* preview */
	{
		.full_w = 2592,
		.full_h = 1944,
		.x0_offset = 0,
		.y0_offset = 0,
		.w0_size = 2592,
		.h0_size = 1944,
		.scale_w = 1296,
		.scale_h = 972,
		.x1_offset = 0,
		.y1_offset = 0,
		.w1_size = 1296,
		.h1_size = 972,
		.x2_tg_offset = 0,
		.y2_tg_offset = 0,
		.w2_tg_size = 1296,
		.h2_tg_size = 972,
	},
	/* capture */
	{
		.full_w = 2592,
		.full_h = 1944,
		.x0_offset = 0,
		.y0_offset = 0,
		.w0_size = 2592,
		.h0_size = 1944,
		.scale_w = 2592,
		.scale_h = 1944,
		.x1_offset = 0,
		.y1_offset = 0,
		.w1_size = 2592,
		.h1_size = 1944,
		.x2_tg_offset = 0,
		.y2_tg_offset = 0,
		.w2_tg_size = 2592,
		.h2_tg_size = 1944,
	},
	/* video */
	{
		.full_w = 2592,
		.full_h = 1944,
		.x0_offset = 0,
		.y0_offset = 0,
		.w0_size = 2592,
		.h0_size = 1944,
		.scale_w = 2592,
		.scale_h = 1944,
		.x1_offset = 0,
		.y1_offset = 0,
		.w1_size = 2592,
		.h1_size = 1944,
		.x2_tg_offset = 0,
		.y2_tg_offset = 0,
		.w2_tg_size = 2592,
		.h2_tg_size = 1944,
	},
		/* hs_video */
	{
		.full_w = 2592,
		.full_h = 1944,
		.x0_offset = 0,
		.y0_offset = 0,
		.w0_size = 2592,
		.h0_size = 1944,
		.scale_w = 2592,
		.scale_h = 1944,
		.x1_offset = 0,
		.y1_offset = 0,
		.w1_size = 2592,
		.h1_size = 1944,
		.x2_tg_offset = 0,
		.y2_tg_offset = 0,
		.w2_tg_size = 2592,
		.h2_tg_size = 1944,
	},
		/* sl_video */
	{
		.full_w = 2592,
		.full_h = 1944,
		.x0_offset = 0,
		.y0_offset = 0,
		.w0_size = 2592,
		.h0_size = 1944,
		.scale_w = 2592,
		.scale_h = 1944,
		.x1_offset = 0,
		.y1_offset = 0,
		.w1_size = 2592,
		.h1_size = 1944,
		.x2_tg_offset = 0,
		.y2_tg_offset = 0,
		.w2_tg_size = 2592,
		.h2_tg_size = 1944,
	},
		/* custom1 reserved */
	{
		.full_w = 2592,
		.full_h = 1944,
		.x0_offset = 0,
		.y0_offset = 0,
		.w0_size = 2592,
		.h0_size = 1944,
		.scale_w = 2592,
		.scale_h = 1944,
		.x1_offset = 0,
		.y1_offset = 0,
		.w1_size = 2592,
		.h1_size = 1944,
		.x2_tg_offset = 0,
		.y2_tg_offset = 0,
		.w2_tg_size = 2592,
		.h2_tg_size = 1944,
	},
		/* custom2 face unlocked*/
	{
		.full_w = 2592,
		.full_h = 1944,
		.x0_offset = 0,
		.y0_offset = 0,
		.w0_size = 2592,
		.h0_size = 1944,
		.scale_w = 2592,
		.scale_h = 1944,
		.x1_offset = 0,
		.y1_offset = 0,
		.w1_size = 2592,
		.h1_size = 1944,
		.x2_tg_offset = 0,
		.y2_tg_offset = 0,
		.w2_tg_size = 2592,
		.h2_tg_size = 1944,
	},
};
extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
	u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
#endif
