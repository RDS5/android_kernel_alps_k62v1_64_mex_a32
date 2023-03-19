/*
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

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/reboot.h>
#include <linux/workqueue.h>

#include <mt-plat/charger_type.h>
#include <mt-plat/mtk_boot.h>
#include <mt-plat/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>

#ifdef CONFIG_MTK_USB2JTAG_SUPPORT
#include <mt-plat/mtk_usb2jtag.h>
#endif

#ifdef CONFIG_Y_CABLE
#include "../../../usb20/hw_ycable.h"
#endif
/* #define __FORCE_USB_TYPE__ */
#ifndef CONFIG_TCPC_CLASS
#define __SW_CHRDET_IN_PROBE_PHASE__
#endif

static enum charger_type g_chr_type;
#ifdef __SW_CHRDET_IN_PROBE_PHASE__
static struct work_struct chr_work;
#endif
static DEFINE_MUTEX(chrdet_lock);
static struct power_supply *chrdet_psy;
static struct power_supply *g_uscp_psy;

#if !defined(CONFIG_FPGA_EARLY_PORTING)
static bool first_connect = true;
#endif

static struct work_struct otg_work;
struct workqueue_struct *otg_wq = NULL;
static struct notifier_block otg_nb;
static bool otg_connect;
static bool slave_connect;
int otg_register_notifier(struct notifier_block *nb);

#define DP_EN             0x2
#define DM_EN             0x1
#define DM_DISABLE        0x0
#define DP_CMP_DISABLE    0x0
#define DP_CMP_ENABLE     0x2
#define DM_CMP_ENABLE     0x1
#define CMP_STATUS_TRUE    1

