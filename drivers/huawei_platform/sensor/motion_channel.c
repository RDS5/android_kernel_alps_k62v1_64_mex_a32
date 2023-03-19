

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <motion.h>
#include <SCP_sensorHub.h>
#include <hwmsensor.h>
#include <sensor_event.h>
#include <linux/platform_device.h>
#include <stepsignhub.h>



static bool motion_status[MOTION_TYPE_END]={0};
extern int motion_route_init(void);
extern void motion__route_destroy(void);
extern ssize_t motion_route_read(char __user *buf, size_t count);
extern ssize_t motion_route_write(char *buf, size_t count);


//static struct class *color_sensor_class;


struct normal_motion_result{
        uint8_t motion;
        uint8_t result;      
        int8_t status;     
        uint8_t data_len;
};


struct motions_cmd_map {
	int mhb_ioctl_app_cmd;
	int motion_type;
	obj_cmd_t cmd;
	obj_sub_cmd_t subcmd;
};
static const struct motions_cmd_map motions_cmd_map_tab[] = {
	{MHB_IOCTL_MOTION_START, -1, CMD_CMN_OPEN_REQ, SUB_CMD_NULL_REQ},
	{MHB_IOCTL_MOTION_STOP, -1, CMD_CMN_CLOSE_REQ, SUB_CMD_NULL_REQ},
	{MHB_IOCTL_MOTION_ATTR_START, -1, CMD_CMN_CONFIG_REQ, SUB_CMD_MOTION_ATTR_ENABLE_REQ},
	{MHB_IOCTL_MOTION_ATTR_STOP, -1, CMD_CMN_CONFIG_REQ, SUB_CMD_MOTION_ATTR_DISABLE_REQ},
	{MHB_IOCTL_MOTION_INTERVAL_SET, -1, CMD_CMN_INTERVAL_REQ, SUB_CMD_NULL_REQ},
};

static char *motion_type_str[] = {
	[MOTION_TYPE_START] = "start",
	[MOTION_TYPE_PICKUP] = "pickup",
	[MOTION_TYPE_FLIP] = "flip",
	[MOTION_TYPE_PROXIMITY] = "proximity",
	[MOTION_TYPE_SHAKE] = "shake",
	[MOTION_TYPE_TAP] = "tap",
	[MOTION_TYPE_TILT_LR] = "tilt_lr",
	[MOTION_TYPE_ROTATION] = "rotation",
	[MOTION_TYPE_POCKET] = "pocket",
	[MOTION_TYPE_ACTIVITY] = "activity",
	[MOTION_TYPE_TAKE_OFF] = "take_off",
	[MOTION_TYPE_EXTEND_STEP_COUNTER] = "ext_step_counter",
	[MOTION_TYPE_EXT_LOG] = "ext_log",
	[MOTION_TYPE_HEAD_DOWN] = "head_down",
	[MOTION_TYPE_PUT_DOWN] = "put_down",
	[MOTION_TYPE_SIDEGRIP] = "sidegrip",
	[MOTION_TYPE_END] = "end",
};


static void update_motion_info(obj_cmd_t cmd, motion_type_t type)
{
	if (!(MOTION_TYPE_START <= type && type < MOTION_TYPE_END))
		return;

	switch (cmd) {
	case CMD_CMN_OPEN_REQ:
		motion_status[type] = true;
		break;

	case CMD_CMN_CLOSE_REQ:
		motion_status[type] = false;
		break;

	default:
		hwlog_err("unknown cmd type in %s\n", __func__);
		break;
	}
}

