/*
 *  Copyright (C) 2018 Huawei Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "imgsensor_sensor_common.h"
#include "imgsensor_sensor_i2c.h"
#include "imx258_sunny_sensor.h"

static DEFINE_SPINLOCK(imgsensor_drv_lock);

/****************************Modify Following Strings for Debug****************************/
#define PFX "[imx258_sunny]"
#define DEBUG_IMX258_SUNNY 1
#define LOG_DBG(fmt, args...)    if (DEBUG_IMX258_SUNNY) pr_debug(PFX "%s %d " fmt, __func__,__LINE__, ##args)
#define LOG_INF(fmt, args...)    pr_info(PFX "%s %d " fmt, __func__,__LINE__, ##args)
#define LOG_ERR(fmt, args...)    pr_err(PFX "%s %d " fmt, __func__,__LINE__, ##args)
/****************************   Modify end    *******************************************/

static kal_uint16 imx258_type;
static kal_uint16 pdaf_mode;
/*VC1 None , VC2 for PDAF(DT=0X36), unit : 8bit*/
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3] = {
	/* Preview mode setting */
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	0x00, 0x2b, 0x0834, 0x0618, 0x00, 0x35, 0x0280, 0x0001,
	0x00, 0x2f, 0x0000, 0x0000, 0x03, 0x00, 0x0000, 0x0000},
	/* Capture mode setting */
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	0x00, 0x2b, 0x1070, 0x0C30, 0x00, 0x35, 0x0280, 0x0001,
	0x00, 0x2f, 0x00A0, 0x0a80, 0x03, 0x00, 0x0000, 0x0000},
	/* Video mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	0x00, 0x2b, 0x1070, 0x0C30, 0x01, 0x00, 0x0000, 0x0000,
	0x00, 0x2f, 0x00A0, 0x0a80, 0x03, 0x00, 0x0000, 0x0000}
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 24,
	.i4OffsetY = 24,
	.i4PitchX = 32,
	.i4PitchY = 32,
	.i4PairNum = 8,
	.i4SubBlkW = 8,
	.i4SubBlkH = 16,
	.i4PosL = {{25, 30}, {33, 30}, {41, 30}, {49, 30},
		   {29, 46}, {37, 46}, {45, 46}, {53, 46} },
	.i4PosR = {{25, 34}, {33, 34}, {41, 34}, {49, 34},
		   {29, 50}, {37, 50}, {45, 50}, {53, 50} },

	/* 0:IMAGE_NORMAL,1:IMAGE_H_MIRROR,2:IMAGE_V_MIRROR,3:IMAGE_HV_MIRROR */
	.iMirrorFlip = 0,
	.i4BlockNumX = 130,
	.i4BlockNumY = 96,
};

