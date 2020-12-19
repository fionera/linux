/*
 * drivers/staging/ekp/romempool.c
 *
 * Copyright (C) 2017-2019 LG Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define pr_fmt(fmt) "romempool: " fmt

#include <linux/bitmap.h>
#include <linux/bootmem.h>
#include <linux/cma.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/ekp.h>
#include <linux/genalloc.h>
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/ktime.h>
#include <linux/memblock.h>
#include <linux/printk.h>
#include <linux/romempool.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>

#include <asm/ekp.h>
#include <asm/sections.h>
#include <asm/tlb.h>
#include <asm-generic/getorder.h>

/*
 * Initial start address and size of romempool. This is set by
 * romempool_reserve() on boot-up time. Don't be confused that
 * these values don't cover entire romempool address ranges when
 * romempool is expandable.
 */
static unsigned long romempool_start __ro_after_init;
static unsigned long romempool_size __ro_after_init;

static unsigned long romempool_total_size;
static unsigned long romempool_max_size;

/* Indicate whether romempool_early_alloc() can be used or not */
static bool romempool_initialized __ro_after_init;

static struct gen_pool *romempool __ro_after_init;

static void (*romempool_free_init_pgtable_func)(unsigned long,
						enum romempool_flags,
						unsigned long) __ro_after_init;

bool romempool_free_init_pgtable(phys_addr_t phys)
{
	if (!romempool_free_init_pgtable_func)
		return false;

	if (romempool_initialized && !is_romempool(phys))
		return false;

	romempool_free_init_pgtable_func((unsigned long)__va(phys),
					 RMP_SPECIAL, 0);
	return true;
}

/* Each architecture should implement this to make memory RO */
#ifndef arch_mark_romempool_ro
static inline void arch_mark_romempool_ro(unsigned long start,
					  unsigned long size)
{
	return;
}
#endif

#ifndef arch_mark_romempool_rw
static inline void arch_mark_romempool_rw(unsigned long start,
					  unsigned long size)
{
	return;
}
#endif

void mark_romempool_ro(void)
{
	if (!romempool)
		return;

	arch_mark_romempool_ro(romempool_start, romempool_size);
}

/*
 * Read-only Memory Pool Statistics
 */
static atomic_t romempool_stats[NR_RMP_FLAGS];

static inline void inc_romempool_stat(enum romempool_flags flag,
				      unsigned int order)
{
	atomic_add(1 << order, &romempool_stats[flag]);
}

static inline void dec_romempool_stat(enum romempool_flags flag,
				      unsigned int order)
{
	atomic_sub(1 << order, &romempool_stats[flag]);
}

#ifdef CONFIG_EKP_ROMEMPOOL_ALLOW_WRITABLE_MEMORY
static atomic_t romempool_out_of_memory_count;
#endif

/*
 * addr_in_gen_pool() can be used to check whether current virtual
 * address is in romempool. However, it iterates over all chunks and
 * this operation might cause performance hit. So, we creates the bitmap
 * indicating which physical address is in the pool here.
 */
DECLARE_BITMAP(romempool_phys, ROMEMPOOL_PHYS_SIZE) __ro_after_init;

bool is_romempool(phys_addr_t phy)
{
	if (!romempool)
		return false;

	return test_bit(round_down(phy, PMD_SIZE) / PMD_SIZE, romempool_phys);
}

#if defined(CONFIG_EKP_ROMEMPOOL_USE_HIGH_ORDER_ALLOCATION) || \
    defined(CONFIG_EKP_ROMEMPOOL_USE_CMA_ALLOCATION)
#define ROMEMPOOL_EXPANDABLE
#endif

/*
 * genalloc extensions for romempool.
 *
 * genalloc cannot satisfy some of romempool requirements. So following
 * new interfaces are implemented to bridge tha gap.
 */

static DEFINE_RWLOCK(chunk_list_lock);

/*
 * genalloc already provides chunk_size(), but it is a local function.
 * This interface just re-implements it.
 */
static inline size_t __gen_pool_chunk_size(struct gen_pool_chunk *chunk)
{
	return chunk->end_addr - chunk->start_addr + 1;
}

/*
 * genalloc always adds a new chunk to the head of the pool, but
 * romempool needs to add it to the tail. so, this function implements
 * it based on gen_pool_add_virt().
 * Need to hold the chunk_list_lock before getting here.
 */
static int __gen_pool_add_chunk(unsigned long start, unsigned long size)
{
	struct gen_pool_chunk *chunk;
	int nbits = size >> romempool->min_alloc_order;
	int nbytes = sizeof(struct gen_pool_chunk) +
		     BITS_TO_LONGS(nbits) * sizeof(long);

	chunk = kzalloc(nbytes, GFP_KERNEL);
	if (unlikely(!chunk))
		return -ENOMEM;

	chunk->start_addr = start;
	chunk->end_addr = start + size - 1;
	atomic_set(&chunk->avail, size);

	list_add_tail_rcu(&chunk->next_chunk, &romempool->chunks);

	return 0;
}

/*
 * genalloc doesn't support "removing unused chunk from the pool". So,
 * this function implements it.
 * Need to hold the chunk_list_lock before getting here.
 */
static void __gen_pool_remove_chunk(struct gen_pool_chunk *chunk)
{
	list_del(&chunk->next_chunk);
	kfree(chunk);
}

