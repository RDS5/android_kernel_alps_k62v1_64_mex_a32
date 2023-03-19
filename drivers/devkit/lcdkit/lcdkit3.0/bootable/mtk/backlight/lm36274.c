/*
 * drivers/display/hisi/backlight/lm36274.c
 *
 * lm36274 driver reffer to lcd
 *
 * Copyright (C) 2012-2015 HUAWEI, Inc.
 * Author: HUAWEI, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef DEVICE_TREE_SUPPORT
#include <libfdt.h>
#include <fdt_op.h>
#endif
#include <platform/mt_i2c.h>
#include <platform/mt_gpio.h>
#include "lm36274.h"
#include "lcd_kit_common.h"
#include "lcd_kit_bl.h"

static int g_hidden_reg_support;
static bool lm36274_init_status;
static unsigned int check_status;
static bool lm36274_checked;
static struct lm36274_backlight_information lm36274_bl_info = {0};

static int lm36274_i2c_read_u8(u8 chip_no, u8 *data_buffer, int addr)
{
	int ret = -1;
	/* read date default length */
	unsigned char len = 1;
	struct mt_i2c_t lm36274_i2c = {0};

	if (!data_buffer) {
		LCD_KIT_ERR("data buffer is NULL");
		return ret;
	}

	*data_buffer = addr;
	lm36274_i2c.id = lm36274_bl_info.lm36274_i2c_bus_id;
	lm36274_i2c.addr = chip_no;
	lm36274_i2c.mode = ST_MODE;
	lm36274_i2c.speed =LM36274_I2C_SPEED;

	ret = i2c_write_read(&lm36274_i2c, data_buffer, len, len);
	if (ret != 0)
		LCD_KIT_ERR("%s: i2c_read failed, reg is 0x%x ret: %d\n",
			__func__, addr, ret);

	return ret;
}

static int lm36274_i2c_write_u8(u8 chip_no, unsigned char addr, unsigned char value)
{
	int ret;
	unsigned char write_data[LM36274_WRITE_LEN] = {0};
	/* write date default length */
	unsigned char len = LM36274_WRITE_LEN;
	struct mt_i2c_t lm36274_i2c = {0};

	/* data0: address, data1: write value */
	write_data[0] = addr;
	write_data[1] = value;

	lm36274_i2c.id = lm36274_bl_info.lm36274_i2c_bus_id;
	lm36274_i2c.addr = chip_no;
	lm36274_i2c.mode = ST_MODE;
	lm36274_i2c.speed = LM36274_I2C_SPEED;

	ret = i2c_write(&lm36274_i2c, write_data, len);
	if (ret != 0)
		LCD_KIT_ERR("%s: i2c_write failed, reg is 0x%x ret: %d\n",
			__func__, addr, ret);

	return ret;
}

