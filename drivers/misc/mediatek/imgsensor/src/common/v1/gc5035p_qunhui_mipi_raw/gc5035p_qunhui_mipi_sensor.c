/*
 * gc5035p_qunhui_mipi_Sensor.c
 * bring up for camera
 * Copyright (C) Huawei Technology Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
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

#include <securec.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "gc5035p_qunhui_mipi_sensor.h"

#define PFX "gc5035p_qunhui_camera_sensor"
#define LOG_1 log_inf("GC5035P_QUNHUI_MIPI, 2LANE\n")
#define GC5035P_QUNHUI_DEBUG 0
#define OTP_CHECK_SUCCESSED 1
#define OTP_CHECK_FAILED 0
#define log_err(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)
#if GC5035P_QUNHUI_DEBUG
#define log_inf(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
#else
#define log_inf(format, args...)
#endif

#define MULTI_WRITE 1
static DEFINE_SPINLOCK(imgsensor_drv_lock);
/* used for shutter compensation */

static kal_uint32 dgain_ratio = GC5035P_QUNHUI_SENSOR_DGAIN_BASE;
static kal_uint8 lensid;

#if GC5035P_QUNHUI_OTP_FOR_CUSTOMER
struct gc5035p_qunhui_otp_t gc5035p_qunhui_otp_data;
#endif

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = GC5035P_QUNHUI_MIPI_SENSOR_ID,
	.checksum_value = 0xcde448ca,

	.pre = {
		.pclk = 83200000,
		.linelength = 1460,
		.framelength = 2008,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1296,
		.grabwindow_height = 972,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 83200000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 166400000,
		.linelength = 2920,
		.framelength = 2008,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 166400000,
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk = 153400000,
		.linelength = 2920,
		.framelength = 2008,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 153400000,
		.max_framerate = 240,             /*less than 13M(include 13M)*/
	},
	.normal_video = {
		.pclk = 166400000,
		.linelength = 2920,
		.framelength = 2008,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 166400000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 166400000,
		.linelength = 1896,
		.framelength = 1536,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 166400000,
		.max_framerate = 600,
	},
	.slim_video = {
		.pclk = 83200000,
		.linelength = 1460,
		.framelength = 2008,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 83200000,
		.max_framerate = 300,
	},
	.margin = 16,
	.min_shutter = 4,
	.max_frame_length = 0x3fff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispgain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 5,

	.cap_delay_frame = 1,
	.pre_delay_frame = 1,
	.video_delay_frame = 1,
	.hs_video_delay_frame = 1,
	.slim_video_delay_frame = 1,

	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
#if GC5035P_QUNHUI_MIRROR_FLIP_ENABLE
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
#else
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
#endif
	.mclk = 26,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	.i2c_addr_table = {0x7e, 0xff},
	.i2c_speed = 400,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x258,
	.gain = 0x40,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x6e,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{2592, 1944, 0, 0, 2592, 1944, 1296,
	972, 0, 0, 1296, 972, 0, 0, 1296, 972 },
	{ 2592, 1944, 0, 0, 2592, 1944, 2592,
	1944, 0, 0, 2592, 1944, 0, 0, 2592, 1944 },
	{ 2592, 1944, 0, 0, 2592, 1944, 2592,
	1944, 0, 0, 2592, 1944, 0, 0, 2592, 1944 },
	{ 2592, 1944, 656, 492, 1280,  960,
	640, 480, 0, 0, 640, 480, 0, 0, 640, 480 },
	{ 2592, 1944,  16, 252, 2560, 1440,
	1280, 720, 0, 0, 1280, 720, 0, 0, 1280, 720 }
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[1] = { (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 1, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[2] = { (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 2, imgsensor.i2c_write_id);
}

#ifdef MULTI_WRITE
#define I2C_BUFFER_LEN 1024

static kal_uint16 gc5035p_qunhui_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char pusendcmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, idx = 0;
	kal_uint16 addr, addr_last, data = 0;

	tosend = 0;
	idx = 0;
	while (len > idx) {
		addr = para[idx];
		{
			pusendcmd[tosend++] = (char)(addr & 0xFF);
			data = para[idx + 1];
			pusendcmd[tosend++] = (char)(data & 0xFF);
			idx += 2;
			addr_last = addr;
		}
		if ((I2C_BUFFER_LEN - tosend) < 2 ||
			len == idx ||
			addr != addr_last) {
			iBurstWriteReg_multi(pusendcmd, tosend,
				imgsensor.i2c_write_id, 2, imgsensor_info.i2c_speed);
			tosend = 0;
		}
	}
	return 0;
}
#endif

#if GC5035P_QUNHUI_OTP_FOR_CUSTOMER
int camcal_read_insensor_gc5035p_qunhui_func(unsigned char *eeprom_buff,
			    int eeprom_size)
{
	int i = 0;

	if (!eeprom_buff || eeprom_size <= 0)
		return -1;

	memcpy_s(eeprom_buff, eeprom_size, gc5035p_qunhui_otp_data.otp_buf, eeprom_size);

	for (i = 0; i < GC5035P_QUNHUI_OTP_BUF_SIZE; i++)
		log_inf("gc5035_otp_buf[%d] = 0x%x\n", i, gc5035p_qunhui_otp_data.otp_buf[i]);
	log_inf("gc5035p qunhui driver read otp success!\n");
	return eeprom_size;
}

static kal_uint8 gc5035p_qunhui_otp_read_byte(kal_uint16 addr)
{
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x69, (addr >> 8) & 0x1f);
	write_cmos_sensor(0x6a, addr & 0xff);
	write_cmos_sensor(0xf3, 0x20);
	return read_cmos_sensor(0x6c);
}

static void gc5035p_qunhui_otp_read_group(kal_uint16 addr,
	kal_uint8 *data, kal_uint16 length)
{
	kal_uint16 i = 0;

	if ((((addr & 0x1fff) >> 3) + length) > GC5035P_QUNHUI_OTP_DATA_LENGTH) {
		log_inf("out of range, start addr: 0x%.4x, length = %d\n",
			addr & 0x1fff, length);
		return;
	}

	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x69, (addr >> 8) & 0x1f);
	write_cmos_sensor(0x6a, addr & 0xff);
	write_cmos_sensor(0xf3, 0x20);
	write_cmos_sensor(0xf3, 0x12);

	for (i = 0; i < length; i++)
		data[i] = read_cmos_sensor(0x6c);

	write_cmos_sensor(0xf3, 0x00);
}

static void gc5035p_qunhui_gcore_read_dpc(void)
{
	kal_uint8 dpcflag = 0;
	struct gc5035p_qunhui_dpc_t *pdpc = &gc5035p_qunhui_otp_data.dpc;

	dpcflag = gc5035p_qunhui_otp_read_byte(GC5035P_QUNHUI_OTP_DPC_FLAG_OFFSET);
	log_inf("dpc flag = 0x%x\n", dpcflag);
	switch (gc5035p_qunhui_otp_get_2bit_flag(dpcflag, 0)) {
	case GC5035P_QUNHUI_OTP_FLAG_EMPTY: {
		log_err("dpc info is empty!!\n");
		pdpc->flag = GC5035P_QUNHUI_OTP_FLAG_EMPTY;
		break;
	}
	case GC5035P_QUNHUI_OTP_FLAG_VALID: {
		log_inf("dpc info is valid!\n");
		pdpc->total_num =
			gc5035p_qunhui_otp_read_byte(
			GC5035P_QUNHUI_OTP_DPC_TOTAL_NUMBER_OFFSET)
			+ gc5035p_qunhui_otp_read_byte(
			GC5035P_QUNHUI_OTP_DPC_ERROR_NUMBER_OFFSET);
		pdpc->flag = GC5035P_QUNHUI_OTP_FLAG_VALID;
		log_inf("total_num = %d\n", pdpc->total_num);
		break;
	}
	default:
		pdpc->flag = GC5035P_QUNHUI_OTP_FLAG_INVALID;
		break;
	}
}

static void gc5035p_qunhui_gcore_read_reg(void)
{
	kal_uint8 i = 0;
	kal_uint8 j = 0;
	kal_uint16 base_group = 0;
	kal_uint8 reg[GC5035P_QUNHUI_OTP_REG_DATA_SIZE];
	struct gc5035p_qunhui_reg_update_t *pregs = &gc5035p_qunhui_otp_data.regs;

	memset_s(&reg, GC5035P_QUNHUI_OTP_REG_DATA_SIZE, 0, GC5035P_QUNHUI_OTP_REG_DATA_SIZE);
	pregs->flag = gc5035p_qunhui_otp_read_byte(GC5035P_QUNHUI_OTP_REG_FLAG_OFFSET);
	log_inf("register update flag = 0x%x\n", pregs->flag);
	if (pregs->flag == GC5035P_QUNHUI_OTP_FLAG_VALID) {
		gc5035p_qunhui_otp_read_group(GC5035P_QUNHUI_OTP_REG_DATA_OFFSET,
		&reg[0], GC5035P_QUNHUI_OTP_REG_DATA_SIZE);

		for (i = 0; i < GC5035P_QUNHUI_OTP_REG_MAX_GROUP; i++) {
			base_group = i * GC5035P_QUNHUI_OTP_REG_BYTE_PER_GROUP;
			for (j = 0; j < GC5035P_QUNHUI_OTP_REG_REG_PER_GROUP; j++)
				if (gc5035p_qunhui_otp_check_1bit_flag(
				reg[base_group], (4 * j + 3))) {
					pregs->reg[pregs->cnt].page =
						(reg[base_group] >>
						(4 * j)) & 0x07;
					pregs->reg[pregs->cnt].addr =
						reg[base_group +
						j * GC5035P_QUNHUI_OTP_REG_BYTE_PER_REG
						+ 1];
					pregs->reg[pregs->cnt].value =
						reg[base_group +
						j * GC5035P_QUNHUI_OTP_REG_BYTE_PER_REG
						+ 2];
					log_inf("register[%d] P%d:0x%x->0x%x\n",
						pregs->cnt,
						pregs->reg[pregs->cnt].page,
						pregs->reg[pregs->cnt].addr,
						pregs->reg[pregs->cnt].value);
					pregs->cnt++;
				}
		}

	}
}

static void gc5035p_qunhui_otp_read_sn_info(void)
{
	kal_uint8 i = 0;
	kal_uint8 flag = 0;
	kal_uint16 check = 0;
	kal_uint16 sn_start_offset = 0;
	kal_uint8 info[GC5035P_QUNHUI_OTP_SN_DATA_SIZE];

	memset_s(&info, GC5035P_QUNHUI_OTP_SN_DATA_SIZE, 0, GC5035P_QUNHUI_OTP_SN_DATA_SIZE);
	flag = gc5035p_qunhui_otp_read_byte(GC5035P_QUNHUI_OTP_SN_FLAG_OFFSET);

	if ((flag & 0x03) == 0x01) {
		sn_start_offset = GC5035P_QUNHUI_OTP_SN_DATA_GROUP2_OFFSET;
	} else if (((flag & 0x0c) >> 2) == 0x01) {
		sn_start_offset = GC5035P_QUNHUI_OTP_SN_DATA_GROUP1_OFFSET;
	} else {
		log_err("Invalid module, error sn flag = 0x%x\n", flag);
		gc5035p_qunhui_otp_data.otp_buf[26] = OTP_CHECK_FAILED; /* SN check flag offset */
		return;
	}

	gc5035p_qunhui_otp_read_group(sn_start_offset, &info[0], GC5035P_QUNHUI_OTP_SN_DATA_SIZE);
	for (i = 0; i < GC5035P_QUNHUI_OTP_SN_DATA_SIZE - 1; i++)
		check += info[i];

	if ((check % 255 + 1) == info[GC5035P_QUNHUI_OTP_SN_DATA_SIZE - 1]) { /* 255: checksum base value  */
		/* Mind the byteorder */
		memcpy_s(gc5035p_qunhui_otp_data.sn.data, GC5035P_QUNHUI_OTP_SN_VALID_DATA_SIZE,
			info, GC5035P_QUNHUI_OTP_SN_VALID_DATA_SIZE);
		memcpy_s(&gc5035p_qunhui_otp_data.otp_buf[6], GC5035P_QUNHUI_OTP_SN_VALID_DATA_SIZE,
			info, GC5035P_QUNHUI_OTP_SN_VALID_DATA_SIZE);
		gc5035p_qunhui_otp_data.sn.data[GC5035P_QUNHUI_OTP_SN_VALID_DATA_SIZE] = '\0';
		log_inf("SN checksum success! otp sn info = %s\n", gc5035p_qunhui_otp_data.sn.data);
		gc5035p_qunhui_otp_data.otp_buf[26] = OTP_CHECK_SUCCESSED;
	} else {
		log_err("SN checksum failed! check sum = 0x%x, calculate result = 0x%x\n",
			info[GC5035P_QUNHUI_OTP_SN_DATA_SIZE - 1], (check % 255 + 1));
		gc5035p_qunhui_otp_data.otp_buf[26] = OTP_CHECK_FAILED;
	}
}

