/**
 * This product contains one or more programs protected under international
 * and U.S. copyright laws as unpublished works.  They are confidential and
 * proprietary to Dolby Laboratories.  Their reproduction or disclosure, in
 * whole or in part, or the production of derivative works therefrom without
 * the express permission of Dolby Laboratories is prohibited.
 * Copyright 2011 - 2015 by Dolby Laboratories.
 * All rights reserved.
 *
 * @brief DoVi Control Path
 * @file control_path_api.c
 *
 * $Id$
 */
//RTK MARK
#ifndef RTK_EDBEC_CONTROL
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#else
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

#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/random.h>
#include <linux/pageremap.h>
#include <linux/kthread.h>
#include <linux/time.h>

#include "nwfpe/fpa11.h"  // for float    // added by smfan
#include "nwfpe/fpopcode.h"       // for float    // added by smfan
#include "typedefs.h"
#endif

#include <rpu_ext_config.h>
#include <KdmTypeFxp.h>
#include <VdrDmApi.h>
#include <CdmTypePriFxp.h>
#include <control_path_api.h>
#include <control_path_std.h>
#include <control_path_priv.h>
#include <dm2_x/VdrDmAPIpFxp.h>


void MetaData_WriteBack_file(char *name,unsigned char *buffer,int size,int end);

static uint32_t (*printLog)(uint32_t, const char *);
#if !(IPCORE)
static uint64_t (*printLog64)(uint64_t, const char *);
#endif
static void (*custom_sprintf)(char*, const char*, ...);

extern uint16_t             user_ms_weight;

#ifdef RTK_EDBEC_1_5

