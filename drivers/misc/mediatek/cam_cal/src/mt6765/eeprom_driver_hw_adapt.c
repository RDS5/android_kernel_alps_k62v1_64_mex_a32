/*
 * eeprom_driver_hw_adapt.c
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

#define PFX "eeprom_adapt"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include "eeprom_driver_hw_adapt.h"
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <securec.h>
#include "eeprom_i2c_common_driver.h"
#include "imgsensor_sensor_i2c.h"
#include "cam_cal_eeprom_type_def/cam_cal_eeprom_type_def.h"

static DEFINE_MUTEX(sensor_eeprom_mutex);
#define SENSOR_NAME_LEN   32
#define OTP_FILE_PATH_LEN 128
extern int camcal_read_hi846_sensorotp_func(struct i2c_client *current_client,
			unsigned int sensor_id, unsigned char **eeprom_buff,
			unsigned int *eeprom_size);
extern int camcal_read_gc8034_sensorotp_func(struct i2c_client *current_client,
			unsigned int sensor_id, unsigned char **eeprom_buff,
			unsigned int *eeprom_size);
extern int camcal_read_ov8856_sensorotp_func(struct i2c_client *current_client,
			unsigned int sensor_id, unsigned char **eeprom_buff,
			unsigned int *eeprom_size);
extern int camcal_read_s5k4h7_sensorotp_func(struct i2c_client *current_client,
			unsigned int sensor_id, unsigned char **eeprom_buff,
			unsigned int *eeprom_size);
static int camcal_read_eeprom_func(struct i2c_client *current_Client,
			unsigned int sensor_id,
			unsigned char **eeprom_buff,
			unsigned int *eeprom_size);
static struct cam_cal_map g_cam_cal_map_info[] = {
	{
		HI1333_QTECH_SENSOR_ID,             SENSOR_DEV_MAIN, 0xA0,
		NULL, 0, camcal_read_eeprom_func,   false
	},
	{
		OV13855_OFILM_TDK_SENSOR_ID,        SENSOR_DEV_MAIN, 0xA0,
		NULL, 0, camcal_read_eeprom_func,   false
	},
	{
		OV13855_OFILM_SENSOR_ID,             SENSOR_DEV_MAIN, 0xA0,
		NULL, 0, camcal_read_eeprom_func,   false
	},
	{
		S5K3L6_LITEON_SENSOR_ID,            SENSOR_DEV_MAIN, 0xA2,
		NULL, 0, camcal_read_eeprom_func,   false
	},
	{
		HI1336_QTECH_SENSOR_ID,             SENSOR_DEV_MAIN, 0xA0,
		NULL, 0, camcal_read_eeprom_func,   false
	},
	{
		S5K4H7_TRULY_SENSOR_ID,             SENSOR_DEV_SUB, 0xA0,
		NULL, 0, camcal_read_eeprom_func,   false
	},
	{
		HI846_LUXVISIONS_SENSOR_ID,         SENSOR_DEV_SUB, 0xA0,
		NULL, 0, camcal_read_eeprom_func,   false
	},
	{
		HI846_OFILM_SENSOR_ID,              SENSOR_DEV_SUB, 0xA0,
		NULL, 0, camcal_read_eeprom_func,   false
	},
	{
		GC8054_BYD_SENSOR_ID,               SENSOR_DEV_SUB, 0xA0,
		NULL, 0, camcal_read_eeprom_func,   false
	},
	{
		GC2375_SUNNY_MONO_SENSOR_ID,        SENSOR_DEV_MAIN_2, 0xA8,
		NULL, 0, camcal_read_eeprom_func,   false
	},
	{
		GC2375_SUNNY_SENSOR_ID,             SENSOR_DEV_MAIN_2, 0xA2,
		NULL, 0, camcal_read_eeprom_func,   false
	},
	{
		OV02A10_FOXCONN_SENSOR_ID,          SENSOR_DEV_MAIN_2, 0xA2,
		NULL, 0, camcal_read_eeprom_func,   false
	},
	{
		OV02A10_SUNWIN_SENSOR_ID,           SENSOR_DEV_MAIN_2, 0xA2,
		NULL, 0, camcal_read_eeprom_func,   false
	},
	{
		S5K5E9YX_BYD_SENSOR_ID,             SENSOR_DEV_SUB_2, 0xA4,
		NULL, 0, camcal_read_eeprom_func,   false
	},
	{
		GC5035_LUXVISIONS_SENSOR_ID,        SENSOR_DEV_SUB_2, 0xA4,
		NULL, 0, camcal_read_eeprom_func,   false
	}, {
		IMX258_SENSOR_ID,                   SENSOR_DEV_MAIN, 0xA0,
		NULL, 0, camcal_read_eeprom_func,   false
	}, {
		IMX258_ZET_SENSOR_ID,               SENSOR_DEV_MAIN, 0xA0,
		NULL, 0, camcal_read_eeprom_func,   false
	}, {
		S5K3L6_TRULY_SENSOR_ID,             SENSOR_DEV_MAIN, 0xA0,
		NULL, 0, camcal_read_eeprom_func,   false
	}, {
		HI846_TRULY_SENSOR_ID,              SENSOR_DEV_MAIN, 0x46,
		NULL, 0, camcal_read_hi846_sensorotp_func,   false
	}, {
		GC8034_SUNWIN_SENSOR_ID,              SENSOR_DEV_MAIN, 0x6E,
		NULL, 0, camcal_read_gc8034_sensorotp_func,   false
	}, {
		GC8034_TXD_SENSOR_ID,              SENSOR_DEV_MAIN, 0x6E,
		NULL, 0, camcal_read_gc8034_sensorotp_func,   false
	}, {
		OV8856_HLT_SENSOR_ID,              SENSOR_DEV_MAIN, 0x20,
		NULL, 0, camcal_read_ov8856_sensorotp_func,   false
	}, {
		S5K4H7_TXD_SENSOR_ID,             SENSOR_DEV_MAIN, 0x20,
		NULL, 0, camcal_read_s5k4h7_sensorotp_func,   false
	},
};

static int get_odm_eeprom_path(MUINT32 sensor_id, char *odm_path, MUINT32 len)
{
	unsigned int camcal_mtk_map_size;
	int device_id = -1;
	char *path_temp = NULL;
	unsigned char driver_name[SENSOR_NAME_LEN];
	int path_size;
	int i;
	int ret;

	camcal_mtk_map_size = ARRAY_SIZE(g_cam_cal_map_info);
	for (i = 0; i < camcal_mtk_map_size; i++) {
		if (sensor_id == g_cam_cal_map_info[i].sensor_id) {
			device_id = g_cam_cal_map_info[i].device_id;
			break;
		}
	}
	switch (device_id) {
	case SENSOR_DEV_MAIN:
		path_temp = "/odm/lib/camera/golden_otp/main/";
		break;
	case SENSOR_DEV_SUB:
		path_temp = "/odm/lib/camera/golden_otp/sub/";
		break;
	case SENSOR_DEV_MAIN_2:
		path_temp = "/odm/lib/camera/golden_otp/main2/";
		break;
	case SENSOR_DEV_SUB_2:
		path_temp = "/odm/lib/camera/golden_otp/sub2/";
		break;
	default:
		pr_err("wrong devcie id [%d].\n", device_id);
		return -1;
	}
	if (memset_s(driver_name, SENSOR_NAME_LEN, '\0', SENSOR_NAME_LEN) != 0)
		return -1;
	if (get_driver_name(sensor_id, driver_name, SENSOR_NAME_LEN - 1) < 0)
		return -1;
	path_size = strlen(path_temp) + strlen(driver_name);
	ret = snprintf_s(odm_path, len, path_size,
		"%s%s", path_temp, driver_name);
	if (ret < 0) {
		pr_err("get path error: %s\n", driver_name);
		return -1;
	}
	return path_size;
}

static int check_file_size(char *path, MUINT32 len)
{
	mm_segment_t fs;
	struct kstat *stat = NULL;
	int size;
	int ret;

	if (strlen(path) != len) {
		pr_err("odm path is err/n");
		return -1;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	stat = kmalloc(sizeof(struct kstat), GFP_KERNEL);
	if (!stat) {
		pr_err("kmalloc state of file buf err/n");
		return -1;
	}
	ret = vfs_stat(path, stat);
	if (ret) {
		kfree(stat);
		set_fs(fs);
		pr_err("can't get state of file err=%d/n", ret);
		return -1;
	}
	size = stat->size;
	kfree(stat);
	set_fs(fs);
	return size;
}

static unsigned int read_odm_file(struct file *fd, unsigned char *data,
	unsigned int data_addr, unsigned int read_len, unsigned int file_size)
{
	mm_segment_t fs;
	loff_t pos;
	unsigned int read_size;

	if (read_len + data_addr > file_size) {
		pr_err("odm eeprom size err, Expect:%d, Actual:%d\n",
			(read_len + data_addr), file_size);
		return 0;
	}
	pos = data_addr;
	fs = get_fs();
	set_fs(KERNEL_DS);
	read_size = vfs_read(fd, data, read_len, &pos);
	if (read_size != read_len) {
		pr_err("odm eeprom Read failed read_size[%d], data_size[%d]!\n",
			read_size, read_len);
		set_fs(fs);
		return 0;
	}
	set_fs(fs);
	return read_size;
}

static unsigned int eeprom_read_odm(unsigned int sensor_id,
	unsigned int data_addr, unsigned int read_len, unsigned char *data)
{
	struct file *fd = NULL;
	unsigned int read_size;
	int file_size;
	int path_size;
	char path[OTP_FILE_PATH_LEN];

	if (memset_s(path, OTP_FILE_PATH_LEN, '\0', OTP_FILE_PATH_LEN) != 0) {
		pr_err("creat odm path buf error.\n");
		return 0;
	}
	path_size = get_odm_eeprom_path(sensor_id, path, OTP_FILE_PATH_LEN);
	if (path_size < 0) {
		pr_err("odm path error:%s\n", path);
		return 0;
	}
	fd = filp_open(path, O_RDONLY, 0644);
	if (IS_ERR(fd)) {
		pr_err("odm eeprom open error: %s\n", path);
		return 0;
	}
	file_size = check_file_size(path, path_size);
	if (file_size < 0) {
		filp_close(fd, NULL);
		pr_err("odm eeprom check_file_size error\n");
		return 0;
	}
	read_size = read_odm_file(fd, data, data_addr, read_len, file_size);
	filp_close(fd, NULL);
	if (read_size == 0) {
		pr_err("odm eeprom Read failed read_size[%d], data_size[%d]!\n",
			read_size, read_len);
		return 0;
	}
	return read_size;
}

/*
 * Func Name     : eeprom_read_region
 * Func Describe : read otp data via I2C
 * return value  : error:0 , other:OK
 */
