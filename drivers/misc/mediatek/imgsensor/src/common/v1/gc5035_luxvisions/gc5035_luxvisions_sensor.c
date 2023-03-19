/*
 * gc5035_luxvisions_sensor.c
 *
 * Copyright (c) 2019-2019 Huawei Technologies Co., Ltd.
 *
 * gc5035_luxvisions_sensor image sensor driver
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

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "imgsensor_sensor_common.h"
#include "imgsensor_sensor_i2c.h"
#include "gc5035_luxvisions_sensor.h"

#ifndef TRUE
#define TRUE (bool)1
#endif
#ifndef FALSE
#define FALSE (bool)0
#endif


#define DEBUG_GC5035_LUXVISIONS 0

#define PFX "GC5035_LUXVISIONS"
#define LOG_INF(fmt, args...) \
	do { \
		if (DEBUG_GC5035_LUXVISIONS) \
			pr_err(PFX "[%s] " fmt, __FUNCTION__, ##args); \
	} while (0)
#define LOG_ERR(fmt, args...) pr_err(PFX "[%s] " fmt, __func__, ##args)

#define RETRY_TIMES 2

static DEFINE_SPINLOCK(imgsensor_drv_lock);
static struct gc5035_otp_t gc5035_otp_data;

#define IMGSENSOR_SENSOR_I2C_READ(sensor, addr, data, data_type) \
	do { \
		if (imgsensor_sensor_i2c_read(sensor, addr, data, data_type) != 0) { \
			LOG_ERR("Imgsensor i2c read failed.\n"); \
	} \
	} while (0)

#define IMGSENSOR_SENSOR_I2C_WRITE(sensor, addr, data, data_type) \
	do { \
		if (imgsensor_sensor_i2c_write(sensor, addr, data, data_type) != 0) { \
			LOG_ERR("Imgsensor i2c write failed.\n"); \
		} \
	} while (0)

static void set_dummy(void)
{
	kal_int32 rc = 0;
	kal_uint32 frame_length = imgsensor.frame_length >> 2;

	LOG_INF("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	frame_length = frame_length << 2;
	rc = imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
		0x00, IMGSENSOR_I2C_BYTE_DATA);
	if (rc < 0)
		LOG_ERR("wtire sensor page ctrl reg failed.");
	rc = imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CISCTRL_CAPT_VB_REG_H,
		(frame_length >> 8) & 0x3f, IMGSENSOR_I2C_BYTE_DATA);
	if (rc < 0)
		LOG_ERR("wtire sensor csictrl capt vb  h reg  failed.");
	rc = imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CISCTRL_CAPT_VB_REG_L,
		frame_length & 0xff, IMGSENSOR_I2C_BYTE_DATA);
	if (rc < 0)
		LOG_ERR("wtire sensor csictrl capt vb  l reg  failed.");
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	if (!framerate || !imgsensor.line_length) {
		LOG_ERR("Invalid params. framerate=%d, line_length=%d\n",
			framerate, imgsensor.line_length);
		return;
	}
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
				frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line =
		imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length =
			imgsensor_info.max_frame_length;
		imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}

static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;
    kal_uint16 cal_shutter = 0;
	LOG_INF("ENTER\n");
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter = (shutter >
		(imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) :
		shutter;
	realtime_fps = imgsensor.pclk /
			imgsensor.line_length * 10 /
			imgsensor.frame_length;

	if (imgsensor.autoflicker_en) {

		/* calc fps between 297~305, real fps set to 298 */
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		/* calc fps between 147~150, real fps set to 146 */
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			set_max_framerate(realtime_fps, 0);

	} else
		set_max_framerate(realtime_fps, 0);

	cal_shutter = shutter >> 2;
	cal_shutter = cal_shutter << 2;
	Dgain_ratio = 256 * shutter / cal_shutter;

	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
					 0x00, IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CISCTRL_BUF_EXP_IN_REG_H,
					 (cal_shutter >> 8) & 0x3f, IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CISCTRL_BUF_EXP_IN_REG_L,
					 cal_shutter & 0xff, IMGSENSOR_I2C_BYTE_DATA);
}

