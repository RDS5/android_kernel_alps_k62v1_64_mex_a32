/*
 * usb_short_circuit_protect.c
 *
 * usb short circuit protect
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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/version.h>
#include <linux/pm_wakeup.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>
#include <linux/regulator/consumer.h>
#include <linux/power/huawei_power_dsm.h>
#include <dsm/dsm_pub.h>
#include "mtk_auxadc.h"
#include <mt-plat/mtk_battery.h>

#define INVALID_DELTA_TEMP         0
#define USCP_DEFAULT_CHK_CNT       (-1)
#define USCP_START_CHK_CNT         0
#define USCP_END_CHK_CNT           1001
#define USCP_CHK_CNT_STEP          1
#define USCP_INSERT_CHG_CNT        1100
#define FAST_MONITOR_INTERVAL      300  /* 300 ms */
#define NORMAL_MONITOR_INTERVAL    10000  /* 10000 ms */
#define GPIO_HIGH                  1
#define GPIO_LOW                   0
#define INTERVAL_BASE              0
#define ADC_SAMPLE_RETRY_MAX       3
#define USB_TEMP_CNT               2
#define TUSB_TEMP_UPPER_LIMIT      100
#define TUSB_TEMP_LOWER_LIMIT      (-30)
#define CONVERT_TEMP_UNIT          10
#define INVALID_BATT_TEMP          (-255)
#define ADC_VOL_INIT               (-1)
#define AVG_COUNT                  3
#define FG_NOT_READY               0
#define DEFAULT_TUSB_THRESHOLD     40
#define HIZ_ENABLE                 1
#define HIZ_DISABLE                0
#define DMD_HIZ_DISABLE            0
#define DELAY_FOR_GPIO_SET         10
#define ERROR_TUSB_THR             46 /* tusb table for more 125 degree */
#define NORMAL_TEMP                938 /* tusb table for 25 degree */
#define VOLTAGE_MAP_WIDTH          2
#define GET_NUM_MID                2
#define DEFAULT_ADC_CHANNEL        3
#define HIZ_CURRENT                100 /* mA */
#define IIN_CURR_MAX               2000 /* mA */
#define USCP_ERROR                 (-1)

struct uscp_device_info {
	struct device *dev;
	struct device_node *np_node;
	struct workqueue_struct *uscp_wq;
	struct work_struct uscp_check_wk;
	struct power_supply *ac_psy;
	struct power_supply *usb_psy;
	struct power_supply *chg_psy;
	struct power_supply *batt_psy;
	struct power_supply *hwbatt_psy;
	struct power_supply *uscp_psy;
	struct power_supply_desc uscp_desc;
	struct power_supply_config uscp_cfg;
	struct hrtimer timer;
	int usb_present;
	int gpio_uscp;
	int uscp_threshold_tusb;
	int open_mosfet_temp;
	int open_hiz_temp;
	int close_mosfet_temp;
	int interval_switch_temp;
	int check_interval;
	int keep_check_cnt;
	int dmd_hiz_enable;
	struct delayed_work type_work;
};