/*
 * We don't need to get a lock because genalloc uses RCU-protected list.
 * However, romempool needs to remove a unused chunk from the list, so
 * that we need to get a lock before accessing the list.
 */
static unsigned long __gen_pool_alloc_locked(unsigned long size)
{
	unsigned long addr;

	read_lock(&chunk_list_lock);
	addr = gen_pool_alloc(romempool, size);
	read_unlock(&chunk_list_lock);

	return addr;
}

/*
 * Pool Chunk List
 */
static LIST_HEAD(romempool_chunk_head);
static unsigned int nr_chunk;

enum romempool_chunk_type {
	RMP_CHUNK_RESERVED,
	RMP_CHUNK_HIGH_ORDER,
	RMP_CHUNK_CMA,
	NR_RMP_CHUNK_TYPES
};

static char *chunk_type_to_string[NR_RMP_CHUNK_TYPES] = {
	"",
	" by high order allocation",
	" by CMA allocation"
};

struct romempool_chunk {
	unsigned long start;
	unsigned long end;
	enum romempool_chunk_type type;
	struct gen_pool_chunk *chunk;
	struct list_head list;
};

static __always_inline void ekp_set_romempool(struct romempool_chunk *chunk)
{
	ekp_tunnel(EKP_SET_ROMEMPOOL, __pa(chunk), 0, 0, 0, 0, 0);
}

/* chunk_list_lock is hold when this function is invoked */
static int romempool_add_chunk(unsigned long start, unsigned long size,
			       enum romempool_chunk_type type)
{
	int ret;
	struct gen_pool_chunk *chunk = NULL;
	struct romempool_chunk *new;

	/* Allocate genpool chunk and add it to the list */
	ret = __gen_pool_add_chunk(start, size);
	if (ret != 0)
		goto err;

	/* We always add a chunk to the tail. */
	chunk = list_last_entry(&romempool->chunks,
				struct gen_pool_chunk, next_chunk);

	/* Allocate romempool chunk and add it to the list */
	new = kmalloc(sizeof(struct romempool_chunk), GFP_KERNEL);
	if (!new)
		goto err;

	new->start = start;
	new->end = start + size;
	new->type = type;
	new->chunk = chunk;
	list_add_tail(&new->list, &romempool_chunk_head);

	ekp_set_romempool(new);

	romempool_total_size += size;
	if (romempool_max_size < romempool_total_size)
		romempool_max_size = romempool_total_size;
	nr_chunk++;

	pr_info("new chunk (%lu MB) is reserved%s\n", (new->end - new->start) / SZ_1M,
		chunk_type_to_string[type]);

	return 0;

err:
	if (chunk)
		__gen_pool_remove_chunk(chunk);

	pr_err("not enough memory for new memory chunk\n");
	return -ENOMEM;
}

#ifdef ROMEMPOOL_EXPANDABLE
#ifdef CONFIG_EKP_ROMEMPOOL_USE_CMA_ALLOCATION
static struct cma *romempool_cma __ro_after_init;
#endif

/* chunk_list_lock is hold when this function is invoked */
static void romempool_remove_chunk(struct romempool_chunk *chunk)
{
	size_t size = PMD_SIZE;

	/* Remove genpool chunk from the list and free it */
	__gen_pool_remove_chunk(chunk->chunk);
	chunk->chunk = NULL;

	/* Remove romempool chunk from the list and free it */
	list_del(&chunk->list);
	ekp_set_romempool(chunk);
	kfree(chunk);

	romempool_total_size -= size;
	nr_chunk--;
}

static void free_expanded_memory(unsigned long start,
				 unsigned long size,
				 enum romempool_chunk_type type)
{
	/*
	 * We can think that arch_mark_romempool_rw()
	 * won't try to get a read lock to allocate memory
	 * from romempool since memory area used by this
	 * function always has 2 MB block mapping.
	 * That is, we don't need to worry about deadlock
	 * situation here even if we already have a write
	 * lock.
	 */
	arch_mark_romempool_rw(start, size);

	switch (type) {
#ifdef CONFIG_EKP_ROMEMPOOL_USE_HIGH_ORDER_ALLOCATION
	case RMP_CHUNK_HIGH_ORDER:
		free_pages(start, get_order(size));
		break;
#endif
#ifdef CONFIG_EKP_ROMEMPOOL_USE_CMA_ALLOCATION
	case RMP_CHUNK_CMA:
		cma_release(romempool_cma, virt_to_page(start),
			    1 << get_order(size));
		break;
#endif
	default:
		BUG();
	}
}

static atomic_t expandable;
static atomic_t in_expanding;

