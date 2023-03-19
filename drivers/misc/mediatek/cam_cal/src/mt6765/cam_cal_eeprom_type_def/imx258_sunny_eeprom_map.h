/*
 * imx258_sunny_eeprom_map.h
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
#ifndef __IMX258_SUNNY_EEPROM_MAP_H
#define __IMX258_SUNNY_EEPROM_MAP_H

#if defined(IMX258_SUNNY)
{
	.sensor_id = IMX258_SENSOR_ID,
	.e2prom_block_num = 2,
	.e2prom_total_size = 4359,
	.e2prom_block_info[0] = {
		.addr_offset = 0x00,
		.data_len = 39,
	},
	.e2prom_block_info[1] = {
		.addr_offset = 0x23c3,
		.data_len = 4320,
	},
},
#endif /* IMX258_SUNNY */

#endif /* __IMX258_SUNNY_EEPROM_MAP_H */
