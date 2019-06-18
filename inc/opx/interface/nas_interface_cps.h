
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
 * filename:  nas_interface_cps.h
 */

#ifndef _NAS_INTERFACE_CPS_H
#define _NAS_INTERFACE_CPS_H

#include "dell-base-if.h"
#include "dell-interface.h"
#include "dell-base-common.h"
#include "ietf-interfaces.h"

#include "cps_class_map.h"
#include "cps_api_events.h"
#include "cps_api_object_key.h"
#include "hal_if_mapping.h"
#include "nas_if_utils.h"
#include "nas_interface.h"
#include "nas_interface_map.h"
#include "event_log.h"

#include "ds_common_types.h"
#include "std_error_codes.h"
#include "std_mutex_lock.h"

#include <string>
#include <unordered_map>
#include <mutex>

void nas_interface_cps_publish_event(std::string &if_name, nas_int_type_t if_type, cps_api_operation_types_t op);
t_std_error nas_interface_generic_init(cps_api_operation_handle_t handle);
cps_api_return_code_t  nas_vxlan_add_cps_attr_for_interface(cps_api_object_t object);
cps_api_return_code_t nas_interface_com_if_get_handler(void * context, cps_api_get_params_t * param,
        size_t key_ix);
cps_api_return_code_t nas_interface_if_get_handler(void * context, cps_api_get_params_t * param,
        size_t key_ix);
cps_api_return_code_t nas_interface_fill_obj(cps_api_object_list_t *list, std::string name, 
        bool get_state);
cps_api_return_code_t nas_interface_com_if_state_get_handler(void * context, cps_api_get_params_t * param,
        size_t key_ix);
cps_api_return_code_t nas_interface_if_state_get_handler(void * context, cps_api_get_params_t * param,
        size_t key_ix);
cps_api_return_code_t nas_interface_com_if_state_set_handler(void* context, cps_api_transaction_params_t* param,
        size_t ix);
void cps_convert_to_state_attribute(cps_api_object_t obj);
#endif /* _NAS_INTERFACE_CPS_H */