static void gc5035p_qunhui_otp_read_af_info(void)
{
	kal_uint8 i = 0;
	kal_uint8 flag = 0;
	kal_uint16 check = 0;
	kal_uint16 af_start_offset = 0;
	kal_uint8 info[GC5035P_QUNHUI_OTP_AF_DATA_SIZE];

	memset_s(&info, GC5035P_QUNHUI_OTP_AF_DATA_SIZE, 0, GC5035P_QUNHUI_OTP_AF_DATA_SIZE);
	flag = gc5035p_qunhui_otp_read_byte(GC5035P_QUNHUI_OTP_AF_FLAG_OFFSET);
	if ((flag & 0x03) == 0x01) {
		af_start_offset = GC5035P_QUNHUI_OTP_AF_DATA_GROUP2_OFFSET;
	} else if (((flag & 0x0c) >> 2) == 0x01) {
		af_start_offset = GC5035P_QUNHUI_OTP_AF_DATA_GROUP1_OFFSET;
	} else {
		log_err("Invalid module, error af flag = 0x%x\n", flag);
		gc5035p_qunhui_otp_data.otp_buf[5] = OTP_CHECK_FAILED; /* AF check flag offset */
		return;
	}

	gc5035p_qunhui_otp_read_group(af_start_offset, &info[0], GC5035P_QUNHUI_OTP_AF_DATA_SIZE);
	for (i = 0; i < GC5035P_QUNHUI_OTP_AF_DATA_SIZE - 1; i++)
		check += info[i];

	if ((check % 255 + 1) == info[GC5035P_QUNHUI_OTP_AF_DATA_SIZE - 1]) { /* 255: checksum base value  */
		gc5035p_qunhui_otp_data.af.direction = info[0];
		gc5035p_qunhui_otp_data.af.macro = info[1] + ((info[2]&0xff) << 8);
		gc5035p_qunhui_otp_data.af.infinity = info[3] + ((info[4]&0xff) << 8);
		gc5035p_qunhui_otp_data.otp_buf[0] = info[0];
		gc5035p_qunhui_otp_data.otp_buf[1] = info[1];
		gc5035p_qunhui_otp_data.otp_buf[2] = info[2];
		gc5035p_qunhui_otp_data.otp_buf[3] = info[3];
		gc5035p_qunhui_otp_data.otp_buf[4] = info[4];
		log_inf("AF checksum success! direction = 0x%x\n", gc5035p_qunhui_otp_data.af.direction);
		log_inf("macro = 0x%x\n", gc5035p_qunhui_otp_data.af.macro);
		log_inf("infinity = 0x%x\n", gc5035p_qunhui_otp_data.af.infinity);
		gc5035p_qunhui_otp_data.otp_buf[5] = OTP_CHECK_SUCCESSED; /* AF check flag offset */
	} else {
		log_err("AF checksum failed! check sum = 0x%x, calculate result = 0x%x\n",
			info[GC5035P_QUNHUI_OTP_AF_DATA_SIZE - 1], (check % 255 + 1));
		gc5035p_qunhui_otp_data.otp_buf[5] = OTP_CHECK_FAILED;
	}
}

static kal_uint8 gc5035p_qunhui_otp_read_module_info(void)
{
	kal_uint8 i = 0;
	kal_uint8 flag = 0;
	kal_uint16 check = 0;
	kal_uint16 module_start_offset = 0;
	kal_uint8 info[GC5035P_QUNHUI_OTP_MODULE_DATA_SIZE];
	struct gc5035p_qunhui_module_info_t module_info = { 0 };

	memset_s(&info, GC5035P_QUNHUI_OTP_MODULE_DATA_SIZE, 0, GC5035P_QUNHUI_OTP_MODULE_DATA_SIZE);
	memset_s(&module_info, sizeof(struct gc5035p_qunhui_module_info_t), 0, sizeof(struct gc5035p_qunhui_module_info_t));
	flag = gc5035p_qunhui_otp_read_byte(GC5035P_QUNHUI_OTP_MODULE_FLAG_OFFSET);

	if ((flag & 0x03) == 0x01) {
		module_start_offset = GC5035P_QUNHUI_OTP_MODULE_DATA_GROUP2_OFFSET;
	} else if (((flag & 0x0c) >> 2) == 0x01) {
		module_start_offset = GC5035P_QUNHUI_OTP_MODULE_DATA_GROUP1_OFFSET;
	} else {
		log_err("Invalid module,error module info flag = 0x%x\n", flag);
		return -1;
	}

	gc5035p_qunhui_otp_read_group(module_start_offset, &info[0], GC5035P_QUNHUI_OTP_MODULE_DATA_SIZE);
	for (i = 0; i < GC5035P_QUNHUI_OTP_MODULE_DATA_SIZE - 1; i++)
		check += info[i];

	if ((check % 255 + 1) == info[GC5035P_QUNHUI_OTP_MODULE_DATA_SIZE - 1]) {
		module_info.module_id = info[0];
		module_info.lens_id = info[1];
		module_info.reserved0 = info[2];
		module_info.reserved1 = info[3];
		module_info.year = info[4];
		module_info.month = info[5];
		module_info.day = info[6];
		lensid = info[1];

		log_inf("module info checksum success! module id = 0x%x\n", module_info.module_id);
		log_inf("lens id = 0x%x\n", module_info.lens_id);
		log_inf("year: %d, month: %d, day: %d\n", module_info.year, module_info.month, module_info.day);
	} else {
		log_err("module info checksum failed! check sum = 0x%x, calculate result = 0x%x\n",
			info[GC5035P_QUNHUI_OTP_MODULE_DATA_SIZE - 1], (check % 255 + 1));
		return -1;
	}
	return module_info.module_id;
}

static void gc5035p_qunhui_otp_read_gwb_info(void)
{
	kal_uint8 i = 0;
	kal_uint8 flag = 0;
	kal_uint16 golden_check = 0;
	kal_uint16 golden_start_offset = 0;
	kal_uint8 golden[GC5035P_QUNHUI_OTP_GOLDEN_DATA_SIZE];
	struct gc5035p_qunhui_wb_t *pgolden = &gc5035p_qunhui_otp_data.golden;

	memset_s(&golden, GC5035P_QUNHUI_OTP_GOLDEN_DATA_SIZE, 0, GC5035P_QUNHUI_OTP_GOLDEN_DATA_SIZE);
	flag = gc5035p_qunhui_otp_read_byte(GC5035P_QUNHUI_OTP_WB_FLAG_OFFSET);

	if ((flag & 0x30) == 0x10) {
		golden_start_offset = GC5035P_QUNHUI_OTP_GOLDEN_DATA_GROUP2_OFFSET;
	} else if (((flag & 0xc0) >> 2) == 0x10) {
		golden_start_offset = GC5035P_QUNHUI_OTP_GOLDEN_DATA_GROUP1_OFFSET;
	} else {
		log_err("Invalid module,error gawb flag = 0x%x\n", flag);
		pgolden->flag = pgolden->flag | GC5035P_QUNHUI_OTP_FLAG_INVALID;
		return;
	}

	/* Golden AWB */
	gc5035p_qunhui_otp_read_group(golden_start_offset, &golden[0], GC5035P_QUNHUI_OTP_GOLDEN_DATA_SIZE);
	for (i = 0; i < GC5035P_QUNHUI_OTP_GOLDEN_DATA_SIZE - 1; i++)
		golden_check += golden[i];

	if ((golden_check % 255 + 1) == golden[GC5035P_QUNHUI_OTP_GOLDEN_DATA_SIZE - 1]) {
		pgolden->rg = (golden[0] | ((golden[1] & 0xf0) << 4));
		pgolden->bg = (((golden[1] & 0x0f) << 8) | golden[2]);
		pgolden->rg = pgolden->rg == 0 ? GC5035P_QUNHUI_OTP_WB_RG_TYPICAL : pgolden->rg;
		pgolden->bg = pgolden->bg == 0 ? GC5035P_QUNHUI_OTP_WB_BG_TYPICAL : pgolden->bg;
		pgolden->flag = pgolden->flag | GC5035P_QUNHUI_OTP_FLAG_VALID;
		log_inf("golden AWB checksum success! golden wb r/g = 0x%x\n", pgolden->rg);
		log_inf("golden awb b/g = 0x%x\n", pgolden->bg);
	} else {
		pgolden->flag = pgolden->flag | GC5035P_QUNHUI_OTP_FLAG_INVALID;
		log_err("golden AWB checksum failed! check sum = 0x%x, calculate result = 0x%x\n",
			golden[GC5035P_QUNHUI_OTP_GOLDEN_DATA_SIZE - 1],
			(golden_check % 255 + 1));
	}
}

static void gc5035p_qunhui_otp_read_wb_info(void)
{
	kal_uint8 i = 0;
	kal_uint8 flag = 0;
	kal_uint16 wb_check = 0;
	kal_uint16 wb_start_offset = 0;
	kal_uint8 wb[GC5035P_QUNHUI_OTP_WB_DATA_SIZE];
	struct gc5035p_qunhui_wb_t *pwb = &gc5035p_qunhui_otp_data.wb;

	memset_s(&wb, GC5035P_QUNHUI_OTP_WB_DATA_SIZE, 0, GC5035P_QUNHUI_OTP_WB_DATA_SIZE);
	flag = gc5035p_qunhui_otp_read_byte(GC5035P_QUNHUI_OTP_WB_FLAG_OFFSET);

	if ((flag & 0x03) == 0x01) {
		wb_start_offset = GC5035P_QUNHUI_OTP_WB_DATA_GROUP2_OFFSET;
	} else if (((flag & 0x0c) >> 2) == 0x01) {
		wb_start_offset = GC5035P_QUNHUI_OTP_WB_DATA_GROUP1_OFFSET;
	} else {
		log_err("Invalid module,error awb flag = 0x%x\n", flag);
		pwb->flag = pwb->flag | GC5035P_QUNHUI_OTP_FLAG_INVALID;
		return;
	}

	/* AWB */
	gc5035p_qunhui_otp_read_group(wb_start_offset, &wb[0], GC5035P_QUNHUI_OTP_WB_DATA_SIZE);
	for (i = 0; i < GC5035P_QUNHUI_OTP_WB_DATA_SIZE - 1; i++)
		wb_check += wb[i];

	if ((wb_check % 255 + 1) == wb[GC5035P_QUNHUI_OTP_WB_DATA_SIZE - 1]) {
		pwb->rg = (wb[0] | ((wb[1] & 0xf0) << 4));
		pwb->bg = (((wb[1] & 0x0f) << 8) | wb[2]);
		pwb->rg = pwb->rg == 0 ? GC5035P_QUNHUI_OTP_WB_RG_TYPICAL : pwb->rg;
		pwb->bg = pwb->bg == 0 ? GC5035P_QUNHUI_OTP_WB_BG_TYPICAL : pwb->bg;
		pwb->flag = pwb->flag | GC5035P_QUNHUI_OTP_FLAG_VALID;
		log_inf("AWB checksum success! golden wb r/g = 0x%x\n", pwb->rg);
		log_inf("awb b/g = 0x%x\n", pwb->bg);
	} else {
		pwb->flag = pwb->flag | GC5035P_QUNHUI_OTP_FLAG_INVALID;
		log_err("AWB checksum failed! check sum = 0x%x, calculate result = 0x%x\n",
			wb[GC5035P_QUNHUI_OTP_WB_DATA_SIZE - 1],
			(wb_check % 255 + 1));
	}
}