static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = gain << 2;

	if (reg_gain < GC5035_SENSOR_GAIN_BASE)
		reg_gain = GC5035_SENSOR_GAIN_BASE;
	else if (reg_gain > GC5035_SENSOR_GAIN_MAX)
		reg_gain = GC5035_SENSOR_GAIN_MAX;

	return (kal_uint16)reg_gain;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint32 reg_gain = 0;
	kal_uint32 temp_gain;
	kal_int16 gain_index;

	reg_gain = gain2reg(gain);
	for (gain_index = GC5035_SENSOR_GAIN_MAX_VALID_INDEX - 1; gain_index >= 0; gain_index--) {
		if (reg_gain >= g_GC5035_AGC_Param[gain_index][0])
			break;
	}
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
						 0x00, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_GAIN_REG,
						 g_GC5035_AGC_Param[gain_index][1], IMGSENSOR_I2C_BYTE_DATA);
	    temp_gain = reg_gain * Dgain_ratio / g_GC5035_AGC_Param[gain_index][0];
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_GAIN_REG_H,
						 (temp_gain >> 8) & 0x0f, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_GAIN_REG_L,
						 temp_gain & 0xfc, IMGSENSOR_I2C_BYTE_DATA);
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.gain = reg_gain;
	    spin_unlock(&imgsensor_drv_lock);
		
	return gain;
}

static kal_uint32 sensor_dump_reg(void)
{
	kal_int32 rc;

	LOG_INF("ENTER\n");
	rc = imgsensor_sensor_i2c_process(&imgsensor,
		&imgsensor_info.dump_info);
	if (rc < 0)
		LOG_ERR("Failed\n");
	LOG_INF("EXIT\n");
	return ERROR_NONE;
}

