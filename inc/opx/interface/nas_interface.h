
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
 * filename: nas_interface.h
 */

#ifndef _NAS_INTERFACE_H
#define _NAS_INTERFACE_H


#include "cps_api_operation.h"
#include "hal_if_mapping.h"
#include "event_log.h"
#include "event_log_types.h"
#include "nas_ndi_common.h"
#include <iostream>
#include <string>
#include <unordered_set>
#include <stdint.h>
#include <functional>

#define NAS_IF_INDEX_INVALID  ((hal_ifindex_t ) ~0)

/*
 * Currently this class is used for physical and LAG interfaces which needs to move out to
 * separate sub classes
 */

typedef enum {
    MEMBER_TYPE_NONE,
    MEMBER_TYPE_1Q,
    MEMBER_TYPE_1D,
}br_member_type_t;
class NAS_INTERFACE {
        bool _create_in_os; // true if vtep creted by linux cli and not cps

        BASE_IF_MAC_LEARN_MODE_t mac_learn_mode;
        std::unordered_set<std::string> sub_intf_list;
        /*
         * split horizon id  and untagged vlan id is only to be used for physical and LAG interfaces and it only
         * takes effect for the 1d sub ports that are created out of the physical and LAG
         * interfaces
         */
        uint32_t       ingress_split_horizon_id;
        uint32_t       egress_split_horizon_id;
        using sub_intf_attr_fn  = std::function<t_std_error (const std::string & ,nas_com_id_value_t & val)>;
        hal_vlan_id_t  untagged_vlan_id; /*  used only for 1d LAG and phy ports. Should be moved to the derived class */

        br_member_type_t br_mem_type;
        bool           enabled; /* Config attribute which need to be stored to handle vrf change */
        BASE_IF_SPEED_t          speed;   /* Config attribute speed */
        BASE_CMN_DUPLEX_TYPE_t   duplex;  /* Config attribute duplex */
        std::string     bridge_name;
        size_t         mtu;
    public:
        std::string    if_name;
        std::string    mac_addr;
        hal_ifindex_t  if_index;
        nas_int_type_t if_type;
        npu_id_t       npu_id;

        NAS_INTERFACE(std::string ifname,
                        hal_ifindex_t ifdx,
                        nas_int_type_t type) : if_name(ifname),
                                               if_index(ifdx),
                                               if_type(type) {
                        npu_id = 0;
                        _create_in_os = true;
                        mac_learn_mode =  BASE_IF_MAC_LEARN_MODE_HW;
                        ingress_split_horizon_id = 0 ;
                        egress_split_horizon_id = 0;
                        br_mem_type = MEMBER_TYPE_NONE;
                        untagged_vlan_id = 1;

        }

        virtual ~NAS_INTERFACE(){}
        virtual cps_api_return_code_t nas_interface_fill_info(cps_api_object_t obj);
        cps_api_return_code_t nas_interface_fill_com_info(cps_api_object_t obj);

        std::string get_bridge_name (void) {
            return bridge_name;
        }
        void set_bridge_name (std::string br_name) {
            bridge_name.assign(br_name);
        }
        void clear_bridge_name (void) {
            bridge_name.clear();
        }

        bool get_enabled (void) {
            return enabled;
        }
        void set_enabled(bool state) {
            enabled = state;
        }
        BASE_IF_SPEED_t get_speed (void) {
            return speed;
        }

        void set_speed (BASE_IF_SPEED_t sp) {
            speed = sp;
        }
        BASE_CMN_DUPLEX_TYPE_t get_duplex (void) {
            return duplex;
        }
        void set_duplex(BASE_CMN_DUPLEX_TYPE_t dp) {
            duplex = dp;
        }
        bool create_in_os() const {
            return _create_in_os;
        }

        void set_create_in_os(bool value){
            _create_in_os = value;
        }

        hal_ifindex_t get_ifindex() const {
            return if_index;
        }
        size_t get_mtu(void) const {
            return mtu;
        }
        void set_ifindex(hal_ifindex_t if_idx) {
            if_index = if_idx;
        }
        std::string get_ifname() const {
            return if_name;
        }


        nas_int_type_t intf_type_get(void) { return if_type;}

        br_member_type_t br_member_type_get(void) { return br_mem_type;}
        bool nas_is_1q_br_member(void) {return (br_mem_type == MEMBER_TYPE_1Q);}
        bool nas_is_1d_br_member(void) {return (br_mem_type == MEMBER_TYPE_1D);}

        void br_member_type_set(br_member_type_t mem_type) { br_mem_type = mem_type;}
        void nas_br_member_type_1d_set(void) { br_mem_type = MEMBER_TYPE_1D;}
        void nas_br_member_type_1q_set(void) { br_mem_type = MEMBER_TYPE_1Q;}
        void nas_br_member_type_none_set(void) { br_mem_type = MEMBER_TYPE_NONE;}


        void set_mac_learn_mode(BASE_IF_MAC_LEARN_MODE_t mode){
            mac_learn_mode = mode;
        }

        t_std_error set_mtu(size_t mtu);


        BASE_IF_MAC_LEARN_MODE_t get_mac_learn_mode() const {
            return mac_learn_mode;
        }

        void add_sub_intf(const std::string & mem_name){
            sub_intf_list.insert(mem_name);
        }

        void del_sub_intf(const std::string & mem_name){
            sub_intf_list.erase(mem_name);
        }

        bool valid_sub_intf(const std::string & mem_name) const {
            return (sub_intf_list.find(mem_name) != sub_intf_list.end());
        }

        t_std_error for_all_sub_intf(sub_intf_attr_fn fn, nas_com_id_value_t & val) const{
            t_std_error rc = STD_ERR_OK;
            for(auto intf : sub_intf_list){
                fn(intf,val);
            }
            return rc;
        }

        void set_ingress_split_horizon_id(uint32_t id){
            ingress_split_horizon_id = id;
        }

        void set_egress_split_horizon_id(uint32_t id){
            egress_split_horizon_id = id;
        }

        uint32_t get_ingress_split_horizon_id() const {
            return ingress_split_horizon_id;
        }

        uint32_t get_egress_split_horizon_id() const {
            return egress_split_horizon_id;
        }
        hal_vlan_id_t untagged_vlan_id_get(void) { return untagged_vlan_id;}

        void untagged_vlan_id_set(hal_vlan_id_t vlan_id) {  untagged_vlan_id = vlan_id;}

};
#endif /* _NAS_INTERFACE_H */
