/*
 * drivers/mmc/host/sdhci-lg1k.c
 *
 * Support of SDHCI platform devices for lg115x
 *
 * Copyright (C) 2013 LG Electronics
 *
 * Author: Chanho Min <chanho.min@lge.com>
 *             Hankyung Yu <hankyung.yu@lge.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>

#include "sdhci-pltfm.h"
#include "sdhci-lg1k.h"

static const struct of_device_id sdhci_lg1k_dt_ids[];

static inline struct sdhci_lg1k_drv_data *sdhci_lg1k_get_driver_data(
                        struct platform_device *pdev)
{
#ifdef CONFIG_OF
        if (pdev->dev.of_node) {
                const struct of_device_id *match;
                match = of_match_node(sdhci_lg1k_dt_ids, pdev->dev.of_node);
                return (struct sdhci_lg1k_drv_data *)match->data;
        }
#endif
        return (struct sdhci_lg1k_drv_data *)
                        platform_get_device_id(pdev)->driver_data;
}

#ifdef CONFIG_OF
#define EMMC_CLOCK_GATING		0x78
#define EMMC_DLL_SELECT			0x7C

static struct sdhci_lg1k_drv_data lg1k_441_sdhci_drv_data = {
	.pm_enable = 0,
};

static struct sdhci_lg1k_drv_data lg1k_451_sdhci_drv_data = {
	.pm_enable = 0,
};

static struct sdhci_lg1k_drv_data lg1k_451_sd_sdhci_drv_data = {
	.pm_enable = 1,
};

static struct sdhci_lg1k_drv_data lg1k_50_sdhci_drv_data = {
	.host_rev = EMMC_HOST_50_V1,
	.offset_tab = 0x0,
	.offset_dll = 0x28,
	.offset_rdy = 0x3C,
	.pm_enable = 1,
};

static struct sdhci_lg1k_drv_data lg1k_50_sdhci_v2_drv_data = {
	.host_rev = EMMC_HOST_50_V2,
	.offset_tab = 0x0,
	.offset_dll = 0x24,
	.offset_rdy = 0x48,
	.offset_delay = 0x30,
	.dll_iff = 0x0,
	.pm_enable = 1,
};

static struct sdhci_lg1k_drv_data lg1k_50_sdhci_v3_drv_data = {
	.host_rev = EMMC_HOST_50_V3,
	.offset_tab = 0x10,
	.offset_dll = 0x60,
	.offset_rdy = 0x4C,
	.offset_delay = 0x6C,
	.dll_iff = 0x0,
	.pm_enable = 1,
	.device_ds = 0,
};

static struct sdhci_lg1k_drv_data lg1k_51_sdhci_drv_data = {
	.host_rev = EMMC_HOST_51,
	.offset_tab = 0x4,
	.offset_dll = 0x60,
	.offset_rdy = 0x4C,
	.offset_delay = 0x6C,
	.dll_iff = 0x0,
	.pm_enable = 1,
	.device_ds = 0,
};



void lg1k_arasan_enhanced_strobe(struct mmc_host *host, struct mmc_ios *ios)
{
	unsigned int reg;
	struct sdhci_host *lg1k_host;
	struct platform_device *pdev = to_platform_device(host->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);

	if (ios->enhanced_strobe == false)
		return;

	lg1k_host = mmc_priv(host);
	if (drv_data->host_rev == EMMC_HOST_51) {
		reg = sdhci_readl(lg1k_host, EMMC_CLOCK_GATING);
		reg |= 0x1;		/* enhanced strobe enable */
		reg |= 0x2;		/* clock gating enable */
		sdhci_writel(lg1k_host, reg, EMMC_CLOCK_GATING);
	}
}

static void lg1k_arasan_441_platform_init(struct sdhci_host *host)
{
	host->quirks2 |= SDHCI_QUIRK2_PRESET_VALUE_BROKEN;
	host->mmc->caps |= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA
				| MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR;
}

static void lg1k_arasan_451_platform_init(struct sdhci_host *host)
{
	host->mmc->caps |= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA;
	host->mmc->caps2 |= MMC_CAP2_HS200;
	host->quirks2 |= SDHCI_QUIRK2_BROKEN_1_8V;
}

