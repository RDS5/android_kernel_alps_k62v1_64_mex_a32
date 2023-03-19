/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/pinctrl/consumer.h>

#include "flashlight-core.h"
#include "flashlight-dt.h"

/* device tree should be defined in flashlight-dt.h */
#ifndef AW36514_DTNAME
#define AW36514_DTNAME "mediatek,flashlights_aw36514"
#endif
#ifndef AW36514_DTNAME_I2C
#define AW36514_DTNAME_I2C "mediatek,flashlights_aw36514_i2c"
#endif

#define AW36514_NAME "flashlights-aw36514"

/* define registers */
#define AW36514_REG_RESET (0x07)
#define AW36514_RESET (0x80)
#define AW36514_REG_ENABLE (0x01)
#define AW36514_MASK_ENABLE_LED2 (0x01)
#define AW36514_MASK_ENABLE_LED1 (0x02)
#define AW36514_DISABLE (0x00)
#define AW36514_ENABLE_LED2 (0x01)
#define AW36514_ENABLE_LED2_TORCH (0x09)
#define AW36514_ENABLE_LED2_FLASH (0x0D)
#define AW36514_ENABLE_LED1 (0x02)
#define AW36514_ENABLE_LED1_TORCH (0x0A)
#define AW36514_ENABLE_LED1_FLASH (0x0E)

#define AW36514_REG_TORCH_LEVEL_LED2 (0x05)
#define AW36514_REG_FLASH_LEVEL_LED2 (0x03)
#define AW36514_REG_TORCH_LEVEL_LED1 (0x06)
#define AW36514_REG_FLASH_LEVEL_LED1 (0x04)

#define AW36514_REG_TIMING_CONF (0x08)
#define AW36514_TORCH_RAMP_TIME (0x10)
#define SY7806_TORCH_RAMP_TIME (0x00)
#define AW36514_FLASH_TIMEOUT   (0x0F)
#define SY7806_FLASH_TIMEOUT   (0x0F)

#define FLASHLIGHT_REG_DEVICE_ID (0x0C)
#define AW36514_DEVICE_ID (0x0A)
#define SY7806_DEVICE_ID (0x18)
#define FL_SET_LED (0x3F)
/* define channel, level */
#define AW36514_CHANNEL_NUM 2
#define AW36514_CHANNEL_CH1 0
#define AW36514_CHANNEL_CH2 1

#define AW36514_LEVEL_NUM 39
#define AW36514_LEVEL_TORCH 3

#define AW36514_HW_TIMEOUT 400 /* ms */

/* define mutex and work queue */
static DEFINE_MUTEX(aw36514_mutex);
static struct work_struct aw36514_work_ch1;
static struct work_struct aw36514_work_ch2;

/* define pinctrl */
#define AW36514_PINCTRL_PIN_HWEN 0
#define AW36514_PINCTRL_PINSTATE_LOW 0
#define AW36514_PINCTRL_PINSTATE_HIGH 1
#define AW36514_PINCTRL_STATE_HWEN_HIGH "hwen_high"
#define AW36514_PINCTRL_STATE_HWEN_LOW "hwen_low"
static struct pinctrl *aw36514_pinctrl;
static struct pinctrl_state *aw36514_hwen_high;
static struct pinctrl_state *aw36514_hwen_low;

/* aw36514 revision */
static int sy7806;
static int chip;

/* define usage count */
static int use_count;

/* define i2c */
static struct i2c_client *aw36514_i2c_client;

/* platform data */
struct aw36514_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

/* aw36514 chip data */
struct aw36514_chip_data {
	struct i2c_client *client;
	struct aw36514_platform_data *pdata;
	struct mutex lock;
};


