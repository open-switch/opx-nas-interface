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
 * filename: nas_stats_if_cps.cpp
 */



#include "dell-base-if.h"
#include "dell-interface.h"
#include "ietf-interfaces.h"
#include "interface_obj.h"
#include "hal_if_mapping.h"

#include "cps_api_key.h"
#include "cps_api_object_key.h"
#include "cps_class_map.h"
#include "cps_api_operation.h"
#include "event_log.h"
#include "nas_ndi_plat_stat.h"
#include "nas_stats.h"
#include "nas_fc_stats.h"
#include "nas_ndi_port.h"
#include "nas_int_utils.h"
#include "std_utils.h"
#include "vrf-mgmt.h"
#include "dell-interface.h"
#include "ietf-interfaces.h"
#include "nas_os_interface.h"
#include "std_time_tools.h"

#include <time.h>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <stdint.h>
#include <algorithm>

#define LPBK_STATS_PATH "/proc/net/dev"
#define ARRAY_SIZE(a)   (sizeof(a)/sizeof((a)[0]))
#define BUF_SIZE         1024
static auto if_stat_ids = new std::vector<ndi_stat_id_t>;


static const struct {
    char     name[32];
    uint64_t oid;
    uint_t   index;
} stats_map[] = {
    {"input_packets", DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_IN_PKTS, 1},
    {"input_bytes", IF_INTERFACES_STATE_INTERFACE_STATISTICS_IN_OCTETS, 0},
    {"input_multicast", IF_INTERFACES_STATE_INTERFACE_STATISTICS_IN_MULTICAST_PKTS, 7},
    {"input_errors", IF_INTERFACES_STATE_INTERFACE_STATISTICS_IN_ERRORS, 2},
    {"input_discards", IF_INTERFACES_STATE_INTERFACE_STATISTICS_IN_DISCARDS, 3},
    {"output_packets", DELL_IF_IF_INTERFACES_STATE_INTERFACE_STATISTICS_OUT_PKTS, 9},
    {"output_bytes", IF_INTERFACES_STATE_INTERFACE_STATISTICS_OUT_OCTETS, 8},
    {"output_errors", IF_INTERFACES_STATE_INTERFACE_STATISTICS_OUT_ERRORS, 10},
    {"output_discards",IF_INTERFACES_STATE_INTERFACE_STATISTICS_OUT_DISCARDS, 11}
};

bool
nas_stat_get_name_from_obj(cps_api_object_t obj, char *if_name, size_t name_sz) {

    hal_ifindex_t index;

    cps_api_object_attr_t if_name_attr = cps_api_get_key_data(obj, IF_INTERFACES_STATE_INTERFACE_NAME);
    cps_api_object_attr_t if_index_attr = cps_api_object_attr_get(obj,IF_INTERFACES_STATE_INTERFACE_IF_INDEX );
    if (if_index_attr == NULL && if_name_attr == NULL) {
        EV_LOGGING (NAS_INT_STATS, ERR, "NAS-STAT" ,"Get intf name: missing Name/ifindex attribute ");
        return false;
    }
    if (if_name_attr) {
       safestrncpy(if_name,(const char *)cps_api_object_attr_data_bin(if_name_attr), name_sz);
    } else {
        index = (hal_ifindex_t) cps_api_object_attr_data_u32(if_index_attr);
        if (nas_int_get_if_index_to_name(index, if_name, name_sz) != STD_ERR_OK) {
            return false;
        }
    }
    return true;
}

