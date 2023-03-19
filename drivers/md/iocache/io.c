// SPDX-License-Identifier: GPL-2.0
/*
 * linux/drivers/md/iocache/io.c
 *
 * Copyright (C) 2019 HUAWEI, Inc.
 *             http://www.huawei.com/
 * Created by Gao Xiang <gaoxiang25@huawei.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of the Linux
 * distribution for more details.
 */
#include "iocache.h"
#include <linux/delay.h>
#include <uapi/linux/sched/types.h>

#ifdef CONFIG_HUAWEI_IO_TRACING
#include <iotrace/iotrace.h>
DEFINE_TRACE(iocache_drain_last_trans_enter);
DEFINE_TRACE(iocache_drain_last_trans_end);
#endif

struct passthru_rd_info {
	int err;
	atomic_t bio_remaining;
	struct bio *orig_bio;
};

static void update_position(pgoff_t *index, unsigned int *offset,
	struct bio_vec *bvec)
{
	*index += (*offset + bvec->bv_len) / PAGE_SIZE;
	*offset = (*offset + bvec->bv_len) % PAGE_SIZE;
}

static void passthru_rd_kickoff(struct passthru_rd_info *rdi, int bios)
{
	struct bio *bio;

	if (atomic_add_return(bios, &rdi->bio_remaining))
		return;

	bio = rdi->orig_bio;
	if (rdi->err)
		bio_io_error(bio);
	kfree(rdi);
	bio_endio(bio);
}

static void passthru_rd_endio(struct bio *bio)
{
	const blk_status_t err = bio->bi_status;
	struct passthru_rd_info *const rdi = bio->bi_private;

	if (err)
		rdi->err = -EIO;
	passthru_rd_kickoff(rdi, -1);
	bio_put(bio);
}

static inline void __submit_bio(struct bio *bio,
	unsigned int op, unsigned int op_flags,
	struct iocache_device *const ioch)
{
	bio_set_op_attrs(bio, op, op_flags);

	if (!atomic_read(&ioch->w_count_fg) && (current->flags & PF_KTHREAD) &&
		!(op_flags & REQ_FG))
		iocache_wait(ioch);

	submit_bio(bio);
}

static int do_read_bio(struct iocache_device *const ioch,
	unsigned int op, struct bio *bio,
	pgoff_t index, unsigned int offset,
	bool *need_endio)
{
	unsigned int nrsegs = bio_segments(bio);
	struct bio *rbio = NULL;
	struct passthru_rd_info *rdi;
	struct bio_vec bvec;
	struct bvec_iter iter;
	unsigned int nrbios;
	int err;
	unsigned char sdio_bio = 0;
	unsigned long sdio_iv;

	if (bio_encrypted(bio) && bio_bcf_test(bio, BC_IV_CTX)) {
		sdio_iv = bio_bc_iv_get(bio);
		sdio_bio = 1;
	}

	rdi = kzalloc(sizeof(*rdi), GFP_NOIO);
	if (!rdi) {
#ifdef CONFIG_HUAWEI_IOCACHE_DSM
		DSM_IOCACHE_LOG(DSM_IOCACHE_ERROR,
			"do read bio kzalloc failed\n");
#endif
		return -ENOMEM;
	}

	rdi->err = 0;
	rdi->orig_bio = bio;

	err = 0;
	nrbios = 0;
	bio_for_each_segment(bvec, bio, iter) {
		struct page *const to = bvec.bv_page;
		struct page *src;

		iocache_migrate_lock(ioch);
		rcu_read_lock();
		src = radix_tree_lookup(&ioch->latest_space, index);

		if (src && src != IOC_DISCARD_MARKED) {
			void *const kto = kmap_atomic(to);
			void *const ksrc = kmap_atomic(src);

			memcpy(kto + bvec.bv_offset, ksrc, bvec.bv_len);
			kunmap_atomic(ksrc);
			kunmap_atomic(kto);
			flush_dcache_page(to);
			rcu_read_unlock();
			iocache_migrate_unlock(ioch);

			if (rbio) {
				__submit_bio(rbio, REQ_OP_READ,
					bio->bi_opf, ioch);
				rbio = NULL;
			}
		} else {
			rcu_read_unlock();
			iocache_migrate_unlock(ioch);
			if (!rbio) {
				unsigned int maxpages;

repeat:
				maxpages = min_t(uint, nrsegs, BIO_MAX_PAGES);
				rbio = bio_alloc(GFP_NOIO, maxpages);


				rbio->bi_opf = bio->bi_opf;
				bio_set_dev(rbio, ioch->rbd);
				rbio->bi_iter.bi_sector =
					index << SECTORS_PER_PAGE_SHIFT;
				rbio->bi_end_io = passthru_rd_endio;
				rbio->bi_private = rdi;
				dup_key_from_bio(rbio, bio);
				if (sdio_bio)
					rbio->bi_crypt_ctx.bc_iv = sdio_iv;

				++nrbios;
			}

			if (bio_add_page(rbio, to, bvec.bv_len,
					 bvec.bv_offset) < bvec.bv_len) {
				__submit_bio(rbio, REQ_OP_READ,
					bio->bi_opf, ioch);
				goto repeat;
			}
		}
		--nrsegs;
		update_position(&index, &offset, &bvec);

		if (sdio_bio)
			++sdio_iv;
	}

	if (!nrbios) {
		kfree(rdi);
		*need_endio = true;
		return 0;
	}

	if (rbio)
		__submit_bio(rbio, REQ_OP_READ, bio->bi_opf, ioch);
err:
	if (err)
		rdi->err = err;
	passthru_rd_kickoff(rdi, nrbios);
	return err;
}

/* commit_lock should be held */
static struct iocache_transaction *
latest_transaction(struct iocache_device *const ioch)
{
	return list_last_entry(&ioch->transactions,
		struct iocache_transaction, list);
}