/* temp/voltage mapping */
static int g_temp_volt_tbl[][VOLTAGE_MAP_WIDTH] = {
	{ -40, 1653356 },
	{ -39, 1643404 },
	{ -38, 1632925 },
	{ -37, 1621906 },
	{ -36, 1610333 },
	{ -35, 1598194 },
	{ -34, 1585478 },
	{ -33, 1572174 },
	{ -32, 1558275 },
	{ -31, 1543774 },
	{ -30, 1528666 },
	{ -29, 1512948 },
	{ -28, 1496620 },
	{ -27, 1479682 },
	{ -26, 1462138 },
	{ -25, 1443994 },
	{ -24, 1425220 },
	{ -23, 1405864 },
	{ -22, 1385941 },
	{ -21, 1365467 },
	{ -20, 1344462 },
	{ -19, 1322947 },
	{ -18, 1300945 },
	{ -17, 1278485 },
	{ -16, 1255594 },
	{ -15, 1232304 },
	{ -14, 1208648 },
	{ -13, 1184660 },
	{ -12, 1160378 },
	{ -11, 1135838 },
	{ -10, 1111081 },
	{ -9, 1086108 },
	{ -8, 1060998 },
	{ -7, 1035794 },
	{ -6, 1010537 },
	{ -5, 985267 },
	{ -4, 960057 },
	{ -3, 934913 },
	{ -2, 909873 },
	{ -1, 884975 },
	{ 0, 860255 },
	{ 1, 835721 },
	{ 2, 811436 },
	{ 3, 787431 },
	{ 4, 763735 },
	{ 5, 740376 },
	{ 6, 717379 },
	{ 7, 694766 },
	{ 8, 672559 },
	{ 9, 650776 },
	{ 10, 629433 },
	{ 11, 608546 },
	{ 12, 588125 },
	{ 13, 568182 },
	{ 14, 548724 },
	{ 15, 529758 },
	{ 16, 511279 },
	{ 17, 493302 },
	{ 18, 475826 },
	{ 19, 458852 },
	{ 20, 442376 },
	{ 21, 426399 },
	{ 22, 410914 },
	{ 23, 395914 },
	{ 24, 381394 },
	{ 25, 367346 },
	{ 26, 353764 },
	{ 27, 340638 },
	{ 28, 327959 },
	{ 29, 315718 },
	{ 30, 303905 },
	{ 31, 292511 },
	{ 32, 281525 },
	{ 33, 270935 },
	{ 34, 260731 },
	{ 35, 250902 },
	{ 36, 241437 },
	{ 37, 232325 },
	{ 38, 223554 },
	{ 39, 215114 },
	{ 40, 206995 },
	{ 41, 199191 },
	{ 42, 191686 },
	{ 43, 184470 },
	{ 44, 177533 },
	{ 45, 170864 },
	{ 46, 164449 },
	{ 47, 158282 },
	{ 48, 152355 },
	{ 49, 146660 },
	{ 50, 141188 },
	{ 51, 135937 },
	{ 52, 130891 },
	{ 53, 126044 },
	{ 54, 121387 },
	{ 55, 116913 },
	{ 56, 112615 },
	{ 57, 108485 },
	{ 58, 104518 },
	{ 59, 100706 },
	{ 60, 97043 },
	{ 61, 93524 },
	{ 62, 90143 },
	{ 63, 86894 },
	{ 64, 83771 },
	{ 65, 80770 },
	{ 66, 77892 },
	{ 67, 75126 },
	{ 68, 72467 },
	{ 69, 69911 },
	{ 70, 67454 },
	{ 71, 65085 },
	{ 72, 62808 },
	{ 73, 60618 },
	{ 74, 58512 },
	{ 75, 56487 },
	{ 76, 54543 },
	{ 77, 52673 },
	{ 78, 50875 },
	{ 79, 49144 },
	{ 80, 47478 },
	{ 81, 45875 },
	{ 82, 44332 },
	{ 83, 42846 },
	{ 84, 41416 },
	{ 85, 40039 },
	{ 86, 38713 },
	{ 87, 37435 },
	{ 88, 36205 },
	{ 89, 35020 },
	{ 90, 33878 },
	{ 91, 32781 },
	{ 92, 31725 },
	{ 93, 30706 },
	{ 94, 29724 },
	{ 95, 28777 },
	{ 96, 27861 },
	{ 97, 26977 },
	{ 98, 26125 },
	{ 99, 25303 },
	{ 100, 24510 },
	{ 101, 23748 },
	{ 102, 23012 },
	{ 103, 22303 },
	{ 104, 21618 },
	{ 105, 20957 },
	{ 106, 20318 },
	{ 107, 19702 },
	{ 108, 19107 },
	{ 109, 18531 },
	{ 110, 17976 },
	{ 111, 17439 },
	{ 112, 16921 },
	{ 113, 16420 },
	{ 114, 15935 },
	{ 115, 15467 },
	{ 116, 15017 },
	{ 117, 14582 },
	{ 118, 14161 },
	{ 119, 13754 },
	{ 120, 13360 },
	{ 121, 12977 },
	{ 122, 12607 },
	{ 123, 12248 },
	{ 124, 11901 },
	{ 125, 11565 },
};

static bool g_uscp_enable;
static bool g_uscp_dmd_enable = true;
static bool g_is_uscp_mode;
static bool g_is_hiz_mode;
static bool g_uscp_dmd_enable_hiz = true;
static int g_uscp_adc_channel = DEFAULT_ADC_CHANNEL;
static struct uscp_device_info *g_uscp_device;
static struct wakeup_source g_uscp_wakelock;
static enum power_supply_property g_uscp_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int get_propety_int(struct power_supply *psy, int propery)
{
	int rc;
	union power_supply_propval ret = {0};