static kal_uint8 gc5035p_qunhui_otp_read_sensor_info(void)
{
	kal_uint8 moduleid = 0;
#if GC5035P_QUNHUI_OTP_DEBUG
	kal_uint16 i = 0;
	kal_uint8 debug[GC5035P_QUNHUI_OTP_DATA_LENGTH];
#endif

	gc5035p_qunhui_gcore_read_dpc();
	gc5035p_qunhui_gcore_read_reg();

	moduleid = gc5035p_qunhui_otp_read_module_info();
	gc5035p_qunhui_otp_read_wb_info();
	gc5035p_qunhui_otp_read_gwb_info();
	gc5035p_qunhui_otp_read_sn_info();
	gc5035p_qunhui_otp_read_af_info();

#if GC5035P_QUNHUI_OTP_DEBUG
	memset_s(&debug[0], GC5035P_QUNHUI_OTP_DATA_LENGTH, 0, GC5035P_QUNHUI_OTP_DATA_LENGTH);
	gc5035p_qunhui_otp_read_group(GC5035P_QUNHUI_OTP_START_ADDR, &debug[0], GC5035P_QUNHUI_OTP_DATA_LENGTH);
	for (i = 0; i < GC5035P_QUNHUI_OTP_DATA_LENGTH; i++)
		log_inf("addr = 0x%x, data = 0x%x\n", GC5035P_QUNHUI_OTP_START_ADDR + i * 8, debug[i]);
#endif

	return moduleid;
}

static void gc5035p_qunhui_otp_update_dd(void)
{
	kal_uint8 state = 0;
	kal_uint8 n = 0;
	struct gc5035p_qunhui_dpc_t *pdpc = &gc5035p_qunhui_otp_data.dpc;

	if (pdpc->flag == GC5035P_QUNHUI_OTP_FLAG_VALID) {
		log_inf("DD auto load start!\n");
		write_cmos_sensor(0xfe, 0x02);
		write_cmos_sensor(0xbe, 0x00);
		write_cmos_sensor(0xa9, 0x01);
		write_cmos_sensor(0x09, 0x33);
		write_cmos_sensor(0x01, (pdpc->total_num >> 8) & 0x07);
		write_cmos_sensor(0x02, pdpc->total_num & 0xff);
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
}

static void gc5035p_qunhui_otp_update_wb(void)
{
	kal_uint16 r_gain = GC5035P_QUNHUI_OTP_WB_GAIN_BASE;
	kal_uint16 g_gain = GC5035P_QUNHUI_OTP_WB_GAIN_BASE;
	kal_uint16 b_gain = GC5035P_QUNHUI_OTP_WB_GAIN_BASE;
	kal_uint16 base_gain = GC5035P_QUNHUI_OTP_WB_CAL_BASE;
	kal_uint16 r_gain_curr = GC5035P_QUNHUI_OTP_WB_CAL_BASE;
	kal_uint16 g_gain_curr = GC5035P_QUNHUI_OTP_WB_CAL_BASE;
	kal_uint16 b_gain_curr = GC5035P_QUNHUI_OTP_WB_CAL_BASE;
	kal_uint16 rg_typical = GC5035P_QUNHUI_OTP_WB_RG_TYPICAL;
	kal_uint16 bg_typical = GC5035P_QUNHUI_OTP_WB_BG_TYPICAL;
	struct gc5035p_qunhui_wb_t *pwb = &gc5035p_qunhui_otp_data.wb;
	struct gc5035p_qunhui_wb_t *pgolden = &gc5035p_qunhui_otp_data.golden;

	if (gc5035p_qunhui_otp_check_1bit_flag(pgolden->flag, 0)) {
		rg_typical = pgolden->rg;
		bg_typical = pgolden->bg;
	} else {
		rg_typical = GC5035P_QUNHUI_OTP_WB_RG_TYPICAL;
		bg_typical = GC5035P_QUNHUI_OTP_WB_BG_TYPICAL;
	}

	if (gc5035p_qunhui_otp_check_1bit_flag(pwb->flag, 0)) {
		r_gain_curr = GC5035P_QUNHUI_OTP_WB_CAL_BASE * rg_typical / pwb->rg;
		b_gain_curr = GC5035P_QUNHUI_OTP_WB_CAL_BASE * bg_typical / pwb->bg;
		g_gain_curr = GC5035P_QUNHUI_OTP_WB_CAL_BASE;
		base_gain = (r_gain_curr < b_gain_curr) ?
			r_gain_curr : b_gain_curr;
		base_gain = (base_gain < g_gain_curr) ? base_gain : g_gain_curr;
		r_gain = GC5035P_QUNHUI_OTP_WB_GAIN_BASE * r_gain_curr / base_gain;
		g_gain = GC5035P_QUNHUI_OTP_WB_GAIN_BASE * g_gain_curr / base_gain;
		b_gain = GC5035P_QUNHUI_OTP_WB_GAIN_BASE * b_gain_curr / base_gain;
		write_cmos_sensor(0xfe, 0x04);
		write_cmos_sensor(0x40, g_gain & 0xff);
		write_cmos_sensor(0x41, r_gain & 0xff);
		write_cmos_sensor(0x42, b_gain & 0xff);
		write_cmos_sensor(0x43, g_gain & 0xff);
		write_cmos_sensor(0x44, g_gain & 0xff);
		write_cmos_sensor(0x45, r_gain & 0xff);
		write_cmos_sensor(0x46, b_gain & 0xff);
		write_cmos_sensor(0x47, g_gain & 0xff);
		write_cmos_sensor(0x48, (g_gain >> 8) & 0x07);
		write_cmos_sensor(0x49, (r_gain >> 8) & 0x07);
		write_cmos_sensor(0x4a, (b_gain >> 8) & 0x07);
		write_cmos_sensor(0x4b, (g_gain >> 8) & 0x07);
		write_cmos_sensor(0x4c, (g_gain >> 8) & 0x07);
		write_cmos_sensor(0x4d, (r_gain >> 8) & 0x07);
		write_cmos_sensor(0x4e, (b_gain >> 8) & 0x07);
		write_cmos_sensor(0x4f, (g_gain >> 8) & 0x07);
		write_cmos_sensor(0xfe, 0x00);
	}
}

static void gc5035p_qunhui_otp_update_reg(void)
{
	kal_uint8 i = 0;

	log_inf("reg count = %d\n", gc5035p_qunhui_otp_data.regs.cnt);

	if (gc5035p_qunhui_otp_check_1bit_flag(gc5035p_qunhui_otp_data.regs.flag, 0))
		for (i = 0; i < gc5035p_qunhui_otp_data.regs.cnt; i++) {
			write_cmos_sensor(
			0xfe, gc5035p_qunhui_otp_data.regs.reg[i].page);
			write_cmos_sensor(
			gc5035p_qunhui_otp_data.regs.reg[i].addr,
			gc5035p_qunhui_otp_data.regs.reg[i].value);
			log_inf("reg[%d] P%d:0x%x -> 0x%x\n",
			i, gc5035p_qunhui_otp_data.regs.reg[i].page,
				gc5035p_qunhui_otp_data.regs.reg[i].addr,
				gc5035p_qunhui_otp_data.regs.reg[i].value);
		}
}

static void gc5035p_qunhui_otp_update(void)
{
	gc5035p_qunhui_otp_update_dd();
	gc5035p_qunhui_otp_update_wb();
	gc5035p_qunhui_otp_update_reg();
}

static kal_uint8 gc5035p_qunhui_otp_identify(void)
{
	kal_uint8 moduleid = 0;

	memset_s(&gc5035p_qunhui_otp_data, sizeof(struct gc5035p_qunhui_otp_t), 0, sizeof(gc5035p_qunhui_otp_data));

	write_cmos_sensor(0xfc, 0x01);
	write_cmos_sensor(0xf4, 0x40);
	write_cmos_sensor(0xf5, 0xe9);
	write_cmos_sensor(0xf6, 0x14);
	write_cmos_sensor(0xf8, 0x40);
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

	gc5035p_qunhui_otp_read_group(
	GC5035P_QUNHUI_OTP_ID_DATA_OFFSET,
	&gc5035p_qunhui_otp_data.otp_id[0],
	GC5035P_QUNHUI_OTP_ID_SIZE);
	moduleid = gc5035p_qunhui_otp_read_sensor_info();

	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x67, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfa, 0x00);
	return moduleid;
}

static void gc5035p_qunhui_otp_function(void)
{
	kal_uint8 i = 0, flag = 0;
	kal_uint8 otp_id[GC5035P_QUNHUI_OTP_ID_SIZE];

	memset_s(&otp_id, GC5035P_QUNHUI_OTP_ID_SIZE, 0, GC5035P_QUNHUI_OTP_ID_SIZE);

	write_cmos_sensor(0xfa, 0x10);
	write_cmos_sensor(0xf5, 0xe9);
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x67, 0xc0);
	write_cmos_sensor(0x59, 0x3f);
	write_cmos_sensor(0x55, 0x80);
	write_cmos_sensor(0x65, 0x80);
	write_cmos_sensor(0x66, 0x03);
	write_cmos_sensor(0xfe, 0x00);

	gc5035p_qunhui_otp_read_group(GC5035P_QUNHUI_OTP_ID_DATA_OFFSET,
	&otp_id[0], GC5035P_QUNHUI_OTP_ID_SIZE);
	for (i = 0; i < GC5035P_QUNHUI_OTP_ID_SIZE; i++)
		if (otp_id[i] != gc5035p_qunhui_otp_data.otp_id[i]) {
			flag = 1;
			break;
		}

	if (flag == 1) {
		log_err("otp id mismatch, read again");
		memset_s(&gc5035p_qunhui_otp_data, sizeof(struct gc5035p_qunhui_otp_t), 0, sizeof(gc5035p_qunhui_otp_data));
		for (i = 0; i < GC5035P_QUNHUI_OTP_ID_SIZE; i++)
			gc5035p_qunhui_otp_data.otp_id[i] = otp_id[i];
		gc5035p_qunhui_otp_read_sensor_info();
	}

	gc5035p_qunhui_otp_update();

	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x67, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfa, 0x00);
}
#endif

static void set_dummy(void)
{
	kal_uint32 frame_length = imgsensor.frame_length >> 2;

	frame_length = frame_length << 2;
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x41, (frame_length >> 8) & 0x3f);
	write_cmos_sensor(0x42, frame_length & 0xff);
	log_inf("Exit! framelength = %d\n", frame_length);
}

static kal_uint32 return_sensor_id(void)
{
	return (((read_cmos_sensor(0xf0) << 8) | read_cmos_sensor(0xf1)) + 2);
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	if (framerate == 0) {
		frame_length = 0;
	} else {
		frame_length = imgsensor.pclk /
		framerate * 10 /
		imgsensor.line_length;
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length)
		? frame_length : imgsensor.min_frame_length;
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
}

