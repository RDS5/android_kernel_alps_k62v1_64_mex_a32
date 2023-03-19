/*
 * tfa98xx.c   tfa98xx codec module
 *
 * Copyright 2014-2017 NXP Semiconductors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "tfa98xx.h"
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/regmap.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/i2c-dev.h>
#include "config.h"
#include "tfa.h"
#include "smartpakit.h"
#include <huawei_platform/charger/huawei_charger.h>
#include <tfa98xx_genregs_N1C.h>
#include "dsm_audio/dsm_audio.h"

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#ifdef CONFIG_PM
#include <linux/suspend.h>
#endif
#include <linux/power_supply.h>
/* required for enum tfa9912_irq */
#include "tfa98xx_tfafieldnames.h"

#define TFA98XX_VERSION	TFA98XX_API_REV_STR

#define I2C_RETRIES 50
#define I2C_RETRY_DELAY 5 /* ms */
#define UNUSED_PARAM(x) ((void)(x))
#define I2C_TRANSFER_MAX_SIZE (4*1024)
#define TFA98XX_I2C_W 0
#define TFA98XX_I2C_R 1
#define SMARTPA_VENDOR 1 // NXP
#define SMARTPA_MODEL  "tfa9894"

/* Change volume selection behavior:
 * Uncomment following line to generate a profile change when updating
 * a volume control (also changes to the profile of the modified  volume
 * control)
 */
#define TF98XX_MAX_DSP_START_TRY_COUNT	10
#define FW_NAME_SIZE 30
#define FW_NAME_BASE "tfa98xx"
static char *boxid_table[] = {"NONE", "AAC", "GK", "GD", "LC", "LX"};

/* data accessible by all instances */
/* Memory pool used for DSP messages */
static struct kmem_cache *tfa98xx_cache;  
/* Mutex protected data */
static DEFINE_MUTEX(tfa98xx_mutex);
static LIST_HEAD(tfa98xx_device_list);
static int tfa98xx_device_count;
static int tfa98xx_sync_count;
static LIST_HEAD(profile_list);        /* list of user selectable profiles */
static int tfa98xx_mixer_profiles; /* number of user selectable profiles */
static int tfa98xx_mixer_profile;  /* current mixer profile */
static nxpTfaContainer_t *tfa98xx_container;

static int tfa98xx_kmsg_regs;
static int tfa98xx_ftrace_regs;

static char fw_name[FW_NAME_SIZE];
static char *dsp_fw_name = fw_name;

module_param(dsp_fw_name, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dsp_fw_name, "TFA98xx DSP firmware (container file) name.");

static int trace_level;
module_param(trace_level, int, S_IRUGO);
MODULE_PARM_DESC(trace_level, "TFA98xx debug trace level (0=off, bits:1=verbose,2=regdmesg,3=regftrace,4=timing).");

static char *dflt_prof_name = "";
module_param(dflt_prof_name, charp, S_IRUGO);

static int no_start = 0;
module_param(no_start, int, S_IRUGO);
MODULE_PARM_DESC(no_start, "do not start the work queue; for debugging via user\n");

static int no_reset = 0;
module_param(no_reset, int, S_IRUGO);
MODULE_PARM_DESC(no_reset, "do not use the reset line; for debugging via user\n");

static int pcm_sample_format = 0;
module_param(pcm_sample_format, int, S_IRUGO);
MODULE_PARM_DESC(pcm_sample_format, "PCM sample format: 0=S16_LE, 1=S24_LE, 2=S32_LE\n");

static int pcm_no_constraint = 0;
module_param(pcm_no_constraint, int, S_IRUGO);
MODULE_PARM_DESC(pcm_no_constraint, "do not use constraints for PCM parameters\n");

static int tfa98xx_get_fssel(unsigned int rate);
static void tfa98xx_interrupt_enable(struct tfa98xx *tfa98xx, bool enable);

static int get_profile_from_list(char *buf, int id);
static int get_profile_id_for_sr(int id, unsigned int rate);

struct tfa98xx_rate {
	unsigned int rate;
	unsigned int fssel;
};

static const struct tfa98xx_rate rate_to_fssel[] = {
	{ 8000, 0 },
	{ 11025, 1 },
	{ 12000, 2 },
	{ 16000, 3 },
	{ 22050, 4 },
	{ 24000, 5 },
	{ 32000, 6 },
	{ 44100, 7 },
	{ 48000, 8 },
};

#define TFA98XX_MAGIC_NUMBER 0x112233
#define TFA98XX_SPK_POWER_OFF				_IOWR(TFA98XX_MAGIC_NUMBER, 3, \
							void *)
#define TFA98XX_SPK_SWITCH_CONFIGURATION	_IOWR(TFA98XX_MAGIC_NUMBER, 5, \
							void *)	
#define SMARTPA_SPK_SWITCH_FIRMWARE         _IOWR(TFA98XX_MAGIC_NUMBER, 13,	\
							void *)
#define TFA98XX_SPK_SET_DEFAULT_CALIB	_IOWR(TFA98XX_MAGIC_NUMBER, 15, void *)
#define TFA98XX_SPK_GET_CALIB_STATE	_IOWR(TFA98XX_MAGIC_NUMBER, 16, void *)

#ifdef CONFIG_COMPAT
#define TFA98XX_SPK_POWER_OFF_COMPAT				_IOWR(TFA98XX_MAGIC_NUMBER, 3, \
							compat_uptr_t)
#define TFA98XX_SPK_SWITCH_CONFIGURATION_COMPAT		_IOWR(TFA98XX_MAGIC_NUMBER, 5, \
							compat_uptr_t)
#define SMARTPA_SPK_SWITCH_FIRMWARE_COMPAT          _IOWR(TFA98XX_MAGIC_NUMBER, 13,	\
							compat_uptr_t)
#define TFA98XX_SPK_SET_DEF_CALIB_COMPAT	_IOWR(TFA98XX_MAGIC_NUMBER, \
							15, compat_uptr_t)
#define TFA98XX_SPK_GET_CALIB_STATE_COMPAT	_IOWR(TFA98XX_MAGIC_NUMBER, \
							16, compat_uptr_t)
#endif

static inline char *tfa_cont_profile_name(struct tfa98xx *tfa98xx, int prof_idx)
{
	if (tfa98xx->tfa->cnt == NULL)
		return NULL;
	return tfaContProfileName(tfa98xx->tfa->cnt, tfa98xx->tfa->dev_idx, prof_idx);
}

static enum tfa_error tfa98xx_write_re25(struct tfa_device *tfa, int value)
{
	enum tfa_error err;

	/* clear MTPEX */
	err = tfa_dev_mtp_set(tfa, TFA_MTP_EX, 0);
	if (err == tfa_error_ok) {
		/* set RE25 in shadow regiser */
		err = tfa_dev_mtp_set(tfa, TFA_MTP_RE25_PRIM, value);
	}
	if (err == tfa_error_ok) {
		/* set MTPEX to copy RE25 into MTP  */
		err = tfa_dev_mtp_set(tfa, TFA_MTP_EX, 2);
	}

	return err;
}

/* Wrapper for tfa start */
static enum tfa_error tfa98xx_tfa_start(struct tfa98xx *tfa98xx, int next_profile, int vstep)
{
	enum tfa_error err;
	ktime_t start_time, stop_time;
	u64 delta_time;

	if (trace_level & 8) {
		start_time = ktime_get_boottime();
	}

	err = tfa_dev_start(tfa98xx->tfa, next_profile, vstep);

	if (trace_level & 8) {
		stop_time = ktime_get_boottime();
		delta_time = ktime_to_ns(ktime_sub(stop_time, start_time));
		do_div(delta_time, 1000);
		dev_dbg(&tfa98xx->i2c->dev, "tfa_dev_start(%d,%d) time = %lld us\n",
		        next_profile, vstep, delta_time);
	}

	if ((err == tfa_error_ok) && (tfa98xx->set_mtp_cal)) {
		enum tfa_error err_cal;
		err_cal = tfa98xx_write_re25(tfa98xx->tfa, tfa98xx->cal_data);
		if (err_cal != tfa_error_ok) {
			pr_err("Error, setting calibration value in mtp, err=%d\n", err_cal);
		} else {
			tfa98xx->set_mtp_cal = false;
			pr_info("Calibration value (%d) set in mtp\n",
			        tfa98xx->cal_data);
		}
	}

	/* Remove sticky bit by reading it once */
	tfa_get_noclk(tfa98xx->tfa);

	/* A cold start erases the configuration, including interrupts setting.
	 * Restore it if required
	 */
	tfa98xx_interrupt_enable(tfa98xx, true);

	return err;
}

#ifdef CONFIG_DEBUG_FS
/* OTC reporting
 * Returns the MTP0 OTC bit value
 */
static int tfa98xx_dbgfs_otc_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int value;

	mutex_lock(&tfa98xx->dsp_lock);
	value = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_OTC);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (value < 0) {
		pr_err("[0x%x] Unable to check DSP access: %d\n", tfa98xx->i2c->addr, value);
		return -EIO;
	}

	*val = value;
	pr_debug("[0x%x] OTC : %d\n", tfa98xx->i2c->addr, value);

	return 0;
}

static int tfa98xx_dbgfs_otc_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa_error err;

	if (val != 0 && val != 1) {
		pr_err("[0x%x] Unexpected value %llu\n", tfa98xx->i2c->addr, val);
		return -EINVAL;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	err = tfa_dev_mtp_set(tfa98xx->tfa, TFA_MTP_OTC, val);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (err != tfa_error_ok) {
		pr_err("[0x%x] Unable to check DSP access: %d\n", tfa98xx->i2c->addr, err);
		return -EIO;
	}

	pr_debug("[0x%x] OTC < %llu\n", tfa98xx->i2c->addr, val);

	return 0;
}

static int tfa98xx_dbgfs_mtpex_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int value;

	mutex_lock(&tfa98xx->dsp_lock);
	value = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_EX);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (value < 0) {
		pr_err("[0x%x] Unable to check DSP access: %d\n", tfa98xx->i2c->addr, value);
		return -EIO;
	}


	*val = value;
	pr_debug("[0x%x] MTPEX : %d\n", tfa98xx->i2c->addr, value);

	return 0;
}

