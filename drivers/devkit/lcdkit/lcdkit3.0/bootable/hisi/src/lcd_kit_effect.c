/*
 * lcd_kit_effect.c
 *
 * lcdkit display effect for lcd driver
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

#include "lcd_kit_effect.h"
#include "lcd_kit_common.h"
#include "lcd_kit_disp.h"
#include <stdint.h>

#define MAX_READ_SIZE 20

static struct lcdbrightnesscoloroeminfo lcd_color_oeminfo = {0};
static struct panelid lcd_panelid = {0};
static int gamma_len;
/*
 * 1542 equals degamma_r + degamma_g + degamma_b
 * equals (257 + 257 +257) * sizeof(u16)
 */
static uint16_t gamma_lut[GM_LUT_LEN] = {0};

int lcd_kit_get_bright_rgbw_id_from_oeminfo(void)
{
	struct oeminfo_operators *oeminfo_ops = NULL;

	oeminfo_ops = get_operators(OEMINFO_MODULE_NAME_STR);
	if (!oeminfo_ops) {
		HISI_FB_ERR("get_operators error\n");
		return LCD_KIT_FAIL;
	}

	if (oeminfo_ops->get_oeminfo(OEMINFO_LCD_BRIGHT_COLOR_CALIBRATION,
		sizeof(struct lcdbrightnesscoloroeminfo),
		(char *)&lcd_color_oeminfo)) {
		HISI_FB_ERR("get bright color oeminfo error\n");
		return LCD_KIT_FAIL;
	}

	return LCD_KIT_OK;
}

int lcd_kit_set_bright_rgbw_id_to_oeminfo(void)
{
	struct oeminfo_operators *oeminfo_ops = NULL;

	oeminfo_ops = get_operators(OEMINFO_MODULE_NAME_STR);
	if (!oeminfo_ops) {
		HISI_FB_ERR("get_operators error\n");
		return LCD_KIT_FAIL;
	}

	if (oeminfo_ops->set_oeminfo(OEMINFO_LCD_BRIGHT_COLOR_CALIBRATION,
		sizeof(struct lcdbrightnesscoloroeminfo),
		(char *)&lcd_color_oeminfo)) {
		HISI_FB_ERR("set bright color oeminfo error\n");
		return LCD_KIT_FAIL;
	}

	return LCD_KIT_OK;
}

int lcd_kit_get_panel_id_info(struct hisi_fb_data_type *hisifd)
{
	int ret;
	char read_value[MAX_READ_SIZE] = {0};

	ret = lcd_kit_dsi_cmds_rx((void *)hisifd, (uint8_t *)read_value,
		MAX_READ_SIZE,
		&disp_info->brightness_color_uniform.modulesn_cmds);
	if (ret != 0)
		LCD_KIT_ERR("rx failed\n");
	/* 24 means 24bits 16 means 16bits 8 means 8bits */
	lcd_panelid.modulesn = (((uint32_t)read_value[0] & 0xFF) << 24) |
			       (((uint32_t)read_value[1] & 0xFF) << 16) |
			       (((uint32_t)read_value[2] & 0xFF) << 8)  |
			       ((uint32_t)read_value[3] & 0xFF);

	ret = memset_s(read_value, MAX_READ_SIZE, 0, MAX_READ_SIZE);
	if (ret != 0) {
		LCD_KIT_ERR("memset_s error\n");
		return LCD_KIT_FAIL;
	}
	ret = lcd_kit_dsi_cmds_rx((void *)hisifd, (uint8_t *)read_value,
		MAX_READ_SIZE,
		&disp_info->brightness_color_uniform.equipid_cmds);
	if (ret < 0) {
		HISI_FB_ERR("check_cmds fail\n");
		return ret;
	}
	lcd_panelid.equipid = (uint32_t)read_value[0];
	ret = memset_s(read_value, MAX_READ_SIZE, 0, MAX_READ_SIZE);
	if (ret != 0) {
		LCD_KIT_ERR("memset_s error\n");
		return LCD_KIT_FAIL;
	}
	ret = lcd_kit_dsi_cmds_rx((void *)hisifd, (uint8_t *)read_value,
		MAX_READ_SIZE,
		&disp_info->brightness_color_uniform.modulemanufact_cmds);
	if (ret < 0) {
		HISI_FB_ERR("check_cmds fail\n");
		return ret;
	}
	/* 16 means 16 bits 8 means 8 bits */
	lcd_panelid.modulemanufactdate = (((uint32_t)read_value[0] & 0xFF) << 16) |
					 (((uint32_t)read_value[1] & 0xFF) << 8)  |
					 ((uint32_t)read_value[2] & 0xFF);

	lcd_panelid.vendorid = 0xFFFF;
	LCD_KIT_INFO("lcd_panelid.modulesn = 0x%x\n", lcd_panelid.modulesn);
	LCD_KIT_INFO("lcd_panelid.equipid = 0x%x\n", lcd_panelid.equipid);
	LCD_KIT_INFO("lcd_panelid.modulemanufactdate = 0x%x\n",
		lcd_panelid.modulemanufactdate);
	LCD_KIT_INFO("lcd_panelid.vendorid = 0x%x\n", lcd_panelid.vendorid);
	return LCD_KIT_OK;
}

unsigned long lcd_kit_get_color_mem_addr(void)
{
	return HISI_SUB_RESERVED_BRIGHTNESS_CHROMA_MEM_PHYMEM_BASE;
}

unsigned long lcd_kit_get_color_mem_size(void)
{
	return HISI_SUB_RESERVED_BRIGHTNESS_CHROMA_MEM_PHYMEM_SIZE;
}

