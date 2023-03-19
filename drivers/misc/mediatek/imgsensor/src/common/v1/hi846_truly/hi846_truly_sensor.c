/*
 * hi846_truly_sensor.c
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * hi846_truly image sensor driver
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

#include "hi846_truly_sensor.h"
#include <linux/spinlock.h>
#include <linux/printk.h>
#include "kd_imgsensor_errcode.h"
#include "imgsensor_sensor_i2c.h"

#define Hi846_MAXGAIN 16
#define RETRY_TIMES 2

/* Modify Following Strings for Debug */
#define PFX "[hi846_truly]"

#define log_dbg(fmt, args...) \
	pr_debug(PFX "%s %d " fmt, __func__, __LINE__, ##args)
#define log_inf(fmt, args...) pr_info(PFX "%s %d " fmt, __func__, __LINE__, ##args)
#define log_err(fmt, args...) pr_err(PFX "%s %d " fmt, __func__, __LINE__, ##args)

/* Modify end */
static DEFINE_SPINLOCK(imgsensor_drv_lock);

static void set_dummy(void)
{
	log_dbg("ENTER\n");
	(void)imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_FRM_LENGTH_LINES_REG_H,
			imgsensor.frame_length,
			IMGSENSOR_I2C_WORD_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_LINE_LENGTH_PCK_REG_H,
			imgsensor.line_length,
			IMGSENSOR_I2C_WORD_DATA);
}

static void write_af_sensor(kal_uint32 para)
{
	char pu_send_cmd[2] = {(char)(para >> 8), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 2, 0x18);
}

static void set_af_power_down_mode(void)
{
	write_af_sensor(0x8000);
	mdelay(12);
}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 4, imgsensor.i2c_write_id);
}