static bool need_motion_close(void)
{
	int i =0;
	for(i=0; i< sizeof(motion_status) / sizeof(motion_status[0]); i++){
		if(motion_status[i]) {
			break;
		}
	}
	if( i==sizeof(motion_status) / sizeof(motion_status[0])){
		return true;
	}
	return false;
}
static bool need_motion_open(void)
{
	int i = 0;
	int count = 0;
	for(i=0; i< sizeof(motion_status) / sizeof(motion_status[0]); i++){
		if(motion_status[i]) {
			count++;
		}
	}
	if(1 == count){//only first need open
		return true;
	}
	return false;
}
static int send_motion_cmd_internal(obj_cmd_t cmd, obj_sub_cmd_t subcmd, motion_type_t type)
{
	uint8_t app_config[2] = { 0, };
	int en = 0;
	app_config[0] = type;
	app_config[1] = cmd;

	if (CMD_CMN_OPEN_REQ == cmd) {
		en = 1;
		if(need_motion_open()){
	    	sensor_enable_to_hub(ID_HW_MOTION, en);
			hwlog_info("send_motion_cmd send enable  cmd!");
		}
		sensor_cfg_to_hub(ID_HW_MOTION, app_config, sizeof(app_config));
		hwlog_info("send_motion_cmd config cmd:%d motion: %s !", cmd, motion_type_str[type]);
	} else if (CMD_CMN_CLOSE_REQ == cmd) {
		/*send config cmd to close motion type*/
		en = 0;
		sensor_cfg_to_hub(ID_HW_MOTION, app_config, sizeof(app_config));		
		hwlog_info("send_motion_cmd config cmd:%d motion: %s !", cmd, motion_type_str[type]);
		if(need_motion_close()){
			sensor_enable_to_hub(ID_HW_MOTION, en);
			hwlog_info("send_motion_cmd send disable  cmd!");
		}

	} else {
		hwlog_err("send_motion_cmd not support cmd!\n");
		return -EINVAL;
	}

	return 0;
}

static int send_motion_cmd(unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int argvalue = 0;
	int i;

	for (i = 0; i < sizeof(motions_cmd_map_tab) / sizeof(motions_cmd_map_tab[0]); ++i) {
		if (motions_cmd_map_tab[i].mhb_ioctl_app_cmd == cmd) {
			break;
		}
	}

	if (sizeof(motions_cmd_map_tab) / sizeof(motions_cmd_map_tab[0]) == i) {
		hwlog_err("send_motion_cmd unknown cmd %d in parse_motion_cmd!\n", cmd);
		return -EFAULT;
	}

	if (copy_from_user(&argvalue, argp, sizeof(argvalue)))
		return -EFAULT;

	if (!(MOTION_TYPE_START <= argvalue && argvalue < MOTION_TYPE_END)) {
		hwlog_err("error motion type %d in %s\n", argvalue, __func__);
		return -EINVAL;
	}
#ifdef CONFIG_MTK_STEPSIGNHUB
	if(argvalue == MOTION_TYPE_EXTEND_STEP_COUNTER){
		//operate pedo
		hwlog_info("%s operate ext step count cmd = %d\n", __func__, motions_cmd_map_tab[i].cmd);
		ext_step_counter_enable(motions_cmd_map_tab[i].cmd);
		return 0;
	}
#endif
	update_motion_info(motions_cmd_map_tab[i].cmd, argvalue);

	return send_motion_cmd_internal(motions_cmd_map_tab[i].cmd,
		motions_cmd_map_tab[i].subcmd, argvalue);
}

#define MAX_STEP_REPORT_LEN 37
int ext_step_counter_report(struct data_unit_t *event,
	void *reserved)
{
	//struct hw_step_counter_t_v2 ext_hw_step_counter_t;
	char step_counter_data[MAX_STEP_REPORT_LEN]={0};//define max step counter data length
	u_int16_t record_count =2000;//define default 2000
	u_int16_t capability = 1;//capability set to default 1
	 u_int32_t total_step_count =0;
	if(event == NULL){
		return -1;
	}
	memset(step_counter_data, 0, sizeof(step_counter_data));
	if(event->flush_action == FLUSH_ACTION || event->flush_action == DATA_ACTION){
		hwlog_info("%s, step counter recv ext data = %d\n", __func__, event->step_counter_t.accumulated_step_count);
		/*ext_hw_step_counter_t.motion = MOTION_TYPE_EXTEND_STEP_COUNTER;
		ext_hw_step_counter_t.record_count = 2000;
		ext_hw_step_counter_t.capability = 1;
		ext_hw_step_counter_t.total_step_count = event->step_counter_t.accumulated_step_count;*/
		total_step_count =  event->step_counter_t.accumulated_step_count;
		step_counter_data[0]=MOTION_TYPE_EXTEND_STEP_COUNTER;
		memcpy(&step_counter_data[5],&record_count,sizeof(u_int16_t));
		memcpy(&step_counter_data[7],&capability,sizeof(u_int16_t));
		memcpy(&step_counter_data[9],&total_step_count,sizeof(u_int32_t));
		motion_route_write(step_counter_data,sizeof(step_counter_data));
	}
	return 0;
}
EXPORT_SYMBOL_GPL(ext_step_counter_report);

