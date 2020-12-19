/*
 * MRI Helper
 *
 * Copyright (C) 2017 LGE, Inc.
 *
 * Juneho Choi <juno.choi@lge.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "mri_helper: " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/taskstats_kern.h>
#include <linux/seq_file.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/atomic.h>
#include <linux/mm.h>
#include <linux/ktime.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/vmstat.h>
#include <linux/sysrq.h>
#include <linux/writeback.h>
#include <linux/eventfd.h>
#include <linux/list_sort.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/low-mem-notify.h>

#include <linux/psi.h>

#define DEBUG			(1)

#define MRI_BUF_DYNAMIC
#define MRI_BUF_SIZE		(256)
#define MRI_BUF_MASK		(MRI_BUF_SIZE - 1)

/* copied from mm/vmstat.c please be careful */
enum writeback_stat_item {
	NR_DIRTY_THRESHOLD = 0,
	NR_DIRTY_BG_THRESHOLD,
	NR_VM_WRITEBACK_STAT_ITEMS,
};

#define NR_VM_STAT_ITEMS (NR_VM_ZONE_STAT_ITEMS + NR_VM_WRITEBACK_STAT_ITEMS)
struct mri_stats_item {
	u64 time;
	long long delays;
	int score;
	int count;
	struct usable_info usable;
	unsigned long vmstat[NR_VM_STAT_ITEMS];
#ifdef CONFIG_VM_EVENT_COUNTERS
	unsigned long _vmstat[NR_VM_EVENT_ITEMS];
#endif
	unsigned long nr_free[MAX_NR_ZONES][MAX_ORDER];
	unsigned long migrate_types[MAX_NR_ZONES][MAX_ORDER];
	unsigned long nr_reserved_highatomic[MAX_NR_ZONES];
#ifdef CONFIG_MEMORY_ISOLATION
	unsigned long nr_isolate_pageblock[MAX_NR_ZONES];
#endif
	int zone_dirty_ok[MAX_NR_ZONES];
	unsigned long zram_stats[NR_ZRAM_STATS_ITEMS];
	unsigned long zram_comp_ratio;
};

struct mri_stat_circ {
	int head;
	int tail;
#ifdef MRI_BUF_DYNAMIC
	struct mri_stats_item *buffer;
#else
	struct mri_stats_item buffer[MRI_BUF_SIZE];
#endif
} mri_stats;

#define mri_stats_circ_empty(circ)	((circ)->head == (circ)->tail)
#define mri_stats_circ_clear(circ)	((circ)->head = (circ)->tail = 0)

#define mri_stats_circ_pending(circ)	\
	(CIRC_CNT((circ)->head, (circ)->tail, MRI_BUF_SIZE))

#define mri_stats_circ_free(circ)	\
	(CIRC_SPACE((circ)->head, (circ)->tail, MRI_BUF_SIZE))

#define mri_stats_circ_head_inc(circ)	\
	((circ)->head = ((circ)->head + 1) & MRI_BUF_MASK)

#define mri_stats_circ_tail_inc(circ)	\
	((circ)->tail = ((circ)->tail + 1) & MRI_BUF_MASK)

/* Milliseconds mrid should sleep between batches */
static unsigned int mri_sleep_millisecs = 500;

static struct task_struct *mri_thread;

static atomic_t mri_count;
static DEFINE_MUTEX(mri_lock);

static atomic_t mri_freeze;

static DEFINE_PER_CPU(long long, freepages_delay);
static atomic64_t freepages_delay_total;
static atomic64_t freepages_delay_prev;

static bool mri_helper_disabled = false;
static int early_mri_helper_param(char *buf)
{
	mri_helper_disabled = true;
	return 0;
}
early_param("mri_helper_disabled", early_mri_helper_param);

void free_pages_delay_add(long long delay)
{
	this_cpu_add(freepages_delay, delay);
}
EXPORT_SYMBOL(free_pages_delay_add);

static void sum_freepages_delay_cpu(int cpu)
{
	long long  *delay;

	preempt_disable();
	delay = per_cpu_ptr(&freepages_delay, cpu);
	atomic64_add(*delay, &freepages_delay_total);
	*delay = 0;
	preempt_enable();
}

static void sum_freepages_delay_total(void)
{
	int cpu;

	get_online_cpus();
	for_each_online_cpu(cpu)
		sum_freepages_delay_cpu(cpu);
	put_online_cpus();
}

static long long get_freepages_delay(void)
{
	sum_freepages_delay_total();
	return atomic64_read(&freepages_delay_total);
}

enum mri_event_type {
	MRI_EVENT_LEVEL = 0,
	MRI_EVENT_NR_ZSPAGES,
	MRI_EVENT_ZRAMCOMP_RATIO,
	MRI_EVENT_NR_UNEVICTABLE,
	MRI_EVENT_LOWMEM_LEVEL,
#ifdef CONFIG_PSI
	MRI_EVENT_PSI_CPU,
	MRI_EVENT_PSI_MEM,
#endif
	MRI_NUM_EVENTS,
};

enum mri_levels {
	MRI_LOW = 0,
	MRI_MEDIUM,
	MRI_HIGH,
	MRI_CRITICAL,
	MRI_NUM_LEVELS,
};

enum mri_lowmem_level {
	MRI_LOWMEM_CRITICAL = 0,
	MRI_LOWMEM_LOW,
	MRI_LOWMEM_MEDIUM,
	MRI_LOWMEM_NORMAL,
	MRI_NUM_LOWMEM_LEVELS,
};

#ifdef CONFIG_PSI
enum mri_psi_level {
	MRI_PSI_LOW = 0,
	MRI_PSI_MID,
	MRI_PSI_HIGH,
	MRI_NUM_PSI_LEVELS,
};
#endif

static int mri_ratio_levels[] = {
	[MRI_LOW]	= 0,
	[MRI_MEDIUM]	= 50,
	[MRI_HIGH]	= 100,
	[MRI_CRITICAL]	= 400,
};

static int mri_lowmem_threshold[] = {
	[MRI_LOWMEM_CRITICAL]	= 512,
	[MRI_LOWMEM_LOW]	= 1280,
	[MRI_LOWMEM_MEDIUM]	= 1920,
	[MRI_LOWMEM_NORMAL]	= 2560,
};

