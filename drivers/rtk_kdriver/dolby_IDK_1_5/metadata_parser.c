/**
 * This product contains one or more programs protected under international
 * and U.S. copyright laws as unpublished works.  They are confidential and
 * proprietary to Dolby Laboratories.  Their reproduction or disclosure, in
 * whole or in part, or the production of derivative works therefrom without
 * the express permission of Dolby Laboratories is prohibited.
 * Copyright 2011 - 2015 by Dolby Laboratories.
 * All rights reserved.
 *
 * @brief Dolby Vision metadata parser unit test application
 * @file  metadata_parser.c
 *
 * $Id$
 */


/*-----------------------------------------------------------------------------+
 |                            I N C L U D E S
 +----------------------------------------------------------------------------*/

//RTK MARK
#ifdef RTK_EDBEC_CONTROL
#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <asm/barrier.h>
#include <asm/cacheflush.h>
#include <mach/irqs.h>
#include <mach/platform.h>
#include <rtk_kdriver/panelConfigParameter.h>
#include <mach/system.h>
#include <../demux/rtkdemux_common.h>

#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/random.h>
#include <linux/pageremap.h>
#include <linux/kthread.h>
#include <linux/time.h>

#include <rbus/sb2_reg.h>
#include <rbus/timer_reg.h>
#include <mach/rtk_log.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#endif
#include <target_display_config.h>

#include <rpu_ext_config.h>
#include <rpu_api_common.h>
#include <rpu_api_decoder.h>
#include <dv_md_parser.h>
#include <rpu_std.h>

//RTK MARK
#ifdef RTK_EDBEC_CONTROL

#include "typedefs.h"

#ifdef RTK_EDBEC_1_5
#include "control_path_priv.h"
#else
#include "DmTypeDef.h"
#include "VdrDmAPI.h"
#endif

#include "dolbyvisionEDR.h"


rpu_data_header_t g_rpu_data_header;				// addedy by smfan
vdr_rpu_data_payload_t g_vdr_rpu_data_payload;	// addedy by smfan
vdr_dm_data_payload_t g_vdr_dm_data_payload;		// addedy by smfan
dm_metadata_t g_dm_metadata;							// addedy by smfan

unsigned char g_parser_ok = 0;	// added by smfan; one MD of frame parse done
int g_mdparser_output_type = -1;		/* 0: for normal case, 1: for certification case */

/* rpu MD size per frame */
volatile unsigned int g_rpu_size = 0, g_more_eos_err = 0;

unsigned int g_recordDMAddr = 0, g_dvframeCnt = 0;
extern unsigned int OTT_backlightSliderScalar;
extern long g_dump_flag;
extern long dump_frame_cnt;

#endif
/*-----------------------------------------------------------------------------+
 |                   M A C R O    D E F I N I T I O N S
 +----------------------------------------------------------------------------*/

#define FILENAME_LEN_MAX               512
#define BITSTREAM_BUFFER_SIZE          1024

#ifdef _MSC_VER
#define snprintf(dst,max_len,...)      _snprintf_s(dst,max_len,_TRUNCATE,__VA_ARGS__)
#define sscanf                         sscanf_s
#define fopen                          win_fopen
#endif



/*-----------------------------------------------------------------------------+
 |              D A T A    T Y P E    D E F I N I T I O N S
 +----------------------------------------------------------------------------*/

/*! @brief Metadata parser application callback context
*/

typedef struct _app_ctx_s
{
    char                        *md_in;
    char                        *comp_out;
    char                        *dm_out;
   rpu_data_t                   rpu_data_outbuf;
    rpu_ext_config_fixpt_main_t  main_cfg;
    char*                        dm_byte_sequence;
    dv_md_parser_conf_t          conf;
    dv_md_parser_handle_t        h_md_parser;
} app_ctx_t;



/*-----------------------------------------------------------------------------+
 |                         P R I V A T E    D A T A
 +----------------------------------------------------------------------------*/

//RTK MARK
#ifdef RTK_EDBEC_CONTROL
app_ctx_t		   ctx;	// modified to global variable by smfan
#endif

