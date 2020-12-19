
#undef TRACE_SYSTEM
#define TRACE_SYSTEM	dmeswap

#if !defined(_TRACE_DM_ESWAP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_DM_ESWAP_H

#include <linux/kernel.h>
#include <linux/bio.h>
#include <linux/blktrace_api.h>
#include <linux/device-mapper.h>
#include <linux/tracepoint.h>

#define RWBS_LEN	8
#define DEV_NAME_LEN	16

DECLARE_EVENT_CLASS(eswap_lbn_rwbs,

	TP_PROTO(unsigned long lbn, unsigned long bi_rw, unsigned int bi_size),

	TP_ARGS(lbn, bi_rw, bi_size),

	TP_STRUCT__entry(
		__field(unsigned long,	lbn	)
		__array(char,	rwbs,	RWBS_LEN)
	),

	TP_fast_assign(
		__entry->lbn	= lbn;
		blk_fill_rwbs(__entry->rwbs, bi_rw, bi_size);
	),

	TP_printk("lbn = %lu, rwbs = %s", __entry->lbn, __entry->rwbs)
);

DEFINE_EVENT(eswap_lbn_rwbs, eswap_map_enter,

	TP_PROTO(unsigned long lbn, unsigned long bi_rw, unsigned int bi_size),

	TP_ARGS(lbn, bi_rw, bi_size)
);

DECLARE_EVENT_CLASS(eswap_lbn,

	TP_PROTO(unsigned long lbn),

	TP_ARGS(lbn),

	TP_STRUCT__entry(
		__field(unsigned long,	lbn)
	),

	TP_fast_assign(
		__entry->lbn	= lbn;
	),

	TP_printk("lbn = %lu", __entry->lbn)
);

DEFINE_EVENT(eswap_lbn, eswap_write_start,

	TP_PROTO(unsigned long lbn),

	TP_ARGS(lbn)
);

DEFINE_EVENT(eswap_lbn, eswap_read_start,

	TP_PROTO(unsigned long lbn),

	TP_ARGS(lbn)
);

DEFINE_EVENT(eswap_lbn, eswap_meta_write_lbn_start,

	TP_PROTO(unsigned long lbn),

	TP_ARGS(lbn)
);

DEFINE_EVENT(eswap_lbn, eswap_meta_write_lbn_end,

	TP_PROTO(unsigned long lbn),

	TP_ARGS(lbn)
);

DEFINE_EVENT(eswap_lbn, eswap_hash_digest_start,

	TP_PROTO(unsigned long lbn),

	TP_ARGS(lbn)
);

DEFINE_EVENT(eswap_lbn, eswap_hash_digest_end,

	TP_PROTO(unsigned long lbn),

	TP_ARGS(lbn)
);

DEFINE_EVENT(eswap_lbn, eswap_hash_insert_start,

	TP_PROTO(unsigned long lbn),

	TP_ARGS(lbn)
);

DEFINE_EVENT(eswap_lbn, eswap_hash_insert_end,

	TP_PROTO(unsigned long lbn),

	TP_ARGS(lbn)
);

DEFINE_EVENT(eswap_lbn, eswap_compress_start,

	TP_PROTO(unsigned long lbn),

	TP_ARGS(lbn)
);

DEFINE_EVENT(eswap_lbn, eswap_meta_read_lbn_start,

	TP_PROTO(unsigned long lbn),

	TP_ARGS(lbn)
);

DEFINE_EVENT(eswap_lbn, eswap_meta_read_lbn_end,

	TP_PROTO(unsigned long lbn),

	TP_ARGS(lbn)
);

DECLARE_EVENT_CLASS(eswap_io_end,

	TP_PROTO(unsigned long lbn, int ret),

	TP_ARGS(lbn, ret),

	TP_STRUCT__entry(
		__field(unsigned long,	lbn	)
		__field(int,	ret		)
	),

	TP_fast_assign(
		__entry->lbn	= lbn;
		__entry->ret	= ret;
	),

	TP_printk("lbn = %lu, ret = %d", __entry->lbn, __entry->ret)
);

DEFINE_EVENT(eswap_io_end, eswap_write_end,

	TP_PROTO(unsigned long lbn, int ret),

	TP_ARGS(lbn, ret)
);

