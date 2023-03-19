/*
* Copyright (C) huawei company
*
*/
 
#include <linux/module.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <hwmsensor.h>
#include <SCP_sensorHub.h>
#if defined(CONFIG_SENSOR_PARAM_TO_SCP)
#include <SCP_power_monitor.h>
#if defined(CONFIG_LCD_KIT_DRIVER)
#include <lcd_kit_core.h>
#endif
#include <sensor_para.h>
#endif

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>


#define INFO_NAME_BUF_LEN    ((sizeof(struct sensorInfo_t)) + 1)
#define GPIO_OUT_ZERO 0
#define GPIO_OUT_ONE  1
#if defined(CONFIG_SENSOR_PARAM_TO_SCP)
#define LCD_NAME_LEN    20
#define ALS_PARAM_LEN   20
#define PARAM_CHECK     0xAA55
#define STK3338_DEFAULT_PARAM_INDEX 0
#define LTR2568_DEFAULT_PARAM_INDEX 5
#define ACC_DEFAULT_DIRC            0x5F
#endif
static int fingersense_enabled;
int finger_scp_data_update_ready;
#define MAX_FINGER_SENSE_DATA_CNT   128
#define FINGER_SENSE_MAX_SIZE       (MAX_FINGER_SENSE_DATA_CNT * sizeof(short))
#define MAX_LATCH_DATA_SIZE         1024
#define MAX_FINGER_RING_CNT         256
#define MAX_STR_SIZE                20
#if defined(CONFIG_SENSOR_PARAM_TO_SCP)
struct finger_para_t *fg_para_latch;

static ssize_t store_fg_sense_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	if (!buf)
		return -EINVAL;

	if (strict_strtoul(buf, 10, &val)) {
		pr_err("%s finger enable val %lu invalid", __func__, val);
		return -EINVAL;
	}

	pr_info("%s: finger sense enable val %ld\n", __func__, val);
	if ((val != 0) && (val != 1)) {
		pr_err("%s finger sense set enable fail, invalid val\n",
			__func__);
		return size;
	}

	if (fingersense_enabled == val) {
		pr_err("%s finger sense already current state\n", __func__);
		return size;
	}

	ret = sensor_enable_to_hub(ID_FINGER_SENSE, val);;
	if (ret) {
		pr_err("%s finger sense enable fail: %d\n", __func__, ret);
		return size;
	}
	fingersense_enabled = val;
	return size;
}

static ssize_t show_fg_sensor_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (!buf)
		return -EINVAL;

	return snprintf(buf, MAX_STR_SIZE, "%d\n", fingersense_enabled);
}

static ssize_t store_fg_req_data(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	// consider vibrator
	pr_info("finger %s\n", __func__);
	if (!fingersense_enabled) {
		pr_err("%s finger sense not enable, dont req data\n", __func__);
		return size;
	}
	finger_scp_data_update_ready = 0;

	return size;
}

static void get_finger_share_mem_addr(void)
{
	static bool shr_finger_mem_ready;

	if (!shr_finger_mem_ready) {
		fg_para_latch = get_sensor_share_mem_addr(SHR_MEM_TYPE_FINGER);
		if (!fg_para_latch) {
			pr_err("finger share dram not ready\n");
			return;
		} else {
			pr_info("finger share dram ready\n");
			shr_finger_mem_ready = true;
		}
	}
}

static ssize_t show_fg_data_ready(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (!buf)
		return -EINVAL;

	get_finger_share_mem_addr();
	if (fg_para_latch)
		finger_scp_data_update_ready = fg_para_latch->finger_ready;

	pr_info("finger status = %d\n", finger_scp_data_update_ready);
	return snprintf(buf, MAX_STR_SIZE, "%d\n", finger_scp_data_update_ready);
}

