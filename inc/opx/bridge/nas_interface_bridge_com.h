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
 * nas_interface_bridge_com.h
 */

#ifndef _NAS_INTERFACE_BRIDGE_COM_H
#define _NAS_INTERFACE_BRIDGE_COM_H

#include "cps_api_operation.h"
#include "ds_common_types.h"
#include "event_log.h"
#include "event_log_types.h"
#include "std_mutex_lock.h"
#include "std_error_codes.h"


#define INTERFACE_VLAN_CFG_FILE "/etc/opx/interface_vlan_config.xml"

std_mutex_type_t *nas_vlan_mode_mtx();
std_mutex_type_t *nas_bridge_mtx_lock();

bool nas_g_scaled_vlan_get(void);
void nas_g_scaled_vlan_set(bool enable);

bool nas_vlan_interface_get_vn_untagged_vlan(hal_vlan_id_t * vlan_id);

bool nas_check_reserved_vlan_id(hal_vlan_id_t vlan_id);

cps_api_return_code_t nas_interface_handle_global_set (void * context, cps_api_transaction_params_t *param, size_t ix);

void process_vlan_config_file(void);

t_std_error nas_default_vlan_cache_init(void);
#endif /* _NAS_INTERFACE_BRIDGE_COM_H */
