
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
 * filename: nas_interface_bridge.cpp
 */
#include "iana-if-type.h"
#include "dell-interface.h"
#include "dell-base-if-vlan.h"
#include "dell-base-if.h"
#include "bridge-model.h"
#include "bridge/nas_interface_bridge.h"
#include "interface/nas_interface_vlan.h"
#include "interface/nas_interface_map.h"
#include "interface/nas_interface_utils.h"
#include "bridge/nas_interface_bridge_com.h"
#include "nas_os_interface.h"


bool NAS_BRIDGE::nas_bridge_tagged_member_present(void) {
    if (tagged_members.empty()) { return false;}
    return true;
}

bool NAS_BRIDGE::nas_bridge_untagged_member_present(void) {
    if (untagged_members.empty()) { return false;}
    return true;
}

t_std_error NAS_BRIDGE::nas_bridge_check_tagged_membership(std::string mem_name, bool *present)
{
    *present = false;
    auto itr = tagged_members.find(mem_name);
    if(itr != tagged_members.end()){
        *present = true;
        return STD_ERR_OK;
    }
    return STD_ERR_OK;
}

t_std_error NAS_BRIDGE::nas_bridge_check_untagged_membership(std::string mem_name, bool *present)
{
    *present = false;
    auto itr = untagged_members.find(mem_name);
    if(itr != untagged_members.end()){
        *present = true;
        return STD_ERR_OK;
    }
    return STD_ERR_OK;
}

t_std_error NAS_BRIDGE::nas_bridge_check_membership(std::string mem_name, bool *present) {

    if ((nas_bridge_check_untagged_membership(mem_name, present) == STD_ERR_OK) && (*present)) {
        return STD_ERR_OK;
    } else if ((nas_bridge_check_tagged_membership(mem_name, present) == STD_ERR_OK) && (*present)) {
        return STD_ERR_OK;
    } else {
        *present = false;
    }
    return STD_ERR_OK;
}


t_std_error NAS_BRIDGE::nas_bridge_add_vlan_in_attached_list(std::string mem_name)
{
    try {
        attached_vlans.insert(mem_name);
    } catch (std::exception& e) {
        EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " Failed to add vlan member in the attached vlan list %s", e.what());
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return STD_ERR_OK;
}

t_std_error NAS_BRIDGE::nas_bridge_add_tagged_member_in_list(std::string mem_name)
{
    try {
        tagged_members.insert(mem_name);
    } catch (std::exception& e) {
        EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " Failed to add tagged member in the list %s", e.what());
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return STD_ERR_OK;
}

t_std_error NAS_BRIDGE::nas_bridge_add_untagged_member_in_list(std::string mem_name)
{
    try {
        untagged_members.insert(mem_name);
    } catch (std::exception& e) {
        EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " Failed to add untagged member in the list %s", e.what());
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return STD_ERR_OK;
}

t_std_error NAS_BRIDGE::nas_bridge_remove_vlan_member_from_attached_list(std::string mem_name)
{
    auto it = attached_vlans.find(mem_name);
    if(it != attached_vlans.end()){
        attached_vlans.erase(it);
        return STD_ERR_OK;
    }

    EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " Failed to remove member %s from the "
                "attached vlan list ",mem_name.c_str());
    return STD_ERR(INTERFACE, FAIL, 0);
}

t_std_error NAS_BRIDGE::nas_bridge_remove_tagged_member_from_list(std::string mem_name)
{
    auto it = tagged_members.find(mem_name);
    if(it != tagged_members.end()){
        tagged_members.erase(it);
        return STD_ERR_OK;
    }
    EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " Failed to remove tagged member %s from the "
                    "list ",mem_name.c_str());
    return STD_ERR(INTERFACE, FAIL, 0);
}

t_std_error NAS_BRIDGE::nas_bridge_remove_untagged_member_from_list(std::string mem_name)
{
    auto it  = untagged_members.find(mem_name);
    if(it != untagged_members.end()){
        untagged_members.erase(it);
        return STD_ERR_OK;
    }
    EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " Failed to remove untagged member  %s from the "
                    "list ",mem_name.c_str());
    return STD_ERR(INTERFACE, FAIL, 0);
}

