/*
 * huawei_charger.c
 *
 * huawei charger driver
 *
 * Copyright (C) 2019-2019 Huawei Technologies Co., Ltd.
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

#include <linux/power/huawei_charger.h>
#include <linux/power/huawei_mtk_charger.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/usb/otg.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/spmi.h>
#include <linux/sysfs.h>
#include <linux/kernel.h>

#ifdef CONFIG_HUAWEI_DSM
#include <dsm/dsm_pub.h>
#endif
#define DEFAULT_ITERM            200
#define DEFAULT_VTERM            4400000
#define DEFAULT_FCP_TEST_DELAY   6000
#define DEFAULT_IIN_CURRENT      1000
#define MAX_CURRENT              3000
#define MIN_CURRENT              100
#define CHARGE_DSM_BUF_SIZE      4096
#define DEC_BASE                 10
#define INPUT_EVENT_ID_MIN       0
#define INPUT_EVENT_ID_MAX       3000
#define CHARGE_INFO_BUF_SIZE     30
#define DEFAULT_IIN_THL          2000
#define DEFAULT_IIN_RT           1
#define DEFAULT_HIZ_MODE         0
#define DEFAULT_CHRG_CONFIG      1
#define DEFAULT_FACTORY_DIAG     1
#define UPDATE_VOLT              1
#define EN_HIZ                   1
#define DIS_HIZ                  0
#define EN_CHG                   1
#define DIS_CHG                  0
#define INVALID_LIMIT_CUR        0
#define NAME                     "charger"
#define NO_CHG_TEMP_LOW          0
#define NO_CHG_TEMP_HIGH         50
#define BATT_EXIST_TEMP_LOW     (-40)

enum param_type {
	ONLINE = 0,
	TYPE,
	CHG_EN,
	CHG_STS,
	BAT_STS,
	BAT_ON,
	BAT_TEMP,
	BAT_VOL,
	BAT_CUR,
	PCT,
	USB_IBUS,
	USB_VOL,
	PARAM_MAX
};

#ifdef CONFIG_HUAWEI_DSM
static struct dsm_client *g_chargemonitor_dclient;
static struct dsm_dev g_dsm_charge_monitor = {
	.name = "dsm_charge_monitor",
	.fops = NULL,
	.buff_size = CHARGE_DSM_BUF_SIZE,
};
#endif /* CONFIG_HUAWEI_DSM */

struct class *g_power_class;
struct device *g_charge_dev;
struct charge_device_info *g_charger_device_para;
struct kobject *g_sysfs_poll;
int g_basp_learned_fcc = -1;

static void set_property_on_psy(struct power_supply *psy,
	enum power_supply_property prop, int val)
{
	int rc;
	union power_supply_propval ret = {0};

	if (!psy) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return;
	}

	ret.intval = val;
	rc = psy->desc->set_property(psy, prop, &ret);
	if (rc)
		pr_err("psy does not allow set prop %d rc = %d\n", prop, rc);
}

static int get_property_from_psy(struct power_supply *psy,
	enum power_supply_property prop)
{
	int rc;
	int val;
	union power_supply_propval ret = { 0, };

	if (!psy) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return -EINVAL;
	}

	rc = psy->desc->get_property(psy, prop, &ret);
	if (rc) {
		pr_err("psy doesn't support reading prop %d rc = %d\n",
			prop, rc);
		return rc;
	}
	val = ret.intval;
	return val;
}

void get_log_info_from_psy(struct power_supply *psy,
	enum power_supply_property prop, char *buf)
{
	int rc;
	union power_supply_propval val = { 0, };

	if (!psy || !buf) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return;
	}

	val.strval = buf;
	rc = psy->desc->get_property(psy, prop, &val);
	if (rc)
		pr_err("psy does not allow get prop %d rc = %d\n", prop, rc);
}

static ssize_t get_poll_charge_start_event(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct charge_device_info *chip = g_charger_device_para;

	if (!dev || !attr || !buf) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return -EINVAL;
	}

	if (chip)
		return snprintf(buf, PAGE_SIZE, "%d\n", chip->input_event);
	else
		return 0;
}

static ssize_t set_poll_charge_event(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct charge_device_info *chip = g_charger_device_para;
	long val = 0;

	if (!dev || !attr || !buf) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return -EINVAL;
	}

	if (chip) {
		if ((kstrtol(buf, DEC_BASE, &val) < 0) ||
			(val < INPUT_EVENT_ID_MIN) ||
			(val > INPUT_EVENT_ID_MAX)) {
			return -EINVAL;
		}
		chip->input_event = val;
		sysfs_notify(g_sysfs_poll, NULL, "poll_charge_start_event");
	}
	return count;
}
static DEVICE_ATTR(poll_charge_start_event, 0644,
	get_poll_charge_start_event, set_poll_charge_event);

