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
 * filename: nas_stats_vxlan.cpp
 */



#include "dell-base-if.h"
#include "dell-interface.h"
#include "ietf-interfaces.h"
#include "std_ip_utils.h"
#include "hal_if_mapping.h"

#include "cps_api_key.h"
#include "cps_api_object_key.h"
#include "cps_class_map.h"
#include "cps_api_operation.h"
#include "event_log.h"
#include "nas_ndi_plat_stat.h"
#include "nas_stats.h"
#include "nas_ndi_vlan.h"
#include "ds_common_types.h"
#include "nas_switch.h"
#include "interface_obj.h"
#include "nas_ndi_1d_bridge.h"
#include "bridge/nas_interface_bridge_utils.h"

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stdint.h>
#include <time.h>

static const auto _vxlan_stat_ids = new std::vector<ndi_stat_id_t>  {
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_IN_PKTS,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_OUT_PKTS,
    IF_INTERFACES_STATE_INTERFACE_STATISTICS_IN_OCTETS,
    IF_INTERFACES_STATE_INTERFACE_STATISTICS_OUT_OCTETS,
};


static cps_api_return_code_t if_stats_get (void * context, cps_api_get_params_t * param,
                                           size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->filters,ix);

    cps_api_object_attr_t _if_name_attr  = cps_api_get_key_data(obj,IF_INTERFACES_STATE_INTERFACE_NAME);
    cps_api_object_attr_t _af_attr  = cps_api_object_attr_get(obj,DELL_IF_IF_INTERFACES_STATE_INTERFACE_REMOTE_ENDPOINT_ADDR_FAMILY);
    cps_api_object_attr_t _ip_attr  = cps_api_object_attr_get(obj,DELL_IF_IF_INTERFACES_STATE_INTERFACE_REMOTE_ENDPOINT_ADDR);
    if(!_if_name_attr || !_af_attr || !_ip_attr){
        EV_LOGGING(INTERFACE,ERR,"NAS-VXLAN-STATS","No interface name/ip address passed to query vxlan stats");
        return cps_api_ret_code_ERR;
    }

    const char * _vxlan_intf_name = (const char *)cps_api_object_attr_data_bin(_if_name_attr);
    hal_ip_addr_t _ip;
    _ip.af_index = cps_api_object_attr_data_uint(_af_attr);
    if(_ip.af_index == AF_INET) {
        struct in_addr *inp = (struct in_addr *) cps_api_object_attr_data_bin(_ip_attr);
        std_ip_from_inet(&_ip,inp);
    } else {
        struct in6_addr *inp6 = (struct in6_addr *) cps_api_object_attr_data_bin(_ip_attr);
        std_ip_from_inet6(&_ip,inp6);
    }
    uint64_t stat_val[_vxlan_stat_ids->size()];

    if(nas_bridge_utils_get_remote_endpoint_stats(_vxlan_intf_name,_ip,(ndi_stat_id_t *)&_vxlan_stat_ids->at(0),
                                    stat_val,_vxlan_stat_ids->size()) != STD_ERR_OK){
        EV_LOGGING(INTERFACE,ERR,"NAS-VXLAN-STAT","Failed to get stat for vxlan %s",_vxlan_intf_name);
        return cps_api_ret_code_ERR;
    }

    cps_api_object_t _r_obj = cps_api_object_list_create_obj_and_append(param->list);
    if(_r_obj == nullptr){
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Failed to allocate object memory");
        return cps_api_ret_code_ERR;
    }


    for(unsigned int ix = 0 ; ix < _vxlan_stat_ids->size() ; ++ix ){
        cps_api_object_attr_add_u64(_r_obj, _vxlan_stat_ids->at(ix), stat_val[ix]);
    }

        cps_api_object_attr_add_u32(_r_obj,DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_STATISTICS_TIME_STAMP,time(NULL));


    return (cps_api_return_code_t)STD_ERR(INTERFACE,FAIL,0);
}


static cps_api_return_code_t if_stats_set (void * context, cps_api_transaction_params_t * param,
                                           size_t ix) {

   return cps_api_ret_code_ERR;
}


t_std_error nas_stats_vxlan_init(cps_api_operation_handle_t handle) {

    if (intf_obj_handler_registration(obj_INTF_STATISTICS, nas_int_type_VXLAN,
                                    if_stats_get, if_stats_set) != STD_ERR_OK) {
        EV_LOG(ERR,INTERFACE, 0,"NAS-STAT", "Failed to register vlab sub interface stats CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return STD_ERR_OK;
}
