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
 *
 * filename: nas_interface_vxlan.cpp
 */



#include "interface/nas_interface_vxlan.h"
#include "dell-base-common.h"
#include "dell-interface.h"
#include "cps_class_map.h"
#include "cps_api_object_key.h"
#include "dell-base-if.h"
#include "ds_common_types.h"
#include "event_log_types.h"
#include "nas_ndi_1d_bridge.h"
#include "std_ip_utils.h"
#include "event_log.h"
#include "dell-base-if.h"
#include "ietf-interfaces.h"
#include "interface_obj.h"
#include "hal_if_mapping.h"
#include "nas_os_vxlan.h"
#include "nas_os_interface.h"

#include "cps_class_map.h"
#include "cps_api_operation.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <mutex>

static std::mutex _m;

static std_mutex_lock_create_static_init_fast(vxlan_mutex);

std_mutex_type_t * get_vxlan_mutex(){
    return &vxlan_mutex;
}

t_std_error NAS_VXLAN_INTERFACE::nas_interface_add_remote_endpoint(remote_endpoint_t *remote_endpoint)
{
    remote_endpoint_list.push_back(*remote_endpoint);
    return STD_ERR_OK;
}

/*  Remvoe remote endpoint from the vxlan object */
t_std_error NAS_VXLAN_INTERFACE::nas_interface_remove_remote_endpoint(remote_endpoint_t *remote_endpoint)
{
    for (auto it = remote_endpoint_list.begin(); it != remote_endpoint_list.end(); it++) {
        if (it->remote_ip == remote_endpoint->remote_ip) {
            *remote_endpoint = *it;
            remote_endpoint_list.erase(it);
            return STD_ERR_OK;
        }
    }
    return STD_ERR(INTERFACE, FAIL, 0);
}

/* Base on IP address  get remote endpoint info */
t_std_error NAS_VXLAN_INTERFACE::nas_interface_get_remote_endpoint(remote_endpoint_t *remote_endpoint)
{

    if (remote_endpoint == NULL) {
        return STD_ERR(INTERFACE,FAIL,0);
    }
    for (auto it = remote_endpoint_list.begin(); it != remote_endpoint_list.end(); it++) {
        if (it->remote_ip == remote_endpoint->remote_ip) {
            *remote_endpoint = *it;
            return STD_ERR_OK;
        }
    }
    return STD_ERR(INTERFACE, FAIL, 0);

}

/* This will replace  all the values to the value passed in remote_endpoint. */
t_std_error NAS_VXLAN_INTERFACE::nas_interface_update_remote_endpoint(remote_endpoint_t *remote_endpoint)
{
    if (remote_endpoint == NULL) {
        return STD_ERR(INTERFACE,FAIL,0);
    }

    for (auto it = remote_endpoint_list.begin(); it != remote_endpoint_list.end(); it++) {
        if (it->remote_ip == remote_endpoint->remote_ip) {
            *it = *remote_endpoint;
            return STD_ERR_OK;
        }
    }
    return STD_ERR(INTERFACE, FAIL, 0);
}


/*  Perform function for each of the remote endpoint inthe vxlan interface */
void NAS_VXLAN_INTERFACE::nas_interface_for_each_remote_endpoint(std::function <void (BASE_CMN_VNI_t, hal_ip_addr_t &, remote_endpoint_t &) > fn)
{
    if (remote_endpoint_list.empty()) {
        return;
    }
    hal_ip_addr_t _source_ip = source_ip;
    for (auto it = remote_endpoint_list.begin(); it != remote_endpoint_list.end(); ++it) {
        fn(vni, _source_ip, *it);
    }
}

t_std_error NAS_VXLAN_INTERFACE::nas_interface_set_mac_learn_remote_endpt(remote_endpoint_t *remote_endpoint) {

    if (remote_endpoint == NULL || remote_endpoint->tunnel_id == NAS_INVALID_TUNNEL_ID) {
        EV_LOGGING(INTERFACE,ERR,"NAS-VXLAN","Remote endpoint is null or has invalid tunnel"
                                 " Id to update mac learn mode");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if(ndi_tunport_mac_learn_mode_set(npu_id, remote_endpoint->tunnel_id,
                (BASE_IF_PHY_MAC_LEARN_MODE_t)remote_endpoint->mac_learn_mode) !=STD_ERR_OK){
        EV_LOGGING(INTERFACE,ERR,"NAS-VXLAN","Failed to update mac learn mode for remote endpoint");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    return STD_ERR_OK;
}

cps_api_return_code_t NAS_VXLAN_INTERFACE::nas_interface_fill_info(cps_api_object_t obj)
{
    if (nas_interface_fill_com_info(obj) != cps_api_ret_code_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF","Could not get common interface info");
        return cps_api_ret_code_ERR;
    }

    cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_VNI, vni);
    BASE_CMN_AF_TYPE_t af_type;
    if (source_ip.af_index == AF_INET) {
        af_type = BASE_CMN_AF_TYPE_INET;
        cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_SOURCE_IP_ADDR,
                                                    (void *)&(source_ip.u.v4_addr), sizeof(source_ip.u.v4_addr));
    } else {
        af_type = BASE_CMN_AF_TYPE_INET6;
        cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_SOURCE_IP_ADDR,
                                                    (void *)&(source_ip.u.v6_addr), sizeof(source_ip.u.v6_addr));
    }
    cps_api_object_attr_add_u32(obj,DELL_IF_IF_INTERFACES_INTERFACE_SOURCE_IP_ADDR_FAMILY, af_type);
    return cps_api_ret_code_OK;
}

