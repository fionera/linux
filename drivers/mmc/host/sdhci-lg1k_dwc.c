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

#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/sizes.h>
#include <linux/delay.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>

#include "sdhci-pltfm.h"
#include "sdhci-lg1k.h"

#define BOUNDARY_OK(addr, len) \
	((addr | (SZ_128M - 1)) == ((addr + len - 1) | (SZ_128M - 1)))

static const struct of_device_id sdhci_lg1k_dwc_dt_ids[];

static inline struct sdhci_lg1k_drv_data *sdhci_lg1k_dwc_get_driver_data(
			struct platform_device *pdev)
{
#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(sdhci_lg1k_dwc_dt_ids, pdev->dev.of_node);
		return (struct sdhci_lg1k_drv_data *)match->data;
	}
#endif
	return (struct sdhci_lg1k_drv_data *)
			platform_get_device_id(pdev)->driver_data;
}

/*
 * If DMA addr spans 128MB boundary, we split the DMA transfer into two
 * so that each DMA transfer doesn't exceed the boundary.
 */
static void lg1k_dwc_adma_write_desc(struct sdhci_host *host, void **desc,
				     dma_addr_t addr, int len, unsigned int cmd)
{
	int tmplen, offset;

	if (addr >= 0x100000000) {
		pr_err("EMMC : OVER 4GB ADDRESS 0x%llx\n", addr);
	}

	if (likely(!len || BOUNDARY_OK(addr, len))) {
		sdhci_adma_write_desc(host, desc, addr, len, cmd);
		return;
	}

	offset = addr & (SZ_128M - 1);
	tmplen = SZ_128M - offset;
	sdhci_adma_write_desc(host, desc, addr, tmplen, cmd);

	addr += tmplen;
	len -= tmplen;
	sdhci_adma_write_desc(host, desc, addr, len, cmd);
}

#ifdef CONFIG_OF
#define SDHCI_DS_50OHM_SYNOPSYS		0x8
#define SDHCI_CTRL_HS400_SYNOPSYS	0x0007

#define EMMC_VENDOR1_BASE			0x100
#define _MMC_CTRL_R					0x2c
#define _AT_CTRL_R					0x40
#define _AT_STAT_R					0x44

#define EMMC_CMDQ_BASE				0x200
#define EMMC_VENDOR2_BASE			0x300
#define _PHY_CNFG					0x00
#define _SDCLKDL_CNFG				0x1c
#define _SMPLDL_CNFG				0x20
#define _DLL_CTRL_CNFG 				0x24
#define _DLL_DLCNFG_OFFST_MSTTSTDC	0x28
#define _DLL_STAT_BTCNFG 			0x2c

#define CLOCK_NOT_INVERT			0
#define CLOCK_INVERT				1


static struct sdhci_lg1k_drv_data lg1k_51_dwc_drv_data = {
	.host_rev = EMMC_HOST_51,
	.pm_enable = 1,
};

static void _lg1k_dwc_regdump(	void __iomem *memp,
								unsigned int size, unsigned int offset)
{
	unsigned int i;

	for (i = 0; i < size; i += 16) {
		pr_info("0x%02X : 0x%08X 0x%08X 0x%08X 0x%08X\n",
			i + offset,
			readl(memp + i + offset + 4),
			readl(memp + i + offset + 4),
			readl(memp + i + offset + 8),
			readl(memp + i + offset + 12));
	}
}