static void set_dummy(void)
{
	kal_int32 rc = ERROR_NONE;
	LOG_DBG("ENTER\n");

	rc = imgsensor_sensor_i2c_write(&imgsensor, SENSOR_GRP_PARAM_HOLD,
		0x01, IMGSENSOR_I2C_BYTE_DATA);
	if ( rc < 0 ){
		LOG_ERR("write group param hold failed");
		return;
	}
	rc = imgsensor_sensor_i2c_write(&imgsensor, SENSOR_FRM_LENGTH_LINES_H,
		imgsensor.frame_length & 0xFFFF, IMGSENSOR_I2C_WORD_DATA);
	if ( rc < 0 ){
		LOG_ERR("write frame_length failed, frame_length:0x%x.\n",
			imgsensor.frame_length);
		return;
	}
	rc = imgsensor_sensor_i2c_write(&imgsensor,SENSOR_LINE_LENGHT_PCK_H,
		imgsensor.line_length & 0xFFFF,IMGSENSOR_I2C_WORD_DATA);
	if ( rc < 0 ){
		LOG_ERR("write line_length failed, line_length:0x%x.\n",
			imgsensor.frame_length);
		return;
	}
	rc = imgsensor_sensor_i2c_write(&imgsensor, SENSOR_GRP_PARAM_HOLD,
		0x00, IMGSENSOR_I2C_BYTE_DATA);
	if ( rc < 0 ){
		LOG_ERR("write group param hold failed");
		return;
	}
	return;
}   /*  set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	kal_int32 rc = 0;
	kal_uint16 sensor_id = 0;
	kal_uint16 tmp_reg_value = 0;

	if(imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SENSOR_CTRL_REG_L,0x0F, IMGSENSOR_I2C_BYTE_DATA) == 0)
	{
		imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SENSOR_CTRL_REG_H,0x01, IMGSENSOR_I2C_BYTE_DATA);
		imgsensor_sensor_i2c_read(&imgsensor, SENSOR_SENSOR_CTRL_REG_M,
			&tmp_reg_value, IMGSENSOR_I2C_BYTE_DATA);
		if(tmp_reg_value == 0x01)
		{
			rc =  imgsensor_sensor_i2c_read(&imgsensor, SENSOR_SENSOR_TYPE,
				&imx258_type, IMGSENSOR_I2C_BYTE_DATA);
			if( rc < 0 ){
				LOG_ERR("Read id failed.id reg: 0x%x\n", SENSOR_SENSOR_TYPE);
				imx258_type = IMX258_SUNNY_NOPDAF_TYPE;
			}
			if(imx258_type != IMX258_SUNNY_HDD_TYPE)
			{
				imx258_type = IMX258_SUNNY_NOPDAF_TYPE;
			}
			rc = imgsensor_sensor_i2c_read(&imgsensor, imgsensor_info.sensor_id_reg,
				&sensor_id, IMGSENSOR_I2C_WORD_DATA);
			if( rc < 0 ){
				LOG_ERR("Read id failed.id reg: 0x%x\n", imgsensor_info.sensor_id_reg);
				sensor_id = 0xFFFF;
			}
			LOG_INF("###sensor id =0x%x",sensor_id);
			sensor_id = (sensor_id>>4);
			LOG_INF("###sensor id =0x%x",sensor_id);
		}
	}

	LOG_INF("####id=0x%x,pdaf type=0x%0x",sensor_id,imx258_type);
	return sensor_id;
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_DBG("framerate = %d, min framelength should enable %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
		(frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;

	imgsensor.dummy_line =
		imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;

		imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}				/*      set_max_framerate  */

static void set_shutter_common(UINT32 shutter, kal_uint16 frm_lenght_ctl_mode)
{
	unsigned char shift = 0;
	kal_uint16 realtime_fps;

	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;

	while (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)){
		shift++;
		if (shift > SENSOR_MAX_SHIFT_NUM){
			shutter = 0xFFFF - imgsensor_info.margin;
			shift = SENSOR_MAX_SHIFT_NUM;
			break;
		}
		shutter >>= 1;
	 }

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk
			/ imgsensor.line_length * 10 / imgsensor.frame_length;

		/* calc fps between 298~305, real fps set to 298*/
		if (realtime_fps >= 298 && realtime_fps <= 305)
			set_max_framerate(298, 0);
		/* calc fps between 147~150, real fps set to 146*/
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_GRP_PARAM_HOLD,
					0x01, IMGSENSOR_I2C_BYTE_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_FRM_LENGTH_LINES_H,
					imgsensor.frame_length & 0xFFFF, IMGSENSOR_I2C_WORD_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_GRP_PARAM_HOLD,
					0x00, IMGSENSOR_I2C_BYTE_DATA);
		}
	} else {
		/* Extend frame length */
			(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_GRP_PARAM_HOLD,
					0x01, IMGSENSOR_I2C_BYTE_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_FRM_LENGTH_LINES_H,
					imgsensor.frame_length & 0xFFFF, IMGSENSOR_I2C_WORD_DATA);
			(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_GRP_PARAM_HOLD,
					0x00, IMGSENSOR_I2C_BYTE_DATA);
	}

	/* Update Shutter */
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_GRP_PARAM_HOLD,
			0x01, IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_LONG_EXPO_REG,
			shift & 0xFF, IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_FRM_LENGHT_CTL_REG,
			frm_lenght_ctl_mode & 0xFF, IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_COARSE_INTEG_TIME_H,
			shutter & 0xFFFF, IMGSENSOR_I2C_WORD_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_GRP_PARAM_HOLD,
			0x00, IMGSENSOR_I2C_BYTE_DATA);
	LOG_DBG("Exit! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
}

