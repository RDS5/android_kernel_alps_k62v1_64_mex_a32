/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
 * Description: get runmode type
 * Author: zhoubokai zhoubokai@huawei.com
 * Create: 2018-12-22
 */

#include <hwmanufac/runmode_type/runmode_type.h>
#include <linux/init.h>
#include <linux/string.h>
#include <huawei_platform/log/hw_log.h>

#define RUNMODE_TYPE_NORMAL     0
#define RUNMODE_TYPE_FACTORY    1
#define RUNMODE_TYPE_TAG        "androidboot.swtype"
#define HWLOG_TAG               runmode
HWLOG_REGIST();

static unsigned int runmode_type = RUNMODE_TYPE_NORMAL;

static int __init early_parse_runmode_cmdline(char *p)
{
	if (p) {
		if (!strncmp(p, "factory", strlen("factory")))
			runmode_type = RUNMODE_TYPE_FACTORY;
		hwlog_err("runmode is %s, runmode_type = %u\n", p, runmode_type);
	}
	return 0;
}

early_param(RUNMODE_TYPE_TAG, early_parse_runmode_cmdline);

/*
* the function interface to check factory/normal mode in kernel
* return 0 is normal type ,return 1 is factory type
*/
unsigned int get_runmode_type(void)
{
	return runmode_type;
}