static void lg1k_dwc_regdump(struct sdhci_host *host)
{
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_dwc_get_driver_data(pdev);

#define	REG_BASE_0_OFFSET 0
#define	REG_BASE_1_OFFSET 0x100
#define	REG_BASE_3_OFFSET 0x300

#define	REG_BASE_0_SIZE 96
#define	REG_BASE_1_SIZE 96
#define	REG_BASE_3_SIZE 64

#define	REG_TOP_0_OFFSET 0
#define	REG_TOP_1_OFFSET 0x60

#define	REG_TOP_0_SIZE 32
#define	REG_TOP_1_SIZE 32

	pr_info("LG1K_DWC REG DUMP ------\n");
	_lg1k_dwc_regdump(host->ioaddr, REG_BASE_0_SIZE, REG_BASE_0_OFFSET);
	_lg1k_dwc_regdump(host->ioaddr, REG_BASE_1_SIZE, REG_BASE_1_OFFSET);
	_lg1k_dwc_regdump(host->ioaddr, REG_BASE_3_SIZE, REG_BASE_3_OFFSET);
	pr_info("LG1K_DWC TOP DUMP ------\n");
	_lg1k_dwc_regdump(drv_data->top_reg, REG_TOP_0_SIZE, REG_TOP_0_OFFSET);
	_lg1k_dwc_regdump(drv_data->top_reg, REG_TOP_1_SIZE, REG_TOP_1_OFFSET);
	pr_info("------------------------\n\n");
}

static void lg1k_dwc_clock_disable(struct sdhci_host *host)
{
	unsigned int delay = 30;

	if (sdhci_lg1k_get_inited(host))
		delay = 1;

	sdhci_lg1k_bit_clear(host, SDHCI_CLOCK_CONTROL, 2);
	udelay(delay);
	sdhci_lg1k_bit_clear(host, SDHCI_CLOCK_CONTROL, 0);
	udelay(delay);
}

static void lg1k_dwc_clock_enable(struct sdhci_host *host)
{
	unsigned int delay = 30;

	if (sdhci_lg1k_get_inited(host))
		delay = 1;

	sdhci_lg1k_bit_set(host, SDHCI_CLOCK_CONTROL, 0);
	udelay(delay);
	sdhci_lg1k_bit_set(host, SDHCI_CLOCK_CONTROL, 2);
	udelay(delay);
}

u32 lg1k_dwc_readl(struct sdhci_host *host, int reg)
{
	u32 ret;

	/*
	synopsys ip
	When cmd timeout occurs, cmd complete occurs within 5clock
	1us delay required to eliminate the malfunction caused by this
	*/
	if (reg == SDHCI_INT_STATUS)
		udelay(1);

	ret = sdhci_lg1k_readl(host, reg);

	/* don't support 64bi address for lg1212 */
	if (reg == SDHCI_CAPABILITIES)
		ret &= ~SDHCI_CAN_64BIT;

	return ret;
}

void lg1k_dwc_writel(struct sdhci_host *host, u32 val, int reg)
{
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_dwc_get_driver_data(pdev);
	unsigned int cmdmode = 0;

	if ((reg == SDHCI_INT_STATUS) && (drv_data->hw_cg == 0))
	{
		/* data not present, don't busy check */
		cmdmode = (readl(host->ioaddr + SDHCI_TRANSFER_MODE) >> 16) & 0x23;
		if (cmdmode < 0x03)
			drv_data->xfer_complete |= 2;

		/* clock off when error occur */
		if (val & 0xffff0000)
			drv_data->xfer_complete = 3;

		if (val & 0x3)
			drv_data->xfer_complete |= (val & 0x3);
	}

	writel(val, host->ioaddr + reg);

	if ((reg == SDHCI_INT_STATUS) && (drv_data->hw_cg == 0))
	{
		if (drv_data->xfer_complete == 3) {
			lg1k_dwc_clock_disable(host);
			drv_data->xfer_complete = 0;
		}
	}
}

void lg1k_dwc_writew(struct sdhci_host *host, u16 val, int reg)
{
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_dwc_get_driver_data(pdev);

	if ((reg == SDHCI_COMMAND) && (drv_data->hw_cg == 0)) {
		drv_data->xfer_complete = 0;
		lg1k_dwc_clock_enable(host);
	}

	sdhci_lg1k_writew(host, val, reg);
}

