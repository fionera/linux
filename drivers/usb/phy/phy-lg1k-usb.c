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
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/gpio.h>

#include "phy-lg1k-usb.h"

static int lg1k_usb_gpio_set_pin_mux(void __iomem *addr,
				unsigned int mask, unsigned int inv, int enable)
{
	unsigned int set_val;

	if (!addr)
		return -EFAULT;

	set_val = readl(addr);

	if (inv)
		set_val = enable ? (set_val & (~mask)) : (set_val | mask);
	else
		set_val = enable ? (set_val | mask) : (set_val & (~mask));

	writel(set_val, addr);

	return 0;
}

static DEFINE_SPINLOCK(lg1k_usb2_phy_reset_lock);

static int lg1k_usb_phy_set_reset_ctl(void __iomem *addr,
				unsigned int mask, unsigned int inv, int enable)
{
	unsigned int set_val;
	unsigned long flags;

	if (!addr)
		return -EFAULT;

	spin_lock_irqsave(&lg1k_usb2_phy_reset_lock, flags);

	set_val = readl(addr);

	if (inv)
		set_val = enable ? (set_val & (~mask)) : (set_val | mask);
	else
		set_val = enable ? (set_val | mask) : (set_val & (~mask));

	writel(set_val, addr);

	spin_unlock_irqrestore(&lg1k_usb2_phy_reset_lock, flags);

	return 0;
}

int lg1k_usbphy_parse_dt(struct lg1k_usbphy *lgphy)
{
	unsigned int regs[2];
	int ret;
	int i;
	struct device_node *np = lgphy->dev->of_node;

	if (!np) {
		dev_err(lgphy->dev, "device node is null\n");
		return -ENODEV;
	}

	i = of_property_match_string(np, "reg-names", "base");
	lgphy->base_addr = of_iomap(np, i);
	if (!lgphy->base_addr) {
		dev_err(lgphy->dev, "error base addr\n");
		ret = -EFAULT;
		goto cleanup;
	}

	i = of_property_match_string(np, "reg-names", "host");
	lgphy->host_reg = of_iomap(np, i);
	if (!lgphy->host_reg) {
		dev_err(lgphy->dev, "error host reg\n");
		ret = -EFAULT;
		goto cleanup;
	}

	i = of_property_match_string(np, "reg-names", "phy");
	lgphy->phy_reg = of_iomap(np, i);
	if (!lgphy->phy_reg) {
		dev_err(lgphy->dev, "error phy reg\n");
		ret = -EFAULT;
		goto cleanup;
	}

	i = of_property_match_string(np, "reg-names", "pinmux");
	lgphy->pin_mux = of_iomap(np, i);

	lgphy->ctl_gpio = lgphy->ocd_gpio = lgphy->rst_gpio = -1;
	lgphy->ctl_gpio = of_get_named_gpio(np,"ctrl-gpios", 0);
	lgphy->ocd_gpio = of_get_named_gpio(np,"ocd-gpios", 0);
	lgphy->rst_gpio = of_get_named_gpio(np,"rst-gpios", 0);
	lgphy->hw_initialized = 0;

	/* For USB3.0 only */
	i = of_property_match_string(np, "reg-names", "gbl");
	lgphy->gbl_reg = of_iomap(np, i);

	/* USB SYNTOP CTRL */
	i = of_property_match_string(np, "reg-names", "syntop");
	lgphy->syntop = of_iomap(np, i);
	if (lgphy->syntop) {
		ret = of_property_read_u32(np, "syntop_val", &regs[0]);
		if (ret) {
			dev_err(lgphy->dev, "no syntop_val\n");

			goto cleanup;
		}
		lgphy->syntop_val = regs[0];
		dev_info(lgphy->dev, "syntop_val=0x%x\n", regs[0]);
	}

	/* For USB2.0 only */
	i = of_property_match_string(np, "reg-names", "phyrst");
	lgphy->phy_rst = of_iomap(np, i);

	return 0;

cleanup:
	if (lgphy->base_addr)
		iounmap(lgphy->base_addr);
	if (lgphy->host_reg)
		iounmap(lgphy->host_reg);
	if (lgphy->phy_reg)
		iounmap(lgphy->phy_reg);
	if (lgphy->pin_mux)
		iounmap(lgphy->pin_mux);
	if (lgphy->gbl_reg)
		iounmap(lgphy->gbl_reg);
	if (lgphy->syntop)
		iounmap(lgphy->syntop);
	if (lgphy->phy_rst)
		iounmap(lgphy->phy_rst);
	return ret;
}
EXPORT_SYMBOL_GPL(lg1k_usbphy_parse_dt);

