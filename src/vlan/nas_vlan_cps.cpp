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
 * nas_vlan_cps.c
 */

#include "dell-base-if-vlan.h"
#include "dell-base-if.h"
#include "dell-interface.h"
#include "nas_os_interface.h"
#include "interface_obj.h"
#include "nas_os_vlan.h"
#include "event_log.h"
#include "event_log_types.h"
#include "nas_int_vlan.h"
#include "nas_int_bridge.h"
#include "nas_int_utils.h"
#include "cps_api_object_key.h"
#include "cps_api_operation.h"
#include "cps_class_map.h"
#include "std_mac_utils.h"
#include "cps_api_events.h"
#include "nas_vlan_lag.h"
#include "dell-base-if-lag.h"
#include "nas_ndi_vlan.h"
#include "nas_ndi_port.h"
#include "std_utils.h"
#include "hal_interface_common.h"
#include "nas_int_com_utils.h"
#include "nas_if_utils.h"

#include <stdbool.h>
#include <stdio.h>
#include <unordered_set>

const static int MAX_CPS_MSG_BUFF=10000;

static  t_std_error nas_process_cps_ports(nas_bridge_t *p_bridge, nas_port_mode_t port_mode,
                                  nas_port_list_t &port_list, vlan_roll_bk_t *p_roll_bk);
static t_std_error nas_cps_del_port_from_vlan(nas_bridge_t *p_bridge, nas_list_node_t *p_link_node, nas_port_mode_t port_mode);
t_std_error nas_cps_cleanup_vlan_lists(hal_vlan_id_t vlan_id, nas_list_t *p_link_node_list);
static t_std_error nas_cps_add_port_to_vlan(nas_bridge_t *p_bridge, hal_ifindex_t port_idx, nas_port_mode_t port_mode);


static inline cps_api_return_code_t nas_vlan_get_admin_status(cps_api_object_t obj, nas_bridge_t *p_bridge)
{
     cps_api_object_attr_add_u32(obj, IF_INTERFACES_INTERFACE_ENABLED,
                 (p_bridge->admin_status == IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS_UP) ? true: false);
     return cps_api_ret_code_OK;
}
static inline cps_api_return_code_t nas_vlan_get_learning_mode(cps_api_object_t obj, nas_bridge_t *p_bridge)
{
     cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_LEARNING_MODE,
                                        p_bridge->learning_disable);
     return cps_api_ret_code_OK;
}
static inline cps_api_return_code_t nas_vlan_get_mac(cps_api_object_t obj, nas_bridge_t *p_bridge)
{
     cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS,
                                        p_bridge->mac_addr, sizeof(p_bridge->mac_addr));
     return cps_api_ret_code_OK;
}
static inline cps_api_return_code_t nas_vlan_get_mtu(cps_api_object_t obj, nas_bridge_t *p_bridge)
{
    if(p_bridge->mtu){
         cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_MTU,p_bridge->mtu);
         return cps_api_ret_code_OK;
    }
    return (nas_os_get_interface_mtu(p_bridge->name, obj));

}

static bool nas_vlan_process_port_association(hal_ifindex_t ifindex, npu_id_t npu, port_t port,bool add){

    auto  m_list = nas_intf_get_master(ifindex);
    for(auto it : m_list){
        if(it.type == nas_int_type_LAG){
            continue;
        }else if(it.type == nas_int_type_VLAN){

            nas_bridge_lock();
            ndi_port_t ndi_port;
            ndi_port.npu_id = npu;
            ndi_port.npu_port = port;
            nas_bridge_t * br_m = nas_get_bridge_node(it.m_if_idx);

            if(br_m == NULL){
                /*  TODO It means bridge is deleted but exists in the master list. It should not be the case
                 *  if lock is used properly.
                 * */
                EV_LOGGING(INTERFACE,ERR,"NAS-VLAN-MAP","No bridge for interface ifindex %d",ifindex);
                nas_bridge_unlock();
                return false;
            }

            if((nas_add_or_del_port_to_vlan(npu,br_m->vlan_id,&ndi_port, it.mode,add)) != STD_ERR_OK){
                EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-MAP","Error adding port <%d %d> to NPU",
                        ndi_port.npu_id, ndi_port.npu_port);
                nas_bridge_unlock();
                return false;
            }
            nas_bridge_unlock();
        }
    }
    return true;
}


static bool nas_vlan_if_set_handler(cps_api_object_t obj, void *context)
{
    EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN-MAP","Got Interface event");
    nas_int_port_mapping_t port_mapping;
    if(!nas_get_phy_port_mapping_change( obj, &port_mapping)){
        EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN-MAP","Interface event is not an "
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
        EV_LOGGING(INTERFACE,DEBUG, "NAS-VLAN-MAP", "Interface object does not have if-index/npu/port");
        return true;
    }

    hal_ifindex_t ifidx = cps_api_object_attr_data_u32(if_index_attr);
    npu_id_t npu = cps_api_object_attr_data_u32(npu_attr);
    npu_port_t port = cps_api_object_attr_data_u32(port_attr);

    bool add = (port_mapping == nas_int_phy_port_MAPPED) ? true : false;

    return nas_vlan_process_port_association(ifidx,npu,port,add);

}


