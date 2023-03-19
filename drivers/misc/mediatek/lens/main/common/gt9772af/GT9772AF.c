/*
 * GT9772AF.c
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

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "lens_info.h"

#include <securec.h>

#define AF_DRVNAME "GT9772AF_DRV"
#define AF_I2C_SLAVE_ADDR        0x18 //
#define AF_GT9772AF_FILE_NAME "GT9772AF"

#define AF_DEBUG
#ifdef AF_DEBUG
#define log_inf(format, args...)  \
	pr_debug(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define log_inf(format, args...)
#endif

/* if use ISRC mode, should modify variables in init_setting */


static struct i2c_client *g_pstaf_i2cclient;
static int *g_paf_opened;
static spinlock_t *g_paf_spinlock;
static long g_i4motorstatus;
static long g_i4dir;

static unsigned long g_u4af_inf;
static unsigned long g_u4af_macro = 1023;
static unsigned long g_u4targetposition;
static unsigned long g_u4currposition;

static int s4af_readreg(unsigned short *a_pu2result)
{
	int i4retvalue = 0;
	char pbuff[2];

	i4retvalue = i2c_master_recv(g_pstaf_i2cclient, pbuff, 2);

	if (i4retvalue < 0) {
		log_inf("I2C read failed!!\n");
		return -1;
	}

	*a_pu2result = (((u16) pbuff[0]) << 2) + (pbuff[1]);

	return 0;
}

static int s4af_writereg(u16 a_u2data)
{
	int i4retvalue = 0;

	char pusendcmd[3] = { 0x03,
					(char)(a_u2data >> 8),
					(char)(a_u2data & 0xFF) };

	g_pstaf_i2cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstaf_i2cclient->addr = g_pstaf_i2cclient->addr >> 1;

	i4retvalue = i2c_master_send(g_pstaf_i2cclient, pusendcmd, 3);

	if (i4retvalue < 0) {
		log_inf("I2C send failed!!\n");
		return -1;
	}

	return 0;
}

static int gt9772_init(void)
{
	int i4retvalue = 0;
	char pusendcmd[3];

	log_inf("%s, - beg\n", __func__);
	pusendcmd[0] = 0xED;
	pusendcmd[1] = 0xAB;
	i4retvalue = i2c_master_send(g_pstaf_i2cclient, pusendcmd, 2);
	if (i4retvalue < 0)
		log_inf("%s I2C send failed!!\n", __func__);
	mdelay(5);
	pusendcmd[0] = 0x06;
	pusendcmd[1] = 0x88;   // AAC
	i4retvalue = i2c_master_send(g_pstaf_i2cclient, pusendcmd, 2);
	if (i4retvalue < 0)
		log_inf("[GT9772AF] I2C send failed!!\n");
	pusendcmd[0] = 0x07;
	pusendcmd[1] = 0x01;
	i4retvalue = i2c_master_send(g_pstaf_i2cclient, pusendcmd, 2);
	if (i4retvalue < 0)
		log_inf("[GT9772AF] I2C send failed!!\n");
	pusendcmd[0] = 0x08;
	pusendcmd[1] = 0x48;
	i4retvalue = i2c_master_send(g_pstaf_i2cclient, pusendcmd, 2);
	if (i4retvalue < 0)
		log_inf(KERN_NOTICE "[GT9772AF] I2C send failed!!\n");
	log_inf("[GT9772AF] GT97772 SRC Init End!!\n");
	return i4retvalue;
}

static int getafinfo(__user struct stAF_MotorInfo *pstmotorinfo)
{
	struct stAF_MotorInfo stmotorinfo;

	stmotorinfo.u4MacroPosition = g_u4af_macro;
	stmotorinfo.u4InfPosition = g_u4af_inf;
	stmotorinfo.u4CurrentPosition = g_u4currposition;
	stmotorinfo.bIsSupportSR = 1;

	if (g_i4motorstatus == 1)
		stmotorinfo.bIsMotorMoving = 1;
	else
		stmotorinfo.bIsMotorMoving = 0;

	if (*g_paf_opened >= 1)
		stmotorinfo.bIsMotorOpen = 1;
	else
		stmotorinfo.bIsMotorOpen = 0;

	if (copy_to_user(pstmotorinfo, &stmotorinfo,
		sizeof(struct stAF_MotorInfo)))
		log_inf("copy to user failed when getting motor information\n");

	return 0;
}

