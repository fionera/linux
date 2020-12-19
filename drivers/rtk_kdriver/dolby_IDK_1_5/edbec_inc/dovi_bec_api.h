/****************************************************************************
* This product contains one or more programs protected under international
* and U.S. copyright laws as unpublished works.  They are confidential and
* proprietary to Dolby Laboratories.  Their reproduction or disclosure, in
* whole or in part, or the production of derivative works therefrom without
* the express permission of Dolby Laboratories is prohibited.
*
*             Copyright 2011 - 2013 by Dolby Laboratories.
*                         All rights reserved.
****************************************************************************/


/*!
    @mainpage Dolby Vision decoder backend control library

    EDR Decoder Backend Control (EDBEC) Software Module Library

    The back end control module interfaces with the Dovi Layer Decoders, the Composer
    hardware, and the Display Management (DM) module to control the data flow
    from the Dovi Layer Decoders' output thru the Composer HW to the
    Display Management (DM) module output.

    @file  dovi_bec_api.h
    @brief Dolby Vision decoder backend control software module interface.

    This header file defines the interface to the Dolby Vision decoder backend
    control software module.
*/

#ifndef __DOVI_BEC_API_H__
#define __DOVI_BEC_API_H__
//RTK MARK
#ifdef RTK_EDBEC_CONTROL
#else
#include "stddef.h"
#endif
#include "dovi_bec_hal_api.h"

/*! @defgroup general Enumerations and data structures
 *
 * @{
 */

/* Define this flag for Dolby Vision TV implementation
   with an internal 10 bits video pipeline */
#undef VBUS_10_BITS


typedef enum
{
    FULL_COMPOSING_MODE,
    EL_FREE_COMPOSING_MODE,
    COMPOSING_MODE_MAX
} dovi_bec_compose_mode_t;

typedef enum
{
    DOVI_BEC_HDMI_SRC_CTL_PARAM,
    DOVI_BEC_COMPOSING_MODE_PARAM,
    DOVI_BEC_FRAME_RATE_PARAM,
    DOVI_BEC_USER_MS_WEIGHT_PARAM,
    DOVI_BEC_PARAM_MAX
} dovi_bec_param_id_t;

/*!
 * @}
 */

/*! @defgroup functions API functions
 *
 * @{
 */

/*! @brief Initialize the Dolby Vision decoder backend control software module.

    @return
        @li 0         if successful.
        @li NEGATIVE  if there was error.
*/
int dovi_bec_initialize
    (
    void
    );

/*! @brief Un-initialize the Dolby Vision decoder backend control software module.
*/
void dovi_bec_uninitialize(void);

/*! @brief Start the Dolby Vision decoder backend control software module.

    @return
        @li 0         if successful.
        @li NEGATIVE  if there was error.
*/
int dovi_bec_start(void);

/*! @brief Pause the Dolby Vision decoder backend control software module.

    @return
        @li 0         if successful.
        @li NEGATIVE  if there was error.
*/
int dovi_bec_pause(void);

/*! @brief Reset the Dolby Vision decoder backend control software module
           to an initialized state.

    @return
        @li 0         if successful.
        @li NEGATIVE  if there was error.

    @note dovi_bec_pause() should be called before this function.
*/
int dovi_bec_reset(void);

/*! @brief Set the parameter for the Dolby Vision decoder backend control
           software module.

    @param[in] param_id the parameter ID.
    @param[in] param_val point to the parameter value.
    @param[in] param_sz size of the parameter value.

    @return
        @li 0         if successful.
        @li NEGATIVE  if there was error.
*/
int dovi_bec_setParam
    (
    dovi_bec_param_id_t  param_id,
    const void          *param_val,
    size_t               param_sz
    );

/*! @brief Get the parameter of the Dolby Vision decoder backend control
           software module.

    @param[in] param_id parameter ID.
    @param[in] param_val point to the parameteter value buffer.
    @param[in] param_sz pointer to the size of the paramter value buffer.

    @return
        @li 0         if successful.
        @li NEGATIVE  if there was error.

    @note Prior to calling this API, *param_sz should be set to the size
          of the parameter value block. Before returning, this API will
          set *param_sz to the actual size of the parameter value.
*/
int dovi_bec_getParam
    (
    dovi_bec_param_id_t  param_id,
    void                *param_val,
    size_t              *param_sz
    );

/*!
 * @}
 */


#endif     /* __DOVI_BEC_API_H__ */
