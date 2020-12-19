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
#include <rbus/sub_vgip_reg.h>
//#include <rbus/rbusHDMIReg.h>
#include <rbus/mdomain_cap_reg.h>
#include <rbus/mdomain_disp_reg.h>
#include <rbus/dc_sys_reg.h>
//#include <rbus/dc2_sys_reg.h>
#include <rbus/rgb2yuv_reg.h>
#include <rbus/dmato3dtable_reg.h>
//#include <rbus/rbusIedge_smoothReg.h>
#include <tvscalercontrol/scalerdrv/scalerdrv.h>
#include <../tvscalercontrol/scaler_vscdev.h>
#ifdef CONFIG_SUPPORT_SCALER
#include <tvscalercontrol/vip/scalerColor.h>
#endif
#include <tvscalercontrol/scalerdrv/scalermemory.h>
#include <tvscalercontrol/scaler/scalerstruct.h>
#include <target_display_config.h>
#include <rtk_kdriver/RPCDriver.h>   /* register_kernel_rpc, send_rpc_command */
#ifdef CONFIG_KDRIVER_USE_NEW_COMMON
	#include <VideoRPC_System.h>
#else
	#include <rpc/VideoRPC_System.h>
#endif
#include <rtk_kdriver/rtk_pwm.h>


#include "rpu_ext_config.h"
#include <KdmTypeFxp.h>
#include <VdrDmApi.h>
#include <CdmTypePriFxp.h>
#include <control_path_api.h>
#include <control_path_priv.h>
#include <dm2_x/VdrDmAPIpFxp.h>

#include "dolbyvisionEDR.h"
#include "dolbyvisionEDR_export.h"
#include "dolby_flowCtrl.h"
#include "BacklightLUT.h"

#ifdef RTK_EDBEC_1_5
#include "control_path_priv.h"
#include "typedefs.h"
#else
#include "VdrDmAPIpFxp.h"
#include "DmTypeDef.h"
#include "VdrDmAPI.h"
#include "VdrDmDataIo.h"
#endif
#include <rbus/iedge_smooth_reg.h>
#include <rbus/mpegnr_reg.h>
#include <rbus/hsd_dither_reg.h>
#include <rbus/di_reg.h>
#include <rbus/idcti_reg.h>
#include <rbus/nr_reg.h>
//#include <rbus/peaking_reg.h> pinyen remove unused file in k5l reg
#include <rbus/scaledown_reg.h>
#include <rbus/scaleup_reg.h>
#include <rbus/sub_dither_reg.h>
#include <rbus/dfilter_reg.h>
#include <rbus/color_mb_peaking_reg.h>
#include <rbus/gamma_reg.h>
#include <rbus/color_reg.h>
//pinyen #include <rbus/srgb_reg.h> pinyen remove unused file in k5l reg
#include <rbus/main_dither_reg.h>
extern unsigned int g_i2r_fps_thr;

void HDMI_TEST_VSIF(unsigned int wid, unsigned int len, unsigned char* mdAddr);

extern int rtk_wrapper_commit_reg_vsif(h_cp_context_t p_ctx,
	signal_format_t  input_format,
	dm_metadata_t *p_src_dm_metadata,
	rpu_ext_config_fixpt_main_t *p_src_comp_metadata,
	pq_config_t* pq_config,
	int view_mode_id,
	composer_register_t *p_dst_comp_reg,
	dm_register_t *p_dm_reg,
	dm_lut_t *p_dm_lut);

extern unsigned char get_cur_hdmi_dolby_apply_state(void);

#include <mach/rtk_log.h>
#include <linux/signal.h>
//#define DISABLE_FILE_RW


#include <tvscalercontrol/scalerdrv/scaler_i2rnd.h>
//#define MIS_SCPU_CLK90K_LO_reg							0xB801B690


#define VE_COMP_ALIGN_WIDTH_PIXEL	64
#define VE_COMP_ALIGN_WIDTH_MASK	(0x3f)
#define VE_COMP_ALIGN_HEIGH_PIXEL	8
#define VE_COMP_ALIGN_HEIGH_MASK	(0x7)


char ifilename[500];
char ofilename[500];



struct file *gFrame1Infp = NULL;	/* HDMI data or BL data */
struct file *gFrame2Infp = NULL;	/* EL data */
struct file *gFrame1Outfp = NULL;	/* output for HDMI or Y-block */
struct file *gFrame2Outfp = NULL;	/* C-block */
struct file *gMDInfp = NULL;		/* rpu data or metadata */
struct file *gFrameCRFOutfp = NULL;
long globe_size=0;

unsigned int gVoFrameAddr1 = 0;
unsigned int gVoFrameAddr2 = 0;
unsigned int gVoFrameAddr3 = 0;
unsigned int gVoFrameAddr4 = 0;
unsigned int gVoFrame1AddrAssign = 0;
unsigned int gVoFrame2AddrAssign = 0;
unsigned int gRpuMDAddr = 0;
unsigned int gRpuMDCacheAddr = 0;
unsigned int g_frameCtrlNo = 0;
unsigned int g_IDKDumpAddr = 0, g_IDKDumpAddr2 = 0;

unsigned int dolby_proverlay_background_h_start_end = 0;//For letter box use.Record h start and end
unsigned int dolby_proverlay_background_v_start_end = 0;//For letter box use. Record V start and end
unsigned char letter_box_black_flag = FALSE;//For letter box use. Need force BG black
unsigned char request_letter_dtg_change = FALSE;//For letter box use. Requesttsk to run
static DEFINE_SPINLOCK(Dolby_Letter_Box_Spinlock);/*Spin lock no context switch. This is for LETTER box parameter*/
static DEFINE_SPINLOCK(Dolby_HDMI_UI_Change);/*Spin lock no context switch. This is for picture and backlight change*/
volatile unsigned char hdmi_ui_change_flag = FALSE;//hdmi backlight and picture mode change flag
volatile unsigned char hdmi_ui_change_delay_count = 0xff; //when hdmi_ui_change_delay_count = HDMI_DOLBY_UI_CHANGE_DELAY_APPLY. We apply new ui setting
//RTK add fix compile error
extern UINT8 uPicMode;
/*
*  record the color mode
*  1. dark
*  2. Bright
*  3. vivid
*/
E_DV_PICTURE_MODE ui_dv_picmode = DV_PIC_DARK_MODE;//Record UI Picture mode for Parser
E_DV_PICTURE_MODE current_dv_picmode = DV_PIC_NONE_MODE;//Current apply pic mode for Parser

E_DV_PICTURE_MODE OTT_last_md_picmode = DV_PIC_NONE_MODE;//last apply pic mode for OTT md apply
E_DV_PICTURE_MODE HDMI_last_md_picmode = DV_PIC_NONE_MODE;//last apply pic mode for HDMI md apply

unsigned char ui_dv_backlight_value = 50;//Record UI backlight value  for Parser
unsigned char current_dv_backlight_value = 0xff;//Current apply backlight value  for Parser

unsigned char OTT_last_md_backlight = 0xff;//last apply backlight value for OTT md apply
unsigned char HDMI_last_md_backlight = 0xff;//last apply backlight value for HDMI md apply


/* flag to force update 3D LUT */
volatile unsigned int g_forceUpdate_3DLUT = 0;
volatile unsigned int g_forceUpdateFirstTime = 0;

/* UI PQ tuning flag */
volatile unsigned int g_picModeUpdateFlag = 0;//Used on HDMI
volatile unsigned int HDMI_Parser_NeedUpdate = 0;//Used on HDMI
volatile unsigned int OTT_Parser_NeedUpdate = TRUE;//Parse md data. When IDR frame and PIC mode change or backlight change

/* copy from rtk_vo.h */
#define VP_DC_IDX_VOUT_V1_Y_CURR	116
#define VP_DC_IDX_VOUT_V1_C_CURR	117
#define VP_DC_IDX_VOUT_V1_Y_CURR2	118
#define VP_DC_IDX_VOUT_V1_C_CURR2	119
#define VP_DC_IDX_VOUT_V2_Y_CURR	120
#define VP_DC_IDX_VOUT_V2_C_CURR	121
#define VP_DC_IDX_VOUT_V2_Y_CURR2	122
#define VP_DC_IDX_VOUT_V2_C_CURR2	123
#define VP_DC_IDX_VOUT_TMP_Y_CURR	124
#define VP_DC_IDX_VOUT_TMP_C_CURR	125
extern void vodma_DCU_Set(uint32_t index, uint32_t st_addr, uint32_t width, uint32_t offset_x, uint32_t offset_y);

// backlight delay control
unsigned int g_bl_HDMI_apply_delay = 0;//(2*90);//Change to 2 ms. Lockup issue. Need to check with dolby
unsigned int g_bl_OTT_apply_delay = 0;//(100*90);	// 100ms
static int last_global_dim_val = -1; //k2l_set_global_dimming use record last PWM setting

//#include "CFG.h"
#include "cfg_files/CFG_low_latency_12.h"
#include "cfg_files/CFG_low_latency_0.h"
//pq_config_t *dlbPqConfig_ott;
//pq_config_t *dlbPqConfig_hdmi;

pq_config_t *dlbPqConfig_ott = (pq_config_t*)CFG_12b_0_low_latency_0;
pq_config_t *dlbPqConfig_hdmi = (pq_config_t*)CFG_12b_0_low_latency_12;


int dlbPqConfig_flag=0;

#define    dimC1 17
#define    dimC2 17
#define    dimC3 17
yuvdata b05_table[dimC1*dimC2*dimC3];
yuvdata B05_DDR_TABLE[dimC1*dimC2*dimC3];
extern unsigned int  *b05_aligned_vir_addr;

#define _ENABLE_BACKLIGHT_DELAY_CTRL
#ifdef _ENABLE_BACKLIGHT_DELAY_CTRL
  #define BACKLIGHT_BUF_SIZE 		128
  typedef struct _BACKLIGHT_CTRL_INFO{
  	unsigned char readIdx;
  	unsigned char writeIdx;
  	unsigned int stc[BACKLIGHT_BUF_SIZE];
  	unsigned int duty_idx[BACKLIGHT_BUF_SIZE];
  }BACKLIGHT_CTRL_INFO;
  BACKLIGHT_CTRL_INFO backLightInfo;
void BacklightDelayCtrl_update(uint16_t duty_idx, unsigned int delayPeriod, unsigned char src, E_DV_PICTURE_MODE uPicMode, unsigned int backlightSliderScalar);
#else
void BacklightCtrl(uint16_t duty_idx, E_DV_PICTURE_MODE uPicMode);
#endif
#define DEN_PROT_LINE_CNT 30	// front porch enter time check

void Reset_Pic_and_Backlight(void)
{//Let force update
	current_dv_picmode = DV_PIC_NONE_MODE;//Current apply pic mode for Parser
	current_dv_backlight_value = 0xff;//Current apply backlight value  for Parser
	OTT_last_md_backlight = 0xff;//last apply backlight value for OTT md apply
	OTT_last_md_picmode = DV_PIC_NONE_MODE;//last apply pic mode for OTT md apply
}


spinlock_t* dolby_letter_box_spinlock(void)
{//Get Dolby_Letter_Box_Spinlock
	return &Dolby_Letter_Box_Spinlock;
}

spinlock_t* dolby_hdmi_ui_change_spinlock(void)
{//Get Dolby_Letter_Box_Spinlock
	return &Dolby_HDMI_UI_Change;
}


static struct file * file_open(const char *path, int flags, int rights)
{
    struct file* filp = NULL;
    mm_segment_t oldfs;
    int err = 0;

    oldfs = get_fs();
    set_fs(get_ds());
    filp = filp_open(path, flags, rights);
    set_fs(oldfs);
    if(IS_ERR(filp)) {
        err = PTR_ERR(filp);
        return NULL;
    }
    return filp;
}

static void file_close(struct file *file)
{
    filp_close(file, NULL);
}


static int file_read(unsigned char *data, unsigned int size, unsigned long long offset, struct file *file)
{
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_read(file, data, size, &offset);

    set_fs(oldfs);
    return ret;
}

static int file_write(unsigned char *data, unsigned int size, unsigned long long offset, struct file *file)
{
#ifndef DISABLE_FILE_RW
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_write(file, data, size, &offset);

    set_fs(oldfs);
    return ret;
#else
	return 0;
#endif
}

static int file_sync(struct file *file)
{
    vfs_fsync(file, 0);
    return 0;
}

extern int tty_num;
extern int g_log_level;
#define DV_LOG_LINE_MAX 256
struct file *tty_fd = NULL;
int DBG_PRINT_RESET(void )
{
	if(tty_fd != NULL)
		file_close(tty_fd);
	tty_fd = NULL;
	return 0;
};
#if 0
static unsigned char* g_dump_buf = NULL;

static void rtk_hdmi_dump(int fn,
	composer_register_t *p_dst_comp_reg,
	dm_register_t *p_dm_reg,
	dm_lut_t *p_dm_lut,
	h_cp_context_t h_ctx)
{
	char tmpStr[128];
	struct file *fout = NULL;
	cp_context_t* p_ctx = h_ctx;
	int size = 0;

	if(g_dump_buf == NULL) {
		g_dump_buf = (unsigned char*)kmalloc(0x10000, GFP_KERNEL);

		if(g_dump_buf == NULL) {
			printk(KERN_ERR"[Baker][%s,%d] kmalloc fail, addr=%p\n", __func__, __LINE__, g_dump_buf);
			return;
		}
	}
	printk(KERN_ERR"[Baker][%s,%d], addr=%p\n", __func__, __LINE__, g_dump_buf);

	size = DumpKsBinBuf(p_ctx->h_ks, g_dump_buf);

	if(p_dst_comp_reg != NULL) {
		snprintf(tmpStr, 128, "/tmp/hdmi_comp_reg_%d_%04d", sizeof(composer_register_t),fn);
		fout = file_open(tmpStr, O_TRUNC | O_WRONLY | O_CREAT, 0x755);
		if(fout == NULL) {
			printk(KERN_ERR"%s can't open %s \n", __func__, tmpStr);
			return;
		}

		file_write((unsigned char *)p_dst_comp_reg, sizeof(composer_register_t), 0, fout);
		file_sync(fout);
		file_close(fout);
	}

	if(p_dm_reg != NULL) {
		snprintf(tmpStr, 128, "/tmp/hdmi_dm_reg_%d_%04d",sizeof(dm_register_t),fn);
		fout = file_open(tmpStr, O_TRUNC | O_WRONLY | O_CREAT, 0x755);
		if(fout == NULL) {
			printk(KERN_ERR"%s can't open %s \n", __func__, tmpStr);
			return;
		}
		file_write((unsigned char *)p_dm_reg, sizeof(dm_register_t), 0, fout);
		file_sync(fout);
		file_close(fout);
}

	if(p_dm_lut != NULL) {
		snprintf(tmpStr, 128, "/tmp/hdmi_dm_lut_%d_%04d",sizeof(dm_lut_t),fn);
		fout = file_open(tmpStr, O_TRUNC | O_WRONLY | O_CREAT, 0x755);
		if(fout == NULL) {
			printk(KERN_ERR"%s can't open %s \n", __func__, tmpStr);
			return;
		}
		file_write((unsigned char *)p_dm_lut->lut3D, sizeof(p_dm_lut->lut3D), 0, fout);
		file_write((unsigned char *)p_dm_lut->g2L, sizeof(p_dm_lut->g2L), sizeof(p_dm_lut->lut3D), fout);
		file_write((unsigned char *)p_dm_lut->tcLut, sizeof(p_dm_lut->tcLut), sizeof(p_dm_lut->lut3D) + sizeof(p_dm_lut->g2L), fout);
		file_sync(fout);
		file_close(fout);
	}
	if(size) {
		snprintf(tmpStr, 128, "/tmp/hdmi_bin_%d_%04d",size ,fn);
		fout = file_open(tmpStr, O_TRUNC | O_WRONLY | O_CREAT, 0x755);
		if(fout == NULL) {
			printk(KERN_ERR"%s can't open %s \n", __func__, tmpStr);
			return;
		}
		file_write((unsigned char *)g_dump_buf, size, 0, fout);
		file_sync(fout);
		file_close(fout);
	}

}
#endif
//int DBG_PRINT_L(int level, const char *fmt, ...)
static char textbuf[DV_LOG_LINE_MAX];
//int DV_printk(const char *fmt, ...)
int DV_printk(int lvl,const char *fmt, ...)
//int DBG_PRINT_L(unsigned char *fmt, ...)
{
	va_list args;
	int r,i;
	char *ptr=0;

	if(tty_num == 0 || g_log_level<lvl)
		return -1;

	va_start(args, fmt);
	r = vsnprintf(textbuf, (DV_LOG_LINE_MAX - 2),fmt, args);
	va_end(args);
	if(DV_DEBUG_KERNEL == lvl || (in_interrupt())){
		printk("%s \n",textbuf);
		return r;
	}
	if(tty_fd == 0){
		char tmp_path[64];
		memset(tmp_path,0x0,64);
		snprintf(tmp_path, 64, "/dev/pts/%d", tty_num);
		tty_fd = file_open(tmp_path, O_TRUNC | O_WRONLY | O_CREAT, 0);
		if (tty_fd == NULL) {
			printk("%s, tty_fd open failed \n", __FUNCTION__);
			tty_fd = NULL;
			return -1;
		}
	}
	ptr = &textbuf[0];
	for (i=0; i < r; i++) {
		file_write(ptr, 1, i, tty_fd);
		ptr++;
	}
	//file_sync(tty_fd);
	return r;
}

void Fmt_Cvt_12YUV422_to_8SEQ444(struct file *rptr, struct file *wptr, unsigned int img_width, unsigned int img_height)
{
	unsigned long filelength = 0, loop = 0, roffset = 0;//, woffset = 0;
	int framecount = 0, framesize = 0, frameIdx = 0;
	short pcbcr, py;
	int bpp = 4;	// bytes per pixel
	unsigned char wU, wY, wV;
	unsigned char *ddrptr = (unsigned char *)gVoFrame1AddrAssign;
	DOLBYVISIONEDR_DEV *dolbyvisionEDR = &dolbyvisionEDR_devices[0];
	dma_addr_t addr;

	printk("%s...\n", __FUNCTION__);

	addr = dma_map_single(dolbyvisionEDR->dev, (void *)ddrptr, (3840*2160*4), DMA_TO_DEVICE);

	//fseek(rptr, 0L, SEEK_END);
	//filelength = ftell(rptr);
	//fseek(rptr, 0L, SEEK_SET);

	framesize = img_width * img_height * bpp;
	framecount = filelength / framesize;

	framecount = 1;	// force 5 frames

	roffset = g_frameCtrlNo * framesize;

	for (frameIdx = 0; frameIdx < framecount; frameIdx++)
	{
		for (loop = 0; loop < (framesize / bpp); loop++)
		{
			/*
			vodma_y[9:0]    ->  hdmi_y_in[11:4]=vodma_y[9:2]
			vodma_cb[9:0]   ->  hdmi_y_in[3:0]=vodma_cb[5:2], hdmi_cbcr_in[3:0]=vodma_cb[9:6]
			vodma_cr[9:0]   ->  hdmi_cbcr_in[11:4]=vodma_cr[9:2]
			*/

			// 24 bit
			// byte one:   Cb=U  U[7:0] = RawCbCr[3:0]<<4 | RawY[3:0]
			// byte two:   Y=Y   Y[7:0] = RawY[11:4]
			// byte three: Cr=V  V[7:0] = RawCbCr[11:4]

			// test sample UYVY_random_64x64_12b_scrambled.yuv
			// -f 1 -i UYVY_random_64x64_12b_scrambled.yuv -o test.yuv -s 64x64
			// -f 1 -i 2002\TestSet_1920x1080_UYVY_12b.0000046.yuv -o 2002\TestSet_1920x1080_UYVY_12b.0000046.yuv -s 1920x1080

			//if (feof(rptr) != 0)
			//{
			//	printf("Read pointer is end \n");
			//	break;
			//}

			file_read((unsigned char *)&pcbcr, 2, roffset, rptr);
			roffset += 2;
			file_read((unsigned char *)&py, 2, roffset, rptr);
			roffset += 2;
			//printk("pcbcr=0x%x, py=0x%x \n", pcbcr, py);

			wY = ((py & 0xFF0) >> 4);
			wU = (py & 0xF) | ((pcbcr & 0xF) << 4);
			wV = ((pcbcr & 0xFF0) >> 4);

			//printk("wY=0x%x, wU=0x%x, wV=0x%x \n", wY, wU, wV);
#if 0
			file_write(&wU, 1, woffset, wptr);
			woffset += 1;
			file_write(&wY, 1, woffset, wptr);
			woffset += 1;
			file_write(&wV, 1, woffset, wptr);
			woffset += 1;
#endif
			// original			// U first, Y second, V third
			// write to DDR
			*ddrptr = wU;
			ddrptr++;
			*ddrptr = wY;
			ddrptr++;
			*ddrptr = wV;
			ddrptr++;

		}
	}

	dma_sync_single_for_device(dolbyvisionEDR->dev, addr, (3840*2160*4), DMA_TO_DEVICE);
	dma_unmap_single(dolbyvisionEDR->dev, addr, (3840*2160*4), DMA_TO_DEVICE);
}


void Fmt_Cvt_14YUV420_to_10SEQ444(struct file *rptr, struct file *wptr, unsigned int img_width, unsigned int img_height)
{
	unsigned long filelength = 0, framecount = 0, framesize = 0/*, frameIdx = 0*/, woffset = 0;
	short vodma_y, vodma_cb, vodma_cr;
	short pcbcr, py;
	int bpp = 2;
	unsigned int framebytes_plane_y, framebytes_plane_u, framebytes_plane_v;
	int loop, looph, loopw, rcbcrFlag = 0, bitIdx = 0;
	unsigned int planeyoffset, planeuoffset, planevoffset;
	//struct stat st;
	unsigned int linebytes;
	unsigned char *linebuffer;
	unsigned char *bitarray;
	int idx, bit;

	unsigned char *ddrptr = (unsigned char *)gVoFrame1AddrAssign;
	DOLBYVISIONEDR_DEV *dolbyvisionEDR = &dolbyvisionEDR_devices[0];
	dma_addr_t addr;

	printk("%s...\n", __FUNCTION__);

	addr = dma_map_single(dolbyvisionEDR->dev, (void *)ddrptr, (3840*2160*4), DMA_TO_DEVICE);

	linebytes = ((img_width * 30) / 8);
	if (((img_width * 30) / 8) % 8)
		linebytes += 1;
	if ((linebytes % 8))
		linebytes += (8 - (linebytes % 8));

	printk("%s, linebytes=%d \n", __FUNCTION__, linebytes);

	linebuffer = (unsigned char *)kmalloc(linebytes, GFP_KERNEL);
	bitarray = (unsigned char *)kmalloc(30 * img_width, GFP_KERNEL);
	memset(linebuffer, 0x00, linebytes);

	//fseek(rptr, 0L, SEEK_END);
	//filelength = ftell(rptr);
	//if (stat(ifilename, &st) == 0)
	//	filelength = st.st_size;
	//fseek(rptr, 0L, SEEK_SET);

	framebytes_plane_y = img_height * img_width * bpp;
	framebytes_plane_u = framebytes_plane_v = img_height / 2 * img_width / 2 * bpp;

	framesize = (framebytes_plane_y + framebytes_plane_u + framebytes_plane_v);
	framecount = (filelength / framesize);

	// line base & 8 bytes boundary alignment
	//for (frameIdx = 0; frameIdx < framecount; frameIdx++)
	{
		// test sample VDR_out_352x288_P420_14b.0086756.yuv
		// -f 2 -i VDR_out_352x288_P420_14b.0086756.yuv -o test.yuv -s 352x288
		// -f 2 -i 2000\TestSet_1920x1080_P420_14b.0000046.yuv -o 2000\TestSet_1920x1080_P420_14b.0000046.yuv -s 1920x1080

		//planeyoffset = framesize * frameIdx;
		planeyoffset = framesize * g_frameCtrlNo;
		planeuoffset = planeyoffset + framebytes_plane_y;
		planevoffset = planeuoffset + framebytes_plane_u;

		for (looph = 0; looph < img_height; looph++)
		{
			memset(linebuffer, 0x00, linebytes);
			memset(bitarray, 0x00, 30 * img_width);
			bitIdx = 0;

			if (looph % 2)
				rcbcrFlag = 0;	// odd line has no CbCr
			else
				rcbcrFlag = 1;	// even line has CbCr

			for (loopw = 0; loopw < img_width; loopw++)
			{
				// read Y
				//fseek(rptr, planeyoffset, SEEK_SET);
				//fread(&py, 2, 1, rptr);
				file_read((unsigned char *)&py, 2, planeyoffset, rptr);

				planeyoffset += 2;	// offset update
				//printf("%x\n", py);

				pcbcr = 0x01;	// DV team fixed U or V to 0x1 when odd line
				if (rcbcrFlag)
				{
					if (loopw % 2)	// odd point is cr
					{
						// read Cr
						//fseek(rptr, planevoffset, SEEK_SET);
						//fread(&pcbcr, 2, 1, rptr);
						file_read((unsigned char *)&pcbcr, 2, planevoffset, rptr);
						planevoffset += 2;
					}
					else            // even point is cb
					{
						// read Cb
						//fseek(rptr, planeuoffset, SEEK_SET);
						//fread(&pcbcr, 2, 1, rptr);
						file_read((unsigned char *)&pcbcr, 2, planeuoffset, rptr);
						planeuoffset += 2;
					}

					//printf("%x\n", pcbcr);
				}

				/*
				vodma_y[9:0]    ->  dm_y_in[13:4]=vodma_y[9:0]
				vodma_cb[9:0]   ->  dm_y_in[3:0]=vodma_cb[5:2], dm_cbcr_in[3:0]=vodma_cb[9:6]
				vodma_cr[9:0]   ->  dm_cbcr_in[13:4]=vodma_cr[9:0]
				*/

				// 30 bit
				// bit[9:0]:     Cb=U  U[9:0] = RawCbCr[3:0]<<6 | RawY[3:0]<<2
				// bit[19:10]:   Y=Y   Y[9:0] = RawY[13:4]
				// bit[29:20]:   Cr=V  V[9:0] = RawCbCr[13:4]

				// pack to vodma YUV which LSB 10 bit is valid bit
				vodma_y = (py & 0x3FF0) >> 4;
				vodma_cr = (pcbcr & 0x3FF0) >> 4;
				vodma_cb = ((pcbcr & 0xF) << 6) | ((py & 0xF) << 2);

				// store each pixel's 30 bit
				for (loop = 0; loop < 30; loop++)
				{
					if (loop < 10)
						bitarray[bitIdx++] = (vodma_cb & (1 << loop)) >> loop;
					else if ((loop >= 10) && (loop < 20))
						bitarray[bitIdx++] = (vodma_y & (1 << (loop - 10))) >> (loop - 10);
					else if (loop >= 20)
						bitarray[bitIdx++] = (vodma_cr & (1 << (loop - 20))) >> (loop - 20);

					//if ((loop%4) == 0)
					//	printf("  %d", bitarray[bitIdx - 1]);
					//else
					//	printf("%d", bitarray[bitIdx - 1]);
				}
				//printf("\n");
			}

			// bit array to linebuffer
			for (loop = 0; loop < (img_width * 30); loop++)
			{
				idx = loop / 8;
				bit = loop % 8;
				linebuffer[idx] |= (bitarray[loop] << bit);
			}

			// write one line
			//fwrite(linebuffer, linebytes, 1, wptr);
			file_write(linebuffer, linebytes, woffset, wptr);


			// write to DDR
			ddrptr += woffset;
			memcpy(ddrptr, linebuffer, linebytes);
			ddrptr = (unsigned char *)gVoFrame1AddrAssign;

			woffset += linebytes;
		}
	}

	kfree(linebuffer);
	kfree(bitarray);

	dma_sync_single_for_device(dolbyvisionEDR->dev, addr, (3840*2160*4), DMA_TO_DEVICE);
	dma_unmap_single(dolbyvisionEDR->dev, addr, (3840*2160*4), DMA_TO_DEVICE);
}


void Fmt_Cvt_12YUV420_to_10SEQ444(struct file *rptr, struct file *wptr, unsigned int img_width, unsigned int img_height)
{
	unsigned long filelength = 0, framecount = 0, framesize = 0, woffset = 0;//, frameIdx = 0;
	short vodma_y, vodma_cb, vodma_cr;
	short pcbcr, py;
	int bpp = 2;
	unsigned int framebytes_plane_y, framebytes_plane_u, framebytes_plane_v;
	int loop, looph, loopw, rcbcrFlag = 0, bitIdx = 0;
	unsigned int planeyoffset, planeuoffset, planevoffset;
	//struct stat st;
	unsigned int linebytes;
	unsigned char *linebuffer;
	unsigned char *bitarray;

	unsigned char *ddrptr = (unsigned char *)gVoFrame1AddrAssign;
	DOLBYVISIONEDR_DEV *dolbyvisionEDR = &dolbyvisionEDR_devices[0];
	dma_addr_t addr;
	int idx, bit;

	printk("%s...\n", __FUNCTION__);

	addr = dma_map_single(dolbyvisionEDR->dev, (void *)ddrptr, (3840*2160*4), DMA_TO_DEVICE);

	linebytes = ((img_width * 30) / 8);
	if (((img_width * 30) / 8) % 8)
		linebytes += 1;
	if ((linebytes % 8))
		linebytes += (8 - (linebytes % 8));

	linebuffer = (unsigned char *)kmalloc(linebytes, GFP_KERNEL);
	bitarray = (unsigned char *)kmalloc(30 * img_width, GFP_KERNEL);
	memset(linebuffer, 0x00, linebytes);

	//fseek(rptr, 0L, SEEK_END);
	//filelength = ftell(rptr);
	//if (stat(ifilename, &st) == 0)
	//	filelength = st.st_size;
	//fseek(rptr, 0L, SEEK_SET);

	framebytes_plane_y = img_height * img_width * bpp;
	framebytes_plane_u = framebytes_plane_v = img_height / 2 * img_width / 2 * bpp;

	framesize = (framebytes_plane_y + framebytes_plane_u + framebytes_plane_v);
	framecount = (filelength / framesize);

	// line base & 8 bytes boundary alignment
	//for (frameIdx = 0; frameIdx < framecount; frameIdx++)
	{
		// test sample VDR_out_352x288_P420_14b.0086756.yuv
		// -f 2 -i VDR_out_352x288_P420_14b.0086756.yuv -o test.yuv -s 352x288
		// -f 2 -i 2000\TestSet_1920x1080_P420_14b.0000046.yuv -o 2000\TestSet_1920x1080_P420_14b.0000046.yuv -s 1920x1080

		//planeyoffset = framesize * frameIdx;
		planeyoffset = framesize * g_frameCtrlNo;
		planeuoffset = planeyoffset + framebytes_plane_y;
		planevoffset = planeuoffset + framebytes_plane_u;

		for (looph = 0; looph < img_height; looph++)
		{
			memset(linebuffer, 0x00, linebytes);
			memset(bitarray, 0x00, 30 * img_width);
			bitIdx = 0;

			if (looph % 2)
				rcbcrFlag = 0;	// odd line has no CbCr
			else
				rcbcrFlag = 1;	// even line has CbCr

			for (loopw = 0; loopw < img_width; loopw++)
			{
				// read Y
				//fseek(rptr, planeyoffset, SEEK_SET);
				//fread(&py, 2, 1, rptr);
				file_read((unsigned char *)&py, 2, planeyoffset, rptr);

				planeyoffset += 2;	// offset update
				//printf("%x\n", py);

				pcbcr = 0x01;	// DV team fixed U or V to 0x1 when odd line
				if (rcbcrFlag)
				{
					if (loopw % 2)	// odd point is cr
					{
						// read Cr
						//fseek(rptr, planevoffset, SEEK_SET);
						//fread(&pcbcr, 2, 1, rptr);
						file_read((unsigned char *)&pcbcr, 2, planevoffset, rptr);
						planevoffset += 2;
					}
					else			// even point is cb
					{
						// read Cb
						//fseek(rptr, planeuoffset, SEEK_SET);
						//fread(&pcbcr, 2, 1, rptr);
						file_read((unsigned char *)&pcbcr, 2, planeuoffset, rptr);
						planeuoffset += 2;
					}

					//printf("%x\n", pcbcr);
				}

				/*
				vodma_y[9:0]	->	dm_y_in[13:4]=vodma_y[9:0]
				vodma_cb[9:0]	->	dm_y_in[3:0]=vodma_cb[5:2], dm_cbcr_in[3:0]=vodma_cb[9:6]
				vodma_cr[9:0]	->	dm_cbcr_in[13:4]=vodma_cr[9:0]
				*/

				// 30 bit
				// bit[9:0]:	 Cb=U  U[9:0] = RawCbCr[3:0]<<6 | RawY[3:0]<<2
				// bit[19:10]:	 Y=Y   Y[9:0] = RawY[13:4]
				// bit[29:20]:	 Cr=V  V[9:0] = RawCbCr[13:4]

				// pull 12 bit to 14 bit
				py = py << 2;
				pcbcr = pcbcr << 2;

				// pack to vodma YUV which LSB 10 bit is valid bit
				vodma_y = (py & 0x3FF0) >> 4;
				vodma_cr = (pcbcr & 0x3FF0) >> 4;
				vodma_cb = ((pcbcr & 0xF) << 6) | ((py & 0xF) << 2);

				// store each pixel's 30 bit
				for (loop = 0; loop < 30; loop++)
				{
					if (loop < 10)
						bitarray[bitIdx++] = (vodma_cb & (1 << loop)) >> loop;
					else if ((loop >= 10) && (loop < 20))
						bitarray[bitIdx++] = (vodma_y & (1 << (loop - 10))) >> (loop - 10);
					else if (loop >= 20)
						bitarray[bitIdx++] = (vodma_cr & (1 << (loop - 20))) >> (loop - 20);

					//if ((loop%4) == 0)
					//	printf("  %d", bitarray[bitIdx - 1]);
					//else
					//	printf("%d", bitarray[bitIdx - 1]);
				}
				//printf("\n");
			}

			// bit array to linebuffer
			for (loop = 0; loop < (img_width * 30); loop++)
			{
				idx = loop / 8;
				bit = loop % 8;
				linebuffer[idx] |= (bitarray[loop] << bit);
			}

			// write one line
			//fwrite(linebuffer, linebytes, 1, wptr);
			file_write(linebuffer, linebytes, woffset, wptr);


			// write to DDR
			ddrptr += woffset;
			memcpy(ddrptr, linebuffer, linebytes);
			ddrptr = (unsigned char *)gVoFrame1AddrAssign;

			woffset += linebytes;
		}
	}

	kfree(linebuffer);
	kfree(bitarray);

	dma_sync_single_for_device(dolbyvisionEDR->dev, addr, (3840*2160*4), DMA_TO_DEVICE);
	dma_unmap_single(dolbyvisionEDR->dev, addr, (3840*2160*4), DMA_TO_DEVICE);
}




void TwoDDR_and_DDR3_data_Mapping(char *pycbcr, unsigned int do_swap_flag)
{
	unsigned int *pi_pycbcr, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8, blkIdx;
	/*
	             _________________________________
	view order   | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |

	bank store
	            _________________________________
	row 0		| 1 | 3 | 2 | 4 | 5 | 7 | 6 | 8 |
	            _________________________________
	row 1		| 3 | 1 | 4 | 2 | 7 | 5 | 8 | 6 |
	            _________________________________
	row 2		| 3 | 1 | 4 | 2 | 7 | 5 | 8 | 6 |
	            _________________________________
	row 3		| 1 | 3 | 2 | 4 | 5 | 7 | 6 | 8 |
	*/

	if (do_swap_flag == 1)
	{
		for (blkIdx = 0; blkIdx < 2; blkIdx++)
		{
			pi_pycbcr = (unsigned int *)pycbcr;
			tmp1 = pi_pycbcr[(blkIdx * 8)];
			tmp2 = pi_pycbcr[(blkIdx * 8) + 1];
			tmp3 = pi_pycbcr[(blkIdx * 8) + 2];
			tmp4 = pi_pycbcr[(blkIdx * 8) + 3];
			tmp5 = pi_pycbcr[(blkIdx * 8) + 4];
			tmp6 = pi_pycbcr[(blkIdx * 8) + 5];
			tmp7 = pi_pycbcr[(blkIdx * 8) + 6];
			tmp8 = pi_pycbcr[(blkIdx * 8) + 7];

			pi_pycbcr[(blkIdx * 8) + 0] = tmp5;
			pi_pycbcr[(blkIdx * 8) + 1] = tmp6;
			pi_pycbcr[(blkIdx * 8) + 2] = tmp1;
			pi_pycbcr[(blkIdx * 8) + 3] = tmp2;
			pi_pycbcr[(blkIdx * 8) + 4] = tmp7;
			pi_pycbcr[(blkIdx * 8) + 5] = tmp8;
			pi_pycbcr[(blkIdx * 8) + 6] = tmp3;
			pi_pycbcr[(blkIdx * 8) + 7] = tmp4;
		}
	}
	if (do_swap_flag == 0)
	{
		pi_pycbcr = (unsigned int *)pycbcr;
		tmp1 = pi_pycbcr[2];
		tmp2 = pi_pycbcr[3];
		tmp3 = pi_pycbcr[10];
		tmp4 = pi_pycbcr[11];

		pi_pycbcr[2] = pi_pycbcr[4];
		pi_pycbcr[3] = pi_pycbcr[5];
		pi_pycbcr[10] = pi_pycbcr[12];
		pi_pycbcr[11] = pi_pycbcr[13];

		pi_pycbcr[4] = tmp1;
		pi_pycbcr[5] = tmp2;
		pi_pycbcr[12] = tmp3;
		pi_pycbcr[13] = tmp4;
	}
}



void Fmt_Cvt_8YUV420_to_8BLK444(struct file *rptr, struct file *wptr_y, struct file *wptr_c, unsigned int img_width, unsigned int img_height)
{
	//img_width = 1920;
	//img_height = 1080;
	//char filepath[500];
	unsigned int width_scale, page_size = 3, blk_size, blk_h, blk_w, blk_total, blk_wNum, blk_hNum, blk_RowCnt;
	unsigned long filelength = 0, framecount = 0, framesize = 0, frameIdx = 0;
	char pcb, pcr, py[64], pcbcr[64];
	int bpp = 1;	// byte per pixel
	unsigned int framebytes_plane_y, framebytes_plane_u, framebytes_plane_v;
	int /*loop,*/ looph, loopw, h, w;
	unsigned int planeyoffset, planeuoffset, planevoffset, planeyblkoffset;
	unsigned int wplaneyoffset = 0, wplaneuvoffset = 0;
	unsigned int ddrNum = 2;	// two DDR and for DDR3
	//unsigned int *pi_py, *pi_pcbcr, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8, blkIdx;
	unsigned int do_swap_flag, do_swap_cnt;
	//struct stat st;

	unsigned char *ddrptr1 = (unsigned char *)gVoFrame1AddrAssign;
	unsigned char *ddrptr2 = (unsigned char *)gVoFrame2AddrAssign;
	DOLBYVISIONEDR_DEV *dolbyvisionEDR = &dolbyvisionEDR_devices[0];
	dma_addr_t addr1, addr2;

	addr1 = dma_map_single(dolbyvisionEDR->dev, (void *)ddrptr1, (3840*2160*2), DMA_TO_DEVICE);
	addr2 = dma_map_single(dolbyvisionEDR->dev, (void *)ddrptr2, (3840*2160*2), DMA_TO_DEVICE);

	printk("%s...\n", __FUNCTION__);

	memset(ddrptr1, 0x00, (3840*2160*2));
	memset(ddrptr2, 0x00, (3840*2160*2));

	//fseek(rptr, 0L, SEEK_END);
	//filelength = ftell(rptr);
	//if (stat(ifilename, &st) == 0)
	//	filelength = st.st_size;
	//fseek(rptr, 0L, SEEK_SET);

	framebytes_plane_y = img_height * img_width * bpp;
	framebytes_plane_u = framebytes_plane_v = img_height / 2 * img_width / 2 * bpp;

	framesize = (framebytes_plane_y + framebytes_plane_u + framebytes_plane_v);
	framecount = (filelength / framesize);

	width_scale = (img_width + 255) / 256;
	blk_size = 512 << (page_size);
	blk_w = 64;
	blk_h = blk_size / blk_w;
	blk_total = (width_scale * 256 / blk_w)*((img_height + blk_h - 1) / blk_h);
	blk_wNum = (width_scale * 256 / blk_w);
	blk_hNum = ((img_height + blk_h - 1) / blk_h);

	//for (frameIdx = 0; frameIdx < framecount; frameIdx++)
	for (frameIdx = 0; frameIdx < 1; frameIdx++)
	{
		// test sample EL_rec_352x288_P420_8b.yuv
		// -f 3 -i 1006\EL_rec_352x288_P420_8b.yuv -o 1006\EL_rec_352x288_P420_8b.yuv
		// -f 3 -i mytest\bl_8bit_1920x1080.yuv -o mytest\bl_8bit_1920x1080.yuv -s 1920x1080
		// -f 3 -i 0001\BL_rec_64x64_P420_8b.yuv -o 0001\BL_rec_64x64_P420_8b.yuv -s 64x64
		// -f 3 -i 5000\bl_8bit_420_1920x1080.yuv -o 5000\bl_8bit_420_1920x1080.yuv -s 1920x1080
		// -f 3 -i 5000\el_8bit_420_960x540.yuv -o 5000\el_8bit_420_960x540.yuv -s 960x540

		if (frameIdx > 0)
		{
#if 0			// fix me later
			char buf[10];
			strcpy(filepath, ofilename);
			strcat(filepath, ".vodmaYBlk8bit_");
			itoa(frameIdx, buf, 10);
			strcat(filepath, buf);
			strcat(filepath, ".yuv");
			wptr_y = fopen(filepath, "wb");
			strcpy(filepath, ofilename);
			strcat(filepath, ".vodmaCBlk8bit_");
			itoa(frameIdx, buf, 10);
			strcat(filepath, buf);
			strcat(filepath, ".yuv");
			wptr_c = fopen(filepath, "wb");
#endif
		}

		//planeyoffset = framesize * frameIdx;
		planeyoffset = framesize * g_frameCtrlNo;
		planeuoffset = planeyoffset + framebytes_plane_y;
		planevoffset = planeuoffset + framebytes_plane_u;

		wplaneyoffset = wplaneuvoffset = 0;
		blk_hNum = ((img_height + blk_h - 1) / blk_h);

		// for Y block
		for (looph = 0; looph < blk_hNum; looph++)
		{
			planeyblkoffset = (looph * img_width * blk_h);
			blk_RowCnt = 0;

			for (loopw = 0; loopw < (width_scale * 256); blk_RowCnt++)
			{
				if ((loopw + blk_w - 1) < img_width)
				{
					do_swap_flag = do_swap_cnt = 0;
					for (h = 0; h < blk_h; h++)
					{
						if (((looph*blk_h)+h) >= img_height)  // over height
						{
							memset(&py, 0x00, sizeof(py));
						}
						else
						{
							// read Y
							//fseek(rptr, planeyoffset + planeyblkoffset + (img_width*h) + loopw, SEEK_SET);
							//fread(&py, blk_w, 1, rptr);
							file_read((unsigned char *)&py[0], blk_w, planeyoffset + planeyblkoffset + (img_width*h) + loopw, rptr);
						}

						//for (loop = 0; loop < blk_w; loop++)
						//	printf("%x\n", py[loop]);

						if (ddrNum == 2)
						{
							if (h >= 1)
								do_swap_cnt++;

							if ((do_swap_cnt % 4) == 1 || (do_swap_cnt % 4) == 2)
								do_swap_flag = 1;
							else
								do_swap_flag = 0;

							TwoDDR_and_DDR3_data_Mapping(py, do_swap_flag);
						}

						if (looph % 2 == 0)
						{
							//fseek(wptr_y, wplaneyoffset, SEEK_SET);
							//fwrite(&py, blk_w, 1, wptr_y);
							file_write(&py[0], blk_w, wplaneyoffset, wptr_y);

							// write to DDR
							ddrptr1 += wplaneyoffset;
							memcpy(ddrptr1, py, blk_w);
							ddrptr1 = (unsigned char *)gVoFrame1AddrAssign;

							wplaneyoffset += blk_w;
						}
						else
						{
							// re-arrangement the bank order in ODD row block
							// from bank 0,1,2,3 to bank 2,3,0,1
							if ((blk_RowCnt % 4) == 0 || (blk_RowCnt % 4) == 1)	// move to bank0 or 1 (shift right two blk_size)
							{
								//fseek(wptr_y, wplaneyoffset + (2 * blk_size), SEEK_SET);
								file_write(&py[0], blk_w, wplaneyoffset + (2 * blk_size), wptr_y);

								// write to DDR
								ddrptr1 += (wplaneyoffset + (2 * blk_size));
								memcpy(ddrptr1, py, blk_w);
								ddrptr1 = (unsigned char *)gVoFrame1AddrAssign;
							}
							else if ((blk_RowCnt % 4) == 2 || (blk_RowCnt % 4) == 3)	// move to bank0 (shift left two blk_size)
							{
								//fseek(wptr_y, wplaneyoffset - (2 * blk_size), SEEK_SET);
								file_write(&py[0], blk_w, wplaneyoffset - (2 * blk_size), wptr_y);

								// write to DDR
								ddrptr1 += (wplaneyoffset - (2 * blk_size));
								memcpy(ddrptr1, py, blk_w);
								ddrptr1 = (unsigned char *)gVoFrame1AddrAssign;
							}
							//fwrite(&py, blk_w, 1, wptr_y);
							//file_write(&py, blk_w, 1, wptr_y);
							wplaneyoffset += blk_w;
						}
					}
					loopw += blk_w;
				}
				else
				{
					for (h = 0; h < blk_h; h++)
					{
						// write dummy
						memset(py, 0x00, sizeof(py));

						if (loopw < img_width && (((looph*blk_h) + h) < img_height))
						{
							// read Y
							//fseek(rptr, planeyoffset + planeyblkoffset + (img_width*h) + loopw, SEEK_SET);
							//fread(&py, (img_width - loopw), 1, rptr);
							file_read((unsigned char *)&py[0], (img_width - loopw), planeyoffset + planeyblkoffset + (img_width*h) + loopw, rptr);
						}

						if (ddrNum == 2)
						{
							if (h >= 1)
								do_swap_cnt++;

							if ((do_swap_cnt % 4) == 1 || (do_swap_cnt % 4) == 2)
								do_swap_flag = 1;
							else
								do_swap_flag = 0;

							TwoDDR_and_DDR3_data_Mapping(py, do_swap_flag);
						}

						if (looph % 2 == 0)
						{
							//fseek(wptr_y, wplaneyoffset, SEEK_SET);
							//fwrite(&py, blk_w, 1, wptr_y);
							file_write(&py[0], blk_w, wplaneyoffset, wptr_y);

							// write to DDR
							ddrptr1 += wplaneyoffset;
							memcpy(ddrptr1, py, blk_w);
							ddrptr1 = (unsigned char *)gVoFrame1AddrAssign;

							wplaneyoffset += blk_w;
						}
						else
						{
							// re-arrangement the bank order in ODD row block
							// from bank 0,1,2,3 to bank 2,3,0,1
							if ((blk_RowCnt % 4) == 0 || (blk_RowCnt % 4) == 1)	// move to bank0 or 1 (shift right two blk_size)
							{
								//fseek(wptr_y, wplaneyoffset + (2 * blk_size), SEEK_SET);
								file_write(&py[0], blk_w, wplaneyoffset + (2 * blk_size), wptr_y);

								// write to DDR
								ddrptr1 += (wplaneyoffset + (2 * blk_size));
								memcpy(ddrptr1, py, blk_w);
								ddrptr1 = (unsigned char *)gVoFrame1AddrAssign;
							}
							else if ((blk_RowCnt % 4) == 2 || (blk_RowCnt % 4) == 3)	// move to bank0 (shift left two blk_size)
							{
								//fseek(wptr_y, wplaneyoffset - (2 * blk_size), SEEK_SET);
								file_write(&py[0], blk_w, wplaneyoffset - (2 * blk_size), wptr_y);

								// write to DDR
								ddrptr1 += (wplaneyoffset - (2 * blk_size));
								memcpy(ddrptr1, py, blk_w);
								ddrptr1 = (unsigned char *)gVoFrame1AddrAssign;
							}
							//fwrite(&py, blk_w, 1, wptr_y);
							wplaneyoffset += blk_w;
						}
					}
					loopw += blk_w;
				}
			}
		}

		// for UV
		blk_hNum = (((img_height / 2) + blk_h - 1) / blk_h);
		for (looph = 0; looph < blk_hNum; looph++)
		{
			//planeyoffset = framesize * frameIdx;
			planeyoffset = framesize * g_frameCtrlNo;
			planeuoffset = planeyoffset + framebytes_plane_y;
			planevoffset = planeuoffset + framebytes_plane_u;

			planevoffset += (looph * (img_width / 2) * blk_h);
			planeuoffset += (looph * (img_width / 2) * blk_h);
			blk_RowCnt = 0;

			for (loopw = 0; loopw < (width_scale * 256); blk_RowCnt++)
			{
				if ((loopw + blk_w - 1) < img_width)
				{
					do_swap_flag = do_swap_cnt = 0;
					for (h = 0; h < blk_h; h++)
					{
						if (((looph*blk_h) + h) >= (img_height / 2))  // over height
						{
							memset(&pcbcr[0], 0x00, sizeof(pcbcr));
						}
						else
						{
							for (w = 0; w < (blk_w / 2); w++)
							{
								// odd point is cr
								//fseek(rptr, planevoffset + ((img_width / 2)*h) + (loopw / 2) + w, SEEK_SET);
								//fread(&pcr, 1, 1, rptr);
								file_read((unsigned char *)&pcr, 1, planevoffset + ((img_width / 2)*h) + (loopw / 2) + w, rptr);

								// even point is cb
								//fseek(rptr, planeuoffset + ((img_width / 2)*h) + (loopw / 2) + w, SEEK_SET);
								//printf("%d \n", planeuoffset + ((img_width / 2)*h) + (loopw / 2) + w);
								//fread(&pcb, 1, 1, rptr);
								file_read((unsigned char *)&pcb, 1, planeuoffset + ((img_width / 2)*h) + (loopw / 2) + w, rptr);

								pcbcr[w * 2] = pcb;
								pcbcr[(w * 2) + 1] = pcr;

								//printf("%x\n", (pcb & 0xFF));
								//printf("%x\n", (pcr & 0xFF));
							}
						}

						if (ddrNum == 2)
						{
							if (h >= 1)
								do_swap_cnt++;

							if ((do_swap_cnt % 4) == 1 || (do_swap_cnt % 4) == 2)
								do_swap_flag = 1;
							else
								do_swap_flag = 0;

							TwoDDR_and_DDR3_data_Mapping(pcbcr, do_swap_flag);
						}

						if (looph % 2 == 0)
						{
							//fseek(wptr_c, wplaneuvoffset, SEEK_SET);
							//fwrite(&pcbcr, blk_w, 1, wptr_c);
							file_write(&pcbcr[0], blk_w, wplaneuvoffset, wptr_c);

							// write to DDR
							ddrptr2 += wplaneuvoffset;
							memcpy(ddrptr2, pcbcr, blk_w);
							ddrptr2 = (unsigned char *)gVoFrame2AddrAssign;

							wplaneuvoffset += blk_w;
						}
						else
						{
							// re-arrangement the bank order in ODD row block
							// from bank 0,1,2,3 to bank 2,3,0,1
							if ((blk_RowCnt % 4) == 0 || (blk_RowCnt % 4) == 1)	// move to bank0 or 1 (shift right two blk_size)
							{
								//fseek(wptr_c, wplaneuvoffset + (2 * blk_size), SEEK_SET);
								file_write(&pcbcr[0], blk_w, wplaneuvoffset + (2 * blk_size), wptr_c);

								// write to DDR
								ddrptr2 += (wplaneuvoffset + (2 * blk_size));
								memcpy(ddrptr2, pcbcr, blk_w);
								ddrptr2 = (unsigned char *)gVoFrame2AddrAssign;
							}
							else if ((blk_RowCnt % 4) == 2 || (blk_RowCnt % 4) == 3)	// move to bank0 (shift left two blk_size)
							{
								//fseek(wptr_c, wplaneuvoffset - (2 * blk_size), SEEK_SET);
								file_write(&pcbcr[0], blk_w, wplaneuvoffset - (2 * blk_size), wptr_c);

								// write to DDR
								ddrptr2 += (wplaneuvoffset - (2 * blk_size));
								memcpy(ddrptr2, pcbcr, blk_w);
								ddrptr2 = (unsigned char *)gVoFrame2AddrAssign;
							}
							//fwrite(&pcbcr, blk_w, 1, wptr_c);
							wplaneuvoffset += blk_w;
						}
					}

					loopw += blk_w;
				}
				else
				{
					for (h = 0; h < blk_h; h++)
					{
						// write dummy
						memset(pcbcr, 0x00, sizeof(pcbcr));

						if (loopw < img_width && (((looph*blk_h) + h) < (img_height / 2)))
						{
							for (w = 0; w < ((img_width - loopw) / 2); w++)
							{
								// odd point is cr
								//fseek(rptr, planevoffset + ((img_width / 2)*h) + (loopw / 2) + w, SEEK_SET);
								//fread(&pcr, 1, 1, rptr);
								file_read((unsigned char *)&pcr, 1, planevoffset + ((img_width / 2)*h) + (loopw / 2) + w, rptr);

								// even point is cb
								//fseek(rptr, planeuoffset + ((img_width / 2)*h) + (loopw / 2) + w, SEEK_SET);
								//printf("%d \n", planeuoffset + ((img_width / 2)*h) + w);
								//fread(&pcb, 1, 1, rptr);
								file_read((unsigned char *)&pcb, 1, planeuoffset + ((img_width / 2)*h) + (loopw / 2) + w, rptr);

								pcbcr[w * 2] = pcb;
								pcbcr[(w * 2) + 1] = pcr;
							}
						}

						if (ddrNum == 2)
						{
							if (h >= 1)
								do_swap_cnt++;

							if ((do_swap_cnt % 4) == 1 || (do_swap_cnt % 4) == 2)
								do_swap_flag = 1;
							else
								do_swap_flag = 0;

							TwoDDR_and_DDR3_data_Mapping(pcbcr, do_swap_flag);
						}

						if (looph % 2 == 0)
						{
							//fseek(wptr_c, wplaneuvoffset, SEEK_SET);
							//fwrite(&pcbcr, blk_w, 1, wptr_c);
							file_write(&pcbcr[0], blk_w, wplaneuvoffset, wptr_c);

							// write to DDR
							ddrptr2 += wplaneuvoffset;
							memcpy(ddrptr2, pcbcr, blk_w);
							ddrptr2 = (unsigned char *)gVoFrame2AddrAssign;

							wplaneuvoffset += blk_w;
						}
						else
						{
							// re-arrangement the bank order in ODD row block
							// from bank 0,1,2,3 to bank 2,3,0,1
							if ((blk_RowCnt % 4) == 0 || (blk_RowCnt % 4) == 1)	// move to bank0 or 1 (shift right two blk_size)
							{
								//fseek(wptr_c, wplaneuvoffset + (2 * blk_size), SEEK_SET);
								file_write(&pcbcr[0], blk_w, wplaneuvoffset + (2 * blk_size), wptr_c);

								// write to DDR
								ddrptr2 += (wplaneuvoffset + (2 * blk_size));
								memcpy(ddrptr2, pcbcr, blk_w);
								ddrptr2 = (unsigned char *)gVoFrame2AddrAssign;
							}
							else if ((blk_RowCnt % 4) == 2 || (blk_RowCnt % 4) == 3)	// move to bank0 (shift left two blk_size)
							{
								//fseek(wptr_c, wplaneuvoffset - (2 * blk_size), SEEK_SET);
								file_write(&pcbcr[0], blk_w, wplaneuvoffset - (2 * blk_size), wptr_c);

								// write to DDR
								ddrptr2 += (wplaneuvoffset - (2 * blk_size));
								memcpy(ddrptr2, pcbcr, blk_w);
								ddrptr2 = (unsigned char *)gVoFrame2AddrAssign;
							}
							//fwrite(&pcbcr, blk_w, 1, wptr_c);
							wplaneuvoffset += blk_w;
						}
					}
					loopw += blk_w;
				}
			}
		}

#if 0
		// close file pointer
		if (wptr_y != NULL)
			fclose(wptr_y);
		if (wptr_c != NULL)
			fclose(wptr_c);
		wptr_y = wptr_c = NULL;
#endif
	}

	dma_sync_single_for_device(dolbyvisionEDR->dev, addr1, (3840*2160*2), DMA_TO_DEVICE);
	dma_sync_single_for_device(dolbyvisionEDR->dev, addr2, (3840*2160*2), DMA_TO_DEVICE);
	dma_unmap_single(dolbyvisionEDR->dev, addr1, (3840*2160*2), DMA_TO_DEVICE);
	dma_unmap_single(dolbyvisionEDR->dev, addr2, (3840*2160*2), DMA_TO_DEVICE);
}



void Fmt_Cvt_10YUV420_to_10BLK444(struct file *rptr, struct file *wptr_y, struct file *wptr_c, unsigned int img_width, unsigned int img_height)
{
	//img_width = 1920;
	//img_height = 1080;
	unsigned int ddr_img_width = (img_width * 10 / 8);
	unsigned int width_scale, page_size = 3, blk_size, blk_h, blk_w, blk_total, blk_wNum, blk_hNum, blk_RowCnt;
	unsigned long filelength = 0, framecount = 0, framesize = 0/*, frameIdx = 0*/, bitIdx = 0, bitUIdx = 0, bitVIdx = 0, uvToogle = 0;
	short pcb, pcr, stmp;
	char py[64], pcbcr[64];
	char tmp;
	int bpp = 2;	// bytes per pixel
	unsigned int framebytes_plane_y, framebytes_plane_u, framebytes_plane_v;
	int loop, looph, loopw, h;//, w;
	unsigned int planeyoffset, planeuoffset, planevoffset, planeyblkoffset;
	unsigned int wplaneyoffset = 0, wplaneuvoffset = 0;
	unsigned int ddrNum = 2;	// two DDR and for DDR3
	//unsigned int *pi_py, *pi_pcbcr, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8, blkIdx;
	unsigned int do_swap_flag, do_swap_cnt;
	//struct stat st;

	unsigned int framebytes = ((img_width * img_height * 10) / 8);

	unsigned char *ddrptr1 = (unsigned char *)gVoFrame1AddrAssign;
	unsigned char *ddrptr2 = (unsigned char *)gVoFrame2AddrAssign;
	DOLBYVISIONEDR_DEV *dolbyvisionEDR = &dolbyvisionEDR_devices[0];
	dma_addr_t addr1, addr2;

	unsigned char *bitarray=0, *rowBlock=0;	// store the valid data only
	unsigned char *bitarray_cb=0, *bitarray_cr=0;

	addr1 = dma_map_single(dolbyvisionEDR->dev, (void *)ddrptr1, (3840*2160*2), DMA_TO_DEVICE);
	addr2 = dma_map_single(dolbyvisionEDR->dev, (void *)ddrptr2, (3840*2160*2), DMA_TO_DEVICE);

	printk("%s, Width=%d, Height=%d \n", __FUNCTION__, img_width, img_height);

	memset(ddrptr1, 0x00, (3840*2160*2));
	memset(ddrptr2, 0x00, (3840*2160*2));


	if (((img_width * img_height * 10) / 8) % 8)
		framebytes += 1;
	if ((framebytes % 8))
		framebytes += (8 - (framebytes % 8));


	//fseek(rptr, 0L, SEEK_END);
	//filelength = ftell(rptr);
	//if (stat(ifilename, &st) == 0)
	//	filelength = st.st_size;
	//fseek(rptr, 0L, SEEK_SET);

	framebytes_plane_y = img_height * img_width * bpp;
	framebytes_plane_u = framebytes_plane_v = img_height / 2 * img_width / 2 * bpp;

	framesize = (framebytes_plane_y + framebytes_plane_u + framebytes_plane_v);
	framecount = (filelength / framesize);

	width_scale = ((img_width * 10 / 8) + 255) / 256;
	blk_size = 512 << (page_size);
	blk_w = 64;
	blk_h = blk_size / blk_w;
	blk_total = (width_scale * 256 / blk_w)*((img_height + blk_h - 1) / blk_h);
	blk_wNum = (width_scale * 256 / blk_w);
	blk_hNum = ((img_height + blk_h - 1) / blk_h);

	bitarray = (unsigned char *)kmalloc(ddr_img_width * blk_h * 8, GFP_KERNEL);
	bitarray_cb = (unsigned char *)kmalloc(ddr_img_width * (blk_h / 2) * 8, GFP_KERNEL);
	bitarray_cr = (unsigned char *)kmalloc(ddr_img_width * (blk_h / 2) * 8, GFP_KERNEL);
	rowBlock = (unsigned char *)kmalloc(ddr_img_width * blk_h, GFP_KERNEL);
	if (rowBlock==0 || bitarray_cb==0 || bitarray_cr==0 || bitarray==0) {
		//klocwork mantis 0136619
		printk("%s, allocate failed \n", __FUNCTION__);
		return ;
	}
	/*
	---------------------------------------------------------------
	|  |  |  |  |  |  |  |  |¡E¡E¡E¡E¡E      |  |  |  |  |  |  |  |   <--- first row block and many banks
	---------------------------------------------------------------
	rowBlock stores one row block including all banks in row line.
	storing order follows the bank order from left to right
	*/

	//for (frameIdx = 0; frameIdx < framecount; frameIdx++)
	{
		// -f 4 -i 1006\BL_rec_352x288_P420_10b.yuv -o 1006\BL_rec_352x288_P420_10b.yuv -s 352x288
		// -f 4 -i 1005\EL_rec_352x288_P420_10b.yuv -o 1005\EL_rec_352x288_P420_10b.yuv -s 352x288
		// -f 4 -i mytest\bl_10bit_1920x1080.yuv -o mytest\bl_10bit_1920x1080.yuv -s 1920x1080
		// -f 4 -i 1007\BL_rec_352x288_P420_10b.yuv -o 1007\BL_rec_352x288_P420_10b.yuv -s 352x288
		// -f 4 -i 1007\EL_rec_352x288_P420_10b.yuv -o 1007\EL_rec_352x288_P420_10b.yuv -s 352x288

		//planeyoffset = framesize * frameIdx;
		planeyoffset = framesize * g_frameCtrlNo;
		planeuoffset = planeyoffset + framebytes_plane_y;
		planevoffset = planeuoffset + framebytes_plane_u;
		wplaneyoffset = wplaneuvoffset = 0;

		// for Y block read
		for (looph = 0; looph < blk_hNum; looph++)
		{
			planeyblkoffset = (looph * img_width * blk_h * 2);
			blk_RowCnt = bitIdx = 0;

			// reset something
			memset(bitarray, 0x00, (ddr_img_width * blk_h * 8));

			// read row bank block direction data
			for (h = 0; h < blk_h; h++)
			{
				for (loopw = 0; loopw < img_width; loopw++)
				{
					// read block Y
					//fseek(rptr, planeyoffset + planeyblkoffset + (h * img_width * 2) + (loopw * 2), SEEK_SET);
					//fread(&stmp, 2, 1, rptr);
					file_read((unsigned char *)&stmp, 2, planeyoffset + planeyblkoffset + (h * img_width * 2) + (loopw * 2), rptr);

					//printf("offset=0x%x\n", planeyoffset + planeyblkoffset + (h * img_width * 2) + (loopw * 2));

					// row block to bit stream into bitarry
					// store each pixel's 10 bit, MSB first
					for (loop = 9; loop >= 0; loop--)
						bitarray[bitIdx++] = (stmp & (1 << loop)) >> loop;

					//if (bitarray[bitIdx - 1] > 0 || stmp > 0)
					//	printf("stmp=0x%x\n", stmp);
				}
			}
			// store in rowBlock
			bitIdx = 0;
			for (loop = 0; loop < (ddr_img_width * blk_h); loop++)
			{
				tmp = 0;
				for (h = 7; h >= 0; h--)
					tmp |= (bitarray[bitIdx++] << h);
				rowBlock[loop] = tmp;
				//if ((tmp&0xFF) > 0)
				//	printf("0x%x\n", (tmp & 0xFF));
			}
			//printf("Row block bit count: %d \n", bitIdx);

			// Y block write process
			bitIdx = blk_RowCnt = 0;
			for (loopw = 0; loopw < (width_scale * 256); blk_RowCnt++)
			{
				if ((loopw + blk_w - 1) < ddr_img_width)
				{
					do_swap_flag = do_swap_cnt = 0;
					// fill one bank data by bitarray
					for (h = 0; h < blk_h; h++)
					{
						if (((looph*blk_h) + h) >= img_height)  // over height
						{
							memset(&py[0], 0x00, sizeof(py));
						}
						else
						{
							for (loop = 0; loop < blk_w; loop++)
							{
								py[loop] = rowBlock[(ddr_img_width*h) + loopw + loop];
								//if ((py[loop] & 0xFF) > 0 && looph==1 && loopw==128)
								//	printf("0x%x\n", py[loop]);
							}
						}

						if (ddrNum == 2)
						{
							if (h >= 1)
								do_swap_cnt++;

							if ((do_swap_cnt % 4) == 1 || (do_swap_cnt % 4) == 2)
								do_swap_flag = 1;
							else
								do_swap_flag = 0;

							TwoDDR_and_DDR3_data_Mapping(py, do_swap_flag);
						}

						if (looph % 2 == 0)
						{
							//fseek(wptr_y, wplaneyoffset, SEEK_SET);
							//fwrite(&py, blk_w, 1, wptr_y);
							file_write(&py[0], blk_w, wplaneyoffset, wptr_y);


							// write to DDR
							ddrptr1 += wplaneyoffset;
							memcpy(ddrptr1, py, blk_w);
							ddrptr1 = (unsigned char *)gVoFrame1AddrAssign;

							wplaneyoffset += blk_w;
						}
						else
						{
							// re-arrangement the bank order in ODD row block
							// from bank 0,1,2,3 to bank 2,3,0,1
							if ((blk_RowCnt % 4) == 0 || (blk_RowCnt % 4) == 1)	// move to bank0 or 1 (shift right two blk_size)
							{
								//fseek(wptr_y, wplaneyoffset + (2 * blk_size), SEEK_SET);
								file_write(&py[0], blk_w, wplaneyoffset + (2 * blk_size), wptr_y);

								// write to DDR
								ddrptr1 += (wplaneyoffset + (2 * blk_size));
								memcpy(ddrptr1, py, blk_w);
								ddrptr1 = (unsigned char *)gVoFrame1AddrAssign;
							}
							else if ((blk_RowCnt % 4) == 2 || (blk_RowCnt % 4) == 3)	// move to bank0 (shift left two blk_size)
							{
								//fseek(wptr_y, wplaneyoffset - (2 * blk_size), SEEK_SET);
								file_write(&py[0], blk_w, wplaneyoffset - (2 * blk_size), wptr_y);

								// write to DDR
								ddrptr1 += (wplaneyoffset - (2 * blk_size));
								memcpy(ddrptr1, py, blk_w);
								ddrptr1 = (unsigned char *)gVoFrame1AddrAssign;
							}

							//fwrite(&py, blk_w, 1, wptr_y);
							wplaneyoffset += blk_w;
						}
					}
					loopw += blk_w;
				}
				else
				{
					do_swap_flag = do_swap_cnt = 0;
					for (h = 0; h < blk_h; h++)
					{
						// write dummy
						memset(py, 0x00, sizeof(py));

						if (loopw < ddr_img_width && (((looph*blk_h) + h) < img_height))
						{
							for (loop = 0; loop < (ddr_img_width - loopw); loop++)
								py[loop] = rowBlock[(ddr_img_width*h) + loopw + loop];
						}

						if (ddrNum == 2)
						{
							if (h >= 1)
								do_swap_cnt++;

							if ((do_swap_cnt % 4) == 1 || (do_swap_cnt % 4) == 2)
								do_swap_flag = 1;
							else
								do_swap_flag = 0;

							TwoDDR_and_DDR3_data_Mapping(py, do_swap_flag);
						}

						if (looph % 2 == 0)
						{
							//fseek(wptr_y, wplaneyoffset, SEEK_SET);
							//fwrite(&py, blk_w, 1, wptr_y);
							file_write(&py[0], blk_w, wplaneyoffset, wptr_y);


							// write to DDR
							ddrptr1 += wplaneyoffset;
							memcpy(ddrptr1, py, blk_w);
							ddrptr1 = (unsigned char *)gVoFrame1AddrAssign;

							wplaneyoffset += blk_w;
						}
						else
						{
							// re-arrangement the bank order in ODD row block
							// from bank 0,1,2,3 to bank 2,3,0,1
							if ((blk_RowCnt % 4) == 0 || (blk_RowCnt % 4) == 1)	// move to bank0 or 1 (shift right two blk_size)
							{
								//fseek(wptr_y, wplaneyoffset + (2 * blk_size), SEEK_SET);
								file_write(&py[0], blk_w, wplaneyoffset + (2 * blk_size), wptr_y);

								// write to DDR
								ddrptr1 += (wplaneyoffset + (2 * blk_size));
								memcpy(ddrptr1, py, blk_w);
								ddrptr1 = (unsigned char *)gVoFrame1AddrAssign;
							}
							else if ((blk_RowCnt % 4) == 2 || (blk_RowCnt % 4) == 3)	// move to bank0 (shift left two blk_size)
							{
								//fseek(wptr_y, wplaneyoffset - (2 * blk_size), SEEK_SET);
								file_write(&py[0], blk_w, wplaneyoffset - (2 * blk_size), wptr_y);

								// write to DDR
								ddrptr1 += (wplaneyoffset - (2 * blk_size));
								memcpy(ddrptr1, py, blk_w);
								ddrptr1 = (unsigned char *)gVoFrame1AddrAssign;
							}

							//fwrite(&py, blk_w, 1, wptr_y);
							wplaneyoffset += blk_w;
						}
					}
					loopw += blk_w;
				}
			}
		}


		// for UV (C) Block
		blk_hNum = (((img_height / 2) + blk_h - 1) / blk_h);
		for (looph = 0; looph < blk_hNum; looph++)
		{
			//planeyoffset = framesize * frameIdx;
			planeyoffset = framesize * g_frameCtrlNo;
			planeuoffset = planeyoffset + framebytes_plane_y;
			planevoffset = planeuoffset + framebytes_plane_u;

			planevoffset += (looph * (img_width / 2) * blk_h * 2);
			planeuoffset += (looph * (img_width / 2) * blk_h * 2);
			bitUIdx = bitVIdx = blk_RowCnt = 0;

			memset(bitarray_cb, 0x00, (ddr_img_width * (blk_h / 2) * 8));
			memset(bitarray_cr, 0x00, (ddr_img_width * (blk_h / 2) * 8));

			// read row bank block direction data
			for (h = 0; h < blk_h; h++)
			{
				for (loopw = 0; loopw < (img_width / 2); loopw++)
				{
					if (((looph*blk_h) + h) >= (img_height/2))  // over height
						break;

					// read block U & V
					//fseek(rptr, planeuoffset + (h*(img_width/2) * 2) + (loopw * 2), SEEK_SET);
					//fread(&pcb, 2, 1, rptr);
					file_read((unsigned char *)&pcb, 2, planeuoffset + (h*(img_width/2) * 2) + (loopw * 2), rptr);
					//fseek(rptr, planevoffset + (h*(img_width/2) * 2) + (loopw * 2), SEEK_SET);
					//fread(&pcr, 2, 1, rptr);
					file_read((unsigned char *)&pcr, 2, planevoffset + (h*(img_width/2) * 2) + (loopw * 2), rptr);

					//printf("%x\n", pcb);
					//printf("%x\n", pcr);

					// row block to bit stream into bitarry
					// store each pixel's 10 bit, MSB first
					for (loop = 9; loop >= 0; loop--)
					{
						bitarray_cb[bitUIdx++] = (pcb & (1 << loop)) >> loop;
						bitarray_cr[bitVIdx++] = (pcr & (1 << loop)) >> loop;
					}
				}
			}
			// store in rowBlock
			bitUIdx = bitVIdx = bitIdx = uvToogle = 0;
			for (loop = 0; loop < (ddr_img_width * blk_h); loop++)
			{
				tmp = 0;
				for (h = 7; h >= 0; h--)
				{
					if (uvToogle == 0) // for Cb
						tmp |= (bitarray_cb[bitUIdx++] << h);
					else
						tmp |= (bitarray_cr[bitVIdx++] << h);

					bitIdx++;
					if (((bitIdx / 10) % 2) == 0)	// Now for Cb write
						uvToogle = 0;
					else
						uvToogle = 1;				// Now for Cr write
				}
				rowBlock[loop] = tmp;
				//if ((tmp&0xFF) > 0)
				//	printf("0x%x\n", (tmp & 0xFF));
			}
			//printf("Row Cb block bit count: %d \n", bitUIdx);
			//printf("Row Cr block bit count: %d \n", bitVIdx);


			// UV (C) Block write process
			bitIdx = bitUIdx = bitVIdx = uvToogle = 0;	// bitIdx use for totoal bit index
			for (loopw = 0; loopw < (width_scale * 256); blk_RowCnt++)
			{
				if ((loopw + blk_w - 1) < ddr_img_width)
				{
					do_swap_flag = do_swap_cnt = 0;
					for (h = 0; h < blk_h; h++)
					{
						if (((looph*blk_h) + h) >= (img_height/2))  // over height
						{
							memset(&pcbcr[0], 0x00, sizeof(pcbcr));
						}
						else
						{
							for (loop = 0; loop < blk_w; loop++)
							{
								pcbcr[loop] = rowBlock[(ddr_img_width*h) + loopw + loop];
							}
						}

						if (ddrNum == 2)
						{
							if (h >= 1)
								do_swap_cnt++;

							if ((do_swap_cnt % 4) == 1 || (do_swap_cnt % 4) == 2)
								do_swap_flag = 1;
							else
								do_swap_flag = 0;

							TwoDDR_and_DDR3_data_Mapping(pcbcr, do_swap_flag);
						}

						if (looph % 2 == 0)
						{
							//fseek(wptr_c, wplaneuvoffset, SEEK_SET);
							//fwrite(&pcbcr, blk_w, 1, wptr_c);
							file_write(&pcbcr[0], blk_w, wplaneuvoffset, wptr_c);


							// write to DDR
							ddrptr2 += wplaneuvoffset;
							memcpy(ddrptr2, pcbcr, blk_w);
							ddrptr2 = (unsigned char *)gVoFrame2AddrAssign;

							wplaneuvoffset += blk_w;
						}
						else
						{
							// re-arrangement the bank order in ODD row block
							// from bank 0,1,2,3 to bank 2,3,0,1
							if ((blk_RowCnt % 4) == 0 || (blk_RowCnt % 4) == 1)	// move to bank0 or 1 (shift right two blk_size)
							{
								//fseek(wptr_c, wplaneuvoffset + (2 * blk_size), SEEK_SET);
								file_write(&pcbcr[0], blk_w, wplaneuvoffset + (2 * blk_size), wptr_c);

								// write to DDR
								ddrptr2 += (wplaneuvoffset + (2 * blk_size));
								memcpy(ddrptr2, pcbcr, blk_w);
								ddrptr2 = (unsigned char *)gVoFrame2AddrAssign;
							}
							else if ((blk_RowCnt % 4) == 2 || (blk_RowCnt % 4) == 3)	// move to bank0 (shift left two blk_size)
							{
								//fseek(wptr_c, wplaneuvoffset - (2 * blk_size), SEEK_SET);
								file_write(&pcbcr[0], blk_w, wplaneuvoffset - (2 * blk_size), wptr_c);

								// write to DDR
								ddrptr2 += (wplaneuvoffset - (2 * blk_size));
								memcpy(ddrptr2, pcbcr, blk_w);
								ddrptr2 = (unsigned char *)gVoFrame2AddrAssign;
							}

							//fwrite(&pcbcr, blk_w, 1, wptr_c);
							wplaneuvoffset += blk_w;
						}
					}
					loopw += blk_w;
				}
				else
				{
					do_swap_flag = do_swap_cnt = 0;
					for (h = 0; h < blk_h; h++)
					{
						// write dummy
						memset(pcbcr, 0x00, sizeof(pcbcr));

						if (loopw < ddr_img_width && (((looph*blk_h) + h) < (img_height/2)))
						{
							for (loop = 0; loop < (ddr_img_width - loopw); loop++)
								pcbcr[loop] = rowBlock[(ddr_img_width*h) + loopw + loop];
						}

						if (ddrNum == 2)
						{
							if (h >= 1)
								do_swap_cnt++;

							if ((do_swap_cnt % 4) == 1 || (do_swap_cnt % 4) == 2)
								do_swap_flag = 1;
							else
								do_swap_flag = 0;

							TwoDDR_and_DDR3_data_Mapping(pcbcr, do_swap_flag);
						}

						if (looph % 2 == 0)
						{
							//seek(wptr_c, wplaneuvoffset, SEEK_SET);
							//fwrite(&pcbcr, blk_w, 1, wptr_c);
							file_write(&pcbcr[0], blk_w, wplaneuvoffset, wptr_c);


							// write to DDR
							ddrptr2 += wplaneuvoffset;
							memcpy(ddrptr2, pcbcr, blk_w);
							ddrptr2 = (unsigned char *)gVoFrame2AddrAssign;

							wplaneuvoffset += blk_w;
						}
						else
						{
							// re-arrangement the bank order in ODD row block
							// from bank 0,1,2,3 to bank 2,3,0,1
							if ((blk_RowCnt % 4) == 0 || (blk_RowCnt % 4) == 1)	// move to bank0 or 1 (shift right two blk_size)
							{
								//fseek(wptr_c, wplaneuvoffset + (2 * blk_size), SEEK_SET);
								file_write(&pcbcr[0], blk_w, wplaneuvoffset + (2 * blk_size), wptr_c);

								// write to DDR
								ddrptr2 += (wplaneuvoffset + (2 * blk_size));
								memcpy(ddrptr2, pcbcr, blk_w);
								ddrptr2 = (unsigned char *)gVoFrame2AddrAssign;
							}
							else if ((blk_RowCnt % 4) == 2 || (blk_RowCnt % 4) == 3)	// move to bank0 (shift left two blk_size)
							{
								//fseek(wptr_c, wplaneuvoffset - (2 * blk_size), SEEK_SET);
								file_write(&pcbcr[0], blk_w, wplaneuvoffset - (2 * blk_size), wptr_c);

								// write to DDR
								ddrptr2 += (wplaneuvoffset - (2 * blk_size));
								memcpy(ddrptr2, pcbcr, blk_w);
								ddrptr2 = (unsigned char *)gVoFrame2AddrAssign;
							}

							//fwrite(&pcbcr, blk_w, 1, wptr_c);
							wplaneuvoffset += blk_w;
						}
					}
					loopw += blk_w;
				}
			}
		}
	}

	kfree(bitarray);
	kfree(bitarray_cb);
	kfree(bitarray_cr);
	kfree(rowBlock);

	dma_sync_single_for_device(dolbyvisionEDR->dev, addr1, (3840*2160*2), DMA_TO_DEVICE);
	dma_sync_single_for_device(dolbyvisionEDR->dev, addr2, (3840*2160*2), DMA_TO_DEVICE);
	dma_unmap_single(dolbyvisionEDR->dev, addr1, (3840*2160*2), DMA_TO_DEVICE);
	dma_unmap_single(dolbyvisionEDR->dev, addr2, (3840*2160*2), DMA_TO_DEVICE);
}


void Fmt_DMCRF422_Mcap_to_DM(unsigned int mcapAddr, DOLBYVISION_PATTERN *dolby)
{
	unsigned short mY, mCb, mCr;
	unsigned char mTmp;
	unsigned long idx, idy, idx_byte, bit_idx, fileoffset = 0;//, outfileOffset = 0;
	unsigned char *addrptr = (unsigned char *)mcapAddr;
	unsigned short sY, sC;
	char tmpStr[500];
	char numStr[8];
	unsigned int lineBitCnt, testcnt = 0;
	unsigned char *bitarray = NULL;	// store the valid data only
	static struct file *SrcOutfp = NULL;


	printk("%s, Width=%d, Height=%d \n", __FUNCTION__, dolby->ColNumTtl, dolby->RowNumTtl);
	flush_cache_all();

	if (gFrame1Outfp != NULL) {
		file_sync(gFrame1Outfp);
		file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}
	if (SrcOutfp != NULL) {
		file_sync(SrcOutfp);
		file_close(SrcOutfp);
		SrcOutfp = NULL;
	}

	strcpy(tmpStr, dolby->fileOutpath);
	tmpStr[strlen(tmpStr)-3] = '\0';
	snprintf(numStr, 8, "%03d.yuv", g_frameCtrlNo);
	strcat(tmpStr, numStr);

	if (gFrame1Outfp == NULL)
		gFrame1Outfp = file_open(tmpStr, O_TRUNC | O_WRONLY | O_CREAT, 0);
	if (gFrame1Outfp == NULL) {
		printk("%s, gFrame1Outfp file open failed \n", __FUNCTION__);
		gFrame1Outfp = NULL;
		return ;
	}
	if (SrcOutfp == NULL)
		SrcOutfp = file_open("/var/mcap_source.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
	if (SrcOutfp == NULL) {
		printk("%s, SrcOutfp file open failed \n", __FUNCTION__);
		SrcOutfp = NULL;
		return ;
	}

#ifdef _SOFT_CAL_
	if ((dolby->ColNumTtl* 30) % 64)
		lineBitCnt = ((dolby->ColNumTtl* 30) / 64) + 4;
	else
		lineBitCnt = (dolby->ColNumTtl* 30) / 64;

	lineBitCnt *= 64;
#else
	lineBitCnt = MDOMAIN_CAP_DDR_In1WTLVL_Num_get_in1_write_num(rtd_inl(MDOMAIN_CAP_DDR_In1WTLVL_Num_reg));
	lineBitCnt *= MDOMAIN_CAP_DDR_In1WrLen_Rem_get_in1_wrlen(rtd_inl(MDOMAIN_CAP_DDR_In1WrLen_Rem_reg));
	lineBitCnt += MDOMAIN_CAP_DDR_In1WrLen_Rem_get_in1_write_remain(rtd_inl(MDOMAIN_CAP_DDR_In1WrLen_Rem_reg));
	lineBitCnt /= dolby->RowNumTtl;
	//lineBitCnt *= MDOMAIN_CAP_DDR_In1WrLen_Rem_get_in1_wrlen(rtd_inl(MDOMAIN_CAP_DDR_In1WrLen_Rem_reg));
	//
	if (lineBitCnt <= 0) {
		printk("%s, lineBitCnt= %d * %d + %d / %d\n", __FUNCTION__,
			MDOMAIN_CAP_DDR_In1WTLVL_Num_get_in1_write_num(rtd_inl(MDOMAIN_CAP_DDR_In1WTLVL_Num_reg)),
			MDOMAIN_CAP_DDR_In1WrLen_Rem_get_in1_wrlen(rtd_inl(MDOMAIN_CAP_DDR_In1WrLen_Rem_reg)),
			MDOMAIN_CAP_DDR_In1WrLen_Rem_get_in1_write_remain(rtd_inl(MDOMAIN_CAP_DDR_In1WrLen_Rem_reg)),
			dolby->RowNumTtl);
		printk("%s, lineBitCnt error get %d, set 1  \n", __FUNCTION__, lineBitCnt);
		lineBitCnt = 1;
	}

		printk("%s, lineBitCnt= %d * %d + %d / %d\n", __FUNCTION__,
			MDOMAIN_CAP_DDR_In1WTLVL_Num_get_in1_write_num(rtd_inl(MDOMAIN_CAP_DDR_In1WTLVL_Num_reg)),
			MDOMAIN_CAP_DDR_In1WrLen_Rem_get_in1_wrlen(rtd_inl(MDOMAIN_CAP_DDR_In1WrLen_Rem_reg)),
			MDOMAIN_CAP_DDR_In1WrLen_Rem_get_in1_write_remain(rtd_inl(MDOMAIN_CAP_DDR_In1WrLen_Rem_reg)),
			dolby->RowNumTtl);
	lineBitCnt *= 64;
#endif

	bitarray = (unsigned char *)kmalloc(lineBitCnt, GFP_KERNEL);
	if (bitarray == NULL) {
		printk("%s, bitarray kmalloc failed  \n", __FUNCTION__);
		return ;
	}
	printk("%s, lineBitCnt=%d, mcapAddr=0x%x \n", __FUNCTION__, lineBitCnt, mcapAddr);

	for (idy = 0; idy < dolby->RowNumTtl; idy++) {

		addrptr = (unsigned char *)(mcapAddr + ((lineBitCnt/8) * idy));
		bit_idx = 0;
		memset(bitarray, 0x00, lineBitCnt);

		for (idx = 0; idx < lineBitCnt; idx+=8) {
			mTmp = *addrptr;

			if (testcnt < 10 && idy == 0) {
				printk("%s, mTmp=0x%x, addrptr=0x%x \n", __FUNCTION__, (unsigned int)mTmp, (unsigned int)addrptr);
				testcnt++;
			}

			// M-cap DDR content output
			//file_write(&mTmp, 1, outfileOffset, SrcOutfp);
			//outfileOffset++;

			for (idx_byte = 0; idx_byte < 8; idx_byte++)
				bitarray[bit_idx++] = (mTmp & (1<<idx_byte)) >> idx_byte;
			addrptr++;
		}

		bit_idx = testcnt = 0;

		for (idx = 0; idx < dolby->ColNumTtl; idx++) {

			mCr = bitarray[bit_idx+0] | (bitarray[bit_idx+1]<<1) | (bitarray[bit_idx+2]<<2) |
				  (bitarray[bit_idx+3]<<3) | (bitarray[bit_idx+4]<<4) | (bitarray[bit_idx+5]<<5) |
				  (bitarray[bit_idx+6]<<6) | (bitarray[bit_idx+7]<<7) | (bitarray[bit_idx+8]<<8) |
				  (bitarray[bit_idx+9]<<9);
			mCb = bitarray[bit_idx+10] | (bitarray[bit_idx+11]<<1) | (bitarray[bit_idx+12]<<2) |
				  (bitarray[bit_idx+13]<<3) | (bitarray[bit_idx+14]<<4) | (bitarray[bit_idx+15]<<5) |
				  (bitarray[bit_idx+16]<<6) | (bitarray[bit_idx+17]<<7) | (bitarray[bit_idx+18]<<8) |
				  (bitarray[bit_idx+19]<<9);
			mY = bitarray[bit_idx+20] | (bitarray[bit_idx+21]<<1) | (bitarray[bit_idx+22]<<2) |
				  (bitarray[bit_idx+23]<<3) | (bitarray[bit_idx+24]<<4) | (bitarray[bit_idx+25]<<5) |
				  (bitarray[bit_idx+26]<<6) | (bitarray[bit_idx+27]<<7) | (bitarray[bit_idx+28]<<8) |
				  (bitarray[bit_idx+29]<<9);

			bit_idx += 30;

			sY = sC = 0;
			sY = (((mY&0x3FC)>>2)<<4) | ((mCb&0x3C)>>2);
			sC = ((mCb&0x3C0)>>6) | (((mCr&0x3FC)>>2)<<4);


			if (testcnt < 5 && idy == 0) {
				printk("%s, mY=0x%x, mCb=0x%x, mCr=0x%x \n", __FUNCTION__, mY, mCb, mCr);
				printk("%s, sY=0x%x, sC=0x%x \n", __FUNCTION__, sY, sC);
				testcnt++;
			}

			// UYVY output
			file_write((unsigned char *)&sC, 2, fileoffset, gFrame1Outfp);
			fileoffset += 2;
			file_write((unsigned char *)&sY, 2, fileoffset, gFrame1Outfp);
			fileoffset += 2;

		}

		flush_cache_all();
	}

	kfree(bitarray);
	file_sync(gFrame1Outfp);
	file_close(gFrame1Outfp);
	gFrame1Outfp = NULL;
	file_sync(SrcOutfp);
	file_close(SrcOutfp);
	SrcOutfp = NULL;

}

void Fmt_DMCRFOTT_Mcap_to_DM(unsigned int mcapAddr, unsigned int mcapAddr2, DOLBYVISION_PATTERN *dolby)
{
	unsigned short mY, mCb, mCr;//, keepCb, keepCr;
	unsigned char mTmp, m2NibbleCrTmp, m2NibbleCbTmp, m2NibbleYTmp;
	unsigned long idx, idy, idx_byte, bit_idx, fileoffset = 0;//, outfileOffset = 0, cSwapIdx = 0;
#ifdef DOLBLY_12B_MODE
	unsigned short keepCb, keepCr;
	unsigned long cSwapIdx = 0;
#endif
	unsigned char *addrptr = (unsigned char *)mcapAddr;
#if 0
	unsigned char *addr2ptr = (unsigned char *)mcapAddr2;
#endif
	unsigned short sY, sC;
	char tmpStr[500];
	char numStr[8];
	unsigned int lineBitCnt, lineBitCnt2, testcnt = 0;//, sCtmpROffset = 0, sCtmpWOffset = 0;
	unsigned char *bitarray = NULL; // store the valid data only
	static struct file *SrcOutfp = NULL;
	//unsigned short *sCtmp = NULL;


	printk("%s, Width=%d, Height=%d \n", __FUNCTION__, dolby->ColNumTtl, dolby->RowNumTtl);
	flush_cache_all();
	flush_cache_all();
	flush_cache_all();
	flush_cache_all();

	if (gFrame1Outfp != NULL) {
		file_sync(gFrame1Outfp);
		file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}
	if (SrcOutfp != NULL) {
		file_sync(SrcOutfp);
		file_close(SrcOutfp);
		SrcOutfp = NULL;
	}

	strcpy(tmpStr, dolby->fileOutpath);
	tmpStr[strlen(tmpStr)-3] = '\0';
	snprintf(numStr, 8, "%03d.yuv", g_frameCtrlNo);
	strcat(tmpStr, numStr);

	if (gFrame1Outfp == NULL)
		gFrame1Outfp = file_open(tmpStr, O_TRUNC | O_WRONLY | O_CREAT, 0);
	if (gFrame1Outfp == NULL) {
		printk("%s, gFrame1Outfp file open failed \n", __FUNCTION__);
		gFrame1Outfp = NULL;
		return ;
	}
	if (SrcOutfp == NULL)
		SrcOutfp = file_open("/var/mcap_source.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
	if (SrcOutfp == NULL) {
		printk("%s, SrcOutfp file open failed \n", __FUNCTION__);
		SrcOutfp = NULL;
		return ;
	}

#ifdef _SOFT_CAL_
	if ((dolby->ColNumTtl* 30) % 64)
		lineBitCnt = ((dolby->ColNumTtl* 30) / 64) + 4;
	else
		lineBitCnt = (dolby->ColNumTtl* 30) / 64;

	lineBitCnt *= 64;
#else
	lineBitCnt = MDOMAIN_CAP_DDR_In1WTLVL_Num_get_in1_write_num(rtd_inl(MDOMAIN_CAP_DDR_In1WTLVL_Num_reg));
	lineBitCnt *= MDOMAIN_CAP_DDR_In1WrLen_Rem_get_in1_wrlen(rtd_inl(MDOMAIN_CAP_DDR_In1WrLen_Rem_reg));
	lineBitCnt += MDOMAIN_CAP_DDR_In1WrLen_Rem_get_in1_write_remain(rtd_inl(MDOMAIN_CAP_DDR_In1WrLen_Rem_reg));
	lineBitCnt /= dolby->RowNumTtl;
	lineBitCnt *= 64;
	//lineBitCnt *= MDOMAIN_CAP_DDR_In1WrLen_Rem_get_in1_wrlen(rtd_inl(MDOMAIN_CAP_DDR_In1WrLen_Rem_reg));
	printk("%s, lineBitCnt= %d * %d + %d / %d * 64 = %d\n", __FUNCTION__,
		MDOMAIN_CAP_DDR_In1WTLVL_Num_get_in1_write_num(rtd_inl(MDOMAIN_CAP_DDR_In1WTLVL_Num_reg)),
		MDOMAIN_CAP_DDR_In1WrLen_Rem_get_in1_wrlen(rtd_inl(MDOMAIN_CAP_DDR_In1WrLen_Rem_reg)),
		MDOMAIN_CAP_DDR_In1WrLen_Rem_get_in1_write_remain(rtd_inl(MDOMAIN_CAP_DDR_In1WrLen_Rem_reg)),
		dolby->RowNumTtl, lineBitCnt);

	if (lineBitCnt <= 0)
		return;

	//lineBitCnt2 = MDOMAIN_CAP_DDR_In2WTLVL_get_in2_write_num(rtd_inl(MDOMAIN_CAP_DDR_In2WTLVL_reg));
	//lineBitCnt2 *= MDOMAIN_CAP_DDR_In2WrLen_Rem_get_in2_wrlen(rtd_inl(MDOMAIN_CAP_DDR_In2WrLen_Rem_reg));
	//lineBitCnt2 += MDOMAIN_CAP_DDR_In2WrLen_Rem_get_in2_write_remain(rtd_inl(MDOMAIN_CAP_DDR_In2WrLen_Rem_reg));
	//lineBitCnt2 /= dolby->RowNumTtl;
	//lineBitCnt2 *= MDOMAIN_CAP_DDR_In1WrLen_Rem_get_in1_wrlen(rtd_inl(MDOMAIN_CAP_DDR_In1WrLen_Rem_reg));
	//lineBitCnt2 *= 64;
#endif

	bitarray = (unsigned char *)kmalloc(lineBitCnt, GFP_KERNEL);
	//sCtmp = (unsigned short *)kmalloc((dolby->RowNumTtl*dolby->ColNumTtl)*2, GFP_KERNEL);
	if (bitarray == NULL) {
		printk("%s, bitarray kmalloc failed  \n", __FUNCTION__);
		return ;
	}
	//if (sCtmp == NULL) {
	//	printk("%s, sCtmp kmalloc failed  \n", __FUNCTION__);
	//	return ;
	//}
	//memset(sCtmp, 0x00, (dolby->RowNumTtl*dolby->ColNumTtl));
	printk("%s, lineBitCnt=%d, lineBitCnt2=%d, mcapAddr=0x%x, mcapAddr2=0x%x \n", __FUNCTION__, lineBitCnt, lineBitCnt2, mcapAddr, mcapAddr2);

	for (idy = 0; idy < dolby->RowNumTtl; idy++) {

		addrptr = (unsigned char *)(mcapAddr + ((lineBitCnt/8) * idy));
		//addr2ptr = (unsigned char *)(mcapAddr2 + ((lineBitCnt2/8) * idy));
		bit_idx = 0;
		memset(bitarray, 0, lineBitCnt);

		for (idx = 0; idx < lineBitCnt; idx+=8) {
			mTmp = *addrptr;

			if (testcnt < 10 && idy == 0) {
				printk("%s, mTmp=0x%x, addrptr=0x%x \n", __FUNCTION__, (unsigned int)mTmp, (unsigned int)addrptr);
				testcnt++;
			}

			// M-cap DDR content output
			//file_write(&mTmp, 1, outfileOffset, SrcOutfp);
			//outfileOffset++;

			for (idx_byte = 0; idx_byte < 8; idx_byte++)
				bitarray[bit_idx++] = (mTmp >> idx_byte)&0x1;
				//bitarray[bit_idx++] = (mTmp & (1<<idx_byte)) >> idx_byte;
			addrptr++;
		}

		bit_idx = testcnt = 0;

		flush_cache_all();

		for (idx = 0; idx < dolby->ColNumTtl; idx++) {

mCr = mCb = mY=0;
			mCr = bitarray[bit_idx+0] | (bitarray[bit_idx+1]<<1) | (bitarray[bit_idx+2]<<2) |
				  (bitarray[bit_idx+3]<<3) | (bitarray[bit_idx+4]<<4) | (bitarray[bit_idx+5]<<5) |
				  (bitarray[bit_idx+6]<<6) | (bitarray[bit_idx+7]<<7) | (bitarray[bit_idx+8]<<8) |
				  (bitarray[bit_idx+9]<<9);
			mCb = bitarray[bit_idx+10] | (bitarray[bit_idx+11]<<1) | (bitarray[bit_idx+12]<<2) |
				  (bitarray[bit_idx+13]<<3) | (bitarray[bit_idx+14]<<4) | (bitarray[bit_idx+15]<<5) |
				  (bitarray[bit_idx+16]<<6) | (bitarray[bit_idx+17]<<7) | (bitarray[bit_idx+18]<<8) |
				  (bitarray[bit_idx+19]<<9);
			mY = bitarray[bit_idx+20] | (bitarray[bit_idx+21]<<1) | (bitarray[bit_idx+22]<<2) |
				  (bitarray[bit_idx+23]<<3) | (bitarray[bit_idx+24]<<4) | (bitarray[bit_idx+25]<<5) |
				  (bitarray[bit_idx+26]<<6) | (bitarray[bit_idx+27]<<7) | (bitarray[bit_idx+28]<<8) |
				  (bitarray[bit_idx+29]<<9);

			bit_idx += 30;
#if 0
			if(idx <=3 && idy==0)
				printk("idx=%d, mCr=%08x, mCb=%08x, mY=%08x\n", idx, mCr, mCb, mY);

			mTmp = *addr2ptr;
			m2NibbleCrTmp = mTmp >> 4;
			addr2ptr++;

			mTmp = *addr2ptr;
			m2NibbleCbTmp = mTmp >> 4;
			addr2ptr++;

			mTmp = *addr2ptr;
			m2NibbleYTmp = mTmp >> 4;
			addr2ptr++;
#endif
			if ((idx % 22) == 0)
				flush_cache_all();


#ifdef DOLBLY_12B_MODE
			sY = sC = 0;
			sY = (mY << 2) & 0xFF0;

			//if ((idx % 2) == 0) {
				keepCr = mCr;
			//	keepCb = mCb;
			//}
			//if ((cSwapIdx % 2) == 0) {
			//	sC = (keepCb << 2) & 0xFF0;
			//	sC = sC | m2NibbleCbTmp;
			//} else if ((cSwapIdx % 2) == 1) {
				sC = (keepCr << 2) & 0xFF0;
				sC = sC | m2NibbleCrTmp;
			//}

			sY = sY | m2NibbleYTmp;
			cSwapIdx++;
#else
			sY = sC = 0;
			sY = (((mY&0x3FC)>>2)<<4) | ((mCb&0x3C)>>2);
			sC = ((mCb&0x3C0)>>6) | (((mCr&0x3FC)>>2)<<4);

			//sY = (((mY&0x3FC)>>2)<<4) | ((mCr&0x3C)>>2);
			//sC = ((mCr&0x3C0)>>6) | (((mCb&0x3FC)>>2)<<4);

			//sY = (mY&0x3FC)<<4 | (mCb&0x3C0)>>6;
			//sC = (mCb&0x3C)<<4 | (mCr&0x3FC)<<2;

			//sY = (mY&0xFF)<<4 | (mCb&0xf);
			//sC = (mCb&0xf0)<<4 | (mCr&0xff);
#endif
			//printk("%s, sCtmpWOffset=%d, sCtmpROffset=%d  \n", __FUNCTION__, sCtmpWOffset, sCtmpROffset);

			if (testcnt < 8 && idy == 0) {
				//printk("overlap check: Main Y=%x, Cr=%x, Cb=%x, Sub Y=%x, Cr=%x, Cb=%x \n",
				//	(mY&0x3), (mCr&0x3), (mCb&0x3), ((m2NibbleYTmp&0xc)>>2), ((m2NibbleCrTmp&0xc)>>2), ((m2NibbleCbTmp&0xc)>>2));
				printk("%s, mY=0x%x, mCb=0x%x, mCr=0x%x \n", __FUNCTION__, mY, mCb, mCr);
				printk("%s, m2NibbleYTmp=0x%x, m2NibbleCbTmp=0x%x, m2NibbleCrTmp=0x%x \n", __FUNCTION__, m2NibbleYTmp, m2NibbleCbTmp, m2NibbleCrTmp);
				printk("%s, sY=0x%x, sC=0x%x \n", __FUNCTION__, sY, sC);
				testcnt++;
			}

			// UYVY output
			file_write((unsigned char *)&sC, 2, fileoffset, gFrame1Outfp);
			fileoffset += 2;
			file_write((unsigned char *)&sY, 2, fileoffset, gFrame1Outfp);
			fileoffset += 2;

		}

		if ((idy % 50) == 0)
			msleep(50);	// msleep to avoid MD parser stopping
	}

	//kfree(sCtmp);
	kfree(bitarray);
	file_sync(gFrame1Outfp);
	file_close(gFrame1Outfp);
	gFrame1Outfp = NULL;
	file_sync(SrcOutfp);
	file_close(SrcOutfp);
	SrcOutfp = NULL;

}


void Fmt_ComposerCRF_Mcap_to_DM(unsigned int mcapAddr, DOLBYVISION_PATTERN *dolby)
{
	unsigned short mY, mCb, mCr;
	unsigned char mTmp;
	unsigned long idx, idy, idx_byte, bit_idx, fileoffset = 0/*, outfileOffset = 0*/, fileCroffset = 0, fileCboffset = 0, cSwapIdx = 0;
	unsigned char *addrptr = (unsigned char *)mcapAddr;
	unsigned short sY, sC;
	char tmpStr[500];
	char numStr[8];
	unsigned int lineBitCnt, testcnt = 0;
	unsigned char *bitarray = NULL; // store the valid data only
	static struct file *SrcOutfp = NULL;


	printk("%s, Width=%d, Height=%d \n", __FUNCTION__, dolby->ColNumTtl, dolby->RowNumTtl);
	flush_cache_all();

	if (gFrame1Outfp != NULL) {
		file_sync(gFrame1Outfp);
		file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}
	if (SrcOutfp != NULL) {
		file_sync(SrcOutfp);
		file_close(SrcOutfp);
		SrcOutfp = NULL;
	}

	strcpy(tmpStr, dolby->fileOutpath);
	tmpStr[strlen(tmpStr)-3] = '\0';
	snprintf(numStr, 8, "%03d.yuv", g_frameCtrlNo);
	strcat(tmpStr, numStr);

	if (gFrame1Outfp == NULL)
		gFrame1Outfp = file_open(tmpStr, O_TRUNC | O_WRONLY | O_CREAT, 0);
	if (gFrame1Outfp == NULL) {
		printk("%s, gFrame1Outfp file open failed \n", __FUNCTION__);
		gFrame1Outfp = NULL;
		return ;
	}
	if (SrcOutfp == NULL)
		SrcOutfp = file_open("/var/mcap_source.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
	if (SrcOutfp == NULL) {
		printk("%s, SrcOutfp file open failed \n", __FUNCTION__);
		SrcOutfp = NULL;
		return ;
	}

#ifdef _SOFT_CAL_
	if ((dolby->ColNumTtl* 30) % 64)
		lineBitCnt = ((dolby->ColNumTtl* 30) / 64) + 4;
	else
		lineBitCnt = (dolby->ColNumTtl* 30) / 64;

	lineBitCnt *= 64;
#else
	lineBitCnt = MDOMAIN_CAP_DDR_In1WTLVL_Num_get_in1_write_num(rtd_inl(MDOMAIN_CAP_DDR_In1WTLVL_Num_reg));
	lineBitCnt *= MDOMAIN_CAP_DDR_In1WrLen_Rem_get_in1_wrlen(rtd_inl(MDOMAIN_CAP_DDR_In1WrLen_Rem_reg));
	lineBitCnt += MDOMAIN_CAP_DDR_In1WrLen_Rem_get_in1_write_remain(rtd_inl(MDOMAIN_CAP_DDR_In1WrLen_Rem_reg));
	lineBitCnt /= dolby->RowNumTtl;
	//lineBitCnt *= MDOMAIN_CAP_DDR_In1WrLen_Rem_get_in1_wrlen(rtd_inl(MDOMAIN_CAP_DDR_In1WrLen_Rem_reg));
	lineBitCnt *= 64;
#endif

	bitarray = (unsigned char *)kmalloc(lineBitCnt, GFP_KERNEL);
	if (bitarray == NULL) {
		printk("%s, bitarray kmalloc failed  \n", __FUNCTION__);
		return ;
	}
	printk("%s, lineBitCnt=%d, mcapAddr=0x%x \n", __FUNCTION__, lineBitCnt, mcapAddr);

	fileCboffset = (dolby->ColNumTtl * dolby->RowNumTtl * 2);
	fileCroffset = fileCboffset + (dolby->ColNumTtl * dolby->RowNumTtl / 2);
	printk("%s, fileCboffset=0x%x, fileCroffset=0x%x \n", __FUNCTION__, (unsigned int)fileCboffset, (unsigned int)fileCroffset);

	for (idy = 0; idy < dolby->RowNumTtl; idy++) {

		addrptr = (unsigned char *)(mcapAddr + ((lineBitCnt/8) * idy));
		bit_idx = 0;
		memset(bitarray, 0x00, lineBitCnt);

		for (idx = 0; idx < lineBitCnt; idx+=8) {
			mTmp = *addrptr;

			if (testcnt < 10 && idy == 0) {
				printk("%s, mTmp=0x%x, addrptr=0x%x \n", __FUNCTION__, (unsigned int)mTmp, (unsigned int)addrptr);
				testcnt++;
			}

			// M-cap DDR content output
			//file_write(&mTmp, 1, outfileOffset, SrcOutfp);
			//outfileOffset++;

			for (idx_byte = 0; idx_byte < 8; idx_byte++)
				bitarray[bit_idx++] = (mTmp & (1<<idx_byte)) >> idx_byte;
			addrptr++;
		}

		bit_idx = testcnt = 0;

		for (idx = 0; idx < dolby->ColNumTtl; idx++) {

			mCr = bitarray[bit_idx+0] | (bitarray[bit_idx+1]<<1) | (bitarray[bit_idx+2]<<2) |
				  (bitarray[bit_idx+3]<<3) | (bitarray[bit_idx+4]<<4) | (bitarray[bit_idx+5]<<5) |
				  (bitarray[bit_idx+6]<<6) | (bitarray[bit_idx+7]<<7) | (bitarray[bit_idx+8]<<8) |
				  (bitarray[bit_idx+9]<<9);
			mCb = bitarray[bit_idx+10] | (bitarray[bit_idx+11]<<1) | (bitarray[bit_idx+12]<<2) |
				  (bitarray[bit_idx+13]<<3) | (bitarray[bit_idx+14]<<4) | (bitarray[bit_idx+15]<<5) |
				  (bitarray[bit_idx+16]<<6) | (bitarray[bit_idx+17]<<7) | (bitarray[bit_idx+18]<<8) |
				  (bitarray[bit_idx+19]<<9);
			mY = bitarray[bit_idx+20] | (bitarray[bit_idx+21]<<1) | (bitarray[bit_idx+22]<<2) |
				  (bitarray[bit_idx+23]<<3) | (bitarray[bit_idx+24]<<4) | (bitarray[bit_idx+25]<<5) |
				  (bitarray[bit_idx+26]<<6) | (bitarray[bit_idx+27]<<7) | (bitarray[bit_idx+28]<<8) |
				  (bitarray[bit_idx+29]<<9);

			bit_idx += 30;

			// for composer mode
			sY = sC = 0;
			sY = ((mY & 0x3FF) << 4) | ((mCb & 0x3C) >> 2);
			sC = ((mCb & 0x3C0) >> 6) | ((mCr & 0x3FF) << 4);


			if (testcnt < 5 && idy == 0) {
				printk("%s, mY=0x%x, mCb=0x%x, mCr=0x%x \n", __FUNCTION__, mY, mCb, mCr);
				printk("%s, sY=0x%x, sC=0x%x \n", __FUNCTION__, sY, sC);
				testcnt++;
			}

			// P420 14b output
			if (cSwapIdx < (dolby->RowNumTtl * dolby->ColNumTtl / 2) && ((idy % 2)==0)) {
				if ((cSwapIdx % 2) == 0) {
					file_write((unsigned char *)&sC, 2, fileCboffset, gFrame1Outfp);
					fileCboffset += 2;
				} else if ((cSwapIdx % 2) == 1) {
					file_write((unsigned char *)&sC, 2, fileCroffset, gFrame1Outfp);
					fileCroffset += 2;
				}
				cSwapIdx++;
			}
			file_write((unsigned char *)&sY, 2, fileoffset, gFrame1Outfp);
			fileoffset += 2;

		}
	}

	kfree(bitarray);
	file_sync(gFrame1Outfp);
	file_close(gFrame1Outfp);
	gFrame1Outfp = NULL;
	file_sync(SrcOutfp);
	file_close(SrcOutfp);
	SrcOutfp = NULL;

}

#ifdef RTK_IDK_CRT
void DM_B01_03_Check(unsigned char src) {
	dm_eotf_para1_RBUS dm_b0103_eotf_p1_reg;
	dm_b0103_lut_ctrl0_RBUS dm_b0103_lut_ctrl_reg;

	unsigned int timeout = 0xffff;
	unsigned int loopIdx=0;
	unsigned int errorindex=0;
	unsigned int set1=0;
	unsigned int set2=0;
	DmExecFxp_t *pDmExec;
	if(src == DOLBY_SRC_HDMI)
		pDmExec = &hdmi_h_dm->dmExec;
	else
		pDmExec = &OTT_h_dm->dmExec;

	//sEotf
	dm_b0103_eotf_p1_reg.regValue = 0;
	dm_b0103_eotf_p1_reg.seotf =pDmExec->sEotf;
	rtd_outl(DM_EOTF_para1_reg, dm_b0103_eotf_p1_reg.regValue);

	dm_b0103_lut_ctrl_reg.regValue = rtd_inl(DM_B0103_LUT_CTRL0_reg);
	dm_b0103_lut_ctrl_reg.b0103_lut_rw_sel = 2;
	dm_b0103_lut_ctrl_reg.b0103_hw_fw_priority_opt = 0;//fw prior
	rtd_outl(DM_B0103_LUT_CTRL0_reg, dm_b0103_lut_ctrl_reg.regValue);
	dm_b0103_lut_ctrl_reg.regValue = rtd_inl(DM_B0103_LUT_CTRL0_reg);

	for (;loopIdx < (DEF_G2L_LUT_SIZE/2); loopIdx++) {

		rtd_outl(DM_B0103_LUT_CTRL1_reg,DM_B0103_LUT_CTRL1_b0103_read_en(1));
		set1 = pDmExec->g2L[0+loopIdx*2];
		set2 = pDmExec->g2L[1+loopIdx*2];
		if(DM_B0103_LUT_RD_DATA2_b0103_rd_lut_data2(set1) != rtd_inl(DM_B0103_LUT_RD_DATA2_reg)) {
			errorindex=rtd_inl(DM_B0103_LUT_CTRL2_reg);
			break;
		}
		if(DM_B0103_LUT_RD_DATA3_b0103_rd_lut_data3(set2) != rtd_inl(DM_B0103_LUT_RD_DATA3_reg)) {
			errorindex=rtd_inl(DM_B0103_LUT_CTRL2_reg);
			break;
		}

		timeout = 0xffff;
		while ((DM_B0103_LUT_CTRL1_get_b0103_write_en(rtd_inl(DM_B0103_LUT_CTRL1_reg)) != 0) && timeout){
			timeout--;
		}
		if(!timeout)
			printk(KERN_ERR"\r\n DM_B0103_read err Wait (0x%08x) timeout\r\n", DM_B0103_LUT_CTRL1_reg);
	}
	if(DM_B0103_LUT_CTRL2_get_b0103_lut_rw_addr(rtd_inl(DM_B0103_LUT_CTRL2_reg)) !=DEF_G2L_LUT_SIZE/2){
		printk(KERN_ERR"\r\n DM_B0103 read b0103 error, rw_addr=%d\r\n",
			DM_B0103_LUT_CTRL2_get_b0103_lut_rw_addr(rtd_inl(DM_B0103_LUT_CTRL2_reg)));
	}

	dm_b0103_lut_ctrl_reg.regValue = 0;
	rtd_outl(DM_B0103_LUT_CTRL0_reg, dm_b0103_lut_ctrl_reg.regValue);
	if(errorindex !=0)
		printk(KERN_ERR"\r\n DM_B0103 check b0103 error, index = %d\r\n",errorindex);
	//else
		//printk(KERN_ERR"\r\n DM_B0103 check ok\r\n");

}
#endif
void DM_B01_03_Set(unsigned char src)
{
	dm_eotf_para1_RBUS dm_b0103_eotf_p1_reg;
	dm_b0103_lut_ctrl0_RBUS dm_b0103_lut_ctrl_reg;

	unsigned int timeout = 0xffff;
	unsigned int loopIdx=0;
	unsigned int set1=0;
	unsigned int set2=0;
	DmExecFxp_t *pDmExec;
	if(src == DOLBY_SRC_HDMI)
		pDmExec = &hdmi_h_dm->dmExec;
	else
		pDmExec = &OTT_h_dm->dmExec;

	//sEotf
	dm_b0103_eotf_p1_reg.regValue = 0;
	dm_b0103_eotf_p1_reg.seotf =pDmExec->sEotf;
	rtd_outl(DM_EOTF_para1_reg, dm_b0103_eotf_p1_reg.regValue);

		dm_b0103_lut_ctrl_reg.regValue = rtd_inl(DM_B0103_LUT_CTRL0_reg);
		dm_b0103_lut_ctrl_reg.b0103_lut_rw_sel = 1;
		dm_b0103_lut_ctrl_reg.b0103_hw_fw_priority_opt = 0;//fw prior
		rtd_outl(DM_B0103_LUT_CTRL0_reg, dm_b0103_lut_ctrl_reg.regValue);
		dm_b0103_lut_ctrl_reg.regValue = rtd_inl(DM_B0103_LUT_CTRL0_reg);

	for (;loopIdx < (DEF_G2L_LUT_SIZE/2); loopIdx++) {
		set1 = pDmExec->g2L[0+loopIdx*2];
		set2 = pDmExec->g2L[1+loopIdx*2];
		rtd_outl(DM_B0103_LUT_WR_DATA0_reg,DM_B0103_LUT_WR_DATA0_b0103_wr_lut_data0(set1));
		rtd_outl(DM_B0103_LUT_WR_DATA1_reg,DM_B0103_LUT_WR_DATA1_b0103_wr_lut_data1(set2));
		rtd_outl(DM_B0103_LUT_WR_DATA2_reg,DM_B0103_LUT_WR_DATA2_b0103_wr_lut_data2(set1));
		rtd_outl(DM_B0103_LUT_WR_DATA3_reg,DM_B0103_LUT_WR_DATA3_b0103_wr_lut_data3(set2));
		rtd_outl(DM_B0103_LUT_WR_DATA4_reg,DM_B0103_LUT_WR_DATA4_b0103_wr_lut_data4(set1));
		rtd_outl(DM_B0103_LUT_WR_DATA5_reg,DM_B0103_LUT_WR_DATA5_b0103_wr_lut_data5(set2));
		rtd_outl(DM_B0103_LUT_CTRL1_reg,DM_B0103_LUT_CTRL1_b0103_write_en(1));
		timeout = 0xffff;
		while ((DM_B0103_LUT_CTRL1_get_b0103_write_en(rtd_inl(DM_B0103_LUT_CTRL1_reg)) != 0) && timeout){
			timeout--;
		}
		if(!timeout)
			printk(KERN_ERR"\r\n DM_B0103_Set err Wait (0x%08x) timeout\r\n", DM_B0103_LUT_CTRL1_reg);
	}
	if(DM_B0103_LUT_CTRL2_get_b0103_lut_rw_addr(rtd_inl(DM_B0103_LUT_CTRL2_reg)) !=DEF_G2L_LUT_SIZE/2){
		printk(KERN_ERR"\r\n DM_B0103 write b0103 error, rw_addr=%d\r\n",
			DM_B0103_LUT_CTRL2_get_b0103_lut_rw_addr(rtd_inl(DM_B0103_LUT_CTRL2_reg)));
	}

	dm_b0103_lut_ctrl_reg.regValue = 0;
	rtd_outl(DM_B0103_LUT_CTRL0_reg, dm_b0103_lut_ctrl_reg.regValue);

	/*Need HDR EOTF write*/

#ifdef RTK_IDK_CRT
	DM_B01_03_Check(src);
#endif
}

#ifdef RTK_IDK_CRT
void DM_B02_Set_check(unsigned char src)
{
	dm_dm_b02_lut_ctrl0_RBUS dm_b02_lut_ctrl0_reg;
	unsigned int timeout = 0xffff;
	int lutIdx = 0, loopIdx;//, lutIdx_tmp, lutEntryNum;
	DmExecFxp_t *pDmExec;
	unsigned char errorindex = 0;
	unsigned int set0=0, set1=0, set2=0;
	if(src == DOLBY_SRC_HDMI)
		pDmExec = &hdmi_h_dm->dmExec;
	else
		pDmExec = &OTT_h_dm->dmExec;

	lutIdx = 0;
	dm_b02_lut_ctrl0_reg.regValue = rtd_inl(DM_DM_B02_LUT_CTRL0_reg);
	dm_b02_lut_ctrl0_reg.b02_lut_rw_sel = 2;

	rtd_outl(DM_DM_B02_LUT_CTRL0_reg, dm_b02_lut_ctrl0_reg.regValue);
			timeout = 0xffff;
	while ((DM_DM_B02_LUT_STATUS0_get_b02_lut_acc_ready(rtd_inl(DM_DM_B02_LUT_STATUS0_reg)) != 1) && timeout){
				timeout--;
			}
			if(!timeout)
	{
		printk(KERN_ERR"\r\n DM_B02_Set err Wait B02 (0xB806B59C) timeout\r\n");
		}

	/*LUT table[512+3], per loop times 5*/
	for (loopIdx = 0; loopIdx < sizeof(pDmExec->tcLut)/2/5; loopIdx++) {

		rtd_outl(DM_DM_B02_LUT_CTRL1_reg,  DM_DM_B02_LUT_CTRL1_b02_lut_read_en_mask);

		timeout = 0xffff;
		// wait write_en clear
		while ((DM_DM_B02_LUT_CTRL1_get_b02_lut_read_en(rtd_inl(DM_DM_B02_LUT_CTRL1_reg)) != 0) && timeout){
			timeout--;
	}
		if(!timeout)
			printk(KERN_ERR"\r\n DM_B02_Set err Wait B02 (0xB806B588) timeout\r\n");

		//lutIdx_tmp = lutIdx;
		lutIdx = loopIdx * 5;
		set0 = DM_DM_B02_LUT_CTRL2_b02_wr_lut_data0(*(pDmExec->tcLut+lutIdx)) |
			DM_DM_B02_LUT_CTRL2_b02_wr_lut_data1(*(pDmExec->tcLut+(lutIdx+1)));
		set1 = DM_DM_B02_LUT_CTRL3_b02_wr_lut_data2(*(pDmExec->tcLut+(lutIdx+2))) |
			DM_DM_B02_LUT_CTRL3_b02_wr_lut_data3(*(pDmExec->tcLut+(lutIdx+3)));
		set2 = DM_DM_B02_LUT_CTRL4_b02_wr_lut_data4(*(pDmExec->tcLut+(lutIdx+4)));

		if(set0 != rtd_inl(DM_DM_B02_LUT_STATUS1_reg)) {
			errorindex = (unsigned char)rtd_inl(DM_DM_B02_LUT_STATUS0_reg);
			break;
		}
		if(set1 != rtd_inl(DM_DM_B02_LUT_STATUS2_reg)) {
			errorindex = (unsigned char)rtd_inl(DM_DM_B02_LUT_STATUS0_reg);
			break;
		}

		if(set2 != rtd_inl(DM_DM_B02_LUT_STATUS3_reg)) {
			errorindex = (unsigned char)rtd_inl(DM_DM_B02_LUT_STATUS0_reg);
			break;
		}
}
	if(errorindex)
		printk(KERN_ERR"\r\n DM_B02_Set check err index = %d\r\n",(unsigned int)errorindex);
	//else
	//	printk(KERN_ERR"\r\n DM_B02_Set check OK!! \r\n");

	dm_b02_lut_ctrl0_reg.b02_lut_rw_sel = 0;
	rtd_outl(DM_DM_B02_LUT_CTRL0_reg, dm_b02_lut_ctrl0_reg.regValue);
}
#endif


void DM_B02_Set(unsigned char src)
{
#if 0//nned to change dma to LUT
	dm_dm_b02_lut_ctrl0_RBUS dm_b02_lut_ctrl0_reg;
	unsigned int timeout = 0xffff;
	int lutIdx = 0, loopIdx;//, lutIdx_tmp, lutEntryNum;
	DmExecFxp_t *pDmExec;
	unsigned char accessTableFlag = 0;
	if(src == DOLBY_SRC_HDMI)
		pDmExec = &hdmi_h_dm->dmExec;
	else
		pDmExec = &OTT_h_dm->dmExec;

	rtd_outl(DM_DM_B02_LUT_MaxVal_reg, DM_DM_B02_LUT_MaxVal_b02_lutmaxval(pDmExec->tcLutMaxVal));

	lutIdx = 0;
	dm_b02_lut_ctrl0_reg.regValue = rtd_inl(DM_DM_B02_LUT_CTRL0_reg);
	accessTableFlag = dm_b02_lut_ctrl0_reg.b02_lut_acc_sel;
	dm_b02_lut_ctrl0_reg.b02_lut_rw_sel = 1;

	rtd_outl(DM_DM_B02_LUT_CTRL0_reg, dm_b02_lut_ctrl0_reg.regValue);
	timeout = 0xffff;
	while ((DM_DM_B02_LUT_STATUS0_get_b02_lut_acc_ready(rtd_inl(DM_DM_B02_LUT_STATUS0_reg)) != 1) && timeout){
		timeout--;
	}
	if(!timeout)
	{
		printk(KERN_ERR"\r\n DM_B02_Set err Wait B02 (0xB806B59C) timeout\r\n");
	}

	/*LUT table[512+3], per loop times 5*/
	for (loopIdx = 0; loopIdx < sizeof(pDmExec->tcLut)/2/5; loopIdx++) {

		//lutIdx_tmp = lutIdx;
		lutIdx = loopIdx * 5;

		rtd_outl(DM_DM_B02_LUT_CTRL2_reg, DM_DM_B02_LUT_CTRL2_b02_wr_lut_data0(*(pDmExec->tcLut+lutIdx)) |
			DM_DM_B02_LUT_CTRL2_b02_wr_lut_data1(*(pDmExec->tcLut+(lutIdx+1))));
		rtd_outl(DM_DM_B02_LUT_CTRL3_reg, DM_DM_B02_LUT_CTRL3_b02_wr_lut_data2(*(pDmExec->tcLut+(lutIdx+2))) |
			DM_DM_B02_LUT_CTRL3_b02_wr_lut_data3(*(pDmExec->tcLut+(lutIdx+3))));
		rtd_outl(DM_DM_B02_LUT_CTRL4_reg, DM_DM_B02_LUT_CTRL4_b02_wr_lut_data4(*(pDmExec->tcLut+(lutIdx+4))));

		// write_en
		rtd_outl(DM_DM_B02_LUT_CTRL1_reg,  DM_DM_B02_LUT_CTRL1_b02_lut_write_en_mask);
		timeout = 0xffff;
		// wait write_en clear
		while ((DM_DM_B02_LUT_CTRL1_get_b02_lut_write_en(rtd_inl(DM_DM_B02_LUT_CTRL1_reg)) != 0) && timeout){
			timeout--;
			//msleep(1);
		}
		if(!timeout)
		{
			printk(KERN_ERR"\r\n DM_B02_Set err Wait B02 (0xB806B588) timeout\r\n");
		}
	}



	//before write lut_acc_sel, read check
#ifdef RTK_IDK_CRT
	DM_B02_Set_check(src);
#endif

	// disable write table
	dm_b02_lut_ctrl0_reg.regValue = 0;
	//dm_b02_lut_ctrl0_reg.lut_enable = 1;
	dm_b02_lut_ctrl0_reg.b02_lut_acc_sel = accessTableFlag ? 0: 1;
	rtd_outl(DM_DM_B02_LUT_CTRL0_reg, dm_b02_lut_ctrl0_reg.regValue);
#endif
}

#ifdef RTK_IDK_CRT
unsigned int DM_B05_Check(uint16_t *lutMap)
{
	unsigned int timeout = 0xffff;
	dm_dm_b05_lut_ctrl0_RBUS dm_b05_lut_ctrl0_reg;
	int lutIdx = 0, lutIdx_tmp, lutEntryNum;
	int sram_addr_h, sram_addr_l;
	unsigned short *lut;
	unsigned short *lutS1;
	unsigned short *lutS2;
	unsigned short *lutS3;
	//unsigned short *lutS1End;
	unsigned int regValue, compareValue;

	// 3 17x17x17 LUTs
	lutEntryNum = 17 * 17 * 17;
	lutS1 = lut = lutMap;//pDmExec->bwDmLut.lutMap;
	lutS2 = lutS1 + lutEntryNum;
	lutS3 = lutS2 + lutEntryNum;
	lutIdx = 0;


	if (1) {
		for (sram_addr_h = 0; sram_addr_h < 17; sram_addr_h++) {
			for (sram_addr_l = 0; sram_addr_l < 17; sram_addr_l++) {

				lutIdx = ((17 * sram_addr_h) + sram_addr_l) * 51;

				// read_en
				dm_b05_lut_ctrl0_reg.regValue = 0;
				dm_b05_lut_ctrl0_reg.lut_b05_rw_adr = ((sram_addr_h<<5) | sram_addr_l);
				dm_b05_lut_ctrl0_reg.lut_b05_read_en = 1;		// read enable
				dm_b05_lut_ctrl0_reg.lut_b05_rw_sel = 2;		// read table
				rtd_outl(DM_DM_B05_LUT_CTRL0_reg, dm_b05_lut_ctrl0_reg.regValue);
				// wait write_en clear
				timeout = 0xffff;
				while ((DM_DM_B05_LUT_CTRL0_get_lut_b05_read_en(rtd_inl(DM_DM_B05_LUT_CTRL0_reg)) != 0) && timeout){
					//msleep(1);
					timeout--;
				}
				if(!timeout)
					printk(KERN_ERR"\r\n DM_B05_Check err Wait B05 (0xB806B5FC) timeout\r\n");
				lutIdx_tmp = 0;

				// read data
				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_1_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_1_get_lut_rd_data_d0_1(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_1_get_lut_rd_data_d1_1(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_2_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_2_get_lut_rd_data_d2_1(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_3_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_3_get_lut_rd_data_d0_2(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_3_get_lut_rd_data_d1_2(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_4_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_4_get_lut_rd_data_d2_2(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_5_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_5_get_lut_rd_data_d0_3(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_5_get_lut_rd_data_d1_3(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_6_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_6_get_lut_rd_data_d2_3(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_7_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_7_get_lut_rd_data_d0_4(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_7_get_lut_rd_data_d1_4(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_8_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_8_get_lut_rd_data_d2_4(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_9_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_9_get_lut_rd_data_d0_5(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_9_get_lut_rd_data_d1_5(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_10_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_10_get_lut_rd_data_d2_5(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_11_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_11_get_lut_rd_data_d0_6(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_11_get_lut_rd_data_d1_6(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_12_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_12_get_lut_rd_data_d2_6(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_13_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_13_get_lut_rd_data_d0_7(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_13_get_lut_rd_data_d1_7(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_14_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_14_get_lut_rd_data_d2_7(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_15_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_15_get_lut_rd_data_d0_8(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_15_get_lut_rd_data_d1_8(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_16_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_16_get_lut_rd_data_d2_8(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_17_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_17_get_lut_rd_data_d0_9(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_17_get_lut_rd_data_d1_9(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_18_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_18_get_lut_rd_data_d2_9(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_19_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_19_get_lut_rd_data_d0_10(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_19_get_lut_rd_data_d1_10(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_20_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_20_get_lut_rd_data_d2_10(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_21_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_21_get_lut_rd_data_d0_11(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_21_get_lut_rd_data_d1_11(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_22_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_22_get_lut_rd_data_d2_11(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_23_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_23_get_lut_rd_data_d0_12(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_23_get_lut_rd_data_d1_12(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_24_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_24_get_lut_rd_data_d2_12(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_25_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_25_get_lut_rd_data_d0_13(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_25_get_lut_rd_data_d1_13(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_26_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_26_get_lut_rd_data_d2_13(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_27_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_27_get_lut_rd_data_d0_14(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_27_get_lut_rd_data_d1_14(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_28_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_28_get_lut_rd_data_d2_14(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_29_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_29_get_lut_rd_data_d0_15(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_29_get_lut_rd_data_d1_15(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_30_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_30_get_lut_rd_data_d2_15(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_31_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_31_get_lut_rd_data_d0_16(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_31_get_lut_rd_data_d1_16(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_32_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_32_get_lut_rd_data_d2_16(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_33_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_33_get_lut_rd_data_d0_17(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;
				compareValue = DM_DM_B05_LUT_RD_DATA_33_get_lut_rd_data_d1_17(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

				regValue = rtd_inl(DM_DM_B05_LUT_RD_DATA_34_reg);
				compareValue = DM_DM_B05_LUT_RD_DATA_34_get_lut_rd_data_d2_17(regValue);
				if (lut[lutIdx+lutIdx_tmp++] != compareValue)
					goto _B05_W_FAIL_;

			}
		}

		dm_b05_lut_ctrl0_reg.regValue = 0;
		rtd_outl(DM_DM_B05_LUT_CTRL0_reg, dm_b05_lut_ctrl0_reg.regValue);
	}

	//printk(KERN_ERR"%s, B05 check ok!!!\n", __FUNCTION__);

	return 0;

_B05_W_FAIL_:

	printk(KERN_ERR"%s, wrong data in B05, write failed.... \n", __FUNCTION__);
	printk(KERN_ERR"source lutIdx=%d, lutIdx_tmp=%d, lut[%d]=0x%x, read-back=0x%x \n", lutIdx, lutIdx_tmp, lutIdx+(lutIdx_tmp-1), lut[lutIdx+(lutIdx_tmp-1)], compareValue);
	dm_b05_lut_ctrl0_reg.regValue = 0;
	rtd_outl(DM_DM_B05_LUT_CTRL0_reg, dm_b05_lut_ctrl0_reg.regValue);
	return 1;
}
#endif
#if 0
#include <rbus/dmato3dtable_reg.h>
static uint16_t* b05table = NULL;
static unsigned int b05tablephy = 0;

typedef struct _yuvdata_idk
{
	unsigned short y;
	unsigned short u;
	unsigned short v;
} yuvdata_idk;
yuvdata_idk table_idk[GMLUT_MAX_DIM*GMLUT_MAX_DIM*GMLUT_MAX_DIM +1];
yuvdata_idk DDR_TABLE_idk[GMLUT_MAX_DIM*GMLUT_MAX_DIM*GMLUT_MAX_DIM +1];
//#define 3DLUT_BUFFER_SIZE 3*GMLUT_MAX_DIM*GMLUT_MAX_DIM*GMLUT_MAX_DIM
/*fast B05 from flora, porting by baker*/
void DM_B05_Set(uint16_t *lutMap, uint32_t forceUpdate)
{
	dmato3dtable_dmato3dtable_db_ctl_RBUS dmato3dtable_dmato3dtable_db_ctl_reg;
	dmato3dtable_dmato3dtable_table0_format0_RBUS dmato3dtable_dmato3dtable_table0_format0_reg;
	dmato3dtable_dmato3dtable_table0_format1_RBUS dmato3dtable_dmato3dtable_table0_format1_reg;
	dmato3dtable_dmato3dtable_table0_addr_RBUS dmato3dtable_dmato3dtable_table0_addr_reg;
	dmato3dtable_dmato3dtable_table0_burst_RBUS dmato3dtable_dmato3dtable_table0_burst_reg;
	dmato3dtable_dmato3dtable_table0_dma_RBUS dmato3dtable_dmato3dtable_table0_dma_reg;
	dm_dmato3dtable_ctrl_RBUS dm_dmato3dtable_ctrl_reg;
	dmato3dtable_dmato3dtable_table0_status_RBUS dmato3dtable_dmato3dtable_table0_status_reg;

	dm_dm_submodule_enable_RBUS dm_submodule_enable_REG;
	dm_hdr_double_buffer_ctrl_RBUS dm_double_buffer_ctrl_reg;
	dm_dm_b05_lut_ctrl2_RBUS dm_dm_b05_lut_ctrl2_Reg;
	//dm_dm_b05_lut_ctrl1_RBUS dm_dm_b05_lut_ctrl1_Reg;

	UINT32 timeoutcnt = 0x062500;
	UINT32 k = 0;

	UINT32 pitch=0,slice=0;

	unsigned int lut_idx=0;
	int x,y,z;


	if(b05table == NULL) {
		b05table = kmalloc(sizeof(OTT_h_dm->dmExec.bwDmLut.lutMap), GFP_KERNEL);
		if(b05table == 0) {
			printk(KERN_ERR"%s kmalloc fail!!\n",__func__);
			return;
		}
		b05tablephy = virt_to_phys(b05table);
		printk(KERN_ERR"%s, get %p, phy=%08x\n",__func__, b05table, b05tablephy);
	}
	dmato3dtable_dmato3dtable_table0_format0_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_Table0_Format0_reg);
	dmato3dtable_dmato3dtable_table0_format0_reg.table0_dma_en = 1;
	dmato3dtable_dmato3dtable_table0_format0_reg.table0_num_y = 0x11;
	dmato3dtable_dmato3dtable_table0_format0_reg.table0_num_x= 0x11;
	rtd_outl(DMATO3DTABLE_DMAto3DTable_Table0_Format0_reg,dmato3dtable_dmato3dtable_table0_format0_reg.regValue);


	dmato3dtable_dmato3dtable_table0_format1_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_Table0_Format1_reg);
	dmato3dtable_dmato3dtable_table0_format1_reg.table0_num_z = 0x11;
	rtd_outl(DMATO3DTABLE_DMAto3DTable_Table0_Format1_reg,dmato3dtable_dmato3dtable_table0_format1_reg.regValue);

	dmato3dtable_dmato3dtable_table0_burst_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_Table0_Burst_reg);
	dmato3dtable_dmato3dtable_table0_burst_reg.table0_burst_num = 0xf3;
	rtd_outl(DMATO3DTABLE_DMAto3DTable_Table0_Burst_reg,dmato3dtable_dmato3dtable_table0_burst_reg.regValue);

	dmato3dtable_dmato3dtable_table0_dma_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_Table0_DMA_reg);
	dmato3dtable_dmato3dtable_table0_dma_reg.table0_remain =0;
	rtd_outl(DMATO3DTABLE_DMAto3DTable_Table0_DMA_reg,dmato3dtable_dmato3dtable_table0_dma_reg.regValue);

	dm_dmato3dtable_ctrl_reg.regValue = rtd_inl(DM_DMAto3Dtable_CTRL_reg);
	dm_dmato3dtable_ctrl_reg.b05_dmato3dtable_en=1;
	rtd_outl(DM_DMAto3Dtable_CTRL_reg,dm_dmato3dtable_ctrl_reg.regValue);


	dmato3dtable_dmato3dtable_db_ctl_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_db_ctl_reg);
	dmato3dtable_dmato3dtable_db_ctl_reg.db_en=1;
	rtd_outl(DMATO3DTABLE_DMAto3DTable_db_ctl_reg,dmato3dtable_dmato3dtable_db_ctl_reg.regValue);

	dmato3dtable_dmato3dtable_table0_addr_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_Table0_Addr_reg);
	dmato3dtable_dmato3dtable_table0_addr_reg.regValue = b05tablephy;

	rtd_outl(DMATO3DTABLE_DMAto3DTable_Table0_Addr_reg, dmato3dtable_dmato3dtable_table0_addr_reg.regValue);

	dm_double_buffer_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
	dm_double_buffer_ctrl_reg.dm_db_en = 1;
	rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buffer_ctrl_reg.regValue);

	dm_submodule_enable_REG.regValue = rtd_inl(DM_dm_submodule_enable_reg);
	if (forceUpdate <= 1) {
		dm_submodule_enable_REG.b0501_enable = 1;
	}else {
		dm_submodule_enable_REG.b0501_enable = 0;
	}

	rtd_outl(DM_dm_submodule_enable_reg, dm_submodule_enable_REG.regValue);
	dm_double_buffer_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
	dm_double_buffer_ctrl_reg.dm_db_apply = 1;
	rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buffer_ctrl_reg.regValue);
	/* wait for db apply*/
	timeoutcnt = 0x062500;
	do {
		dm_double_buffer_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
		timeoutcnt--;
	} while(dm_double_buffer_ctrl_reg.dm_db_apply == 1 && timeoutcnt > 10);

	if (timeoutcnt <= 10) {
		printk(KERN_ERR"%s, DM DB timeout!!\n",__func__);
	        return;
	}
	dm_dm_b05_lut_ctrl2_Reg.regValue = rtd_inl(DM_DM_B05_LUT_CTRL2_reg);
	dm_dm_b05_lut_ctrl2_Reg.dm_b05_dimension = 0;
	/*dm_dm_b05_lut_ctrl2_Reg.lut24_b05_acc_sel = tbl_sel;*/	    /*tbl sel control by fw flow*/
	rtd_outl(DM_DM_B05_LUT_CTRL2_reg, dm_dm_b05_lut_ctrl2_Reg.regValue);

	pitch = GMLUT_MAX_DIM;
	slice = GMLUT_MAX_DIM * GMLUT_MAX_DIM;

	for(k=0;k<GMLUT_MAX_DIM*GMLUT_MAX_DIM*GMLUT_MAX_DIM;k++)
	{
		table_idk[k].y=*(lutMap+(k*3));
		table_idk[k].u=*(lutMap+(k*3)+1);
		table_idk[k].v=*(lutMap+(k*3)+2);
	}

	//for 3d DMA//////////////////////////////////////////////////////////////////////////////////////


	for( z=0;z<(GMLUT_MAX_DIM+1)/2;z++){
		for( y=0;y<(GMLUT_MAX_DIM+1)/2;y++){
			for( x=0;x<(GMLUT_MAX_DIM+1)/2;x++) {
				DDR_TABLE_idk[lut_idx++] = table_idk[2*x + 2*y*pitch + 2*z*slice];
				if(x<(GMLUT_MAX_DIM/2))
					DDR_TABLE_idk[lut_idx++] = table_idk[ (2*x+1) + 2*y*pitch + 2*z*slice];
				if(y<(GMLUT_MAX_DIM/2)) {
					DDR_TABLE_idk[lut_idx++] = table_idk[2*x + (2*y+1)*pitch + 2*z*slice];
					if(x<(GMLUT_MAX_DIM/2))
						DDR_TABLE_idk[lut_idx++] = table_idk[(2*x+1) + (2*y+1)*pitch + 2*z*slice];
				}

				if(z<(GMLUT_MAX_DIM/2)) {
					DDR_TABLE_idk[lut_idx++] = table_idk[2*x + 2*y*pitch + (2*z+1)*slice];
					if(x<(GMLUT_MAX_DIM/2))
						DDR_TABLE_idk[lut_idx++] = table_idk[ (2*x+1) + 2*y*pitch + (2*z+1)*slice];
					if(y<(GMLUT_MAX_DIM/2)) {
						DDR_TABLE_idk[lut_idx++] = table_idk[2*x + (2*y+1)*pitch + (2*z+1)*slice];
						if(x<(GMLUT_MAX_DIM/2))
							DDR_TABLE_idk[lut_idx++] = (table_idk[(2*x+1) + (2*y+1)*pitch + (2*z+1)*slice]);
					}
				}
			}
		}
	}

	for(lut_idx=0; lut_idx<(GMLUT_MAX_DIM*GMLUT_MAX_DIM*GMLUT_MAX_DIM);lut_idx+=2) {
		*b05table=DDR_TABLE_idk[lut_idx].y>>4;
		b05table++;
		*b05table=((DDR_TABLE_idk[lut_idx].y&0xf)<<4)+(DDR_TABLE_idk[lut_idx].u>>8);
		b05table++;
		*b05table=DDR_TABLE_idk[lut_idx].u & 0xff;
		b05table++;
		*b05table=DDR_TABLE_idk[lut_idx].v>>4;
		b05table++;
		*b05table=((DDR_TABLE_idk[lut_idx].v&0xf)<<4)+(DDR_TABLE_idk[lut_idx+1].y>>8);
		b05table++;
		*b05table=DDR_TABLE_idk[lut_idx+1].y & 0xff;
		b05table++;
		*b05table=DDR_TABLE_idk[lut_idx+1].u>>4;
		b05table++;
		*b05table=((DDR_TABLE_idk[lut_idx+1].u&0xf)<<4)+(DDR_TABLE_idk[lut_idx+1].v>>8);
		b05table++;
		*b05table=DDR_TABLE_idk[lut_idx+1].v & 0xff;
		b05table++;
	}

	dmato3dtable_dmato3dtable_db_ctl_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_db_ctl_reg);
	dmato3dtable_dmato3dtable_db_ctl_reg.wtable_apply=1;
	rtd_outl(DMATO3DTABLE_DMAto3DTable_db_ctl_reg,dmato3dtable_dmato3dtable_db_ctl_reg.regValue);

	dmato3dtable_dmato3dtable_table0_status_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_Table2_Status_reg);
	dmato3dtable_dmato3dtable_table0_status_reg.table0_wdone =1;

	rtd_outl(DMATO3DTABLE_DMAto3DTable_Table0_Status_reg,dmato3dtable_dmato3dtable_table0_status_reg.regValue);

	dmato3dtable_dmato3dtable_db_ctl_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_db_ctl_reg);
	dmato3dtable_dmato3dtable_db_ctl_reg.wtable_apply=1;
	rtd_outl(DMATO3DTABLE_DMAto3DTable_db_ctl_reg,dmato3dtable_dmato3dtable_db_ctl_reg.regValue);


}


#else

void DM_B05_DMA_Set(uint16_t *lutMap)
{
	dmato3dtable_dmato3dtable_db_ctl_RBUS dmato3dtable_dmato3dtable_db_ctl_reg;
	dmato3dtable_dmato3dtable_table0_format0_RBUS dmato3dtable_dmato3dtable_table0_format0_reg;
	dmato3dtable_dmato3dtable_table0_format1_RBUS dmato3dtable_dmato3dtable_table0_format1_reg;
	//dmato3dtable_dmato3dtable_table0_addr_RBUS dmato3dtable_dmato3dtable_table0_addr_reg;
	dmato3dtable_dmato3dtable_table0_burst_RBUS dmato3dtable_dmato3dtable_table0_burst_reg;
	dmato3dtable_dmato3dtable_table0_dma_RBUS dmato3dtable_dmato3dtable_table0_dma_reg;
	dmato3dtable_dmato3dtable_table0_status_RBUS dmato3dtable_dmato3dtable_table0_status_reg;
	dm_dm_submodule_enable_RBUS dm_submodule_enable_REG;
	dm_hdr_double_buffer_ctrl_RBUS dm_double_buffer_ctrl_reg;
	dm_dmato3dtable_ctrl_RBUS dm_dmato3dtable_ctrl_reg;

	unsigned char *vir_addr_8=NULL;
	UINT32 pitch=0,slice=0;
	int lut_idx=0;
	int x,y,z,k;

	dm_double_buffer_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
	dm_double_buffer_ctrl_reg.dm_db_en = 1;
	rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buffer_ctrl_reg.regValue);

	dmato3dtable_dmato3dtable_table0_format0_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_Table0_Format0_reg);
	dmato3dtable_dmato3dtable_table0_format0_reg.table0_dma_en = 1;
	dmato3dtable_dmato3dtable_table0_format0_reg.table0_num_y = 0x11;
	dmato3dtable_dmato3dtable_table0_format0_reg.table0_num_x= 0x11;
	rtd_outl(DMATO3DTABLE_DMAto3DTable_Table0_Format0_reg,dmato3dtable_dmato3dtable_table0_format0_reg.regValue);

	dmato3dtable_dmato3dtable_table0_format1_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_Table0_Format1_reg);
	dmato3dtable_dmato3dtable_table0_format1_reg.table0_num_z = 0x11;
	rtd_outl(DMATO3DTABLE_DMAto3DTable_Table0_Format1_reg,dmato3dtable_dmato3dtable_table0_format1_reg.regValue);

	dmato3dtable_dmato3dtable_table0_burst_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_Table0_Burst_reg);
	dmato3dtable_dmato3dtable_table0_burst_reg.table0_burst_num = 0x73/*0xf3*/;
	rtd_outl(DMATO3DTABLE_DMAto3DTable_Table0_Burst_reg,dmato3dtable_dmato3dtable_table0_burst_reg.regValue);

	dmato3dtable_dmato3dtable_table0_dma_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_Table0_DMA_reg);
	dmato3dtable_dmato3dtable_table0_dma_reg.table0_remain =2;
	rtd_outl(DMATO3DTABLE_DMAto3DTable_Table0_DMA_reg,dmato3dtable_dmato3dtable_table0_dma_reg.regValue);

	dm_dmato3dtable_ctrl_reg.regValue = rtd_inl(DM_DMAto3Dtable_CTRL_reg);
	dm_dmato3dtable_ctrl_reg.b05_dmato3dtable_en=1;
	rtd_outl(DM_DMAto3Dtable_CTRL_reg,dm_dmato3dtable_ctrl_reg.regValue);

	dmato3dtable_dmato3dtable_db_ctl_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_db_ctl_reg);
	dmato3dtable_dmato3dtable_db_ctl_reg.db_en=1;
	rtd_outl(DMATO3DTABLE_DMAto3DTable_db_ctl_reg,dmato3dtable_dmato3dtable_db_ctl_reg.regValue);

	for(k=0;k<17*17*17;k++)
	{
		b05_table[k].y=*(lutMap+(k*3));
		b05_table[k].u=*(lutMap+(k*3)+1);
		b05_table[k].v=*(lutMap+(k*3)+2);
	}

	//for 3d DMA//////////////////////////////////////////////////////////////////////////////////////

	pitch = dimC3;
	slice = dimC2 * dimC3;

	for( z=0;z<(dimC3+1)/2;z++){
	   for( y=0;y<(dimC2+1)/2;y++){
		     for( x=0;x<(dimC1+1)/2;x++){
			      B05_DDR_TABLE[lut_idx++] = b05_table[2*x + 2*y*pitch + 2*z*slice];
			      if(x<(dimC1/2))
					B05_DDR_TABLE[lut_idx++] = b05_table[ (2*x+1) + 2*y*pitch + 2*z*slice];
			      if(y<(dimC2/2))
			      {
					B05_DDR_TABLE[lut_idx++] = b05_table[2*x + (2*y+1)*pitch + 2*z*slice];
					if(x<(dimC1/2))
						  B05_DDR_TABLE[lut_idx++] = b05_table[(2*x+1) + (2*y+1)*pitch + 2*z*slice];
			      }
			      if(z<(dimC3/2))
			      {
					B05_DDR_TABLE[lut_idx++] = b05_table[2*x + 2*y*pitch + (2*z+1)*slice];
					if(x<(dimC1/2))
						  B05_DDR_TABLE[lut_idx++] = b05_table[ (2*x+1) + 2*y*pitch + (2*z+1)*slice];
					if(y<(dimC2/2))
					{
						  B05_DDR_TABLE[lut_idx++] = b05_table[2*x + (2*y+1)*pitch + (2*z+1)*slice];
						  if(x<(dimC1/2))
							   B05_DDR_TABLE[lut_idx++] = (b05_table[(2*x+1) + (2*y+1)*pitch + (2*z+1)*slice]);
					}
				}
			}
		}
	}

	vir_addr_8 = (unsigned char*)b05_aligned_vir_addr;

	for(lut_idx=0; lut_idx<(dimC1*dimC2*dimC3)-1;lut_idx+=2)
	{
		*vir_addr_8=B05_DDR_TABLE[lut_idx].y>>4;
		vir_addr_8++;
		*vir_addr_8=((B05_DDR_TABLE[lut_idx].y&0xf)<<4)+(B05_DDR_TABLE[lut_idx].u>>8);
		vir_addr_8++;
		*vir_addr_8=B05_DDR_TABLE[lut_idx].u & 0xff;
		vir_addr_8++;
		*vir_addr_8=B05_DDR_TABLE[lut_idx].v>>4;
		vir_addr_8++;
		*vir_addr_8=((B05_DDR_TABLE[lut_idx].v&0xf)<<4)+(B05_DDR_TABLE[lut_idx+1].y>>8);
		vir_addr_8++;
		*vir_addr_8=B05_DDR_TABLE[lut_idx+1].y & 0xff;
		vir_addr_8++;
		*vir_addr_8=B05_DDR_TABLE[lut_idx+1].u>>4;
		vir_addr_8++;
		*vir_addr_8=((B05_DDR_TABLE[lut_idx+1].u&0xf)<<4)+(B05_DDR_TABLE[lut_idx+1].v>>8);
		vir_addr_8++;
		*vir_addr_8=B05_DDR_TABLE[lut_idx+1].v & 0xff;
		vir_addr_8++;
	}
	//final data for dim=17*17*17
	*vir_addr_8=B05_DDR_TABLE[lut_idx].y>>4;
	vir_addr_8++;
	*vir_addr_8=((B05_DDR_TABLE[lut_idx].y&0xf)<<4)+(B05_DDR_TABLE[lut_idx].u>>8);
	vir_addr_8++;
	*vir_addr_8=B05_DDR_TABLE[lut_idx].u & 0xff;
	vir_addr_8++;
	*vir_addr_8=B05_DDR_TABLE[lut_idx].v>>4;
	vir_addr_8++;
	*vir_addr_8=((B05_DDR_TABLE[lut_idx].v&0xf)<<4);

	dmato3dtable_dmato3dtable_db_ctl_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_db_ctl_reg);
	dmato3dtable_dmato3dtable_db_ctl_reg.wtable_apply=1;
	rtd_outl(DMATO3DTABLE_DMAto3DTable_db_ctl_reg,dmato3dtable_dmato3dtable_db_ctl_reg.regValue);

	dmato3dtable_dmato3dtable_table0_status_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_Table0_Status_reg);
	dmato3dtable_dmato3dtable_table0_status_reg.table0_wdone =1;
	rtd_outl(DMATO3DTABLE_DMAto3DTable_Table0_Status_reg,dmato3dtable_dmato3dtable_table0_status_reg.regValue);

	dmato3dtable_dmato3dtable_db_ctl_reg.regValue = rtd_inl(DMATO3DTABLE_DMAto3DTable_db_ctl_reg);
	dmato3dtable_dmato3dtable_db_ctl_reg.wtable_apply=1;
	rtd_outl(DMATO3DTABLE_DMAto3DTable_db_ctl_reg,dmato3dtable_dmato3dtable_db_ctl_reg.regValue);

	dm_submodule_enable_REG.regValue = rtd_inl(DM_dm_submodule_enable_reg);
	dm_submodule_enable_REG.b0501_enable = 1;
	rtd_outl(DM_dm_submodule_enable_reg, dm_submodule_enable_REG.regValue);

	dm_double_buffer_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
	dm_double_buffer_ctrl_reg.dm_db_apply = 1;
	rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buffer_ctrl_reg.regValue);

//original flow
#if 0
	unsigned int timeout = 0xffff;
	dm_dm_b05_lut_ctrl0_RBUS dm_b05_lut_ctrl0_reg;
	int lutIdx = 0, lutEntryNum;//, lutIdx_tmp, loopIdx;
	int sram_addr_h,sram_addr_l;
	unsigned short *lut;
	unsigned short *lutS1;
	unsigned short *lutS2;
	unsigned short *lutS3;
	//unsigned short *lutS1End;
	static int threeDLutUpdatedFlag = 1;
	//unsigned int ret;

	// 3 17x17x17 LUTs
	lutEntryNum = 17 * 17 * 17;
	lutS1 = lut = lutMap;//pDmExec->bwDmLut.lutMap;
	lutS2 = lutS1 + lutEntryNum;
	lutS3 = lutS2 + lutEntryNum;
	lutIdx = 0;

	//for (sram_addr_h = 0; sram_addr_h < 14739; sram_addr_h++) {
	//	printk("0x%x,\n", lut[sram_addr_h]);
	//}

	if (threeDLutUpdatedFlag || forceUpdate) {
		for (sram_addr_h = 0; sram_addr_h < 17; sram_addr_h++) {
			for (sram_addr_l = 0; sram_addr_l < 17; sram_addr_l++) {
				lutIdx = ((17 * sram_addr_h) + sram_addr_l) * 51;

				//dm_b05_lut_ctrl0_reg.regValue = rtd_inl(DM_DM_B05_LUT_CTRL0_reg);
				//dm_b05_lut_ctrl0_reg.lut_b05_rw_sel = 1;	// write table
				//dm_b05_lut_ctrl0_reg.lut_b05_write_en = 0;
				//dm_b05_lut_ctrl0_reg.lut_b05_read_en = 0;
				//rtd_outl(DM_DM_B05_LUT_CTRL0_reg, dm_b05_lut_ctrl0_reg.regValue);

				// write data
				rtd_outl(DM_DM_B05_LUT_WR_DATA_1_reg, DM_DM_B05_LUT_WR_DATA_1_lut_wr_data_d0_1(lut[lutIdx+0]) |
																DM_DM_B05_LUT_WR_DATA_1_lut_wr_data_d1_1(lut[lutIdx+1]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_2_reg, DM_DM_B05_LUT_WR_DATA_2_lut_wr_data_d2_1(lut[lutIdx+2]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_3_reg, DM_DM_B05_LUT_WR_DATA_3_lut_wr_data_d0_2(lut[lutIdx+3]) |
																DM_DM_B05_LUT_WR_DATA_3_lut_wr_data_d1_2(lut[lutIdx+4]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_4_reg, DM_DM_B05_LUT_WR_DATA_4_lut_wr_data_d2_2(lut[lutIdx+5]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_5_reg, DM_DM_B05_LUT_WR_DATA_5_lut_wr_data_d0_3(lut[lutIdx+6]) |
																DM_DM_B05_LUT_WR_DATA_5_lut_wr_data_d1_3(lut[lutIdx+7]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_6_reg, DM_DM_B05_LUT_WR_DATA_6_lut_wr_data_d2_3(lut[lutIdx+8]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_7_reg, DM_DM_B05_LUT_WR_DATA_7_lut_wr_data_d0_4(lut[lutIdx+9]) |
																DM_DM_B05_LUT_WR_DATA_7_lut_wr_data_d1_4(lut[lutIdx+10]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_8_reg, DM_DM_B05_LUT_WR_DATA_8_lut_wr_data_d2_4(lut[lutIdx+11]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_9_reg, DM_DM_B05_LUT_WR_DATA_9_lut_wr_data_d0_5(lut[lutIdx+12]) |
																DM_DM_B05_LUT_WR_DATA_9_lut_wr_data_d1_5(lut[lutIdx+13]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_10_reg, DM_DM_B05_LUT_WR_DATA_10_lut_wr_data_d2_5(lut[lutIdx+14]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_11_reg, DM_DM_B05_LUT_WR_DATA_11_lut_wr_data_d0_6(lut[lutIdx+15]) |
																DM_DM_B05_LUT_WR_DATA_11_lut_wr_data_d1_6(lut[lutIdx+16]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_12_reg, DM_DM_B05_LUT_WR_DATA_12_lut_wr_data_d2_6(lut[lutIdx+17]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_13_reg, DM_DM_B05_LUT_WR_DATA_13_lut_wr_data_d0_7(lut[lutIdx+18]) |
																DM_DM_B05_LUT_WR_DATA_13_lut_wr_data_d1_7(lut[lutIdx+19]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_14_reg, DM_DM_B05_LUT_WR_DATA_14_lut_wr_data_d2_7(lut[lutIdx+20]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_15_reg, DM_DM_B05_LUT_WR_DATA_15_lut_wr_data_d0_8(lut[lutIdx+21]) |
																DM_DM_B05_LUT_WR_DATA_15_lut_wr_data_d1_8(lut[lutIdx+22]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_16_reg, DM_DM_B05_LUT_WR_DATA_16_lut_wr_data_d2_8(lut[lutIdx+23]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_17_reg, DM_DM_B05_LUT_WR_DATA_17_lut_wr_data_d0_9(lut[lutIdx+24]) |
																DM_DM_B05_LUT_WR_DATA_17_lut_wr_data_d1_9(lut[lutIdx+25]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_18_reg, DM_DM_B05_LUT_WR_DATA_18_lut_wr_data_d2_9(lut[lutIdx+26]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_19_reg, DM_DM_B05_LUT_WR_DATA_19_lut_wr_data_d0_10(lut[lutIdx+27]) |
																DM_DM_B05_LUT_WR_DATA_19_lut_wr_data_d1_10(lut[lutIdx+28]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_20_reg, DM_DM_B05_LUT_WR_DATA_20_lut_wr_data_d2_10(lut[lutIdx+29]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_21_reg, DM_DM_B05_LUT_WR_DATA_21_lut_wr_data_d0_11(lut[lutIdx+30]) |
																DM_DM_B05_LUT_WR_DATA_21_lut_wr_data_d1_11(lut[lutIdx+31]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_22_reg, DM_DM_B05_LUT_WR_DATA_22_lut_wr_data_d2_11(lut[lutIdx+32]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_23_reg, DM_DM_B05_LUT_WR_DATA_23_lut_wr_data_d0_12(lut[lutIdx+33]) |
																DM_DM_B05_LUT_WR_DATA_23_lut_wr_data_d1_12(lut[lutIdx+34]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_24_reg, DM_DM_B05_LUT_WR_DATA_24_lut_wr_data_d2_12(lut[lutIdx+35]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_25_reg, DM_DM_B05_LUT_WR_DATA_25_lut_wr_data_d0_13(lut[lutIdx+36]) |
																DM_DM_B05_LUT_WR_DATA_25_lut_wr_data_d1_13(lut[lutIdx+37]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_26_reg, DM_DM_B05_LUT_WR_DATA_26_lut_wr_data_d2_13(lut[lutIdx+38]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_27_reg, DM_DM_B05_LUT_WR_DATA_27_lut_wr_data_d0_14(lut[lutIdx+39]) |
																DM_DM_B05_LUT_WR_DATA_27_lut_wr_data_d1_14(lut[lutIdx+40]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_28_reg, DM_DM_B05_LUT_WR_DATA_28_lut_wr_data_d2_14(lut[lutIdx+41]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_29_reg, DM_DM_B05_LUT_WR_DATA_29_lut_wr_data_d0_15(lut[lutIdx+42]) |
																DM_DM_B05_LUT_WR_DATA_29_lut_wr_data_d1_15(lut[lutIdx+43]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_30_reg, DM_DM_B05_LUT_WR_DATA_30_lut_wr_data_d2_15(lut[lutIdx+44]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_31_reg, DM_DM_B05_LUT_WR_DATA_31_lut_wr_data_d0_16(lut[lutIdx+45]) |
																DM_DM_B05_LUT_WR_DATA_31_lut_wr_data_d1_16(lut[lutIdx+46]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_32_reg, DM_DM_B05_LUT_WR_DATA_32_lut_wr_data_d2_16(lut[lutIdx+47]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_33_reg, DM_DM_B05_LUT_WR_DATA_33_lut_wr_data_d0_17(lut[lutIdx+48]) |
																DM_DM_B05_LUT_WR_DATA_33_lut_wr_data_d1_17(lut[lutIdx+49]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_34_reg, DM_DM_B05_LUT_WR_DATA_34_lut_wr_data_d2_17(lut[lutIdx+50]));



				// write_en
				dm_b05_lut_ctrl0_reg.regValue = 0;
				dm_b05_lut_ctrl0_reg.lut_b05_rw_adr = ((sram_addr_h<<5) | sram_addr_l);
				dm_b05_lut_ctrl0_reg.lut_b05_write_en = 1;	// write enable
				dm_b05_lut_ctrl0_reg.lut_b05_rw_sel = 1;		// write table
				rtd_outl(DM_DM_B05_LUT_CTRL0_reg, dm_b05_lut_ctrl0_reg.regValue);
				timeout = 0xffff;
				// wait write_en clear
				while ((DM_DM_B05_LUT_CTRL0_get_lut_b05_write_en(rtd_inl(DM_DM_B05_LUT_CTRL0_reg)) != 0) && timeout){
					timeout--;
				}
				if(!timeout)
				{
					pr_err("\r\n DM_B05_Set err Wait B05 (0xB806B5FC) timeout\r\n");
				}
			}
		}

		dm_b05_lut_ctrl0_reg.regValue = 0;
		rtd_outl(DM_DM_B05_LUT_CTRL0_reg, dm_b05_lut_ctrl0_reg.regValue);

		// read back to check data integrity

#ifdef RTK_IDK_CRT
		DM_B05_Check(lutMap);
#endif
		//if (ret)
		//	goto _B05_TRY_;
	}

#if EN_GLOBAL_DIMMING
		threeDLutUpdatedFlag = 1;
#else
		threeDLutUpdatedFlag = g_forceUpdate_3DLUT = 1;
#endif
#endif

}

void DM_B05_Set(uint16_t *lutMap, uint32_t forceUpdate)
{

	unsigned int timeout = 0xffff;
	dm_dm_b05_lut_ctrl0_RBUS dm_b05_lut_ctrl0_reg;
	int lutIdx = 0, lutEntryNum;//, lutIdx_tmp, loopIdx;
	int sram_addr_h,sram_addr_l;
	unsigned short *lut;
	unsigned short *lutS1;
	unsigned short *lutS2;
	unsigned short *lutS3;
	//unsigned short *lutS1End;
	static int threeDLutUpdatedFlag = 1;
	//unsigned int ret;

	// 3 17x17x17 LUTs
	lutEntryNum = 17 * 17 * 17;
	lutS1 = lut = lutMap;//pDmExec->bwDmLut.lutMap;
	lutS2 = lutS1 + lutEntryNum;
	lutS3 = lutS2 + lutEntryNum;
	lutIdx = 0;

	//for (sram_addr_h = 0; sram_addr_h < 14739; sram_addr_h++) {
	//	printk("0x%x,\n", lut[sram_addr_h]);
	//}

	if (threeDLutUpdatedFlag || forceUpdate) {
		for (sram_addr_h = 0; sram_addr_h < 17; sram_addr_h++) {
			for (sram_addr_l = 0; sram_addr_l < 17; sram_addr_l++) {
				lutIdx = ((17 * sram_addr_h) + sram_addr_l) * 51;

				//dm_b05_lut_ctrl0_reg.regValue = rtd_inl(DM_DM_B05_LUT_CTRL0_reg);
				//dm_b05_lut_ctrl0_reg.lut_b05_rw_sel = 1;	// write table
				//dm_b05_lut_ctrl0_reg.lut_b05_write_en = 0;
				//dm_b05_lut_ctrl0_reg.lut_b05_read_en = 0;
				//rtd_outl(DM_DM_B05_LUT_CTRL0_reg, dm_b05_lut_ctrl0_reg.regValue);

				// write data
				rtd_outl(DM_DM_B05_LUT_WR_DATA_1_reg, DM_DM_B05_LUT_WR_DATA_1_lut_wr_data_d0_1(lut[lutIdx+0]) |
																DM_DM_B05_LUT_WR_DATA_1_lut_wr_data_d1_1(lut[lutIdx+1]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_2_reg, DM_DM_B05_LUT_WR_DATA_2_lut_wr_data_d2_1(lut[lutIdx+2]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_3_reg, DM_DM_B05_LUT_WR_DATA_3_lut_wr_data_d0_2(lut[lutIdx+3]) |
																DM_DM_B05_LUT_WR_DATA_3_lut_wr_data_d1_2(lut[lutIdx+4]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_4_reg, DM_DM_B05_LUT_WR_DATA_4_lut_wr_data_d2_2(lut[lutIdx+5]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_5_reg, DM_DM_B05_LUT_WR_DATA_5_lut_wr_data_d0_3(lut[lutIdx+6]) |
																DM_DM_B05_LUT_WR_DATA_5_lut_wr_data_d1_3(lut[lutIdx+7]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_6_reg, DM_DM_B05_LUT_WR_DATA_6_lut_wr_data_d2_3(lut[lutIdx+8]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_7_reg, DM_DM_B05_LUT_WR_DATA_7_lut_wr_data_d0_4(lut[lutIdx+9]) |
																DM_DM_B05_LUT_WR_DATA_7_lut_wr_data_d1_4(lut[lutIdx+10]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_8_reg, DM_DM_B05_LUT_WR_DATA_8_lut_wr_data_d2_4(lut[lutIdx+11]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_9_reg, DM_DM_B05_LUT_WR_DATA_9_lut_wr_data_d0_5(lut[lutIdx+12]) |
																DM_DM_B05_LUT_WR_DATA_9_lut_wr_data_d1_5(lut[lutIdx+13]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_10_reg, DM_DM_B05_LUT_WR_DATA_10_lut_wr_data_d2_5(lut[lutIdx+14]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_11_reg, DM_DM_B05_LUT_WR_DATA_11_lut_wr_data_d0_6(lut[lutIdx+15]) |
																DM_DM_B05_LUT_WR_DATA_11_lut_wr_data_d1_6(lut[lutIdx+16]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_12_reg, DM_DM_B05_LUT_WR_DATA_12_lut_wr_data_d2_6(lut[lutIdx+17]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_13_reg, DM_DM_B05_LUT_WR_DATA_13_lut_wr_data_d0_7(lut[lutIdx+18]) |
																DM_DM_B05_LUT_WR_DATA_13_lut_wr_data_d1_7(lut[lutIdx+19]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_14_reg, DM_DM_B05_LUT_WR_DATA_14_lut_wr_data_d2_7(lut[lutIdx+20]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_15_reg, DM_DM_B05_LUT_WR_DATA_15_lut_wr_data_d0_8(lut[lutIdx+21]) |
																DM_DM_B05_LUT_WR_DATA_15_lut_wr_data_d1_8(lut[lutIdx+22]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_16_reg, DM_DM_B05_LUT_WR_DATA_16_lut_wr_data_d2_8(lut[lutIdx+23]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_17_reg, DM_DM_B05_LUT_WR_DATA_17_lut_wr_data_d0_9(lut[lutIdx+24]) |
																DM_DM_B05_LUT_WR_DATA_17_lut_wr_data_d1_9(lut[lutIdx+25]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_18_reg, DM_DM_B05_LUT_WR_DATA_18_lut_wr_data_d2_9(lut[lutIdx+26]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_19_reg, DM_DM_B05_LUT_WR_DATA_19_lut_wr_data_d0_10(lut[lutIdx+27]) |
																DM_DM_B05_LUT_WR_DATA_19_lut_wr_data_d1_10(lut[lutIdx+28]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_20_reg, DM_DM_B05_LUT_WR_DATA_20_lut_wr_data_d2_10(lut[lutIdx+29]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_21_reg, DM_DM_B05_LUT_WR_DATA_21_lut_wr_data_d0_11(lut[lutIdx+30]) |
																DM_DM_B05_LUT_WR_DATA_21_lut_wr_data_d1_11(lut[lutIdx+31]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_22_reg, DM_DM_B05_LUT_WR_DATA_22_lut_wr_data_d2_11(lut[lutIdx+32]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_23_reg, DM_DM_B05_LUT_WR_DATA_23_lut_wr_data_d0_12(lut[lutIdx+33]) |
																DM_DM_B05_LUT_WR_DATA_23_lut_wr_data_d1_12(lut[lutIdx+34]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_24_reg, DM_DM_B05_LUT_WR_DATA_24_lut_wr_data_d2_12(lut[lutIdx+35]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_25_reg, DM_DM_B05_LUT_WR_DATA_25_lut_wr_data_d0_13(lut[lutIdx+36]) |
																DM_DM_B05_LUT_WR_DATA_25_lut_wr_data_d1_13(lut[lutIdx+37]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_26_reg, DM_DM_B05_LUT_WR_DATA_26_lut_wr_data_d2_13(lut[lutIdx+38]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_27_reg, DM_DM_B05_LUT_WR_DATA_27_lut_wr_data_d0_14(lut[lutIdx+39]) |
																DM_DM_B05_LUT_WR_DATA_27_lut_wr_data_d1_14(lut[lutIdx+40]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_28_reg, DM_DM_B05_LUT_WR_DATA_28_lut_wr_data_d2_14(lut[lutIdx+41]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_29_reg, DM_DM_B05_LUT_WR_DATA_29_lut_wr_data_d0_15(lut[lutIdx+42]) |
																DM_DM_B05_LUT_WR_DATA_29_lut_wr_data_d1_15(lut[lutIdx+43]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_30_reg, DM_DM_B05_LUT_WR_DATA_30_lut_wr_data_d2_15(lut[lutIdx+44]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_31_reg, DM_DM_B05_LUT_WR_DATA_31_lut_wr_data_d0_16(lut[lutIdx+45]) |
																DM_DM_B05_LUT_WR_DATA_31_lut_wr_data_d1_16(lut[lutIdx+46]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_32_reg, DM_DM_B05_LUT_WR_DATA_32_lut_wr_data_d2_16(lut[lutIdx+47]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_33_reg, DM_DM_B05_LUT_WR_DATA_33_lut_wr_data_d0_17(lut[lutIdx+48]) |
																DM_DM_B05_LUT_WR_DATA_33_lut_wr_data_d1_17(lut[lutIdx+49]));
				rtd_outl(DM_DM_B05_LUT_WR_DATA_34_reg, DM_DM_B05_LUT_WR_DATA_34_lut_wr_data_d2_17(lut[lutIdx+50]));



				// write_en
				dm_b05_lut_ctrl0_reg.regValue = 0;
				dm_b05_lut_ctrl0_reg.lut_b05_rw_adr = ((sram_addr_h<<5) | sram_addr_l);
				dm_b05_lut_ctrl0_reg.lut_b05_write_en = 1;	// write enable
				dm_b05_lut_ctrl0_reg.lut_b05_rw_sel = 1;		// write table
				rtd_outl(DM_DM_B05_LUT_CTRL0_reg, dm_b05_lut_ctrl0_reg.regValue);
				timeout = 0xffff;
				// wait write_en clear
				while ((DM_DM_B05_LUT_CTRL0_get_lut_b05_write_en(rtd_inl(DM_DM_B05_LUT_CTRL0_reg)) != 0) && timeout){
					timeout--;
				}
				if(!timeout)
				{
					pr_err("\r\n DM_B05_Set err Wait B05 (0xB806B5FC) timeout\r\n");
				}
			}
		}

		dm_b05_lut_ctrl0_reg.regValue = 0;
		rtd_outl(DM_DM_B05_LUT_CTRL0_reg, dm_b05_lut_ctrl0_reg.regValue);

		// read back to check data integrity

#ifdef RTK_IDK_CRT
		DM_B05_Check(lutMap);
#endif
		//if (ret)
		//	goto _B05_TRY_;
	}

#if EN_GLOBAL_DIMMING
		threeDLutUpdatedFlag = 1;
#else
		threeDLutUpdatedFlag = g_forceUpdate_3DLUT = 1;
#endif
}
#endif

void DMRbusSet(E_DV_MODE mode, unsigned int B02_update)
{
	DmExecFxp_t *pDmExec;
	dm_hdr_double_buffer_ctrl_RBUS dm_double_buf_ctrl_reg;
	//hdr_all_top_top_ctl_RBUS hdr_all_top_top_ctl_reg;
	int offset0 = 0;
	int offset1 = 0;
	int offset2 = 0;
	if(mode == DV_HDMI_MODE)
		pDmExec = &hdmi_h_dm->dmExec;//Point to HDMI
	else
		pDmExec = &OTT_h_dm->dmExec;//Point to OTT


#if 0	// B05 update move to vo_end timing
	// disable double buffer if need to update B05
	if (g_forceUpdate_3DLUT) {
		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
		dm_double_buf_ctrl_reg.dm_db_en = 0;
		rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
	}

	rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) |
												DM_dm_submodule_enable_dither_en(1));
	// disable B05
	rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) & ~DM_dm_submodule_enable_b05_enable_mask);

#endif
	// enable double buffer
	dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
	dm_double_buf_ctrl_reg.dm_db_apply = 0;
	dm_double_buf_ctrl_reg.dm_db_en = 1;
	rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
	//while (1) {
	//	dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
	//	if (dm_double_buf_ctrl_reg.dm_db_apply == 0)
	//		break;
	//}

	// B01-01
#if 0 //need to check by will
	hdr_all_top_top_ctl_reg.regValue = rtd_inl(HDR_ALL_TOP_TOP_CTL_reg);
	if (mode == DV_HDMI_MODE) {
		//Eric@20180612 if hdr_yuv444_en is enable, need set dm_inbits_del = 1 (10 bit)
		if(hdr_all_top_top_ctl_reg.hdr_yuv444_en == 1)
			rtd_outl(DM_Input_Format_reg, DM_Input_Format_runmode(1)| DM_Input_Format_dm_inbits_sel(1));
		else
			rtd_outl(DM_Input_Format_reg, DM_Input_Format_runmode(1)| DM_Input_Format_dm_inbits_sel(2));
	}else {
		rtd_outl(DM_Input_Format_reg, DM_Input_Format_runmode(0)| DM_Input_Format_dm_inbits_sel(3));
	}
#endif

	// B01-02
	//



	rtd_outl(DM_YCCtoRGB_coef0_reg, DM_YCCtoRGB_coef0_m33yuv2rgb01(pDmExec->m33Yuv2Rgb[0][1]) |
												DM_YCCtoRGB_coef0_m33yuv2rgb00(pDmExec->m33Yuv2Rgb[0][0]));
	rtd_outl(DM_YCCtoRGB_coef1_reg, DM_YCCtoRGB_coef1_m33yuv2rgb02(pDmExec->m33Yuv2Rgb[0][2]));
	rtd_outl(DM_YCCtoRGB_coef2_reg, DM_YCCtoRGB_coef2_m33yuv2rgb11(pDmExec->m33Yuv2Rgb[1][1]) |
												DM_YCCtoRGB_coef2_m33yuv2rgb10(pDmExec->m33Yuv2Rgb[1][0]));
	rtd_outl(DM_YCCtoRGB_coef3_reg, DM_YCCtoRGB_coef3_m33yuv2rgb12(pDmExec->m33Yuv2Rgb[1][2]));
	rtd_outl(DM_YCCtoRGB_coef4_reg, DM_YCCtoRGB_coef4_m33yuv2rgb21(pDmExec->m33Yuv2Rgb[2][1]) |
												DM_YCCtoRGB_coef4_m33yuv2rgb20(pDmExec->m33Yuv2Rgb[2][0]));
	rtd_outl(DM_YCCtoRGB_coef5_reg, DM_YCCtoRGB_coef5_m33yuv2rgb22(pDmExec->m33Yuv2Rgb[2][2]));


	offset0 = DM_YCCtoRGB_offset0_v3yuv2rgboffinrgb0(pDmExec->v3Yuv2RgbOffInRgb[0]);
	offset1 = DM_YCCtoRGB_offset1_v3yuv2rgboffinrgb1(pDmExec->v3Yuv2RgbOffInRgb[1]);
	offset2 = DM_YCCtoRGB_offset2_v3yuv2rgboffinrgb2(pDmExec->v3Yuv2RgbOffInRgb[2]);

	//Eric@20180612 HDMI input 444 10bit case, offset need to divide 4
#if 0 //need to check by will
	if((mode == DV_HDMI_MODE) && (hdr_all_top_top_ctl_reg.hdr_yuv444_en == 1)){
		offset0 = offset0/4;
		offset1 = offset1/4;
		offset2 = offset2/4;
	}
#endif
	rtd_outl(DM_YCCtoRGB_offset0_reg, offset0);
	rtd_outl(DM_YCCtoRGB_offset1_reg, offset1);
	rtd_outl(DM_YCCtoRGB_offset2_reg, offset2);
	rtd_outl(DM_YCCtoRGB_Scale_reg, DM_YCCtoRGB_Scale_ycctorgb_scale(pDmExec->m33Yuv2RgbScale2P));
#if 0
//for RGB bypass
		rtd_outl(DM_YCCtoRGB_coef0_reg, 0x2000);
		rtd_outl(DM_YCCtoRGB_coef1_reg, 0x0);
		rtd_outl(DM_YCCtoRGB_coef2_reg, 0x20000000);
		rtd_outl(DM_YCCtoRGB_coef3_reg, 0);
		rtd_outl(DM_YCCtoRGB_coef4_reg, 0);
		rtd_outl(DM_YCCtoRGB_coef5_reg, 0x2000);

		rtd_outl(DM_YCCtoRGB_offset0_reg, 0);
		rtd_outl(DM_YCCtoRGB_offset1_reg, 0);
		rtd_outl(DM_YCCtoRGB_offset2_reg, 0);
		//rtd_outl(DM_YCCtoRGB_Scale_reg, DM_YCCtoRGB_Scale_ycctorgb_scale(pDmExec->m33Yuv2RgbScale2P));
#endif

	rtd_outl(DM_Signal_range_reg, DM_Signal_range_rangemax(pDmExec->sRange) |
											DM_Signal_range_rangemin(pDmExec->sRangeMin));
	rtd_outl(DM_sRangeInv_reg, DM_sRangeInv_srangeinv(pDmExec->sRangeInv));

	DBG_PRINT("pDmExec->m33Yuv2Rgb[0][1]=0x%x \n", pDmExec->m33Yuv2Rgb[0][1]);
	DBG_PRINT("pDmExec->m33Yuv2Rgb[0][0]=0x%x \n", pDmExec->m33Yuv2Rgb[0][0]);
	DBG_PRINT("pDmExec->m33Yuv2Rgb[0][2]=0x%x \n", pDmExec->m33Yuv2Rgb[0][2]);
	DBG_PRINT("pDmExec->m33Yuv2Rgb[1][1]=0x%x \n", pDmExec->m33Yuv2Rgb[1][1]);
	DBG_PRINT("pDmExec->m33Yuv2Rgb[1][0]=0x%x \n", pDmExec->m33Yuv2Rgb[1][0]);
	DBG_PRINT("pDmExec->m33Yuv2Rgb[1][2]=0x%x \n", pDmExec->m33Yuv2Rgb[1][2]);
	DBG_PRINT("pDmExec->m33Yuv2Rgb[2][1]=0x%x \n", pDmExec->m33Yuv2Rgb[2][1]);
	DBG_PRINT("pDmExec->m33Yuv2Rgb[2][0]=0x%x \n", pDmExec->m33Yuv2Rgb[2][0]);
	DBG_PRINT("pDmExec->m33Yuv2Rgb[2][2]=0x%x \n", pDmExec->m33Yuv2Rgb[2][2]);
	DBG_PRINT("pDmExec->v3Yuv2RgbOffInRgb[0]=0x%x \n", pDmExec->v3Yuv2RgbOffInRgb[0]);
	DBG_PRINT("pDmExec->v3Yuv2RgbOffInRgb[1]=0x%x \n", pDmExec->v3Yuv2RgbOffInRgb[1]);
	DBG_PRINT("pDmExec->v3Yuv2RgbOffInRgb[2]=0x%x \n", pDmExec->v3Yuv2RgbOffInRgb[2]);
	DBG_PRINT("pDmExec->m33Yuv2RgbScale2P=0x%x \n", pDmExec->m33Yuv2RgbScale2P);
	DBG_PRINT("(pDmExec->sRange)=0x%x \n", (pDmExec->sRange));
	DBG_PRINT("(pDmExec->sRangeMin)=0x%x \n", (pDmExec->sRangeMin));
	DBG_PRINT("pDmExec->sRangeInv=0x%x \n", pDmExec->sRangeInv);

	// B01-03
	// TODO: B01-03
	if(mode == DV_HDMI_MODE)
		DM_B01_03_Set(DOLBY_SRC_HDMI);//Set for HDMI
	else
		DM_B01_03_Set(DOLBY_SRC_OTT);//Set for OTT

/*
	rtd_outl(DM_EOTF_para0_reg, DM_EOTF_para0_signal_eotf_param1(pDmExec->sB) |
											DM_EOTF_para0_signal_eotf_param0(pDmExec->sA));
	rtd_outl(DM_EOTF_para1_reg, DM_EOTF_para1_seotf(pDmExec->sEotf) |
											DM_EOTF_para1_signal_eotf(pDmExec->sGamma));
	rtd_outl(DM_EOTF_para2_reg, DM_EOTF_para2_signal_eotf_param2(pDmExec->sG));
*/
	DBG_PRINT("pDmExec->sB=0x%x \n", pDmExec->sB);
	DBG_PRINT("pDmExec->sA=0x%x \n", pDmExec->sA);
	DBG_PRINT("pDmExec->sEotf=0x%x \n", pDmExec->sEotf);
	DBG_PRINT("pDmExec->sGamma=0x%x \n", pDmExec->sGamma);
	DBG_PRINT("pDmExec->sG=0x%x \n", pDmExec->sG);


	// B01-04
	rtd_outl(DM_RGBtoOPT_coef0_reg, DM_RGBtoOPT_coef0_m33rgb2opt01(pDmExec->m33Rgb2Opt[0][1]) |
											DM_RGBtoOPT_coef0_m33rgb2opt00(pDmExec->m33Rgb2Opt[0][0]));
	rtd_outl(DM_RGBtoOPT_coef1_reg, DM_RGBtoOPT_coef1_m33rgb2opt02(pDmExec->m33Rgb2Opt[0][2]));
	rtd_outl(DM_RGBtoOPT_coef2_reg, DM_RGBtoOPT_coef2_m33rgb2opt11(pDmExec->m33Rgb2Opt[1][1]) |
											DM_RGBtoOPT_coef2_m33rgb2opt10(pDmExec->m33Rgb2Opt[1][0]));
	rtd_outl(DM_RGBtoOPT_coef3_reg, DM_RGBtoOPT_coef3_m33rgb2opt12(pDmExec->m33Rgb2Opt[1][2]));
	rtd_outl(DM_RGBtoOPT_coef4_reg, DM_RGBtoOPT_coef4_m33rgb2opt21(pDmExec->m33Rgb2Opt[2][1]) |
											DM_RGBtoOPT_coef4_m33rgb2opt20(pDmExec->m33Rgb2Opt[2][0]));
	rtd_outl(DM_RGBtoOPT_coef5_reg, DM_RGBtoOPT_coef5_m33rgb2opt22(pDmExec->m33Rgb2Opt[2][2]));
	rtd_outl(DM_RGBtoOPT_Scale_reg, DM_RGBtoOPT_Scale_rgb2optscale(pDmExec->m33Rgb2OptScale2P));

	DBG_PRINT("(pDmExec->m33Rgb2Opt[0][1])=0x%x \n", (pDmExec->m33Rgb2Opt[0][1]));
	DBG_PRINT("(pDmExec->m33Rgb2Opt[0][0])=0x%x \n", (pDmExec->m33Rgb2Opt[0][0]));
	DBG_PRINT("(pDmExec->m33Rgb2Opt[0][2])=0x%x \n", (pDmExec->m33Rgb2Opt[0][2]));
	DBG_PRINT("(pDmExec->m33Rgb2Opt[1][1])=0x%x \n", (pDmExec->m33Rgb2Opt[1][1]));
	DBG_PRINT("(pDmExec->m33Rgb2Opt[1][0])=0x%x \n", (pDmExec->m33Rgb2Opt[1][0]));
	DBG_PRINT("(pDmExec->m33Rgb2Opt[1][2])=0x%x \n", (pDmExec->m33Rgb2Opt[1][2]));
	DBG_PRINT("(pDmExec->m33Rgb2Opt[2][1])=0x%x \n", (pDmExec->m33Rgb2Opt[2][1]));
	DBG_PRINT("(pDmExec->m33Rgb2Opt[2][0])=0x%x \n", (pDmExec->m33Rgb2Opt[2][0]));
	DBG_PRINT("(pDmExec->m33Rgb2Opt[2][2])=0x%x \n", (pDmExec->m33Rgb2Opt[2][2]));
	DBG_PRINT("(pDmExec->m33Rgb2OptScale2P)=0x%x \n", (pDmExec->m33Rgb2OptScale2P));


	// B01-06
	rtd_outl(DM_OPTtoOPT_coef0_reg, DM_OPTtoOPT_coef0_m33opt2opt01(pDmExec->m33Opt2Opt[0][1]) |
											DM_OPTtoOPT_coef0_m33opt2opt00(pDmExec->m33Opt2Opt[0][0]));
	rtd_outl(DM_OPTtoOPT_coef1_reg, DM_OPTtoOPT_coef1_m33opt2opt02(pDmExec->m33Opt2Opt[0][2]));
	rtd_outl(DM_OPTtoOPT_coef2_reg, DM_OPTtoOPT_coef2_m33opt2opt11(pDmExec->m33Opt2Opt[1][1]) |
											DM_OPTtoOPT_coef2_m33opt2opt10(pDmExec->m33Opt2Opt[1][0]));
	rtd_outl(DM_OPTtoOPT_coef3_reg, DM_OPTtoOPT_coef3_m33opt2opt12(pDmExec->m33Opt2Opt[1][2]));
	rtd_outl(DM_OPTtoOPT_coef4_reg, DM_OPTtoOPT_coef4_m33opt2opt21(pDmExec->m33Opt2Opt[2][1]) |
											DM_OPTtoOPT_coef4_m33opt2opt20(pDmExec->m33Opt2Opt[2][0]));
	rtd_outl(DM_OPTtoOPT_coef5_reg, DM_OPTtoOPT_coef5_m33opt2opt22(pDmExec->m33Opt2Opt[2][2]));
	rtd_outl(DM_OPTtoOPT_Offset_reg, DM_OPTtoOPT_Offset_opt2optoffset(pDmExec->Opt2OptOffset));
	rtd_outl(DM_OPTtoOPT_Scale_reg, DM_OPTtoOPT_Scale_opt2optscale(pDmExec->m33Opt2OptScale2P));

	DBG_PRINT("(pDmExec->m33Opt2Opt[0][1])=0x%x \n", (pDmExec->m33Opt2Opt[0][1]));
	DBG_PRINT("(pDmExec->m33Opt2Opt[0][0])=0x%x \n", (pDmExec->m33Opt2Opt[0][0]));
	DBG_PRINT("(pDmExec->m33Opt2Opt[0][2])=0x%x \n", (pDmExec->m33Opt2Opt[0][2]));
	DBG_PRINT("(pDmExec->m33Opt2Opt[1][1])=0x%x \n", (pDmExec->m33Opt2Opt[1][1]));
	DBG_PRINT("(pDmExec->m33Opt2Opt[1][0])=0x%x \n", (pDmExec->m33Opt2Opt[1][0]));
	DBG_PRINT("(pDmExec->m33Opt2Opt[1][2])=0x%x \n", (pDmExec->m33Opt2Opt[1][2]));
	DBG_PRINT("(pDmExec->m33Opt2Opt[2][1])=0x%x \n", (pDmExec->m33Opt2Opt[2][1]));
	DBG_PRINT("(pDmExec->m33Opt2Opt[2][0])=0x%x \n", (pDmExec->m33Opt2Opt[2][0]));
	DBG_PRINT("(pDmExec->m33Opt2Opt[2][2])=0x%x \n", (pDmExec->m33Opt2Opt[2][2]));
	DBG_PRINT("(pDmExec->Opt2OptOffset)=0x%x \n", (pDmExec->Opt2OptOffset));
	DBG_PRINT("(pDmExec->m33Opt2OptScale2P)=0x%x \n", (pDmExec->m33Opt2OptScale2P));


	// B01-07
	rtd_outl(DM_LumaAdj_chromaWeight_reg, DM_LumaAdj_chromaWeight_chromaweight(pDmExec->chromaWeight));

	DBG_PRINT("(pDmExec->chromaWeight)=0x%x \n", (pDmExec->chromaWeight));


	// B02
	//if (B02_update)
	if (1)
	{
		if(mode == DV_HDMI_MODE)
			DM_B02_Set(DOLBY_SRC_HDMI);//Set for HDMI
		else
			DM_B02_Set(DOLBY_SRC_OTT);//Set for OTT
	}

	// B03
	rtd_outl(DM_BlendDbEdge_reg, DM_BlendDbEdge_weight(pDmExec->msWeight) |
										DM_BlendDbEdge_weight_edge(pDmExec->msWeightEdge));
#if 0
	rtd_outl(DM_FilterRow0_reg, DM_FilterRow0_filter_row_c01(pDmExec->msFilterRow[1]) |
										DM_FilterRow0_filter_row_c00(pDmExec->msFilterRow[0]));
	rtd_outl(DM_FilterRow1_reg, DM_FilterRow1_filter_row_c03(pDmExec->msFilterRow[3]) |
										DM_FilterRow1_filter_row_c02(pDmExec->msFilterRow[2]));
	rtd_outl(DM_FilterRow2_reg, DM_FilterRow2_filter_row_c05(pDmExec->msFilterRow[5]) |
										DM_FilterRow2_filter_row_c04(pDmExec->msFilterRow[4]));
	rtd_outl(DM_FilterCol0_reg, DM_FilterCol0_filter_col_c01(pDmExec->msFilterCol[1]) |
										DM_FilterCol0_filter_col_c00(pDmExec->msFilterCol[0]));
	rtd_outl(DM_FilterCol1_reg, DM_FilterCol1_filter_col_c02(pDmExec->msFilterCol[2]));
	rtd_outl(DM_FilterEdgeRow0_reg, DM_FilterEdgeRow0_filter_edge_row_c01(pDmExec->msFilterEdgeRow[1]) |
											DM_FilterEdgeRow0_filter_edge_row_c00(pDmExec->msFilterEdgeRow[0]));
	rtd_outl(DM_FilterEdgeRow1_reg, DM_FilterEdgeRow1_filter_edge_row_c03(pDmExec->msFilterEdgeRow[3]) |
											DM_FilterEdgeRow1_filter_edge_row_c02(pDmExec->msFilterEdgeRow[2]));
	rtd_outl(DM_FilterEdgeRow2_reg, DM_FilterEdgeRow2_filter_edge_row_c04(pDmExec->msFilterEdgeRow[4]));
	rtd_outl(DM_FilterEdgeCol0_reg, DM_FilterEdgeCol0_filter_edge_col_c01(pDmExec->msFilterEdgeCol[1]) |
											DM_FilterEdgeCol0_filter_edge_col_c00(pDmExec->msFilterEdgeCol[0]));

#endif
	DBG_PRINT("(pDmExec->msWeight)=0x%x \n", (pDmExec->msWeight));
	DBG_PRINT("(pDmExec->msWeightEdge)=0x%x \n", (pDmExec->msWeightEdge));
#if 0
	DBG_PRINT("(pDmExec->msFilterRow[1])=0x%x \n", (pDmExec->msFilterRow[1]));
	DBG_PRINT("(pDmExec->msFilterRow[0])=0x%x \n", (pDmExec->msFilterRow[0]));
	DBG_PRINT("(pDmExec->msFilterRow[3])=0x%x \n", (pDmExec->msFilterRow[3]));
	DBG_PRINT("(pDmExec->msFilterRow[2])=0x%x \n", (pDmExec->msFilterRow[2]));
	DBG_PRINT("(pDmExec->msFilterRow[5])=0x%x \n", (pDmExec->msFilterRow[5]));
	DBG_PRINT("(pDmExec->msFilterRow[4])=0x%x \n", (pDmExec->msFilterRow[4]));
	DBG_PRINT("(pDmExec->msFilterCol[1])=0x%x \n", (pDmExec->msFilterCol[1]));
	DBG_PRINT("(pDmExec->msFilterCol[0])=0x%x \n", (pDmExec->msFilterCol[0]));
	DBG_PRINT("(pDmExec->msFilterCol[2])=0x%x \n", (pDmExec->msFilterCol[2]));
	DBG_PRINT("(pDmExec->msFilterEdgeRow[1])=0x%x \n", (pDmExec->msFilterEdgeRow[1]));
	DBG_PRINT("(pDmExec->msFilterEdgeRow[0])=0x%x \n", (pDmExec->msFilterEdgeRow[0]));
	DBG_PRINT("(pDmExec->msFilterEdgeRow[3])=0x%x \n", (pDmExec->msFilterEdgeRow[3]));
	DBG_PRINT("(pDmExec->msFilterEdgeRow[2])=0x%x \n", (pDmExec->msFilterEdgeRow[2]));
	DBG_PRINT("(pDmExec->msFilterEdgeRow[4])=0x%x \n", (pDmExec->msFilterEdgeRow[4]));
	DBG_PRINT("(pDmExec->msFilterEdgeCol[1])=0x%x \n", (pDmExec->msFilterEdgeCol[1]));
	DBG_PRINT("(pDmExec->msFilterEdgeCol[0])=0x%x \n", (pDmExec->msFilterEdgeCol[0]));
#endif

	// B04
	rtd_outl(DM_SaturationAdj_reg, DM_SaturationAdj_saturationadj_offset(pDmExec->offset) |
											DM_SaturationAdj_saturationadj_gain(pDmExec->gain));
	rtd_outl(DM_SaturationGain_reg, DM_SaturationGain_saturationgain(pDmExec->saturationGain));

	DBG_PRINT("(pDmExec->offset)=0x%x \n", (pDmExec->offset));
	DBG_PRINT("(pDmExec->gain)=0x%x \n", (pDmExec->gain));
	DBG_PRINT("(pDmExec->saturationGain)=0x%x \n", (pDmExec->saturationGain));


	// B05
	rtd_outl(DM_ThreeDLUT_MinMaxC1_reg, DM_ThreeDLUT_MinMaxC1_maxc1(pDmExec->bwDmLut.iMaxC1) |
												DM_ThreeDLUT_MinMaxC1_minc1(pDmExec->bwDmLut.iMinC1));
	rtd_outl(DM_ThreeDLUT_MinMaxC2_reg, DM_ThreeDLUT_MinMaxC2_maxc2(pDmExec->bwDmLut.iMaxC2) |
												DM_ThreeDLUT_MinMaxC2_minc2(pDmExec->bwDmLut.iMinC2));
	rtd_outl(DM_ThreeDLUT_MinMaxC3_reg, DM_ThreeDLUT_MinMaxC3_minc3(pDmExec->bwDmLut.iMinC3));
	rtd_outl(DM_ThreeDLUT_MinMaxInv1_reg, DM_ThreeDLUT_MinMaxInv1_idistc1inv(pDmExec->bwDmLut.iDistC1Inv));
	rtd_outl(DM_ThreeDLUT_MinMaxInv2_reg, DM_ThreeDLUT_MinMaxInv2_idistc2inv(pDmExec->bwDmLut.iDistC2Inv));
	rtd_outl(DM_ThreeDLUT_MinMaxInv3_reg, DM_ThreeDLUT_MinMaxInv3_idistc3inv(pDmExec->bwDmLut.iDistC3Inv));


	DBG_PRINT("(pDmExec->bwDmLut.iMaxC1)=0x%x \n", (pDmExec->bwDmLut.iMaxC1));
	DBG_PRINT("(pDmExec->bwDmLut.iMinC1)=0x%x \n", (pDmExec->bwDmLut.iMinC1));
	DBG_PRINT("(pDmExec->bwDmLut.iMaxC2)=0x%x \n", (pDmExec->bwDmLut.iMaxC2));
	DBG_PRINT("(pDmExec->bwDmLut.iMinC2)=0x%x \n", (pDmExec->bwDmLut.iMinC2));
	DBG_PRINT("(pDmExec->bwDmLut.iMinC3)=0x%x \n", (pDmExec->bwDmLut.iMinC3));
	DBG_PRINT("(pDmExec->bwDmLut.iDistC1Inv)=0x%x \n", (pDmExec->bwDmLut.iDistC1Inv));
	DBG_PRINT("(pDmExec->bwDmLut.iDistC2Inv)=0x%x \n", (pDmExec->bwDmLut.iDistC2Inv));
	DBG_PRINT("(pDmExec->bwDmLut.iDistC3Inv)=0x%x \n", (pDmExec->bwDmLut.iDistC3Inv));

	// B05 3D LUT
	//DM_B05_Set(pDmExec->bwDmLut.lutMap);// B05 update move to vo_end timing


    //using fix pattern :enable pattern 0
	//disable dither fix pattern at normal case
	//Eric@20180612 HDMI yuv444_en need to set dm dither 10 bit
#if 0 //need to check by will
	if((mode == DV_HDMI_MODE) && (top_ctl_reg.hdr_yuv444_en == 1))
		rtd_outl(DM_DITHER_SET_reg,DM_DITHER_SET_dither_fix_pattern_en(0) | DM_DITHER_SET_dither_fix_pattern_num(0) | DM_DITHER_SET_dither_in_format(1));
	else
		rtd_outl(DM_DITHER_SET_reg,DM_DITHER_SET_dither_fix_pattern_en(0) | DM_DITHER_SET_dither_fix_pattern_num(0));
#endif
	//set_DM_DITHER_SET_reg(DM_DITHER_SET_dither_fix_pattern_en(0) | DM_DITHER_SET_dither_fix_pattern_num(0));

	// all submodule enable bit12 enable dither
	//bit 11 , 0:enable b05_02 444to422
	//rtd_outl(DM_dm_submodule_enable_reg, 0x17ff);
	rtd_outl(DM_dm_submodule_enable_reg,
			 DM_dm_submodule_enable_dither_en(1) | DM_dm_submodule_enable_b0502_enable(0) |
			 DM_dm_submodule_enable_b0501_enable(1) | DM_dm_submodule_enable_b04_enable(1) |
			 DM_dm_submodule_enable_b02_enable(1)|
			 DM_dm_submodule_enable_b03_enable(1) | DM_dm_submodule_enable_b01_07_enable(1) |
			 DM_dm_submodule_enable_b01_06_enable(1) | DM_dm_submodule_enable_b01_05_enable(1) |
			 DM_dm_submodule_enable_b01_04_enable(1) | DM_dm_submodule_enable_b01_03_enable(1) |
			 DM_dm_submodule_enable_b01_02_enable(1) | DM_dm_submodule_enable_b01_01_422to444_enable(1) |
			 DM_dm_submodule_enable_b01_01_420to422_enable(1));

	// apply

	/*
			dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
			dm_double_buf_ctrl_reg.dm_db_apply = 0;
			dm_double_buf_ctrl_reg.dm_db_en = 1;
			rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);


	dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
	dm_double_buf_ctrl_reg.dm_db_apply = 1;
	rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
	//while (1) {
		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
		if (dm_double_buf_ctrl_reg.dm_db_apply == 0)
			break;
	}
	*/

}

void ComposerRbusSet(rpu_ext_config_fixpt_main_t *p_comp)
{
	dhdr_v_composer_composer_db_ctrl_RBUS comp_db_ctr_reg;

	// double buffer enable
	comp_db_ctr_reg.regValue = rtd_inl(DHDR_V_COMPOSER_Composer_db_ctrl_reg);
	comp_db_ctr_reg.composer_db_en = 1;
	rtd_outl(DHDR_V_COMPOSER_Composer_db_ctrl_reg, comp_db_ctr_reg.regValue);



	if(p_comp->rpu_BL_bit_depth==8) {
	rtd_outl(DHDR_V_COMPOSER_Composer_reg,
					DHDR_V_COMPOSER_Composer_vdr_bit_depth(p_comp->rpu_VDR_bit_depth) |
					DHDR_V_COMPOSER_Composer_bl_bit_depth(0) |
					DHDR_V_COMPOSER_Composer_coefficient_log2_denom(p_comp->coefficient_log2_denom));
	}else if(p_comp->rpu_BL_bit_depth==10) {
	rtd_outl(DHDR_V_COMPOSER_Composer_reg,
					DHDR_V_COMPOSER_Composer_vdr_bit_depth(p_comp->rpu_VDR_bit_depth) |
					DHDR_V_COMPOSER_Composer_bl_bit_depth(1) |
					DHDR_V_COMPOSER_Composer_coefficient_log2_denom(p_comp->coefficient_log2_denom));
	}else{
		printk(KERN_ERR "%s unupported BL bit depth = %d\n",__FUNCTION__,p_comp->rpu_BL_bit_depth);
		return;

	}

	DBG_PRINT("rpu_VDR_bit_depth=0x%x \n", p_comp->rpu_VDR_bit_depth);
	DBG_PRINT("rpu_BL_bit_depth=0x%x \n", p_comp->rpu_BL_bit_depth);
	DBG_PRINT("coefficient_log2_denom=0x%x \n", p_comp->coefficient_log2_denom);
	DBG_PRINT("disable_EL_flag=0x%x \n", p_comp->disable_residual_flag);
	DBG_PRINT("el_resampling=0x%x \n", p_comp->el_spatial_resampling_filter_flag);

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_reg,
					DHDR_V_COMPOSER_Composer_BL_num_pivots_cmp2(p_comp->num_pivots[2]) |
					DHDR_V_COMPOSER_Composer_BL_num_pivots_cmp1(p_comp->num_pivots[1]) |
					DHDR_V_COMPOSER_Composer_BL_num_pivots_cmp0(p_comp->num_pivots[0]) |
					DHDR_V_COMPOSER_Composer_BL_mapping_idc_cmp2(p_comp->mapping_idc[2]) |
					DHDR_V_COMPOSER_Composer_BL_mapping_idc_cmp1(p_comp->mapping_idc[1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp0_P01_reg,
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp0_P01_pivot_value_cmp0_pivot_1(p_comp->pivot_value[0][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp0_P01_pivot_value_cmp0_pivot_0(p_comp->pivot_value[0][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp0_P23_reg,
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp0_P23_pivot_value_cmp0_pivot_3(p_comp->pivot_value[0][3]) |
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp0_P23_pivot_value_cmp0_pivot_2(p_comp->pivot_value[0][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp0_P45_reg,
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp0_P45_pivot_value_cmp0_pivot_5(p_comp->pivot_value[0][5]) |
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp0_P45_pivot_value_cmp0_pivot_4(p_comp->pivot_value[0][4]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp0_P67_reg,
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp0_P67_pivot_value_cmp0_pivot_7(p_comp->pivot_value[0][7]) |
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp0_P67_pivot_value_cmp0_pivot_6(p_comp->pivot_value[0][6]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp0_P8_reg,
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp0_P8_pivot_value_cmp0_pivot_8(p_comp->pivot_value[0][8]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp1_P01_reg,
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp1_P01_pivot_value_cmp1_pivot_1(p_comp->pivot_value[1][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp1_P01_pivot_value_cmp1_pivot_0(p_comp->pivot_value[1][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp1_P23_reg,
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp1_P23_pivot_value_cmp1_pivot_3(p_comp->pivot_value[1][3]) |
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp1_P23_pivot_value_cmp1_pivot_2(p_comp->pivot_value[1][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp1_P45_reg,
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp1_P45_pivot_value_cmp1_pivot_4(p_comp->pivot_value[1][4]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp2_P01_reg,
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp2_P01_pivot_value_cmp2_pivot_1(p_comp->pivot_value[2][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp2_P01_pivot_value_cmp2_pivot_0(p_comp->pivot_value[2][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp2_P23_reg,
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp2_P23_pivot_value_cmp2_pivot_3(p_comp->pivot_value[2][3]) |
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp2_P23_pivot_value_cmp2_pivot_2(p_comp->pivot_value[2][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp2_P45_reg,
					DHDR_V_COMPOSER_Composer_BL_Pivot_Val_Cmp2_P45_pivot_value_cmp2_pivot_4(p_comp->pivot_value[2][4]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Luma_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Luma_poly_order_cmp0_pivot7(p_comp->poly_order[0][7]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Luma_poly_order_cmp0_pivot6(p_comp->poly_order[0][6]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Luma_poly_order_cmp0_pivot5(p_comp->poly_order[0][5]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Luma_poly_order_cmp0_pivot4(p_comp->poly_order[0][4]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Luma_poly_order_cmp0_pivot3(p_comp->poly_order[0][3]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Luma_poly_order_cmp0_pivot2(p_comp->poly_order[0][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Luma_poly_order_cmp0_pivot1(p_comp->poly_order[0][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Luma_poly_order_cmp0_pivot0(p_comp->poly_order[0][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Chroma1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Chroma1_poly_order_cmp1_pivot3(p_comp->poly_order[1][3]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Chroma1_poly_order_cmp1_pivot2(p_comp->poly_order[1][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Chroma1_poly_order_cmp1_pivot1(p_comp->poly_order[1][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Chroma1_poly_order_cmp1_pivot0(p_comp->poly_order[1][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Chroma2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Chroma2_poly_order_cmp2_pivot3(p_comp->poly_order[2][3]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Chroma2_poly_order_cmp2_pivot2(p_comp->poly_order[2][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Chroma2_poly_order_cmp2_pivot1(p_comp->poly_order[2][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Order_Chroma2_poly_order_cmp2_pivot0(p_comp->poly_order[2][0]));
/*
	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P0_poly_coef_int_cmp0_pivot0_i2(p_comp->poly_coef_int[0][0][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P0_poly_coef_int_cmp0_pivot0_i1(p_comp->poly_coef_int[0][0][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P0_poly_coef_int_cmp0_pivot0_i0(p_comp->poly_coef_int[0][0][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P1_poly_coef_int_cmp0_pivot1_i2(p_comp->poly_coef_int[0][1][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P1_poly_coef_int_cmp0_pivot1_i1(p_comp->poly_coef_int[0][1][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P1_poly_coef_int_cmp0_pivot1_i0(p_comp->poly_coef_int[0][1][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P2_poly_coef_int_cmp0_pivot2_i2(p_comp->poly_coef_int[0][2][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P2_poly_coef_int_cmp0_pivot2_i1(p_comp->poly_coef_int[0][2][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P2_poly_coef_int_cmp0_pivot2_i0(p_comp->poly_coef_int[0][2][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P3_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P3_poly_coef_int_cmp0_pivot3_i2(p_comp->poly_coef_int[0][3][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P3_poly_coef_int_cmp0_pivot3_i1(p_comp->poly_coef_int[0][3][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P3_poly_coef_int_cmp0_pivot3_i0(p_comp->poly_coef_int[0][3][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P4_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P4_poly_coef_int_cmp0_pivot4_i2(p_comp->poly_coef_int[0][4][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P4_poly_coef_int_cmp0_pivot4_i1(p_comp->poly_coef_int[0][4][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P4_poly_coef_int_cmp0_pivot4_i0(p_comp->poly_coef_int[0][4][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P5_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P5_poly_coef_int_cmp0_pivot5_i2(p_comp->poly_coef_int[0][5][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P5_poly_coef_int_cmp0_pivot5_i1(p_comp->poly_coef_int[0][5][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P5_poly_coef_int_cmp0_pivot5_i0(p_comp->poly_coef_int[0][5][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P6_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P6_poly_coef_int_cmp0_pivot6_i2(p_comp->poly_coef_int[0][6][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P6_poly_coef_int_cmp0_pivot6_i1(p_comp->poly_coef_int[0][6][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P6_poly_coef_int_cmp0_pivot6_i0(p_comp->poly_coef_int[0][6][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P7_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P7_poly_coef_int_cmp0_pivot7_i2(p_comp->poly_coef_int[0][7][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P7_poly_coef_int_cmp0_pivot7_i1(p_comp->poly_coef_int[0][7][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp0_P7_poly_coef_int_cmp0_pivot7_i0(p_comp->poly_coef_int[0][7][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp1_P0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp1_P0_poly_coef_int_cmp1_pivot0_i2(p_comp->poly_coef_int[1][0][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp1_P0_poly_coef_int_cmp1_pivot0_i1(p_comp->poly_coef_int[1][0][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp1_P0_poly_coef_int_cmp1_pivot0_i0(p_comp->poly_coef_int[1][0][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp1_P1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp1_P1_poly_coef_int_cmp1_pivot1_i2(p_comp->poly_coef_int[1][1][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp1_P1_poly_coef_int_cmp1_pivot1_i1(p_comp->poly_coef_int[1][1][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp1_P1_poly_coef_int_cmp1_pivot1_i0(p_comp->poly_coef_int[1][1][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp1_P2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp1_P2_poly_coef_int_cmp1_pivot2_i2(p_comp->poly_coef_int[1][2][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp1_P2_poly_coef_int_cmp1_pivot2_i1(p_comp->poly_coef_int[1][2][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp1_P2_poly_coef_int_cmp1_pivot2_i0(p_comp->poly_coef_int[1][2][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp1_P3_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp1_P3_poly_coef_int_cmp1_pivot3_i2(p_comp->poly_coef_int[1][3][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp1_P3_poly_coef_int_cmp1_pivot3_i1(p_comp->poly_coef_int[1][3][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp1_P3_poly_coef_int_cmp1_pivot3_i0(p_comp->poly_coef_int[1][3][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp2_P0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp2_P0_poly_coef_int_cmp2_pivot0_i2(p_comp->poly_coef_int[2][0][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp2_P0_poly_coef_int_cmp2_pivot0_i1(p_comp->poly_coef_int[2][0][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp2_P0_poly_coef_int_cmp2_pivot0_i0(p_comp->poly_coef_int[2][0][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp2_P1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp2_P1_poly_coef_int_cmp2_pivot1_i2(p_comp->poly_coef_int[2][1][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp2_P1_poly_coef_int_cmp2_pivot1_i1(p_comp->poly_coef_int[2][1][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp2_P1_poly_coef_int_cmp2_pivot1_i0(p_comp->poly_coef_int[2][1][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp2_P2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp2_P2_poly_coef_int_cmp2_pivot2_i2(p_comp->poly_coef_int[2][2][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp2_P2_poly_coef_int_cmp2_pivot2_i1(p_comp->poly_coef_int[2][2][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp2_P2_poly_coef_int_cmp2_pivot2_i0(p_comp->poly_coef_int[2][2][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp2_P3_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp2_P3_poly_coef_int_cmp2_pivot3_i2(p_comp->poly_coef_int[2][3][2]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp2_P3_poly_coef_int_cmp2_pivot3_i1(p_comp->poly_coef_int[2][3][1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_INT_Cmp2_P3_poly_coef_int_cmp2_pivot3_i0(p_comp->poly_coef_int[2][3][0]));
*/
	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P0_I0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P0_I0_poly_coef_cmp0_pivot0_i0(p_comp->poly_coef[0][0][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P0_I1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P0_I1_poly_coef_cmp0_pivot0_i1(p_comp->poly_coef[0][0][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P0_I2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P0_I2_poly_coef_cmp0_pivot0_i2(p_comp->poly_coef[0][0][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P1_I0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P1_I0_poly_coef_cmp0_pivot1_i0(p_comp->poly_coef[0][1][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P1_I1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P1_I1_poly_coef_cmp0_pivot1_i1(p_comp->poly_coef[0][1][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P1_I2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P1_I2_poly_coef_cmp0_pivot1_i2(p_comp->poly_coef[0][1][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P2_I0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P2_I0_poly_coef_cmp0_pivot2_i0(p_comp->poly_coef[0][2][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P2_I1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P2_I1_poly_coef_cmp0_pivot2_i1(p_comp->poly_coef[0][2][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P2_I2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P2_I2_poly_coef_cmp0_pivot2_i2(p_comp->poly_coef[0][2][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P3_I0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P3_I0_poly_coef_cmp0_pivot3_i0(p_comp->poly_coef[0][3][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P3_I1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P3_I1_poly_coef_cmp0_pivot3_i1(p_comp->poly_coef[0][3][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P3_I2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P3_I2_poly_coef_cmp0_pivot3_i2(p_comp->poly_coef[0][3][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P4_I0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P4_I0_poly_coef_cmp0_pivot4_i0(p_comp->poly_coef[0][4][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P4_I1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P4_I1_poly_coef_cmp0_pivot4_i1(p_comp->poly_coef[0][4][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P4_I2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P4_I2_poly_coef_cmp0_pivot4_i2(p_comp->poly_coef[0][4][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P5_I0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P5_I0_poly_coef_cmp0_pivot5_i0(p_comp->poly_coef[0][5][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P5_I1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P5_I1_poly_coef_cmp0_pivot5_i1(p_comp->poly_coef[0][5][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P5_I2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P5_I2_poly_coef_cmp0_pivot5_i2(p_comp->poly_coef[0][5][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P6_I0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P6_I0_poly_coef_cmp0_pivot6_i0(p_comp->poly_coef[0][6][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P6_I1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P6_I1_poly_coef_cmp0_pivot6_i1(p_comp->poly_coef[0][6][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P6_I2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P6_I2_poly_coef_cmp0_pivot6_i2(p_comp->poly_coef[0][6][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P7_I0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P7_I0_poly_coef_cmp0_pivot7_i0(p_comp->poly_coef[0][7][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P7_I1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P7_I1_poly_coef_cmp0_pivot7_i1(p_comp->poly_coef[0][7][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P7_I2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp0_P7_I2_poly_coef_cmp0_pivot7_i2(p_comp->poly_coef[0][7][2]));


	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P0_I0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P0_I0_poly_coef_cmp1_pivot0_i0(p_comp->poly_coef[1][0][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P0_I1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P0_I1_poly_coef_cmp1_pivot0_i1(p_comp->poly_coef[1][0][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P0_I2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P0_I2_poly_coef_cmp1_pivot0_i2(p_comp->poly_coef[1][0][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P1_I0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P1_I0_poly_coef_cmp1_pivot1_i0(p_comp->poly_coef[1][1][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P1_I1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P1_I1_poly_coef_cmp1_pivot1_i1(p_comp->poly_coef[1][1][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P1_I2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P1_I2_poly_coef_cmp1_pivot1_i2(p_comp->poly_coef[1][1][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P2_I0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P2_I0_poly_coef_cmp1_pivot2_i0(p_comp->poly_coef[1][2][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P2_I1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P2_I1_poly_coef_cmp1_pivot2_i1(p_comp->poly_coef[1][2][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P2_I2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P2_I2_poly_coef_cmp1_pivot2_i2(p_comp->poly_coef[1][2][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P3_I0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P3_I0_poly_coef_cmp1_pivot3_i0(p_comp->poly_coef[1][3][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P3_I1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P3_I1_poly_coef_cmp1_pivot3_i1(p_comp->poly_coef[1][3][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P3_I2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp1_P3_I2_poly_coef_cmp1_pivot3_i2(p_comp->poly_coef[1][3][2]));


	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P0_I0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P0_I0_poly_coef_cmp2_pivot0_i0(p_comp->poly_coef[2][0][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P0_I1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P0_I1_poly_coef_cmp2_pivot0_i1(p_comp->poly_coef[2][0][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P0_I2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P0_I2_poly_coef_cmp2_pivot0_i2(p_comp->poly_coef[2][0][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P1_I0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P1_I0_poly_coef_cmp2_pivot1_i0(p_comp->poly_coef[2][1][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P1_I1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P1_I1_poly_coef_cmp2_pivot1_i1(p_comp->poly_coef[2][1][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P1_I2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P1_I2_poly_coef_cmp2_pivot1_i2(p_comp->poly_coef[2][1][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P2_I0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P2_I0_poly_coef_cmp2_pivot2_i0(p_comp->poly_coef[2][2][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P2_I1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P2_I1_poly_coef_cmp2_pivot2_i1(p_comp->poly_coef[2][2][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P2_I2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P2_I2_poly_coef_cmp2_pivot2_i2(p_comp->poly_coef[2][2][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P3_I0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P3_I0_poly_coef_cmp2_pivot3_i0(p_comp->poly_coef[2][3][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P3_I1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P3_I1_poly_coef_cmp2_pivot3_i1(p_comp->poly_coef[2][3][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P3_I2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_Poly_Coef_Cmp2_P3_I2_poly_coef_cmp2_pivot3_i2(p_comp->poly_coef[2][3][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Order_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Order_mmr_order_cmp2(p_comp->MMR_order[1]) |
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Order_mmr_order_cmp1(p_comp->MMR_order[0]));
/*20180315 pinyen remove unused DV register
	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_0_mmr_coef_int_cmp1_coef0(p_comp->MMR_coef_int[0][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_1_mmr_coef_int_cmp1_coef1(p_comp->MMR_coef_int[0][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_2_mmr_coef_int_cmp1_coef2(p_comp->MMR_coef_int[0][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_3_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_3_mmr_coef_int_cmp1_coef3(p_comp->MMR_coef_int[0][3]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_4_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_4_mmr_coef_int_cmp1_coef4(p_comp->MMR_coef_int[0][4]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_5_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_5_mmr_coef_int_cmp1_coef5(p_comp->MMR_coef_int[0][5]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_6_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_6_mmr_coef_int_cmp1_coef6(p_comp->MMR_coef_int[0][6]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_7_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_7_mmr_coef_int_cmp1_coef7(p_comp->MMR_coef_int[0][7]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_8_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_8_mmr_coef_int_cmp1_coef8(p_comp->MMR_coef_int[0][8]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_9_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_9_mmr_coef_int_cmp1_coef9(p_comp->MMR_coef_int[0][9]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_10_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_10_mmr_coef_int_cmp1_coef10(p_comp->MMR_coef_int[0][10]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_11_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_11_mmr_coef_int_cmp1_coef11(p_comp->MMR_coef_int[0][11]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_12_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_12_mmr_coef_int_cmp1_coef12(p_comp->MMR_coef_int[0][12]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_13_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_13_mmr_coef_int_cmp1_coef13(p_comp->MMR_coef_int[0][13]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_14_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_14_mmr_coef_int_cmp1_coef14(p_comp->MMR_coef_int[0][14]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_15_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_15_mmr_coef_int_cmp1_coef15(p_comp->MMR_coef_int[0][15]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_16_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_16_mmr_coef_int_cmp1_coef16(p_comp->MMR_coef_int[0][16]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_17_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_17_mmr_coef_int_cmp1_coef17(p_comp->MMR_coef_int[0][17]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_18_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_18_mmr_coef_int_cmp1_coef18(p_comp->MMR_coef_int[0][18]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_19_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_19_mmr_coef_int_cmp1_coef19(p_comp->MMR_coef_int[0][19]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_20_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_20_mmr_coef_int_cmp1_coef20(p_comp->MMR_coef_int[0][20]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_21_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_INT_21_mmr_coef_int_cmp1_coef21(p_comp->MMR_coef_int[0][21]));



	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_0_mmr_coef_int_cmp2_coef0(p_comp->MMR_coef_int[1][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_1_mmr_coef_int_cmp2_coef1(p_comp->MMR_coef_int[1][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_2_mmr_coef_int_cmp2_coef2(p_comp->MMR_coef_int[1][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_3_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_3_mmr_coef_int_cmp2_coef3(p_comp->MMR_coef_int[1][3]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_4_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_4_mmr_coef_int_cmp2_coef4(p_comp->MMR_coef_int[1][4]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_5_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_5_mmr_coef_int_cmp2_coef5(p_comp->MMR_coef_int[1][5]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_6_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_6_mmr_coef_int_cmp2_coef6(p_comp->MMR_coef_int[1][6]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_7_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_7_mmr_coef_int_cmp2_coef7(p_comp->MMR_coef_int[1][7]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_8_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_8_mmr_coef_int_cmp2_coef8(p_comp->MMR_coef_int[1][8]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_9_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_9_mmr_coef_int_cmp2_coef9(p_comp->MMR_coef_int[1][9]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_10_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_10_mmr_coef_int_cmp2_coef10(p_comp->MMR_coef_int[1][10]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_11_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_11_mmr_coef_int_cmp2_coef11(p_comp->MMR_coef_int[1][11]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_12_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_12_mmr_coef_int_cmp2_coef12(p_comp->MMR_coef_int[1][12]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_13_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_13_mmr_coef_int_cmp2_coef13(p_comp->MMR_coef_int[1][13]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_14_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_14_mmr_coef_int_cmp2_coef14(p_comp->MMR_coef_int[1][14]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_15_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_15_mmr_coef_int_cmp2_coef15(p_comp->MMR_coef_int[1][15]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_16_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_16_mmr_coef_int_cmp2_coef16(p_comp->MMR_coef_int[1][16]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_17_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_17_mmr_coef_int_cmp2_coef17(p_comp->MMR_coef_int[1][17]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_18_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_18_mmr_coef_int_cmp2_coef18(p_comp->MMR_coef_int[1][18]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_19_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_19_mmr_coef_int_cmp2_coef19(p_comp->MMR_coef_int[1][19]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_20_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_20_mmr_coef_int_cmp2_coef20(p_comp->MMR_coef_int[1][20]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_21_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_INT_21_mmr_coef_int_cmp2_coef21(p_comp->MMR_coef_int[1][21]));
*/


	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_0_mmr_coef_cmp1_coef0(p_comp->MMR_coef[0][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_1_mmr_coef_cmp1_coef1(p_comp->MMR_coef[0][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_2_mmr_coef_cmp1_coef2(p_comp->MMR_coef[0][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_3_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_3_mmr_coef_cmp1_coef3(p_comp->MMR_coef[0][3]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_4_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_4_mmr_coef_cmp1_coef4(p_comp->MMR_coef[0][4]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_5_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_5_mmr_coef_cmp1_coef5(p_comp->MMR_coef[0][5]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_6_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_6_mmr_coef_cmp1_coef6(p_comp->MMR_coef[0][6]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_7_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_7_mmr_coef_cmp1_coef7(p_comp->MMR_coef[0][7]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_8_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_8_mmr_coef_cmp1_coef8(p_comp->MMR_coef[0][8]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_9_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_9_mmr_coef_cmp1_coef9(p_comp->MMR_coef[0][9]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_10_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_10_mmr_coef_cmp1_coef10(p_comp->MMR_coef[0][10]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_11_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_11_mmr_coef_cmp1_coef11(p_comp->MMR_coef[0][11]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_12_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_12_mmr_coef_cmp1_coef12(p_comp->MMR_coef[0][12]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_13_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_13_mmr_coef_cmp1_coef13(p_comp->MMR_coef[0][13]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_14_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_14_mmr_coef_cmp1_coef14(p_comp->MMR_coef[0][14]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_15_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_15_mmr_coef_cmp1_coef15(p_comp->MMR_coef[0][15]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_16_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_16_mmr_coef_cmp1_coef16(p_comp->MMR_coef[0][16]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_17_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_17_mmr_coef_cmp1_coef17(p_comp->MMR_coef[0][17]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_18_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_18_mmr_coef_cmp1_coef18(p_comp->MMR_coef[0][18]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_19_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_19_mmr_coef_cmp1_coef19(p_comp->MMR_coef[0][19]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_20_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_20_mmr_coef_cmp1_coef20(p_comp->MMR_coef[0][20]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_21_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp1_Coef_21_mmr_coef_cmp1_coef21(p_comp->MMR_coef[0][21]));



	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_0_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_0_mmr_coef_cmp2_coef0(p_comp->MMR_coef[1][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_1_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_1_mmr_coef_cmp2_coef1(p_comp->MMR_coef[1][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_2_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_2_mmr_coef_cmp2_coef2(p_comp->MMR_coef[1][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_3_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_3_mmr_coef_cmp2_coef3(p_comp->MMR_coef[1][3]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_4_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_4_mmr_coef_cmp2_coef4(p_comp->MMR_coef[1][4]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_5_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_5_mmr_coef_cmp2_coef5(p_comp->MMR_coef[1][5]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_6_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_6_mmr_coef_cmp2_coef6(p_comp->MMR_coef[1][6]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_7_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_7_mmr_coef_cmp2_coef7(p_comp->MMR_coef[1][7]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_8_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_8_mmr_coef_cmp2_coef8(p_comp->MMR_coef[1][8]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_9_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_9_mmr_coef_cmp2_coef9(p_comp->MMR_coef[1][9]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_10_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_10_mmr_coef_cmp2_coef10(p_comp->MMR_coef[1][10]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_11_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_11_mmr_coef_cmp2_coef11(p_comp->MMR_coef[1][11]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_12_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_12_mmr_coef_cmp2_coef12(p_comp->MMR_coef[1][12]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_13_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_13_mmr_coef_cmp2_coef13(p_comp->MMR_coef[1][13]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_14_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_14_mmr_coef_cmp2_coef14(p_comp->MMR_coef[1][14]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_15_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_15_mmr_coef_cmp2_coef15(p_comp->MMR_coef[1][15]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_16_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_16_mmr_coef_cmp2_coef16(p_comp->MMR_coef[1][16]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_17_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_17_mmr_coef_cmp2_coef17(p_comp->MMR_coef[1][17]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_18_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_18_mmr_coef_cmp2_coef18(p_comp->MMR_coef[1][18]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_19_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_19_mmr_coef_cmp2_coef19(p_comp->MMR_coef[1][19]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_20_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_20_mmr_coef_cmp2_coef20(p_comp->MMR_coef[1][20]));

	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_21_reg,
					DHDR_V_COMPOSER_Composer_BL_Pred_MMR_Cmp2_Coef_21_mmr_coef_cmp2_coef21(p_comp->MMR_coef[1][21]));


/*20180315 pinyen remove unused DV register
	rtd_outl(DHDR_V_COMPOSER_Composer_BL_Up_reg,
					DHDR_V_COMPOSER_Composer_BL_Up_spatial_resampling_filter_flag(p_comp->spatial_resampling_filter_flag));


	// EL
	rtd_outl(DHDR_V_COMPOSER_Composer_EL_RS_reg,
					DHDR_V_COMPOSER_Composer_EL_RS_el_spatial_resampling_filter_flag(p_comp->el_spatial_resampling_filter_flag));

	rtd_outl(DHDR_V_COMPOSER_Composer_EL_IQ_reg,
					DHDR_V_COMPOSER_Composer_EL_IQ_disable_residual_flag(p_comp->disable_residual_flag) |
					DHDR_V_COMPOSER_Composer_EL_IQ_nlq_method_idc(p_comp->NLQ_method_idc));
*/
	// disable EL for testing
/*
	if (rtd_inl(0xb800501c) & _BIT4) {
		rtd_outl(DHDR_V_COMPOSER_Composer_EL_IQ_reg,
						DHDR_V_COMPOSER_Composer_EL_IQ_disable_residual_flag(1) |
						DHDR_V_COMPOSER_Composer_EL_IQ_nlq_method_idc(p_comp->NLQ_method_idc));
	}
*/
/*20180315 pinyen remove unused DV register
	rtd_outl(DHDR_V_COMPOSER_Composer_EL_IQ_offset_01_reg,
					DHDR_V_COMPOSER_Composer_EL_IQ_offset_01_nlq_offset_cmp1(p_comp->NLQ_offset[1]) |
					DHDR_V_COMPOSER_Composer_EL_IQ_offset_01_nlq_offset_cmp0(p_comp->NLQ_offset[0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_EL_IQ_offset_2_reg,
					DHDR_V_COMPOSER_Composer_EL_IQ_offset_2_nlq_offset_cmp2(p_comp->NLQ_offset[2]));


	rtd_outl(DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Slop_0_reg,
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Slop_0_linear_deadzone_slope_cmp0(p_comp->NLQ_coeff[0][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Slop_1_reg,
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Slop_1_linear_deadzone_slope_cmp1(p_comp->NLQ_coeff[1][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Slop_2_reg,
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Slop_2_linear_deadzone_slope_cmp2(p_comp->NLQ_coeff[2][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Th_0_reg,
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Th_0_linear_deadzone_threshold_cmp0(p_comp->NLQ_coeff[0][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Th_1_reg,
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Th_1_linear_deadzone_threshold_cmp1(p_comp->NLQ_coeff[1][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Th_2_reg,
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Th_2_linear_deadzone_threshold_cmp2(p_comp->NLQ_coeff[2][2]));



	rtd_outl(DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Max_0_reg,
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Max_0_vdr_in_max_cmp0(p_comp->NLQ_coeff[0][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Max_1_reg,
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Max_1_vdr_in_max_cmp1(p_comp->NLQ_coeff[1][1]));

	rtd_outl(DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Max_2_reg,
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_Max_2_vdr_in_max_cmp2(p_comp->NLQ_coeff[2][1]));


	rtd_outl(DHDR_V_COMPOSER_Composer_EL_IQ_Coef_INT_Slop_reg,
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_INT_Slop_linear_deadzone_slope_int_cmp2(p_comp->NLQ_coeff_int[2][0]) |
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_INT_Slop_linear_deadzone_slope_int_cmp1(p_comp->NLQ_coeff_int[1][0]) |
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_INT_Slop_linear_deadzone_slope_int_cmp0(p_comp->NLQ_coeff_int[0][0]));

	rtd_outl(DHDR_V_COMPOSER_Composer_EL_IQ_Coef_INT_Th_reg,
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_INT_Th_linear_deadzone_threshold_int_cmp2(p_comp->NLQ_coeff_int[2][2]) |
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_INT_Th_linear_deadzone_threshold_int_cmp1(p_comp->NLQ_coeff_int[1][2]) |
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_INT_Th_linear_deadzone_threshold_int_cmp0(p_comp->NLQ_coeff_int[0][2]));

	rtd_outl(DHDR_V_COMPOSER_Composer_EL_IQ_Coef_INT_Max_reg,
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_INT_Max_vdr_in_max_int_cmp2(p_comp->NLQ_coeff_int[2][1]) |
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_INT_Max_vdr_in_max_int_cmp1(p_comp->NLQ_coeff_int[1][1]) |
					DHDR_V_COMPOSER_Composer_EL_IQ_Coef_INT_Max_vdr_in_max_int_cmp0(p_comp->NLQ_coeff_int[0][1]));
*/

	// apply
	//comp_db_ctr_reg.composer_db_apply = 1;
	//rtd_outl(DHDR_V_COMPOSER_Composer_db_ctrl_reg, comp_db_ctr_reg.regValue);

}


void DolbyVTopRbusSet(DOLBYVISION_PATTERN *dolby, E_DV_MODE mode)
{
	hdr_all_top_top_ctl_RBUS hdr_all_top_top_ctl_reg;
	hdr_all_top_top_d_buf_RBUS hdr_all_top_top_d_buf_reg;
	hdr_all_top_top_pic_size_RBUS hdr_all_top_top_pic_size_reg;
	//hdr_all_top_top_pic_sta_RBUS hdr_all_top_top_pic_sta_reg;
	//hdr_all_top_top_pic_total_RBUS hdr_all_top_top_pic_total_reg;
	int v_den_sta, h_den_sta;

	hdr_all_top_top_d_buf_reg.regValue = 0;
	hdr_all_top_top_d_buf_reg.dolby_double_en = 1;
	hdr_all_top_top_d_buf_reg.dolby_double_apply = 0;
	rtd_outl(HDR_ALL_TOP_TOP_D_BUF_reg, hdr_all_top_top_d_buf_reg.regValue);

	hdr_all_top_top_ctl_reg.regValue = rtd_inl(HDR_ALL_TOP_TOP_CTL_reg);
	if (mode == DV_HDMI_MODE) {
		hdr_all_top_top_ctl_reg.dolby_mode = 1; 	// hdmi in
	} else if (mode == DV_NORMAL_MODE) {
		hdr_all_top_top_ctl_reg.dolby_mode = 2; 	// normal mode
		/*20180315 pinyen remove unused DV register
		if (dolby->BL_EL_ratio == 1)
			top_ctl_reg.dolby_ratio = 1;	// BL:EL = 4:1
		else
			top_ctl_reg.dolby_ratio = 0;	// BL:EL = 1:1
		*/
	} else if (mode == DV_COMPOSER_CRF_MODE) {
		hdr_all_top_top_ctl_reg.dolby_mode = 6; 	// composer crf mode
		/*20180315 pinyen remove unused DV register
		if (dolby->BL_EL_ratio == 1)
			top_ctl_reg.dolby_ratio = 1;	// BL:EL = 4:1
		else
			top_ctl_reg.dolby_ratio = 0;	// BL:EL = 1:1
		*/
	} else if (mode == DV_DM_CRF_420_MODE) {
		hdr_all_top_top_ctl_reg.dolby_mode = 5; 	// DM crf 420 mode
		//top_ctl_reg.dolby_ratio = 0; 20180315 pinyen remove unused DV register
	} else if (mode == DV_DM_CRF_422_MODE) {
		hdr_all_top_top_ctl_reg.dolby_mode = 4; 	// DM crf 422 mode
		//top_ctl_reg.dolby_ratio = 0; 20180315 pinyen remove unused DV register
	}
	//baker for mac5
	hdr_all_top_top_ctl_reg.en_422to444_1 = 0;
	hdr_all_top_top_ctl_reg.end_out_mux = 0;

	//top_ctl_reg.dolby_v_read_sel = 1;   lm@20161221, no dolby_v_read_sel in mac5 v-top spec

	rtd_outl(HDR_ALL_TOP_TOP_CTL_reg, hdr_all_top_top_ctl_reg.regValue);

       /* 20171228,pinyen remove unused V_top reg, the same reg has been updated in videoFW
	rtd_outl(DOLBY_V_TOP_TOP_PIC_SIZE_reg, DOLBY_V_TOP_TOP_PIC_SIZE_dolby_vsize(dolby->RowNumTtl) |
		DOLBY_V_TOP_TOP_PIC_SIZE_dolby_hsize(dolby->ColNumTtl));
       */


	// notice: vodma's h_total increase ONE, follow DV setting
	/* 20171228,pinyen remove unused V_top reg, the same reg has been updated in videoFW
	rtd_outl(DOLBY_V_TOP_TOP_PIC_TOTAL_reg, VODMA_VODMA_V1SGEN_get_h_thr(rtd_inl(VODMA_VODMA_V1SGEN_reg))+1);
	*/


	v_den_sta = VODMA_VODMA_V1VGIP_VACT1_get_v_st(rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg));
	h_den_sta = VODMA_VODMA_V1VGIP_HACT_get_h_st(rtd_inl(VODMA_VODMA_V1VGIP_HACT_reg));

	//h_den_sta += 3;

#ifdef IDK_CRF_USE
       /* 20171228,pinyen remove unused V_top reg, the same reg has been updated in videoFW
	rtd_outl(DOLBY_V_TOP_TOP_PIC_STA_reg, DOLBY_V_TOP_TOP_PIC_STA_dolby_v_den_sta(v_den_sta-3) |	// for IDK test
	*/
#else
	#if (defined(CONFIG_SUPPORT_SCALER))
	//if (get_OTT_HDR_mode()==HDR_DOLBY_COMPOSER && (get_HDMI_HDR_mode()!=HDR_DOLBY_HDMI))
	//	v_den_sta -= 4;	// for BL shift up issue
	if (get_HDMI_HDR_mode()==HDR_DOLBY_HDMI)
		h_den_sta = VODMA_VODMA_V1VGIP_HACT_get_h_st(rtd_inl(VODMA_VODMA_V1VGIP_HACT_reg));// + 3;
	#endif
	/* 20171228,pinyen remove unused V_top reg, the same reg has been updated in videoFW
	rtd_outl(DOLBY_V_TOP_TOP_PIC_STA_reg, DOLBY_V_TOP_TOP_PIC_STA_dolby_v_den_sta(v_den_sta) |
	*/
#endif
       /* 20171228,pinyen remove unused V_top reg, the same reg has been updated in videoFW
		DOLBY_V_TOP_TOP_PIC_STA_dolby_h_den_sta(h_den_sta));
	*/


#ifdef CONFIG_SUPPORT_SCALER
	if (get_HDMI_HDR_mode()==HDR_DOLBY_HDMI) {
		// to fix UV inverse issue for HDMI path
		hdr_all_top_top_pic_size_reg.dolby_hsize = dolby->ColNumTtl;
		hdr_all_top_top_pic_size_reg.dolby_vsize = dolby->RowNumTtl;
		//hdr_all_top_top_pic_total_reg.dolby_h_total = VODMA_VODMA_V1SGEN_get_h_thr(rtd_inl(VODMA_VODMA_V1SGEN_reg))+1;
		//hdr_all_top_top_pic_sta_reg.dolby_h_den_sta = h_den_sta;
		//[K3LG-203]video will have vertical blue line for DOLBY HDMI UHD Test
		//top_pic_size_reg.dolby_hsize = ((top_pic_total_reg.dolby_h_total + 2 - top_pic_sta_reg.dolby_h_den_sta)>=0xfff) ?
		//										0xffe : (top_pic_total_reg.dolby_h_total + 2 - top_pic_sta_reg.dolby_h_den_sta);
		rtd_outl(HDR_ALL_TOP_TOP_PIC_SIZE_reg, hdr_all_top_top_pic_size_reg.regValue);
	}
#endif

	// notice: dolby_v_top's v_sta follows sensio timing
	//v_den_sta = SENSIO_SENSIO_TIM_CTRL_1_get_v_den_st(rtd_inl(SENSIO_SENSIO_TIM_CTRL_1_reg));
	//rtd_outl(DOLBY_V_TOP_TOP_PIC_STA_reg, DOLBY_V_TOP_TOP_PIC_STA_dolby_v_den_sta(v_den_sta) |
	//												DOLBY_V_TOP_TOP_PIC_STA_dolby_h_den_sta(h_den_sta));

	// apply
	//hdr_all_top_top_d_buf_reg.dolby_double_apply = 1;
	//rtd_outl(HDR_ALL_TOP_TOP_D_BUF_reg, hdr_all_top_top_d_buf_reg.regValue);
	//while (DOLBY_V_TOP_TOP_D_BUF_get_dolby_double_apply(rtd_inl(DOLBY_V_TOP_TOP_D_BUF_reg))) ;
}


void Normal_In_TestFlow(DOLBYVISION_PATTERN *dolby)
{
#if 0//#ifdef CONFIG_CUSTOMER_TV006//Remove by will

	int idx = 0, mdidx = 0;
	unsigned long roffset = 0;
	char tmp;
	int ret;
	MdsChg_t mdsChg;
	unsigned char *ddrptr, *mdptr;
	dm_hdr_dm_double_buffer_ctrl_RBUS dm_double_buf_ctrl_reg;
	dhdr_v_composer_composer_db_ctrl_RBUS comp_db_ctr_reg;
	dolby_v_top_top_d_buf_RBUS top_d_buf_reg;
	rpu_ext_config_fixpt_main_t *p_CompMd;
	static unsigned int iv_act_sta = 0;
	unsigned int el_width, el_height;
	//DOLBYVISION_INIT_STRUCT dv_init;

	dm_cfg.frmPri.PxlDefOut.pxlClr = 0;
	dm_cfg.frmPri.PxlDefOut.pxlChrm = 1;
	dm_cfg.frmPri.PxlDefOut.pxlDtp = 0;
	dm_cfg.frmPri.PxlDefOut.pxlBdp = dolby->pxlBdp;
	dm_cfg.frmPri.PxlDefOut.pxlWeav = 2;
	dm_cfg.frmPri.PxlDefOut.pxlLoc = 0;
	dm_cfg.frmPri.PxlDefOut.pxlEotf = 0;
	dm_cfg.frmPri.PxlDefOut.pxlRng = 1;
	dm_cfg.frmPri.RowNumTtl = dolby->RowNumTtl;
	dm_cfg.frmPri.ColNumTtl = dolby->ColNumTtl;
	h_dm->dmExec.runMode = dm_cfg.dmCtrl.RunMode = RUN_MODE_NORMAL;

	SanityCheckDmCfg(&dm_cfg);


	if (gFrame1Infp == NULL)
		gFrame1Infp = file_open(dolby->file1Inpath, O_RDONLY, 0);
	if (gFrame1Infp == NULL) {
		printk("%s, gFrame1Infp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame2Infp = gFrame1Outfp = gFrame2Outfp = gMDInfp = NULL;
		return ;
	}
	if (gFrame2Infp == NULL)
		gFrame2Infp = file_open(dolby->file2Inpath, O_RDONLY, 0);
	if (gFrame2Infp == NULL) {
		printk("%s, gFrame2Infp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame2Infp = gFrame1Outfp = gFrame2Outfp = gMDInfp = NULL;
		return ;
	}
	if (gFrame1Outfp == NULL)
		gFrame1Outfp = file_open("/var/Yblock.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
	if (gFrame1Outfp == NULL) {
		printk("%s, gFrame1Outfp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame2Infp = gFrame1Outfp = gFrame2Outfp = gMDInfp = NULL;
		return ;
	}
	if (gFrame2Outfp == NULL)
		gFrame2Outfp = file_open("/var/Cblock.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
	if (gFrame2Outfp == NULL) {
		printk("%s, gFrame2Outfp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame2Infp = gFrame1Outfp = gFrame2Outfp = gMDInfp = NULL;
		return ;
	}
	if (gMDInfp == NULL)
		gMDInfp = file_open(dolby->fileMdInpath, O_RDONLY, 0);
	if (gMDInfp == NULL) {
		printk("%s, gMDInfp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame2Infp = gFrame1Outfp = gFrame2Outfp = gMDInfp = NULL;
		return ;
	}

	if (gVoFrameAddr1 == 0)
		gVoFrameAddr1 = (unsigned int)dvr_malloc_specific((3840*2160*2), GFP_DCU1);
	if (gVoFrameAddr2 == 0)
		gVoFrameAddr2 = (unsigned int)dvr_malloc_specific((3840*2160*2), GFP_DCU1);
	if (gVoFrameAddr3 == 0)
		gVoFrameAddr3 = (unsigned int)dvr_malloc_specific((3840*2160*2), GFP_DCU1);
	if (gVoFrameAddr4 == 0)
		gVoFrameAddr4 = (unsigned int)dvr_malloc_specific((3840*2160*2), GFP_DCU1);
	printk(KERN_INFO"physical gVoFrameAddr1=0x%x, virtual gVoFrameAddr1=0x%x \n", (unsigned int)dvr_to_phys((void *)gVoFrameAddr1), gVoFrameAddr1);
	printk(KERN_INFO"physical gVoFrameAddr2=0x%x, virtual gVoFrameAddr2=0x%x \n", (unsigned int)dvr_to_phys((void *)gVoFrameAddr2), gVoFrameAddr2);
	printk(KERN_INFO"physical gVoFrameAddr3=0x%x, virtual gVoFrameAddr3=0x%x \n", (unsigned int)dvr_to_phys((void *)gVoFrameAddr3), gVoFrameAddr3);
	printk(KERN_INFO"physical gVoFrameAddr4=0x%x, virtual gVoFrameAddr4=0x%x \n", (unsigned int)dvr_to_phys((void *)gVoFrameAddr4), gVoFrameAddr4);

	if (gRpuMDAddr == 0)
		gRpuMDAddr = (unsigned int)dvr_malloc_specific(RPU_MD_SIZE, GFP_DCU1);
	printk(KERN_INFO"gRpuMDAddr=0x%x\n", (unsigned int)dvr_to_phys((void *)gRpuMDAddr));


	if (g_dmMDAddr == 0) {
		g_dmMDAddr = (unsigned int)dvr_malloc_specific(RPU_MD_SIZE, GFP_DCU2);
		if (g_dmMDAddr)
			DBG_PRINT(KERN_INFO"physical g_dmMDAddr=0x%x\n", (unsigned int)dvr_to_phys((void *)g_dmMDAddr));
	}
	if (g_compMDAddr == 0) {
		g_compMDAddr = (unsigned int)dvr_malloc_specific(RPU_MD_SIZE, GFP_DCU2);
		if (g_compMDAddr)
			DBG_PRINT(KERN_EMERG"physical g_compMDAddr=0x%x\n", (unsigned int)dvr_to_phys((void *)g_compMDAddr));
	}

	/* store MD data to DDR */
	ddrptr = (unsigned char *)gRpuMDAddr;
	for (idx = 0; idx < dolby->fileMdInSize; idx++) {
		file_read((unsigned char *)&tmp, 1, idx, gMDInfp);
		*ddrptr = tmp;
		ddrptr++;
	}

	// check EL size
	if (dolby->BL_EL_ratio) {	// 4:1
		el_width = dolby->ColNumTtl / 2;
		el_height = dolby->RowNumTtl / 2;
	} else {
		el_width = dolby->ColNumTtl;
		el_height = dolby->RowNumTtl;
	}



	if (iv_act_sta == 0) {
		// sensio timing setting
		rtd_outl(SENSIO_Sensio_ctrl_2_reg, (dolby->RowNumTtl<<16) | (dolby->ColNumTtl-1));
		rtd_outl(SENSIO_sub_vp9_decomp_ctrl_1_reg, (el_height<<16) | (el_width-1));

		rtd_outl(SENSIO_SENSIO_TIM_CTRL_0_reg, (5<<16) |
				VODMA_VODMA_V1SGEN_get_h_thr(rtd_inl(VODMA_VODMA_V1SGEN_reg)));
		rtd_outl(SENSIO_SENSIO_TIM_CTRL_1_reg, (VODMA_VODMA_V1VGIP_VACT1_get_v_st(rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg))-5)<<16 |
					(VODMA_VODMA_V1VGIP_HACT_get_h_st(rtd_inl(VODMA_VODMA_V1VGIP_HACT_reg))+5));
		rtd_outl(SENSIO_SUB_SENSIO_TIM_CTRL_0_reg, (5<<16) |
				VODMA_VODMA_V1SGEN_get_h_thr(rtd_inl(VODMA2_VODMA2_V1SGEN_reg)));
		rtd_outl(SENSIO_SUB_SENSIO_TIM_CTRL_1_reg, (VODMA2_VODMA2_V1VGIP_VACT1_get_v_st(rtd_inl(VODMA2_VODMA2_V1VGIP_VACT1_reg))-5)<<16 |
					(VODMA2_VODMA2_V1VGIP_HACT_get_h_st(rtd_inl(VODMA2_VODMA2_V1VGIP_HACT_reg))+5));
		rtd_outl(SENSIO_Sensio_ctrl_0_reg, 0x00000060);
		rtd_outl(SENSIO_SUB_vp9_decomp_ctrl_0_reg, 0x00000000);
		rtd_outl(SENSIO_Sensio_ctrl_3_reg, 0x00070000);		// apply
		msleep(40);


		rtd_outl(VODMA_VODMA_VGIP_INTPOS_reg, 0x00);	// disable interrupt
		rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, 0x00);	// disable interrupt
		// disable VODMA1
		//rtd_outl(VODMA_VODMA_V1_DCFG_reg, (rtd_inl(VODMA_VODMA_V1_DCFG_reg)&~1) | (1<<3));
		// disable VODMA2
		//rtd_outl(VODMA2_VODMA2_V1_DCFG_reg, (rtd_inl(VODMA2_VODMA2_V1_DCFG_reg)&~1) | (1<<3));
		//rtd_outl(VODMA_VODMA_V1SGEN_reg, rtd_inl(VODMA_VODMA_V1SGEN_reg) | (1<<30));	// vo reset high
		//rtd_outl(VODMA2_VODMA2_V1SGEN_reg, rtd_inl(VODMA2_VODMA2_V1SGEN_reg) | (1<<30));	// vo reset high
		//printk("%s, vodma reset....\n", __FUNCTION__);
	}



	// test 5 frame
	for (idx = 0; idx < 1; idx++) {

		g_frameCtrlNo = dolby->frameNo;

		// TODO: rpu data  ->  composer & DM metadata
		ddrptr = (unsigned char *)gRpuMDAddr;
		g_MD_parser_state = MD_PARSER_INIT;
		g_mdparser_output_type = 1;
		g_dvframeCnt = g_recordDMAddr = 0;	// output all metadata
		for (mdidx = 0; mdidx <= g_frameCtrlNo; mdidx++) {
			//g_dvframeCnt = g_recordDMAddr = 0;	// output one metadata each frame in starting address
			roffset = metadata_parser_main(ddrptr, dolby->fileMdInSize, g_MD_parser_state);
			g_MD_parser_state = MD_PARSER_RUN;
			dolby->fileMdInSize -= roffset;
			ddrptr += roffset;

			if (roffset == 0 && g_more_eos_err && mdidx==g_frameCtrlNo) {
				g_MD_parser_state = MD_PARSER_EOS;
				roffset = metadata_parser_main(ddrptr, dolby->fileMdInSize, g_MD_parser_state);
				dolby->fileMdInSize -= roffset;
			}
			DV_Flush();
		}


		// TODO:  Composer meta data parsing to software structure
		p_CompMd = (rpu_ext_config_fixpt_main_t *)g_compMDAddr;
		//p_CompMd = g_compMDAddr + (sizeof(rpu_ext_config_fixpt_main_t)*g_frameCtrlNo);


		gVoFrame1AddrAssign = gVoFrameAddr1;
		gVoFrame2AddrAssign = gVoFrameAddr2;
		if (p_CompMd->rpu_BL_bit_depth == 8)
			Fmt_Cvt_8YUV420_to_8BLK444(gFrame1Infp, gFrame1Outfp, gFrame2Outfp, dolby->ColNumTtl, dolby->RowNumTtl);
		else if (p_CompMd->rpu_BL_bit_depth == 10)
			Fmt_Cvt_10YUV420_to_10BLK444(gFrame1Infp, gFrame1Outfp, gFrame2Outfp, dolby->ColNumTtl, dolby->RowNumTtl);
		else {
			printk("unknown BL bit type \n");
			gFrame1Infp = gFrame2Infp = gFrame1Outfp = gFrame2Outfp = gMDInfp = NULL;
			return ;
		}
		gVoFrame1AddrAssign = gVoFrameAddr3;
		gVoFrame2AddrAssign = gVoFrameAddr4;
		if (p_CompMd->rpu_EL_bit_depth == 8)
			Fmt_Cvt_8YUV420_to_8BLK444(gFrame2Infp, gFrame1Outfp, gFrame2Outfp, el_width, el_height);
		else if (p_CompMd->rpu_EL_bit_depth == 10)
			Fmt_Cvt_10YUV420_to_10BLK444(gFrame2Infp, gFrame1Outfp, gFrame2Outfp, el_width, el_height);
		else {
			printk("unknown EL bit type \n");
			gFrame1Infp = gFrame2Infp = gFrame1Outfp = gFrame2Outfp = gMDInfp = NULL;
			return ;
		}



		// TODO:  DM meta data parsing to software structure
		mdptr = (char *)g_dmMDAddr;
		//for (mdidx = 0; mdidx <= g_frameCtrlNo; mdidx++) {
			ret = dm_metadata_2_dm_param((dm_metadata_base_t *)mdptr, &mds_ext);
			//mdptr += ret;
		//}
		mdsChg = CheckMds(&mds_ext, &h_dm->mdsExt);	// modified by smfan
		CommitMds(&mds_ext, h_dm);


		// TODO: dolby_v_top setting
		DolbyVTopRbusSet(dolby, DV_NORMAL_MODE);

		top_d_buf_reg.regValue = rtd_inl(DOLBY_V_TOP_TOP_D_BUF_reg);
		top_d_buf_reg.dolby_double_apply = 1;
		rtd_outl(DOLBY_V_TOP_TOP_D_BUF_reg, top_d_buf_reg.regValue);
		//while (DOLBY_V_TOP_TOP_D_BUF_get_dolby_double_apply(rtd_inl(DOLBY_V_TOP_TOP_D_BUF_reg))) {
		//	msleep(1);
		//}

		// TODO:  DM setting
		DMRbusSet(DV_NORMAL_MODE, (mdsChg==MDS_CHG_NONE)?0:1);

		// TODO:  Composer setting
		ComposerRbusSet(p_CompMd);


		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
		dm_double_buf_ctrl_reg.dm_db_apply = 1;
		comp_db_ctr_reg.regValue = rtd_inl(DHDR_V_COMPOSER_Composer_db_ctrl_reg);
		comp_db_ctr_reg.composer_db_apply = 1;

		rtd_outl(DHDR_V_COMPOSER_Composer_db_ctrl_reg, comp_db_ctr_reg.regValue);
		//msleep(40);
		rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
		//msleep(40);



		// set some vo register
		// VP_DC_IDX_VOUT_V2_Y_CURR = 0x78
		// VP_DC_IDX_VOUT_V2_C_CURR = 0x79
		// VP_DC_IDX_VOUT_V2_Y_CURR2 = 0x7a
		// VP_DC_IDX_VOUT_V2_C_CURR2 = 0x7b
		if (p_CompMd->rpu_BL_bit_depth == 8) {
#ifdef CONFIG_SUPPORT_SCALER
			vodma_DCU_Set(VP_DC_IDX_VOUT_V2_Y_CURR, dvr_to_phys((void *)gVoFrameAddr1), dolby->ColNumTtl, 0, 0);
			vodma_DCU_Set(VP_DC_IDX_VOUT_V2_C_CURR, dvr_to_phys((void *)gVoFrameAddr2), dolby->ColNumTtl, 0, 0);
#endif
		} else {
#ifdef CONFIG_SUPPORT_SCALER
			vodma_DCU_Set(VP_DC_IDX_VOUT_V2_Y_CURR, dvr_to_phys((void *)gVoFrameAddr1), ((dolby->ColNumTtl*10)/8), 0, 0);
			vodma_DCU_Set(VP_DC_IDX_VOUT_V2_C_CURR, dvr_to_phys((void *)gVoFrameAddr2), ((dolby->ColNumTtl*10)/8), 0, 0);
#endif
		}
		if (p_CompMd->rpu_EL_bit_depth == 8) {
#ifdef CONFIG_SUPPORT_SCALER
			vodma_DCU_Set(VP_DC_IDX_VOUT_V2_Y_CURR2, dvr_to_phys((void *)gVoFrameAddr3), el_width, 0, 0);
			vodma_DCU_Set(VP_DC_IDX_VOUT_V2_C_CURR2, dvr_to_phys((void *)gVoFrameAddr4), el_width, 0, 0);
#endif
		} else {
#ifdef CONFIG_SUPPORT_SCALER
			vodma_DCU_Set(VP_DC_IDX_VOUT_V2_Y_CURR2, dvr_to_phys((void *)gVoFrameAddr3), ((el_width*10)/8), 0, 0);
			vodma_DCU_Set(VP_DC_IDX_VOUT_V2_C_CURR2, dvr_to_phys((void *)gVoFrameAddr4), ((el_width*10)/8), 0, 0);
#endif
		}



#if 1
		//rtd_outl(VODMA_VODMA_V1SGEN_reg, rtd_inl(VODMA_VODMA_V1SGEN_reg) & ~(1<<30));	// release vo reset
		//rtd_outl(VODMA2_VODMA2_V1SGEN_reg, rtd_inl(VODMA2_VODMA2_V1SGEN_reg) & ~(1<<30));	// release vo reset

		// vo bg setting
		rtd_outl(VODMA_VODMA_V1VGIP_HBG_reg, rtd_inl(VODMA_VODMA_V1VGIP_HACT_reg));
		rtd_outl(VODMA2_VODMA2_V1VGIP_HBG_reg, rtd_inl(VODMA2_VODMA2_V1VGIP_HACT_reg));

		rtd_outl(VODMA_VODMA_V1VGIP_VBG1_reg, rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg));
		rtd_outl(VODMA2_VODMA2_V1VGIP_VBG1_reg, rtd_inl(VODMA2_VODMA2_V1VGIP_VACT1_reg));
		rtd_outl(VODMA_VODMA_V1VGIP_VBG2_reg, rtd_inl(VODMA_VODMA_V1VGIP_VBG1_reg));
		rtd_outl(VODMA2_VODMA2_V1VGIP_VBG2_reg, rtd_inl(VODMA2_VODMA2_V1VGIP_VBG1_reg));

		// passive mode  for VO2
		rtd_outl(VODMA2_VODMA2_PVS0_Gen_reg, rtd_inl(VODMA2_VODMA2_PVS0_Gen_reg) |
						VODMA2_VODMA2_PVS0_Gen_iv_src_sel(7));
		rtd_outl(VODMA2_VODMA2_dma_option_reg, rtd_inl(VODMA2_VODMA2_dma_option_reg) |
						VODMA2_VODMA2_dma_option_vo_out_passivemode(1));


		if (iv_act_sta == 0) {

			// chanage vgip v_sta once
			iv_act_sta = VGIP_VGIP_CHN1_ACT_VSTA_Length_get_ch1_iv_act_sta(rtd_inl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg));
			rtd_outl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg, (rtd_inl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg) & ~VGIP_VGIP_CHN1_ACT_VSTA_Length_ch1_iv_act_sta_mask) |
						VGIP_VGIP_CHN1_ACT_VSTA_Length_ch1_iv_act_sta(iv_act_sta-6));
		}

		rtd_outl(VODMA_VODMA_VGIP_INTPOS_reg, 0x00);
		rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, 0x00);
		rtd_outl(VODMA_VODMA_V1_DSIZE_reg, (dolby->ColNumTtl<<16) | (dolby->RowNumTtl));
		rtd_outl(VODMA_VODMA_VD1_ADS_reg, (VP_DC_IDX_VOUT_V2_C_CURR<<16) | (VP_DC_IDX_VOUT_V2_C_CURR<<8) | VP_DC_IDX_VOUT_V2_Y_CURR);

		rtd_outl(VODMA_VODMA_dma_option_reg, rtd_inl(VODMA_VODMA_dma_option_reg) &
														~(VODMA_VODMA_dma_option_blockmode_sel_c_mask|VODMA_VODMA_dma_option_blockmode_sel_y_mask));
		if (p_CompMd->rpu_BL_bit_depth == 10)
			rtd_outl(VODMA_VODMA_dma_option_reg, rtd_inl(VODMA_VODMA_dma_option_reg) |
							VODMA_VODMA_dma_option_blockmode_sel_c(3) |
							VODMA_VODMA_dma_option_blockmode_sel_y(3));
		rtd_outl(VODMA_VODMA_V1_DCFG_reg, 0x00180f4b);

		rtd_outl(VODMA2_VODMA2_V1_DSIZE_reg, (el_width<<16) | (el_height));
		rtd_outl(VODMA2_VODMA2_VD1_ADS_reg, (VP_DC_IDX_VOUT_V2_C_CURR2<<16) | (VP_DC_IDX_VOUT_V2_C_CURR2<<8) | VP_DC_IDX_VOUT_V2_Y_CURR2);

		rtd_outl(VODMA2_VODMA2_dma_option_reg, rtd_inl(VODMA2_VODMA2_dma_option_reg) &
														~(VODMA2_VODMA2_dma_option_blockmode_sel_c_mask|VODMA2_VODMA2_dma_option_blockmode_sel_y_mask));
		if (p_CompMd->rpu_EL_bit_depth == 10)
			rtd_outl(VODMA2_VODMA2_dma_option_reg, rtd_inl(VODMA2_VODMA2_dma_option_reg) |
							VODMA2_VODMA2_dma_option_blockmode_sel_c(3) |
							VODMA2_VODMA2_dma_option_blockmode_sel_y(3));


		// for VODMA2 underflow, which cause the output image right side and left side swap..
		if (VODMA2_VODMA2_CLKGEN_get_vodma_clk_div_n(rtd_inl(VODMA2_VODMA2_CLKGEN_reg)) > 1)
			rtd_outl(VODMA2_VODMA2_CLKGEN_reg, (rtd_inl(VODMA2_VODMA2_CLKGEN_reg) & ~VODMA2_VODMA2_CLKGEN_vodma_clk_div_n_mask) |
							VODMA2_VODMA2_CLKGEN_vodma_clk_div_n(1));


		rtd_outl(VODMA2_VODMA2_V1_DCFG_reg, 0x00180f4b);
#endif

	}

#if 0
	// disable double buffer
	msleep(40);
	dm_double_buf_ctrl_reg.dm_db_en = 0;
	comp_db_ctr_reg.composer_db_en = 0;
	top_d_buf_reg.dolby_double_en = 0;
	rtd_outl(DOLBY_V_TOP_TOP_D_BUF_reg, top_d_buf_reg.regValue);
	rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
	rtd_outl(DHDR_V_COMPOSER_Composer_db_ctrl_reg, comp_db_ctr_reg.regValue);
	msleep(40);
#endif
#if 0
		// write md to file
		if (gFrame1Outfp != NULL) {
			file_sync(gFrame1Outfp);
			file_close(gFrame1Outfp);
			gFrame1Outfp = NULL;
		}
		gFrame1Outfp = file_open("/var/Cblock_ddr.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
		ddrptr = gVoFrameAddr2;
		for (idx=0; idx<1179648; idx++) {
			file_write(ddrptr, 1, idx, gFrame1Outfp);
			ddrptr++;
		}
		if (gFrame1Outfp != NULL) {
			file_sync(gFrame1Outfp);
			file_close(gFrame1Outfp);
			gFrame1Outfp = NULL;
		}
		gFrame1Outfp = file_open("/var/Yblock_ddr.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
		ddrptr = gVoFrameAddr1;
		for (idx=0; idx<2228224; idx++) {
			file_write(ddrptr, 1, idx, gFrame1Outfp);
			ddrptr++;
		}
#endif


#if 1	// output rpu data  ->  real composer & DM metadata
	// write md to file
	if (gFrame1Outfp != NULL) {
		file_sync(gFrame1Outfp);
		file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}
	gFrame1Outfp = file_open("/var/1dm.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
	ddrptr = (unsigned char *)g_dmMDAddr;
	for (idx=0; idx<9840; idx++) {
		file_write(ddrptr, 1, idx, gFrame1Outfp);
		ddrptr++;
	}
	if (gFrame1Outfp != NULL) {
		file_sync(gFrame1Outfp);
		file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}
	gFrame1Outfp = file_open("/var/1comp.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
	ddrptr = (unsigned char *)g_compMDAddr;
	for (idx=0; idx<216000; idx++) {
		file_write(ddrptr, 1, idx, gFrame1Outfp);
		ddrptr++;
	}
#endif


	if (gFrame1Infp != NULL) {
	    file_sync(gFrame1Infp);
	    file_close(gFrame1Infp);
		gFrame1Infp = NULL;
	}
	if (gFrame2Infp != NULL) {
	    file_sync(gFrame2Infp);
	    file_close(gFrame2Infp);
		gFrame2Infp = NULL;
	}
	if (gFrame1Outfp != NULL) {
	    file_sync(gFrame1Outfp);
	    file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}
	if (gFrame2Outfp != NULL) {
	    file_sync(gFrame2Outfp);
	    file_close(gFrame2Outfp);
		gFrame2Outfp = NULL;
	}
	if (gMDInfp != NULL) {
	    file_sync(gMDInfp);
	    file_close(gMDInfp);
		gMDInfp = NULL;
	}
#endif

}



void HDMI_In_TestFlow(DOLBYVISION_PATTERN *dolby)
{
#if 0//#ifdef CONFIG_CUSTOMER_TV006//Remove by will
	int idx = 0, mdidx = 0;
	char tmp;
	int ret;
	MdsChg_t mdsChg;
	static unsigned int mdAddr = 0;
	unsigned char *ddrptr, *mdptr;
	dm_hdr_dm_double_buffer_ctrl_RBUS dm_double_buf_ctrl_reg;
	dolby_v_top_top_d_buf_RBUS top_d_buf_reg;
	static unsigned int voVsync_workaround = 0;


	dm_cfg.frmPri.PxlDefOut.pxlClr = 0;
	dm_cfg.frmPri.PxlDefOut.pxlChrm = 1;
	dm_cfg.frmPri.PxlDefOut.pxlDtp = 0;
	dm_cfg.frmPri.PxlDefOut.pxlBdp = dolby->pxlBdp;//14;
	dm_cfg.frmPri.PxlDefOut.pxlWeav = 2;
	dm_cfg.frmPri.PxlDefOut.pxlLoc = 0;
	dm_cfg.frmPri.PxlDefOut.pxlEotf = 0;
	dm_cfg.frmPri.PxlDefOut.pxlRng = 1;
	dm_cfg.frmPri.RowNumTtl = dolby->RowNumTtl;
	dm_cfg.frmPri.ColNumTtl = dolby->ColNumTtl;//3
	h_dm->dmExec.runMode = dm_cfg.dmCtrl.RunMode = RUN_MODE_FROMHDMI;

	SanityCheckDmCfg(&dm_cfg);


	if (gFrame1Infp == NULL)
		//gInfp = file_open("/usr/local/gstreamer/share/Teststream_scrambled_3840x2160_UYVY_12b.yuv", O_RDONLY, 0);
		gFrame1Infp = file_open(dolby->file1Inpath, O_RDONLY, 0);
	if (gFrame1Infp == NULL) {
		printk("%s, gInfp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame1Outfp = gMDInfp = NULL;
		return ;
	}
	if (gFrame1Outfp == NULL)
		//gOutfp = file_open("/var/Teststream_scrambled_3840x2160_UYVY_12b.yuv.vodmaSeq8bit.yuv", O_TRUNC | O_WRONLY | O_CREAT, 0);
		gFrame1Outfp = file_open(dolby->fileOutpath, O_TRUNC | O_WRONLY | O_CREAT, 0);
	if (gFrame1Outfp == NULL) {
		printk("%s, gOutfp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame1Outfp = gMDInfp = NULL;
		return ;
	}
	if (gMDInfp == NULL)
		gMDInfp = file_open(dolby->fileMdInpath, O_RDONLY, 0);
	if (gMDInfp == NULL) {
		printk("%s, gMDInfp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame1Outfp = gMDInfp = NULL;
		return ;
	}

	if (gVoFrameAddr1 == 0) {
		gVoFrameAddr1 = (unsigned int)dvr_malloc_specific((3840*2160*4), GFP_DCU1);
	}
	if (gVoFrameAddr2 == 0) {
		gVoFrameAddr2 = (unsigned int)dvr_malloc_specific((3840*2160*4), GFP_DCU1);
	}
	printk(KERN_EMERG"physical gVoFrameAddr1=0x%x, virtual gVoFrameAddr1=0x%x \n", (unsigned int)dvr_to_phys((void *)gVoFrameAddr1), gVoFrameAddr1);
	printk(KERN_EMERG"physical gVoFrameAddr2=0x%x, virtual gVoFrameAddr2=0x%x \n", (unsigned int)dvr_to_phys((void *)gVoFrameAddr2), gVoFrameAddr2);

	if (mdAddr == 0)
		mdAddr = (unsigned int)dvr_malloc_specific(0x100000, GFP_DCU2);
	printk(KERN_EMERG"mdAddr=0x%x\n", (unsigned int)dvr_to_phys((void *)mdAddr));

	/* store MD data to DDR */
	ddrptr = (unsigned char *)mdAddr;
	for (idx = 0; idx < dolby->fileMdInSize; idx++) {
		file_read((unsigned char *)&tmp, 1, idx, gMDInfp);
		*ddrptr = tmp;
		ddrptr++;
	}

	if (voVsync_workaround == 0) {

		rtd_outl(VODMA_VODMA_VGIP_INTPOS_reg, 0x00);	// disable interrupt
		rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, 0x00);	// disable interrupt
		// disable VODMA1
		//rtd_outl(VODMA_VODMA_V1_DCFG_reg, (rtd_inl(VODMA_VODMA_V1_DCFG_reg)&~1) | (1<<3));
		// disable VODMA2
		//rtd_outl(VODMA2_VODMA2_V1_DCFG_reg, (rtd_inl(VODMA2_VODMA2_V1_DCFG_reg)&~1) | (1<<3));
		msleep(40);
		//rtd_outl(VODMA_VODMA_V1SGEN_reg, rtd_inl(VODMA_VODMA_V1SGEN_reg) | (1<<30));	// vo reset high
		//rtd_outl(VODMA2_VODMA2_V1SGEN_reg, rtd_inl(VODMA2_VODMA2_V1SGEN_reg) | (1<<30));	// vo reset high
		//msleep(40);
		//printk("%s, vodma reset....\n", __FUNCTION__);
	}


	// test 5 frame
	for (idx = 0; idx < 1; idx++) {

		g_frameCtrlNo = dolby->frameNo;
		if ((g_frameCtrlNo%2)==0)
			gVoFrame1AddrAssign = gVoFrameAddr1;
		else
			gVoFrame1AddrAssign = gVoFrameAddr2;
		Fmt_Cvt_12YUV422_to_8SEQ444(gFrame1Infp, gFrame1Outfp, dolby->ColNumTtl, dolby->RowNumTtl);

		// TODO:  meta data parsing
		mdptr = (char *)mdAddr;
		for (mdidx = 0; mdidx <= g_frameCtrlNo; mdidx++) {
			ret = dm_metadata_2_dm_param((dm_metadata_base_t *)mdptr, &mds_ext);
			mdptr += ret;
		}
		mdsChg = CheckMds(&mds_ext, &h_dm->mdsExt);	// modified by smfan
		CommitMds(&mds_ext, h_dm);


		// TODO: dolby_v_top setting
		DolbyVTopRbusSet(dolby, DV_HDMI_MODE);
		top_d_buf_reg.regValue = rtd_inl(DOLBY_V_TOP_TOP_D_BUF_reg);
		top_d_buf_reg.dolby_double_apply = 1;
		rtd_outl(DOLBY_V_TOP_TOP_D_BUF_reg, top_d_buf_reg.regValue);

		// TODO:  DM setting
		DMRbusSet(DV_HDMI_MODE, (mdsChg==MDS_CHG_NONE)?0:1);
		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
		dm_double_buf_ctrl_reg.dm_db_apply = 1;
		rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
		//msleep(40);


		// set some vo register
#if 1

		//rtd_outl(VODMA_VODMA_V1SGEN_reg, rtd_inl(VODMA_VODMA_V1SGEN_reg) & ~(1<<30));	// release vo reset
		//rtd_outl(VODMA2_VODMA2_V1SGEN_reg, rtd_inl(VODMA2_VODMA2_V1SGEN_reg) & ~(1<<30));	// release vo reset

		rtd_outl(0xb80050b0, 0x00);
		//rtd_outl(0xb8005030, 0x00001043);		// to 422 bypass
		rtd_outl(0xb8005030, 0x000010a4);		// to 444 bypass
		rtd_outl(0xb800500c, dvr_to_phys((void *)gVoFrame1AddrAssign));
		rtd_outl(0xb8005000, 0x00100f49);
#endif

		// TODO: frame ready, then rpc to vcpu to start
		//int send_rpc_command(int opt, unsigned long command, unsigned long param1,
		//    unsigned long param2, unsigned long *retvalue)
		//send_rpc_command(RPC_VIDEO, VIDEO_RPC_VOUT_ToAgent_VoDriverReady, VoReady, 0, &ret))



		// TODO: recevice DM process and m-domain ready from vcpu rpc signal




		// TODO: format transfer from i-domain 10 bit to YUV422 12 bit


		voVsync_workaround = 1;
	}




	if (gFrame1Infp != NULL) {
	    file_sync(gFrame1Infp);
	    file_close(gFrame1Infp);
		gFrame1Infp = NULL;
	}
	if (gFrame1Outfp != NULL) {
	    file_sync(gFrame1Outfp);
	    file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}
	if (gMDInfp != NULL) {
	    file_sync(gMDInfp);
	    file_close(gMDInfp);
		gMDInfp = NULL;
	}

#endif

}


void IdomainBypassForCRF(DOLBYVISION_PATTERN *dolby)
{
#ifdef RTK_IDK_CRT
//#ifdef CONFIG_CUSTOMER_TV006
#if 1 //fix  me
	/* disable VIP */
#ifdef CONFIG_SUPPORT_SCALER
	fwif_color_disable_VIP(_ENABLE);
#endif


	/*
	i Edge Smooth:	0xb8025100.0
	H-MPEG NR:	0xb8023834.0
	SD_Dither(temporal): 0xb8023600.4
	SD_Dither:	0xb8023600.6:5
	RTNR_Y:  0xb80241a0.0
	RTNR_C:  0xb80241a0.1
	iDCTi:	 0xb80231d4.0
	Spatial_NR_Y:  0xb8025004.2
	Spatial_NR_C:  0xb8025004.1
	I_Peaking:	0xb8023100.26
	Spatial_NR_Impulse:  b8025004.0
	mosquito_NR: b8025090.0
	uzd vzoom en: b8025204.1
	uzd hzoom en: b8025204.0
	*/
	//default is enable?!
	//rtd_clearbits(IEDGE_SMOOTH_EdgeSmooth_EX_CTRL_reg, IEDGE_SMOOTH_EdgeSmooth_EX_CTRL_t2df_en_mask);
	//rtd_outl(IEDGE_SMOOTH_EdgeSmooth_EX_CTRL_reg, 0);
	//disabe all db
	//VGIP ch1
	rtd_clearbits(0xb80222e4,0x40000000);
	//i Edge Smooth bit 0 db
	rtd_clearbits(0xb8025170,0x1);
	//De XCXL db
	rtd_clearbits(0xb8023970,0x2);
	//DI
	rtd_clearbits(0xb8024538,0x2);
	//sNR
	rtd_clearbits(0xb802508c,0x1);
	//Decounter
	rtd_clearbits(0xb802575c,0x2);
	//disable vlpf
	rtd_clearbits(0xb8025164,0x40000000);
	//disable snrY/snrC:
	rtd_clearbits(0xb8025004,0x00000006);
	//disable Mnr
	rtd_clearbits(0xb8025090,0x00000001);

	rtd_outl(IEDGE_SMOOTH_EDSM_DB_CTRL_reg,IEDGE_SMOOTH_EDSM_DB_CTRL_get_edsm_db_en(0));
	rtd_clearbits(IEDGE_SMOOTH_dejagging_ctrl0_reg, IEDGE_SMOOTH_dejagging_ctrl0_i_dejag_en_mask);





	rtd_clearbits(MPEGNR_ICH1_MPEGNR2_reg, MPEGNR_ICH1_MPEGNR2_cp_mpegenable_x_mask);
	rtd_outl(MPEGNR_ICH1_MPEGNR2_reg,0);
	rtd_clearbits(HSD_DITHER_SD_Dither_CTRL1_reg, (0x1 << HSD_DITHER_SD_Dither_CTRL1_ch1_temporal_enable_shift));
	rtd_clearbits(HSD_DITHER_SD_Dither_CTRL1_reg, (0x3 << HSD_DITHER_SD_Dither_CTRL1_ch1_dithering_table_select_shift));
	rtd_clearbits(DI_IM_DI_RTNR_CONTROL_reg, DI_IM_DI_RTNR_CONTROL_cp_rtnr_y_enable_mask);
	rtd_clearbits(DI_IM_DI_RTNR_CONTROL_reg, DI_IM_DI_RTNR_CONTROL_cp_rtnr_c_enable_mask);
	rtd_clearbits(IDCTI_I_DCTI_CTRL_1_reg, IDCTI_I_DCTI_CTRL_1_dcti_en_mask);

	rtd_clearbits(NR_DCH1_CP_Ctrl_reg, NR_DCH1_CP_Ctrl_cp_spatialenabley_mask);
	rtd_clearbits(NR_DCH1_CP_Ctrl_reg, NR_DCH1_CP_Ctrl_cp_spatialenablec_mask);

	//rtd_clearbits(PEAKING_ICH1_PEAKING_FILTER_reg, PEAKING_ICH1_PEAKING_FILTER_peaking_enable_mask); 20180315 pinyen remove unused DV register
	rtd_clearbits(NR_DCH1_CP_Ctrl_reg, NR_DCH1_DebugMode_cp_debugmode_en_mask/*20180315 pinyen remove unused DV register NR_DCH1_CP_Ctrl_cp_impulseenable_mask*/);
	rtd_clearbits(NR_MOSQUITO_CTRL_reg, NR_MOSQUITO_CTRL_mosquito_detect_en_mask);
	rtd_clearbits(SCALEDOWN_ICH1_UZD_Ctrl0_reg, SCALEDOWN_ICH1_UZD_Ctrl0_v_zoom_en_mask);
	rtd_clearbits(SCALEDOWN_ICH1_UZD_Ctrl0_reg, SCALEDOWN_ICH1_UZD_Ctrl0_h_zoom_en_mask);

	/*
	SD_Dither(temporal): 0xb8022670.4
	I_Peaking:	0xb802310c.26
	Dither_T1:	0xb8022670.6:5
	Dither_T2:	0xb8022670.6:5
	uzd vzoom en: b8025400.1
	uzd hzoom en: b8025400.0

	*/
	rtd_clearbits(SUB_DITHER_I_Sub_Dither_CTRL1_reg, SUB_DITHER_I_Sub_Dither_CTRL1_ch2_temporal_enable_mask);
	//rtd_clearbits(PEAKING_ICH2_PEAKING_FILTER_reg, PEAKING_ICH2_PEAKING_FILTER_peaking_enable_mask); 20180315 pinyen remove unused DV register
	rtd_clearbits(SUB_DITHER_I_Sub_Dither_CTRL1_reg, (0x3<<SUB_DITHER_I_Sub_Dither_CTRL1_ch2_dithering_table_select_shift));
	rtd_clearbits(SUB_DITHER_I_Sub_Dither_CTRL1_reg, (0x3<<SUB_DITHER_I_Sub_Dither_CTRL1_ch2_dithering_table_select_shift));
	rtd_clearbits(SCALEDOWN_ICH2_UZD_Ctrl0_reg, SCALEDOWN_ICH2_UZD_Ctrl0_v_zoom_en_mask);
	rtd_clearbits(SCALEDOWN_ICH2_UZD_Ctrl0_reg, SCALEDOWN_ICH2_UZD_Ctrl0_h_zoom_en_mask);


	// dfilter: b8023504[18:16] =0
	rtd_clearbits(DFILTER_ICH1_DFILTER_NRING_MIS_NOS_YPBPR_reg,
		(0x7<<DFILTER_ICH1_DFILTER_NRING_MIS_NOS_YPBPR_ch1_y_en_shift));

	// multiband peaking: 0x18023C00.0 = 0
	rtd_clearbits(COLOR_MB_PEAKING_MB_PEAKING_CTRL_reg,
		COLOR_MB_PEAKING_MB_PEAKING_CTRL_mb_peaking_en_mask);

	// UZD setting
	rtd_outl(SCALEDOWN_ICH1_UZD_Ctrl0_reg,
		SCALEDOWN_ICH1_UZD_Ctrl0_sort_fmt_mask|SCALEDOWN_ICH1_UZD_Ctrl0_out_bit_mask);

	// disable H-mpeg NR
	rtd_clearbits(MPEGNR_ICH1_MPEGNR2_reg, MPEGNR_ICH1_MPEGNR2_cp_mpegenable_x_mask);

	// disable RTNR_R_Ct (DI spec)
	rtd_clearbits(DI_IM_DI_RTNR_CONTROL_reg, DI_IM_DI_RTNR_CONTROL_cp_rtnr_rounding_correction_mask);

	// i-domain 422to444 (rgb2yuv spec)
	//rtd_outl(RGB2YUV_ICH1_422to444_CTRL_reg, RGB2YUV_ICH1_422to444_CTRL_en_422to444_mask);

	// rgb2yuv(18023000 = 0x00000040) by Arnewss
	rtd_setbits(RGB2YUV_ICH1_RGB2YUV_CTRL_reg, RGB2YUV_ICH1_RGB2YUV_CTRL_set_uv_out_offset_mask);

	// added by smfan for d-domain
	// DM_UZU(_Ctrl)  BypassForNR	[23]
	rtd_clearbits(SCALEUP_DM_UZU_Ctrl_reg, SCALEUP_DM_UZU_Ctrl_bypassfornr_mask);

	// DI		// .31	chromaErr_en
	//rtd_clearbits(DI_IM_DI_FRAMESOBEL_STATISTIC_reg, DI_IM_DI_FRAMESOBEL_STATISTIC_chromaerror_en_mask); 20180315 pinyen remove unused DV register
	// DI		// .29	HMC_follow_Pan
	rtd_clearbits(DI_IM_DI_FRAMESOBEL_STATISTIC_reg, DI_IM_DI_FRAMESOBEL_STATISTIC_hmc_vector_follow_pan_en_mask);

	// UZD_LP121
	rtd_clearbits(SCALEDOWN_ICH1_UZD_Ctrl1_reg, SCALEDOWN_ICH1_UZD_Ctrl1_lp121_en_mask);

	// Dither_T1
	rtd_clearbits(MAIN_DITHER_I_Main_DITHER_CTRL1_reg, (0x3<<MAIN_DITHER_I_Main_DITHER_CTRL1_ch1_dithering_table_select_shift));

	//if(dolbyvisionEDR_current_vsif >= 5054)
//		rtd_setbits(MAIN_DITHER_I_Main_DITHER_CTRL1_reg, MAIN_DITHER_I_Main_DITHER_CTRL1_dither_bit_sel_shift);
//	else
		rtd_clearbits(MAIN_DITHER_I_Main_DITHER_CTRL1_reg, MAIN_DITHER_I_Main_DITHER_CTRL1_dither_bit_sel_mask);

	// Dither_bit10	// can't disable for composer dump
	//rtd_clearbits(0xb8022600, (1<<10));

	if(dolby != NULL) {
	if (dolby->dolby_mode == DV_NORMAL_MODE) {
		// rgb2yuv (rgb2yuv spec)
		rtd_outl(RGB2YUV_ICH2_RGB2YUV_CTRL_reg, RGB2YUV_ICH2_RGB2YUV_CTRL_set_uv_out_offset_mask);

		/*== sub ==*/
		rtd_clearbits(SUB_DITHER_I_Sub_Dither_CTRL1_reg,
			SUB_DITHER_I_Sub_Dither_CTRL1_ch2_temporal_enable_mask);
		//rtd_clearbits(PEAKING_ICH2_PEAKING_FILTER_reg, PEAKING_ICH2_PEAKING_FILTER_peaking_enable_mask);20180315 pinyen remove unused DV register
		rtd_clearbits(SUB_DITHER_I_Sub_Dither_CTRL1_reg,
			SUB_DITHER_I_Sub_Dither_CTRL1_ch2_dithering_table_select_mask);
		rtd_clearbits(SUB_DITHER_I_Sub_Dither_CTRL1_reg,
			SUB_DITHER_I_Sub_Dither_CTRL1_ch2_temporal_offset_separate_mode_mask);
		rtd_clearbits(SUB_DITHER_I_Sub_Dither_CTRL1_reg,
			SUB_DITHER_I_Sub_Dither_CTRL1_ch2_dithering_table_select_mask);
		rtd_clearbits(SUB_DITHER_I_Sub_Dither_CTRL1_reg,
			SUB_DITHER_I_Sub_Dither_CTRL1_ch2_temporal_offset_separate_mode_mask);
		/*== sub ==*/
		rtd_clearbits(COLOR_D_VC_Global_CTRL_reg,
			COLOR_D_VC_Global_CTRL_m_dcti_en_mask);
		//rtd_clearbits(GAMMA_GAMMA_CTRL_2_reg,_BIT4);
		//rtd_clearbits(GAMMA_GAMMA_CTRL_2_reg,_BIT5);
		rtd_clearbits(GAMMA_GAMMA_CTRL_2_reg,
			0x3 << GAMMA_GAMMA_CTRL_2_gamma_s_tab_sel_shift);

		rtd_clearbits(COLOR_D_VC_Global_CTRL_reg,
			COLOR_D_VC_Global_CTRL_m_sharp_en_mask);
		rtd_clearbits(COLOR_D_VC_Global_CTRL_reg,
			COLOR_D_VC_Global_CTRL_m_icm_en_mask);
		//rtd_clearbits(SRGB_sRGB_CTRL_reg,_BIT2);
		//rtd_clearbits(SRGB_sRGB_CTRL_reg,_BIT3);
/*20180315 pinyen remove unused DV register
		rtd_clearbits(SRGB_sRGB_CTRL_reg,
			0x3 << SRGB_sRGB_CTRL_srgb_s_tab_sel_shift);
*/
		// digital mode
		rtd_setbits(SUB_VGIP_VGIP_CHN2_CTRL_reg, SUB_VGIP_VGIP_CHN2_CTRL_ch2_digital_mode_mask);
	}


	// digital mode
	rtd_setbits(VGIP_VGIP_CHN1_CTRL_reg, VGIP_VGIP_CHN1_CTRL_ch1_hs_inv_shift);
	rtd_outl(VGIP_VGIP_CHN1_ACT_HSTA_Width_reg, dolby->ColNumTtl);
	rtd_outl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg, dolby->RowNumTtl);
	}

	//rtd_outl(VGIP_VGIP_CHN1_ACT_HSTA_Width_reg, (rtd_inl(VGIP_VGIP_CHN1_ACT_HSTA_Width_reg)&0xffffC000)|dolby->ColNumTtl);
	//rtd_outl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg, (rtd_inl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg)&0xffffC000)|dolby->RowNumTtl);

#endif
#endif
}


void DM_CRF_420_TestFlow(DOLBYVISION_PATTERN *dolby)
{
#if 0//#ifdef CONFIG_CUSTOMER_TV006//Remove by will

	int idx = 0, mdidx = 0;
	char tmp;
	int ret;
	MdsChg_t mdsChg;
	static unsigned int mdAddr = 0;
	unsigned char *ddrptr, *mdptr;
	dm_hdr_dm_double_buffer_ctrl_RBUS dm_double_buf_ctrl_reg;
	dolby_v_top_top_d_buf_RBUS top_d_buf_reg;
	static unsigned int voVsync_workaround = 0;
	mdomain_cap_cap_in1_enable_RBUS cap_in1_enable_reg;
	unsigned int mcapAddr;
	unsigned int Capture_byte_swap;
	//unsigned int TotalCapSize;
	DmExecFxp_t *pDmExec = h_dm->pDmExec;


	dm_cfg.frmPri.PxlDefOut.pxlClr = 0;
	dm_cfg.frmPri.PxlDefOut.pxlChrm = 1;
	dm_cfg.frmPri.PxlDefOut.pxlDtp = 0;
	dm_cfg.frmPri.PxlDefOut.pxlBdp = dolby->pxlBdp;
	dm_cfg.frmPri.PxlDefOut.pxlWeav = 2;
	dm_cfg.frmPri.PxlDefOut.pxlLoc = 0;
	dm_cfg.frmPri.PxlDefOut.pxlEotf = 0;
	dm_cfg.frmPri.PxlDefOut.pxlRng = 1;
	dm_cfg.frmPri.RowNumTtl = dolby->RowNumTtl;
	dm_cfg.frmPri.ColNumTtl = dolby->ColNumTtl;//3
	h_dm->dmExec.runMode = dm_cfg.dmCtrl.RunMode = RUN_MODE_NORMAL;

	SanityCheckDmCfg(&dm_cfg);

	/* disable VGIP interrupt */
	rtd_outl(0xb8022210, rtd_inl(0xb8022210) & ~(0x3<<24));
	rtd_outl(0xb802228C, 0);

	IdomainBypassForCRF(dolby);

	if (gFrame1Infp == NULL)
		//gInfp = file_open("/usr/local/gstreamer/share/Teststream_scrambled_3840x2160_UYVY_12b.yuv", O_RDONLY, 0);
		gFrame1Infp = file_open(dolby->file1Inpath, O_RDONLY, 0);
	if (gFrame1Infp == NULL) {
		printk("%s, gInfp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame1Outfp = gMDInfp = NULL;
		return ;
	}
	if (gFrame1Outfp == NULL)
		//gOutfp = file_open("/var/Teststream_scrambled_3840x2160_UYVY_12b.yuv.vodmaSeq8bit.yuv", O_TRUNC | O_WRONLY | O_CREAT, 0);
		gFrame1Outfp = file_open(dolby->fileOutpath, O_TRUNC | O_WRONLY | O_CREAT, 0);
	if (gFrame1Outfp == NULL) {
		printk("%s, gOutfp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame1Outfp = gMDInfp = NULL;
		return ;
	}
	if (gMDInfp == NULL)
		gMDInfp = file_open(dolby->fileMdInpath, O_RDONLY, 0);
	if (gMDInfp == NULL) {
		printk("%s, gMDInfp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame1Outfp = gMDInfp = NULL;
		return ;
	}

	if (gVoFrameAddr1 == 0) {
		gVoFrameAddr1 = (unsigned int)dvr_malloc_specific((3840*2160*4), GFP_DCU2);
	}
	if (gVoFrameAddr2 == 0) {
		gVoFrameAddr2 = (unsigned int)dvr_malloc_specific((3840*2160*4), GFP_DCU2);
	}
	printk(KERN_EMERG"physical gVoFrameAddr1=0x%x, virtual gVoFrameAddr1=0x%x \n", (unsigned int)dvr_to_phys((void *)gVoFrameAddr1), gVoFrameAddr1);
	printk(KERN_EMERG"physical gVoFrameAddr2=0x%x, virtual gVoFrameAddr2=0x%x \n", (unsigned int)dvr_to_phys((void *)gVoFrameAddr2), gVoFrameAddr2);

	if (mdAddr == 0)
		mdAddr = (unsigned int)dvr_malloc_specific(0x100000, GFP_DCU2);
	printk(KERN_EMERG"mdAddr=0x%x\n", (unsigned int)dvr_to_phys((void *)mdAddr));

	/* store MD data to DDR */
	ddrptr = (unsigned char *)mdAddr;
	for (idx = 0; idx < dolby->fileMdInSize; idx++) {
		file_read((unsigned char *)&tmp, 1, idx, gMDInfp);
		*ddrptr = tmp;
		ddrptr++;
	}

	if (voVsync_workaround == 0) {

		rtd_outl(VODMA_VODMA_VGIP_INTPOS_reg, 0x00);	// disable interrupt
		rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, 0x00);	// disable interrupt
		// disable VODMA1
		rtd_outl(VODMA_VODMA_V1_DCFG_reg, (rtd_inl(VODMA_VODMA_V1_DCFG_reg)&~1) | (1<<3));
		// disable VODMA2
		rtd_outl(VODMA2_VODMA2_V1_DCFG_reg, (rtd_inl(VODMA2_VODMA2_V1_DCFG_reg)&~1) | (1<<3));
		//rtd_outl(VODMA_VODMA_V1SGEN_reg, rtd_inl(VODMA_VODMA_V1SGEN_reg) | (1<<30));	// vo reset high
		//rtd_outl(VODMA2_VODMA2_V1SGEN_reg, rtd_inl(VODMA2_VODMA2_V1SGEN_reg) | (1<<30));	// vo reset high
		//printk("%s, vodma reset....\n", __FUNCTION__);

		// VO timing	// for 2K1K case
		if (dolby->ColNumTtl == 1920 && dolby->RowNumTtl == 1080) {
			rtd_outl(VODMA_VODMA_V1VGIP_VACT1_reg, 0x044a0012);
			rtd_outl(VODMA_VODMA_V1VGIP_VACT2_reg, 0x044a0012);
			rtd_outl(VODMA_VODMA_V1VGIP_VBG1_reg, 0x044a0012);
			rtd_outl(VODMA_VODMA_V1VGIP_VBG2_reg, 0x044a0012);
		}
	}


	// test 5 frame
	for (idx = 0; idx < 1; idx++) {

		g_frameCtrlNo = dolby->frameNo;
		if ((g_frameCtrlNo%2)==0)
			gVoFrame1AddrAssign = gVoFrameAddr1;
		else
			gVoFrame1AddrAssign = gVoFrameAddr2;
		Fmt_Cvt_14YUV420_to_10SEQ444(gFrame1Infp, gFrame1Outfp, dolby->ColNumTtl, dolby->RowNumTtl);

		flush_cache_all();

		// TODO:  meta data parsing
		mdptr = (char *)mdAddr;
		for (mdidx = 0; mdidx <= g_frameCtrlNo; mdidx++) {
			ret = dm_metadata_2_dm_param((dm_metadata_base_t *)mdptr, &mds_ext);
			mdptr += ret;
		}
		mdsChg = CheckMds(&mds_ext, &h_dm->mdsExt);	// modified by smfan
		CommitMds(&mds_ext, h_dm);

		flush_cache_all();

		// TODO: dolby_v_top setting
		DolbyVTopRbusSet(dolby, DV_DM_CRF_420_MODE);
		top_d_buf_reg.regValue = rtd_inl(DOLBY_V_TOP_TOP_D_BUF_reg);
		top_d_buf_reg.dolby_double_apply = 1;
		rtd_outl(DOLBY_V_TOP_TOP_D_BUF_reg, top_d_buf_reg.regValue);

		// TODO:  DM setting
		DMRbusSet(DV_NORMAL_MODE, (mdsChg==MDS_CHG_NONE)?0:1);

		// postpone the update B05 timing
		// B05 3D LUT
		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
		dm_double_buf_ctrl_reg.dm_db_en = 0;
		rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
		// disable B05
		rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) & ~DM_dm_submodule_enable_b05_enable_mask);
		DM_B05_Set(pDmExec->bwDmLut.lutMap, g_forceUpdate_3DLUT);
		// enable double buffer
		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
		dm_double_buf_ctrl_reg.dm_db_apply = 0;
		dm_double_buf_ctrl_reg.dm_db_en = 1;
		rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
		// enable B05
		rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) | DM_dm_submodule_enable_b05_enable_mask);

		// disable dithering
		rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) & ~DM_dm_submodule_enable_dither_en_mask);

		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
		dm_double_buf_ctrl_reg.dm_db_apply = 1;
		rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
		msleep(40);


		// set some vo register
#if 1
		//rtd_outl(VODMA_VODMA_V1SGEN_reg, rtd_inl(VODMA_VODMA_V1SGEN_reg) & ~(1<<30));	// release vo reset
		//rtd_outl(VODMA2_VODMA2_V1SGEN_reg, rtd_inl(VODMA2_VODMA2_V1SGEN_reg) & ~(1<<30));	// release vo reset

		rtd_outl(0xb80050b0, 0x00);
		//rtd_outl(0xb8005030, 0x00001043);		// to 422 bypass
		rtd_outl(0xb8005030, 0x000010a4);		// to 444 bypass
		rtd_outl(0xb800500c, dvr_to_phys((void *)gVoFrame1AddrAssign));
		//rtd_outl(0xb8005000, 0x00104f49);	// enable 10b seq mode
		rtd_outl(0xb8005000, 0x0010cf49);	// enable 10b seq mode + field based (dummy data behind frame data)
#endif

		IdomainBypassForCRF(dolby);

#if 0	// for PowerOnVideo method
		// disable M-cap DDR_In1Ctrl double buffer & M-disp DDR_MainCtrl Double buffer
		rtd_outl(MDOMAIN_CAP_DDR_In1Ctrl_reg, rtd_inl(MDOMAIN_CAP_DDR_In1Ctrl_reg) & ~(MDOMAIN_CAP_DDR_In1Ctrl_in1_double_enable_mask));
		rtd_outl(MDOMAIN_DISP_DDR_MainCtrl_reg, rtd_inl(MDOMAIN_DISP_DDR_MainCtrl_reg) & ~(MDOMAIN_DISP_DDR_MainCtrl_main_double_en_mask));
		// clear M-cap freeze
		rtd_outl(MDOMAIN_CAP_DDR_In1Ctrl_reg, rtd_inl(MDOMAIN_CAP_DDR_In1Ctrl_reg) & ~(MDOMAIN_CAP_DDR_In1Ctrl_in1_freeze_enable_mask));
#endif

		// enable M-cap
		cap_in1_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In1_enable_reg);
		cap_in1_enable_reg.in1_cap_enable = 1;
		rtd_outl(MDOMAIN_CAP_Cap_In1_enable_reg, cap_in1_enable_reg.regValue);


		msleep(100);

		// record Capture_byte_swap setting and set No byte swap
		Capture_byte_swap = rtd_inl(MDOMAIN_CAP_Capture1_byte_swap_reg);
		rtd_outl(MDOMAIN_CAP_Capture1_byte_swap_reg, (rtd_inl(MDOMAIN_CAP_Capture1_byte_swap_reg)&~(0xf)) | 0x7);	// byte swap 1+2+4

		// wait M-capture done
		// wait data end
		flush_cache_all();
		flush_cache_all();
#ifdef CONFIG_SUPPORT_SCALER
		WaitFor_DEN_STOP_Done();
		WaitFor_DEN_STOP_Done();
		WaitFor_DEN_STOP_Done();
		WaitFor_DEN_STOP_Done();
#endif
		flush_cache_all();
		flush_cache_all();
		flush_cache_all();
		msleep(1000);

		// disable M-cap
		cap_in1_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In1_enable_reg);
		cap_in1_enable_reg.in1_cap_enable = 0;
		rtd_outl(MDOMAIN_CAP_Cap_In1_enable_reg, cap_in1_enable_reg.regValue);

		// get M-cap buffer address
		mcapAddr = g_IDKDumpAddr;
		if (mcapAddr == 0)
			mcapAddr = (unsigned int)phys_to_virt(rtd_inl(MDOMAIN_CAP_DDR_In1Addr_reg));
		//TotalCapSize = memory_get_capture_size(Scaler_DispGetInputInfo(SLR_INPUT_DISPLAY), MEMCAPTYPE_LINE);
		//TotalCapSize = (TotalCapSize<<3) * dolby->RowNumTtl;
		//mcapAddr = dvr_remap_uncached_memory(rtd_inl(MDOMAIN_CAP_DDR_In1Addr_reg),
		//													TotalCapSize,
		//													__builtin_return_address(0));

		printk("%s, physical mcapAddr=0x%x \n", __FUNCTION__, rtd_inl(MDOMAIN_CAP_DDR_In1Addr_reg));
		printk("%s, mcapAddr=0x%x,Capture_byte_swap=%d \n", __FUNCTION__, mcapAddr, Capture_byte_swap);


		// captured fmt transfers to DolbyVision DM Output fmt
		Fmt_DMCRF422_Mcap_to_DM(mcapAddr, dolby);


		// restore byte swap
		rtd_outl(MDOMAIN_CAP_Capture1_byte_swap_reg, Capture_byte_swap);

		// enable M-cap
		cap_in1_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In1_enable_reg);
		cap_in1_enable_reg.in1_cap_enable = 1;
		rtd_outl(MDOMAIN_CAP_Cap_In1_enable_reg, cap_in1_enable_reg.regValue);

		// free virtual memory
		//dvr_unmap_memory(mcapAddr, TotalCapSize);

		voVsync_workaround = 1;


	}

	if (gFrame1Infp != NULL) {
	    file_sync(gFrame1Infp);
	    file_close(gFrame1Infp);
		gFrame1Infp = NULL;
	}
	if (gFrame1Outfp != NULL) {
	    file_sync(gFrame1Outfp);
	    file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}
	if (gMDInfp != NULL) {
	    file_sync(gMDInfp);
	    file_close(gMDInfp);
		gMDInfp = NULL;
	}

#endif

}



void DM_CRF_422_TestFlow(DOLBYVISION_PATTERN *dolby)
{
#if 0//#ifdef CONFIG_CUSTOMER_TV006//Remove by will

	int idx = 0, mdidx = 0;
	char tmp;
	int ret;
	MdsChg_t mdsChg;
	static unsigned int mdAddr = 0;
	unsigned char *ddrptr, *mdptr;
	dm_hdr_dm_double_buffer_ctrl_RBUS dm_double_buf_ctrl_reg;
	dolby_v_top_top_d_buf_RBUS top_d_buf_reg;
	static unsigned int voVsync_workaround = 0;
	mdomain_cap_cap_in1_enable_RBUS cap_in1_enable_reg;
	unsigned int mcapAddr, AddrTmp;
	unsigned int Capture_byte_swap;
	//unsigned int TotalCapSize;
	unsigned int lineCnt_in, vo_vstart, vo_vend;
	DmExecFxp_t *pDmExec = h_dm->pDmExec;


	dm_cfg.frmPri.PxlDefOut.pxlClr = 0;
	dm_cfg.frmPri.PxlDefOut.pxlChrm = 1;
	dm_cfg.frmPri.PxlDefOut.pxlDtp = 0;
	dm_cfg.frmPri.PxlDefOut.pxlBdp = dolby->pxlBdp;//14;
	dm_cfg.frmPri.PxlDefOut.pxlWeav = 2;
	dm_cfg.frmPri.PxlDefOut.pxlLoc = 0;
	dm_cfg.frmPri.PxlDefOut.pxlEotf = 0;
	dm_cfg.frmPri.PxlDefOut.pxlRng = 1;
	dm_cfg.frmPri.RowNumTtl = dolby->RowNumTtl;
	dm_cfg.frmPri.ColNumTtl = dolby->ColNumTtl;//3
	h_dm->dmExec.runMode = dm_cfg.dmCtrl.RunMode = RUN_MODE_FROMHDMI;

	SanityCheckDmCfg(&dm_cfg);


	/* disable VGIP interrupt */
	rtd_outl(0xb8022210, rtd_inl(0xb8022210) & ~(0x3<<24));
	rtd_outl(0xb802228C, 0);

	IdomainBypassForCRF(dolby);

	if (gFrame1Infp == NULL)
		//gInfp = file_open("/usr/local/gstreamer/share/Teststream_scrambled_3840x2160_UYVY_12b.yuv", O_RDONLY, 0);
		gFrame1Infp = file_open(dolby->file1Inpath, O_RDONLY, 0);
	if (gFrame1Infp == NULL) {
		printk("%s, gInfp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame1Outfp = gMDInfp = NULL;
		return ;
	}
	if (gFrame1Outfp == NULL)
		//gOutfp = file_open("/var/Teststream_scrambled_3840x2160_UYVY_12b.yuv.vodmaSeq8bit.yuv", O_TRUNC | O_WRONLY | O_CREAT, 0);
		gFrame1Outfp = file_open(dolby->fileOutpath, O_TRUNC | O_WRONLY | O_CREAT, 0);
	if (gFrame1Outfp == NULL) {
		printk("%s, gOutfp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame1Outfp = gMDInfp = NULL;
		return ;
	}
	if (gMDInfp == NULL)
		gMDInfp = file_open(dolby->fileMdInpath, O_RDONLY, 0);
	if (gMDInfp == NULL) {
		printk("%s, gMDInfp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame1Outfp = gMDInfp = NULL;
		return ;
	}

	if (gVoFrameAddr1 == 0) {
		AddrTmp = (unsigned int)dvr_malloc_uncached_specific((3840*2160*3), GFP_DCU2_FIRST, (void **)&gVoFrameAddr1);
	}
	if (gVoFrameAddr2 == 0) {
		AddrTmp = (unsigned int)dvr_malloc_uncached_specific((3840*2160*3), GFP_DCU2_FIRST, (void **)&gVoFrameAddr2);
	}
	printk(KERN_EMERG"physical gVoFrameAddr1=0x%x, virtual gVoFrameAddr1=0x%x \n", (unsigned int)dvr_to_phys((void *)gVoFrameAddr1), gVoFrameAddr1);
	printk(KERN_EMERG"physical gVoFrameAddr2=0x%x, virtual gVoFrameAddr2=0x%x \n", (unsigned int)dvr_to_phys((void *)gVoFrameAddr2), gVoFrameAddr2);

	if (mdAddr == 0)
		AddrTmp = (unsigned int)dvr_malloc_uncached_specific(0x100000, GFP_DCU2, (void **)&mdAddr);
	printk(KERN_EMERG"physical mdAddr=0x%x, virtual mdAddr=0x%x \n", (unsigned int)dvr_to_phys((void *)mdAddr), mdAddr);

	/* store MD data to DDR */
	ddrptr = (unsigned char *)mdAddr;
	for (idx = 0; idx < dolby->fileMdInSize; idx++) {
		file_read((unsigned char *)&tmp, 1, idx, gMDInfp);
		*ddrptr = tmp;
		ddrptr++;
	}

	if (voVsync_workaround == 0) {

		rtd_outl(VODMA_VODMA_VGIP_INTPOS_reg, 0x00);	// disable interrupt
		rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, 0x00);	// disable interrupt

		// disable VODMA1
		rtd_outl(VODMA_VODMA_V1_DCFG_reg, (rtd_inl(VODMA_VODMA_V1_DCFG_reg)&~1) | (1<<3));
		// disable VODMA2
		rtd_outl(VODMA2_VODMA2_V1_DCFG_reg, (rtd_inl(VODMA2_VODMA2_V1_DCFG_reg)&~1) | (1<<3));
		//rtd_outl(VODMA_VODMA_V1SGEN_reg, rtd_inl(VODMA_VODMA_V1SGEN_reg) | (1<<30));	// vo reset high
		//rtd_outl(VODMA2_VODMA2_V1SGEN_reg, rtd_inl(VODMA2_VODMA2_V1SGEN_reg) | (1<<30));	// vo reset high
		//printk("%s, vodma reset....\n", __FUNCTION__);

		// VO timing	// for IDK test 5010 and other 2K1K case
		if (dolby->ColNumTtl == 1920 && dolby->RowNumTtl == 1080) {
			rtd_outl(VODMA_VODMA_V1VGIP_VACT1_reg, 0x044a0012);
			rtd_outl(VODMA_VODMA_V1VGIP_VACT2_reg, 0x044a0012);
			rtd_outl(VODMA_VODMA_V1VGIP_VBG1_reg, 0x044a0012);
			rtd_outl(VODMA_VODMA_V1VGIP_VBG2_reg, 0x044a0012);
		}

	}


	// test 5 frame
	for (idx = 0; idx < 1; idx++) {

		g_frameCtrlNo = dolby->frameNo;
		if ((g_frameCtrlNo%2)==0)
			gVoFrame1AddrAssign = gVoFrameAddr1;
		else
			gVoFrame1AddrAssign = gVoFrameAddr2;

		Fmt_Cvt_12YUV422_to_8SEQ444(gFrame1Infp, gFrame1Outfp, dolby->ColNumTtl, dolby->RowNumTtl);

		flush_cache_all();
		flush_cache_all();

		// TODO:  meta data parsing
		mdptr = (char *)mdAddr;
		for (mdidx = 0; mdidx <= g_frameCtrlNo; mdidx++) {
			ret = dm_metadata_2_dm_param((dm_metadata_base_t *)mdptr, &mds_ext);
			mdptr += ret;
		}

		mdsChg = CheckMds(&mds_ext, &h_dm->mdsExt);	// modified by smfan
		CommitMds(&mds_ext, h_dm);

		flush_cache_all();

		// TODO: dolby_v_top setting
		DolbyVTopRbusSet(dolby, DV_DM_CRF_422_MODE);
		top_d_buf_reg.regValue = rtd_inl(DOLBY_V_TOP_TOP_D_BUF_reg);
		top_d_buf_reg.dolby_double_apply = 1;
		rtd_outl(DOLBY_V_TOP_TOP_D_BUF_reg, top_d_buf_reg.regValue);


		// polling the front porch timing
		lineCnt_in = rtd_inl(VODMA_VODMA_LINE_ST_reg) & 0xfff;
		vo_vstart = rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) & 0xfff;
		vo_vend = (rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) >> 16) & 0xfff;
		while (lineCnt_in < (vo_vend-1)) {
			lineCnt_in = rtd_inl(VODMA_VODMA_LINE_ST_reg) & 0xfff;
			vo_vstart = rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) & 0xfff;
			vo_vend = (rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) >> 16) & 0xfff;
			Warning("%s, waiting for front porch \n", __FUNCTION__);
		}

		printk("%s, mdsChg=%d \n", __FUNCTION__, mdsChg);

		// TODO:  DM setting
		DMRbusSet(DV_HDMI_MODE, (mdsChg==MDS_CHG_NONE)?0:1);

		// postpone the update B05 timing
		// B05 3D LUT
		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
		dm_double_buf_ctrl_reg.dm_db_en = 0;
		rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
		// disable B05
		rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) & ~DM_dm_submodule_enable_b05_enable_mask);
		DM_B05_Set(pDmExec->bwDmLut.lutMap, g_forceUpdate_3DLUT);
		// enable double buffer
		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
		dm_double_buf_ctrl_reg.dm_db_apply = 0;
		dm_double_buf_ctrl_reg.dm_db_en = 1;
		rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
		// enable B05
		rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) | DM_dm_submodule_enable_b05_enable_mask);

		// disable dithering
		rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) & ~DM_dm_submodule_enable_dither_en_mask);

		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
		dm_double_buf_ctrl_reg.dm_db_apply = 1;
		rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
		msleep(40);


#if 1
		// set some vo register
		//rtd_outl(VODMA_VODMA_V1SGEN_reg, rtd_inl(VODMA_VODMA_V1SGEN_reg) & ~(1<<30));	// release vo reset
		//rtd_outl(VODMA2_VODMA2_V1SGEN_reg, rtd_inl(VODMA2_VODMA2_V1SGEN_reg) & ~(1<<30));	// release vo reset

		rtd_outl(0xb80050b0, 0x00);
		rtd_outl(0xb8005030, 0x000010a4);		// to 444 bypass
		rtd_outl(0xb800500c, dvr_to_phys((void *)gVoFrame1AddrAssign));
		rtd_outl(VODMA_VODMA_V1_DSIZE_reg, (dolby->ColNumTtl<<16 | dolby->RowNumTtl));
		//rtd_outl(0xb8005000, 0x00100f49);
		rtd_outl(0xb8005000, 0x00108f49);	// enable 8b seq mode + field based (dummy data behind frame data)
#endif


		IdomainBypassForCRF(dolby);


		// for testing to control vactive start
		//rtd_outl(0xb802429c, (rtd_inl(0xb802429c)) | (0xe<<16));

#if 0	// for PowerOnVideo method
		// disable M-cap DDR_In1Ctrl double buffer & M-disp DDR_MainCtrl Double buffer
		rtd_outl(MDOMAIN_CAP_DDR_In1Ctrl_reg, rtd_inl(MDOMAIN_CAP_DDR_In1Ctrl_reg) & ~(MDOMAIN_CAP_DDR_In1Ctrl_in1_double_enable_mask));
		rtd_outl(MDOMAIN_DISP_DDR_MainCtrl_reg, rtd_inl(MDOMAIN_DISP_DDR_MainCtrl_reg) & ~(MDOMAIN_DISP_DDR_MainCtrl_main_double_en_mask));
		// clear M-cap freeze
		rtd_outl(MDOMAIN_CAP_DDR_In1Ctrl_reg, rtd_inl(MDOMAIN_CAP_DDR_In1Ctrl_reg) & ~(MDOMAIN_CAP_DDR_In1Ctrl_in1_freeze_enable_mask));
#endif

		// enable M-cap
		cap_in1_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In1_enable_reg);
		cap_in1_enable_reg.in1_cap_enable = 1;
		rtd_outl(MDOMAIN_CAP_Cap_In1_enable_reg, cap_in1_enable_reg.regValue);


		msleep(100);

		// record Capture_byte_swap setting and set No byte swap
		Capture_byte_swap = rtd_inl(MDOMAIN_CAP_Capture1_byte_swap_reg);
		rtd_outl(MDOMAIN_CAP_Capture1_byte_swap_reg, (rtd_inl(MDOMAIN_CAP_Capture1_byte_swap_reg)&~(0xf)) | 0x7);	// byte swap 1+2+4

		// wait M-capture done
		// wait data end
		flush_cache_all();
		flush_cache_all();
#ifdef CONFIG_SUPPORT_SCALER
		WaitFor_DEN_STOP_Done();
		WaitFor_DEN_STOP_Done();
		WaitFor_DEN_STOP_Done();
		WaitFor_DEN_STOP_Done();
#endif
		flush_cache_all();
		flush_cache_all();
		flush_cache_all();
		msleep(1000);

		// disable M-cap
		cap_in1_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In1_enable_reg);
		cap_in1_enable_reg.in1_cap_enable = 0;
		rtd_outl(MDOMAIN_CAP_Cap_In1_enable_reg, cap_in1_enable_reg.regValue);

		// get M-cap buffer address
		mcapAddr = g_IDKDumpAddr;
		if (mcapAddr == 0)
			mcapAddr = (unsigned int)phys_to_virt(rtd_inl(MDOMAIN_CAP_DDR_In1Addr_reg));
		//TotalCapSize = memory_get_capture_size(Scaler_DispGetInputInfo(SLR_INPUT_DISPLAY), MEMCAPTYPE_LINE);
		//TotalCapSize = (TotalCapSize<<3) * dolby->RowNumTtl;
		//mcapAddr = dvr_remap_uncached_memory(rtd_inl(MDOMAIN_CAP_DDR_In1Addr_reg),
		//													TotalCapSize,
		//													__builtin_return_address(0));

		printk("%s, physical mcapAddr=0x%x \n", __FUNCTION__, rtd_inl(MDOMAIN_CAP_DDR_In1Addr_reg));
		printk("%s, mcapAddr=0x%x,Capture_byte_swap=%d \n", __FUNCTION__, mcapAddr, Capture_byte_swap);


		// captured fmt transfers to DolbyVision DM Output fmt
		Fmt_DMCRF422_Mcap_to_DM(mcapAddr, dolby);


		// restore byte swap
		rtd_outl(MDOMAIN_CAP_Capture1_byte_swap_reg, Capture_byte_swap);

		// enable M-cap
		cap_in1_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In1_enable_reg);
		cap_in1_enable_reg.in1_cap_enable = 1;
		rtd_outl(MDOMAIN_CAP_Cap_In1_enable_reg, cap_in1_enable_reg.regValue);

		// free virtual memory
		//dvr_unmap_memory(mcapAddr, TotalCapSize);

		voVsync_workaround = 1;

	}


	if (gFrame1Infp != NULL) {
	    file_sync(gFrame1Infp);
	    file_close(gFrame1Infp);
		gFrame1Infp = NULL;
	}
	if (gFrame1Outfp != NULL) {
	    file_sync(gFrame1Outfp);
	    file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}
	if (gMDInfp != NULL) {
	    file_sync(gMDInfp);
	    file_close(gMDInfp);
		gMDInfp = NULL;
	}
#endif
}

void Composer_CRF_TestFlow(DOLBYVISION_PATTERN *dolby)
{
//#ifdef CONFIG_CUSTOMER_TV006
#if 0
	int idx = 0, mdidx = 0;
	unsigned long roffset = 0;
	char tmp;
	//int ret;
	//MdsChg_t mdsChg;
	unsigned char *ddrptr;//, *mdptr;
	//dm_double_buffer_ctrl_RBUS dm_double_buf_ctrl_reg;
	dhdr_v_composer_composer_db_ctrl_RBUS comp_db_ctr_reg;
	dolby_v_top_top_d_buf_RBUS top_d_buf_reg;
	rpu_ext_config_fixpt_main_t *p_CompMd;
	static unsigned int iv_act_sta = 0;
	unsigned int el_width, el_height;
	//DOLBYVISION_INIT_STRUCT dv_init;


	dm_cfg.frmPri.PxlDefOut.pxlClr = 0;
	dm_cfg.frmPri.PxlDefOut.pxlChrm = 1;
	dm_cfg.frmPri.PxlDefOut.pxlDtp = 0;
	dm_cfg.frmPri.PxlDefOut.pxlBdp = dolby->pxlBdp;
	dm_cfg.frmPri.PxlDefOut.pxlWeav = 2;
	dm_cfg.frmPri.PxlDefOut.pxlLoc = 0;
	dm_cfg.frmPri.PxlDefOut.pxlEotf = 0;
	dm_cfg.frmPri.PxlDefOut.pxlRng = 1;
	dm_cfg.frmPri.RowNumTtl = dolby->RowNumTtl;
	dm_cfg.frmPri.ColNumTtl = dolby->ColNumTtl;
	h_dm->dmExec.runMode = dm_cfg.dmCtrl.RunMode = RUN_MODE_NORMAL;

	SanityCheckDmCfg(&dm_cfg);


	if (gFrame1Infp == NULL)
		gFrame1Infp = file_open(dolby->file1Inpath, O_RDONLY, 0);
	if (gFrame1Infp == NULL) {
		printk("%s, gFrame1Infp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame2Infp = gFrame1Outfp = gFrame2Outfp = gMDInfp = NULL;
		return ;
	}
	if (gFrame2Infp == NULL)
		gFrame2Infp = file_open(dolby->file2Inpath, O_RDONLY, 0);
	if (gFrame2Infp == NULL) {
		printk("%s, gFrame2Infp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame2Infp = gFrame1Outfp = gFrame2Outfp = gMDInfp = NULL;
		return ;
	}
	if (gFrame1Outfp == NULL)
		gFrame1Outfp = file_open("/var/Yblock.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
	if (gFrame1Outfp == NULL) {
		printk("%s, gFrame1Outfp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame2Infp = gFrame1Outfp = gFrame2Outfp = gMDInfp = NULL;
		return ;
	}
	if (gFrame2Outfp == NULL)
		gFrame2Outfp = file_open("/var/Cblock.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
	if (gFrame2Outfp == NULL) {
		printk("%s, gFrame2Outfp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame2Infp = gFrame1Outfp = gFrame2Outfp = gMDInfp = NULL;
		return ;
	}
	if (gMDInfp == NULL)
		gMDInfp = file_open(dolby->fileMdInpath, O_RDONLY, 0);
	if (gMDInfp == NULL) {
		printk("%s, gMDInfp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame2Infp = gFrame1Outfp = gFrame2Outfp = gMDInfp = NULL;
		return ;
	}

	if (gVoFrameAddr1 == 0)
		gVoFrameAddr1 = (unsigned int)dvr_malloc_specific((3840*2160*2), GFP_DCU1);
	if (gVoFrameAddr2 == 0)
		gVoFrameAddr2 = (unsigned int)dvr_malloc_specific((3840*2160*2), GFP_DCU1);
	if (gVoFrameAddr3 == 0)
		gVoFrameAddr3 = (unsigned int)dvr_malloc_specific((3840*2160*2), GFP_DCU1);
	if (gVoFrameAddr4 == 0)
		gVoFrameAddr4 = (unsigned int)dvr_malloc_specific((3840*2160*2), GFP_DCU1);
	printk(KERN_EMERG"physical gVoFrameAddr1=0x%x, virtual gVoFrameAddr1=0x%x \n", (unsigned int)dvr_to_phys((void *)gVoFrameAddr1), gVoFrameAddr1);
	printk(KERN_EMERG"physical gVoFrameAddr2=0x%x, virtual gVoFrameAddr2=0x%x \n", (unsigned int)dvr_to_phys((void *)gVoFrameAddr2), gVoFrameAddr2);
	printk(KERN_EMERG"physical gVoFrameAddr3=0x%x, virtual gVoFrameAddr3=0x%x \n", (unsigned int)dvr_to_phys((void *)gVoFrameAddr3), gVoFrameAddr3);
	printk(KERN_EMERG"physical gVoFrameAddr4=0x%x, virtual gVoFrameAddr4=0x%x \n", (unsigned int)dvr_to_phys((void *)gVoFrameAddr4), gVoFrameAddr4);

	if (gRpuMDAddr == 0)
		gRpuMDAddr = (unsigned int)dvr_malloc_specific(0x100000, GFP_DCU1);
	printk(KERN_EMERG"gRpuMDAddr=0x%x\n", (unsigned int)dvr_to_phys((void *)gRpuMDAddr));


	/* store MD data to DDR */
	ddrptr = (unsigned char *)gRpuMDAddr;
	for (idx = 0; idx < dolby->fileMdInSize; idx++) {
		file_read((unsigned char *)&tmp, 1, idx, gMDInfp);
		*ddrptr = tmp;
		ddrptr++;
	}

	// check EL size
	if (dolby->BL_EL_ratio) {	// 4:1
		el_width = dolby->ColNumTtl / 2;
		el_height = dolby->RowNumTtl / 2;
	} else {
		el_width = dolby->ColNumTtl;
		el_height = dolby->RowNumTtl;
	}



	if (iv_act_sta == 0) {
		// sensio timing setting
		rtd_outl(SENSIO_Sensio_ctrl_2_reg, (dolby->RowNumTtl<<16) | (dolby->ColNumTtl-1));
		rtd_outl(SENSIO_sub_vp9_decomp_ctrl_1_reg, (el_height<<16) | (el_width-1));

		rtd_outl(SENSIO_SENSIO_TIM_CTRL_0_reg, (5<<16) |
				VODMA_VODMA_V1SGEN_get_h_thr(rtd_inl(VODMA_VODMA_V1SGEN_reg)));
		rtd_outl(SENSIO_SENSIO_TIM_CTRL_1_reg, (VODMA_VODMA_V1VGIP_VACT1_get_v_st(rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg))-5)<<16 |
					(VODMA_VODMA_V1VGIP_HACT_get_h_st(rtd_inl(VODMA_VODMA_V1VGIP_HACT_reg))+5));
		rtd_outl(SENSIO_SUB_SENSIO_TIM_CTRL_0_reg, (5<<16) |
				VODMA_VODMA_V1SGEN_get_h_thr(rtd_inl(VODMA2_VODMA2_V1SGEN_reg)));
		rtd_outl(SENSIO_SUB_SENSIO_TIM_CTRL_1_reg, (VODMA2_VODMA2_V1VGIP_VACT1_get_v_st(rtd_inl(VODMA2_VODMA2_V1VGIP_VACT1_reg))-5)<<16 |
					(VODMA2_VODMA2_V1VGIP_HACT_get_h_st(rtd_inl(VODMA2_VODMA2_V1VGIP_HACT_reg))+5));
		rtd_outl(SENSIO_Sensio_ctrl_0_reg, 0x00000060);
		rtd_outl(SENSIO_SUB_vp9_decomp_ctrl_0_reg, 0x00000000);
		rtd_outl(SENSIO_Sensio_ctrl_3_reg, 0x00070000);		// apply
		msleep(40);


		rtd_outl(VODMA_VODMA_VGIP_INTPOS_reg, 0x00);	// disable interrupt
		rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, 0x00);	// disable interrupt
		// disable VODMA1
		//rtd_outl(VODMA_VODMA_V1_DCFG_reg, (rtd_inl(VODMA_VODMA_V1_DCFG_reg)&~1) | (1<<3));
		// disable VODMA2
		//rtd_outl(VODMA2_VODMA2_V1_DCFG_reg, (rtd_inl(VODMA2_VODMA2_V1_DCFG_reg)&~1) | (1<<3));
		//rtd_outl(VODMA_VODMA_V1SGEN_reg, rtd_inl(VODMA_VODMA_V1SGEN_reg) | (1<<30));	// vo reset high
		//rtd_outl(VODMA2_VODMA2_V1SGEN_reg, rtd_inl(VODMA2_VODMA2_V1SGEN_reg) | (1<<30));	// vo reset high
		//printk("%s, vodma reset....\n", __FUNCTION__);
	}



	// test 5 frame
	for (idx = 0; idx < 1; idx++) {

		g_frameCtrlNo = dolby->frameNo;

		// TODO: rpu data  ->  composer & DM metadata
		ddrptr = (unsigned char *)gRpuMDAddr;
		g_MD_parser_state = MD_PARSER_INIT;
		g_mdparser_output_type = 1;
		for (mdidx = 0; mdidx <= g_frameCtrlNo; mdidx++) {
			g_dvframeCnt = g_recordDMAddr = 0;
			roffset = metadata_parser_main(ddrptr, dolby->fileMdInSize, g_MD_parser_state);
			g_MD_parser_state = MD_PARSER_RUN;
			dolby->fileMdInSize -= roffset;
			ddrptr += roffset;
			DV_Flush();
		}


		// TODO:  Composer meta data parsing to software structure
		p_CompMd = (rpu_ext_config_fixpt_main_t *)g_compMDAddr;
		//p_CompMd = g_compMDAddr + (sizeof(rpu_ext_config_fixpt_main_t)*g_frameCtrlNo);


		gVoFrame1AddrAssign = gVoFrameAddr1;
		gVoFrame2AddrAssign = gVoFrameAddr2;
		if (p_CompMd->rpu_BL_bit_depth == 8)
			Fmt_Cvt_8YUV420_to_8BLK444(gFrame1Infp, gFrame1Outfp, gFrame2Outfp, dolby->ColNumTtl, dolby->RowNumTtl);
		else if (p_CompMd->rpu_BL_bit_depth == 10)
			Fmt_Cvt_10YUV420_to_10BLK444(gFrame1Infp, gFrame1Outfp, gFrame2Outfp, dolby->ColNumTtl, dolby->RowNumTtl);
		else {
			printk("unknown BL bit type \n");
			gFrame1Infp = gFrame2Infp = gFrame1Outfp = gFrame2Outfp = gMDInfp = NULL;
			return ;
		}
		gVoFrame1AddrAssign = gVoFrameAddr3;
		gVoFrame2AddrAssign = gVoFrameAddr4;
		if (p_CompMd->rpu_EL_bit_depth == 8)
			Fmt_Cvt_8YUV420_to_8BLK444(gFrame2Infp, gFrame1Outfp, gFrame2Outfp, el_width, el_height);
		else if (p_CompMd->rpu_EL_bit_depth == 10)
			Fmt_Cvt_10YUV420_to_10BLK444(gFrame2Infp, gFrame1Outfp, gFrame2Outfp, el_width, el_height);
		else {
			printk("unknown EL bit type \n");
			gFrame1Infp = gFrame2Infp = gFrame1Outfp = gFrame2Outfp = gMDInfp = NULL;
			return ;
		}



		// TODO:  DM meta data parsing to software structure
		//mdptr = (char *)g_dmMDAddr;
		//for (mdidx = 0; mdidx <= g_frameCtrlNo; mdidx++) {
		//	ret = dm_metadata_2_dm_param((dm_metadata_base_t *)g_dmMDAddr, &mds_ext);
		//	mdptr += ret;
		//}
		//mdsChg = CheckMds(&mds_ext, &h_dm->mdsExt);	// modified by smfan
		//CommitMds(&mds_ext, h_dm);


		// TODO: dolby_v_top setting
		DolbyVTopRbusSet(dolby, DV_COMPOSER_CRF_MODE);
		top_d_buf_reg.regValue = rtd_inl(DOLBY_V_TOP_TOP_D_BUF_reg);
		top_d_buf_reg.dolby_double_apply = 1;
		rtd_outl(DOLBY_V_TOP_TOP_D_BUF_reg, top_d_buf_reg.regValue);
		//while (DOLBY_V_TOP_TOP_D_BUF_get_dolby_double_apply(rtd_inl(DOLBY_V_TOP_TOP_D_BUF_reg))) {
		//	msleep(1);
		//}

		// TODO:  DM setting
		//DMRbusSet(DV_NORMAL_MODE, (mdsChg==MDS_CHG_NONE)?0:1);

		// TODO:  Composer setting
		ComposerRbusSet(p_CompMd);
		comp_db_ctr_reg.regValue = rtd_inl(DHDR_V_COMPOSER_Composer_db_ctrl_reg);
		comp_db_ctr_reg.composer_db_apply = 1;
		rtd_outl(DHDR_V_COMPOSER_Composer_db_ctrl_reg, comp_db_ctr_reg.regValue);
		//msleep(40);



		// set some vo register
		// VP_DC_IDX_VOUT_V2_Y_CURR = 0x78
		// VP_DC_IDX_VOUT_V2_C_CURR = 0x79
		// VP_DC_IDX_VOUT_V2_Y_CURR2 = 0x7a
		// VP_DC_IDX_VOUT_V2_C_CURR2 = 0x7b
		if (p_CompMd->rpu_BL_bit_depth == 8) {
#ifdef CONFIG_SUPPORT_SCALER
			vodma_DCU_Set(VP_DC_IDX_VOUT_V2_Y_CURR, dvr_to_phys((void *)gVoFrameAddr1), dolby->ColNumTtl, 0, 0);
			vodma_DCU_Set(VP_DC_IDX_VOUT_V2_C_CURR, dvr_to_phys((void *)gVoFrameAddr2), dolby->ColNumTtl, 0, 0);
#endif
		} else {
#ifdef CONFIG_SUPPORT_SCALER
			vodma_DCU_Set(VP_DC_IDX_VOUT_V2_Y_CURR, dvr_to_phys((void *)gVoFrameAddr1), ((dolby->ColNumTtl*10)/8), 0, 0);
			vodma_DCU_Set(VP_DC_IDX_VOUT_V2_C_CURR, dvr_to_phys((void *)gVoFrameAddr2), ((dolby->ColNumTtl*10)/8), 0, 0);
#endif
		}
		if (p_CompMd->rpu_EL_bit_depth == 8) {
#ifdef CONFIG_SUPPORT_SCALER
			vodma_DCU_Set(VP_DC_IDX_VOUT_V2_Y_CURR2, dvr_to_phys((void *)gVoFrameAddr3), el_width, 0, 0);
			vodma_DCU_Set(VP_DC_IDX_VOUT_V2_C_CURR2, dvr_to_phys((void *)gVoFrameAddr4), el_width, 0, 0);
#endif
		} else {
#ifdef CONFIG_SUPPORT_SCALER
			vodma_DCU_Set(VP_DC_IDX_VOUT_V2_Y_CURR2, dvr_to_phys((void *)gVoFrameAddr3), ((el_width*10)/8), 0, 0);
			vodma_DCU_Set(VP_DC_IDX_VOUT_V2_C_CURR2, dvr_to_phys((void *)gVoFrameAddr4), ((el_width*10)/8), 0, 0);
#endif
		}


#if 1
		//rtd_outl(VODMA_VODMA_V1SGEN_reg, rtd_inl(VODMA_VODMA_V1SGEN_reg) & ~(1<<30));	// release vo reset
		//rtd_outl(VODMA2_VODMA2_V1SGEN_reg, rtd_inl(VODMA2_VODMA2_V1SGEN_reg) & ~(1<<30));	// release vo reset

		// vo bg setting
		rtd_outl(VODMA_VODMA_V1VGIP_HBG_reg, rtd_inl(VODMA_VODMA_V1VGIP_HACT_reg));
		rtd_outl(VODMA2_VODMA2_V1VGIP_HBG_reg, rtd_inl(VODMA2_VODMA2_V1VGIP_HACT_reg));

		rtd_outl(VODMA_VODMA_V1VGIP_VBG1_reg, rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg));
		rtd_outl(VODMA2_VODMA2_V1VGIP_VBG1_reg, rtd_inl(VODMA2_VODMA2_V1VGIP_VACT1_reg));
		rtd_outl(VODMA_VODMA_V1VGIP_VBG2_reg, rtd_inl(VODMA_VODMA_V1VGIP_VBG1_reg));
		rtd_outl(VODMA2_VODMA2_V1VGIP_VBG2_reg, rtd_inl(VODMA2_VODMA2_V1VGIP_VBG1_reg));

		// passive mode  for VO2
		rtd_outl(VODMA2_VODMA2_PVS0_Gen_reg, rtd_inl(VODMA2_VODMA2_PVS0_Gen_reg) |
						VODMA2_VODMA2_PVS0_Gen_iv_src_sel(7));
		rtd_outl(VODMA2_VODMA2_dma_option_reg, rtd_inl(VODMA2_VODMA2_dma_option_reg) |
						VODMA2_VODMA2_dma_option_vo_out_passivemode(1));


		if (iv_act_sta == 0) {

			// chanage vgip v_sta once
			iv_act_sta = VGIP_VGIP_CHN1_ACT_VSTA_Length_get_ch1_iv_act_sta(rtd_inl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg));
			rtd_outl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg, (rtd_inl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg) & ~VGIP_VGIP_CHN1_ACT_VSTA_Length_ch1_iv_act_sta_mask) |
						VGIP_VGIP_CHN1_ACT_VSTA_Length_ch1_iv_act_sta(iv_act_sta-6));
		}

		rtd_outl(VODMA_VODMA_VGIP_INTPOS_reg, 0x00);
		rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, 0x00);
		rtd_outl(VODMA_VODMA_V1_DSIZE_reg, (dolby->ColNumTtl<<16) | (dolby->RowNumTtl));
		rtd_outl(VODMA_VODMA_VD1_ADS_reg, (VP_DC_IDX_VOUT_V2_C_CURR<<16) | (VP_DC_IDX_VOUT_V2_C_CURR<<8) | VP_DC_IDX_VOUT_V2_Y_CURR);

		rtd_outl(VODMA_VODMA_dma_option_reg, rtd_inl(VODMA_VODMA_dma_option_reg) &
														~(VODMA_VODMA_dma_option_blockmode_sel_c_mask|VODMA_VODMA_dma_option_blockmode_sel_y_mask));
		if (p_CompMd->rpu_BL_bit_depth == 10)
			rtd_outl(VODMA_VODMA_dma_option_reg, rtd_inl(VODMA_VODMA_dma_option_reg) |
							VODMA_VODMA_dma_option_blockmode_sel_c(3) |
							VODMA_VODMA_dma_option_blockmode_sel_y(3));
		rtd_outl(VODMA_VODMA_V1_DCFG_reg, 0x00180f4b);

		rtd_outl(VODMA2_VODMA2_V1_DSIZE_reg, (el_width<<16) | (el_height));
		rtd_outl(VODMA2_VODMA2_VD1_ADS_reg, (VP_DC_IDX_VOUT_V2_C_CURR2<<16) | (VP_DC_IDX_VOUT_V2_C_CURR2<<8) | VP_DC_IDX_VOUT_V2_Y_CURR2);

		rtd_outl(VODMA2_VODMA2_dma_option_reg, rtd_inl(VODMA2_VODMA2_dma_option_reg) &
														~(VODMA2_VODMA2_dma_option_blockmode_sel_c_mask|VODMA2_VODMA2_dma_option_blockmode_sel_y_mask));
		if (p_CompMd->rpu_EL_bit_depth == 10)
			rtd_outl(VODMA2_VODMA2_dma_option_reg, rtd_inl(VODMA2_VODMA2_dma_option_reg) |
							VODMA2_VODMA2_dma_option_blockmode_sel_c(3) |
							VODMA2_VODMA2_dma_option_blockmode_sel_y(3));


		// for VODMA2 underflow, which cause the output image right side and left side swap..
		if (VODMA2_VODMA2_CLKGEN_get_vodma_clk_div_n(rtd_inl(VODMA2_VODMA2_CLKGEN_reg)) > 1)
			rtd_outl(VODMA2_VODMA2_CLKGEN_reg, (rtd_inl(VODMA2_VODMA2_CLKGEN_reg) & ~VODMA2_VODMA2_CLKGEN_vodma_clk_div_n_mask) |
							VODMA2_VODMA2_CLKGEN_vodma_clk_div_n(1));


		rtd_outl(VODMA2_VODMA2_V1_DCFG_reg, 0x00180f4b);
#endif

	}

#if 0
	// disable double buffer
	msleep(40);
	dm_double_buf_ctrl_reg.dm_db_en = 0;
	comp_db_ctr_reg.composer_db_en = 0;
	top_d_buf_reg.dolby_double_en = 0;
	rtd_outl(DOLBY_V_TOP_TOP_D_BUF_reg, top_d_buf_reg.regValue);
	rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
	rtd_outl(DHDR_V_COMPOSER_Composer_db_ctrl_reg, comp_db_ctr_reg.regValue);
	msleep(40);
#endif
#if 0
		// write md to file
		if (gFrame1Outfp != NULL) {
			file_sync(gFrame1Outfp);
			file_close(gFrame1Outfp);
			gFrame1Outfp = NULL;
		}
		gFrame1Outfp = file_open("/var/Cblock_ddr.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
		ddrptr = gVoFrameAddr2;
		for (idx=0; idx<1179648; idx++) {
			file_write(ddrptr, 1, idx, gFrame1Outfp);
			ddrptr++;
		}
		if (gFrame1Outfp != NULL) {
			file_sync(gFrame1Outfp);
			file_close(gFrame1Outfp);
			gFrame1Outfp = NULL;
		}
		gFrame1Outfp = file_open("/var/Yblock_ddr.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
		ddrptr = gVoFrameAddr1;
		for (idx=0; idx<2228224; idx++) {
			file_write(ddrptr, 1, idx, gFrame1Outfp);
			ddrptr++;
		}
#endif


#if 0	// output rpu data  ->  real composer & DM metadata
	// write md to file
	if (gFrame1Outfp != NULL) {
		file_sync(gFrame1Outfp);
		file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}
	gFrame1Outfp = file_open("/var/1dm.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
	ddrptr = g_dmMDAddr;
	for (idx=0; idx<9840; idx++) {
		file_write(ddrptr, 1, idx, gFrame1Outfp);
		ddrptr++;
	}
	if (gFrame1Outfp != NULL) {
		file_sync(gFrame1Outfp);
		file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}
	gFrame1Outfp = file_open("/var/1comp.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
	ddrptr = g_compMDAddr;
	for (idx=0; idx<216000; idx++) {
		file_write(ddrptr, 1, idx, gFrame1Outfp);
		ddrptr++;
	}
#endif


	if (gFrame1Infp != NULL) {
	    file_sync(gFrame1Infp);
	    file_close(gFrame1Infp);
		gFrame1Infp = NULL;
	}
	if (gFrame2Infp != NULL) {
	    file_sync(gFrame2Infp);
	    file_close(gFrame2Infp);
		gFrame2Infp = NULL;
	}
	if (gFrame1Outfp != NULL) {
	    file_sync(gFrame1Outfp);
	    file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}
	if (gFrame2Outfp != NULL) {
	    file_sync(gFrame2Outfp);
	    file_close(gFrame2Outfp);
		gFrame2Outfp = NULL;
	}
	if (gMDInfp != NULL) {
	    file_sync(gMDInfp);
	    file_close(gMDInfp);
		gMDInfp = NULL;
	}

#endif

}

#if 1
void DM_CRF_Dump_TestFlow(DOLBYVISION_PATTERN *dolby)
{
	int idx = 0, chkcnt = 0;
	dm_hdr_double_buffer_ctrl_RBUS dm_double_buf_ctrl_reg;
	hdr_all_top_top_ctl_RBUS hdr_all_top_top_ctl_reg;
	hdr_all_top_top_d_buf_RBUS hdr_all_top_top_d_buf_reg;
	static unsigned int voVsync_workaround = 0;
	mdomain_cap_cap_in1_enable_RBUS cap_in1_enable_reg;
	//mdomain_cap_cap_in2_enable_RBUS cap_in2_enable_reg;
	unsigned int mcapAddr/*, AddrTmp 20180315 pinyen remove unused DV register*/;
	unsigned int Capture_byte_swap;
	unsigned int vgipIntSave, dolbyModeSave,dolbyRegSave;

	/*20160923, base on official-368,baker
	    when disable compress mode, need to force 2D. cause memory size isn't enough
	 */
	rtd_clearbits(DI_IM_DI_WEAVE_WINDOW_CONTROL_reg, DI_IM_DI_WEAVE_WINDOW_CONTROL_force2d_mask);


	/* disable VGIP interrupt */
	rtd_outl(VGIP_VGIP_CHN1_CTRL_reg, rtd_inl(VGIP_VGIP_CHN1_CTRL_reg) &
		~(VGIP_VGIP_CHN1_CTRL_ch1_vact_start_ie_mask | VGIP_VGIP_CHN1_CTRL_ch1_vact_end_ie_mask));
	rtd_outl(SUB_VGIP_VGIP_CHN2_CTRL_reg, rtd_inl(SUB_VGIP_VGIP_CHN2_CTRL_reg) &
		~(SUB_VGIP_VGIP_CHN2_CTRL_ch2_vact_start_ie_mask | SUB_VGIP_VGIP_CHN2_CTRL_ch2_vact_end_ie_mask));
	vgipIntSave = rtd_inl(VGIP_INT_CTL_reg);
	rtd_outl(VGIP_INT_CTL_reg, 0);

	/* disable VO interrupt */
	rtd_outl(VODMA_VODMA_VGIP_INTPOS_reg, 0x0);	// disable interrupt
	rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, 0x0);	// disable interrupt

	// switch dolby_v_top's dolby_mode
	msleep(100);
	hdr_all_top_top_ctl_reg.regValue = rtd_inl(HDR_ALL_TOP_TOP_CTL_reg);
	dolbyRegSave = hdr_all_top_top_ctl_reg.regValue;

	dolbyModeSave = hdr_all_top_top_ctl_reg.dolby_mode;
	hdr_all_top_top_ctl_reg.en_422to444_1 = 0;
	hdr_all_top_top_ctl_reg.end_out_mux = 0;
	if (dolby->dolby_mode)
		hdr_all_top_top_ctl_reg.dolby_mode = dolby->dolby_mode;

	if (dolby->dolby_mode == DV_NORMAL_MODE) {
        //enable reorder hw to ctf
        hdr_all_top_top_ctl_reg.end_out_mux = 1;
	}

	if (dolby->dolby_mode == DV_DM_CRF_422_MODE) {
		//enable reorder hw to ctf
		hdr_all_top_top_ctl_reg.hdmi_in_mux = 0;
	}

	rtd_outl(HDR_ALL_TOP_TOP_CTL_reg, hdr_all_top_top_ctl_reg.regValue);

	hdr_all_top_top_d_buf_reg.regValue = rtd_inl(HDR_ALL_TOP_TOP_D_BUF_reg);
	hdr_all_top_top_d_buf_reg.dolby_double_apply = 1;
	rtd_outl(HDR_ALL_TOP_TOP_D_BUF_reg, hdr_all_top_top_d_buf_reg.regValue);

	// disable dithering
	rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) & ~DM_dm_submodule_enable_dither_en_mask);
	dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
	dm_double_buf_ctrl_reg.dm_db_apply = 1;
	rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);

	msleep(1000);

	IdomainBypassForCRF(dolby);

#if 0
rtd_outl(0xb80241a0,rtd_inl(0xb80241a0) & ~(0x3));
rtd_outl(0xb8023c00,rtd_inl(0xb8023c00) & ~(0x1));
rtd_outl(0xb8025004,rtd_inl(0xb8025004) & ~(0x6));
rtd_outl(0xb8025090,rtd_inl(0xb8025090) & ~(0x1));
rtd_outl(0xb8022600,rtd_inl(0xb8022600) & ~(0x60));
#endif

	// test x frame
	for (idx = 0; idx < 1; idx++) {
		g_frameCtrlNo = dolby->frameNo;
		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
		dm_double_buf_ctrl_reg.dm_db_apply = 1;
		rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
		msleep(60);

		/* check SENSIO FIFO status */
		if ((strcmp(dolby->caseNum, "5022") == 0) ||
			(strcmp(dolby->caseNum, "5021") == 0) || (strcmp(dolby->caseNum, "5023") == 0) || /*(strcmp(dolby->caseNum, "5102a") == 0) ||
			(strcmp(dolby->caseNum, "5102b") == 0) || (strcmp(dolby->caseNum, "5102c") == 0) || (strcmp(dolby->caseNum, "5102d") == 0) ||
			(strcmp(dolby->caseNum, "5102e") == 0) || (strcmp(dolby->caseNum, "5102f") == 0) || (strcmp(dolby->caseNum, "5102g") == 0) ||
			(strcmp(dolby->caseNum, "5102h") == 0) || (strcmp(dolby->caseNum, "5102i") == 0) || (strcmp(dolby->caseNum, "5102j") == 0) ||*/
			(strcmp(dolby->caseNum, "5240") == 0) ||
			(dolby->dolby_mode == DV_DM_CRF_422_MODE))
		goto _SKIP_SENSIO_CHK_;

		rtd_outl(HDR_ALL_TOP_SENSIO_FIFO_STATUS_reg, 0x1);	// write one to clear
		chkcnt = 0;
		for (chkcnt = 0; chkcnt < 5; chkcnt++) {
			msleep(1000);
			/*20180315 pinyen remove unused DV register
			AddrTmp = DOLBY_V_TOP_SENSIO_FIFO_STATUS_get_sen_fifo_uf(rtd_inl(DOLBY_V_TOP_SENSIO_FIFO_STATUS_reg));
			if (AddrTmp == 0)
				break;
			*/
			rtd_outl(HDR_ALL_TOP_SENSIO_FIFO_STATUS_reg, 0x1);	// write one to clear
		}
		if (chkcnt == 5) {
			printk("%s, Dolby_V_top underflow...Pls Check !\n", __FUNCTION__);
			goto _DUMP_EXIT_;
		}
_SKIP_SENSIO_CHK_:
#if 0
	// enable M-cap
	cap_in1_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In1_enable_reg);
	cap_in1_enable_reg.in1_cap_enable = 1;
	//cap_in2_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In2_enable_reg);
	//cap_in2_enable_reg.in2_cap_enable = 1;
	rtd_outl(MDOMAIN_CAP_Cap_In1_enable_reg, cap_in1_enable_reg.regValue);
	//rtd_outl(MDOMAIN_CAP_Cap_In2_enable_reg, cap_in2_enable_reg.regValue);

	msleep(100);

	// record Capture_byte_swap setting and set No byte swap
	//baker no byteswap
	Capture_byte_swap = rtd_inl(MDOMAIN_CAP_Capture1_byte_swap_reg);
	rtd_outl(MDOMAIN_CAP_Capture1_byte_swap_reg, (rtd_inl(MDOMAIN_CAP_Capture1_byte_swap_reg)&~(0xf)) | 0x7);	// byte swap 1+2+4

	// wait M-capture done
	// wait data end
	flush_cache_all();
	flush_cache_all();
#ifdef CONFIG_SUPPORT_SCALER
	WaitFor_DEN_STOP_Done();
	WaitFor_DEN_STOP_Done();
	WaitFor_DEN_STOP_Done();
	WaitFor_DEN_STOP_Done();
#endif
	flush_cache_all();
	flush_cache_all();
	flush_cache_all();
	msleep(500);

	// disable M-cap
	cap_in1_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In1_enable_reg);
	cap_in1_enable_reg.in1_cap_enable = 0;
	//cap_in2_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In2_enable_reg);
	//cap_in2_enable_reg.in2_cap_enable = 0;
	rtd_outl(MDOMAIN_CAP_Cap_In1_enable_reg, cap_in1_enable_reg.regValue);
	//rtd_outl(MDOMAIN_CAP_Cap_In2_enable_reg, cap_in2_enable_reg.regValue);
#endif
	Capture_byte_swap = rtd_inl(MDOMAIN_CAP_Capture1_byte_swap_reg);
	rtd_outl(MDOMAIN_CAP_Capture1_byte_swap_reg, (rtd_inl(MDOMAIN_CAP_Capture1_byte_swap_reg)&~(0xf)) | 0x7);	// byte swap 1+2+4
	drv_memory_wait_cap_last_write_done(SLR_MAIN_DISPLAY, 3, FALSE);
	rtd_outl(MDOMAIN_CAP_DDR_In1Ctrl_reg, rtd_inl(MDOMAIN_CAP_DDR_In1Ctrl_reg)|0x2);
	// get M-cap buffer address
	//fox add comment , need to check mcapAddr and mcapAddr2
	mcapAddr = g_IDKDumpAddr;
	//mcapAddr2 = g_IDKDumpAddr2;
	printk("%s, g_IDKDumpAddr=0x%x, g_IDKDumpAddr2=0x%x \n", __FUNCTION__, g_IDKDumpAddr, g_IDKDumpAddr2);

#if 0

	if(dolby->ColNumTtl <=1920)
		dv_dump_agent_add_task_to_queue(rtd_inl(MDOMAIN_CAP_DDR_In1_3rdAddr_reg), DUMP_2K, dolby);
	else
		dv_dump_agent_add_task_to_queue(rtd_inl(MDOMAIN_CAP_DDR_In1_3rdAddr_reg), DUMP_4K, dolby);

#else
	if (mcapAddr == 0)
		//mcapAddr = (unsigned int)dvr_remap_cached_memory(rtd_inl(MDOMAIN_CAP_DDR_In1Addr_reg), 1920*1080*32,__builtin_return_address(0));
		mcapAddr = (unsigned int)dvr_remap_cached_memory(rtd_inl(MDOMAIN_CAP_DDR_In1_3rdAddr_reg), 1920*1080*32,__builtin_return_address(0));

	//TotalCapSize = memory_get_capture_size(Scaler_DispGetInputInfo(SLR_INPUT_DISPLAY), MEMCAPTYPE_LINE);
	//TotalCapSize = (TotalCapSize<<3) * dolby->RowNumTtl;
	//mcapAddr = dvr_remap_uncached_memory(rtd_inl(MDOMAIN_CAP_DDR_In1Addr_reg),
	//													TotalCapSize,
	//													__builtin_return_address(0));

	printk("%s, physical mcapAddr=0x%x, mapAddr2=0x%x \n", __FUNCTION__, rtd_inl(MDOMAIN_CAP_DDR_In1Addr_reg), rtd_inl(MDOMAIN_CAP_DDR_In2Addr_reg));
	printk("%s, mcapAddr=0x%x, Capture_byte_swap=0x%x \n", __FUNCTION__, mcapAddr, Capture_byte_swap);
    //capture 422 12 bit to file, if capture 422 10bit , please implement another function
    Fmt_DMCRF422_Mcap_to_DM(mcapAddr, dolby);
	//baker
	//Fmt_DMCRFOTT_Mcap_to_DM(mcapAddr, mcapAddr2, dolby);
#endif
	// restore byte swap
	rtd_outl(MDOMAIN_CAP_Capture1_byte_swap_reg, Capture_byte_swap);

	// enable M-cap
	cap_in1_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In1_enable_reg);
	cap_in1_enable_reg.in1_cap_enable = 1;
	//cap_in2_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In2_enable_reg);
	//cap_in2_enable_reg.in2_cap_enable = 1;
	rtd_outl(MDOMAIN_CAP_Cap_In1_enable_reg, cap_in1_enable_reg.regValue);
	//rtd_outl(MDOMAIN_CAP_Cap_In2_enable_reg, cap_in2_enable_reg.regValue);

#if 1
	// free virtual memory
	dvr_unmap_memory((void*)mcapAddr, 1920*1080*32);
#endif
	voVsync_workaround = 1;
    }
_DUMP_EXIT_:
	/* restore dolby_mode */
	hdr_all_top_top_ctl_reg.regValue = dolbyRegSave;
	hdr_all_top_top_ctl_reg.dolby_mode = dolbyModeSave;
	rtd_outl(HDR_ALL_TOP_TOP_CTL_reg, hdr_all_top_top_ctl_reg.regValue);

	hdr_all_top_top_d_buf_reg.regValue = rtd_inl(HDR_ALL_TOP_TOP_D_BUF_reg);
	hdr_all_top_top_d_buf_reg.dolby_double_apply = 1;
	rtd_outl(HDR_ALL_TOP_TOP_D_BUF_reg, hdr_all_top_top_d_buf_reg.regValue);
	// enable dithering
	rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) | DM_dm_submodule_enable_dither_en_mask);
	dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
	dm_double_buf_ctrl_reg.dm_db_apply = 1;
	rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);


	msleep(100);

	/* restore VGIP interrupt */
	rtd_outl(0xb8022210, rtd_inl(0xb8022210) | (0x3<<24));
	rtd_outl(0xb8022710, rtd_inl(0xb8022710) | (0x3<<24));
	rtd_outl(0xb802228C, vgipIntSave);

	msleep(100);

	/* enable VO interrupt */
	rtd_outl(VODMA_VODMA_VGIP_INTPOS_reg, 0x06);	// enable interrupt

	if ((strcmp(dolby->caseNum, "5022") == 0) ||
			(strcmp(dolby->caseNum, "5021") == 0) || (strcmp(dolby->caseNum, "5023") == 0) || (strcmp(dolby->caseNum, "5102a") == 0) ||
			(strcmp(dolby->caseNum, "5102b") == 0) || (strcmp(dolby->caseNum, "5102c") == 0) || (strcmp(dolby->caseNum, "5102d") == 0) ||
			(strcmp(dolby->caseNum, "5102e") == 0) || (strcmp(dolby->caseNum, "5102f") == 0) || (strcmp(dolby->caseNum, "5102g") == 0) ||
			(strcmp(dolby->caseNum, "5102h") == 0) || (strcmp(dolby->caseNum, "5102i") == 0) || (strcmp(dolby->caseNum, "5102j") == 0) ||
			(strcmp(dolby->caseNum, "5240") == 0))
		rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, 0x00);	// diable interrupt
	else
		rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, 0x07);	// enable interrupt

	msleep(500);

	if (gFrame1Infp != NULL) {
		file_sync(gFrame1Infp);
		file_close(gFrame1Infp);
		gFrame1Infp = NULL;
	}
	if (gFrame1Outfp != NULL) {
		file_sync(gFrame1Outfp);
		file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}
	if (gMDInfp != NULL) {
		file_sync(gMDInfp);
		file_close(gMDInfp);
		gMDInfp = NULL;
	}

	rtd_outl(MDOMAIN_CAP_DDR_In1Ctrl_reg, rtd_inl(MDOMAIN_CAP_DDR_In1Ctrl_reg)&0xfffffffd);
}
#else
void DM_CRF_Dump_TestFlow(DOLBYVISION_PATTERN *dolby)
{
#if 1//#ifdef CONFIG_CUSTOMER_TV006//Remove by will

	int idx = 0, chkcnt = 0;
	dm_hdr_dm_double_buffer_ctrl_RBUS dm_double_buf_ctrl_reg;
	dolby_v_top_top_ctl_RBUS top_ctl_reg;
	dolby_v_top_top_d_buf_RBUS top_d_buf_reg;
	static unsigned int voVsync_workaround = 0;
	mdomain_cap_cap_in1_enable_RBUS cap_in1_enable_reg;
	mdomain_cap_cap_in2_enable_RBUS cap_in2_enable_reg;
	unsigned int mcapAddr, mcapAddr2, AddrTmp;
	unsigned int Capture_byte_swap;
	//unsigned int TotalCapSize;
	unsigned int vgipIntSave, dolbyModeSave;

/*
	dm_cfg.frmPri.PxlDefOut.pxlClr = 0;
	dm_cfg.frmPri.PxlDefOut.pxlChrm = 1;
	dm_cfg.frmPri.PxlDefOut.pxlDtp = 0;
	dm_cfg.frmPri.PxlDefOut.pxlBdp = dolby->pxlBdp;
	dm_cfg.frmPri.PxlDefOut.pxlWeav = 2;
	dm_cfg.frmPri.PxlDefOut.pxlLoc = 0;
	dm_cfg.frmPri.PxlDefOut.pxlEotf = 0;
	dm_cfg.frmPri.PxlDefOut.pxlRng = 1;
	dm_cfg.frmPri.RowNumTtl = dolby->RowNumTtl;
	dm_cfg.frmPri.ColNumTtl = dolby->ColNumTtl;//3

	SanityCheckDmCfg(&dm_cfg);
*/

	/* disable VGIP interrupt */
	rtd_outl(0xb8022210, rtd_inl(0xb8022210) & ~(0x3<<24));
	rtd_outl(0xb8022710, rtd_inl(0xb8022710) & ~(0x3<<24));
	vgipIntSave = rtd_inl(0xb802228C);
	rtd_outl(0xb802228C, 0);
	msleep(100);

	/* disable VO interrupt */
	rtd_outl(VODMA_VODMA_VGIP_INTPOS_reg, 0x00);	// disable interrupt
	rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, 0x00);	// disable interrupt

	if (dolby->dolby_mode == DV_NORMAL_MODE) {
		// disable function of display main
		rtd_outl(MDOMAIN_DISP_Disp_main_enable_reg, 0x00);
		// disable function of display Sub
		rtd_outl(MDOMAIN_DISP_Disp_sub_enable_reg, 0x00);
		// apply
		rtd_outl(MDOMAIN_DISP_DDR_MainSubCtrl_reg, rtd_inl(MDOMAIN_DISP_DDR_MainSubCtrl_reg) | (1<<0) | (1<<16));
	}


	// switch dolby_v_top's dolby_mode
	msleep(100);
	top_ctl_reg.regValue = rtd_inl(DOLBY_V_TOP_TOP_CTL_reg);
	dolbyModeSave = top_ctl_reg.dolby_mode;
	if (dolby->dolby_mode)
		top_ctl_reg.dolby_mode = dolby->dolby_mode;
	rtd_outl(DOLBY_V_TOP_TOP_CTL_reg, top_ctl_reg.regValue);
	top_d_buf_reg.regValue = rtd_inl(DOLBY_V_TOP_TOP_D_BUF_reg);
	top_d_buf_reg.dolby_double_apply = 1;
	rtd_outl(DOLBY_V_TOP_TOP_D_BUF_reg, top_d_buf_reg.regValue);
	msleep(60);


	IdomainBypassForCRF(dolby);


#if 0
	if (dolby->ColNumTtl == 1920 && dolby->RowNumTtl == 1080) {
		rtd_outl(VODMA_VODMA_V1VGIP_VACT1_reg, 0x0447000f/*0x044a0012*/);
		if (top_ctl_reg.dolby_ratio)	// 4:1
			rtd_outl(VODMA2_VODMA2_V1VGIP_VACT1_reg, 0x022b000f);
		else
			rtd_outl(VODMA2_VODMA2_V1VGIP_VACT1_reg, 0x0447000f);
	}
#endif

	// test x frame
	for (idx = 0; idx < 1; idx++) {

		g_frameCtrlNo = dolby->frameNo;

		// disable dithering
		rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) & ~DM_dm_submodule_enable_dither_en_mask);

		if (dolby->dolby_mode == DV_NORMAL_MODE) {
			VIDEO_WID_T wid = VIDEO_WID_0;
			VIDEO_RECT_T videoSize;

			extern unsigned char rtk_hal_vsc_GetInputRegion(VIDEO_WID_T wid, VIDEO_RECT_T * pinregion);
			extern unsigned char rtk_hal_vsc_SetInputRegion(VIDEO_WID_T wid, VIDEO_RECT_T  inregion);
			extern unsigned char rtk_hal_vsc_GetOutputRegion(VIDEO_WID_T wid, VIDEO_RECT_T * poutregion);
			extern unsigned char rtk_hal_vsc_SetOutputRegion(VIDEO_WID_T wid, VIDEO_RECT_T outregion);

			wid = VIDEO_WID_0;
#ifdef CONFIG_SUPPORT_SCALER
			rtk_hal_vsc_GetInputRegion(wid, &videoSize);
#endif
			printk(KERN_EMERG"%s, input videoSize(x,y,w,h)=%d,%d,%d,%d \n", __FUNCTION__, videoSize.x, videoSize.y, videoSize.w, videoSize.h);
			wid = VIDEO_WID_1;
#ifdef CONFIG_SUPPORT_SCALER
			rtk_hal_vsc_SetInputRegion(wid, videoSize);
#endif
			wid = VIDEO_WID_0;
#ifdef CONFIG_SUPPORT_SCALER
			rtk_hal_vsc_GetOutputRegion(wid, &videoSize);
#endif
			printk(KERN_EMERG"%s, output videoSize(x,y,w,h)=%d,%d,%d,%d \n", __FUNCTION__, videoSize.x, videoSize.y, videoSize.w, videoSize.h);
			wid = VIDEO_WID_1;
#ifdef CONFIG_SUPPORT_SCALER
			rtk_hal_vsc_SetOutputRegion(wid, videoSize);
#endif
			if (voVsync_workaround == 0)
				msleep(1100);

#ifdef DOLBLY_12B_MODE
			// Input select for uzd2: 0: Ch2, DI bypass
			rtd_outl(VGIP_Data_Path_Select_reg, rtd_inl(VGIP_Data_Path_Select_reg) & ~(0x3<<8));

			// disable ICH2 color conversion
			rtd_outl(RGB2YUV_ICH2_RGB2YUV_CTRL_reg, rtd_inl(RGB2YUV_ICH2_RGB2YUV_CTRL_reg) & ~0x3);

			// disable ICH2_422to444_CTRL
			rtd_outl(RGB2YUV_ICH2_422to444_CTRL_reg, rtd_inl(RGB2YUV_ICH2_422to444_CTRL_reg) & ~1);

			// disable 444to422; No needed to disable for data dump
			//rtd_outl(DM_Format_444to422_reg, 0x1);

			// lbuf_disable : eline buffer disable
			rtd_outl(IEDGE_SMOOTH_EDGESMOOTH_EX_CTRL_VADDR, rtd_inl(IEDGE_SMOOTH_EDGESMOOTH_EX_CTRL_VADDR) | (1<<16));

			// truncationCtrl = 0
			rtd_outl(0xb8025204, rtd_inl(0xb8025204) & ~(1<<24));

			// Sub VGIP timing
			rtd_outl(SUB_VGIP_VGIP_CHN2_ACT_HSTA_Width_reg, VODMA_VODMA_V1_DSIZE_get_p_y_len(rtd_inl(VODMA_VODMA_V1_DSIZE_reg)));
			rtd_outl(SUB_VGIP_VGIP_CHN2_ACT_VSTA_Length_reg, VODMA_VODMA_V1_DSIZE_get_p_y_nline(rtd_inl(VODMA_VODMA_V1_DSIZE_reg)));

			// VGIP's Main/Sub VGIP source select to VODMA1
			rtd_outl(VGIP_VGIP_CHN1_CTRL_reg, (rtd_inl(VGIP_VGIP_CHN1_CTRL_reg)&~(0x7<<28)) | (1<<30));
			rtd_outl(SUB_VGIP_VGIP_CHN2_CTRL_reg, (rtd_inl(SUB_VGIP_VGIP_CHN2_CTRL_reg)&~(0x7<<28)) | (1<<30) | (1<<31) | (1<<1));

			// enable dolby_12bit_mode
			rtd_outl(VGIP_V8_CTRL_reg, rtd_inl(VGIP_V8_CTRL_reg) | (1<<3));

			// SUB_use_main_block's use capture block, controlled by SUB_Source_sel
			rtd_outl(MDOMAIN_DISP_DDR_SubCtrl_reg, rtd_inl(MDOMAIN_DISP_DDR_SubCtrl_reg) | 0x1);

			// DDR_In2Ctrl's force sub capture block_sel = main capture block_sel
			rtd_outl(MDOMAIN_CAP_DDR_In2Ctrl_reg, rtd_inl(MDOMAIN_CAP_DDR_In2Ctrl_reg) | (1<<24));
#endif
			msleep(100);
		}

		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
		dm_double_buf_ctrl_reg.dm_db_apply = 1;
		rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
		msleep(60);


		IdomainBypassForCRF(dolby);

		rtd_outl(DOLBY_V_TOP_SENSIO_FIFO_STATUS_reg, 0x1);	// write one to clear

		/* check SENSIO FIFO status */
		if ((strcmp(dolby->caseNum, "5022") == 0) ||
			(strcmp(dolby->caseNum, "5021") == 0) || (strcmp(dolby->caseNum, "5023") == 0) || /*(strcmp(dolby->caseNum, "5102a") == 0) ||
			(strcmp(dolby->caseNum, "5102b") == 0) || (strcmp(dolby->caseNum, "5102c") == 0) || (strcmp(dolby->caseNum, "5102d") == 0) ||
			(strcmp(dolby->caseNum, "5102e") == 0) || (strcmp(dolby->caseNum, "5102f") == 0) || (strcmp(dolby->caseNum, "5102g") == 0) ||
			(strcmp(dolby->caseNum, "5102h") == 0) || (strcmp(dolby->caseNum, "5102i") == 0) || (strcmp(dolby->caseNum, "5102j") == 0) ||*/
			(strcmp(dolby->caseNum, "5240") == 0) ||
			(dolby->dolby_mode == DV_DM_CRF_422_MODE))
			goto _SKIP_SENSIO_CHK_;


		chkcnt = 0;
		for (chkcnt = 0; chkcnt < 5; chkcnt++) {
			msleep(1000);
			AddrTmp = DOLBY_V_TOP_SENSIO_FIFO_STATUS_get_sen_fifo_uf(rtd_inl(DOLBY_V_TOP_SENSIO_FIFO_STATUS_reg));
			if (AddrTmp == 0)
				break;
			rtd_outl(DOLBY_V_TOP_SENSIO_FIFO_STATUS_reg, 0x1);	// write one to clear
		}
		if (chkcnt == 5) {
			printk("%s, Dolby_V_top underflow...Pls Check !\n", __FUNCTION__);
			goto _DUMP_EXIT_;
		}
_SKIP_SENSIO_CHK_:

		// enable M-cap
		cap_in1_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In1_enable_reg);
		cap_in1_enable_reg.in1_cap_enable = 1;
		cap_in2_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In2_enable_reg);
		cap_in2_enable_reg.in2_cap_enable = 1;
		rtd_outl(MDOMAIN_CAP_Cap_In1_enable_reg, cap_in1_enable_reg.regValue);
		//rtd_outl(MDOMAIN_CAP_Cap_In2_enable_reg, cap_in2_enable_reg.regValue);

		msleep(100);

		// record Capture_byte_swap setting and set No byte swap
		Capture_byte_swap = rtd_inl(MDOMAIN_CAP_Capture1_byte_swap_reg);
		rtd_outl(MDOMAIN_CAP_Capture1_byte_swap_reg, (rtd_inl(MDOMAIN_CAP_Capture1_byte_swap_reg)&~(0xf)) | 0x7);	// byte swap 1+2+4

		// wait M-capture done
		// wait data end
		flush_cache_all();
		flush_cache_all();
#ifdef CONFIG_SUPPORT_SCALER
		WaitFor_DEN_STOP_Done();
		WaitFor_DEN_STOP_Done();
		WaitFor_DEN_STOP_Done();
		WaitFor_DEN_STOP_Done();
#endif
		flush_cache_all();
		flush_cache_all();
		flush_cache_all();
		msleep(500);

		// disable M-cap
		cap_in1_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In1_enable_reg);
		cap_in1_enable_reg.in1_cap_enable = 0;
		cap_in2_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In2_enable_reg);
		cap_in2_enable_reg.in2_cap_enable = 0;
		rtd_outl(MDOMAIN_CAP_Cap_In1_enable_reg, cap_in1_enable_reg.regValue);
		//rtd_outl(MDOMAIN_CAP_Cap_In2_enable_reg, cap_in2_enable_reg.regValue);

		// get M-cap buffer address
		mcapAddr = g_IDKDumpAddr;
		printk("%s, g_IDKDumpAddr=0x%x\n", __FUNCTION__, g_IDKDumpAddr);
		if (mcapAddr == 0)
			mcapAddr = (unsigned int)dvr_remap_cached_memory(rtd_inl(MDOMAIN_CAP_DDR_In1Addr_reg), 1920*1080*32,__builtin_return_address(0));

		//TotalCapSize = memory_get_capture_size(Scaler_DispGetInputInfo(SLR_INPUT_DISPLAY), MEMCAPTYPE_LINE);
		//TotalCapSize = (TotalCapSize<<3) * dolby->RowNumTtl;
		//mcapAddr = dvr_remap_uncached_memory(rtd_inl(MDOMAIN_CAP_DDR_In1Addr_reg),
		//													TotalCapSize,
		//													__builtin_return_address(0));

		printk("%s, physical mcapAddr=0x%x\n", __FUNCTION__, rtd_inl(MDOMAIN_CAP_DDR_In1Addr_reg));
		printk("%s, mcapAddr=0x%x, Capture_byte_swap=0x%x \n", __FUNCTION__, mcapAddr, Capture_byte_swap);


		// captured fmt transfers to DolbyVision DM Output fmt
		if (dolby->dolby_mode == DV_NORMAL_MODE)
			Fmt_DMCRFOTT_Mcap_to_DM(mcapAddr, mcapAddr2, dolby);
		else if (dolby->dolby_mode == DV_DM_CRF_422_MODE || dolby->dolby_mode == DV_DM_CRF_420_MODE)
			Fmt_DMCRF422_Mcap_to_DM(mcapAddr, dolby);
		else if (dolby->dolby_mode == DV_COMPOSER_CRF_MODE)
			Fmt_ComposerCRF_Mcap_to_DM(mcapAddr, dolby);


		// restore byte swap
		rtd_outl(MDOMAIN_CAP_Capture1_byte_swap_reg, Capture_byte_swap);

		// enable M-cap
		cap_in1_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In1_enable_reg);
		cap_in1_enable_reg.in1_cap_enable = 1;
		cap_in2_enable_reg.regValue = rtd_inl(MDOMAIN_CAP_Cap_In2_enable_reg);
		cap_in2_enable_reg.in2_cap_enable = 1;
		rtd_outl(MDOMAIN_CAP_Cap_In1_enable_reg, cap_in1_enable_reg.regValue);
		//rtd_outl(MDOMAIN_CAP_Cap_In2_enable_reg, cap_in2_enable_reg.regValue);

		// free virtual memory
		//dvr_unmap_memory(mcapAddr, TotalCapSize);

		voVsync_workaround = 1;

	}

_DUMP_EXIT_:

	if (dolby->dolby_mode == DV_NORMAL_MODE) {
		// enable function of display main
		//rtd_outl(MDOMAIN_DISP_Disp_main_enable_reg, 0x1);
		// enable function of display sub
		rtd_outl(MDOMAIN_DISP_Disp_sub_enable_reg, 0x1);
		// apply
		rtd_outl(MDOMAIN_DISP_DDR_MainSubCtrl_reg, rtd_inl(MDOMAIN_DISP_DDR_MainSubCtrl_reg) | (1<<0) | (1<<16));

		// disable dolby_12bit_mode for display next frame
		rtd_outl(VGIP_V8_CTRL_reg, rtd_inl(VGIP_V8_CTRL_reg) & ~(1<<3));
	}

	/* restore dolby_mode */
	top_ctl_reg.regValue = rtd_inl(DOLBY_V_TOP_TOP_CTL_reg);
	top_ctl_reg.dolby_mode = dolbyModeSave;
	rtd_outl(DOLBY_V_TOP_TOP_CTL_reg, top_ctl_reg.regValue);
	top_d_buf_reg.regValue = rtd_inl(DOLBY_V_TOP_TOP_D_BUF_reg);
	top_d_buf_reg.dolby_double_apply = 1;
	rtd_outl(DOLBY_V_TOP_TOP_D_BUF_reg, top_d_buf_reg.regValue);
	msleep(100);

	/* restore VGIP interrupt */
	rtd_outl(0xb8022210, rtd_inl(0xb8022210) | (0x3<<24));
	rtd_outl(0xb8022710, rtd_inl(0xb8022710) | (0x3<<24));
	rtd_outl(0xb802228C, vgipIntSave);

	msleep(100);

	/* enable VO interrupt */
	rtd_outl(VODMA_VODMA_VGIP_INTPOS_reg, 0x06);	// enable interrupt

	if ((strcmp(dolby->caseNum, "5022") == 0) ||
			(strcmp(dolby->caseNum, "5021") == 0) || (strcmp(dolby->caseNum, "5023") == 0) || (strcmp(dolby->caseNum, "5102a") == 0) ||
			(strcmp(dolby->caseNum, "5102b") == 0) || (strcmp(dolby->caseNum, "5102c") == 0) || (strcmp(dolby->caseNum, "5102d") == 0) ||
			(strcmp(dolby->caseNum, "5102e") == 0) || (strcmp(dolby->caseNum, "5102f") == 0) || (strcmp(dolby->caseNum, "5102g") == 0) ||
			(strcmp(dolby->caseNum, "5102h") == 0) || (strcmp(dolby->caseNum, "5102i") == 0) || (strcmp(dolby->caseNum, "5102j") == 0) ||
			(strcmp(dolby->caseNum, "5240") == 0))
		rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, 0x00);	// diable interrupt
	else
		rtd_outl(VODMA2_VODMA2_VGIP_INTPOS_reg, 0x07);	// enable interrupt

	msleep(500);

	if (gFrame1Infp != NULL) {
		file_sync(gFrame1Infp);
		file_close(gFrame1Infp);
		gFrame1Infp = NULL;
	}
	if (gFrame1Outfp != NULL) {
		file_sync(gFrame1Outfp);
		file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}
	if (gMDInfp != NULL) {
		file_sync(gMDInfp);
		file_close(gMDInfp);
		gMDInfp = NULL;
	}
#endif
}
#endif

void MetaData_WriteBack_file(char *name,unsigned char *buffer,int size,int end)
{
	// output rpu data	->	real composer & DM metadata
	int idx = 0;
	unsigned char *ddrptr;
	// write md to file
	if (gFrame1Outfp == NULL)
	     gFrame1Outfp = file_open(name, O_TRUNC | O_WRONLY | O_CREAT, 0);
    ddrptr = buffer;
	if (gFrame1Outfp != NULL){
        idx = globe_size;
		for (; idx < (size+globe_size); idx++) {
			file_write(ddrptr, 1, idx, gFrame1Outfp);
			file_sync(gFrame1Outfp);
			ddrptr++;
		}
		globe_size += size;
    }
    if(end == 1){
		if (gFrame1Outfp != NULL) {
			file_close(gFrame1Outfp);
			gFrame1Outfp = NULL;
			globe_size = 0;
		}
	}
}


void MetaData_WriteBack_Check(void)
{
	// output rpu data	->	real composer & DM metadata
	int idx = 0;
	unsigned char *ddrptr;
	unsigned int compmdsize = 540000;//5662800;
	unsigned int dmmdsize = 257972;

	// write md to file
	gFrame1Outfp = file_open("/var/1dm.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);

	//fix klocwork mantis 0136617
	if(gFrame1Outfp == NULL )
		return;
	ddrptr = (unsigned char *)g_dmMDAddr;
	for (idx=0; idx < dmmdsize; idx++) {
		file_write(ddrptr, 1, idx, gFrame1Outfp);
		ddrptr++;
	}
	if (gFrame1Outfp != NULL) {
		file_sync(gFrame1Outfp);
		file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}
	gFrame1Outfp = file_open("/var/1comp.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
	if(gFrame1Outfp == NULL )
		return;
	ddrptr = (unsigned char *)g_compMDAddr;
	for (idx=0; idx < compmdsize; idx++) {
		file_write(ddrptr, 1, idx, gFrame1Outfp);
		ddrptr++;
	}
	if (gFrame1Outfp != NULL) {
		file_sync(gFrame1Outfp);
		file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}

	if (gFrame2Outfp != NULL) {
		file_sync(gFrame2Outfp);
		file_close(gFrame2Outfp);
		gFrame2Outfp = NULL;
	}
}

void Rpu_Md_WriteBack_Check(unsigned int addr, int size)
{
	// output rpu data	->	real composer & DM metadata
	static int idxoffset = 0;
	int idx;
	unsigned char *ddrptr;
	// write md to file
	if (gFrame2Outfp == NULL) {
		gFrame2Outfp = file_open("/var/rpuMd.bin", O_TRUNC | O_WRONLY | O_CREAT, 0);
		idxoffset = 0;
	}

	ddrptr = (unsigned char *)addr;

	for (idx = 0; idx < size; idx++) {
		file_write(ddrptr, 1, idxoffset, gFrame2Outfp);
		idxoffset++;
		ddrptr++;
	}

	if (gFrame2Outfp != NULL)
		file_sync(gFrame2Outfp);

	//gFrame2Outfp = NULL;
	Warning("%s, done..\n", __FUNCTION__);
}


void Load_RpuMdTest(DOLBYVISION_PATTERN *dolby)
{
//#ifdef CONFIG_CUSTOMER_TV006
#if 0
	int idx = 0, mdidx = 0;
	unsigned long roffset = 0;
	char tmp;
	int ret;
	MdsChg_t mdsChg;
	unsigned char *ddrptr, *mdptr;
	dm_hdr_dm_double_buffer_ctrl_RBUS dm_double_buf_ctrl_reg;
	dhdr_v_composer_composer_db_ctrl_RBUS comp_db_ctr_reg;
	dolby_v_top_top_d_buf_RBUS top_d_buf_reg;
	rpu_ext_config_fixpt_main_t *p_CompMd;
	//DOLBYVISION_INIT_STRUCT dv_init;
	unsigned int lineCnt_in, vo_vstart, vo_vend;
	DmExecFxp_t *pDmExec = h_dm->pDmExec;


	dm_cfg.frmPri.PxlDefOut.pxlClr = 0;
	dm_cfg.frmPri.PxlDefOut.pxlChrm = 1;
	dm_cfg.frmPri.PxlDefOut.pxlDtp = 0;
	dm_cfg.frmPri.PxlDefOut.pxlBdp = dolby->pxlBdp;
	dm_cfg.frmPri.PxlDefOut.pxlWeav = 2;
	dm_cfg.frmPri.PxlDefOut.pxlLoc = 0;
	dm_cfg.frmPri.PxlDefOut.pxlEotf = 0;
	dm_cfg.frmPri.PxlDefOut.pxlRng = 1;
	dm_cfg.frmPri.RowNumTtl = dolby->RowNumTtl;
	dm_cfg.frmPri.ColNumTtl = dolby->ColNumTtl;
	h_dm->dmExec.runMode = dm_cfg.dmCtrl.RunMode = RUN_MODE_NORMAL;

	SanityCheckDmCfg(&dm_cfg);

	if (gMDInfp == NULL)
		gMDInfp = file_open(dolby->fileMdInpath, O_RDONLY, 0);
	if (gMDInfp == NULL) {
		printk("%s, gMDInfp file open failed \n", __FUNCTION__);
		gFrame1Infp = gFrame2Infp = gFrame1Outfp = gFrame2Outfp = gMDInfp = NULL;
		return ;
	}

	if (gRpuMDAddr == 0)
		gRpuMDAddr = (unsigned int)dvr_malloc_specific(RPU_MD_SIZE, GFP_DCU1);
	printk(KERN_EMERG"gRpuMDAddr=0x%x\n", (unsigned int)dvr_to_phys((void *)gRpuMDAddr));


	if (g_dmMDAddr == 0) {
		g_dmMDAddr = (unsigned int)dvr_malloc_specific(RPU_MD_SIZE, GFP_DCU2);
		if (g_dmMDAddr)
			DBG_PRINT(KERN_EMERG"physical g_dmMDAddr=0x%x\n", (unsigned int)dvr_to_phys((void *)g_dmMDAddr));
	}
	if (g_compMDAddr == 0) {
		g_compMDAddr = (unsigned int)dvr_malloc_specific(RPU_MD_SIZE, GFP_DCU2);
		if (g_compMDAddr)
			DBG_PRINT(KERN_EMERG"physical g_compMDAddr=0x%x\n", (unsigned int)dvr_to_phys((void *)g_compMDAddr));
	}

	printk("%s, check point 1 \n", __FUNCTION__);

	/* store MD data to DDR */
	ddrptr = (unsigned char *)gRpuMDAddr;
	for (idx = 0; idx < dolby->fileMdInSize; idx++) {
		file_read((unsigned char *)&tmp, 1, idx, gMDInfp);
		*ddrptr = tmp;
		ddrptr++;
	}

	printk("%s, check point 2 \n", __FUNCTION__);

	// test 5 frame
	for (idx = 0; idx <= dolby->frameNo; idx++) {

		g_frameCtrlNo = dolby->frameNo;

		// TODO: rpu data  ->  composer & DM metadata
		ddrptr = (unsigned char *)gRpuMDAddr;
		g_MD_parser_state = MD_PARSER_INIT;
		g_mdparser_output_type = 1;
		g_dvframeCnt = g_recordDMAddr = 0;	// output all metadata

		for (mdidx = 0; mdidx <= g_frameCtrlNo; mdidx++) {
			g_dvframeCnt = g_recordDMAddr = 0;	// output one metadata each frame in starting address

			roffset = metadata_parser_main(ddrptr, dolby->fileMdInSize, g_MD_parser_state);
			g_MD_parser_state = MD_PARSER_RUN;
			dolby->fileMdInSize -= roffset;
			ddrptr += roffset;


			if (roffset == 0 && g_more_eos_err && mdidx==g_frameCtrlNo) {
				g_MD_parser_state = MD_PARSER_EOS;
				roffset = metadata_parser_main(ddrptr, dolby->fileMdInSize, g_MD_parser_state);
				dolby->fileMdInSize -= roffset;
			}
			DV_Flush();
		}


		// TODO:  Composer meta data parsing to software structure
		p_CompMd = (rpu_ext_config_fixpt_main_t *)g_compMDAddr;
		//p_CompMd = g_compMDAddr + (sizeof(rpu_ext_config_fixpt_main_t)*g_frameCtrlNo);

		printk("%s, check point 5 \n", __FUNCTION__);

		// TODO:  DM meta data parsing to software structure
		mdptr = (char *)g_dmMDAddr;
		//for (mdidx = 0; mdidx <= g_frameCtrlNo; mdidx++) {
			ret = dm_metadata_2_dm_param((dm_metadata_base_t *)mdptr, &mds_ext);
			//mdptr += ret;
		//}
		mdsChg = CheckMds(&mds_ext, &h_dm->mdsExt);	// modified by smfan
		CommitMds(&mds_ext, h_dm);

		printk("%s, check point 6 \n", __FUNCTION__);

		// polling the front porch timing
		lineCnt_in = rtd_inl(VODMA_VODMA_LINE_ST_reg) & 0xfff;
		vo_vstart = rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) & 0xfff;
		vo_vend = (rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) >> 16) & 0xfff;
		while (lineCnt_in < (vo_vend-1)) {
			lineCnt_in = rtd_inl(VODMA_VODMA_LINE_ST_reg) & 0xfff;
			vo_vstart = rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) & 0xfff;
			vo_vend = (rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) >> 16) & 0xfff;
			Warning("%s, waiting for front porch \n", __FUNCTION__);
		}


		// TODO: dolby_v_top setting
		DolbyVTopRbusSet(dolby, DV_NORMAL_MODE);

		top_d_buf_reg.regValue = rtd_inl(DOLBY_V_TOP_TOP_D_BUF_reg);
		top_d_buf_reg.dolby_double_apply = 1;
		rtd_outl(DOLBY_V_TOP_TOP_D_BUF_reg, top_d_buf_reg.regValue);
		//while (DOLBY_V_TOP_TOP_D_BUF_get_dolby_double_apply(rtd_inl(DOLBY_V_TOP_TOP_D_BUF_reg))) {
		//	msleep(1);
		//}

		// TODO:  DM setting
		DMRbusSet(DV_NORMAL_MODE, (mdsChg==MDS_CHG_NONE)?0:1);
		// postpone the update B05 timing
		// B05 3D LUT
		DM_B05_Set(pDmExec->bwDmLut.lutMap, g_forceUpdate_3DLUT);

		// TODO:  Composer setting
		ComposerRbusSet(p_CompMd);


		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
		dm_double_buf_ctrl_reg.dm_db_apply = 1;
		comp_db_ctr_reg.regValue = rtd_inl(DHDR_V_COMPOSER_Composer_db_ctrl_reg);
		comp_db_ctr_reg.composer_db_apply = 1;

		rtd_outl(DHDR_V_COMPOSER_Composer_db_ctrl_reg, comp_db_ctr_reg.regValue);
		//msleep(40);
		rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
		//msleep(40);

		msleep(1000);
	}

	if (gFrame1Infp != NULL) {
	    file_sync(gFrame1Infp);
	    file_close(gFrame1Infp);
		gFrame1Infp = NULL;
	}
	if (gFrame2Infp != NULL) {
	    file_sync(gFrame2Infp);
	    file_close(gFrame2Infp);
		gFrame2Infp = NULL;
	}
	if (gFrame1Outfp != NULL) {
	    file_sync(gFrame1Outfp);
	    file_close(gFrame1Outfp);
		gFrame1Outfp = NULL;
	}
	if (gFrame2Outfp != NULL) {
	    file_sync(gFrame2Outfp);
	    file_close(gFrame2Outfp);
		gFrame2Outfp = NULL;
	}
	if (gMDInfp != NULL) {
	    file_sync(gMDInfp);
	    file_close(gMDInfp);
		gMDInfp = NULL;
	}
#endif

}


#if 0
/*
*	type:
*   0: old fixed 3D LUT
*   1: gdLutA
*   2: gdLutB
*/
int GMLUT_3DLUT_Update(char *path, uint32_t foffset, int type, E_DV_PICTURE_MODE uPicMode)
{//Type means different LUT
	unsigned int gmLUTAddr = 0, idx= 0;
	unsigned char *ddrptr, tmp;
	int ret;
	struct file *gmlutfp = NULL;


	if (gmLUTAddr == 0)
		gmLUTAddr = (unsigned int)dvr_malloc_specific(0x8000, GFP_DCU2);
	if (gmLUTAddr == 0) {
		printk(KERN_EMERG"gmLUTAddr allocates failed \n");
		return DV_ERR_NO_MEMORY;
	}

	if (gmlutfp == NULL)
		gmlutfp = file_open(path, O_RDONLY, 0);
	if (gmlutfp == 0) {
		dvr_free((void *)gmLUTAddr);
		//printk(KERN_EMERG"file %s open failed \n", path);
		return DV_ERR_RUN_FAIL;
	}

	printk(KERN_EMERG"gmLUTAddr=0x%x\n", (unsigned int)dvr_to_phys((void *)gmLUTAddr));

	ddrptr = (unsigned char *)gmLUTAddr;
	for (idx = 0; idx < 29509; idx++) {
		file_read((unsigned char *)&tmp, 1, idx + foffset, gmlutfp);
		*ddrptr = tmp;
		ddrptr++;
	}

	flush_cache_all();
	if (gmlutfp != NULL) {
	    file_sync(gmlutfp);
	    file_close(gmlutfp);
	}

	// load GMLut 3DLut
	if (type == 0) {
		if(uPicMode == DV_PIC_DARK_MODE)
			ret = CreateLoadDmLut((char *)gmLUTAddr, &dm_dark_cfg.bwDmLut, false, 0);
		else if(uPicMode == DV_PIC_BRIGHT_MODE)
			ret = CreateLoadDmLut((char *)gmLUTAddr, &dm_bright_cfg.bwDmLut, false, 0);
		else if(uPicMode == DV_PIC_VIVID_MODE)
			ret = CreateLoadDmLut((char *)gmLUTAddr, &dm_vivid_cfg.bwDmLut, false, 0);
	}
#if EN_GLOBAL_DIMMING
	else if (type == 1) {
		if(uPicMode == DV_PIC_DARK_MODE)
			ret = CreateLoadDmLut((char *)gmLUTAddr, &dm_dark_cfg.gdLutA, false, 1);
		else if(uPicMode == DV_PIC_BRIGHT_MODE)
			ret = CreateLoadDmLut((char *)gmLUTAddr, &dm_bright_cfg.gdLutA, false, 1);
		else if(uPicMode == DV_PIC_VIVID_MODE)
			ret = CreateLoadDmLut((char *)gmLUTAddr, &dm_vivid_cfg.gdLutA, false, 1);
	}else if (type == 2) {
		if(uPicMode == DV_PIC_DARK_MODE)
			ret = CreateLoadDmLut((char *)gmLUTAddr, &dm_dark_cfg.gdLutB, false, 2);
		else if(uPicMode == DV_PIC_BRIGHT_MODE)
			ret = CreateLoadDmLut((char *)gmLUTAddr, &dm_bright_cfg.gdLutB, false, 2);
		else if(uPicMode == DV_PIC_VIVID_MODE)
			ret = CreateLoadDmLut((char *)gmLUTAddr, &dm_vivid_cfg.gdLutB, false, 2);
	}
#endif

	if (ret != 0) {
		printk("%s, CreateLoadDmLut failed \n", __FUNCTION__);
	}

	flush_cache_all();

	// free
	dvr_free((void *)gmLUTAddr);

	// 3D lut
	//CommitDmLut(&(dm_cfg.bwDmLut), &h_dm->dmExec.bwDmLut);
	//// simply use the income lut buffer
	//h_dm->dmExec.bwDmLut.lutMap = (uint16_t *)dm_cfg.bwDmLut.LutMap;	// v1.1.10
#if EN_GLOBAL_DIMMING
	// final 3D LUT by UpdateKsOMapTmm funciton
#else
	//MArk by will need to check
	memcpy(h_dm->dmExec.bwDmLut.lutMap, dm_cfg.bwDmLut.LutMap, sizeof(uint16_t)*3*GMLUT_MAX_DIM*GMLUT_MAX_DIM*GMLUT_MAX_DIM);	// v1.1.11
#endif

	// rising force update flag
	g_forceUpdate_3DLUT = 1;

	printk(KERN_EMERG"file %s open successfully \n", path);

	return DV_SUCCESS;
}


int GMLUT_Pq2GLut_Update(char *path, uint32_t foffset, uint32_t size, E_DV_PICTURE_MODE uPicMode)
{
	unsigned int pq2LUTAddr = 0, idx= 0;
	unsigned char *ddrptr, tmp;
	struct file *pq2lutfp = NULL;


	if (pq2LUTAddr == 0)
		pq2LUTAddr = (unsigned int)dvr_malloc_specific(0x8000, GFP_DCU2);
	if (pq2LUTAddr == 0) {
		printk(KERN_EMERG"pq2LUTAddr allocates failed \n");
		return DV_ERR_NO_MEMORY;
	}

	if (pq2lutfp == NULL)
		pq2lutfp = file_open(path, O_RDONLY, 0);
	if (pq2lutfp == 0) {
		dvr_free((void *)pq2LUTAddr);
		//printk(KERN_EMERG"file %s open failed \n", path);
		return DV_ERR_RUN_FAIL;
	}

	printk(KERN_EMERG"pq2LUTAddr=0x%x\n", (unsigned int)dvr_to_phys((void *)pq2LUTAddr));

	ddrptr = (unsigned char *)pq2LUTAddr;
	for (idx = 0; idx < size; idx++) {
		file_read((unsigned char *)&tmp, 1, idx + foffset, pq2lutfp);
		*ddrptr = tmp;
		ddrptr++;
	}

#if EN_GLOBAL_DIMMING
	if(uPicMode == DV_PIC_DARK_MODE)
		ParsePq2GLut((const int32_t *)pq2LUTAddr, &dm_dark_cfg.Pq2GLut);
	else if(uPicMode == DV_PIC_BRIGHT_MODE)
		ParsePq2GLut((const int32_t *)pq2LUTAddr, &dm_bright_cfg.Pq2GLut);
	else if(uPicMode == DV_PIC_VIVID_MODE)
		ParsePq2GLut((const int32_t *)pq2LUTAddr, &dm_vivid_cfg.Pq2GLut);
#endif

	flush_cache_all();
	if (pq2lutfp != NULL) {
		file_sync(pq2lutfp);
		file_close(pq2lutfp);
	}

	// free
	dvr_free((void *)pq2LUTAddr);

	printk(KERN_EMERG"file %s open successfully \n", path);

	return DV_SUCCESS;
}


int Target_Display_Parameters_Update(char *path, uint32_t foffset)
{
#if 0//Mark by will
	unsigned int *buffer = NULL;
	int itmp, idx;
	unsigned short ustmp;
	struct file *pqParafp = NULL;
	unsigned short ustmpLut[14];

	/*
		e.g.
	    TminPQ = 62				(2 bytes)
		TmaxPQ = 2771			(2 bytes)
		Tgamma = 36045			(2 bytes)
		TEOTF = 0				(4 bytes)
		TBitDepth = 12			(4 bytes)
		TSignalRange = 1		(4 bytes)
		Tdiagonalinches = 42	(2 bytes)
		MidPQBias = 0			(2 bytes)	// invalidate
		SaturationGainBias = 0	(2 bytes)	// invalidate
		color_mode				(4 bytes)	// invalidate
		TMidPQBiasLut			(28 bytes)
		SaturationGainBiasLut	(28 bytes)
	*/


	if (buffer == NULL)
		buffer = (unsigned int *)kmalloc(255, GFP_KERNEL);
	if (buffer == NULL) {
		printk(KERN_EMERG"buffer allocates failed \n");
		return DV_ERR_NO_MEMORY;
	}

	if (pqParafp == NULL)
		pqParafp = file_open(path, O_RDONLY, 0);
	if (pqParafp == 0) {
		kfree(buffer);
		printk(KERN_EMERG"file %s open failed \n", path);
		return DV_ERR_RUN_FAIL;
	}

	idx = 0;
	file_read((unsigned char *)&ustmp, 2, idx + foffset, pqParafp);
	dm_cfg.tgtSigEnv.MinPq = ustmp;
	idx += sizeof(ustmp);
	file_read((unsigned char *)&ustmp, 2, idx + foffset, pqParafp);
	dm_cfg.tgtSigEnv.MaxPq = ustmp;
	idx += sizeof(ustmp);
	file_read((unsigned char *)&ustmp, 2, idx + foffset, pqParafp);
	dm_cfg.tgtSigEnv.Gamma = ustmp;
	idx += sizeof(ustmp);

	file_read((unsigned char *)&itmp, 4, idx + foffset, pqParafp);
	dm_cfg.tgtSigEnv.Eotf = itmp;
	idx += sizeof(itmp);
	file_read((unsigned char *)&itmp, 4, idx + foffset, pqParafp);
	dm_cfg.tgtSigEnv.BitDepth = itmp;
	idx += sizeof(itmp);
	file_read((unsigned char *)&itmp, 4, idx + foffset, pqParafp);
	dm_cfg.tgtSigEnv.RgbRng = itmp;
	idx += sizeof(itmp);

	file_read((unsigned char *)&ustmp, 2, idx + foffset, pqParafp);
	dm_cfg.tgtSigEnv.DiagSize = ustmp;
	idx += sizeof(ustmp);

/*
	file_read((unsigned char *)&ustmp, 2, idx + foffset, pqParafp);
	dm_cfg.dmCtrl.MidPQBias = ustmp;
	idx += sizeof(ustmp);

	file_read((unsigned char *)&ustmp, 2, idx + foffset, pqParafp);
	dm_cfg.dmCtrl.SaturationGainBias = ustmp;
	idx += sizeof(ustmp);

	file_read((unsigned char *)&itmp, 4, idx + foffset, pqParafp);
	g_dv_picmode = itmp;
*/

	file_read((unsigned char *)&ustmpLut, 28, idx + foffset, pqParafp);
	for (idx = 0; idx < sizeof(dm_cfg.dmCtrl.MidPQBiasLut)/sizeof(int16_t); idx++)
		dm_cfg.dmCtrl.MidPQBiasLut[idx] = (int16_t)ustmpLut[idx];
	idx += sizeof(dm_cfg.dmCtrl.MidPQBiasLut);

	file_read((unsigned char *)&ustmpLut, 28, idx + foffset, pqParafp);
	for (idx = 0; idx < sizeof(dm_cfg.dmCtrl.SaturationGainBiasLut)/sizeof(int16_t); idx++)
		dm_cfg.dmCtrl.SaturationGainBiasLut[idx] = (int16_t)ustmpLut[idx];
	idx += sizeof(dm_cfg.dmCtrl.SaturationGainBiasLut);

	if (pqParafp != NULL) {
		file_sync(pqParafp);
		file_close(pqParafp);
	}
	kfree(buffer);

	printk(KERN_EMERG"file %s open successfully \n", path);

	printk("dm_cfg.tgtSigEnv.MinPq = %d \n", dm_cfg.tgtSigEnv.MinPq);
	printk("dm_cfg.tgtSigEnv.MaxPq = %d \n", dm_cfg.tgtSigEnv.MaxPq);
	printk("dm_cfg.tgtSigEnv.Gamma = %d \n", dm_cfg.tgtSigEnv.Gamma);
	printk("dm_cfg.tgtSigEnv.Eotf = %d \n", dm_cfg.tgtSigEnv.Eotf);
	printk("dm_cfg.tgtSigEnv.BitDepth = %d \n", dm_cfg.tgtSigEnv.BitDepth);
	printk("dm_cfg.tgtSigEnv.RgbRng = %d \n", dm_cfg.tgtSigEnv.RgbRng);
	printk("dm_cfg.tgtSigEnv.DiagSize = %d \n", dm_cfg.tgtSigEnv.DiagSize);
	printk("dm_cfg.dmCtrl.MinPQBias = 0x%x \n", dm_cfg.dmCtrl.MinPQBias);
	printk("dm_cfg.dmCtrl.SaturationGainBias = 0x%x \n", dm_cfg.dmCtrl.SaturationGainBias);
	printk("g_dv_picmode=0x%x \n", g_dv_picmode);
#endif
	return DV_SUCCESS;
}

int Target_Display_Parameters_NewUpdate(char *path, uint32_t foffset, E_DV_PICTURE_MODE uPicMode)
{
	unsigned int *buffer = NULL;
	int idx, loop;
	unsigned short ustmp;
	struct file *pqParafp = NULL;
	unsigned short ustmpLut[14];
	DmCfgFxp_t *p_DmCfgFxp_t = &dm_dark_cfg;
	if(uPicMode == DV_PIC_BRIGHT_MODE)
	{
		p_DmCfgFxp_t = &dm_bright_cfg;
	}
	else if(uPicMode == DV_PIC_VIVID_MODE)
	{
		p_DmCfgFxp_t = &dm_vivid_cfg;
	}
	//unsigned int uitmp[10];

	/*
		e.g.
		reserved						(4 bytes)
		Tgamma = 36045					(2 bytes)
		TEOTF = 0						(2 bytes)
		TBitDepth = 12					(2 bytes)
		TSignalRange = 1				(2 bytes)
		Tdiagonalinches = 42			(2 bytes)
		TmaxPQ = 2771					(2 bytes)
		TminPQ = 62 					(2 bytes)
		reserved						(61 bytes)
		MidPQBiasLut					(28 bytes)
		reserved						(141 bytes)
		TgtGDCfg_t structure
	*/


	if (buffer == NULL)
		buffer = (unsigned int *)kmalloc(255, GFP_KERNEL);
	if (buffer == NULL) {
		printk(KERN_EMERG"buffer allocates failed \n");
		return DV_ERR_NO_MEMORY;
	}

	if (pqParafp == NULL)
		pqParafp = file_open(path, O_RDONLY, 0);
	if (pqParafp == 0) {
		kfree(buffer);
		printk(KERN_EMERG"file %s open failed \n", path);
		return DV_ERR_RUN_FAIL;
	}

	idx = 4;

	file_read((unsigned char *)&ustmp, 2, idx + foffset, pqParafp);
	p_DmCfgFxp_t->tgtSigEnv.Gamma = ustmp;
	idx += sizeof(ustmp);

	file_read((unsigned char *)&ustmp, 2, idx + foffset, pqParafp);
	p_DmCfgFxp_t->tgtSigEnv.Eotf = ustmp;
	idx += sizeof(ustmp);

	file_read((unsigned char *)&ustmp, 2, idx + foffset, pqParafp);
	p_DmCfgFxp_t->tgtSigEnv.BitDepth = ustmp;
	idx += sizeof(ustmp);

	file_read((unsigned char *)&ustmp, 2, idx + foffset, pqParafp);
	p_DmCfgFxp_t->tgtSigEnv.RgbRng = ustmp;
	idx += sizeof(ustmp);

	file_read((unsigned char *)&ustmp, 2, idx + foffset, pqParafp);
	p_DmCfgFxp_t->tgtSigEnv.DiagSize = ustmp;
	idx += sizeof(ustmp);

	file_read((unsigned char *)&ustmp, 2, idx + foffset, pqParafp);
	p_DmCfgFxp_t->tgtSigEnv.MaxPq = ustmp;
	idx += sizeof(ustmp);

	file_read((unsigned char *)&ustmp, 2, idx + foffset, pqParafp);
	p_DmCfgFxp_t->tgtSigEnv.MinPq = ustmp;
	idx += sizeof(ustmp);

	idx += 60;

	file_read((unsigned char *)&ustmpLut, 28, idx + foffset, pqParafp);
	for (loop = 0; loop < sizeof(p_DmCfgFxp_t->dmCtrl.MidPQBiasLut)/sizeof(int16_t); loop++)
		p_DmCfgFxp_t->dmCtrl.MidPQBiasLut[loop] = (int16_t)ustmpLut[loop];
	idx += sizeof(p_DmCfgFxp_t->dmCtrl.MidPQBiasLut);

	idx += 140;

	file_read((unsigned char *)&g_TgtGDCfg, sizeof(TgtGDCfg_t), idx + foffset, pqParafp);

	idx += sizeof(TgtGDCfg_t);

	g_bl_OTT_apply_delay = g_TgtGDCfg.gdDelayMilliSec;//*90;
	if(g_bl_OTT_apply_delay > 200) g_bl_OTT_apply_delay = 200;//unit ms. For PWM setting delay time
	g_bl_HDMI_apply_delay = g_TgtGDCfg.gdDelayMilliSec;//*90;
	if(g_bl_HDMI_apply_delay > 200) g_bl_HDMI_apply_delay = 200;//unit ms. For PWM setting delay time


	if (pqParafp != NULL) {
		file_sync(pqParafp);
		file_close(pqParafp);
	}
	kfree(buffer);

	printk(KERN_EMERG"file %s open successfully \n", path);

	printk("dm_cfg.tgtSigEnv.Gamma = %d \n", p_DmCfgFxp_t->tgtSigEnv.Gamma);
	printk("dm_cfg.tgtSigEnv.Eotf = %d \n", p_DmCfgFxp_t->tgtSigEnv.Eotf);
	printk("dm_cfg.tgtSigEnv.BitDepth = %d \n", p_DmCfgFxp_t->tgtSigEnv.BitDepth);
	printk("dm_cfg.tgtSigEnv.RgbRng = %d \n", p_DmCfgFxp_t->tgtSigEnv.RgbRng);
	printk("dm_cfg.tgtSigEnv.DiagSize = %d \n", p_DmCfgFxp_t->tgtSigEnv.DiagSize);
	printk("dm_cfg.tgtSigEnv.MaxPq = %d \n", p_DmCfgFxp_t->tgtSigEnv.MaxPq);
	printk("dm_cfg.tgtSigEnv.MinPq = %d \n", p_DmCfgFxp_t->tgtSigEnv.MinPq);
	printk("dm_cfg.dmCtrl.MidPQBiasLut[0] = %d \n", p_DmCfgFxp_t->dmCtrl.MidPQBiasLut[0]);
	//printk("g_TgtGDCfg.gdEnable = %d \n", g_TgtGDCfg.gdEnable);//DOlby Hari 0616
	//printk("g_TgtGDCfg.gdWMin = %d \n", g_TgtGDCfg.gdWMin);//DOlby Hari 0616
	//printk("g_TgtGDCfg.gdWMax = %d \n", g_TgtGDCfg.gdWMax);//DOlby Hari 0616
	//printk("g_TgtGDCfg.gdWeightMean = %d \n", g_TgtGDCfg.gdWeightMean);//DOlby Hari 0616

	return DV_SUCCESS;
}


void Backlight_DelayTiming_Update(char *path, uint32_t foffset)
{
	unsigned int *buffer = NULL;
	int idx;
	unsigned short ustmp;
	struct file *bl_delayfp = NULL;

	if (buffer == NULL)
		buffer = (unsigned int *)kmalloc(255, GFP_KERNEL);
	if (buffer == NULL) {
		printk(KERN_EMERG"buffer allocates failed \n");
		return ;
	}

	if (bl_delayfp == NULL)
		bl_delayfp = file_open(path, O_RDONLY, 0);
	if (bl_delayfp == 0) {
		kfree(buffer);
		printk(KERN_EMERG"file %s open failed \n", path);
		return ;
	}

	idx = 0;
	file_read((unsigned char *)&ustmp, 2, idx + foffset, bl_delayfp);
	idx += sizeof(ustmp);
	g_bl_OTT_apply_delay = ustmp * 90;

	file_read((unsigned char *)&ustmp, 2, idx + foffset, bl_delayfp);
	g_bl_HDMI_apply_delay = ustmp * 90;

	if (bl_delayfp != NULL) {
		file_sync(bl_delayfp);
		file_close(bl_delayfp);
	}
	kfree(buffer);

	printk("g_bl_OTT_apply_delay=0x%x \n", g_bl_OTT_apply_delay);
	printk("g_bl_HDMI_apply_delay=0x%x \n", g_bl_HDMI_apply_delay);

	return ;
}

void Backlight_LUT_Update(char *path, uint32_t foffset, E_DV_PICTURE_MODE uPicMode)
{
	unsigned int *buffer = NULL;
	int idxlut, idx, offset;
	//unsigned short ustmp;
	unsigned char uctmp;
	unsigned short *lut = gd_Dark_Backlight_LUT;
	struct file *bl_lutfp = NULL;

	if(uPicMode == DV_PIC_BRIGHT_MODE)
		lut = gd_Bright_Backlight_LUT;
	else if(uPicMode == DV_PIC_VIVID_MODE)
		lut = gd_Vivid_Backlight_LUT;

	if (buffer == NULL)
		buffer = (unsigned int *)kmalloc(0x2800, GFP_KERNEL);
	if (buffer == NULL) {
		printk(KERN_EMERG"buffer allocates failed \n");
		return ;
	}

	if (bl_lutfp == NULL)
		bl_lutfp = file_open(path, O_RDONLY, 0);
	if (bl_lutfp == 0) {
		kfree(buffer);
		printk(KERN_EMERG"file %s open failed \n", path);
		return ;
	}

	idxlut = offset = 0;
	for (idx = 0; idx < 4096; idx++) {

		file_read((unsigned char *)&uctmp, 1, offset + foffset, bl_lutfp);
		offset += 1;
		lut[idxlut++] = uctmp;

	}

	if (bl_lutfp != NULL) {
		file_sync(bl_lutfp);
		file_close(bl_lutfp);
	}
	kfree(buffer);

	printk("%s, done..\n", __FUNCTION__);

	return ;
}


int DolbyPQ_1Model_1File_Config(char *picModeFilePath, E_DV_PICTURE_MODE uPicMode)
{
	unsigned int file_offset = 0;
	unsigned int picMode_offset = 109727;	/* size define by DOLBY  */

	unsigned int cBufEntries = DM_LUT_VERSION_CHARS + DM_LUT_ENDIAN_CHARS + DM_LUT_TYPE_CHARS;
	char cBuf[DM_LUT_VERSION_CHARS + DM_LUT_ENDIAN_CHARS + DM_LUT_TYPE_CHARS];
	char Version[DM_LUT_VERSION_CHARS + 1];
	char Endian[DM_LUT_ENDIAN_CHARS + 1];
	int version;
	char *pCh;
	DmCfgFxp_t *p_DmCfgFxp_t = &dm_dark_cfg;
	struct file *filefp = NULL;

	if (picModeFilePath==NULL) {
		printk(KERN_EMERG"%s, wrong file path, please check \n", __FUNCTION__);
		return 0;
	}

	if (uPicMode == DV_PIC_DARK_MODE) {
		file_offset = 0;
		p_DmCfgFxp_t = &dm_dark_cfg;
	} else if (uPicMode == DV_PIC_BRIGHT_MODE) {
		file_offset = picMode_offset;
		p_DmCfgFxp_t = &dm_bright_cfg;
	} else if (uPicMode == DV_PIC_VIVID_MODE) {
		file_offset = picMode_offset * 2;
		p_DmCfgFxp_t = &dm_vivid_cfg;
	} else
		return 0;

	// check the header of default bcms
	if (filefp == NULL)
		filefp = file_open(picModeFilePath, O_RDONLY, 0);
	if (filefp == 0) {
		printk(KERN_EMERG"file %s open failed \n", picModeFilePath);

#ifdef DOLBYPQ_DEBUG
		if (strcmp(g_PicModeFilePath[0], "/tmp/usb/sda/sda1/dolbypq/DolbyPQ_PanelType.bin")==0) {
			strcpy(g_PicModeFilePath[0], "/tmp/usb/sdb/sdb1/dolbypq/DolbyPQ_PanelType.bin");
			strcpy(g_PicModeFilePath[1], "/tmp/usb/sdb/sdb1/dolbypq/DolbyPQ_PanelType.bin");
			strcpy(g_PicModeFilePath[2], "/tmp/usb/sdb/sdb1/dolbypq/DolbyPQ_PanelType.bin");
			filefp = file_open(picModeFilePath, O_RDONLY, 0);
			if (filefp == 0)
				return 0;
		} else if (strcmp(g_PicModeFilePath[0], "/tmp/usb/sdb/sdb1/dolbypq/DolbyPQ_PanelType.bin")==0) {
			strcpy(g_PicModeFilePath[0], "/tmp/usb/sda/sda1/dolbypq/DolbyPQ_PanelType.bin");
			strcpy(g_PicModeFilePath[1], "/tmp/usb/sda/sda1/dolbypq/DolbyPQ_PanelType.bin");
			strcpy(g_PicModeFilePath[2], "/tmp/usb/sda/sda1/dolbypq/DolbyPQ_PanelType.bin");
			filefp = file_open(picModeFilePath, O_RDONLY, 0);
			if (filefp == 0)
				return 0;
		}
		else
#endif
			return 0;
	}

	file_read(&cBuf[0], cBufEntries, 0, filefp);

	//// version
	pCh = cBuf;
	memcpy(&Version[0], pCh, DM_LUT_VERSION_CHARS);
	pCh += DM_LUT_VERSION_CHARS;

	version = simple_strtol(Version, NULL, 10);
	if (version < 64) {
		printf("invalide lut file. version not supported.\n");
		return 0;
	}
	//// endian
	Endian[0] = TO_LOWER_CASE(*pCh);
	if (Endian[0] != 'l') {
		printf("invalide lut file. only little endia is supported\n");
		return 0;
	}

	if (filefp != NULL)
	    file_close(filefp);



	// default bcms
	GMLUT_3DLUT_Update(picModeFilePath, file_offset, 0, uPicMode);
	file_offset += 29509;
	// gd 3dlut a
	GMLUT_3DLUT_Update(picModeFilePath, file_offset, 1, uPicMode);
	file_offset += 29509;
	// gd 3dlut b
	GMLUT_3DLUT_Update(picModeFilePath, file_offset, 2, uPicMode);
	file_offset += 29509;

	// PQ2BT1886
	GMLUT_Pq2GLut_Update(picModeFilePath, file_offset, (4 + 1024*4 + 16*3) * sizeof(int32_t), uPicMode);
	file_offset += 16592;

	// backlight lut
	Backlight_LUT_Update(picModeFilePath, file_offset, uPicMode);
	file_offset += 4096;

	// pq parameters
	Target_Display_Parameters_NewUpdate(picModeFilePath, file_offset, uPicMode);

	// to avoid gdWDynRngSqrt field to be zero
	if (g_TgtGDCfg.gdWDynRngSqrt == 0)
		g_TgtGDCfg.gdWDynRngSqrt = p_DmCfgFxp_t->gdCtrl.GdWDynRngSqrt;

	// commit configuration//Need to move to parse //Mark by will
	InitGdCtrlFromConfig(&(p_DmCfgFxp_t->gdCtrl), &g_TgtGDCfg);
	//CommitDmCfg(&dm_cfg, h_dm);//Need to move to parse //Mark by will

	return 1;
}
#else
#ifndef RTK_EDBEC_1_5
void handleDoViBacklight(DmCfgFxp_t *p_cfg, unsigned char uiBackLightValue,  unsigned char src, unsigned int *SliderScalar)
{
		#define LOG2_UI_STEPSIZE 3
		int16_t x_x1,x1,y2,y1, Y;
		int32_t temp;

		uiBackLightValue+=2;
		uiBackLightValue = CLAMPS(uiBackLightValue,0,100);

		x1   = uiBackLightValue>>LOG2_UI_STEPSIZE;
		x_x1 = uiBackLightValue - (x1<<LOG2_UI_STEPSIZE);

		y1 = p_cfg->dmCtrl.MidPQBiasLut[x1];
		y2 = p_cfg->dmCtrl.MidPQBiasLut[x1+1];
		y2 = y2-y1 ; //Y2-Y1
		//X2-X1 = 8;
		temp = (y2 * x_x1) >>LOG2_UI_STEPSIZE   ;//(y2-y1)/(x2-x1) * (x-x1)
		Y = (int16_t)(temp + y1);
		p_cfg->dmCtrl.MidPQBias = Y;

		y1 = p_cfg->dmCtrl.SaturationGainBiasLut[x1];
		y2 = p_cfg->dmCtrl.SaturationGainBiasLut[x1+1];
		y2 = y2-y1 ; //Y2-Y1
		//X2-X1 = 8;
		temp = (y2 * x_x1) >>LOG2_UI_STEPSIZE   ;//(y2-y1)/(x2-x1) * (x-x1)
		Y = (int16_t)(temp + y1);
		p_cfg->dmCtrl.SaturationGainBias = Y;


		y1 = p_cfg->dmCtrl.ChromaWeightBiasLut[x1];
		y2 = p_cfg->dmCtrl.ChromaWeightBiasLut[x1+1];
		y2 = y2-y1 ; //Y2-Y1
		//X2-X1 = 8;
		temp = (y2 * x_x1) >>LOG2_UI_STEPSIZE   ;//(y2-y1)/(x2-x1) * (x-x1)
		Y = (int16_t)(temp + y1);
		p_cfg->dmCtrl.ChromaWeightBias = Y;

		y1 = p_cfg->dmCtrl.SlopeBiasLut[x1];
		y2 = p_cfg->dmCtrl.SlopeBiasLut[x1+1];
		y2 = y2-y1 ; //Y2-Y1
		//X2-X1 = 8;
		temp = (y2 * x_x1) >>LOG2_UI_STEPSIZE   ;//(y2-y1)/(x2-x1) * (x-x1)
		Y = (int16_t)(temp + y1);
		p_cfg->dmCtrl.TrimSlopeBias = Y;


		y1 = p_cfg->dmCtrl.OffsetBiasLut[x1];
		y2 = p_cfg->dmCtrl.OffsetBiasLut[x1+1];
		y2 = y2-y1 ; //Y2-Y1
		//X2-X1 = 8;
		temp = (y2 * x_x1) >>LOG2_UI_STEPSIZE   ;//(y2-y1)/(x2-x1) * (x-x1)
		Y = (int16_t)(temp + y1);
		p_cfg->dmCtrl.TrimOffsetBias = Y;


		y1 = p_cfg->dmCtrl.BacklightBiasLut[x1];
		y2 = p_cfg->dmCtrl.BacklightBiasLut[x1+1];
		y2 = y2-y1 ; //Y2-Y1
		//X2-X1 = 8;
		temp = (y2 * x_x1) >>LOG2_UI_STEPSIZE   ;//(y2-y1)/(x2-x1) * (x-x1)
		Y = (int16_t)(temp + y1);
		p_cfg->dmCtrl.BacklightBias = 4096+Y;
		*SliderScalar = p_cfg->dmCtrl.BacklightBias;

//	printk(KERN_EMERG "\r\n########handleDoViBacklight uiBackLightValue:%d p_cfg->dmCtrl.MidPQBias%d#########\r\n",uiBackLightValue, p_cfg->dmCtrl.MidPQBias);


}
#endif

int  DolbyPQ_1Model_1File_Config(char *picModeFilePath, E_DV_PICTURE_MODE uPicMode)
{
	if( !dlbPqConfig_flag){
	//memcpy((unsigned char *)&dlbPqConfig, CFG_12b_0,sizeof(dlbPqConfig));
	//memcpy((unsigned char *)&dlbPqConfig, CFG_12b_0_low_latency,sizeof(dlbPqConfig));
	//
	dlbPqConfig_ott = (pq_config_t*)CFG_12b_0_low_latency_0;
	dlbPqConfig_hdmi = (pq_config_t*)CFG_12b_0_low_latency_12;
	printk(KERN_ERR"[DV]-GET OTT  CONFIG-bitdepth = %08x\n", dlbPqConfig_ott->target_display_config.bitDepth);
	printk(KERN_ERR"[DV]-GET HDMI CONFIG-bitdepth = %08x\n", dlbPqConfig_hdmi->target_display_config.bitDepth);
	dlbPqConfig_flag = 1;
    }
	return 0;

}

#endif

unsigned char check_need_to_reload_config(void)
{//This api is for dolby debug. Can not use finally
	struct file *filefp = NULL;
	filefp = file_open("/tmp/usb/sda/sda1/dolby_debug_enable", O_RDONLY, 0);
	if (filefp == 0)
	{
		return FALSE;
	}
	else
{
		file_close(filefp);
		pr_notice("\r\n###Find dolby_debug_enable so reload config######\r\n");
		return TRUE;
	}
}

#ifndef _ENABLE_BACKLIGHT_DELAY_CTRL
void BacklightCtrl(uint16_t duty_idx, E_DV_PICTURE_MODE uPicMode)
{
	unsigned short *lut = gd_Dark_Backlight_LUT;
    short pwmVal;

	if(uPicMode == DV_PIC_BRIGHT_MODE)
		lut = gd_Bright_Backlight_LUT;
	else if(uPicMode == DV_PIC_VIVID_MODE)
		lut = gd_Vivid_Backlight_LUT;

	// added by smfan for GD's backlight control
	DBG_PRINT(KERN_EMERG"%s, duty_idx = %d \n", __FUNCTION__, duty_idx);

	pwmVal  = lut[duty_idx];

	pwmVal  = (pwmVal * backlightSliderScalar)>>12;
	if(pwmVal > 255) pwmVal=255;
	if(pwmVal < 0) pwmVal = 0;
	// backlight control
#if (defined(CONFIG_RTK_KDRV_PWM))
	if (0 != rtk_pwm_backlight_set_duty(pwmVal)) { /*20171226, baker recover for dolby backlight control*/
		printk("%s, PWM control failed...duty=%d \n", __FUNCTION__, pwmVal);
	}
#endif
}
#endif
//>>>> reduce delay period to 5~6 ms
#if 1
#define MAX_GD_BUF  20

struct{
    int dim_level;
    struct timer_list timer;
    //struct timespec *triggered_tp;
} global_dimming_buf[MAX_GD_BUF];

static int gloabl_dim_count = 0;

#if 0
static void timer_handler_thread(int index )
{
    //pr_notice("==PWMSet idx=%d pwm=%d jiffies=%x %x\n",index, global_dimming_buf[index].dim_level, global_dimming_buf[index].timer.expires, jiffies);
#if (defined(CONFIG_RTK_KDRV_PWM))
    // Set this to PWM
    if(0!=rtk_pwm_backlight_set_duty(global_dimming_buf[index].dim_level)){
	   pr_err("rtk_pwm_backlight_set_duty() Failed!!!\n");
	}
#endif
}


// add for testing
void append_timer(struct timer_list *timerID, unsigned int msec, int value)
{
	DEFINE_TIMER(tmp_timer, timer_handler_thread, jiffies+msecs_to_jiffies(msec), value);
	*timerID = tmp_timer;
	add_timer_on(timerID, 1);//designate cpu 1
}
#endif

void k2l_set_global_dimming(unsigned int latency_in_ms, int dim_val)
{
	int i, local_idx;
	local_idx = gloabl_dim_count;
    if(latency_in_ms != 0)
    {
        //global_dimming_buf[gloabl_dim_count].timer = (timer_t *)malloc(sizeof(timer_t));

        if(dim_val > 255)
            dim_val = 255;
		for(i = 0 ; i < MAX_GD_BUF ; i++)
		{
			if(local_idx == MAX_GD_BUF)
				local_idx = 0;
			if(timer_pending(&global_dimming_buf[local_idx].timer))
			{
				local_idx++;
			}
			else
			{
				global_dimming_buf[local_idx].dim_level = dim_val;
				//append_timer(&global_dimming_buf[local_idx].timer, latency_in_ms, local_idx);
				gloabl_dim_count = (local_idx + 1) % MAX_GD_BUF;
				return;
			}
		}
		pr_err("\r\n [error] set_global_dimming can not find no pending index \r\n");

        // test code
        //global_dimming_buf[gloabl_dim_count].triggered_tp = malloc(sizeof(struct timespec));
        //clock_gettime(CLOCK_MONOTONIC, global_dimming_buf[gloabl_dim_count].triggered_tp);
    }
}
#endif
//<<< reduce delay period to 5~6 ms

#ifdef _ENABLE_BACKLIGHT_DELAY_CTRL
#define BACKLIGHT_PRINT(s, args...) //printk(s, ## args)
void BacklightDelayCtrl_update(uint16_t duty_idx, unsigned int delayPeriod, unsigned char src, E_DV_PICTURE_MODE uPicMode, unsigned int backlightSliderScalar)
{
#ifndef RTK_EDBEC_1_5
	//unsigned int stc = rtd_inl(SCPU_CLK90K_LO_reg);
	//unsigned char cur_wrIdx, next_wrIdx;
	//unsigned char cur_rdIdx, next_rdIdx;
	//static unsigned int last_stc = 0;
	unsigned short *lut = gd_Dark_Backlight_LUT;
    short pwmVal;
	if(uPicMode == DV_PIC_BRIGHT_MODE)
		lut = gd_Bright_Backlight_LUT;
	else if(uPicMode == DV_PIC_VIVID_MODE)
		lut = gd_Vivid_Backlight_LUT;


	// added by smfan for GD's backlight control
	//DBG_PRINT("duty_idx = %d\n", duty_idx);

	// turn on backlight to maximum when gd off
	if(src == DOLBY_SRC_HDMI) {
		if (hdmi_h_dm->dmCfg.gdCtrl.GdOn == 0)
			duty_idx = 4095;
	} else
	{
		if (OTT_h_dm->dmCfg.gdCtrl.GdOn == 0)
			duty_idx = 4095;
	}
	if(duty_idx > 4095)
		duty_idx = 4095;

	pwmVal = lut[duty_idx];
	//printk(KERN_EMERG "\r\n###func:%s pwmVal:%d slider:%d#####\r\n",__FUNCTION__,pwmVal,backlightSliderScalar);

    pwmVal  = (pwmVal * backlightSliderScalar)>>12;

	if(pwmVal > 255) pwmVal=255;
	if(pwmVal < 10) pwmVal = 10;


	//printk("cur_rdIdx=%d,duty_idx = %d  delay=%d @%x\n",cur_rdIdx, duty_idx,delayPeriod, stc);
	if(last_global_dim_val == pwmVal)
        return;
    else
        last_global_dim_val = pwmVal;
	if (delayPeriod == 0) {
		if (0 != rtk_pwm_backlight_set_duty(pwmVal)) { /*20171226, baker recover for dolby backlight control*/
			pr_err("%s, PWM control failed...duty=%d \n", __FUNCTION__, pwmVal);
		}
		return ;
	}
	else {
        #if 1
            k2l_set_global_dimming(delayPeriod , pwmVal);
        #else
        // ShuMing`s original delay period flow
	// update backlight info to queue
	cur_wrIdx = backLightInfo.writeIdx;
	next_wrIdx = ((cur_wrIdx < (BACKLIGHT_BUF_SIZE-1)) ? cur_wrIdx+1: 0);
	backLightInfo.stc[cur_wrIdx] = stc;
	backLightInfo.duty_idx[cur_wrIdx] = duty_idx;

	// update read index
	cur_rdIdx = backLightInfo.readIdx;

	// output the STC matched index, need cover STC reset case
	if (stc > last_stc) {
		int i = cur_rdIdx;
		unsigned int last_mach_idx = cur_rdIdx;
		while (i != next_wrIdx){
			// find the closest STC to apply the backlight setting
			if ((backLightInfo.stc[i]+delayPeriod) >= stc){
				DBG_PRINT("G[%d]=%d\n", i, (stc - backLightInfo.stc[i])/90);
				break;
			}
			DBG_PRINT("t[%d]=%d\n", i, (stc - backLightInfo.stc[i])/90);

			last_mach_idx = i;
			i = (i < (BACKLIGHT_BUF_SIZE-1)? i+1: 0);
		}
		next_rdIdx = last_mach_idx;
	} else { // reset read/write index
		// reset read/write index when STC rewind
		// don't update backlight in this time
		next_rdIdx = cur_wrIdx;
		printk("[BL] rst=%d@%x->%x\n", next_rdIdx, last_stc, stc);
	}

	printk("cur_rdIdx=%d,duty_idx = %d  delay=%d @%x\n",cur_rdIdx, duty_idx,delayPeriod, stc);

	// backlight control
	BACKLIGHT_PRINT("[BL] duty=%d\n", gd_Backlight_LUT[backLightInfo.duty_idx[cur_rdIdx]]);
	if (0 != rtk_pwm_backlight_set_duty(gd_Backlight_LUT[backLightInfo.duty_idx[cur_rdIdx]])) {
		printk("%s, PWM control failed...duty=%d \n", __FUNCTION__, gd_Backlight_LUT[backLightInfo.duty_idx[next_rdIdx]]);
	}


	// update write index
	backLightInfo.writeIdx = next_wrIdx;
	backLightInfo.readIdx = next_rdIdx;
	last_stc = stc;

	DBG_PRINT("Nr/w=%d/%d=%d@%d\n", next_rdIdx, next_wrIdx, gd_Backlight_LUT[backLightInfo.duty_idx[cur_rdIdx]], (backLightInfo.stc[cur_wrIdx]- backLightInfo.stc[cur_rdIdx])/90);
        #endif
	}
#endif
	return;
}

void BacklightDelayCtrl_init(void)
{

	DBG_PRINT("%s\n", __FUNCTION__);
	memset(&backLightInfo, 0, sizeof(backLightInfo));
	return;
}
#endif


void HDMI_TEST(unsigned int wid, unsigned int len, unsigned int mdAddr)
{
	//extern void calculate_backlight_setting_for_cfg(DmCfgFxp_t *p_cfg, unsigned char backlight, unsigned char src);
	static unsigned int bg_hdmi_last_h_start = 0;
	static unsigned int bg_hdmi_last_h_end = 0;
	static unsigned int bg_hdmi_last_v_start = 0;
	static unsigned int bg_hdmi_last_v_end = 0;
	static unsigned int HDMI_backlightSliderScalar = 0;
	unsigned char fullupdate = FALSE;//if ture updte b02 and b05
#if EN_GLOBAL_DIMMING
# if EN_AOI
	unsigned int h_start, h_end, v_start, v_end;
#endif
#endif
	MdsChg_t mdsChg;
#ifndef RTK_EDBEC_1_5
	unsigned char *mdptr;
#endif
	dm_hdr_double_buffer_ctrl_RBUS dm_double_buf_ctrl_reg;
	hdr_all_top_top_ctl_RBUS hdr_all_top_top_ctl_reg;
	hdr_all_top_top_d_buf_RBUS hdr_all_top_top_d_buf_reg;
	DOLBYVISION_PATTERN dolby;
	DmExecFxp_t *pDmExec = &hdmi_h_dm->dmExec;
	unsigned int vo_vend, vgip_lineCnt, vgipVend;
	unsigned int vgip_time1, vgip_time2;
	E_DV_PICTURE_MODE cur_pic_mode;
	E_DV_PICTURE_MODE cur_backlight_value;
	static unsigned char pair_check = 0;

	ppoverlay_memcdtg_dh_den_start_end_RBUS memcdtg_dh_den_start_end;
	ppoverlay_memcdtg_dv_den_start_end_RBUS memcdtg_dv_den_start_end;
	unsigned int actvie_h = 0;//memc size or panel size
	unsigned int actvie_v = 0;//memc size or panel size
	unsigned char ui_setting_change = FALSE;//Means UI setting update picture mode and back light
	static unsigned char hdmi_force_3dLutupdate = FALSE;//when pair_check don't reset last time
	memcdtg_dh_den_start_end.regValue = rtd_inl(PPOVERLAY_memcdtg_DH_DEN_Start_End_reg);
	memcdtg_dv_den_start_end.regValue = rtd_inl(PPOVERLAY_memcdtg_DV_DEN_Start_End_reg);
	actvie_h = memcdtg_dh_den_start_end.memcdtg_dh_den_end-memcdtg_dh_den_start_end.memcdtg_dh_den_sta;
	actvie_v = memcdtg_dv_den_start_end.memcdtg_dv_den_end-memcdtg_dv_den_start_end.memcdtg_dv_den_sta;


	vgip_lineCnt = VGIP_VGIP_CHN1_LC_get_ch1_line_cnt(rtd_inl(VGIP_VGIP_CHN1_LC_reg));
	vo_vend = (rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) >> 16) & 0xfff;
	vgipVend = VGIP_VGIP_CHN1_ACT_VSTA_Length_get_ch1_iv_act_sta(rtd_inl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg))
							+ VGIP_VGIP_CHN1_ACT_VSTA_Length_get_ch1_iv_act_len(rtd_inl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg));


	hdr_all_top_top_ctl_reg.regValue = rtd_inl(HDR_ALL_TOP_TOP_CTL_reg);
	if ((rtd_inl(DM_dm_submodule_enable_reg) & DM_dm_submodule_enable_b01_02_enable_mask) == 0 || hdr_all_top_top_ctl_reg.dolby_mode == 0) {
		g_lvl4_exist = 0;
#if (defined(CONFIG_RTK_KDRV_PWM))
		if (0 != rtk_pwm_backlight_set_duty(255)) { /*20171226, baker recover for dolby backlight control*/
			printk("%s, PWM control failed...duty=%d \n", __FUNCTION__, 255);
		}
#endif
#ifdef _ENABLE_BACKLIGHT_DELAY_CTRL
		BacklightDelayCtrl_init();
		//Backlight_DelayTiming_Update("/usr/local/etc/backlight_delay.bin");
#endif
	}

	dolby.ColNumTtl = wid;
	dolby.RowNumTtl = len;
	dolby.BL_EL_ratio = 0;


	// make sure that sacler flow had completed
	//if (rtd_inl(PPOVERLAY_Main_Display_Control_RSV_reg) & PPOVERLAY_Main_Display_Control_RSV_m_force_bg_mask) {
	//	printk("vgiplc=%d\n", vgip_lineCnt);
	//	up(&g_dv_sem);
	//	return ;
	//}


	vo_vend = (rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) >> 16) & 0xfff;

	//if (rtd_inl(0xb800501c) & (1<<3)) {
	//	printk("[DBG] vgip_lineCnt=%d\n", vgip_lineCnt);
	//	printk("[DBG] vo_vend=%d\n", vo_vend);
	//}


	if (get_cur_hdmi_dolby_apply_state() == HDMI_STATE_POSITION)/*(vgip_lineCnt < vgipVend)*/ {
		if(HDMI_last_md_picmode == DV_PIC_NONE_MODE)
		{//means first time
			bg_hdmi_last_h_start = 0;
			bg_hdmi_last_h_end = 0;
			bg_hdmi_last_v_start = 0;
			bg_hdmi_last_v_end = 0;
			last_global_dim_val = -1;//reset last pwm value
		}
		if(g_picModeUpdateFlag)
		    fullupdate = TRUE;
		#if EN_GLOBAL_DIMMING
		// reset g_forceUpdate_3DLUT
		g_forceUpdate_3DLUT = 0;
		#endif
		if((hdmi_ui_change_delay_count == HDMI_DOLBY_UI_CHANGE_DELAY_APPLY) || (HDMI_last_md_picmode == DV_PIC_NONE_MODE))
		{
			cur_pic_mode = ui_dv_picmode;
			if(HDMI_last_md_picmode != cur_pic_mode)
			{
				if(cur_pic_mode == DV_PIC_BRIGHT_MODE)
					p_dm_hdmi_cfg = &dm_hdmi_bright_cfg;
				else if(cur_pic_mode == DV_PIC_VIVID_MODE)
					p_dm_hdmi_cfg = &dm_hdmi_vivid_cfg;
				else
					p_dm_hdmi_cfg = &dm_hdmi_dark_cfg;
				HDMI_last_md_picmode = cur_pic_mode;
				ui_setting_change = TRUE;
#ifndef RTK_EDBEC_1_5
				CommitDmCfg(p_dm_hdmi_cfg, hdmi_h_dm);
#endif
			}
			cur_backlight_value = ui_dv_backlight_value;
			if(HDMI_last_md_backlight != cur_backlight_value)
			{
				//calculate_backlight_setting_for_cfg(p_dm_hdmi_cfg, cur_backlight_value, DOLBY_SRC_HDMI);
				HDMI_last_md_backlight = cur_backlight_value;
				ui_setting_change = TRUE;
			}
#ifndef RTK_EDBEC_1_5
			if(ui_setting_change)
				handleDoViBacklight(&hdmi_h_dm->dmCfg, cur_backlight_value, DOLBY_SRC_HDMI, &HDMI_backlightSliderScalar);//calculate new backlight
#endif
			hdmi_ui_change_delay_count = 0xff;
		}

		if(ui_setting_change)
		{
			HDMI_Parser_NeedUpdate = 1;//parser need to update
		}
#if 1
		/*baker: it danguous to dm_metadata_base_t to dm_metadata_t,
		   ...but it seems we only need base..haha */


		hdmi_h_dm->dm_reg.colNum = wid;
		hdmi_h_dm->dm_reg.rowNum = len;
		rtk_wrapper_commit_reg(hdmi_h_dm,
			FORMAT_DOVI,
			(dm_metadata_t *)mdAddr,
			NULL,
			dlbPqConfig_hdmi, 2,
			&hdmi_h_dm->dst_comp_reg,
			&hdmi_h_dm->dm_reg,
			&hdmi_h_dm->dm_lut);

		rtk_dm_exec_params_to_DmExecFxp(hdmi_h_dm->h_ks,
			&hdmi_h_dm->dmExec,
			&hdmi_h_dm->dmExec_rtk,
			(MdsExt_t*)mdAddr,
			&hdmi_h_dm->dm_lut,
			dlbPqConfig_hdmi,
			RUN_MODE_FROMHDMI);

		//lut3D B05 table
		memcpy(hdmi_h_dm->dmExec.bwDmLut.lutMap,hdmi_h_dm->dm_lut.lut3D,sizeof(hdmi_h_dm->dm_lut.lut3D));
#if 0
		rtk_hdmi_dump(0,
			&hdmi_h_dm->dst_comp_reg,
			&hdmi_h_dm->dm_reg,
			&hdmi_h_dm->dm_lut,
			hdmi_h_dm);
#endif
#endif


#if EN_GLOBAL_DIMMING

		//if (g_lvl4_exist)
#ifdef _ENABLE_BACKLIGHT_DELAY_CTRL
		{
			BacklightDelayCtrl_update(hdmi_h_dm->h_dm->tcCtrl.tcTgtSig.maxPq, g_bl_HDMI_apply_delay, DOLBY_SRC_HDMI, HDMI_last_md_picmode, HDMI_backlightSliderScalar);
		}
#else
			BacklightCtrl(hdmi_h_dm->tcCtrl.tMaxPq, HDMI_last_md_picmode);
#endif
#endif

#if EN_GLOBAL_DIMMING
# if EN_AOI
			if (mds_ext.lvl5AoiAvail) {
			// horizontal mask line for Lv5 MD
			h_start = ((mds_ext.active_area_left_offset * actvie_h / wid)<<16);
			h_end = (actvie_h - (mds_ext.active_area_right_offset * actvie_h / wid));
			v_start = ((mds_ext.active_area_top_offset * actvie_v / len)<<16);
			v_end = (actvie_v - (mds_ext.active_area_bottom_offset * actvie_v / len));
			if((h_start != bg_hdmi_last_h_start) || (v_start != bg_hdmi_last_v_start) || (h_end != bg_hdmi_last_h_end) || (v_end != bg_hdmi_last_v_end)) {
				bg_hdmi_last_h_start = h_start;
				bg_hdmi_last_v_start = v_start;
				bg_hdmi_last_h_end = h_end;
				bg_hdmi_last_v_end = v_end;
				spin_lock(&Dolby_Letter_Box_Spinlock);
				dolby_proverlay_background_h_start_end = h_start | h_end;
				dolby_proverlay_background_v_start_end = v_start | v_end;
				letter_box_black_flag = TRUE;
				request_letter_dtg_change = TRUE;
				spin_unlock(&Dolby_Letter_Box_Spinlock);
			}
		} else {
				h_start = 0;
				h_end = actvie_h;
				v_start = 0;
				v_end = actvie_v;
				if((h_start != bg_hdmi_last_h_start) || (v_start != bg_hdmi_last_v_start) || (h_end != bg_hdmi_last_h_end) || (v_end != bg_hdmi_last_v_end)) {
					bg_hdmi_last_h_start = h_start;
					bg_hdmi_last_v_start = v_start;
					bg_hdmi_last_h_end = h_end;
					bg_hdmi_last_v_end = v_end;
					spin_lock(&Dolby_Letter_Box_Spinlock);
					dolby_proverlay_background_h_start_end = h_start | h_end;
					dolby_proverlay_background_v_start_end = v_start | v_end;
					letter_box_black_flag = FALSE;
					request_letter_dtg_change = TRUE;
					spin_unlock(&Dolby_Letter_Box_Spinlock);
				}
		}
#endif
#endif

		// TODO: dolby_v_top setting
		DolbyVTopRbusSet(&dolby, DV_HDMI_MODE);
		hdr_all_top_top_d_buf_reg.regValue = rtd_inl(HDR_ALL_TOP_TOP_D_BUF_reg);
		hdr_all_top_top_d_buf_reg.dolby_double_apply = 1;
		rtd_outl(HDR_ALL_TOP_TOP_D_BUF_reg, hdr_all_top_top_d_buf_reg.regValue);

		// TODO:  DM setting
		DMRbusSet(DV_HDMI_MODE, ((mdsChg==MDS_CHG_NONE) && !ui_setting_change)?0:1);//!ui_setting_change means picture mode change. We need to update B02
		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
		dm_double_buf_ctrl_reg.dm_db_apply = 1;
		rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
		hdmi_force_3dLutupdate = FALSE;

		if(pair_check || ui_setting_change)
		{
			hdmi_force_3dLutupdate = TRUE;//Last time don't run finish part
		}
		pair_check = 1;
		HDMI_Parser_NeedUpdate = 0;

		pr_debug("[H] position \n");
		vgip_time1 = VGIP_VGIP_CHN1_LC_get_ch1_line_cnt(rtd_inl(VGIP_VGIP_CHN1_LC_reg));
		if((vgip_time1 >= vgipVend) || (vgip_time1 <= vgip_lineCnt))
		{
			//pr_err("HDMI_TEST position vgip_lc=ori:%d final:%d\n", vgip_lineCnt, vgip_time1);
		}
#if 0
/*20170908, pinyen remove update debug_enable(b806b400[14] action in dolby hdmi flow,
this bit is for hdr10 and dolby hdr should reset to 0)*/
#ifdef CONFIG_SUPPORT_SCALER
		//if less than 30, set B05 in porch region.
		//if(Scaler_DispGetInputInfoByDisp(SLR_MAIN_DISPLAY, SLR_INPUT_V_FREQ)/10 > g_i2r_fps_thr) {
			//rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) | DM_dm_submodule_enable_debug_enable_mask);
			DM_B05_Set(pDmExec->bwDmLut.lutMap, 1);

			// enable double buffer
			dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
			dm_double_buf_ctrl_reg.dm_db_apply = 0;
			dm_double_buf_ctrl_reg.dm_db_en = 1;
			rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
		//}
#endif
#endif


	}
	//Eric@20180524 DMAtoB05 should do at vgip start
	DM_B05_DMA_Set(pDmExec->bwDmLut.lutMap);

	//Change to check by VGIP V_END @Crixus 20160321
	if ((/*(vgip_lineCnt >= vgipVend)*/(get_cur_hdmi_dolby_apply_state() == HDMI_STATE_FINISH) && pair_check) || fullupdate){

		vgip_time1 = VGIP_VGIP_CHN1_LC_get_ch1_line_cnt(rtd_inl(VGIP_VGIP_CHN1_LC_reg));
		pair_check = 0; //has to clear pair_check every finish isr


		// disable double buffer if need to update B05
		if (g_forceUpdate_3DLUT || g_picModeUpdateFlag || hdmi_force_3dLutupdate) {
		//if (1) {
			g_forceUpdate_3DLUT = 0;
			dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
			dm_double_buf_ctrl_reg.dm_db_en = 0;
			rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);

			// disable B05
			rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) & ~DM_dm_submodule_enable_debug_enable_mask);

			// postpone the update B05 timing
			// B05 3D LUT
			if (g_picModeUpdateFlag) {
				g_picModeUpdateFlag--;
			}
#if 0	//Eric@20180524 DMAtoB05 move to VGIP_V_STA
#ifdef CONFIG_SUPPORT_SCALER
			//if less than 30, set B05 in porch region.
			if(Scaler_DispGetInputInfoByDisp(SLR_MAIN_DISPLAY, SLR_INPUT_V_FREQ)/10 <= g_i2r_fps_thr) {
				// disable B05
				rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) & ~DM_dm_submodule_enable_debug_enable_mask);
			DM_B05_Set(pDmExec->bwDmLut.lutMap, 1);
			}
#else
			DM_B05_Set(pDmExec->bwDmLut.lutMap, 1);
#endif
#endif

			//using fix pattern :enable pattern 0
			//disable dither fix pattern at normal case
            rtd_outl(DM_DITHER_SET_reg,DM_DITHER_SET_dither_fix_pattern_en(0) | DM_DITHER_SET_dither_fix_pattern_num(0) | DM_DITHER_SET_dither_in_format(1));
			//set_DM_DITHER_SET_reg(DM_DITHER_SET_dither_fix_pattern_en(0) | DM_DITHER_SET_dither_fix_pattern_num(0));

			// all submodule enable bit12 enable dither
			//bit 11 , 0:enable b05_02 444to422
			//rtd_outl(DM_dm_submodule_enable_reg, 0x17ff);
			rtd_outl(DM_dm_submodule_enable_reg,
					 DM_dm_submodule_enable_dither_en(1) | DM_dm_submodule_enable_b0502_enable(0) |
					 DM_dm_submodule_enable_b0501_enable(1) | DM_dm_submodule_enable_b04_enable(1) |
					 DM_dm_submodule_enable_b02_enable(1)|
					 DM_dm_submodule_enable_b03_enable(1) | DM_dm_submodule_enable_b01_07_enable(1) |
					 DM_dm_submodule_enable_b01_06_enable(1) | DM_dm_submodule_enable_b01_05_enable(1) |
					 DM_dm_submodule_enable_b01_04_enable(1) | DM_dm_submodule_enable_b01_03_enable(1) |
					 DM_dm_submodule_enable_b01_02_enable(1) | DM_dm_submodule_enable_b01_01_422to444_enable(1) |
					 DM_dm_submodule_enable_b01_01_420to422_enable(1));

			//dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
			//dm_double_buf_ctrl_reg.dm_db_apply = 1;
			//rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);

			// enable double buffer
			dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
			dm_double_buf_ctrl_reg.dm_db_apply = 0;
			dm_double_buf_ctrl_reg.dm_db_en = 1;
			rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);

			g_forceUpdate_3DLUT = 0;

			pr_debug("[H] finish \n");
		}

		vgip_time2 = VGIP_VGIP_CHN1_LC_get_ch1_line_cnt(rtd_inl(VGIP_VGIP_CHN1_LC_reg));


		if (vgip_time2 < vgip_time1) {
			//pr_err("HDMI_TEST finish vgip_lc=ori:%d final:%d\n", vgip_time1, vgip_time2);
		}
	}


	//vgip_lineCnt = VGIP_VGIP_CHN1_LC_get_ch1_line_cnt(rtd_inl(VGIP_VGIP_CHN1_LC_reg));
	//printk("leave vgip_lc=%d\n", vgip_lineCnt);
}

//#ifdef RTK_IDK_CRT
void HDMI_TEST_VSIF(unsigned int wid, unsigned int len, unsigned char* mdAddr)
{
	static unsigned int bg_hdmi_last_h_start = 0;
	static unsigned int bg_hdmi_last_h_end = 0;
	static unsigned int bg_hdmi_last_v_start = 0;
	static unsigned int bg_hdmi_last_v_end = 0;
	static unsigned int HDMI_backlightSliderScalar = 0;
#if EN_GLOBAL_DIMMING
# if EN_AOI
	unsigned int h_start, h_end, v_start, v_end;
#endif
#endif
	//MdsChg_t mdsChg = MDS_CHG_TC;

	dm_hdr_double_buffer_ctrl_RBUS dm_double_buf_ctrl_reg;
	hdr_all_top_top_ctl_RBUS hdr_all_top_top_ctl_reg;
	hdr_all_top_top_d_buf_RBUS hdr_all_top_top_d_buf_reg;
	DOLBYVISION_PATTERN dolby;
	DmExecFxp_t *pDmExec = &hdmi_h_dm->dmExec;
	unsigned int vo_vend, vgip_lineCnt, vgipVend;
	unsigned int vgip_time1, vgip_time2;
	E_DV_PICTURE_MODE cur_pic_mode;
	E_DV_PICTURE_MODE cur_backlight_value;
	static unsigned char pair_check = 0;

	ppoverlay_memcdtg_dh_den_start_end_RBUS memcdtg_dh_den_start_end;
	ppoverlay_memcdtg_dv_den_start_end_RBUS memcdtg_dv_den_start_end;
	unsigned int actvie_h = 0;//memc size or panel size
	unsigned int actvie_v = 0;//memc size or panel size
	unsigned char ui_setting_change = 1;//Means UI setting update picture mode and back light
	static unsigned char hdmi_force_3dLutupdate = 1;//when pair_check don't reset last time
	memcdtg_dh_den_start_end.regValue = rtd_inl(PPOVERLAY_memcdtg_DH_DEN_Start_End_reg);
	memcdtg_dv_den_start_end.regValue = rtd_inl(PPOVERLAY_memcdtg_DV_DEN_Start_End_reg);
	actvie_h = memcdtg_dh_den_start_end.memcdtg_dh_den_end-memcdtg_dh_den_start_end.memcdtg_dh_den_sta;
	actvie_v = memcdtg_dv_den_start_end.memcdtg_dv_den_end-memcdtg_dv_den_start_end.memcdtg_dv_den_sta;


	vgip_lineCnt = VGIP_VGIP_CHN1_LC_get_ch1_line_cnt(rtd_inl(VGIP_VGIP_CHN1_LC_reg));
	vo_vend = (rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) >> 16) & 0xfff;
	vgipVend = VGIP_VGIP_CHN1_ACT_VSTA_Length_get_ch1_iv_act_sta(rtd_inl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg))
							+ VGIP_VGIP_CHN1_ACT_VSTA_Length_get_ch1_iv_act_len(rtd_inl(VGIP_VGIP_CHN1_ACT_VSTA_Length_reg));

	hdr_all_top_top_ctl_reg.regValue = rtd_inl(HDR_ALL_TOP_TOP_CTL_reg);
	if ((rtd_inl(DM_dm_submodule_enable_reg) & DM_dm_submodule_enable_b01_02_enable_mask) == 0 || hdr_all_top_top_ctl_reg.dolby_mode == 0) {
		g_lvl4_exist = 0;
#if (defined(CONFIG_RTK_KDRV_PWM))
		if (0 != rtk_pwm_backlight_set_duty(255)) { /*20171226, baker recover for dolby backlight control*/
			printk("%s, PWM control failed...duty=%d \n", __FUNCTION__, 255);
		}
#endif
#ifdef _ENABLE_BACKLIGHT_DELAY_CTRL
		BacklightDelayCtrl_init();
		//Backlight_DelayTiming_Update("/usr/local/etc/backlight_delay.bin");
#endif
	}

	dolby.ColNumTtl = wid;
	dolby.RowNumTtl = len;
	dolby.BL_EL_ratio = 0;

	vo_vend = (rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) >> 16) & 0xfff;

	if(HDMI_last_md_picmode == DV_PIC_NONE_MODE)
	{//means first time
		bg_hdmi_last_h_start = 0;
		bg_hdmi_last_h_end = 0;
		bg_hdmi_last_v_start = 0;
		bg_hdmi_last_v_end = 0;
		last_global_dim_val = -1;//reset last pwm value
	}

	if(vgip_lineCnt < vgipVend) {
		memset(hdmi_h_dm, 0, sizeof(cp_context_t));
        memset(hdmi_h_dm_st, 0, sizeof(cp_context_t_half_st));
        init_cp(hdmi_h_dm,NULL , NULL, (char*)hdmi_h_dm_st);

		#if EN_GLOBAL_DIMMING
		// reset g_forceUpdate_3DLUT
		g_forceUpdate_3DLUT = 0;
		#endif
		if((hdmi_ui_change_delay_count == HDMI_DOLBY_UI_CHANGE_DELAY_APPLY) || (HDMI_last_md_picmode == DV_PIC_NONE_MODE))
		{
			cur_pic_mode = ui_dv_picmode;
			if(HDMI_last_md_picmode != cur_pic_mode)
			{
				if(cur_pic_mode == DV_PIC_BRIGHT_MODE)
					p_dm_hdmi_cfg = &dm_hdmi_bright_cfg;
				else if(cur_pic_mode == DV_PIC_VIVID_MODE)
					p_dm_hdmi_cfg = &dm_hdmi_vivid_cfg;
				else
					p_dm_hdmi_cfg = &dm_hdmi_dark_cfg;
				HDMI_last_md_picmode = cur_pic_mode;
				ui_setting_change = TRUE;
			}
			cur_backlight_value = ui_dv_backlight_value;
			if(HDMI_last_md_backlight != cur_backlight_value)
			{
				//calculate_backlight_setting_for_cfg(p_dm_hdmi_cfg, cur_backlight_value, DOLBY_SRC_HDMI);
				HDMI_last_md_backlight = cur_backlight_value;
				ui_setting_change = TRUE;
			}
			hdmi_ui_change_delay_count = 0xff;
		}

		if(ui_setting_change)
		{
			HDMI_Parser_NeedUpdate = 1;//parser need to update
		}

		HDMI_Parser_NeedUpdate = 1;//parser need to update
		/*baker: it danguous to dm_metadata_base_t to dm_metadata_t,
		   ...but it seems we only need base..haha */
		hdmi_h_dm->dm_reg.colNum = wid;
		hdmi_h_dm->dm_reg.rowNum = len;

		rtk_wrapper_commit_reg_vsif(hdmi_h_dm,
			FORMAT_DOVI,
			(dm_metadata_t *)mdAddr,
			NULL,
			dlbPqConfig_hdmi,
			//dolbyvisionEDR_pq_config_current_view%dolbyvisionEDR_pq_config_num_views,
			2,
			&hdmi_h_dm->dst_comp_reg,
			&hdmi_h_dm->dm_reg,
			&hdmi_h_dm->dm_lut);

		rtk_dm_exec_params_to_DmExecFxp(hdmi_h_dm->h_ks,
			&hdmi_h_dm->dmExec,
			&hdmi_h_dm->dmExec_rtk,
			(MdsExt_t*)mdAddr,
			&hdmi_h_dm->dm_lut,
			dlbPqConfig_hdmi,
			RUN_MODE_FROMHDMI);

		//lut3D B05 table
		memcpy(hdmi_h_dm->dmExec.bwDmLut.lutMap,hdmi_h_dm->dm_lut.lut3D,sizeof(hdmi_h_dm->dm_lut.lut3D));
#if 0
		rtk_hdmi_save(0,
			&hdmi_h_dm->dst_comp_reg,
			&hdmi_h_dm->dm_reg,
			&hdmi_h_dm->dm_lut,
			hdmi_h_dm);
#endif


#if EN_GLOBAL_DIMMING

		//if (g_lvl4_exist)
#ifdef _ENABLE_BACKLIGHT_DELAY_CTRL
		{
			BacklightDelayCtrl_update(hdmi_h_dm->h_dm->tcCtrl.tcTgtSig.maxPq, g_bl_HDMI_apply_delay, DOLBY_SRC_HDMI, HDMI_last_md_picmode, HDMI_backlightSliderScalar);
		}
#else
			BacklightCtrl(hdmi_h_dm->tcCtrl.tMaxPq, HDMI_last_md_picmode);
#endif
#endif

#if EN_GLOBAL_DIMMING
# if EN_AOI
			if (mds_ext.lvl5AoiAvail) {
			// horizontal mask line for Lv5 MD
			h_start = ((mds_ext.active_area_left_offset * actvie_h / wid)<<16);
			h_end = (actvie_h - (mds_ext.active_area_right_offset * actvie_h / wid));
			v_start = ((mds_ext.active_area_top_offset * actvie_v / len)<<16);
			v_end = (actvie_v - (mds_ext.active_area_bottom_offset * actvie_v / len));
			if((h_start != bg_hdmi_last_h_start) || (v_start != bg_hdmi_last_v_start) || (h_end != bg_hdmi_last_h_end) || (v_end != bg_hdmi_last_v_end)) {
				bg_hdmi_last_h_start = h_start;
				bg_hdmi_last_v_start = v_start;
				bg_hdmi_last_h_end = h_end;
				bg_hdmi_last_v_end = v_end;
				spin_lock(&Dolby_Letter_Box_Spinlock);
				dolby_proverlay_background_h_start_end = h_start | h_end;
				dolby_proverlay_background_v_start_end = v_start | v_end;
				letter_box_black_flag = TRUE;
				request_letter_dtg_change = TRUE;
				spin_unlock(&Dolby_Letter_Box_Spinlock);
			}
		} else {
				h_start = 0;
				h_end = actvie_h;
				v_start = 0;
				v_end = actvie_v;
				if((h_start != bg_hdmi_last_h_start) || (v_start != bg_hdmi_last_v_start) || (h_end != bg_hdmi_last_h_end) || (v_end != bg_hdmi_last_v_end)) {
					bg_hdmi_last_h_start = h_start;
					bg_hdmi_last_v_start = v_start;
					bg_hdmi_last_h_end = h_end;
					bg_hdmi_last_v_end = v_end;
					spin_lock(&Dolby_Letter_Box_Spinlock);
					dolby_proverlay_background_h_start_end = h_start | h_end;
					dolby_proverlay_background_v_start_end = v_start | v_end;
					letter_box_black_flag = FALSE;
					request_letter_dtg_change = TRUE;
					spin_unlock(&Dolby_Letter_Box_Spinlock);
				}
		}
#endif
#endif

		// TODO: dolby_v_top setting
		DolbyVTopRbusSet(&dolby, DV_HDMI_MODE);
		hdr_all_top_top_d_buf_reg.regValue = rtd_inl(HDR_ALL_TOP_TOP_D_BUF_reg);
		hdr_all_top_top_d_buf_reg.dolby_double_apply = 1;
		rtd_outl(HDR_ALL_TOP_TOP_D_BUF_reg, hdr_all_top_top_d_buf_reg.regValue);

		// TODO:  DM setting
		//DMRbusSet(DV_HDMI_MODE, ((mdsChg==MDS_CHG_NONE) && !ui_setting_change)?0:1);//!ui_setting_change means picture mode change. We need to update B02
		DMRbusSet(DV_HDMI_MODE, 1);//!ui_setting_change means picture mode change. We need to update B02
		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
		dm_double_buf_ctrl_reg.dm_db_apply = 1;
		rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
		hdmi_force_3dLutupdate = FALSE;

		if(pair_check || ui_setting_change)
		{
			hdmi_force_3dLutupdate = TRUE;//Last time don't run finish part
		}
		//baker
		hdmi_force_3dLutupdate = TRUE;//Last time don't run finish part
		pair_check = 1;
		HDMI_Parser_NeedUpdate = 0;

		pr_debug("[H] position \n");
		vgip_time1 = VGIP_VGIP_CHN1_LC_get_ch1_line_cnt(rtd_inl(VGIP_VGIP_CHN1_LC_reg));
		if((vgip_time1 >= vgipVend) || (vgip_time1 <= vgip_lineCnt))
		{
			//pr_err("VISF HDMI_TEST position vgip_lc=ori:%d final:%d\n", vgip_lineCnt, vgip_time1);
		}
#if 0
/*20170908, pinyen remove update debug_enable(b806b400[14] action in dolby hdmi flow,
this bit is for hdr10 and dolby hdr should reset to 0)*/
#ifdef CONFIG_SUPPORT_SCALER
		//if less than 30, set B05 in porch region.
		//if(Scaler_DispGetInputInfoByDisp(SLR_MAIN_DISPLAY, SLR_INPUT_V_FREQ)/10 > g_i2r_fps_thr) {
			//baker
			rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) | DM_dm_submodule_enable_debug_enable_mask);
			DM_B05_Set(pDmExec->bwDmLut.lutMap, 1);

			// enable double buffer
			dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
			dm_double_buf_ctrl_reg.dm_db_apply = 0;
			dm_double_buf_ctrl_reg.dm_db_en = 1;
			rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
		//}
#endif
#endif

#if 0

			DM_B05_Set(pDmExec->bwDmLut.lutMap, 1);

			// enable double buffer
			dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
			dm_double_buf_ctrl_reg.dm_db_apply = 0;
			dm_double_buf_ctrl_reg.dm_db_en = 1;
			rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
#endif
	}

	//Change to check by VGIP V_END @Crixus 20160321
	//if ((vgip_lineCnt >= vgipVend) && pair_check) {
			if ((vgip_lineCnt >= vgipVend)) {

		vgip_time1 = VGIP_VGIP_CHN1_LC_get_ch1_line_cnt(rtd_inl(VGIP_VGIP_CHN1_LC_reg));
		pair_check = 0; //has to clear pair_check every finish isr


		// disable double buffer if need to update B05
		if (g_forceUpdate_3DLUT || g_picModeUpdateFlag || hdmi_force_3dLutupdate) {
		//if (1) {
			g_forceUpdate_3DLUT = 0;
			dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
			dm_double_buf_ctrl_reg.dm_db_en = 0;
			rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);

			// disable B05
			//rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) & ~DM_dm_submodule_enable_debug_enable_mask);

			// postpone the update B05 timing
			// B05 3D LUT
			if (g_picModeUpdateFlag) {
				g_picModeUpdateFlag--;
			}

#ifdef CONFIG_SUPPORT_SCALER
			//if less than 30, set B05 in porch region.
			if(Scaler_DispGetInputInfoByDisp(SLR_MAIN_DISPLAY, SLR_INPUT_V_FREQ)/10 <= g_i2r_fps_thr) {
				// disable B05
				//rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) & ~DM_dm_submodule_enable_debug_enable_mask);
				DM_B05_Set(pDmExec->bwDmLut.lutMap, 1);
			}
#else
			DM_B05_Set(pDmExec->bwDmLut.lutMap, 1);
#endif

			//using fix pattern :enable pattern 0
			//disable dither fix pattern at normal case
			rtd_outl(DM_DITHER_SET_reg,DM_DITHER_SET_dither_fix_pattern_en(0) | DM_DITHER_SET_dither_fix_pattern_num(0) | DM_DITHER_SET_dither_in_format(1));
			//set_DM_DITHER_SET_reg(DM_DITHER_SET_dither_fix_pattern_en(0) | DM_DITHER_SET_dither_fix_pattern_num(0));

			// all submodule enable bit12 enable dither
			//bit 11 , 0:enable b05_02 444to422
			//rtd_outl(DM_dm_submodule_enable_reg, 0x17ff);
			rtd_outl(DM_dm_submodule_enable_reg,
					 DM_dm_submodule_enable_dither_en(1) | DM_dm_submodule_enable_b0502_enable(0) |
					 DM_dm_submodule_enable_b0501_enable(1) | DM_dm_submodule_enable_b04_enable(1) |
					 DM_dm_submodule_enable_b02_enable(1)|
					 DM_dm_submodule_enable_b03_enable(1) | DM_dm_submodule_enable_b01_07_enable(1) |
					 DM_dm_submodule_enable_b01_06_enable(1) | DM_dm_submodule_enable_b01_05_enable(1) |
					 DM_dm_submodule_enable_b01_04_enable(1) | DM_dm_submodule_enable_b01_03_enable(1) |
					 DM_dm_submodule_enable_b01_02_enable(1) | DM_dm_submodule_enable_b01_01_422to444_enable(1) |
					 DM_dm_submodule_enable_b01_01_420to422_enable(1));

			//dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
			//dm_double_buf_ctrl_reg.dm_db_apply = 1;
			//rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);

			// enable double buffer
			dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
			dm_double_buf_ctrl_reg.dm_db_apply = 0;
			dm_double_buf_ctrl_reg.dm_db_en = 1;
			rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);

			g_forceUpdate_3DLUT = 0;

			pr_debug("[H] finish \n");
		}

		vgip_time2 = VGIP_VGIP_CHN1_LC_get_ch1_line_cnt(rtd_inl(VGIP_VGIP_CHN1_LC_reg));


		if (vgip_time2 < vgip_time1) {
			//pr_err("VSIF HDMI_TEST finish vgip_lc=ori:%d final:%d\n", vgip_time1, vgip_time2);
		}
	}


	//vgip_lineCnt = VGIP_VGIP_CHN1_LC_get_ch1_line_cnt(rtd_inl(VGIP_VGIP_CHN1_LC_reg));
	//printk("leave vgip_lc=%d\n", vgip_lineCnt);
}
//#endif

void DM_B02_Set_Static1dLut(unsigned int mdDmOutAddr)
{
#if 0//need to change dma to lut
	unsigned int timeout = 0xffff;
	dm_dm_b02_lut_ctrl0_RBUS dm_b02_lut_ctrl0_reg;
	int lutIdx = 0, loopIdx;//, lutIdx_tmp;
	static int accessTableFlag = 0;
	uint16_t *tcLut = (uint16_t*)&((DmExecFxp_t_rtk*)mdDmOutAddr)->tcLut;

	DBG_PRINT("tcLut=0x%x\n", tcLut);


	// B02
	lutIdx = 0;
	dm_b02_lut_ctrl0_reg.regValue = rtd_inl(DM_DM_B02_LUT_CTRL0_reg);
	//dm_b02_lut_ctrl0_reg.lut_enable = 1;//Avoid wait timeout
	//dm_b02_lut_ctrl0_reg.lut_acc_start_index = 0;
	dm_b02_lut_ctrl0_reg.b02_lut_rw_sel = 1;

	accessTableFlag = dm_b02_lut_ctrl0_reg.b02_lut_acc_sel;
	//if (dm_b02_lut_ctrl0_reg.lut_acc_sel == 1)
	//	dm_b02_lut_ctrl0_reg.lut_acc_sel = 0;	// write table 0
	//else
	//	dm_b02_lut_ctrl0_reg.lut_acc_sel = 1;	// write table 1

	rtd_outl(DM_DM_B02_LUT_CTRL0_reg, dm_b02_lut_ctrl0_reg.regValue);
/*
	while ((DM_DM_B02_LUT_STATUS0_get_lut_acc_ready(rtd_inl(DM_DM_B02_LUT_STATUS0_reg)) != 1) && timeout){
		//msleep(1);
		timeout --;
	}
	if(!timeout)
	{
		pr_err("\r\n DM_B02_Set_Static1dLut err Wait B02 (0xB806B59C) timeout\r\n");
	}
*/

	/*LUT table[512+3], per loop times 5*/
	for (loopIdx = 0; loopIdx < 103; loopIdx++) {

		//lutIdx_tmp = lutIdx;
		lutIdx = loopIdx * 5;

		rtd_outl(DM_DM_B02_LUT_CTRL2_reg, DM_DM_B02_LUT_CTRL2_b02_wr_lut_data0(tcLut[lutIdx]) |
			DM_DM_B02_LUT_CTRL2_b02_wr_lut_data1(tcLut[lutIdx+1]));
		rtd_outl(DM_DM_B02_LUT_CTRL3_reg, DM_DM_B02_LUT_CTRL3_b02_wr_lut_data2(tcLut[lutIdx+2]) |
			DM_DM_B02_LUT_CTRL3_b02_wr_lut_data3(tcLut[lutIdx+3]));
		rtd_outl(DM_DM_B02_LUT_CTRL4_reg, DM_DM_B02_LUT_CTRL4_b02_wr_lut_data4(tcLut[lutIdx+4]) ); //need to check data4 is right or not
/*		rtd_outl(DM_DM_B02_LUT_CTRL5_reg, DM_DM_B02_LUT_CTRL5_lut_data6(pDmExec->tcLut[lutIdx+6]) |
													DM_DM_B02_LUT_CTRL5_lut_data7(pDmExec->tcLut[lutIdx+7]));*/
		// write index
		//dm_b02_lut_ctrl0_reg.lut_acc_start_index = lutIdx / 8;
		//rtd_outl(DM_DM_B02_LUT_CTRL0_reg, dm_b02_lut_ctrl0_reg.regValue);

		// write_en
		rtd_outl(DM_DM_B02_LUT_CTRL1_reg, DM_DM_B02_LUT_CTRL1_b02_lut_write_en_mask);
		timeout = 0xffff;
		// wait write_en clear
		while ((DM_DM_B02_LUT_CTRL1_get_b02_lut_write_en(rtd_inl(DM_DM_B02_LUT_CTRL1_reg)) != 0) && timeout){
			//msleep(1);
			timeout --;
		}

		if(!timeout)
		{
			pr_err("\r\n DM_B02_Set_Static1dLut err Wait B02 (0xB806B588) timeout\r\n");
			printk(KERN_ERR"\r\n DM_B02_Set_Static1dLut err Wait B02 (0xB806B588) timeout\r\n");
		}

		if (loopIdx == 0) {
			DBG_PRINT("(tcLut[%d])=0x%x \n", (loopIdx*8), tcLut[lutIdx]);
			DBG_PRINT("(tcLut[%d])=0x%x \n", (loopIdx*8)+1, tcLut[lutIdx+1]);
			DBG_PRINT("(tcLut[%d])=0x%x \n", (loopIdx*8)+2, tcLut[lutIdx+2]);
			DBG_PRINT("(tcLut[%d])=0x%x \n", (loopIdx*8)+3, tcLut[lutIdx+3]);
			DBG_PRINT("(tcLut[%d])=0x%x \n", (loopIdx*8)+4, tcLut[lutIdx+4]);
			DBG_PRINT("(tcLut[%d])=0x%x \n", (loopIdx*8)+5, tcLut[lutIdx+5]);
			DBG_PRINT("(tcLut[%d])=0x%x \n", (loopIdx*8)+6, tcLut[lutIdx+6]);
			DBG_PRINT("(tcLut[%d])=0x%x \n", (loopIdx*8)+7, tcLut[lutIdx+7]);
		}
		//printk("0 loopIdx=%d, DM_DM_B02_LUT_CTRL0_reg=0x%x \n", loopIdx, rtd_inl(DM_DM_B02_LUT_CTRL0_reg));
	}

#if 0
	dm_b02_lut_ctrl0_reg.regValue = rtd_inl(DM_DM_B02_LUT_CTRL0_reg);
	//dm_b02_lut_ctrl0_reg.lut_enable = 1;
	dm_b02_lut_ctrl0_reg.lut_acc_start_index = 0;
	dm_b02_lut_ctrl0_reg.lut_rw_sel = 1;
	if (dm_b02_lut_ctrl0_reg.lut_acc_sel == 1)
		dm_b02_lut_ctrl0_reg.lut_acc_sel = 0;	// write table 0
	else
		dm_b02_lut_ctrl0_reg.lut_acc_sel = 1;	// write table 1
	rtd_outl(DM_DM_B02_LUT_CTRL0_reg, dm_b02_lut_ctrl0_reg.regValue);
	while (DM_DM_B02_LUT_STATUS0_get_lut_acc_ready(rtd_inl(DM_DM_B02_LUT_STATUS0_reg)) != 1) {
		msleep(1);
	}

	for (loopIdx = 0; loopIdx < 512; loopIdx++) {

		//lutIdx_tmp = lutIdx;
		lutIdx = loopIdx * 8;

		rtd_outl(DM_DM_B02_LUT_CTRL2_reg, DM_DM_B02_LUT_CTRL2_lut_data0(pDmExec->tcLut[lutIdx]) |
													DM_DM_B02_LUT_CTRL2_lut_data1(pDmExec->tcLut[lutIdx+1]));
		rtd_outl(DM_DM_B02_LUT_CTRL3_reg, DM_DM_B02_LUT_CTRL3_lut_data2(pDmExec->tcLut[lutIdx+2]) |
													DM_DM_B02_LUT_CTRL3_lut_data3(pDmExec->tcLut[lutIdx+3]));
		rtd_outl(DM_DM_B02_LUT_CTRL4_reg, DM_DM_B02_LUT_CTRL4_lut_data4(pDmExec->tcLut[lutIdx+4]) |
													DM_DM_B02_LUT_CTRL4_lut_data5(pDmExec->tcLut[lutIdx+5]));
		rtd_outl(DM_DM_B02_LUT_CTRL5_reg, DM_DM_B02_LUT_CTRL5_lut_data6(pDmExec->tcLut[lutIdx+6]) |
													DM_DM_B02_LUT_CTRL5_lut_data7(pDmExec->tcLut[lutIdx+7]));

		// write index
		//dm_b02_lut_ctrl0_reg.lut_acc_start_index = lutIdx / 8;
		//rtd_outl(DM_DM_B02_LUT_CTRL0_reg, dm_b02_lut_ctrl0_reg.regValue);

		// write_en
		rtd_outl(DM_DM_B02_LUT_CTRL1_reg, DM_DM_B02_LUT_CTRL1_write_en_mask);
		// wait write_en clear
		while (DM_DM_B02_LUT_CTRL1_get_write_en(rtd_inl(DM_DM_B02_LUT_CTRL1_reg)) != 0) {
			msleep(1);
		}

		//printk("1 loopIdx=%d, DM_DM_B02_LUT_CTRL0_reg=0x%x \n", loopIdx, rtd_inl(DM_DM_B02_LUT_CTRL0_reg));
	}
	//printk("1 Final: DM_DM_B02_LUT_CTRL0_reg=0x%x \n", rtd_inl(DM_DM_B02_LUT_CTRL0_reg));
#endif


	// disable write table
	dm_b02_lut_ctrl0_reg.regValue = 0;
	//dm_b02_lut_ctrl0_reg.lut_enable = 1;
	dm_b02_lut_ctrl0_reg.b02_lut_acc_sel = accessTableFlag ? 0: 1;
	rtd_outl(DM_DM_B02_LUT_CTRL0_reg, dm_b02_lut_ctrl0_reg.regValue);
#endif
}



void DMRbusSet_Static1dLut(E_DV_MODE mode, unsigned int B02_update, unsigned int mdDmOutAddr, E_DV_PICTURE_MODE uPicMode)
{
	dm_hdr_double_buffer_ctrl_RBUS dm_double_buf_ctrl_reg;
	//DmExecFxp_t *pDmExec_for3D = h_dm->pDmExec;
	unsigned int first_chk = 0;

	DmExecFxp_t_rtk* pDmExec = (DmExecFxp_t_rtk*)mdDmOutAddr;
	DmExecFxp_t_rtk* pDmExec_part3 = (DmExecFxp_t_rtk*)mdDmOutAddr;

	if ((rtd_inl(DM_dm_submodule_enable_reg) & DM_dm_submodule_enable_b01_02_enable_mask) == 0)
		first_chk = 1;

	DBG_PRINT("%s, pDmExec=0x%x \n", __FUNCTION__, pDmExec);
	DBG_PRINT("%s, pDmExec_part3=0x%x \n", __FUNCTION__, pDmExec_part3);

#if 0	// B05 update move to vo_end timing
	// disable double buffer if need to update B05
	if (g_forceUpdate_3DLUT) {
		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
		dm_double_buf_ctrl_reg.dm_db_en = 0;
		rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
	}

	rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) |
												DM_dm_submodule_enable_dither_en(1));

	// disable B05
	rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) & ~DM_dm_submodule_enable_debug_enable_mask);
#endif


	// enable double buffer
	dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
	dm_double_buf_ctrl_reg.dm_db_apply = 0;
	dm_double_buf_ctrl_reg.dm_db_en = 1;
	rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
	//while (1) {
	//	dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
	//	if (dm_double_buf_ctrl_reg.dm_db_apply == 0)
	//		break;
	//}

	// B01-01
#if 0 //need to check by will
	if ( mode == DV_HDMI_MODE){
		rtd_outl(DM_Input_Format_reg, DM_Input_Format_runmode(1)| DM_Input_Format_dm_inbits_sel(2));
	}else {
		rtd_outl(DM_Input_Format_reg, DM_Input_Format_runmode(0)| DM_Input_Format_dm_inbits_sel(2));
	}
#endif


	// B01-02
	rtd_outl(DM_YCCtoRGB_coef0_reg, DM_YCCtoRGB_coef0_m33yuv2rgb01(pDmExec->m33Yuv2Rgb[0][1]) |
		DM_YCCtoRGB_coef0_m33yuv2rgb00(pDmExec->m33Yuv2Rgb[0][0]));
	rtd_outl(DM_YCCtoRGB_coef1_reg, DM_YCCtoRGB_coef1_m33yuv2rgb02(pDmExec->m33Yuv2Rgb[0][2]));
	rtd_outl(DM_YCCtoRGB_coef2_reg, DM_YCCtoRGB_coef2_m33yuv2rgb11(pDmExec->m33Yuv2Rgb[1][1]) |
		DM_YCCtoRGB_coef2_m33yuv2rgb10(pDmExec->m33Yuv2Rgb[1][0]));
	rtd_outl(DM_YCCtoRGB_coef3_reg, DM_YCCtoRGB_coef3_m33yuv2rgb12(pDmExec->m33Yuv2Rgb[1][2]));
	rtd_outl(DM_YCCtoRGB_coef4_reg, DM_YCCtoRGB_coef4_m33yuv2rgb21(pDmExec->m33Yuv2Rgb[2][1]) |
		DM_YCCtoRGB_coef4_m33yuv2rgb20(pDmExec->m33Yuv2Rgb[2][0]));
	rtd_outl(DM_YCCtoRGB_coef5_reg, DM_YCCtoRGB_coef5_m33yuv2rgb22(pDmExec->m33Yuv2Rgb[2][2]));


	rtd_outl(DM_YCCtoRGB_offset0_reg, DM_YCCtoRGB_offset0_v3yuv2rgboffinrgb0(pDmExec->v3Yuv2RgbOffInRgb[0]));
	rtd_outl(DM_YCCtoRGB_offset1_reg, DM_YCCtoRGB_offset1_v3yuv2rgboffinrgb1(pDmExec->v3Yuv2RgbOffInRgb[1]));
	rtd_outl(DM_YCCtoRGB_offset2_reg, DM_YCCtoRGB_offset2_v3yuv2rgboffinrgb2(pDmExec->v3Yuv2RgbOffInRgb[2]));
	rtd_outl(DM_YCCtoRGB_Scale_reg, DM_YCCtoRGB_Scale_ycctorgb_scale(pDmExec->m33Yuv2RgbScale2P));

	rtd_outl(DM_Signal_range_reg, DM_Signal_range_rangemax(pDmExec->sRange) |
											DM_Signal_range_rangemin(pDmExec->sRangeMin));
	rtd_outl(DM_sRangeInv_reg, DM_sRangeInv_srangeinv(pDmExec->sRangeInv));

	DBG_PRINT("pDmExec->m33Yuv2Rgb[0][1]=0x%x \n", pDmExec->m33Yuv2Rgb[0][1]);
	DBG_PRINT("pDmExec->m33Yuv2Rgb[0][0]=0x%x \n", pDmExec->m33Yuv2Rgb[0][0]);
	DBG_PRINT("pDmExec->m33Yuv2Rgb[0][2]=0x%x \n", pDmExec->m33Yuv2Rgb[0][2]);
	DBG_PRINT("pDmExec->m33Yuv2Rgb[1][1]=0x%x \n", pDmExec->m33Yuv2Rgb[1][1]);
	DBG_PRINT("pDmExec->m33Yuv2Rgb[1][0]=0x%x \n", pDmExec->m33Yuv2Rgb[1][0]);
	DBG_PRINT("pDmExec->m33Yuv2Rgb[1][2]=0x%x \n", pDmExec->m33Yuv2Rgb[1][2]);
	DBG_PRINT("pDmExec->m33Yuv2Rgb[2][1]=0x%x \n", pDmExec->m33Yuv2Rgb[2][1]);
	DBG_PRINT("pDmExec->m33Yuv2Rgb[2][0]=0x%x \n", pDmExec->m33Yuv2Rgb[2][0]);
	DBG_PRINT("pDmExec->m33Yuv2Rgb[2][2]=0x%x \n", pDmExec->m33Yuv2Rgb[2][2]);
	DBG_PRINT("pDmExec->v3Yuv2RgbOffInRgb[0]=0x%x \n", pDmExec->v3Yuv2RgbOffInRgb[0]);
	DBG_PRINT("pDmExec->v3Yuv2RgbOffInRgb[1]=0x%x \n", pDmExec->v3Yuv2RgbOffInRgb[1]);
	DBG_PRINT("pDmExec->v3Yuv2RgbOffInRgb[2]=0x%x \n", pDmExec->v3Yuv2RgbOffInRgb[2]);
	DBG_PRINT("pDmExec->m33Yuv2RgbScale2P=0x%x \n", pDmExec->m33Yuv2RgbScale2P);
	DBG_PRINT("(pDmExec->sRange)=0x%x \n", (pDmExec->sRange));
	DBG_PRINT("(pDmExec->sRangeMin)=0x%x \n", (pDmExec->sRangeMin));
	DBG_PRINT("pDmExec->sRangeInv=0x%x \n", pDmExec->sRangeInv);

	// B01-03
	// TODO: B01-03
/*
	rtd_outl(DM_EOTF_para0_reg, DM_EOTF_para0_signal_eotf_param1(pDmExec->sB) |
											DM_EOTF_para0_signal_eotf_param0(pDmExec->sA));
	rtd_outl(DM_EOTF_para1_reg, DM_EOTF_para1_seotf(pDmExec->sEotf) |
											DM_EOTF_para1_signal_eotf(pDmExec->sGamma));
	rtd_outl(DM_EOTF_para2_reg, DM_EOTF_para2_signal_eotf_param2(pDmExec->sG));
*/
	DBG_PRINT("pDmExec->sB=0x%x \n", pDmExec->sB);
	DBG_PRINT("pDmExec->sA=0x%x \n", pDmExec->sA);
	DBG_PRINT("pDmExec->sEotf=0x%x \n", pDmExec->sEotf);
	DBG_PRINT("pDmExec->sGamma=0x%x \n", pDmExec->sGamma);
	DBG_PRINT("pDmExec->sG=0x%x \n", pDmExec->sG);


	// B01-04
	rtd_outl(DM_RGBtoOPT_coef0_reg, DM_RGBtoOPT_coef0_m33rgb2opt01(pDmExec->m33Rgb2Opt[0][1]) |
											DM_RGBtoOPT_coef0_m33rgb2opt00(pDmExec->m33Rgb2Opt[0][0]));
	rtd_outl(DM_RGBtoOPT_coef1_reg, DM_RGBtoOPT_coef1_m33rgb2opt02(pDmExec->m33Rgb2Opt[0][2]));
	rtd_outl(DM_RGBtoOPT_coef2_reg, DM_RGBtoOPT_coef2_m33rgb2opt11(pDmExec->m33Rgb2Opt[1][1]) |
											DM_RGBtoOPT_coef2_m33rgb2opt10(pDmExec->m33Rgb2Opt[1][0]));
	rtd_outl(DM_RGBtoOPT_coef3_reg, DM_RGBtoOPT_coef3_m33rgb2opt12(pDmExec->m33Rgb2Opt[1][2]));
	rtd_outl(DM_RGBtoOPT_coef4_reg, DM_RGBtoOPT_coef4_m33rgb2opt21(pDmExec->m33Rgb2Opt[2][1]) |
											DM_RGBtoOPT_coef4_m33rgb2opt20(pDmExec->m33Rgb2Opt[2][0]));
	rtd_outl(DM_RGBtoOPT_coef5_reg, DM_RGBtoOPT_coef5_m33rgb2opt22(pDmExec->m33Rgb2Opt[2][2]));
	rtd_outl(DM_RGBtoOPT_Scale_reg, DM_RGBtoOPT_Scale_rgb2optscale(pDmExec->m33Rgb2OptScale2P));

	DBG_PRINT("(pDmExec->m33Rgb2Opt[0][1])=0x%x \n", (pDmExec->m33Rgb2Opt[0][1]));
	DBG_PRINT("(pDmExec->m33Rgb2Opt[0][0])=0x%x \n", (pDmExec->m33Rgb2Opt[0][0]));
	DBG_PRINT("(pDmExec->m33Rgb2Opt[0][2])=0x%x \n", (pDmExec->m33Rgb2Opt[0][2]));
	DBG_PRINT("(pDmExec->m33Rgb2Opt[1][1])=0x%x \n", (pDmExec->m33Rgb2Opt[1][1]));
	DBG_PRINT("(pDmExec->m33Rgb2Opt[1][0])=0x%x \n", (pDmExec->m33Rgb2Opt[1][0]));
	DBG_PRINT("(pDmExec->m33Rgb2Opt[1][2])=0x%x \n", (pDmExec->m33Rgb2Opt[1][2]));
	DBG_PRINT("(pDmExec->m33Rgb2Opt[2][1])=0x%x \n", (pDmExec->m33Rgb2Opt[2][1]));
	DBG_PRINT("(pDmExec->m33Rgb2Opt[2][0])=0x%x \n", (pDmExec->m33Rgb2Opt[2][0]));
	DBG_PRINT("(pDmExec->m33Rgb2Opt[2][2])=0x%x \n", (pDmExec->m33Rgb2Opt[2][2]));
	DBG_PRINT("(pDmExec->m33Rgb2OptScale2P)=0x%x \n", (pDmExec->m33Rgb2OptScale2P));


	// B01-06
	rtd_outl(DM_OPTtoOPT_coef0_reg, DM_OPTtoOPT_coef0_m33opt2opt01(pDmExec->m33Opt2Opt[0][1]) |
											DM_OPTtoOPT_coef0_m33opt2opt00(pDmExec->m33Opt2Opt[0][0]));
	rtd_outl(DM_OPTtoOPT_coef1_reg, DM_OPTtoOPT_coef1_m33opt2opt02(pDmExec->m33Opt2Opt[0][2]));
	rtd_outl(DM_OPTtoOPT_coef2_reg, DM_OPTtoOPT_coef2_m33opt2opt11(pDmExec->m33Opt2Opt[1][1]) |
											DM_OPTtoOPT_coef2_m33opt2opt10(pDmExec->m33Opt2Opt[1][0]));
	rtd_outl(DM_OPTtoOPT_coef3_reg, DM_OPTtoOPT_coef3_m33opt2opt12(pDmExec->m33Opt2Opt[1][2]));
	rtd_outl(DM_OPTtoOPT_coef4_reg, DM_OPTtoOPT_coef4_m33opt2opt21(pDmExec->m33Opt2Opt[2][1]) |
											DM_OPTtoOPT_coef4_m33opt2opt20(pDmExec->m33Opt2Opt[2][0]));
	rtd_outl(DM_OPTtoOPT_coef5_reg, DM_OPTtoOPT_coef5_m33opt2opt22(pDmExec->m33Opt2Opt[2][2]));
	rtd_outl(DM_OPTtoOPT_Offset_reg, DM_OPTtoOPT_Offset_opt2optoffset(pDmExec->Opt2OptOffset));
	rtd_outl(DM_OPTtoOPT_Scale_reg, DM_OPTtoOPT_Scale_opt2optscale(pDmExec->m33Opt2OptScale2P));

	DBG_PRINT("(pDmExec->m33Opt2Opt[0][1])=0x%x \n", (pDmExec->m33Opt2Opt[0][1]));
	DBG_PRINT("(pDmExec->m33Opt2Opt[0][0])=0x%x \n", (pDmExec->m33Opt2Opt[0][0]));
	DBG_PRINT("(pDmExec->m33Opt2Opt[0][2])=0x%x \n", (pDmExec->m33Opt2Opt[0][2]));
	DBG_PRINT("(pDmExec->m33Opt2Opt[1][1])=0x%x \n", (pDmExec->m33Opt2Opt[1][1]));
	DBG_PRINT("(pDmExec->m33Opt2Opt[1][0])=0x%x \n", (pDmExec->m33Opt2Opt[1][0]));
	DBG_PRINT("(pDmExec->m33Opt2Opt[1][2])=0x%x \n", (pDmExec->m33Opt2Opt[1][2]));
	DBG_PRINT("(pDmExec->m33Opt2Opt[2][1])=0x%x \n", (pDmExec->m33Opt2Opt[2][1]));
	DBG_PRINT("(pDmExec->m33Opt2Opt[2][0])=0x%x \n", (pDmExec->m33Opt2Opt[2][0]));
	DBG_PRINT("(pDmExec->m33Opt2Opt[2][2])=0x%x \n", (pDmExec->m33Opt2Opt[2][2]));
	DBG_PRINT("(pDmExec->Opt2OptOffset)=0x%x \n", (pDmExec->Opt2OptOffset));
	DBG_PRINT("(pDmExec->m33Opt2OptScale2P)=0x%x \n", (pDmExec->m33Opt2OptScale2P));


	// B01-07
	rtd_outl(DM_LumaAdj_chromaWeight_reg, DM_LumaAdj_chromaWeight_chromaweight(pDmExec->chromaWeight));

	DBG_PRINT("(pDmExec->chromaWeight)=0x%x \n", (pDmExec->chromaWeight));

	DBG_PRINT("B02_update=%d", B02_update);

	// B02
	if (B02_update) {
		DM_B02_Set_Static1dLut(mdDmOutAddr);
	}


	// B03
	rtd_outl(DM_BlendDbEdge_reg, DM_BlendDbEdge_weight(pDmExec_part3->msWeight) |
										DM_BlendDbEdge_weight_edge(pDmExec_part3->msWeightEdge));
	rtd_outl(DM_FilterRow0_reg, DM_FilterRow0_filter_row_c01(pDmExec_part3->msFilterRow[1]) |
										DM_FilterRow0_filter_row_c00(pDmExec_part3->msFilterRow[0]));
	rtd_outl(DM_FilterRow1_reg, DM_FilterRow1_filter_row_c03(pDmExec_part3->msFilterRow[3]) |
										DM_FilterRow1_filter_row_c02(pDmExec_part3->msFilterRow[2]));
	rtd_outl(DM_FilterRow2_reg, DM_FilterRow2_filter_row_c05(pDmExec_part3->msFilterRow[5]) |
										DM_FilterRow2_filter_row_c04(pDmExec_part3->msFilterRow[4]));
	rtd_outl(DM_FilterCol0_reg, DM_FilterCol0_filter_col_c01(pDmExec_part3->msFilterCol[1]) |
										DM_FilterCol0_filter_col_c00(pDmExec_part3->msFilterCol[0]));
	rtd_outl(DM_FilterCol1_reg, DM_FilterCol1_filter_col_c02(pDmExec_part3->msFilterCol[2]));
	rtd_outl(DM_FilterEdgeRow0_reg, DM_FilterEdgeRow0_filter_edge_row_c01(pDmExec_part3->msFilterEdgeRow[1]) |
											DM_FilterEdgeRow0_filter_edge_row_c00(pDmExec_part3->msFilterEdgeRow[0]));
	rtd_outl(DM_FilterEdgeRow1_reg, DM_FilterEdgeRow1_filter_edge_row_c03(pDmExec_part3->msFilterEdgeRow[3]) |
											DM_FilterEdgeRow1_filter_edge_row_c02(pDmExec_part3->msFilterEdgeRow[2]));
	rtd_outl(DM_FilterEdgeRow2_reg, DM_FilterEdgeRow2_filter_edge_row_c04(pDmExec_part3->msFilterEdgeRow[4]));
	rtd_outl(DM_FilterEdgeCol0_reg, DM_FilterEdgeCol0_filter_edge_col_c01(pDmExec_part3->msFilterEdgeCol[1]) |
											DM_FilterEdgeCol0_filter_edge_col_c00(pDmExec_part3->msFilterEdgeCol[0]));


	DBG_PRINT("(pDmExec_part3->msWeight)=0x%x \n", (pDmExec_part3->msWeight));
	DBG_PRINT("(pDmExec_part3->msWeightEdge)=0x%x \n", (pDmExec_part3->msWeightEdge));
	DBG_PRINT("(pDmExec_part3->msFilterRow[1])=0x%x \n", (pDmExec_part3->msFilterRow[1]));
	DBG_PRINT("(pDmExec_part3->msFilterRow[0])=0x%x \n", (pDmExec_part3->msFilterRow[0]));
	DBG_PRINT("(pDmExec_part3->msFilterRow[3])=0x%x \n", (pDmExec_part3->msFilterRow[3]));
	DBG_PRINT("(pDmExec_part3->msFilterRow[2])=0x%x \n", (pDmExec_part3->msFilterRow[2]));
	DBG_PRINT("(pDmExec_part3->msFilterRow[5])=0x%x \n", (pDmExec_part3->msFilterRow[5]));
	DBG_PRINT("(pDmExec_part3->msFilterRow[4])=0x%x \n", (pDmExec_part3->msFilterRow[4]));
	DBG_PRINT("(pDmExec_part3->msFilterCol[1])=0x%x \n", (pDmExec_part3->msFilterCol[1]));
	DBG_PRINT("(pDmExec_part3->msFilterCol[0])=0x%x \n", (pDmExec_part3->msFilterCol[0]));
	DBG_PRINT("(pDmExec_part3->msFilterCol[2])=0x%x \n", (pDmExec_part3->msFilterCol[2]));
	DBG_PRINT("(pDmExec_part3->msFilterEdgeRow[1])=0x%x \n", (pDmExec_part3->msFilterEdgeRow[1]));
	DBG_PRINT("(pDmExec_part3->msFilterEdgeRow[0])=0x%x \n", (pDmExec_part3->msFilterEdgeRow[0]));
	DBG_PRINT("(pDmExec_part3->msFilterEdgeRow[3])=0x%x \n", (pDmExec_part3->msFilterEdgeRow[3]));
	DBG_PRINT("(pDmExec_part3->msFilterEdgeRow[2])=0x%x \n", (pDmExec_part3->msFilterEdgeRow[2]));
	DBG_PRINT("(pDmExec_part3->msFilterEdgeRow[4])=0x%x \n", (pDmExec_part3->msFilterEdgeRow[4]));
	DBG_PRINT("(pDmExec_part3->msFilterEdgeCol[1])=0x%x \n", (pDmExec_part3->msFilterEdgeCol[1]));
	DBG_PRINT("(pDmExec_part3->msFilterEdgeCol[0])=0x%x \n", (pDmExec_part3->msFilterEdgeCol[0]));


	// B04
	rtd_outl(DM_SaturationAdj_reg, DM_SaturationAdj_saturationadj_offset(pDmExec_part3->offset) |
											DM_SaturationAdj_saturationadj_gain(pDmExec_part3->gain));
	rtd_outl(DM_SaturationGain_reg, DM_SaturationGain_saturationgain(pDmExec_part3->saturationGain));

	DBG_PRINT("(pDmExec_part3->offset)=0x%x \n", (pDmExec_part3->offset));
	DBG_PRINT("(pDmExec_part3->gain)=0x%x \n", (pDmExec_part3->gain));
	DBG_PRINT("(pDmExec_part3->saturationGain)=0x%x \n", (pDmExec_part3->saturationGain));


	// B05
	rtd_outl(DM_ThreeDLUT_MinMaxC1_reg, DM_ThreeDLUT_MinMaxC1_maxc1(pDmExec_part3->bwDmLut.iMaxC1) |
												DM_ThreeDLUT_MinMaxC1_minc1(pDmExec_part3->bwDmLut.iMinC1));
	rtd_outl(DM_ThreeDLUT_MinMaxC2_reg, DM_ThreeDLUT_MinMaxC2_maxc2(pDmExec_part3->bwDmLut.iMaxC2) |
												DM_ThreeDLUT_MinMaxC2_minc2(pDmExec_part3->bwDmLut.iMinC2));
	rtd_outl(DM_ThreeDLUT_MinMaxC3_reg, DM_ThreeDLUT_MinMaxC3_minc3(pDmExec_part3->bwDmLut.iMinC3));
	rtd_outl(DM_ThreeDLUT_MinMaxInv1_reg, DM_ThreeDLUT_MinMaxInv1_idistc1inv(pDmExec_part3->bwDmLut.iDistC1Inv));
	rtd_outl(DM_ThreeDLUT_MinMaxInv2_reg, DM_ThreeDLUT_MinMaxInv2_idistc2inv(pDmExec_part3->bwDmLut.iDistC2Inv));
	rtd_outl(DM_ThreeDLUT_MinMaxInv3_reg, DM_ThreeDLUT_MinMaxInv3_idistc3inv(pDmExec_part3->bwDmLut.iDistC3Inv));


	DBG_PRINT("(pDmExec_part3->bwDmLut.iMaxC1)=0x%x \n", (pDmExec_part3->bwDmLut.iMaxC1));
	DBG_PRINT("(pDmExec_part3->bwDmLut.iMinC1)=0x%x \n", (pDmExec_part3->bwDmLut.iMinC1));
	DBG_PRINT("(pDmExec_part3->bwDmLut.iMaxC2)=0x%x \n", (pDmExec_part3->bwDmLut.iMaxC2));
	DBG_PRINT("(pDmExec_part3->bwDmLut.iMinC2)=0x%x \n", (pDmExec_part3->bwDmLut.iMinC2));
	DBG_PRINT("(pDmExec_part3->bwDmLut.iMinC3)=0x%x \n", (pDmExec_part3->bwDmLut.iMinC3));
	DBG_PRINT("(pDmExec_part3->bwDmLut.iDistC1Inv)=0x%x \n", (pDmExec_part3->bwDmLut.iDistC1Inv));
	DBG_PRINT("(pDmExec_part3->bwDmLut.iDistC2Inv)=0x%x \n", (pDmExec_part3->bwDmLut.iDistC2Inv));
	DBG_PRINT("(pDmExec_part3->bwDmLut.iDistC3Inv)=0x%x \n", (pDmExec_part3->bwDmLut.iDistC3Inv));


	// B05 3D LUT
	//DM_B05_Set(pDmExec_for3D->bwDmLut.lutMap);// B05 update move to vo_end timing


	// 444to422 enable
	//rtd_outl(DM_Format_444to422_reg, 0);

    //using fix pattern :enable pattern 0
	//disable dither fix pattern at normal case
	rtd_outl(DM_DITHER_SET_reg,DM_DITHER_SET_dither_fix_pattern_en(0) | DM_DITHER_SET_dither_fix_pattern_num(0));
    //set_DM_DITHER_SET_reg(DM_DITHER_SET_dither_fix_pattern_en(0) | DM_DITHER_SET_dither_fix_pattern_num(0));

	// all submodule enable bit12 enable dither
	//bit 11 , 0:enable b05_02 444to422
	//rtd_outl(DM_dm_submodule_enable_reg, 0x17ff);
	rtd_outl(DM_dm_submodule_enable_reg,
	DM_dm_submodule_enable_dither_en(1) | DM_dm_submodule_enable_b0502_enable(0) |
	DM_dm_submodule_enable_b0501_enable(1) | DM_dm_submodule_enable_b04_enable(1) |
	DM_dm_submodule_enable_b02_enable(1)|
	DM_dm_submodule_enable_b03_enable(1) | DM_dm_submodule_enable_b01_07_enable(1) |
	DM_dm_submodule_enable_b01_06_enable(1) | DM_dm_submodule_enable_b01_05_enable(1) |
	DM_dm_submodule_enable_b01_04_enable(1) | DM_dm_submodule_enable_b01_03_enable(1) |
	DM_dm_submodule_enable_b01_02_enable(1) | DM_dm_submodule_enable_b01_01_422to444_enable(1) |
	DM_dm_submodule_enable_b01_01_420to422_enable(1));


	// apply
	/*
	dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
	dm_double_buf_ctrl_reg.dm_db_apply = 1;
	rtd_outl(DM_HDR_DM_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
	while (1) {
		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_DM_Double_Buffer_CTRL_reg);
		if (dm_double_buf_ctrl_reg.dm_db_apply == 0)
			break;
	}
	*/
}

void Normal_TEST(DOLBYVISION_MD_OUTPUT *p_mdOutput, unsigned int rpcType)
{
#ifdef IDK_CRF_USE
	if (g_MdFlow == NULL) {
		printk(KERN_ERR"[%s - %d] return...get null g_MdFlow\n",__func__, __LINE__);
		return ;
	}
#endif
	static unsigned int bg_last_h_start = 0;
	static unsigned int bg_last_h_end = 0;
	static unsigned int bg_last_v_start = 0;
	static unsigned int bg_last_v_end = 0;
#if EN_GLOBAL_DIMMING
	unsigned int h_start, h_end, v_start, v_end;
#endif
	dm_hdr_double_buffer_ctrl_RBUS dm_double_buf_ctrl_reg;
	dhdr_v_composer_composer_db_ctrl_RBUS comp_db_ctr_reg;
	hdr_all_top_top_d_buf_RBUS hdr_all_top_top_d_buf_reg;
	unsigned char bLut1dUpdate=1;
	DOLBYVISION_PATTERN dolby;
	unsigned int wid, len, ratioMode, vtop_wid, vtop_len;
	unsigned int lut1dAddr;
	//unsigned int ptsh = Swap4Bytes(p_mdOutput->PTSH);//Scaler_ChangeUINT32Endian(p_mdOutput->PTSH);
	//unsigned int ptsl = Swap4Bytes(p_mdOutput->PTSL);//Scaler_ChangeUINT32Endian(p_mdOutput->PTSL);
	static uint16_t *last_tcLut = NULL;
	unsigned int time1, time2, first_chk = 0, vgip_time1, vgip_time2;
	unsigned int lineCnt_in = 0, vo_vstart = 0, vo_vend = 0, vgip_lineCnt = 0;
	unsigned int mdOutAddr = (unsigned int)p_mdOutput;
	static unsigned int mdOutAddrSave;
	DmExecFxp_t_rtk* pDmExec_part3;
	static unsigned char pair_check = 0;
	static unsigned int pair_mdOutAddr = 0;
	static unsigned char force_3dLutupdate = FALSE;//When picture change or backlight change

	//static unsigned int debug_cnt = 0;

	dhdr_v_composer_composer_RBUS composer_reg;
	ppoverlay_memcdtg_dh_den_start_end_RBUS memcdtg_dh_den_start_end;
	ppoverlay_memcdtg_dv_den_start_end_RBUS memcdtg_dv_den_start_end;
	unsigned int actvie_h = 0;//memc size or panel size
	unsigned int actvie_v = 0;//memc size or panel size
	memcdtg_dh_den_start_end.regValue = rtd_inl(PPOVERLAY_memcdtg_DH_DEN_Start_End_reg);
	memcdtg_dv_den_start_end.regValue = rtd_inl(PPOVERLAY_memcdtg_DV_DEN_Start_End_reg);
	actvie_h = memcdtg_dh_den_start_end.memcdtg_dh_den_end-memcdtg_dh_den_start_end.memcdtg_dh_den_sta;
	actvie_v = memcdtg_dv_den_start_end.memcdtg_dv_den_end-memcdtg_dv_den_start_end.memcdtg_dv_den_sta;

	pDmExec_part3 = (DmExecFxp_t_rtk*)(mdOutAddr+ MD_OFFSET + sizeof(rpu_ext_config_fixpt_main_t));
       if(get_dolby_init_flag())
       {//do parameter reset for fix [ROKUTV-17]omx player play dolby crash
               mdOutAddrSave = 0;
               pair_mdOutAddr = 0;
               pair_check = 0;
               set_dolby_init_flag(FALSE);
       }

	if ((rtd_inl(DM_dm_submodule_enable_reg) & DM_dm_submodule_enable_b02_enable_mask) == 0) {
#ifdef _ENABLE_BACKLIGHT_DELAY_CTRL
		BacklightDelayCtrl_init();
#endif
		first_chk = 1;
		pair_check = 0;
	}

	if (last_tcLut == NULL) {
		last_tcLut = kmalloc(DEF_FXP_TC_LUT_SIZE*sizeof(uint16_t), GFP_KERNEL);
		if (last_tcLut == NULL) {
			printk(KERN_ERR"%s, kmalloc last_tcLut failed \n", __FUNCTION__);
			goto _NORMAL_END_;
		}
	}

	/*DBG_PRINT("PTS=%d@%x\n", ptsh, mdOutAddr);*/

	if ((mdOutAddr & 0x3fffffff) == 0) {
		pr_debug(KERN_EMERG"NULL MD\n");
		goto _NORMAL_END_;
	}

	if(p_mdOutput->own_bit == 0) {
		pr_debug("Own=%d\n", Swap4Bytes(p_mdOutput->own_bit));
		goto _NORMAL_END_;
	}


#if 0//update once. debug use
	if(!g_forceUpdateFirstTime)
	{
		if(rpcType == OTT_STATE_FINISH)
		{
			p_mdOutput->own_bit = 0;
			return;
		}
		else
			return;
	}
#endif

	lut1dAddr = (unsigned int)&((DmExecFxp_t_rtk*)(mdOutAddr + MD_OFFSET +
				sizeof(rpu_ext_config_fixpt_main_t)))->tcLut;

 	DBG_PRINT_L(DV_INFO,"mdOutAddr=0x%x, lut1dAddr=0x%x \n", mdOutAddr, lut1dAddr);

	lineCnt_in = rtd_inl(VODMA_VODMA_LINE_ST_reg) & 0xfff;
	vgip_lineCnt = VGIP_VGIP_CHN1_LC_get_ch1_line_cnt(rtd_inl(VGIP_VGIP_CHN1_LC_reg));
	vo_vstart = rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) & 0xfff;
	vo_vend = (rtd_inl(VODMA_VODMA_V1VGIP_VACT1_reg) >> 16) & 0xfff;

#ifdef IDK_CRF_USE
	pr_debug(KERN_EMERG"%s, vgiplc=%d, lineCnt_in=%d, vo_vend=%d, rpcType=%d \n", __FUNCTION__, vgip_lineCnt, lineCnt_in, vo_vend, rpcType);
	rpu_ext_config_fixpt_main_t *compPtr = mdOutAddr + MD_OFFSET;
	pr_debug(KERN_EMERG"%s: ptsh=0x%x, ptsl=0x%x, composer->poly_coef[0][0][0]=0x%x \n", __FUNCTION__, ptsh, ptsl, compPtr->poly_coef[0][0][0]);

	if (vgip_lineCnt == 0) goto _NORMAL_END_;
	if (vo_vend < 300) goto _NORMAL_END_;

#endif

#if 0//Patch Q6536.  Need to update before mute off//#ifndef IDK_CRF_USE	// for DolbyVision IDK
	if ((rtd_inl(PPOVERLAY_Main_Display_Control_RSV_reg) & PPOVERLAY_Main_Display_Control_RSV_m_force_bg_mask) && rpcType!=0x2379) {
		printk(KERN_EMERG"vgiplc=%d\n", vgip_lineCnt);
		p_mdOutput->own_bit = 0;
		goto _NORMAL_END_;
	}
#endif

	wid = (p_mdOutput->h_v_size & 0xffff0000) >> 16;//h size[16-31]and v size [0-15] Get the video size w
	len = (p_mdOutput->h_v_size & 0x0000ffff);//h size[16-31]and v size [0-15] Get the video size h
	if(!wid)//Avoid wid is 0
	wid = VODMA_VODMA_V1_DSIZE_get_p_y_len(rtd_inl(VODMA_VODMA_V1_DSIZE_reg));
	if(!len)//Avoid len is 0
	len = VODMA_VODMA_V1_DSIZE_get_p_y_nline(rtd_inl(VODMA_VODMA_V1_DSIZE_reg));
	ratioMode = 1;//currently always different//(wid == VODMA2_VODMA2_V1_DSIZE_get_p_y_len(rtd_inl(VODMA2_VODMA2_V1_DSIZE_reg))? 0: 1);
#ifdef CONFIG_SUPPORT_SCALER
	if(Scaler_DispGetInputInfoByDisp(SLR_MAIN_DISPLAY, SLR_INPUT_STATE) == _MODE_STATE_SEARCH)
	{//means first time
		bg_last_h_start = 0;
		bg_last_h_end = 0;
		bg_last_v_start = 0;
		bg_last_v_end = 0;
		last_global_dim_val = -1;//reset last pwm value
	}
#endif
#ifdef IDK_CRF_USE // for DolbyVision IDK
	if (/*lineCnt_in < (vo_vend-1) || */(rpcType == 1))
#else
	//if (lineCnt_in < (vo_vend-1) || rpcType==0x2379)
    if (rpcType == OTT_STATE_POSITION || rpcType == OTT_STATE_WHOLE)
#endif
	{
		vtop_wid = wid;
		vtop_len = len;

		// pixel align for dolby vision video
		// VE compress mode
		if(rtd_inl(VODMA_DECOMP_CTRL0_reg) & _BIT0){
			if(wid % VE_COMP_ALIGN_WIDTH_PIXEL != 0){
				// 64 byte align for better performance issue
				vtop_wid = ((wid + VE_COMP_ALIGN_WIDTH_MASK) & ~VE_COMP_ALIGN_WIDTH_MASK);
				// VE min wrap width = 320
				if(vtop_wid < 320)
					vtop_wid = 320;
			}
			if(len % VE_COMP_ALIGN_HEIGH_PIXEL != 0){
				// 8 line align for VE compress mode
				vtop_len = ((len + VE_COMP_ALIGN_HEIGH_MASK) & ~VE_COMP_ALIGN_HEIGH_MASK);
			}
		}else{
			/*  remove align512
			if (wid <= 2048)
				vtop_wid = 2048;
			else if (wid < 3840 && ((wid%1024) != 0)) {
				vtop_wid = 3840;
			}*/
		       ;
		}
		dolby.ColNumTtl = vtop_wid;//need alignment
		dolby.RowNumTtl = vtop_len;
		dolby.BL_EL_ratio = ratioMode;


#if EN_GLOBAL_DIMMING
#ifdef CONFIG_SUPPORT_SCALER
		if(Scaler_DispGetInputInfoByDisp(SLR_MAIN_DISPLAY, SLR_INPUT_STATE) == _MODE_STATE_ACTIVE) {
#endif
			if (pDmExec_part3->lvl5AoiAvail) {
			DBG_PRINT("pDmExec_part3->lvl5AoiAvail=%d \n", pDmExec_part3->lvl5AoiAvail);
			DBG_PRINT("top=%d, bottom=%d \n", pDmExec_part3->active_area_top_offset, pDmExec_part3->active_area_bottom_offset);

			// horizontal mask line for Lv5 MD
				h_start = ((pDmExec_part3->active_area_left_offset * actvie_h / wid)<<16);
				h_end = (actvie_h - (pDmExec_part3->active_area_right_offset * actvie_h / wid));
				v_start = ((pDmExec_part3->active_area_top_offset * actvie_v / len)<<16);
				v_end = (actvie_v - (pDmExec_part3->active_area_bottom_offset * actvie_v / len));

				if((h_start != bg_last_h_start) || (v_start != bg_last_v_start) || (h_end != bg_last_h_end) || (v_end != bg_last_v_end)) {
					bg_last_h_start = h_start;
					bg_last_v_start = v_start;
					bg_last_h_end = h_end;
					bg_last_v_end = v_end;
					spin_lock(&Dolby_Letter_Box_Spinlock);
					dolby_proverlay_background_h_start_end = h_start | h_end;
					dolby_proverlay_background_v_start_end = v_start | v_end;
					letter_box_black_flag = TRUE;
					request_letter_dtg_change = TRUE;
					spin_unlock(&Dolby_Letter_Box_Spinlock);
					//rtd_outl(PPOVERLAY_Main_Display_Control_RSV_reg, rtd_inl(PPOVERLAY_Main_Display_Control_RSV_reg) | (1<<2));
				}
		} else {
				h_start = 0;
				h_end = actvie_h;
				v_start = 0;
				v_end = actvie_v;
				if((h_start != bg_last_h_start) || (v_start != bg_last_v_start) || (h_end != bg_last_h_end) || (v_end != bg_last_v_end)) {
					bg_last_h_start = h_start;
					bg_last_v_start = v_start;
					bg_last_h_end = h_end;
					bg_last_v_end = v_end;
					spin_lock(&Dolby_Letter_Box_Spinlock);
					dolby_proverlay_background_h_start_end = h_start | h_end;
					dolby_proverlay_background_v_start_end = v_start | v_end;
					letter_box_black_flag = FALSE;
					request_letter_dtg_change = TRUE;
					spin_unlock(&Dolby_Letter_Box_Spinlock);
					//rtd_outl(PPOVERLAY_Main_Display_Control_RSV_reg, rtd_inl(PPOVERLAY_Main_Display_Control_RSV_reg) & ~(1<<2));
				}
			}
#ifdef CONFIG_SUPPORT_SCALER
		}
#endif
#endif

		// TODO: dolby_v_top setting
		DolbyVTopRbusSet(&dolby, DV_NORMAL_MODE);

		hdr_all_top_top_d_buf_reg.regValue = rtd_inl(HDR_ALL_TOP_TOP_D_BUF_reg);
		hdr_all_top_top_d_buf_reg.dolby_double_apply = 1;
		rtd_outl(HDR_ALL_TOP_TOP_D_BUF_reg, hdr_all_top_top_d_buf_reg.regValue);


		// force update in 1st frame
		if((rtd_inl(DM_dm_submodule_enable_reg) & DM_dm_submodule_enable_b02_enable_mask) == 0
		    /*|| ((rtd_inl(DOLBY_V_TOP_TOP_CTL_reg) & 0x7) != 2) */) {
			bLut1dUpdate = 1;
		}else if(lut1dAddr) {// compare 1D LUT
			bLut1dUpdate = memcmp((unsigned char*)last_tcLut, (unsigned char*)lut1dAddr, DEF_FXP_TC_LUT_SIZE*sizeof(uint16_t));
		}
		if (bLut1dUpdate != 0)
			memcpy(last_tcLut, (void *)lut1dAddr, DEF_FXP_TC_LUT_SIZE*sizeof(uint16_t));
		bLut1dUpdate = 1;//B02 update

		force_3dLutupdate = FALSE;
		if((OTT_last_md_picmode != p_mdOutput->PicMode) || (OTT_last_md_backlight != p_mdOutput->BackLight))
		{//picture change or backlight change
			force_3dLutupdate = TRUE; //force update 3d
		}
		OTT_last_md_picmode = p_mdOutput->PicMode;
		OTT_last_md_backlight = p_mdOutput->BackLight;

		DBG_PRINT_L(DV_INFO, "ch=0x%x\n", lut1dAddr);
		// TODO:  DM setting
		DMRbusSet_Static1dLut(DV_NORMAL_MODE, bLut1dUpdate , (mdOutAddr + MD_OFFSET + sizeof(rpu_ext_config_fixpt_main_t)), OTT_last_md_picmode);

#if EN_GLOBAL_DIMMING
#ifdef _ENABLE_BACKLIGHT_DELAY_CTRL // backlight delay control
		BacklightDelayCtrl_update(pDmExec_part3->pwm_duty, g_bl_OTT_apply_delay, DOLBY_SRC_OTT, OTT_last_md_picmode, p_mdOutput->BacklightSliderScalar);

#else
	if (g_lvl4_exist)
		BacklightCtrl(pDmExec_part3->pwm_duty, OTT_last_md_picmode);
#endif
#endif
		// TODO:  Composer setting
		ComposerRbusSet((rpu_ext_config_fixpt_main_t *)(mdOutAddr + MD_OFFSET));
#ifdef RTK_EDBEC_1_5
		composer_reg.regValue = rtd_inl(DHDR_V_COMPOSER_Composer_reg);
		if (composer_reg.vdr_bit_depth == 0xC) {
				// DM_Input_Format_dm_inbits_sel is 12 bit for Single Layer NBC (14/12b 420 output)
				rtd_outl(DM_Input_Format_reg, rtd_inl(DM_Input_Format_reg) | DM_Input_Format_dm_inbits_sel(2));
		}else{
			// DM_Input_Format_dm_inbits_sel is 14 bit
			rtd_outl(DM_Input_Format_reg, rtd_inl(DM_Input_Format_reg) | DM_Input_Format_dm_inbits_sel(3));
		}
#endif
		dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
		dm_double_buf_ctrl_reg.dm_db_apply = 1;
		comp_db_ctr_reg.regValue = rtd_inl(DHDR_V_COMPOSER_Composer_db_ctrl_reg);
		comp_db_ctr_reg.composer_db_apply = 1;

		rtd_outl(DHDR_V_COMPOSER_Composer_db_ctrl_reg, comp_db_ctr_reg.regValue);
		rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);


		if (pair_check == 1) {
			// clear own_bit
                      if(mdOutAddrSave){
                               if (((DOLBYVISION_MD_OUTPUT *)mdOutAddrSave)->own_bit) {
                                       ((DOLBYVISION_MD_OUTPUT *)mdOutAddrSave)->own_bit = 0;
                                       DBG_PRINT("own=%d\n", Swap4Bytes(((DOLBYVISION_MD_OUTPUT *)mdOutAddrSave)->own_bit));
                               }
                  	}
			force_3dLutupdate = TRUE;//Last do not apply 3d LUT, so this time fource update
		}
		pr_debug(KERN_EMERG"%s: Position RPC updated \n", __FUNCTION__);

		mdOutAddrSave = (unsigned int)p_mdOutput;
		pair_check = 1;
		pair_mdOutAddr = mdOutAddr;

		time2 = rtd_inl(VODMA_VODMA_LINE_ST_reg);
		if (time2 < lineCnt_in)
			printk("Position lc=%d-%d\n", lineCnt_in, time2);

		if (rpcType != OTT_STATE_WHOLE && Get_OTT_Force_update() == FALSE)
			goto _NORMAL_END_;

	}

#ifdef IDK_CRF_USE // for DolbyVision IDK
	if (rpcType == 2)
#else
	//if (lineCnt_in >= (vo_vend-1) || rpcType==0x2379 || Get_OTT_Force_update())
	if (rpcType == OTT_STATE_FINISH || rpcType == OTT_STATE_WHOLE || Get_OTT_Force_update())
#endif
	{

        if(Get_OTT_Force_update())
        {
            Set_OTT_Force_update(FALSE);
        }
#ifndef IDK_CRF_USE
		// check paire status with B02 setting
		if (!pair_check) {
			if (pair_mdOutAddr != 0)
				*(u32 *)pair_mdOutAddr = 0;
			p_mdOutput->own_bit = 0;
			//printk("[ERR] not paired %x-%x\n", pair_mdOutAddr, mdOutAddr);
			printk("[ERR] NPaired\n");
			goto _NORMAL_END_;
		} else if (mdOutAddr != pair_mdOutAddr) {
			if (pair_mdOutAddr != 0)
				*(u32 *)pair_mdOutAddr = 0;
			p_mdOutput->own_bit = 0;
			pair_check = 0;
			printk("[ERR] Diff ptr %x->%x\n", pair_mdOutAddr, mdOutAddr);
			goto _NORMAL_END_;
		}
#else
		vtop_wid = wid;
		vtop_len = len;

		// pixel align for dolby vision video
		// VE compress mode
		if(rtd_inl(VODMA_DECOMP_CTRL0_reg) & _BIT0){
			if(wid % VE_COMP_ALIGN_WIDTH_PIXEL != 0){
				// 64 byte align for better performance issue
				vtop_wid = ((wid + VE_COMP_ALIGN_WIDTH_MASK) & ~VE_COMP_ALIGN_WIDTH_MASK);
				// VE min wrap width = 320
				if(vtop_wid < 320)
					vtop_wid = 320;
			}
			if(len % VE_COMP_ALIGN_HEIGH_PIXEL != 0){
				// 8 line align for VE compress mode
				vtop_len = ((len + VE_COMP_ALIGN_HEIGH_MASK) & ~VE_COMP_ALIGN_HEIGH_MASK);
			}
		}else{
			/*  remove align512
			if (wid <= 2048)
				vtop_wid = 2048;
			else if (wid < 3840 && ((wid%1024) != 0)) {
				vtop_wid = 3840;
			}*/
		       ;
		}
		dolby.ColNumTtl = vtop_wid;
		dolby.RowNumTtl = vtop_len;
		dolby.BL_EL_ratio = ratioMode;
		if (!pair_check) {
			printk("[ERR] Finish RPC too early, pair_check=%d, DOLBYVISION_IDK_TRIGGER_PORT=0x%x \n", pair_check, rtd_inl(DOLBYVISION_IDK_TRIGGER_PORT));
			/* check SENSIO FIFO status */
			rtd_outl(DOLBY_V_TOP_SENSIO_FIFO_STATUS_reg, 0x1);	// write one to clear
			Normal_PositionRPCReDo(dolby, wid, len, ratioMode, mdOutAddr, pDmExec_part3);
			/* check SENSIO FIFO status */
			rtd_outl(DOLBY_V_TOP_SENSIO_FIFO_STATUS_reg, 0x1);	// write one to clear
			goto _NORMAL_END_;
		}

		// for safe for that problem below
		// when finish RPC came first, then positon RPC came; pair_check will keep 1
		// finish RPC came first again at next frame, pair_check will clear to zero, then position RPC can't set
		Normal_PositionRPCReDo(dolby, wid, len, ratioMode, mdOutAddr, pDmExec_part3);
#endif
		time1 = rtd_inl(VODMA_VODMA_LINE_ST_reg);
		vgip_time1 = VGIP_VGIP_CHN1_LC_get_ch1_line_cnt(rtd_inl(VGIP_VGIP_CHN1_LC_reg));


		// disable double buffer if need to update B05
#if EN_GLOBAL_DIMMING
		if (pDmExec_part3->update3DLut || g_forceUpdateFirstTime || force_3dLutupdate)
#else
		if (g_forceUpdate_3DLUT)
#endif
		{

			dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
			dm_double_buf_ctrl_reg.dm_db_en = 0;
			dm_double_buf_ctrl_reg.dm_db_read_sel = 1;
			rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);

			// disable B05 rtd_outl
			rtd_outl(DM_dm_submodule_enable_reg, rtd_inl(DM_dm_submodule_enable_reg) & ~DM_dm_submodule_enable_debug_enable_mask);

			// postpone the update B05 timing
			// B05 3D LUT
#if EN_GLOBAL_DIMMING
			DM_B05_Set(pDmExec_part3->bwDmLut.lutMap, 1);
			g_forceUpdateFirstTime = 0;
			pr_debug("%s, GD g_forceUpdate_3DLUT done, update3DLut=%d \n", __FUNCTION__, pDmExec_part3->update3DLut);
#else
			DM_B05_Set(pDmExec_part3->bwDmLut.lutMap, 1);

#endif

			//g_picModeUpdateFlag = 0; //Mark by Will No need

			// all submodule enable
			//rtd_outl(DM_dm_submodule_enable_reg, 0xfff);

			//disable dither fix pattern at normal case
            rtd_outl(DM_DITHER_SET_reg,DM_DITHER_SET_dither_fix_pattern_en(0) | DM_DITHER_SET_dither_fix_pattern_num(0));
			//set_DM_DITHER_SET_reg(DM_DITHER_SET_dither_fix_pattern_en(0) | DM_DITHER_SET_dither_fix_pattern_num(0));

			// all submodule enable bit12 enable dither
			//bit 11 , 0:enable b05_02 444to422
			//rtd_outl(DM_dm_submodule_enable_reg, 0x17ff);
			rtd_outl(DM_dm_submodule_enable_reg,
					 DM_dm_submodule_enable_dither_en(1) | DM_dm_submodule_enable_b0502_enable(0) |
					 DM_dm_submodule_enable_b0501_enable(1) | DM_dm_submodule_enable_b04_enable(1) |
					 DM_dm_submodule_enable_b02_enable(1)|
					 DM_dm_submodule_enable_b03_enable(1) | DM_dm_submodule_enable_b01_07_enable(1) |
					 DM_dm_submodule_enable_b01_06_enable(1) | DM_dm_submodule_enable_b01_05_enable(1) |
					 DM_dm_submodule_enable_b01_04_enable(1) | DM_dm_submodule_enable_b01_03_enable(1) |
					 DM_dm_submodule_enable_b01_02_enable(1) | DM_dm_submodule_enable_b01_01_422to444_enable(1) |
					 DM_dm_submodule_enable_b01_01_420to422_enable(1));

			// enable double buffer
			dm_double_buf_ctrl_reg.regValue = rtd_inl(DM_HDR_Double_Buffer_CTRL_reg);
			dm_double_buf_ctrl_reg.dm_db_apply = 0;
			dm_double_buf_ctrl_reg.dm_db_en = 1;
			rtd_outl(DM_HDR_Double_Buffer_CTRL_reg, dm_double_buf_ctrl_reg.regValue);
		}

		time2 = rtd_inl(VODMA_VODMA_LINE_ST_reg);
		vgip_time2 = VGIP_VGIP_CHN1_LC_get_ch1_line_cnt(rtd_inl(VGIP_VGIP_CHN1_LC_reg));
		if ((time2 < time1)||(vgip_time2 < vgip_time1)) {
			if (time2 < time1)
				printk("lc=%d-%d\n", time1, time2);
			if (vgip_time2 < vgip_time1)
				printk("vgip_lc=%d-%d\n", vgip_time1, vgip_time2);
		}

		pair_check = 0;

		// clear own_bit
		pr_debug("\r\n####Normal_TEST Clear PTSH:0x%x(%d)", Swap4Bytes(p_mdOutput->PTSH), Swap4Bytes(p_mdOutput->PTSH));
		p_mdOutput->own_bit = 0;
		//DBG_PRINT("own=%d\n", Swap4Bytes(p_mdOutput->own_bit));
		//*(u32 *)pair_mdOutAddr = 0;

	} else {
		printk(KERN_EMERG"%s, No match condition, vgiplc=%d, lineCnt_in=%d, vo_vend=%d, rpcType=0x%x \n", __FUNCTION__, vgip_lineCnt, lineCnt_in, vo_vend, rpcType);
	}

_NORMAL_END_:


	if (first_chk)
		pr_debug("[DBG] 1st reset\n");

	pr_debug(KERN_EMERG"%s, vgiplc=%d, lineCnt_in=%d, vo_vend=%d, rpcType=0x%x \n", __FUNCTION__, vgip_lineCnt, lineCnt_in, vo_vend, rpcType);
	pr_debug(KERN_EMERG"%s, vo1_dcfg=0x%x, vo2_dcfg=0x%x \n", __FUNCTION__, rtd_inl(VODMA_VODMA_V1_DCFG_reg), rtd_inl(VODMA2_VODMA2_V1_DCFG_reg));

	return;
}


#endif
