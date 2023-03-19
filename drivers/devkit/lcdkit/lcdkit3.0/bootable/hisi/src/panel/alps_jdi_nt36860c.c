/*
 * alps_jdi_nt36860c.c
 *
 * alps_jdi_nt36860c lcd driver
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

static void alps_jdi_nt36860c_get_tp_color(struct hisi_panel_info *pinfo)
{
	LCD_KIT_INFO("get_tp_color success\n");
}

static struct lcd_kit_panel_ops alps_jdi_nt36860c_ops = {
	.lcd_kit_get_tp_color = NULL,
};

static int alps_jdi_nt36860c_probe(void)
{
	int ret;

	ret = lcd_kit_panel_ops_register(&alps_jdi_nt36860c_ops);
	if (ret)
		LCD_KIT_ERR("failed\n");
	return ret;
}
