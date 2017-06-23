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
 * nas_vlan_lag.h
 *
 */


#ifndef NAS_VLAN_LAG_H_
#define NAS_VLAN_LAG_H_

#include "ds_common_types.h"
#include "nas_ndi_common.h"
#include "nas_int_bridge.h"

#include <stdbool.h>
#include <unordered_map>
#include <unordered_set>

#ifdef __cplusplus
extern "C" {
#endif

typedef std::unordered_set <hal_ifindex_t> nas_port_list_t;
typedef std::unordered_set <hal_ifindex_t> ::iterator nas_port_list_it;

typedef struct nas_port_node_s {
    hal_ifindex_t ifindex; //Interface index
    ndi_port_t ndi_port;   //npu and port id's for this interface
} nas_port_node_t;


typedef std::unordered_map <hal_ifindex_t, nas_port_node_t> nas_lag_mem_list_t;
typedef std::unordered_map <hal_ifindex_t, nas_port_node_t>::iterator nas_lag_mem_list_it;

typedef std::unordered_set <hal_vlan_id_t> nas_vlan_list_t;
typedef std::unordered_set <hal_vlan_id_t> ::iterator nas_vlan_list_it;

typedef struct nas_vlan_lag_t{
    hal_ifindex_t lag_index; //the lag index from NAS LAG module
    bool vlan_enable;  //set to true once lag is configured in a vlan
    nas_lag_mem_list_t lag_mem; //Members in LAG group
    nas_vlan_list_t tagged_list; //Tagged Vlans configured on this LAG
    nas_vlan_list_t untagged_list; //Untagged vlans configured on this LAG
}nas_vlan_lag_t;

t_std_error nas_handle_lag_index_in_cps_set(nas_bridge_t *p_bridge, nas_port_list_t &port_index_list,
                                            nas_port_mode_t port_mode, vlan_roll_bk_t *roll_bk);


#ifdef __cplusplus
}
#endif


#endif /* NAS_VLAN_LAG_H_ */
