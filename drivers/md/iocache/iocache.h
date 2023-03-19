/* SPDX-License-Identifier: GPL-2.0 */
/*
 * linux/drivers/md/iocache/iocache.h
 *
 * Copyright (C) 2019 HUAWEI, Inc.
 *             http://www.huawei.com/
 * Created by Gao Xiang <gaoxiang25@huawei.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of the Linux
 * distribution for more details.
 */
#ifndef __IOCACHE_H
#define __IOCACHE_H

#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/backing-dev.h>
#include <linux/kthread.h>
#include <linux/wait.h>

#ifdef CONFIG_HUAWEI_IOCACHE_DSM
#include "dsm_iocache.h"
#endif

#define IOCACHE_MAX_DEVICES	1
#define SECTORS_PER_PAGE_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define SECTORS_PER_PAGE	(1 << SECTORS_PER_PAGE_SHIFT)


#define IOC_DISCARD_MARKED	((void *)-4)
#define IOC_DISCARD_ISSUED	((void *)-8)

/* 4M dicard 1024*4K pages */
#define INFLIGHT_DISCARD_MAX_PAGES 1024

#ifndef SECTOR_SHIFT
#define SECTOR_SHIFT 9
#endif

#define IOCACHE_MAX_DEPTH 4
#define IOCACHE_FG_DEPTH 1
#define IOCACHE_VIP_DEPTH 1

#define IOCACHE_IDLE_FLUSH_INTERVAL     (HZ / 15)
#define IOCACHE_BUSY_FLUSH_INTERVAL     (HZ)

#define IOCACHE_DRAIN_TIME  500
#define IOCACHE_DRAIN_WRITE_TIME  100
#define IOCACHE_DRAIN_FLUSH_TIME  20
#define IOCACHE_DRAIN_LASTTANS_TIME 40
#define IOCACHE_MAKE_REQUEST_TIME 50
#define IOCACHE_STAT_SIZE 128
/* msecs */
#define IOCACHE_REGISTER_TIMEOUT 3000
#define IOCACHE_DEBUG



struct iocache_device {
	struct list_head	transactions;
	struct mutex		drain_lock;
	struct mutex		no_mem_lock;
	struct rw_semaphore	commit_rwsem;
	struct rw_semaphore	switch_rwsem;

	spinlock_t		update_lock;
	struct radix_tree_root  latest_space;

	struct request_queue	*queue;
	struct gendisk		*disk;
	struct block_device	*rbd;	/* read block device */

	/* TODO: introduce sparse bitmap to take place of it */
	struct radix_tree_root	erasemap;
	/* serialize discard node modifications */
	spinlock_t		erase_lock;
	wait_queue_head_t	erase_wq;

	atomic_t		inflight_write_ios;
	atomic_t		inflight_discard;
	wait_queue_head_t	inflight_write_wait;

	wait_queue_head_t	alloc_pages_wait;

	wait_queue_head_t	no_mem_wait;
	struct task_struct	*thread;

	struct list_head	list;
	unsigned int		shrinker_run_no;
	atomic_t		tot_uppages;
	struct kmem_cache *encrypt_key_cache;
	atomic_t		cur_key_cnt;
	atomic_t		iocache_page_hard_limit;

	int iotc_curr;
	struct timer_list window_timer;
	wait_queue_head_t wait;
	atomic_t		iocontrol_write_ios;

	atomic_t		flushmerge_waiters;
	atomic_t		flushmerge_no;

	atomic_t		w_count_fg;
	/* for async drain */
	wait_queue_head_t	drain_trans_wait;
	bool			need_drain;
	bool			no_mem;
#ifdef CONFIG_MIGRATION
	struct address_space	mapping;
	struct rw_semaphore	migrate_lock;
#endif
	int				ioch_switch;

	/* for flush sysfs */
	unsigned int busy_flush_interval;
	unsigned int threshold[2];
	/* for stat */
	unsigned long capacity_stat[IOCACHE_STAT_SIZE];
};

struct iocache_transaction {
	struct list_head	list;

	struct radix_tree_root	space;

	unsigned long		start_time;
	unsigned long		commit_time;
	atomic_t		nr_pages;
};

blk_qc_t iocache_make_request(struct request_queue *q,
			      struct bio *bio);
struct iocache_transaction *iocache_new_transaction(gfp_t gfp);
int iocache_flush_thread_func(void *data);
int drain_transactions(struct iocache_device *const ioch,
	unsigned int nr, bool reclaim,
	bool senddiscard, bool trydrain);

/* shrinker.c */
unsigned long iocache_shrink_count(struct shrinker *shrink,
	struct shrink_control *sc);
unsigned long iocache_shrink_scan(struct shrinker *shrink,
	struct shrink_control *sc);
void iocache_join_shrinker(struct iocache_device *ioch);
void iocache_leave_shrinker(struct iocache_device *ioch);
/* util.c */
void iocache_wait(struct iocache_device *ioch);
void iocache_iocontrol_init(struct iocache_device *ioch);
void iocache_iocontrol_exit(struct iocache_device *ioch);

extern const struct address_space_operations iocache_aops;
void iocache_migrate_lock(struct iocache_device *ioch);
void iocache_migrate_unlock(struct iocache_device *ioch);
/* hcrypt.c */
struct iocache_encrypt_key *alloc_encrypt_key(
	struct iocache_device *const ioch);
void free_encrypt_key(struct iocache_device *const ioch,
	struct iocache_encrypt_key *encrypt_key);
void free_page_key(struct iocache_device *const ioch, struct page *page);
void dup_bio_key_2_page(struct bio *bio, struct page *page);
void save_org_page_idx(struct page *new_page, struct page *org_page);
void clean_page_key(struct page *page);
int page_contain_key(struct page *page);
int key_same(struct bio_crypt_ctx *key1, struct bio_crypt_ctx *key2);
int page_key_same(struct page *src_page, struct page *dst_page);
int page_bcf_test(struct page *page, unsigned int flag);
void dup_key_from_bio(struct bio *dst, const struct bio *src);
void dup_key_from_page(struct bio *dst, struct page *page);

/* for io control */
bool fg_in_flight(struct iocache_device *ioch);
unsigned int iocache_get_used_pages(void);
#endif