static ssize_t show_fg_latch_data(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int size;
	int tail;
	int head_pos;
	int first_half_size;
	int sec_half_size;

	get_finger_share_mem_addr();
	if (!fg_para_latch || !buf) {
		pr_err("%s: finger share memory pointer err\n", __func__);
		return size;
	}

	size = sizeof(fg_para_latch->fg_data) > FINGER_SENSE_MAX_SIZE ?
		FINGER_SENSE_MAX_SIZE : sizeof(fg_para_latch->fg_data);

	if ((!finger_scp_data_update_ready) || (!fingersense_enabled)) {
		pr_err("%s:fingersense zaxix not ready %d or not enable %d\n",
			__func__,
			finger_scp_data_update_ready, fingersense_enabled);
		return size;
	}

	tail = fg_para_latch->tail;
	pr_info("%s finger ring tail = %d\n", __func__, tail);
	if ((tail >= MAX_FINGER_RING_CNT) || (tail < 0)) {
		pr_err("%s ring buffer tail pos err");
		return size;
	}

	if ((tail >= (MAX_FINGER_SENSE_DATA_CNT - 1)) &&
		(tail <= MAX_FINGER_RING_CNT - 1)) {
		head_pos = tail - 127; // calc 128 ring buf start size
		memcpy(buf, (char *)&fg_para_latch->fg_data[head_pos], size);
	} else {
		head_pos = MAX_FINGER_SENSE_DATA_CNT + 1 + tail;
		first_half_size = (MAX_FINGER_SENSE_DATA_CNT - tail - 1) * sizeof(short);
		memcpy(buf, (char *)&fg_para_latch->fg_data[head_pos],
			first_half_size);
		sec_half_size = (tail + 1) * sizeof(short);
		memcpy(buf + first_half_size,
			(char *)&fg_para_latch->fg_data[0], sec_half_size);
	}
	return size;
}
#endif

static ssize_t sensor_show_ACC_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned char info_name[INFO_NAME_BUF_LEN]={0};
    int ret = 0;

	if (NULL == buf) {
	    pr_err("%s, buf is NULL!\n", __func__);
		return -1;
	}

    ret = sensor_set_cmd_to_hub(ID_ACCELEROMETER, CUST_ACTION_GET_SENSOR_INFO, info_name);
    if (ret < 0) {
        pr_err("set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
        ID_ACCELEROMETER, CUST_ACTION_GET_SENSOR_INFO);
    }
    return snprintf(buf, PAGE_SIZE, "%s\n", info_name);
}

static ssize_t sensor_show_MAG_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned char info_name[INFO_NAME_BUF_LEN]={0};
    int ret = 0;

	if (NULL == buf) {
	    pr_err("%s, buf is NULL!\n", __func__);
		return -1;
	}

    ret = sensor_set_cmd_to_hub(ID_MAGNETIC, CUST_ACTION_GET_SENSOR_INFO, info_name);
    if (ret < 0) {
        pr_err("set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
        ID_MAGNETIC, CUST_ACTION_GET_SENSOR_INFO);
    }
    return snprintf(buf, PAGE_SIZE, "%s\n", info_name);
}

static ssize_t sensor_show_GYRO_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned char info_name[INFO_NAME_BUF_LEN]={0};
    int ret = 0;

	if (NULL == buf) {
	    pr_err("%s, buf is NULL!\n", __func__);
		return -1;
	}

    ret = sensor_set_cmd_to_hub(ID_GYROSCOPE, CUST_ACTION_GET_SENSOR_INFO, info_name);
    if (ret < 0) {
        pr_err("set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
        ID_GYROSCOPE, CUST_ACTION_GET_SENSOR_INFO);
    }
    return snprintf(buf, PAGE_SIZE, "%s\n", info_name);
}

static ssize_t sensor_show_ALS_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned char info_name[INFO_NAME_BUF_LEN]={0};
    int ret = 0;

	if (NULL == buf) {
	    pr_err("%s, buf is NULL!\n", __func__);
		return -1;
	}

    ret = sensor_set_cmd_to_hub(ID_LIGHT, CUST_ACTION_GET_SENSOR_INFO, info_name);
    if (ret < 0) {
        pr_err("set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
        ID_LIGHT, CUST_ACTION_GET_SENSOR_INFO);
    }
    return snprintf(buf, PAGE_SIZE, "%s\n", info_name);
}

static ssize_t sensor_show_PRE_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned char info_name[INFO_NAME_BUF_LEN]={0};
    int ret = 0;

	if (NULL == buf) {
	    pr_err("%s, buf is NULL!\n", __func__);
		return -1;
	}

    ret = sensor_set_cmd_to_hub(ID_PRESSURE, CUST_ACTION_GET_SENSOR_INFO, info_name);
    if (ret < 0) {
        pr_err("set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
        ID_PRESSURE, CUST_ACTION_GET_SENSOR_INFO);
    }
    return snprintf(buf, PAGE_SIZE, "%s\n", info_name);
}

static ssize_t sensor_show_SAR_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned char info_name[INFO_NAME_BUF_LEN]={0};
    int ret = 0;

	if (NULL == buf) {
	    pr_err("%s, buf is NULL!\n", __func__);
		return -1;
	}

    ret = sensor_set_cmd_to_hub(ID_SAR, CUST_ACTION_GET_SENSOR_INFO, info_name);
    if (ret < 0) {
        pr_err("set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
        ID_SAR, CUST_ACTION_GET_SENSOR_INFO);
    }
    return snprintf(buf, PAGE_SIZE, "%s\n", info_name);
}

