#ifndef WIN32
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/slab.h>     /* kmalloc()      */
#include <linux/fs.h>
#include <asm/segment.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/file.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/random.h>
#include <linux/pageremap.h>
#include <linux/kthread.h>  /* for threads */
#include <linux/freezer.h>
#include <linux/vmalloc.h>
#include <asm/barrier.h> /*dsb()*/
#include <asm/cacheflush.h>
#include <rbus/dhdr_v_composer_reg.h>
#include <rbus/dm_reg.h>
#include <rbus/hdr_all_top_reg.h>
#include <rbus/ppoverlay_reg.h>
#include <rbus/sys_reg_reg.h>
#include <rbus/pll27x_reg_reg.h>
#include <rbus/pll_reg_reg.h>
#include <rbus/vodma_reg.h>
#include <rbus/vodma2_reg.h>
#include <rbus/vgip_reg.h>
#include <rbus/timer_reg.h>
#include <rtk_kdriver/rtk_crt.h>
#include <rbus/mdomain_cap_reg.h>
#include <tvscalercontrol/scalerdrv/scalerdrv.h>
#include <tvscalercontrol/scaler/scalerstruct.h>
#include <../tvscalercontrol/scaler_vscdev.h>
#include <rbus/sb2_reg.h>
#include <rtk_kdriver/rtk_pwm.h>

#include <rtk_kdriver/RPCDriver.h>   /* register_kernel_rpc, send_rpc_command */
#ifdef CONFIG_KDRIVER_USE_NEW_COMMON
	#include <VideoRPC_System.h>
#else
	#include <rpc/VideoRPC_System.h>
#endif
#include <target_display_config.h>

#include "dovi_bec_api.h"
#ifdef RTK_EDBEC_1_5
#include "control_path_priv.h"
#else
#include "VdrDmAPIpFxp.h"
#include "rpu_ext_config.h"
#include "DmTypeDef.h"
#include "VdrDmAPI.h"
#endif
#include "rpu_ext_config.h"

#include <mach/timex.h>
#include "dolbyvisionEDR.h"
#include <mach/rtk_log.h>
#include <rbus/dm_reg.h>

static unsigned char dolby_init_flag = FALSE;//do normal test reset parameter

void set_dolby_init_flag(unsigned char enable)
{
       dolby_init_flag = enable;
}

unsigned char get_dolby_init_flag(void)
{
       return dolby_init_flag;
}

extern void Reset_Pic_and_Backlight(void);
/* for HDMI interrupt control */
extern unsigned int g_hdmi_lastIdxRd;
extern unsigned char hdmi_dolby_last_pair_num;
extern unsigned char hdmi_dolby_cur_pair_num;
unsigned int OTT_backlightSliderScalar = 0;//from handleDoViBacklight
extern long g_dump_flag;
/* local variable to control MD output RB & Inband Cmd */
volatile DOLBYVISION_FLOW_STRUCT *g_MdFlow = NULL;
#ifdef RTK_EDBEC_1_5
composer_register_t dst_comp_reg = {0};
dm_register_t dst_dm_reg = {0};
dm_lut_t dst_dm_lut;
#endif
/* MD parser state control */
MD_PARSER_STATE g_MD_parser_state = MD_PARSER_INIT;


/* thread to check write pointer */
struct task_struct *g_dvRunThread = NULL;


/* in-band & MD ring buffer */
volatile RINGBUFFER_HEADER *g_pInband_rbh = NULL;
volatile RINGBUFFER_HEADER *g_pMd_rbh = NULL;


/* store the virtual address by pli allocation */
volatile unsigned int g_Inband_rb_vaddr = 0;
volatile unsigned int g_Md_rb_vaddr = 0;
volatile unsigned int g_Md_output_buf_vaddr = 0;
volatile unsigned int g_Md_output_buf_phyaddr = 0;

unsigned char OTT_Force_update = FALSE;//force update finish part
/* current PTS info */
PTS_INFO2 g_pts_info;

/* EOS info */
EOS g_eos_info;

/* DV semaphore */
struct semaphore g_dv_sem;
struct semaphore g_dv_pq_sem;

/* record Lv4 exist in video stream */
unsigned int g_lvl4_exist = 0;

/* pid */
pid_t g_dv_task_pid = 0;

unsigned int g_dmMDAddr = 0, g_compMDAddr = 0;
extern pq_config_t* dlbPqConfig_ott;

#ifdef IDK_CRF_USE
unsigned int g_forceB05Update_IDK = 0;
#endif

/* MD calculation time */
unsigned int g_utime_mddeal = 0;

// Dump MD output information
unsigned int DV_MD_DumpOutput_Info(int print)
{
	unsigned int idx, count = 0, max_idx = 0;
	unsigned int mdAddr = g_Md_output_buf_vaddr;
	DOLBYVISION_MD_OUTPUT *mdptr;
	if (g_MdFlow) {
		if(g_MdFlow->bsUnitSize)
			max_idx = g_MdFlow->bsSize/g_MdFlow->bsUnitSize;
		for (idx = 0;idx < max_idx; idx++) {

			mdptr = (DOLBYVISION_MD_OUTPUT *)mdAddr;

			if (mdptr->own_bit)
				count++;

			if (print) {
				printk("[MD][%d] ownbit=%d, PTSH=0x%x, PTSL=0x%x  \n",  idx, Swap4Bytes(mdptr->own_bit), Swap4Bytes(mdptr->PTSH), Swap4Bytes(mdptr->PTSL));
				printk("[MD][%d] timestampH=0x%x, timestampL=0x%x  \n",  idx, mdptr->TimeStampH, mdptr->TimeStampL);
			}
			mdAddr = (mdAddr + sizeof(DOLBYVISION_MD_OUTPUT) >= g_MdFlow->bsLimit) ?
					g_MdFlow->bsBase : (mdAddr + sizeof(DOLBYVISION_MD_OUTPUT));
		}

		if (print) {
#if (defined(CONFIG_CUSTOMER_TV006) && defined(CONFIG_SUPPORT_SCALER))
			printk("%s, get_OTT_HDR_mode=%d, own_bit=1 count=%d \n", __FUNCTION__, get_OTT_HDR_mode(), count);
#endif
			printk("%s, g_pMd_rbh->writePtr=0x%x, readPtr=0x%x \n", __FUNCTION__, (unsigned int)Swap4Bytes(g_pMd_rbh->writePtr), (unsigned int)Swap4Bytes(g_pMd_rbh->readPtr[0]));
			printk("%s, g_pts_info.wPtr=0x%x \n", __FUNCTION__, g_pts_info.wPtr);
			printk("%s, *g_MdFlow->INBAND_ICQ.pRdPtr=0x%x, *g_MdFlow->INBAND_ICQ.pWrPtr=0x%x \n", __FUNCTION__, (unsigned int)*(g_MdFlow->INBAND_ICQ.pRdPtr), (unsigned int)*(g_MdFlow->INBAND_ICQ.pWrPtr));
			printk("%s, g_pInband_rbh->readPtr[0]=0x%x, g_pInband_rbh->writePtr=0x%x \n", __FUNCTION__, (unsigned int)g_pInband_rbh->readPtr[0], (unsigned int)g_pInband_rbh->writePtr);

			//Rpu_Md_WriteBack_Check(g_Md_rb_vaddr,
			//									Swap4Bytes(g_pMd_rbh->size));
		}
	}

	if (print) {
		printk("%s, g_forceUpdate_3DLUT=%d, g_forceUpdateFirstTime=%d \n", __FUNCTION__, g_forceUpdate_3DLUT, g_forceUpdateFirstTime);
	}

	return count;
}

/*! \brief	The API to update the inband command queue
	\param	pRBH	Pointer to the Ring Buffer Header
	\param	size	The size on which the buffer should be updated. Unit is bytes.
*/
void ICQ_UpdateReadPtr (VP_INBAND_RING_BUF *pVIRB, int size)
{
	unsigned int p = Swap4Bytes(*pVIRB->pRdPtr) + size;

	DBG_PRINT_L(DV_INFO,"%s,*pVIRB->pRdPtr=0x%x, size=0x%x  \n", __FUNCTION__, Swap4Bytes(*pVIRB->pRdPtr), size);

	// write-back with big-endain
	*pVIRB->pRdPtr = p < pVIRB->limit ? Swap4Bytes(p) : Swap4Bytes(p - (pVIRB->limit - pVIRB->base));

	DBG_PRINT_L(DV_INFO,"%s, p=0x%x, pVIRB->pRdPtr=0x%x, *pVIRB->pRdPtr=0x%x  \n", __FUNCTION__, p, pVIRB->pRdPtr, Swap4Bytes(*pVIRB->pRdPtr));
}

/*! \brief	The API to retrieve the command.
	\param	pRBH			Pointer to the Ring Buffer Header
	\param	bUpdateReadPtr	Bool variable to specify whether to update the read pointer or not
	\return Returns the pointer to the retrieved data structure, 0 if command queue empty.
			You will need to free the pointer.
*/
void *ICQ_GetCmd (void *p, int p_size, VP_INBAND_RING_BUF *pVIRB, int bUpdateReadPtr)
{
	unsigned int read, write, base, limit, size;

	/*
	//read = CONFIG_PAGE_OFFSET | Swap4Bytes(*pVIRB->pRdPtr);
	read = CONFIG_PAGE_OFFSET | Swap4Bytes(pVIRB->read);		// avoid write RBH read pointer for workaround
	write = CONFIG_PAGE_OFFSET | Swap4Bytes(*pVIRB->pWrPtr);
	base = CONFIG_PAGE_OFFSET | pVIRB->base;
	limit = CONFIG_PAGE_OFFSET | pVIRB->limit;
	*/
	//read = g_Inband_rb_vaddr + (Swap4Bytes(pVIRB->read) - pVIRB->base);
	read = g_Inband_rb_vaddr + (Swap4Bytes(*pVIRB->pRdPtr) - pVIRB->base);
	write = g_Inband_rb_vaddr + (Swap4Bytes(*pVIRB->pWrPtr) - pVIRB->base);
	base = g_Inband_rb_vaddr;
	limit = g_Inband_rb_vaddr + Swap4Bytes(g_pInband_rbh->size);

	pr_debug("%s, read=0x%x, write=0x%x \n", __FUNCTION__, read, write);
	//DBG_PRINT(KERN_EMERG"%s, write=0x%x \n", __FUNCTION__, write);
	//DBG_PRINT("%s, base=0x%x \n", __FUNCTION__, base);
	//DBG_PRINT("%s, limit=0x%x \n", __FUNCTION__, limit);
	//printk("%s, write-read=0x%x \n", __FUNCTION__, write - read);

	if (read == write) {
		//Warning("%s, wait Inband RB Command, read=0x%x, write=0x%x \n", __FUNCTION__, read, write);
		msleep(1);
		return NULL;
	}

	size = &((INBAND_CMD_PKT_HEADER *)read)->size == (void *)limit
			? *(unsigned int *)base : ((INBAND_CMD_PKT_HEADER *)read)->size;

	size = Swap4Bytes(size);

	//DBG_PRINT("%s, size=0x%x, p_size=0x%x \n", __FUNCTION__, size, p_size);
	//DBG_PRINT("%s, p=0x%x \n", __FUNCTION__, p);

	if (p)
	{
		if (size <= p_size)
		{
			if ((read + size) <= limit) {
				memcpy(p, (void *)read, size);
			} else	{
				memcpy(p, (void *)read, limit - read);
				memcpy((void *)((unsigned int)p + limit - read), (void *)base, size - (limit - read));
			}

			if (bUpdateReadPtr)
				ICQ_UpdateReadPtr(pVIRB, size);
		}
		else{
			//ASSERT (1,SW_TRAP_FLOW_ERROR,"ICQ_GetCmd size too big!\n") ;
			printk(KERN_EMERG"ICQ_GetCmd size too big! size=0x%x, p_size=0x%x \n", size, p_size);
		}
	}

	return p;
}


unsigned int cur_buffer_num = 0;

