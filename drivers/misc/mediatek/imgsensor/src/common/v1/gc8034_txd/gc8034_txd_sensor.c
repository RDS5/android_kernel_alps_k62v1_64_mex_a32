/*
 * gc8034_txd_sensor.c
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * gc8034_txd image sensor driver
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

#include <linux/i2c.h>
#include "gc8034_txd_sensor.h"

#define RETRY_TIMES	2
#define REG_GAIN_OFFSET_LEFT  4


#define PFX "[gc8034_txd]"
#define log_dbg(fmt, args...) \
	pr_debug(PFX "%s %d " fmt, __func__, __LINE__, ##args)
#define log_inf(fmt, args...) \
	pr_info(PFX "%s %d " fmt, __func__, __LINE__, ##args)
#define log_err(fmt, args...) \
	pr_err(PFX "%s %d " fmt, __func__, __LINE__, ##args)
/* Modify end */
static kal_uint32 dgain_ratio = 256;
static DEFINE_SPINLOCK(imgsensor_drv_lock);

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


static struct gc8034_txd_otp_t otp_info;
#ifdef GC8034_TXD_OTP_FOR_CUSTOMER
static kal_uint8 lsc_addr[4] = { 0x0e, 0x20, 0x1a, 0x88 };
#endif

static kal_uint8 read_otp(kal_uint8 page, kal_uint8 addr)
{
	log_inf("E!\n");
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xd4, ((page << 2) & 0x3c) + ((addr >> 5) & 0x03));
	write_cmos_sensor(0xd5, (addr << 3) & 0xff);

	write_cmos_sensor(0xf3, 0x20);

	return  read_cmos_sensor(0xd7);
}

#if defined(GC8034_TXD_OTP_FOR_CUSTOMER)
static void read_otp_group(kal_uint8 page, kal_uint8 addr,
	kal_uint8 *buff, int size)
{
	kal_uint8 i;
	kal_uint8 regf4 = read_cmos_sensor(0xf4);

	write_cmos_sensor(0xd4, ((page << 2) & 0x3c) + ((addr >> 5) & 0x03));
	write_cmos_sensor(0xd5, (addr << 3) & 0xff);

	write_cmos_sensor(0xf3, 0x20);
	write_cmos_sensor(0xf4, regf4 | 0x02);
	write_cmos_sensor(0xf3, 0x80);

	for (i = 0; i < size; i++)
		buff[i] = read_cmos_sensor(0xd7);

	write_cmos_sensor(0xf3, 0x00);
	write_cmos_sensor(0xf4, regf4 & 0xfd);
}
#endif

