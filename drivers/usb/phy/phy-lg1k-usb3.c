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
/*
 * Part of SS Function Control Registers based on "SuperSpeed USB 3.0 PHY for
 * TSMC 40-nm LP 1.1/2.5 V Databook"
 * RX_OVRD_IN_HI register for rx equalizer tuning
 */
#define RX_OVRD_IN_HI 0x1006
#define RX_EQ_OVRD_MASK (1 << 11)
#define RX_EQ_MASK (7 << 8)
#define RX_EQ_EN_OVRD_MASK (1 << 7)
#define RX_EQ_EN_MASK (1 << 6)
#define FIXED_RX_EQ (RX_EQ_OVRD_MASK | RX_EQ_EN_OVRD_MASK)
#define CR_ACK 0x1
#define CR_NAK 0x0

static int lg1k_usb3_phy_init(struct lg1k_usbphy *lgphy)
{
	if (lgphy->syntop)
		writel(lgphy->syntop_val, lgphy->syntop);

	return 0;
}

static int lg1k_usb3_reset(struct lg1k_usbphy *lgphy)
{
	union usb3_phy_reset_union phy_reset;
	union usb3_host_reset_union host_reset;

	if (!lgphy->phy_reg || !lgphy->host_reg) {
		dev_err(lgphy->dev, "error reg iomem\n");
		return -EFAULT;
	}

	phy_reset.val = readl(lgphy->phy_reg + phy_offset(3, reset));
	host_reset.val = readl(lgphy->host_reg + host_offset(3, reset));

	phy_reset.reg.usb3_phy_reset = 1;
	writel(phy_reset.val, lgphy->phy_reg + phy_offset(3, reset));

	/* delay is required between phy_reset and host_reset for some chips */
	usleep_range(700, 800);

	host_reset.reg.usb3_host_bus_reset = 1;
	host_reset.reg.usb3_host_core_reset = 1;
	writel(host_reset.val, lgphy->host_reg + host_offset(3, reset));

	return 0;
}

static inline void lg1k_usb3_wait_cr_ack(void __iomem *ack_addr, int ack_val)
{
	long cnt = 0;
	long timeout = 10 * 1000;
	union usb3_phy_param_5_union parm5;

	while (cnt < timeout) {
		parm5.val = readl(ack_addr);
		if (parm5.reg.phy_cr_ack == ack_val)
			break;
		cnt++;
		udelay(1);
	}
}

static void lg1k_usb3_cr_addr_capture(struct lg1k_usbphy *lgphy, u16 addr)
{
	union usb3_phy_param_4_union parm4;

	if (!lgphy->phy_reg) {
		dev_err(lgphy->dev, "error phy reg iomem\n");
		return;
	}

	parm4.val = readl(lgphy->phy_reg + phy_offset(3, param4));

	parm4.reg.phy_cr_data_in = addr;
	writel(parm4.val, lgphy->phy_reg + phy_offset(3, param4));

	parm4.reg.phy_cr_cap_addr = 0x1;
	writel(parm4.val, lgphy->phy_reg + phy_offset(3, param4));
	lg1k_usb3_wait_cr_ack(lgphy->phy_reg + phy_offset(3, param5), CR_ACK);

	parm4.reg.phy_cr_cap_addr = 0x0;
	writel(parm4.val, lgphy->phy_reg + phy_offset(3, param4));
	lg1k_usb3_wait_cr_ack(lgphy->phy_reg + phy_offset(3, param5), CR_NAK);
}