static void lg1k_arasan_platform_init(struct sdhci_host *host)
{
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct device_node *np = pdev->dev.of_node;
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);
	u32 tab_delay = 0;
	u32 device_ds = 1;
	u32 reg = 0;

	host->mmc->caps |= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA | MMC_CAP_UHS_DDR50;
	host->mmc->caps2 |= MMC_CAP2_HS400_1_8V | MMC_CAP2_HS200_1_8V_SDR | MMC_CAP2_FULL_PWR_CYCLE;
	drv_data->top_reg = of_iomap(np, 1);
	host->quirks2 |= SDHCI_QUIRK2_BROKEN_1_8V;

	if (!drv_data->top_reg) {
		dev_err(&pdev->dev, "Failed to map IO space\n");
		goto fail;
	}

	if (of_property_read_u32(np, "tab-delay", &tab_delay) < 0) {
		goto fail;
	}

	if (of_property_read_u32(np, "delaycell-50Mhz",
					&drv_data->delay_type_50) < 0) {
		drv_data->delay_type_50 = 0;
	}

	if (of_property_read_u32(np, "device-ds", &device_ds) < 0)
		drv_data->device_ds = 1;
	else
		drv_data->device_ds = device_ds;

	drv_data->hs50_out = tab_delay & 0xF;
	drv_data->hs50_in = (tab_delay >> 4) & 0x3F;
	drv_data->hs200_out = (tab_delay >> 10) & 0xF;
	drv_data->hs200_in = (tab_delay >> 14) & 0x3F;
	drv_data->hs400_out = (tab_delay >> 20) & 0xF;
	drv_data->hs400_in = (tab_delay >> 24) & 0x3F;

	if (drv_data->host_rev == EMMC_HOST_50_V1) {
		reg = readl(drv_data->top_reg + drv_data->offset_dll);
		drv_data->dll_iff = (reg >> 8) & 0x7;
		drv_data->trm_icp = reg & 0xF;
		drv_data->strb = (reg >> 4) & 0xF;
		drv_data->host_ds = (reg >> 20) & 0x7;
	} else if ((drv_data->host_rev == EMMC_HOST_50_V2) || \
				(drv_data->host_rev == EMMC_HOST_50_V3) || \
				(drv_data->host_rev == EMMC_HOST_51)) {
		reg = readl(drv_data->top_reg + drv_data->offset_delay);
		drv_data->strb = reg & 0xF;
		reg = readl(drv_data->top_reg + drv_data->offset_dll);
		drv_data->trm_icp = reg & 0xF;
		drv_data->host_ds = (reg >> 20) & 0x7;
	}

	mmc_of_parse(host->mmc);

	host->mmc_host_ops.hs400_enhanced_strobe = lg1k_arasan_enhanced_strobe;

	return;

fail:
	host->quirks2 |= SDHCI_QUIRK2_BROKEN_HS200|SDHCI_QUIRK2_BROKEN_DDR50;
	iounmap(drv_data->top_reg);
}

static void lg1k_arasan_set_tab_strobe(struct sdhci_host *host)
{
	unsigned int reg;
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);

	if (drv_data->host_rev == EMMC_HOST_50_V1) {
		reg = readl(drv_data->top_reg + drv_data->offset_dll);

		if (((reg >> 4) & 0xF) == drv_data->strb)
			return;

		reg &= 0xFFFFFF0F;
		reg |= (drv_data->strb << 4);
		writel(reg, drv_data->top_reg + drv_data->offset_dll);

		mdelay(1);
	} else if ((drv_data->host_rev == EMMC_HOST_50_V2) || \
				(drv_data->host_rev == EMMC_HOST_50_V3) || \
				(drv_data->host_rev == EMMC_HOST_51)) {

		reg = readl(drv_data->top_reg + drv_data->offset_delay);

		if ((reg & 0xF) == drv_data->strb)
			return;

		reg &= 0xFFFFFFF0;
		reg |= drv_data->strb;
		writel(reg, drv_data->top_reg + drv_data->offset_delay);

		mdelay(1);
	}
}