static int moveaf(unsigned long a_u4position)
{
	int ret = 0;

	if ((a_u4position > g_u4af_macro) || (a_u4position < g_u4af_inf)) {
		log_inf("out of range\n");
		return -EINVAL;
	}

	if (*g_paf_opened == 1) {
		unsigned short initpos;

		gt9772_init();

		ret = s4af_readreg(&initpos);

		if (ret == 0) {
			spin_lock(g_paf_spinlock);
			g_u4currposition = (unsigned long)initpos;
			spin_unlock(g_paf_spinlock);

		} else {
			spin_lock(g_paf_spinlock);
			g_u4currposition = 0;
			spin_unlock(g_paf_spinlock);
		}

		spin_lock(g_paf_spinlock);
		*g_paf_opened = 2;
		spin_unlock(g_paf_spinlock);
	}

	if (g_u4currposition < a_u4position) {
		spin_lock(g_paf_spinlock);
		g_i4dir = 1;
		spin_unlock(g_paf_spinlock);
	} else if (g_u4currposition > a_u4position) {
		spin_lock(g_paf_spinlock);
		g_i4dir = -1;
		spin_unlock(g_paf_spinlock);
	} else {
		return 0;
	}

	spin_lock(g_paf_spinlock);
	g_u4targetposition = a_u4position;
	spin_unlock(g_paf_spinlock);

	/* log_inf("move [curr] %d [target] %d\n", */
	/* g_u4currposition, g_u4targetposition); */

	spin_lock(g_paf_spinlock);
	g_i4motorstatus = 0;
	spin_unlock(g_paf_spinlock);

	if (s4af_writereg((unsigned short)g_u4targetposition) == 0) {
		spin_lock(g_paf_spinlock);
		g_u4currposition = (unsigned long)g_u4targetposition;
		spin_unlock(g_paf_spinlock);
	} else {
		log_inf("set I2C failed when moving the motor\n");

		spin_lock(g_paf_spinlock);
		g_i4motorstatus = -1;
		spin_unlock(g_paf_spinlock);
	}

	return 0;
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

long GT9772AF_Ioctl(struct file *a_pstfile,
unsigned int a_u4command, unsigned long a_u4param)
{
	long i4retvalue = 0;

	switch (a_u4command) {
	case AFIOC_G_MOTORINFO:
		i4retvalue =
		getafinfo((__user struct stAF_MotorInfo *) (a_u4param));
		break;
	case AFIOC_T_MOVETO:
		i4retvalue = moveaf(a_u4param);
		break;
	case AFIOC_T_SETINFPOS:
		i4retvalue = setafinf(a_u4param);
		break;
	case AFIOC_T_SETMACROPOS:
		i4retvalue = setafmacro(a_u4param);
		break;
	default:
		log_inf("No CMD\n");
		i4retvalue = -EPERM;
		break;
	}

	return i4retvalue;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
int GT9772AF_Release(struct inode *a_pstinode, struct file *a_pstfile)
{
	log_inf("Start\n");

	if (*g_paf_opened == 2) {
		log_inf("Wait\n");
		s4af_writereg(300); // actuator move position 50*6
		msleep(20); // sleep time 20ms for actuator stable
		s4af_writereg(250); // actuator move position 50*5
		msleep(20); // sleep time 20ms for actuator stable
		s4af_writereg(150); // actuator move position 50*3
		msleep(20); // sleep time 20ms for actuator stable
		s4af_writereg(100); // actuator move position 50*2
		msleep(20); // sleep time 20ms for actuator stable
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

int GT9772AF_SetI2Cclient(struct i2c_client *pstaf_i2cclient,
	spinlock_t *paf_spinlock, int *paf_opened)
{
	g_pstaf_i2cclient = pstaf_i2cclient;
	g_paf_spinlock = paf_spinlock;
	g_paf_opened = paf_opened;

	return 1;
}

int GT9772AF_GetFileName(unsigned char *pfilename)
{
	if (!pfilename) {
		log_inf("pfilename is NULL error!\n");
		return -1;
	}
	strncpy_s(pfilename, AF_MOTOR_NAME + 1, AF_GT9772AF_FILE_NAME, AF_MOTOR_NAME);
	log_inf("FileName:%s\n", pfilename);

	return 1;
}


