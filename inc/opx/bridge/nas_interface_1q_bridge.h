
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
 * filename: nas_interface_1q_bridge.h
 */

#ifndef _NAS_INTERFACE_1Q_BRIDGE_H
#define _NAS_INTERFACE_1Q_BRIDGE_H


#include "dell-interface.h"
#include "dell-base-if-vlan.h"
#include "dell-base-if.h"

#include "cps_api_object_key.h"
#include "nas_interface_bridge.h"
#include "interface/nas_interface_vlan.h"

#include "stdint.h"
#include <iostream>
#include <map>
#include <string>
#include <stdlib.h>

#define MIN_VLAN_ID         1
#define MAX_VLAN_ID         4094
#define SYSTEM_DEFAULT_VLAN 1
#define NAS_VLAN_ID_INVALID 0

class NAS_DOT1Q_BRIDGE : public NAS_BRIDGE {

    public:
        hal_vlan_id_t       bridge_vlan_id;
        BASE_IF_VLAN_TYPE_t bridge_sub_type; /*  DATA/MGMT */

        BASE_IF_MODE_t   l3_mode;  // In case of L3 scale mode sub interfaces are created in the kernel
                                   // and added in the bridge. Otherwise it is just updated inthe NPU.
                                   // In Case of non-L3 mode , there is not need to allocate resources in the
                                   // kernel for sub-interfaces. For attached case precedence is give to mode of VN.

        NAS_DOT1Q_BRIDGE(std::string name,
                           BASE_IF_BRIDGE_MODE_t type,
                           hal_ifindex_t idx) : NAS_BRIDGE(name,
                                                           type,
                                                           idx)
                                                           {
                                                               bridge_sub_type = BASE_IF_VLAN_TYPE_DATA;
                                                               bridge_vlan_id = NAS_VLAN_ID_INVALID;
                                                               l3_mode            = BASE_IF_MODE_MODE_L3;
                                                           }
        virtual ~NAS_DOT1Q_BRIDGE(){}
        t_std_error nas_bridge_npu_create();
        t_std_error nas_bridge_npu_delete();
        t_std_error nas_bridge_add_remove_member(std::string & mem_name, nas_port_mode_t port_mode, bool add);
        t_std_error nas_bridge_npu_add_remove_member(std::string &mem_name, nas_int_type_t mem_type, bool add_member);
        t_std_error nas_bridge_add_remove_memberlist(memberlist_t & m_list, nas_port_mode_t port_mode, bool add);
        t_std_error nas_bridge_associate_npu_port(std::string &mem_name, ndi_port_t *port, nas_port_mode_t port_mode, bool associate);
        t_std_error nas_bridge_npu_update_all_untagged_members(void);
        t_std_error nas_bridge_intf_cntrl_block_register(hal_intf_reg_op_type_t op);
        bool nas_add_sub_interface();
        cps_api_return_code_t nas_bridge_fill_info(cps_api_object_t obj);
        void nas_bridge_vlan_id_set(hal_vlan_id_t vlan_id) { bridge_vlan_id = vlan_id;}
        hal_vlan_id_t nas_bridge_vlan_id_get(void) {return bridge_vlan_id;}
        BASE_IF_VLAN_TYPE_t nas_bridge_sub_type_get(void) { return bridge_sub_type;}
        BASE_IF_MODE_t bridge_l3_mode_get(void) { return l3_mode;}
        void bridge_l3_mode_set(BASE_IF_MODE_t mode) { l3_mode = mode;}
        void nas_bridge_sub_type_set(BASE_IF_VLAN_TYPE_t type ) {bridge_sub_type = type;}
        t_std_error nas_bridge_set_learning_disable(bool disable);
};
#endif /* _NAS_INTERFACE_1Q_BRIDGE_H */