static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_uint32 cal_shutter = 0;

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
	shutter = (shutter < imgsensor_info.min_shutter) ?
	imgsensor_info.min_shutter : shutter;
	shutter = (shutter >
	(imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length -
		imgsensor_info.margin) : shutter;

	realtime_fps = imgsensor.pclk /
	imgsensor.line_length * 10 / imgsensor.frame_length;

	if (imgsensor.autoflicker_en) {
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			set_max_framerate(realtime_fps, 0);
	} else {
		set_max_framerate(realtime_fps, 0);
	}

	cal_shutter = shutter >> 2;
	cal_shutter = cal_shutter << 2;
	dgain_ratio = 256 * shutter / cal_shutter;

	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x03, (cal_shutter >> 8) & 0x3F);
	write_cmos_sensor(0x04, cal_shutter & 0xFF);

	log_inf("Exit! shutter = %d", shutter);
	log_inf("framelength = %d\n", imgsensor.frame_length);
	log_inf("Exit! cal_shutter = %d", cal_shutter);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = gain << 2;

	if (reg_gain < GC5035P_QUNHUI_SENSOR_GAIN_BASE)
		reg_gain = GC5035P_QUNHUI_SENSOR_GAIN_BASE;
	else if (reg_gain > GC5035P_QUNHUI_SENSOR_GAIN_MAX)
		reg_gain = GC5035P_QUNHUI_SENSOR_GAIN_MAX;

	return (kal_uint16)reg_gain;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;
	kal_uint32 temp_gain;
	kal_int16 gain_index;
	kal_uint16 gc5035p_qunhui_agc_param[GC5035P_QUNHUI_SENSOR_GAIN_MAP_SIZE][2] = {
		{  256,  0 },
		{  302,  1 },
		{  358,  2 },
		{  425,  3 },
		{  502,  8 },
		{  599,  9 },
		{  717, 10 },
		{  845, 11 },
		{  998, 12 },
		{ 1203, 13 },
		{ 1434, 14 },
		{ 1710, 15 },
		{ 1997, 16 },
		{ 2355, 17 },
		{ 2816, 18 },
		{ 3318, 19 },
		{ 3994, 20 },
	};

	reg_gain = gain2reg(gain);

	for (gain_index = GC5035P_QUNHUI_SENSOR_GAIN_MAX_VALID_INDEX - 1;
	gain_index >= 0; gain_index--)
		if (reg_gain >= gc5035p_qunhui_agc_param[gain_index][0])
			break;

	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xb6, gc5035p_qunhui_agc_param[gain_index][1]);
	temp_gain = reg_gain * dgain_ratio / gc5035p_qunhui_agc_param[gain_index][0];
	write_cmos_sensor(0xb1, (temp_gain >> 8) & 0x0f);
	write_cmos_sensor(0xb2, temp_gain & 0xfc);
	log_inf("Exit! gc5035p_qunhui_agc_param[gain_index][1] = 0x%x",
		gc5035p_qunhui_agc_param[gain_index][1]);
	log_inf("temp_gain = 0x%x, reg_gain = %d\n",
		temp_gain, reg_gain);

	return reg_gain;
}

static void night_mode(kal_bool enable)
{
	/* No Need to implement this function */
}

#ifdef MULTI_WRITE
kal_uint16 addr_data_pair_init_gc5035p_qunhui[] = {
	0xfc, 0x01,
	0xf4, 0x40,
	0xf5, 0xe9,
	0xf6, 0x14,
	0xf8, 0x40,
	0xf9, 0x82,
	0xfa, 0x00,
	0xfc, 0x81,
	0xfe, 0x00,
	0x36, 0x01,
	0xd3, 0x87,
	0x36, 0x00,
	0x33, 0x00,
	0xfe, 0x03,
	0x01, 0xe7,
	0xf7, 0x01,
	0xfc, 0x8f,
	0xfc, 0x8f,
	0xfc, 0x8e,
	0xfe, 0x00,
	0xee, 0x30,
	0x87, 0x18,
	0xfe, 0x01,
	0x8c, 0x90,
	0xfe, 0x00,
	0xfe, 0x00,
	0x05, 0x02,
	0x06, 0xda,
	0x9d, 0x0c,
	0x09, 0x00,
	0x0a, 0x04,
	0x0b, 0x00,
	0x0c, 0x03,
	0x0d, 0x07,
	0x0e, 0xa8,
	0x0f, 0x0a,
	0x10, 0x30,
	0x11, 0x02,
	0x17, GC5035P_QUNHUI_MIRROR,
	0x19, 0x05,
	0xfe, 0x02,
	0x30, 0x03,
	0x31, 0x03,
	0xfe, 0x00,
	0xd9, 0xc0,
	0x1b, 0x20,
	0x21, 0x48,
	0x28, 0x22,
	0x29, 0x58,
	0x44, 0x20,
	0x4b, 0x10,
	0x4e, 0x1a,
	0x50, 0x11,
	0x52, 0x33,
	0x53, 0x44,
	0x55, 0x10,
	0x5b, 0x11,
	0xc5, 0x02,
	0x8c, 0x1a,
	0xfe, 0x02,
	0x33, 0x05,
	0x32, 0x38,
	0xfe, 0x00,
	0x91, 0x80,
	0x92, 0x28,
	0x93, 0x20,
	0x95, 0xa0,
	0x96, 0xe0,
	0xd5, 0xfc,
	0x97, 0x28,
	0x16, 0x0c,
	0x1a, 0x1a,
	0x1f, 0x11,
	0x20, 0x10,
	0x46, 0x83,
	0x4a, 0x04,
	0x54, GC5035P_QUNHUI_RSTDUMMY1,
	0x62, 0x00,
	0x72, 0x8f,
	0x73, 0x89,
	0x7a, 0x05,
	0x7d, 0xcc,
	0x90, 0x00,
	0xce, 0x18,
	0xd0, 0xb2,
	0xd2, 0x40,
	0xe6, 0xe0,
	0xfe, 0x02,
	0x12, 0x01,
	0x13, 0x01,
	0x14, 0x01,
	0x15, 0x02,
	0x22, GC5035P_QUNHUI_RSTDUMMY2,
	0x91, 0x00,
	0x92, 0x00,
	0x93, 0x00,
	0x94, 0x00,
	0xfe, 0x00,
	0xfc, 0x88,
	0xfe, 0x10,
	0xfe, 0x00,
	0xfc, 0x8e,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfc, 0x88,
	0xfe, 0x10,
	0xfe, 0x00,
	0xfc, 0x8e,
	0xfe, 0x00,
	0xb0, 0x6e,
	0xb1, 0x01,
	0xb2, 0x00,
	0xb3, 0x00,
	0xb4, 0x00,
	0xb6, 0x00,
	0xfe, 0x01,
	0x53, 0x00,
	0x89, 0x03,
	0x60, 0x40,
	0xfe, 0x01,
	0x42, 0x21,
	0x49, 0x03,
	0x4a, 0xff,
	0x4b, 0xc0,
	0x55, 0x00,
	0xfe, 0x01,
	0x41, 0x28,
	0x4c, 0x00,
	0x4d, 0x00,
	0x4e, 0x3c,
	0x44, 0x08,
	0x48, 0x02,
	0xfe, 0x01,
	0x91, 0x00,
	0x92, 0x08,
	0x93, 0x00,
	0x94, 0x07,
	0x95, 0x07,
	0x96, 0x98,
	0x97, 0x0a,
	0x98, 0x20,
	0x99, 0x00,
	0xfe, 0x03,
	0x02, 0x57,
	0x03, 0xb7,
	0x15, 0x14,
	0x18, 0x0f,
	0x21, 0x22,
	0x22, 0x06,
	0x23, 0x48,
	0x24, 0x12,
	0x25, 0x28,
	0x26, 0x08,
	0x29, 0x06,
	0x2a, 0x58,
	0x2b, 0x08,
	0xfe, 0x01,
	0x8c, 0x10,
	0xfe, 0x00,
	0x3e, 0x01
};
#endif

static void sensor_init(void)
{
	log_inf("E\n");
#ifdef MULTI_WRITE
	gc5035p_qunhui_table_write_cmos_sensor(
		addr_data_pair_init_gc5035p_qunhui,
		sizeof(addr_data_pair_init_gc5035p_qunhui) /
		sizeof(kal_uint16));
#else
	/* SYSTEM */
	write_cmos_sensor(0xfc, 0x01);
	write_cmos_sensor(0xf4, 0x40);
	write_cmos_sensor(0xf5, 0xe9);
	write_cmos_sensor(0xf6, 0x14);
	write_cmos_sensor(0xf8, 0x40);
	write_cmos_sensor(0xf9, 0x82);
	write_cmos_sensor(0xfa, 0x00);
	write_cmos_sensor(0xfc, 0x81);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x36, 0x01);
	write_cmos_sensor(0xd3, 0x87);
	write_cmos_sensor(0x36, 0x00);
	write_cmos_sensor(0x33, 0x00);
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x01, 0xe7);
	write_cmos_sensor(0xf7, 0x01);
	write_cmos_sensor(0xfc, 0x8f);
	write_cmos_sensor(0xfc, 0x8f);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xee, 0x30);
	write_cmos_sensor(0x87, 0x18);
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x8c, 0x90);
	write_cmos_sensor(0xfe, 0x00);

	/* Analog & CISCTL */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x05, 0x02);
	write_cmos_sensor(0x06, 0xda);
	write_cmos_sensor(0x9d, 0x0c);
	write_cmos_sensor(0x09, 0x00);
	write_cmos_sensor(0x0a, 0x04);
	write_cmos_sensor(0x0b, 0x00);
	write_cmos_sensor(0x0c, 0x03);
	write_cmos_sensor(0x0d, 0x07);
	write_cmos_sensor(0x0e, 0xa8);
	write_cmos_sensor(0x0f, 0x0a);
	write_cmos_sensor(0x10, 0x30);
	write_cmos_sensor(0x11, 0x02);
	write_cmos_sensor(0x17, GC5035P_QUNHUI_MIRROR);
	write_cmos_sensor(0x19, 0x05);
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x30, 0x03);
	write_cmos_sensor(0x31, 0x03);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xd9, 0xc0);
	write_cmos_sensor(0x1b, 0x20);
	write_cmos_sensor(0x21, 0x48);
	write_cmos_sensor(0x28, 0x22);
	write_cmos_sensor(0x29, 0x58);
	write_cmos_sensor(0x44, 0x20);
	write_cmos_sensor(0x4b, 0x10);
	write_cmos_sensor(0x4e, 0x1a);
	write_cmos_sensor(0x50, 0x11);
	write_cmos_sensor(0x52, 0x33);
	write_cmos_sensor(0x53, 0x44);
	write_cmos_sensor(0x55, 0x10);
	write_cmos_sensor(0x5b, 0x11);
	write_cmos_sensor(0xc5, 0x02);
	write_cmos_sensor(0x8c, 0x1a);
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x33, 0x05);
	write_cmos_sensor(0x32, 0x38);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x91, 0x80);
	write_cmos_sensor(0x92, 0x28);
	write_cmos_sensor(0x93, 0x20);
	write_cmos_sensor(0x95, 0xa0);
	write_cmos_sensor(0x96, 0xe0);
	write_cmos_sensor(0xd5, 0xfc);
	write_cmos_sensor(0x97, 0x28);
	write_cmos_sensor(0x16, 0x0c);
	write_cmos_sensor(0x1a, 0x1a);
	write_cmos_sensor(0x1f, 0x11);
	write_cmos_sensor(0x20, 0x10);
	write_cmos_sensor(0x46, 0x83);
	write_cmos_sensor(0x4a, 0x04);
	write_cmos_sensor(0x54, GC5035P_QUNHUI_RSTDUMMY1);
	write_cmos_sensor(0x62, 0x00);
	write_cmos_sensor(0x72, 0x8f);
	write_cmos_sensor(0x73, 0x89);
	write_cmos_sensor(0x7a, 0x05);
	write_cmos_sensor(0x7d, 0xcc);
	write_cmos_sensor(0x90, 0x00);
	write_cmos_sensor(0xce, 0x18);
	write_cmos_sensor(0xd0, 0xb2);
	write_cmos_sensor(0xd2, 0x40);
	write_cmos_sensor(0xe6, 0xe0);
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x12, 0x01);
	write_cmos_sensor(0x13, 0x01);
	write_cmos_sensor(0x14, 0x01);
	write_cmos_sensor(0x15, 0x02);
	write_cmos_sensor(0x22, GC5035P_QUNHUI_RSTDUMMY2);
	write_cmos_sensor(0x91, 0x00);
	write_cmos_sensor(0x92, 0x00);
	write_cmos_sensor(0x93, 0x00);
	write_cmos_sensor(0x94, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x88);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x88);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x8e);

	/* Gain */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xb0, 0x6e);
	write_cmos_sensor(0xb1, 0x01);
	write_cmos_sensor(0xb2, 0x00);
	write_cmos_sensor(0xb3, 0x00);
	write_cmos_sensor(0xb4, 0x00);
	write_cmos_sensor(0xb6, 0x00);

	/* ISP */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x53, 0x00);
	write_cmos_sensor(0x89, 0x03);
	write_cmos_sensor(0x60, 0x40);

	/* BLK */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x42, 0x21);
	write_cmos_sensor(0x49, 0x03);
	write_cmos_sensor(0x4a, 0xff);
	write_cmos_sensor(0x4b, 0xc0);
	write_cmos_sensor(0x55, 0x00);

	/* Anti_blooming */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x41, 0x28);
	write_cmos_sensor(0x4c, 0x00);
	write_cmos_sensor(0x4d, 0x00);
	write_cmos_sensor(0x4e, 0x3c);
	write_cmos_sensor(0x44, 0x08);
	write_cmos_sensor(0x48, 0x02);

	/* Crop */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x91, 0x00);
	write_cmos_sensor(0x92, 0x08);
	write_cmos_sensor(0x93, 0x00);
	write_cmos_sensor(0x94, 0x07);
	write_cmos_sensor(0x95, 0x07);
	write_cmos_sensor(0x96, 0x98);
	write_cmos_sensor(0x97, 0x0a);
	write_cmos_sensor(0x98, 0x20);
	write_cmos_sensor(0x99, 0x00);

	/* MIPI */
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x02, 0x57);
	write_cmos_sensor(0x03, 0xb7);
	write_cmos_sensor(0x15, 0x14);
	write_cmos_sensor(0x18, 0x0f);
	write_cmos_sensor(0x21, 0x22);
	write_cmos_sensor(0x22, 0x06);
	write_cmos_sensor(0x23, 0x48);
	write_cmos_sensor(0x24, 0x12);
	write_cmos_sensor(0x25, 0x28);
	write_cmos_sensor(0x26, 0x08);
	write_cmos_sensor(0x29, 0x06);
	write_cmos_sensor(0x2a, 0x58);
	write_cmos_sensor(0x2b, 0x08);
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x8c, 0x10);

	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x3e, 0x01);