unsigned int DV_SearchAvailOutBuffer(void)
{
	unsigned int idx = 0, max_idx = 0;
	//unsigned int ownBitAddr = g_Md_output_buf_vaddr;
	DOLBYVISION_MD_OUTPUT *p_md_output;

	unsigned long timeout = jiffies + (HZ/5); /* timeout in 200 ms */

	if(g_MdFlow)
	{
		if(g_MdFlow->bsUnitSize)
			max_idx = g_MdFlow->bsSize/g_MdFlow->bsUnitSize;
		else
			return 0;
	}
	else
		return 0;



	while (1) {

		p_md_output = (DOLBYVISION_MD_OUTPUT *)g_Md_output_buf_vaddr;
		cur_buffer_num = 0;

		for (idx = 0; idx < max_idx; idx++) {

			/* check own_bit */
			if ((p_md_output->own_bit != 0) || (p_md_output->own_bit2 != 0)) {
				cur_buffer_num++;
			}
			p_md_output++;
		}


		
		p_md_output = (DOLBYVISION_MD_OUTPUT *)g_Md_output_buf_vaddr;

		for (idx = 0; idx < max_idx; idx++) {

			/* check own_bit */
			if ((p_md_output->own_bit == 0) && (p_md_output->own_bit2 == 0)) {
				DBG_PRINT("%s, number %d unit offset \n", __FUNCTION__, idx);
				DBG_PRINT("DV_SearchAvailOutBuffer mdoutput phy=0x%x\n", g_Md_output_buf_phyaddr + (p_md_output - g_Md_output_buf_vaddr));
				/* available buffer */
				return (unsigned int)p_md_output;
			}
			p_md_output++;
		}

		if (idx == max_idx) {	/* buffer full */
			//DBG_PRINT(KERN_EMERG"%s, MD output buffer is full \n", __FUNCTION__);

			/* pause/stop command comes, does not search */
			if (g_MdFlow->state == DV_FLOW_PAUSE || g_MdFlow->state == DV_FLOW_STOP)
				return 0;


			msleep(1);

			pr_debug("%s, MD output buffer is full \n", __FUNCTION__);
		}

		if (time_before(jiffies, timeout) == 0) {		/*  over 200 ms */
			//Warning("%s, Time-out (over 200ms) \n", __FUNCTION__);
			timeout = jiffies + (HZ/5);
			/* pause/stop command comes, does not search */
			if (g_MdFlow->state == DV_FLOW_PAUSE || g_MdFlow->state == DV_FLOW_STOP)
				return 0;
		}

		msleep(1);
	}
}

void DV_scanInBandCmd (VP_INBAND_RING_BUF *ib_icq/*, unsigned int BS_Ptr*/)
{
	unsigned int	inbandCmd[16];
	unsigned char new_pts = 0;
	INBAND_CMD_TYPE type;
	unsigned int trycnt = 0;
	//unsigned int p;
	//static unsigned int timebak = 0;


	while (ICQ_GetCmd (inbandCmd, sizeof(inbandCmd), ib_icq, 0) && !new_pts)
	{
		type = Swap4Bytes(((INBAND_CMD_PKT_HEADER *)inbandCmd)->type);

		DBG_PRINT_L(DV_INFO,"%s, type %d, g_more_eos_err %d \n", __FUNCTION__, type, g_more_eos_err);

		switch (type)
		{
			case INBAND_CMD_TYPE_PTS:
				if (g_more_eos_err == 0)
				{
					g_pts_info.PTSH = Swap4Bytes(((PTS_INFO2 *)inbandCmd)->PTSH);//PTS not real
					g_pts_info.PTSL = Swap4Bytes(((PTS_INFO2 *)inbandCmd)->PTSL);//PTS not real
					g_pts_info.header.type = Swap4Bytes(((INBAND_CMD_PKT_HEADER *)inbandCmd)->type);
					g_pts_info.header.size = Swap4Bytes(((INBAND_CMD_PKT_HEADER *)inbandCmd)->size);
					g_pts_info.wPtr = Swap4Bytes(((PTS_INFO2 *)inbandCmd)->wPtr);//This is for MD parse data read point
					g_pts_info.PTSH2 = Swap4Bytes(((PTS_INFO2 *)inbandCmd)->PTSH2);//Real PTS
					g_pts_info.PTSL2 = Swap4Bytes(((PTS_INFO2 *)inbandCmd)->PTSL2);//Real PTS
					g_pts_info.length = Swap4Bytes(((PTS_INFO2 *)inbandCmd)->length);//Kernel no need
					g_pts_info.flag = Swap4Bytes(((PTS_INFO2 *)inbandCmd)->flag);//Can check IDR frame. If yes change picture mode to parse
					new_pts = 1;

					pr_debug("%s, PTSH=0x%x, PTSL=0x%x \n", __FUNCTION__, g_pts_info.PTSH, g_pts_info.PTSL);
					pr_debug("%s, PTSH2=0x%x, PTSL2=0x%x \n", __FUNCTION__, g_pts_info.PTSH2, g_pts_info.PTSL2);
					pr_debug("%s, header.type=0x%x, header.size=0x%x \n", __FUNCTION__, g_pts_info.header.type, g_pts_info.header.size);
					pr_debug("%s, wPtr=0x%x  \n", __FUNCTION__, g_pts_info.wPtr);
					pr_debug("%s, flag=0x%x,\n", __FUNCTION__, g_pts_info.flag);

					//if (timebak == 0)
					//	timebak = rtd_inl(SCPU_CLK90K_LO_reg);
					//printk("%s, timestamp=%lld \n", __FUNCTION__, (rtd_inl(SCPU_CLK90K_HI_reg)<<32)|rtd_inl(SCPU_CLK90K_LO_reg));
					//Warning("%s, wait PTS time=%d msec, g_dvframeCnt=%d \n", __FUNCTION__, ((rtd_inl(SCPU_CLK90K_LO_reg)-timebak)*1000)/90000, g_dvframeCnt);
					//timebak = rtd_inl(SCPU_CLK90K_LO_reg);
					//g_utime_mddeal = (rtd_inl(SCPU_CLK90K_HI_reg)<<32) | rtd_inl(SCPU_CLK90K_LO_reg);
				}
				break;

			case INBAND_CMD_TYPE_EOS:
				g_MD_parser_state = MD_PARSER_EOS;
				g_eos_info.header.type = Swap4Bytes(((INBAND_CMD_PKT_HEADER *)inbandCmd)->type);
				g_eos_info.header.size = Swap4Bytes(((INBAND_CMD_PKT_HEADER *)inbandCmd)->size);
				g_eos_info.wPtr = Swap4Bytes(((EOS *)inbandCmd)->wPtr);
				DBG_PRINT_L(DV_INFO,"%s, g_eos_info.header.type=0x%x, g_eos_info.header.size=0x%x \n", __FUNCTION__, g_eos_info.header.type, g_eos_info.header.size);
				DBG_PRINT_L(DV_INFO,"%s, g_eos_info.wPtr=0x%x  \n", __FUNCTION__, g_eos_info.wPtr);
				DBG_PRINT_L(DV_INFO,"DV_scanInBandCmd: INBAND_CMD_TYPE_EOS command type \n");
				break;

			case INBAND_CMD_TYPE_NEW_SEG:
				DBG_PRINT_L(DV_INFO,"DV_scanInBandCmd: INBAND_CMD_TYPE_NEW_SEG command type \n");
				break;

			case INBAND_CMD_TYPE_DECODE:
				DBG_PRINT_L(DV_INFO,"DV_scanInBandCmd: INBAND_CMD_TYPE_DECODE command type \n");
				break;

			default:
				memset(&g_pts_info, 0x00, sizeof(g_pts_info));
				DBG_PRINT_L(DV_INFO,"DV_scanInBandCmd: unknown in-band command type %d\n", Swap4Bytes(((INBAND_CMD_PKT_HEADER *)inbandCmd)->type));
				break ;
		}

		if (g_more_eos_err == 0 || type == INBAND_CMD_TYPE_EOS)
			ICQ_UpdateReadPtr(ib_icq, Swap4Bytes(((INBAND_CMD_PKT_HEADER *)inbandCmd)->size));
/*
		// read pointer workaround// write-back with big-endain
		if (g_more_eos_err == 0 || type == INBAND_CMD_TYPE_EOS) {
			p = Swap4Bytes(ib_icq->read) + Swap4Bytes(((INBAND_CMD_PKT_HEADER *)inbandCmd)->size);
			ib_icq->read = p < ib_icq->limit ? Swap4Bytes(p) : Swap4Bytes(p - (ib_icq->limit - ib_icq->base));
			DBG_PRINT("%s, p=0x%x, ib_icq->read=0x%x  \n", __FUNCTION__, p, Swap4Bytes(ib_icq->read));
		}
*/

		if (new_pts || type == INBAND_CMD_TYPE_EOS)	// added by smfan
			break;	// added by smfan

		if (new_pts == 0)
			msleep(1);

		if (trycnt++ > 10) {	// added by smfan
			if (g_more_eos_err == 0)
				memset(&g_pts_info, 0x00, sizeof(g_pts_info));
			break;	// added by smfan
		}

	}

}


