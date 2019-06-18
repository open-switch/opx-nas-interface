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
 * filename: nas_stats_tunnel.cpp
 */



#include "dell-base-if.h"
#include "tunnel.h"
#include "dell-interface.h"
#include "ietf-interfaces.h"
#include "interface_obj.h"
#include "hal_if_mapping.h"
#include "bridge/nas_interface_bridge_utils.h"
#include "nas_ndi_common.h"

#include "cps_api_key.h"
#include "cps_api_object_key.h"
#include "nas_ndi_1d_bridge.h"
#include "cps_class_map.h"
#include "cps_api_operation.h"
#include "event_log.h"
#include "nas_stats.h"

#include <time.h>
#include <vector>

static const auto _tunnel_stat_ids = new std::vector<ndi_stat_id_t> {
    TUNNEL_TUNNEL_STATS_TUNNELS_IN_PKTS,
    TUNNEL_TUNNEL_STATS_TUNNELS_OUT_PKTS,
    TUNNEL_TUNNEL_STATS_TUNNELS_IN_OCTETS,
    TUNNEL_TUNNEL_STATS_TUNNELS_OUT_OCTETS
};

static void _get_ip_addr(hal_ip_addr_t & ip_addr, cps_api_object_attr_t & af_attr, cps_api_object_attr_t & ip_attr){
    ip_addr.af_index = cps_api_object_attr_data_uint(af_attr);
    if(ip_addr.af_index == AF_INET){
        memcpy(&ip_addr.u.ipv4,cps_api_object_attr_data_bin(ip_attr),sizeof(ip_addr.u.ipv4));
    }else{
        memcpy(&ip_addr.u.ipv6,cps_api_object_attr_data_bin(ip_attr),sizeof(ip_addr.u.ipv6));
    }
}