void lcd_kit_write_oeminfo_to_mem(void)
{
	int ret;
	unsigned long mem_addr;
	unsigned long mem_size;

	mem_addr = lcd_kit_get_color_mem_addr();
	mem_size = lcd_kit_get_color_mem_size();
	ret = memcpy_s((void *)(uintptr_t)mem_addr, mem_size, &lcd_color_oeminfo,
		sizeof(lcd_color_oeminfo));
	if (ret != 0)
		LCD_KIT_ERR("memcpy_s error\n");
}

void lcd_kit_bright_rgbw_id_from_oeminfo(struct hisi_fb_data_type *hisifd)
{
	int ret;

	if (!hisifd) {
		HISI_FB_ERR("pointer is NULL\n");
		return;
	}
	ret = lcd_kit_get_panel_id_info(hisifd);
	if (ret != LCD_KIT_OK)
		HISI_FB_ERR("read panel id from lcd failed!\n");

	ret = lcd_kit_get_bright_rgbw_id_from_oeminfo();
	if (ret != LCD_KIT_OK)
		HISI_FB_ERR("read oeminfo failed!\n");

	if (lcd_color_oeminfo.id_flag == 0) {
		HISI_FB_INFO("panel id_flag is 0\n");
		lcd_color_oeminfo.panel_id = lcd_panelid;
		lcd_color_oeminfo.id_flag = 1;
		ret = lcd_kit_set_bright_rgbw_id_to_oeminfo();
		if (ret != LCD_KIT_OK)
			HISI_FB_ERR("write oeminfo failed!\n");
		lcd_kit_write_oeminfo_to_mem();
		return;
	}
	if (!memcmp(&lcd_color_oeminfo.panel_id, &lcd_panelid,
		sizeof(lcd_color_oeminfo.panel_id))) {
		HISI_FB_INFO("panel id is same\n");
		lcd_kit_write_oeminfo_to_mem();
		return;
	}
	lcd_color_oeminfo.panel_id = lcd_panelid;
	if ((lcd_color_oeminfo.panel_id.modulesn == lcd_panelid.modulesn) &&
		(lcd_color_oeminfo.panel_id.equipid == lcd_panelid.equipid) &&
		(lcd_color_oeminfo.panel_id.modulemanufactdate == lcd_panelid.modulemanufactdate)) {
		HISI_FB_INFO("panle id and oeminfo panel id is same\n");
		if (lcd_color_oeminfo.tc_flag == 0) {
			HISI_FB_INFO("panle id and oeminfo panel id is same, set tc_flag to true\n");
			lcd_color_oeminfo.tc_flag = 1;
		}
	} else {
		HISI_FB_INFO("panle id and oeminfo panel id is not same, set tc_flag to false\n");
		lcd_color_oeminfo.tc_flag = 0;
	}
	ret = lcd_kit_set_bright_rgbw_id_to_oeminfo();
	if (ret != LCD_KIT_OK)
		HISI_FB_ERR("write panel id wight and color info to oeminfo failed!\n");
	lcd_kit_write_oeminfo_to_mem();
	return;
}

static int lcd_kit_read_gamma_from_oeminfo(void)
{

	struct oeminfo_operators *oeminfo_ops = NULL;

	oeminfo_ops = get_operators(OEMINFO_MODULE_NAME_STR);
	if (!oeminfo_ops) {
		HISI_FB_ERR("get_operators error\n");
		return LCD_KIT_FAIL;
	}

	if (oeminfo_ops->get_oeminfo(OEMINFO_GAMMA_INDEX, OEMINFO_GAMMA_LEN,
		(char *)&gamma_len)) {
		HISI_FB_ERR("get gamma oeminfo len error, gamma len = %d!\n",
			gamma_len);
		return LCD_KIT_FAIL;
	}
	if (gamma_len != GM_IGM_LEN) {
		HISI_FB_ERR("gamma oeminfo len is not correct\n");
		return LCD_KIT_FAIL;
	}

	if (oeminfo_ops->get_oeminfo(OEMINFO_GAMMA_DATA, GM_IGM_LEN,
		(char *)gamma_lut)) {
		HISI_FB_ERR("get gamma oeminfo data error\n");
		return LCD_KIT_FAIL;
	}

	return LCD_KIT_OK;
}

int lcd_kit_write_gm_to_reserved_mem(void)
{
	int ret;
	unsigned long gm_addr = HISI_SUB_RESERVED_LCD_GAMMA_MEM_PHYMEM_BASE;
	unsigned long gm_size = HISI_SUB_RESERVED_LCD_GAMMA_MEM_PHYMEM_SIZE;

	if (gm_size < GM_IGM_LEN + OEMINFO_GAMMA_LEN) {
		HISI_FB_ERR("gamma mem size is not enough !\n");
		return LCD_KIT_FAIL;
	}

	ret = lcd_kit_read_gamma_from_oeminfo();
	if (ret) {
		writel(0, gm_addr);
		HISI_FB_ERR("can not get oeminfo gamma data!\n");
		return LCD_KIT_FAIL;
	}

	writel((unsigned int)GM_IGM_LEN, gm_addr);
	gm_addr += OEMINFO_GAMMA_LEN;
	ret = memcpy_s((void *)(uintptr_t)gm_addr, gm_size, gamma_lut, GM_IGM_LEN);
	if (ret != 0) {
		LCD_KIT_ERR("memcpy_s error\n");
		return LCD_KIT_FAIL;
	}
	return LCD_KIT_OK;
}
