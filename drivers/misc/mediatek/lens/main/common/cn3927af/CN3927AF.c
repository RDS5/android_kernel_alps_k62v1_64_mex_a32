/*
 * CN3927AF.c
 * bring up for actuator
 * Copyright (C) Huawei Technology Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>

#include <securec.h>

#include "lens_info.h"

#define AF_DRVNAME "CN3927AF_DRV"
#define AF_I2C_SLAVE_ADDR 0x18
#define AF_CN3927AF_FILE_NAME "CN3927AF"
#define AF_DEBUG
#ifdef AF_DEBUG
#define log_inf(format, args...)                                               \
	pr_debug(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define log_inf(format, args...)
#endif

static struct i2c_client *g_pstaf_i2cclient;
static int *g_paf_opened;
static spinlock_t *g_paf_spinlock;

static unsigned long g_u4af_inf;
static unsigned long g_u4af_macro = 1023;
static unsigned long g_u4currposition;

static int s4af_writereg(u16 a_u2data)
{
	int i4retvalue = 0;

	char pusendcmd[2] = {(char)(a_u2data >> 4),
			     (char)((a_u2data & 0xF) << 4)};

	g_pstaf_i2cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstaf_i2cclient->addr = g_pstaf_i2cclient->addr >> 1;

	i4retvalue = i2c_master_send(g_pstaf_i2cclient, pusendcmd, 2);

	if (i4retvalue < 0) {
		log_inf("I2C send failed!!\n");
		return -1;
	}

	return 0;
}

static int s4af_writereg_directly(u16 a_u2data)
{
	int i4retvalue = 0;
	char pusendcmd[2] = { (char)(a_u2data >> 8), (char)(a_u2data & 0xFF) };

	g_pstaf_i2cclient->addr = AF_I2C_SLAVE_ADDR;
	g_pstaf_i2cclient->addr = g_pstaf_i2cclient->addr >> 1;
	i4retvalue = i2c_master_send(g_pstaf_i2cclient, pusendcmd, 2);
	if (i4retvalue < 0) {
		log_inf("I2C send failed!!\n");
		return -1;
	}
	return 0;
}

static int getAFInfo(__user struct stAF_MotorInfo *pstMotorInfo)
{
	struct stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4af_macro;
	stMotorInfo.u4InfPosition = g_u4af_inf;
	stMotorInfo.u4CurrentPosition = g_u4currposition;
	stMotorInfo.bIsSupportSR = 1;

	stMotorInfo.bIsMotorMoving = 1;

	if (*g_paf_opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo,
			 sizeof(struct stAF_MotorInfo)))
		log_inf("copy to user failed when getting motor information\n");

	return 0;
}

/* initaf include driver initialization and standby mode */
static int initaf(void)
{
	log_inf("+\n");

	if (*g_paf_opened == 1) {

		spin_lock(g_paf_spinlock);
		*g_paf_opened = 2;
		spin_unlock(g_paf_spinlock);
	}

	log_inf("-\n");

	return 0;
}

/* moveaf only use to control moving the motor */
static int moveaf(unsigned long a_u4position)
{
	int ret = 0;

	if (s4af_writereg((unsigned short)a_u4position) == 0) {
		g_u4currposition = a_u4position;
		ret = 0;
	} else {
		log_inf("set I2C failed when moving the motor\n");
		ret = -1;
	}

	return ret;
}

static int setafinf(unsigned long a_u4position)
{
	spin_lock(g_paf_spinlock);
	g_u4af_inf = a_u4position;
	spin_unlock(g_paf_spinlock);
	return 0;
}

static int setafmacro(unsigned long a_u4position)
{
	spin_lock(g_paf_spinlock);
	g_u4af_macro = a_u4position;
	spin_unlock(g_paf_spinlock);
	return 0;
}

/* ////////////////////////////////////////////////////////////// */
long CN3927AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
	unsigned long a_u4Param)
{
	long i4retvalue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4retvalue =
			getAFInfo((__user struct stAF_MotorInfo *)(a_u4Param));
		break;

	case AFIOC_T_MOVETO:
		i4retvalue = moveaf(a_u4Param);
		break;

	case AFIOC_T_SETINFPOS:
		i4retvalue = setafinf(a_u4Param);
		break;

	case AFIOC_T_SETMACROPOS:
		i4retvalue = setafmacro(a_u4Param);
		break;

	default:
		log_inf("No CMD\n");
		i4retvalue = -EPERM;
		break;
	}

	return i4retvalue;
}

static int s4af_edlc_mode_power_off(void)
{
	s4af_writereg_directly(0xECA3);
	s4af_writereg_directly(0xA114);
	s4af_writereg_directly(0xF230);
	s4af_writereg_directly(0xDC51);
	s4af_writereg_directly(0x12C0);
	mdelay(15); // 15: move pos delay time
	s4af_writereg_directly(0xECA3);
	s4af_writereg_directly(0xA100);
	s4af_writereg_directly(0xF200);
	s4af_writereg_directly(0xDC51);
	s4af_writereg_directly(0x0006);
	return 0;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
int CN3927AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	log_inf("Start\n");

	if (*g_paf_opened == 2) {
		log_inf("Wait\n");
		s4af_edlc_mode_power_off();
		mdelay(100); // 100: poweroff delay time
	}

	if (*g_paf_opened) {
		log_inf("Free\n");

		spin_lock(g_paf_spinlock);
		*g_paf_opened = 0;
		spin_unlock(g_paf_spinlock);
	}

	log_inf("End\n");

	return 0;
}

int CN3927AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
	spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstaf_i2cclient = pstAF_I2Cclient;
	g_paf_spinlock = pAF_SpinLock;
	g_paf_opened = pAF_Opened;

	initaf();

	return 1;
}

int CN3927AF_GetFileName(unsigned char *pFileName)
{
	if(NULL == pFileName){
		log_inf("pFileName is NULL error!\n");
		return -1;
	}
	strncpy_s(pFileName, AF_MOTOR_NAME + 1, AF_CN3927AF_FILE_NAME, AF_MOTOR_NAME);
	log_inf("FileName:%s\n", pFileName);
	return 1;
}