static void lg1k_dwc_out_phase(struct sdhci_host *host, unsigned int invert)
{
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_dwc_get_driver_data(pdev);
	unsigned int reg = 0;

#define TOP_CLOCK_CONTROL 0x18

	/* not inverting gated cclk_tx applied to EMMC */

	if (invert == CLOCK_NOT_INVERT)
		reg = 0x3;

	writel(reg, drv_data->top_reg + TOP_CLOCK_CONTROL);
}

static void lg1k_dwc_otap_disable(struct sdhci_host *host)
{
	sdhci_writel(host, 0x00000200, EMMC_VENDOR2_BASE + _SDCLKDL_CNFG);
	lg1k_dwc_out_phase(host, CLOCK_INVERT);
}

static void lg1k_dwc_itap_disable(struct sdhci_host *host)
{
	sdhci_writel(host, 0x0, EMMC_VENDOR1_BASE + _AT_CTRL_R);
	sdhci_writel(host, 0x0, EMMC_VENDOR1_BASE + _AT_STAT_R);

	sdhci_writel(host, 0x1A, EMMC_VENDOR2_BASE + _SMPLDL_CNFG);
}

static void lg1k_dwc_set_intap(struct sdhci_host *host, unsigned int in)
{
	unsigned int delayconfig = 0x18;

	/* Setup RX delay line
	  enable S/W tune & enable Tune Clk Stop */
	sdhci_writel(host, 0x00010010, EMMC_VENDOR1_BASE + _AT_CTRL_R);

	if (in >= 128) {
		in = in - 128;
		delayconfig = 0x19;
	}

	/* set RX delay code to 2 */
	sdhci_writel(host, in, EMMC_VENDOR1_BASE + _AT_STAT_R);

	/* through delayline &	input config 2 */
	sdhci_writel(host, delayconfig, EMMC_VENDOR2_BASE + _SMPLDL_CNFG);
}

static void lg1k_dwc_set_outtap(struct sdhci_host *host, unsigned int out)
{
	unsigned int reg;

	/* setup TX delay line */
	sdhci_writel(host, 0x00001000, EMMC_VENDOR2_BASE + _SDCLKDL_CNFG);

	reg = (out << 16) + 0x1000;
	sdhci_writel(host, reg, EMMC_VENDOR2_BASE + _SDCLKDL_CNFG);

	reg = (out << 16);
	sdhci_writel(host, reg, EMMC_VENDOR2_BASE + _SDCLKDL_CNFG);

	/* not inverting gated cclk_tx applied to EMMC */
	lg1k_dwc_out_phase(host, CLOCK_NOT_INVERT);
}

static void lg1k_dwc_dll_init(struct sdhci_host *host)
{
	/* DLL slaves update delay input */
	sdhci_writel(host, 0x00002000, EMMC_VENDOR2_BASE + _DLL_CTRL_CNFG);

	/* M/S DLL config (input clock select) */
	sdhci_writel(host, 0x00000060, EMMC_VENDOR2_BASE + _DLL_DLCNFG_OFFST_MSTTSTDC);

	/* DLL Load Val for revaldifation of lock */
	sdhci_writel(host, 0x00003000, EMMC_VENDOR2_BASE + _DLL_STAT_BTCNFG);
}

static void lg1k_dwc_dll_enable(struct sdhci_host *host)
{
	unsigned int dll_lock;
	unsigned int timeout = 10;

	sdhci_writel(host, 0x00004E30, EMMC_VENDOR2_BASE + _DLL_STAT_BTCNFG);
	sdhci_writel(host, 0x00002401, EMMC_VENDOR2_BASE + _DLL_CTRL_CNFG);

	while(1){
		dll_lock = sdhci_readl(host, EMMC_VENDOR2_BASE + _DLL_STAT_BTCNFG);
		if(dll_lock & 0x00010000)
			break;

		if (timeout == 0) {
			pr_err("EMMC : DLL LOCK ERR\n");
			return;
		}

		timeout--;
		udelay(1000);
	}
}