static void hook_page(struct iocache_device *const ioch,
	pgoff_t index, struct page *page)
{
	bool locked = false;
	void *pslot;
	struct page *old;
	unsigned long flags;

golock:
	pslot = radix_tree_lookup_slot(&ioch->latest_space, index);
	BUG_ON(!pslot);

	old = radix_tree_deref_slot(pslot);

	if (radix_tree_exception(old)) {
		if (radix_tree_deref_retry(old))
			goto golock;
		BUG();
	} else if (old == page) {
		goto out;
	}
	else if (!locked) {
		locked = true;
		spin_lock_irqsave(&ioch->update_lock, flags);
		goto golock;
	} else {
		get_page(page);
		radix_tree_replace_slot(&ioch->latest_space,
			pslot, page);
		if (old != IOC_DISCARD_MARKED) {
			struct iocache_encrypt_key *old_encrypt_key =
				(struct iocache_encrypt_key *)
				page_private(old);
			struct iocache_encrypt_key *page_encrypt_key =
				(struct iocache_encrypt_key *)
				page_private(page);
			struct iocache_transaction *old_trans;
			struct iocache_transaction *page_trans;

			pr_info("hook page: page=%px, old=%px, page_encrypt_key=%px, old_encrypt_key=%px, index=%lu\n",
				page, old, page_encrypt_key,
				old_encrypt_key, index);
			old_trans = old_encrypt_key->trans;
			page_trans = page_encrypt_key->trans;
			pr_info("hook page: page_trans=%px, old_trans=%px\n",
				page_trans, old_trans);
#ifdef CONFIG_HUAWEI_IOCACHE_DSM
			DSM_IOCACHE_LOG(DSM_IOCACHE_ERROR,
				"iocache: hook page(old != DISCARD)\n");
#endif
			BUG();
		}
	}

out:
	if (locked)
		spin_unlock_irqrestore(&ioch->update_lock, flags);
}

#ifdef CONFIG_MIGRATION
#include <linux/migrate.h>
#endif
static struct page *register_page(struct iocache_device *const ioch,
	pgoff_t index, bool *locked,
	struct iocache_transaction **trans)
{
	void *pslot;
	struct page *page, *old;
	unsigned long flags;
	unsigned int retries = 3;
	struct iocache_encrypt_key *encrypt_key = NULL;
	int ret;
	int release_flag = 0;
	iocache_migrate_unlock(ioch);
	if (atomic_read(&ioch->tot_uppages) >=
	atomic_read(&ioch->iocache_page_hard_limit)) {
		up_read(&ioch->commit_rwsem);
		release_flag = 1;
		wait_event_interruptible(ioch->alloc_pages_wait,
			atomic_read(&ioch->tot_uppages) <=
			atomic_read(&ioch->iocache_page_hard_limit) - 1);
	}

	encrypt_key = alloc_encrypt_key(ioch);
	if (!encrypt_key) {
		if (release_flag)
			down_read(&ioch->commit_rwsem);
		iocache_migrate_lock(ioch);
		pr_err("iocache error no mem for encrypt key");
		return ERR_PTR(-ENOMEM);
	}

	do {
		page = alloc_pages(GFP_NOWAIT | __GFP_NORETRY | __GFP_NOWARN,
			0);
		if (page) {
			break;
		} else if (!release_flag) {
				release_flag = 1;
				up_read(&ioch->commit_rwsem);
			}
		congestion_wait(BLK_RW_ASYNC, HZ/50);
		cond_resched();
	} while (--retries);

	if (!page) {
		page = alloc_pages(GFP_NOIO, 0);
		if (!page) {
			free_encrypt_key(ioch, encrypt_key);
			if (release_flag)
				down_read(&ioch->commit_rwsem);
			iocache_migrate_lock(ioch);
			return ERR_PTR(-ENOMEM);
		}
	}
	if (release_flag)
		down_read(&ioch->commit_rwsem);
	iocache_migrate_lock(ioch);
	*trans = latest_transaction(ioch);
	spin_lock_irqsave(&ioch->update_lock, flags);
	page->index = index;
#ifdef CONFIG_MIGRATION
	__SetPageMovable(page, &ioch->mapping);
#endif
	get_page(page);
	page->private = (unsigned long)encrypt_key;

	ret = radix_tree_insert(&((*trans)->space), index, page);
	if (!ret) {
#ifdef CONFIG_MIGRATION
		encrypt_key->trans = *trans;
#endif
	} else {
		spin_unlock_irqrestore(&ioch->update_lock, flags);
		free_encrypt_key(ioch, encrypt_key);
		page->private = 0;
		put_page(page);
		put_page(page);
		return ERR_PTR(-EAGAIN);
	}
	atomic_inc(&((*trans)->nr_pages));
	atomic_inc(&ioch->tot_uppages);
	if (atomic_read(&ioch->tot_uppages) >=
		80 * atomic_read(&ioch->iocache_page_hard_limit) / 100) {
		ioch->need_drain = true;
		wake_up_interruptible_all(&ioch->drain_trans_wait);
	}

	/* update latest space as well */
	get_page(page);

	pslot = radix_tree_lookup_slot(&ioch->latest_space, index);
	if (!pslot) {
		if (radix_tree_insert(&ioch->latest_space, index, page))
			BUG();
		goto out;
	}

repeat:
	old = radix_tree_deref_slot(pslot);

	if (radix_tree_exception(old)) {
		if (radix_tree_deref_retry(old))
			goto repeat;
		BUG();
	} else {
		radix_tree_replace_slot(&ioch->latest_space,
			pslot, page);
		if (old && old != IOC_DISCARD_MARKED)
			put_page(old);
	}
out:
	spin_unlock_irqrestore(&ioch->update_lock, flags);
	return page;
}

