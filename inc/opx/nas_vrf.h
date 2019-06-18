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
 * filename: nas_vrf.h
 */


#ifndef NAS_VRF_H_
#define NAS_VRF_H_

#include "std_error_codes.h"
#include "cps_api_operation.h"

#define NAS_VRF_LOG_ERR(ID, ...) EV_LOGGING(VRF, ERR, ID, __VA_ARGS__)
#define NAS_VRF_LOG_INFO(ID, ...) EV_LOGGING(VRF, INFO, ID, __VA_ARGS__)
#define NAS_VRF_LOG_DEBUG(ID, ...) EV_LOGGING(VRF, DEBUG, ID, __VA_ARGS__)

t_std_error nas_vrf_object_vrf_init(cps_api_operation_handle_t nas_vrf_cps_handle );
t_std_error nas_vrf_object_vrf_intf_init(cps_api_operation_handle_t nas_vrf_cps_handle );

t_std_error nas_vrf_process_cps_vrf_msg(cps_api_transaction_params_t * param, size_t ix);
t_std_error nas_vrf_process_cps_vrf_intf_msg(cps_api_transaction_params_t * param, size_t ix);
cps_api_return_code_t nas_intf_bind_vrf_rpc_handler (void * context, cps_api_transaction_params_t * param, size_t ix);
bool nas_vrf_update_vrf_id(const char *vrf_name, bool is_add);
cps_api_return_code_t nas_vrf_get_intf_info(cps_api_object_list_t list, const char *vrf_name,
                                            const char *if_name);
cps_api_return_code_t nas_vrf_get_router_intf_info(cps_api_object_list_t list, const char *if_name);
t_std_error nas_vrf_create_publish_handle();
#endif /* NAS_VRF_H_ */
