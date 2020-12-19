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

#include <linux/module.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>

#pragma scalar_storage_order big-endian
#include <rpc_common.h>
#include <VideoRPC_Agent.h>
#pragma scalar_storage_order default

#include "vcodec_conf.h"
#include "vcodec_def.h"
#include "vcodec_macros.h"
#include "vcodec_struct.h"
#include "vcodec_utils.h"
#include "vdec_attrs.h"
#include "vdec_core.h"

#include "vutils/vdma_carveout.h"

#define SHOW(fmt, ...) { \
	len += snprintf(buf + len, PAGE_SIZE - len, fmt, ##__VA_ARGS__); \
}

#ifdef HAVE_VDEC_PROC_SYMLINK

static struct proc_dir_entry *vdec_proc_symlink = NULL;

static int access_file(const char *path)
{
	struct file *fp;

	fp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(fp))
		return -1;

	filp_close(fp, NULL);

	return 0;
}

static void create_proc_symlink(struct device *dev)
{
	if (access_file(VDEC_PROC_DIR_PATH)) {
		if (!proc_mkdir(VDEC_PROC_DIR, NULL)) {
			dev_err(dev, "fail to create proc dir\n");
			return;
		}
	}

	if (!vdec_proc_symlink) {
		char target_path[64], *kobj_path;
		kobj_path = kobject_get_path(&dev->kobj, GFP_KERNEL);
		if (!kobj_path) {
			dev_err(dev, "fail to get kobj_path\n");
			return;
		}
		snprintf(target_path, sizeof(target_path), "/sys%s", kobj_path);
		kfree(kobj_path);
		vdec_proc_symlink = proc_symlink(VDEC_PROC_SYMLINK, NULL, target_path);
		if (!vdec_proc_symlink) {
			dev_err(dev, "fail to create proc symlink\n");
			return;
		}
		dev_info(dev, "symlink %s -> %s\n", VDEC_PROC_SYMLINK_PATH, target_path);
	}

}

static void delete_proc_symlink(void)
{
	if (vdec_proc_symlink) {
		proc_remove(vdec_proc_symlink);
		vdec_proc_symlink = NULL;
	}
}

#endif

static ssize_t show_channel(struct device *dev, struct device_attribute *attr, char *buf)
{
	vcodec_device_t *device = dev_get_drvdata(dev);
	int ret, len = 0;
	vdec_handle_t *h;
	struct v4l2_pix_format_mplane *mp;
	vcodec_attr_t *vattr = (vcodec_attr_t*)attr;
	VIDEO_DEC_PIC_MSG *msg;
	struct v4l2_ext_decoder_status dec_stat;

	h = vcodec_getDecByChannel(device, vattr->nr);
	if (!h) {
		SHOW("channel %d not exist\n", vattr->nr);
		return len;
	}

	ret = vdec_getDecoderStatus(h, &dec_stat);
	if (ret) {
		SHOW("no DecoderStatus\n");
		return len;
	}

	mp = &h->pixfmt_out;
	msg = h->elem.msgQ? h->pPicMsg:NULL;

	SHOW("linuxtv-ext-headerver.%d.%d.%d (submissions/%d)\n",
		LINUXTV_EXT_VER_MAJOR,
		LINUXTV_EXT_VER_MINOR,
		LINUXTV_EXT_VER_PATCH,
		LINUXTV_EXT_VER_SUBMISSION);

	// 1. Port
	SHOW("\n1. Port\n");
	SHOW("Port = [%d]\n", TO_SIGNED(h->bChannel, h->channel));

	// 2. VTP port
	SHOW("\n2. VTP Port\n");
	SHOW("VTP Port = [%d]\n", TO_SIGNED(h->bVTPPort, h->vtp_port));

	// 3. VDO port
	SHOW("\n3. VDO port\n");
	SHOW("VDO Port = [%d]\n", TO_SIGNED(h->bDisplay, h->display));
	SHOW("VDO Connected = [%d]\n", h->bDisplay);

	// 4. Codec type
	SHOW("\n4. Codec type\n");
	SHOW("Codec type = [%.4s]\n", (char*)&mp->pixelformat);

	// 5. Decoder Status
	SHOW("\n5. Decoder Status\n");
	SHOW("vdecState = [%d]\n", dec_stat.vdec_state);
	SHOW("av lipsync = [%d]\n", dec_stat.av_libsync);
	SHOW("pts matched = [%d]\n", dec_stat.pts_matched);

	// 6. Subscribe event list
	SHOW("\n6. Subscribe event list\n");
	SHOW("V4L2_SUB_EXT_VDEC_FRAME = %d\n", h->event.bFrmInfo);
	SHOW("V4L2_SUB_EXT_VDEC_PICINFO = %d\n", h->event.bPicInfo);
	SHOW("V4L2_SUB_EXT_VDEC_USERDATA = %d\n", h->event.bUserData);

	// 7. Input status(ES Size)
	SHOW("\n7. Input status(ES size)\n");
	SHOW("Total ES Input = [0x%08x]\n", dec_stat.es_data_size);

	if (msg) {
		// 8. resolution(x,y,w,h, Crop x,y,w,h)
		SHOW("\n8. resolution(x,y,w,h, Crop x,y,w,h)\n");
		SHOW("active [x,y,w,h]\n");
		SHOW("[%4d, %4d, %4d, %4d]\n",
			msg->actXY >> 16, msg->actXY & 0xffff,
			msg->actWH >> 16, msg->actWH & 0xffff);
		SHOW("crop [x,y,w,h]\n");
		SHOW("[%4d, %4d, %4d, %4d]\n", 0, 0, 0, 0);

		// 9. afd
		SHOW("\n9. afd\n");
		SHOW("afd = %d\n", msg->afd_3d >> 16);

		// 10. aspect ratio(aspect ratio , idc, par, sar)
		SHOW("\n10. aspect ration(aspect ratio, idc, par, sar)\n");
		SHOW("aspect ratio = %d\n", msg->ratio_info >> 16);
		SHOW("aspect ratio idc = %d\n",  msg->ratio_info & 0xffff);
		SHOW("par(width, height) = %d, %d\n", 0, 0);
		SHOW("sar(width, height) = %d, %d\n", msg->sarWH >> 16, msg->sarWH & 0xffff);

		// 11. frame rate
		SHOW("\n11. frame rate\n");
		SHOW("frame rate = %d\n", msg->frame_rate);
	}

	// 12. display delay
	SHOW("\n12. display delay\n");
	SHOW("display delay = %d\n", h->display_delay);

	// 13. Decoded pic count
	SHOW("\n13. Decoded pic count\n");
	SHOW("Decoded pic count = %d\n", h->nFrmInfo);

	// 14. Displayed pic count
	SHOW("\n14. Displayed pic count\n");
	SHOW("Displayed Pic count = %d\n", h->nPicInfo);

	// 15. MB error info
	SHOW("\n15. MB error info\n");
	SHOW("Total MB error count = %d\n", dec_stat.mb_dec_error_count);

	// 16. Decoded mode(tv = 0/direct = 1/drip dec = 2/media = 3)
	SHOW("\n16. Decoded mode(tv = 0/direct = 1/drip dec = 2/media = 3)\n");
	SHOW("Decode mode = %d\n", (h->userType != V4L2_EXT_VDEC_USERTYPE_DTV)? 3:
		h->bDirectMode? 1:
		h->bDripDecode? 2:
		0);

	// 17. hfr mode */
	SHOW("\n17. hfr mode\n");
	SHOW("hfr = %d\n", h->HFR_type);

	// 18. HW profile spec
	SHOW("\n18. HW profile spec\n");
	SHOW("Profile = %s\n", VDEC_V4L2_NAME);

	// 19. Secure Video Path mode
	SHOW("\n19. Secure Video Path mode\n");
	SHOW("SVP = %d\n", h->bSVP);

	return len;
}