static bool __romempool_expand(void)
{
	struct page *page = NULL;
	int ret;
	unsigned long start;
	size_t size = PMD_SIZE;
	enum romempool_chunk_type type;
#ifdef CONFIG_EKP_ROMEMPOOL_USE_HIGH_ORDER_ALLOCATION
	gfp_t gfp_mask = 0;

	gfp_mask |= __GFP_NOMEMALLOC;	/* don't use emergency reserves */
	gfp_mask |= __GFP_NORETRY;	/* don't loop in __alloc_pages */
	gfp_mask |= __GFP_NOWARN;	/* don't make warning on allocation fail */

	page = alloc_pages(gfp_mask, get_order(size));
	if (page)
		type = RMP_CHUNK_HIGH_ORDER;
#endif
#ifdef CONFIG_EKP_ROMEMPOOL_USE_CMA_ALLOCATION
	if (!page) {
		page = cma_alloc(romempool_cma, 1 << get_order(size),
				 get_order(size));
		if (page)
			type = RMP_CHUNK_CMA;
	}
#endif

	if (page) {
		start = (unsigned long)page_address(page);

		/*
		 * If current architecture needs to flush TLB for this
		 * range, arch-specific code should take care of it.
		 *
		 * See arch_mark_romempool_ro() on arch/arm64/mm/mmu.c
		 * for your reference.
		 */
		arch_mark_romempool_ro(start, size);

		write_lock(&chunk_list_lock);

		ret = romempool_add_chunk(start, size, type);
		if (ret) {
			free_expanded_memory(start, size, type);
			page = NULL;

			write_unlock(&chunk_list_lock);

			/*
			 * We cannot add a new chunk to the list, but
			 * we're still able to expand. So, in this case,
			 * we don't set 'expandable' to zero to re-try
			 * to expand the pool later.
			 */
			goto err;
		}

		write_unlock(&chunk_list_lock);

		return true;
	}
	/*
	 * If we get here, it means that we cannot allocate high order
	 * memory and reserved CMA area for romempool is already
	 * exhausted. We can say that once high order allocation failed,
	 * we cannot expect that high order (order 9) memory will be
	 * re-created later by any memory compaction since it is too high
	 * order.
	 * Therefore, we set 'expandable' to zero to indicate that there
	 * is no chance to expand the pool at all from now on.
	 */
	else
		atomic_set(&expandable, 0);

err:
	pr_err("not enough memory for expanding romempool\n");
	return false;
}

struct romempool_work {
	struct work_struct work;
	ktime_t start;
	ktime_t end;
};

struct romempool_workqueue {
	struct workqueue_struct *workq;
	wait_queue_head_t waitq;
	struct romempool_work *work;
};
static struct romempool_workqueue *rmp_wq;

#define NR_EXPAND_RETRY		10

static void expand_work(struct work_struct *ws)
{
	struct romempool_work *work;
	bool ret;
	int retry = 1;

	work = container_of(ws, struct romempool_work, work);

retry:
	ret = __romempool_expand();
	if (!ret) {
		if (!atomic_read(&expandable))
			goto done;
		else {
			/*
			 * Trying to expand the pool 10 times, but it
			 * failed because we couldn't get some memory
			 * for our data structures. Let's give up !
			 */
			if (retry > NR_EXPAND_RETRY) {
				atomic_set(&expandable, 0);
				goto done;
			}

			pr_warn("retry to expand the pool %d times\n",
				retry);
			retry++;
			msleep(1L << retry);
			goto retry;
		}
	}

done:
	atomic_dec(&in_expanding);

	work->end = ktime_get();

	pr_info("expanding the pool took %lld msecs\n",
		ktime_ms_delta(work->end, work->start));

	wake_up_interruptible_all(&rmp_wq->waitq);
}

/*
 * If the system has less than 4 GB physical memory, 3 emergency pages
 * are enough to expand the pool.
 */
#define NR_EMERGENCY_POOL	3
static struct page *emergency_pages[NR_EMERGENCY_POOL];
static unsigned int emerg_pool_index;

#if defined(CONFIG_WEBOS) && defined(CONFIG_LG_SNAPSHOT_BOOT) && \
    defined(CONFIG_EKP_ROMEMPOOL_USE_CMA_ALLOCATION)
extern unsigned int snapshot_enable;
#endif

static int __init romempool_wq_init(void)
{
	int flag, i;
	int default_expandable = 1;
	unsigned long addr;

	rmp_wq = kzalloc(sizeof(struct romempool_workqueue), GFP_KERNEL);
	if (!rmp_wq)
		goto err;

	flag = WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_FREEZABLE;
	rmp_wq->workq = alloc_workqueue("romempool", flag, 1);
	if (!rmp_wq->workq)
		goto err;

	init_waitqueue_head(&rmp_wq->waitq);

	rmp_wq->work = kmalloc(sizeof(struct romempool_work), GFP_KERNEL);
	if (!rmp_wq->work)
		goto err;

#if defined(CONFIG_WEBOS) && defined(CONFIG_LG_SNAPSHOT_BOOT) && \
    defined(CONFIG_EKP_ROMEMPOOL_USE_CMA_ALLOCATION)
	if (snapshot_enable)
		default_expandable = 0;
#endif
	atomic_set(&expandable, default_expandable);

	return 0;

err:
	if (rmp_wq) {
		if (rmp_wq->workq)
			destroy_workqueue(rmp_wq->workq);
		kfree(rmp_wq);
		rmp_wq = NULL;
	}

	/* Free emergency pages */
	for (i = 0; i < NR_EMERGENCY_POOL; i++) {
		addr = (unsigned long)page_address(emergency_pages[i]);
		romempool_free(addr, RMP_EMERGENCY, 0);
	}

	return -ENOMEM;
}
core_initcall(romempool_wq_init);
#endif	/* ROMEMPOOL_EXPANDABLE */

