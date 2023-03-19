/*
 * flashlights-sgm3785.c
 *
 * driver for sgm3785
 *
 * Copyright (c) 2019-2019 Huawei Technologies Co., Ltd.
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
#include <linux/pinctrl/consumer.h>
#include "flashlight-core.h"
#include "flashlight-dt.h"
#include <mt-plat/mtk_pwm.h>

/* define device tree */
#define SGM3785_DTNAME "mediatek,flashlights_sgm3785"

/* Device name */
#define SGM3785_NAME "flashlights_sgm3785"

#define LEVEL_DEFAULT 8
#define LEVEL_MAX 11
#define SGM3785_TIMEOUT_TIME 100
#define SGM3785_PINCTRL_PINSTATE_LOW 0
#define SGM3785_PINCTRL_PINSTATE_HIGH 1
#define TIME_STEP_MS 1000
#define TIME_STEP_NS 1000000
#define FLASH_IOC_SET_ON 1

/* define mutex and work queue */
static DEFINE_MUTEX(sgm3785_gpio_mutex);
static struct work_struct sgm3785_gpio_work;
unsigned int current_level;

/* define pinctrl */
enum pin_type_t {
	SGM3785_ENM_GPIO_MODE,
	SGM3785_ENM_PWM_MODE,
	SGM3785_ENF_GPIO_MODE,
	SGM3785_ENF_PWM__MODE,
};

enum pin_type_t pin_type;

enum flashlight_mode_t {
	MODE_SHUTOFF = 0,
	MODE_TORCH,
	MODE_PREFLASH,
	MODE_FLASH,
	MODE_MAX,
};

enum flashlight_mode_t flashlight_mode;

struct pinctrl *sgm_gpio_pinctrl;
struct pinctrl_state *sgm_gpio_default;
struct pinctrl_state *sgm3785_enm_gpio_h;
struct pinctrl_state *sgm3785_enm_gpio_l;
struct pinctrl_state *sgm3785_enm_pwm_h;
struct pinctrl_state *sgm3785_enm_pwm_l;

struct pinctrl_state *sgm3785_enf_gpio_h;
struct pinctrl_state *sgm3785_enf_gpio_l;

struct pwm_spec_config flashlight_config = {
	.pwm_no = PWM_MIN,
	.mode = PWM_MODE_OLD,
	.clk_div = CLK_DIV32, // CLK_DIV16: 147.7kHz, CLK_DIV32: 73.8kHz
	.clk_src = PWM_CLK_OLD_MODE_BLOCK,
	.pmic_pad = false,
	.PWM_MODE_OLD_REGS.IDLE_VALUE = IDLE_FALSE,
	.PWM_MODE_OLD_REGS.GUARD_VALUE = GUARD_FALSE,
	.PWM_MODE_OLD_REGS.GDURATION = 0,
	.PWM_MODE_OLD_REGS.WAVE_NUM = 0,
	.PWM_MODE_OLD_REGS.DATA_WIDTH = LEVEL_MAX,
	.PWM_MODE_OLD_REGS.THRESH = 5,
};

/* define usage count */
static int use_count;

/* platform data */
struct sgm3785_gpio_platform_data {
	int channel_num;
	struct flashlight_device_id *dev_id;
};

/*
 * Pinctrl configuration
 */
