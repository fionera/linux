#include <linux/module.h>

#include <linux/kernel.h>

#include <linux/ethtool.h>
#include <linux/gpio/consumer.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/phy.h>
#include <linux/syscore_ops.h>
#include <linux/timer.h>

#define DSP			0x00
#define WOL			0x01
#define BIST			0x03
#define VMDAC			0x07

#define TSTCNTL_R(RB,AD)	(0x8000 | ((RB) << 11) | ((AD) << 5))
#define TSTCNTL_W(RB,AD)	(0x4000 | ((RB) << 11) | ((AD) << 0))
#define TSTCNTL(RW,RB,AD)	TSTCNTL_##RW(RB,AD)

#define OMNIPHY_MCSR		0x11
#define OMNIPHY_TST_CNTL	0x14
#define OMNIPHY_TST_READ1	0x15
#define OMNIPHY_TST_READ2	0x16
#define OMNIPHY_TST_WRITE	0x17
#define OMNIPHY_SCSI		0x1b
#define OMNIPHY_ISF		0x1d
#define OMNIPHY_IMR		0x1e
#define OMNIPHY_SCSR		0x1f

#define MEDIA_NONE		0x00
#define MEDIA_MDI		0x01
#define MEDIA_MDIX		0x02

#define MAX_AUTONEG_RETRY	4
#define MAX_SET_WOL_RETRY	4
#define MAX_GPIOS		4

#define WAKE_MAGIC_SUSPEND	(1 << 15)

struct omniphy_priv {
	struct list_head list;
	struct phy_device *phydev;
	int off_gpio;	/* gpios-index to disable omniphy (-1 for none) */
	int wol_gpio;	/* gpios-index to isolate omniphy (-1 for none) */
	int release_gpio;
	struct gpio_desc *gpios[MAX_GPIOS];
};

static int omniphy_ack_interrupt(struct phy_device *phydev);
static int omniphy_config_intr(struct phy_device *phydev);
static int omniphy_isolate(struct phy_device *phydev, int enable);
static int omniphy_read_status(struct phy_device *phydev);

static LIST_HEAD(omniphy_list);

static int omniphy_syscore_suspend(void)
{
	struct omniphy_priv *priv;
	list_for_each_entry(priv, &omniphy_list, list)
		omniphy_isolate(priv->phydev, 1);
	return 0;
}

static void omniphy_syscore_resume(void)
{
	struct omniphy_priv *priv;
	list_for_each_entry(priv, &omniphy_list, list)
		omniphy_isolate(priv->phydev, 0);
}

static struct syscore_ops omniphy_syscore_ops = {
	.suspend	= omniphy_syscore_suspend,
	.resume		= omniphy_syscore_resume,
};

int __weak omniphy_arch_reset(struct phy_device *phydev, int enable)
{
	return 0;
}

static int omniphy_soft_reset(struct phy_device *phydev)
{
	struct omniphy_priv *priv = phydev->priv;
	int v;

	v = omniphy_arch_reset(phydev, 1);
	if (v < 0)
		return v;

	v = omniphy_isolate(phydev, 0);
	if (priv->release_gpio) {
		if (priv->wol_gpio >= 0 && priv->wol_gpio < MAX_GPIOS) {
			if (priv->gpios[priv->wol_gpio]) {
				gpiod_put(priv->gpios[priv->wol_gpio]);
				priv->gpios[priv->wol_gpio] = NULL;
			}
			priv->wol_gpio = -1;
		}
		priv->release_gpio = 0;
	}
	if (v < 0)
		return v;

	return omniphy_arch_reset(phydev, 0);
}

static int omniphy_config_init(struct phy_device *phydev)
{
	phydev->mdix = ETH_TP_MDI_AUTO;
	return omniphy_config_intr(phydev);
}

int __weak omniphy_arch_init(struct phy_device *phydev)
{
	return 0;
}