DEFINE_EVENT(eswap_io_end, eswap_read_end,

	TP_PROTO(unsigned long lbn, int ret),

	TP_ARGS(lbn, ret)
);

DECLARE_EVENT_CLASS(eswap_dev_lbn,

	TP_PROTO(unsigned long lbn, struct dm_dev *dev),

	TP_ARGS(lbn, dev),

	TP_STRUCT__entry(
		__field(unsigned long,	lbn	)
		__array(char,	dev_name, DEV_NAME_LEN	)
	),

	TP_fast_assign(
		__entry->lbn	= lbn;
		memcpy(__entry->dev_name, dev->name, DEV_NAME_LEN);
	),

	TP_printk("lbn = %lu, dev = (%s)",
		__entry->lbn,
		__entry->dev_name)
);


DEFINE_EVENT(eswap_dev_lbn, eswap_dev_write_start,

	TP_PROTO(unsigned long lbn, struct dm_dev *dev),

	TP_ARGS(lbn, dev)
);

DEFINE_EVENT(eswap_dev_lbn, eswap_dev_write_end,

	TP_PROTO(unsigned long lbn, struct dm_dev *dev),

	TP_ARGS(lbn, dev)
);

DEFINE_EVENT(eswap_dev_lbn, eswap_alloc_pblk_start,

	TP_PROTO(unsigned long lbn, struct dm_dev *dev),

	TP_ARGS(lbn, dev)
);

DECLARE_EVENT_CLASS(eswap_pblk,

	TP_PROTO(unsigned long lbn, u64 sector, struct dm_dev *dev),

	TP_ARGS(lbn, sector, dev),

	TP_STRUCT__entry(
		__field(unsigned long,	lbn	)
		__field(u64,	sector		)
		__array(char,	dev_name,	DEV_NAME_LEN)
	),

	TP_fast_assign(
		__entry->lbn	= lbn;
		__entry->sector	= sector;
		memcpy(__entry->dev_name, dev->name, DEV_NAME_LEN);
	),

	TP_printk("lbn = %lu, sector = %llu, dev = (%s)",
		__entry->lbn,
		__entry->sector,
		__entry->dev_name)
);

DEFINE_EVENT(eswap_pblk, eswap_alloc_pblk_end,

	TP_PROTO(unsigned long lbn, u64 sector, struct dm_dev *dev),

	TP_ARGS(lbn, sector, dev)
);

DEFINE_EVENT(eswap_pblk, eswap_dev_read_start,

	TP_PROTO(unsigned long lbn, u64 sector, struct dm_dev *dev),

	TP_ARGS(lbn, sector, dev)
);

DECLARE_EVENT_CLASS(eswap_bio,

	TP_PROTO(struct bio *bio),

	TP_ARGS(bio),

	TP_STRUCT__entry(
		__field(sector_t, sector	)
		__field(unsigned int, size	)
		__field(dev_t, bd_dev)
	),

	TP_fast_assign(
		__entry->sector		= bio->bi_iter.bi_sector;
		__entry->size	= bio->bi_iter.bi_size;
		__entry->bd_dev	= bio->bi_bdev->bd_dev;
	),

	TP_printk("sector = %llu, size = %u, dev = (%d:%d)",
		__entry->sector,
		__entry->size,
		MAJOR(__entry->bd_dev),
		MINOR(__entry->bd_dev))
);

DEFINE_EVENT(eswap_bio, eswap_wb_start,

	TP_PROTO(struct bio *bio),

	TP_ARGS(bio)
);

DEFINE_EVENT(eswap_bio, eswap_bio_write_start,

	TP_PROTO(struct bio *bio),

	TP_ARGS(bio)
);

DEFINE_EVENT(eswap_bio, eswap_bio_read_start,

	TP_PROTO(struct bio *bio),

	TP_ARGS(bio)
);

DECLARE_EVENT_CLASS(eswap_dev_mobj,

	TP_PROTO(u64 sector, u16 size, struct dm_dev *dev),

	TP_ARGS(sector, size, dev),

	TP_STRUCT__entry(
		__field(u64, sector	)
		__field(u16, size	)
		__array(char, dev_name, DEV_NAME_LEN)
	),

	TP_fast_assign(
		__entry->sector	= sector;
		__entry->size	= size;
		memcpy(__entry->dev_name, dev->name, DEV_NAME_LEN);
	),

	TP_printk("sector = %llu, size = %u, dev = (%s)",
		__entry->sector,
		__entry->size,
		__entry->dev_name)
);