bool nas_stat_get_ifindex_from_obj(cps_api_object_t obj,hal_ifindex_t *index, bool clear){
    cps_api_attr_id_t attr_id;

    if (clear) {
        attr_id = DELL_IF_CLEAR_COUNTERS_INPUT_INTF_CHOICE_IFNAME_CASE_IFNAME;
    } else {
        attr_id = IF_INTERFACES_STATE_INTERFACE_NAME;
    }

    cps_api_object_attr_t if_name_attr = cps_api_get_key_data(obj,attr_id);

    if (clear) {
        attr_id = DELL_IF_CLEAR_COUNTERS_INPUT_INTF_CHOICE_IFINDEX_CASE_IFINDEX;
    } else {
        attr_id = DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX;
    }
    cps_api_object_attr_t if_index_attr = cps_api_object_attr_get(obj, attr_id);

    // Since interfaces state object is used to get stats, ifindex can also be searched in the ifindex attribute from interfaces state object
    if(if_index_attr == NULL)
        if_index_attr = cps_api_object_attr_get(obj, IF_INTERFACES_STATE_INTERFACE_IF_INDEX);

    if(if_index_attr == NULL && if_name_attr == NULL) {
        EV_LOG(ERR, INTERFACE, ev_log_s_CRITICAL, "NAS-STAT",
            "Missing Name/ifindex attribute for STAT Get");
        return false;
    }

    if(if_index_attr){
        *index = (hal_ifindex_t) cps_api_object_attr_data_u32(if_index_attr);
    }else{
        const char * name = (const char *)cps_api_object_attr_data_bin(if_name_attr);
        interface_ctrl_t i;
        memset(&i,0,sizeof(i));
        strncpy(i.if_name,name,sizeof(i.if_name)-1);
        i.q_type = HAL_INTF_INFO_FROM_IF_NAME;
        if (dn_hal_get_interface_info(&i)!=STD_ERR_OK){
            EV_LOG(ERR, INTERFACE, 0, "NAS-STAT",
                    "Can't get interface control information for %s",name);
            return false;
        }
        *index = i.if_index;
    }
    return true;
}

