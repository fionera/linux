#ifndef SDHCI_LG1K_H
#define SDHCI_LG1K_H
#define EMMC_HOST_50_V1			0
#define EMMC_HOST_50_V2			1
#define EMMC_HOST_50_V3			2
#define EMMC_HOST_51			3

struct sdhci_lg1k_drv_data {
	void __iomem 	*top_reg;
	unsigned int	host_rev;
	unsigned int	offset_tab;
	unsigned int	offset_dll;
	unsigned int	offset_rdy;
	unsigned int	offset_delay;
	unsigned int	hs50_out;
	unsigned int	hs50_in;
	unsigned int	hs200_out;
	unsigned int	hs200_in;
	unsigned int	hs400_out;
	unsigned int	hs400_in;
	unsigned int	delay_type_50;
	unsigned int	dll_iff;
	unsigned int	trm_icp;
	unsigned int	strb;
	unsigned int	host_ds;		// host driver strength
	unsigned int	device_ds;		// device driver strength
	unsigned int	pm_enable;
	unsigned int	xfer_complete;
	unsigned int	hw_cg;


};

void sdhci_lg1k_set_inited(struct sdhci_host *host, unsigned int val);
unsigned int sdhci_lg1k_get_inited(struct sdhci_host *host);
void sdhci_lg1k_writew(struct sdhci_host *host, u16 val, int reg);
void sdhci_lg1k_writeb(struct sdhci_host *host, u8 val, int reg);
u32 sdhci_lg1k_readl_syn(struct sdhci_host *host, int reg);
u32 sdhci_lg1k_readl(struct sdhci_host *host, int reg);
u16 sdhci_lg1k_readw(struct sdhci_host *host, int reg);
u8 sdhci_lg1k_readb(struct sdhci_host *host, int reg);
void sdhci_lg1k_bit_set(struct sdhci_host *host,
							unsigned int offset, unsigned int bit);
void sdhci_lg1k_bit_clear(struct sdhci_host *host,
							unsigned int offset, unsigned int bit);
unsigned int sdhci_lg1k_get_min_clock(struct sdhci_host *host);
unsigned int sdhci_lg1k_get_max_clock(struct sdhci_host *host);
void sdhci_lg1k_set_clock(struct sdhci_host *host, unsigned int clock);
int sdhci_lg1k_select_drive_strength(struct sdhci_host *host,
				 struct mmc_card *card,
				 unsigned int max_dtr, int host_drv,
				 int card_drv, int *drv_type);
unsigned int sdhci_lg1k_set_uhs_signaling(struct sdhci_host *host,
								unsigned int uhs, unsigned short reserved);
#endif