#endif
}

#ifdef MULTI_WRITE
kal_uint16 addr_data_pair_preview_gc5035p_qunhui[] = {
	0xfc, 0x01,
	0xf4, 0x40,
	0xf5, 0xe4,
	0xf6, 0x14,
	0xf8, 0x40,
	0xf9, 0x12,
	0xfa, 0x01,
	0xfc, 0x81,
	0xfe, 0x00,
	0x36, 0x01,
	0xd3, 0x87,
	0x36, 0x00,
	0x33, 0x20,
	0xfe, 0x03,
	0x01, 0x87,
	0xf7, 0x11,
	0xfc, 0x8f,
	0xfc, 0x8f,
	0xfc, 0x8e,
	0xfe, 0x00,
	0xee, 0x30,
	0x87, 0x18,
	0xfe, 0x01,
	0x8c, 0x90,
	0xfe, 0x00,
	0xfe, 0x00,
	0x05, 0x02,
	0x06, 0xda,
	0x9d, 0x0c,
	0x09, 0x00,
	0x0a, 0x04,
	0x0b, 0x00,
	0x0c, 0x03,
	0x0d, 0x07,
	0x0e, 0xa8,
	0x0f, 0x0a,
	0x10, 0x30,
	0x21, 0x60,
	0x29, 0x30,
	0x44, 0x18,
	0x4e, 0x20,
	0x8c, 0x20,
	0x91, 0x15,
	0x92, 0x3a,
	0x93, 0x20,
	0x95, 0x45,
	0x96, 0x35,
	0xd5, 0xf0,
	0x97, 0x20,
	0x1f, 0x19,
	0xce, 0x18,
	0xd0, 0xb3,
	0xfe, 0x02,
	0x14, 0x02,
	0x15, 0x00,
	0xfe, 0x00,
	0xfc, 0x88,
	0xfe, 0x10,
	0xfe, 0x00,
	0xfc, 0x8e,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfc, 0x88,
	0xfe, 0x10,
	0xfe, 0x00,
	0xfc, 0x8e,
	0xfe, 0x01,
	0x49, 0x00,
	0x4a, 0x01,
	0x4b, 0xf8,
	0xfe, 0x01,
	0x4e, 0x06,
	0x44, 0x02,
	0xfe, 0x01,
	0x91, 0x00,
	0x92, 0x04,
	0x93, 0x00,
	0x94, 0x03,
	0x95, 0x03,
	0x96, 0xcc,
	0x97, 0x05,
	0x98, 0x10,
	0x99, 0x00,
	0xfe, 0x03,
	0x02, 0x59,
	0x22, 0x03,
	0x26, 0x06,
	0x29, 0x03,
	0x2b, 0x06,
	0xfe, 0x01,
	0x8c, 0x10
};
#endif

static void preview_setting(void)
{
	log_inf("E\n");
#ifdef MULTI_WRITE
	gc5035p_qunhui_table_write_cmos_sensor(
		addr_data_pair_preview_gc5035p_qunhui,
		sizeof(addr_data_pair_preview_gc5035p_qunhui) /
		sizeof(kal_uint16));
#else
	/* System */
	write_cmos_sensor(0xfc, 0x01);
	write_cmos_sensor(0xf4, 0x40);
	write_cmos_sensor(0xf5, 0xe4);
	write_cmos_sensor(0xf6, 0x14);
	write_cmos_sensor(0xf8, 0x40);
	write_cmos_sensor(0xf9, 0x12);
	write_cmos_sensor(0xfa, 0x01);
	write_cmos_sensor(0xfc, 0x81);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x36, 0x01);
	write_cmos_sensor(0xd3, 0x87);
	write_cmos_sensor(0x36, 0x00);
	write_cmos_sensor(0x33, 0x20);
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x01, 0x87);
	write_cmos_sensor(0xf7, 0x11);
	write_cmos_sensor(0xfc, 0x8f);
	write_cmos_sensor(0xfc, 0x8f);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xee, 0x30);
	write_cmos_sensor(0x87, 0x18);
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x8c, 0x90);
	write_cmos_sensor(0xfe, 0x00);

	/* Analog & CISCTL */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x05, 0x02);
	write_cmos_sensor(0x06, 0xda);
	write_cmos_sensor(0x9d, 0x0c);
	write_cmos_sensor(0x09, 0x00);
	write_cmos_sensor(0x0a, 0x04);
	write_cmos_sensor(0x0b, 0x00);
	write_cmos_sensor(0x0c, 0x03);
	write_cmos_sensor(0x0d, 0x07);
	write_cmos_sensor(0x0e, 0xa8);
	write_cmos_sensor(0x0f, 0x0a);
	write_cmos_sensor(0x10, 0x30);
	write_cmos_sensor(0x21, 0x60);
	write_cmos_sensor(0x29, 0x30);
	write_cmos_sensor(0x44, 0x18);
	write_cmos_sensor(0x4e, 0x20);
	write_cmos_sensor(0x8c, 0x20);
	write_cmos_sensor(0x91, 0x15);
	write_cmos_sensor(0x92, 0x3a);
	write_cmos_sensor(0x93, 0x20);
	write_cmos_sensor(0x95, 0x45);
	write_cmos_sensor(0x96, 0x35);
	write_cmos_sensor(0xd5, 0xf0);
	write_cmos_sensor(0x97, 0x20);
	write_cmos_sensor(0x1f, 0x19);
	write_cmos_sensor(0xce, 0x18);
	write_cmos_sensor(0xd0, 0xb3);
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x14, 0x02);
	write_cmos_sensor(0x15, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x88);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x88);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x8e);

	/* BLK */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x49, 0x00);
	write_cmos_sensor(0x4a, 0x01);
	write_cmos_sensor(0x4b, 0xf8);

	/* Anti_blooming */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x4e, 0x06);
	write_cmos_sensor(0x44, 0x02);

	/* Crop */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x91, 0x00);
	write_cmos_sensor(0x92, 0x04);
	write_cmos_sensor(0x93, 0x00);
	write_cmos_sensor(0x94, 0x03);
	write_cmos_sensor(0x95, 0x03);
	write_cmos_sensor(0x96, 0xcc);
	write_cmos_sensor(0x97, 0x05);
	write_cmos_sensor(0x98, 0x10);
	write_cmos_sensor(0x99, 0x00);

	/* MIPI */
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x02, 0x59);
	write_cmos_sensor(0x22, 0x03);
	write_cmos_sensor(0x26, 0x06);
	write_cmos_sensor(0x29, 0x03);
	write_cmos_sensor(0x2b, 0x06);
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x8c, 0x10);
#endif
}

#ifdef MULTI_WRITE
kal_uint16 addr_data_pair_capture_24fps_gc5035p_qunhui[] = {
	0xfc, 0x01,
	0xf4, 0x40,
	0xf5, 0xe7,
	0xf6, 0x14,
	0xf8, 0x3b,
	0xf9, 0x82,
	0xfa, 0x00,
	0xfc, 0x81,
	0xfe, 0x00,
	0x36, 0x01,
	0xd3, 0x87,
	0x36, 0x00,
	0x33, 0x00,
	0xfe, 0x03,
	0x01, 0xe7,
	0xf7, 0x01,
	0xfc, 0x8f,
	0xfc, 0x8f,
	0xfc, 0x8e,
	0xfe, 0x00,
	0xee, 0x30,
	0x87, 0x18,
	0xfe, 0x01,
	0x8c, 0x90,
	0xfe, 0x00,
	0xfe, 0x00,
	0x05, 0x02,
	0x06, 0xda,
	0x9d, 0x0c,
	0x09, 0x00,
	0x0a, 0x04,
	0x0b, 0x00,
	0x0c, 0x03,
	0x0d, 0x07,
	0x0e, 0xa8,
	0x0f, 0x0a,
	0x10, 0x30,
	0x21, 0x48,
	0x29, 0x58,
	0x44, 0x20,
	0x4e, 0x1a,
	0x8c, 0x1a,
	0x91, 0x80,
	0x92, 0x28,
	0x93, 0x20,
	0x95, 0xa0,
	0x96, 0xe0,
	0xd5, 0xfc,
	0x97, 0x28,
	0x1f, 0x11,
	0xce, 0x18,
	0xd0, 0xb2,
	0xfe, 0x02,
	0x14, 0x01,
	0x15, 0x02,
	0xfe, 0x00,
	0xfc, 0x88,
	0xfe, 0x10,
	0xfe, 0x00,
	0xfc, 0x8e,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfc, 0x88,
	0xfe, 0x10,
	0xfe, 0x00,
	0xfc, 0x8e,
	0xfe, 0x01,
	0x49, 0x03,
	0x4a, 0xff,
	0x4b, 0xc0,
	0xfe, 0x01,
	0x4e, 0x3c,
	0x44, 0x08,
	0xfe, 0x01,
	0x91, 0x00,
	0x92, 0x08,
	0x93, 0x00,
	0x94, 0x07,
	0x95, 0x07,
	0x96, 0x98,
	0x97, 0x0a,
	0x98, 0x20,
	0x99, 0x00,
	0xfe, 0x03,
	0x02, 0x59,
	0x22, 0x06,
	0x26, 0x08,
	0x29, 0x06,
	0x2b, 0x08,
	0xfe, 0x01,
	0x8c, 0x10
};