static t_std_error populate_if_stat_ids(){

    unsigned int max_if_stat_id;
    if(get_stat_ids_len(NAS_STAT_IF,&max_if_stat_id ) != STD_ERR_OK){
        EV_LOG(ERR,INTERFACE, 0,"NAS-STAT", "Failed to get max length of supported stat ids");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    ndi_stat_id_t ids_list[max_if_stat_id];

    memset(ids_list,0,sizeof(ids_list));
    if(port_stat_list_get(ids_list, &max_if_stat_id) != STD_ERR_OK) {
        return STD_ERR(INTERFACE,FAIL,0);
    }

    for(unsigned int ix = 0 ; ix < max_if_stat_id ; ++ix ){
        if_stat_ids->push_back(ids_list[ix]);
    }

    return STD_ERR_OK;
}

static bool get_stats(hal_ifindex_t ifindex, cps_api_object_list_t list){

    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(list);

    if (obj == NULL) {
        EV_LOG(ERR,INTERFACE, 0,"NAS-STAT", "Failed to create/append new object to list");
        return false;
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

    const size_t max_port_stat_id = if_stat_ids->size();
    uint64_t stat_values[max_port_stat_id];
    memset(stat_values,0,sizeof(stat_values));

    if(ndi_port_stats_get(intf_ctrl.npu_id, intf_ctrl.port_id,
                          (ndi_stat_id_t *)&(if_stat_ids->at(0)),
                          stat_values,max_port_stat_id) != STD_ERR_OK) {
        return false;
    }

    for(unsigned int ix = 0 ; ix < max_port_stat_id ; ++ix ){
        cps_api_object_attr_add_u64(obj, if_stat_ids->at(ix), stat_values[ix]);
    }

    uint64_t time_uptime = std_get_uptime(nullptr);
    uint64_t time_from_epoch = std_time_get_current_from_epoch_in_nanoseconds();
    cps_api_object_set_timestamp(obj,time_from_epoch);
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_STATISTICS_TIME_STAMP,
    		(uint32_t)time_uptime); // Used for measuring time intervals and rate calculations
    cps_api_object_attr_add_u32(obj,IF_INTERFACES_STATE_INTERFACE_IF_INDEX, ifindex);
    if (strlen(intf_ctrl.if_name) != 0)
        cps_api_object_attr_add(obj, IF_INTERFACES_STATE_INTERFACE_NAME, intf_ctrl.if_name, strlen(intf_ctrl.if_name) + 1);

    return true;
}

static bool fill_cps_stats (cps_api_object_t obj, char *ptr, const char *name, const char *ifname) {

    char *save_ptr, *p;
    uint_t cnt = 0;
    bool ret = true;
    uint64_t   stats_arr[16];
    memset(stats_arr, 0, sizeof(stats_arr));
    if ((p = strstr(ptr, name)) == NULL) {
        EV_LOGGING (NAS_INT_STATS, ERR ,"NAS-STAT", "lpbk :fill_cps_stats error");
        return false;
    }
    p = strtok_r(p, " ", &save_ptr);
    while (((p = strtok_r(NULL, " ", &save_ptr)) != NULL)
        && (cnt < ARRAY_SIZE(stats_arr))) {
        stats_arr[cnt++] = atoll(p);
    }
    for(unsigned int ix = 0 ; ix < ARRAY_SIZE(stats_map) ; ++ix ){
       cps_api_object_attr_add_u64(obj, stats_map[ix].oid, stats_arr[stats_map[ix].index]);
    }

    uint64_t time_uptime = std_get_uptime(nullptr);
    uint64_t time_from_epoch = std_time_get_current_from_epoch_in_nanoseconds();
    cps_api_object_set_timestamp(obj,time_from_epoch); // For  retrieving time from epoch
    cps_api_object_attr_add_u32(obj, DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_STATISTICS_TIME_STAMP,
    		(uint32_t)time_uptime); // Used for measuring time intervals
    cps_api_object_attr_add(obj, IF_INTERFACES_STATE_INTERFACE_NAME, ifname, strlen(ifname) + 1);
    return ret;
}

bool get_intf_stats_from_os( const char *name, cps_api_object_list_t list) {

    char stats_buf[BUF_SIZE];
    char intf_name[HAL_IF_NAME_SZ];
    FILE *fp = NULL;
    bool ret = false;

    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(list);

    if (obj == NULL) {
        EV_LOGGING (NAS_INT_STATS, ERR ,"NAS-STAT", "lpbk: Failed to create/append new object to list");
        return ret;
    }

    if ((fp = fopen(LPBK_STATS_PATH, "r")) == NULL) {
        return ret;
    }

    memset(stats_buf, 0, sizeof(stats_buf));

    /* space at start and ':' at end will uniquely identify an interface*/
    snprintf(intf_name, sizeof(intf_name), " %s%s", name, ":");

    stats_buf[0] = ' ';
    while (fgets(&stats_buf[1], sizeof(stats_buf) -1, fp) == &stats_buf[1]) {
        if (strstr(stats_buf, intf_name) != NULL) {
            ret = fill_cps_stats(obj, stats_buf, intf_name, name);
            break;
        }
    }
    fclose(fp);
    return ret;
}


static cps_api_return_code_t if_lpbk_stats_get (void * context, cps_api_get_params_t * param,
                                           size_t ix) {
    char name[HAL_IF_NAME_SZ];

    cps_api_object_t obj = cps_api_object_list_get(param->filters,ix);

    if (!nas_stat_get_name_from_obj(obj, name, sizeof(name))) {
        return (cps_api_return_code_t)STD_ERR(INTERFACE,CFG,0);
    }
    if (get_intf_stats_from_os((const char *)name, param->list)) return cps_api_ret_code_OK;

    return (cps_api_return_code_t)STD_ERR(INTERFACE,FAIL,0);

}

cps_api_return_code_t if_mgmt_stats_get (void * context, cps_api_get_params_t * param,
                                          size_t ix)
{
    char name[HAL_IF_NAME_SZ];
    cps_api_object_t filter = cps_api_object_list_get(param->filters,ix);
    cps_api_object_t obj = cps_api_object_list_create_obj_and_append(param->list);

    cps_api_object_clone(obj, filter);

    if (!nas_stat_get_name_from_obj(obj, name, sizeof(name))) {
        return (cps_api_return_code_t)STD_ERR(INTERFACE,CFG,0);
    }

     interface_ctrl_t intf_ctrl;
     memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));
     safestrncpy(intf_ctrl.if_name, name, sizeof(intf_ctrl.if_name));
     intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF_NAME;

     if ((dn_hal_get_interface_info(&intf_ctrl)) == STD_ERR_OK) {
         if (intf_ctrl.vrf_id != NAS_DEFAULT_VRF_ID) {
             cps_api_object_attr_add_u32(obj, VRF_MGMT_NI_IF_INTERFACES_INTERFACE_VRF_ID,
                     intf_ctrl.vrf_id);
         }
     }

     nas_os_get_interface_stats(name, obj);
     cps_api_object_attr_add(obj, IF_INTERFACES_STATE_INTERFACE_NAME, name, strlen(name) + 1);
     return cps_api_ret_code_OK;
}