unsigned int eeprom_read_region(struct i2c_client *current_Client,
	unsigned int data_addr, unsigned int read_len, unsigned char *data)
{
	unsigned int read_size = 0;
	unsigned int read_data_length = read_len;
	unsigned int data_offset = 0;
	unsigned int addr_offset = data_addr;
	unsigned char *read_buff = data;
	unsigned int data_size = 0;
	unsigned char i = 0;

	if (!current_Client || !data) {
		pr_err("ptr is null!!\n");
		return 0;
	}

	/* read 8 byte every time, and retry 3 times when read fail */
	do {
		data_size = (read_data_length > EEPROM_READ_DATA_SIZE_8) ?
			   EEPROM_READ_DATA_SIZE_8 : read_data_length;

		read_size = Common_read_region(current_Client, addr_offset,
			read_buff, data_size);
		if (read_size != data_size) {
			for (i = 0; i < EEPROM_READ_FAIL_RETRY_TIMES; i++) {
				pr_err("I2C ReadData failed [0x%x], retry [%d]!!\n",
					addr_offset, i);
				read_size = Common_read_region(current_Client,
					addr_offset, read_buff, data_size);
				if (read_size == data_size)
					break;
			}
		}

		if (read_size != data_size) {
			pr_err("I2C ReadData failed read_size [%d], data_size [%d]!!\n",
				read_size, data_size);
			return 0;
		}

		data_offset += data_size;
		read_data_length -= data_size;
		addr_offset = data_addr + data_offset;
		read_buff = data + data_offset;
	} while (read_data_length > 0);

	return read_len;
}

