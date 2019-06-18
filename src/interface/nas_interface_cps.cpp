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
 * filename: nas_interface_cps.cpp
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
#include "nas_os_lpbk.h"
#include "nas_int_com_utils.h"
#include "ds_api_linux_interface.h"


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

cps_api_return_code_t _nas_interface_fill_obj(cps_api_object_t filt, cps_api_object_list_t *list, std::string name, bool get_state){

        NAS_INTERFACE *nas_intf = nas_interface_map_obj_get(name);

        if (nas_intf == nullptr) {
            EV_LOGGING(INTERFACE,ERR,"NAS-IF","Interface get failure : Could not get interface object");
            return cps_api_ret_code_ERR;
        }
        cps_api_return_code_t rc = cps_api_ret_code_OK;
        hal_ifindex_t ifix= nas_intf->get_ifindex();
        char *req_if_ietf_type = NULL;
        if (nas_intf->intf_type_get() == nas_int_type_VLANSUB_INTF or nas_intf->intf_type_get() == nas_int_type_VXLAN){

            /* If valid ifindex get interface info from OS */
            if (ifix !=  NAS_IF_INDEX_INVALID){
                /* Add ifindex in the filters list */
                cps_api_object_attr_add_u32(filt, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, ifix);
                if (nas_os_get_interface(filt,*list)!=STD_ERR_OK) {
                    EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Failed to get interfaces from OS");
                    return cps_api_ret_code_ERR;
                }
                cps_api_object_attr_delete(filt,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);

                size_t mx = cps_api_object_list_size(*list);
                cps_api_object_t object = cps_api_object_list_get(*list, (mx-1));

                if (object != NULL){
                    /* Add type attribute */
                    char if_ietf_type[256];
                    if (nas_to_ietf_if_type_get(nas_intf->intf_type_get(), if_ietf_type, sizeof(if_ietf_type))) {
                        req_if_ietf_type = if_ietf_type;
                        cps_api_object_attr_add(object, IF_INTERFACES_INTERFACE_TYPE,(const void *)req_if_ietf_type,strlen(req_if_ietf_type) + 1);
                    }
                    /* if vxlan type */
                    if (nas_intf->intf_type_get() == nas_int_type_VXLAN){
                        if (nas_vxlan_add_cps_attr_for_interface(object) != cps_api_ret_code_OK) {
                            EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Failed to add vxlan attributes");
                            return cps_api_ret_code_ERR;;
                         }
                    }
                    if (get_state) {
                        cps_convert_to_state_attribute(object);
                    }
                }

            /* If invalid ifindex, get from interface cache */
            } else {

                cps_api_object_t obj = cps_api_object_create();
                cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, (get_state) ? cps_api_qualifier_OBSERVED : cps_api_qualifier_TARGET);
                /* fill obj from interface cache */
                if (nas_intf->nas_interface_fill_info(obj) == cps_api_ret_code_OK) {
                    if (get_state) {
                        cps_convert_to_state_attribute(obj);
                    }
                    if (!cps_api_object_list_append(*list, obj)){
                        EV_LOGGING(INTERFACE,INFO,"NAS-IF","Failed to add interface obj to list");
                        rc = cps_api_ret_code_ERR;
                    }
                }
                if (rc == cps_api_ret_code_ERR){
                    cps_api_object_delete(obj);
                    return rc;
                }
            }
        }
    return cps_api_ret_code_OK;
}