/*-----------------------------------------------------------------------------+
 |                  P R I V A T E    F U N C T I O N S
 +----------------------------------------------------------------------------*/
static dv_md_parser_handle_t g_rtk_parser_cache_ptr = NULL;
static dv_md_parser_handle_t g_rtk_parser_noncache_ptr = NULL;
static unsigned int g_rtk_parser_size = 0;

static void* rtk_dolby_kmalloc_and_save(unsigned int size) {
	if(size == g_rtk_parser_size) {
		if(g_rtk_parser_noncache_ptr)
			return g_rtk_parser_noncache_ptr;

	}

	if(g_rtk_parser_cache_ptr) {
		printk(KERN_ERR"[%s]ONCE. dvr_free %d MD parser, ptr=%p, for new size=%d\n",__func__, g_rtk_parser_size, g_rtk_parser_cache_ptr,size);
		
		dvr_free(g_rtk_parser_cache_ptr);
		g_rtk_parser_cache_ptr = NULL;
		g_rtk_parser_noncache_ptr = NULL;
		g_rtk_parser_size = 0;

	}

	g_rtk_parser_cache_ptr = (dv_md_parser_handle_t)dvr_malloc_uncached_specific(size, GFP_DCU2_FIRST, &g_rtk_parser_noncache_ptr);

	if(g_rtk_parser_cache_ptr == NULL) {
		printk(KERN_ERR"[%s] size(%d) dvr_malloc fail! Can't parser MD data\n",__func__, size);
		g_rtk_parser_noncache_ptr = NULL;
		g_rtk_parser_size = 0;
		return 0;
	}
	printk(KERN_NOTICE "[%s]ONCE. dvr_malloc_ %d for MD parser, ptr=%p\n",__func__, size, g_rtk_parser_cache_ptr);
	memset(g_rtk_parser_noncache_ptr, 0x0,size);
	g_rtk_parser_size = size;
	return g_rtk_parser_noncache_ptr;
}


/*-----------------------------------------------------------------------------+
 | Windows file open
 +----------------------------------------------------------------------------*/

#ifdef _MSC_VER
static FILE *win_fopen
    (
    const char *_filename,
    const char *_mode
    )
{
    FILE *f = NULL;

    fopen_s(&f, _filename, _mode);
    return f;
}
#endif



/*-----------------------------------------------------------------------------+
 | Display the application usage help
 +----------------------------------------------------------------------------*/

#ifndef RTK_EDBEC_CONTROL
static void display_help
    (
    int    status,
    char  *appname
    )
{
    printf("\nDolby Vision metadata parser test application\n");
    printf(" RPU syntax version: %s\n", dv_md_parser_get_api_ver());
    printf("Library API version: %s\n", dv_md_parser_get_algo_ver());
    printf("Usage: %s [options]\n", appname);
    printf("\t -h                       To see this information\n");
    printf("\t -rpu  <filename>         RPU input file name\n");
    printf("\t -comp <filename>         Composer binary output file name\n");
    printf("\t -dm   <filename>         DM binary output file name\n");
   exit(status);
}
#endif


/*-----------------------------------------------------------------------------+
 | Parse the command line options
 +----------------------------------------------------------------------------*/