DEFINE_EVENT(eswap_dev_mobj, eswap_wb_end,

	TP_PROTO(u64 sector, u16 size, struct dm_dev *dev),

	TP_ARGS(sector, size, dev)
);

DEFINE_EVENT(eswap_dev_mobj, eswap_bio_write_end,

	TP_PROTO(u64 sector, u16 size, struct dm_dev *dev),

	TP_ARGS(sector, size, dev)
);

DEFINE_EVENT(eswap_dev_mobj, eswap_bio_read_end,

	TP_PROTO(u64 sector, u16 size, struct dm_dev *dev),

	TP_ARGS(sector, size, dev)
);

DEFINE_EVENT(eswap_dev_mobj, eswap_dev_read_end,

	TP_PROTO(u64 sector, u16 size, struct dm_dev *dev),

	TP_ARGS(sector, size, dev)
);

DEFINE_EVENT(eswap_dev_mobj, eswap_decompress_start,

	TP_PROTO(u64 sector, u16 size, struct dm_dev *dev),

	TP_ARGS(sector, size, dev)
);

DEFINE_EVENT(eswap_dev_mobj, eswap_decompress_end,

	TP_PROTO(u64 sector, u16 size, struct dm_dev *dev),

	TP_ARGS(sector, size, dev)
);

DECLARE_EVENT_CLASS(eswap_lbn_compress,

	TP_PROTO(unsigned long lbn, unsigned int comp_len),

	TP_ARGS(lbn, comp_len),

	TP_STRUCT__entry(
		__field(unsigned long,	lbn	)
		__field(unsigned int,	comp_len)
	),

	TP_fast_assign(
		__entry->lbn		= lbn;
		__entry->comp_len	= comp_len;
	),

	TP_printk("lbn = %lu, comp_len = %u",
		__entry->lbn,
		__entry->comp_len)
);

DEFINE_EVENT(eswap_lbn_compress, eswap_compress_end,

	TP_PROTO(unsigned long lbn, unsigned int comp_len),

	TP_ARGS(lbn, comp_len)
);

DECLARE_EVENT_CLASS(eswap_merge,

	TP_PROTO(struct bio *buf_bio, struct bio *new_bio),

	TP_ARGS(buf_bio, new_bio),

	TP_STRUCT__entry(
		__field(sector_t, buf_sector	)
		__field(sector_t, new_sector	)
		__field(dev_t, bd_dev		)
	),

	TP_fast_assign(
		__entry->buf_sector	= buf_bio->bi_iter.bi_sector;
		__entry->new_sector	= new_bio->bi_iter.bi_sector;
		__entry->bd_dev		= buf_bio->bi_bdev->bd_dev;
	),

	TP_printk("buf_sector = %llu, new_sector = %llu, dev = (%d:%d)",
		__entry->buf_sector,
		__entry->new_sector,
		MAJOR(__entry->bd_dev),
		MINOR(__entry->bd_dev))
);

DEFINE_EVENT(eswap_merge, eswap_wb_merged,

	TP_PROTO(struct bio *buf_bio, struct bio *new_bio),

	TP_ARGS(buf_bio, new_bio)
);

DECLARE_EVENT_CLASS(eswap_gc,

	TP_PROTO(unsigned long key),

	TP_ARGS(key),

	TP_STRUCT__entry(
		__field(unsigned long,	key)
	),

	TP_fast_assign(
		__entry->key	= key;
	),

	TP_printk("key = %lu", __entry->key)
);

DEFINE_EVENT(eswap_gc, eswap_gc_scan_start,

	TP_PROTO(unsigned long key),

	TP_ARGS(key)
);

DEFINE_EVENT(eswap_gc, eswap_gc_scan_end,

	TP_PROTO(unsigned long key),

	TP_ARGS(key)
);

DEFINE_EVENT(eswap_gc, eswap_gc_run_start,

	TP_PROTO(unsigned long key),

	TP_ARGS(key)
);

DEFINE_EVENT(eswap_gc, eswap_gc_run_end,

	TP_PROTO(unsigned long key),

	TP_ARGS(key)
);

#endif

#include <trace/define_trace.h>
