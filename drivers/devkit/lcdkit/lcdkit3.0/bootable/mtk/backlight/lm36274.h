/*
* Simple driver for Texas Instruments LM3630 LED Flash driver chip
* Copyright (C) 2012 Texas Instruments
*
*/

#ifndef __LM36274_H
#define __LM36274_H

#ifndef LCD_KIT_OK
#define LCD_KIT_OK 0
#endif

#ifndef LCD_KIT_FAIL
#define LCD_KIT_FAIL (-1)
#endif

#define LM36274_MODULE_NAME_STR     "lm36274"
#define LM36274_MODULE_NAME          lm36274
#define LM36274_INVALID_VAL          0xFFFF
#define LM36274_SLAV_ADDR            0x11
#define LM36274_I2C_SPEED            100

#define DTS_COMP_LM36274             "ti,lm36274"
#define LM36274_SUPPORT              "lm36274_support"
#define LM36274_I2C_BUS_ID           "lm36274_i2c_bus_id"
#define LM36274_HW_LDSEN_GPIO        "lm36274_hw_ldsen_gpio"
#define LM36274_HW_EN_GPIO           "lm36274_hw_en_gpio"
#define GPIO_LM36274_LDSEN_NAME      "lm36274_hw_ldsen"
#define GPIO_LM36274_EN_NAME         "lm36274_hw_enable"
#define LM36274_HW_EN_DELAY          "bl_on_lk_mdelay"
#define LM36274_BL_LEVEL             "bl_level"
#define LM36274_HIDDEN_REG_SUPPORT   "lm36274_hidden_reg_support"
#define LM36274_EXTEND_REV_SUPPORT   "lm36274_extend_rev_support"
#define LM36274_ONLY_BIAS            "only_bias"
#define MAX_STR_LEN	                 50
#define LM36274_WRITE_LEN            2

/* base reg */
#define PWM_DISABLE                  0x00
#define PWM_ENABLE                   0x01
#define REG_REVISION                 0x01
#define REG_BL_CONFIG_1              0x02
#define REG_BL_CONFIG_2              0x03
#define REG_BL_BRIGHTNESS_LSB        0x04
#define REG_BL_BRIGHTNESS_MSB        0x05
#define REG_AUTO_FREQ_LOW            0x06
#define REG_AUTO_FREQ_HIGH           0x07
#define REG_BL_ENABLE                0x08
#define REG_DISPLAY_BIAS_CONFIG_1    0x09
#define REG_DISPLAY_BIAS_CONFIG_2    0x0A
#define REG_DISPLAY_BIAS_CONFIG_3    0x0B
#define REG_LCM_BOOST_BIAS           0x0C
#define REG_VPOS_BIAS                0x0D
#define REG_VNEG_BIAS                0x0E
#define REG_FLAGS                    0x0F
#define REG_BL_OPTION_1              0x10
#define REG_BL_OPTION_2              0x11
#define REG_PWM_TO_DIGITAL_LSB       0x12
#define REG_PWM_TO_DIGITAL_MSB       0x13

//mask mode
#define MASK_CHANGE_MODE             0xFF

#define LM36274_RAMP_EN              0x02
#define LM36274_RAMP_DIS             0x00

#define LM36274_PWN_CONFIG_HIGH      0x00
#define LM36274_PWN_CONFIG_LOW       0x04

#define MASK_BL_LSB                  0x07
#define BL_MIN                       0
#define BL_MAX                       2047
#define LM36274_RW_REG_MAX           15
#define MASK_LCM_EN                  0xE0

#define MSB                          3
#define LSB                          0x07
#define OVP_SHUTDOWN_ENABLE          0x10
#define OVP_SHUTDOWN_DISABLE         0x00

#define REG_HIDDEN_ADDR              0x6A
#define REG_SET_SECURITYBIT_ADDR     0x50
#define REG_SET_SECURITYBIT_VAL      0x08
#define REG_CLEAR_SECURITYBIT_VAL    0x00

#define VENDOR_ID_TI                 0x01
#define DEV_MASK                     0x03
#define VENDOR_ID_EXTEND_LM32674     0xE0
#define DEV_MASK_EXTEND_LM32674      0xE0
#define LM36274_EXTEND_REV_VAL       1

#define CHECK_STATUS_FAIL            0
#define CHECK_STATUS_OK              1
#define REG_MAX                      0x14

#define LM36274_BL_DEFAULT_LEVEL     255

