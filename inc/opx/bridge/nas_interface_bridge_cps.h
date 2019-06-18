
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
 * filename: nas_interface_bridge_cps.h
 */

#ifndef _NAS_INTERFACE_BRIDGE_CPS_H
#define _NAS_INTERFACE_BRIDGE_CPS_H



#include "hal_if_mapping.h"
#include "nas_int_utils.h"
#include "ds_common_types.h"
#include "cps_api_object_key.h"
#include "cps_api_events.h"
#include "cps_class_map.h"
#include "nas_interface_bridge.h"
#include "nas_interface_1q_bridge.h"
#include "nas_interface_1d_bridge.h"
#include "nas_interface_bridge_map.h"
#include "nas_interface_bridge_utils.h"

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif
const static int MAX_CPS_MSG_BUFF=10000;

cps_api_return_code_t nas_bridge_cps_publish_1q_object(NAS_DOT1Q_BRIDGE *p_bridge_node, cps_api_operation_types_t op);
cps_api_return_code_t nas_bridge_cps_publish_1d_object(NAS_DOT1D_BRIDGE *p_bridge_node, cps_api_operation_types_t op);
t_std_error nas_bridge_cps_obj_init(cps_api_operation_handle_t handle);

t_std_error nas_vxlan_bridge_cps_init(cps_api_operation_handle_t handle);

#ifdef __cplusplus
}
#endif
#endif /* _NAS_INTERFACE_BRIDGE_CPS_UTILS_H */