static ssize_t sensor_show_PS_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned char info_name[INFO_NAME_BUF_LEN]={0};
    int ret = 0;

	if (NULL == buf) {
	    pr_err("%s, buf is NULL!\n", __func__);
		return -1;
	}

    ret = sensor_set_cmd_to_hub(ID_PROXIMITY, CUST_ACTION_GET_SENSOR_INFO, info_name);
    if (ret < 0) {
        pr_err("set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
        ID_PROXIMITY, CUST_ACTION_GET_SENSOR_INFO);
    }
    return snprintf(buf, PAGE_SIZE, "%s\n", info_name);
}

static DEVICE_ATTR(acc_info, S_IRUGO, sensor_show_ACC_info, NULL);
static DEVICE_ATTR(mag_info, S_IRUGO, sensor_show_MAG_info, NULL);
static DEVICE_ATTR(gyro_info, S_IRUGO, sensor_show_GYRO_info, NULL);
static DEVICE_ATTR(als_info, S_IRUGO, sensor_show_ALS_info, NULL);
static DEVICE_ATTR(pre_info, S_IRUGO, sensor_show_PRE_info, NULL);
static DEVICE_ATTR(sar_info, S_IRUGO, sensor_show_SAR_info, NULL);
static DEVICE_ATTR(ps_info, S_IRUGO, sensor_show_PS_info, NULL);
#if defined(CONFIG_SENSOR_PARAM_TO_SCP)
static DEVICE_ATTR(set_fingersense_enable, 0660, show_fg_sensor_enable,
	store_fg_sense_enable);
static DEVICE_ATTR(fingersense_req_data, 0220, NULL, store_fg_req_data);
static DEVICE_ATTR(fingersense_data_ready, 0440, show_fg_data_ready, NULL);
static DEVICE_ATTR(fingersense_latch_data, 0440, show_fg_latch_data, NULL);
#endif

static struct attribute *sensor_attributes[] = {
    &dev_attr_acc_info.attr,
    &dev_attr_mag_info.attr,
    &dev_attr_gyro_info.attr, 
    &dev_attr_als_info.attr,
    &dev_attr_pre_info.attr, 
    &dev_attr_sar_info.attr,
	&dev_attr_ps_info.attr,
#if defined(CONFIG_SENSOR_PARAM_TO_SCP)
	&dev_attr_set_fingersense_enable.attr,
	&dev_attr_fingersense_req_data.attr,
	&dev_attr_fingersense_data_ready.attr,
	&dev_attr_fingersense_latch_data.attr,
#endif
	NULL
};

static const struct attribute_group sensor_node = {
    .attrs = sensor_attributes,
};
 
 static struct platform_device sensor_input_info = {
    .name = "huawei_sensor",
    .id = -1,
};

#if defined(CONFIG_SENSOR_PARAM_TO_SCP)
enum product_name {
	PRODUCT_MED_MOA = 4,
};
static uint8_t acc_direction;
static uint8_t product_type;
static char lcd_type[LCD_NAME_LEN] = {0};
static char als_type[INFO_NAME_BUF_LEN] = {0};

struct alsps_params {
	uint8_t product_type;
	char lcd_type[LCD_NAME_LEN];
	char als_type[INFO_NAME_BUF_LEN];
	uint16_t params[ALS_PARAM_LEN];
};