static cps_api_return_code_t nas_vlan_set_admin_status(cps_api_object_t obj, nas_bridge_t *p_bridge)
{

    cps_api_object_attr_t attr = cps_api_object_attr_get(obj, IF_INTERFACES_INTERFACE_ENABLED);

    if (attr == NULL){ return cps_api_ret_code_ERR; }

    cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, p_bridge->ifindex);

    if(nas_os_interface_set_attribute(obj,IF_INTERFACES_INTERFACE_ENABLED) != STD_ERR_OK)
    {
        EV_LOGGING(INTERFACE, ERR ,"NAS-CPS-VLAN","Failure setting Vlan admin status");
        return cps_api_ret_code_ERR;
    }

    p_bridge->admin_status = ((bool)cps_api_object_attr_data_u32(attr) == true)?
        IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS_UP : IF_INTERFACES_STATE_INTERFACE_ADMIN_STATUS_DOWN;

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t nas_cps_set_vlan_learning_mode(cps_api_object_t obj, nas_bridge_t *p_bridge)
{
    cps_api_object_attr_t attr = cps_api_object_attr_get(obj, DELL_IF_IF_INTERFACES_INTERFACE_LEARNING_MODE);

    if (attr == nullptr) { return cps_api_ret_code_ERR; }

    bool learning_disable = cps_api_object_attr_data_u32(attr) ? false :true ;

    if(ndi_set_vlan_learning(0, p_bridge->vlan_id, learning_disable) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR ,"NAS-Vlan", "Error setting learning disable state for VLAN %d",
               p_bridge->ifindex);
        return cps_api_ret_code_ERR;
    }

    p_bridge->learning_disable = learning_disable;
    return cps_api_ret_code_OK;
}

cps_api_return_code_t nas_cps_set_vlan_mac(cps_api_object_t obj, nas_bridge_t *p_bridge)
{
    cps_api_object_attr_t _mac_attr = cps_api_object_attr_get(obj, DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS);

    if (_mac_attr == nullptr) { return cps_api_ret_code_ERR; }

    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, p_bridge->ifindex);

    if(nas_os_interface_set_attribute(obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR ,"NAS-Vlan", "Failure setting bridge %d MAC in OS",
               p_bridge->ifindex);
        return cps_api_ret_code_ERR;
    }
    char * mac_addr_str = (char *) cps_api_object_attr_data_bin(_mac_attr);
    safestrncpy(p_bridge->mac_addr, (const char *)mac_addr_str, sizeof(p_bridge->mac_addr));
    EV_LOGGING(INTERFACE, INFO ,"NAS-Vlan", "Rcvd MAC  %s set for bridge %d\n",
           p_bridge->mac_addr, p_bridge->ifindex);

    if (dn_hal_update_intf_mac(p_bridge->ifindex, (const char *)mac_addr_str) != STD_ERR_OK)  {
        EV_LOGGING(INTERFACE, ERR ,"NAS-Vlan", "Failure saving bridge %d MAC in intf block",
                p_bridge->ifindex);
        return cps_api_ret_code_ERR;
    }
    return cps_api_ret_code_OK;
}

bool nas_set_vlan_member_port_mtu(hal_ifindex_t ifindex, uint32_t mtu, hal_vlan_id_t vlan_id){
    cps_api_object_guard og(cps_api_object_create());
    if(og.get() == nullptr){
        EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN","No memory to create new object");
        return false;
    }

    cps_api_object_attr_add_u32(og.get(), DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, ifindex);
    cps_api_object_attr_add_u32(og.get(), DELL_IF_IF_INTERFACES_INTERFACE_MTU, mtu);
    cps_api_object_attr_add_u16(og.get(), BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, vlan_id);
    if(nas_os_vlan_set_member_port_mtu(og.get()) == STD_ERR_OK) return true;

    return false;
}

cps_api_return_code_t nas_cps_set_vlan_mtu(cps_api_object_t obj, nas_bridge_t *p_bridge)
{
    cps_api_object_attr_t _mtu_attr = cps_api_object_attr_get(obj, DELL_IF_IF_INTERFACES_INTERFACE_MTU);

    if (_mtu_attr == nullptr) { return cps_api_ret_code_ERR; }

    uint32_t mtu = cps_api_object_attr_data_u32(_mtu_attr);

    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, p_bridge->ifindex);

    nas_list_node_t *p_link_node = NULL;
       p_link_node = nas_get_first_link_node(&(p_bridge->tagged_list.port_list));
       while (p_link_node != NULL) {
           if(!nas_set_vlan_member_port_mtu(p_link_node->ifindex,mtu,p_bridge->vlan_id)){
               EV_LOGGING(INTERFACE,ERR,"NAS-VLAN","Failed to set mtu for member ports of vlan %d",
                       p_bridge->ifindex);
               return cps_api_ret_code_ERR;
           }
           p_link_node = nas_get_next_link_node(&p_bridge->tagged_list.port_list, p_link_node);
       }

    nas_vlan_set_tagged_lag_mtu(p_bridge->vlan_id,mtu);
    if(nas_os_interface_set_attribute(obj,DELL_IF_IF_INTERFACES_INTERFACE_MTU) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR ,"NAS-Vlan", "Failure setting  MTU for VLAN %d  in OS",
               p_bridge->ifindex);
        return cps_api_ret_code_ERR;
    }
    p_bridge->mtu = mtu;

    return cps_api_ret_code_OK;
}

static auto set_vlan_attr = new std::unordered_map<cps_api_attr_id_t,
    cps_api_return_code_t (*)(cps_api_object_t, nas_bridge_t *)>
{
    { IF_INTERFACES_INTERFACE_ENABLED, nas_vlan_set_admin_status },
    { DELL_IF_IF_INTERFACES_INTERFACE_LEARNING_MODE, nas_cps_set_vlan_learning_mode },
    { DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS, nas_cps_set_vlan_mac},
    { DELL_IF_IF_INTERFACES_INTERFACE_MTU, nas_cps_set_vlan_mtu},
};

static auto get_vlan_attr = new  std::unordered_map<cps_api_attr_id_t,
    cps_api_return_code_t (*)(cps_api_object_t, nas_bridge_t *)>
{
    { IF_INTERFACES_INTERFACE_ENABLED, nas_vlan_get_admin_status },
    { DELL_IF_IF_INTERFACES_INTERFACE_LEARNING_MODE, nas_vlan_get_learning_mode },
    { DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS, nas_vlan_get_mac},
    { DELL_IF_IF_INTERFACES_INTERFACE_MTU, nas_vlan_get_mtu},
};

