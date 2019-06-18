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
 * interface_obj.h
 *
 *  Created on: Jan 21, 2016
 *      Author: cwichmann
 */

#ifndef INTERFACE_OBJ_H_
#define INTERFACE_OBJ_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "hal_if_mapping.h"

typedef cps_api_return_code_t (*cps_rdfn) (void * context, cps_api_get_params_t * param, size_t key_ix);

typedef cps_api_return_code_t (*cps_wrfn) (void * context, cps_api_transaction_params_t * param,size_t ix);

typedef enum {
    obj_INTF,
    obj_INTF_STATE,
    obj_INTF_STATISTICS,
    obj_INTF_MAX
} obj_intf_cat_t;

t_std_error interface_obj_init(cps_api_operation_handle_t handle);

t_std_error intf_obj_handler_registration(obj_intf_cat_t obj_cat, nas_int_type_t intf_type, cps_rdfn rd, cps_wrfn wr);

#ifdef __cplusplus
}
#endif
#endif /* INTERFACE_OBJ_H_ */