static void lg1k_arasan_set_trm_n_dll(struct sdhci_host *host)
{
	unsigned int reg;
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);

	reg = readl(drv_data->top_reg + drv_data->offset_dll);

	if (drv_data->host_rev == EMMC_HOST_50_V1) {
		// Set DLL IFF & TRM ICP
		if (((reg & 0xF) == drv_data->trm_icp) && (((reg >> 8) & 0x7) == drv_data->dll_iff))
			return;

		reg &= 0xFFFFF8F0;
		reg |= drv_data->trm_icp & 0xF;
		reg |= (drv_data->dll_iff << 8);
	} else if ((drv_data->host_rev == EMMC_HOST_50_V2) || \
				(drv_data->host_rev == EMMC_HOST_50_V3) || \
				(drv_data->host_rev == EMMC_HOST_51)) {
		if ((reg & 0xF) == drv_data->trm_icp)
			return;

		reg &= 0xFFFFFFF0;
		reg |= drv_data->trm_icp & 0xF;
	}

	writel(reg, drv_data->top_reg + drv_data->offset_dll);

	mdelay(1);
}

static void lg1k_arasan_disable_dll(struct sdhci_host *host)
{
	unsigned int reg;
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);

	reg = readl(drv_data->top_reg + drv_data->offset_dll);

	if (drv_data->host_rev == EMMC_HOST_50_V1)
		reg &= 0xFFFFF7FF;
	else if ((drv_data->host_rev == EMMC_HOST_50_V2) || \
			(drv_data->host_rev == EMMC_HOST_50_V3) || \
			(drv_data->host_rev == EMMC_HOST_51))
		reg &= 0xFFFFFF7F;

	writel(reg, drv_data->top_reg + drv_data->offset_dll);
	mdelay(1);
}

static void lg1k_arasan_enable_dll(struct sdhci_host *host)
{
	unsigned int reg;
	unsigned int timeout = 10;
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);

	lg1k_arasan_disable_dll(host);
	reg = readl(drv_data->top_reg + drv_data->offset_dll);

	if (drv_data->host_rev == EMMC_HOST_50_V1)
		reg |= 0x00000800;
	else if ((drv_data->host_rev == EMMC_HOST_50_V2) || \
			(drv_data->host_rev == EMMC_HOST_50_V3) || \
			(drv_data->host_rev == EMMC_HOST_51))
		reg |= 0x00000080;

	writel(reg, drv_data->top_reg + drv_data->offset_dll);

	while (timeout--) {
		if (readl(drv_data->top_reg + drv_data->offset_rdy) & 1)
			break;
		mdelay(1);
	}
}

static void lg1k_arasan_set_ds(struct sdhci_host *host, unsigned int ds)
{
	unsigned int reg;
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);

	reg = readl(drv_data->top_reg + drv_data->offset_dll);
	if ((reg & 0x700000) == (ds << 20))
		return;

	reg &= 0xFF8FFFFF;
	reg |= (ds << 20);
	writel(reg, drv_data->top_reg + drv_data->offset_dll);
}

static void lg1k_arasan_set_intab(struct sdhci_host *host, unsigned int in,
				 unsigned int enable)
{
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);

#define IN_TAB_ENABLE			0x00000100
#define IN_TAB_CHANGE_WINDOW	0x00000200
#define IN_TAB_DISABLE			0xFFFFFE00
#define IN_TAB_SHIFT			0
#define IN_TAB_MASK				0xFFFFFF00

	unsigned int tab = 0;

	tab = readl(drv_data->top_reg + drv_data->offset_tab);
	tab &= IN_TAB_DISABLE;
	writel(tab, drv_data->top_reg + drv_data->offset_tab);

	if (enable) {
		tab |= IN_TAB_CHANGE_WINDOW;
		writel(tab, drv_data->top_reg + drv_data->offset_tab);

		tab &= IN_TAB_MASK;
		tab |= (in << IN_TAB_SHIFT);
		writel(tab, drv_data->top_reg + drv_data->offset_tab);

		tab &= ~IN_TAB_CHANGE_WINDOW;
		writel(tab, drv_data->top_reg + drv_data->offset_tab);

		tab |= IN_TAB_ENABLE;
		writel(tab, drv_data->top_reg + drv_data->offset_tab);
	}
}

static void lg1k_arasan_set_outtab(struct sdhci_host *host, unsigned int out,
				unsigned int enable)
{
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);

