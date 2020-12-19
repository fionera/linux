/*
 * dvb_ca.c: generic DVB functions for EN50221 CAM interfaces
 *
 * Copyright (C) 2004 Andrew de Quincey
 *
 * Parts of this file were based on sources as follows:
 *
 * Copyright (C) 2003 Ralph Metzler <rjkm@metzlerbros.de>
 *
 * based on code:
 *
 * Copyright (C) 1999-2002 Ralph  Metzler
 *                       & Marcus Metzler for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/freezer.h>

#include "dvb_ca_en50221.h"

static int dvb_ca_debug;

module_param_named(cam_debug, dvb_ca_debug, int, 0644);
MODULE_PARM_DESC(cam_debug, "enable verbose debug messages");

#define dprintk if (dvb_ca_debug) printk

#define INIT_TIMEOUT_SECS 10

//#define HOST_LINK_BUF_SIZE 0x1000

#define RX_BUFFER_SIZE (256*1024)

#define MAX_RX_PACKETS_PER_ITERATION 10

#define CTRLIF_DATA      0
#define CTRLIF_COMMAND   1
#define CTRLIF_STATUS    1
#define CTRLIF_SIZE_LOW  2
#define CTRLIF_SIZE_HIGH 3

#define CMDREG_HC        1	/* Host control */
#define CMDREG_SW        2	/* Size write */
#define CMDREG_SR        4	/* Size read */
#define CMDREG_RS        8	/* Reset interface */
#define CMDREG_FRIE   0x40	/* Enable FR interrupt */
#define CMDREG_DAIE   0x80	/* Enable DA interrupt */
#define IRQEN (CMDREG_DAIE)

#define STATUSREG_RE     1	/* read error */
#define STATUSREG_WE     2	/* write error */
#define STATUSREG_FR  0x40	/* module free */
#define STATUSREG_DA  0x80	/* data available */
#define STATUSREG_TXERR (STATUSREG_RE|STATUSREG_WE)	/* general transfer error */

#define CHECK_ERROR(condition,ret,msg)    \
{ \
        if(condition) \
        { \
                printk(msg); \
                ret; \
        } \
}
static void dvb_ca_en50221_thread_wakeup(struct dvb_ca_private *ca);
static int dvb_ca_en50221_read_data(struct dvb_ca_private *ca, int slot, u8 * ebuf, int ecount);
static int dvb_ca_en50221_write_data(struct dvb_ca_private *ca, int slot, u8 * ebuf, int ecount);
static int dvb_ca_en50221_write_negobuffer(struct dvb_ca_private *ca, int slot, u8 * buf, int bytes_write);


/**
 * Safely find needle in haystack.
 *
 * @haystack: Buffer to look in.
 * @hlen: Number of bytes in haystack.
 * @needle: Buffer to find.
 * @nlen: Number of bytes in needle.
 * @return Pointer into haystack needle was found at, or NULL if not found.
 */
static char *findstr(char * haystack, int hlen, char * needle, int nlen)
{
	int i;

	if (hlen < nlen)
		return NULL;

	for (i = 0; i <= hlen - nlen; i++) {
		if (!strncmp(haystack + i, needle, nlen))
			return haystack + i;
	}

	return NULL;
}



/* ******************************************************************************** */
/* EN50221 physical interface functions */


/**
 * dvb_ca_en50221_check_camstatus - Check CAM status.
 */
static int dvb_ca_en50221_check_camstatus(struct dvb_ca_private *ca, int slot)
{
	int slot_status;
	int cam_present_now;
	int cam_changed;

	/* IRQ mode */
	if (ca->flags & DVB_CA_EN50221_FLAG_IRQ_CAMCHANGE) {
		return (atomic_read(&ca->slot_info[slot].camchange_count) != 0);
	}

	/* poll mode */
	slot_status = ca->pub->poll_slot_status(ca->pub, slot, ca->open);

	cam_present_now = (slot_status & DVB_CA_EN50221_POLL_CAM_PRESENT) ? 1 : 0;
	cam_changed = (slot_status & DVB_CA_EN50221_POLL_CAM_CHANGED) ? 1 : 0;
	if (!cam_changed) {
		int cam_present_old = (ca->slot_info[slot].slot_state != DVB_CA_SLOTSTATE_NONE);
		cam_changed = (cam_present_now != cam_present_old);
	}

	if (cam_changed) {
		if (!cam_present_now) {
			ca->slot_info[slot].camchange_type = DVB_CA_EN50221_CAMCHANGE_REMOVED;
		} else {
			ca->slot_info[slot].camchange_type = DVB_CA_EN50221_CAMCHANGE_INSERTED;
		}
		printk("%s cam changed, %s\n", __func__,cam_present_now ? "Cam INSERTED" : "Cam REMOVED");
		atomic_set(&ca->slot_info[slot].camchange_count, 1);
	} else {
		if ((ca->slot_info[slot].slot_state == DVB_CA_SLOTSTATE_WAITREADY) &&
		    (slot_status & DVB_CA_EN50221_POLL_CAM_READY)) {
			// move to validate state if reset is completed
			ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_VALIDATE;
		}
	}

	return cam_changed;
}


/**
 * dvb_ca_en50221_wait_if_status - Wait for flags to become set on the STATUS
 *	 register on a CAM interface, checking for errors and timeout.
 *
 * @ca: CA instance.
 * @slot: Slot on interface.
 * @waitfor: Flags to wait for.
 * @timeout_ms: Timeout in milliseconds.
 *
 * @return 0 on success, nonzero on error.
 */
static int dvb_ca_en50221_wait_if_status(struct dvb_ca_private *ca, int slot,
					 u8 waitfor, int timeout_hz)
{
	unsigned long timeout;
	unsigned long start;

	dprintk("%s\n", __func__);

	/* loop until timeout elapsed */
	start = jiffies;
	timeout = jiffies + timeout_hz;
	while (1) {
		/* read the status and check for error */
		int res = ca->pub->read_cam_control(ca->pub, slot, CTRLIF_STATUS);
		if (res < 0)
			return -EIO;

		/* if we got the flags, it was successful! */
		if (res & waitfor) {
			dprintk("%s succeeded timeout:%lu\n", __func__, jiffies - start);
			return 0;
		}

		/* check for timeout */
		if (time_after(jiffies, timeout)) {
			break;
		}

		/* wait for a bit */
		msleep(1);
	}

	dprintk("%s failed timeout:%lu\n", __func__, jiffies - start);

	/* if we get here, we've timed out */
	return -ETIMEDOUT;
}


/**
 * dvb_ca_en50221_link_init - Initialise the link layer connection to a CAM.
 *
 * @ca: CA instance.
 * @slot: Slot id.
 *
 * @return 0 on success, nonzero on failure.
 */
static int dvb_ca_en50221_link_init(struct dvb_ca_private *ca, int slot)
{
	int ret;
	int buf_size;
	u8 buf[2];

	dprintk("%s\n", __func__);
	
	if (ca->slot_info[slot].link_read_buf) {
		vfree(ca->slot_info[slot].link_read_buf);
		ca->slot_info[slot].link_read_buf = NULL;
	}
	if (ca->slot_info[slot].link_write_buf) {
		vfree(ca->slot_info[slot].link_write_buf);
		ca->slot_info[slot].link_write_buf = NULL;
	}

	/* we'll be determining these during this function */
	ca->slot_info[slot].da_irq_supported = 0;

	/* set the host link buffer size temporarily. it will be overwritten with the
	 * real negotiated size later. */
	ca->slot_info[slot].link_buf_size = 2;

	/* read the buffer size from the CAM */
	if ((ret = ca->pub->write_cam_control(ca->pub, slot, CTRLIF_COMMAND, IRQEN | CMDREG_SR)) != 0)
		return ret;
	if ((ret = dvb_ca_en50221_wait_if_status(ca, slot, STATUSREG_DA, HZ * 10)) != 0)
		return ret;
	if ((ret = dvb_ca_en50221_read_data(ca, slot, buf, 2)) != 2)
		return -EIO;
	if ((ret = ca->pub->write_cam_control(ca->pub, slot, CTRLIF_COMMAND, IRQEN)) != 0)
		return ret;

	/* store it, and choose the minimum of our buffer and the CAM's buffer size */
	buf_size = (buf[0] << 8) | buf[1];
	ca->slot_info[slot].link_buf_size = buf_size;
	
	ca->slot_info[slot].link_read_buf = vmalloc(buf_size);
	if (ca->slot_info[slot].link_read_buf == NULL) {
		log_error("fail to alloc read buf");
		return -ENOMEM;
	}
	ca->slot_info[slot].link_write_buf = vmalloc(buf_size);
	if (ca->slot_info[slot].link_write_buf == NULL) {
		log_error("fail to alloc write buf");
		vfree(ca->slot_info[slot].link_read_buf);
		ca->slot_info[slot].link_read_buf = NULL;
		return -ENOMEM;
	}

	buf[0] = buf_size >> 8;
	buf[1] = buf_size & 0xff;
	dprintk("Chosen link buffer size of %i\n", buf_size);

	/* write the buffer size to the CAM */
	if ((ret = ca->pub->write_cam_control(ca->pub, slot, CTRLIF_COMMAND, IRQEN | CMDREG_SW)) != 0)
		return ret;
	if ((ret = dvb_ca_en50221_wait_if_status(ca, slot, STATUSREG_FR, HZ / 10)) != 0)
		return ret;
	/* need extra delay for specific CAM */
	msleep(10);
	if ((ret = dvb_ca_en50221_write_negobuffer(ca, slot, buf, 2)) != 2)
		return -EIO;
	if ((ret = ca->pub->write_cam_control(ca->pub, slot, CTRLIF_COMMAND, IRQEN)) != 0)
		return ret;

	/* success */
	return 0;
}

