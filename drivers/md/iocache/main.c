// SPDX-License-Identifier: GPL-2.0
/*
 * linux/drivers/md/iocache/main.c
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
#include <linux/workqueue.h>
#include "iocache.h"


#define kobj_attribute_rw(n, fnr, fnw) \
	static struct kobj_attribute ksysfs_##n = __ATTR(n, 0664, fnr, fnw)
#define kobj_attribute_write(n, fn) \
	static struct kobj_attribute ksysfs_##n = __ATTR(n, 0200, NULL, fn)

#define IOCACHE_PAGE_HARD_LIMIT (64 * 1024 / 4)
#define IOCACHE_MIN_PAGE_HARD_LIMIT (4 * 1024 / 4)

static struct iocache_device ioch_devices[IOCACHE_MAX_DEVICES];
static int iocache_major;

static int max_part = 1;
module_param(max_part, int, 0444);
MODULE_PARM_DESC(max_part, "Num Minors to reserve between devices");

static const struct block_device_operations iocache_fops = {
	.owner =		THIS_MODULE,
};

static struct shrinker iocache_shrinker_info = {
	.scan_objects = iocache_shrink_scan,
	.count_objects = iocache_shrink_count,
	.seeks = DEFAULT_SEEKS,
	.batch = 8192,
};

/* get first valid iocache dev for now only one is avaliable */
static struct iocache_device *get_iocache_device(void)
{
	struct iocache_device *ioch = NULL;
	int i;

	for (i = 0; i < IOCACHE_MAX_DEVICES; ++i) {
		if (ioch_devices[i].queue) {
			ioch = ioch_devices + i;
			break;
		}
	}

	return ioch;
}

int iocache_do_drains(bool reachlimit, bool senddiscard, bool try)
{
	struct iocache_device *ioch = NULL;
	int ret;

	ioch = get_iocache_device();
	if (!ioch)
		return -ERANGE;
	if (IS_ERR_OR_NULL(ioch->rbd))
		return -EFAULT;

	ret = drain_transactions(ioch, UINT_MAX, reachlimit, senddiscard, try);

	return ret;
}
EXPORT_SYMBOL(iocache_do_drains);

struct delayed_work drain_work;

void iocache_drain_no_wait(struct work_struct *work)
{
#ifdef IOCACHE_DEBUG
	pr_info("iocache drain no wait running\n");
#endif
	return iocache_do_drains(true, true, true);
}

static int iocache_schedule_delayed_work(struct delayed_work *work,
	unsigned long delay)
{
	int ret = 0;
	/*
	 * We use the system_freezable_wq, because of two reasons.
	 * First, it allows several works (not the same work item) to be
	 * executed simultaneously. Second, the queue becomes frozen when
	 * userspace becomes frozen during system PM.
	 */
#ifdef IOCACHE_DEBUG
	pr_info("prepare to insert the drain to the queue\n");
#endif
	ret = queue_delayed_work(system_freezable_wq, work, delay);
#ifdef IOCACHE_DEBUG
	pr_info("insert the drain to the queue success\n");
#endif
	return ret;
}

int iocache_drains_notify(struct notifier_block *self,
	unsigned long event, void *data)
{
	return iocache_schedule_delayed_work(&drain_work, 0);
}
EXPORT_SYMBOL(iocache_drains_notify);

#define INIT_NOTIFIER(name)				\
	static struct notifier_block on_##name##_nb = { \
		.notifier_call = iocache_drains_notify,	\
	}

INIT_NOTIFIER(reboot);
INIT_NOTIFIER(panic);
INIT_NOTIFIER(die);

