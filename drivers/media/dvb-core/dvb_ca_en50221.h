/*
 * dvb_ca.h: generic DVB functions for EN50221 CA interfaces
 *
 * Copyright (C) 2004 Andrew de Quincey
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#ifndef _DVB_CA_EN50221_H_
#define _DVB_CA_EN50221_H_

#include <linux/list.h>
#include <linux/dvb/ca.h>
#include <ca-ext.h>
#include "dvbdev.h"
#include "dvb_ringbuffer.h"

#define DVB_CA_EN50221_POLL_CAM_PRESENT      	1
#define DVB_CA_EN50221_POLL_CAM_CHANGED       	2
#define DVB_CA_EN50221_POLL_CAM_READY       	4

#define DVB_CA_EN50221_FLAG_IRQ_CAMCHANGE     	1
#define DVB_CA_EN50221_FLAG_IRQ_FR            	2
#define DVB_CA_EN50221_FLAG_IRQ_DA          	4

#define DVB_CA_EN50221_CAMCHANGE_REMOVED      	0
#define DVB_CA_EN50221_CAMCHANGE_INSERTED   	1

#define DVB_CA_SLOTSTATE_NONE                	0
#define DVB_CA_SLOTSTATE_UNINITIALISED      	1
#define DVB_CA_SLOTSTATE_RUNNING             	2
#define DVB_CA_SLOTSTATE_INVALID            	3
#define DVB_CA_SLOTSTATE_WAITREADY           	4
#define DVB_CA_SLOTSTATE_VALIDATE           	5
#define DVB_CA_SLOTSTATE_WAITFR               	6
#define DVB_CA_SLOTSTATE_LINKINIT            	7

enum {
	CA_DEBUG_TRACE   	= (0x1) << 0,
	CA_DEBUG_RW       	= (0x1) << 1,
	CA_DEBUG_STATE    	= (0x1) << 2,
	CA_DEBUG_WRITE   	= (0x1) << 3,
};


#define log_error(fmt, args...)     printk(KERN_ERR "[DVB-CA], " fmt, ## args)
#define log_noti(fmt, args...)      printk(KERN_NOTICE "[DVB-CA], " fmt, ## args)
#define log_tuple(fmt, args...)     printk(KERN_INFO "[DVB-CA], " fmt, ## args)
#define log_debug(fmt, args...)     printk(KERN_DEBUG "[DVB-CA], " fmt, ## args)


/* Information on a CA slot */
struct dvb_ca_slot {

	/* current state of the CAM */
	int slot_state;

	/* mutex used for serializing access to one CI slot */
	struct mutex slot_lock;

	/* Number of CAMCHANGES that have occurred since last processing */
	atomic_t camchange_count;

	/* Type of last CAMCHANGE */
	int camchange_type;

	/* base address of CAM config */
	u32 config_base;

	/* value to write into Config Control register */
	u8 config_option;

	/* if 1, the CAM supports DA IRQs */
	u8 da_irq_supported:1;

	/* size of the buffer to use when talking to the CAM */
	int link_buf_size;
	
	/*  temporal buffer for hw read/write sequence */
	u8 *link_read_buf;
	u8 *link_write_buf;
	
	/* buffer for incoming packets */
	struct dvb_ringbuffer rx_buffer;

	/* timer used during various states of the slot */
	unsigned long timeout;

	bool reset_running;
};

/* Private CA-interface information */
struct dvb_ca_private {

	/* pointer back to the public data structure */
	struct dvb_ca_en50221 *pub;

	/* the DVB device */
	struct dvb_device *dvbdev;

	/* Flags describing the interface (DVB_CA_FLAG_*) */
	u32 flags;

	/* number of slots supported by this CA interface */
	unsigned int slot_count;

	/* information on each slot */
	struct dvb_ca_slot *slot_info;

	/* wait queues for read() and write() operations */
	wait_queue_head_t wait_queue;

	/* PID of the monitoring thread */
	struct task_struct *thread;

	/* Flag indicating if the CA device is open */
	unsigned int open;

	/* Flag indicating the thread should wake up now */
	unsigned int wakeup:1;

	/* Delay the main thread should use */
	unsigned long delay;

	/* Slot to start looking for data to read from in the next user-space read operation */
	int next_read_slot;

	/* mutex serializing ioctls */
	struct mutex ioctl_mutex;
};

/**
 * struct dvb_ca_en50221- Structure describing a CA interface
 *
 * @owner:		the module owning this structure
 * @read_attribute_mem:	function for reading attribute memory on the CAM
 * @write_attribute_mem: function for writing attribute memory on the CAM
 * @read_cam_control:	function for reading the control interface on the CAM
 * @write_cam_control:	function for reading the control interface on the CAM
 * @slot_reset:		function to reset the CAM slot
 * @slot_shutdown:	function to shutdown a CAM slot
 * @slot_ts_enable:	function to enable the Transport Stream on a CAM slot
 * @poll_slot_status:	function to poll slot status. Only necessary if
 *			DVB_CA_FLAG_EN50221_IRQ_CAMCHANGE is not set.
 * @data:		private data, used by caller.
 * @private:		Opaque data used by the dvb_ca core. Do not modify!
 *
 * NOTE: the read_*, write_* and poll_slot_status functions will be
 * called for different slots concurrently and need to use locks where
 * and if appropriate. There will be no concurrent access to one slot.
 */
