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

#include <stdbool.h>
#include <stdio.h>
#include <unordered_set>

const static int MAX_CPS_MSG_BUFF=10000;

static  t_std_error nas_process_cps_ports(nas_bridge_t *p_bridge, nas_port_mode_t port_mode,
                                  nas_port_list_t &port_list);

t_std_error nas_cps_cleanup_vlan_lists(hal_vlan_id_t vlan_id, nas_list_t *p_link_node_list);

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

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t nas_cps_update_vlan(nas_bridge_t *p_bridge, cps_api_object_t obj)
{
    if (p_bridge == NULL) return cps_api_ret_code_ERR;

    cps_api_object_it_t it;
    nas_port_list_t tag_port_list;
    nas_port_list_t untag_port_list;
    bool untag_list_attr = false;
    bool tag_list_attr = false;

    cps_api_object_it_begin(obj,&it);

    for ( ; cps_api_object_it_valid(&it) ; cps_api_object_it_next(&it) ) {

        int id = (int) cps_api_object_attr_id(it.attr);
        switch (id) {
        //currently handling only following attributes, more in the next commit
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
            nas_cps_set_vlan_mac(obj, p_bridge);
            break;

        case DELL_IF_IF_INTERFACES_INTERFACE_LEARNING_MODE:
            nas_cps_set_vlan_learning_mode(obj, p_bridge);
            break;

        case IF_INTERFACES_INTERFACE_ENABLED:
            nas_vlan_set_admin_status(obj, p_bridge);
            break;
        default:
            EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                   "Received attrib %d", id);
            break;
        }
    }

    if (tag_list_attr == true) {
        EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                "Received %d tagged ports for VLAN %d index %d", tag_port_list.size(),
                p_bridge->vlan_id, p_bridge->ifindex);

        if (nas_process_cps_ports(p_bridge, NAS_PORT_TAGGED, tag_port_list) != STD_ERR_OK) {
             return cps_api_ret_code_ERR;

        }
    }
    if (untag_list_attr == true) {
        EV_LOGGING(INTERFACE, INFO , "NAS-Vlan",
               "Received %d untagged ports for VLAN %d index %d", untag_port_list.size(),
               p_bridge->vlan_id, p_bridge->ifindex);

        if (nas_process_cps_ports(p_bridge, NAS_PORT_UNTAGGED, untag_port_list) != STD_ERR_OK){
             return cps_api_ret_code_ERR;
        }
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
            if(mac_attr != NULL) {
                strncpy((char *)p_bridge_node->mac_addr, (char *)cps_api_object_attr_data_bin(mac_attr),
                        cps_api_object_attr_len(mac_attr));
            }
            //Create Vlan in NPU, @TODO for npu_id
            if(vlan_id != SYSTEM_DEFAULT_VLAN) {
                if (nas_vlan_create(0, vlan_id) != STD_ERR_OK) {
                    EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                           "Failure creating VLAN id %d in NPU ", vlan_id);
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
        if(nas_register_vlan_intf(p_bridge_node, HAL_INTF_OP_REG) != STD_ERR_OK) {
            break;
        }
        std_mutex_simple_lock_guard lock_t(vlan_lag_mutex_lock());


        if (nas_cps_update_vlan(p_bridge_node, obj) != cps_api_ret_code_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                   "VLAN Attribute configuration failed for bridge interface %d vlan %d",
                   p_bridge_node->ifindex, p_bridge_node->vlan_id);
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

    // TODO no need to pass both name and if_index
    if (vlan_if_attr == nullptr) {
        cps_api_object_attr_add_u32(idx_obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, p_bridge_node->ifindex);
    }
    if (vlan_name_attr == nullptr) {
        cps_api_set_key_data(idx_obj,IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN,
                              p_bridge_node->name, strlen(p_bridge_node->name)+1);
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

     do {
         cps_api_object_t filter = cps_api_object_list_get(param->filters, ix);
         cps_api_object_attr_t name_attr = cps_api_get_key_data(filter,
                                            IF_INTERFACES_INTERFACE_NAME);

         if (name_attr != NULL) {
              const char *if_name = (char *)cps_api_object_attr_data_bin(name_attr);
              nas_get_vlan_intf(if_name, param->list);
          }
          else {
              nas_vlan_get_all_info(param->list);
          }

         ++iix;
     }while (iix < mx);

    return cps_api_ret_code_OK;
}


t_std_error nas_cps_vlan_init(cps_api_operation_handle_t handle) {

    if (intf_obj_handler_registration(obj_INTF, nas_int_type_VLAN,
                nas_process_cps_vlan_get, nas_process_cps_vlan_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-INIT", "Failed to register VLAN interface CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }
    return STD_ERR_OK;
}

t_std_error nas_cps_add_port_to_os(hal_ifindex_t br_index, hal_vlan_id_t vlan_id,
                                   nas_port_mode_t port_mode, hal_ifindex_t port_idx)
{
    hal_ifindex_t vlan_index = 0;
    char buff[MAX_CPS_MSG_BUFF];
    cps_api_object_t name_obj = cps_api_object_init(buff, sizeof(buff));

    cps_api_object_attr_add_u32(name_obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, br_index);
    cps_api_object_attr_add_u32(name_obj,BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, vlan_id);
    if(port_mode == NAS_PORT_TAGGED) {
        cps_api_object_attr_add_u32(name_obj,DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS, port_idx);
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

    //add to kernel
    if(nas_cps_add_port_to_os(p_bridge->ifindex, p_bridge->vlan_id, port_mode,
                           port_idx) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
               "Error inserting index %d in Vlan %d to OS", port_idx, p_bridge->vlan_id);
        rc = (STD_ERR(INTERFACE,FAIL, 0));
    }

    bool create_flag = false;
    ///Kernel add successful, insert in the local data structure
    p_link_node = nas_create_vlan_port_node(p_bridge, port_idx,
                                            port_mode, &create_flag);

    if (p_link_node != NULL) {
        if((rc = nas_add_or_del_port_to_vlan(p_link_node->ndi_port.npu_id, vlan_id,
                                      &(p_link_node->ndi_port), port_mode, true)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                   "Error adding port <%d %d> to NPU",
                   p_link_node->ndi_port.npu_id, p_link_node->ndi_port.npu_port);
        }

        //Set the untagged port VID
        if(port_mode == NAS_PORT_UNTAGGED) {
            if((rc = ndi_set_port_vid(p_link_node->ndi_port.npu_id,
                             p_link_node->ndi_port.npu_port, vlan_id)) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                       "Error setting port <%d %d> VID to %d ",
                       p_link_node->ndi_port.npu_id, p_link_node->ndi_port.npu_port,
                       vlan_id);

            }
        }
    }
    else {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
               "Error inserting index %d in list", port_idx);
        rc = (STD_ERR(INTERFACE,FAIL, 0));
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

    //delete the port from NPU
    if (nas_add_or_del_port_to_vlan(p_link_node->ndi_port.npu_id, p_bridge->vlan_id,
                                    &(p_link_node->ndi_port), port_mode, false) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
               "Error deleting port %d with mode %d from vlan %d", p_link_node->ifindex,
               port_mode, p_bridge->vlan_id);
        return (STD_ERR(INTERFACE,FAIL, 0));
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
                                  nas_port_list_t &port_index_list)
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
            if (nas_cps_del_port_from_vlan(p_bridge, p_temp_node, port_mode) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                       "Error deleting port %d from Bridge %d in OS", ifindex, p_bridge->ifindex);
            }
        }
    } //while
    if (!publish_list.empty()) {
        nas_publish_vlan_port_list(p_bridge, publish_list, port_mode, cps_api_oper_DELETE);
        publish_list.clear();
    }

    nas_handle_lag_index_in_cps_set(p_bridge, port_index_list, port_mode);

    /*Now lets just consider the add case ..
      check if this port index already exists, if not go and add this port */
    EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
           "Now checking if any new ports are added");
    for (auto it = port_index_list.begin() ; it != port_index_list.end() ; ++it) {
        nas_get_int_type(*it, &int_type);

        if(int_type == nas_int_type_PORT) {
            p_link_node = nas_get_link_node(&(p_list->port_list), *it);

            if (p_link_node == NULL) {
                EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                       "Received new port %d in bridge %d, VLAN %d", *it, p_bridge->ifindex,
                       p_bridge->vlan_id);
                if (nas_cps_add_port_to_vlan(p_bridge, *it, port_mode) != STD_ERR_OK) {
                    EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                           "Error adding port %d to bridge %d in OS", *it, p_bridge->ifindex);
                }

                publish_list.insert(*it);
            }
        }
        else if(int_type == nas_int_type_LAG) {
            EV_LOGGING(INTERFACE, INFO, "NAS-Vlan",
                   "Received LAG %d for addition to Vlan %d", *it, p_bridge->vlan_id);

            rc = nas_handle_lag_update_for_vlan(p_bridge, *it, p_bridge->vlan_id,
                                           port_mode, true, true);
        }
    } /* for */
    if (!publish_list.empty()) {
        nas_publish_vlan_port_list(p_bridge, publish_list, port_mode, cps_api_oper_CREATE);
    }
    return rc;
}
