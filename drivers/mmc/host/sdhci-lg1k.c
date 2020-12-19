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

#ifdef CONFIG_OF
#define REG_DUMP_COUNT			16

struct sdhci_lg1k_debug_data {
	unsigned int	inited;
	unsigned int	dump_count;
	unsigned int	dump_order;
	unsigned int	reg_dump[3][REG_DUMP_COUNT];
};

static struct sdhci_lg1k_debug_data lg1k_debug_data = {
	.inited = 0,
	.dump_count = 0,
	.dump_order = 0,
};

static void sdhci_lg1k_put_reg_fifo(struct sdhci_host *host, u32 value)
{
	struct sdhci_lg1k_debug_data *drv_data = &lg1k_debug_data;

	drv_data->reg_dump[0][drv_data->dump_count] = drv_data->dump_order;
		if (drv_data->dump_order >= 0xffffffff)
	drv_data->dump_order = 0;
	drv_data->dump_order++;

	drv_data->reg_dump[1][drv_data->dump_count] = value;
	drv_data->reg_dump[2][drv_data->dump_count] = sdhci_readl(host, SDHCI_ARGUMENT);

	drv_data->dump_count++;
	if (drv_data->dump_count >= REG_DUMP_COUNT)
		drv_data->dump_count = 0;
}

static void sdhci_lg1k_get_reg_fifo(struct sdhci_host *host)
{
	struct sdhci_lg1k_debug_data *drv_data = &lg1k_debug_data;

	unsigned int j;
	unsigned int start = 0;
	unsigned int compare;

	compare = drv_data->reg_dump[0][0];

	for (j = 1; j < REG_DUMP_COUNT; j++) {
		if (drv_data->reg_dump[0][j] <= compare) {
			compare = drv_data->reg_dump[0][j];
			start = j;
		}
	}

	pr_err("sdhci history dump ==================================\n");
	for (j = start; j < REG_DUMP_COUNT; j++) {
		pr_err("[%u] cmd:0x%08X arg:0x%08X\n",
			drv_data->reg_dump[0][j], drv_data->reg_dump[1][j],
			drv_data->reg_dump[2][j]);
	}

	for (j = 0; j < start; j++) {
		pr_err("[%u] cmd:0x%08X arg:0x%08X\n",
			drv_data->reg_dump[0][j], drv_data->reg_dump[1][j],
			drv_data->reg_dump[2][j]);
	}
}

static void sdhci_lg1k_get_reg(struct sdhci_host *host)
{
	unsigned int i;

#define REG_DUMP_SIZE     128

	for (i = 0; i < REG_DUMP_SIZE; i += 16) {
		pr_info("0x%02X : 0x%08X 0x%08X 0x%08X 0x%08X\n",
			i,
			readl(host->ioaddr + i),
			readl(host->ioaddr + i + 4),
			readl(host->ioaddr + i + 8),
			readl(host->ioaddr + i + 12));
	}
}

void sdhci_lg1k_set_inited(struct sdhci_host *host, unsigned int val)
{
	struct sdhci_lg1k_debug_data *drv_data = &lg1k_debug_data;

	drv_data->inited = val;
}

unsigned int sdhci_lg1k_get_inited(struct sdhci_host *host)
{
	struct sdhci_lg1k_debug_data *drv_data = &lg1k_debug_data;

	return drv_data->inited;
}

#define LG1K_ALIGN32(host, addr)	(host->ioaddr + (addr & 0xFFFFFFFC))
#define LG1K_SHIFT(addr)			((addr & 0x3) * 8)
#define LG1K_MASK8(addr)			(~(0xff << ((addr & 0x3) * 8)))
#define LG1K_MASK16(addr)			(~(0xffff << ((addr & 0x3) * 8)))

u32 sdhci_lg1k_readl(struct sdhci_host *host, int reg)
{
	u32 read_reg;

	read_reg = readl(host->ioaddr + reg);
	if ((reg == SDHCI_INT_STATUS) && (read_reg & 0xffff0000)) {
		if (sdhci_lg1k_get_inited(host)) {
			sdhci_lg1k_get_reg(host);
			sdhci_lg1k_get_reg_fifo(host);
		}
	}

	return read_reg;
}

void sdhci_lg1k_writew(struct sdhci_host *host, u16 val, int reg)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	if (reg == SDHCI_TRANSFER_MODE) {
		pltfm_host->xfer_mode_shadow = val;
		return;
	} else if (reg == SDHCI_COMMAND) {
		sdhci_lg1k_put_reg_fifo(host,
			(val << LG1K_SHIFT(reg)) | pltfm_host->xfer_mode_shadow);
		writel((val << LG1K_SHIFT(reg)) | pltfm_host->xfer_mode_shadow, \
			LG1K_ALIGN32(host, reg));
		return;
	}

	writel((readl(LG1K_ALIGN32(host, reg)) & LG1K_MASK16(reg)) | \
		(val << LG1K_SHIFT(reg)), LG1K_ALIGN32(host, reg));
}

void sdhci_lg1k_writeb(struct sdhci_host *host, u8 val, int reg)
{
	writel((readl(LG1K_ALIGN32(host, reg)) & LG1K_MASK8(reg)) | \
		(val << LG1K_SHIFT(reg)), LG1K_ALIGN32(host, reg));
}