static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	vcodec_device_t *device = dev_get_drvdata(dev);
	int i, len = 0;
	struct list_head *handle_list;
	vdec_handle_t *h;
	struct v4l2_pix_format_mplane *mp;

	handle_list = &device->share->vdec->handle_list;

	list_for_each_entry (h, handle_list, list) {

		SHOW("handle: 0x%p id 0x%08x type %d\n", h, h->userID, h->userType);

		mp = &h->pixfmt_out;
		SHOW("\tout pixfmt 0x%x %dx%d\n", mp->pixelformat, mp->width, mp->height);

		mp = &h->pixfmt_cap;
		SHOW("\tcap pixfmt 0x%x %dx%d\n", mp->pixelformat, mp->width, mp->height);

		for (i=0; i<h->pixfmt_cap.num_planes; i++) {
			SHOW("\tcap bytesperline %d\n", mp->plane_fmt[i].bytesperline);
			SHOW("\tcap sizeimage %d\n", mp->plane_fmt[i].sizeimage);
		}

		if (h->bChannel) {
			SHOW("\tchannel %d\n", h->channel);
		}

		if (h->bSource) {
			SHOW("\tbs hdr 0x%x buf 0x%x len 0x%x\n", h->src.bs_hdr_addr, h->src.bs_buf_addr, h->src.bs_buf_size);
			SHOW("\tib hdr 0x%x buf 0x%x len 0x%x\n", h->src.ib_hdr_addr, h->src.ib_buf_addr, h->src.ib_buf_size);
			SHOW("\trefclk 0x%x\n", h->src.refclk_addr);
		}

		if (h->pVTPRefClk) {
			REFCLOCK* pVTPRefClk = (REFCLOCK*)h->pVTPRefClk;
			SHOW("\tsysPTS A:%lld V:%lld A-V:%lld\n",
				pVTPRefClk->audioSystemPTS, pVTPRefClk->videoSystemPTS,
				pVTPRefClk->audioSystemPTS - pVTPRefClk->videoSystemPTS);
		}

		if (h->bDisplay) {
			SHOW("\tdisplay %d\n", h->display);
		}

		if (h->elem.enable) {
			SHOW("\tflt_dec 0x%x\n", h->flt_dec);
			SHOW("\tflt_out 0x%x\n", h->flt_out);
			if (h->nFrmInfo) SHOW("\tnFrmInfo %d\n", h->nFrmInfo);
			if (h->nPicInfoIn) SHOW("\tnPicInfoIn %d\n", h->nPicInfoIn);
			if (h->nPicInfoOut) SHOW("\tnPicInfoOut %d\n", h->nPicInfoOut);
			if (h->nUserData) SHOW("\tnUserData %d\n", h->nUserData);
		}
	}

	return len;
}

