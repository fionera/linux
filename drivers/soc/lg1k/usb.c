#include <linux/kernel.h>

#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/usb/ehci_pdriver.h>
#include <linux/usb/ohci_pdriver.h>
#include <linux/usb/phy.h>
#include <linux/usb/hcd.h>
#include "../../usb/core/usb.h"

struct usb_host_data {
	char const *name;
	char const *phy_name;
	int *id;
	void *pdata;
	bool is_userresume;
	bool is_userresume_phy;
};

static int xhci_id;
static int ehci_id;
static int ohci_id;
static int drd2_id;

static struct usb_ehci_pdata ehci_pdata = {
	.has_synopsys_hc_bug = 1,
	.reset_on_resume = 1,
};

static struct usb_ohci_pdata ohci_pdata = {
	.num_ports = 1,
};

static struct usb_host_data xhci_data = {
	.name	= "xhci-hcd",
	.phy_name	= "lg1k-usb3phy",
	.id	= &xhci_id,
	.is_userresume = true,
	.is_userresume_phy = false,
};

static struct usb_host_data ehci_data = {
	.name	= "generic-ehci",
	.phy_name	= "lg1k-usb2phy",
	.id	= &ehci_id,
	.pdata	= &ehci_pdata,
	.is_userresume = true,
	.is_userresume_phy = false,
};

static struct usb_host_data drd2_data = {
	.name	= "snps,dwc2",
	.phy_name	= "lg1k-drd2phy",
	.id	= &drd2_id,
	.is_userresume = true,
	.is_userresume_phy = false,
};

static struct usb_host_data ohci_data = {
	.name	= "generic-ohci",
	.phy_name	= "lg1k-usb1phy",
	.id	= &ohci_id,
	.pdata	= &ohci_pdata,
	.is_userresume = true,
	.is_userresume_phy = false,
};

static struct of_device_id const usb_host_ids[] = {
	{ .compatible = "lge,lg115x-xhci", .data = &xhci_data, },
	{ .compatible = "lge,lg115x-ehci", .data = &ehci_data, },
	{ .compatible = "lge,lg115x-ohci", .data = &ohci_data, },
	{ .compatible = "lge,lg115x-drd2", .data = &drd2_data, },
	{ },
};

#ifdef CONFIG_DPM_WATCHDOG
extern int (*dpm_watchdog_timeout_notify)(struct device *dev);

struct usb_dpm_timeout_work {
	struct usb_device       *udev;
	struct work_struct      work;
};

static void usb_phy_vbus_off_on(struct usb_phy *phy)
{
	if (!IS_ENABLED(CONFIG_USB_PHY) || phy == NULL || phy->dev == NULL)
		return;

	usb_phy_vbus_off(phy);
	msleep(500);
	usb_phy_vbus_on(phy);
}

static void usb_remove_and_vbus_off_on_work(struct work_struct *work)
{
	struct usb_dpm_timeout_work *dpm_work =
		container_of(work, struct usb_dpm_timeout_work, work);
	struct usb_device *udev = dpm_work->udev;
	struct usb_device *hdev;
	struct usb_hcd *hcd = bus_to_hcd(udev->bus);

	kfree(dpm_work);

	dev_dbg(&udev->dev, "finding ancestor attached directly to root hub\n");
	for ( ; udev; udev = udev->parent)
		if (udev->level == 1)
			break;

	if (!udev)
		udev = dpm_work->udev;

	/* sanity check */
	if (udev->level != 1 || !udev->parent || udev->parent->parent)
		dev_warn(&udev->dev, "not attached directly to root hub\n");

	hdev = udev->parent;

	dev_info(&udev->dev, "removing device and its descendants\n");
	usb_remove_device(udev);

	dev_info(&hdev->dev, "switch vbus off and on\n");
	usb_phy_vbus_off_on(hcd->usb_phy);
}