static ssize_t get_poll_charge_done_event(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (!dev || !attr || !buf) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return -EINVAL;
	}
	return sprintf(buf, "%d\n", g_basp_learned_fcc);
}
static DEVICE_ATTR(poll_charge_done_event, 0644,
	get_poll_charge_done_event, NULL);

static ssize_t get_poll_fcc_learn_event(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (!dev || !attr || !buf) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return -EINVAL;
	}
	return sprintf(buf, "%d\n", g_basp_learned_fcc);
}
static DEVICE_ATTR(poll_fcc_learn_event, 0644,
	get_poll_fcc_learn_event, NULL);

static int charge_event_poll_register(struct device *dev)
{
	int ret;

	if (!dev) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return -EINVAL;
	}

	ret = sysfs_create_file(&dev->kobj,
		&dev_attr_poll_charge_start_event.attr);
	if (ret) {
		pr_err("fail to create poll node for %s\n", dev->kobj.name);
		return ret;
	}
	ret = sysfs_create_file(&dev->kobj,
		&dev_attr_poll_charge_done_event.attr);
	if (ret) {
		pr_err("fail to create poll node for %s\n", dev->kobj.name);
		return ret;
	}
	ret = sysfs_create_file(&dev->kobj,
		&dev_attr_poll_fcc_learn_event.attr);
	if (ret) {
		pr_err("fail to create poll node for %s\n", dev->kobj.name);
		return ret;
	}

	g_sysfs_poll = &dev->kobj;
	return ret;
}

void cap_learning_event_done_notify(void)
{
	struct charge_device_info *chip = g_charger_device_para;

	if (!chip) {
		pr_info("charger device is not init, do nothing!\n");
		return;
	}

	pr_info("fg cap learning event notify!\n");
	if (g_sysfs_poll)
		sysfs_notify(g_sysfs_poll, NULL, "basp_learned_fcc");
}

void cap_charge_done_event_notify(void)
{
	struct charge_device_info *chip = g_charger_device_para;

	if (!chip) {
		pr_info("charger device is not init, do nothing\n");
		return;
	}

	pr_info("charging done event notify\n");
	if (g_sysfs_poll)
		sysfs_notify(g_sysfs_poll, NULL, "poll_basp_done_event");
}

void charge_event_notify(unsigned int event)
{
	struct charge_device_info *chip = g_charger_device_para;

	if (!chip) {
		pr_info("smb device is not init, do nothing!\n");
		return;
	}
	/* avoid notify charge stop event,
	 * continuously without charger inserted
	 */
	if ((chip->input_event != event) || (event == SMB_START_CHARGING)) {
		chip->input_event = event;
		if (g_sysfs_poll)
			sysfs_notify(g_sysfs_poll, NULL,
				"poll_charge_start_event");
	}
}

static int huawei_charger_set_runningtest(struct charge_device_info *di,
	int val)
{
	int iin_rt;

	if (!di || !(di->hwbatt_psy)) {
		pr_err("charge_device is not ready\n");
		return -EINVAL;
	}

	set_property_on_psy(di->hwbatt_psy,
		POWER_SUPPLY_PROP_CHARGING_ENABLED, !!val);
	iin_rt = get_property_from_psy(di->hwbatt_psy,
		POWER_SUPPLY_PROP_CHARGING_ENABLED);
	di->sysfs_data.iin_rt = iin_rt;

	return 0;
}

static int huawei_charger_enable_charge(struct charge_device_info *di, int val)
{
	if (!di || !(di->batt_psy)) {
		pr_err("charge_device is not ready\n");
		return -EINVAL;
	}

	set_property_on_psy(di->hwbatt_psy, POWER_SUPPLY_PROP_CHARGING_ENABLED,
		!!val);
	di->chrg_config = get_property_from_psy(di->hwbatt_psy,
		POWER_SUPPLY_PROP_CHARGING_ENABLED);
	return 0;
}

#ifndef CONFIG_HLTHERM_RUNTEST
static bool is_batt_temp_normal(int temp)
{
	if (((temp > BATT_EXIST_TEMP_LOW) && (temp <= NO_CHG_TEMP_LOW)) ||
		(temp >= NO_CHG_TEMP_HIGH))
		return false;
	return true;
}
#endif

static void huawei_set_enable_charge(struct charge_device_info *di, int val)
{
	int batt_temp;

	if (!di) {
		pr_err("di is null\n");
		return;
	}

	batt_temp = battery_get_bat_temperature();

#ifndef CONFIG_HLTHERM_RUNTEST
	if ((val == EN_CHG) && !is_batt_temp_normal(batt_temp)) {
		pr_err("battery temp is %d, abandon enable_charge\n",
			batt_temp);
		return;
	}
#endif

	huawei_charger_enable_charge(di, val);
	return;
}

