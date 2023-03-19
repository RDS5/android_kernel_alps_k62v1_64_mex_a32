/*
 * nt50356.c
 *
 * adapt for backlight driver
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

#ifdef DEVICE_TREE_SUPPORT
#include <libfdt.h>
#include <fdt_op.h>
#endif
#include <platform/mt_i2c.h>
#include <platform/mt_gpio.h>
#include "nt50356.h"
#include "lcd_kit_common.h"
#include "lcd_kit_bl.h"
#include "lcd_bl_linear_exponential_table.h"

static bool nt50356_init_status;
static unsigned int check_status;

static struct lcd_kit_bl_ops bl_ops = {
	.set_backlight = nt50356_set_backlight,
};

static int nt50356_i2c_read_u8(u8 chip_no, u8 *data_buffer, int addr)
{
	int ret = NT50356_FAIL;
	/* read date default length */
	unsigned char len = 1;
	struct mt_i2c_t nt50356_i2c = {0};

	if (!data_buffer) {
		LCD_KIT_ERR("data buffer is NULL");
		return ret;
	}

	*data_buffer = addr;
	nt50356_i2c.id = nt50356_bl_info.nt50356_i2c_bus_id;
	nt50356_i2c.addr = chip_no;
	nt50356_i2c.mode = ST_MODE;
	nt50356_i2c.speed = NT50356_I2C_SPEED;

	ret = i2c_write_read(&nt50356_i2c, data_buffer, len, len);
	if (ret != 0)
		LCD_KIT_ERR("%s: i2c_read failed, reg is 0x%x ret: %d\n",
			__func__, addr, ret);

	return ret;
}

static int nt50356_i2c_write_u8(u8 chip_no, unsigned char addr, unsigned char value)
{
	int ret;
	unsigned char write_data[NT50356_WRITE_LEN] = {0};
	/* write date default length */
	unsigned char len = NT50356_WRITE_LEN;
	struct mt_i2c_t nt50356_i2c = {0};

	/* data0: address, data1: write value */
	write_data[0] = addr;
	write_data[1] = value;

	nt50356_i2c.id = nt50356_bl_info.nt50356_i2c_bus_id;
	nt50356_i2c.addr = chip_no;
	nt50356_i2c.mode = ST_MODE;
	nt50356_i2c.speed = NT50356_I2C_SPEED;

	ret = i2c_write(&nt50356_i2c, write_data, len);
	if (ret != 0)
		LCD_KIT_ERR("%s: i2c_write failed, reg is 0x%x ret: %d\n",
			__func__, addr, ret);

	return ret;
}

static int nt50356_i2c_update_bits(u8 chip_no, int reg, u8 mask, u8 val)
{
	int ret;
	unsigned char tmp;
	unsigned char orig = 0;

	ret = nt50356_i2c_read_u8(chip_no, &orig, reg);
	if (ret < 0)
		return ret;
	tmp = orig & ~mask;
	tmp |= val & mask;
	if (tmp != orig) {
		ret = nt50356_i2c_write_u8(chip_no, reg, val);
		if (ret < 0)
			return ret;
	}
	return ret;
}

static int nt50356_parse_dts(void)
{
	int ret;
	int i;

	for (i = 0; i < NT50356_RW_REG_MAX; i++) {
		ret = lcd_kit_parse_get_u32_default(DTS_COMP_NT50356, nt50356_dts_string[i],
			&nt50356_bl_info.nt50356_reg[i],0);
		if (ret < 0)
			LCD_KIT_INFO("get nt50356 dts config failed\n");
		else
			LCD_KIT_INFO("get %s value = 0x%x\n",
				nt50356_dts_string[i], nt50356_bl_info.nt50356_reg[i]);
	}
}

static int nt50356_config_register(void)
{
	int ret;
	int i;

	for (i = 0; i < NT50356_RW_REG_MAX; i++) {
		ret = nt50356_i2c_write_u8(NT50356_SLAV_ADDR,
			nt50356_reg_addr[i], nt50356_bl_info.nt50356_reg[i]);
		if (ret < 0) {
			LCD_KIT_ERR("write ktz bl config reg 0x%x failed",
				nt50356_reg_addr[i]);
			return ret;
		}
	}
	return ret;
}

static int nt50356_config_read(void)
{
	int ret;
	int i;

	for (i = 0; i < NT50356_RW_REG_MAX; i++) {
		ret = nt50356_i2c_read_u8(NT50356_SLAV_ADDR,
			(u8 *)&nt50356_bl_info.nt50356_reg[i], nt50356_reg_addr[i]);
		if (ret < 0) {
			LCD_KIT_ERR("read nt bl config reg 0x%x failed",
				nt50356_bl_info.nt50356_reg[i]);
			return ret;
		}
	}
	return ret;
}