static int motion_channel_recv_data(struct data_unit_t *event,
	void *reserved)
{
	struct normal_motion_result normal_motion_result_t;
	if (event->flush_action == FLUSH_ACTION)
		hwlog_info("stat do not support flush\n");
	else if (event->flush_action == DATA_ACTION){
		hwlog_info("motion recv action data0=%d,data1=%d,data2=%d. \n",event->hw_motion_event.data[0],event->hw_motion_event.data[1],event->hw_motion_event.data[2]);
		if(MOTION_TYPE_PICKUP == event->hw_motion_event.data[0]){
			normal_motion_result_t.motion = MOTION_TYPE_PICKUP;
			normal_motion_result_t.result=(uint8_t)event->hw_motion_event.data[1];
			normal_motion_result_t.status=0;
			normal_motion_result_t.data_len=0;
			motion_route_write((char*)&normal_motion_result_t,sizeof(struct normal_motion_result));
		}else{
			hwlog_info("do not support type=%d. \n",event->hw_motion_event.data[0]);
		}
	}
	return 0;
}


/*******************************************************************************************
Function:       mhb_read
Description:   read /dev/motionhub
Data Accessed:  no
Data Updated:   no
Input:          struct file *file, char __user *buf, size_t count, loff_t *pos
Output:         no
Return:         length of read data
*******************************************************************************************/
static ssize_t mhb_read(struct file *file, char __user *buf, size_t count,
			loff_t *pos)
{

	return motion_route_read(buf,count);
}

/*******************************************************************************************
Function:       mhb_write
Description:   write to /dev/motionhub, do nothing now
Data Accessed:  no
Data Updated:   no
Input:          struct file *file, const char __user *data, size_t len, loff_t *ppos
Output:         no
Return:         length of write data
*******************************************************************************************/
static ssize_t mhb_write(struct file *file, const char __user *data,
			 size_t len, loff_t *ppos)
{
	hwlog_info("%s need to do...\n", __func__);

	return len;
}

/*******************************************************************************************
Function:       mhb_ioctl
Description:   ioctrl function to /dev/motionhub, do open, close motion, or set interval and attribute to motion
Data Accessed:  no
Data Updated:   no
Input:          struct file *file, unsigned int cmd, unsigned long arg
			cmd indicates command, arg indicates parameter
Output:         no
Return:         result of ioctrl command, 0 successed, -ENOTTY failed
*******************************************************************************************/
static long mhb_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case MHB_IOCTL_MOTION_START:
	case MHB_IOCTL_MOTION_STOP:
	case MHB_IOCTL_MOTION_ATTR_START:
	case MHB_IOCTL_MOTION_ATTR_STOP:
		break;
	default:
		hwlog_err("%s unknown cmd : %d\n", __func__, cmd);
		return -ENOTTY;
	}
	return send_motion_cmd(cmd, arg);
}

/*******************************************************************************************
Function:       mhb_open
Description:   open to /dev/motionhub, do nothing now
Data Accessed:  no
Data Updated:   no
Input:          struct inode *inode, struct file *file
Output:         no
Return:         result of open
*******************************************************************************************/
static int mhb_open(struct inode *inode, struct file *file)
{
	hwlog_info("%s ok!\n", __func__);
	return 0;
}

