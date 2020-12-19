/****************************************************************************
* This product contains one or more programs protected under international
* and U.S. copyright laws as unpublished works.  They are confidential and
* proprietary to Dolby Laboratories.  Their reproduction or disclosure, in
* whole or in part, or the production of derivative works therefrom without
* the express permission of Dolby Laboratories is prohibited.
*
*             Copyright 2011 - 2013 by Dolby Laboratories.
* 			              All rights reserved.
****************************************************************************/

/*! Copyright &copy; 2013 Dolby Laboratories, Inc.
    All Rights Reserved

    @file  dovi_bec_hal_api.h
    @brief Dolby Vision backend control software HAL definition file.

    This header file specifies the interface for the hardware abstraction
    layer of the Dolby Vision backend control software.

    @note This is only a suggestion for the HW I/F. It's subject to change
          as desired by the implementer.
*/

#ifndef __DOVI_BEC_HAL_API_H__
#define __DOVI_BEC_HAL_API_H__

//RTK MARK
#ifdef RTK_EDBEC_CONTROL
#include <linux/types.h>
#include "typedefs.h"
#else
#include <stdint.h>
#endif
#include "dovi_bec_osal_api.h"
#include "rpu_ext_config.h"
#include "VdrDmApi.h"


/*! @defgroup general Enumerations and data structures
 *
 * @{
 */
/* Composer HW block I/F */

#define COMPOSER_INTR_LEVEL             1

#define FULL_COMPOSE_MODE               0
#define EL_FREE_COMPOSE_MODE            1

#define COMPOSER_HW_BASE_PROFILE        0
#define COMPOSER_HW_MAIN_PROFILE        1


/* Composer command register definition, WR_ONLY register */
typedef union _composer_cmd_reg_
{
    uint32_t value;
    struct 
    {
        uint32_t reserved     : 28;
        uint32_t compose_mode : 1;         /* 0 = FULL mode, 1 = EL FREE mode */
        uint32_t start        : 1;                               /* 1 = start */
        uint32_t stop         : 1;                                /* 1 = stop */
        uint32_t reset        : 1;                  /* 1 = reset the composer */
    } fields;
} composer_cmd_reg;

/* Composer status register definition - RD_ONLY register */
typedef union _composer_status_reg_
{
    uint32_t value;
    struct
    {
        uint32_t reserved    : 30;
        uint32_t intr_enable : 1;        /* 1 = INTR ENABLE, 0 = INTR DISABLE */
        uint32_t busy        : 1;                       /* 1 = BUSY, 0 = IDLE */
    } fields;
} composer_status_reg;

/* There shall be an interrupt event indicating the composing of the
   current frame is completed.
*/

/*!
 * @}
 */

/*! @defgroup functions API functions
 *
 * @{
 */

/* Get 90 KHz System Clock */
uint64_t GetStc90KHzClk(void);


/* Composer HAL control APIs */
void    hal_composer_start(uint8_t mode);
void    hal_composer_stop(void);
void    hal_composer_reset(void);
void    hal_composer_intr_enable(void);
void    hal_composer_intr_disable(void);
void    hal_composer_set_pts(uint64_t  pts);
void    hal_composer_set_bl_buffer(intptr_t  bl_buffer_addr);
void    hal_composer_set_el_buffer(intptr_t  el_buffer_addr);
bool_t  hal_vdr_hw_busy(void);
uint8_t hal_composer_hw_profile_read(void);
void    hal_composer_ld_main_profile_param(rpu_ext_config_fixpt_main_t *p_param);


/* Display Management HAL control APIs */
void    hal_dm_exec_ld_param(HDmKsFxp_t h_dm_ks);
void    hal_dm_start(void);

/* EDR-over-HDMI HAL control APIs */
bool_t  hal_eoh_edr_capable_display(void);

/*!
 * @}
 */

/* Dithering HAL control APIs */
void    hal_dither_enable(void);
void    hal_dither_disable(void);


#endif     /* __DOVI_BEC_HAL_API_H__ */