static int tfa98xx_dbgfs_mtpex_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa_error err;

	if (val != 0) {
		pr_err("[0x%x] Can only clear MTPEX (0 value expected)\n", tfa98xx->i2c->addr);
		return -EINVAL;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	err = tfa_dev_mtp_set(tfa98xx->tfa, TFA_MTP_EX, val);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (err != tfa_error_ok) {
		pr_err("[0x%x] Unable to check DSP access: %d\n", tfa98xx->i2c->addr, err);
		return -EIO;
	}

	pr_debug("[0x%x] MTPEX < 0\n", tfa98xx->i2c->addr);

	return 0;
}

static int tfa98xx_dbgfs_temp_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);

	mutex_lock(&tfa98xx->dsp_lock);
	*val = tfa98xx_get_exttemp(tfa98xx->tfa);
	mutex_unlock(&tfa98xx->dsp_lock);

	pr_debug("[0x%x] TEMP : %llu\n", tfa98xx->i2c->addr, *val);

	return 0;
}

static int tfa98xx_dbgfs_temp_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);

	mutex_lock(&tfa98xx->dsp_lock);
	tfa98xx_set_exttemp(tfa98xx->tfa, (short)val);
	mutex_unlock(&tfa98xx->dsp_lock);

	pr_debug("[0x%x] TEMP < %llu\n", tfa98xx->i2c->addr, val);

	return 0;
}

static ssize_t tfa98xx_dbgfs_start_set(struct file *file,
				     const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa_error ret;
	char buf[32];
	const char ref[] = "please calibrate now";
	int buf_size, cal_profile = 0;

	/* check string length, and account for eol */
	if (count > sizeof(ref) + 1 || count < (sizeof(ref) - 1))
		return -EINVAL;

	buf_size = min(count, (size_t)(sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	/* Compare string, excluding the trailing \0 and the potentials eol */
	if (strncmp(buf, ref, sizeof(ref) - 1))
		return -EINVAL;

	mutex_lock(&tfa98xx->dsp_lock);
	ret = tfa_calibrate(tfa98xx->tfa);
	if (ret == tfa_error_ok) {
		cal_profile = tfaContGetCalProfile(tfa98xx->tfa);
		if (cal_profile < 0) {
			pr_warn("[0x%x] Calibration profile not found\n",
			        tfa98xx->i2c->addr);
		}

		ret = tfa98xx_tfa_start(tfa98xx, cal_profile, tfa98xx->vstep);
	}
	if (ret == tfa_error_ok)
			tfa_dev_set_state(tfa98xx->tfa, TFA_STATE_UNMUTE,0);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (ret) {
		pr_info("[0x%x] Calibration start failed (%d)\n", tfa98xx->i2c->addr, ret);
		return -EIO;
	} else {
		pr_info("[0x%x] Calibration started\n", tfa98xx->i2c->addr);
	}

	return count;
}

static ssize_t tfa98xx_dbgfs_r_read(struct file *file,
				     char __user *user_buf, size_t count,
				     loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	char *str;
	uint16_t status;
	int ret;

	mutex_lock(&tfa98xx->dsp_lock);

	/* Need to ensure DSP is access-able, use mtp read access for this
	 * purpose
	 */
	ret = tfa98xx_get_mtp(tfa98xx->tfa, &status);
	if (ret) {
		ret = -EIO;
		pr_err("[0x%x] MTP read failed\n", tfa98xx->i2c->addr);
		goto r_c_err;
	}

	ret = tfaRunSpeakerCalibration(tfa98xx->tfa);
	if (ret) {
		ret = -EIO;
		pr_err("[0x%x] calibration failed\n", tfa98xx->i2c->addr);
		goto r_c_err;
	}

	str = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!str) {
		ret = -ENOMEM;
		pr_err("[0x%x] memory allocation failed\n", tfa98xx->i2c->addr);
		goto r_c_err;
	}

	if (tfa98xx->tfa->spkr_count > 1) {
		ret = snprintf(str, PAGE_SIZE,
		              "Prim:%d mOhms, Sec:%d mOhms\n",
		              tfa98xx->tfa->mohm[0],
		              tfa98xx->tfa->mohm[1]);
	} else {
		ret = snprintf(str, PAGE_SIZE,
		              "Prim:%d mOhms\n",
		              tfa98xx->tfa->mohm[0]);
	}

	pr_debug("[0x%x] calib_done: %s", tfa98xx->i2c->addr, str);

	if (ret < 0)
		goto r_err;

	ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);

r_err:
	kfree(str);
r_c_err:
	mutex_unlock(&tfa98xx->dsp_lock);
	return ret;
}

static ssize_t tfa98xx_dbgfs_version_read(struct file *file,
				     char __user *user_buf, size_t count,
				     loff_t *ppos)
{
	char str[] = TFA98XX_VERSION "\n";
	int ret;

	ret = simple_read_from_buffer(user_buf, count, ppos, str, sizeof(str));

	return ret;
}

static ssize_t tfa98xx_dbgfs_dsp_state_get(struct file *file,
				     char __user *user_buf, size_t count,
				     loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	char *str;

	switch (tfa98xx->dsp_init) {
	case TFA98XX_DSP_INIT_STOPPED:
		str = "Stopped\n";
		break;
	case TFA98XX_DSP_INIT_RECOVER:
		str = "Recover requested\n";
		break;
	case TFA98XX_DSP_INIT_FAIL:
		str = "Failed init\n";
		break;
	case TFA98XX_DSP_INIT_PENDING:
		str =  "Pending init\n";
		break;
	case TFA98XX_DSP_INIT_DONE:
		str = "Init complete\n";
		break;
	default:
		str = "Invalid\n";
	}

	pr_debug("[0x%x] dsp_state : %s\n", tfa98xx->i2c->addr, str);

	ret = simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));
	return ret;
}

static ssize_t tfa98xx_dbgfs_dsp_state_set(struct file *file,
				     const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa_error ret;
	char buf[32];
	const char start_cmd[] = "start";
	const char stop_cmd[] = "stop";
	const char mon_start_cmd[] = "monitor start";
	const char mon_stop_cmd[] = "monitor stop";
	int buf_size;

	buf_size = min(count, (size_t)(sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	/* Compare strings, excluding the trailing \0 */
	if (!strncmp(buf, start_cmd, sizeof(start_cmd) - 1)) {
		pr_info("[0x%x] Manual triggering of dsp start...\n", tfa98xx->i2c->addr);
		mutex_lock(&tfa98xx->dsp_lock);
		ret = tfa98xx_tfa_start(tfa98xx, tfa98xx->profile, tfa98xx->vstep);
		mutex_unlock(&tfa98xx->dsp_lock);
		pr_debug("[0x%x] tfa_dev_start complete: %d\n",  tfa98xx->i2c->addr, ret);
	} else if (!strncmp(buf, stop_cmd, sizeof(stop_cmd) - 1)) {
		pr_info("[0x%x] Manual triggering of dsp stop...\n",  tfa98xx->i2c->addr);
		mutex_lock(&tfa98xx->dsp_lock);
		ret = tfa_dev_stop(tfa98xx->tfa);
		mutex_unlock(&tfa98xx->dsp_lock);
		pr_debug("[0x%x] tfa_dev_stop complete: %d\n",  tfa98xx->i2c->addr, ret);
	} else if (!strncmp(buf, mon_start_cmd, sizeof(mon_start_cmd) - 1)) {
		pr_info("[0x%x] Manual start of monitor thread...\n",  tfa98xx->i2c->addr);
		queue_delayed_work(tfa98xx->tfa98xx_wq,
					&tfa98xx->monitor_work, HZ);
	} else if (!strncmp(buf, mon_stop_cmd, sizeof(mon_stop_cmd) - 1)) {
		pr_info("[0x%x] Manual stop of monitor thread...\n",  tfa98xx->i2c->addr);
		cancel_delayed_work_sync(&tfa98xx->monitor_work);
	} else {
		return -EINVAL;
	}

	return count;
}

static ssize_t tfa98xx_dbgfs_fw_state_get(struct file *file,
				     char __user *user_buf, size_t count,
				     loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	char *str;

	switch (tfa98xx->dsp_fw_state) {
	case TFA98XX_DSP_FW_NONE:
		str = "None\n";
		break;
	case TFA98XX_DSP_FW_PENDING:
		str = "Pending\n";
		break;
	case TFA98XX_DSP_FW_FAIL:
		str = "Fail\n";
		break;
	case TFA98XX_DSP_FW_OK:
		str =  "Ok\n";
		break;
	default:
		str = "Invalid\n";
	}

	pr_debug("[0x%x] fw_state : %s", tfa98xx->i2c->addr, str);

	return simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));
}

