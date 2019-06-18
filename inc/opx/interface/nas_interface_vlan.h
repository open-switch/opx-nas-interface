
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
 * filename: nas_interface_vlan.h
 */

#ifndef _NAS_INTERFACE_VLAN_H
#define _NAS_INTERFACE_VLAN_H

#include "dell-base-common.h"

#include "nas_interface.h"
#include "nas_interface_cps.h"
#include "ds_common_types.h"
#include "std_mutex_lock.h"

class NAS_VLAN_INTERFACE : public NAS_INTERFACE {

    // TODO move these to private
    public:
        hal_vlan_id_t   vlan_id;
        std::string     parent_intf_name;
        nas_bridge_id_t bridge_id;
        nas_int_type_t  parent_intf_type; /* PHYSICAL or LAG */
        lag_id_t        lag_id;  /*  In case of lag type  */

        /*  Imp: Do not store ndi port for physical since it can change in case of virtual interface */
        /*  Even storing parent interface index  is not safe since it can change if VRF changes  */
        NAS_VLAN_INTERFACE (std::string if_name,
                         hal_ifindex_t if_index,
                         hal_vlan_id_t vlan,
                         std::string parent_intf) : NAS_INTERFACE(if_name,
                                                                    if_index,
                                                                    nas_int_type_VLANSUB_INTF) {
                                                                        vlan_id = vlan;
                                                                        parent_intf_name = parent_intf;
                                                                    }

        t_std_error nas_interface_parent_info_get(interface_ctrl_t *parent_info);
        cps_api_return_code_t nas_interface_fill_info(cps_api_object_t obj);
        t_std_error update_os_mtu(void);
        std::string parent_name_get(void) {return parent_intf_name;}
        hal_vlan_id_t vlan_id_get(void) {return vlan_id;}
        void parent_type_set(nas_int_type_t parent_type) { parent_intf_type = parent_type;}
        nas_int_type_t parent_type_get(void) { return parent_intf_type;}

};

/*
 * This set/get default vlan APIs are to be used only for the VXLAN feature
 * goal is not do the dynamic changing of the default VLAN at run time.
 */

void nas_vlan_interface_set_default_vlan(hal_vlan_id_t vlan_id);

bool nas_vlan_interface_get_default_vlan(hal_vlan_id_t * vlan_id);

std_mutex_type_t * get_vlan_mutex();

#endif /* _NAS_INTERFACE_VLAN_H */
