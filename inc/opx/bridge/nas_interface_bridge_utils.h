
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
 * filename:  nas_interface_bridge_utils.h
 */

#ifndef _NAS_INTERFACE_BRIDGE_UTILS_H
#define _NAS_INTERFACE_BRIDGE_UTILS_H

#include "dell-base-interface-common.h"
#include "hal_if_mapping.h"
#include "ds_common_types.h"
#include "nas_interface_bridge.h"
#include "interface/nas_interface_vxlan.h"
#include "nas_interface_1q_bridge.h"
#include "nas_interface_1d_bridge.h"
#include "nas_interface_bridge_cps.h"
#include "nas_interface_bridge_map.h"
#include "stdint.h"

#include <iostream>
#include <vector>
#include <string>
#include <stdlib.h>

/*  APIs for vlan to bridge mapping */
bool nas_bridge_vlan_to_bridge_get(hal_vlan_id_t vlan_id, std::string &bridge_name);
bool nas_bridge_vlan_to_bridge_map_add(hal_vlan_id_t vlan_id, std::string &bridge_name);
bool nas_bridge_vlan_to_bridge_map_del(hal_vlan_id_t vlan_id);
bool nas_bridge_vlan_in_use(hal_vlan_id_t vlan_id);

/*  APIs to Get/Set Bridge attributes */
bool nas_bridge_is_empty(const std::string & bridge_name);
t_std_error nas_bridge_utils_parent_bridge_get(const char *br_name, std::string &parent_bridge );
t_std_error nas_bridge_utils_parent_bridge_set(const char *br_name, std::string &parent_bridge );
t_std_error nas_bridge_utils_vlan_id_get(const char * br_name, hal_vlan_id_t *vlan_id );
t_std_error nas_bridge_utils_l3_mode_set(const char * br_name, BASE_IF_MODE_t mode);
t_std_error nas_bridge_utils_l3_mode_get(const char * br_name, BASE_IF_MODE_t *mode);
t_std_error nas_bridge_utils_vlan_type_set(const char * br_name, BASE_IF_VLAN_TYPE_t vlan_type);
t_std_error nas_bridge_utils_mem_list_get(const char * br_name, memberlist_t &mem_list,
                                            nas_port_mode_t port_mode);

t_std_error nas_bridge_utils_set_attribute(const char *br_name, cps_api_object_t obj , cps_api_object_it_t & it);

t_std_error nas_bridge_utils_set_learning_disable(const char *br_name, bool disable);


/*  APIs to create and delete bridge  and its memberlist in the kernel  */
t_std_error nas_bridge_utils_os_create_bridge(const char *br_name, cps_api_object_t obj , hal_ifindex_t *idx);
t_std_error nas_bridge_utils_os_delete_bridge(const char *br_name);
t_std_error nas_bridge_utils_os_add_remove_memberlist(const char *br_name, memberlist_t & memlist, nas_port_mode_t port_mode, bool add);
/*  Generic API to delete bridge completly in the kernel and in the NPU along with all of its members */
t_std_error nas_bridge_delete_bridge(const char *br_name);

/*  APIs to to create bridge in the NPU and local cache */
t_std_error nas_bridge_utils_change_mode(const char *br_name, BASE_IF_BRIDGE_MODE_t br_mode);
t_std_error nas_bridge_utils_create_obj(const char *name, BASE_IF_BRIDGE_MODE_t br_type, hal_ifindex_t idx, NAS_BRIDGE **bridge_obj = nullptr);
t_std_error nas_create_bridge(const char *name, BASE_IF_BRIDGE_MODE_t br_type, hal_ifindex_t idx, NAS_BRIDGE **bridge_obj);
t_std_error nas_bridge_create_vlan(const char *br_name, hal_vlan_id_t vlan_id, cps_api_object_t obj,
                                    NAS_BRIDGE **bridge_obj = nullptr);
t_std_error nas_bridge_utils_validate_bridge_mem_list(const char *br_name,
                                                std::list <std::string> &intf_list,
                                                std::list <std::string>&mem_list);