void NAS_VXLAN_INTERFACE::nas_interface_publish_remote_endpoint_event(remote_endpoint_t *remote_endpoint, cps_api_operation_types_t op, bool tunnel_event)
{
    cps_api_object_guard og(cps_api_object_create());

    if (!cps_api_key_from_attr_with_qual(cps_api_object_key(og.get()),
                DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT,
                cps_api_qualifier_OBSERVED)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF"," remote end point event key conversion failure ");
        return;
    }

    cps_api_object_set_type_operation(cps_api_object_key(og.get()), op);
    cps_api_set_key_data(og.get(),IF_INTERFACES_INTERFACE_NAME,
                               cps_api_object_ATTR_T_BIN, if_name.c_str(), strlen(if_name.c_str())+1);
    char ietf_if_type[256];
    if (!nas_to_ietf_if_type_get(if_type, ietf_if_type, sizeof(ietf_if_type))) {
        EV_LOGGING(INTERFACE, ERR, "NAS-INT-GET", "Failed to get IETF interface type for type id %d",
                   if_type);
        return;
    }
    cps_api_object_attr_add(og.get(),IF_INTERFACES_INTERFACE_TYPE,
                                                (const void *)ietf_if_type, strlen(ietf_if_type)+1);


    cps_api_object_attr_add_u32(og.get(),DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_FLOODING_ENABLED,
                                 remote_endpoint->flooding_enabled);
    hal_ip_addr_t *ip = &(remote_endpoint->remote_ip);
    BASE_CMN_AF_TYPE_t af_type;
    if (ip->af_index == AF_INET) {
        af_type = BASE_CMN_AF_TYPE_INET;
        cps_api_object_attr_add(og.get(), DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR,
                                                    (void *)&(ip->u.v4_addr), sizeof(ip->u.v4_addr));
    } else {
        af_type = BASE_CMN_AF_TYPE_INET6;
        cps_api_object_attr_add(og.get(), DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR,
                                                    (void *)&(ip->u.v6_addr), sizeof(ip->u.v6_addr));
    }
    cps_api_object_attr_add_u32(og.get(),DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR_FAMILY, af_type);

    if (tunnel_event) {
        /*  Add tunnel ID  */
        ndi_obj_id_t tunnel_id = remote_endpoint->tunnel_id;
        cps_api_object_attr_add(og.get(), DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_TUNNEL_ID,
                                                    (void *)&tunnel_id, sizeof(tunnel_id));
    }
    /* Send remote Endpoint  event */
    cps_api_object_set_type_operation(cps_api_object_key(og.get()), op);
    cps_api_event_thread_publish(og.get());
    return;
}

void NAS_VXLAN_INTERFACE::nas_vxlan_add_attr_for_interface_obj(cps_api_object_t obj) {


    cps_api_object_attr_add_u32(obj, DELL_IF_IF_INTERFACES_INTERFACE_VNI, vni);
    BASE_CMN_AF_TYPE_t af_type;
    BASE_IF_MAC_LEARN_MODE_t mac_mode;

    if (source_ip.af_index == AF_INET) {
        af_type = BASE_CMN_AF_TYPE_INET;
        cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_SOURCE_IP_ADDR,
                                                    (void *)&(source_ip.u.v4_addr), sizeof(source_ip.u.v4_addr));
    } else {
        af_type = BASE_CMN_AF_TYPE_INET6;
        cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_SOURCE_IP_ADDR,
                                                    (void *)&(source_ip.u.v6_addr), sizeof(source_ip.u.v6_addr));
    }
    cps_api_object_attr_add_u32(obj,DELL_IF_IF_INTERFACES_INTERFACE_SOURCE_IP_ADDR_FAMILY, af_type);

    /* Iterate through each remote end pt and add its attribute */


    cps_api_attr_id_t ids[3] = {DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT,0,
                                DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR_FAMILY };

    const int ids_len = 3;
    uint32_t flood_enable;
    for (auto it = remote_endpoint_list.begin(); it != remote_endpoint_list.end(); it++) {

        ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR;

        if (it->remote_ip.af_index == AF_INET) {
            cps_api_object_e_add(obj, ids, ids_len, cps_api_object_ATTR_T_BIN,
                             (void *)&(it->remote_ip.u.v4_addr), sizeof(it->remote_ip.u.v4_addr));
            af_type = BASE_CMN_AF_TYPE_INET;
        }  else {
            cps_api_object_e_add(obj, ids, ids_len, cps_api_object_ATTR_T_BIN,
                             (void *)&(it->remote_ip.u.v6_addr), sizeof(it->remote_ip.u.v6_addr));

            af_type = BASE_CMN_AF_TYPE_INET6;

        }
        ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR_FAMILY;
        cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U32,&af_type,sizeof(af_type));
        mac_mode  = it->mac_learn_mode;
        ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_MAC_LEARNING_MODE;
        cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U32,&mac_mode,sizeof(mac_mode));
        flood_enable  = it->flooding_enabled;
        ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_FLOODING_ENABLED;
        cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U32,&flood_enable ,sizeof(flood_enable));

        ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_TUNNEL_ID;
        cps_api_object_e_add(obj, ids, ids_len, cps_api_object_ATTR_T_BIN,
                             (void *)&(it->tunnel_id), sizeof(it->tunnel_id));

        ++ids[1];
     }
}