/**
 * dvb_ca_en50221_read_tuple - Read a tuple from attribute memory.
 *
 * @ca: CA instance.
 * @slot: Slot id.
 * @address: Address to read from. Updated.
 * @tupleType: Tuple id byte. Updated.
 * @tupleLength: Tuple length. Updated.
 * @tuple: Dest buffer for tuple (must be 256 bytes). Updated.
 *
 * @return 0 on success, nonzero on error.
 */
static int dvb_ca_en50221_read_tuple(struct dvb_ca_private *ca, int slot,
				     int *address, int *tupleType, int *tupleLength, u8 * tuple)
{
	int i;
	int _tupleType;
	int _tupleLength;
	int _address = *address;

	/* grab the next tuple length and type */
	if ((_tupleType = ca->pub->read_attribute_mem(ca->pub, slot, _address)) < 0)
		return _tupleType;
	if (_tupleType == 0xff) {
		dprintk("END OF CHAIN TUPLE type:0x%x\n", _tupleType);
		*address += 2;
		*tupleType = _tupleType;
		*tupleLength = 0;
		return 0;
	}
	if ((_tupleLength = ca->pub->read_attribute_mem(ca->pub, slot, _address + 2)) < 0)
		return _tupleLength;
	_address += 4;

	dprintk("TUPLE type:0x%x length:%i\n", _tupleType, _tupleLength);

	/* read in the whole tuple */
	for (i = 0; i < _tupleLength; i++) {
		tuple[i] = ca->pub->read_attribute_mem(ca->pub, slot, _address + (i * 2));
		dprintk("  0x%02x: 0x%02x %c\n",
			i, tuple[i] & 0xff,
			((tuple[i] > 31) && (tuple[i] < 127)) ? tuple[i] : '.');
	}
	_address += (_tupleLength * 2);

	// success
	*tupleType = _tupleType;
	*tupleLength = _tupleLength;
	*address = _address;
	return 0;
}


/**
 * dvb_ca_en50221_parse_attributes - Parse attribute memory of a CAM module,
 *	extracting Config register, and checking it is a DVB CAM module.
 *
 * @ca: CA instance.
 * @slot: Slot id.
 *
 * @return 0 on success, <0 on failure.
 */
static int dvb_ca_en50221_parse_attributes(struct dvb_ca_private *ca, int slot)
{
	int address = 0;
	int tupleLength;
	int tupleType;
	u8 tuple[257];
	char *dvb_str;
	int rasz;
	int status;
	int got_cftableentry = 0;
	int end_chain = 0;
	int i;
	u16 manfid = 0;
	u16 devid = 0;


	// CISTPL_DEVICE_0A
	if ((status =
	     dvb_ca_en50221_read_tuple(ca, slot, &address, &tupleType, &tupleLength, tuple)) < 0)
		return status;
	if (tupleType != 0x1D)
		return -EINVAL;



	// CISTPL_DEVICE_0C
	if ((status =
	     dvb_ca_en50221_read_tuple(ca, slot, &address, &tupleType, &tupleLength, tuple)) < 0)
		return status;
	if (tupleType != 0x1C)
		return -EINVAL;



	// CISTPL_VERS_1
	if ((status =
	     dvb_ca_en50221_read_tuple(ca, slot, &address, &tupleType, &tupleLength, tuple)) < 0)
		return status;
	if (tupleType != 0x15)
		return -EINVAL;

	/*parse version */
	ca->pub->parse_cam_version(ca->pub, slot, tuple, tupleLength);

	// CISTPL_MANFID
	if ((status = dvb_ca_en50221_read_tuple(ca, slot, &address, &tupleType,
						&tupleLength, tuple)) < 0)
		return status;
	if (tupleType != 0x20)
		return -EINVAL;
	
	/*parse manfid */
	ca->pub->parse_cam_manfid(ca->pub, slot, tuple, tupleLength);
	
	if (tupleLength != 4)
		return -EINVAL;
	manfid = (tuple[1] << 8) | tuple[0];
	devid = (tuple[3] << 8) | tuple[2];



	// CISTPL_CONFIG
	if ((status = dvb_ca_en50221_read_tuple(ca, slot, &address, &tupleType,
						&tupleLength, tuple)) < 0)
		return status;
	if (tupleType != 0x1A)
		return -EINVAL;
	if (tupleLength < 3)
		return -EINVAL;

	/* extract the configbase */
	rasz = tuple[0] & 3;
	if (tupleLength < (3 + rasz + 14))
		return -EINVAL;
	ca->slot_info[slot].config_base = 0;
	for (i = 0; i < rasz + 1; i++) {
		ca->slot_info[slot].config_base |= (tuple[2 + i] << (8 * i));
	}

	/* check it contains the correct DVB string */
	dvb_str = findstr((char *)tuple, tupleLength, "DVB_CI_V", 8);
	if (dvb_str == NULL)
		return -EINVAL;
	if (tupleLength < ((dvb_str - (char *) tuple) + 12))
		return -EINVAL;

	/* is it a version we support? */
	if (strncmp(dvb_str + 8, "1.00", 4)) {
		printk("dvb_ca adapter %d: Unsupported DVB CAM module version %c%c%c%c\n",
		       ca->dvbdev->adapter->num, dvb_str[8], dvb_str[9], dvb_str[10], dvb_str[11]);
		return -EINVAL;
	}

	/* process the CFTABLE_ENTRY tuples, and any after those */
	while ((!end_chain) && (address < 0x1000)) {
		if ((status = dvb_ca_en50221_read_tuple(ca, slot, &address, &tupleType,
							&tupleLength, tuple)) < 0)
			return status;
		switch (tupleType) {
		case 0x1B:	// CISTPL_CFTABLE_ENTRY
			if (tupleLength < (2 + 11 + 17))
				break;

			/* if we've already parsed one, just use it */
			if (got_cftableentry)
				break;

			/* get the config option */
			/* RQ150707-00647, about Ziggo latency issue, change value 0x3f -> 0x7f.
			The COR register is defined in PC Card Standard Volume 2, section 4.13.1, LevIREQ should be set. */			
			ca->slot_info[slot].config_option = tuple[0] & 0x7f;


			/* OK, check it contains the correct strings */
			if ((findstr((char *)tuple, tupleLength, "DVB_HOST", 8) == NULL) ||
			    (findstr((char *)tuple, tupleLength, "DVB_CI_MODULE", 13) == NULL))
				break;
			
			/*parse cam ireq */
			ca->pub->parse_cam_ireq(ca->pub, slot, tuple, tupleLength);

			got_cftableentry = 1;
			break;

		case 0x14:	// CISTPL_NO_LINK
			break;

		case 0xFF:	// CISTPL_END
			end_chain = 1;
			break;

		default:	/* Unknown tuple type - just skip this tuple and move to the next one */
			dprintk("dvb_ca: Skipping unknown tuple type:0x%x length:0x%x\n", tupleType,
				tupleLength);
			break;
		}
	}

	if ((address > 0x1000) || (!got_cftableentry))
		return -EINVAL;

	dprintk("Valid DVB CAM detected MANID:%x DEVID:%x CONFIGBASE:0x%x CONFIGOPTION:0x%x\n",
		manfid, devid, ca->slot_info[slot].config_base, ca->slot_info[slot].config_option);

	// success!
	return 0;
}


