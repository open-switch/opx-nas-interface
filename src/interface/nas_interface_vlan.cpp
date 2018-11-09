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
 * filename: nas_interface_vlan.cpp
 */

#include "interface/nas_interface_vlan.h"
#include "dell-interface.h"
#include "ds_common_types.h"
#include "event_log_types.h"
#include "event_log.h"
#include "dell-base-if-vlan.h"
#include "nas_os_vlan.h"
#include "nas_os_interface.h"

#include<mutex>

static hal_vlan_id_t default_vlan_id = 1;
static std::mutex _m;

cps_api_return_code_t NAS_VLAN_INTERFACE::nas_interface_fill_info(cps_api_object_t obj)
{
    if (nas_interface_fill_com_info(obj) != cps_api_ret_code_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF","Could not get common interface info");
        return cps_api_ret_code_ERR;
    }
    cps_api_object_attr_add_u32(obj, BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, vlan_id);
    cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_PARENT_INTERFACE,
            parent_intf_name.c_str(), strlen(parent_intf_name.c_str())+1);
    return cps_api_ret_code_OK;
}


t_std_error NAS_VLAN_INTERFACE::set_mtu(size_t mtu){
    cps_api_object_guard og(cps_api_object_create());
    if(og.get() == nullptr){
        EV_LOGGING(INTERFACE,ERR,"NAS-VLAN","No memory to create new object");
        return STD_ERR(INTERFACE,NOMEM,0);
    }
    cps_api_object_attr_add_u32(og.get(), DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, if_index);
    cps_api_object_attr_add_u32(og.get(), DELL_IF_IF_INTERFACES_INTERFACE_MTU, mtu);
    cps_api_object_attr_add_u16(og.get(), BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, vlan_id);
    if (nas_os_interface_set_attribute(og.get(),DELL_IF_IF_INTERFACES_INTERFACE_MTU)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-VLAN","Failed to set mtu to %d for vlan sub interface %s",mtu, if_name.c_str());
        return (STD_ERR(INTERFACE,FAIL, 0));
    }
    this->mtu = mtu;
    return STD_ERR_OK;
}

t_std_error NAS_VLAN_INTERFACE::create_in_os(){
    /*
     * call nas-linux api to create sub interface
     *
     */
    return STD_ERR_OK;
}

t_std_error NAS_VLAN_INTERFACE::delete_from_os(){
    /*
     * call nas-linux api to delete sub interface
     */
    return STD_ERR_OK;
}

void nas_vlan_interface_set_default_vlan(hal_vlan_id_t vlan_id){
    std::lock_guard<std::mutex> l(_m);
    default_vlan_id = vlan_id;
}

bool nas_vlan_interface_get_default_vlan(hal_vlan_id_t * vlan_id){
    std::lock_guard<std::mutex> l(_m);
    *vlan_id = default_vlan_id;
    return true;
}