int chrdet_inform_psy_changed(enum charger_type chg_type,
				bool chg_online)
{
	int ret = 0;
	union power_supply_propval propval;

	pr_info("charger type: %s: online = %d, type = %d\n", __func__,
		chg_online, chg_type);

	/* Inform chg det power supply */
	if (chg_online) {
		propval.intval = chg_online;
		ret = power_supply_set_property(chrdet_psy,
				POWER_SUPPLY_PROP_ONLINE, &propval);
		if (ret < 0)
			pr_notice("%s: psy online failed, ret = %d\n",
				__func__, ret);

		propval.intval = chg_type;
		ret = power_supply_set_property(chrdet_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
		if (ret < 0)
			pr_notice("%s: psy type failed, ret = %d\n",
				__func__, ret);

		return ret;
	}

	propval.intval = chg_type;
	ret = power_supply_set_property(chrdet_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
	if (ret < 0)
		pr_notice("%s: psy type failed, ret(%d)\n", __func__, ret);

	propval.intval = chg_online;
	ret = power_supply_set_property(chrdet_psy, POWER_SUPPLY_PROP_ONLINE,
				&propval);
	if (ret < 0)
		pr_notice("%s: psy online failed, ret(%d)\n", __func__, ret);
	return ret;
}

static void chrdet_inform_uscp(const bool chg_online)
{
	int ret;
	union power_supply_propval propval;

	if (!g_uscp_psy) {
		g_uscp_psy = power_supply_get_by_name("uscp");
		if (!g_uscp_psy) {
			pr_notice("%s: get uscp failed\n", __func__);
			return;
		}
	}

	pr_info("%s: online = %d\n", __func__, chg_online);
	propval.intval = chg_online;
	ret = power_supply_set_property(g_uscp_psy,
		POWER_SUPPLY_PROP_ONLINE, &propval);
	if (ret < 0)
		pr_err("%s: notice uscp online failed, ret = %d\n",
			__func__, ret);
	return;
}

#if defined(CONFIG_FPGA_EARLY_PORTING)
/* FPGA */
int hw_charging_get_charger_type(void)
{
	/* Force Standard USB Host */
	g_chr_type = STANDARD_HOST;
	chrdet_inform_psy_changed(g_chr_type, 1);
	return g_chr_type;
}

#else
/* EVB / Phone */
static void hw_bc11_init(void)
{
	int timeout = 200;

	msleep(200);

	if (first_connect == true) {
		/* add make sure USB Ready */
		if (is_usb_rdy() == false) {
			pr_info("CDP, block\n");
			while (is_usb_rdy() == false && timeout > 0) {
				msleep(100);
				timeout--;
			}
			if (timeout == 0)
				pr_info("CDP, timeout\n");
			else
				pr_info("CDP, free\n");
		} else
			pr_info("CDP, PASS\n");
		first_connect = false;
	}

	/* RG_bc11_BIAS_EN=1 */
	bc11_set_register_value(PMIC_RG_BC11_BIAS_EN, 1);
	/* RG_bc11_VSRC_EN[1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0);
	/* RG_bc11_VREF_VTH = [1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0);
	/* RG_bc11_IPU_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPU_EN, 0);
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0);
	/* bc11_RST=1 */
	bc11_set_register_value(PMIC_RG_BC11_RST, 1);
	/* bc11_BB_CTRL=1 */
	bc11_set_register_value(PMIC_RG_BC11_BB_CTRL, 1);
	/* add pull down to prevent PMIC leakage */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x1);
	msleep(50);

	Charger_Detect_Init();
}

static unsigned int hw_bc11_DCD(void)
{
	unsigned int wChargerAvail = 0;
	/* RG_bc11_IPU_EN[1.0] = 10 */
	bc11_set_register_value(PMIC_RG_BC11_IPU_EN, 0x2);
	/* RG_bc11_IPD_EN[1.0] = 01 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x1);
	/* RG_bc11_VREF_VTH = [1:0]=01 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x1);
	/* RG_bc11_CMP_EN[1.0] = 10 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x2);
	msleep(80);
	/* mdelay(80); */
	wChargerAvail = bc11_get_register_value(PMIC_RGS_BC11_CMP_OUT);

	/* RG_bc11_IPU_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPU_EN, 0x0);
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x0);
	/* RG_bc11_VREF_VTH = [1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0);
	return wChargerAvail;
}

#if 0 /* If need to detect apple adapter, please enable this code section */
static unsigned int hw_bc11_stepA1(void)
{
	unsigned int wChargerAvail = 0;
	/* RG_bc11_IPD_EN[1.0] = 01 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x1);
	/* RG_bc11_VREF_VTH = [1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 01 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x1);
	msleep(80);
	/* mdelay(80); */
	wChargerAvail = bc11_get_register_value(PMIC_RGS_BC11_CMP_OUT);

	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x0);
	return wChargerAvail;
}
#endif

static unsigned int hw_bc11_stepA2(void)
{
	unsigned int wChargerAvail = 0;
	/* RG_bc11_VSRC_EN[1.0] = 10 */
	bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x2);
	/* RG_bc11_IPD_EN[1:0] = 01 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x1);
	/* RG_bc11_VREF_VTH = [1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 01 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x1);
	msleep(80);
	/* mdelay(80); */
	wChargerAvail = bc11_get_register_value(PMIC_RGS_BC11_CMP_OUT);

	/* RG_bc11_VSRC_EN[1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x0);
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x0);
	return wChargerAvail;
}

static unsigned int hw_bc11_stepB2(void)
{
	unsigned int wChargerAvail = 0;

	/*enable the voltage source to DM*/
	bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x1);
	/* enable the pull-down current to DP */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x2);
	/* VREF threshold voltage for comparator  =0.325V */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0);
	/* enable the comparator to DP */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x2);
	msleep(80);
	wChargerAvail = bc11_get_register_value(PMIC_RGS_BC11_CMP_OUT);
	/*reset to default value*/
	bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x0);
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x0);
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x0);
	if (wChargerAvail == 1) {
		bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x2);
		pr_info("charger type: DCP, keep DM voltage source in stepB2\n");
	}
	return wChargerAvail;
}