void handle_vlan_create_roll_bk(nas_bridge_t *p_bridge_node) {


    cps_api_object_t obj = cps_api_object_create();
    if (obj == NULL) {
        EV_LOGGING(INTERFACE, ERR , "NAS-Vlan",
               "Roll_bk: handle_vlan_create_roll_bk cps obj create failed for br %s ", p_bridge_node->name);
        return;
    }
    cps_api_object_guard g(obj);

    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
            "Roll_bk :Deleting Bridge %s", p_bridge_node->name);

    cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, p_bridge_node->ifindex);
    cps_api_set_key_data(obj,IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
                               p_bridge_node->name, strlen(p_bridge_node->name)+1);

    if (nas_os_del_vlan(obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR , "NAS-Vlan",
               "Roll_bk: Failure deleting vlan/Br %s from kernel", p_bridge_node->name);
    }
    /* Walk through and delete each tagged vlan interface in kernel */
    nas_cps_cleanup_vlan_lists(p_bridge_node->vlan_id, &p_bridge_node->tagged_list);
    //Delete Vlan in NPU
    if (nas_cleanup_bridge(p_bridge_node) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan", "Roll_bk: :Failure cleaning vlan data");
    }
}

/* Delete has come from cps : It should come with a lock held */
void handle_roll_bak(nas_bridge_t *p_bridge, vlan_roll_bk_t &p_roll_bk, cps_api_object_t obj)
{
    cps_api_object_it_t it;

    cps_api_object_it_begin(obj,&it);
    hal_ifindex_t del_ifindex = 0;

    for ( ; cps_api_object_it_valid(&it) ; cps_api_object_it_next(&it) ) {
        int id = (int) cps_api_object_attr_id(it.attr);
        auto func = set_vlan_attr->find(id);
        if (func == set_vlan_attr->end()) continue;
        cps_api_return_code_t ret = func->second(obj,p_bridge);
        if (ret != cps_api_ret_code_OK) {
             EV_LOGGING(INTERFACE, ERR, "NAS-Vlan","Rollback failure for the attribute %d vlan index %d", id,
                     p_bridge->ifindex);
        }
    }
   /* Now roll back any ports or bonds that were added or deleted */
   for (auto it=p_roll_bk.port_del_list.begin(); it!=p_roll_bk.port_del_list.end(); ++it) {
       /* Add these deleted ports back */
       if (nas_cps_add_port_to_vlan(p_bridge, it->first, it->second) != STD_ERR_OK) {
           EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                     "Roll_bk: Error adding port %d to bridge %d in OS", *it, p_bridge->ifindex);
       } else {
           EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                     "Roll_bk: Success adding port %d to bridge %d in OS", *it, p_bridge->ifindex);
       }

   }

   for (auto it=p_roll_bk.port_add_list.begin(); it!=p_roll_bk.port_add_list.end(); ++it) {
       auto mode  = it->second;
       nas_list_t *p_list = NULL;
       nas_list_node_t *p_link_node = NULL;
       nas_list_node_t *p_temp_node = NULL;
       if(mode == NAS_PORT_TAGGED) {
           p_list = &(p_bridge->tagged_list);
       } else {
           p_list = &(p_bridge->untagged_list);
       }

       p_link_node = nas_get_first_link_node(&(p_list->port_list));
       while (p_link_node != NULL) {
           p_temp_node = p_link_node;
           if (p_link_node->ifindex  == it->first) {
               /* del_port from vlan */
               del_ifindex = p_link_node->ifindex;
               if (nas_cps_del_port_from_vlan(p_bridge, p_temp_node, mode) != STD_ERR_OK) {
                   EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                       "Roll_bk: Error deleting port %d from Bridge %d ", p_link_node->ifindex, p_bridge->ifindex);
               } else {
                   EV_LOGGING(INTERFACE, DEBUG, "NAS-Vlan",
                       "Roll_bk: Success deleting port %d from Bridge %d ", del_ifindex, p_bridge->ifindex);

               }
               break;
           }
           p_link_node = nas_get_next_link_node(&p_list->port_list, p_temp_node);
       }
   }
   for (auto it=p_roll_bk.lag_del_list.begin(); it!=p_roll_bk.lag_del_list.end(); ++it) {
       /* add deleted lag back to vlan */
       if (nas_handle_lag_add_to_vlan(p_bridge, it->first,
                                           it->second, true, nullptr) != STD_ERR_OK) {
           EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                       "Roll_bk: Error adding lag  %d to Bridge %d ", it->first, p_bridge->ifindex);
       } else {
           EV_LOGGING(INTERFACE, DEBUG, "NAS-Vlan",
                       "Roll_bk: Success adding lag  %d to Bridge %d ", it->first, p_bridge->ifindex);

       }
   }

   for (auto it=p_roll_bk.lag_add_list.begin(); it!=p_roll_bk.lag_add_list.end(); ++it) {
       /* del added lag from vlan */
       if (nas_handle_lag_del_from_vlan(p_bridge, it->first,
                                           it->second, true, nullptr) != STD_ERR_OK) {
           EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                       "Roll_bk: Error deleting lag %d from Bridge %d", it->first, p_bridge->ifindex);

       } else {
           EV_LOGGING(INTERFACE, DEBUG, "NAS-Vlan",
                       "Roll_bk: Success deleting lag %d from Bridge %d", it->first, p_bridge->ifindex);
       }
   }
}


