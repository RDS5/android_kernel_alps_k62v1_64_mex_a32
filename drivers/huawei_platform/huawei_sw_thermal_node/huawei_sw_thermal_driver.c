/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2034. All rights reserved.
 *
 * File name: huawei_sw_thermal_driver.c
 * Description: This file use to send kernel state
 * Author: huoyanjun@huawei.com
 * Version: 0.1
 * Date:  2018/06/7
 * shell thermal config and algorithm AR000AFR67
 */

/******************************************************************************
 Head file
******************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <huawei_platform/power/hw_kstate.h>
#include <net/sock.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
// #include <linux/msm_thermal.h>

#include <xen/page.h>  /* mtk */
// #include "hisi_peripheral_tm.h" /* qcom no use! */
// #include <linux/power/hisi/coul/hisi_coul_drv.h> /* batt temp.qcom no use! */

#define CREATE_TRACE_POINTS

#include <trace/events/shell_temp.h> /* for log */
#include <../drivers/misc/mediatek/include/mt-plat/mtk_thermal_monitor.h>

#define	MUTIPLY_FACTOR		(100000)
#define	DEFAULT_SHELL_TEMP	(25000)
#define ERROR_SHELL_TEMP	(125000)
#define	BATTERY_ID   		(255)
#define SENSOR_PARA_COUNT	3

/* begin of platform content */
#define	BATTERY_NAME		    "battery"
#define BOARD_THERM_NAME        "msm_therm" /* system_h */
#define PA_THERM_NAME           "pa_therm0" /* PA temp == pa_0 */
#define XO_NAME                 "pmic_xo_therm"
#define CHG_IC_THERM_NAME       "CHG_IC_THERM"
#define COOL_NAME        		"COOL_THERM"

#define GPU_THERM "tsens_tz_sensor8"    /* GPU thermal */
#define LITTLE_CORE_1_THERM "tsens_tz_sensor1"  /* little core bus */
#define LITTLE_CORE_2_THERM "tsens_tz_sensor2"  /* little core bus */
#define BIG_CORE_BUS_THERM "tsens_tz_sensor7"    /* big core bus */
#define BIG_CORE_L2_THERM "tsens_tz_sensor12"    /* big core(CPU-L2) */
#define BIG_CORE_1_THERM "tsens_tz_sensor3"    /* big core -1 */
#define BIG_CORE_2_THERM "tsens_tz_sensor4"    /* big core -2 */
#define BIG_CORE_3_THERM "tsens_tz_sensor5"    /* big core -3 */
#define BIG_CORE_4_THERM "tsens_tz_sensor6"    /* big core -4 */
typedef enum {
	BATTERY_THERMAL_ID = 1,
	BOARD_THERMAL_ID,
	XO_THERMAL_ID,
	PA_THERMAL_ID,
	CHG_IC_THERMAL_ID,
	COOL_DOMAIN_THERMAL_ID,	
	UNKNOWN_THERMAL_ID
}thermal_id_enum;

/* end of platform content */

/******************************************************************************
 Type define
******************************************************************************/
typedef enum {
	TYPE_TSENS = 0,  /* soc temp type */
	TYPE_PERIPHERAL, /* peripheral temp  type */
	TYPE_BATTERY,    /* battery temp type */
	TYPE_UNKNOWN,
	TYPE_MAX,
}therm_type;

struct hisi_temp_tracing_t {
	int temp;
	int coef; /* temp coefficient */
	int temp_invalid_flag;/* 0:temp is valid;1:temp is invalid  */
};

#define PASSAGE_NAME_LEN  (23)  /* passage name max len is 23! */
#define PASSAGE_ARRAY_LEN (PASSAGE_NAME_LEN + 1)
struct hisi_shell_sensor_t{
	char passage_name[PASSAGE_ARRAY_LEN];/* qcom; */
	u32 id;
	u32 sensor_type;
	struct hisi_temp_tracing_t temp_tracing[0];
};

struct temperature_node_t {
	struct device *device; /* soft link device */
	int ambient;
};

struct hw_thermal_class {
	struct class *thermal_class;
	struct temperature_node_t temperature_node;
};

struct hw_thermal_class hw_thermal_info;

struct hisi_shell_t {
	int sensor_count; /* the num of child sensor node */
	int sample_count; /* sampling num == needing data num */
	int tsensor_temp_step;
	int tsensor_max_temp;
	int tsensor_min_temp;
	int ntc_temp_step;
	int ntc_max_temp;
	int ntc_min_temp;
	u32 interval;     /* sampling interval(ms) */
	int bias;         /* XXX bias value */
	int temp;         /* save temp of shell_XXX */
	int index;        /* current idx to update temp */
	int valid_flag;   /* is current node valid */
#ifdef CONFIG_HISI_SHELL_TEMP_DEBUG
	int channel;
	int debug_temp;
#endif
	struct thermal_zone_device	*tz_dev;
	struct delayed_work work;
	struct hisi_shell_sensor_t hisi_shell_sensor[0];
};

/******************************************************************************
 include function api 
******************************************************************************/
/* battery thermal sensor */
extern signed int battery_get_bat_temperature(void);

/* board thermal sensor. */
extern int mtkts_bts_get_hw_temp(void);

/* PA thermal sensor. */
extern int mtkts_btsmdpa_get_hw_temp(void);

