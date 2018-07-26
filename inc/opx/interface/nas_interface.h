
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
class NAS_INTERFACE {

        bool _create_in_os;
        BASE_IF_MAC_LEARN_MODE_t mac_learn_mode;
        std::unordered_set<std::string> sub_intf_list;
        /*
         * split horizon id is only to be used for physical and LAG interfaces and it only
         * takes effect for the 1d sub ports that are created out of the physical and LAG
         * interfaces
         */
        uint32_t       ingress_split_horizon_id;
        uint32_t       egress_split_horizon_id;
        using sub_intf_attr_fn  = std::function<t_std_error (const std::string & ,nas_com_id_value_t & val)>;
    public:
        std::string    if_name;
        std::string    mac_addr;
        hal_ifindex_t  if_index;
        size_t         mtu;
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

        }

        virtual ~NAS_INTERFACE(){}
        cps_api_return_code_t nas_interface_fill_info(cps_api_object_t obj);
        cps_api_return_code_t nas_interface_fill_com_info(cps_api_object_t obj);

        bool create_in_os() const {
            return _create_in_os;
        }

        void set_create_in_os(bool value){
            _create_in_os = value;
        }

        hal_ifindex_t get_ifindex() const {
            return if_index;
        }

        nas_int_type_t intf_type_get(void) { return if_type;}

        void set_mac_learn_mode(BASE_IF_MAC_LEARN_MODE_t mode){
            mac_learn_mode = mode;
        }

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
                if((rc = fn(intf,val)) != STD_ERR_OK) return rc;
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

};
#endif /* _NAS_INTERFACE_H */