static unsigned long
romempool_alloc_from_emergency_pool(enum romempool_flags flag,
				    unsigned long order)
{
	void *p = NULL;

#ifdef ROMEMPOOL_EXPANDABLE
	if (!atomic_read(&in_expanding))
		return 0;
	if (flag != RMP_SPECIAL || order != 0)
		return 0;
	if (emerg_pool_index >= NR_EMERGENCY_POOL)
		return 0;

	p = __va(page_to_phys(emergency_pages[emerg_pool_index]));
	emerg_pool_index++;

	dec_romempool_stat(RMP_EMERGENCY, order);

	pr_debug("allocation is from emergency pool (free: %u pages)\n",
		 (NR_EMERGENCY_POOL - emerg_pool_index));
#endif

	return (unsigned long)p;
}

static int romempool_wait(int retry)
{
#ifdef ROMEMPOOL_EXPANDABLE
	wait_queue_t wait;

	if (!atomic_read(&expandable) || !retry)
		return 0;

	init_wait(&wait);
	prepare_to_wait(&rmp_wq->waitq, &wait, TASK_INTERRUPTIBLE);

	pr_debug("%s(%d) goes to sleep... (%d)\n", current->comm,
		 current->pid, retry);

	/* Wait 1 seconds unless no one wakes me up */
	schedule_timeout(1 * HZ);

	pr_debug("%s(%d) was woken up\n", current->comm, current->pid);

	finish_wait(&rmp_wq->waitq, &wait);

	return 1;
#else
	return 0;
#endif
}

#ifdef ROMEMPOOL_EXPANDABLE
#define DEFAULT_POOL_MIN_FREE_KB	256
#define DEFAULT_POOL_HIGH_FREE_KB \
	((PMD_SIZE / SZ_1K) + (DEFAULT_POOL_MIN_FREE_KB * 3))

/*
 * If free memory of the pool is less than pool_min_free_kbytes, we
 * enqueue a job to the workqueue for expanding the pool. On the other
 * hands, if free memory of the pool is more than 2 MB + pool_min_free
 * _kbytes * 3, pool can be shrinked if it's possible (when an empty
 * chunk exists).
 *
 * Note that "2 MB + pool_min_free_kbytes * 3" means we want to
 * guarantee that at least "pool_min_free_kbytes * 3" memory is available
 * in the pool when shrink logic removes 2 MB chunk.
 *
 * By default, pool_min_free_kbytes is set to 256 KB, but this is
 * configurable by sysfs entry. pool_high_free_kbytes is set to 2 MB +
 * pool_min_free_kbytes * 3 by default, but it is also configurable.
 */
static unsigned int pool_min_free_kbytes = DEFAULT_POOL_MIN_FREE_KB;
static unsigned int pool_high_free_kbytes = DEFAULT_POOL_HIGH_FREE_KB;
#endif

static void romempool_expand(void)
{
#ifdef ROMEMPOOL_EXPANDABLE
	struct romempool_work *work;

	/* We don't try to expand the pool if !expandable. */
	if (!atomic_read(&expandable))
		return;
	/* TODO: gen_pool_avail() might cause some performance impact. */
	if (gen_pool_avail(romempool) > (pool_min_free_kbytes * SZ_1K))
		return;
	/* There should be only one active work for pool expand. */
	if (!atomic_add_unless(&in_expanding, 1, 1))
		return;

	work = rmp_wq->work;

	INIT_WORK(&work->work, expand_work);
	work->start = ktime_get();

	pr_debug("expanding the pool...\n");

	queue_work(rmp_wq->workq, &work->work);
#endif
}

static void romempool_shrink(void)
{
#ifdef ROMEMPOOL_EXPANDABLE
	struct list_head *pos;
	struct romempool_chunk *chunk;
	int bit, end_bit;

	if (nr_chunk == 1)
		return;
	/* TODO: gen_pool_avail() might cause some performance impact. */
	if (gen_pool_avail(romempool) < (pool_high_free_kbytes * SZ_1K))
		return;

	list_for_each_prev(pos, &romempool_chunk_head) {
		chunk = list_entry(pos, struct romempool_chunk, list);

		if (chunk->type == RMP_CHUNK_RESERVED)
			continue;

		end_bit = __gen_pool_chunk_size(chunk->chunk);
		end_bit >>= romempool->min_alloc_order;
		bit = find_next_bit(chunk->chunk->bits, end_bit, 0);

		if (bit >= end_bit) {
			unsigned long start = chunk->start;
			enum romempool_chunk_type type = chunk->type;

			romempool_remove_chunk(chunk);
			free_expanded_memory(start, PMD_SIZE, type);
			break;
		}
	}
#endif
}

#define NR_ALLOC_RETRY	5

