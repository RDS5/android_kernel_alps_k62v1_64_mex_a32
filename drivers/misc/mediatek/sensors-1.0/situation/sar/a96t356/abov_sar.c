/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2019. All rights reserved.
 * Description: ABOV SAR SENSOR module
 * Author: liming
 * Create: 2019-11-5
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/regulator/consumer.h>
#include <linux/usb.h>
#include <linux/power_supply.h>
#if defined(CONFIG_FB)
#include <linux/fb.h>
#endif
#include <asm/segment.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/async.h>
#include <linux/firmware.h>
#include <hwmsensor.h>
#include <situation.h>
#include "sensor_list.h"
#include "abov_sar.h"
#include "abov_sar_firmware.h"
#include "sar_factory.h"

#include <securec.h>

#define DRIVER_NAME "abov_sar"
#define LOG_TAG     "ABOV "

static u8 checksum_h;
static u8 checksum_h_bin;
static u8 checksum_l;
static u8 checksum_l_bin;
static struct abov_dev_t *abov_sar_ptr;
int32_t value2[ABOV_DATA_SIZE] = {0};
static int fw_update_status;

struct abov_fw_tmp_var {
	int i;
	u8 fw_version;
	u8 fw_modelno;
	u8 fw_file_version;
	u8 fw_file_modeno;
};

/* Define  Registers default value */
static struct abov_reg_data abov_i2c_reg_setup[] = {
	{
		.reg = 0x09,
		.val = 0x00, /* re-calibration */
	},
};

static int write_register(struct abov_dev_t *pdev, u8 address, u8 value)
{
	struct i2c_client *i2c = NULL;
	char buffer[ABOV_REG_VAL_BUF_SIZE];
	int ret;
	int i;

	buffer[0] = address;
	buffer[1] = value;

	if ((pdev == NULL) || (pdev->bus == NULL)) {
		pr_err("%s, input pointer is null\n", __func__);
		return -EINVAL;
	}

	i2c = pdev->bus;
	for (i = 0; i < ABOV_TRY_TIMES; i++) {
		ret = i2c_master_send(i2c, buffer, ABOV_REG_VAL_BUF_SIZE);
		if (ret != ABOV_REG_VAL_BUF_SIZE) {
			pr_err("%s: failed,ABOV_RETRY_TIMES %d\n", __func__, i);
			mdelay(2); // retry for 2ms
		} else {
			break;
		}
	}
	pr_info("%s Addr: 0x%02x Val: 0x%02x Return: %d\n",
		__func__, address, value, ret);

	return ret;
}

static void read_register(struct abov_dev_t *pdev, u8 address, u8 *value)
{
	struct i2c_client *i2c = NULL;
	int ret;

	if ((pdev == NULL) || (value == NULL) || (pdev->bus == NULL)) {
		pr_err("%s, input pointer is null\n", __func__);
		return;
	}

	i2c = pdev->bus;
	ret = i2c_smbus_read_byte_data(i2c, address);
	pr_info("%s Addr: 0x%02x Return: 0x%02x\n", __func__, address, ret);
	if (ret >= 0) {
		*value = ret;
		return;
	}

	pr_err("%s failed\n", __func__);
}

static int abov_device_detect(struct i2c_client *client)
{
	int ret;
	unsigned int i;
	u8 address = ABOV_VENDOR_ID_REG;
	u8 value = 0xAB;

	for (i = 0; i < ABOV_TRY_TIMES; i++) {
		ret = i2c_smbus_read_byte_data(client, address);
		pr_info("%s: %d time Addr: 0x%02x Return: 0x%02x\n",
			__func__, i, address, ret);
		if (value == ret) {
			pr_info("%s: detect succ\n", __func__);
			return ABOV_CHIP_DETECTED;
		}
	}

	for (i = 0; i < ABOV_TRY_TIMES; i++) {
		if (abov_tk_fw_mode_enter(client) == 0) {
			pr_info("%s boot detect succ\n", __func__);
			return ABOV_CHIP_IS_PRELOAD;
		}
	}

	pr_info("%s: chip detect failed\n", __func__);
	return 0;
}

static int manual_offset_calibration(struct abov_dev_t *pdev)
{
	pr_info("Enter function: %s\n", __func__);

	if (pdev == NULL) {
		pr_err("%s, input pointer is null\n", __func__);
		return -EINVAL;
	}
	return write_register(pdev, ABOV_RECALI_REG, 0x01); /* 0x01:calibrate */
}

static ssize_t manual_offset_calibration_show(
	struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u8 reg_value = 0;
	struct abov_dev_t *pdev = NULL;

	if ((dev == NULL) || (buf == NULL)) {
		pr_err("%s: dev is null\n", __func__);
		return -EINVAL;
	}

	pdev = dev_get_drvdata(dev);
	if (pdev == NULL) {
		pr_err("%s: pdev is null\n", __func__);
		return -EINVAL;
	}

	read_register(pdev, ABOV_IRQSTAT_REG, &reg_value);
	pr_info("%s: Reading IRQSTAT_REG: %d\n", __func__, reg_value);

	return scnprintf(buf, PAGE_SIZE, "0x%02x\n", reg_value);
}

static ssize_t manual_offset_calibration_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct abov_dev_t *pdev = NULL;
	unsigned long val;
	int ret;

	if ((dev == NULL) || (buf == NULL)) {
		pr_err("%s: dev is null\n", __func__);
		return -EINVAL;
	}

	pdev = dev_get_drvdata(dev);
	if (pdev == NULL) {
		pr_err("%s: pdev is null\n", __func__);
		return -EINVAL;
	}

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;
	if (val != 0) {
		ret = manual_offset_calibration(pdev);
		pr_info("%s: ret is %d\n", __func__, ret);
	}
	return count;
}

static DEVICE_ATTR(calibrate, 0644, manual_offset_calibration_show,
	manual_offset_calibration_store);

static struct attribute *abov_attributes[] = {
	&dev_attr_calibrate.attr,
	NULL,
};

static struct attribute_group abov_attr_group = {
	.attrs = abov_attributes,
};

static int read_reg_stat(struct abov_dev_t *pdev)
{
	u8 data = 0;

	if (pdev == NULL) {
		pr_err("%s, input pointer is null\n", __func__);
		return -EINVAL;
	}

	read_register(pdev, ABOV_IRQSTAT_REG, &data);
	return (data & ABOV_CH0_CH1_MASK);
}