static struct alsps_params param_to_scp[] = {
	{
		.product_type = PRODUCT_MED_MOA,
		.lcd_type = "Y1639I130",
		.als_type = "stk3338_als",
		.params = { PARAM_CHECK, 2000, 570, 861, 1984, 293, 176, 102 },
	},
	{
		.product_type = PRODUCT_MED_MOA,
		.lcd_type = "Y16379130",
		.als_type = "stk3338_als",
		.params = { PARAM_CHECK, 2000, 570, 861, 1984, 293, 176, 102 },
	},
	{
		.product_type = PRODUCT_MED_MOA,
		.lcd_type = "Y16379250",
		.als_type = "stk3338_als",
		.params = { PARAM_CHECK, 2000, 570, 861, 1984, 293, 176, 102 },
	},
	{
		.product_type = PRODUCT_MED_MOA,
		.lcd_type = "Y1639J170",
		.als_type = "stk3338_als",
		.params = { PARAM_CHECK, 2000, 570, 861, 1984, 293, 176, 102 },
	},
	{
		.product_type = PRODUCT_MED_MOA,
		.lcd_type = "Y16379120",
		.als_type = "stk3338_als",
		.params = { PARAM_CHECK, 2000, 570, 861, 1150, 293, 176, 102 },
	},
	{
		.product_type = PRODUCT_MED_MOA,
		.lcd_type = "Y1639I130",
		.als_type = "ltr2568_als",
		.params = { PARAM_CHECK, 600, 80, 1946, 3733, 7767, 11800,
			921, 818, 818, 864, 864 },
	},
	{
		.product_type = PRODUCT_MED_MOA,
		.lcd_type = "Y16379130",
		.als_type = "ltr2568_als",
		.params = { PARAM_CHECK, 600, 80, 1946, 3733, 7767, 11800,
			921, 818, 818, 864, 864 },
	},
	{
		.product_type = PRODUCT_MED_MOA,
		.lcd_type = "Y16379250",
		.als_type = "ltr2568_als",
		.params = { PARAM_CHECK, 600, 80, 1946, 3733, 7767, 11800,
			921, 818, 818, 864, 864 },
	},
	{
		.product_type = PRODUCT_MED_MOA,
		.lcd_type = "Y1639J170",
		.als_type = "ltr2568_als",
		.params = { PARAM_CHECK, 600, 80, 1946, 3733, 7767, 11800,
			921, 818, 818, 864, 864 },
	},
	{
		.product_type = PRODUCT_MED_MOA,
		.lcd_type = "Y16379120",
		.als_type = "ltr2568_als",
		.params = { PARAM_CHECK, 600, 80, 1946, 3733, 7767, 11800,
			921, 818, 818, 864, 864 },
	},
};

static struct work_struct init_done_work;

static void get_als_param(uint16_t *param, uint32_t size)
{
	int i;

	if (size != ALS_PARAM_LEN)
		return;

	for (i = 0; i < ARRAY_SIZE(param_to_scp); i++) {
		if ((param_to_scp[i].product_type == product_type) &&
			!strcmp(param_to_scp[i].lcd_type, lcd_type) &&
			!strcmp(param_to_scp[i].als_type, als_type)) {
			pr_info("%s : success\n", __func__);
			memcpy(param, param_to_scp[i].params, ALS_PARAM_LEN *
				sizeof(uint16_t));
			return;
		}
	}

	if (!strcmp(param_to_scp[STK3338_DEFAULT_PARAM_INDEX].als_type, als_type)) {
		memcpy(param, param_to_scp[STK3338_DEFAULT_PARAM_INDEX].params,
			ALS_PARAM_LEN * sizeof(uint16_t));
		pr_info("%s : use stk3338_als default param\n", __func__);
	} else if (!strcmp(param_to_scp[LTR2568_DEFAULT_PARAM_INDEX].als_type,
		als_type)) {
		memcpy(param, param_to_scp[LTR2568_DEFAULT_PARAM_INDEX].params,
				ALS_PARAM_LEN * sizeof(uint16_t));
		pr_info("%s : use ltr2568_als default param\n", __func__);
	} else {
		pr_err("%s : als sensor not found\n", __func__);
	}
}

static int get_product_type(void)
{
	int ret;
	uint32_t type = 0;
	struct device_node* scpinfo_node = NULL;

	scpinfo_node = of_find_compatible_node(NULL, NULL, "huawei,huawei_scp_info");
	if (scpinfo_node == NULL) {
		pr_err("Cannot find huawei_scp_info from dts\n");
		return -1;
	} else {
		ret = of_property_read_u32(scpinfo_node, "product_number",
			&type);
		if (!ret) {
			pr_info("find product_type = %u\n", type);
			product_type = type;
		} else {
			pr_err("Cannot find product_type from dts\n");
			product_type = PRODUCT_MED_MOA;
		}
	}

	return ret;
}

static int get_acc_direction(void)
{
	int ret;
	uint32_t direction = 0;
	struct device_node* direction_node = NULL;

	direction_node = of_find_compatible_node(NULL, NULL, "huawei,huawei_scp_info");
	if (direction_node == NULL) {
		pr_err("Cannot find huawei_scp_info from dts\n");
		return -1;
	} else {
		ret = of_property_read_u32(direction_node, "direction_number",
			&direction);
		if (!ret) {
			pr_info("find acc_direction = %u\n", direction);
			acc_direction = direction;
		} else {
			pr_err("Cannot find acc_direction from dts\n");
			acc_direction = ACC_DEFAULT_DIRC;
		}
	}

	return ret;
}

