#ifndef __ZACC_H
#define __ZACC_H

#define ZACC_DC			0x0000 /* DMA Config */
#define ZACC_AC			0x0004 /* ADMA Ctrl */
#define ZACC_ADC		0x0008 /* ADMA Desc Ctrl */
#define ZACC_ADB		0x000C /* ADMA Desc Base  */
#define ZACC_ADPIC		0x0010 /* ADMA Desc Poll Int Ctrl */
#define ZACC_ADPI		0x0014 /* ADMA Desc Poll Int: 0xffff */
#define ZACC_ADPL		0x0018 /* ADMA Desc Poll Limit: 0xf */
#define ZACC_AS			0x001C /* ADMA Status */
#define ZACC_ACDA		0x0020 /* ADMA Cur Desc Addr (RO) */
#define ZACC_ACSRA		0x0024 /* ADMA Cur Src Read Addr (RO) */
#define ZACC_ACDWA		0x0028 /* ADMA Cur Dest Write Addr (RO) */
#define ZACC_R1			0x002C /* Reserve 1 */
#define ZACC_SC			0x0030 /* SDMA Ctrl */
#define ZACC_SS			0x0034 /* SDMA Status (RO) */
#define ZACC_SSA		0x0038 /* SDMA Src Addr */
#define ZACC_SDA		0x003C /* SDMA Dest Addr */
#define ZACC_SSL		0x0040 /* SDMA Src Length */
#define ZACC_SDL		0x0044 /* SDMA Decomp Length */
#define ZACC_STWL		0x0048 /* SDMA Total WB Length (RO) */
#define ZACC_R2			0x004C /* Reserve 2 */
#define ZACC_IM			0x0050 /* Intr Mask: 0xffff */
#define ZACC_MIS		0x0054 /* Masked Intr Status */
#define ZACC_RIS		0x0058 /* Raw Intr Status (RO) */
#define ZACC_R3			0x005C /* Reserve 3 */
#define ZACC_SR			0x0060 /* Soft Reset */
#define ZACC_CC			0x0064 /* Core Control */
#define ZACC_R5			0x0068 /* Reserve 5 */
#define ZACC_R6			0x006C /* Reserve 6 */
#define ZACC_DR0		0x0070 /* Debug Reg 0 (RO) */
#define ZACC_DR1		0x0074 /* Debug Reg 1 (RO) */
#define ZACC_DR2		0x0078 /* Debug Reg 2 (RO) */
#define ZACC_V			0x0100 /* Version (RO) */

struct zacc_dev {
	struct platform_device *pdev;

	struct list_head list;
	spinlock_t lock;

	void *base;

	/* DMA bounce buffers */
	struct page *dst_pg;
	struct page *src_pg;
};

extern int zacc_decoder_ready(void);
extern int zacc_encoder_ready(void);

extern struct zacc_dev *__zacc_device(struct list_head *list, spinlock_t *lock);
extern struct zacc_dev *__zacc_probe(struct platform_device *pdev);
extern void __zacc_remove(struct platform_device *pdev);
extern int zacc_suspend(struct device *dev);
extern int zacc_resume(struct device *dev);

extern struct dev_pm_ops const zacc_pm_ops;

#endif	/* __ZACC_H */