static void write_cmos_sensor_8(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void hi846_otp_init(void)
{
	write_cmos_sensor(0x2000, 0x0000);
	write_cmos_sensor(0x2002, 0x00FF);
	write_cmos_sensor(0x2004, 0x0000);
	write_cmos_sensor(0x2008, 0x3FFF);
	write_cmos_sensor(0x23FE, 0xC056);
	write_cmos_sensor(0x326E, 0x0000);
	write_cmos_sensor(0x0A00, 0x0000);
	write_cmos_sensor(0x0E04, 0x0012);
	write_cmos_sensor(0x0F08, 0x2F04);
	write_cmos_sensor(0x0F30, 0x001F);
	write_cmos_sensor(0x0F36, 0x001F);
	write_cmos_sensor(0x0F04, 0x3A00);
	write_cmos_sensor(0x0F32, 0x025A);
	write_cmos_sensor(0x0F38, 0x025A);
	write_cmos_sensor(0x0F2A, 0x4124);
	write_cmos_sensor(0x006A, 0x0100);
	write_cmos_sensor(0x004C, 0x0100);
	write_cmos_sensor(0x0A00, 0x0100);
}

static void hi846_otp_enable(void)
{
	write_cmos_sensor_8(0x0A02, 0x01);
	write_cmos_sensor_8(0x0A00, 0x00);
	mdelay(10);
	write_cmos_sensor_8(0x0F02, 0x00);
	write_cmos_sensor_8(0x071A, 0x01);
	write_cmos_sensor_8(0x071B, 0x09);
	write_cmos_sensor_8(0x0D04, 0x00);
	write_cmos_sensor_8(0x0D00, 0x07);
	write_cmos_sensor_8(0x003E, 0x10);
	write_cmos_sensor_8(0x0A00, 0x01);
}

static void hi846_otp_disable(void)
{
	write_cmos_sensor_8(0x0A00, 0x00);
	mdelay(10);
	write_cmos_sensor_8(0x003E, 0x00);
	write_cmos_sensor_8(0x0A00, 0x01);
	write_cmos_sensor(0x004c, 0x0000);
	mdelay(1);
}

static kal_uint8 hi846_otp_read_byte(kal_uint16 addr)
{
	write_cmos_sensor_8(HI864_OTP_REG_ADDRH, addr >> 8);
	write_cmos_sensor_8(HI864_OTP_REG_ADDRL, addr & 0xFF);
	write_cmos_sensor_8(HI864_OTP_REG_CMD, HI864_OTP_CMD_NORMAL);
	return (kal_uint8)read_cmos_sensor(HI864_OTP_REG_RDATA);
}

static void hi846_read_otp(void)
{
	kal_uint8 flag_module = 0;
	kal_uint32 i = 0;

	const kal_uint16 addr_module[] = {HI864_OTP_OFFSET_MODULE1,
		HI864_OTP_OFFSET_MODULE2};

	hi846_otp_init();
	hi846_otp_enable();
	flag_module = hi846_otp_read_byte(HI864_OTP_OFFSET_MODULE_FLAG);

	for (i = 0; i < 2; i++) {
		int flag = (flag_module >> (2 * i)) & 0x3;

		if (flag == 0x1) {
			otp_info.flag_module = 1;
			otp_info.mid = hi846_otp_read_byte(addr_module[i]);
			log_inf(" module group%d is ok mid =0x%x", i, otp_info.mid);
			break;
		}
	}
	hi846_otp_disable();
}

static kal_uint32 return_sensor_id(void)
{
	kal_int32 rc;
	kal_uint16 sensor_id = 0;
	kal_uint16 sensor_id_l;
	kal_uint16 sensor_id_h;

	rc = imgsensor_sensor_i2c_read(&imgsensor,
			imgsensor_info.sensor_id_reg,
			&sensor_id, IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		log_err("Read id failed.id reg: 0x%x\n",
			imgsensor_info.sensor_id_reg);
		sensor_id = 0xFFFF;
	}

	sensor_id_h = sensor_id & 0xff;
	sensor_id_l = (sensor_id >> 8) & 0xff;
	sensor_id = (sensor_id_h << 8 | sensor_id_l);
	if (sensor_id == HI846_SENSOR_ID) {
		hi846_read_otp();
		if (otp_info.mid == HI846_TRULY_MODULE_ID)
			sensor_id = HI846_TRULY_SENSOR_ID;
		else
			sensor_id = 0;
	}
	return sensor_id;
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	log_dbg("ENTER\n");

	if (!framerate || !imgsensor.line_length) {
		log_err("Invalid params. framerate=%d, line_length=%d\n",
			framerate, imgsensor.line_length);
		return;
	}
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
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

static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;

	log_dbg("ENTER\n");

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

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
				imgsensor.frame_length;
		/* calc fps between 298~305, real fps set to 298 */
		if (realtime_fps >= 298 && realtime_fps <= 305)
			set_max_framerate(298, 0);
		/* calc fps between 147~150, real fps set to 146 */
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			(void)imgsensor_sensor_i2c_write(&imgsensor,
					SENSOR_FRM_LENGTH_LINES_REG_H,
					imgsensor.frame_length,
					IMGSENSOR_I2C_WORD_DATA);
	} else {
		(void)imgsensor_sensor_i2c_write(&imgsensor,
				SENSOR_FRM_LENGTH_LINES_REG_H,
				imgsensor.frame_length,
				IMGSENSOR_I2C_WORD_DATA);
	}

	(void)imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_INTEG_TIME_REG_H,
			shutter,
			IMGSENSOR_I2C_WORD_DATA);

	log_dbg("Exit! shutter =%d, framelength =%d\n", shutter,
		imgsensor.frame_length);
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
	kal_uint16 reg_gain;

	reg_gain = gain / 4 - 16;

	return (kal_uint16)reg_gain;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	log_dbg("ENTER\n");
	/*
	 * 0x350A[0:1], 0x350B[0:7] AGC real gain
	 * [0:3] = N meams N /16 X
	 * [4:9] = M meams M X
	 * Total gain = M + N /16 X
	 */

	if (gain < BASEGAIN) {
		log_err("Invaild gain: %d\n", gain);
		gain = BASEGAIN;
	} else if (gain > Hi846_MAXGAIN * BASEGAIN) {
		log_err("Invaild gain: %d\n", gain);
		gain = Hi846_MAXGAIN * BASEGAIN;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	log_dbg("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	(void)imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_ANA_GAIN_REG,
			(reg_gain & 0xFFFF),
			IMGSENSOR_I2C_WORD_DATA);
	log_dbg("EXIT\n");

	return gain;
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

static void set_mirror_flip(kal_uint8 image_mirror)
{
	/*
	 * SENSOR_IMAGE_ORIENTATION:
	 * bit[7:2]: Reserved
	 * bit[1]: V Flip enable [0:no flip, 1:v flip]
	 * bit[0]: H mirror enable [0:no mirror, 1: H mirror]
	 */
	switch (image_mirror) {
	case IMAGE_NORMAL:
		(void)imgsensor_sensor_i2c_write(&imgsensor,
				SENSOR_IMAGE_ORIENTATION, 0x0000,
				IMGSENSOR_I2C_WORD_DATA);
		break;
	case IMAGE_H_MIRROR:
		(void)imgsensor_sensor_i2c_write(&imgsensor,
				SENSOR_IMAGE_ORIENTATION, 0x0100,
				IMGSENSOR_I2C_WORD_DATA);
		break;
	case IMAGE_V_MIRROR:
		(void)imgsensor_sensor_i2c_write(&imgsensor,
				SENSOR_IMAGE_ORIENTATION, 0x0200,
				IMGSENSOR_I2C_WORD_DATA);
		break;
	case IMAGE_HV_MIRROR:
		(void)imgsensor_sensor_i2c_write(&imgsensor,
				SENSOR_IMAGE_ORIENTATION, 0x0300,
				IMGSENSOR_I2C_WORD_DATA);
		break;
	default:
		log_inf("Error image_mirror setting");
	}
}

static void sensor_init(void)
{
	kal_int32 rc;

	log_dbg("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.init_setting);
	if (rc < 0) {
		log_err("Failed\n");
		return;
	}
	log_dbg("EXIT\n");
}

static void set_preview_setting(void)
{
	kal_int32 rc;

	log_dbg("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.pre_setting);
	if (rc < 0) {
		log_err("Failed\n");
		return;
	}
	log_dbg("EXIT\n");
}

static void set_capture_setting(void)
{
	kal_int32 rc;

	log_dbg("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.cap_setting);
	if (rc < 0) {
		log_err("Failed\n");
		return;
	}
	log_dbg("EXIT\n");
}

static void set_normal_video_setting(void)
{
	kal_int32 rc;

	log_dbg("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
				&imgsensor_info.normal_video_setting);
	if (rc < 0) {
		log_err("Failed\n");
		return;
	}

	log_dbg("EXIT\n");

}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = RETRY_TIMES; /* retry 2 time */
	UINT32 reg_sensor_id = 0;

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
			reg_sensor_id = return_sensor_id();
			if (reg_sensor_id == imgsensor_info.sensor_id) {
				log_inf("id reg: 0x%x, read id: 0x%x,\n",
					imgsensor.i2c_write_id,
					reg_sensor_id);
				*sensor_id = reg_sensor_id;
				if (setaf_flag == FAIL) {
					set_af_power_down_mode();
					setaf_flag = TURE;
				}
				return ERROR_NONE;
			}
			log_inf("Check id fail,reg: 0x%x,read id: 0x%x, expect id:0x%x\n",
				imgsensor.i2c_write_id,
				reg_sensor_id,
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

static kal_uint32 open(void)
{
	int rc;
	UINT32 sensor_id = 0;

	log_inf("ENTER\n");

	rc = get_imgsensor_id(&sensor_id);
	if (rc != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;
	log_dbg("sensor probe successfully. sensor_id=0x%x\n", sensor_id);

	sensor_init();

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
	log_inf("ENTER\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	set_mirror_flip(imgsensor.mirror);
	set_preview_setting();
	log_inf("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	log_inf("ENTER\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mirror_flip(imgsensor.mirror);
	set_capture_setting();
	log_inf("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	log_inf("ENTER\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.current_fps = imgsensor_info.normal_video.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mirror_flip(imgsensor.mirror);
	set_normal_video_setting();

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
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_INFO_STRUCT *sensor_info,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	if (!sensor_info || !sensor_config_data) {
		log_err("Fatal: NULL ptr. sensor_info:%pK, sensor_config_data:%pK\n",
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
	log_dbg("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		normal_video(image_window, sensor_config_data);
		break;
	default:
		log_err("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	log_inf("framerate = %d\n ", framerate);
	// SetVideoMode Function should fix framerate
	if (framerate == 0)
		return ERROR_NONE;

	spin_lock(&imgsensor_drv_lock);
	/* fps set to 298 for anti-flicker */
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 298;
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
	log_inf("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			UINT32 framerate)
{
	kal_uint32 frame_length;

	log_dbg("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

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
		log_err("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM
		scenario_id, UINT32 *framerate)
{
	if (framerate != NULL) {
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

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	/*
	 * 0x0A04(SENSOR_ISP_EN_REG_H)
	 * bit[7:1]: Reserved
	 * bit[0]: mipi enable
	 *
	 * 0x0A05(SENSOR_ISP_EN_REG_L)
	 * bit[7]: Reserved
	 * bit[6]: Fmt enable
	 * bit[5]: H binning enable
	 * bit[4]: Lsc enable
	 * bit[3]: dga enable
	 * bit[2]: Reserved
	 * bit[1]: adpc enable
	 * bit[0]: tpg enable
	 */
	/*
	 * 0x020a(SENSOR_TP_MODE_REG)
	 * bit[3:0]: TP Mode
	 * 0: no pattern
	 * 1: solid color
	 * 2: 100% color bars
	 * 3: Fade to grey color bars
	 * 4: PN9
	 * 5: h gradient pattern
	 * 6: v gradient pattern
	 * 7: check board
	 * 8: slant pattern
	 * 9: resolution pattern
	 */
	if (enable) {
		log_inf("enter color bar");
		(void)imgsensor_sensor_i2c_write(&imgsensor,
				SENSOR_ISP_EN_REG_H, 0x0141,
				IMGSENSOR_I2C_WORD_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor,
				SENSOR_TP_MODE_REG, 0x0200,
				IMGSENSOR_I2C_WORD_DATA);
	} else {
		(void)imgsensor_sensor_i2c_write(&imgsensor,
				SENSOR_ISP_EN_REG_H, 0x0142,
				IMGSENSOR_I2C_WORD_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor,
				SENSOR_TP_MODE_REG, 0x0000,
				IMGSENSOR_I2C_WORD_DATA);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
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

static kal_uint32 feature_control_hi846_truly(MSDK_SENSOR_FEATURE_ENUM feature_id,
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
		log_err("Fatal null ptr. feature_para:%pK,feature_para_len:%pK\n",
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
		// for factory mode auto testing
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		log_dbg("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
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
		(void)streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter((UINT16)*feature_data);
		(void)streaming_control(KAL_TRUE);
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
		log_inf("Not support the feature_id:%d\n", feature_id);
		break;
	}
	return rc;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control_hi846_truly,
	control,
	close
};

UINT32 HI846_TRULY_SensorInit(struct SENSOR_FUNCTION_STRUCT **pf_func)
{
	if (pf_func != NULL)
		*pf_func = &sensor_func;
	return ERROR_NONE;
}
