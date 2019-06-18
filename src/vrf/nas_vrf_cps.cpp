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
 * nas_vrf_cps.cpp
 */
#include "cps_class_map.h"
#include "event_log.h"
#include "ietf-network-instance.h"
#include "nas_vrf.h"
#include "nas_vrf_utils.h"
#include "nas_vrf_extn.h"
#include "vrf-mgmt.h"
#include "dell-base-common.h"
#include "hal_if_mapping.h"
#include "std_utils.h"
#include <vector>

#define NUM_INT_CPS_API_THREAD 1

static cps_api_operation_handle_t nas_vrf_handle;
extern "C" {
t_std_error nas_vrf_init(void) {

    //Create a handle for CPS objects
    if (cps_api_operation_subsystem_init(&nas_vrf_handle,NUM_INT_CPS_API_THREAD)!=cps_api_ret_code_OK) {
        return STD_ERR(ROUTE,FAIL,0);
    }

    /* Create the handle for publishing the messages. */
    if (nas_vrf_create_publish_handle() != STD_ERR_OK) {
        return STD_ERR(ROUTE,FAIL,0);
    }

    /* Create the default VRF oid */
    if (nas_vrf_update_vrf_id(NAS_DEFAULT_VRF_NAME, true) == false) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS", "Default VRF initialisation failed!");
        return STD_ERR(ROUTE,FAIL,0);
    }
    NAS_VRF_LOG_INFO("NAS-RT-CPS", "Default VRF initialisation successful!");

    if (nas_vrf_object_vrf_init(nas_vrf_handle) != STD_ERR_OK) {
        NAS_VRF_LOG_ERR("NAS-VRF", "Failed to initialize VRF handler");
        return STD_ERR(ROUTE,FAIL,0);
    }

    if (nas_vrf_object_vrf_intf_init(nas_vrf_handle) != STD_ERR_OK) {
        NAS_VRF_LOG_ERR("NAS-VRF-INTF", "Failed to initialize VRF and Intf bind handler");
        return STD_ERR(ROUTE,FAIL,0);
    }

    return STD_ERR_OK;
}
}

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