int DV_MD_ThreadProcess(void *qq)
{
	static unsigned int IDR_cur_flag = 0;
#ifndef RTK_EDBEC_1_5
	extern void handleDoViBacklight(DmCfgFxp_t *p_cfg, unsigned char uiBackLightValue,  unsigned char src, unsigned int *SliderScalar);
#endif
	unsigned int firstFrameDone = 0;		/* first frame's MD must be parsed, otherwise following frame will parse fail */
	unsigned int rbMdLimit  = Swap4Bytes(g_pMd_rbh->beginAddr) + Swap4Bytes(g_pMd_rbh->size);
	unsigned int mdParserDoneSize = 0, mdParserRemainSize = 0;
	unsigned int rbFillLen, rbFillLen2;
	VP_INBAND_RING_BUF rbMdCmd;
	unsigned int dstAddr = 0, srcAddr = 0;
	//unsigned int p;
	static unsigned int savePTS_chk = 0;
	unsigned int testCnt = 0;
	unsigned long md_ring_wPtr = 0;
	unsigned int framerate = 24;
	unsigned short *ptr=0;

	rbMdCmd.pWrPtr = (unsigned long *)&g_pMd_rbh->writePtr;
	rbMdCmd.pRdPtr = (unsigned long *)&g_pMd_rbh->readPtr[0];
	rbMdCmd.base   =  Swap4Bytes(g_pMd_rbh->beginAddr);		// to little-endian
	rbMdCmd.limit  =  rbMdLimit;
	rbMdCmd.read = *(rbMdCmd.pRdPtr);

	printk(KERN_INFO "in DV_MD_process \n");

	while (1) {

		set_freezable();

		if (kthread_should_stop())
			break;

		if(!g_MdFlow)
		{
			msleep(1);
			continue;
		}


		if (g_MdFlow && g_MdFlow->state == DV_FLOW_STOP) {
			g_MdFlow->state_chk = DV_FLOW_STOP;
			msleep(1);
			continue;
		}

		if (g_MdFlow && g_MdFlow->state == DV_FLOW_PAUSE) {
			g_MdFlow->state_chk = DV_FLOW_PAUSE;
			msleep(1);
			continue;
		}

		if(!g_MdFlow)
		{
			msleep(1);
			continue;
		}
#ifdef CONFIG_SUPPORT_SCALER
		if(Scaler_DispGetInputInfoByDisp(SLR_MAIN_DISPLAY, SLR_INPUT_STATE) != _MODE_STATE_ACTIVE)
			framerate = 300;//means not yet run scaler
		else
			framerate = Scaler_DispGetInputInfoByDisp(SLR_MAIN_DISPLAY, SLR_INPUT_V_FREQ);
#endif
		framerate = framerate / 10;
		if (framerate < 24)
			framerate = 24;

		if (DV_MD_DumpOutput_Info(0) > framerate) {
			msleep(1);
			continue;
		}

		down(&g_dv_pq_sem);

		// check inband command
		DV_scanInBandCmd((VP_INBAND_RING_BUF *)&(g_MdFlow->INBAND_ICQ));

		// check inband write pointer which pointer to MD buffer address
		rbFillLen = 0;
		if (g_pts_info.wPtr != 0) {
			//rbFillLen = g_pts_info.wPtr + (g_pts_info.wPtr >= Swap4Bytes(g_pMd_rbh->readPtr[0]) ?
			//				0 : rbMdLimit) - Swap4Bytes(g_pMd_rbh->readPtr[0]);
			rbFillLen = Swap4Bytes(g_pMd_rbh->writePtr) + ((Swap4Bytes(g_pMd_rbh->writePtr) >= g_pts_info.wPtr) ?
							0 : rbMdLimit) - g_pts_info.wPtr;
			DBG_PRINT_L(DV_INFO,"%s, g_pMd_rbh->writePtr=0x%x, g_pts_info.wPtr=0x%x \n", __FUNCTION__, Swap4Bytes(g_pMd_rbh->writePtr), g_pts_info.wPtr);

			//if (rbFillLen==0)
			//	printk("%s, rbFillLen=0x%x \n", __FUNCTION__, rbFillLen);

			if (firstFrameDone == 0 && rbFillLen == 0) {
				g_more_eos_err = 1;	/* waiting for MD write pointer has new data */
				msleep(1);
				pr_debug(KERN_DEBUG"\r\n#### func:%s line:%d ####\r\n",__FUNCTION__,__LINE__);
			} else
				firstFrameDone = 1;

			/* wait MD write pointer of ring buffer */
			if (g_more_eos_err)
			{
				msleep(1);
				pr_debug(KERN_DEBUG"\r\n#### func:%s line:%d ####\r\n",__FUNCTION__,__LINE__);
			}

			/* force update MD ring buffer's read pointer sync with PTS write pointer */
			*rbMdCmd.pRdPtr = Swap4Bytes(g_pts_info.wPtr);

		}

		DBG_PRINT_L(DV_INFO,"g_pMd_rbh->writePtr=0x%x, readPtr[0]=0x%x, g_pts_info.wPtr=0x%x \n", (unsigned int)Swap4Bytes(g_pMd_rbh->writePtr), (unsigned int)Swap4Bytes(g_pMd_rbh->readPtr[0]), g_pts_info.wPtr);
		DBG_PRINT_L(DV_INFO,"g_more_eos_err=%d, firstFrameDone=%d, \n", g_more_eos_err, firstFrameDone);
		DBG_PRINT_L(DV_INFO,"g_MdFlow->state=%d, g_pts_info.PTSH=%d, g_pts_info.PTSL=%d\n", g_MdFlow->state, g_pts_info.PTSH, g_pts_info.PTSL);

		// for testing
		if (g_pts_info.PTSH==savePTS_chk && g_more_eos_err==0) {
			testCnt++;
			if (testCnt > 1000) {
				testCnt = 0;
				msleep(1);
				//Warning("g_pts_info.PTSH=%d, g_pts_info.PTSL=%d\n", g_pts_info.PTSH, g_pts_info.PTSL);
				pr_debug(KERN_DEBUG"\r\n#### func:%s line:%d ####\r\n",__FUNCTION__,__LINE__);
			}
		}
		savePTS_chk = g_pts_info.PTSH;

		if (g_MdFlow->state == DV_FLOW_RUN && rbFillLen && (g_pts_info.PTSH || g_pts_info.PTSL)) {

			g_MdFlow->state_chk = DV_FLOW_RUN;

			DBG_PRINT_L(DV_INFO,"%s, rbMdCmd.pWrPtr =0x%x \n", __FUNCTION__, rbMdCmd.pWrPtr);
			DBG_PRINT_L(DV_INFO,"%s, *rbMdCmd.pRdPtr =0x%x \n", __FUNCTION__, Swap4Bytes(*((unsigned long *)rbMdCmd.pRdPtr)));
			DBG_PRINT_L(DV_INFO,"%s, rbMdCmd.base =0x%x \n", __FUNCTION__, rbMdCmd.base);
			DBG_PRINT_L(DV_INFO,"%s, rbMdCmd.limit =0x%x \n", __FUNCTION__, rbMdCmd.limit);
			DBG_PRINT_L(DV_INFO,"%s, rbMdCmd.read =0x%x \n", __FUNCTION__, Swap4Bytes(rbMdCmd.read));
			DBG_PRINT_L(DV_INFO,"%s, rbFillLen = 0x%x \n", __FUNCTION__, rbFillLen);
			DBG_PRINT_L(DV_INFO,"%s, rbMdLimit = 0x%x \n", __FUNCTION__, rbMdLimit);
			DBG_PRINT_L(DV_INFO,"%s, g_pMd_rbh->bufferID=0x%x \n", __FUNCTION__, Swap4Bytes(g_pMd_rbh->bufferID));
			DBG_PRINT_L(DV_INFO,"%s, g_pMd_rbh->writePtr=0x%x \n", __FUNCTION__, Swap4Bytes(g_pMd_rbh->writePtr));
			DBG_PRINT_L(DV_INFO,"%s, g_pMd_rbh->readPtr[0]=0x%x \n", __FUNCTION__, Swap4Bytes(g_pMd_rbh->readPtr[0]));
			DBG_PRINT_L(DV_INFO,"%s, g_pMd_rbh->beginAddr=0x%x \n", __FUNCTION__, Swap4Bytes(g_pMd_rbh->beginAddr));
			DBG_PRINT_L(DV_INFO,"%s, g_pMd_rbh->size=0x%x \n", __FUNCTION__, Swap4Bytes(g_pMd_rbh->size));

/*
			// check write pointer
			if (saveMD_wptr == 0)
				saveMD_wptr = Swap4Bytes(g_pMd_rbh->writePtr);
			else {
				if (Swap4Bytes(g_pMd_rbh->writePtr) < saveMD_wptr) {
					DBG_PRINT("%s, Error: g_pMd_rbh->writePtr=0x%x, saveMD_wptr=0x%x \n", __FUNCTION__, Swap4Bytes(g_pMd_rbh->writePtr), saveMD_wptr);
					break;
				}
			}
			if (saveIB_wptr == 0)
				saveIB_wptr = Swap4Bytes(g_pInband_rbh->writePtr);
			else {
				if (Swap4Bytes(g_pInband_rbh->writePtr) < saveIB_wptr) {
					DBG_PRINT("%s, Error: g_pInband_rbh->writePtr=0x%x, saveIB_wptr=0x%x \n", __FUNCTION__, Swap4Bytes(g_pInband_rbh->writePtr), saveIB_wptr);
					break;
				}
			}
*/

			rbFillLen = rbFillLen2 = 0;

			md_ring_wPtr = g_pMd_rbh->writePtr;

			// TODO: copy MD stream to gRpuMDAddr by avaiable size
			//if (g_pts_info.wPtr > Swap4Bytes(g_pMd_rbh->readPtr[0])) {	// no wrapping
			if (Swap4Bytes(md_ring_wPtr) > g_pts_info.wPtr) {	// no wrapping
				DBG_PRINT_L(DV_INFO,"%s, no wrapping \n", __FUNCTION__);
				DBG_PRINT_L(DV_INFO,"%s, 2 g_pts_info.wPtr=0x%x, g_pMd_rbh->writePtr=0x%x \n", __FUNCTION__, g_pts_info.wPtr, Swap4Bytes(md_ring_wPtr));

				//rbFillLen = (g_pts_info.wPtr - Swap4Bytes(g_pMd_rbh->readPtr[0]));
				rbFillLen = Swap4Bytes(md_ring_wPtr) - g_pts_info.wPtr;
				dstAddr = gRpuMDAddr;
				//srcAddr = g_Md_rb_vaddr + (Swap4Bytes(g_pMd_rbh->readPtr[0]) - Swap4Bytes(g_pMd_rbh->beginAddr));
				srcAddr = g_Md_rb_vaddr + (g_pts_info.wPtr - Swap4Bytes(g_pMd_rbh->beginAddr));
				if ((rbFillLen + dstAddr) > (gRpuMDAddr + RPU_MD_SIZE))
					rbFillLen = (gRpuMDAddr + RPU_MD_SIZE) - dstAddr;

				DBG_PRINT_L(DV_INFO,"%s, dstAddr=0x%x \n", __FUNCTION__, dstAddr);
				DBG_PRINT_L(DV_INFO,"%s, srcAddr=0x%x \n", __FUNCTION__, srcAddr);
				DBG_PRINT_L(DV_INFO,"%s, rbFillLen=0x%x \n", __FUNCTION__, rbFillLen);

				memcpy((void *)dstAddr, (void *)srcAddr,	rbFillLen);

			} else {	// rb wrapping
				DBG_PRINT_L(DV_INFO,"%s, rb wrapping \n", __FUNCTION__);

				//rbFillLen = Swap4Bytes(g_pMd_rbh->beginAddr) + Swap4Bytes(g_pMd_rbh->size) - Swap4Bytes(g_pMd_rbh->readPtr[0]);
				rbFillLen = rbMdLimit - g_pts_info.wPtr;
				dstAddr = gRpuMDAddr;
				//srcAddr = g_Md_rb_vaddr + (Swap4Bytes(g_pMd_rbh->readPtr[0]) - Swap4Bytes(g_pMd_rbh->beginAddr));
				srcAddr = g_Md_rb_vaddr + (g_pts_info.wPtr - Swap4Bytes(g_pMd_rbh->beginAddr));
				if ((rbFillLen + dstAddr) > (gRpuMDAddr + RPU_MD_SIZE))
					rbFillLen = (gRpuMDAddr + RPU_MD_SIZE) - dstAddr;

				DBG_PRINT_L(DV_INFO,"%s, dstAddr=0x%x \n", __FUNCTION__, dstAddr);
				DBG_PRINT_L(DV_INFO,"%s, srcAddr=0x%x \n", __FUNCTION__, srcAddr);
				DBG_PRINT_L(DV_INFO,"%s, rbFillLen=0x%x \n", __FUNCTION__, rbFillLen);

				memcpy((void *)dstAddr, (void *)srcAddr,	rbFillLen);


				//rbFillLen2 = g_pts_info.wPtr - Swap4Bytes(g_pMd_rbh->beginAddr);
				rbFillLen2 = Swap4Bytes(md_ring_wPtr) - Swap4Bytes(g_pMd_rbh->beginAddr);
				//dstAddr = gRpuMDAddr + mdParserRemainSize + (Swap4Bytes(g_pMd_rbh->beginAddr) + Swap4Bytes(g_pMd_rbh->size) - Swap4Bytes(g_pMd_rbh->readPtr[0]));
				dstAddr = gRpuMDAddr + rbFillLen;
				srcAddr = g_Md_rb_vaddr;
				if ((rbFillLen2 + dstAddr) > (gRpuMDAddr + RPU_MD_SIZE))
					rbFillLen2 = (gRpuMDAddr + RPU_MD_SIZE) - dstAddr;
				DBG_PRINT_L(DV_INFO,"%s, dstAddr=0x%x \n", __FUNCTION__, dstAddr);
				DBG_PRINT_L(DV_INFO,"%s, srcAddr=0x%x \n", __FUNCTION__, srcAddr);
				DBG_PRINT_L(DV_INFO,"%s, rbFillLen2=0x%x \n", __FUNCTION__, rbFillLen2);

				memcpy((void *)dstAddr, (void *)srcAddr,	rbFillLen2);
			}

			IDR_cur_flag = VRPC_PTS2_FLAG_IDR_FRAME & g_pts_info.flag;
			if((IDR_cur_flag != 0) || (current_dv_picmode == DV_PIC_NONE_MODE))
			{//Means IDR happen .Need to change picture mode and back light.
				//(current_dv_picmode == DV_PIC_NONE_MODE) means first time
				unsigned char force_bl_update = FALSE;
				if((ui_dv_picmode != current_dv_picmode))
				{
					if(ui_dv_picmode == DV_PIC_DARK_MODE)
						p_dm_cfg = &dm_dark_cfg;
					else if(ui_dv_picmode == DV_PIC_BRIGHT_MODE)
						p_dm_cfg = &dm_bright_cfg;
					else if(ui_dv_picmode == DV_PIC_VIVID_MODE)
						p_dm_cfg = &dm_vivid_cfg;
					else {
						pr_err("\r\n DV_MD_ThreadProcess Picture mode not found \r\n");
						p_dm_cfg = &dm_dark_cfg;
					}
					current_dv_picmode = ui_dv_picmode;
					force_bl_update = TRUE;//Let back light force update. Avoid use the new picture the backlight wrong
#ifndef RTK_EDBEC_1_5
					CommitDmCfg(p_dm_cfg, OTT_h_dm);
#endif
					OTT_Parser_NeedUpdate = TRUE;//Parse MD data update
				}
				if((ui_dv_backlight_value != current_dv_backlight_value) || force_bl_update)
				{
					current_dv_backlight_value = ui_dv_backlight_value;
#ifndef RTK_EDBEC_1_5
					handleDoViBacklight(&OTT_h_dm->dmCfg, current_dv_backlight_value, DOLBY_SRC_OTT, &OTT_backlightSliderScalar);
#endif
					//calculate_backlight_setting_for_cfg(p_dm_cfg, current_dv_backlight_value, DOLBY_SRC_OTT);
					OTT_Parser_NeedUpdate = TRUE;//Parse MD data update
				}
			}
			ptr = (unsigned short *)gRpuMDAddr;
#ifdef SUPPORT_HDR10_TO_MD
			if(ptr[0] != 0x2379){
#endif
			OTT_Parser_NeedUpdate = TRUE;//Parse MD data update
			// TODO: copy MD(rpu data) fill into MD parser
			mdParserDoneSize = (unsigned int)metadata_parser_main((unsigned char *)gRpuMDAddr, rbFillLen+rbFillLen2, g_MD_parser_state);
#ifdef SUPPORT_HDR10_TO_MD
			}
			else{
				//DBG_PRINT_L(DV_INFO,"\n===========  HDR 10 =========== rbFillLen+rbFillLen2=%x ptr[0]=%08x\n",rbFillLen+rbFillLen2,ptr[0]);
				/*151 has no hdr10 parser*/
				//hdr10_to_md_parser((void*)gRpuMDAddr,rbFillLen+rbFillLen2);
				mdParserDoneSize = rbFillLen+rbFillLen2;
			}
#endif
			if(mdParserDoneSize == 0)
			{
				pr_debug(KERN_DEBUG"\r\n#### func:%s line:%d ####\r\n",__FUNCTION__,__LINE__);
			}
			mdParserRemainSize = (rbFillLen + rbFillLen2) - mdParserDoneSize;

			if(g_dump_flag & DV_DUMP_RPU){
				int num,i;
				unsigned char *ptr = (unsigned char *)gRpuMDAddr;

				num = (rbFillLen + rbFillLen2);
                DBG_PRINT_L(DV_INFO,"\n===========  RPU dump ===========\n");
				for(i=0;i<num;){
					DBG_PRINT_L(DV_INFO,"%02x ",*ptr);
					i++;
					ptr++;
					if(i%16 == 0)
						DBG_PRINT_L(DV_INFO,"\n");
				}
                DBG_PRINT_L(DV_INFO,"\n===========  RPU dump end ===========\n");
			}

			if (g_MD_parser_state == MD_PARSER_INIT)
				g_MD_parser_state = MD_PARSER_RUN;
			else if (g_MD_parser_state == MD_PARSER_EOS) {
				g_pts_info.wPtr += mdParserDoneSize;

			} else {

				//printk("%s, MD Cal time=%d usec\n", __FUNCTION__, ((unsigned int)(((rtd_inl(SCPU_CLK90K_HI_reg)<<32) | rtd_inl(SCPU_CLK90K_LO_reg))-g_utime_mddeal)*1000000)/90000);
			}

#ifdef RPU_METADATA_OUTPUT
			// write back to check rpu md
			Rpu_Md_WriteBack_Check(gRpuMDAddr,
												mdParserDoneSize);
#endif
			//OTT_Parser_NeedUpdate = FALSE;//Reset OTT_Parser_NeedUpdate //Current we need to update each frame

			DBG_PRINT_L(DV_INFO,"%s, rbFillLen+rbFillLen2=0x%x \n", __FUNCTION__, rbFillLen+rbFillLen2);
			DBG_PRINT_L(DV_INFO,"%s, mdParserDoneSize=0x%x \n", __FUNCTION__, mdParserDoneSize);
			DBG_PRINT_L(DV_INFO,"%s, mdParserRemainSize=0x%x \n", __FUNCTION__, mdParserRemainSize);


			/* copy the remaining byte which is NOT complete */
			/*memcpy(gRpuMDAddr, gRpuMDAddr + mdParserDoneSize, mdParserRemainSize);*/

			DBG_PRINT_L(DV_INFO,"%s, -gRpuMDAddr[0]=0x%x \n", __FUNCTION__, *(unsigned char *)(gRpuMDAddr));
			DBG_PRINT_L(DV_INFO,"%s, -gRpuMDAddr[1]=0x%x \n", __FUNCTION__, *(unsigned char *)(gRpuMDAddr+1));
			DBG_PRINT_L(DV_INFO,"%s, -gRpuMDAddr[2]=0x%x \n", __FUNCTION__, *(unsigned char *)(gRpuMDAddr+2));
			DBG_PRINT_L(DV_INFO,"%s, -gRpuMDAddr[3]=0x%x \n", __FUNCTION__, *(unsigned char *)(gRpuMDAddr+3));


			// Update readptr
			//ICQ_UpdateReadPtr(&rbMdCmd, mdParserDoneSize);

			// read pointer workaround
			//p = Swap4Bytes(rbMdCmd.read) + mdParserDoneSize;
			//rbMdCmd.read = p < rbMdCmd.limit ? Swap4Bytes(p) : Swap4Bytes(p - (rbMdCmd.limit - rbMdCmd.base));

		} else
			msleep(1);

		up(&g_dv_pq_sem);

	}

	return DV_SUCCESS;
}