static int iocache_alloc(struct iocache_device *ioch)
{
	const unsigned int i = ioch - ioch_devices;
	struct request_queue *q;
	struct gendisk *disk;

	q = blk_alloc_queue(GFP_KERNEL);
	if (!q)
		return -ENOMEM;

	blk_queue_make_request(q, iocache_make_request);
	blk_queue_max_hw_sectors(q, 1024);

	/*
	 * This is so fdisk will align partitions on 4k, because of
	 * direct_access API needing 4k alignment, returning a PFN
	 * (This is only a problem on very small devices <= 4M,
	 *  otherwise fdisk will align on 1M. Regardless this call
	 *  is harmless)
	 */
	blk_queue_physical_block_size(q, PAGE_SIZE);
	blk_queue_logical_block_size(q, PAGE_SIZE);

	blk_queue_io_min(q, PAGE_SIZE);
	blk_queue_io_opt(q, PAGE_SIZE);
	q->limits.discard_granularity = PAGE_SIZE;
	blk_queue_max_discard_sectors(q, UINT_MAX);
	queue_flag_set_unlocked(QUEUE_FLAG_INLINECRYPT, q);

	disk = alloc_disk(max_part);
	if (!disk)
		goto err_free_queue;

	/* init iocache io control */
	iocache_iocontrol_init(ioch);

	disk->major		= iocache_major;
	disk->first_minor	= i * max_part;
	disk->fops		= &iocache_fops;
	disk->private_data	= ioch;
	disk->flags		= GENHD_FL_EXT_DEVT;
	disk->queue		= q;
	sprintf(disk->disk_name, "iocache%d", i);
	set_capacity(disk, ioch->rbd->bd_part->nr_sects);

	/* Tell the block layer that this is not a rotational device */
	set_bit(QUEUE_FLAG_NONROT, &q->queue_flags);
	set_bit(QUEUE_FLAG_ADD_RANDOM, &q->queue_flags);
	set_bit(QUEUE_FLAG_DISCARD, &q->queue_flags);
	blk_queue_write_cache(q, true, true);
	add_disk(disk);

	ioch->queue = q;
	ioch->disk = disk;

#ifdef CONFIG_MIGRATION
	address_space_init_once(&ioch->mapping);
	ioch->mapping.a_ops = &iocache_aops;
	init_rwsem(&ioch->migrate_lock);
#endif
	return 0;
err_free_queue:
	blk_cleanup_queue(q);
	return -ENOMEM;
}

static void clean_iocache_queue_disk(struct iocache_device *ioch)
{
	blk_cleanup_queue(ioch->queue);
	if (ioch->disk) {
		del_gendisk(ioch->disk);
		put_disk(ioch->disk);
	}
}

static ssize_t iocache_device_up(struct kobject *k, struct kobj_attribute *attr,
	const char *buffer, size_t size)
{
	int i, err;
	struct iocache_device *ioch;
	char *path;
	struct iocache_transaction *trans;
	struct task_struct *th;

	/* try to grab a module reference to avoid freed */
	if (!try_module_get(THIS_MODULE))
		return -EBUSY;

	ioch = NULL;
	for (i = 0; i < IOCACHE_MAX_DEVICES; ++i) {
		if (!ioch_devices[i].queue) {
			ioch = ioch_devices + i;
			break;
		}
	}

	for (i = 0; i < IOCACHE_STAT_SIZE; ++i)
		ioch->capacity_stat[i] = 0;

	if (!ioch)
		return -ERANGE;

	path = kstrndup(buffer, size, GFP_KERNEL);
	if (!path)
		return -ENOMEM;

	ioch->rbd = blkdev_get_by_path(strim(path),
		FMODE_READ | FMODE_WRITE | FMODE_EXCL,
		ioch);
	if (IS_ERR(ioch->rbd))
		goto err_free;

	atomic_set(&ioch->inflight_write_ios, 0);
	atomic_set(&ioch->inflight_discard, 0);
	atomic_set(&ioch->iocontrol_write_ios, 0);
	atomic_set(&ioch->w_count_fg, 0);

	init_rwsem(&ioch->switch_rwsem);
	ioch->ioch_switch = 1;

	err = iocache_alloc(ioch);
	if (err)
		goto err_blkput;

	mutex_init(&ioch->drain_lock);
	mutex_init(&ioch->no_mem_lock);
	init_rwsem(&ioch->commit_rwsem);

	spin_lock_init(&ioch->update_lock);
	INIT_RADIX_TREE(&ioch->latest_space, GFP_ATOMIC);

	INIT_LIST_HEAD(&ioch->transactions);

	INIT_RADIX_TREE(&ioch->erasemap, GFP_ATOMIC);
	spin_lock_init(&ioch->erase_lock);

	init_waitqueue_head(&ioch->erase_wq);
	init_waitqueue_head(&ioch->inflight_write_wait);
	init_waitqueue_head(&ioch->alloc_pages_wait);
	init_waitqueue_head(&ioch->no_mem_wait);
	init_waitqueue_head(&ioch->drain_trans_wait);

	/* for async drain */
	ioch->need_drain = false;
	ioch->no_mem = false;

	trans = iocache_new_transaction(GFP_KERNEL);
	if (IS_ERR(trans))
		goto err_ioch;
	list_add(&trans->list, &ioch->transactions);

	ioch->encrypt_key_cache = kmem_cache_create("iocache_key_cache",
		sizeof(struct iocache_encrypt_key),
		1, 0, NULL);
	if (!ioch->encrypt_key_cache)
		goto err_blkput;

	atomic_set(&ioch->cur_key_cnt, 0);

	ioch->busy_flush_interval = 1;
	ioch->threshold[0] = IOCACHE_BUSY_FLUSH_INTERVAL;
	ioch->threshold[1] = IOCACHE_IDLE_FLUSH_INTERVAL;

	th = kthread_run(iocache_flush_thread_func, ioch,
		"iocache-%u:%u", ioch->disk->major,
		ioch->disk->first_minor);
	if (IS_ERR(th))
		goto err_trans;
	ioch->thread = th;

	INIT_LIST_HEAD(&ioch->list);
	iocache_join_shrinker(ioch);
	atomic_set(&ioch->tot_uppages, 0);
	atomic_set(&ioch->iocache_page_hard_limit, IOCACHE_PAGE_HARD_LIMIT);

	register_reboot_notifier(&on_reboot_nb);
	register_die_notifier(&on_die_nb);
	atomic_notifier_chain_register(&panic_notifier_list, &on_panic_nb);

	INIT_DELAYED_WORK(&drain_work, iocache_drain_no_wait);

	kfree(path);
	return (ssize_t)size;

err_trans:
	kfree(trans);
err_ioch:
	iocache_iocontrol_exit(ioch);
	clean_iocache_queue_disk(ioch);
err_blkput:
	blkdev_put(ioch->rbd, FMODE_READ | FMODE_WRITE | FMODE_EXCL);
	ioch->queue = NULL;
err_free:
	kfree(path);
	return -ENOMEM;
}

