/*
 * lcd_kit_utils.c
 *
 * lcdkit utils function for lcd driver
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

#include "lcd_kit_utils.h"
#include "lcd_kit_disp.h"
#include "lcd_kit_power.h"
#include "lcd_kit_utils.h"
#include "lcd_kit_panels.h"

u32 lcd_kit_get_blmaxnit(struct mtk_panel_info *pinfo)
{
	u32 bl_max_nit;
	u32 lcd_kit_brightness_ddic_info;

	lcd_kit_brightness_ddic_info =
		pinfo->blmaxnit.lcd_kit_brightness_ddic_info;
	if ((pinfo->blmaxnit.get_blmaxnit_type == GET_BLMAXNIT_FROM_DDIC) &&
		(lcd_kit_brightness_ddic_info > BL_MIN) &&
		(lcd_kit_brightness_ddic_info < BL_MAX)) {
		bl_max_nit =
			(lcd_kit_brightness_ddic_info < BL_REG_NOUSE_VALUE) ?
			(lcd_kit_brightness_ddic_info + BL_NIT) :
			(lcd_kit_brightness_ddic_info + BL_NIT - 1);
	} else {
		bl_max_nit = pinfo->bl_max_nit;
	}

	return bl_max_nit;
}

char *lcd_kit_get_compatible(uint32_t product_id, uint32_t lcd_id)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(lcd_kit_map); ++i) {
		if ((lcd_kit_map[i].board_id == product_id) &&
		    (lcd_kit_map[i].gpio_id == lcd_id)) {
			return lcd_kit_map[i].compatible;
		}
	}
	/* use defaut panel */
	return LCD_KIT_DEFAULT_COMPATIBLE;
}

char *lcd_kit_get_lcd_name(uint32_t product_id, uint32_t lcd_id)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(lcd_kit_map); ++i) {
		if ((lcd_kit_map[i].board_id == product_id) &&
		    (lcd_kit_map[i].gpio_id == lcd_id)) {
			return lcd_kit_map[i].lcd_name;
		}
	}
	/* use defaut panel */
	return LCD_KIT_DEFAULT_PANEL;
}

static int lcd_kit_parse_disp_info(char *compatible)
{
	if (!compatible) {
		LCD_KIT_ERR("compatible is NULL!\n");
		return LCD_KIT_FAIL;
	}

	/* quickly sleep out */
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,quickly-sleep-out-support",
		&disp_info->quickly_sleep_out.support, 0);
	if (disp_info->quickly_sleep_out.support) {
		lcd_kit_parse_get_u32_default(compatible,
			"lcd-kit,quickly-sleep-out-interval",
			&disp_info->quickly_sleep_out.interval, 0);
	}
	/* tp color support */
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,tp-color-support",
		&disp_info->tp_color.support, 0);
	if (disp_info->tp_color.support) {
		lcd_kit_parse_dcs_cmds(compatible, "lcd-kit,tp-color-cmds",
			"lcd-kit,tp-color-cmds-state",
			&disp_info->tp_color.cmds);
	}
	return LCD_KIT_OK;
}