static ssize_t carveout_stat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return vdma_carveout_show_status(buf);
}

#define MAX_CNT 16
static ssize_t carveout_conf_store(struct device *dev, struct device_attribute *dev_attr,
	const char *buf, size_t count)
{
	vcodec_device_t *device = dev_get_drvdata(dev);
	int enable, blk, addr_cnt, size_cnt, cnt, i;
	dma_addr_t addr[MAX_CNT]={0};
	int size[MAX_CNT]={0};
	char *cmd, *attrs, *attr, *name, *val, *v;

	if (!(cmd = vmalloc(count + 1)))
		return count;

	enable = blk = addr_cnt = size_cnt = 0;

	memcpy(cmd, buf, count);
	cmd[count] = '\0';

	attrs = cmd;
	while ((attr = strsep(&attrs, " "))) {
		name = strsep(&attr, "=");
		val = attr;
		if (name && val) {
			if (!strcmp(name, "enable")) {
				if (!kstrtoint(val, 0, &i))
					enable = i;
			}
			else if (!strcmp(name, "blk")) {
				if (!kstrtoint(val, 0, &i))
					blk = i;
			}
			else if (!strcmp(name, "addr")) {
				while (addr_cnt < MAX_CNT && (v = strsep(&val, ","))) {
					if (!kstrtoint(v, 0, &i))
						addr[addr_cnt++] = i;
				}
			}
			else if (!strcmp(name, "size")) {
				while (size_cnt < MAX_CNT && (v = strsep(&val, ","))) {
					if (!kstrtoint(v, 0, &i))
						size[size_cnt++] = i;
				}
			}
		}
	}

	vfree(cmd);

	cnt = min(addr_cnt, size_cnt);

	dev_info(dev, "enable=%d blk=%d\n", enable, blk);
	for (i=0; i<cnt; i++)
		dev_info(dev, "[%d] addr 0x%x size 0x%x\n", i, addr[i], size[i]);

	if (device->share->vdec) device->share->vdec->dma_carveout_type = 0;
	if (device->share->vcap) device->share->vcap->dma_carveout_type = 0;
	vdma_carveout_exit();

	if (enable && blk && cnt) {
		vdma_carveout_init(addr, size, cnt, blk);
		if (device->share->vdec) device->share->vdec->dma_carveout_type = enable;
		if (device->share->vcap) device->share->vcap->dma_carveout_type = enable;
	}

	return count;
}

static DEVICE_ATTR_RO(status);
static DEVICE_ATTR_RO(carveout_stat);
static DEVICE_ATTR_WO(carveout_conf);

static struct attribute *vdec_attrs[] = {
	&dev_attr_status.attr,
	&dev_attr_carveout_stat.attr,
	&dev_attr_carveout_conf.attr,
	NULL,
};

static const struct attribute_group vdec_attr_group = {
	.attrs = vdec_attrs,
};

int vdec_addChannelAttr(vdec_handle_t *handle, vcodec_attr_t *vattr, int nr)
{
	snprintf(vattr->name, sizeof(vattr->name), "status.p%d", nr);
	vattr->nr = nr;

	vattr->dev_attr.attr.name = vattr->name;
	vattr->dev_attr.attr.mode = S_IRUGO;
	vattr->dev_attr.show = show_channel;
	sysfs_add_file_to_group(&handle->dev->kobj, &vattr->dev_attr.attr, NULL);

	dev_info(handle->dev, "add attr %s\n", vattr->name);

	#ifdef HAVE_VDEC_PROC_SYMLINK
	if (!vdec_proc_symlink)
		create_proc_symlink(handle->dev);
	#endif

	return 0;
}

int vdec_delAttr(vdec_handle_t *handle, vcodec_attr_t *vattr)
{
	sysfs_remove_file_from_group(&handle->dev->kobj, &vattr->dev_attr.attr, NULL);

	dev_info(handle->dev, "del attr %s\n", vattr->name);

	return 0;
}

int vdec_initAttrs(struct device *dev)
{
	int ret;

	ret = sysfs_create_group(&dev->kobj, &vdec_attr_group);
	if (ret) {
		dev_err(dev, "fail to create vdec_attr_group\n");
		return ret;
	}

	return 0;
}

int vdec_exitAttrs(struct device *dev)
{

	#ifdef HAVE_VDEC_PROC_SYMLINK
	delete_proc_symlink();
	#endif

	sysfs_remove_group(&dev->kobj, &vdec_attr_group);

	return 0;
}
