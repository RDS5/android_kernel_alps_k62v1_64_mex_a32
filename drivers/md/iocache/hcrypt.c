// SPDX-License-Identifier: GPL-2.0
/*
 * linux/drivers/md/iocache/hcrypt.c
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

struct iocache_encrypt_key *alloc_encrypt_key(
	struct iocache_device *const ioch)
{
	BUG_ON(!ioch->encrypt_key_cache);
	struct iocache_encrypt_key *encrypt_key = NULL;
	unsigned int retries = 3;

	do {
		encrypt_key = (struct iocache_encrypt_key *)kmem_cache_zalloc(
			ioch->encrypt_key_cache,
			GFP_NOWAIT | __GFP_NORETRY | __GFP_NOWARN);
		if (encrypt_key)
			break;
		congestion_wait(BLK_RW_ASYNC, HZ/50);
		cond_resched();
	} while (--retries);

	if (!encrypt_key)
		encrypt_key = (struct iocache_encrypt_key *)kmem_cache_zalloc(
			ioch->encrypt_key_cache,
			GFP_NOIO);
	if (!encrypt_key)
		return NULL;

	spin_lock_init(&encrypt_key->key_lock);
	atomic_inc(&ioch->cur_key_cnt);
	encrypt_key->org_page_index = IOCACHE_INVALID_IDX;

	return encrypt_key;
}

void free_encrypt_key(struct iocache_device *const ioch,
	struct iocache_encrypt_key *encrypt_key)
{
	BUG_ON(!(ioch->encrypt_key_cache && encrypt_key));
	kmem_cache_free(ioch->encrypt_key_cache, encrypt_key);
	atomic_dec(&ioch->cur_key_cnt);
}

void free_page_key(struct iocache_device *const ioch,
	struct page *page)
{
	BUG_ON(!(page->private && ioch->encrypt_key_cache));
	free_encrypt_key(ioch, (struct iocache_encrypt_key *)page->private);
	page->private = 0;
}

void dup_bio_key_2_page(struct bio *bio, struct page *page)
{
	BUG_ON(!page->private);
	/* for HIE */
	if (bio->bi_crypt_ctx.bc_info)
		bio->bi_crypt_ctx.bc_info_act(
			bio->bi_crypt_ctx.bc_info,
			BIO_BC_INFO_GET);

	*iocache_page_key_entity(page) = bio->bi_crypt_ctx;

#if defined(CONFIG_MTK_HW_FDE)
	/* for FDE */
	iocache_page_key(page)->bi_hw_fde = bio->bi_hw_fde;
	iocache_page_key(page)->bi_key_idx = bio->bi_key_idx;
#endif
}

void save_org_page_idx(struct page *new_page, struct page *org_page)
{
	BUG_ON(!new_page->private);
	iocache_page_key_org_idx(new_page) = page_index(org_page);
}

void clean_page_key(struct page *page)
{
	BUG_ON(!page->private);

	if (iocache_page_key_entity(page)->bc_info_act)
		iocache_page_key_entity(page)->bc_info_act(
			iocache_page_key_entity(page)->bc_info,
			BIO_BC_INFO_PUT);

	memset((void *)iocache_page_key_entity(page), 0,
		sizeof(struct bio_crypt_ctx));
	iocache_page_key_org_idx(page) = IOCACHE_INVALID_IDX;
}

int page_contain_key(struct page *page)
{
	BUG_ON(!page->private);
	return iocache_page_key_entity(page)->bc_info ? 1 : 0;
}

int key_same(struct bio_crypt_ctx *key1, struct bio_crypt_ctx *key2)
{
	return ((key1->bc_flags == key2->bc_flags) &&
		(key1->bc_key_size == key2->bc_key_size) &&
		(key1->bc_info  == key2->bc_info) &&
		(key1->bc_info_act == key2->bc_info_act) &&
		(key1->bc_ino == key2->bc_ino) &&
		(key1->bc_sb == key2->bc_sb));
}

int page_key_same(struct page *src_page, struct page *dst_page)
{
	BUG_ON(!(src_page->private && dst_page->private));
	return key_same(iocache_page_key_entity(src_page),
		iocache_page_key_entity(dst_page));
}

int page_bcf_test(struct page *page, unsigned int flag)
{
	BUG_ON(!page->private);
	return iocache_page_key_entity(page)->bc_flags & flag;
}

void dup_key_from_bio(struct bio *dst, const struct bio *src)
{
	/* for HIE */
	dst->bi_crypt_ctx = src->bi_crypt_ctx;

	if (src->bi_crypt_ctx.bc_info)
		src->bi_crypt_ctx.bc_info_act(
			src->bi_crypt_ctx.bc_info,
			BIO_BC_INFO_GET);

#if defined(CONFIG_MTK_HW_FDE)
	/* for FDE */
	dst->bi_hw_fde = src->bi_hw_fde;
	dst->bi_key_idx = src->bi_key_idx;
#endif
}

void dup_key_from_page(struct bio *dst, struct page *page)
{
	/* for HIE */
	dst->bi_crypt_ctx = *iocache_page_key_entity(page);

	if (dst->bi_crypt_ctx.bc_info)
		dst->bi_crypt_ctx.bc_info_act(
			dst->bi_crypt_ctx.bc_info,
			BIO_BC_INFO_GET);

#if defined(CONFIG_MTK_HW_FDE)
	/* for FDE */
	dst->bi_hw_fde = iocache_page_key(page)->bi_hw_fde;
	dst->bi_key_idx = iocache_page_key(page)->bi_key_idx;
#endif
}