static cps_api_return_code_t _nas_tunnel_stat_get (void * context, cps_api_get_params_t * param,
                                           size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->filters,ix);

    cps_api_object_attr_t _rem_ip_af_attr = cps_api_get_key_data(obj,TUNNEL_TUNNEL_STATS_TUNNELS_REMOTE_IP_ADDR_FAMILY);
    cps_api_object_attr_t _rem_ip_attr = cps_api_get_key_data(obj,TUNNEL_TUNNEL_STATS_TUNNELS_REMOTE_IP_ADDR);
    cps_api_object_attr_t _loc_ip_af_attr = cps_api_get_key_data(obj,TUNNEL_TUNNEL_STATS_TUNNELS_LOCAL_IP_ADDR_FAMILY);
    cps_api_object_attr_t _loc_ip_attr = cps_api_get_key_data(obj,TUNNEL_TUNNEL_STATS_TUNNELS_LOCAL_IP_ADDR);

    if(!_rem_ip_af_attr || !_rem_ip_attr || !_loc_ip_af_attr || !_loc_ip_attr){
        EV_LOGGING(INTERFACE,ERR,"NAS-TUNNNEL-STAT","Missing key attribute for tunnel stats get");
        return cps_api_ret_code_ERR;
    }

    hal_ip_addr_t _rem_ip = {0}, _local_ip = {0};
    _get_ip_addr(_rem_ip,_rem_ip_af_attr,_rem_ip_attr);
    _get_ip_addr(_local_ip,_loc_ip_af_attr,_loc_ip_attr);

    uint64_t stat_val[_tunnel_stat_ids->size()];
    nas_com_id_value_t tunnel_params[2];
    tunnel_params[0].attr_id = TUNNEL_TUNNEL_STATE_TUNNELS_REMOTE_IP_ADDR;
    tunnel_params[0].val = &_rem_ip;
    tunnel_params[0].vlen  = sizeof(_rem_ip);
    tunnel_params[1].attr_id = TUNNEL_TUNNEL_STATE_TUNNELS_LOCAL_IP_ADDR;
    tunnel_params[1].val = &_local_ip;
    tunnel_params[1].vlen  = sizeof(_local_ip);

    const size_t _npu_id = 0;

    if(ndi_tunnel_stats_get(_npu_id,tunnel_params,sizeof(tunnel_params)/sizeof(tunnel_params[0]),
                            (ndi_stat_id_t *)&_tunnel_stat_ids->at(0),
                                stat_val,_tunnel_stat_ids->size()) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Failed to get tunnel stats");
        return cps_api_ret_code_ERR;
    }

    cps_api_object_t _r_obj = cps_api_object_list_create_obj_and_append(param->list);
    if(_r_obj == nullptr){
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Failed to allocate object memory");
        return cps_api_ret_code_ERR;
    }

    for(unsigned int ix = 0 ; ix < _tunnel_stat_ids->size() ; ++ix ){
        cps_api_object_attr_add_u64(_r_obj, _tunnel_stat_ids->at(ix), stat_val[ix]);
    }

    cps_api_object_attr_add_u32(_r_obj,DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_STATISTICS_TIME_STAMP,time(NULL));

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t nas_tunnel_stat_clear (void * context, cps_api_transaction_params_t * param,
                                                         size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    if (op != cps_api_oper_ACTION) {
        EV_LOGGING(INTERFACE,ERR,"NAS-TUNNEL-STAT","Invalid operation %d for clearing stat",op);
        return (cps_api_return_code_t)STD_ERR(INTERFACE,PARAM,0);
    }

    cps_api_object_attr_t _rem_ip_af_attr = cps_api_get_key_data(obj,TUNNEL_CLEAR_TUNNEL_STATS_INPUT_REMOTE_IP_ADDR_FAMILY);
    cps_api_object_attr_t _rem_ip_attr = cps_api_get_key_data(obj,TUNNEL_CLEAR_TUNNEL_STATS_INPUT_REMOTE_IP_ADDR);
    cps_api_object_attr_t _loc_ip_af_attr = cps_api_get_key_data(obj,TUNNEL_CLEAR_TUNNEL_STATS_INPUT_LOCAL_IP_ADDR_FAMILY);
    cps_api_object_attr_t _loc_ip_attr = cps_api_get_key_data(obj,TUNNEL_CLEAR_TUNNEL_STATS_INPUT_LOCAL_IP_ADDR);

    if(!_rem_ip_af_attr || !_rem_ip_attr || !_loc_ip_af_attr || !_loc_ip_attr){
        EV_LOGGING(INTERFACE,ERR,"NAS-TUNNNEL-STAT","Missing key attribute for tunnel stats clear");
        return cps_api_ret_code_ERR;
    }

    hal_ip_addr_t _rem_ip = {0}, _local_ip = {0};
    _get_ip_addr(_rem_ip,_rem_ip_af_attr,_rem_ip_attr);
    _get_ip_addr(_local_ip,_loc_ip_af_attr,_loc_ip_attr);

    nas_com_id_value_t tunnel_params[2];
    tunnel_params[0].attr_id = TUNNEL_CLEAR_TUNNEL_STATS_INPUT_REMOTE_IP_ADDR;
    tunnel_params[0].val = &_rem_ip;
    tunnel_params[0].vlen  = sizeof(_rem_ip);
    tunnel_params[1].attr_id = TUNNEL_CLEAR_TUNNEL_STATS_INPUT_LOCAL_IP_ADDR;
    tunnel_params[1].val = &_local_ip;
    tunnel_params[1].vlen  = sizeof(_local_ip);

    const size_t _npu_id = 0;

    if(ndi_tunnel_stats_clear(_npu_id,tunnel_params,sizeof(tunnel_params)/sizeof(tunnel_params[0]),
                            (ndi_stat_id_t *)&_tunnel_stat_ids->at(0),
                            _tunnel_stat_ids->size()) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-STAT","Failed to clear tunnel stats");
        return cps_api_ret_code_ERR;
    }

    return cps_api_ret_code_OK;
}


t_std_error nas_stats_tunnel_init(cps_api_operation_handle_t handle) {

    cps_api_registration_functions_t f;
    memset(&f,0,sizeof(f));

    char buff[CPS_API_KEY_STR_MAX];
    memset(buff,0,sizeof(buff));

    if (!cps_api_key_from_attr_with_qual(&f.key,TUNNEL_TUNNEL_STATS_OBJ,
                                         cps_api_qualifier_OBSERVED)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-STATS","Could not translate %d to key %s",
               (int)(TUNNEL_TUNNEL_STATS_OBJ),
               cps_api_key_print(&f.key,buff,sizeof(buff)-1));
        return STD_ERR(INTERFACE,FAIL,0);
    }

    f.handle = handle;
    f._read_function = _nas_tunnel_stat_get;

    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
        return STD_ERR(INTERFACE,FAIL,0);
    }

    memset(&f,0,sizeof(f));
    memset(buff,0,sizeof(buff));

    if (!cps_api_key_from_attr_with_qual(&f.key,TUNNEL_CLEAR_TUNNEL_STATS_OBJ,
                                         cps_api_qualifier_TARGET)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-STATS","Could not translate %d to key %s",
                   (int)(TUNNEL_CLEAR_TUNNEL_STATS_OBJ),
                   cps_api_key_print(&f.key,buff,sizeof(buff)-1));
        return STD_ERR(INTERFACE,FAIL,0);
    }

    f.handle = handle;
    f._write_function = nas_tunnel_stat_clear;

    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return STD_ERR_OK;
}