/*baker, 5051 need Dolby_VSIF_LL_DoVi_GD.bin */
UINT8 Dolby_VSIF_LL_DoVi_GD[0x100] = {
	0x81, 0x01, 0x1b, 0x4a, 0x46, 0xd0, 0x00, 0x03,
	0x89, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/*baker, 5052,54,59 need Dolby_VSIF_LL_DoVi.bin */
UINT8 Dolby_VSIF_LL_DoVi[0x100] = {
	0x81, 0x01, 0x1b, 0x4a, 0x46, 0xd0, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const int16_t msFilterRow[11] = {
	36,111,267,498,725,822,725,498,267,111,36
};
static const int16_t msFilterCol[5] = {
	223,1000,1650,1000,223
};
static const int16_t msFilterEdgeRow[11] = {
	180,445,800,997,725,0,-725,-997,-800,-445,-180
};
static const int16_t msFilterEdgeCol[5] = {
	446,1000,0,-1000,-446
};
#endif

#if 1
/*modified by Fox*/
/*-----------------------------------------------------------------------------+
 |  Extract DM parameters from DM metadata byte-stream
 +----------------------------------------------------------------------------*/

static void dm_metadata_2_dm_param
    (
    dm_metadata_base_t  *p_dm_md,
    MdsExt_t            *p_mds_ext,
    const DmCfgFxp_t   *p_dm_cfg
	)
{
    uint32_t   ext_blk_len;
    uint8_t    ext_blk_lvl;
    uint8_t    i_blk;
    uint8_t   *p_ext_blk;
    TrimSet_t *p_trim_set;
    Trim_t    *p_trim;
	int i = 0;
    p_mds_ext->affected_dm_metadata_id = p_dm_md->dm_metadata_id;
    p_mds_ext->scene_refresh_flag = p_dm_md->scene_refresh_flag;

    p_mds_ext->m33Yuv2RgbScale2P = 13;
    p_mds_ext->m33Yuv2Rgb[0][0] = (p_dm_md->YCCtoRGB_coef0_hi << 8) |
                                   p_dm_md->YCCtoRGB_coef0_lo;
    p_mds_ext->m33Yuv2Rgb[0][1] = (p_dm_md->YCCtoRGB_coef1_hi << 8) |
                                   p_dm_md->YCCtoRGB_coef1_lo;
    p_mds_ext->m33Yuv2Rgb[0][2] = (p_dm_md->YCCtoRGB_coef2_hi << 8) |
                                   p_dm_md->YCCtoRGB_coef2_lo;
    p_mds_ext->m33Yuv2Rgb[1][0] = (p_dm_md->YCCtoRGB_coef3_hi << 8) |
                                   p_dm_md->YCCtoRGB_coef3_lo;
    p_mds_ext->m33Yuv2Rgb[1][1] = (p_dm_md->YCCtoRGB_coef4_hi << 8) |
                                   p_dm_md->YCCtoRGB_coef4_lo;
    p_mds_ext->m33Yuv2Rgb[1][2] = (p_dm_md->YCCtoRGB_coef5_hi << 8) |
                                   p_dm_md->YCCtoRGB_coef5_lo;
    p_mds_ext->m33Yuv2Rgb[2][0] = (p_dm_md->YCCtoRGB_coef6_hi << 8) |
                                   p_dm_md->YCCtoRGB_coef6_lo;
    p_mds_ext->m33Yuv2Rgb[2][1] = (p_dm_md->YCCtoRGB_coef7_hi << 8) |
                                   p_dm_md->YCCtoRGB_coef7_lo;
    p_mds_ext->m33Yuv2Rgb[2][2] = (p_dm_md->YCCtoRGB_coef8_hi << 8) |
                                   p_dm_md->YCCtoRGB_coef8_lo;

    p_mds_ext->v3Yuv2Rgb[0] = (p_dm_md->YCCtoRGB_offset0_byte3 << 24) |
                              (p_dm_md->YCCtoRGB_offset0_byte2 << 16) |
                              (p_dm_md->YCCtoRGB_offset0_byte1 <<  8) |
                               p_dm_md->YCCtoRGB_offset0_byte0;
    p_mds_ext->v3Yuv2Rgb[1] = (p_dm_md->YCCtoRGB_offset1_byte3 << 24) |
                              (p_dm_md->YCCtoRGB_offset1_byte2 << 16) |
                              (p_dm_md->YCCtoRGB_offset1_byte1 <<  8) |
                               p_dm_md->YCCtoRGB_offset1_byte0;
    p_mds_ext->v3Yuv2Rgb[2] = (p_dm_md->YCCtoRGB_offset2_byte3 << 24) |
                              (p_dm_md->YCCtoRGB_offset2_byte2 << 16) |
                              (p_dm_md->YCCtoRGB_offset2_byte1 <<  8) |
                               p_dm_md->YCCtoRGB_offset2_byte0;

    p_mds_ext->m33Rgb2WpLmsScale2P = 14;
    p_mds_ext->m33Rgb2WpLms[0][0] = (p_dm_md->RGBtoLMS_coef0_hi << 8) |
                                     p_dm_md->RGBtoLMS_coef0_lo;
    p_mds_ext->m33Rgb2WpLms[0][1] = (p_dm_md->RGBtoLMS_coef1_hi << 8) |
                                     p_dm_md->RGBtoLMS_coef1_lo;
    p_mds_ext->m33Rgb2WpLms[0][2] = (p_dm_md->RGBtoLMS_coef2_hi << 8) |
                                     p_dm_md->RGBtoLMS_coef2_lo;
    p_mds_ext->m33Rgb2WpLms[1][0] = (p_dm_md->RGBtoLMS_coef3_hi << 8) |
                                     p_dm_md->RGBtoLMS_coef3_lo;
    p_mds_ext->m33Rgb2WpLms[1][1] = (p_dm_md->RGBtoLMS_coef4_hi << 8) |
                                     p_dm_md->RGBtoLMS_coef4_lo;
    p_mds_ext->m33Rgb2WpLms[1][2] = (p_dm_md->RGBtoLMS_coef5_hi << 8) |
                                     p_dm_md->RGBtoLMS_coef5_lo;
    p_mds_ext->m33Rgb2WpLms[2][0] = (p_dm_md->RGBtoLMS_coef6_hi << 8) |
                                     p_dm_md->RGBtoLMS_coef6_lo;
    p_mds_ext->m33Rgb2WpLms[2][1] = (p_dm_md->RGBtoLMS_coef7_hi << 8) |
                                     p_dm_md->RGBtoLMS_coef7_lo;
    p_mds_ext->m33Rgb2WpLms[2][2] = (p_dm_md->RGBtoLMS_coef8_hi << 8) |
                                     p_dm_md->RGBtoLMS_coef8_lo;

    p_mds_ext->signal_eotf        = (p_dm_md->signal_eotf_hi << 8) |
                                     p_dm_md->signal_eotf_lo;
    p_mds_ext->signal_eotf_param0 = (p_dm_md->signal_eotf_param0_hi << 8) |
                                     p_dm_md->signal_eotf_param0_lo;
    p_mds_ext->signal_eotf_param1 = (p_dm_md->signal_eotf_param1_hi << 8) |
                                     p_dm_md->signal_eotf_param1_lo;
    p_mds_ext->signal_eotf_param2 = (p_dm_md->signal_eotf_param2_byte3 << 24) |
                                    (p_dm_md->signal_eotf_param2_byte2 << 16) |
                                    (p_dm_md->signal_eotf_param2_byte1 <<  8) |
                                     p_dm_md->signal_eotf_param2_byte0;

    p_mds_ext->signal_bit_depth       = p_dm_md->signal_bit_depth;
    p_mds_ext->signal_color_space     = p_dm_md->signal_color_space;
    p_mds_ext->signal_chroma_format   = p_dm_md->signal_chroma_format;
    p_mds_ext->signal_full_range_flag = p_dm_md->signal_full_range_flag;

    p_mds_ext->source_min_PQ   = (p_dm_md->source_min_PQ_hi << 8) |
                                  p_dm_md->source_min_PQ_lo;
    p_mds_ext->source_max_PQ   = (p_dm_md->source_max_PQ_hi << 8) |
                                  p_dm_md->source_max_PQ_lo;
    p_mds_ext->source_diagonal = (p_dm_md->source_diagonal_hi << 8) |
                                  p_dm_md->source_diagonal_lo;

    /* initialize level 1 default */
    p_mds_ext->min_PQ = p_mds_ext->source_min_PQ;
    p_mds_ext->max_PQ = p_mds_ext->source_max_PQ;
    p_mds_ext->mid_PQ = (p_mds_ext->source_min_PQ + p_mds_ext->max_PQ) >> 1;

    // level 2 and 3 handling: default no trim
    p_trim_set = &(p_mds_ext->trimSets);
    p_trim_set->TrimSetNum = 0;
    p_trim_set->TrimSets[0].TrimNum = 0;
    p_trim_set->TrimSets[0].Trima[0] = p_mds_ext->source_max_PQ;

#if EN_GLOBAL_DIMMING
    p_mds_ext->lvl4GdAvail = 0;
#endif
    p_mds_ext->lvl5AoiAvail = 0;
#if EN_AOI
    p_mds_ext->active_area_left_offset   = (unsigned short)(p_dm_cfg->dmCtrl.AoiCol0);
    p_mds_ext->active_area_right_offset  = (unsigned short)(p_dm_cfg->srcSigEnv.ColNum - p_dm_cfg->dmCtrl.AoiCol1Plus1);
    p_mds_ext->active_area_top_offset    = (unsigned short)(p_dm_cfg->dmCtrl.AoiRow0);
    p_mds_ext->active_area_bottom_offset = (unsigned short)(p_dm_cfg->srcSigEnv.RowNum - p_dm_cfg->dmCtrl.AoiRow1Plus1);
#endif

    /* parse the extenstion block */
    p_mds_ext->num_ext_blocks = p_dm_md->num_ext_blocks;

    p_ext_blk = &p_dm_md->num_ext_blocks + sizeof(p_dm_md->num_ext_blocks);

    for (i_blk = 0; i_blk < p_mds_ext->num_ext_blocks; ++i_blk)
    {
        ext_blk_len = (p_ext_blk[0] << 24) |
                      (p_ext_blk[1] << 16) |
                      (p_ext_blk[2] <<  8) |
                       p_ext_blk[3];
        p_ext_blk += sizeof(uint32_t);

        ext_blk_lvl = p_ext_blk[0];
        p_ext_blk += sizeof(uint8_t);

        if (ext_blk_lvl == 1)
        {
            p_mds_ext->min_PQ = (p_ext_blk[0] << 8) | p_ext_blk[1];
            p_ext_blk += sizeof(uint16_t);
            p_mds_ext->max_PQ = (p_ext_blk[0] << 8) | p_ext_blk[1];
            p_ext_blk += sizeof(uint16_t);
            p_mds_ext->mid_PQ = (p_ext_blk[0] << 8) | p_ext_blk[1];
            p_ext_blk += sizeof(uint16_t);

            ext_blk_len -= 3 * sizeof(uint16_t);
        }
        else if ((ext_blk_lvl == 2) && !p_dm_cfg->tmCtrl.l2off)
        {
            p_trim = &p_trim_set->TrimSets[ext_blk_lvl - 2]; /* lvl = 2 in [0] */
            if (p_trim->TrimNum < p_trim->TrimNumMax)
            {
                /* only get the first TrimNumMax */
                ++(p_trim->TrimNum);
                for (i=0; i < p_trim->TrimTypeDim; ++i)
                {
                    p_trim->Trima[(p_trim->TrimNum * p_trim->TrimTypeDim) + i] = (p_ext_blk[0] << 8) | p_ext_blk[1];
                    p_ext_blk += sizeof(uint16_t);
                }

                /* update multi-scale blender weight if not specified */
                if (p_trim->Trima[(p_trim->TrimNum * p_trim->TrimTypeDim) + TrimTypeMsWeight] == 0xFFFF)
                    p_trim->Trima[(p_trim->TrimNum * p_trim->TrimTypeDim) + TrimTypeMsWeight] = user_ms_weight;

                ext_blk_len -= p_trim->TrimTypeDim * sizeof(uint16_t);
            }
            ++p_trim_set->TrimSetNum;
        }
        else if (ext_blk_lvl == 4)
        {
            #if EN_GLOBAL_DIMMING
            p_mds_ext->lvl4GdAvail = 1;
            p_mds_ext->filtered_mean_PQ = (p_ext_blk[0] << 8) | p_ext_blk[1];
            p_ext_blk += sizeof(uint16_t);
            p_mds_ext->filtered_power_PQ = (p_ext_blk[0] << 8) | p_ext_blk[1];;
            p_ext_blk += sizeof(uint16_t);
            ext_blk_len -= 2 * sizeof(uint16_t);
            #endif
        }
        else if (ext_blk_lvl == 5)
        {
            p_mds_ext->lvl5AoiAvail = 1;
#if EN_AOI
            p_mds_ext->active_area_left_offset = (p_ext_blk[0] << 8) | p_ext_blk[1];
            p_ext_blk += sizeof(uint16_t);
            p_mds_ext->active_area_right_offset = (p_ext_blk[0] << 8) | p_ext_blk[1];
            p_ext_blk += sizeof(uint16_t);
            p_mds_ext->active_area_top_offset = (p_ext_blk[0] << 8) | p_ext_blk[1];
            p_ext_blk += sizeof(uint16_t);
            p_mds_ext->active_area_bottom_offset = (p_ext_blk[0] << 8) | p_ext_blk[1];
            p_ext_blk += sizeof(uint16_t);
#endif
            ext_blk_len -= 4 * sizeof(uint16_t);
        }

        /* skip remaining bytes or if level is unknown */
        p_ext_blk += ext_blk_len;
    }
}
#else

static uint16_t dm_metadata_2_dm_param
(
    dm_metadata_t   *p_dm_md,
    MdsExt_t        *p_mds_ext,
    const DmCfgFxp_t   *p_dm_cfg,
    cp_context_t* p_ctx
)
{
    dm_metadata_base_t* p_base_md = &p_dm_md->base;
    dm_metadata_ext_t* p_ext_md = p_dm_md->ext;
    TargetDisplayConfig_t *p_target_cfg=&p_ctx->cur_pq_config->target_display_config;
    TrimSet_t  *p_trim_set;
    Trim_t     *p_trim;
    int        i;

    p_mds_ext->affected_dm_metadata_id = p_base_md->dm_metadata_id;
    p_mds_ext->scene_refresh_flag = p_base_md->scene_refresh_flag;

    p_mds_ext->m33Yuv2RgbScale2P = 13;
    p_mds_ext->m33Yuv2Rgb[0][0] = (p_base_md->YCCtoRGB_coef0_hi << 8) | p_base_md->YCCtoRGB_coef0_lo;
    p_mds_ext->m33Yuv2Rgb[0][1] = (p_base_md->YCCtoRGB_coef1_hi << 8) | p_base_md->YCCtoRGB_coef1_lo;
    p_mds_ext->m33Yuv2Rgb[0][2] = (p_base_md->YCCtoRGB_coef2_hi << 8) | p_base_md->YCCtoRGB_coef2_lo;
    p_mds_ext->m33Yuv2Rgb[1][0] = (p_base_md->YCCtoRGB_coef3_hi << 8) | p_base_md->YCCtoRGB_coef3_lo;
    p_mds_ext->m33Yuv2Rgb[1][1] = (p_base_md->YCCtoRGB_coef4_hi << 8) | p_base_md->YCCtoRGB_coef4_lo;
    p_mds_ext->m33Yuv2Rgb[1][2] = (p_base_md->YCCtoRGB_coef5_hi << 8) | p_base_md->YCCtoRGB_coef5_lo;
    p_mds_ext->m33Yuv2Rgb[2][0] = (p_base_md->YCCtoRGB_coef6_hi << 8) | p_base_md->YCCtoRGB_coef6_lo;
    p_mds_ext->m33Yuv2Rgb[2][1] = (p_base_md->YCCtoRGB_coef7_hi << 8) | p_base_md->YCCtoRGB_coef7_lo;
    p_mds_ext->m33Yuv2Rgb[2][2] = (p_base_md->YCCtoRGB_coef8_hi << 8) | p_base_md->YCCtoRGB_coef8_lo;

    p_mds_ext->v3Yuv2Rgb[0] = (p_base_md->YCCtoRGB_offset0_byte3 << 24) |
                              (p_base_md->YCCtoRGB_offset0_byte2 << 16) |
                              (p_base_md->YCCtoRGB_offset0_byte1 <<  8) |
                              p_base_md->YCCtoRGB_offset0_byte0;
    p_mds_ext->v3Yuv2Rgb[1] = (p_base_md->YCCtoRGB_offset1_byte3 << 24) |
                              (p_base_md->YCCtoRGB_offset1_byte2 << 16) |
                              (p_base_md->YCCtoRGB_offset1_byte1 <<  8) |
                              p_base_md->YCCtoRGB_offset1_byte0;
    p_mds_ext->v3Yuv2Rgb[2] = (p_base_md->YCCtoRGB_offset2_byte3 << 24) |
                              (p_base_md->YCCtoRGB_offset2_byte2 << 16) |
                              (p_base_md->YCCtoRGB_offset2_byte1 <<  8) |
                              p_base_md->YCCtoRGB_offset2_byte0;

    p_mds_ext->m33Rgb2WpLmsScale2P = 14;
    p_mds_ext->m33Rgb2WpLms[0][0] = (p_base_md->RGBtoLMS_coef0_hi << 8) | p_base_md->RGBtoLMS_coef0_lo;
    p_mds_ext->m33Rgb2WpLms[0][1] = (p_base_md->RGBtoLMS_coef1_hi << 8) | p_base_md->RGBtoLMS_coef1_lo;
    p_mds_ext->m33Rgb2WpLms[0][2] = (p_base_md->RGBtoLMS_coef2_hi << 8) | p_base_md->RGBtoLMS_coef2_lo;
    p_mds_ext->m33Rgb2WpLms[1][0] = (p_base_md->RGBtoLMS_coef3_hi << 8) | p_base_md->RGBtoLMS_coef3_lo;
    p_mds_ext->m33Rgb2WpLms[1][1] = (p_base_md->RGBtoLMS_coef4_hi << 8) | p_base_md->RGBtoLMS_coef4_lo;
    p_mds_ext->m33Rgb2WpLms[1][2] = (p_base_md->RGBtoLMS_coef5_hi << 8) | p_base_md->RGBtoLMS_coef5_lo;
    p_mds_ext->m33Rgb2WpLms[2][0] = (p_base_md->RGBtoLMS_coef6_hi << 8) | p_base_md->RGBtoLMS_coef6_lo;
    p_mds_ext->m33Rgb2WpLms[2][1] = (p_base_md->RGBtoLMS_coef7_hi << 8) | p_base_md->RGBtoLMS_coef7_lo;
    p_mds_ext->m33Rgb2WpLms[2][2] = (p_base_md->RGBtoLMS_coef8_hi << 8) | p_base_md->RGBtoLMS_coef8_lo;

    /* EOTF GAMMA, A, B */
    p_mds_ext->signal_eotf = (p_base_md->signal_eotf_hi << 8) | p_base_md->signal_eotf_lo;
    p_mds_ext->signal_eotf_param0 = (p_base_md->signal_eotf_param0_hi << 8) |
									p_base_md->signal_eotf_param0_lo;
    p_mds_ext->signal_eotf_param1 = (p_base_md->signal_eotf_param1_hi << 8) |
									p_base_md->signal_eotf_param1_lo;
    p_mds_ext->signal_eotf_param2 = (p_base_md->signal_eotf_param2_byte3 << 24) |
                                    (p_base_md->signal_eotf_param2_byte2 << 16) |
                                    (p_base_md->signal_eotf_param2_byte1 <<  8) |
									p_base_md->signal_eotf_param2_byte0;

    /* signal info */
    p_mds_ext->signal_bit_depth       = p_base_md->signal_bit_depth;
    p_mds_ext->signal_color_space     = p_base_md->signal_color_space;
    p_mds_ext->signal_chroma_format   = p_base_md->signal_chroma_format;
    p_mds_ext->signal_full_range_flag = p_base_md->signal_full_range_flag;

    /* source monitor: all PQ scale 4095 */
    p_mds_ext->source_min_PQ   = (p_base_md->source_min_PQ_hi << 8) |
								 p_base_md->source_min_PQ_lo;
    p_mds_ext->source_max_PQ   = (p_base_md->source_max_PQ_hi << 8) |
								 p_base_md->source_max_PQ_lo;
    p_mds_ext->source_diagonal = (p_base_md->source_diagonal_hi << 8) |
								 p_base_md->source_diagonal_lo;

    /* initialize level 1 default value */
    p_mds_ext->min_PQ = p_mds_ext->source_min_PQ;
    p_mds_ext->max_PQ = p_mds_ext->source_max_PQ;
    p_mds_ext->mid_PQ = (p_mds_ext->source_min_PQ + p_mds_ext->max_PQ) >> 1;

    p_mds_ext->num_ext_blocks = p_base_md->num_ext_blocks;

    p_trim_set = &p_mds_ext->trimSets;
    p_trim_set->TrimSetNum = 0;
    p_trim_set->TrimSets[0].TrimNum = 0;
    p_trim_set->TrimSets[0].Trima[0] = p_mds_ext->source_max_PQ;

    #if EN_GLOBAL_DIMMING
    p_mds_ext->lvl4GdAvail  = 0;
    #endif
    p_mds_ext->lvl5AoiAvail = 0;

    if (p_target_cfg->tuningMode & TUNINGMODE_EXTLEVEL255_DISABLE) {
        p_mds_ext->lvl255RunModeAvail = 0;
        p_mds_ext->dm_run_mode = 0;
    } else {
        p_mds_ext->lvl255RunModeAvail = !!(p_target_cfg->tuningMode & TUNINGMODE_FORCE_ABSOLUTE);
        p_mds_ext->dm_run_mode  = !!(p_target_cfg->tuningMode & TUNINGMODE_FORCE_ABSOLUTE);
    }
    p_mds_ext->dm_run_version = 0;
    p_mds_ext->dm_debug0 = 0;
    p_mds_ext->dm_debug1 = 0;
    p_mds_ext->dm_debug2 = 0;
    p_mds_ext->dm_debug3 = 0;

# if EN_AOI
    p_mds_ext->active_area_left_offset   = 0;
    p_mds_ext->active_area_right_offset  = 0;
    p_mds_ext->active_area_top_offset    = 0;
    p_mds_ext->active_area_bottom_offset = 0;
#endif

    for (i = 0; i < p_mds_ext->num_ext_blocks; ++i)
    {
        if ((p_ext_md[i].ext_block_level == 1) && !(p_target_cfg->tuningMode & TUNINGMODE_EXTLEVEL1_DISABLE))
        {
            p_mds_ext->min_PQ = (p_ext_md[i].l.level_1.min_PQ_hi << 8) | p_ext_md[i].l.level_1.min_PQ_lo;
            p_mds_ext->max_PQ = (p_ext_md[i].l.level_1.max_PQ_hi << 8) | p_ext_md[i].l.level_1.max_PQ_lo;
            p_mds_ext->mid_PQ = (p_ext_md[i].l.level_1.avg_PQ_hi << 8) | p_ext_md[i].l.level_1.avg_PQ_lo;
        }
        else if ((p_ext_md[i].ext_block_level == 2 && !p_dm_cfg->tmCtrl.l2off) && !(p_target_cfg->tuningMode & TUNINGMODE_EXTLEVEL2_DISABLE))
        {
            p_trim = &p_trim_set->TrimSets[p_ext_md[i].ext_block_level - 2]; /* lvl = 2 in [0] */
            if (p_trim->TrimNum < p_trim->TrimNumMax)
            {
                /* only get the first TrimNumMax */
                ++(p_trim->TrimNum);
                p_trim->Trima[p_trim->TrimNum * p_trim->TrimTypeDim + TrimTypeTMaxPq2]      = (p_ext_md[i].l.level_2.target_max_PQ_hi << 8)        | p_ext_md[i].l.level_2.target_max_PQ_lo;
                p_trim->Trima[p_trim->TrimNum * p_trim->TrimTypeDim + TrimTypeSlope]        = (p_ext_md[i].l.level_2.trim_slope_hi << 8)           | p_ext_md[i].l.level_2.trim_slope_lo;
                p_trim->Trima[p_trim->TrimNum * p_trim->TrimTypeDim + TrimTypeOffset]       = (p_ext_md[i].l.level_2.trim_offset_hi << 8)          | p_ext_md[i].l.level_2.trim_offset_lo;
                p_trim->Trima[p_trim->TrimNum * p_trim->TrimTypeDim + TrimTypePower]        = (p_ext_md[i].l.level_2.trim_power_hi << 8)           | p_ext_md[i].l.level_2.trim_power_lo;
                p_trim->Trima[p_trim->TrimNum * p_trim->TrimTypeDim + TrimTypeChromaWeight] = (p_ext_md[i].l.level_2.trim_chroma_weight_hi << 8)   | p_ext_md[i].l.level_2.trim_chroma_weight_lo;
                p_trim->Trima[p_trim->TrimNum * p_trim->TrimTypeDim + TrimTypeSatGain]      = (p_ext_md[i].l.level_2.trim_saturation_gain_hi << 8) | p_ext_md[i].l.level_2.trim_saturation_gain_lo;
                p_trim->Trima[p_trim->TrimNum * p_trim->TrimTypeDim + TrimTypeMsWeight]     = (p_ext_md[i].l.level_2.ms_weight_hi << 8)            | p_ext_md[i].l.level_2.ms_weight_lo;
                if ((p_trim->Trima[p_trim->TrimNum * p_trim->TrimTypeDim + TrimTypeMsWeight] & 0x1FFF) == DM_MS_WEIGHT_UNDEFINED_VALUE)
                    p_trim->Trima[p_trim->TrimNum * p_trim->TrimTypeDim + TrimTypeMsWeight] = p_target_cfg->mSWeight;
            }
            ++p_trim_set->TrimSetNum;
        }
        else if ((p_ext_md[i].ext_block_level == 4)  && !(p_target_cfg->tuningMode & TUNINGMODE_EXTLEVEL4_DISABLE))
        {
#if EN_GLOBAL_DIMMING
            p_mds_ext->lvl4GdAvail = 1;
            p_mds_ext->filtered_mean_PQ  = (p_ext_md[i].l.level_4.anchor_PQ_hi    << 8) | p_ext_md[i].l.level_4.anchor_PQ_lo;
            p_mds_ext->filtered_power_PQ = (p_ext_md[i].l.level_4.anchor_power_hi << 8) | p_ext_md[i].l.level_4.anchor_power_lo;
#endif
        }
        else if ((p_ext_md[i].ext_block_level == 5)  && !(p_target_cfg->tuningMode & TUNINGMODE_EXTLEVEL5_DISABLE))
        {
            p_mds_ext->lvl5AoiAvail = 1;
# if EN_AOI
            p_mds_ext->active_area_left_offset   = (p_ext_md[i].l.level_5.active_area_left_offset_hi   << 8) | p_ext_md[i].l.level_5.active_area_left_offset_lo;
            p_mds_ext->active_area_right_offset  = (p_ext_md[i].l.level_5.active_area_right_offset_hi  << 8) | p_ext_md[i].l.level_5.active_area_right_offset_lo;
            p_mds_ext->active_area_top_offset    = (p_ext_md[i].l.level_5.active_area_top_offset_hi    << 8) | p_ext_md[i].l.level_5.active_area_top_offset_lo;
            p_mds_ext->active_area_bottom_offset = (p_ext_md[i].l.level_5.active_area_bottom_offset_hi << 8) | p_ext_md[i].l.level_5.active_area_bottom_offset_lo;
#endif
#if IPCORE
            p_ctx->active_area_left_offset   = (p_ext_md[i].l.level_5.active_area_left_offset_hi   << 8) | p_ext_md[i].l.level_5.active_area_left_offset_lo;
            p_ctx->active_area_right_offset  = (p_ext_md[i].l.level_5.active_area_right_offset_hi  << 8) | p_ext_md[i].l.level_5.active_area_right_offset_lo;
            p_ctx->active_area_top_offset    = (p_ext_md[i].l.level_5.active_area_top_offset_hi    << 8) | p_ext_md[i].l.level_5.active_area_top_offset_lo;
            p_ctx->active_area_bottom_offset = (p_ext_md[i].l.level_5.active_area_bottom_offset_hi << 8) | p_ext_md[i].l.level_5.active_area_bottom_offset_lo;
#endif
        }
        else if ((p_ext_md[i].ext_block_level == 255) && (p_mds_ext->lvl255RunModeAvail == 0) && !(p_target_cfg->tuningMode & TUNINGMODE_EXTLEVEL255_DISABLE)) /* only use md if not already set by config file */
        {
            p_mds_ext->lvl255RunModeAvail = 1;
            p_mds_ext->dm_run_mode    = p_ext_md[i].l.level_255.dm_run_mode   ;
            p_mds_ext->dm_run_version = p_ext_md[i].l.level_255.dm_run_version;
            p_mds_ext->dm_debug0      = p_ext_md[i].l.level_255.dm_debug0     ;
            p_mds_ext->dm_debug1      = p_ext_md[i].l.level_255.dm_debug1     ;
            p_mds_ext->dm_debug2      = p_ext_md[i].l.level_255.dm_debug2     ;
            p_mds_ext->dm_debug3      = p_ext_md[i].l.level_255.dm_debug3     ;
        }
    }

    return 0;
}
#endif

static uint16_t fill_bypass_comp_cfg(rpu_ext_config_fixpt_main_t * p_comp, int src_bit_depth) {
    int cmp;
    p_comp->rpu_VDR_bit_depth = 12;
    p_comp->rpu_BL_bit_depth = src_bit_depth;
    p_comp->rpu_EL_bit_depth = 0;
    p_comp->coefficient_log2_denom = 23;
    p_comp->disable_residual_flag = 1;
    p_comp->el_spatial_resampling_filter_flag = 0;

    for (cmp = 0; cmp <3; cmp++) {
        p_comp->num_pivots[cmp] = 2;
        p_comp->pivot_value[cmp][0] = 0;
        p_comp->pivot_value[cmp][1] = (uint32_t)((1 << p_comp->rpu_BL_bit_depth) - 1);
        p_comp->mapping_idc[cmp] = 0;
        p_comp->poly_order[cmp][0] = 1;
        p_comp->poly_coef_int[cmp][0][0] = 0;
        p_comp->poly_coef[cmp][0][0] = 0;
        p_comp->poly_coef_int[cmp][0][1] = 1;
        if (p_comp->rpu_BL_bit_depth == 8)
            p_comp->poly_coef[cmp][0][1] = 0x7878; /* (4095/(16*255) -1) * 2^23 */
        else
            p_comp->poly_coef[cmp][0][1] = 0x1806; /* (4095/(4*1023) -1) * 2^23 */
    }

    return 0;
}

char cStr[30];
static uint32_t printLog_deb(uint32_t val, const char *paramName)
{
    DLB_PRINTF("%-30s\t= 0x%08x\n",paramName,val);
    return(val);
}

#if !(IPCORE)
static uint64_t printLog64_deb(uint64_t val, const char *paramName)
{
    DLB_PRINTF("%-30s\t= 0x%08x%08x\n", paramName, (uint32_t)(val >> 32), (uint32_t)(val & 0xFFFFFFFF));
    return(val);
}
#endif

static void custom_sprintf_deb(char* string, const char* format, ...) {
  va_list args;
  va_start (args, format);
  vsprintf (string, format, args);
  va_end (args);
}

static uint32_t printLog_rel(uint32_t val, const char *paramName)
{
    (void) paramName;
    return(val);
}

#if !(IPCORE)
static uint64_t printLog64_rel(uint64_t val, const char *paramName)
{
    (void) paramName;
    return(val);
}
#endif

static void custom_sprintf_rel(char* string, const char* format, ...) {
    (void) string;
    (void) format;
}

#if IPCORE
#ifndef RTK_EDBEC_CONTROL
static void dmKs2dmreg(DmKsFxp_t *pKs, register_ipcore_t *p_dm_reg_ipcore, cp_context_t* p_ctx, src_param_t *p_src_param) {
    int left_abs, right_abs, bottom_abs, top_abs;
    int left_rel, right_rel, bottom_rel, top_rel;
    static int frame_nr = 0;

    if ((p_ctx->dbgExecParamsPrintPeriod != 0) && (frame_nr % p_ctx->dbgExecParamsPrintPeriod == 0)) {
        printf("\nProcessing Frame %d\n",frame_nr);
        printLog       =  &printLog_deb      ;
        custom_sprintf =  &custom_sprintf_deb;
    } else {
        printLog       =  &printLog_rel      ;
        custom_sprintf =  &custom_sprintf_rel;
    }

    p_dm_reg_ipcore->SRange           = printLog((uint32_t)((uint16_t)pKs->ksIMap.eotfParam.range | (pKs->ksIMap.eotfParam.rangeMin << 16)),"sRangeMin|sRange");
    p_dm_reg_ipcore->Srange_Inverse   = printLog(pKs->ksIMap.eotfParam.rangeInv,"sRangeInv");
    p_dm_reg_ipcore->Frame_Format_1   = printLog((uint32_t)0,"FrameFormat_1");
    p_dm_reg_ipcore->Frame_Format_2   = printLog((uint32_t)0,"FrameFormat_2");
    p_dm_reg_ipcore->pixDef           = printLog((uint32_t)0,"frameOutPixDef");


    p_dm_reg_ipcore->Y2RGB_Coefficient_1 = printLog((uint32_t)((uint16_t)pKs->ksIMap.m33Yuv2Rgb[0][0] | (pKs->ksIMap.m33Yuv2Rgb[0][1]<<16)) ,"YToRGBC1|YToRGBC0");
    p_dm_reg_ipcore->Y2RGB_Coefficient_2 = printLog((uint32_t)((uint16_t)pKs->ksIMap.m33Yuv2Rgb[0][2] | (pKs->ksIMap.m33Yuv2Rgb[1][0]<<16)) ,"YToRGBC3|YToRGBC2");
    p_dm_reg_ipcore->Y2RGB_Coefficient_3 = printLog((uint32_t)((uint16_t)pKs->ksIMap.m33Yuv2Rgb[1][1] | (pKs->ksIMap.m33Yuv2Rgb[1][2]<<16)) ,"YToRGBC5|YToRGBC4");
    p_dm_reg_ipcore->Y2RGB_Coefficient_4 = printLog((uint32_t)((uint16_t)pKs->ksIMap.m33Yuv2Rgb[2][0] | (pKs->ksIMap.m33Yuv2Rgb[2][1]<<16)) ,"YToRGBC7|YToRGBC6");
    p_dm_reg_ipcore->Y2RGB_Coefficient_5 = printLog((uint32_t)((uint16_t)pKs->ksIMap.m33Yuv2Rgb[2][2] | (pKs->ksIMap.m33Yuv2RgbScale2P<<16)) ,"YToRGBScale|YToRGBC8");

    p_dm_reg_ipcore->Y2RGB_Offset_1    =   printLog((uint32_t)pKs->ksIMap.v3Yuv2RgbOffInRgb[0],"YToRGBOffset_0");
    p_dm_reg_ipcore->Y2RGB_Offset_2    =   printLog((uint32_t)pKs->ksIMap.v3Yuv2RgbOffInRgb[1],"YToRGBOffset_1");
    p_dm_reg_ipcore->Y2RGB_Offset_3    =   printLog((uint32_t)pKs->ksIMap.v3Yuv2RgbOffInRgb[2],"YToRGBOffset_2");

    p_dm_reg_ipcore->EOTF              =   printLog((uint32_t)pKs->ksIMap.eotfParam.eotf,"signaleotf");
    p_dm_reg_ipcore->Sparam_1          =   printLog((uint32_t)0,"s_param1");
    p_dm_reg_ipcore->Sparam_2          =   printLog((uint32_t)0,"s_param2");
    p_dm_reg_ipcore->Sgamma            =   printLog((uint32_t)0,"s_gamma");

    p_dm_reg_ipcore->A2B_Coefficient_1 = printLog((uint32_t)((uint16_t)pKs->ksIMap.m33Rgb2Lms[0][0] | (pKs->ksIMap.m33Rgb2Lms[0][1]<<16)) ,"AtoBCoeff1|AtoBCoeff0");
    p_dm_reg_ipcore->A2B_Coefficient_2 = printLog((uint32_t)((uint16_t)pKs->ksIMap.m33Rgb2Lms[0][2] | (pKs->ksIMap.m33Rgb2Lms[1][0]<<16)) ,"AtoBCoeff3|AtoBCoeff2");
    p_dm_reg_ipcore->A2B_Coefficient_3 = printLog((uint32_t)((uint16_t)pKs->ksIMap.m33Rgb2Lms[1][1] | (pKs->ksIMap.m33Rgb2Lms[1][2]<<16)) ,"AtoBCoeff5|AtoBCoeff4");
    p_dm_reg_ipcore->A2B_Coefficient_4 = printLog((uint32_t)((uint16_t)pKs->ksIMap.m33Rgb2Lms[2][0] | (pKs->ksIMap.m33Rgb2Lms[2][1]<<16)) ,"AtoBCoeff7|AtoBCoeff6");
    p_dm_reg_ipcore->A2B_Coefficient_5 = printLog((uint32_t)((uint16_t)pKs->ksIMap.m33Rgb2Lms[2][2] | (pKs->ksIMap.m33Rgb2LmsScale2P<<16)) ,"AtoBCoeff_scale|AtoBCoeff8");

    p_dm_reg_ipcore->C2D_Coefficient_1   = printLog((uint32_t)((uint16_t)pKs->ksIMap.m33Lms2Ipt[0][0] | (pKs->ksIMap.m33Lms2Ipt[0][1]<<16)) ,"CtoDCoeff1|CtoDCoeff0");
    p_dm_reg_ipcore->C2D_Coefficient_2   = printLog((uint32_t)((uint16_t)pKs->ksIMap.m33Lms2Ipt[0][2] | (pKs->ksIMap.m33Lms2Ipt[1][0]<<16)) ,"CtoDCoeff3|CtoDCoeff2");
    p_dm_reg_ipcore->C2D_Coefficient_3   = printLog((uint32_t)((uint16_t)pKs->ksIMap.m33Lms2Ipt[1][1] | (pKs->ksIMap.m33Lms2Ipt[1][2]<<16)) ,"CtoDCoeff5|CtoDCoeff4");
    p_dm_reg_ipcore->C2D_Coefficient_4   = printLog((uint32_t)((uint16_t)pKs->ksIMap.m33Lms2Ipt[2][0] | (pKs->ksIMap.m33Lms2Ipt[2][1]<<16)) ,"CtoDCoeff7|CtoDCoeff6");
    p_dm_reg_ipcore->C2D_Coefficient_5   = printLog((uint32_t)((uint16_t)pKs->ksIMap.m33Lms2Ipt[2][2] | (pKs->ksIMap.m33Lms2IptScale2P<<16)) ,"CtoDCoeff_scale|CtoDCoeff8");

    p_dm_reg_ipcore->C2D_Offset         = printLog((uint32_t)0,"CtoDOffset");
    p_dm_reg_ipcore->Chroma_Weight      = printLog((uint32_t)(0xFFFF & pKs->ksTMap.chromaWeight),"chroma_weight");
    p_dm_reg_ipcore->mFilter_Scale      = printLog((uint32_t)12,"mFilterScale");
    p_dm_reg_ipcore->msWeight           = printLog((uint32_t)((uint16_t)pKs->ksMs.msEdgeWeight | (pKs->ksMs.msWeight<<16)),"msWeight|msWeightEdge");

    p_dm_reg_ipcore->Hunt_Value         = printLog((uint32_t)((0xFFF & pKs->ksOMap.gain) | ((0xFFF & pKs->ksOMap.offset) << 12)),"huntOffset|huntGain");
    p_dm_reg_ipcore->Saturation_Gain    = printLog((uint32_t)pKs->ksOMap.satGain ,"saturationGain");
    p_dm_reg_ipcore->Min_C_1            = printLog((uint32_t)((pKs->ksOMap.ksGmLut.iMinC2<<16)|(uint16_t)pKs->ksOMap.ksGmLut.iMinC1),"minC2|minC1");
    p_dm_reg_ipcore->Min_C_2            = printLog((uint32_t)pKs->ksOMap.ksGmLut.iMinC3,"minC3");
    p_dm_reg_ipcore->Max_C              = printLog((uint32_t)((pKs->ksOMap.ksGmLut.iMaxC2<<16)|(uint16_t)pKs->ksOMap.ksGmLut.iMaxC1),"maxC2|maxC1");


    p_dm_reg_ipcore->C1_Inverse = printLog(pKs->ksOMap.ksGmLut.iDistC1Inv,"C1Inv");
    p_dm_reg_ipcore->C2_Inverse = printLog(pKs->ksOMap.ksGmLut.iDistC2Inv,"C2Inv");
    p_dm_reg_ipcore->C3_Inverse = printLog(pKs->ksOMap.ksGmLut.iDistC3Inv,"C3Inv");

    /* 13 bit relative offset AOI coordinates from metadata */
    left_abs   = p_ctx->active_area_left_offset;
    right_abs  = p_ctx->active_area_right_offset;
    top_abs    = p_ctx->active_area_top_offset;
    bottom_abs = p_ctx->active_area_bottom_offset;

#if 0
    /*
        AOI relative to absolute convertion for ip_gold_2_09212015
        Top left coordinates.
        X = floor(0 + active_area_left_offset/4096 * XSize/4 + 0.5)
        Y = floor(0 + active_area_top_offset/4096 * YSize/4 + 0.5)
        Bottom right coordinates.
        X = floor(XSize - active_area_right_offset/4096 * XSize/4 + 0.5)
        Y = floor(YSize - active_area_bottom_offset/4096 * YSize/4 + 0.5)
    */
    if ((p_src_param->width == 0) || (!p_ctx->h_mds_ext.lvl5GdAvail)) {
        left_rel   = 0;
        right_rel  = 0;
    } else {
        left_rel   = (left_abs  * 4096*4)/p_src_param->width;
        right_rel  = (right_abs * 4096*4)/p_src_param->width;
    }
    if ((p_src_param->height == 0) || (!p_ctx->h_mds_ext.lvl5GdAvail)) {
        top_rel     = 0;
        bottom_rel  = 0;
    } else {
        top_rel    = (top_abs   * 4096*4)/p_src_param->height;
        bottom_rel = (bottom_abs* 4096*4)/p_src_param->height;
    }
#else
    /* AOI relative offset to absolute coordinates conversion */
    left_rel   = left_abs;
    top_rel    = top_abs;
    right_rel  = p_src_param->width  - right_abs  - 1;
    bottom_rel = p_src_param->height - bottom_abs - 1;
#endif
    /* Clip to 4095. But AOI should never be larger than a quarter of the image */
    if (left_rel   > 4095) left_rel   = 4095;
    if (top_rel    > 4095) top_rel    = 4095;
    if (right_rel  > 4095) right_rel  = 4095;
    if (bottom_rel > 4095) bottom_rel = 4095;

    p_dm_reg_ipcore->Active_area_left_top       =   printLog((uint32_t)(left_rel  | (top_rel   <<12)) ,"AOI_left_top  ");
    p_dm_reg_ipcore->Active_area_bottom_right   =   printLog((uint32_t)(right_rel | (bottom_rel<<12)) ,"AOI_right_bottom ");

    frame_nr++;
}
#endif

static int comp_cfg_2_comp_reg
(
    rpu_ext_config_fixpt_main_t *p_md,
    register_ipcore_t *p_reg,
    int disable_el, int xres, int yres, cp_context_t* p_ctx
)
{
    int i,j;
    static int frame_nr = 0;

    uint32_t temp;

    /* if disable_residual flag is 0 this streram is a FEL stream, return an error */
    if (p_md->disable_residual_flag == 0) {
        return CP_EL_DETECTED;
    }


    if ((p_ctx->dbgExecParamsPrintPeriod != 0) && (frame_nr % p_ctx->dbgExecParamsPrintPeriod == 0)) {
        printLog       =  &printLog_deb      ;
        custom_sprintf =  &custom_sprintf_deb;
    } else {
        printLog       =  &printLog_rel      ;
        custom_sprintf =  &custom_sprintf_rel;
    }
    p_reg->Composer_Mode              = printLog((uint32_t)((disable_el) || p_md->disable_residual_flag),"composer_mode");
    p_reg->VDR_Resolution             = printLog((uint32_t)((yres<<13) | xres),"vdr_yres|vdr_xres");
    p_reg->Bit_Depth                  = printLog((uint32_t)((p_md->rpu_VDR_bit_depth<<8) |
                                                             (p_md->rpu_EL_bit_depth<<4) |
                                                              p_md->rpu_BL_bit_depth),"VDR_bpp|EL_bpp|BL_bpp");
    p_reg->Coefficient_Log2_Denominator  = printLog((uint32_t)p_md->coefficient_log2_denom,"coefficient_log2_denom");
    p_reg->BL_Num_Pivots_Y            = printLog((uint32_t)p_md->num_pivots[0],"BL_Pivot_Number");

    temp          =(p_md->pivot_value[0][1]<<10) | p_md->pivot_value[0][0];
    p_reg->BL_Pivot[0] = printLog(temp,"bl_pivot_y1|bl_pivot_y0");
    temp          =(p_md->pivot_value[0][3]<<10) | p_md->pivot_value[0][2];
    p_reg->BL_Pivot[1] = printLog(temp,"bl_pivot_y3|bl_pivot_y2");
    temp          =(p_md->pivot_value[0][5]<<10) | p_md->pivot_value[0][4];
    p_reg->BL_Pivot[2] = printLog(temp,"bl_pivot_y5|bl_pivot_y4");
    temp          =(p_md->pivot_value[0][7]<<10) | p_md->pivot_value[0][6];
    p_reg->BL_Pivot[3] = printLog(temp,"bl_pivot_y7|bl_pivot_y6");
    p_reg->BL_Pivot[4] = printLog(p_md->pivot_value[0][8],"bl_pivot_y8");

    temp  =  p_md->poly_order[0][0];
    temp |= (p_md->poly_order[0][1]<<2);
    temp |= (p_md->poly_order[0][2]<<4);
    temp |= (p_md->poly_order[0][3]<<6);
    temp |= (p_md->poly_order[0][4]<<8);
    temp |= (p_md->poly_order[0][5]<<10);
    temp |= (p_md->poly_order[0][6]<<12);
    temp |= (p_md->poly_order[0][7]<<14);
    p_reg->BL_Order = printLog(temp,"bl_order_y");

    for(i=0 ; i < 8; i++) {
        for(j=0 ; j < 3; j++) {
            custom_sprintf(cStr,"bl_coeff_y%d%d",i,j);
            p_reg->BL_Coefficient_Y[i][j]   = printLog((uint32_t)(((p_md->poly_coef_int[0][i][j]<<p_md->coefficient_log2_denom) | p_md->poly_coef[0][i][j]) & 0x3FFFFFFF),cStr);
        }
    }
    p_reg->EL_NLQ_Offset_Y            = printLog((uint32_t)p_md->NLQ_offset[0],"el_nlq_offset_y");
    p_reg->EL_Coefficient_Y[0]        = printLog((uint32_t)(((p_md->NLQ_coeff_int[0][0]<<p_md->coefficient_log2_denom) | p_md->NLQ_coeff[0][0]) & 0x00FFFFFF),"el_coeff_y0");
    p_reg->EL_Coefficient_Y[1]        = printLog((uint32_t)(((p_md->NLQ_coeff_int[0][1]<<p_md->coefficient_log2_denom) | p_md->NLQ_coeff[0][1]) & 0x00FFFFFF),"el_coeff_y1");
    p_reg->EL_Coefficient_Y[2]        = printLog((uint32_t)(((p_md->NLQ_coeff_int[0][2]<<p_md->coefficient_log2_denom) | p_md->NLQ_coeff[0][2]) & 0x00FFFFFF),"el_coeff_y2");

    /* U */
    p_reg->Mapping_IDC_U              = printLog((uint32_t)p_md->mapping_idc[1] ,"mapping_idc_u");
    p_reg->BL_Num_Pivots_U = printLog((uint32_t)p_md->num_pivots[1], "bl_num_pivots_u");

    temp  = p_md->pivot_value[1][0];
    temp |= (p_md->pivot_value[1][1]<<10);
    p_reg->BL_Pivot_U[0]          = printLog(temp,"bl_pivot_u_reg1");
    temp  = p_md->pivot_value[1][2];
    temp |= (p_md->pivot_value[1][3]<<10);
    p_reg->BL_Pivot_U[1]          = printLog(temp,"bl_pivot_u_reg2");
    p_reg->BL_Pivot_U[2]          = printLog(p_md->pivot_value[1][4],"bl_pivot_u_reg3");

    temp  =  p_md->poly_order[1][0];
    temp |= (p_md->poly_order[1][1]<<2);
    temp |= (p_md->poly_order[1][2]<<4);
    temp |= (p_md->poly_order[1][3]<<6);
    p_reg->BL_Order_U               = printLog(temp,"bl_order_u");


    for(i=0 ; i < 4; i++) {
        for(j=0 ; j < 3; j++) {
            custom_sprintf(cStr,"bl_coeff_u%d%d",i,j);
            p_reg->BL_Coefficient_U[i][j]   = printLog((uint32_t)(((p_md->poly_coef_int[1][i][j]<<p_md->coefficient_log2_denom) | p_md->poly_coef[1][i][j]) & 0x3FFFFFFF),cStr);
        }
    }

    for(i=0 ; i < 22; i++) {
        uint64_t u64_temp;
        custom_sprintf(cStr,"mmr_coeff_u%d",i);
        u64_temp         = ((int64_t)p_md->MMR_coef_int[0][i]<<p_md->coefficient_log2_denom) | p_md->MMR_coef[0][i];

        p_reg->MMR_Coefficient_U[i][0] = printLog(u64_temp & 0xFFFFFFFF,cStr);
        p_reg->MMR_Coefficient_U[i][1] = printLog((u64_temp>>32) & 0xFF,cStr);
    }
    p_reg->MMR_Order_U                = printLog((uint32_t)p_md->MMR_order[0],"mmr_order_u");

    p_reg->EL_NLQ_Offset_U            = printLog((uint32_t)p_md->NLQ_offset[1],"el_nlq_offset_u");
    p_reg->EL_Coefficient_U[0]        = printLog((uint32_t)(((p_md->NLQ_coeff_int[1][0]<<p_md->coefficient_log2_denom) | p_md->NLQ_coeff[1][0]) & 0x00FFFFFF),"el_coeff_u0");
    p_reg->EL_Coefficient_U[1]        = printLog((uint32_t)(((p_md->NLQ_coeff_int[1][1]<<p_md->coefficient_log2_denom) | p_md->NLQ_coeff[1][1]) & 0x00FFFFFF),"el_coeff_u1");
    p_reg->EL_Coefficient_U[2]        = printLog((uint32_t)(((p_md->NLQ_coeff_int[1][2]<<p_md->coefficient_log2_denom) | p_md->NLQ_coeff[1][2]) & 0x00FFFFFF),"el_coeff_u2");

    /* V */
    p_reg->Mapping_IDC_V              = printLog((uint32_t)p_md->mapping_idc[2] ,"mapping_idc_v");
    p_reg->BL_Num_Pivots_V            = printLog((uint32_t)p_md->num_pivots[2]  ,"bl_num_pivots_v");

    temp  = p_md->pivot_value[2][0];
    temp |= (p_md->pivot_value[2][1]<<10);
    p_reg->BL_Pivot_V[0]          = printLog(temp,"bl_pivot_v_reg1");
    temp  = p_md->pivot_value[2][2];
    temp |= (p_md->pivot_value[2][3]<<10);
    p_reg->BL_Pivot_V[1]          = printLog(temp,"bl_pivot_v_reg2");
    p_reg->BL_Pivot_V[2]          = printLog(p_md->pivot_value[2][4],"bl_pivot_v_reg3");

    temp  =  p_md->poly_order[2][0];
    temp |= (p_md->poly_order[2][1]<<2);
    temp |= (p_md->poly_order[2][2]<<4);
    temp |= (p_md->poly_order[2][3]<<6);
    p_reg->BL_Order_V               = printLog(temp,"bl_order_v");


    for(i=0 ; i < 4; i++) {
        for(j=0 ; j < 3; j++) {
            custom_sprintf(cStr,"bl_coeff_v%d%d",i,j);
            p_reg->BL_Coefficient_V[i][j]   = printLog((uint32_t)(((p_md->poly_coef_int[2][i][j]<<p_md->coefficient_log2_denom) | p_md->poly_coef[2][i][j]) & 0x3FFFFFFF),cStr);
        }
    }

    for(i=0 ; i < 22; i++) {
        uint64_t u64_temp;
        custom_sprintf(cStr,"mmr_coeff_v%d",i);
        u64_temp = ((int64_t)p_md->MMR_coef_int[1][i]<<p_md->coefficient_log2_denom) | p_md->MMR_coef[1][i];

        p_reg->MMR_Coefficient_V[i][0] = printLog(u64_temp & 0xFFFFFFFF,cStr);
        p_reg->MMR_Coefficient_V[i][1] = printLog((u64_temp>>32) & 0xFF,cStr);
    }
    p_reg->MMR_Order_V                = printLog((uint32_t)p_md->MMR_order[1],"mmr_order_v");

    p_reg->EL_NLQ_Offset_V            = printLog((uint32_t)p_md->NLQ_offset[2],"el_nlq_offset_u");
    p_reg->EL_Coefficient_V[0]        = printLog((uint32_t)(((p_md->NLQ_coeff_int[2][0]<<p_md->coefficient_log2_denom) | p_md->NLQ_coeff[2][0]) & 0x00FFFFFF),"el_coeff_u0");
    p_reg->EL_Coefficient_V[1]        = printLog((uint32_t)(((p_md->NLQ_coeff_int[2][1]<<p_md->coefficient_log2_denom) | p_md->NLQ_coeff[2][1]) & 0x00FFFFFF),"el_coeff_u1");
    p_reg->EL_Coefficient_V[2]        = printLog((uint32_t)(((p_md->NLQ_coeff_int[2][2]<<p_md->coefficient_log2_denom) | p_md->NLQ_coeff[2][2]) & 0x00FFFFFF),"el_coeff_u2");

    frame_nr++;
    return 0;
}

static void dmKs2dmlut(DmKsFxp_t *pKs, dm_lut_t *p_dm_lut)
{
    int16_t i;
    uint64_t *lut_pack_lo, *lut_pack_hi;
    const unsigned short *lut = pKs->ksOMap.ksGmLut.lutMap;

    for (i = 0; i < 256; i++) {
        p_dm_lut->tcLut[i] = ((0xFFF & pKs->ksTMap.tmInternal515Lut[2*i+2]) << 12) | (0xFFF & pKs->ksTMap.tmInternal515Lut[2*i+1]);
    }
    for (i = 0; i < DEF_G2L_LUT_SIZE; i++)
        p_dm_lut->g2L[i] = pKs->ksIMap.g2L[i];
    /* 3 * 3 lut entries per 16 byte memory line, with 20 bit padding */
#if USE_12BITS_IN_3D_LUT == 0
    for (i = 0; i < (GMLUT_MAX_DIM*GMLUT_MAX_DIM*GMLUT_MAX_DIM) / 3; i++) {
        lut_pack_lo = (uint64_t*)&p_dm_lut->lut3D[16 * i];
        lut_pack_hi = (uint64_t*)&p_dm_lut->lut3D[16 * i + 8];
        *lut_pack_lo =
             (uint64_t)(0xFFF & (lut[9 * i    ] >>4)) |
            ((uint64_t)(0xFFF & (lut[9 * i + 1] >>4)) << 12) |
            ((uint64_t)(0xFFF & (lut[9 * i + 2] >>4)) << 24) |
            ((uint64_t)(0xFFF & (lut[9 * i + 3] >>4)) << 36) |
            ((uint64_t)(0xFFF & (lut[9 * i + 4] >>4)) << 48) |
            ((uint64_t)(0xFFF & (lut[9 * i + 5] >>4)) << 60);
        *lut_pack_hi =
            ((uint64_t)(0xFFF & (lut[9 * i + 5] >>4)) >>  4) |
            ((uint64_t)(0xFFF & (lut[9 * i + 6] >>4)) <<  8) |
            ((uint64_t)(0xFFF & (lut[9 * i + 7] >>4)) << 20) |
            ((uint64_t)(0xFFF & (lut[9 * i + 8] >>4)) << 32);
    }
    /* handle last incomplete line with only 2 * 3 entries */
    lut_pack_lo = (uint64_t*)&p_dm_lut->lut3D[16 * i];
    lut_pack_hi = (uint64_t*)&p_dm_lut->lut3D[16 * i + 8];
    *lut_pack_lo =
         (uint64_t)(0xFFF & (lut[9 * i    ] >>4)) |
        ((uint64_t)(0xFFF & (lut[9 * i + 1] >>4)) << 12) |
        ((uint64_t)(0xFFF & (lut[9 * i + 2] >>4)) << 24) |
        ((uint64_t)(0xFFF & (lut[9 * i + 3] >>4)) << 36) |
        ((uint64_t)(0xFFF & (lut[9 * i + 4] >>4)) << 48) |
        ((uint64_t)(0xFFF & (lut[9 * i + 5] >>4)) << 60);
    *lut_pack_hi =
        ((uint64_t)(0xFFF & (lut[9 * i + 5] >>4)) >> 4);
#else
    for (i = 0; i < (GMLUT_MAX_DIM*GMLUT_MAX_DIM*GMLUT_MAX_DIM) / 3; i++) {
        lut_pack_lo = (uint64_t*)&p_dm_lut->lut3D[16 * i];
        lut_pack_hi = (uint64_t*)&p_dm_lut->lut3D[16 * i + 8];
        *lut_pack_lo =
             (uint64_t)(0xFFF & lut[9 * i    ]) |
            ((uint64_t)(0xFFF & lut[9 * i + 1]) << 12) |
            ((uint64_t)(0xFFF & lut[9 * i + 2]) << 24) |
            ((uint64_t)(0xFFF & lut[9 * i + 3]) << 36) |
            ((uint64_t)(0xFFF & lut[9 * i + 4]) << 48) |
            ((uint64_t)(0xFFF & lut[9 * i + 5]) << 60);
        *lut_pack_hi =
            ((uint64_t)(0xFFF & lut[9 * i + 5]) >>  4) |
            ((uint64_t)(0xFFF & lut[9 * i + 6]) <<  8) |
            ((uint64_t)(0xFFF & lut[9 * i + 7]) << 20) |
            ((uint64_t)(0xFFF & lut[9 * i + 8]) << 32);
    }
    /* handle last incomplete line with only 2 * 3 entries */
    lut_pack_lo = (uint64_t*)&p_dm_lut->lut3D[16 * i];
    lut_pack_hi = (uint64_t*)&p_dm_lut->lut3D[16 * i + 8];
    *lut_pack_lo =
         (uint64_t)(0xFFF & lut[9 * i    ]) |
        ((uint64_t)(0xFFF & lut[9 * i + 1]) << 12) |
        ((uint64_t)(0xFFF & lut[9 * i + 2]) << 24) |
        ((uint64_t)(0xFFF & lut[9 * i + 3]) << 36) |
        ((uint64_t)(0xFFF & lut[9 * i + 4]) << 48) |
        ((uint64_t)(0xFFF & lut[9 * i + 5]) << 60);
    *lut_pack_hi =
        ((uint64_t)(0xFFF & lut[9 * i + 5]) >> 4);
#endif /* USE_12BITS_IN_3D_LUT */
}

#else //#if IPCORE
/*dolby*/
static int comp_cfg_2_comp_reg
(
    rpu_ext_config_fixpt_main_t *p_md,
    composer_register_t *p_reg,
    int disable_el, int xres, int yres, cp_context_t* p_ctx
)
{
    int i,j;
    static int frame_nr = 0;
    (void) xres;
    (void) yres;

    /* if disable_residual flag is 0 this stream contains an ehnacement layer, return an error if EL is not supported */
    if (!SUPPORT_EL && (p_md->disable_residual_flag == 0)) {
        return CP_EL_DETECTED;
    }

    if ((p_ctx->dbgExecParamsPrintPeriod != 0) && (frame_nr % p_ctx->dbgExecParamsPrintPeriod == 0)) {
        printLog       =  &printLog_deb      ;
        printLog64     =  &printLog64_deb    ;
        custom_sprintf =  &custom_sprintf_deb;
    } else {
        printLog       =  &printLog_rel      ;
        printLog64     =  &printLog64_rel    ;
        custom_sprintf =  &custom_sprintf_rel;
    }
    p_reg->rpu_VDR_bit_depth                 = printLog(p_md->rpu_VDR_bit_depth                  , "rpu_VDR_bit_depth       ");
    p_reg->rpu_BL_bit_depth                  = printLog(p_md->rpu_BL_bit_depth                   , "rpu_BL_bit_depth        ");
    p_reg->rpu_EL_bit_depth                  = printLog(p_md->rpu_EL_bit_depth                   , "rpu_EL_bit_depth        ");
    p_reg->coefficient_log2_denom            = printLog(p_md->coefficient_log2_denom             , "coefficient_log2_denom  ");
    p_reg->disable_EL_flag                   = printLog(p_md->disable_residual_flag || disable_el, "disable_EL_flag         ");
    p_reg->el_spatial_resampling_filter_flag = printLog(p_md->el_spatial_resampling_filter_flag  , "el_resampling           ");


    /* BL Mapping */

    /* Coefficients for Luma */
    p_reg->num_pivots_y = printLog(p_md->num_pivots[0], "num_pivots_y");
    for (i=0; i<8; i++) {
        custom_sprintf(cStr,"pivot_value_y_%d",i);
        p_reg->pivot_value_y[i]= printLog(p_md->pivot_value[0][i], cStr);
    }
    for (i=0; i<8; i++) {
        custom_sprintf(cStr,"order_y_%d",i);
        p_reg->order_y[i]      = printLog(p_md->poly_order[0][i] , cStr);
    }
    for (i=0; i<8; i++) {
        for(j=0 ; j < 3; j++) {
            custom_sprintf(cStr,"coeff_y_%d",i*3+j);
            p_reg->coeff_y[i*3 + j] = printLog(((p_md->poly_coef_int[0][i][j]<<p_md->coefficient_log2_denom) | p_md->poly_coef[0][i][j])  & 0x3FFFFFFF, cStr);
        }
    }
    p_reg->pivot_value_y[8]= printLog(p_md->pivot_value[0][8], "pivot_value_y_8");

    /* Coefficients for Chroma b */
    p_reg->num_pivots_cb = printLog(p_md->num_pivots[1], "num_pivots_cb");
    for (i=0; i<5; i++) {
        custom_sprintf(cStr,"pivot_value_cb_%d",i);
        p_reg->pivot_value_cb[i] = printLog(p_md->pivot_value[1][i], cStr);
    }
    p_reg->mapping_idc_cb        = printLog(p_md->mapping_idc[1], "mapping_idc_cb");
    if (p_reg->mapping_idc_cb == 0) {
        for (i=0; i<4; i++) {
            custom_sprintf(cStr,"order_cb_%d",i);
            p_reg->order_cb[i] = printLog(p_md->poly_order[1][i], cStr);
        }
    } else {
        p_reg->order_cb[0] = printLog(p_md->MMR_order[0], "order_cb");
        for (i=1; i<4; i++) {
            p_reg->order_cb[i] = 0;
        }
    }
    for (i=0; i<4; i++) {
        for(j=0 ; j < 3; j++) {
            custom_sprintf(cStr,"coeff_cb_%d", 3*i + j);
            p_reg->coeff_cb[i*3 + j] = printLog(((p_md->poly_coef_int[1][i][j]<<p_md->coefficient_log2_denom) | p_md->poly_coef[1][i][j]) & 0x3FFFFFFF, cStr);
        }
    }
    for (i=0; i<22; i++) {
        custom_sprintf(cStr,"coeff_mmr_cb_%d",i);
        p_reg->coeff_mmr_cb[i] = printLog64((((int64_t)((int64_t)p_md->MMR_coef_int[0][i]<<p_md->coefficient_log2_denom)) | p_md->MMR_coef[0][i]) , cStr);
        /* The construct (((int64_t)0xFF << 32) | 0xFFFFFFFF) is needed for lin32 with c89 compiler it is identical to 0xFFFFFFFFFF */
    }

    /* Coefficients for Chroma r */
    p_reg->num_pivots_cr = printLog(p_md->num_pivots[2], "num_pivots_cr");
    for (i=0; i<5; i++) {
        custom_sprintf(cStr,"pivot_value_cr_%d",i);
        p_reg->pivot_value_cr[i] = printLog(p_md->pivot_value[2][i], cStr);
    }
    p_reg->mapping_idc_cr = printLog(p_md->mapping_idc[2], "mapping_idc_cr");
    if (p_reg->mapping_idc_cr == 0) {
        for (i=0; i<4; i++) {
            custom_sprintf(cStr,"order_cr_%d",i);
            p_reg->order_cr[i] = printLog(p_md->poly_order[2][i], cStr);
        }
    } else {
        p_reg->order_cr[0] = printLog(p_md->MMR_order[1], "order_cr");
        for (i=1; i<4; i++) {
            p_reg->order_cr[i] = 0;
        }
    }
    for (i=0; i<4; i++) {
        for(j=0; j<3; j++) {
            custom_sprintf(cStr,"coeff_cr_%d", 3*i + j);
            p_reg->coeff_cr[i*3 + j] = printLog(((p_md->poly_coef_int[2][i][j]<<p_md->coefficient_log2_denom) | p_md->poly_coef[2][i][j]) & 0x3FFFFFFF, cStr);
        }
    }
    for (i=0; i<22; i++) {
        custom_sprintf(cStr,"coeff_mmr_cr_%d",i);
        p_reg->coeff_mmr_cr[i] = printLog64(((int64_t)((int64_t)p_md->MMR_coef_int[1][i]<<p_md->coefficient_log2_denom) | p_md->MMR_coef[1][i]) & (((int64_t)0xFF << 32) | 0xFFFFFFFF), cStr);
        /* The construct (((int64_t)0xFF << 32) | 0xFFFFFFFF) is needed for lin32 with c89 compiler it is identical to 0xFFFFFFFFFF */
    }


    /* EL NLdQ */
    if (!p_reg->disable_EL_flag) {
        p_reg->NLQ_offset_y = printLog(p_md->NLQ_offset[0], "NLQ_offset_y");
        for (i=0; i<3; i++) {
            custom_sprintf(cStr,"NLQ_coeff_y_%d",i);
            p_reg->NLQ_coeff_y[i] = printLog((p_md->NLQ_coeff_int[0][i]<<p_md->coefficient_log2_denom) | p_md->NLQ_coeff[0][i], cStr);
        }
        p_reg->NLQ_offset_cb  = printLog(p_md->NLQ_offset[1], "NLQ_offset_cb");
        for (i=0; i<3; i++) {
            custom_sprintf(cStr,"NLQ_coeff_cb_%d",i);
            p_reg->NLQ_coeff_cb[i] = printLog((p_md->NLQ_coeff_int[1][i]<<p_md->coefficient_log2_denom) | p_md->NLQ_coeff[1][i], cStr);
        }
        p_reg->NLQ_offset_cr = printLog(p_md->NLQ_offset[2], "NLQ_offset_cr");
        for (i=0; i<3; i++) {
            custom_sprintf(cStr,"NLQ_coeff_cr_%d",i);
            p_reg->NLQ_coeff_cr[i] = printLog((p_md->NLQ_coeff_int[2][i]<<p_md->coefficient_log2_denom) | p_md->NLQ_coeff[2][i], cStr);
        }
    }

    frame_nr++;
    return 0;
}

#if DM286_MODE
#ifndef RTK_EDBEC_CONTROL
static void dmKs2dmreg(DmKsFxp_t *pKs, dm_register_286_t *p_dm_reg, cp_context_t* p_ctx, src_param_t *p_src_param)
{
    int16_t i, j;
    static int frame_nr = 0;
    (void) p_src_param;

    if ((p_ctx->dbgExecParamsPrintPeriod != 0) && (frame_nr % p_ctx->dbgExecParamsPrintPeriod == 0)) {
        printf("\nProcessing Frame %d\n",frame_nr);
        printLog       =  &printLog_deb      ;
        custom_sprintf =  &custom_sprintf_deb;
    } else {
        printLog       =  &printLog_rel      ;
        custom_sprintf =  &custom_sprintf_rel;
}

    p_dm_reg->colNum    = printLog(pKs->ksFrmFmtI.colNum, "colNum");
    p_dm_reg->rowNum    = printLog(pKs->ksFrmFmtI.rowNum, "rowNum");
    p_dm_reg->in_clr    = printLog(pKs->ksFrmFmtI.clr   , "in_clr");
    p_dm_reg->in_bdp    = printLog(pKs->ksFrmFmtI.bdp   , "in_bdp");
    p_dm_reg->in_chrm   = printLog(pKs->ksFrmFmtI.chrm   , "in_chrm");
    p_dm_reg->sRangeMin = printLog(pKs->ksIMap.eotfParam.rangeMin, "sRangeMin");
    p_dm_reg->sRange    = printLog(pKs->ksIMap.eotfParam.range   , "sRange   ");
    p_dm_reg->sRangeInv = printLog(pKs->ksIMap.eotfParam.rangeInv, "sRangeInv");
    p_dm_reg->sEotf     = printLog(pKs->ksIMap.eotfParam.eotf    , "sEotf    ");

    p_dm_reg->sparam0 = printLog(pKs->ksIMap.eotfParam.a    , "a    ");
    p_dm_reg->sparam1 = printLog(pKs->ksIMap.eotfParam.b    , "b    ");
    p_dm_reg->sparam2 = printLog(pKs->ksIMap.eotfParam.g    , "g    ");
    p_dm_reg->sgamma  = printLog(pKs->ksIMap.eotfParam.gamma   , "gamma    ");

    p_dm_reg->m33Yuv2RgbScale2P = printLog(pKs->ksIMap.m33Yuv2RgbScale2P, "m33Yuv2RgbScale2P");
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            custom_sprintf(cStr,"m33Yuv2Rgb_%d_%d",i,j);
            p_dm_reg->m33Yuv2Rgb[i][j] = printLog(pKs->ksIMap.m33Yuv2Rgb[i][j], cStr);
    }
    for (i = 0; i < 3; i++) {
        custom_sprintf(cStr,"v3Yuv2RgbOffInRgb_%d",i);
        p_dm_reg->v3Yuv2RgbOffInRgb[i] = printLog(pKs->ksIMap.v3Yuv2RgbOffInRgb[i], cStr);
    }

    p_dm_reg->m33Rgb2OptScale2P = printLog(pKs->ksIMap.m33Rgb2LmsScale2P, "m33Rgb2LmsScale2P");
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            custom_sprintf(cStr,"m33Rgb2Lms_%d_%d",i,j);
            p_dm_reg->m33Rgb2Opt[i][j] = printLog(pKs->ksIMap.m33Rgb2Lms[i][j], cStr);
    }

    p_dm_reg->m33Opt2OptScale2P = printLog(pKs->ksIMap.m33Lms2IptScale2P, "m33Rgb2LmsScale2P");
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            custom_sprintf(cStr,"m33Lms2Ipt_%d_%d",i,j);
            p_dm_reg->m33Opt2Opt[i][j] = printLog(pKs->ksIMap.m33Lms2Ipt[i][j], cStr);
}
    p_dm_reg->Opt2OptOffset = printLog(0, "Opt2OptOffset");


    p_dm_reg->chromaWeight = printLog(pKs->ksTMap.chromaWeight, "chromaWeight");
    p_dm_reg->msWeight     = printLog(pKs->ksMs.msWeight      , "msWeight    ");
    p_dm_reg->msWeightEdge = printLog(pKs->ksMs.msEdgeWeight  , "msEdgeWeight");

    p_dm_reg->gain           = printLog(pKs->ksOMap.gain   , "gain   ");
    p_dm_reg->offset         = printLog(pKs->ksOMap.offset , "offset ");
    p_dm_reg->saturationGain = printLog(pKs->ksOMap.satGain, "satGain");

    p_dm_reg->maxC[0] = printLog(pKs->ksOMap.ksGmLut.iMaxC1    , "maxC");
    p_dm_reg->maxC[1] = printLog(pKs->ksOMap.ksGmLut.iMaxC2    , "maxC");
    p_dm_reg->maxC[2] = printLog(pKs->ksOMap.ksGmLut.iMaxC3    , "maxC");
    p_dm_reg->minC[0] = printLog(pKs->ksOMap.ksGmLut.iMinC1    , "minC");
    p_dm_reg->minC[1] = printLog(pKs->ksOMap.ksGmLut.iMinC2    , "minC");
    p_dm_reg->minC[2] = printLog(pKs->ksOMap.ksGmLut.iMinC3    , "minC");
    p_dm_reg->CInv[0] = printLog(pKs->ksOMap.ksGmLut.iDistC1Inv, "CInv");
    p_dm_reg->CInv[1] = printLog(pKs->ksOMap.ksGmLut.iDistC2Inv, "CInv");
    p_dm_reg->CInv[2] = printLog(pKs->ksOMap.ksGmLut.iDistC3Inv, "CInv");
