/*
 * huawei_power_dsm.c
 *
 * huawei power dsm driver
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

#include <linux/power/huawei_power_dsm.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include "lcm_pmic.h"

#if (defined(POWER_DSM_DEBUG_TEST) && defined(CONFIG_SYSFS))
static struct kobject *power_dsm_kobj;
#endif

#define POWER_DSM_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define SUCCESS  0
#define ERROR    (-1)

/* define dsm device stract data */
static struct dsm_dev power_dsm_dev_usb = {
	.name = "dsm_usb",
	.device_name = NULL,
	.ic_name = NULL,
	.module_name = NULL,
	.fops = NULL,
	.buff_size = POWER_DSM_BUF_SIZE_0256,
};

static struct dsm_dev power_dsm_dev_battery_detect = {
	.name = "dsm_battery_detect",
	.device_name = NULL,
	.ic_name = NULL,
	.module_name = NULL,
	.fops = NULL,
	.buff_size = POWER_DSM_BUF_SIZE_1024,
};

static struct dsm_dev power_dsm_dev_battery = {
	.name = "dsm_battery",
	.device_name = NULL,
	.ic_name = NULL,
	.module_name = NULL,
	.fops = NULL,
	.buff_size = POWER_DSM_BUF_SIZE_2048,
};

static struct dsm_dev power_dsm_dev_charge_monitor = {
	.name = "dsm_charge_monitor",
	.device_name = NULL,
	.ic_name = NULL,
	.module_name = NULL,
	.fops = NULL,
	.buff_size = POWER_DSM_BUF_SIZE_1024,
};

static struct dsm_dev power_dsm_dev_smpl = {
	.name = "dsm_smpl",
	.device_name = NULL,
	.ic_name = NULL,
	.module_name = NULL,
	.fops = NULL,
	.buff_size = POWER_DSM_BUF_SIZE_1024,
};

static struct dsm_dev power_dsm_dev_uscp = {
	.name = "dsm_usb_short_circuit_protect",
	.device_name = NULL,
	.ic_name = NULL,
	.module_name = NULL,
	.fops = NULL,
	.buff_size = POWER_DSM_BUF_SIZE_1024,
};

static struct dsm_dev power_dsm_dev_pmu_ocp = {
	.name = "dsm_pmu_ocp",
	.device_name = NULL,
	.ic_name = NULL,
	.module_name = NULL,
	.fops = NULL,
	.buff_size = POWER_DSM_BUF_SIZE_1024,
};

static struct dsm_dev power_dsm_dev_pmic_irq = {
	.name = "dsm_pmu_irq",
	.device_name = NULL,
	.ic_name = NULL,
	.module_name = NULL,
	.fops = NULL,
	.buff_size = POWER_DSM_BUF_SIZE_1024,
};

static struct dsm_dev vibrator_dsm_pmic_irq = {
	.name = "dsm_vibrator",
	.device_name = NULL,
	.ic_name = NULL,
	.module_name = NULL,
	.fops = NULL,
	.buff_size = POWER_DSM_BUF_SIZE_1024,
};

/* define power dsm struct data */
struct power_dsm_data_info power_dsm_data[] = {
	{ POWER_DSM_USB, "dsm_usb", NULL, &power_dsm_dev_usb },
	{ POWER_DSM_BATTERY_DETECT, "dsm_battery_detect", NULL,
		&power_dsm_dev_battery_detect },
	{ POWER_DSM_BATTERY, "dsm_battery", NULL, &power_dsm_dev_battery },
	{ POWER_DSM_CHARGE_MONITOR, "dsm_charge_monitor", NULL,
		&power_dsm_dev_charge_monitor },
	{ POWER_DSM_SMPL, "dsm_smpl", NULL, &power_dsm_dev_smpl },
	{ POWER_DSM_USCP, "dsm_usb_short_circuit_protect", NULL,
		&power_dsm_dev_uscp },
	{ POWER_DSM_PMU_OCP, "dsm_pmu_ocp", NULL, &power_dsm_dev_pmu_ocp },
	{ POWER_DSM_PMU_IRQ, "dsm_pmu_irq", NULL, &power_dsm_dev_pmic_irq },
	{ VIBR_DSM_PMU_IRQ, "dsm_vibrator", NULL, &vibrator_dsm_pmic_irq },
};

struct dsm_client *power_dsm_get_dclient(power_dsm_type type)
{
	int i;
	int size = POWER_DSM_ARRAY_SIZE(power_dsm_data);
	struct dsm_client *client = NULL;

	if ((type < POWER_DSM_TYPE_BEGIN) || (type >= POWER_DSM_TYPE_END)) {
		pr_err("error: invalid type %d\n", type);
		return NULL;
	}

	pr_info("type is [%d]\n", type);

	for (i = 0; i < size; i++) {
		if (type == power_dsm_data[i].type) {
			client = power_dsm_data[i].client;
			break;
		}
	}

	if (client == NULL) {
		pr_err("error: client is null!\n");
		return NULL;
	}

	return client;
}
EXPORT_SYMBOL_GPL(power_dsm_get_dclient);