int DV_RingBuffer_Init(DOLBYVISION_INIT_STRUCT *data)
{
	extern unsigned int g_bl_OTT_apply_delay;
	char runthread1[] = "dvRunThread";
	//DOLBYVISIONEDR_DEV *dvEDR_devices = &dolbyvisionEDR_devices[0];
	unsigned int mapsize = 0;
    unsigned int src_type=0;
	DBG_PRINT_L(DV_INFO,"%s, init... \n", __FUNCTION__);

	/* clear/stop pre-proces if it is exist */
	DV_Pause();
	DV_Flush();
	DV_Stop();
#if (defined(CONFIG_SUPPORT_SCALER))
	// set HDR mode to scaler
	set_OTT_HDR_mode(HDR_DOLBY_COMPOSER);
#endif
	down(&g_dv_sem);

	// enable DM & Composer clock
#if 1
	CRT_CLK_OnOff(DOLBY_HDR, CLK_ON, &src_type);
#else
	hw_semaphore_try_lock();
	rtd_outl(SYS_REG_SYS_CLKEN1_reg, rtd_inl(SYS_REG_SYS_CLKEN1_reg) |
		SYS_REG_SYS_CLKEN1_clken_dolby_dm_mask |
		SYS_REG_SYS_CLKEN1_clken_dolby_comp_mask);
	hw_semaphore_unlock();
#endif

#ifdef CONFIG_ENABLE_DOLBY_VISION_HDMI_AUTO_DETECT
	dolby_ott_dm_init();//dm dolby initial
#endif
	//g_pInband_rbh = (data->inband_rbh_addr + CONFIG_PAGE_OFFSET);
	//g_Inband_rb_vaddr = Swap4Bytes(g_pInband_rbh->beginAddr) + CONFIG_PAGE_OFFSET;
	g_pInband_rbh = dvr_remap_uncached_memory(data->inband_rbh_addr,
		(PAGE_SIZE>sizeof(RINGBUFFER_HEADER))?PAGE_SIZE:sizeof(RINGBUFFER_HEADER),
		__builtin_return_address(0));
	g_Inband_rb_vaddr = (volatile unsigned int)dvr_remap_uncached_memory(Swap4Bytes(g_pInband_rbh->beginAddr),
		(PAGE_SIZE>Swap4Bytes(g_pInband_rbh->size))?PAGE_SIZE:Swap4Bytes(g_pInband_rbh->size),
		__builtin_return_address(0));

	printk(KERN_EMERG "\033[0;32;31m@@@Dolby OTT Video start@@@\n\033[0m");
	
	DBG_PRINT_L(DV_INFO,"g_pInband_rbh->bufferID=0x%x \n", (unsigned int)Swap4Bytes(g_pInband_rbh->bufferID));
	DBG_PRINT_L(DV_INFO,"g_pInband_rbh->writePtr=0x%x \n", (unsigned int)Swap4Bytes(g_pInband_rbh->writePtr));
	DBG_PRINT_L(DV_INFO,"g_pInband_rbh->readPtr[0]=0x%x \n", (unsigned int)Swap4Bytes(g_pInband_rbh->readPtr[0]));
	DBG_PRINT_L(DV_INFO,"g_pInband_rbh->beginAddr=0x%x \n", (unsigned int)Swap4Bytes(g_pInband_rbh->beginAddr));
	DBG_PRINT_L(DV_INFO,"g_pInband_rbh->size=0x%x \n", (unsigned int)Swap4Bytes(g_pInband_rbh->size));
	DBG_PRINT_L(DV_INFO,"g_Inband_rb_vaddr=0x%x \n", g_Inband_rb_vaddr);


	//g_pMd_rbh = (data->md_rbh_addr + CONFIG_PAGE_OFFSET);
	//g_Md_rb_vaddr = Swap4Bytes(g_pMd_rbh->beginAddr) + CONFIG_PAGE_OFFSET;
	g_pMd_rbh = dvr_remap_uncached_memory(data->md_rbh_addr,
		(PAGE_SIZE>sizeof(RINGBUFFER_HEADER))?PAGE_SIZE:sizeof(RINGBUFFER_HEADER),
		__builtin_return_address(0));
	g_Md_rb_vaddr = (volatile unsigned int)dvr_remap_uncached_memory(Swap4Bytes(g_pMd_rbh->beginAddr),
		(PAGE_SIZE>Swap4Bytes(g_pMd_rbh->size))?PAGE_SIZE:Swap4Bytes(g_pMd_rbh->size),
		__builtin_return_address(0));
	DBG_PRINT_L(DV_INFO,"g_pMd_rbh->bufferID=0x%x \n", (unsigned int)Swap4Bytes(g_pMd_rbh->bufferID));
	DBG_PRINT_L(DV_INFO,"g_pMd_rbh->writePtr=0x%x \n", (unsigned int)Swap4Bytes(g_pMd_rbh->writePtr));
	DBG_PRINT_L(DV_INFO,"g_pMd_rbh->readPtr[0]=0x%x \n", (unsigned int)Swap4Bytes(g_pMd_rbh->readPtr[0]));
	DBG_PRINT_L(DV_INFO,"g_pMd_rbh->beginAddr=0x%x \n", (unsigned int)Swap4Bytes(g_pMd_rbh->beginAddr));
	DBG_PRINT_L(DV_INFO,"g_pMd_rbh->size=0x%x \n", (unsigned int)Swap4Bytes(g_pMd_rbh->size));
	DBG_PRINT_L(DV_INFO,"g_Md_rb_vaddr=0x%x \n", g_Md_rb_vaddr);

	//g_Md_output_buf_vaddr = (data->md_output_buf_addr + CONFIG_PAGE_OFFSET);
	if (PAGE_SIZE > data->md_output_buf_size)
		mapsize = PAGE_SIZE;
	else {
		mapsize = data->md_output_buf_size / PAGE_SIZE;
		if (data->md_output_buf_size % PAGE_SIZE)
			mapsize++;

		mapsize = mapsize * PAGE_SIZE;
		DBG_PRINT_L(DV_INFO,"%s, mapsize=0x%x \n", __FUNCTION__, mapsize);
	}
	g_Md_output_buf_vaddr = (volatile unsigned int)dvr_remap_uncached_memory(data->md_output_buf_addr,
		mapsize,
		__builtin_return_address(0));
	memset((void *)g_Md_output_buf_vaddr, 0x00, data->md_output_buf_size);
	g_Md_output_buf_phyaddr = data->md_output_buf_addr;
	DBG_PRINT_L(DV_INFO,"g_Md_output_buf_phyaddr=0x%x outputsize:%d\n", g_Md_output_buf_phyaddr, sizeof(DOLBYVISION_MD_OUTPUT));
	DBG_PRINT_L(DV_INFO,"g_Md_output_buf_vaddr=0x%x \n", g_Md_output_buf_vaddr);

	pr_debug(KERN_EMERG"g_Md_output_buf_phyaddr=0x%x outputsize:%d\n", g_Md_output_buf_phyaddr, sizeof(DOLBYVISION_MD_OUTPUT));
	pr_debug(KERN_EMERG"g_Md_output_buf_vaddr=0x%x \n", g_Md_output_buf_vaddr);

	// create polling thread
	if (g_dvRunThread == NULL)
		g_dvRunThread = kthread_create(DV_MD_ThreadProcess, NULL, runthread1);


	// local rpu MD buffer to MD parser
	if (gRpuMDAddr == 0) {
		gRpuMDCacheAddr = (unsigned long)dvr_malloc_uncached_specific(RPU_MD_SIZE, GFP_DCU2, (void **)&gRpuMDAddr);
		if (gRpuMDAddr) {
			DBG_PRINT_L(DV_INFO,"physical gRpuMDCacheAddr=0x%x\n", (unsigned int)dvr_to_phys((void *)gRpuMDCacheAddr));
			DBG_PRINT_L(DV_INFO,"uncache gRpuMDAddr=0x%x\n", gRpuMDAddr);
		}
	}

	// allocate local control struture
	if (g_MdFlow == NULL) {
		g_MdFlow = kmalloc(sizeof(DOLBYVISION_FLOW_STRUCT), GFP_KERNEL);
		if (g_MdFlow == NULL) {
			DBG_PRINT_L(DV_INFO,"%s, allocate MD Output flow variable failed \n", __FUNCTION__);

			up(&g_dv_sem);
			return DV_ERR_NO_MEMORY;
		}
	}
	memset((void *)g_MdFlow, 0x00, sizeof(DOLBYVISION_FLOW_STRUCT));
	memset(&g_pts_info, 0x00, sizeof(PTS_INFO2));
	g_MdFlow->bsSize = data->md_output_buf_size;
	g_MdFlow->pBsWrite = (unsigned int *)(g_Md_output_buf_vaddr);
	//g_MdFlow->pBsRead =
	g_MdFlow->bsBase = g_Md_output_buf_vaddr;
	g_MdFlow->bsLimit = g_Md_output_buf_vaddr + g_MdFlow->bsSize;
	//g_MdFlow->bsRead = *g_MdFlow->pBsRead;
	g_MdFlow->bsUnitSize = sizeof(DOLBYVISION_MD_OUTPUT);
	g_MdFlow->bsWrite = g_MdFlow->bsBase;


	g_MdFlow->INBAND_ICQ.pWrPtr = (unsigned long *)&g_pInband_rbh->writePtr;
	g_MdFlow->INBAND_ICQ.pRdPtr = (unsigned long *)&g_pInband_rbh->readPtr[0];
	g_MdFlow->INBAND_ICQ.base   =  Swap4Bytes(g_pInband_rbh->beginAddr);
	g_MdFlow->INBAND_ICQ.limit  =  Swap4Bytes(g_pInband_rbh->beginAddr) + Swap4Bytes(g_pInband_rbh->size);
	g_MdFlow->INBAND_ICQ.write  = Swap4Bytes(*g_MdFlow->INBAND_ICQ.pWrPtr);
	g_MdFlow->INBAND_ICQ.read  = *(g_MdFlow->INBAND_ICQ.pRdPtr);

	DBG_PRINT_L(DV_INFO,"%s, g_MdFlow->INBAND_ICQ.read=0x%x \n", __FUNCTION__, Swap4Bytes(g_MdFlow->INBAND_ICQ.read));


	g_MdFlow->state = DV_FLOW_INIT;
	g_MdFlow->state_chk = DV_FLOW_INIT;
	g_rpu_size = g_more_eos_err = g_dvframeCnt = 0;

	/* get PID */
	g_dv_task_pid = task_tgid_nr(current);
	DBG_PRINT_L(DV_INFO,"%s, g_dv_task_pid=%d \n", __FUNCTION__, g_dv_task_pid);

	// reset h_dm's mdsExt
	//baker mark this , cuz' need init_cp->InitMdsExt's trim  pointer
	//memset(&OTT_h_dm->h_mds_ext, 0x00, sizeof(MdsExt_t));

	/* check .bcms file in file system & PQ parameters update */
#if 0
#if EN_GLOBAL_DIMMING
	GMLUT_3DLUT_Update("/tmp/usb/sda/sda1/default.bcms", 0, 0);
	GMLUT_3DLUT_Update("/tmp/usb/sda/sda1/gdlut_a.bcms", 0, 1);
	GMLUT_3DLUT_Update("/tmp/usb/sda/sda1/gdlut_b.bcms", 0, 2);
	GMLUT_Pq2GLut_Update("/tmp/usb/sda/sda1/PQ2GammaLUT.bin", 0, (4 + 1024*4 + 16*3) * sizeof(int32_t));
	Backlight_DelayTiming_Update("/tmp/usb/sda/sda1/backlight_delay.bin", 0);
	Backlight_LUT_Update("/tmp/usb/sda/sda1/backlightLUT.bin", 0);
	printk(KERN_INFO"%s, EN_GLOBAL_DIMMING to update GD_LUT \n", __FUNCTION__);
#else
	GMLUT_3DLUT_Update("/tmp/usb/sda/sda1/default.bcms", 0, 0);
#endif
#endif

	//DolbyPQ_1Model_1File_Config(g_PicModeFilePath[g_dv_picmode], g_dv_picmode);//MArk by Will

	g_forceUpdate_3DLUT = g_forceUpdateFirstTime = 1;

	/*
	*  adjust PQ parameters by dv_pq_para.bin which has seven parameters
	*  e.g.
	    TminPQ = 62				(2 bytes)
		TmaxPQ = 2771			(2 bytes)
		Tgamma = 36045			(2 bytes)
		TEOTF = 0				(4 bytes)
		TBitDepth = 12			(4 bytes)
		TSignalRange = 1		(4 bytes)
		Tdiagonalinches = 42	(2 bytes)
	*/
	//Target_Display_Parameters_Update("/tmp/usb/sda/sda1/dv_pq_para.bin", 0);
	//CommitDmCfg(p_dm_cfg, OTT_h_dm);//Mark by will


	g_mdparser_output_type = 0;	// normal flow use
	//g_mdparser_output_type = 1;	// metadata output for certification case


	if (g_mdparser_output_type == 1) {
		if (g_dmMDAddr == 0) {
			g_dmMDAddr = (unsigned int)dvr_malloc_specific(RPU_MD_SIZE*8, GFP_DCU2);
			if (g_dmMDAddr)
				DBG_PRINT_L(DV_INFO,"physical g_dmMDAddr=0x%x\n", (unsigned int)dvr_to_phys((void *)g_dmMDAddr));
		}
		if (g_compMDAddr == 0) {
			g_compMDAddr = (unsigned int)dvr_malloc_specific(RPU_MD_SIZE*8, GFP_DCU2);
			if (g_compMDAddr)
				DBG_PRINT_L(DV_INFO,"physical g_compMDAddr=0x%x\n", (unsigned int)dvr_to_phys((void *)g_compMDAddr));
		}
	}

	// turn on backlight to full bright in default
	g_lvl4_exist = 0;
#if EN_GLOBAL_DIMMING
#if (defined(CONFIG_RTK_KDRV_PWM))
	if (0 != rtk_pwm_backlight_set_duty(255)) { /*20171226, baker recover for dolby backlight control*/
		DBG_PRINT_L(DV_INFO,"%s, PWM control failed...duty=%d \n", __FUNCTION__, 255);
	}
#endif
#endif
#if (defined(CONFIG_SUPPORT_SCALER))
#ifdef IDK_CRF_USE
	// for Sub-cap
	extern unsigned char rtk_hal_vsc_Connect(VIDEO_WID_T wid, VSC_INPUT_SRC_INFO_T inputSrcInfo, VSC_OUTPUT_MODE_T outputMode);
	extern unsigned char rtk_hal_vsc_open(VIDEO_WID_T wid);

	VSC_INPUT_SRC_INFO_T inputSrcInfo;
	inputSrcInfo.type = VSC_INPUTSRC_VDEC;
	inputSrcInfo.attr = 0;
	inputSrcInfo.resourceIndex = 0;

	rtk_hal_vsc_open(VIDEO_WID_1);
	rtk_hal_vsc_Connect(VIDEO_WID_1, inputSrcInfo, 0);

	g_forceB05Update_IDK = 0;
#endif
#endif

	up(&g_dv_sem);
	pr_notice("\r\n PWM Delay time OTT:%d\r\n", g_bl_OTT_apply_delay);
	set_dolby_init_flag(TRUE);
	return DV_SUCCESS;
}

