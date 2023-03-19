/*
 * ktd3133.c
 *
 * ktd3133 backlight driver
 *
 * Copyright (c) 2018-2019 Huawei Technologies Co., Ltd.
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

#include "ktd3133.h"
#ifdef DEVICE_TREE_SUPPORT
#include <libfdt.h>
#include <fdt_op.h>
#endif
#include <platform/mt_i2c.h>
#include <platform/mt_gpio.h>
#include "lcd_kit_common.h"
#include "lcd_kit_bl.h"

#define CHECK_STATUS_FAIL 0
#define CHECK_STATUS_OK 1

static struct ktd3133_backlight_information ktd3133_bl_info = {0};
static bool ktd3133_init_status;
static bool ktd3133_checked;
static unsigned int check_status;
static char *ktd3133_dts_string[KTD3133_RW_REG_MAX] = {
	"ktd3133_reg_control",
	"ktd3133_reg_lsb",
	"ktd3133_reg_msb",
	"ktd3133_reg_pwm",
	"ktd3133_reg_ramp_on",
	"ktd3133_reg_transit_ramp",
	"ktd3133_reg_mode",
};

static unsigned char ktd3133_reg_addr[KTD3133_RW_REG_MAX] = {
	KTD3133_REG_CONTROL,
	KTD3133_REG_RATIO_LSB,
	KTD3133_REG_RATIO_MSB,
	KTD3133_REG_PWM,
	KTD3133_REG_RAMP_ON,
	KTD3133_REG_TRANS_RAMP,
	KTD3133_REG_MODE,
};

static int ktd3133_i2c_read_u8(unsigned char addr, unsigned char *dataBuffer)
{
	int ret;
	unsigned char len;
	struct mt_i2c_t ktd3133_i2c = {0};
	*dataBuffer = addr;

	ktd3133_i2c.id = ktd3133_bl_info.ktd3133_i2c_bus_id;
	ktd3133_i2c.addr = KTD3133_SLAV_ADDR;
	ktd3133_i2c.mode = ST_MODE;
	ktd3133_i2c.speed = 100;
	len = 1;

	ret = i2c_write_read(&ktd3133_i2c, dataBuffer, len, len);
	if (ret != 0) {
		LCD_KIT_ERR("%s: i2c_read  failed! reg is 0x%x ret: %d\n",
			__func__, addr, ret);
	}
	return ret;
}

static int ktd3133_i2c_write_u8(unsigned char addr, unsigned char value)
{
	int ret;
	unsigned char write_data[2] = {0};
	unsigned char len;
	struct mt_i2c_t ktd3133_i2c = {0};

	write_data[0] = addr;
	write_data[1] = value;

	ktd3133_i2c.id = ktd3133_bl_info.ktd3133_i2c_bus_id;
	ktd3133_i2c.addr = KTD3133_SLAV_ADDR;
	ktd3133_i2c.mode = ST_MODE;
	ktd3133_i2c.speed = 100;
	len = 2;

	ret = i2c_write(&ktd3133_i2c, write_data, len);
	if (ret != 0) {
		LCD_KIT_ERR("%s: i2c_write  failed! reg is  0x%x ret: %d\n",
			__func__, addr, ret);
	}
	return ret;
}

static int ktd3133_parse_dts(void)
{
	int ret = 0;
	int i;

	LCD_KIT_INFO("ktd3133_parse_dts +!\n");

	for (i = 0; i < KTD3133_RW_REG_MAX; i++) {
		ret = lcd_kit_parse_get_u32_default(DTS_COMP_KTD3133,
			ktd3133_dts_string[i],
			&ktd3133_bl_info.ktd3133_reg[i], 0);
		if (ret < 0) {
			ktd3133_bl_info.ktd3133_reg[i] = 0xffff;
			LCD_KIT_INFO("can not find %s dts\n",
				ktd3133_dts_string[i]);
		} else {
			LCD_KIT_INFO("get %s value = 0x%x\n",
				ktd3133_dts_string[i],
				ktd3133_bl_info.ktd3133_reg[i]);
		}
	}

	return ret;
}

static int ktd3133_config_register(void)
{
	int ret = 0;
	int i;

	for (i = 0; i < KTD3133_RW_REG_MAX; i++) {
		if (ktd3133_bl_info.ktd3133_reg[i] != 0xffff) {
			ret = ktd3133_i2c_write_u8(ktd3133_reg_addr[i],
				(u8)ktd3133_bl_info.ktd3133_reg[i]);
			if (ret < 0) {
				LCD_KIT_ERR("write ktd3133 backlight config register 0x%x failed\n",
					ktd3133_reg_addr[i]);
				goto exit;
			}
		}
	}
exit:
	return ret;
}

static void ktd3133_enable(void)
{
	int ret;

	if (ktd3133_bl_info.ktd3133_hw_en) {
		mt_set_gpio_mode(ktd3133_bl_info.ktd3133_hw_en_gpio,
			GPIO_MODE_00);
		mt_set_gpio_dir(ktd3133_bl_info.ktd3133_hw_en_gpio,
			GPIO_DIR_OUT);
		mt_set_gpio_out(ktd3133_bl_info.ktd3133_hw_en_gpio,
			GPIO_OUT_ONE);
		if (ktd3133_bl_info.bl_on_lk_mdelay)
			mdelay(ktd3133_bl_info.bl_on_lk_mdelay);
	}
	ret = ktd3133_config_register();
	if (ret < 0) {
		LCD_KIT_ERR("ktd3133 config register failed\n");
		return;
	}
	ktd3133_init_status = true;
}

static void ktd3133_disable(void)
{
	if (ktd3133_bl_info.ktd3133_hw_en)
		mt_set_gpio_out(ktd3133_bl_info.ktd3133_hw_en_gpio,
			GPIO_OUT_ZERO);
	ktd3133_init_status = false;
}

static int ktd3133_set_backlight(uint32_t bl_level)
{
	static int last_bl_level;
	int bl_msb = 0;
	int bl_lsb = 0;
	int ret;

	/* first set backlight, enable ktd3133 */
	if (ktd3133_init_status == false && bl_level > 0)
		ktd3133_enable();

	/* set backlight level */
	ret = ktd3133_i2c_write_u8(KTD3133_REG_RATIO_LSB,
		ktd3133_reg_ratio_lsb_brightness[bl_level]);
	if (ret < 0)
		LCD_KIT_ERR("write ktd3133 backlight level lsb:0x%x failed\n",
			bl_lsb);
	ret = ktd3133_i2c_write_u8(KTD3133_REG_RATIO_MSB,
		ktd3133_reg_ratio_msb_brightness[bl_level]);
	if (ret < 0)
		LCD_KIT_ERR("write ktd3133 backlight level msb:0x%x failed\n",
			bl_msb);
	LCD_KIT_INFO("write ktd3133 backlight level success\n");

	/* if set backlight level 0, disable ktd3133 */
	if (ktd3133_init_status == true && bl_level == 0)
		ktd3133_disable();

	return ret;
}

