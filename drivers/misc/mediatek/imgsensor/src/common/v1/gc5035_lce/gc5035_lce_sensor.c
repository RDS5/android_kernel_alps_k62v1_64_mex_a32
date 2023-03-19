/*
 * gc5035_lce_sensor.c
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * gc5035_lce_sensor image sensor driver
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


#include "gc5035_lce_sensor.h"

#ifndef TRUE
#define TRUE (bool)1
#endif
#ifndef FALSE
#define FALSE (bool)0
#endif

#define PFX "GC5035_LCE"
#define log_inf(fmt, args...) \
	pr_info(PFX "%s %d " fmt, __func__, __LINE__, ##args)
#define log_err(fmt, args...) pr_err(PFX "[%s] " fmt, __func__, ##args)

#define RETRY_TIMES 2

static DEFINE_SPINLOCK(imgsensor_drv_lock);
static struct gc5035_otp_t otp_data;

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

static kal_uint8 gc5035_otp_read_byte(kal_uint16 addr)
{
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x69, (addr >> 8) & 0x1f);
	write_cmos_sensor(0x6a, addr & 0xff);
	write_cmos_sensor(0xf3, 0x20);

	return read_cmos_sensor(0x6c);
}

static void gc5035_otp_read_group(kal_uint16 addr,
	kal_uint8 *data, kal_uint16 length)
{
	kal_uint16 i;

	if ((((addr & 0x1fff) >> 3) + length) > GC5035_OTP_DATA_LENGTH) {
		log_err("out of range, start addr: 0x%.4x, length = %d\n",
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

static void gc5035_gcore_read_dpc(void)
{
	kal_uint8 dpc_flag;
	struct gc5035_dpc_t *p_dpc = &otp_data.dpc;

	dpc_flag = gc5035_otp_read_byte(GC5035_OTP_DPC_FLAG_OFFSET);
	log_inf("dpc flag = 0x%x\n", dpc_flag);
	switch (gc5035_otp_get_2bit_flag(dpc_flag, 0)) {
	case GC5035_OTP_FLAG_EMPTY:
		log_err("dpc info is empty!!\n");
		p_dpc->flag = GC5035_OTP_FLAG_EMPTY;
		break;
	case GC5035_OTP_FLAG_VALID:
		log_inf("dpc info is valid!\n");
		p_dpc->total_num = gc5035_otp_read_byte(GC5035_OTP_DPC_TOTAL_NUM_OFFSET)
			+ gc5035_otp_read_byte(GC5035_OTP_DPC_ERROR_NUM_OFFSET);
		p_dpc->flag = GC5035_OTP_FLAG_VALID;
		log_inf("total_num = %d\n", p_dpc->total_num);
		break;
	default:
		p_dpc->flag = GC5035_OTP_FLAG_INVALID;
		break;
	}
}

static void gc5035_gcore_read_reg(void)
{
	kal_uint8 i = 0, j = 0;
	kal_uint16 base_group = 0;
	kal_uint8 reg[GC5035_OTP_REG_DATA_SIZE];
	struct gc5035_reg_update_t *p_regs = &otp_data.regs;

	memset_s(&reg, GC5035_OTP_REG_DATA_SIZE, 0, GC5035_OTP_REG_DATA_SIZE);
	p_regs->flag = gc5035_otp_read_byte(GC5035_OTP_REG_FLAG_OFFSET);
	log_inf("register update flag = 0x%x\n", p_regs->flag);
	if (p_regs->flag == GC5035_OTP_FLAG_VALID) {
		gc5035_otp_read_group(GC5035_OTP_REG_DATA_OFFSET,
				&reg[0], GC5035_OTP_REG_DATA_SIZE);

		for (i = 0; i < GC5035_OTP_REG_MAX_GROUP; i++) {
			base_group = i * GC5035_OTP_REG_BYTE_PER_GROUP;
			for (j = 0; j < GC5035_OTP_REG_REG_PER_GROUP; j++)
				if (gc5035_otp_check_1bit_flag(reg[base_group],
						(4 * j + 3))) {
					p_regs->reg[p_regs->cnt].page =
						(reg[base_group] >> (4 * j)) &
						0x07;
					p_regs->reg[p_regs->cnt].addr =
						reg[base_group + j *
						GC5035_OTP_REG_BYTE_PER_REG
						+ 1];
					p_regs->reg[p_regs->cnt].value =
						reg[base_group + j *
						GC5035_OTP_REG_BYTE_PER_REG
						+ 2];
					log_inf("register[%d] P%d:0x%x->0x%x\n",
						p_regs->cnt,
						p_regs->reg[p_regs->cnt].page,
						p_regs->reg[p_regs->cnt].addr,
						p_regs->reg[p_regs->cnt].value);
					p_regs->cnt++;
				}
		}

	}
}

#if GC5035_OTP_FOR_CUSTOMER
static kal_uint8 gc5035_otp_read_module_info(void)
{
	kal_uint8 i = 0, idx = 0, flag = 0, check = 0;
	kal_uint16 module_start_offset = GC5035_OTP_MODULE_DATA_OFFSET;
	kal_uint8 info[GC5035_OTP_MODULE_DATA_SIZE];
	struct gc5035_module_info_t module_info = { 0 };

	memset_s(&info, GC5035_OTP_MODULE_DATA_SIZE,
		0, GC5035_OTP_MODULE_DATA_SIZE);
	memset_s(&module_info, sizeof(struct gc5035_module_info_t),
		0, sizeof(struct gc5035_module_info_t));

	flag = gc5035_otp_read_byte(GC5035_OTP_MODULE_FLAG_OFFSET);
	log_inf("flag = 0x%x\n", flag);

	for (idx = 0; idx < GC5035_OTP_GROUP_CNT; idx++) {
		switch (gc5035_otp_get_2bit_flag(flag, 2 * (1 - idx))) {
		case GC5035_OTP_FLAG_EMPTY: {
			log_inf("group %d is empty!\n", idx + 1);
			break;
		}
		case GC5035_OTP_FLAG_VALID: {
			log_inf("group %d is valid!\n", idx + 1);
			module_start_offset = GC5035_OTP_MODULE_DATA_OFFSET
				+ gc5035_otp_get_offset(idx *
				GC5035_OTP_MODULE_DATA_SIZE);
			gc5035_otp_read_group(module_start_offset,
				&info[0], GC5035_OTP_MODULE_DATA_SIZE);
			for (i = 0; i < GC5035_OTP_MODULE_DATA_SIZE - 1; i++)
				check += info[i];

			if ((check % 255 + 1) == info[GC5035_OTP_MODULE_DATA_SIZE - 1]) {
				module_info.module_id = info[0];
				module_info.lens_id = info[1];
				module_info.year = info[2];
				module_info.month = info[3];
				module_info.day = info[4];

				log_inf("module_id = 0x%x\n",
					module_info.module_id);
				log_inf("lens_id = 0x%x\n",
					module_info.lens_id);
				log_inf("data = %d-%d-%d\n",
					module_info.year,
					module_info.month,
					module_info.day);
			} else {
				log_inf("sum1 %d error! sum2 = 0x%x, result = 0x%x\n",
					idx + 1,
					info[GC5035_OTP_MODULE_DATA_SIZE - 1],
					(check % 255 + 1));
			}
			break;
		}
		case GC5035_OTP_FLAG_INVALID:
		case GC5035_OTP_FLAG_INVALID2: {
			log_inf("group %d is invalid!\n", idx + 1);
			break;
		}
		default:
			break;
		}
	}

	return module_info.module_id;
}

static void gc5035_otp_read_wb_info(void)
{
	kal_uint8 i = 0, idx = 0, flag = 0;
	kal_uint16 wb_check = 0, golden_check = 0;
	kal_uint16 wb_start_offset = 0, golden_start_offset = 0;
	kal_uint8 golden[GC5035_OTP_GOLDEN_DATA_SIZE];
	kal_uint8 wb[GC5035_OTP_WB_DATA_SIZE];

	struct gc5035_wb_t *p_wb = &otp_data.wb;
	struct gc5035_wb_t *p_golden = &otp_data.golden;

	wb_start_offset = GC5035_OTP_WB_DATA_OFFSET;
	golden_start_offset = GC5035_OTP_GOLDEN_DATA_OFFSET;

	memset_s(&wb, GC5035_OTP_WB_DATA_SIZE,
		0, GC5035_OTP_WB_DATA_SIZE);
	memset_s(&golden, GC5035_OTP_GOLDEN_DATA_SIZE,
		0, GC5035_OTP_GOLDEN_DATA_SIZE);
	flag = gc5035_otp_read_byte(GC5035_OTP_WB_FLAG_OFFSET);
	log_inf("flag = 0x%x\n", flag);

	for (idx = 0; idx < GC5035_OTP_GROUP_CNT; idx++) {
		switch (gc5035_otp_get_2bit_flag(flag, 2 * (1 - idx))) {
		case GC5035_OTP_FLAG_EMPTY: {
			log_inf("wb group %d is empty!\n", idx + 1);
			p_wb->flag = p_wb->flag | GC5035_OTP_FLAG_EMPTY;
			break;
		}
		case GC5035_OTP_FLAG_VALID: {
			log_inf("wb group %d is valid!\n", idx + 1);
			wb_start_offset = GC5035_OTP_WB_DATA_OFFSET
				+ gc5035_otp_get_offset(idx *
				GC5035_OTP_WB_DATA_SIZE);
			gc5035_otp_read_group(wb_start_offset, &wb[0],
				GC5035_OTP_WB_DATA_SIZE);

			for (i = 0; i < GC5035_OTP_WB_DATA_SIZE - 1; i++)
				wb_check += wb[i];

			if ((wb_check % 255 + 1) ==
					wb[GC5035_OTP_WB_DATA_SIZE - 1]) {
				p_wb->rg = (wb[0] | ((wb[1] & 0xf0) << 4));
				p_wb->bg = (((wb[1] & 0x0f) << 8) | wb[2]);
				p_wb->rg = p_wb->rg == 0 ?
					GC5035_OTP_WB_RG_TYPICAL : p_wb->rg;
				p_wb->bg = p_wb->bg == 0 ?
					GC5035_OTP_WB_BG_TYPICAL : p_wb->bg;
				p_wb->flag = p_wb->flag | GC5035_OTP_FLAG_VALID;
				log_inf("wb r/g = 0x%x\n", p_wb->rg);
				log_inf("wb b/g = 0x%x\n", p_wb->bg);
			} else {
				p_wb->flag = p_wb->flag | GC5035_OTP_FLAG_INVALID;
				log_inf("sum1 %d error! sum2 = 0x%x, result = 0x%x\n",
					idx + 1,
					wb[GC5035_OTP_WB_DATA_SIZE - 1],
					(wb_check % 255 + 1));
			}
			break;
		}
		case GC5035_OTP_FLAG_INVALID:
		case GC5035_OTP_FLAG_INVALID2: {
			log_inf("wb group %d is invalid!\n", idx + 1);
			p_wb->flag = p_wb->flag | GC5035_OTP_FLAG_INVALID;
			break;
		}
		default:
			break;
		}

		switch (gc5035_otp_get_2bit_flag(flag, 2 * (3 - idx))) {
		case GC5035_OTP_FLAG_EMPTY: {
			log_inf("golden group %d is empty!\n", idx + 1);
			p_golden->flag = p_golden->flag | GC5035_OTP_FLAG_EMPTY;
			break;
		}
		case GC5035_OTP_FLAG_VALID: {
			log_inf("golden group %d is valid!\n", idx + 1);
			golden_start_offset = GC5035_OTP_GOLDEN_DATA_OFFSET
				+ gc5035_otp_get_offset(idx *
				GC5035_OTP_GOLDEN_DATA_SIZE);
			gc5035_otp_read_group(golden_start_offset,
				&golden[0], GC5035_OTP_GOLDEN_DATA_SIZE);
			for (i = 0; i < GC5035_OTP_GOLDEN_DATA_SIZE - 1; i++)
				golden_check += golden[i];

			if ((golden_check % 255 + 1) ==
				golden[GC5035_OTP_GOLDEN_DATA_SIZE - 1]) {
				p_golden->rg = (golden[0] | ((golden[1] &
						0xf0) << 4));
				p_golden->bg = (((golden[1] & 0x0f) << 8) |
						golden[2]);
				p_golden->rg = p_golden->rg == 0 ?
					GC5035_OTP_WB_RG_TYPICAL : p_golden->rg;
				p_golden->bg = p_golden->bg == 0 ?
					GC5035_OTP_WB_BG_TYPICAL : p_golden->bg;
				p_golden->flag = p_golden->flag |
					GC5035_OTP_FLAG_VALID;
				log_inf("golden r/g = 0x%x\n",
						p_golden->rg);
				log_inf("golden b/g = 0x%x\n",
						p_golden->bg);
			} else {
				p_golden->flag = p_golden->flag |
					GC5035_OTP_FLAG_INVALID;
				log_inf("sum1 %d error! sum2 = 0x%x, result = 0x%x\n",
					idx + 1,
					golden[GC5035_OTP_WB_DATA_SIZE - 1],
					(golden_check % 255 + 1));
			}
			break;
		}
		case GC5035_OTP_FLAG_INVALID:
		case GC5035_OTP_FLAG_INVALID2: {
			log_inf("golden group %d is invalid!\n", idx + 1);
			p_golden->flag = p_golden->flag | GC5035_OTP_FLAG_INVALID;
			break;
		}
		default:
			break;
		}
	}
}
#endif

static kal_uint8 gc5035_otp_read_sensor_info(void)
{
	kal_uint8 module_id = 0;
#if GC5035_OTP_DEBUG
	kal_uint16 i = 0;
	kal_uint8 debug[GC5035_OTP_DATA_LENGTH];
#endif

	gc5035_gcore_read_dpc();
	gc5035_gcore_read_reg();
#if GC5035_OTP_FOR_CUSTOMER
	module_id = gc5035_otp_read_module_info();
	gc5035_otp_read_wb_info();

#endif

#if GC5035_OTP_DEBUG
	memset_s(&debug[0], GC5035_OTP_DATA_LENGTH,
		0, GC5035_OTP_DATA_LENGTH);
	gc5035_otp_read_group(GC5035_OTP_START_ADDR, &debug[0],
			GC5035_OTP_DATA_LENGTH);
	for (i = 0; i < GC5035_OTP_DATA_LENGTH; i++)
		log_inf("addr = 0x%x, data = 0x%x\n",
				GC5035_OTP_START_ADDR + i * 8, debug[i]);
#endif

	return module_id;
}

static void gc5035_otp_update_dd(void)
{
	kal_uint8 state = 0;
	kal_uint8 n = 0;
	struct gc5035_dpc_t *p_dpc = &otp_data.dpc;

	if (p_dpc->flag == GC5035_OTP_FLAG_VALID) {
		log_inf("DD auto load start!\n");
		write_cmos_sensor(0xfe, 0x02);
		write_cmos_sensor(0xbe, 0x00);
		write_cmos_sensor(0xa9, 0x01);
		write_cmos_sensor(0x09, 0x33);
		write_cmos_sensor(0x01, (p_dpc->total_num >> 8) & 0x07);
		write_cmos_sensor(0x02, p_dpc->total_num & 0xff);
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

#if GC5035_OTP_FOR_CUSTOMER
static void gc5035_otp_update_wb(void)
{
	kal_uint16 r_gain = 0, g_gain = 0, b_gain = 0, base_gain = 0;
	kal_uint16 r_gain_curr = 0, g_gain_curr = 0, b_gain_curr = 0;
	kal_uint16 rg_typical = 0, bg_typical = 0;
	struct gc5035_wb_t *p_wb = &otp_data.wb;
	struct gc5035_wb_t *p_golden = &otp_data.golden;

	r_gain = GC5035_OTP_WB_GAIN_BASE;
	g_gain = GC5035_OTP_WB_GAIN_BASE;
	b_gain = GC5035_OTP_WB_GAIN_BASE;
	base_gain = GC5035_OTP_WB_CAL_BASE;
	r_gain_curr = GC5035_OTP_WB_CAL_BASE;
	g_gain_curr = GC5035_OTP_WB_CAL_BASE;
	b_gain_curr = GC5035_OTP_WB_CAL_BASE;
	rg_typical = GC5035_OTP_WB_RG_TYPICAL;
	bg_typical = GC5035_OTP_WB_BG_TYPICAL;

	if (gc5035_otp_check_1bit_flag(p_golden->flag, 0)) {
		rg_typical = p_golden->rg;
		bg_typical = p_golden->bg;
	} else {
		rg_typical = GC5035_OTP_WB_RG_TYPICAL;
		bg_typical = GC5035_OTP_WB_BG_TYPICAL;
	}
	log_inf("typical rg = 0x%x, bg = 0x%x\n", rg_typical, bg_typical);

	if (gc5035_otp_check_1bit_flag(p_wb->flag, 0)) {
		r_gain_curr = GC5035_OTP_WB_CAL_BASE * rg_typical / p_wb->rg;
		b_gain_curr = GC5035_OTP_WB_CAL_BASE * bg_typical / p_wb->bg;
		g_gain_curr = GC5035_OTP_WB_CAL_BASE;

		base_gain = (r_gain_curr < b_gain_curr) ?
			r_gain_curr : b_gain_curr;
		base_gain = (base_gain < g_gain_curr) ? base_gain : g_gain_curr;

		r_gain = GC5035_OTP_WB_GAIN_BASE * r_gain_curr / base_gain;
		g_gain = GC5035_OTP_WB_GAIN_BASE * g_gain_curr / base_gain;
		b_gain = GC5035_OTP_WB_GAIN_BASE * b_gain_curr / base_gain;
		log_inf("channel gain r = 0x%x, g = 0x%x, b = 0x%x\n",
				r_gain, g_gain, b_gain);

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
#endif

static void gc5035_otp_update_reg(void)
{
	kal_uint8 i = 0;

	log_inf("reg count = %d\n", otp_data.regs.cnt);

	if (gc5035_otp_check_1bit_flag(otp_data.regs.flag, 0))
		for (i = 0; i < otp_data.regs.cnt; i++) {
			write_cmos_sensor(0xfe,
					otp_data.regs.reg[i].page);
			write_cmos_sensor(otp_data.regs.reg[i].addr,
					otp_data.regs.reg[i].value);
			log_inf("reg[%d] P%d:0x%x -> 0x%x\n", i,
				otp_data.regs.reg[i].page,
				otp_data.regs.reg[i].addr,
				otp_data.regs.reg[i].value);
		}
}

static void gc5035_otp_update(void)
{
	gc5035_otp_update_dd();
#if GC5035_OTP_FOR_CUSTOMER
	gc5035_otp_update_wb();
#endif
	gc5035_otp_update_reg();
}

static kal_uint8 gc5035_otp_identify(void)
{
	kal_uint8 module_id = 0;

	memset_s(&otp_data, sizeof(otp_data),
		0, sizeof(otp_data));

	write_cmos_sensor(0xfc, 0x01);
	write_cmos_sensor(0xf4, 0x40);
	write_cmos_sensor(0xf5, 0xe9);
	write_cmos_sensor(0xf6, 0x14);
	write_cmos_sensor(0xf8, 0x49);
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

	gc5035_otp_read_group(GC5035_OTP_ID_DATA_OFFSET,
			&otp_data.otp_id[0], GC5035_OTP_ID_SIZE);
	module_id = gc5035_otp_read_sensor_info();

	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x67, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfa, 0x00);
	return module_id;
}

static void gc5035_otp_function(void)
{
	kal_uint8 i = 0, flag = 0;
	kal_uint8 otp_id[GC5035_OTP_ID_SIZE];

	memset_s(&otp_id, GC5035_OTP_ID_SIZE,
		0, GC5035_OTP_ID_SIZE);

	write_cmos_sensor(0xfa, 0x10);
	write_cmos_sensor(0xf5, 0xe9);
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x67, 0xc0);
	write_cmos_sensor(0x59, 0x3f);
	write_cmos_sensor(0x55, 0x80);
	write_cmos_sensor(0x65, 0x80);
	write_cmos_sensor(0x66, 0x03);
	write_cmos_sensor(0xfe, 0x00);

	gc5035_otp_read_group(GC5035_OTP_ID_DATA_OFFSET,
			&otp_id[0], GC5035_OTP_ID_SIZE);
	for (i = 0; i < GC5035_OTP_ID_SIZE; i++)
		if (otp_id[i] != otp_data.otp_id[i]) {
			flag = 1;
			break;
		}

	if (flag == 1) {
		log_inf("otp id mismatch, read again");
		memset_s(&otp_data, sizeof(otp_data),
			0, sizeof(otp_data));
		for (i = 0; i < GC5035_OTP_ID_SIZE; i++)
			otp_data.otp_id[i] = otp_id[i];
		gc5035_otp_read_sensor_info();
	}

	gc5035_otp_update();

	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x67, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfa, 0x00);
}

static void set_dummy(void)
{
	kal_int32 rc = 0;
	kal_uint32 frame_length = imgsensor.frame_length >> 2;

	log_inf("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	frame_length = frame_length << 2;
	rc = imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
		0x00, IMGSENSOR_I2C_BYTE_DATA);
	if (rc < 0)
		log_err("wtire sensor page ctrl reg failed");
	rc = imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CISCTRL_CAPT_VB_REG_H,
		(frame_length >> 8) & 0x3f, IMGSENSOR_I2C_BYTE_DATA);
	if (rc < 0)
		log_err("wtire sensor csictrl capt vb  h reg  failed");
	rc = imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CISCTRL_CAPT_VB_REG_L,
		frame_length & 0xff, IMGSENSOR_I2C_BYTE_DATA);
	if (rc < 0)
		log_err("wtire sensor csictrl capt vb  l reg  failed");
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

	if (imgsensor.autoflicker_en) {

		/* calc fps between 297~305, real fps set to 298 */
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		/* calc fps between 147~150, real fps set to 146 */
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

	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
					 0x00, IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor,
	SENSOR_CISCTRL_BUF_EXP_IN_REG_H, (cal_shutter >> 8) & 0x3f,
		IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_CISCTRL_BUF_EXP_IN_REG_L,
					 cal_shutter & 0xff, IMGSENSOR_I2C_BYTE_DATA);
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
	kal_uint16 reg_gain = gain << 2;

	if (reg_gain < GC5035_SENSOR_GAIN_BASE)
		reg_gain = GC5035_SENSOR_GAIN_BASE;
	else if (reg_gain > GC5035_SENSOR_GAIN_MAX)
		reg_gain = GC5035_SENSOR_GAIN_MAX;

	return (kal_uint16)reg_gain;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint32 reg_gain = 0;
	kal_uint32 temp_gain;
	kal_int16 gain_index;

	reg_gain = gain2reg(gain);
	for (gain_index = GC5035_SENSOR_GAIN_MAX_VALID_INDEX - 1;
		gain_index >= 0; gain_index--) {
		if (reg_gain >= g_gc5035_agc_param[gain_index][0])
			break;
	}
	(void)imgsensor_sensor_i2c_write(&imgsensor,
		SENSOR_PAGE_SELECT_REG, 0x00, IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(
		&imgsensor, SENSOR_SET_GAIN_REG,
		g_gc5035_agc_param[gain_index][1],
		IMGSENSOR_I2C_BYTE_DATA);
	temp_gain = reg_gain * dgain_ratio /
		g_gc5035_agc_param[gain_index][0];
	(void)imgsensor_sensor_i2c_write(
		&imgsensor, SENSOR_SET_GAIN_REG_H, (temp_gain >> 8) & 0x0f,
		IMGSENSOR_I2C_BYTE_DATA);
	(void)imgsensor_sensor_i2c_write(&imgsensor,
		SENSOR_SET_GAIN_REG_L, temp_gain & 0xfc,
		IMGSENSOR_I2C_BYTE_DATA);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);

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
		&imgsensor_info.pre_setting);
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
		&imgsensor_info.cap_setting);
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
					 &imgsensor_info.normal_video_setting);
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
					 &imgsensor_info.custom2_setting);
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
	int mod_id = 0;

	rc = imgsensor_sensor_i2c_read(&imgsensor, imgsensor_info.sensor_id_reg,
				       &sensor_id, IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0) {
		log_err("Read id failed.id reg: 0x%x\n", imgsensor_info.sensor_id_reg);
		sensor_id = 0xFFFF;
	}
	if (sensor_id == GC5035_SENSOR_ID) {
		mod_id  = gc5035_otp_identify();
		log_inf("mod_id = %d\n", mod_id);
		if (mod_id == LCE_MOD_ID)
			sensor_id = GC5035_LCE_SENSOR_ID;
		else
			sensor_id = 0;
	}
	return sensor_id;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	log_inf("enable: %d\n", enable);
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
					 0x01, IMGSENSOR_I2C_BYTE_DATA);
	if (enable) {
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_PATTERN_MODE,
						 0x11, IMGSENSOR_I2C_BYTE_DATA);
	} else {
		(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_SET_PATTERN_MODE,
						 0x10, IMGSENSOR_I2C_BYTE_DATA);
	}
	(void)imgsensor_sensor_i2c_write(&imgsensor, SENSOR_PAGE_SELECT_REG,
					 0x00, IMGSENSOR_I2C_BYTE_DATA);
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
	gc5035_otp_function();
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
		imgsensor.current_fps = 298;
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