# if EN_AOI
    p_dm_reg->aoiRow0      = printLog(pKs->ksDmCtrl.aoiRow0     , "AOI_Row0     ");
    p_dm_reg->aoiRow1Plus1 = printLog(pKs->ksDmCtrl.aoiRow1Plus1, "AOI_Row1Plus1");
    p_dm_reg->aoiCol0      = printLog(pKs->ksDmCtrl.aoiCol0     , "AOI_Col0     ");
    p_dm_reg->aoiCol1Plus1 = printLog(pKs->ksDmCtrl.aoiCol1Plus1, "AOI_Col1Plus1");
#else
    p_dm_reg->aoiRow0      = printLog(0                    , "AOI_Row0     ");
    p_dm_reg->aoiRow1Plus1 = printLog(pKs->ksFrmFmtI.rowNum, "AOI_Row1Plus1");
    p_dm_reg->aoiCol0      = printLog(0                    , "AOI_Col0     ");
    p_dm_reg->aoiCol1Plus1 = printLog(pKs->ksFrmFmtI.colNum, "AOI_Col1Plus1");
#endif
    frame_nr++;
}
#endif

static void dmKs2dmlut(DmKsFxp_t *pKs, dm_lut_286_t *p_dm_lut)
{
	int16_t i;
    for (i = 0; i < 4096; i++)
        p_dm_lut->tcLut[i] = pKs->ksTMap.tmLutI[i];
    for (i = 0; i < 3*GMLUT_MAX_DIM*GMLUT_MAX_DIM*GMLUT_MAX_DIM; i++)
        p_dm_lut->lut3D[i] = pKs->ksOMap.ksGmLut.lutMap[i];
}
#else
#ifndef RTK_EDBEC_CONTROL
static void dmKs2dmreg(DmKsFxp_t *pKs, dm_register_t *p_dm_reg, cp_context_t* p_ctx, src_param_t *p_src_param)
{
    int16_t i, j;
    static int frame_nr = 0;
    (void) p_src_param;

    if ((p_ctx->dbgExecParamsPrintPeriod != 0) && (frame_nr % p_ctx->dbgExecParamsPrintPeriod == 0)) {
        printf("\nProcessing Frame %d\n",frame_nr);
        printLog       =  &printLog_deb      ;
        custom_sprintf =  &custom_sprintf_deb;
    } else {
        printLog       =  &printLog_rel      ;
        custom_sprintf =  &custom_sprintf_rel;
    }

    p_dm_reg->colNum    = printLog(pKs->ksFrmFmtI.colNum, "colNum");
    p_dm_reg->rowNum    = printLog(pKs->ksFrmFmtI.rowNum, "rowNum");
    p_dm_reg->in_clr    = printLog(pKs->ksFrmFmtI.clr   , "in_clr");
    p_dm_reg->in_bdp    = printLog(pKs->ksFrmFmtI.bdp   , "in_bdp");
    p_dm_reg->in_chrm   = printLog(pKs->ksFrmFmtI.chrm   , "in_chrm");
    p_dm_reg->sRangeMin = printLog(pKs->ksIMap.eotfParam.rangeMin, "sRangeMin");
    p_dm_reg->sRange    = printLog(pKs->ksIMap.eotfParam.range   , "sRange   ");
    p_dm_reg->sRangeInv = printLog(pKs->ksIMap.eotfParam.rangeInv, "sRangeInv");
    p_dm_reg->sEotf     = printLog(pKs->ksIMap.eotfParam.eotf    , "sEotf    ");

    p_dm_reg->m33Yuv2RgbScale2P = printLog(pKs->ksIMap.m33Yuv2RgbScale2P, "m33Yuv2RgbScale2P");
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            custom_sprintf(cStr,"m33Yuv2Rgb_%d_%d",i,j);
            p_dm_reg->m33Yuv2Rgb[i][j] = printLog(pKs->ksIMap.m33Yuv2Rgb[i][j], cStr);
        }
    for (i = 0; i < 3; i++) {
        custom_sprintf(cStr,"v3Yuv2RgbOffInRgb_%d",i);
        p_dm_reg->v3Yuv2RgbOffInRgb[i] = printLog(pKs->ksIMap.v3Yuv2RgbOffInRgb[i], cStr);
    }

    p_dm_reg->m33Rgb2OptScale2P = printLog(pKs->ksIMap.m33Rgb2LmsScale2P, "m33Rgb2LmsScale2P");
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            custom_sprintf(cStr,"m33Rgb2Lms_%d_%d",i,j);
            p_dm_reg->m33Rgb2Opt[i][j] = printLog(pKs->ksIMap.m33Rgb2Lms[i][j], cStr);
        }

    p_dm_reg->m33Opt2OptScale2P = printLog(pKs->ksIMap.m33Lms2IptScale2P, "m33Rgb2LmsScale2P");
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            custom_sprintf(cStr,"m33Lms2Ipt_%d_%d",i,j);
            p_dm_reg->m33Opt2Opt[i][j] = printLog(pKs->ksIMap.m33Lms2Ipt[i][j], cStr);
        }
    p_dm_reg->Opt2OptOffset = printLog(0, "Opt2OptOffset");


    p_dm_reg->chromaWeight = printLog(pKs->ksTMap.chromaWeight, "chromaWeight");
