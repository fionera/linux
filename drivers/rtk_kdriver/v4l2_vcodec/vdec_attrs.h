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

#ifndef __V4L2_VDEC_ATTRS_H__
#define __V4L2_VDEC_ATTRS_H__

#define HAVE_VDEC_PROC_SYMLINK

#define VDEC_PROC_DIR "lgtv-driver"
#define VDEC_PROC_SYMLINK VDEC_PROC_DIR"/v4l2_vdec"
#define VDEC_PROC_DIR_PATH "/proc/"VDEC_PROC_DIR
#define VDEC_PROC_SYMLINK_PATH "/proc/"VDEC_PROC_SYMLINK

int vdec_addChannelAttr(vdec_handle_t *handle, vcodec_attr_t *vattr, int nr);
int vdec_delAttr(vdec_handle_t *handle, vcodec_attr_t *vattr);
int vdec_initAttrs(struct device *dev);
int vdec_exitAttrs(struct device *dev);

#endif
