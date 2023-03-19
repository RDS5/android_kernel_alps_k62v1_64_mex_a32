// SPDX-License-Identifier: GPL-2.0
/*
 * linux/drivers/md/iocache/shrinker.c
 *
 * Copyright (C) 2019 HUAWEI, Inc.
 *             http://www.huawei.com/
 * Created by Qiuyang Sun <sunqiuyang@huawei.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of the Linux
 * distribution for more details.
 */

#include "iocache.h"

static LIST_HEAD(iocache_list);
static DEFINE_SPINLOCK(iocache_list_lock);
static unsigned int shrinker_run_no;

unsigned long iocache_shrink_count(struct shrinker *shrink,
	struct shrink_control *sc)
{
	struct iocache_device *ioch;
	struct list_head *p;
	unsigned long count = 0;

	spin_lock(&iocache_list_lock);
	p = iocache_list.next;
	while (p != &iocache_list) {
		ioch = list_entry(p, struct iocache_device, list);
		spin_unlock(&iocache_list_lock);

		count += atomic_read(&ioch->tot_uppages);

		spin_lock(&iocache_list_lock);
		p = p->next;
	}
	spin_unlock(&iocache_list_lock);
	return count;
}

/* fake shrink */
unsigned long iocache_shrink_scan(struct shrinker *shrink,
	struct shrink_control *sc)
{
	unsigned long nr = sc->nr_to_scan;
	struct iocache_device *ioch;
	struct list_head *p;
	unsigned int run_no;

	spin_lock(&iocache_list_lock);
	do {
		run_no = ++shrinker_run_no;
	} while (run_no == 0);

	p = iocache_list.next;
	while (p != &iocache_list) {
		ioch = list_entry(p, struct iocache_device, list);

		if (ioch->shrinker_run_no == run_no)
			break;
		spin_unlock(&iocache_list_lock);

		ioch->shrinker_run_no = run_no;

		ioch->need_drain = true;
		wake_up_interruptible_all(&ioch->drain_trans_wait);
		spin_lock(&iocache_list_lock);
		p = p->next;
		list_move_tail(&ioch->list, &iocache_list);
	}
	spin_unlock(&iocache_list_lock);
	return 0;
}

void iocache_join_shrinker(struct iocache_device *ioch)
{
	spin_lock(&iocache_list_lock);
	list_add_tail(&ioch->list, &iocache_list);
	spin_unlock(&iocache_list_lock);
}

void iocache_leave_shrinker(struct iocache_device *ioch)
{
	spin_lock(&iocache_list_lock);
	list_del_init(&ioch->list);
	spin_unlock(&iocache_list_lock);
}
