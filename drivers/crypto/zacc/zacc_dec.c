#include <linux/module.h>

#include <linux/kernel.h>

#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/zacc.h>

#include "zacc.h"

static LIST_HEAD(zacc_dec_list);
static spinlock_t zacc_dec_lock;

int zacc_decoder_ready(void)
{
	return !list_empty(&zacc_dec_list);
}

static struct zacc_dev *zacc_decoder(void)
{
	return __zacc_device(&zacc_dec_list, &zacc_dec_lock);
}

ssize_t zacc_decode(void *dst, void const *src, size_t size)
{
	struct zacc_dev *zdev;
	struct device *dev;
	struct page *dst_pg, *src_pg;
	dma_addr_t dst_pa, src_pa;
	int err;

	if (!dst || !src)
		return -EINVAL;

	zdev = zacc_decoder();
	if (!zdev)
		return -ENODEV;
	dev = &zdev->pdev->dev;

	spin_lock(&zdev->lock);

	/* ensure device is idle */
	while (readl_relaxed(zdev->base + ZACC_SS) & 0x0001)
		cpu_relax();

	dst_pg = zdev->dst_pg;
	src_pg = zdev->src_pg;

	memcpy(page_address(src_pg), src, size);

	dst_pa = dma_map_page(dev, dst_pg, 0, PAGE_SIZE, DMA_FROM_DEVICE);
	src_pa = dma_map_page(dev, src_pg, 0, size, DMA_TO_DEVICE);

	writel_relaxed(src_pa, zdev->base + ZACC_SSA);
	writel_relaxed(dst_pa, zdev->base + ZACC_SDA);
	writel_relaxed(size, zdev->base + ZACC_SSL);
	writel_relaxed(PAGE_SIZE, zdev->base + ZACC_SDL);

	/* trigger starting SDMA */
	writel_relaxed(0x0001, zdev->base + ZACC_SC);

	while (!readl_relaxed(zdev->base + ZACC_MIS))
		cpu_relax();

	/* clear pending interrupt(s) */
	writel_relaxed(0xffff, zdev->base + ZACC_MIS);

	dma_unmap_page(dev, dst_pa, PAGE_SIZE, DMA_FROM_DEVICE);
	dma_unmap_page(dev, src_pa, size, DMA_TO_DEVICE);

	if (readl_relaxed(zdev->base + ZACC_SS) & 0x0002) {
		dev_dbg(dev, "abnormal termination\n");
		err = -EIO;
	} else {
		err = readl_relaxed(zdev->base + ZACC_STWL);
		memcpy(dst, page_address(dst_pg), err);
	}

	spin_unlock(&zdev->lock);

	if (err < 0) {
		/* hex-dump source compressed data as debug information */
		print_hex_dump(KERN_ERR, "input data: ", DUMP_PREFIX_ADDRESS,
		               16, 1, src, size, true);
	}

	return err;
}
EXPORT_SYMBOL(zacc_decode);

static int zacc_probe(struct platform_device *pdev)
{
	struct zacc_dev *zdev;
	unsigned long flags;

	zdev = __zacc_probe(pdev);
	if (!zdev)
		return -ENODEV;

	spin_lock_irqsave(&zacc_dec_lock, flags);
	list_add_tail(&zdev->list, &zacc_dec_list);
	spin_unlock_irqrestore(&zacc_dec_lock, flags);

	pr_info("zacc_dec: %s initialized\n", pdev->name);

	return 0;
}

static int zacc_remove(struct platform_device *pdev)
{
	struct zacc_dev *zdev = platform_get_drvdata(pdev);
	unsigned long flags;

	spin_lock_irqsave(&zacc_dec_lock, flags);
	list_del(&zdev->list);
	spin_unlock_irqrestore(&zacc_dec_lock, flags);

	__zacc_remove(pdev);

	return 0;
}

#ifdef CONFIG_OF

static struct of_device_id const zacc_of_match[] = {
	{ .compatible = "lge,lg1k-zacc-dec", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, zacc_of_match);

#endif	/* CONFIG_OF */

static struct platform_driver zacc_dec_drv = {
	.probe	= zacc_probe,
	.remove	= zacc_remove,
	.driver	= {
		.name	= "zacc-dec",
		.owner	= THIS_MODULE,
		.pm	= &zacc_pm_ops,
		.of_match_table	= of_match_ptr(zacc_of_match),
	},
};
module_platform_driver(zacc_dec_drv);

MODULE_AUTHOR("Jongsung Kim <neidhard.kim@lge.com>");
MODULE_DESCRIPTION("LG1K ZACC HW decompressor support");
MODULE_LICENSE("GPL");
