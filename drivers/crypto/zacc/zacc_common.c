#include <linux/kernel.h>

#include <linux/dma-mapping.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include "zacc.h"

int zacc_ready(void)
{
	return zacc_decoder_ready() && zacc_encoder_ready();
}
EXPORT_SYMBOL(zacc_ready);

struct zacc_dev *__zacc_device(struct list_head *list, spinlock_t *lock)
{
	struct zacc_dev *zdev;
	unsigned long flags;

	if (list_empty(list))
		return NULL;

	spin_lock_irqsave(lock, flags);

	zdev = list_first_entry(list, struct zacc_dev, list);
	if (!list_is_singular(list))
		list_rotate_left(list);

	spin_unlock_irqrestore(lock, flags);

	return zdev;
}

static void zacc_init(struct zacc_dev *zdev)
{
	/* enable complete interrupt only */
	writel_relaxed(0xfffe, zdev->base + ZACC_IM);
	/* clear interrupt(s) */
	writel_relaxed(0xffff, zdev->base + ZACC_MIS);
	/* select SDMA */
	writel_relaxed(0x0001, zdev->base + ZACC_DC);
	/* enable core */
	writel_relaxed(0x0001, zdev->base + ZACC_CC);
}

static void zacc_stop(struct zacc_dev *zdev)
{
	/* disable SDMA */
	writel_relaxed(0x0000, zdev->base + ZACC_SC);
}

struct zacc_dev *__zacc_probe(struct platform_device *pdev)
{
	struct zacc_dev *zdev;
	struct resource *res;

	zdev = devm_kzalloc(&pdev->dev, sizeof(struct zacc_dev), GFP_KERNEL);
	if (!zdev)
		return NULL;

	platform_set_drvdata(pdev, zdev);
	zdev->pdev = pdev;

	INIT_LIST_HEAD(&zdev->list);
	spin_lock_init(&zdev->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return NULL;

	zdev->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(zdev->base))
		return NULL;

	zdev->dst_pg = alloc_page(GFP_DMA32);
	if (!zdev->dst_pg)
		goto __failed;

	zdev->src_pg = alloc_page(GFP_DMA32);
	if (!zdev->src_pg)
		goto __failed;

	zacc_init(zdev);

	return zdev;
__failed:
	if (zdev->dst_pg)
		__free_page(zdev->dst_pg);
	if (zdev->src_pg)
		__free_page(zdev->src_pg);

	return NULL;
}

void __zacc_remove(struct platform_device *pdev)
{
	struct zacc_dev *zdev = platform_get_drvdata(pdev);

	zacc_stop(zdev);

	__free_page(zdev->dst_pg);
	__free_page(zdev->src_pg);
}

int zacc_suspend(struct device *dev)
{
	struct zacc_dev *zdev = dev_get_drvdata(dev);
	zacc_stop(zdev);
	return 0;
}

int zacc_resume(struct device *dev)
{
	struct zacc_dev *zdev = dev_get_drvdata(dev);
	zacc_init(zdev);
	return 0;
}

SIMPLE_DEV_PM_OPS(zacc_pm_ops, zacc_suspend, zacc_resume);
