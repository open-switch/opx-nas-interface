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
 * nas_lag_api.cpp
 */

#include "dell-base-if-lag.h"
#include "dell-base-if.h"
#include "dell-base-interface-common.h"
#include "dell-interface.h"
#include "nas_ndi_lag.h"
#include "nas_os_lag.h"
#include "nas_os_interface.h"
#include "nas_int_port.h"
#include "cps_api_events.h"
#include "event_log.h"
#include "event_log_types.h"
#include "nas_int_lag_api.h"
#include "nas_int_lag_cps.h"
#include "nas_int_lag.h"
#include "std_mac_utils.h"
#include "nas_ndi_obj_id_table.h"
#include "interface_obj.h"
#include "nas_int_utils.h"
#include "nas_int_logical.h"
#include "hal_interface_common.h"
#include "nas_ndi_port.h"
#include "nas_linux_l2.h"
#include "iana-if-type.h"
#include "nas_if_utils.h"
#include "nas_int_com_utils.h"
#include "interface/nas_interface_lag.h"
#include "interface/nas_interface_map.h"
#include "interface/nas_interface_utils.h"
#include "std_rw_lock.h"

#include <stdio.h>

#include "cps_class_map.h"
#include "cps_api_events.h"
#include "cps_api_object_key.h"
#include <unordered_set>


const static int MAX_CPS_MSG_BUFF=4096;

typedef std::unordered_set <hal_ifindex_t> nas_port_list_t;
static cps_api_return_code_t nas_cps_set_lag(cps_api_object_t obj);
static cps_api_return_code_t nas_cps_delete_port_from_lag(nas_lag_master_info_t *nas_lag_entry, hal_ifindex_t ifindex);
static void nas_cps_update_oper_state(nas_lag_master_info_t *nas_lag_entry);
cps_api_return_code_t lag_state_object_publish(nas_lag_master_info_t *nas_lag_entry,bool oper_status);

static bool nas_lag_get_ifindex_from_obj(cps_api_object_t obj,hal_ifindex_t *index, bool get_state){
    cps_api_object_attr_t lag_attr = cps_api_object_attr_get(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX );
    cps_api_object_attr_t lag_name_attr;

    if (get_state) {
        lag_name_attr = cps_api_get_key_data(obj, IF_INTERFACES_STATE_INTERFACE_NAME);
    } else {
        lag_name_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);
    }

    if(lag_attr == NULL && lag_name_attr == NULL) {
        EV_LOGGING(INTERFACE, DEBUG, "NAS-CPS-LAG",
            "Missing Name/ifindex attribute for CPS Set");
        return false;
    }

    if(lag_attr){
        *index = (hal_ifindex_t) cps_api_object_attr_data_u32(lag_attr);
    }else{
        const char * name = (const char *)cps_api_object_attr_data_bin(lag_name_attr);
        interface_ctrl_t i;
        memset(&i,0,sizeof(i));
        strncpy(i.if_name,name,sizeof(i.if_name)-1);
        i.q_type = HAL_INTF_INFO_FROM_IF_NAME;
        if (dn_hal_get_interface_info(&i)!=STD_ERR_OK){
            EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                       "Can't get interface control information for %s",name);
            return false;
        }
        *index = i.if_index;
    }
    return true;
}

static inline bool nas_get_lag_id_from_str(const char * str, nas_lag_id_t * id){
    std::string full_str(str);
    std::size_t pos = full_str.find_first_of("0123456789");

    if (pos != std::string::npos) {
        std::string id_str = full_str.substr(pos);
        *id = std::stoi(id_str);
        if (*id == INVALID_LAG_ID) {
            EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "%u is reserved as an invalid lag id", INVALID_LAG_ID);
        }
    } else {
        *id = INVALID_LAG_ID;
    }
    return true;
}

static cps_api_return_code_t nas_cps_set_desc(const cps_api_object_attr_t &desc_attr, const hal_ifindex_t lag_index)
{
    /** TODO set LAG description to OS **/
    if (!desc_attr) {
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "Interface description attribute is null.");
        return cps_api_ret_code_ERR;
    }

    const char *desc = (const char *) cps_api_object_attr_data_bin(desc_attr);

    if ((strlen(desc) > MAX_INTF_DESC_LEN))  {
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "lag desc length exceed maximum (%d)", MAX_INTF_DESC_LEN);
        return cps_api_ret_code_ERR;
    }

    if(nas_lag_set_desc(lag_index, desc) != STD_ERR_OK)
    {
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "lag desc failure in NAS");
        return cps_api_ret_code_ERR;
    }

    return cps_api_ret_code_OK;
}


static void _nas_publish_lag_cps_req_obj(cps_api_object_t obj) {

    cps_api_key_set_qualifier(cps_api_object_key(obj), cps_api_qualifier_OBSERVED);
    cps_api_event_thread_publish(obj);
    cps_api_key_set_qualifier(cps_api_object_key(obj), cps_api_qualifier_TARGET);

}
static cps_api_return_code_t nas_cps_create_lag(cps_api_object_t obj)
{
    hal_ifindex_t lag_index = 0;
    cps_api_return_code_t rc =cps_api_ret_code_OK;
    nas_lag_id_t lag_id=0;
    cps_api_object_attr_t lag_id_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);
    cps_api_object_attr_t lag_desc_attr = cps_api_object_attr_get(obj, IF_INTERFACES_INTERFACE_DESCRIPTION);

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "Create Lag using CPS");

    if(lag_id_attr == NULL) {
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "Missing Lag Name");
        return cps_api_ret_code_ERR;
    }

    const char * name = (const char *)cps_api_object_attr_data_bin(lag_id_attr);

    if(nas_lag_get_ifindex_from_obj(obj,&lag_index, false)){
        EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG","Lag %s already exists",
                name);
        return nas_cps_set_lag(obj);
    }

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG","Create Lag %s in kernel", name);

    //Acquring lock to avoid pocessing netlink msg from kernel.
    std_mutex_simple_lock_guard lock_t(nas_lag_mutex_lock());

    if((nas_os_create_lag(obj, &lag_index)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                "Failure creating Lag in Kernel");
        return cps_api_ret_code_ERR;
    }

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
            "Kernel programmed, creating lag with index %d", lag_index);

    nas_get_lag_id_from_str(name,&lag_id);

    if(nas_lag_master_add(lag_index,name,lag_id) != STD_ERR_OK)
    {
        EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
                   "Failure in NAS-CPS-LAG (NPU) creating lag with index %d",
                   lag_index);
        return cps_api_ret_code_ERR;
    }

    if (nas_intf_handle_intf_mode_change(lag_index, BASE_IF_MODE_MODE_NONE) == false) {
        EV_LOGGING(INTERFACE, DEBUG, "NAS-CPS-LAG",
                "Update to NAS-L3 about interface mode change failed(%d)", lag_index);
    }

    cps_api_set_key_data(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,cps_api_object_ATTR_T_U32,
            &lag_index,sizeof(lag_index));

    if(nas_cps_set_lag(obj)!=cps_api_ret_code_OK)
    {
        EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
                   "Failure in NAS-CPS-LAG Set operation in Create Lag index %d",
                   lag_index);
        return cps_api_ret_code_ERR;
    }

    if(lag_desc_attr && nas_cps_set_desc(lag_desc_attr, lag_index) != cps_api_ret_code_OK) {
        EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
                   "Failure in NAS-CPS-LAG Set description for LAG index %d",
                   lag_index);
        return cps_api_ret_code_ERR;
    }
    std::string lag_name = std::string(name);
    /*
     * Register LAG object with interface cache
     */
    NAS_LAG_INTERFACE *l_obj = new NAS_LAG_INTERFACE(lag_name, lag_index, nas_int_type_LAG);

    if(l_obj){
        nas_interface_map_obj_add(lag_name,l_obj);
    }

    EV_LOGGING(INTERFACE, NOTICE, "NAS-CPS-LAG", "Create Lag %s successful", name);
    _nas_publish_lag_cps_req_obj(obj);

    return rc;
}