/*
 * Description: Initialize the device register used dts config
 */
static int hw_init(struct abov_dev_t *pdev)
{
	struct abov_plat *pdevice = NULL;
	struct abov_platform_data *pdata = NULL;
	int i = 0;
	int ret = 0;

	pr_info("%s\n", __func__);
	if ((pdev == NULL) || (pdev->pdevice == NULL) ||
		(((struct abov_plat *)pdev->pdevice)->hw == NULL)) {
		pr_err("%s: invalid paramter\n", __func__);
		return -EINVAL;
	}

	pdevice = pdev->pdevice;
	pdata = pdevice->hw;

	while (i < pdata->i2c_reg_num) {
		pr_info("Going to Write Reg: 0x%02x Value: 0x%02x\n",
			pdata->pi2c_reg[i].reg, pdata->pi2c_reg[i].val);
		ret = write_register(pdev, pdata->pi2c_reg[i].reg,
			pdata->pi2c_reg[i].val);
		if (ret < 0) {
			pr_err("%s: write reg fail, ret %d\n", __func__, ret);
			return ret;
		}
		i++;
	}

	pr_info("Exit function: %s\n", __func__);
	return ret;
}

static int abov_device_init(struct abov_dev_t *pdev)
{
	int ret;

	pr_info("Enter function %s\n", __func__);

	if (pdev == NULL) {
		pr_err("%s, input pointer is null\n", __func__);
		return -EINVAL;
	}

	/* prepare reset by disabling any irq handling */
	pdev->irq_disabled = ABOV_IRQ_DISABLE;
	disable_irq(pdev->irq);

	ret = hw_init(pdev);
	if (ret < 0) {
		pr_err("%s: hw_init fail, ret %d\n", __func__, ret);
		enable_irq(pdev->irq);
		pdev->irq_disabled = ABOV_IRQ_ENABLE;
		return ret;
	}
	mdelay(300); /* make sure everything is running */
	if (fw_update_status == ABOV_FW_UPDATE_SUCCESS) {
		write_register(pdev, abov_i2c_reg_setup[0].reg, abov_i2c_reg_setup[0].val);
		// after fw update need delay 50ms
		mdelay(50);
	}

	/* re-enable interrupt handling */
	enable_irq(pdev->irq);
	pdev->irq_disabled = ABOV_IRQ_ENABLE;
	ret = read_reg_stat(pdev);
	pr_info("Exiting %s, ret = %d\n", __func__, ret);
	return 0;
}

static void sar_data_process(struct abov_dev_t *pdev)
{
	int ret;
	u8 irq_reg_val = 0;
	u8 diff_offset_buf[ABOV_DIF_OFF_BUF_SIZE] = {0};
	u16 diff_reg_val;
	u16 offset_reg_val;
	int32_t value[ABOV_DATA_SIZE] = {0};
	struct abov_platform_data *board = NULL;

	board = pdev->board;
	if ((pdev == NULL) || (board == NULL)) {
		pr_err("%s, board is null\n", __func__);
		return;
	}

	read_register(pdev, ABOV_IRQSTAT_REG, &irq_reg_val);
	read_register(pdev, ABOV_CH0_DIFF_MSB_REG, &diff_offset_buf[0]);
	read_register(pdev, ABOV_CH0_DIFF_LSB_REG, &diff_offset_buf[1]);
	diff_reg_val =
		(diff_offset_buf[0] << ABOV_SHIFT_8_BIT) | diff_offset_buf[1];

	read_register(pdev, ABOV_CH0_CAP_MSB_REG, &diff_offset_buf[0]);
	read_register(pdev, ABOV_CH0_CAP_LSB_REG, &diff_offset_buf[1]);
	offset_reg_val =
		(diff_offset_buf[0] << ABOV_SHIFT_8_BIT) | diff_offset_buf[1];

	value[0] = irq_reg_val & ABOV_CH0_CH1_MASK;
	value[1] = diff_reg_val & ABOV_FOUR_BYTE_MASK;
	value[2] = offset_reg_val & ABOV_FOUR_BYTE_MASK;
	pr_err("%s:CH0 state: %d, diff: %d, offset: %d\n",
		__func__, value[0], value[1], value[2]);

	read_register(pdev, ABOV_IRQSTAT_REG, &irq_reg_val);
	read_register(pdev, ABOV_CH1_DIFF_MSB_REG, &diff_offset_buf[0]);
	read_register(pdev, ABOV_CH1_DIFF_LSB_REG, &diff_offset_buf[1]);
	diff_reg_val =
		(diff_offset_buf[0] << ABOV_SHIFT_8_BIT) | diff_offset_buf[1];

	read_register(pdev, ABOV_CH1_CAP_MSB_REG, &diff_offset_buf[0]);
	read_register(pdev, ABOV_CH1_CAP_LSB_REG, &diff_offset_buf[1]);
	offset_reg_val =
		(diff_offset_buf[0] << ABOV_SHIFT_8_BIT) | diff_offset_buf[1];

	value2[0] = irq_reg_val & ABOV_CH0_CH1_MASK;
	value2[1] = diff_reg_val & ABOV_FOUR_BYTE_MASK;
	value2[2] = offset_reg_val & ABOV_FOUR_BYTE_MASK;
	pr_err("%s:CH1 state: %d, diff: %d, offset: %d\n",
		__func__, value2[0], value2[1], value2[2]);

	ret = sar_data_report(value);
	if (ret != 0)
		pr_err("%s: report data fail\n", __func__);

	pr_info("Exit %s\n", __func__);
}

/*
 * Description: read the config from dts, and set
 * all the config to abov_i2c_reg_setup pointer
 */
