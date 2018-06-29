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
#include "event_log.h"
#include "nas_int_vlan.h"
#include "nas_int_lag_api.h"
#include "nas_ndi_vlan.h"
#include "nas_ndi_lag.h"

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

/* Function to add or delete all the LAG members in SAI
 * */
t_std_error nas_add_or_del_lag_in_vlan(hal_ifindex_t lag_index, hal_vlan_id_t vlan_id,
                                              nas_port_mode_t port_mode, bool add_flag, bool roll_bk)
{
    ndi_obj_id_t *untagged_lag = NULL;
    ndi_obj_id_t *tagged_lag = NULL;
    ndi_obj_id_t ndi_lag_id;
    t_std_error ret = STD_ERR_OK;
    size_t tag_cnt =0, untag_cnt=0;
    nas_bridge_t *vlan_entry = nas_get_bridge_node_from_vid(vlan_id);
    nas_lag_master_info_t *nas_lag_entry = nas_get_lag_node(lag_index);

    if (!vlan_entry || !nas_lag_entry) {
        return (STD_ERR(INTERFACE,FAIL, 0));
    }

    (port_mode == NAS_PORT_TAGGED) ?  (tagged_lag = &ndi_lag_id, tag_cnt=1) :
                                      (untagged_lag = &ndi_lag_id,untag_cnt=1);

    EV_LOGGING(INTERFACE, INFO, "NAS-VLAN",
            "LAG entry %d being updated %d in Vlan %d",
            lag_index, add_flag, vlan_id);

    if ((ret = nas_lag_get_ndi_lag_id (lag_index, &ndi_lag_id)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, INFO, "NAS-VLAN", "Error finding NDI LAG ID %d  %d %d",
                             lag_index, vlan_id, add_flag);
        return ret;
    }

    if (add_flag) {
        ret = ndi_add_lag_to_vlan(0, vlan_id, tagged_lag, tag_cnt, untagged_lag, untag_cnt);
        if (port_mode == NAS_PORT_UNTAGGED) {
            ndi_set_lag_pvid(0, ndi_lag_id, vlan_id);
        }

        if (nas_lag_entry->oper_status) {
           (vlan_entry->oper_list)[lag_index] = true;
           vlan_entry->oper_status = IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_UP;
        } else {
           (vlan_entry->oper_list)[lag_index] = true;
        }
        nas_update_port_to_vlans_map(lag_index, vlan_entry->ifindex, true);
    } else {
        ret = ndi_del_lag_from_vlan(0, vlan_id, tagged_lag, tag_cnt, untagged_lag, untag_cnt);
        (vlan_entry->oper_list).erase(lag_index);
        vlan_entry->oper_status = IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_DOWN;
        for (auto oper_status: vlan_entry->oper_list) {
            if (oper_status.second) {
                vlan_entry->oper_status = IF_INTERFACES_STATE_INTERFACE_OPER_STATUS_UP;
                break;
            }
        }
        nas_update_port_to_vlans_map(lag_index, vlan_entry->ifindex, false);
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

    bool roll_bk_sup = (roll_bk==nullptr)? false:true;

    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
          "Adding LAG %d to bridge %d in OS", lag_index, p_bridge->ifindex);

    if(cps_add == true) {
        /* Came via CPS */
        if(nas_cps_add_port_to_os(p_bridge->ifindex, p_bridge->vlan_id, port_mode, lag_index,
                p_bridge->mtu, p_bridge->mode) != STD_ERR_OK) {
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

    if (nas_add_or_del_lag_in_vlan(lag_index, p_bridge->vlan_id,
                               port_mode, true, roll_bk_sup) != STD_ERR_OK) {
        /* if Roll back */
        if (roll_bk_sup == true) {
            nas_cps_del_port_from_os(p_bridge->vlan_id, lag_index, port_mode,p_bridge->mode);
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
        nas_insert_link_node(&p_bridge->tagged_lag.port_list, p_link_node);
    }
    else {
        if ((nas_get_link_node(&p_bridge->untagged_lag.port_list, lag_index)) == NULL) {
            /* lag index could be present in the untagged list so check before adding */
            nas_insert_link_node(&p_bridge->untagged_lag.port_list, p_link_node);
        }

    }
    if (roll_bk_sup) {
        roll_bk->lag_add_list[lag_index] = port_mode;
        EV_LOGGING(INTERFACE, INFO,"NAS-Vlan",
           "Updating lag roll bk add list vlan id %d, lag id %d \n",
           p_bridge->vlan_id, lag_index);
    }
    nas_port_list_t publish_list = {lag_index};
    nas_publish_vlan_port_list(p_bridge, publish_list, port_mode, cps_api_oper_CREATE);
    return STD_ERR_OK;
}

t_std_error  nas_handle_lag_del_from_vlan(nas_bridge_t *p_bridge, hal_ifindex_t lag_index,
                nas_port_mode_t port_mode, bool cps_del ,vlan_roll_bk_t *roll_bk)
{

    bool roll_bk_sup = (roll_bk==nullptr)? false:true;


    if (nas_add_or_del_lag_in_vlan(lag_index, p_bridge->vlan_id,
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
        if(nas_cps_del_port_from_os(p_bridge->vlan_id, lag_index, port_mode,p_bridge->mode) !=
                            STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                  "Error deleting lag %d from bridge %d in OS", lag_index, p_bridge->ifindex);

            /* If roll back  : add back passed NDI ports */
            if (roll_bk_sup) {
                nas_add_or_del_lag_in_vlan(lag_index, p_bridge->vlan_id,
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
        nas_process_lag_for_vlan_del(&p_bridge->tagged_lag, lag_index);
    }
    else {
        nas_process_lag_for_vlan_del(&p_bridge->untagged_lag, lag_index);
    }
    if (roll_bk) {
        EV_LOGGING(INTERFACE, INFO,"NAS-Vlan",
             "Updating lag roll bk delete list vlan id %d, lag id %d \n",
              p_bridge->vlan_id, lag_index);

        roll_bk->lag_del_list[lag_index] = port_mode;
    }
    nas_port_list_t publish_list = {lag_index};
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
    nas_list_t *p_list = NULL;
    nas_list_node_t *p_link_node = NULL, *p_temp_node = NULL;
    hal_ifindex_t ifindex = 0;
    t_std_error rc = STD_ERR_OK;

    if(port_mode == NAS_PORT_TAGGED) {
        p_list = &(p_bridge->tagged_lag);
    }
    else {
        p_list = &(p_bridge->untagged_lag);
    }

    p_link_node = nas_get_first_link_node(&(p_list->port_list));
    while (p_link_node != NULL) {
        ifindex = p_link_node->ifindex;
        p_temp_node = p_link_node;
        p_link_node = nas_get_next_link_node(&p_list->port_list, p_temp_node);

        if (port_index_list.find(ifindex) == port_index_list.end()){
            EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                   "Lag %d does not exist in the SET request, delete it",
                    ifindex);

            if ((rc = nas_handle_lag_del_from_vlan(p_bridge, ifindex,
                                           port_mode, true, roll_bk)) != STD_ERR_OK ){
                EV_LOGGING(INTERFACE, INFO, "NAS-Vlan", "nas_handle_lag_index_in_cps_set: nas_handle_lag_del_from_vlan failed \n");
                return rc;
            };
        }
    } //while
    return STD_ERR_OK;
}