static cps_api_return_code_t nas_cps_delete_lag(cps_api_object_t obj)
{
    cps_api_return_code_t rc = cps_api_ret_code_OK;

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "CPS Delete LAG");

    hal_ifindex_t lag_index = 0;
    if(!nas_lag_get_ifindex_from_obj(obj,&lag_index, false)){
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "CPS Delete LAG or it's Member operation Failed");
        return cps_api_ret_code_ERR;
    }

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "Deleting Lag Intf %d",
               lag_index);

    cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,lag_index);
    cps_api_object_attr_t type = cps_api_object_attr_get(obj,DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS);
    cps_api_object_attr_t member_port_attr = cps_api_get_key_data(obj, DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS_NAME);
    nas_lag_master_info_t *nas_lag_entry = nas_get_lag_node(lag_index);

    if(type || member_port_attr){
        EV_LOGGING(INTERFACE, INFO,"NAS-CPS-LAG",
                   "Remove Member Port for Lag %d",lag_index);
        return nas_cps_set_lag(obj);
    }

    if(!nas_lag_entry) {
        EV_LOGGING(INTERFACE, INFO,"NAS-CPS-LAG",
                    "Can't find LAG entry for Lag %d", lag_index);
        return cps_api_ret_code_ERR;
    }

      NAS_INTERFACE * l_obj = nullptr;
      std::string intf_name = std::string(nas_lag_entry->name);
      if(nas_interface_map_obj_remove(intf_name,&l_obj) ==STD_ERR_OK){
          if(l_obj) delete l_obj;
      }


    for (auto it : nas_lag_entry->port_list) {
        if (nas_intf_handle_intf_mode_change(it, BASE_IF_MODE_MODE_NONE) == false) {
            EV_LOGGING(INTERFACE, DEBUG, "NAS-CPS-LAG",
                    "Error setting interface mode to NONE on intf(%d)", lag_index);
            return cps_api_ret_code_ERR;
        }

        if (nas_cps_delete_port_from_lag(nas_lag_entry, it) !=  cps_api_ret_code_OK) {
            EV_LOGGING(INTERFACE, INFO,"NAS-CPS-LAG",
                        "Error clearing port %d from member port list", it);
            return cps_api_ret_code_ERR;
        }
    }

    //Acquring lock to avoid pocessing netlink msg from kernel.
    std_mutex_simple_lock_guard lock_t(nas_lag_mutex_lock());

    if (nas_intf_handle_intf_mode_change(lag_index, BASE_IF_MODE_MODE_L2) == false) {
        EV_LOGGING(INTERFACE, DEBUG, "NAS-CPS-LAG",
                "Update to NAS-L3 about interface mode change failed(%d)", lag_index);
    }
    // Cleanup nas multicast synchronously
    if (nas_intf_l3mc_intf_delete (lag_index, BASE_IF_MODE_MODE_NONE) == false) {
        EV_LOGGING(INTERFACE, DEBUG, "NAS-CPS-LAG",
                "Update to NAS-L3-MCAST about interface delete change failed(%d)", lag_index);
    }
    if(nas_os_delete_lag(obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                   "Failure deleting LAG index from kernel");
        return cps_api_ret_code_ERR;
    }

    if(nas_lag_master_delete(lag_index) != STD_ERR_OK){
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                   "Failure deleting LAG index from Nas-lag(NPU)");
        return cps_api_ret_code_ERR;
    }

    EV_LOGGING(INTERFACE, NOTICE, "NAS-CPS-LAG", "Lag Intf %d delete successful", lag_index);
    _nas_publish_lag_cps_req_obj(obj);
    return rc;
}

static cps_api_return_code_t nas_cps_set_mac(cps_api_object_t obj,hal_ifindex_t lag_index)
{
    cps_api_object_attr_t attr = cps_api_object_attr_get(obj, DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS);
    if (attr==NULL) return cps_api_ret_code_ERR;

    cps_api_set_key_data(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,cps_api_object_ATTR_T_U32,
            &lag_index,sizeof(lag_index));

    if(nas_os_interface_set_attribute(obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS) != STD_ERR_OK)
    {
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "lag MAC set failure OS");
        return cps_api_ret_code_ERR;
    }

    if(nas_lag_set_mac(lag_index,(const char *)cps_api_object_attr_data_bin(attr)) != STD_ERR_OK)
    {
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "lag MAC failure in NAS");
        return cps_api_ret_code_ERR;
    }

    return cps_api_ret_code_OK;
}

void nas_cps_handle_mac_set (const char *lag_name, hal_ifindex_t lag_index) {

   char  mac_str[MAC_STRING_SZ];
   char buff[MAX_CPS_MSG_BUFF];

   memset(mac_str, 0, MAC_STRING_SZ);
   memset(buff, 0 , MAX_CPS_MSG_BUFF);
   if (nas_if_get_assigned_mac(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
                               lag_name, 0, mac_str, sizeof(mac_str)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR , "NAS-LAG",
                 "Error get base mac");
        return;
   }

   cps_api_object_t name_obj = cps_api_object_init(buff, sizeof(buff));
   cps_api_object_attr_add_u32(name_obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, lag_index);
   cps_api_object_attr_add(name_obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS,
            mac_str,strlen(mac_str)+1);
   nas_cps_set_mac(name_obj, lag_index);

}

static cps_api_return_code_t nas_cps_set_admin_status(cps_api_object_t obj,hal_ifindex_t lag_index,
                                                       nas_lag_master_info_t *nas_lag_entry)
{

    cps_api_object_attr_t attr = cps_api_object_attr_get(obj,IF_INTERFACES_INTERFACE_ENABLED);

    if (attr == NULL){
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG","admin status %d attr is NULL",
                   lag_index);
        return cps_api_ret_code_ERR;
    }

    if(nas_os_interface_set_attribute(obj,IF_INTERFACES_INTERFACE_ENABLED) != STD_ERR_OK)
    {
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                   "lag Admin status set failure OS");
        return cps_api_ret_code_ERR;
    }

    //Update Status in lag struct
    nas_lag_entry->admin_status = cps_api_object_attr_data_u32(attr);

    return cps_api_ret_code_OK;
}


static cps_api_return_code_t nas_cps_add_port_to_lag(nas_lag_master_info_t *nas_lag_entry, hal_ifindex_t port_idx)
{
    cps_api_return_code_t rc = cps_api_ret_code_OK;
    BASE_IF_MODE_t intf_mode = nas_intf_get_mode(port_idx);
    /* Add to kernel only if port is not part of block list
     * since for blocked port we don't want to add the port to lag in kernel to
     * prevent hashing on that port in kernel
     */

    bool block_port = true;
    if(nas_lag_entry->block_port_list.find(port_idx) == nas_lag_entry->block_port_list.end()) {
        block_port = false;
        char buff[MAX_CPS_MSG_BUFF];
        EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "Add Ports to Lag");

        cps_api_object_t name_obj = cps_api_object_init(buff, sizeof(buff));
        cps_api_object_attr_add_u32(name_obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, nas_lag_entry->ifindex);
        cps_api_object_attr_add_u32(name_obj,DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS, port_idx);

        /*  Apply save and then reapply the admin state on the interface after port add/delete  */
        bool phy_admin_state  = false;
        std_mutex_simple_lock_guard g(nas_physical_intf_lock());
        nas_intf_admin_state_get(port_idx, &phy_admin_state);
        if(nas_os_add_port_to_lag(name_obj) != STD_ERR_OK) {
             EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                        "Error adding port %d to lag  %d in the Kernel",
                        port_idx,nas_lag_entry->ifindex);
             return cps_api_ret_code_ERR;
        }
    /*  Apply the admin in the kernel */
        nas_set_intf_admin_state_os(port_idx, phy_admin_state);
        EV_LOGGING(INTERFACE,NOTICE,"NAS-CPS-LAG","set Admin state for %d is %d ", port_idx, phy_admin_state);
    }

    if_master_info_t master_info = { nas_int_type_LAG, NAS_PORT_NONE, nas_lag_entry->ifindex};

    if(!nas_intf_add_master(port_idx, master_info)){
        EV_LOGGING(INTERFACE,DEBUG,"NAS-LAG-MASTER","Failed to add master for lag memeber port");
    } else {
        BASE_IF_MODE_t new_mode = nas_intf_get_mode(port_idx);
        if (new_mode != intf_mode) {
            if (nas_intf_handle_intf_mode_change(port_idx, new_mode) == false) {
                EV_LOGGING(INTERFACE,DEBUG,"NAS-LAG-MASTER", "Update to NAS-L3 about interface mode change failed(%d)", port_idx);
            }
            if (nas_intf_l3mc_intf_mode_change(port_idx, new_mode) == false) {
                EV_LOGGING(INTERFACE, ERR, "NAS-LAG-MASTER", "L3 MC mode change RPC failed if_index(%d), mode(%d)",
                        port_idx, new_mode);
            }
        }
    }

    /*
     * Check for virtual port and if it is a virtual port don't add it
     */
    if(!nas_is_virtual_port(port_idx)){
        bool prev_oper_status = nas_lag_entry->oper_status;

        if(nas_lag_member_add(nas_lag_entry->ifindex,port_idx) != STD_ERR_OK)
        {
            EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                    "Error inserting index %d in list", port_idx);
            return cps_api_ret_code_ERR;
        }

        if (prev_oper_status != nas_lag_entry->oper_status) {
            lag_state_object_publish(nas_lag_entry, nas_lag_entry->oper_status);
        }

        if (block_port && nas_lag_block_port(nas_lag_entry, port_idx, block_port) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "Error Block/unblock Port %d",port_idx);
            return cps_api_ret_code_ERR;
        }
    }

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG","Add Ports to Lag Exit");
    return rc;
}