void lg1k_usbphy_remove(struct lg1k_usbphy *lgphy)
{
	usb_remove_phy(&lgphy->phy);
	iounmap(lgphy->base_addr);
	iounmap(lgphy->host_reg);
	iounmap(lgphy->phy_reg);

	if (lgphy->pin_mux)
		iounmap(lgphy->pin_mux);
	if (lgphy->gbl_reg)
		iounmap(lgphy->gbl_reg);
	if (lgphy->syntop)
		iounmap(lgphy->syntop);
	if (lgphy->phy_rst)
		iounmap(lgphy->phy_rst);
}
EXPORT_SYMBOL_GPL(lg1k_usbphy_remove);

int lg1k_usbphy_set_vbus(struct lg1k_usbphy *lgphy, int on)
{
	if (lgphy->pin_mux) {
		unsigned int regs[2];
		int ret;

		if (of_property_read_u32_array(lgphy->dev->of_node,
					"gpio-pinmux", regs, 2)) {
			dev_warn(lgphy->dev, "pinmux won't be set\n");
		} else {
			ret = lg1k_usb_gpio_set_pin_mux(lgphy->pin_mux, regs[0],
					regs[1], 1);
			if (ret) {
				dev_err(lgphy->dev, "error in pinmux set\n");
				return ret;
			}
		}
	}

	/*
	 * gpio_request() and gpio_free() should not be called in order to avoid
	 * interference with user space agents.
	 */
	if (lgphy->ctl_gpio >= 0) {
		gpio_direction_output(lgphy->ctl_gpio, on);
		gpio_set_value(lgphy->ctl_gpio, on);

		dev_info(lgphy->dev, "vbus turn %s\n", on ? "on" : "off");
	}

	if (on && (lgphy->ocd_gpio >= 0)) {
		gpio_direction_input(lgphy->ocd_gpio);
	}

	if (on && (lgphy->rst_gpio >= 0)) {
		gpio_direction_output(lgphy->rst_gpio, 0);
		gpio_set_value(lgphy->rst_gpio, 0);
		mdelay(10);
		gpio_set_value(lgphy->rst_gpio, 1);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(lg1k_usbphy_set_vbus);

int lg1k_usbphy_set_reset_ctl_mask(struct lg1k_usbphy *lgphy, int on)
{
	unsigned int regs[2];
	int ret;

	if (!lgphy->phy_rst)
		return 0;

	if (of_property_read_u32_array(lgphy->dev->of_node, "phy-rst-msk",
				regs, 2)) {
		dev_warn(lgphy->dev, "phy_rst exists, but won't be set\n");
		return 0;
	}

	ret = lg1k_usb_phy_set_reset_ctl(lgphy->phy_rst, regs[0], regs[1], on);
	if (ret) {
		dev_err(lgphy->dev, "err phy-rst-msk\n");
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(lg1k_usbphy_set_reset_ctl_mask);

int lg1k_usb_phy_set_type(struct usb_phy *phy, enum usb_phy_type type)
{
	phy->type = type;

	return 0;
}
EXPORT_SYMBOL_GPL(lg1k_usb_phy_set_type);

static int lg1k_usb_phy_init(struct usb_phy *phy)
{
	struct lg1k_usbphy *lgphy = phy_to_lgphy(phy);

	if (lgphy->init)
		lgphy->init(lgphy);

	return 0;
}

static int lg1k_usb_phy_set_vbus(struct usb_phy *phy, int on)
{
	struct lg1k_usbphy *lgphy = phy_to_lgphy(phy);

	return lg1k_usbphy_set_vbus(lgphy, on);
}

static void lg1k_usb_phy_shutdown(struct usb_phy *phy)
{
	struct lg1k_usbphy *lgphy = phy_to_lgphy(phy);

	dev_warn(phy->dev, "Not properly implemeted yet..\n");
	lg1k_usbphy_set_vbus(lgphy, false);
}

static int lg1k_usb_phy_set_wakeup(struct usb_phy *phy, bool enabled)
{
	dev_warn(phy->dev, "Wake up setting is not suppored!\n");

	return 0;
}

static int lg1k_usb_phy_set_suspend(struct usb_phy *phy, int suspend)
{
	dev_warn(phy->dev, "Suspend setting is not supported!\n");

	return 0;
}

int lg1k_usb_phy_add(struct usb_phy *phy, struct device *dev,
		const char *label, enum usb_phy_type type)
{
	phy->dev		= dev;
	phy->label		= label;
	phy->type		= type;
	phy->init		= lg1k_usb_phy_init;
	phy->shutdown		= lg1k_usb_phy_shutdown;
	phy->set_vbus		= lg1k_usb_phy_set_vbus;
	phy->set_wakeup		= lg1k_usb_phy_set_wakeup;
	phy->set_suspend	= lg1k_usb_phy_set_suspend;

	dev_info(phy->dev, "%s:add usb phy\n", __func__);

	return usb_add_phy_dev(phy);
}
EXPORT_SYMBOL_GPL(lg1k_usb_phy_add);

MODULE_DESCRIPTION("LG DTV USB phy controller");
MODULE_AUTHOR("Shinhoe Kim <shinhoe.kim@lge.com>, Daewoong Kim <daewoong00.kim@lge.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lg1k-usbphy");
