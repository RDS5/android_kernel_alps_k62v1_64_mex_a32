/*
 * gc5025h_sunwin_sensor.c
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * gc5025h_sunwin_sensor image sensor driver
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
#include <securec.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "imgsensor_sensor_common.h"
#include "imgsensor_sensor_i2c.h"
#include "gc5025h_sunwin_sensor.h"

#ifndef TRUE
#define TRUE (bool)1
#endif
#ifndef FALSE
#define FALSE (bool)0
#endif

#define PFX "GC5025H_SUNWIN"
#define log_inf(fmt, args...) \
	pr_info(PFX "%s %d " fmt, __func__, __LINE__, ##args)
#define log_err(fmt, args...) pr_err(PFX "[%s] " fmt, __func__, ##args)

#define RETRY_TIMES 2

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static kal_uint8 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[1] = { (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 1, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}
static void write_cmos_sensor(kal_uint32 addr, kal_uint16 para)
{
	char pu_send_cmd[2] = { (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 2, imgsensor.i2c_write_id);
}
static struct gc5025h_otp otp_info;

static kal_uint8 gc5025h_read_otp(kal_uint8 addr)
{
	kal_uint8 value;
	kal_uint8 regd4;
	kal_uint16 realaddr = addr * 8;

	regd4 = read_cmos_sensor(0xd4);

	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xd4, (regd4 & 0xfc) + ((realaddr >> 8) & 0x03));
	write_cmos_sensor(0xd5, realaddr & 0xff);
	write_cmos_sensor(0xf3, 0x20);
	value = read_cmos_sensor(0xd7);

	return value;
}

static void gc5025h_read_otp_group(kal_uint8 addr, kal_uint8 *buff, int size)
{
	kal_uint8 i;
	kal_uint8 regd4;
	kal_uint16 realaddr = addr * 8;

	regd4 = read_cmos_sensor(0xd4);

	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xd4, (regd4 & 0xfc) + ((realaddr >> 8) & 0x03));
	write_cmos_sensor(0xd5, realaddr);
	write_cmos_sensor(0xf3, 0x20);
	write_cmos_sensor(0xf3, 0x88);

	for (i = 0; i < size; i++)
		buff[i] = read_cmos_sensor(0xd7);
}

static void gc5025h_select_page_otp(kal_uint8 otp_select_page)
{
	kal_uint8 page;

	write_cmos_sensor(0xfe, 0x00);
	page = read_cmos_sensor(0xd4);

	switch (otp_select_page) {
	case OTP_PAGE0:
		page = page & 0xfb;
		break;
	case OTP_PAGE1:
		page = page | 0x04;
		break;
	default:
		break;
	}

	mdelay(5);
	write_cmos_sensor(0xd4, page);
}

static void gc5025h_gcore_read_otp_info(void)
{
	kal_uint8 flagdd, flag_chipversion, check_sum_all = 0;
	kal_uint16 check = 0, i, j, cnt = 0, total_number = 0;
	kal_uint8 ddtempbuff[50 * 4];
#if GC5025HOTP_FOR_CUSTOMER
	kal_uint8 flag1, flag_golden, index;
	kal_uint8 info[8], wb[5], golden[5];
#endif

	memset_s(&otp_info, sizeof(otp_info), 0, sizeof(otp_info));

	/*TODO*/
	gc5025h_select_page_otp(OTP_PAGE0);
	flagdd = gc5025h_read_otp(0x00);
	log_inf("flag_dd = 0x%x\n", flagdd);
	flag_chipversion = gc5025h_read_otp(0x7f);

	/* DD */
	switch (flagdd & 0x03) {
	case 0x00:
		log_inf("DD is empty!\n");
		otp_info.dd_flag = 0x00;
		break;
	case 0x01:
		log_inf("DD is valid!\n");
		total_number = gc5025h_read_otp(0x01) + gc5025h_read_otp(0x02);
		log_inf("total_number = %d\n", total_number);

		if (total_number <= 31) {
			gc5025h_read_otp_group(0x03, &ddtempbuff[0],
					total_number * 4);
		} else {
			gc5025h_read_otp_group(0x03, &ddtempbuff[0], 31 * 4);
			gc5025h_select_page_otp(OTP_PAGE1);
			gc5025h_read_otp_group(0x29, &ddtempbuff[31 * 4],
				(total_number - 31) * 4);
		}

		for (i = 0; i < total_number; i++) {

			if (ddtempbuff[4 * i + 3] & 0x10) {
				switch (ddtempbuff[4 * i + 3] & 0x0f) {
				case 3:
					for (j = 0; j < 4; j++) {
						otp_info.dd_param_x[cnt] =
						(((kal_uint16)ddtempbuff[4 * i + 1] &
						0x0f) << 8) + ddtempbuff[4 * i];
						otp_info.dd_param_y[cnt] =
						((kal_uint16)ddtempbuff[4 * i + 2] << 4) +
						((ddtempbuff[4 * i + 1] & 0xf0) >> 4) + j;
						otp_info.dd_param_type[cnt++] = 2;
					}
					break;
				case 4:
					for (j = 0; j < 2; j++) {
						otp_info.dd_param_x[cnt] =
						(((kal_uint16)ddtempbuff[4 * i + 1] &
							0x0f) << 8) + ddtempbuff[4 * i];
						otp_info.dd_param_y[cnt] =
						((kal_uint16)ddtempbuff[4 * i + 2] << 4) +
						((ddtempbuff[4 * i + 1] & 0xf0) >> 4) + j;
						otp_info.dd_param_type[cnt++] = 2;
					}
					break;
				default:
					otp_info.dd_param_x[cnt] =
					(((kal_uint16)ddtempbuff[4 * i + 1] & 0x0f)
					<< 8) + ddtempbuff[4 * i];
					otp_info.dd_param_y[cnt] =
					((kal_uint16)ddtempbuff[4 * i + 2] << 4) +
					((ddtempbuff[4 * i + 1] & 0xf0) >> 4);
					otp_info.dd_param_type[cnt++] =
					ddtempbuff[4 * i + 3] & 0x0f;
					break;
				}
			}
		}

		otp_info.dd_cnt = cnt;
		otp_info.dd_flag = 0x01;
		break;
	case 0x02:
	case 0x03:
		log_inf("DD is invalid!\n");
		otp_info.dd_flag = 0x02;
		break;
	default:
		break;
	}

	/*For Chip Version*/
	log_inf("flag_chipversion = 0x%x\n", flag_chipversion);

	switch ((flag_chipversion >> 4) & 0x03) {
	case 0x00:
		log_inf("CHIPVERSION is empty!\n");
		break;
	case 0x01:
		log_inf("CHIPVERSION is valid!\n");
		gc5025h_select_page_otp(OTP_PAGE1);
		i = 0;
		do {
			otp_info.reg_addr[i] =
			gc5025h_read_otp(REG_ROM_START + i*2);
			otp_info.reg_value[i] =
			gc5025h_read_otp(REG_ROM_START + i * 2 + 1);
			i++;
		} while ((otp_info.reg_addr[i - 1] != 0) && (i < 10));
		otp_info.reg_num = i - 1;
		break;
	case 0x02:
		log_inf("CHIPVERSION is invalid!\n");
		break;
	default:
		break;
	}