static cps_api_return_code_t nas_cps_update_vlan(nas_bridge_t *p_bridge, cps_api_object_t obj)
{
    if (p_bridge == NULL) return cps_api_ret_code_ERR;

    cps_api_object_it_t it;
    nas_port_list_t tag_port_list;
    nas_port_list_t untag_port_list;
    bool roll_back = false;
    bool create = false;
    bool untag_list_attr = false;
    bool tag_list_attr = false;
    vlan_roll_bk_t p_roll_bk;


    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));
    if (op == cps_api_oper_CREATE) create =true;
    cps_api_object_t obj_roll_bk = cps_api_object_create();
    if (obj_roll_bk == NULL) {
        return cps_api_ret_code_ERR;
    }
    cps_api_object_guard obj_guard (obj_roll_bk);

    cps_api_set_key_data(obj_roll_bk, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,cps_api_object_ATTR_T_U32,
            &p_bridge->ifindex,sizeof(hal_ifindex_t));

    cps_api_object_it_begin(obj,&it);

    for ( ; cps_api_object_it_valid(&it) ; cps_api_object_it_next(&it) ) {

        int id = (int) cps_api_object_attr_id(it.attr);
        switch (id) {
        case DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS:
            untag_list_attr = true;
            //for delete all, attribute will be sent but with zero length
            if (cps_api_object_attr_len(it.attr) != 0) {
                hal_ifindex_t port_if_index;
                if (nas_int_name_to_if_index(&port_if_index, (const char *)cps_api_object_attr_data_bin(it.attr))
                                     == STD_ERR_OK) {
                    untag_port_list.insert(port_if_index);
                }
            }
            break;

        case DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS:
            tag_list_attr = true;
            if (cps_api_object_attr_len(it.attr) != 0) {
                hal_ifindex_t port_if_index;
                if (nas_int_name_to_if_index(&port_if_index, (const char *)cps_api_object_attr_data_bin(it.attr))
                                     == STD_ERR_OK) {
                    tag_port_list.insert(port_if_index);
                }
            }
            break;

        case DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS:
        case DELL_IF_IF_INTERFACES_INTERFACE_LEARNING_MODE:
        case DELL_IF_IF_INTERFACES_INTERFACE_MTU:
        case IF_INTERFACES_INTERFACE_ENABLED:
            {
                auto get_func = get_vlan_attr->find(id);
                auto set_func = set_vlan_attr->find(id);
                // save previous value in the rollback obj
                get_func = get_vlan_attr->find(id);
                if (get_func == get_vlan_attr->end()) break;
                (void)get_func->second(obj_roll_bk,p_bridge);
                // Now set the vlan attribute
                if (set_func == set_vlan_attr->end()) break;
                if (set_func->second(obj,p_bridge) != cps_api_ret_code_OK) {
                    roll_back = true;
                }
            }
            break;
        default:
            EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                   "Received attrib %d", id);
            break;
        }
    }

    if (tag_list_attr == true && !roll_back) {
        EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                "Received %d tagged ports for VLAN %d index %d", tag_port_list.size(),
                p_bridge->vlan_id, p_bridge->ifindex);

        if (nas_process_cps_ports(p_bridge, NAS_PORT_TAGGED, tag_port_list , &p_roll_bk) != STD_ERR_OK) {
             roll_back = true;
        }
    }
    if (untag_list_attr == true && !roll_back) {
        EV_LOGGING(INTERFACE, INFO , "NAS-Vlan",
               "Received %d untagged ports for VLAN %d index %d", untag_port_list.size(),
               p_bridge->vlan_id, p_bridge->ifindex);

        if (nas_process_cps_ports(p_bridge, NAS_PORT_UNTAGGED, untag_port_list, &p_roll_bk) != STD_ERR_OK){
            roll_back = true;
        }
    }

    if (roll_back) {
        EV_LOGGING(INTERFACE, INFO , "NAS-Vlan",
               "Roll back set for vlanid %d bridge  %d", p_bridge->vlan_id, p_bridge->ifindex);
        if (!create) {
            handle_roll_bak(p_bridge, p_roll_bk, obj_roll_bk);
        }
        return cps_api_ret_code_ERR;
    }
    return cps_api_ret_code_OK;

}

t_std_error nas_del_tagged_lag_intf(hal_ifindex_t lag_index, hal_vlan_id_t vlan_id) {

    char buff[MAX_CPS_MSG_BUFF];
    cps_api_object_t vlan_obj = cps_api_object_init(buff, sizeof(buff));

    cps_api_object_attr_add_u32(vlan_obj,DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS, lag_index);
    cps_api_object_attr_add_u32(vlan_obj,BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, vlan_id);

    if (nas_os_del_vlan_interface(vlan_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                     "Failure deleting vlan tagged bond  %d from OS, vid %d", lag_index, vlan_id);
        return (STD_ERR(INTERFACE,FAIL, 0));
    }
    return STD_ERR_OK;

}

bool nas_ck_port_exist(hal_vlan_id_t vlan_id, hal_ifindex_t port_index) {
    cps_api_object_t vlan_obj = cps_api_object_create();
    if (vlan_obj == NULL) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan", "nas_ck_port_exist: Out of mem");
        return false;
    }
    cps_api_object_guard obj_guard(vlan_obj);
    cps_api_object_attr_add_u32(vlan_obj,DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS, port_index);
    cps_api_object_attr_add_u32(vlan_obj,BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, vlan_id);
    return nas_os_tag_port_exist(vlan_obj);
}