static int omniphy_probe(struct phy_device *phydev)
{
	struct omniphy_priv *priv;
	struct gpio_desc *desc;
	int i;

	priv = devm_kzalloc(&phydev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	INIT_LIST_HEAD(&priv->list);
	priv->phydev = phydev;
	phydev->priv = priv;

	for (i = 0; i < MAX_GPIOS; i++) {
		desc = gpiod_get_index(&phydev->dev, "isol", i, GPIOD_ASIS);
		priv->gpios[i] = IS_ERR(desc) ? NULL : desc;
	}

	omniphy_arch_init(phydev);

	list_add(&priv->list, &omniphy_list);

	return 0;
}

static void omniphy_remove(struct phy_device *phydev)
{
	struct omniphy_priv *priv = phydev->priv;
	int i;

	list_del(&priv->list);

	for (i = 0; i < MAX_GPIOS; i++) {
		if (priv->gpios[i])
			gpiod_put(priv->gpios[i]);
	}
}

static int omniphy_sanity(struct phy_device *phydev)
{
	u16 phyidr[2];

	phyidr[0] = phy_read(phydev, MII_PHYSID1);
	phyidr[1] = phy_read(phydev, MII_PHYSID2);

	return ((phyidr[0] << 16) | phyidr[1]) == phydev->phy_id;
}

static int omniphy_resume(struct phy_device *phydev)
{
	if (!phydev->adjust_link || !phydev->attached_dev) {
		dev_info(&phydev->dev, "skip resume\n");
		return 0;
	}

	/* disable WOL */
	phy_write(phydev, OMNIPHY_TST_WRITE, 0x0000);
	phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(W, WOL, 0x03));

	/* clear possible WOL-interrupt */
	omniphy_ack_interrupt(phydev);

	if (omniphy_sanity(phydev)) {
		/* update link-status */
		omniphy_read_status(phydev);

		if (phydev->link && phydev->speed != SPEED_10) {
			dev_info(&phydev->dev, "fast-path resume\n");
			phydev->state = PHY_RUNNING;
			phydev->adjust_link(phydev->attached_dev);

			return 0;
		}
	}

	dev_info(&phydev->dev, "slow-path resume\n");
	phy_init_hw(phydev);

	return 0;
}

static int omniphy_config_aneg(struct phy_device *phydev)
{
	int v;

	switch (phydev->mdix) {
	case ETH_TP_MDI:
		dev_info(&phydev->dev, "forced MDI\n");

		v = phy_read(phydev, OMNIPHY_MCSR);
		v &= ~0x00c0;
		phy_write(phydev, OMNIPHY_MCSR, v);
		break;

	case ETH_TP_MDI_X:
		dev_info(&phydev->dev, "forced MDI-X\n");

		v = phy_read(phydev, OMNIPHY_MCSR);
		v = (v & ~0x0080) | 0x0040;
		phy_write(phydev, OMNIPHY_MCSR, v);
		break;

	default:
		dev_warn(&phydev->dev, "invalid media-type\n");

	case ETH_TP_MDI_AUTO:
		dev_info(&phydev->dev, "MDI/MDI-X auto-detect\n");

		v = phy_read(phydev, OMNIPHY_MCSR);
		v |= 0x0080;
		phy_write(phydev, OMNIPHY_MCSR, v);
		break;
	}

	return genphy_config_aneg(phydev);
}

static void omniphy_switch_media(struct phy_device *phydev, int media)
{
	switch (media) {
	case MEDIA_MDI:
		dev_info(&phydev->dev, "MDI\n");

		phy_write(phydev, OMNIPHY_TST_WRITE, 0x8003);
		phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(W, VMDAC, 0x02));
		phy_write(phydev, OMNIPHY_TST_WRITE, 0x707c);
		phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(W, VMDAC, 0x03));
		break;

	case MEDIA_MDIX:
		dev_info(&phydev->dev, "MDI-X\n");

		phy_write(phydev, OMNIPHY_TST_WRITE, 0xf07b);
		phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(W, VMDAC, 0x02));
		phy_write(phydev, OMNIPHY_TST_WRITE, 0x0004);
		phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(W, VMDAC, 0x03));
		break;

	default:
		dev_warn(&phydev->dev, "invalid media-type\n");
		break;
	}
}

