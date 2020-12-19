#ifndef __DMX_BUF_DBG__
#define __DMX_BUF_DBG__
#include <linux/types.h>

struct pin_debug
{
	/*dump data*/
	bool         is_dump_enable;
	bool         is_dump_flush;
	char       * dump_path;
	struct file* dump_file;
	loff_t       dump_pos;
	//monitor
	unsigned int total_size;
	unsigned int drop_count;
	unsigned int deliver_size;
	//drop handle
	int64_t      drop_info_ts;

};
void dmx_buf_dbg_init(struct pin_debug * pin_debug);
int dmx_buf_dbg_dump_ctrl(bool ctrl, struct pin_debug * debug, char *path, bool flush);
int dmx_buf_dbg_dump(struct pin_debug * pin_debug, void * data, unsigned int size);
int dmx_buf_dbg_dump_flush(struct pin_debug * pin_debug);
void dmx_buf_dbg_reset(struct pin_debug * debug);
#endif