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
 * filename: nas_interface_fc.h
 */


#ifndef NAS_INTF_FC_H_
#define NAS_INTF_FC_H_

#include "nas_ndi_common.h"
#include "std_error_codes.h"
#include "cps_api_operation.h"
#include "nas_int_com_utils.h"



cps_api_return_code_t nas_cps_set_fc_attr(npu_id_t npu_id, port_t port, cps_api_object_t obj, cps_api_object_t prev);
cps_api_return_code_t nas_cps_get_fc_attr(void * context, cps_api_get_params_t *param,
                                                      size_t ix);
cps_api_return_code_t nas_cps_create_fc_port(npu_id_t npu_id, port_t port, BASE_IF_SPEED_t speed,
                                     uint32_t *hw_port_list, size_t count);
cps_api_return_code_t nas_cps_delete_fc_port(npu_id_t npu_id, port_t port);


void nas_fc_fill_speed_autoneg_state(npu_id_t npu, port_t port, cps_api_object_t obj);
void nas_fc_fill_supported_speed(npu_id_t npu, port_t port, cps_api_object_t obj);
void nas_fc_phy_fill_supported_speed(npu_id_t npu, port_t port, cps_api_object_t obj);
void nas_fc_fill_intf_attr(npu_id_t npu, port_t port, cps_api_object_t obj);
void nas_fc_fill_misc_state(npu_id_t npu, port_t port, cps_api_object_t obj);
#endif /* NAS_INTF_FC_H_ */