/*
 * Func Name     : camcal_otp_set_sensor_setting
 * Func Describe : set setting to sensor
 * return value  : error: -1 , other: OK
 */
int camcal_otp_set_sensor_setting(struct imgsensor_t *pstimgsensor,
		struct imgsensor_i2c_reg_setting *pst_reg_setting)
{
	int rc = 0;

	if (!pstimgsensor || !pst_reg_setting) {
		pr_err("pstimgsensor or pst_reg_setting is null!!\n");
		return -1;
	}

	if (pst_reg_setting->setting != 0) {
		rc = imgsensor_sensor_write_setting(pstimgsensor,
				pst_reg_setting);
		if (rc < 0) {
			pr_err("write sensor Failed\n");
			return -1;
		}
	}

	return 0;
}

/*
 * Func Name     : camcal_read_eeprom_func
 * Func Describe : eeprom type map read func
 * input para    : current_Client, sensor_id
 * output para   : eeprom_buff, eeprom_size
 * return value  : error: -1 , other: OK
 */
static int camcal_read_eeprom_func(struct i2c_client *current_Client,
			    unsigned int sensor_id, unsigned char **eeprom_buff,
			    unsigned int *eeprom_size)
{
	unsigned int eeprom_type_map_size;
	unsigned int eeprom_read_size;
	unsigned int block_num;
	unsigned int block_addr;
	unsigned int block_size;
	unsigned int block_total_size = 0;
	unsigned int return_size = 0;
	unsigned char *peeprom_readbuff;
	unsigned char *peeprom_readbuff_tmp = 0;
	int eeprom_map_index;
	int block_index;

	if ((!current_Client) || (!eeprom_buff) || (!eeprom_size)) {
		pr_err("current_Client or eeprom_buff is null!\n");
		return -1;
	}

	eeprom_type_map_size = ARRAY_SIZE(g_eeprom_map_info);
	for (eeprom_map_index = 0; eeprom_map_index < eeprom_type_map_size;
			eeprom_map_index++) {
		if (sensor_id == g_eeprom_map_info[eeprom_map_index].sensor_id)
			break;
	}

	if (eeprom_map_index >= eeprom_type_map_size) {
		pr_err("can't find sensor [0x%x]!\n", sensor_id);
		return -1;
	}

	eeprom_read_size = g_eeprom_map_info[eeprom_map_index].e2prom_total_size;
	if (eeprom_read_size == 0) {
		pr_err("eeprom_read_size is 0!\n");
		return -1;
	}

	peeprom_readbuff = kmalloc(eeprom_read_size, GFP_KERNEL);
	if (!peeprom_readbuff)
		return -1;

	peeprom_readbuff_tmp = peeprom_readbuff;
	block_num = g_eeprom_map_info[eeprom_map_index].e2prom_block_num;
	for (block_index = 0; block_index < block_num; block_index++) {
		block_addr =
			g_eeprom_map_info[eeprom_map_index].e2prom_block_info[block_index].addr_offset;
		block_size =
			g_eeprom_map_info[eeprom_map_index].e2prom_block_info[block_index].data_len;
		if ((block_total_size + block_size) > eeprom_read_size) {
			pr_err("return_size [%d], block_size [%d], eeprom_read_size [%d]!\n",
				return_size, block_size, eeprom_read_size);
			kfree(peeprom_readbuff);
			return -1;
		}

		return_size = eeprom_read_region(current_Client, block_addr,
				block_size, peeprom_readbuff_tmp);
		while (return_size != block_size) {
			return_size = eeprom_read_odm(sensor_id, block_addr,
				block_size,
				peeprom_readbuff_tmp);
			if (return_size == block_size)
				break;
			pr_err("return_size [%d], block_size [%d]!\n",
			return_size, block_size);
			kfree(peeprom_readbuff);
			return -1;
		}
		peeprom_readbuff_tmp = peeprom_readbuff_tmp + block_size;
		block_total_size = block_total_size + block_size;

		if (block_total_size == eeprom_read_size)
			break;
	}

	if (block_total_size != eeprom_read_size) {
		pr_err("block_total_size [%d] != eeprom_read_size [%d]!\n",
			block_total_size, eeprom_read_size);
		kfree(peeprom_readbuff);
		return -1;
	}

	*eeprom_buff = peeprom_readbuff;
	*eeprom_size = block_total_size;

	pr_info("block_size [%d]!\n", block_total_size);
	return block_total_size;
}