unsigned long *romempool_alloc(gfp_t gfpmask,
			       enum romempool_flags flag,
			       unsigned long order)
{
	unsigned long size = PAGE_SIZE << order;
	unsigned long addr;
	int retry = NR_ALLOC_RETRY;

	if (!romempool)
		return NULL;

	might_sleep_if(gfpmask & __GFP_DIRECT_RECLAIM);

repeat_alloc:
	addr = __gen_pool_alloc_locked(size);
	if (likely(addr))
		goto allocated;

	/*
	 * When the pool is out of memory, we can get some memory from
	 * the emergency pool only if all conditions for the pool are met.
	 */
	addr = romempool_alloc_from_emergency_pool(flag, order);
	if (addr)
		goto allocated;

	/* We cannot go to sleep if !__GFP_DIRECT_RECLAIM */
	if (!(gfpmask & __GFP_DIRECT_RECLAIM))
		goto fail;

	/*
	 * When we get here, the pool is out of memory and we cannot get
	 * some memory from the emergency pool. This means that we need to
	 * wait until the workqueue allocates a new chunk for the pool.
	 */
	if (romempool_wait(retry)) {
		retry--;
		goto repeat_alloc;
	}

fail:
#ifdef CONFIG_EKP_ROMEMPOOL_ALLOW_WRITABLE_MEMORY
	/*
	 * Despite of trying hard to allocate some memory from the pool,
	 * we couldn't make it. Now, we just allocates memory from the
	 * buddy allocator.
	 */
	pr_err("not enough memory for order %lu, flag %x\n", order,
	       gfpmask);
#ifdef ROMEMPOOL_EXPANDABLE
	pr_err("wait for expanding the pool %d times\n",
	       NR_ALLOC_RETRY - retry);
#endif

	atomic_add(1 << order, &romempool_out_of_memory_count);
#else	/* !CONFIG_EKP_ROMEMPOOL_ALLOW_WRITABLE_MEMORY */
	panic("romempool doesn't have enough memory for order %lu, flag %x\n",
	      order, gfpmask);
#endif
	return NULL;

allocated:
	if (gfpmask & __GFP_ZERO)
		ekp_clear_romem(__pa(addr), size);

	inc_romempool_stat(flag, order);

	romempool_expand();

#ifndef CONFIG_EKP_FORCE_ENABLE
	pr_debug("(alloc) addr 0x%lx size/order 0x%lx/%lu\n", addr,
		 size, order);
#endif

	return (unsigned long *)addr;
}

struct page *romempool_alloc_pages(gfp_t gfpmask,
				   enum romempool_flags flag,
				   unsigned long order)
{
	unsigned long *virt = romempool_alloc(gfpmask, flag, order);

	if (virt)
		return virt_to_page(virt);

	return NULL;
}

/* Should be checked that 'addr' is in the pool before getting here */
void romempool_free(unsigned long addr, enum romempool_flags flag,
		    unsigned long order)
{
	unsigned long size = PAGE_SIZE << order;

	write_lock(&chunk_list_lock);

	gen_pool_free(romempool, addr, size);
	romempool_shrink();

	write_unlock(&chunk_list_lock);

	dec_romempool_stat(flag, order);

#ifndef CONFIG_EKP_FORCE_ENABLE
	pr_debug("(free) addr 0x%lx order: %lu\n", addr, order);
#endif
}

#define EARLY_POOL_SIZE		(SZ_8M)

static unsigned long __initdata early_map_size = EARLY_POOL_SIZE / PAGE_SIZE;
static DECLARE_BITMAP(romempool_early_map, EARLY_POOL_SIZE / PAGE_SIZE) __initdata;

static unsigned long __init *__romempool_early_alloc(unsigned long order)
{
	unsigned long found = 0;
	unsigned long size = 1 << order;
	unsigned long *ret;

	if (romempool_initialized || !romempool_size)
		return NULL;

	found = bitmap_find_next_zero_area(romempool_early_map,
					   early_map_size, 0, size,
					   size - 1);
	if (found >= early_map_size)
		return NULL;

	bitmap_set(romempool_early_map, found, size);
	ret = (unsigned long *)(romempool_start + (found * PAGE_SIZE));

	inc_romempool_stat(RMP_SPECIAL, order);

#ifndef CONFIG_EKP_FORCE_ENABLE
	pr_debug("early alloc 0x%lx ~ 0x%lx\n",
		 romempool_start + (found * PAGE_SIZE),
		 romempool_start + ((found + size) * PAGE_SIZE));
#endif

	return ret;
}

unsigned long __init *romempool_early_alloc(unsigned long order)
{
	if (!ekp_initialized())
		return NULL;
	return __romempool_early_alloc(order);
}

void __init romempool_early_free(unsigned long addr, enum romempool_flags flag,
				 unsigned long order)
{
	unsigned long pos;
	unsigned long size;

	if (!ekp_initialized())
		return;
	if (romempool_initialized || !romempool_size)
		return;
	if (addr < romempool_start)
		return;
	if (addr >= (romempool_start + romempool_size))
		return;

	pos = (addr - romempool_start) >> PAGE_SHIFT;
	size = 1 << order;

	bitmap_clear(romempool_early_map, pos, size);

	dec_romempool_stat(flag, order);

#ifndef CONFIG_EKP_FORCE_ENABLE
	pr_debug("early free 0x%lx ~ 0x%lx\n",
		 romempool_start + (pos * PAGE_SIZE),
		 romempool_start + ((pos + size) * PAGE_SIZE));
#endif
}

#ifdef CONFIG_HIBERNATION
#if defined(CONFIG_WEBOS) && defined(CONFIG_LG_SNAPSHOT_BOOT) && \
    defined(CONFIG_EKP_ROMEMPOOL_USE_CMA_ALLOCATION)