void NAS_BRIDGE::nas_bridge_for_each_member(std::function <void (std::string mem_name, nas_port_mode_t port_mode)> fn)
{
    if (!tagged_members.empty()) {
        for (auto it = tagged_members.begin(); it != tagged_members.end(); ++it) {
            std::string _name = *it;
            // TODO check type of function parameter
            fn(_name, NAS_PORT_TAGGED);
        }
    }
    if (!untagged_members.empty()) {
        for (auto it = untagged_members.begin(); it != untagged_members.end(); ++it) {
            // TODO check type of function parameter
            std::string _name = *it;
            fn(_name, NAS_PORT_UNTAGGED);
        }
    }
}
t_std_error NAS_BRIDGE::nas_bridge_get_member_list(nas_port_mode_t port_mode, memberlist_t &m_list)
{
    memberlist_t *_list = NULL;
    if (port_mode == NAS_PORT_TAGGED) {
        _list  = &tagged_members;
    } else {
        _list  = &untagged_members;
    }
    if (!_list->empty()) {
        for (auto it = _list->begin(); it != _list->end(); ++it) {
            m_list.insert(*it);
        }
    }
    return STD_ERR_OK;
}

t_std_error NAS_BRIDGE::nas_bridge_update_member_list(std::string &mem_name, nas_port_mode_t port_mode, bool add_member)
{
    t_std_error rc = STD_ERR_OK;
    if ( port_mode == NAS_PORT_TAGGED) {
        if (add_member) {
            rc = nas_bridge_add_tagged_member_in_list(mem_name);
        } else {
            rc = nas_bridge_remove_tagged_member_from_list(mem_name);
        }
    } else if (port_mode == NAS_PORT_UNTAGGED) {
        if (add_member) {
            rc = nas_bridge_add_untagged_member_in_list(mem_name);
        } else {
            rc= nas_bridge_remove_untagged_member_from_list(mem_name);
        }
    } else {
        EV_LOGGING(INTERFACE, ERR, "NAS-BRIDGE", " Wrong Port mode type %d", port_mode);
        rc =  STD_ERR(INTERFACE, FAIL, 0);
    }
    return rc;
}

t_std_error NAS_BRIDGE::nas_bridge_update_member_list(memberlist_t &memlist, nas_port_mode_t port_mode, bool add_member)
{
    t_std_error rc = STD_ERR_OK;
    for ( auto mem : memlist) {
        if ((rc = nas_bridge_update_member_list(mem, port_mode, add_member)) != STD_ERR_OK) {
            return rc;
        }
    }
    return rc;
}
bool NAS_BRIDGE::nas_bridge_multiple_vlans_present(void)
{
    // for all tagged members check if vlan differs
    if (!nas_bridge_tagged_member_present()) {
        // If no tagged member present then return false
        return false;
    }
    NAS_VLAN_INTERFACE *vlan_intf = nullptr;
    auto it = tagged_members.begin();
    vlan_intf = (NAS_VLAN_INTERFACE *)nas_interface_map_obj_get(*it);
    hal_vlan_id_t vlan_id = vlan_intf->vlan_id;
    for (; it != tagged_members.end(); it++) {
        vlan_intf = (NAS_VLAN_INTERFACE *)nas_interface_map_obj_get(*it);
        if (vlan_id != vlan_intf->vlan_id) {
            return false;
        }
    }
    return true;
}