static cps_api_return_code_t nas_cps_create_vlan(cps_api_object_t obj)
{
    hal_ifindex_t br_index = 0;
    char name[HAL_IF_NAME_SZ] = "\0";
    cps_api_return_code_t rc = cps_api_ret_code_ERR;
    bool create = false;

    cps_api_object_attr_t vlan_id_attr = cps_api_object_attr_get(obj, BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID);

    if (vlan_id_attr == NULL) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                       "Missing VLAN ID during create vlan");
        return rc;
    }
    cps_api_object_attr_t vlan_name_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);
    cps_api_object_attr_t mac_attr = cps_api_object_attr_get(obj,DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS);

    nas_bridge_lock();
    /* Construct Bridge name for this vlan -
     * if vlan_id is 100 then bridge name is "br100"
     */
    hal_vlan_id_t vlan_id = cps_api_object_attr_data_u32(vlan_id_attr);

    cps_api_object_attr_t attr = cps_api_object_attr_get(obj, DELL_IF_IF_INTERFACES_INTERFACE_VLAN_TYPE);
    BASE_IF_VLAN_TYPE_t vlan_type = BASE_IF_VLAN_TYPE_DATA;

    if (attr != NULL) {
        vlan_type = (BASE_IF_VLAN_TYPE_t) cps_api_object_attr_data_u32(attr);
    }

    if( (vlan_id < MIN_VLAN_ID) || (vlan_id > MAX_VLAN_ID) || (nas_vlan_id_in_use(vlan_id))) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                           "Invalid or Used VLAN ID during create vlan %d", vlan_id);
        nas_bridge_unlock();
        return rc;
    }
    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan", "Create VLAN %d using CPS", vlan_id);

    memset(name,0,sizeof(name));

    if(vlan_name_attr != NULL) {
        strncpy(name,(char *)cps_api_object_attr_data_bin(vlan_name_attr),sizeof(name)-1);
    } else {
        snprintf(name, sizeof(name), "br%d", vlan_id);
        cps_api_set_key_data(obj, IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN, name, strlen(name)+1);
    }

    //Configure bridge in Kernel
    if (nas_os_add_vlan(obj, &br_index) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
               "Failure creating VLAN id %d in Kernel", vlan_id);
        nas_bridge_unlock();
        return rc;
    }

    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
           "Kernel programmed, creating bridge with index %d", br_index);

    do {
        //Create bridge data structure using kernel index ..
        nas_bridge_t *p_bridge_node = nas_create_insert_bridge_node(br_index, name, create);
        if (p_bridge_node != NULL &&
                     (nas_handle_vid_to_br(vlan_id, p_bridge_node->ifindex))) {
            p_bridge_node->vlan_id = vlan_id;
            p_bridge_node->int_sub_type = (unsigned int) vlan_type;
            if(mac_attr != NULL) {
                strncpy((char *)p_bridge_node->mac_addr, (char *)cps_api_object_attr_data_bin(mac_attr),
                        cps_api_object_attr_len(mac_attr));
            }
            //Create Vlan in NPU, @TODO for npu_id
            if(vlan_id != SYSTEM_DEFAULT_VLAN) {
                if (nas_vlan_create(0, vlan_id) != STD_ERR_OK) {
                    EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                           "Failure creating VLAN id %d in NPU ", vlan_id);
                    if (nas_os_del_vlan(obj) != STD_ERR_OK) {
                        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                        "Roll_bk: Failure deleting vlan %s in kernel\n", name);
                    }
                    break;
                }
            }
        }
        else {
            EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                   "Kernel programmed bridge %d vlan %d already exists in NAS",
                   p_bridge_node->ifindex, p_bridge_node->vlan_id);
            break;
        }
        std_mutex_simple_lock_guard lock_t(vlan_lag_mutex_lock());

        if(nas_register_vlan_intf(p_bridge_node, HAL_INTF_OP_REG) != STD_ERR_OK) {
            break;
        }
        if (nas_intf_handle_intf_mode_change(br_index, BASE_IF_MODE_MODE_NONE) == false) {
             EV_LOGGING(INTERFACE, DEBUG, "NAS-CPS-LAG",
                    "Update to NAS-L3 about interface mode change failed(%d)", br_index);
        }
        if (nas_cps_update_vlan(p_bridge_node, obj) != cps_api_ret_code_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                   "VLAN Attribute configuration failed for create bridge interface %d vlan %d",
                   p_bridge_node->ifindex, p_bridge_node->vlan_id);
            handle_vlan_create_roll_bk(p_bridge_node);

            break;
        }

        if(nas_publish_vlan_object(p_bridge_node, cps_api_oper_CREATE)!= cps_api_ret_code_OK)
        {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                    "Failure publishing VLAN create event");
            break;
        }
        //Add the kernel index in obj for application to process
        cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, br_index);
        rc = cps_api_ret_code_OK;

    }while(0);

    nas_bridge_unlock();
    return rc;
}


static cps_api_return_code_t nas_cps_delete_vlan(cps_api_object_t obj)
{
    cps_api_return_code_t rc = cps_api_ret_code_OK;
    char buff[MAX_CPS_MSG_BUFF];


    EV_LOGGING(INTERFACE, INFO, "NAS-VLAN-DELETE", "CPS Delete VLAN ");
    cps_api_object_attr_t vlan_name_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);
    cps_api_object_attr_t vlan_if_attr = cps_api_object_attr_get(obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);

    if ((vlan_name_attr == nullptr)  && (vlan_if_attr == nullptr)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-DELETE", "Missing Vlan interface name or if_index");
        return cps_api_ret_code_ERR;
    }

    nas_bridge_lock();
    nas_bridge_t *p_bridge_node = NULL;

    if (vlan_if_attr != nullptr) {
        hal_ifindex_t br_index = (hal_ifindex_t) cps_api_object_attr_data_u32(vlan_if_attr);
        p_bridge_node = nas_get_bridge_node(br_index);
        EV_LOGGING(INTERFACE, INFO, "NAS-VLAN-DELETE", "CPS Delete VLAN for vlan index %d", br_index);
    } else {
        p_bridge_node  = nas_get_bridge_node_from_name((const char *)cps_api_object_attr_data_bin(vlan_name_attr));
        EV_LOGGING(INTERFACE, INFO, "NAS-VLAN-DELETE", "CPS Delete VLAN for vlan interface %s",
                (char *)cps_api_object_attr_data_bin(vlan_name_attr));
    }

    //Delete bridge from kernel.

    if (p_bridge_node == NULL) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-DELETE", "Error finding bridge node");
        nas_bridge_unlock();
        return cps_api_ret_code_ERR;
    }

    cps_api_object_t idx_obj = cps_api_object_init(buff, sizeof(buff));

    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
           "Deleting Bridge %s", p_bridge_node->name);

    cps_api_object_attr_add_u32(idx_obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, p_bridge_node->ifindex);
    cps_api_set_key_data(idx_obj,IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
            p_bridge_node->name, strlen(p_bridge_node->name)+1);

    if (nas_intf_handle_intf_mode_change(p_bridge_node->ifindex, BASE_IF_MODE_MODE_L2) == false) {
        EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN", "Update to NAS-L3 about interface mode change failed(%d)",
                p_bridge_node->ifindex);
    }
    if (nas_os_del_vlan(idx_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR , "NAS-Vlan",
               "Failure deleting vlan %s from kernel", p_bridge_node->name);
    }
    std_mutex_simple_lock_guard lock_t(vlan_lag_mutex_lock());
    /* Walk through and delete each tagged vlan interface in kernel */
    nas_cps_cleanup_vlan_lists(p_bridge_node->vlan_id, &p_bridge_node->tagged_list);
    //Delete Vlan in NPU
    if (nas_cleanup_bridge(p_bridge_node) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, INFO, "NAS-Vlan", "Failure cleaning vlan data");
        rc = cps_api_ret_code_ERR;
    }
    nas_bridge_unlock();
    return rc;
}