static int usb_notify_dpm_watchdog_timeout(struct device *dev)
{
	struct usb_dpm_timeout_work *dpm_work;
	struct usb_device *udev;

	if (!is_usb_device(dev))
		return -1;

	udev = to_usb_device(dev);
	if (!udev->parent) /* root hub device? */
		return 0;

	dpm_work = kmalloc(sizeof(*dpm_work), GFP_KERNEL);
	if (!dpm_work) {
		dev_info(dev, "couldn't kmalloc usb dpm timout work struct\n");
		return -ENOMEM;
	}
	dpm_work->udev = udev;

	INIT_WORK(&dpm_work->work, usb_remove_and_vbus_off_on_work);
	queue_work(pm_wq, &dpm_work->work);

	return 0;
}

void lg1k_dpm_watchdog_timeout_notify_register(void)
{
	if (dpm_watchdog_timeout_notify) {
		pr_warn("dpm_watchdog_timeout_notify is already registered:"
				"%pF at %p\n", dpm_watchdog_timeout_notify,
				dpm_watchdog_timeout_notify);
		return;
	}

	dpm_watchdog_timeout_notify = usb_notify_dpm_watchdog_timeout;
}
#else
void lg1k_dpm_watchdog_timeout_notify_register(void) { }
#endif /* CONFIG_DPM_WATCHDOG */

static int lg115x_init_usb(void)
{
	struct device_node *np;
	static int initialized;

	if (initialized) {
		pr_err("lg115x usb host/phy devices are already registered\n");
		return 0;
	}

	for_each_matching_node(np, usb_host_ids) {
		struct usb_host_data const *data;
		struct resource *res;
		struct platform_device *pdev = NULL;
		struct device_node *phy_np = NULL;

		data = of_match_node(usb_host_ids, np)->data;

		res = kzalloc(sizeof(struct resource) * 2, GFP_KERNEL);
		of_address_to_resource(np, 0, &res[0]);
		of_irq_to_resource(np, 0, &res[1]);

		pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
		pdev->name = data->name;
		pdev->id = (*data->id)++;
		pdev->dev.of_node = np;
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
		pdev->dev.platform_data = data->pdata;
#ifdef CONFIG_PM_SLEEP
		pdev->dev.power.is_userresume = data->is_userresume;
#endif	/* CONFIG_PM_SLEEP */
		of_dma_configure(&pdev->dev, pdev->dev.of_node);
		pdev->num_resources = 2;
		pdev->resource = res;

		phy_np = of_get_next_child(np, NULL);
		if(phy_np) {
			struct platform_device *phy_pdev = NULL;
			char *pdev_name, *phy_pdev_name;

			pdev_name = kzalloc(32, GFP_KERNEL);
			phy_pdev_name = kzalloc(32, GFP_KERNEL);
			phy_pdev = kzalloc(sizeof(struct platform_device),
				GFP_KERNEL);
			phy_pdev->name = data->phy_name;
			phy_pdev->id = pdev->id;
			phy_pdev->dev.of_node = phy_np;
#ifdef CONFIG_PM_SLEEP
			phy_pdev->dev.power.is_userresume = data->is_userresume_phy;
#endif	/* CONFIG_PM_SLEEP */
			of_dma_configure(&phy_pdev->dev, phy_pdev->dev.of_node);
			/*
			 * get device names and bind in advance before devices
			 * are registered. In some initcall sequence,controller
			 * must be probed after bind phy device.
			 */
			sprintf(pdev_name, "%s.%d", pdev->name, pdev->id);
			sprintf(phy_pdev_name, "%s.%d", phy_pdev->name,
					phy_pdev->id);
			usb_bind_phy(pdev_name, 0, phy_pdev_name);
			platform_device_register(phy_pdev);
		}

		platform_device_register(pdev);
	}

	lg1k_dpm_watchdog_timeout_notify_register();

	initialized++;
	return 0;
}
#ifndef	CONFIG_USER_INITCALL_USB
device_initcall(lg115x_init_usb);
#else	/* CONFIG_USER_INITCALL_USB */
user_initcall_grp("USB", lg115x_init_usb);
#endif	/* CONFIG_USER_INITCALL_USB */