static int omniphy_update_link(struct phy_device *phydev)
{
	int v;

	/* do a fake read */
	v = phy_read(phydev, MII_BMSR);
	if (v < 0)
		return v;

	v = phy_read(phydev, MII_BMSR);
	if (v < 0)
		return v;

	if (v & BMSR_LSTATUS) {
		if (!phydev->link) {
			v = phy_read(phydev, OMNIPHY_MCSR);
			if (v & 0x0040)
				omniphy_switch_media(phydev, MEDIA_MDIX);
			else
				omniphy_switch_media(phydev, MEDIA_MDI);
		}

		phydev->link = 1;
	} else
		phydev->link = 0;

	return 0;
}

static int omniphy_read_status(struct phy_device *phydev)
{
	unsigned int retry = 0;
	int v;

	if (!of_machine_is_compatible("lge,lg1313"))
		return genphy_read_status(phydev);

__retry__:
	v = omniphy_update_link(phydev);
	if (v < 0)
		return v;

	phydev->speed = SPEED_10;
	phydev->duplex = DUPLEX_HALF;
	phydev->pause = 0;
	phydev->asym_pause = 0;
	phydev->lp_advertising = 0;

	if (!phydev->link)
		return 0;

	if (phydev->autoneg) {
		v = phy_read(phydev, MII_LPA);
		if (v < 0)
			return v;

		phydev->lp_advertising = mii_lpa_to_ethtool_lpa_t(v);

		v = phy_read(phydev, OMNIPHY_SCSR);
		if (v < 0)
			return v;

		if ((v & 0x000c) == 0x0000 || (v & 0x000c) == 0x000c) {
			/* reset and retry upon illegal link-mode */
			if (retry++ < MAX_AUTONEG_RETRY) {
				phy_init_hw(phydev);
				goto __retry__;
			}

			dev_warn(&phydev->dev, "auto-negotiation failed\n");

			return -EIO;
		}

		if (v & 0x0008)
			phydev->speed = SPEED_100;
		if (v & 0x0010) {
			phydev->duplex = DUPLEX_FULL;

			if (phydev->lp_advertising & ADVERTISED_Pause)
				phydev->pause = 1;
			if (phydev->lp_advertising & ADVERTISED_Asym_Pause)
				phydev->asym_pause = 1;
		}
	} else {
		v = phy_read(phydev, MII_BMCR);
		if (v < 0)
			return v;

		if (v & BMCR_FULLDPLX)
			phydev->duplex = DUPLEX_FULL;
		if (v & BMCR_SPEED100)
			phydev->speed = SPEED_100;
	}

	return 0;
}

static int omniphy_ack_interrupt(struct phy_device *phydev)
{
	phy_read(phydev, OMNIPHY_ISF);
	return 0;
}

int __weak omniphy_arch_config_intr(struct phy_device *phydev)
{
	return 0;
}

static int omniphy_config_intr(struct phy_device *phydev)
{
	if (phydev->interrupts == PHY_INTERRUPT_ENABLED) {
		/* enable link-related interrupts */
		phy_write(phydev, OMNIPHY_IMR, 0x007e);
	} else {
		/* enable WOL interrupts only */
		phy_write(phydev, OMNIPHY_IMR, 0x0600);
	}

	return omniphy_arch_config_intr(phydev);
}

static void
omniphy_get_wol(struct phy_device *phydev, struct ethtool_wolinfo *wol)
{
	struct net_device *netdev = phydev->attached_dev;
	u16 mac_addr[3];
	int wol_event;

	phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(R, WOL, 0x03));
	wol_event = phy_read(phydev, OMNIPHY_TST_READ1);

	wol->supported = WAKE_MAGIC;
	wol->wolopts = 0;

	if ((wol_event & 0x0007) == 0x0007) {
		wol->wolopts |= WAKE_MAGIC;

		phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(R, WOL, 0x00));
		mac_addr[2] = ntohs(phy_read(phydev, OMNIPHY_TST_READ1));
		phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(R, WOL, 0x01));
		mac_addr[1] = ntohs(phy_read(phydev, OMNIPHY_TST_READ1));
		phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(R, WOL, 0x02));
		mac_addr[0] = ntohs(phy_read(phydev, OMNIPHY_TST_READ1));

		if (netdev && memcmp(mac_addr, netdev->dev_addr, ETH_ALEN)) {
			dev_warn(&phydev->dev, "invalid MAC address: %pM\n",
			         mac_addr);
		}
	}
}