void ktd3133_set_backlight_status(void)
{
	int ret;
	int offset;
	void *kernel_fdt = NULL;

#ifdef DEVICE_TREE_SUPPORT
	kernel_fdt = get_kernel_fdt();
#endif
	if (kernel_fdt == NULL) {
		LCD_KIT_ERR("kernel_fdt is NULL\n");
		return;
	}
	offset = fdt_node_offset_by_compatible(kernel_fdt, 0, DTS_COMP_KTD3133);
	if (offset < 0) {
		LCD_KIT_ERR("Could not find ktd3133 node, change dts failed\n");
		return;
	}

	if (check_status == CHECK_STATUS_OK)
		ret = fdt_setprop_string(kernel_fdt, offset, (const char *)"status",
			"okay");
	else
		ret = fdt_setprop_string(kernel_fdt, offset, (const char *)"status",
			"disabled");
	if (ret) {
		LCD_KIT_ERR("Cannot update ktd3133 status errno=%d\n", ret);
		return;
	}

	LCD_KIT_INFO("ktd3133_set_backlight_status OK!\n");
}

static struct lcd_kit_bl_ops bl_ops = {
	.set_backlight = ktd3133_set_backlight,
};

static int ktd3133_device_verify(void)
{
	int ret;
	unsigned char vendor_id = 0;

	if (ktd3133_bl_info.ktd3133_hw_en) {
		mt_set_gpio_mode(ktd3133_bl_info.ktd3133_hw_en_gpio,
			GPIO_MODE_00);
		mt_set_gpio_dir(ktd3133_bl_info.ktd3133_hw_en_gpio,
			GPIO_DIR_OUT);
		mt_set_gpio_out(ktd3133_bl_info.ktd3133_hw_en_gpio,
			GPIO_OUT_ONE);
		if (ktd3133_bl_info.bl_on_lk_mdelay)
			mdelay(ktd3133_bl_info.bl_on_lk_mdelay);
	}

	ret = ktd3133_i2c_read_u8(KTD3133_REG_DEV_ID, &vendor_id);
	if (ret < 0) {
		LCD_KIT_INFO("no ktd3133 device, read vendor id failed\n");
		goto error_exit;
	}
	if (vendor_id != KTD3133_VENDOR_ID) {
		LCD_KIT_INFO("no ktd3133 device, vendor id is not right\n");
		goto error_exit;
	}
	if (ktd3133_bl_info.ktd3133_hw_en)
		mt_set_gpio_out(ktd3133_bl_info.ktd3133_hw_en_gpio,
			GPIO_OUT_ZERO);
	return 0;
error_exit:
	if (ktd3133_bl_info.ktd3133_hw_en)
		mt_set_gpio_out(ktd3133_bl_info.ktd3133_hw_en_gpio,
			GPIO_OUT_ZERO);

	return LCD_KIT_FAIL;
}