	if (!psy || !(psy->desc) || !(psy->desc->get_property)) {
		pr_err("get input source power supply node failed!\n");
		return -EINVAL;
	}
	rc = psy->desc->get_property(psy, propery, &ret);
	if (rc) {
		pr_err("couldn't get type rc = %d\n", rc);
		ret.intval = -EINVAL;
	}

	return ret.intval;
}

static int set_propety_int(struct power_supply *psy,
	enum power_supply_property prop, const union power_supply_propval *val)
{
	int rc;

	if (!val || !psy || !(psy->desc) || !(psy->desc->set_property)) {
		pr_err("get input source power supply node failed\n");
		return -EINVAL;
	}
	rc = psy->desc->set_property(psy, prop, val);
	if (rc < 0)
		pr_err("couldn't set type rc = %d\n", rc);

	return rc;
}

void charge_set_hiz_enable(const int hiz_mode)
{
	int rc;
	static int save_iin_curr = IIN_CURR_MAX;
	int iin_hiz_curr;
	union power_supply_propval val = {0};

	if (hiz_mode == HIZ_ENABLE) {
		save_iin_curr = get_propety_int(g_uscp_device->hwbatt_psy,
			POWER_SUPPLY_PROP_INPUT_CURRENT_MAX);
		save_iin_curr = max(save_iin_curr, IIN_CURR_MAX);
		iin_hiz_curr = HIZ_CURRENT;
		pr_info("%s hiz mode, curr = %d, iin_curr = %d\n",
			__func__, iin_hiz_curr, save_iin_curr);
	} else {
		iin_hiz_curr = save_iin_curr;
		pr_info("%s non-hiz mode, curr = %d\n", __func__, iin_hiz_curr);
	}

	val.intval = iin_hiz_curr;
	rc = set_propety_int(g_uscp_device->hwbatt_psy,
		POWER_SUPPLY_PROP_INPUT_CURRENT_MAX, &val);
	if (rc < 0)
		pr_err("couldn't set hiz enable\n");
}

static void uscp_wake_lock(void)
{
	if (!(g_uscp_wakelock.active)) {
		pr_info("wake lock\n");
		__pm_stay_awake(&g_uscp_wakelock);
	}
}

static void uscp_wake_unlock(void)
{
	if (g_uscp_wakelock.active) {
		pr_info("wake unlock\n");
		__pm_relax(&g_uscp_wakelock);
	}
}

static void charge_type_handler(struct uscp_device_info *di, const int type)
{
	int interval;

	pr_info("%s uscp enable = %d\n", __func__, g_uscp_enable);
	if (!g_uscp_enable)
		return;

	if (type == 0) {
		pr_info("usb present = %d, do nothing\n", type);
		return;
	}

	if (hrtimer_active(&(di->timer))) {
		pr_info("timer already started, do nothing\n");
		return;
	}

	pr_info("start uscp check\n");
	interval = INTERVAL_BASE;
	/* record 30 seconds after the charger; */
	/* just insert 30s = (1100 - 1001 + 1) * 300ms */
	di->keep_check_cnt = USCP_INSERT_CHG_CNT;
	hrtimer_start(&di->timer, ktime_set((interval / MSEC_PER_SEC),
		((interval % MSEC_PER_SEC) * USEC_PER_SEC)), HRTIMER_MODE_REL);
}

static int get_one_adc_sample(void)
{
	int i;
	int ret;
	int t_sample = ADC_VOL_INIT;

	for (i = 0; i < ADC_SAMPLE_RETRY_MAX; ++i) {
		ret = IMM_GetOneChannelValue_Cali(g_uscp_adc_channel,
			&t_sample);
		if (ret)
			pr_err("adc read fail\n");
		else
			break;
	}
	return t_sample;
}

