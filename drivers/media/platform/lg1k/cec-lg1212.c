#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/cec.h>
#include <media/cec.h>

#define LG1K_CEC_NAME	"lg1k-cec"
#define CEC_VERSION_1_4	5
#define CEC_VENDOR_ID	0x00e091 /* need to check lge vendor id */

/* CEC register offset */
#define CEC_RX_LENGTH			0x030
#define CEC_RX_DONE			0x031
#define CEC_TX_DONE			0x032
#define CEC_RX_DATA_BASE		0x034
#define CEC_NOTI_FROM_MiCOM		0x088
#define CEC_TX_LENGTH			0x130
#define CEC_TX_DATA_BASE		0x134
#define CEC_NOTI_FROM_ARM		0x188


struct lg1k_cec {
	struct cec_adapter	*adap;
	struct device		*dev;
	struct clk		*clk;
	void __iomem		*base;
	int			irq;
	u32			irq_status;
	u32			len;
	struct cec_notifier	*notifier;
	bool			is_enabled;
};

static inline u8 cec_read(struct lg1k_cec *cec, u32 reg)
{
	u32 val = readl(cec->base + (reg & 0x1fc));
	u32 offset = reg % 4;

	return (u8)((val >> (8*offset)) & 0xff);
}

static inline void cec_write(struct lg1k_cec *cec, u32 reg, u32 val)
{
	writel(val, cec->base + reg);
}

void cec_transmit_attempt_done(struct cec_adapter *adap, u8 status)
{
	switch (status & ~CEC_TX_STATUS_MAX_RETRIES) {
		case CEC_TX_STATUS_OK:
			cec_transmit_done(adap, status, 0, 0, 0, 0);
			return;
		case CEC_TX_STATUS_ARB_LOST:
			cec_transmit_done(adap, status, 1, 0, 0, 0);
			return;
		case CEC_TX_STATUS_NACK:
			cec_transmit_done(adap, status, 0, 1, 0, 0);
			return;
		case CEC_TX_STATUS_LOW_DRIVE:
			cec_transmit_done(adap, status, 0, 0, 1, 0);
			return;
		case CEC_TX_STATUS_ERROR:
			cec_transmit_done(adap, status, 0, 0, 0, 1);
			return;
		default:
			/* should never happen */
			WARN(1, "cec-%s: invalid status 0x%02x\n",
				adap->name, status);
			return;
	}
}

static void lg1k_tx_done(struct lg1k_cec *cec, u32 status)
{
	/* micom fw do max retry of transmit already , set
	CEC_TX_STATUS_MAX_RETRIES to avoid driver max retlry*/

	if (status & CEC_TX_STATUS_NACK) {
		cec_transmit_attempt_done(cec->adap,
			CEC_TX_STATUS_NACK|CEC_TX_STATUS_MAX_RETRIES);
		return;
	}
	else if (status & CEC_TX_STATUS_ERROR) {
		cec_transmit_attempt_done(cec->adap,
			CEC_TX_STATUS_ERROR|CEC_TX_STATUS_MAX_RETRIES);
		return;
	}

	cec_transmit_attempt_done(cec->adap, CEC_TX_STATUS_OK);
}

static void lg1k_rx_done(struct lg1k_cec *cec, u32 status)
{
	struct cec_msg msg = {};
	int i, count;
	u32 data;

	/* can't check rx error condition because micom fw only gives rx ok status */
	msg.len = cec->len;

	if (!msg.len) {
		dev_warn(cec->dev, "cec-rx: zero-length packet\n");
		return;
	}

	if (msg.len > 16)
		msg.len = 16;

	/* ipc region access should be aligned to 4-byte boundary */
	count = ALIGN(msg.len, 4);

	for (i = 0; i < count; i += 4) {
		data = readl(cec->base + CEC_RX_DATA_BASE + i);
		writel(data, msg.msg + i);
	}

	dev_dbg(cec->dev, "cec-rx: (%2u): %*ph", msg.len, msg.len, msg.msg);

	cec_received_msg(cec->adap, &msg);
}