static int sgm3785_gpio_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	sgm_gpio_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(sgm_gpio_pinctrl)) {
		pr_err("Failed to get flashlight pinctrl\n");
		ret = PTR_ERR(sgm_gpio_pinctrl);
		return ret;
	}

	/* get enm gpio mode */
	sgm3785_enm_gpio_h = pinctrl_lookup_state(sgm_gpio_pinctrl,
		"flashlight_enm_high");
	if (IS_ERR(sgm3785_enm_gpio_h)) {
		pr_err("Failed to init flashlight_enm_high\n");
		ret = PTR_ERR(sgm3785_enm_gpio_h);
		return ret;
	}

	sgm3785_enm_gpio_l = pinctrl_lookup_state(sgm_gpio_pinctrl,
		"flashlight_enm_low");
	if (IS_ERR(sgm3785_enm_gpio_l)) {
		pr_err("Failed to init flashlight_enm_low\n");
		ret = PTR_ERR(sgm3785_enm_gpio_l);
		return ret;
	}

	/* get enm pwm mode */
	sgm3785_enm_pwm_h = pinctrl_lookup_state(sgm_gpio_pinctrl,
		"flashlight_enm_pwm_high");
	if (IS_ERR(sgm3785_enm_pwm_h)) {
		pr_err("Failed to init flashlight_enm_pwm_high\n");
		ret = PTR_ERR(sgm3785_enm_pwm_h);
		return ret;
	}

	sgm3785_enm_pwm_l = pinctrl_lookup_state(sgm_gpio_pinctrl,
		"flashlight_enm_pwm_low");
	if (IS_ERR(sgm3785_enm_pwm_l)) {
		pr_err("Failed to init flashlight_enm_pwm_low\n");
		ret = PTR_ERR(sgm3785_enm_pwm_l);
		return ret;
	}

	/* get enf gpio mode */
	sgm3785_enf_gpio_h = pinctrl_lookup_state(sgm_gpio_pinctrl,
		"flashlight_enf_high");
	if (IS_ERR(sgm3785_enf_gpio_h)) {
		pr_err("Failed to init flashlight_enf_high\n");
		ret = PTR_ERR(sgm3785_enf_gpio_h);
		return ret;
	}

	sgm3785_enf_gpio_l = pinctrl_lookup_state(sgm_gpio_pinctrl,
		"flashlight_enf_low");
	if (IS_ERR(sgm3785_enf_gpio_l)) {
		pr_err("Failed to init flashlight_enf_low\n");
		ret = PTR_ERR(sgm3785_enf_gpio_l);
		return ret;
	}

	return ret;
}

static int sgm3785_gpio_pinctrl_set(int pin_type, int state)
{
	int ret = 0;

	pr_info("%s enter\n", __func__);

	if (IS_ERR(sgm_gpio_pinctrl)) {
		pr_err("pinctrl is not available\n");
		ret = -1;
		return ret;
	}

	switch (pin_type) {
	case SGM3785_ENM_GPIO_MODE:
		if (state == SGM3785_PINCTRL_PINSTATE_LOW &&
			!IS_ERR(sgm3785_enm_gpio_l)) {
			pinctrl_select_state(sgm_gpio_pinctrl,
				sgm3785_enm_gpio_l);
		} else if (state == SGM3785_PINCTRL_PINSTATE_HIGH &&
			!IS_ERR(sgm3785_enm_gpio_h)) {
			pinctrl_select_state(sgm_gpio_pinctrl,
				sgm3785_enm_gpio_h);
		} else {
			pr_err("pin = %d state = %d\n", pin_type, state);
			ret = -1;
		}
		break;
	case SGM3785_ENM_PWM_MODE:
		if (state == SGM3785_PINCTRL_PINSTATE_LOW &&
			!IS_ERR(sgm3785_enm_pwm_l)) {
			pinctrl_select_state(sgm_gpio_pinctrl,
				sgm3785_enm_pwm_l);
		} else if (state == SGM3785_PINCTRL_PINSTATE_HIGH &&
			!IS_ERR(sgm3785_enm_pwm_h)) {
			pinctrl_select_state(sgm_gpio_pinctrl,
				sgm3785_enm_pwm_h);
		} else {
			pr_err("pin = %d state = %d\n", pin_type, state);
			ret = -1;
		}
		break;
	case SGM3785_ENF_GPIO_MODE:
		if (state == SGM3785_PINCTRL_PINSTATE_LOW &&
			!IS_ERR(sgm3785_enf_gpio_l)) {
			pinctrl_select_state(sgm_gpio_pinctrl,
				sgm3785_enf_gpio_l);
		} else if (state == SGM3785_PINCTRL_PINSTATE_HIGH &&
			!IS_ERR(sgm3785_enf_gpio_h)) {
			pinctrl_select_state(sgm_gpio_pinctrl,
				sgm3785_enf_gpio_h);
		} else {
			pr_err("pin = %d state = %d\n", pin_type, state);
			ret = -1;
		}
		break;
	default:
		pr_err("pin = %d state = %d\n", pin_type, state);
		ret = -1;
		break;
	}
	pr_info("pin = %d state = %d\n", pin_type, state);
	return ret;
}

/*
 * sgm3785_set_torch_mode
 * flashlight enable function
 */