#define OUT_TAB_ENABLE		0x00100000
#define OUT_TAB_DISABLE		0xFFEFFFFF
#define OUT_TAB_SHIFT		16
#define OUT_TAB_MASK		0xFFF0FFFF

	unsigned int tab = 0;

	tab = readl(drv_data->top_reg + drv_data->offset_tab);

	if (enable) {
		tab &= OUT_TAB_MASK;
		tab |= (OUT_TAB_ENABLE | (out << OUT_TAB_SHIFT));
	} else {
		tab &= OUT_TAB_DISABLE;
	}

	writel(tab, drv_data->top_reg + drv_data->offset_tab);
}

static void lg1k_arasan_set_tab(struct sdhci_host *host)
{
	u16 ctrl2;
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);

	ctrl2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	switch (ctrl2 & SDHCI_CTRL_UHS_MASK)
	{
		case SDHCI_CTRL_HS400:
			lg1k_arasan_set_outtab(host, drv_data->hs400_out, 1);
			lg1k_arasan_set_intab(host, drv_data->hs400_in, 1);
			break;
		case SDHCI_CTRL_UHS_SDR104:
			if (host->clock < 50000000) {
				lg1k_arasan_set_outtab(host, drv_data->hs50_out, 0);
				lg1k_arasan_set_intab(host, drv_data->hs50_in, 0);
			} else if (host->clock > 52000000) {
				lg1k_arasan_set_outtab(host, drv_data->hs200_out, 1);
				lg1k_arasan_set_intab(host, drv_data->hs200_in, 1);
			} else {
				lg1k_arasan_set_outtab(host, drv_data->hs50_out, 1);
				lg1k_arasan_set_intab(host, drv_data->hs50_in, 1);
			}
			break;
		case SDHCI_CTRL_UHS_DDR50:
			lg1k_arasan_set_outtab(host, drv_data->hs50_out, 1);
			lg1k_arasan_set_intab(host, drv_data->hs50_in, 1);
			break;
		case SDHCI_CTRL_UHS_SDR25:
			if ((drv_data->host_rev == EMMC_HOST_50_V2) || \
				(drv_data->host_rev == EMMC_HOST_50_V3) || \
				(drv_data->host_rev == EMMC_HOST_51)) {
				lg1k_arasan_set_outtab(host, drv_data->hs50_out, 1);
				lg1k_arasan_set_intab(host, drv_data->hs50_in, 1);
			}
			break;
		default :
			break;
	}
}

static void lg1k_arasan_unset_tab(struct sdhci_host *host)
{
	lg1k_arasan_set_outtab(host, 0, 0);
	lg1k_arasan_set_intab(host, 0, 0);
}

static void lg1k_arasan_select_standardcell(struct sdhci_host *host)
{
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);
	unsigned short reg;

	reg = readl(drv_data->top_reg + drv_data->offset_delay);
	reg &= 0xffffffcf;

	if ((drv_data->host_rev == EMMC_HOST_50_V3) || \
		(drv_data->host_rev == EMMC_HOST_51)) {
		writel(0x1, drv_data->top_reg + EMMC_DLL_SELECT);

		if (drv_data->delay_type_50)
			reg |= 0x10;
	} else
		reg |= 0x30;

	writel(reg, drv_data->top_reg + drv_data->offset_delay);

	return;
}

static void lg1k_arasan_select_dll(struct sdhci_host *host)
{
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);
	unsigned short reg;

	if ((drv_data->host_rev == EMMC_HOST_50_V3) || \
		(drv_data->host_rev == EMMC_HOST_51))
		writel(0x0, drv_data->top_reg + EMMC_DLL_SELECT);

	reg = readl(drv_data->top_reg + drv_data->offset_delay);
	reg &= 0xffffffcf;
	writel(reg, drv_data->top_reg + drv_data->offset_delay);

	return;
}

