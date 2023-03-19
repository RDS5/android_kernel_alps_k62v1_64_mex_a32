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

#ifndef _LENS_LIST_H

#define _LENS_LIST_H

#define DW9714AF_SetI2Cclient DW9714AF_SetI2Cclient_Main
#define DW9714AF_Ioctl DW9714AF_Ioctl_Main
#define DW9714AF_Release DW9714AF_Release_Main
#define DW9714AF_GetFileName DW9714AF_GetFileName_Main
extern int DW9714AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9714AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int DW9714AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int DW9714AF_GetFileName(unsigned char *pFileName);
#define CN3927AF_SetI2Cclient CN3927AF_SetI2Cclient_Main
#define CN3927AF_Ioctl CN3927AF_Ioctl_Main
#define CN3927AF_Release CN3927AF_Release_Main
#define CN3927AF_GetFileName CN3927AF_GetFileName_Main
extern int CN3927AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
	spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long CN3927AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
	unsigned long a_u4Param);
extern int CN3927AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int CN3927AF_GetFileName(unsigned char *pFileName);

#define GT9772AF_SetI2Cclient GT9772AF_SetI2Cclient_Main
#define GT9772AF_Ioctl GT9772AF_Ioctl_Main
#define GT9772AF_Release GT9772AF_Release_Main
#define GT9772AF_GetFileName GT9772AF_GetFileName_Main
extern int GT9772AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
	 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long GT9772AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
	unsigned long a_u4Param);
extern int GT9772AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int GT9772AF_GetFileName(unsigned char *pFileName);
#endif
