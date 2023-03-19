/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * DW9714AF voice coil motor driver
 *
 *
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>

#include "lens_info.h"
#include "DW9714AF.h"

#define AF_DRVNAME "DW9714AF_DRV"
#define AF_I2C_SLAVE_ADDR 0x18
#define AF_DW9714_FILE_NAME "DW9714AF"

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...)                                               \
	pr_debug(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif
#define LOG_ERR(format, args...)                                               \
	pr_err(AF_DRVNAME " [%s] " format, __func__, ##args)

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened=0;
static spinlock_t *g_pAF_SpinLock;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4CurrPosition;
static unsigned int g_u4AF_dri_mode = DRIVING_MODE_DIRECT_MODE;
extern char uMotorName[];
static unsigned int AF_Name_Index = 0;
static int (*s4AF_WriteReg)(u16 a_u2Data) = NULL;

static int32_t af_sensor_write_table(
		struct af_i2c_reg *setting, int32_t size)
{
	int32_t rc = 0;
	int32_t  i = 0;
	char puSendCmd[2] = {0};
	unsigned short addr = 0, data = 0, delay = 0;
	if(NULL == setting){
		LOG_ERR("there are some wrong of the setting!\n");
		return -1;
	}
	for( i = 0; i < size; i++ ) {
		addr = setting[i].addr;
		data = setting[i].data;
		delay = setting[i].delay;
		puSendCmd[0] = (int8_t)addr;
		puSendCmd[1] = (int8_t)data;
		g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;
		g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;
		rc = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
		LOG_INF("puSendCmd0=0x%2x,puSendCmd1=0x%2x\n",puSendCmd[0],puSendCmd[1]);
		if(delay > 0){
			mdelay(delay);
		}
	}
	LOG_INF("Exit. size:%d\n", size);
	return 0;
}

static int32_t af_sensor_write_Exittable(
		struct af_exit_reg *setting, int32_t size)
{
	int32_t  i = 0;
	unsigned short  data = 0, delay = 0;
	if(NULL == setting){
		LOG_ERR("there are some wrong of exit setting!\n");
		return -1;
	}
	for( i = 0; i < size; i++ ){
		data = setting[i].data;
		delay = setting[i].delay;
		if(NULL != s4AF_WriteReg){
			s4AF_WriteReg(data);
			if(delay > 0){
				mdelay(delay);
			}
		}
		else{
			LOG_ERR("s4AF_WriteReg failed!!\n");
			return -1;
		}
	}
	LOG_INF("Exit. size:%d\n", size);
	return 0;
}

static int hi1333_qtech_initAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_table(hi1333_qtech_init_setting,sizeof(hi1333_qtech_init_setting)/sizeof(struct af_i2c_reg));
	return i4RetValue;
}

static int hi1336_qtech_initAF(void)
{
	int ret = af_sensor_write_table(hi1336_qtech_init_setting,
		sizeof(hi1336_qtech_init_setting) / sizeof(struct af_i2c_reg));
	return ret;
}
static int ov13855_ofilm_tdk_initAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_table(ov13855_oflim_tdk_init_setting,sizeof(ov13855_oflim_tdk_init_setting)/sizeof(struct af_i2c_reg));
	return i4RetValue;
}

static int ov13855_ofilm_initAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_table(ov13855_oflim_init_setting,sizeof(ov13855_oflim_init_setting)/sizeof(struct af_i2c_reg));
	return i4RetValue;
}

static int s5k3l6_liteon_initAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_table(s5k3l6_liteon_init_setting,sizeof(s5k3l6_liteon_init_setting)/sizeof(struct af_i2c_reg));
	return i4RetValue;
}

static int s5k3l6_truly_initAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_table(s5k3l6_truly_init_setting,sizeof(s5k3l6_truly_init_setting)/sizeof(struct af_i2c_reg));
	return i4RetValue;
}

static int imx258_sunny_zet_initAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_table(imx258_sunny_zet_init_setting,sizeof(imx258_sunny_zet_init_setting)/sizeof(struct af_i2c_reg));
	return i4RetValue;
}

static int imx258_sunny_initAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_table(imx258_sunny_init_setting,sizeof(imx258_sunny_init_setting)/sizeof(struct af_i2c_reg));
	return i4RetValue;
}