static void lg1k_arasan_set_clock(struct sdhci_host *host, unsigned int clock)
{
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);
	unsigned short reg;

	if (clock == 0)
		return;

	lg1k_arasan_set_trm_n_dll(host);

	sdhci_lg1k_set_clock(host, clock);

	reg = sdhci_readw(host, SDHCI_HOST_CONTROL2) & SDHCI_CTRL_UHS_MASK;

	if (reg == SDHCI_CTRL_HS400)
		lg1k_arasan_set_tab_strobe(host);

	if (reg == SDHCI_CTRL_UHS_SDR12) {
		lg1k_arasan_disable_dll(host);
		lg1k_arasan_unset_tab(host);
	} else {
		lg1k_arasan_enable_dll(host);
		lg1k_arasan_set_tab(host);
	}

	if ((drv_data->host_rev == EMMC_HOST_50_V2) || \
		(drv_data->host_rev == EMMC_HOST_50_V3) || \
		(drv_data->host_rev == EMMC_HOST_51)) {
		if (clock <= 52000000) {
			lg1k_arasan_select_standardcell(host);
		} else {
			lg1k_arasan_select_dll(host);
		}
	}

	if (clock >= 52000000)
		lg1k_arasan_set_ds(host, drv_data->host_ds);
	else
		lg1k_arasan_set_ds(host, 0);
}

static void lg1k_arasan_4xx_set_uhs_signaling(struct sdhci_host *host,
														unsigned int uhs)
{
	sdhci_lg1k_set_uhs_signaling(host, uhs, 0);

	return;
}


static void lg1k_arasan_set_uhs_signaling(struct sdhci_host *host,
														unsigned int uhs)
{
	u16 ctrl2;
	u32 clock = 0;

	clock = sdhci_lg1k_set_uhs_signaling(host, uhs, 0);

	ctrl2 = sdhci_readw(host, SDHCI_HOST_CONTROL2) & SDHCI_CTRL_UHS_MASK;

	if (ctrl2 == SDHCI_CTRL_HS400) {
		lg1k_arasan_set_tab_strobe(host);
	}

	if (clock) {
		host->mmc->ios.clock = clock;
		host->clock = clock;
		lg1k_arasan_set_clock(host, clock);
	}

	return;
}

int lg1k_select_drive_strength(struct sdhci_host *host,
				 struct mmc_card *card,
				 unsigned int max_dtr, int host_drv,
				 int card_drv, int *drv_type)
{
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);

	return drv_data->device_ds;
}

static int lg1k_arasan_execute_tuning(struct sdhci_host *host,
												unsigned int opcode)
{
	unsigned int reg;
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);

	lg1k_arasan_set_outtab(host, drv_data->hs200_out, 1);
	lg1k_arasan_set_intab(host, drv_data->hs200_in, 1);

	if (drv_data->host_rev == EMMC_HOST_50_V3)
		sdhci_writel(host, 0x1, EMMC_CLOCK_GATING);
	else if (drv_data->host_rev == EMMC_HOST_51) {
		reg = sdhci_readl(host, EMMC_CLOCK_GATING);
		reg |= 0x2;
		sdhci_writel(host, reg, EMMC_CLOCK_GATING);
	}

	return 0;
}
#endif

static struct sdhci_ops lg1k_441_sdhci_ops = {
	.platform_init = lg1k_arasan_441_platform_init,
	.set_clock = sdhci_set_clock,
	.reset = sdhci_reset,
	.set_uhs_signaling = lg1k_arasan_4xx_set_uhs_signaling,
	.set_bus_width = sdhci_set_bus_width,
};

#ifdef CONFIG_OF
static struct sdhci_ops lg1k_451_sdhci_ops = {
	.platform_init = lg1k_arasan_451_platform_init,
	.set_clock = sdhci_lg1k_set_clock,
	.reset = sdhci_reset,
	.set_uhs_signaling = lg1k_arasan_4xx_set_uhs_signaling,
	.set_bus_width = sdhci_set_bus_width,
	.get_min_clock = sdhci_lg1k_get_min_clock,
	.get_max_clock = sdhci_lg1k_get_max_clock,
};

static struct sdhci_ops lg1k_50_sdhci_ops = {
	.platform_init = lg1k_arasan_platform_init,
	.set_clock = lg1k_arasan_set_clock,
	.reset = sdhci_reset,
	.set_bus_width = sdhci_set_bus_width,
	.get_min_clock = sdhci_lg1k_get_min_clock,
	.get_max_clock = sdhci_lg1k_get_max_clock,
	.set_uhs_signaling = lg1k_arasan_set_uhs_signaling,
	.platform_execute_tuning = lg1k_arasan_execute_tuning,
};