/*
 * Func Name     : eeprom_read_data
 * Func Describe : eeprom read api, after search sensor will call this func
 * input para    : sensor_id
 * output para   : null
 * return value  : error: -1 , other: OK
 */
int eeprom_read_data(unsigned int sensor_id)
{
	int i;
	int ret;
	unsigned int camcal_mtk_map_size;
	unsigned int device_id = 0;
	unsigned int eeprom_size = 0;
	unsigned int slave_addr = 0;
	unsigned char *eeprom_buff = NULL;
	struct i2c_client *current_Client = NULL;
	int (*read_func)(struct i2c_client *, unsigned int,
				unsigned char **, unsigned int *);

	camcal_mtk_map_size = ARRAY_SIZE(g_cam_cal_map_info);
	for (i = 0; i < camcal_mtk_map_size; i++) {
		if (sensor_id == g_cam_cal_map_info[i].sensor_id) {
			device_id = g_cam_cal_map_info[i].device_id;
			slave_addr = g_cam_cal_map_info[i].i2c_addr;
			read_func =
				g_cam_cal_map_info[i].camcal_read_func;
			break;
		}
	}

	if (i == camcal_mtk_map_size) {
		pr_err("can't find sensor [0x%x]!\n", sensor_id);
		return -1;
	}

	if (g_cam_cal_map_info[i].active_flag == true) {
		pr_err("sensor [0x%x] has find befor!\n", sensor_id);
		return -1;
	}

	current_Client = EEPROM_get_i2c_client(device_id);
	if (current_Client == NULL) {
		pr_err("current_Client is null!\n");
		return -1;
	}

	current_Client->addr = slave_addr >> 1;

	if (read_func == NULL) {
		pr_err("read_func is null!\n");
		return -1;
	}

	pr_info("read eeprom data start!\n");

	mutex_lock(&sensor_eeprom_mutex);
	ret = read_func(current_Client, sensor_id,
		&eeprom_buff, &eeprom_size);
	if (ret < 0) {
		pr_err("sensor [0x%x] read eeprom fail!\n", sensor_id);
		mutex_unlock(&sensor_eeprom_mutex);
		return -1;
	}

	g_cam_cal_map_info[i].eeprom_buff = eeprom_buff;
	g_cam_cal_map_info[i].buff_size = eeprom_size;
	g_cam_cal_map_info[i].active_flag = true;
	mutex_unlock(&sensor_eeprom_mutex);

	pr_info("read eeprom data end!\n");

	return eeprom_size;
}

