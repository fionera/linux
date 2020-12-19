#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/ioport.h>

static struct resource kdrv_buf[] = {
	[0] = { .name = "kdrv_buffer1", },
	[1] = { .name = "kdrv_buffer2", },
};

int arch_pfn_is_nosave(unsigned long pfn)
{
	unsigned int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(kdrv_buf); i++)
		ret |= (pfn >= kdrv_buf[i].start) && (pfn < kdrv_buf[i].end);

	return ret;
}

static int __init setup_arch_nosave_region(void)
{
	struct device_node *np;
	struct resource r;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(kdrv_buf); i++) {
		np = of_find_node_by_name(NULL, kdrv_buf[i].name);
		if (!np) {
			pr_err("Failed to find kdrv_buffer dt node\n");
			return -EINVAL;
		}

		if (of_address_to_resource(np, 0, &r)) {
			pr_err("Failed to read address to resource\n");
			return -EINVAL;
		}

		kdrv_buf[i].start = __phys_to_pfn(r.start);
		kdrv_buf[i].end = __phys_to_pfn(r.end + 1);

		pr_debug("arch_nosave_region[%d] %pa-%pa\n",
				i, &kdrv_buf[i].start, &kdrv_buf[i].end);
	}

	return 0;
}
late_initcall(setup_arch_nosave_region);