static cps_api_return_code_t if_mgmt_stats_set (void * context, cps_api_transaction_params_t * param,                                                    size_t ix) {
     EV_LOGGING(INTERFACE,DEBUG,"NAS-MGMT-STAT","Set mgmt stats not supported");
     return cps_api_ret_code_ERR;
}

static cps_api_return_code_t if_stats_get (void * context, cps_api_get_params_t * param,
                                           size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->filters,ix);

    hal_ifindex_t ifindex=0;
    if(!nas_stat_get_ifindex_from_obj(obj,&ifindex,false)){
        return (cps_api_return_code_t)STD_ERR(INTERFACE,CFG,0);
    }


    if(get_stats(ifindex,param->list)) return cps_api_ret_code_OK;

    return (cps_api_return_code_t)STD_ERR(INTERFACE,FAIL,0);
}

static cps_api_return_code_t if_lpbk_stats_set (void * context, cps_api_transaction_params_t * param,                                                    size_t ix) {
     EV_LOGGING (NAS_INT_STATS, DEBUG,"NAS-STAT","Set loopback stats not supported");
     return cps_api_ret_code_ERR;
}

static cps_api_return_code_t if_stats_set (void * context, cps_api_transaction_params_t * param,
                                           size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);

    hal_ifindex_t ifindex=0;
    if(!nas_stat_get_ifindex_from_obj(obj,&ifindex,false)){
        return (cps_api_return_code_t)STD_ERR(INTERFACE,CFG,0);
    }

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    if( op == cps_api_oper_DELETE){
        std::vector<ndi_stat_id_t> del_stat_ids;

        interface_ctrl_t intf_ctrl;
        memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

        intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
        intf_ctrl.if_index = ifindex;

        if (dn_hal_get_interface_info(&intf_ctrl) != STD_ERR_OK) {
            EV_LOGGING (NAS_INT_STATS, DEBUG,"NAS-STAT","Interface %d has NO slot %d, port %d",
                    intf_ctrl.if_index, intf_ctrl.npu_id, intf_ctrl.port_id);
            return cps_api_ret_code_ERR;
        }

        cps_api_object_it_t it;
        cps_api_object_it_begin(obj, &it);

        for (; cps_api_object_it_valid(&it); cps_api_object_it_next(&it)) {

            ndi_stat_id_t id = (ndi_stat_id_t)cps_api_object_attr_id(it.attr);
            auto id_it = std::find(if_stat_ids->begin(),if_stat_ids->end(),id);
            if(id_it != if_stat_ids->end()){
                del_stat_ids.push_back(id);
            }
        }

        if(ndi_port_stats_clear(intf_ctrl.npu_id, intf_ctrl.port_id,
                                (ndi_stat_id_t *)&del_stat_ids[0],
                              del_stat_ids.size()) != STD_ERR_OK) {
            return cps_api_ret_code_ERR;;
        }
    }

    return cps_api_ret_code_OK;
}