struct dvb_ca_en50221 {
	struct module *owner;

	int (*read_attribute_mem)(struct dvb_ca_en50221 *ca,
				  int slot, int address);
	int (*write_attribute_mem)(struct dvb_ca_en50221 *ca,
				   int slot, int address, u8 value);

	int (*read_cam_control)(struct dvb_ca_en50221 *ca,
				int slot, u8 address);
	int (*write_cam_control)(struct dvb_ca_en50221 *ca,
				 int slot, u8 address, u8 value);

	int (*slot_reset)(struct dvb_ca_en50221 *ca, int slot);
	int (*slot_shutdown)(struct dvb_ca_en50221 *ca, int slot);
	int (*slot_ts_enable)(struct dvb_ca_en50221 *ca, int slot);

	int (*poll_slot_status)(struct dvb_ca_en50221 *ca, int slot, int open);

	int (*set_input_source)(struct dvb_ca_en50221 *ca, int slot, struct ca_ext_source *ca_src);
	int (*get_input_source)(struct dvb_ca_en50221 *ca, int slot, struct ca_ext_source *ca_src);

	int (*parse_cam_version)(struct dvb_ca_en50221 *ca, int slot, u8 *para, u8 size);
	int (*get_ca_plus_version)(struct dvb_ca_en50221 *ca, int slot, signed long long *ver);
	int (*get_ca_plus_enable)(struct dvb_ca_en50221 *ca, int slot);
	int (*parse_cam_manfid)(struct dvb_ca_en50221 *ca, int slot, u8 *para, u8 size);
	int (*parse_cam_ireq)(struct dvb_ca_en50221 *ca, int slot, u8 *para, u8 size);
	int (*get_ca_ireq)(struct dvb_ca_en50221 *ca, int slot);
	void (*install_camchange_irq)(struct dvb_ca_en50221 *ca, void (*func)(struct dvb_ca_en50221 *pubca, int slot, int change_type));
	void (*install_camready_irq)(struct dvb_ca_en50221 *ca, void (*func)(struct dvb_ca_en50221 *pubca, int slot));
	void (*install_frda_irq)(struct dvb_ca_en50221 *ca, void (*func)(struct dvb_ca_en50221 *pubca, int slot));
	int (*slot_enable)(struct dvb_ca_en50221 *ca, int slot, int enable);
	int (*str_status_get)(void);
	void (*str_status_set)(int str);

	int (*read_cam_multi_data)(struct dvb_ca_en50221 *ca, int slot, unsigned short len, unsigned char* p_data);
	int (*write_cam_multi_data)(struct dvb_ca_en50221 *ca, int slot, unsigned short len, unsigned char* p_data);

	void *data;

	void *private;
};

/*
 * Functions for reporting IRQ events
 */

/**
 * dvb_ca_en50221_camchange_irq - A CAMCHANGE IRQ has occurred.
 *
 * @pubca: CA instance.
 * @slot: Slot concerned.
 * @change_type: One of the DVB_CA_CAMCHANGE_* values
 */
void dvb_ca_en50221_camchange_irq(struct dvb_ca_en50221 *pubca, int slot,
				  int change_type);

/**
 * dvb_ca_en50221_camready_irq - A CAMREADY IRQ has occurred.
 *
 * @pubca: CA instance.
 * @slot: Slot concerned.
 */
void dvb_ca_en50221_camready_irq(struct dvb_ca_en50221 *pubca, int slot);

/**
 * dvb_ca_en50221_frda_irq - An FR or a DA IRQ has occurred.
 *
 * @ca: CA instance.
 * @slot: Slot concerned.
 */
void dvb_ca_en50221_frda_irq(struct dvb_ca_en50221 *ca, int slot);

/*
 * Initialisation/shutdown functions
 */

/**
 * dvb_ca_en50221_init - Initialise a new DVB CA device.
 *
 * @dvb_adapter: DVB adapter to attach the new CA device to.
 * @ca: The dvb_ca instance.
 * @flags: Flags describing the CA device (DVB_CA_EN50221_FLAG_*).
 * @slot_count: Number of slots supported.
 *
 * @return 0 on success, nonzero on failure
 */
extern int dvb_ca_en50221_init(struct dvb_adapter *dvb_adapter,
			       struct dvb_ca_en50221 *ca, int flags,
			       int slot_count);

/**
 * dvb_ca_en50221_release - Release a DVB CA device.
 *
 * @ca: The associated dvb_ca instance.
 */
extern void dvb_ca_en50221_release(struct dvb_ca_en50221 *ca);

extern void dvb_ca_en50221_slot_shutdown_ext(struct dvb_ca_en50221 *pubca, int slot);
#endif