// Fill common bridge attributes in the CPS object
cps_api_return_code_t NAS_BRIDGE::nas_bridge_fill_bridge_model_com_info(cps_api_object_t obj)
{
    if(obj == nullptr) {
        return  cps_api_ret_code_ERR;
    }
    cps_api_set_key_data(obj,BRIDGE_DOMAIN_BRIDGE_NAME,
                           cps_api_object_ATTR_T_BIN, bridge_name.c_str(), strlen(bridge_name.c_str())+1);
    cps_api_object_attr_add_u32(obj,BRIDGE_DOMAIN_BRIDGE_MODE, bridge_mode);
    // Add tagged and untagged members name
    if (!tagged_members.empty()) {
        for (auto it = tagged_members.begin(); it != tagged_members.end(); ++it) {
            cps_api_object_attr_add(obj, BRIDGE_DOMAIN_BRIDGE_MEMBER_INTERFACE, it->c_str(),
                    strlen(it->c_str())+1);
        }
        if (!untagged_members.empty()) {
            for (auto it = untagged_members.begin(); it != untagged_members.end(); ++it) {
                cps_api_object_attr_add(obj, BRIDGE_DOMAIN_BRIDGE_MEMBER_INTERFACE, it->c_str(),
                        strlen(it->c_str())+1);
            }
        }
        return cps_api_ret_code_OK;
    }

    return cps_api_ret_code_OK;
}
cps_api_return_code_t NAS_BRIDGE::nas_bridge_fill_vlan_intf_model_com_info(cps_api_object_t obj)
{
    if(obj == nullptr) {
        return  cps_api_ret_code_ERR;
    }
    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,
                           cps_api_object_ATTR_T_BIN, bridge_name.c_str(), strlen(bridge_name.c_str())+1);
    cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, get_bridge_intf_index());
    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
                            (const void *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
                            strlen(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN)+1);
    // Add tagged and untagged members name
    if (!tagged_members.empty()) {
        for (auto it = tagged_members.begin(); it != tagged_members.end(); ++it) {
            std::string parent_intf;
            if (nas_interface_utils_parent_name_get((std::string &)*it, parent_intf) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE,ERR,"NAS-IF","Could not translate to parent interface for %s ", it->c_str());
                continue;
            }
            cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS, parent_intf.c_str(),
                    strlen(parent_intf.c_str())+1);
        }
    }
    if (!untagged_members.empty()) {
        for (auto it = untagged_members.begin(); it != untagged_members.end(); ++it) {
            cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS, it->c_str(),
                    strlen(it->c_str())+1);
        }
    }
    cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_LEARNING_MODE, get_bridge_mac_learn_mode());
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, get_bridge_intf_index());
    return cps_api_ret_code_OK;
}
cps_api_return_code_t NAS_BRIDGE::nas_bridge_fill_com_info(cps_api_object_t obj)
{

    if (get_bridge_model() == BRIDGE_MODEL) {
        return nas_bridge_fill_bridge_model_com_info(obj);
    } else {
        return nas_bridge_fill_vlan_intf_model_com_info(obj);
    }
    return  cps_api_ret_code_OK;
}

// Publish bridge create/delete event with bridge name, bridge mode and operation type.
void NAS_BRIDGE::nas_bridge_publish_event(cps_api_operation_types_t op)
{
    cps_api_object_guard og(cps_api_object_create());

    if (!cps_api_key_from_attr_with_qual(cps_api_object_key(og.get()),
                BRIDGE_DOMAIN_BRIDGE_OBJ, cps_api_qualifier_OBSERVED)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF","Could not translate to logical interface key ");
        return;
    }
    cps_api_object_set_type_operation(cps_api_object_key(og.get()), op);
    cps_api_set_key_data(og.get(),BRIDGE_DOMAIN_BRIDGE_NAME,
            cps_api_object_ATTR_T_BIN, bridge_name.c_str(), strlen(bridge_name.c_str())+1);
    cps_api_object_attr_add_u32(og.get(),BRIDGE_DOMAIN_BRIDGE_MODE, bridge_mode);

    cps_api_event_thread_publish(og.get());
}

