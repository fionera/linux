#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/amba/pl08x.h>
#include <linux/amba/pl080.h>

extern struct pl08x_platform_data dmac_data;
extern struct pl022_ssp_controller spi0_data;
extern struct pl022_ssp_controller spi1_data;

static struct of_dev_auxdata lg115x_auxdata_lookup[] __initdata = {
#ifdef CONFIG_AMBA_PL08X
	OF_DEV_AUXDATA("arm,pl080", 0xff200000, "dmac", &dmac_data),
#endif
#ifdef CONFIG_SPI_PL022
	OF_DEV_AUXDATA("arm,pl022", 0xfe800000, "spi0", &spi0_data),
	OF_DEV_AUXDATA("arm,pl022", 0xfe900000, "spi1", &spi1_data),
#endif
	{ },
};

static int lg1k_platform_notify(struct device *dev)
{
#ifdef CONFIG_PM_SLEEP
	if(dev->parent && dev->parent->power.is_userresume == true)
		dev->power.is_userresume = true;

	return 0;
#endif  /* CONFIG_PM_SLEEP */
}

void lg1k_platform_notify_register(void)
{
	if (platform_notify) {
		pr_warn("platform_notify is already registered: %pF at %p\n",
				platform_notify, platform_notify);
		return;
	}

	platform_notify = lg1k_platform_notify;
}

static int __init lg115x_init_machine(void)
{
	of_platform_populate(NULL, of_default_bus_match_table,
				lg115x_auxdata_lookup, NULL);
	lg1k_platform_notify_register();

	return 0;
}
arch_initcall(lg115x_init_machine);

