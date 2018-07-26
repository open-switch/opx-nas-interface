
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
#include "interface/nas_interface_vxlan.h"
#include "interface/nas_interface_utils.h"
#include "nas_os_interface.h"
#include "interface_obj.h"
#include "cps_api_object.h"
#include "cps_api_object_key.h"
#include "std_ip_utils.h"


static std_mutex_lock_create_static_init_fast(vlan_mutex);

std_mutex_type_t * get_vlan_mutex(){
    return &vlan_mutex;
}

cps_api_return_code_t nas_interface_com_if_get (void * context, cps_api_get_params_t * param,
        size_t key_ix) {

    cps_api_object_t filt = cps_api_object_list_get(param->filters,key_ix);
    cps_api_object_attr_t type_attr = cps_api_object_attr_get(filt, IF_INTERFACES_INTERFACE_TYPE);
    char *req_if_ietf_type = NULL;
    char if_ietf_type[256];
    if (type_attr != nullptr) {
        req_if_ietf_type = (char *)cps_api_object_attr_data_bin(type_attr);
    }
    EV_LOGGING(INTERFACE, INFO, "NAS-INT-GET", "GET request received for vlan sub interface or vxlan interface type");
    cps_api_object_attr_t ifix = cps_api_object_attr_get(filt,
                                      DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
    cps_api_object_attr_t name = cps_api_get_key_data(filt,IF_INTERFACES_INTERFACE_NAME);

    if (ifix == nullptr && name != NULL) {
        interface_ctrl_t _if;
        memset(&_if,0,sizeof(_if));
        strncpy(_if.if_name,(const char *)cps_api_object_attr_data_bin(name),sizeof(_if.if_name)-1);
        _if.q_type = HAL_INTF_INFO_FROM_IF_NAME;
        if (dn_hal_get_interface_info(&_if)!=STD_ERR_OK)  {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT-GET", "Wrong interface name or interface not present %s",_if.if_name);
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

    if (nas_os_get_interface(filt,param->list)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Failed to get interfaces from OS");
        return cps_api_ret_code_ERR;
    }
    size_t mx = cps_api_object_list_size(param->list);
    size_t ix = 0;
    cps_api_return_code_t rc =  cps_api_ret_code_OK;
    while (ix < mx) {
        cps_api_object_t object = cps_api_object_list_get(param->list,ix);
        cps_api_key_from_attr_with_qual(cps_api_object_key(object), DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                cps_api_qualifier_TARGET);
        cps_api_object_attr_add(object,IF_INTERFACES_INTERFACE_TYPE,
                                                (const void *)req_if_ietf_type, strlen(req_if_ietf_type)+1);
        if (req_if_ietf_type != nullptr) {
           if (strncmp(req_if_ietf_type,IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_BASE_IF_VXLAN,
                                   strlen(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_BASE_IF_VXLAN)) == 0) {
               rc = nas_vxlan_add_cps_attr_for_interface(object);
               if (rc  != cps_api_ret_code_OK) {
                  return rc;
               }
            }
        }
        ix++;
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


t_std_error nas_interface_generic_init(cps_api_operation_handle_t handle)  {

    if (intf_obj_handler_registration(obj_INTF, nas_int_type_VLANSUB_INTF, nas_interface_com_if_get,
                nas_interface_com_if_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT", "Failed to register VLAN SUB interface CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return STD_ERR_OK;
}
