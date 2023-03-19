/*
 * huawei_charger.h
 *
 * huawei charger driver interface
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

#ifndef HUAWEI_CHARGER_H
#define HUAWEI_CHARGER_H

#include <linux/power_supply.h>

#define TRUE                    1
#define FALSE                   0
#define MAX_SIZE                1024
#define CHARGELOG_SIZE          2048
#define SMB_START_CHARGING      0x40
#define SMB_STOP_CHARGING       0x60

enum charge_sysfs_type {
	CHARGE_SYSFS_IIN_THERMAL = 0,
	CHARGE_SYSFS_IIN_RUNNINGTEST,
	CHARGE_SYSFS_IIN_RT_CURRENT,
	CHARGE_SYSFS_ENABLE_CHARGER,
	CHARGE_SYSFS_FACTORY_DIAG_CHARGER,
	CHARGE_SYSFS_RUNNING_TEST_STATUS,
	CHARGE_SYSFS_CHARGELOG_HEAD,
	CHARGE_SYSFS_CHARGELOG,
	CHARGE_SYSFS_UPDATE_VOLT_NOW,
	CHARGE_SYSFS_IBUS,
	CHARGE_SYSFS_VBUS,
	CHARGE_SYSFS_FCP_SUPPORT,
	CHARGE_SYSFS_HIZ,
	CHARGE_SYSFS_CHARGE_TYPE,
	CHARGE_SYSFS_CHARGE_TERM_VOLT_DESIGN,
	CHARGE_SYSFS_CHARGE_TERM_CURR_DESIGN,
	CHARGE_SYSFS_CHARGE_TERM_VOLT_SETTING,
	CHARGE_SYSFS_CHARGE_TERM_CURR_SETTING,
};

enum huawei_charger_type {
	CHARGER_TYPE_USB = 0,
	CHARGER_TYPE_BC_USB,
	CHARGER_TYPE_NON_STANDARD,
	CHARGER_TYPE_STANDARD,
	CHARGER_TYPE_FCP,
	CHARGER_REMOVED,
	USB_EVENT_OTG_ID,
	CHARGER_TYPE_VR,
	CHARGER_TYPE_TYPEC,
	CHARGER_TYPE_PD,
	CHARGER_TYPE_SCP,
	CHARGER_TYPE_WIRELESS,
};

struct charge_sysfs_data {
	int iin_rt;
	int iin_rt_curr;
	int hiz_mode;
	unsigned int iin_thl;
	char *reg_value;
	char *reg_head;
	struct mutex dump_reg_lock;
	struct mutex dump_reg_head_lock;
	int vterm;
	int iterm;
};

struct charge_device_info {
	struct device *dev;
	struct charge_sysfs_data sysfs_data;
	struct spmi_device *spmi;
	struct power_supply *usb_psy;
	struct power_supply *batt_psy;
	struct power_supply *hwbatt_psy;
	int chrg_config;
	int factory_diag;
	unsigned int input_event;
	unsigned long event;
	int fcp_test_delay;
};

/* variable and function declarationn area */
void strncat_length_protect(char *dst, char *src);
int get_loginfo_int(struct power_supply *psy, int property);
int huawei_handle_charger_event(void);
void cap_learning_event_done_notify(void);
#endif /* HUAWEI_CHARGER_H */