#if EN_MS_OPTION
    p_dm_reg->msWeight     = printLog(pKs->ksMs.msWeight      , "msWeight    ");
    p_dm_reg->msWeightEdge = printLog(pKs->ksMs.msEdgeWeight  , "msEdgeWeight");
#else
    p_dm_reg->msWeight     = printLog(0  , "msWeight    ");
    p_dm_reg->msWeightEdge = printLog(0  , "msEdgeWeight");
#endif
    p_dm_reg->gain           = printLog(pKs->ksOMap.gain   , "gain   ");
    p_dm_reg->offset         = printLog(pKs->ksOMap.offset , "offset ");
    p_dm_reg->saturationGain = printLog(pKs->ksOMap.satGain, "satGain");

    p_dm_reg->tRangeMin     = printLog(pKs->ksOMap.tRangeMin    , "tRangeMin    ");
    p_dm_reg->tRange        = printLog(pKs->ksOMap.tRange       , "tRange       ");
    p_dm_reg->tRangeOverOne = printLog(pKs->ksOMap.tRangeOverOne, "tRangeOverOne");
    p_dm_reg->tRangeInv     = printLog(pKs->ksOMap.tRangeInv    , "tRangeInv    ");

    p_dm_reg->m33Rgb2YuvScale2P = printLog(pKs->ksOMap.m33Rgb2YuvScale2P, "m33Rgb2YuvScale2P");
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            custom_sprintf(cStr,"m33Rgb2Yuv_%d_%d",i,j);
            p_dm_reg->m33Rgb2Yuv[i][j] = printLog(pKs->ksOMap.m33Rgb2Yuv[i][j], cStr);
        }
    for (i = 0; i < 3; i++) {
        custom_sprintf(cStr,"v3Rgb2YuvOff_%d",i);
        p_dm_reg->v3Rgb2YuvOff[i]  = printLog(pKs->ksOMap.v3Rgb2YuvOff[i], cStr);
    }
    p_dm_reg->maxC[0] = printLog(pKs->ksOMap.ksGmLut.iMaxC1    , "maxC");
    p_dm_reg->maxC[1] = printLog(pKs->ksOMap.ksGmLut.iMaxC2    , "maxC");
    p_dm_reg->maxC[2] = printLog(pKs->ksOMap.ksGmLut.iMaxC3    , "maxC");
    p_dm_reg->minC[0] = printLog(pKs->ksOMap.ksGmLut.iMinC1    , "minC");
    p_dm_reg->minC[1] = printLog(pKs->ksOMap.ksGmLut.iMinC2    , "minC");
    p_dm_reg->minC[2] = printLog(pKs->ksOMap.ksGmLut.iMinC3    , "minC");
    p_dm_reg->CInv[0] = printLog(pKs->ksOMap.ksGmLut.iDistC1Inv, "CInv");
    p_dm_reg->CInv[1] = printLog(pKs->ksOMap.ksGmLut.iDistC2Inv, "CInv");
    p_dm_reg->CInv[2] = printLog(pKs->ksOMap.ksGmLut.iDistC3Inv, "CInv");
    p_dm_reg->out_clr = printLog(pKs->ksFrmFmtO.clr         , "out_clr");
    p_dm_reg->out_bdp = printLog(pKs->ksFrmFmtO.bdp         , "out_bdp");
    p_dm_reg->lut_type    = printLog(pKs->ksOMap.ksGmLut.valTp  , "lut_type   ");
    p_dm_reg->tcLutMaxVal = printLog(pKs->ksTMap.tmLutMaxVal    , "tcLutMaxVal");