/*
 * Func Name     : eeprom_get_data_from_buff
 * Func Describe : get eeprom data from eeprom buff
 * input para    : sensor_id, size
 * output para   : data
 * return value  : error: -1 , other: OK
 */
int eeprom_get_data_from_buff(unsigned char *data, unsigned int sensor_id,
			      unsigned int size)
{
	unsigned int i;
	unsigned int camcal_mtk_map_size;

	if (data == 0) {
		pr_err("data is null !\n");
		return -1;
	}

	camcal_mtk_map_size = ARRAY_SIZE(g_cam_cal_map_info);
	for (i = 0; i < camcal_mtk_map_size; i++) {
		if ((sensor_id == g_cam_cal_map_info[i].sensor_id) &&
		    (g_cam_cal_map_info[i].active_flag == true))
			break;
	}

	if (i == camcal_mtk_map_size)
		return -1;

	if ((g_cam_cal_map_info[i].eeprom_buff == NULL) ||
	    (g_cam_cal_map_info[i].buff_size == 0)) {
		pr_err("eeprom_buff is null !\n");
		return -1;
	}

	pr_info("read data size [%d] !\n", size);
	if (size != g_cam_cal_map_info[i].buff_size) {
		pr_err("size is err [0x%x]!\n", size);
		return -1;
	}

	mutex_lock(&sensor_eeprom_mutex);

	memcpy(data, g_cam_cal_map_info[i].eeprom_buff, size);

	kfree(g_cam_cal_map_info[i].eeprom_buff);
	g_cam_cal_map_info[i].eeprom_buff = NULL;
	g_cam_cal_map_info[i].buff_size = 0;
	g_cam_cal_map_info[i].active_flag = false;
	mutex_unlock(&sensor_eeprom_mutex);

	return size;
}

