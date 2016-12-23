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
 * filename: nas_int_bridge.c
 */


#include "nas_int_bridge.h"
#include "std_mutex_lock.h"
#include "event_log.h"
#include "event_log_types.h"
#include "hal_interface_common.h"
#include "nas_int_utils.h"
#include "nas_int_vlan.h"
#include "nas_os_vlan.h"
#include "cps_api_object.h"
#include "std_utils.h"
#include <unordered_map>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static bridge_list_t bridge_list;
typedef std::unordered_map <hal_vlan_id_t, hal_ifindex_t>  vlanid_to_bridge_t;
vlanid_to_bridge_t vid_to_bridge;

//@TODO gcc define to allow use of recursive mutexes is required.
//then switch to recursive mutex

static std_mutex_lock_create_static_init_fast(br_lock);


bool nas_vlan_id_in_use(hal_vlan_id_t vlan_id)
{
    bool ret = true;

    auto iter = vid_to_bridge.find(vlan_id);
    if (iter == vid_to_bridge.end()){
        ret = false;
    } else {
        EV_LOGGING(INTERFACE, INFO, "NAS-Br",
           "Vid to bridge already exist for bridge %d vlan-id %d",
           iter->second, vlan_id);
    }
    return ret;
}

bool nas_handle_vid_to_br(hal_vlan_id_t vlan_id, hal_ifindex_t if_index)
{
    if (nas_vlan_id_in_use(vlan_id)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Br",
        "Failed to map vid %d to bridge id %d", vlan_id ,if_index);
        return false;
    }
    vid_to_bridge[vlan_id] = if_index;
    return true;
}

void nas_del_vlan_to_bridge_map (hal_vlan_id_t vlan_id, hal_ifindex_t if_index) {

    auto iter = vid_to_bridge.find(vlan_id);
    if (iter != vid_to_bridge.end()) {
          vid_to_bridge.erase(iter);
    }
}


t_std_error nas_vlan_get_all_info(cps_api_object_list_t list)
{

    EV_LOGGING(INTERFACE, DEBUG, "NAS-Vlan",
           "Getting all vlan interfaces");

    for ( auto local_it = bridge_list.begin(); local_it!= bridge_list.end(); ++local_it ) {
        cps_api_object_t obj = cps_api_object_list_create_obj_and_append(list);
        nas_pack_vlan_if(obj, &local_it->second);
    }
    return STD_ERR_OK;
}

void nas_bridge_lock(void)
{
    std_mutex_lock (&br_lock);
}

void nas_bridge_unlock(void)
{
    std_mutex_unlock (&br_lock);
}


nas_bridge_t *nas_get_bridge_node(hal_ifindex_t index)
{

    auto it = bridge_list.find(index);

    if (it == bridge_list.end()) {
       EV_LOGGING(INTERFACE, INFO, "NAS-Br","Error finding bridge intf %d", index);
       return NULL;
    }
    EV_LOGGING(INTERFACE, DEBUG, "NAS-Br", "Found Bridge node %d", index);
    return &it->second;
}

nas_bridge_t *nas_get_bridge_node_from_name (const char *vlan_if_name)
{
    if (vlan_if_name == NULL) return NULL;
    hal_ifindex_t if_index;
    if (nas_int_name_to_if_index(&if_index, vlan_if_name) == STD_ERR_OK) {
        return(nas_get_bridge_node(if_index));
    }
    return NULL;
}
nas_bridge_t* nas_create_insert_bridge_node(hal_ifindex_t index, const char *name, bool &create)
{
    nas_bridge_t *p_bridge_node;

    p_bridge_node = nas_get_bridge_node(index);
    create = false;

    if (p_bridge_node == NULL) {
        nas_bridge_t node;
        memset(&node, 0, sizeof(node));
        EV_LOGGING(INTERFACE, INFO, "NAS-Br",
                    "Bridge intf %d created", index);
        node.ifindex = index;
        safestrncpy(node.name, name, sizeof(node.name));

        bridge_list[index] = node;
        nas_bridge_t *p_node = &bridge_list.at(index);
        std_dll_init (&p_node->tagged_list.port_list);
        std_dll_init (&p_node->untagged_list.port_list);
        std_dll_init (&p_node->untagged_lag.port_list);
        create = true;
        nas_os_set_bridge_default_mac_ageing(index);
        return p_node;
    } else {
        EV_LOGGING(INTERFACE, INFO, "NAS-Br",
                    "Bridge intf index %d already exists", index);
    }
    return p_bridge_node;
}

t_std_error nas_cleanup_bridge(nas_bridge_t *p_bridge_node)
{
    t_std_error rc = STD_ERR_OK;
    //If vlan is set for this bridge, delete VLAN from NPU
    if((p_bridge_node->vlan_id != 0) && (p_bridge_node->vlan_id != SYSTEM_DEFAULT_VLAN)) {
        /* @TODO - NPU ID for bridge structure */
        if(nas_vlan_delete(0, p_bridge_node->vlan_id) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Br",
                       "Error deleting vlan %d for bridge %d",
                        p_bridge_node->vlan_id, p_bridge_node->ifindex);
            rc = (STD_ERR(INTERFACE,FAIL, 0));
        }
    }
    nas_delete_port_list(&(p_bridge_node->untagged_list));
    nas_delete_port_list(&(p_bridge_node->tagged_list));
    nas_delete_port_list(&(p_bridge_node->untagged_lag));
    /* Deregister the VLAN intf from ifCntrl - vlanId in open source gets created
     * only after an interface is added in the bridge */
    if(p_bridge_node->vlan_id !=0 ) {
        if(nas_publish_vlan_object(p_bridge_node, cps_api_oper_DELETE)!= cps_api_ret_code_OK)
        {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                    "Failure publishing VLAN delete event");
        }
        if(nas_register_vlan_intf(p_bridge_node, HAL_INTF_OP_DEREG) != STD_ERR_OK) {
            rc = (STD_ERR(INTERFACE, FAIL, 0));
        }
        nas_del_vlan_to_bridge_map(p_bridge_node->vlan_id, p_bridge_node->ifindex);
    }
    /* Delete the bridge */
    bridge_list.erase(p_bridge_node->ifindex);
    return rc;
}
t_std_error nas_delete_bridge(hal_ifindex_t index)
{
    t_std_error rc = STD_ERR_OK;

    EV_LOGGING(INTERFACE, INFO, "NAS-Br",
                "Bridge intf %d for deletion", index);

    nas_bridge_lock();
    do {
        nas_bridge_t *p_bridge_node = nas_get_bridge_node(index);
        if (p_bridge_node==NULL) {
            EV_LOGGING(INTERFACE, INFO, "NAS-Br",
                       "Error finding bridge %d for deletion", index);
            rc = (STD_ERR(INTERFACE, FAIL, 0));
            break;
        }

        if(nas_cleanup_bridge(p_bridge_node) != STD_ERR_OK) {
            rc = (STD_ERR(INTERFACE, FAIL, 0));
        }

    }while (0);

    nas_bridge_unlock();
    return rc;
}