static int omniphy_isolate(struct phy_device *phydev, int enable)
{
	struct omniphy_priv *priv = phydev->priv;

	if (enable) {
		/* IRQ for WOL */
		phydev->interrupts = PHY_INTERRUPT_DISABLED;
		omniphy_config_intr(phydev);
		omniphy_ack_interrupt(phydev);

		/* isolation for WOL */
		if (priv->wol_gpio >= 0 && priv->gpios[priv->wol_gpio])
			gpiod_set_value(priv->gpios[priv->wol_gpio], enable);
	} else {
		/* release isolation */
		if (priv->off_gpio >= 0 && priv->gpios[priv->off_gpio])
			gpiod_set_value(priv->gpios[priv->off_gpio], enable);
		if (priv->wol_gpio >= 0 && priv->gpios[priv->wol_gpio])
			gpiod_set_value(priv->gpios[priv->wol_gpio], enable);

		/* IRQ for PHY-state */
		omniphy_ack_interrupt(phydev);
		phydev->interrupts = PHY_INTERRUPT_ENABLED;
		omniphy_config_intr(phydev);
	}

	return 0;
}

static int
omniphy_set_wol(struct phy_device *phydev, struct ethtool_wolinfo *wol)
{
	struct net_device *netdev = phydev->attached_dev;
	u16 mac_addr[3];
	bool differ_isol = false;
	int wol_event = 0;
	int failure, retry = 0;
	int v;

	/*
	 * handle and clear any private flags first
	 */

	if (wol->wolopts & WAKE_MAGIC_SUSPEND) {
		differ_isol = true;
		wol->wolopts ^= WAKE_MAGIC_SUSPEND;
	}

	if ((wol->wolopts & wol->supported) != wol->wolopts)
		return -ENOTSUPP;

	if (wol->wolopts & WAKE_MAGIC)
		wol_event |= 0x0007;
__try__:
	/* set WOL event(s) */
	phy_write(phydev, OMNIPHY_TST_WRITE, wol_event);
	phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(W, WOL, 0x03));

	if (!wol_event)
		return 0;

	if (wol->wolopts & WAKE_MAGIC) {
		memcpy(mac_addr, netdev->dev_addr, ETH_ALEN);

		/* set MAC address */
		phy_write(phydev, OMNIPHY_TST_WRITE, htons(mac_addr[2]));
		phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(W, WOL, 0x00));
		phy_write(phydev, OMNIPHY_TST_WRITE, htons(mac_addr[1]));
		phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(W, WOL, 0x01));
		phy_write(phydev, OMNIPHY_TST_WRITE, htons(mac_addr[0]));
		phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(W, WOL, 0x02));
	}

	/* enable WOL interrupts */
	phy_write(phydev, OMNIPHY_IMR, 0x0e00);

	/*
	 * disable MDI/MDI-X auto-detection and retain current mode
	 * while WOL-standby -- SW workaround cannot handle MDI/MDI-X
	 * mode changes while WOL-standby, because CPUs are powered-
	 * off. Disable MDI/MDI-X auto-detection and retain current
	 * mode to prevent random WOL-malfunction.
	 */
	v = phy_read(phydev, OMNIPHY_MCSR) & ~0x0080;
	phy_write(phydev, OMNIPHY_MCSR, v);

	/*
	 * check all the settings are really applied
	 */

	failure = 0;

	phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(R, WOL, 0x03));
	v = phy_read(phydev, OMNIPHY_TST_READ1);
	if (v != wol_event) {
		dev_warn(&phydev->dev, "WOL-event not set: %04x\n", v);
		failure++;
	}

	phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(R, WOL, 0x00));
	v = phy_read(phydev, OMNIPHY_TST_READ1);
	if (v != htons(mac_addr[2])) {
		dev_warn(&phydev->dev, "MAC-address[0] not set: %04x\n", v);
		failure++;
	}

	phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(R, WOL, 0x01));
	v = phy_read(phydev, OMNIPHY_TST_READ1);
	if (v != htons(mac_addr[1])) {
		dev_warn(&phydev->dev, "MAC-address[1] not set: %04x\n", v);
		failure++;
	}

	phy_write(phydev, OMNIPHY_TST_CNTL, TSTCNTL(R, WOL, 0x02));
	v = phy_read(phydev, OMNIPHY_TST_READ1);
	if (v != htons(mac_addr[0])) {
		dev_warn(&phydev->dev, "MAC-address[2] not set: %04x\n", v);
		failure++;
	}

	v = phy_read(phydev, OMNIPHY_IMR);
	if (v != 0x0e00) {
		dev_warn(&phydev->dev, "WOL-interrupt not set: %04x\n", v);
		failure++;
	}

	v = phy_read(phydev, OMNIPHY_MCSR);
	if (v & 0x0080) {
		dev_warn(&phydev->dev, "MDI-X auto-detection: %04x\n", v);
		failure++;
	}

	if (failure) {
		if (retry++ < MAX_SET_WOL_RETRY) {
			omniphy_isolate(phydev, 0);
			goto __try__;
		}

		dev_warn(&phydev->dev, "WOL might not work\n");
	} else
		dev_info(&phydev->dev, "WOL activation confirmed\n");

	return differ_isol ? 0 : omniphy_isolate(phydev, 1);
}