# if EN_AOI
    p_dm_reg->aoiRow0      = printLog(pKs->ksDmCtrl.aoiRow0     , "AOI_Row0     ");
    p_dm_reg->aoiRow1Plus1 = printLog(pKs->ksDmCtrl.aoiRow1Plus1, "AOI_Row1Plus1");
    p_dm_reg->aoiCol0      = printLog(pKs->ksDmCtrl.aoiCol0     , "AOI_Col0     ");
    p_dm_reg->aoiCol1Plus1 = printLog(pKs->ksDmCtrl.aoiCol1Plus1, "AOI_Col1Plus1");
#endif
    frame_nr++;
    }
#endif

static void dmKs2dmlut(DmKsFxp_t *pKs, dm_lut_t *p_dm_lut)
{
    int16_t i;
    for (i = 0; i < (512+3); i++)
        p_dm_lut->tcLut[i] = pKs->ksTMap.tmInternal515Lut[i];
    for (i = 0; i < DEF_G2L_LUT_SIZE; i++)
        p_dm_lut->g2L[i] = pKs->ksIMap.g2L[i];
    for (i = 0; i < 3*GMLUT_MAX_DIM*GMLUT_MAX_DIM*GMLUT_MAX_DIM; i++)
        p_dm_lut->lut3D[i] = pKs->ksOMap.ksGmLut.lutMap[i];
}
#endif
#endif

static int parse_dolby_vsif(uint8_t* vsif, int *low_latency, int* bl_avail, int *eff_tmax) {
    uint8_t *HB, *PB;
    int i, length;
    if (vsif == NULL) {
        *low_latency = 0;
        *eff_tmax = 0;
        return 0;
    }
    HB = vsif;
    PB = &vsif[3];

    if (HB[0] != 0x81) return CP_ERR_UNREC_VSIF; /* Packet type */
    if (HB[1] != 0x01) return CP_ERR_UNREC_VSIF; /* Version */
    length = HB[2]; /* VSIF Length */
    if (((PB[3] << 16) | (PB[2] << 8) | (PB[1])) == 0x00D046) { /* found Dolby VSIF */
        if (length != 0x1B)
            return CP_ERR_UNREC_VSIF;
        /* PB[4] Reserved (0x00) (6 bit) | Dolby_Vision Signal (1 bit) | Low_Latency (1 bit) */
        *low_latency = (PB[4] & 0x01);
        /* PB[5] Backlt_Ctrl_MD_Present (1 bit) | Auxiliary_MD_Present (1 bit )| Reserved (=0x0) (2bit) |  Eff_tmax_PQ_hi[3:0] (4 bit) */
        *bl_avail = (PB[5] & 0x80); /* Backlt_Ctrl_MD_Present */
        if (*bl_avail) {
            /* PB[6] Eff_tmax_PQ_low[7:0] */
            *eff_tmax = ((PB[5] & 0x0F) << 8) | PB[6];
        }
        /* PB[7] Auxiliary_runmode[7:0] */
        /* PB[8] Auxiliary_runversion[7:0] */
        /* PB[9] Auxiliary_debug0[7:0] */
        /* skip PB[7-9] for now */
        for (i=10; i<=27; i++) {
            if (PB[i] != 0) return CP_ERR_UNREC_VSIF;
        }
    } else if (((PB[3] << 16) | (PB[2] << 8) | (PB[1])) == 0x000C03) { /* found HDMI 1.4b VSIF */
        if (length != 0x18)
            return CP_ERR_UNREC_VSIF;
        for (i=6; i<=24; i++) {
            if (PB[i] != 0) return CP_ERR_UNREC_VSIF;
        }
    } else {
        /* found unknown VSIF */
        return CP_ERR_UNREC_VSIF;
    }
    return 0;
}

int get_dm_kernel_buf(h_cp_context_t h_ctx, char* buf) {
    cp_context_t* p_ctx = h_ctx;
    int size;
    size = DumpKsBinBuf(p_ctx->h_ks, buf);

    return size;
}

int init_cp(h_cp_context_t h_ctx, run_mode_t *run_mode, char* lut_buf, char* dm_ctx_buf) {

    cp_context_t* p_ctx = h_ctx;
    DmCfgFxp_t   *p_dm_cfg = &p_ctx->dm_cfg;
    (void) run_mode;

    p_ctx->last_view_mode_id = -1; /* set last viewing mode to an invalid number so it will change on the first frame */
    p_ctx->last_bl_scaler = -1; /* set last backlight scaler to an invalid number so it will change on the first frame */
    p_ctx->last_input_format = FORMAT_INVALID; /* set last input format to an invalid number so it will change on the first frame */

    #if DM_VER_LOWER_THAN212
    p_ctx->gm_lut.LutMap = (unsigned short *)lut_buf;
    p_ctx->gm_lut_mode1.LutMap = (unsigned short *)(lut_buf + sizeof(unsigned short) * GM_LUT_SIZE);
    p_ctx->gm_lut_mode2.LutMap = (unsigned short *)(lut_buf + 2 * sizeof(unsigned short) * GM_LUT_SIZE);
    #else
    /* to avoid compiler warning */
    (void) lut_buf;
    #endif

    p_ctx->h_ks      = (HDmKsFxp_t)(dm_ctx_buf                          );
    p_ctx->h_mds_ext = (HMdsExt_t )((char*)p_ctx->h_ks      + sizeof(DmKsFxp_t));
    p_ctx->h_dm      = (HDmFxp_t  )((char*)p_ctx->h_mds_ext + sizeof(MdsExt_t ));

    /* CTRL: create dm cfg/ctrl plan */
    memset(p_dm_cfg, 0, sizeof(DmCfgFxp_t));

    /* 1st round set up: internal default cfg value */
    InitDmCfg(CPlatformCpu, &p_ctx->mmg, p_dm_cfg);
    if (p_dm_cfg == NULL)
    {
        printf("InitDmCfg() failed\n");
        return -1;
    }

    /* Initialize the kernel structure */
    if ((InitDmKs(p_dm_cfg, p_ctx->h_ks)) == NULL)
    {
        return -1;
    }

    if ((InitDm(p_dm_cfg, p_ctx->h_dm)) == NULL)
    {
        return -1;
    }

    InitMdsExt(p_dm_cfg, p_ctx->h_mds_ext);
    return 0;

}