u16 sdhci_lg1k_readw(struct sdhci_host *host, int reg)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	if (reg == SDHCI_TRANSFER_MODE)
		return pltfm_host->xfer_mode_shadow;

	return (u16)((readl(LG1K_ALIGN32(host, reg)) >> LG1K_SHIFT(reg)) & 0xffff);
}

u8 sdhci_lg1k_readb(struct sdhci_host *host, int reg)
{
	return (u8)((readl(LG1K_ALIGN32(host, reg)) >> LG1K_SHIFT(reg)) & 0xff);
}

void sdhci_lg1k_bit_set(struct sdhci_host *host,
							unsigned int offset, unsigned int bit)
{
	unsigned int reg;

	reg = sdhci_readl(host, offset);
	reg |= (1 << bit);
	sdhci_writel(host, reg, offset);
}

void sdhci_lg1k_bit_clear(struct sdhci_host *host,
							unsigned int offset, unsigned int bit)
{
	unsigned int reg;

	reg = sdhci_readl(host, offset);
	reg &= ~(1 << bit);
	sdhci_writel(host, reg, offset);
}

unsigned int sdhci_lg1k_get_min_clock(struct sdhci_host *host)
{
	return 398000;
}

unsigned int sdhci_lg1k_get_max_clock(struct sdhci_host *host)
{
	unsigned int clk;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	clk = pltfm_host->clock;

	if (clk)
		return clk;
	else
		return 198000000;
}

void sdhci_lg1k_set_clock(struct sdhci_host *host, unsigned int clock)
{
	int div = 0; /* Initialized for compiler warning */
	u16 clk = 0;
	unsigned long timeout;

	host->mmc->actual_clock = 0;

	sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);

	if (clock == 0)
		return;

	if (clock > (host->max_clk / 2))
		div = 0;
	else {
		for (div = 1; div <= 0x3FF; div *= 2)
			if ((host->max_clk / (div * 2)) <= clock)
				break;
	}

	clk |= (div & SDHCI_DIV_MASK) << SDHCI_DIVIDER_SHIFT;
	clk |= ((div & SDHCI_DIV_HI_MASK) >> SDHCI_DIV_MASK_LEN)
		<< SDHCI_DIVIDER_HI_SHIFT;
	clk |= SDHCI_CLOCK_INT_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
	mdelay(1);

	/* Wait max 20 ms */
	timeout = 20;
	while (!((clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL))
		& SDHCI_CLOCK_INT_STABLE)) {
		if (timeout == 0) {
			pr_err("%s: Internal clock never "
				"stabilised.\n", mmc_hostname(host->mmc));
			return;
		}
		timeout--;
		mdelay(1);
	}

	clk |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
}

unsigned int sdhci_lg1k_set_uhs_signaling(struct sdhci_host *host,
								unsigned int uhs, unsigned short reserved)
{
	u16 ctrl2;
	u32 clock = 0;

	ctrl2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	/* Select Bus Speed Mode for host */
	ctrl2 &= ~SDHCI_CTRL_UHS_MASK;
	ctrl2 &= ~SDHCI_CTRL_VDD_180;

	if (uhs == MMC_TIMING_MMC_HS400) {
		ctrl2 |= (SDHCI_CTRL_HS400 | SDHCI_CTRL_VDD_180);
		clock = 200000000;
		sdhci_lg1k_set_inited(host, 1);
	} else if ((uhs == MMC_TIMING_MMC_HS200) ||
		(uhs == MMC_TIMING_UHS_SDR104)) {
		ctrl2 |= (SDHCI_CTRL_UHS_SDR104 | SDHCI_CTRL_VDD_180);
		clock = 200000000;
	} else if (uhs == MMC_TIMING_UHS_SDR12)
		ctrl2 |= SDHCI_CTRL_UHS_SDR12;
	else if (uhs == MMC_TIMING_UHS_SDR25) {
		ctrl2 |= SDHCI_CTRL_UHS_SDR25;
		clock = 50000000;
	} else if (uhs == MMC_TIMING_UHS_SDR50)
		ctrl2 |= (SDHCI_CTRL_UHS_SDR50 | SDHCI_CTRL_VDD_180);
	else if (uhs == MMC_TIMING_MMC_DDR52)
		ctrl2 |= (SDHCI_CTRL_UHS_DDR50 | SDHCI_CTRL_VDD_180);
	else if (uhs == MMC_TIMING_UHS_DDR50)
		ctrl2 |= (SDHCI_CTRL_UHS_DDR50 | SDHCI_CTRL_VDD_180);
	else if (uhs == MMC_TIMING_MMC_HS) {
		ctrl2 |= SDHCI_CTRL_UHS_SDR25;
		clock = 50000000;
	}

	if (reserved) {
		ctrl2 &= ~SDHCI_CTRL_UHS_MASK;
		ctrl2 &= ~SDHCI_CTRL_VDD_180;
		ctrl2 |= reserved;
	}

	sdhci_writew(host, ctrl2, SDHCI_HOST_CONTROL2);

	return clock;
}
#endif

