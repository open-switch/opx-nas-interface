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

/*!
 * \file   nas_vrf_extn.h
 */

#ifndef __NAS_VRF_EXTN_H
#define __NAS_VRF_EXTN_H

#include "std_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 *  \fn      int nas_vrf_init (void)
 *  \brief   Init function for VRF
 *  \warning none
 *  \param   void
 *  \return  success 0/failure -1
 *  \sa
 */
t_std_error nas_vrf_init(void);

#ifdef __cplusplus
}
#endif

#endif
