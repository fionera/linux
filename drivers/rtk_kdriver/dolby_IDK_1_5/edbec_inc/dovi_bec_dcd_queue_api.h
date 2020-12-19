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

    @file  dovi_bec_dcd_queue_api.h
    @brief Dolby Vision decoder decoded sample queue management API

    This header file defines the APIs that must be provided by the
    implementation platform for managing the decoded sample queue.

    There shall be 3 decoded sample queues:
        (1) Bl decoded sample queue.
        (2) EL decoded sample queue.
        (3) Metadata decoded sample queue.


   @note The decoded sample queue shall be implemented as a FIFO.
         If the remove_dcd_sample() API is not called and the decoded
         sample queue is full at the time a new decoded sample must be
         added, the oldest decoded sample shall be removed automatically.
*/

#ifndef __DOVI_BEC_DCD_QUEUE_API_H__
#define __DOVI_BEC_DCD_QUEUE_API_H__

/*! @defgroup general Enumerations and data structures
 *
 * @{
 */

struct _dcd_stat_t_;

typedef uint64_t pts_t;

typedef void* dcd_sample_handle_t;

typedef enum
{
    BL_QID,
    EL_QID,
    MD_QID
} dcd_output_qid_t;

/*!
 * @}
 */

/*! @defgroup functions API functions
 *
 * @{
 */

/*! @brief Get the next decoded sample.

    @param[in] qid the decoded sample queue ID.

    @return
        @li NON-ZERO  if successful, a handle to the decoded sample.
        @li NULL      if the queue if empty
*/
dcd_sample_handle_t get_next_dcd_sample(dcd_output_qid_t qid);

/*! @brief Get the decoded sample with the specified PTS.

    @param[in] qid the decoded sample queue ID.
    @param[in] pts the PTS of the decoded sample.

    @return
        @li NON-ZERO  A handle to the decoded sample with the specified PTS.
        @li NULL      if there was no decoded sample withe the specified PTS.
*/
dcd_sample_handle_t get_dcd_sample_with_pts(dcd_output_qid_t qid, pts_t pts);

/*! @brief Remove all decoded sample whose PTS is less than or equal to the
           specified PTS.

    @param[in] qid the decoded sample queue ID.
    @param[in] pts a PTS value
*/
void remove_dcd_sample(dcd_output_qid_t qid, pts_t pts);

/*! @brief Get the PTS value for the decoded sample.

    @param[in] h_sample A handle to the decoded sample.

    @return
        @li A 64-bit PTS-value
*/
pts_t get_dcd_sample_pts(dcd_sample_handle_t h_sample);

/*! @brief Get the decoder status for the decoded sample.

    @param[in] h_sample A handle to the decoded sample.

    @return
        @li pointer to the decoder status structure of thte decoded sample.

    @note This data structure is platform-dependent and must defined.
*/
struct _dcd_stat_t_ *get_dcd_sample_stat(dcd_sample_handle_t h_sample);

/*! @brief Get the data pointer for the decoded sample.

    @param[in] h_sample A handle to the decoded sample.

    @return
        @li pointer to the decoded sample data.
*/
char *get_dcd_sample_frm_buf(dcd_sample_handle_t h_sample);

/*!
 * @}
 */


#endif     /* __DOVI_BEC_DCD_QUEUE_API_H__ */