static int do_write_bio(struct iocache_device *const ioch,
	unsigned int op, struct bio *bio,
	pgoff_t index, unsigned int offset,
	bool *need_endio)
{
	struct bio_vec bvec;
	struct bvec_iter iter;
	struct page *to = NULL;
	struct radix_tree_root *trans_space;
	struct iocache_transaction *trans = NULL;
	unsigned long flags;
	unsigned char sdio_bio = 0;
	unsigned long sdio_iv;
	unsigned int nrbios = 0;
	int err = 0;
	int retry_time = 10;

	if (bio_segments(bio) == 0)
		goto err;

	if (bio_encrypted(bio) && bio_bcf_test(bio, BC_IV_CTX)) {
		sdio_iv = bio_bc_iv_get(bio);
		sdio_bio = 1;
	}

	down_read(&ioch->commit_rwsem);
	if (bio->bi_opf & REQ_FG)
		atomic_inc(&ioch->w_count_fg);

	trans = latest_transaction(ioch);

	bio_for_each_segment(bvec, bio, iter) {
		void *pslot;
		struct page *const src = bvec.bv_page;
		void *kto, *ksrc;

		/* update latest & transaction space */
		if (!to || index != to->index) {
			if (to)
				put_page(to);

			iocache_migrate_lock(ioch);
repeat:
			rcu_read_lock();
			trans_space = &trans->space;
			pslot = radix_tree_lookup_slot(trans_space, index);
			if (!pslot)
				goto reg;

			to = radix_tree_deref_slot(pslot);
			if (!to) {
reg:
				rcu_read_unlock();
				to = register_page(ioch, index, pslot, &trans);
				if (to == ERR_PTR(-EAGAIN)) {
#ifdef CONFIG_HUAWEI_IOCACHE_DSM
					DSM_IOCACHE_LOG(DSM_IOCACHE_ERROR,
						"register_page failed EAGAIN\n");
#endif
					goto repeat;
				} else if (to == ERR_PTR(-ENOMEM)) {
					up_read(&ioch->commit_rwsem);
					mutex_lock(&ioch->no_mem_lock);
					ioch->no_mem = true;
					pr_info("iocache: page register failed & submit remained bio\n");
					wake_up_interruptible_all(
						&ioch->drain_trans_wait);
					wait_event(ioch->no_mem_wait,
						!(ioch->no_mem));
					mutex_unlock(&ioch->no_mem_lock);
					down_read(&ioch->commit_rwsem);
					if (retry_time--) {
#ifdef CONFIG_HUAWEI_IOCACHE_DSM
						DSM_IOCACHE_LOG(
							DSM_IOCACHE_ERROR,
							"register_page failed ENOMEM, retry_time=%d\n",
							retry_time);
#endif
						trans =
						    latest_transaction(ioch);
						goto repeat;
					}
					pr_err("iocache: no_mem & retry failed\n");
					BUG();
				}

				if (IS_ERR(to)) {
					if (bio->bi_opf & REQ_FG)
						atomic_dec(&ioch->w_count_fg);

					iocache_migrate_unlock(ioch);
					up_read(&ioch->commit_rwsem);
					return PTR_ERR(to);
				}
				iocache_migrate_unlock(ioch);
			} else if (radix_tree_exception(to)) {
				if (radix_tree_deref_retry(to)) {
					rcu_read_unlock();
					goto repeat;
				}
				BUG();
			} else {
				/* latest space should be to as well */
				get_page(to);

				hook_page(ioch, index, to);
				rcu_read_unlock();
				iocache_migrate_unlock(ioch);
			}
		}

		/* no need to consider reading when writing */
		kto = kmap_atomic(to);
		ksrc = kmap_atomic(src);
		memcpy(kto, ksrc + bvec.bv_offset, bvec.bv_len);

		kunmap_atomic(ksrc);
		kunmap_atomic(kto);

		iocache_migrate_lock(ioch);
		spin_lock_irqsave(iocache_page_key_lock(to), flags);
		if (page_contain_key(to))
			clean_page_key(to);
		dup_bio_key_2_page(bio, to);
		save_org_page_idx(to, src);
		if (sdio_bio)
			iocache_page_key_entity(to)->bc_iv = sdio_iv;
		spin_unlock_irqrestore(iocache_page_key_lock(to), flags);
		iocache_migrate_unlock(ioch);

		update_position(&index, &offset, &bvec);

		if (sdio_bio)
			++sdio_iv;
	}
	if (bio->bi_opf & REQ_FG)
		atomic_dec(&ioch->w_count_fg);

	up_read(&ioch->commit_rwsem);
err:
	if (to)
		put_page(to);

	*need_endio = true;
	return err;
}

static int do_discard_bio(struct iocache_device *const ioch,
			  unsigned int op, struct bio *bio,
			  pgoff_t index, unsigned long offset,
			  bool *need_endio)
{
	size_t n = bio->bi_iter.bi_size;
	unsigned long flags;
	int err;

	if (offset) {
		if (n <= (PAGE_SIZE - offset))
			return -EIO;

		n -= (PAGE_SIZE - offset);
		index++;
	}

	err = 0;
	down_read(&ioch->commit_rwsem);
	while (n >= PAGE_SIZE) {
		void *pslot;
		struct page *to;

		err = radix_tree_preload(GFP_NOIO);
		if (err)
			goto out;

		spin_lock_irqsave(&ioch->update_lock, flags);

repeat:
		pslot = radix_tree_lookup_slot(&ioch->latest_space,
					       index);
		if (!pslot)
			goto reg;

		to = radix_tree_deref_slot(pslot);

		if (!to) {
reg:
			err = radix_tree_insert(&ioch->latest_space,
						index, IOC_DISCARD_MARKED);
			BUG_ON(err);
		} else if (radix_tree_exception(to)) {
			if (radix_tree_deref_retry(to))
				goto repeat;
			BUG();
		} else if (to != IOC_DISCARD_MARKED) {
			radix_tree_replace_slot(&ioch->latest_space,
				pslot, IOC_DISCARD_MARKED);
			BUG_ON(page_ref_count(to) < 2);
			put_page(to);
		}
		spin_unlock_irqrestore(&ioch->update_lock, flags);
		++index;
		n -= PAGE_SIZE;

		radix_tree_preload_end();
	}
out:
	up_read(&ioch->commit_rwsem);
	*need_endio = true;
	return err;
}