int DV_HDMI_Init(void)
{
	extern unsigned int g_bl_HDMI_apply_delay;
	unsigned int src_type=0;
	down(&g_dv_pq_sem);
	// copy to HDMI dm_hdmi_cfg
	if(ui_dv_picmode == DV_PIC_BRIGHT_MODE)
		p_dm_hdmi_cfg = &dm_hdmi_bright_cfg;
	else if(ui_dv_picmode == DV_PIC_VIVID_MODE)
		p_dm_hdmi_cfg = &dm_hdmi_vivid_cfg;
	else
		p_dm_hdmi_cfg = &dm_hdmi_dark_cfg;
#ifndef RTK_EDBEC_1_5
	CommitDmCfg(p_dm_hdmi_cfg, hdmi_h_dm);//Avoid the the same picture mode don't set this. Green video
#endif
	g_picModeUpdateFlag = 3;
	pr_notice("\r\n PWM Delay time HDMI:%d\r\n", g_bl_HDMI_apply_delay);
	up(&g_dv_pq_sem);
	HDMI_last_md_picmode = DV_PIC_NONE_MODE;//Reset
	HDMI_last_md_backlight = 0xff;//Reset

	down(&g_dv_sem);

	printk(KERN_INFO"%s, init... \n", __FUNCTION__);
#if 1
	CRT_CLK_OnOff(DOLBY_HDR, CLK_ON, &src_type);
#else
	// enable DM clock
	hw_semaphore_try_lock();
	rtd_outl(SYS_REG_SYS_CLKEN1_reg, rtd_inl(SYS_REG_SYS_CLKEN1_reg) |
												SYS_REG_SYS_CLKEN1_clken_dolby_dm_mask |
												SYS_REG_SYS_CLKEN1_clken_dolby_comp_mask);
	hw_semaphore_unlock();
#endif

	/* check .bcms file in file system & PQ parameters update */
	//DolbyPQ_1Model_1File_Config(g_PicModeFilePath[g_dv_picmode], g_dv_picmode);//Mark by Will

	g_forceUpdate_3DLUT = 1;
	g_hdmi_lastIdxRd = 0xfff;
	hdmi_dolby_last_pair_num = 0xff;
	hdmi_dolby_cur_pair_num = 0;


	// turn on backlight to full bright in default
	g_lvl4_exist = 0;
#if EN_GLOBAL_DIMMING
#if (defined(CONFIG_RTK_KDRV_PWM))
	if (0 != rtk_pwm_backlight_set_duty(255)) { /*20171226, baker recover for dolby backlight control*/
		DBG_PRINT_L(DV_INFO,"%s, PWM control failed...duty=%d \n", __FUNCTION__, 255);
	}
#endif
#endif
	up(&g_dv_sem);

	return DV_SUCCESS;
}


