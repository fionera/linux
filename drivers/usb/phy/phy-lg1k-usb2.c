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

static int lg1k_usb2_phy_init(struct lg1k_usbphy *lgphy)
{
	if (lgphy->syntop)
		writel(lgphy->syntop_val, lgphy->syntop);

	return 0;
}

static int lg1k_usb2_reset(struct lg1k_usbphy *lgphy)
{
	union usb2_host_reset_union host_reset;

	if (!lgphy->phy_reg || !lgphy->host_reg) {
		dev_err(lgphy->dev, "error reg iomem\n");
		return -EFAULT;
	}

	if (lgphy->phy_rst) {
		lg1k_usbphy_set_reset_ctl_mask(lgphy, 1);
	} else {
		union usb2_phy_reset_union phy_reset;
		phy_reset.val = readl(lgphy->phy_reg +
						phy_offset(2, reset));
		phy_reset.reg.usb2_phy_reset = 1;
		writel(phy_reset.val, lgphy->phy_reg +
						phy_offset(2, reset));
	}
	usleep_range(1000, 1100);

	host_reset.val = readl(lgphy->host_reg + host_offset(2, reset));
	host_reset.reg.usb2_host_utmi_reset = 1;
	writel(host_reset.val, lgphy->host_reg + host_offset(2, reset));
	host_reset.reg.usb2_host_bus_reset = 1;
	host_reset.reg.usb2_host_core_reset = 1;
	writel(host_reset.val, lgphy->host_reg + host_offset(2, reset));

	return 0;
}

static int lg1k_usb2_init(struct lg1k_usbphy *lgphy)
{
	if (lgphy->hw_initialized) {
		dev_info(lgphy->dev, "phy is already initialized\n");
		return 0;
	}

	lg1k_usb2_phy_init(lgphy);
	lg1k_usb2_reset(lgphy);

	lgphy->hw_initialized = 1;

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int lg1k_usb2_phy_ctrl_backup(struct lg1k_usbphy *lgphy)
{
	struct usb2_phy_ctrl_backup *p;

	if (!lgphy->phy_reg) {
		dev_err(lgphy->dev, "error phy reg iomem\n");
		return -1;
	}
	dev_dbg(lgphy->dev, "phy ctrl backup\n");

	/* backup phy parameters */
	p = &lgphy->priv.usb2.phy_ctrl_backup;

	if (lgphy->phy_rst) {
		p->param0 = readl(lgphy->phy_reg + phy_param_offset(2, param0));
		p->param1 = readl(lgphy->phy_reg + phy_param_offset(2, param1));
	} else {
		p->param0 = readl(lgphy->phy_reg + phy_offset(2, param0));
		p->param1 = readl(lgphy->phy_reg + phy_offset(2, param1));
	}

	return 0;
}

static int lg1k_usb2_phy_ctrl_restore(struct lg1k_usbphy *lgphy)
{
	struct usb2_phy_ctrl_backup *p;

	if (!lgphy->phy_reg) {
		dev_err(lgphy->dev, "error phy reg iomem\n");
		return -1;
	}
	dev_dbg(lgphy->dev, "phy ctrl restore\n");

	/* restore phy parameters */
	p = &lgphy->priv.usb2.phy_ctrl_backup;

	if (lgphy->phy_rst) {
		writel(p->param0, lgphy->phy_reg + phy_param_offset(2, param0));
		writel(p->param1, lgphy->phy_reg + phy_param_offset(2, param1));
	} else {
		writel(p->param0, lgphy->phy_reg + phy_offset(2, param0));
		writel(p->param1, lgphy->phy_reg + phy_offset(2, param1));
	}

	return 0;
}

static int lg1k_usb2_host_ctrl_backup(struct lg1k_usbphy *lgphy)
{
	struct usb2_host_ctrl_backup *ph;

	if (!lgphy->host_reg) {
		dev_err(lgphy->dev, "error host reg iomem\n");
		return -1;
	}
	dev_dbg(lgphy->dev, "host ctrl backup\n");

	/* backup host parameters */
	ph = &lgphy->priv.usb2.host_ctrl_backup;
	ph->param0 = readl(lgphy->host_reg + host_offset(2, param0));

	return 0;
}

static int lg1k_usb2_host_ctrl_restore(struct lg1k_usbphy *lgphy)
{
	struct usb2_host_ctrl_backup *ph;

	if (!lgphy->host_reg) {
		dev_err(lgphy->dev, "error host reg iomem\n");
		return -1;
	}
	dev_dbg(lgphy->dev, "host ctrl restore\n");

	/* restore host parameters */
	ph = &lgphy->priv.usb2.host_ctrl_backup;
	writel(ph->param0, lgphy->host_reg + host_offset(2, param0));

	return 0;
}

static int lg1k_usb2_ehci_insnreg_backup(struct lg1k_usbphy *lgphy)
{
	struct usb2_ehci_insnreg_backup *ei;
	struct device_node *np = lgphy->dev->of_node;

	if (!of_device_is_compatible(np->parent, "lge,lg115x-ehci"))
		return 0;
	if (!lgphy->base_addr) {
		dev_err(lgphy->dev, "error host base iomem\n");
		return -1;
	}

	dev_dbg(lgphy->dev, "ehci insnregs backup\n");

	/* backup ehci insnregs */
	ei = &lgphy->priv.usb2.ehci_insnreg_backup;
	ei->insnreg01 = readl(lgphy->base_addr + EHCI_INSNREG01);
	ei->insnreg02 = readl(lgphy->base_addr + EHCI_INSNREG02);
	ei->insnreg03 = readl(lgphy->base_addr + EHCI_INSNREG03);

	return 0;
}

static int lg1k_usb2_ehci_insnreg_restore(struct lg1k_usbphy *lgphy)
{
	struct usb2_ehci_insnreg_backup *ei;
	struct device_node *np = lgphy->dev->of_node;

	if (!of_device_is_compatible(np->parent, "lge,lg115x-ehci"))
		return 0;
	if (!lgphy->base_addr) {
		dev_err(lgphy->dev, "error host base iomem\n");
		return -1;
	}

	dev_dbg(lgphy->dev, "ehci insnregs restore\n");

	/* restore ehci insnregs */
	ei = &lgphy->priv.usb2.ehci_insnreg_backup;
	writel(ei->insnreg01, lgphy->base_addr + EHCI_INSNREG01);
	writel(ei->insnreg02, lgphy->base_addr + EHCI_INSNREG02);
	writel(ei->insnreg03, lgphy->base_addr + EHCI_INSNREG03);

	return 0;
}

static int lg1k_usb2_suspend_late(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct lg1k_usbphy *lgphy = platform_get_drvdata(pdev);

	dev_info(dev, "suspend_late phy\n");

	lg1k_usb2_ehci_insnreg_backup(lgphy);
	lg1k_usb2_phy_ctrl_backup(lgphy);
	lg1k_usb2_host_ctrl_backup(lgphy);
	lg1k_usbphy_set_vbus(lgphy, false);

	lgphy->hw_initialized = 0;

	return 0;
}

static int lg1k_usb2_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct lg1k_usbphy *lgphy = platform_get_drvdata(pdev);

	dev_info(dev, "resume phy\n");

	if (lgphy->hw_initialized) {
		dev_info(lgphy->dev, "phy is already initialized\n");
		return 0;
	}

	lg1k_usb2_host_ctrl_restore(lgphy);
	lg1k_usb2_phy_ctrl_restore(lgphy);
	lg1k_usb2_reset(lgphy);
	lg1k_usb2_ehci_insnreg_restore(lgphy);
	usleep_range(100000, 100100);
	lg1k_usbphy_set_vbus(lgphy, true);

	lgphy->hw_initialized = 1;

	return 0;
}