static struct sdhci_ops lg1k_50_sdhci_ops_v3 = {
	.platform_init = lg1k_arasan_platform_init,
	.set_clock = lg1k_arasan_set_clock,
	.reset = sdhci_reset,
	.set_bus_width = sdhci_set_bus_width,
	.get_min_clock = sdhci_lg1k_get_min_clock,
	.get_max_clock = sdhci_lg1k_get_max_clock,
	.set_uhs_signaling = lg1k_arasan_set_uhs_signaling,
	.platform_execute_tuning = lg1k_arasan_execute_tuning,
	.select_drive_strength = lg1k_select_drive_strength,
	.read_l = sdhci_lg1k_readl,
	.read_w = sdhci_lg1k_readw,
	.read_b = sdhci_lg1k_readb,
	.write_w = sdhci_lg1k_writew,
	.write_b = sdhci_lg1k_writeb,
};

static struct sdhci_ops lg1k_51_sdhci_ops = {
	.platform_init = lg1k_arasan_platform_init,
	.set_clock = lg1k_arasan_set_clock,
	.reset = sdhci_reset,
	.set_bus_width = sdhci_set_bus_width,
	.get_min_clock = sdhci_lg1k_get_min_clock,
	.get_max_clock = sdhci_lg1k_get_max_clock,
	.set_uhs_signaling = lg1k_arasan_set_uhs_signaling,
	.platform_execute_tuning = lg1k_arasan_execute_tuning,
	.select_drive_strength = lg1k_select_drive_strength,
	.read_l = sdhci_lg1k_readl,
	.read_w = sdhci_lg1k_readw,
	.read_b = sdhci_lg1k_readb,
	.write_w = sdhci_lg1k_writew,
	.write_b = sdhci_lg1k_writeb,
};
#endif

static struct sdhci_pltfm_data sdhci_lg1k_441_pdata = {
	.ops  = &lg1k_441_sdhci_ops,
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL,
};

#ifdef CONFIG_OF
static struct sdhci_pltfm_data sdhci_lg1k_451_pdata = {
	.ops  = &lg1k_451_sdhci_ops,
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL
			| SDHCI_QUIRK_FORCE_BLK_SZ_2048
			| SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN
			| SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK,
};

static struct sdhci_pltfm_data sdhci_lg1k_451_sd_pdata = {
	.ops  = &lg1k_451_sdhci_ops,
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL
			| SDHCI_QUIRK_FORCE_BLK_SZ_2048
			| SDHCI_QUIRK_MULTIBLOCK_READ_ACMD12
			| SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN
			| SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK,
	.quirks2 = SDHCI_QUIRK2_NO_1_8_V,
};

static struct sdhci_pltfm_data sdhci_lg1k_50_pdata = {
	.ops  = &lg1k_50_sdhci_ops,
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL
			| SDHCI_QUIRK_FORCE_BLK_SZ_2048
			| SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN
			| SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK,
};

static struct sdhci_pltfm_data sdhci_lg1k_50_pdata_v3 = {
	.ops  = &lg1k_50_sdhci_ops_v3,
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL
			| SDHCI_QUIRK_FORCE_BLK_SZ_2048
			| SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN
			| SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
};

static struct sdhci_pltfm_data sdhci_lg1k_51_pdata = {
	.ops  = &lg1k_51_sdhci_ops,
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL
			| SDHCI_QUIRK_FORCE_BLK_SZ_2048
			| SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN
			| SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
};

/*****************************************************************************\
 *                                                                           *
 * Common Function                                                           *
 *                                                                           *
\*****************************************************************************/
#endif