/*
driver strength set
*/
static void lg1k_dwc_set_ds (struct sdhci_host *host, unsigned int ds)
{
	unsigned int reg;

	reg = sdhci_readl(host, EMMC_VENDOR2_BASE + _PHY_CNFG);
	reg &= 0xff00ffff;
	reg |= ((ds << 16) + (ds << 20));
	sdhci_writel(host, reg, EMMC_VENDOR2_BASE + _PHY_CNFG);

	return;
}

/*
enhanced data strobe set
*/
static int lg1k_dwc_set_eds (struct sdhci_host *host, unsigned int onoff)
{
	unsigned int reg;
#define _EMMC_EDS_ENABLE				0x100

	reg = sdhci_readl(host, EMMC_VENDOR1_BASE + _MMC_CTRL_R);
	if (onoff == 0)
		reg &= ~_EMMC_EDS_ENABLE;
	else
		reg |= _EMMC_EDS_ENABLE;
	sdhci_writel(host, reg, EMMC_VENDOR1_BASE + _MMC_CTRL_R);

	return 0;
}

static void lg1k_dwc_reset_cmddata(struct sdhci_host *host)
{
	unsigned int timeout = 100;
	unsigned int reg;

	/* you must cmd & data line reset after clock change */
#define EMMC_CMD_RESET		25
#define EMMC_DATA_RESET		26

	sdhci_lg1k_bit_set(host, SDHCI_CLOCK_CONTROL, EMMC_CMD_RESET);
	mdelay(1);
	sdhci_lg1k_bit_set(host, SDHCI_CLOCK_CONTROL, EMMC_DATA_RESET);
	mdelay(1);

	while(1) {
		reg = sdhci_readl(host, SDHCI_CLOCK_CONTROL);
		if ((reg & 0x06000000) == 0)
			break;

		if (timeout == 0) {
			pr_err("EMMC : CMD/DATA Reset Timeout\n");
			lg1k_dwc_regdump(host);
			return;
		}

		timeout--;
		udelay(1000);
	}
}

static void lg1k_dwc_set_strobe(struct sdhci_host *host)
{
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_dwc_get_driver_data(pdev);
	unsigned int reg;
	unsigned int offset;

	if ((drv_data->strb & 0x80000000) == 0)
		return;

	offset = drv_data->strb & 0xff;

	sdhci_writel(host, 0x0002, EMMC_VENDOR2_BASE + _DLL_CTRL_CNFG);
	sdhci_writel(host, 0x1F60, EMMC_VENDOR2_BASE + _DLL_DLCNFG_OFFST_MSTTSTDC);

	reg = sdhci_readl(host, EMMC_VENDOR2_BASE + _DLL_DLCNFG_OFFST_MSTTSTDC);
	reg &= 0xFFFF00FF;
	reg |= (offset << 8);
	sdhci_writel(host, reg, EMMC_VENDOR2_BASE + _DLL_DLCNFG_OFFST_MSTTSTDC);
}

static void lg1k_dwc_set_tap(struct sdhci_host *host)
{
	u16 ctrl2;
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_dwc_get_driver_data(pdev);

	ctrl2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	switch (ctrl2 & SDHCI_CTRL_UHS_MASK) {
		case SDHCI_CTRL_HS400_SYNOPSYS:
			lg1k_dwc_dll_init(host);
			lg1k_dwc_dll_enable(host);
			lg1k_dwc_set_outtap(host, drv_data->hs400_out);
			lg1k_dwc_set_intap(host, drv_data->hs400_in);
			lg1k_dwc_set_strobe(host);
			break;
		case SDHCI_CTRL_UHS_SDR104:
			if (host->clock <= 26000000) {
				lg1k_dwc_otap_disable(host);
				lg1k_dwc_itap_disable(host);
			} else if (host->clock > 52000000) {
				lg1k_dwc_set_outtap(host, drv_data->hs200_out);
				lg1k_dwc_set_intap(host, drv_data->hs200_in);
			} else {
				lg1k_dwc_set_outtap(host, drv_data->hs50_out);
				lg1k_dwc_set_intap(host, drv_data->hs50_in);
			}
			break;
		case SDHCI_CTRL_UHS_DDR50:
			lg1k_dwc_otap_disable(host);
			lg1k_dwc_set_intap(host, drv_data->hs50_in);
			break;
		case SDHCI_CTRL_UHS_SDR25:
			lg1k_dwc_otap_disable(host);
			lg1k_dwc_set_intap(host, drv_data->hs50_in);
			break;
		default:
			break;
	}
}