int DV_Run(void)
{

	if (g_MdFlow == NULL) {
		DBG_PRINT_L(DV_INFO,"%s, get null pointer!!  \n", __FUNCTION__);
		return DV_ERR_RUN_FAIL;
	}
	down(&g_dv_pq_sem);
	Reset_Pic_and_Backlight();
	up(&g_dv_pq_sem);
	down(&g_dv_sem);

	DBG_PRINT_L(DV_INFO,"%s, run... \n", __FUNCTION__);

	DBG_PRINT_L(DV_INFO,"%s, g_pMd_rbh->bufferID=0x%x \n", __FUNCTION__, Swap4Bytes(g_pMd_rbh->bufferID));
	DBG_PRINT_L(DV_INFO,"%s, g_pMd_rbh->writePtr=0x%x \n", __FUNCTION__, Swap4Bytes(g_pMd_rbh->writePtr));
	DBG_PRINT_L(DV_INFO,"%s, g_pMd_rbh->readPtr[0]=0x%x \n", __FUNCTION__, Swap4Bytes(g_pMd_rbh->readPtr[0]));
	DBG_PRINT_L(DV_INFO,"%s, g_pMd_rbh->beginAddr=0x%x \n", __FUNCTION__, Swap4Bytes(g_pMd_rbh->beginAddr));
	DBG_PRINT_L(DV_INFO,"%s, g_pMd_rbh->size=0x%x \n", __FUNCTION__, Swap4Bytes(g_pMd_rbh->size));

	if (g_MdFlow->state == DV_FLOW_RUN) {
		// run already
		up(&g_dv_sem);
		return DV_SUCCESS;
	}

	if (g_MdFlow)
		g_MdFlow->state = DV_FLOW_RUN;

	g_MD_parser_state = MD_PARSER_INIT;
	g_lvl4_exist = 0;

	if (g_dvRunThread) {
		wake_up_process(g_dvRunThread);
	} else {
		DBG_PRINT_L(DV_INFO,"%s, run thread created failed \n", __FUNCTION__);
	}

	up(&g_dv_sem);

	return DV_SUCCESS;
}


int DV_Stop(void)
{
	//DOLBYVISIONEDR_DEV *dvEDR_devices = &dolbyvisionEDR_devices[0];
	unsigned int mapsize = 0;

	sys_reg_sys_clken1_RBUS sys_clken1_reg;

	DBG_PRINT_L(DV_INFO,"%s, stop... \n", __FUNCTION__);

	down(&g_dv_pq_sem);//in order to sleep at DV_MD_ThreadProcess
	up(&g_dv_pq_sem);

	down(&g_dv_sem);
	if (g_mdparser_output_type == 1) {
		//flush_cache_all();
		MetaData_WriteBack_Check();
	}

	if (g_MdFlow)
		g_MdFlow->state = DV_FLOW_STOP;


	if (g_dvRunThread) {
		kthread_stop(g_dvRunThread);
		g_dvRunThread = NULL;
	}

	// free remap memory (vmemory)
	if (g_Inband_rb_vaddr)
		dvr_unmap_memory((void *)g_Inband_rb_vaddr, (PAGE_SIZE>Swap4Bytes(g_pInband_rbh->size))?PAGE_SIZE:Swap4Bytes(g_pInband_rbh->size));
	if (g_pInband_rbh) {
		dvr_unmap_memory((void *)g_pInband_rbh, (PAGE_SIZE>sizeof(RINGBUFFER_HEADER))?PAGE_SIZE:sizeof(RINGBUFFER_HEADER));
		g_pInband_rbh = NULL;
	}
	if (g_Md_rb_vaddr)
		dvr_unmap_memory((void *)g_Md_rb_vaddr, (PAGE_SIZE>Swap4Bytes(g_pMd_rbh->size))?PAGE_SIZE:Swap4Bytes(g_pMd_rbh->size));
	if (g_pMd_rbh) {
		dvr_unmap_memory((void *)g_pMd_rbh, (PAGE_SIZE>sizeof(RINGBUFFER_HEADER))?PAGE_SIZE:sizeof(RINGBUFFER_HEADER));
		g_pMd_rbh = NULL;
	}

	if (g_Md_output_buf_vaddr && g_MdFlow) {
		if (PAGE_SIZE > g_MdFlow->bsSize)
			mapsize = PAGE_SIZE;
		else {
			mapsize = g_MdFlow->bsSize / PAGE_SIZE;
			if (g_MdFlow->bsSize % PAGE_SIZE)
				mapsize++;

			mapsize = mapsize * PAGE_SIZE;
			DBG_PRINT_L(DV_INFO,"%s, mapsize=0x%x \n", __FUNCTION__, mapsize);
		}
		dvr_unmap_memory((void *)g_Md_output_buf_vaddr, mapsize);
	}


	/* free by kmalloc *///need to check
	if (g_MdFlow) {
		memset((void *)g_MdFlow, 0x00, sizeof(DOLBYVISION_FLOW_STRUCT));
		kfree((void *)g_MdFlow);
		g_MdFlow = NULL;
	}


	g_Inband_rb_vaddr = 0;
	g_Md_rb_vaddr = 0;
	g_Md_output_buf_vaddr = g_Md_output_buf_phyaddr = 0;
	g_rpu_size = g_more_eos_err = g_dvframeCnt = 0;
	g_mdparser_output_type = -1;
	g_lvl4_exist = g_dv_task_pid = 0;

#ifdef RPU_METADATA_OUTPUT
	extern struct file *gFrame2Outfp;
	if (gFrame2Outfp)
		filp_close(gFrame2Outfp, NULL);
	gFrame2Outfp = NULL;
#endif
	sys_clken1_reg.regValue = rtd_inl(SYS_REG_SYS_CLKEN1_reg);

//#if 1
//	CRT_CLK_OnOff(DOLBY_HDR, CLK_OFF, &src_type);
#if 0//#else
	// disable DM & Composer clock
	hw_semaphore_try_lock();
	rtd_outl(SYS_REG_SYS_CLKEN1_reg, rtd_inl(SYS_REG_SYS_CLKEN1_reg) &
												~(SYS_REG_SYS_CLKEN1_clken_dolby_dm_mask |
												SYS_REG_SYS_CLKEN1_clken_dolby_comp_mask));
	hw_semaphore_unlock();
#endif

#ifdef CONFIG_SUPPORT_SCALER
	// set HDR mode to scaler
	set_OTT_HDR_mode(HDR_MODE_DISABLE);
#endif

#ifdef IDK_CRF_USE
	extern unsigned char rtk_hal_vsc_SetWinBlank(VIDEO_WID_T wid, bool bonoff, VIDEO_DDI_WIN_COLOR_T color);
	extern unsigned char rtk_hal_vsc_Disconnect(VIDEO_WID_T wid, VSC_INPUT_SRC_INFO_T inputSrcInfo, VSC_OUTPUT_MODE_T outputMode);
	extern unsigned char rtk_hal_vsc_close(VIDEO_WID_T wid);

	VSC_INPUT_SRC_INFO_T inputSrcInfo;
	inputSrcInfo.type = VSC_INPUTSRC_VDEC;
	inputSrcInfo.attr = 0;
	inputSrcInfo.resourceIndex = 0;

	//rtk_hal_vsc_SetWinBlank(VIDEO_WID_1, TRUE, VIDEO_DDI_WIN_COLOR_BLACK);
	rtk_hal_vsc_Disconnect(VIDEO_WID_1, inputSrcInfo, 0);
	rtk_hal_vsc_close(VIDEO_WID_1);
#endif

	up(&g_dv_sem);

	printk(KERN_EMERG "\033[0;32;31m@@@Dolby OTT Video finish@@@\n\033[0m");
	return DV_SUCCESS;
}


int DV_Pause(void)
{
	unsigned long timeout = jiffies + ((HZ/5)*4);		/* 800 ms */

	DBG_PRINT_L(DV_INFO,"%s, pause... \n", __FUNCTION__);
	down(&g_dv_sem);

	if (g_MdFlow)
		g_MdFlow->state = DV_FLOW_PAUSE;


	// wait process get in pause state
	if (g_MdFlow) {
		while (g_MdFlow->state_chk != DV_FLOW_PAUSE) {

			if (time_before(jiffies, timeout) == 0) {	/* over 800 ms */
				DBG_PRINT_L(DV_INFO,"%s, Time-out (over 800ms) \n", __FUNCTION__);
				break;
			}
			msleep(1);
		}
	}

	up(&g_dv_sem);

	return DV_SUCCESS;
}


int DV_Flush(void)
{
	unsigned int idx, max_idx = 0;
	DOLBYVISION_MD_OUTPUT *mdptr;
	DBG_PRINT_L(DV_INFO,"%s, flush... \n", __FUNCTION__);
	down(&g_dv_sem);

	if (g_MdFlow) {

		//if (g_Md_output_buf_vaddr)
			//memset((void *)g_Md_output_buf_vaddr, 0x00, g_MdFlow->bsSize);
		if(g_MdFlow->bsUnitSize)
			max_idx = g_MdFlow->bsSize/g_MdFlow->bsUnitSize;
		mdptr = (DOLBYVISION_MD_OUTPUT *)g_Md_output_buf_vaddr;
		for (idx = 0; idx < max_idx; idx++) {
			if (mdptr->own_bit2 == 0)
			{//if mdptr->own_bit2 != 0 means vo keep the buffer. We can not reset it
				mdptr->own_bit = 0;
			}
			mdptr++;
		}


		memset(&g_pts_info, 0x00, sizeof(PTS_INFO2));

/*
		// reset inband command w/r pointer
		if (g_MdFlow->INBAND_ICQ.pRdPtr && g_MdFlow->INBAND_ICQ.pWrPtr)
			g_MdFlow->INBAND_ICQ.write = *g_MdFlow->INBAND_ICQ.pRdPtr = *g_MdFlow->INBAND_ICQ.pWrPtr;
*/
		g_pInband_rbh->readPtr[0] = g_pInband_rbh->writePtr;//read point to write point
		g_MdFlow->INBAND_ICQ.pWrPtr = (unsigned long *)&g_pInband_rbh->writePtr;//For parse use
		g_MdFlow->INBAND_ICQ.pRdPtr = (unsigned long *)&g_pInband_rbh->readPtr[0];//For parse use
		g_MdFlow->INBAND_ICQ.write  = Swap4Bytes(*g_MdFlow->INBAND_ICQ.pWrPtr);//No use
		g_MdFlow->INBAND_ICQ.read  = *(g_MdFlow->INBAND_ICQ.pRdPtr);//No use

		// reset MD ring buffer w/r pointer
		g_pMd_rbh->readPtr[0] = g_pMd_rbh->writePtr;
	}

	g_more_eos_err = 0;

	//if (g_Md_rb_vaddr)
	//	Rpu_Md_WriteBack_Check(g_Md_rb_vaddr,
	//										Swap4Bytes(g_pMd_rbh->size));

	up(&g_dv_sem);

	return DV_SUCCESS;
}


/** @brief force stop thread while application process was killed
*  @param process_id proccess ID from VDEC driver
*  @return None
*/
int DV_ForceSTOP(unsigned long process_id)
{
	if (process_id == g_dv_task_pid) {
		DV_Pause();
		DV_Stop();
		DBG_PRINT_L(DV_INFO,"%s, done... \n", __FUNCTION__);
	}

	return 0;
}