static void hw_bc11_done(void)
{
	/* RG_bc11_VSRC_EN[1:0]=00 */
	bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, 0x0);
	/* RG_bc11_VREF_VTH = [1:0]=0 */
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0);
	/* RG_bc11_CMP_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, 0x0);
	/* RG_bc11_IPU_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPU_EN, 0x0);
	/* RG_bc11_IPD_EN[1.0] = 00 */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0x0);
	/* RG_bc11_BIAS_EN=0 */
	bc11_set_register_value(PMIC_RG_BC11_BIAS_EN, 0x0);
	Charger_Detect_Release();
}

static void dump_charger_name(enum charger_type type)
{
	switch (type) {
	case CHARGER_UNKNOWN:
		pr_info("charger type: %d, CHARGER_UNKNOWN\n", type);
		break;
	case STANDARD_HOST:
		pr_info("charger type: %d, Standard USB Host\n", type);
		break;
	case CHARGING_HOST:
		pr_info("charger type: %d, Charging USB Host\n", type);
		break;
	case NONSTANDARD_CHARGER:
		pr_info("charger type: %d, Non-standard Charger\n", type);
		break;
	case STANDARD_CHARGER:
		pr_info("charger type: %d, Standard Charger\n", type);
		break;
	case APPLE_2_1A_CHARGER:
		pr_info("charger type: %d, APPLE_2_1A_CHARGER\n", type);
		break;
	case APPLE_1_0A_CHARGER:
		pr_info("charger type: %d, APPLE_1_0A_CHARGER\n", type);
		break;
	case APPLE_0_5A_CHARGER:
		pr_info("charger type: %d, APPLE_0_5A_CHARGER\n", type);
		break;
	default:
		pr_info("charger type: %d, Not Defined!!!\n", type);
		break;
	}
}

int hw_charging_get_charger_type(void)
{
	enum charger_type CHR_Type_num = CHARGER_UNKNOWN;

#ifdef CONFIG_MTK_USB2JTAG_SUPPORT
	if (usb2jtag_mode()) {
		pr_info("[USB2JTAG] in usb2jtag mode, skip charger detection\n");
		return STANDARD_HOST;
	}
#endif

	hw_bc11_init();

	if (hw_bc11_DCD()) {
#if 0 /* If need to detect apple adapter, please enable this code section */
		if (hw_bc11_stepA1())
			CHR_Type_num = APPLE_2_1A_CHARGER;
		else
			CHR_Type_num = NONSTANDARD_CHARGER;
#else
		CHR_Type_num = NONSTANDARD_CHARGER;
#endif
	} else {
		if (hw_bc11_stepA2()) {
			if (hw_bc11_stepB2())
				CHR_Type_num = STANDARD_CHARGER;
			else
				CHR_Type_num = CHARGING_HOST;
		} else
			CHR_Type_num = STANDARD_HOST;
	}

	if (CHR_Type_num != STANDARD_CHARGER)
		hw_bc11_done();
	else
		pr_info("charger type: skip bc11 release for BC12 DCP SPEC\n");

	dump_charger_name(CHR_Type_num);

#ifdef __FORCE_USB_TYPE__
	CHR_Type_num = STANDARD_HOST;
	pr_info("charger type: Froce to STANDARD_HOST\n");
#endif

	return CHR_Type_num;
}

static int bc11_read_dp_cmp_reg_status(int count)
{
	unsigned int ret = 0;

	/* enable dp cmp and set vref voltage */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, DP_EN);
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0); /* Vref: 0.375v */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, DP_CMP_ENABLE);

	while(count) {
		ret = bc11_get_register_value(PMIC_RGS_BC11_CMP_OUT);
		if(ret == CMP_STATUS_TRUE) {
			pr_info("%s dp cmp true\n", __func__);
			break;
		}
		usleep_range(5000, 5100); /* 5000-5100: 50ms */
		count--;
	}
	/* disable dp cmp  */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0);
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, DP_CMP_DISABLE);
	return ret;
}