static ssize_t tfa98xx_dbgfs_rpc_read(struct file *file,
				     char __user *user_buf, size_t count,
				     loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	uint8_t *buffer;
	enum Tfa98xx_Error error;

	if (tfa98xx->tfa == NULL) {
		pr_debug("[0x%x] dsp is not available\n", tfa98xx->i2c->addr);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	buffer = kmalloc(count, GFP_KERNEL);
	if (buffer == NULL) {
		pr_debug("[0x%x] can not allocate memory\n", tfa98xx->i2c->addr);
		return -ENOMEM;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	error = dsp_msg_read(tfa98xx->tfa, count, buffer);
	mutex_unlock(&tfa98xx->dsp_lock);
	if (error != Tfa98xx_Error_Ok) {
		pr_debug("[0x%x] dsp_msg_read error: %d\n", tfa98xx->i2c->addr, error);
		kfree(buffer);
		return -EFAULT;
	}

	ret = copy_to_user(user_buf, buffer, count);
	kfree(buffer);
	if (ret)
		return -EFAULT;

	*ppos += count;
	return count;
}

static ssize_t tfa98xx_dbgfs_rpc_send(struct file *file,
				     const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	nxpTfaFileDsc_t *msg_file;
	enum Tfa98xx_Error error;
	int err = 0;

	if (tfa98xx->tfa == NULL) {
		pr_debug("[0x%x] dsp is not available\n", tfa98xx->i2c->addr);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	/* msg_file.name is not used */
	msg_file = kmalloc(count + sizeof(nxpTfaFileDsc_t), GFP_KERNEL);
	if ( msg_file == NULL ) {
		pr_debug("[0x%x] can not allocate memory\n", tfa98xx->i2c->addr);
		return  -ENOMEM;
	}
	msg_file->size = count;

	if (copy_from_user(msg_file->data, user_buf, count))
		return -EFAULT;

	mutex_lock(&tfa98xx->dsp_lock);
	if ((msg_file->data[0] == 'M') && (msg_file->data[1] == 'G')) {
		error = tfaContWriteFile(tfa98xx->tfa, msg_file, 0, 0); /* int vstep_idx, int vstep_msg_idx both 0 */
		if (error != Tfa98xx_Error_Ok) {
			pr_debug("[0x%x] tfaContWriteFile error: %d\n", tfa98xx->i2c->addr, error);
			err = -EIO;
		}
	} else {
		error = dsp_msg(tfa98xx->tfa, msg_file->size, msg_file->data);
		if (error != Tfa98xx_Error_Ok) {
			pr_debug("[0x%x] dsp_msg error: %d\n", tfa98xx->i2c->addr, error);
			err = -EIO;
		}
	}
	mutex_unlock(&tfa98xx->dsp_lock);

	kfree(msg_file);

	if (err)
		return err;
	return count;
}
/* -- RPC */

static int tfa98xx_dbgfs_pga_gain_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	unsigned int value;

	value = tfa_get_pga_gain(tfa98xx->tfa);
	if (value < 0)
		return -EINVAL;

	*val = value;
	return 0;
}

static int tfa98xx_dbgfs_pga_gain_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	uint16_t value;
	int err;

	value = val & 0xffff;
	if (value > 7)
		return -EINVAL;

	err = tfa_set_pga_gain(tfa98xx->tfa, value);
	if (err < 0)
		return -EINVAL;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_calib_otc_fops, tfa98xx_dbgfs_otc_get,
						tfa98xx_dbgfs_otc_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_calib_mtpex_fops, tfa98xx_dbgfs_mtpex_get,
						tfa98xx_dbgfs_mtpex_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_calib_temp_fops, tfa98xx_dbgfs_temp_get,
						tfa98xx_dbgfs_temp_set, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_pga_gain_fops, tfa98xx_dbgfs_pga_gain_get,
						tfa98xx_dbgfs_pga_gain_set, "%llu\n");

static const struct file_operations tfa98xx_dbgfs_calib_start_fops = {
	.open = simple_open,
	.write = tfa98xx_dbgfs_start_set,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_r_fops = {
	.open = simple_open,
	.read = tfa98xx_dbgfs_r_read,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_version_fops = {
	.open = simple_open,
	.read = tfa98xx_dbgfs_version_read,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_dsp_state_fops = {
	.open = simple_open,
	.read = tfa98xx_dbgfs_dsp_state_get,
	.write = tfa98xx_dbgfs_dsp_state_set,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_fw_state_fops = {
	.open = simple_open,
	.read = tfa98xx_dbgfs_fw_state_get,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_rpc_fops = {
	.open = simple_open,
	.read = tfa98xx_dbgfs_rpc_read,
	.write = tfa98xx_dbgfs_rpc_send,
	.llseek = default_llseek,
};

static void tfa98xx_debug_init(struct tfa98xx *tfa98xx, struct i2c_client *i2c)
{
	char name[50];

	scnprintf(name, MAX_CONTROL_NAME, "%s-%x", i2c->name, i2c->addr);
	tfa98xx->dbg_dir = debugfs_create_dir(name, NULL);
	debugfs_create_file("OTC", S_IRUGO|S_IWUGO, tfa98xx->dbg_dir,
						i2c, &tfa98xx_dbgfs_calib_otc_fops);
	debugfs_create_file("MTPEX", S_IRUGO|S_IWUGO, tfa98xx->dbg_dir,
						i2c, &tfa98xx_dbgfs_calib_mtpex_fops);
	debugfs_create_file("TEMP", S_IRUGO|S_IWUGO, tfa98xx->dbg_dir,
						i2c, &tfa98xx_dbgfs_calib_temp_fops);
	debugfs_create_file("calibrate", S_IRUGO|S_IWUGO, tfa98xx->dbg_dir,
						i2c, &tfa98xx_dbgfs_calib_start_fops);
	debugfs_create_file("R", S_IRUGO, tfa98xx->dbg_dir,
						i2c, &tfa98xx_dbgfs_r_fops);
	debugfs_create_file("version", S_IRUGO, tfa98xx->dbg_dir,
						i2c, &tfa98xx_dbgfs_version_fops);
	debugfs_create_file("dsp-state", S_IRUGO|S_IWUGO, tfa98xx->dbg_dir,
						i2c, &tfa98xx_dbgfs_dsp_state_fops);
	debugfs_create_file("fw-state", S_IRUGO|S_IWUGO, tfa98xx->dbg_dir,
						i2c, &tfa98xx_dbgfs_fw_state_fops);
	debugfs_create_file("rpc", S_IRUGO|S_IWUGO, tfa98xx->dbg_dir,
						i2c, &tfa98xx_dbgfs_rpc_fops);

	if (tfa98xx->flags & TFA98XX_FLAG_SAAM_AVAILABLE) {
		dev_dbg(tfa98xx->dev, "Adding pga_gain debug interface\n");
		debugfs_create_file("pga_gain", S_IRUGO, tfa98xx->dbg_dir,
						tfa98xx->i2c,
						&tfa98xx_dbgfs_pga_gain_fops);
	}
}

static void tfa98xx_debug_remove(struct tfa98xx *tfa98xx)
{
	if (tfa98xx->dbg_dir)
		debugfs_remove_recursive(tfa98xx->dbg_dir);
}
#endif


/* copies the profile basename (i.e. part until .) into buf */
static void get_profile_basename(char* buf, char* profile)
{
	int cp_len = 0, idx = 0;
	char *pch;

	pch = strchr(profile, '.');
	idx = pch - profile;
	cp_len = (pch != NULL) ? idx : (int) strlen(profile);
	memcpy(buf, profile, cp_len);
	buf[cp_len] = 0;
}

/* return the profile name accociated with id from the profile list */
static int get_profile_from_list(char *buf, int id)
{
	struct tfa98xx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {
		if (bprof->item_id == id) {
			strcpy(buf, bprof->basename);
			return 0;
		}
	}

	return -1;
}

/* search for the profile in the profile list */
static int is_profile_in_list(char *profile, int len)
{
	struct tfa98xx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {

		if ((len == bprof->len) && (0 == strncmp(bprof->basename, profile, len)))
			return 1;
	}

    return 0;
}

/*
 * for the profile with id, look if the requested samplerate is
 * supported, if found return the (container)profile for this
 * samplerate, on error or if not found return -1
 */
static int get_profile_id_for_sr(int id, unsigned int rate)
{
	int idx = 0;
	struct tfa98xx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {
		if (id == bprof->item_id) {
			idx = tfa98xx_get_fssel(rate);
			if (idx < 0) {
				/* samplerate not supported */
				return -1;
			}

			return bprof->sr_rate_sup[idx];
		}
	}

	/* profile not found */
	return -1;
}

/* check if this profile is a calibration profile */
static int is_calibration_profile(char *profile)
{
	if (strstr(profile, ".cal") != NULL)
		return 1;
	return 0;
}

/*
 * adds the (container)profile index of the samplerate found in
 * the (container)profile to a fixed samplerate table in the (mixer)profile
 */
static int add_sr_to_profile(struct tfa98xx *tfa98xx, char *basename, int len, int profile)
{
	struct tfa98xx_baseprofile *bprof;
	int idx = 0;
	unsigned int sr = 0;

	list_for_each_entry(bprof, &profile_list, list) {
		if ((len == bprof->len) && (0 == strncmp(bprof->basename, basename, len))) {
			/* add supported samplerate for this profile */
			sr = tfa98xx_get_profile_sr(tfa98xx->tfa, profile);
			if (!sr) {
				pr_err("unable to identify supported sample rate for %s\n", bprof->basename);
				return -1;
			}

			/* get the index for this samplerate */
			idx = tfa98xx_get_fssel(sr);
			if (idx < 0 || idx >= TFA98XX_NUM_RATES) {
				pr_err("invalid index for samplerate %d\n", idx);
				return -1;
			}

			/* enter the (container)profile for this samplerate at the corresponding index */
			bprof->sr_rate_sup[idx] = profile;

			pr_debug("added profile:samplerate = [%d:%d] for mixer profile: %s\n", profile, sr, bprof->basename);
		}
	}

	return 0;
}

static int tfa98xx_set_profile(struct tfa98xx *tfa98xx,
			       int new_profile)
{
	int change = 0;
	int prof_idx;
	int profile_count = tfa98xx_mixer_profiles;
	int profile = tfa98xx_mixer_profile;

	if (no_start != 0)
		return 0;
	if (new_profile == profile)
		return 0;
	pr_debug("%s new profile %d\n", __func__, new_profile);

	if ((new_profile < 0) || (new_profile >= profile_count)) {
		pr_err("not existing profile (%d)\n", new_profile);
		return -EINVAL;
	}


	/* update mixer profile */
	tfa98xx_mixer_profile = new_profile;
    prof_idx = new_profile;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		int err;
		int ready = 0;

		/* update 'real' profile (container profile) */
		tfa98xx->profile = prof_idx;
		tfa98xx->vstep = tfa98xx->prof_vsteps[prof_idx];

		/* Don't call tfa_dev_start() if there is no clock. */
		mutex_lock(&tfa98xx->dsp_lock);
		tfa98xx_dsp_system_stable(tfa98xx->tfa, &ready);
		if (ready) {
			/* Also re-enables the interrupts */
			err = tfa98xx_tfa_start(tfa98xx, prof_idx, tfa98xx->vstep);
			if (err) {
				pr_info("Write profile error: %d\n", err);
			} else {
				pr_debug("Changed to profile %d (vstep = %d)\n",
				         prof_idx, tfa98xx->vstep);
				change = 1;
			}
		} else {
            pr_info("%s system not ready\n", __func__);
        }
		mutex_unlock(&tfa98xx->dsp_lock);

		/* Flag DSP as invalidated as the profile change may invalidate the
		 * current DSP configuration. That way, further stream start can
		 * trigger a tfa_dev_start.
		 */
		tfa98xx->dsp_init = TFA98XX_DSP_INIT_INVALIDATED;
	}

	if (change) {
		list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
			mutex_lock(&tfa98xx->dsp_lock);
			tfa_dev_set_state(tfa98xx->tfa, TFA_STATE_UNMUTE,0);
			mutex_unlock(&tfa98xx->dsp_lock);
		}
	}

	mutex_unlock(&tfa98xx_mutex);

	return change;
}

/* I2C wrapper functions */
enum Tfa98xx_Error tfa98xx_write_register16(struct tfa_device *tfa,
					unsigned char subaddress,
					unsigned short value)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	struct tfa98xx *tfa98xx;
	int ret;
	int retries = I2C_RETRIES;

	if (tfa == NULL) {
		pr_err("No device available\n");
		return Tfa98xx_Error_Fail;
	}

	tfa98xx = (struct tfa98xx *)tfa->data;
	if (!tfa98xx || !tfa98xx->regmap) {
		pr_err("No tfa98xx regmap available\n");
		return Tfa98xx_Error_Bad_Parameter;
	}
retry:
	ret = regmap_write(tfa98xx->regmap, subaddress, value);
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
		return Tfa98xx_Error_Fail;
	}
	if (tfa98xx_kmsg_regs)
		dev_dbg(&tfa98xx->i2c->dev, "  WR reg=0x%02x, val=0x%04x %s\n",
		        subaddress, value,
		        ret<0? "Error!!" : "");

	if (tfa98xx_ftrace_regs)
		tfa98xx_trace_printk("\tWR     reg=0x%02x, val=0x%04x %s\n",
		                     subaddress, value,
		                     ret<0? "Error!!" : "");
	return error;
}

enum Tfa98xx_Error tfa98xx_read_register16(struct tfa_device *tfa,
					unsigned char subaddress,
					unsigned short *val)
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	struct tfa98xx *tfa98xx;
	unsigned int value;
	int retries = I2C_RETRIES;
	int ret;

	if (tfa == NULL) {
		pr_err("No device available\n");
		return Tfa98xx_Error_Fail;
	}

	tfa98xx = (struct tfa98xx *)tfa->data;
	if (!tfa98xx || !tfa98xx->regmap) {
		pr_err("No tfa98xx regmap available\n");
		return Tfa98xx_Error_Bad_Parameter;
	}
retry:
	ret = regmap_read(tfa98xx->regmap, subaddress, &value);
	if (ret < 0) {
		pr_warn("i2c error at subaddress 0x%x, retries left: %d\n", subaddress, retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
		return Tfa98xx_Error_Fail;
	}
	*val = value & 0xffff;

	if (tfa98xx_kmsg_regs)
		dev_dbg(&tfa98xx->i2c->dev, "RD   reg=0x%02x, val=0x%04x %s\n",
		        subaddress, *val,
		        ret<0? "Error!!" : "");
	if (tfa98xx_ftrace_regs)
		tfa98xx_trace_printk("\tRD     reg=0x%02x, val=0x%04x %s\n",
		                    subaddress, *val,
		                    ret<0? "Error!!" : "");

	return error;
}


/*
 * init external dsp
 */
enum Tfa98xx_Error
tfa98xx_init_dsp(struct tfa_device *tfa)
{
	return Tfa98xx_Error_Not_Supported;
}

int tfa98xx_get_dsp_status(struct tfa_device *tfa)
{
	return 0;
}

/*
 * write external dsp message
 */
enum Tfa98xx_Error
tfa98xx_write_dsp(struct tfa_device *tfa,  int num_bytes, const char *command_buffer)
{
	return Tfa98xx_Error_Not_Supported;
}

/*
 * read external dsp message
 */
enum Tfa98xx_Error
tfa98xx_read_dsp(struct tfa_device *tfa,  int num_bytes, unsigned char *result_buffer)
{
	return Tfa98xx_Error_Not_Supported;
}
/*
 * write/read external dsp message
 */
enum Tfa98xx_Error
tfa98xx_writeread_dsp(struct tfa_device *tfa, int command_length, void *command_buffer,
											  int result_length, void *result_buffer)
{
	return Tfa98xx_Error_Not_Supported;
}

enum Tfa98xx_Error tfa98xx_read_data(struct tfa_device *tfa,
				unsigned char reg,
				int len, unsigned char value[])
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	struct tfa98xx *tfa98xx;
	struct i2c_client *tfa98xx_client;
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.len = 1,
			.buf = &reg,
		}, {
			.flags = I2C_M_RD,
			.len = len,
			.buf = value,
		},
	};

	if (tfa == NULL) {
		pr_err("No device available\n");
		return Tfa98xx_Error_Fail;
	}

	tfa98xx = (struct tfa98xx *)tfa->data;
	if (tfa98xx->i2c) {
		tfa98xx_client = tfa98xx->i2c;
		msgs[0].addr = tfa98xx_client->addr;
		msgs[1].addr = tfa98xx_client->addr;

		do {
			err = i2c_transfer(tfa98xx_client->adapter, msgs,
							ARRAY_SIZE(msgs));
			if (err != ARRAY_SIZE(msgs))
				msleep_interruptible(I2C_RETRY_DELAY);
		} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

		if (err != ARRAY_SIZE(msgs)) {
			dev_err(&tfa98xx_client->dev, "read transfer error %d\n",
									err);
			error = Tfa98xx_Error_Fail;
		}

		if (tfa98xx_kmsg_regs)
			dev_dbg(&tfa98xx_client->dev, "RD-DAT reg=0x%02x, len=%d\n",
								reg, len);
		if (tfa98xx_ftrace_regs)
			tfa98xx_trace_printk("\t\tRD-DAT reg=0x%02x, len=%d\n",
					reg, len);
	} else {
		pr_err("No device available\n");
		error = Tfa98xx_Error_Fail;
	}
	return error;
}