t_std_error nas_bridge_utils_if_bridge_exists(const char *name, NAS_BRIDGE **bridge_obj = nullptr);
t_std_error nas_bridge_utils_delete_obj(NAS_BRIDGE *br_obj);
t_std_error nas_bridge_utils_delete(const char  *br_name);
t_std_error nas_bridge_utils_npu_add_member(const char *br_name, nas_int_type_t mem_type, const char *mem_name);
t_std_error nas_bridge_utils_npu_remove_member(const char *br_name, nas_int_type_t mem_type, const char *mem_name);

t_std_error nas_bridge_utils_add_remote_endpoint(const char *vxlan_intf_name, remote_endpoint_t & endpoint);
t_std_error nas_bridge_utils_remove_remote_endpoint(const char *vxlan_intf_name, remote_endpoint_t & endpoint);
t_std_error nas_bridge_utils_update_remote_endpoint(const char * vxlan_intf_name, remote_endpoint_t & endpoint);

/*  Generic API to add and delete a Member to a bridge in both kernel and NPU */
t_std_error nas_bridge_utils_add_member(const char *br_name, const char *mem_name);
t_std_error nas_bridge_utils_remove_member(const char *br_name, const char *mem_name);

t_std_error nas_bridge_utils_ifindex_get(const char * br_name, hal_ifindex_t *id);
t_std_error nas_bridge_utils_get_remote_endpoint_stats(const char *vxlan_intf_name, hal_ip_addr_t & remote_ip, ndi_stat_id_t *ndi_stat_ids, uint64_t* stats_val, size_t len);

/*  APIs to Publish bridge events  */
void nas_bridge_utils_publish_event(const char * bridge_name, cps_api_operation_types_t op);
void nas_bridge_utils_publish_member_event(std::string bridge_name, std::string mem_name, cps_api_operation_types_t op);

bool nas_bridge_is_empty(const std::string & bridge_name);
t_std_error nas_bridge_utils_set_untagged_vlan(const char * bridge_name, hal_vlan_id_t vlan_id);

void nas_bridge_utils_publish_memberlist_event(std::string bridge_name, memberlist_t &memlist, cps_api_operation_types_t op);
void nas_bridge_utils_publish_vlan_intf_event(const char * bridge_name, cps_api_operation_types_t op);

/*  APIs for VLAN attach and Detach to a parent bridge */
t_std_error nas_bridge_utils_detach_vlan(const char *vlan_intf, const char *parent_bridge);
t_std_error nas_bridge_utils_attach_vlan(const char *vlan_intf, const char *parent_bridge);

/*  API to Process CPS request for updating new tagged and untagged list for a VLAN bridge  */
t_std_error nas_bridge_process_cps_memberlist(const char *br_name, memberlist_t *new_tagged_list,
                                                        memberlist_t *new_untagged_list);

t_std_error nas_bridge_utils_associate_npu_port(const char * br_name, const char *mem_name, ndi_port_t *ndi_port,
                                                    nas_port_mode_t mode, bool associate);

t_std_error nas_bridge_utils_check_membership(const char *br_name, const char *mem_name , bool *present);

t_std_error nas_bridge_set_mode_if_bridge_exists(const char *name, create_type_t ty, bool &exist);
t_std_error nas_bridge_delete_vn_bridge(const char *name, cps_api_object_t obj);

t_std_error nas_bridge_utils_is_l3_bridge(std::string br_name);
t_std_error nas_bridge_utils_is_l2_bridge(std::string br_name);

void nas_bridge_utils_set_mtu (const char *br_name);
void nas_bridge_utils_os_set_mtu (const char *br_name);

t_std_error nas_bridge_os_vxlan_del_frm_br(std::string &br_name);
t_std_error nas_bridge_os_vxlan_create_add_to_br(std::string &br_name);

#endif /* _NAS_INTERFACE_BRIDGE_UTILS_H */
