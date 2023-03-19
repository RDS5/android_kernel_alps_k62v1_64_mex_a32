/*
 * gc5025_sunwin_sensor.h
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * gc5025_luxvisions_sensor image sensor config settings
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

#ifndef _GC5025H_SUNWIN_SENSOR_H
#define _GC5025H_SUNWIN_SENSOR_H

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

#define SENSOR_CISCTRL_CAPT_VB_REG_H 0x07
#define SENSOR_CISCTRL_CAPT_VB_REG_L 0x08
#define SENSOR_PAGE_SELECT_REG 0xfe
#define SENSOR_SET_GAIN_REG 0xb6
#define SENSOR_SET_GAIN_REG_H 0xb1
#define SENSOR_SET_GAIN_REG_L 0xb2
#define SENSOR_CISCTRL_EXP_REG1 0xd9
#define SENSOR_CISCTRL_EXP_REG2 0xb0
#define SENSOR_CISCTRL_BUF_EXP_IN_REG_H 0x03
#define SENSOR_CISCTRL_BUF_EXP_IN_REG_L 0x04
#define SENSOR_SET_PATTERN_MODE 0x8c

#define ANALOG_GAIN_1 64   /* 1.00x */
#define ANALOG_GAIN_2 91   /* 1.42x */
#define ANALOG_GAIN_3 127  /* 1.99x */

#define SUNWIN_MOD_ID 0x02

/* SENSOR PRIVATE INFO FOR GAIN SETTING */
#define GC5025H_SENSOR_GAIN_BASE             0x40
#define GC5025H_SENSOR_DGAIN_BASE            0x100

/* SENSOR MIRROR FLIP INFO */
#define GC5025H_MIRROR_FLIP_ENABLE    0
#if GC5025H_MIRROR_FLIP_ENABLE
#define GC5025H_MIRROR 0xc3
#define GC5025H_STARTY 0x02
#define GC5025H_STARTX 0x0d
#else
#define GC5025H_MIRROR 0xc0
#define GC5025H_STARTY 0x02
#define GC5025H_STARTX 0x03
#endif

/* SENSOR OTP INFO */
#define GC5025HOTP_FOR_CUSTOMER    1

#define DD_PARAM_QTY        200
#define WINDOW_WIDTH        0x0a30
#define WINDOW_HEIGHT       0x079c
#define REG_ROM_START       0x62
#if  GC5025HOTP_FOR_CUSTOMER
#define RG_TYPICAL          0x0400
#define BG_TYPICAL          0x0400
#define INFO_ROM_START      0x01
#define INFO_WIDTH          0x08
#define WB_ROM_START        0x11
#define WB_WIDTH            0x05
#define GOLDEN_ROM_START    0x1c
#define GOLDEN_WIDTH        0x05
#endif

struct gc5025h_otp {
	kal_uint16 dd_param_x[DD_PARAM_QTY];
	kal_uint16 dd_param_y[DD_PARAM_QTY];
	kal_uint16 dd_param_type[DD_PARAM_QTY];
	kal_uint16 dd_cnt;
	kal_uint16 dd_flag;
	kal_uint16 reg_addr[10];
	kal_uint16 reg_value[10];
	kal_uint16 reg_num;
#if GC5025HOTP_FOR_CUSTOMER
	kal_uint16 module_id;
	kal_uint16 lens_id;
	kal_uint16 vcm_id;
	kal_uint16 vcm_driver_id;
	kal_uint16 year;
	kal_uint16 month;
	kal_uint16 day;
	kal_uint16 rg_gain;
	kal_uint16 bg_gain;
	kal_uint16 wb_flag;
	kal_uint16 golden_flag;
	kal_uint16 golden_rg;
	kal_uint16 golden_bg;
#endif
	kal_uint16 checksum_ok;
};

enum{
	OTP_PAGE0 = 0,
	OTP_PAGE1,
};

enum{
	OTP_CLOSE = 0,
	OTP_OPEN,
};

static kal_uint32 dgain_ratio = GC5025H_SENSOR_DGAIN_BASE;

static struct imgsensor_i2c_reg stream_on[] = {
	{ 0xfe, 0x00, 0x00 },
	{ 0x3f, 0x91, 0x00 },
};