enum Tfa98xx_Error tfa98xx_write_raw(struct tfa_device *tfa,
				int len,
				const unsigned char data[])
{
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;
	struct tfa98xx *tfa98xx;
	int ret;
	int retries = I2C_RETRIES;


	if (tfa == NULL) {
		pr_err("No device available\n");
		return Tfa98xx_Error_Fail;
	}

	tfa98xx = (struct tfa98xx *)tfa->data;

retry:
	ret = i2c_master_send(tfa98xx->i2c, data, len);
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
	}

	if (ret == len) {
		if (tfa98xx_kmsg_regs)
			dev_dbg(&tfa98xx->i2c->dev, "  WR-RAW len=%d\n", len);
		if (tfa98xx_ftrace_regs)
			tfa98xx_trace_printk("\t\tWR-RAW len=%d\n", len);
		return Tfa98xx_Error_Ok;
	}
	pr_err("  WR-RAW (len=%d) Error I2C send size mismatch %d\n", len, ret);
	error = Tfa98xx_Error_Fail;

	return error;
}

/* Interrupts management */

static void tfa98xx_interrupt_enable_tfa2(struct tfa98xx *tfa98xx, bool enable)
{
	/* Only for 0x72 we need to enable NOCLK interrupts */
	if (tfa98xx->flags & TFA98XX_FLAG_REMOVE_PLOP_NOISE)
		tfa_irq_ena(tfa98xx->tfa, tfa9912_irq_stnoclk, enable);

	if (tfa_irq_ena(tfa98xx->tfa, tfa9874_irq_stotds, enable))
		pr_err("%s enable irq tfa9874_irq_stotds err", __func__);
	if (tfa_irq_ena(tfa98xx->tfa, tfa9874_irq_stocpr, enable))
		pr_err("%s enable irq tfa9874_irq_stocpr err", __func__);
	if (tfa_irq_ena(tfa98xx->tfa, tfa9874_irq_stuvds, enable))
		pr_err("%s enable irq tfa9874_irq_stuvds err", __func__);

	if (tfa98xx->flags & TFA98XX_FLAG_LP_MODES) {
		tfa_irq_ena(tfa98xx->tfa, 36, enable); /* FIXME: IELP0 does not excist for 9912 */
		tfa_irq_ena(tfa98xx->tfa, tfa9912_irq_stclpr, enable);
	}
}

/* global enable / disable interrupts */
static void tfa98xx_interrupt_enable(struct tfa98xx *tfa98xx, bool enable)
{
	if (tfa98xx->flags & TFA98XX_FLAG_SKIP_INTERRUPTS)
		return;

	if (tfa98xx->tfa->tfa_family == 2)
		tfa98xx_interrupt_enable_tfa2(tfa98xx, enable);
}

