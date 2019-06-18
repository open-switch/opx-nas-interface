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
 * filename: .cpp
 */

#include <unordered_map>
#include "interface/nas_interface_map.h"

// TODO define map based on the interface type
static nas_intf_obj_map_t intf_obj_map;
nas_intf_obj_map_t nas_interface_obj_map_get() {
    return intf_obj_map;
}
class NAS_INTERFACE *nas_interface_map_obj_get(const std::string &intf_name)
{
    auto it = intf_obj_map.find(intf_name);
    if (it == intf_obj_map.end()) {
        return nullptr;
    }
    return it->second;
}

t_std_error nas_interface_map_obj_add(const std::string &intf_name, class NAS_INTERFACE *intf_obj) {
    auto it = intf_obj_map.find(intf_name);
    if (it != intf_obj_map.end()) {
        EV_LOGGING(INTERFACE,INFO,"NAS-INT", "interface  name already exists in the map %s", intf_name.c_str());
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    intf_obj_map[intf_name] = intf_obj;
    return STD_ERR_OK;
}

t_std_error nas_interface_map_obj_remove(std::string &intf_name, class NAS_INTERFACE **intf_obj) {
    auto it = intf_obj_map.find(intf_name);
    if (it == intf_obj_map.end()) {
        EV_LOGGING(INTERFACE,INFO,"NAS-INT", "interface  name does not exists in the map %s", intf_name.c_str());
        /* not present in the map */
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    *intf_obj = it->second;
    intf_obj_map.erase(intf_name);
    return STD_ERR_OK;
}