static cps_api_return_code_t nas_vrf_add_obj(const nas_vrf_ctrl_t &vrf_ctrl_block, cps_api_object_list_t param_list) {

    cps_api_object_t obj = cps_api_object_create();
    if(obj == NULL){
        NAS_VRF_LOG_ERR("VRF-ADD", "Failed to allocate memory to cps object");
        return cps_api_ret_code_ERR;
    }

    cps_api_key_t key;
    cps_api_key_from_attr_with_qual(&key, NI_NETWORK_INSTANCES_OBJ,
                                    cps_api_qualifier_TARGET);
    cps_api_object_set_key(obj, &key);
    cps_api_object_attr_add(obj, NI_NETWORK_INSTANCES_NETWORK_INSTANCE_NAME, (const void*) vrf_ctrl_block.vrf_name,
                    strlen(vrf_ctrl_block.vrf_name) + 1);
    if (!cps_api_object_list_append(param_list, obj)) {
        cps_api_object_delete(obj);
        NAS_VRF_LOG_ERR("VRF-ADD", "Object append to list is failed for %s", vrf_ctrl_block.vrf_name);
        return cps_api_ret_code_ERR;
    }

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t nas_vrf_cps_vrf_get_func (void *ctx,
                                                         cps_api_get_params_t * param,
                                                         size_t ix) {
    cps_api_object_t filt = cps_api_object_list_get(param->filters, ix);
    if (filt == NULL) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS","VRF intf object is not present");
        return cps_api_ret_code_ERR;
    }

    const char *vrf_name = (const char*) cps_api_object_get_data(filt,NI_NETWORK_INSTANCES_NETWORK_INSTANCE_NAME);

    if (vrf_name) {
        /* get VRF into from name */
        nas_vrf_ctrl_t vrf_ctrl_block;
        if (nas_get_vrf_ctrl_from_vrf_name(vrf_name, &vrf_ctrl_block) != STD_ERR_OK) {
            NAS_VRF_LOG_ERR("NAS-RT-CPS","VRF ctrl-block is not present");
            return cps_api_ret_code_ERR;
        }

        if (nas_vrf_add_obj(vrf_ctrl_block, param->list) != cps_api_ret_code_OK) {
            return cps_api_ret_code_ERR;
        }
    } else {
        /* get all VRF intf info */
        std::vector <nas_vrf_ctrl_t> vrf_ctrl_lst;
        nas_get_all_vrf_ctrl(vrf_ctrl_lst);
        for (auto vrf_ctrl_block : vrf_ctrl_lst) {
            if (nas_vrf_add_obj(vrf_ctrl_block, param->list) != cps_api_ret_code_OK) {
                return cps_api_ret_code_ERR;
            }
        }
    }

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
    cps_api_return_code_t rc = cps_api_ret_code_OK;

    cps_api_object_t filt = cps_api_object_list_get(param->filters,ix);
    if (filt == NULL) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS","VRF intf object is not present");
        return cps_api_ret_code_ERR;
    }

    const char *vrf_name  = (const char *)cps_api_object_get_data(filt, NI_IF_INTERFACES_INTERFACE_BIND_NI_NAME);
    const char *if_name  = (const char *)cps_api_object_get_data(filt, IF_INTERFACES_INTERFACE_NAME);

    if (if_name == nullptr) {
        NAS_VRF_LOG_DEBUG("NAS-RT-CPS","No If-name, VRF-Intf get failed!");
        return cps_api_ret_code_ERR;
    }

    if((rc = nas_vrf_get_intf_info(param->list, vrf_name, if_name)) != STD_ERR_OK){
        return (cps_api_return_code_t)rc;
    }

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t nas_vrf_cps_vrf_router_intf_get_func (void *ctx,
                                                                   cps_api_get_params_t * param,
                                                                   size_t ix) {
    NAS_VRF_LOG_DEBUG("NAS-RT-CPS", "VRF Intf get function");
    cps_api_return_code_t rc = cps_api_ret_code_OK;

    cps_api_object_t filt = cps_api_object_list_get(param->filters,ix);
    if (filt == NULL) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS","VRF intf object is not present");
        return cps_api_ret_code_ERR;
    }

    const char *if_name  = (const char *)cps_api_object_get_data(filt, VRF_MGMT_ROUTER_INTF_ENTRY_NAME);

    if (if_name == nullptr) {
        NAS_VRF_LOG_DEBUG("NAS-RT-CPS","No If-name, VRF-Intf get failed!");
        return cps_api_ret_code_ERR;
    }

    if((rc = nas_vrf_get_router_intf_info(param->list, if_name)) != STD_ERR_OK){
        return (cps_api_return_code_t)rc;
    }

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

    memset(&f,0,sizeof(f));

    /* Register for interface bind with VRF rpc object with CPS */
    if (!cps_api_key_from_attr_with_qual(&f.key, VRF_MGMT_INTF_BIND_NI_OBJ,
                                         cps_api_qualifier_TARGET)) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS","Could not translate %d to key %s",
                        (int)(VRF_MGMT_INTF_BIND_NI_OBJ),
                        cps_api_key_print(&f.key,buff,sizeof(buff)-1));
        return STD_ERR(ROUTE,FAIL,0);
    }

    NAS_VRF_LOG_DEBUG("NAS-RT-CPS", "Registering for %s",
                     cps_api_key_print(&f.key,buff,sizeof(buff)-1));

    f.handle = nas_vrf_cps_handle;
    f._write_function = nas_intf_bind_vrf_rpc_handler;

    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
        return STD_ERR(ROUTE,FAIL,0);
    }

    memset(&f,0,sizeof(f));

    NAS_VRF_LOG_DEBUG("NAS-RT-CPS", "VRF router Intf binding CPS Initialization");

    if (!cps_api_key_from_attr_with_qual(&f.key,VRF_MGMT_ROUTER_INTF_ENTRY_OBJ,cps_api_qualifier_TARGET)) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS","Could not translate router intf obj %d to key %s",
                        (int)(VRF_MGMT_ROUTER_INTF_ENTRY_OBJ),cps_api_key_print(&f.key,buff,sizeof(buff)-1));
        return STD_ERR(ROUTE,FAIL,0);
    }

    NAS_VRF_LOG_DEBUG("NAS-RT-CPS", "Registering for %s",
                      cps_api_key_print(&f.key,buff,sizeof(buff)-1));

    f.handle                 = nas_vrf_cps_handle;
    f._read_function         = nas_vrf_cps_vrf_router_intf_get_func;

    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
        return STD_ERR(ROUTE,FAIL,0);
    }

    return STD_ERR_OK;
}

