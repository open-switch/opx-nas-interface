/*
 * Copyright (c) 2016 Dell Inc.
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
 * nas_vlan_lag_api.cpp
 *
 */

#include "std_error_codes.h"
#include "cps_api_object_attr.h"
#include "cps_api_object_key.h"
#include "cps_api_object.h"
#include "event_log.h"
#include "event_log_types.h"
#include "nas_vlan_lag.h"
#include "nas_int_vlan.h"
#include "nas_int_utils.h"
#include "hal_interface_defaults.h"
#include "dell-base-if.h"
#include "dell-interface.h"
#include "std_mutex_lock.h"
#include "nas_if_utils.h"
#include "nas_int_lag_api.h"
#include "nas_ndi_vlan.h"
#include "nas_ndi_lag.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>


typedef std::unordered_set <hal_ifindex_t> nas_vlan_untag_lag_list_t;
typedef std::unordered_set <hal_ifindex_t> nas_vlan_tag_lag_list_t;
typedef std::vector <ndi_port_t> nas_lag_mem_ndi_list_t;
typedef std::unordered_map <hal_ifindex_t, nas_vlan_lag_t> nas_lag_list_t;
typedef std::unordered_map <hal_ifindex_t, nas_vlan_lag_t >::iterator nas_lag_list_it;

static auto lag_list = new nas_lag_list_t;

static std_mutex_lock_create_static_init_rec(vlan_lag_lock);
std_mutex_type_t *vlan_lag_mutex_lock()
{
    return &vlan_lag_lock;
}

static t_std_error nas_vlan_create_lag(hal_ifindex_t ifindex)
{
    nas_lag_list_it lag_it = lag_list->find(ifindex);

    EV_LOGGING(INTERFACE, INFO , "NAS-VLAN-LAG",
            "Add LAG %d using CPS", ifindex);

        /* Now that LAG exists, proceed further */
    if(lag_it == lag_list->end()) {
        nas_vlan_lag_t vlan_lag_entry;

        vlan_lag_entry.lag_index = ifindex;
        vlan_lag_entry.vlan_enable = false;
        lag_list->insert({ifindex, vlan_lag_entry});
    }
    else {
        EV_LOGGING(INTERFACE, INFO , "NAS-VLAN-LAG",
                " Lag IfIndex %d already created", ifindex);
        return (STD_ERR(INTERFACE,FAIL, 0));
    }
    return STD_ERR_OK;
}

static t_std_error nas_vlan_delete_lag(hal_ifindex_t ifindex) {
    nas_lag_list_it lag_it = lag_list->find(ifindex);

    EV_LOGGING(INTERFACE, INFO, "NAS-CPS-LAG",
            "Delete LAG %d using CPS", ifindex);

    if(lag_it == lag_list->end()) {
        EV_LOGGING(INTERFACE, ERR, "NAS-CPS-LAG",
                "Missing Lag IfIndex %d ", ifindex);
        return (STD_ERR(INTERFACE,FAIL, 0));
    }

    lag_list->erase(ifindex);

    return STD_ERR_OK;
}

static 
void nas_set_vlan_enable(nas_vlan_lag_t *lag_entry) {

    if (lag_entry->tagged_list.empty() && lag_entry->untagged_list.empty()) {
        lag_entry->vlan_enable = false;
    }
}

void nas_handle_del_vlan_lag(hal_vlan_id_t vlan_id) {

    for (auto lag_it = lag_list->begin(); lag_it != lag_list->end(); ++lag_it) {
        nas_vlan_lag_t *lag_entry = &(lag_it->second);
        if (lag_entry->vlan_enable != true) {
            continue;

        }

        auto vlan_it_t = lag_entry->tagged_list.find(vlan_id);
        if (vlan_it_t != lag_entry->tagged_list.end()) {
            if (nas_del_tagged_lag_intf(lag_entry->lag_index, vlan_id) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE, ERR , "NAS-CPS-LAG",
                 "Error deleting tagged bond %d in vlan %d", lag_entry->lag_index, vlan_id);
            }
            lag_entry->tagged_list.erase(vlan_id);

        }

        auto vlan_it_ut = lag_entry->untagged_list.find(vlan_id);
        if (vlan_it_ut != lag_entry->untagged_list.end()) {
            /* Del vlan tag id from lag_entry */
            lag_entry->untagged_list.erase(vlan_id);

        }
        nas_set_vlan_enable(lag_entry);
    }
}