/**
 * dvb_ca_en50221_set_configoption - Set CAM's configoption correctly.
 *
 * @ca: CA instance.
 * @slot: Slot containing the CAM.
 */
static int dvb_ca_en50221_set_configoption(struct dvb_ca_private *ca, int slot)
{
	int configoption;

	dprintk("%s\n", __func__);

	/* set the config option */
	ca->pub->write_attribute_mem(ca->pub, slot,
				     ca->slot_info[slot].config_base,
				     ca->slot_info[slot].config_option);

	/* check it */
	configoption = ca->pub->read_attribute_mem(ca->pub, slot, ca->slot_info[slot].config_base);
	dprintk("Set configoption 0x%x, read configoption 0x%x\n",
		ca->slot_info[slot].config_option, configoption & 0x7f);

	/* fine! */
	return 0;

}


/**
 * dvb_ca_en50221_read_data - This function talks to an EN50221 CAM control
 *	interface. It reads a buffer of data from the CAM. The data can either
 *	be stored in a supplied buffer, or automatically be added to the slot's
 *	rx_buffer.
 *
 * @ca: CA instance.
 * @slot: Slot to read from.
 * @ebuf: If non-NULL, the data will be written to this buffer. If NULL,
 * the data will be added into the buffering system as a normal fragment.
 * @ecount: Size of ebuf. Ignored if ebuf is NULL.
 *
 * @return Number of bytes read, or < 0 on error
 */
static int dvb_ca_en50221_read_data(struct dvb_ca_private *ca, int slot, u8 * ebuf, int ecount)
{
	int bytes_read;
	int status;
	u8 *buf = NULL;/*u8 buf[HOST_LINK_BUF_SIZE];*/
	int i;
	bool wakeup = false;
	struct dvb_ca_slot *slot_info;

	slot_info = &ca->slot_info[slot];
	dprintk("%s\n", __func__);
	
	/* check if we have space for a link buf in the rx_buffer */
	if (ebuf == NULL) {
		int buf_free;

		if (ca->slot_info[slot].rx_buffer.data == NULL) {
			status = -EIO;
			goto exit;
		}
		buf_free = dvb_ringbuffer_free(&ca->slot_info[slot].rx_buffer);

		if (buf_free < (ca->slot_info[slot].link_buf_size + DVB_RINGBUFFER_PKTHDRSIZE)) {
			status = -EAGAIN;
			goto exit;
		}
	}

	/* check if there is data available */
	if ((status = ca->pub->read_cam_control(ca->pub, slot, CTRLIF_STATUS)) < 0)
		goto exit;
	if (!(status & STATUSREG_DA)) {
		/* no data */
		status = 0;
		goto exit;
	}

	/* read the amount of data */
	if ((status = ca->pub->read_cam_control(ca->pub, slot, CTRLIF_SIZE_HIGH)) < 0)
		goto exit;
	bytes_read = status << 8;
	if ((status = ca->pub->read_cam_control(ca->pub, slot, CTRLIF_SIZE_LOW)) < 0)
		goto exit;
	bytes_read |= status;

	/* check it will fit */
	if (ebuf == NULL) {
		if (bytes_read > ca->slot_info[slot].link_buf_size) {
			printk("dvb_ca adapter %d: CAM tried to send a buffer larger than the link buffer size (%i > %i)!\n",
			       ca->dvbdev->adapter->num, bytes_read, ca->slot_info[slot].link_buf_size);
			ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_LINKINIT;
			status = -EIO;
			goto exit;
		}
		if (bytes_read < 2) {
			printk("dvb_ca adapter %d: CAM sent a buffer that was less than 2 bytes!\n",
			       ca->dvbdev->adapter->num);
			ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_LINKINIT;
			status = -EIO;
			goto exit;
		}
		/* alloc temporal link buffer to read */
		buf = slot_info->link_read_buf;
		if (!buf) {
			log_error("link read buffer is not allocated");
			slot_info->slot_state = DVB_CA_SLOTSTATE_LINKINIT;
			status = -EIO;
			goto exit;
		}
	} else {
		if (bytes_read > ecount) {
			printk("dvb_ca adapter %d: CAM tried to send a buffer larger than the ecount size!\n",
			       ca->dvbdev->adapter->num);
			status = -EIO;
			goto exit;
		}
		/* use actual buffer */
		buf = ebuf;
	}
#if 0
	/* fill the buffer */
	for (i = 0; i < bytes_read; i++) {
		/* read byte and check */
		if ((status = ca->pub->read_cam_control(ca->pub, slot, CTRLIF_DATA)) < 0)
			goto exit;

		/* OK, store it in the buffer */
		buf[i] = status;
	}
#endif

	if ((status = ca->pub->read_cam_multi_data(ca->pub, slot, bytes_read, buf)) < 0)
		goto exit;

	/* check for read error (RE should now be 0) */
	if ((status = ca->pub->read_cam_control(ca->pub, slot, CTRLIF_STATUS)) < 0)
		goto exit;
	if (status & STATUSREG_RE) {
		ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_LINKINIT;
		status = -EIO;
		goto exit;
	}

	/* OK, add it to the receive buffer, or copy into external buffer if supplied */
	if (ebuf == NULL) {
		if (ca->slot_info[slot].rx_buffer.data == NULL) {
			status = -EIO;
			goto exit;
		}

		status = dvb_ringbuffer_pkt_write(&ca->slot_info[slot].rx_buffer, buf, bytes_read);
                if (status < 0)
                    goto exit;
		wakeup = true;
	}
	
	if (dvb_ca_debug & CA_DEBUG_TRACE) {
		log_noti("Received CA packet for slot %i size:0x%x",
			 slot, bytes_read);
	}

	/* wake up readers when a last_fragment is received */
	if (wakeup) {
		wake_up_interruptible(&ca->wait_queue);
	}
	status = bytes_read;

exit:
	return status;
}


/**
 * dvb_ca_en50221_write_data - This function talks to an EN50221 CAM control
 *				interface. It writes a buffer of data to a CAM.
 *
 * @ca: CA instance.
 * @slot: Slot to write to.
 * @ebuf: The data in this buffer is treated as a complete link-level packet to
 * be written.
 * @count: Size of ebuf.
 *
 * @return Number of bytes written, or < 0 on error.
 */
static int dvb_ca_en50221_write_data(struct dvb_ca_private *ca, int slot, u8 * buf, int bytes_write)
{
	int status;
	int i;

	/* sanity check */
	if (bytes_write > ca->slot_info[slot].link_buf_size)
		return -EINVAL;

	/* it is possible we are dealing with a single buffer implementation,
	   thus if there is data available for read or if there is even a read
	   already in progress, we do nothing but awake the kernel thread to
	   process the data if necessary. */
	if ((status = ca->pub->read_cam_control(ca->pub, slot, CTRLIF_STATUS)) < 0)
		goto exitnowrite;
	if (status & (STATUSREG_DA | STATUSREG_RE)) {
		if (status & STATUSREG_DA)
			dvb_ca_en50221_thread_wakeup(ca);

		status = -EAGAIN;
		goto exitnowrite;
	}

	/* OK, set HC bit */
	if ((status = ca->pub->write_cam_control(ca->pub, slot, CTRLIF_COMMAND,
						 IRQEN | CMDREG_HC)) != 0)
		goto exit;

	/* check if interface is still free */
	if ((status = ca->pub->read_cam_control(ca->pub, slot, CTRLIF_STATUS)) < 0)
		goto exit;
	if (!(status & STATUSREG_FR)) {
		/* it wasn't free => try again later */
		status = -EAGAIN;
		goto exit;
	}

	/* send the amount of data */
	if ((status = ca->pub->write_cam_control(ca->pub, slot, CTRLIF_SIZE_HIGH, bytes_write >> 8)) != 0)
		goto exit;
	if ((status = ca->pub->write_cam_control(ca->pub, slot, CTRLIF_SIZE_LOW,
						 bytes_write & 0xff)) != 0)
		goto exit;
#if 0
	/* send the buffer */
	for (i = 0; i < bytes_write; i++) {
		if ((status = ca->pub->write_cam_control(ca->pub, slot, CTRLIF_DATA, buf[i])) != 0)
			goto exit;
	}
#endif

	if ((status = ca->pub->write_cam_multi_data(ca->pub, slot, bytes_write, buf)) < 0)
		goto exit;

	/* check for write error (WE should now be 0) */
	if ((status = ca->pub->read_cam_control(ca->pub, slot, CTRLIF_STATUS)) < 0)
		goto exit;
	if (status & STATUSREG_WE) {
		ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_LINKINIT;
		status = -EIO;
		goto exit;
	}
	status = bytes_write;
	if (dvb_ca_debug & CA_DEBUG_WRITE) {
		log_debug("Wrote CA packet for slot %i size:0x%x", slot, bytes_write);
	}
exit:
	ca->pub->write_cam_control(ca->pub, slot, CTRLIF_COMMAND, IRQEN);

exitnowrite:
	return status;
}
EXPORT_SYMBOL(dvb_ca_en50221_camchange_irq);

