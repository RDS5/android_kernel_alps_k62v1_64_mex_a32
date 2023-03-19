/*
 * hi1336_qtech_sensor.c
 *
 * Copyright (c) 2019-2019 Huawei Technologies Co., Ltd.
 *
 * hi1336_qtech image sensor driver
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

#include "hi1336_qtech_sensor.h"
#include <linux/spinlock.h>
#include <linux/printk.h>
#include <linux/delay.h>
#include "kd_imgsensor_errcode.h"
#include "imgsensor_sensor_i2c.h"

#define RETRY_TIMES 2

#define PFX "[hi1336_qtech]"
#define DEBUG_HI1336_QTECH 1
#define LOG_DBG(fmt, args...) \
	do { \
		if (DEBUG_HI1336_QTECH) \
			pr_debug(PFX "%s %d " fmt, __func__, __LINE__, ##args); \
	} while (0)
#define LOG_INF(fmt, args...) pr_info(PFX "%s %d " fmt, __func__, __LINE__, ##args)
#define LOG_ERR(fmt, args...) pr_err(PFX "%s %d " fmt, __func__, __LINE__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static void set_dummy(void)
{
	kal_int32 rc;

	LOG_INF("set_dummy line_length:0x%x, frame_length:0x%x\n",
		imgsensor.line_length, imgsensor.frame_length);
	rc = imgsensor_sensor_i2c_write(&imgsensor,
		SENSOR_FRM_LENGTH_REG_H,
		imgsensor.frame_length,
		IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		LOG_ERR("write frame_length failed, frame_length:0x%x\n",
			imgsensor.frame_length);
		return;
	}
	rc = imgsensor_sensor_i2c_write(&imgsensor,
		SENSOR_LINE_LENGTH_PCK_REG_H,
		imgsensor.line_length / 4,
		IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		LOG_ERR("write line_length failed, line_length:0x%x\n",
			imgsensor.frame_length);
		return;
	}
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length;

	LOG_DBG("ENTER\n");

	if (!framerate || !imgsensor.line_length) {
		LOG_ERR("Invalid params. framerate=%u, line_length=0x%x\n",
			framerate, imgsensor.line_length);
		return;
	}
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
		(frame_length > imgsensor.min_frame_length) ?
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

static void write_shutter(UINT32 shutter)
{
	kal_uint16 realtime_fps = 0;

	LOG_DBG("E\n");

	if (!imgsensor.frame_length || !imgsensor.line_length) {
		LOG_ERR("Invalid params");
		return;
	}

	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;

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
				SENSOR_FRM_LENGTH_REG_H,
				imgsensor.frame_length,
				IMGSENSOR_I2C_WORD_DATA);
	} else {
		// Extend frame length
		(void)imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_FRM_LENGTH_REG_H,
			imgsensor.frame_length,
			IMGSENSOR_I2C_WORD_DATA);
	}
	(void)imgsensor_sensor_i2c_write(&imgsensor,
		SENSOR_INTEG_TIME_HL,
		((shutter & 0xFF0000) >> 16),
		IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor,
		SENSOR_INTEG_TIME_H,
		(shutter & 0xFFFF),
		IMGSENSOR_I2C_WORD_DATA);
	LOG_DBG("Exit! shutter =%u, framelength =0x%x\n", shutter,
		imgsensor.frame_length);
}

static void set_shutter(UINT32 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	write_shutter(shutter);
}

static void set_shutter_frame_length(UINT32 shutter, UINT32 frame_length)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	LOG_DBG("set_shutter_frame_length Enter. shutter = %u\n", shutter);
	spin_lock(&imgsensor_drv_lock);
	/* Change frame time */
	if (frame_length > 1)
		imgsensor.frame_length = frame_length;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length < imgsensor.min_frame_length)
		imgsensor.frame_length = imgsensor.min_frame_length;
	// make sure frame_length smaller than limitation
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	write_shutter(shutter);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain;
	/* analog gain = reg_gain / 16 + 1 */
	reg_gain = (gain * HI1336_GAIN_1X) / BASEGAIN - HI1336_GAIN_1X;
	return reg_gain;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	/*
	 * [0:3] = N meams N /16 X
	 * [4:9] = M meams M X
	 * Total gain = M + N /16 X
	 */
	LOG_DBG("gain [%u]", gain);

	if (gain < BASEGAIN || gain > HI1336_QTECH_MAX_GAIN * BASEGAIN) {
		LOG_INF("Error gain setting");
		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > HI1336_QTECH_MAX_GAIN * BASEGAIN)
			gain = HI1336_QTECH_MAX_GAIN * BASEGAIN;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_DBG("gain = %d , reg_gain = 0x%x\n", gain, reg_gain);

	reg_gain = reg_gain & 0x00FF;
	(void)imgsensor_sensor_i2c_write(&imgsensor,
		SENSOR_ANA_GAIN_REG_L, reg_gain,
		IMGSENSOR_I2C_WORD_DATA);
	LOG_DBG("EXIT\n");

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

	LOG_DBG("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.init_setting);
	if (rc < 0) {
		LOG_ERR("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_DBG("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_preview_setting(void)
{
	kal_int32 rc;

	LOG_DBG("ENTER\n");
	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.pre_setting);
	if (rc < 0) {
		LOG_ERR("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	LOG_DBG("EXIT\n");
	return ERROR_NONE;
}

static kal_uint32 set_capture_setting(void)
{
	kal_int32 rc;

	LOG_DBG("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.cap_setting);
	if (rc < 0) {
		LOG_ERR("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_DBG("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_normal_video_setting(void)
{
	kal_int32 rc;

	LOG_DBG("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.normal_video_setting);
	if (rc < 0) {
		LOG_ERR("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	LOG_DBG("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_hs_video_setting(void)
{
	kal_int32 rc;
	LOG_DBG("ENTER.\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.hs_video_setting);
	if (rc < 0) {
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	LOG_DBG("EXIT.\n");

	return ERROR_NONE;
}

static kal_uint32 set_ls_video_setting(void)
{
	kal_int32 rc;
	LOG_DBG("ENTER.\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.slim_setting);
	if (rc < 0) {
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	LOG_DBG("EXIT.\n");

	return ERROR_NONE;
}

static kal_uint32 set_custom1_setting(void)
{
	kal_int32 rc = 0;
	LOG_DBG("ENTER\n");
	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.custom1_setting);
	if (rc < 0) {
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_DBG("EXIT.\n");

	return ERROR_NONE;
}

static kal_uint32 return_sensor_id(void)
{
	kal_int32 rc;
	kal_uint16 sensor_id = 0;

	rc = imgsensor_sensor_i2c_read(&imgsensor, imgsensor_info.sensor_id_reg,
		&sensor_id, IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		LOG_ERR("Read id failed.id reg: 0x%x\n",
			imgsensor_info.sensor_id_reg);
		sensor_id = 0xFFFF;
	}
	return sensor_id;
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
				LOG_INF("reg: 0x%x, read: 0x%x, expect:0x%x\n",
					imgsensor.i2c_write_id,
					*sensor_id,
					imgsensor_info.sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("CheckIdFail,reg:0x%x,read:0x%x,expect:0x%x\n",
				imgsensor.i2c_write_id,
				*sensor_id,
				imgsensor_info.sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = RETRY_TIMES;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static kal_uint32 open(void)
{
	kal_uint32 sensor_id = 0;
	kal_uint32 rc;

	LOG_INF("ENTER\n");

	rc = get_imgsensor_id(&sensor_id);
	if (rc != ERROR_NONE) {
		LOG_ERR("probe sensor failed\n");
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	LOG_DBG("sensor probe successfully. sensor_id=0x%x\n", sensor_id);

	/* initail sequence write in  */
	rc = sensor_init();
	if (rc != ERROR_NONE) {
		LOG_ERR("init failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
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
	LOG_INF("E\n");
	return ERROR_NONE;
}

static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	LOG_INF("ENTER\n");

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
		LOG_ERR("preview_setting failed\n");
		return rc;
	}
	mdelay(10);
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	LOG_INF("ENTER\n");

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
		LOG_ERR("capture_setting failed\n");
		return rc;
	}
	mdelay(10);
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	LOG_INF("ENTER\n");

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
		LOG_ERR("normal video setting failed\n");
		return rc;
	}
	mdelay(10);
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 hs_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	LOG_INF("ENTER.\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.current_fps = imgsensor_info.hs_video.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	rc = set_hs_video_setting();
	if (rc != ERROR_NONE) {
		LOG_ERR("normal video setting failed.\n");
		return rc;
	}
	mdelay(10);
	LOG_INF("EXIT.\n");

	return ERROR_NONE;
}

static kal_uint32 slim_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	LOG_INF("ENTER.\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.current_fps = imgsensor_info.slim_video.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	rc = set_ls_video_setting();
	if (rc != ERROR_NONE) {
		LOG_ERR("slim_video setting failed.\n");
		return rc;
	}
	mdelay(10);
	LOG_INF("EXIT.\n");

	return ERROR_NONE;
}

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	LOG_INF("ENTER.\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);

	rc = set_custom1_setting();
	if (rc != ERROR_NONE) {
		LOG_ERR("custom1 setting failed.\n");
		return rc;
	}
	mdelay(10);
	LOG_INF("EXIT\n");

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

		sensor_resolution->SensorHighSpeedVideoWidth =
			imgsensor_info.hs_video.grabwindow_width;

		sensor_resolution->SensorHighSpeedVideoHeight =
			imgsensor_info.hs_video.grabwindow_height;

		sensor_resolution->SensorSlimVideoWidth =
			imgsensor_info.slim_video.grabwindow_width;

		sensor_resolution->SensorSlimVideoHeight =
			imgsensor_info.slim_video.grabwindow_height;

		sensor_resolution->SensorCustom1Width =
			imgsensor_info.custom1.grabwindow_width;

		sensor_resolution->SensorCustom1Height =
			imgsensor_info.custom1.grabwindow_height;
	}
	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_INFO_STRUCT *sensor_info,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	if (!sensor_info || !sensor_config_data) {
		LOG_ERR("Fatal: NULL ptr");
		return ERROR_INVALID_PARA;
	}
	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity =
		SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity =
		SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;
	sensor_info->SensorResetActiveHigh = FALSE;
	sensor_info->SensorResetDelayCount = 5;

	sensor_info->SensroInterfaceType =
		imgsensor_info.sensor_interface_type;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame =
		imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame =
		imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame =
		imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;
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
	sensor_info->PDAF_Support = PDAF_SUPPORT_RAW;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;
	sensor_info->SensorPixelClockCount = 3;
	sensor_info->SensorDataLatchCount = 2;

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;
	sensor_info->SensorHightSampling = 0;
	sensor_info->SensorPacketECCOrder = 1;

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
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX =
			imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.slim_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
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
	LOG_DBG("scenario_id = %d\n", scenario_id);
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
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		custom1(image_window, sensor_config_data);
		break;
	default:
		LOG_ERR("Error ScenarioId setting. scenario_id:%d\n",
			scenario_id);
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %u\n ", framerate);
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
	LOG_INF("enable = %d, framerate = %u\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
	UINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_DBG("scenario_id = %d, framerate = %u\n", scenario_id, framerate);

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
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk
			/ framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.hs_video.framelength)
		? (frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length =
		    imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk
			/ framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.slim_video.framelength)
		? (frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length =
		  imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		if (!framerate || !imgsensor_info.custom1.linelength)
			return ERROR_NONE;
		frame_length = imgsensor_info.custom1.pclk / framerate * 10 /
			imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom1.framelength) ?
			(frame_length - imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength +
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
		LOG_DBG("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM
	scenario_id, UINT32 *framerate)
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
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
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
	LOG_INF("Exit.enable:%d\n", enable);

	return ERROR_NONE;
}

static void get_crop_info(UINT32 feature,
	struct SENSOR_WINSIZE_INFO_STRUCT *win_info)
{
	switch (feature) {
	case MSDK_SCENARIO_ID_CUSTOM1:
		/* arry 5 is custom1 setting */
		memcpy((void *)win_info,
			(void *)&imgsensor_winsize_info[5],
			sizeof(imgsensor_winsize_info[5]));
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		/* arry 1 is capture setting */
		memcpy((void *)win_info,
			(void *)&imgsensor_winsize_info[1],
			sizeof(imgsensor_winsize_info[1]));
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		/* arry 2 is video setting */
		memcpy((void *)win_info,
			(void *)&imgsensor_winsize_info[2],
			sizeof(imgsensor_winsize_info[2]));
		break;
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	default:
		/* arry 0 is preview setting */
		memcpy((void *)win_info,
			(void *)&imgsensor_winsize_info[0],
			sizeof(imgsensor_winsize_info[0]));
		break;
	}

}

static void get_mipi_pixel_rate(unsigned long long *feature_data)
{
	switch (*feature_data) {
	case MSDK_SCENARIO_ID_CUSTOM1:
		*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
			imgsensor_info.custom1.mipi_pixel_rate;
		break;
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
}

static void get_pixel_rate(unsigned long long *feature_data)
{
	switch (*feature_data) {
	case MSDK_SCENARIO_ID_CUSTOM1:
		if (imgsensor_info.custom1.linelength == 0)
			return;
		if (imgsensor_info.custom1.linelength >
			IMGSENSOR_LINGLENGTH_GAP)
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.custom1.pclk /
				(imgsensor_info.custom1.linelength -
				IMGSENSOR_LINGLENGTH_GAP))*
				imgsensor_info.custom1.grabwindow_width;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor_info.cap.linelength == 0)
			return;
		if (imgsensor_info.cap.linelength >
			IMGSENSOR_LINGLENGTH_GAP)
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.cap.pclk /
				(imgsensor_info.cap.linelength -
				IMGSENSOR_LINGLENGTH_GAP))*
				imgsensor_info.cap.grabwindow_width;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (imgsensor_info.normal_video.linelength == 0)
			return;
		if (imgsensor_info.normal_video.linelength >
			IMGSENSOR_LINGLENGTH_GAP)
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.normal_video.pclk /
				(imgsensor_info.normal_video.linelength -
				IMGSENSOR_LINGLENGTH_GAP))*
				imgsensor_info.normal_video.grabwindow_width;
		break;
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	default:
		if (imgsensor_info.pre.linelength == 0)
			return;
		if (imgsensor_info.pre.linelength >
			IMGSENSOR_LINGLENGTH_GAP)
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.pre.pclk /
				(imgsensor_info.pre.linelength -
				IMGSENSOR_LINGLENGTH_GAP))*
				imgsensor_info.pre.grabwindow_width;
		break;
	}

}

static kal_uint32 feature_control_hi1336_qtech(
	MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *)feature_para;
	UINT16 *feature_data_16 = (UINT16 *)feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *)feature_para;
	UINT32 *feature_data_32 = (UINT32 *)feature_para;
	struct SET_PD_BLOCK_INFO_T *PDAF_info = NULL;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	struct SENSOR_WINSIZE_INFO_STRUCT *win_info = NULL;

	if (feature_para == NULL || feature_para_len == NULL) {
		LOG_ERR("null ptr input params\n");
		return ERROR_INVALID_PARA;
	}
	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data_32);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain(*feature_data_16);
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data_16);
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
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		// for factory mode auto testing
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data_16;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		win_info = (struct SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t)(*(feature_data + 1));
		get_crop_info(*feature_data_32, win_info);
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		(void)streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter(*feature_data_32);
		(void)streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_INF("PDAF SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			*feature_data_32);
		PDAF_info = (struct SET_PD_BLOCK_INFO_T *)
			(uintptr_t)(*(feature_data + 1));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)PDAF_info, (void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		default:
			break;
		}
		break;

	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		default:
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT32)*feature_data,
			(UINT32)*(feature_data + 1));
		break;
	case SENSOR_HUAWEI_FEATURE_DUMP_REG:
		sensor_dump_reg();
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		get_mipi_pixel_rate(feature_data);
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
		get_pixel_rate(feature_data);
		break;
	default:
		LOG_INF("Not support the feature_id:%d\n", feature_id);
		break;
	}

	return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control_hi1336_qtech,
	control,
	close
};

UINT32 HI1336_QTECH_SensorInit(struct SENSOR_FUNCTION_STRUCT **pf_func)
{
	if (pf_func != NULL)
		*pf_func = &sensor_func;
	return ERROR_NONE;
}
