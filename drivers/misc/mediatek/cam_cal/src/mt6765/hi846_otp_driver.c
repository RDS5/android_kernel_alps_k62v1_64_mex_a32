/*
 * hi846_otp_driver.c
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * camera hi846_otp_drive
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

#define log_info(format, args...)    \
	pr_info(PFX "[%s] " format, __func__, ##args)

static struct otp_t otp_info = {0};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, HI846_IIC_ADDR);

	return get_byte;
}
static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 4, HI846_IIC_ADDR);
}
static void write_cmos_sensor_8(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, HI846_IIC_ADDR);
}

static void hi846_otp_init(void)
{
	write_cmos_sensor(0x2000, 0x0000);
	write_cmos_sensor(0x2002, 0x00FF);
	write_cmos_sensor(0x2004, 0x0000);
	write_cmos_sensor(0x2008, 0x3FFF);
	write_cmos_sensor(0x23FE, 0xC056);
	write_cmos_sensor(0x326E, 0x0000);
	write_cmos_sensor(0x0A00, 0x0000);
	write_cmos_sensor(0x0E04, 0x0012);
	write_cmos_sensor(0x0F08, 0x2F04);
	write_cmos_sensor(0x0F30, 0x001F);
	write_cmos_sensor(0x0F36, 0x001F);
	write_cmos_sensor(0x0F04, 0x3A00);
	write_cmos_sensor(0x0F32, 0x025A);
	write_cmos_sensor(0x0F38, 0x025A);
	write_cmos_sensor(0x0F2A, 0x4124);
	write_cmos_sensor(0x006A, 0x0100);
	write_cmos_sensor(0x004C, 0x0100);
	write_cmos_sensor(0x0A00, 0x0100);
}
static void hi846_otp_enable(void)
{
	write_cmos_sensor_8(0x0A02, 0x01);
	write_cmos_sensor_8(0x0A00, 0x00);
	mdelay(10);
	write_cmos_sensor_8(0x0F02, 0x00);
	write_cmos_sensor_8(0x071A, 0x01);
	write_cmos_sensor_8(0x071B, 0x09);
	write_cmos_sensor_8(0x0D04, 0x01);
	write_cmos_sensor_8(0x0D00, 0x07);
	write_cmos_sensor_8(0x003E, 0x10);
	write_cmos_sensor_8(0x0A00, 0x01);
}
static void hi846_otp_disable(void)
{
	write_cmos_sensor_8(0x0A00, 0x00);
	mdelay(10);
	write_cmos_sensor_8(0x003E, 0x00);
	write_cmos_sensor_8(0x0A00, 0x01);
	mdelay(1);
}
static kal_uint8 hi846_otp_read_byte(kal_uint16 addr)
{
	write_cmos_sensor_8(HI846_OTP_REG_ADDRH, addr >> 8);
	write_cmos_sensor_8(HI846_OTP_REG_ADDRL, addr & 0xFF);
	write_cmos_sensor_8(HI846_OTP_REG_CMD, HI846_OTP_CMD_NORMAL);
	return (kal_uint8)read_cmos_sensor(HI846_OTP_REG_RDATA);
}
static unsigned int otp_read(kal_uint16 addr, kal_uint8 *buf, kal_uint16 size)
{
	int i = 0;

	write_cmos_sensor_8(HI846_OTP_REG_ADDRH, addr >> 8);
	write_cmos_sensor_8(HI846_OTP_REG_ADDRL, addr & 0xFF);
	write_cmos_sensor_8(HI846_OTP_REG_CMD, HI846_OTP_CMD_CONTINUOUS_READ);
	for (i = 0; i < size; i++)
		buf[i] = read_cmos_sensor(HI846_OTP_REG_RDATA);
	return i;
}
static void hi846_otp_read_awb(kal_uint16 addr)
{
	int i = 0;
	unsigned char buf[HI846_OTP_SIZE_AWB] = "0";

	otp_read(addr, buf, HI846_OTP_SIZE_AWB);
	for (i = 0; i < HI846_OTP_SIZE_AWB; i++)
		otp_info.awb_data[i] = buf[i];
}
static void hi846_otp_read_lsc(kal_uint16 addr)
{
	int i = 0;
	unsigned char buf[HI846_OTP_SIZE_LSC] = "0";

	otp_read(addr, buf, HI846_OTP_SIZE_LSC);
	for (i = 0; i < HI846_OTP_SIZE_LSC; i++)
		otp_info.lsc_data[i] = buf[i];
}
static void hi846_otp_read_af(kal_uint16 addr)
{
	unsigned char buf[OTP_AF_DATA_SIZE] = "0";

	otp_read(addr, buf, OTP_AF_DATA_SIZE);
	otp_info.af_data[0] = buf[2];
	otp_info.af_data[1] = buf[0];
	otp_info.af_data[2] = buf[3];
	otp_info.af_data[3] = buf[1];
}
static void hi846_read_otp(void)
{
	kal_uint8 awb_flag = 0;
	kal_uint8 lsc_flag = 0;
	kal_uint32 i = 0;
	kal_uint32 flag = 0;

	const kal_uint16 ADDR_AWB[] = {HI846_OTP_AWB_DATA_GROUP1_OFFSET,
		HI846_OTP_AWB_DATA_GROUP2_OFFSET};
	const kal_uint16 ADDR_LSC[] = {HI846_OTP_LSC_DATA_GROUP1_OFFSET,
		HI846_OTP_LSC_DATA_GROUP2_OFFSET};
	const kal_uint16 ADDR_AF[] = {HI846_OTP_AF_DATA_GROUP1_OFFSET,
		HI846_OTP_AF_DATA_GROUP2_OFFSET};
	hi846_otp_init();
	hi846_otp_enable();

	awb_flag = hi846_otp_read_byte(HI846_OTP_OFFSET_AWB_FLAG);
	lsc_flag = hi846_otp_read_byte(HI846_OTP_OFFSET_LSC_FLAG);

	otp_info.awb_data[HI846_OTP_SIZE_AWB] = OTP_CHECK_FAILED;
	otp_info.lsc_data[HI846_OTP_SIZE_LSC] = OTP_CHECK_FAILED;
	otp_info.af_data[OTP_AF_DATA_SIZE] = OTP_CHECK_FAILED;
	for (i = 0; i < 2; i++) {
		flag = (awb_flag >> (2 * i)) & 0x3;
		if (flag == 0x1) {
			log_info(" awb group%d is valid", i);
			hi846_otp_read_awb(ADDR_AWB[i]);
			otp_info.awb_data[HI846_OTP_SIZE_AWB] = OTP_CHECK_SUCCESSED;
		} else if (flag == 0x0) {
			log_info(" awb group%d is empty", i);
		} else {
			log_info(" awb group%d is invalid", i);
		}

		log_info(" start read lsc group%d ", i);

		flag = (lsc_flag >> (2 * i)) & 0x3;
		if (flag == 0x1) {
			log_info(" lsc group%d is valid", i);
			hi846_otp_read_lsc(ADDR_LSC[i]);
			otp_info.lsc_data[HI846_OTP_SIZE_LSC] = OTP_CHECK_SUCCESSED;
		} else if (flag == 0x0) {
			log_info(" lsc group%d is empty", i);
		} else {
			log_info(" lsc group%d is invalid", i);
		}

		log_info(" start read af group%d ", i);
		flag = hi846_otp_read_byte(ADDR_AF[i]);
		if (flag == 0x1) {
			log_info(" af group%d is valid", i);
			hi846_otp_read_af(ADDR_AF[i] + 1);
			otp_info.af_data[OTP_AF_DATA_SIZE] = OTP_CHECK_SUCCESSED;
		} else {
			log_info(" af group%d is invalid", i);
		}
	}
	hi846_otp_disable();
}

int camcal_read_hi846_sensorotp_func(struct i2c_client *current_client,
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
	peeprom_readbuff = kzalloc(HI846_OTP_TOTAL_SIZE, GFP_KERNEL);
	if (!peeprom_readbuff)
		return -1;
	hi846_read_otp();
	err = memcpy_s(peeprom_readbuff, OTP_AF_DATA_SIZE + 1, otp_info.af_data,
		OTP_AF_DATA_SIZE + 1);
	if (err != EOK) {
		pr_err("memcpy_s fail!\n");
		return -1;
	}
	block_total_size += OTP_AF_DATA_SIZE + 1;
	err = memcpy_s(peeprom_readbuff + block_total_size, HI846_OTP_SIZE_AWB + 1,
		otp_info.awb_data, HI846_OTP_SIZE_AWB + 1);
	if (err != EOK) {
		pr_err("memcpy_s fail!\n");
		return -1;
	}
	block_total_size += HI846_OTP_SIZE_AWB + 1;
	err = memcpy_s(peeprom_readbuff + HI846_OTP_SIZE_AWB + OTP_AF_DATA_SIZE + 2,
		HI846_OTP_SIZE_LSC + 1, otp_info.lsc_data, HI846_OTP_SIZE_LSC + 1);
	if (err != EOK) {
		pr_err("memcpy_s fail!\n");
		return -1;
	}
	block_total_size += HI846_OTP_SIZE_LSC + 1;

	*eeprom_buff = peeprom_readbuff;
	*eeprom_size = block_total_size;
	pr_info("block_size [%d]!\n", block_total_size);
	return block_total_size;
}
