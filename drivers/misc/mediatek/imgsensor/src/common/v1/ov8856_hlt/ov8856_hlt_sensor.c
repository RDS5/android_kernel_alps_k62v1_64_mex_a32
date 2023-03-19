/*
 * ov8856_hlt_sensor.c
 *
 * Copyright (c) 2019-2019 Huawei Technologies Co., Ltd.
 *
 * ov8856_hlt image sensor driver
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

#include "ov8856_hlt_sensor.h"

#define RETRY_TIMES 2
#define PIX_10_BITS 10
#define OTP_CHECK_SUCCESSED 1
/* Modify Following Strings for Debug */
#define PFX "[ov8856_hlt]"
#define log_inf(fmt, args...) \
	pr_info(PFX "%s %d " fmt, __func__, __LINE__, ##args)
#define log_err(fmt, args...) \
	pr_err(PFX "%s %d " fmt, __func__, __LINE__, ##args)
/* Modify end */

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF)};

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}
static struct otp_struct otp_info;

#ifdef OV8856OTP
static void enable_read_otp(void)
{
	int temp1;

	write_cmos_sensor(0x0100, 0x01);
	temp1 = read_cmos_sensor(0x5001);
	write_cmos_sensor(0x5001, (0x00 & 0x08) | (temp1 & (~0x08)));
	// read OTP into buffer
	write_cmos_sensor(0x3d84, 0xC0);
	write_cmos_sensor(0x3d88, 0x70); // OTP start address
	write_cmos_sensor(0x3d89, 0x10);
	write_cmos_sensor(0x3d8A, 0x72); // OTP end address
	write_cmos_sensor(0x3d8B, 0x0a);
	write_cmos_sensor(0x3d81, 0x01); // load otp into buffer
	mdelay(10);
}

static void disable_read_otp(void)
{
	int temp1;

	temp1 = read_cmos_sensor(0x5001);
	write_cmos_sensor(0x5001, (0x08 & 0x08) | (temp1 & (~0x08)));
	write_cmos_sensor(0x0100, 0x00);
}

static int read_otp(void)
{
	int checksum = 0, awb_checksum = 0, lsc_checksum = 0;
	int otp_flag = 0, addr = 0, i = 0;

	enable_read_otp();
	memset_s(&otp_info, sizeof(otp_info), 0, sizeof(otp_info));
	// OTP base information and WB calibration data
	otp_flag = read_cmos_sensor(0x7010);
	log_inf("awb_otp_flag = 0x%x,\n", otp_flag);
	if ((otp_flag & 0xc0) == 0x40) {//group1 used
		addr = 0x7011; // base address of info group 1
	} else if ((otp_flag & 0x30) == 0x10) {//group2 used
		addr = 0x7018; // base address of info group 2
	} else {
		log_inf("OTP group1 group2 all error\n");
		return 1;
	}
	if (addr != 0) {
		otp_info.flag = 0xC0; // valid info and AWB in OTP
		otp_info.module_integrator_id = read_cmos_sensor(addr);
		log_inf("module_id : 0x%x\n", otp_info.module_integrator_id);

	for (i = 0; i < 7; i++) {
		otp_info.awb[i] = read_cmos_sensor(addr + i);
		awb_checksum += otp_info.awb[i];
	}

		otp_info.production_year_month = read_cmos_sensor(addr + 1);
		otp_info.production_day = read_cmos_sensor(addr + 2);
		otp_info.rg_ratio = read_cmos_sensor(addr + 3);
		otp_info.bg_ratio = read_cmos_sensor(addr + 4);
		otp_info.rg_ratio_golden = read_cmos_sensor(addr + 5);
		otp_info.bg_ratio_golden = read_cmos_sensor(addr + 6);
	} else {
		otp_info.flag = 0x00; // not info and AWB in OTP
		otp_info.module_integrator_id = 0;
		otp_info.production_year_month = 0;
		otp_info.production_day = 0;
		otp_info.rg_ratio = 0;
		otp_info.bg_ratio = 0;
		otp_info.rg_ratio_golden = 0;
		otp_info.bg_ratio_golden = 0;
	}
	// OTP LSC Calibration
	otp_flag = read_cmos_sensor(0x7028);
	log_inf("lsc_otp_flag = %d,\n", otp_flag);
	if ((otp_flag & 0xc0) == 0x40) {
		addr = 0x7029; // base address of Lsc Calibration group 1
	} else if ((otp_flag & 0x30) == 0x10) {
		addr = 0x711a; // base address of Lsc Calibration group 2
	} else {
		log_inf("lsc  group1 group2 all error\n");
		return 1;
	}
	if (addr != 0) {
		for (i = 0; i < 240; i++) {
			otp_info.lsc[i] = read_cmos_sensor(addr + i);
			lsc_checksum += otp_info.lsc[i];
		}
		checksum = (awb_checksum + lsc_checksum) % 255 + 1;
		otp_info.checksum = read_cmos_sensor(addr + 240);
		if (otp_info.checksum == checksum) {
			otp_info.flag |= 0x10;
			log_inf("Read AWB&LSC OTP Successful\n");
		} else {
			log_inf("Read AWB&LSC OTP Check sum failed\n");
		}
	} else {
		for (i = 0; i < 240; i++)
			otp_info.lsc[i] = 0;
	}
	for (i = 0x7010; i <= 0x720a; i++)
		write_cmos_sensor(i, 0);
	disable_read_otp();
	return 0;
}