/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int aw36514_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	aw36514_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(aw36514_pinctrl)) {
		pr_err("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(aw36514_pinctrl);
		return ret;
	}

	/* Flashlight HWEN pin initialization */
	aw36514_hwen_high = pinctrl_lookup_state(aw36514_pinctrl,
		AW36514_PINCTRL_STATE_HWEN_HIGH);
	if (IS_ERR(aw36514_hwen_high)) {
		pr_err("Failed to init (%s)\n", AW36514_PINCTRL_STATE_HWEN_HIGH);
		ret = PTR_ERR(aw36514_hwen_high);
	}
	aw36514_hwen_low = pinctrl_lookup_state(aw36514_pinctrl,
		AW36514_PINCTRL_STATE_HWEN_LOW);
	if (IS_ERR(aw36514_hwen_low)) {
		pr_err("Failed to init (%s)\n", AW36514_PINCTRL_STATE_HWEN_LOW);
		ret = PTR_ERR(aw36514_hwen_low);
	}

	return ret;
}

static int aw36514_pinctrl_set(int pin, int state)
{
	int ret = 0;

	if (IS_ERR(aw36514_pinctrl)) {
		pr_err("pinctrl is not available\n");
		return -1;
	}

	switch (pin) {
	case AW36514_PINCTRL_PIN_HWEN:
		if (state == AW36514_PINCTRL_PINSTATE_LOW &&
			!IS_ERR(aw36514_hwen_low))
			pinctrl_select_state(aw36514_pinctrl, aw36514_hwen_low);
		else if (state == AW36514_PINCTRL_PINSTATE_HIGH &&
			!IS_ERR(aw36514_hwen_high))
			pinctrl_select_state(aw36514_pinctrl, aw36514_hwen_high);
		else
			pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	default:
		pr_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	}
	pr_debug("pin(%d) state(%d)\n", pin, state);

	return ret;
}


/******************************************************************************
 * aw36514 operations
 *****************************************************************************/
static const int aw36514_current[AW36514_LEVEL_NUM] = {
	 22, 46, 70, 93, 116, 140, 163, 187, 210, 234,
	257, 281, 304, 327, 351, 374, 398, 421, 445, 468,
	492, 515, 539, 562, 585, 609, 632, 656, 679, 703,
	726, 761, 796, 832, 869, 902, 937, 972, 1008
};

static const unsigned char sy7806_torch_level[AW36514_LEVEL_NUM] = {
	0x47, 0x6a, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char aw36514_torch_level[AW36514_LEVEL_NUM] = {
	0x42, 0x63, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char aw36514_flash_level[AW36514_LEVEL_NUM] = {
	0x05, 0x0A, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	0x99, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA
};
static const unsigned char sy7806_flash_level[AW36514_LEVEL_NUM] = {
	0x01, 0x04, 0x08, 0x10, 0x18, 0x22, 0x2A, 0x32, 0x3B, 0x43,
	0x4C, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54,
	0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54,
	0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54
};
static unsigned char aw36514_reg_enable;
static int aw36514_level_ch1 = -1;
static int aw36514_level_ch2 = -1;

static int aw36514_is_torch(int level)
{
	if (level >= AW36514_LEVEL_TORCH)
		return -1;

	return 0;
}

static int aw36514_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= AW36514_LEVEL_NUM)
		level = AW36514_LEVEL_NUM - 1;

	return level;
}

/* i2c wrapper function */
static int aw36514_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct aw36514_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		pr_err("failed writing at 0x%02x\n", reg);

	return ret;
}

static int aw36514_read_reg(struct i2c_client *client, u8 reg)
{
	int val = 0;
	struct aw36514_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	val = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	return val;
}

/* flashlight enable function */
static int aw36514_enable_ch1(void)
{
	unsigned char reg, val;

	reg = AW36514_REG_ENABLE;
	if (!aw36514_is_torch(aw36514_level_ch1))
		aw36514_reg_enable |= AW36514_ENABLE_LED1_TORCH;
	else
		aw36514_reg_enable |= AW36514_ENABLE_LED1_FLASH;

	val = aw36514_reg_enable;

	return aw36514_write_reg(aw36514_i2c_client, reg, val);
}

static int aw36514_enable_ch2(void)
{
	unsigned char reg, val;

	reg = AW36514_REG_ENABLE;
	if (!aw36514_is_torch(aw36514_level_ch2))
		aw36514_reg_enable |= AW36514_ENABLE_LED2_TORCH;
	else
		aw36514_reg_enable |= AW36514_ENABLE_LED2_FLASH;

	val = aw36514_reg_enable;

	return aw36514_write_reg(aw36514_i2c_client, reg, val);
}