struct iocache_transaction *iocache_new_transaction(gfp_t gfp)
{
	struct iocache_transaction *trans;

	trans = kmalloc(sizeof(struct iocache_transaction), gfp);
	if (!trans)
		return ERR_PTR(-ENOMEM);

	INIT_RADIX_TREE(&trans->space, GFP_ATOMIC);

	trans->start_time = jiffies;
	trans->commit_time = trans->start_time - 1;
	atomic_set(&trans->nr_pages, 0);
	return trans;
}

static struct iocache_transaction *latest_trans(struct iocache_device *ioch)
{
	return container_of(READ_ONCE(ioch->transactions.prev),
		struct iocache_transaction, list);
}


static int commit_transaction(struct iocache_device *const ioch)
{
	int flushmerge_no;
	struct iocache_transaction *oldtrans, *trans;
	int ret;

	trans = iocache_new_transaction(GFP_NOIO);
	if (IS_ERR(trans))
		return PTR_ERR(trans);

	down_read(&ioch->commit_rwsem);
	flushmerge_no = atomic_read(&ioch->flushmerge_no);
	/* update commit_time of the current transaction */
	oldtrans = latest_trans(ioch);
	ret = radix_tree_empty(&oldtrans->space);
	up_read(&ioch->commit_rwsem);

	if (ret) {
#ifdef CONFIG_HUAWEI_IOCACHE_DSM
		DSM_IOCACHE_LOG(DSM_IOCACHE_ERROR,
			"iocache: flush from app be dropped\n");
#endif
		goto notrans;
	}

	if (atomic_inc_return(&ioch->flushmerge_waiters) != 1) {
		unsigned int retries = 2;

		do {
			io_schedule();

			/* take lock for serialization */
			down_read(&ioch->commit_rwsem);
			/* not the last transaction, flush is not needed */
			ret = (flushmerge_no !=
				atomic_read(&ioch->flushmerge_no));
			up_read(&ioch->commit_rwsem);
			if (ret)
				goto notrans;
		} while (--retries);
	}
	down_write(&ioch->commit_rwsem);
	oldtrans = latest_trans(ioch);
	oldtrans->commit_time = jiffies;
	list_add(&trans->list, &oldtrans->list);
	atomic_inc(&ioch->flushmerge_no);
	atomic_set(&ioch->flushmerge_waiters, 0);
	up_write(&ioch->commit_rwsem);
	return 0;
notrans:
	kfree(trans);
	return 0;
}

static unsigned int radix_tree_gang_lookup_contig(struct radix_tree_root *root,
	pgoff_t *pdx,
	unsigned int nr_pages,
	struct page **pages)
{
	struct radix_tree_iter iter;
	void **pslot;
	unsigned int i = 0;
	struct page *prev_page = NULL;
	pgoff_t  pre_page_org_index;

	rcu_read_lock();
	radix_tree_for_each_slot(pslot, root, &iter, *pdx) {
		struct page *page;

		if (i && *pdx != iter.index)
			break;

repeat:
		page = radix_tree_deref_slot(pslot);
		if (unlikely(!page))
			continue;

		if (radix_tree_exception(page)) {
			if (radix_tree_deref_retry(page)) {
				pslot = radix_tree_iter_retry(&iter);
				goto repeat;
			}
			BUG();
		}

		/* key not match, do not merge */
		if (prev_page && !page_key_same(prev_page, page))
			break;

		/* encrypt page, page index must continuous */
		if (prev_page && page_contain_key(prev_page) &&
			((pre_page_org_index + 1) !=
			iocache_page_key_org_idx(page)))
			break;

#ifdef CONFIG_MIGRATION
		iocache_page_key(page)->trans = NULL;
#endif
		pages[i++] = page;
		prev_page = page;
		pre_page_org_index = iocache_page_key_org_idx(page);
		*pdx = iter.index + 1;

		if (i >= nr_pages)
			break;
	}
	rcu_read_unlock();

	return i;
}

static int iocache_gang_discard_replace(struct iocache_device *const ioch,
	pgoff_t *pdx,
	pgoff_t *head,
	unsigned int nr_pages)
{
	struct radix_tree_iter iter;
	void **pslot;
	bool found = false;
	int err;
	unsigned int i = 0;
	unsigned long flags;

	err = radix_tree_preload(GFP_KERNEL);
	if (err)
		return err;

	spin_lock_irqsave(&ioch->erase_lock, flags);
	spin_lock(&ioch->update_lock);
	radix_tree_for_each_slot(pslot, &ioch->latest_space, &iter, *pdx) {
		struct page *page;

		if (i && *pdx != iter.index)
			break;

repeat:
		page = radix_tree_deref_slot(pslot);
		if (unlikely(!page))
			continue;

		if (radix_tree_exception(page)) {
			if (radix_tree_deref_retry(page)) {
				pslot = radix_tree_iter_retry(&iter);
				goto repeat;
			}
			BUG();
		}

		*pdx = iter.index + 1;
		found = true;
		if (page != IOC_DISCARD_MARKED)
			break;

		if (!i)
			*head = iter.index;

		err = radix_tree_insert(&ioch->erasemap,
			iter.index, IOC_DISCARD_ISSUED);
		if (err == -EEXIST) {
			pr_err("idx %lu exist in erasemap\n", iter.index);
			radix_tree_delete(&ioch->latest_space, iter.index);
			break;
		} else if (err) {
			pr_err("discard err: iter.index = %lu\n", iter.index);
			*pdx = iter.index;
			break;
		}
		radix_tree_delete(&ioch->latest_space, iter.index);
		if (++i >= nr_pages)
			break;
	}
	spin_unlock(&ioch->update_lock);
	spin_unlock_irqrestore(&ioch->erase_lock, flags);

	radix_tree_preload_end();
	return found ? i : -1;
}