static const char * const mri_lowmem_str_levels[] = {
	[MRI_LOWMEM_CRITICAL]	= "critical",
	[MRI_LOWMEM_LOW]	= "low",
	[MRI_LOWMEM_MEDIUM]	= "medium",
	[MRI_LOWMEM_NORMAL]	= "normal",
};

static const char * const mri_str_levels[] = {
	[MRI_LOW]	= "low",
	[MRI_MEDIUM]	= "medium",
	[MRI_HIGH]	= "high",
	[MRI_CRITICAL]	= "critical",
};

static unsigned long total_scale;

static enum mri_levels mri_level(unsigned long pressure)
{
	int level;

	for (level = MRI_CRITICAL; level > 0; level--) {
		if (pressure >= mri_ratio_levels[level])
			return level;
	}
	return MRI_LOW;
}

static enum mri_levels mri_calc_level(long long delays)
{
	unsigned long pressure = delays;

	do_div(pressure, NSEC_PER_MSEC);
	pressure = pressure * 100 / total_scale;

	pr_debug("%s: %3lu  (s: %lu)\n", __func__, pressure, total_scale);

	return mri_level(pressure);
}

static enum mri_lowmem_level mri_calc_lowmem_level(void)
{
	struct zone *z;
	int i;
	unsigned long threshold;
	int wmark_level = MRI_NUM_LOWMEM_LEVELS;
	int wmark_idx;
	int vm_lvl = 0;