kal_uint16 addr_data_pair_capture_fps_gc5035p_qunhui[] = {
	0xfc, 0x01,
	0xf4, 0x40,
	0xf5, 0xe9,
	0xf6, 0x14,
	0xf8, 0x40,
	0xf9, 0x82,
	0xfa, 0x00,
	0xfc, 0x81,
	0xfe, 0x00,
	0x36, 0x01,
	0xd3, 0x87,
	0x36, 0x00,
	0x33, 0x00,
	0xfe, 0x03,
	0x01, 0xe7,
	0xf7, 0x01,
	0xfc, 0x8f,
	0xfc, 0x8f,
	0xfc, 0x8e,
	0xfe, 0x00,
	0xee, 0x30,
	0x87, 0x18,
	0xfe, 0x01,
	0x8c, 0x90,
	0xfe, 0x00,
	0xfe, 0x00,
	0x05, 0x02,
	0x06, 0xda,
	0x9d, 0x0c,
	0x09, 0x00,
	0x0a, 0x04,
	0x0b, 0x00,
	0x0c, 0x03,
	0x0d, 0x07,
	0x0e, 0xa8,
	0x0f, 0x0a,
	0x10, 0x30,
	0x21, 0x48,
	0x29, 0x58,
	0x44, 0x20,
	0x4e, 0x1a,
	0x8c, 0x1a,
	0x91, 0x80,
	0x92, 0x28,
	0x93, 0x20,
	0x95, 0xa0,
	0x96, 0xe0,
	0xd5, 0xfc,
	0x97, 0x28,
	0x1f, 0x11,
	0xce, 0x18,
	0xd0, 0xb2,
	0xfe, 0x02,
	0x14, 0x01,
	0x15, 0x02,
	0xfe, 0x00,
	0xfc, 0x88,
	0xfe, 0x10,
	0xfe, 0x00,
	0xfc, 0x8e,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfc, 0x88,
	0xfe, 0x10,
	0xfe, 0x00,
	0xfc, 0x8e,
	0xfe, 0x01,
	0x49, 0x03,
	0x4a, 0xff,
	0x4b, 0xc0,
	0xfe, 0x01,
	0x4e, 0x3c,
	0x44, 0x08,
	0xfe, 0x01,
	0x91, 0x00,
	0x92, 0x08,
	0x93, 0x00,
	0x94, 0x07,
	0x95, 0x07,
	0x96, 0x98,
	0x97, 0x0a,
	0x98, 0x20,
	0x99, 0x00,
	0xfe, 0x03,
	0x02, 0x59,
	0x22, 0x06,
	0x26, 0x08,
	0x29, 0x06,
	0x2b, 0x08,
	0xfe, 0x01,
	0x8c, 0x10
};
#endif


static void capture_setting(kal_uint16 currefps)
{
	log_inf("E! currefps: %d\n", currefps);

#ifdef MULTI_WRITE
	if (currefps == 240)
		gc5035p_qunhui_table_write_cmos_sensor(
			addr_data_pair_capture_24fps_gc5035p_qunhui,
			sizeof(addr_data_pair_capture_24fps_gc5035p_qunhui) /
			sizeof(kal_uint16));
	else
		gc5035p_qunhui_table_write_cmos_sensor(
			addr_data_pair_capture_fps_gc5035p_qunhui,
			sizeof(addr_data_pair_capture_fps_gc5035p_qunhui) /
			sizeof(kal_uint16));
#else
	/* System */
	write_cmos_sensor(0xfc, 0x01);
	write_cmos_sensor(0xf4, 0x40);
	if (currefps == 240) { /* PIP */
		write_cmos_sensor(0xf5, 0xe7);
		write_cmos_sensor(0xf6, 0x14);
		write_cmos_sensor(0xf8, 0x3b);
	} else {
		write_cmos_sensor(0xf5, 0xe9);
		write_cmos_sensor(0xf6, 0x14);
		write_cmos_sensor(0xf8, 0x40);
	}
	write_cmos_sensor(0xf9, 0x82);
	write_cmos_sensor(0xfa, 0x00);
	write_cmos_sensor(0xfc, 0x81);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x36, 0x01);
	write_cmos_sensor(0xd3, 0x87);
	write_cmos_sensor(0x36, 0x00);
	write_cmos_sensor(0x33, 0x00);
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x01, 0xe7);
	write_cmos_sensor(0xf7, 0x01);
	write_cmos_sensor(0xfc, 0x8f);
	write_cmos_sensor(0xfc, 0x8f);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xee, 0x30);
	write_cmos_sensor(0x87, 0x18);
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x8c, 0x90);
	write_cmos_sensor(0xfe, 0x00);

	/* Analog & CISCTL */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x05, 0x02);
	write_cmos_sensor(0x06, 0xda);
	write_cmos_sensor(0x9d, 0x0c);
	write_cmos_sensor(0x09, 0x00);
	write_cmos_sensor(0x0a, 0x04);
	write_cmos_sensor(0x0b, 0x00);
	write_cmos_sensor(0x0c, 0x03);
	write_cmos_sensor(0x0d, 0x07);
	write_cmos_sensor(0x0e, 0xa8);
	write_cmos_sensor(0x0f, 0x0a);
	write_cmos_sensor(0x10, 0x30);
	write_cmos_sensor(0x21, 0x48);
	write_cmos_sensor(0x29, 0x58);
	write_cmos_sensor(0x44, 0x20);
	write_cmos_sensor(0x4e, 0x1a);
	write_cmos_sensor(0x8c, 0x1a);
	write_cmos_sensor(0x91, 0x80);
	write_cmos_sensor(0x92, 0x28);
	write_cmos_sensor(0x93, 0x20);
	write_cmos_sensor(0x95, 0xa0);
	write_cmos_sensor(0x96, 0xe0);
	write_cmos_sensor(0xd5, 0xfc);
	write_cmos_sensor(0x97, 0x28);
	write_cmos_sensor(0x1f, 0x11);
	write_cmos_sensor(0xce, 0x18);
	write_cmos_sensor(0xd0, 0xb2);
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x14, 0x01);
	write_cmos_sensor(0x15, 0x02);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x88);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x88);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x8e);

	/* BLK */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x49, 0x03);
	write_cmos_sensor(0x4a, 0xff);
	write_cmos_sensor(0x4b, 0xc0);

	/* Anti_blooming */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x4e, 0x3c);
	write_cmos_sensor(0x44, 0x08);

	/* Crop */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x91, 0x00);
	write_cmos_sensor(0x92, 0x08);
	write_cmos_sensor(0x93, 0x00);
	write_cmos_sensor(0x94, 0x07);
	write_cmos_sensor(0x95, 0x07);
	write_cmos_sensor(0x96, 0x98);
	write_cmos_sensor(0x97, 0x0a);
	write_cmos_sensor(0x98, 0x20);
	write_cmos_sensor(0x99, 0x00);

	/* MIPI */
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x02, 0x59);
	write_cmos_sensor(0x22, 0x06);
	write_cmos_sensor(0x26, 0x08);
	write_cmos_sensor(0x29, 0x06);
	write_cmos_sensor(0x2b, 0x08);
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x8c, 0x10);
#endif
}

#ifdef MULTI_WRITE
kal_uint16 addr_data_pair_normal_video_gc5035p_qunhui[] = {
	0xfc, 0x01,
	0xf4, 0x40,
	0xf5, 0xe9,
	0xf6, 0x14,
	0xf8, 0x40,
	0xf9, 0x82,
	0xfa, 0x00,
	0xfc, 0x81,
	0xfe, 0x00,
	0x36, 0x01,
	0xd3, 0x87,
	0x36, 0x00,
	0x33, 0x00,
	0xfe, 0x03,
	0x01, 0xe7,
	0xf7, 0x01,
	0xfc, 0x8f,
	0xfc, 0x8f,
	0xfc, 0x8e,
	0xfe, 0x00,
	0xee, 0x30,
	0x87, 0x18,
	0xfe, 0x01,
	0x8c, 0x90,
	0xfe, 0x00,
	0xfe, 0x00,
	0x05, 0x02,
	0x06, 0xda,
	0x9d, 0x0c,
	0x09, 0x00,
	0x0a, 0x04,
	0x0b, 0x00,
	0x0c, 0x03,
	0x0d, 0x07,
	0x0e, 0xa8,
	0x0f, 0x0a,
	0x10, 0x30,
	0x21, 0x48,
	0x29, 0x58,
	0x44, 0x20,
	0x4e, 0x1a,
	0x8c, 0x1a,
	0x91, 0x80,
	0x92, 0x28,
	0x93, 0x20,
	0x95, 0xa0,
	0x96, 0xe0,
	0xd5, 0xfc,
	0x97, 0x28,
	0x1f, 0x11,
	0xce, 0x18,
	0xd0, 0xb2,
	0xfe, 0x02,
	0x14, 0x01,
	0x15, 0x02,
	0xfe, 0x00,
	0xfc, 0x88,
	0xfe, 0x10,
	0xfe, 0x00,
	0xfc, 0x8e,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfc, 0x88,
	0xfe, 0x10,
	0xfe, 0x00,
	0xfc, 0x8e,
	0xfe, 0x01,
	0x49, 0x03,
	0x4a, 0xff,
	0x4b, 0xc0,
	0xfe, 0x01,
	0x4e, 0x3c,
	0x44, 0x08,
	0xfe, 0x01,
	0x91, 0x00,
	0x92, 0x08,
	0x93, 0x00,
	0x94, 0x07,
	0x95, 0x07,
	0x96, 0x98,
	0x97, 0x0a,
	0x98, 0x20,
	0x99, 0x00,
	0xfe, 0x03,
	0x02, 0x59,
	0x22, 0x06,
	0x26, 0x08,
	0x29, 0x06,
	0x2b, 0x08,
	0xfe, 0x01,
	0x8c, 0x10
};
#endif

static void normal_video_setting(kal_uint16 currefps)
{
	log_inf("E! currefps: %d\n", currefps);
#ifdef MULTI_WRITE
	gc5035p_qunhui_table_write_cmos_sensor(
		addr_data_pair_normal_video_gc5035p_qunhui,
		sizeof(addr_data_pair_normal_video_gc5035p_qunhui) /
		sizeof(kal_uint16));
#else
	/* System */
	write_cmos_sensor(0xfc, 0x01);
	write_cmos_sensor(0xf4, 0x40);
	write_cmos_sensor(0xf5, 0xe4);
	write_cmos_sensor(0xf6, 0x14);
	write_cmos_sensor(0xf8, 0x40);
	write_cmos_sensor(0xf9, 0x12);
	write_cmos_sensor(0xfa, 0x01);
	write_cmos_sensor(0xfc, 0x81);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x36, 0x01);
	write_cmos_sensor(0xd3, 0x87);
	write_cmos_sensor(0x36, 0x00);
	write_cmos_sensor(0x33, 0x20);
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x01, 0x87);
	write_cmos_sensor(0xf7, 0x11);
	write_cmos_sensor(0xfc, 0x8f);
	write_cmos_sensor(0xfc, 0x8f);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xee, 0x30);
	write_cmos_sensor(0x87, 0x18);
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x8c, 0x90);
	write_cmos_sensor(0xfe, 0x00);

	/* Analog & CISCTL */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x05, 0x02);
	write_cmos_sensor(0x06, 0xda);
	write_cmos_sensor(0x9d, 0x0c);
	write_cmos_sensor(0x09, 0x00);
	write_cmos_sensor(0x0a, 0x04);
	write_cmos_sensor(0x0b, 0x00);
	write_cmos_sensor(0x0c, 0x03);
	write_cmos_sensor(0x0d, 0x07);
	write_cmos_sensor(0x0e, 0xa8);
	write_cmos_sensor(0x0f, 0x0a);
	write_cmos_sensor(0x10, 0x30);
	write_cmos_sensor(0x21, 0x60);
	write_cmos_sensor(0x29, 0x30);
	write_cmos_sensor(0x44, 0x18);
	write_cmos_sensor(0x4e, 0x20);
	write_cmos_sensor(0x8c, 0x20);
	write_cmos_sensor(0x91, 0x15);
	write_cmos_sensor(0x92, 0x3a);
	write_cmos_sensor(0x93, 0x20);
	write_cmos_sensor(0x95, 0x45);
	write_cmos_sensor(0x96, 0x35);
	write_cmos_sensor(0xd5, 0xf0);
	write_cmos_sensor(0x97, 0x20);
	write_cmos_sensor(0x1f, 0x19);
	write_cmos_sensor(0xce, 0x18);
	write_cmos_sensor(0xd0, 0xb3);
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x14, 0x02);
	write_cmos_sensor(0x15, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x88);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x88);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x8e);

	/* BLK */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x49, 0x00);
	write_cmos_sensor(0x4a, 0x01);
	write_cmos_sensor(0x4b, 0xf8);

	/* Anti_blooming */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x4e, 0x06);
	write_cmos_sensor(0x44, 0x02);

	/* Crop */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x91, 0x00);
	write_cmos_sensor(0x92, 0x04);
	write_cmos_sensor(0x93, 0x00);
	write_cmos_sensor(0x94, 0x03);
	write_cmos_sensor(0x95, 0x03);
	write_cmos_sensor(0x96, 0xcc);
	write_cmos_sensor(0x97, 0x05);
	write_cmos_sensor(0x98, 0x10);
	write_cmos_sensor(0x99, 0x00);

	/* MIPI */
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x02, 0x59);
	write_cmos_sensor(0x22, 0x03);
	write_cmos_sensor(0x26, 0x06);
	write_cmos_sensor(0x29, 0x03);
	write_cmos_sensor(0x2b, 0x06);
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x8c, 0x10);
	#endif
}