static void page_rcu_callback(struct rcu_head *rcu)
{
	struct page *const page = container_of(rcu, struct page, rcu_head);

	put_page(page);
}

static void pagewrite_endio(struct bio *bio)
{
	struct iocache_device *const ioch = bio->bi_private;
	struct bio_vec *bvec;
	int i;
	unsigned long flags;

	if (bio->bi_status) {
		pr_err("iocache: bi_error: %d\n", bio->bi_status);
		BUG();
	}

	bio_for_each_segment_all(bvec, bio, i) {
		struct page *const page = bvec->bv_page;

		spin_lock_irqsave(iocache_page_key_lock(page), flags);
		clean_page_key(page);
		spin_unlock_irqrestore(iocache_page_key_lock(page), flags);
		free_page_key(ioch, page);
		spin_lock_irqsave(&ioch->update_lock, flags);
		put_page(page);

		if (radix_tree_delete_item(&ioch->latest_space,
			page->index, page) == page)
			put_page(page);
		spin_unlock_irqrestore(&ioch->update_lock, flags);
	}

	if (atomic_sub_return(i, &ioch->tot_uppages) <
	    atomic_read(&ioch->iocache_page_hard_limit))
		wake_up_all(&ioch->alloc_pages_wait);
	atomic_dec(&ioch->iocontrol_write_ios);

	if (!atomic_dec_return(&ioch->inflight_write_ios))
		wake_up(&ioch->inflight_write_wait);

	wake_up(&ioch->wait);

	if (!fg_in_flight(ioch)) {
		ioch->need_drain = true;
		wake_up_interruptible_all(&ioch->drain_trans_wait);
	}

	bio_put(bio);
}

static int do_drain_one_page(struct iocache_device *const ioch,
	struct bio **bio_ret, struct page *page,
	pgoff_t *idx_ret, unsigned int maxpages,
	unsigned int *nrbios, pgoff_t *pre_org_index)
{
	struct bio *bio = *bio_ret;
	pgoff_t index = *idx_ret;
	unsigned long flags;
	unsigned int no_merge = 0;

	if (page == IOC_DISCARD_MARKED)
		return 0;

	if (bio) {
		if (bio_encrypted(bio) && page_bcf_test(page, BC_CRYPT)) {
			if ((!key_same(iocache_page_key_entity(page),
				&(bio->bi_crypt_ctx))) ||
				((*pre_org_index) + 1 !=
				iocache_page_key_org_idx(page)))
				no_merge = 1;
		}

		/* encrypt type mismatch do not merge */
		if ((bio_encrypted(bio) ^ page_bcf_test(page, BC_CRYPT)))
			no_merge = 1;
	}

	if (((page->index != index) || no_merge) && bio) {
		bio_bcf_set(bio, BC_BIO_FROM_IOCACHE);
                bio->bi_opf |= REQ_SYNC;
		atomic_inc(&ioch->iocontrol_write_ios);
		__submit_bio(bio, REQ_OP_WRITE, 0, ioch);
		bio = NULL;
	}

	if (!bio) {
grab_bio:
		maxpages = BIO_MAX_PAGES;
		if (fg_in_flight(ioch))
			maxpages = maxpages / 2;
		bio = bio_alloc(GFP_NOIO, maxpages);

		bio_set_dev(bio, ioch->rbd);
		bio->bi_end_io = pagewrite_endio;
		index = page->index;

		bio->bi_iter.bi_sector = index << SECTORS_PER_PAGE_SHIFT;
		bio->bi_private = ioch;
		spin_lock_irqsave(iocache_page_key_lock(page), flags);
		dup_key_from_page(bio, page);
		spin_unlock_irqrestore(iocache_page_key_lock(page), flags);
		++*nrbios;
	}

	if (bio_add_page(bio, page, PAGE_SIZE, 0) < PAGE_SIZE) {
		bio_bcf_set(bio, BC_BIO_FROM_IOCACHE);
                bio->bi_opf |= REQ_SYNC;
		atomic_inc(&ioch->iocontrol_write_ios);
		__submit_bio(bio, REQ_OP_WRITE, 0, ioch);
		goto grab_bio;
	}

	*bio_ret = bio;
	*idx_ret = ++index;
	*pre_org_index =  iocache_page_key_org_idx(page);
	return 0;
}

struct iocache_discard_info {
	struct iocache_device *ioch;
	pgoff_t head;
	unsigned int nrpages;
};

static void iocache_discard_endio(struct bio *bio)
{
	struct iocache_discard_info *const di = bio->bi_private;
	struct iocache_device *const ioch = di->ioch;
	pgoff_t idx;
	unsigned long flags;

	spin_lock_irqsave(&ioch->erase_lock, flags);
	for (idx = di->head; idx < di->head + di->nrpages; ++idx) {
		void *entry;

		entry = radix_tree_delete_item(&ioch->erasemap, idx,
					       IOC_DISCARD_ISSUED);
		if (entry != IOC_DISCARD_ISSUED) {
			pr_err("%s: entry = %p, idx = %lu\n",
				__func__, entry, idx);
			BUG();
		}
	}
	spin_unlock_irqrestore(&ioch->erase_lock, flags);
	atomic_sub(di->nrpages, &ioch->inflight_discard);

	wake_up_all(&ioch->erase_wq);
	kfree(di);
	bio_put(bio);
}