static int hi846_truly_initAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_table(hi846_truly_init_setting,sizeof(hi846_truly_init_setting)/sizeof(struct af_i2c_reg));
	return i4RetValue;
}

static int gc8034_sunwin_initAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_table(gc8034_sunwin_init_setting,sizeof(gc8034_sunwin_init_setting)/sizeof(struct af_i2c_reg));
	return i4RetValue;
}

static int gc8034_txd_initAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_table(gc8034_txd_init_setting,sizeof(gc8034_txd_init_setting)/sizeof(struct af_i2c_reg));
	return i4RetValue;
}
static int ov8856_hlt_initAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_table(ov8856_hlt_init_setting,sizeof(ov8856_hlt_init_setting)/sizeof(struct af_i2c_reg));
	return i4RetValue;
}
static int s5k4h7_txd_init_af(void)
{
	int ret = 0;
	ret = af_sensor_write_table(s5k4h7_txd_init_setting,
		sizeof(s5k4h7_txd_init_setting) / sizeof(struct af_i2c_reg));
	return ret;
}
static int hi1333_qtech_exitAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_Exittable(hi1333_qtech_exit_setting,sizeof(hi1333_qtech_exit_setting)/sizeof(struct af_exit_reg));
	return i4RetValue;
}

static int hi1336_qtech_exitAF(void)
{
	int ret = af_sensor_write_Exittable(hi1336_qtech_exit_setting,
		sizeof(hi1336_qtech_exit_setting) / sizeof(struct af_exit_reg));
	return ret;
}

static int ov13855_ofilm_tdk_exitAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_Exittable(ov13855_oflim_tdk_exit_setting,sizeof(ov13855_oflim_tdk_exit_setting)/sizeof(struct af_exit_reg));
	return i4RetValue;
}

static int ov13855_ofilm_exitAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_Exittable(ov13855_oflim_exit_setting,sizeof(ov13855_oflim_exit_setting)/sizeof(struct af_exit_reg));
	return i4RetValue;
}

static int s5k3l6_liteon_exitAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_Exittable(s5k3l6_liteon_exit_setting,sizeof(s5k3l6_liteon_exit_setting)/sizeof(struct af_exit_reg));
	return i4RetValue;
}

static int s5k3l6_truly_exitAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_Exittable(s5k3l6_truly_exit_setting,sizeof(s5k3l6_truly_exit_setting)/sizeof(struct af_exit_reg));
	return i4RetValue;
}

static int imx258_sunny_zet_exitAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_Exittable(imx258_sunny_zet_exit_setting,sizeof(imx258_sunny_zet_exit_setting)/sizeof(struct af_exit_reg));
	return i4RetValue;
}

static int imx258_sunny_exitAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_Exittable(imx258_sunny_exit_setting,sizeof(imx258_sunny_exit_setting)/sizeof(struct af_exit_reg));
	return i4RetValue;
}

static int hi846_truly_exitAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_Exittable(hi846_truly_exit_setting,sizeof(hi846_truly_exit_setting)/sizeof(struct af_exit_reg));
	return i4RetValue;
}

static int gc8034_sunwin_exitAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_Exittable(gc8034_sunwin_exit_setting,sizeof(gc8034_sunwin_exit_setting)/sizeof(struct af_exit_reg));
	return i4RetValue;
}

static int gc8034_txd_exitAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_Exittable(gc8034_txd_exit_setting,sizeof(gc8034_txd_exit_setting)/sizeof(struct af_exit_reg));
	return i4RetValue;
}
static int ov8856_hlt_exitAF(void)
{
	int i4RetValue = 0;
	i4RetValue= af_sensor_write_Exittable(ov8856_hlt_exit_setting,sizeof(ov8856_hlt_exit_setting)/sizeof(struct af_exit_reg));
	return i4RetValue;
}

static int s5k4h7_txd_exit_af(void)
{
	int ret = 0;
	ret = af_sensor_write_Exittable(s5k4h7_txd_exit_setting,
		sizeof(s5k4h7_txd_exit_setting) / sizeof(struct af_exit_reg));
	return ret;
}

