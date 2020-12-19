#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <rdvb-common/clock.h>
#include "dmx_buf_dbg.h"

void dmx_buf_dbg_init(struct pin_debug * pin_debug)
{
	memset(pin_debug, 0, sizeof(struct pin_debug));
}
int dmx_buf_dbg_dump_ctrl(bool ctrl, struct pin_debug *debug, char *path, bool flush)
{
	if (ctrl) {
		if (debug->dump_file){
			filp_close(debug->dump_file, NULL);
			debug->dump_file = NULL;
			if (debug->dump_path) {
				kfree(debug->dump_path);
				debug->dump_path = NULL;
			}
		}
		debug->dump_file =  filp_open(path, O_RDWR|O_CREAT|O_TRUNC, 0777);
		if (debug->dump_file== NULL || IS_ERR(debug->dump_file)) {
			printk("%s_%d:ERROR  open file :%s fail \n", __func__, __LINE__, path);
			kfree(debug->dump_path);
			return -1;
		}
		debug->dump_path = kstrdup(path, GFP_KERNEL);
		debug->dump_pos = 0;
		debug->is_dump_flush = flush;
		printk("%s_%d: start dump , file:%s\n",	__func__, __LINE__,  path);
	} else {
		if (debug->dump_file){
			filp_close(debug->dump_file, NULL);
			debug->dump_file = NULL;
			if (debug->dump_path){
				kfree(debug->dump_path);
				debug->dump_path = NULL;
			}

		}
		printk("%s_%d: STOP dump  file:%s\n", __func__, __LINE__, path);
	}
	return 0;	
}
int dmx_buf_dbg_dump(struct pin_debug * debug, void * data, unsigned int size)
{
	if (debug->dump_file){

		mm_segment_t fs;
		fs = get_fs();
		set_fs(KERNEL_DS);    /* set to KERNEL_DS */
		vfs_write(debug->dump_file, data, size, &debug->dump_pos);
		set_fs(fs);

	}
	return 0;
}

int dmx_buf_dbg_dump_flush(struct pin_debug * debug)
{
	if (debug->dump_file == NULL || !debug->is_dump_flush)
		return 0;
	printk("%s_%d: path:%s, dum restart\n", __func__, __LINE__, debug->dump_path);
	filp_close(debug->dump_file, NULL);
	debug->dump_file = NULL;
	debug->dump_file =  
		filp_open(debug->dump_path, O_RDWR | O_CREAT | O_TRUNC, 0777);
	if (debug->dump_file== NULL || IS_ERR(debug->dump_file)) {
		printk("%s_%d:ERROR open file :%s fail \n", __func__, __LINE__, debug->dump_path);
		kfree(debug->dump_path);
		return -1;
	}
	debug->dump_pos = 0;
	return 0;
}
void dmx_buf_dbg_reset(struct pin_debug * debug)
{
	//printk("%s_%d: dmx_buf debug reset \n", __func__, __LINE__);
	dmx_buf_dbg_dump_flush(debug);
	debug->drop_count = 0;
	debug->deliver_size = 0;

	debug->drop_info_ts = CLOCK_GetPTS();
}
