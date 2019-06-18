
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
 * filename: nas_interface_vxlan.h
 */

#ifndef _NAS_INTERFACE_VXLAN_H
#define _NAS_INTERFACE_VXLAN_H

#include "cps_api_operation.h"
#include "hal_interface_common.h"
#include "nas_interface.h"
#include "nas_interface_cps.h"
#include "ds_common_types.h"
#include "std_ip_utils.h"
#include "std_mutex_lock.h"

#include <list>
#include <functional>
#include <map>
#include <netinet/in.h>

#define NAS_INVALID_TUNNEL_ID ((ndi_obj_id_t ) ~0x0)

struct remote_endpoint_t {
    hal_ip_addr_t remote_ip;
    bool rem_membership; /*  will be set only if remote endpoint IP is explicitly added by vxlan
                                         interface object. */
    bool flooding_enabled;
    ndi_obj_id_t tunnel_id;
    BASE_IF_MAC_LEARN_MODE_t mac_learn_mode;

    remote_endpoint_t(){
        memset(&remote_ip,0,sizeof(remote_ip));
        rem_membership = false;
        flooding_enabled = true;
        tunnel_id = NAS_INVALID_TUNNEL_ID;
        mac_learn_mode = BASE_IF_MAC_LEARN_MODE_HW;
    };
};


typedef std::map<hal_ip_addr_t, remote_endpoint_t> remote_endpoint_map_t;
typedef std::pair<hal_ip_addr_t, remote_endpoint_t> remote_endpoint_pair_t;
typedef std::pair<remote_endpoint_map_t::iterator, bool> remote_endpoint_ret_t;

class NAS_VXLAN_INTERFACE : public NAS_INTERFACE {

    public:
        BASE_CMN_VNI_t               vni;
        hal_ip_addr_t                source_ip;
        uint64_t                     learning_mode;
        std::string                  bridge_name;
        std::list<remote_endpoint_t> remote_endpoint_list;

        NAS_VXLAN_INTERFACE(std::string if_name,
                         hal_ifindex_t if_index,
                         BASE_CMN_VNI_t _vni,
                         hal_ip_addr_t _source_ip) : NAS_INTERFACE(if_name,
                                                                  if_index,
                                                                  nas_int_type_VXLAN)
                                                                  {
                                                                      vni = _vni;
                                                                      source_ip = _source_ip;
                                                                  }
        t_std_error nas_interface_add_remote_endpoint(remote_endpoint_t *remote_endpoint);
        t_std_error nas_interface_remove_remote_endpoint(remote_endpoint_t *remote_endpoint);
        t_std_error nas_interface_get_remote_endpoint(remote_endpoint_t *remote_endpoint);
        t_std_error nas_interface_update_remote_endpoint(remote_endpoint_t *remote_endpoint);
        t_std_error nas_vxlan_os_enable_flooding(remote_endpoint_t & rem_ep, bool enable);
        t_std_error nas_vxlan_create_in_os();
        t_std_error nas_vxlan_del_in_os();
        /* creates 00 mac for all remote vteps if flooding is enabled */
        t_std_error nas_interface_update_all_rem_endpt_flooding();
        bool nas_vxlan_register_vxlan_intf(bool add);
        t_std_error nas_interface_set_mac_learn_remote_endpt(remote_endpoint_t *remote_endpoint);

        void nas_interface_for_each_remote_endpoint(std::function <void (BASE_CMN_VNI_t, hal_ip_addr_t &, remote_endpoint_t &) > fn);
        void nas_interface_publish_remote_endpoint_event(remote_endpoint_t *remote_endpoint, cps_api_operation_types_t op, bool tunnel_event);
        cps_api_return_code_t nas_interface_fill_info(cps_api_object_t obj);
        void nas_vxlan_add_attr_for_interface_obj(cps_api_object_t obj);
        std::string get_bridge_name(void) { return bridge_name;}
};

inline bool operator == (const hal_ip_addr_t& ip1, const hal_ip_addr_t& ip2) noexcept
{
    return ((std_ip_cmp_ip_addr (&ip1, &ip2) == 0)? true :false);
}

bool nas_interface_vxlan_exsist(const std::string & vxlan_name);

std_mutex_type_t * get_vxlan_mutex();

#endif /* _NAS_INTERFACE_VXLAN_H */