static int apply_otp(void)
{
	int rg_ratio_typical = 0, bg_ratio_typical = 0;
	int rg, bg, r_gain, g_gain, b_gain, base_gain, temp, i;

	// apply OTP WB Calibration
	rg = otp_info.rg_ratio;
	bg = otp_info.bg_ratio;
	rg_ratio_typical = otp_info.rg_ratio_golden;
	bg_ratio_typical = otp_info.bg_ratio_golden;
	//calculate G gain
	if (rg == 0 || bg == 0)
		return ERROR_NONE;
	r_gain = (rg_ratio_typical*1000) / rg;
	b_gain = (bg_ratio_typical*1000) / bg;
	g_gain = 1000;
	if (r_gain < 1000 || b_gain < 1000) {
		if (r_gain < b_gain)
			base_gain = r_gain;
		else
			base_gain = b_gain;
	} else {
		base_gain = g_gain;
	}
	if (base_gain == 0)
		return ERROR_NONE;
	r_gain = 0x400 * r_gain / (base_gain);
	b_gain = 0x400 * b_gain / (base_gain);
	g_gain = 0x400 * g_gain / (base_gain);

	log_inf("r_gain = %d, b_gain = %d, g_gain =%d,\n", r_gain, b_gain, g_gain);
	// update sensor WB gain
	if (r_gain > 0x400) {
		write_cmos_sensor(0x5019, r_gain >> 8);
		write_cmos_sensor(0x501a, r_gain & 0x00ff);
	}
	if (g_gain > 0x400) {
		write_cmos_sensor(0x501b, g_gain >> 8);
		write_cmos_sensor(0x501c, g_gain & 0x00ff);
	}
	if (b_gain > 0x400) {
		write_cmos_sensor(0x501d, b_gain >> 8);
		write_cmos_sensor(0x501e, b_gain & 0x00ff);
	}

	// apply OTP Lsc
	temp = read_cmos_sensor(0x5000);
	temp = 0x20 | temp;
	write_cmos_sensor(0x5000, temp);
	for (i = 0; i < 240; i++)
		write_cmos_sensor(0x5900 + i, otp_info.lsc[i]);

	return ERROR_NONE;
}
#endif