void DV_HDR10_MD_Output_Gen(unsigned char *mdAddr, unsigned char *dm_out, unsigned char *comp_out)
{

#if 0
	unsigned int wSize = 0;
	MdsChg_t mdsChg;
	DOLBYVISION_PATTERN dolby;
	DmExecFxp_t *pDmExec = &OTT_h_dm->dmExec;
	uint16_t *testptr;


	dolby.ColNumTtl = 1920;
	dolby.RowNumTtl = 1080;
	dolby.BL_EL_ratio = 0;



	rtk_wrapper_commit_reg(OTT_h_dm,
		FORMAT_HDR10,
		(dm_metadata_t *)mdAddr,
		(rpu_ext_config_fixpt_main_t *)comp_out,
		dlbPqConfig, 0);

	rtk_dm_exec_params_to_DmExecFxp(OTT_h_dm->h_ks,
		&OTT_h_dm->dmExec,
		&OTT_h_dm->dmExec_rtk,
		(MdsExt_t *)mdAddr,
		&OTT_h_dm->dm_lut,
		dlbPqConfig,
		RUN_MODE_NORMAL);

	//lut3D B05 table
	memcpy(OTT_h_dm->dmExec.bwDmLut.lutMap,OTT_h_dm->dm_lut.lut3D,3*GMLUT_MAX_DIM*GMLUT_MAX_DIM*GMLUT_MAX_DIM*sizeof(uint16_t));
	DBG_PRINT_L(DV_INFO,"lut3D [%08x] [%08x] [%08x] [%08x]\n", OTT_h_dm->dm_lut.lut3D[0],OTT_h_dm->dm_lut.lut3D[1],OTT_h_dm->dm_lut.lut3D[2],OTT_h_dm->dm_lut.lut3D[3]);
	//commit_reg() ??

	DBG_PRINT_L(DV_INFO,"%s, mdsChg=%d \n", __FUNCTION__, mdsChg);
	DBG_PRINT_L(DV_INFO,"0 dm_out=0x%x \n", dm_out);


	// write to MD output buffer
	wSize = sizeof(DmExecFxp_MdOutput_part1_t);

	memcpy(dm_out, (unsigned char *)&(pDmExec->implMethod), wSize);
	dm_out += wSize;

	DBG_PRINT_L(DV_INFO,"1 dm_out=0x%x, wSize=0x%x\n", dm_out, wSize);

	wSize = (DEF_FXP_TC_LUT_SIZE * sizeof(uint16_t));
	memcpy(dm_out, (unsigned char *)&OTT_h_dm->dm_lut.tcLut, wSize);

	testptr = (uint16_t *)dm_out;

	DBG_PRINT_L(DV_INFO,"OTT_h_dm->dm_lut.tcLut[0]=0x%x \n", OTT_h_dm->dm_lut.tcLut[0]);
	DBG_PRINT_L(DV_INFO,"OTT_h_dm->dm_lut.tcLut[500]=0x%x \n", OTT_h_dm->dm_lut.tcLut[500]);
	DBG_PRINT_L(DV_INFO,"dm_out[0]=0x%x \n", testptr[0]);
	DBG_PRINT_L(DV_INFO,"dm_out[500]=0x%x \n", testptr[500]);

	dm_out += wSize;

	DBG_PRINT_L(DV_INFO,"2 dm_out=0x%x, wSize=0x%x\n", dm_out, wSize);

	wSize = sizeof(DmExecFxp_MdOutput_part3_t);


	memcpy(dm_out, (unsigned char *)&(pDmExec->msFilterScale), wSize);

	DBG_PRINT_L(DV_INFO,"3 dm_out=0x%x, wSize=0x%x\n", dm_out, wSize);

	flush_cache_all();	// take too such time??

#endif
}

