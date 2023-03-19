/*
 * huawei_fuelguage.c
 *
 * huawei fuelguage driver
 *
 * Copyright (c) 2019-2019 Huawei Technologies Co., Ltd.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/power/huawei_fuelguage.h>

struct device *fuelguage_dev;
struct fuelguage_device_info *g_fg_device_para;
extern int get_loginfo_int(struct power_supply *phy, int propery);
extern struct class *hw_power_get_class(void);
extern void get_log_info_from_psy(struct power_supply *psy,
	enum power_supply_property prop, char *buf);

enum coul_sysfs_type {
	FUELGUAGE_SYSFS_GAUGELOG_HEAD = 0,
	FUELGUAGE_SYSFS_GAUGELOG,
};

#define FUELGUAGE_SYSFS_FIELD(fg_name, n, m, store) { \
	.attr = __ATTR(fg_name, m, fuelguage_sysfs_show, store), \
	.name = FUELGUAGE_SYSFS_##n, \
}

#define FUELGUAGE_SYSFS_FIELD_RO(name, n) \
	FUELGUAGE_SYSFS_FIELD(name, n, 0444, NULL)

#define SUCCESS 0

static ssize_t fuelguage_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf);

struct fg_sysfs_field_info {
	struct device_attribute attr;
	char name;
};

static struct fg_sysfs_field_info fg_sysfs_field_tbl[] = {
	FUELGUAGE_SYSFS_FIELD_RO(gaugelog_head, GAUGELOG_HEAD),
	FUELGUAGE_SYSFS_FIELD_RO(gaugelog, GAUGELOG),
};

static struct attribute *fuelguage_sysfs_attrs[ARRAY_SIZE(fg_sysfs_field_tbl) +
	1];

static const struct attribute_group fuelguage_sysfs_attr_group = {
	.attrs = fuelguage_sysfs_attrs,
};

static void fuelguage_sysfs_init_attrs(void)
{
	int i;
	const int limit = ARRAY_SIZE(fg_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		fuelguage_sysfs_attrs[i] =
			&fg_sysfs_field_tbl[i].attr.attr;

	/* Has additional entry for this */
	fuelguage_sysfs_attrs[limit] = NULL;
}

/*
 *  Discription: get the current device_attribute from
 *  charge_sysfs_field_tbl by attr's name
 */
static struct fg_sysfs_field_info *fg_sysfs_field_lookup(const char *name)
{
	int i;
	const int limit = ARRAY_SIZE(fg_sysfs_field_tbl);

	if (!name) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return NULL;
	}

	for (i = 0; i < limit; i++) {
		if (!strcmp(name, fg_sysfs_field_tbl[i].attr.attr.name))
			break;
	}

	if (i >= limit)
		return NULL;

	return &fg_sysfs_field_tbl[i];
}

static void get_loginfo_str(struct power_supply *psy, int propery, char *p_str)
{
	int rc;
	union power_supply_propval ret = { 0, };

	if (!psy) {
		pr_err("get input source power supply node failed!\n");
		return;
	}

	if (p_str == NULL) {
		pr_err("the p_str is NULL\n");
		return;
	}

	rc = psy->desc->get_property(psy, propery, &ret);
	if (rc) {
		pr_err("Couldn't get type rc = %d\n", rc);
		return;
	}
	strncpy(p_str, ret.strval, strlen(ret.strval));
}

/* Discription: show the value for all charge device's node */
static ssize_t fuelguage_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct fg_sysfs_field_info *info = NULL;
	struct fuelguage_device_info *di = NULL;
	int ocv;
	int ret = 0;
	char ctype[NAME_SIZE] = {0};

	if (!dev || !attr || !buf) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return -EINVAL;
	}

	di = dev_get_drvdata(dev);
	if (!di) {
		pr_err("%s: Cannot get fualguage device info, fatal error\n",
			__func__);
		return -EINVAL;
	}

	info = fg_sysfs_field_lookup(attr->attr.name);
	if (!info)
		return -EINVAL;

	switch (info->name) {
	case FUELGUAGE_SYSFS_GAUGELOG_HEAD:
		mutex_lock(&di->sysfs_data.dump_reg_head_lock);
		ret = snprintf(buf, MAX_SIZE,
			"ocv        battery_type                \n");

		mutex_unlock(&di->sysfs_data.dump_reg_head_lock);
		break;
	case FUELGUAGE_SYSFS_GAUGELOG:
		ocv = get_loginfo_int(di->hwbatt_psy,
				POWER_SUPPLY_PROP_VOLTAGE_OCV);
		get_loginfo_str(di->hwbatt_psy, POWER_SUPPLY_PROP_BATTERY_TYPE,
				ctype);
		mutex_lock(&di->sysfs_data.dump_reg_lock);
		ret = snprintf(buf, MAX_SIZE, "%-10d %-27s \n", ocv, ctype);
		mutex_unlock(&di->sysfs_data.dump_reg_lock);
		break;
	default:
		pr_err("(%s)NODE ERR!!HAVE NO THIS NODE:%d\n", __func__,
			info->name);
		break;
	}

	return ret;
}