static struct af_drv_select_func selectAF_table[] = {
	{"DW9714AF_HI1333_QTECH",hi1333_qtech_initAF,hi1333_qtech_exitAF},
	{ "DW9714AF_HI1336_QTECH", hi1336_qtech_initAF, hi1336_qtech_exitAF },
	{"DW9714AF_OV13855_OFILM_TDK",ov13855_ofilm_tdk_initAF,ov13855_ofilm_tdk_exitAF},
	{"DW9714AF_OV13855_OFILM",ov13855_ofilm_initAF,ov13855_ofilm_exitAF},
	{"DW9714AF_S5K3L6_LITEON",s5k3l6_liteon_initAF,s5k3l6_liteon_exitAF},
	{"DW9714AF_S5K3L6_TRULY",s5k3l6_truly_initAF,s5k3l6_truly_exitAF},
	{"DW9714AF_IMX258_SUNNY_ZET",imx258_sunny_zet_initAF,imx258_sunny_zet_exitAF},
	{"DW9714AF_IMX258_SUNNY",imx258_sunny_initAF,imx258_sunny_exitAF},
	{"DW9714AF_HI846_TRULY",hi846_truly_initAF,hi846_truly_exitAF},
	{"DW9714AF_GC8034_SUNWIN",gc8034_sunwin_initAF,gc8034_sunwin_exitAF},
	{"DW9714AF_GC8034_TXD",gc8034_txd_initAF,gc8034_txd_exitAF},
	{"DW9714AF_OV8856_HLT",ov8856_hlt_initAF,ov8856_hlt_exitAF},
	{"DW9714AF_S5K4H7_TXD", s5k4h7_txd_init_af, s5k4h7_txd_exit_af},
};

static int s4AF_ReadReg(u16 addr, u16 *data)
{
	u8 u8data=0;
	u8 pu_send_cmd[2] = {(u8)(addr & 0xFF),(u8)(addr >> 8)};

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;
	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;
	if (i2c_master_send(g_pstAF_I2Cclient, pu_send_cmd, 1) < 0) {
		LOG_INF("read I2C send failed!!\n");
		return -1;
	}
	if (i2c_master_recv(g_pstAF_I2Cclient, &u8data, 1) < 0) {
		LOG_INF("AF_ReadReg failed!!\n");
		return -1;
	}
	*data = u8data;
	LOG_INF("actuator 0x%x, 0x%x\n", addr, *data);

	return 0;
}

static int s4AF_DirectMode_WriteReg(u16 a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmd[2] = {(char)(a_u2Data >> 4),
			     (char)((a_u2Data & 0xF) << 4)};
	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;
	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}
	return 0;
}

static int s4AF_SacMode_WriteReg(u16 a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmd[2] = {0x03,(char)((a_u2Data & 0x0300) >>8)};
	char puSendCmd2[2] ={0x04,(char)(a_u2Data & 0xFF)};
	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;
	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	LOG_INF("####AF_Name=%s!!\n",uMotorName);
	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd2, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C send cmd 2 failed!!\n");
		return -1;
	}
	return 0;
}