/* get temp by searching the voltage-temp table */
static int adc_to_temp(const int adc_value)
{
	const int table_size = ARRAY_SIZE(g_temp_volt_tbl);
	int low = 0;
	int mid = 0;
	int high = table_size - 1;

	if (adc_value >= g_temp_volt_tbl[0][1])
		return g_temp_volt_tbl[0][0];
	if (adc_value <= g_temp_volt_tbl[table_size - 1][1])
		return g_temp_volt_tbl[table_size - 1][0];
	/* use binary search */
	while (low < high) {
		pr_debug("low = %d, high = %d!\n", low, high);
		mid = (low + high) / GET_NUM_MID;
		if (mid == 0)
			return g_temp_volt_tbl[1][0];
		if (adc_value > g_temp_volt_tbl[mid][1]) {
			if (adc_value < g_temp_volt_tbl[mid - 1][1])
				return g_temp_volt_tbl[mid][0];
			high = mid - 1;
		} else if (adc_value < g_temp_volt_tbl[mid][1]) {
			if (adc_value >= g_temp_volt_tbl[mid + 1][1])
				return g_temp_volt_tbl[mid + 1][0];
			low = mid + 1;
		} else {
			return g_temp_volt_tbl[mid][0];
		}
	}
	pr_err("transform adb:%d to temp fail, use 0 as default\n", adc_value);
	return 0;
}

static int get_usb_temp_value(void)
{
	int i;
	int cnt = 0;
	int adc_temp;
	int sum = 0;
	int temp = 0;

	for (i = 0; i < AVG_COUNT; ++i) {
		adc_temp = get_one_adc_sample();
		if (adc_temp < 0) {
			pr_err(" get temperature fail\n");
			continue;
		}
		sum += adc_temp;
		++cnt;
	}
	if (cnt > 0)
		temp = adc_to_temp(sum / cnt);
	else
		pr_err("use 0 as default temperature\n");

	pr_info("uscp adc get temp = %d\n", temp);
	return temp;
}

static int get_batt_temp_value(void)
{
	int ret;

	ret = get_propety_int(g_uscp_device->batt_psy, POWER_SUPPLY_PROP_TEMP);
	if (ret == -EINVAL) {
		pr_err("%s get temp error\n", __func__);
		return INVALID_BATT_TEMP;
	}

	return (ret / CONVERT_TEMP_UNIT);
}

static void set_interval(struct uscp_device_info *di, const int temp)
{
	if (!di) {
		pr_err("%s di is NULL\n", __func__);
		return;
	}

	if (temp > di->interval_switch_temp) {
		di->check_interval = FAST_MONITOR_INTERVAL;
		di->keep_check_cnt = USCP_START_CHK_CNT;
		pr_info("after set cnt = %d\n", di->keep_check_cnt);
	} else {
		pr_info("before set cnt = %d\n", di->keep_check_cnt);
		if (di->keep_check_cnt > USCP_END_CHK_CNT) {
			/* check the temperature, */
			/* per 0.3 second for 100 times , */
			/* when the charger just insert. */
			di->keep_check_cnt -= USCP_CHK_CNT_STEP;
			di->check_interval = FAST_MONITOR_INTERVAL;
			g_is_uscp_mode = false;
		} else if (di->keep_check_cnt == USCP_END_CHK_CNT) {
			/* reset the flag when, */
			/* the temperature status is stable */
			di->keep_check_cnt = USCP_DEFAULT_CHK_CNT;
			di->check_interval = NORMAL_MONITOR_INTERVAL;
			g_is_uscp_mode = false;
			uscp_wake_unlock();
		} else if (di->keep_check_cnt >= USCP_START_CHK_CNT) {
			di->keep_check_cnt = di->keep_check_cnt +
				USCP_CHK_CNT_STEP;
			di->check_interval = FAST_MONITOR_INTERVAL;
		} else {
			di->check_interval = NORMAL_MONITOR_INTERVAL;
			g_is_uscp_mode = false;
		}
	}
}

static void protection_process(struct uscp_device_info *di, const int tbatt,
	const int tusb)
{
	int gpio_value;
	int tdiff;

	if (!di) {
		pr_err("%s di is NULL\n", __func__);
		return;
	}

	tdiff = tusb - tbatt;
	gpio_value = gpio_get_value(di->gpio_uscp);
	if ((tbatt != INVALID_BATT_TEMP) && (tusb >= di->uscp_threshold_tusb) &&
		(tdiff >= di->open_hiz_temp)) {
		g_is_hiz_mode = true;
		pr_err("enable charge hiz\n");
		charge_set_hiz_enable(HIZ_ENABLE);
	}
	if ((tbatt != INVALID_BATT_TEMP) && (tusb >= di->uscp_threshold_tusb) &&
		(tdiff >= di->open_mosfet_temp)) {
		uscp_wake_lock();
		g_is_uscp_mode = true;
		gpio_set_value(di->gpio_uscp, GPIO_HIGH);  /* open mosfet */
		pr_info("pull up gpio_uscp\n");
	} else if (tdiff <= di->close_mosfet_temp) {
		if (g_is_uscp_mode) {
			/* close mosfet */
			gpio_set_value(di->gpio_uscp, GPIO_LOW);
			msleep(DELAY_FOR_GPIO_SET);
			charge_set_hiz_enable(HIZ_DISABLE);
			g_is_hiz_mode = false;
			pr_info("pull down gpio_uscp\n");
		}
		if (g_is_hiz_mode) {
			charge_set_hiz_enable(HIZ_DISABLE);
			g_is_hiz_mode = false;
			pr_info("disable charge hiz\n");
		}
	}
}

