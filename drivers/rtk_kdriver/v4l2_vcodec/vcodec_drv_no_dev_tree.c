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
#include <linux/platform_device.h>
#include <linux/moduleparam.h>

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

#include "v4l2_vdec_ext.h"

#include "vcodec_def.h"
#include "vcodec_struct.h"
#include "vdec_attrs.h"

#include "vutils/vdma_carveout.h"

extern const struct v4l2_file_operations vdec_file_ops;
extern const struct v4l2_file_operations vdo_file_ops;
extern const struct v4l2_file_operations vcap_file_ops;
extern const struct v4l2_file_operations venc_file_ops;
extern const struct v4l2_ioctl_ops vdec_ioctl_ops;
extern const struct v4l2_ioctl_ops vdo_ioctl_ops;
extern const struct v4l2_ioctl_ops vcap_ioctl_ops;
extern const struct v4l2_ioctl_ops venc_ioctl_ops;
extern const struct attribute_group vdec_attr_group;
extern const struct v4l2_ioctl_ops vdogav_ioctl_ops;
extern const struct v4l2_file_operations vdogav_file_ops;

static vcodec_share_t g_share = {0};

static dma_addr_t dma_carveout_addr[16];
static int dma_carveout_size[16];
static int dma_carveout_addr_cnt = 0;
static int dma_carveout_size_cnt = 0;
static int dma_carveout_blk_size = 0;
static int dma_carveout_init = 0;
static int dma_carveout_type = 0;

module_param_array_named(addr, dma_carveout_addr, int, &dma_carveout_addr_cnt, S_IRUGO);
module_param_array_named(size, dma_carveout_size, int, &dma_carveout_size_cnt, S_IRUGO);
module_param_named(blk, dma_carveout_blk_size, int, S_IRUGO);
module_param_named(carveout, dma_carveout_type, int, S_IRUGO);

static struct platform_device *pdev_vdec = NULL;
static struct platform_device *pdev_vdo0 = NULL;
static struct platform_device *pdev_vdo1 = NULL;
static struct platform_device *pdev_vcap = NULL;
static struct platform_device *pdev_vdogav = NULL;

