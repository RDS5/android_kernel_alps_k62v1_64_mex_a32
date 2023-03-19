/*
 * gc8034_otp_driver.c
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * camera gc8034_otp_drive
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

static struct otp_t otp_info = {0};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[1] = { (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 1, (u8 *)&get_byte, 1, GC8034_IIC_ADDR);
	return get_byte;
}
static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[2] = { (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 2, GC8034_IIC_ADDR);
}
static void gc8034_gcore_enable_otp(kal_uint8 state)
{
	kal_uint8 otp_clk = 0;
	kal_uint8 otp_en = 0;

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
static void gc8034_gcore_initial_otp(void)
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
static kal_uint8 gc8034_read_otp(kal_uint8 page, kal_uint8 addr)
{
	log_inf("E!\n");
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xd4, ((page << 2) & 0x3c) + ((addr >> 5) & 0x03));
	write_cmos_sensor(0xd5, (addr << 3) & 0xff);

	write_cmos_sensor(0xf3, 0x20);

	return  read_cmos_sensor(0xd7);
}
static void gc8034_read_otp_group(kal_uint8 page, kal_uint8 addr,
	kal_uint8 *buff, int size)
{
	kal_uint8 i;
	kal_uint8 regf4 = read_cmos_sensor(0xf4);

	write_cmos_sensor(0xd4, ((page << 2) & 0x3c) + ((addr >> 5) & 0x03));
	write_cmos_sensor(0xd5, (addr << 3) & 0xff);
	mdelay(1);
	write_cmos_sensor(0xf3, 0x20);
	write_cmos_sensor(0xf4, regf4 | 0x02);
	write_cmos_sensor(0xf3, 0x80);

	for (i = 0; i < size; i++)
		buff[i] = read_cmos_sensor(0xd7);

	write_cmos_sensor(0xf3, 0x00);
	write_cmos_sensor(0xf4, regf4 & 0xfd);
}

static void gc8034_otp_read_af(void)
{
	kal_uint16 i = 0;
	kal_uint8 index = 0;
	kal_uint8 flag_af = 0;
	kal_uint32 check = 0;
	kal_uint8 af[OTP_AF_DATA_SIZE] = { 0 };

	gc8034_gcore_initial_otp();
	gc8034_gcore_enable_otp(OTP_OPEN);
	flag_af = gc8034_read_otp(PAGE_NUM_3, GC8034_AF_FLAG);
	log_inf(" flag_af = 0x%x\n ", flag_af);
	otp_info.af_data[OTP_AF_DATA_SIZE] = OTP_CHECK_FAILED;
	for (index = 0; index < 2; index++) {
		switch ((flag_af << (2 * index)) & 0x0c) {
		case 0x00:
			log_inf("GC8034_OTP_AF group %d is Empty !!\n",
				index + 1);
			break;
		case 0x04:
			log_inf("GC8034_OTP_AF group %d is Valid !!\n",
				index + 1);
			gc8034_read_otp_group(PAGE_NUM_3, AF_ROM_START +
				index * OTP_AF_DATA_SIZE,
				&af[0], OTP_AF_DATA_SIZE);
			for (i = 0; i < OTP_AF_DATA_SIZE - 1; i++)
				check += af[i];

			if ((check % BASE + 1) ==
				af[OTP_AF_DATA_SIZE - 1]) {
				otp_info.af_data[0] = (af[0] >> MOVE_BIT_4) & 0x0f;
				otp_info.af_data[1] = af[0] & 0x0f;
				otp_info.af_data[2] = af[1];
				otp_info.af_data[3] = af[2];
				otp_info.af_data[OTP_AF_DATA_SIZE] = OTP_CHECK_SUCCESSED;
			} else {
				log_inf("GC8034_OTP_AF Check sum %d Error !!\n",
					index + 1);
				otp_info.af_data[OTP_AF_DATA_SIZE] = OTP_CHECK_FAILED;
			}
			break;
		case 0x08:
		case 0x0c:
			log_inf("GC8034_OTP_AF group %d is Invalid !!\n",
				index + 1);
			break;
		default:
			break;
		}
	}

	gc8034_gcore_enable_otp(OTP_CLOSE);
}

int camcal_read_gc8034_sensorotp_func(struct i2c_client *current_client,
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
	gc8034_otp_read_af();
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
