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
 * filename: nas_stats_virtual_ntwrk.cpp
 */



#include "dell-base-if.h"
#include "bridge-model.h"
#include "dell-interface.h"
#include "ietf-interfaces.h"
#include "interface_obj.h"
#include "hal_if_mapping.h"
#include "bridge/nas_interface_bridge_utils.h"

#include "cps_api_key.h"
#include "cps_api_object_key.h"
#include "nas_ndi_1d_bridge.h"
#include "cps_class_map.h"
#include "cps_api_operation.h"
#include "event_log.h"
#include "nas_stats.h"
#include "std_time_tools.h"

#include <time.h>
#include <vector>


static const auto bridge_stat_ids = new std::vector<ndi_stat_id_t> {
    BRIDGE_DOMAIN_BRIDGE_STATS_IN_PKTS,
    BRIDGE_DOMAIN_BRIDGE_STATS_OUT_PKTS,
    BRIDGE_DOMAIN_BRIDGE_STATS_IN_OCTETS,
    BRIDGE_DOMAIN_BRIDGE_STATS_OUT_OCTETS,
};

static bool bridge_domain_to_intf_stats(ndi_stat_id_t bridge_dom, ndi_stat_id_t &vn_stat) {
    static const auto br_dom2_vn_type_map =
             new std::map<ndi_stat_id_t, ndi_stat_id_t>
    {
        {BRIDGE_DOMAIN_BRIDGE_STATS_IN_PKTS, DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_IN_PKTS},
        {BRIDGE_DOMAIN_BRIDGE_STATS_OUT_PKTS, DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_OUT_PKTS},
        {BRIDGE_DOMAIN_BRIDGE_STATS_IN_OCTETS, IF_INTERFACES_STATE_INTERFACE_STATISTICS_IN_OCTETS},
        {BRIDGE_DOMAIN_BRIDGE_STATS_OUT_OCTETS, IF_INTERFACES_STATE_INTERFACE_STATISTICS_OUT_OCTETS},
    };
    auto it = br_dom2_vn_type_map->find(bridge_dom);
    if(it == br_dom2_vn_type_map->end()){
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Invalid get bridge domain stats to intf stats %d", bridge_dom);
        return false;
    }

    vn_stat = it->second;
    return true;
}