void nt50356_set_backlight_status (void)
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
	offset = fdt_node_offset_by_compatible(kernel_fdt, 0, DTS_COMP_NT50356);
	if (offset < 0) {
		LCD_KIT_ERR("Could not find nt50356 node, change dts failed\n");
		return;
	}

	if (check_status == CHECK_STATUS_OK)
		ret = fdt_setprop_string(kernel_fdt, offset, (const char *)"status",
			"okay");
	else
		ret = fdt_setprop_string(kernel_fdt, offset, (const char *)"status",
			"disabled");
	if (ret) {
		LCD_KIT_ERR("Cannot update nt50356 status errno=%d\n", ret);
		return;
	}

	LCD_KIT_INFO("nt50356_set_backlight_status OK!\n");
}

static int nt50356_device_verify(void)
{
	int ret;
	int is_support = 0;
	unsigned char reg_val = 0;

	if (nt50356_bl_info.nt50356_hw_en) {
		mt_set_gpio_mode(nt50356_bl_info.nt50356_hw_en,
			GPIO_MODE_00);
		mt_set_gpio_dir(nt50356_bl_info.nt50356_hw_en_gpio,
			GPIO_DIR_OUT);
		mt_set_gpio_out(nt50356_bl_info.nt50356_hw_en_gpio,
			GPIO_OUT_ONE);
		if (nt50356_bl_info.bl_on_lk_mdelay)
			mdelay(nt50356_bl_info.bl_on_lk_mdelay);
	}

	/* Read IC revision */
	ret = nt50356_i2c_read_u8(NT50356_SLAV_ADDR, &reg_val, REG_REVISION);
	if (ret < 0) {
		LCD_KIT_ERR("read nt50356 revision failed\n");
		goto error_exit;
	}
	LCD_KIT_INFO("nt50356 reg revision = 0x%x\n", reg_val);
	if ((reg_val & DEV_MASK) != VENDOR_ID_NT50356) {
		LCD_KIT_INFO("nt50356 check vendor id failed\n");
		ret = NT50356_FAIL;
		goto error_exit;
	}

	ret = lcd_kit_parse_get_u32_default(DTS_COMP_NT50356, NT50356_EXTEND_REV_SUPPORT,
		&is_support,0);
	if (ret < 0) {
		LCD_KIT_INFO("no nt50356 extend rev compatible \n");
	} else {
		LCD_KIT_INFO("get nt50356 extend rev %d, support\n", is_support);
		if (is_support == NT50356_EXTEND_REV_VAL) {
			/* Read BL_BLCONFIG1, to fix TI and NT same revision problem */
			ret = nt50356_i2c_read_u8(NT50356_SLAV_ADDR, &reg_val, REG_BL_CONFIG_1);
			if (ret < 0) {
				LCD_KIT_ERR("read nt50356 bl config 1 failed\n");
				goto error_exit;
			}
			LCD_KIT_INFO("nt50356 reg bl config1 = 0x%x\n", reg_val);
			if ((reg_val & DEV_MASK_EXTEND_NT50356) != VENDOR_ID_EXTEND_NT50356) {
				LCD_KIT_INFO("nt50356 check vendor id exend failed\n");
				ret = NT50356_FAIL;
				goto error_exit;
			}
		}
	}

	ret = nt50356_config_register();
	if (ret < 0) {
		LCD_KIT_ERR("nt50356 config register failed\n");
		goto error_exit;
	}

	ret = nt50356_config_read();
	if (ret < 0) {
		LCD_KIT_ERR("nt50356 config read failed\n");
		goto error_exit;
	}
error_exit:
	if (nt50356_bl_info.nt50356_hw_en)
		mt_set_gpio_out(nt50356_bl_info.nt50356_hw_en_gpio,
			GPIO_OUT_ZERO);
	return ret;
}

int nt50356_backlight_ic_recognize(void)
{
	int ret;

	ret = nt50356_device_verify();
	if (ret < 0) {
		check_status = CHECK_STATUS_FAIL;
		LCD_KIT_ERR("nt50356 is not right backlight ic\n");
	} else {
		nt50356_parse_dts();
		lcd_kit_bl_register(&bl_ops);
		check_status = CHECK_STATUS_OK;
		LCD_KIT_INFO("nt50356 is right backlight ic\n");
	}
	return ret;
}

