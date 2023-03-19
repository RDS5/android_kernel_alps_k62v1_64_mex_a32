/*
 * lcd_kit_common.h
 *
 * lcdkit common function head file for lcd driver
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

#ifndef _LCD_KIT_COMMON_H_
#define _LCD_KIT_COMMON_H_
#include <types.h>
#include <stdarg.h>
#include "lcd_kit_adapt.h"
#include "lcd_kit_bl.h"

/* log level */
#define MSG_LEVEL_ERROR		1
#define MSG_LEVEL_WARNING	2
#define MSG_LEVEL_INFO		3
#define MSG_LEVEL_DEBUG		4
#define LCD_POWER_LEN		3

#define LCD_RESET_HIGH 1

#define LCD_KIT_FAIL	(-1)
#define LCD_KIT_OK	0

#define MAX_REG_READ_COUNT	4

struct lcd_kit_common_ops *lcd_kit_get_common_ops(void);
#define common_ops	lcd_kit_get_common_ops()
struct lcd_kit_common_desc *lcd_kit_get_common_info(void);
#define common_info	lcd_kit_get_common_info()
struct lcd_kit_power_seq_desc *lcd_kit_get_power_seq(void);
#define power_seq	lcd_kit_get_power_seq()
struct lcd_kit_power_desc *lcd_kit_get_power_handle(void);
#define power_hdl	lcd_kit_get_power_handle()

/* enum */
enum lcd_kit_mipi_ctrl_mode {
	LCD_KIT_DSI_LP_MODE,
	LCD_KIT_DSI_HS_MODE,
};

enum lcd_kit_event {
	EVENT_NONE,
	EVENT_VCI,
	EVENT_IOVCC,
	EVENT_VSP,
	EVENT_VSN,
	EVENT_RESET,
	EVENT_MIPI,
	EVENT_EARLY_TS,
	EVENT_LATER_TS,
	EVENT_VDD,
	EVENT_AOD,
};

enum lcd_kit_power_mode {
	NONE_MODE,
	REGULATOR_MODE,
	GPIO_MODE,
};

enum lcd_kit_power_type {
	LCD_KIT_VCI,
	LCD_KIT_IOVCC,
	LCD_KIT_VSP,
	LCD_KIT_VSN,
	LCD_KIT_RST,
	LCD_KIT_BL,
	LCD_KIT_TP_RST,
	LCD_KIT_VDD,
	LCD_KIT_AOD,
	LCD_KIT_POWERIC,
	LCD_KIT_BUTT,
};

enum bl_order {
	BL_BIG_ENDIAN,
	BL_LITTLE_ENDIAN,
};

enum {
	LCD_KIT_WAIT_US = 0,
	LCD_KIT_WAIT_MS,
};

/*
 * struct definition
 */
struct lcd_kit_dsi_cmd_desc {
	char dtype; /* data type */
	char last; /* last in chain */
	char vc; /* virtual chan */
	char ack; /* ask ACK from peripheral */
	char wait; /* ms */
	char waittype;
	char dlen; /* 8 bits */
	char *payload;
} __packed;

struct lcd_kit_dsi_cmd_desc_header {
	char dtype; /* data type */
	char last; /* last in chain */
	char vc; /* virtual chan */
	char ack; /* ask ACK from peripheral */
	char wait; /* ms */
	char waittype;
	char dlen; /* 8 bits */
};

/* dsi cmd struct */
struct lcd_kit_dsi_panel_cmds {
	char *buf;
	int blen;
	struct lcd_kit_dsi_cmd_desc *cmds;
	int cmd_cnt;
	int link_state;
	u32 flags;
};

struct lcd_kit_array_data {
	uint32_t *buf;
	int cnt;
};

struct lcd_kit_arrays_data {
	struct lcd_kit_array_data *arry_data;
	int cnt;
};

struct lcd_kit_backlight {
	u32 order;
	u32 bl_min;
	u32 bl_max;
	struct lcd_kit_dsi_panel_cmds bl_cmd;
};

struct lcd_kit_effect_on_desc {
	uint32_t support;
	struct lcd_kit_dsi_panel_cmds cmds;
};

struct lcd_kit_check_reg_dsm {
	u32 support;
	u32 support_dsm_report;
	struct lcd_kit_dsi_panel_cmds cmds;
	struct lcd_kit_array_data value;
};

