/*
 * imgsensor_dt_util.c
 *
 * Copyright (c) 2018-2019 Huawei Technologies Co., Ltd.
 *
 * image sensor device tree parse interface
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

#include "imgsensor_cfg_table.h"
#include "imgsensor_dt_util.h"
#include <linux/of.h>

static const struct IMGSENSOR_STRING_TO_ENUM id_map[] = {
	STRING_TO_ENUM("regulator",  IMGSENSOR_HW_ID_REGULATOR),
	STRING_TO_ENUM("gpio",       IMGSENSOR_HW_ID_GPIO),
	STRING_TO_ENUM("mclk",       IMGSENSOR_HW_ID_MCLK),
	STRING_TO_ENUM("none",       IMGSENSOR_HW_ID_NONE),
};

static const struct IMGSENSOR_STRING_TO_ENUM pin_map[] = {
	STRING_TO_ENUM("cam_none",  IMGSENSOR_HW_PIN_NONE),
	STRING_TO_ENUM("cam_pdn",   IMGSENSOR_HW_PIN_PDN),
	STRING_TO_ENUM("cam_rst",   IMGSENSOR_HW_PIN_RST),
	STRING_TO_ENUM("cam_avdd",  IMGSENSOR_HW_PIN_AVDD),
	STRING_TO_ENUM("cam_dvdd",  IMGSENSOR_HW_PIN_DVDD),
	STRING_TO_ENUM("cam_dovdd", IMGSENSOR_HW_PIN_DOVDD),
	STRING_TO_ENUM("cam_afvdd", IMGSENSOR_HW_PIN_AFVDD),
#ifdef MIPI_SWITCH
	STRING_TO_ENUM("cam_mipi_switch_en",  IMGSENSOR_HW_PIN_MIPI_SWITCH_EN),
	STRING_TO_ENUM("cam_mipi_switch_sel", IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL),
#endif

	STRING_TO_ENUM("cam_mclk", IMGSENSOR_HW_PIN_MCLK),
};

static int imgsensor_dt_string_to_enum(const char *string,
			const struct IMGSENSOR_STRING_TO_ENUM *map,
			unsigned int map_size)
{
	unsigned int i = 0;

	pr_debug("%d: map_size = %d. string=%s\n",
		__LINE__, map_size, string);

	for (i = 0; i < map_size; i++) {
		pr_debug("%d: map[%d].string:%s, src_string:%s\n",
			__LINE__, i, map[i].string, string);
		if (!strncmp(string, map[i].string, strlen(string) + 1))
			break;
	}

	if (i == map_size) {
		pr_err("Invalid string:%d, map_size:%d, string:%s\n",
			i, map_size, string);
		return -EINVAL;
	}

	pr_debug("%d: Exit.table[%d].index = %d\n",
		__LINE__, i, map[i].enum_val);
	return map[i].enum_val;
}

static int imgsensor_dt_get_power_info(struct device_node *of_node, int index,
		struct IMGSENSOR_HW_CUSTOM_POWER_INFO *pwr_info,
		unsigned int max_size)
{
	int rc = 0;
	int i = 0;

	const char *id_name = NULL;
	const char *pin_name = NULL;
	int id_count = 0;
	int pin_count = 0;

	char property0[IMGSENSOR_PROPERTY_MAXSIZE];
	char property1[IMGSENSOR_PROPERTY_MAXSIZE];

	if (!of_node || !pwr_info) {
		pr_err("%d: Fatal,NULL ptr, of_node:%pK, pwr_info: %pK\n",
		       __LINE__, of_node, pwr_info);
		return -EINVAL;
	}

	snprintf(property0, IMGSENSOR_PROPERTY_MAXSIZE,
		"hw_cfg,power-type%d", index);
	id_count = of_property_count_strings(of_node, property0);
	if (id_count <= 0) {
		pr_err("%d: get id failed, id_count:%d\n", __LINE__, id_count);
		return -EINVAL;
	}

	snprintf(property1, IMGSENSOR_PROPERTY_MAXSIZE,
		"hw_cfg,power-pin%d", index);
	pin_count = of_property_count_strings(of_node, property1);
	if (pin_count <= 0) {
		pr_err("%d: get pin failed, pin_count:%d\n",
		       __LINE__, pin_count);
		return -EINVAL;
	}

	if (id_count != pin_count || id_count > max_size) {
		pr_err("%d: id_count:%d mismatched with pin_count:%d\n",
		       __LINE__, id_count, pin_count);
		return -EINVAL;
	}

	for (i = 0; i < id_count; i++) {
		/* 1st get id */
		rc = of_property_read_string_index(of_node, property0,
				i, &id_name);
		if (rc < 0) {
			pr_err("%d: get the %dth %s property failed\n",
			       __LINE__, i, property0);
			return -EINVAL;
		}
		rc = imgsensor_dt_string_to_enum(id_name,
				id_map, IMGSENSOR_ARRAY_SIZE(id_map));
		if (rc == -EINVAL) {
			pr_err("%d: get the %dth %s \"%s\" enum failed\n",
			       __LINE__, i, property0, id_name);
			return -EINVAL;
		}
		pwr_info[i].id = (enum IMGSENSOR_HW_ID)rc;

		/* 2nd get pin */
		rc = of_property_read_string_index(of_node, property1,
				i, &pin_name);
		if (rc < 0) {
			pr_err("%d: get the %dth %s property failed\n",
			       __LINE__, i, property1);
			return -EINVAL;
		}
		rc = imgsensor_dt_string_to_enum(pin_name,
				pin_map, IMGSENSOR_ARRAY_SIZE(pin_map));
		if (rc == -EINVAL) {
			pr_err("%d: get the %dth %s \"%s\" enum failed\n",
			       __LINE__, i, property1, pin_name);
			return -EINVAL;
		}
		pwr_info[i].pin = (enum IMGSENSOR_HW_PIN)rc;
	}

	return 0;
}