	/* low mem reserved threshold */
	unsigned long wmark;
	for_each_populated_zone(z) {
		unsigned long free_pages = global_page_state(NR_FREE_PAGES);
		wmark = min_wmark_pages(z);
		/* watermark */
		for (i = 0; i < MAX_NR_ZONES; i++) {
			free_pages -= z->nr_reserved_highatomic;
			for (wmark_idx = 0; wmark_idx < MRI_NUM_LOWMEM_LEVELS; wmark_idx++) {
				threshold = wmark + z->lowmem_reserve[i] + mri_lowmem_threshold[wmark_idx];
				if (free_pages >= threshold)
					vm_lvl = wmark_idx;
			}
			wmark_level = vm_lvl;
		}
#ifdef CONFIG_ARM64
		if(zone_idx(z) == ZONE_DMA) {
#else
		if(zone_idx(z) == ZONE_NORMAL) {
#endif
			return wmark_level;
		}
	}
	return MRI_LOWMEM_NORMAL;
}

struct mri_event {
	struct eventfd_ctx *efd;
	int type;
	int level;
	struct list_head list;
};

struct mri_event_list {
	struct list_head list;
	struct mutex lock;	/* mutex lock for event list */
	int curr;
};

struct mri_event_list mri_event_lists[MRI_NUM_EVENTS];

static void mri_event_level(long long delays)
{
	struct mri_event_list *ev_lst = &mri_event_lists[MRI_EVENT_LEVEL];
	struct mri_event *ev;
	int level = mri_calc_level(delays);

	if (!delays) {
		delays = get_freepages_delay();
		delays -= atomic64_read(&freepages_delay_prev);
	}

	if (list_empty(&ev_lst->list) || ev_lst->curr == level)
		return;

	pr_debug("mri_helper: %s -> %s\n",
		 mri_str_levels[ev_lst->curr],
		 mri_str_levels[level]);

	mutex_lock(&ev_lst->lock);
	list_for_each_entry(ev, &ev_lst->list, list) {
		/* level up */
		if (ev->level > ev_lst->curr && ev->level <= level) {
			eventfd_signal(ev->efd, 1);
			continue;
		}
		/* level down */
		if (ev->level < ev_lst->curr && ev->level >= level)
			eventfd_signal(ev->efd, 1);
	}
	ev_lst->curr = level;
	mutex_unlock(&ev_lst->lock);
}

static void mri_event_zramcomp_ratio(int ratio)
{
	struct mri_event_list *ev_lst;
	struct mri_event *ev;

	if (!ratio) {
		unsigned long zram_stats[NR_ZRAM_STATS_ITEMS];

		low_mem_notify_get_zram_stats(zram_stats);
		if (!zram_stats[ZRAM_STATS_ORIG_DATA]) {
			ratio = 0;
		} else {
			ratio = (zram_stats[ZRAM_STATS_COMP_DATA] * 100) /
				zram_stats[ZRAM_STATS_ORIG_DATA];
			ratio = 100 - (ratio);
		}
	}

	ev_lst = &mri_event_lists[MRI_EVENT_ZRAMCOMP_RATIO];
	if (list_empty(&ev_lst->list))
		return;

	pr_debug("zramcomp_ratio: curr = %d : %d\n",
		 ev_lst->curr, ratio);

	mutex_lock(&ev_lst->lock);
	list_for_each_entry(ev, &ev_lst->list, list) {
		if (ratio < ev->level) {
			if (ev->level != ev_lst->curr) {
				eventfd_signal(ev->efd, 1);
				ev_lst->curr = ev->level;
			}
			break;
		}
	}
	mutex_unlock(&ev_lst->lock);
}

#ifdef CONFIG_PSI
static void mri_event_psi(void)
{

	struct mri_event_list *ev_lst_cpu = &mri_event_lists[MRI_EVENT_PSI_CPU];
	struct mri_event *ev_cpu, *pos_cpu = NULL;

	struct mri_event_list *ev_lst_mem = &mri_event_lists[MRI_EVENT_PSI_MEM];
	struct mri_event *ev_mem, *pos_mem = NULL;

	unsigned long avg[NR_PSI_STATES - 1][3] = {0,};
	u64 total[NR_PSI_STATES - 1] = {0, };
	int i;
	int psi_stall_mem = 0;
	int psi_stall_cpu = 0;
	int ret = 0;

	ret = get_psi_info(avg, total);
	if (ret)
		return;

	psi_stall_mem = LOAD_INT(avg[PSI_MEM_SOME][0]);
	psi_stall_cpu = LOAD_INT(avg[PSI_CPU_SOME][0]);

	for (i = 0; i < PSI_NONIDLE; i++) {
		total[i] = div_u64(total[i], NSEC_PER_USEC);
		pr_debug("PSI: [%d] avg: %lu.%02lu, total:%llu \n",
			i, LOAD_INT(avg[i][0]), LOAD_FRAC(avg[i][0]), total[i]);
	}

	if (!list_empty(&ev_lst_cpu->list)) {
		mutex_lock(&ev_lst_cpu->lock);

		list_for_each_entry(ev_cpu, &ev_lst_cpu->list, list) {
			if (psi_stall_cpu < ev_cpu->level)
				break;
			pos_cpu = ev_cpu;
		}

		if (pos_cpu) {
			if (pos_cpu->level != ev_lst_cpu->curr) {
				pr_debug("mri_event_psi_cpu: %d\n", pos_cpu->level);
				ev_lst_cpu->curr = pos_cpu->level;
				eventfd_signal(pos_cpu->efd, 1);
			}
		}

		mutex_unlock(&ev_lst_cpu->lock);
	}

	if (!list_empty(&ev_lst_mem->list)) {
		mutex_lock(&ev_lst_mem->lock);

		list_for_each_entry(ev_mem, &ev_lst_mem->list, list) {
			if (psi_stall_mem < ev_mem->level)
				break;
			pos_mem = ev_mem;
		}

		if (pos_mem) {
			if (pos_mem->level != ev_lst_mem->curr) {
				pr_debug("mri_event_psi_mem: %d\n", pos_mem->level);
				ev_lst_mem->curr = pos_mem->level;
				eventfd_signal(pos_mem->efd, 1);
			}
		}

		mutex_unlock(&ev_lst_mem->lock);
	}
}
#else
static void mri_event_psi(void) {};
#endif

static void mri_event_lowmem_level(void)
{
	struct mri_event_list *ev_lst = &mri_event_lists[MRI_EVENT_LOWMEM_LEVEL];
	struct mri_event *ev;
	int level = mri_calc_lowmem_level();

	if (list_empty(&ev_lst->list) || ev_lst->curr == level)
		return;

	pr_debug("mri_helper: %s -> %s\n",
		 mri_lowmem_str_levels[ev_lst->curr],
		 mri_lowmem_str_levels[level]);

	mutex_lock(&ev_lst->lock);
	list_for_each_entry(ev, &ev_lst->list, list) {
		/* level up */
		if (ev->level > ev_lst->curr && ev->level <= level) {
			eventfd_signal(ev->efd, 1);
			continue;
		}
		/* level down */
		if (ev->level < ev_lst->curr && ev->level >= level)
			eventfd_signal(ev->efd, 1);
	}
	ev_lst->curr = level;
	mutex_unlock(&ev_lst->lock);
}

static void mri_event_nr_unevictable(int nr_unevictable)
{
	struct mri_event_list *ev_lst;
	struct mri_event *ev, *pos = NULL;

	if (!nr_unevictable)
		nr_unevictable = global_page_state(NR_UNEVICTABLE) -
			global_page_state(NR_MLOCK);

	ev_lst = &mri_event_lists[MRI_EVENT_NR_UNEVICTABLE];
	if (list_empty(&ev_lst->list))
		return;

	pr_debug("nr_unevictable: curr = %d : %d\n",
		 ev_lst->curr, nr_unevictable);

	mutex_lock(&ev_lst->lock);

	list_for_each_entry(ev, &ev_lst->list, list) {
		if (nr_unevictable < ev->level)
			break;
		pos = ev;
	}

	if (pos) {
		if (pos->level != ev_lst->curr) {
			pr_debug("mri_event_nr_unevictable: %d\n", pos->level);
			ev_lst->curr = pos->level;
			eventfd_signal(pos->efd, 1);
		}
	}

	mutex_unlock(&ev_lst->lock);
}

static void mri_event_nr_zspages(int nr_zspages)
{
	struct mri_event_list *ev_lst;
	struct mri_event *ev, *pos = NULL;

	if (!nr_zspages)
		nr_zspages = global_page_state(NR_ZSPAGES);

	ev_lst = &mri_event_lists[MRI_EVENT_NR_ZSPAGES];
	if (list_empty(&ev_lst->list))
		return;

	pr_debug("nr_zspages: curr = %d : %d\n",
		 ev_lst->curr, nr_zspages);

	mutex_lock(&ev_lst->lock);

	list_for_each_entry(ev, &ev_lst->list, list) {
		if (nr_zspages < ev->level)
			break;
		pos = ev;
	}

	if (pos) {
		if (pos->level != ev_lst->curr) {
			pr_debug("mri_event_nr_zspages: %d\n", pos->level);
			ev_lst->curr = pos->level;
			eventfd_signal(pos->efd, 1);
		}
	}

	mutex_unlock(&ev_lst->lock);
}

static void mri_event_work(struct mri_stats_item *item)
{
	if (item) {
		mri_event_level(item->delays);
		mri_event_nr_zspages(item->vmstat[NR_ZSPAGES]);
		mri_event_zramcomp_ratio(item->zram_comp_ratio);
		mri_event_nr_unevictable(global_page_state(NR_UNEVICTABLE) -
				global_page_state(NR_MLOCK));
		mri_event_lowmem_level();
		mri_event_psi();
	} else {
		mri_event_level(0);
		mri_event_nr_zspages(0);
		mri_event_zramcomp_ratio(0);
		mri_event_nr_unevictable(0);
		mri_event_lowmem_level();
		mri_event_psi();
	}
}

static int mri_check_event_registered(struct eventfd_ctx *efd)
{
	struct mri_event_list *ev_lst;
	struct mri_event *ev;
	int type;

	for (type = 0; type < MRI_NUM_EVENTS; type++) {
		ev_lst = &mri_event_lists[type];

		mutex_lock(&ev_lst->lock);
		list_for_each_entry(ev, &ev_lst->list, list) {
			if (ev->efd == efd) {
				mutex_unlock(&ev_lst->lock);
				return 1;
			}
		}
		mutex_unlock(&ev_lst->lock);
	}

	return 0;
}

static int _event_cmp(void *priv, struct list_head *a, struct list_head *b)
{
	struct mri_event *eva, *evb;

	eva = (struct mri_event *)container_of(a, struct mri_event, list);
	evb = (struct mri_event *)container_of(b, struct mri_event, list);

	return (int)(eva->level - evb->level);
}

static void mri_reset_events(void)
{
	struct mri_event_list *ev_lst;
	struct mri_event *ev, *tmp;
	int type;

	for (type = 0; type < MRI_NUM_EVENTS; type++) {
		ev_lst = &mri_event_lists[type];

		mutex_lock(&ev_lst->lock);
		list_for_each_entry_safe(ev, tmp, &ev_lst->list, list) {
			list_del(&ev->list);
			kfree(ev);
		}
		mutex_unlock(&ev_lst->lock);
	}
}

static int mri_register_event(struct eventfd_ctx *efd, int type, char *buf)
{
	struct mri_event_list *ev_lst;
	struct mri_event *new;
	int level;

	switch (type) {
	case MRI_EVENT_LOWMEM_LEVEL:
		for (level = 0; level < MRI_NUM_LOWMEM_LEVELS; level++)
			if (!strcmp(mri_lowmem_str_levels[level], buf))
				break;

		if (level >= MRI_NUM_LOWMEM_LEVELS)
			return -EINVAL;

		break;

	case MRI_EVENT_LEVEL:
		for (level = 0; level < MRI_NUM_LEVELS; level++)
			if (!strcmp(mri_str_levels[level], buf))
				break;

		if (level >= MRI_NUM_LEVELS)
			return -EINVAL;

		break;

	case MRI_EVENT_NR_ZSPAGES:
	case MRI_EVENT_ZRAMCOMP_RATIO:
	case MRI_EVENT_NR_UNEVICTABLE:
#ifdef CONFIG_PSI
	case MRI_EVENT_PSI_CPU:
	case MRI_EVENT_PSI_MEM:
#endif
		if (kstrtoint(buf, 10, &level))
			return -EINVAL;

		break;
	default:
		return -EINVAL;
	}

	new = kzalloc(sizeof(*new), GFP_KERNEL);
	if (!new)
		return -ENOMEM;

	new->efd = efd;
	new->type = type;
	new->level = level;
	INIT_LIST_HEAD(&new->list);
	pr_debug("mri_register_event: type = %d, level = %d\n", type, level);

	ev_lst = &mri_event_lists[type];

	mutex_lock(&ev_lst->lock);
	list_add_tail(&new->list, &ev_lst->list);
	if (type != MRI_EVENT_LEVEL && type != MRI_EVENT_LOWMEM_LEVEL)
		list_sort(NULL, &ev_lst->list, _event_cmp);
	mutex_unlock(&ev_lst->lock);

	return 0;
}

static void mri_unregister_event(struct eventfd_ctx *efd)
{
	struct mri_event_list *ev_lst;
	struct mri_event *ev, *tmp;
	int type;

	for (type = 0; type < MRI_NUM_EVENTS; type++) {
		ev_lst = &mri_event_lists[type];

		mutex_lock(&ev_lst->lock);
		list_for_each_entry_safe(ev, tmp, &ev_lst->list, list) {
			if (ev->efd == efd) {
				list_del(&ev->list);
				kfree(ev);
				break;
			}
		}
		mutex_unlock(&ev_lst->lock);
	}
}

static void mri_reset_profile(void)
{
	mutex_lock(&mri_lock);
	atomic_set(&mri_count, 0);
	mri_stats_circ_clear(&mri_stats);
	mutex_unlock(&mri_lock);
}

#define MRI_SCORE_UNITSIZE		(100 * NSEC_PER_MSEC)
#define MRI_TRIGGER_CONST		(4)	/* 25% */
#define MRI_WEIGHT_CONST		(1)	/* 100% */

static int mri_trigger_const = MRI_TRIGGER_CONST;
static int mri_weight_const = MRI_WEIGHT_CONST;

static inline int __mri_get_score(long long delays)
{
	do_div(delays, MRI_SCORE_UNITSIZE);
	return delays;
}

static inline bool mri_trigger(int score)
{
	return (score * mri_trigger_const) > total_scale;
}

static inline bool mri_weight(int score)
{
	return (score * mri_weight_const) > total_scale;
}

static void __mri_do_profile(struct mri_stats_item *item, long long delays)
{
	struct zone *zone;
	unsigned long *vmstat = item->vmstat;
	unsigned long *zram_stats = item->zram_stats;
	unsigned long zram_comp_ratio;
	int i;

	memset(item, 0, sizeof(struct mri_stats_item));

	item->time = ktime_get_ns();
	item->delays = delays;
	item->score = __mri_get_score(delays);
	item->count = atomic_read(&mri_count);
	get_usable_info(&item->usable);

	for_each_populated_zone(zone) {
		unsigned int order;
		unsigned long flags;
		unsigned int zi = zone_idx(zone);
		unsigned long *nr_free = item->nr_free[zi];
		unsigned long *types = item->migrate_types[zi];

		spin_lock_irqsave(&zone->lock, flags);
		for (order = 0; order < MAX_ORDER; order++) {
			struct free_area *area = &zone->free_area[order];
			int type;

			nr_free[order] = area->nr_free;

			types[order] = 0;
			for (type = 0; type < MIGRATE_TYPES; type++) {
				if (!list_empty(&area->free_list[type]))
					types[order] |= 1 << type;
			}
		}
		spin_unlock_irqrestore(&zone->lock, flags);
		item->nr_reserved_highatomic[zi] = zone->nr_reserved_highatomic;
		#ifdef CONFIG_MEMORY_ISOLATION
		item->nr_isolate_pageblock[zi] = zone->nr_isolate_pageblock;
		#endif
		item->zone_dirty_ok[zi] = (int)zone_dirty_ok(zone);
	}

	/* vm zone stat */
	for (i = 0; i < NR_VM_ZONE_STAT_ITEMS; i++)
		vmstat[i] = global_page_state(i);
	vmstat += NR_VM_ZONE_STAT_ITEMS;

	/* dirty limits */
	global_dirty_limits(vmstat + NR_DIRTY_BG_THRESHOLD,
			    vmstat + NR_DIRTY_THRESHOLD);
	vmstat += NR_VM_WRITEBACK_STAT_ITEMS;

#ifdef CONFIG_VM_EVENT_COUNTERS
	/* vm events */
	all_vm_events(vmstat);
	vmstat[PGPGIN] /= 2;         /* sectors -> kbytes */
	vmstat[PGPGOUT] /= 2;
#endif
	/* zram stats */
	low_mem_notify_get_zram_stats(zram_stats);
	if (!zram_stats[ZRAM_STATS_ORIG_DATA]) {
		zram_comp_ratio = 0;
	} else {
		zram_comp_ratio = (zram_stats[ZRAM_STATS_COMP_DATA] * 100) /
			zram_stats[ZRAM_STATS_ORIG_DATA];
		zram_comp_ratio = 100 - (zram_comp_ratio);
	}
	item->zram_comp_ratio = zram_comp_ratio;
}

static struct mri_stats_item *mri_do_profile(long long delays)
{
	struct mri_stats_item *item;