struct lcd_kit_power_seq_desc {
	/* power on sequence */
	struct lcd_kit_arrays_data power_on_seq;
	/* power off sequence */
	struct lcd_kit_arrays_data power_off_seq;
	/* power on low power sequence */
	struct lcd_kit_arrays_data panel_on_lp_seq;
	/* power on high speed sequence */
	struct lcd_kit_arrays_data panel_on_hs_seq;
	/* power off high speed sequence */
	struct lcd_kit_arrays_data panel_off_hs_seq;
	/* power off low power sequence */
	struct lcd_kit_arrays_data panel_off_lp_seq;
};

struct lcd_kit_power_desc {
	struct lcd_kit_array_data lcd_vci;
	struct lcd_kit_array_data lcd_iovcc;
	struct lcd_kit_array_data lcd_vsp;
	struct lcd_kit_array_data lcd_vsn;
	struct lcd_kit_array_data lcd_rst;
	struct lcd_kit_array_data lcd_backlight;
	struct lcd_kit_array_data lcd_te0;
	struct lcd_kit_array_data tp_rst;
	struct lcd_kit_array_data lcd_vdd;
	struct lcd_kit_array_data lcd_aod;
	struct lcd_kit_array_data lcd_poweric;
};

struct lcd_kit_common_desc {
	/* panel on command */
	struct lcd_kit_dsi_panel_cmds panel_on_cmds;
	/* panel off command */
	struct lcd_kit_dsi_panel_cmds panel_off_cmds;
	/* backlight */
	struct lcd_kit_backlight backlight;
	/* effect on */
	struct lcd_kit_effect_on_desc effect_on;
	/* check backlight short/open */
	u32 check_bl_support;
	/* power on check reg */
	struct lcd_kit_check_reg_dsm check_reg_on;
	/* power off check reg */
	struct lcd_kit_check_reg_dsm check_reg_off;
};
struct lcd_kit_adapt_ops {
	int (*mipi_tx)(void *hld, struct lcd_kit_dsi_panel_cmds *cmds);
	int (*mipi_rx)(void *hld, u8 *out, int out_len,
		struct lcd_kit_dsi_panel_cmds *cmds);
	int (*gpio_enable)(u32 type);
	int (*gpio_disable)(u32 type);
	int (*regulator_enable)(u32 type);
	int (*regulator_disable)(u32 type);
	int (*buf_trans)(const char *inbuf, int inlen, char **outbuf,
		int *outlen);
	int (*get_data_by_property)(const char *compatible,
		const char *propertyname, int **data, u32 *len);
	int (*get_string_by_property)(const char *compatible,
		const char *propertyname, char *out_str, unsigned int length);
	int (*change_dts_status_by_compatible)(const char *name);
};

struct lcd_kit_common_ops {
	int (*panel_power_on)(void *hld);
	int (*panel_on_lp)(void *hld);
	int (*panel_on_hs)(void *hld);
	int (*panel_off_hs)(void *hld);
	int (*panel_off_lp)(void *hld);
	int (*panel_power_off)(void *hld);
	int (*set_mipi_backlight)(void *hld, u32 bl_level);
	int (*common_init)(const char *compatible);
};

/* function declare */
struct lcd_kit_adapt_ops *lcd_kit_get_adapt_ops(void);
int lcd_kit_parse_get_u64_default(const char *np,
	const char *propertyname, uint64_t *value, uint64_t def_val);
int lcd_kit_parse_get_u32_default(const char *np,
	const char *propertyname, u32 *value, u32 def_val);
int lcd_kit_parse_get_u8_default(const char *np,
	const char *propertyname, u8 *value, u32 def_val);
int lcd_kit_parse_array(const char *np, const char *propertyname,
	struct lcd_kit_array_data *out);
int lcd_kit_parse_u32_array(const char *np, const char *propertyname,
	u32 out[], unsigned int len);
int lcd_kit_parse_dcs_cmds(const char *np,
	const char *propertyname, const char *link_key,
	struct lcd_kit_dsi_panel_cmds *pcmds);
int lcd_kit_adapt_register(struct lcd_kit_adapt_ops *ops);
int lcd_kit_parse_arrays(const char *np, const char *propertyname,
	struct lcd_kit_arrays_data *out, u32 num);
void lcd_kit_delay(int wait, int waittype);
#endif
