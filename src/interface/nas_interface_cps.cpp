/*
 * Copyright (c) 2018 Dell Inc.
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
 * filename: .cpp
 */

#include "dell-base-if-linux.h"
#include "iana-if-type.h"
#include "interface/nas_interface_cps.h"
#include "interface/nas_interface_vxlan_cps.h"
#include "interface/nas_interface_utils.h"
#include "interface/nas_interface_mgmt.h"
#include "interface/nas_interface_mgmt_cps.h"
#include "nas_os_interface.h"
#include "interface_obj.h"
#include "nas_int_logical.h"
#include "cps_api_object.h"
#include "cps_api_object_key.h"
#include "std_ip_utils.h"
#include "vrf-mgmt.h"
#include "std_utils.h"
#include <unordered_map>

static std_mutex_lock_create_static_init_fast(vlan_mutex);

std_mutex_type_t * get_vlan_mutex(){
    return &vlan_mutex;
}

static std::unordered_map <cps_api_attr_id_t, cps_api_attr_id_t> intf_to_state_map = {
    {DELL_IF_IF_INTERFACES_INTERFACE_DUPLEX, DELL_IF_IF_INTERFACES_STATE_INTERFACE_DUPLEX},
    {IF_INTERFACES_INTERFACE_ENABLED, IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS},
    {IF_INTERFACES_INTERFACE_TYPE, IF_INTERFACES_STATE_INTERFACE_TYPE}
};

void convert_name_attr_to_state(cps_api_object_t obj) {
    cps_api_object_attr_t intf_attr;
    if ((intf_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME)) != nullptr) {
        cps_api_set_key_data(obj, IF_INTERFACES_STATE_INTERFACE_NAME, cps_api_object_ATTR_T_BIN, cps_api_object_attr_data_bin(intf_attr),
            cps_api_object_attr_len(intf_attr));
        /* TODO: Need a way to delete key-data */
        cps_api_object_attr_delete(obj, IF_INTERFACES_INTERFACE_NAME);
    }

    return;
}

void cps_convert_to_state_attribute(cps_api_object_t obj) {
    cps_api_object_attr_t intf_attr;

    convert_name_attr_to_state(obj);
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj), DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
            cps_api_qualifier_OBSERVED);

    for (const auto &val : intf_to_state_map) {
        intf_attr = cps_api_object_attr_get(obj, val.first);

        if (!intf_attr) {
            EV_LOGGING(INTERFACE, DEBUG, "NAS-INT-GET", "Can't find interface attribute (%lu)",
                val.first);
            continue;
        }
        cps_api_object_attr_add(obj, val.second, cps_api_object_attr_data_bin(intf_attr),
                cps_api_object_attr_len(intf_attr));
        cps_api_object_attr_delete(obj, val.first);
    }

    return;
}

cps_api_return_code_t nas_mgmt_add_cps_attr_for_interface(cps_api_object_t obj, bool get_state) {
    const char* if_name = (const char *) cps_api_object_get_data(obj, IF_INTERFACES_INTERFACE_NAME);
    interface_ctrl_t intf_ctrl;
    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));
    safestrncpy(intf_ctrl.if_name, if_name, sizeof(intf_ctrl.if_name));
    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    if ((dn_hal_get_interface_info(&intf_ctrl)) == STD_ERR_OK) {
        if (intf_ctrl.vrf_id != NAS_DEFAULT_VRF_ID) {
            cps_api_object_attr_add_u32(obj, VRF_MGMT_NI_IF_INTERFACES_INTERFACE_VRF_ID,
                    intf_ctrl.vrf_id);
        }
    }
    nas_os_get_interface_ethtool_cmd_data(if_name, obj);
    nas_os_get_interface_oper_status(if_name, obj);
    cps_api_object_attr_t spattr = cps_api_object_attr_get(obj,
            DELL_IF_IF_INTERFACES_INTERFACE_SPEED);
    if (get_state && spattr != NULL) {
        uint64_t state_speed = 0;
        BASE_IF_SPEED_t speed = (BASE_IF_SPEED_t)cps_api_object_attr_data_u32(spattr);
        if (nas_base_to_ietf_state_speed(speed, &state_speed) == true) {
            cps_api_object_attr_add_u64(obj, IF_INTERFACES_STATE_INTERFACE_SPEED, state_speed);
        }
    }

    return cps_api_ret_code_OK;
}