static void set_shutter(UINT32 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	LOG_DBG("Enter.shutter = %d\n", shutter);
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	set_shutter_common(shutter, 0x01);
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

	set_shutter_common(shutter, 0x00);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint8 i;

	for (i = 0; i < IMX258MIPI_MaxGainIndex; i++) {
		if (gain <= IMX258MIPI_sensorGainMapping[i][0])
			break;
	}
	if( i >= IMX258MIPI_MaxGainIndex)
		i = IMX258MIPI_MaxGainIndex - 1;
	if (gain != IMX258MIPI_sensorGainMapping[i][0])
		LOG_DBG("Gain mapping don't correctly:%d %d\n", gain,
			IMX258MIPI_sensorGainMapping[i][0]);
	return IMX258MIPI_sensorGainMapping[i][1];
}

/*************************************************************************
 * FUNCTION
 *	set_gain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	/* 0x350A[0:1], 0x350B[0:7] AGC real gain */
	/* [0:3] = N meams N /16 X        */
	/* [4:9] = M meams M X             */
	/* Total gain = M + N /16 X   */

	/*  */
	if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
		LOG_DBG("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 16 * BASEGAIN)
			gain = 16 * BASEGAIN;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_DBG("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_GRP_PARAM_HOLD,
				0x01, IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_ANA_GAIN_GLOBAL_H,
				reg_gain & 0xFFFF, IMGSENSOR_I2C_WORD_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_GRP_PARAM_HOLD,
				0x00, IMGSENSOR_I2C_BYTE_DATA);
	return gain;
}				/*      set_gain  */

