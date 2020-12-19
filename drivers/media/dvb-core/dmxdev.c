/*
 * dmxdev.c - DVB demultiplexer device
 *
 * Copyright (C) 2000 Ralph Metzler & Marcus Metzler
 *		      for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/ioctl.h>
#include <linux/wait.h>
#include <asm/uaccess.h>
#include "dmxdev.h"
#include <dmx-ext.h>
#include <pvr-ext.h>

static int debug;

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Turn on/off debugging (default:off).");

#define dprintk	if (debug) printk

static int dvb_dmxdev_buffer_write(struct dvb_ringbuffer *buf,
				   const u8 *src, size_t len)
{
	ssize_t free;

	if (!len)
		return 0;
	if (!buf->data)
		return 0;

	free = dvb_ringbuffer_free(buf);
	if (len > free) {
		dprintk("dmxdev: buffer overflow\n");
		return -EOVERFLOW;
	}

	return dvb_ringbuffer_write(buf, src, len);
}

static ssize_t dvb_dmxdev_buffer_read(struct dvb_ringbuffer *src,
				      int non_blocking, char __user *buf,
				      size_t count, loff_t *ppos)
{
	size_t todo;
	ssize_t avail;
	ssize_t ret = 0;

	if (!src->data)
		return 0;
	if (src->error) {
		ret = src->error;
		dvb_ringbuffer_flush(src);
		return ret;
	}

	for (todo = count; todo > 0; todo -= ret) {
		if (non_blocking && dvb_ringbuffer_empty(src)) {
			ret = -EWOULDBLOCK;
			break;
		}

		ret = wait_event_interruptible(src->queue,
					       !dvb_ringbuffer_empty(src) ||
					       (src->error != 0));
		if (ret < 0)
			break;

		if (src->error) {
			ret = src->error;
			dvb_ringbuffer_flush(src);
			break;
		}

		avail = dvb_ringbuffer_avail(src);
		if (avail > todo)
			avail = todo;

		ret = dvb_ringbuffer_read_user(src, buf, avail);
		if (ret < 0)
			break;

		buf += ret;
	}

	return (count - todo) ? (count - todo) : ret;
}

static struct dmx_frontend *get_fe(struct dmx_demux *demux, int type)
{
	struct list_head *head, *pos;

	head = demux->get_frontends(demux);
	if (!head)
		return NULL;
	list_for_each(pos, head)
		if (DMX_FE_ENTRY(pos)->source == type)
			return DMX_FE_ENTRY(pos);

	return NULL;
}

static int dvb_dvr_callback(const u8 *buffer1, size_t buffer1_len,
				  const u8 *buffer2, size_t buffer2_len,
				  void *source)
{
	struct dmxdev *dmxdev = (struct dmxdev *)source;
	struct dvb_ringbuffer *buffer;
	int ret;

	spin_lock(&dmxdev->lock);
	buffer = &dmxdev->dvr_buffer;
	if (buffer->error) {
		spin_unlock(&dmxdev->lock);
		wake_up(&buffer->queue);
		return 0;
	}
	if (dmxdev->dvrActiveFilterCnt <= 0) {
		spin_unlock(&dmxdev->lock);
		wake_up(&buffer->queue);
		return 0;
	}
	ret = dvb_dmxdev_buffer_write(buffer, buffer1, buffer1_len);
	if (ret == buffer1_len)
		ret = dvb_dmxdev_buffer_write(buffer, buffer2, buffer2_len);

	spin_unlock(&dmxdev->lock);
	wake_up(&buffer->queue);
	return ret;
}

static int dvb_dvr_open(struct inode *inode, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dmxdev *dmxdev = dvbdev->priv;
	struct dmx_frontend *front;

	dprintk("function : %s\n", __func__);

	if (mutex_lock_interruptible(&dmxdev->mutex))
		return -ERESTARTSYS;

	if (dmxdev->exit) {
		mutex_unlock(&dmxdev->mutex);
		return -ENODEV;
	}

	if ((file->f_flags & O_ACCMODE) == O_RDWR) {
		if (!(dmxdev->capabilities & DMXDEV_CAP_DUPLEX)) {
			mutex_unlock(&dmxdev->mutex);
			return -EOPNOTSUPP;
		}
	}

	if ((file->f_flags & O_ACCMODE) == O_RDONLY) {
		void *mem;
		if (!dvbdev->readers) {
			mutex_unlock(&dmxdev->mutex);
			return -EBUSY;
		}
		mem = vmalloc(DVR_BUFFER_SIZE);
		if (!mem) {
			mutex_unlock(&dmxdev->mutex);
			return -ENOMEM;
		}
		if (dmxdev->demux->dvr_open == NULL) {
			vfree(mem);
			mutex_unlock(&dmxdev->mutex);
			return -ENOMEM;
		}
		if (dmxdev->demux->dvr_open(dmxdev->demux, dvb_dvr_callback) < 0) {
			vfree(mem);
			mutex_unlock(&dmxdev->mutex);
			return -ENOMEM;
		}
		dvb_ringbuffer_init(&dmxdev->dvr_buffer, mem, DVR_BUFFER_SIZE);
		dmxdev->dvr_buffer_total_readSize = 0;

		dvbdev->readers--;
	}

	if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
		dmxdev->dvr_orig_fe = dmxdev->demux->frontend;

		if (!dmxdev->demux->write) {
			mutex_unlock(&dmxdev->mutex);
			return -EOPNOTSUPP;
		}

		front = get_fe(dmxdev->demux, DMX_MEMORY_FE);

		if (!front) {
			mutex_unlock(&dmxdev->mutex);
			return -EINVAL;
		}
		dmxdev->demux->disconnect_frontend(dmxdev->demux);
		dmxdev->demux->connect_frontend(dmxdev->demux, front);
	}
	dvbdev->users++;
	mutex_unlock(&dmxdev->mutex);
	return 0;
}

static int dvb_dvr_release(struct inode *inode, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dmxdev *dmxdev = dvbdev->priv;

	mutex_lock(&dmxdev->mutex);
	if (dmxdev->demux->dvr_close) {
		dmxdev->demux->dvr_close(dmxdev->demux);
	}
	if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
		dmxdev->demux->disconnect_frontend(dmxdev->demux);
		dmxdev->demux->connect_frontend(dmxdev->demux,
						dmxdev->dvr_orig_fe);
	}
	if ((file->f_flags & O_ACCMODE) == O_RDONLY) {
		dvbdev->readers++;
		if (dmxdev->dvr_buffer.data) {
			void *mem = dmxdev->dvr_buffer.data;
			mb();
			spin_lock_irq(&dmxdev->lock);
			dmxdev->dvr_buffer.data = NULL;
			spin_unlock_irq(&dmxdev->lock);
			vfree(mem);
			dmxdev->dvr_buffer_total_readSize = 0;
		}
	}
	/* TODO */
	dvbdev->users--;
	if (dvbdev->users == 1 && dmxdev->exit == 1) {
		mutex_unlock(&dmxdev->mutex);
		wake_up(&dvbdev->wait_queue);
	} else
		mutex_unlock(&dmxdev->mutex);

	return 0;
}

static ssize_t dvb_dvr_write(struct file *file, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dmxdev *dmxdev = dvbdev->priv;
	int ret;

	if (!dmxdev->demux->write)
		return -EOPNOTSUPP;
	if ((file->f_flags & O_ACCMODE) != O_WRONLY)
		return -EINVAL;
	if (mutex_lock_interruptible(&dmxdev->mutex))
		return -ERESTARTSYS;

	if (dmxdev->exit) {
		mutex_unlock(&dmxdev->mutex);
		return -ENODEV;
	}
	ret = dmxdev->demux->write(dmxdev->demux, buf, count);
	mutex_unlock(&dmxdev->mutex);
	return ret;
}