/*******************************************************************************************
Function:       mhb_release
Description:   releaseto /dev/motionhub, do nothing now
Data Accessed:  no
Data Updated:   no
Input:          struct inode *inode, struct file *file
Output:         no
Return:         result of release
*******************************************************************************************/
static int mhb_release(struct inode *inode, struct file *file)
{
	hwlog_info("%s ok!\n", __func__);
	return 0;
}


/*******************************************************************************************
Description:   file_operations to motion
*******************************************************************************************/
static const struct file_operations mhb_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = mhb_read,
	.write = mhb_write,
	.unlocked_ioctl = mhb_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mhb_ioctl,
#endif
	.open = mhb_open,
	.release = mhb_release,
};

/*******************************************************************************************
Description:   miscdevice to motion
*******************************************************************************************/
static struct miscdevice motionhub_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "motionhub",
	.fops = &mhb_fops,
};

ssize_t motion_debug_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{

	bool state;
	if(NULL == dev || NULL == attr || NULL == buf)
	{
		hwlog_err("[%s] input NULL!! \n", __func__);
		return -1;
	}
	hwlog_info("[%s] motion_debug_store in!! \n", __func__);
	strtobool(buf, &state);
	hwlog_info("[%s]  enable = %d.\n", __func__,state);
	if(state){
		update_motion_info(CMD_CMN_OPEN_REQ, MOTION_TYPE_PICKUP);
		send_motion_cmd_internal(CMD_CMN_OPEN_REQ,SUB_CMD_NULL_REQ,MOTION_TYPE_PICKUP);
	}else{
	    update_motion_info(CMD_CMN_CLOSE_REQ, MOTION_TYPE_PICKUP);
		send_motion_cmd_internal(CMD_CMN_CLOSE_REQ,SUB_CMD_NULL_REQ,MOTION_TYPE_PICKUP);
	}		
	return size;
}

#if 0
DEVICE_ATTR(motion_debug, 0660, NULL, motion_debug_store);

static struct attribute *color_sensor_attributes[] = {
	&dev_attr_motion_debug.attr,
	NULL,
};

static const struct attribute_group color_sensor_attr_group = {
	.attrs = color_sensor_attributes
};

static const struct attribute_group color_sensor_attr_groups[] = {
	&color_sensor_attr_group
};
#endif

/*******************************************************************************************
Function:       motionhub_init
Description:   apply kernel buffer, register motionhub_miscdev
Data Accessed:  no
Data Updated:   no
Input:          void
Output:        void
Return:        result of function, 0 successed, else false
*******************************************************************************************/
static int __init motionhub_init(void)
{
	int ret;

	ret = misc_register(&motionhub_miscdev);
	if (ret != 0) {
		hwlog_err("cannot register miscdev err=%d\n", ret);
		goto exit1;
	}
	ret = scp_sensorHub_data_registration(ID_HW_MOTION,
		motion_channel_recv_data);
	if (ret != 0) {
		hwlog_err("cannot register cp_sensorHub_data_registration  err=%d\n", ret);
		goto exit2;
	}
	ret = motion_route_init();
	if (ret != 0) {
		hwlog_err("cannot motion_route_init  err=%d\n", ret);
		goto exit2;
	}
	/*debuge file node*/
	//color_sensor_class = class_create(THIS_MODULE, "ap_sensor");
	//color_sensor_class->dev_groups = &color_sensor_attr_group;
	return ret;
exit1:
	return -1;
exit2:
	misc_deregister(&motionhub_miscdev);
	return -1;

}

/*******************************************************************************************
Function:       motionhub_exit
Description:   release kernel buffer, deregister motionhub_miscdev
Data Accessed:  no
Data Updated:   no
Input:          void
Output:        void
Return:        void
*******************************************************************************************/
static void __exit motionhub_exit(void)
{
	misc_deregister(&motionhub_miscdev);
	motion__route_destroy();
	hwlog_info("exit %s\n", __func__);
}

late_initcall_sync(motionhub_init);
module_exit(motionhub_exit);

MODULE_AUTHOR("MotionHub <smartphone@huawei.com>");
MODULE_DESCRIPTION("MotionHub driver");
MODULE_LICENSE("GPL");