static int huawei_charger_set_in_thermal(struct charge_device_info *di,
	int val)
{
	if (!di || !(di->hwbatt_psy)) {
		pr_err("charge_device is not ready\n");
		return -EINVAL;
	}

	set_property_on_psy(di->hwbatt_psy, POWER_SUPPLY_PROP_IN_THERMAL, val);
	di->sysfs_data.iin_thl = get_property_from_psy(di->hwbatt_psy,
		POWER_SUPPLY_PROP_IN_THERMAL);

	return 0;
}

static int huawei_charger_set_rt_current(struct charge_device_info *di, int val)
{
	int iin_rt_curr;

	if (!di || !(di->hwbatt_psy)) {
		pr_err("charge_device is not ready\n");
		return -EINVAL;
	}
	/* 0&1:open limit 100:limit to 100 */
	if ((val == 0) || (val == 1)) {
		iin_rt_curr = get_property_from_psy(di->hwbatt_psy,
			POWER_SUPPLY_PROP_INPUT_CURRENT_MAX);
		pr_info("%s rt_current =%d\n", __func__, iin_rt_curr);
	} else if ((val <= MIN_CURRENT) && (val > 1)) {
		iin_rt_curr = MIN_CURRENT;
	} else {
		iin_rt_curr = val;
	}

	set_property_on_psy(di->hwbatt_psy,
		POWER_SUPPLY_PROP_INPUT_CURRENT_MAX, iin_rt_curr);

	di->sysfs_data.iin_rt_curr = iin_rt_curr;
	return 0;
}

static int huawei_charger_set_hz_mode(struct charge_device_info *di, int val)
{
	int hiz_mode;

	if (!di || !(di->hwbatt_psy)) {
		pr_err("charge_device is not ready\n");
		return -EINVAL;
	}
	hiz_mode = !!val;
	set_property_on_psy(di->hwbatt_psy,
		POWER_SUPPLY_PROP_HIZ_MODE, hiz_mode);
	di->sysfs_data.hiz_mode = hiz_mode;
	return 0;
}

#define charge_sysfs_field(_name, n, m, store) { \
	.attr = __ATTR(_name, m, charge_sysfs_show, store), \
	.name = CHARGE_SYSFS_##n, \
}

#define charge_sysfs_field_rw(_name, n) \
	charge_sysfs_field(_name, n, 0644, charge_sysfs_store)

#define charge_sysfs_field_ro(_name, n) \
	charge_sysfs_field(_name, n, 0444, NULL)

static ssize_t charge_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t charge_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

struct charge_sysfs_field_info {
	char name;
	struct device_attribute attr;
};

static struct charge_sysfs_field_info g_chg_sysfs_field_tbl[] = {
	charge_sysfs_field_rw(iin_thermal, IIN_THERMAL),
	charge_sysfs_field_rw(iin_runningtest, IIN_RUNNINGTEST),
	charge_sysfs_field_rw(iin_rt_current, IIN_RT_CURRENT),
	charge_sysfs_field_rw(enable_hiz, HIZ),
	charge_sysfs_field_rw(enable_charger, ENABLE_CHARGER),
	charge_sysfs_field_rw(factory_diag, FACTORY_DIAG_CHARGER),
	charge_sysfs_field_ro(chargelog_head, CHARGELOG_HEAD),
	charge_sysfs_field_ro(chargelog, CHARGELOG),
	charge_sysfs_field_rw(update_volt_now, UPDATE_VOLT_NOW),
	charge_sysfs_field_ro(ibus, IBUS),
	charge_sysfs_field_ro(vbus, VBUS),
	charge_sysfs_field_ro(chargerType, CHARGE_TYPE),
	charge_sysfs_field_ro(charge_term_volt_design,
		CHARGE_TERM_VOLT_DESIGN),
	charge_sysfs_field_ro(charge_term_curr_design,
		CHARGE_TERM_CURR_DESIGN),
	charge_sysfs_field_ro(charge_term_volt_setting,
		CHARGE_TERM_VOLT_SETTING),
	charge_sysfs_field_ro(charge_term_curr_setting,
		CHARGE_TERM_CURR_SETTING),
};

static struct attribute *g_chg_sysfs_attrs[ARRAY_SIZE(g_chg_sysfs_field_tbl) +
	1];

static const struct attribute_group charge_sysfs_attr_group = {
	.attrs = g_chg_sysfs_attrs,
};

/* initialize g_chg_sysfs_attrs[] for charge attribute */
static void charge_sysfs_init_attrs(void)
{
	int i;
	const int limit = ARRAY_SIZE(g_chg_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		g_chg_sysfs_attrs[i] = &g_chg_sysfs_field_tbl[i].attr.attr;

	g_chg_sysfs_attrs[limit] = NULL; /* Has additional entry for this */
}

static struct charge_sysfs_field_info *charge_sysfs_field_lookup(
	const char *name)
{
	int i;
	const int limit = ARRAY_SIZE(g_chg_sysfs_field_tbl);

	if (!name) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return NULL;
	}

	for (i = 0; i < limit; i++) {
		if (!strcmp(name, g_chg_sysfs_field_tbl[i].attr.attr.name))
			break;
	}

	if (i >= limit)
		return NULL;

	return &g_chg_sysfs_field_tbl[i];
}

