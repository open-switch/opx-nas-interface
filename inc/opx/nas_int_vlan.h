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
 * filename: nas_int_vlan.h
 */

#ifndef NAS_INTF_VLAN_H_
#define NAS_INTF_VLAN_H_

#include "ds_common_types.h"
#include "nas_ndi_common.h"
#include "std_error_codes.h"
#include "cps_api_operation.h"
#include "nas_int_bridge.h"
#include "nas_int_com_utils.h"
#include <stdbool.h>
#include <string>

#define MIN_VLAN_ID 1
#define MAX_VLAN_ID 4094

t_std_error nas_vlan_get_default_info(cps_api_object_list_t list);

t_std_error nas_vlan_create(npu_id_t npu_id, hal_vlan_id_t vlan_id);

t_std_error nas_add_port_to_vlan(npu_id_t npu_id, hal_vlan_id_t vlan_id,
                                 ndi_port_t *p_ndi_port, bool tagged_port);

t_std_error nas_del_port_from_vlan(npu_id_t npu_id, hal_vlan_id_t vlan_id, \
                                   ndi_port_t *p_ndi_port, nas_port_mode_t port_mode);
bool nas_ck_port_exist(hal_vlan_id_t vlan_id, hal_ifindex_t port_index);
t_std_error nas_vlan_delete(npu_id_t npu_id, hal_vlan_id_t vlan_id);
void nas_pack_vlan_if(cps_api_object_t obj, nas_bridge_t *p_bridge);

t_std_error nas_delete_lag_from_vlan_in_npu(hal_ifindex_t ifindex ,hal_vlan_id_t vid,
                 nas_port_mode_t port_mode);
t_std_error nas_process_lag_for_vlan_del(nas_list_t *p_list,
                                              hal_ifindex_t if_index);

cps_api_return_code_t nas_cps_set_vlan_mac(cps_api_object_t obj, nas_bridge_t *p_bridge);

/**
 * @brief Handle the vlan interface deletion request
 *
 * @param br_ifindex - kernel ifindex of the bridge
 *
 * @param port_list  - kernel ifindex list of the vlan interfaces
 *
 * @param port_mode - tag or untag port mode
 * @return None
 */

void nas_process_del_vlan_mem_from_os(hal_ifindex_t br_ifindex, nas_port_list_t &port_list,
                                                nas_port_mode_t port_mode);

/**
 * @brief Handle the vlan interface creation request
 *
 * @param br_ifindex - kernel ifindex of the bridge
 *
 * @param port_list - ifindex list of member ports
 *
 * @param vlan_id - Vlan id for tagged interfaces, zero for untagged
 *
 * @param port_mode - tag or untag port mode
 *
 * @return None
 */
void nas_process_add_vlan_mem_from_os(hal_ifindex_t bridge_id, nas_port_list_t &port_list,
                                         hal_vlan_id_t vlan_id, nas_port_mode_t port_mode);

t_std_error nas_cps_vlan_init(cps_api_operation_handle_t handle);

/**
 * @brief Handle the member port creation and insertion in list
 *
 * @param p_bridge_node - Pointer to Bridge structure
 *
 * @param ifindex - member port index
 *
 * @param port_mode - mode to specify tagged/untagged
 *
 * @return - Pointer to the newly created node, NULL in case of error
*/
nas_list_node_t *nas_create_vlan_port_node(nas_bridge_t *p_bridge_node,
                                           hal_ifindex_t ifindex,
                                           nas_port_mode_t port_mode,
                                           bool *create_flag);

/**
 * @brief Gets the default vlan from SAI and creates in kernel
 *
 * @return : Standard error code
 */
t_std_error nas_create_default_vlan(void);

/**
 * @brief Gets all the vlan interfaces
 *
 * @param  list - CPS API to be filled with Vlan interface
 *
 * @return : Standard error code
 */
t_std_error nas_vlan_get_all_info(cps_api_object_list_t list);

/**
 * @brief Gets a particular vlan interfaces
 *
 * @param  : if_name : Vlan interface name
 *
 * @param  : list : CPS API to be filled with Vlan interface
 *
 * @return : Standard error code
 */
t_std_error nas_get_vlan_intf(const char *if_name, hal_ifindex_t ifindex, cps_api_object_list_t list);

t_std_error nas_get_vlan_intf_from_vid(hal_vlan_id_t vid, cps_api_object_list_t list);
t_std_error nas_register_vlan_intf(nas_bridge_t *p_bridge, hal_intf_reg_op_type_t op);
cps_api_return_code_t nas_publish_vlan_object(nas_bridge_t *p_bridge_node, cps_api_operation_types_t op);

t_std_error nas_add_or_del_port_to_vlan(npu_id_t npu_id, hal_vlan_id_t vlan_id,
                                        ndi_port_t *p_ndi_port, nas_port_mode_t port_mode,
                                        bool add_port, hal_ifindex_t ifindex);

t_std_error nas_cps_add_port_to_os(hal_ifindex_t br_index, hal_vlan_id_t vlan_id,
                                   nas_port_mode_t port_mode, hal_ifindex_t port_idx,uint32_t mtu,
                                   BASE_IF_MODE_t mode);

t_std_error nas_lag_add_del_vlan_update(hal_ifindex_t lag_index, hal_vlan_id_t,
                                        nas_port_mode_t port_mode, bool add_flag);

t_std_error nas_handle_lag_add_to_vlan(nas_bridge_t *p_bridge, hal_ifindex_t lag_index,
                              nas_port_mode_t port_mode, bool cps_add, vlan_roll_bk_t *roll_bk);
t_std_error  nas_handle_lag_del_from_vlan(nas_bridge_t *p_bridge, hal_ifindex_t lag_index,
                nas_port_mode_t port_mode, bool cps_del ,vlan_roll_bk_t *roll_bk,bool migrate);

t_std_error nas_cps_del_port_from_os(hal_vlan_id_t vlan_id, hal_ifindex_t port_index,
                                     nas_port_mode_t port_mode, BASE_IF_MODE_t vlan_mode);

t_std_error nas_del_tagged_lag_intf(hal_ifindex_t lag_index, hal_vlan_id_t vlan_id);
cps_api_return_code_t nas_publish_vlan_port_list(nas_bridge_t *p_bridge_node, nas_port_list_t &port_list,
                                                 nas_port_mode_t port_mode, cps_api_operation_types_t op);
void nas_handle_bridge_mac(nas_bridge_t *pnode);
t_std_error nas_default_vlan_cache_init(void);
bool nas_set_vlan_member_port_mtu(hal_ifindex_t ifindex, uint32_t mtu, hal_vlan_id_t vlan_id);
t_std_error nas_handle_lag_index_in_cps_set(nas_bridge_t *p_bridge, nas_port_list_t &port_index_list,
                                            nas_port_mode_t port_mode, vlan_roll_bk_t *roll_bk);
t_std_error nas_add_or_del_lag_in_vlan(hal_ifindex_t lag_index, hal_vlan_id_t vlan_id,
                                              nas_port_mode_t port_mode, bool add_flag, bool roll_bk);

bool get_parent_bridge_info(const std::string & bridge_name, interface_ctrl_t & entry);
#endif /* NAS_INTF_VLAN_H_ */