#if GC5025HOTP_FOR_CUSTOMER
	gc5025h_select_page_otp(OTP_PAGE1);
	flag1 = gc5025h_read_otp(0x00);
	flag_golden = gc5025h_read_otp(0x1b);
	log_inf("flag1 = 0x%x , flag_golden = 0x%x\n", flag1, flag_golden);

	/* INFO & WB */
	for (index = 0; index < 2; index++) {
		switch ((flag1 >> (4 + 2 * index)) & 0x03) {
		case 0x00:
			log_inf("module info group %d is empty!\n", index + 1);
			break;
		case 0x01:
			log_inf("module info group %d is valid!\n", index + 1);
			gc5025h_read_otp_group(INFO_ROM_START + index * INFO_WIDTH,
				&info[0], INFO_WIDTH);
			for (i = 0; i < INFO_WIDTH - 1; i++)
				check += info[i];
			otp_info.module_id = info[0];
			otp_info.lens_id = info[1];
			otp_info.vcm_driver_id = info[2];
			otp_info.vcm_id = info[3];
			otp_info.year = info[4];
			otp_info.month = info[5];
			otp_info.day = info[6];
			break;
		case 0x02:
		case 0x03:
			log_inf("module info group %d is invalid!\n", index + 1);
			break;
		default:
			break;
		}

		switch ((flag1 >> (2 * index)) & 0x03) {
		case 0x00:
			log_inf("wb group %d is empty!\n", index + 1);
			otp_info.wb_flag = otp_info.wb_flag | 0x00;
			break;
		case 0x01:
			log_inf("wb group %d is valid!\n", index + 1);
			gc5025h_read_otp_group(WB_ROM_START + index * WB_WIDTH,
				&wb[0], WB_WIDTH);
			for (i = 0; i < WB_WIDTH - 1; i++)
				check += wb[i];
			otp_info.rg_gain = (((wb[0] << 8) & 0xff00) | wb[1]) > 0 ?
				(((wb[0] << 8) & 0xff00) | wb[1]) : 0x400;
			otp_info.bg_gain = (((wb[2] << 8) & 0xff00) | wb[3]) > 0 ?
				(((wb[2] << 8) & 0xff00) | wb[3]) : 0x400;
			otp_info.wb_flag = otp_info.wb_flag | 0x01;
			break;
		case 0x02:
		case 0x03:
			log_inf("wb group %d is invalid!\n", index + 1);
			otp_info.wb_flag = otp_info.wb_flag | 0x02;
			break;
		default:
			break;
		}

		switch ((flag_golden >> (2 * index)) & 0x03) {
		case 0x00:
			log_inf("wb golden group %d is empty!\n", index + 1);
			otp_info.golden_flag = otp_info.golden_flag | 0x00;
			break;
		case 0x01:
			log_inf("wb golden group %d is valid!\n", index + 1);
			gc5025h_read_otp_group(GOLDEN_ROM_START + index * GOLDEN_WIDTH,
				&golden[0], GOLDEN_WIDTH);
			for (i = 0; i < GOLDEN_WIDTH - 1; i++)
				check += golden[i];
			check_sum_all = golden[GOLDEN_WIDTH - 1];
			otp_info.golden_rg = (((golden[0] << 8) & 0xff00) |
			golden[1]) > 0 ? (((golden[0] << 8) & 0xff00) |
			golden[1]) : RG_TYPICAL;
			otp_info.golden_bg = (((golden[2] << 8) & 0xff00) |
			golden[3]) > 0 ? (((golden[2] << 8) & 0xff00) |
			golden[3]) : BG_TYPICAL;
			otp_info.golden_flag = otp_info.golden_flag | 0x01;
			break;
		case 0x02:
		case 0x03:
			log_inf("wb golden group %d is invalid!\n", index + 1);
			otp_info.golden_flag = otp_info.golden_flag | 0x02;
			break;
		default:
			break;
		}
	}

	if ((check % 0xff + 1) == check_sum_all) {
		otp_info.checksum_ok = KAL_TRUE;
		log_inf("module_id = 0x%x\n", otp_info.module_id);
		log_inf("r/g = 0x%x\n", otp_info.rg_gain);
		log_inf("b/g = 0x%x\n", otp_info.bg_gain);
		log_inf("golden_rg = 0x%x\n", otp_info.golden_rg);
		log_inf("golden_bg = 0x%x\n", otp_info.golden_bg);
	} else {
		otp_info.checksum_ok = KAL_FALSE;
		log_err("err:cursum failed\n");
	}