t_std_error nas_delete_lag_from_vlan_in_npu(hal_ifindex_t ifindex ,hal_vlan_id_t vid, nas_port_mode_t port_mode) {

    ndi_obj_id_t *untagged_lag = NULL;
    ndi_obj_id_t *tagged_lag = NULL;
    size_t tag_cnt =0, untag_cnt=0;
    ndi_obj_id_t ndi_lag_id;
    t_std_error ret;

    (port_mode == NAS_PORT_TAGGED) ?  (tagged_lag = &ndi_lag_id, tag_cnt=1) :
                                      (untagged_lag = &ndi_lag_id,untag_cnt=1);


    if ((ret = nas_lag_get_ndi_lag_id (ifindex, &ndi_lag_id)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, INFO, "NAS-VLAN", "Error finding NDI LAG ID %d  %d ",
                 ifindex, vid);
        return ret;
    }
    ret = ndi_del_lag_from_vlan(0, vid, tagged_lag, tag_cnt, untagged_lag, untag_cnt);
    if (ret != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN", "nas_delete_lag_from_vlan failed to delete LAG from vlan id <%d>",  vid );
        return ret;
    }
    return STD_ERR_OK;
}

void nas_vlan_set_tagged_lag_mtu(hal_vlan_id_t vlan_id,uint32_t mtu) {

    for (auto lag_it = lag_list->begin(); lag_it != lag_list->end(); ++lag_it) {
        nas_vlan_lag_t *lag_entry = &(lag_it->second);
        if (lag_entry->vlan_enable != true) {
            continue;
        }

        auto vlan_it_t = lag_entry->tagged_list.find(vlan_id);
        if (vlan_it_t != lag_entry->tagged_list.end()) {
            if(!nas_set_vlan_member_port_mtu(lag_entry->lag_index, mtu, vlan_id)){
                EV_LOGGING(INTERFACE, DEBUG , "NAS-CPS-LAG","Error setting mtu to %d for tagged "
                            "bond %d in vlan %d", mtu,lag_entry->lag_index, vlan_id);
            }
        }
    }

}

void nas_dump_val_lag_list()
{
    int i =1;
    for (auto lag_it = lag_list->begin();
        lag_it != lag_list->end(); ++lag_it) {

        auto lag_entry = &(lag_it->second);

        printf("Lag Entry %d ------------ \n", i);
        printf("lag_index %d , %d  ,vlan_enable: %d \n",
                lag_it->first, lag_entry->lag_index ,lag_entry->vlan_enable);
        printf("lag members index: ");
        for (auto it = lag_entry->lag_mem.begin(); it != lag_entry->lag_mem.end(); ++it) {
            printf(" %d, " ,it->first);
        }
        printf("\n");
        printf("Tagged vlan_ids: ");

        for (auto vlan_it = lag_entry->tagged_list.begin();
                              vlan_it != lag_entry->tagged_list.end(); ++vlan_it) {
            printf(" %d, ", *(vlan_it));

        }
        printf("\n");
        printf("Untagged vlan_ids: ");
        for (auto vlan_uit = lag_entry->untagged_list.begin();
            vlan_uit != lag_entry->untagged_list.end(); ++vlan_uit) {

            printf(" %d, ", *(vlan_uit));
        }
        printf("\n\n");
        i++;
    }

}

/*  TODO LAG event is not processed anymore. Will be cleaned up in the next phase of bridgeport  */
bool nas_vlan_lag_event_func_cb(const cps_api_object_t obj) {
    cps_api_object_attr_t lag_attr = cps_api_get_key_data(obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);

    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
           "Callback for LAG notification!!!");

    if (lag_attr == NULL) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
               "Missing ifindex attribute for CPS LAG CB");
        return false;
    }
    std_mutex_simple_lock_guard lock_t(vlan_lag_mutex_lock());

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));
    hal_ifindex_t ifindex = (hal_ifindex_t)cps_api_object_attr_data_u32(lag_attr);

    if(op == cps_api_oper_CREATE) {
        nas_vlan_create_lag(ifindex);
    }
    else if (op == cps_api_oper_DELETE) {
        nas_vlan_delete_lag(ifindex);
    }
    return true;
}

/* Function to add or delete all the LAG members in SAI
 * */