static struct imgsensor_i2c_reg stream_off[] = {
	{ 0xfe, 0x00, 0x00 },
	{ 0x3f, 0x00, 0x00 },
};

static struct imgsensor_i2c_reg init_setting[] = {
	{ 0xfe, 0x00, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xf7, 0x01, 0x00 },
	{ 0xf8, 0x11, 0x00 },
	{ 0xf9, 0x00, 0x00 },
	{ 0xfa, 0xa0, 0x00 },
	{ 0xfc, 0x2a, 0x00 },
	{ 0xfe, 0x03, 0x00 },
	{ 0x01, 0x07, 0x00 },
	{ 0xfc, 0x2e, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0x88, 0x03, 0x00 },
	{ 0x3f, 0x00, 0x00 },

	/*Cisctl&Analog*/
	{ 0x03, 0x07, 0x00 },
	{ 0x04, 0x08, 0x00 },
	{ 0x05, 0x02, 0x00 },
	{ 0x06, 0x58, 0x00 },
	{ 0x08, 0x20, 0x00 },
	{ 0x0a, 0x1c, 0x00 },
	{ 0x0c, 0x04, 0x00 },
	{ 0x0d, 0x07, 0x00 },
	{ 0x0e, 0x9c, 0x00 },
	{ 0x0f, 0x0a, 0x00 },
	{ 0x10, 0x30, 0x00 },
	{ 0x17, GC5025H_MIRROR, 0x00 },
	{ 0x18, 0x02, 0x00 },
	{ 0x19, 0x17, 0x00 },
	{ 0x1a, 0x0a, 0x00 },
	{ 0x1c, 0x2c, 0x00 },
	{ 0x1d, 0x00, 0x00 },
	{ 0x1e, 0xa0, 0x00 },
	{ 0x1f, 0xb0, 0x00 },
	{ 0x20, 0x22, 0x00 },
	{ 0x21, 0x22, 0x00 },
	{ 0x26, 0x22, 0x00 },
	{ 0x25, 0xc1, 0x00 },
	{ 0x27, 0x64, 0x00 },
	{ 0x28, 0x00, 0x00 },
	{ 0x29, 0x44, 0x00 },
	{ 0x2b, 0x80, 0x00 },
	{ 0x2f, 0x4d, 0x00 },
	{ 0x30, 0x11, 0x00 },
	{ 0x31, 0x20, 0x00 },
	{ 0x32, 0xc0, 0x00 },
	{ 0x33, 0x00, 0x00 },
	{ 0x34, 0x60, 0x00 },
	{ 0x38, 0x04, 0x00 },
	{ 0x39, 0x02, 0x00 },
	{ 0x3a, 0x00, 0x00 },
	{ 0x3b, 0x00, 0x00 },
	{ 0x3c, 0x08, 0x00 },
	{ 0x3d, 0x0f, 0x00 },
	{ 0x81, 0x50, 0x00 },
	{ 0xcb, 0x02, 0x00 },
	{ 0xcd, 0x4d, 0x00 },
	{ 0xcf, 0x50, 0x00 },
	{ 0xd0, 0xb3, 0x00 },
	{ 0xd1, 0x19, 0x00 },
	{ 0xd3, 0xc4, 0x00 },
	{ 0xd9, 0xaa, 0x00 },
	{ 0xdc, 0x03, 0x00 },
	{ 0xdd, 0xc8, 0x00 },
	{ 0xe0, 0x00, 0x00 },
	{ 0xe1, 0x1c, 0x00 },
	{ 0xe3, 0x2a, 0x00 },
	{ 0xe4, 0xc0, 0x00 },
	{ 0xe5, 0x06, 0x00 },
	{ 0xe6, 0x10, 0x00 },
	{ 0xe7, 0xc3, 0x00 },
	{ 0xfe, 0x10, 0x00 },
	{ 0xfe, 0x00, 0x00 },
	{ 0xfe, 0x10, 0x00 },
	{ 0xfe, 0x00, 0x00 },