#ifndef RTK_EDBEC_CONTROL
static char  md_file_name[FILENAME_LEN_MAX];
static char  comp_file_name[FILENAME_LEN_MAX];
static char  dm_file_name[FILENAME_LEN_MAX];
static int parse_cmd_line_option
    (
    int    argc,
    char  *argv[]
    )
{
    int32_t  param_num;

    if (argc < 6)
    {
        display_help(EXIT_FAILURE, argv[0]);
    }
    param_num = 1;

    while (param_num < argc)
    {
        if (strcmp( "-h", argv[param_num]) == 0)
        {
            display_help(EXIT_SUCCESS, argv[0]);
        }
        else if (strcmp("-rpu", argv[param_num]) == 0)
        {
            if ((param_num + 1) >= argc)
            {
                display_help(EXIT_FAILURE, argv[0]);
            }
            snprintf(md_file_name, sizeof(md_file_name),
                     "%s", argv[++param_num]);
        }
        else if (strcmp("-comp", argv[param_num]) == 0)
        {
            if ((param_num + 1) >= argc)
            {
                display_help(EXIT_FAILURE, argv[0]);
            }
            snprintf(comp_file_name, sizeof(comp_file_name),
                     "%s", argv[++param_num]);
        }
        else if (strcmp("-dm", argv[param_num]) == 0)
        {
            if ((param_num + 1) >= argc)
            {
                display_help(EXIT_FAILURE, argv[0]);
            }
            snprintf(dm_file_name, sizeof(dm_file_name),
                     "%s", argv[++param_num]);
        }
        else
        {
            printf("Unknown Parameter: %s\n", argv[param_num]);
            display_help(EXIT_FAILURE, argv[0]);
        }

        param_num++;
    }

    return 0;
}
#endif

extern unsigned int cur_buffer_num;

unsigned int cur_time;

/*-----------------------------------------------------------------------------+
 | Metadata parser event notification callback handler.
 +----------------------------------------------------------------------------*/
