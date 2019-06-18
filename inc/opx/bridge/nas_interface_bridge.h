
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
 * filename: nas_interface_bridge.h
 */

#ifndef _NAS_INTERFACE_BRIDGE_H
#define _NAS_INTERFACE_BRIDGE_H

#include "bridge-model.h"

#include "hal_if_mapping.h"
#include "ds_common_types.h"
#include "nas_int_com_utils.h"
#include "nas_os_vlan.h"
#include "cps_api_errors.h"
#include "cps_api_object_key.h"
#include "cps_api_events.h"
#include "cps_class_map.h"
#include "cps_api_operation.h"
#include "event_log.h"
#include "event_log_types.h"
#include "std_utils.h"
#include "nas_if_utils.h"
#include "nas_ndi_vlan.h"
#include "nas_ndi_lag.h"
#include "nas_ndi_port.h"

#include <iostream>
#include <vector>
#include <string>
#include <stdlib.h>
#include <algorithm>
#include <unordered_set>

#define NAS_BRIDGE_INTF_DEFAULT_MTU 1532

typedef std::unordered_set<std::string> memberlist_t;

typedef enum {
    BRIDGE_MODEL,
    INT_VLAN_MODEL,
} model_type_t;


class NAS_BRIDGE {

    public:

        bool source_cps;
        uint64_t              type;
        model_type_t          model_type; // VLAN bridge can be created either by using interface yang model
                                          // with intf type vlan or Bridge Yang model

        hal_ifindex_t         if_index;
        npu_id_t              npu_id;
        memberlist_t          tagged_members;
        memberlist_t          untagged_members;
        memberlist_t          attached_vlans;
        std::string           bridge_name;
        std::string           parent_bridge; // In case if this is created as vlan bridge then it can be attached to
                                             /// parent bridge along with all its members.
        BASE_IF_BRIDGE_MODE_t bridge_mode; // This is 1D or 1Q mode of the bridge
        uint32_t              num_attached_vlans;

        /** Constructor */
        NAS_BRIDGE(std::string name, BASE_IF_BRIDGE_MODE_t type, hal_ifindex_t idx)
        {
            bridge_name        = name;
            num_attached_vlans = 0;
            bridge_mode        = type;
            npu_id             = 0;
            if_index           = idx;
            model_type         = BRIDGE_MODEL;
            source_cps = false;
            learning_mode = BASE_IF_MAC_LEARN_MODE_HW;
            mtu = NAS_BRIDGE_INTF_DEFAULT_MTU;
        }

        virtual ~NAS_BRIDGE() {}
        virtual t_std_error nas_bridge_npu_create() = 0;
        virtual t_std_error nas_bridge_npu_delete() = 0;
        virtual t_std_error nas_bridge_add_remove_member(std::string & mem_name, nas_port_mode_t port_mode, bool add) =0;
        virtual t_std_error nas_bridge_add_remove_memberlist(memberlist_t & m_list, nas_port_mode_t port_mode, bool add)=0;
        virtual t_std_error nas_bridge_npu_add_remove_member(std::string &mem_name, nas_int_type_t mem_type, bool add_member) = 0;
        virtual t_std_error nas_bridge_intf_cntrl_block_register(hal_intf_reg_op_type_t op) = 0;
        virtual cps_api_return_code_t nas_bridge_fill_info(cps_api_object_t obj) = 0;
        virtual t_std_error nas_bridge_associate_npu_port(std::string &mem_name, ndi_port_t *port, nas_port_mode_t port_mode, bool associate) = 0;
        virtual bool nas_add_sub_interface() = 0;
        void nas_bridge_for_each_member(std::function <void (std::string mem_name, nas_port_mode_t port_mode)> fn);
        bool nas_bridge_multiple_vlans_present(void);
        bool nas_bridge_tagged_member_present(void);
        bool nas_bridge_untagged_member_present(void);
        t_std_error nas_bridge_update_member_list(std::string &mem_name, nas_port_mode_t port_mode, bool add_member);
        t_std_error nas_bridge_update_member_list(memberlist_t &memlist, nas_port_mode_t port_mode, bool add_member);
        t_std_error nas_bridge_get_member_list(nas_port_mode_t port_mode, memberlist_t &m_list);
        t_std_error nas_bridge_memberlist_clear(void) { tagged_members.clear(); untagged_members.clear(); return STD_ERR_OK;}
        t_std_error nas_bridge_check_tagged_membership(std::string mem_name, bool *present);
        t_std_error nas_bridge_check_untagged_membership(std::string mem_name, bool *present);
        t_std_error nas_bridge_check_membership(std::string mem_name, bool *present);
        t_std_error nas_bridge_add_vlan_in_attached_list(std::string mem_name);
        t_std_error nas_bridge_add_tagged_member_in_list(std::string mem_name);
        t_std_error nas_bridge_add_untagged_member_in_list(std::string mem_name);
        t_std_error nas_bridge_remove_vlan_member_from_attached_list(std::string mem_name);
        t_std_error nas_bridge_remove_tagged_member_from_list(std::string mem_name);
        t_std_error nas_bridge_remove_untagged_member_from_list(std::string mem_name);
        t_std_error nas_bridge_set_tag_untag_drop(hal_ifindex_t ifx, ndi_port_t *port); /* For ethernet port */
        t_std_error nas_bridge_set_lag_tag_untag_drop(npu_id_t npu_id, ndi_obj_id_t lag_id ,hal_ifindex_t ifx);
        cps_api_return_code_t nas_bridge_fill_com_info(cps_api_object_t obj);

