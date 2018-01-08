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
 * nas_vrf_cps.cpp
 */
#include "cps_class_map.h"
#include "event_log.h"
#include "ietf-network-instance.h"
#include "nas_vrf.h"


static cps_api_return_code_t nas_vrf_cps_vrf_set_func(void *ctx,
                                                        cps_api_transaction_params_t * param,
                                                        size_t ix) {
    cps_api_object_t          obj;

    NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "VRF CPS set");
    if(param == NULL){
        NAS_VRF_LOG_ERR("NAS-RT-CPS", "VRF with no param: ");
        return cps_api_ret_code_ERR;
    }

    obj = cps_api_object_list_get (param->change_list, ix);
    if (obj == NULL) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS", "Missing VRF CPS Object");
        return cps_api_ret_code_ERR;
    }

    cps_api_return_code_t rc = cps_api_ret_code_OK;
    rc = nas_vrf_process_cps_vrf_msg(param,ix);

    return rc;
}

static cps_api_return_code_t nas_vrf_cps_vrf_get_func (void *ctx,
                                                         cps_api_get_params_t * param,
                                                         size_t ix) {
    NAS_VRF_LOG_DEBUG("NAS-RT-CPS", "VRF get function");
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t nas_vrf_cps_vrf_rollback_func(void * ctx,
                                                             cps_api_transaction_params_t * param, size_t ix){

    NAS_VRF_LOG_DEBUG("NAS-RT-CPS", "VRF Rollback function");
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t nas_vrf_cps_vrf_intf_set_func(void *ctx,
                                                        cps_api_transaction_params_t * param,
                                                        size_t ix) {
    cps_api_object_t          obj;

    NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "VRF CPS intf");
    if(param == NULL){
        NAS_VRF_LOG_ERR("NAS-RT-CPS", "VRF Init CPS with no param: ");
        return cps_api_ret_code_ERR;
    }

    obj = cps_api_object_list_get (param->change_list, ix);
    if (obj == NULL) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS", "Missing VRF CPS Object");
        return cps_api_ret_code_ERR;
    }

    cps_api_return_code_t rc = cps_api_ret_code_OK;
    rc = nas_vrf_process_cps_vrf_intf_msg(param,ix);

    return rc;
}

static cps_api_return_code_t nas_vrf_cps_vrf_intf_get_func (void *ctx,
                                                         cps_api_get_params_t * param,
                                                         size_t ix) {
    NAS_VRF_LOG_DEBUG("NAS-RT-CPS", "VRF Intf get function");
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t nas_vrf_cps_vrf_intf_rollback_func(void * ctx,
                                                             cps_api_transaction_params_t * param, size_t ix){

    NAS_VRF_LOG_DEBUG("NAS-RT-CPS", "VRF Intf Rollback function");
    return cps_api_ret_code_OK;
}

t_std_error nas_vrf_object_vrf_init(cps_api_operation_handle_t nas_vrf_cps_handle ) {

    cps_api_registration_functions_t f;
    char buff[CPS_API_KEY_STR_MAX];

    memset(&f,0,sizeof(f));

    NAS_VRF_LOG_DEBUG("NAS-RT-CPS", "VRF CPS Initialization");

    if (!cps_api_key_from_attr_with_qual(&f.key,NI_NETWORK_INSTANCES_OBJ,cps_api_qualifier_TARGET)) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS","Could not translate %d to key %s",
                       (int)(NI_NETWORK_INSTANCES_OBJ),cps_api_key_print(&f.key,buff,sizeof(buff)-1));
        return STD_ERR(ROUTE,FAIL,0);
    }

    NAS_VRF_LOG_DEBUG("NAS-RT-CPS", "Registering for %s",
                     cps_api_key_print(&f.key,buff,sizeof(buff)-1));

    f.handle                 = nas_vrf_cps_handle;
    f._read_function         = nas_vrf_cps_vrf_get_func;
    f._write_function        = nas_vrf_cps_vrf_set_func;
    f._rollback_function     = nas_vrf_cps_vrf_rollback_func;

    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
        return STD_ERR(ROUTE,FAIL,0);
    }
    return STD_ERR_OK;
}

t_std_error nas_vrf_object_vrf_intf_init(cps_api_operation_handle_t nas_vrf_cps_handle ) {

    cps_api_registration_functions_t f;
    char buff[CPS_API_KEY_STR_MAX];

    memset(&f,0,sizeof(f));

    NAS_VRF_LOG_DEBUG("NAS-RT-CPS", "VRF Intf binding CPS Initialization");

    if (!cps_api_key_from_attr_with_qual(&f.key,NI_IF_INTERFACES_INTERFACE_OBJ,cps_api_qualifier_TARGET)) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS","Could not translate %d to key %s",
                       (int)(NI_IF_INTERFACES_INTERFACE_OBJ),cps_api_key_print(&f.key,buff,sizeof(buff)-1));
        return STD_ERR(ROUTE,FAIL,0);
    }

    NAS_VRF_LOG_DEBUG("NAS-RT-CPS", "Registering for %s",
                     cps_api_key_print(&f.key,buff,sizeof(buff)-1));

    f.handle                 = nas_vrf_cps_handle;
    f._read_function         = nas_vrf_cps_vrf_intf_get_func;
    f._write_function        = nas_vrf_cps_vrf_intf_set_func;
    f._rollback_function     = nas_vrf_cps_vrf_intf_rollback_func;

    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
        return STD_ERR(ROUTE,FAIL,0);
    }
    return STD_ERR_OK;
}