#define LM36274_VOL_400 (0x00) //4.0V
#define LM36274_VOL_405 (0x01) //4.05V
#define LM36274_VOL_410 (0x02) //4.1V
#define LM36274_VOL_415 (0x03) //4.15V
#define LM36274_VOL_420 (0x04) //4.2V
#define LM36274_VOL_425 (0x05) //4.25V
#define LM36274_VOL_430 (0x06) //4.3V
#define LM36274_VOL_435 (0x07) //4.35V
#define LM36274_VOL_440 (0x08) //4.4V
#define LM36274_VOL_445 (0x09) //4.45V
#define LM36274_VOL_450 (0x0A) //4.5V
#define LM36274_VOL_455 (0x0B) //4.55V
#define LM36274_VOL_460 (0x0C) //4.6V
#define LM36274_VOL_465 (0x0D) //4.65V
#define LM36274_VOL_470 (0x0E) //4.7V
#define LM36274_VOL_475 (0x0F) //4.75V
#define LM36274_VOL_480 (0x10) //4.8V
#define LM36274_VOL_485 (0x11) //4.85V
#define LM36274_VOL_490 (0x12) //4.9V
#define LM36274_VOL_495 (0x13) //4.95V
#define LM36274_VOL_500 (0x14) //5.0V
#define LM36274_VOL_505 (0x15) //5.05V
#define LM36274_VOL_510 (0x16) //5.1V
#define LM36274_VOL_515 (0x17) //5.15V
#define LM36274_VOL_520 (0x18) //5.2V
#define LM36274_VOL_525 (0x19) //5.25V
#define LM36274_VOL_560 (0x20) //5.6V
#define LM36274_VOL_565 (0x21) //5.65V
#define LM36274_VOL_570 (0x22) //5.7V
#define LM36274_VOL_575 (0x23) //5.75V
#define LM36274_VOL_580 (0x24) //5.8V
#define LM36274_VOL_585 (0x25) //5.85V
#define LM36274_VOL_590 (0x26) //5.9V
#define LM36274_VOL_595 (0x27) //5.95V
#define LM36274_VOL_600 (0x28) //6.0V
#define LM36274_VOL_605 (0x29) //6.05V
#define LM36274_VOL_640 (0x30) //6.4V
#define LM36274_VOL_645 (0x31) //6.45V
#define LM36274_VOL_650 (0x32) //6.5V


struct lm36274_vsp_vsn_voltage {
	u32 voltage;
	int value;
};

static struct lm36274_backlight_information {
	/* whether support lm36274 or not */
	int lm36274_support;
	/* which i2c bus controller lm36274 mount */
	int lm36274_i2c_bus_id;
	/* lm36274 hw_ldsen gpio */
	int lm36274_hw_ldsen_gpio;
	/* lm36274 hw_en gpio */
	int lm36274_hw_en;
	int lm36274_hw_en_gpio;
	int bl_on_lk_mdelay;
	int bl_level;
	int lm36274_reg[LM36274_RW_REG_MAX];
} ;

static char *lm36274_dts_string[LM36274_RW_REG_MAX] = {
	"lm36274_bl_config_1",
	"lm36274_bl_config_2",
	"lm36274_bl_brightness_lsb",
	"lm36274_bl_brightness_msb",
	"lm36274_auto_freq_low",
	"lm36274_auto_freq_high",
	"lm36274_display_bias_config_1",
	"lm36274_display_bias_config_2",
	"lm36274_display_bias_config_3",
	"lm36274_lcm_boost_bias",
	"lm36274_vpos_bias",
	"lm36274_vneg_bias",
	"lm36274_bl_option_1",
	"lm36274_bl_option_2",
	"lm36274_bl_en",
};

static u8 lm36274_reg_addr[LM36274_RW_REG_MAX] = {
	REG_BL_CONFIG_1,
	REG_BL_CONFIG_2,
	REG_BL_BRIGHTNESS_LSB,
	REG_BL_BRIGHTNESS_MSB,
	REG_AUTO_FREQ_LOW,
	REG_AUTO_FREQ_HIGH,
	REG_DISPLAY_BIAS_CONFIG_1,
	REG_DISPLAY_BIAS_CONFIG_2,
	REG_DISPLAY_BIAS_CONFIG_3,
	REG_LCM_BOOST_BIAS,
	REG_VPOS_BIAS,
	REG_VNEG_BIAS,
	REG_BL_OPTION_1,
	REG_BL_OPTION_2,
	REG_BL_ENABLE,
};

enum lm36274_bl_ovp {
	LM36274_BL_OVP_17 = 0x00,
	LM36274_BL_OVP_21 = 0x20,
	LM36274_BL_OVP_25 = 0x40,
	LM36274_BL_OVP_29 = 0x60,
};

enum {
	LM36274_BIAS_BOOST_TIME_0 = 0x00, /* 156ns */
	LM36274_BIAS_BOOST_TIME_1 = 0x10, /* 181ns */
	LM36274_BIAS_BOOST_TIME_2 = 0x20, /* 206ns */
	LM36274_BIAS_BOOST_TIME_3 = 0x30, /* 231ns */
};

enum lm36274_bled_mode {
	LM36274_BLED_MODE_EXPONETIAL = 0x00,
	LM36274_BLED_MODE_LINEAR = 0x08,
};

static int lm36274_set_backlight(uint32_t bl_level);
int lm36274_init(void);

#endif /* __LM36274_H */