int get_loginfo_int(struct power_supply *psy, int propery)
{
	int rc;
	union power_supply_propval ret = { 0, };

	if (!psy) {
		pr_err("get input source power supply node failed!\n");
		return -EINVAL;
	}

	rc = psy->desc->get_property(psy, propery, &ret);
	if (rc)
		ret.intval = -EINVAL;

	return ret.intval;
}
EXPORT_SYMBOL_GPL(get_loginfo_int);

static void conver_usbtype(int val, char *p_str)
{
	if (!p_str) {
		pr_err("the p_str is NULL\n");
		return;
	}

	switch (val) {
	case CHARGER_UNKNOWN:
		strncpy(p_str, "UNKNOWN", sizeof("UNKNOWN"));
		break;
	case STANDARD_HOST:
		strncpy(p_str, "USB", sizeof("USB"));
		break;
	case CHARGING_HOST:
		strncpy(p_str, "CDP", sizeof("CDP"));
		break;
	case NONSTANDARD_CHARGER:
		strncpy(p_str, "NONSTD", sizeof("NONSTD"));
		break;
	case STANDARD_CHARGER:
		strncpy(p_str, "DC", sizeof("DC"));
		break;
	case APPLE_2_1A_CHARGER:
		strncpy(p_str, "APPLE_2_1A", sizeof("APPLE_2_1A"));
		break;
	case APPLE_1_0A_CHARGER:
		strncpy(p_str, "APPLE_1_0A", sizeof("APPLE_1_0A"));
		break;
	case APPLE_0_5A_CHARGER:
		strncpy(p_str, "APPLE_0_5A", sizeof("APPLE_0_5A"));
		break;
	case WIRELESS_CHARGER:
		strncpy(p_str, "WIRELESS", sizeof("WIRELESS"));
		break;
	default:
		strncpy(p_str, "UNSTANDARD", sizeof("UNSTANDARD"));
		break;
	}
}

static void conver_charging_status(int val, char *p_str)
{
	if (!p_str) {
		pr_err("the p_str is NULL\n");
		return;
	}

	switch (val) {
	case POWER_SUPPLY_STATUS_UNKNOWN:
		strncpy(p_str, "UNKNOWN", sizeof("UNKNOWN"));
		break;
	case POWER_SUPPLY_STATUS_CHARGING:
		strncpy(p_str, "CHARGING", sizeof("CHARGING"));
		break;
	case POWER_SUPPLY_STATUS_DISCHARGING:
		strncpy(p_str, "DISCHARGING", sizeof("DISCHARGING"));
		break;
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		strncpy(p_str, "NOTCHARGING", sizeof("NOTCHARGING"));
		break;
	case POWER_SUPPLY_STATUS_FULL:
		strncpy(p_str, "FULL", sizeof("FULL"));
		break;
	default:
		break;
	}
}

static void conver_charger_health(int val, char *p_str)
{
	if (!p_str) {
		pr_err("the p_str is NULL\n");
		return;
	}

	switch (val) {
	case POWER_SUPPLY_HEALTH_OVERHEAT:
		strncpy(p_str, "OVERHEAT", sizeof("OVERHEAT"));
		break;
	case POWER_SUPPLY_HEALTH_COLD:
		strncpy(p_str, "COLD", sizeof("COLD"));
		break;
	case POWER_SUPPLY_HEALTH_WARM:
		strncpy(p_str, "WARM", sizeof("WARM"));
		break;
	case POWER_SUPPLY_HEALTH_COOL:
		strncpy(p_str, "COOL", sizeof("COOL"));
		break;
	case POWER_SUPPLY_HEALTH_GOOD:
		strncpy(p_str, "GOOD", sizeof("GOOD"));
		break;
	default:
		break;
	}
}

static int converse_usb_type(int val)
{
	int type = 0;

	switch (val) {
	case CHARGER_UNKNOWN:
		type = CHARGER_REMOVED;
		break;
	case CHARGING_HOST:
		type = CHARGER_TYPE_BC_USB;
		break;
	case STANDARD_HOST:
		type = CHARGER_TYPE_USB;
		break;
	case STANDARD_CHARGER:
		type = CHARGER_TYPE_STANDARD;
		break;
	default:
		type = CHARGER_REMOVED;
		break;
	}
	return type;
}

static bool g_charger_shutdown_flag;
static int __init early_parse_shutdown_flag(char *p)
{
	if (p) {
		if (!strcmp(p, "charger"))
			g_charger_shutdown_flag = true;
	}
	return 0;
}
early_param("androidboot.mode", early_parse_shutdown_flag);