	mutex_lock(&mri_lock);

	/* insert one item into the buffer */
	item = &mri_stats.buffer[mri_stats.head];
	__mri_do_profile(item, delays);

	mri_stats_circ_head_inc(&mri_stats);
	if (mri_stats_circ_empty(&mri_stats))
		mri_stats_circ_tail_inc(&mri_stats);

	mutex_unlock(&mri_lock);

	return item;
}

static struct mri_stats_item *mri_profile_work(long long delays)
{
	int score = __mri_get_score(delays);

	if (atomic_read(&mri_count) > 0) {
		if (mri_weight(score))
			atomic_inc(&mri_count);
		else
			atomic_dec(&mri_count);

		return mri_do_profile(delays);
	}

	if (mri_trigger(score)) {
		atomic_set(&mri_count, score);
		return mri_do_profile(delays);
	}

	return NULL;
}

static int mri_kthread(void *nothing)
{
	long long cur_delays, delays;
	bool frozen;
	struct mri_stats_item *item;
	unsigned long msecs = msecs_to_jiffies(mri_sleep_millisecs);

	set_freezable();
	set_user_nice(current, 5);

	pr_info("mri thread run\n\n\n");
	atomic64_set(&freepages_delay_prev, get_freepages_delay());

	while (!kthread_freezable_should_stop(&frozen)) {
		if (frozen)
			mri_reset_profile();

		while (atomic_read(&mri_freeze))
			freezable_schedule_timeout_interruptible(msecs);

		cur_delays = get_freepages_delay();
		delays = cur_delays - atomic64_read(&freepages_delay_prev);

		item = mri_profile_work(delays);
		mri_event_work(item);

		atomic64_set(&freepages_delay_prev, cur_delays);
		freezable_schedule_timeout_interruptible(msecs);
	}
	return 0;
}

static int mri_delay_cpu_notify(struct notifier_block *self,
				unsigned long action, void *hcpu)
{
	int cpu = (unsigned long)hcpu;

	if (action == CPU_DEAD || action == CPU_DEAD_FROZEN)
		sum_freepages_delay_cpu(cpu);

	return NOTIFY_OK;
}

static struct notifier_block __refdata mri_notifier = {
	.notifier_call = mri_delay_cpu_notify,
};

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *mri_proc_dir;

static ssize_t mri_high_order_alloc_proc_write(struct file *file,
					       const char __user *buffer,
					       size_t count, loff_t *pos)
{
	unsigned int order;
	struct page *page;
	int ret;

	ret = kstrtou32_from_user(buffer, count, 10, &order);
	if (ret < 0)
		return ret;

	if (order > 10)
		return -EINVAL;

	pr_info("order %d page(s) are allocated\n", order);
	page = alloc_pages(GFP_KERNEL, order);
	__free_pages(page, order);

	return count;
}

static const struct file_operations mri_high_order_alloc_proc_fops = {
	.write		= mri_high_order_alloc_proc_write,
};

static int mri_delays_show(struct seq_file *m, void *v)
{
	seq_printf(m, "freepages delay total: %lld\n", get_freepages_delay());
	return 0;
}

static int mri_delays_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mri_delays_show, 0);
}