static int abov_reg_setup_init(struct i2c_client *client)
{
	u32 array_len;
	int ret;
	int i, j;
	u32 *data_array = NULL;
	struct device_node *np = client->dev.of_node;

	if (np == NULL) {
		pr_err("%s, np is null\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "reg_array_len", &array_len);
	if (ret < 0) {
		pr_err("%s: array_len read error", __func__);
		return -EINVAL;
	}
	pr_info("%s: array_len = %d\n", __func__, array_len);

	data_array = kzalloc(array_len *
		ABOV_REG_VAL_BUF_SIZE * sizeof(u32),
		GFP_KERNEL);
	if (data_array == NULL) {
		pr_err("%s: data_array alloc fail\n", __func__);
		return -ENOMEM;
	}

	ret = of_property_read_u32_array(np, "reg_array_val", data_array,
		array_len * ABOV_REG_VAL_BUF_SIZE);
	if (ret < 0) {
		pr_err("%s: data read error", __func__);
		kfree(data_array);
		return -EINVAL;
	}
	for (i = 0; i < ARRAY_SIZE(abov_i2c_reg_setup); i++) {
		for (j = 0; j < array_len * ABOV_REG_VAL_BUF_SIZE;
			j += ABOV_REG_VAL_BUF_SIZE) {
			pr_info("read dtsi 0x%02x:0x%02x set reg\n",
				data_array[j], data_array[j + 1]);
			if (data_array[j] == abov_i2c_reg_setup[i].reg)
				abov_i2c_reg_setup[i].val = data_array[j + 1];
		}
	}
	kfree(data_array);
	return ret;
}

static int abov_plat_data_init_from_dts(
	struct i2c_client *client,
	struct abov_platform_data *ppdata)
{
	int ret;
	struct device_node *np = client->dev.of_node;

	np = of_find_compatible_node(NULL, NULL, "abov,abov_sar");
	if (np == NULL) {
		pr_err("%s: can't found the abov dts config\n", __func__);
		return -EINVAL;
	}

	ppdata->irq_gpio = of_get_named_gpio(np, "sar_int", 0);
	if (ppdata->irq_gpio < 0) {
		pr_err("%s read sar_int fail\n", __func__);
		return -EINVAL;
	}
	client->irq = ppdata->irq_gpio;
	pr_info("%s: irq = %d\n", __func__, client->irq);

	ret = gpio_request(ppdata->irq_gpio, "sar_int");
	if (ret != 0) {
		pr_err("%s: gpio request err %d\n", __func__, ret);
		return ret;
	}

	ret = gpio_direction_input(ppdata->irq_gpio);
	if (ret != 0) {
		pr_err("%s: gpio_direction_input err %d\n", __func__, ret);
		gpio_free(ppdata->irq_gpio);
		return ret;
	}

	ppdata->get_is_nirq_low = NULL;
	ppdata->init_platform_hw = NULL;
	ppdata->exit_platform_hw = NULL;
	ret = abov_reg_setup_init(client);
	if (ret < 0) {
		pr_err("%s: read dts config fail, ret %d\n", __func__, ret);
		gpio_free(ppdata->irq_gpio);
		return ret;
	}

	ppdata->pi2c_reg = abov_i2c_reg_setup;
	ppdata->i2c_reg_num = ARRAY_SIZE(abov_i2c_reg_setup);
	ret = of_property_read_string(np, "label", &ppdata->fw_name);
	if (ret < 0) {
		pr_err("firmware name read error\n");
		gpio_free(ppdata->irq_gpio);
		return ret;
	}
	pr_info("%s: fw_name = %s\n", __func__, ppdata->fw_name);
	return 0;
}

static ssize_t reg_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	int i;
	int len = 0;
	u8 reg_value = 0;
	char *p = buf;

	if (buf == NULL) {
		pr_err("%s\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < 0x26; i++) {
		read_register(abov_sar_ptr, i, &reg_value);
		len += snprintf_s(p + len, PAGE_SIZE - len,
			PAGE_SIZE - len - 1,
			"(0x%02x)=0x%02x\n",
			i, reg_value);
	}

	for (i = 0x80; i < 0x8C; i++) {
		read_register(abov_sar_ptr, i, &reg_value);
		len += snprintf_s(p + len, PAGE_SIZE - len,
			PAGE_SIZE - len - 1,
			"(0x%02x)=0x%02x\n",
			i, reg_value);
	}

	return len;
}

static ssize_t reg_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	unsigned int val;
	int reg;

	if (sscanf_s(buf, "%x,%x", &reg, &val) == ABOV_TWO_BYTE) {
		u8 reg_char = (char)(reg & ABOV_CHAR_MASK);
		u8 val_char = (char)(val & ABOV_CHAR_MASK);

		pr_info("%s,reg = 0x%02x, val = 0x%02x\n",
			__func__, reg_char, val_char);
		write_register(abov_sar_ptr, reg_char, val_char);
	}

	return count;
}

static CLASS_ATTR_RW(reg);

static ssize_t abov_info_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	unsigned char info_name[] = "A96T456HF";

	if (buf == NULL) {
		pr_err("%s, buf is NULL!\n", __func__);
		return -1;
	}
	return snprintf_s(buf, PAGE_SIZE, PAGE_SIZE - 1, "%s\n", info_name);
}

static CLASS_ATTR_RO(abov_info);

static struct class capsense_class = {
	.name  = "capsense",
	.owner = THIS_MODULE,
};

static int _i2c_adapter_block_write(
	struct i2c_client *client,
	u8 *data,
	u8 len)
{
	u8 buffer[C_I2C_FIFO_SIZE] = {0};
	u8 left = len;
	u8 offset = 0;
	u8 retry;

	struct i2c_msg msg = {
		.addr = client->addr & I2C_MASK_FLAG,
		.flags = 0,
		.buf = buffer,
	};

	if ((data == NULL) || (len < ABOV_ONE_BYTE)) {
		pr_err("%s: data is null or len=%d\n", __func__, len);
		return -EINVAL;
	}

	while (left > 0) {
		retry = 0;
		if (left >= C_I2C_FIFO_SIZE) {
			msg.buf = &data[offset];
			msg.len = C_I2C_FIFO_SIZE;
			left -= C_I2C_FIFO_SIZE;
			offset += C_I2C_FIFO_SIZE;
		} else {
			msg.buf = &data[offset];
			msg.len = left;
			left = 0;
		}

		while (i2c_transfer(client->adapter, &msg, ABOV_ONE_BYTE) !=
			ABOV_ONE_BYTE) {
			retry++;
			if (retry > ABOV_RETRY_TIMES) {
				pr_err("%s: fail - addr:%#x len:%d\n",
					__func__, client->addr, msg.len);
				return -EIO;
			}
		}
	}
	return 0;
}