static int fuelguage_sysfs_create_group(struct fuelguage_device_info *di)
{
	if (!di) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return -EINVAL;
	}

	fuelguage_sysfs_init_attrs();
	return sysfs_create_group(&di->dev->kobj, &fuelguage_sysfs_attr_group);
}

static inline void fuelguage_sysfs_remove_group(
	struct fuelguage_device_info *di)
{
	if (!di)
		return;
	sysfs_remove_group(&di->dev->kobj, &fuelguage_sysfs_attr_group);
}

static struct fuelguage_device_info *fuelguage_device_info_alloc(void)
{
	struct fuelguage_device_info *di;

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		pr_err("alloc di failed\n");
		return NULL;
	}
	di->sysfs_data.reg_value =
		kzalloc(sizeof(char) * CHARGELOG_SIZE, GFP_KERNEL);
	if (!di->sysfs_data.reg_value) {
		pr_err("alloc reg_value failed\n");
		goto alloc_fail_1;
	}
	di->sysfs_data.reg_head =
		kzalloc(sizeof(char) * CHARGELOG_SIZE, GFP_KERNEL);
	if (!di->sysfs_data.reg_head) {
		pr_err("alloc reg_head failed\n");
		goto alloc_fail_2;
	}
	return di;
alloc_fail_2:
	kfree(di->sysfs_data.reg_value);
	di->sysfs_data.reg_value = NULL;
alloc_fail_1:
	kfree(di);
	return NULL;
}

static void fuelguage_device_info_free(struct fuelguage_device_info *di)
{
	if (di != NULL) {
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
}

static void fuelguage_device_create(struct class *power_class,
	struct fuelguage_device_info *di)
{
	int ret;

	if (!power_class)
		return;

	if (fuelguage_dev == NULL) {
		fuelguage_dev = device_create(power_class, NULL, 0,
			"%s", "coul");
		if (IS_ERR(fuelguage_dev))
			fuelguage_dev = NULL;
	}

	if (fuelguage_dev) {
		ret = sysfs_create_link(&fuelguage_dev->kobj,
			&di->dev->kobj, "coul_data");
		if (ret != SUCCESS)
			pr_err("%s failed to create sysfs link!!!\n",
				__func__);
	} else {
		pr_err("%s failed to create new_dev!!!\n", __func__);
	}
}

static int fuel_guage_probe(struct platform_device *pdev)
{
	struct fuelguage_device_info *di = NULL;
	struct power_supply *hwbatt_psy = NULL;
	struct class *power_class = NULL;
	int ret;

	if (!pdev) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return -EPROBE_DEFER;
	}

	hwbatt_psy = power_supply_get_by_name("hwbatt");
	if (!hwbatt_psy) {
		pr_err("hwbatt supply not found deferring probe\n");
		return -EPROBE_DEFER;
	}

	di = fuelguage_device_info_alloc();
	if (!di) {
		pr_err("memory allocation failed\n");
		return -ENOMEM;
	}
	di->dev = &(pdev->dev);
	dev_set_drvdata(&(pdev->dev), di);
	di->hwbatt_psy = hwbatt_psy;
	mutex_init(&di->sysfs_data.dump_reg_lock);
	mutex_init(&di->sysfs_data.dump_reg_head_lock);

	/* create sysfs */
	ret = fuelguage_sysfs_create_group(di);
	if (ret) {
		pr_err("can't create fuel guage sysfs entries\n");
		goto fuel_guage_fail_0;
	}

	power_class = hw_power_get_class();
	fuelguage_device_create(power_class, di);
	if ((power_class == NULL) || (fuelguage_dev == NULL)) {
		pr_err("power_class or fuelguage_dev is NULL\n");
		goto fuel_guage_fail_0;
	}
	g_fg_device_para = di;

	return ret;
fuel_guage_fail_0:
	dev_set_drvdata(&pdev->dev, NULL);
	fuelguage_device_info_free(di);
	di = NULL;

	return ret;
}

static int fuel_guage_remove(struct platform_device *pdev)
{
	struct fuelguage_device_info *di = NULL;

	if (!pdev) {
		pr_err("%s: invalid param, fatal error\n", __func__);
		return -EINVAL;
	}

	di = dev_get_drvdata(&pdev->dev);
	if (!di) {
		pr_err("%s: Cannot get fuelguage device info, fatal error\n",
			__func__);
		return -EINVAL;
	}

	fuelguage_sysfs_remove_group(di);
	fuelguage_device_info_free(di);
	di = NULL;

	return SUCCESS;
}

static const struct of_device_id fuel_guage_match_table[] = {
	{
		.compatible = "huawei,fuelguage",
		.data = NULL,
	},
	{
	},
};

static struct platform_driver fuel_guage_driver = {
	.probe = fuel_guage_probe,
	.remove = fuel_guage_remove,
	.driver = {
		.name = "huawei,fuelguage",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(fuel_guage_match_table),
	},
};

static int __init fuel_guage_init(void)
{
	return platform_driver_register(&fuel_guage_driver);
}

static void __exit fuel_guage_exit(void)
{
	platform_driver_unregister(&fuel_guage_driver);
}

late_initcall(fuel_guage_init);
module_exit(fuel_guage_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("huawei fuel guage module driver");
MODULE_AUTHOR("HUAWEI Inc");