static cps_api_return_code_t nas_cps_delete_port_from_lag(nas_lag_master_info_t *nas_lag_entry, hal_ifindex_t ifindex)
{
    char buff[MAX_CPS_MSG_BUFF];
    cps_api_return_code_t rc=cps_api_ret_code_OK;
    if_master_info_t master_info = { nas_int_type_LAG, NAS_PORT_NONE, nas_lag_entry->ifindex};
    BASE_IF_MODE_t intf_mode = nas_intf_get_mode(ifindex);

    cps_api_object_t obj = cps_api_object_init(buff, sizeof(buff));
    cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS, ifindex);


    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
               "nas_cps_del_port_from_lag %d", ifindex);

    if(!nas_is_virtual_port(ifindex)){
        //delete the port from NPU
        if((nas_lag_member_delete(0,ifindex) != STD_ERR_OK)){
            EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                        "Error deleting interface %d from NPU", ifindex);
            return cps_api_ret_code_ERR;
        }
    }

    if(!nas_intf_del_master(ifindex, master_info)){
        EV_LOGGING(INTERFACE,DEBUG,"NAS-LAG-MASTER","Failed to delete master for lag memeber port");
    } else {
        BASE_IF_MODE_t new_mode = nas_intf_get_mode(ifindex);
        if (new_mode != intf_mode) {
            if (nas_intf_handle_intf_mode_change(ifindex, new_mode) == false) {
                EV_LOGGING(INTERFACE,DEBUG,"NAS-LAG-MASTER", "Update to NAS-L3 about interface mode change failed(%d)",
                        ifindex);
            }
            if (nas_intf_l3mc_intf_mode_change(ifindex, new_mode) == false) {
                EV_LOGGING(INTERFACE, ERR, "NAS-LAG-MASTER", "L3 MC mode change RPC failed if_index(%d), mode(%d)",
                        ifindex, new_mode);
            }
        }
    }

    //NPU delete done, now delete from Kernel

    /*
     * Check if the port is in block port list then it won't be added in the kernel
     * in that case no need of deleting it from kernel
     */
    if(nas_lag_entry->block_port_list.find(ifindex) == nas_lag_entry->block_port_list.end()){
        bool phy_admin_state  = false;
        std_mutex_simple_lock_guard g(nas_physical_intf_lock());
        nas_intf_admin_state_get(ifindex, &phy_admin_state);
        if(nas_os_delete_port_from_lag(obj) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG","Error deleting interface %d from OS", ifindex);
            return cps_api_ret_code_ERR;
        }
        /*  Apply the admin in the kernel */
        nas_set_intf_admin_state_os(ifindex, phy_admin_state);
    }

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
               "nas_cps_del_port_from_lag Exit %d",ifindex);
    return rc;
}

static bool nas_lag_get_intf_ctrl_info(hal_ifindex_t index, interface_ctrl_t &i){
    memset(&i,0,sizeof(i));
    i.if_index = index;
    i.q_type = HAL_INTF_INFO_FROM_IF;
    if (dn_hal_get_interface_info(&i)!=STD_ERR_OK){
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                   "Failed to get interface control information for "
                   "interface %d", index);
        return false;
    }
    return true;
}

static bool nas_lag_get_intf_ctrl_info(const char * name, interface_ctrl_t & i){
    memset(&i,0,sizeof(i));
    strncpy(i.if_name,name,sizeof(i.if_name)-1);
    i.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    if (dn_hal_get_interface_info(&i)!=STD_ERR_OK){
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                   "Failed to get interface control information for "
                   "interface %s",name);
        return false;
    }
    return true;
}

static cps_api_return_code_t cps_lag_update_ports(nas_lag_master_info_t  *nas_lag_entry,
        nas_port_list_t &port_index_list, cps_api_operation_types_t op)
{

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "cps_lag_update_ports %d",
               op);

    /* If it's a SET operation we need to remove the ports that are not part of the new port list first */
    if (op == cps_api_oper_SET) {
        hal_ifindex_t port;
        for(auto it = nas_lag_entry->port_list.begin(); it != nas_lag_entry->port_list.end();) {
            if (port_index_list.find(*it) == port_index_list.end()) {
                port = *it;
                it++;
                EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
                           "Removing unneeded memberport %d", port);

                if(nas_cps_delete_port_from_lag(nas_lag_entry, port) != cps_api_ret_code_OK) {
                    EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                               "Error deleting unneeded memberport %d from  OS/NPU", port);
                    return cps_api_ret_code_ERR;
                }
                nas_lag_entry->port_list.erase(port);
                nas_lag_entry->port_oper_list.erase(port);

                /* When LAG member port deleted, remove port from block list also */
                if(nas_lag_entry->block_port_list.find(port) != nas_lag_entry->block_port_list.end()) {

                    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
                                                "Delete uneeded memberport %d from Block list", port);
                    nas_lag_entry->block_port_list.erase(port);
                }
            } else {
                it++;
            }
        }
    }

    for(auto it = port_index_list.begin() ; it != port_index_list.end() ; ++it){

        if(((op == cps_api_oper_CREATE) || (op == cps_api_oper_SET)) &&
            (nas_lag_entry->port_list.find(*it) == nas_lag_entry->port_list.end())) {

            EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
                    "Received new port add %d", *it);

            if(nas_cps_add_port_to_lag(nas_lag_entry, *it) != cps_api_ret_code_OK) {
                EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                           "Error adding port %d to OS/NPU", *it);
                return cps_api_ret_code_ERR;
            }
            nas_lag_entry->port_list.insert(*it);
            /* BLOCK list may have been saved before new port member addition: apply it now */
            if ((nas_lag_entry->block_port_list.find(*it) != nas_lag_entry->block_port_list.end()) &&
                (nas_lag_block_port(nas_lag_entry,*it, true) != STD_ERR_OK)) {
                EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                "When adding slave port, Error in blocking Port %d ",*it);
                return cps_api_ret_code_ERR;
            }


        } else if((op == cps_api_oper_DELETE) &&
            (nas_lag_entry->port_list.find(*it) != nas_lag_entry->port_list.end())) {

            EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
                       "Received new port delete %d", *it);

            if(nas_cps_delete_port_from_lag(nas_lag_entry, *it) != cps_api_ret_code_OK) {
                EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                           "Error deleting port  %d from  OS/NPU", *it);
                return cps_api_ret_code_ERR;
            }
            nas_lag_entry->port_list.erase(*it);
            nas_lag_entry->port_oper_list.erase(*it);

            /* When LAG member port deleted, remove port from block list also */
            if(nas_lag_entry->block_port_list.find(*it) != nas_lag_entry->block_port_list.end()) {

                EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
                                            "Delete port %d from Block list", *it);
                nas_lag_entry->block_port_list.erase(*it);
            }

        }

        EV_LOGGING(INTERFACE, NOTICE, "NAS-CPS-LAG", "CPS %s Port index %d to LAG %s operation successful",
                        (op != cps_api_oper_DELETE) ? "Add" : "Remove", *it, nas_lag_entry->name);
    }

    nas_cps_update_oper_state(nas_lag_entry);

    return cps_api_ret_code_OK;
}


