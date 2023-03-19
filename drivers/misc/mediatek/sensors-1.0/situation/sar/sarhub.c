/* sarhub motion sensor driver
 *
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#define pr_fmt(fmt) "[sarhub] " fmt

#include <hwmsensor.h>
#include "sarhub.h"
#include <situation.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"
#include "sar_factory.h"
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include "sensor_para.h"

static struct situation_init_info sarhub_init_info;

static DEFINE_SPINLOCK(calibration_lock);

struct sarhub_ipi_data {
	bool factory_enable;

	int32_t cali_data[3];
	int8_t cali_status;
	struct completion calibration_done;
};
static struct sarhub_ipi_data *obj_ipi_data;

extern struct sensor_para_t *sensor_para;

#if defined(CONFIG_MTK_ABOV_SAR)
int hw_get_sensor_dts_config(void)
{
	int dts_config_len;
	const char *sar_dts_config = NULL;
	struct device_node *dp = NULL;

	dp = of_find_node_by_path("/huawei_scp_info");
	if (dp == NULL) {
		pr_err("device is not available!\n");
		return -EINVAL;
	}
	pr_debug("%s:dp->name:%s,dp->full_name:%s;\n",
		__func__, dp->name, dp->full_name);

	sar_dts_config = of_get_property(dp, "is_ap_sar", &dts_config_len);
	if (sar_dts_config == NULL) {
		pr_err("%s: sar_dts_config not exist\n", __func__);
		return -EINVAL;
	}

	if (strncmp(sar_dts_config, "true", dts_config_len) == 0) {
		pr_err("%s: use the ap sar driver\n", __func__);
		return 0;
	}
	pr_err("%s: use the scp sar driver\n", __func__);
	return -EINVAL;
}
#else
void hw_sensorhub_get_chip_type(void)
{
	struct device_node* scpinfo_node = NULL;
	unsigned int product_name = 0;
	int ret = 0;

	pr_err("Enter function :hw_sensorhub_get_chip_type\n");
	if (NULL == sensor_para)
	{
		pr_err("sensor_para is NULL!\n");
		return ;
	}

	scpinfo_node = of_find_compatible_node(NULL, NULL, "huawei,huawei_scp_info");

	if (NULL == scpinfo_node)
	{
		pr_err("Cannot find huawei_scp_info from dts\n");
		return;
	}
	else
	{
		ret = of_property_read_u32(scpinfo_node, "product_number", &product_name);

		if (0 == ret)
		{
			sensor_para->sar_para.product_name = product_name;
			pr_info("find product_name success %d\n", product_name);
		}
		else
		{
			pr_err("Cannot find product_name from dts\n");
			sensor_para->sar_para.product_name = 0;
			return;
		}
	}

	pr_err("Exit function :hw_sensorhub_get_chip_type, product_name: %d\n", sensor_para->sar_para.product_name);
	return;
 }
#endif

static int sar_factory_enable_sensor(bool enabledisable,
					 int64_t sample_periods_ms)
{
	int err = 0;
	struct sarhub_ipi_data *obj = obj_ipi_data;

	if (enabledisable == true)
		WRITE_ONCE(obj->factory_enable, true);
	else
		WRITE_ONCE(obj->factory_enable, false);
	if (enabledisable == true) {
		err = sensor_set_delay_to_hub(ID_SAR,
					      sample_periods_ms);
		if (err) {
			pr_err("sensor_set_delay_to_hub failed!\n");
			return -1;
		}
	}
	err = sensor_enable_to_hub(ID_SAR, enabledisable);
	if (err) {
		pr_err("sensor_enable_to_hub failed!\n");
		return -1;
	}
	return 0;
}

static int sar_factory_get_data(int32_t sensor_data[3])
{
	int err = 0;
	struct data_unit_t data;

	err = sensor_get_data_from_hub(ID_SAR, &data);
	if (err < 0) {
		pr_err_ratelimited("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	sensor_data[0] = data.sar_event.data[0];
	sensor_data[1] = data.sar_event.data[1];
	sensor_data[2] = data.sar_event.data[2];

	return err;
}

static int sar_factory_enable_calibration(void)
{
	return sensor_calibration_to_hub(ID_SAR);
}

static int sar_factory_get_cali(int32_t data[3])
{
	int err = 0;
	struct sarhub_ipi_data *obj = obj_ipi_data;
	int8_t status = 0;

	err = wait_for_completion_timeout(&obj->calibration_done,
					  msecs_to_jiffies(3000));
	if (!err) {
		pr_err("sar factory get cali fail!\n");
		return -1;
	}
	spin_lock(&calibration_lock);
	data[0] = obj->cali_data[0];
	data[1] = obj->cali_data[1];
	data[2] = obj->cali_data[2];
	status = obj->cali_status;
	spin_unlock(&calibration_lock);
	if (status != 0) {
		pr_debug("sar cali fail!\n");
		return -2;
	}
	return 0;
}


static struct sar_factory_fops sarhub_factory_fops = {
	.enable_sensor = sar_factory_enable_sensor,
	.get_data = sar_factory_get_data,
	.enable_calibration = sar_factory_enable_calibration,
	.get_cali = sar_factory_get_cali,
};

static struct sar_factory_public sarhub_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &sarhub_factory_fops,
};

static int sar_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_SAR, &data);
	if (err < 0) {
		pr_err_ratelimited("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	*probability	= data.sar_event.data[0];
	pr_info("sar_get_data = %d\n", data.sar_event.data[0]);
	return 0;
}
static int sar_open_report_data(int open)
{
	int ret = 0;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_SAR, 120);
#elif defined CONFIG_NANOHUB

#else

#endif
	ret = sensor_enable_to_hub(ID_SAR, open);
	return ret;
}
static int sar_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_SAR,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int sar_flush(void)
{
	return sensor_flush_to_hub(ID_SAR);
}

static int sar_recv_data(struct data_unit_t *event, void *reserved)
{
	struct sarhub_ipi_data *obj = obj_ipi_data;
	int32_t value[3] = {0};
	int err = 0;

	if (event->flush_action == FLUSH_ACTION)
		err = situation_flush_report(ID_SAR);
	else if (event->flush_action == DATA_ACTION) {
		value[0] = event->sar_event.data[0];
		value[1] = event->sar_event.data[1];
		value[2] = event->sar_event.data[2];
		err = sar_data_report_t(value, (int64_t)event->time_stamp);
	} else if (event->flush_action == CALI_ACTION) {
		spin_lock(&calibration_lock);
		obj->cali_data[0] =
			event->sar_event.x_bias;
		obj->cali_data[1] =
			event->sar_event.y_bias;
		obj->cali_data[2] =
			event->sar_event.z_bias;
		obj->cali_status =
			(int8_t)event->sar_event.status;
		spin_unlock(&calibration_lock);
		complete(&obj->calibration_done);
	}
	return err;
}


static int sarhub_local_init(void)
{
	struct situation_control_path ctl = {0};
	struct situation_data_path data = {0};
	int err = 0;

	struct sarhub_ipi_data *obj;

	pr_debug("%s\n", __func__);
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(*obj));
	obj_ipi_data = obj;
	WRITE_ONCE(obj->factory_enable, false);
	init_completion(&obj->calibration_done);

	ctl.open_report_data = sar_open_report_data;
	ctl.batch = sar_batch;
	ctl.flush = sar_flush;
	ctl.is_support_wake_lock = true;
	ctl.is_support_batch = false;
	err = situation_register_control_path(&ctl, ID_SAR);
	if (err) {
		pr_err("register sar control path err\n");
		goto exit;
	}

	data.get_data = sar_get_data;
	err = situation_register_data_path(&data, ID_SAR);
	if (err) {
		pr_err("register sar data path err\n");
		goto exit;
	}

	err = sar_factory_device_register(&sarhub_factory_device);
	if (err) {
		pr_err("sar_factory_device register failed\n");
		goto exit;
	}

	err = scp_sensorHub_data_registration(ID_SAR,
		sar_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit;
	}
	return 0;
exit:
	return -1;
}
static int sarhub_local_uninit(void)
{
	return 0;
}

static struct situation_init_info sarhub_init_info = {
	.name = "sar_hub",
	.init = sarhub_local_init,
	.uninit = sarhub_local_uninit,
};

static int __init sarhub_init(void)
{
#if defined(CONFIG_MTK_ABOV_SAR)
	if (hw_get_sensor_dts_config() != 0) {
		pr_err("sarhub_init\n");
		situation_driver_add(&sarhub_init_info, ID_SAR);
	}
#else
	pr_err("sarhub_init!!\n");
	hw_sensorhub_get_chip_type();
	situation_driver_add(&sarhub_init_info, ID_SAR);
#endif
	return 0;
}

static void __exit sarhub_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(sarhub_init);
module_exit(sarhub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SAR_HUB driver");
MODULE_AUTHOR("Jashon.zhang@mediatek.com");