static void set_register(kal_uint32 reg_addr, kal_uint32 reg_data)
{
	kal_int32 rc = ERROR_NONE;

	rc = imgsensor_sensor_i2c_write(&imgsensor, (kal_uint16)reg_addr,
					(kal_uint16)reg_data, IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0)
		log_err("failed. reg_addr:0x%x, reg_data: 0x%x.\n",
			reg_addr, reg_data);
}

static void get_register(kal_uint32 reg_addr, kal_uint32 *reg_data)
{
	kal_int32 rc = 0;
	kal_uint16 temp_data = 0;

	rc = imgsensor_sensor_i2c_read(&imgsensor, reg_addr,
				       &temp_data, IMGSENSOR_I2C_WORD_DATA);
	if (rc < 0)
		log_err("failed. reg_addr:0x%x, reg_data: 0x%x.\n",
			reg_addr, temp_data);
	*reg_data = temp_data;
	log_inf("reg_addr:0x%x, reg_data: 0x%x.\n",
		reg_addr, *reg_data);
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

static kal_uint32 feature_control_gc5035_lce(MSDK_SENSOR_FEATURE_ENUM feature_id,
		UINT8 *feature_para, UINT32 *feature_para_len)
{
	kal_uint32 rc = ERROR_NONE;
	UINT16 *feature_return_para_16 = (UINT16 *)feature_para;
	UINT16 *feature_data_16 = (UINT16 *)feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *)feature_para;
	UINT32 *feature_data_32 = (UINT32 *)feature_para;
	INT32 *feature_return_para_i32 = (INT32 *)feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo = NULL;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *)feature_para;

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
	case SENSOR_FEATURE_SET_REGISTER:
		set_register(sensor_reg_data->RegAddr,
			     sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		get_register(sensor_reg_data->RegAddr, &sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		(void)set_video_mode((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		rc = get_imgsensor_id(feature_return_para_32);
		if (rc == ERROR_NONE)
			gc5035_otp_identify();
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
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = 0;
		*feature_para_len = 4;
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
} /* feature_control_gc5035_lce() */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control_gc5035_lce,
	control,
	close
};

UINT32 GC5035_LCE_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	if (pfFunc)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