#endif
}

static void gc5025h_gcore_update_dd(void)
{
	kal_uint16 i = 0, j = 0, temp_x = 0, temp_y = 0;
	kal_uint8 flag = 0, temp_type = 0;
	kal_uint8 temp_val0, temp_val1, temp_val2;

	if (otp_info.dd_flag == 0x01) {
#if GC5025H_MIRROR_FLIP_ENABLE
		for (i = 0; i < otp_info.dd_cnt; i++) {
			if (otp_info.dd_param_type[i] == 0) {
				otp_info.dd_param_x[i] =
				WINDOW_WIDTH - otp_info.dd_param_x[i] + 1;
				otp_info.dd_param_y[i] =
				WINDOW_HEIGHT - otp_info.dd_param_y[i] + 1;
			} else if (otp_info.dd_param_type[i] == 1) {
				otp_info.dd_param_x[i] =
				WINDOW_WIDTH - otp_info.dd_param_x[i] - 1;
				otp_info.dd_param_y[i] =
				WINDOW_HEIGHT - otp_info.dd_param_y[i] + 1;
			} else {
				otp_info.dd_param_x[i] =
				WINDOW_WIDTH - otp_info.dd_param_x[i];
				otp_info.dd_param_y[i] =
				WINDOW_HEIGHT - otp_info.dd_param_y[i] + 1;
			}
		}
#else
		/* do nothing */
#endif

		for (i = 0; i < otp_info.dd_cnt - 1; i++)
			for (j = i + 1; j < otp_info.dd_cnt; j++)
				if (otp_info.dd_param_y[i] * WINDOW_WIDTH
					+ otp_info.dd_param_x[i]
					> otp_info.dd_param_y[j] * WINDOW_WIDTH
					+ otp_info.dd_param_x[j]) {
					temp_x = otp_info.dd_param_x[i];
					otp_info.dd_param_x[i] = otp_info.dd_param_x[j];
					otp_info.dd_param_x[j] = temp_x;
					temp_y = otp_info.dd_param_y[i];
					otp_info.dd_param_y[i] = otp_info.dd_param_y[j];
					otp_info.dd_param_y[j] = temp_y;
					temp_type = otp_info.dd_param_type[i];
					otp_info.dd_param_type[i] =
						otp_info.dd_param_type[j];
					otp_info.dd_param_type[j] = temp_type;
				}

		/* write SRAM */
		write_cmos_sensor(0xfe, 0x01);
		write_cmos_sensor(0xa8, 0x00);
		write_cmos_sensor(0x9d, 0x04);
		write_cmos_sensor(0xbe, 0x00);
		write_cmos_sensor(0xa9, 0x01);

		for (i = 0; i < otp_info.dd_cnt; i++) {
			temp_val0 = otp_info.dd_param_x[i] & 0x00ff;
			temp_val1 = (((otp_info.dd_param_y[i]) << 4) & 0x00f0)
				+ ((otp_info.dd_param_x[i] >> 8) & 0x000f);
			temp_val2 = (otp_info.dd_param_y[i] >> 4) & 0xff;
			write_cmos_sensor(0xaa, i);
			write_cmos_sensor(0xac, temp_val0);
			write_cmos_sensor(0xac, temp_val1);
			write_cmos_sensor(0xac, temp_val2);
			while ((i < (otp_info.dd_cnt - 1)) &&
				(otp_info.dd_param_x[i] == otp_info.dd_param_x[i + 1]) &&
				(otp_info.dd_param_y[i] == otp_info.dd_param_y[i + 1])) {
				flag = 1;
				i++;
			}
			if (flag)
				write_cmos_sensor(0xac, 0x02);
			else
				write_cmos_sensor(0xac, otp_info.dd_param_type[i]);
		}

		write_cmos_sensor(0xbe, 0x01);
		write_cmos_sensor(0xfe, 0x00);
	}
}