static int abov_tk_check_busy(struct i2c_client *client)
{
	int ret;
	int count = 0;
	unsigned char val = 0;

	do {
		ret = i2c_master_recv(client, &val, sizeof(val));
		if ((ret != sizeof(val)) || ((val & 0x01) != 0)) {
			pr_err("%s: read i2c error, ret %d\n", __func__, ret);
			count++;
			if (count > 1000)
				return ret;
		} else {
			pr_info("%s: break the loop, ret %d\n",
				__func__, ret);
			break;
		}
	} while (1);

	return ret;
}

static int abov_tk_fw_write(
	struct i2c_client *client,
	unsigned char *addr_high,
	unsigned char *addr_low,
	unsigned char *val,
	unsigned int val_len)
{
	int ret;
	unsigned char buf[ABOV_FW_BUF_SIZE] = {0};

	buf[0] = 0xAC;
	buf[1] = 0x7A;
	buf[2] = *addr_high;
	buf[3] = *addr_low;
	ret = memcpy_s(&buf[4], ABOV_FW_BUF_SIZE - 4, val, val_len);
	if (ret != EOK)
		pr_err("[%s-%d]memcpy_s failed\n", __func__, __LINE__);

	ret = _i2c_adapter_block_write(client, buf, ABOV_FW_BUF_SIZE);
	if (ret < 0) {
		pr_err("%s: Firmware write fail\n", __func__);
		return ret;
	}
	// after tk fw write, need delay 3ms
	mdelay(3);
	ret = abov_tk_check_busy(client);
	if (ret < 0) {
		pr_err("%s: deivce is busy, ret %d\n", __func__, ret);
		return ret;
	}
	return 0;
}

static int abov_tk_reset_for_bootmode(struct i2c_client *client)
{
	int ret;
	unsigned char buf[ABOV_REG_VAL_BUF_SIZE];
	int i;

	for (i = 0; i < ABOV_RETRY_TIMES; i++) {
		buf[0] = 0xF0;
		buf[1] = 0xAA;
		ret = i2c_master_send(client, buf, ABOV_REG_VAL_BUF_SIZE);
		if (ret != ABOV_REG_VAL_BUF_SIZE)
			pr_info("%s: write fail, retry:%d\n",
				__func__, i);
		else
			break;
	}

	if ((i == ABOV_RETRY_TIMES) && (ret < 0)) {
		pr_err("%s: write always fail %d\n", __func__, ret);
		return -EIO;
	}

	pr_info("success reset & boot mode\n");
	return 0;
}

static int abov_tk_fw_mode_enter(struct i2c_client *client)
{
	int ret;
	unsigned char buf[ABOV_REG_VAL_BUF_SIZE];

	buf[0] = 0xAC;
	buf[1] = 0x5B;
	ret = i2c_master_send(client, buf, ABOV_REG_VAL_BUF_SIZE);
	if (ret != ABOV_REG_VAL_BUF_SIZE) {
		pr_err("%s: fail - addr:%#x data:%#x %#x, ret:%d\n",
			__func__, client->addr, buf[0], buf[1], ret);
		return -EIO;
	}
	pr_info("%s: succ - addr:%#x data:%#x %#x, ret:%d\n",
		__func__, client->addr, buf[0], buf[1], ret);

	ret = i2c_master_recv(client, buf, ABOV_ONE_BYTE);
	if (ret != ABOV_ONE_BYTE) {
		pr_err("%s: Enter fw downloader mode fail\n", __func__);
		return -EIO;
	}

	pr_info("%s: succ, device id=0x%#x\n", __func__, buf[0]);
	return 0;
}

static int abov_tk_fw_mode_exit(struct i2c_client *client)
{
	int ret;
	unsigned char buf[ABOV_REG_VAL_BUF_SIZE];

	buf[0] = 0xAC;
	buf[1] = 0xE1;
	ret = i2c_master_send(client, buf, ABOV_REG_VAL_BUF_SIZE);
	if (ret != ABOV_REG_VAL_BUF_SIZE) {
		pr_err("%s: fail - addr:%#x data:%#x %#x ret:%d\n",
			__func__, client->addr, buf[0], buf[1], ret);
		return -EIO;
	}
	pr_info("%s: succ - addr:%#x data:%#x %#x ret:%d\n",
		__func__, client->addr, buf[0], buf[1], ret);
	return 0;
}

static int abov_tk_flash_erase(struct i2c_client *client)
{
	int ret;
	unsigned char buf[ABOV_REG_VAL_BUF_SIZE];

	buf[0] = 0xAC;
	buf[1] = 0x2E;

	ret = i2c_master_send(client, buf, ABOV_REG_VAL_BUF_SIZE);
	if (ret != ABOV_REG_VAL_BUF_SIZE) {
		pr_err("%s: fail - addr:%#x data:%#x %#x ret:%d\n",
			__func__, client->addr, buf[0], buf[1], ret);
		return -EIO;
	}

	pr_info("%s: succ - addr:%#x data:%#x %#x ret:%d\n",
		__func__, client->addr, buf[0], buf[1], ret);
	return 0;
}

static int abov_tk_i2c_read_checksum(struct i2c_client *client)
{
	unsigned char checksum[ABOV_CHECK_SUM_LEN] = {0};
	unsigned char buf[ABOV_CHECK_SUM_LEN];
	int ret;

	checksum_h = 0;
	checksum_l = 0;

	buf[0] = 0xAC;
	buf[1] = 0x9E;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = checksum_h_bin;
	buf[5] = checksum_l_bin;

	ret = i2c_master_send(client, buf, ABOV_CHECK_SUM_LEN);
	if (ret != ABOV_CHECK_SUM_LEN) {
		pr_err("%s: fail - addr:%#x len:%d ret:%d\n",
			__func__, client->addr, ABOV_CHECK_SUM_LEN, ret);
		return -EIO;
	}
	mdelay(ABOV_SLEEP_5_MS);

	buf[0] = 0x00;
	ret = i2c_master_send(client, buf, ABOV_ONE_BYTE);
	if (ret != ABOV_ONE_BYTE) {
		pr_err("%s: fail - addr:%#x data:%#x ret:%d\n",
			__func__, client->addr, buf[0], ret);
		return -EIO;
	}
	mdelay(ABOV_SLEEP_5_MS);

	ret = i2c_master_recv(client, checksum, ABOV_CHECK_SUM_LEN);
	if (ret != ABOV_CHECK_SUM_LEN) {
		pr_err("%s: Read raw fail\n", __func__);
		return -EIO;
	}

	checksum_h = checksum[4];
	checksum_l = checksum[5];
	return 0;
}