cps_api_return_code_t nas_interface_com_if_get (void * context, cps_api_get_params_t * param,
        size_t key_ix, bool get_state) {

    cps_api_object_t filt = cps_api_object_list_get(param->filters,key_ix);
    char *req_if_ietf_type = NULL;
    char if_ietf_type[256];
    cps_api_object_attr_t ifix;
    cps_api_object_attr_t type_attr;
    cps_api_object_attr_t name;

    if (get_state) {
        ifix = cps_api_object_attr_get(filt, IF_INTERFACES_STATE_INTERFACE_IF_INDEX);
        if (ifix != nullptr) {
            /*  Call os API with interface object  */
            cps_api_object_attr_add_u32(filt,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,
                                     cps_api_object_attr_data_u32(ifix));
            cps_api_object_attr_delete(filt,IF_INTERFACES_STATE_INTERFACE_IF_INDEX);
        }
        type_attr = cps_api_object_attr_get(filt, IF_INTERFACES_STATE_INTERFACE_TYPE);
        name = cps_api_get_key_data(filt, IF_INTERFACES_STATE_INTERFACE_NAME);
    } else {
        ifix = cps_api_object_attr_get(filt, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
        type_attr = cps_api_object_attr_get(filt, IF_INTERFACES_INTERFACE_TYPE);
        name = cps_api_get_key_data(filt, IF_INTERFACES_INTERFACE_NAME);
    }

    if (type_attr != nullptr) {
        req_if_ietf_type = (char *)cps_api_object_attr_data_bin(type_attr);
    }


    if (ifix == nullptr && name != nullptr) {
        interface_ctrl_t _if;
        memset(&_if, 0, sizeof(_if));
        safestrncpy(_if.if_name, (const char *)cps_api_object_attr_data_bin(name), sizeof(_if.if_name)-1);
        _if.q_type = HAL_INTF_INFO_FROM_IF_NAME;
        if (dn_hal_get_interface_info(&_if) != STD_ERR_OK)  {
            EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Wrong interface name or interface not present %s", _if.if_name);
            return cps_api_ret_code_ERR;
        }

        cps_api_object_attr_add_u32(filt, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,
                                    _if.if_index);
        if (!nas_to_ietf_if_type_get(_if.int_type, if_ietf_type, sizeof(if_ietf_type))) {
            EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Failed to get IETF interface type for type id %d",
                       _if.int_type);
            return cps_api_ret_code_ERR;
        }
        req_if_ietf_type = if_ietf_type;
    }


    EV_LOGGING(INTERFACE, INFO, "NAS-INT-GET", "GET request received for interface type: %s", (req_if_ietf_type) ? req_if_ietf_type : "unknown");

    if (nas_os_get_interface(filt,param->list)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Failed to get interfaces from OS");
        return cps_api_ret_code_ERR;
    }

    cps_api_return_code_t rc = cps_api_ret_code_OK;

    for (size_t ix = 0, mx = cps_api_object_list_size(param->list); ix < mx; ix++) {
        if (!req_if_ietf_type) {
            continue;
        }

        cps_api_object_t object = cps_api_object_list_get(param->list, ix);
        cps_api_key_from_attr_with_qual(cps_api_object_key(object), DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                cps_api_qualifier_TARGET);

        cps_api_object_attr_add(object, IF_INTERFACES_INTERFACE_TYPE, (const void *)req_if_ietf_type,
                strlen(req_if_ietf_type) + 1);

        if (strncmp(req_if_ietf_type, IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_BASE_IF_VXLAN,
                        strlen(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_BASE_IF_VXLAN)) == 0) {
            rc = nas_vxlan_add_cps_attr_for_interface(object);
        } else if (strncmp(req_if_ietf_type, IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_BASE_IF_MANAGEMENT,
                            strlen(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_BASE_IF_MANAGEMENT)) == 0) {
            rc = nas_mgmt_add_cps_attr_for_interface(object, get_state);
        }

        if (rc != cps_api_ret_code_OK) {
           return rc;
        }

        if (get_state) {
            cps_convert_to_state_attribute(object);
        }
    }

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _vlan_sub_intf_create(cps_api_object_t obj){

    std_mutex_simple_lock_guard lock(get_vlan_mutex());
    EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN-SUB-INTF","Entry");

    cps_api_object_attr_t name_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);
    cps_api_object_attr_t vlan_id_attr = cps_api_object_attr_get(obj,DELL_IF_IF_INTERFACES_INTERFACE_VLAN_ID);
    cps_api_object_attr_t parent_if_attr = cps_api_object_attr_get(obj,DELL_IF_IF_INTERFACES_INTERFACE_PARENT_INTERFACE);

    if(!name_attr || !vlan_id_attr || !parent_if_attr) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-SUB-INTF","Missing vlan sub interface name/vlan id/parent "
                "interface for create");
        return cps_api_ret_code_ERR;
    }

    std::string vlan_sub_intf_name = std::string((const char *)cps_api_object_attr_data_bin(name_attr));
    hal_vlan_id_t vlan_id = cps_api_object_attr_data_uint(vlan_id_attr);
    std::string parent_intf_name = std::string((const char *)cps_api_object_attr_data_bin(parent_if_attr));

    if(nas_interface_vlan_subintf_create(vlan_sub_intf_name,vlan_id,parent_intf_name,true) == STD_ERR_OK){
        return cps_api_ret_code_OK;
    }

    return cps_api_ret_code_ERR;

}