static void get_charger_shutdown_flag(bool flag, char *p_str)
{
	if (!p_str) {
		pr_err("the p_str is NULL\n");
		return;
	}
	if (flag)
		strncpy(p_str, "OFF", sizeof("OFF"));
	else
		strncpy(p_str, "ON", sizeof("ON"));
}

static int dump_charge_param(struct charge_device_info *di, char *buf)
{
	char c_type[CHARGE_INFO_BUF_SIZE] = {0};
	char c_status[CHARGE_INFO_BUF_SIZE] = {0};
	char c_health[CHARGE_INFO_BUF_SIZE] = {0};
	char c_on[CHARGE_INFO_BUF_SIZE] = {0};
	int prm[PARAM_MAX];
	int len = 0;

	memset(di->sysfs_data.reg_value, 0, CHARGELOG_SIZE);
	prm[ONLINE] = get_loginfo_int(di->hwbatt_psy, POWER_SUPPLY_PROP_ONLINE);
	prm[TYPE] = get_loginfo_int(di->hwbatt_psy,
		POWER_SUPPLY_PROP_CHARGE_TYPE);
	conver_usbtype(prm[TYPE], c_type);
	prm[USB_VOL] = get_loginfo_int(di->hwbatt_psy,
		POWER_SUPPLY_PROP_CHARGE_VOLTAGE_NOW);
	prm[CHG_EN] = get_loginfo_int(di->hwbatt_psy,
		POWER_SUPPLY_PROP_CHARGING_ENABLED);
	prm[CHG_STS] = get_loginfo_int(di->batt_psy, POWER_SUPPLY_PROP_STATUS);
	conver_charging_status(prm[CHG_STS], c_status);
	prm[BAT_STS] = get_loginfo_int(di->batt_psy, POWER_SUPPLY_PROP_HEALTH);
	conver_charger_health(prm[BAT_STS], c_health);
	prm[BAT_ON] = get_loginfo_int(di->batt_psy, POWER_SUPPLY_PROP_PRESENT);
	prm[BAT_TEMP] = get_loginfo_int(di->batt_psy, POWER_SUPPLY_PROP_TEMP);
	prm[BAT_VOL] = get_loginfo_int(di->batt_psy,
		POWER_SUPPLY_PROP_VOLTAGE_NOW);
	prm[BAT_CUR] = get_loginfo_int(di->batt_psy,
		POWER_SUPPLY_PROP_CURRENT_NOW);
	prm[PCT] = get_loginfo_int(di->batt_psy, POWER_SUPPLY_PROP_CAPACITY);
	prm[USB_IBUS] = get_loginfo_int(di->hwbatt_psy,
		POWER_SUPPLY_PROP_CHARGE_CURRENT_NOW);
	get_charger_shutdown_flag(g_charger_shutdown_flag, c_on);
	mutex_lock(&di->sysfs_data.dump_reg_lock);
	get_log_info_from_psy(di->hwbatt_psy, POWER_SUPPLY_PROP_DUMP_REGISTER,
		di->sysfs_data.reg_value);
	len += snprintf(buf, MAX_SIZE,
		" %-8d %-11s %-11d %-11d %-7d %-14s %-9s %-13d %-7d %-9d %-9d ",
		prm[ONLINE], c_type, prm[USB_VOL], di->sysfs_data.iin_thl,
		prm[CHG_EN], c_status, c_health, prm[BAT_ON], prm[BAT_TEMP],
		prm[BAT_VOL], prm[BAT_CUR]);
	len += snprintf(buf + len, MAX_SIZE - len, "%-10d %-10d %-16s %s\n",
		prm[PCT], prm[USB_IBUS], di->sysfs_data.reg_value, c_on);
	mutex_unlock(&di->sysfs_data.dump_reg_lock);
	return len;
}

static int dump_param_head(struct charge_device_info *di, char *buf)
{
	int len = 0;

	mutex_lock(&di->sysfs_data.dump_reg_head_lock);
	get_log_info_from_psy(di->hwbatt_psy,
		POWER_SUPPLY_PROP_REGISTER_HEAD,
		di->sysfs_data.reg_head);

	len += snprintf(buf, MAX_SIZE - len, " online   in_type     usb_vol");
	len += snprintf(buf + len, MAX_SIZE - len, "     iin_thl     ch_en   ");
	len += snprintf(buf + len, MAX_SIZE - len, "status         health    ");
	len += snprintf(buf + len, MAX_SIZE - len, "bat_present   temp    vol");
	len += snprintf(buf + len, MAX_SIZE - len, "       cur       capacity");
	len += snprintf(buf + len, MAX_SIZE - len, "   ibus       %s Mode\n",
		di->sysfs_data.reg_head);
	mutex_unlock(&di->sysfs_data.dump_reg_head_lock);
	return len;
}