static int abov_set_chip_fw_mode(
	struct i2c_client *client)
{
	int i;

	if (abov_tk_reset_for_bootmode(client) < 0) {
		pr_err("%s: don't reset, enter boot mode", __func__);
		return -EIO;
	}
	/*
	 * Master IC has to send the I2C protocol
	 * within as 40ms ~ 50ms to enter fw download mode
	 */
	mdelay(45);
	for (i = 0; i < ABOV_RETRY_TIMES; i++) {
		if (abov_tk_fw_mode_enter(client) < 0) {
			pr_err("%s: don't enter the download mode %d",
				__func__, i);
			continue;
		}
		break;
	}

	if (i >= ABOV_RETRY_TIMES) {
		pr_err("%s: retry fail\n", __func__);
		return -EAGAIN;
	}

	if (abov_tk_flash_erase(client) < 0) {
		pr_err("%s: don't erase flash data", __func__);
		return -EIO;
	}

	return 0;
}

static int _abov_fw_update(
	struct i2c_client *client,
	const u8 *image, u32 size)
{
	int ret;
	int i;
	int count;
	unsigned short address;
	unsigned char addr_high;
	unsigned char addr_low;
	unsigned char data[ABOV_FW_UPDATE_SIZE] = {0};

	pr_info("Enter %s\n", __func__);

	if ((size % ABOV_FW_UPDATE_SIZE) != 0) {
		pr_err("%s: firmware invalid", __func__);
		return -EIO;
	}

	ret = abov_set_chip_fw_mode(client);
	if (ret < 0)
		return ret;

	// after erase the flash need delay 1400ms
	mdelay(1400);
	address = 0x800;  /* Flash addr of ABOV chip */
	count = size / ABOV_FW_UPDATE_SIZE;

	for (i = 0; i < count; i++) {
		/* first 32byte is header */
		addr_high =
			(unsigned char)((address >> ABOV_SHIFT_8_BIT) & 0xFF);
		addr_low = (unsigned char)(address & 0xFF);
		memcpy_s(data, ABOV_FW_UPDATE_SIZE,
			&image[i * ABOV_FW_UPDATE_SIZE],
			ABOV_FW_UPDATE_SIZE);
		ret = abov_tk_fw_write(client, &addr_high, &addr_low, data,
			ABOV_FW_UPDATE_SIZE);
		if (ret < 0) {
			pr_info("%s: fw_write i = 0x%x err\n", __func__, i);
			return ret;
		}

		address += ABOV_FW_UPDATE_SIZE;
		memset_s(data, ABOV_FW_UPDATE_SIZE, 0, ABOV_FW_UPDATE_SIZE);
	}

	ret = abov_tk_i2c_read_checksum(client);
	ret = abov_tk_fw_mode_exit(client);
	if ((checksum_h == checksum_h_bin) &&
		(checksum_l == checksum_l_bin)) {
		pr_info("succ, sum_h:0x%02x=0x%02x sum_l:0x%02x=0x%02x\n",
			checksum_h, checksum_h_bin, checksum_l, checksum_l_bin);
	} else {
		pr_info("error, sum_h:0x%02x!=0x%02x sum_l:0x%02x!=0x%02x\n",
			checksum_h, checksum_h_bin, checksum_l, checksum_l_bin);
		ret = -EINVAL;
	}
	return ret;
}

static int abov_fw_update(bool force)
{
	int ret;
	bool fw_upgrade = false;
	bool fw_slect = true;
	const struct firmware *fw = NULL;
	char fw_name[ABOV_FW_NAME_SIZE] = {0};
	struct i2c_client *client = abov_sar_ptr->bus;
	struct abov_fw_tmp_var tmp;

	pr_info("Enter function: %s\n", __func__);

	strlcpy(fw_name, abov_sar_ptr->board->fw_name, ABOV_FW_NAME_SIZE);
	strlcat(fw_name, ".BIN", ABOV_FW_NAME_SIZE);

	if (!force) {
		read_register(abov_sar_ptr, ABOV_VERSION_REG, &tmp.fw_version);
		read_register(abov_sar_ptr, ABOV_MODELNO_REG, &tmp.fw_modelno);
	}
	if (!fw_slect) {
		tmp.fw_file_modeno = fw->data[ABOV_MODENO_INDEX];
		tmp.fw_file_version = fw->data[ABOV_VERSION_INDEX];
		checksum_h_bin = fw->data[ABOV_CK_H_INDEX];
		checksum_l_bin = fw->data[ABOV_CK_L_INDEX];
	} else {
		tmp.fw_file_modeno = abov_fw[ABOV_MODENO_INDEX];
		tmp.fw_file_version = abov_fw[ABOV_VERSION_INDEX];
		checksum_h_bin = abov_fw[ABOV_CK_H_INDEX];
		checksum_l_bin = abov_fw[ABOV_CK_L_INDEX];
	}

	if ((force) ||
		(tmp.fw_version != tmp.fw_file_version) ||
		(tmp.fw_modelno != tmp.fw_file_modeno)) {
		fw_upgrade = true;
		fw_update_status = ABOV_FW_UPDATE_FAILED;
	} else {
		pr_info("%s: Exiting fw upgrade\n", __func__);
		fw_upgrade = false;
		fw_update_status = ABOV_FW_UPDATE_FAILED;
		ret = -EIO;
		goto rel_fw;
	}

	if (fw_upgrade) {
		for (tmp.i = 0; tmp.i < ABOV_RETRY_TIMES; tmp.i++) {
			if (fw_slect == false)
				ret = _abov_fw_update(client,
					&fw->data[ABOV_FW_UPDATE_SIZE],
					fw->size - ABOV_FW_UPDATE_SIZE);
			else
				ret = _abov_fw_update(client,
					&abov_fw[ABOV_FW_UPDATE_SIZE],
					sizeof(abov_fw) - ABOV_FW_UPDATE_SIZE);
			if (ret < 0) {
				pr_info("retry : %d times\n", tmp.i);
			} else {
				fw_update_status = ABOV_FW_UPDATE_SUCCESS;
				break;
			}
			// Each firmware upgrade failure interval delay 50ms
			mdelay(100);
		}
		if (tmp.i >= ABOV_RETRY_TIMES) {
			fw_update_status = ABOV_FW_UPDATE_FAILED;
			ret = -EIO;
		}
	}

rel_fw:
	release_firmware(fw);
	pr_info("Exit function: %s\n", __func__);
	return ret;
}