static void nas_pack_lag_port_list(cps_api_object_t obj,nas_lag_master_info_t *nas_lag_entry, int attr_id,bool get)
{
    bool port_mode, block_port_set = false, unblock_port_set = false;
    unsigned int list_id =0;
    for (auto it=nas_lag_entry->port_list.begin(); it!=nas_lag_entry->port_list.end(); ++it) {
        EV_LOGGING(INTERFACE, INFO, "NAS-LAG-CPS", "port idx %d", *it);
        interface_ctrl_t i;
        if(!nas_lag_get_intf_ctrl_info(*it,i)) return;

        if(get){
            cps_api_attr_id_t ids[3] = {DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS,list_id++,
                                        DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS_NAME };
            const int ids_len = sizeof(ids)/sizeof(ids[0]);
            cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_BIN,i.if_name,strlen(i.if_name)+1);
        }else{
            cps_api_object_attr_add_u32(obj, attr_id, *it);
        }

        if (nas_lag_get_port_mode(*it, port_mode) == STD_ERR_OK) {
            if (port_mode) {
                if(get){
                    cps_api_object_attr_add(obj,BASE_IF_LAG_IF_INTERFACES_INTERFACE_BLOCK_PORT_LIST,i.if_name,strlen(i.if_name)+1);
                }else{
                    cps_api_object_attr_add_u32(obj, BASE_IF_LAG_IF_INTERFACES_INTERFACE_BLOCK_PORT_LIST, *it);
                }
                block_port_set = true;
            } else {
                if(get){
                    cps_api_object_attr_add(obj,BASE_IF_LAG_IF_INTERFACES_INTERFACE_UNBLOCK_PORT_LIST,i.if_name,strlen(i.if_name)+1);
                }else{
                    cps_api_object_attr_add_u32(obj, BASE_IF_LAG_IF_INTERFACES_INTERFACE_UNBLOCK_PORT_LIST, *it);
                }
                unblock_port_set = true;
            }
        }
    }

    if(nas_lag_entry->port_list.size() == 0) {
        cps_api_object_attr_add(obj, attr_id, 0, 0);
    }

    if (!block_port_set) {
        cps_api_object_attr_add(obj, BASE_IF_LAG_IF_INTERFACES_INTERFACE_BLOCK_PORT_LIST, 0, 0);
    }
    if (!unblock_port_set) {
        cps_api_object_attr_add(obj, BASE_IF_LAG_IF_INTERFACES_INTERFACE_UNBLOCK_PORT_LIST, 0, 0);
    }
}

 cps_api_return_code_t lag_object_publish(nas_lag_master_info_t *nas_lag_entry,hal_ifindex_t lag_idx,
                                cps_api_operation_types_t op)
{
    char buff[MAX_CPS_MSG_BUFF];
    memset(buff,0,sizeof(buff));
    cps_api_object_t obj_pub = cps_api_object_init(buff, sizeof(buff));
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj_pub),BASE_IF_LAG_IF_INTERFACES_INTERFACE_OBJ,cps_api_qualifier_OBSERVED);
    cps_api_set_key_data(obj_pub,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,cps_api_object_ATTR_T_U32,
            &lag_idx,sizeof(lag_idx));
    cps_api_object_set_type_operation(cps_api_object_key(obj_pub),op);

    if(op == cps_api_oper_SET){
        nas_pack_lag_port_list(obj_pub,nas_lag_entry, DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS,false);
        cps_api_object_attr_add_u32(obj_pub,IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS,(nas_lag_entry->admin_status ?
                IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS_UP : IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS_DOWN) );
    }
    if (cps_api_event_thread_publish(obj_pub)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-INTF-EVENT",
                   "Failed to send event. Service issue");
        return cps_api_ret_code_ERR;
    }
    EV_LOGGING(INTERFACE, INFO, "NAS-INTF-EVENT",
               "Published LAG object %d", lag_idx);
    return cps_api_ret_code_OK;
}


cps_api_return_code_t lag_state_object_publish(nas_lag_master_info_t *nas_lag_entry,bool oper_status)
{
    char buff[MAX_CPS_MSG_BUFF];
    memset(buff,0,sizeof(buff));
    cps_api_object_t obj_pub = cps_api_object_init(buff, sizeof(buff));
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj_pub),BASE_IF_LAG_IF_INTERFACES_STATE_INTERFACE_OBJ,cps_api_qualifier_OBSERVED);
    cps_api_set_key_data(obj_pub,IF_INTERFACES_STATE_INTERFACE_IF_INDEX,cps_api_object_ATTR_T_U32,
             &(nas_lag_entry->ifindex),sizeof(nas_lag_entry->ifindex));
    cps_api_object_set_type_operation(cps_api_object_key(obj_pub),cps_api_oper_SET);
    cps_api_object_attr_add_u32(obj_pub,IF_INTERFACES_STATE_INTERFACE_OPER_STATUS,(oper_status ?
                 IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_UP : IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_DOWN) );

    if (cps_api_event_thread_publish(obj_pub)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INTF-EVENT","Failed to send event.  Service issue");
        return cps_api_ret_code_ERR;
    }
    EV_LOGGING(INTERFACE,DEBUG,"NAS-INTF-EVENT","Published LAG Oper status for object %d", nas_lag_entry->ifindex);
    return cps_api_ret_code_OK;
 }

static void nas_cps_update_oper_state(nas_lag_master_info_t *nas_lag_entry) {
    bool prev_oper_status = nas_lag_entry->oper_status;
    nas_lag_entry->oper_status = false;

    for (const auto &it : nas_lag_entry->port_oper_list) {
        if (it.second) {
            nas_lag_entry->oper_status = true;
            break;
        }
    }

    if (nas_lag_entry->oper_status != prev_oper_status) {
        lag_state_object_publish(nas_lag_entry, nas_lag_entry->oper_status);
    }

    return;
}

static cps_api_return_code_t nas_process_lag_block_ports(nas_lag_master_info_t  *nas_lag_entry,
        nas_port_list_t &port_index_list,bool port_state)
{
    cps_api_return_code_t rc = cps_api_ret_code_OK;

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "Processing lag block ports");

    for(auto it = port_index_list.begin() ; it != port_index_list.end() ; ++it){

        /* Blocked port add to block list, if not if its added
           previosuly delete it */
        if (port_state) {
           nas_lag_entry->block_port_list.insert(*it);
           EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
                             "Add port %d to Block list", *it);
        }
        else {
           nas_lag_entry->block_port_list.erase(*it);
           EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
                           "Delete port %d from Block list", *it);
        }

        if(nas_lag_block_port(nas_lag_entry,*it,port_state) != STD_ERR_OK)
        {
            EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                       "Error Block/unblock Port %d",*it);
            return cps_api_ret_code_ERR;
        }

        /*
         * If blocked port is not in member port_list,
         * it won't be added to kernel in the first place
         */

        if(nas_lag_entry->port_list.find(*it) == nas_lag_entry->port_list.end()){
            continue;
        }


        cps_api_object_t lag_obj = cps_api_object_create();
        if(!lag_obj){
            EV_LOGGING(INTERFACE,ERR,"NAS-LAG","Failed to allocate memory to create obj to add/delete"
                    "port to/from LAG");
            return cps_api_ret_code_ERR;
        }
        cps_api_object_guard og(lag_obj);
        cps_api_object_attr_add_u32(lag_obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,
                                    nas_lag_entry->ifindex);
        cps_api_object_attr_add_u32(lag_obj,DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS,*it);

        EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "Update block/unblock port %d,lag %d to kernel",
                  *it,nas_lag_entry->ifindex);

        /*  Save the current admin state of the port  */
        bool phy_admin_state  = false;
        std_mutex_simple_lock_guard g(nas_physical_intf_lock());
        nas_intf_admin_state_get(*it, &phy_admin_state);
        if(port_state) {
            if(nas_os_delete_port_from_lag(lag_obj) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "OS:Error deleting intf %d, mem idx %d",
                                            nas_lag_entry->ifindex, *it);
                return cps_api_ret_code_ERR;
            }
        } else {
            if(nas_os_add_port_to_lag(lag_obj) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "OS:Error adding intf %d, mem idx %d",
                                            nas_lag_entry->ifindex, *it);
                return cps_api_ret_code_ERR;
            }
        }
    /*  Apply the admin state of the port in the kernel */
        nas_set_intf_admin_state_os(*it, phy_admin_state);
    }

    return rc;
}

static bool nas_lag_process_member_ports(cps_api_object_t obj,nas_port_list_t & list,
                                         const cps_api_object_it_t & it){
    cps_api_object_it_t it_lvl_1 = it;
    interface_ctrl_t i;
    cps_api_attr_id_t ids[3] = {DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS,0,
                                DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS_NAME };
    const int ids_len = sizeof(ids)/sizeof(ids[0]);

    for (cps_api_object_it_inside (&it_lvl_1); cps_api_object_it_valid (&it_lvl_1);
         cps_api_object_it_next (&it_lvl_1)) {

        ids[1] = cps_api_object_attr_id (it_lvl_1.attr);
        cps_api_object_attr_t intf = cps_api_object_e_get(obj,ids,ids_len);

        if(intf == NULL){
            EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                       "No Interface Name is passed");
            return false;
        }

       if(!nas_lag_get_intf_ctrl_info((const char *)cps_api_object_attr_data_bin(intf),i)){
           return false;
       }

       list.insert(i.if_index);

    }

    return true;

}

