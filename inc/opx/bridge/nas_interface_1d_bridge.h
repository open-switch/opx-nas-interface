
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
 * filename: nas_interface_1d_bridge.h
 */

#ifndef _NAS_INTERFACE_1D_BRIDGE_H
#define _NAS_INTERFACE_1D_BRIDGE_H

#include "dell-interface.h"

#include "nas_interface_bridge.h"
#include "nas_ndi_1d_bridge.h"
#include "interface/nas_interface_vxlan.h"
#include "bridge/nas_interface_bridge_com.h"

#include <iostream>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <stdlib.h>

#define DEFAULT_UNTAGGED_VLAN_ID 1
#define INVALID_L2MC_GROUP_ID ((ndi_obj_id_t) ~0x0)


typedef enum {
    BRIDGE_MOD_CREATE, /* Created from bidge model */
    INT_MOD_CREATE, /* created from INTERFACE model */
    BOTH_MODEL, /* Cretaed from bridge ans interface model */
    NONE, /* Means could be 1Q-> 1D migration  like at addition of vtep to 1Q bridge or set of mode change attribute.Currently noone uses mode change from 1q->1d*/
} create_type_t;

class NAS_DOT1D_BRIDGE : public NAS_BRIDGE {
    private:
         nas_bridge_id_t bridge_id; /*  .1D bridge ID created in the NPU */
         ndi_obj_id_t    l2mc_group_id; /*  L2MC group created for the .1D bridge */

         memberlist_t    _vxlan_members;
         using l2mc_member_map = std::unordered_set<ndi_obj_id_t>;
         l2mc_member_map l2mc_members;
         hal_vlan_id_t   untagged_vlan_id;
         create_type_t  create_flg;

    public:
        /** Constructor */
        NAS_DOT1D_BRIDGE(std::string name,
                           BASE_IF_BRIDGE_MODE_t type,
                           hal_ifindex_t idx) : NAS_BRIDGE(name,
                                                           type,
                                                           idx)
                                                           {
                                                               bridge_id = NAS_INVALID_BRIDGE_ID;
                                                               l2mc_group_id = INVALID_L2MC_GROUP_ID;
                                                               untagged_vlan_id = DEFAULT_UNTAGGED_VLAN_ID;
                                                               create_flg = NONE;
                                                            }
        virtual ~NAS_DOT1D_BRIDGE(){}
        t_std_error nas_bridge_npu_create();
        t_std_error nas_bridge_l2mc_create();
        t_std_error nas_bridge_npu_delete();
        t_std_error nas_bridge_l2mc_delete();
        t_std_error nas_bridge_check_vxlan_membership(std::string mem_name, bool *present);
        t_std_error nas_bridge_get_vxlan_member_list(memberlist_t &m_list);
        t_std_error nas_bridge_add_remove_member(std::string & mem_name, nas_port_mode_t port_mode, bool add);
        t_std_error nas_bridge_add_remove_memberlist(memberlist_t & m_list, nas_port_mode_t port_mode, bool add);
        t_std_error nas_bridge_npu_add_remove_member(std::string &mem_name, nas_int_type_t mem_type, bool add_member);
        t_std_error nas_bridge_associate_npu_port(std::string &mem_name, ndi_port_t *port, nas_port_mode_t port_mode, bool associate);
        bool  nas_add_sub_interface(void);
        t_std_error nas_bridge_intf_cntrl_block_register(hal_intf_reg_op_type_t op);
        t_std_error nas_bridge_remove_remote_endpoint(BASE_CMN_VNI_t vni, hal_ip_addr_t local_ip, remote_endpoint_t *rm_endpoint);
        t_std_error nas_bridge_add_remote_endpoint(BASE_CMN_VNI_t vni, hal_ip_addr_t local_ip, remote_endpoint_t *rm_endpoint);
        t_std_error nas_bridge_update_remote_endpoint(remote_endpoint_t * rem_ep);
        t_std_error nas_bridge_get_remote_endpoint_stats(hal_ip_addr_t local_ip, remote_endpoint_t *rm_endpoint, ndi_stat_id_t *ndi_stat_ids, uint64_t* stats_val, size_t len);
        t_std_error nas_bridge_npu_add_vxlan_member(std::string mem_name);
        t_std_error nas_bridge_npu_remove_vxlan_member(std::string mem_name);
        cps_api_return_code_t nas_bridge_fill_info(cps_api_object_t obj);
        bool nas_bridge_vxlan_intf_present(void);
        t_std_error nas_bridge_add_vxlan_member_in_list(std::string mem_name);
        t_std_error nas_bridge_nas_bridge_remove_vxlan_member_from_list(std::string mem_name);
        t_std_error nas_bridge_set_flooding(remote_endpoint_t *rm_endpoint); /* Sets flooding in NPU */

        nas_bridge_id_t bridge_id_get(void) {return bridge_id;}
        ndi_obj_id_t l2mc_group_id_get(void) {return l2mc_group_id;}
        t_std_error nas_bridge_set_learning_disable(bool disable);
        void nas_bridge_untagged_vlan_id_set(hal_vlan_id_t untagged_vlan) { untagged_vlan_id = untagged_vlan;}

        /*  untagged vlan ID is by default set to 1 during bridge creation.
         *  it is updated when global untagged vlan ID is configured
         *  In case if the bridge is parent bridge of a vlan interface then that vlan ID is used as
         *  untagged vlan ID. During detch of the vlan interface it is reset to default UNTAGGED VLAN ID.
         *  Untagged VLAN ID is used to program NPU for untagged port member of the 1d bridge.
         *  untagged VLAN ID is set as pvid and subport vlan id in case of untagged port.
         *  */
        hal_vlan_id_t nas_bridge_untagged_vlan_id_get(void) {
           if (untagged_vlan_id == DEFAULT_UNTAGGED_VLAN_ID) {
                nas_vlan_interface_get_vn_untagged_vlan(&untagged_vlan_id);
                if (untagged_vlan_id == DEFAULT_UNTAGGED_VLAN_ID) {
                    EV_LOGGING(INTERFACE,INFO,"NAS-BRIDGE", "UNTAGGED VLAN ID is not configured yet");
                }
            }
            return untagged_vlan_id;
        }
        create_type_t get_create_flag(void) {return create_flg;}
        void set_create_flag(create_type_t flg) {create_flg = flg;}
        virtual t_std_error nas_bridge_set_mtu(cps_api_object_t obj, cps_api_object_it_t & it);


};

#endif /* _NAS_INTERFACE_1D_BRIDGE_H */
