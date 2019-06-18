/*
 * Copyright (c) 2019 Dell Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS CODE IS PROVIDED ON AN *AS IS* BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 * LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
 * FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 * See the Apache Version 2.0 License for specific language governing
 * permissions and limitations under the License.
 */

/*
 * filename: swp_util_tap.h
 */


#ifndef _SWP_UTIL_TAP_H
#define _SWP_UTIL_TAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/select.h>

#include "event_log.h"
#include "std_error_codes.h"
#include "ds_common_types.h"

#include <stdbool.h>

#define SWP_UTIL_INV_FD (-1)
#define SWP_UTIL_TYPE_UNDEF 0
#define SWP_UTIL_TYPE_TAP 1
#define SWP_UTIL_TYPE_TUN 2


typedef struct _swp_util_tap_descr *swp_util_tap_descr;

/**
 * Allocate a single tun/tap structure
 * @return the structure pointer
 */
swp_util_tap_descr  swp_util_alloc_descr(void);

const char * swp_util_tap_descr_get_name(swp_util_tap_descr p);

int  swp_util_tap_descr_get_queue(swp_util_tap_descr p,int queue);

hal_ifindex_t swp_init_tap_ifindex(swp_util_tap_descr p);

/**
 * \brief free's the pointer "taps" but will not clean up resources..
 * \param[in] taps the pointer to free
 */
void swp_util_free_descrs(swp_util_tap_descr  taps);

/**
 * Create/Delete a tap/tun interface based on the user name and type specified
 * @param name the name of the interface to create
 * @param type the type of the interface (TUN or TAP)
 * @param create true if want to create, otherwise will set persist to 0
 * @return standard return code
 */
t_std_error swp_util_tap_operation(const char * name, int type, bool create);

/**
 * \brief Initializes tap descriptor with an instance and sub instance (both optional)
 * \param[in/out] p initialized tap details
 * \param[in] n The prefix of the tap name or the entire name if no inst or subi is included
 * \param[in] inst The instance id - NULL can be passed for no inst
 * \param[in] subi The sub instance to use NULL can be passed for no subi
 */
void swp_util_tap_descr_init_wname(swp_util_tap_descr p,
                           const char *n,int *chas, int *inst, port_t *port, port_t * subi, int queues);

/**
 * \brief Initializes tap descriptor with an instance and sub instance (both optional)
 * Must be called after the tap interface has been created with the tap operation
 * or the tap will be set to non persistent by default.
 *
 * \param[in] tap tap description to allocate (SWP_UTIL_TYPE_TAP or SWP_UTIL_TYPE_TUN)
 */
t_std_error swp_util_alloc_tap(swp_util_tap_descr tap, int type);

/**
 * Close all associated file descriptors with the tap device.  If the device is
 * persistent then the device will show as operationally down otherwise will delete
 * the interface from the linux kernel
 * @param tap structure to operate on
 */
void swp_util_close_fds(swp_util_tap_descr tap);

/**
 * \brief Prints a tap descriptor
 * \param[in] tap tap to print
 */
void swp_util_print_tap(swp_util_tap_descr tap);

/**
 * \brief Resets a tap descriptor
 * \param[in] p tap to initialize
 */
void swp_util_tap_descr_init(swp_util_tap_descr p);

/*
 * \brief Return the maximum file descriptor and add all filedescriptors to the set
 * \param[in] p tap descriptor array
 * \param[in] descrs the number of descriptors to use
 * */
int swp_util_tap_fd_set_add(register swp_util_tap_descr *p, register int descrs, register fd_set *set);

/*
 * \brief Return the file descriptor that is active in the set
 * \param[in] p The descriptor to search for a FD that is set
 * \param[in] set The fd_set that contains the set fds
 * */
int swp_util_tap_fd_locate_from_set(register swp_util_tap_descr p, register fd_set *set);
bool swp_util_tap_is_fd_in_tap_fd_set(register swp_util_tap_descr p, int fd);

#ifdef __cplusplus
}
#endif
#endif