static int __iocache_issue_discard(struct iocache_device *const ioch,
	pgoff_t head, unsigned int nrpages,
	gfp_t gfp_mask, unsigned long flags)
{
	struct block_device *bdev = ioch->rbd;
	struct bio *bio = NULL;
	struct iocache_discard_info *di;
	int err;

	di = kmalloc(sizeof(*di), gfp_mask);
	if (!di)
		return -ENOMEM;

	di->head = head;
	di->nrpages = nrpages;
	di->ioch = ioch;
	err = __blkdev_issue_discard(bdev, head << SECTORS_PER_PAGE_SHIFT,
		nrpages << SECTORS_PER_PAGE_SHIFT,
		gfp_mask, flags, &bio);
	if (!err && bio) {
		atomic_add(nrpages, &ioch->inflight_discard);
		bio->bi_private = di;
		bio->bi_end_io = iocache_discard_endio;
		bio->bi_opf |= REQ_SYNC;
		__submit_bio(bio, REQ_OP_DISCARD, 0, ioch);
	}
	return err;
}

static void iocache_issue_discard(struct iocache_device *const ioch)
{
	pgoff_t idx, head;

	head = idx = 0;
	do {
		if (atomic_read(&ioch->inflight_discard) + 16 >
			INFLIGHT_DISCARD_MAX_PAGES)
			break;
		int nrpages = iocache_gang_discard_replace(ioch,
			&idx, &head, 16);

		if (nrpages < 0)
			break;
		if (!nrpages)
			continue;

		__iocache_issue_discard(ioch, head, nrpages, GFP_NOIO, 0);
	} while (1);
	up_read(&ioch->commit_rwsem);
}

static void wait_on_erasing_page(struct iocache_device *const ioch,
	pgoff_t idx)
{
	void **pslot;
	struct page *page;
	DEFINE_WAIT(wait);

	rcu_read_lock();
	pslot = radix_tree_lookup_slot(&ioch->erasemap, idx);
	if (!pslot) {
		rcu_read_unlock();
		return;
	}
	prepare_to_wait(&ioch->erase_wq, &wait, TASK_UNINTERRUPTIBLE);

repeat:
	page = radix_tree_deref_slot(pslot);
	if (!page)
		goto out;

	if (radix_tree_exception(page)) {
		if (radix_tree_deref_retry(page))
			goto repeat;
		BUG();
	}

	if (page != IOC_DISCARD_ISSUED)
		goto out;
	rcu_read_unlock();
	io_schedule();
	goto repeat;
out:
	rcu_read_unlock();
	finish_wait(&ioch->erase_wq, &wait);
}

static void ioch_delete_trans_pages(struct iocache_device *const ioch,
	struct iocache_transaction *const trans,
	pgoff_t idx, unsigned int nrpages,
	struct page **pages)
{
	unsigned long flags;
	unsigned int i;
	struct page *page;

	spin_lock_irqsave(&ioch->update_lock, flags);
	for (i = 0; i < nrpages; ++i, ++idx) {
		void *et;

		page = pages[i];

		et = radix_tree_delete(&trans->space, idx);
		BUG_ON(et != page);
		if (et != IOC_DISCARD_MARKED)
			atomic_dec(&trans->nr_pages);
	}
	spin_unlock_irqrestore(&ioch->update_lock, flags);
}

static int drain_transaction(struct iocache_device *const ioch,
	struct iocache_transaction *const trans,
	unsigned int *nr)
{
	pgoff_t idx = 0, idx2 = 0, idx3;
	struct bio *bio = NULL;
	unsigned int nrpages, nrbios;
	struct page *pages[16];
	int submitted = 0;
	unsigned long flags;
	pgoff_t pre_org_index;

	nrbios = 0;
	do {
		unsigned int i;

		iocache_migrate_lock(ioch);
		/* will not be freed since commit_rwsem is held */
		nrpages = radix_tree_gang_lookup_contig(&trans->space,
							&idx, 16,
							pages);
		iocache_migrate_unlock(ioch);

		if (!nrpages)
			break;

		idx3 = idx - nrpages;

		if (*nr < nrpages)
			nrpages = *nr;

		for (i = 0; i < nrpages; ++i) {
			wait_on_erasing_page(ioch, idx3 + i);

			if (do_drain_one_page(ioch, &bio, pages[i],
				&idx2, *nr,
				&nrbios, &pre_org_index)) {
				ioch_delete_trans_pages(ioch, trans,
							idx3, i, pages);
				atomic_add(nrbios, &ioch->inflight_write_ios);
				pr_err("do_drain_one_page error\n");
#ifdef CONFIG_HUAWEI_IOCACHE_DSM
				DSM_IOCACHE_LOG(DSM_IOCACHE_ERROR,
					"drain transaction submit bio partly && failed\n");
#endif
				return -ENOMEM;
			}
			--*nr;
			submitted++;
		}

		ioch_delete_trans_pages(ioch, trans, idx3, nrpages, pages);
	} while (*nr);

	if (bio) {
		bio_bcf_set(bio, BC_BIO_FROM_IOCACHE);
		atomic_inc(&ioch->iocontrol_write_ios);
		__submit_bio(bio, REQ_OP_WRITE, 0, ioch);
	}

	atomic_add(nrbios, &ioch->inflight_write_ios);

	return submitted;
}

static void putback_isolated_trans(struct iocache_device *const ioch,
	struct list_head *isolated_trans,
	bool locked)
{
	/* TODO: NO TESTED, someone should test the path. */
	if (!locked)
		down_write(&ioch->commit_rwsem);

	list_splice_init(isolated_trans, &ioch->transactions);
	up_write(&ioch->commit_rwsem);
}