static ssize_t charge_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = -EINVAL;
	struct charge_sysfs_field_info *info = NULL;
	struct charge_device_info *di = NULL;
	int type = 0;
	int conver_type = 0;
	int vol;
	int ibus;

	if (!dev || !attr || !buf) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return -EINVAL;
	}

	di = dev_get_drvdata(dev);
	info = charge_sysfs_field_lookup(attr->attr.name);
	if (!di || !info)
		return -EINVAL;

	switch (info->name) {
	case CHARGE_SYSFS_IIN_THERMAL:
		ret = snprintf(buf, MAX_SIZE, "%u\n", di->sysfs_data.iin_thl);
		break;
	case CHARGE_SYSFS_IIN_RUNNINGTEST:
		ret = snprintf(buf, MAX_SIZE, "%u\n", di->sysfs_data.iin_rt);
		break;
	case CHARGE_SYSFS_IIN_RT_CURRENT:
		ret = snprintf(buf, MAX_SIZE, "%u\n",
			di->sysfs_data.iin_rt_curr);
		break;
	case CHARGE_SYSFS_ENABLE_CHARGER:
		ret = snprintf(buf, MAX_SIZE, "%u\n", di->chrg_config);
		break;
	case CHARGE_SYSFS_CHARGELOG_HEAD:
		ret = dump_param_head(di, buf);
		break;
	case CHARGE_SYSFS_CHARGELOG:
		ret = dump_charge_param(di, buf);
		break;
	case CHARGE_SYSFS_IBUS:
		ibus = get_loginfo_int(di->hwbatt_psy,
			POWER_SUPPLY_PROP_CHARGE_CURRENT_NOW);
		ret = snprintf(buf, MAX_SIZE, "%d\n", ibus);
		break;
	case CHARGE_SYSFS_VBUS:
		vol = get_loginfo_int(di->hwbatt_psy,
			POWER_SUPPLY_PROP_CHARGE_VOLTAGE_NOW);
		ret = snprintf(buf, MAX_SIZE, "%d\n", vol);
		break;
	case CHARGE_SYSFS_CHARGE_TYPE:
		type = get_loginfo_int(di->hwbatt_psy,
			POWER_SUPPLY_PROP_CHARGE_TYPE);
		conver_type = converse_usb_type(type);
		ret = snprintf(buf, MAX_SIZE, "%d\n", conver_type);
		break;
	case CHARGE_SYSFS_HIZ:
		ret = snprintf(buf, MAX_SIZE, "%u\n", di->sysfs_data.hiz_mode);
		break;
	case CHARGE_SYSFS_CHARGE_TERM_VOLT_DESIGN:
		ret = snprintf(buf, PAGE_SIZE, "%d\n", di->sysfs_data.vterm);
		break;
	case CHARGE_SYSFS_CHARGE_TERM_CURR_DESIGN:
		ret = snprintf(buf, PAGE_SIZE, "%d\n", di->sysfs_data.iterm);
		break;
	case CHARGE_SYSFS_CHARGE_TERM_VOLT_SETTING:
		vol = get_loginfo_int(di->hwbatt_psy,
			POWER_SUPPLY_PROP_VOLTAGE_MAX);
		ret = snprintf(buf, PAGE_SIZE, "%d\n",
			((vol < di->sysfs_data.vterm) ? vol :
			di->sysfs_data.vterm));
		break;
	case CHARGE_SYSFS_CHARGE_TERM_CURR_SETTING:
		ret = snprintf(buf, PAGE_SIZE, "%d\n", di->sysfs_data.iterm);
		break;
	default:
		break;
	}

	return ret;
}

static ssize_t charge_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	long val = 0;
	struct charge_sysfs_field_info *info = NULL;
	struct charge_device_info *di = NULL;

	if (!dev || !attr || !buf)
		return -EINVAL;

	di = dev_get_drvdata(dev);
	info = charge_sysfs_field_lookup(attr->attr.name);
	if (!info || !di)
		return -EINVAL;

	if (kstrtol(buf, DEC_BASE, &val) < 0)
		return -EINVAL;

	switch (info->name) {
	case CHARGE_SYSFS_IIN_THERMAL:
		if ((val < INVALID_LIMIT_CUR) || (val > MAX_CURRENT))
			return -EINVAL;
		pr_info("set input current = %ld\n", val);
		huawei_charger_set_in_thermal(di, val);
		break;
	case CHARGE_SYSFS_IIN_RUNNINGTEST:
		if ((val < INVALID_LIMIT_CUR) || (val > MAX_CURRENT))
			return -EINVAL;
		pr_info("set running test val = %ld\n", val);
		huawei_charger_set_runningtest(di, val);
		break;
	case CHARGE_SYSFS_IIN_RT_CURRENT:
		if ((val < INVALID_LIMIT_CUR) || (val > MAX_CURRENT))
			return -EINVAL;
		pr_info("set rt test val = %ld\n", val);
		huawei_charger_set_rt_current(di, val);
		break;
	case CHARGE_SYSFS_ENABLE_CHARGER:
		if ((val < DIS_CHG) || (val > EN_CHG))
			return -EINVAL;
		pr_info("set enable charger val = %ld\n", val);
		huawei_set_enable_charge(di, val);
		break;
	case CHARGE_SYSFS_HIZ:
		if ((val < DIS_HIZ) || (val > EN_HIZ))
			return -EINVAL;
		pr_info("set hz mode val = %ld\n", val);
		huawei_charger_set_hz_mode(di, val);
		break;
	default:
		break;
	}
	return count;
}