static int sgm3785_set_torch_mode(int level)
{
	int pin_type;
	int state;
	int ret;

	pr_info("torch_mode enter\n");

	pin_type = SGM3785_ENF_GPIO_MODE;
	state = SGM3785_PINCTRL_PINSTATE_LOW;
	ret = sgm3785_gpio_pinctrl_set(pin_type, state);

	pin_type = SGM3785_ENM_GPIO_MODE;
	state = SGM3785_PINCTRL_PINSTATE_HIGH;
	ret = sgm3785_gpio_pinctrl_set(pin_type, state);
	if (ret) {
		pr_err("%s failed\n", __func__);
		return ret;
	}
	mdelay(5);

	return ret;
}

/*
 * sgm3785_set_flash_mode
 * flashlight enable function
 */
static int sgm3785_set_flash_mode(int level)
{
	int pin_type;
	int state;
	int ret;

	pr_info("flash_mode enter\n");

	pin_type = SGM3785_ENM_PWM_MODE;
	state = SGM3785_PINCTRL_PINSTATE_HIGH;
	ret = sgm3785_gpio_pinctrl_set(pin_type, state);

	flashlight_config.PWM_MODE_OLD_REGS.THRESH = level;
	ret = pwm_set_spec_config(&flashlight_config);

	pin_type = SGM3785_ENF_GPIO_MODE;
	state = SGM3785_PINCTRL_PINSTATE_HIGH;
	ret = sgm3785_gpio_pinctrl_set(pin_type, state);

	return ret;
}

/* set flashlight level */
static int sgm3785_gpio_set_level(int level)
{
	pr_info("[flashlight] level = %d", level);

	if (level < 0) {
		pr_err("Get level failed\n");
		return -1;
	}

	if (level > LEVEL_MAX)
		level = LEVEL_MAX;

	current_level = level;
	pr_info("current level = %d", current_level);
	return 0;
}

/* flashlight disable function */
static int sgm3785_set_shutoff(void)
{
	int pin_type;
	int state;
	int ret;

	pr_info("%s enter\n", __func__);
	pin_type = SGM3785_ENM_GPIO_MODE;
	state = SGM3785_PINCTRL_PINSTATE_LOW;
	ret = sgm3785_gpio_pinctrl_set(pin_type, state);

	pin_type = SGM3785_ENF_GPIO_MODE;
	state = SGM3785_PINCTRL_PINSTATE_LOW;
	ret = sgm3785_gpio_pinctrl_set(pin_type, state);

	flashlight_mode = MODE_SHUTOFF;

	return ret;
}

/* flashlight init */
static int sgm3785_gpio_init(void)
{
	int ret;

	ret = sgm3785_set_shutoff();
	if (ret) {
		pr_err("%s failed\n", __func__);
		return ret;
	}
	return 0;
}

/* flashlight uninit */
static int sgm3785_gpio_uninit(void)
{
	return 0;
}

/*
 * Timer and work queue
 */
static struct hrtimer sgm3785_gpio_timer;
static unsigned int sgm3785_gpio_timeout_ms;


static void sgm3785_gpio_work_disable(struct work_struct *data)
{
	pr_debug("work queue callback\n");
	sgm3785_set_shutoff();
}

static enum hrtimer_restart sgm3785_gpio_timer_func(struct hrtimer *timer)
{
	schedule_work(&sgm3785_gpio_work);
	return HRTIMER_NORESTART;
}

static int sgm3785_gpio_ioctl_set_on_off(struct flashlight_dev_arg *fl_arg)
{
	int ret = 0;

	if (fl_arg->arg != FLASH_IOC_SET_ON) {
		pr_info("fl_arg->arg == %d", fl_arg->arg);
		sgm3785_set_shutoff();
		hrtimer_cancel(&sgm3785_gpio_timer);

		return ret;
	}

	if (current_level < 0) {
		pr_info("invalid current_level");
		return ret;
	} else if (current_level < 1) {
		flashlight_mode = MODE_TORCH;
	} else {
		flashlight_mode = MODE_FLASH;
	}

	switch (flashlight_mode) {
	case MODE_FLASH:
		sgm3785_set_flash_mode(current_level);
		break;
	case MODE_SHUTOFF:
	case MODE_PREFLASH:
	case MODE_TORCH:
		ret = sgm3785_set_torch_mode(current_level);
		if (ret) {
			pr_err("set torch mode failed\n");
			return ret;
		}
		break;
	default:
		break;
	}

	return ret;
}