static void gc8034_txd_gcore_read_otp_info(void)
{
	kal_uint8 flagdd = 0, i = 0, j = 0, temp = 0;
#ifdef GC8034_TXD_OTP_FOR_CUSTOMER
	kal_uint32 check = 0;
	kal_uint8 flag_wb = 0, index = 0, flag_module = 0;
	kal_uint8 flag_lsc = 0;
	kal_uint8 info[INFO_WIDTH] = {0}, wb[WB_WIDTH] = {0};
	kal_uint8 golden[GOLDEN_WIDTH] = {0};
#endif
	memset_s(&otp_info, sizeof(otp_info), 0, sizeof(otp_info));

	/* Static Defective Pixel */
	flagdd = read_otp(0, 0x0b);

	switch (flagdd & 0x03) {
	case 0x00:
		log_inf("DD is Empty !!\n");
		otp_info.dd_flag = 0x00;
		break;
	case 0x01:
		otp_info.dd_cnt = read_otp(0, 0x0c) +
			read_otp(0, 0x0d);
		otp_info.dd_flag = 0x01;
		break;
	case 0x02:
	case 0x03:
		log_inf("DD is Invalid !!\n");
		otp_info.dd_flag = 0x02;
		break;
	default:
		break;
	}

#ifdef GC8034_TXD_OTP_FOR_CUSTOMER
	flag_module = read_otp(9, 0x6f);
	flag_wb = read_otp(9, 0x5e);

	/* INFO & WB */
	for (index = 0; index < 2; index++) {
		switch ((flag_module << (2 * index)) & 0x0c) {
		case 0x00:
			log_inf("otp_info group %d is Empty !!\n", index + 1);
			break;
		case 0x04:
			check = 0;
			read_otp_group(9, INFO_ROM_START +
				index * INFO_WIDTH, &info[0], INFO_WIDTH);
			for (i = 0; i < INFO_WIDTH - 1; i++)
				check += info[i];

			if ((check % 255 + 1) == info[INFO_WIDTH - 1]) {
				otp_info.module_id = info[0];
				otp_info.lens_id = info[1];
				otp_info.vcm_driver_id = info[2];
				otp_info.vcm_id = info[3];
				otp_info.year = info[4];
				otp_info.month = info[5];
				otp_info.day = info[6];
			} else {
				log_inf("otp_info Check sum %d Error !!\n", index + 1);
			}
			break;
		case 0x08:
		case 0x0c:
			log_inf("otp_info group %d is Invalid !!\n", index + 1);
			break;
		default:
			break;
		}

		switch ((flag_wb << (2 * index)) & 0x0c) {
		case 0x00:
			log_inf("WB group %d is Empty !!\n", index + 1);
			otp_info.wb_flag = otp_info.wb_flag | 0x00;
			break;
		case 0x04:
			check = 0;
			read_otp_group(9, WB_ROM_START +
				index * WB_WIDTH, &wb[0], WB_WIDTH);
			for (i = 0; i < WB_WIDTH - 1; i++)
				check += wb[i];

			if ((check % 255 + 1) == wb[WB_WIDTH - 1]) {
				otp_info.rg_gain = (wb[0] | ((wb[1] & 0xf0) << 4)) > 0 ?
					(wb[0] | ((wb[1] & 0xf0) << 4)) : 0x400;
				otp_info.bg_gain = (((wb[1] & 0x0f) << 8) | wb[2]) > 0 ?
					(((wb[1] & 0x0f) << 8) | wb[2]) : 0x400;
				otp_info.wb_flag = otp_info.wb_flag | 0x01;
			} else {
				log_inf("WB Check sum %d Error !!\n", index + 1);
			}
			break;
		case 0x08:
		case 0x0c:
			log_inf("WB group %d is Invalid !!\n", index + 1);
			otp_info.wb_flag = otp_info.wb_flag | 0x02;
			break;
		default:
			break;
		}

		switch ((flag_wb << (2 * index)) & 0xc0) {
		case 0x00:
			log_inf("GOLDEN group %d is Empty !!\n", index + 1);
			otp_info.golden_flag = otp_info.golden_flag | 0x00;
			break;
		case 0x40:
			check = 0;
			read_otp_group(9, GOLDEN_ROM_START +
				index * GOLDEN_WIDTH, &golden[0], GOLDEN_WIDTH);
			for (i = 0; i < GOLDEN_WIDTH - 1; i++)
				check += golden[i];

			if ((check % 255 + 1) == golden[GOLDEN_WIDTH - 1]) {
				otp_info.golden_rg =
				(golden[0] | ((golden[1] & 0xf0) << 4)) > 0 ?
				(golden[0] | ((golden[1] & 0xf0) << 4)) : RG_TYPICAL;
				otp_info.golden_bg = (((golden[1] & 0x0f) << 8) |
				golden[2]) > 0 ? (((golden[1] & 0x0f) << 8) |
				golden[2]) : BG_TYPICAL;
				otp_info.golden_flag = otp_info.golden_flag | 0x01;
			} else {
				log_inf("GOLDEN Check sum %d Error !!\n", index + 1);
			}

			break;
		case 0x80:
		case 0xc0:
			log_inf("GOLDEN group %d is Invalid !!\n", index + 1);
			otp_info.golden_flag = otp_info.golden_flag | 0x02;
			break;
		default:
			break;
		}
	}
	/* LSC */
	flag_lsc = read_otp(3, 0x43);

	otp_info.lsc_flag = 2;
	for (index = 0; index < 2; index++) {
		switch ((flag_lsc << (2 * index)) & 0x0c) {
		case 0x00:
			log_inf("LSC group %d is Empty !\n", index + 1);
			break;
		case 0x04:
			otp_info.lsc_flag = index;
			break;
		case 0x08:
		case 0x0c:
			log_inf("LSC group %d is Invalid !!\n", index + 1);
			break;
		default:
			break;
		}
	}

#endif

	/* chip regs */
	otp_info.reg_flag = read_otp(2, 0x4e);

	if (otp_info.reg_flag == 1)
		for (i = 0; i < 5; i++) {
			temp = read_otp(2, 0x4f + 5 * i);
			for (j = 0; j < 2; j++)
				if (((temp >> (4 * j + 3)) & 0x01) == 0x01) {
					otp_info.reg_page[otp_info.reg_num] =
					(temp >> (4 * j)) & 0x03;
					otp_info.reg_addr[otp_info.reg_num] =
					read_otp(2, 0x50 + 5 * i + 2 * j);
					otp_info.reg_value[otp_info.reg_num] =
					read_otp(2, 0x50 + 5 * i + 2 * j + 1);
					otp_info.reg_num++;
				}
		}
}

