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
 * filename: nas_stats_fc_if_cps.cpp
 */

#include "interface_obj.h"
#include "nas_stats.h"
#include "hal_if_mapping.h"
#include "dell-base-if.h"
#include "dell-interface.h"
#include "ietf-interfaces.h"
#include "event_log.h"
#include "cps_api_object_key.h"
#include "nas_ndi_common.h"
#include "nas_ndi_fc_stat.h"

#include "std_time_tools.h"

typedef uint64_t nas_fc_if_stat_id_t;

static const nas_fc_if_stat_id_t nas_fc_if_stat_ids [] = {
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_RX_BYTES,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_RX_FRAMES,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_RX_UCAST_PKTS,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_RX_BCAST_PKTS,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_RX_INVALID_CRC,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_RX_FRAME_TOO_LONG,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_RX_FRAME_TRUNCATED,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_RX_LINK_FAIL,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_RX_LOSS_SYNC,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_CLASS2_RX_GOOD_FRAMES,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_CLASS3_RX_GOOD_FRAMES,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_RX_BB_CREDIT0,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_RX_BB_CREDIT0_DROP,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_RX_PRIM_SEQ_ERR,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_RX_LIP_COUNT,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_TX_BYTES,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_TX_FRAMES,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_TX_UCAST_PKTS,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_TX_BCAST_PKTS,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_CLASS2_TX_FRAMES,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_CLASS3_TX_FRAMES,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_TX_BB_CREDIT0,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_TX_OVERSIZE_FRAMES,
    DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_TOTAL_ERRORS,
};

static size_t nas_stats_id_len_get()
{
   return (sizeof(nas_fc_if_stat_ids)/sizeof(nas_fc_if_stat_ids[0]));
}

static bool get_fc_stats(hal_ifindex_t ifindex, cps_api_object_list_t list){

    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(list);

    if (obj == NULL) {
        EV_LOGGING (NAS_INT_STATS,  ERR,"NAS-FC-STAT", "Failed to create/append new object to list");
        return false;
    }

    interface_ctrl_t intf_ctrl;
    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
    intf_ctrl.if_index = ifindex;

    if (dn_hal_get_interface_info(&intf_ctrl) != STD_ERR_OK) {
        EV_LOGGING (NAS_INT_STATS, ERR,"NAS-FC-STAT","Interface %d has NO slot %d, port %d",
               intf_ctrl.if_index, intf_ctrl.npu_id, intf_ctrl.port_id);
        return false;
    }

    const size_t max_port_stat_id = nas_stats_id_len_get();
    uint64_t nas_fc_stat_values[max_port_stat_id];
    memset(nas_fc_stat_values,0,sizeof(nas_fc_stat_values));

    if(ndi_port_fc_stats_get(intf_ctrl.npu_id, intf_ctrl.port_id,
                        (nas_fc_if_stat_id_t *)&nas_fc_if_stat_ids[0],
                        nas_fc_stat_values,max_port_stat_id) != STD_ERR_OK) {
        EV_LOGGING (NAS_INT_STATS, ERR,"NAS-FC-STAT","Unable to get FC stats for if index %d port %d",
                    intf_ctrl.if_index, intf_ctrl.port_id);
        return false;
    }

    for(unsigned int ix = 0 ; ix < max_port_stat_id ; ++ix ){
        cps_api_object_attr_add_u64(obj, nas_fc_if_stat_ids[ix], nas_fc_stat_values[ix]);
    }

    uint64_t time_uptime = std_get_uptime(nullptr);
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_STATISTICS_TIME_STAMP,
                                (uint32_t)time_uptime);
    cps_api_object_attr_add_u32(obj,IF_INTERFACES_STATE_INTERFACE_IF_INDEX, ifindex);
    if(strlen(intf_ctrl.if_name) != 0)
        cps_api_object_attr_add(obj, IF_INTERFACES_STATE_INTERFACE_NAME, intf_ctrl.if_name, strlen(intf_ctrl.if_name) + 1);


    return true;
}


static cps_api_return_code_t if_fc_stats_get (void * context, cps_api_get_params_t * param,
                                           size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->filters,ix);

    hal_ifindex_t ifindex=0;
    if(!nas_stat_get_ifindex_from_obj(obj,&ifindex,false)){
        return STD_ERR(INTERFACE,CFG,0);
    }

    if(get_fc_stats(ifindex,param->list)) return cps_api_ret_code_OK;

    return STD_ERR(INTERFACE,FAIL,0);
}


static cps_api_return_code_t if_fc_stats_set (void * context, cps_api_transaction_params_t * param,
                                           size_t ix) {

    EV_LOGGING (NAS_INT_STATS, DEBUG,"NAS-FC-STAT","Clear specific FC counter not supported");

    return cps_api_ret_code_OK;
}

extern "C" cps_api_return_code_t nas_if_fc_stats_clear (void * context, cps_api_transaction_params_t * param,
                                             size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    if (op != cps_api_oper_ACTION) {
        EV_LOGGING (NAS_INT_STATS, ERR,"NAS-FC-STAT","Invalid operation %d for clearing stat",op);
        return STD_ERR(INTERFACE,PARAM,0);
    }

    hal_ifindex_t ifindex=0;
    if(!nas_stat_get_ifindex_from_obj(obj,&ifindex,true)){
        return STD_ERR(INTERFACE,CFG,0);
    }

    interface_ctrl_t intf_ctrl;
    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
    intf_ctrl.if_index = ifindex;

    if (dn_hal_get_interface_info(&intf_ctrl) != STD_ERR_OK) {
        EV_LOGGING (NAS_INT_STATS, DEBUG,"NAS-FC-STAT","Interface %d has NO slot %d, port %d",
               intf_ctrl.if_index, intf_ctrl.npu_id, intf_ctrl.port_id);
        return STD_ERR(INTERFACE,FAIL,0);
    }

    const size_t max_port_stat_id = nas_stats_id_len_get();

    if(ndi_port_fc_stats_clear(intf_ctrl.npu_id,intf_ctrl.port_id,
                            (nas_fc_if_stat_id_t *)&nas_fc_if_stat_ids[0],
                            max_port_stat_id) != STD_ERR_OK) {
        EV_LOGGING (NAS_INT_STATS, ERR,"NAS-FC-STAT","Unable to clear FC stats for if index %d port %d",
                    intf_ctrl.if_index, intf_ctrl.port_id);
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return cps_api_ret_code_OK;
}

extern "C" t_std_error nas_stats_fc_if_init(cps_api_operation_handle_t handle) {

    if (intf_obj_handler_registration(obj_INTF_STATISTICS, nas_int_type_FC,
                                      if_fc_stats_get, if_fc_stats_set) != STD_ERR_OK) {
        EV_LOGGING (NAS_INT_STATS, DEBUG,"NAS-FC-STATS-INIT", "Failed to register FC interface stats CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return STD_ERR_OK;
}