#ifdef MULTI_WRITE
kal_uint16 addr_data_pair_hs_video_gc5035p_qunhui[] = {
	0xfc, 0x01,
	0xf4, 0x40,
	0xf5, 0xe9,
	0xf6, 0x14,
	0xf8, 0x40,
	0xf9, 0x82,
	0xfa, 0x00,
	0xfc, 0x81,
	0xfe, 0x00,
	0x36, 0x01,
	0xd3, 0x87,
	0x36, 0x00,
	0x33, 0x20,
	0xfe, 0x03,
	0x01, 0x87,
	0xf7, 0x11,
	0xfc, 0x8f,
	0xfc, 0x8f,
	0xfc, 0x8e,
	0xfe, 0x00,
	0xee, 0x30,
	0x87, 0x18,
	0xfe, 0x01,
	0x8c, 0x90,
	0xfe, 0x00,
	0xfe, 0x00,
	0x05, 0x03,
	0x06, 0xb4,
	0x9d, 0x20,
	0x09, 0x00,
	0x0a, 0xf4,
	0x0b, 0x00,
	0x0c, 0x03,
	0x0d, 0x05,
	0x0e, 0xc8,
	0x0f, 0x0a,
	0x10, 0x30,
	0xd9, 0xf8,
	0x21, 0xe0,
	0x29, 0x40,
	0x44, 0x30,
	0x4e, 0x20,
	0x8c, 0x20,
	0x91, 0x15,
	0x92, 0x3a,
	0x93, 0x20,
	0x95, 0x45,
	0x96, 0x35,
	0xd5, 0xf0,
	0x97, 0x20,
	0x1f, 0x19,
	0xce, 0x18,
	0xd0, 0xb3,
	0xfe, 0x02,
	0x14, 0x02,
	0x15, 0x00,
	0x91, 0x00,
	0x92, 0xf0,
	0x93, 0x00,
	0x94, 0x00,
	0xfe, 0x00,
	0xfc, 0x88,
	0xfe, 0x10,
	0xfe, 0x00,
	0xfc, 0x8e,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfc, 0x88,
	0xfe, 0x10,
	0xfe, 0x00,
	0xfc, 0x8e,
	0xfe, 0x01,
	0x49, 0x00,
	0x4a, 0x01,
	0x4b, 0xf8,
	0xfe, 0x01,
	0x4e, 0x06,
	0x44, 0x02,
	0xfe, 0x01,
	0x91, 0x00,
	0x92, 0x0a,
	0x93, 0x00,
	0x94, 0x0b,
	0x95, 0x02,
	0x96, 0xd0,
	0x97, 0x05,
	0x98, 0x00,
	0x99, 0x00,
	0xfe, 0x03,
	0x02, 0x59,
	0x22, 0x03,
	0x26, 0x06,
	0x29, 0x03,
	0x2b, 0x06,
	0xfe, 0x01,
	0x8c, 0x10
};
#endif

static void hs_video_setting(void)
{
	log_inf("E\n");
#ifdef MULTI_WRITE
	gc5035p_qunhui_table_write_cmos_sensor(
		addr_data_pair_hs_video_gc5035p_qunhui,
		sizeof(addr_data_pair_hs_video_gc5035p_qunhui) /
		sizeof(kal_uint16));
#else
	/* System */
	write_cmos_sensor(0xfc, 0x01);
	write_cmos_sensor(0xf4, 0x40);
	write_cmos_sensor(0xf5, 0xe9);
	write_cmos_sensor(0xf6, 0x14);
	write_cmos_sensor(0xf8, 0x40);
	write_cmos_sensor(0xf9, 0x82);
	write_cmos_sensor(0xfa, 0x00);
	write_cmos_sensor(0xfc, 0x81);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x36, 0x01);
	write_cmos_sensor(0xd3, 0x87);
	write_cmos_sensor(0x36, 0x00);
	write_cmos_sensor(0x33, 0x20);
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x01, 0x87);
	write_cmos_sensor(0xf7, 0x11);
	write_cmos_sensor(0xfc, 0x8f);
	write_cmos_sensor(0xfc, 0x8f);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xee, 0x30);
	write_cmos_sensor(0x87, 0x18);
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x8c, 0x90);
	write_cmos_sensor(0xfe, 0x00);

	/* Analog & CISCTL */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x05, 0x03);
	write_cmos_sensor(0x06, 0xb4);
	write_cmos_sensor(0x9d, 0x20);
	write_cmos_sensor(0x09, 0x00);
	write_cmos_sensor(0x0a, 0xf4);
	write_cmos_sensor(0x0b, 0x00);
	write_cmos_sensor(0x0c, 0x03);
	write_cmos_sensor(0x0d, 0x05);
	write_cmos_sensor(0x0e, 0xc8);
	write_cmos_sensor(0x0f, 0x0a);
	write_cmos_sensor(0x10, 0x30);
	write_cmos_sensor(0xd9, 0xf8);
	write_cmos_sensor(0x21, 0xe0);
	write_cmos_sensor(0x29, 0x40);
	write_cmos_sensor(0x44, 0x30);
	write_cmos_sensor(0x4e, 0x20);
	write_cmos_sensor(0x8c, 0x20);
	write_cmos_sensor(0x91, 0x15);
	write_cmos_sensor(0x92, 0x3a);
	write_cmos_sensor(0x93, 0x20);
	write_cmos_sensor(0x95, 0x45);
	write_cmos_sensor(0x96, 0x35);
	write_cmos_sensor(0xd5, 0xf0);
	write_cmos_sensor(0x97, 0x20);
	write_cmos_sensor(0x1f, 0x19);
	write_cmos_sensor(0xce, 0x18);
	write_cmos_sensor(0xd0, 0xb3);
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x14, 0x02);
	write_cmos_sensor(0x15, 0x00);
	write_cmos_sensor(0x91, 0x00);
	write_cmos_sensor(0x92, 0xf0);
	write_cmos_sensor(0x93, 0x00);
	write_cmos_sensor(0x94, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x88);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x88);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x8e);

	/* BLK */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x49, 0x00);
	write_cmos_sensor(0x4a, 0x01);
	write_cmos_sensor(0x4b, 0xf8);

	/* Anti_blooming */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x4e, 0x06);
	write_cmos_sensor(0x44, 0x02);

	/* Crop */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x91, 0x00);
	write_cmos_sensor(0x92, 0x0a);
	write_cmos_sensor(0x93, 0x00);
	write_cmos_sensor(0x94, 0x0b);
	write_cmos_sensor(0x95, 0x02);
	write_cmos_sensor(0x96, 0xd0);
	write_cmos_sensor(0x97, 0x05);
	write_cmos_sensor(0x98, 0x00);
	write_cmos_sensor(0x99, 0x00);

	/* MIPI */
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x02, 0x59);
	write_cmos_sensor(0x22, 0x03);
	write_cmos_sensor(0x26, 0x06);
	write_cmos_sensor(0x29, 0x03);
	write_cmos_sensor(0x2b, 0x06);
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x8c, 0x10);
#endif
}

#ifdef MULTI_WRITE
kal_uint16 addr_data_pair_slim_video_gc5035p_qunhui[] = {
	0xfc, 0x01,
	0xf4, 0x40,
	0xf5, 0xe4,
	0xf6, 0x14,
	0xf8, 0x40,
	0xf9, 0x12,
	0xfa, 0x01,
	0xfc, 0x81,
	0xfe, 0x00,
	0x36, 0x01,
	0xd3, 0x87,
	0x36, 0x00,
	0x33, 0x20,
	0xfe, 0x03,
	0x01, 0x87,
	0xf7, 0x11,
	0xfc, 0x8f,
	0xfc, 0x8f,
	0xfc, 0x8e,
	0xfe, 0x00,
	0xee, 0x30,
	0x87, 0x18,
	0xfe, 0x01,
	0x8c, 0x90,
	0xfe, 0x00,
	0xfe, 0x00,
	0x05, 0x02,
	0x06, 0xda,
	0x9d, 0x0c,
	0x09, 0x00,
	0x0a, 0x04,
	0x0b, 0x00,
	0x0c, 0x03,
	0x0d, 0x07,
	0x0e, 0xa8,
	0x0f, 0x0a,
	0x10, 0x30,
	0x21, 0x60,
	0x29, 0x30,
	0x44, 0x18,
	0x4e, 0x20,
	0x8c, 0x20,
	0x91, 0x15,
	0x92, 0x3a,
	0x93, 0x20,
	0x95, 0x45,
	0x96, 0x35,
	0xd5, 0xf0,
	0x97, 0x20,
	0x1f, 0x19,
	0xce, 0x18,
	0xd0, 0xb3,
	0xfe, 0x02,
	0x14, 0x02,
	0x15, 0x00,
	0xfe, 0x00,
	0xfc, 0x88,
	0xfe, 0x10,
	0xfe, 0x00,
	0xfc, 0x8e,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfc, 0x88,
	0xfe, 0x10,
	0xfe, 0x00,
	0xfc, 0x8e,
	0xfe, 0x01,
	0x49, 0x00,
	0x4a, 0x01,
	0x4b, 0xf8,
	0xfe, 0x01,
	0x4e, 0x06,
	0x44, 0x02,
	0xfe, 0x01,
	0x91, 0x00,
	0x92, 0x0a,
	0x93, 0x00,
	0x94, 0x0b,
	0x95, 0x02,
	0x96, 0xd0,
	0x97, 0x05,
	0x98, 0x00,
	0x99, 0x00,
	0xfe, 0x03,
	0x02, 0x59,
	0x22, 0x03,
	0x26, 0x06,
	0x29, 0x03,
	0x2b, 0x06,
	0xfe, 0x01,
	0x8c, 0x10
};
#endif