#if GC5025HOTP_FOR_CUSTOMER
static void gc5025h_gcore_update_wb(void)
{
	kal_uint16 r_gain_current = 0, g_gain_current = 0;
	kal_uint16 b_gain_current = 0, base_gain = 0;
	kal_uint16 r_gain = 1024, g_gain = 1024, b_gain = 1024;
	kal_uint16 rg_typical, bg_typical;

	if (otp_info.golden_flag == 0x02)
		return;

	if (0x00 == (otp_info.golden_flag & 0x01)) {
		rg_typical = RG_TYPICAL;
		bg_typical = BG_TYPICAL;
	}

	if (0x01 == (otp_info.golden_flag & 0x01)) {
		rg_typical = otp_info.golden_rg;
		bg_typical = otp_info.golden_bg;
		log_inf("rg_typical = 0x%x, bg_typical = 0x%x\n", rg_typical, bg_typical);
	}

	if (0x01 == (otp_info.wb_flag & 0x01)) {
		r_gain_current = 1024 * rg_typical / otp_info.rg_gain;
		b_gain_current = 1024 * bg_typical / otp_info.bg_gain;
		g_gain_current = 1024;

		base_gain = (r_gain_current < b_gain_current) ?
			r_gain_current : b_gain_current;
		base_gain = (base_gain < g_gain_current) ?
			base_gain : g_gain_current;
		log_inf("r_g_cur = 0x%x, b_g_cur = 0x%x, base_gain = 0x%x\n",
			r_gain_current, b_gain_current, base_gain);

		r_gain = 0x400 * r_gain_current / base_gain;
		g_gain = 0x400 * g_gain_current / base_gain;
		b_gain = 0x400 * b_gain_current / base_gain;

		/*TODO*/
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xc6, g_gain >> 3);
		write_cmos_sensor(0xc7, r_gain >> 3);
		write_cmos_sensor(0xc8, b_gain >> 3);
		write_cmos_sensor(0xc9, g_gain >> 3);
		write_cmos_sensor(0xc4, ((g_gain & 0x07) << 4) +
			(r_gain & 0x07));
		write_cmos_sensor(0xc5, ((b_gain & 0x07) << 4) +
			(g_gain & 0x07));
	}
}
#endif