static t_std_error nas_lag_set_mac_learn_mode(cps_api_object_t obj,nas_lag_master_info_t * entry){
    /*  TODO BASE_IF_LAG_IF_INTERFACES_INTERFACE_LEARN_MODE is being replaced with
     *  DELL_IF_IF_INTERFACES_INTERFACE_MAC_LEARN.  in the mean time both are supported*/
    cps_api_object_attr_t mac_learn_mode = cps_api_object_attr_get(obj,
                                               DELL_IF_IF_INTERFACES_INTERFACE_MAC_LEARN);
    if(mac_learn_mode == nullptr){
        mac_learn_mode = cps_api_object_attr_get(obj,
                                               BASE_IF_LAG_IF_INTERFACES_INTERFACE_LEARN_MODE);
    }
    if (mac_learn_mode == nullptr) {
        return STD_ERR(INTERFACE,CFG,0);
    }

    BASE_IF_PHY_MAC_LEARN_MODE_t mode = (BASE_IF_PHY_MAC_LEARN_MODE_t)
                                           cps_api_object_attr_data_u32(mac_learn_mode);

    bool enable = true;
    if((mode == BASE_IF_PHY_MAC_LEARN_MODE_DROP) || (mode == BASE_IF_PHY_MAC_LEARN_MODE_DISABLE)) {
        enable = false;
    }
    if(nas_os_mac_change_learning(entry->ifindex,enable) != STD_ERR_OK){
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-SET","Failed to update MAC learn mode in OS for intf:%d mode:%d",
                   entry->ifindex, mode);
        return STD_ERR(INTERFACE,CFG,0);
    }

    entry->mac_learn_mode = mode;
    entry->mac_learn_mode_set = true;

    t_std_error rc;
    std::string intf_name = std::string(entry->name);
    if ( (rc = nas_interface_utils_set_lag_mac_learn_mode(intf_name, 0, entry->ndi_lag_id, mode))  != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-SET","Failed to update MAC learn mode:%d in the npu for %s", mode, entry->name);
    }
    EV_LOGGING(INTERFACE,DEBUG,"NAS-IF-SET","Updated MAC learn mode:%d in the npu for %s", mode, entry->name);
    return rc;

}

static t_std_error nas_lag_set_split_horizon(cps_api_object_it_t & it,
                                             nas_lag_master_info_t * entry, bool ingress ){

    uint32_t sh_id = cps_api_object_attr_data_uint(it.attr);

    t_std_error rc = STD_ERR_OK;

    NAS_INTERFACE *l_obj = nullptr;
    std::string intf_name = std::string(entry->name);
    if((l_obj = nas_interface_map_obj_get(intf_name)) != nullptr){
        ingress ? l_obj->set_ingress_split_horizon_id(sh_id) : l_obj->set_egress_split_horizon_id(sh_id);
        nas_com_id_value_t attr;
        attr.attr_id = ingress ? DELL_IF_IF_INTERFACES_INTERFACE_INGRESS_SPLIT_HORIZON_ID :
                                 DELL_IF_IF_INTERFACES_INTERFACE_EGRESS_SPLIT_HORIZON_ID;
        attr.val = &sh_id;
        attr.vlen = sizeof(uint32_t);
        rc = nas_interface_utils_set_all_sub_intf_attr(intf_name,&attr,1);
    }

    return rc;
}

static cps_api_return_code_t nas_cps_set_lag(cps_api_object_t obj)
{
    hal_ifindex_t lag_index = 0;
    cps_api_return_code_t rc = cps_api_ret_code_OK;
    nas_lag_master_info_t *nas_lag_entry;
    cps_api_object_it_t it;
    nas_port_list_t port_list, block_port_list, unblock_port_list;

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "CPS Set LAG");

    if(!nas_lag_get_ifindex_from_obj(obj,&lag_index, false)){
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "CPS Set LAG failed");
        return cps_api_ret_code_ERR;
    }

    cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, lag_index);

    nas_lag_entry = nas_get_lag_node(lag_index);

    if(nas_lag_entry == NULL) {
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "Lag node is NULL");
        return cps_api_ret_code_ERR;
    }

    cps_api_object_attr_t member_port_attr = cps_api_get_key_data(obj, DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS_NAME);

    bool port_list_attr = false;
    interface_ctrl_t i;

    if(member_port_attr != NULL){
        interface_ctrl_t _if;
        if(!nas_lag_get_intf_ctrl_info((const char *)cps_api_object_attr_data_bin(member_port_attr),_if)){
            EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "Lag set operation failed for Lag %d", lag_index);
            return cps_api_ret_code_ERR;
        }
        port_list.insert(_if.if_index);
        port_list_attr = true;
    }

    cps_api_object_it_begin(obj,&it);
    for ( ; cps_api_object_it_valid(&it) ; cps_api_object_it_next(&it) ) {
        int id = (int) cps_api_object_attr_id(it.attr);
        switch (id) {
            case DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS:
                rc = nas_cps_set_mac(obj,lag_index);
                break;
            case DELL_IF_IF_INTERFACES_INTERFACE_MTU:
                rc= nas_os_interface_set_attribute(obj,DELL_IF_IF_INTERFACES_INTERFACE_MTU);
                break;
            case BASE_IF_LAG_IF_INTERFACES_INTERFACE_LEARN_MODE:
            case DELL_IF_IF_INTERFACES_INTERFACE_MAC_LEARN:
                rc = nas_lag_set_mac_learn_mode(obj,nas_lag_entry);
                break;
            case DELL_IF_IF_INTERFACES_INTERFACE_INGRESS_SPLIT_HORIZON_ID:
                nas_lag_set_split_horizon(it,nas_lag_entry,true);
                break;
            case DELL_IF_IF_INTERFACES_INTERFACE_EGRESS_SPLIT_HORIZON_ID:
                nas_lag_set_split_horizon(it,nas_lag_entry,false);
                break;
            case IF_INTERFACES_INTERFACE_ENABLED:
                rc = nas_cps_set_admin_status(obj,lag_index,nas_lag_entry);
                break;
            case DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS:
                port_list_attr = true;
                if (cps_api_object_attr_len(it.attr) != 0) {
                   if(!nas_lag_process_member_ports(obj,port_list,it)){
                       return cps_api_ret_code_ERR;
                   }
                }
                break;
            case BASE_IF_LAG_IF_INTERFACES_INTERFACE_BLOCK_PORT_LIST:
                if (cps_api_object_attr_len(it.attr) != 0) {
                    if(!nas_lag_get_intf_ctrl_info((const char *)cps_api_object_attr_data_bin(it.attr),i)){
                        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "CPS block list processing failed for LAG %d", lag_index);
                        return cps_api_ret_code_ERR;
                    }
                    block_port_list.insert(i.if_index);
                }
                break;
            case BASE_IF_LAG_IF_INTERFACES_INTERFACE_UNBLOCK_PORT_LIST:
                if (cps_api_object_attr_len(it.attr) != 0) {
                    if(!nas_lag_get_intf_ctrl_info((const char *)cps_api_object_attr_data_bin(it.attr),i)){
                        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "CPS unblock list processing failed for LAG %d", lag_index);
                        return cps_api_ret_code_ERR;
                    }
                    unblock_port_list.insert(i.if_index);
                }
                break;
            case IF_INTERFACES_INTERFACE_DESCRIPTION:
                rc = nas_cps_set_desc(it.attr, lag_index);
                break;
            default:
                EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
                           "Received attrib %d", id);
                break;
        }
    }

    if(unblock_port_list.size()) {
        rc = nas_process_lag_block_ports(nas_lag_entry, unblock_port_list, false);
        if(rc == cps_api_ret_code_ERR){
            EV_LOGGING(INTERFACE,ERR,"NAS-CPS-LAG", "Failed to process unblocked lag ports");
            return rc;
        }
    }

    if(block_port_list.size()) {
        rc = nas_process_lag_block_ports(nas_lag_entry, block_port_list, true);
        if(rc == cps_api_ret_code_ERR){
            EV_LOGGING(INTERFACE,ERR,"NAS-CPS-LAG", "Failed to process blocked lag ports");
            return rc;
        }
    }


    if(port_list_attr) {
        EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
                "Received %lu valid ports ", port_list.size());
        cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));
        /* If a CPS 'set' operation is used on an existing LAG and a port_list is provided, we will just
         * insert the ports into the member port like, the same way it's performed in a CPS 'create'
         * operation.
         */
        if(cps_lag_update_ports(nas_lag_entry, port_list,op) !=STD_ERR_OK)
        {
            EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                       "nas_process_cps_ports failure");
            return cps_api_ret_code_ERR;
        }

        op = cps_api_oper_SET;
        // publish CPS_LAG events on port add/delete. This is internal publish
        if(lag_object_publish(nas_lag_entry,lag_index,op)!= cps_api_ret_code_OK)
        {
            EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                       "LAG events publish failure");
            return cps_api_ret_code_ERR;
        }
    }
    _nas_publish_lag_cps_req_obj(obj);
    return rc;
}