/* Get from interface cache or os, required for vxlan and vlan_sub_interfaces */
cps_api_return_code_t nas_interface_if_get(void * context, cps_api_get_params_t * param, size_t key_ix, bool get_state) {
    cps_api_object_attr_t type_attr;
    cps_api_object_attr_t name_attr;
    cps_api_object_attr_t ifix_attr;
    EV_LOGGING(INTERFACE, DEBUG, "NAS-VlanSubIntf/VxLAN cps get", "nas_interface_if_get");
    cps_api_object_t filt = cps_api_object_list_get(param->filters,key_ix);
    if (get_state) {
        ifix_attr = cps_api_object_attr_get(filt, IF_INTERFACES_STATE_INTERFACE_IF_INDEX);
        if (ifix_attr != nullptr) {
            cps_api_object_attr_add_u32(filt,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,
                                     cps_api_object_attr_data_u32(ifix_attr));
            cps_api_object_attr_delete(filt,IF_INTERFACES_STATE_INTERFACE_IF_INDEX);
        }
        name_attr = cps_api_get_key_data(filt, IF_INTERFACES_STATE_INTERFACE_NAME);
        type_attr = cps_api_object_attr_get(filt, IF_INTERFACES_STATE_INTERFACE_TYPE);
    } 
    else {
        ifix_attr = cps_api_object_attr_get(filt, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
        name_attr = cps_api_get_key_data(filt, IF_INTERFACES_INTERFACE_NAME);
        type_attr = cps_api_object_attr_get(filt, IF_INTERFACES_INTERFACE_TYPE);
    }
    /*Get exact based on valid ifindex*/
    if (ifix_attr != nullptr){
        hal_ifindex_t ifix = cps_api_object_attr_data_u32(ifix_attr); 
        if(ifix !=  NAS_IF_INDEX_INVALID){
            interface_ctrl_t intf_ctrl;
            memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));
            intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
            intf_ctrl.if_index = ifix;
            if (dn_hal_get_interface_info(&intf_ctrl) != STD_ERR_OK){
                EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Interface for given ifix not present in cntrl block");
            }
            std::string _name = intf_ctrl.if_name;
            if (_nas_interface_fill_obj(filt, &param->list, _name, get_state) != cps_api_ret_code_OK){
                EV_LOGGING(INTERFACE,DEBUG,"NAS-INT", "Interface not in cache %s", _name); 
            }
             return cps_api_ret_code_OK;
        }
    }
    /* Get exact based on name */
    if (name_attr != nullptr ) {
        EV_LOGGING(INTERFACE,INFO, "NAS-INTF", "cps get sub intf from cache based on given name");
        std::string name = std::string((const char *)cps_api_object_attr_data_bin(name_attr));
        if (_nas_interface_fill_obj(filt, &param->list, name, get_state) != cps_api_ret_code_OK){
            EV_LOGGING(INTERFACE,DEBUG,"NAS-INT", "Interface not in cache %s", name);
            return cps_api_ret_code_ERR;
        }
        return cps_api_ret_code_OK;
    }
    /* Get all based on type */
    char *req_if_ietf_type = NULL;
    if (type_attr != nullptr) {
        req_if_ietf_type = (char *)cps_api_object_attr_data_bin(type_attr);
        for (const std::pair<std::string, class NAS_INTERFACE *> & intf_obj : nas_interface_obj_map_get())
        {
            nas_int_type_t nas_if_type=intf_obj.second->intf_type_get();
            char if_ietf_type[256];
            /* Convert to ietf type */
            if (!nas_to_ietf_if_type_get(nas_if_type, if_ietf_type, sizeof(if_ietf_type))) {
                EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Failed to get IETF interface type for nas_int_type id %d", nas_if_type);
                return cps_api_ret_code_ERR;
            }
            /* Check if requested interface type */
            if(strncmp(if_ietf_type,req_if_ietf_type, sizeof(if_ietf_type)) != 0) {
                continue;
            }
            std::string name = intf_obj.second->get_ifname(); 
            if (_nas_interface_fill_obj(filt, &param->list, name, get_state) != cps_api_ret_code_OK){
                EV_LOGGING(INTERFACE,DEBUG,"NAS-INT", "Interface not in cache %s", name);
                return cps_api_ret_code_ERR;
            }
        }
        return cps_api_ret_code_OK;
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

    /* Don't create just save the subinterface */
    if(nas_interface_vlan_subintf_create(vlan_sub_intf_name,vlan_id,parent_intf_name,false) == STD_ERR_OK){
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

/*
 * Sends out a notification of the operational state.
 * This creates and publishes an event, which locks the
 * the value of interface status (IF_INTERFACES_INTERFACE_ENABLED)
 * to operational status (IF_INTERFACES_STATE_INTERFACE_OPER_STATUS).
 */
static t_std_error nas_interface_lpbk_oper_state_send(cps_api_object_t obj)
{
    char                  buff[CPS_API_MIN_OBJ_LEN];
    cps_api_object_t      new_obj = cps_api_object_init(buff, sizeof(buff));
    unsigned int          state = 0;
    cps_api_object_attr_t attr;
    char                 *lpbk_name = NULL;

    if (!cps_api_key_from_attr_with_qual(cps_api_object_key(new_obj),
                DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_OBJ,
                cps_api_qualifier_OBSERVED)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-OS-LPBK", "Could not set key for logical interface");
        return (STD_ERR(NAS_OS, FAIL, 0));
    }

    /* set the object key (interface name) */
    attr = cps_api_object_attr_get(obj, IF_INTERFACES_INTERFACE_NAME);
    if (attr == CPS_API_ATTR_NULL) {
        EV_LOGGING(NAS_OS, ERR, "NAS-OS-LPBK", "Missing required loopback name for state update");
        return (STD_ERR(NAS_OS, FAIL, 0));
    }

    lpbk_name = (char*)cps_api_object_attr_data_bin(attr);

    cps_api_set_key_data(new_obj, IF_INTERFACES_STATE_INTERFACE_NAME,
        cps_api_object_ATTR_T_BIN, lpbk_name, strlen(lpbk_name)+1);

    /* set the operational state (from interface-enable) */
    attr = cps_api_object_attr_get(obj, IF_INTERFACES_INTERFACE_ENABLED);
    if (attr == CPS_API_ATTR_NULL) {
        EV_LOGGING(NAS_OS, ERR, "NAS-OS-LPBK", "Missing required interface state for update");
        return (STD_ERR(NAS_OS, FAIL, 0));
    }

    state = cps_api_object_attr_data_uint(attr);

    /* INTERFACE_ENABLED is boolean, OPER_STATUS is enumerated */
    cps_api_object_attr_add_u32(new_obj, IF_INTERFACES_STATE_INTERFACE_OPER_STATUS, 
            (state ? IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_UP : IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_DOWN));

    EV_LOGGING(INTERFACE, DEBUG, "NAS-OS_LPBK",
               "oper-status event notification for %s interface: interface is %s", lpbk_name,
                state ? "up" : "down ");

    /* publish the object */
    hal_interface_send_event(new_obj);

    return cps_api_ret_code_OK;
}

static t_std_error nas_interface_lpbk_set(cps_api_object_t obj)
{
    cps_api_object_attr_t   attr;
    t_std_error             res = cps_api_ret_code_OK;
    cps_api_object_it_t     it;
    char                    *lpbk_name = NULL;

    attr = cps_api_object_attr_get(obj, IF_INTERFACES_INTERFACE_NAME);
    if (attr == CPS_API_ATTR_NULL) {
        EV_LOGGING(NAS_OS, ERR, "NAS-OS-LPBK", "Missing loopback name for adding to kernel");
        return (STD_ERR(NAS_OS, FAIL, 0));
    } else {
        lpbk_name = (char*)cps_api_object_attr_data_bin(attr);
    }

    hal_ifindex_t ifix = cps_api_interface_name_to_if_index(lpbk_name);
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, ifix);

    cps_api_object_it_begin(obj, &it);
    for ( ; cps_api_object_it_valid(&it) ; cps_api_object_it_next(&it) ) {
        int id = (int)cps_api_object_attr_id(it.attr);
        switch (id) {
            case DELL_IF_IF_INTERFACES_INTERFACE_MTU:
                    res = nas_os_interface_set_attribute(obj, DELL_IF_IF_INTERFACES_INTERFACE_MTU);
                    break;
            case IF_INTERFACES_INTERFACE_ENABLED:
                res = nas_os_interface_set_attribute(obj, IF_INTERFACES_INTERFACE_ENABLED);
                break;
            case DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS:
                res = nas_os_interface_set_attribute(obj, DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS);
                break;
            case IF_INTERFACES_INTERFACE_NAME:
                res = nas_os_interface_set_attribute(obj, IF_INTERFACES_INTERFACE_NAME);
                break;
            default:
                EV_LOGGING(NAS_OS, ERR, "NAS-OS-LPBK", "unsupported set attribute, %d", id);
                break;
        }
    }

    return res;
}


/* 
 * Create a loopback interface.  
 * The create operation involves creating the interface, then setting all the attributes in the object
 */
static t_std_error nas_interface_lpbk_create(cps_api_object_t obj)
{
    t_std_error res = cps_api_ret_code_OK;

    res = nas_os_lpbk_create(obj);
    if (res == cps_api_ret_code_OK) {
        res = nas_interface_lpbk_set(obj);
    }

    return res;
}

static cps_api_return_code_t nas_interface_lpbk_if_set(void* context, 
        cps_api_transaction_params_t* param, size_t ix)
{
    cps_api_return_code_t res = cps_api_ret_code_ERR;
    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    switch (op) {
        case cps_api_oper_CREATE:
            res = nas_interface_lpbk_create(obj);
            break;
        case cps_api_oper_DELETE:
            res = nas_os_lpbk_delete(obj);
            break;
        case cps_api_oper_SET:
            res = nas_interface_lpbk_set(obj);
            break;
        default:
            EV_LOGGING(INTERFACE, ERR, "NAS-LPBK-SUB-INTF", "(loopback) operation not supported (%d)", op);
            break;
    }

    /* notify HAL of changes */
    nas_intf_event_hal_update(obj);

    /* notify (publish) listenting clients of changes */
    nas_intf_event_publish(obj);

    /* update operational status */
    nas_interface_lpbk_oper_state_send(obj);

    return res;
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


cps_api_return_code_t nas_interface_if_get_handler (void * context, cps_api_get_params_t * param,
        size_t key_ix) {
    return nas_interface_if_get (context, param, key_ix, false);
}

cps_api_return_code_t nas_interface_com_if_state_get_handler (void * context, cps_api_get_params_t * param,
        size_t key_ix) {
    return nas_interface_com_if_get (context, param, key_ix, true);
}

cps_api_return_code_t nas_interface_if_state_get_handler (void * context, cps_api_get_params_t * param,
        size_t key_ix) {
    return nas_interface_if_get (context, param, key_ix, true);
}
cps_api_return_code_t nas_interface_com_if_state_set_handler(void* context, cps_api_transaction_params_t* param, size_t ix) {
    return cps_api_ret_code_ERR;
}

t_std_error nas_interface_generic_init(cps_api_operation_handle_t handle)  {

    if (intf_obj_handler_registration(obj_INTF, nas_int_type_VLANSUB_INTF, nas_interface_if_get_handler,
                nas_interface_com_if_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-INIT", "Failed to register VLAN SUB interface CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (intf_obj_handler_registration(obj_INTF_STATE, nas_int_type_VLANSUB_INTF, nas_interface_if_state_get_handler,
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

    if (intf_obj_handler_registration(obj_INTF, nas_int_type_LPBK, 
                nas_interface_com_if_get_handler, nas_interface_lpbk_if_set) != STD_ERR_OK) {
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