static void gc8034_txd_gcore_check_prsel(void)
{
	kal_uint8 product_level = 0;

	product_level = read_otp(2, 0x68) & 0x07;

	if ((product_level == 0x00) || (product_level == 0x01)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xd2, 0xcb);
	} else {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xd2, 0xc3);
	}
}

static void gc8034_txd_gcore_update_dd(void)
{
	kal_uint8 state = 0;

	if (otp_info.dd_flag == 0x01) {

		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x79, 0x2e);
		write_cmos_sensor(0x7b, otp_info.dd_cnt);
		write_cmos_sensor(0x7e, 0x00);
		write_cmos_sensor(0x7f, 0x70);
		write_cmos_sensor(0x6e, 0x01);
		write_cmos_sensor(0xbd, 0xd4);
		write_cmos_sensor(0xbe, 0x9c);
		write_cmos_sensor(0xbf, 0xa0);
		write_cmos_sensor(0xfe, 0x01);
		write_cmos_sensor(0xbe, 0x00);
		write_cmos_sensor(0xa9, 0x01);
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xf2, 0x41);

		while (1) {
			state = read_cmos_sensor(0x6e);
			if ((state | 0xef) != 0xff)
				break;
			mdelay(10);
		}

		write_cmos_sensor(0xfe, 0x01);
		write_cmos_sensor(0xbe, 0x01);
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x79, 0x00);
	}
}

#ifdef GC8034_TXD_OTP_FOR_CUSTOMER
static int gc8034_txd_gcore_update_wb(void)
{
	kal_uint16 r_gain_current = 0, g_gain_current = 0;
	kal_uint16 b_gain_current = 0, base_gain = 0;
	kal_uint16 r_gain = 1024, g_gain = 1024, b_gain = 1024;
	kal_uint16 rg_typical = 0, bg_typical = 0;

	if ((otp_info.golden_flag & 0x01) == 0x01) {
		rg_typical = otp_info.golden_rg;
		bg_typical = otp_info.golden_bg;
	} else {
		rg_typical = RG_TYPICAL;
		bg_typical = BG_TYPICAL;
	}
	if (otp_info.rg_gain == 0 || otp_info.bg_gain == 0)
		return ERROR_NONE;
	if ((otp_info.wb_flag & 0x01) == 0x01) {
		r_gain_current = 2048 * rg_typical / otp_info.rg_gain;
		b_gain_current = 2048 * bg_typical / otp_info.bg_gain;
		g_gain_current = 2048;

		base_gain = (r_gain_current < b_gain_current) ?
		r_gain_current : b_gain_current;
		base_gain = (base_gain < g_gain_current) ?
		base_gain : g_gain_current;
	if (base_gain == 0)
		return ERROR_NONE;
	r_gain = 0x400 * r_gain_current / base_gain;
	g_gain = 0x400 * g_gain_current / base_gain;
	b_gain = 0x400 * b_gain_current / base_gain;

	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x84, g_gain >> 3);
	write_cmos_sensor(0x85, r_gain >> 3);
	write_cmos_sensor(0x86, b_gain >> 3);
	write_cmos_sensor(0x87, g_gain >> 3);
	write_cmos_sensor(0x88, ((g_gain & 0x07) << 4) + (r_gain & 0x07));
	write_cmos_sensor(0x89, ((b_gain & 0x07) << 4) + (g_gain & 0x07));
	write_cmos_sensor(0xfe, 0x00);
	}
	return 0;
}