static int lm36274_i2c_update_bits(u8 chip_no, u8 reg, u8 mask, u8 val)
{
	int ret;
	u8 tmp;
	u8 orig = 0;

	ret = lm36274_i2c_read_u8(chip_no, &orig, reg);
	if (ret < 0)
		return ret;

	tmp = orig & ~mask;
	tmp |= val & mask;

	if (tmp != orig) {
		ret = lm36274_i2c_write_u8(chip_no, reg, val);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static int lm36274_parse_dts()
{
	int ret;
	int i;

	LCD_KIT_INFO("lm36274_parse_dts +!\n");
	for (i = 0; i < LM36274_RW_REG_MAX; i++) {
		ret = lcd_kit_parse_get_u32_default(DTS_COMP_LM36274,
			lm36274_dts_string[i],&lm36274_bl_info.lm36274_reg[i], 0);
		if (ret < 0) {
			lm36274_bl_info.lm36274_reg[i] = LM36274_INVALID_VAL;
			LCD_KIT_INFO("can not find %s dts\n", lm36274_dts_string[i]);
		} else {
			LCD_KIT_INFO("get %s value = 0x%x\n",
				lm36274_dts_string[i], lm36274_bl_info.lm36274_reg[i]);
		}

	}

	return ret;
}

static int lm36274_set_hidden_reg()
{
	int ret;
	u8 val = 0;

	ret = lm36274_i2c_write_u8(LM36274_SLAV_ADDR, REG_SET_SECURITYBIT_ADDR, REG_SET_SECURITYBIT_VAL);
	if (ret < 0) {
		LCD_KIT_ERR("write register 0x%x failed\n",REG_SET_SECURITYBIT_ADDR);
		goto out;
	} else {
		LCD_KIT_INFO("register 0x%x value = 0x%x\n", REG_SET_SECURITYBIT_ADDR, REG_SET_SECURITYBIT_VAL);
	}

	ret = lm36274_i2c_read_u8(LM36274_SLAV_ADDR,  &val, REG_HIDDEN_ADDR);
	if (ret < 0) {
		LCD_KIT_ERR("read register 0x%x failed", REG_HIDDEN_ADDR);
		goto out;
	} else {
		LCD_KIT_INFO("read 0x%x value = 0x%x\n", REG_HIDDEN_ADDR, val);
	}

	if (LM36274_BIAS_BOOST_TIME_3 != (val & LM36274_BIAS_BOOST_TIME_3))
	{
		val |= LM36274_BIAS_BOOST_TIME_3;

		ret = lm36274_i2c_write_u8(LM36274_SLAV_ADDR, REG_HIDDEN_ADDR, val);
		if (ret < 0) {
			LCD_KIT_ERR("write register 0x%x failed\n",REG_HIDDEN_ADDR);
			goto out;
		} else {
			LCD_KIT_INFO("register 0x%x value = 0x%x\n", REG_HIDDEN_ADDR, val);
		}

		ret = lm36274_i2c_read_u8(LM36274_SLAV_ADDR,  &val, REG_HIDDEN_ADDR);
		if (ret < 0) {
			LCD_KIT_ERR("read register 0x%x failed", REG_HIDDEN_ADDR);
			goto out;
		} else {
			LCD_KIT_INFO("read 0x%x value = 0x%x\n", REG_HIDDEN_ADDR, val);
		}
	}

	ret = lm36274_i2c_write_u8(LM36274_SLAV_ADDR, REG_SET_SECURITYBIT_ADDR, REG_CLEAR_SECURITYBIT_VAL);
	if (ret < 0) {
		LCD_KIT_ERR("write register 0x%x failed\n",REG_SET_SECURITYBIT_ADDR);
		goto out;
	} else {
		LCD_KIT_INFO("register 0x%x value = 0x%x\n", REG_SET_SECURITYBIT_ADDR, REG_CLEAR_SECURITYBIT_VAL);
	}

out:
	LCD_KIT_INFO("lm36274_set_hidden_reg exit\n");
	return ret;
}

static int lm36274_config_register()
{
	int ret;
	int i;
	for(i = 0;i < LM36274_RW_REG_MAX;i++) {
		ret = lm36274_i2c_write_u8(LM36274_SLAV_ADDR, lm36274_reg_addr[i],
				lm36274_bl_info.lm36274_reg[i]);
		if (ret < 0) {
			LCD_KIT_ERR("write lm36274 backlight config register 0x%x failed",lm36274_reg_addr[i]);
			return ret;
		}
	}

	if (g_hidden_reg_support)
	{
		ret = lm36274_set_hidden_reg();
		if (ret < 0)
			LCD_KIT_ERR("lm36274_set_hidden_reg failed");
	}

	/* set bright control mode to 0x68 as linear mode ,do not write one bit only. */
	ret = lm36274_i2c_update_bits(LM36274_SLAV_ADDR, REG_BL_CONFIG_1, MASK_CHANGE_MODE,
		LM36274_BL_OVP_29|OVP_SHUTDOWN_DISABLE|LM36274_BLED_MODE_LINEAR|LM36274_PWN_CONFIG_HIGH|LM36274_RAMP_DIS|PWM_DISABLE);
	LCD_KIT_INFO(" bootloader set lm36274 for linear mode \n");
	if (ret < 0) {
		LCD_KIT_ERR("set lm36274 backlight control mode failed");
		return LCD_KIT_FAIL;
	}

	return ret;
}


static int lm36274_config_read()
{
	int ret;
	int i;
	for(i = 0;i < LM36274_RW_REG_MAX;i++) {
		ret = lm36274_i2c_read_u8(LM36274_SLAV_ADDR,  &lm36274_bl_info.lm36274_reg[i],
				lm36274_reg_addr[i]);
		if (ret < 0) {
			LCD_KIT_ERR("read lm36274 backlight config register 0x%x failed", lm36274_bl_info.lm36274_reg[i]);
			goto exit;
		}
	}
exit:
	return ret;
}

static struct lcd_kit_bl_ops bl_ops = {
	.set_backlight = lm36274_set_backlight,
};

static int lm36274_device_verify(void)
{
	int ret;
	int is_support = 0;
	unsigned char reg_val = 0;

	if (lm36274_bl_info.lm36274_hw_en) {
		mt_set_gpio_mode(lm36274_bl_info.lm36274_hw_en_gpio,
			GPIO_MODE_00);
		mt_set_gpio_dir(lm36274_bl_info.lm36274_hw_en_gpio,
			GPIO_DIR_OUT);
		mt_set_gpio_out(lm36274_bl_info.lm36274_hw_en_gpio,
			GPIO_OUT_ONE);
		if (lm36274_bl_info.bl_on_lk_mdelay)
			mdelay(lm36274_bl_info.bl_on_lk_mdelay);
	}

	/* Read IC revision */
	ret = lm36274_i2c_read_u8(LM36274_SLAV_ADDR, &reg_val, REG_REVISION);
	if (ret < 0) {
		LCD_KIT_ERR("read lm36274 revision failed\n");
		goto error_exit;
	}
	LCD_KIT_INFO("lm36274 reg revision = 0x%x\n", reg_val);
	if ((reg_val & DEV_MASK) != VENDOR_ID_TI) {
		LCD_KIT_INFO("lm36274 check vendor id failed\n");
		ret = LCD_KIT_FAIL;
		goto error_exit;
	}

	ret = lcd_kit_parse_get_u32_default(DTS_COMP_LM36274, LM36274_EXTEND_REV_SUPPORT,
		&is_support,0);
	if (ret < 0) {
		LCD_KIT_INFO("no lm36274 extend rev compatible \n");
	} else {
		LCD_KIT_INFO("get lm36274 extend rev %d, support\n", is_support);
		if (is_support == LM36274_EXTEND_REV_VAL) {
			/* Read BL_BLCONFIG1, to fix TI and NT same revision problem */
			ret = lm36274_i2c_read_u8(LM36274_SLAV_ADDR, &reg_val, REG_BL_CONFIG_1);
			if (ret < 0) {
				LCD_KIT_ERR("read lm36274 bl config 1 failed\n");
				goto error_exit;
			}
			LCD_KIT_INFO("lm32674 reg bl config1 = 0x%x\n", reg_val);
			if ((reg_val & DEV_MASK_EXTEND_LM32674) == VENDOR_ID_EXTEND_LM32674) {
				LCD_KIT_INFO("lm36274 check vendor id exend failed\n");
				ret = LCD_KIT_FAIL;
				goto error_exit;
			}
		}
	}

	ret = lm36274_config_register();
	if (ret < 0) {
		LCD_KIT_ERR("lm36274 config register failed\n");
		goto error_exit;
	}

	ret = lm36274_config_read();
	if (ret < 0) {
		LCD_KIT_ERR("lm36274 config read failed\n");
		goto error_exit;
	}
error_exit:
	if (lm36274_bl_info.lm36274_hw_en)
		mt_set_gpio_out(lm36274_bl_info.lm36274_hw_en_gpio,
			GPIO_OUT_ZERO);
	return ret;
}

int lm36274_backlight_ic_recognize(void)
{
	int ret =0;

	if (lm36274_checked) {
		LCD_KIT_INFO("lm36274 already check, not again setting\n");
		return ret;
	}

	ret = lm36274_device_verify();
	if (ret < 0) {
		check_status = CHECK_STATUS_FAIL;
		LCD_KIT_ERR("lm36274 is not right backlight ics\n");
	} else {
		lm36274_parse_dts();
		lcd_kit_bl_register(&bl_ops);
		check_status = CHECK_STATUS_OK;
		LCD_KIT_INFO("lm36274 is right backlight ic\n");
	}
	lm36274_checked = true;
	return ret;
}

int lm36274_init(void)
{
	int ret;

	LCD_KIT_INFO("lm36274 in %s\n", __func__);

	ret = lcd_kit_parse_get_u32_default(DTS_COMP_LM36274,
		LM36274_SUPPORT, &lm36274_bl_info.lm36274_support, 0);
	if (ret < 0 || !lm36274_bl_info.lm36274_support) {
		LCD_KIT_ERR("get lm36274_support failed!\n");
		return LCD_KIT_FAIL;
	}

      /* register bl ops */
	lcd_kit_backlight_recognize_register(lm36274_backlight_ic_recognize);

	ret = lcd_kit_parse_get_u32_default(DTS_COMP_LM36274,
		LM36274_I2C_BUS_ID, &lm36274_bl_info.lm36274_i2c_bus_id, 0);
	if (ret < 0) {
		LCD_KIT_ERR("parse dts lm36274_i2c_bus_id fail!\n");
		lm36274_bl_info.lm36274_i2c_bus_id = 0;
		return LCD_KIT_FAIL;
	}

	ret = lcd_kit_parse_get_u32_default(DTS_COMP_LM36274,
		GPIO_LM36274_EN_NAME, &lm36274_bl_info.lm36274_hw_en, 0);
	if (ret < 0) {
		LCD_KIT_ERR(" parse dts lm36274_hw_enable fail!\n");
		lm36274_bl_info.lm36274_hw_en = 0;
		return LCD_KIT_FAIL;
	}

	if (lm36274_bl_info.lm36274_hw_en) {
		ret = lcd_kit_parse_get_u32_default(DTS_COMP_LM36274,
			LM36274_HW_EN_GPIO,&lm36274_bl_info.lm36274_hw_en_gpio, 0);
		if (ret < 0) {
			LCD_KIT_ERR("parse dts lm36274_hw_en_gpio fail!\n");
			lm36274_bl_info.lm36274_hw_en_gpio= 0;
			return LCD_KIT_FAIL;
		}
		ret = lcd_kit_parse_get_u32_default(DTS_COMP_LM36274,
			LM36274_HW_EN_DELAY,&lm36274_bl_info.bl_on_lk_mdelay, 0);
		if (ret < 0)
			LCD_KIT_INFO("parse dts bl_on_lk_mdelay fail!\n");
	}

	ret = lcd_kit_parse_get_u32_default(DTS_COMP_LM36274, LM36274_HIDDEN_REG_SUPPORT, &g_hidden_reg_support,0);
	if (ret < 0)
		g_hidden_reg_support = 0;
	else
		LCD_KIT_INFO("g_hidden_reg is support\n");

	ret = lcd_kit_parse_get_u32_default(DTS_COMP_LM36274,
		LM36274_BL_LEVEL, &lm36274_bl_info.bl_level, LM36274_BL_DEFAULT_LEVEL);
	if (ret < 0)
		LCD_KIT_ERR("parse dts lm36274_bl_level fail!\n");

	LCD_KIT_INFO("[%s]:lm36274 is support\n", __FUNCTION__);

	return LCD_KIT_OK;
}

static void lm36274_enable(void)
{
	int ret;

	if (lm36274_bl_info.lm36274_hw_en) {
			mt_set_gpio_mode(lm36274_bl_info.lm36274_hw_en_gpio,
				GPIO_MODE_00);
			mt_set_gpio_dir(lm36274_bl_info.lm36274_hw_en_gpio,
				GPIO_DIR_OUT);
			mt_set_gpio_out(lm36274_bl_info.lm36274_hw_en_gpio,
				GPIO_OUT_ONE);
			if (lm36274_bl_info.bl_on_lk_mdelay)
				mdelay(lm36274_bl_info.bl_on_lk_mdelay);
	}

	ret = lm36274_config_register();
	if (ret < 0) {
		LCD_KIT_ERR(" lm36274 config register failed\n");
		return;
	}

	lm36274_init_status = true;
}

static void lm36274_disable(void)
{
	if (lm36274_bl_info.lm36274_hw_en)
		mt_set_gpio_out(lm36274_bl_info.lm36274_hw_en_gpio,
				GPIO_OUT_ZERO);

	lm36274_init_status = false;
}

/**
 * lm36274_set_backlight(): Set Backlight working mode
 *
 * @bl_level: value for backlight ,range from 0 to 2047
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */

int lm36274_set_backlight(uint32_t bl_level)
{
	int ret;
	uint32_t level;
	int bl_msb;
	int bl_lsb;

	/* first set backlight, enable lm36274 */
	if ((lm36274_init_status == false) && (bl_level > 0))
		lm36274_enable();

	/* map bl_level from 10000 to 2048 stage brightness */
	level = bl_level * lm36274_bl_info.bl_level / LM36274_BL_DEFAULT_LEVEL;

	if (level > BL_MAX)
		level = BL_MAX;

	/* 11-bit brightness code */
	bl_msb = level >> MSB;
	bl_lsb = level & LSB;

	LCD_KIT_INFO("level = %d, bl_msb = %d, bl_lsb = %d", level, bl_msb, bl_lsb);

	ret = lm36274_i2c_update_bits(LM36274_SLAV_ADDR, REG_BL_BRIGHTNESS_LSB, MASK_BL_LSB,bl_lsb);
	if (ret < 0)
		goto i2c_error;

	ret = lm36274_i2c_write_u8(LM36274_SLAV_ADDR, REG_BL_BRIGHTNESS_MSB, bl_msb);
	if (ret < 0)
		goto i2c_error;

	LCD_KIT_INFO("write lm36274 backlight %u success\n", bl_level);

	/* if set backlight level 0, disable lm36274 */
	if ((lm36274_init_status == true) && (bl_level == 0))
		lm36274_disable();

	return ret;

i2c_error:
	LCD_KIT_ERR("%s:i2c access fail to register", __func__);
	return ret;
}

void lm36274_set_backlight_status (void)
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
	offset = fdt_node_offset_by_compatible(kernel_fdt, 0, DTS_COMP_LM36274);
	if (offset < 0) {
		LCD_KIT_ERR("Could not find lm36274 node, change dts failed\n");
		return;
	}

	if (check_status == CHECK_STATUS_OK)
		ret = fdt_setprop_string(kernel_fdt, offset, (const char *)"status",
			"okay");
	else
		ret = fdt_setprop_string(kernel_fdt, offset, (const char *)"status",
			"disabled");
	if (ret) {
		LCD_KIT_ERR("Cannot update lm36274 status errno=%d\n", ret);
		return;
	}

	LCD_KIT_INFO("lm36274_set_backlight_status OK!\n");
}