static ssize_t update_fw_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	u8 fw_ver = 0;
	int len = 16;

	read_register(abov_sar_ptr, ABOV_VERSION_REG, &fw_ver);
	return snprintf_s(buf, len, len - 1, "ABOV:0x%02x\n", fw_ver);
}

static ssize_t update_fw_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	unsigned long val;
	int ret;

	if (count != ABOV_UPDATE_COUNT_VALID) {
		pr_err("%s: count is invalid, count %ld\n", __func__, count);
		return -EINVAL;
	}

	ret = kstrtoul(buf, ABOV_HEX, &val);
	if (ret != 0)
		return ret;

	abov_sar_ptr->irq_disabled = ABOV_IRQ_DISABLE;
	disable_irq(abov_sar_ptr->irq);

	mutex_lock(&abov_sar_ptr->mutex);
	if (!abov_sar_ptr->loading_fw  && val) {
		abov_sar_ptr->loading_fw = true;
		abov_fw_update(false);
		abov_sar_ptr->loading_fw = false;
	}
	mutex_unlock(&abov_sar_ptr->mutex);

	enable_irq(abov_sar_ptr->irq);
	abov_sar_ptr->irq_disabled = ABOV_IRQ_ENABLE;

	return count;
}

static CLASS_ATTR_RW(update_fw);

static ssize_t force_update_fw_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	u8 fw_ver = 0;
	int len = 16;

	read_register(abov_sar_ptr, ABOV_VERSION_REG, &fw_ver);
	return snprintf_s(buf, len, len - 1, "ABOV:0x%02x\n", fw_ver);
}

static ssize_t force_update_fw_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	unsigned long val;
	int ret;

	if (count != ABOV_UPDATE_COUNT_VALID) {
		pr_err("%s: count is invalid %ld\n", __func__, count);
		return -EINVAL;
	}

	ret = kstrtoul(buf, ABOV_HEX, &val);
	if (ret != 0)
		return ret;

	abov_sar_ptr->irq_disabled = ABOV_IRQ_DISABLE;
	disable_irq(abov_sar_ptr->irq);

	mutex_lock(&abov_sar_ptr->mutex);
	if (!abov_sar_ptr->loading_fw  && (val != 0)) {
		abov_sar_ptr->loading_fw = true;
		abov_fw_update(true);
		abov_sar_ptr->loading_fw = false;
	}
	mutex_unlock(&abov_sar_ptr->mutex);

	enable_irq(abov_sar_ptr->irq);
	abov_sar_ptr->irq_disabled = ABOV_IRQ_ENABLE;
	return count;
}

static CLASS_ATTR_RW(force_update_fw);

static void capsense_update_work(struct work_struct *work)
{
	int ret;
	struct abov_dev_t *pdev = container_of(work, struct abov_dev_t,
		fw_update_work);

	pr_info("%s: start update firmware\n", __func__);

	mutex_lock(&pdev->mutex);
	pdev->loading_fw = true;
	ret = abov_fw_update(false);
	pdev->loading_fw = false;
	mutex_unlock(&pdev->mutex);

	if (fw_update_status == ABOV_FW_UPDATE_SUCCESS) {
		// after fw update need delay 50ms
		mdelay(50);
		abov_device_init(abov_sar_ptr);
	}

	pr_info("%s: exit, ret=%d\n", __func__, ret);
}

static void capsense_force_update_work(struct work_struct *work)
{
	int ret;
	struct abov_dev_t *pdev = container_of(work, struct abov_dev_t,
		fw_update_work);

	pr_info("%s: start force update firmware\n", __func__);

	mutex_lock(&pdev->mutex);
	pdev->loading_fw = true;
	ret = abov_fw_update(true);
	pdev->loading_fw = false;
	mutex_unlock(&pdev->mutex);

	if (fw_update_status == ABOV_FW_UPDATE_SUCCESS) {
		// after fw update need delay 50ms
		mdelay(50);
		abov_device_init(abov_sar_ptr);
	}

	pr_info("%s: exit, ret=%d\n", __func__, ret);
}

static int abov_sar_open_report_data(int open)
{
	static bool is_first_open = true;
	int ret;

	pr_info("%s: open is %d\n", __func__, open);

	if (open == 1) {
		if (is_first_open) {
			ret = manual_offset_calibration(abov_sar_ptr);
			if (ret < 0)
				pr_info("%s: calibration fail %d\n",
					__func__, ret);
			is_first_open = false;
		}
	}
	return 0;
}

static int abov_sar_batch(
	int flag, int64_t sampling_period_ns,
	int64_t max_batch_report_latency_ns)
{
	pr_info("%s\n", __func__);
	return 0;
}

static int abov_sar_flush(void)
{
	/* Only flush after sensor was enabled */
	pr_info("%s\n", __func__);
	return situation_flush_report(ID_SAR);
}

static int abov_sar_get_data(int *probability, int *status)
{
	pr_info("%s\n", __func__);
	return 0;
}

static void abov_create_attr(struct i2c_client *client)
{
	int ret;

	pr_info("%s\n", __func__);
	ret = sysfs_create_group(&client->dev.kobj, &abov_attr_group);
	if (ret < 0) {
		pr_err("%s: Create attr group failed %d\n", __func__, ret);
		return;
	}

	ret = class_register(&capsense_class);
	if (ret < 0) {
		pr_err("%s: Create fsys class failed %d\n", __func__, ret);
		goto err_remove_attr_group;
	}

	ret = class_create_file(&capsense_class, &class_attr_reg);
	if (ret < 0) {
		pr_err("%s: Create reg failed %d\n", __func__, ret);
		goto err_remove_cap_class;
	}

	ret = class_create_file(&capsense_class, &class_attr_update_fw);
	if (ret < 0) {
		pr_err("%s: Create update_fw failed %d\n", __func__, ret);
		goto err_remove_attr_reg;
	}

	ret = class_create_file(&capsense_class, &class_attr_force_update_fw);
	if (ret < 0) {
		pr_err("%s: Create update_fw failed %d\n", __func__, ret);
		goto err_remove_update_fw;
	}
	ret = class_create_file(&capsense_class, &class_attr_abov_info);
	if (ret < 0) {
		pr_err("%s: Create  abov_info failed %d\n", __func__, ret);
		goto err_remove_abov_info;
	}
	return;

err_remove_abov_info:
	class_remove_file(&capsense_class, &class_attr_abov_info);
err_remove_update_fw:
	class_remove_file(&capsense_class, &class_attr_update_fw);
err_remove_attr_reg:
	class_remove_file(&capsense_class, &class_attr_reg);
err_remove_cap_class:
	class_unregister(&capsense_class);
err_remove_attr_group:
	sysfs_remove_group(&client->dev.kobj, &abov_attr_group);
}

