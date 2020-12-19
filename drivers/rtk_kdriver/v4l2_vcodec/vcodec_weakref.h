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

#ifndef __VCODEC_WEAKREF_H__
#define __VCODEC_WEAKREF_H__

#include "v4l2_vdec_ext.h"
#include "rdvb/rdvb-dmx/rdvb_dmx_ctrl.h"

#define WEAKREF(__ret, __name, ...) \
	static __ret __weakref_##__name (__VA_ARGS__) __attribute__((weakref(#__name), long_call)); \

WEAKREF(int, rdvb_dmx_ctrl_privateInfo,
		enum dmx_priv_cmd dmx_priv_cmd, enum pin_type pin_type,
		int pin_port, struct dmx_priv_data* priv_data);

WEAKREF(int, rdvb_dmx_ctrl_releaseBuffer,
		enum pin_type pin_type, int pin_port,
		enum host_type host_type, int host_port);

WEAKREF(int, rdvb_dmx_ctrl_getBufferInfo,
		enum pin_type pin_type, int pin_port,
		enum host_type host_type, int host_port,
		struct dmx_buff_info_t *dmx_buff_info);

WEAKREF(int, vsc_v4l2_vdo_control,
		unsigned char display, struct v4l2_ext_vsc_vdo_mode mode);

#endif