static int romempool_pm_notify(struct notifier_block *nb,
			       unsigned long action, void *data)
{
	switch (action) {
	case PM_POST_HIBERNATION:
		if (rmp_wq)
			atomic_set(&expandable, 1);
		return NOTIFY_OK;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block romempool_pm_nb = {
        .notifier_call = romempool_pm_notify,
};
#endif

/* This function is called with rcu_read_lock held */
void __mark_free_pools(struct gen_pool *pool,
		       struct gen_pool_chunk *chunk, void *__unused)
{
	unsigned long found = 0;
	unsigned long size;
	unsigned long next_set_bit, bit;
	unsigned long virt;

	size = __gen_pool_chunk_size(chunk) >> PAGE_SHIFT;

	/* Clear the free_pages_map first */
	for (bit = 0; bit < size; bit++) {
		virt = chunk->start_addr + (bit * PAGE_SIZE);
		swsusp_unset_page_free(virt_to_page(virt));
	}

	/* Find the free bits and set them on the free_pages_map */
	while(1) {
		found = bitmap_find_next_zero_area(chunk->bits, size,
						   found, 1, 0);
		if (found >= size)
			break;

		next_set_bit = find_next_bit(chunk->bits, size, found);

#ifndef CONFIG_EKP_FORCE_ENABLE
		pr_debug("mark romempool 0x%lx ~ 0x%lx as free\n",
			 chunk->start_addr + (found * PAGE_SIZE),
			 chunk->start_addr + (next_set_bit * PAGE_SIZE));
#endif

		for (bit = found; bit < next_set_bit; bit++) {
			virt = chunk->start_addr + (bit * PAGE_SIZE);
			swsusp_set_page_free(virt_to_page(virt));
		}
		found = next_set_bit + 1;
	}
}

void mark_free_romempool(void)
{
	if (!romempool)
		return;

	gen_pool_for_each_chunk(romempool, __mark_free_pools, NULL);
}
#endif	/* CONFIG_HIBERNATION */

static unsigned long default_pool_size = RO_MEMPOOL_SIZE;
#ifdef CONFIG_EKP_ROMEMPOOL_USE_CMA_ALLOCATION
static unsigned long cma_pool_size = CONFIG_EKP_ROMEMPOOL_CMA_SIZE;
#endif

#ifndef CONFIG_EKP_FORCE_ENABLE
static int __init set_romempool_size(char *p)
{
	default_pool_size = memparse(p, &p);

	pr_info("overriding default pool size: %luMB\n",
		default_pool_size / SZ_1M);

#ifdef CONFIG_EKP_ROMEMPOOL_USE_CMA_ALLOCATION
	if (*p++ != ';')
		return 0;

	cma_pool_size = memparse(p, &p);

	pr_info("overriding cma pool size: %luMB\n",
		cma_pool_size / SZ_1M);
#endif

        return 0;
}
early_param("romempool", set_romempool_size);
#endif	/* !CONFIG_EKP_FORCE_ENABLE */

static bool __init check_reserve_area(unsigned long start,
				      unsigned long size)
{
	/* We don't allow zero-size pool */
	if (!size)
		return false;
	/* Should be aligned in PAGE_SIZE */
	if (!IS_ALIGNED(start, PMD_SIZE) || !IS_ALIGNED(size, PMD_SIZE))
		return false;
	/* Should be reserved for this purpose only */
	if (memblock_is_region_reserved(__pa(start), size))
		return false;

	return true;
}

static void __init romempool_cma_reserve(unsigned long start)
{
#ifdef CONFIG_EKP_ROMEMPOOL_USE_CMA_ALLOCATION
	unsigned long size = cma_pool_size;

	if (!check_reserve_area(start, size))
		return;

	cma_declare_contiguous(__pa(start), size, 0, PMD_SIZE,
			       PMD_SHIFT - PAGE_SHIFT, true,
			       &romempool_cma);
#endif
}

static bool __init __romempool_reserve(unsigned long start, unsigned long size)
{
	/* Only one memory pool can be reserved on boot-up time */
	if (romempool_size)
		return false;

	if (!check_reserve_area(start, size))
		return false;

	romempool_start = start;
	romempool_size = size;

	memblock_reserve(__pa(start), size);
	romempool_cma_reserve(start + size);

	return true;
}

static void __init romempool_static_pgtable_alloc(unsigned long size)
{
	unsigned long count = size / PAGE_SIZE;

	while (count) {
		__romempool_early_alloc(0);
		count--;
	}
}

void __init romempool_reserve(bool init)
{
	unsigned long start = (unsigned long)__romempool_start;
	unsigned long size = default_pool_size;
	unsigned long static_pgtab_size =
				(unsigned long)__static_pgtab_end - start;

	if (init && __romempool_reserve(start, size)) {
		romempool_static_pgtable_alloc(static_pgtab_size);
		romempool_free_init_pgtable_func = romempool_early_free;
		return;
	}

	memblock_reserve(__pa(start), static_pgtab_size);
}

static void __init reserve_early_romempool(void)
{
	struct gen_pool_chunk *chunk;
	unsigned long found = 0;
	unsigned long next_zero_bit;
	unsigned long nbits;

	/* Currently, only one chunk is available on boot-up time. */
	chunk = list_first_entry(&romempool_chunk_head,
				 struct romempool_chunk, list)->chunk;

	while(1) {
		found = find_next_bit(romempool_early_map,
				      early_map_size, found);
		if (found >= early_map_size)
			break;

		next_zero_bit = bitmap_find_next_zero_area(romempool_early_map,
							   early_map_size,
							   found + 1, 1, 0);

		nbits = next_zero_bit - found;
		bitmap_set(chunk->bits, found, nbits);
		atomic_sub(nbits * PAGE_SIZE, &chunk->avail);

#ifndef CONFIG_EKP_FORCE_ENABLE
		pr_info("reserved area 0x%lx ~ 0x%lx for early allocation\n",
			romempool_start + (found * PAGE_SIZE),
			romempool_start + (next_zero_bit * PAGE_SIZE));
#endif

		found = next_zero_bit + 1;
	}
}

static void __init reserve_emergency_pool(void)
{
#ifdef ROMEMPOOL_EXPANDABLE
	int i;

	for (i = 0; i < NR_EMERGENCY_POOL; i++)
		emergency_pages[i] = romempool_alloc_pages(GFP_KERNEL,
							   RMP_EMERGENCY,
							   0);
#endif
}

void __init romempool_init(void)
{
	int ret;

	/* Some memory spaces should be reserved before getting here */
	if (!romempool_size)
		return;

	romempool = gen_pool_create(PAGE_SHIFT, -1);
	BUG_ON(!romempool);

	gen_pool_set_algo(romempool, gen_pool_first_fit_order_align,
			  NULL);

	ret = romempool_add_chunk(romempool_start, romempool_size,
				  RMP_CHUNK_RESERVED);
	BUG_ON(ret != 0);

	reserve_early_romempool();
	reserve_emergency_pool();

	pr_info("initialized w/ genpool\n");

/*
 * In case of webOS, snapshot boot doesn't allow to save pages on
 * CMA zone. This means that we cannot expand the pool before making
 * the snapshot image.
 */
#if defined(CONFIG_WEBOS) && defined(CONFIG_LG_SNAPSHOT_BOOT) && \
    defined(CONFIG_EKP_ROMEMPOOL_USE_CMA_ALLOCATION)
	if (snapshot_enable)
		register_pm_notifier(&romempool_pm_nb);
#endif

	romempool_free_init_pgtable_func = romempool_free;
	romempool_initialized = true;
}

static void * __ref romempool_vmemmap_early_alloc(enum romempool_flags flag)
{
	void *ptr = NULL;

	if (!romempool_initialized) {
		ptr = (void *)romempool_early_alloc(0);
		if (ptr)
			memset(ptr, 0, PAGE_SIZE);
	}
	return ptr;
}

void * __meminit romempool_vmemmap_alloc_pgtable(enum romempool_flags flag)
{
	void *ptr;

	if (!ekp_initialized())
		return NULL;

	ptr = romempool_vmemmap_early_alloc(flag);
	if (!ptr)
		ptr = romempool_alloc(PGALLOC_GFP, flag, 0);

	return ptr;
}

/* /sys/kernel/mm/romempool */
#ifdef CONFIG_SYSFS
#define ROMEMPOOL_ATTR_RO(_name) \
	static struct kobj_attribute _name##_attr = \
		__ATTR(_name, 0400, _name##_show, NULL)

#define ROMEMPOOL_ATTR(_name) \
	static struct kobj_attribute _name##_attr = \
		__ATTR(_name, 0600, _name##_show, _name##_store)

static ssize_t total_size_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu MB\n", romempool_total_size / SZ_1M);
}
ROMEMPOOL_ATTR_RO(total_size);

static ssize_t max_size_show(struct kobject *kobj,
			     struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu MB\n", romempool_max_size / SZ_1M);
}
ROMEMPOOL_ATTR_RO(max_size);

#ifdef ROMEMPOOL_EXPANDABLE
static ssize_t expandable_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&expandable));
}
ROMEMPOOL_ATTR_RO(expandable);

