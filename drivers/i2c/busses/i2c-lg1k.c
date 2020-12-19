#include <linux/module.h>

#include <linux/kernel.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define I2C_MDR		0x0000	/* Master Data Register */
#define I2C_SDR		0x0004	/* Slave Data Register */
#define I2C_SAR		0x0008	/* Slave Address Register */
#define I2C_CTR0	0x000c	/* Control Register 0 */
#define I2C_CTR1	0x0010	/* Control Register 1 */
#define I2C_MSR		0x0014	/* Master Status Register */
#define I2C_SSR		0x0018	/* Slave Status Register */
#define I2C_CPHR	0x0020	/* Clock Prescaler High Register */
#define I2C_CPLR	0x0024	/* Clock Prescaler Low Register */

#define I2C_CTR0_TGO	0x0001
#define I2C_CTR0_IACK	0x0002
#define I2C_CTR0_NTB(v)	(((v) - 1) << 3)
#define I2C_CTR0_RSTA	0x0020
#define I2C_CTR0_STO	0x0040
#define I2C_CTR0_STA	0x0080

#define I2C_CTR1_SWR	0x0002
#define I2C_CTR1_CSF	0x0004
#define I2C_CTR1_CMF	0x0008
#define I2C_CTR1_IEN	0x0010
#define I2C_CTR1_CEN	0x0020
#define I2C_CTR1_TMS_TX	0x0000
#define I2C_CTR1_TMS_RX	0x0040

#define I2C_MSR_MTB	0x0001
#define I2C_MSR_SNB	0x0002
#define I2C_MSR_MIS	0x0004

#define I2C_FIFO_SIZE	4

#define I2C_XFER_TX	0x0001
#define I2C_XFER_RX	0x0002
#define I2C_XFER_STA	0x0010
#define I2C_XFER_RSTA	0x0020
#define I2C_XFER_STO	0x0040
#define I2C_XFER_POLL	0x0080

struct lg1k_i2c_priv {
	struct i2c_adapter adap;
	struct platform_device *pdev;

	void __iomem *base;
	int irq;

	struct clk *clk;
	u32 rate;

	struct completion completion;
};

#define adap_to_priv(a)	container_of(a, struct lg1k_i2c_priv, adap)

struct lg1k_i2c_req {
	u8 *buf;
	u8 len;
	u16 flags;
	u8 addr;
	u32 rate;
};

static int lg1k_i2c_init(struct i2c_adapter *adap)
{
	struct lg1k_i2c_priv *priv = adap_to_priv(adap);

	/* core reset */
	writeb_relaxed(I2C_CTR1_SWR, priv->base + I2C_CTR1);
	mdelay(1);
	writeb_relaxed(0x00, priv->base + I2C_CTR1);

	return 0;
}

static int lg1k_i2c_stop(struct i2c_adapter *adap)
{
	struct lg1k_i2c_priv *priv = adap_to_priv(adap);
	writeb_relaxed(0x00, priv->base + I2C_CTR1);
	return 0;
}

#define I2C_CTR1_INIT	(I2C_CTR1_CMF | I2C_CTR1_IEN | I2C_CTR1_CEN)
#define I2C_CTR1_TX	(I2C_CTR1_INIT | I2C_CTR1_TMS_TX)
#define I2C_CTR1_RX	(I2C_CTR1_INIT | I2C_CTR1_TMS_RX)

static int lg1k_i2c_xfer(struct i2c_adapter *adap, struct lg1k_i2c_req *req)
{
	struct lg1k_i2c_priv *priv = adap_to_priv(adap);
	struct platform_device *pdev = priv->pdev;
	unsigned long timeout;
	u8 v;
	int i, r;

	if (req->flags & I2C_XFER_POLL)
		disable_irq(priv->irq);

	/* CTR1 */
	v = I2C_CTR1_CMF | I2C_CTR1_IEN | I2C_CTR1_CEN;
	if (req->flags & I2C_XFER_RX)
		v |= I2C_CTR1_TMS_RX;
	if (req->flags & I2C_XFER_TX)
		v |= I2C_CTR1_TMS_TX;
	writeb_relaxed(v, priv->base + I2C_CTR1);

	/* TX - write data info MDR */
	if (req->flags & I2C_XFER_TX) {
		for (i = 0; i < req->len; i++)
			writeb_relaxed(req->buf[i], priv->base + I2C_MDR);
	}

	/* CTR0 */
	v = I2C_CTR0_TGO | I2C_CTR0_NTB(req->len);
	if (req->flags & I2C_XFER_STA)
		v |= I2C_CTR0_STA;
	if (req->flags & I2C_XFER_RSTA)
		v |= I2C_CTR0_RSTA;
	if (req->flags & I2C_XFER_STO)
		v |= I2C_CTR0_STO;
	writeb_relaxed(v, priv->base + I2C_CTR0);

	if (req->flags & I2C_XFER_POLL) {
		/* busy-wait for completion */
		timeout = jiffies + adap->timeout;
		do {
			if (time_after_eq(jiffies, timeout)) {
				dev_warn(&pdev->dev, "timed-out - reset\n");
				lg1k_i2c_init(adap);
				enable_irq(priv->irq);
				return -ETIMEDOUT;
			}

			v = readb_relaxed(priv->base + I2C_MSR);
		} while (!(v & I2C_MSR_MIS));

		/* clear interrupt(s) */
		writeb_relaxed(I2C_CTR0_IACK, priv->base + I2C_CTR0);

		enable_irq(priv->irq);
	} else {
		r = wait_for_completion_timeout(&priv->completion, adap->timeout);
		if (!r) {
			dev_warn(&pdev->dev, "timed-out - reset\n");
			lg1k_i2c_init(adap);
			return -ETIMEDOUT;
		}
	}

	/* RX - read data from MDR */
	if (req->flags & I2C_XFER_RX) {
		for (i = 0; i < req->len; i++)
			req->buf[i] = readb_relaxed(priv->base + I2C_MDR);
	}

	if (req->flags & I2C_XFER_TX) {
		v = readb_relaxed(priv->base + I2C_MSR);
		if (v & I2C_MSR_SNB) {
			dev_dbg(&pdev->dev, "slave-NAK\n");
			return -EIO;
		}
	}

	return 0;
}

