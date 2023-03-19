/*
 * Copyright (c) 2018-2019 Huawei Technologies Co., Ltd.
 *
 * Description: driver for camera sensor module
 * Author: wangguoying@huawei.com
 * Date: 2018-10-29
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

#ifndef __EEPROM_DRIVER_HW_ADAPT_H
#define __EEPROM_DRIVER_HW_ADAPT_H
#include <linux/i2c.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"

#define EEPROM_READ_DATA_SIZE_8 8
#define EEPROM_READ_FAIL_RETRY_TIMES 3

enum
{
    SENSOR_DEV_NONE = 0x00,
    SENSOR_DEV_MAIN = 0x01,
    SENSOR_DEV_SUB  = 0x02,
    SENSOR_DEV_PIP  = 0x03,
    SENSOR_DEV_MAIN_2 = 0x04,
    SENSOR_DEV_MAIN_3D = 0x05,
    SENSOR_DEV_SUB_2 = 0x08,
    SENSOR_DEV_MAIN_3 = 0x10,
    SENSOR_DEV_SUB_3 = 0x20,
    SENSOR_DEV_MAIN_4 = 0x40,
    SENSOR_DEV_MAX = 0x50
};

struct cam_cal_map {
	unsigned int sensor_id;
	unsigned int device_id;
	unsigned int i2c_addr;
	unsigned char *eeprom_buff;
	unsigned int buff_size;
	int (*camcal_read_func)(struct i2c_client *, unsigned int,
				     unsigned char **, unsigned int *);
	bool active_flag;
};

int eeprom_hal_get_data(unsigned char *data,
			unsigned int sensor_id,
			unsigned int size);

#endif