/* Firmware management */
static void tfa98xx_container_loaded(const struct firmware *cont, void *context)
{
	nxpTfaContainer_t *container;
	struct tfa98xx *tfa98xx = context;
	enum tfa_error tfa_err;
	int container_size;
	int ret;

	tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;

	if (!cont) {
		pr_err("Failed to read %s\n", fw_name);
		return;
	}

	pr_debug("loaded %s - size: %zu\n", fw_name, cont->size);

	mutex_lock(&tfa98xx_mutex);

	if (tfa98xx_container == NULL) {
		container = kzalloc(cont->size, GFP_KERNEL);
		if (container == NULL) {
			mutex_unlock(&tfa98xx_mutex);
			release_firmware(cont);
			pr_err("Error allocating memory\n");
			return;
		}

		container_size = cont->size;
		memcpy(container, cont->data, container_size);
		release_firmware(cont);

		pr_info("%.2s%.2s\n", container->version, container->subversion);
		pr_info("%.8s\n", container->customer);
		pr_info("%.8s\n", container->application);
		pr_info("%.8s\n", container->type);
		pr_info("%d ndev\n", container->ndev);
		pr_info("%d nprof\n", container->nprof);

        tfa98xx_mixer_profiles = container->nprof;

		tfa_err = tfa_load_cnt(container, container_size);
		if (tfa_err != tfa_error_ok) {
			mutex_unlock(&tfa98xx_mutex);
			kfree(container);
			dev_err(tfa98xx->dev, "Cannot load container file, aborting\n");
			return;
		}

		tfa98xx_container = container;
	} else {
		pr_debug("container file already loaded...\n");
		container = tfa98xx_container;
		release_firmware(cont);
	}
	mutex_unlock(&tfa98xx_mutex);

	tfa98xx->tfa->cnt = container;

	/*
		i2c transaction limited to 64k
		(Documentation/i2c/writing-clients)
	*/
	tfa98xx->tfa->buffer_size = 65536;

	/* DSP messages via i2c */
	tfa98xx->tfa->has_msg = 0;

	if (tfa_dev_probe(tfa98xx->i2c->addr, tfa98xx->tfa) != 0) {
		dev_err(tfa98xx->dev, "Failed to probe TFA98xx @ 0x%.2x\n", tfa98xx->i2c->addr);
		return;
	}

	tfa98xx->tfa->dev_idx = tfa_cont_get_idx(tfa98xx->tfa);
	if (tfa98xx->tfa->dev_idx < 0) {
		dev_err(tfa98xx->dev, "Failed to find TFA98xx @ 0x%.2x in container file\n", tfa98xx->i2c->addr);
		return;
	}

	/* Enable debug traces */
	tfa98xx->tfa->verbose = trace_level & 1;

	/* prefix is the application name from the cnt */
	tfa_cnt_get_app_name(tfa98xx->tfa, tfa98xx->fw.name);

	/* set default profile/vstep */
	tfa98xx->profile = 0;
	tfa98xx->vstep = 0;

	/* Override default profile if requested */
	if (strcmp(dflt_prof_name, "")) {
		unsigned int i;
		int nprof = tfa_cnt_get_dev_nprof(tfa98xx->tfa);
		for (i = 0; i < nprof; i++) {
			if (strcmp(tfa_cont_profile_name(tfa98xx, i),
							dflt_prof_name) == 0) {
				tfa98xx->profile = i;
				dev_info(tfa98xx->dev,
					"changing default profile to %s (%d)\n",
					dflt_prof_name, tfa98xx->profile);
				break;
			}
		}
		if (i >= nprof)
			dev_info(tfa98xx->dev,
				"Default profile override failed (%s profile not found)\n",
				dflt_prof_name);
	}

	tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_OK;
	pr_debug("Firmware init complete\n");

	if (no_start != 0)
		return;

	if (tfa_is_cold(tfa98xx->tfa) == 0) {
		pr_debug("Warning: device 0x%.2x is still warm\n", tfa98xx->i2c->addr);
		tfa_reset(tfa98xx->tfa);
	}

	/* Preload settings using internal clock on TFA2 */
	if (tfa98xx->tfa->tfa_family == 2) {
		mutex_lock(&tfa98xx->dsp_lock);
		ret = tfa98xx_tfa_start(tfa98xx, tfa98xx->profile, tfa98xx->vstep);
		if (ret == Tfa98xx_Error_Not_Supported)
			tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;
		mutex_unlock(&tfa98xx->dsp_lock);
	}

	tfa98xx_interrupt_enable(tfa98xx, true);
}

static int tfa98xx_load_container(struct tfa98xx *tfa98xx)
{
	tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_PENDING;

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
	                               fw_name, tfa98xx->dev, GFP_KERNEL,
	                               tfa98xx, tfa98xx_container_loaded);
}

static void tfa98xx_monitor(struct work_struct *work)
{
	struct tfa98xx *tfa98xx;
	enum Tfa98xx_Error error = Tfa98xx_Error_Ok;

	tfa98xx = container_of(work, struct tfa98xx, monitor_work.work);

	/* Check for tap-detection - bypass monitor if it is active */
	mutex_lock(&tfa98xx->dsp_lock);
	error = tfa_status(tfa98xx->tfa);
	mutex_unlock(&tfa98xx->dsp_lock);
	if (error == Tfa98xx_Error_DSP_not_running) {
		if (tfa98xx->dsp_init == TFA98XX_DSP_INIT_DONE) {
			tfa98xx->dsp_init = TFA98XX_DSP_INIT_RECOVER;
			queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->init_work, 0);
		}
	}

	/* reschedule */
	queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->monitor_work, 5*HZ);
}

static void tfa98xx_dsp_init(struct tfa98xx *tfa98xx)
{
	int ret;
	bool failed = false;
	bool reschedule = false;
	bool sync= false;

	if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK) {
		pr_err("Skipping tfa_dev_start (no FW: %d)\n", tfa98xx->dsp_fw_state);
		return;
	}

	if(tfa98xx->dsp_init == TFA98XX_DSP_INIT_DONE) {
		pr_debug("Stream already started, skipping DSP power-on\n");
		return;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	tfa98xx->dsp_init = TFA98XX_DSP_INIT_PENDING;
	if (tfa98xx->init_count < TF98XX_MAX_DSP_START_TRY_COUNT) {
		/* directly try to start DSP */
		ret = tfa98xx_tfa_start(tfa98xx, tfa98xx->profile, tfa98xx->vstep);
		if (ret == Tfa98xx_Error_Not_Supported) {
			tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;
			dev_err(&tfa98xx->i2c->dev, "Failed starting device\n");
			failed = true;
		} else if (ret != Tfa98xx_Error_Ok) {
			/* It may fail as we may not have a valid clock at that
			 * time, so re-schedule and re-try later.
			 */
			dev_err(&tfa98xx->i2c->dev,
					"tfa_dev_start failed! (err %d) - %d\n",
					ret, tfa98xx->init_count);
			reschedule = true;
		} else {
			sync = true;

			/* Subsystem ready, tfa init complete */
			tfa98xx->dsp_init = TFA98XX_DSP_INIT_DONE;
			dev_dbg(&tfa98xx->i2c->dev,
						"tfa_dev_start success (%d)\n",
						tfa98xx->init_count);
			/* cancel other pending init works */
			cancel_delayed_work(&tfa98xx->init_work);
			tfa98xx->init_count = 0;
		}
	} else {
		/* exceeded max number ot start tentatives, cancel start */
		dev_err(&tfa98xx->i2c->dev,
			"Failed starting device (%d)\n",
			tfa98xx->init_count);
			failed = true;
	}
	if (reschedule) {
		/* reschedule this init work for later */
		queue_delayed_work(tfa98xx->tfa98xx_wq,
						&tfa98xx->init_work,
						msecs_to_jiffies(5));
		tfa98xx->init_count++;
	}
	if (failed) {
		tfa98xx->dsp_init = TFA98XX_DSP_INIT_FAIL;
		/* cancel other pending init works */
		cancel_delayed_work(&tfa98xx->init_work);
		tfa98xx->init_count = 0;
	}
	mutex_unlock(&tfa98xx->dsp_lock);

	if (sync) {
		/* check if all devices have started */
		bool do_sync;
		mutex_lock(&tfa98xx_mutex);

		if (tfa98xx_sync_count < tfa98xx_device_count)
			tfa98xx_sync_count++;

		do_sync = (tfa98xx_sync_count >= tfa98xx_device_count);
		mutex_unlock(&tfa98xx_mutex);

		/* when all devices have started then unmute */
		if (do_sync) {
			tfa98xx_sync_count = 0;
			list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
				mutex_lock(&tfa98xx->dsp_lock);
				tfa_dev_set_state(tfa98xx->tfa, TFA_STATE_UNMUTE,0);

				/*
				 * start monitor thread to check IC status bit
				 * periodically, and re-init IC to recover if
				 * needed.
				 */
				if (tfa98xx->tfa->tfa_family == 1)
					queue_delayed_work(tfa98xx->tfa98xx_wq,
					                &tfa98xx->monitor_work,
					                1*HZ);
				mutex_unlock(&tfa98xx->dsp_lock);
			}

		}
	}


	return;
}


static void tfa98xx_dsp_init_work(struct work_struct *work)
{
	struct tfa98xx *tfa98xx = container_of(work, struct tfa98xx, init_work.work);

	tfa98xx_dsp_init(tfa98xx);
}

#define TFA98XX_INTERRUPT_OUT_REG1_ISTOCPR 0x20
#define TFA98XX_INTERRUPT_IN_MASK 0xFFFF
static void tfa98xx_interrupt(struct work_struct *work)
{
	struct tfa98xx *tfa98xx = container_of(work, struct tfa98xx, interrupt_work.work);
	int reg  = 0;

	if (tfa98xx->flags & TFA98XX_FLAG_REMOVE_PLOP_NOISE) {
		int start_triggered;

		mutex_lock(&tfa98xx->dsp_lock);
		start_triggered = tfa_plop_noise_interrupt(tfa98xx->tfa, tfa98xx->profile, tfa98xx->vstep);
		/* Only enable when the return value is 1, otherwise the interrupt is triggered twice */
		if(start_triggered)
			tfa98xx_interrupt_enable(tfa98xx, true);
		mutex_unlock(&tfa98xx->dsp_lock);
	} /* TFA98XX_FLAG_REMOVE_PLOP_NOISE */

	if (tfa98xx->flags & TFA98XX_FLAG_LP_MODES) {
		tfa_lp_mode_interrupt(tfa98xx->tfa);
	} /* TFA98XX_FLAG_LP_MODES */

	if (regmap_read(tfa98xx->regmap, TFA98XX_STATUS_FLAGS0, &reg))
		pr_err("%s i2c err in read TFA98XX_STATUS_FLAGS0", __func__);
	else
		pr_info(" %s TFA98XX_STATUS_FLAGS0 0x%04x\n", __func__, reg);

	if (regmap_read(tfa98xx->regmap, TFA98XX_STATUS_FLAGS1, &reg))
		pr_err("%s i2c err in read TFA98XX_STATUS_FLAGS1", __func__);
	else
		pr_info(" %s TFA98XX_STATUS_FLAGS1 0x%04x\n", __func__, reg);

	if (regmap_read(tfa98xx->regmap, TFA98XX_INTERRUPT_OUT_REG1, &reg)) {
		pr_err("%s i2c err in read TFA98XX_INTERRUPT_OUT_REG1",
			__func__);
	} else if (reg & TFA98XX_INTERRUPT_OUT_REG1_ISTUVDS) {
		audio_dsm_report_info(AUDIO_SMARTPA, DSM_SMARTPA_INT_ERR,
				      "tfa98xx UVP happened 0x%x\n", reg);
		pr_info(" %s UVP happened", __func__);
	} else if (reg & TFA98XX_INTERRUPT_OUT_REG1_ISTOCPR) {
		audio_dsm_report_info(AUDIO_SMARTPA, DSM_SMARTPA_INT_ERR,
				      "tfa98xx OCP happened 0x%x\n", reg);
		pr_info(" %s OCP happened", __func__);
	} else if (reg & TFA98XX_INTERRUPT_OUT_REG1_ISTOTDS) {
		audio_dsm_report_info(AUDIO_SMARTPA, DSM_SMARTPA_INT_ERR,
				      "tfa98xx OTP happened 0x%x\n", reg);
		pr_info(" %s OTP happened", __func__);
	} else {
		pr_info(" %s 0x40 0x%04x", __func__, reg);
	}

	if (regmap_write(tfa98xx->regmap, TFA98XX_INTERRUPT_IN_REG1,
			 TFA98XX_INTERRUPT_IN_MASK))
		pr_err("%s i2c err in write TFA98XX_INTERRUPT_IN_REG1",
			__func__);

	/* unmask interrupts masked in IRQ handler */
	 tfa_irq_unmask(tfa98xx->tfa);
}