static int lg1k_usb2_resume_early(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops lg1k_usb2_pm_ops = {
	.resume = lg1k_usb2_resume,
	SET_LATE_SYSTEM_SLEEP_PM_OPS(lg1k_usb2_suspend_late,
						lg1k_usb2_resume_early)
};
#define DEV_PM_OPS	(&lg1k_usb2_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif /* CONFIG_PM_SLEEP */

static int lg1k_usb2_platform_probe(struct platform_device *pdev)
{
	struct lg1k_usbphy *lgphy;
	struct device *dev = &pdev->dev;
	int ret;

	lgphy = devm_kzalloc(dev, sizeof(*lgphy), GFP_KERNEL);
	if (!lgphy) {
		dev_err(dev, "lgphy alloc fail|\n");
		return -ENOMEM;
	}
	lgphy->dev = dev;
	lgphy->id = pdev->id;
	lgphy->init = lg1k_usb2_init;

	if (dev->of_node) {
		ret = lg1k_usbphy_parse_dt(lgphy);
		if (ret < 0) {
			dev_err(dev, "device tree parsing fail!\n");
			return ret;
		}
	}

	platform_set_drvdata(pdev, lgphy);
	device_enable_async_suspend(dev);

	return lg1k_usb_phy_add(&lgphy->phy, dev, "lg1k-usb2phy",
			USB_PHY_TYPE_UNDEFINED);
}

static int lg1k_usb2_platform_remove(struct platform_device *pdev)
{
	struct lg1k_usbphy *lgphy = platform_get_drvdata(pdev);

	dev_info(lgphy->dev, "remove phy\n");

	lg1k_usbphy_remove(lgphy);

	return 0;
}

static struct platform_device_id lg1k_usb2_platform_ids[] = {
	{ .name = "lg1k-usb2phy", },
	{ .name = "lg1k-drd2phy", },
	{},
};
MODULE_DEVICE_TABLE(platform, lg1k_usb2_platform_ids);

static struct platform_driver lg1k_usb2_platform_driver = {
	.probe		= lg1k_usb2_platform_probe,
	.remove		= lg1k_usb2_platform_remove,
	.id_table	= lg1k_usb2_platform_ids,
	.driver	= {
		.name = "lg1k-usb2phy",
		.pm = DEV_PM_OPS,
	},
};
module_platform_driver(lg1k_usb2_platform_driver);

MODULE_DESCRIPTION("LG DTV USB 2.0 phy controller");
MODULE_AUTHOR("Shinhoe Kim <shinhoe.kim@lge.com>, Daewoong Kim <daewoong00.kim@lge.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lg1k-usb2phy");
