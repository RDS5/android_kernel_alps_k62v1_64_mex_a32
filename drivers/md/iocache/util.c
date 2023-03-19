// SPDX-License-Identifier: GPL-2.0
/*
 * linux/drivers/md/iocache/util.c
 *
 * Copyright (C) 2019 HUAWEI, Inc.
 *             http://www.huawei.com/
 * Created by Gao Xiang <gaoxiang25@huawei.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of the Linux
 * distribution for more details.
 */
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/kdebug.h>
#include "iocache.h"

#ifdef CONFIG_MIGRATION
#include <linux/migrate.h>

int iocache_migrate_page_move_mapping(struct address_space *mapping,
	struct page *newpage, struct page *page, enum migrate_mode mode)
{
	int dirty;
	struct iocache_device *ioch =
			container_of(mapping, struct iocache_device, mapping);
	void **pslot;
	struct iocache_transaction *trans;
	struct page *old;
	struct iocache_encrypt_key *encrypt_key;

	if (!page->private) {
		pr_info("migrate: empty private!\n");
		return -EINVAL;
	}

	if (!iocache_page_key(page)->trans) {
		pr_info("migrate: page drained!\n");
		return -EINVAL;
	}

	if (unlikely(page_count(page) > 3)) {
		pr_info("migrate: page kmap!\n");
		return -EINVAL;
	}

	set_page_private(newpage, page_private(page));
	newpage->index = page->index;
	newpage->mapping = page->mapping;
	__ClearPageMovable(page);

	if (PageSwapBacked(page)) {
		__SetPageSwapBacked(newpage);
		if (PageSwapCache(page))
			SetPageSwapCache(newpage);
	} else {
		VM_BUG_ON_PAGE(PageSwapCache(page), page);
	}

	dirty = PageDirty(page);
	if (dirty) {
		ClearPageDirty(page);
		SetPageDirty(newpage);
	}

	pslot = radix_tree_lookup_slot(&ioch->latest_space, page->index);
	if (pslot) {
		old = radix_tree_deref_slot(pslot);
		if (old == page) {
			radix_tree_replace_slot(&ioch->latest_space,
				pslot, newpage);
			put_page(page);
			get_page(newpage);
		}
	} else {
		pr_info("migrate: empty pslot from latest\n");
	}

	encrypt_key = (struct iocache_encrypt_key *)page_private(page);
	trans = encrypt_key->trans;
	BUG_ON(!trans);

	pslot = radix_tree_lookup_slot(&trans->space, page->index);
	if (pslot) {
		old = radix_tree_deref_slot(pslot);
		if (old == page) {
			radix_tree_replace_slot(&trans->space,
				pslot, newpage);
			put_page(page);
			get_page(newpage);
		} else {
			pr_info("migrate: page mismatch with trans\n");
		}
	} else {
		pr_info("migrate: empty pslot from trans\n");
	}

	return MIGRATEPAGE_SUCCESS;
}

int iocache_migrate_page(struct address_space *mapping,
	struct page *newpage, struct page *page, enum migrate_mode mode)
{
	int rc;
	struct iocache_device *ioch =
			container_of(mapping, struct iocache_device, mapping);
	unsigned long flags;

	BUG_ON(PageWriteback(page));    /* Writeback must be complete */

	down_write(&ioch->migrate_lock);
	spin_lock_irqsave(&ioch->update_lock, flags);
	rc = iocache_migrate_page_move_mapping(mapping, newpage, page, mode);

	if (rc == MIGRATEPAGE_SUCCESS)
		migrate_page_copy(newpage, page);

	spin_unlock_irqrestore(&ioch->update_lock, flags);
	up_write(&ioch->migrate_lock);
	return rc;
}

bool iocache_page_isolate(struct page *page, isolate_mode_t mode)
{
	return true;
}

void iocache_page_putback(struct page *page)
{
}

const struct address_space_operations iocache_aops = {
	.migratepage = iocache_migrate_page,
	.isolate_page = iocache_page_isolate,
	.putback_page = iocache_page_putback,
};
#endif

void iocache_migrate_lock(struct iocache_device *ioch)
{
#ifdef CONFIG_MIGRATION
	down_read(&ioch->migrate_lock);
#endif
}

void iocache_migrate_unlock(struct iocache_device *ioch)
{
#ifdef CONFIG_MIGRATION
	up_read(&ioch->migrate_lock);
#endif
}

/* iocache wait for FG IO */
bool fg_in_flight(struct iocache_device *const ioch)
{
	struct request_queue *rq = ioch->rbd->bd_disk->queue;

	if (likely(rq))
		return rq->in_flight[3] ? true : false;

	return false;
}

static void calc_limits(struct iocache_device *ioch)
{
	if (!fg_in_flight(ioch))
		ioch->iotc_curr = IOCACHE_MAX_DEPTH;
	else
		ioch->iotc_curr = IOCACHE_FG_DEPTH;
}

void iocache_update_limits(struct iocache_device *ioch)
{
	calc_limits(ioch);

	if (waitqueue_active(&ioch->wait))
		wake_up_all(&ioch->wait);
}

static inline void re_arm_timer(struct iocache_device *ioch)
{
	unsigned long expires;

	expires = jiffies + msecs_to_jiffies(10);
	mod_timer(&ioch->window_timer, expires);
}

static void _iocache_wait(struct iocache_device *ioch)
{
	DEFINE_WAIT(wait);
	int cycle_th = 0;

	calc_limits(ioch);
	if (atomic_read(&ioch->iocontrol_write_ios) < ioch->iotc_curr)
		return;

	do {
		cycle_th++;

		prepare_to_wait_exclusive(&ioch->wait, &wait,
					TASK_UNINTERRUPTIBLE);

		calc_limits(ioch);
		if (atomic_read(&ioch->iocontrol_write_ios) < ioch->iotc_curr)
			break;

		io_schedule();
	} while (1);

	finish_wait(&ioch->wait, &wait);
}

void iocache_wait(struct iocache_device *ioch)
{
	_iocache_wait(ioch);
}

static void iocache_timer_fn(unsigned long data)
{
	struct iocache_device *ioch = (struct iocache_device *)data;

	calc_limits(ioch);

	if (waitqueue_active(&ioch->wait))
		wake_up_all(&ioch->wait);

	re_arm_timer(ioch);
}

void iocache_iocontrol_init(struct iocache_device *ioch)
{
	init_waitqueue_head(&ioch->wait);
	iocache_update_limits(ioch);
}

void iocache_iocontrol_exit(struct iocache_device *ioch)
{
}