void NAS_BRIDGE::nas_bridge_publish_member_event(std::string &mem_name, cps_api_operation_types_t op)
{
    cps_api_object_guard og(cps_api_object_create());

    // ??? from here until ???END lines may have been inserted/deleted
    if (!cps_api_key_from_attr_with_qual(cps_api_object_key(og.get()), BRIDGE_DOMAIN_BRIDGE_OBJ, cps_api_qualifier_OBSERVED)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF","Could not translate to logical interface key ");
        return;
    }
    cps_api_object_set_type_operation(cps_api_object_key(og.get()), op);
    cps_api_set_key_data(og.get(),BRIDGE_DOMAIN_BRIDGE_NAME,
            cps_api_object_ATTR_T_BIN, bridge_name.c_str(), strlen(bridge_name.c_str())+1);
    cps_api_object_attr_add(og.get(), BRIDGE_DOMAIN_BRIDGE_MEMBER_INTERFACE,
            mem_name.c_str(), strlen(mem_name.c_str())+1);


    cps_api_event_thread_publish(og.get());
}

void NAS_BRIDGE::nas_bridge_publish_memberlist_event(memberlist_t &memlist, cps_api_operation_types_t op)
{
    cps_api_object_guard og(cps_api_object_create());

    // ??? from here until ???END lines may have been inserted/deleted
    if (!cps_api_key_from_attr_with_qual(cps_api_object_key(og.get()), BRIDGE_DOMAIN_BRIDGE_OBJ, cps_api_qualifier_OBSERVED)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF","Could not translate to logical interface key ");
        return;
    }
    cps_api_object_set_type_operation(cps_api_object_key(og.get()), op);
    cps_api_set_key_data(og.get(),BRIDGE_DOMAIN_BRIDGE_NAME,
            cps_api_object_ATTR_T_BIN, bridge_name.c_str(), strlen(bridge_name.c_str())+1);
    for (auto mem : memlist) {
        cps_api_object_attr_add(og.get(), BRIDGE_DOMAIN_BRIDGE_MEMBER_INTERFACE,
            mem.c_str(), strlen(mem.c_str())+1);
    }


    cps_api_event_thread_publish(og.get());
}

t_std_error NAS_BRIDGE::nas_bridge_os_add_remove_member(std::string & mem_name, nas_port_mode_t port_mode, bool add)
{
    t_std_error rc = STD_ERR_OK;
    if ((nas_g_scaled_vlan_get() == true) && (bridge_l3_mode_get() != BASE_IF_MODE_MODE_L3)) {
        // Nothing to do in case of non L3 mode
        return rc;
    }

    /*  check if the interface exist  */
    // if not then first create

    cps_api_object_guard _og(cps_api_object_create());
    if(!_og.valid()){
        EV_LOGGING(INTERFACE,ERR,"INT-DB-GET","Failed to create object  member addition");
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    cps_api_object_attr_add(_og.get(),IF_INTERFACES_INTERFACE_NAME,
                                get_bridge_name().c_str(),
                               strlen(get_bridge_name().c_str())+1);
    cps_api_attr_id_t port_mode_id = DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS;
    hal_vlan_id_t vlan_id =0;
    if(port_mode == NAS_PORT_TAGGED) {
        // check if sub interface exists
        // TODO crate if sub interface is required
        port_mode_id = DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS;
        std::string parent;
        hal_ifindex_t ifindex;
        rc = nas_interface_utils_parent_name_get(mem_name, parent);
        rc = nas_interface_utils_vlan_id_get(mem_name, vlan_id);
        rc = nas_interface_utils_ifindex_get(mem_name,ifindex);
        if (rc != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"INT-DB-GET","Member %s does not exist member", mem_name.c_str());
            return rc;
        }
        cps_api_object_attr_add(_og.get(),DELL_IF_IF_INTERFACES_INTERFACE_PARENT_INTERFACE, parent.c_str(), strlen(parent.c_str())+1);
        cps_api_object_attr_add_u32(_og.get(),DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,ifindex);
    }
    // TODO Though vlan id not needed for adding member
    cps_api_object_attr_add_u32(_og.get(),BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, vlan_id);

    // Add member Name
    cps_api_object_attr_add(_og.get(), port_mode_id, mem_name.c_str(), strlen(mem_name.c_str())+1);
    if (add) {
        if ((rc = nas_os_add_intf_to_bridge(_og.get())) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"INT-DB-GET","Failed to add member %s to the bridge %s in the kernel",
                                    mem_name.c_str(), get_bridge_name().c_str() );
        }
    } else  {
        if ((rc = nas_os_del_intf_from_bridge(_og.get())) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"INT-DB-GET","Failed to delete member %s from the bridge %s  in the kernel",
                                    mem_name.c_str(), get_bridge_name().c_str() );
        }
    }
    return rc;
}
t_std_error NAS_BRIDGE::nas_bridge_os_add_remove_memberlist(memberlist_t & memlist, nas_port_mode_t port_mode, bool add)
{

    for (auto mem : memlist ) {
       nas_bridge_os_add_remove_member(mem, port_mode, add);
       // TODO May need to add check error for rollback
    }
    return STD_ERR_OK;
}