/* private mode bits to control bus clock freq. */
#define I2C_M_CLK_DEAFULT	0x0000
#define I2C_M_CLK__50KHZ	0x0002
#define I2C_M_CLK_100KHZ	0x0004
#define I2C_M_CLK_400KHZ	0x0006
#define I2C_M_CLK_800KHZ	0x0008
#define I2C_M_CLK_MASK		0x000e
/* private mode bit to enforce busy-wait mode */
#define I2C_M_POLL		0x0100

static void lg1k_i2c_rate(struct i2c_adapter *adap, u32 rate)
{
	struct lg1k_i2c_priv *priv = adap_to_priv(adap);
	u16 div;

	/*
	 * Measured bus clock-freq. is slightly higher than intended
	 * speed. To avoid over-clocked bus driving, compensate bit-
	 * rate by -10% for standard-mode, or by -5% for fast-mode.
	 */
	if (rate > 100000)
		rate -= rate / 20;
	else
		rate -= rate / 10;

	div = (clk_get_rate(priv->clk) + (rate * 10) - 1) / (rate * 10);
	writeb_relaxed(div >> 8, priv->base + I2C_CPHR);
	writeb_relaxed(div & 0xff, priv->base + I2C_CPLR);
}

static int
lg1k_i2c_master_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	struct lg1k_i2c_priv *priv = adap_to_priv(adap);
	struct lg1k_i2c_req *req;
	int i, j, r, x;

	/* count required low-level transfers */
	for (i = x = 0; i < num; i++)
		x += (msgs[i].len + I2C_FIFO_SIZE - 1) / I2C_FIFO_SIZE + 1;

	req = kzalloc(sizeof(struct lg1k_i2c_req) * x, GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	for (i = x = 0; i < num; i++) {
		/* address phase */
		req[x].addr = (u8)(msgs[i].addr << 1);
		req[x].addr |= msgs[i].flags & I2C_M_RD;
		req[x].buf = &req[x].addr;
		req[x].len = 1;
		switch (msgs[i].flags & I2C_M_CLK_MASK) {
		case I2C_M_CLK__50KHZ:
			req[x].rate = 50000;
			break;
		case I2C_M_CLK_100KHZ:
			req[x].rate = 100000;
			break;
		case I2C_M_CLK_400KHZ:
			req[x].rate = 400000;
			break;
		case I2C_M_CLK_800KHZ:
			req[x].rate = 800000;
			break;
		default:
			req[x].rate = priv->rate;
			break;
		}
		req[x].flags = I2C_XFER_TX;
		req[x++].flags |= msgs[i].flags & I2C_M_POLL ?
			I2C_XFER_POLL : 0x0;

		/* data phase */
		for (j = 0; j < msgs[i].len; j += I2C_FIFO_SIZE, x++) {
			req[x].buf = &msgs[i].buf[j];
			req[x].len = min_t(u16, msgs[i].len - j, I2C_FIFO_SIZE);
			if (msgs[i].flags & I2C_M_RD)
				req[x].flags = I2C_XFER_RX;
			else
				req[x].flags = I2C_XFER_TX;
			req[x].flags |= msgs[i].flags & I2C_M_POLL ?
				I2C_XFER_POLL : 0x0;

			if (j + req[x].len == msgs[i].len)
				req[x].flags |= I2C_XFER_RSTA;
		}
	}

	/* mark start-/stop-condition */
	req[0].flags |= I2C_XFER_STA;
	req[x - 1].flags |= I2C_XFER_STO;
	req[x - 1].flags &= ~I2C_XFER_RSTA;

	/* NOTE: Only I2C_XFER_POLL bit in the first msg will be respected. */
	if (req[0].flags & I2C_XFER_POLL)
		preempt_disable();

	for (i = 0; i < x; i++) {
		/* control bus-speed if specified */
		if (req[i].rate)
			lg1k_i2c_rate(adap, req[i].rate);

		r = lg1k_i2c_xfer(adap, &req[i]);
		if (r < 0) {
			num = r;
			break;
		}
	}

	if (req[0].flags & I2C_XFER_POLL)
		preempt_enable();

	kfree(req);

	return num;
}