/*
 * Flashlight operations
 */
static int sgm3785_gpio_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg = NULL;
	int channel;
	int ret;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	pr_info("cmd = %d,arg = %d\n", _IOC_NR(cmd), (int)fl_arg->arg);
	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		sgm3785_gpio_timeout_ms = fl_arg->arg;
		break;
	case FLASH_IOC_SET_DUTY:
		sgm3785_gpio_set_level(fl_arg->arg);
		break;
	case FLASH_IOC_GET_PRE_ON_TIME_MS:
		sgm3785_gpio_timeout_ms = fl_arg->arg;
		break;
	case FLASH_IOC_GET_PRE_ON_TIME_MS_DUTY:
		sgm3785_gpio_set_level(fl_arg->arg);
		break;
	case FLASH_IOC_SET_SCENARIO:
		break;
	case FLASH_IOC_SET_ONOFF:
		pr_info("current_level = %d, flashlight_mode = %d\n",
			current_level, flashlight_mode);

		ret = sgm3785_gpio_ioctl_set_on_off(fl_arg);
		if (ret)
			return ret;
		break;
	case FLASH_IOC_GET_HW_FAULT2:
		break;
	default:
		pr_info("No such command and arg: %d, %d, %d\n",
			channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int sgm3785_gpio_open(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int sgm3785_gpio_release(void)
{
	/* Move to set driver for saving power */
	return 0;
}

static int sgm3785_gpio_set_driver(int set)
{
	int ret = 0;
	/* set chip and usage count */
	pr_info("%s enter\n", __func__);
	mutex_lock(&sgm3785_gpio_mutex);
	if (set) {
		if (!use_count)
			ret = sgm3785_gpio_init();

		use_count++;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = sgm3785_gpio_uninit();
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	mutex_unlock(&sgm3785_gpio_mutex);

	return ret;
}

static ssize_t sgm3785_gpio_strobe_store(struct flashlight_arg arg)
{
	pr_info("%s enter\n", __func__);

	sgm3785_gpio_set_driver(1);
	sgm3785_gpio_set_level(arg.level);
	sgm3785_gpio_timeout_ms = 0;
	sgm3785_set_torch_mode(0);

	if (arg.dur < 0) {
		pr_err("The parameter is illegal\n");
		sgm3785_set_shutoff();
		return -1;
	}

	msleep(arg.dur);
	sgm3785_set_shutoff();
	sgm3785_gpio_set_driver(0);

	return 0;
}

static struct flashlight_operations sgm3785_gpio_ops = {
	sgm3785_gpio_open,
	sgm3785_gpio_release,
	sgm3785_gpio_ioctl,
	sgm3785_gpio_strobe_store,
	sgm3785_gpio_set_driver
};

/*
 * Platform device and driver
 */
static int sgm3785_gpio_chip_init(void)
{
	/*
	 * NOTE: Chip initialication move to "set driver" for power saving.
	 * sgm3785_gpio_init();
	 */
	return 0;
}

static int sgm3785_gpio_parse_dt(struct device *dev,
	struct sgm3785_gpio_platform_data *pdata)
{
	struct device_node *np = NULL;
	struct device_node *cnp = NULL;
	u32 decouple = 0;
	int i = 0;

	if (!dev || !dev->of_node || !pdata)
		return -ENODEV;

	np = dev->of_node;
	if (!np) {
		pr_err("The node does not exit\n");
		return -ENODEV;
	}

	pdata->channel_num = of_get_child_count(np);
	if (!pdata->channel_num) {
		pr_info("Parse no dt, node\n");
		return 0;
	}
	pr_info("Channel number %d\n", pdata->channel_num);

	if (of_property_read_u32(np, "decouple", &decouple))
		pr_info("Parse no dt, decouple\n");

	pdata->dev_id = devm_kzalloc(dev,
		pdata->channel_num *
		sizeof(struct flashlight_device_id),
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
		snprintf(pdata->dev_id[i].name, FLASHLIGHT_NAME_SIZE,
			SGM3785_NAME);
		pdata->dev_id[i].channel = i;
		pdata->dev_id[i].decouple = decouple;

		pr_info("Parse dt type,ct,part,name,channel,decouple = %d, %d, %d, %s, %d, %d\n",
			pdata->dev_id[i].type, pdata->dev_id[i].ct,
			pdata->dev_id[i].part, pdata->dev_id[i].name,
			pdata->dev_id[i].channel,
			pdata->dev_id[i].decouple);
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	devm_kfree(dev, pdata->dev_id);
	return -EINVAL;
}

static int sgm3785_gpio_probe(struct platform_device *pdev)
{
	struct sgm3785_gpio_platform_data *pdata = NULL;
	int ret;
	int i;

	pr_info("Probe start\n");

	pdata = dev_get_platdata(&pdev->dev);
	/* init pinctrl */
	if (sgm3785_gpio_pinctrl_init(pdev)) {
		pr_debug("Failed to init pinctrl\n");
		ret = -EFAULT;
		goto err;
	}

	/* init platform data */
	if (!pdata) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			ret = -ENOMEM;
			goto err;
		}
		pdev->dev.platform_data = pdata;
		ret = sgm3785_gpio_parse_dt(&pdev->dev, pdata);
		if (ret)
			goto err;
	}

	/* init work queue */
	INIT_WORK(&sgm3785_gpio_work, sgm3785_gpio_work_disable);

	/* init timer */
	hrtimer_init(&sgm3785_gpio_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sgm3785_gpio_timer.function = sgm3785_gpio_timer_func;
	sgm3785_gpio_timeout_ms = SGM3785_TIMEOUT_TIME;

	/* init chip hw */
	ret = sgm3785_gpio_chip_init();
	if (ret) {
		pr_err("sgm3785 chip init failed\n");
		goto err;
	}

	/* clear usage count */
	use_count = 0;

	/* register flashlight device */
	if (pdata->channel_num) {
		for (i = 0; i < pdata->channel_num; i++)
			if (flashlight_dev_register_by_device_id(
				&pdata->dev_id[i],
				&sgm3785_gpio_ops)) {
				ret = -EFAULT;
				goto err;
			}
	} else {
		if (flashlight_dev_register(SGM3785_NAME,
			&sgm3785_gpio_ops)) {
			ret = -EFAULT;
			goto err;
		}
	}

	pr_info("Probe done\n");
	return 0;

err:
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int sgm3785_gpio_remove(struct platform_device *pdev)
{
	struct sgm3785_gpio_platform_data *pdata = NULL;
	int i;

	pr_info("Remove start\n");

	pdata = dev_get_platdata(&pdev->dev);
	pdev->dev.platform_data = NULL;

	/* unregister flashlight device */
	if (pdata && pdata->channel_num)
		for (i = 0; i < pdata->channel_num; i++)
			flashlight_dev_unregister_by_device_id(
				&pdata->dev_id[i]);
	else
		flashlight_dev_unregister(SGM3785_NAME);

	/* flush work queue */
	flush_work(&sgm3785_gpio_work);

	pr_info("Remove done\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sgm3785_gpio_of_match[] = {
	{ .compatible = SGM3785_DTNAME },
	{},
};
MODULE_DEVICE_TABLE(of, sgm3785_gpio_of_match);
#else
static struct platform_device sgm3785_gpio_platform_device[] = {
	{
		.name = SGM3785_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, sgm3785_gpio_platform_device);
#endif

static struct platform_driver sgm3785_gpio_platform_driver = {
	.probe = sgm3785_gpio_probe,
	.remove = sgm3785_gpio_remove,
	.driver = {
		.name = SGM3785_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = sgm3785_gpio_of_match,
#endif
	},
};

static int __init flashlight_sgm3785_gpio_init(void)
{
	int ret;

	pr_debug("Init start\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&sgm3785_gpio_platform_device);
	if (ret) {
		pr_err("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&sgm3785_gpio_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}

	pr_debug("Init done\n");
	return 0;
}

static void __exit flashlight_sgm3785_gpio_exit(void)
{
	pr_debug("Exit start\n");

	platform_driver_unregister(&sgm3785_gpio_platform_driver);

	pr_debug("Exit done\n");
}

module_init(flashlight_sgm3785_gpio_init);
module_exit(flashlight_sgm3785_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huawei Technologies Co., Ltd.");
MODULE_DESCRIPTION("SGM3785 GPIO Driver");