static void gc5025h_gcore_update_chipversion(void)
{
	kal_uint8 i;

	log_inf("reg_num = %d\n", otp_info.reg_num);

	for (i = 0; i < otp_info.reg_num; i++) {
		write_cmos_sensor(otp_info.reg_addr[i],
			otp_info.reg_value[i]);
		log_inf("{0x%x, 0x%x}\n", otp_info.reg_addr[i], otp_info.reg_value[i]);
	}
}

static void gc5025h_gcore_update_otp(void)
{
	gc5025h_gcore_update_dd();
#if GC5025HOTP_FOR_CUSTOMER
	gc5025h_gcore_update_wb();
#endif
	gc5025h_gcore_update_chipversion();
}

static void gc5025h_gcore_enable_otp(kal_uint8 state)
{
	kal_uint8 otp_clk, otp_en;

	otp_clk = read_cmos_sensor(0xfa);
	otp_en = read_cmos_sensor(0xd4);
	if (state) {
		otp_clk = otp_clk | 0x10;
		otp_en = otp_en | 0x80;
		mdelay(5);
		write_cmos_sensor(0xfa, otp_clk);
		write_cmos_sensor(0xd4, otp_en);
		log_inf("Enable OTP!\n");
	} else {
		otp_en = otp_en & 0x7f;
		otp_clk = otp_clk & 0xef;
		mdelay(5);
		write_cmos_sensor(0xd4, otp_en);
		write_cmos_sensor(0xfa, otp_clk);
		log_inf("Disable OTP!\n");
	}
}


static void gc5025h_gcore_initial_otp(void)
{
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xf7, 0x01);
	write_cmos_sensor(0xf8, 0x11);
	write_cmos_sensor(0xf9, 0x00);
	write_cmos_sensor(0xfa, 0xa0);
	write_cmos_sensor(0xfc, 0x2e);
}

static void gc5025h_gcore_identify_otp(void)
{
	gc5025h_gcore_initial_otp();
	gc5025h_gcore_enable_otp(OTP_OPEN);
	gc5025h_gcore_read_otp_info();
	gc5025h_gcore_enable_otp(OTP_CLOSE);
}