static int setup_src_config(cp_context_t* p_ctx, signal_format_t  input_format, src_param_t *p_src_param, int low_latency) {
    DmCfgFxp_t   *p_dm_cfg = &p_ctx->dm_cfg;
    unsigned char *vsvdb = p_ctx->cur_pq_config->target_display_config.vsvdb;
    int vsvdb_version;

    p_dm_cfg->tgtSigEnv.RowNum = p_src_param->height;
    p_dm_cfg->tgtSigEnv.ColNum = p_src_param->width;

    /******************************/
    /* Source Config              */
    /******************************/
    p_dm_cfg->srcSigEnv.RowNum = p_src_param->height;
    p_dm_cfg->srcSigEnv.ColNum = p_src_param->width;
    p_dm_cfg->srcSigEnv.Dtp    = CDtpU16;
    p_dm_cfg->srcSigEnv.Clr    = CClrYuv;
    p_dm_cfg->srcSigEnv.CrossTalk   = p_ctx->cur_pq_config->target_display_config.crossTalk;

    if (input_format == FORMAT_DOVI) {
        /* DoVi source config will be updated by metadata */
        if (!low_latency) {
            p_dm_cfg->srcSigEnv.Chrm   = CChrm420;
            p_dm_cfg->srcSigEnv.Weav   = CWeavPlnr;
        } else {
            p_dm_cfg->srcSigEnv.Clr    = (p_src_param->src_color_format == 0) ? CClrYuv : CClrRgb;
            p_dm_cfg->srcSigEnv.Chrm   = (p_src_param->src_color_format == 0) ? CChrm422 : CChrm444;
            p_dm_cfg->srcSigEnv.Weav   = (p_src_param->src_color_format == 0) ? CWeavUyVy : CWeavIntl;
            p_dm_cfg->srcSigEnv.Eotf   = CEotfPq;
            p_dm_cfg->srcSigEnv.Rng    = CRngNarrow;
            p_dm_cfg->srcSigEnv.Bdp    = p_src_param->src_bit_depth;
            p_dm_cfg->srcSigEnv.YuvXferSpec = CYuvXferSpecR2020;
            p_dm_cfg->srcSigEnv.Rgb2LmsM33Ext = 0;
            p_dm_cfg->srcSigEnv.Rgb2LmsRgbwExt = 1;
            vsvdb_version = (vsvdb[0] >> 5);
            if (vsvdb_version == 1) {
                if ((vsvdb[3] & 0x03) != 1)
                    return CP_ERR_LOW_LATENCY_NOT_SUPPORTED;
            p_dm_cfg->srcSigEnv.Min   = ((vsvdb[2] >> 1)*(vsvdb[2] >> 1)*(1<<18)/(127*127));
            p_dm_cfg->srcSigEnv.Max   = (100 + 50 * (vsvdb[1] >> 1)) * (1<<18);
            p_dm_cfg->srcSigEnv.MinPq = LToPQ12((uint32_t)(p_dm_cfg->srcSigEnv.Min));
            p_dm_cfg->srcSigEnv.MaxPq = LToPQ12((uint32_t)(p_dm_cfg->srcSigEnv.Max));
            p_dm_cfg->srcSigEnv.V8Rgbw[0]  = (0xA0 | (vsvdb[6] >> 3)) << (RGBW_SCALE-8);
            p_dm_cfg->srcSigEnv.V8Rgbw[1]  = (0x40 | (((vsvdb[6] & 0x7 )<< 2) | ((vsvdb[5] & 0x01) << 1) | (vsvdb[4] & 0x01))) << (RGBW_SCALE-8);
            p_dm_cfg->srcSigEnv.V8Rgbw[2]  = (0x00 | (vsvdb[4] >> 1)) << (RGBW_SCALE-8);
            p_dm_cfg->srcSigEnv.V8Rgbw[3]  = (0x80 | (vsvdb[5] >> 1)) << (RGBW_SCALE-8);
            p_dm_cfg->srcSigEnv.V8Rgbw[4]  = (0x20 | (vsvdb[3] >> 5)) << (RGBW_SCALE-8);
            p_dm_cfg->srcSigEnv.V8Rgbw[5]  = (0x08 | ((vsvdb[3] >> 2) & 0x7)) << (RGBW_SCALE-8);
            p_dm_cfg->srcSigEnv.V8Rgbw[6]  = WP_D65x;
            p_dm_cfg->srcSigEnv.V8Rgbw[7]  = WP_D65y;
        } else if (vsvdb_version == 2) {
                p_dm_cfg->srcSigEnv.MinPq = (20*(vsvdb[1] >> 3));
                p_dm_cfg->srcSigEnv.MaxPq = (2055 + 65 * (vsvdb[2] >> 3));
            p_dm_cfg->srcSigEnv.Min   = PQ12ToL(p_dm_cfg->srcSigEnv.MinPq);
            p_dm_cfg->srcSigEnv.Max   = PQ12ToL(p_dm_cfg->srcSigEnv.MaxPq);
                p_dm_cfg->srcSigEnv.V8Rgbw[0]  = (0xA0 | (vsvdb[5] >>  3)) << (RGBW_SCALE-8);
                p_dm_cfg->srcSigEnv.V8Rgbw[1]  = (0x40 | (vsvdb[6] >>  3)) << (RGBW_SCALE-8);
                p_dm_cfg->srcSigEnv.V8Rgbw[2]  = (0x00 | (vsvdb[3] >>  1)) << (RGBW_SCALE-8);
                p_dm_cfg->srcSigEnv.V8Rgbw[3]  = (0x80 | (vsvdb[4] >>  1)) << (RGBW_SCALE-8);
                p_dm_cfg->srcSigEnv.V8Rgbw[4]  = (0x20 | (vsvdb[5] & 0x7)) << (RGBW_SCALE-8);
                p_dm_cfg->srcSigEnv.V8Rgbw[5]  = (0x08 | (vsvdb[6] & 0x7)) << (RGBW_SCALE-8);
            p_dm_cfg->srcSigEnv.V8Rgbw[6]  = WP_D65x;
            p_dm_cfg->srcSigEnv.V8Rgbw[7]  = WP_D65y;
        } else {
                return CP_ERR_LOW_LATENCY_NOT_SUPPORTED;
            }
        }
    } else if (input_format == FORMAT_HDR10) {
        p_dm_cfg->srcSigEnv.Clr    = CClrYuv;
        p_dm_cfg->srcSigEnv.Chrm   = (p_src_param->src_chroma_format == 0) ? CChrm420 :  (p_src_param->src_chroma_format == 1) ? CChrm422  : CChrm422;
        p_dm_cfg->srcSigEnv.Weav   = (p_src_param->src_chroma_format == 0) ? CWeavPlnr : (p_src_param->src_chroma_format == 1) ? CWeavUyVy : CWeavPlnr;
        p_dm_cfg->srcSigEnv.Eotf   = CEotfPq;
        p_dm_cfg->srcSigEnv.Rng    = (p_src_param->src_yuv_range == SIG_RANGE_SMPTE) ? CRngNarrow : CRngFull;
        p_dm_cfg->srcSigEnv.Rgb2LmsM33Ext = 0;
        p_dm_cfg->srcSigEnv.Rgb2LmsRgbwExt = 1;
        p_dm_cfg->srcSigEnv.V8Rgbw[0]  = div64_s64(((uint64_t)p_src_param->hdr10_param.Rx * (1<< RGBW_SCALE)),50000);
        p_dm_cfg->srcSigEnv.V8Rgbw[1]  = div64_s64(((uint64_t)p_src_param->hdr10_param.Ry * (1<< RGBW_SCALE)),50000);
        p_dm_cfg->srcSigEnv.V8Rgbw[2]  = div64_s64(((uint64_t)p_src_param->hdr10_param.Gx * (1<< RGBW_SCALE)),50000);
        p_dm_cfg->srcSigEnv.V8Rgbw[3]  = div64_s64(((uint64_t)p_src_param->hdr10_param.Gy * (1<< RGBW_SCALE)),50000);
        p_dm_cfg->srcSigEnv.V8Rgbw[4]  = div64_s64(((uint64_t)p_src_param->hdr10_param.Bx * (1<< RGBW_SCALE)),50000);
        p_dm_cfg->srcSigEnv.V8Rgbw[5]  = div64_s64(((uint64_t)p_src_param->hdr10_param.By * (1<< RGBW_SCALE)),50000);
        p_dm_cfg->srcSigEnv.V8Rgbw[6]  = div64_s64(((uint64_t)p_src_param->hdr10_param.Wx * (1<< RGBW_SCALE)),50000);
        p_dm_cfg->srcSigEnv.V8Rgbw[7]  = div64_s64(((uint64_t)p_src_param->hdr10_param.Wy * (1<< RGBW_SCALE)),50000);
        p_dm_cfg->srcSigEnv.YuvXferSpec = CYuvXferSpecR2020;
        p_dm_cfg->srcSigEnv.Bdp    = (p_src_param->input_mode == INPUT_MODE_HDMI) ? p_src_param->src_bit_depth : 12;
        p_dm_cfg->srcSigEnv.Min    = (p_src_param->hdr10_param.min_display_mastering_luminance*(1<<18))/10000;
#ifndef RTK_EDBEC_CONTROL
        p_dm_cfg->srcSigEnv.Max    = (uint32_t)(((uint64_t)p_src_param->hdr10_param.max_display_mastering_luminance*(1<<18))/10000);
#else
        p_dm_cfg->srcSigEnv.Max    = (uint32_t)div_u64(((uint64_t)p_src_param->hdr10_param.max_display_mastering_luminance*(1<<18)),10000);
#endif
        p_dm_cfg->srcSigEnv.MinPq  = LToPQ12(p_dm_cfg->srcSigEnv.Min);
        p_dm_cfg->srcSigEnv.MaxPq  = LToPQ12(p_dm_cfg->srcSigEnv.Max);
    } else if (input_format == FORMAT_SDR) {
        p_dm_cfg->srcSigEnv.Clr    = CClrYuv;
        p_dm_cfg->srcSigEnv.Chrm   = (p_src_param->src_chroma_format == 0) ? CChrm420 : CChrm422;
        p_dm_cfg->srcSigEnv.Weav   = (p_src_param->src_chroma_format == 0) ? CWeavPlnr : CWeavUyVy;
        p_dm_cfg->srcSigEnv.Eotf   = CEotfBt1886;
        p_dm_cfg->srcSigEnv.Bdp    = (p_src_param->input_mode == INPUT_MODE_HDMI) ? p_src_param->src_bit_depth : 12;
        p_dm_cfg->srcSigEnv.Rng    = (p_src_param->src_yuv_range == SIG_RANGE_SMPTE) ? CRngNarrow : CRngFull;
        p_dm_cfg->srcSigEnv.RgbDef      = CRgbDefR709;
        p_dm_cfg->srcSigEnv.YuvXferSpec = CYuvXferSpecR709;
        p_dm_cfg->srcSigEnv.Gamma  = SDR_DEFAULT_GAMMA;
        p_dm_cfg->srcSigEnv.Min    = SDR_DEFAULT_MIN_LUM;
        p_dm_cfg->srcSigEnv.Max    = SDR_DEFAULT_MAX_LUM;
        p_dm_cfg->srcSigEnv.MinPq  = LToPQ12(p_dm_cfg->srcSigEnv.Min);
        p_dm_cfg->srcSigEnv.MaxPq  = LToPQ12(p_dm_cfg->srcSigEnv.Max);
        p_dm_cfg->srcSigEnv.A      =     385;
        p_dm_cfg->srcSigEnv.B      =    1075;
        p_dm_cfg->srcSigEnv.G      = 1383604;
    }

# if EN_AOI
        p_dm_cfg->dmCtrl.AoiRow0      = CLAMPS(p_dm_cfg->dmCtrl.AoiRow0,      0, p_dm_cfg->srcSigEnv.RowNum-1);
        p_dm_cfg->dmCtrl.AoiRow1Plus1 = CLAMPS(p_dm_cfg->dmCtrl.AoiRow1Plus1, 1, p_dm_cfg->srcSigEnv.RowNum  );
        p_dm_cfg->dmCtrl.AoiCol0      = CLAMPS(p_dm_cfg->dmCtrl.AoiCol0,      0, p_dm_cfg->srcSigEnv.ColNum-1);
        p_dm_cfg->dmCtrl.AoiCol1Plus1 = CLAMPS(p_dm_cfg->dmCtrl.AoiCol1Plus1, 1, p_dm_cfg->srcSigEnv.ColNum  );
#endif
    return 0;
}

static int commit_target_config(cp_context_t* p_ctx, pq_config_t* pq_config, int bl_scaler) {

    int ret = 0;
    int i,j;
    TargetDisplayConfig_t*  td_config = &pq_config->target_display_config;
    DmCfgFxp_t   *p_dm_cfg = &p_ctx->dm_cfg;
    #if DM_VER_LOWER_THAN212
    const uint16_t *lut_p;
    #endif

    p_ctx->dbgExecParamsPrintPeriod = td_config->dbgExecParamsPrintPeriod;

    /******************************/
    /* Target Config              */
    /******************************/
    p_dm_cfg->tgtSigEnv.Bdp    = td_config->bitDepth;
    p_dm_cfg->tgtSigEnv.Eotf   = td_config->eotf;

    p_dm_cfg->tgtSigEnv.YuvXferSpec = CYuvXferSpecR709;
    p_dm_cfg->tgtSigEnv.Rng         = td_config->rangeSpec;
    p_dm_cfg->tgtSigEnv.MinPq       = td_config->minPq;
    p_dm_cfg->tgtSigEnv.MaxPq       = td_config->maxPq;
    p_dm_cfg->tgtSigEnv.Min         = td_config->min_lin;
    p_dm_cfg->tgtSigEnv.Max         = td_config->max_lin;
    p_dm_cfg->tgtSigEnv.Gamma       = td_config->gamma;
    p_dm_cfg->tgtSigEnv.DiagSize    = td_config->diagSize;
    p_dm_cfg->tgtSigEnv.CrossTalk   = td_config->crossTalk;

    /* White point */
    p_dm_cfg->tgtSigEnv.WpExt = 1; /* 1 for given externally */
    for (i = 0; i < 3; i++)
        p_dm_cfg->tgtSigEnv.V3Wp[i] = td_config->ocscConfig.whitePoint[i];
    p_dm_cfg->tgtSigEnv.WpScale = td_config->ocscConfig.whitePointScale;

    /* RGB to LMS */
    p_dm_cfg->tgtSigEnv.Rgb2YuvExt = 1;  /* 1 for given externally */
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            p_dm_cfg->tgtSigEnv.M33Rgb2Yuv[i][j] = td_config->gdConfig.gdM33Rgb2Yuv[i][j];
        }
    p_dm_cfg->tgtSigEnv.M33Rgb2YuvScale2P = td_config->gdConfig.gdM33Rgb2YuvScale2P;
    p_dm_cfg->tgtSigEnv.Rgb2YuvOffExt = 1;  /* 1 for given externally */
    for (i = 0; i < 3; i++)
        p_dm_cfg->tgtSigEnv.V3Rgb2YuvOff[i] =  td_config->gdConfig.gdV3Rgb2YuvOff[i];


    /* LMS to RGB */
    p_dm_cfg->tgtSigEnv.Lms2RgbM33Ext = 1; /* 1 for given externally */
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            p_dm_cfg->tgtSigEnv.M33Lms2Rgb[i][j] = td_config->ocscConfig.lms2RgbMat[i][j];
        }
    p_dm_cfg->tgtSigEnv.M33Lms2RgbScale2P = td_config->ocscConfig.lms2RgbMatScale;

    /* initialize tone mapping control parameters */
    p_dm_cfg->tmCtrl.TMidBias    = td_config->midPQBias;
    p_dm_cfg->tmCtrl.TMaxBias    = td_config->maxPQBias;
    p_dm_cfg->tmCtrl.TMinBias    = td_config->minPQBias;
    p_dm_cfg->tmCtrl.DBrightness = td_config->brightness;
    p_dm_cfg->tmCtrl.DContrast   = td_config->contrast;
    p_dm_cfg->tmCtrl.ChrmVectorWeight        = td_config->chromaVectorWeight;
    p_dm_cfg->tmCtrl.IntensityVectorWeight   = td_config->intensityVectorWeight;
    p_dm_cfg->tmCtrl.KeyWeight               = td_config->keyWeight;
    p_dm_cfg->tmCtrl.BpWeight                = td_config->brightnessPreservation;
    # if DM_VER_LOWER_THAN212
    p_dm_cfg->tmCtrl.HGain    = td_config->gain;
    p_dm_cfg->tmCtrl.HOffset  = td_config->offset;
    # endif

    /* default level2 */
    /* DM internal definition of these three values is different than cfg file, we need to adjust these values with the coding bias */
    p_dm_cfg->tmCtrl.Default2[TrimTypeChromaWeight]  = td_config->chromaWeight   + p_dm_cfg->tmCtrl.CodeBias2[TrimTypeChromaWeight];
    p_dm_cfg->tmCtrl.Default2[TrimTypeSatGain]       = td_config->saturationGain + p_dm_cfg->tmCtrl.CodeBias2[TrimTypeSatGain];
    p_dm_cfg->tmCtrl.Default2[TrimTypeMsWeight]      = td_config->mSWeight       + p_dm_cfg->tmCtrl.CodeBias2[TrimTypeMsWeight];

    p_dm_cfg->tmCtrl.ValueAdj2[TrimTypeSlope]         = td_config->trimSlopeBias;
    p_dm_cfg->tmCtrl.ValueAdj2[TrimTypeOffset]        = td_config->trimOffsetBias;
    p_dm_cfg->tmCtrl.ValueAdj2[TrimTypePower]         = td_config->trimPowerBias;
    p_dm_cfg->tmCtrl.ValueAdj2[TrimTypeChromaWeight]  = td_config->chromaWeightBias;
    p_dm_cfg->tmCtrl.ValueAdj2[TrimTypeSatGain]       = td_config->saturationGainBias;
    p_dm_cfg->tmCtrl.ValueAdj2[TrimTypeMsWeight]      = td_config->msWeightBias;


    #if DM_VER_LOWER_THAN212
    ParseGmLutHdr(pq_config->default_gm_lut, GM_LUT_HDR_SIZE, &p_ctx->gm_lut);
    lut_p = (const unsigned short *)(pq_config->default_gm_lut+GM_LUT_HDR_SIZE);
    #if ALIGN_LUTS
    if (((uintptr_t)lut_p & 0x1) == 1) { /* if the LUT is at a odd address copy it to an even address */
        memcpy(p_ctx->tmp_lut, pq_config->default_gm_lut+GM_LUT_HDR_SIZE, GM_LUT_SIZE);
        lut_p = p_ctx->tmp_lut;
    }
    #endif
    ParseGmLutMap(lut_p, p_ctx->gm_lut.LutMap, 1, &p_ctx->gm_lut);
    p_dm_cfg->hGmLut   = &p_ctx->gm_lut;

    /* GmLutA is used for runmode 1 / absolute */
    ParseGmLutHdr(pq_config->gd_gm_lut_min, GM_LUT_HDR_SIZE, &p_ctx->gm_lut_mode1);
    lut_p = (const unsigned short *)(pq_config->gd_gm_lut_min+GM_LUT_HDR_SIZE);
    #if ALIGN_LUTS
    if (((uintptr_t)lut_p & 0x1) == 1) { /* if the LUT is at a odd address copy it to an even address */
        memcpy(p_ctx->tmp_lut, pq_config->gd_gm_lut_min+GM_LUT_HDR_SIZE, GM_LUT_SIZE);
        lut_p = p_ctx->tmp_lut;
    }
    #endif
    ParseGmLutMap(lut_p, p_ctx->gm_lut_mode1.LutMap, 0, &p_ctx->gm_lut_mode1);
    p_dm_cfg->hGmLutA  = &p_ctx->gm_lut_mode1;

    /* GmLutB is used for runmode 2 / relativ */
    ParseGmLutHdr(pq_config->gd_gm_lut_max, GM_LUT_HDR_SIZE, &p_ctx->gm_lut_mode2);
    lut_p = (const unsigned short *)(pq_config->gd_gm_lut_max+GM_LUT_HDR_SIZE);
    #if ALIGN_LUTS
    if (((uintptr_t)lut_p & 0x1) == 1) { /* if the LUT is at a odd address copy it to an even address */
        memcpy(p_ctx->tmp_lut, pq_config->gd_gm_lut_max+GM_LUT_HDR_SIZE, GM_LUT_SIZE);
        lut_p = p_ctx->tmp_lut;
    }
    #endif
    ParseGmLutMap(lut_p, p_ctx->gm_lut_mode2.LutMap, 0, &p_ctx->gm_lut_mode2);
    p_dm_cfg->hGmLutB  = &p_ctx->gm_lut_mode2;
    #endif
    #if EN_GLOBAL_DIMMING
    if (!(td_config->tuningMode & TUNINGMODE_EXTLEVEL4_DISABLE)) {
        p_dm_cfg->gdCtrl.GdOn            = td_config->gdConfig.gdEnable;
        p_dm_cfg->gdCtrl.GdWMin          = td_config->gdConfig.gdWMin;
        p_dm_cfg->gdCtrl.GdWMax          = td_config->gdConfig.gdWMax;
        p_dm_cfg->gdCtrl.GdWMm           = td_config->gdConfig.gdWMm;
        p_dm_cfg->gdCtrl.GdWMinPq        = LToPQ12(p_dm_cfg->gdCtrl.GdWMin);
        p_dm_cfg->gdCtrl.GdWMaxPq        = LToPQ12(p_dm_cfg->gdCtrl.GdWMax);
        p_dm_cfg->gdCtrl.GdWMmPq         = LToPQ12(p_dm_cfg->gdCtrl.GdWMm);
        p_dm_cfg->gdCtrl.GdWDynRngSqrt   = td_config->gdConfig.gdWDynRngSqrt;
        p_dm_cfg->gdCtrl.GdWeightMean    = td_config->gdConfig.gdWeightMean;
        p_dm_cfg->gdCtrl.GdWeightStd     = td_config->gdConfig.gdWeightStd ;
        p_dm_cfg->gdCtrl.GdUdPerFrmsTh = td_config->gdConfig.gdTriggerPeriod;
        p_dm_cfg->gdCtrl.GdUdDltTMaxTh = td_config->gdConfig.gdTriggerLinThresh;
        p_dm_cfg->gdCtrl.GdRiseWeight    = CLAMPS((td_config->gdConfig.gdRiseWeight * bl_scaler) >> BL_SCALER_SCALE, 0, MAX_GD_RISE_FALLWEIGHT);
        p_dm_cfg->gdCtrl.GdFallWeight    = CLAMPS((td_config->gdConfig.gdFallWeight * bl_scaler) >> BL_SCALER_SCALE, 0, MAX_GD_RISE_FALLWEIGHT);
    }
    else {
        p_dm_cfg->gdCtrl.GdOn = 0;
    }
    #else
    (void) bl_scaler; /* avoid compiler warning */
    #endif
    ret = CommitDmCfg(p_dm_cfg, p_ctx->h_ks, p_ctx->h_dm);

    return ret;

}