static int lcd_kit_pinfo_init(char *compatible, struct mtk_panel_info *pinfo)
{
	int ret = LCD_KIT_OK;

	if (!compatible) {
		LCD_KIT_ERR("compatible is null\n");
		return LCD_KIT_FAIL;
	}
	if (!pinfo) {
		LCD_KIT_ERR("pinfo is null\n");
		return LCD_KIT_FAIL;
	}
	/* panel info */
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,panel-xres",
		&pinfo->xres, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,panel-yres",
		&pinfo->yres, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,panel-width",
		&pinfo->width, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,panel-height",
		&pinfo->height, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,panel-orientation",
		&pinfo->orientation, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,panel-bpp",
		&pinfo->bpp, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,panel-bgr-fmt",
		&pinfo->bgr_fmt, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,panel-bl-type",
		&pinfo->bl_set_type, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,panel-bl-min",
		&pinfo->bl_min, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,panel-bl-max",
		&pinfo->bl_max, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,panel-cmd-type",
		&pinfo->type, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,panel-pxl-clk",
		&pinfo->pxl_clk_rate, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,panel-pxl-clk-div",
		&pinfo->pxl_clk_rate_div, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,panel-data-rate",
		&pinfo->data_rate, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,h-back-porch",
		&pinfo->ldi.h_back_porch, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,h-front-porch",
		&pinfo->ldi.h_front_porch, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,h-pulse-width",
		&pinfo->ldi.h_pulse_width, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,v-back-porch",
		&pinfo->ldi.v_back_porch, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,v-front-porch",
		&pinfo->ldi.v_front_porch, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,v-pulse-width",
		&pinfo->ldi.v_pulse_width, 0);
	lcd_kit_parse_get_u32_default(compatible, "lcd-kit,mipi-lane-nums",
		&pinfo->mipi.lane_nums, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,mipi-clk-post-adjust",
		&pinfo->mipi.clk_post_adjust, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,mipi-rg-vrefsel-vcm-adjust",
		&pinfo->mipi.rg_vrefsel_vcm_adjust, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,mipi-phy-mode",
		&pinfo->mipi.phy_mode, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,mipi-lp11-flag",
		&pinfo->mipi.lp11_flag, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,mipi-non-continue-enable",
		&pinfo->mipi.non_continue_en, 0);

	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,panel-lcm-type",
		&pinfo->panel_lcm_type, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,panel-ldi-dsi-mode",
		&pinfo->panel_dsi_mode, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,panel-dsi-switch-mode",
		&pinfo->panel_dsi_switch_mode, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,panel-ldi-trans-seq",
		&pinfo->panel_trans_seq, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,panel-ldi-data-padding",
		&pinfo->panel_data_padding, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,panel-ldi-packet-size",
		&pinfo->panel_packtet_size, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,panel-ldi-ps",
		&pinfo->panel_ps, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,panel-vfp-lp",
		&pinfo->ldi.v_front_porch_forlp, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,panel-fbk-div",
		&pinfo->pxl_fbk_div, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,ssc-disable",
		&pinfo->ssc_disable, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,ssc-range",
		&pinfo->ssc_range, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,panel-bl-ic-ctrl-type",
		&pinfo->bl_ic_ctrl_mode, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,panel-bias-ic-ctrl-type",
		&pinfo->bias_ic_ctrl_mode, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,panel-bl-max-nit",
		&pinfo->bl_max_nit, 0);
	lcd_kit_parse_get_u32_default(compatible,
		"lcd-kit,panel-getblmaxnit-type",
		&pinfo->blmaxnit.get_blmaxnit_type, 0);
	if (pinfo->blmaxnit.get_blmaxnit_type == GET_BLMAXNIT_FROM_DDIC) {
		lcd_kit_parse_dcs_cmds(compatible,
			"lcd-kit,panel-bl-maxnit-command",
			"lcd-kit,panel-bl-maxnit-command-state",
			&pinfo->blmaxnit.bl_maxnit_cmds);
	}
	return ret;
}

int lcd_kit_parse_dt(char *compatible)
{
	if (!compatible) {
		LCD_KIT_ERR("compatible is null");
		return LCD_KIT_FAIL;
	}
	/* parse display info */
	lcd_kit_parse_disp_info(compatible);
	return LCD_KIT_OK;
}

int lcd_kit_utils_init(struct mtk_panel_info *pinfo)
{
	int ret = LCD_KIT_OK;

	if (!pinfo) {
		LCD_KIT_ERR("pinfo is NULL!\n");
		return LCD_KIT_FAIL;
	}

	/* pinfo init */
	lcd_kit_pinfo_init(disp_info->compatible, pinfo);
	/* parse panel dts */
	lcd_kit_parse_dt(disp_info->compatible);
	return ret;
}