static ssize_t dvb_dvr_read(struct file *file, char __user *buf, size_t count,
			    loff_t *ppos)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dmxdev *dmxdev = dvbdev->priv;
	ssize_t ret = 0;

	if (dmxdev->exit)
		return -ENODEV;

	ret = dvb_dmxdev_buffer_read(&dmxdev->dvr_buffer,
				      file->f_flags & O_NONBLOCK,
				      buf, count, ppos);
	if (ret > 0) {
		dmxdev->dvr_buffer_total_readSize += (unsigned int)ret;
	}
	return ret;
}

static int dvb_dvr_set_buffer_size(struct dmxdev *dmxdev,
				      unsigned long size)
{
	struct dvb_ringbuffer *buf = &dmxdev->dvr_buffer;
	void *newmem;
	void *oldmem;

	dprintk("function : %s\n", __func__);

	if (buf->size == size)
		return 0;
	if (!size)
		return -EINVAL;

	newmem = vmalloc(size);
	if (!newmem)
		return -ENOMEM;

	oldmem = buf->data;

	spin_lock_irq(&dmxdev->lock);
	buf->data = newmem;
	buf->size = size;

	/* reset and not flush in case the buffer shrinks */
	dvb_ringbuffer_reset(buf);
	spin_unlock_irq(&dmxdev->lock);

	vfree(oldmem);

	return 0;
}

static inline void dvb_dmxdev_filter_state_set(struct dmxdev_filter
					       *dmxdevfilter, int state)
{
	spin_lock_irq(&dmxdevfilter->dev->lock);
	dmxdevfilter->state = state;
	spin_unlock_irq(&dmxdevfilter->dev->lock);
}

static int dvb_dmxdev_set_buffer_size(struct dmxdev_filter *dmxdevfilter,
				      unsigned long size)
{
	struct dvb_ringbuffer *buf = &dmxdevfilter->buffer;
	void *newmem;
	void *oldmem;

	if (buf->size == size)
		return 0;
	if (!size)
		return -EINVAL;
	if (dmxdevfilter->state >= DMXDEV_STATE_GO)
		return -EBUSY;

	newmem = vmalloc(size);
	if (!newmem)
		return -ENOMEM;

	oldmem = buf->data;

	spin_lock_irq(&dmxdevfilter->dev->lock);
	buf->data = newmem;
	buf->size = size;
 
	/* reset and not flush in case the buffer shrinks */
	dvb_ringbuffer_reset(buf);
	spin_unlock_irq(&dmxdevfilter->dev->lock);

	vfree(oldmem);

	return 0;
}

static void dvb_dmxdev_filter_timeout(unsigned long data)
{
	struct dmxdev_filter *dmxdevfilter = (struct dmxdev_filter *)data;

	dmxdevfilter->buffer.error = -ETIMEDOUT;
	spin_lock_irq(&dmxdevfilter->dev->lock);
	dmxdevfilter->state = DMXDEV_STATE_TIMEDOUT;
	spin_unlock_irq(&dmxdevfilter->dev->lock);
	wake_up(&dmxdevfilter->buffer.queue);
}

static void dvb_dmxdev_filter_timer(struct dmxdev_filter *dmxdevfilter)
{
	struct dmx_sct_filter_params *para = &dmxdevfilter->params.sec;

	del_timer(&dmxdevfilter->timer);
	if (para->timeout) {
		dmxdevfilter->timer.function = dvb_dmxdev_filter_timeout;
		dmxdevfilter->timer.data = (unsigned long)dmxdevfilter;
		dmxdevfilter->timer.expires =
		    jiffies + 1 + (HZ / 2 + HZ * para->timeout) / 1000;
		add_timer(&dmxdevfilter->timer);
	}
}

static int dvb_dmxdev_section_callback(const u8 *buffer1, size_t buffer1_len,
				       const u8 *buffer2, size_t buffer2_len,
				       struct dmx_section_filter *filter)
{
	struct dmxdev_filter *dmxdevfilter = filter->priv;
	int ret;
	ssize_t free = 0;

	if (dmxdevfilter->buffer.error) {
		wake_up(&dmxdevfilter->buffer.queue);
		return 0;
	}
	spin_lock(&dmxdevfilter->dev->lock);
	if (dmxdevfilter->state != DMXDEV_STATE_GO) {
		spin_unlock(&dmxdevfilter->dev->lock);
		return 0;
	}
	free = dvb_ringbuffer_free(&dmxdevfilter->buffer);
	if (free < (buffer1_len +buffer2_len)) {
		ret = free -(buffer1_len +buffer2_len) ;
		spin_unlock(&dmxdevfilter->dev->lock);
		wake_up(&dmxdevfilter->buffer.queue);
		return ret;
	}
	del_timer(&dmxdevfilter->timer);
	dprintk("dmxdev: section callback %*ph\n", 6, buffer1);

	ret = dvb_dmxdev_buffer_write(&dmxdevfilter->buffer, buffer1,
				      buffer1_len);
	if (ret == buffer1_len) {
		ret = dvb_dmxdev_buffer_write(&dmxdevfilter->buffer, buffer2,
					      buffer2_len);
	}
	if (ret < 0) {
		dmxdevfilter->buffer.error = ret;
		dvb_ringbuffer_flush(&dmxdevfilter->buffer);
		dprintk("dmxdev:section overflow, flush\n");
	}
	if (dmxdevfilter->params.sec.flags & DMX_ONESHOT)
		dmxdevfilter->state = DMXDEV_STATE_DONE;
	spin_unlock(&dmxdevfilter->dev->lock);
	wake_up(&dmxdevfilter->buffer.queue);
	return ret;
}

static int dvb_dmxdev_ts_callback(const u8 *buffer1, size_t buffer1_len,
				  const u8 *buffer2, size_t buffer2_len,
				  struct dmx_ts_feed *feed)
{
	struct dmxdev_filter *dmxdevfilter = feed->priv;
	struct dvb_ringbuffer *buffer;
	int ret;
	ssize_t free = 0;

	spin_lock(&dmxdevfilter->dev->lock);
	if (dmxdevfilter->params.pes.output == DMX_OUT_DECODER) {
		spin_unlock(&dmxdevfilter->dev->lock);
		return 0;
	}

	dprintk("dmxdev: ts callback %*ph\n", 6, buffer1);
	if (dmxdevfilter->params.pes.output == DMX_OUT_TAP
	    || dmxdevfilter->params.pes.output == DMX_OUT_TSDEMUX_TAP)
		buffer = &dmxdevfilter->buffer;
	else
		buffer = &dmxdevfilter->dev->dvr_buffer;
	if (buffer->error) {
		spin_unlock(&dmxdevfilter->dev->lock);
		wake_up(&buffer->queue);
		return 0;
	}
	free = dvb_ringbuffer_free(buffer);
	if (free < (buffer1_len +buffer2_len)) {
		ret = free -(buffer1_len +buffer2_len) ;
		goto end;
	}
	ret = dvb_dmxdev_buffer_write(buffer, buffer1, buffer1_len);
	if (ret == buffer1_len)
		ret = dvb_dmxdev_buffer_write(buffer, buffer2, buffer2_len);
	if (ret < 0) {
		buffer->error = ret;
		dvb_ringbuffer_flush(buffer);
		dprintk("dmxdev: ts  overflow, flush\n");
	}
end:
	spin_unlock(&dmxdevfilter->dev->lock);
	wake_up(&buffer->queue);
	return ret;
}

static int dvb_dmxdev_temi_callback(const u8 *buffer1, size_t buffer1_len,
									const u8 *buffer2, size_t buffer2_len,
									struct dmx_temi_feed *feed)
{
	struct dmxdev_filter *dmxdevfilter = feed->priv;
	struct dvb_ringbuffer *buffer;
	int ret;


