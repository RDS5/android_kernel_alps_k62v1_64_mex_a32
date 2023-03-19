

#ifndef __KTZ8864_H
#define __KTZ8864_H

#include <libfdt.h>
#include <platform/mt_i2c.h>
#include <platform/mt_gpio.h>

#ifndef SUCC
#define SUCC 0
#endif

#ifndef FAIL
#define FAIL (-1)
#endif

#define KTZ8864_MODULE_NAME_STR     "ktz8864"
#define KTZ8864_MODULE_NAME         ktz8864
#define KTZ8864_INVALID_VAL         0xFFFF
#define KTZ8864_SLAV_ADDR           0x11
#define KTZ8864_I2C_SPEED           100
#define DTS_COMP_KTZ8864            "ktz,ktz8864"
#define KTZ8864_SUPPORT             "ktz8864_support"
#define KTZ8864_I2C_BUS_ID          "ktz8864_i2c_bus_id"
#define GPIO_KTZ8864_LDSEN_NAME     "ktz8864_hw_ldsen"
#define KTZ8864_EN_NAME             "ktz8864_hw_enable"
#define KTZ8864_HW_LDSEN_GPIO       "ktz8864_hw_ldsen_gpio"
#define KTZ8864_HW_EN_GPIO          "ktz8864_hw_en_gpio"
#define KTZ8864_HW_EN_DELAY         "bl_on_lk_mdelay"
#define KTZ8864_BL_LEVEL            "bl_level"
#define KTZ8864_HIDDEN_REG_SUPPORT  "ktz8864_hidden_reg_support"
#define MAX_STR_LEN                 50
#define KTZ8864_BL_DEFAULT_LEVEL    255

/* base reg */
#define PWM_DISABLE                 0x00
#define PWM_ENABLE                  0x01
#define REG_REVISION                0x01
#define REG_BL_CONFIG_1             0x02
#define REG_BL_CONFIG_2             0x03
#define REG_BL_BRIGHTNESS_LSB       0x04
#define REG_BL_BRIGHTNESS_MSB       0x05
#define REG_AUTO_FREQ_LOW           0x06
#define REG_AUTO_FREQ_HIGH          0x07
#define REG_BL_ENABLE               0x08
#define REG_DISPLAY_BIAS_CONFIG_1   0x09
#define REG_DISPLAY_BIAS_CONFIG_2   0x0A
#define REG_DISPLAY_BIAS_CONFIG_3   0x0B
#define REG_LCM_BOOST_BIAS          0x0C
#define REG_VPOS_BIAS               0x0D
#define REG_VNEG_BIAS               0x0E
#define REG_FLAGS                   0x0F
#define REG_BL_OPTION_1             0x10
#define REG_BL_OPTION_2             0x11
#define REG_PWM_TO_DIGITAL_LSB      0x12
#define REG_PWM_TO_DIGITAL_MSB      0x13
#define REG_MAX                     0x14
#define VENDOR_ID_KTZ               0x02
#define DEV_MASK                    0x03
#define KTZ8864_RW_REG_MAX          15
#define KTZ8864_WRITE_LEN           2
// mask mode
#define MASK_CHANGE_MODE            0xFF
#define KTZ8864_RAMP_EN             0x02
#define KTZ8864_RAMP_DIS            0x00
#define KTZ8864_PWN_CONFIG_HIGH     0x00
#define KTZ8864_PWN_CONFIG_LOW      0x04
#define MASK_BL_LSB                 0x07
#define BL_MIN                      0
#define BL_MAX                      2047
#define MASK_LCM_EN                 0xE0
#define MSB                         3
#define LSB                         0x07
#define OVP_SHUTDOWN_ENABLE         0x10
#define OVP_SHUTDOWN_DISABLE        0x00
#define REG_HIDDEN_ADDR             0x6A
#define REG_SET_SECURITYBIT_ADDR    0x50
#define REG_SET_SECURITYBIT_VAL     0x08
#define REG_CLEAR_SECURITYBIT_VAL   0x00
#define LINEAR_EXPONENTIAL_MASK     0x08
#define LCD_KIT_OK                  0

#define CHECK_STATUS_FAIL           0
#define CHECK_STATUS_OK             1

static struct backlight_information {
	/* whether support ktz8864 or not */
	int ktz8864_support;
	/* which i2c bus controller ktz8864 mount */
	int ktz8864_i2c_bus_id;
	/* ktz8864 hw_ldsen gpio */
	unsigned int ktz8864_hw_ldsen_gpio;
	/* ktz8864 hw_en gpio */
	unsigned int ktz8864_hw_en;
	unsigned int ktz8864_hw_en_gpio;
	unsigned int bl_on_lk_mdelay;
	unsigned int bl_level;
	int ktz8864_reg[KTZ8864_RW_REG_MAX];
};

enum lcm_en {
	NORMAL_MODE = 0x80,
	BIAS_SUPPLY_OFF = 0x00,
};

enum ktz8864_bl_ovp {
	BL_OVP_17 = 0x00,
	BL_OVP_21 = 0x20,
	BL_OVP_25 = 0x40,
	BL_OVP_29 = 0x60,
};

enum ktz8864_bled_mode {
	KTZ8864_BLED_MODE_EXPONETIAL = 0x00,
	KTZ8864_BLED_MODE_LINEAR = 0x08,
};

enum {
	BIAS_BOOST_TIME_0 = 0x00, /* 156ns */
	BIAS_BOOST_TIME_1 = 0x10, /* 181ns */
	BIAS_BOOST_TIME_2 = 0x20, /* 206ns */
	BIAS_BOOST_TIME_3 = 0x30, /* 231ns */
};

void ktz8864_set_backlight_status(void);
int ktz8864_init(void);

#endif /* __KTZ8864_H */