int drain_transactions(struct iocache_device *const ioch,
	unsigned int nr, bool reclaim,
	bool senddiscard, bool trydrain)
{
	struct iocache_transaction *trans;
	unsigned int submitted = 0;
	bool locked;
	int ret;
	struct list_head isolated_trans;
	unsigned long drain_start_time = jiffies;
	unsigned long drain_finished_time;
	unsigned long drain_write_start_time;
#ifdef IOCACHE_DEBUG
	unsigned long drain_write_time;
	unsigned long drain_flush_start_time;
	unsigned long drain_flush_time;
#endif
	unsigned long tot_uppages = 0;
	int flush_ret = 0;

	if (!nr)
		return 0;

	if (!trydrain) {
		mutex_lock(&ioch->drain_lock);
	} else if (!mutex_trylock(&ioch->drain_lock)) {
		pr_info("shrink_scan: try drain_lock failed!\n");
		return 0;
	}

	if (reclaim) {
		trans = iocache_new_transaction(GFP_NOIO);
		if (IS_ERR(trans))
			trans = NULL;
	}

	locked = false;
	down_write(&ioch->commit_rwsem);
	int index = atomic_read(&ioch->tot_uppages) / 256;
	ioch->capacity_stat[index]++;
	if (reclaim && (trans || list_is_singular(&ioch->transactions))) {
		/* isolate all transactions and add a new noflush trans */
		isolated_trans = ioch->transactions;
		isolated_trans.prev->next = &isolated_trans;
		isolated_trans.next->prev = &isolated_trans;
		INIT_LIST_HEAD(&ioch->transactions);

		if (!trans) {
			locked = true;
		} else {
			list_add(&trans->list, &ioch->transactions);
			up_write(&ioch->commit_rwsem);
		}
	} else {
		/* isolate transactions except for the last transaction */
		trans = list_last_entry(&ioch->transactions,
			struct iocache_transaction, list);
		list_cut_position(&isolated_trans, &ioch->transactions,
			trans->list.prev);
		up_write(&ioch->commit_rwsem);
	}

	tot_uppages = atomic_read(&ioch->tot_uppages);
	pr_info("ioch->tot_uppages=%lu \n", tot_uppages);

	/* start to drain transactions_no_lock */
	while (!list_empty(&isolated_trans)) {
		trans = list_first_entry(&isolated_trans,
			struct iocache_transaction, list);
		list_del(&trans->list);

		if (radix_tree_empty(&trans->space)) {
			kfree(trans);
			continue;
		}

		ret = drain_transaction(ioch, trans, &nr);
		if (ret < 0 || !radix_tree_empty(&trans->space)) {
			pr_err("drain failed %d\n", ret);
#ifdef CONFIG_HUAWEI_IOCACHE_DSM
			DSM_IOCACHE_LOG(DSM_IOCACHE_ERROR,
				"drain failed & ret = %d\n", ret);
#endif
			list_add(&trans->list, &isolated_trans);
			goto err;
		}

		drain_write_start_time = jiffies;
		/* first wait all write command ends */
		io_wait_event(ioch->inflight_write_wait,
			!atomic_read(&ioch->inflight_write_ios));
#ifdef IOCACHE_DEBUG
		if (time_after_eq(jiffies,
			drain_write_start_time +
			msecs_to_jiffies(IOCACHE_DRAIN_WRITE_TIME))) {
			drain_write_time = jiffies;
			pr_info("drain write time: %u\n",
				jiffies_to_msecs(drain_write_time -
				drain_write_start_time));
#ifdef CONFIG_HUAWEI_IOCACHE_DSM
			DSM_IOCACHE_LOG(DSM_IOCACHE_ERROR,
				"drain write cost time: %u\n",
				jiffies_to_msecs(drain_write_time -
				drain_write_start_time));
#endif
		}
		drain_flush_start_time = jiffies;
#endif
		if (time_after_eq(trans->commit_time, trans->start_time)) {
			sector_t error_sector;

			/* it's time to send FLUSH command */
			flush_ret = blkdev_issue_flush(ioch->rbd, GFP_NOIO,
				&error_sector);
			if (flush_ret) {
				pr_err("drain flush error: flush_ret=%d\n",
					flush_ret);
				BUG();
			}

#ifdef IOCACHE_DEBUG
			if (time_after_eq(jiffies,
				drain_flush_start_time +
				msecs_to_jiffies(IOCACHE_DRAIN_FLUSH_TIME))) {
				drain_flush_time = jiffies;
				pr_info("drain flush time: %u\n",
					jiffies_to_msecs(drain_flush_time -
					drain_flush_start_time));
#ifdef CONFIG_HUAWEI_IOCACHE_DSM
				DSM_IOCACHE_LOG(DSM_IOCACHE_ERROR,
					"drain flush cost time: %u\n",
					jiffies_to_msecs(drain_flush_time -
					drain_flush_start_time));
#endif
			}
#endif
		}
		submitted += ret;
		if (atomic_read(&trans->nr_pages)) {
			pr_err("drained trans nr_pages = %u!\n",
				atomic_read(&trans->nr_pages));
			BUG();
		}
		kfree(trans);

		/* deal with nr = 0 */
		if (!nr) {
			pr_info("iocache: nr = 0 && exit\n");
			goto err;
		}
	}

	if (locked) {
		BUG_ON(!reclaim);
		BUG_ON(!list_empty(&ioch->transactions));
		/* still cannot get memory, do nofail at this */
		trans = iocache_new_transaction(GFP_NOIO | __GFP_NOFAIL);
		list_add(&trans->list, &ioch->transactions);
		if (senddiscard)
			downgrade_write(&ioch->commit_rwsem);
		else
			up_write(&ioch->commit_rwsem);
	} else if (senddiscard) {
		down_read(&ioch->commit_rwsem);

		/* still no new transaction, which means no more flush */
		if (!list_is_singular(&ioch->transactions)) {
			up_read(&ioch->commit_rwsem);
			senddiscard = false;
		}
	}

	if (senddiscard)
		/* let's issue discard now */
		iocache_issue_discard(ioch);

	mutex_unlock(&ioch->drain_lock);
	if (reclaim && ioch->no_mem) {
		ioch->no_mem = false;
		wake_up(&ioch->no_mem_wait);
	}

	if (time_after_eq(jiffies,
		drain_start_time + msecs_to_jiffies(IOCACHE_DRAIN_TIME))) {
		drain_finished_time = jiffies;
		pr_info("drain cost time: %u\n",
			jiffies_to_msecs(drain_finished_time -
			drain_start_time));
#ifdef CONFIG_HUAWEI_IOCACHE_DSM
		DSM_IOCACHE_LOG(DSM_IOCACHE_ERROR,
			"drain cost time: %u\n",
			jiffies_to_msecs(drain_finished_time -
			drain_start_time));
#endif
	}
	if (time_after_eq(drain_start_time, drain_write_start_time))
		drain_write_start_time = drain_start_time;

	if (time_after_eq(jiffies,
		drain_write_start_time +
		msecs_to_jiffies(IOCACHE_DRAIN_LASTTANS_TIME))) {
		drain_finished_time = jiffies;
		pr_info("drain last_trans cost time: %u\n",
			jiffies_to_msecs(drain_finished_time -
			drain_write_start_time));
#ifdef CONFIG_HUAWEI_IOCACHE_DSM
		DSM_IOCACHE_LOG(DSM_IOCACHE_ERROR,
			"drain last_trans cost time: %u\n",
			jiffies_to_msecs(drain_finished_time -
			drain_write_start_time));
#endif
		drain_write_start_time = jiffies;
	}
#ifdef CONFIG_HUAWEI_IO_TRACING
	trace_iocache_drain_last_trans_end(submitted);
#endif
	return submitted;

err:
	putback_isolated_trans(ioch, &isolated_trans, locked);
	mutex_unlock(&ioch->drain_lock);
	return ret;
}