static void set_dummy(void)
{
	kal_int32 rc;

	log_inf("ENTER\n");

	rc = imgsensor_sensor_i2c_write(&imgsensor,
		SENSOR_TIMEING_VTS_REG_H,
		imgsensor.frame_length & 0xFFFF,
		IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		log_err("write frame_length failed, frame_length: 0x%x\n",
			imgsensor.frame_length);
		return;
	}
	rc = imgsensor_sensor_i2c_write(&imgsensor,
		SENSOR_TIMEING_HTS_REG_H,
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

	log_inf("ENTER\n");

	if (!framerate || !imgsensor.line_length) {
		log_err("Invalid params. framerate=%u, line_length=%u\n",
			framerate, imgsensor.line_length);
		return;
	}
	frame_length = imgsensor.pclk / framerate * PIX_10_BITS /
		imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
		frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length -
		imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length -
			imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}

static void set_shutter(UINT32 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;

	log_inf("ENTER\n");
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

	/* 3. calc and update framelength to abandon the flicker issue */
	if (imgsensor.autoflicker_en) {
		if (!imgsensor.line_length || !imgsensor.frame_length) {
			log_err("Invalid parms. line_length: %u, frame_length: %u\n",
				imgsensor.line_length, imgsensor.frame_length);
			return;
		}
		realtime_fps = imgsensor.pclk / imgsensor.line_length *
			PIX_10_BITS / imgsensor.frame_length;
		/* 1) calc fps between 298~305, real fps set to 298 */
		if (realtime_fps >= 298 && realtime_fps <= 305)
			set_max_framerate(298, 0);
		/* 2) calc fps between 147~150, real fps set to 146 */
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		/* 3) otherwise, update frame length directly */
		else
			(void)imgsensor_sensor_i2c_write(&imgsensor,
				SENSOR_TIMEING_VTS_REG_H,
				imgsensor.frame_length & 0xFFFF,
				IMGSENSOR_I2C_WORD_DATA);
	} else {
		/* 3) otherwise, update frame length directly */
		(void)imgsensor_sensor_i2c_write(&imgsensor,
				SENSOR_TIMEING_VTS_REG_H,
				imgsensor.frame_length & 0xFFFF,
				IMGSENSOR_I2C_WORD_DATA);
	}

		(void)imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_LONG_EXPO_REG_H,
			(shutter >> 12) & 0x0F,
			IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_LONG_EXPO_REG_M,
			(shutter >> 4) & 0xFF,
			IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_LONG_EXPO_REG_L,
			(shutter << 4) & 0xFF,
			IMGSENSOR_I2C_BYTE_DATA);
	log_inf("shutter = %u, framelength = %u, realtime_fps = %u\n",
		shutter, imgsensor.frame_length, realtime_fps);
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	if (gain < BASEGAIN || gain > 15 * BASEGAIN) {
		log_inf("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 15 * BASEGAIN)
			gain = 15 * BASEGAIN;
	}

	reg_gain = gain*2;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	log_inf("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	(void)imgsensor_sensor_i2c_write(&imgsensor,
		SENSOR_LONG_GAIN_REG_H,
		(reg_gain & 0xFFFF),
		IMGSENSOR_I2C_WORD_DATA);
	return gain;
}

static kal_uint32 sensor_init(void)
{
	kal_int32 rc;

	log_inf("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.init_setting);
	if (rc < 0) {
		log_err("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	log_inf("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_preview_setting(void)
{
	kal_int32 rc;

	log_inf("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.pre_setting);
	if (rc < 0) {
		log_err("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	log_inf("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_capture_setting(kal_uint16 current_fps)
{
	kal_int32 rc;

	log_inf("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.cap_setting);
	if (rc < 0) {
		log_err("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	log_inf("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_normal_video_setting(kal_uint16 current_fps)
{
	kal_int32 rc;

	log_inf("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.normal_video_setting);
	if (rc < 0) {
		log_err("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	log_inf("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	kal_int32 rc;

	log_inf("ENTER.enable: %d\n", enable);
	if (enable)
		rc = imgsensor_sensor_write_setting(&imgsensor,
			&imgsensor_info.test_pattern_on_setting);
	else
		rc = imgsensor_sensor_write_setting(&imgsensor,
			&imgsensor_info.test_pattern_off_setting);
	if (rc < 0)
		log_err("enable: %d failed\n", enable);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	log_inf("EXIT\n");
	return ERROR_NONE;
}

static kal_uint32 return_sensor_id(void)
{
	kal_int32 rc;
	kal_uint16 sensor_id = 0;
	int mode_id = 0, mode_id2 = 0;

	rc = imgsensor_sensor_i2c_read(&imgsensor,
		imgsensor_info.sensor_id_reg, &sensor_id, IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		log_err("Read id failed.id reg: 0x%x\n",
			imgsensor_info.sensor_id_reg);
		sensor_id = 0xFFFF;
	}
	if (sensor_id == OV8856_SENSOR_ID) {
		enable_read_otp();
		mode_id = read_cmos_sensor(0x7011);
		mode_id2 = read_cmos_sensor(0x7018);
		disable_read_otp();
		log_inf("mode_id1 = %d,mode_id2 = %d\n", mode_id, mode_id2);
		if (mode_id == HLT_MOD_ID || mode_id2 == HLT_MOD_ID)
			sensor_id = OV8856_HLT_SENSOR_ID;
		else
			sensor_id = 0;
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

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
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
			log_inf("Fail, reg 0x%x, read: 0x%x, expect:0x%x\n",
				imgsensor.i2c_write_id, *sensor_id,
				imgsensor_info.sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = RETRY_TIMES;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if *sensor_id is not correct, set to 0xFFFFFFFF */
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
	log_inf("sensor probe successfully. sensor_id = 0x%x\n", sensor_id);

	/* initail sequence write in  */
	rc = sensor_init();
	if (rc != ERROR_NONE) {
		log_err("init failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	#ifdef OV8856OTP
	apply_otp();
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
	log_inf("E\n");
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
		log_err("preview_setting failed\n");
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
		log_err("capture_setting failed\n");
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
		log_err("normal video setting failed\n");
		return rc;
	}

	log_inf("EXIT\n");

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
		log_err("NULL ptr. sensor_info:%pK, sensor_config_data:%pK\n",
			sensor_info, sensor_config_data);
		return ERROR_NONE;
	}

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;
	sensor_info->SensorResetActiveHigh = FALSE;
	sensor_info->SensorResetDelayCount = 5;

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;
	sensor_info->SensorPixelClockCount = 3;
	sensor_info->SensorDataLatchCount = 2;

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;  /* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
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
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
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

	log_inf("scenario_id = %d\n", scenario_id);
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
	default:
		log_err("Error ScenarioId setting. scenario_id: %d\n",
			scenario_id);
		rc = preview(image_window, sensor_config_data);
		if (rc != ERROR_NONE)
			log_err("preview failed\n");
		return ERROR_INVALID_SCENARIO_ID;
	}
	return rc;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	log_inf("framerate = %u\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	/* fps set to 298 for anti-flicker */
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	/* fps set to 146 for anti-flicker */
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
	log_inf("enable = %d, framerate = %u\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, UINT32 framerate)
{
	kal_uint32 frame_length;

	log_inf("scenario_id = %d, framerate = %u\n", scenario_id, framerate);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		if ((framerate == 0) || (imgsensor_info.pre.linelength == 0))
			return ERROR_NONE;
		frame_length = imgsensor_info.pre.pclk / framerate *
			PIX_10_BITS / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		if (frame_length > imgsensor_info.pre.framelength)
			imgsensor.dummy_line = (frame_length -
			imgsensor_info.pre.framelength);
		else
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if ((framerate == 0) ||
			(imgsensor_info.normal_video.linelength == 0))
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk / framerate *
			PIX_10_BITS / imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		if (frame_length > imgsensor_info.normal_video.framelength)
			imgsensor.dummy_line =
				(frame_length - imgsensor_info.normal_video.framelength);
		else
			imgsensor.dummy_line = 0;
		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if ((framerate == 0) || (imgsensor_info.cap.linelength == 0))
			return ERROR_NONE;
		frame_length = imgsensor_info.cap.pclk / framerate *
			PIX_10_BITS / imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		if (frame_length > imgsensor_info.cap.framelength)
			imgsensor.dummy_line = (frame_length -
			imgsensor_info.cap.framelength);
		else
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;
	default:  /* coding with  preview scenario by default */
		if ((framerate == 0) || (imgsensor_info.pre.linelength == 0))
			return ERROR_NONE;
		frame_length = imgsensor_info.pre.pclk / framerate *
			PIX_10_BITS / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		if (frame_length > imgsensor_info.pre.framelength)
			imgsensor.dummy_line = (frame_length -
			imgsensor_info.pre.framelength);
		else
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		log_inf("error scenario_id = %d, use preview\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, UINT32 *framerate)
{
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

	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	kal_int32 rc;

	log_inf("Enter.enable:%d\n", enable);
	if (enable)
		rc = imgsensor_sensor_write_setting(&imgsensor,
			&imgsensor_info.streamon_setting);
	else
		rc = imgsensor_sensor_write_setting(&imgsensor,
			&imgsensor_info.streamoff_setting);

	if (rc < 0) {
		log_err("Failed enable:%d\n", enable);
		return ERROR_SENSOR_POWER_ON_FAIL;
	}
	log_inf("Exit.enable:%d\n", enable);

	return ERROR_NONE;
}
static void get_sensor_crop_info(UINT32 feature,
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo)
{
	kal_uint32 rc = ERROR_NONE;

	switch (feature) {
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		/* arry 1 is capture setting */
		rc = memcpy_s((void *)wininfo, sizeof(imgsensor_winsize_info[1]),
			(void *)&imgsensor_winsize_info[1],
			sizeof(imgsensor_winsize_info[1]));
		if (rc != EOK)
				log_err("memcpy_s fail!\n");
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		/* arry 2 is video setting */
		rc = memcpy_s((void *)wininfo, sizeof(imgsensor_winsize_info[2]),
			(void *)&imgsensor_winsize_info[2],
			sizeof(imgsensor_winsize_info[2]));
		if (rc != EOK)
				log_err("memcpy_s fail!\n");
		break;
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	default:
		/* arry 0 is preview setting */
		rc = memcpy_s((void *)wininfo, sizeof(imgsensor_winsize_info[0]),
			(void *)&imgsensor_winsize_info[0],
			sizeof(imgsensor_winsize_info[0]));
		if (rc != EOK)
				log_err("memcpy_s fail!\n");
		break;
	}

}
static kal_uint32 feature_control_ov8856_hlt(MSDK_SENSOR_FEATURE_ENUM feature_id,
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
		log_err("Fatal null. feature_para:%pK,feature_para_len:%pK\n",
			feature_para, feature_para_len);
		return ERROR_NONE;
	}

	log_inf("feature_id = %d\n", feature_id);
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
		set_shutter((UINT32)*feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		rc = get_imgsensor_id(feature_return_para_32);
		if (rc == ERROR_NONE)
			read_otp();
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16,
			*(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
			(UINT32)*(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(UINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		(void)set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		log_inf("current fps :%u\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t)(*(feature_data + 1));
		get_sensor_crop_info(*feature_data_32, wininfo);
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		rc = streaming_control(KAL_FALSE);
		if (rc != ERROR_NONE)
			log_err("stream off failed\n");
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter((UINT32)*feature_data);
		rc = streaming_control(KAL_TRUE);
		if (rc != ERROR_NONE)
			log_err("stream on failed\n");
		break;
	case SENSOR_HUAWEI_FEATURE_DUMP_REG:
		sensor_dump_reg();
		break;
	default:
		log_inf("Not support the feature_id:%d\n", feature_id);
		break;
	}
	return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control_ov8856_hlt,
	control,
	close
};

UINT32 OV8856_HLT_SensorInit(struct SENSOR_FUNCTION_STRUCT **pf_func)
{
	if (pf_func != NULL)
		*pf_func = &sensor_func;
	return ERROR_NONE;
}