static void slim_video_setting(void)
{
	log_inf("E\n");
#ifdef MULTI_WRITE
	gc5035p_qunhui_table_write_cmos_sensor(
		addr_data_pair_hs_video_gc5035p_qunhui,
		sizeof(addr_data_pair_hs_video_gc5035p_qunhui) /
		sizeof(kal_uint16));
#else
	/* System */
	write_cmos_sensor(0xfc, 0x01);
	write_cmos_sensor(0xf4, 0x40);
	write_cmos_sensor(0xf5, 0xe4);
	write_cmos_sensor(0xf6, 0x14);
	write_cmos_sensor(0xf8, 0x40);
	write_cmos_sensor(0xf9, 0x12);
	write_cmos_sensor(0xfa, 0x01);
	write_cmos_sensor(0xfc, 0x81);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x36, 0x01);
	write_cmos_sensor(0xd3, 0x87);
	write_cmos_sensor(0x36, 0x00);
	write_cmos_sensor(0x33, 0x20);
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x01, 0x87);
	write_cmos_sensor(0xf7, 0x11);
	write_cmos_sensor(0xfc, 0x8f);
	write_cmos_sensor(0xfc, 0x8f);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xee, 0x30);
	write_cmos_sensor(0x87, 0x18);
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x8c, 0x90);
	write_cmos_sensor(0xfe, 0x00);

	/* Analog & CISCTL */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x05, 0x02);
	write_cmos_sensor(0x06, 0xda);
	write_cmos_sensor(0x9d, 0x0c);
	write_cmos_sensor(0x09, 0x00);
	write_cmos_sensor(0x0a, 0x04);
	write_cmos_sensor(0x0b, 0x00);
	write_cmos_sensor(0x0c, 0x03);
	write_cmos_sensor(0x0d, 0x07);
	write_cmos_sensor(0x0e, 0xa8);
	write_cmos_sensor(0x0f, 0x0a);
	write_cmos_sensor(0x10, 0x30);
	write_cmos_sensor(0x21, 0x60);
	write_cmos_sensor(0x29, 0x30);
	write_cmos_sensor(0x44, 0x18);
	write_cmos_sensor(0x4e, 0x20);
	write_cmos_sensor(0x8c, 0x20);
	write_cmos_sensor(0x91, 0x15);
	write_cmos_sensor(0x92, 0x3a);
	write_cmos_sensor(0x93, 0x20);
	write_cmos_sensor(0x95, 0x45);
	write_cmos_sensor(0x96, 0x35);
	write_cmos_sensor(0xd5, 0xf0);
	write_cmos_sensor(0x97, 0x20);
	write_cmos_sensor(0x1f, 0x19);
	write_cmos_sensor(0xce, 0x18);
	write_cmos_sensor(0xd0, 0xb3);
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x14, 0x02);
	write_cmos_sensor(0x15, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x88);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x88);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x8e);

	/* BLK */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x49, 0x00);
	write_cmos_sensor(0x4a, 0x01);
	write_cmos_sensor(0x4b, 0xf8);

	/* Anti_blooming */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x4e, 0x06);
	write_cmos_sensor(0x44, 0x02);

	/* Crop */
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x91, 0x00);
	write_cmos_sensor(0x92, 0x0a);
	write_cmos_sensor(0x93, 0x00);
	write_cmos_sensor(0x94, 0x0b);
	write_cmos_sensor(0x95, 0x02);
	write_cmos_sensor(0x96, 0xd0);
	write_cmos_sensor(0x97, 0x05);
	write_cmos_sensor(0x98, 0x00);
	write_cmos_sensor(0x99, 0x00);

	/* MIPI */
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x02, 0x59);
	write_cmos_sensor(0x22, 0x03);
	write_cmos_sensor(0x26, 0x06);
	write_cmos_sensor(0x29, 0x03);
	write_cmos_sensor(0x2b, 0x06);
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x8c, 0x10);
#endif
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	log_inf("enable: %d\n", enable);

	write_cmos_sensor(0xfe, 0x01);
	if (enable)
		write_cmos_sensor(0x8c, 0x11);
	else
		write_cmos_sensor(0x8c, 0x10);
	write_cmos_sensor(0xfe, 0x00);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			log_inf("[gc5035p_qunhui_camera_sensor]\n");
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				log_inf("[gc5035p_qunhui_camera_sensor]\n");
				#if GC5035P_QUNHUI_OTP_FOR_CUSTOMER
				gc5035p_qunhui_otp_identify();
				log_err("gc5035p qunhui lensid = %d\n", lensid);
				if (lensid != 2) { // 2: dule module lensid
					log_err("return error lensid=%d\n", lensid);
					*sensor_id = 0xFFFFFFFF; // 0xFFFFFFFF: Default error camera id
					return ERROR_SENSOR_CONNECT_FAIL;
				}
				#endif
				log_inf("[gc5035p_qunhui]%s:i2c write id: 0x%x",
				__func__, imgsensor.i2c_write_id);
				log_inf("[gc5035p_qunhui]%s:sensor id: 0x%x\n",
				__func__, *sensor_id);
				return ERROR_NONE;
			}
			log_err("[gc5035p_qunhui]%s:fail, write id: 0x%x",
			__func__, imgsensor.i2c_write_id);
			log_err("[gc5035p_qunhui]%s:fail,id: 0x%x\n",
			__func__, *sensor_id);
			retry--;
			mdelay(1000);
		} while (retry > 0);
		i++;
		retry = 10;
	}

	if (*sensor_id != imgsensor_info.sensor_id) {
		log_err("[gc5035p_qunhui] *sensor_id = 0xFFFFFFFFr");
		log_err("eturn ERROR_SENSOR_CONNECT_FAIL\n");
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	return ERROR_NONE;
}

static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id =
		imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				pr_debug("[gc5035p_qunhui]%s:i2c write id: 0x%x",
				__func__, imgsensor.i2c_write_id);
				pr_debug("[gc5035p_qunhui]%s:sensor id: 0x%x\n",
				__func__, sensor_id);
				break;
			}
			log_err("[gc5035p_qunhui]%s:fail, write id: 0x%x",
			__func__, imgsensor.i2c_write_id);
			log_err("[gc5035p_qunhui]%s:fail, id: 0x%x\n",
			__func__, sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init();

#if GC5035P_QUNHUI_OTP_FOR_CUSTOMER
	gc5035p_qunhui_otp_function();
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
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

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
	log_inf("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();

	return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	log_inf("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_TRUE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			log_inf("Warning: current_fps %d fps is not support",
			imgsensor.current_fps);
			log_inf("Warning: so use cap's setting: %d fps!\n",
			imgsensor_info.cap.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_TRUE;
	}
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	return ERROR_NONE;
}

static kal_uint32 normal_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	log_inf("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);

	normal_video_setting(imgsensor.current_fps);
	return ERROR_NONE;
}

static kal_uint32 hs_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	log_inf("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	return ERROR_NONE;
}

static kal_uint32 slim_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	log_inf("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	return ERROR_NONE;
}

static kal_uint32 get_resolution(
MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	log_inf("E\n");
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

	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_INFO_STRUCT *sensor_info,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	log_inf("Enter %s!\n", __func__);
	log_inf("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;
	sensor_info->SensorResetActiveHigh = FALSE;
	sensor_info->SensorResetDelayCount = 5;

	sensor_info->SensroInterfaceType =
	imgsensor_info.sensor_interface_type;
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

	sensor_info->SensorMasterClockSwitch = 0;
	sensor_info->SensorDrivingCurrent =
	imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/*The frame of setting shutter default 0 for TG int*/
	sensor_info->AESensorGainDelayFrame =
	imgsensor_info.ae_sensor_gain_delay_frame;
	/*The frame of setting sensor gain*/
	sensor_info->AEISPGainDelayFrame =
	imgsensor_info.ae_ispgain_delay_frame;
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
	sensor_info->SensorWidthSampling = 0;
	sensor_info->SensorHightSampling = 0;
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
}

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	log_inf("Enter %s!\n", __func__);
	log_inf("scenario_id = %d\n", scenario_id);
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
	default:
		log_err("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}
static kal_uint32 set_video_mode(UINT16 framerate)
{
	/*This Function not used after ROME*/
	log_inf("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
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

static kal_uint32 set_max_framerate_by_scenario(
enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	log_inf("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		if (framerate == 0) {
			frame_length = 0;
		} else {
			frame_length = imgsensor_info.pre.pclk /
			framerate * 10 /
			imgsensor_info.pre.linelength;
		}
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
		imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
		imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0) {
			frame_length = 0;
		} else {
			frame_length = imgsensor_info.pre.pclk /
			framerate * 10 /
			imgsensor_info.pre.linelength;
		}
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
		imgsensor_info.normal_video.framelength) ?
			(frame_length -
			imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length =
		imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
			imgsensor_info.cap1.max_framerate) {
			if (framerate == 0) {
				frame_length = 0;
			} else {
				frame_length = imgsensor_info.pre.pclk /
				framerate * 10 /
				imgsensor_info.pre.linelength;
			}
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap1.framelength) ?
				(frame_length -
				imgsensor_info.cap1.framelength) : 0;
			imgsensor.frame_length =
			imgsensor_info.cap1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
			imgsensor_info.cap.max_framerate)
				log_inf("Warning: current_fps %d", framerate);
				log_inf("use cap's setting: %d fps!\n",
				imgsensor_info.cap.max_framerate / 10);
			if (framerate == 0) {
				frame_length = 0;
			} else {
				frame_length = imgsensor_info.pre.pclk /
				framerate * 10 /
				imgsensor_info.pre.linelength;
			}
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap.framelength) ?
				(frame_length -
				imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
			imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		if (framerate == 0) {
			frame_length = 0;
		} else {
			frame_length = imgsensor_info.pre.pclk /
			framerate * 10 /
			imgsensor_info.pre.linelength;
		}
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		(frame_length > imgsensor_info.hs_video.framelength) ?
			(frame_length -
			imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length =
		imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		if (framerate == 0) {
			frame_length = 0;
		} else {
			frame_length = imgsensor_info.pre.pclk /
			framerate * 10 /
			imgsensor_info.pre.linelength;
		}
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		(frame_length > imgsensor_info.slim_video.framelength) ?
			(frame_length -
			imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length =
		imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:
		if (framerate == 0) {
			frame_length = 0;
		} else {
			frame_length = imgsensor_info.pre.pclk /
			framerate * 10 /
			imgsensor_info.pre.linelength;
		}
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		(frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
		imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		log_err("error scenario_id = %d, we use preview scenario\n",
		scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	log_inf("scenario_id = %d\n", scenario_id);

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
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	if (enable) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x3e, 0x91);	/* Stream on */
	} else {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x3e, 0x01);	/* Stream off */
	}
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *)feature_para;
	UINT16 *feature_data_16 = (UINT16 *)feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *)feature_para;
	UINT32 *feature_data_32 = (UINT32 *)feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;


	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo = NULL;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
	(MSDK_SENSOR_REG_INFO_STRUCT *)feature_para;

	if (!feature_para || !feature_para_len) {
		log_err("null ptr input params.\n");
		return ERROR_NONE;
	}

	log_inf("feature_id = %d\n", feature_id);

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
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		{
			kal_uint32 rate;

			switch (*feature_data_32) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				rate = imgsensor_info.cap.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				rate =
				imgsensor_info.normal_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				rate = imgsensor_info.hs_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				rate =
				imgsensor_info.slim_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				rate = imgsensor_info.pre.mipi_pixel_rate;
				break;
			}
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
		}
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter((UINT32)*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr,
		sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
		read_cmos_sensor(sensor_reg_data->RegAddr);
		log_inf("adb_i2c_read 0x%x = 0x%x\n",
		sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
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
		(enum MSDK_SCENARIO_ID_ENUM)*feature_data, (UINT32)*(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
		(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
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
		log_inf("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
		(UINT32)*feature_data);
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
		(uintptr_t)(*(feature_data + 1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy_s((void *)wininfo,
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT),
			(void *)&imgsensor_winsize_info[1],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy_s((void *)wininfo,
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT),
			(void *)&imgsensor_winsize_info[2],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy_s((void *)wininfo,
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT),
			(void *)&imgsensor_winsize_info[3],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy_s((void *)wininfo,
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT),
			(void *)&imgsensor_winsize_info[4],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy_s((void *)wininfo,
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT),
			(void *)&imgsensor_winsize_info[0],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		log_inf("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		log_inf("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter((UINT32)*feature_data);
		streaming_control(KAL_TRUE);
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 GC5035P_QUNHUI_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pffunc)
{
	/* To Do : Check Sensor status here */
	if (pffunc != NULL)
		*pffunc = &sensor_func;
	return ERROR_NONE;
}