static void nas_pack_lag_if(cps_api_object_t obj, nas_lag_master_info_t *nas_lag_entry)
{
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),BASE_IF_LAG_IF_INTERFACES_INTERFACE_OBJ,
            cps_api_qualifier_TARGET);

    cps_api_set_key_data(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,
            cps_api_object_ATTR_T_U32,
            &nas_lag_entry->ifindex,sizeof(nas_lag_entry->ifindex));
    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME,
                cps_api_object_ATTR_T_BIN,
                nas_lag_entry->name, strlen(nas_lag_entry->name)+1);

    if (nas_lag_entry->lag_id != INVALID_LAG_ID)
        cps_api_object_attr_add_u32(obj, BASE_IF_LAG_IF_INTERFACES_INTERFACE_ID, nas_lag_entry->lag_id);

    cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
                            (const void *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
                            strlen(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG)+1);

    nas_pack_lag_port_list(obj,nas_lag_entry,DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS,true);

    if(!nas_lag_entry->mac_addr.empty()){
        cps_api_object_attr_add(obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS, nas_lag_entry->mac_addr.c_str(),
           nas_lag_entry->mac_addr.length()+1);
    }

    cps_api_object_attr_add_u32(obj,IF_INTERFACES_INTERFACE_ENABLED ,nas_lag_entry->admin_status);
    /*
     * Get MTU on LAG Interface
     */
    nas_os_get_interface_mtu(nas_lag_entry->name, obj);

    nas::ndi_obj_id_table_t lag_opaque_data_table;
    //@TODO to retrive NPU ID in multi npu case
    lag_opaque_data_table[0] = nas_lag_entry->ndi_lag_id;
    cps_api_attr_id_t  attr_id_list[] = {BASE_IF_LAG_IF_INTERFACES_INTERFACE_LAG_OPAQUE_DATA};
    nas::ndi_obj_id_table_cps_serialize (lag_opaque_data_table, obj, attr_id_list,
            sizeof(attr_id_list)/sizeof(attr_id_list[0]));
    interface_ctrl_t intf_ctrl_blk;

    if(!nas_lag_get_intf_ctrl_info(nas_lag_entry->ifindex, intf_ctrl_blk)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                   "Failed to get interface control information for "
                   "interface %d", nas_lag_entry->ifindex);
        return;
    }

    if (intf_ctrl_blk.desc) {
        cps_api_object_attr_add(obj, IF_INTERFACES_INTERFACE_DESCRIPTION,
                                    (const void*)intf_ctrl_blk.desc, strlen(intf_ctrl_blk.desc) + 1);
    }
}

static void nas_pack_lag_if_state(cps_api_object_t obj, nas_lag_master_info_t *nas_lag_entry)
{
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),BASE_IF_LAG_IF_INTERFACES_STATE_INTERFACE_OBJ,
            cps_api_qualifier_OBSERVED);

    cps_api_set_key_data(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,
            cps_api_object_ATTR_T_U32,
            &nas_lag_entry->ifindex,sizeof(nas_lag_entry->ifindex));
    cps_api_set_key_data(obj,IF_INTERFACES_STATE_INTERFACE_NAME,
                cps_api_object_ATTR_T_BIN,
                nas_lag_entry->name, strlen(nas_lag_entry->name)+1);

    if (nas_lag_entry->lag_id != INVALID_LAG_ID)
        cps_api_object_attr_add_u32(obj, BASE_IF_LAG_IF_INTERFACES_INTERFACE_ID, nas_lag_entry->lag_id);
    cps_api_object_attr_add_u32(obj, IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS, nas_lag_entry->admin_status ?
        IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS_UP : IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS_DOWN);
    cps_api_object_attr_add_u32(obj, IF_INTERFACES_STATE_INTERFACE_OPER_STATUS, nas_lag_entry->oper_status ?
        IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_UP : IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_DOWN);
    cps_api_object_attr_add_u32(obj, BASE_IF_LAG_IF_INTERFACES_INTERFACE_LEARN_MODE,
        nas_lag_entry->mac_learn_mode_set ? nas_lag_entry->mac_learn_mode : BASE_IF_PHY_MAC_LEARN_MODE_DISABLE);
    cps_api_object_attr_add_u32(obj, BASE_IF_LAG_IF_INTERFACES_STATE_INTERFACE_NUM_PORTS, (nas_lag_entry->port_list).size());

    cps_api_object_attr_add(obj,IF_INTERFACES_STATE_INTERFACE_TYPE,
                            (const void *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG,
                            strlen(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_IEEE8023ADLAG)+1);

    nas_pack_lag_port_list(obj,nas_lag_entry,DELL_IF_IF_INTERFACES_INTERFACE_MEMBER_PORTS,true);

    if(!nas_lag_entry->mac_addr.empty()){
        cps_api_object_attr_add(obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS, nas_lag_entry->mac_addr.c_str(),
           nas_lag_entry->mac_addr.length()+1);
    }

    cps_api_object_attr_add_u32(obj,IF_INTERFACES_INTERFACE_ENABLED ,nas_lag_entry->admin_status);
    /*
     * Get MTU on LAG Interface
     */
    nas_os_get_interface_mtu(nas_lag_entry->name, obj);

    nas::ndi_obj_id_table_t lag_opaque_data_table;
    //@TODO to retrive NPU ID in multi npu case
    lag_opaque_data_table[0] = nas_lag_entry->ndi_lag_id;
    cps_api_attr_id_t  attr_id_list[] = {BASE_IF_LAG_IF_INTERFACES_INTERFACE_LAG_OPAQUE_DATA};
    nas::ndi_obj_id_table_cps_serialize (lag_opaque_data_table, obj, attr_id_list,
            sizeof(attr_id_list)/sizeof(attr_id_list[0]));
}

static t_std_error nas_get_lag_intf(hal_ifindex_t ifindex, cps_api_object_list_t list, bool get_intf_state)
{
    nas_lag_master_info_t *nas_lag_entry = NULL;

    EV_LOGGING(INTERFACE, INFO, "NAS-LAG-CPS",
               "Get lag %s %d", (get_intf_state ? "interface-state" : "interface"), ifindex);

    nas_lag_entry = nas_get_lag_node(ifindex);

    if(nas_lag_entry == NULL) {
        EV_LOGGING(INTERFACE, ERR, "NAS-LAG-CPS",
                   "Error finding lagnode %d for get operation", ifindex);
        return(STD_ERR(INTERFACE, FAIL, 0));
    }
    cps_api_object_t object = cps_api_object_list_create_obj_and_append(list);

    if(object == NULL){
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "obj NULL failure");
        return(STD_ERR(INTERFACE, FAIL, 0));
    }

    if (get_intf_state) {
        nas_pack_lag_if_state(object, nas_lag_entry);
    } else {
        nas_pack_lag_if(object, nas_lag_entry);
    }

    return STD_ERR_OK;
}

static t_std_error nas_lag_get_all_info(cps_api_object_list_t list, bool get_intf_state)
{
    nas_lag_master_info_t *nas_lag_entry = NULL;
    nas_lag_master_table_t nas_lag_master_table;

    EV_LOGGING(INTERFACE, INFO, "NAS-LAG-CPS", "Getting all lag %s", (get_intf_state ? "interface-states" : "interfaces"));

    nas_lag_master_table = nas_get_lag_table();

    for (auto it =nas_lag_master_table.begin();it != nas_lag_master_table.end();++it){

        EV_LOGGING(INTERFACE, INFO, "NAS-LAG-CPS", "Inside Loop");
        cps_api_object_t obj = cps_api_object_list_create_obj_and_append(list);
        if (obj == NULL) {
            EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "obj NULL failure");
            return STD_ERR(INTERFACE, NOMEM, 0);
        }

        nas_lag_entry = nas_get_lag_node(it->first);

        if(nas_lag_entry == NULL) {
            EV_LOGGING(INTERFACE, ERR, "NAS-LAG-CPS",
                       "Error finding lagnode %d for get operation", it->first);
            return(STD_ERR(INTERFACE, FAIL, 0));
        }

        if(get_intf_state) {
            nas_pack_lag_if_state(obj, nas_lag_entry);
        } else {
            nas_pack_lag_if(obj, nas_lag_entry);
        }
    }

    return STD_ERR_OK;
}

