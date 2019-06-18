
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
 * filename: .cpp
 */

#include "interface/nas_interface.h"
#include "dell-base-if.h"
#include "dell-base-common.h"
#include "dell-interface.h"
#include "event_log_types.h"
#include "event_log.h"
#include "ds_common_types.h"
#include "nas_os_interface.h"
#include "cps_api_object_key.h"


cps_api_return_code_t NAS_INTERFACE::nas_interface_fill_com_info(cps_api_object_t obj)
{
    // Add common attributes like if index, mtu phy address
    if(obj == nullptr) {
        return  cps_api_ret_code_ERR;
    }
    EV_LOGGING(INTERFACE,INFO,"NAS-INT","nas_interface_fill_com_info");
    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_NAME, if_name.c_str(), strlen(if_name.c_str())+1);
    cps_api_object_attr_add_u32(obj,DELL_IF_IF_INTERFACES_INTERFACE_MTU, mtu);
    cps_api_object_attr_add_u32(obj,IF_INTERFACES_STATE_INTERFACE_IF_INDEX, if_index);
    char if_ietf_type[256];
    /*Get ietf type*/
    if (nas_to_ietf_if_type_get(if_type, if_ietf_type, sizeof(if_ietf_type))) {
    cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_TYPE, (const char *)if_ietf_type,
                            strlen(if_ietf_type) + 1);
    }
    //TODO add phy address attribute in cache
    cps_api_object_attr_add(obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS, mac_addr.c_str(),
                            mac_addr.length()+1);

    return cps_api_ret_code_OK;
}


cps_api_return_code_t NAS_INTERFACE::nas_interface_fill_info(cps_api_object_t obj){
    /*
     *  Class structure needs to be updated. Need only one method to fill interface info
     *  need to instantiate the interface object for lag and physical port
     */
    EV_LOGGING(INTERFACE,INFO,"NAS-INT","nas_interface_fill_info");

    return cps_api_ret_code_OK;
}


t_std_error NAS_INTERFACE::set_mtu(size_t mtu){
    if (get_ifindex() == NAS_IF_INDEX_INVALID) {
        this->mtu = mtu;
        return STD_ERR_OK;
    }
    if (this->mtu == mtu) {
        EV_LOGGING(INTERFACE,DEBUG,"NAS-INT"," MTU %d already set for %s", mtu, if_name.c_str());
        return STD_ERR_OK;
    }
    cps_api_object_guard og(cps_api_object_create());
    if(og.get() == nullptr){
        EV_LOGGING(INTERFACE,ERR,"NAS-INT","No memory to create new object");
        return STD_ERR(INTERFACE,NOMEM,0);
    }
    cps_api_object_attr_add_u32(og.get(), DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, if_index);
    cps_api_object_attr_add_u32(og.get(), DELL_IF_IF_INTERFACES_INTERFACE_MTU, mtu);
    if (nas_os_interface_set_attribute(og.get(),DELL_IF_IF_INTERFACES_INTERFACE_MTU)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT","Failed to set mtu to %d for interface %s",mtu, if_name.c_str());
        return (STD_ERR(INTERFACE,FAIL, 0));
    }
    this->mtu = mtu;
    return STD_ERR_OK;
}
