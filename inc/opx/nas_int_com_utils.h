
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
* filename: nas_int_com_utils.h
*/

#ifndef NAS_INT_COM_UTILS_H_
#define NAS_INT_COM_UTILS_H_

#include "hal_interface_defaults.h"
#include "hal_if_mapping.h"
#include "dell-base-interface-common.h"

#include <stdlib.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <list>

typedef std::unordered_set<hal_ifindex_t> nas_port_list_t;

typedef struct _if_master_info {
    nas_int_type_t type;
    nas_port_mode_t mode;
    hal_ifindex_t  m_if_idx;
}if_master_info_t;

bool nas_intf_add_master(hal_ifindex_t ifx, if_master_info_t m_info);
bool nas_intf_del_master(hal_ifindex_t ifx, if_master_info_t m_info);
void nas_intf_master_callback(hal_ifindex_t ifx, std::function< void (if_master_info_t)> fn);
std::list<if_master_info_t> nas_intf_get_master(hal_ifindex_t ifx);
BASE_IF_MODE_t nas_intf_get_mode(hal_ifindex_t ifx);
bool nas_intf_handle_intf_mode_change(hal_ifindex_t ifx, BASE_IF_MODE_t mode);
bool nas_intf_handle_intf_mode_change (const char *if_name, BASE_IF_MODE_t mode);

t_std_error nas_intf_db_obj_get(const char * intf_name,cps_api_attr_id_t id, cps_api_qualifier_t cat,
                            cps_api_object_t  obj);
#endif //NAS_INT_COM_UTILS_H_