static void set_dummy(void)
{
	kal_uint32 vb = 32;
	kal_uint32 basic_line = 1968;

	vb = imgsensor.frame_length - basic_line;
	vb = vb / 2;
	vb = vb * 2;
	vb = (vb < 32) ? 32 : vb;

	imgsensor_sensor_i2c_write(&imgsensor,
		SENSOR_CISCTRL_CAPT_VB_REG_H,
		(vb >> 8) & 0x1f, IMGSENSOR_I2C_BYTE_DATA);
	imgsensor_sensor_i2c_write(&imgsensor,
		SENSOR_CISCTRL_CAPT_VB_REG_L,
		vb & 0xff, IMGSENSOR_I2C_BYTE_DATA);
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	if (!framerate || !imgsensor.line_length) {
		log_err("Invalid params. framerate=%d, line_length=%d\n",
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

	log_inf("ENTER\n");
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

	if (imgsensor.sensor_mode == IMGSENSOR_MODE_VIDEO && imgsensor.autoflicker_en) {
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
	}

	cal_shutter = shutter / 2;
	cal_shutter = cal_shutter * 2;
	dgain_ratio = 256 * shutter / cal_shutter;
	if (cal_shutter <= 10) {
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
					0x00, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CISCTRL_EXP_REG1,
					0xdd, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CISCTRL_EXP_REG2,
					0x58, IMGSENSOR_I2C_BYTE_DATA);
	} else {
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
					0x00, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CISCTRL_EXP_REG1,
					0xaa, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CISCTRL_EXP_REG2,
					0x4d, IMGSENSOR_I2C_BYTE_DATA);
	}
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
					 0x00, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_CISCTRL_BUF_EXP_IN_REG_H, (cal_shutter >> 8) & 0x3f,
			IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_CISCTRL_BUF_EXP_IN_REG_L, cal_shutter & 0xff,
			IMGSENSOR_I2C_BYTE_DATA);
}

