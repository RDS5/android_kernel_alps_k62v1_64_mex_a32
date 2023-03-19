/*
 * ov8856_otp_driver.c
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * camera ov8856_otp_drive
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
	pr_info(PFX "[%s] " format, __func__, ##args)

static struct otp_t otp_info;

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, OV8856_IIC_ADDR);
	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF),
		(char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, OV8856_IIC_ADDR);
}
static void enable_read_otp(void)
{
	int temp1;

	write_cmos_sensor(0x0100, 0x01);
	temp1 = read_cmos_sensor(0x5001);
	write_cmos_sensor(0x5001, (0x00 & 0x08) | (temp1 & (~0x08)));
	// read OTP into buffer
	write_cmos_sensor(0x3d84, 0xC0);
	write_cmos_sensor(0x3d88, 0x70); // OTP start address
	write_cmos_sensor(0x3d89, 0x10);
	write_cmos_sensor(0x3d8A, 0x72); // OTP end address
	write_cmos_sensor(0x3d8B, 0x0a);
	write_cmos_sensor(0x3d81, 0x01); // load otp into buffer
	mdelay(10);
}
static void disable_read_otp(void)
{
	int temp1;

	temp1 = read_cmos_sensor(0x5001);
	write_cmos_sensor(0x5001, (0x08 & 0x08) | (temp1 & (~0x08)));
	write_cmos_sensor(0x0100, 0x00);
}
static void ov8856_otp_read_af(void)
{
	int otp_flag = 0;
	int addr = 0;
	int i = 0;
	kal_uint8 af[OTP_AF_DATA_SIZE] = { 0 };

	enable_read_otp();
	otp_flag = read_cmos_sensor(0x701F);
	log_inf("af_otp_flag = %d\n", otp_flag);
	if ((otp_flag & 0xc0) == 0x40) {
		addr = OV8856_OTP_AF_DATA_GROUP1_OFFSET;
	} else if ((otp_flag & 0x30) == 0x10) {
		addr = OV8856_OTP_AF_DATA_GROUP2_OFFSET;
	} else {
		log_inf(" af group1 group2 all error\n");
		otp_info.af_data[OTP_AF_DATA_SIZE] = OTP_CHECK_FAILED;
		return;
	}
	if (addr != 0) {
		for (i = 0; i < 3; i++)
			af[i] = read_cmos_sensor(addr + i);
		otp_info.af_data[0] = af[0];
		otp_info.af_data[1] = af[1];
		otp_info.af_data[2] = (af[2] >> MOVE_BIT_6) & 0x03;
		otp_info.af_data[3] = (af[2] >> MOVE_BIT_4) & 0x03;
		otp_info.af_data[OTP_AF_DATA_SIZE] = OTP_CHECK_SUCCESSED;
	} else {
		otp_info.af_data[OTP_AF_DATA_SIZE] = OTP_CHECK_FAILED;
	}
	disable_read_otp();
}
int camcal_read_ov8856_sensorotp_func(struct i2c_client *current_client,
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
	ov8856_otp_read_af();
	err = memcpy_s(peeprom_readbuff, OTP_AF_DATA_SIZE + 1, otp_info.af_data,
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
