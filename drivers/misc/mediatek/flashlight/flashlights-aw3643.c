/*
 * \flashlights-aw3643.c
 *
 * Copyright (c) 2020-2020 Huawei Technologies Co., Ltd.
 *
 * camera flash aw3643
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
//#include <tp_color.h>
#include "flashlight-core.h"
#include "flashlight-dt.h"

/* device tree should be defined in flashlight-dt.h */
#ifndef AW3643_DTNAME
#define AW3643_DTNAME "mediatek,flashlights_aw3643"
#endif
#ifndef AW3643_DTNAME_I2C
#define AW3643_DTNAME_I2C "mediatek,flashlights_aw3643_i2c"
#endif

#define AW3643_NAME "flashlights-aw3643"

/* define registers */
#define AW3643_REG_ENABLE (0x01)
#define AW3643_MASK_ENABLE_LED2 (0x01)
#define AW3643_MASK_ENABLE_LED1 (0x02)
#define AW3643_DISABLE (0x00)
#define AW3643_ENABLE_LED2 (0x01)
#define AW3643_ENABLE_LED2_TORCH (0x09)
#define AW3643_ENABLE_LED2_FLASH (0x0D)
#define AW3643_ENABLE_LED1 (0x02)
#define AW3643_ENABLE_LED1_TORCH (0x0A)
#define AW3643_ENABLE_LED1_FLASH (0x0E)

#define FLASH_FLAG1_REGISTER (0x0A)
#define FLASH_FLAG2_REGISTER (0x0B)

#define FLASHLIGHT_REG_TORCH_LEVEL_LED2 (0x05)
#define FLASHLIGHT_REG_FLASH_LEVEL_LED2 (0x03)
#define AW3643_REG_TORCH_LEVEL_LED1 (0x06)
#define AW3643_REG_FLASH_LEVEL_LED1 (0x04)

#define AW3643_REG_TIMING_CONF (0x08)
#define AW3643_TORCH_RAMP_TIME (0x00)
#define SY7806_TORCH_RAMP_TIME (0x00)
#define AW3643_FLASH_TIMEOUT   (0x09)
#define SY7806_FLASH_TIMEOUT   (0x0F)

#define FLASHLIGHT_REG_DEVICE_ID   (0x0C)
#define AW3643_DEVICE_ID   (0x12)
#define SY7806_DEVICE_ID   (0x18)
#define FLASHLIGHT_SET_LED   (0x3F)
/* define flag_reg */
#define LED_OVP                      (1<<1)
#define LED2_SHORT                   (1<<4)
#define LED1_SHORT                   (1<<5)
/* define channel, level */
#define AW3643_CHANNEL_NUM 2
#define AW3643_CHANNEL_CH1 0
#define AW3643_CHANNEL_CH2 1

#define AW3643_LEVEL_NUM 26
#define AW3643_LEVEL_TORCH 8

#define AW3643_HW_TIMEOUT 400 /* ms */

/* define mutex and work queue */
static DEFINE_MUTEX(aw3643_mutex);
static struct work_struct aw3643_work_ch1;
static struct work_struct aw3643_work_ch2;

/* define pinctrl */
#define AW3643_PINCTRL_PIN_HWEN 0
#define AW3643_PINCTRL_PINSTATE_LOW 0
#define AW3643_PINCTRL_PINSTATE_HIGH 1
#define AW3643_PINCTRL_STATE_HWEN_HIGH "hwen_high"
#define AW3643_PINCTRL_STATE_HWEN_LOW  "hwen_low"
static struct pinctrl *aw3643_pinctrl;
static struct pinctrl_state *aw3643_hwen_high;
static struct pinctrl_state *aw3643_hwen_low;

/* aw3643 revision */
static int sy7806;
static int chip;

/* define usage count */
static int use_count;

/* define i2c */
static struct i2c_client *aw3643_i2c_client;

/* platform data */
struct aw3643_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

/* aw3643 chip data */
struct aw3643_chip_data {
	struct i2c_client *client;
	struct aw3643_platform_data *pdata;
	struct mutex lock;
};

/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int aw3643_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	aw3643_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(aw3643_pinctrl)) {
		pr_err("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(aw3643_pinctrl);
		return ret;
	}

	/* Flashlight HWEN pin initialization */
	aw3643_hwen_high = pinctrl_lookup_state(aw3643_pinctrl,
		AW3643_PINCTRL_STATE_HWEN_HIGH);
	if (IS_ERR(aw3643_hwen_high)) {
		pr_err("Failed to init (%s)\n", AW3643_PINCTRL_STATE_HWEN_HIGH);
		ret = PTR_ERR(aw3643_hwen_high);
	}
	aw3643_hwen_low = pinctrl_lookup_state(aw3643_pinctrl,
		AW3643_PINCTRL_STATE_HWEN_LOW);
	if (IS_ERR(aw3643_hwen_low)) {
		pr_err("Failed to init (%s)\n", AW3643_PINCTRL_STATE_HWEN_LOW);
		ret = PTR_ERR(aw3643_hwen_low);
	}

	return ret;
}