static kal_uint32 sensor_init(void)
{
	kal_int32 rc;

	LOG_INF("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.init_setting);
	if (rc < 0) {
		LOG_ERR("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_preview_setting(void)
{
	kal_int32 rc;

	LOG_INF("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.pre_setting);
	if (rc < 0) {
		LOG_ERR("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_capture_setting(void)
{
	kal_int32 rc;

	LOG_INF("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.cap_setting);
	if (rc < 0) {
		LOG_ERR("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_normal_video_setting(void)
{
	kal_int32 rc;

	LOG_INF("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
					 &imgsensor_info.normal_video_setting);
	if (rc < 0) {
		LOG_ERR("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 return_sensor_id(void)
{
	kal_int32 rc = 0;
	kal_uint16 sensor_id = 0;

	rc = imgsensor_sensor_i2c_read(&imgsensor, imgsensor_info.sensor_id_reg,
				       &sensor_id, IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		LOG_ERR("Read id failed.id reg: 0x%x\n", imgsensor_info.sensor_id_reg);
		sensor_id = 0xFFFF;
	}
	return sensor_id;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
					 0x01, IMGSENSOR_I2C_BYTE_DATA);
	if (enable) {
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_PATTERN_MODE,
						 0x11, IMGSENSOR_I2C_BYTE_DATA);
	} else {
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_PATTERN_MODE,
						 0x10, IMGSENSOR_I2C_BYTE_DATA);
	}
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
					 0x00, IMGSENSOR_I2C_BYTE_DATA);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = RETRY_TIMES;

	spin_lock(&imgsensor_drv_lock);
	/* init i2c config */
	imgsensor.i2c_speed = imgsensor_info.i2c_speed;
	imgsensor.addr_type = imgsensor_info.addr_type;
	spin_unlock(&imgsensor_drv_lock);

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("id reg: 0x%x, read id: 0x%x, expect id:0x%x\n",
					imgsensor.i2c_write_id,
					*sensor_id,
					imgsensor_info.sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Check sensor id fail, id reg: 0x%x,read id: 0x%x, expect id:0x%x\n",
				imgsensor.i2c_write_id,
				*sensor_id,
				imgsensor_info.sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = RETRY_TIMES;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/*
		 * if Sensor ID is not correct,
		 * Must set *sensor_id to 0xFFFFFFFF
		 */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint16 data)
{
	IMGSENSOR_SENSOR_I2C_WRITE(&imgsensor, addr, data,
		IMGSENSOR_I2C_BYTE_DATA);
}

static kal_uint8 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	IMGSENSOR_SENSOR_I2C_READ(&imgsensor, addr,
		&get_byte, IMGSENSOR_I2C_BYTE_DATA);
	return (kal_uint8)get_byte;
}

static kal_uint8 gc5035_otp_read_byte(kal_uint16 addr)
{
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x69, (addr >> 8) & 0x1f);
	write_cmos_sensor(0x6a, addr & 0xff);
	write_cmos_sensor(0xf3, 0x20);

	return read_cmos_sensor(0x6c);
}

static void gc5035_otp_read_group(kal_uint16 addr,
	kal_uint8 *data, kal_uint16 length)
{
	kal_uint16 i;
	if ((((addr & 0x1fff) >> 3) + length) > GC5035_OTP_DATA_LENGTH) {
		LOG_ERR("out of range, start addr: 0x%.4x, length = %d\n",
			addr & 0x1fff, length);
		return;
	}

	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x69, (addr >> 8) & 0x1f);
	write_cmos_sensor(0x6a, addr & 0xff);
	write_cmos_sensor(0xf3, 0x20);
	write_cmos_sensor(0xf3, 0x12);

	for (i = 0; i < length; i++) {
		data[i] = read_cmos_sensor(0x6c);
	}

	write_cmos_sensor(0xf3, 0x00);
}

static void gc5035_gcore_read_dpc(void)
{
	kal_uint8 dpcFlag;
	struct gc5035_dpc_t *pDPC = &gc5035_otp_data.dpc;

	dpcFlag = gc5035_otp_read_byte(GC5035_OTP_DPC_FLAG_OFFSET);
	LOG_INF("dpc flag = 0x%x\n", dpcFlag);
	switch (GC5035_OTP_GET_2BIT_FLAG(dpcFlag, 0)) {
	case GC5035_OTP_FLAG_EMPTY:
		LOG_ERR("dpc info is empty!!\n");
		pDPC->flag = GC5035_OTP_FLAG_EMPTY;
		break;
	case GC5035_OTP_FLAG_VALID:
		LOG_INF("dpc info is valid!\n");
		pDPC->total_num = gc5035_otp_read_byte(GC5035_OTP_DPC_TOTAL_NUM_OFFSET)
			+ gc5035_otp_read_byte(GC5035_OTP_DPC_ERROR_NUM_OFFSET);
		pDPC->flag = GC5035_OTP_FLAG_VALID;
		LOG_INF("total_num = %d\n", pDPC->total_num);
		break;
	default:
		pDPC->flag = GC5035_OTP_FLAG_INVALID;
		break;
	}
}

static void gc5035_otp_update_dd(void)
{
	kal_uint8 state = 0;
	kal_uint8 n = 0;
	struct gc5035_dpc_t *pDPC = &gc5035_otp_data.dpc;

	if (GC5035_OTP_FLAG_VALID == pDPC->flag) {
		LOG_INF("DD auto load start!\n");
		write_cmos_sensor(0xfe, 0x02);
		write_cmos_sensor(0xbe, 0x00);
		write_cmos_sensor(0xa9, 0x01);
		write_cmos_sensor(0x09, 0x33);
		write_cmos_sensor(0x01, (pDPC->total_num >> 8) & 0x07);
		write_cmos_sensor(0x02, pDPC->total_num & 0xff);
		write_cmos_sensor(0x03, 0x00);
		write_cmos_sensor(0x04, 0x80);
		write_cmos_sensor(0x95, 0x0a);
		write_cmos_sensor(0x96, 0x30);
		write_cmos_sensor(0x97, 0x0a);
		write_cmos_sensor(0x98, 0x32);
		write_cmos_sensor(0x99, 0x07);
		write_cmos_sensor(0x9a, 0xa9);
		write_cmos_sensor(0xf3, 0x80);
		while (n < 3) {
			state = read_cmos_sensor(0x06);
			if ((state | 0xfe) == 0xff)
				mdelay(10);
			else
				n = 3;
			n++;
		}
		write_cmos_sensor(0xbe, 0x01);
		write_cmos_sensor(0x09, 0x00);
		write_cmos_sensor(0xfe, 0x01);
		write_cmos_sensor(0x80, 0x02);
		write_cmos_sensor(0xfe, 0x00);
	}
	return;
}

static void gc5035_insensor_otp_identify(void)
{
	memset(&gc5035_otp_data, 0, sizeof(gc5035_otp_data));

	write_cmos_sensor(0xfc, 0x01);
	write_cmos_sensor(0xf4, 0x40);
	write_cmos_sensor(0xf5, 0xe9);
	write_cmos_sensor(0xf6, 0x14);
	write_cmos_sensor(0xf8, 0x49);
	write_cmos_sensor(0xf9, 0x82);
	write_cmos_sensor(0xfa, 0x00);
	write_cmos_sensor(0xfc, 0x81);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x36, 0x01);
	write_cmos_sensor(0xd3, 0x87);
	write_cmos_sensor(0x36, 0x00);
	write_cmos_sensor(0x33, 0x00);
	write_cmos_sensor(0xf7, 0x01);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xee, 0x30);
	write_cmos_sensor(0xfa, 0x10);
	write_cmos_sensor(0xf5, 0xe9);
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x67, 0xc0);
	write_cmos_sensor(0x59, 0x3f);
	write_cmos_sensor(0x55, 0x80);
	write_cmos_sensor(0x65, 0x80);
	write_cmos_sensor(0x66, 0x03);
	write_cmos_sensor(0xfe, 0x00);

	gc5035_otp_read_group(GC5035_OTP_ID_DATA_OFFSET,
		&gc5035_otp_data.otp_id[0],GC5035_OTP_ID_SIZE);
	gc5035_gcore_read_dpc();

	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x67, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfa, 0x00);
	return;
}

static void gc5035_insensor_otp_function(void)
{
	kal_uint8 i;
	kal_uint8 flag = 0;
	kal_uint8 otp_id[GC5035_OTP_ID_SIZE];

	memset(&otp_id, 0, GC5035_OTP_ID_SIZE);

	write_cmos_sensor(0xfa, 0x10);
	write_cmos_sensor(0xf5, 0xe9);
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x67, 0xc0);
	write_cmos_sensor(0x59, 0x3f);
	write_cmos_sensor(0x55, 0x80);
	write_cmos_sensor(0x65, 0x80);
	write_cmos_sensor(0x66, 0x03);
	write_cmos_sensor(0xfe, 0x00);

	gc5035_otp_read_group(GC5035_OTP_ID_DATA_OFFSET, &otp_id[0],
		GC5035_OTP_ID_SIZE);

	for (i = 0; i < GC5035_OTP_ID_SIZE; i++) {
		if (otp_id[i] != gc5035_otp_data.otp_id[i]) {
			flag = 1;
			break;
		}
	}

	if (flag == 1) {
		LOG_ERR("otp id mismatch, read again");
		memset(&gc5035_otp_data, 0, sizeof(gc5035_otp_data));
		for (i = 0; i < GC5035_OTP_ID_SIZE; i++) {
			gc5035_otp_data.otp_id[i] = otp_id[i];
		}
		gc5035_gcore_read_dpc();
	}

	gc5035_otp_update_dd();

	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x67, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfa, 0x00);
}

static kal_uint32 open(void)
{
	int rc;
	UINT32 sensor_id = 0;

	LOG_INF("E\n");

	rc = get_imgsensor_id(&sensor_id);
	if (rc != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;
	LOG_INF("sensor probe successfully. sensor_id=0x%x\n", sensor_id);
	rc = sensor_init();
	if (rc != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;
	gc5035_insensor_otp_function();
	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = KAL_FALSE;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 close(void)
{
	LOG_INF("E");
	/* No Need to implement this function */
	return ERROR_NONE;
}

static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	rc = set_preview_setting();
	if (rc != ERROR_NONE) {
		LOG_ERR("preview setting failed.\n");
		return rc;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	rc = set_capture_setting();
	if (rc != ERROR_NONE) {
		LOG_ERR("capture setting failed.\n");
		return rc;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.current_fps = imgsensor_info.normal_video.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	rc = set_normal_video_setting();
	if (rc != ERROR_NONE) {
		LOG_ERR("%s video setting failed\n", __func__);
		return rc;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT
				*sensor_resolution)
{
	if (sensor_resolution) {
		sensor_resolution->SensorFullWidth =
			imgsensor_info.cap.grabwindow_width;
		sensor_resolution->SensorFullHeight =
			imgsensor_info.cap.grabwindow_height;

		sensor_resolution->SensorPreviewWidth =
			imgsensor_info.pre.grabwindow_width;
		sensor_resolution->SensorPreviewHeight =
			imgsensor_info.pre.grabwindow_height;

		sensor_resolution->SensorVideoWidth =
			imgsensor_info.normal_video.grabwindow_width;
		sensor_resolution->SensorVideoHeight =
			imgsensor_info.normal_video.grabwindow_height;
	}
	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_INFO_STRUCT *sensor_info,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	if (!sensor_info || !sensor_config_data) {
		LOG_ERR("Fatal: NULL ptr. sensor_info:%pK, sensor_config_data:%pK\n",
			sensor_info, sensor_config_data);
		return ERROR_NONE;
	}

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity =
		SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity =
		SENSOR_CLOCK_POLARITY_LOW;  // inverse with datasheet
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType =
		imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType =
		imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode =
		imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame =
		imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame =
		imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame =
		imgsensor_info.video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent =
		imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame =
		imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support =
		imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine =
		imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum =
		imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
	sensor_info->SensorHightSampling = 0;  // 0 is default 1x
	sensor_info->SensorPacketECCOrder = 1; /* not use */
	sensor_info->PDAF_Support = 0;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX =
			imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		sensor_info->SensorGrabStartX =
			imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.normal_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
		break;
	default:
		sensor_info->SensorGrabStartX =
			imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;

	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		rc = preview(image_window, sensor_config_data);
		if (rc != ERROR_NONE)
			LOG_ERR("preview failed\n");
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		rc = capture(image_window, sensor_config_data);
		if (rc != ERROR_NONE)
			LOG_ERR("capture failed\n");
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		rc = normal_video(image_window, sensor_config_data);
		if (rc != ERROR_NONE)
			LOG_ERR("video failed\n");
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		(void)preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return rc;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	if (framerate == 0)
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	/* fps set to 298 when frame is 300 and auto-flicker enaled */
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 298;
	/* fps set to 146 when frame is 150 and auto-flicker enaled */
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);
	set_dummy();
	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d ", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MUINT32 framerate)
{
	kal_uint32 frame_length;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		if ((framerate == 0) || (imgsensor_info.pre.linelength == 0))
			return ERROR_NONE;
		frame_length = imgsensor_info.pre.pclk / framerate * 10 /
				imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if ((framerate == 0) ||
			(imgsensor_info.normal_video.linelength == 0))
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk /
				framerate * 10 /
				imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
				imgsensor_info.normal_video.framelength) ?
			(frame_length -
				imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if ((framerate == 0) || (imgsensor_info.cap.linelength == 0))
			return ERROR_NONE;
		frame_length = imgsensor_info.cap.pclk / framerate * 10 /
				imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap.framelength) ?
			(frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength +
					imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:  /* coding with  preview scenario by default */
		if ((framerate == 0) || (imgsensor_info.pre.linelength == 0))
			return ERROR_NONE;
		frame_length = imgsensor_info.pre.pclk / framerate * 10 /
				imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
					imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MUINT32 *framerate)
{
	if (framerate) {
		switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*framerate = imgsensor_info.pre.max_framerate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*framerate = imgsensor_info.normal_video.max_framerate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*framerate = imgsensor_info.cap.max_framerate;
			break;
		default:
			break;
		}
	}
	return ERROR_NONE;
}

static void set_register(kal_uint32 reg_addr, kal_uint32 reg_data)
{
	kal_int32 rc = ERROR_NONE;

	rc = imgsensor_sensor_i2c_write(&imgsensor, (kal_uint16)reg_addr,
					(kal_uint16)reg_data, IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		LOG_ERR("failed. reg_addr:0x%x, reg_data: 0x%x.\n",
			reg_addr, reg_data);
	}
}

static void get_register(kal_uint32 reg_addr, kal_uint32 *reg_data)
{
	kal_int32 rc = 0;
	kal_uint16 temp_data = 0;

	rc = imgsensor_sensor_i2c_read(&imgsensor, reg_addr,
				       &temp_data, IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		LOG_ERR("failed. reg_addr:0x%x, reg_data: 0x%x.\n",
			reg_addr, temp_data);
	}
	*reg_data = temp_data;
	LOG_INF("reg_addr:0x%x, reg_data: 0x%x.\n",
		reg_addr, *reg_data);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	kal_int32 rc;

	LOG_INF("Enter.enable:%d\n", enable);
	if (enable)
		rc = imgsensor_sensor_write_setting(&imgsensor,
				&imgsensor_info.streamon_setting);
	else
		rc = imgsensor_sensor_write_setting(&imgsensor,
				&imgsensor_info.streamoff_setting);
	if (rc < 0) {
		LOG_ERR("Failed enable:%d\n", enable);
		return ERROR_SENSOR_POWER_ON_FAIL;
	}
	return ERROR_NONE;
}

static kal_uint32 feature_control_gc5035_luxvisions(MSDK_SENSOR_FEATURE_ENUM feature_id,
		UINT8 *feature_para, UINT32 *feature_para_len)
{
	kal_uint32 rc = ERROR_NONE;
	UINT16 *feature_return_para_16 = (UINT16 *)feature_para;
	UINT16 *feature_data_16 = (UINT16 *)feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *)feature_para;
	UINT32 *feature_data_32 = (UINT32 *)feature_para;
	INT32 *feature_return_para_i32 = (INT32 *)feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo = NULL;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *)feature_para;

	if (!feature_para || !feature_para_len) {
		LOG_ERR("Fatal null ptr. feature_para:%pK,feature_para_len:%pK\n",
			feature_para, feature_para_len);
		return ERROR_NONE;
	}

	LOG_INF("feature_id = %d.\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4; /* return 4 byte data */
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		(void)set_gain((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		set_register(sensor_reg_data->RegAddr,
			     sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		get_register(sensor_reg_data->RegAddr, &sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		(void)set_video_mode((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		rc = get_imgsensor_id(feature_return_para_32);
		if (rc == ERROR_NONE)
			gc5035_insensor_otp_identify();
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		(void)set_auto_flicker_mode((BOOL)*feature_data_16,
				*(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((
			enum MSDK_SCENARIO_ID_ENUM) *feature_data,
			(MUINT32) *(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		(void)get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		(void)set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		// for factory mode auto testing
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16) *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			/* imgsensor_winsize_info arry 1 is capture setting */
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			/* imgsensor_winsize_info arry 2 is video setting */
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			/* imgsensor_winsize_info arry 0 is preview setting */
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = 0;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		rc = streaming_control(KAL_FALSE);
		if (rc != ERROR_NONE)
			LOG_ERR("stream off failed\n");
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter((UINT16)*feature_data);
		rc = streaming_control(KAL_TRUE);
		if (rc != ERROR_NONE)
			LOG_ERR("stream on failed\n");
		break;
	case SENSOR_HUAWEI_FEATURE_DUMP_REG:
		(void)sensor_dump_reg();
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.cap.pclk /
				 (imgsensor_info.cap.linelength - 80)) *
				imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.normal_video.pclk /
				 (imgsensor_info.normal_video.linelength - 80)) *
				imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.pre.pclk /
				 (imgsensor_info.pre.linelength - 80)) *
				imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;
	default:
		LOG_ERR("Not support the feature_id:%d\n", feature_id);
		break;
	}

	return rc;
} /* feature_control_gc5035_luxvisions() */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control_gc5035_luxvisions,
	control,
	close
};

UINT32 GC5035_LUXVISIONS_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	if (pfFunc)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
} /*  GC5035_LUXVISIONS_SensorInit  */