static int tfa98xx_startup(struct tfa98xx *tfa98xx)
{	
	unsigned int sr;
	int len, prof, nprof, idx = 0;
	char *basename;
	u64 formats;
	int err;

    pr_err("%s %d", __func__, __LINE__);

	if (pcm_no_constraint != 0)
		return 0;

	if (no_start != 0)
		return 0;

	if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK) {
		dev_info(tfa98xx->dev, "Container file not loaded\n");
		return -EINVAL;
	}

	basename = kzalloc(MAX_CONTROL_NAME, GFP_KERNEL);
	if (!basename)
		return -ENOMEM;

	/* copy profile name into basename until the . */
	get_profile_basename(basename, tfa_cont_profile_name(tfa98xx, tfa98xx->profile));
	len = strlen(basename);

	/* loop over all profiles and get the supported samples rate(s) from
	 * the profiles with the same basename
	 */
	nprof = tfa_cnt_get_dev_nprof(tfa98xx->tfa);

	for (prof = 0; prof < nprof; prof++) {
		if (0 == strncmp(basename, tfa_cont_profile_name(tfa98xx, prof), len)) {
			/* Check which sample rate is supported with current profile,
			 * and enforce this.
			 */
			sr = tfa98xx_get_profile_sr(tfa98xx->tfa, prof);
			if (!sr)
				dev_info(tfa98xx->dev, "Unable to identify supported sample rate\n");
		}
	}

	kfree(basename);

	return 0;
}

static int tfa98xx_get_fssel(unsigned int rate)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(rate_to_fssel); i++) {
		if (rate_to_fssel[i].rate == rate) {
			return rate_to_fssel[i].fssel;
		}
	}
	return -EINVAL;
}

static int tfa98xx_set_default_calib(struct tfa98xx *tfa98xx, int rdc)
{
	enum tfa_error err = tfa_error_ok;

	if (rdc > 9216 || rdc < 4096) // default value should within 4ohm~9ohm
		return tfa_error_bad_param;

	mutex_lock(&tfa98xx->dsp_lock);
	err |= tfa_dev_mtp_set(tfa98xx->tfa, TFA_MTP_OTC, 1); // enable mtp_otc
	err |= tfa98xx_write_re25(tfa98xx->tfa, rdc);
	mutex_unlock(&tfa98xx->dsp_lock);

	return err;
}

static int tfa98xx_mute(struct tfa98xx *tfa98xx, int mute)
{
	pr_info("%s: state: %d\n", __func__, mute);

	if (no_start) {
		pr_debug("no_start parameter set no tfa_dev_start or tfa_dev_stop, returning\n");
		return 0;
	}

	if (mute) {
		mutex_lock(&tfa98xx_mutex);
		tfa98xx_sync_count = 0;
		mutex_unlock(&tfa98xx_mutex);

		cancel_delayed_work_sync(&tfa98xx->monitor_work);

		cancel_delayed_work_sync(&tfa98xx->init_work);
		if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK)
			return 0;
		mutex_lock(&tfa98xx->dsp_lock);
		tfa_dev_stop(tfa98xx->tfa);
		tfa98xx->dsp_init = TFA98XX_DSP_INIT_STOPPED;
		mutex_unlock(&tfa98xx->dsp_lock);
	} else {
		/* Start DSP */
		 tfa98xx_dsp_init(tfa98xx);	
	}

	return 0;
}

static int tfa98xx_probe(struct tfa98xx *tfa98xx)
{
	int ret;

	pr_debug("\n");

	/* setup work queue, will be used to initial DSP on first boot up */
	tfa98xx->tfa98xx_wq = create_singlethread_workqueue("tfa98xx");
	if (!tfa98xx->tfa98xx_wq)
		return -ENOMEM;

	INIT_DELAYED_WORK(&tfa98xx->init_work, tfa98xx_dsp_init_work);
	INIT_DELAYED_WORK(&tfa98xx->monitor_work, tfa98xx_monitor);
	INIT_DELAYED_WORK(&tfa98xx->interrupt_work, tfa98xx_interrupt);

	ret = tfa98xx_load_container(tfa98xx);
	pr_debug("Container loading requested: %d\n", ret);

	dev_info(tfa98xx->dev, "tfa98xx registered (%s)",
							tfa98xx->fw.name);

	return ret;
}

static int tfa98xx_remove(struct tfa98xx *tfa98xx)
{
	cancel_delayed_work_sync(&tfa98xx->interrupt_work);
	cancel_delayed_work_sync(&tfa98xx->monitor_work);
	cancel_delayed_work_sync(&tfa98xx->init_work);

	if (tfa98xx->tfa98xx_wq)
		destroy_workqueue(tfa98xx->tfa98xx_wq);

	return 0;
}

#define LOW_TEMP_STATE_IN_TEMP -7
#define LOW_TEMP_STATE_OUT_TEMP -5
#define LOW_TEMP_STATE_STATE_VOLTAGE 3800000
#define DIV_TRANSFORM_TO_DEGREE 10

enum low_temreture_enum {
	LOW_TEMP_STATE_DEFAULT = 0,
	LOW_TEMP_STATE_IN_STAGE,
	LOW_TEMP_STATE_OUT,
};

enum tfa9894_config {
	TFA9894_CONFIG_MUSIC = 0,
	TFA9894_CONFIG_RING,
	TFA9894_CONFIG_RING_HS_SPK,
	TFA9894_CONFIG_INCALL,
	TFA9894_CONFIG_VOIP,
	TFA9894_CONFIG_LOWPOWER,
	TFA9894_CONFIG_MMI,
	TFA9894_CONFIG_BYPASS,
	TFA9894_CONFIG_LOWTEMP,
	TFA9894_CONFIG_CALIB,
};

static int low_temperature_state_get(void)
{
	struct power_supply *batt_psy = NULL;
	int state_ret;
	int temp;
	int vol;

	batt_psy = power_supply_get_by_name("battery");
	if (!batt_psy) {
		pr_err("batt_psy get failed\n");
		return LOW_TEMP_STATE_OUT;
	}

	temp = get_loginfo_int(batt_psy, POWER_SUPPLY_PROP_TEMP);
	temp = temp / DIV_TRANSFORM_TO_DEGREE;
	vol = get_loginfo_int(batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW);
	power_supply_put(batt_psy);

	pr_info("battery temp %d vol %d\n", temp, vol);
	if ((temp <= LOW_TEMP_STATE_IN_TEMP) &&
	    (vol <= LOW_TEMP_STATE_STATE_VOLTAGE))
		state_ret = LOW_TEMP_STATE_IN_STAGE;
	else
		state_ret = LOW_TEMP_STATE_OUT;

	pr_info("low temperature state in  %d\n", state_ret);
	return state_ret;
}

static long tfa98xx_ioctl(struct file *f, 
	unsigned int cmd, void __user *arg)
{
	struct miscdevice *dev = f->private_data;
	struct tfa98xx *tfa98xx;
	int new_profile = 0, ret = 0;
	int low_temp_state;
	int value = 0;

	tfa98xx = container_of(dev, struct tfa98xx, misc_dev);

	pr_info("%s cmd 0x%x", __func__, cmd);

	switch(cmd) {
		case SMARTPA_SPK_SWITCH_FIRMWARE:
			if(tfa98xx_container) {
				tfa98xx_remove(tfa98xx);
				kfree(tfa98xx_container);
				tfa98xx_container = NULL;
			}
			if(copy_from_user(&new_profile, arg, sizeof(new_profile))) {
				dev_err(tfa98xx->dev, "copy from user failed\n");
				ret = -EFAULT;
				goto exit;
			}
			snprintf(fw_name, FW_NAME_SIZE-1, "%s_%s.cnt", FW_NAME_BASE, boxid_table[new_profile]);
			pr_info("%s fw_name %s boxid %d", __func__, fw_name, new_profile);
			ret = tfa98xx_probe(tfa98xx);
			if(ret < 0) {
				dev_err(tfa98xx->dev, "Failed to initialize TFA98XX Ampilier: %d", ret);
				goto exit;
			}
			break;
		case TFA98XX_SPK_SET_DEFAULT_CALIB:
			if (copy_from_user(&value, arg, sizeof(value))) {
				dev_err(tfa98xx->dev, "copy from user fail");
				ret = -EFAULT;
				goto exit;
			}

			if (tfa98xx->dsp_fw_state == TFA98XX_DSP_FW_OK) {
				ret = tfa98xx_set_default_calib(tfa98xx, value);
				if (ret != 0)
					dev_err(tfa98xx->dev,
						"Fail to set default calibration value %d", ret);
			}
			break;
		case TFA98XX_SPK_GET_CALIB_STATE:
			if (tfa98xx->dsp_fw_state == TFA98XX_DSP_FW_OK) {
				mutex_lock(&tfa98xx->dsp_lock);
				if (tfa_get_bf(tfa98xx->tfa, TFA2_BF_MTPOTC) &&
					tfa_get_bf(tfa98xx->tfa, TFA2_BF_MTPEX))
					value = 1; // calibration has been set
				else
					value = 0; // calibration never been set
				mutex_unlock(&tfa98xx->dsp_lock);
			} else {
				pr_info("%s tfa98xx dsp firmware is not ready",
					__func__);
				value = 1; // tfa98xx device is not ready
			}

			ret = copy_to_user(arg, &value, sizeof(value));
			break;
		case TFA98XX_SPK_SWITCH_CONFIGURATION:
			if(copy_from_user(&new_profile, arg, sizeof(new_profile))) {
				dev_err(tfa98xx->dev, "copy from user failed\n");
				ret = -EFAULT;
				goto exit;
			}
			low_temp_state = low_temperature_state_get();
			if ((low_temp_state == LOW_TEMP_STATE_IN_STAGE) &&
			    ((new_profile == TFA9894_CONFIG_RING) ||
			    (new_profile == TFA9894_CONFIG_RING_HS_SPK)))
				tfa98xx_set_profile(tfa98xx,
					TFA9894_CONFIG_LOWTEMP);
			else
				tfa98xx_set_profile(tfa98xx, new_profile);
			tfa98xx_mute(tfa98xx, 0);
			break;
		case TFA98XX_SPK_POWER_OFF:
			tfa98xx_mute(tfa98xx, 1);
			tfa98xx_interrupt_enable(tfa98xx, false);
			break;
		case I2C_SLAVE:
		case I2C_SLAVE_FORCE:
			pr_info("%s 0x%08x", __func__, arg);
			tfa98xx->i2c->addr = arg;
			break;
		default:
			dev_info(tfa98xx->dev, "COMMAND = 0x%08x\n", cmd);
			return 0;
		}
exit:
	return ret;
}

