/*
 * s5k3l6_truly_eeprom.c
 *
 * Copyright (c) 2019-2020 Huawei Technologies Co., Ltd.
 *
 * s5k3l6_truly eeprom config
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
#ifndef _S5K3L6_TRULY_EEPROM_MAP_H
#define _S5K3L6_TRULY_EEPROM_MAP_H

#if defined(S5K3L6_TRULY)
{
	.sensor_id = S5K3L6_TRULY_SENSOR_ID,
	.e2prom_block_info[0] = {
		.addr_offset = 0x00,
		.data_len = 39,
	},
	.e2prom_block_info[1] = {
		.addr_offset = 0x23c3,
		.data_len = 4320,
	},
	.e2prom_block_num = 2,
	.e2prom_total_size = 4359,
},
#endif

#endif