static ssize_t min_free_kbytes_show(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    char *buf)
{
	return sprintf(buf, "%u\n", pool_min_free_kbytes);
}
static ssize_t min_free_kbytes_store(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned int min_free_kbytes;
	int err;

	err = kstrtouint(buf, 10, &min_free_kbytes);
	if (err || min_free_kbytes > UINT_MAX)
		return -EINVAL;

	pool_min_free_kbytes = min_free_kbytes;

	return count;
}
ROMEMPOOL_ATTR(min_free_kbytes);

static ssize_t high_free_kbytes_show(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    char *buf)
{
	return sprintf(buf, "%u\n", pool_high_free_kbytes);
}
static ssize_t high_free_kbytes_store(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned int high_free_kbytes;
	int err;

	err = kstrtouint(buf, 10, &high_free_kbytes);
	if (err || high_free_kbytes > UINT_MAX)
		return -EINVAL;

	pool_high_free_kbytes = high_free_kbytes;

	return count;
}
ROMEMPOOL_ATTR(high_free_kbytes);
#endif	/* ROMEMPOOL_EXPANDABLE */

#ifdef CONFIG_EKP_ROMEMPOOL_ALLOW_WRITABLE_MEMORY
static ssize_t oom_count_show(struct kobject *kobj,
			      struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",
		       atomic_read(&romempool_out_of_memory_count));
}
ROMEMPOOL_ATTR_RO(oom_count);
#endif