static void scp_init_done_work(struct work_struct *work)
{
	int err;
	uint16_t als_param[ALS_PARAM_LEN] = {0};
#if defined(CONFIG_LCD_KIT_DRIVER)
	struct lcd_kit_ops *tp_ops = NULL;
#endif

	if (sensor_para == NULL) {
		pr_err("%s : sensor_para is null", __func__);
		return;
	}

	pr_info("%s\n", __func__);
	err = get_product_type();
	if (err)
		pr_err("%s : get product type failed. ret is %d", __func__, err);

#if defined(CONFIG_LCD_KIT_DRIVER)
	tp_ops = lcd_kit_get_ops();
	if (tp_ops && tp_ops->get_project_id) {
		err = tp_ops->get_project_id(lcd_type);
		if (err)
			pr_err("%s : get lcd type failed. ret is %d", __func__, err);
		else
			pr_info("%s : get lcd type : %s", __func__, lcd_type);
	} else {
		pr_err("%s : cannot get lcd type", __func__);
	}
#endif

	err = sensor_set_cmd_to_hub(ID_LIGHT, CUST_ACTION_GET_SENSOR_INFO,
		als_type);
	if (err)
		pr_err("%s : get als type failed. ret is : %d", __func__, err);
	else
		pr_info("%s : get als type : %s", __func__, als_type);

	err = get_acc_direction();
	if (err) {
		pr_err("%s : get direction failed. err is %d, set to 5F", __func__, err);
		sensor_para->acc_para.direction = ACC_DEFAULT_DIRC;
	} else {
		sensor_para->acc_para.direction = acc_direction;
	}
	get_als_param(als_param, ALS_PARAM_LEN);
	memcpy(sensor_para->als_para.als_extend_para, als_param, ALS_PARAM_LEN *
		sizeof(uint16_t));
}

static int scp_ready_event(uint8_t event, void *ptr)
{
	switch (event) {
	case SENSOR_POWER_UP:
		schedule_work(&init_done_work);
		break;
	case SENSOR_POWER_DOWN:
		break;
	default:
		break;
	}
	return 0;
}

static struct scp_power_monitor scp_ready_notifier = {
	.name = "param_to_scp",
	.notifier_call = scp_ready_event,
};

static int params_to_scp_probe(void)
{
	pr_info("%s Entrance\n", __func__);

	INIT_WORK(&init_done_work, scp_init_done_work);
	scp_power_monitor_register(&scp_ready_notifier);

	return 0;
}
#endif

static int __init sensor_input_info_init(void)
{
    int ret = 0;
	int ven_gpio=0;
    int gpio_err = 0;
    struct device_node *psensor_node =NULL;
    pr_err("[%s] ++ \n", __func__);
    ret = platform_device_register(&sensor_input_info);
    if (ret) {
        pr_err("%s: platform_device_register failed, ret:%d.\n", __func__, ret);
        goto REGISTER_ERR;
    }
 
    ret = sysfs_create_group(&sensor_input_info.dev.kobj, &sensor_node);
    if (ret) {
        pr_err("sensor_input_info_init sysfs_create_group error ret =%d. \n", ret);
        goto SYSFS_CREATE_CGOUP_ERR;
    }
    psensor_node=of_find_compatible_node(NULL, NULL, "mediatek,psensor");
    if(psensor_node){
    pr_info("%s :find psensor node!!!\n",__func__);
    /*get gpio config*/
        ven_gpio=of_get_named_gpio_flags(psensor_node, "psensor,ldo_ven", 0,NULL);
		if (ven_gpio < 0) {
			pr_err( "fail to get ldo ven\n");
			return 0;
		}
    pr_info("%s :find psensor node  ven_gpio= %d !!!\n",__func__,ven_gpio);
    gpio_err=gpio_request(ven_gpio,"ldo_ven");
    if(gpio_err<0){
        pr_err( "ret = %d : could not req gpio reset\n", gpio_err);
    }
    gpio_err=gpio_direction_output(ven_gpio, GPIO_OUT_ZERO);
    if (gpio_err) {
                pr_err("Could not set psensor GPIO170 as output.\n");
        }
    gpio_set_value(ven_gpio, GPIO_OUT_ONE);
    }else{
        pr_err("[%s] --can't find psensor node!!!\n", __func__);
    }

#if defined(CONFIG_SENSOR_PARAM_TO_SCP)
	params_to_scp_probe();
#endif

    pr_err("[%s] --\n", __func__);
    return 0;
 SYSFS_CREATE_CGOUP_ERR:
    platform_device_unregister(&sensor_input_info);
 REGISTER_ERR:
    return ret;
}

 module_init(sensor_input_info_init);
 MODULE_DESCRIPTION("sensor input info");
 MODULE_AUTHOR("huawei sensor driver group");
 MODULE_LICENSE("GPL");