static cps_api_return_code_t nas_cps_set_vlan(cps_api_object_t obj)
{
    hal_ifindex_t br_index = 0;
    nas_bridge_t *p_bridge;

    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan", "CPS Set VLAN");

    cps_api_object_attr_t vlan_name_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);
    cps_api_object_attr_t vlan_if_attr = cps_api_object_attr_get(obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);

    if ((vlan_name_attr == nullptr)  && (vlan_if_attr == nullptr)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan-SET", "Missing Vlan interface name or if_index");
        return cps_api_ret_code_ERR;
    }

    nas_bridge_lock();

    if (vlan_if_attr != nullptr) {
        br_index = (hal_ifindex_t) cps_api_object_attr_data_u32(vlan_if_attr);
        p_bridge = nas_get_bridge_node(br_index);
    } else {
        p_bridge = nas_get_bridge_node_from_name((const char *)cps_api_object_attr_data_bin(vlan_name_attr));
    }

    if (p_bridge == NULL) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
               "Error finding ifindex %d for Bridge in CPS Set", br_index);
        nas_bridge_unlock();
        return cps_api_ret_code_ERR;
    }

    cps_api_object_attr_t attr = cps_api_object_attr_get(obj, DELL_IF_IF_INTERFACES_INTERFACE_VLAN_TYPE);

    if (attr != NULL) {
        BASE_IF_VLAN_TYPE_t vlan_type = (BASE_IF_VLAN_TYPE_t) cps_api_object_attr_data_u32(attr);
        p_bridge->int_sub_type = (unsigned int) vlan_type;
    }

    std_mutex_simple_lock_guard lock_t(vlan_lag_mutex_lock());
    if (nas_cps_update_vlan(p_bridge, obj) != cps_api_ret_code_OK) {
        EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
               "VLAN Attribute configuration failed for bridge interface %d vlan %d",
               p_bridge->ifindex, p_bridge->vlan_id);
        nas_bridge_unlock();
        return cps_api_ret_code_ERR;
    }
    nas_bridge_unlock();
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t nas_process_cps_vlan_set(void *context, cps_api_transaction_params_t *param, size_t ix)
{
    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    EV_LOGGING(INTERFACE, DEBUG, "NAS-Vlan",
           "nas_process_cps_vlan_set");

    cps_api_object_t cloned = cps_api_object_list_create_obj_and_append(param->prev);
    if (cloned == NULL) return cps_api_ret_code_ERR;

    cps_api_object_clone(cloned,obj);

    if (op == cps_api_oper_CREATE)      return(nas_cps_create_vlan(obj));
    else if (op == cps_api_oper_SET)    return(nas_cps_set_vlan(obj));
    else if (op == cps_api_oper_DELETE) return(nas_cps_delete_vlan(obj));

    return cps_api_ret_code_ERR;
}

static cps_api_return_code_t nas_process_cps_vlan_get(void * context,
                                                      cps_api_get_params_t *param,
                                                      size_t ix)
{
    size_t iix = 0;
    size_t mx = cps_api_object_list_size(param->list);

    EV_LOGGING(INTERFACE, DEBUG, "NAS-Vlan",
            "nas_process_cps_vlan_get");
    nas_bridge_lock();
    do {
         cps_api_object_t filter = cps_api_object_list_get(param->filters, ix);
         cps_api_object_attr_t name_attr = cps_api_get_key_data(filter,
                                            IF_INTERFACES_INTERFACE_NAME);
         cps_api_object_attr_t vlan_id_attr = cps_api_object_attr_get(filter,
                                    BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID);

         if (name_attr != NULL) {
              const char *if_name = (char *)cps_api_object_attr_data_bin(name_attr);
              if (nas_get_vlan_intf(if_name, param->list) != STD_ERR_OK) {
                  nas_bridge_unlock();
                  return cps_api_ret_code_ERR;
              }
          } else if (vlan_id_attr != NULL) {
              hal_vlan_id_t vid = cps_api_object_attr_data_u32(vlan_id_attr);
              if (nas_get_vlan_intf_from_vid(vid, param->list) != STD_ERR_OK) {
                  nas_bridge_unlock();
                  return cps_api_ret_code_ERR;
              }
          }
          else {
              nas_vlan_get_all_info(param->list);
          }

         ++iix;
    }while (iix < mx);

    nas_bridge_unlock();
    return cps_api_ret_code_OK;
}