static void gc8034_txd_gcore_update_lsc(void)
{
	kal_uint8 state = 0;

	if (otp_info.lsc_flag != 0x02) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x78, 0x9b);
		write_cmos_sensor(0x79, 0x0d);
		write_cmos_sensor(0x7a, LSC_NUM);
		write_cmos_sensor(0x7c, lsc_addr[otp_info.lsc_flag * 2]);
		write_cmos_sensor(0x7d, lsc_addr[otp_info.lsc_flag * 2 + 1]);
		write_cmos_sensor(0x6e, 0x01);
		write_cmos_sensor(0xfe, 0x01);
		write_cmos_sensor(0xcf, 0x00);
		write_cmos_sensor(0xc9, 0x01);
		write_cmos_sensor(0xf2, 0x41);
		write_cmos_sensor(0xfe, 0x00);

		while (1) {
			state = read_cmos_sensor(0x6e);
			if ((state | 0xdf) != 0xff)
				break;
			mdelay(10);
		}

		write_cmos_sensor(0xfe, 0x01);
		write_cmos_sensor(0xcf, 0x01);
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x79, 0x00);
		write_cmos_sensor(0xfe, 0x01);
		write_cmos_sensor(0xa0, 0x13);
		write_cmos_sensor(0xfe, 0x00);
	}
}
#endif

static void gc8034_txd_gcore_update_chipversion(void)
{
	kal_uint8 i = 0;

	if (otp_info.reg_flag)
		for (i = 0; i < otp_info.reg_num; i++) {
			write_cmos_sensor(0xfe, otp_info.reg_page[i]);
			write_cmos_sensor(otp_info.reg_addr[i], otp_info.reg_value[i]);
		}
}

static void gc8034_txd_gcore_update_otp(void)
{
	gc8034_txd_gcore_update_dd();
	gc8034_txd_gcore_check_prsel();
#ifdef GC8034_TXD_OTP_FOR_CUSTOMER
	gc8034_txd_gcore_update_wb();
	gc8034_txd_gcore_update_lsc();
#endif
	gc8034_txd_gcore_update_chipversion();
}

static void gc8034_txd_gcore_enable_otp(kal_uint8 state)
{
	kal_uint8 otp_clk = 0, otp_en = 0;

	otp_clk = read_cmos_sensor(0xf2);
	otp_en = read_cmos_sensor(0xf4);
	if (state) {
		otp_clk = otp_clk | 0x01;
		otp_en = otp_en | 0x08;
		write_cmos_sensor(0xf2, otp_clk);
		write_cmos_sensor(0xf4, otp_en);
	} else {
		otp_en = otp_en & 0xf7;
		otp_clk = otp_clk & 0xfe;
		write_cmos_sensor(0xf4, otp_en);
		write_cmos_sensor(0xf2, otp_clk);

	}
}

