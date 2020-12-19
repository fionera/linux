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

static LIST_HEAD(zacc_enc_list);
static spinlock_t zacc_enc_lock;

int zacc_encoder_ready(void)
{
	return !list_empty(&zacc_enc_list);
}

static struct zacc_dev *zacc_encoder(void)
{
	return __zacc_device(&zacc_enc_list, &zacc_enc_lock);
}

ssize_t zacc_encode(void *dst, void const *src)
{
	struct zacc_dev *zdev;
	struct device *dev;
	struct page *dst_pg, *src_pg;
	dma_addr_t dst_pa, src_pa;
	int err;

	if (!dst || !src)
		return -EINVAL;

	zdev = zacc_encoder();
	if (!zdev)
		return -ENODEV;
	dev = &zdev->pdev->dev;

	spin_lock(&zdev->lock);

	/* ensure device is idle */
	while (readl_relaxed(zdev->base + ZACC_SS) & 0x0001)
		cpu_relax();

	dst_pg = zdev->dst_pg;
	src_pg = zdev->src_pg;

	memcpy(page_address(src_pg), src, PAGE_SIZE);

	dst_pa = dma_map_page(dev, dst_pg, 0, PAGE_SIZE, DMA_FROM_DEVICE);
	src_pa = dma_map_page(dev, src_pg, 0, PAGE_SIZE, DMA_TO_DEVICE);

	writel_relaxed(src_pa, zdev->base + ZACC_SSA);
	writel_relaxed(dst_pa, zdev->base + ZACC_SDA);
	writel_relaxed(PAGE_SIZE, zdev->base + ZACC_SSL);
	writel_relaxed(PAGE_SIZE, zdev->base + ZACC_SDL);

	/* trigger starting SDMA */
	writel_relaxed(0x0001, zdev->base + ZACC_SC);

	while (!readl_relaxed(zdev->base + ZACC_MIS))
		cpu_relax();

	/* clear pending interrupt(s) */
	writel_relaxed(0xffff, zdev->base + ZACC_MIS);

	dma_unmap_page(dev, dst_pa, PAGE_SIZE, DMA_FROM_DEVICE);
	dma_unmap_page(dev, src_pa, PAGE_SIZE, DMA_TO_DEVICE);

	if (readl_relaxed(zdev->base + ZACC_SS) & 0x0002) {
		dev_dbg(dev, "abnormal termination\n");
		err = -EIO;
	} else {
		err = readl_relaxed(zdev->base + ZACC_STWL);
		memcpy(dst, page_address(dst_pg), err);
	}

	spin_unlock(&zdev->lock);

	return err;
}
EXPORT_SYMBOL(zacc_encode);

static int zacc_probe(struct platform_device *pdev)
{
	struct zacc_dev *zdev;
	unsigned long flags;

	zdev = __zacc_probe(pdev);
	if (!zdev)
		return -ENODEV;

	spin_lock_irqsave(&zacc_enc_lock, flags);
	list_add_tail(&zdev->list, &zacc_enc_list);
	spin_unlock_irqrestore(&zacc_enc_lock, flags);

	pr_info("zacc_enc: %s initialized\n", pdev->name);

	return 0;
}

static int zacc_remove(struct platform_device *pdev)
{
	struct zacc_dev *zdev = platform_get_drvdata(pdev);
	unsigned long flags;

	spin_lock_irqsave(&zacc_enc_lock, flags);
	list_del(&zdev->list);
	spin_unlock_irqrestore(&zacc_enc_lock, flags);

	__zacc_remove(pdev);

	return 0;
}

#ifdef CONFIG_OF

static struct of_device_id const zacc_of_match[] = {
	{ .compatible = "lge,lg1k-zacc-enc", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, zacc_of_match);

#endif	/* CONFIG_OF */

static struct platform_driver zacc_enc_drv = {
	.probe	= zacc_probe,
	.remove	= zacc_remove,
	.driver	= {
		.name	= "zacc-enc",
		.owner	= THIS_MODULE,
		.pm	= &zacc_pm_ops,
		.of_match_table	= of_match_ptr(zacc_of_match),
	},
};
module_platform_driver(zacc_enc_drv);

MODULE_AUTHOR("Jongsung Kim <neidhard.kim@lge.com>");
MODULE_DESCRIPTION("LG1K ZACC HW compressor support");
MODULE_LICENSE("GPL");