static long tfa98xx_unlocked_ioctl(struct file *f, 
	unsigned int cmd, unsigned long arg)
{
	return tfa98xx_ioctl(f, cmd, (void __user *)arg);
}

#ifdef CONFIG_COMPAT
static long tfa98xx_compat_ioctl(struct file *f, 
	unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = f->private_data;
	struct tfa98xx *tfa98xx;
	unsigned int cmd64;

	tfa98xx = container_of(dev, struct tfa98xx, misc_dev);
	switch (cmd) {
		case SMARTPA_SPK_SWITCH_FIRMWARE_COMPAT:
			cmd64 = SMARTPA_SPK_SWITCH_FIRMWARE;
			break;
		case TFA98XX_SPK_SWITCH_CONFIGURATION_COMPAT:
			cmd64 = TFA98XX_SPK_SWITCH_CONFIGURATION;
			break;
		case TFA98XX_SPK_POWER_OFF_COMPAT:
			cmd64 = TFA98XX_SPK_POWER_OFF;
			break;
		case TFA98XX_SPK_SET_DEF_CALIB_COMPAT:
			cmd64 = TFA98XX_SPK_SET_DEFAULT_CALIB;
			break;
		case TFA98XX_SPK_GET_CALIB_STATE_COMPAT:
			cmd64 = TFA98XX_SPK_GET_CALIB_STATE;
			break;
		default:
			break;
		}
	return tfa98xx_ioctl(f, cmd64, compat_ptr(arg));
}
#endif

static ssize_t tfa98xx_i2c_write(struct file *file,
		const char __user *buf, size_t nbytes, loff_t *ppos)
{
	UNUSED_PARAM(ppos);
	void *kern_buf = NULL;
	int ret = 0;
	struct miscdevice *dev = file->private_data;
	struct tfa98xx *tfa98xx = NULL;

	tfa98xx = container_of(dev, struct tfa98xx, misc_dev);
	if (tfa98xx == NULL ||file == NULL ||
		buf == NULL || nbytes > I2C_TRANSFER_MAX_SIZE)
		pr_err("%s tfa98xx %p file %p buf %p nbytes %ld",
				__func__, tfa98xx, file, buf, nbytes);
	kern_buf = kzalloc(nbytes, GFP_KERNEL);
	if (!kern_buf) {
		pr_err("Failed to allocate buffer\n");
		ret = -ENOMEM;
		goto EXIT;
	}

	if (copy_from_user(kern_buf, (void  __user *)buf, nbytes)) {
		ret = -EFAULT;
		goto EXIT;
	}

	tfa98xx->i2c->flags  = TFA98XX_I2C_W;
	ret = i2c_master_send(tfa98xx->i2c, kern_buf, nbytes);
	if (ret < 0) {
		pr_err("%s i2c transfer error", __func__);
		goto EXIT;
	}

EXIT:
	kfree(kern_buf);
	return ret;
}

static ssize_t tfa98xx_i2c_read(struct file *file,
		char __user *buf, size_t nbytes, loff_t *ppos)
{
	UNUSED_PARAM(ppos);
	void *kern_buf = NULL;
	int ret = 0;
	struct miscdevice *dev = file->private_data;
	struct tfa98xx *tfa98xx = NULL;

	tfa98xx = container_of(dev, struct tfa98xx, misc_dev);
	if (tfa98xx == NULL ||file == NULL ||
		buf == NULL || nbytes > I2C_TRANSFER_MAX_SIZE)
		pr_err("%s tfa98xx %p file %p buf %p  nbytes %ld",
				__func__, tfa98xx, file, buf, nbytes);
	kern_buf = kzalloc(nbytes, GFP_KERNEL);
	if (!kern_buf) {
		pr_err("Failed to allocate buffer\n");
		ret = -ENOMEM;
		goto EXIT;
	}

	tfa98xx->i2c->flags  = TFA98XX_I2C_R;
	ret = i2c_master_recv(tfa98xx->i2c, kern_buf, nbytes);
	if (ret < 0) {
		pr_err("%s i2c transfer error", __func__);
		goto EXIT;
	}

	ret = copy_to_user((void  __user *)buf, kern_buf,  nbytes);
	if (ret) {
		pr_err("%s copy to user fail %d", __func__, ret);
		goto EXIT;
	}

	ret =  nbytes;
EXIT:
	kfree(kern_buf);
	return ret;
}

static const struct file_operations tfa98xx_fops = {
	.owner 			= THIS_MODULE,
	.read           = tfa98xx_i2c_read,
	.write          = tfa98xx_i2c_write,
	.unlocked_ioctl = tfa98xx_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tfa98xx_compat_ioctl,
#endif
};

static bool tfa98xx_writeable_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static bool tfa98xx_readable_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static bool tfa98xx_volatile_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static const struct regmap_config tfa98xx_regmap = {
	.reg_bits = 8,
	.val_bits = 16,

	.max_register = TFA98XX_MAX_REGISTER,
	.writeable_reg = tfa98xx_writeable_register,
	.readable_reg = tfa98xx_readable_register,
	.volatile_reg = tfa98xx_volatile_register,
	.cache_type = REGCACHE_NONE,
};

static void tfa98xx_irq_tfa2(struct tfa98xx *tfa98xx)
{
	/*
	 * mask interrupts
	 * will be unmasked after handling interrupts in workqueue
	 */
	tfa_irq_mask(tfa98xx->tfa);
	queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->interrupt_work, 0);
}


static irqreturn_t tfa98xx_irq(int irq, void *data)
{
	struct tfa98xx *tfa98xx = data;

	if (tfa98xx->tfa->tfa_family == 2)
		tfa98xx_irq_tfa2(tfa98xx);

	return IRQ_HANDLED;
}

static int tfa98xx_ext_reset(struct tfa98xx *tfa98xx)
{
	if (tfa98xx && gpio_is_valid(tfa98xx->reset_gpio)) {
		gpio_set_value_cansleep(tfa98xx->reset_gpio, 1);
		mdelay(1);
		gpio_set_value_cansleep(tfa98xx->reset_gpio, 0);
		mdelay(1);
	}
	return 0;
}

static int tfa98xx_parse_dt(struct device *dev, struct tfa98xx *tfa98xx,
		struct device_node *np) {
	tfa98xx->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (tfa98xx->reset_gpio < 0)
		dev_dbg(dev, "No reset GPIO provided, will not HW reset device\n");

	tfa98xx->irq_gpio =  of_get_named_gpio(np, "irq-gpio", 0);
	if (tfa98xx->irq_gpio < 0)
		dev_dbg(dev, "No IRQ GPIO provided.\n");

	return 0;
}

static ssize_t tfa98xx_reg_write(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);

	if (count != 1) {
		pr_debug("invalid register address");
		return -EINVAL;
	}

	tfa98xx->reg = buf[0];

	return 1;
}

static ssize_t tfa98xx_rw_write(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	u8 *data;
	int ret;
	int retries = I2C_RETRIES;

	data = kmalloc(count+1, GFP_KERNEL);
	if (data == NULL) {
		pr_debug("can not allocate memory\n");
		return  -ENOMEM;
	}

	data[0] = tfa98xx->reg;
	memcpy(&data[1], buf, count);

retry:
	ret = i2c_master_send(tfa98xx->i2c, data, count+1);
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
	}

	kfree(data);

	/* the number of data bytes written without the register address */
	return ((ret > 1) ? count : -EIO);
}

static ssize_t tfa98xx_rw_read(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	struct i2c_msg msgs[] = {
		{
			.addr = tfa98xx->i2c->addr,
			.flags = 0,
			.len = 1,
			.buf = &tfa98xx->reg,
		},
		{
			.addr = tfa98xx->i2c->addr,
			.flags = I2C_M_RD,
			.len = count,
			.buf = buf,
		},
	};
	int ret;
	int retries = I2C_RETRIES;
retry:
	ret = i2c_transfer(tfa98xx->i2c->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
		return ret;
	}
	/* ret contains the number of i2c transaction */
	/* return the number of bytes read */
	return ((ret > 1) ? count : -EIO);
}

static struct bin_attribute dev_attr_rw = {
	.attr = {
		.name = "rw",
		.mode = S_IRUSR | S_IWUSR | S_IWOTH | S_IROTH,
	},
	.size = 0,
	.read = tfa98xx_rw_read,
	.write = tfa98xx_rw_write,
};

static struct bin_attribute dev_attr_reg = {
	.attr = {
		.name = "reg",
		.mode = S_IWUSR|S_IWOTH,
	},
	.size = 0,
	.read = NULL,
	.write = tfa98xx_reg_write,
};