static t_std_error nas_add_or_del_lag_in_vlan(nas_vlan_lag_t *lag_entry, hal_vlan_id_t vlan_id,
                                              nas_port_mode_t port_mode, bool add_flag, bool roll_bk)
{
    nas_lag_mem_list_it it;
    nas_lag_mem_ndi_list_t lag_mem_ndi;
    ndi_obj_id_t *untagged_lag = NULL;
    ndi_obj_id_t *tagged_lag = NULL;
    ndi_obj_id_t ndi_lag_id;
    t_std_error ret = STD_ERR_OK;
    size_t tag_cnt =0, untag_cnt=0;

    (port_mode == NAS_PORT_TAGGED) ?  (tagged_lag = &ndi_lag_id, tag_cnt=1) :
                                      (untagged_lag = &ndi_lag_id,untag_cnt=1);

    EV_LOGGING(INTERFACE, INFO, "NAS-VLAN",
            "LAG entry %d being updated %d in Vlan %d",
            lag_entry->lag_index, add_flag, vlan_id);

    if ((ret = nas_lag_get_ndi_lag_id (lag_entry->lag_index, &ndi_lag_id)) != STD_ERR_OK) {
    EV_LOGGING(INTERFACE, INFO, "NAS-VLAN", "Error finding NDI LAG ID %d  %d %d",
                         lag_entry->lag_index, vlan_id, add_flag);
    return ret;
    }

    if (add_flag) {
        ret = ndi_add_lag_to_vlan(0, vlan_id, tagged_lag, tag_cnt, untagged_lag, untag_cnt);
        if (port_mode == NAS_PORT_UNTAGGED) {
            ndi_set_lag_pvid(0, ndi_lag_id, vlan_id);
        }
    } else {
        ret = ndi_del_lag_from_vlan(0, vlan_id, tagged_lag, tag_cnt, untagged_lag, untag_cnt);
    }
    /* TODO rollback case */

    return ret;
}

/*  Function to add LAG interface to kernel and in the bridge
 *  Once kernel succesful, add all the members in SAI
 *  Create LAG to vlan mapping for traversal when new member gets added or deleted
 * */
t_std_error nas_handle_lag_add_to_vlan(nas_bridge_t *p_bridge, hal_ifindex_t lag_index,
                              nas_port_mode_t port_mode, bool cps_add, vlan_roll_bk_t *roll_bk) {

    nas_vlan_lag_t *lag_entry;
    nas_lag_mem_list_it it;
    bool roll_bk_sup = (roll_bk==nullptr)? false:true;

    nas_lag_list_it lag_it = lag_list->find(lag_index);
    if(lag_it == lag_list->end()) {
        EV_LOGGING(INTERFACE, ERR , "NAS-Vlan",
               "Invalid LAG %d being added to Vlan %d", lag_index, p_bridge->vlan_id);
        return (STD_ERR(INTERFACE,FAIL, 0));
    }

    lag_entry = &(lag_it->second);

    /* Now that LAG exists, add interface to kernel */
    nas_vlan_list_it vlan_it;
    if(port_mode == NAS_PORT_TAGGED) {
        vlan_it = lag_entry->tagged_list.find(p_bridge->vlan_id);
        if(vlan_it != lag_entry->tagged_list.end()) {
            EV_LOGGING(INTERFACE, INFO , "NAS-Vlan",
              "LAG %d already added to Vlan %d", lag_index, p_bridge->vlan_id);
            return STD_ERR_OK;
        }
    }else {
        vlan_it = lag_entry->untagged_list.find(p_bridge->vlan_id);
        if(vlan_it != lag_entry->untagged_list.end()) {
            EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                 "LAG %d already added to Vlan %d", lag_index, p_bridge->vlan_id);
            return STD_ERR_OK;
        }
    }

    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
          "Adding LAG %d to bridge %d in OS", lag_index, p_bridge->ifindex);

    if(cps_add == true) {
        /* Came via CPS */
        if(nas_cps_add_port_to_os(p_bridge->ifindex, p_bridge->vlan_id, port_mode, lag_index,p_bridge->mtu) !=
            STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                 "Error adding port %d to bridge %d in OS", lag_index, p_bridge->ifindex);
            return STD_ERR(INTERFACE,FAIL, 0);
        }
    }
    if_master_info_t master_info = { nas_int_type_VLAN, port_mode, p_bridge->ifindex};
    BASE_IF_MODE_t intf_mode = nas_intf_get_mode(lag_index);
    if(!nas_intf_add_master(lag_index, master_info)){
        EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN","Failed to add master for vlan memeber lag");
    } else {
        BASE_IF_MODE_t new_mode = nas_intf_get_mode(lag_index);
        if (new_mode != intf_mode) {
            if (nas_intf_handle_intf_mode_change(lag_index, new_mode) == false) {
                EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN", "Update to NAS-L3 about interface mode change failed(%d)", lag_index);
            }
        }
    }

    if (nas_add_or_del_lag_in_vlan(lag_entry, p_bridge->vlan_id,
                               port_mode, true, roll_bk_sup) != STD_ERR_OK) {
        /* if Roll back */
        if (roll_bk_sup == true) {
            nas_cps_del_port_from_os(p_bridge->vlan_id, lag_index, port_mode);
         }
         return STD_ERR(INTERFACE,FAIL, 0);

    }
    nas_list_node_t *p_link_node = (nas_list_node_t *)malloc(sizeof(nas_list_node_t));
    if (p_link_node != NULL) {
        memset(p_link_node, 0, sizeof(nas_list_node_t));
        p_link_node->ifindex = lag_index;
    }
    /* Add the vlan id to LAG - need it to traverse when member add/del to
     * LAG happens*/
    if(port_mode == NAS_PORT_TAGGED) {
        lag_entry->tagged_list.insert(p_bridge->vlan_id);
        nas_insert_link_node(&p_bridge->tagged_lag.port_list, p_link_node);
    }
    else {
        if ((nas_get_link_node(&p_bridge->untagged_lag.port_list, lag_index)) == NULL) {
            /* lag index could be present in the untagged list so check before adding */
            nas_insert_link_node(&p_bridge->untagged_lag.port_list, p_link_node);
        }

        lag_entry->untagged_list.insert(p_bridge->vlan_id);
    }
    lag_entry->vlan_enable = true;
    if (roll_bk_sup) {
        roll_bk->lag_add_list[lag_entry->lag_index] = port_mode;
        EV_LOGGING(INTERFACE, INFO,"NAS-Vlan",
           "Updating lag roll bk add list vlan id %d, lag id %d \n",
           p_bridge->vlan_id, lag_entry->lag_index);
    }
    nas_port_list_t publish_list = {lag_entry->lag_index};
    nas_publish_vlan_port_list(p_bridge, publish_list, port_mode, cps_api_oper_CREATE);
    return STD_ERR_OK;
}