t_std_error nas_cps_vlan_init(cps_api_operation_handle_t handle) {

    if (intf_obj_handler_registration(obj_INTF, nas_int_type_VLAN,
                nas_process_cps_vlan_get, nas_process_cps_vlan_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-INIT", "Failed to register VLAN interface CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    cps_api_event_reg_t reg;
    cps_api_key_t key;

    memset(&reg, 0, sizeof(cps_api_event_reg_t));

    if (!cps_api_key_from_attr_with_qual(&key,
            DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
            cps_api_qualifier_OBSERVED)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-INIT", "Cannot create a key for interface event");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    reg.objects = &key;
    reg.number_of_objects = 1;

    if (cps_api_event_thread_reg(&reg, nas_vlan_if_set_handler, NULL)
            != cps_api_ret_code_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-INIT", "Cannot register interface operation event");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return STD_ERR_OK;
}

t_std_error nas_cps_add_port_to_os(hal_ifindex_t br_index, hal_vlan_id_t vlan_id,
                                   nas_port_mode_t port_mode, hal_ifindex_t port_idx,uint32_t mtu)
{
    hal_ifindex_t vlan_index = 0;
    char buff[MAX_CPS_MSG_BUFF];
    cps_api_object_t name_obj = cps_api_object_init(buff, sizeof(buff));

    cps_api_object_attr_add_u32(name_obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, br_index);
    cps_api_object_attr_add_u32(name_obj,BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, vlan_id);
    if(port_mode == NAS_PORT_TAGGED) {
        cps_api_object_attr_add_u32(name_obj,DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS, port_idx);
        if(mtu) cps_api_object_attr_add_u32(name_obj,DELL_IF_IF_INTERFACES_INTERFACE_MTU,mtu);
    }
    else {
        cps_api_object_attr_add_u32(name_obj,DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS, port_idx);
    }

    if (nas_os_add_port_to_vlan(name_obj, &vlan_index) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
               "Error adding port %d to vlan in the Kernel", port_idx);
        return (STD_ERR(INTERFACE,FAIL, 0));
    }

    return STD_ERR_OK;
}

static t_std_error nas_cps_add_port_to_vlan(nas_bridge_t *p_bridge, hal_ifindex_t port_idx, nas_port_mode_t port_mode)
{
    nas_list_node_t *p_link_node = NULL;
    hal_vlan_id_t vlan_id = 0;
    t_std_error rc = STD_ERR_OK;

    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
           "nas_process_cps_port_add_vlan_mem");

    /* During add ports etc, application isn't passing the vlan id but just the index */

    vlan_id = p_bridge->vlan_id;
    if_master_info_t master_info = { nas_int_type_VLAN, port_mode, p_bridge->ifindex};
    BASE_IF_MODE_t intf_mode = nas_intf_get_mode(port_idx);

    //add to kernel
    if(nas_cps_add_port_to_os(p_bridge->ifindex, p_bridge->vlan_id, port_mode,
                           port_idx, p_bridge->mtu) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
               "Error inserting index %d in Vlan %d to OS", port_idx, p_bridge->vlan_id);
        rc = (STD_ERR(INTERFACE,FAIL, 0));
    }

    if(!nas_intf_add_master(port_idx, master_info)){
        EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN","Failed to add master for vlan memeber port");
    } else {
        BASE_IF_MODE_t new_mode = nas_intf_get_mode(port_idx);

        if (new_mode != intf_mode) {
            if (nas_intf_handle_intf_mode_change(port_idx, new_mode) == false) {
                EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN", "Update to NAS-L3 about interface mode change failed(%d)", port_idx);
            }
        }
    }

    bool create_flag = false;
    ///Kernel add successful, insert in the local data structure
    p_link_node = nas_create_vlan_port_node(p_bridge, port_idx,
                                            port_mode, &create_flag);

    if ((p_link_node != NULL) && (!nas_is_non_npu_phy_port(port_idx))) {
        if((rc = nas_add_or_del_port_to_vlan(p_link_node->ndi_port.npu_id, vlan_id,
                                      &(p_link_node->ndi_port), port_mode, true)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                   "Error adding port <%d %d> to NPU",
                   p_link_node->ndi_port.npu_id, p_link_node->ndi_port.npu_port);
            nas_cps_del_port_from_os(vlan_id, port_idx, port_mode);
            return rc;
        }

        //Set the untagged port VID
        if(port_mode == NAS_PORT_UNTAGGED) {
            if((rc = ndi_set_port_vid(p_link_node->ndi_port.npu_id,
                             p_link_node->ndi_port.npu_port, vlan_id)) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                       "Error setting port <%d %d> VID to %d ",
                       p_link_node->ndi_port.npu_id, p_link_node->ndi_port.npu_port,
                       vlan_id);
                nas_cps_del_port_from_os(vlan_id, port_idx, port_mode);
                return rc;

            }
        }
    }
    else {
        if (p_link_node == NULL) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                    "Error inserting index %d in list", port_idx);
            rc = (STD_ERR(INTERFACE,FAIL, 0));
        }
    }

    return rc;
}

/* Delete has come from cps : It should come with a lock held */
t_std_error nas_cps_del_port_from_os(hal_vlan_id_t vlan_id, hal_ifindex_t port_index,
                                     nas_port_mode_t port_mode)
{
    char buff[MAX_CPS_MSG_BUFF];

    cps_api_object_t obj = cps_api_object_init(buff, sizeof(buff));

    cps_api_object_attr_add_u32(obj, BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, vlan_id);

    if (port_mode == NAS_PORT_TAGGED) {
        cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS, port_index);
    }
    else {
        cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS, port_index);
    }

    if (nas_os_del_port_from_vlan(obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
               "Error deleting interface %d from OS", port_index);
    }

    return STD_ERR_OK;
}