int destroy_cp(h_cp_context_t h_ctx) {
    /* Nothing to do here currently. We keep it for future use. */
    (void) h_ctx;
    return 0;
}


static int32_t dovi_handle_ui_menu(
        TargetDisplayConfig_t   *pTargetDisplayConfig,
        uint16_t                  u16BackLightUIVal,
        uint16_t                  u16BrightnessUIVal,
        uint16_t                  u16ContrastUIVal)
{


    int16_t x_x1,x1,y2,y1;
    int32_t temp;
    (void)u16BrightnessUIVal;
    (void)u16ContrastUIVal;

    u16BackLightUIVal = CLAMPS(u16BackLightUIVal,1,100);
    u16BackLightUIVal +=2;

    x1   = u16BackLightUIVal>>LOG2_UI_STEPSIZE;
    x_x1 = u16BackLightUIVal - (x1<<LOG2_UI_STEPSIZE);

    y1 = pTargetDisplayConfig->midPQBiasLut[x1];
    y2 = pTargetDisplayConfig->midPQBiasLut[x1+1];
    y2 = y2-y1 ;
    /* X2-X1 = 8; */
    temp = (y2 * x_x1) >>LOG2_UI_STEPSIZE   ;/*(y2-y1)/(x2-x1) * (x-x1) */
    pTargetDisplayConfig->midPQBias = (int16_t)(temp + y1);


    #if 0
    y1 = pTargetDisplayConfig->chromaWeightBiasLut[x1];
    y2 = pTargetDisplayConfig->chromaWeightBiasLut[x1+1];
    y2 = y2-y1 ;
    temp = (y2 * x_x1) >>LOG2_UI_STEPSIZE   ;/* (y2-y1)/(x2-x1) * (x-x1) */
    pTargetDisplayConfig->chromaWeightBias =(int16_t)(temp + y1);

    y1 = pTargetDisplayConfig->saturationGainBiasLut[x1];
    y2 = pTargetDisplayConfig->saturationGainBiasLut[x1+1];
    y2 = y2-y1 ;
    temp = (y2 * x_x1) >>LOG2_UI_STEPSIZE   ;/* (y2-y1)/(x2-x1) * (x-x1) */
    pTargetDisplayConfig->saturationGainBias =(int16_t)(temp + y1);
    #endif
    y1 = pTargetDisplayConfig->slopeBiasLut[x1];
    y2 = pTargetDisplayConfig->slopeBiasLut[x1+1];
    y2 = y2-y1 ;
    temp = (y2 * x_x1) >>LOG2_UI_STEPSIZE   ;/* (y2-y1)/(x2-x1) * (x-x1) */
    pTargetDisplayConfig->trimSlopeBias =(int16_t)(temp + y1);
    #if 0
    y1 = pTargetDisplayConfig->offsetBiasLut[x1];
    y2 = pTargetDisplayConfig->offsetBiasLut[x1+1];
    y2 = y2-y1 ;
    temp = (y2 * x_x1) >>LOG2_UI_STEPSIZE   ;/* (y2-y1)/(x2-x1) * (x-x1) */
    pTargetDisplayConfig->trimOffsetBias =(int16_t)(temp + y1);
    #endif

    /* backlight scaler, default is 0 + 4096 */
    y1 = pTargetDisplayConfig->backlightBiasLut[x1];
    y2 = pTargetDisplayConfig->backlightBiasLut[x1+1];
    y2 = y2-y1 ;
    temp = (y2 * x_x1) >>LOG2_UI_STEPSIZE   ;/* (y2-y1)/(x2-x1) * (x-x1) */
    pTargetDisplayConfig->backlight_scaler =(int16_t)(temp + y1 + 4096);

    return 0;
}


void init_cp_mmg(cp_mmg_t * p_cp_mmg) {
    Mmg_t dm_mmg;

    p_cp_mmg->cp_ctx_size = sizeof(cp_context_t);


    #if DM_VER_LOWER_THAN212
    p_cp_mmg->lut_mem_size = 3* GM_LUT_SIZE * sizeof(unsigned short);
    #else
    p_cp_mmg->lut_mem_size = 0;
    #endif

    InitMmg(&dm_mmg);

    p_cp_mmg->dm_ctx_size = dm_mmg.dmCtxtSize + dm_mmg.mdsExtSize + dm_mmg.dmKsSize;
}

void (*cbFxHandle)(uint8_t backlight_pwm_val , uint32_t delay_in_millisec , void *p_rsrvd);

void register_dovi_callback(
                                    void (*call_back_hadler)(uint8_t backlight_pwm_val , uint32_t delay_in_millisec , void *p_rsrvd)
                                       )
{
    cbFxHandle = call_back_hadler;
}

#ifdef RTK_EDBEC_1_5
static hdr10_param_t hdr10_param;
int rtk_dm_exec_params_to_DmExecFxp(DmKsFxp_t *pKs,
	DmExecFxp_t *pDmExecFxp_t,
	DmExecFxp_t_rtk *pDmExecFxp_t_rtk,
	MdsExt_t *p_MdsExt_t,
	dm_lut_t *p_dm_lut,
	pq_config_t* pq_config,
	input_mode_t mode)
{
	int i,j;
	pDmExecFxp_t_rtk->runMode           = mode;
	pDmExecFxp_t_rtk->colNum            = pKs->ksFrmFmtI.colNum;
	pDmExecFxp_t_rtk->rowNum            = pKs->ksFrmFmtI.rowNum;
	pDmExecFxp_t_rtk->in_clr            = pKs->ksFrmFmtI.clr;
	pDmExecFxp_t_rtk->in_bdp            = pKs->ksFrmFmtI.bdp;
	pDmExecFxp_t_rtk->in_chrm           = pKs->ksFrmFmtI.chrm;

	//pDmExecFxp_t->runMode           = mode;
	//pDmExecFxp_t->colNum            = pKs->ksFrmFmtI.colNum;
	//pDmExecFxp_t->rowNum            = pKs->ksFrmFmtI.rowNum;
	//pDmExecFxp_t->in_clr            = pKs->ksFrmFmtI.clr;
	//pDmExecFxp_t->in_bdp            = pKs->ksFrmFmtI.bdp;
	//pDmExecFxp_t->in_chrm           = pKs->ksFrmFmtI.chrm;

	pDmExecFxp_t->sRange            = pKs->ksIMap.eotfParam.range;
	pDmExecFxp_t->sRangeMin         = pKs->ksIMap.eotfParam.rangeMin;
	pDmExecFxp_t->sRangeInv         = pKs->ksIMap.eotfParam.rangeInv;
	pDmExecFxp_t->sEotf             = pKs->ksIMap.eotfParam.eotf;
	pDmExecFxp_t->m33Yuv2RgbScale2P = pKs->ksIMap.m33Yuv2RgbScale2P;

	pDmExecFxp_t_rtk->sRange            = pKs->ksIMap.eotfParam.range;
	pDmExecFxp_t_rtk->sRangeMin         = pKs->ksIMap.eotfParam.rangeMin;
	pDmExecFxp_t_rtk->sRangeInv         = pKs->ksIMap.eotfParam.rangeInv;
	pDmExecFxp_t_rtk->m33Yuv2RgbScale2P = pKs->ksIMap.m33Yuv2RgbScale2P;




	pDmExecFxp_t->tcLutMaxVal = pKs->ksTMap.tmLutMaxVal;
	pDmExecFxp_t_rtk->tcLutMaxVal = pKs->ksTMap.tmLutMaxVal;
	for(i=0;i<3;i++) {
		for(j=0;j<3;j++) {
			pDmExecFxp_t->m33Yuv2Rgb[i][j] = pKs->ksIMap.m33Yuv2Rgb[i][j];
			pDmExecFxp_t_rtk->m33Yuv2Rgb[i][j] = pKs->ksIMap.m33Yuv2Rgb[i][j];
		}
	}

	for(i=0;i<3;i++) {
		pDmExecFxp_t->v3Yuv2RgbOffInRgb[i] = pKs->ksIMap.v3Yuv2RgbOffInRgb[i];
		pDmExecFxp_t_rtk->v3Yuv2RgbOffInRgb[i] = pKs->ksIMap.v3Yuv2RgbOffInRgb[i];
	}


	pDmExecFxp_t_rtk->sEotf             = pKs->ksIMap.eotfParam.eotf;

	pDmExecFxp_t->m33Rgb2OptScale2P = pKs->ksIMap.m33Rgb2LmsScale2P;
	pDmExecFxp_t_rtk->m33Rgb2OptScale2P = pKs->ksIMap.m33Rgb2LmsScale2P;

	for(i=0;i<3;i++) {
		for(j=0;j<3;j++) {
			pDmExecFxp_t->m33Rgb2Opt[i][j] = pKs->ksIMap.m33Rgb2Lms[i][j];
			pDmExecFxp_t_rtk->m33Rgb2Opt[i][j] = pKs->ksIMap.m33Rgb2Lms[i][j];
		}
	}

	pDmExecFxp_t->m33Opt2OptScale2P = pKs->ksIMap.m33Lms2IptScale2P;
	pDmExecFxp_t_rtk->m33Opt2OptScale2P = pKs->ksIMap.m33Lms2IptScale2P;

	for(i=0;i<3;i++) {
		for(j=0;j<3;j++) {
			pDmExecFxp_t->m33Opt2Opt[i][j] = pKs->ksIMap.m33Lms2Ipt[i][j];
			pDmExecFxp_t_rtk->m33Opt2Opt[i][j] = pKs->ksIMap.m33Lms2Ipt[i][j];
		}
	}


	pDmExecFxp_t->Opt2OptOffset = 0;
	pDmExecFxp_t->chromaWeight = pKs->ksTMap.chromaWeight;

	pDmExecFxp_t->msWeight = pKs->ksMs.msWeight;
	pDmExecFxp_t->msWeightEdge = pKs->ksMs.msEdgeWeight;

	pDmExecFxp_t->gain = pKs->ksOMap.gain;
	pDmExecFxp_t->offset = pKs->ksOMap.offset;
	pDmExecFxp_t->saturationGain = pKs->ksOMap.satGain;

	pDmExecFxp_t->tRangeMin = pKs->ksOMap.tRangeMin;
	pDmExecFxp_t->tRange = pKs->ksOMap.tRange;
	pDmExecFxp_t->tRangeOverOne = pKs->ksOMap.tRangeOverOne;
	pDmExecFxp_t->tRangeInv = pKs->ksOMap.tRangeInv;

	pDmExecFxp_t_rtk->Opt2OptOffset = 0;
	pDmExecFxp_t_rtk->chromaWeight = pKs->ksTMap.chromaWeight;

	pDmExecFxp_t_rtk->msWeight = pKs->ksMs.msWeight;
	pDmExecFxp_t_rtk->msWeightEdge = pKs->ksMs.msEdgeWeight;

	pDmExecFxp_t_rtk->gain = pKs->ksOMap.gain;
	pDmExecFxp_t_rtk->offset = pKs->ksOMap.offset;
	pDmExecFxp_t_rtk->saturationGain = pKs->ksOMap.satGain;
	pDmExecFxp_t_rtk->tRangeMin = pKs->ksOMap.tRangeMin;
	pDmExecFxp_t_rtk->tRange = pKs->ksOMap.tRange;
	pDmExecFxp_t_rtk->tRangeOverOne = pKs->ksOMap.tRangeOverOne;
	pDmExecFxp_t_rtk->tRangeInv = pKs->ksOMap.tRangeInv;

	pDmExecFxp_t_rtk->msFilterScale = pKs->ksMs.fltrScale;


#if 1
	//all the these are constant..need to copy
	memcpy(&pDmExecFxp_t_rtk->msFilterRow,msFilterRow,11*sizeof(int16_t));
	memcpy(&pDmExecFxp_t_rtk->msFilterCol,msFilterCol,5*sizeof(int16_t));
	memcpy(&pDmExecFxp_t_rtk->msFilterEdgeRow,msFilterEdgeRow,11*sizeof(int16_t));
	memcpy(&pDmExecFxp_t_rtk->msFilterEdgeCol,msFilterEdgeCol,5*sizeof(int16_t));
#endif

	//memcpy(&pDmExecFxp_t->msFilterRow,msFilterRow,11*sizeof(int16_t));
	//memcpy(&pDmExecFxp_t->msFilterCol,msFilterCol,5*sizeof(int16_t));
	//memcpy(&pDmExecFxp_t->msFilterEdgeRow,msFilterEdgeRow,11*sizeof(int16_t));
	//memcpy(&pDmExecFxp_t->msFilterEdgeCol,msFilterEdgeCol,5*sizeof(int16_t));

	//pDmExecFxp_t->tcLutArray //p_dm_exec_params_t has not this.

	pDmExecFxp_t->bwDmLut.iMaxC1= pKs->ksOMap.ksGmLut.iMaxC1;
	pDmExecFxp_t->bwDmLut.iMaxC2= pKs->ksOMap.ksGmLut.iMaxC2;
	pDmExecFxp_t->bwDmLut.iMaxC3= pKs->ksOMap.ksGmLut.iMaxC3;
	pDmExecFxp_t->bwDmLut.iMinC1= pKs->ksOMap.ksGmLut.iMinC1;
	pDmExecFxp_t->bwDmLut.iMinC2= pKs->ksOMap.ksGmLut.iMinC2;
	pDmExecFxp_t->bwDmLut.iMinC3= pKs->ksOMap.ksGmLut.iMinC3;

	pDmExecFxp_t->bwDmLut.iDistC1Inv = pKs->ksOMap.ksGmLut.iDistC1Inv;
	pDmExecFxp_t->bwDmLut.iDistC2Inv = pKs->ksOMap.ksGmLut.iDistC2Inv;
	pDmExecFxp_t->bwDmLut.iDistC3Inv = pKs->ksOMap.ksGmLut.iDistC3Inv;

	pDmExecFxp_t_rtk->bwDmLut.iMaxC1= pKs->ksOMap.ksGmLut.iMaxC1;
	pDmExecFxp_t_rtk->bwDmLut.iMaxC2= pKs->ksOMap.ksGmLut.iMaxC2;
	pDmExecFxp_t_rtk->bwDmLut.iMaxC3= pKs->ksOMap.ksGmLut.iMaxC3;
	pDmExecFxp_t_rtk->bwDmLut.iMinC1= pKs->ksOMap.ksGmLut.iMinC1;
	pDmExecFxp_t_rtk->bwDmLut.iMinC2= pKs->ksOMap.ksGmLut.iMinC2;
	pDmExecFxp_t_rtk->bwDmLut.iMinC3= pKs->ksOMap.ksGmLut.iMinC3;

	pDmExecFxp_t_rtk->bwDmLut.iDistC1Inv = pKs->ksOMap.ksGmLut.iDistC1Inv;
	pDmExecFxp_t_rtk->bwDmLut.iDistC2Inv = pKs->ksOMap.ksGmLut.iDistC2Inv;
	pDmExecFxp_t_rtk->bwDmLut.iDistC3Inv = pKs->ksOMap.ksGmLut.iDistC3Inv;



	memcpy(pDmExecFxp_t->g2L ,p_dm_lut->g2L,sizeof(p_dm_lut->g2L));
	memcpy(pDmExecFxp_t_rtk->g2L ,p_dm_lut->g2L,sizeof(p_dm_lut->g2L));

	memcpy(pDmExecFxp_t->tcLut ,p_dm_lut->tcLut,sizeof(p_dm_lut->tcLut));
	memcpy(pDmExecFxp_t_rtk->tcLut ,p_dm_lut->tcLut,sizeof(p_dm_lut->tcLut));

	return 0;

}