static void check_temperature(struct uscp_device_info *di)
{
	int tusb;
	int tbatt;
	int tdiff;
	int ret;

	if (!di) {
		pr_err("%s di is NULL\n", __func__);
		return;
	}

	tusb = get_usb_temp_value();
	tbatt = get_batt_temp_value();
	pr_info("tusb = %d, tbatt = %d\n", tusb, tbatt);
	tdiff = tusb - tbatt;
	if (tbatt == INVALID_BATT_TEMP) {
		tdiff = INVALID_DELTA_TEMP;
		pr_err("get battery adc temp err, not care\n");
	}

	if (di->dmd_hiz_enable && (tusb >= di->uscp_threshold_tusb) &&
		(tdiff >= di->open_hiz_temp) && g_uscp_dmd_enable_hiz) {
		ret = power_dsm_dmd_report(POWER_DSM_USCP,
			ERROR_NO_USB_SHORT_PROTECT_HIZ,
			"usb short happened,open hiz\n");
		if (ret)
			pr_err("usb short: hiz dmd fail\n");
		else
			g_uscp_dmd_enable_hiz = false;
	}

	if ((tusb >= di->uscp_threshold_tusb) &&
		(tdiff >= di->open_mosfet_temp) &&
		 g_uscp_dmd_enable) {
		ret = power_dsm_dmd_report(POWER_DSM_USCP,
			ERROR_NO_USB_SHORT_PROTECT,
			"usb short happened");
		if (ret)
			pr_err("usb short: mosfet dmd fail\n");
		else
			g_uscp_dmd_enable = false;
	}

	set_interval(di, tdiff);
	protection_process(di, tbatt, tusb);
}

static void uscp_check_work(struct work_struct *work)
{
	struct uscp_device_info *di = NULL;
	int interval;
	int type;

#ifdef CONFIG_HLTHERM_RUNTEST
	pr_info("HLTHERM disable uscp protect\n");
	return;
#endif

	if (!work) {
		pr_err("%s: work is NULL\n", __func__);
		return;
	}

	di = container_of(work, struct uscp_device_info, uscp_check_wk);
	if (!di) {
		pr_err("%s: get uscp device info is NULL\n", __func__);
		return;
	}

	type = get_propety_int(di->hwbatt_psy, POWER_SUPPLY_PROP_CHARGE_TYPE);
	if ((di->keep_check_cnt == USCP_DEFAULT_CHK_CNT) &&
		(type == POWER_SUPPLY_TYPE_UNKNOWN)) {
		g_uscp_dmd_enable = true;
		g_uscp_dmd_enable_hiz = true;
		gpio_set_value(di->gpio_uscp, GPIO_LOW);  /* close mosfet */
		di->check_interval = NORMAL_MONITOR_INTERVAL;
		g_is_uscp_mode = false;
		di->keep_check_cnt = USCP_INSERT_CHG_CNT;
		pr_info("charger type is %d, stop checking\n", type);
		return;
	}

	check_temperature(di);
	interval = di->check_interval;
	hrtimer_start(&di->timer, ktime_set(interval / MSEC_PER_SEC,
		(interval % MSEC_PER_SEC) * USEC_PER_SEC), HRTIMER_MODE_REL);
}

static enum hrtimer_restart uscp_timer_func(struct hrtimer *timer)
{
	struct uscp_device_info *di = NULL;

	if (!timer) {
		pr_err("%s: timer is NULL\n", __func__);
		return HRTIMER_NORESTART;
	}

