#ifndef _LINUX_LOW_MEM_NOTIFY_H
#define _LINUX_LOW_MEM_NOTIFY_H

struct xreclaimable;

enum zram_stats_item {
	ZRAM_STATS_COMP_DATA= 0,
	ZRAM_STATS_ORIG_DATA,
	ZRAM_STATS_MAX_USED,
	ZRAM_STATS_ZERO_PAGES,
	NR_ZRAM_STATS_ITEMS,
};

struct usable_info {
	unsigned long free;
	unsigned long swap;
	unsigned long swapcache;
	unsigned long file;
	unsigned long usable;
};

#ifdef CONFIG_LOW_MEM_NOTIFY

extern void low_mem_notify(void);
extern int low_mem_notify_register_zram(void *refp, u64 disksize);
extern void low_mem_notify_get_zram_stats(unsigned long *stats);
extern struct xreclaimable * low_mem_notify_add_xreclaimable(char *name);
extern void low_mem_notify_remove_xreclaimable(struct xreclaimable * xr);
extern void low_mem_notify_set_xreclaimable(unsigned long size);
extern void low_mem_notify_set_xreclaimable_size(struct xreclaimable * xr, unsigned long size);
extern void low_mem_notify_set_freeze(unsigned long flag);
extern unsigned long get_usable_pages(void);
extern void get_usable_info(struct usable_info *ui);

#else /* !CONFIG_LOW_MEM_NOTIFY */

static inline void low_mem_notify(void) {}
static inline int low_mem_notify_register_zram(void *refp, u64 disksize) {}
static inline void low_mem_notify_get_zram_stats(unsigned long *stats) {}
static inline struct xreclaimable * low_mem_notify_add_xreclaimable(char *name) { return NULL; }
static inline void low_mem_notify_remove_xreclaimable(struct xreclaimable * xr) {}
static inline void low_mem_notify_set_xreclaimable(unsigned long size) {}
static inline void low_mem_notify_set_xreclaimable_size(struct xreclaimable * xr, unsigned long size) {}
static inline void low_mem_notify_set_freeze(unsigned long flag) {}
static inline unsigned long get_usable_pages(void) { return 0; }
static inline void get_usable_info(struct usable_info *ui) {}

#endif

#endif
