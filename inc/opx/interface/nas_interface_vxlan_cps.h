
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
 * filename: nas_interface_vxlan_cps.h
 */

#ifndef _NAS_INTERFACE_VXLAN_CPS_H
#define _NAS_INTERFACE_VXLAN_CPS_H

#include "cps_api_operation.h"
#include "hal_interface_common.h"
#include "nas_interface.h"
#include "nas_interface_cps.h"
#include "ds_common_types.h"
#include "std_ip_utils.h"
#include "std_mutex_lock.h"


cps_api_return_code_t  nas_vxlan_add_cps_attr_for_interface(cps_api_object_t object);

t_std_error nas_vxlan_init(cps_api_operation_handle_t handle);
#endif /* _NAS_INTERFACE_VXLAN_CPS_H */