static int dvb_ca_en50221_write_negobuffer(struct dvb_ca_private *ca, int slot, u8 * buf, int bytes_write)
{
	int status;
	int i;

	/* sanity check */
	if (bytes_write > ca->slot_info[slot].link_buf_size)
		return -EINVAL;

	/* it is possible we are dealing with a single buffer implementation,
	   thus if there is data available for read or if there is even a read
	   already in progress, we do nothing but awake the kernel thread to
	   process the data if necessary. */
	if ((status = ca->pub->read_cam_control(ca->pub, slot, CTRLIF_STATUS)) < 0)
		goto exitnowrite;
	if (status & (STATUSREG_DA | STATUSREG_RE)) {
		if (status & STATUSREG_DA)
			dvb_ca_en50221_thread_wakeup(ca);

		status = -EAGAIN;
		goto exitnowrite;
	}

	/* OK, set HC bit */
	if ((status = ca->pub->write_cam_control(ca->pub, slot, CTRLIF_COMMAND,
						 IRQEN | CMDREG_HC | CMDREG_SW)) != 0)
		goto exit;

	/* check if interface is still free */
	if ((status = ca->pub->read_cam_control(ca->pub, slot, CTRLIF_STATUS)) < 0)
		goto exit;
	if (!(status & STATUSREG_FR)) {
		/* it wasn't free => try again later */
		status = -EAGAIN;
		goto exit;
	}

	/* send the amount of data */
	if ((status = ca->pub->write_cam_control(ca->pub, slot, CTRLIF_SIZE_HIGH, bytes_write >> 8)) != 0)
		goto exit;
	if ((status = ca->pub->write_cam_control(ca->pub, slot, CTRLIF_SIZE_LOW,
						 bytes_write & 0xff)) != 0)
		goto exit;
#if 0
	/* send the buffer */
	for (i = 0; i < bytes_write; i++) {
		if ((status = ca->pub->write_cam_control(ca->pub, slot, CTRLIF_DATA, buf[i])) != 0)
			goto exit;
	}
#endif

	if ((status = ca->pub->write_cam_multi_data(ca->pub, slot, bytes_write, buf)) < 0)
		goto exit;

	/* check for write error (WE should now be 0) */
	if ((status = ca->pub->read_cam_control(ca->pub, slot, CTRLIF_STATUS)) < 0)
		goto exit;
	if (status & STATUSREG_WE) {
		ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_LINKINIT;
		status = -EIO;
		goto exit;
	}
	status = bytes_write;
	if (dvb_ca_debug & CA_DEBUG_WRITE) {
		log_debug("Wrote CA packet for slot %i size:0x%x", slot, bytes_write);
	}
exit:
	ca->pub->write_cam_control(ca->pub, slot, CTRLIF_COMMAND, IRQEN);

exitnowrite:
	return status;
}


/* ******************************************************************************** */
/* EN50221 higher level functions */


/**
 * dvb_ca_en50221_camready_irq - A CAM has been removed => shut it down.
 *
 * @ca: CA instance.
 * @slot: Slot to shut down.
 */
static int dvb_ca_en50221_slot_shutdown(struct dvb_ca_private *ca, int slot)
{
	dprintk("%s\n", __func__);

	ca->pub->slot_shutdown(ca->pub, slot);
	ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_NONE;

	/* need to wake up all processes to check if they're now
	   trying to write to a defunct CAM */
	wake_up_interruptible(&ca->wait_queue);

	dprintk("Slot %i shutdown\n", slot);

	/* success */
	return 0;
}
EXPORT_SYMBOL(dvb_ca_en50221_camready_irq);

void dvb_ca_en50221_slot_shutdown_ext(struct dvb_ca_en50221 *pubca, int slot)
{
    struct dvb_ca_private *ca = pubca->private;

    dvb_ca_en50221_slot_shutdown(ca, slot);
}
EXPORT_SYMBOL(dvb_ca_en50221_slot_shutdown_ext);
/**
 * dvb_ca_en50221_camready_irq - A CAMCHANGE IRQ has occurred.
 *
 * @ca: CA instance.
 * @slot: Slot concerned.
 * @change_type: One of the DVB_CA_CAMCHANGE_* values.
 */
void dvb_ca_en50221_camchange_irq(struct dvb_ca_en50221 *pubca, int slot, int change_type)
{
	struct dvb_ca_private *ca = pubca->private;

	dprintk("CAMCHANGE IRQ slot:%i change_type:%i\n", slot, change_type);

	switch (change_type) {
	case DVB_CA_EN50221_CAMCHANGE_REMOVED:
	case DVB_CA_EN50221_CAMCHANGE_INSERTED:
		break;

	default:
		return;
	}

	ca->slot_info[slot].camchange_type = change_type;
	atomic_inc(&ca->slot_info[slot].camchange_count);
	dvb_ca_en50221_thread_wakeup(ca);
}
EXPORT_SYMBOL(dvb_ca_en50221_frda_irq);


/**
 * dvb_ca_en50221_camready_irq - A CAMREADY IRQ has occurred.
 *
 * @ca: CA instance.
 * @slot: Slot concerned.
 */
void dvb_ca_en50221_camready_irq(struct dvb_ca_en50221 *pubca, int slot)
{
	struct dvb_ca_private *ca = pubca->private;

	dprintk("CAMREADY IRQ slot:%i\n", slot);

	if (ca->slot_info[slot].slot_state == DVB_CA_SLOTSTATE_WAITREADY) {
		ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_VALIDATE;
		dvb_ca_en50221_thread_wakeup(ca);
	}
}


/**
 * An FR or DA IRQ has occurred.
 *
 * @ca: CA instance.
 * @slot: Slot concerned.
 */
void dvb_ca_en50221_frda_irq(struct dvb_ca_en50221 *pubca, int slot)
{
	struct dvb_ca_private *ca = pubca->private;
	int flags;

	dprintk("FR/DA IRQ slot:%i\n", slot);

	switch (ca->slot_info[slot].slot_state) {
	case DVB_CA_SLOTSTATE_LINKINIT:
		flags = ca->pub->read_cam_control(pubca, slot, CTRLIF_STATUS);
		if (flags & STATUSREG_DA) {
			dprintk("CAM supports DA IRQ\n");
			ca->slot_info[slot].da_irq_supported = 1;
		}
		break;

	case DVB_CA_SLOTSTATE_RUNNING:
		if (ca->open)
			dvb_ca_en50221_thread_wakeup(ca);
		break;
	}
}



/* ******************************************************************************** */
/* EN50221 thread functions */

/**
 * Wake up the DVB CA thread
 *
 * @ca: CA instance.
 */
static void dvb_ca_en50221_thread_wakeup(struct dvb_ca_private *ca)
{

	dprintk("%s\n", __func__);

	ca->wakeup = 1;
	mb();
	wake_up_process(ca->thread);
}

/**
 * Update the delay used by the thread.
 *
 * @ca: CA instance.
 */