static const struct file_operations mri_delays_proc_fops = {
	.open		= mri_delays_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void *mri_stats_start(struct seq_file *m, loff_t *pos)
	__acquires(&mri_lock)
{
	struct mri_stats_item *item = NULL;

	mutex_lock(&mri_lock);
	if (mri_stats_circ_pending(&mri_stats)) {
		item = &mri_stats.buffer[mri_stats.tail];
		mri_stats_circ_tail_inc(&mri_stats);
	}
	return (void *)item;
}

static void *mri_stats_next(struct seq_file *m, void *arg, loff_t *pos)
{
	struct mri_stats_item *item = NULL;

	(*pos)++;
	return NULL;

	if (mri_stats_circ_pending(&mri_stats)) {
		mri_stats_circ_tail_inc(&mri_stats);
		item = &mri_stats.buffer[mri_stats.tail];
	}
	return (void *)item;
}

static void mri_stats_stop(struct seq_file *m, void *arg)
	__releases(&mri_lock)
{
	mutex_unlock(&mri_lock);
}

#define K(x) ((x) << (PAGE_SHIFT - 10))

#define mri_printf(m, fmt, args...)		\
do {						\
	if (m)					\
		seq_printf(m, fmt, ## args);	\
	else					\
		printk(fmt, ## args);		\
} while (0)

static void show_migrate_types(struct seq_file *m, unsigned char type)
{
	static const char types[MIGRATE_TYPES] = {
		[MIGRATE_UNMOVABLE]	= 'U',
		[MIGRATE_MOVABLE]	= 'M',
		[MIGRATE_RECLAIMABLE]	= 'E',
		[MIGRATE_HIGHATOMIC]	= 'H',
#ifdef CONFIG_MEMORY_ISOLATION
		[MIGRATE_ISOLATE]	= 'I',
#endif
	};
	char tmp[MIGRATE_TYPES + 1];
	char *p = tmp;
	int i;

	for (i = 0; i < MIGRATE_TYPES; i++) {
		if (type & (1 << i))
			*p++ = types[i];
	}

	*p = '\0';
	mri_printf(m, "(%s) ", tmp);
}

static const char * const wmark_strs[] = {
	[WMARK_MIN]	= "min",
	[WMARK_LOW]	= "low",
	[WMARK_HIGH]	= "high",
};

static const char * const zram_stats_strs[] = {
	[ZRAM_STATS_COMP_DATA]	= "comp_data_size",
	[ZRAM_STATS_ORIG_DATA]	= "orig_data_size",
	[ZRAM_STATS_MAX_USED]	= "mem_used_max",
	[ZRAM_STATS_ZERO_PAGES]	= "zero_pages",
};

static int mri_stats_show(struct seq_file *m, void *arg)
{
	struct mri_stats_item *item = arg;
	struct usable_info *ui = &item->usable;
	unsigned long *vmstat = &item->vmstat[0];
	unsigned long *zram_stats = &item->zram_stats[0];
	struct zone *z;
	int i, stat_items_size;

	mri_printf(m, "=============================================\n");
	mri_printf(m, "time %llu\n", item->time);
	mri_printf(m, "delays %llu\n", item->delays);
	mri_printf(m, "score %d\n", item->score);
	mri_printf(m, "count %d\n", item->count);

	/* usable */
	mri_printf(m, "usable %ld\n", ui->usable);
	mri_printf(m, "free %ld\n", ui->free);
	mri_printf(m, "swap %ld\n", ui->swap);
	mri_printf(m, "file %ld\n", ui->file);
	mri_printf(m, "swapcache %ld\n", ui->swapcache);

	/* vmstat */
	stat_items_size = NR_VM_ZONE_STAT_ITEMS + NR_VM_WRITEBACK_STAT_ITEMS;
#ifdef CONFIG_VM_EVENT_COUNTERS
	stat_items_size += NR_VM_EVENT_ITEMS;
#endif
	for (i = 0; i < stat_items_size; i++)
		mri_printf(m, "%s %lu\n", vmstat_text[i], vmstat[i]);

	/* nr free foreach zone */
	for_each_populated_zone(z) {
		struct pglist_data *pgdat = z->zone_pgdat;
		unsigned int idx = zone_idx(z);
		unsigned long *nr_free = item->nr_free[idx];
		unsigned long *types = item->migrate_types[idx];
		unsigned long total = 0;
		int order, wmark_level;

		mri_printf(m, "Node_%d_zone_%s: ", pgdat->node_id, z->name);
		for (order = 0; order < MAX_ORDER; ++order) {
			total += nr_free[order] << order;
			mri_printf(m, "%lu*%lukB ", nr_free[order],
				   K(1UL) << order);
			if (nr_free[order])
				show_migrate_types(m, types[order]);
		}
		mri_printf(m, "= %lukB\n", K(total));
		mri_printf(m, "nr_free %lu\n", total);

		mri_printf(m, "nr_reserved_highatomic %lu\n",
			   item->nr_reserved_highatomic[idx]);
		#ifdef CONFIG_MEMORY_ISOLATION
		mri_printf(m, "nr_isolate_pageblock %lu\n",
			   item->nr_isolate_pageblock[idx]);
		#endif

		/* watermark */
		for (i = 0; i < MAX_NR_ZONES; i++) {
			unsigned long free_pages = vmstat[NR_FREE_PAGES];
			unsigned long wmark;
			int wmark_idx, vm_lvl = 0;

			/* !alloc_harder */
			free_pages -= z->nr_reserved_highatomic;
			for (wmark_idx = 0; wmark_idx < NR_WMARK; wmark_idx++) {
				wmark = z->watermark[wmark_idx];
				if (free_pages >= wmark + z->lowmem_reserve[i])
					vm_lvl = wmark_idx;
			}

			wmark_level = vm_lvl;
		}
		mri_printf(m, "watermark_level %s\n", wmark_strs[wmark_level]);
		mri_printf(m, "zone_dirty_ok %s\n",
			   item->zone_dirty_ok ? "ok" : "nok");
	}

	/* zram stats */
	for (i = 0; i < NR_ZRAM_STATS_ITEMS; i++)
		mri_printf(m, "zram_%s %lu\n", zram_stats_strs[i], zram_stats[i]);
	mri_printf(m, "zram_comp_ratio %lu\n", item->zram_comp_ratio);

	return 0;
}

static const struct seq_operations mri_stats_op = {
	.start	= mri_stats_start,
	.next	= mri_stats_next,
	.stop	= mri_stats_stop,
	.show	= mri_stats_show,
};

static int mri_stats_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &mri_stats_op);
}

static const struct file_operations mri_stats_fops = {
	.open		= mri_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static ssize_t mri_control_proc_write(struct file *file,
				      const char __user *buffer, size_t count,
				      loff_t *pos)
{
	struct eventfd_ctx *eventfd = NULL;
	unsigned int efd;
	struct fd efile;
	int type;
	char tmp[128], *buf, *endp;
	int ret;

	memset(tmp, 0, sizeof(tmp));
	if (copy_from_user(tmp, buffer, count))
		return -EFAULT;

	buf = strstrip(tmp);

	/* eventfd */
	efd = simple_strtoul(buf, &endp, 10);
	if (*endp != ' ')
		return -EINVAL;
	buf = endp + 1;

	/* type */
	type = simple_strtoul(buf, &endp, 10);
	if (*endp != ' ')
		return -EINVAL;
	buf = endp + 1;

	efile = fdget(efd);
	if (!efile.file)
		return -EBADF;

	eventfd = eventfd_ctx_fileget(efile.file);
	if (IS_ERR(eventfd)) {
		ret = PTR_ERR(eventfd);
		goto out;
	}

	if (mri_check_event_registered(eventfd)) {
		mri_unregister_event(eventfd);
		ret = count;
		goto out;
	}

	ret = mri_register_event(eventfd, type, buf);
	if (ret)
		goto fail;

	fdput(efile);

	return count;
fail:
	if (eventfd && !IS_ERR(eventfd))
		eventfd_ctx_put(eventfd);
out:
	fdput(efile);
	return ret;
}

static const struct file_operations mri_control_proc_fops = {
	.write		= mri_control_proc_write,
};

#if DEBUG
static long long freepages_delay_prev_snap;
static int mri_snap_show(struct seq_file *m, void *v)
{
	struct mri_stats_item *item;
	unsigned long *vmstat;
	long long cur_delays = get_freepages_delay();
	long long delays = cur_delays - freepages_delay_prev_snap;

	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (!item)
		return -ENOMEM;

	vmstat = &item->vmstat[0];

	__mri_do_profile(item, delays);

	#if 1
	mri_stats_show(m, (void *)item);
	#else
	seq_puts(m, "================================================\n");
	seq_printf(m, "time %llu\n", item->time);
	seq_printf(m, "delays %llu\n", item->delays);
	seq_printf(m, "score %d\n", item->score);
	seq_printf(m, "count %d\n", item->count);
	seq_printf(m, "usable %ld\n", item->usable);
	seq_printf(m, "active_anon:%lu inactive_anon:%lu isolated_anon:%lu\n"
		   " active_file:%lu inactive_file:%lu isolated_file:%lu\n"
		   " unevictable:%lu dirty:%lu writeback:%lu unstable:%lu\n"
		   " slab_reclaimable:%lu slab_unreclaimable:%lu\n"
		   " mapped:%lu shmem:%lu pagetables:%lu bounce:%lu\n"
		   " free:%lu\n",
		   vmstat[NR_ACTIVE_ANON],
		   vmstat[NR_INACTIVE_ANON],
		   vmstat[NR_ISOLATED_ANON],
		   vmstat[NR_ACTIVE_FILE],
		   vmstat[NR_INACTIVE_FILE],
		   vmstat[NR_ISOLATED_FILE],
		   vmstat[NR_UNEVICTABLE],
		   vmstat[NR_FILE_DIRTY],
		   vmstat[NR_WRITEBACK],
		   vmstat[NR_UNSTABLE_NFS],
		   vmstat[NR_SLAB_RECLAIMABLE],
		   vmstat[NR_SLAB_UNRECLAIMABLE],
		   vmstat[NR_FILE_MAPPED],
		   vmstat[NR_SHMEM],
		   vmstat[NR_PAGETABLE],
		   vmstat[NR_BOUNCE],
		   vmstat[NR_FREE_PAGES]);
#if IS_ENABLED(CONFIG_ZSMALLOC)
	seq_printf(m, " zspages:%lu\n", vmstat[NR_ZSPAGES]);
#endif

	/* nr free foreach zone */
	for_each_populated_zone(z) {
		struct pglist_data *pgdat = z->zone_pgdat;
		unsigned int idx = zone_idx(z);
		unsigned long *nr_free = item->nr_free[idx];
		unsigned long *types = item->migrate_types[idx];
		unsigned long total = 0;
		int order, wmark_level;

		seq_printf(m, "Node_%d_zone_%s:\n", pgdat->node_id, z->name);
		for (order = 0; order < MAX_ORDER; ++order) {
			total += nr_free[order] << order;
			seq_printf(m, "%lu*%lukB ", nr_free[order],
				   K(1UL) << order);
			if (nr_free[order])
				show_migrate_types(m, types[order]);
		}
		seq_printf(m, "= %lukB\n", K(total));

		seq_printf(m, "nr_reserved_highatomic %lu\n",
			   item->nr_reserved_highatomic[idx]);
		#ifdef CONFIG_MEMORY_ISOLATION
		seq_printf(m, "nr_isolate_pageblock %lu\n",
			   item->nr_isolate_pageblock[idx]);
		#endif

		/* watermark */
		seq_puts(m, "watermark_level[]: ");
		for (i = 0; i < MAX_NR_ZONES; i++) {
			unsigned long free_pages = vmstat[NR_FREE_PAGES];
			unsigned long wmark;
			int wmark_idx, vm_lvl = 0;

			/* !alloc_harder */
			free_pages -= z->nr_reserved_highatomic;
			for (wmark_idx = 0; wmark_idx < NR_WMARK; wmark_idx++) {
				wmark = z->watermark[wmark_idx];
				if (free_pages >= wmark + z->lowmem_reserve[i])
					vm_lvl = wmark_idx;
			}

			wmark_level = vm_lvl;
			seq_printf(m, " %s", wmark_strs[wmark_level]);
		}
		seq_puts(m, "\n");
		seq_printf(m, "zone_dirty_ok %s\n",
			   item->zone_dirty_ok ? "ok" : "nok");
	}
	#endif

	kfree(item);

	freepages_delay_prev_snap = cur_delays;
	return 0;
}

static int mri_snap_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mri_snap_show, NULL);
}

