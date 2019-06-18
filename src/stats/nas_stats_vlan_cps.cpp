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
 * filename: nas_stats_vlan_cps.cpp
 */



#include "dell-base-if.h"
#include "dell-interface.h"
#include "ietf-interfaces.h"

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
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stdint.h>
#include <time.h>

static auto vlan_stat_ids = new std::vector<ndi_stat_id_t>;
static auto npu_ids = new std::unordered_set<npu_id_t>;



static t_std_error populate_vlan_stat_ids(){

    unsigned int max_vlan_stat_id;
    if(get_stat_ids_len(NAS_STAT_VLAN,&max_vlan_stat_id ) != STD_ERR_OK){
        EV_LOG(ERR,INTERFACE, 0,"NAS-STAT", "Failed to get max length of supported stat ids");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    ndi_stat_id_t ids_list[max_vlan_stat_id];

    memset(ids_list,0,sizeof(ids_list));

    if(vlan_stat_list_get(ids_list, &max_vlan_stat_id) != STD_ERR_OK) {
        return STD_ERR(INTERFACE,FAIL,0);
    }

    for(unsigned int ix = 0 ; ix < max_vlan_stat_id ; ++ix ){
        vlan_stat_ids->push_back(ids_list[ix]);
    }

    const nas_switches_t * switches = nas_switch_inventory();

    for (size_t ix = 0; ix < switches->number_of_switches; ++ix) {

        const nas_switch_detail_t * sd = nas_switch((nas_switch_id_t) ix);
        if (sd == NULL) {
            EV_LOG(ERR,INTERFACE, 0, "NAS-STAT","Switch Details Configuration file is erroneous");
            return STD_ERR(INTERFACE, PARAM, 0);
        }

        for (size_t sd_ix = 0; sd_ix < sd->number_of_npus; ++sd_ix) {
            npu_ids->insert(sd->npus[sd_ix]);
        }
    }
    return STD_ERR_OK;
}


static bool get_stats(hal_ifindex_t vlan_ifindex, cps_api_object_list_t list){

    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(list);

    if (obj == NULL) {
        EV_LOG(ERR,INTERFACE, 0,"NAS-STAT", "Failed to create/append new object to list");
        return false;
    }

    interface_ctrl_t intf_ctrl;
    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
    intf_ctrl.if_index = vlan_ifindex;

    if (dn_hal_get_interface_info(&intf_ctrl) != STD_ERR_OK) {
        EV_LOG(ERR,INTERFACE,0,"NAS-STAT","Interface %d has NO slot %d, port %d",
               intf_ctrl.if_index, intf_ctrl.npu_id, intf_ctrl.port_id);
        return false;
    }

    const size_t vlan_stat_id_len = vlan_stat_ids->size();
    uint64_t stat_values[vlan_stat_id_len];
    uint64_t total_stat_values[vlan_stat_id_len];
    memset(stat_values,0,sizeof(stat_values));
    memset(total_stat_values,0,sizeof(total_stat_values));


    for(auto it = npu_ids->begin(); it != npu_ids->end() ; ++it ){

        if(ndi_vlan_stats_get(*it, intf_ctrl.vlan_id,
                              (ndi_stat_id_t *)&(vlan_stat_ids->at(0)),
                              stat_values,vlan_stat_id_len) != STD_ERR_OK) {
            return false;
        }

        for(unsigned int ix = 0 ; ix < vlan_stat_id_len ; ++ix ){
            total_stat_values[ix] += stat_values[ix];
        }

        memset(stat_values,0,sizeof(stat_values));
    }

    for(unsigned int ix = 0 ; ix < vlan_stat_id_len ; ++ix ){
         cps_api_object_attr_add_u64(obj, vlan_stat_ids->at(ix), total_stat_values[ix]);
    }

    cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_STATISTICS_TIME_STAMP,time(NULL));
    cps_api_object_attr_add_u32(obj,IF_INTERFACES_STATE_INTERFACE_IF_INDEX, vlan_ifindex);
    if(strlen(intf_ctrl.if_name) != 0)
        cps_api_object_attr_add(obj, IF_INTERFACES_STATE_INTERFACE_NAME, intf_ctrl.if_name, strlen(intf_ctrl.if_name) + 1);

    return true;
}


static cps_api_return_code_t if_stats_get (void * context, cps_api_get_params_t * param,
                                           size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->filters,ix);
    hal_ifindex_t ifindex=0;

    if(!nas_stat_get_ifindex_from_obj(obj,&ifindex,false)){
        return (cps_api_return_code_t)STD_ERR(INTERFACE,CFG,0);
    }

    interface_ctrl_t intf_ctrl;
    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
    intf_ctrl.if_index = ifindex;

    if (dn_hal_get_interface_info(&intf_ctrl) != STD_ERR_OK) {
        EV_LOG(ERR,INTERFACE,0,"NAS-STAT","Interface %d has NO slot %d, port %d",
                intf_ctrl.if_index, intf_ctrl.npu_id, intf_ctrl.port_id);
        return false;
    }
    if ((intf_ctrl.int_type == nas_int_type_VLAN)
        && (intf_ctrl.int_sub_type == BASE_IF_VLAN_TYPE_MANAGEMENT)) {

        char name[HAL_IF_NAME_SZ];

        if (!nas_stat_get_name_from_obj(obj, name, sizeof(name))) {
            return (cps_api_return_code_t)STD_ERR(INTERFACE,CFG,0);
        }

        if (get_intf_stats_from_os((const char *)name, param->list)) {
            return cps_api_ret_code_OK;
        }
    } else {
        if(get_stats(ifindex,param->list)) return cps_api_ret_code_OK;
    }

    return (cps_api_return_code_t)STD_ERR(INTERFACE,FAIL,0);
}


static cps_api_return_code_t if_stats_set (void * context, cps_api_transaction_params_t * param,
                                           size_t ix) {

   return cps_api_ret_code_ERR;
}


t_std_error nas_stats_vlan_init(cps_api_operation_handle_t handle) {

    if (intf_obj_handler_registration(obj_INTF_STATISTICS, nas_int_type_VLAN,
                                    if_stats_get, if_stats_set) != STD_ERR_OK) {
        EV_LOG(ERR,INTERFACE, 0,"NAS-STAT", "Failed to register interface stats CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (populate_vlan_stat_ids() != STD_ERR_OK){
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return STD_ERR_OK;
}