static cps_api_return_code_t _vlan_sub_intf_delete(cps_api_object_t obj){
    std_mutex_simple_lock_guard lock(get_vlan_mutex());
    EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN-SUB-INTF","Entry");

    cps_api_object_attr_t name_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);

    if(!name_attr) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-SUB-INTF","Missing vlan sub interface name for delete");
        return cps_api_ret_code_ERR;
    }

    std::string vlan_sub_intf_name = std::string((const char *)cps_api_object_attr_data_bin(name_attr));

    if(nas_interface_vlan_subintf_delete(vlan_sub_intf_name) == STD_ERR_OK){
        return cps_api_ret_code_OK;
    }

    return cps_api_ret_code_ERR;

}

static cps_api_return_code_t nas_interface_com_if_set(void* context, cps_api_transaction_params_t* param,
                                                        size_t ix)
{
     cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
     cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));


    if(op == cps_api_oper_CREATE){
        return _vlan_sub_intf_create(obj);
    }else if(op == cps_api_oper_DELETE){
        return _vlan_sub_intf_delete(obj);
    }else{
        EV_LOGGING(INTERFACE,ERR,"NAS-VLAN-SUB-INTF","Operation %d not supported",op);
    }

    return cps_api_ret_code_ERR;
}

/* Wrappers for the NAS common handlers */
cps_api_return_code_t nas_interface_com_if_get_handler (void * context, cps_api_get_params_t * param,
        size_t key_ix) {
    return nas_interface_com_if_get (context, param, key_ix, false);
}

cps_api_return_code_t nas_interface_com_if_state_get_handler (void * context, cps_api_get_params_t * param,
        size_t key_ix) {
    return nas_interface_com_if_get (context, param, key_ix, true);
}

cps_api_return_code_t nas_interface_com_if_state_set_handler(void* context, cps_api_transaction_params_t* param, size_t ix) {
    return cps_api_ret_code_ERR;
}

t_std_error nas_interface_generic_init(cps_api_operation_handle_t handle)  {

    if (intf_obj_handler_registration(obj_INTF, nas_int_type_VLANSUB_INTF, nas_interface_com_if_get_handler,
                nas_interface_com_if_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT", "Failed to register VLAN SUB interface CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (intf_obj_handler_registration(obj_INTF_STATE, nas_int_type_VLANSUB_INTF, nas_interface_com_if_state_get_handler,
                nas_interface_com_if_state_set_handler) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT", "Failed to register VLAN SUB interface-state CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (intf_obj_handler_registration(obj_INTF, nas_int_type_MGMT,
                nas_interface_com_if_get_handler, nas_interface_mgmt_if_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-MGMT-INTF", "Failed to register mgmt interface CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (intf_obj_handler_registration(obj_INTF_STATE, nas_int_type_MGMT,
                nas_interface_com_if_state_get_handler, nas_interface_mgmt_if_state_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-MGMT-INTF", "Failed to register mgmt interface state CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (intf_obj_handler_registration(obj_INTF, nas_int_type_LPBK, nas_interface_com_if_get_handler,
                nas_interface_com_if_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT", "Failed to register loopback interface CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (intf_obj_handler_registration(obj_INTF_STATE, nas_int_type_LPBK, nas_interface_com_if_state_get_handler,
                nas_interface_com_if_state_set_handler) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT", "Failed to register loopback interface-state CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return STD_ERR_OK;
}
