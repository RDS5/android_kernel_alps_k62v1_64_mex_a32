/*
 * nt50356.h
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

#ifndef __NT50356_H
#define __NT50356_H

#ifndef NT50356_OK
#define NT50356_OK 0
#endif

#ifndef NT50356_FAIL
#define NT50356_FAIL (-1)
#endif

#define NT50356_MODULE_NAME_STR             "nt50356"
#define NT50356_MODULE_NAME                  nt50356
#define NT50356_INVALID_VAL                  0xFFFF
#define NT50356_SLAV_ADDR                    0x11
#define NT50356_I2C_SPEED                    100
#define DTS_COMP_NT50356                     "nt,nt50356"
#define NT50356_SUPPORT                      "nt50356_support"
#define NT50356_I2C_BUS_ID                   "nt50356_i2c_bus_id"
#define NT50356_HW_LDSEN_GPIO                "nt50356_hw_ldsen_gpio"
#define NT50356_HW_EN_GPIO                   "nt50356_hw_en_gpio"
#define NT50356_HW_EN_DELAY                  "bl_on_lk_mdelay"
#define NT50356_BL_LEVEL                     "bl_level"
#define GPIO_NT50356_EN_NAME                 "nt50356_hw_enable"
#define NT50356_HIDDEN_REG_SUPPORT           "nt50356_hidden_reg_support"
#define NT50356_EXTEND_REV_SUPPORT           "nt50356_extend_rev_support"
#define NT50356_CONFIG1_STRING               "nt50356_bl_config_1"
#define NT50356_BL_EN_STRING                 "nt50356_bl_en"
#define MAX_STR_LEN                          50

/* base reg */
#define REG_REVISION                         0x01
#define REG_BL_CONFIG_1                      0x02
#define REG_BL_CONFIG_2                      0x03
#define REG_BL_BRIGHTNESS_LSB                0x04
#define REG_BL_BRIGHTNESS_MSB                0x05
#define REG_AUTO_FREQ_LOW                    0x06
#define REG_AUTO_FREQ_HIGH                   0x07
#define REG_BL_ENABLE                        0x08
#define REG_DISPLAY_BIAS_CONFIG_1            0x09
#define REG_DISPLAY_BIAS_CONFIG_2            0x0A
#define REG_DISPLAY_BIAS_CONFIG_3            0x0B
#define REG_LCM_BOOST_BIAS                   0x0C
#define REG_VPOS_BIAS                        0x0D
#define REG_VNEG_BIAS                        0x0E
#define REG_FLAGS                            0x0F
#define REG_BL_OPTION_1                      0x10
#define REG_BL_OPTION_2                      0x11
#define REG_BL_CURRENT_CONFIG                0x20
#define REG_MAX                              0x21
#define NT50356_WRITE_LEN                    2
/* add config reg for nt50356 TSD bug */
#define NT50356_REG_CONFIG_A9                0xA9
#define NT50356_REG_CONFIG_E8                0xE8
#define NT50356_REG_CONFIG_E9                0xE9

#define VENDOR_ID_NT50356                    0x01
#define DEV_MASK                             0x03
#define VENDOR_ID_EXTEND_NT50356             0xE0
#define DEV_MASK_EXTEND_NT50356              0xE0

/* mask mode */
#define MASK_CHANGE_MODE                     0xFF

#define NT50356_RAMP_EN                      0x02
#define NT50356_RAMP_DIS                     0x00

#define NT50356_PWN_CONFIG_HIGH              0x00
#define NT50356_PWN_CONFIG_LOW               0x04

#define MASK_BL_LSB                          0x07
#define BL_MIN                               0
#define BL_MAX                               2047
#define NT50356_RW_REG_MAX                   19
#define MASK_LCM_EN                          0xE0

#define MSB                                  3
#define LSB                                  0x07
#define OVP_SHUTDOWN_ENABLE                  0x10
#define OVP_SHUTDOWN_DISABLE                 0x00

