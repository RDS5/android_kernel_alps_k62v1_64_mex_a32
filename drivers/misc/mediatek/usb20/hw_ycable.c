/*
 * hw_ycable.c
 *
 * y cable driver
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

#include "hw_ycable.h"
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <mt-plat/charger_type.h>
#include "../auxadc/mtk_auxadc.h"
#include <mt-plat/charger_class.h>

struct hrtimer g_thub_kthread_timer;
int g_vbus_status = false;
static int g_ycable_type = Y_CABLE_RESET;
static bool g_thub_thread_timeout;
static DEFINE_MUTEX(thub_polling_mutex);
static DECLARE_WAIT_QUEUE_HEAD(thub_polling_thread_wq);

enum charger_type ycable_get_charger_type(void)
{
	int y_cable_type;
	enum charger_type y_cable_charger;

	y_cable_type = ycable_get_type_detection();
	if (y_cable_type == Y_CABLE_CHARGER) {
		y_cable_charger = NONSTANDARD_CHARGER;
		chrdet_inform_psy_changed(y_cable_charger, 1);
	} else {
		y_cable_charger = CHARGER_UNKNOWN;
		chrdet_inform_psy_changed(y_cable_charger, 0);
	}
	return y_cable_charger;
}

void ycable_set_vbus_status(void)
{
	int y_cable_type;

	ycable_reset_type();
	y_cable_type = ycable_get_type();
	pr_info("[%s][%d] y_cable_type =%d\n",
		__func__, __LINE__, y_cable_type);

	if (y_cable_type == Y_CABLE_CHARGER)
		g_vbus_status = true;
}

int ycable_get_type_detection(void)
{
	int adc_voltage;
	int ret_value;

	msleep(20);
	if (IMM_IsAdcInitReady() == 0) {
		pr_err("[%s]: AUXADC is not ready\n", __func__);
		return Y_CABLE_DISCONNECT;
	}

	ret_value = IMM_GetOneChannelValue_Cali(USB_ID_ADC_CHANNEL,
				&adc_voltage);
	if (ret_value != 0) {
		pr_info("%s read usb id adc error!\n", __func__);
		adc_voltage = 0;
	}

	adc_voltage = adc_voltage / UV_TO_MV;

	pr_info("[%s]: y cable usb id voltage %d",
		__func__, adc_voltage);

	if (adc_voltage > CHARGER_VOLTAGE_HIGH_THRESHOLD)
		return Y_CABLE_DISCONNECT;
	else if (adc_voltage < CHARGER_VOLTAGE_HIGH_THRESHOLD &&
		adc_voltage > CHARGER_VOLTAGE_LOW_THRESHOLD)
		return Y_CABLE_CHARGER;
	else if (adc_voltage < OTG_VOLTAGE_HIGH_THRESHOLD &&
		adc_voltage >= OTG_VOLTAGE_LOW_THRESHOLD)
		return Y_CABLE_OTG;
	else
		return Y_CABLE_UNKNOWN;
}

void ycable_reset_type(void)
{
	g_ycable_type = Y_CABLE_RESET;
}

int ycable_get_type(void)
{
	if (g_ycable_type == Y_CABLE_RESET)
		g_ycable_type = ycable_get_type_detection();

	return g_ycable_type;
}

enum hrtimer_restart thub_kthread_hrtimer_func(struct hrtimer *timer)
{
	g_thub_thread_timeout = true;
	wake_up(&thub_polling_thread_wq);

	return HRTIMER_NORESTART;
}

int thub_polling_thread(void *arg)
{
	int y_cable_type = Y_CABLE_OTG;
	int y_cable_type_last = Y_CABLE_OTG;

	while (1) {
		wait_event(thub_polling_thread_wq,
			(g_thub_thread_timeout == true));
		g_thub_thread_timeout = false;

		mutex_lock(&thub_polling_mutex);
		ycable_reset_type();
		y_cable_type = ycable_get_type();
		pr_info("[%s][%d] y_cable_type now:%d last: %d\n",
			__func__, __LINE__, y_cable_type, y_cable_type_last);
		if ((y_cable_type == Y_CABLE_CHARGER) &&
			(y_cable_type_last != y_cable_type)) {
			msleep(60);
			ycable_reset_type();
			y_cable_type = ycable_get_type();
			if (y_cable_type == Y_CABLE_CHARGER) {
				pr_info("Adaptor plug in\n");
				pr_info("disable charger otg\n");
				charger_dev_enable_otg(primary_charger, false);
				mtk_pmic_enable_chr_type_det(true);
			}
		} else if ((y_cable_type == Y_CABLE_OTG) &&
				(y_cable_type_last == Y_CABLE_CHARGER) &&
				(y_cable_type_last != y_cable_type)) {
			pr_info("Adaptor plug out\n");
			pr_info("call charger detect\n");
			mtk_pmic_enable_chr_type_det(false);
			pr_info("enable charger otg\n");
			charger_dev_enable_otg(primary_charger, true);
		} else if (y_cable_type == Y_CABLE_DISCONNECT) {
			pr_info("ycable plug out\n");
		}

		if (y_cable_type != Y_CABLE_DISCONNECT)
			hrtimer_start(&g_thub_kthread_timer,
				ktime_set(0, msec_to_nsec(SW_POLLING_PERIOD)),
				HRTIMER_MODE_REL);
		y_cable_type_last = y_cable_type;

		mutex_unlock(&thub_polling_mutex);
	}
	return 0;
}

void ycable_thread_init(void)
{
	hrtimer_init(&g_thub_kthread_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	g_thub_kthread_timer.function = thub_kthread_hrtimer_func;
	kthread_run(thub_polling_thread, NULL, "thub_thread");
}
