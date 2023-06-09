/*
 * gc2375_sunny_mono_eeprom_map.h
 *
 * eeprom map
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
#ifndef _GC2375_SUNNY_MONO_EEPROM_MAP_H
#define _GC2375_SUNNY_MONO_EEPROM_MAP_H

#if defined(GC2375_SUNNY_MONO)
{
	.sensor_id = GC2375_SUNNY_MONO_SENSOR_ID,
	.e2prom_block_info[0] = {
		.addr_offset = 0x00,
		.data_len = 39,
	},
	.e2prom_block_info[1] = {
		.addr_offset = 0x15ea,
		.data_len = 2400,
	},
	.e2prom_block_num = 2,
	.e2prom_total_size = 2439,
},
#endif

#endif
