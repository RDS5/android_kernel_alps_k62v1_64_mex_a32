/*
 * ov02a10_sunwin_eeprom_map.h
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
#ifndef _OV02A10_SUNWIN_EEPROM_MAP_H
#define _OV02A10_SUNWIN_EEPROM_MAP_H

#if defined(OV02A10_SUNWIN)
{
	.sensor_id = OV02A10_SUNWIN_SENSOR_ID,
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