static void dvb_ca_en50221_thread_update_delay(struct dvb_ca_private *ca)
{
	int delay;
	int curdelay = 100000000;
	int slot;

	/* Beware of too high polling frequency, because one polling
	 * call might take several hundred milliseconds until timeout!
	 */
	for (slot = 0; slot < ca->slot_count; slot++) {
		switch (ca->slot_info[slot].slot_state) {
		default:
		case DVB_CA_SLOTSTATE_NONE:
			delay = HZ * 60;  /* 60s */
			if (!(ca->flags & DVB_CA_EN50221_FLAG_IRQ_CAMCHANGE))
				//delay = HZ * 5;  /* 5s */
				delay = HZ / 10;  /* 100ms */
			break;
		case DVB_CA_SLOTSTATE_INVALID:
			delay = HZ * 60;  /* 60s */
			if (!(ca->flags & DVB_CA_EN50221_FLAG_IRQ_CAMCHANGE))
				delay = HZ / 10;  /* 100ms */
			break;

		case DVB_CA_SLOTSTATE_UNINITIALISED:
		case DVB_CA_SLOTSTATE_WAITREADY:
		case DVB_CA_SLOTSTATE_VALIDATE:
		case DVB_CA_SLOTSTATE_WAITFR:
		case DVB_CA_SLOTSTATE_LINKINIT:
			delay = HZ / 50;  /* 20ms */
			break;

		case DVB_CA_SLOTSTATE_RUNNING:
			delay = HZ * 60;  /* 60s */
			if (!(ca->flags & DVB_CA_EN50221_FLAG_IRQ_CAMCHANGE))
				delay = HZ / 50;  /* 20ms */
			if (ca->open) {
				if ((!ca->slot_info[slot].da_irq_supported) ||
				    (!(ca->flags & DVB_CA_EN50221_FLAG_IRQ_DA)))
					delay = HZ / 50;  /* 20ms */
			}
			break;
		}

		if (delay < curdelay)
			curdelay = delay;
	}

	ca->delay = curdelay;
}



/**
 * Kernel thread which monitors CA slots for CAM changes, and performs data transfers.
 */
static int dvb_ca_en50221_thread(void *data)
{
	struct dvb_ca_private *ca = data;
	int slot;
	int flags;
	int status;
	//int pktcount;
	void *rxbuf;

	dprintk("%s\n", __func__);
	set_freezable();
	/* choose the correct initial delay */
	dvb_ca_en50221_thread_update_delay(ca);

	/* main loop */
	while (!kthread_should_stop()) {
		/* sleep for a bit */
		if (!ca->wakeup) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(ca->delay);
			if (kthread_should_stop())
				return 0;
			if (freezing(current)) {
				try_to_freeze();
			}
		}
		ca->wakeup = 0;

		/* go through all the slots processing them */
		for (slot = 0; slot < ca->slot_count; slot++) {
			mutex_lock(&ca->slot_info[slot].slot_lock);

			// check the cam status + deal with CAMCHANGEs
			while (dvb_ca_en50221_check_camstatus(ca, slot)) {
				/* clear down an old CI slot if necessary */
				if (ca->slot_info[slot].slot_state != DVB_CA_SLOTSTATE_NONE)
					dvb_ca_en50221_slot_shutdown(ca, slot);

				/* if a CAM is NOW present, initialise it */
				if (ca->slot_info[slot].camchange_type == DVB_CA_EN50221_CAMCHANGE_INSERTED) {
					ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_UNINITIALISED;
				}

				/* we've handled one CAMCHANGE */
				dvb_ca_en50221_thread_update_delay(ca);
				atomic_dec(&ca->slot_info[slot].camchange_count);
			}

			// CAM state machine
			switch (ca->slot_info[slot].slot_state) {
			case DVB_CA_SLOTSTATE_NONE:
			case DVB_CA_SLOTSTATE_INVALID:
				// no action needed
				break;

			case DVB_CA_SLOTSTATE_UNINITIALISED:
				ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_WAITREADY;

				ca->slot_info[slot].reset_running = 1;
				if (ca->slot_info[slot].rx_buffer.data != NULL) {
					dvb_ringbuffer_reset(&ca->slot_info[slot].rx_buffer);
				}

				ca->pub->slot_reset(ca->pub, slot);
				ca->slot_info[slot].timeout = jiffies + (INIT_TIMEOUT_SECS * HZ);
				break;

			case DVB_CA_SLOTSTATE_WAITREADY:
				if (time_after(jiffies, ca->slot_info[slot].timeout)) {
					printk("dvb_ca adaptor %d: PC card did not respond :(\n",
					       ca->dvbdev->adapter->num);
					ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_INVALID;
					dvb_ca_en50221_thread_update_delay(ca);
					break;
				}
				// no other action needed; will automatically change state when ready
				break;

			case DVB_CA_SLOTSTATE_VALIDATE:
				if (dvb_ca_en50221_parse_attributes(ca, slot) != 0) {
					/* we need this extra check for annoying interfaces like the budget-av */
					if ((!(ca->flags & DVB_CA_EN50221_FLAG_IRQ_CAMCHANGE)) &&
					    (ca->pub->poll_slot_status)) {
						status = ca->pub->poll_slot_status(ca->pub, slot, 0);
						if (!(status & DVB_CA_EN50221_POLL_CAM_PRESENT)) {
							ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_NONE;
							dvb_ca_en50221_thread_update_delay(ca);
							break;
						}
					}

					printk("dvb_ca adapter %d: Invalid PC card inserted :(\n",
					       ca->dvbdev->adapter->num);
					ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_INVALID;
					dvb_ca_en50221_thread_update_delay(ca);
					break;
				}
				if (dvb_ca_en50221_set_configoption(ca, slot) != 0) {
					printk("dvb_ca adapter %d: Unable to initialise CAM :(\n",
					       ca->dvbdev->adapter->num);
					ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_INVALID;
					dvb_ca_en50221_thread_update_delay(ca);
					break;
				}
				if (ca->pub->write_cam_control(ca->pub, slot,
							       CTRLIF_COMMAND, CMDREG_RS) != 0) {
					printk("dvb_ca adapter %d: Unable to reset CAM IF\n",
					       ca->dvbdev->adapter->num);
					ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_INVALID;
					dvb_ca_en50221_thread_update_delay(ca);
					break;
				}
				dprintk("DVB CAM validated successfully\n");

				ca->slot_info[slot].timeout = jiffies + (INIT_TIMEOUT_SECS * HZ);
				ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_WAITFR;
				ca->wakeup = 1;
				break;

			case DVB_CA_SLOTSTATE_WAITFR:
				if (time_after(jiffies, ca->slot_info[slot].timeout)) {
					printk("dvb_ca adapter %d: DVB CAM did not respond :(\n",
					       ca->dvbdev->adapter->num);
					ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_INVALID;
					dvb_ca_en50221_thread_update_delay(ca);
					break;
				}

				flags = ca->pub->read_cam_control(ca->pub, slot, CTRLIF_STATUS);
				if (flags & STATUSREG_FR) {
					ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_LINKINIT;
					ca->wakeup = 1;
				}
				break;

			case DVB_CA_SLOTSTATE_LINKINIT:
				if (dvb_ca_en50221_link_init(ca, slot) != 0) {
					/* we need this extra check for annoying interfaces like the budget-av */
					if ((!(ca->flags & DVB_CA_EN50221_FLAG_IRQ_CAMCHANGE)) &&
					    (ca->pub->poll_slot_status)) {
						status = ca->pub->poll_slot_status(ca->pub, slot, 0);
						if (!(status & DVB_CA_EN50221_POLL_CAM_PRESENT)) {
							ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_NONE;
							dvb_ca_en50221_thread_update_delay(ca);
							break;
						}
					}

					printk("dvb_ca adapter %d: DVB CAM link initialisation failed :(\n", ca->dvbdev->adapter->num);
					ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_INVALID;
					dvb_ca_en50221_thread_update_delay(ca);
					break;
				}

				if (ca->slot_info[slot].rx_buffer.data == NULL) {
					rxbuf = vmalloc(RX_BUFFER_SIZE);
					if (rxbuf == NULL) {
						printk("dvb_ca adapter %d: Unable to allocate CAM rx buffer :(\n", ca->dvbdev->adapter->num);
						ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_INVALID;
						dvb_ca_en50221_thread_update_delay(ca);
						break;
					}
					dvb_ringbuffer_init(&ca->slot_info[slot].rx_buffer, rxbuf, RX_BUFFER_SIZE);
				}

				ca->pub->slot_ts_enable(ca->pub, slot);
				ca->slot_info[slot].slot_state = DVB_CA_SLOTSTATE_RUNNING;
				dvb_ca_en50221_thread_update_delay(ca);
				printk("dvb_ca adapter %d: DVB CAM detected and initialised successfully\n", ca->dvbdev->adapter->num);
				break;

			case DVB_CA_SLOTSTATE_RUNNING:
				if (!ca->open)
					break;

				// poll slots for data
#if  0
				pktcount = 0;
				while ((status = dvb_ca_en50221_read_data(ca, slot, NULL, 0)) > 0) {
					if (!ca->open)
						break;

					/* if a CAMCHANGE occurred at some point, do not do any more processing of this slot */
					if (dvb_ca_en50221_check_camstatus(ca, slot)) {
						// we dont want to sleep on the next iteration so we can handle the cam change
						ca->wakeup = 1;
						break;
					}

					/* check if we've hit our limit this time */
					if (++pktcount >= MAX_RX_PACKETS_PER_ITERATION) {
						// dont sleep; there is likely to be more data to read
						ca->wakeup = 1;
						break;
					}
				}
#endif
				break;
			}
			if(ca->slot_info[slot].slot_state == DVB_CA_SLOTSTATE_RUNNING ||
				ca->slot_info[slot].slot_state == DVB_CA_SLOTSTATE_INVALID){
				ca->slot_info[slot].reset_running = 0;
			}
			mutex_unlock(&ca->slot_info[slot].slot_lock);
		}
		if (freezing(current)) {
			try_to_freeze();
		}
	}

	return 0;
}