t_std_error nas_lag_ndi_it_to_obj_fill(nas_obj_id_t ndi_lag_id,cps_api_object_list_t list, bool get_intf_state)
{
    nas_lag_master_info_t *nas_lag_entry = NULL;
    nas_lag_master_table_t nas_lag_master_table;

    EV_LOGGING(INTERFACE, INFO, "NAS-LAG-CPS", "Fill opaque data....");

    if(ndi_lag_id == 0)
        return STD_ERR(INTERFACE, FAIL, 0);

    nas_lag_master_table = nas_get_lag_table();
    for (auto it =nas_lag_master_table.begin();it != nas_lag_master_table.end();++it){


        nas_lag_entry = nas_get_lag_node(it->first);

        if(nas_lag_entry == NULL) {
            EV_LOGGING(INTERFACE, ERR, "NAS-LAG-CPS",
                       "Error finding lag Entry %d for get operation",
                       it->first);
            return(STD_ERR(INTERFACE, FAIL, 0));
        }

        EV_LOGGING(INTERFACE, INFO, "NAS-LAG-CPS",
                   "Lg id %lu and appid %lu",
                   nas_lag_entry->ndi_lag_id, ndi_lag_id);

        if(nas_lag_entry->ndi_lag_id == ndi_lag_id){
            cps_api_object_t obj = cps_api_object_list_create_obj_and_append(list);
            if(obj == NULL) {
                EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "obj NULL failure");
                return STD_ERR(INTERFACE, NOMEM, 0);
            }

            if(get_intf_state) {
                nas_pack_lag_if_state(obj,nas_lag_entry);
            } else {
                nas_pack_lag_if(obj,nas_lag_entry);
            }

            return STD_ERR_OK;
        }
    }

    return (STD_ERR(INTERFACE,FAIL,0));
}

static cps_api_return_code_t nas_process_cps_lag_get(void * context, cps_api_get_params_t * param,
        size_t ix) {
    hal_ifindex_t ifindex = 0;
    bool opaque_attr_data = false;
    nas_obj_id_t ndi_lag_id=0;

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "cps_nas_lag_get_function");

    cps_api_object_t obj = cps_api_object_list_get(param->filters, ix);

    cps_api_object_attr_t ndi_lag_id_attr = cps_api_object_attr_get(obj,
                    BASE_IF_LAG_IF_INTERFACES_INTERFACE_LAG_OPAQUE_DATA);

    if(ndi_lag_id_attr != nullptr){
        EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
                   "LAG OPAQUE DATA FOUND %lu!!!",
                   cps_api_object_attr_data_u64(ndi_lag_id_attr));
        ndi_lag_id = cps_api_object_attr_data_u64(ndi_lag_id_attr);
        opaque_attr_data=true;
    }

    std_mutex_simple_lock_guard lock_t(nas_lag_mutex_lock());

    if(nas_lag_get_ifindex_from_obj(obj,&ifindex, false)){
        if(nas_get_lag_intf(ifindex, param->list, false)!= STD_ERR_OK){
            return cps_api_ret_code_ERR;
        }
    }else if(opaque_attr_data == true){
        if(nas_lag_ndi_it_to_obj_fill(ndi_lag_id,param->list, false) != STD_ERR_OK)
            return cps_api_ret_code_ERR;
    }else{
        if(nas_lag_get_all_info(param->list, false) != STD_ERR_OK)
            return cps_api_ret_code_ERR;
    }

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t nas_process_cps_lag_set(void *context, cps_api_transaction_params_t *param,
                                                      size_t ix)
{
    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));
    cps_api_return_code_t rc = cps_api_ret_code_OK;

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "cps_nas_lag_set_function");

    cps_api_object_t cloned = cps_api_object_list_create_obj_and_append(param->prev);
    if(cloned == NULL){
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "obj NULL failure");
        return cps_api_ret_code_ERR;
    }

    cps_api_object_clone(cloned,obj);

    // Acquring lock to avoid pocessing netlink msg from kernel.
    std_mutex_simple_lock_guard lock_t(nas_lag_mutex_lock());

    if( op == cps_api_oper_CREATE){
        return (nas_cps_create_lag(obj));
    }
    if(op == cps_api_oper_DELETE){
        return (nas_cps_delete_lag(obj));
    }
    if(op == cps_api_oper_SET){
        return (nas_cps_set_lag(obj));
    }

    return rc;
}

static cps_api_return_code_t nas_process_cps_lag_state_get(void * context, cps_api_get_params_t * param,
        size_t ix) {
    hal_ifindex_t ifindex = 0;
    bool opaque_attr_data = false;
    nas_obj_id_t ndi_lag_id=0;

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "cps_nas_lag_state_get_function");

    cps_api_object_t obj = cps_api_object_list_get(param->filters, ix);

    cps_api_object_attr_t ndi_lag_id_attr = cps_api_object_attr_get(obj,
                    BASE_IF_LAG_IF_INTERFACES_INTERFACE_LAG_OPAQUE_DATA);

    if(ndi_lag_id_attr != nullptr){
        EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
                   "LAG OPAQUE DATA FOUND %lld!!!",
                   cps_api_object_attr_data_u64(ndi_lag_id_attr));
        ndi_lag_id = cps_api_object_attr_data_u64(ndi_lag_id_attr);
        opaque_attr_data=true;
    }

    std_mutex_simple_lock_guard lock_t(nas_lag_mutex_lock());

    if(nas_lag_get_ifindex_from_obj(obj,&ifindex, true)){
        if(nas_get_lag_intf(ifindex, param->list, true)!= STD_ERR_OK){
            return cps_api_ret_code_ERR;
        }
    }else if(opaque_attr_data == true){
        if(nas_lag_ndi_it_to_obj_fill(ndi_lag_id,param->list, true) != STD_ERR_OK)
            return cps_api_ret_code_ERR;
    }else{
        if(nas_lag_get_all_info(param->list, true) != STD_ERR_OK)
            return cps_api_ret_code_ERR;
    }

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t nas_process_cps_lag_state_set(void *context, cps_api_transaction_params_t *param,
                                                      size_t ix)
{
    /* This shouldn't be called */
    return cps_api_ret_code_ERR;
}

static bool nas_lag_hash_value_set(bool set, bool new_value)
{
    static bool lag_hash_value;
    if (set)
        lag_hash_value = new_value;
    return lag_hash_value;
}

bool nas_lag_hash_value_get(void)
{
    return nas_lag_hash_value_set(false, false);
}

static cps_api_return_code_t nas_process_cps_lag_resilient_get(void * context,
        cps_api_get_params_t *param, size_t ix) {

    bool             hash_enabled = nas_lag_hash_value_get();
    cps_api_object_t ret_obj;

    ret_obj = cps_api_object_list_create_obj_and_append(param->list);
    if (ret_obj == NULL) {
        EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
                "failed creating resilient-hash return object");
        return cps_api_ret_code_ERR;
    }

    cps_api_object_attr_add_u32(ret_obj,
            DELL_IF_IF_INTERFACES_LAG_GLOBALS_RESILIENT_HASH_ENABLE, hash_enabled);

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
            "LAG resilient-hash (%d)", hash_enabled);

    return cps_api_ret_code_OK;
}


/*
 * Scans and updates all routes when
 * (global) resilient hash configuration has changed.
 */
static void nas_process_lag_paths_rh_update(bool new_setting)
{
    nas_lag_master_info_t *nas_lag_entry = NULL;
    nas_lag_master_table_t nas_lag_master_table;
    npu_id_t npu = 0;   //@TODO to retrive NPU ID in multi npu case

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "nas_process_lag_paths_rh_update");

    nas_lag_master_table = nas_get_lag_table();

    for (auto it = nas_lag_master_table.begin();it != nas_lag_master_table.end(); ++it){

        EV_LOGGING(INTERFACE, INFO, "NAS-LAG-CPS", "Inside Loop");
        nas_lag_entry = nas_get_lag_node(it->first);

        if(nas_lag_entry == NULL) {
            EV_LOGGING(INTERFACE, ERR, "NAS-LAG-CPS",
                       "Error finding lagnode %d for RH update operation", it->first);
            return;
        }

        ndi_set_lag_resilient_hash(npu, nas_lag_entry->ndi_lag_id, new_setting);
    }
    return;
}


