/*
 * gc5035p_qunhui_mipi_sensor.h
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * gc5035 qunhui image sensor config settings
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
#ifndef __GC5035P_QUNHUI_MIPI_SENSOR_H__
#define __GC5035P_QUNHUI_MIPI_SENSOR_H__

/* SENSOR MIRROR FLIP INFO */
#define GC5035P_QUNHUI_MIRROR_FLIP_ENABLE         0
#if GC5035P_QUNHUI_MIRROR_FLIP_ENABLE
#define GC5035P_QUNHUI_MIRROR                     0x83
#define GC5035P_QUNHUI_RSTDUMMY1                  0x03
#define GC5035P_QUNHUI_RSTDUMMY2                  0xfc
#else
#define GC5035P_QUNHUI_MIRROR                     0x80
#define GC5035P_QUNHUI_RSTDUMMY1                  0x02
#define GC5035P_QUNHUI_RSTDUMMY2                  0x7c
#endif

/* SENSOR PRIVATE INFO FOR GAIN SETTING */
#define GC5035P_QUNHUI_SENSOR_GAIN_BASE             256
#define GC5035P_QUNHUI_SENSOR_GAIN_MAX              (16 * GC5035P_QUNHUI_SENSOR_GAIN_BASE)
#define GC5035P_QUNHUI_SENSOR_GAIN_MAX_VALID_INDEX  17
#define GC5035P_QUNHUI_SENSOR_GAIN_MAP_SIZE         17
#define GC5035P_QUNHUI_SENSOR_DGAIN_BASE            0x100

/* SENSOR PRIVATE INFO FOR OTP SETTINGS */
#define GC5035P_QUNHUI_OTP_FOR_CUSTOMER            1
#define GC5035P_QUNHUI_OTP_DEBUG                   0

/* DEBUG */
#if GC5035P_QUNHUI_OTP_DEBUG
#define GC5035P_QUNHUI_OTP_START_ADDR              0x0
#endif

#define GC5035P_QUNHUI_OTP_DATA_LENGTH             1024

/* OTP FLAG TYPE */
#define GC5035P_QUNHUI_OTP_FLAG_EMPTY              0x00
#define GC5035P_QUNHUI_OTP_FLAG_VALID              0x01
#define GC5035P_QUNHUI_OTP_FLAG_INVALID            0x02
#define GC5035P_QUNHUI_OTP_FLAG_INVALID2           0x03
#define gc5035p_qunhui_otp_get_offset(size)           (size << 3)
#define gc5035p_qunhui_otp_get_2bit_flag(flag, bit)   ((flag >> bit) & 0x03)
#define gc5035p_qunhui_otp_check_1bit_flag(flag, bit) (((flag >> bit) & 0x01) == GC5035P_QUNHUI_OTP_FLAG_VALID)

/* 0~4:af_data 5:af_flag 6~25:sn_data 26:sn_flag */
#define GC5035P_QUNHUI_OTP_BUF_SIZE                27

#define GC5035P_QUNHUI_OTP_ID_SIZE                 9
#define GC5035P_QUNHUI_OTP_ID_DATA_OFFSET          0x0020

/* OTP DPC PARAMETERS */
#define GC5035P_QUNHUI_OTP_DPC_FLAG_OFFSET         0x0068
#define GC5035P_QUNHUI_OTP_DPC_TOTAL_NUMBER_OFFSET 0x0070
#define GC5035P_QUNHUI_OTP_DPC_ERROR_NUMBER_OFFSET 0x0078

/* OTP REGISTER UPDATE PARAMETERS */
#define GC5035P_QUNHUI_OTP_REG_FLAG_OFFSET         0x0880
#define GC5035P_QUNHUI_OTP_REG_DATA_OFFSET         0x0888
#define GC5035P_QUNHUI_OTP_REG_MAX_GROUP           5
#define GC5035P_QUNHUI_OTP_REG_BYTE_PER_GROUP      5
#define GC5035P_QUNHUI_OTP_REG_REG_PER_GROUP       2
#define GC5035P_QUNHUI_OTP_REG_BYTE_PER_REG        2
#define GC5035P_QUNHUI_OTP_REG_DATA_SIZE           (GC5035P_QUNHUI_OTP_REG_MAX_GROUP * GC5035P_QUNHUI_OTP_REG_BYTE_PER_GROUP)
#define GC5035P_QUNHUI_OTP_REG_REG_SIZE            (GC5035P_QUNHUI_OTP_REG_MAX_GROUP * GC5035P_QUNHUI_OTP_REG_REG_PER_GROUP)

#if GC5035P_QUNHUI_OTP_FOR_CUSTOMER
#define GC5035P_QUNHUI_OTP_CHECK_SUM_BYTE          1
#define GC5035P_QUNHUI_OTP_GROUP_CNT               2

