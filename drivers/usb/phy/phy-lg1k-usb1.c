/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/err.h>

#include "phy-lg1k-usb.h"

static int lg1k_usb1_init(struct lg1k_usbphy *lgphy)
{
	if (lgphy->hw_initialized) {
		dev_info(lgphy->dev, "phy is already initialized\n");
		return 0;
	}

	lgphy->hw_initialized = 1;

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int lg1k_usb1_suspend_late(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct lg1k_usbphy *lgphy = platform_get_drvdata(pdev);

	dev_info(dev, "suspend_late phy\n");

	lgphy->hw_initialized = 0;

	return 0;
}

static int lg1k_usb1_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct lg1k_usbphy *lgphy = platform_get_drvdata(pdev);

	dev_info(dev, "resume phy\n");

	if (lgphy->hw_initialized) {
		dev_info(lgphy->dev, "phy is already initialized\n");
		return 0;
	}

	lgphy->hw_initialized = 1;

	return 0;
}

static int lg1k_usb1_resume_early(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops lg1k_usb1_pm_ops = {
	.resume = lg1k_usb1_resume,
	SET_LATE_SYSTEM_SLEEP_PM_OPS(lg1k_usb1_suspend_late,
						lg1k_usb1_resume_early)
};
#define DEV_PM_OPS	(&lg1k_usb1_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif /* CONFIG_PM_SLEEP */

static int lg1k_usb1_platform_probe(struct platform_device *pdev)
{
	struct lg1k_usbphy *lgphy;
	struct device *dev = &pdev->dev;
	int ret;

	lgphy = devm_kzalloc(dev, sizeof(*lgphy), GFP_KERNEL);
	if (!lgphy) {
		dev_err(dev, "lgphy alloc fail!\n");
		return -ENOMEM;
	}
	lgphy->dev = dev;
	lgphy->id = pdev->id;
	lgphy->init = lg1k_usb1_init;

	if (dev->of_node) {
		ret = lg1k_usbphy_parse_dt(lgphy);
		if (ret < 0) {
			dev_err(dev, "device tree pasing fail!\n");
			return ret;
		}
	}

	platform_set_drvdata(pdev, lgphy);
	device_enable_async_suspend(dev);

	return lg1k_usb_phy_add(&lgphy->phy, dev, "lg1k-usb1phy",
			USB_PHY_TYPE_UNDEFINED);
}

static int lg1k_usb1_platform_remove(struct platform_device *pdev)
{
	struct lg1k_usbphy *lgphy = platform_get_drvdata(pdev);

	dev_info(lgphy->dev, "remove phy\n");

	lg1k_usbphy_remove(lgphy);

	return 0;
}

static struct platform_device_id lg1k_usb1_platform_ids[] = {
	{ .name = "lg1k-usb1phy", },
	{},
};
MODULE_DEVICE_TABLE(platform, lg1k_usb1_platform_ids);

static struct platform_driver lg1k_usb1_platform_driver = {
	.probe		= lg1k_usb1_platform_probe,
	.remove		= lg1k_usb1_platform_remove,
	.id_table	= lg1k_usb1_platform_ids,
	.driver	= {
		.name = "lg1k-usb1phy",
		.pm = DEV_PM_OPS,
	},
};
module_platform_driver(lg1k_usb1_platform_driver);

MODULE_DESCRIPTION("LG DTV USB 1.1 phy controller");
MODULE_AUTHOR("Shinhoe Kim <shinhoe.kim@lge.com>, Daewoong Kim <daewoong00.kim@lge.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lg1k-usb1phy");