static void lg1k_dwc_unset_tap(struct sdhci_host *host)
{
	lg1k_dwc_itap_disable(host);
	lg1k_dwc_otap_disable(host);
}

static void lg1k_dwc_enhanced_strobe(struct mmc_host *host, struct mmc_ios *ios)
{
	struct sdhci_host *dwc_host;
	struct platform_device *pdev = to_platform_device(host->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_dwc_get_driver_data(pdev);

	if (ios->enhanced_strobe == false)
		return;

	dwc_host = mmc_priv(host);
	if (drv_data->host_rev == EMMC_HOST_51) {
		lg1k_dwc_set_eds(dwc_host, 1);
	}
}

static int lg1k_dwc_phy_setting(struct sdhci_host *host)
{
	unsigned int timeout = 10;
	unsigned int reg;

	do {
		reg = sdhci_readl(host, EMMC_VENDOR2_BASE);
		if (reg == 0x2)
			break;

		mdelay(1);
		timeout--;

		if (timeout == 0) {
			pr_err("EMMC : DLL LOCK TIMEOUT\n");
			break;
		}
	} while (1);

	sdhci_writel(host, 0x00880000, EMMC_VENDOR2_BASE);
	sdhci_writel(host, 0x04410441, EMMC_VENDOR2_BASE + 0x4);
	sdhci_writel(host, 0x04410441, EMMC_VENDOR2_BASE + 0x8);
	sdhci_writel(host, 0x00000441, EMMC_VENDOR2_BASE + 0xC);
	sdhci_writel(host, 0x00880001, EMMC_VENDOR2_BASE);
	sdhci_writel(host, 0x00000200, EMMC_VENDOR2_BASE + 0x1C);
	sdhci_writel(host, 0x0000001A, EMMC_VENDOR2_BASE + 0x20);
	sdhci_writel(host, 0x00000000, EMMC_VENDOR1_BASE + 0x40);

	return 0;
}

static void lg1k_dwc_set_clock(struct sdhci_host *host, unsigned int clock)
{
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_dwc_get_driver_data(pdev);
	unsigned short reg;

	if (clock == 0)
		return;

	sdhci_lg1k_bit_clear(host, SDHCI_CLOCK_CONTROL, 2);
	mdelay(1);
	sdhci_lg1k_bit_clear(host, SDHCI_CLOCK_CONTROL, 0);
	mdelay(1);

	sdhci_lg1k_set_clock(host, clock);
	mdelay(1);

	reg = sdhci_readw(host, SDHCI_HOST_CONTROL2) & SDHCI_CTRL_UHS_MASK;
	if (reg == SDHCI_CTRL_UHS_SDR12)
		lg1k_dwc_unset_tap(host);
	else
		lg1k_dwc_set_tap(host);

	if (clock >= 52000000)
		lg1k_dwc_set_ds(host, drv_data->host_ds);
	else
		lg1k_dwc_set_ds(host, SDHCI_DS_50OHM_SYNOPSYS);

	mdelay(1);
	lg1k_dwc_reset_cmddata(host);
}

static void lg1k_dwc_set_uhs_signaling(struct sdhci_host *host,
														unsigned int uhs)
{
	u32 clock = 0;
	u16 overmode = 0;
	u16 ctrl2;

	if (uhs == MMC_TIMING_MMC_HS400)
		overmode = SDHCI_CTRL_HS400_SYNOPSYS;

	clock = sdhci_lg1k_set_uhs_signaling(host, uhs, overmode);

	ctrl2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	ctrl2 |= SDHCI_CTRL_VDD_180;
	sdhci_writew(host, ctrl2, SDHCI_HOST_CONTROL2);

	if (clock) {
		host->mmc->ios.clock = clock;
		host->clock = clock;
		lg1k_dwc_set_clock(host, clock);
	}

	return;
}

void lg1k_dwc_syntop_reset(void)
{
	void __iomem *dwc_syntop = NULL;

	if (of_machine_is_compatible("lge,lg1212"))
	{
#define EMMC_CNTROLLER_SYNTOP	0xc9307000

		dwc_syntop = ioremap(EMMC_CNTROLLER_SYNTOP, 4096);
		writel(0x1e, dwc_syntop + 0x14);
		mdelay(1);
		writel(0x0, dwc_syntop + 0x14);

		iounmap(dwc_syntop);
	}
}

static void lg1k_dwc_reset(struct sdhci_host *host, u8 mask)
{
	unsigned int reg;
	unsigned long timeout;

	lg1k_dwc_unset_tap(host);

	if (mask == SDHCI_RESET_ALL)
	{
		lg1k_dwc_syntop_reset();

		sdhci_lg1k_bit_clear(host, SDHCI_CLOCK_CONTROL, 2);
		mdelay(1);
		sdhci_lg1k_bit_clear(host, SDHCI_CLOCK_CONTROL, 0);
		mdelay(1);

		sdhci_writel(host, 0x01000000, SDHCI_CLOCK_CONTROL);
		/* Wait max 100 ms */
		timeout = 100;

		/* hw clears the bit when it's done */
		while (sdhci_readb(host, SDHCI_SOFTWARE_RESET) & mask) {
			if (timeout == 0) {
				pr_err("%s: Reset 0x%x never completed.\n",
					mmc_hostname(host->mmc), (int)mask);
				lg1k_dwc_regdump(host);
				return;
			}
			timeout--;
			mdelay(1);
		}
	}
	else
		sdhci_reset(host, mask);

	if (mask == SDHCI_RESET_ALL) {
		/* CARD IS EMMC SET */
		reg = sdhci_readl(host, EMMC_VENDOR1_BASE +  _MMC_CTRL_R);
		reg |= 0x1;
		sdhci_writel(host, reg, EMMC_VENDOR1_BASE +  _MMC_CTRL_R);

		/* 1.8v signaling must set */
		reg = sdhci_readl(host, SDHCI_ACMD12_ERR);
		reg |= (0x8 << 16);
		sdhci_writel(host, reg, SDHCI_ACMD12_ERR);

		lg1k_dwc_phy_setting(host);
	}
	return;
}

int lg1k_dwc_select_drive_strength(struct sdhci_host *host,
				 struct mmc_card *card,
				 unsigned int max_dtr, int host_drv,
				 int card_drv, int *drv_type)
{
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_dwc_get_driver_data(pdev);

	return drv_data->device_ds;
}

static int lg1k_dwc_execute_tuning(struct sdhci_host *host, unsigned int opcode)
{


	return 0;
}

/*
                 50ohm, 33ohm, 66ohm, 100ohm, 40ohm
standard table     0      1      2       3      4
synopsys table    0x8    0xe    0x4     0x0    0xc
*/
static unsigned int lg1k_dwc_ds_convert(unsigned int standard_ds)
{
	unsigned int dwc_ds[5] = {0x8, 0xe, 0x4, 0x0, 0xc};

	if (standard_ds >= 5)
		standard_ds = 0;

	return dwc_ds[standard_ds];
}

static void lg1k_dwc_platform_init(struct sdhci_host *host)
{
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct device_node *np = pdev->dev.of_node;
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_dwc_get_driver_data(pdev);
	u32 intap_delay = 0;
	u32 outtap_delay = 0;
	u32 device_ds = 1;
	u32 host_ds = 0;
	u32 strobe = 0;
	u32 caps = 0;
	u32 caps2 = 0;
	u32 op_mode = 0;
	u32 extra;
	u32 hw_cg = 0;

	/*
	 * extra adma table cnt for cross 128M boundary handling.
	 */
	extra = DIV_ROUND_UP_ULL(dma_get_required_mask(&pdev->dev), SZ_128M);
	if (extra > SDHCI_MAX_SEGS)
		extra = SDHCI_MAX_SEGS;
	host->adma_table_cnt += extra;

	drv_data->top_reg = of_iomap(np, 1);

	of_property_read_u32(np, "op-mode", &op_mode);

	if (op_mode == 0) {
		caps |= MMC_CAP_UHS_DDR50;
		caps2 |= MMC_CAP2_HS400_1_8V | MMC_CAP2_HS200_1_8V_SDR | MMC_CAP2_HS400_ES;
	} else {
		host->quirks |= SDHCI_QUIRK_MISSING_CAPS;
		host->caps = 0x3c4dc381;
		host->caps1 = 0x08008077;

		host->quirks2 |= SDHCI_QUIRK2_BROKEN_HS200;
		host->quirks2 |= SDHCI_QUIRK2_BROKEN_DDR50;
	}

	caps |= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA;
	caps2 |= MMC_CAP2_FULL_PWR_CYCLE;

	host->mmc->caps |= caps;
	host->mmc->caps2 |= caps2;

	host->quirks2 |= SDHCI_QUIRK2_BROKEN_1_8V;

	if (!drv_data->top_reg) {
		dev_err(&pdev->dev, "Failed to map IO space\n");
		goto fail;
	}

	if (of_property_read_u32(np, "intap-delay", &intap_delay) < 0) {
		goto fail;
	}

	if (of_property_read_u32(np, "outtap-delay", &outtap_delay) < 0) {
		goto fail;
	}

	if (of_property_read_u32(np, "strobe", &strobe) < 0)
		drv_data->strb = 0;
	else
		drv_data->strb = strobe;

	if (of_property_read_u32(np, "device-ds", &device_ds) < 0)
		drv_data->device_ds = 0;
	else
		drv_data->device_ds = device_ds;

	if (of_property_read_u32(np, "host-ds", &host_ds) < 0)
		drv_data->host_ds = SDHCI_DS_50OHM_SYNOPSYS;
	else
		drv_data->host_ds = lg1k_dwc_ds_convert(host_ds);

	if (of_property_read_u32(np, "hw-clockgating", &hw_cg) < 0)
		drv_data->hw_cg = 0;
	else
		drv_data->hw_cg = hw_cg;

	drv_data->xfer_complete = 0;

	drv_data->hs50_out = outtap_delay & 0xFF;
	drv_data->hs200_out = (outtap_delay >> 16) & 0xFF;
	drv_data->hs400_out = (outtap_delay >> 24) & 0xFF;

	drv_data->hs50_in = intap_delay & 0xFF;
	drv_data->hs200_in = (intap_delay >> 16) & 0xFF;
	drv_data->hs400_in = (intap_delay >> 24) & 0xFF;


	mmc_of_parse(host->mmc);

	host->mmc_host_ops.hs400_enhanced_strobe = lg1k_dwc_enhanced_strobe;

	return;

fail:
	host->quirks2 |= SDHCI_QUIRK2_BROKEN_HS200|SDHCI_QUIRK2_BROKEN_DDR50;
	iounmap(drv_data->top_reg);
}

static struct sdhci_ops lg1k_51_dwc_ops = {
	.platform_init = lg1k_dwc_platform_init,
	.set_clock = lg1k_dwc_set_clock,
	.reset = lg1k_dwc_reset,
	.adma_write_desc = lg1k_dwc_adma_write_desc,
	.set_bus_width = sdhci_set_bus_width,
	.get_min_clock = sdhci_lg1k_get_min_clock,
	.get_max_clock = sdhci_lg1k_get_max_clock,
	.set_uhs_signaling = lg1k_dwc_set_uhs_signaling,
	.platform_execute_tuning = lg1k_dwc_execute_tuning,
	.select_drive_strength = lg1k_dwc_select_drive_strength,
	.read_l = lg1k_dwc_readl,
	.read_w = sdhci_lg1k_readw,
	.read_b = sdhci_lg1k_readb,
	.write_l = lg1k_dwc_writel,
	.write_w = lg1k_dwc_writew,
	.write_b = sdhci_lg1k_writeb,
};


static struct sdhci_pltfm_data dwc_lg1k_51_pdata = {
	.ops  = &lg1k_51_dwc_ops,
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL
			| SDHCI_QUIRK_FORCE_BLK_SZ_2048
			| SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN
			| SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
};
#endif

static int sdhci_lg1k_dwc_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	int ret = -1;
	struct device_node *np = pdev->dev.of_node;
	if (of_device_is_compatible(np, "lge,lg1k-dwc-5.1"))
		ret = sdhci_pltfm_register(pdev, &dwc_lg1k_51_pdata, 0);
	else
		dev_err(&pdev->dev, "Can't find compatible device \n");

	return ret;
#endif
}