static int sdhci_lg1k_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	int ret = -1;
	struct device_node *np = pdev->dev.of_node;
	if (of_device_is_compatible(np, "lge,lg1k-sdhci-5.0"))
		ret = sdhci_pltfm_register(pdev, &sdhci_lg1k_50_pdata, 0);
	else if (of_device_is_compatible(np, "lge,lg1k-sdhci-5.0-v2"))
		ret = sdhci_pltfm_register(pdev, &sdhci_lg1k_50_pdata, 0);
	else if (of_device_is_compatible(np, "lge,lg1k-sdhci-5.0-v3"))
		ret = sdhci_pltfm_register(pdev, &sdhci_lg1k_50_pdata_v3, 0);
	else if (of_device_is_compatible(np, "lge,lg1k-sdhci-5.1"))
		ret = sdhci_pltfm_register(pdev, &sdhci_lg1k_51_pdata, 0);
	else if (of_device_is_compatible(np, "lge,lg1154-sdhci")
		|| of_device_is_compatible(np, "lge,lg1k-sdhci-4.41"))
		ret = sdhci_pltfm_register(pdev, &sdhci_lg1k_441_pdata, 0);
	else if (of_device_is_compatible(np, "lge,lg1156-sdhci")
		|| of_device_is_compatible(np, "lge,lg1k-sdhci-4.51"))
		ret = sdhci_pltfm_register(pdev, &sdhci_lg1k_451_pdata, 0);
	else if (of_device_is_compatible(np, "lge,lg1k-sdhci-4.51-sd"))
		ret = sdhci_pltfm_register(pdev, &sdhci_lg1k_451_sd_pdata, 0);
	else
		dev_err(&pdev->dev, "Can't find compatible device \n");

	return ret;
#else
	return sdhci_pltfm_register(pdev, &sdhci_lg1k_441_pdata);
#endif
}

static int sdhci_lg1k_remove(struct platform_device *pdev)
{
	return sdhci_pltfm_unregister(pdev);
}

static struct platform_device_id sdhci_lg1k_driver_ids[] = {
	{
		.name		= "sdhci-lg1k",
		.driver_data	= (kernel_ulong_t)&lg1k_51_sdhci_drv_data,
	},
	{}
};
MODULE_DEVICE_TABLE(platform, sdhci_lg1k_driver_ids);

static const struct of_device_id sdhci_lg1k_dt_ids[] = {
	{ .compatible = "lge,lg1154-sdhci",
		.data = (void *)&lg1k_441_sdhci_drv_data },
	{ .compatible = "lge,lg1156-sdhci",
		.data = (void *)&lg1k_451_sdhci_drv_data },
	{ .compatible = "lge,lg1k-sdhci-4.51",
		.data = (void *)&lg1k_451_sdhci_drv_data },
	{ .compatible = "lge,lg1k-sdhci-4.51-sd",
		.data = (void *)&lg1k_451_sd_sdhci_drv_data },
	{ .compatible = "lge,lg1k-sdhci-4.41",
		.data = (void *)&lg1k_441_sdhci_drv_data },
	{ .compatible = "lge,lg1k-sdhci-5.0",
		.data = (void *)&lg1k_50_sdhci_drv_data },
	{ .compatible = "lge,lg1k-sdhci-5.0-v2",
		.data = (void *)&lg1k_50_sdhci_v2_drv_data },
	{ .compatible = "lge,lg1k-sdhci-5.0-v3",
		.data = (void *)&lg1k_50_sdhci_v3_drv_data },
	{ .compatible = "lge,lg1k-sdhci-5.1",
		.data = (void *)&lg1k_51_sdhci_drv_data },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sdhci_lg1k_dt_ids);

#ifdef CONFIG_PM_SLEEP
static int lg1k_sdhci_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);

	if (drv_data->pm_enable) {
		sdhci_lg1k_set_inited(host, 0);
		return sdhci_suspend_host(host);
	} else
		return 0;
}

static int lg1k_sdhci_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_get_driver_data(pdev);

	if (drv_data->pm_enable) {
		sdhci_lg1k_set_inited(host, 0);
		return sdhci_resume_host(host);
	}
	else
		return 0;
}
#endif	/* CONFIG_PM_SLEEP */
SIMPLE_DEV_PM_OPS(lg1k_sdhci_pm_ops, lg1k_sdhci_suspend, lg1k_sdhci_resume);

static struct platform_driver sdhci_driver = {
	.id_table	= sdhci_lg1k_driver_ids,
	.driver = {
		.name	= "sdhci-lg1k",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(sdhci_lg1k_dt_ids),
		.pm = &lg1k_sdhci_pm_ops,
	},
	.probe		= sdhci_lg1k_probe,
	.remove		= sdhci_lg1k_remove,
};

module_platform_driver(sdhci_driver);

MODULE_DESCRIPTION("LG1XXX Secure Digital Host Controller Interface driver");
MODULE_AUTHOR("Chanho Min <chanho.min@lge.com>, "
				  "Hankyung Yu <hankyung.yu@lge.com>");
MODULE_LICENSE("GPL v2");
