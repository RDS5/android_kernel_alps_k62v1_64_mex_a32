/*
 * huawei_mtk_battery.c
 *
 * battery solution of huawei
 *
 * Copyright (c) 2019-2019 Huawei Technologies Co., Ltd.
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

#include <linux/power/huawei_mtk_battery.h>
#include <linux/power/huawei_mtk_charger.h>
#include <linux/power/huawei_charger.h>
#include <linux/of.h>
#include "mtk_battery_internal.h"
#include "mtk_charger_intf.h"
#include "../../../auxadc/mtk_auxadc.h"

#define HLTHERM_BATTERY_TEMP_UI_MAX    250
#define FACTORY_LOW_BATT_CAPACITY      10
#define HLTHERM_LOW_BATT_CAPACITY      30
#define DELAY_FOR_QRY_SOC              5
#define BATT_HEALTH_TEMP_HOT           60
#define BATT_HEALTH_TEMP_WARM          48
#define BATT_HEALTH_TEMP_COOL          10
#define BATT_HEALTH_TEMP_COLD          (-20)
#define AGING_FACTOR_SKIP_TIMES        2
#define TEMP_UNIT_CONVERT              10
#define VOLT_GROUP_MAX                 5
#define BAT_BRAND_LEN_MAX              16
#define BAT_ID_CHANNEL                 4
#define BAT_VENDOR_NAME_LEN            32
#define DES_BASE                       10
#define DEFAULT_ID_VOL_LOW             250000
#define DEFAULT_ID_VOL_HIGH            375000
#define MAX_ID_VOL                     1000000

static int g_batt_len;
static int g_batt_id_channel = BAT_ID_CHANNEL;
static int g_min_fcc = MIN_DESIGN_UAH;
static int g_max_fcc = MAX_DESIGN_UAH;
static int g_fcc_design = MAX_DESIGN_UAH;

struct batt_id_voltage {
	char vendor_name[BAT_VENDOR_NAME_LEN];
	int id_vol_low;
	int id_vol_high;
};

static struct batt_id_voltage g_batt_id[VOLT_GROUP_MAX];

enum bat_id_info {
	PARA_BAT_ID = 0,
	PARA_VOL_LOW,
	PARA_VOL_HIGH,
	PARA_BAT_TOTAL,
};

void set_ui_batt_temp_hltherm(int batt_temp, int *ui_temp)
{
#ifdef CONFIG_HLTHERM_RUNTEST
	if (!ui_temp)
		return;

	if ((batt_temp * TEMP_UNIT_CONVERT) > HLTHERM_BATTERY_TEMP_UI_MAX)
		*ui_temp = HLTHERM_BATTERY_TEMP_UI_MAX;
#endif
}

void set_low_capacity_hltherm(int batt_capacity, int *set_capacity)
{
	if (!set_capacity)
		return;
#ifdef CONFIG_HLTHERM_RUNTEST
	if (batt_capacity < HLTHERM_LOW_BATT_CAPACITY)
		*set_capacity = HLTHERM_LOW_BATT_CAPACITY;
#else
	if (strstr(saved_command_line, "androidboot.swtype=factory")) {
		if (batt_capacity < FACTORY_LOW_BATT_CAPACITY) {
			*set_capacity = FACTORY_LOW_BATT_CAPACITY;
			pr_err("FACTORY set low capacity to 10\n");
		}
	}
#endif
}

void set_power_supply_health(int batt_temp, int *health_status)
{
	if (!health_status)
		return;

	if (batt_temp > BATT_HEALTH_TEMP_HOT)
		*health_status = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (batt_temp < BATT_HEALTH_TEMP_COLD)
		*health_status = POWER_SUPPLY_HEALTH_COLD;
	else if (batt_temp > BATT_HEALTH_TEMP_WARM)
		*health_status = POWER_SUPPLY_HEALTH_WARM;
	else if (batt_temp < BATT_HEALTH_TEMP_COOL)
		*health_status = POWER_SUPPLY_HEALTH_COOL;
	else
		*health_status = POWER_SUPPLY_HEALTH_GOOD;
}

int fg_get_soc_by_ocv(int ocv)
{
	fg_ocv_query_soc(ocv);
	msleep(DELAY_FOR_QRY_SOC);
	return gm.algo_ocv_to_soc;
}

int fg_get_fcc(void)
{
	return gm.fcc;
}

void huawei_mtk_battery_dts_parse(struct device_node *np)
{
	int fcc = 0;
	unsigned int vbus_r = 0;

	if (!np)
		return;

	if (!of_property_read_u32(np, "rbat_pull_up_r", &vbus_r))
		gm.rbat.rbat_pull_up_r = vbus_r;

	if (!of_property_read_u32(np, "vbus_r_charger_1", &vbus_r))
		fg_cust_data.r_charger_1 = vbus_r;

	if (!of_property_read_u32(np, "batt_id_channel", &g_batt_id_channel))
		pr_info("g_batt_id_channel:%d\n", g_batt_id_channel);

	if (!of_property_read_u32(np, "min_fcc", &fcc))
		g_min_fcc = fcc;
	else
		g_min_fcc = MIN_DESIGN_UAH;
	pr_info("get min_fcc : %d\n", g_min_fcc);

	if (!of_property_read_u32(np, "max_fcc", &fcc)) {
		g_max_fcc = fcc;
		g_fcc_design = fcc;
	} else {
		g_max_fcc = MAX_DESIGN_UAH;
		g_fcc_design = MAX_DESIGN_UAH;
	}
	pr_info("get max_fcc : %d\n", g_max_fcc);
}

bool is_set_aging_fatcor(void)
{
	static int first_in;

	/* every power on will trigger 2 times, skip */
	if (first_in < AGING_FACTOR_SKIP_TIMES) {
		pr_info("fg_set_aging_factor in %d\n", first_in);
		first_in++;
		return false;
	}
	return true;
}