	di = container_of(timer, struct uscp_device_info, timer);
	if (!di) {
		pr_err("%s: get uscp device info is NULL\n", __func__);
		return HRTIMER_NORESTART;
	}
	queue_work(di->uscp_wq, &di->uscp_check_wk);
	return HRTIMER_NORESTART;
}

static int uscp_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	/* no property need get for uscp */
	return 0;
}

static int uscp_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	if (!val || !g_uscp_device) {
		pr_err("input para NULL\n");
		return -EINVAL;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("uscp usb_present = %d\n", val->intval);
		charge_type_handler(g_uscp_device, val->intval);
		return 0;
	default:
		return -EINVAL;
	}
}

static int uscp_property_is_writeable(struct power_supply *psy,
	enum power_supply_property prop)
{
	int rc;

	if (!psy) {
		pr_err("%s: psy is NULL\n", __func__);
		return 0;
	}

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static int uscp_init_power_supply(struct uscp_device_info *info)
{
	struct power_supply *ac_psy = NULL;
	struct power_supply *usb_psy = NULL;
	struct power_supply *chg_psy = NULL;
	struct power_supply *batt_psy = NULL;
	struct power_supply *hwbatt_psy = NULL;

	ac_psy = power_supply_get_by_name("ac");
	if (!ac_psy) {
		pr_err("%s ac supply not found\n", __func__);
		return -EPROBE_DEFER;
	}

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		pr_err("%s usb supply not found\n", __func__);
		return -EPROBE_DEFER;
	}

	chg_psy = power_supply_get_by_name("charger");
	if (!chg_psy) {
		pr_err("%s chg supply not found\n", __func__);
		return -EPROBE_DEFER;
	}

	batt_psy = power_supply_get_by_name("battery");
	if (!batt_psy) {
		pr_err("%s batt supply not found\n", __func__);
		return -EPROBE_DEFER;
	}
	hwbatt_psy = power_supply_get_by_name("hwbatt");
	if (!hwbatt_psy) {
		pr_err("%s hwbatt supply not found\n", __func__);
		return -EPROBE_DEFER;
	}

	info->ac_psy = ac_psy;
	info->usb_psy = usb_psy;
	info->chg_psy = chg_psy;
	info->batt_psy = batt_psy;
	info->hwbatt_psy = hwbatt_psy;
	return 0;
}

static int uscp_gpio_init(struct device_node *np, struct uscp_device_info *info)
{
	int ret;

	info->gpio_uscp = of_get_named_gpio(np, "gpio_uscp", 0);
	if (!gpio_is_valid(info->gpio_uscp)) {
		pr_err("gpio_uscp is not valid\n");
		return -EINVAL;
	}
	pr_info("gpio_uscp = %d\n", info->gpio_uscp);

	ret = gpio_request(info->gpio_uscp, "uscp");
	if (ret) {
		pr_err("could not request gpio_uscp\n");
		return -EINVAL;
	}
	gpio_direction_output(info->gpio_uscp, GPIO_LOW);

	return 0;
}

static int uscp_parse_dt(struct device_node *np, struct uscp_device_info *info)
{
	int ret;

	ret = of_property_read_u32(np, "uscp_adc", &g_uscp_adc_channel);
	if (ret)
		g_uscp_adc_channel = DEFAULT_ADC_CHANNEL;
	pr_info("get uscp_adc = %d, ret = %d\n", g_uscp_adc_channel, ret);

	ret = of_property_read_u32(np, "uscp_threshold_tusb",
		&(info->uscp_threshold_tusb));
	if (ret)
		info->uscp_threshold_tusb = DEFAULT_TUSB_THRESHOLD;
	pr_info("get uscp_threshold_tusb = %d, ret = %d\n",
		info->uscp_threshold_tusb, ret);

	ret = of_property_read_u32(np, "open_mosfet_temp",
		&(info->open_mosfet_temp));
	if (ret) {
		pr_err("get open_mosfet_temp info fail\n");
		return ret;
	}
	pr_info("open_mosfet_temp = %d\n", info->open_mosfet_temp);

	ret = of_property_read_u32(np, "close_mosfet_temp",
		&(info->close_mosfet_temp));
	if (ret) {
		pr_err("get close_mosfet_temp info fail\n");
		return ret;
	}
	pr_info("close_mosfet_temp = %d\n", info->close_mosfet_temp);

	ret = of_property_read_u32(np, "interval_switch_temp",
		&(info->interval_switch_temp));
	if (ret) {
		pr_err("get interval_switch_temp info fail\n");
		return ret;
	}
	pr_info("interval_switch_temp = %d\n", info->interval_switch_temp);

	ret = of_property_read_u32(np, "open_hiz_temp", &(info->open_hiz_temp));
	if (ret)
		info->open_hiz_temp = info->open_mosfet_temp;
	pr_info("get open_hiz_temp = %d, ret = %d\n", info->open_hiz_temp, ret);

	ret = of_property_read_u32(np, "dmd_hiz_enable",
		&(info->dmd_hiz_enable));
	if (ret)
		info->dmd_hiz_enable = DMD_HIZ_DISABLE;
	pr_info("get dmd_hiz_enable = %d, ret = %d\n",
		info->dmd_hiz_enable, ret);
	return 0;
}