/* OTP MODULE INFO PARAMETERS */
#define GC5035P_QUNHUI_OTP_MODULE_FLAG_OFFSET          0x1ef0
#define GC5035P_QUNHUI_OTP_MODULE_DATA_GROUP1_OFFSET   0x1ef8
#define GC5035P_QUNHUI_OTP_MODULE_DATA_GROUP2_OFFSET   0x1f38
#define GC5035P_QUNHUI_OTP_MODULE_DATA_SIZE            8

/* OTP WB PARAMETERS */
#define GC5035P_QUNHUI_OTP_WB_FLAG_OFFSET              0x1f78
#define GC5035P_QUNHUI_OTP_WB_DATA_GROUP1_OFFSET       0x1f80
#define GC5035P_QUNHUI_OTP_WB_DATA_GROUP2_OFFSET       0x1fa0
#define GC5035P_QUNHUI_OTP_WB_DATA_SIZE                4
#define GC5035P_QUNHUI_OTP_GOLDEN_DATA_GROUP1_OFFSET   0x1fc0
#define GC5035P_QUNHUI_OTP_GOLDEN_DATA_GROUP2_OFFSET   0x1fe0
#define GC5035P_QUNHUI_OTP_GOLDEN_DATA_SIZE            4
#define GC5035P_QUNHUI_OTP_WB_CAL_BASE                 0x0800
#define GC5035P_QUNHUI_OTP_WB_GAIN_BASE                0x0400

/* WB TYPICAL VALUE 0.5 */
#define GC5035P_QUNHUI_OTP_WB_RG_TYPICAL           0x0400
#define GC5035P_QUNHUI_OTP_WB_BG_TYPICAL           0x0400

/*OTP SN PARAMETERS */
#define GC5035P_QUNHUI_OTP_SN_FLAG_OFFSET          0x1c90
#define GC5035P_QUNHUI_OTP_SN_DATA_GROUP1_OFFSET   0x1c98
#define GC5035P_QUNHUI_OTP_SN_DATA_GROUP2_OFFSET   0x1d90
#define GC5035P_QUNHUI_OTP_SN_DATA_SIZE            31
#define GC5035P_QUNHUI_OTP_SN_VALID_DATA_SIZE      20

/*OTP AF PARAMETERS */
#define GC5035P_QUNHUI_OTP_AF_FLAG_OFFSET          0x1e88
#define GC5035P_QUNHUI_OTP_AF_DATA_GROUP1_OFFSET   0x1e90
#define GC5035P_QUNHUI_OTP_AF_DATA_GROUP2_OFFSET   0x1ec0
#define GC5035P_QUNHUI_OTP_AF_DATA_SIZE            6
#endif

enum {
	OTP_CLOSE = 0,
	OTP_OPEN,
};

/* DPC STRUCTURE */
struct gc5035p_qunhui_dpc_t {
	kal_uint8 flag;
	kal_uint16 total_num;
};

/* REGISTER UPDATE STRUCTURE */
struct gc5035p_qunhui_reg_t {
	kal_uint8 page;
	kal_uint8 addr;
	kal_uint8 value;
};

/* REGISTER UPDATE STRUCTURE */
struct gc5035p_qunhui_reg_update_t {
	kal_uint8 flag;
	kal_uint8 cnt;
	struct gc5035p_qunhui_reg_t reg[GC5035P_QUNHUI_OTP_REG_REG_SIZE];
};

/* SN info STRUCTURE */
struct gc5035p_qunhui_sn_info_t {
	kal_uint8 data[GC5035P_QUNHUI_OTP_SN_VALID_DATA_SIZE + 1];
};

/* AF info STRUCTURE */
struct gc5035p_qunhui_af_info_t {
	kal_uint8 direction;
	kal_uint16 macro;
	kal_uint16 infinity;
};

#if GC5035P_QUNHUI_OTP_FOR_CUSTOMER
/* MODULE INFO STRUCTURE */
struct gc5035p_qunhui_module_info_t {
	kal_uint8 module_id;
	kal_uint8 lens_id;
	kal_uint8 reserved0;
	kal_uint8 reserved1;
	kal_uint8 year;
	kal_uint8 month;
	kal_uint8 day;
};

/* WB STRUCTURE */
struct gc5035p_qunhui_wb_t {
	kal_uint8  flag;
	kal_uint16 rg;
	kal_uint16 bg;
};

/* OTP STRUCTURE */
struct gc5035p_qunhui_otp_t {
	kal_uint8 otp_id[GC5035P_QUNHUI_OTP_ID_SIZE];
	kal_uint8 otp_buf[GC5035P_QUNHUI_OTP_BUF_SIZE];
	struct gc5035p_qunhui_dpc_t dpc;
	struct gc5035p_qunhui_reg_update_t regs;
	struct gc5035p_qunhui_wb_t wb;
	struct gc5035p_qunhui_wb_t golden;
	struct gc5035p_qunhui_sn_info_t sn;
	struct gc5035p_qunhui_af_info_t af;
};
#endif