static int aw3643_pinctrl_set(int pin, int state)
{
	int ret = 0;

	if (IS_ERR(aw3643_pinctrl)) {
		pr_err("pinctrl is not available\n");
		return -1;
	}

	switch (pin) {
	case AW3643_PINCTRL_PIN_HWEN:
		if (state == AW3643_PINCTRL_PINSTATE_LOW &&
			!IS_ERR(aw3643_hwen_low))
			pinctrl_select_state(aw3643_pinctrl, aw3643_hwen_low);
		else if (state == AW3643_PINCTRL_PINSTATE_HIGH &&
			!IS_ERR(aw3643_hwen_high))
			pinctrl_select_state(aw3643_pinctrl, aw3643_hwen_high);
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
 * aw3643 operations
 *****************************************************************************/
static const int aw3643_current[AW3643_LEVEL_NUM] = {
	22, 46, 70, 93, 116, 140, 163, 198, 245, 304,
	351, 398, 445, 503, 550, 597, 656, 703, 750, 796,
	855, 902, 949, 996, 1054, 1101
};

static const unsigned char aw3643_torch_level[AW3643_LEVEL_NUM] = {
	0x05, 0x0F, 0x12, 0x14, 0x17, 0x1D, 0x25, 0x1F, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char sy7806_torch_level[AW3643_LEVEL_NUM] = {
	0x0B, 0x20, 0x26, 0x2A, 0x31, 0x3D, 0x4E, 0x41, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char aw3643_flash_level[AW3643_LEVEL_NUM] = {
	0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x10, 0x14, 0x19,
	0x1D, 0x59, 0x25, 0x2A, 0x2E, 0x32, 0x37, 0x3B, 0x3F, 0x43,
	0x48, 0x4C, 0x50, 0x54, 0x59, 0x5D
};

static unsigned char aw3643_reg_enable;
static int aw3643_level_ch1 = -1;
static int aw3643_level_ch2 = -1;

static int aw3643_is_torch(int level)
{

	if (level >= AW3643_LEVEL_TORCH)
		return -1;
	return 0;
}

static int aw3643_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= AW3643_LEVEL_NUM)
		level = AW3643_LEVEL_NUM - 1;

	return level;
}

/* i2c wrapper function */
static int aw3643_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct aw3643_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);
	if (ret < 0)
		pr_err("failed writing at 0x%02x\n", reg);

	return ret;
}

static int aw3643_read_reg(struct i2c_client *client, u8 reg)
{
	int val = 0;
	struct aw3643_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	val = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	return val;
}

/* flashlight enable function */
static int aw3643_enable_ch1(void)
{
	unsigned char reg, val;

	reg = AW3643_REG_ENABLE;
	if (!aw3643_is_torch(aw3643_level_ch1))
		aw3643_reg_enable |= AW3643_ENABLE_LED1_TORCH;
	else
		aw3643_reg_enable |= AW3643_ENABLE_LED1_FLASH;

	val = aw3643_reg_enable;

	return aw3643_write_reg(aw3643_i2c_client, reg, val);
}

static int aw3643_enable_ch2(void)
{
	unsigned char reg, val;

	reg = AW3643_REG_ENABLE;
	if (!aw3643_is_torch(aw3643_level_ch2))
		aw3643_reg_enable |= AW3643_ENABLE_LED2_TORCH;
	else
		aw3643_reg_enable |= AW3643_ENABLE_LED2_FLASH;

	val = aw3643_reg_enable;

	return aw3643_write_reg(aw3643_i2c_client, reg, val);
}

