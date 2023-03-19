/*
 * s5k5e9yx_byd_eeprom_map.h
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
#ifndef _S5K5E9YX_BYD_EEPROM_MAP_H
#define _S5K5E9YX_BYD_EEPROM_MAP_H

#if defined(S5K5E9YX_BYD)
{
	.sensor_id = S5K5E9YX_BYD_SENSOR_ID,
	.e2prom_block_info[0] = {
		.addr_offset = 0x00,
		.data_len = 39,
	},
	.e2prom_block_info[1] = {
		.addr_offset = 0x0af4,
		.data_len = 2489,
	},
	.e2prom_block_info[2] = {
		.addr_offset = 0x163F,
		.data_len = 2400,
	},
	.e2prom_block_num = 3,
	.e2prom_total_size = 4928,
},
#endif

#endif