static int abov_fill_situation_func(void)
{
	int ret;
	struct situation_control_path ctl;
	struct situation_data_path data;

	pr_info("Enter %s\n", __func__);
	ctl.open_report_data = abov_sar_open_report_data;
	ctl.batch = abov_sar_batch;
	ctl.flush = abov_sar_flush;
	ctl.is_support_wake_lock = true;
	ctl.is_support_batch = false;
	ret = situation_register_control_path(&ctl, ID_SAR);
	if (ret != 0) {
		pr_err("%s: register sar control path err %d\n", __func__, ret);
		return ret;
	}

	data.get_data = abov_sar_get_data;
	ret = situation_register_data_path(&data, ID_SAR);
	if (ret != 0) {
		pr_err("%s: register sar data path err %d\n", __func__, ret);
		return ret;
	}
	return 0;
}

static int abov_remove(struct i2c_client *client)
{
	struct abov_platform_data *ppdata = NULL;
	struct abov_dev_t *pdev = NULL;

	pdev = i2c_get_clientdata(client);
	ppdata = client->dev.platform_data;

	if ((pdev == NULL) || (ppdata == NULL)) {
		pr_info("%s: this or ppdata is NULL\n", __func__);
		return -ENOMEM;
	}

	sysfs_remove_group(&client->dev.kobj, &abov_attr_group);
	if (ppdata->exit_platform_hw != NULL)
		ppdata->exit_platform_hw();
	kfree(ppdata);
	kfree(pdev->pdevice);
	cancel_delayed_work_sync(&pdev->dworker); /* Cancel the Worker Func */
	free_irq(pdev->irq, pdev);
	kfree(pdev);
	return 0;
}

static int abov_probe_init(
	struct i2c_client *client)
{
	struct sensorInfo_t abov_sar_info;
	int ret;

	pr_info("%s:start\n", __func__);

	if (!i2c_check_functionality(client->adapter,
		I2C_FUNC_SMBUS_READ_WORD_DATA)) {
		pr_err("%s: platform can't support read word\n", __func__);
		return -EIO;
	}

	/* detect if abov exist or not */
	ret = abov_device_detect(client);
	if (ret == 0) {
		pr_err("%s: chip detect fail\n", __func__);
		return -EINVAL;
	} else if (ret == ABOV_CHIP_IS_PRELOAD) {
		pr_info("%s: chip is preloader mode\n", __func__);
		return 1;
	}

