/*
 * Block Dump Msg Log
 *
 * Copyright (C) 2019 LGE, Inc.
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

#define pr_fmt(fmt)     "bdmsg: " fmt

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/circ_buf.h>
#include <linux/bio.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/genhd.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#define BDMSG_LEN		(256)
#define BDMSG_SIZE		(8192)
#define BDMSG_MASK		(BDMSG_SIZE -1)

#define CBUF_EMPTY(c)		((c)->head == (c)->tail)
#define CBUF_CLEAR(c)		((c)->head = (c)->tail = 0)
#define CBUF_COUNT(c)		(CIRC_CNT((c)->head, (c)->tail, BDMSG_SIZE))
#define CBUF_FREE(c)		(CIRC_SPACE((c)->head, (c)->tail, BDMSG_SIZE))
#define CBUF_HEAD_INC(c)	((c)->head = ((c)->head + 1) & BDMSG_MASK)
#define CBUF_TAIL_INC(c)	((c)->tail = ((c)->tail + 1) & BDMSG_MASK)
#define CBUF_INDEX_INC(i)	(i = (i + 1) & BDMSG_MASK)
#define CBUF_INDEX_COUNT(h, i)  (CIRC_CNT(h, i, BDMSG_SIZE))

struct bdmsg_item {
	int len;
	char msg[BDMSG_LEN];
};

struct bdmsg_cbuf {
	int head;
	int tail;
	struct bdmsg_item buffer[BDMSG_SIZE];
};

static struct bdmsg_cbuf bdmsg_log;

static DECLARE_WAIT_QUEUE_HEAD(bdmsg_log_wait);

static bool bdmsg_disabled = false;
static bool bdmsg_rw = false;
static int  bdmsg_filter_len = strlen(CONFIG_BLOCK_DUMP_MSG_DEFAULT_FILTER);
static char bdmsg_filter[32] = CONFIG_BLOCK_DUMP_MSG_DEFAULT_FILTER;

static int early_bdmsg_rw_param(char *buf)
{
	bdmsg_rw = true;
	pr_info("set Read-write");

	return 0;
}
early_param("bdmsg.rw", early_bdmsg_rw_param);

static int early_bdmsg_filter_param(char *buf)
{
	int count = 0;

	// set filter
	count = strlen(buf);
	if (count > 0) {
		strncpy(bdmsg_filter, buf, count);
		bdmsg_filter_len= count;
		pr_info("set filter(%s)", bdmsg_filter);
	}

	return 0;
}
early_param("bdmsg.filter", early_bdmsg_filter_param);

static int early_bdmsg_disabled_param(char *buf)
{
	bdmsg_disabled = true;

	return 0;
}
early_param("bdmsg.disabled", early_bdmsg_disabled_param);

static DEFINE_RAW_SPINLOCK(bdmsg_log_lock);

static inline void bdmsg_lock(raw_spinlock_t *lock) {
	raw_spin_lock_irq(lock);
}

static inline void bdmsg_unlock(raw_spinlock_t *lock) {
	raw_spin_unlock_irq(lock);
}

static int push_bdmsg(int rw, struct bio *bio, unsigned int count)
{
	char bdev[BDEVNAME_SIZE];

	//produce_item(item);
	struct bdmsg_item *item = &bdmsg_log.buffer[bdmsg_log.head];
	item->len = snprintf(item->msg, BDMSG_LEN,
			     "%s(%d): %s block %llu on %s (%u sectors)\n",
			     current->comm, task_pid_nr(current),
			     (rw & WRITE) ? "WRITE" : "READ",
			     (unsigned long long)bio->bi_iter.bi_sector,
			     (bdmsg_filter_len) ? bdmsg_filter : bdevname(bio->bi_bdev, bdev),
			     count);

	if (CBUF_FREE(&bdmsg_log) == 0) {
		CBUF_TAIL_INC(&bdmsg_log);
	}
	CBUF_HEAD_INC(&bdmsg_log);
	//pr_info("bdmsg[%d]:%s", CBUF_COUNT(&bdmsg_log), item->msg);

	return 0;
}

static void _block_dump_msg(int rw, struct bio *bio, unsigned int count)
{
	struct gendisk *disk = NULL;

	if (bdmsg_disabled)
		return;

	if (!bdmsg_rw && !(rw & WRITE))
		return;

	disk = bio->bi_bdev->bd_disk;
	if (!(disk->flags & GENHD_FL_FILTERED)) {
		return;
	}

#if 0
	char b[BDEVNAME_SIZE];
	printk(KERN_DEBUG "%s(%d): %s block %Lu on %s (%u sectors)\n",
	       current->comm, task_pid_nr(current),
	       (rw & WRITE) ? "WRITE" : "READ",
	       (unsigned long long)bio->bi_iter.bi_sector,
	       bdevname(bio->bi_bdev, b),
	       count);
#endif
	bdmsg_lock(&bdmsg_log_lock);

	push_bdmsg(rw, bio, count);

	//wake_up(consumer);
	wake_up(&bdmsg_log_wait);

	bdmsg_unlock(&bdmsg_log_lock);
}

static void _block_dump_msg_null(int rw, struct bio *bio, unsigned int count) { };
void (*block_dump_msg)(int rw, struct bio *bio, unsigned int count) =  _block_dump_msg_null;
EXPORT_SYMBOL(block_dump_msg);

void set_filtered_disk(struct gendisk *disk)
{
	if (!bdmsg_filter_len)
		return ;

	if (strncmp(disk->disk_name, bdmsg_filter, bdmsg_filter_len) == 0) {
		disk->flags |= GENHD_FL_FILTERED;
		block_dump_msg = _block_dump_msg;
	}
}
EXPORT_SYMBOL(set_filtered_disk);

struct bdmsg_user {
	u32 idx;
	struct mutex lock;
	char filter[32];
};

static int bdmsg_open(struct inode * inode, struct file * file)
{
	struct bdmsg_user *user;

	/* write-only does not need any file context */
	if ((file->f_flags & O_ACCMODE) == O_WRONLY)
		return 0;

	user = kmalloc(sizeof(struct bdmsg_user), GFP_KERNEL);
	if (!user)
		return -ENOMEM;

	mutex_init(&user->lock);

	bdmsg_lock(&bdmsg_log_lock);
	user->idx = bdmsg_log.tail;
	pr_debug("user index = %d", user->idx);
	bdmsg_unlock(&bdmsg_log_lock);

	file->private_data = user;
	return 0;
}