int ktd3133_backlight_ic_recognize(void)
{
	int ret = 0;

	if (ktd3133_checked) {
		LCD_KIT_INFO("ktd3133 already check ,not again setting\n");
		return ret;
	}
	ret = ktd3133_device_verify();
	if (ret < 0) {
		check_status = CHECK_STATUS_FAIL;
		LCD_KIT_ERR("ktd3133 is not right backlight ics\n");
	} else {
		ktd3133_parse_dts();
		lcd_kit_bl_register(&bl_ops);
		check_status = CHECK_STATUS_OK;
		LCD_KIT_INFO("ktd3133 is right backlight ic\n");
	}
	ktd3133_checked = true;
	return ret;
}

int ktd3133_init(void)
{
	int ret;

	LCD_KIT_INFO("ktd3133 in %s\n", __func__);
	ret = lcd_kit_parse_get_u32_default(DTS_COMP_KTD3133,
		KTD3133_SUPPORT,
		&ktd3133_bl_info.ktd3133_support, 0);
	if ((ret < 0) || (!ktd3133_bl_info.ktd3133_support)) {
		LCD_KIT_ERR("not support ktd3133!\n");
		return LCD_KIT_OK;
	}

	/* register bl ops */
	lcd_kit_backlight_recognize_register(ktd3133_backlight_ic_recognize);
	ret = lcd_kit_parse_get_u32_default(DTS_COMP_KTD3133,
		KTD3133_I2C_BUS_ID,
		&ktd3133_bl_info.ktd3133_i2c_bus_id, 0);
	if (ret < 0) {
		ktd3133_bl_info.ktd3133_i2c_bus_id = 0;
		return LCD_KIT_OK;
	}
	ret = lcd_kit_parse_get_u32_default(DTS_COMP_KTD3133,
		KTD3133_HW_ENABLE,
		&ktd3133_bl_info.ktd3133_hw_en, 0);
	if (ret < 0) {
		LCD_KIT_ERR("parse dts ktd3133_hw_enable fail!\n");
		ktd3133_bl_info.ktd3133_hw_en = 0;
		return LCD_KIT_OK;
	}
	if (ktd3133_bl_info.ktd3133_hw_en) {
		ret = lcd_kit_parse_get_u32_default(DTS_COMP_KTD3133,
			KTD3133_HW_EN_GPIO,
			&ktd3133_bl_info.ktd3133_hw_en_gpio, 0);
		if (ret < 0) {
			LCD_KIT_ERR("parse dts ktd3133_hw_en_gpio fail!\n");
			ktd3133_bl_info.ktd3133_hw_en_gpio = 0;
			return LCD_KIT_OK;
		}
		ret = lcd_kit_parse_get_u32_default(DTS_COMP_KTD3133,
			KTD3133_HW_EN_DELAY,
			&ktd3133_bl_info.bl_on_lk_mdelay, 0);
		if (ret < 0) {
			LCD_KIT_ERR("parse dts bl_on_lk_mdelay fail!\n");
			ktd3133_bl_info.bl_on_lk_mdelay = 0;
			return LCD_KIT_OK;
		}
	}
	LCD_KIT_INFO("[%s]:ktd3133 is support!\n", __func__);
	LCD_KIT_INFO("ktd3133 out %s\n", __func__);
	return LCD_KIT_OK;
}
