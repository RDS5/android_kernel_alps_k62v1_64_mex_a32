/*
 * s5k5e9yx_sunrise_sensor.h
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * s5k5e9yx_sunrise image sensor config settings
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

#ifndef _S5K5E9YX_SUNRISE_H
#define _S5K5E9YX_SUNRISE_H


#include <linux/delay.h>
#include <linux/types.h>
#include <securec.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "imgsensor_sensor_i2c.h"
#include "imgsensor_sensor_common.h"
#include "kd_imgsensor.h"


#define SENSOR_FRM_LENGTH_LINE_REG_H 0x0340
#define SENSOR_FRM_LENGTH_LINE_REG_L 0x0341
#define SENSOR_LINE_LENGTH_PCK_REG_H 0x0342
#define SENSOR_LINE_LENGTH_PCK_REG_L 0x0343
#define SENSOR_COARSE_INT_TIME_REG_H 0x0202
#define SENSOR_COARSE_INT_TIME_REG_L 0x0203
#define SENSOR_ANA_GAIN_REG_H 0x0204
#define SENSOR_ANA_GAIN_REG_L 0x0205
#define SENSOR_FRM_CNT_REG 0x0005

#define OTP_MODULE_BASIC_ADDRESS 0x0A05
#define OTP_AWB_BASIC_ADDRESS  0x0A27
#define OTP_MODULE_FLAG_ADDRESS 0x0A04
#define OTP_AWB_FLAG_ADDRESS  0x0A26
#define BASIC_GAIN 0x200
#define OTP_ENABLE 1
#define SANGLAISHI_MOD_ID 0x0c
#define INFO_START_ADDR1   0x0A05
#define INFO_START_ADDR2   0x0A0F
#define TURE 1
#define FAIL 0
static char otp_read_flag = FAIL;
extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData,
				u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);

struct otp_struct {
	kal_uint8 flag; // 0x00:empty, 0x55:valid, other:invalid

	kal_uint16 unit_rg_ratio; // unit awb
	kal_uint16 unit_bg_ratio;
	kal_uint16 unit_grgb_ratio;
	kal_uint16 typical_rg_ratio; // golden awb
	kal_uint16 typical_bg_ratio; // golden awb
	kal_uint16 typical_grgb_ratio;

	bool module_valid;
	bool awb_valid;
};
static struct imgsensor_i2c_reg stream_on[] = {
	{ 0x0100, 0x01, 0x0a },
};

static struct imgsensor_i2c_reg stream_off[] = {
	{ 0x0100, 0x00, 0x0a },
};

static struct imgsensor_i2c_reg init_setting[] = {
	{ 0x0100, 0x00, 0x00 },
	{ 0x0000, 0x04, 0x00 },
	{ 0x0000, 0x55, 0x00 },
	{ 0x0A02, 0x3F, 0x00 },
	{ 0x3B45, 0x01, 0x00 },
	{ 0x0B05, 0x01, 0x00 },
	{ 0x392F, 0x01, 0x00 },
	{ 0x3930, 0x00, 0x00 },
	{ 0x3924, 0x7F, 0x00 },
	{ 0x3925, 0xFD, 0x00 },
	{ 0x3C08, 0xFF, 0x00 },
	{ 0x3C09, 0xFF, 0x00 },
	{ 0x3C31, 0xFF, 0x00 },
	{ 0x3C32, 0xFF, 0x00 },
	{ 0x3290, 0x10, 0x00 },
	{ 0x3200, 0x01, 0x00 },
	{ 0x3074, 0x06, 0x00 },
	{ 0x3075, 0x2F, 0x00 },
	{ 0x308A, 0x20, 0x00 },
	{ 0x308B, 0x08, 0x00 },
	{ 0x308C, 0x0B, 0x00 },
	{ 0x3081, 0x07, 0x00 },
	{ 0x307B, 0x85, 0x00 },
	{ 0x307A, 0x0A, 0x00 },
	{ 0x3079, 0x0A, 0x00 },
	{ 0x306E, 0x71, 0x00 },
	{ 0x306F, 0x28, 0x00 },
	{ 0x301F, 0x20, 0x00 },
	{ 0x3012, 0x4E, 0x00 },
	{ 0x306B, 0x9A, 0x00 },
	{ 0x3091, 0x16, 0x00 },
	{ 0x30C4, 0x06, 0x00 },
	{ 0x306A, 0x79, 0x00 },
	{ 0x30B0, 0xFF, 0x00 },
	{ 0x306D, 0x08, 0x00 },
	{ 0x3084, 0x16, 0x00 },
	{ 0x3070, 0x0F, 0x00 },
	{ 0x30C2, 0x05, 0x00 },
	{ 0x3069, 0x87, 0x00 },
	{ 0x3C0F, 0x00, 0x00 },
	{ 0x3083, 0x14, 0x00 },
	{ 0x3080, 0x08, 0x00 },
	{ 0x3C34, 0xEA, 0x00 },
	{ 0x3C35, 0x5C, 0x00 },
	{ 0x3931, 0x01, 0x00 },
	{ 0x3932, 0x40, 0x00 },
};

static struct imgsensor_i2c_reg preview_setting[] = {
	{ 0x0100, 0x00, 0x00 },
	{ 0x0136, 0x18, 0x00 },
	{ 0x0137, 0x00, 0x00 },
	{ 0x0305, 0x04, 0x00 },
	{ 0x0306, 0x00, 0x00 },
	{ 0x0307, 0x5F, 0x00 },
	{ 0x030D, 0x04, 0x00 },
	{ 0x030E, 0x00, 0x00 },
	{ 0x030F, 0x8B, 0x00 },
	{ 0x3C1F, 0x00, 0x00 },
	{ 0x3C17, 0x00, 0x00 },
	{ 0x0112, 0x0A, 0x00 },
	{ 0x0113, 0x0A, 0x00 },
	{ 0x0114, 0x01, 0x00 },
	{ 0x0820, 0x03, 0x00 },
	{ 0x0821, 0x42, 0x00 },
	{ 0x0822, 0x00, 0x00 },
	{ 0x0823, 0x00, 0x00 },
	{ 0x3929, 0x0F, 0x00 },
	{ 0x0344, 0x00, 0x00 },
	{ 0x0345, 0x08, 0x00 },
	{ 0x0346, 0x00, 0x00 },
	{ 0x0347, 0x08, 0x00 },
	{ 0x0348, 0x0A, 0x00 },
	{ 0x0349, 0x27, 0x00 },
	{ 0x034A, 0x07, 0x00 },
	{ 0x034B, 0x9F, 0x00 },
	{ 0x034C, 0x05, 0x00 },
	{ 0x034D, 0x10, 0x00 },
	{ 0x034E, 0x03, 0x00 },
	{ 0x034F, 0xCC, 0x00 },
	{ 0x0900, 0x01, 0x00 },
	{ 0x0901, 0x22, 0x00 },
	{ 0x0381, 0x01, 0x00 },
	{ 0x0383, 0x01, 0x00 },
	{ 0x0385, 0x01, 0x00 },
	{ 0x0387, 0x03, 0x00 },
	{ 0x0101, 0x00, 0x00 },
	{ 0x0340, 0x03, 0x00 },
	{ 0x0341, 0xF9, 0x00 },
	{ 0x0342, 0x0C, 0x00 },
	{ 0x0343, 0x28, 0x00 },
	{ 0x0200, 0x0B, 0x00 },
	{ 0x0201, 0x9C, 0x00 },
	{ 0x0202, 0x00, 0x00 },
	{ 0x0203, 0x02, 0x00 },
	{ 0x30B8, 0x2A, 0x00 },
	{ 0x30BA, 0x2E, 0x00 },
	{ 0x0100, 0x01, 0x00 },
};

static struct imgsensor_i2c_reg capture_setting[] = {
	{ 0x0100, 0x00, 0x00 },
	{ 0x0136, 0x18, 0x00 },
	{ 0x0137, 0x00, 0x00 },
	{ 0x0305, 0x04, 0x00 },
	{ 0x0306, 0x00, 0x00 },
	{ 0x0307, 0x5F, 0x00 },
	{ 0x030D, 0x04, 0x00 },
	{ 0x030E, 0x00, 0x00 },
	{ 0x030F, 0x8B, 0x00 },
	{ 0x3C1F, 0x00, 0x00 },
	{ 0x3C17, 0x00, 0x00 },
	{ 0x0112, 0x0A, 0x00 },
	{ 0x0113, 0x0A, 0x00 },
	{ 0x0114, 0x01, 0x00 },
	{ 0x0820, 0x03, 0x00 },
	{ 0x0821, 0x42, 0x00 },
	{ 0x0822, 0x00, 0x00 },
	{ 0x0823, 0x00, 0x00 },
	{ 0x3929, 0x0F, 0x00 },
	{ 0x0344, 0x00, 0x00 },
	{ 0x0345, 0x08, 0x00 },
	{ 0x0346, 0x00, 0x00 },
	{ 0x0347, 0x08, 0x00 },
	{ 0x0348, 0x0A, 0x00 },
	{ 0x0349, 0x27, 0x00 },
	{ 0x034A, 0x07, 0x00 },
	{ 0x034B, 0x9f, 0x00 },
	{ 0x034C, 0x0A, 0x00 },
	{ 0x034D, 0x20, 0x00 },
	{ 0x034E, 0x07, 0x00 },
	{ 0x034F, 0x98, 0x00 },
	{ 0x0900, 0x00, 0x00 },
	{ 0x0901, 0x00, 0x00 },
	{ 0x0381, 0x01, 0x00 },
	{ 0x0383, 0x01, 0x00 },
	{ 0x0385, 0x01, 0x00 },
	{ 0x0387, 0x01, 0x00 },
	{ 0x0101, 0x00, 0x00 },
	{ 0x0340, 0x07, 0x00 },
	{ 0x0341, 0xEE, 0x00 },
	{ 0x0342, 0x0C, 0x00 },
	{ 0x0343, 0x28, 0x00 },
	{ 0x0200, 0x0B, 0x00 },
	{ 0x0201, 0x9C, 0x00 },
	{ 0x0202, 0x00, 0x00 },
	{ 0x0203, 0x02, 0x00 },
	{ 0x30B8, 0x2E, 0x00 },
	{ 0x30BA, 0x36, 0x00 },
	{ 0x0100, 0x01, 0x00 },
};

static struct imgsensor_i2c_reg video_setting[] = {
	{ 0x0100, 0x00, 0x00 },
	{ 0x0136, 0x18, 0x00 },
	{ 0x0137, 0x00, 0x00 },
	{ 0x0305, 0x04, 0x00 },
	{ 0x0306, 0x00, 0x00 },
	{ 0x0307, 0x5F, 0x00 },
	{ 0x030D, 0x04, 0x00 },
	{ 0x030E, 0x00, 0x00 },
	{ 0x030F, 0x8B, 0x00 },
	{ 0x3C1F, 0x00, 0x00 },
	{ 0x3C17, 0x00, 0x00 },
	{ 0x0112, 0x0A, 0x00 },
	{ 0x0113, 0x0A, 0x00 },
	{ 0x0114, 0x01, 0x00 },
	{ 0x0820, 0x03, 0x00 },
	{ 0x0821, 0x42, 0x00 },
	{ 0x0822, 0x00, 0x00 },
	{ 0x0823, 0x00, 0x00 },
	{ 0x3929, 0x0F, 0x00 },
	{ 0x0344, 0x00, 0x00 },
	{ 0x0345, 0x08, 0x00 },
	{ 0x0346, 0x00, 0x00 },
	{ 0x0347, 0x08, 0x00 },
	{ 0x0348, 0x0A, 0x00 },
	{ 0x0349, 0x27, 0x00 },
	{ 0x034A, 0x07, 0x00 },
	{ 0x034B, 0x9f, 0x00 },
	{ 0x034C, 0x0A, 0x00 },
	{ 0x034D, 0x20, 0x00 },
	{ 0x034E, 0x07, 0x00 },
	{ 0x034F, 0x98, 0x00 },
	{ 0x0900, 0x00, 0x00 },
	{ 0x0901, 0x00, 0x00 },
	{ 0x0381, 0x01, 0x00 },
	{ 0x0383, 0x01, 0x00 },
	{ 0x0385, 0x01, 0x00 },
	{ 0x0387, 0x01, 0x00 },
	{ 0x0101, 0x00, 0x00 },
	{ 0x0340, 0x07, 0x00 },
	{ 0x0341, 0xEE, 0x00 },
	{ 0x0342, 0x0C, 0x00 },
	{ 0x0343, 0x28, 0x00 },
	{ 0x0200, 0x0B, 0x00 },
	{ 0x0201, 0x9C, 0x00 },
	{ 0x0202, 0x00, 0x00 },
	{ 0x0203, 0x02, 0x00 },
	{ 0x30B8, 0x2E, 0x00 },
	{ 0x30BA, 0x36, 0x00 },
	{ 0x0100, 0x01, 0x00 },
};

static struct imgsensor_i2c_reg cust2_setting[] = {
	{ 0x0100, 0x00, 0x00 },
	{ 0x0136, 0x18, 0x00 },
	{ 0x0137, 0x00, 0x00 },
	{ 0x0305, 0x04, 0x00 },
	{ 0x0306, 0x00, 0x00 },
	{ 0x0307, 0x5F, 0x00 },
	{ 0x030D, 0x04, 0x00 },
	{ 0x030E, 0x00, 0x00 },
	{ 0x030F, 0x8B, 0x00 },
	{ 0x3C1F, 0x00, 0x00 },
	{ 0x3C17, 0x00, 0x00 },
	{ 0x0112, 0x0A, 0x00 },
	{ 0x0113, 0x0A, 0x00 },
	{ 0x0114, 0x01, 0x00 },
	{ 0x0820, 0x03, 0x00 },
	{ 0x0821, 0x42, 0x00 },
	{ 0x0822, 0x00, 0x00 },
	{ 0x0823, 0x00, 0x00 },
	{ 0x3929, 0x0F, 0x00 },
	{ 0x0344, 0x00, 0x00 },
	{ 0x0345, 0x08, 0x00 },
	{ 0x0346, 0x00, 0x00 },
	{ 0x0347, 0x08, 0x00 },
	{ 0x0348, 0x0A, 0x00 },
	{ 0x0349, 0x27, 0x00 },
	{ 0x034A, 0x07, 0x00 },
	{ 0x034B, 0x9f, 0x00 },
	{ 0x034C, 0x0A, 0x00 },
	{ 0x034D, 0x20, 0x00 },
	{ 0x034E, 0x07, 0x00 },
	{ 0x034F, 0x98, 0x00 },
	{ 0x0900, 0x00, 0x00 },
	{ 0x0901, 0x00, 0x00 },
	{ 0x0381, 0x01, 0x00 },
	{ 0x0383, 0x01, 0x00 },
	{ 0x0385, 0x01, 0x00 },
	{ 0x0387, 0x01, 0x00 },
	{ 0x0101, 0x00, 0x00 },
	{ 0x0340, 0x07, 0x00 },
	{ 0x0341, 0xEE, 0x00 },
	{ 0x0342, 0x0C, 0x00 },
	{ 0x0343, 0x28, 0x00 },
	{ 0x0200, 0x0B, 0x00 },
	{ 0x0201, 0x9C, 0x00 },
	{ 0x0202, 0x00, 0x00 },
	{ 0x0203, 0x02, 0x00 },
	{ 0x30B8, 0x2E, 0x00 },
	{ 0x30BA, 0x36, 0x00 },
	{ 0x0100, 0x01, 0x00 },
};

static struct imgsensor_i2c_reg_table dump_setting[] = {
	{ 0x0100, 0x0100, IMGSENSOR_I2C_WORD_DATA, IMGSENSOR_I2C_READ, 0 },
	{ 0x0005, 0x0000, IMGSENSOR_I2C_WORD_DATA, IMGSENSOR_I2C_READ, 0 },
	{ 0x0340, 0x0000, IMGSENSOR_I2C_WORD_DATA, IMGSENSOR_I2C_READ, 0 },
	{ 0x0342, 0x0000, IMGSENSOR_I2C_WORD_DATA, IMGSENSOR_I2C_READ, 0 },
	{ 0x0204, 0x0000, IMGSENSOR_I2C_WORD_DATA, IMGSENSOR_I2C_READ, 0 },
	{ 0x0302, 0x0000, IMGSENSOR_I2C_WORD_DATA, IMGSENSOR_I2C_READ, 0 },
};

static struct imgsensor_info_t imgsensor_info = {
	.sensor_id_reg = 0x0000,
	.sensor_id = S5K5E9YX_SUNRISE_SENSOR_ID,
	.checksum_value = 0x2ae69154,

	.pre = {
		.pclk = 190000000,
		.linelength = 3112,
		.framelength = 1017,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1296,
		.grabwindow_height = 972,
		/*
		 * following for MIPIDataLowPwr2HighSpeedSettleDelayCount by
		 * different scenario
		 */
		.mipi_data_lp2hs_settle_dc = 85,
		/* following for GetDefaultFramerateByScenario() */
		.max_framerate = 300,
		.mipi_pixel_rate = 166400000,
	},

	.cap = {
		.pclk = 190000000,
		.linelength = 3112,
		.framelength = 2030,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		/*
		 * following for MIPIDataLowPwr2HighSpeedSettleDelayCount by
		 * different scenario
		 */
		.mipi_data_lp2hs_settle_dc = 85,
		/* following for GetDefaultFramerateByScenario() */
		.max_framerate = 300,
		.mipi_pixel_rate = 166400000,
	},

	.normal_video = {
		.pclk = 190000000,
		.linelength = 3112,
		.framelength = 2030,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		/*
		 * following for MIPIDataLowPwr2HighSpeedSettleDelayCount by
		 * different scenario
		 */
		.mipi_data_lp2hs_settle_dc = 85,
		/* following for GetDefaultFramerateByScenario() */
		.max_framerate = 300,
		.mipi_pixel_rate = 175200000,
	},

	.custom2 = {
		.pclk = 190000000,
		.linelength = 3112,
		.framelength = 2030,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		/*
		 * following for MIPIDataLowPwr2HighSpeedSettleDelayCount by
		 * different scenario
		 */
		.mipi_data_lp2hs_settle_dc = 85,
		/* following for GetDefaultFramerateByScenario() */
		.max_framerate = 300,
		.mipi_pixel_rate = 166400000,
	},

	.init_setting = {
		.setting = init_setting,
		.size = IMGSENSOR_ARRAY_SIZE(init_setting),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 2,
	},
	.pre_setting = {
		.setting = preview_setting,
		.size = IMGSENSOR_ARRAY_SIZE(preview_setting),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 2,
	},
	.cap_setting = {
		.setting = capture_setting,
		.size = IMGSENSOR_ARRAY_SIZE(capture_setting),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 2,
	},
	.normal_video_setting = {
		.setting = video_setting,
		.size = IMGSENSOR_ARRAY_SIZE(video_setting),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 2,
	},
	.custom2_setting = {
		.setting = cust2_setting,
		.size = IMGSENSOR_ARRAY_SIZE(cust2_setting),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 2,
	},
	.streamon_setting = {
		.setting = stream_on,
		.size = IMGSENSOR_ARRAY_SIZE(stream_on),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 3,
	},

	.streamoff_setting = {
		.setting = stream_off,
		.size = IMGSENSOR_ARRAY_SIZE(stream_off),
		.addr_type = IMGSENSOR_I2C_WORD_ADDR,
		.data_type = IMGSENSOR_I2C_BYTE_DATA,
		.delay = 2,
	},

	.dump_info = {
		.setting = dump_setting,
		.size = IMGSENSOR_ARRAY_SIZE(dump_setting),
	},

	.margin = 6,
	.min_shutter = 2,
	.max_frame_length = 0xFFFF,
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
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	.i2c_addr_table = { 0x5a, 0xff },
	.i2c_speed = 400,
	.addr_type = IMGSENSOR_I2C_WORD_ADDR,
};

static struct imgsensor_t imgsensor = {
	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,
	.gain = 0x0100,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = KAL_FALSE,
	.i2c_write_id = 0x5a,
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

extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId,
	u16 transfer_length,	u16 timing);
#endif