static int get_ci_negobuf_size(struct dvb_ca_private *ca, int slot, int *val)
{
	int rc = -EAGAIN;
	int state;

	mutex_lock(&ca->slot_info[slot].slot_lock);

	state = ca->slot_info[slot].slot_state;
	if (state == DVB_CA_SLOTSTATE_RUNNING) {
		*val = ca->slot_info[slot].link_buf_size;
		rc = 0;
	} else {
		//log_error("CAM is not running state: %s",
		//	  get_slot_state_str(state));
		*val = 0;
	}

	mutex_unlock(&ca->slot_info[slot].slot_lock);

	return rc;
}
static int dvb_ca_en50221_io_ext_set_ioctl(struct dvb_ca_private *ca, int slot,struct ca_ext_control *ext){
	int err = 0;
	int u8Cmd = 0;
	//dprintk("%s SET ca_ext_control.id=%d\n", __func__,ext->id);
	switch(ext->id){
		case CA_EXT_CID_INPUTSOURCE:{
			struct ca_ext_source ca_source;
			if(ext->size){
			    if (copy_from_user(&ca_source, (void __user *) ext->ptr, ext->size)){
		            return -EINVAL;
			    }
			}
			if(ca->pub->set_input_source(ca->pub, slot, &ca_source)){
			    printk("SET CA_EXT_CID_INPUTSOURCE failed. %d, %d\n", ca_source.input_port_num,ca_source.input_src_type);
			    return -EINVAL;
			}
			dprintk("%s SET CA_EXT_CID_INPUTSOURCE input_port_num=%d, input_src_type=%d\n", __func__,ca_source.input_port_num,ca_source.input_src_type);
			break;
		}
		case CA_EXT_CID_ERROR_MODE:{
			enum ca_ext_error_mode err_mode = ext->value64;
			
			dprintk("%s SET CA_EXT_CID_ERROR_MODE err_mode=%d\n", __func__,(int)err_mode);
			
			break;
		}
		case CA_EXT_CID_PCMCIA_SPEED:{
			enum ca_ext_pcmcia_speed pcmcia_speed = ext->value64;
			dprintk("%s SET CA_EXT_CID_PCMCIA_SPEED pcmcia_speed=%d\n", __func__,(int)pcmcia_speed);

			break;
		}
		case CA_EXT_CID_SET_RS_BIT:{

			if (ca->pub->get_ca_ireq(ca->pub, slot))
			{
			    u8Cmd = (0x80 | 0x40);
			}
			//Set RS (Rest)
			err = ca->pub->write_cam_control(ca->pub, slot, CTRLIF_COMMAND, (u8Cmd | 0x08));
			
			if (err==0)
			{
			    dprintk("Reset Command interface.\n");
			}
			else
			{
			    dprintk("Write RS fail: 0x%x.\n", u8Cmd | 0x08);
			}
					
			break;
		}

		default:
			err = -EINVAL;
			break;
	}
	return err;
}

static int dvb_ca_en50221_io_ext_get_ioctl(struct dvb_ca_private *ca, int slot,struct ca_ext_control *ext){

	int err = 0;
	//dprintk("%s GET ca_ext_control.id=%d\n", __func__,ext->id);
	switch(ext->id){
		case CA_EXT_CID_INPUTSOURCE:{
			struct ca_ext_source ca_source;
			if(ca->pub->get_input_source(ca->pub, slot, &ca_source)){
			    printk("GET CA_EXT_CID_INPUTSOURCE failed. %d, %d\n", ca_source.input_port_num,ca_source.input_src_type);
			    err = -EINVAL;
			}
			//copy_to_user
			ext->size = sizeof(ca_source);
			if (copy_to_user(ext->ptr,&ca_source, sizeof(ca_source))){
			    return -EINVAL;
			}

			dprintk("%s GET CA_EXT_CID_INPUTSOURCE input_port_num=%d, input_src_type=%d\n", __func__,ca_source.input_port_num,ca_source.input_src_type);
			break;
		}
		case CA_EXT_CID_ERROR_MODE:{
			ext->value64 = CA_EXT_CI_ERROR_CHECK_MODE_MAX;
			dprintk("%s GET CA_EXT_CID_ERROR_MODE ciplus_error_check=%d\n", __func__,(int)ext->value64);
			
			break;
		}
		case CA_EXT_CID_PLUS_CAPA:{
			ext->value64 = 0;
			if (ca->slot_info[0].slot_state == DVB_CA_SLOTSTATE_RUNNING) {
			    ext->value64 = ca->pub->get_ca_plus_enable(ca->pub, slot);
			}
			dprintk("%s GET CA_EXT_CID_PLUS_CAPA ciplus_capa=%d\n", __func__,(int)ext->value64);
			break;
		}
		case CA_EXT_CID_DATA_RATE:{
			if (ca->slot_info[0].slot_state == DVB_CA_SLOTSTATE_RUNNING) {
			    ext->value64 = CA_EXT_CIPLUS_DATARATE_96;
			}else{
			    ext->value64 = CA_EXT_CIPLUS_DATARATE_72;
			}
			dprintk("%s GET CA_EXT_CID_DATA_RATE ciplus_data_rate=%d\n", __func__,(int)ext->value64);
			break;
		}

		case CA_EXT_CID_PLUS_VERSION:{
			ca->pub->get_ca_plus_version(ca->pub, slot, &ext->value64);
			dprintk("%s GET CA_EXT_CID_PLUS_VERSION ciplus_version=%x\n", __func__,(int)ext->value64);
			
			break;
		}
		case CA_EXT_CID_PLUS_IIR_STATUS:{
			#define IIR                 (0x10)
			int status = 0;
			if (ca->slot_info[slot].slot_state == DVB_CA_SLOTSTATE_RUNNING && ca->pub->get_ca_plus_enable(ca->pub, slot)) {
			   status = ca->pub->read_cam_control(ca->pub, slot, 1);
			}
			if(status & IIR)
			    ext->value64 = 1;
			else
			    ext->value64 = 0;

			//dprintk("%s GET CA_EXT_CID_PLUS_IIR_STATUS ciplus_iir_status=%d\n", __func__,(int)ext->value64);
			break;
		}
		case CA_EXT_CID_GET_NEGO_BUFF:{
			int negobuf_size = 0;
			err = get_ci_negobuf_size(ca, 0, &negobuf_size);
			ext->value64 = negobuf_size;
			dprintk("%s GET CA_EXT_CID_GET_NEGO_BUFF negobuf_size=%d\n", __func__,(int)ext->value64);
			break;
		}
		default:
			err = -EINVAL;
			break;
	}	
	return err;
}


/* ******************************************************************************** */
/* EN50221 IO interface functions */

/**
 * Real ioctl implementation.
 * NOTE: CA_SEND_MSG/CA_GET_MSG ioctls have userspace buffers passed to them.
 *
 * @inode: Inode concerned.
 * @file: File concerned.
 * @cmd: IOCTL command.
 * @arg: Associated argument.
 *
 * @return 0 on success, <0 on error.
 */