static cps_api_return_code_t if_stats_clear (void * context, cps_api_transaction_params_t * param,
                                             size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    if (op != cps_api_oper_ACTION) {
        EV_LOG(ERR,INTERFACE,0,"NAS-STAT","Invalid operation %d for clearing stat",op);
        return (cps_api_return_code_t)STD_ERR(INTERFACE,PARAM,0);
    }

    hal_ifindex_t ifindex=0;
    if(!nas_stat_get_ifindex_from_obj(obj,&ifindex,true)){
        return (cps_api_return_code_t)STD_ERR(INTERFACE,CFG,0);
    }

    interface_ctrl_t intf_ctrl;

    memset(&intf_ctrl, 0, sizeof(interface_ctrl_t));

    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
    intf_ctrl.if_index = ifindex;

    if (dn_hal_get_interface_info(&intf_ctrl) != STD_ERR_OK) {
        EV_LOG(ERR,INTERFACE,0,"NAS-STAT","Interface %d has NO slot %d, port %d",
               intf_ctrl.if_index, intf_ctrl.npu_id, intf_ctrl.port_id);
        return (cps_api_return_code_t)STD_ERR(INTERFACE,FAIL,0);
    }

    if (intf_ctrl.int_type == nas_int_type_FC)
    {
        EV_LOGGING (NAS_INT_STATS, DEBUG,"NAS-FC-STAT","clearing FC stat");
        return (nas_if_fc_stats_clear(context, param, ix));
    }else if(intf_ctrl.int_type == nas_int_type_VLANSUB_INTF){
        return nas_vlan_sub_intf_stat_clear(obj);
    }
    if(ndi_port_clear_all_stat(intf_ctrl.npu_id,intf_ctrl.port_id) != STD_ERR_OK) {
        return (cps_api_return_code_t)STD_ERR(INTERFACE,FAIL,0);
    }

    return cps_api_ret_code_OK;
}


t_std_error nas_stats_if_init(cps_api_operation_handle_t handle) {

    if (intf_obj_handler_registration(obj_INTF_STATISTICS, nas_int_type_PORT,
                                      if_stats_get, if_stats_set) != STD_ERR_OK) {
        EV_LOGGING (NAS_INT_STATS, ERR,"NAS-STATS-INIT", "Failed to register interface stats CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }
    if (intf_obj_handler_registration(obj_INTF_STATISTICS, nas_int_type_LPBK,
                                      if_lpbk_stats_get, if_lpbk_stats_set) != STD_ERR_OK) {
        EV_LOGGING (NAS_INT_STATS, ERR ,"NAS-STATS-INIT", "Failed to register loopback stats CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);

    }
    if (intf_obj_handler_registration(obj_INTF_STATISTICS, nas_int_type_MGMT,
                                      if_mgmt_stats_get, if_mgmt_stats_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR ,"NAS-MGMT-INTF-STATS", "Failed to register mgmt stats CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }
    cps_api_registration_functions_t f;
    memset(&f,0,sizeof(f));

    char buff[CPS_API_KEY_STR_MAX];
    memset(buff,0,sizeof(buff));

    if (!cps_api_key_from_attr_with_qual(&f.key,
                DELL_BASE_IF_CMN_DELL_IF_CLEAR_COUNTERS_INPUT_OBJ,
                cps_api_qualifier_TARGET)) {
        EV_LOG(ERR,INTERFACE,0,"NAS-STATS","Could not translate %d to key %s",
               (int)(DELL_BASE_IF_CMN_DELL_IF_CLEAR_COUNTERS_INPUT_OBJ),
               cps_api_key_print(&f.key,buff,sizeof(buff)-1));
        return STD_ERR(INTERFACE,FAIL,0);
    }

    f.handle = handle;
    f._write_function = if_stats_clear;

    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (populate_if_stat_ids() != STD_ERR_OK){
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return STD_ERR_OK;
}
