/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef _SENSOR_LIST_H
#define _SENSOR_LIST_H

#ifndef CONFIG_CUSTOM_KERNEL_SENSORHUB
struct sensorInfo_NonHub_t {
	char name[16];
};
int sensorlist_register_deviceinfo(int sensor,
		struct sensorInfo_NonHub_t *devinfo);
#endif
#if defined(CONFIG_MTK_ABOV_SAR)
struct mag_dev_info_t {
	char libname[16];
	int8_t layout;
	int8_t deviceid;
};

struct sensorInfo_t {
	union {
		char name[16];
		struct mag_dev_info_t mag_dev_info;
	};
};
int sensorlist_register_deviceinfo(int sensor,
		struct sensorInfo_t *devinfo);

#endif

#endif