int battery_get_fcc_design(void)
{
	return g_fcc_design;
}

void huawei_set_aging_factor(void)
{
	gm.fcc = ((g_fcc_design / MAH_TO_UAH) * gm.aging_factor) / MAH_TO_UAH;
	pr_info("gm.aging_factor:%d gm.algo_qmax:%d gm.fcc:%d\n",
		gm.aging_factor, gm.algo_qmax, gm.fcc);
	if ((gm.fcc < g_min_fcc) || (gm.fcc > g_max_fcc)) {
		pr_err("not ok use default fcc\n");
		gm.fcc = g_fcc_design;
	}

	cap_learning_event_done_notify();
	dsm_learning_event_report(gm.fcc);
}

static void get_batt_id_para(struct device_node *np, int len)
{
	int i;
	int row;
	int col;
	int ret;
	int idata = 0;
	const char *tmp_string = NULL;

	for (i = 0; i < len; i++) {
		ret = of_property_read_string_index(np, "bat_para",
			i, &tmp_string);
		if (ret) {
			pr_err("bat_para dts read failed\n");
			return;
		}

		row = i / PARA_BAT_TOTAL;
		col = i % PARA_BAT_TOTAL;

		switch (col) {
		case PARA_BAT_ID:
			if (strlen(tmp_string) >= BAT_VENDOR_NAME_LEN - 1)
				strncpy(g_batt_id[row].vendor_name,
					tmp_string, BAT_VENDOR_NAME_LEN - 1);
			else
				strncpy(g_batt_id[row].vendor_name,
					tmp_string, strlen(tmp_string));
			break;
		case PARA_VOL_LOW:
			ret = kstrtoint(tmp_string, DES_BASE, &idata);
			if ((ret != 0) || (idata < 0) || (idata > MAX_ID_VOL))
				return;

			g_batt_id[row].id_vol_low = idata;
			break;
		case PARA_VOL_HIGH:
			ret = kstrtoint(tmp_string, DES_BASE, &idata);
			if ((ret != 0) || (idata < 0) || (idata > MAX_ID_VOL))
				return;

			g_batt_id[row].id_vol_high = idata;
			break;
		default:
			break;
		}
	}
}

void battid_parse_para(struct device_node *np)
{
	int i;
	int array_len;

	if (!np)
		return;

	array_len = of_property_count_strings(np, "bat_para");
	g_batt_len = array_len / PARA_BAT_TOTAL;
	pr_info("bat_para array_len=%d\n", array_len);
	if (g_batt_len > VOLT_GROUP_MAX)
		g_batt_len = VOLT_GROUP_MAX;

	if ((array_len <= 0) ||
		(array_len % PARA_BAT_TOTAL != 0) ||
		(array_len > VOLT_GROUP_MAX * PARA_BAT_TOTAL)) {
		g_batt_id[0].id_vol_low = DEFAULT_ID_VOL_LOW;
		g_batt_id[0].id_vol_high = DEFAULT_ID_VOL_HIGH;
		strncpy(g_batt_id[0].vendor_name, "DeasyCoslight",
			strlen("DeasyCoslight"));
		pr_err("bat_para invalid\n");
		return;
	}

	get_batt_id_para(np, array_len);
	for (i = 0; i < g_batt_len; i++)
		pr_info("bat_para[%d]=%s %d %d\n", i,
			g_batt_id[i].vendor_name,
			g_batt_id[i].id_vol_low,
			g_batt_id[i].id_vol_high);
}

void hw_get_profile_id(void)
{
	int id_volt = 0;
	int i;
	int ret;

	ret = IMM_GetOneChannelValue_Cali(g_batt_id_channel, &id_volt);
	if (ret != 0) {
		pr_err("getchannelvalue fail\n");
		gm.battery_id = 0;
		return;
	}
	pr_info("batt_id_volt = %d\n", id_volt);
	for (i = 0; i < g_batt_len; i++) {
		if ((id_volt > g_batt_id[i].id_vol_low) &&
			(id_volt < g_batt_id[i].id_vol_high)) {
			bm_err("voltag_low:%d  high:%d\n",
				g_batt_id[i].id_vol_low,
				g_batt_id[i].id_vol_high);
			gm.battery_id = i;
			break;
		}
	}

	if (i == g_batt_len)
		gm.battery_id = 0;
	pr_info("Battery id %d\n", gm.battery_id);
}

char *huawei_get_battery_type(void)
{
	int i = gm.battery_id;

	return g_batt_id[i].vendor_name;
}