static int bdmsg_release(struct inode * inode, struct file * file)
{
	struct bdmsg_user *user = file->private_data;

	if (!user)
		return 0;

	mutex_destroy(&user->lock);
	kfree(user);
	return 0;
}

static ssize_t bdmsg_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	struct bdmsg_user *user = file->private_data;
	struct bdmsg_item *item = NULL;
	size_t len;
	ssize_t ret;

	if (!user)
		return -EBADF;

	ret = mutex_lock_interruptible(&user->lock);
	if (ret)
		return ret;

	bdmsg_lock(&bdmsg_log_lock);
	while (!CBUF_INDEX_COUNT(bdmsg_log.head, user->idx)) {
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto unlock_out;
		}

		bdmsg_unlock(&bdmsg_log_lock);
		ret = wait_event_interruptible(bdmsg_log_wait,
			       CBUF_INDEX_COUNT(bdmsg_log.head, user->idx));
		if (ret)
			goto out;

		bdmsg_lock(&bdmsg_log_lock);
	}

	/* extract one item from the buffer */
	item = &bdmsg_log.buffer[user->idx];
	len = item->len;
	//pr_info("read [%d]:%s", CBUF_INDEX_COUNT(bdmsg_log.head, user->idx), item->msg);

	if (len > count) {
		ret = -EINVAL;
		goto unlock_out;
	}

	if (copy_to_user(buf, item->msg, len) > 0) {
		ret = -EFAULT;
		goto unlock_out;
	}

	ret = len;
	/* Finish reading descriptor before incrementing user index */
	CBUF_INDEX_INC(user->idx);

unlock_out:
	bdmsg_unlock(&bdmsg_log_lock);

out:
	mutex_unlock(&user->lock);
	return ret;
}

static unsigned int bdmsg_poll(struct file *file, poll_table *wait)
{
	struct bdmsg_user *user = file->private_data;
	int ret = 0;

	if (!user)
		return POLLERR|POLLNVAL;

	poll_wait(file, &bdmsg_log_wait, wait);
#if 0
	bdmsg_lock(&bdmsg_log_lock);
	if (CBUF_INDEX_COUNT(bdmsg_log.head, user->idx) > 0)
		return POLLIN | POLLRDNORM;
	bdmsg_unlock(&bdmsg_log_lock);
#endif
	return ret;
}

static const struct file_operations bdmsg_fops = {
	.read		= bdmsg_read,
	.poll		= bdmsg_poll,
	.open		= bdmsg_open,
	.release	= bdmsg_release,
	.llseek		= generic_file_llseek,
};

static int filter_show(struct seq_file *m, void *v)
{
	if (bdmsg_filter_len)
		seq_printf(m, "%s\n", bdmsg_filter);
	return 0;
}

static int filter_open(struct inode *inode, struct file *file)
{
	return single_open(file, filter_show, NULL);
}

static ssize_t filter_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	char buffer[32];

	if (count >= 32)
		return -EINVAL;

	if (count == 0) {
		memset(bdmsg_filter, 0, sizeof(buffer));
		bdmsg_filter_len = 0;
		return 0;
	}

	memset(buffer, 0, sizeof(buffer));
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	bdmsg_lock(&bdmsg_log_lock);

	strncpy(bdmsg_filter, buffer, count);
	bdmsg_filter_len = count;
	pr_info("filtered: %s[%d]", bdmsg_filter, bdmsg_filter_len);

	bdmsg_unlock(&bdmsg_log_lock);

	return count;
}

static const struct file_operations filter_fops = {
	.open		= filter_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.write		= filter_write,
	.release	= single_release,
};

static struct proc_dir_entry *bdmsg_proc_dir;
static int __init proc_bdmsg_init(void)
{
	struct proc_dir_entry *entry;

	bdmsg_proc_dir = proc_mkdir("block_dump_msg", NULL);
	if (!bdmsg_proc_dir)
		return -ENOMEM;

	entry = proc_create("bdmsg", S_IRUSR, bdmsg_proc_dir, &bdmsg_fops);
	if (!entry)
		goto fail_bdmsg;

	entry = proc_create("filter", S_IRUSR, bdmsg_proc_dir, &filter_fops);
	if (!entry)
		goto fail_filter;

	bdmsg_lock(&bdmsg_log_lock);

	CBUF_CLEAR(&bdmsg_log);

	bdmsg_unlock(&bdmsg_log_lock);

	return 0;

fail_filter:
	remove_proc_entry("bdmsg", NULL);

fail_bdmsg:
	remove_proc_entry("block_dump_msg", NULL);
	return -ENOMEM;
}
fs_initcall(proc_bdmsg_init);