	strlcpy(abov_sar_info.name, DRIVER_NAME, sizeof(abov_sar_info.name));
	ret = sensorlist_register_deviceinfo(ID_SAR, &abov_sar_info);
	if (ret != 0) {
		pr_err("%s: sersorlist register fail\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static void abov_init_ppdev(struct i2c_client *client,
	struct abov_dev_t *ppdev, struct abov_platform_data *ppdata)
{

	pr_info("%s:start\n", __func__);
	abov_sar_ptr = ppdev;

	ppdev->init = abov_device_init;
	/* shortcut to read status of interrupt */
	ppdev->refresh_status = read_reg_stat;
	ppdev->get_nirq_low = ppdata->get_is_nirq_low;
	ppdev->irq = gpio_to_irq(client->irq);
	ppdev->use_irq_timer = 0;
	ppdev->board = ppdata;

	/* Setup function to call on corresponding reg irq source bit */
	ppdev->status_func = sar_data_process;

	/* setup i2c communication */
	ppdev->bus = client;
	i2c_set_clientdata(client, ppdev);
	ppdev->pdev = &client->dev;
}

static int abov_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct abov_dev_t *ppdev = NULL;
	struct abov_platform_data *ppdata = NULL;
	bool is_force_update = 0;
	int ret;

	ret = abov_probe_init(client);
	if (ret < 0)
		return -EINVAL;
	else if (ret == 1)
		is_force_update = true;

	ppdata = kzalloc(sizeof(struct abov_platform_data), GFP_KERNEL);
	if (ppdata == NULL)
		return -ENOMEM;

	ret = abov_plat_data_init_from_dts(client, ppdata);
	if (ret != 0) {
		pr_err("%s: chip irq init fail\n", __func__);
		goto exit_free_ppdata;
	}
	client->dev.platform_data = ppdata;

	ppdev = kzalloc(sizeof(struct abov_dev_t), GFP_KERNEL);
	if (ppdev == NULL)
		goto exit_free_ppdata;

	abov_init_ppdev(client, ppdev, ppdata);

	/* create memory for device specific struct */
	ppdev->pdevice = kzalloc(sizeof(struct abov_dev_t), GFP_KERNEL);
	if (ppdev->pdevice == NULL)
		goto exit_free_ppdev;

	pr_info("%s: Init Device Specific Memory: 0x%p\n",
		__func__, ppdev->pdevice);
	((struct abov_plat *)ppdev->pdevice)->hw = ppdata;
	if (ppdata->init_platform_hw)
		ppdata->init_platform_hw();

	(void)abov_create_attr(client);
	ret = abov_sar_interrupt_init(ppdev);
	if (ret != 0) {
		pr_err("%s: sar intterrup init fail %d\n", __func__, ret);
		goto exit_free_pdevice;
	}
	if (ppdev->init != NULL) {
		ret = ppdev->init(ppdev);
		if (ret != 0) {
			pr_err("%s: abov sar device init fail %d\n", __func__, ret);
			goto exit_free_irq;
		}
	}
	ret = abov_fill_situation_func();
	if (ret != 0) {
		pr_err("%s: abov fill situation fail %d\n", __func__, ret);
		goto exit_free_irq;
	}
	ret = write_register(ppdev, ABOV_CTRL_MODE_REG, ABOV_NOMAL_MODE);
	if (ret < 0)
		pr_err("%s: write ctrl reg fail, ret %d\n", __func__, ret);

	ppdev->loading_fw = false;
	if (is_force_update == true)
		INIT_WORK(&ppdev->fw_update_work, capsense_force_update_work);
	else
		INIT_WORK(&ppdev->fw_update_work, capsense_update_work);

	schedule_work(&ppdev->fw_update_work);

	pr_info("%s: end\n", __func__);
	return 0;

exit_free_irq:
	free_irq(ppdev->irq, ppdev);
exit_free_pdevice:
	kfree(ppdev->pdevice);
	ppdev->pdevice = NULL;
exit_free_ppdev:
	kfree(ppdev);
	ppdev = NULL;
	abov_sar_ptr = NULL;
exit_free_ppdata:
	gpio_free(ppdata->irq_gpio);
	kfree(ppdata);
	ppdata = NULL;
	pr_err("%s() fail\n", __func__);
	return -ENOMEM;
}

static int abov_suspend(struct device *dev)
{
	struct abov_dev_t *pdev = NULL;

	if (dev == NULL) {
		pr_err("%s: dev is null\n", __func__);
		return -EINVAL;
	}

	pdev = dev_get_drvdata(dev);
	if (pdev == NULL) {
		pr_err("%s: NULL abov_dev_t\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: irq_disabled = %d, sar_irq = %d", __func__, pdev->irq_disabled, pdev->irq);
	if (pdev->irq_disabled == ABOV_IRQ_ENABLE)
		enable_irq_wake(pdev->irq);

	return 0;
}

static int abov_resume(struct device *dev)
{
	struct abov_dev_t *pdev = NULL;

	if (dev == NULL) {
		pr_err("%s: dev is null\n", __func__);
		return -EINVAL;
	}

	pdev = dev_get_drvdata(dev);
	if (pdev == NULL) {
		pr_err("%s: NULL abov_dev_t\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: irq_disabled = %d, sar_irq = %d", __func__, pdev->irq_disabled, pdev->irq);
	if (pdev->irq_disabled == ABOV_IRQ_ENABLE)
		disable_irq_wake(pdev->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(abov_pm_ops, abov_suspend, abov_resume);

#ifdef CONFIG_OF
static const struct of_device_id abov_match_tbl[] = {
	{ .compatible = "abov,abov_sar" },
	{ },
};
MODULE_DEVICE_TABLE(of, abov_match_tbl);
#endif

static struct i2c_device_id abov_idtable[] = {
	{ DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, abov_idtable);

static struct i2c_driver abov_driver = {
	.driver = {
		.owner  = THIS_MODULE,
		.name   = DRIVER_NAME,
		.pm = &abov_pm_ops,
	},
	.id_table = abov_idtable,
	.probe    = abov_probe,
	.remove   = abov_remove,
};

static void abov_process_interrupt(struct abov_dev_t *pdev, u8 nirqlow)
{
	int status;

	pr_info("Enter function: %s\n", __func__);
	if (pdev == NULL) {
		pr_err("%s: invalid paramter\n", __func__);
		return;
	}
	/* since we are not in an interrupt don't need to disable irq. */
	status = pdev->refresh_status(pdev);
	pr_info("%s: Refresh Status %d\n", __func__, status);
	pdev->status_func(pdev);
	if (unlikely(pdev->use_irq_timer && nirqlow)) {
		cancel_delayed_work(&pdev->dworker);
		schedule_delayed_work(&pdev->dworker,
			msecs_to_jiffies(pdev->irq_timeout));
		pr_info("Schedule Irq timer");
	}
	pr_info("Exit function: %s\n", __func__);
}

static void abov_worker_func(struct work_struct *work)
{
	struct abov_dev_t *pdev = NULL;

	pr_info("Enter function: %s\n", __func__);

	pdev = container_of(work, struct abov_dev_t, dworker.work);
	if (pdev == NULL) {
		pr_err("%s: NULL abovXX_t\n", __func__);
		return;
	}

	if ((!pdev->get_nirq_low) ||
		(!pdev->get_nirq_low(pdev->board->irq_gpio)))
		abov_process_interrupt(pdev, 0);
}

static irqreturn_t abov_interrupt_thread(int irq, void *data)
{
	struct abov_dev_t *pdev = data;

	pr_info("Enter function: %s\n", __func__);
	if (pdev == NULL) {
		pr_err("%s: invalid paramter\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&pdev->mutex);
	if ((pdev->get_nirq_low == NULL) ||
		pdev->get_nirq_low(pdev->board->irq_gpio))
		abov_process_interrupt(pdev, 1);
	else
		pr_info("abovXX_irq - nirq read high\n");
	mutex_unlock(&pdev->mutex);
	return IRQ_HANDLED;
}

static int abov_sar_interrupt_init(struct abov_dev_t *ppdev)
{
	int ret;

	pr_info("Enter %s\n", __func__);

	INIT_DELAYED_WORK(&ppdev->dworker, abov_worker_func);
	mutex_init(&ppdev->mutex);
	ppdev->irq_disabled = ABOV_IRQ_ENABLE;
	ret = request_threaded_irq(
		ppdev->irq,
		NULL,
		abov_interrupt_thread,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		ppdev->pdev->driver->name,
		ppdev);
	if (ret != 0) {
		pr_err("%s: irq %d busy\n", __func__, ppdev->irq);
		return ret;
	}

	pr_info("registered with threaded irq %d\n", ppdev->irq);

	return ret;
}

static int abov_sar_local_init(void)
{
	pr_info("%s\n", __func__);
	if (i2c_add_driver(&abov_driver)) {
		pr_err("%s: add driver error\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int abov_sar_local_uninit(void)
{
	i2c_del_driver(&abov_driver);
	return 0;
}

static struct situation_init_info abov_sar_init_info = {
	.name = "abov_sar",
	.init = abov_sar_local_init,
	.uninit = abov_sar_local_uninit,
};

static int __init abov_sar_init(void)
{
	if (hw_get_sensor_dts_config() == 0) {
		pr_info("sarhub_init\n");
		situation_driver_add(&abov_sar_init_info, ID_SAR);
	}
	return 0;
}

static void __exit abov_sar_exit(void)
{
	pr_info("%s\n", __func__);
}

module_init(abov_sar_init);
module_exit(abov_sar_exit);
