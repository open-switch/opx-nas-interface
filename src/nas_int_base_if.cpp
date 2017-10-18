/*
 * Copyright (c) 2017 Dell Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 * LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
 * FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 * See the Apache Version 2.0 License for specific language governing
 * permissions and limitations under the License.
 */

/*
 * filename: nas_int_base_if.cpp
 *
 *  Created on: May 22, 2017
 */

#include "event_log.h"
#include "event_log_types.h"
#include "nas_int_base_if.h"
#include "nas_int_utils.h"
#include "dell-base-routing.h"
#include "cps_api_operation.h"
#include "cps_api_object_key.h"
#include "cps_class_map.h"

#include <exception>

static std::unique_ptr<nas_intf_container> if_cont_inst (new nas_intf_container);

//Interface common APIs

bool nas_intf_add_master(hal_ifindex_t ifx, if_master_info_t m_info)
{
    try {
        if_cont_inst->nas_intf_add_master(ifx, m_info);
    } catch (std::exception& e) {
        EV_LOGGING(INTERFACE,ERR,"IF_CONT", "%s", e.what());
        return false;
    }

    return true;
}

bool nas_intf_del_master(hal_ifindex_t ifx, if_master_info_t m_info)
{
    return if_cont_inst->nas_intf_del_master(ifx, m_info);
}

void nas_intf_master_callback(hal_ifindex_t ifx, std::function< void (if_master_info_t)> fn)
{
    if_cont_inst->nas_intf_master_callbk(ifx, fn);
}

std::list<if_master_info_t> nas_intf_get_master(hal_ifindex_t ifx)
{
    return if_cont_inst->nas_intf_get_master_list(ifx);
}

BASE_IF_MODE_t nas_intf_get_mode(hal_ifindex_t ifx)
{
    return if_cont_inst->nas_intf_get_mode(ifx);
}

void nas_intf_dump_container(hal_ifindex_t ifx=0) {
    if_cont_inst->nas_intf_dump_container(ifx);
}

bool nas_intf_handle_intf_mode_change (hal_ifindex_t ifx, BASE_IF_MODE_t mode)
{
    interface_ctrl_t intf_ctrl;
    memset(&intf_ctrl, 0, sizeof(intf_ctrl));
    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
    intf_ctrl.if_index = ifx;

    if (dn_hal_get_interface_info(&intf_ctrl) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR,"IF_CONT", "Interface (%d) not found", ifx);
        return false;
    }

    return nas_intf_handle_intf_mode_change(intf_ctrl.if_name, mode);
}

/*
 * nas_intf_handle_intf_mode_change is to update nas-l3 about the interface mode
 * change(BASE_IF_MODE_MODE_NONE/BASE_IF_MODE_MODE_L2)
 */

bool nas_intf_handle_intf_mode_change (const char * if_name, BASE_IF_MODE_t mode)
{
    cps_api_transaction_params_t params;
    cps_api_object_t             obj;
    cps_api_key_t                keys;
    bool                         rc = true;

    EV_LOGGING(INTERFACE, DEBUG, "IF_CONT", "Interface mode change update called");
    do {
        if ((obj = cps_api_object_create()) == NULL) {
            EV_LOGGING(INTERFACE,ERR,"IF_CONT", "Interface mode change update failed");
            rc = false;
            break;
        }
        cps_api_object_guard obj_g (obj);
        if (cps_api_transaction_init(&params) != cps_api_ret_code_OK) {
            rc = false;
            break;
        }
        cps_api_transaction_guard tgd(&params);
        cps_api_key_from_attr_with_qual(&keys, BASE_ROUTE_INTERFACE_MODE_CHANGE_OBJ,
                                        cps_api_qualifier_TARGET);
        cps_api_object_set_key(obj, &keys);

        cps_api_object_attr_add(obj, BASE_ROUTE_INTERFACE_MODE_CHANGE_INPUT_IFNAME,
                                if_name, strlen(if_name) + 1);
        cps_api_object_attr_add_u32(obj, BASE_ROUTE_INTERFACE_MODE_CHANGE_INPUT_MODE,
                                    mode);

        if (cps_api_action(&params, obj) != cps_api_ret_code_OK) {
            rc = false;
            break;
        }
        if (cps_api_commit(&params) != cps_api_ret_code_OK) {
            rc = false;
            break;
        }
        obj = NULL;
    } while (false);

    EV_LOGGING(INTERFACE, DEBUG, "IF_CONT", "Interface mode change update returning  (%s)",
               rc == true ? "SUCCESS" : "FAILED");
    return rc;
}

//Interface container class definitions