static int dvb_ca_en50221_io_do_ioctl(struct file *file,
				      unsigned int cmd, void *parg)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dvb_ca_private *ca = dvbdev->priv;
	int err = 0;
	int slot;

	//dprintk("%s\n", __func__);

	if (mutex_lock_interruptible(&ca->ioctl_mutex))
		return -ERESTARTSYS;

	switch (cmd) {
	case CA_RESET:
		for (slot = 0; slot < ca->slot_count; slot++) {
			if((ca->pub->str_status_get() == 2) &&
				(ca->slot_info[slot].camchange_type == DVB_CA_EN50221_CAMCHANGE_INSERTED)){
				ca->pub->str_status_set(1);
				printk("[%s]---pcmcia is rusume statues, ca_reset skip!!\n",__func__);
				err = -EBUSY;
			}
			else if(ca->slot_info[slot].reset_running == 1){
				printk("[%s]---en50221 thread reset is running, ca_reset skip!!\n",__func__);
				err = -EBUSY;
			}
			else{
				printk("[%s]----ca_reset enter!!\n",__func__);
				mutex_lock(&ca->slot_info[slot].slot_lock);
				if (ca->slot_info[slot].slot_state != DVB_CA_SLOTSTATE_NONE) {
					dvb_ca_en50221_slot_shutdown(ca, slot);
					if (ca->flags & DVB_CA_EN50221_FLAG_IRQ_CAMCHANGE)
						dvb_ca_en50221_camchange_irq(ca->pub,
										 slot,
										 DVB_CA_EN50221_CAMCHANGE_INSERTED);
				}
				mutex_unlock(&ca->slot_info[slot].slot_lock);
				ca->next_read_slot = 0;
				dvb_ca_en50221_thread_wakeup(ca);
			}
		}
		break;

	case CA_GET_CAP: {
		struct ca_caps *caps = parg;

		caps->slot_num = ca->slot_count;
		caps->slot_type = CA_CI_PHYS;
		caps->descr_num = 0;
		caps->descr_type = 0;
		break;
	}

	case CA_GET_SLOT_INFO: {
		struct ca_slot_info *info = parg;

		if ((info->num > ca->slot_count) || (info->num < 0)) {
			err = -EINVAL;
			goto out_unlock;
		}

		info->type = CA_CI_PHYS;
		info->flags = 0;
		if ((ca->slot_info[info->num].slot_state != DVB_CA_SLOTSTATE_NONE)
			&& (ca->slot_info[info->num].slot_state != DVB_CA_SLOTSTATE_INVALID)) {
			info->flags = CA_CI_MODULE_PRESENT;
		}
		if (ca->slot_info[info->num].slot_state == DVB_CA_SLOTSTATE_RUNNING) {
			info->flags |= CA_CI_MODULE_READY;
		}else if (ca->slot_info[info->num].slot_state == DVB_CA_SLOTSTATE_INVALID){
			info->flags |= CA_EXT_CI_MODULE_INVALID;
		}
		break;
	}
	case CA_EXT_S_CTL: {
		struct ca_ext_control *ca_set_ext = parg;
		if (dvb_ca_en50221_io_ext_set_ioctl(ca,0,ca_set_ext)< 0) {
			err = -EINVAL;
			goto out_unlock;
		}
		break;
	}
	case CA_EXT_G_CTL: {
		struct ca_ext_control *ca_get_ext = parg;
		if (dvb_ca_en50221_io_ext_get_ioctl(ca,0,ca_get_ext)< 0) {
			err = -EINVAL;
			goto out_unlock;
		}
		break;
	}
	default:
		err = -EINVAL;
		break;
	}

out_unlock:
	mutex_unlock(&ca->ioctl_mutex);
	return err;
}

/**
 * Wrapper for ioctl implementation.
 *
 * @inode: Inode concerned.
 * @file: File concerned.
 * @cmd: IOCTL command.
 * @arg: Associated argument.
 *
 * @return 0 on success, <0 on error.
 */
static long dvb_ca_en50221_io_ioctl(struct file *file,
				    unsigned int cmd, unsigned long arg)
{
	return dvb_usercopy(file, cmd, arg, dvb_ca_en50221_io_do_ioctl);
}

// TEST CODE
static void debug_io_rw_str(const char __user *buf, size_t count, bool is_read)
{
	u8 *b;
	u8 byte;
	int i, size = count * 6 + 1;

	b = kmalloc(size, GFP_KERNEL);
	CHECK_ERROR(!b, return, "malloc error");

	for (i = 0; i < count; i++) {
		if (copy_from_user(&byte, buf + i, 1)) {
			log_error("copy_from_user failed");
			kfree(b);
			return;
		}
		snprintf(b + (i * 6), 7, "[0x%02x]", byte);
	}

	b[size - 1] = '\0';
	dprintk("\n debug_io_rw_str %s str:%s\n", (is_read == 1) ? "read" : "write", b);

	kfree(b);
}
/**
 * Implementation of write() syscall.
 *
 * @file: File structure.
 * @buf: Source buffer.
 * @count: Size of source buffer.
 * @ppos: Position in file (ignored).
 *
 * @return Number of bytes read, or <0 on error.
 */
static ssize_t dvb_ca_en50221_io_write(struct file *file,
				       const char __user * buf, size_t count, loff_t * ppos)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dvb_ca_private *ca = dvbdev->priv;
	u8 slot;
	int status;
	u8 *writebuf;
	unsigned long timeout;
	bool written;
	s64 a, b;
	
	if (dvb_ca_debug & CA_DEBUG_RW)
		debug_io_rw_str(buf, count, 0);
	if (dvb_ca_debug & CA_DEBUG_TRACE)
		log_noti("%s count=%d \n", __func__, count);
	if (dvb_ca_debug & CA_DEBUG_WRITE)
		a = ktime_to_us(ktime_get());
	
	slot = 0; // FIXED
	if (count < 2)
		return -EINVAL;
	if (count > ca->slot_info[slot].link_buf_size) {
		log_error("exceeds link buf size(%u) < %zu",
			  ca->slot_info[slot].link_buf_size, count);
		return -EINVAL;
	}
	
	/* alloc temporal link buffer to write */
	writebuf = ca->slot_info[slot].link_write_buf;
	CHECK_ERROR(!writebuf, return -ENOMEM, "write buffer not allocated");
	/* store in the buffer */
	status = copy_from_user(writebuf, buf, count);
	if (status) {
		status = -EFAULT;
		goto exit;
	}
	timeout = jiffies + HZ / 2;
	written = false;
	while (!time_after(jiffies, timeout)) {
		mutex_lock(&ca->slot_info[slot].slot_lock);
		/* check the CAM hasn't been removed/reset in the meantime */
		if (ca->slot_info[slot].slot_state != DVB_CA_SLOTSTATE_RUNNING) {
			status = -EIO;
			mutex_unlock(&ca->slot_info[slot].slot_lock);
			goto exit;
		}

		status = dvb_ca_en50221_write_data(ca, slot, writebuf, count);
		mutex_unlock(&ca->slot_info[slot].slot_lock);
		if (status == count) {
			written = true;
			break;
		}

		if (status != -EAGAIN)
			goto exit;

		msleep(1);
	}

	if (!written) {
		status = -EIO;
		goto exit;
	}
	status = count;
 exit:
	if (dvb_ca_debug & CA_DEBUG_WRITE) {
		b = ktime_to_us(ktime_get());
		log_noti("Write elapsed %lld us / %d\n", (b - a), status);
	}
	return status;
}

					   
/**
* Condition for waking up in dvb_ca_en50221_io_read_condition
*/
static int dvb_ca_en50221_io_read_condition(struct dvb_ca_private *ca,
					   int *result, int *_slot)
{
	int slot =0;
	int status = 0;

	/* check if there is data available */
	if ((status = ca->pub->read_cam_control(ca->pub, slot, CTRLIF_STATUS)) < 0)
		return 0;
	
	if (!(status & STATUSREG_DA)) {
		/* no data */
		return 0;
	}else{
		return 1;
	}
}

/**
* Implementation of read() syscall.
*
* @file: File structure.
* @buf: Destination buffer.
* @count: Size of destination buffer.
* @ppos: Position in file (ignored).
*
* @return Number of bytes read, or <0 on error.
*/
static ssize_t dvb_ca_en50221_io_read(struct file *file, char __user * buf,
					size_t count, loff_t * ppos)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dvb_ca_private *ca = dvbdev->priv;
	int status;
	int slot;
	int pktlen;
	u8 *readbuf;
  
	//dprintk("dvb_ca_en50221_io_read enter count=%d link_buf_size=%d\n", count,ca->slot_info[slot].link_buf_size);
  
	/* Outgoing packet has a 2 byte header. hdr[0] = slot_id, hdr[1] = connection_id */
	if (count < 2)
		return -EINVAL;

	slot = 0 ;//fixed
	readbuf = ca->slot_info[slot].link_read_buf;

	pktlen = dvb_ca_en50221_read_data(ca, slot, readbuf, ca->slot_info[slot].link_buf_size);

	if(pktlen < 0) {
		printk("%s pktlen [%d] < 0\n", __func__, pktlen);
		return -EINVAL;
	}

	status = copy_to_user(buf, readbuf, pktlen);
	if (status) {
		status = -EFAULT;
		return status;
	}

	status = pktlen;

	if(status >0 ){
		if (dvb_ca_debug & CA_DEBUG_TRACE)
		   log_noti("%s done with len = %d count=%d \n", __func__,status,(int)count);
		if (dvb_ca_debug & CA_DEBUG_RW)
		   debug_io_rw_str(buf, status, 1);
	}
	return status;
}



