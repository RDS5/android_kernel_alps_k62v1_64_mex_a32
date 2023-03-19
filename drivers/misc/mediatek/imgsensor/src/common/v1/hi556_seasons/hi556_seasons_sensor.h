/*
 * hi556_seasons_sensor.h
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * hi846_ofilm image sensor config settings
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
#ifndef _HI556_SEASONS_SENSOR_H
#define _HI556_SEASONS_SENSOR_H


#include <linux/delay.h>
#include <linux/types.h>
#include <securec.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "imgsensor_sensor_i2c.h"
#include "imgsensor_sensor_common.h"
#include "kd_imgsensor.h"

#define SENSOR_FRM_LENGTH_LINES_REG_H 0x0006
#define SENSOR_FRM_LENGTH_LINES_REG_L 0x0007
#define SENSOR_LINE_LENGTH_PCK_REG_H 0x0008
#define SENSOR_LINE_LENGTH_PCK_REG_L 0x0009
#define SENSOR_IMAGE_ORIENTATION 0x000e
#define SENSOR_ISP_EN_REG_H 0x0a04
#define SENSOR_ISP_EN_REG_L 0x0a05
#define SENSOR_TP_MODE_REG 0x0200

#define SENSOR_INTEG_TIME_REG_H 0x0074
#define SENSOR_INTEG_TIME_REG_L 0x0075
#define SENSOR_ANA_GAIN_REG 0x0076

#define SENSOR_MODULE_ID_REG_H 0x0F17
#define SENSOR_MODULE_ID_REG_L 0x0F16

#define HI556_SEASONS_OTP 1
#ifdef HI556_SEASONS_OTP
#define SEASONS_MMODULE_ID 0X10
#define BASIC_GAIN 0x200
static int unit_rg_r_ratio, unit_bg_b_ratio, unit_gb_gr_ratio;
static int typical_rg_r_ratio, typical_bg_b_ratio, typical_gb_gr_ratio;
#endif
#define TURE 1
#define FAIL 0
static char otp_read_flag = FAIL;

static struct imgsensor_i2c_reg stream_on[] = {
	{ 0x0a00, 0x0100, 0x00 },
};

static struct imgsensor_i2c_reg stream_off[] = {
	{ 0x0a00, 0x0000, 0x00 },
};

static struct imgsensor_i2c_reg init_setting[] = {
	{ 0x0e00, 0x0102, 0x00 },
	{ 0x0e02, 0x0102, 0x00 },
	{ 0x0e0c, 0x0100, 0x00 },
	{ 0x2000, 0x7400, 0x00 },
	{ 0x2002, 0x001c, 0x00 },
	{ 0x2004, 0x0242, 0x00 },
	{ 0x2006, 0x0942, 0x00 },
	{ 0x2008, 0x7007, 0x00 },
	{ 0x200a, 0x0fd9, 0x00 },
	{ 0x200c, 0x0259, 0x00 },
	{ 0x200e, 0x7008, 0x00 },
	{ 0x2010, 0x160e, 0x00 },
	{ 0x2012, 0x0047, 0x00 },
	{ 0x2014, 0x2118, 0x00 },
	{ 0x2016, 0x0041, 0x00 },
	{ 0x2018, 0x00d8, 0x00 },
	{ 0x201a, 0x0145, 0x00 },
	{ 0x201c, 0x0006, 0x00 },
	{ 0x201e, 0x0181, 0x00 },
	{ 0x2020, 0x13cc, 0x00 },
	{ 0x2022, 0x2057, 0x00 },
	{ 0x2024, 0x7001, 0x00 },
	{ 0x2026, 0x0fca, 0x00 },
	{ 0x2028, 0x00cb, 0x00 },
	{ 0x202a, 0x009f, 0x00 },
	{ 0x202c, 0x7002, 0x00 },
	{ 0x202e, 0x13cc, 0x00 },
	{ 0x2030, 0x019b, 0x00 },
	{ 0x2032, 0x014d, 0x00 },
	{ 0x2034, 0x2987, 0x00 },
	{ 0x2036, 0x2766, 0x00 },
	{ 0x2038, 0x0020, 0x00 },
	{ 0x203a, 0x2060, 0x00 },
	{ 0x203c, 0x0e5d, 0x00 },
	{ 0x203e, 0x181d, 0x00 },
	{ 0x2040, 0x2066, 0x00 },
	{ 0x2042, 0x20c4, 0x00 },
	{ 0x2044, 0x5000, 0x00 },
	{ 0x2046, 0x0005, 0x00 },
	{ 0x2048, 0x0000, 0x00 },
	{ 0x204a, 0x01db, 0x00 },
	{ 0x204c, 0x025a, 0x00 },
	{ 0x204e, 0x00c0, 0x00 },
	{ 0x2050, 0x0005, 0x00 },
	{ 0x2052, 0x0006, 0x00 },
	{ 0x2054, 0x0ad9, 0x00 },
	{ 0x2056, 0x0259, 0x00 },
	{ 0x2058, 0x0618, 0x00 },
	{ 0x205a, 0x0258, 0x00 },
	{ 0x205c, 0x2266, 0x00 },
	{ 0x205e, 0x20c8, 0x00 },
	{ 0x2060, 0x2060, 0x00 },
	{ 0x2062, 0x707b, 0x00 },
	{ 0x2064, 0x0fdd, 0x00 },
	{ 0x2066, 0x81b8, 0x00 },
	{ 0x2068, 0x5040, 0x00 },
	{ 0x206a, 0x0020, 0x00 },
	{ 0x206c, 0x5060, 0x00 },
	{ 0x206e, 0x3143, 0x00 },
	{ 0x2070, 0x5081, 0x00 },
	{ 0x2072, 0x025c, 0x00 },
	{ 0x2074, 0x7800, 0x00 },
	{ 0x2076, 0x7400, 0x00 },
	{ 0x2078, 0x001c, 0x00 },
	{ 0x207a, 0x0242, 0x00 },
	{ 0x207c, 0x0942, 0x00 },
	{ 0x207e, 0x0bd9, 0x00 },
	{ 0x2080, 0x0259, 0x00 },
	{ 0x2082, 0x7008, 0x00 },
	{ 0x2084, 0x160e, 0x00 },
	{ 0x2086, 0x0047, 0x00 },
	{ 0x2088, 0x2118, 0x00 },
	{ 0x208a, 0x0041, 0x00 },
	{ 0x208c, 0x00d8, 0x00 },
	{ 0x208e, 0x0145, 0x00 },
	{ 0x2090, 0x0006, 0x00 },
	{ 0x2092, 0x0181, 0x00 },
	{ 0x2094, 0x13cc, 0x00 },
	{ 0x2096, 0x2057, 0x00 },
	{ 0x2098, 0x7001, 0x00 },
	{ 0x209a, 0x0fca, 0x00 },
	{ 0x209c, 0x00cb, 0x00 },
	{ 0x209e, 0x009f, 0x00 },
	{ 0x20a0, 0x7002, 0x00 },
	{ 0x20a2, 0x13cc, 0x00 },
	{ 0x20a4, 0x019b, 0x00 },
	{ 0x20a6, 0x014d, 0x00 },
	{ 0x20a8, 0x2987, 0x00 },
	{ 0x20aa, 0x2766, 0x00 },
	{ 0x20ac, 0x0020, 0x00 },
	{ 0x20ae, 0x2060, 0x00 },
	{ 0x20b0, 0x0e5d, 0x00 },
	{ 0x20b2, 0x181d, 0x00 },
	{ 0x20b4, 0x2066, 0x00 },
	{ 0x20b6, 0x20c4, 0x00 },
	{ 0x20b8, 0x50a0, 0x00 },
	{ 0x20ba, 0x0005, 0x00 },
	{ 0x20bc, 0x0000, 0x00 },
	{ 0x20be, 0x01db, 0x00 },
	{ 0x20c0, 0x025a, 0x00 },
	{ 0x20c2, 0x00c0, 0x00 },
	{ 0x20c4, 0x0005, 0x00 },
	{ 0x20c6, 0x0006, 0x00 },
	{ 0x20c8, 0x0ad9, 0x00 },
	{ 0x20ca, 0x0259, 0x00 },
	{ 0x20cc, 0x0618, 0x00 },
	{ 0x20ce, 0x0258, 0x00 },
	{ 0x20d0, 0x2266, 0x00 },
	{ 0x20d2, 0x20c8, 0x00 },
	{ 0x20d4, 0x2060, 0x00 },
	{ 0x20d6, 0x707b, 0x00 },
	{ 0x20d8, 0x0fdd, 0x00 },
	{ 0x20da, 0x86b8, 0x00 },
	{ 0x20dc, 0x50e0, 0x00 },
	{ 0x20de, 0x0020, 0x00 },
	{ 0x20e0, 0x5100, 0x00 },
	{ 0x20e2, 0x3143, 0x00 },
	{ 0x20e4, 0x5121, 0x00 },
	{ 0x20e6, 0x7800, 0x00 },
	{ 0x20e8, 0x3140, 0x00 },
	{ 0x20ea, 0x01c4, 0x00 },
	{ 0x20ec, 0x01c1, 0x00 },
	{ 0x20ee, 0x01c0, 0x00 },
	{ 0x20f0, 0x01c4, 0x00 },
	{ 0x20f2, 0x2700, 0x00 },
	{ 0x20f4, 0x3d40, 0x00 },
	{ 0x20f6, 0x7800, 0x00 },
	{ 0x20f8, 0xffff, 0x00 },
	{ 0x27fe, 0xe000, 0x00 },
	{ 0x3000, 0x60f8, 0x00 },
	{ 0x3002, 0x187f, 0x00 },
	{ 0x3004, 0x7060, 0x00 },
	{ 0x3006, 0x0114, 0x00 },
	{ 0x3008, 0x60b0, 0x00 },
	{ 0x300a, 0x1473, 0x00 },
	{ 0x300c, 0x0013, 0x00 },
	{ 0x300e, 0x140f, 0x00 },
	{ 0x3010, 0x0040, 0x00 },
	{ 0x3012, 0x100f, 0x00 },
	{ 0x3014, 0x60f8, 0x00 },
	{ 0x3016, 0x187f, 0x00 },
	{ 0x3018, 0x7060, 0x00 },
	{ 0x301a, 0x0114, 0x00 },
	{ 0x301c, 0x60b0, 0x00 },
	{ 0x301e, 0x1473, 0x00 },
	{ 0x3020, 0x0013, 0x00 },
	{ 0x3022, 0x140f, 0x00 },
	{ 0x3024, 0x0040, 0x00 },
	{ 0x3026, 0x000f, 0x00 },
	{ 0x0b00, 0x0000, 0x00 },
	{ 0x0b02, 0x0045, 0x00 },
	{ 0x0b04, 0xb405, 0x00 },
	{ 0x0b06, 0xc403, 0x00 },
	{ 0x0b08, 0x0081, 0x00 },
	{ 0x0b0a, 0x8252, 0x00 },
	{ 0x0b0c, 0xf814, 0x00 },
	{ 0x0b0e, 0xc618, 0x00 },
	{ 0x0b10, 0xa828, 0x00 },
	{ 0x0b12, 0x002c, 0x00 },
	{ 0x0b14, 0x4068, 0x00 },
	{ 0x0b16, 0x0000, 0x00 },
	{ 0x0f30, 0x6a25, 0x00 },
	{ 0x0f32, 0x7067, 0x00 },
	{ 0x0954, 0x0009, 0x00 },
	{ 0x0956, 0x0000, 0x00 },
	{ 0x0958, 0xbb80, 0x00 },
	{ 0x095a, 0x5140, 0x00 },
	{ 0x0c00, 0x1110, 0x00 },
	{ 0x0c02, 0x0011, 0x00 },
	{ 0x0c04, 0x0000, 0x00 },
	{ 0x0c06, 0x0200, 0x00 },
	{ 0x0c10, 0x0040, 0x00 },
	{ 0x0c12, 0x0040, 0x00 },
	{ 0x0c14, 0x0040, 0x00 },
	{ 0x0c16, 0x0040, 0x00 },
	{ 0x0a10, 0x4000, 0x00 },
	{ 0x3068, 0xf800, 0x00 },
	{ 0x306a, 0xf876, 0x00 },
	{ 0x006c, 0x0000, 0x00 },
	{ 0x005e, 0x0200, 0x00 },
	{ 0x000e, 0x0100, 0x00 },
	{ 0x0e0a, 0x0001, 0x00 },
	{ 0x004a, 0x0100, 0x00 },
	{ 0x004c, 0x0000, 0x00 },
	{ 0x004e, 0x0100, 0x00 },
	{ 0x000c, 0x0022, 0x00 },
	{ 0x0008, 0x0b00, 0x00 },
	{ 0x005a, 0x0202, 0x00 },
	{ 0x0012, 0x000e, 0x00 },
	{ 0x0018, 0x0a31, 0x00 },
	{ 0x0022, 0x0008, 0x00 },
	{ 0x0028, 0x0017, 0x00 },
	{ 0x0024, 0x0028, 0x00 },
	{ 0x002a, 0x002d, 0x00 },
	{ 0x0026, 0x0030, 0x00 },
	{ 0x002c, 0x07c7, 0x00 },
	{ 0x002e, 0x1111, 0x00 },
	{ 0x0030, 0x1111, 0x00 },
	{ 0x0032, 0x1111, 0x00 },
	{ 0x0006, 0x07d5, 0x00 },
	{ 0x0a22, 0x0000, 0x00 },
	{ 0x0a12, 0x0a20, 0x00 },
	{ 0x0a14, 0x0798, 0x00 },
	{ 0x003e, 0x0000, 0x00 },
	{ 0x0074, 0x07d3, 0x00 },
	{ 0x0070, 0x03e9, 0x00 },
	{ 0x0002, 0x0000, 0x00 },
	{ 0x0a02, 0x0100, 0x00 },
	{ 0x0a24, 0x0100, 0x00 },
	{ 0x0046, 0x0000, 0x00 },
	{ 0x0076, 0x0000, 0x00 },
	{ 0x0060, 0x0000, 0x00 },
	{ 0x0062, 0x0530, 0x00 },
	{ 0x0064, 0x0500, 0x00 },
	{ 0x0066, 0x0530, 0x00 },
	{ 0x0068, 0x0500, 0x00 },
	{ 0x0122, 0x0300, 0x00 },
	{ 0x015a, 0xff08, 0x00 },
	{ 0x0804, 0x0208, 0x00 },
	{ 0x005c, 0x0102, 0x00 },
	{ 0x0a1a, 0x0800, 0x00 },
};

static struct imgsensor_i2c_reg preview_setting[] = {
	{ 0x0a00, 0x0000, 0x00 },
	{ 0x0b0a, 0x8259, 0x00 },
	{ 0x0f30, 0x6a25, 0x00 },
	{ 0x0f32, 0x7167, 0x00 },
	{ 0x004a, 0x0100, 0x00 },
	{ 0x004c, 0x0000, 0x00 },
	{ 0x004e, 0x0100, 0x00 },
	{ 0x000c, 0x0122, 0x00 },
	{ 0x0008, 0x0b00, 0x00 },
	{ 0x005a, 0x0404, 0x00 },
	{ 0x0012, 0x000c, 0x00 },
	{ 0x0018, 0x0a33, 0x00 },
	{ 0x0022, 0x0008, 0x00 },
	{ 0x0028, 0x0017, 0x00 },
	{ 0x0024, 0x0022, 0x00 },
	{ 0x002a, 0x002b, 0x00 },
	{ 0x0026, 0x0030, 0x00 },
	{ 0x002c, 0x07c7, 0x00 },
	{ 0x002e, 0x3311, 0x00 },
	{ 0x0030, 0x3311, 0x00 },
	{ 0x0032, 0x3311, 0x00 },
	{ 0x0006, 0x07d5, 0x00 },
	{ 0x0a22, 0x0000, 0x00 },
	{ 0x0a12, 0x0510, 0x00 },
	{ 0x0a14, 0x03cc, 0x00 },
	{ 0x003e, 0x0000, 0x00 },
	{ 0x0074, 0x07d3, 0x00 },
	{ 0x0070, 0x03e9, 0x00 },
	{ 0x0804, 0x0200, 0x00 },
	{ 0x0a04, 0x016a, 0x00 },
	{ 0x090e, 0x0010, 0x00 },
	{ 0x090c, 0x09c0, 0x00 },
	{ 0x0902, 0x4319, 0x00 },
	{ 0x0914, 0xc106, 0x00 },
	{ 0x0916, 0x040e, 0x00 },
	{ 0x0918, 0x0304, 0x00 },
	{ 0x091a, 0x0708, 0x00 },
	{ 0x091c, 0x0e06, 0x00 },
	{ 0x091e, 0x0300, 0x00 },
	{ 0x0958, 0xbb80, 0x00 },
};

static struct imgsensor_i2c_reg capture_setting[] = {
	{ 0x0a00, 0x0000, 0x00 },
	{ 0x0b0a, 0x8252, 0x00 },
	{ 0x0f30, 0x6a25, 0x00 },
	{ 0x0f32, 0x7067, 0x00 },
	{ 0x004a, 0x0100, 0x00 },
	{ 0x004c, 0x0000, 0x00 },
	{ 0x004e, 0x0100, 0x00 },
	{ 0x000c, 0x0022, 0x00 },
	{ 0x0008, 0x0b00, 0x00 },
	{ 0x005a, 0x0202, 0x00 },
	{ 0x0012, 0x000e, 0x00 },
	{ 0x0018, 0x0a31, 0x00 },
	{ 0x0022, 0x0008, 0x00 },
	{ 0x0028, 0x0017, 0x00 },
	{ 0x0024, 0x0028, 0x00 },
	{ 0x002a, 0x002d, 0x00 },
	{ 0x0026, 0x0030, 0x00 },
	{ 0x002c, 0x07c7, 0x00 },
	{ 0x002e, 0x1111, 0x00 },
	{ 0x0030, 0x1111, 0x00 },
	{ 0x0032, 0x1111, 0x00 },
	{ 0x0006, 0x07d5, 0x00 },
	{ 0x0116, 0x07b6, 0x00 },
	{ 0x0a22, 0x0000, 0x00 },
	{ 0x0a12, 0x0a20, 0x00 },
	{ 0x0a14, 0x0798, 0x00 },
	{ 0x003e, 0x0000, 0x00 },
	{ 0x0074, 0x07d3, 0x00 },
	{ 0x0070, 0x03e9, 0x00 },
	{ 0x0804, 0x0200, 0x00 },
	{ 0x0a04, 0x014a, 0x00 },
	{ 0x090c, 0x0fdc, 0x00 },
	{ 0x090e, 0x002d, 0x00 },
	{ 0x0902, 0x4319, 0x00 },
	{ 0x0914, 0xc10a, 0x00 },
	{ 0x0916, 0x071f, 0x00 },
	{ 0x0918, 0x0408, 0x00 },
	{ 0x091a, 0x0c0d, 0x00 },
	{ 0x091c, 0x0f09, 0x00 },
	{ 0x091e, 0x0a00, 0x00 },
	{ 0x0958, 0xbb80, 0x00 },
};

static struct imgsensor_i2c_reg video_setting[] = {
	{ 0x0a00, 0x0000, 0x00 },
	{ 0x0b0a, 0x8252, 0x00 },
	{ 0x0f30, 0x6a25, 0x00 },
	{ 0x0f32, 0x7067, 0x00 },
	{ 0x004a, 0x0100, 0x00 },
	{ 0x004c, 0x0000, 0x00 },
	{ 0x004e, 0x0100, 0x00 },
	{ 0x000c, 0x0022, 0x00 },
	{ 0x0008, 0x0b00, 0x00 },
	{ 0x005a, 0x0202, 0x00 },
	{ 0x0012, 0x000e, 0x00 },
	{ 0x0018, 0x0a31, 0x00 },
	{ 0x0022, 0x0008, 0x00 },
	{ 0x0028, 0x0017, 0x00 },
	{ 0x0024, 0x0028, 0x00 },
	{ 0x002a, 0x002d, 0x00 },
	{ 0x0026, 0x0030, 0x00 },
	{ 0x002c, 0x07c7, 0x00 },
	{ 0x002e, 0x1111, 0x00 },
	{ 0x0030, 0x1111, 0x00 },
	{ 0x0032, 0x1111, 0x00 },
	{ 0x0006, 0x07d5, 0x00 },
	{ 0x0116, 0x07b6, 0x00 },
	{ 0x0a22, 0x0000, 0x00 },
	{ 0x0a12, 0x0a20, 0x00 },
	{ 0x0a14, 0x0798, 0x00 },
	{ 0x003e, 0x0000, 0x00 },
	{ 0x0074, 0x07d3, 0x00 },
	{ 0x0070, 0x03e9, 0x00 },
	{ 0x0804, 0x0200, 0x00 },
	{ 0x0a04, 0x014a, 0x00 },
	{ 0x090c, 0x0fdc, 0x00 },
	{ 0x090e, 0x002d, 0x00 },
	{ 0x0902, 0x4319, 0x00 },
	{ 0x0914, 0xc10a, 0x00 },
	{ 0x0916, 0x071f, 0x00 },
	{ 0x0918, 0x0408, 0x00 },
	{ 0x091a, 0x0c0d, 0x00 },
	{ 0x091c, 0x0f09, 0x00 },
	{ 0x091e, 0x0a00, 0x00 },
	{ 0x0958, 0xbb80, 0x00 },
};
static struct imgsensor_i2c_reg cust2_setting[] = {
	{ 0x0a00, 0x0000, 0x00 },
	{ 0x0b0a, 0x8252, 0x00 },
	{ 0x0f30, 0x6a25, 0x00 },
	{ 0x0f32, 0x7067, 0x00 },
	{ 0x004a, 0x0100, 0x00 },
	{ 0x004c, 0x0000, 0x00 },
	{ 0x004e, 0x0100, 0x00 },
	{ 0x000c, 0x0022, 0x00 },
	{ 0x0008, 0x0b00, 0x00 },
	{ 0x005a, 0x0202, 0x00 },
	{ 0x0012, 0x000e, 0x00 },
	{ 0x0018, 0x0a31, 0x00 },
	{ 0x0022, 0x0008, 0x00 },
	{ 0x0028, 0x0017, 0x00 },
	{ 0x0024, 0x0028, 0x00 },
	{ 0x002a, 0x002d, 0x00 },
	{ 0x0026, 0x0030, 0x00 },
	{ 0x002c, 0x07c7, 0x00 },
	{ 0x002e, 0x1111, 0x00 },
	{ 0x0030, 0x1111, 0x00 },
	{ 0x0032, 0x1111, 0x00 },
	{ 0x0006, 0x07d5, 0x00 },
	{ 0x0116, 0x07b6, 0x00 },
	{ 0x0a22, 0x0000, 0x00 },
	{ 0x0a12, 0x0a20, 0x00 },
	{ 0x0a14, 0x0798, 0x00 },
	{ 0x003e, 0x0000, 0x00 },
	{ 0x0074, 0x07d3, 0x00 },
	{ 0x0070, 0x03e9, 0x00 },
	{ 0x0804, 0x0200, 0x00 },
	{ 0x0a04, 0x014a, 0x00 },
	{ 0x090c, 0x0fdc, 0x00 },
	{ 0x090e, 0x002d, 0x00 },
	{ 0x0902, 0x4319, 0x00 },
	{ 0x0914, 0xc10a, 0x00 },
	{ 0x0916, 0x071f, 0x00 },
	{ 0x0918, 0x0408, 0x00 },
	{ 0x091a, 0x0c0d, 0x00 },
	{ 0x091c, 0x0f09, 0x00 },
	{ 0x091e, 0x0a00, 0x00 },
	{ 0x0958, 0xbb80, 0x00 },
};

static struct imgsensor_info_t imgsensor_info = {
	.sensor_id_reg = 0x0F16,
	.sensor_id = HI556_SEASONS_SENSOR_ID,
	.checksum_value = 0x55e2a82f,

	.pre = {
		.pclk = 176000000,
		.linelength = 2816,
		.framelength = 2005,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1296,
		.grabwindow_height = 972,
		.mipi_data_lp2hs_settle_dc = 14,
		.max_framerate = 300,
		.mipi_pixel_rate = 142400000,
	},
	.cap = {
		.pclk = 176000000,
		.linelength = 2816,
		.framelength = 2005,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 14,
		.max_framerate = 300,
		.mipi_pixel_rate = 284800000,
	},
	.normal_video = {
		.pclk = 176000000,
		.linelength = 2816,
		.framelength = 2005,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 14,
		.max_framerate = 300,
		.mipi_pixel_rate = 284800000,
	},
	.custom2 = {
		.pclk = 176000000,
		.linelength = 2816,
		.framelength = 2005,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 14,
		.max_framerate = 300,
		.mipi_pixel_rate = 284800000,
	},
	.init_setting = {
		.setting = init_setting,
		.size = IMGSENSOR_ARRAY_SIZE(init_setting),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_WORD_DATA,
		.delay = 0,
	},
	.pre_setting = {
		.setting = preview_setting,
		.size = IMGSENSOR_ARRAY_SIZE(preview_setting),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_WORD_DATA,
		.delay = 0,
	},
	.cap_setting = {
		.setting = capture_setting,
		.size = IMGSENSOR_ARRAY_SIZE(capture_setting),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_WORD_DATA,
		.delay = 0,
	},
	.normal_video_setting = {
		.setting = video_setting,
		.size = IMGSENSOR_ARRAY_SIZE(video_setting),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_WORD_DATA,
		.delay = 0,
	},
	.custom2_setting = {
		.setting = cust2_setting,
		.size = IMGSENSOR_ARRAY_SIZE(cust2_setting),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_WORD_DATA,
		.delay = 0,
	},
	.streamon_setting = {
		.setting = stream_on,
		.size = IMGSENSOR_ARRAY_SIZE(stream_on),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_WORD_DATA,
		.delay = 0,
	},
	.streamoff_setting = {
		.setting = stream_off,
		.size = IMGSENSOR_ARRAY_SIZE(stream_off),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_WORD_DATA,
		.delay = 0,
	},

	.margin = 6,
	.min_shutter = 6,
	.max_frame_length = 0x7FFF,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 7,

	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,
	.custom2_delay_frame = 3,

	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = 1,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	.i2c_addr_table = { 0x40, 0xff },
	.addr_type = IMGSENSOR_I2C_WORD_ADDR,
};

static struct imgsensor_t imgsensor = {
	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x0100,
	.gain = 0xe0,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x40,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	{	/* preview */
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
	}, { /* capture */
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
	}, { /* video */
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
	}, { /* hs_video */
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
	}, { /* sl_video */
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
	}, { /* custom1 reserved */
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
	}, { /* custom2 face unlocked  */
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

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData,
				u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);
extern int iBurstWriteReg(u8 *pData, u32 bytes, u16 i2cId);

extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId,
	u16 transfer_length, u16 timing);
#endif