auto nas_intf_container::nas_intf_add_object(hal_ifindex_t ifx) -> _if_iter_type {

    char name[HAL_IF_NAME_SZ] = "\0";
    memset(name,0,sizeof(name));
    if (nas_int_get_if_index_to_name(ifx, name, sizeof(name)) != STD_ERR_OK)
        throw std::invalid_argument("Conversion to name failed");

    _if_unique_obj obj (new nas_intf_obj(ifx, name));
    return (if_objects.insert(std::make_pair(ifx, std::move(obj)))).first;
}

void nas_intf_container::nas_intf_del_object(hal_ifindex_t ifx) {

    auto itr = if_objects.find(ifx);

    if(itr != if_objects.end()) {
        if_objects.erase(itr);
    }
}

bool nas_intf_container::nas_intf_add_master(hal_ifindex_t ifx, if_master_info_t m_info) {

    std_rw_lock_write_guard lg(&rw_lock);

    auto itr = if_objects.find(ifx);

    if(itr == if_objects.end()) {
        itr = nas_intf_add_object(ifx);
    }

    auto& ptr = itr->second;
    return ptr->nas_intf_obj_master_add(m_info);
}

bool nas_intf_container::nas_intf_del_master(hal_ifindex_t ifx, if_master_info_t m_info) {

    std_rw_lock_write_guard lg(&rw_lock);

    auto itr = if_objects.find(ifx);

    //Interface object not created
    if(itr == if_objects.end()) {
        return false;
    }

    auto& ptr = itr->second;
    auto rc = ptr->nas_intf_obj_master_delete(m_info);
    if(ptr->nas_intf_obj_is_mlist_empty()) {
        nas_intf_del_object(ifx);
    }

    return rc;
}

void nas_intf_container::nas_intf_master_callbk(hal_ifindex_t ifx, master_fn_type fn) {

    std_rw_lock_read_guard lg(&rw_lock);

    auto itr = if_objects.find(ifx);

    if(itr == if_objects.end()) {
        return;
    }

    auto& ptr = itr->second;
    ptr->nas_intf_obj_for_each_master(fn);
}

std::list<if_master_info_t> nas_intf_container::nas_intf_get_master_list(hal_ifindex_t ifx) {

    std::list<if_master_info_t> tmp;

    std_rw_lock_read_guard lg(&rw_lock);

    auto itr = if_objects.find(ifx);

    //Return empty list if interface not created
    if(itr == if_objects.end()) {
        return tmp;
    }

    auto& ptr = itr->second;
    return ptr->nas_intf_obj_master_list();
}

BASE_IF_MODE_t nas_intf_container::nas_intf_get_mode(hal_ifindex_t ifx) {

    std_rw_lock_read_guard lg(&rw_lock);

    auto itr = if_objects.find(ifx);

    return ((itr == if_objects.end())
            ? BASE_IF_MODE_MODE_NONE
            : BASE_IF_MODE_MODE_L2);
}

void nas_intf_container::nas_intf_dump_container(hal_ifindex_t ifx) noexcept {

    std_rw_lock_read_guard lg(&rw_lock);

    auto l_fn = [] (const std::unique_ptr<nas_intf_obj> & ptr) {
        auto list = ptr->nas_intf_obj_master_list();
        for(auto iy : list) {
            EV_LOGGING(INTERFACE,DEBUG,"IF_CONT", "Master idx %d, type %d, mode %d",
                       iy.m_if_idx, iy.type, iy.mode);
        }
    };

    if(ifx) {
        auto itr = if_objects.find(ifx);
        if(itr == if_objects.end()) {
            EV_LOGGING(INTERFACE,DEBUG,"IF_CONT", "Object doesn't exist %d", ifx);
            return;
        }

        EV_LOGGING(INTERFACE,DEBUG,"IF_CONT", "*****Dumping list for %d*****", ifx);
        l_fn(itr->second);

        return;
    }

    for(const auto& ix : if_objects) {
        EV_LOGGING(INTERFACE,DEBUG,"IF_CONT", "*****Dumping list for %d*****", ix.first);
        l_fn(ix.second);
    }
}

//Interface object class definitions

bool nas_intf_obj::nas_intf_obj_master_add(if_master_info_t m_info) {

    //If LAG master, insert at front
    if(m_info.type == nas_int_type_LAG){
        m_list.push_front(m_info);
    }else{
        m_list.push_back(m_info);
    }

    return true;
}

bool nas_intf_obj::nas_intf_obj_master_delete(if_master_info_t m_info) {

    for(auto itr = m_list.begin(); itr != m_list.end(); ++itr) {
        if(itr->m_if_idx == m_info.m_if_idx) {
            itr = m_list.erase(itr);
            return true;
        }
    }

    //No master found
    return false;
}

