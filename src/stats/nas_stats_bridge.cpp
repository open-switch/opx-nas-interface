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
 * filename: nas_stats_bridge.cpp
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
#include <string>

static const auto bridge_stat_ids = new std::vector<ndi_stat_id_t> {
    BRIDGE_DOMAIN_BRIDGE_STATS_IN_PKTS,
    BRIDGE_DOMAIN_BRIDGE_STATS_OUT_PKTS,
    BRIDGE_DOMAIN_BRIDGE_STATS_IN_OCTETS,
    BRIDGE_DOMAIN_BRIDGE_STATS_OUT_OCTETS,
};

static cps_api_return_code_t _nas_bridge_stat_get_by_name (std::string _br_name, cps_api_get_params_t * param) {


    interface_ctrl_t intf_ctrl;
    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    memcpy(intf_ctrl.if_name, _br_name.c_str(), sizeof(intf_ctrl.if_name));

    if (dn_hal_get_interface_info(&intf_ctrl) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Failed to find interface information for bridge %s",_br_name.c_str());
        return cps_api_ret_code_ERR;
    }

    if(intf_ctrl.int_type != nas_int_type_DOT1D_BRIDGE){
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Bridge %s is not a dot1d bridge",_br_name.c_str());
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
    cps_api_key_from_attr_with_qual(cps_api_object_key(_r_obj),
                                    BRIDGE_DOMAIN_BRIDGE_STATS,
                                    cps_api_qualifier_REALTIME);

    cps_api_object_attr_add(_r_obj, BRIDGE_DOMAIN_BRIDGE_NAME, _br_name.c_str(), strlen(_br_name.c_str()) + 1);

    for(unsigned int ix = 0 ; ix < bridge_stat_ids->size() ; ++ix ){
        cps_api_object_attr_add_u64(_r_obj, bridge_stat_ids->at(ix), stat_val[ix]);
    }

    uint64_t time_uptime = std_get_uptime(nullptr);
    uint64_t time_from_epoch = std_time_get_current_from_epoch_in_nanoseconds();

    cps_api_object_set_timestamp(_r_obj,time_from_epoch); // For  retrieving time from epoch
    cps_api_object_attr_add_u32(_r_obj, DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_STATISTICS_TIME_STAMP,
    		(uint32_t)time_uptime); // Used for measuring time intervals

    return cps_api_ret_code_OK;
}


cps_api_return_code_t _nas_fill_all_bridge_stats_info(cps_api_get_params_t *param, model_type_t model)
{
    // TODO check if List pointer is required to be passed
    std::for_each(nas_bridge_map_get().bmap.begin(),
        nas_bridge_map_get().bmap.end(), [param, model] (pair_t const& br_obj) {

        if (br_obj.second->get_bridge_model() != model) return;

        _nas_bridge_stat_get_by_name(br_obj.second->get_bridge_name(), param);
    });
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _nas_bridge_stat_get (void * context, cps_api_get_params_t * param,
                                                   size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->filters,ix);

    cps_api_object_attr_t _br_name_attr = cps_api_get_key_data(obj,BRIDGE_DOMAIN_BRIDGE_NAME);
    std::string _br_name;

    if(!_br_name_attr){
        return _nas_fill_all_bridge_stats_info(param, BRIDGE_MODEL);
    }

    _br_name.assign((char *)cps_api_object_attr_data_bin(_br_name_attr));

    return _nas_bridge_stat_get_by_name(_br_name, param);

}

static cps_api_return_code_t nas_bridge_stat_clear (void * context, cps_api_transaction_params_t * param,
                                             size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    if (op != cps_api_oper_ACTION) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE-STAT","Invalid operation %d for clearing stat",op);
        return (cps_api_return_code_t)STD_ERR(INTERFACE,PARAM,0);
    }

    cps_api_object_attr_t _br_name_attr = cps_api_object_attr_get(obj,BRIDGE_DOMAIN_CLEAR_BRIDGE_STATS_INPUT_NAME);

    if(!_br_name_attr){
        return cps_api_ret_code_ERR;
    }

    const char * _br_name = (const char *)cps_api_object_attr_data_bin(_br_name_attr);


    interface_ctrl_t intf_ctrl;
    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    memcpy(intf_ctrl.if_name,_br_name,sizeof(intf_ctrl.if_name));

    if (dn_hal_get_interface_info(&intf_ctrl) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Failed to find interface information for bridge %s",_br_name);
        return cps_api_ret_code_ERR;
    }

    if(intf_ctrl.int_type != nas_int_type_DOT1D_BRIDGE){
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Bridge %s is not a dot1d bridge",_br_name);
        return cps_api_ret_code_ERR;
    }

    if(ndi_bridge_1d_stats_clear(intf_ctrl.npu_id,intf_ctrl.bridge_id,(ndi_stat_id_t *)&bridge_stat_ids->at(0),
                                 bridge_stat_ids->size()) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Failed to clear bridge stat for %s",_br_name);
        return cps_api_ret_code_ERR;
    }

    return cps_api_ret_code_OK;
}


t_std_error nas_stats_bridge_init(cps_api_operation_handle_t handle) {

    cps_api_registration_functions_t f;
    memset(&f,0,sizeof(f));

    char buff[CPS_API_KEY_STR_MAX];
    memset(buff,0,sizeof(buff));

    if (!cps_api_key_from_attr_with_qual(&f.key,BRIDGE_DOMAIN_BRIDGE_STATS,
                                         cps_api_qualifier_REALTIME)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-STATS","Could not translate %d to key %s",
               (int)(BRIDGE_DOMAIN_BRIDGE_STATS),
               cps_api_key_print(&f.key,buff,sizeof(buff)-1));
        return STD_ERR(INTERFACE,FAIL,0);
    }

    f.handle = handle;
    f._read_function = _nas_bridge_stat_get;

    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
        return STD_ERR(INTERFACE,FAIL,0);
    }

    memset(&f,0,sizeof(f));
    memset(buff,0,sizeof(buff));

    if (!cps_api_key_from_attr_with_qual(&f.key,BRIDGE_DOMAIN_CLEAR_BRIDGE_STATS_OBJ,
                                         cps_api_qualifier_REALTIME)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-STATS","Could not translate %d to key %s",
                    (int)(BRIDGE_DOMAIN_CLEAR_BRIDGE_STATS_OBJ),
                    cps_api_key_print(&f.key,buff,sizeof(buff)-1));
        return STD_ERR(INTERFACE,FAIL,0);
    }

    f.handle = handle;
    f._write_function = nas_bridge_stat_clear;

    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return STD_ERR_OK;
}