int rtk_wrapper_commit_reg(h_cp_context_t p_ctx,
	signal_format_t  input_format,
	dm_metadata_t *p_src_dm_metadata,
	rpu_ext_config_fixpt_main_t *p_src_comp_metadata,
	pq_config_t* pq_config,
	int view_mode_id,
	composer_register_t *p_dst_comp_reg,
	dm_register_t *p_dm_reg,
	dm_lut_t *p_dm_lut)
{
	int status=0;
	src_param_t src_param;
	ui_menu_params_t ui_menu_params;
	uint16_t backlight_val;

	/*baker, 5252 is 3718, others default is 4096*/
        int bl_scaler = 4096;
	/*baker, 5051 is Dolby_VSIF_LL_DoVi_GD*/
	/*baker, 5052,54,59 is Dolby_VSIF_LL_DoVi */
        //UINT8* vsif = &Dolby_VSIF_LL_DoVi[0];


	memset(&src_param,0x0,sizeof(src_param_t));
	memset(&ui_menu_params,0x0,sizeof(ui_menu_params_t));

	if(input_format == FORMAT_HDR10)
	{
		//we hack this for IDK-test 5230
		src_param.width              = 1920;
		src_param.height             = 1080;
		src_param.src_bit_depth      = 10;
		src_param.src_chroma_format  = 0;
		src_param.src_yuv_range      = SIG_RANGE_SMPTE;

		hdr10_param.min_display_mastering_luminance = 50;
		hdr10_param.max_display_mastering_luminance = 10000000;
		hdr10_param.max_content_light_level = 1000;
		hdr10_param.max_pic_average_light_level = 600;
	}
	else{
		src_param.width              = 0;
		src_param.height             = 0;
		src_param.src_bit_depth      = 0;
		src_param.src_chroma_format  = 0;
		src_param.src_yuv_range      = 0;
		//src_param.hdr10_param        = 0;
	}

	memcpy(&src_param.hdr10_param,&hdr10_param,sizeof(hdr10_param_t));



	src_param.width = p_dm_reg->colNum;
	src_param.height = p_dm_reg->rowNum;

	if(p_src_comp_metadata == NULL)
		src_param.input_mode               = INPUT_MODE_HDMI;
	else
		src_param.input_mode               = INPUT_MODE_OTT;

	ui_menu_params.u16BackLightUIVal  = 0;
	ui_menu_params.u16BrightnessUIVal = 0;
	ui_menu_params.u16ContrastUIVal   = 0;

	/* Generate the register values */
	status = commit_reg(p_ctx, input_format,
		p_src_dm_metadata,
		p_src_comp_metadata,
		pq_config, view_mode_id,
		&src_param,
		&ui_menu_params,
		//vsif,
		NULL,
		bl_scaler,
#if IPCORE
		p_dst_reg,
#else
		p_dst_comp_reg, p_dm_reg,
#endif
		p_dm_lut,
		&backlight_val);

	//if(status < 0 && status != CP_EL_DETECTED)
	if(status < 0 )
		printk(KERN_ERR"[Dolby][%s]ERROR, commit_reg status = %d\n",__func__, status);
	return 0;
}

//#ifdef RTK_IDK_CRT
int rtk_wrapper_commit_reg_vsif(h_cp_context_t p_ctx,
	signal_format_t  input_format,
	dm_metadata_t *p_src_dm_metadata,
	rpu_ext_config_fixpt_main_t *p_src_comp_metadata,
	pq_config_t* pq_config,
	int view_mode_id,
	composer_register_t *p_dst_comp_reg,
	dm_register_t *p_dm_reg,
	dm_lut_t *p_dm_lut)
{
	int status=0;
	src_param_t src_param;
	ui_menu_params_t ui_menu_params;
	uint16_t backlight_val;
        int bl_scaler = 4096;

	memset(&src_param,0x0,sizeof(src_param_t));
	memset(&ui_menu_params,0x0,sizeof(ui_menu_params_t));

	src_param.width = p_dm_reg->colNum;
	src_param.height = p_dm_reg->rowNum;

	src_param.input_mode               = INPUT_MODE_HDMI;

	ui_menu_params.u16BackLightUIVal  = 0;
	ui_menu_params.u16BrightnessUIVal = 0;
	ui_menu_params.u16ContrastUIVal   = 0;

	src_param.src_bit_depth      = 12;
	src_param.src_chroma_format  = 0;
	src_param.src_yuv_range      = 0;

	ui_menu_params.u16BackLightUIVal  = 50;
	ui_menu_params.u16BrightnessUIVal = 50;
	ui_menu_params.u16ContrastUIVal   = 50;

	//if input is RGB enable this
	//src_param.src_color_format  = 1;


	memcpy(&src_param.hdr10_param,&hdr10_param,sizeof(hdr10_param_t));

	/* Generate the register values */
	status = commit_reg(p_ctx, input_format,
		p_src_dm_metadata,
		p_src_comp_metadata,
		pq_config, view_mode_id,
		&src_param,
		&ui_menu_params,
		(unsigned char*)p_src_dm_metadata, //vsif
		bl_scaler,
#if IPCORE
		p_dst_reg,
#else
		p_dst_comp_reg, p_dm_reg,
#endif
		p_dm_lut,
		&backlight_val);

	//if(status < 0 && status != CP_EL_DETECTED)
	if(status < 0 )
		printk(KERN_ERR"[Dolby][%s]ERROR, commit_reg status = %d\n",__func__, status);
	return 0;
}
//endif
#endif

static int commit_reg_core(h_cp_context_t h_ctx,
               signal_format_t  input_format,
               dm_metadata_t *p_src_dm_metadata,
               pq_config_t* pq_config,
               int view_mode_id,
               src_param_t *p_src_param,
               ui_menu_params_t *ui_menu_params,
               uint8_t* vsif,
               int bl_scaler,
               uint16_t *backlight_return_val)
{
    cp_context_t* p_ctx = h_ctx;
    int cp_ret = 0;
    DmCfgFxp_t   *p_dm_cfg = &p_ctx->dm_cfg;
    TargetDisplayConfig_t*  td_config;
    int ui_params_changed = 0;
    int dm_ret = 0;
    int wMaxPQ;
    int ll_teff_max = 0, ll_bl_avail = 0, low_latency = 0;

    if ((input_format == FORMAT_DOVI) && (vsif != NULL)) {
        if ((cp_ret = parse_dolby_vsif(vsif, &low_latency, &ll_bl_avail, &ll_teff_max)) != 0)
            return cp_ret;
    }

    if (view_mode_id   != p_ctx->last_view_mode_id) {
        //p_ctx->cur_pq_config = &pq_config[view_mode_id + ((low_latency == 1)? 10:0)];
        p_ctx->cur_pq_config = pq_config;
    }
    td_config = &p_ctx->cur_pq_config->target_display_config;

    if (input_format   != p_ctx->last_input_format) {
        if ((cp_ret = setup_src_config(p_ctx, input_format, p_src_param, low_latency)) != 0)
            return cp_ret;
        p_ctx->last_input_format = input_format;
    }
    if ((ui_menu_params->u16BackLightUIVal  != p_ctx->last_ui_menu_params.u16BackLightUIVal ) ||
        (ui_menu_params->u16BrightnessUIVal != p_ctx->last_ui_menu_params.u16BrightnessUIVal) ||
        (ui_menu_params->u16ContrastUIVal   != p_ctx->last_ui_menu_params.u16ContrastUIVal  ))
    {
        dovi_handle_ui_menu(
            td_config,
            ui_menu_params->u16BackLightUIVal,
            ui_menu_params->u16BrightnessUIVal,
            ui_menu_params->u16ContrastUIVal);
        p_ctx->last_ui_menu_params = *ui_menu_params;
        ui_params_changed = 1;
    }

    if ((view_mode_id   != p_ctx->last_view_mode_id) ||
        (ui_params_changed) ||
        bl_scaler != p_ctx->last_bl_scaler)
    {
        dm_ret |= commit_target_config(p_ctx, p_ctx->cur_pq_config, bl_scaler);
        p_ctx->last_view_mode_id = view_mode_id;
        p_ctx->last_bl_scaler = bl_scaler;
    }


    /* Commit metadata if the input format is Dolby Vision */
    if ((input_format == FORMAT_DOVI) && (low_latency == 0)) {
        dm_metadata_2_dm_param(&p_src_dm_metadata->base, p_ctx->h_mds_ext, p_dm_cfg);
        dm_ret |= CommitMds(p_ctx->h_mds_ext, p_ctx->h_dm);
    } else {
        dm_ret |= CommitMds(NULL, p_ctx->h_dm);
    }

    /* Backlight control */
    /* set default maxPQ to target maxPQ, will be overwritten if GD is available from metadata or VSIF*/
    wMaxPQ = td_config->maxPq;
    if (input_format == FORMAT_DOVI) {
        if (low_latency == 0) {
            #if EN_GLOBAL_DIMMING
            if (p_ctx->h_mds_ext->lvl4GdAvail) {
                uint32_t bl;
                GetGdBackLightModelInput(&bl, p_ctx->h_dm);
                wMaxPQ = LToPQ12(bl);
            }
            #endif
        } else if (ll_bl_avail) {
            wMaxPQ = ll_teff_max;
        }
    }
	*backlight_return_val = (p_ctx->cur_pq_config->backlight_lut[wMaxPQ] * td_config->backlight_scaler)>>12;
    *backlight_return_val = CLAMPS(*backlight_return_val, 10, 255);


    /* Convert DM return value to 3 simpler return values */
    if (dm_ret < 0) {
        cp_ret = dm_ret;
    } else {
        if (p_src_param->input_mode == INPUT_MODE_OTT)  cp_ret |= CP_CHANGE_COMP_REG;
        if (dm_ret > 0)                                 cp_ret |= CP_CHANGE_DM_REG;
        if (dm_ret & FLAG_CHANGE_3DLUT)                 cp_ret |= CP_CHANGE_3DLUT;
        if (dm_ret & FLAG_CHANGE_TC)                    cp_ret |= CP_CHANGE_TC ;
    }


#if 0
    //baker , need register callback, IDK need this?

    cbFxHandle((uint8_t)*backlight_return_val ,
                     p_src_param->input_mode == INPUT_MODE_HDMI ?
                     p_ctx->cur_pq_config->target_display_config.gdConfig.gdDelayMilliSec_hdmi :
                     p_ctx->cur_pq_config->target_display_config.gdConfig.gdDelayMilliSec_ott ,
                     (void *)&p_ctx->cur_pq_config->target_display_config.max_lin);
#endif


    return cp_ret;
}

int commit_reg(h_cp_context_t h_ctx,
               signal_format_t  input_format,
               dm_metadata_t *p_src_dm_metadata,
               rpu_ext_config_fixpt_main_t *p_src_comp_metadata,
               pq_config_t* pq_config,
               int view_mode_id,
               src_param_t *p_src_param,
               ui_menu_params_t *ui_menu_params,
               uint8_t* vsif,
               int bl_scaler,
               #if IPCORE
               register_ipcore_t *p_dst_reg,
               #else
               composer_register_t *p_dst_comp_reg,
               dm_register_t *p_dst_dm_reg,
		#endif
               dm_lut_t *p_dm_lut,
               uint16_t *backlight_return_val)
{
    int ret;

#if !DM286_MODE
    cp_context_t* p_ctx = h_ctx;
    ret = commit_reg_core(h_ctx,
               input_format,
               p_src_dm_metadata,
               pq_config,
               view_mode_id,
               p_src_param,
               ui_menu_params,
               vsif,
               bl_scaler,
               backlight_return_val);

    /* Only generate composer registers if we are not in HDMI mode */
    if (p_src_param->input_mode == INPUT_MODE_OTT) {
        if ((input_format == FORMAT_SDR) || (input_format == FORMAT_HDR10)){
            fill_bypass_comp_cfg(&p_ctx->tmp_comp_metadata, p_src_param->src_bit_depth);
            comp_cfg_2_comp_reg(&p_ctx->tmp_comp_metadata,
            #if IPCORE
            p_dst_reg,
            #else
            p_dst_comp_reg,
            #endif
            p_ctx->cur_pq_config->target_display_config.tuningMode&TUNINGMODE_EL_FORCEDDISABLE, p_src_param->width, p_src_param->height, p_ctx);
        } else {
            if (comp_cfg_2_comp_reg(p_src_comp_metadata,
            #if IPCORE
            p_dst_reg,
            #else
            p_dst_comp_reg,
            #endif
                p_ctx->cur_pq_config->target_display_config.tuningMode&TUNINGMODE_EL_FORCEDDISABLE,
                p_src_param->width, p_src_param->height, p_ctx) == CP_EL_DETECTED) {
            return CP_EL_DETECTED;
	    }
        }
    }

#ifndef RTK_EDBEC_CONTROL
    /* Convert the DM kernel structure to DM registers.
       These functions may need to be updated by the customer */
    dmKs2dmreg(p_ctx->h_ks,
               #if IPCORE
               p_dst_reg,
               #else
               p_dst_dm_reg,
               #endif
               p_ctx, p_src_param);

#endif
    dmKs2dmlut(p_ctx->h_ks, p_dm_lut);

#else
    (void)h_ctx;
    (void)input_format;
    (void)p_src_dm_metadata;
//    (void)p_src_comp_metadata;
    (void)pq_config;
    (void)view_mode_id;
    (void)p_src_param;
    (void)ui_menu_params;
    (void)vsif;
    (void)bl_scaler;
    (void)p_dst_comp_reg;
    (void)p_dst_dm_reg;
    (void)p_dm_lut;
    (void)backlight_return_val;
    ret= CP_ERR_286_MODE;
#endif
    return ret;
}

#if !IPCORE
int commit_reg_286(h_cp_context_t h_ctx,
               signal_format_t  input_format,
               dm_metadata_t *p_src_dm_metadata,
               rpu_ext_config_fixpt_main_t *p_src_comp_metadata,
               pq_config_t* pq_config,
               int view_mode_id,
               src_param_t *p_src_param,
               ui_menu_params_t *ui_menu_params,
               uint8_t* vsif,
               int bl_scaler,
               composer_register_t *p_dst_comp_reg,
               dm_register_286_t *p_dst_dm_reg,
               dm_lut_286_t *p_dm_lut,
               uint16_t *backlight_return_val)
{
    int ret;
#if DM286_MODE
    cp_context_t* p_ctx = h_ctx;
    ret = commit_reg_core(h_ctx,
               input_format,
               p_src_dm_metadata,
               pq_config,
               view_mode_id,
               p_src_param,
               ui_menu_params,
               vsif,
               bl_scaler,
               backlight_return_val);

    /* Only generate composer registers if we are not in HDMI mode */
    if (p_src_param->input_mode == INPUT_MODE_OTT) {
        if ((input_format == FORMAT_SDR) || (input_format == FORMAT_HDR10)){
            fill_bypass_comp_cfg(&p_ctx->tmp_comp_metadata, p_src_param->src_bit_depth);
            comp_cfg_2_comp_reg(&p_ctx->tmp_comp_metadata,
            p_dst_comp_reg,
            p_ctx->cur_pq_config->target_display_config.tuningMode&TUNINGMODE_EL_FORCEDDISABLE, p_src_param->width, p_src_param->height, p_ctx);
    } else {
            if (comp_cfg_2_comp_reg(p_src_comp_metadata,
            p_dst_comp_reg,
            p_ctx->cur_pq_config->target_display_config.tuningMode&TUNINGMODE_EL_FORCEDDISABLE,
            p_src_param->width, p_src_param->height, p_ctx) == CP_EL_DETECTED)
            return CP_EL_DETECTED;
        }
    }

    /* Convert the DM kernel structure to DM registers.
       These functions may need to be updated by the customer */
    dmKs2dmreg(p_ctx->h_ks,
               p_dst_dm_reg,
               p_ctx, p_src_param);
    dmKs2dmlut(p_ctx->h_ks, p_dm_lut);

#else
    (void)h_ctx;
    (void)input_format;
    (void)p_src_dm_metadata;
    //(void)p_src_comp_metadata;
    (void)pq_config;
    (void)view_mode_id;
    (void)p_src_param;
    (void)ui_menu_params;
    (void)vsif;
    (void)bl_scaler;
    (void)p_dst_comp_reg;
    (void)p_dst_dm_reg;
    (void)p_dm_lut;
    (void)backlight_return_val;
    ret= CP_ERR_NO286_MODE;
#endif
    return ret;

}
#endif

static char  str_algorithm_ver[20] = "\0";
static char  str_data_path_ver[20] = "\0";

char *cp_get_dm_data_path_ver(void)
{
    if (DM286_MODE == 1)
        sprintf(str_data_path_ver, "2.8.6");
    else
        sprintf(str_data_path_ver, "2.11.0");

    return str_data_path_ver;
}

char *cp_get_dm_algorithm_ver(void)
{
    if (DM_VER_CTRL_MINOR == 110)
        sprintf(str_algorithm_ver, "2.11.0");
    else
        sprintf(str_algorithm_ver, "3.1.0");

    return str_algorithm_ver;
}