int nt50356_init(void)
{
	u8 reg_val = 0;
	int ret;
	int is_support = 0;

	LCD_KIT_INFO("nt50356 in %s\n", __func__);

	memset(&nt50356_bl_info, 0, sizeof(struct nt50356_backlight_information));

	ret = lcd_kit_parse_get_u32_default(DTS_COMP_NT50356,
		NT50356_SUPPORT, &nt50356_bl_info.nt50356_support, 0);
	if (ret < 0 || !nt50356_bl_info.nt50356_support) {
		LCD_KIT_ERR("get nt50356_support failed!\n");
		goto exit;
	}

      /* register bl ops */
	lcd_kit_backlight_recognize_register(nt50356_backlight_ic_recognize);

	ret = lcd_kit_parse_get_u32_default(DTS_COMP_NT50356,
		NT50356_I2C_BUS_ID, &nt50356_bl_info.nt50356_i2c_bus_id, 0);
	if (ret < 0) {
		LCD_KIT_ERR("parse dts nt50356_i2c_bus_id fail!\n");
		nt50356_bl_info.nt50356_i2c_bus_id = 0;
		goto exit;
	}

	ret = lcd_kit_parse_get_u32_default(DTS_COMP_NT50356,
		GPIO_NT50356_EN_NAME, &nt50356_bl_info.nt50356_hw_en, 0);
	if (ret < 0) {
		LCD_KIT_ERR(" parse dts nt50356_hw_enable fail!\n");
		nt50356_bl_info.nt50356_hw_en = 0;
		goto exit;
	}

	if (nt50356_bl_info.nt50356_hw_en) {
		ret = lcd_kit_parse_get_u32_default(DTS_COMP_NT50356,
			NT50356_HW_EN_GPIO,&nt50356_bl_info.nt50356_hw_en_gpio, 0);
		if (ret < 0) {
			LCD_KIT_ERR("parse dts nt50356_hw_en_gpio fail!\n");
			nt50356_bl_info.nt50356_hw_en_gpio= 0;
			goto exit;
		}
		ret = lcd_kit_parse_get_u32_default(DTS_COMP_NT50356,
			NT50356_HW_EN_DELAY,&nt50356_bl_info.bl_on_lk_mdelay, 0);
		if (ret < 0)
			LCD_KIT_INFO("parse dts bl_on_lk_mdelay fail!\n");
	}

	ret = lcd_kit_parse_get_u32_default(DTS_COMP_NT50356,
		NT50356_BL_LEVEL, &nt50356_bl_info.bl_level, NT50356_BL_DEFAULT_LEVEL);
	if (ret < 0)
		LCD_KIT_ERR("parse dts nt50356_bl_level fail!\n");

	LCD_KIT_INFO("[%s]:nt50356 is support\n", __FUNCTION__);

exit:
	return ret;
}

static void nt50356_enable(void)
{
	int ret;

	if (nt50356_bl_info.nt50356_hw_en) {
			mt_set_gpio_mode(nt50356_bl_info.nt50356_hw_en_gpio,
				GPIO_MODE_00);
			mt_set_gpio_dir(nt50356_bl_info.nt50356_hw_en_gpio,
				GPIO_DIR_OUT);
			mt_set_gpio_out(nt50356_bl_info.nt50356_hw_en_gpio,
				GPIO_OUT_ONE);
			if (nt50356_bl_info.bl_on_lk_mdelay)
				mdelay(nt50356_bl_info.bl_on_lk_mdelay);
	}

	ret = nt50356_config_register();
	if (ret < 0) {
		LCD_KIT_ERR("nt50356 config register failed\n");
		return;
	}
	nt50356_init_status = true;
}

static void nt50356_disable(void)
{
	if (nt50356_bl_info.nt50356_hw_en)
		mt_set_gpio_out(nt50356_bl_info.nt50356_hw_en_gpio,
				GPIO_OUT_ZERO);
	nt50356_init_status = false;
}
/*
 * nt50356_set_backlight_reg(): Set Backlight working mode
 *
 * @bl_level: value for backlight ,range from 0 to 2047
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
static int nt50356_set_backlight(uint32_t bl_level)
{
	int ret = NT50356_FAIL;
	uint32_t level;
	int bl_msb;
	int bl_lsb;

	/* first set backlight, enable nt50356 */
	if ((nt50356_init_status == false) && (bl_level > 0))
		nt50356_enable();

	/* map bl_level from 10000 to 2048 stage brightness */
	level = bl_level * nt50356_bl_info.bl_level / NT50356_BL_DEFAULT_LEVEL;

	if (level > BL_MAX)
		level = BL_MAX;

	/* 11-bit brightness code */
	bl_msb = level >> MSB;
	bl_lsb = level & LSB;

	LCD_KIT_INFO("level = %d, bl_msb = %d, bl_lsb = %d", level, bl_msb,
		bl_lsb);

	ret = nt50356_i2c_update_bits(NT50356_SLAV_ADDR, REG_BL_BRIGHTNESS_LSB,
		MASK_BL_LSB, bl_lsb);
	if (ret < 0) {
		goto i2c_error;
	}

	ret = nt50356_i2c_write_u8(NT50356_SLAV_ADDR, REG_BL_BRIGHTNESS_MSB,
		bl_msb);
	if (ret < 0) {
		goto i2c_error;
	}

	/* if set backlight level 0, disable nt50356 */
	if ((nt50356_init_status == true) && (bl_level == 0))
		nt50356_disable();

	return ret;

i2c_error:
	LCD_KIT_ERR("%s:i2c access fail to register", __func__);
	return ret;

}