static const struct file_operations mri_snap_proc_fops = {
	.open		= mri_snap_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

static ssize_t mri_ratio_read(struct file *file, char __user *buf, size_t count,
			      loff_t *ppos)
{
	char buffer[128], *tmp;
	int i, len = 0;

	memset(buffer, 0, sizeof(buffer));
	tmp = buffer;

	for (i = MRI_LOW; i < MRI_NUM_LEVELS; i++)
		len += sprintf(&tmp[len], "%d ", mri_ratio_levels[i]);
	len += sprintf(&tmp[len], "\n");

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t mri_ratio_write(struct file *file, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	int ratio_levels[MRI_NUM_LEVELS];
	char buffer[128], *tmp, *endp;
	int i;

	memset(buffer, 0, sizeof(buffer));
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	tmp = strstrip((char *)buffer);
	if (strlen(tmp) == 0)
		return -EINVAL;

	/* get levels */
	for (i = 0; i < MRI_NUM_LEVELS; i++) {
		ratio_levels[i] = simple_strtoul(tmp, &endp, 10);
		if (*endp != ' ')
			break;
		tmp = strstrip((char *)endp);
	}

	if (++i != MRI_NUM_LEVELS)
		return -EINVAL;

	/* check levels */
	for (i = 1; i < MRI_NUM_LEVELS; i++) {
		if (ratio_levels[i - 1] > ratio_levels[i])
			return -EINVAL;
	}

	/* update levels */
	memcpy(mri_ratio_levels, ratio_levels, sizeof(mri_ratio_levels));

	return count;
}

static const struct file_operations mri_ratio_proc_fops = {
	.read		= mri_ratio_read,
	.write		= mri_ratio_write,
	.llseek		= generic_file_llseek,
};

static ssize_t mri_reset_proc_write(struct file *file,
				    const char __user *buffer, size_t count,
				    loff_t *pos)
{
	atomic_set(&mri_freeze, 1);
	mri_reset_events();
	pr_info("reset done.\n");

	return count;
}

static const struct file_operations mri_reset_proc_fops = {
	.write		= mri_reset_proc_write,
};

static int mri_freeze_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", atomic_read(&mri_freeze));
	return 0;
}

static int mri_freeze_open(struct inode *inode, struct file *file)
{
	return single_open(file, mri_freeze_show, NULL);
}

static ssize_t mri_freeze_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	char buffer[128], *tmp;
	int t, ret;

	memset(buffer, 0, sizeof(buffer));
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	tmp = strstrip((char *)buffer);
	if (strlen(tmp) == 0)
		return -EINVAL;

	ret = kstrtoint(tmp, 10, &t);
	if (ret)
		return -EINVAL;

	atomic_set(&mri_freeze, (t > 0) ? 1 : 0);

	return count;
}

