/*
 * s5k4h7_txd_sensor.c
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * s5k4h7_txd image sensor driver
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

#include "s5k4h7_txd_sensor.h"

#define RETRY_TIMES 2

#define PFX "[s5k4h7_txd]"
#define log_dbg(fmt, args...) \
	pr_debug(PFX "%s %d " fmt, __func__, __LINE__, ##args)
#define log_inf(fmt, args...) \
	pr_info(PFX "%s %d " fmt, __func__, __LINE__, ##args)
#define log_err(fmt, args...) \
	pr_err(PFX "%s %d " fmt, __func__, __LINE__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

struct otp_struct g_otp_info;

static void s5k4h7_write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void get_4h7_page_awb_data(int pageidx, unsigned char *pdata)
{
	unsigned short get_byte;
	unsigned int addr = S5K4H7_OTP_AWB_FLAG_ADDR;
	int i;
	int j = REG_READ_TIMES;

	s5k4h7_write_cmos_sensor_8(0x0A02, pageidx);
	s5k4h7_write_cmos_sensor_8(0x0A00, 0x01);

	do {
		mdelay(1);
		get_byte = read_cmos_sensor(0x0A01);
		j--;
	} while ((get_byte & 0x01) != 1 && (j > 0));

	for (i = 0; i < S5K4H7_AWB_FLAG_TEMP; i++) {
		pdata[i] = read_cmos_sensor(addr);
		addr++;
	}

	s5k4h7_write_cmos_sensor_8(0x0A00, 0x00);
}

static unsigned short selective_read_region(int pageidx, unsigned int addr)
{
	unsigned short get_byte;
	int i = REG_READ_TIMES;

	s5k4h7_write_cmos_sensor_8(0x0A02, pageidx);
	s5k4h7_write_cmos_sensor_8(0x0A00, 0x01);
	do {
		mdelay(1);
		get_byte = read_cmos_sensor(0x0A01);
		i--;
	} while ((get_byte & 0x01) != 1 && (i > 0));

	get_byte = read_cmos_sensor(addr);
	s5k4h7_write_cmos_sensor_8(0x0A00, 0x00);

	return get_byte;
}

static int apply_4h7_otp_awb(void)
{
	unsigned int r_ratio;
	unsigned int b_ratio;
	unsigned int g_ratio;

	if (g_otp_info.rg_current == 0 || g_otp_info.bg_current == 0) {
		log_err("otp data error\n");
		return -1;
	}
	r_ratio = (unsigned int)(g_otp_info.rg_golden * BASE_RATIO /
		g_otp_info.rg_current);
	b_ratio = (unsigned int)(g_otp_info.bg_golden * BASE_RATIO /
		g_otp_info.bg_current);
	g_ratio = BASE_RATIO;

	log_inf("r_ratio = 0x%x, b_ratio = 0x%x, g_ratio = 0x%x.\n",
		r_ratio, b_ratio, g_ratio);
	if (r_ratio == 0 || b_ratio == 0) {
		log_err("otp data error\n");
		return -1;
	}
	if (r_ratio < b_ratio) {
		if (r_ratio < BASE_RATIO) {
			b_ratio = BASE_GAIN_RATIO * b_ratio / r_ratio;
			g_ratio = BASE_GAIN_RATIO * g_ratio / r_ratio;
			r_ratio = BASE_GAIN_RATIO;
		}
	} else {
		if (b_ratio < BASE_RATIO) {
			r_ratio = BASE_GAIN_RATIO * r_ratio / b_ratio;
			g_ratio = BASE_GAIN_RATIO * g_ratio / b_ratio;
			b_ratio = BASE_GAIN_RATIO;
		}
	}
	log_inf("g_ratio = 0x%x, r_ratio = 0x%x, b_ratio = 0x%x.\n",
		g_ratio, r_ratio, b_ratio);

	s5k4h7_write_cmos_sensor_8(0x0210, (r_ratio >> 8) & 0xff);
	s5k4h7_write_cmos_sensor_8(0x0211, (r_ratio >> 0) & 0xff);

	s5k4h7_write_cmos_sensor_8(0x020E, (g_ratio >> 8) & 0xff);
	s5k4h7_write_cmos_sensor_8(0x020F, (g_ratio >> 0) & 0xff);

	s5k4h7_write_cmos_sensor_8(0x0214, (g_ratio >> 8) & 0xff);
	s5k4h7_write_cmos_sensor_8(0x0215, (g_ratio >> 0) & 0xff);

	s5k4h7_write_cmos_sensor_8(0x0212, (b_ratio >> 8) & 0xff);
	s5k4h7_write_cmos_sensor_8(0x0213, (b_ratio >> 0) & 0xff);
	return 0;
}

static void apply_4h7_otp_enb_lsc(void)
{
	log_inf("enable lsc\n");
	s5k4h7_write_cmos_sensor_8(0x0B00, 0x01);
}

static void otp_update_lsc_4h7(void)
{
	g_otp_info.lsc_flag =
		selective_read_region(S5K4H7_LSC_PAGE,
			S5K4H7_OTP_LSC_FLAG_ADDR);

	if ((g_otp_info.awb_flag & AWB_GROUP1) && (g_otp_info.lsc_flag ==
		LSC_GROUP1)) {
		g_otp_info.lsc_group = 1;
	} else if ((g_otp_info.awb_flag & AWB_GROUP2) && (g_otp_info.lsc_flag ==
		LSC_GROUP2)) {
		g_otp_info.lsc_group = 2;
	} else {
		g_otp_info.lsc_group = 0;
		log_inf("lsc group is invalid/empty");
	}

	if (g_otp_info.lsc_group != 0)
		apply_4h7_otp_enb_lsc();

}

static int get_awb_otp_data(void)
{
	unsigned char data_p[S5K4H7_AWB_FLAG_TEMP] = {0};
	unsigned int  sum_awbfg = 0;
	unsigned short offset;

	get_4h7_page_awb_data(S5K4H7_AWB_PAGE, data_p);
	if ((g_otp_info.awb_flag & AWB_GROUP1) &&
		(data_p[0] & AWB_GROUP1)) {
		// awb group1
		log_inf("awb group1 is valid\n");
		offset = (S5K4H7_GROUP1_AWB_DATA_ADDR -
			S5K4H7_OTP_AWB_FLAG_ADDR);
		sum_awbfg = 1;
	} else if ((g_otp_info.awb_flag & AWB_GROUP2) &&
		(data_p[0] & AWB_GROUP2)) {
		// awb group2
		log_inf("awb group2 is valid\n");
		offset = (S5K4H7_GROUP2_AWB_DATA_ADDR -
			S5K4H7_OTP_AWB_FLAG_ADDR);
		sum_awbfg = 1;
	} else {
		pr_err("awb group is invalid/empty, gropflag = 0x%x, awbflag = 0x%x\n",
			g_otp_info.awb_flag, data_p[0]);
			return -1;
	}
	if (sum_awbfg) {
		g_otp_info.rg_current = (data_p[offset + 0] << 8) |
			data_p[offset + 1];
		g_otp_info.bg_current = (data_p[offset + 2] << 8) |
			data_p[offset + 3];
		g_otp_info.grgb_current = (data_p[offset + 4] << 8) |
			data_p[offset + 5];
		g_otp_info.rg_golden = (data_p[offset + 6] << 8) |
			data_p[offset + 7];
		g_otp_info.bg_golden = (data_p[offset + 8] << 8) |
			data_p[offset + 9];
		g_otp_info.grgb_golden = (data_p[offset + 10] << 8) |
			data_p[offset + 11];
		log_inf("Module: rg:%d, bg:%d, gg:%d\n",
			g_otp_info.rg_current, g_otp_info.bg_current,
				g_otp_info.grgb_current);
		log_inf("Golden: rg:%d, bg:%d, gg:%d\n",
			g_otp_info.rg_golden, g_otp_info.bg_golden,
				g_otp_info.grgb_golden);
		apply_4h7_otp_awb();
	} else {
		pr_err("check awb flag fail!!!");
		return -1;
	}
	return sum_awbfg;
}

unsigned int update_otp(void)
{
	memset_s(&g_otp_info, sizeof(g_otp_info), 0, sizeof(g_otp_info));
	g_otp_info.awb_flag = selective_read_region(S5K4H7_AWB_PAGE,
		S5K4H7_OTP_AWB_FLAG_ADDR);
	log_inf("group_flag = 0x%x\n", g_otp_info.awb_flag);
	otp_update_lsc_4h7();
	get_awb_otp_data();
	return 0;
}

static void set_dummy(void)
{
	kal_int32 rc;

	log_dbg("ENTER\n");

	rc = imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_FRM_LENGHT_LINE_REG_H,
			imgsensor.frame_length & 0xFFFF,
			IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		log_err("write frame_length failed, frame_length: 0x%x\n",
			imgsensor.frame_length);
		return;
	}
	rc = imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_LINE_LENGHT_PCK_REG_H,
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
		log_err("Invalid params. framerate=%u, line_length=0x%x\n",
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
				SENSOR_FRM_LENGHT_LINE_REG_H,
				imgsensor.frame_length & 0xFFFF,
				IMGSENSOR_I2C_WORD_DATA);
	} else {
		/* 3) otherwise, update frame length directly */
		(void)imgsensor_sensor_i2c_write(&imgsensor,
				SENSOR_FRM_LENGHT_LINE_REG_H,
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

	reg_gain = (gain * REG_GAIN_1X) / BASEGAIN;

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
	log_dbg("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

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

static kal_uint32 return_sensor_id(void)
{
	kal_int32 rc;
	kal_uint16 sensor_id = 0;
	int mode_id = 0, mode_id1 = 0;

	rc = imgsensor_sensor_i2c_read(&imgsensor,
			imgsensor_info.sensor_id_reg,
			&sensor_id, IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		log_err("Read id failed.id reg: 0x%x\n",
			imgsensor_info.sensor_id_reg);
		sensor_id = 0xFFFF;
	}
	if (sensor_id == S5K4H7_SENSOR_ID) {
		mode_id = selective_read_region(S5K4H7_AWB_PAGE,
			S5K4H7_GROUP1_MODULE_ID_ADDR);
		mode_id1 = selective_read_region(S5K4H7_AWB_PAGE,
			S5K4H7_GROUP2_MODULE_ID_ADDR);
		log_inf("mode_id1 = %d,mode_id2 = %d\n",
			mode_id, mode_id1);
		if (mode_id == TXD_MOD_ID || mode_id1 == TXD_MOD_ID)
			sensor_id = S5K4H7_TXD_SENSOR_ID;
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

	log_dbg("Enter\n");

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
			log_inf("Read id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id,
				*sensor_id);
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

	/* initail sequence write in  */
	rc = sensor_init();
	if (rc != ERROR_NONE) {
		log_err("init failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	update_otp();
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

static kal_uint32 feature_control_s5k4h7_txd(MSDK_SENSOR_FEATURE_ENUM
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

	return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control_s5k4h7_txd,
	control,
	close
};

UINT32 s5k4h7_txd_sensorinit(struct SENSOR_FUNCTION_STRUCT **pf_func)
{
	if (pf_func != NULL)
		*pf_func = &sensor_func;
	return ERROR_NONE;
}
