/*
 * Copyright (c) 2019 Dell Inc.
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

bool nas_intf_add_master(hal_ifindex_t ifx, if_master_info_t m_info, BASE_IF_MODE_t *new_mode, bool *mode_change)
{
    try {
        if_cont_inst->nas_intf_add_master(ifx, m_info, new_mode, mode_change);
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

bool nas_intf_del_master(hal_ifindex_t ifx, if_master_info_t m_info, BASE_IF_MODE_t *new_mode, bool *mode_change)
{
    return if_cont_inst->nas_intf_del_master(ifx, m_info, new_mode, mode_change);
}


bool nas_intf_update_master(hal_ifindex_t ifx, if_master_info_t m_info, bool add, BASE_IF_MODE_t *new_mode, bool *mode_change)
{
    if(add) {
        return (nas_intf_add_master(ifx, m_info, new_mode, mode_change));
    } else {
        return (nas_intf_del_master(ifx, m_info, new_mode, mode_change));
    }
    return false;
}

void nas_intf_master_callback(hal_ifindex_t ifx, std::function< void (if_master_info_t)> fn)
{
    if_cont_inst->nas_intf_master_callbk(ifx, fn);
}

std::pair<int,int> nas_intf_untag_tag_count(hal_ifindex_t ifx)
{
     return if_cont_inst->nas_intf_get_untag_tag_cnt(ifx);

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

bool nas_intf_container::nas_intf_add_master(hal_ifindex_t ifx, if_master_info_t m_info,
                                                BASE_IF_MODE_t *new_mode, bool *mode_change) {

    std_rw_lock_write_guard lg(&rw_lock);
    *mode_change = false;

    auto itr = if_objects.find(ifx);

    if(itr == if_objects.end()) {
        itr = nas_intf_add_object(ifx);
        *new_mode = BASE_IF_MODE_MODE_L2;
        *mode_change = true;
    }

    auto& ptr = itr->second;
    return ptr->nas_intf_obj_master_add(m_info);
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

bool nas_intf_container::nas_intf_del_master(hal_ifindex_t ifx, if_master_info_t m_info,
                                                BASE_IF_MODE_t *new_mode, bool *mode_change) {

    std_rw_lock_write_guard lg(&rw_lock);
    *mode_change = false;

    auto itr = if_objects.find(ifx);

    //Interface object not created
    if(itr == if_objects.end()) {
        return false;
    }

    auto& ptr = itr->second;
    auto rc = ptr->nas_intf_obj_master_delete(m_info);
    if(ptr->nas_intf_obj_is_mlist_empty()) {
        /*  If mester list has become empty then Intf mode changes from L2 to NONE */
        *new_mode = BASE_IF_MODE_MODE_NONE;
        *mode_change = true;
        nas_intf_del_object(ifx);
    }

    return rc;
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

std::pair<int,int> nas_intf_container::nas_intf_get_untag_tag_cnt(hal_ifindex_t ifx) {
    std_rw_lock_read_guard lg(&rw_lock);

    auto itr = if_objects.find(ifx);

    //Return -1 if interface not created
    if(itr == if_objects.end()) {
        return std::make_pair(-1,-1);
    }
    auto& ptr = itr->second;
    return ptr->nas_intf_obj_untag_tag_cnt();


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
        auto untag_tag_pair = ptr->nas_intf_obj_untag_tag_cnt();
        EV_LOGGING(INTERFACE,DEBUG,"IF_CONT", "Untagged cnt %d , tagged_cnt %d \n",
                untag_tag_pair.first, untag_tag_pair.second);
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

    for(auto itr = m_list.begin(); itr != m_list.end(); ++itr) {
        if(itr->m_if_idx == m_info.m_if_idx) {
            return false;
        }
    }

    //If LAG master, insert at front
    if(m_info.type == nas_int_type_LAG){
        m_list.push_front(m_info);
    }else{
        m_list.push_back(m_info);
    }
    if (m_info.type == nas_int_type_VLAN) {
        if (m_info.mode == NAS_PORT_UNTAGGED) {
            untagged_cnt ++;
            EV_LOGGING(INTERFACE,DEBUG,"IF_CONT", "nas_intf_obj_master_add master %d untag_cnt %d", m_info.m_if_idx, untagged_cnt);
        }
        if (m_info.mode == NAS_PORT_TAGGED) {
            tagged_cnt ++;
            EV_LOGGING(INTERFACE,DEBUG,"IF_CONT", "nas_intf_obj_master_add master %d tag_cnt %d", m_info.m_if_idx, tagged_cnt);
        }
    }

    return true;
}

bool nas_intf_obj::nas_intf_obj_master_delete(if_master_info_t m_info) {

    for(auto itr = m_list.begin(); itr != m_list.end(); ++itr) {
        if(itr->m_if_idx == m_info.m_if_idx) {
            if (m_info.type == nas_int_type_VLAN) {
                if (m_info.mode == NAS_PORT_UNTAGGED || m_info.mode == NAS_PORT_HYBRID) {
                    untagged_cnt --;
                }
                if (m_info.mode == NAS_PORT_TAGGED || m_info.mode == NAS_PORT_HYBRID) {
                    tagged_cnt --;
                }
            }
        itr = m_list.erase(itr);
        return true;
        }
    }
    //No master found
    return false;
}


std::pair<int, int> nas_intf_obj::nas_intf_obj_untag_tag_cnt(void) {
    std::pair<int,int> val;
    val.first = untagged_cnt;
    val.second = tagged_cnt;
    return val;
}