static t_std_error nas_cps_del_port_from_vlan(nas_bridge_t *p_bridge, nas_list_node_t *p_link_node, nas_port_mode_t port_mode)
{
    nas_list_t *p_list = NULL;

    if (port_mode == NAS_PORT_TAGGED) {
        p_list = &p_bridge->tagged_list;
    }
    else {
        p_list = &p_bridge->untagged_list;
    }

    if_master_info_t master_info = { nas_int_type_VLAN, port_mode, p_bridge->ifindex};
    BASE_IF_MODE_t intf_mode = nas_intf_get_mode(p_link_node->ifindex);
    if(!nas_intf_del_master(p_link_node->ifindex, master_info)){
        EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN","Failed to del master for vlan memeber port");
    } else {
        BASE_IF_MODE_t new_mode = nas_intf_get_mode(p_link_node->ifindex);
        if (new_mode != intf_mode) {
            if (nas_intf_handle_intf_mode_change(p_link_node->ifindex, new_mode) == false) {
                EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN", "Update to NAS-L3 about interface mode change failed(%d)",
                                                        p_link_node->ifindex);
            }
        }
    }


    if(!nas_intf_cleanup_l2mc_config(p_link_node->ifindex,  p_bridge->vlan_id)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
               "Error cleaning L2MC membership for interface %d", p_link_node->ifindex);
    }
    if (!nas_is_non_npu_phy_port(p_link_node->ifindex)) {
    //delete the port from NPU if it is a NPU port
        if (nas_add_or_del_port_to_vlan(p_link_node->ndi_port.npu_id, p_bridge->vlan_id,
                                    &(p_link_node->ndi_port), port_mode, false) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                  "Error deleting port %d with mode %d from vlan %d", p_link_node->ifindex,
                   port_mode, p_bridge->vlan_id);
            return (STD_ERR(INTERFACE,FAIL, 0));
        }
    }
    //NPU delete done, now delete from Kernel
    if (nas_cps_del_port_from_os(p_bridge->vlan_id, p_link_node->ifindex, port_mode) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
               "Error deleting interface %d from OS", p_link_node->ifindex);
    }

    nas_delete_link_node(&p_list->port_list, p_link_node);
    --p_list->port_count;

    return STD_ERR_OK;
}

t_std_error nas_cps_cleanup_vlan_lists(hal_vlan_id_t vlan_id, nas_list_t *p_link_node_list)
{
    nas_list_node_t *p_link_node = NULL, *temp_node = NULL;
    char buff[MAX_CPS_MSG_BUFF];

    p_link_node = nas_get_first_link_node(&p_link_node_list->port_list);
    if(p_link_node != NULL) {
        EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
               "Found vlan intf %d for deletion from OS", p_link_node->ifindex);

        while(p_link_node != NULL) {
            temp_node = nas_get_next_link_node(&p_link_node_list->port_list, p_link_node);

            cps_api_object_t vlan_obj = cps_api_object_init(buff, sizeof(buff));

            cps_api_object_attr_add_u32(vlan_obj,DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS, p_link_node->ifindex);
            cps_api_object_attr_add_u32(vlan_obj,BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, vlan_id);

            if(nas_os_del_vlan_interface(vlan_obj) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                       "Failure deleting vlan intf %d from OS", p_link_node->ifindex);
            }
            p_link_node = temp_node;
        }
    }
    nas_handle_del_vlan_lag(vlan_id);
    return STD_ERR_OK;
}

static t_std_error nas_process_cps_ports(nas_bridge_t *p_bridge, nas_port_mode_t port_mode,
                                  nas_port_list_t &port_index_list, vlan_roll_bk_t *p_roll_bk )
{
    nas_list_t *p_list = NULL;
    nas_list_node_t *p_link_node = NULL, *p_temp_node = NULL;
    hal_ifindex_t ifindex = 0;
    nas_int_type_t int_type;
    nas_port_list_t publish_list;
    t_std_error rc = STD_ERR_OK;

    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
           "Processing Port list for changes for Vlan %d", p_bridge->vlan_id);

    if(port_mode == NAS_PORT_TAGGED) {
        p_list = &(p_bridge->tagged_list);
    }
    else {
        p_list = &(p_bridge->untagged_list);
    }

    /* First lets check for node deletion scenarios.
     * */
    p_link_node = nas_get_first_link_node(&(p_list->port_list));
    while (p_link_node != NULL) {
        ifindex = p_link_node->ifindex;
        p_temp_node = p_link_node;
        p_link_node = nas_get_next_link_node(&p_list->port_list, p_temp_node);

        if (port_index_list.find(ifindex) == port_index_list.end()){
            EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                   "Port %d does not exist in the SET request, delete it",
                    ifindex);

            publish_list.insert(p_temp_node->ifindex);
            if ((rc = nas_cps_del_port_from_vlan(p_bridge, p_temp_node, port_mode)) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                       "Error deleting port %d from Bridge %d in OS", ifindex, p_bridge->ifindex);
                return rc;
            }
            p_roll_bk->port_del_list[ifindex] = port_mode;


        }
    } //while
    if (!publish_list.empty()) {
        nas_publish_vlan_port_list(p_bridge, publish_list, port_mode, cps_api_oper_DELETE);
        publish_list.clear();
    }

    if ((rc = nas_handle_lag_index_in_cps_set(p_bridge, port_index_list, port_mode, p_roll_bk)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan", "nas_handle_lag_index_in_cps_set delete lag failed for bridge %d ",
           p_bridge->ifindex);
        return rc;
    }

    /*Now lets just consider the add case ..
      check if this port index already exists, if not go and add this port */
    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
           "Now checking if any new ports are added");
    for (auto it = port_index_list.begin() ; it != port_index_list.end() ; ++it) {
        nas_get_int_type(*it, &int_type);

        if(int_type == nas_int_type_LAG) {
            EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                   "Received LAG %d for addition to Vlan %d", *it, p_bridge->vlan_id);

            if ((rc = nas_handle_lag_add_to_vlan(p_bridge, *it,
                                           port_mode, true, p_roll_bk)) != STD_ERR_OK) {
                return rc;
            }
        }
        else {
            p_link_node = nas_get_link_node(&(p_list->port_list), *it);

            if (p_link_node == NULL) {
                EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                       "Received new port %d in bridge %d, VLAN %d", *it, p_bridge->ifindex,
                       p_bridge->vlan_id);

                if ((rc = nas_cps_add_port_to_vlan(p_bridge, *it, port_mode)) != STD_ERR_OK) {
                    EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                           "Error adding port %d to bridge %d in vlan", *it, p_bridge->ifindex);
                    return rc;
                }

                p_roll_bk->port_add_list[*(it)] = port_mode;
                publish_list.insert(*it);
            }
        }
    } /* for */
    if (!publish_list.empty()) {
        nas_publish_vlan_port_list(p_bridge, publish_list, port_mode, cps_api_oper_CREATE);
    }
    return rc;
}