kobj_attribute_write(register, iocache_device_up);

static ssize_t iocache_page_limit_read(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return sprintf(buf, "%s, total: %ld * 4K(>=16M), used: %ld * 4K\n",
			attr->attr.name,
			atomic_read(&ioch_devices->iocache_page_hard_limit),
			atomic_read(&ioch_devices->tot_uppages));
}

unsigned int iocache_get_used_pages(void)
{
	return atomic_read(&ioch_devices->tot_uppages);
}

static ssize_t iocache_page_limit_write(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t n)
{
	int tmp = 0;

	if (kstrtoint(buf, 10, &tmp))
		return -EINVAL;

	if (tmp >= IOCACHE_MIN_PAGE_HARD_LIMIT) {
		atomic_set(&ioch_devices->iocache_page_hard_limit, tmp);
		return (ssize_t)n;
	}
	return -EINVAL;
}

kobj_attribute_rw(iocache_page_limit,
	iocache_page_limit_read,
	iocache_page_limit_write);

static ssize_t iocache_flush_interval_read(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return sprintf(buf, "%ld\n", ioch_devices->busy_flush_interval);
}

static ssize_t iocache_flush_interval_write(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t n)
{
	int tmp = 0;

	if (kstrtoint(buf, 10, &tmp))
		return -EINVAL;

	if (tmp > 0 && tmp <= 30) {
		ioch_devices->busy_flush_interval = tmp;
		ioch_devices->threshold[0] = tmp * HZ;
		return (ssize_t)n;
	}
	return -EINVAL;
}

kobj_attribute_rw(iocache_flush_interval,
	iocache_flush_interval_read,
	iocache_flush_interval_write);

static ssize_t iocache_func_read(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return sprintf(buf, "%s: %d\n",
			attr->attr.name,
			ioch_devices->ioch_switch);
}

static ssize_t iocache_func_write(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t n)
{
	int tmp = 0;

	if (kstrtoint(buf, 10, &tmp))
		return -EINVAL;

	if (tmp == 1) {
		down_write(&ioch_devices->switch_rwsem);
		ioch_devices->ioch_switch = 1;
		up_write(&ioch_devices->switch_rwsem);
		return (ssize_t)n;
	} else if (tmp == 0 && ioch_devices->ioch_switch == 1) {
		down_write(&ioch_devices->switch_rwsem);
		ioch_devices->ioch_switch = 0;
		drain_transactions(ioch_devices, UINT_MAX, true, true, false);
		up_write(&ioch_devices->switch_rwsem);
		return (ssize_t)n;
	}
	return -EINVAL;
}

