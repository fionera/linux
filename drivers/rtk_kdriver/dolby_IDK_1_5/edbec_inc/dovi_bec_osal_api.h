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

    @file  dovi_bec_osal_api.h
    @brief OS abstraction layer (OSAL) API definition

    This header file defines the OS abstraction layer APIs that must
    be implemented.
*/

#ifndef __DOVI_BEC_OSAL_API_H__
#define __DOVI_BEC_OSAL_API_H__
//RTK MARK
#ifdef RTK_EDBEC_CONTROL
#include <linux/types.h>
#else
#include <stdint.h>
#endif



#define COMPOSER_THREAD_PRIO            1


#ifndef  TRUE
#define TRUE              1
#endif
#ifndef  FALSE
#define FALSE             0
#endif

/*! @defgroup general Enumerations and data structures
 *
 * @{
 */

#ifndef CONFIG_KDRIVER_USE_NEW_COMMON
typedef unsigned char bool_t;
#endif


/* OS Abstraction Layer (OSAL) Data Types */

typedef void* os_sem_handle_t;

typedef void* os_mutex_handle_t;

typedef void* os_thread_handle_t;

typedef void (*os_intr_handler_t)(void);

typedef void (*os_thread_entry_func_t)(void *arg);


/* OS Abstraction Layer (OSAL) APIs */
/*!
 * @}
 */

/*! @defgroup functions API functions
 *
 * @{
 */


/*! @brief Create a binary semaphore.

    @return
        @li NON-ZERO  if successful, a handle to the binary semaphore.
        @li NULL      if there was error.
*/
//RTK MARK
#ifndef RTK_EDBEC_CONTROL
os_sem_handle_t os_sem_create();
#endif
/*! @brief Delete a binary semaphore.

    @param[in] h_sem handle for the binary semaphore.
*/
//RTK MARK
#ifndef RTK_EDBEC_CONTROL
void os_sem_delete(os_sem_handle_t h_sem);
#endif
/*! @brief Wait for the binary semaphore to be signaled.

    @param[in] h_sem handle for the binary semaphore.

    @return
        @li 0         if successful.
        @li NEGATIVE  if there was error.
*/
//RTK MARK
#ifndef RTK_EDBEC_CONTROL
int os_sem_wait(os_sem_handle_t h_sem);
#endif
/*! @brief Signal/Post the binary semaphore.

    @param[in] h_sem handle for the binary semaphore.

    @return
        @li 0         if successful.
        @li NEGATIVE  if there was error.
*/
//RTK MARK
#ifndef RTK_EDBEC_CONTROL
int os_sem_post(os_sem_handle_t h_sem);
#endif
/*! @brief Create a mutex.

    @return
        @li NON-ZERO  if successful, a handle to the mutex.
        @li NULL      if there was error.
*/
//RTK MARK
#ifndef RTK_EDBEC_CONTROL
os_mutex_handle_t os_mutex_create();
#endif
/*! @brief Delete a mutex.

    @param[in] h_mutex handle for the mutex.
*/
//RTK MARK
#ifndef RTK_EDBEC_CONTROL
void os_mutex_delete(os_mutex_handle_t h_mutex);
#endif
/*! @brief Acquire the mutex.

    @param[in] h_mutex handle for the mutex.

    @return
        @li 0         if successful.
        @li NEGATIVE  if there was error.
*/
//RTK MARK
#ifndef RTK_EDBEC_CONTROL
int os_mutex_lock(os_mutex_handle_t h_mutex);
#endif
/*! @brief Release the mutex.

    @param[in] h_mutex handle for the mutex.

    @return
        @li 0         if successful.
        @li NEGATIVE  if there was error.
*/
//RTK MARK
#ifndef RTK_EDBEC_CONTROL
int os_mutex_unlock(os_mutex_handle_t h_mutex);
#endif
/*! @brief Create a thread.

    @param[in] pf_thread_entry pointer to the thread entry function.
    @param[in] prio thread priority.

    @return
        @li NON-ZERO  if successful, a handle to the thread.
        @li NULL      if there was error.
*/
//RTK MARK
#ifndef RTK_EDBEC_CONTROL
os_thread_handle_t os_thread_create(os_thread_entry_func_t pf_thread_entry, int prio);
#endif
/*! @brief Delete a thread.

    @param[in] h_thread handle for the thread.
*/
//RTK MARK
#ifndef RTK_EDBEC_CONTROL
void os_thread_delete(os_thread_handle_t h_thread);
#endif
/*! @brief Install an application interrupt handler

    @param[in] pf_intr_handler pointer to the application interrupt handler function.
    @param[in] level an interrupt level, range TBD

    @return
        @li 0         if successful.
        @li NEGATIVE  if there was error.
*/
//RTK MARK
#ifndef RTK_EDBEC_CONTROL
int os_install_intr(os_intr_handler_t pf_intr_handler, int level);
#endif
/*! @brief Sleep for a number of millisec.

    @param[in] msec sleep time in millisec
*/
//RTK MARK
#ifndef RTK_EDBEC_CONTROL
void os_sleep_msec(uint32_t msec);
#endif
/*!
 * @}
 */

#endif     /* __DOVI_BEC_OSAL_API_H__ */