	/* ISP */
	{ 0x80, 0x10, 0x00 },
	{ 0x89, 0x03, 0x00 },
	{ 0x8b, 0x31, 0x00 },
	{ 0xfe, 0x01, 0x00 },
	{ 0x88, 0x00, 0x00 },
	{ 0x8a, 0x03, 0x00 },
	{ 0x8e, 0xc7, 0x00 },

	/* BLK */
	{ 0xfe, 0x00, 0x00 },
	{ 0x40, 0x22, 0x00 },
	{ 0x41, 0x28, 0x00 },
	{ 0x42, 0x04, 0x00 },
	{ 0x43, 0x08, 0x00 },
	{ 0x4e, 0x0f, 0x00 },
	{ 0x4f, 0xf0, 0x00 },
	{ 0x67, 0x0c, 0x00 },
	{ 0xae, 0x40, 0x00 },
	{ 0xaf, 0x04, 0x00 },
	{ 0x60, 0x00, 0x00 },
	{ 0x61, 0x80, 0x00 },

	/* gain */
	{ 0xb0, 0x4d, 0x00 },
	{ 0xb1, 0x01, 0x00 },
	{ 0xb2, 0x00, 0x00 },
	{ 0xb6, 0x00, 0x00 },

	/* Crop window */
	{ 0x91, 0x00, 0x00 },
	{ 0x92, GC5025H_STARTY, 0x00 },
	{ 0x94, GC5025H_STARTX, 0x00 },

	/* MIPI */
	{ 0xfe, 0x03, 0x00 },
	{ 0x02, 0x03, 0x00 },
	{ 0x03, 0x8e, 0x00 },
	{ 0x06, 0x80, 0x00 },
	{ 0x15, 0x00, 0x00 },
	{ 0x16, 0x09, 0x00 },
	{ 0x18, 0x0a, 0x00 },
	{ 0x21, 0x10, 0x00 },
	{ 0x22, 0x05, 0x00 },
	{ 0x23, 0x20, 0x00 },
	{ 0x24, 0x02, 0x00 },
	{ 0x25, 0x20, 0x00 },
	{ 0x26, 0x08, 0x00 },
	{ 0x29, 0x06, 0x00 },
	{ 0x2a, 0x0a, 0x00 },
	{ 0x2b, 0x08, 0x00 },
	{ 0xfe, 0x00, 0x00 },
};

static struct imgsensor_i2c_reg_table dump_setting[] = {
	{ 0xfe, 0x00, IMGSENSOR_I2C_BYTE_DATA, IMGSENSOR_I2C_READ, 0 },
	{ 0x00, 0x00, IMGSENSOR_I2C_BYTE_DATA, IMGSENSOR_I2C_READ, 0 },
};

static struct imgsensor_info_t imgsensor_info = {
	.sensor_id_reg = 0xf0,
	.sensor_id = GC5025H_SUNWIN_SENSOR_ID,
	.checksum_value = 0xD470,

	.pre = {
		.pclk = 288000000,
		.linelength = 4800,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 172800000,
		.max_framerate = 300,
	},

	.cap = {
		.pclk = 288000000,
		.linelength = 4800,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 172800000,
		.max_framerate = 300,
	},

	.normal_video = {
		.pclk = 288000000,
		.linelength = 4800,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 172800000,
		.max_framerate = 300,
	},

	.custom2 = {
		.pclk = 288000000,
		.linelength = 4800,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 172800000,
		.max_framerate = 300,
	},
	.init_setting = {
		.setting = init_setting,
		.size = IMGSENSOR_ARRAY_SIZE(init_setting),
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
	.max_frame_length = 0x27af,
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
#if GC5025H_MIRROR_FLIP_ENABLE
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
#else
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
#endif
	.mclk = 24, // mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
	.mipi_lane_num = SENSOR_MIPI_2_LANE,  // mipi lane num
	.i2c_addr_table = { 0x6c, 0xff },
	.i2c_speed = 400, // i2c read/write speed
	.addr_type = IMGSENSOR_I2C_BYTE_ADDR,
};

static struct imgsensor_t imgsensor = {
	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3ED, // current shutter
	.gain = 0x40, // current gain
	.dummy_pixel = 0, // current dummypixel
	.dummy_line = 0, // current dummyline
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = KAL_FALSE,
	.i2c_write_id = 0x6c,
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