u32 sensor_get_id(const char *name){
	u32 thermal_id = UNKNOWN_THERMAL_ID;
	
	if(!name)return thermal_id;

	u32 len = strlen(name);
	if(!strncmp(name,BATTERY_NAME,(len > strlen(BATTERY_NAME))?len:strlen(BATTERY_NAME))){
		thermal_id = (u32)BATTERY_THERMAL_ID;
	}else if(!strncmp(name,BOARD_THERM_NAME,(len > strlen(BOARD_THERM_NAME))?len:strlen(BOARD_THERM_NAME))){		
		thermal_id = (u32)BOARD_THERMAL_ID;
	}else if(!strncmp(name,XO_NAME,(len > strlen(XO_NAME))?len:strlen(XO_NAME))){		
		thermal_id = (u32)XO_THERMAL_ID;
	}	
	else if(!strncmp(name,PA_THERM_NAME,(len > strlen(PA_THERM_NAME))?len:strlen(PA_THERM_NAME))){
		thermal_id = (u32)PA_THERMAL_ID;
	}else if(!strncmp(name,CHG_IC_THERM_NAME,(len > strlen(CHG_IC_THERM_NAME))?len:strlen(CHG_IC_THERM_NAME))){
		thermal_id = (u32)CHG_IC_THERMAL_ID;
	}else if(!strncmp(name,COOL_NAME,(len > strlen(COOL_NAME))?len:strlen(COOL_NAME))){
		thermal_id = (u32)COOL_DOMAIN_THERMAL_ID;
	}else{
		thermal_id = (u32)UNKNOWN_THERMAL_ID;
	}
	
	return thermal_id;
}

int therm_get_temp_for_huawei(uint32_t id,int *temp){

    int rt_value = 0;

    if(!temp)
    {
        pr_err("%s:temp_value is NULL!\n", __func__);        
        return -EINVAL;
    }

	if(BATTERY_THERMAL_ID == id){
		*temp = battery_get_bat_temperature();
	}else if(BOARD_THERMAL_ID == id){
		*temp = mtk_thermal_get_temp(MTK_THERMAL_SENSOR_AP);
	}else if(XO_THERMAL_ID == id){
		/* using PMIC xo thermal as xo thermal.because mtk doesn't xo thermal */
		*temp = mtk_thermal_get_temp(MTK_THERMAL_SENSOR_PMIC);
	}else if(PA_THERMAL_ID == id){
		*temp = mtk_thermal_get_temp(MTK_THERMAL_SENSOR_MD_PA);
	}else if(CHG_IC_THERMAL_ID == id){
		*temp = mtk_thermal_get_temp(MTK_THERMAL_SENSOR_CHARGERNTC);
	}else if(COOL_DOMAIN_THERMAL_ID == id){
		/* usb board ntc as cool domain thermal.  */
		*temp = mtk_thermal_get_temp(MTK_THERMAL_SENSOR_USBNTC); 
	}else{
		rt_value = -EINVAL;
	}

	return rt_value;	
}

extern struct thermal_zone_device *thermal_zone_device_register(const char *type,
    int trips, int mask, void *devdata,
    struct thermal_zone_device_ops *ops,
    struct thermal_zone_params *tzp,
    int passive_delay, int polling_delay);

/******************************************************************************
 Get "/sys/class/hw_thermal/temp/shell_XXX/temp" value
******************************************************************************/
static ssize_t
hisi_shell_show_temp(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct hisi_shell_t *hisi_shell;

	if (dev == NULL || devattr == NULL || NULL == buf)
		return 0;

	if (dev->driver_data == NULL)
		return 0;

	hisi_shell = dev->driver_data;

	return snprintf(buf, (PAGE_SIZE - 1), "%d\n", hisi_shell->temp);
}
static DEVICE_ATTR(temp, S_IWUSR | S_IRUGO,
           hisi_shell_show_temp, NULL);

#ifdef CONFIG_HISI_SHELL_TEMP_DEBUG
static ssize_t
hisi_shell_store_debug_temp(struct device *dev, struct device_attribute *devattr,
						   const char *buf, size_t count)
{
	int channel, temp;
	struct platform_device *pdev;
	struct hisi_shell_t *hisi_shell;
	if (dev == NULL || devattr == NULL)
		return 0;

	if (!sscanf(buf, "%d %d\n", &channel, &temp)) {
		pr_err("%s Invalid input para\n", __func__);
		return -EINVAL;
	}

	pdev = container_of(dev, struct platform_device, dev);
	hisi_shell = platform_get_drvdata(pdev);

	hisi_shell->channel = channel;
	hisi_shell->debug_temp = temp;

	return (ssize_t)count;
}
static DEVICE_ATTR(debug_temp, S_IWUSR, NULL, hisi_shell_store_debug_temp);
#endif

/******************************************************************************
 For creating soft link and node:temp
******************************************************************************/
static struct attribute *hisi_shell_attributes[] = {
    &dev_attr_temp.attr,
#ifdef CONFIG_HISI_SHELL_TEMP_DEBUG
	&dev_attr_debug_temp.attr,
#endif
    NULL
};

static struct attribute_group hisi_shell_attribute_group = {
    .attrs = hisi_shell_attributes,
};

/******************************************************************************
 Get "/sys/class/hw_thermal/temp/ambient" value
******************************************************************************/
#define SHOW_TEMP(temp_name)                                            \
static ssize_t show_##temp_name                                         \
(struct device *dev, struct device_attribute *attr, char *buf)          \
{                                                                       \
    if (dev == NULL || attr == NULL)                                    \
        return 0;                                                       \
                                                                        \
    return snprintf(buf, (PAGE_SIZE - 1), "%d\n",                       \
                    (int)hw_thermal_info.temperature_node.temp_name);   \
}
SHOW_TEMP(ambient);