static struct phy_driver omniphy_drvs[] = {
	{
		.phy_id		= 0x01814570,
		.name		= "omniphy",
		.phy_id_mask	= 0xfffffff0,
		.features	= PHY_BASIC_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.soft_reset	= omniphy_soft_reset,
		.config_init	= omniphy_config_init,
		.probe		= omniphy_probe,
		.remove		= omniphy_remove,
		.resume		= omniphy_resume,
		.config_aneg	= omniphy_config_aneg,
		.read_status	= omniphy_read_status,
		.ack_interrupt	= omniphy_ack_interrupt,
		.config_intr	= omniphy_config_intr,
		.get_wol	= omniphy_get_wol,
		.set_wol	= omniphy_set_wol,
		.driver.owner	= THIS_MODULE,
	}, {
		.phy_id		= 0x01814580,
		.name		= "omniphy v2",
		.phy_id_mask	= 0xfffffff0,
		.features	= PHY_BASIC_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_init	= omniphy_config_init,
		.probe		= omniphy_probe,
		.remove		= omniphy_remove,
		.resume		= omniphy_resume,
		.config_aneg	= omniphy_config_aneg,
		.read_status	= genphy_read_status,
		.ack_interrupt	= omniphy_ack_interrupt,
		.config_intr	= omniphy_config_intr,
		.driver.owner	= THIS_MODULE,
	},
};

static int __init omniphy_init(void)
{
	int v;

	v = phy_drivers_register(omniphy_drvs, ARRAY_SIZE(omniphy_drvs));
	if (v < 0)
		return v;

	register_syscore_ops(&omniphy_syscore_ops);

	return 0;
}
module_init(omniphy_init);

static void __exit omniphy_exit(void)
{
	unregister_syscore_ops(&omniphy_syscore_ops);
	phy_drivers_unregister(omniphy_drvs, ARRAY_SIZE(omniphy_drvs));
}
module_exit(omniphy_exit);

static struct mdio_device_id omniphy_ids[] __maybe_unused = {
	{ 0x01814570, 0xfffffff0 },
	{ 0x01814580, 0xfffffff0 },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(mdio, omniphy_ids);

MODULE_AUTHOR("Jongsung Kim <neidhard.kim@lge.com>");
MODULE_DESCRIPTION("OmniPhy 10/100Mbps Ethernet PHY driver");
MODULE_LICENSE("GPL");