static int sdhci_lg1k_dwc_remove(struct platform_device *pdev)
{
	return sdhci_pltfm_unregister(pdev);
}

static struct platform_device_id sdhci_lg1k_dwc_driver_ids[] = {
	{
		.name		= "sdhci-lg1k_dwc",
		.driver_data	= (kernel_ulong_t)&lg1k_51_dwc_drv_data,
	},
	{}
};
MODULE_DEVICE_TABLE(platform, sdhci_lg1k_dwc_driver_ids);

static const struct of_device_id sdhci_lg1k_dwc_dt_ids[] = {
	{ .compatible = "lge,lg1k-dwc-5.1",
		.data = (void *)&lg1k_51_dwc_drv_data },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sdhci_lg1k_dwc_dt_ids);

#ifdef CONFIG_PM_SLEEP
static int lg1k_dwc_sdhci_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_dwc_get_driver_data(pdev);

	if (drv_data->pm_enable) {
		sdhci_lg1k_set_inited(host, 0);
		return sdhci_suspend_host(host);
	} else
		return 0;
}

static int lg1k_dwc_sdhci_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct platform_device *pdev = to_platform_device(host->mmc->parent);
	struct sdhci_lg1k_drv_data *drv_data = sdhci_lg1k_dwc_get_driver_data(pdev);

	if (drv_data->pm_enable) {
		sdhci_lg1k_set_inited(host, 0);
		return sdhci_resume_host(host);
	}
	else
		return 0;
}
#endif	/* CONFIG_PM_SLEEP */
SIMPLE_DEV_PM_OPS(lg1k_dwc_sdhci_pm_ops, lg1k_dwc_sdhci_suspend, lg1k_dwc_sdhci_resume);

static struct platform_driver sdhci_driver = {
	.id_table	= sdhci_lg1k_dwc_driver_ids,
	.driver = {
		.name	= "sdhci-lg1k_dwc",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(sdhci_lg1k_dwc_dt_ids),
		.pm = &lg1k_dwc_sdhci_pm_ops,
	},
	.probe		= sdhci_lg1k_dwc_probe,
	.remove		= sdhci_lg1k_dwc_remove,
};

module_platform_driver(sdhci_driver);

MODULE_DESCRIPTION("LG1XXX Secure Digital Host Controller Interface driver");
MODULE_AUTHOR("Chanho Min <chanho.min@lge.com>, "
	      "Hankyung Yu <hankyung.yu@lge.com>"
	      "Wonmin Jung <wonmin.jung@lge.com>");
MODULE_LICENSE("GPL v2");