enum {
	IMGSENSOR_MODE_INIT,
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
};

struct imgsensor_mode_struct {
	kal_uint32 pclk; /* record different mode's pclk */
	kal_uint32 linelength; /* record different mode's linelength */
	kal_uint32 framelength; /* record different mode's framelength */
	kal_uint8  startx; /* record different mode's startx of grabwindow */
	kal_uint8  starty; /* record different mode's startx of grabwindow */
	kal_uint16 grabwindow_width; /* record different mode's width of grabwindow */
	kal_uint16 grabwindow_height; /* record different mode's height of grabwindow */
	kal_uint32 mipi_pixel_rate;
	/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario */
	kal_uint8  mipi_data_lp2hs_settle_dc;
	/* following for GetDefaultFramerateByScenario() */
	kal_uint16 max_framerate;
};

/* SENSOR PRIVATE STRUCT FOR VARIABLES */
struct imgsensor_struct {
	kal_uint8  mirror; /* mirrorflip information */
	kal_uint8  sensor_mode; /* record IMGSENSOR_MODE enum value */
	kal_uint32 shutter; /* current shutter */
	kal_uint16 gain; /* current gain */
	kal_uint32 pclk; /* current pclk */
	kal_uint32 frame_length; /* current framelength */
	kal_uint32 line_length; /* current linelength */
	kal_uint32 min_frame_length; /* current min  framelength to max framerate */
	kal_uint16 dummy_pixel; /* current dummypixel */
	kal_uint16 dummy_line; /* current dummline */
	kal_uint16 current_fps; /* current max fps */
	kal_bool   autoflicker_en; /* record autoflicker enable or disable */
	kal_bool   test_pattern; /* record test pattern mode or not */
	enum MSDK_SCENARIO_ID_ENUM current_scenario_id; /* current scenario id */
	kal_bool   ihdr_en; /* ihdr enable or disable */
	kal_uint8  i2c_write_id; /* record current sensor's i2c write id */
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT */
struct imgsensor_info_struct {
	kal_uint32 sensor_id; /* record sensor id defined in Kd_imgsensor.h */
	kal_uint32 checksum_value; /* checksum value for Camera Auto Test */
	struct imgsensor_mode_struct pre; /* preview scenario relative information */
	struct imgsensor_mode_struct cap; /* capture scenario relative information */
	struct imgsensor_mode_struct cap1; /* capture for PIP 24fps relative information */
	/* capture1 mode must use same framelength, linelength with Capture mode for shutter calculate */
	struct imgsensor_mode_struct cap2;
	struct imgsensor_mode_struct normal_video;/* normal video  scenario relative information */
	struct imgsensor_mode_struct hs_video; /* high speed video scenario relative information */
	struct imgsensor_mode_struct slim_video; /* slim video for VT scenario relative information */
	kal_uint8 ae_shut_delay_frame; /* shutter delay frame for AE cycle */
	kal_uint8 ae_sensor_gain_delay_frame; /* sensor gain delay frame for AE cycle */
	kal_uint8 ae_ispgain_delay_frame; /* isp gain delay frame for AE cycle */
	kal_uint8 ihdr_support; /* 1, support; 0,not support */
	kal_uint8 ihdr_le_firstline; /* 1,le first ; 0, se first */
	kal_uint8 sensor_mode_num; /* support sensor mode num */
	kal_uint8 cap_delay_frame; /* enter capture delay frame num */
	kal_uint8 pre_delay_frame; /* enter preview delay frame num */
	kal_uint8 video_delay_frame; /* enter video delay frame num */
	kal_uint8 hs_video_delay_frame; /* enter high speed video  delay frame num */
	kal_uint8 slim_video_delay_frame; /* enter slim video delay frame num */
	kal_uint8 margin; /* sensor framelength & shutter margin */
	kal_uint32 min_shutter; /* min shutter */
	kal_uint32 max_frame_length; /* max framelength by sensor register's limitation */
	kal_uint8 isp_driving_current; /* mclk driving current */
	kal_uint8 sensor_interface_type; /* sensor_interface_type */
	kal_uint8 mipi_sensor_type;
	/* 0, MIPI_OPHY_NCSI2; 1, MIPI_OPHY_CSI2, default is NCSI2, don't modify this para */
	kal_uint8 mipi_settle_delay_mode;
	/* 0, high speed signal auto detect; 1, use settle delay, unit is ns*/
	/* default is auto detect, don't modify this para */
	kal_uint8 sensor_output_dataformat; /* sensor output first pixel color */
	kal_uint8 mclk; /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	kal_uint8 mipi_lane_num; /* mipi lane num */
	kal_uint8 i2c_addr_table[5];
	/* record sensor support all write id addr, only supprt 4must end with 0xff */
	kal_uint32  i2c_speed; /* i2c speed */
};

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId);
extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId,
	u16 transfer_length, u16 timing);
#endif