/**
 * Implementation of file open syscall.
 *
 * @inode: Inode concerned.
 * @file: File concerned.
 *
 * @return 0 on success, <0 on failure.
 */
static int dvb_ca_en50221_io_open(struct inode *inode, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dvb_ca_private *ca = dvbdev->priv;
	int err;
	int i;

	dprintk("%s\n", __func__);

	if(!ca->thread)
		return -ENODEV;
		
	if (!try_module_get(ca->pub->owner)){
		printk("%s try_module_get(ca->pub->owner) fail \n", __func__);
		return -EIO;
	}
	err = dvb_generic_open(inode, file);
	if (err < 0) {
		printk("%s dvb_generic_open fail \n", __func__);
		module_put(ca->pub->owner);
		return err;
	}

	ca->open++;
			
	for (i = 0; i < ca->slot_count; i++) {

		if (ca->slot_info[i].slot_state == DVB_CA_SLOTSTATE_RUNNING) {
			if (ca->slot_info[i].rx_buffer.data != NULL) {
				/* it is safe to call this here without locks because
				* ca->open == 0. Data is not read in this case */
				dvb_ringbuffer_flush(&ca->slot_info[i].rx_buffer);
			}
		}
	}
	ca->pub->slot_enable(ca, 0, 1);
	dvb_ca_en50221_thread_update_delay(ca);
	dvb_ca_en50221_thread_wakeup(ca);

	return 0;
}


/**
 * Implementation of file close syscall.
 *
 * @inode: Inode concerned.
 * @file: File concerned.
 *
 * @return 0 on success, <0 on failure.
 */
static int dvb_ca_en50221_io_release(struct inode *inode, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dvb_ca_private *ca = dvbdev->priv;
	int err;

	dprintk("%s\n", __func__);

	/* mark the CA device as closed */
	if(ca->open)
	{
		ca->open--;
	}
	dvb_ca_en50221_thread_update_delay(ca);

	err = dvb_generic_release(inode, file);

	module_put(ca->pub->owner);

	return err;
}


/**
 * Implementation of poll() syscall.
 *
 * @file: File concerned.
 * @wait: poll wait table.
 *
 * @return Standard poll mask.
 */
static unsigned int dvb_ca_en50221_io_poll(struct file *file, poll_table * wait)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dvb_ca_private *ca = dvbdev->priv;
	unsigned int mask = 0;
	int slot;
	int result = 0;

	//dprintk("%s\n", __func__);

	if (dvb_ca_en50221_io_read_condition(ca, &result, &slot) == 1) {
		mask |= POLLIN;
	}

	/* if there is something, return now */
	if (mask)
		return mask;

	/* wait for something to happen */
	poll_wait(file, &ca->wait_queue, wait);

	if (dvb_ca_en50221_io_read_condition(ca, &result, &slot) == 1) {
		mask |= POLLIN;
	}

	return mask;
}
EXPORT_SYMBOL(dvb_ca_en50221_init);


static const struct file_operations dvb_ca_fops = {
	.owner = THIS_MODULE,
	.read = dvb_ca_en50221_io_read,
	.write = dvb_ca_en50221_io_write,
	.unlocked_ioctl = dvb_ca_en50221_io_ioctl,
	.open = dvb_ca_en50221_io_open,
	.release = dvb_ca_en50221_io_release,
	.poll = dvb_ca_en50221_io_poll,
	.llseek = noop_llseek,
};

static const struct dvb_device dvbdev_ca = {
	.priv = NULL,
	.users = 1,
	.readers = 1,
	.writers = 1,
#if defined(CONFIG_MEDIA_CONTROLLER_DVB)
	.name = "dvb-ca-en50221",
#endif
	.fops = &dvb_ca_fops,
};

/* ******************************************************************************** */
/* Initialisation/shutdown functions */


/**
 * Initialise a new DVB CA EN50221 interface device.
 *
 * @dvb_adapter: DVB adapter to attach the new CA device to.
 * @ca: The dvb_ca instance.
 * @flags: Flags describing the CA device (DVB_CA_FLAG_*).
 * @slot_count: Number of slots supported.
 *
 * @return 0 on success, nonzero on failure
 */
int dvb_ca_en50221_init(struct dvb_adapter *dvb_adapter,
			struct dvb_ca_en50221 *pubca, int flags, int slot_count)
{
	int ret;
	struct dvb_ca_private *ca = NULL;
	int i;

	dprintk("%s\n", __func__);

	if (slot_count < 1)
		return -EINVAL;

	/* initialise the system data */
	if ((ca = kzalloc(sizeof(struct dvb_ca_private), GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto exit;
	}
	ca->pub = pubca;
	ca->flags = flags;
	ca->slot_count = slot_count;
	if ((ca->slot_info = kcalloc(slot_count, sizeof(struct dvb_ca_slot), GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		goto free_ca;
	}
	init_waitqueue_head(&ca->wait_queue);
	ca->open = 0;
	ca->wakeup = 0;
	ca->next_read_slot = 0;
	pubca->private = ca;

	/* register the DVB device */
	ret = dvb_register_device(dvb_adapter, &ca->dvbdev, &dvbdev_ca, ca, DVB_DEVICE_CA);
	if (ret)
		goto free_slot_info;

	/* now initialise each slot */
	for (i = 0; i < slot_count; i++) {
		memset(&ca->slot_info[i], 0, sizeof(struct dvb_ca_slot));
		ca->slot_info[i].slot_state = DVB_CA_SLOTSTATE_NONE;
		ca->slot_info[i].link_read_buf = NULL;
		ca->slot_info[i].link_write_buf = NULL;
		atomic_set(&ca->slot_info[i].camchange_count, 0);
		ca->slot_info[i].camchange_type = DVB_CA_EN50221_CAMCHANGE_REMOVED;
		mutex_init(&ca->slot_info[i].slot_lock);
	}

	mutex_init(&ca->ioctl_mutex);

	if (signal_pending(current)) {
		ret = -EINTR;
		goto unregister_device;
	}
	mb();

	/* create a kthread for monitoring this CA device */
	ca->thread = kthread_run(dvb_ca_en50221_thread, ca, "kdvb-ca-%i:%i",
				 ca->dvbdev->adapter->num, ca->dvbdev->id);
	if (IS_ERR(ca->thread)) {
		ret = PTR_ERR(ca->thread);
		printk("dvb_ca_init: failed to start kernel_thread (%d)\n",
			ret);
		goto unregister_device;
	}
	ca->pub->install_camchange_irq(ca, dvb_ca_en50221_camchange_irq);
	ca->pub->install_camready_irq(ca, dvb_ca_en50221_camready_irq);
	ca->pub->install_frda_irq(ca, dvb_ca_en50221_frda_irq);

	return 0;

unregister_device:
	dvb_unregister_device(ca->dvbdev);
free_slot_info:
	kfree(ca->slot_info);
free_ca:
	kfree(ca);
exit:
	pubca->private = NULL;
	return ret;
}
EXPORT_SYMBOL(dvb_ca_en50221_release);



/**
 * Release a DVB CA EN50221 interface device.
 *
 * @ca_dev: The dvb_device_t instance for the CA device.
 * @ca: The associated dvb_ca instance.
 */
void dvb_ca_en50221_release(struct dvb_ca_en50221 *pubca)
{
	struct dvb_ca_private *ca = pubca->private;
	int i;

	dprintk("%s\n", __func__);

	ca->pub->install_camchange_irq(ca, NULL);
	ca->pub->install_camready_irq(ca, NULL);
	ca->pub->install_frda_irq(ca, NULL);

	/* shutdown the thread if there was one */
	kthread_stop(ca->thread);

	for (i = 0; i < ca->slot_count; i++) {
		dvb_ca_en50221_slot_shutdown(ca, i);
		if (ca->slot_info[i].link_read_buf)
			vfree(ca->slot_info[i].link_read_buf);
		if (ca->slot_info[i].link_write_buf)
			vfree(ca->slot_info[i].link_write_buf);
		vfree(ca->slot_info[i].rx_buffer.data);
	}
	kfree(ca->slot_info);
	dvb_unregister_device(ca->dvbdev);
	kfree(ca);
	pubca->private = NULL;
}