static int check_ntc_error(void)
{
	int temp;
	int ret;
	int i;
	int sum = 0;

	for (i = 0; i < USB_TEMP_CNT; ++i)
		sum += get_usb_temp_value();

	temp = sum / USB_TEMP_CNT;
	if (temp > TUSB_TEMP_UPPER_LIMIT || temp < TUSB_TEMP_LOWER_LIMIT) {
		ret = power_dsm_dmd_report(POWER_DSM_USCP,
			ERROR_NO_USB_SHORT_PROTECT_NTC, "ntc error happened\n");
		if (ret)
			pr_err("ntc error report dmd fail\n");
		return USCP_ERROR;
	}
	return 0;
}

static int check_batt_present(struct uscp_device_info *info)
{
	int batt_present;

	batt_present = get_propety_int(info->batt_psy,
		POWER_SUPPLY_PROP_PRESENT);
	if (!batt_present)
		return USCP_ERROR;

	return 0;
}

static int check_uscp_enable(struct uscp_device_info *info)
{
	int ret;

	ret = check_ntc_error();
	if (ret) {
		pr_err("check uscp ntc fail");
		return ret;
	}
	ret = check_batt_present(info);
	if (ret) {
		pr_err("check uscp batt present fail");
		return ret;
	}
	g_uscp_enable = true;
	pr_info("check uscp enable ok");
	return 0;
}

static int uscp_power_supply_register(struct uscp_device_info *info)
{
	info->uscp_desc.name = "uscp";
	info->uscp_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->uscp_desc.properties = g_uscp_properties;
	info->uscp_desc.num_properties = ARRAY_SIZE(g_uscp_properties);
	info->uscp_desc.set_property = uscp_set_property;
	info->uscp_desc.get_property = uscp_get_property;
	info->uscp_desc.property_is_writeable = uscp_property_is_writeable;
	info->uscp_cfg.drv_data = info;
	info->uscp_cfg.of_node = info->dev->of_node;

	info->uscp_psy = power_supply_register(info->dev,
		(const struct power_supply_desc *)(&info->uscp_desc),
		(const struct power_supply_config *)(&info->uscp_cfg));
	if (IS_ERR(info->uscp_psy)) {
		pr_err("uscp registe power supply fail: %ld\n",
			PTR_ERR(info->uscp_psy));
		return -EPROBE_DEFER;
	}
	return 0;
}

static void get_usb_present(struct uscp_device_info *info)
{
	int ac_online;
	int usb_online;

	ac_online = get_propety_int(info->ac_psy, POWER_SUPPLY_PROP_ONLINE);
	usb_online = get_propety_int(info->usb_psy, POWER_SUPPLY_PROP_ONLINE);
	info->usb_present = ac_online || usb_online;
	pr_info("usb present = %d\n", info->usb_present);
}

