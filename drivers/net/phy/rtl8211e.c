#include <linux/module.h>

#include <linux/kernel.h>

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/netdevice.h>
#include <linux/phy.h>

#define RTL8211E_MACR		0x0d
#define RTL8211E_MADR		0x0e
#define RTL8211E_INER		0x12
#define RTL8211E_INSR		0x13
#define RTL8211E_EPSR		0x1e
#define RTL8211E_PGSR		0x1f

#define WAKE_MAGIC_SUSPEND	(1 << 15)

int __weak rtl8211_arch_init(struct phy_device *phydev)
{
	return 0;
}

static int rtl8211e_soft_reset(struct phy_device *phydev)
{
	struct gpio_desc *gpio = gpiod_get(&phydev->dev, "reset", GPIOD_ASIS);

	if (IS_ERR(gpio))
		goto soft_reset;

	dev_info(&phydev->dev, "hard-reset by GPIO\n");
	gpiod_direction_output(gpio, 0);
	gpiod_set_value(gpio, 1);
	msleep(10);
	gpiod_set_value(gpio, 0);
	msleep(30);

	gpiod_put(gpio);

	return 0;
soft_reset:
	dev_info(&phydev->dev, "soft-reset by MDIO\n");
	return genphy_soft_reset(phydev);
}

static int rtl8211e_config_init(struct phy_device *phydev)
{
	struct ethtool_eee eee;
	int v;

	phy_ethtool_get_eee(phydev, &eee);
	eee.advertised = 0;
	phy_ethtool_set_eee(phydev, &eee);

	/* enable spread-spectrum clocking */
	phy_write(phydev, RTL8211E_PGSR, 0x0007);
	phy_write(phydev, RTL8211E_EPSR, 0x00a0);
	v = phy_read(phydev, 0x1a) & 0xfffb;
	phy_write(phydev, 0x1a, v);
	phy_write(phydev, RTL8211E_PGSR, 0x0000);

	return 0;
}

static int rtl8211e_probe(struct phy_device *phydev)
{
	return rtl8211_arch_init(phydev);
}

static int rtl8211e_sanity(struct phy_device *phydev)
{
	u16 phyidr[2];

	phyidr[0] = phy_read(phydev, MII_PHYSID1);
	phyidr[1] = phy_read(phydev, MII_PHYSID2);

	return ((phyidr[0] << 16) | phyidr[1]) == phydev->phy_id;
}

static int rtl8211e_resume(struct phy_device *phydev)
{
	rtl8211_arch_init(phydev);

	phy_write(phydev, RTL8211E_PGSR, 0x0007);
	phy_write(phydev, RTL8211E_EPSR, 0x006d);
	phy_write(phydev, 0x15, 0x0000);
	phy_write(phydev, 0x16, 0x9fff);

	phy_write(phydev, RTL8211E_PGSR, 0x0000);

	if (!phydev->adjust_link || !phydev->attached_dev) {
		dev_info(&phydev->dev, "skip resume\n");
		return 0;
	}

	if (rtl8211e_sanity(phydev)) {
		genphy_read_status(phydev);
		if (phydev->link) {
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

static int rtl8211e_ack_interrupt(struct phy_device *phydev)
{
	phy_read(phydev, RTL8211E_INSR);
	return 0;
}

static int rtl8211e_config_intr(struct phy_device *phydev)
{
	phy_write(phydev, RTL8211E_INER, phydev->interrupts ? 0x0400 : 0x0000);
	/* read INSR once for interrupt logic to work properly */
	rtl8211e_ack_interrupt(phydev);
	return 0;
}

static void
rtl8211e_get_wol(struct phy_device *phydev, struct ethtool_wolinfo *wol)
{
	struct net_device *netdev = phydev->attached_dev;
	u16 mac_addr[3];
	int wol_event;

	phy_write(phydev, RTL8211E_PGSR, 0x0007);
	phy_write(phydev, RTL8211E_EPSR, 0x006d);
	wol_event = phy_read(phydev, 0x15);

	wol->supported = WAKE_MAGIC;
	wol->wolopts = 0;

	if (wol_event & 0x1000) {
		wol->wolopts |= WAKE_MAGIC;

		phy_write(phydev, RTL8211E_PGSR, 0x0007);
		phy_write(phydev, RTL8211E_EPSR, 0x006e);
		mac_addr[0] = phy_read(phydev, 0x15);
		mac_addr[1] = phy_read(phydev, 0x16);
		mac_addr[2] = phy_read(phydev, 0x17);

		if (netdev && memcmp(mac_addr, netdev->dev_addr, ETH_ALEN)) {
			dev_warn(&phydev->dev, "invalid MAC address: %pM\n",
			         mac_addr);
		}
	}

	phy_write(phydev, RTL8211E_PGSR, 0x0000);
}

static int
rtl8211e_set_wol(struct phy_device *phydev, struct ethtool_wolinfo *wol)
{
	struct net_device *netdev = phydev->attached_dev;
	u16 const *mac_addr = (u16 *)netdev->dev_addr;
	int wol_event = 0;

	wol->wolopts &= ~WAKE_MAGIC_SUSPEND;

	if ((wol->wolopts & wol->supported) != wol->wolopts)
		return -ENOTSUPP;

	if (wol->wolopts & WAKE_MAGIC) {
		wol_event |= 0x1000;

		phy_write(phydev, RTL8211E_PGSR, 0x0007);
		phy_write(phydev, RTL8211E_EPSR, 0x006e);
		phy_write(phydev, 0x15, mac_addr[0]);
		phy_write(phydev, 0x16, mac_addr[1]);
		phy_write(phydev, 0x17, mac_addr[2]);
	}

	phy_write(phydev, RTL8211E_PGSR, 0x0007);
	phy_write(phydev, RTL8211E_EPSR, 0x006d);
	phy_write(phydev, 0x16, 0x1fff);
	phy_write(phydev, 0x15, wol_event);

	phy_write(phydev, RTL8211E_PGSR, 0x0000);

	return 0;
}

static struct phy_driver rtl8211e_drvs[] = {
	{
		.phy_id		= 0x001cc915,
		.name		= "RTL8211E(G) Gigabit Ethernet",
		.phy_id_mask	= 0xffffffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.soft_reset	= rtl8211e_soft_reset,
		.config_init	= rtl8211e_config_init,
		.probe		= rtl8211e_probe,
		.resume		= rtl8211e_resume,
		.config_aneg	= genphy_config_aneg,
		.read_status	= genphy_read_status,
		.ack_interrupt	= rtl8211e_ack_interrupt,
		.config_intr	= rtl8211e_config_intr,
		.get_wol	= rtl8211e_get_wol,
		.set_wol	= rtl8211e_set_wol,
		.driver.owner	= THIS_MODULE,
	},
};

module_phy_driver(rtl8211e_drvs);

static struct mdio_device_id rtl8211e_ids[] __maybe_unused = {
	{ 0x001cc915, 0xffffffff },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(mdio, rtl8211e_ids);

MODULE_AUTHOR("Jongsung Kim <neidhard.kim@lge.com>");
MODULE_DESCRIPTION("Realtek RTL8211E(G) Gigabit Ethernet PHY driver");
MODULE_LICENSE("GPL");