static int createInstance(struct platform_device *pdev, const char *name, int nr,
	const struct v4l2_file_operations *fops, const struct v4l2_ioctl_ops *ioctl_ops)
{
	int ret;
	vcodec_device_t *device;
	struct v4l2_device *v4l2_dev = NULL;
	struct video_device *vdev = NULL;

	device = devm_kzalloc(&pdev->dev, sizeof(vcodec_device_t), GFP_KERNEL);
	if (!device) {
		pr_err("vcodec: devm_kzalloc fail\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&device->list);
	INIT_LIST_HEAD(&device->handle_list);

	mutex_init(&device->lock);

	device->pdev = pdev;
	device->dev = &pdev->dev;
	device->share = &g_share;

	strlcpy(device->v4l2_dev.name, name, sizeof(device->v4l2_dev.name));

	ret = v4l2_device_register(device->dev, &device->v4l2_dev);
	if (ret) {
		pr_err("vcodec: v4l2_device_register ret %d\n", ret);
		goto err;
	}

	v4l2_dev = &device->v4l2_dev;
	vdev = &device->vdev;

	vdev->release = video_device_release_empty;
	vdev->fops = fops;
	vdev->ioctl_ops	= ioctl_ops;
	vdev->lock = &device->lock;
	vdev->v4l2_dev = v4l2_dev;
	vdev->vfl_dir = VFL_DIR_M2M;

	ret = video_register_device(vdev, VFL_TYPE_GRABBER, nr);
	if (ret) {
		pr_err("vcodec: video_register_device ret %d\n", ret);
		goto err;
	}

	/* finalize */
	video_set_drvdata(vdev, device);
	platform_set_drvdata(pdev, device);

	return 0;

err:

	if (v4l2_dev)
		v4l2_device_unregister(v4l2_dev);

	return ret;
}

static int vcodec_probe(struct platform_device *pdev)
{
	int ret = -ENODEV, port = -1;
	vcodec_device_t *device;

	if (!strcmp(pdev->name, VDEC_PLATFORM_DEV_NAME)) {
		ret = createInstance(pdev, VDEC_V4L2_NAME, V4L2_EXT_DEV_NO_VDEC,
			&vdec_file_ops, &vdec_ioctl_ops);
		if (!ret) {
			g_share.vdec = platform_get_drvdata(pdev);
			ret = vdec_initAttrs(&pdev->dev);
			if (ret)
				pr_err("vcodec: fail to init vdec attrs\n");
		}
	}

	else if (!strcmp(pdev->name, VDO_PLATFORM_DEV_NAME)) {
		ret = createInstance(pdev,
			(pdev->id == 1)? VDO1_V4L2_NAME : VDO0_V4L2_NAME,
			(pdev->id == 1)? V4L2_EXT_DEV_NO_VDO1 : V4L2_EXT_DEV_NO_VDO0,
			&vdo_file_ops, &vdo_ioctl_ops);
		port = pdev->id;
	}

	else if (!strcmp(pdev->name, VCAP_PLATFORM_DEV_NAME)) {
		ret = createInstance(pdev, VCAP_V4L2_NAME, V4L2_EXT_DEV_NO_GPSCALER,
			&vcap_file_ops, &vcap_ioctl_ops);
		if (!ret) g_share.vcap = platform_get_drvdata(pdev);
	}

	else if (!strcmp(pdev->name, VENC_PLATFORM_DEV_NAME)) {
		ret = createInstance(pdev, VENC_V4L2_NAME, V4L2_EXT_DEV_NO_VENC,
			&venc_file_ops, &venc_ioctl_ops	);
	}

	else if (!strcmp(pdev->name, VDOGAV_PLATFORM_DEV_NAME)) {
		ret = createInstance(pdev, VDOGAV_V4L2_NAME, V4L2_EXT_DEV_NO_VDOGAV,
			&vdogav_file_ops, &vdogav_ioctl_ops);
	}

	if (ret) {
		pr_err("vcodec: fail to create instance ret %d\n", ret);
		return ret;
	}

	device = platform_get_drvdata(pdev);
	device->port = port;
	device->dma_carveout_type = dma_carveout_type;

	if (!dma_carveout_init && dma_carveout_type) {
		vdma_carveout_init(dma_carveout_addr, dma_carveout_size,
			min(dma_carveout_addr_cnt, dma_carveout_size_cnt),
			dma_carveout_blk_size);
		dma_carveout_init = 1;
	}

	return 0;
}

static int vcodec_remove(struct platform_device *pdev)
{
	vcodec_device_t *device;

	device = platform_get_drvdata(pdev);

	if (!strcmp(pdev->name, VDEC_PLATFORM_DEV_NAME)) {
		vdec_exitAttrs(&pdev->dev);
	}

	if (device) {
		video_unregister_device(&device->vdev);
		v4l2_device_unregister(&device->v4l2_dev);
	}

	if (dma_carveout_init) {
		vdma_carveout_exit();
		dma_carveout_init = 0;
	}

	return 0;
}

static struct platform_driver v4l2_vdec_drv = {
	.probe = vcodec_probe,
	.remove = vcodec_remove,
	.driver = {
		.name = VDEC_PLATFORM_DEV_NAME,
	},
};

static struct platform_driver v4l2_vdo_drv = {
	.probe = vcodec_probe,
	.remove = vcodec_remove,
	.driver = {
		.name = VDO_PLATFORM_DEV_NAME,
	},
};

static struct platform_driver v4l2_vcap_drv = {
	.probe = vcodec_probe,
	.remove = vcodec_remove,
	.driver = {
		.name = VCAP_PLATFORM_DEV_NAME,
	},
};

static struct platform_driver v4l2_vdogav_drv = {
	.probe = vcodec_probe,
	.remove = vcodec_remove,
	.driver = {
		.name = VDOGAV_PLATFORM_DEV_NAME,
	},
};

static int __init vcodec_drv_init(void)
{
	pdev_vdec = platform_device_register_simple(VDEC_PLATFORM_DEV_NAME, -1, NULL, 0);
	pdev_vdo0 = platform_device_register_simple(VDO_PLATFORM_DEV_NAME, 0, NULL, 0);
	pdev_vdo1 = platform_device_register_simple(VDO_PLATFORM_DEV_NAME, 1, NULL, 0);
	pdev_vcap = platform_device_register_simple(VCAP_PLATFORM_DEV_NAME, -1, NULL, 0);
	pdev_vdogav = platform_device_register_simple(VDOGAV_PLATFORM_DEV_NAME, -1, NULL, 0);

	platform_driver_register(&v4l2_vdec_drv);
	platform_driver_register(&v4l2_vdo_drv);
	platform_driver_register(&v4l2_vcap_drv);
	platform_driver_register(&v4l2_vdogav_drv);

	return 0;
}

static void __exit vcodec_drv_exit(void)
{
	platform_driver_unregister(&v4l2_vdec_drv);
	platform_driver_unregister(&v4l2_vdo_drv);
	platform_driver_unregister(&v4l2_vcap_drv);
	platform_driver_unregister(&v4l2_vdogav_drv);

	platform_device_unregister(pdev_vdec);
	platform_device_unregister(pdev_vdo0);
	platform_device_unregister(pdev_vdo1);
	platform_device_unregister(pdev_vcap);
	platform_device_unregister(pdev_vdogav);

	pdev_vdec =
	pdev_vdo0 =
	pdev_vdo1 =
	pdev_vcap =
	pdev_vdogav = NULL;
}

module_init(vcodec_drv_init);
module_exit(vcodec_drv_exit);
MODULE_LICENSE("GPL");