static cps_api_return_code_t _fill_virt_ntwk_intf_stats_get(cps_api_get_params_t *param, std::string _br_name) {

    interface_ctrl_t intf_ctrl;
    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    memcpy(intf_ctrl.if_name,_br_name.c_str(),sizeof(intf_ctrl.if_name));

    if (dn_hal_get_interface_info(&intf_ctrl) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Failed to find interface information for bridge %s",_br_name.c_str());
        return cps_api_ret_code_ERR;
    }

    if(intf_ctrl.int_type != nas_int_type_DOT1D_BRIDGE){
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Virtual Netwk :Bridge %s is not a dot1d bridge",_br_name.c_str());
        return cps_api_ret_code_ERR;
    }

    if (nas_bridge_utils_is_l3_bridge(_br_name) != STD_ERR_OK) {
        return cps_api_ret_code_ERR;
    }

    uint64_t stat_val[bridge_stat_ids->size()];

    if(ndi_bridge_1d_stats_get(intf_ctrl.npu_id,intf_ctrl.bridge_id,(ndi_stat_id_t *)&bridge_stat_ids->at(0),
                                stat_val,bridge_stat_ids->size()) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Failed to get bridge stat for %s",_br_name.c_str());
        return cps_api_ret_code_ERR;
    }

    cps_api_object_t _r_obj = cps_api_object_list_create_obj_and_append(param->list);
    if(_r_obj == nullptr){
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Failed to allocate object memory");
        return cps_api_ret_code_ERR;
    }

    for(unsigned int ix = 0 ; ix < bridge_stat_ids->size() ; ++ix ){
        ndi_stat_id_t vn_stat_id;
        if (bridge_domain_to_intf_stats(bridge_stat_ids->at(ix), vn_stat_id)) {
           cps_api_object_attr_add_u64(_r_obj, vn_stat_id, stat_val[ix]);
        } else {
            EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Failed to get corresponding vn stats for br_domain stat id %u",
                                                 bridge_stat_ids->at(ix));
        }
    }

    cps_api_object_attr_add_u32(_r_obj,DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_STATISTICS_TIME_STAMP,time(NULL));

    if(strlen(intf_ctrl.if_name) != 0)
        cps_api_object_attr_add(_r_obj, IF_INTERFACES_STATE_INTERFACE_NAME, intf_ctrl.if_name, strlen(intf_ctrl.if_name) + 1);

    uint64_t time_uptime = std_get_uptime(nullptr);
    uint64_t time_from_epoch = std_time_get_current_from_epoch_in_nanoseconds();

    cps_api_object_set_timestamp(_r_obj,time_from_epoch); // For  retrieving time from epoch
    cps_api_object_attr_add_u32(_r_obj, DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_STATISTICS_TIME_STAMP,
            (uint32_t)time_uptime); // Used for measuring time intervals

    return cps_api_ret_code_OK;
}
static cps_api_return_code_t _nas_virt_ntwk_stats_get (void * context, cps_api_get_params_t * param,
                                           size_t ix) {
    std::string _if_name;

    cps_api_object_t obj = cps_api_object_list_get(param->filters,ix);

    cps_api_object_attr_t _br_name_attr = cps_api_get_key_data(obj, IF_INTERFACES_STATE_INTERFACE_NAME);

    if(!_br_name_attr){

        std::for_each(nas_bridge_map_get().bmap.begin(),
                      nas_bridge_map_get().bmap.end(), [param, obj] (pair_t const& intf_obj) {
                         _fill_virt_ntwk_intf_stats_get(param, intf_obj.second->get_bridge_name());
                      });
        return cps_api_ret_code_OK;
    } else {
        _if_name.assign((char *)cps_api_object_attr_data_bin(_br_name_attr));
        return _fill_virt_ntwk_intf_stats_get(param, _if_name);
    }
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _nas_virt_ntwk_stats_clear(std::string _br_name) {

    interface_ctrl_t intf_ctrl;
    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    memcpy(intf_ctrl.if_name,_br_name.c_str(),sizeof(intf_ctrl.if_name));

    if (dn_hal_get_interface_info(&intf_ctrl) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Failed to find interface information for bridge %s",_br_name.c_str());
        return cps_api_ret_code_ERR;
    }

    if(intf_ctrl.int_type != nas_int_type_DOT1D_BRIDGE){
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Virtual Netwk :Bridge %s is not a dot1d bridge",_br_name.c_str());
        return cps_api_ret_code_ERR;
    }

    EV_LOGGING(INTERFACE,DEBUG, "NAS-STAT", "Virtual Netwk : dlete all stats for bridge %s", _br_name.c_str());
    if(ndi_bridge_1d_stats_clear(intf_ctrl.npu_id,intf_ctrl.bridge_id,(ndi_stat_id_t *)&bridge_stat_ids->at(0),
                                 bridge_stat_ids->size()) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Failed to clear bridge stat for %s",_br_name.c_str());
        return cps_api_ret_code_ERR;
    }
    return cps_api_ret_code_OK;
}
static cps_api_return_code_t _nas_virt_ntwk_stats_set (void * context, cps_api_transaction_params_t * param,
                                             size_t ix) {
    std::string _if_name;
    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    if (op != cps_api_oper_DELETE) {
        EV_LOGGING(INTERFACE,ERR,"NAS-VN-STAT","Invalid operation %d for clearing stat",op);
        return (cps_api_return_code_t)STD_ERR(INTERFACE,PARAM,0);
    }

    cps_api_object_attr_t _br_name_attr = cps_api_get_key_data(obj, IF_INTERFACES_STATE_INTERFACE_NAME);
    if(!_br_name_attr){
        std::for_each(nas_bridge_map_get().bmap.begin(),
                      nas_bridge_map_get().bmap.end(), [] (pair_t const& intf_obj){
                         _nas_virt_ntwk_stats_clear(intf_obj.second->get_bridge_name());
                      });

        return cps_api_ret_code_OK;
    } else {
        _if_name.assign((char *)cps_api_object_attr_data_bin(_br_name_attr));
        return _nas_virt_ntwk_stats_clear(_if_name);
    }
    return cps_api_ret_code_OK;
}


t_std_error nas_stats_virt_network_init(cps_api_operation_handle_t handle) {

    cps_api_registration_functions_t f;
    memset(&f,0,sizeof(f));

    char buff[CPS_API_KEY_STR_MAX];
    memset(buff,0,sizeof(buff));

     if (intf_obj_handler_registration(obj_INTF_STATISTICS, nas_int_type_DOT1D_BRIDGE,
                                    _nas_virt_ntwk_stats_get, _nas_virt_ntwk_stats_set) != STD_ERR_OK) {
        EV_LOG(ERR,INTERFACE, 0,"NAS-STAT", "Failed to register virtual network stats CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return STD_ERR_OK;
}