	spin_lock(&dmxdevfilter->dev->lock);
	if (dmxdevfilter->state != DMXDEV_STATE_GO) {
		spin_unlock(&dmxdevfilter->dev->lock);
		return 0;
	}
	dprintk("dmxdev: temi callback %*ph\n", 6, buffer1);
	buffer = &dmxdevfilter->buffer;
	if (dmxdevfilter->buffer.error) {
		spin_unlock(&dmxdevfilter->dev->lock);
		wake_up(&buffer->queue);
		return 0;
	}
	ret = dvb_dmxdev_buffer_write(buffer, buffer1, buffer1_len);
	if (ret == buffer1_len)
		ret = dvb_dmxdev_buffer_write(buffer, buffer2, buffer2_len);
	if (ret < 0)
		buffer->error = ret;

	spin_unlock(&dmxdevfilter->dev->lock);
	wake_up(&buffer->queue);
	return 0;
}

/* stop feed but only mark the specified filter as stopped (state set) */
static int dvb_dmxdev_feed_stop(struct dmxdev_filter *dmxdevfilter)
{
	struct dmxdev_feed *feed;

	dvb_dmxdev_filter_state_set(dmxdevfilter, DMXDEV_STATE_SET);

	switch (dmxdevfilter->type) {
	case DMXDEV_TYPE_SEC:
		del_timer(&dmxdevfilter->timer);
		dmxdevfilter->feed.sec->stop_filtering(dmxdevfilter->feed.sec);
		break;
	case DMXDEV_TYPE_PES:
		list_for_each_entry(feed, &dmxdevfilter->feed.ts, next)
			feed->ts->stop_filtering(feed->ts);
		break;
	case DMXDEV_TYPE_TEMI:
		dmxdevfilter->feed.temi->stop_filtering(dmxdevfilter->feed.temi);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/* start feed associated with the specified filter */
static int dvb_dmxdev_feed_start(struct dmxdev_filter *filter)
{
	struct dmxdev_feed *feed;
	int ret;

	dvb_dmxdev_filter_state_set(filter, DMXDEV_STATE_GO);

	switch (filter->type) {
	case DMXDEV_TYPE_SEC:
		return filter->feed.sec->start_filtering(filter->feed.sec);
	case DMXDEV_TYPE_PES:
		list_for_each_entry(feed, &filter->feed.ts, next) {
			ret = feed->ts->start_filtering(feed->ts);
			if (ret < 0) {
				dvb_dmxdev_feed_stop(filter);
				return ret;
			}
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* restart section feed if it has filters left associated with it,
   otherwise release the feed */
static int dvb_dmxdev_feed_restart(struct dmxdev_filter *filter)
{
	//int i;
	struct dmxdev *dmxdev = filter->dev;
	//u16 pid = filter->params.sec.pid;

	/*
	for (i = 0; i < dmxdev->filternum; i++)
		if (dmxdev->filter[i].state >= DMXDEV_STATE_GO &&
		    dmxdev->filter[i].type == DMXDEV_TYPE_SEC &&
		    dmxdev->filter[i].params.sec.pid == pid) {
			dvb_dmxdev_feed_start(&dmxdev->filter[i]);
			return 0;
		}
*/
	filter->dev->demux->release_section_feed(dmxdev->demux,
						 filter->feed.sec);

	return 0;
}

static int dvb_dmxdev_filter_stop(struct dmxdev_filter *dmxdevfilter)
{
	struct dmxdev *dmxdev = dmxdevfilter->dev;
	struct dmxdev_feed *feed;
	struct dmx_demux *demux;

	if (dmxdevfilter->state < DMXDEV_STATE_GO)
		return 0;

	switch (dmxdevfilter->type) {
	case DMXDEV_TYPE_SEC:
		if (!dmxdevfilter->feed.sec)
			break;
		dvb_dmxdev_feed_stop(dmxdevfilter);
		if (dmxdevfilter->filter.sec)
			dmxdevfilter->feed.sec->
			    release_filter(dmxdevfilter->feed.sec,
					   dmxdevfilter->filter.sec);
		dvb_dmxdev_feed_restart(dmxdevfilter);
		dmxdevfilter->feed.sec = NULL;
		break;
	case DMXDEV_TYPE_PES:
		dvb_dmxdev_feed_stop(dmxdevfilter);
		demux = dmxdevfilter->dev->demux;
		list_for_each_entry(feed, &dmxdevfilter->feed.ts, next) {
			demux->release_ts_feed(demux, feed->ts);
			feed->ts = NULL;
		}
		if (dmxdevfilter->params.pes.output == DMX_OUT_TS_TAP
			&& dmxdev->dvrActiveFilterCnt > 0) {
			dmxdev->dvrActiveFilterCnt--;
			printk("(%s,%d): pid=%d, dvrActiveFilterCnt=%d\n",__func__, __LINE__,
			dmxdevfilter->params.pes.pid, dmxdev->dvrActiveFilterCnt);
		}
		break;
	case DMXDEV_TYPE_TEMI:
		if (!dmxdevfilter->feed.temi)
			break;
		dvb_dmxdev_feed_stop(dmxdevfilter);
		demux = dmxdevfilter->dev->demux;
		demux->release_temi_feed(demux, dmxdevfilter->feed.temi);
		dmxdevfilter->feed.temi = NULL;
		break;
	default:
		if (dmxdevfilter->state == DMXDEV_STATE_ALLOCATED)
			return 0;
		return -EINVAL;
	}

	dvb_ringbuffer_flush(&dmxdevfilter->buffer);
	return 0;
}

static void dvb_dmxdev_delete_pids(struct dmxdev_filter *dmxdevfilter)
{
	struct dmxdev_feed *feed, *tmp;

	/* delete all PIDs */
	list_for_each_entry_safe(feed, tmp, &dmxdevfilter->feed.ts, next) {
		list_del(&feed->next);
		kfree(feed);
	}

	BUG_ON(!list_empty(&dmxdevfilter->feed.ts));
}

static inline int dvb_dmxdev_filter_reset(struct dmxdev_filter *dmxdevfilter)
{
	if (dmxdevfilter->state < DMXDEV_STATE_SET)
		return 0;

	if (dmxdevfilter->type == DMXDEV_TYPE_PES)
		dvb_dmxdev_delete_pids(dmxdevfilter);

	dmxdevfilter->type = DMXDEV_TYPE_NONE;
	dvb_dmxdev_filter_state_set(dmxdevfilter, DMXDEV_STATE_ALLOCATED);
	return 0;
}

static int dvb_dmxdev_start_feed(struct dmxdev *dmxdev,
				 struct dmxdev_filter *filter,
				 struct dmxdev_feed *feed)
{
	struct timespec timeout = { 0 };
	struct dmx_pes_filter_params *para = &filter->params.pes;
	dmx_output_t otype;
	int ret;
	int ts_type;
	enum dmx_ts_pes ts_pes;
	struct dmx_ts_feed *tsfeed;

	feed->ts = NULL;
	otype = para->output;

	ts_pes = para->pes_type;

	if (ts_pes < DMX_PES_OTHER)
		ts_type = TS_DECODER;
	else
		ts_type = 0;

	if (otype == DMX_OUT_TS_TAP) {
		ts_type |= TS_PACKET;
	} else if (otype == DMX_OUT_TSDEMUX_TAP)
		ts_type |= TS_PACKET | TS_DEMUX;
	else if (otype == DMX_OUT_TAP) {
		ts_type |= TS_PACKET | TS_DEMUX | TS_PAYLOAD_ONLY;
		if (ts_pes == DMX_PES_OTHER)
			ts_pes = DMX_PES_SUBTITLE0;
	}

	ret = dmxdev->demux->allocate_ts_feed(dmxdev->demux, &feed->ts,
					      dvb_dmxdev_ts_callback);
	if (ret < 0)
		return ret;

	tsfeed = feed->ts;
	tsfeed->priv = filter;

	ret = tsfeed->set(tsfeed, feed->pid, ts_type, ts_pes, 32768, timeout);
	if (ret < 0) {
		dmxdev->demux->release_ts_feed(dmxdev->demux, tsfeed);
		return ret;
	}

	ret = tsfeed->start_filtering(tsfeed);
	if (ret < 0) {
		dmxdev->demux->release_ts_feed(dmxdev->demux, tsfeed);
		return ret;
	}

	return 0;
}

static int dvb_dmxdev_start_temi_feed(struct dmxdev *dmxdev,
									struct dmxdev_filter *filter,
									struct dmx_temi_feed **temifeed)
{
	int ret;
	u8 timeline_id;
	struct dmx_sct_filter_params *para = &filter->params.sec;
	struct dmx_demux *dmx = dmxdev->demux;
	ret = dmx->allocate_temi_feed(dmx, temifeed,dvb_dmxdev_temi_callback);
	if (ret < 0) {
		dprintk("DVB (%s): could not alloc feed\n", __func__);
		return ret;
	}
	(*temifeed)->priv = filter;
	timeline_id = para->filter.filter[0];
	ret = (*temifeed)->set(*temifeed, para->pid, timeline_id);
	if (ret < 0) {
		dmx->release_temi_feed(dmx, *temifeed);
		return ret;
	}
	ret = (*temifeed)->start_filtering(*temifeed);
	if (ret < 0) {
		dmx->release_temi_feed(dmx, *temifeed);
		return ret;
	}

	return 0;
}

static int dvb_dmxdev_filter_start(struct dmxdev_filter *filter)
{
	struct dmxdev *dmxdev = filter->dev;
	struct dmxdev_feed *feed;
	void *mem;
	int ret;
	//int i;

	if (filter->state < DMXDEV_STATE_SET)
		return -EINVAL;

	if (filter->state >= DMXDEV_STATE_GO)
		dvb_dmxdev_filter_stop(filter);

	if (!filter->buffer.data) {
		mem = vmalloc(filter->buffer.size);
		if (!mem)
			return -ENOMEM;
		spin_lock_irq(&filter->dev->lock);
		filter->buffer.data = mem;
		spin_unlock_irq(&filter->dev->lock);
	}

	dvb_ringbuffer_flush(&filter->buffer);

	switch (filter->type) {
	case DMXDEV_TYPE_SEC:
	{
		struct dmx_sct_filter_params *para = &filter->params.sec;
		struct dmx_section_filter **secfilter = &filter->filter.sec;
		struct dmx_section_feed **secfeed = &filter->feed.sec;

		*secfilter = NULL;
		*secfeed = NULL;


		/* find active filter/feed with same PID */
		/*
		for (i = 0; i < dmxdev->filternum; i++) {
			if (dmxdev->filter[i].state >= DMXDEV_STATE_GO &&
			    dmxdev->filter[i].type == DMXDEV_TYPE_SEC &&
			    dmxdev->filter[i].params.sec.pid == para->pid) {
				*secfeed = dmxdev->filter[i].feed.sec;
				break;
			}
		}
		*/
		/* if no feed found, try to allocate new one */
		if (!*secfeed) {
			ret = dmxdev->demux->allocate_section_feed(dmxdev->demux,
								   secfeed,
								   dvb_dmxdev_section_callback);
			if (ret < 0) {
				dprintk("DVB (%s): could not alloc feed\n",
				       __func__);
				return ret;
			}

			ret = (*secfeed)->set(*secfeed, para->pid, 32768,
					      (para->flags & DMX_CHECK_CRC) ? 1 : 0);
			if (ret < 0) {
				dprintk("DVB (%s): could not set feed\n",
				       __func__);
				dvb_dmxdev_feed_restart(filter);
				return ret;
			}
		} else {
			dvb_dmxdev_feed_stop(filter);
		}

		ret = (*secfeed)->allocate_filter(*secfeed, secfilter);
		if (ret < 0) {
			dvb_dmxdev_feed_restart(filter);
			filter->feed.sec->start_filtering(*secfeed);
			dprintk("could not get filter\n");
			return ret;
		}

		(*secfilter)->priv = filter;

		memcpy(&((*secfilter)->filter_value[3]),
		       &(para->filter.filter[1]), DMX_FILTER_SIZE - 1);
		memcpy(&(*secfilter)->filter_mask[3],
		       &para->filter.mask[1], DMX_FILTER_SIZE - 1);
		memcpy(&(*secfilter)->filter_mode[3],
		       &para->filter.mode[1], DMX_FILTER_SIZE - 1);

		(*secfilter)->filter_value[0] = para->filter.filter[0];
		(*secfilter)->filter_mask[0] = para->filter.mask[0];
		(*secfilter)->filter_mode[0] = para->filter.mode[0];
		(*secfilter)->filter_mask[1] = 0;
		(*secfilter)->filter_mask[2] = 0;

		(*secfilter)->one_shot = (para->flags & DMX_ONESHOT);

		filter->todo = 0;

		ret = filter->feed.sec->start_filtering(filter->feed.sec);
		if (ret < 0)
			return ret;

		dvb_dmxdev_filter_timer(filter);
		break;
	}
	case DMXDEV_TYPE_PES:
		list_for_each_entry(feed, &filter->feed.ts, next) {
			ret = dvb_dmxdev_start_feed(dmxdev, filter, feed);
			if (ret < 0) {
				dvb_dmxdev_filter_stop(filter);
				return ret;
			}
		}
		if (filter->params.pes.output == DMX_OUT_TS_TAP) {
			dmxdev->dvrActiveFilterCnt++;
			printk("(%s,%d): pid=%d, dvrActiveFilterCnt=%d\n",__func__, __LINE__,
				filter->params.pes.pid, dmxdev->dvrActiveFilterCnt);
		}
		break;
	case DMXDEV_TYPE_TEMI:
		ret = dvb_dmxdev_start_temi_feed(dmxdev, filter, &filter->feed.temi);
		if (ret < 0) {
			dvb_dmxdev_filter_stop(filter);
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}

	dvb_dmxdev_filter_state_set(filter, DMXDEV_STATE_GO);
	return 0;
}

static int dvb_demux_open(struct inode *inode, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dmxdev *dmxdev = dvbdev->priv;
	int i;
	struct dmxdev_filter *dmxdevfilter;

	if (!dmxdev->filter)
		return -EINVAL;

	if (mutex_lock_interruptible(&dmxdev->mutex))
		return -ERESTARTSYS;

	for (i = 0; i < dmxdev->filternum; i++)
		if (dmxdev->filter[i].state == DMXDEV_STATE_FREE)
			break;

	if (i == dmxdev->filternum) {
		mutex_unlock(&dmxdev->mutex);
		return -EMFILE;
	}

	dmxdevfilter = &dmxdev->filter[i];
	mutex_init(&dmxdevfilter->mutex);
	file->private_data = dmxdevfilter;

	dvb_ringbuffer_init(&dmxdevfilter->buffer, NULL, 8192);
	dmxdevfilter->type = DMXDEV_TYPE_NONE;
	dvb_dmxdev_filter_state_set(dmxdevfilter, DMXDEV_STATE_ALLOCATED);
	init_timer(&dmxdevfilter->timer);

	dvbdev->users++;

	mutex_unlock(&dmxdev->mutex);
	return 0;
}

static int dvb_dmxdev_filter_free(struct dmxdev *dmxdev,
				  struct dmxdev_filter *dmxdevfilter)
{
	mutex_lock(&dmxdev->mutex);
	mutex_lock(&dmxdevfilter->mutex);

	dvb_dmxdev_filter_stop(dmxdevfilter);
	dvb_dmxdev_filter_reset(dmxdevfilter);

	if (dmxdevfilter->buffer.data) {
		void *mem = dmxdevfilter->buffer.data;

		spin_lock_irq(&dmxdev->lock);
		dmxdevfilter->buffer.data = NULL;
		spin_unlock_irq(&dmxdev->lock);
		vfree(mem);
	}

	dvb_dmxdev_filter_state_set(dmxdevfilter, DMXDEV_STATE_FREE);
	wake_up(&dmxdevfilter->buffer.queue);
	mutex_unlock(&dmxdevfilter->mutex);
	mutex_unlock(&dmxdev->mutex);
	return 0;
}

static inline void invert_mode(dmx_filter_t *filter)
{
	int i;

	for (i = 0; i < DMX_FILTER_SIZE; i++)
		filter->mode[i] ^= 0xff;
}

static int dvb_dmxdev_add_pid(struct dmxdev *dmxdev,
			      struct dmxdev_filter *filter, u16 pid)
{
	struct dmxdev_feed *feed;

	if ((filter->type != DMXDEV_TYPE_PES) ||
	    (filter->state < DMXDEV_STATE_SET))
		return -EINVAL;

	/* only TS packet filters may have multiple PIDs */
	if ((filter->params.pes.output != DMX_OUT_TSDEMUX_TAP) &&
	    (!list_empty(&filter->feed.ts)))
		return -EINVAL;

	feed = kzalloc(sizeof(struct dmxdev_feed), GFP_KERNEL);
	if (feed == NULL)
		return -ENOMEM;

	feed->pid = pid;
	list_add(&feed->next, &filter->feed.ts);

	if (filter->state >= DMXDEV_STATE_GO)
		return dvb_dmxdev_start_feed(dmxdev, filter, feed);

	return 0;
}

static int dvb_dmxdev_remove_pid(struct dmxdev *dmxdev,
				  struct dmxdev_filter *filter, u16 pid)
{
	struct dmxdev_feed *feed, *tmp;

	if ((filter->type != DMXDEV_TYPE_PES) ||
	    (filter->state < DMXDEV_STATE_SET))
		return -EINVAL;

	list_for_each_entry_safe(feed, tmp, &filter->feed.ts, next) {
		if ((feed->pid == pid) && (feed->ts != NULL)) {
			feed->ts->stop_filtering(feed->ts);
			filter->dev->demux->release_ts_feed(filter->dev->demux,
							    feed->ts);
			list_del(&feed->next);
			kfree(feed);
		}
	}

	return 0;
}

static int dvb_dmxdev_filter_set(struct dmxdev *dmxdev,
				 struct dmxdev_filter *dmxdevfilter,
				 struct dmx_sct_filter_params *params)
{
	dprintk("function : %s, PID=0x%04x, flags=%02x, buf_size:0x%x timeout=%d\n",
		__func__, params->pid, params->flags, dmxdevfilter->buffer.size, params->timeout);

	dvb_dmxdev_filter_stop(dmxdevfilter);

	dmxdevfilter->type = DMXDEV_TYPE_SEC;
	memcpy(&dmxdevfilter->params.sec,
	       params, sizeof(struct dmx_sct_filter_params));
	invert_mode(&dmxdevfilter->params.sec.filter);
	dvb_dmxdev_filter_state_set(dmxdevfilter, DMXDEV_STATE_SET);

	if (params->flags & DMX_IMMEDIATE_START)
		return dvb_dmxdev_filter_start(dmxdevfilter);

	return 0;
}

static int dvb_dmxdev_pes_filter_set(struct dmxdev *dmxdev,
				     struct dmxdev_filter *dmxdevfilter,
				     struct dmx_pes_filter_params *params)
{
	int ret;

	dvb_dmxdev_filter_stop(dmxdevfilter);
	dvb_dmxdev_filter_reset(dmxdevfilter);

	if ((unsigned)params->pes_type > DMX_PES_OTHER)
		return -EINVAL;
	printk("%s_%d: in:%d, out:%d pes_type:%d\n",
		__func__, __LINE__, params->input, params->output, params->pes_type);
	dmxdevfilter->type = DMXDEV_TYPE_PES;
	memcpy(&dmxdevfilter->params, params,
	       sizeof(struct dmx_pes_filter_params));
	INIT_LIST_HEAD(&dmxdevfilter->feed.ts);

	dvb_dmxdev_filter_state_set(dmxdevfilter, DMXDEV_STATE_SET);

	ret = dvb_dmxdev_add_pid(dmxdev, dmxdevfilter,
				 dmxdevfilter->params.pes.pid);
	if (ret < 0)
		return ret;

	if (params->flags & DMX_IMMEDIATE_START)
		return dvb_dmxdev_filter_start(dmxdevfilter);

	return 0;
}

static int dvb_dmxdev_temi_filter_set(struct dmxdev *dmxdev,
				     struct dmxdev_filter *dmxdevfilter,
				     struct dmx_pes_filter_params *params)
{
	dvb_dmxdev_filter_stop(dmxdevfilter);

	dmxdevfilter->type = DMXDEV_TYPE_TEMI;
	memcpy(&dmxdevfilter->params.sec,
		   params, sizeof(struct dmx_sct_filter_params));
	invert_mode(&dmxdevfilter->params.sec.filter);
	dvb_dmxdev_filter_state_set(dmxdevfilter, DMXDEV_STATE_SET);

	if (params->flags & DMX_IMMEDIATE_START)
		return dvb_dmxdev_filter_start(dmxdevfilter);
	return 0;
}

static ssize_t dvb_dmxdev_read_sec(struct dmxdev_filter *dfil,
				   struct file *file, char __user *buf,
				   size_t count, loff_t *ppos)
{
	int result, hcount;
	int done = 0;
	if (dfil->todo <= 0) {
		hcount = 3 + dfil->todo;
		if (hcount > count)
			hcount = count;
		result = dvb_dmxdev_buffer_read(&dfil->buffer,
						file->f_flags & O_NONBLOCK,
						buf, hcount, ppos);
		if (result < 0) {
			dfil->todo = 0;
			return result;
		}
		if (copy_from_user(dfil->secheader - dfil->todo, buf, result))
			return -EFAULT;
		buf += result;
		done = result;
		count -= result;
		dfil->todo -= result;
		if (dfil->todo > -3)
			return done;
		dfil->todo = ((dfil->secheader[1] << 8) | dfil->secheader[2]) & 0xfff;
		if (!count)
			return done;
	}
	if (count > dfil->todo)
		count = dfil->todo;
	result = dvb_dmxdev_buffer_read(&dfil->buffer,
					file->f_flags & O_NONBLOCK,
					buf, count, ppos);
	if (result < 0)
		return result;
	dfil->todo -= result;
	return (result + done);
}

static ssize_t
dvb_demux_read(struct file *file, char __user *buf, size_t count,
	       loff_t *ppos)
{
	struct dmxdev_filter *dmxdevfilter = file->private_data;
	int ret;

	if (mutex_lock_interruptible(&dmxdevfilter->mutex))
		return -ERESTARTSYS;

	if (dmxdevfilter->type == DMXDEV_TYPE_SEC)
		ret = dvb_dmxdev_read_sec(dmxdevfilter, file, buf, count, ppos);
	else{
		ret = dvb_dmxdev_buffer_read(&dmxdevfilter->buffer,
					     file->f_flags & O_NONBLOCK,
					     buf, count, ppos);
	}

	mutex_unlock(&dmxdevfilter->mutex);
	return ret;
}


static int dvb_dmxdev_prop_set(struct dmxdev *dmxdev, 
				struct dmx_ext_control *dmx_ex_con)
{
	void *  parg = NULL;
	void *  mbuf = NULL;
	char    sbuf[128];
	struct dmx_demux *demux = dmxdev->demux;
	int     err  = 0;
	if (!demux->prop_set) {
		return -ENODEV;
	}
	if ( dmx_ex_con->size ){
		if (dmx_ex_con->size <= sizeof(sbuf)) {
				parg = sbuf;
		} else {
			/* too big to allocate from stack */
			mbuf = kmalloc(dmx_ex_con->size ,GFP_KERNEL);
			if (NULL == mbuf)
				return -ENOMEM;
			parg = mbuf;
		}
		
		if (copy_from_user(parg, (void __user *)dmx_ex_con->ptr,
			dmx_ex_con->size )){
			err = -EFAULT;
			goto out;
		}
	}
	else
		parg = &dmx_ex_con->value64;
	if (demux->prop_set(demux, dmx_ex_con->id, parg) < 0) {
		err = -EFAULT;
	}
out:
	if (mbuf)
		kfree(mbuf);
	return err;

}

static int dvb_dmxdev_prop_get(struct dmxdev *dmxdev, 
				struct dmx_ext_control *dmx_ex_con)
{
	void *  parg = NULL;
	void *  mbuf = NULL;
	char    sbuf[128];
	struct dmx_demux *demux = dmxdev->demux;
	int     err  = 0;
	if (!demux->prop_get) {
		return -ENODEV;
	}
	if ( dmx_ex_con->size ){
		if (dmx_ex_con->size <= sizeof(sbuf)) {
				parg = sbuf;
		} else {
			/* too big to allocate from stack */
			mbuf = kmalloc(dmx_ex_con->size ,GFP_KERNEL);
			if (NULL == mbuf)
				return -ENOMEM;
			parg = mbuf;
		}
		
		if (copy_from_user(parg, (void __user *)dmx_ex_con->ptr,
			dmx_ex_con->size )) {
			err = -EFAULT;
			goto out;
		}
	}
	else
		parg = &dmx_ex_con->value64;
	if (demux->prop_get(demux, dmx_ex_con->id, parg) < 0) {
		err = -EFAULT;
		goto out;
	}

	if (dmx_ex_con->size) {
		if (copy_to_user((void __user *)dmx_ex_con->ptr, parg,
			dmx_ex_con->size))
			err = -EFAULT;
	}
out:
	if (mbuf)
		kfree(mbuf);
	return err;
}

static int dvb_dmxdev_filter_ctrl(struct dmxdev_filter *dmxdevfilter, u32 cmd, 
					struct dmx_ext_control *dmx_ex_con)
{
	int ret = 0;
	struct dmxdev_feed *feed = NULL;
	void *  parg = NULL;
	void *  mbuf = NULL;
	char    sbuf[128];
	if (dmxdevfilter->type != DMXDEV_TYPE_PES ||
		dmxdevfilter->state != DMXDEV_STATE_GO)
		return -EINVAL;


	if (dmx_ex_con){
		if ( dmx_ex_con->size ){
			if (dmx_ex_con->size <= sizeof(sbuf)) {
					parg = sbuf;
			} else {
				/* too big to allocate from stack */
				mbuf = kmalloc(dmx_ex_con->size ,GFP_KERNEL);
				if (NULL == mbuf)
					return -ENOMEM;
				parg = mbuf;
			}
			
			if (copy_from_user(parg, (void __user *)dmx_ex_con->ptr,
				dmx_ex_con->size )) {
				ret = -EFAULT;
				goto out;
			}
		}
		else
			parg = &dmx_ex_con->value64;
	}
	list_for_each_entry(feed, &dmxdevfilter->feed.ts, next) {
		ret = feed->ts->control(feed->ts, cmd, parg);
		if (ret < 0) 
			break;
	}

out:
	if (mbuf)
		kfree(mbuf);
	return ret;
}

static int dvb_demux_do_ioctl_rtk_ex_set(struct dmxdev_filter *dmxdevfilter,
			      struct dmx_ext_control *dmx_ex_con)
{
	int ret = 0;
	struct dmxdev *dmxdev = dmxdevfilter->dev;

	switch(dmx_ex_con->id) {
	case DMX_EXT_CID_PLATFORM:
	case DMX_EXT_CID_COUNTRY:
	case DMX_EXT_CID_MODEL_NO:
	case DMX_EXT_CID_CHIP_ID:
	case DMX_EXT_CID_DRV_VER:
	case DMX_EXT_CID_PORT_NUM:
	case DMX_EXT_CID_AFIFO:
	case DMX_EXT_CID_INPUTSOURCE:
	case DMX_EXT_CID_PCR_ONOFF:
	case DMX_EXT_CID_DSCRMB_TYPE:
	case DMX_EXT_CID_DSCRMB_KEY:
	case DMX_EXT_CID_ECP_INFO_NOTI:
	{
		ret = dvb_dmxdev_prop_set(dmxdev, dmx_ex_con);
		break;
	}
	case DMX_EXT_CID_REQUEST_SCRMB:
	{
		struct dmx_pes_filter_params params;
		params.pid = dmx_ex_con->value64;
		params.input = DMX_IN_FRONTEND;
		params.output = DMX_OUT_TSDEMUX_TAP;
		params.pes_type = DMX_PES_OTHER;
		params.flags = DMX_IMMEDIATE_START;
		
		if (mutex_lock_interruptible(&dmxdevfilter->mutex)) {
			return -ERESTARTSYS;
		}
		ret = dvb_dmxdev_pes_filter_set(dmxdev, dmxdevfilter, &params);
		if (ret < 0) {
			pr_err("%s_%d:scramble fileter set FAIL!\n", __func__, __LINE__);
		}
		ret = dvb_dmxdev_filter_ctrl(dmxdevfilter, dmx_ex_con->id, NULL);
		if (ret < 0) {
			pr_err("%s_%d: scramble check Enable FAIL!\n", __func__, __LINE__);
		}
		mutex_unlock(&dmxdevfilter->mutex);
		break;
	}
	case DMX_EXT_CID_CANCEL_SCRMB:
	{
		if (mutex_lock_interruptible(&dmxdevfilter->mutex)) {
			return -ERESTARTSYS;
		}
		ret = dvb_dmxdev_filter_ctrl(dmxdevfilter, dmx_ex_con->id, NULL);
		ret = dvb_dmxdev_filter_stop(dmxdevfilter);
		mutex_unlock(&dmxdevfilter->mutex);
		break;
	}
	case DMX_EXT_CID_ADD_PID:
		ret = -EINVAL;
		break;
	//base  on exist filters.
	case DMX_EXT_CID_CHECK_SCRMB:
	case DMX_EXT_CID_DSCRMB_PID:
	{
		if (mutex_lock_interruptible(&dmxdevfilter->mutex))
			return -ERESTARTSYS;
		ret = dvb_dmxdev_filter_ctrl(dmxdevfilter, dmx_ex_con->id, dmx_ex_con);
		mutex_unlock(&dmxdevfilter->mutex);
		break;
	}
	default:
		ret = -EINVAL;
		break;

	}
	return ret;
}
static int dvb_demux_do_ioctl_rtk_ex_get(struct dmxdev_filter *dmxdevfilter,
			      struct dmx_ext_control *dmx_ex_con)
{
	int ret = 0;
	struct dmxdev *dmxdev = dmxdevfilter->dev;

	switch(dmx_ex_con->id) {
	case DMX_EXT_CID_PLATFORM:
	case DMX_EXT_CID_COUNTRY:
	case DMX_EXT_CID_MODEL_NO:
	case DMX_EXT_CID_CHIP_ID:
	case DMX_EXT_CID_DRV_VER:
	case DMX_EXT_CID_PORT_NUM:
	case DMX_EXT_CID_AFIFO:
	case DMX_EXT_CID_INPUTSOURCE:
	case DMX_EXT_CID_PCR_ONOFF:
	case DMX_EXT_CID_DSCRMB_TYPE:
	case DMX_EXT_CID_DSCRMB_PID:
	case DMX_EXT_CID_DSCRMB_KEY:
	{
		ret = dvb_dmxdev_prop_get(dmxdev, dmx_ex_con);
		break;
	}	
	case DMX_EXT_CID_REQUEST_SCRMB:
	case DMX_EXT_CID_CANCEL_SCRMB:
		ret = -EINVAL;
	break;
	case DMX_EXT_CID_CHECK_SCRMB:
	{
		
		if (mutex_lock_interruptible(&dmxdevfilter->mutex))
			return -ERESTARTSYS;
		ret = dvb_dmxdev_filter_ctrl(dmxdevfilter, dmx_ex_con->id, dmx_ex_con);
		mutex_unlock(&dmxdevfilter->mutex);
		break;
	}
	case DMX_EXT_CID_ADD_PID:
		ret = -EINVAL;
		break;
	default:
		ret = -EINVAL;
		break;

	}
	return ret;
}

static int dvb_demux_do_ioctl_rtk_ex(struct file *file,
			      unsigned int cmd, void *parg)
{
	struct dmxdev_filter *dmxdevfilter = file->private_data;
	struct dmx_ext_control *dmx_ex_con = parg; 
	int ret = 0;

	switch (cmd) {
	case DMX_EXT_S_CTL:
		ret = dvb_demux_do_ioctl_rtk_ex_set(dmxdevfilter, dmx_ex_con);
		break;
	case DMX_EXT_G_CTL:
		ret = dvb_demux_do_ioctl_rtk_ex_get(dmxdevfilter, dmx_ex_con);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret ;

}

/*
 * lg set size =  max_section_length *2
 * hack size to max_section_length *2 *2 +1  (4 max section)
 * eit section is  (40k)
*/
static unsigned long  dvb_dmxdev_buffer_size_reconfig(unsigned long size)
{
	if (size <4 *1024 *4) //4 section (16k)
		size = (size<<2) ; //*4
	return size +1;
}


static int dvb_demux_do_ioctl(struct file *file,
			      unsigned int cmd, void *parg)
{
	struct dmxdev_filter *dmxdevfilter = file->private_data;
	struct dmxdev *dmxdev = dmxdevfilter->dev;
	unsigned long arg = (unsigned long)parg;
	int ret = 0;

	if (mutex_lock_interruptible(&dmxdev->mutex))
		return -ERESTARTSYS;

	switch (cmd) {
	case DMX_START:
		if (mutex_lock_interruptible(&dmxdevfilter->mutex)) {
			mutex_unlock(&dmxdev->mutex);
			return -ERESTARTSYS;
		}
		if (dmxdevfilter->state < DMXDEV_STATE_SET)
			ret = -EINVAL;
		else
			ret = dvb_dmxdev_filter_start(dmxdevfilter);
		mutex_unlock(&dmxdevfilter->mutex);
		break;

	case DMX_STOP:
		if (mutex_lock_interruptible(&dmxdevfilter->mutex)) {
			mutex_unlock(&dmxdev->mutex);
			return -ERESTARTSYS;
		}
		ret = dvb_dmxdev_filter_stop(dmxdevfilter);
		mutex_unlock(&dmxdevfilter->mutex);
		break;

	case DMX_SET_FILTER:
		if (mutex_lock_interruptible(&dmxdevfilter->mutex)) {
			mutex_unlock(&dmxdev->mutex);
			return -ERESTARTSYS;
		}
		ret = dvb_dmxdev_filter_set(dmxdev, dmxdevfilter, parg);
		mutex_unlock(&dmxdevfilter->mutex);
		break;

	case DMX_SET_PES_FILTER:
		if (mutex_lock_interruptible(&dmxdevfilter->mutex)) {
			mutex_unlock(&dmxdev->mutex);
			return -ERESTARTSYS;
		}
		ret = dvb_dmxdev_pes_filter_set(dmxdev, dmxdevfilter, parg);
		mutex_unlock(&dmxdevfilter->mutex);
		break;

	case DMX_SET_BUFFER_SIZE:
		if (mutex_lock_interruptible(&dmxdevfilter->mutex)) {
			mutex_unlock(&dmxdev->mutex);
			return -ERESTARTSYS;
		}
		arg = dvb_dmxdev_buffer_size_reconfig(arg);
		ret = dvb_dmxdev_set_buffer_size(dmxdevfilter, arg);
		mutex_unlock(&dmxdevfilter->mutex);
		break;

	case DMX_GET_PES_PIDS:
		if (!dmxdev->demux->get_pes_pids) {
			ret = -EINVAL;
			break;
		}
		dmxdev->demux->get_pes_pids(dmxdev->demux, parg);
		break;

#if 0
	/* Not used upstream and never documented */

	case DMX_GET_CAPS:
		if (!dmxdev->demux->get_caps) {
			ret = -EINVAL;
			break;
		}
		ret = dmxdev->demux->get_caps(dmxdev->demux, parg);
		break;

	case DMX_SET_SOURCE:
		if (!dmxdev->demux->set_source) {
			ret = -EINVAL;
			break;
		}
		ret = dmxdev->demux->set_source(dmxdev->demux, parg);
		break;
#endif

	case DMX_GET_STC:
		if (!dmxdev->demux->get_stc) {
			ret = -EINVAL;
			break;
		}
		ret = dmxdev->demux->get_stc(dmxdev->demux,
					     ((struct dmx_stc *)parg)->num,
					     &((struct dmx_stc *)parg)->stc,
					     &((struct dmx_stc *)parg)->base);
		break;

	case DMX_ADD_PID:
		if (mutex_lock_interruptible(&dmxdevfilter->mutex)) {
			ret = -ERESTARTSYS;
			break;
		}
		ret = dvb_dmxdev_add_pid(dmxdev, dmxdevfilter, *(u16 *)parg);
		mutex_unlock(&dmxdevfilter->mutex);
		break;

	case DMX_REMOVE_PID:
		if (mutex_lock_interruptible(&dmxdevfilter->mutex)) {
			ret = -ERESTARTSYS;
			break;
		}
		ret = dvb_dmxdev_remove_pid(dmxdev, dmxdevfilter, *(u16 *)parg);
		mutex_unlock(&dmxdevfilter->mutex);
		break;
	case DMX_SET_TEMI_FILTER:
		if (mutex_lock_interruptible(&dmxdevfilter->mutex)) {
			ret = -ERESTARTSYS;
			break;
		}
		ret = dvb_dmxdev_temi_filter_set(dmxdev, dmxdevfilter, parg);
		mutex_unlock(&dmxdevfilter->mutex);
		break;
	default:
		ret = dvb_demux_do_ioctl_rtk_ex(file, cmd, parg);
		break;
	}
	mutex_unlock(&dmxdev->mutex);
	return ret;
}

static long dvb_demux_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	return dvb_usercopy(file, cmd, arg, dvb_demux_do_ioctl);
}

static unsigned int dvb_demux_poll(struct file *file, poll_table *wait)
{
	struct dmxdev_filter *dmxdevfilter = file->private_data;
	unsigned int mask = 0;

	if ((!dmxdevfilter) || dmxdevfilter->dev->exit)
		return POLLERR;

	poll_wait(file, &dmxdevfilter->buffer.queue, wait);

	if (dmxdevfilter->state != DMXDEV_STATE_GO &&
	    dmxdevfilter->state != DMXDEV_STATE_DONE &&
	    dmxdevfilter->state != DMXDEV_STATE_TIMEDOUT)
		return 0;

	if (dmxdevfilter->buffer.error)
		mask |= (POLLIN | POLLRDNORM | POLLPRI | POLLERR);

	if (!dvb_ringbuffer_empty(&dmxdevfilter->buffer))
		mask |= (POLLIN | POLLRDNORM | POLLPRI);

	return mask;
}

static int dvb_demux_release(struct inode *inode, struct file *file)
{
	struct dmxdev_filter *dmxdevfilter = file->private_data;
	struct dmxdev *dmxdev = dmxdevfilter->dev;

	int ret;

	ret = dvb_dmxdev_filter_free(dmxdev, dmxdevfilter);

	mutex_lock(&dmxdev->mutex);
	dmxdev->dvbdev->users--;
	if(dmxdev->dvbdev->users==1 && dmxdev->exit==1) {
		mutex_unlock(&dmxdev->mutex);
		wake_up(&dmxdev->dvbdev->wait_queue);
	} else
		mutex_unlock(&dmxdev->mutex);

	return ret;
}

static const struct file_operations dvb_demux_fops = {
	.owner = THIS_MODULE,
	.read = dvb_demux_read,
	.unlocked_ioctl = dvb_demux_ioctl,
	.open = dvb_demux_open,
	.release = dvb_demux_release,
	.poll = dvb_demux_poll,
	.llseek = default_llseek,
};

static const struct dvb_device dvbdev_demux = {
	.priv = NULL,
	.users = 1,
	.writers = 1,
#if defined(CONFIG_MEDIA_CONTROLLER_DVB)
	.name = "dvb-demux",
#endif
	.fops = &dvb_demux_fops
};

static int dvb_dvrdev_prop_set(struct dmxdev *dmxdev,
				struct pvr_ext_control *pvr_ext_ctl)
{
	void *  parg = NULL;
	void *  mbuf = NULL;
	char    sbuf[128];
	struct dmx_demux *demux = dmxdev->demux;
	int     err  = 0;

	if (!demux->pvr_prop_set) {
		return -ENODEV;
	}
	if (pvr_ext_ctl->size) {
		if (pvr_ext_ctl->size <= sizeof(sbuf)) {
				parg = sbuf;
		} else {
			/* too big to allocate from stack */
			mbuf = kmalloc(pvr_ext_ctl->size ,GFP_KERNEL);
			if (NULL == mbuf)
				return -ENOMEM;
			parg = mbuf;
		}

		if (copy_from_user(parg, (void __user *)pvr_ext_ctl->ptr,
			pvr_ext_ctl->size )){
			err = -EFAULT;
			goto out;
		}
	}
	else
		parg = &pvr_ext_ctl->value64;

	if (demux->pvr_prop_set(demux, pvr_ext_ctl->id, parg) < 0) {
		err = -EFAULT;
	}
out:
	if (mbuf)
		kfree(mbuf);

	return err;

}

static int dvb_dvr_do_ioctl_rtk_ex_set(struct dmxdev *dmxdev,
			      struct pvr_ext_control *pvr_ex_ctl)
{
	int ret = 0;

	ret = dvb_dvrdev_prop_set(dmxdev, pvr_ex_ctl);
	return ret;
}

static int dvb_dvr_do_ioctl_rtk_ex(struct file *file,
			      unsigned int cmd, void *parg)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dmxdev *dmxdev = dvbdev->priv;
	struct pvr_ext_control *pvr_ext_ctl = parg;
	int ret = 0;

	switch (cmd) {
	case PVR_EXT_S_CTL:
		ret = dvb_dvr_do_ioctl_rtk_ex_set(dmxdev, pvr_ext_ctl);
		break;
	case PVR_EXT_G_CTL:
		//ret = dvb_dvr_do_ioctl_rtk_ex_get(dmxdev, pvr_ext_ctl);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret ;

}

static int dvb_dvr_do_ioctl(struct file *file,
			    unsigned int cmd, void *parg)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dmxdev *dmxdev = dvbdev->priv;
	unsigned long arg = (unsigned long)parg;
	int ret;

	if (mutex_lock_interruptible(&dmxdev->mutex))
		return -ERESTARTSYS;

	switch (cmd) {
	case DMX_SET_BUFFER_SIZE:
		ret = dvb_dvr_set_buffer_size(dmxdev, arg);
		break;

	default:
		ret = dvb_dvr_do_ioctl_rtk_ex(file, cmd, parg);
		break;
	}
	mutex_unlock(&dmxdev->mutex);
	return ret;
}

static long dvb_dvr_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	return dvb_usercopy(file, cmd, arg, dvb_dvr_do_ioctl);
}

static unsigned int dvb_dvr_poll(struct file *file, poll_table *wait)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dmxdev *dmxdev = dvbdev->priv;
	unsigned int mask = 0;

	dprintk("function : %s\n", __func__);

	if (dmxdev->exit)
		return POLLERR;

	poll_wait(file, &dmxdev->dvr_buffer.queue, wait);

	if ((file->f_flags & O_ACCMODE) == O_RDONLY) {
		if (dmxdev->dvr_buffer.error)
			mask |= (POLLIN | POLLRDNORM | POLLPRI | POLLERR);

		if (!dvb_ringbuffer_empty(&dmxdev->dvr_buffer))
			mask |= (POLLIN | POLLRDNORM | POLLPRI);
	} else
		mask |= (POLLOUT | POLLWRNORM | POLLPRI);

	return mask;
}

static const struct file_operations dvb_dvr_fops = {
	.owner = THIS_MODULE,
	.read = dvb_dvr_read,
	.write = dvb_dvr_write,
	.unlocked_ioctl = dvb_dvr_ioctl,
	.open = dvb_dvr_open,
	.release = dvb_dvr_release,
	.poll = dvb_dvr_poll,
	.llseek = default_llseek,
};

static const struct dvb_device dvbdev_dvr = {
	.priv = NULL,
	.readers = 1,
	.users = 1,
#if defined(CONFIG_MEDIA_CONTROLLER_DVB)
	.name = "dvb-dvr",
#endif
	.fops = &dvb_dvr_fops
};
int dvb_dmxdev_init(struct dmxdev *dmxdev, struct dvb_adapter *dvb_adapter)
{
	int i;

	if (dmxdev->demux->open(dmxdev->demux) < 0)
		return -EUSERS;

	dmxdev->filter = vmalloc(dmxdev->filternum * sizeof(struct dmxdev_filter));
	if (!dmxdev->filter)
		return -ENOMEM;

	mutex_init(&dmxdev->mutex);
	spin_lock_init(&dmxdev->lock);
	for (i = 0; i < dmxdev->filternum; i++) {
		dmxdev->filter[i].dev = dmxdev;
		dmxdev->filter[i].buffer.data = NULL;
		dvb_dmxdev_filter_state_set(&dmxdev->filter[i],
					    DMXDEV_STATE_FREE);
	}

	dvb_register_device(dvb_adapter, &dmxdev->dvbdev, &dvbdev_demux, dmxdev,
			    DVB_DEVICE_DEMUX);
	dvb_register_device(dvb_adapter, &dmxdev->dvr_dvbdev, &dvbdev_dvr,
			    dmxdev, DVB_DEVICE_DVR);

	dvb_ringbuffer_init(&dmxdev->dvr_buffer, NULL, 8192);
	dmxdev->dvrActiveFilterCnt = 0;

	return 0;
}

EXPORT_SYMBOL(dvb_dmxdev_init);

void dvb_dmxdev_release(struct dmxdev *dmxdev)
{
	dmxdev->exit=1;
	if (dmxdev->dvbdev->users > 1) {
		wait_event(dmxdev->dvbdev->wait_queue,
				dmxdev->dvbdev->users==1);
	}
	if (dmxdev->dvr_dvbdev->users > 1) {
		wait_event(dmxdev->dvr_dvbdev->wait_queue,
				dmxdev->dvr_dvbdev->users==1);
	}

	dvb_unregister_device(dmxdev->dvbdev);
	dvb_unregister_device(dmxdev->dvr_dvbdev);

	vfree(dmxdev->filter);
	dmxdev->filter = NULL;
	dmxdev->demux->close(dmxdev->demux);
}

EXPORT_SYMBOL(dvb_dmxdev_release);