blk_qc_t iocache_make_request(struct request_queue *q,
	struct bio *bio)
{
	struct iocache_device *const ioch = bio->bi_disk->private_data;
	pgoff_t index;
	unsigned int offset, op;
	int err;
	bool postflush, need_endio;
	unsigned long make_request_start_time = jiffies;
	unsigned long make_request_end_time;

	down_read(&ioch->switch_rwsem);
	if (ioch->ioch_switch == 0) {
		/* iocache switch off */
		up_read(&ioch->switch_rwsem);
		bio_set_dev(bio, ioch->rbd);
		submit_bio(bio);
		return BLK_QC_T_NONE;
	}

	if (bio_end_sector(bio) > get_capacity(bio->bi_disk))
		goto err_io;

	postflush = false;
	need_endio = false;

	if (op_is_flush(bio->bi_opf)) {
		/* transfer REQ_FUA to POSTFLUSH */
		if (bio->bi_opf & REQ_FUA)
			postflush = true;

		/* commit and start a new iocache transaction */
		if (bio->bi_opf & REQ_PREFLUSH)
			if (commit_transaction(ioch))
				goto err_io;
	}

	op = bio_op(bio);

	index = bio->bi_iter.bi_sector >> SECTORS_PER_PAGE_SHIFT;
	offset = (bio->bi_iter.bi_sector &
		  (SECTORS_PER_PAGE - 1)) << SECTOR_SHIFT;

	/* handle DISCARD command */
	if (op == REQ_OP_DISCARD || op == REQ_OP_WRITE_ZEROES) {
		err = do_discard_bio(ioch, op, bio, index, offset,
				     &need_endio);
		goto out;
	}

	if (!op_is_write(op))
		err = do_read_bio(ioch, op, bio, index, offset,
				  &need_endio);
	else
		err = do_write_bio(ioch, op, bio, index, offset,
				   &need_endio);

out:
	if (time_after_eq(jiffies,
		make_request_start_time +
		msecs_to_jiffies(IOCACHE_MAKE_REQUEST_TIME))) {
		make_request_end_time = jiffies;
		pr_info("iocache make request cost time: %u, r/w=%u ,fua=%d, preflush=%d\n",
			jiffies_to_msecs(make_request_end_time -
			make_request_start_time),
			op_is_write(op),
			bio->bi_opf & REQ_FUA,
			bio->bi_opf & REQ_PREFLUSH);
#ifdef CONFIG_HUAWEI_IOCACHE_DSM
		DSM_IOCACHE_LOG(DSM_IOCACHE_ERROR,
			"iocache make request cost time: %u, r/w=%u, fua=%d, preflush=%d\n",
			jiffies_to_msecs(make_request_end_time -
			make_request_start_time),
			op_is_write(op),
			bio->bi_opf & REQ_FUA,
			bio->bi_opf & REQ_PREFLUSH);
#endif
	}

	if (!err) {
		/* here need to commit translation as well - POSTFLUSH */
		if (postflush && commit_transaction(ioch))
			goto err_io;

		up_read(&ioch->switch_rwsem);
		if (need_endio)
			bio_endio(bio);
		return BLK_QC_T_NONE;
	}

err_io:
	up_read(&ioch->switch_rwsem);
	bio_io_error(bio);
	return BLK_QC_T_NONE;
}


static inline bool is_idle(struct iocache_device *const ioch)
{
	return !fg_in_flight(ioch);
}

int iocache_flush_thread_func(void *data)
{
	struct iocache_device *ioch = data;
	unsigned long jiffies_interval;
	bool idle = true, reach_limit;
	struct sched_param scheduler_params;
 	/* Set as RT priority */
 	scheduler_params.sched_priority = 1;
 	sched_setscheduler(current, SCHED_FIFO, &scheduler_params);
	while (1) {
		if (kthread_should_stop())
			break;

		jiffies_interval = jiffies;

		if (reach_limit || ioch->no_mem)
			drain_transactions(ioch, UINT_MAX, true, false, false);
		else
			drain_transactions(ioch, UINT_MAX, false, idle, false);

		jiffies_interval = jiffies - jiffies_interval;
		idle = is_idle(ioch);
		reach_limit = atomic_read(&ioch->tot_uppages) >=
			atomic_read(&ioch->iocache_page_hard_limit);
		if (jiffies_interval < ioch->threshold[idle] && !reach_limit) {
			wait_event_interruptible_timeout(ioch->drain_trans_wait,
				ioch->need_drain | ioch->no_mem,
			1000 / HZ * (ioch->threshold[idle] - jiffies_interval));
			if (ioch->need_drain)
				ioch->need_drain = false;
		}
	}
	return 0;
}

