/*
 * A fast way to read system free and aviliable meminfo
 *
 * Copyright (c) 2001-2021, Huawei Tech. Co., Ltd. All rights reserved.
 *
 * Authors:
 * liang hui <lianghuiliang.lianghui@huawei.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/fs.h>
#include <linux/hugetlb.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/mmzone.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/vmstat.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <asm/page.h>
#include <asm/pgtable.h>

static int meminfo_lite_proc_show(struct seq_file *m, void *v)
{
	struct sysinfo i;
	long available;

	/*
	 * display in kilobytes.
	 */
#define K(x) (((x) << (PAGE_SHIFT - 10)))
	si_meminfo(&i);
	available = si_mem_available();

	if (available < 0)
		available = 0;

	/*
	 * Tagged format, for easy grepping and expansion.
	 */
	seq_printf(m, "Fr:%lu,Av:%lu\n", K(i.freeram), K(available));

	hugetlb_report_meminfo(m);

	return 0;
#undef K
}

static int meminfo_lite_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, meminfo_lite_proc_show, NULL);
}

static const struct file_operations meminfo_lite_proc_fops = {
	.open		= meminfo_lite_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_meminfo_lite_init(void)
{
	proc_create("meminfo_lite", 0, NULL, &meminfo_lite_proc_fops);
	return 0;
}

module_init(proc_meminfo_lite_init);