/* create the charge device sysfs group */
static int charge_sysfs_create_group(struct charge_device_info *di)
{
	if (!di) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return -EINVAL;
	}

	charge_sysfs_init_attrs();
	return sysfs_create_group(&di->dev->kobj, &charge_sysfs_attr_group);
}

/* remove the charge device sysfs group */
static inline void charge_sysfs_remove_group(struct charge_device_info *di)
{
	if (!di)
		return;
	sysfs_remove_group(&di->dev->kobj, &charge_sysfs_attr_group);
}

static DEFINE_MUTEX(mutex_of_hw_power_class);
static struct class *g_hw_power_class;
struct class *hw_power_get_class(void)
{
	if (!g_hw_power_class) {
		mutex_lock(&mutex_of_hw_power_class);
		g_hw_power_class = class_create(THIS_MODULE, "hw_power");
		if (IS_ERR(g_hw_power_class)) {
			pr_err("hw_power_class create fail\n");
			mutex_unlock(&mutex_of_hw_power_class);
			return NULL;
		}
		mutex_unlock(&mutex_of_hw_power_class);
	}
	return g_hw_power_class;
}
EXPORT_SYMBOL_GPL(hw_power_get_class);

static struct charge_device_info *charge_device_info_alloc(void)
{
	struct charge_device_info *di = NULL;

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di)
		return NULL;
	di->sysfs_data.reg_value = kzalloc(sizeof(char) * CHARGELOG_SIZE,
		GFP_KERNEL);
	if (!di->sysfs_data.reg_value)
		goto alloc_reg_value_fail;
	di->sysfs_data.reg_head = kzalloc(sizeof(char) * CHARGELOG_SIZE,
		GFP_KERNEL);
	if (!di->sysfs_data.reg_head)
		goto alloc_reg_head_fail;
	return di;

alloc_reg_head_fail:
	kfree(di->sysfs_data.reg_value);
	di->sysfs_data.reg_value = NULL;
alloc_reg_value_fail:
	kfree(di);
	return NULL;
}

static void charge_device_info_free(struct charge_device_info *di)
{
	if (!di) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return;
	}

	if (di->sysfs_data.reg_head != NULL) {
		kfree(di->sysfs_data.reg_head);
		di->sysfs_data.reg_head = NULL;
	}
	if (di->sysfs_data.reg_value != NULL) {
		kfree(di->sysfs_data.reg_value);
		di->sysfs_data.reg_value = NULL;
	}
	kfree(di);
}

static int hw_psy_init(struct charge_device_info *di)
{
	struct power_supply *usb_psy = NULL;
	struct power_supply *batt_psy = NULL;
	struct power_supply *hwbatt_psy = NULL;

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		pr_err("usb supply not found deferring probe\n");
		return -EPROBE_DEFER;
	}
	batt_psy = power_supply_get_by_name("battery");
	if (!batt_psy) {
		pr_err("batt supply not found deferring probe\n");
		return -EPROBE_DEFER;
	}
	hwbatt_psy = power_supply_get_by_name("hwbatt");
	if (!batt_psy) {
		pr_err("batt supply not found deferring probe\n");
		return -EPROBE_DEFER;
	}

	di->usb_psy = usb_psy;
	di->batt_psy = batt_psy;
	di->hwbatt_psy = hwbatt_psy;
	return 0;
}

static void prase_charge_dts(struct platform_device *pdev)
{
	int ret;
	struct charge_device_info *di = NULL;
	int vterm = 0;
	int iterm = 0;

	di = dev_get_drvdata(&pdev->dev);
	if (!di)
		return;

	di->sysfs_data.iin_thl = DEFAULT_IIN_THL;
	di->sysfs_data.iin_rt = DEFAULT_IIN_RT;
	di->sysfs_data.iin_rt_curr = DEFAULT_IIN_CURRENT;
	di->sysfs_data.hiz_mode = DEFAULT_HIZ_MODE;
	di->chrg_config = DEFAULT_CHRG_CONFIG;
	di->factory_diag = DEFAULT_FACTORY_DIAG;

	ret = of_property_read_u32(pdev->dev.of_node, "huawei,vterm", &vterm);
	if (ret) {
		pr_info("There is no huawei,vterm,use default\n");
		di->sysfs_data.vterm = DEFAULT_VTERM;
	} else {
		di->sysfs_data.vterm = vterm;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "huawei,iterm", &iterm);
	if (ret) {
		pr_info("There is no huawei,iterm,use default\n");
		di->sysfs_data.iterm = DEFAULT_ITERM;
	} else {
		di->sysfs_data.iterm = iterm;
	}
	ret = of_property_read_u32(pdev->dev.of_node,
		"huawei,fcp-test-delay", &di->fcp_test_delay);
	if (ret) {
		pr_info("There is no fcp test delay setting, use default\n");
		di->fcp_test_delay = DEFAULT_FCP_TEST_DELAY;
	}
}

