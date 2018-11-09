/*
 * Copyright (c) 2018 Dell EMC.
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
 * nas_interface_mgmt_cps.h
 */

#ifndef _NAS_INTERFACE_MGMT_CPS_H_
#define _NAS_INTERFACE_MGMT_CPS_H_

#include "nas_interface.h"
#include "cps_api_events.h"
#include "cps_api_object_attr.h"


cps_api_return_code_t nas_interface_mgmt_if_state_set (void* context, cps_api_transaction_params_t* param,
                                                       size_t ix);

cps_api_return_code_t nas_interface_mgmt_if_set (void* context, cps_api_transaction_params_t* param,
                                                 size_t ix);

void nas_interface_mgmt_attr_up(cps_api_object_t obj, bool enabled);

#endif //NAS_INTERFACE_MGMT_CPS_H_