static irqreturn_t lg1k_cec_irq_thread_handler(int irq, void *data)
{
	struct device *dev = data;
	struct lg1k_cec *cec = dev_get_drvdata(dev);

	if (cec->len) {	/* rx done response */
		lg1k_rx_done(cec, cec->irq_status);
	} else {	/* tx done response */
		lg1k_tx_done(cec, cec->irq_status);
	}

	cec->irq_status = 0;

	return IRQ_HANDLED;
}

static irqreturn_t lg1k_cec_irq_handler(int irq, void *data)
{
	struct device *dev = data;
	struct lg1k_cec *cec = dev_get_drvdata(dev);
	u8 noti, len, status;
	/* clear interrupt by micom noti */

	noti = cec_read(cec, CEC_NOTI_FROM_MiCOM);
	cec_write(cec, CEC_NOTI_FROM_MiCOM, 0);

	if(!cec->is_enabled)
		return IRQ_HANDLED;

	len = cec_read(cec, CEC_RX_LENGTH);

	if (noti) {
		if (len) { /* rx done response */
			status = cec_read(cec, CEC_RX_DONE);
			cec->irq_status = (u32)(status & 0xff);
			cec->len = (u32)(len & 0xff);
		} else { /* tx done response */
			status = cec_read(cec, CEC_TX_DONE);
			cec->irq_status = (u32)(status & 0xff);
			cec->len = (u32)(len & 0xff);
		}
	}

	return IRQ_WAKE_THREAD;
}

static int lg1k_cec_adap_enable(struct cec_adapter *adap, bool enable)
{
	struct lg1k_cec * cec = (struct lg1k_cec *)(adap->priv);

	cec->is_enabled = enable;
	return 0;
}

static int lg1k_cec_adap_log_addr(struct cec_adapter *adap, u8 log_addr)
{
	return 0;
}

static int lg1k_cec_adap_monitor_all_enable(struct cec_adapter *adap,
				bool enable)
{
	return 0;
}

static int lg1k_cec_adap_transmit(struct cec_adapter *adap, u8 attempts,
				u32 signal_free_time, struct cec_msg *msg)
{
	struct lg1k_cec * cec = (struct lg1k_cec *)(adap->priv);
	int i, count;
	u32 data;

	/* Copy message into registers */

	/* ipc region access should be aligned to 4-byte boundary */
	count = ALIGN(msg->len, 4);

	for (i = 0; i < count; i += 4) {
		data = *((u32 *)(msg->msg + i));
		writel(data, cec->base + CEC_TX_DATA_BASE + i);
	}

	dev_dbg(cec->dev, "cec-tx: <%2u>: %*ph", msg->len, msg->len, msg->msg);

	/*
	 * Start transmission, configure hardware to add start and stop bits
	 * Signal free time is handled by the hardware
	 */
	writel(msg->len, cec->base + CEC_TX_LENGTH);
	writel(1, cec->base + CEC_NOTI_FROM_ARM);

	return 0;
}

static void lge_cec_adapter_config(struct cec_adapter *adap)
{
	if(adap) {
		adap->log_addrs.num_log_addrs = 1;
		adap->log_addrs.log_addr_type[0] = CEC_LOG_ADDR_TYPE_TV;
		adap->log_addrs.cec_version = CEC_VERSION_1_4;
		adap->log_addrs.vendor_id = CEC_VENDOR_ID;
	}
}

static const struct cec_adap_ops lg1k_cec_ops = {
	.adap_enable = lg1k_cec_adap_enable,
	.adap_log_addr = lg1k_cec_adap_log_addr,
	.adap_transmit = lg1k_cec_adap_transmit,
	.adap_monitor_all_enable = lg1k_cec_adap_monitor_all_enable,
};