/******************************************************************************
 Set "/sys/class/hw_thermal/temp/ambient" value
******************************************************************************/
#define STORE_TEMP(temp_name)                                           \
static ssize_t store_##temp_name                                        \
(struct device *dev, struct device_attribute *attr,                     \
    const char *buf, size_t count)                                      \
{                                                                       \
    int temp_name;                                                      \
                                                                        \
    if (dev == NULL || attr == NULL)                                    \
        return 0;                                                       \
                                                                        \
    if (kstrtouint(buf, 10, &temp_name)) /*lint !e64*/                  \
        return -EINVAL;                                                 \
                                                                        \
    hw_thermal_info.temperature_node.temp_name = temp_name;             \
                                                                        \
    return (ssize_t)count;                                              \
}
STORE_TEMP(ambient);

/*lint -e84 -e846 -e514 -e778 -e866 -esym(84,846,514,778,866,*)*/
#define TEMP_ATTR_RW(temp_name)                                         \
static DEVICE_ATTR(temp_name, S_IWUSR | S_IRUSR | S_IRGRP | S_IWGRP,	\
                   show_##temp_name,                                    \
                   store_##temp_name)

TEMP_ATTR_RW(ambient);
/*lint -e84 -e846 -e514 -e778 -e866 +esym(84,846,514,778,866,*)*/

/******************************************************************************
 Function :
     get "/sys/class/hw_thermal/temp/shell_XXX/temp" value api for other module
******************************************************************************/
int hisi_get_shell_temp(struct thermal_zone_device *thermal,
                        int *temp)
{
    struct hisi_shell_t *hisi_shell = (struct hisi_shell_t *)thermal->devdata;

    if (!hisi_shell || !temp)
        return -EINVAL;

    *temp = hisi_shell->temp;

    return 0;
}

/*lint -e785*/
struct thermal_zone_device_ops shell_thermal_zone_ops = {
	.get_temp = hisi_get_shell_temp,  /* get temp api for other module */
};
/*lint +e785*/

/******************************************************************************
 Function : 
     Calc "/sys/class/hw_thermal/temp/shell_XXX/temp" value by weighted average
******************************************************************************/
static int calc_shell_temp(struct hisi_shell_t *hisi_shell)
{
    int i, j, k;
    struct hisi_shell_sensor_t *shell_sensor;
    long sum = 0;

    for (i = 0; i < hisi_shell->sensor_count; i++) {
		shell_sensor = (struct hisi_shell_sensor_t *)((u64)(hisi_shell->hisi_shell_sensor) + (u64)((long)i) * (sizeof(struct hisi_shell_sensor_t)
						+ sizeof(struct hisi_temp_tracing_t) * (u64)((long)hisi_shell->sample_count)));

        for (j = 0; j < hisi_shell->sample_count; j++) {
			k = (hisi_shell->index - j) <  0 ? ((hisi_shell->index - j) + hisi_shell->sample_count) : hisi_shell->index - j;

			sum += (long)shell_sensor->temp_tracing[j].coef * (long)shell_sensor->temp_tracing[k].temp;
            trace_calc_shell_temp(hisi_shell->tz_dev, i, j, 
                                  shell_sensor->temp_tracing[j].coef,
                                  shell_sensor->temp_tracing[k].temp, 
                                  sum);  /* for trace log */
        }
    }

    sum += ((long)hisi_shell->bias * 1000L);

    return (int)(sum / MUTIPLY_FACTOR);
}
static int handle_temp_data_first_sample(struct hisi_shell_t *hisi_shell, struct hisi_shell_sensor_t *shell_sensor, int temp, int channel)
{
	int ret = 0;

	if ((shell_sensor->sensor_type == TYPE_PERIPHERAL) || (shell_sensor->sensor_type == TYPE_BATTERY)) {
		if ((temp > hisi_shell->ntc_max_temp) || (temp < hisi_shell->ntc_min_temp)) {
            /* the temp value is invalid! */
			pr_err("%s, %s, channel is %d, first ntc temp is %d\n", __func__, hisi_shell->tz_dev->type, channel, temp);
			shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag = 1;
			ret = 1;
		} else
			shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag = 0;
	} else if (shell_sensor->sensor_type == TYPE_TSENS) {
		if ((temp > hisi_shell->tsensor_max_temp) || (temp < hisi_shell->tsensor_min_temp)) {
            /* the Tsensor temp value is invalid! */
			pr_err("%s, %s, channel is %d, first tsensor temp is %d\n", __func__, hisi_shell->tz_dev->type, channel, temp);
			shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag = 1;
			ret = 1;
		} else
			shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag = 0;
	} else
		shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag = 0;

	return ret;
}

static int handle_temp_data_other_sample(struct hisi_shell_t *hisi_shell, struct hisi_shell_sensor_t *shell_sensor, int temp, int channel)
{
	int ret = 0;
	int old_index, old_temp, diff;

    /* find pre temp idx */
	old_index = (hisi_shell->index - 1) <  0 ? ((hisi_shell->index - 1) + hisi_shell->sample_count) : hisi_shell->index - 1;
	old_temp = shell_sensor->temp_tracing[old_index].temp;
	diff = (temp - old_temp) < 0 ? (old_temp - temp) : (temp - old_temp);
	diff = (u32)diff / hisi_shell->interval * 1000;
	
	if ((shell_sensor->sensor_type == TYPE_PERIPHERAL) || (shell_sensor->sensor_type == TYPE_BATTERY)) {
		if ((temp > hisi_shell->ntc_max_temp) || (temp < hisi_shell->ntc_min_temp) || (diff > hisi_shell->ntc_temp_step)) {
            /* the temp value is invalid! */
			pr_err("%s, %s, channel is %d, ntc temp is %d, diff is %d\n", __func__, hisi_shell->tz_dev->type, channel, temp, diff);
			shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag = 1;
			ret = 1;
		} else
			shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag = 0;
	} else if (shell_sensor->sensor_type == TYPE_TSENS) {
		if ((temp > hisi_shell->tsensor_max_temp) || (temp < hisi_shell->tsensor_min_temp) || (diff > hisi_shell->tsensor_temp_step)) {
            /* the temp value is invalid! */
			pr_err("%s, %s, channel is %d, tsensor temp is %d, diff is %d\n", __func__, hisi_shell->tz_dev->type, channel, temp, diff);
			shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag = 1;
			ret = 1;
		} else
			shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag = 0;
	} else
		shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag = 0;

	return ret;
}

/******************************************************************************
 Function : 
     Update sensor's [0,1,2,3...] temp value and temp_invalid_flag of hisi_shell->index.
     Details:Update shell_XXX's temp_tracing[hisi_shell->index].temp value by 
     hardware temp value.
     note:channel is temp idx of xxx(sensor[0,1,2,3...]) sensor.
******************************************************************************/
static int hkadc_handle_temp_data(struct hisi_shell_t *hisi_shell, struct hisi_shell_sensor_t *shell_sensor, int temp, int channel)
{
	int ret = 0;

	if (!hisi_shell->valid_flag && !hisi_shell->index)
		ret = handle_temp_data_first_sample(hisi_shell, shell_sensor, temp, channel);
	else
		ret = handle_temp_data_other_sample(hisi_shell, shell_sensor, temp, channel);

	shell_sensor->temp_tracing[hisi_shell->index].temp = temp;

	return ret;
}
/* get valid temp and calculating average.
   return value:
   1--all tsensors or ntc sensors damaged;
   0--others
*/
static int calc_sensor_temp_avg(struct hisi_shell_t *hisi_shell, int *tsensor_avg, int *ntc_avg)
{
	int i;
	int tsensor_good_count = 0;
	int ntc_good_count = 0;
	int sum_tsensor_temp = 0;
	int sum_ntc_temp = 0;
	int have_tsensor = 0;
	int have_ntc = 0;
	struct hisi_shell_sensor_t *shell_sensor;

	for (i = 0; i < hisi_shell->sensor_count; i++) {
        /* get valid temp of hisi_shell->index for calculating average. */
		shell_sensor = (struct hisi_shell_sensor_t *)((u64)(hisi_shell->hisi_shell_sensor) + (u64)((long)i) * (sizeof(struct hisi_shell_sensor_t)
						+ sizeof(struct hisi_temp_tracing_t) * (u64)((long)hisi_shell->sample_count)));
		if (shell_sensor->sensor_type == TYPE_TSENS) {
			have_tsensor = 1;
			if (!shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag) {
				tsensor_good_count++;
				sum_tsensor_temp += shell_sensor->temp_tracing[hisi_shell->index].temp;
			}
		}
		if ((shell_sensor->sensor_type == TYPE_PERIPHERAL) || (shell_sensor->sensor_type == TYPE_BATTERY)) {
			have_ntc = 1;
			if (!shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag) {
				ntc_good_count++;
				sum_ntc_temp += shell_sensor->temp_tracing[hisi_shell->index].temp;
			}
		}
	}

	if ((have_tsensor && !tsensor_good_count) || (have_ntc && !ntc_good_count)) {
		pr_err("%s, %s, all tsensors or ntc sensors damaged!\n", __func__, hisi_shell->tz_dev->type);
		return 1;
	}
    /* calculating average */
	if (tsensor_good_count)
		*tsensor_avg = sum_tsensor_temp / tsensor_good_count;
	if (ntc_good_count)
		*ntc_avg = sum_ntc_temp / ntc_good_count;

	return 0;
}
/* begin of platform content */
/******************************************************************************
 Function : 
     Get battery temp.
 para mean:
     int *temp_value[out]:passage's temperature
 return value:
     0    :successfully
     other:fail     
******************************************************************************/
static int huawei_sw_get_battery_temp(int *temp_value)
{
    int rt_value = 0;

    if(!temp_value)
    {
        pr_err("%s:temp_value is NULL!\n", __func__);        
        return -EINVAL;
    }

	temp_value = battery_get_bat_temperature();

    return rt_value;
}

/******************************************************************************
 Function : 
    Get therm type.
 para mean:
    const char * passage_name : temp name
 return value:
    temp type
******************************************************************************/
static therm_type get_therm_type(const char * passage_name)
{
    if(passage_name == NULL || '\0' == passage_name[0])
        return TYPE_UNKNOWN;
	
	u32 len = strlen(passage_name);
	
    if(!strncmp(passage_name,BATTERY_NAME,(len > strlen(BATTERY_NAME))?len:strlen(BATTERY_NAME))){
        return TYPE_BATTERY;
    }else if(!strncmp(passage_name,BOARD_THERM_NAME,(len > strlen(BOARD_THERM_NAME))?len:strlen(BOARD_THERM_NAME))
           ||!strncmp(passage_name,PA_THERM_NAME,(len > strlen(PA_THERM_NAME))?len:strlen(PA_THERM_NAME))
           ||!strncmp(passage_name,CHG_IC_THERM_NAME,(len > strlen(CHG_IC_THERM_NAME))?len:strlen(CHG_IC_THERM_NAME))
           ||!strncmp(passage_name,XO_NAME,(len > strlen(XO_NAME))?len:strlen(XO_NAME))
           ||!strncmp(passage_name,COOL_NAME,(len > strlen(COOL_NAME))?len:strlen(COOL_NAME))
    ){
        return TYPE_PERIPHERAL;
    }else if(!strncmp(passage_name,GPU_THERM,(len > strlen(GPU_THERM))?len:strlen(GPU_THERM))){
        return TYPE_TSENS;
    }else {
        return TYPE_UNKNOWN;
    }
}
/* end of platform content */

/* update the invalid temp with average if shell_sensor[hisi_shell->index] is invalid
   and you can record the log if needing.
   return value:
   1--all tsensors or ntc sensors damaged;
   0--others
*/
static int handle_invalid_temp(struct hisi_shell_t *hisi_shell)
{
	int i;
	int invalid_temp = ERROR_SHELL_TEMP;
	int tsensor_avg = DEFAULT_SHELL_TEMP;
	int ntc_avg = DEFAULT_SHELL_TEMP;
	struct hisi_shell_sensor_t *shell_sensor;

	if (calc_sensor_temp_avg(hisi_shell, &tsensor_avg, &ntc_avg))
		return 1;

	for (i = 0; i < hisi_shell->sensor_count; i++) {
        /* update the invalid temp with average if shell_sensor[hisi_shell->index] is invalid */
		shell_sensor = (struct hisi_shell_sensor_t *)((u64)(hisi_shell->hisi_shell_sensor) + (u64)((long)i) * (sizeof(struct hisi_shell_sensor_t)
						+ sizeof(struct hisi_temp_tracing_t) * (u64)((long)hisi_shell->sample_count)));
		if ((shell_sensor->sensor_type == TYPE_TSENS) && shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag) {
			invalid_temp = shell_sensor->temp_tracing[hisi_shell->index].temp;
			pr_err("%s, %s, handle tsensor %d invalid data [%d], result is %d\n", __func__, hisi_shell->tz_dev->type,
					(i + 1), invalid_temp, tsensor_avg);
			shell_sensor->temp_tracing[hisi_shell->index].temp = tsensor_avg;
		}
		if (((shell_sensor->sensor_type == TYPE_PERIPHERAL) || (shell_sensor->sensor_type == TYPE_BATTERY)) && shell_sensor->temp_tracing[hisi_shell->index].temp_invalid_flag) {
			invalid_temp = shell_sensor->temp_tracing[hisi_shell->index].temp;
			pr_err("%s, %s, handle ntc sensor %d invalid data [%d], result is %d\n", __func__, hisi_shell->tz_dev->type,
					(i + 1), invalid_temp, ntc_avg);
			shell_sensor->temp_tracing[hisi_shell->index].temp = ntc_avg;
		}
        /* record the sensor,invalid_temp,average temp */
		trace_handle_invalid_temp(hisi_shell->tz_dev, i, invalid_temp, shell_sensor->temp_tracing[hisi_shell->index].temp);
	}

	return 0;
}
/* get battery temp or peripheral average temp as default temp! */
static int calc_shell_temp_first(struct hisi_shell_t *hisi_shell)
{
	int i;
	int temp = 0;
	int sum_board_sensor_temp = 0;
	int count_board_sensor = 0;
	struct hisi_shell_sensor_t *shell_sensor;


    if(huawei_sw_get_battery_temp(&hisi_shell->temp)) {   /* read fail */
        hisi_shell->temp = DEFAULT_SHELL_TEMP; /* update with default value! */
    }

	if ((temp <= hisi_shell->ntc_max_temp) && (temp >= hisi_shell->ntc_min_temp))
		return temp;
	else {
		pr_err("%s, %s, battery temperature [%d] out of range!!!\n", __func__, hisi_shell->tz_dev->type, temp / 1000);

		for (i = 0; i < hisi_shell->sensor_count; i++) {
			shell_sensor = (struct hisi_shell_sensor_t *)((u64)(hisi_shell->hisi_shell_sensor) + (u64)((long)i) * (sizeof(struct hisi_shell_sensor_t)
							+ sizeof(struct hisi_temp_tracing_t) * (u64)((long)hisi_shell->sample_count)));
			if (shell_sensor->sensor_type == TYPE_PERIPHERAL) {
				count_board_sensor++;
				sum_board_sensor_temp += shell_sensor->temp_tracing[hisi_shell->index].temp;
			}
		}
		if (count_board_sensor)
			return (sum_board_sensor_temp / count_board_sensor);
		else {
			pr_err("%s, %s, read board temperature sensor err!\n", __func__, hisi_shell->tz_dev->type);
			return DEFAULT_SHELL_TEMP;
		}
	}
}
/* calculating current note temp
   return value:
   1--all tsensors or ntc sensors damaged;
   0--others
*/
static int set_shell_temp(struct hisi_shell_t *hisi_shell, int have_invalid_temp, int index)
{
	int ret = 0;

    /* if have invalid temp,calc the sensor[hisi_shell->index] temp  */
	if (have_invalid_temp)
		ret = handle_invalid_temp(hisi_shell);

    /* if all tsensors or ntc sensors damaged,update valid_flag to 0 */
	if (ret)
		hisi_shell->valid_flag = 0;

    /* if current node is invalid and hisi_shell->index is max value */
	if (!hisi_shell->valid_flag && index >= hisi_shell->sample_count - 1)
		hisi_shell->valid_flag = 1;

    /* calc hisi_shell->sample_count temp first or after */
	if ((hisi_shell->valid_flag) && (!ret))
		hisi_shell->temp = calc_shell_temp(hisi_shell);
	else if ((!hisi_shell->valid_flag) && (!ret))
		hisi_shell->temp = calc_shell_temp_first(hisi_shell);
	else
		hisi_shell->temp = ERROR_SHELL_TEMP;

	trace_shell_temp(hisi_shell->tz_dev, hisi_shell->temp);

	return ret;
}

/******************************************************************************
 Function : 
     Update shell_XXX's temp_tracing[hisi_shell->index].temp value by hardware 
     temp value and when hisi_shell->valid_flag is true, update "shell_XXX/temp"
     value.
******************************************************************************/
static void hkadc_sample_temp(struct work_struct *work)
{
    int i, index, ret;
	int have_invalid_temp = 0;
    struct hisi_shell_t *hisi_shell;
    struct hisi_shell_sensor_t *shell_sensor;
    int temp = 0;

    if(NULL == work)return;

    hisi_shell = container_of((struct delayed_work *)work, struct hisi_shell_t, work); /*lint !e826*/

    if(NULL == hisi_shell)return;

    index = hisi_shell->index;
    mod_delayed_work(system_freezable_power_efficient_wq, (struct delayed_work *)work, round_jiffies(msecs_to_jiffies(hisi_shell->interval)));

    for (i = 0; i < hisi_shell->sensor_count ; i++) {
        /* find data(child node) address!Once timeout,only update once time per sensor[0,1,2...]  */
        shell_sensor = (struct hisi_shell_sensor_t *)((u64)(hisi_shell->hisi_shell_sensor) + (u64)((long)i) * (sizeof(struct hisi_shell_sensor_t)
						+ sizeof(struct hisi_temp_tracing_t) * (u64)((long)hisi_shell->sample_count)));

        // ret = therm_get_temp_for_huawei(shell_sensor->id,THERM_ZONE_ID,&temp);
        ret = therm_get_temp_for_huawei(shell_sensor->id,&temp);

		/* return value is not 0,means fai!In initing,read id fail */
        if (0 == ret) {   /* successfully */
            trace_shell_temp_read_succ(hisi_shell->tz_dev,
                        temp,
                        (const char *)shell_sensor->passage_name,
                        shell_sensor->id,
                        (u32)shell_sensor->sensor_type,
                        sensor_get_id((const char *)shell_sensor->passage_name)
                        );
        }
        else {   /* Fail */
            trace_shell_temp_read_fail(hisi_shell->tz_dev,
                                temp,
                                ret,
                                (const char *)shell_sensor->passage_name,
                                shell_sensor->id,
                                (u32)shell_sensor->sensor_type,
								sensor_get_id((const char *)shell_sensor->passage_name)
							   );
            temp = DEFAULT_SHELL_TEMP;			
			shell_sensor->id = sensor_get_id(shell_sensor->passage_name);
        }

#ifdef CONFIG_HISI_SHELL_TEMP_DEBUG
		if ((i == hisi_shell->channel - 1) && (hisi_shell->debug_temp)) {
			pr_err("%s, %s, channel is %d, temp is %d\n", __func__, hisi_shell->tz_dev->type, hisi_shell->channel, hisi_shell->debug_temp);
			temp = hisi_shell->debug_temp;
		}
#endif

		if (hkadc_handle_temp_data(hisi_shell, shell_sensor, (int)temp, i + 1))
			have_invalid_temp = 1;
    }

	if (!set_shell_temp(hisi_shell, have_invalid_temp, index))
		index++;
	else
		index = 0;/* if all tsensors or ntc sensors damaged,restart from 0 */

    trace_shell_temp(hisi_shell->tz_dev, hisi_shell->temp);

    hisi_shell->index = index >= hisi_shell->sample_count ? 0 : index;
}

/******************************************************************************
 Function :
     Update shell_XXX's coef.
******************************************************************************/
static int fill_sensor_coef(struct hisi_shell_t *hisi_shell, struct device_node *np)
{
    int i = 0, j = 0, coef = 0, ret = 0;
    const char *ptr_coef = NULL, *ptr_type = NULL;
    struct device_node *child;
    struct hisi_shell_sensor_t *shell_sensor;

    for_each_child_of_node(np, child) {
        /* find data(child node) address */
		shell_sensor = (struct hisi_shell_sensor_t *)((u64)hisi_shell + sizeof(struct hisi_shell_t) + (u64)((long)i) * (sizeof(struct hisi_shell_sensor_t)
                          + sizeof(struct hisi_temp_tracing_t) * (u64)((long)hisi_shell->sample_count)));

		ret = of_property_read_string(child, "type", &ptr_type); // "type" means "thermal_type"
        if (ret) {
			pr_err("%s, %s, type read err\n", __func__, hisi_shell->tz_dev->type);
            return ret;
        }

        /* get passage name */
        if(strlen(ptr_type) >= PASSAGE_ARRAY_LEN)
        {   /* len is out */
            pr_err("%s : the len of sensor %s is out\n", __func__,ptr_type);
            shell_sensor->sensor_type = TYPE_UNKNOWN;
            shell_sensor->passage_name[0] = '\0';
            /******************************************************************
            strncpy(shell_sensor->passage_name,ptr_type,PASSAGE_ARRAY_LEN-1);
            shell_sensor->passage_name[PASSAGE_ARRAY_LEN-1] = '\0';
            ******************************************************************/
            continue;
        }
        else
        {
            strncpy(shell_sensor->passage_name,ptr_type,strlen(ptr_type));
            shell_sensor->passage_name[strlen(ptr_type)] = '\0';
            /* update key info */
            shell_sensor->sensor_type = get_therm_type(ptr_type);
            shell_sensor->id = sensor_get_id(ptr_type);
        }

        for (j = 0; j < hisi_shell->sample_count; j++)  {
            ret =  of_property_read_string_index(child, "coef", j, &ptr_coef);
            if (ret) {
                pr_err("%s coef [%d] read err\n", __func__, j);
                return ret;
            }

            ret = kstrtoint(ptr_coef, 10, &coef);
            if (ret) {
                pr_err("%s kstortoint is failed\n", __func__);
                return ret;
            }
            shell_sensor->temp_tracing[j].coef = coef;
        }

        i++;
    }

    return ret;
}

static int read_sensor_temp_range(struct device_node *dev_node, int *tsensor_range, int *ntc_range)
{
	int ret = 0;
	const char *ptr_tsensor = NULL, *ptr_ntc = NULL;
	int i;
	if ((dev_node == NULL) || (tsensor_range == NULL) || (ntc_range == NULL)) {
		pr_err("%s: input err!\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < SENSOR_PARA_COUNT; i++) {
        /* read tsensor_para and ntc_para from dtsi,store to array */
		ret = of_property_read_string_index(dev_node, "tsensor_para", i, &ptr_tsensor);
		if (ret) {
			pr_err("%s tsensor_para[%d] read err\n", __func__, i);
			return -EINVAL;
		}

		ret = kstrtoint(ptr_tsensor, 10, &tsensor_range[i]);
		if (ret) {
			pr_err("%s kstrtoint is failed with tsensor_para[%d]\n", __func__, i);
			return -EINVAL;
		}

		ret = of_property_read_string_index(dev_node, "ntc_para", i, &ptr_ntc);
		if (ret) {
			pr_err("%s ntc_para[%d] read err\n", __func__, i);
			return -EINVAL;
		}

		ret = kstrtoint(ptr_ntc, 10, &ntc_range[i]);
		if (ret) {
			pr_err("%s kstrtoint is failed with ntc_para[%d]", __func__, i);
			return -EINVAL;
		}
	}

	return ret;
}



/******************************************************************************
 Function :
     1.Update shell_XXX's coef.
     2.Create soft link and node.
     3.reg calling function by interval.
******************************************************************************/
static int hisi_shell_probe(struct platform_device *pdev)
{
    struct device *dev = NULL;
    struct device_node *dev_node = NULL;
    int ret = 0;
    u32 sample_count = 0;
    int sensor_count = 0;
	int tsensor_para[SENSOR_PARA_COUNT], ntc_para[SENSOR_PARA_COUNT];
    struct device_node *np = NULL;
    struct hisi_shell_t *hisi_shell = NULL;

    if(NULL == pdev)return -ENODEV;
    dev = &(pdev->dev);
    dev_node = dev->of_node;

    if (!of_device_is_available(dev_node)) {
        dev_err(dev, "HISI shell dev not found\n");
        pr_err("%s count read err\n", __func__);
        return -ENODEV;
    }

    ret = of_property_read_s32(dev_node, "count", &sample_count);
    if (ret) {
        pr_err("%s count read err\n", __func__);
        goto exit;
    }

	if (read_sensor_temp_range(dev_node, tsensor_para, ntc_para))
		goto exit;

    np = of_find_node_by_name(dev_node, "sensors");
    if (!np) {
        pr_err("%s sensors node not found\n",__func__);
        ret = -ENODEV;
        goto exit;
    }

    sensor_count = of_get_child_count(np);
    if (sensor_count <= 0) {
        ret = -EINVAL;
        pr_err("%s sensor count read err\n", __func__);
        goto node_put;
    }

    hisi_shell = kzalloc(sizeof(struct hisi_shell_t) + (u64)((long)sensor_count) * (sizeof(struct hisi_shell_sensor_t)
                           + sizeof(struct hisi_temp_tracing_t) * (u64)((long)sample_count)), GFP_KERNEL);
    if (!hisi_shell) {
        ret = -ENOMEM;
        pr_err("%s no enough memory\n",__func__);
        goto node_put;
    }

	hisi_shell->index = 0;
	hisi_shell->valid_flag = 0;
    hisi_shell->sensor_count = sensor_count; /* the num of child sensor node */
    hisi_shell->sample_count = sample_count; /* sampling num */
	hisi_shell->tsensor_temp_step = tsensor_para[0];
	hisi_shell->tsensor_max_temp = tsensor_para[1];
	hisi_shell->tsensor_min_temp = tsensor_para[2];
	hisi_shell->ntc_temp_step = ntc_para[0];
	hisi_shell->ntc_max_temp = ntc_para[1];
	hisi_shell->ntc_min_temp = ntc_para[2];
#ifdef CONFIG_HISI_SHELL_TEMP_DEBUG
	hisi_shell->channel = 0;
	hisi_shell->debug_temp = 0;
#endif

    ret = of_property_read_u32(dev_node, "interval", &hisi_shell->interval);
    if (ret) {
        pr_err("%s interval read err\n", __func__);
        goto free_mem;
    }

    ret = of_property_read_s32(dev_node, "bias", &hisi_shell->bias);
    if (ret) {
        pr_err("%s bias read err\n", __func__);
        goto free_mem;
    }

    ret = fill_sensor_coef(hisi_shell, np);
	if (ret){
        pr_err("%s low mem!\n", __func__);
		goto free_mem;
	}

    hisi_shell->tz_dev = thermal_zone_device_register(dev_node->name,
        0, 0, hisi_shell, &shell_thermal_zone_ops, NULL, 0, 0); /* reg get func */
    if (IS_ERR(hisi_shell->tz_dev)) {
        dev_err(dev, "register thermal zone for shell failed.\n");
        ret = -ENODEV;
        goto unregister;
    }

    if(huawei_sw_get_battery_temp(&hisi_shell->temp)) {   /* read fail */
        hisi_shell->temp = DEFAULT_SHELL_TEMP; /* update with default value! */
    }

    pr_info("%s: temp %d\n", dev_node->name, hisi_shell->temp);
    of_node_put(np);

    platform_set_drvdata(pdev, hisi_shell);

    INIT_DELAYED_WORK(&hisi_shell->work, hkadc_sample_temp); /*lint !e747*/
    mod_delayed_work(system_freezable_power_efficient_wq, &hisi_shell->work, round_jiffies(msecs_to_jiffies(hisi_shell->interval)));

    /* Create soft link and node of shell_xxx(= dev_node->name) */
    ret = sysfs_create_link(&hw_thermal_info.temperature_node.device->kobj, &pdev->dev.kobj, dev_node->name);
    if (ret) {
        pr_err("%s : %s: create hw_thermal device file error: %d\n",__func__, dev_node->name, ret);
		goto cancel_work;
    }

    /* Create temp node(can be read) of shell_xxx */
    ret = sysfs_create_group(&pdev->dev.kobj, &hisi_shell_attribute_group);
	if (ret) {
		pr_err("%s : %s create shell file error: %d\n",__func__, dev_node->name, ret);
		sysfs_remove_link(&hw_thermal_info.temperature_node.device->kobj, dev_node->name);
		goto cancel_work;
	}

    return 0; /*lint !e429*/

cancel_work:
	cancel_delayed_work(&hisi_shell->work);

unregister:
    thermal_zone_device_unregister(hisi_shell->tz_dev);
free_mem:
    kfree(hisi_shell);
node_put:
    of_node_put(np);
exit:

    return ret;
}

/******************************************************************************
 Function :
    removing soft link and node.
******************************************************************************/
static int hisi_shell_remove(struct platform_device *pdev)
{
    struct hisi_shell_t *hisi_shell = platform_get_drvdata(pdev);

    if (hisi_shell) {
        platform_set_drvdata(pdev, NULL);
        thermal_zone_device_unregister(hisi_shell->tz_dev);
        kfree(hisi_shell);
    }

    return 0;
}

/******************************************************************************
 Function :
    Define the variable of dtsi file.
******************************************************************************/
/*lint -e785*/
static struct of_device_id hisi_shell_of_match[] = {
    { .compatible = "hisi,shell-temp" },
    {},
};
/*lint +e785*/
MODULE_DEVICE_TABLE(of, hisi_shell_of_match);

/******************************************************************************
 Function :
    waking up from sleeping,reset the thermal.
******************************************************************************/
int shell_temp_pm_resume(struct platform_device *pdev)
{
    struct hisi_shell_t *hisi_shell;

    pr_info("%s+\n", __func__);
    hisi_shell = platform_get_drvdata(pdev);

    if (hisi_shell) {
        if(huawei_sw_get_battery_temp(&hisi_shell->temp)) {   /* read fail */
            hisi_shell->temp = DEFAULT_SHELL_TEMP; /* update with default value! */
        }

        pr_info("%s: temp %d\n", hisi_shell->tz_dev->type, hisi_shell->temp);
        hisi_shell->index = 0;
        hisi_shell->valid_flag = 0;
#ifdef CONFIG_HISI_SHELL_TEMP_DEBUG
		hisi_shell->channel = 0;
		hisi_shell->debug_temp = 0;
#endif
    }
    pr_info("%s-\n", __func__);

    return 0;
}

/******************************************************************************
 Function :
    Define the variable of module.
******************************************************************************/
/*lint -e64 -e785 -esym(64,785,*)*/
static struct platform_driver hisi_shell_platdrv ={
    .driver ={
        .name		= "hisi-shell-temp",
        .owner		= THIS_MODULE,
        .of_match_table = hisi_shell_of_match,
    },
    .probe	= hisi_shell_probe,
    .remove	= hisi_shell_remove,
    .resume = shell_temp_pm_resume,
};
/*lint -e64 -e785 +esym(64,785,*)*/

/******************************************************************************
 Function :
    initing function of shell thermal 3.0 etc module.
******************************************************************************/
static int __init hisi_shell_init(void)
{
    int ret = 0;

    /* create huawei thermal class */
    hw_thermal_info.thermal_class = class_create(THIS_MODULE, "hw_thermal");  //lint !e64
    if (IS_ERR(hw_thermal_info.thermal_class)){
        pr_err("Huawei thermal class create error\n");
		return PTR_ERR(hw_thermal_info.thermal_class);		
    } else {
        /* create device "temp" */
        hw_thermal_info.temperature_node.device =
        device_create(hw_thermal_info.thermal_class, NULL, 0, NULL, "temp");
        if (IS_ERR(hw_thermal_info.temperature_node.device)) {
            pr_err("Huawei temp node device create error\n");
            class_destroy(hw_thermal_info.thermal_class);
            hw_thermal_info.thermal_class = NULL;
			return PTR_ERR(hw_thermal_info.temperature_node.device);			
        } else {
            ret = device_create_file(hw_thermal_info.temperature_node.device, &dev_attr_ambient);
            if (ret) {
				pr_err("hw_thermal:ambient node create error\n");
                device_destroy(hw_thermal_info.thermal_class, 0);
                class_destroy(hw_thermal_info.thermal_class);
                hw_thermal_info.thermal_class = NULL;
				return ret;
            }
        }
    }

    return platform_driver_register(&hisi_shell_platdrv); /*lint !e64*/
}

/******************************************************************************
 Function :
    exiting function of module.
******************************************************************************/
static void __exit hisi_shell_exit(void)
{
    if (hw_thermal_info.thermal_class) {
        device_destroy(hw_thermal_info.thermal_class, 0);
        class_destroy(hw_thermal_info.thermal_class);
    }
    platform_driver_unregister(&hisi_shell_platdrv);
}
/*lint -e528 -esym(528,*)*/
module_init(hisi_shell_init);
module_exit(hisi_shell_exit);
/*lint -e528 +esym(528,*)*/

/*lint -e753 -esym(753,*)*/
MODULE_LICENSE("GPL v2");
/*lint -e753 +esym(753,*)*/
