
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
 * filename: nas_interface_bridge_map.h
 */

#ifndef _NAS_INTERFACE_BRIDGE_MAP_H
#define _NAS_INTERFACE_BRIDGE_MAP_H

#include "nas_interface_bridge.h"
#include "nas_interface_1q_bridge.h"
#include "nas_interface_1d_bridge.h"
#include "nas_interface_bridge_cps.h"
#include "nas_interface_bridge_utils.h"
#include "cps_api_object.h"
#include <unordered_map>
#include <string>
#include <stdlib.h>

typedef std::unordered_map<std::string, class NAS_BRIDGE *> bridge_map_s;
typedef std::pair<std::string, class NAS_BRIDGE *> pair_t;
typedef std::pair<bridge_map_s::iterator, bool> ret_t;

class bridge_map_t {
    public:
        bridge_map_s bmap;
        bridge_map_t(){}
        t_std_error insert(std::string name, NAS_BRIDGE *obj);
        t_std_error remove(std::string name);
        t_std_error find(std::string name, bool *present);
        t_std_error get(std::string name, NAS_BRIDGE **ptr);
        t_std_error show();
};

t_std_error nas_bridge_map_obj_add(std::string name, NAS_BRIDGE *br_obj);
t_std_error nas_bridge_map_obj_remove(std::string name, NAS_BRIDGE **br_obj);
t_std_error nas_bridge_map_obj_get(std::string name, NAS_BRIDGE **br_obj);
cps_api_return_code_t nas_bridge_fill_info(std::string br_name, cps_api_object_t obj);
cps_api_return_code_t nas_fill_all_bridge_info(cps_api_object_list_t *list, model_type_t model, bool get_state = false);
bridge_map_t& nas_bridge_map_get();
#endif /* _NAS_INTERFACE_BRIDGE_MAP_H */