static const struct file_operations mri_freeze_proc_fops = {
	.open		= mri_freeze_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.write		= mri_freeze_write,
	.release	= single_release,
};
#endif /* CONFIG_PROC_FS */

#ifdef CONFIG_MAGIC_SYSRQ
static void sysrq_handle_show_mri_buffer(int key)
{
	mutex_lock(&mri_lock);

	while (mri_stats_circ_pending(&mri_stats)) {
		struct mri_stats_item *item = &mri_stats.buffer[mri_stats.tail];

		mri_stats_show(NULL, (void *)item);

		mri_stats_circ_tail_inc(&mri_stats);
	}

	mutex_unlock(&mri_lock);
}

static struct sysrq_key_op sysrq_show_mri_buffer_op = {
	.handler	= sysrq_handle_show_mri_buffer,
	.help_msg	= "show-mri-buffer(v)",
	.action_msg	= "Show MRI Buffer",
	.enable_mask	= SYSRQ_ENABLE_LOG,
};
#endif

static int mri_proc_init(void)
{
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *entry;

	mri_proc_dir = proc_mkdir("mri", NULL);
	if (!mri_proc_dir)
		return -ENOMEM;

	entry = proc_create("delays", S_IRUGO, mri_proc_dir,
			    &mri_delays_proc_fops);
	if (!entry)
		goto fail1;

	entry = proc_create("high_order_alloc", S_IRUGO, mri_proc_dir,
			    &mri_high_order_alloc_proc_fops);
	if (!entry)
		goto fail2;

	entry = proc_create("stats", S_IRUGO, mri_proc_dir, &mri_stats_fops);
	if (!entry)
		goto fail3;

	entry = proc_create("event_ctrl", S_IRUGO, mri_proc_dir,
			    &mri_control_proc_fops);
	if (!entry)
		goto fail4;

	entry = proc_create("ratio", S_IRUGO, mri_proc_dir,
			    &mri_ratio_proc_fops);
	if (!entry)
		goto fail5;

	entry = proc_create("reset", S_IRUGO, mri_proc_dir,
			    &mri_reset_proc_fops);
	if (!entry)
		goto fail6;

	entry = proc_create("freeze", S_IRUGO, mri_proc_dir,
			    &mri_freeze_proc_fops);
	if (!entry)
		goto fail7;

	entry = proc_create("snap", S_IRUGO, mri_proc_dir, &mri_snap_proc_fops);
	if (entry)
		goto out;

	remove_proc_entry("freeze", mri_proc_dir);
fail7:
	remove_proc_entry("reset", mri_proc_dir);
fail6:
	remove_proc_entry("ratio", mri_proc_dir);
fail5:
	remove_proc_entry("event_ctrl", mri_proc_dir);
fail4:
	remove_proc_entry("stats", mri_proc_dir);
fail3:
	remove_proc_entry("high_order_alloc", mri_proc_dir);
fail2:
	remove_proc_entry("delays", mri_proc_dir);
fail1:
	remove_proc_entry("mri", NULL);

	return -ENOMEM;
out:
#endif
	return 0;
}