static u32 lg1k_i2c_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C;
}

static struct i2c_algorithm lg1k_i2c_algorithm = {
	.master_xfer	= lg1k_i2c_master_xfer,
	.functionality	= lg1k_i2c_functionality,
};

static irqreturn_t lg1k_i2c_isr(int irq, void *dev_id)
{
	struct lg1k_i2c_priv *priv = dev_id;
	u8 v;

	if (irq != priv->irq)
		return IRQ_NONE;

	v = readb_relaxed(priv->base + I2C_MSR);
	if (!(v & I2C_MSR_MIS))
		return IRQ_NONE;

	/* clear interrupt(s) */
	writel_relaxed(I2C_CTR0_IACK, priv->base + I2C_CTR0);

	/* wake up task */
	complete(&priv->completion);

	return IRQ_HANDLED;
}

static int lg1k_i2c_probe(struct platform_device *pdev)
{
	struct lg1k_i2c_priv *priv;
	int r;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		r = -ENOMEM;
		goto __return__;
	}

	priv->base = of_iomap(pdev->dev.of_node, 0);
	if (!priv->base) {
		r = -EIO;
		goto __kfree__;
	}

	priv->irq = of_irq_get(pdev->dev.of_node, 0);
	if (priv->irq < 0) {
		r = priv->irq;
		goto __iounmap__;
	}

	priv->clk = clk_get(&pdev->dev, "apb_pclk");
	if (IS_ERR(priv->clk)) {
		r = PTR_ERR(priv->clk);
		goto __iounmap__;
	}

	priv->pdev = pdev;
	priv->rate = 100000;	/* standard-mode if not specified */
	init_completion(&priv->completion);

	of_property_read_u32(pdev->dev.of_node, "clock-frequency", &priv->rate);

	priv->adap.owner = THIS_MODULE;
	priv->adap.class = I2C_CLASS_DEPRECATED;
	priv->adap.algo = &lg1k_i2c_algorithm;
	priv->adap.dev.parent = &pdev->dev;
	priv->adap.dev.of_node = pdev->dev.of_node;
	priv->adap.nr = pdev->id;
	strcpy(priv->adap.name, dev_name(&pdev->dev));
	init_completion(&priv->adap.dev_released);

	platform_set_drvdata(pdev, priv);

	lg1k_i2c_init(&priv->adap);

	r = request_irq(priv->irq, lg1k_i2c_isr, 0, dev_name(&pdev->dev), priv);
	if (r < 0)
		goto __iounmap__;

	r = i2c_add_numbered_adapter(&priv->adap);
	if (r < 0)
		goto __free_irq__;

	dev_info(&pdev->dev, "lg1k-i2c bus-adapter\n");

	return 0;

__free_irq__:
	free_irq(priv->irq, priv);
__iounmap__:
	iounmap(priv->base);
__kfree__:
	kfree(priv);
__return__:
	return r;
}

static int lg1k_i2c_remove(struct platform_device *pdev)
{
	struct lg1k_i2c_priv *priv = platform_get_drvdata(pdev);
	struct i2c_adapter *adap = &priv->adap;
	lg1k_i2c_stop(adap);
	i2c_del_adapter(adap);
	free_irq(priv->irq, priv);
	iounmap(priv->base);
	kfree(priv);
	return 0;
}

static int lg1k_i2c_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct i2c_adapter *adap = platform_get_drvdata(pdev);
	return lg1k_i2c_stop(adap);
}

static int lg1k_i2c_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct i2c_adapter *adap = platform_get_drvdata(pdev);
	return lg1k_i2c_init(adap);
}

static struct dev_pm_ops const lg1k_i2c_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(lg1k_i2c_suspend, lg1k_i2c_resume)
};

#ifdef CONFIG_OF

static struct of_device_id const lg1k_i2c_of_match[] = {
	{ .compatible = "lge,lg1k-i2c", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, lg1k_i2c_of_match);

#endif	/* CONFIG_OF */

static struct platform_driver lg1k_i2c_driver = {
	.probe	= lg1k_i2c_probe,
	.remove	= lg1k_i2c_remove,
	.driver	= {
		.name	= "lg1k-i2c",
		.owner	= THIS_MODULE,
		.pm	= &lg1k_i2c_pm_ops,
		.of_match_table	= of_match_ptr(lg1k_i2c_of_match),
	},
};

module_platform_driver(lg1k_i2c_driver);

MODULE_AUTHOR("Jongsung Kim <neidhard.kim@lge.com>");
MODULE_DESCRIPTION("I2C bus adapter integrated in LG1K DTV SoCs");
MODULE_LICENSE("GPL");
