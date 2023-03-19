/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
	{ HI1333_QTECH_SENSOR_ID,      0xA0, Common_read_region },
	{ OV13855_OFILM_TDK_SENSOR_ID, 0xA0, Common_read_region },
	{ OV13855_OFILM_SENSOR_ID,     0xA0, Common_read_region },
	{ S5K3L6_LITEON_SENSOR_ID,     0xA2, Common_read_region },
	{ HI1336_QTECH_SENSOR_ID,      0xA0, Common_read_region },
	{ S5K4H7_TRULY_SENSOR_ID,      0xA0, Common_read_region },
	{ HI846_LUXVISIONS_SENSOR_ID,  0xA0, Common_read_region },
	{ HI846_OFILM_SENSOR_ID,       0xA0, Common_read_region },
	{ GC8054_BYD_SENSOR_ID,        0xA0, Common_read_region },
	{ GC2375_SUNNY_MONO_SENSOR_ID, 0xA8, Common_read_region },
	{ GC2375_SUNNY_SENSOR_ID,      0xA2, Common_read_region },
	{ OV02A10_FOXCONN_SENSOR_ID,   0xA2, Common_read_region },
	{ OV02A10_SUNWIN_SENSOR_ID,    0xA2, Common_read_region },
	{ S5K5E9YX_BYD_SENSOR_ID,      0xA4, Common_read_region },
	{ GC5035_LUXVISIONS_SENSOR_ID, 0xA4, Common_read_region },
	{ IMX258_ZET_SENSOR_ID,        0xA0, Common_read_region },
	{ IMX258_SENSOR_ID,            0xA0, Common_read_region },
	{ S5K3L6_TRULY_SENSOR_ID,      0xA0, Common_read_region },
	{ HI846_TRULY_SENSOR_ID,       0x46, Common_read_region },
	{ GC8034_SUNWIN_SENSOR_ID,     0x6e, Common_read_region },
	{ GC8034_TXD_SENSOR_ID,        0x6e, Common_read_region },
	{ OV8856_HLT_SENSOR_ID,        0x20, Common_read_region },
	{ S5K4H7_TXD_SENSOR_ID,        0x20, Common_read_region },
	/*  ADD before this line */
	{0, 0, 0}       /*end of list */
};

unsigned int cam_cal_get_sensor_list(
	struct stCAM_CAL_LIST_STRUCT **ppCamcalList)
{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


