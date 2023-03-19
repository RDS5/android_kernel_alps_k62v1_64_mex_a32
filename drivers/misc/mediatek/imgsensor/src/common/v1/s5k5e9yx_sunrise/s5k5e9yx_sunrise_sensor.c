/*
 * s5k5e9yx_sunrise_sensor.c
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * s5k5e9yx_sunrise image sensor driver
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

#include "s5k5e9yx_sunrise_sensor.h"

#define RETRY_TIMES 2

/* Modify Following Strings for Debug */
#define PFX "[s5k5e9yx_sunrise]"
#define log_dbg(fmt, args...) \
		pr_debug(PFX "%s %d " fmt, __func__, __LINE__, ##args)
#define log_inf(fmt, args...) \
		pr_info(PFX "%s %d " fmt, __func__, __LINE__, ##args)
#define log_err(fmt, args...) \
		pr_err(PFX "%s %d " fmt, __func__, __LINE__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}
#ifdef OTP_ENABLE
static struct otp_struct current_5e9_otp;

static void start_read_otp(u8 page)
{
	int n = 0;
	uint temp;

	write_cmos_sensor(0x0100, 0x01);
	mdelay(10);

	write_cmos_sensor(0x0a02, page);
	write_cmos_sensor(0x0a00, 0x01); // read command

	do {
		mdelay(1);
		temp = read_cmos_sensor(0x0a01);
		n++;
	} while (((temp & 0xff) != 0x01) && (n < 12));

	if ((temp & 0xff) != 0x01)
		pr_err("OTP invalid !0x0A01 = 0x%x\n", temp);

}

static void stop_read_otp(void)
{
	write_cmos_sensor(0x0a00, 0x04); // clear error bits
	write_cmos_sensor(0x0a00, 0x00); // initial command
	write_cmos_sensor(0x0100, 0x00);
}

static void read_otp_data(struct otp_struct *otp)
{
	u8 awb_group_id = 0;
	UINT16 basic_address = OTP_MODULE_BASIC_ADDRESS;
	UINT16 temp_address = OTP_MODULE_BASIC_ADDRESS;

	start_read_otp(17);// page 17
	// --------AWB Info--------
	basic_address = OTP_AWB_BASIC_ADDRESS;
	temp_address = OTP_AWB_BASIC_ADDRESS;

	if ((read_cmos_sensor(OTP_AWB_FLAG_ADDRESS) & 0x03) == 0x01) {
		awb_group_id = 1;
		log_inf(" awb group 1.\n");
	} else if ((read_cmos_sensor(OTP_AWB_FLAG_ADDRESS) & 0x0C) == 0x04) {
		awb_group_id = 2;
		log_inf("awb group 2.\n");
	}

	if (awb_group_id != 0) {
		temp_address = basic_address + (awb_group_id - 1) * 13;
		otp->unit_rg_ratio = (read_cmos_sensor(temp_address) << 8) |
			(read_cmos_sensor(temp_address + 1) & 0x03FF);
		otp->unit_bg_ratio = (read_cmos_sensor(temp_address + 2) << 8) |
			(read_cmos_sensor(temp_address + 3) & 0x03FF);
		otp->unit_grgb_ratio = (read_cmos_sensor(temp_address + 4) << 8) |
			(read_cmos_sensor(temp_address + 5) & 0x03FF);
		otp->typical_rg_ratio = (read_cmos_sensor(temp_address + 6) << 8) |
			(read_cmos_sensor(temp_address + 7) & 0x03FF);
		otp->typical_bg_ratio = (read_cmos_sensor(temp_address + 8) << 8) |
			(read_cmos_sensor(temp_address + 9) & 0x03FF);
		otp->typical_grgb_ratio = (read_cmos_sensor(temp_address + 10) << 8) |
			(read_cmos_sensor(temp_address + 11) & 0x03FF);

		otp->awb_valid = 1;
	} else {
		otp->awb_valid = 0;
		pr_err("awb group invalid/empty.\n");
	}

	stop_read_otp();
}

static void update_otp_wb(struct otp_struct *otp)
{
	kal_uint16 r_gain, b_gain, g_gain, base_gain;

	if (!otp->awb_valid) {
		log_inf("opt awb is invalid\n");
		return;
	}
	r_gain = (BASIC_GAIN * otp->typical_rg_ratio) / otp->unit_rg_ratio;
	b_gain = (BASIC_GAIN * otp->typical_bg_ratio) / otp->unit_bg_ratio;
	g_gain = BASIC_GAIN;
	base_gain = BASIC_GAIN;

	if (r_gain < b_gain) {
		if (r_gain < base_gain) {
			b_gain = BASIC_GAIN * b_gain / r_gain;
			g_gain = BASIC_GAIN * g_gain / r_gain;
			r_gain = BASIC_GAIN;
		}
	} else {
		if (b_gain < base_gain) {
			r_gain = BASIC_GAIN * r_gain / b_gain;
			g_gain = BASIC_GAIN * g_gain / b_gain;
			b_gain = BASIC_GAIN;
		}
	}

	log_inf("g_gain = 0x%x, r_gain = 0x%x, b_gain = 0x%x.\n", g_gain, r_gain, b_gain);

	// digital gain Gr
	write_cmos_sensor(0x020e, g_gain >> 8);
	write_cmos_sensor(0x020f, g_gain & 0xff);
	// digital gain R
	write_cmos_sensor(0x0210, r_gain >> 8);
	write_cmos_sensor(0x0211, r_gain & 0xff);
	// digital gain B
	write_cmos_sensor(0x0212, b_gain >> 8);
	write_cmos_sensor(0x0213, b_gain&0xff);
	// digital gain Gb
	write_cmos_sensor(0x0214, g_gain>>8);
	write_cmos_sensor(0x0215, g_gain & 0xff);
}

static void read_register_from_otp(void)
{
	memset_s(&current_5e9_otp, sizeof(struct otp_struct),
		0, sizeof(struct otp_struct));

	read_otp_data(&current_5e9_otp);
}

#endif

static void set_dummy(void)
{
	kal_int32 rc;

	log_dbg("ENTER\n");

	rc = imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_FRM_LENGTH_LINE_REG_H,
			imgsensor.frame_length & 0xFFFF,
			IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		log_err("write frame_length failed, frame_length: 0x%x\n",
			imgsensor.frame_length);
		return;
	}
	rc = imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_LINE_LENGTH_PCK_REG_H,
			imgsensor.line_length & 0xFFFF,
			IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		log_err("write line_length failed, line_length: 0x%x\n",
			imgsensor.frame_length);
		return;
	}
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	log_dbg("ENTER\n");

	if (!framerate || !imgsensor.line_length) {
		log_err("Invalid params. framerate = %d, line_length = %d\n",
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

static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps;

	log_dbg("ENTER\n");

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/* 1. calc the framelength depend on shutter */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	/* 2. calc the to be in the min,max threshold */
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter = (shutter >
		(imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) :
		shutter;

	/* 3. calc and update framelength to abandon the flicker issue */
	if (imgsensor.autoflicker_en) {
		if (!imgsensor.line_length || !imgsensor.frame_length) {
			log_err("Invalid parms. line_length: %d, frame_length: %d\n",
				imgsensor.line_length, imgsensor.frame_length);
			return;
		}
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
				imgsensor.frame_length;
		if (realtime_fps >= 298 && realtime_fps <= 305)
			/* 1) calc fps between 297~305, real fps set to 296 */
			set_max_framerate(298, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			/* 2) calc fps between 147~150, real fps set to 146 */
			set_max_framerate(146, 0);
		else
			/* 3) otherwise, update frame length directly */
			(void)imgsensor_sensor_i2c_write(&imgsensor,
				SENSOR_FRM_LENGTH_LINE_REG_H,
				imgsensor.frame_length & 0xFFFF,
				IMGSENSOR_I2C_WORD_DATA);
	} else {
		/* 3) otherwise, update frame length directly */
		(void)imgsensor_sensor_i2c_write(&imgsensor,
				SENSOR_FRM_LENGTH_LINE_REG_H,
				imgsensor.frame_length & 0xFFFF,
				IMGSENSOR_I2C_WORD_DATA);
	}

	/* 4. update shutter */
	(void)imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_COARSE_INT_TIME_REG_H,
			shutter & 0xFFFF,
			IMGSENSOR_I2C_WORD_DATA);
	log_dbg("Exit! shutter =%d, framelength = %d\n", shutter,
		imgsensor.frame_length);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain;

	reg_gain = gain / 2;

	return (kal_uint16)reg_gain;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	log_dbg("ENTER\n");

	log_dbg("gain %d\n", gain);
	if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
		log_err("Error gain setting\n");
		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 16 * BASEGAIN)
			gain = 16 * BASEGAIN;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	log_dbg("gain = %d , reg_gain = 0x%x\n", gain, reg_gain);

	(void)imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_ANA_GAIN_REG_H,
			(reg_gain & 0xFFFF),
			IMGSENSOR_I2C_WORD_DATA);
	log_dbg("EXIT\n");

	return gain;
}

static kal_uint32 sensor_init(void)
{
	kal_int32 rc;

	log_dbg("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
				&imgsensor_info.init_setting);
	if (rc < 0) {
		log_err("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	log_dbg("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_preview_setting(void)
{
	kal_int32 rc;

	log_dbg("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
				&imgsensor_info.pre_setting);
	if (rc < 0) {
		log_err("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	log_dbg("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_capture_setting(kal_uint16 current_fps)
{
	kal_int32 rc;

	log_dbg("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
				&imgsensor_info.cap_setting);
	if (rc < 0) {
		log_err("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	log_dbg("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_normal_video_setting(kal_uint16 current_fps)
{
	kal_int32 rc;

	log_dbg("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
				&imgsensor_info.normal_video_setting);
	if (rc < 0) {
		log_err("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	log_dbg("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_custom2_setting(kal_uint16 current_fps)
{
	kal_int32 rc;

	log_dbg("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
				&imgsensor_info.custom2_setting);
	if (rc < 0) {
		log_err("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	log_dbg("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 return_sensor_id(void)
{
	kal_int32 rc;
	kal_uint16 sensor_id = 0;
	int mode_id1 = 0, mode_id2 = 0, reg = 0, i;

	rc = imgsensor_sensor_i2c_read(&imgsensor,
			imgsensor_info.sensor_id_reg,
			&sensor_id, IMGSENSOR_I2C_WORD_DATA);
	log_inf("sensor_id = 0x%x\n", sensor_id);
	if (rc < 0) {
		log_err("Read id failed.id reg: 0x%x\n",
			imgsensor_info.sensor_id_reg);
		sensor_id = 0xFFFF;
	}
	if (sensor_id == S5K5E9_SENSOR_ID) {
		sensor_id = S5K5E9YX_SUNRISE_SENSOR_ID;

		write_cmos_sensor(0x0100, 0x01);
		mdelay(10);
		write_cmos_sensor(0x0A02, 17);
		write_cmos_sensor(0x0A00, 0x01);
		for (i = 0; i <= 10; i++) {
			reg = read_cmos_sensor(0x0A01);
			if ((reg & 0xff) == 0x01)
				break;
		}
		mode_id1 = read_cmos_sensor(INFO_START_ADDR1);
		mode_id2 = read_cmos_sensor(INFO_START_ADDR2);
		write_cmos_sensor(0x0A00, 0x04);
		write_cmos_sensor(0x0A00, 0x00);
		log_inf("mod1 = %d, mod2 = %d\n", mode_id1, mode_id2);
		if (mode_id1 == SANGLAISHI_MOD_ID ||
			mode_id2 == SANGLAISHI_MOD_ID) {
			sensor_id = S5K5E9YX_SUNRISE_SENSOR_ID;
			log_inf("SANGLAISHI sensor_id = %d\n", sensor_id);
		} else {
			sensor_id = 0;
		}
	}

	return sensor_id;
}

static kal_uint32 sensor_dump_reg(void)
{
	kal_int32 rc;

	log_inf("ENTER\n");
	rc = imgsensor_sensor_i2c_process(&imgsensor,
		&imgsensor_info.dump_info);
	if (rc < 0)
		log_err("Failed\n");
	log_inf("EXIT\n");
	return ERROR_NONE;
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = RETRY_TIMES; /* retry 2 time */

	spin_lock(&imgsensor_drv_lock);
	/* init i2c config */
	imgsensor.i2c_speed = imgsensor_info.i2c_speed;
	imgsensor.addr_type = imgsensor_info.addr_type;
	spin_unlock(&imgsensor_drv_lock);
	while (imgsensor_info.i2c_addr_table[i] != 0xFF) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				log_inf("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			log_inf("Read sensor id fail, id: 0x%x\n", *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = RETRY_TIMES;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		// if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static kal_uint32 open(void)
{
	kal_uint32 sensor_id = 0;
	kal_uint32 rc;

	log_inf("ENTER\n");

	rc = get_imgsensor_id(&sensor_id);
	if (rc != ERROR_NONE) {
		log_err("probe sensor failed\n");
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	log_dbg("sensor probe successfully. sensor_id = 0x%x\n", sensor_id);

	/* initail sequence write in */
	rc = sensor_init();
	if (rc != ERROR_NONE) {
		log_err("init failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	// otp function
#ifdef OTP_ENABLE
	if (otp_read_flag == FAIL) {
		read_register_from_otp();
		otp_read_flag = TURE;
	}
	update_otp_wb(&current_5e9_otp);// apply awb otp
#endif
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
	log_inf("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 close(void)
{
	log_inf("Enter\n");
	/* No Need to implement this function */
	return ERROR_NONE;
}


static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	log_inf("ENTER\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	rc = set_preview_setting();
	if (rc != ERROR_NONE) {
		log_err("Failed\n");
		return rc;
	}
	log_inf("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	log_inf("ENTER\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	rc = set_capture_setting(imgsensor.current_fps);
	if (rc != ERROR_NONE) {
		log_err("Failed\n");
		return rc;
	}
	log_inf("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	log_inf("ENTER\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	rc = set_normal_video_setting(imgsensor.current_fps);
	if (rc != ERROR_NONE) {
		log_err("Failed\n");
		return rc;
	}
	log_inf("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	log_inf("ENTER\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	rc = set_custom2_setting(imgsensor.current_fps);
	if (rc != ERROR_NONE) {
		log_err("Failed\n");
		return rc;
	}
	log_inf("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT
				*sensor_resolution)
{
	if (sensor_resolution != NULL) {
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

		sensor_resolution->SensorCustom2Width =
			imgsensor_info.custom2.grabwindow_width;
		sensor_resolution->SensorCustom2Height =
			imgsensor_info.custom2.grabwindow_height;
	}
	return ERROR_NONE;
} /*  get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_INFO_STRUCT *sensor_info,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	if (!sensor_info || !sensor_config_data) {
		log_err("Fatal: NULL ptr. sensor_info: %pK, sensor_config_data: %pK\n",
			sensor_info, sensor_config_data);
		return ERROR_NONE;
	}

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity =
		SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity =
		SENSOR_CLOCK_POLARITY_LOW;  /* inverse with datasheet */
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
	sensor_info->HighSpeedVideoDelayFrame =
			imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
			imgsensor_info.slim_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
			imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom2DelayFrame =
			imgsensor_info.custom2_delay_frame;

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
	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;  /* 0 is default 1x */
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
	case MSDK_SCENARIO_ID_CUSTOM2:
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
	kal_uint32 rc;

	log_dbg("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		rc = preview(image_window, sensor_config_data);
		if (rc != ERROR_NONE)
			log_err("preview failed\n");
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		rc = capture(image_window, sensor_config_data);
		if (rc != ERROR_NONE)
			log_err("capture failed\n");
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		rc = normal_video(image_window, sensor_config_data);
		if (rc != ERROR_NONE)
			log_err("video failed\n");
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		rc = custom2(image_window, sensor_config_data);
		if (rc != ERROR_NONE)
			log_err("custom2 failed\n");
		break;
	default:
		log_err("Error ScenarioId setting. scenario_id:%d\n",
			scenario_id);
		rc = preview(image_window, sensor_config_data);
		if (rc != ERROR_NONE)
			log_err("preview failed\n");
		return ERROR_INVALID_SCENARIO_ID;
	}
	return rc;
} /* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
	log_inf("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
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

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	log_inf("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) /* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else /* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM
		scenario_id, UINT32 framerate)
{
	kal_uint32 frame_length;

	log_dbg("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		if (framerate == 0 || imgsensor_info.pre.linelength == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.pre.pclk / framerate * 10 /
				imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0 ||
			imgsensor_info.normal_video.linelength == 0)
			return ERROR_NONE;
		frame_length =
			imgsensor_info.normal_video.pclk / framerate * 10 /
			imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.normal_video.framelength) ?
			(frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (framerate == 0 || imgsensor_info.cap.linelength == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.cap.pclk / framerate * 10 /
				imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap.framelength) ?
			(frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:  /* coding with  preview scenario by default */
		if (framerate == 0 || imgsensor_info.pre.linelength == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.pre.pclk / framerate * 10 /
				imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		log_err("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM
		scenario_id, UINT32 *framerate)
{
	if (!framerate) {
		log_err("Fatal err. NULL ptr\n");
		return ERROR_INVALID_PARA;
	}
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static void check_streamoff(void)
{
	kal_int32 rc;

	kal_uint32 timeout;

	if (imgsensor.current_fps == 0) {
		log_err("fps is zero\n");
		return;
	}
	/* Max timeout is one frame duration. 10000 = 1s */
	timeout = (10000 / imgsensor.current_fps) + 1;
	rc = imgsensor_sensor_i2c_poll(&imgsensor, SENSOR_FRM_CNT_REG,
				0xFF, IMGSENSOR_I2C_BYTE_DATA, timeout);
	if (rc < 0)
		log_err("Poll the i2c status failed\n");
}

static kal_uint32 streaming_control(kal_bool enable)
{
	kal_int32 rc;

	log_inf("Enter.enable:%d\n", enable);
	if (enable) {
		rc = imgsensor_sensor_write_setting(&imgsensor,
					&imgsensor_info.streamon_setting);
	} else {
		rc = imgsensor_sensor_write_setting(&imgsensor,
					&imgsensor_info.streamoff_setting);
		check_streamoff();
	}
	if (rc < 0) {
		log_err("Failed enable:%d\n", enable);
		return ERROR_SENSOR_POWER_ON_FAIL;
	}
	log_inf("Exit.enable:%d\n", enable);

	return ERROR_NONE;
}

static kal_uint32 feature_control_s5k5e9yx_sunrise(MSDK_SENSOR_FEATURE_ENUM
		feature_id,
		UINT8 *feature_para, UINT32 *feature_para_len)
{
	kal_uint32 rc = ERROR_NONE;
	UINT16 *feature_return_para_16 = (UINT16 *)feature_para;
	UINT16 *feature_data_16 = (UINT16 *)feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *)feature_para;
	UINT32 *feature_data_32 = (UINT32 *)feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo = NULL;

	if (!feature_para || !feature_para_len) {
		log_err("Fatal null ptr. feature_para: %pK,feature_para_len: %pK\n",
			feature_para, feature_para_len);
		return ERROR_NONE;
	}

	log_dbg("feature_id = %d\n", feature_id);
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
		set_gain((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16,
				*(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *feature_data,
			(UINT32) *(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
			(UINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		log_dbg("current fps: %d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16) *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t)(*(feature_data + 1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			/* imgsensor_winsize_info arry 1 is capture setting */
			rc = memcpy_s((void *)wininfo,
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT),
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			if (rc != EOK)
				log_err("memcpy_s fail!\n");
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			/* imgsensor_winsize_info arry 2 is video setting */
			rc = memcpy_s((void *)wininfo,
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT),
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			if (rc != EOK)
				log_err("memcpy_s fail!\n");
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			/* imgsensor_winsize_info arry 0 is preview setting */
			rc = memcpy_s((void *)wininfo,
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT),
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			if (rc != EOK)
				log_err("memcpy_s fail!\n");
			break;
		}
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		rc = streaming_control(KAL_FALSE);
		if (rc != ERROR_NONE)
			log_err("stream off failed\n");
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter((UINT16)*feature_data);
		rc = streaming_control(KAL_TRUE);
		if (rc != ERROR_NONE)
			log_err("stream on failed\n");
		break;
	case SENSOR_HUAWEI_FEATURE_DUMP_REG:
		sensor_dump_reg();
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			if (imgsensor_info.cap.linelength >
				IMGSENSOR_LINGLENGTH_GAP)
				*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.cap.pclk /
					(imgsensor_info.cap.linelength -
					IMGSENSOR_LINGLENGTH_GAP)) *
					imgsensor_info.cap.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if (imgsensor_info.normal_video.linelength >
				IMGSENSOR_LINGLENGTH_GAP)
				*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.normal_video.pclk /
					(imgsensor_info.normal_video.linelength -
					IMGSENSOR_LINGLENGTH_GAP)) *
					imgsensor_info.normal_video.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			if (imgsensor_info.pre.linelength >
				IMGSENSOR_LINGLENGTH_GAP)
				*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.pre.pclk /
					(imgsensor_info.pre.linelength -
					IMGSENSOR_LINGLENGTH_GAP)) *
					imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;
	default:
		log_inf("Not support the feature_id: %d\n", feature_id);
		break;
	}

	return rc;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control_s5k5e9yx_sunrise,
	control,
	close
};

UINT32 S5K5E9YX_SUNRISE_SensorInit(struct SENSOR_FUNCTION_STRUCT **pf_func)
{
	if (pf_func != NULL)
		*pf_func = &sensor_func;
	return ERROR_NONE;
}
