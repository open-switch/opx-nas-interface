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
 * nas_vrf_api.cpp
 *
 */

#include "event_log.h"
#include "nas_os_l3.h"
#include "nas_vrf.h"
#include "ietf-network-instance.h"
#include "cps_api_events.h"

cps_api_return_code_t nas_vrf_process_cps_vrf_msg(cps_api_transaction_params_t * param, size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    cps_api_return_code_t rc = cps_api_ret_code_OK;

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    cps_api_object_t cloned = cps_api_object_create();
    if (!cloned) {
        NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "CPS malloc error");
        return cps_api_ret_code_ERR;
    }
    cps_api_object_clone(cloned,obj);
    cps_api_object_list_append(param->prev,cloned);

    if (op == cps_api_oper_CREATE) {
        NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "In VRF CREATE ");
        if(nas_os_add_vrf(obj) != STD_ERR_OK){
            NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "OS VRF add failed");
            rc = cps_api_ret_code_ERR;
        }
    } else if (op == cps_api_oper_SET) {
        NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "In VRF SET");
        if(nas_os_set_vrf(obj) != STD_ERR_OK){
            NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "OS VRF add failed");
            rc = cps_api_ret_code_ERR;
        }
    } else if (op == cps_api_oper_DELETE) {
        NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "In VRF del ");
        if(nas_os_del_vrf(obj) != STD_ERR_OK){
            NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "OS VRF del failed");
            rc = cps_api_ret_code_ERR;
        }
    }

    if (rc == cps_api_ret_code_OK) {
        cps_api_key_set(cps_api_object_key(obj),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_OBSERVED);
        if (cps_api_event_thread_publish(obj) != STD_ERR_OK) {
            NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "VRF publish failed!");
        }
        cps_api_key_set(cps_api_object_key(obj),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_TARGET);
    }
    return rc;
}

cps_api_return_code_t nas_vrf_process_cps_vrf_intf_msg(cps_api_transaction_params_t * param, size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    cps_api_return_code_t rc = cps_api_ret_code_OK;

    if (obj == NULL) {
        NAS_VRF_LOG_ERR("NAS-RT-CPS","Route VRF Intf object is not present");
        return cps_api_ret_code_ERR;
    }

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    cps_api_object_t cloned = cps_api_object_create();
    if (!cloned) {
        NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "CPS malloc error");
        return cps_api_ret_code_ERR;
    }
    cps_api_object_clone(cloned,obj);
    cps_api_object_list_append(param->prev,cloned);

    if (op == cps_api_oper_CREATE) {
        NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "In VRF Intf CREATE ");
        if(nas_os_bind_if_name_to_vrf(obj) != STD_ERR_OK){
            NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "OS VRF add failed");
            rc = cps_api_ret_code_ERR;
        }
    } else if (op == cps_api_oper_DELETE) {
        NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "In VRF intf del ");
        if(nas_os_unbind_if_name_from_vrf(obj) != STD_ERR_OK){
            NAS_VRF_LOG_DEBUG("NAS-RT-CPS-SET", "OS VRF del failed");
            rc = cps_api_ret_code_ERR;
        }
    }

    if (rc == cps_api_ret_code_OK) {
        cps_api_key_set(cps_api_object_key(obj),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_OBSERVED);
        if (cps_api_event_thread_publish(obj) != STD_ERR_OK) {
            NAS_VRF_LOG_ERR("NAS-RT-CPS-SET", "VRF interface publish failed!");
        }
        cps_api_key_set(cps_api_object_key(obj),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_TARGET);
    }

    return rc;
}