static int uscp_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *np = NULL;
	struct uscp_device_info *di = NULL;

	pr_info("%s enter", __func__);
	if (!pdev) {
		pr_err("%s: pdev is NULL\n", __func__);
		return -EINVAL;
	}

	np = pdev->dev.of_node;
	if (!np) {
		pr_err("%s: np is NULL\n", __func__);
		return -EINVAL;
	}

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di)
		return -ENOMEM;

	di->dev = &pdev->dev;
	dev_set_drvdata(&(pdev->dev), di);
	di->keep_check_cnt = USCP_INSERT_CHG_CNT;
	g_is_uscp_mode = false;
	g_uscp_device = di;

	ret = uscp_init_power_supply(di);
	if (ret)
		goto free_mem;

	ret = uscp_gpio_init(np, di);
	if (ret)
		goto free_mem;

	wakeup_source_init(&g_uscp_wakelock,
		"usb_short_circuit_protect_wakelock");

	ret = uscp_parse_dt(np, di);
	if (ret)
		goto free_gpio;

	ret = check_uscp_enable(di);
	if (ret)
		goto free_gpio;

	di->uscp_wq = create_singlethread_workqueue(
		"usb_short_circuit_protect_wq");
	INIT_WORK(&di->uscp_check_wk, uscp_check_work);
	hrtimer_init(&di->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	di->timer.function = uscp_timer_func;

	ret = uscp_power_supply_register(di);
	if (ret)
		goto free_gpio;

	get_usb_present(di);
	charge_type_handler(di, di->usb_present);

	pr_info("uscp probe ok\n");
	return 0;

free_gpio:
	gpio_free(di->gpio_uscp);
free_mem:
	kfree(di);
	di = NULL;
	g_uscp_device = NULL;
	g_uscp_enable = false;
	return ret;
}

static int uscp_remove(struct platform_device *pdev)
{
	struct uscp_device_info *di = NULL;

	if (!pdev) {
		pr_err("%s: pdev is NULL\n", __func__);
		return -EINVAL;
	}

	di = dev_get_drvdata(&pdev->dev);
	if (!di) {
		pr_err("%s: get uscp device info is NULL\n", __func__);
		return -ENODEV;
	}

	gpio_free(di->gpio_uscp);
	power_supply_unregister(di->uscp_psy);
	kfree(di);
	di = NULL;
	g_uscp_device = NULL;
	return 0;
}

#ifndef CONFIG_HLTHERM_RUNTEST
#ifdef CONFIG_PM
static int usb_short_circuit_protect_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct uscp_device_info *di = NULL;

	if (!pdev) {
		pr_err("%s: pdev is NULL\n", __func__);
		return -EINVAL;
	}

	di = platform_get_drvdata(pdev);
	if (!di) {
		pr_err("%s: get uscp device info is NULL\n", __func__);
		return -ENODEV;
	}

	if (g_uscp_enable == false) {
		pr_info("%s: uscp enable false\n", __func__);
		return 0;
	}

	pr_info("%s:+\n", __func__);
	cancel_work_sync(&di->uscp_check_wk);
	hrtimer_cancel(&di->timer);
	pr_info("%s:-\n", __func__);
	return 0;
}

static int usb_short_circuit_protect_resume(struct platform_device *pdev)
{
	struct uscp_device_info *di = NULL;

	if (!pdev) {
		pr_err("%s: pdev is NULL\n", __func__);
		return -EINVAL;
	}

	di = platform_get_drvdata(pdev);
	if (!di) {
		pr_err("%s: get uscp device info is NULL\n", __func__);
		return -ENODEV;
	}

	if (g_uscp_enable == false) {
		pr_info("%s: uscp enable false\n", __func__);
		return 0;
	}

	get_usb_present(di);
	if (di->usb_present == false) {
		pr_err("%s: di->usb_present = %d\n", __func__, di->usb_present);
		return 0;
	}
	pr_info("%s:+ di->usb_present = %d\n", __func__, di->usb_present);
	queue_work(di->uscp_wq, &di->uscp_check_wk);
	pr_info("%s:-\n", __func__);
	return 0;
}
#endif
#endif

static const struct of_device_id uscp_match_table[] = {
	{
		.compatible = "huawei,usb_short_circuit_protect",
		.data = NULL,
	},
	{
	},
};

static struct platform_driver uscp_driver = {
	.probe = uscp_probe,
#ifndef CONFIG_HLTHERM_RUNTEST
#ifdef CONFIG_PM
	/* depend on IPC driver,so we set SR suspend/resume, */
	/* and IPC is suspend_late/early_resume */
	.suspend = usb_short_circuit_protect_suspend,
	.resume = usb_short_circuit_protect_resume,
#endif
#endif
	.remove = uscp_remove,
	.driver = {
		.name = "huawei,usb_short_circuit_protect",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(uscp_match_table),
	},
};

static int __init uscp_init(void)
{
	return platform_driver_register(&uscp_driver);
}

device_initcall_sync(uscp_init);

static void __exit uscp_exit(void)
{
	platform_driver_unregister(&uscp_driver);
}

module_exit(uscp_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:uscp");
MODULE_AUTHOR("HUAWEI Inc");