#ifdef IMGSENSOR_DT_DEBUG_ENABLE
void imgsensor_dt_pwr_config_debug(struct IMGSENSOR_HW_CFG *pwr_cfg,
				   unsigned int pwr_cfg_size)
{
	int i = 0;
	int j = 0;

	pr_err("%d Enter\n", __LINE__);

	for (i = 0; i < pwr_cfg_size; i++) {
		pr_err("%d pwr_cfg[%d].sensor_idx: %d\n", __LINE__, i,
			pwr_cfg[i].sensor_idx);
		pr_err("%d pwr_cfg[%d].i2c_dev: %d\n", __LINE__, i,
			pwr_cfg[i].i2c_dev);
		pr_err("%d pwr_cfg[%d].pwr_info Begin:\n", __LINE__, i);
		for (j = 0; j < IMGSENSOR_HW_POWER_INFO_MAX; j++) {
			pr_err("%d pwr_cfg[%d].pwr_info[%d]: %d, %d\n",
				__LINE__, i, j,
				pwr_cfg[i].pwr_info[j].id,
				pwr_cfg[i].pwr_info[j].pin);
		}
		pr_err("%d pwr_cfg[%d].pwr_info End\n", __LINE__, i);
	}

	pr_err("%d Exit\n", __LINE__);
}
#endif
/*
 * imgsensor_hw_config_init()
 * Get pwr cfg from dts.
 * if no define hw_cfg,num-sensors, then use the default pwr cfg.
 */
int imgsensor_dt_hw_config_init(struct IMGSENSOR_HW_CFG *pwr_cfg,
				unsigned int pwr_cfg_size)
{
	int rc = 0;
	int i = 0;
	struct device_node *of_node = NULL;
	unsigned int sensor_num = 0;
	char property[IMGSENSOR_PROPERTY_MAXSIZE];

	pr_info("%d: Enter\n", __LINE__);
	if (pwr_cfg == NULL) {
		pr_err("Fatal. NULL ptr\n");
		return -EINVAL;
	}
#ifdef IMGSENSOR_DT_DEBUG_ENABLE
	pr_err("%d, Default pwr_cfg begin:\n", __LINE__);
	imgsensor_dt_pwr_config_debug(pwr_cfg, pwr_cfg_size);
	pr_err("%d, Default pwr_cfg end\n", __LINE__);
#endif
	of_node = of_find_compatible_node(NULL, NULL, "mediatek,camera_hw");
	if (of_node == NULL) {
		pr_err("%d get mediatek,camera_hw node failed!\n", __LINE__);
		return -EINVAL;
	}

	/*
	 * get sensor num, the sensor_num must be less-than pwr_cfg_size,
	 * since the last pwr_cfg should be IMGSENSOR_SENSOR_IDX_NONE
	 */
	rc = of_property_read_u32(of_node, "hw_cfg,num-sensors", &sensor_num);
	if (rc < 0) {
		pr_err("%d:get num-sensors failed. use default pwr_cfg-ignore\n",
			__LINE__);
		return 0;
	}

	if (sensor_num >= pwr_cfg_size) {
		pr_err("%s, %d:get num-sensors failed,sensor_num:%d, pwr_cfg_size: %d\n",
			__func__, __LINE__, sensor_num, pwr_cfg_size);
		return -EINVAL;
	}

	pr_info("%d:get num-sensors,sensor_num:%d\n", __LINE__, sensor_num);

	for (i = 0; i < sensor_num; i++) {
		snprintf(property, IMGSENSOR_PROPERTY_MAXSIZE,
			"hw_cfg,sensor-idx%d", i);
		rc = of_property_read_u32(of_node, property,
			&pwr_cfg[i].sensor_idx);
		if (rc < 0) {
			pr_err("%d failed rc %d\n", __LINE__, rc);
			return rc;
		}
		pr_info("%s %d: %s pwr_cfg[%d].sensor_idx=%d\n",
			__func__, __LINE__, property,
			i, pwr_cfg[i].sensor_idx);

		snprintf(property, IMGSENSOR_PROPERTY_MAXSIZE,
			"hw_cfg,i2c-dev%d", i);
		rc = of_property_read_u32(of_node, property,
			&pwr_cfg[i].i2c_dev);
		if (rc < 0) {
			pr_err("%d failed rc %d\n", __LINE__, rc);
			return rc;
		}
		pr_info("%d: %s pwr_cfg[%d].i2c_dev=%d\n",
			__LINE__, property, i, pwr_cfg[i].i2c_dev);

		rc = imgsensor_dt_get_power_info(of_node, i,
					pwr_cfg[i].pwr_info,
					IMGSENSOR_HW_POWER_INFO_MAX);
		if (rc < 0) {
			pr_err("%d: failed to get pwr_info i = %d, rc = %d\n",
			       __LINE__, i, rc);
			return rc;
		}
	}
	/* The last element of pwr_cfg must be IMGSENSOR_SENSOR_IDX_NONE */
	pwr_cfg[sensor_num].sensor_idx = IMGSENSOR_SENSOR_IDX_NONE;

#ifdef IMGSENSOR_DT_DEBUG_ENABLE
	pr_err("%d, DTS pwr_cfg begin:\n", __LINE__);
	imgsensor_dt_pwr_config_debug(pwr_cfg, pwr_cfg_size);
	pr_err("%d, DTS pwr_cfg end\n", __LINE__);
#endif

	pr_info("%d: Exit\n", __LINE__);

	return 0;
}