static int tfa98xx_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	//struct snd_soc_dai_driver *dai;
	struct tfa98xx *tfa98xx;
	struct device_node *np = i2c->dev.of_node;
	int irq_flags;
	unsigned int reg;
	int ret;
	struct smartpa_vendor_info vendor_info;

	pr_info(" %s addr=0x%x\n", __func__, i2c->addr);

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
//	pr_debug("selected container profile [%d]\n", prof_idx);
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	tfa98xx = devm_kzalloc(&i2c->dev, sizeof(struct tfa98xx), GFP_KERNEL);
	if (tfa98xx == NULL)
		return -ENOMEM;

	tfa98xx->dev = &i2c->dev;
	tfa98xx->i2c = i2c;
	tfa98xx->dsp_init = TFA98XX_DSP_INIT_STOPPED;
	tfa98xx->rate = 48000; /* init to the default sample rate (48kHz) */
	tfa98xx->tfa = NULL;

	if (np) {
		ret = tfa98xx_parse_dt(&i2c->dev, tfa98xx, np);
		if (ret) {
			dev_err(&i2c->dev, "Failed to parse DT node\n");
			goto err;
		}
		if (no_start)
			tfa98xx->irq_gpio = -1;
		if (no_reset)
			tfa98xx->reset_gpio = -1;
	} else {
		tfa98xx->reset_gpio = -1;
		tfa98xx->irq_gpio = -1;
	}

	if (gpio_is_valid(tfa98xx->reset_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, tfa98xx->reset_gpio,
			GPIOF_OUT_INIT_LOW, "TFA98XX_RST");
		if (ret) {
			pr_err(" %s reset_gpio %d request failed",
					__func__, tfa98xx->reset_gpio);
			goto err;
		}
	}

	if (gpio_is_valid(tfa98xx->irq_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, tfa98xx->irq_gpio,
			GPIOF_DIR_IN, "TFA98XX_INT");
		if (ret) {
			pr_err(" %s irq_gpio %d request failed",
					__func__, tfa98xx->irq_gpio);
			goto err;
		}
	}

	/* Power up! */
	tfa98xx_ext_reset(tfa98xx);

	tfa98xx->regmap = devm_regmap_init_i2c(i2c, &tfa98xx_regmap);
	if (IS_ERR(tfa98xx->regmap)) {
		ret = PTR_ERR(tfa98xx->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		goto err;
	}

	i2c_set_clientdata(i2c, tfa98xx);
	mutex_init(&tfa98xx->dsp_lock);
	init_waitqueue_head(&tfa98xx->wq);


	if ((no_start == 0) && (no_reset == 0)) {
		ret = regmap_read(tfa98xx->regmap, 0x03, &reg);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to read Revision register: %d\n",
				ret);
			 goto err;
		}
		switch (reg & 0xff) {
		case 0x72: /* tfa9872 */
			pr_info("TFA9872 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_CALIBRATION_CTL;
			tfa98xx->flags |= TFA98XX_FLAG_REMOVE_PLOP_NOISE;
			/* tfa98xx->flags |= TFA98XX_FLAG_LP_MODES; */
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x74: /* tfa9874 */
			pr_info("TFA9874 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_CALIBRATION_CTL;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x88: /* tfa9888 */
			pr_info("TFA9888 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_STEREO_DEVICE;
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x13: /* tfa9912 */
			pr_info("TFA9912 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			/* tfa98xx->flags |= TFA98XX_FLAG_TAPDET_AVAILABLE; */
			break;
		case 0x94: /* tfa9894 */
			pr_info("TFA9894 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x80: /* tfa9890 */
		case 0x81: /* tfa9890 */
			pr_info("TFA9890 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			break;
		case 0x92: /* tfa9891 */
			pr_info("TFA9891 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SAAM_AVAILABLE;
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			break;
		case 0x12: /* tfa9895 */
			pr_info("TFA9895 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			break;
		case 0x97:
			pr_info("TFA9897 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x96:
			pr_info("TFA9896 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		default:
			pr_info("Unsupported device revision (0x%x)\n", reg & 0xff);
			goto err;
		}
	}

	tfa98xx->tfa = devm_kzalloc(&i2c->dev, sizeof(struct tfa_device), GFP_KERNEL);
	if (tfa98xx->tfa == NULL) {
		pr_err("%s kzalloc for tfa_device fail",
				__func__);
		goto err;
	}

	tfa98xx->tfa->data = (void *)tfa98xx;
	tfa98xx->tfa->cachep = tfa98xx_cache;

	if (gpio_is_valid(tfa98xx->irq_gpio) &&
		!(tfa98xx->flags & TFA98XX_FLAG_SKIP_INTERRUPTS)) {
		/* register irq handler */
		pr_info("%s IRQ registration\n", __func__);
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(&i2c->dev,
					gpio_to_irq(tfa98xx->irq_gpio),
					NULL, tfa98xx_irq, irq_flags,
					"tfa98xx", tfa98xx);
		if (ret != 0) {
			dev_err(&i2c->dev, "Failed to request IRQ %d: %d\n",
					gpio_to_irq(tfa98xx->irq_gpio), ret);
			goto err;
		}
	} else {
		dev_info(&i2c->dev, "Skipping IRQ registration\n");
		/* disable feature support if gpio was invalid */
		tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
	}

#ifdef CONFIG_DEBUG_FS
	if (no_start == 0)
		tfa98xx_debug_init(tfa98xx, i2c);
#endif

	/* Register the sysfs files for climax access */
	ret = device_create_bin_file(&i2c->dev, &dev_attr_rw);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs files\n");
	ret = device_create_bin_file(&i2c->dev, &dev_attr_reg);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs files\n");
	vendor_info.vendor = SMARTPA_VENDOR;
	vendor_info.chip_model = SMARTPA_MODEL;
	ret = smartpakit_set_info(&vendor_info);
	if (ret ) {
		pr_err("%s: set smartpa info err %d", __func__, ret);
		goto err;
	}
#if 1
	tfa98xx->misc_dev.minor = MISC_DYNAMIC_MINOR;
	tfa98xx->misc_dev.name = "tfa98xx";
	tfa98xx->misc_dev.fops = &tfa98xx_fops;

	ret = misc_register(&tfa98xx->misc_dev);
	if (ret)
		dev_info(&i2c->dev, "error creating misc device\n");
#endif	

	pr_info("%s Probe completed successfully!\n", __func__);

	INIT_LIST_HEAD(&tfa98xx->list);

	mutex_lock(&tfa98xx_mutex);
	tfa98xx_device_count++;
	list_add(&tfa98xx->list, &tfa98xx_device_list);
	mutex_unlock(&tfa98xx_mutex);
err:
	return ret;
}

static int tfa98xx_i2c_remove(struct i2c_client *i2c)
{
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);

	pr_debug("addr=0x%x\n", i2c->addr);

	tfa98xx_interrupt_enable(tfa98xx, false);

	tfa98xx_remove(tfa98xx);

	device_remove_bin_file(&i2c->dev, &dev_attr_reg);
	device_remove_bin_file(&i2c->dev, &dev_attr_rw);
#ifdef CONFIG_DEBUG_FS
	tfa98xx_debug_remove(tfa98xx);
#endif

	misc_deregister(&tfa98xx->misc_dev);

	if (gpio_is_valid(tfa98xx->irq_gpio))
		devm_gpio_free(&i2c->dev, tfa98xx->irq_gpio);
	if (gpio_is_valid(tfa98xx->reset_gpio))
		devm_gpio_free(&i2c->dev, tfa98xx->reset_gpio);

	mutex_lock(&tfa98xx_mutex);
	list_del(&tfa98xx->list);
	tfa98xx_device_count--;
	if (tfa98xx_device_count == 0) {
		if (tfa98xx_container) {
			kfree(tfa98xx_container);
			tfa98xx_container = NULL;
		}
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static const struct i2c_device_id tfa98xx_i2c_id[] = {
	{ "tfa98xx", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tfa98xx_i2c_id);

#ifdef CONFIG_OF
static struct of_device_id tfa98xx_dt_match[] = {
	{ .compatible = "nxp,tfa98xx" },
	{ .compatible = "nxp,tfa9872" },
	{ .compatible = "nxp,tfa9874" },
	{ .compatible = "nxp,tfa9888" },
	{ .compatible = "nxp,tfa9890" },
	{ .compatible = "nxp,tfa9891" },
	{ .compatible = "nxp,tfa9894" },
	{ .compatible = "nxp,tfa9895" },
	{ .compatible = "nxp,tfa9896" },
	{ .compatible = "nxp,tfa9897" },
	{ .compatible = "nxp,tfa9912" },
	{ },
};
#endif

static struct i2c_driver tfa98xx_i2c_driver = {
	.driver = {
		.name = "tfa98xx",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tfa98xx_dt_match),
	},
	.probe =    tfa98xx_i2c_probe,
	.remove =   tfa98xx_i2c_remove,
	.id_table = tfa98xx_i2c_id,
};

static int __init tfa98xx_i2c_init(void)
{
	int ret = 0;

	pr_info("TFA98XX driver version %s\n", TFA98XX_VERSION);

	/* Enable debug traces */
	tfa98xx_kmsg_regs = trace_level & 2;
	tfa98xx_ftrace_regs = trace_level & 4;

	/* Initialize kmem_cache */
	tfa98xx_cache = kmem_cache_create("tfa98xx_cache", /* Cache name /proc/slabinfo */
				PAGE_SIZE, /* Structure size, we should fit in single page */
				0, /* Structure alignment */
				(SLAB_HWCACHE_ALIGN | SLAB_RECLAIM_ACCOUNT |
				SLAB_MEM_SPREAD), /* Cache property */
				NULL); /* Object constructor */
	if (!tfa98xx_cache) {
		pr_err("tfa98xx can't create memory pool\n");
		ret = -ENOMEM;
	}

	ret = i2c_add_driver(&tfa98xx_i2c_driver);

	return ret;
}
late_initcall_sync(tfa98xx_i2c_init);

static void __exit tfa98xx_i2c_exit(void)
{
	i2c_del_driver(&tfa98xx_i2c_driver);
	kmem_cache_destroy(tfa98xx_cache);
}
module_exit(tfa98xx_i2c_exit);

MODULE_DESCRIPTION("Misc TFA98XX driver");
MODULE_AUTHOR("NXP Guy");
MODULE_LICENSE("GPL");