void rtk_self_check(unsigned int n,unsigned char* out, DmExecFxp_t_rtk* golden){

	DmExecFxp_MdOutput_part1_t* dmout = NULL;
	uint16_t* tmplut = NULL;
	DmExecFxp_MdOutput_part3_t* dmout3 = NULL;
	int coni=0,conj=0;



	// for memcpy(dm_out, (unsigned char *)&(pDmExec_rtk->runMode), wSize);
	dmout = (DmExecFxp_MdOutput_part1_t*)out;
	if(dmout->runMode != golden->runMode){
		printk(KERN_ERR"runMode error\n");
	}
	if(dmout->colNum != golden->colNum){
		printk(KERN_ERR"colNum error\n");
	}

	if(dmout->rowNum != golden->rowNum){
		printk(KERN_ERR"rowNum error\n");
	}
	if(dmout->in_clr != golden->in_clr){
		printk(KERN_ERR"in_clr error\n");
	}
	if(dmout->in_bdp != golden->in_bdp){
		printk(KERN_ERR"in_bdp error\n");
	}

	if(dmout->in_chrm != golden->in_chrm){
		printk(KERN_ERR"in_chrm error\n");
	}

	if(dmout->sRangeMin != golden->sRangeMin){
		printk(KERN_ERR"sRangeMin error\n");
	}
	if(dmout->sRange != golden->sRange){
		printk(KERN_ERR"sRange error\n");
	}
	if(dmout->sRangeInv != golden->sRangeInv){
		printk(KERN_ERR"sRangeInv error\n");
	}

	if(dmout->m33Yuv2RgbScale2P != golden->m33Yuv2RgbScale2P){
		printk(KERN_ERR"m33Yuv2RgbScale2P error\n");
	}

	for(coni=0;coni<=2;coni++) {
		for(conj=0;conj<=2;conj++) {
			if(dmout->m33Yuv2Rgb[coni][conj] != golden->m33Yuv2Rgb[coni][conj]){
				printk(KERN_ERR"m33Yuv2Rgb error\n");
			}
		}
	}

	for(coni=0;coni<=2;coni++) {
		if(dmout->v3Yuv2RgbOffInRgb[coni] != golden->v3Yuv2RgbOffInRgb[coni]){
			printk(KERN_ERR"v3Yuv2RgbOffInRgb error\n");
		}
	}

	for(coni=0;coni<DEF_G2L_LUT_SIZE;coni++) {
		if(dmout->g2L[coni] != golden->g2L[coni]){
			printk(KERN_ERR"g2L error\n");
		}
	}

	if(dmout->sEotf != golden->sEotf){
		printk(KERN_ERR"sEotf error\n");
	}

	if(dmout->sA != golden->sA){
		printk(KERN_ERR"sA error\n");
	}

	if(dmout->sB != golden->sB){
		printk(KERN_ERR"sB error\n");
	}

	if(dmout->sGamma != golden->sGamma){
		printk(KERN_ERR"sGamma error\n");
	}

	if(dmout->sG != golden->sG){
		printk(KERN_ERR"sG error\n");
	}

	if(dmout->m33Rgb2OptScale2P != golden->m33Rgb2OptScale2P){
		printk(KERN_ERR"m33Rgb2OptScale2P error\n");
	}

	for(coni=0;coni<=2;coni++) {
		for(conj=0;conj<=2;conj++) {
			if(dmout->m33Rgb2Opt[coni][conj] != golden->m33Rgb2Opt[coni][conj]){
				printk(KERN_ERR"m33Rgb2Opt error\n");
			}
		}
	}

	if(dmout->m33Opt2OptScale2P != golden->m33Opt2OptScale2P){
		printk(KERN_ERR"m33Opt2OptScale2P error\n");
	}

	for(coni=0;coni<=2;coni++) {
		for(conj=0;conj<=2;conj++) {
			if(dmout->m33Opt2Opt[coni][conj] != golden->m33Opt2Opt[coni][conj]){
				printk(KERN_ERR"m33Opt2Opt error\n");
			}
		}
	}

	if(dmout->Opt2OptOffset != golden->Opt2OptOffset){
		printk(KERN_ERR"Opt2OptOffset error\n");
	}

	if(dmout->chromaWeight != golden->chromaWeight){
		printk(KERN_ERR"chromaWeight error\n");
	}



	//for memcpy(dm_out, (unsigned char *)pDmExec_rtk->tcLut, wSize);
	tmplut = (uint16_t*)(out+sizeof(DmExecFxp_MdOutput_part1_t));

	coni=0;
# if REDUCED_TC_LUT
	for(coni=0;coni<sizeof(golden->tcLut)/2;coni++) {
#else
	for(coni=0;coni<4096; coni++) {
#endif
		if(*(tmplut+coni) != golden->tcLut[coni]) {
			printk(KERN_ERR"tcLut[%d/%d] error\n", coni, sizeof(golden->tcLut)/2);
			break;
		}
	}

	dmout3 = (DmExecFxp_MdOutput_part3_t*)(out+sizeof(DmExecFxp_MdOutput_part1_t) + sizeof(golden->tcLut)+2);
	//dmout3 = (DmExecFxp_MdOutput_part3_t*)(out+sizeof(DmExecFxp_MdOutput_part1_t) + sizeof(golden->tcLut));
	//for memcpy(dm_out, (unsigned char *)&(pDmExec_rtk->msFilterScale), wSize);
#if 0
	if(dmout3->msFilterScale != golden->msFilterScale){
		printk(KERN_ERR"msFilterScale error\n");
	}
#endif
#if 1
//baker, the below table are all const,...need  to cpy
	for(coni=0;coni<11;coni++) {
		if(dmout3->msFilterRow[coni] != golden->msFilterRow[coni]){
			printk(KERN_ERR"msFilterRow error\n");
		}
	}

	for(coni=0;coni<5;coni++) {
		if(dmout3->msFilterCol[coni] != golden->msFilterCol[coni]){
			printk(KERN_ERR"msFilterCol error\n");
		}
	}

	for(coni=0;coni<11;coni++) {
		if(dmout3->msFilterEdgeRow[coni] != golden->msFilterEdgeRow[coni]){
			printk(KERN_ERR"msFilterEdgeRow error\n");
		}
	}
	for(coni=0;coni<5;coni++) {
		if(dmout3->msFilterEdgeCol[coni] != golden->msFilterEdgeCol[coni]){
			printk(KERN_ERR"msFilterEdgeCol error\n");
		}
	}
#endif
	if(dmout3->msWeight != golden->msWeight){
		printk(KERN_ERR"msWeight error\n");
	}

	if(dmout3->msWeightEdge != golden->msWeightEdge){
		printk(KERN_ERR"msWeightEdge error\n");
	}

	if(dmout3->bwDmLut.dimC1 != golden->bwDmLut.dimC1){
		printk(KERN_ERR"dimC1 error\n");
	}

	if(dmout3->bwDmLut.dimC2 != golden->bwDmLut.dimC2){
		printk(KERN_ERR"dimC2 error\n");
	}

	if(dmout3->bwDmLut.dimC3 != golden->bwDmLut.dimC3){
		printk(KERN_ERR"dimC3 error\n");
	}

	if(dmout3->bwDmLut.valTp != golden->bwDmLut.valTp){
		printk(KERN_ERR"valTp error\n");
	}
	if(dmout3->bwDmLut.iMinC1 != golden->bwDmLut.iMinC1){
		printk(KERN_ERR"iMinC1 error\n");
	}

	if(dmout3->bwDmLut.iMaxC1 != golden->bwDmLut.iMaxC1){
		printk(KERN_ERR"iMaxC1 error\n");
	}
	if(dmout3->bwDmLut.iMinC2 != golden->bwDmLut.iMinC2){
		printk(KERN_ERR"iMinC2 error\n");
	}

	if(dmout3->bwDmLut.iMaxC2 != golden->bwDmLut.iMaxC2){
		printk(KERN_ERR"iMaxC2 error\n");
	}
	if(dmout3->bwDmLut.iMinC3 != golden->bwDmLut.iMinC3){
		printk(KERN_ERR"iMinC3 error\n");
	}
	if(dmout3->bwDmLut.iMaxC3 != golden->bwDmLut.iMaxC3){
		printk(KERN_ERR"iMaxC3 error\n");
	}
	if(dmout3->bwDmLut.iDistC1Inv != golden->bwDmLut.iDistC1Inv){
		printk(KERN_ERR"iDistC1Inv error\n");
	}
	if(dmout3->bwDmLut.iDistC2Inv != golden->bwDmLut.iDistC2Inv){
		printk(KERN_ERR"iDistC2Inv error\n");
	}
	if(dmout3->bwDmLut.iDistC3Inv != golden->bwDmLut.iDistC3Inv){
		printk(KERN_ERR"iDistC3Inv error\n");
	}

	if(dmout3->bwDmLut.pitch != golden->bwDmLut.pitch){
		printk(KERN_ERR"pitch error\n");
	}
	if(dmout3->bwDmLut.slice != golden->bwDmLut.slice){
		printk(KERN_ERR"slice error\n");
	}

	for(coni=0;coni<3*GMLUT_MAX_DIM*GMLUT_MAX_DIM*GMLUT_MAX_DIM;coni++) {
		if(dmout3->bwDmLut.lutMap[coni] != golden->bwDmLut.lutMap[coni]){
			printk(KERN_ERR"lutMap error\n");
			break;
		}
	}
	if(dmout3->gain != golden->gain){
		printk(KERN_ERR"gain error\n");
	}
	if(dmout3->offset != golden->offset){
		printk(KERN_ERR"offset error\n");
	}
	if(dmout3->saturationGain != golden->saturationGain){
		printk(KERN_ERR"saturationGain error\n");
	}
	if(dmout3->tRangeMin != golden->tRangeMin){
		printk(KERN_ERR"tRangeMin error\n");
	}
	if(dmout3->tRange != golden->tRange){
		printk(KERN_ERR"tRange error\n");
	}
	if(dmout3->tRangeOverOne != golden->tRangeOverOne){
		printk(KERN_ERR"tRangeOverOne error, %d, %d\n", dmout3->tRangeOverOne ,golden->tRangeOverOne);
		//dmout3->tRangeOverOne = golden->tRangeOverOne;
	}

	if(dmout3->tRangeInv != golden->tRangeInv){
		printk(KERN_ERR"tRangeInv error\n");
	}
#if 0
	for(coni=0;coni<3;coni++) {
		if(dmout3->minC[coni] != golden->minC[coni]){
			printk(KERN_ERR"minC error\n");
		}
	}

	for(coni=0;coni<3;coni++) {
		if(dmout3->maxC[coni] != golden->maxC[coni]){
			printk(KERN_ERR"maxC error\n");
		}
	}

	for(coni=0;coni<3;coni++) {
		if(dmout3->CInv[coni] != golden->CInv[coni]){
			printk(KERN_ERR"CInv error\n");
		}
	}
#endif
	if(dmout3->m33Rgb2YuvScale2P != golden->m33Rgb2YuvScale2P){
		printk(KERN_ERR"m33Rgb2YuvScale2P error\n");
	}

	for(coni=0;coni<3;coni++) {
		for(conj=0;conj<3;conj++) {
			if(dmout3->m33Rgb2Yuv[coni][conj] != golden->m33Rgb2Yuv[coni][conj]){
				printk(KERN_ERR"m33Rgb2Yuv error\n");
			}
		}
	}

	for(coni=0;coni<3;coni++) {
		if(dmout3->v3Rgb2YuvOff[coni] != golden->v3Rgb2YuvOff[coni]){
			printk(KERN_ERR"v3Rgb2YuvOff error\n");
		}
	}

#if 0
	if(dmout3->out_clr != golden->out_clr){
		printk(KERN_ERR"out_clr error\n");
	}
	if(dmout3->out_bdp != golden->out_bdp){
		printk(KERN_ERR"out_bdp error\n");
	}

	if(dmout3->lut_type != golden->lut_type){
		printk(KERN_ERR"lut_type error\n");
	}
#endif

}

/** @brief write composer MD & DM parameter & 1D LUT to output buffer
*  @param mdAddr the pointer to DM metadata
*  @param dm_out the pointer to output buffer address for DM
*  @param comp_out the pointer to output buffer address for composer
*  @return None
*/
void DV_MD_Output_Gen(unsigned char *mdAddr, unsigned char *dm_out, unsigned char *comp_out)
{

#ifndef RTK_EDBEC_1_5
	DOLBYVISION_PATTERN dolby;
	dolby.ColNumTtl = 1920;
	dolby.RowNumTtl = 1080;
	dolby.BL_EL_ratio = 0;
	OTT_h_dm->dmExec.runMode = p_dm_cfg->dmCtrl.RunMode = RUN_MODE_NORMAL;
	p_dm_cfg->frmPri.PxlDefOut.pxlClr = 0;
	p_dm_cfg->frmPri.PxlDefOut.pxlChrm = 1;
	p_dm_cfg->frmPri.PxlDefOut.pxlDtp = 0;
	p_dm_cfg->frmPri.PxlDefOut.pxlBdp = 12;
	p_dm_cfg->frmPri.PxlDefOut.pxlWeav = 2;
	p_dm_cfg->frmPri.PxlDefOut.pxlLoc = 0;
	p_dm_cfg->frmPri.PxlDefOut.pxlEotf = 0;
	p_dm_cfg->frmPri.PxlDefOut.pxlRng = 1;
	p_dm_cfg->frmPri.ColNumTtl = dolby.ColNumTtl;//3
	p_dm_cfg->frmPri.RowNumTtl = dolby.RowNumTtl;

	SanityCheckDmCfg(p_dm_cfg);
#endif

#if EN_GLOBAL_DIMMING
	// reset g_forceUpdate_3DLUT
	g_forceUpdate_3DLUT = 0;

	if (g_MD_parser_state==MD_PARSER_INIT) 	// force update while replay each stream
		g_forceUpdate_3DLUT = 1;
#endif

#ifndef RTK_EDBEC_1_5
	dm_metadata_2_dm_param((dm_metadata_base_t *)mdAddr, &mds_ext);

	mdsChg = CheckMds(&mds_ext, &OTT_h_dm->mdsExt);	// modified by smfan
	CommitMds(&mds_ext, OTT_h_dm, DOLBY_SRC_OTT);
#else

	rtk_wrapper_commit_reg(OTT_h_dm,
		FORMAT_DOVI,
		(dm_metadata_t *)mdAddr,
		(rpu_ext_config_fixpt_main_t *)comp_out, /*baker, 151 don't use*/
		dlbPqConfig_ott, 0,
		&OTT_h_dm->dst_comp_reg,
		&OTT_h_dm->dm_reg,
		&OTT_h_dm->dm_lut);

	rtk_dm_exec_params_to_DmExecFxp(OTT_h_dm->h_ks,
		&OTT_h_dm->dmExec,
		//&OTT_h_dm->dmExec_rtk,
		(DmExecFxp_t_rtk*)dm_out,
		(MdsExt_t *)mdAddr,
		&OTT_h_dm->dm_lut,
		dlbPqConfig_ott,
		RUN_MODE_NORMAL);

	//lut3D B05 table
	memcpy(OTT_h_dm->dmExec.bwDmLut.lutMap,OTT_h_dm->dm_lut.lut3D,3*GMLUT_MAX_DIM*GMLUT_MAX_DIM*GMLUT_MAX_DIM*sizeof(uint16_t));
	memcpy(((DmExecFxp_t_rtk*)dm_out)->bwDmLut.lutMap,OTT_h_dm->dm_lut.lut3D,3*GMLUT_MAX_DIM*GMLUT_MAX_DIM*GMLUT_MAX_DIM*sizeof(uint16_t));


#endif
#if EN_GLOBAL_DIMMING
#if 0
	// fill the PWM duty value
	pDmExec->pwm_duty = OTT_h_dm->h_dm->tcCtrl.tcTgtSig.maxPq;
	// 3D LUT update flag
	pDmExec->update3DLut = g_forceUpdate_3DLUT || OTT_Parser_NeedUpdate;
	// Lv5
	pDmExec->lvl5AoiAvail = mds_ext.lvl5AoiAvail;
#endif
	((DmExecFxp_t_rtk*)dm_out)->update3DLut = g_forceUpdate_3DLUT || OTT_Parser_NeedUpdate;

# if EN_AOI
	pDmExec->active_area_left_offset = mds_ext.active_area_left_offset;
	pDmExec->active_area_right_offset = mds_ext.active_area_right_offset;
	pDmExec->active_area_top_offset = mds_ext.active_area_top_offset;
	pDmExec->active_area_bottom_offset = mds_ext.active_area_bottom_offset;
#endif
#endif



	//flush_cache_all();	// take too such time??

	/*43236 is from DOLBYVISION_MD_OUTPUT, size can't large than this value*/
	if(sizeof(DmExecFxp_t_rtk) > 43236) {
		printk(KERN_ERR"[Dolby][%s][Warnings]md_output size too large",__func__);
	}


#if 0
	// little2Big-endian
	for (idx = 0; idx < sizeof(rpu_ext_config_fixpt_main_t)/sizeof(unsigned int); idx++) {
		*comp_out_ptr = Swap4Bytes(*comp_out_ptr);
		comp_out_ptr++;
	}
	for (idx = 0; idx < sizeof(DmExecFxp_MdOutput_t)/sizeof(unsigned int); idx++) {
		*dm_out_bak = Swap4Bytes(*dm_out_bak);
		dm_out_bak++;
	}
#endif
}

void Check_DV_Mode(void)
{
	//check the playback is dolby mode or not. If the mode is not right, set again
	/*
	* case for DOLBY HDMI mode switch to DOLBY OTT mode which playback work on background
	* WebOS does NOT call init function again, therefore driver needs to check when VSC connect
	*/
	extern volatile unsigned int g_forceUpdateFirstTime;
	if (g_MdFlow) {
       // g_forceUpdateFirstTime = 1;//Force update 3d lut
        Set_OTT_Force_update(TRUE);//Force update finish part

		// set UI PQ tuning flag
		//g_picModeUpdateFlag = 1;//Mark by will Need to check
	}
}

void Set_OTT_Force_update(unsigned char enable)
{
    OTT_Force_update = enable;
}

unsigned char Get_OTT_Force_update(void)
{
    return OTT_Force_update;
}

void DV_disable_DM(void){
	/**/
	dm_hdr_double_buffer_ctrl_RBUS dm_double_buf_ctrl_reg;
	rtd_clearbits(DM_dm_submodule_enable_reg, DM_dm_submodule_enable_b02_enable_mask);

	dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
	dm_double_buf_ctrl_reg.dm_db_apply = 1;
	rtd_outl(DM_HDR_Double_Buffer_CTRL, dm_double_buf_ctrl_reg.regValue);
}


#if 0
unsigned int DV_pickUp_MD(unsigned int ptsh, unsigned int ptsl)
{
	unsigned int ret, idx;
	MdsChg_t mdsChg;
	DOLBYVISION_PATTERN dolby;
	DmExecFxp_t *pDmExec = h_dm->pDmExec;
	DmExecFxp_MdOutput_t *mdoutPtr;
	unsigned int ownBitAddr = g_Md_output_buf_vaddr;
	unsigned int *ownBitPtr;
	DOLBYVISION_MD_OUTPUT *sMDOut;


	for (idx = 0; idx < (g_MdFlow->bsSize/g_MdFlow->bsUnitSize); idx++) {

		DBG_PRINT("%s, ownBitAddr=0x%x \n", __FUNCTION__, ownBitAddr);

		sMDOut = ownBitAddr;
		DBG_PRINT("%s, *ownBitPtr=0x%x \n", __FUNCTION__, *ownBitPtr);

		/* check own_bit 1 */
		if (Swap4Bytes(sMDOut->own_bit) == 1) {
			DBG_PRINT("%s, number %d unit offset \n", __FUNCTION__, idx);

			//if (sMDOut->PTSH < ptsh) /* clear own_bit */
			//	sMDOut->own_bit = 0;
			if (Swap4Bytes(sMDOut->PTSH) == ptsh)
				return ownBitAddr;

		}

		ownBitAddr = (ownBitAddr + sizeof(DOLBYVISION_MD_OUTPUT) >= g_MdFlow->bsLimit) ?
						g_MdFlow->bsBase : (ownBitAddr + sizeof(DOLBYVISION_MD_OUTPUT));
	}



	if (idx == (g_MdFlow->bsSize/g_MdFlow->bsUnitSize)) {	/* buffer full */
		DBG_PRINT(KERN_EMERG"%s, Error...No match.... \n", __FUNCTION__);
		return 0;
	}
}
#endif

E_DV_PICTURE_MODE DV_Get_PictureMode(void)
{
	return ui_dv_picmode;
}


void el_not_support_handler(void)
{//when detect FEL type notice omx
	DOLBYVISION_MD_OUTPUT *p_md_output = NULL;
	p_md_output = (DOLBYVISION_MD_OUTPUT *)g_Md_output_buf_vaddr;
	if(p_md_output)
	{
		if(p_md_output->FEL == 0)
		{
			printk(KERN_ERR " func=%s line=%d detect FEL , not support \n",__FUNCTION__,__LINE__);
		}
		p_md_output->FEL = 1;
		p_md_output->own_bit = 1;
	}
}


#endif		/* end of #ifndef WIN32  */