t_std_error  nas_handle_lag_del_from_vlan(nas_bridge_t *p_bridge, hal_ifindex_t lag_index,
                nas_port_mode_t port_mode, bool cps_del ,vlan_roll_bk_t *roll_bk)
{

    nas_vlan_lag_t *lag_entry;
    nas_lag_mem_list_it it;
    bool roll_bk_sup = (roll_bk==nullptr)? false:true;

    nas_lag_list_it lag_it = lag_list->find(lag_index);
    if (lag_it == lag_list->end()) {
        EV_LOGGING(INTERFACE, ERR , "NAS-Vlan",
           "Invalid LAG %d being added to Vlan %d", lag_index, p_bridge->vlan_id);
        return (STD_ERR(INTERFACE,FAIL, 0));
    }
    lag_entry = &(lag_it->second);
    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
         "LAG %d being deleted from Vlan %d", lag_index, p_bridge->vlan_id);

    if (nas_add_or_del_lag_in_vlan(lag_entry, p_bridge->vlan_id,
          port_mode, false, roll_bk_sup) != STD_ERR_OK){
        return STD_ERR(INTERFACE,FAIL, 0);
    }

    if_master_info_t master_info = { nas_int_type_VLAN, port_mode, p_bridge->ifindex};
    BASE_IF_MODE_t intf_mode = nas_intf_get_mode(lag_index);
    if(!nas_intf_del_master(lag_index, master_info)){
        EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN","Failed to delete master for vlan memeber lag");
    } else {
        BASE_IF_MODE_t new_mode = nas_intf_get_mode(lag_index);
        if (new_mode != intf_mode) {
            if (nas_intf_handle_intf_mode_change(lag_index, new_mode) == false) {
                EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN", "Update to NAS-L3 about interface mode change failed(%d)", lag_index);
            }
        }
    }

    /* Delete from Kernel now */
    if(cps_del == true) {
        if(nas_cps_del_port_from_os(p_bridge->vlan_id, lag_index, port_mode) !=
                            STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                  "Error deleting lag %d from bridge %d in OS", lag_index, p_bridge->ifindex);

            /* If roll back  : add back passed NDI ports */
            if (roll_bk_sup) {
                nas_add_or_del_lag_in_vlan(lag_entry, p_bridge->vlan_id,
                                   port_mode, true, true);
            }
            return STD_ERR(INTERFACE,FAIL, 0);
        }
    }

    if(!nas_intf_cleanup_l2mc_config(lag_index,  p_bridge->vlan_id)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
               "Error cleaning L2MC membership for Lag interface %d", lag_index);
    }
    if(port_mode == NAS_PORT_TAGGED) {
        lag_entry->tagged_list.erase(p_bridge->vlan_id);
        nas_process_lag_for_vlan_del(&p_bridge->tagged_lag, lag_index);
    }
    else {
        lag_entry->untagged_list.erase(p_bridge->vlan_id);
        nas_process_lag_for_vlan_del(&p_bridge->untagged_lag, lag_index);
    }
    if (lag_entry->tagged_list.empty() && lag_entry->untagged_list.empty()) {
        lag_entry->vlan_enable = false;
    }
    if (roll_bk) {
        EV_LOGGING(INTERFACE, INFO,"NAS-Vlan",
             "Updating lag roll bk delete list vlan id %d, lag id %d \n",
              p_bridge->vlan_id, lag_entry->lag_index);

        roll_bk->lag_del_list[lag_entry->lag_index] = port_mode;
    }
    nas_port_list_t publish_list = {lag_entry->lag_index};
    nas_publish_vlan_port_list(p_bridge, publish_list, port_mode, cps_api_oper_DELETE);
    return STD_ERR_OK;
}

