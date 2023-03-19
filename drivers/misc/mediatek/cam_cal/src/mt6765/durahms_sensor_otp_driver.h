/*
 * durahms_sensor_otp_driver.h
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * camera durahms_sensor_otp_driver.h
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


#ifndef __DURAHMS_SENSOR_OTP_DRIVER__
#define __DURAHMS_SENSOR_OTP_DRIVER__

#define PFX "CAM_CAL"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <securec.h>
#include "cam_cal.h"
#include "cam_cal_define.h"
#include "cam_cal_list.h"
#include "kd_camera_typedef.h"

#define HI846_IIC_ADDR 0x46
#define HI846_OTP_REG_CMD 0x0702
#define HI846_OTP_REG_WDATA 0x0706
#define HI846_OTP_REG_RDATA 0x0708
#define HI846_OTP_REG_ADDRH 0x070A
#define HI846_OTP_REG_ADDRL 0x070B
#define HI846_OTP_CMD_NORMAL 0x00
#define HI846_OTP_CMD_CONTINUOUS_READ 0x01
#define HI846_OTP_CMD_CONTINUOUS_WRITE 0x02
#define HI846_OTP_OFFSET_AWB_FLAG 0x0CD1
#define HI846_OTP_OFFSET_LSC_FLAG 0x0CDD
#define HI846_OTP_AWB_DATA_GROUP1_OFFSET 0x0CD2
#define HI846_OTP_AWB_DATA_GROUP2_OFFSET 0x1441
#define HI846_OTP_LSC_DATA_GROUP1_OFFSET 0x0CDE
#define HI846_OTP_LSC_DATA_GROUP2_OFFSET 0x144D
#define HI846_OTP_AF_DATA_GROUP1_OFFSET 0x1C00
#define HI846_OTP_AF_DATA_GROUP2_OFFSET 0x1C06
#define HI846_OTP_SIZE_AWB 8
#define HI846_OTP_SIZE_LSC 1868
#define HI846_OTP_TOTAL_SIZE 1883

#define GC8034_IIC_ADDR 0x6e
#define GC8034_OTP_REG_CMD 0x0702
#define GC8034_OTP_CMD_NORMAL 0x00
#define GC8034_OTP_CMD_CONTINUOUS_READ 0x01
#define GC8034_OTP_CMD_CONTINUOUS_WRITE 0x02
#define AF_ROM_START 0x3b
#define PAGE_NUM_3 3
#define GC8034_AF_FLAG 0X3a
#define OTP_AF_DATA_SIZE 4
#define OTP_CHECK_SUCCESSED 1
#define OTP_CHECK_FAILED 0

#define OV8856_IIC_ADDR 0x20
#define OV8856_OTP_AF_DATA_GROUP1_OFFSET 0x7020
#define OV8856_OTP_AF_DATA_GROUP2_OFFSET 0x7024

#define S5K4H7_IIC_ADDR 0x20
#define S5K4H7_OTP_AF_FLAG_ADDR 0x0A37
#define S5K4H7_OTP_AF_GROUP1_DATA_ADDR 0x0A39
#define S5K4H7_OTP_AF_GROUP2_DATA_ADDR 0x0A3F
#define S5K4H7_AF_PAGE 21
#define S5K4H7_AF_GROUP1 0x40
#define S5K4H7_AF_GROUP2 0x10
#define S5K4H7_AF_DATA_NUM 13
#define S5K4H7_AF_TEMP 0x50
#define REG_READ_TIMES 10

#define BASE 255
#define MOVE_BIT_2 2
#define MOVE_BIT_4 4
#define MOVE_BIT_6 6
#define MOVE_BIT_8 8
enum {
	OTP_CLOSE = 0,
	OTP_OPEN,
};

struct otp_t {
	kal_uint8 awb_flag;
	kal_uint8 lsc_flag;
	kal_uint8 af_flag;
	kal_uint8 af_data[OTP_AF_DATA_SIZE + 1];
	kal_uint8 awb_data[HI846_OTP_SIZE_AWB + 1];
	kal_uint8 lsc_data[HI846_OTP_SIZE_LSC + 1];
};

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData,
				u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);

#endif //__DURAHMS_SENSOR_OTP_DRIVER__