kobj_attribute_rw(iocache_func_switch,
	iocache_func_read,
	iocache_func_write);

static ssize_t iocache_interval_stat_read(struct kobject *kobj,
						struct kobj_attribute *attr,
						char *buf)
{
	int i;
	int len = 0;

	down_read(&ioch_devices->switch_rwsem);
	len += sprintf(buf + len, "iocache_capacity_stat: %lu",
		ioch_devices->capacity_stat[0]);
	for (i = 1; i < IOCACHE_STAT_SIZE; ++i)
		len += sprintf(buf + len, ", %lu",
			ioch_devices->capacity_stat[i]);

	up_read(&ioch_devices->switch_rwsem);
	len += sprintf(buf + len, "\n");
	return len;
}

static ssize_t iocache_interval_stat_clear(struct kobject *kobj,
						struct kobj_attribute *attr,
						const char *buf,
						size_t n)
{
	int i;
	down_read(&ioch_devices->switch_rwsem);
	for (i = 0; i < IOCACHE_STAT_SIZE; ++i)
		ioch_devices->capacity_stat[i] = 0;
	up_read(&ioch_devices->switch_rwsem);
	return (ssize_t)n;
}

kobj_attribute_rw(iocache_interval_stat,
				iocache_interval_stat_read,
				iocache_interval_stat_clear);
static struct kobject *iocache_kobj;

static const struct attribute *iocache_attrs[] = {
	&ksysfs_register.attr,
	&ksysfs_iocache_page_limit.attr,
	&ksysfs_iocache_flush_interval.attr,
	&ksysfs_iocache_func_switch.attr,
	&ksysfs_iocache_interval_stat.attr,
	NULL,
};

static int __init iocache_init(void)
{
	if (strnstr(saved_command_line, "androidboot.iocache=disable",
		strlen(saved_command_line))) {
		pr_info("iocache is diabled in cmdline, skip it\n");
		return 0;
	}

	iocache_major = register_blkdev(0, "iocache");
	if (iocache_major < 0)
		return iocache_major;

	iocache_kobj = kobject_create_and_add("iocache", fs_kobj);
	if (!iocache_kobj)
		goto err_iocache_kobj;

	if (sysfs_create_files(iocache_kobj, iocache_attrs))
		goto err_sysfs_create_files;

	if (register_shrinker(&iocache_shrinker_info))
		goto err_shrinker;

	pr_info("module loaded\n");
	return 0;

err_shrinker:
	sysfs_remove_files(iocache_kobj, iocache_attrs);
err_sysfs_create_files:
	kobject_put(iocache_kobj);
err_iocache_kobj:
	unregister_blkdev(iocache_major, "iocache");
	pr_info("module NOT loaded\n");
	return -ENOMEM;
}

static void iocache_exit(void)
{
	struct iocache_device *ioch = NULL;

	if (iocache_major < 0)
		return;
	if (!iocache_kobj) {
		unregister_blkdev(iocache_major, "iocache");
		return;
	}

	ioch = get_iocache_device();
	if (!ioch)
		goto out;

	unregister_reboot_notifier(&on_reboot_nb);
	unregister_die_notifier(&on_die_nb);
	atomic_notifier_chain_unregister(&panic_notifier_list, &on_panic_nb);

	iocache_leave_shrinker(ioch);
	iocache_iocontrol_exit(ioch);
	clean_iocache_queue_disk(ioch);
	if (ioch->rbd)
		blkdev_put(ioch->rbd, FMODE_READ | FMODE_WRITE | FMODE_EXCL);

out:
	unregister_shrinker(&iocache_shrinker_info);
	sysfs_remove_files(iocache_kobj, iocache_attrs);
	kobject_put(iocache_kobj);
	unregister_blkdev(iocache_major, "iocache");
}

module_exit(iocache_exit);
module_init(iocache_init);

MODULE_DESCRIPTION("IOcache: a Linux block layer cache");
MODULE_AUTHOR("Gao Xiang <gaoxiang25@huawei.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("iocache");