static int bc11_read_dm_cmp_reg_status(void)
{
	unsigned int ret;

	/* enable dm cmp and set vref voltage */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, DM_EN);
	bc11_set_register_value(PMIC_RG_BC11_VREF_VTH, 0x0); /* Vref: 0.375v */
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, DM_CMP_ENABLE);
	usleep_range(5000, 5100); /* 5000-5100: 5ms */

	ret = bc11_get_register_value(PMIC_RGS_BC11_CMP_OUT);
	if(ret == CMP_STATUS_TRUE)
		pr_info("%s dm cmp true\n", __func__);

	/* disable dm cmp  */
	bc11_set_register_value(PMIC_RG_BC11_IPD_EN, 0);
	bc11_set_register_value(PMIC_RG_BC11_CMP_EN, DP_CMP_DISABLE);

	return ret;
}
static void do_otg_work(struct work_struct *work)
{
	unsigned int ret = 0;
	int try_time = 2; /* read cmp try 2 times */
	bool dm_status = false;

	if (!work || !otg_connect)
		return;

	pr_info("do otg work start\n");
	hw_bc11_init();
	while(try_time) {
		if (dm_status)
			break;

		while (true) {
			/* 1 means bc1.2 second check for qcom */
			if (try_time == 1) {
				/* read  DP 100 times for qcom plarform 500ms */
				ret = bc11_read_dp_cmp_reg_status(100);
				if (ret == CMP_STATUS_TRUE || !otg_connect) {
					break;
				} else {
					pr_info("read dp statue timeout reset try_time 2\n");
					try_time = 3; /* if timeout, reset try time 3 */
					break;
				}
			}
			/* read  DP 300 times for 1500ms */
			ret = bc11_read_dp_cmp_reg_status(300);
			if (ret == CMP_STATUS_TRUE || !otg_connect)
				break;
			/* read  DM  */
			ret = bc11_read_dm_cmp_reg_status();
			if (ret == CMP_STATUS_TRUE || !otg_connect) {
				pr_info("read dm status true\n");
				dm_status = true;
				break;
			}
		}
		if (ret == CMP_STATUS_TRUE) {
			pr_info("start enable DM\n");
			bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, DM_EN);
			usleep_range(50000, 51000); /* 5000-5100: 50ms */
		}
		pr_info("end disable DM\n");
		bc11_set_register_value(PMIC_RG_BC11_VSRC_EN, DM_DISABLE);
		try_time--;
	}

	hw_bc11_done();
	pr_info("do otg work end\n");
}

static int otg_notifier_call(struct notifier_block *nb, unsigned long event,
	void *data)
{
	unsigned int rcv_event;

	if (!nb)
		return NOTIFY_OK;

	rcv_event = (unsigned int)event;
	pr_info("otg_notifier_call event=%u\n", rcv_event);

	switch (rcv_event) {
	case OTG_CHARGER_CONNECT_EVENT:
		otg_connect = true;
		usleep_range(10000, 11000); /* 10000-11000: 10ms */
		queue_work(otg_wq, &otg_work);
		break;
	case OTG_CHARGER_DISCONNECT_EVENT:
		otg_connect = false;
		/* do nothing */
		break;
	case OTG_CHARGER_SLAVE_CONNECT_EVENT:
		slave_connect = true;
		/* do nothing */
		break;
	case OTG_CHARGER_SAVLE_DISCONNECT_EVENT:
		slave_connect = false;
		queue_work(otg_wq, &otg_work);
		break;
	default :
		break;
	}
	return NOTIFY_OK;
}