static int aw36514_enable(int channel)
{
	if (channel == AW36514_CHANNEL_CH1) {
		aw36514_enable_ch1();
	} else if (channel == AW36514_CHANNEL_CH2) {
		aw36514_enable_ch2();
	} else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

/* flashlight disable function */
static int aw36514_disable_ch1(void)
{
	unsigned char reg, val;

	reg = AW36514_REG_ENABLE;
	if (aw36514_reg_enable & AW36514_MASK_ENABLE_LED2)
		aw36514_reg_enable &= (~AW36514_ENABLE_LED1);
	else
		aw36514_reg_enable &= (~AW36514_ENABLE_LED1_FLASH);

	val = aw36514_reg_enable;

	return aw36514_write_reg(aw36514_i2c_client, reg, val);
}

static int aw36514_disable_ch2(void)
{
	unsigned char reg, val;

	reg = AW36514_REG_ENABLE;
	if (aw36514_reg_enable & AW36514_MASK_ENABLE_LED1)
		aw36514_reg_enable &= (~AW36514_ENABLE_LED2);
	else
		aw36514_reg_enable &= (~AW36514_ENABLE_LED2_FLASH);

	val = aw36514_reg_enable;

	return aw36514_write_reg(aw36514_i2c_client, reg, val);
}

static int aw36514_disable(int channel)
{
	if (channel == AW36514_CHANNEL_CH1) {
		aw36514_disable_ch1();
	} else if (channel == AW36514_CHANNEL_CH2) {
		aw36514_disable_ch2();
	} else {
		pr_err("Error channel\n");
		return -1;
	}
	return 0;
}

/* set flashlight level */
static int aw36514_set_level_ch1(int level)
{
	int ret;
	unsigned char reg, val;

	level = aw36514_verify_level(level);

	/* set torch brightness level */
	reg = AW36514_REG_TORCH_LEVEL_LED1;
	if (sy7806)
		val = sy7806_torch_level[level];
	else
		val = aw36514_torch_level[level];
	ret = aw36514_write_reg(aw36514_i2c_client, reg, val);

	aw36514_level_ch1 = level;

	/* set flash brightness level */
	reg = AW36514_REG_FLASH_LEVEL_LED1;
	if (sy7806)
		val = sy7806_flash_level[level];
	else
		val = aw36514_flash_level[level];
	ret = aw36514_write_reg(aw36514_i2c_client, reg, val);

	return ret;
}

static int aw36514_set_level_ch2(int level)
{
	int ret;
	unsigned char reg, val;

	level = aw36514_verify_level(level);

	/* set torch brightness level */
	reg = AW36514_REG_TORCH_LEVEL_LED2;
	if (sy7806)
		val = sy7806_torch_level[level];
	else
		val = aw36514_torch_level[level];

	ret = aw36514_write_reg(aw36514_i2c_client, reg, val);

	aw36514_level_ch2 = level;

	/* set flash brightness level */
	reg = AW36514_REG_FLASH_LEVEL_LED2;
	if (sy7806)
		val = sy7806_flash_level[level];
	else
		val = aw36514_flash_level[level];

	ret = aw36514_write_reg(aw36514_i2c_client, reg, val);

	return ret;
}

static int aw36514_set_level(int channel, int level)
{
	if (channel == AW36514_CHANNEL_CH1) {
		aw36514_set_level_ch1(level);
	} else if (channel == AW36514_CHANNEL_CH2) {
		aw36514_set_level_ch2(level);
	} else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

/* flashlight init */
int aw36514_init(void)
{
	int ret;
	unsigned char reg, val;

	aw36514_pinctrl_set(AW36514_PINCTRL_PIN_HWEN, AW36514_PINCTRL_PINSTATE_HIGH);
	msleep(20);
	aw36514_write_reg(aw36514_i2c_client, AW36514_REG_RESET, AW36514_RESET);
	msleep(20);

	/* clear enable register */
	reg = AW36514_REG_ENABLE;
	val = AW36514_DISABLE;
	ret = aw36514_write_reg(aw36514_i2c_client, reg, val);

	aw36514_reg_enable = val;

	/* set torch current ramp time and flash timeout */
	reg = AW36514_REG_TIMING_CONF;
	/* get chip revision */
	chip = aw36514_read_reg(aw36514_i2c_client, FLASHLIGHT_REG_DEVICE_ID);


	if (chip == AW36514_DEVICE_ID) {
		val = AW36514_TORCH_RAMP_TIME | AW36514_FLASH_TIMEOUT;
		sy7806 = 0;
		pr_info("The chip is AW36514!!\n");
	} else if (chip == SY7806_DEVICE_ID) {
		aw36514_write_reg(aw36514_i2c_client,
			AW36514_REG_FLASH_LEVEL_LED2, FL_SET_LED);
		aw36514_write_reg(aw36514_i2c_client,
			AW36514_REG_TORCH_LEVEL_LED2, FL_SET_LED);
		val = SY7806_TORCH_RAMP_TIME | SY7806_FLASH_TIMEOUT;
		sy7806 = 1;
		pr_info("The chip is SY7806!!\n");
	} else {
		pr_info("The chip not exits!!\n");
		return val;
	}
	ret = aw36514_write_reg(aw36514_i2c_client, reg, val);

	return ret;
}

/* flashlight uninit */
int aw36514_uninit(void)
{
	aw36514_disable(AW36514_CHANNEL_CH1);
	aw36514_disable(AW36514_CHANNEL_CH2);
	aw36514_pinctrl_set(AW36514_PINCTRL_PIN_HWEN,
		AW36514_PINCTRL_PINSTATE_LOW);

	return 0;
}


/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer aw36514_timer_ch1;
static struct hrtimer aw36514_timer_ch2;
static unsigned int aw36514_timeout_ms[AW36514_CHANNEL_NUM];

static void aw36514_work_disable_ch1(struct work_struct *data)
{
	pr_debug("ht work queue callback\n");
	aw36514_disable_ch1();
}

static void aw36514_work_disable_ch2(struct work_struct *data)
{
	pr_debug("lt work queue callback\n");
	aw36514_disable_ch2();
}

static enum hrtimer_restart aw36514_timer_func_ch1(struct hrtimer *timer)
{
	schedule_work(&aw36514_work_ch1);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart aw36514_timer_func_ch2(struct hrtimer *timer)
{
	schedule_work(&aw36514_work_ch2);
	return HRTIMER_NORESTART;
}

static int aw36514_timer_start(int channel, ktime_t ktime)
{
	if (channel == AW36514_CHANNEL_CH1) {
		hrtimer_start(&aw36514_timer_ch1, ktime, HRTIMER_MODE_REL);
	} else if (channel == AW36514_CHANNEL_CH2) {
		hrtimer_start(&aw36514_timer_ch2, ktime, HRTIMER_MODE_REL);
	} else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

static int aw36514_timer_cancel(int channel)
{
	if (channel == AW36514_CHANNEL_CH1) {
		hrtimer_cancel(&aw36514_timer_ch1);
	} else if (channel == AW36514_CHANNEL_CH2) {
		hrtimer_cancel(&aw36514_timer_ch2);
	} else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int aw36514_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;
	unsigned int s;
	unsigned int ns;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	/* verify channel */
	if (channel < 0 || channel >= AW36514_CHANNEL_NUM) {
		pr_err("Failed with error channel\n");
		return -EINVAL;
	}

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		pr_debug("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n",
				channel, (int)fl_arg->arg);
		aw36514_timeout_ms[channel] = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		pr_debug("FLASH_IOC_SET_DUTY(%d): %d\n",
				channel, (int)fl_arg->arg);
		aw36514_set_level(channel, fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		pr_debug("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (aw36514_timeout_ms[channel]) {
				s = aw36514_timeout_ms[channel] / 1000;
				ns = aw36514_timeout_ms[channel] % 1000
				* 1000000;
				ktime = ktime_set(s, ns);
				aw36514_timer_start(channel, ktime);
			}
			aw36514_enable(channel);
		} else {
			aw36514_disable(channel);
			aw36514_timer_cancel(channel);
		}
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		pr_debug("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = AW36514_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		pr_debug("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = AW36514_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = aw36514_verify_level(fl_arg->arg);
		pr_debug("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n",
				channel, (int)fl_arg->arg);
		fl_arg->arg = aw36514_current[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		pr_debug("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = AW36514_HW_TIMEOUT;
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int aw36514_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int aw36514_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int aw36514_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&aw36514_mutex);
	if (set) {
		if (!use_count)
			ret = aw36514_init();
		use_count++;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = aw36514_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&aw36514_mutex);

	return ret;
}

static ssize_t aw36514_strobe_store(struct flashlight_arg arg)
{
	aw36514_set_driver(1);
	aw36514_set_level(arg.channel, arg.level);
	aw36514_timeout_ms[arg.channel] = 0;
	aw36514_enable(arg.channel);
	msleep(arg.dur);
	aw36514_disable(arg.channel);
	aw36514_set_driver(0);

	return 0;
}

static struct flashlight_operations aw36514_ops = {
	aw36514_open,
	aw36514_release,
	aw36514_ioctl,
	aw36514_strobe_store,
	aw36514_set_driver
};


/******************************************************************************
 * I2C device and driver
 *****************************************************************************/
static int aw36514_chip_init(struct aw36514_chip_data *chip)
{
	return 0;
}

static int aw36514_parse_dt(struct device *dev,
		struct aw36514_platform_data *pdata)
{
	struct device_node *np = NULL, *cnp = NULL;
	u32 decouple = 0;
	int i = 0;

	if (!dev || !dev->of_node || !pdata)
		return -ENODEV;

	np = dev->of_node;

	pdata->channel_num = of_get_child_count(np);
	if (!pdata->channel_num) {
		pr_info("Parse no dt, node.\n");
		return 0;
	}
	pr_info("Channel number(%d).\n", pdata->channel_num);

	if (of_property_read_u32(np, "decouple", &decouple))
		pr_info("Parse no dt, decouple.\n");

	pdata->dev_id = devm_kzalloc(dev,
			pdata->channel_num * sizeof(struct flashlight_device_id),
			GFP_KERNEL);
	if (!pdata->dev_id)
		return -ENOMEM;

	for_each_child_of_node(np, cnp) {
		if (of_property_read_u32(cnp, "type", &pdata->dev_id[i].type))
			goto err_node_put;
		if (of_property_read_u32(cnp, "ct", &pdata->dev_id[i].ct))
			goto err_node_put;
		if (of_property_read_u32(cnp, "part", &pdata->dev_id[i].part))
			goto err_node_put;
		snprintf(pdata->dev_id[i].name, FLASHLIGHT_NAME_SIZE, AW36514_NAME);
		pdata->dev_id[i].channel = i;
		pdata->dev_id[i].decouple = decouple;

		pr_info("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				pdata->dev_id[i].type, pdata->dev_id[i].ct,
				pdata->dev_id[i].part, pdata->dev_id[i].name,
				pdata->dev_id[i].channel, pdata->dev_id[i].decouple);
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

static int aw36514_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct aw36514_platform_data *pdata = dev_get_platdata(&client->dev);
	struct aw36514_chip_data *chip = NULL;
	int err;
	int i;

	pr_debug("Probe start.\n");

	/* check i2c */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}

	/* init chip private data */
	chip = kzalloc(sizeof(struct aw36514_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}
	chip->client = client;

	/* init platform data */
	if (!pdata) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto err_free;
		}
		client->dev.platform_data = pdata;
		err = aw36514_parse_dt(&client->dev, pdata);
		if (err)
			goto err_free;
	}
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
	aw36514_i2c_client = client;

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	/* init work queue */
	INIT_WORK(&aw36514_work_ch1, aw36514_work_disable_ch1);
	INIT_WORK(&aw36514_work_ch2, aw36514_work_disable_ch2);

	/* init timer */
	hrtimer_init(&aw36514_timer_ch1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw36514_timer_ch1.function = aw36514_timer_func_ch1;
	hrtimer_init(&aw36514_timer_ch2, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw36514_timer_ch2.function = aw36514_timer_func_ch2;
	aw36514_timeout_ms[AW36514_CHANNEL_CH1] = 100;
	aw36514_timeout_ms[AW36514_CHANNEL_CH2] = 100;

	/* init chip hw */
	aw36514_chip_init(chip);

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(&pdata->dev_id[i],
			&aw36514_ops)) {
				err = -EFAULT;
				goto err_free;
			}
	} else {
		if (flashlight_dev_register(AW36514_NAME, &aw36514_ops)) {
			err = -EFAULT;
			goto err_free;
		}
	}

	pr_debug("Probe done.\n");

	return 0;

err_free:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
err_out:
	return err;
}

static int aw36514_i2c_remove(struct i2c_client *client)
{
	struct aw36514_platform_data *pdata = dev_get_platdata(&client->dev);
	struct aw36514_chip_data *chip = i2c_get_clientdata(client);
	int i;

	pr_debug("Remove start.\n");

	client->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(AW36514_NAME);

	/* flush work queue */
	flush_work(&aw36514_work_ch1);
	flush_work(&aw36514_work_ch2);

	/* free resource */
	kfree(chip);

	pr_debug("Remove done.\n");

	return 0;
}

static const struct i2c_device_id aw36514_i2c_id[] = {
	{AW36514_NAME, 0},
	{ }
};

#ifdef CONFIG_OF
static const struct of_device_id aw36514_i2c_of_match[] = {
	{.compatible = AW36514_DTNAME_I2C},
	{ },
};
#endif

static struct i2c_driver aw36514_i2c_driver = {
	.driver = {
		.name = AW36514_NAME,
#ifdef CONFIG_OF
		.of_match_table = aw36514_i2c_of_match,
#endif
	},
	.probe = aw36514_i2c_probe,
	.remove = aw36514_i2c_remove,
	.id_table = aw36514_i2c_id,
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int aw36514_probe(struct platform_device *dev)
{
	pr_debug("Probe start.\n");

	/* init pinctrl */
	if (aw36514_pinctrl_init(dev)) {
		pr_debug("Failed to init pinctrl.\n");
		return -1;
	}

	if (i2c_add_driver(&aw36514_i2c_driver)) {
		pr_debug("Failed to add i2c driver.\n");
		return -1;
	}

	pr_debug("Probe done.\n");

	return 0;
}

static int aw36514_remove(struct platform_device *dev)
{
	pr_debug("Remove start.\n");

	i2c_del_driver(&aw36514_i2c_driver);

	pr_debug("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aw36514_of_match[] = {
	{.compatible = AW36514_DTNAME},
	{ },
};
MODULE_DEVICE_TABLE(of, aw36514_of_match);
#else
static struct platform_device aw36514_platform_device[] = {
	{
		.name = AW36514_NAME,
		.id = 0,
		.dev = {}
	},
	{ }
};
MODULE_DEVICE_TABLE(platform, aw36514_platform_device);
#endif

static struct platform_driver aw36514_platform_driver = {
	.probe = aw36514_probe,
	.remove = aw36514_remove,
	.driver = {
		.name = AW36514_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aw36514_of_match,
#endif
	},
};

static int __init flashlight_aw36514_init(void)
{
	int ret;

	pr_debug("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&aw36514_platform_device);
	if (ret) {
		pr_err("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&aw36514_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}

	pr_debug("Init done.\n");

	return 0;
}

static void __exit flashlight_aw36514_exit(void)
{
	pr_debug("Exit start.\n");

	platform_driver_unregister(&aw36514_platform_driver);

	pr_debug("Exit done.\n");
}

module_init(flashlight_aw36514_init);
module_exit(flashlight_aw36514_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huawei Technologies Co., Ltd.");
MODULE_DESCRIPTION("Flashlight AW36514 Driver");