static void gc8034_txd_gcore_initial_otp(void)
{
	write_cmos_sensor(0xfc, 0x01);
	write_cmos_sensor(0xf2, 0x00);
	write_cmos_sensor(0xf4, 0x80);
	write_cmos_sensor(0xf5, 0x19);
	write_cmos_sensor(0xf6, 0x44);
	write_cmos_sensor(0xf8, 0x63);
	write_cmos_sensor(0xfa, 0x45);
	write_cmos_sensor(0xf9, 0x00);
	write_cmos_sensor(0xf7, 0x9d);
	write_cmos_sensor(0xfc, 0x03);
	write_cmos_sensor(0xfc, 0xea);
}

static void gc8034_txd_gcore_identify_otp(void)
{
	gc8034_txd_gcore_enable_otp(OTP_OPEN);
	gc8034_txd_gcore_read_otp_info();
	gc8034_txd_gcore_update_otp();
	gc8034_txd_gcore_enable_otp(OTP_CLOSE);
}

static void set_dummy(void)
{
	kal_uint32 vb;
	kal_int32 rc;

	vb = imgsensor.frame_length - GC8034_BASIC_FRAME_LENGTH;
	vb = vb < GC8034_MIN_VB ? GC8034_MIN_VB : vb;
	vb = vb > GC8034_MAX_VB ? GC8034_MAX_VB : vb;
	rc = imgsensor_sensor_i2c_write(&imgsensor, GC8034_PAGE_SEL_REG,
		0x00, IMGSENSOR_I2C_BYTE_DATA);
	rc = imgsensor_sensor_i2c_write(&imgsensor, GC8034_VB_REG_H,
		(vb >> 8) & 0x1f, IMGSENSOR_I2C_BYTE_DATA);
	rc = imgsensor_sensor_i2c_write(&imgsensor, GC8034_VB_REG_L,
		vb & 0xfe, IMGSENSOR_I2C_BYTE_DATA);
}