static int aw3643_enable(int channel)
{
	if (channel == AW3643_CHANNEL_CH1) {
		aw3643_enable_ch1();
	} else if (channel == AW3643_CHANNEL_CH2) {
		aw3643_enable_ch2();
	} else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

/* flashlight disable function */
static int aw3643_disable_ch1(void)
{
	unsigned char reg, val;

	reg = AW3643_REG_ENABLE;
	if (aw3643_reg_enable & AW3643_MASK_ENABLE_LED2)
		aw3643_reg_enable &= (~AW3643_ENABLE_LED1);
	else
		aw3643_reg_enable &= (~AW3643_ENABLE_LED1_FLASH);

	val = aw3643_reg_enable;

	return aw3643_write_reg(aw3643_i2c_client, reg, val);
}

static int aw3643_disable_ch2(void)
{
	unsigned char reg, val;

	reg = AW3643_REG_ENABLE;
	if (aw3643_reg_enable & AW3643_MASK_ENABLE_LED1)
		aw3643_reg_enable &= (~AW3643_ENABLE_LED2);
	else
		aw3643_reg_enable &= (~AW3643_ENABLE_LED2_FLASH);

	val = aw3643_reg_enable;

	return aw3643_write_reg(aw3643_i2c_client, reg, val);
}

static int aw3643_disable(int channel)
{
	if (channel == AW3643_CHANNEL_CH1) {
		aw3643_disable_ch1();
	} else if (channel == AW3643_CHANNEL_CH2) {
		aw3643_disable_ch2();
	} else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

/* set flashlight level */
static int aw3643_set_level_ch1(int level)
{
	int ret;
	unsigned char reg, val;

	level = aw3643_verify_level(level);

	/* set torch brightness level */
	reg = AW3643_REG_TORCH_LEVEL_LED1;
	if (sy7806)
		val = sy7806_torch_level[level];
	else
		val = aw3643_torch_level[level];
	ret = aw3643_write_reg(aw3643_i2c_client, reg, val);

	aw3643_level_ch1 = level;

	/* set flash brightness level */
	reg = AW3643_REG_FLASH_LEVEL_LED1;
	val = aw3643_flash_level[level];
	ret = aw3643_write_reg(aw3643_i2c_client, reg, val);
	return ret;
}

static int aw3643_set_level_ch2(int level)
{
	int ret;
	unsigned char reg, val;

	level = aw3643_verify_level(level);

	/* set torch brightness level */
	reg = FLASHLIGHT_REG_TORCH_LEVEL_LED2;
	if (sy7806)
		val = sy7806_torch_level[level];
	else
		val = aw3643_torch_level[level];

	ret = aw3643_write_reg(aw3643_i2c_client, reg, val);

	aw3643_level_ch2 = level;

	/* set flash brightness level */
	reg = FLASHLIGHT_REG_FLASH_LEVEL_LED2;
	val = aw3643_flash_level[level];
	ret = aw3643_write_reg(aw3643_i2c_client, reg, val);

	return ret;
}

static int aw3643_set_level(int channel, int level)
{
	if (channel == AW3643_CHANNEL_CH1) {
		aw3643_set_level_ch1(level);
	} else if (channel == AW3643_CHANNEL_CH2) {
		aw3643_set_level_ch2(level);
	} else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

int aw3643_init(void)
{
	int ret;
	unsigned char reg, val;

	aw3643_pinctrl_set(AW3643_PINCTRL_PIN_HWEN, AW3643_PINCTRL_PINSTATE_HIGH);
	msleep(20);
	aw3643_write_reg(aw3643_i2c_client,
		FLASHLIGHT_REG_FLASH_LEVEL_LED2, FLASHLIGHT_SET_LED);
	aw3643_write_reg(aw3643_i2c_client,
		FLASHLIGHT_REG_TORCH_LEVEL_LED2, FLASHLIGHT_SET_LED);
	/* clear enable register */
	reg = AW3643_REG_ENABLE;
	val = AW3643_DISABLE;
	ret = aw3643_write_reg(aw3643_i2c_client, reg, val);

	aw3643_reg_enable = val;
	chip = aw3643_read_reg(aw3643_i2c_client, FLASHLIGHT_REG_DEVICE_ID);

	/* set torch current ramp time and flash timeout */
	reg = AW3643_REG_TIMING_CONF;
	if (chip == AW3643_DEVICE_ID) {
		pr_info("The chip is AW3643!!\n");
		val = AW3643_TORCH_RAMP_TIME | AW3643_FLASH_TIMEOUT;
	} else if (chip == SY7806_DEVICE_ID) {
		pr_info("The chip is SY7806!!\n");
		val = SY7806_TORCH_RAMP_TIME | SY7806_FLASH_TIMEOUT;
	} else {
		pr_err("The chip not find!!\n");
		return val;
	}
	ret = aw3643_write_reg(aw3643_i2c_client, reg, val);
	//get_torch_level_by_tp_info(chip);
	return ret;
}

/* flashlight uninit */
int aw3643_uninit(void)
{
	aw3643_disable(AW3643_CHANNEL_CH1);
	aw3643_disable(AW3643_CHANNEL_CH2);
	aw3643_pinctrl_set(AW3643_PINCTRL_PIN_HWEN, AW3643_PINCTRL_PINSTATE_LOW);

	return 0;
}


/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer aw3643_timer_ch1;
static struct hrtimer aw3643_timer_ch2;
static unsigned int aw3643_timeout_ms[AW3643_CHANNEL_NUM];

static void aw3643_work_disable_ch1(struct work_struct *data)
{
	//pr_debug("ht work queue callback\n");
	aw3643_disable_ch1();
}

static void aw3643_work_disable_ch2(struct work_struct *data)
{
	//pr_debug("lt work queue callback\n");
	aw3643_disable_ch2();
}

static enum hrtimer_restart aw3643_timer_func_ch1(struct hrtimer *timer)
{
	schedule_work(&aw3643_work_ch1);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart aw3643_timer_func_ch2(struct hrtimer *timer)
{
	schedule_work(&aw3643_work_ch2);
	return HRTIMER_NORESTART;
}

static int aw3643_timer_start(int channel, ktime_t ktime)
{
	if (channel == AW3643_CHANNEL_CH1) {
		hrtimer_start(&aw3643_timer_ch1, ktime, HRTIMER_MODE_REL);
	} else if (channel == AW3643_CHANNEL_CH2) {
		hrtimer_start(&aw3643_timer_ch2, ktime, HRTIMER_MODE_REL);
	} else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}

static int aw3643_timer_cancel(int channel)
{
	if (channel == AW3643_CHANNEL_CH1) {
		hrtimer_cancel(&aw3643_timer_ch1);
	} else if (channel == AW3643_CHANNEL_CH2) {
		hrtimer_cancel(&aw3643_timer_ch2);
	} else {
		pr_err("Error channel\n");
		return -1;
	}

	return 0;
}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int aw3643_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	ktime_t ktime;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	/* verify channel */
	if (channel < 0 || channel >= AW3643_CHANNEL_NUM) {
		pr_err("Failed with error channel\n");
		return -EINVAL;
	}

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		aw3643_timeout_ms[channel] = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		aw3643_set_level(channel, fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		if (fl_arg->arg == 1) {
			if (aw3643_timeout_ms[channel]) {
				ktime = ktime_set(aw3643_timeout_ms[channel] / 1000,
				(aw3643_timeout_ms[channel] % 1000) * 1000000);
				aw3643_timer_start(channel, ktime);
			}
			aw3643_enable(channel);
		} else {
			aw3643_disable(channel);
			aw3643_timer_cancel(channel);
		}
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		//pr_debug("FLASH_IOC_GET_DUTY_NUMBER(%d)\n", channel);
		fl_arg->arg = AW3643_LEVEL_NUM;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		//pr_debug("FLASH_IOC_GET_MAX_TORCH_DUTY(%d)\n", channel);
		fl_arg->arg = AW3643_LEVEL_TORCH - 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		fl_arg->arg = aw3643_verify_level(fl_arg->arg);
		fl_arg->arg = aw3643_current[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_TIMEOUT:
		//pr_debug("FLASH_IOC_GET_HW_TIMEOUT(%d)\n", channel);
		fl_arg->arg = AW3643_HW_TIMEOUT;
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int aw3643_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int aw3643_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int aw3643_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	mutex_lock(&aw3643_mutex);
	if (set) {
		if (!use_count)
			ret = aw3643_init();
		use_count++;
		//pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = aw3643_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&aw3643_mutex);

	return ret;
}

static ssize_t aw3643_strobe_store(struct flashlight_arg arg)
{
	aw3643_set_driver(1);
	aw3643_set_level(arg.channel, arg.level);
	aw3643_timeout_ms[arg.channel] = 0;
	aw3643_enable(arg.channel);
	msleep(arg.dur);
	aw3643_disable(arg.channel);
	aw3643_set_driver(0);

	return 0;
}

static struct flashlight_operations aw3643_ops = {
	aw3643_open,
	aw3643_release,
	aw3643_ioctl,
	aw3643_strobe_store,
	aw3643_set_driver
};

/******************************************************************************
 * I2C device and driver
 *****************************************************************************/
static int aw3643_chip_init(struct aw3643_chip_data *chip)
{
	return 0;
}

static int aw3643_parse_dt(struct device *dev,
		struct aw3643_platform_data *pdata)
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
	//pr_info("Channel number(%d).\n", pdata->channel_num);

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
		snprintf(pdata->dev_id[i].name, FLASHLIGHT_NAME_SIZE, AW3643_NAME);
		pdata->dev_id[i].channel = i;
		pdata->dev_id[i].decouple = decouple;

		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

static int aw3643_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct aw3643_platform_data *pdata = dev_get_platdata(&client->dev);
	struct aw3643_chip_data *chip = NULL;
	int err;
	int i;

	//pr_debug("Probe start.\n");

	/* check i2c */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}

	/* init chip private data */
	chip = kzalloc(sizeof(struct aw3643_chip_data), GFP_KERNEL);
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
		err = aw3643_parse_dt(&client->dev, pdata);
		if (err)
			goto err_free;
	}
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
	aw3643_i2c_client = client;

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	/* init work queue */
	INIT_WORK(&aw3643_work_ch1, aw3643_work_disable_ch1);
	INIT_WORK(&aw3643_work_ch2, aw3643_work_disable_ch2);

	/* init timer */
	hrtimer_init(&aw3643_timer_ch1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw3643_timer_ch1.function = aw3643_timer_func_ch1;
	hrtimer_init(&aw3643_timer_ch2, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw3643_timer_ch2.function = aw3643_timer_func_ch2;
	aw3643_timeout_ms[AW3643_CHANNEL_CH1] = 100;
	aw3643_timeout_ms[AW3643_CHANNEL_CH2] = 100;

	/* init chip hw */
	aw3643_chip_init(chip);

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(&pdata->dev_id[i],
				&aw3643_ops)) {
				err = -EFAULT;
				goto err_free;
			}
	} else {
		if (flashlight_dev_register(AW3643_NAME, &aw3643_ops)) {
			err = -EFAULT;
			goto err_free;
		}
	}

	//pr_debug("Probe done.\n");

	return 0;

err_free:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
err_out:
	return err;
}

static int aw3643_i2c_remove(struct i2c_client *client)
{
	struct aw3643_platform_data *pdata = dev_get_platdata(&client->dev);
	struct aw3643_chip_data *chip = i2c_get_clientdata(client);
	int i;

	//pr_debug("Remove start.\n");

	client->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(AW3643_NAME);

	/* flush work queue */
	flush_work(&aw3643_work_ch1);
	flush_work(&aw3643_work_ch2);

	/* free resource */
	kfree(chip);

	//pr_debug("Remove done.\n");

	return 0;
}

static const struct i2c_device_id aw3643_i2c_id[] = {
	{AW3643_NAME, 0},
	{ }
};

#ifdef CONFIG_OF
static const struct of_device_id aw3643_i2c_of_match[] = {
	{.compatible = AW3643_DTNAME_I2C},
	{ },
};
#endif

static struct i2c_driver aw3643_i2c_driver = {
	.driver = {
		.name = AW3643_NAME,
#ifdef CONFIG_OF
		.of_match_table = aw3643_i2c_of_match,
#endif
	},
	.probe = aw3643_i2c_probe,
	.remove = aw3643_i2c_remove,
	.id_table = aw3643_i2c_id,
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int aw3643_probe(struct platform_device *dev)
{
	//pr_debug("Probe start.\n");

	/* init pinctrl */
	if (aw3643_pinctrl_init(dev)) {
		pr_debug("Failed to init pinctrl.\n");
		return -1;
	}

	if (i2c_add_driver(&aw3643_i2c_driver)) {
		pr_debug("Failed to add i2c driver.\n");
		return -1;
	}

	//pr_debug("Probe done.\n");

	return 0;
}

static int aw3643_remove(struct platform_device *dev)
{
	//pr_debug("Remove start.\n");

	i2c_del_driver(&aw3643_i2c_driver);

	//pr_debug("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aw3643_of_match[] = {
	{.compatible = AW3643_DTNAME},
	{ },
};
MODULE_DEVICE_TABLE(of, aw3643_of_match);
#else
static struct platform_device aw3643_platform_device[] = {
	{
		.name = AW3643_NAME,
		.id = 0,
		.dev = {}
	},
	{ }
};
MODULE_DEVICE_TABLE(platform, aw3643_platform_device);
#endif

static struct platform_driver aw3643_platform_driver = {
	.probe = aw3643_probe,
	.remove = aw3643_remove,
	.driver = {
		.name = AW3643_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aw3643_of_match,
#endif
	},
};

static int __init flashlight_aw3643_init(void)
{
	int ret;

	//pr_debug("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&aw3643_platform_device);
	if (ret) {
		pr_err("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&aw3643_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}

	//pr_debug("Init done.\n");

	return 0;
}

static void __exit flashlight_aw3643_exit(void)
{
	//pr_debug("Exit start.\n");

	platform_driver_unregister(&aw3643_platform_driver);

	//pr_debug("Exit done.\n");
}

module_init(flashlight_aw3643_init);
module_exit(flashlight_aw3643_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simon Wang <Simon-TCH.Wang@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight AW3643 Driver");