static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 ireg, temp;

	ireg = gain;

	if (ireg < GC5025H_SENSOR_GAIN_BASE)
		ireg = GC5025H_SENSOR_GAIN_BASE;

	if ((ireg >= ANALOG_GAIN_1) && (ireg < ANALOG_GAIN_2)) {
		(void)imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_PAGE_SELECT_REG, 0x00, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor,
		SENSOR_SET_GAIN_REG, 0x00, IMGSENSOR_I2C_BYTE_DATA);
		temp = ireg * dgain_ratio / 256;
		log_inf("analogic gain 1x, add pregain = %d\n", temp);
	} else if ((ireg >= ANALOG_GAIN_2) && (ireg < ANALOG_GAIN_3)) {
		(void)imgsensor_sensor_i2c_write(&imgsensor,
		SENSOR_PAGE_SELECT_REG, 0x00, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor,
		SENSOR_SET_GAIN_REG, 0x01, IMGSENSOR_I2C_BYTE_DATA);
		temp = 64 * ireg / ANALOG_GAIN_2;
		temp = temp * dgain_ratio / 256;
		log_inf("analogic gain 1.4x, add pregain = %d\n", temp);
	} else {
		(void)imgsensor_sensor_i2c_write(&imgsensor,
		SENSOR_PAGE_SELECT_REG, 0x00, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor,
			SENSOR_SET_GAIN_REG, 0x02, IMGSENSOR_I2C_BYTE_DATA);
		temp = 64 * ireg/ANALOG_GAIN_3;
		temp = temp * dgain_ratio / 256;
	}
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_GAIN_REG_H,
					temp >> 6, IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_GAIN_REG_L,
					(temp << 2) & 0xfc, IMGSENSOR_I2C_BYTE_DATA);
		log_inf("analogic gain 2x, add pregain = %d\n", temp);

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
		&imgsensor_info.init_setting);
	if (rc < 0) {
		log_err("Failed\n");
		return ERROR_DRIVER_INIT_FAIL;
	}
	log_inf("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 set_capture_setting(void)
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

static kal_uint32 set_normal_video_setting(void)
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

static kal_uint32 set_custom2_setting(void)
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

static kal_uint32 return_sensor_id(void)
{

	kal_int32 rc = 0;
	kal_uint16 sensor_id = 0;
	int mode_id = 0, mode_id2 = 0;

	rc = imgsensor_sensor_i2c_read(&imgsensor, imgsensor_info.sensor_id_reg,
				&sensor_id, IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		log_err("Read id failed.id reg: 0x%x\n", imgsensor_info.sensor_id_reg);
		sensor_id = 0xFFFF;
	}
	if (sensor_id == GC5025H_SENSOR_ID) {
		gc5025h_gcore_initial_otp();
		gc5025h_gcore_enable_otp(OTP_OPEN);
		gc5025h_select_page_otp(OTP_PAGE1);
		mode_id = gc5025h_read_otp(0x01);
		mode_id2 = gc5025h_read_otp(0x09);
		log_inf("mode_id1 = %d\n", mode_id);
		log_inf("mode_id2 = %d\n", mode_id2);
		gc5025h_gcore_enable_otp(OTP_CLOSE);
		if (mode_id == SUNWIN_MOD_ID || mode_id2 == SUNWIN_MOD_ID)
			sensor_id = GC5025H_SUNWIN_SENSOR_ID;
		else
			sensor_id = 0;
	}
	return sensor_id;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	log_inf("enable: %d\n", enable);
	if (enable) {
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
					 0x00, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_PATTERN_MODE,
						 0x11, IMGSENSOR_I2C_BYTE_DATA);
	} else {
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
					 0x00, IMGSENSOR_I2C_BYTE_DATA);
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_PATTERN_MODE,
						 0x10, IMGSENSOR_I2C_BYTE_DATA);
	}
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
				log_inf("id reg: 0x%x, read id: 0x%x, expect id:0x%x\n",
					imgsensor.i2c_write_id,
					*sensor_id,
					imgsensor_info.sensor_id);
				return ERROR_NONE;
			}
			log_inf("fail,: 0x%x,read id: 0x%x, expect id:0x%x\n",
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

static kal_uint32 open(void)
{
	int rc;
	UINT32 sensor_id = 0;

	log_inf("E\n");

	rc = get_imgsensor_id(&sensor_id);
	if (rc != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;
	log_inf("sensor probe successfully. sensor_id=0x%x\n", sensor_id);
	rc = sensor_init();
	if (rc != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;
	gc5025h_gcore_update_otp();
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
	log_inf("E");
	/* No Need to implement this function */
	return ERROR_NONE;
}

static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	log_inf("E");

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
		log_err("%s failed\n", __func__);
		return rc;
	}
	log_inf("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	log_inf("E");

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
		log_err("%s failed\n", __func__);
		return rc;
	}
	log_inf("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	log_inf("E");

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
		log_err("%s failed\n", __func__);
		return rc;
	}
	log_inf("EXIT\n");

	return ERROR_NONE;
}

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 rc;

	log_inf("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.current_fps = imgsensor_info.custom2.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	rc = set_custom2_setting();
	if (rc != ERROR_NONE) {
		log_err("%s failed\n", __func__);
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

		sensor_resolution->SensorCustom2Width =
			imgsensor_info.custom2.grabwindow_width;
		sensor_resolution->SensorCustom2Height =
			imgsensor_info.custom2.grabwindow_height;
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
	case MSDK_SCENARIO_ID_CUSTOM2:
		rc = custom2(image_window, sensor_config_data);
		if (rc != ERROR_NONE)
			log_err("custom2 failed\n");
		break;
	default:
		log_inf("Error ScenarioId setting");
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
		imgsensor.current_fps = 296;
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
	log_inf("enable = %d, framerate = %d ", enable, framerate);
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
	case MSDK_SCENARIO_ID_CUSTOM2:
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
		log_inf("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
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
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*framerate = imgsensor_info.cap.max_framerate;
			break;
		default:
			break;
		}
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
	return ERROR_NONE;
}

static kal_uint32 feature_control_gc5025h_sunwin(MSDK_SENSOR_FEATURE_ENUM feature_id,
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

	log_inf("feature_id = %d.\n", feature_id);
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
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		(void)set_video_mode((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		rc = get_imgsensor_id(feature_return_para_32);
		if (rc == ERROR_NONE)
			gc5025h_gcore_identify_otp();
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
		log_inf("current fps :%d\n", *feature_data_32);
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
		(void)sensor_dump_reg();
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CUSTOM2:
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
		case MSDK_SCENARIO_ID_CUSTOM2:
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
		log_err("Not support the feature_id:%d\n", feature_id);
		break;
	}

	return rc;
} /* feature_control_gc5025h_sunwin() */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control_gc5025h_sunwin,
	control,
	close
};

UINT32 GC5025H_SUNWIN_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	if (pfFunc)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