static void md_parser_evt_handler_cb
    (
    void                   *ctx_arg,
    dv_md_parser_evt_t      md_parser_evt,
    timestamp_t            *p_pts,
    rpu_data_header_t      *hdr,
    vdr_rpu_data_payload_t *rpu_payload,
    int                     rpu_payload_sz,
    vdr_dm_data_payload_t  *dm_payload,
    int                     dm_payload_sz,
    int                     len
    )
{
	int rc = 0;
	int dm_byte_sequence_length;
	static int frame_nr = 0;
	app_ctx_t  *p_ctx = (app_ctx_t *)ctx_arg;
#ifndef WIN32
	DOLBYVISION_MD_OUTPUT *outPtr = NULL;
#endif
	unsigned int outAddr;//, idx;
	//just update 1Model_1File
	DolbyPQ_1Model_1File_Config(0,0);

	frame_nr++;
	if (md_parser_evt == MD_PARSER_SAMPLE_AVAIL_EVT) {
		/* write composer metadata */
		memset(&p_ctx->main_cfg, 0, sizeof(p_ctx->main_cfg));
		comp_rpu_2_main_cfg(hdr, rpu_payload, &p_ctx->main_cfg);

		//fox modify for 5008 case
		if(p_ctx->main_cfg.disable_residual_flag == 0){
			el_not_support_handler();//when detect FEL type notice omx
			//printk(KERN_ERR " func=%s line=%d detect FEL , not support \n",__FUNCTION__,__LINE__);
			return;
		}
		
		rc = validate_composer_config(&p_ctx->main_cfg, frame_nr, p_ctx->conf.pf_msg_log, S_DEBUG);
		if (rc != 0) {
			printk(KERN_ERR"%s-1, %s\n", __func__,rpu_error_code_2_str(rc));
		}
//RTK MARK
#ifndef RTK_EDBEC_CONTROL
		fwrite(&p_ctx->main_cfg, 1, sizeof(p_ctx->main_cfg), p_ctx->comp_out);
#else
		if(g_dump_flag & DV_DUMP_COMPOSE){
			int num,i;
			unsigned char *ptr = (unsigned char*)&p_ctx->main_cfg;

			num = sizeof(p_ctx->main_cfg);
			DBG_PRINT_L(DV_INFO,"\n===========  COMPOSE dump %d ===========\n",g_dvframeCnt);
			for(i=0;i<num;){
				DBG_PRINT_L(DV_INFO,"%02x ",*ptr);
				i++;
				ptr++;
				if(i%16 == 0)
			DBG_PRINT_L(DV_INFO,"\n");
			}
			DBG_PRINT_L(DV_INFO,"\n===========  COMPOSE dump end ===========\n");
		}
		if(g_dump_flag & DV_DUMP_COMPOSE_FILE){
			char data[128];
			sprintf(data,"/tmp/compose_%d.bin",g_dvframeCnt);
			if(dump_frame_cnt == g_dvframeCnt || dump_frame_cnt == 0)
				MetaData_WriteBack_file(data,(unsigned char *)&p_ctx->main_cfg,sizeof(p_ctx->main_cfg),1);
			else
				MetaData_WriteBack_file(data,(unsigned char *)&p_ctx->main_cfg,sizeof(p_ctx->main_cfg),0);
		}
		// added by smfan
		if (g_mdparser_output_type == 0) {
			/* write MD output data to ring buffer */
			outAddr = DV_SearchAvailOutBuffer();
			if (outAddr == 0) {
				g_parser_ok = 1;
				return ;
			}
			outPtr = (DOLBYVISION_MD_OUTPUT *)outAddr;
			outPtr->PTSH = Swap4Bytes(g_pts_info.PTSH);
			outPtr->PTSL = Swap4Bytes(g_pts_info.PTSL);
			outPtr->Real_PTSH = Swap4Bytes(g_pts_info.PTSH2);//Real PTSH
			outPtr->Real_PTSL = Swap4Bytes(g_pts_info.PTSL2);//Real PTSL
			p_ctx->comp_out = (char *)(outPtr->comp_metadata);
			p_ctx->dm_out = (p_ctx->comp_out + sizeof(p_ctx->main_cfg));

		} else if (g_mdparser_output_type == 1) {

			// hack for metadata check
			p_ctx->comp_out = (char *)(g_compMDAddr + (g_dvframeCnt * sizeof(p_ctx->main_cfg)));
			if (g_recordDMAddr == 0)
				p_ctx->dm_out = (char *)g_dmMDAddr;
			else
				p_ctx->dm_out = (char *)g_recordDMAddr;
		} else {
			g_parser_ok = 1;
			return ;
		}
		DBG_PRINT_L(DV_INFO,"p_ctx->comp_out=0x%x \n", p_ctx->comp_out);
		DBG_PRINT_L(DV_INFO,"p_ctx->dm_out=0x%x \n", p_ctx->dm_out);


		memcpy(p_ctx->comp_out, &p_ctx->main_cfg, sizeof(p_ctx->main_cfg));		// modified by smfan
		//p_ctx->comp_out += sizeof(p_ctx->main_cfg);	// mask by smfan
#endif
		/* write DM metadata */
		rc = validate_dm_payload (dm_payload, frame_nr, p_ctx->conf.pf_msg_log, S_DEBUG);
		if (rc != 0) {
			printk(KERN_ERR"%s-2 %s\n",__func__, rpu_error_code_2_str(rc));
		}
		dm_byte_sequence_length = sizeof(dm_metadata_t);
		rc = dm_rpu_payload_2_byte_sequence(dm_payload, p_ctx->dm_byte_sequence, &dm_byte_sequence_length);
		if (rc != 0) {
			printk(KERN_ERR"%s-3,%s\n",__func__, rpu_error_code_2_str(rc));
		}
		if(g_dump_flag & DV_DUMP_DM){
			int num,i;
			unsigned char *ptr = (unsigned char *)p_ctx->dm_byte_sequence;
			num = dm_byte_sequence_length;
			DBG_PRINT_L(DV_INFO,"\n===========  DM dump ===========\n");
			for(i=0;i<num;){
				DBG_PRINT_L(DV_INFO,"%02x ",*ptr);
				i++;
				ptr++;
				if(i%16 == 0)
				DBG_PRINT_L(DV_INFO,"\n");
			}
			DBG_PRINT_L(DV_INFO,"\n===========  DM dump end ===========\n");
		}
		if(g_dump_flag & DV_DUMP_DM_FILE){
			char data[128];
			sprintf(data,"/tmp/md_%d.bin",g_dvframeCnt);
			if((dump_frame_cnt == g_dvframeCnt) || (dump_frame_cnt == 0))
			    MetaData_WriteBack_file(data,(unsigned char *)p_ctx->dm_byte_sequence,dm_byte_sequence_length,1);
			else
			    MetaData_WriteBack_file(data,(unsigned char *)p_ctx->dm_byte_sequence,dm_byte_sequence_length,0);
		}
//RTK MARK
#ifndef RTK_EDBEC_CONTROL
        fwrite(p_ctx->dm_byte_sequence, 1, dm_byte_sequence_length, p_ctx->dm_out);
#else
		//rpu_ext_config_fixpt_main_t *composer = (rpu_ext_config_fixpt_main_t *)p_ctx->comp_out;

		if (g_mdparser_output_type == 1) {
			memcpy(p_ctx->dm_out, p_ctx->dm_byte_sequence, dm_byte_sequence_length);	// modifiedy by smfan; mask by smfan

			p_ctx->dm_out += dm_byte_sequence_length;
			g_recordDMAddr = (unsigned int)p_ctx->dm_out;

		} else if (g_mdparser_output_type == 0) {
			// calcuate 1D LUT & prepare DM parameters
		    DBG_PRINT_L(DV_INFO,"MD_OUT: enter \n");
			DV_MD_Output_Gen(p_ctx->dm_byte_sequence, p_ctx->dm_out, p_ctx->comp_out);
			outPtr->TimeStampH = rtd_inl(TIMER_SCPU_CLK90K_HI_reg);
			outPtr->TimeStampL = rtd_inl(TIMER_SCPU_CLK90K_LO_reg);
			outPtr->PicMode = current_dv_picmode;
			outPtr->BackLight = current_dv_backlight_value;
			outPtr->BacklightSliderScalar = OTT_backlightSliderScalar;
			g_MdFlow->bsWrite = (g_MdFlow->bsWrite + sizeof(DOLBYVISION_MD_OUTPUT) >= g_MdFlow->bsLimit) ?
									g_MdFlow->bsBase : (g_MdFlow->bsWrite + sizeof(DOLBYVISION_MD_OUTPUT));
			g_MdFlow->pBsWrite = (unsigned int *)g_MdFlow->bsWrite;
			DBG_PRINT_L(DV_INFO,"MD_OUT: ptsh=0x%x, ptsl=0x%x \n", Swap4Bytes(outPtr->PTSH), Swap4Bytes(outPtr->PTSL));
		}

		//DBG_PRINT_L(DV_INFO,"Composer: ptsh=0x%x, ptsl=0x%x, composer->poly_coef[0][0][0]=0x%x \n", Swap4Bytes(outPtr[1]), Swap4Bytes(outPtr[2]), composer->poly_coef[0][0][0]);
		//for (idx = 0; idx < MAX_PIVOT; idx++)
		//	pr_debug(KERN_EMERG"Composer: composer->pivot_value[0][%d]=0x%x \n", idx, composer->pivot_value[0][idx]);
		DBG_PRINT_L(DV_INFO,"%s, g_dvframeCnt=%d \n", __FUNCTION__, g_dvframeCnt);
		g_dvframeCnt++;
#endif
        memset(p_ctx->rpu_data_outbuf.vdr_rpu_data_payload,
               0,
               rpu_payload_sz);
        memset(p_ctx->rpu_data_outbuf.rpu_data_header,
               0,
               sizeof(rpu_data_header_t));
        memset(p_ctx->rpu_data_outbuf.vdr_dm_data_payload,
               0,
               sizeof(vdr_dm_data_payload_t));
//RTK MARK
#ifdef RTK_EDBEC_CONTROL
		g_parser_ok = 1;	// added by smfan
 		if (g_mdparser_output_type == 0)
		{
			cur_time = rtd_inl(0xB801B690)/90;

			outPtr->own_bit = Swap4Bytes(1);	/* own bit set 1 */ //Move to the final position

            pr_debug(KERN_EMERG"addr:%x, ptsh:%d cur_time:%d buf_num:%d\n", (unsigned int)outPtr, g_pts_info.PTSH, cur_time, cur_buffer_num);
			
		}
#endif
    }
    else if (md_parser_evt == MD_PARSER_ERROR_EVT)
    {
        dv_md_parser_rc_t rc = dv_md_parser_get_last_err(p_ctx->h_md_parser);
	printk(KERN_ERR"%s,%s\n",__func__, dv_md_parser_get_errstr(rc));
#ifdef RTK_EDBEC_CONTROL
	g_parser_ok = 1;	// added by smfan
#endif
    }
}