t_std_error NAS_BRIDGE::nas_bridge_set_admin_status(cps_api_object_t obj, cps_api_object_it_t & it)
{

    t_std_error rc;

    cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, if_index);

    if((rc = nas_os_interface_set_attribute(obj,IF_INTERFACES_INTERFACE_ENABLED)) != STD_ERR_OK)
    {
        EV_LOGGING(INTERFACE, ERR ,"BRIDGE-ADMIN","Failed to set Admin status for bridge %s",bridge_name.c_str());
        return rc;
    }

    admin_status = ((bool)cps_api_object_attr_data_u32(it.attr) == true)?
        IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS_UP : IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS_DOWN;

    return rc;
}


t_std_error NAS_BRIDGE::nas_bridge_set_mac_address(cps_api_object_t obj, cps_api_object_it_t & it)
{

    t_std_error rc;

    cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, if_index);

    if((rc = nas_os_interface_set_attribute(obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR ,"BRIDGE-ADMIN","Failed to set MAC address for bridge %s",bridge_name.c_str());
        return rc;
    }

    mac_addr = std::string((char *) cps_api_object_attr_data_bin(it.attr));

    if ((rc = dn_hal_update_intf_mac(if_index,mac_addr.c_str())) != STD_ERR_OK)  {
       EV_LOGGING(INTERFACE, ERR ,"BRIDGE-MAC", "Failure updating MAC address for bridge %s in interface cache",
                       bridge_name.c_str());

   }
   return rc;

}




t_std_error NAS_BRIDGE::nas_bridge_set_mtu(cps_api_object_t obj, cps_api_object_it_t  & it)
{

    uint32_t mtu = cps_api_object_attr_data_u32(it.attr);
    t_std_error rc;
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, if_index);

    for(auto member : tagged_members){
        NAS_VLAN_INTERFACE * intf = dynamic_cast<NAS_VLAN_INTERFACE *>(nas_interface_map_obj_get(member));
        if(intf){
            if((rc = intf->set_mtu(mtu))!=STD_ERR_OK){
                return rc;
            }
        }
    }
    this->mtu = mtu;

    return cps_api_ret_code_OK;
}


t_std_error NAS_BRIDGE::nas_bridge_set_attribute(cps_api_object_t obj , cps_api_object_it_t & it){

    std::unordered_map<cps_api_attr_id_t,t_std_error (NAS_BRIDGE::*)(cps_api_object_t, cps_api_object_it_t & )> set_bridge_attrs =
    {
        { IF_INTERFACES_INTERFACE_ENABLED,&NAS_BRIDGE::nas_bridge_set_admin_status },
        { DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS, &NAS_BRIDGE::nas_bridge_set_mac_address},
        { DELL_IF_IF_INTERFACES_INTERFACE_MTU, &NAS_BRIDGE::nas_bridge_set_mtu},
    };

    auto attr_it = set_bridge_attrs.find(cps_api_object_attr_id(it.attr));
    if(attr_it == set_bridge_attrs.end()){
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE-ATTR-SET","Failed to find the handler for bridge attribute %d",it.attr);
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return (this->*(attr_it->second))(obj,it);
}
