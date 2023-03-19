#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "lens_info.h"

#define DW9714V_STATUS_REG    (0x05)
#define DW9714V_VCM_CFG_REG  (0x06)
#define DW9714V_VCM_IC_INFO_REG   (0x00)
#define DW9714V_VCM_IC_INFO_DEFAULT   (0xFE)
#define DW9714V_BUSY_STATE_MASK  (0x01)
#define AF_REINIT_ESD_CHECK   (10)
#define DW9714V_STATUS_DELAY (2000)
enum AF_DRIVING_MODE_IDX {
	DRIVING_MODE_DIRECT_MODE=0,
	DRIVING_MODE_SAC_MODE,
};
struct af_i2c_reg hi1333_qtech_init_setting[] = {
	{0xED, 0xAB, 0x00},
	{0x02, 0x01, 0x00},
	{0x02, 0x00, 0x01},
	{0x06, 0x84, 0x00},
	{0x07, 0x01, 0x00},
	{0x08, 0x59, 0x00},
};

struct af_i2c_reg hi1336_qtech_init_setting[] = {
	{ 0xED, 0xAB, 0x00 },
	{ 0x02, 0x01, 0x00 },
	{ 0x02, 0x00, 0x01 },
	{ 0x06, 0x84, 0x00 },
	{ 0x07, 0x01, 0x00 },
	{ 0x08, 0x59, 0x00 },
};

struct af_i2c_reg imx258_sunny_init_setting[] = {
	{0xED, 0xAB, 0x00},
	{0x02, 0x01, 0x00},
	{0x02, 0x00, 0x01},
	{0x06, 0x84, 0x00},
	{0x07, 0x01, 0x00},
	{0x08, 0x59, 0x00},
};

struct af_i2c_reg imx258_sunny_zet_init_setting[] = {
	{0xED, 0xAB, 0x00},
	{0x02, 0x01, 0x00},
	{0x02, 0x00, 0x01},
	{0x06, 0x84, 0x00},
	{0x07, 0x01, 0x00},
	{0x08, 0x59, 0x00},
};

struct af_i2c_reg ov13855_oflim_init_setting[] = {
	{0xED, 0xAB, 0x00},
	{0x02, 0x01, 0x00},
	{0x02, 0x00, 0x01},
	{0x06, 0x84, 0x00},
	{0x07, 0x01, 0x00},
	{0x08, 0x4B, 0x00},
};

struct af_i2c_reg ov13855_oflim_tdk_init_setting[] = {
	{0xED, 0xAB, 0x00},
	{0x02, 0x01, 0x00},
	{0x02, 0x00, 0x01},
	{0x06, 0x84, 0x00},
	{0x07, 0x01, 0x00},
	{0x08, 0x4B, 0x00},
};

struct af_i2c_reg s5k3l6_liteon_init_setting[] = {
	{0xED, 0xAB, 0x00},
	{0x02, 0x01, 0x00},
	{0x02, 0x00, 0x01},
	{0x06, 0x84, 0x00},
	{0x07, 0x01, 0x00},
	{0x08, 0x59, 0x00},
};

struct af_i2c_reg s5k3l6_truly_init_setting[] = {
	{0xED, 0xAB, 0x00},
	{0x02, 0x01, 0x00},
	{0x02, 0x00, 0x01},
	{0x06, 0x84, 0x00},
	{0x07, 0x01, 0x00},
	{0x08, 0x59, 0x00},
};

struct af_i2c_reg hi846_truly_init_setting[] = {
	{0xED, 0xAB, 0x00},
	{0x02, 0x01, 0x00},
	{0x02, 0x00, 0x01},
	{0x06, 0x84, 0x00},
	{0x07, 0x01, 0x00},
	{0x08, 0x59, 0x00},
};

struct af_i2c_reg gc8034_sunwin_init_setting[] = {
	{0xED, 0xAB, 0x00},
	{0x02, 0x01, 0x00},
	{0x02, 0x00, 0x01},
	{0x06, 0x84, 0x00},
	{0x07, 0x01, 0x00},
	{0x08, 0x59, 0x00},
};

struct af_i2c_reg gc8034_txd_init_setting[] = {
	{0xED, 0xAB, 0x00},
	{0x02, 0x01, 0x00},
	{0x02, 0x00, 0x01},
	{0x06, 0x84, 0x00},
	{0x07, 0x01, 0x00},
	{0x08, 0x59, 0x00},
};

struct af_i2c_reg ov8856_hlt_init_setting[] = {
	{0xED, 0xAB, 0x00},
	{0x02, 0x01, 0x00},
	{0x02, 0x00, 0x01},
	{0x06, 0x84, 0x00},
	{0x07, 0x01, 0x00},
	{0x08, 0x59, 0x00},
};

struct af_i2c_reg s5k4h7_txd_init_setting[] = {
	{0xED, 0xAB, 0x00},
	{0x02, 0x01, 0x00},
	{0x02, 0x00, 0x01},
	{0x06, 0x84, 0x00},
	{0x07, 0x01, 0x00},
	{0x08, 0x59, 0x00},
};

struct af_exit_reg hi1333_qtech_exit_setting[] = {
	{320, 13},
	{150, 13},
	{100, 13},
};

struct af_exit_reg hi1336_qtech_exit_setting[] = {
	{ 320, 13 },
	{ 150, 13 },
	{ 100, 13 },
};

struct af_exit_reg imx258_sunny_exit_setting[] = {
	{320, 13},
	{150, 13},
	{100, 13},
};

struct af_exit_reg imx258_sunny_zet_exit_setting[] = {
	{280, 13},
	{150, 13},
	{100, 13},
};

struct af_exit_reg ov13855_oflim_exit_setting[] = {
	{250, 12},
	{150, 12},
	{100, 12},
};

struct af_exit_reg ov13855_oflim_tdk_exit_setting[] = {
	{280, 12},
	{150, 12},
	{100, 12},
};

struct af_exit_reg s5k3l6_liteon_exit_setting[] = {
	{200, 13},
	{150, 13},
	{100, 13},
};

struct af_exit_reg s5k3l6_truly_exit_setting[] = {
	{200, 13},
	{150, 13},
	{100, 13},
};

struct af_exit_reg hi846_truly_exit_setting[] = {
	{200, 13},
	{150, 13},
	{100, 13},
};

struct af_exit_reg gc8034_sunwin_exit_setting[] = {
	{200, 13},
	{150, 13},
	{100, 13},
};

struct af_exit_reg gc8034_txd_exit_setting[] = {
	{200, 13},
	{150, 13},
	{100, 13},
};

struct af_exit_reg ov8856_hlt_exit_setting[] = {
	{200, 13},
	{150, 13},
	{100, 13},
};

struct af_exit_reg s5k4h7_txd_exit_setting[] = {
	{200, 13},
	{150, 13},
	{100, 13},
};

struct af_i2c_reg af_reset_setting[] = {
	{0x80, 0x00, 0x00},
	{0x00, 0x01, 0x01},
};
struct af_i2c_reg sac_mode_power_off_setting[] = {
	{0x02, 0x01, 0x00},
};
struct af_i2c_reg direct_mode_power_off_setting[] = {
	{0x08, 0x00, 0x00},
	{0x80, 0x00, 0x00},
};