        bool is_source_cps (void) { return source_cps;}
        void set_source_cps (void) {source_cps = true;}
        hal_ifindex_t get_bridge_intf_index(void) { return if_index;}
        std::string get_bridge_name(void) { return bridge_name;}
        model_type_t get_bridge_model (void) { return model_type;}
        void set_bridge_model(model_type_t _type) { model_type = _type;}
        void set_bridge_mac_learn_mode(BASE_IF_MAC_LEARN_MODE_t mode) { learning_mode = mode;}
        BASE_IF_MAC_LEARN_MODE_t get_bridge_mac_learn_mode(void) { return learning_mode; }
        std::string bridge_parent_bridge_get(void) { return parent_bridge;}
        void bridge_parent_bridge_set(std::string &p_bridge) { parent_bridge = p_bridge;}
        void bridge_parent_bridge_clear(void) { parent_bridge.clear();}
        bool bridge_is_parent_bridge_exists(void) { if (!parent_bridge.empty()) return true; else return false;}
        bool bridge_is_parent_bridge(std::string &p_bridge) {
            if (parent_bridge.compare(p_bridge) == 0)
                return true;
            else
                return false;
        }
        BASE_IF_BRIDGE_MODE_t bridge_mode_get(void) {return bridge_mode;}
        void bridge_mode_set(BASE_IF_BRIDGE_MODE_t mode) { bridge_mode  = mode;}
        virtual t_std_error nas_bridge_set_learning_disable(bool disable) =0;
        bool nas_bridge_get_learning_disable() const{
            return learning_disable;
        }
        void nas_bridge_publish_event(cps_api_operation_types_t op);
        void nas_bridge_publish_member_event(std::string &mem_name, cps_api_operation_types_t op);
        void nas_bridge_publish_memberlist_event(memberlist_t &memlist, cps_api_operation_types_t op);
        t_std_error nas_bridge_add_member_in_os(std::string & mem_name);
        t_std_error nas_bridge_remove_member_from_os(std::string & mem_name);
        t_std_error nas_bridge_set_attribute(cps_api_object_t obj,cps_api_object_it_t & it);
        t_std_error nas_bridge_os_add_remove_member(std::string & mem_name, nas_port_mode_t port_mode, bool add);
        t_std_error nas_bridge_os_add_remove_memberlist(memberlist_t & memlist, nas_port_mode_t port_mode, bool add);
        std::string get_bridge_mac(void) {return mac_addr;}
        void nas_bridge_os_set_mtu (void);
        void nas_bridge_set_mtu (void);
        void nas_bridge_set_mac_address(
                std::string &mac_addr_string) { mac_addr.assign(mac_addr_string); }

    protected:
        uint32_t nas_bridge_get_mtu(void) { return(mtu); }
        virtual t_std_error nas_bridge_set_mtu(cps_api_object_t obj, cps_api_object_it_t & it);
        t_std_error nas_bridge_set_admin_status(cps_api_object_t obj, cps_api_object_it_t & it);
        t_std_error nas_bridge_set_mac_address(cps_api_object_t obj, cps_api_object_it_t & it);
        void nas_bridge_com_set_learning_disable(bool disable);

    private:
        std::string               mac_addr;
        bool                      learning_disable;
        BASE_IF_MAC_LEARN_MODE_t  learning_mode;
        bool                      admin_status;
        uint32_t                  mtu;

        cps_api_return_code_t nas_bridge_fill_bridge_model_com_info(cps_api_object_t obj);
        cps_api_return_code_t nas_bridge_fill_vlan_intf_model_com_info(cps_api_object_t obj);
        bool get_bridge_enabled(void) { return admin_status; }
        uint32_t get_bridge_mtu(void) { return mtu; }

};

#endif /* _NAS_INTERFACE_BRIDGE_H */