static kal_uint32 sensor_init(void)
{
	kal_int32 rc = 0;
	LOG_DBG("ENTER.\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.init_setting);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_DBG("EXIT.\n");

	return ERROR_NONE;
}

static kal_uint32 set_preview_setting(void)
{
	kal_int32 rc = 0;
	LOG_DBG("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.pre_setting);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_DBG("EXIT.\n");

	return ERROR_NONE;
}   /*  preview_setting  */

static kal_uint32 set_capture_setting(kal_uint16 currefps)
{
	kal_int32 rc = 0;
	LOG_DBG("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.cap_setting);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_DBG("EXIT.\n");

	return ERROR_NONE;
}

static kal_uint32 set_normal_video_setting(kal_uint16 currefps)
{
	kal_int32 rc = 0;
	LOG_DBG("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.normal_video_setting);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	LOG_DBG("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_hs_video_setting(kal_uint16 currefps)
{
	kal_int32 rc = 0;
	LOG_DBG("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.hs_video_setting);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	LOG_DBG("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_ls_video_setting(kal_uint16 currefps)
{
	kal_int32 rc = 0;
	LOG_DBG("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.slim_setting);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	LOG_DBG("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_custom1_setting(kal_uint16 currefps)
{
	kal_int32 rc = 0;
	LOG_DBG("ENTER\n");

	rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.custom1_setting);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	LOG_DBG("EXIT.\n");

	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 *	get_imgsensor_id
 *
 * DESCRIPTION
 *	This function get the sensor ID
 *
 * PARAMETERS
 *	*sensorID : return the sensor ID
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
/* #define SLT_DEVINFO_CMM */
#define  RETRY_TIMES 2
#ifdef SLT_DEVINFO_CMM
#include  <linux/dev_info.h>
static struct devinfo_struct *s_DEVINFO_ccm;	/* suppose 10 max lcm device */
#endif

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = RETRY_TIMES; /* retry 2 time */
	kal_uint16 tmp_sensor_id = 0;

	spin_lock(&imgsensor_drv_lock);
	/*init i2c config*/
	imgsensor.i2c_speed = imgsensor_info.i2c_speed;
	imgsensor.addr_type = imgsensor_info.addr_type;
	spin_unlock(&imgsensor_drv_lock);

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			tmp_sensor_id = return_sensor_id();
			/* check lens type */
			*sensor_id = imgsensor_convert_sensor_id(imgsensor_info.sensor_id,
				tmp_sensor_id, EEPROM_LENS_TYPE_ADDR, 0);
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("id reg: 0x%x, read id: 0x%x, expect id:0x%x.\n",
					imgsensor.i2c_write_id, *sensor_id, imgsensor_info.sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Check sensor id fail, id reg: 0x%x,read id: 0x%x, expect id:0x%x.\n",
				imgsensor.i2c_write_id, *sensor_id, imgsensor_info.sensor_id);
			retry--;
		} while(retry > 0);
		i++;
		retry = RETRY_TIMES;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/*if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF*/
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 *	open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{
	kal_uint32 sensor_id = 0;
	kal_uint32 rc = ERROR_NONE;
	LOG_INF("ENTER\n");

	rc = get_imgsensor_id(&sensor_id);
	if( rc != ERROR_NONE ){
		LOG_ERR("probe sensor failed.\n");
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	LOG_DBG("sensor probe successfully. sensor_id=0x%x.\n", sensor_id);

	/* initail sequence write in  */
	sensor_init();

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en   = KAL_FALSE;
	imgsensor.sensor_mode      = IMGSENSOR_MODE_INIT;
	imgsensor.shutter          = SENSOR_SHUTTER_INIT;
	imgsensor.gain             = SENSOR_GAIN_DEFAULT;	/* 0x100; */
	imgsensor.pclk             = imgsensor_info.pre.pclk;
	imgsensor.frame_length     = imgsensor_info.pre.framelength;
	imgsensor.line_length      = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel      = 0;
	imgsensor.dummy_line       = 0;
	imgsensor.test_pattern     = KAL_FALSE;
	imgsensor.current_fps      = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}				/*      open  */

/*************************************************************************
 * FUNCTION
 *	close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");

	/*No Need to implement this function */

	return ERROR_NONE;
}				/*      close  */

/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	rc = set_preview_setting();
	if( rc != ERROR_NONE ){
		LOG_ERR("preview setting failed.\n");
		return rc;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}   /*  preview   */
/*************************************************************************
 * FUNCTION
 *	capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;
	LOG_INF("ENTER\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	rc = set_capture_setting(imgsensor.current_fps);
	if( rc != ERROR_NONE ){
		LOG_ERR("capture setting failed.\n");
		return rc;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}   /* capture() */			/* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;
	LOG_INF("ENTER\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	rc = set_normal_video_setting(imgsensor.current_fps);
	if( rc != ERROR_NONE ){
		LOG_ERR("normal video setting failed.\n");
		return rc;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}   /*  normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;
	LOG_INF("ENTER\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	rc = set_hs_video_setting(imgsensor.current_fps);
	if( rc != ERROR_NONE ){
		LOG_ERR("hs_video setting failed.\n");
		return rc;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}   /*  normal_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;
	LOG_INF("ENTER\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	rc = set_ls_video_setting(imgsensor.current_fps);
	if( rc != ERROR_NONE ){
		LOG_ERR("slim_video setting failed.\n");
		return rc;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}   /*  normal_video   */

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;
	LOG_INF("ENTER\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	rc = set_custom1_setting(imgsensor.current_fps);
	if( rc != ERROR_NONE ){
		LOG_ERR("custom1 setting failed.\n");
		return rc;
	}
	LOG_INF("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	if(sensor_resolution != NULL)
	{
		sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
		sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

		sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
		sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

		sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
		sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;
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
}   /*  get_resolution  */
static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	if( !sensor_info || !sensor_config_data ){
		LOG_ERR("Fatal: NULL ptr. sensor_info:%pK, sensor_config_data:%pK.\n",
			sensor_info, sensor_config_data);
		return ERROR_INVALID_PARA;
	}
	LOG_DBG("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* not use */
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* inverse with datasheet */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = FALSE;	/* not use */
	sensor_info->SensorResetDelayCount = 5;	/* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;

	sensor_info->SettleDelayMode =
		imgsensor_info.mipi_settle_delay_mode;

	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;

	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;

	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;

	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;

	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;

	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;

	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->TEMPERATURE_SUPPORT = 1;

	if (imx258_type == IMX258_SUNNY_HDD_TYPE)
		sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV;
	else
		sensor_info->PDAF_Support = PDAF_SUPPORT_NA;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;	/* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;	/* not use */
	sensor_info->SensorPixelClockCount = 3;	/* not use */
	sensor_info->SensorDataLatchCount = 2;	/* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
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
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}				/*      get_info  */

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc = ERROR_NONE;

	LOG_DBG("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		rc = preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		rc = capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		rc = normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		rc = hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		rc = slim_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		rc = custom1(image_window, sensor_config_data);
		break;
	default:
		LOG_ERR("Error ScenarioId setting. scenario_id:%d.\n", scenario_id);
		rc = preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return rc;
}/*control()*/

static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
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
	LOG_DBG("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, UINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_DBG("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		if(framerate == 0 || imgsensor_info.pre.linelength == 0){
			return ERROR_NONE;
		}
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);

		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.pre.framelength)
		? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if(framerate == 0 || imgsensor_info.normal_video.linelength == 0){
			return ERROR_NONE;
		}
		frame_length = imgsensor_info.normal_video.pclk
			/ framerate * 10 / imgsensor_info.normal_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.normal_video.framelength)
		? (frame_length - imgsensor_info.normal_video.framelength) : 0;

		imgsensor.frame_length =
		 imgsensor_info.normal_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		if(framerate == 0 || imgsensor_info.hs_video.linelength == 0){
			return ERROR_NONE;
		}
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
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		if(framerate == 0 || imgsensor_info.slim_video.linelength == 0){
			return ERROR_NONE;
		}
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
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		if (framerate == 0 || imgsensor_info.custom1.linelength == 0)
			return ERROR_NONE;

		frame_length = imgsensor_info.custom1.pclk /
			framerate * 10 / imgsensor_info.custom1.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom1.framelength) ?
			(frame_length - imgsensor_info.custom1.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.custom1.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if(framerate == 0 || imgsensor_info.cap.linelength == 0){
			return ERROR_NONE;
		}

		frame_length = imgsensor_info.cap.pclk /
			framerate * 10 / imgsensor_info.cap.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.cap.framelength)
		? (frame_length - imgsensor_info.cap.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.cap.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:		/* coding with  preview scenario by default */
		if(framerate == 0 || imgsensor_info.pre.linelength == 0){
			return ERROR_NONE;
		}
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.pre.framelength)
		? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		LOG_DBG("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, UINT32 *framerate)
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
	kal_int32 rc = ERROR_NONE;

	LOG_DBG("ENTER.enable:%d\n", enable);
	if (enable) {
		rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.test_pattern_on_setting);
	} else {
		rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.test_pattern_off_setting);
	}
	if(rc < 0){
		LOG_ERR("enable:%d failed\n", enable);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	LOG_DBG("EXIT\n");
	return ERROR_NONE;
}
/*************************************************************************
* FUNCTION
*   sensor_dump_reg
*
* DESCRIPTION
*   This function dump some sensor reg
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 sensor_dump_reg(void)
{
	kal_int32 rc = 0;
	rc = imgsensor_sensor_i2c_process(&imgsensor, &imgsensor_info.dump_info);
	if( rc < 0 ){
		LOG_ERR("Failed.\n");
	}
	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	kal_int32 rc = 0;
	LOG_INF("Enter.enable:%d\n", enable);
	if (enable){
		rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.streamon_setting);
	} else {
		rc = imgsensor_sensor_write_setting(&imgsensor, &imgsensor_info.streamoff_setting);
	}
	if ( rc < 0 ){
		LOG_ERR("Failed enable:%d.\n", enable);
		return ERROR_SENSOR_POWER_ON_FAIL;
	}
	LOG_INF("Exit.enable:%d\n", enable);

	return ERROR_NONE;
}

static kal_uint32 feature_control_imx258_sunny(MSDK_SENSOR_FEATURE_ENUM feature_id,
					UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	struct SET_PD_BLOCK_INFO_T *PDAFinfo = 0;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo = 0;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo = 0;

	if (feature_para == NULL || feature_para_len == NULL) {
		LOG_ERR("null ptr input params\n");
		return ERROR_INVALID_PARA;
	}

	LOG_DBG("feature_id = %d.\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4; /* return 4 byte data */
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;  /* return 4 byte data */
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter((UINT32) *feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		(void)set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode(
			(BOOL) * feature_data_16, *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *feature_data,
					(UINT32)*(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *feature_data,
			(UINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		(void)set_test_pattern_mode((BOOL)*feature_data);
		break;
	/* for factory mode auto testing */
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4; /* return 4 byte data */
		break;

	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_DBG("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_DBG("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32) *feature_data);
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[5],
				sizeof(imgsensor_winsize_info[5]));
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			/*imgsensor_winsize_info arry 1 is capture setting*/
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			/*imgsensor_winsize_info arry 2 is preview setting*/
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			/*imgsensor_winsize_info arry 0 is preivew setting*/
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_DBG("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%llu\n",
			*feature_data);
		PDAFinfo =
			(struct SET_PD_BLOCK_INFO_T *) (uintptr_t) (*(feature_data + 1));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info,sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		LOG_DBG("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16) *feature_data);
		pvcinfo =
			(struct SENSOR_VC_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[1],sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[2],sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[0],sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_DBG(
			"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%llu\n",
			*feature_data);
		/* PDAF capacity enable or not, 2p8 only full size support PDAF */
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(UINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(UINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_PDAF:
		LOG_DBG("PDAF mode :%d\n", *feature_data_16);
		pdaf_mode = *feature_data_16;
		break;
	case SENSOR_FEATURE_SET_PDAF_TYPE:
		imx258_type = IMX258_SUNNY_HDD_TYPE;
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT32)*feature_data,
			(UINT32)*(feature_data + 1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_DBG("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		(void)streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_DBG("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0){
			set_shutter((UINT32) *feature_data);
		}
		(void)streaming_control(KAL_TRUE);
		break;
	case SENSOR_HUAWEI_FEATURE_DUMP_REG:
		sensor_dump_reg();
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:

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
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM1:
			if (imgsensor_info.custom1.linelength >
				IMGSENSOR_LINGLENGTH_GAP)
				*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.custom1.pclk /
					(imgsensor_info.custom1.linelength -
					IMGSENSOR_LINGLENGTH_GAP))*
					imgsensor_info.custom1.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			if(imgsensor_info.cap.linelength > IMGSENSOR_LINGLENGTH_GAP){
				*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.cap.pclk /
					(imgsensor_info.cap.linelength - IMGSENSOR_LINGLENGTH_GAP))*
					imgsensor_info.cap.grabwindow_width;
			}
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if(imgsensor_info.normal_video.linelength > IMGSENSOR_LINGLENGTH_GAP){
				*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.normal_video.pclk /
					(imgsensor_info.normal_video.linelength - IMGSENSOR_LINGLENGTH_GAP))*
					imgsensor_info.normal_video.grabwindow_width;
			}
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			if(imgsensor_info.pre.linelength > IMGSENSOR_LINGLENGTH_GAP){
				*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.pre.pclk /
					(imgsensor_info.pre.linelength - IMGSENSOR_LINGLENGTH_GAP))*
					imgsensor_info.pre.grabwindow_width;
			}
			break;
		}
		break;
	case SENSOR_FEATURE_GET_MIPI_TRAIL_VAL:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom1.mipi_trail_val;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap.mipi_trail_val;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_trail_val;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_trail_val;
			break;
		}
		break;
	default:
		break;
	}

	return ERROR_NONE;
}				/*      feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control_imx258_sunny,
	control,
	close
};

UINT32 IMX258_SUNNY_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