static u16 lg1k_usb3_cr_read(struct lg1k_usbphy *lgphy, u16 addr)
{
	union usb3_phy_param_4_union parm4;
	union usb3_phy_param_5_union parm5;
	u16 val;

	if (!lgphy->phy_reg) {
		dev_err(lgphy->dev, "error phy reg iomem\n");
		return 0;
	}

	lg1k_usb3_cr_addr_capture(lgphy, addr);

	parm4.val = readl(lgphy->phy_reg + phy_offset(3, param4));

	parm4.reg.phy_cr_read = 1;
	writel(parm4.val, lgphy->phy_reg + phy_offset(3, param4));
	lg1k_usb3_wait_cr_ack(lgphy->phy_reg + phy_offset(3, param5), CR_ACK);

	parm5.val = readl(lgphy->phy_reg + phy_offset(3, param5));
	val = parm5.reg.phy_cr_data_out;

	parm4.reg.phy_cr_read = 0;
	writel(parm4.val, lgphy->phy_reg + phy_offset(3, param4));
	lg1k_usb3_wait_cr_ack(lgphy->phy_reg + phy_offset(3, param5), CR_NAK);

	return val;
}

static int lg1k_usb3_rx_equalizer_backup(struct lg1k_usbphy *lgphy)
{
	u16 addr = RX_OVRD_IN_HI;
	u16 val;

	if (!lgphy->phy_reg) {
		dev_err(lgphy->dev, "error phy reg iomem\n");
		return -EFAULT;
	}

	val = lg1k_usb3_cr_read(lgphy, addr);
	dev_info(lgphy->dev, "rx_eq_val backup: 0x%x -> 0x%x, index=0x%x\n",
			lgphy->priv.usb3.rx_eq_val, val, val & RX_EQ_MASK);

	lgphy->priv.usb3.rx_eq_val = val;

	if ((val & FIXED_RX_EQ) != FIXED_RX_EQ)
		dev_warn(lgphy->dev, "adaptive equalization used 0x%x\n", val);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static void lg1k_usb3_cr_write(struct lg1k_usbphy *lgphy, u16 addr, u16 val)
{
	union usb3_phy_param_4_union parm4;

	if (!lgphy->phy_reg) {
		dev_err(lgphy->dev, "error phy reg iomem\n");
		return;
	}

	lg1k_usb3_cr_addr_capture(lgphy, addr);

	parm4.val = readl(lgphy->phy_reg + phy_offset(3, param4));

	parm4.reg.phy_cr_data_in = val;
	writel(parm4.val, lgphy->phy_reg + phy_offset(3, param4));

	parm4.reg.phy_cr_cap_data = 1;
	writel(parm4.val, lgphy->phy_reg + phy_offset(3, param4));
	lg1k_usb3_wait_cr_ack(lgphy->phy_reg + phy_offset(3, param5), CR_ACK);
	parm4.reg.phy_cr_cap_data = 0;
	writel(parm4.val, lgphy->phy_reg + phy_offset(3, param4));
	lg1k_usb3_wait_cr_ack(lgphy->phy_reg + phy_offset(3, param5), CR_NAK);

	parm4.reg.phy_cr_write = 1;
	writel(parm4.val, lgphy->phy_reg + phy_offset(3, param4));
	lg1k_usb3_wait_cr_ack(lgphy->phy_reg + phy_offset(3, param5), CR_ACK);
	parm4.reg.phy_cr_write = 0;
	writel(parm4.val, lgphy->phy_reg + phy_offset(3, param4));
	lg1k_usb3_wait_cr_ack(lgphy->phy_reg + phy_offset(3, param5), CR_NAK);
}

static int lg1k_usb3_rx_equalizer_restore(struct lg1k_usbphy *lgphy)
{
	u16 addr = RX_OVRD_IN_HI;
	u16 val;

	if (!lgphy->phy_reg) {
		dev_err(lgphy->dev, "error phy reg iomem\n");
		return -EFAULT;
	}

	val = lgphy->priv.usb3.rx_eq_val;
	if (val)
		lg1k_usb3_cr_write(lgphy, addr, val);
	else
		dev_warn(lgphy->dev, "no rx equalizer setting\n");

	dev_info(lgphy->dev, "rx_eq_val restore: 0x%x, index=0x%x\n",
			val, val & RX_EQ_MASK);

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

#ifdef LG1K_SUPPORT_QUIRK_CHIP
static int lg1k_usb3_port_up(struct lg1k_usbphy *lgphy)
{
	unsigned int val;

	if (!lgphy || !lgphy->gbl_reg || !lgphy->base_addr)
		return -EFAULT;

	/* gbl_reg = base_addr + 0xC000 */
	val = readl(lgphy->gbl_reg + 0x2C0);
	val &= ~(1 << 31);
	writel(val, lgphy->gbl_reg + 0x2C0);

	msleep(100);

	val = readl(lgphy->base_addr + 0x420);
	val |= (1 << 9);
	writel(val, lgphy->base_addr + 0x420);

	return 0;

}

static int lg1k_usb3_port_reset(struct lg1k_usbphy *lgphy)
{
	union usb3_phy_reset_union phy_rst;
	unsigned int val;

	if (!lgphy || !lgphy->base_addr || !lgphy->gbl_reg || !lgphy->phy_reg)
		return -EFAULT;

	val = readl(lgphy->base_addr+0x420);
	val &= ~(1<<9);
	writel(val, lgphy->base_addr + 0x420);
	msleep(100);

	/* gbl_reg = base_addr + 0xC000 */
	val = readl(lgphy->gbl_reg + 0x2C0);
	val |= (1 << 31);

	phy_rst.val = readl(lgphy->phy_reg + phy_offset(3, reset));
	phy_rst.reg.usb3_phy_reset = 0;
	writel(phy_rst.val, lgphy->phy_reg + phy_offset(3, reset));

	return 0;
}

static int lg1k_usb3_port_start(struct lg1k_usbphy *lgphy)
{
	union usb3_phy_reset_union phy_rst;

	if (!lgphy || !lgphy->phy_reg)
		return -EFAULT;

	phy_rst.val = readl(lgphy->phy_reg + phy_offset(3, reset));
	phy_rst.reg.usb3_phy_reset = 1;
	writel(phy_rst.val, lgphy->phy_reg + phy_offset(3, reset));

	return 0;
}
#endif /* LG1K_SUPPORT_QUIRK_CHIP */

static int lg1k_usb3_init(struct lg1k_usbphy *lgphy)
{
	if (lgphy->hw_initialized) {
		dev_info(lgphy->dev, "phy is already initialized\n");
		return 0;
	}

	lg1k_usb3_phy_init(lgphy);
	lg1k_usb3_reset(lgphy);

	/* backup to check rx equalization value */
	if (lg1k_usb3_rx_equalizer_backup(lgphy))
		dev_warn(lgphy->dev, "rx eq backing-up fail! continue.\n");

	lgphy->hw_initialized = 1;

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int lg1k_usb3_phy_ctrl_backup(struct lg1k_usbphy *lgphy)
{
	struct usb3_phy_ctrl_backup *pp;

	if (!lgphy->phy_reg) {
		dev_err(lgphy->dev, "error phy reg iomem\n");
		return -1;
	}
	dev_dbg(lgphy->dev, "phy ctrl backup\n");

	/* backup phy parameters */
	pp = &lgphy->priv.usb3.phy_ctrl_backup;

	pp->param0 = readl(lgphy->phy_reg + phy_offset(3, param0));
	pp->param1 = readl(lgphy->phy_reg + phy_offset(3, param1));
	pp->param3 = readl(lgphy->phy_reg + phy_offset(3, param3));
	pp->param2 = readl(lgphy->phy_reg + phy_offset(3, param2));
	pp->param6 = readl(lgphy->phy_reg + phy_offset(3, param6));

	return 0;
}

static int lg1k_usb3_phy_ctrl_restore(struct lg1k_usbphy *lgphy)
{
	struct usb3_phy_ctrl_backup *pp;

	if (!lgphy->phy_reg) {
		dev_err(lgphy->dev, "error phy reg iomem\n");
		return -1;
	}

	dev_dbg(lgphy->dev, "phy ctrl restore\n");

	/* restore phy parameters */
	pp = &lgphy->priv.usb3.phy_ctrl_backup;

	writel(pp->param0, lgphy->phy_reg + phy_offset(3, param0));
	writel(pp->param1, lgphy->phy_reg + phy_offset(3, param1));
	writel(pp->param3, lgphy->phy_reg + phy_offset(3, param3));
	writel(pp->param2, lgphy->phy_reg + phy_offset(3, param2));
	writel(pp->param6, lgphy->phy_reg + phy_offset(3, param6));

	return 0;
}

static int lg1k_usb3_host_ctrl_backup(struct lg1k_usbphy *lgphy)
{
	struct usb3_host_ctrl_backup *pp;

	if (!lgphy->host_reg) {
		dev_err(lgphy->dev, "error host reg iomem\n");
		return -1;
	}
	dev_dbg(lgphy->dev, "host ctrl backup\n");

	/* backup host parameters */
	pp = &lgphy->priv.usb3.host_ctrl_backup;

	pp->param0 = readl(lgphy->host_reg + host_offset(3, param0));

	return 0;
}

static int lg1k_usb3_host_ctrl_restore(struct lg1k_usbphy *lgphy)
{
	struct usb3_host_ctrl_backup *pp;

	if (!lgphy->host_reg) {
		dev_err(lgphy->dev, "error host reg iomem\n");
		return -1;
	}

	dev_dbg(lgphy->dev, "host ctrl restore\n");

	/* restore host parameters */
	pp = &lgphy->priv.usb3.host_ctrl_backup;

	writel(pp->param0, lgphy->host_reg + host_offset(3, param0));

	return 0;
}

static int lg1k_usb3_gbl_regs_backup(struct lg1k_usbphy *lgphy)
{
	struct usb3_gbl_regs_backup *pg;

	if (!lgphy->phy_reg) {
		dev_err(lgphy->dev, "error phy reg iomem\n");
		return -1;
	}
	dev_dbg(lgphy->dev, "global registers backup\n");

	/* backup global registers */
	pg = &lgphy->priv.usb3.gbl_regs_backup;

	pg->gsbuscfg0 = readl(lgphy->gbl_reg + GBL_GSBUSCFG0);
	pg->grxthrcfg = readl(lgphy->gbl_reg + GBL_GRXTHRCFG);
	pg->guctl1 = readl(lgphy->gbl_reg + GBL_GUCTL1);
	pg->guctl = readl(lgphy->gbl_reg + GBL_GUCTL);
	pg->gusb2phycfg = readl(lgphy->gbl_reg + GBL_GUSB2PHYCFG);
	pg->guctl2 = readl(lgphy->gbl_reg + GBL_GUCTL2);

	return 0;
}

static int lg1k_usb3_gbl_regs_restore(struct lg1k_usbphy *lgphy)
{
	struct usb3_gbl_regs_backup *pg;

	if (!lgphy->phy_reg) {
		dev_err(lgphy->dev, "error phy reg iomem\n");
		return -1;
	}
	dev_dbg(lgphy->dev, "global registers restore\n");

	/* restore global registers */
	pg = &lgphy->priv.usb3.gbl_regs_backup;

	writel(pg->gsbuscfg0, lgphy->gbl_reg + GBL_GSBUSCFG0);
	writel(pg->grxthrcfg, lgphy->gbl_reg + GBL_GRXTHRCFG);
	writel(pg->guctl1, lgphy->gbl_reg + GBL_GUCTL1);
	writel(pg->guctl, lgphy->gbl_reg + GBL_GUCTL);
	writel(pg->gusb2phycfg, lgphy->gbl_reg + GBL_GUSB2PHYCFG);
	writel(pg->guctl2, lgphy->gbl_reg + GBL_GUCTL2);

	return 0;
}

static int lg1k_usb3_suspend_late(struct device *dev)
{
	struct platform_device *pdev;
	struct lg1k_usbphy *lgphy;

	pdev = to_platform_device(dev);
	lgphy = platform_get_drvdata(pdev);

	dev_info(dev, "suspend_late phy\n");

	if (lg1k_usb3_rx_equalizer_backup(lgphy))
		dev_warn(lgphy->dev, "rx eq backing-up fail! continue.\n");

	lg1k_usb3_gbl_regs_backup(lgphy);
	lg1k_usb3_phy_ctrl_backup(lgphy);
	lg1k_usb3_host_ctrl_backup(lgphy);

	lgphy->hw_initialized = 0;

	return 0;
}

static int lg1k_usb3_resume(struct device *dev)
{
	struct platform_device *pdev;
	struct lg1k_usbphy *lgphy;

	pdev = to_platform_device(dev);
	lgphy = platform_get_drvdata(pdev);

	dev_info(dev, "resume phy\n");

	if (lgphy->hw_initialized) {
		dev_info(lgphy->dev, "phy is already initialized\n");
		return 0;
	}

	lg1k_usbphy_set_vbus(lgphy, true);

	lg1k_usb3_host_ctrl_restore(lgphy);
	lg1k_usb3_phy_init(lgphy);
	lg1k_usb3_phy_ctrl_restore(lgphy);
	lg1k_usb3_reset(lgphy);
	lg1k_usb3_gbl_regs_restore(lgphy);

	if (lg1k_usb3_rx_equalizer_restore(lgphy))
		dev_warn(lgphy->dev, "rx eq restoring fail! continue.\n");

	lgphy->hw_initialized = 1;

	return 0;
}

static int lg1k_usb3_resume_early(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops lg1k_usb3_pm_ops = {
	.resume = lg1k_usb3_resume,
	SET_LATE_SYSTEM_SLEEP_PM_OPS(lg1k_usb3_suspend_late,
						lg1k_usb3_resume_early)
};
#define DEV_PM_OPS	(&lg1k_usb3_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif /* CONFIG_PM_SLEEP */

static int lg1k_usb3_platform_probe(struct platform_device *pdev)
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
	lgphy->init = lg1k_usb3_init;

	if (dev->of_node) {
		ret = lg1k_usbphy_parse_dt(lgphy);
		if (ret < 0) {
			dev_err(dev, "device tree parsing fail!\n");
			return ret;
		}
	}

	platform_set_drvdata(pdev, lgphy);
	device_enable_async_suspend(dev);

	return lg1k_usb_phy_add(&lgphy->phy, dev, "lg1k-usb3phy",
			USB_PHY_TYPE_USB3);
}

static int lg1k_usb3_platform_remove(struct platform_device *pdev)
{
	struct lg1k_usbphy *lgphy = platform_get_drvdata(pdev);

	dev_info(lgphy->dev, "remove phy\n");

	lg1k_usbphy_remove(lgphy);

	return 0;
}

static struct platform_device_id lg1k_usb3_platform_ids[] = {
	{ .name = "lg1k-usb3phy", },
	{},
};
MODULE_DEVICE_TABLE(platform, lg1k_usb3_platform_ids);

static struct platform_driver lg1k_usb3_platform_driver = {
	.probe		= lg1k_usb3_platform_probe,
	.remove		= lg1k_usb3_platform_remove,
	.id_table	= lg1k_usb3_platform_ids,
	.driver	= {
		.name = "lg1k-usb3phy",
		.pm = DEV_PM_OPS,
	},
};
module_platform_driver(lg1k_usb3_platform_driver);

MODULE_DESCRIPTION("LG DTV USB 3.0 phy controller");
MODULE_AUTHOR("Shinhoe Kim <shinhoe.kim@lge.com>, Daewoong Kim <daewoong00.kim@lge.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lg1k-usb3phy");