#define REG_HIDDEN_ADDR                      0x6A
#define REG_SET_SECURITYBIT_ADDR             0x50
#define REG_SET_SECURITYBIT_VAL              0x08
#define REG_CLEAR_SECURITYBIT_VAL            0x00
#define LCD_KIT_OK                           0
#define CHECK_STATUS_FAIL                    0
#define CHECK_STATUS_OK                      1
#define LCD_BL_LINEAR_EXPONENTIAL_TABLE_NUM  2048

#define LINEAR_EXPONENTIAL_MASK              0x08

#define NT50356_BL_DEFAULT_LEVEL             255
#define NT50356_EXTEND_REV_VAL               1

static struct nt50356_backlight_information {
	/* whether support nt50356 or not */
	int nt50356_support;
	/* which i2c bus controller nt50356 mount */
	int nt50356_i2c_bus_id;
	/* nt50356 hw_ldsen gpio */
	unsigned int nt50356_hw_ldsen_gpio;
	/* nt50356 hw_en gpio */
	unsigned int nt50356_hw_en_gpio;
	unsigned int nt50356_hw_en;
	unsigned int bl_on_lk_mdelay;
	unsigned int bl_level;
	int nt50356_reg[NT50356_RW_REG_MAX];
} nt50356_bl_info;

static char *nt50356_dts_string[NT50356_RW_REG_MAX] = {
	"nt50356_bl_config_1",
	"nt50356_bl_config_2",
	"nt50356_bl_brightness_lsb",
	"nt50356_bl_brightness_msb",
	"nt50356_auto_freq_low",
	"nt50356_auto_freq_high",
	"nt50356_reg_config_e8",
	"nt50356_reg_config_e9",
	"nt50356_reg_config_a9",
	"nt50356_display_bias_config_1",
	"nt50356_display_bias_config_2",
	"nt50356_display_bias_config_3",
	"nt50356_lcm_boost_bias",
	"nt50356_vpos_bias",
	"nt50356_vneg_bias",
	"nt50356_bl_option_1",
	"nt50356_bl_option_2",
	"nt50356_bl_current_config",
	"nt50356_bl_en",
};

static int nt50356_reg_addr[NT50356_RW_REG_MAX] = {
	REG_BL_CONFIG_1,
	REG_BL_CONFIG_2,
	REG_BL_BRIGHTNESS_LSB,
	REG_BL_BRIGHTNESS_MSB,
	REG_AUTO_FREQ_LOW,
	REG_AUTO_FREQ_HIGH,
	NT50356_REG_CONFIG_E8,
	NT50356_REG_CONFIG_E9,
	NT50356_REG_CONFIG_A9,
	REG_DISPLAY_BIAS_CONFIG_1,
	REG_DISPLAY_BIAS_CONFIG_2,
	REG_DISPLAY_BIAS_CONFIG_3,
	REG_LCM_BOOST_BIAS,
	REG_VPOS_BIAS,
	REG_VNEG_BIAS,
	REG_BL_OPTION_1,
	REG_BL_OPTION_2,
	REG_BL_CURRENT_CONFIG,
	REG_BL_ENABLE,
};

enum nt50356_bl_ovp {
	NT50356_BL_OVP_17 = 0x00,
	NT50356_BL_OVP_21 = 0x20,
	NT50356_BL_OVP_25 = 0x40,
	NT50356_BL_OVP_29 = 0x60,
};

enum nt50356_bled_mode {
	NT50356_BLED_MODE_EXPONETIAL = 0x00,
	NT50356_BLED_MODE_LINEAR = 0x08,
};

enum {
	NT50356_BIAS_BOOST_TIME_0 = 0x00, /* 156ns */
	NT50356_BIAS_BOOST_TIME_1 = 0x10, /* 181ns */
	NT50356_BIAS_BOOST_TIME_2 = 0x20, /* 206ns */
	NT50356_BIAS_BOOST_TIME_3 = 0x30, /* 231ns */
};

static int nt50356_set_backlight(uint32_t bl_level);
int nt50356_init(void);
void nt50356_set_backlight_status (void);
#endif /* __NT50356_H */