static inline int getAFInfo(__user struct stAF_MotorInfo *pstMotorInfo)
{
	struct stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;

	stMotorInfo.bIsMotorMoving = 1;

	if (*g_pAF_Opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo,
			 sizeof(struct stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

/* initAF include driver initialization and standby mode */
static int initAF(void)
{
	LOG_INF("+\n");
	if (*g_pAF_Opened == 1) {
		int i4RetValue = 0;
		unsigned int index=0;
		int (*doInitAF)(void);
		doInitAF=NULL;
		s4AF_WriteReg = s4AF_DirectMode_WriteReg;
		for(index=0;index<sizeof(selectAF_table)/sizeof(struct af_drv_select_func);index++)
		{
			if(strcmp(uMotorName,selectAF_table[index].uMotorName)==0)
			{
				AF_Name_Index = index;
				doInitAF=selectAF_table[index].initAF;
				g_u4AF_dri_mode = DRIVING_MODE_SAC_MODE;
				s4AF_WriteReg = s4AF_SacMode_WriteReg;
				break;
			}
		}
		if(NULL != doInitAF)
		{
			i4RetValue = doInitAF();
		}
		if (i4RetValue < 0) {
			LOG_INF("I2C send failed!!\n");
			return -1;
		}
		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("-\n");

	return 0;
}

static int exitAF(void)
{
	int (*doExitAF)(void);
	doExitAF=NULL;
	LOG_INF("+\n");
	if( AF_Name_Index >=
		sizeof(selectAF_table)/sizeof(struct af_drv_select_func)){
		LOG_ERR("Invalid AF index: %d.\n",AF_Name_Index);
		return -1;
	}
	doExitAF=selectAF_table[AF_Name_Index].exitAF;
	if(NULL == doExitAF){
		LOG_ERR("get doExitAF error!\n");
		return -1;
	}
	if(doExitAF()){
		LOG_INF("I2C send failed!!\n");
		return -1;
	}
	LOG_INF("-\n");
	return 0;
}
/* moveAF only use to control moving the motor */
static int move_af_times=0;
static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;
	unsigned short Reg_0x05=0,Reg_0x00=0,Reg_0x06=0;
	LOG_INF("moveAF %ld\n", a_u4Position);
	if (*g_pAF_Opened == 1)
	{
		initAF();
	}
	if(g_u4AF_dri_mode == DRIVING_MODE_SAC_MODE)
	{
		move_af_times++;
		if(move_af_times%AF_REINIT_ESD_CHECK ==0)
		{
			s4AF_ReadReg(DW9714V_STATUS_REG,&Reg_0x05);
			s4AF_ReadReg(DW9714V_VCM_CFG_REG,&Reg_0x06);
			s4AF_ReadReg(DW9714V_VCM_IC_INFO_REG,&Reg_0x00);
			if(((Reg_0x05&DW9714V_BUSY_STATE_MASK) != 0)||(DW9714V_VCM_IC_INFO_DEFAULT!= Reg_0x00)||(Reg_0x06== 0))
			{
				LOG_ERR("AF_REINIT_ESD_CHECK: Reg_0x05=0x%x,Reg_0x06=0x%x, Reg_0x00=0x%x.\n", Reg_0x05, Reg_0x06, Reg_0x00);
				af_sensor_write_table(af_reset_setting,sizeof(af_reset_setting)/sizeof(struct af_i2c_reg));
				spin_lock(g_pAF_SpinLock);
				*g_pAF_Opened = 1;
				spin_unlock(g_pAF_SpinLock);
				initAF();
			}
			move_af_times=0;
		}
	}
	if((a_u4Position&0xF000))
	{
		LOG_INF("Motor position exceeds maximum\n");
		ret =-1;
	}
	if(s4AF_WriteReg != NULL)
	{
		if (s4AF_WriteReg((unsigned short)a_u4Position) == 0) {
			ret = 0;
		} else {
			LOG_INF("set I2C failed when moving the motor\n");
			ret = -1;
		}
	}

	return ret;
}

static inline int setAFInf(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_INF = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

static inline int setAFMacro(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_MACRO = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

/* ////////////////////////////////////////////////////////////// */
long DW9714AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
		    unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue =
			getAFInfo((__user struct stAF_MotorInfo *)(a_u4Param));
		break;

	case AFIOC_T_MOVETO:
		i4RetValue = moveAF(a_u4Param);
		break;

	case AFIOC_T_SETINFPOS:
		i4RetValue = setAFInf(a_u4Param);
		break;

	case AFIOC_T_SETMACROPOS:
		i4RetValue = setAFMacro(a_u4Param);
		break;

	default:
		LOG_INF("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	return i4RetValue;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
int DW9714AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");
	if (*g_pAF_Opened == 2) {
		LOG_INF("Wait\n");
		if (exitAF()){
			LOG_ERR("exitAF failed!!\n");
		}
		if(g_u4AF_dri_mode == DRIVING_MODE_SAC_MODE)
		{
			af_sensor_write_table(sac_mode_power_off_setting,sizeof(sac_mode_power_off_setting)/sizeof(struct af_i2c_reg));
		}
		else
		{
			af_sensor_write_table(direct_mode_power_off_setting,sizeof(direct_mode_power_off_setting)/sizeof(struct af_i2c_reg));
		}
	}

	if (*g_pAF_Opened) {
		LOG_INF("Free\n");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("End\n");

	return 0;
}

int DW9714AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			  spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;
	LOG_INF("pAF_Opened=%d",*pAF_Opened);


	return 1;
}

int DW9714AF_GetFileName(unsigned char *pFileName)
{
	if(NULL == pFileName)
	{
		LOG_ERR("pFileName is NULL error!\n");
		return -1;
	}
	strncpy(pFileName, AF_DW9714_FILE_NAME, AF_MOTOR_NAME);
	LOG_INF("FileName:%s\n", pFileName);

	return 1;
}