/*-----------------------------------------------------------------------------+
 | main() entry point
 +----------------------------------------------------------------------------*/
//RTK MARK
/*
* proto-type modified by smfan
* @miInPtr the pointer of rpu MD
* @fileMdInSize size of rpu MD
* @endofstream end of stream
* @return captured byte size of metadata parser
*/
int metadata_parser_main(unsigned char *mdInPtr, unsigned int fileMdInSize, MD_PARSER_STATE mdparser_state) {
	//unsigned char* bs_buffer = NULL;
	static unsigned char bs_buffer[BITSTREAM_BUFFER_SIZE];
	size_t             size = 0;
	int                eos = 0;
	int                payload_buffer_size;
	dv_md_parser_rc_t  rc;
	unsigned int fileRemainSize, doneSize = 0;
	//bs_buffer = (unsigned char*)kmalloc(BITSTREAM_BUFFER_SIZE*sizeof(unsigned char), GFP_KERNEL);
	//if(bs_buffer == NULL) {
	//	printk(KERN_ERR"[%s] bs_buffer kmalloc fail!\n",__func__);
	//	return doneSize;
	//}


	DBG_PRINT_L(DV_INFO,"%s, mdparser_state=%d \n", __FUNCTION__, mdparser_state);
	ctx.md_in = mdInPtr;

	if (mdparser_state == MD_PARSER_INIT) {
		memset(&ctx, 0, sizeof(ctx));

		ctx.rpu_data_outbuf.rpu_data_header = &g_rpu_data_header;
		memset(ctx.rpu_data_outbuf.rpu_data_header, 0, sizeof(rpu_data_header_t));

		/* allocate rpu payload buffer */
		payload_buffer_size = (MAX_NUM_X_PARTITIONS_L1 * MAX_NUM_X_PARTITIONS_L1)*sizeof(vdr_rpu_data_payload_t);

		ctx.rpu_data_outbuf.vdr_rpu_data_payload = &g_vdr_rpu_data_payload;
		memset(ctx.rpu_data_outbuf.vdr_rpu_data_payload, 0, payload_buffer_size);

		ctx.rpu_data_outbuf.vdr_dm_data_payload = &g_vdr_dm_data_payload;
		memset(ctx.rpu_data_outbuf.vdr_dm_data_payload, 0, sizeof(vdr_dm_data_payload_t));


		/* dm byte sequence */
		ctx.dm_byte_sequence = (char *)&g_dm_metadata;
		memset(ctx.dm_byte_sequence, 0, sizeof(dm_metadata_t));

		ctx.md_in = mdInPtr;
		/*
		if (g_dmMDAddr == 0)
		    g_dmMDAddr = (unsigned int)dvr_malloc_specific(0x100000, GFP_DCU2);
		if (g_compMDAddr == 0) {
		    g_compMDAddr = (unsigned int)dvr_malloc_specific(0x100000, GFP_DCU2);
		    printk(KERN_EMERG"dmMDAddr=0x%x\n", dvr_to_phys((void *)g_dmMDAddr));
		    printk(KERN_EMERG"compMDAddr=0x%x\n", dvr_to_phys((void *)g_compMDAddr));
		}
		ctx.comp_out = g_compMDAddr;
		ctx.dm_out = g_dmMDAddr;
		*/

		memset(&ctx.conf.flags, 0, sizeof(ctx.conf.flags));
		ctx.conf.flags.profile = RPU_MAIN_PROFILE;
		ctx.conf.flags.level = RPU_LEVEL_1;
		size = dv_md_parser_mem_query(&ctx.conf);
		ctx.h_md_parser = (dv_md_parser_handle_t)rtk_dolby_kmalloc_and_save(size);
		/* create the Dolby Vision metadata parser instance */
		if (ctx.h_md_parser) {
			/* configure and initialize the metadata parser instance */

			ctx.conf.pf_md_parser_cb = md_parser_evt_handler_cb;
			ctx.conf.p_md_parser_cb_ctx = (void *)&ctx;
			ctx.conf.pf_msg_log = NULL;
			if ((rc = dv_md_parser_init(ctx.h_md_parser, &ctx.conf )) != DV_MD_PARSER_OK) {
				//DBG_PRINT_L(DV_INFO,"dv_md_parser_init(rc='%s') failed)\n",
				//dv_md_parser_get_errstr(rc));
				printk(KERN_ERR"[%s]dv_md_parser_init(rc='%s') failed)\n",
				__func__,dv_md_parser_get_errstr(rc));
				goto metadata_parser_exit;
			}
		}else{
			printk(KERN_ERR"Error creating Dolby Vision metadata parser instance\n");
			goto metadata_parser_exit;
		}
	} else {
		// adde by smfan
		/* reset the buffer of RPU decoder for frame by frame parsing*/
		dv_md_parser_buffer_reset(ctx.h_md_parser);
	}


	fileRemainSize = fileMdInSize;
	g_rpu_size = g_more_eos_err = g_parser_ok = 0;

	while (!eos) {
		if (fileRemainSize >= BITSTREAM_BUFFER_SIZE) {
			size = BITSTREAM_BUFFER_SIZE;
			memcpy(bs_buffer, ctx.md_in, BITSTREAM_BUFFER_SIZE);
			ctx.md_in += BITSTREAM_BUFFER_SIZE;
			fileRemainSize -= BITSTREAM_BUFFER_SIZE;
		} else {	/* last data for rpu metadata */
			size = fileRemainSize;
			memcpy(bs_buffer, ctx.md_in, size);
			ctx.md_in += size;
			fileRemainSize -= size;
		}

		if (fileRemainSize == 0 && mdparser_state == MD_PARSER_EOS)
			eos = 1;
		/* push the metadata buffer to the metadata parser */
		if ((rc = dv_md_parser_process(
			ctx.h_md_parser,0,NULL,bs_buffer,
			(uint32_t) size, eos)) != DV_MD_PARSER_OK) {

			DBG_PRINT_L(DV_INFO,"dv_md_parser_process(rc='%s') failed\n", dv_md_parser_get_errstr(rc));
			/* restore the final MD size if the integrity of lastest MD data is NOT compelte */
			doneSize = g_rpu_size;	// added by smfan
			break;
		}
		if (eos) {
			doneSize = g_rpu_size;	// added by smfan
			break;
		}

		if (g_parser_ok) {
			doneSize = g_rpu_size;
			break;
		}
		if (g_more_eos_err) {
			DBG_PRINT_L(DV_INFO,"%s, g_more_eos_err\n", __FUNCTION__);
			doneSize = g_rpu_size;
			break;
		}
    }


metadata_parser_exit:

#if 0			// mask by smfan
   if (ctx.h_md_parser)
    {
        dv_md_parser_destroy(ctx.h_md_parser);
        ctx.h_md_parser = NULL;
    }

    if (ctx.md_in)
        fclose(ctx.md_in);
    if (ctx.comp_out)
        fclose(ctx.comp_out);
    if (ctx.dm_out)
        fclose(ctx.dm_out);

    if (ctx.dm_byte_sequence)
        free(ctx.dm_byte_sequence);
    if (ctx.rpu_data_outbuf.rpu_data_header)
        free(ctx.rpu_data_outbuf.rpu_data_header);
    if (ctx.rpu_data_outbuf.vdr_rpu_data_payload)
        free(ctx.rpu_data_outbuf.vdr_rpu_data_payload);
    if (ctx.rpu_data_outbuf.vdr_dm_data_payload)
        free(ctx.rpu_data_outbuf.vdr_dm_data_payload);
    free(ctx.h_md_parser);

    exit(exit_code);
#endif

	//kfree(bs_buffer);
	return doneSize;
}