/**
 * This function walk through all the LAG groups and checks if the incoming vlan list
 * has removed the LAG, then need to clean up from SAI.
 *
 */
t_std_error nas_handle_lag_index_in_cps_set(nas_bridge_t *p_bridge, nas_port_list_t &port_index_list,
                                            nas_port_mode_t port_mode, vlan_roll_bk_t *roll_bk)
{
    t_std_error rc = STD_ERR_OK;

    for(auto lag_it = lag_list->begin(); lag_it != lag_list->end(); ++lag_it) {
        nas_vlan_lag_t *lag_entry = &(lag_it->second);
        //check in the list only if VLAN is enabled for this LAG
        nas_vlan_list_it vlan_it;
        if(port_mode == NAS_PORT_TAGGED) {
            vlan_it = lag_entry->tagged_list.find(p_bridge->vlan_id);
            if(vlan_it == lag_entry->tagged_list.end())
                continue;
        }else {
            vlan_it = lag_entry->untagged_list.find(p_bridge->vlan_id);
            if(vlan_it == lag_entry->untagged_list.end())
                            continue;
        }

        if((lag_entry->vlan_enable == true) &&
           (port_index_list.find(lag_entry->lag_index) == port_index_list.end())){
            EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                   "LAG index %d not found in incoming list, delete from Vlan %d", lag_entry->lag_index, p_bridge->vlan_id);

            if ((rc = nas_handle_lag_del_from_vlan(p_bridge, lag_entry->lag_index,
                                           port_mode, true, roll_bk)) != STD_ERR_OK ){
                EV_LOGGING(INTERFACE, INFO, "NAS-Vlan", "nas_handle_lag_index_in_cps_set: nas_handle_lag_del_from_vlan failed \n");
                return rc;
            };
        }
    }


    return STD_ERR_OK;
}

t_std_error nas_base_handle_lag_del(hal_ifindex_t br_index, hal_ifindex_t lag_index,
                                    hal_vlan_id_t vlan_id )
{
    nas_vlan_lag_t *lag_entry;
    nas_lag_mem_list_it it;
    nas_lag_list_it lag_it = lag_list->find(lag_index);
    if(lag_it == lag_list->end()) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
               "Invalid LAG %d being deleted from Vlan %d", lag_index, vlan_id);
        return (STD_ERR(INTERFACE,FAIL, 0));
    }

    lag_entry = &(lag_it->second);

    nas_vlan_list_it vlan_it;
    nas_port_mode_t port_mode;
    vlan_it = lag_entry->tagged_list.find(vlan_id);
    if(vlan_it != lag_entry->tagged_list.end()) {
        port_mode = NAS_PORT_TAGGED;
    }
    else {
        vlan_it = lag_entry->untagged_list.find(vlan_id);

        if(vlan_it != lag_entry->untagged_list.end()) {
            port_mode = NAS_PORT_UNTAGGED;
        }
        else {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                   "Error finding LAG %d in VLAN lists", lag_index);
            return STD_ERR(INTERFACE,FAIL, 0);
        }
    }

    nas_add_or_del_lag_in_vlan(lag_entry, vlan_id,
                               port_mode, false, false);

    if(port_mode == NAS_PORT_TAGGED) {
        lag_entry->tagged_list.erase(vlan_id);
    }
    else {
        lag_entry->untagged_list.erase(vlan_id);
    }
    if (lag_entry->tagged_list.empty() && lag_entry->untagged_list.empty()) {
        lag_entry->vlan_enable = false;
    }
    return STD_ERR_OK;
}
