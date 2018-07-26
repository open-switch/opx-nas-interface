/*
 * Copyright (c) 2018 Dell Inc.
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
 * filename: nas_interface_map.h
 */

#ifndef _NAS_INTERFACE_MAP_H
#define _NAS_INTERFACE_MAP_H

#include "nas_interface.h"
#include <string>
#include <stdlib.h>

class NAS_INTERFACE *nas_interface_map_obj_get(const std::string &intf_name);
t_std_error nas_interface_map_obj_add(const std::string &intf_name, class NAS_INTERFACE *intf_obj);
t_std_error nas_interface_map_obj_remove(std::string &intf_name, class NAS_INTERFACE **intf_obj);

#endif /* _NAS_INTERFACE_MAP_H */
