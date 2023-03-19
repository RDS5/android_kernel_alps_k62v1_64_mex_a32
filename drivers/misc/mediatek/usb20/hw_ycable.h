/*
 * hw_ycable.h
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

#ifndef _HW_YCABLE_H_
#define _HW_YCABLE_H_

#include <linux/time.h>
#include <linux/kthread.h>

#define CHARGER_VOLTAGE_HIGH_THRESHOLD 450
#define CHARGER_VOLTAGE_LOW_THRESHOLD  270
#define OTG_VOLTAGE_HIGH_THRESHOLD     150
#define OTG_VOLTAGE_LOW_THRESHOLD      0

#define UV_TO_MV                       1000
#define msec_to_nsec(x)                (x * 1000000UL)
#define SW_POLLING_PERIOD              50
#define USB_ID_ADC_CHANNEL             2

enum CABLE_TYPE {
	Y_CABLE_OTG = 0,
	Y_CABLE_CHARGER,
	Y_CABLE_UNKNOWN,
	Y_CABLE_DISCONNECT = 0x8,
	Y_CABLE_RESET = 0xF,
};

enum charger_type ycable_get_charger_type(void);
void ycable_set_vbus_status(void);
int ycable_get_type_detection(void);
void ycable_reset_type(void);
int ycable_get_type(void);
enum hrtimer_restart thub_kthread_hrtimer_func(struct hrtimer *timer);
int thub_polling_thread(void *arg);
void ycable_thread_init(void);
extern void mtk_pmic_enable_chr_type_det(bool en);
extern int IMM_IsAdcInitReady(void);
extern int chrdet_inform_psy_changed(enum charger_type chg_type,
	bool chg_online);
extern struct charger_device *primary_charger;

#endif /* _HW_YCABLE_H_ */