int power_dsm_dmd_report(power_dsm_type type, int err_no, void *buf)
{
	struct dsm_client *client = NULL;

	client = power_dsm_get_dclient(type);

	if ((buf == NULL) || (client == NULL)) {
		pr_err("error: msg is null or client is null!\n");
		return ERROR;
	}
	if (!dsm_client_ocuppy(client)) {
		dsm_client_record(client, "%s", buf);
		dsm_client_notify(client, err_no);
		pr_info("report type:%d, err_no:%d\n", type, err_no);
		return SUCCESS;
	}

	pr_err("error: power dsm client is busy!\n");
	return ERROR;
}
EXPORT_SYMBOL_GPL(power_dsm_dmd_report);

#ifdef CONFIG_HUAWEI_DATA_ACQUISITION
int power_dsm_bigdata_report(power_dsm_type type, int err_no, void *msg)
{
	struct dsm_client *client = NULL;

	client = power_dsm_get_dclient(type);

	if ((msg == NULL) || (client == NULL)) {
		pr_err("error: msg is null or client is null!\n");
		return ERROR;
	}

	if (!dsm_client_ocuppy(client)) {
		dsm_client_copy_ext(client, (struct message *)msg,
			sizeof(struct message));
		dsm_client_notify(client, err_no);
		pr_info("report type:%d, err_no:%d\n", type, err_no);
		return SUCCESS;
	}

	pr_err("error: power dsm client is busy!\n");
	return ERROR;
}
EXPORT_SYMBOL_GPL(power_dsm_bigdata_report);
#endif

#if (defined(POWER_DSM_DEBUG_TEST) && defined(CONFIG_SYSFS))
static power_dsm_type g_power_dsm_type = POWER_DSM_TYPE_END;
static int g_power_dsm_errno;

static ssize_t power_dsm_set_errno(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int dsm_type = 0;
	unsigned int dsm_errno = 0;
	int ret;

	if (count >= POWER_DSM_BUF_SIZE_0016) {
		pr_err("error: input too long!\n");
		return -EINVAL;
	}

	if (sscanf(buf, "%d %d", &dsm_type, &dsm_errno) != 2) {
		pr_err("error: unable to parse input:%s!\n", buf);
		return -EINVAL;
	}

	if ((dsm_type < POWER_DSM_TYPE_BEGIN) ||
		(dsm_type >= POWER_DSM_TYPE_END)) {
		pr_err("error: invalid type %d\n", dsm_type);
		return -EINVAL;
	}

	g_power_dsm_type = dsm_type;
	g_power_dsm_errno = dsm_errno;

	ret = power_dsm_dmd_report(dsm_type, dsm_errno, buf);
	if (ret != SUCCESS)
		pr_err("power dsm dmd report error %d\n", ret);

	return count;
}

static ssize_t power_dsm_show_errno_info(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "type:%d, err_no:%d",
		g_power_dsm_type, g_power_dsm_errno);
}

static DEVICE_ATTR(set_errno, 0644,
	power_dsm_show_errno_info, power_dsm_set_errno);

static struct attribute *power_dsm_attributes[] = {
	&dev_attr_set_errno.attr,
	NULL,
};

static const struct attribute_group power_dsm_attr_group = {
	.attrs = power_dsm_attributes,
};
#endif

static int __init power_dsm_init(void)
{
#if (defined(POWER_DSM_DEBUG_TEST) && defined(CONFIG_SYSFS))
	int ret = SUCCESS;
#endif
	int i;
	int size = POWER_DSM_ARRAY_SIZE(power_dsm_data);

	pr_info("[%s] probe begin\n", __func__);

	for (i = 0; i < size; i++) {
		if (!power_dsm_data[i].client)
			power_dsm_data[i].client =
				dsm_register_client(power_dsm_data[i].dev);

		if (power_dsm_data[i].client == NULL)
			pr_err("error: dsm %s register fail\n",
				power_dsm_data[i].name);
		else
			pr_info("dsm %s register ok\n", power_dsm_data[i].name);
	}

#if (defined(POWER_DSM_DEBUG_TEST) && defined(CONFIG_SYSFS))
	power_dsm_kobj = kobject_create_and_add("power_dsm", NULL);
	if (power_dsm_kobj == NULL) {
		pr_err("error: kobject create failed\n");
		ret = -ENOMEM;
		goto exit;
	}

	ret = sysfs_create_group(power_dsm_kobj, &power_dsm_attr_group);
	if (ret < SUCCESS) {
		pr_err("error: sysfs group create failed\n");
		ret = -ENOMEM;
		goto exit_kobject_del;
	}
#endif

	pr_info("probe end\n");
	return SUCCESS;

#if (defined(POWER_DSM_DEBUG_TEST) && defined(CONFIG_SYSFS))
exit_kobject_del:
	kobject_del(power_dsm_kobj);
exit:
	return ret;
#endif
}

static void __exit power_dsm_exit(void)
{
	int i;
	int size = POWER_DSM_ARRAY_SIZE(power_dsm_data);

	pr_info("remove begin\n");

	for (i = 0; i < size; i++) {
		power_dsm_data[i].client = NULL;
		pr_info("dsm %s un-register ok\n", power_dsm_data[i].name);
	}

#if (defined(POWER_DSM_DEBUG_TEST) && defined(CONFIG_SYSFS))
	sysfs_remove_group(power_dsm_kobj, &power_dsm_attr_group);
	kobject_del(power_dsm_kobj);
#endif

	pr_info("remove end\n");
}

subsys_initcall_sync(power_dsm_init);
module_exit(power_dsm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("power dsm module driver");
MODULE_AUTHOR("HUAWEI Inc");