static int creat_power_sysfs(struct charge_device_info *di)
{
	int ret;
	struct class *power_class = NULL;

	power_class = hw_power_get_class();
	if (!power_class)
		return -EINVAL;

	if (!g_charge_dev)
		g_charge_dev = device_create(power_class, NULL, 0, NULL, NAME);

	if (!g_charge_dev || !(di->dev))
		return -EINVAL;

	ret = sysfs_create_link(&g_charge_dev->kobj,
		&di->dev->kobj, "charge_data");
	return ret;
}

static int charge_probe(struct platform_device *pdev)
{
	int ret;
	struct charge_device_info *di = NULL;

	if (!pdev)
		return -EPROBE_DEFER;

	di = charge_device_info_alloc();
	if (!di)
		return -ENOMEM;

	ret = hw_psy_init(di);
	if (ret < 0)
		goto psy_get_fail;

	di->dev = &(pdev->dev);
	dev_set_drvdata(&(pdev->dev), di);
	prase_charge_dts(pdev);
	mutex_init(&di->sysfs_data.dump_reg_lock);
	mutex_init(&di->sysfs_data.dump_reg_head_lock);
	ret = charge_sysfs_create_group(di);
	if (ret) {
		pr_err("can't create charge sysfs entries\n");
		goto charge_fail;
	}

	ret = creat_power_sysfs(di);
	if (ret)
		goto charge_fail;

#ifdef CONFIG_HUAWEI_DSM
	dsm_register_client(&g_dsm_charge_monitor);
#endif
	g_charger_device_para = di;
	huawei_kick_otg_wdt();
	ret = charge_event_poll_register(g_charge_dev);
	if (ret)
		pr_err("poll register fail\n");
	pr_info("huawei charger probe ok!\n");
	return ret;

charge_fail:
	dev_set_drvdata(&pdev->dev, NULL);
psy_get_fail:
	charge_device_info_free(di);
	g_charger_device_para = NULL;
	return ret;
}

static void charge_event_poll_unregister(struct device *dev)
{
	if (!dev) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return;
	}

	sysfs_remove_file(&dev->kobj, &dev_attr_poll_charge_start_event.attr);
	g_sysfs_poll = NULL;
}

static int charge_remove(struct platform_device *pdev)
{
	struct charge_device_info *di = NULL;

	if (!pdev) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return -EINVAL;
	}

	di = dev_get_drvdata(&pdev->dev);
	if (!di) {
		pr_err("%s: Cannot get charge device info,fatal error\n",
			__func__);
		return -EINVAL;
	}

	charge_event_poll_unregister(g_charge_dev);
#ifdef CONFIG_HUAWEI_DSM
	dsm_unregister_client(g_chargemonitor_dclient, &g_dsm_charge_monitor);
#endif
	charge_sysfs_remove_group(di);
	charge_device_info_free(di);
	g_charger_device_para = NULL;
	dev_set_drvdata(&(pdev->dev), NULL);

	return 0;
}

static void charge_shutdown(struct platform_device  *pdev)
{
	if (!pdev)
		pr_err("%s: invalid param\n", __func__);
}

#ifdef CONFIG_PM
static int charge_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (!pdev)
		pr_err("%s: invalid param\n", __func__);
	return 0;
}

static int charge_resume(struct platform_device *pdev)
{
	if (!pdev)
		pr_err("%s: invalid param\n", __func__);
	return 0;
}
#endif

static const struct of_device_id charge_match_table[] = {
	{
		.compatible = "huawei,charger",
		.data = NULL,
	},
	{
	},
};

static struct platform_driver g_charge_driver = {
	.probe = charge_probe,
	.remove = charge_remove,
#ifdef CONFIG_PM
	.suspend = charge_suspend,
	.resume = charge_resume,
#endif
	.shutdown = charge_shutdown,
	.driver = {
		.name = "huawei,charger",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(charge_match_table),
	},
};

static int __init charge_init(void)
{
	int ret;

	ret = platform_driver_register(&g_charge_driver);
	if (ret) {
		pr_info("register platform_driver_register failed!\n");
		return ret;
	}
	pr_info("%s: %d\n", __func__, __LINE__);
	return 0;
}

static void __exit charge_exit(void)
{
	platform_driver_unregister(&g_charge_driver);
}

late_initcall(charge_init);
module_exit(charge_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("huawei charger module driver");
MODULE_AUTHOR("HUAWEI Inc");