static int lg1k_cec_probe(struct platform_device *pdev)
{
	struct lg1k_cec *cec;
	struct resource *res;
	int ret = 0;

	cec = devm_kzalloc(&pdev->dev, sizeof(struct lg1k_cec), GFP_KERNEL);

	if (!cec)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res) {
		dev_err(&pdev->dev,
			"Unable to allocate resources for device\n");
		return -EBUSY;
	}

	if (!devm_request_mem_region(&pdev->dev, res->start, resource_size(res),
		pdev->name)) {
		dev_err(&pdev->dev,
			"Unable to request mem region for device\n");
		return -EBUSY;
	}

	cec->base = devm_ioremap_nocache(&pdev->dev, res->start,
			resource_size(res));

	if (!cec->base) {
		dev_err(&pdev->dev, "Unable to grab IOs for device\n");
		return -EBUSY;
	}

	/* set context info. */
	cec->dev = &pdev->dev;

	platform_set_drvdata(pdev, cec);

	cec->irq = of_irq_get(pdev->dev.of_node, 0);
	if (cec->irq < 0) {
		dev_err(&pdev->dev, "Unable to get irq for device\n");
		return -ENOENT;
	}

	ret = devm_request_threaded_irq(&pdev->dev, cec->irq,
		lg1k_cec_irq_handler, lg1k_cec_irq_thread_handler,
		0, "cec_irq", &pdev->dev);

	if (ret) {
		dev_err(&pdev->dev, "Unable to request interrupt for device\n");
		return ret;
	}

	cec->adap = cec_allocate_adapter(&lg1k_cec_ops, cec, LG1K_CEC_NAME,
		CEC_CAP_LOG_ADDRS | CEC_CAP_PHYS_ADDR | CEC_CAP_TRANSMIT |
		CEC_CAP_PASSTHROUGH,
		CEC_MAX_LOG_ADDRS, &pdev->dev);


	if (IS_ERR(cec->adap)) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "Couldn't create cec adapter\n");
		goto cec_error;
	}

	/* XXX: remove this; userspace should invoke ioctl S_LOG_ADDRS */
	lge_cec_adapter_config(cec->adap);

	ret = cec_register_adapter(cec->adap);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't register device\n");
		goto cec_error;
	}

	platform_set_drvdata(pdev, cec);

	return 0;

cec_error:
	cec_delete_adapter(cec->adap);

	return ret;
}

static int lg1k_cec_remove(struct platform_device *pdev)
{
	struct lg1k_cec *cec = platform_get_drvdata(pdev);

	clk_disable_unprepare(cec->clk);

	cec_unregister_adapter(cec->adap);

	return 0;
}

#ifdef CONFIG_PM
static int lg1k_cec_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct lg1k_cec *cec = platform_get_drvdata(pdev);

	cec->is_enabled = false;
	dev_notice(&pdev->dev, "suspend\n");

	return 0;
}

static int lg1k_cec_resume(struct platform_device *pdev)
{
	struct lg1k_cec *cec = platform_get_drvdata(pdev);

	cec->is_enabled = true;
	dev_notice(&pdev->dev, "resume\n");

	return 0;
}
#endif

static const struct of_device_id lg1k_cec_of_match[] = {
	{ .compatible = "lge,lg1k-ucom-cec", },
	{},
};
MODULE_DEVICE_TABLE(of, lg1k_cec_of_match);

static struct platform_driver lg1k_cec_driver = {
	.driver = {
		.name = LG1K_CEC_NAME,
		.of_match_table = of_match_ptr(lg1k_cec_of_match),
	},
	.probe = lg1k_cec_probe,
	.remove = lg1k_cec_remove,

#ifdef CONFIG_PM
	.suspend = lg1k_cec_suspend,
	.resume = lg1k_cec_resume,
#endif
};

module_platform_driver(lg1k_cec_driver);

MODULE_DESCRIPTION("lg1k HDMI CEC driver");
MODULE_AUTHOR("LGE");
MODULE_LICENSE("GPL");
