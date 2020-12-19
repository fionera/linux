/*
 * Copyright (c) 2018 Jias Huang <jias_huang@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __VCODEC_CONF_H__
#define __VCODEC_CONF_H__

#define VDEC_NUM_MSGQ 2

#define VDEC_CONF_MAX_INSTANCES 16
#define VDEC_CONF_FRM_MSG_CNT 512
#define VDEC_CONF_MSGQ_ENTRY_CNT 16
#define VDEC_CONF_USERDATAQ_SIZE 20480
#define VDEC_CONF_EVENT_QSIZE 10
#define VDEC_CONF_MAX_CHANNEL 4
#define VDEC_CONF_MAX_VTPPORT 2
#define VDEC_CONF_DIRECTMODE_BS_SIZE 0x100000
#define VDEC_CONF_DIRECTMODE_IB_SIZE 0x40000

#define VCAP_CONF_BUFQ_IN_SIZE (64 * 64)
#define VCAP_CONF_BUFQ_OUT_SIZE (64 * 256)
#define VCAP_CONF_BUF_WIDTH_ALIGN 64
#define VCAP_CONF_BUF_HEIGHT_ALIGN 64
#define VCAP_CONF_BUF_PADDING_SIZE 96

#endif
