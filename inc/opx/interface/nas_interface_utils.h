
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
 * filename: nas_interface_utils.h
 */

#ifndef _NAS_INTERFACE_UTILS_H
#define _NAS_INTERFACE_UTILS_H

#include "dell-base-interface-common.h"

#include "nas_interface_map.h"
#include "nas_interface_vlan.h"
#include "nas_interface_vxlan.h"
#include "ds_common_types.h"
#include "hal_if_mapping.h"

#include "stdint.h"

#include <iostream>
#include <vector>
#include <unordered_set>
#include <string>
#include <stdlib.h>

typedef std::unordered_set<std::string> intf_list_t;

// TODO add brief description
t_std_error nas_interface_utils_vlan_create(std::string intf_name, hal_ifindex_t intf_idx, hal_vlan_id_t vlan_id, std::string parent, NAS_VLAN_INTERFACE **vlan_obj = nullptr);
t_std_error nas_interface_utils_vlan_delete(std::string intf_name);
t_std_error nas_interface_utils_vxlan_create(std::string intf_name, hal_ifindex_t intf_idx, BASE_CMN_VNI_t vni, hal_ip_addr_t local_ip, NAS_VXLAN_INTERFACE **vxlan_obj = nullptr);
t_std_error nas_interface_utils_vxlan_delete(std::string intf_name);

t_std_error nas_interface_utils_parent_name_get(std::string &intf_name, std::string &parent);
t_std_error  nas_interface_utils_vlan_id_get(std::string &intf_name, hal_vlan_id_t &vlan_id);
t_std_error nas_interface_vlan_subintf_create(std::string &intf_name, hal_vlan_id_t vlan_id, std::string &parent, bool in_os);
/*  Create sub interfaces with vlan id and physical interface provided.
 *  if in_os is false then just create object for the local  cache */
t_std_error nas_interface_vlan_subintf_list_create(intf_list_t & intf_list, hal_vlan_id_t vlan_id, bool in_os);

t_std_error nas_interface_vlan_subintf_delete(std::string &intf_name);
t_std_error nas_interface_vlan_subintf_list_delete(intf_list_t &list);
t_std_error nas_interface_utils_ifindex_get(const std::string &intf_name, hal_ifindex_t &ifindex);

t_std_error nas_interface_utils_set_vlan_subintf_attr(const std::string & intf_name,  nas_com_id_value_t val[], size_t len);
t_std_error nas_interface_utils_set_all_sub_intf_attr(const std::string & parent_intf,  nas_com_id_value_t val[], size_t len);
void nas_interface_cps_publish_event(std::string &if_name, nas_int_type_t if_type, cps_api_operation_types_t op);


#endif /* _NAS_INTERFACE_UTILS_H */
