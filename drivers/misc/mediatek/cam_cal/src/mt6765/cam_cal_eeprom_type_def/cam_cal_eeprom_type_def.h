/*
 * cam_cal_eeprom_type_def.h
 *
 * define supported eeprom map info.
 *
 * Copyright (c) 2018-2019 Huawei Technologies Co., Ltd.
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

#ifndef __CAM_CAL_EEPROM_TYPE_DEF_H
#define __CAM_CAL_EEPROM_TYPE_DEF_H

#define EEPROM_MAX_BLOCK_INFO 10
struct eeprom_block_info {
	unsigned int addr_offset;
	unsigned int data_len;
};

struct cam_cal_eeprom_map {
	unsigned int sensor_id;
	unsigned int e2prom_block_num;
	unsigned int e2prom_total_size;
	struct eeprom_block_info e2prom_block_info[EEPROM_MAX_BLOCK_INFO];
};

static struct cam_cal_eeprom_map g_eeprom_map_info[] = {
	#include "hi1333_qtech_eeprom_map.h"
	#include "ov13855_ofilm_eeprom_map.h"
	#include "ov13855_ofilm_tdk_eeprom_map.h"
	#include "s5k3l6_liteon_eeprom_map.h"
	#include "hi1336_qtech_eeprom_map.h"
	#include "gc8054_byd_eeprom_map.h"
	#include "hi846_luxvisions_eeprom_map.h"
	#include "hi846_ofilm_eeprom_map.h"
	#include "s5k4h7_truly_eeprom_map.h"
	#include "gc2375_sunny_mono_eeprom_map.h"
	#include "gc2375_sunny_eeprom_map.h"
	#include "ov02a10_sunwin_eeprom_map.h"
	#include "ov02a10_foxconn_eeprom_map.h"
	#include "s5k5e9yx_byd_eeprom_map.h"
	#include "gc5035_luxvisions_eeprom_map.h"
	#include "imx258_sunny_eeprom_map.h"
	#include "imx258_zet_eeprom_map.h"
	#include "s5k3l6_truly_eeprom_map.h"
};

#endif