static void mri_exit(void)
{
	kthread_stop(mri_thread);

#ifdef MRI_BUF_DYNAMIC
	kfree(mri_stats.buffer);
#endif

	unregister_hotcpu_notifier(&mri_notifier);

	remove_proc_entry("reset", mri_proc_dir);
	remove_proc_entry("ratio", mri_proc_dir);
	remove_proc_entry("event_ctrl", mri_proc_dir);
	remove_proc_entry("stats", mri_proc_dir);
	remove_proc_entry("high_order_alloc", mri_proc_dir);
	remove_proc_entry("delays", mri_proc_dir);
	remove_proc_entry("mri", NULL);
}

static int __init mri_init(void)
{
	int i;

	if (mri_helper_disabled)
		return 0;

	if (mri_proc_init())
		goto out;

#ifdef MRI_BUF_DYNAMIC
	mri_stats.buffer = kzalloc(sizeof(*mri_stats.buffer) * MRI_BUF_SIZE,
			     GFP_KERNEL);
	if (!mri_stats.buffer)
		goto out;
#endif
	/* list init */
	for (i = 0 ; i < MRI_NUM_EVENTS; i++) {
		mutex_init(&mri_event_lists[i].lock);
		mri_event_lists[i].curr = 0;
		INIT_LIST_HEAD(&mri_event_lists[i].list);
	}

	/* total scale */
	total_scale = num_online_cpus() * mri_sleep_millisecs;
	total_scale /= (MRI_SCORE_UNITSIZE / NSEC_PER_MSEC);
	pr_info("total_scale = %ld\n", total_scale);

#ifdef CONFIG_MAGIC_SYSRQ
	register_sysrq_key('v', &sysrq_show_mri_buffer_op);
#endif
	register_hotcpu_notifier(&mri_notifier);

	atomic_set(&mri_freeze, 1);

	mri_thread = kthread_run(mri_kthread, NULL, "mrid");
	if (IS_ERR(mri_thread)) {
		pr_err("mri: creating kthread failed %ld\n",
		       PTR_ERR(mri_thread));
		goto out;
	}

	return 0;
out:
	return -ENOMEM;
}

module_init(mri_init);
module_exit(mri_exit);
MODULE_LICENSE("GPL");
