/*
 * Copyright (c) 2017 Dell Inc.
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

#define NAS_VRF_NAME_STR_SZ 32
/* VRF name with VRF-id mapping */
#define NAS_DEFAULT_VRF_NAME  "default"
#define NAS_MGMT_VRF_NAME     "management"
#define NAS_DEFAULT_VRF_ID    0
#define NAS_MGMT_VRF_ID       1

#define NAS_VRF_LOG_ERR(ID, ...) EV_LOGGING(INTERFACE, ERR, ID, __VA_ARGS__)
#define NAS_VRF_LOG_INFO(ID, ...) EV_LOGGING(INTERFACE, INFO, ID, __VA_ARGS__)
#define NAS_VRF_LOG_DEBUG(ID, ...) EV_LOGGING(INTERFACE, DEBUG, ID, __VA_ARGS__)

t_std_error nas_vrf_object_vrf_init(cps_api_operation_handle_t nas_vrf_cps_handle );
t_std_error nas_vrf_object_vrf_intf_init(cps_api_operation_handle_t nas_vrf_cps_handle );

t_std_error nas_vrf_process_cps_vrf_msg(cps_api_transaction_params_t * param, size_t ix);
t_std_error nas_vrf_process_cps_vrf_intf_msg(cps_api_transaction_params_t * param, size_t ix);
#endif /* NAS_VRF_H_ */