static struct attribute *romempool_attrs[] = {
	&total_size_attr.attr,
	&max_size_attr.attr,
#ifdef ROMEMPOOL_EXPANDABLE
	&expandable_attr.attr,
	&min_free_kbytes_attr.attr,
	&high_free_kbytes_attr.attr,
#endif
#ifdef CONFIG_EKP_ROMEMPOOL_ALLOW_WRITABLE_MEMORY
	&oom_count_attr.attr,
#endif
	NULL,
};

static struct attribute_group romempool_attr_group = {
	.attrs = romempool_attrs,
	.name = "romempool",
};

static int __init romempool_sysfs_init(void)
{
	return sysfs_create_group(mm_kobj, &romempool_attr_group);
}
late_initcall(romempool_sysfs_init);
#endif	/* CONFIG_SYSFS */

/* /sys/kernel/debug/romempool */
#if defined(CONFIG_DEBUG_FS) && defined(CONFIG_EKP_ROMEMPOOL_DEBUGFS)
static struct dentry *romempool_debugfs;

static unsigned int chunk_count;

/* This function is called with rcu_read_lock held */
static void __romempool_show(struct gen_pool *pool,
			     struct gen_pool_chunk *chunk, void *seq)
{
	unsigned long found = 0;
	unsigned long size;
	unsigned long next_set_bit;
	struct seq_file *m = (struct seq_file *)seq;

	size = __gen_pool_chunk_size(chunk) >> PAGE_SHIFT;

	seq_printf(m, "chunk #%02u 0x%lx ~ 0x%lx (%u/%lu KB)\n",
		   chunk_count, chunk->start_addr, chunk->end_addr + 1,
		   atomic_read(&chunk->avail) / SZ_1K,
		   (size << PAGE_SHIFT) / SZ_1K);

	/* Find the free bits and set them on the free_pages_map */
	while(1) {
		found = bitmap_find_next_zero_area(chunk->bits, size, found,
						   1, 0);
		if (found >= size)
			break;

		next_set_bit = find_next_bit(chunk->bits, size, found);

		seq_printf(m, "\t      0x%lx ~ 0x%lx (%lu free pages)\n",
			   chunk->start_addr + (found * PAGE_SIZE),
			   chunk->start_addr + (next_set_bit * PAGE_SIZE),
			   next_set_bit - found);

		found = next_set_bit + 1;
	}

	chunk_count++;
}

static inline int romempool_stat(enum romempool_flags flag)
{
	return atomic_read(&romempool_stats[flag]) * (PAGE_SIZE / SZ_1K);
}

static inline int romempool_stat_pgtables(void)
{
	return romempool_stat(RMP_PGD) + romempool_stat(RMP_PUD) +
	       romempool_stat(RMP_PMD) + romempool_stat(RMP_PTE_KERNEL) +
	       romempool_stat(RMP_PTE_USER);
}

/* This is just for debugging, so we don't need to get a lock. */
static int romempool_show(struct seq_file *m, void *v)
{
	unsigned long total, free;

	total = romempool_total_size / SZ_1K;
	free = gen_pool_avail(romempool) / SZ_1K;

	seq_printf(m,
		"Total:			%5lu KB\n"
		"Free:			%5lu KB\n"
#ifdef ROMEMPOOL_EXPANDABLE
		"Min/High Free:		%5u/%u KB\n"
#endif
		"Page Tables:		%5u KB\n"
		"  PGD:			  %5u KB\n"
		"  PUD:			  %5u KB\n"
		"  PMD:			  %5u KB\n"
		"  PTE(K):		  %5u KB\n"
		"  PTE(U):		  %5u KB\n"
		"Special Pages:		%5u KB\n"
		"Emergency Pages:	%5u KB\n"
		"Slab Pages:		%5u KB\n"
		"\n",
		total, free,
#ifdef ROMEMPOOL_EXPANDABLE
		pool_min_free_kbytes, pool_high_free_kbytes,
#endif
		romempool_stat_pgtables(),
		romempool_stat(RMP_PGD), romempool_stat(RMP_PUD),
		romempool_stat(RMP_PMD), romempool_stat(RMP_PTE_KERNEL),
		romempool_stat(RMP_PTE_USER), romempool_stat(RMP_SPECIAL),
		romempool_stat(RMP_EMERGENCY), romempool_stat(RMP_SLAB)
		);

	chunk_count = 1;
	gen_pool_for_each_chunk(romempool, __romempool_show, (void *)m);

	return 0;
}

static int status_open(struct inode *inode, struct file *file)
{
	return single_open(file, romempool_show, NULL);
}

static const struct file_operations status_fops = {
	.open		= status_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init romempool_debugfs_init(void)
{
	if (!debugfs_initialized() || !romempool)
		return -ENODEV;

	romempool_debugfs = debugfs_create_dir("romempool", NULL);
	if (IS_ERR_OR_NULL(romempool_debugfs)) {
		pr_err("cannot create debugfs entry\n");
		return -ENOMEM;
	}

	debugfs_create_file("status", S_IRUSR, romempool_debugfs, NULL,
			    &status_fops);

	pr_debug("debugfs initialized\n");

	return 0;
}
late_initcall(romempool_debugfs_init);
#endif	/* CONFIG_DEBUG_FS && CONFIG_EKP_ROMEMPOOL_DEBUGFS */