#define START_CHARGING      0x40
extern void charge_event_notify(unsigned int event);
void mtk_pmic_enable_chr_type_det(bool en)
{
#ifndef CONFIG_TCPC_CLASS
	if (!mt_usb_is_device()) {
#ifdef CONFIG_Y_CABLE
		g_chr_type = ycable_get_charger_type();
		pr_info("charger type: g_chr_type, Now is usb host mode. Skip detection\n");
#else
		g_chr_type = CHARGER_UNKNOWN;
		pr_info("charger type: UNKNOWN, Now is usb host mode. Skip detection\n");
#endif
		return;
	}
#endif

	mutex_lock(&chrdet_lock);

	if (en) {
		if (is_meta_mode()) {
			/* Skip charger type detection to speed up meta boot */
			pr_notice("charger type: force Standard USB Host in meta\n");
			g_chr_type = STANDARD_HOST;
			chrdet_inform_psy_changed(g_chr_type, 1);
		} else {
			pr_info("charger type: charger IN\n");
			g_chr_type = hw_charging_get_charger_type();
			chrdet_inform_psy_changed(g_chr_type, 1);
			charge_event_notify(START_CHARGING);
		}
		chrdet_inform_uscp(true);
	} else {
#ifdef CONFIG_Y_CABLE
		ycable_reset_type();
#endif
		pr_info("charger type: charger OUT\n");
		g_chr_type = CHARGER_UNKNOWN;
		chrdet_inform_psy_changed(g_chr_type, 0);
		chrdet_inform_uscp(false);
	}

	mutex_unlock(&chrdet_lock);
}

/* Charger Detection */
void do_charger_detect(void)
{
	if (pmic_get_register_value(PMIC_RGS_CHRDET))
		mtk_pmic_enable_chr_type_det(true);
	else
		mtk_pmic_enable_chr_type_det(false);
}

/* PMIC Int Handler */
void chrdet_int_handler(void)
{
	/*
	 * pr_notice("[chrdet_int_handler]CHRDET status = %d....\n",
	 *	pmic_get_register_value(PMIC_RGS_CHRDET));
	 */
	if (!pmic_get_register_value(PMIC_RGS_CHRDET)) {
		int boot_mode = 0;

		hw_bc11_done();
		boot_mode = get_boot_mode();

		if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
		    || boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_info("[%s] Unplug Charger/USB\n", __func__);
#ifndef CONFIG_TCPC_CLASS
			pr_info("%s: system_state=%d\n", __func__,
				system_state);
#else
			return;
#endif
		}
	}
	pmic_set_register_value(PMIC_RG_USBDL_RST, 1);

	do_charger_detect();
}

/* Charger Probe Related */
#ifdef __SW_CHRDET_IN_PROBE_PHASE__
static void do_charger_detection_work(struct work_struct *data)
{
	if (pmic_get_register_value(PMIC_RGS_CHRDET))
		do_charger_detect();
}
#endif

static int __init pmic_chrdet_init(void)
{
	int ret;

	mutex_init(&chrdet_lock);
	chrdet_psy = power_supply_get_by_name("charger");
	if (!chrdet_psy) {
		pr_notice("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}

#ifdef __SW_CHRDET_IN_PROBE_PHASE__
	/* do charger detect here to prevent HW miss interrupt*/
	INIT_WORK(&chr_work, do_charger_detection_work);
	schedule_work(&chr_work);
#endif

#ifndef CONFIG_TCPC_CLASS
	pmic_register_interrupt_callback(INT_CHRDET_EDGE, chrdet_int_handler);
	pmic_enable_interrupt(INT_CHRDET_EDGE, 1, "PMIC");
#endif

	otg_nb.notifier_call = otg_notifier_call;
	ret = otg_register_notifier(&otg_nb);
	if (ret < 0)
		pr_notice("%s: otg nogifier failed\n", __func__);
	otg_wq = create_singlethread_workqueue("otg_wq");
	if (!otg_wq) {
		pr_notice("%s: create_singlethread_workqueue failed\n", __func__);
		return -EINVAL;
	}
	INIT_WORK(&otg_work, do_otg_work);

	return 0;
}

late_initcall(pmic_chrdet_init);

#endif /* CONFIG_FPGA_EARLY_PORTING */
