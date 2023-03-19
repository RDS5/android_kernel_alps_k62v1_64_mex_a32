/*
 *  linux/include/rwlog/rwlog.h
 *
 *  Copyright (C) 2003 Russell King, All Rights Reserved.
 *  Copyright 2007 Pierre Ossman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _HUAWEI_RWLOG_H
#define _HUAWEI_RWLOG_H

#include <linux/version.h>
#include <linux/kernel.h>

void rwlog_init(void);
void mmc_start_request_rwlog(struct mmc_host *host, struct mmc_request *mrq);
void mmc_start_cmdq_request_rwlog(struct mmc_blk_request *brq, struct request *req);

#endif
