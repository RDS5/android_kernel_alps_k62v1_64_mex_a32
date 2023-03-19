/*
 * s5k4h7_otp_driver.c
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * camera s5k4h7_otp_drive
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

#include "durahms_sensor_otp_driver.h"

#define log_inf(format, args...)    \
	pr_err(PFX "[%s] " format, __func__, ##args)

static struct otp_t g_otp_info;

static void s5k4h7_write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, S5K4H7_IIC_ADDR);
}
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, S5K4H7_IIC_ADDR);
	return get_byte;
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

static int s5k4h7_otp_read_af(void)
{
	unsigned char data_p[S5K4H7_AF_DATA_NUM] = {0};
	unsigned int  sum_af_fg = 0;
	unsigned short offset;
	unsigned int addr = S5K4H7_OTP_AF_FLAG_ADDR;
	int i;

	g_otp_info.af_flag = selective_read_region(S5K4H7_AF_PAGE,
		S5K4H7_OTP_AF_FLAG_ADDR);
	for (i = 0; i < S5K4H7_AF_DATA_NUM; i++) {
		data_p[i] = selective_read_region(S5K4H7_AF_PAGE, addr);
		log_inf("s5k4h7_otp:data_p[%d] = 0x%x addr = 0x%x\n",
			i, data_p[i], addr);
		addr++;
	}
	if ((g_otp_info.af_flag & S5K4H7_AF_TEMP) ==
		S5K4H7_AF_GROUP1) {
		// af group1
		log_inf("af group1 is valid\n");
		offset = (S5K4H7_OTP_AF_GROUP1_DATA_ADDR -
			S5K4H7_OTP_AF_FLAG_ADDR);
		sum_af_fg = 1;
	} else if ((g_otp_info.af_flag & S5K4H7_AF_TEMP) ==
		S5K4H7_AF_GROUP1) {
		// af group2
		log_inf("af group2 is valid\n");
		offset = (S5K4H7_OTP_AF_GROUP2_DATA_ADDR -
			S5K4H7_OTP_AF_FLAG_ADDR);
		sum_af_fg = 1;
	} else {
		pr_err("af group is invalid/empty, gropflag = 0x%x, afflag = 0x%x\n",
			g_otp_info.af_flag, data_p[0]);
		g_otp_info.af_data[OTP_AF_DATA_SIZE] = OTP_CHECK_FAILED;
		return -1;
	}
	if (sum_af_fg) {
		g_otp_info.af_data[0] = data_p[offset + 0];
		g_otp_info.af_data[1] = data_p[offset + 2];
		g_otp_info.af_data[2] = data_p[offset + 1];
		g_otp_info.af_data[3] = data_p[offset + 3];
		g_otp_info.af_data[OTP_AF_DATA_SIZE] = OTP_CHECK_SUCCESSED;
	} else {
		pr_err("check af flag fail!!!");
		g_otp_info.af_data[OTP_AF_DATA_SIZE] = OTP_CHECK_FAILED;
		return -1;
	}
	return sum_af_fg;
}

int camcal_read_s5k4h7_sensorotp_func(struct i2c_client *current_client,
			unsigned int sensor_id,
			unsigned char **eeprom_buff,
			unsigned int *eeprom_size)
{
	int err = 0;
	unsigned int block_total_size = 0;
	unsigned char *peeprom_readbuff = NULL;

	if ((!current_client) || (!eeprom_buff) || (!eeprom_size)) {
		pr_err("current_client or eeprom_buff is null!\n");
		return -1;
	}
	peeprom_readbuff = kzalloc(OTP_AF_DATA_SIZE + 1, GFP_KERNEL);
	if (!peeprom_readbuff)
		return -1;
	s5k4h7_otp_read_af();
	err = memcpy_s(peeprom_readbuff, OTP_AF_DATA_SIZE + 1, g_otp_info.af_data,
		OTP_AF_DATA_SIZE + 1);
	if (err != EOK) {
		pr_err("memcpy_s fail!\n");
		return -1;
	}
	block_total_size += OTP_AF_DATA_SIZE + 1;

	*eeprom_buff = peeprom_readbuff;
	*eeprom_size = block_total_size;

	pr_info("block_size [%d]!\n", block_total_size);
	return block_total_size;
}