/*
 * Func Name     : EEPROM_hal_get_data
 * Func Describe : hal get eeprom data will call this func
 * input para    : sensor_id, size
 * output para   : data
 * return value  : error: -1 , other: OK
 */
int eeprom_hal_get_data(unsigned char *data, unsigned int sensor_id,
			unsigned int size)
{
	int ret;

	if (!data) {
		pr_err("data is null !\n");
		return -1;
	}

	pr_info("hal get data size [%d]!\n", size);

	ret = eeprom_get_data_from_buff(data, sensor_id, size);
	if (ret < 0) {
		pr_err("eeprom_get_data_from_buff fail!\n");
		ret = eeprom_read_data(sensor_id);
		if (ret > 0)
			return eeprom_get_data_from_buff(data, sensor_id, size);
	}

	return ret;
}

/*
 * Func Name     : get_eeprom_byte_info_by_offset
 * Func Describe : read 1 byte data from eeprom
 * input para    : offset, sensor_id
 * output para   : null
 * return value  : data value
 */
unsigned char get_eeprom_byte_info_by_offset(unsigned int offset,
		unsigned int sensor_id)
{
	unsigned int device_id = 0;
	int i;
	unsigned int return_size;
	unsigned int camcal_mtk_map_size = 0;
	unsigned char read_value = 0;
	unsigned int slave_addr = 0;
	struct i2c_client *current_Client = NULL;

	camcal_mtk_map_size = ARRAY_SIZE(g_cam_cal_map_info);
	for (i = 0; i < camcal_mtk_map_size; i++) {
		if (sensor_id == g_cam_cal_map_info[i].sensor_id) {
			device_id = g_cam_cal_map_info[i].device_id;
			slave_addr = g_cam_cal_map_info[i].i2c_addr;
			break;
		}
	}

	if (i == camcal_mtk_map_size) {
		pr_err("can't find sensor [0x%x]!\n", sensor_id);
		return 0;
	}

	current_Client = EEPROM_get_i2c_client(device_id);
	if (current_Client == NULL) {
		pr_err("current_Client is null!\n");
		return 0;
	}

	current_Client->addr = slave_addr >> 1;

	mutex_lock(&sensor_eeprom_mutex);
	/* read 1 byte from eeprom */
	return_size = eeprom_read_region(current_Client, offset, 1, &read_value);
	if (return_size == 0) {
		return_size = eeprom_read_odm(sensor_id, offset,
			1, &read_value);
		if (return_size != 0) {
			mutex_unlock(&sensor_eeprom_mutex);
			return read_value;
		}
		pr_err("read offset [%d] fail!\n", offset);
		mutex_unlock(&sensor_eeprom_mutex);
		return 0;
	}
	mutex_unlock(&sensor_eeprom_mutex);

	return read_value;
}