static void set_max_framerate(UINT16 framerate,
	kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	log_dbg("ENTER\n");

	if (!framerate || !imgsensor.line_length) {
		log_err("Invalid params. framerate=%u, line_length=%u\n",
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

static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps, cal_shutter = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/* if shutter bigger than frame_length, extend frame length */
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
	shutter = (shutter > (imgsensor_info.max_frame_length -
		imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length -
		imgsensor_info.margin) : shutter;

	if (!imgsensor.frame_length || !imgsensor.line_length) {
		log_err("Invalid length. frame_length=%u, line_length=%u\n",
			imgsensor.frame_length, imgsensor.line_length);
		return;
	}

	realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
		imgsensor.frame_length;

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

	if (!shutter) {
		log_err("Invalid shutter. shutter=%u\n", shutter);
		return;
	}

	cal_shutter = shutter >> 1;
	cal_shutter = cal_shutter << 1;
	dgain_ratio = 256 * shutter / cal_shutter;
	/* Update Shutter */
	imgsensor_sensor_i2c_write(&imgsensor, GC8034_STREAM_REG,
		0x00, IMGSENSOR_I2C_BYTE_DATA);
	imgsensor_sensor_i2c_write(&imgsensor, GC8034_SHUT_REG_H,
		(cal_shutter >> 8) & 0x7f, IMGSENSOR_I2C_BYTE_DATA);
	imgsensor_sensor_i2c_write(&imgsensor, GC8034_SHUT_REG_L,
		cal_shutter & 0xff, IMGSENSOR_I2C_BYTE_DATA);

	log_dbg("Exit! shutter =%u, framelength =%u\n",
		shutter, imgsensor.frame_length);
}

static void gain2reg(kal_uint16 gain)
{
	kal_int16 gain_index = 0, temp_gain = 0;
	/*kal_uint8 i = 0;*/
	kal_uint16 gain_level[MAX_AG_ID] = {
		0x0040, /* 1.000*/
		0x0058, /* 1.375*/
		0x007d, /* 1.950*/
		0x00ad, /* 2.700*/
		0x00f3, /* 3.800*/
		0x0159, /* 5.400*/
		0x01ea, /* 7.660*/
		0x02ac, /*10.688*/
		0x03c2, /*15.030*/
	};
	kal_uint8 agc_register[2 * MAX_AG_ID][AGC_REG_NUM] = {
		/* binning */
		{ 0x00, 0x55, 0x83, 0x01, 0x06, 0x18, 0x20, 0x16, 0x17, 0x50,
			0x6c, 0x9b, 0xd8, 0x00 },
		{ 0x00, 0x55, 0x83, 0x01, 0x06, 0x18, 0x20, 0x16, 0x17, 0x50,
			0x6c, 0x9b, 0xd8, 0x00 },
		{ 0x00, 0x4e, 0x84, 0x01, 0x0c, 0x2e, 0x2d, 0x15, 0x19, 0x47,
			0x70, 0x9f, 0xd8, 0x00 },
		{ 0x00, 0x51, 0x80, 0x01, 0x07, 0x28, 0x32, 0x22, 0x20, 0x49,
			0x70, 0x91, 0xd9, 0x00 },
		{ 0x00, 0x4d, 0x83, 0x01, 0x0f, 0x3b, 0x3b, 0x1c, 0x1f, 0x47,
			0x6f, 0x9b, 0xd3, 0x00 },
		{ 0x00, 0x50, 0x83, 0x01, 0x08, 0x35, 0x46, 0x1e, 0x22, 0x4c,
			0x70, 0x9a, 0xd2, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c,
			0x67, 0x8d, 0xc0, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c,
			0x67, 0x8d, 0xc0, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c,
			0x67, 0x8d, 0xc0, 0x00 },
		/* fullsize */
		{ 0x00, 0x55, 0x83, 0x01, 0x06, 0x18, 0x20, 0x16, 0x17, 0x50,
			0x6c, 0x9b, 0xd8, 0x00 },
		{ 0x00, 0x55, 0x83, 0x01, 0x06, 0x18, 0x20, 0x16, 0x17, 0x50,
			0x6c, 0x9b, 0xd8, 0x00 },
		{ 0x00, 0x4e, 0x84, 0x01, 0x0c, 0x2e, 0x2d, 0x15, 0x19, 0x47,
			0x70, 0x9f, 0xd8, 0x00 },
		{ 0x00, 0x51, 0x80, 0x01, 0x07, 0x28, 0x32, 0x22, 0x20, 0x49,
			0x70, 0x91, 0xd9, 0x00 },
		{ 0x00, 0x4d, 0x83, 0x01, 0x0f, 0x3b, 0x3b, 0x1c, 0x1f, 0x47,
			0x6f, 0x9b, 0xd3, 0x00 },
		{ 0x00, 0x50, 0x83, 0x01, 0x08, 0x35, 0x46, 0x1e, 0x22, 0x4c,
			0x70, 0x9a, 0xd2, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c,
			0x67, 0x8d, 0xc0, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c,
			0x67, 0x8d, 0xc0, 0x00 },
		{ 0x00, 0x52, 0x80, 0x01, 0x0c, 0x35, 0x3a, 0x2b, 0x2d, 0x4c,
			0x67, 0x8d, 0xc0, 0x00 }
	};

	for (gain_index = MEAG_INDEX - 1; gain_index >= 0; gain_index--)
		if (gain >= gain_level[gain_index]) {
			write_cmos_sensor(0xb6, gain_index);
			temp_gain = 256 * gain / gain_level[gain_index];
			temp_gain = temp_gain * dgain_ratio / 256;
			write_cmos_sensor(0xb1, temp_gain >> 8);
			write_cmos_sensor(0xb2, temp_gain & 0xff);

			write_cmos_sensor(0xfe,
				agc_register[bor_flag * MAX_AG_ID + gain_index][0]);
			write_cmos_sensor(0x20,
				agc_register[bor_flag * MAX_AG_ID + gain_index][1]);
			write_cmos_sensor(0x33,
				agc_register[bor_flag * MAX_AG_ID + gain_index][2]);
			write_cmos_sensor(0xfe,
				agc_register[bor_flag * MAX_AG_ID + gain_index][3]);
			write_cmos_sensor(0xdf,
				agc_register[bor_flag * MAX_AG_ID + gain_index][4]);
			write_cmos_sensor(0xe7,
				agc_register[bor_flag * MAX_AG_ID + gain_index][5]);
			write_cmos_sensor(0xe8,
				agc_register[bor_flag * MAX_AG_ID + gain_index][6]);
			write_cmos_sensor(0xe9,
				agc_register[bor_flag * MAX_AG_ID + gain_index][7]);
			write_cmos_sensor(0xea,
				agc_register[bor_flag * MAX_AG_ID + gain_index][8]);
			write_cmos_sensor(0xeb,
				agc_register[bor_flag * MAX_AG_ID + gain_index][9]);
			write_cmos_sensor(0xec,
				agc_register[bor_flag * MAX_AG_ID + gain_index][10]);
			write_cmos_sensor(0xed,
				agc_register[bor_flag * MAX_AG_ID + gain_index][11]);
			write_cmos_sensor(0xee,
				agc_register[bor_flag * MAX_AG_ID + gain_index][12]);
			write_cmos_sensor(0xfe,
				agc_register[bor_flag * MAX_AG_ID + gain_index][13]);
			break;
		}
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint32 real_gain = 0;

	log_inf("Enter!\n");
	real_gain = (gain < SENSOR_BASE_GAIN) ? SENSOR_BASE_GAIN : gain;
	real_gain = (gain > SENSOR_MAX_GAIN) ? SENSOR_MAX_GAIN : gain;

	gain2reg(real_gain);

	return gain;
}

static kal_uint32 sensor_init(void)
{
	kal_int32 rc;

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.init_setting);
	if (rc < 0) {
		log_err("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	return ERROR_NONE;
}

static kal_uint32 set_preview_setting(void)
{
	kal_int32 rc;

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.pre_setting);
		bor_flag = 0;
	if (rc < 0) {
		log_err("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	return ERROR_NONE;
}

static kal_uint32 set_capture_setting(kal_uint16 currefps)
{
	kal_int32 rc;

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.cap_setting);
		bor_flag = 1;
	if (rc < 0) {
		log_err("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	return ERROR_NONE;
}

static kal_uint32 set_normal_video_setting(kal_uint16 currefps)
{
	kal_int32 rc;

	rc = imgsensor_sensor_write_setting(&imgsensor,
		&imgsensor_info.normal_video_setting);
		bor_flag = 1;
	if (rc < 0) {
		log_err("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}

	return ERROR_NONE;
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

static kal_uint32 return_sensor_id(void)
{
	kal_int32 rc;
	kal_uint16 sensor_id = 0;
	int mode_id1 = 0, mode_id2 = 0;

	rc = imgsensor_sensor_i2c_read(&imgsensor,
			imgsensor_info.sensor_id_reg,
			&sensor_id, IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		log_err("Read id failed.id reg: 0x%x\n",
			imgsensor_info.sensor_id_reg);
		sensor_id = 0xFFFF;
	}
	if (sensor_id == GC8034_SENSOR_ID) {
		gc8034_txd_gcore_initial_otp();
		gc8034_txd_gcore_enable_otp(OTP_OPEN);
		mode_id1 = read_otp(9, 0x70);
		mode_id2 = read_otp(9, 0x78);
		log_inf("mode_id1 = %d\n", mode_id1);
		log_inf("mode_id2 = %d\n", mode_id2);
		gc8034_txd_gcore_enable_otp(OTP_CLOSE);
		if (mode_id1 == TXD_MOD_ID || mode_id2 == TXD_MOD_ID)
			sensor_id = GC8034_TXD_SENSOR_ID;
		else
			sensor_id = 0;
	}
	return sensor_id;
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

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
				set_af_power_down_mode();
				return ERROR_NONE;
			}
			log_inf("Read id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static kal_uint32 open(void)
{
	kal_uint32 sensor_id = 0;
	kal_uint32 rc = ERROR_NONE;

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

	gc8034_txd_gcore_identify_otp();

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 close(void)
{
	log_inf("Enter\n");

	return ERROR_NONE;
}

static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

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
		log_err("set_preview_setting failed\n");
		return rc;
	}

	return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

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
		log_err("set_capture_setting failed\n");
		return rc;
	}

	return ERROR_NONE;
}

static kal_uint32 normal_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

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
		log_err("set_normal_video_setting failed\n");
		return rc;
	}

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

	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting sensor gain */
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
	sensor_info->SensorWidthSampling = 0; // 0 is default 1x
	sensor_info->SensorHightSampling = 0; // 0 is default 1x
	sensor_info->SensorPacketECCOrder = 1;
	sensor_info->PDAF_Support = 0;

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
	kal_uint32 rc;

	log_dbg("scenario_id = %d\n", scenario_id);
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
	default:
		log_err("Error ScenarioId setting. scenario_id:%d\n",
			scenario_id);
		rc = preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return rc;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	log_inf("framerate = %u\n ", framerate);

	if (!framerate)
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	/* fps set to 298 when frame is 300 and auto-flicker enabled */
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 298;
	/* fps set to 146 when frame is 150 and auto-flicker enabled */
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

	log_dbg("scenario_id = %d, framerate = %u\n", scenario_id, framerate);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		if (!framerate || !imgsensor_info.pre.linelength)
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
		if (!framerate || !imgsensor_info.normal_video.linelength)
			return ERROR_NONE;

		frame_length = imgsensor_info.normal_video.pclk / framerate *
			10 / imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
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
		if (!framerate || !imgsensor_info.cap.linelength)
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
	default:  // coding with preview scenario by default
		if (!framerate || !imgsensor_info.pre.linelength)
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

		log_err("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
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

static void gc8034_txd_get_crop_info(UINT32 feature,
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

static void gc8034_txd_get_mipi_pixel_rate(
	unsigned long long *feature_data)
{
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
}

static void gc8034_txd_get_pixel_rate(unsigned long long *feature_data)
{
	switch (*feature_data) {
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor_info.cap.linelength >
			IMGSENSOR_LINGLENGTH_GAP)
			*(UINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.cap.pclk /
				(imgsensor_info.cap.linelength -
				IMGSENSOR_LINGLENGTH_GAP))*
				imgsensor_info.cap.grabwindow_width;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
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

static kal_uint32 feature_control_gc8034_txd(
	MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para, UINT32 *feature_para_len)
{
	kal_uint32 rc = ERROR_NONE;
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo = NULL;

	if (!feature_para || !feature_para_len) {
		log_err("Fatal null. feature_para:%pK,feature_para_len:%pK\n",
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
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		rc = get_imgsensor_id(feature_return_para_32);
		if (rc == ERROR_NONE)
			log_dbg("SENSOR_FEATURE_CHECK_SENSOR_ID OK\n");
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
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t)(*(feature_data + 1));
		gc8034_txd_get_crop_info(*feature_data_32, wininfo);
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		rc = streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter((UINT16)*feature_data);

		rc = streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		gc8034_txd_get_mipi_pixel_rate(feature_data);
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
		gc8034_txd_get_pixel_rate(feature_data);
		break;
	default:
		log_inf("Not support the feature_id:%d\n", feature_id);
		break;
	}

	return ERROR_NONE;
} /* feature_control */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control_gc8034_txd,
	control,
	close
};

UINT32 GC8034_TXD_SensorInit(struct SENSOR_FUNCTION_STRUCT **pf_func)
{
	if (pf_func)
		*pf_func = &sensor_func;
	return ERROR_NONE;
}