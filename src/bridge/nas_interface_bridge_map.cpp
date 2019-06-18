
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
 * filename: nas_interface_bridge_map.cpp
 */

#include "bridge/nas_interface_bridge_map.h"
#include "event_log.h"
#include "event_log_types.h"
static bridge_map_t &bridge_map = *new bridge_map_t();

t_std_error bridge_map_t::insert(std::string name, NAS_BRIDGE *obj)
{
    bridge_map_s::iterator it = bmap.find(name);
    if (it != bmap.end()) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "bridge %s already exists", name.c_str());
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    ret_t ret = bmap.insert(pair_t(name, obj));
    if (ret.second == false) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "bridge %s insert failed", name.c_str());
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return STD_ERR_OK;
}

t_std_error bridge_map_t::remove(std::string name)
{
    bridge_map_s::iterator it = bmap.find(name);
    if (it == bmap.end()) {
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    bmap.erase(it);
    return STD_ERR_OK;
}

t_std_error bridge_map_t::get(std::string name, NAS_BRIDGE **obj)
{
    bridge_map_s::iterator it = bmap.find(name);
    if (it == bmap.end()) {
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    *obj = it->second;
    return STD_ERR_OK;
}

t_std_error bridge_map_t::find(std::string name, bool *present)
{
    bridge_map_s::iterator it = bmap.find(name);
    if (it == bmap.end()) {
        *present = false;
        return STD_ERR_OK;
    }
    *present = true;
    return STD_ERR_OK;
}

t_std_error bridge_map_t::show(void)
{
    std::cout << "\n Bridge Map : ";
    for(auto it = bmap.begin(); it != bmap.end(); ++it)
    {
        std::cout << "bridge name: " << it->first << "\n";
    }
    // TODO Add bridge show function
    return STD_ERR_OK;
}

t_std_error nas_bridge_map_obj_add(std::string name, NAS_BRIDGE *br_obj) {
    return bridge_map.insert(name, br_obj);
}

t_std_error nas_bridge_map_obj_remove(std::string name, NAS_BRIDGE **br_obj) {
    if ((bridge_map.get(name, br_obj)) != STD_ERR_OK) {
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return (bridge_map.remove(name));
}

t_std_error nas_bridge_map_obj_get(std::string name, NAS_BRIDGE **br_obj) {
    if ((bridge_map.get(name, br_obj)) != STD_ERR_OK) {
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return STD_ERR_OK;
}

cps_api_return_code_t nas_bridge_fill_info(std::string br_name, cps_api_object_t obj)
{
    NAS_BRIDGE *br_obj = nullptr;
    if (nas_bridge_map_obj_get(br_name, &br_obj) != STD_ERR_OK) {
        return  cps_api_ret_code_ERR;
    }
    return (br_obj->nas_bridge_fill_info(obj));
}

cps_api_return_code_t nas_fill_all_bridge_info(cps_api_object_list_t *list, model_type_t model, bool get_state)
{
    // TODO check if List pointer is required to be passed
    std::for_each(bridge_map.bmap.begin(), bridge_map.bmap.end(), [list, model, get_state] (pair_t const& br_obj) {


        if (br_obj.second->get_bridge_model() != model) return;
        cps_api_object_t obj = cps_api_object_create();
        if(obj == nullptr) return;
        cps_api_object_set_type_operation(cps_api_object_key(obj),cps_api_oper_NULL);
        cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
            BRIDGE_DOMAIN_BRIDGE_OBJ, cps_api_qualifier_TARGET);

        if (br_obj.second->nas_bridge_fill_info(obj) != cps_api_ret_code_OK) {
            cps_api_object_delete(obj);
            return;
        }

        if (get_state) {
            cps_convert_to_state_attribute(obj);
        }

        if (cps_api_object_list_append(*list, obj)) {
            return;
        } else {
            cps_api_object_delete(obj);
            return;
        }
    });
    return cps_api_ret_code_OK;
}

bridge_map_t& nas_bridge_map_get() {
   return bridge_map;
}