static cps_api_return_code_t nas_process_cps_lag_resilient_set(void *context,
        cps_api_transaction_params_t *param, size_t ix)
{
    cps_api_return_code_t rc = cps_api_ret_code_OK;
    cps_api_object_it_t   it;
    bool                  is_valid = false;
    bool                  new_setting = false;

    if (param == NULL) {
        EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "no params with lag global-config set");
        return cps_api_ret_code_ERR;
    }

    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    if (obj == NULL) {
        EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "lag global-config set missing");
        return cps_api_ret_code_ERR;
    }

    cps_api_object_it_begin(obj, &it);
    for ( ; cps_api_object_it_valid(&it) ; cps_api_object_it_next(&it) ) {
        cps_api_attr_id_t id = cps_api_object_attr_id(it.attr);

        switch (id) {
            case DELL_IF_IF_INTERFACES_LAG_GLOBALS_RESILIENT_HASH_ENABLE:
                new_setting = cps_api_object_attr_data_u32(it.attr);
                EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "set LAG resilient hash to %s",
                        new_setting ? "enable" : "disable");
                is_valid = true;
                break;

            default:
                EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "ignore unknown leaf id (%d)", id);
                break;
        }
    }

    if ((nas_lag_hash_value_get() != new_setting) && is_valid) {
        /* set global configuration flag */
        nas_lag_hash_value_set(true, new_setting);

        /* update any existing LAG paths */
        nas_process_lag_paths_rh_update(new_setting);
    }

    return rc;
}

cps_api_return_code_t nas_process_cps_lag_resilient_rollback(void * ctx,
                                      cps_api_transaction_params_t * param, size_t ix){

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "LAG global rollback function");
    return cps_api_ret_code_OK;
}


t_std_error nas_process_cps_lag_globals_init(cps_api_operation_handle_t nas_process_cps_handle) {
    cps_api_registration_functions_t f;
    memset(&f,0,sizeof(f));

    f.handle             = nas_process_cps_handle;
    f._read_function     = nas_process_cps_lag_resilient_get;
    f._write_function    = nas_process_cps_lag_resilient_set;
    f._rollback_function = nas_process_cps_lag_resilient_rollback;

    if (!cps_api_key_from_attr_with_qual(&f.key,
               DELL_IF_IF_INTERFACES_LAG_GLOBALS, cps_api_qualifier_TARGET)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "failed translating LAG resilient hash key");
        return STD_ERR(INTERFACE, FAIL, 0);
   }

   if (cps_api_register(&f) != cps_api_ret_code_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG", "failed registering LAG resilient hash object");
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    return STD_ERR_OK;
}


void nas_lag_port_oper_state_cb(npu_id_t npu, npu_port_t port, IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_t status)
{
    ndi_port_t ndi_port;
    ndi_port.npu_id = npu;
    ndi_port.npu_port = port;
    hal_ifindex_t slave_index, master_index;
    bool block_status = true;

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
                      "LAG member port oper status processing");

    if (nas_int_get_if_index_from_npu_port(&slave_index, &ndi_port) != STD_ERR_OK) {
        return;
    }
    std_mutex_simple_lock_guard lock_t(nas_lag_mutex_lock());
    if ( (master_index = nas_get_master_idx(slave_index)) == -1 ) {
        return; // not a part of any lag  so nothing to do
    }
    nas_lag_master_info_t *nas_lag_entry= NULL;
    if ((nas_lag_entry = nas_get_lag_node(master_index)) == NULL ) {
        return;
    }

    nas_lag_entry->port_oper_list[slave_index]=(status == IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_UP) ?
                                                true : false;

    /* if Oper up and port not in block list, then reset egress_disable */
    if ((nas_lag_entry->block_port_list.find(slave_index) ==
         nas_lag_entry->block_port_list.end()) &&
         (status == IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_UP) ) {

         block_status = false;
         if (!nas_lag_entry->oper_status) {
            nas_lag_entry->oper_status = true;
            lag_state_object_publish(nas_lag_entry,true);
         }
    }

    if (nas_lag_block_port(nas_lag_entry, slave_index, block_status) != STD_ERR_OK){
            EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                "Error Block/unblock Port %d lag %d ",slave_index, master_index);
        return ;
    }

    if(status == IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_DOWN){
        bool publish_oper_down = true;
        for(const auto &it : nas_lag_entry->port_oper_list){
            if(it.second == true){
                publish_oper_down = false;
                break;
            }
        }

        if(publish_oper_down){
            nas_lag_entry->oper_status = false;
            lag_state_object_publish(nas_lag_entry,false);
        }
    }

}

static bool nas_lag_process_port_association(hal_ifindex_t ifindex, npu_id_t npu, port_t port,bool add){

    std_mutex_simple_lock_guard lock_t(nas_lag_mutex_lock());
    auto  m_list = nas_intf_get_master(ifindex);
    for(auto it : m_list){
        if(it.type == nas_int_type_LAG){

            if(add){
                nas_lag_master_info_t *nas_lag_entry = nas_get_lag_node(it.m_if_idx);

                if(nas_lag_entry == NULL){
                    EV_LOGGING(INTERFACE,ERR,"NAS-LAG-MAP","No LAG entry for ifindex %d exist",
                            it.m_if_idx);
                    continue;
                }
                bool prev_oper_status = nas_lag_entry->oper_status;

                if(nas_lag_member_add(nas_lag_entry->ifindex,ifindex) != STD_ERR_OK){
                    EV_LOGGING(INTERFACE, ERR, "NAS-LAG-MAP","Error inserting index %d in list",
                                ifindex);
                    continue;
                }

                if (prev_oper_status != nas_lag_entry->oper_status) {
                    lag_state_object_publish(nas_lag_entry, nas_lag_entry->oper_status);
                }

            }else{
                if((nas_lag_member_delete(0,ifindex) != STD_ERR_OK)){
                    EV_LOGGING(INTERFACE, ERR, "NAS-LAG-MAP","Error deleting lag"
                                " interface %d from NPU", ifindex);
                    continue;
                }
            }
        }
    }

    return true;
}


static bool nas_lag_if_set_handler(cps_api_object_t obj, void *context)
{
    nas_int_port_mapping_t port_mapping;
    if(!nas_get_phy_port_mapping_change( obj, &port_mapping)){
        EV_LOGGING(INTERFACE,DEBUG,"NAS-LAG-MAP","Interface event is not an "
                "association/dis-association event");
        return true;
    }

    cps_api_object_attr_t npu_attr = cps_api_object_attr_get(obj,
                                    BASE_IF_PHY_IF_INTERFACES_INTERFACE_NPU_ID);
    cps_api_object_attr_t port_attr = cps_api_object_attr_get(obj,
                                    BASE_IF_PHY_IF_INTERFACES_INTERFACE_PORT_ID);
    cps_api_object_attr_t if_index_attr = cps_api_get_key_data(obj,
                                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);

    if (npu_attr == nullptr || port_attr == nullptr || if_index_attr == nullptr) {
        EV_LOGGING(INTERFACE,DEBUG, "NAS-LAG-MAP", "Interface object does not have if-index/npu/port");
        return true;
    }

    hal_ifindex_t ifidx = cps_api_object_attr_data_u32(if_index_attr);
    npu_id_t npu = cps_api_object_attr_data_u32(npu_attr);
    npu_port_t port = cps_api_object_attr_data_u32(port_attr);

    bool add = (port_mapping == nas_int_phy_port_MAPPED) ? true : false;

    return nas_lag_process_port_association(ifidx,npu,port,add);

}

t_std_error nas_cps_lag_init(cps_api_operation_handle_t lag_intf_handle) {

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG", "CPS LAG Initialize");

    /* register global LAG CPS container */
    nas_process_cps_lag_globals_init(lag_intf_handle);

    if (intf_obj_handler_registration(obj_INTF, nas_int_type_LAG, nas_process_cps_lag_get, nas_process_cps_lag_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-LAG-INIT",
                   "Failed to register LAG interface CPS handler");
           return STD_ERR(INTERFACE,FAIL,0);
    }

    if (intf_obj_handler_registration(obj_INTF_STATE, nas_int_type_LAG, nas_process_cps_lag_state_get, nas_process_cps_lag_state_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-LAG-INIT",
                   "Failed to register LAG interface-state CPS handler");
           return STD_ERR(INTERFACE,FAIL,0);
    }

    /*  register a handler for physical port oper state change */
    nas_int_oper_state_register_cb(nas_lag_port_oper_state_cb);

    if (cps_api_event_service_init() != cps_api_ret_code_OK) {
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (cps_api_event_thread_init() != cps_api_ret_code_OK) {
        return STD_ERR(INTERFACE,FAIL,0);
    }

    cps_api_event_reg_t reg;
    cps_api_key_t key;

    memset(&reg, 0, sizeof(cps_api_event_reg_t));

    if (!cps_api_key_from_attr_with_qual(&key,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                        cps_api_qualifier_OBSERVED)) {
           EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-INIT", "Cannot create a key for interface event");
           return STD_ERR(INTERFACE,FAIL,0);
    }

    reg.objects = &key;
    reg.number_of_objects = 1;

    if (cps_api_event_thread_reg(&reg, nas_lag_if_set_handler, NULL)!= cps_api_ret_code_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-INIT", "Cannot register interface operation event");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return STD_ERR_OK;;
}
