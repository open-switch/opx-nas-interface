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
#include "bridge/nas_interface_bridge_utils.h"
#include "interface/nas_interface_utils.h"
#include "interface/nas_interface_cps.h"

#include "cps_class_map.h"
#include "cps_api_operation.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <mutex>

static bool _crt_vxlan_intf = true;
static std::mutex _m;
static hal_ifindex_t default_ifindex = 0;

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
        ndi_obj_id_t tunnel_id = (op == cps_api_oper_DELETE) ? 0 : remote_endpoint->tunnel_id;
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


static void _set_vxlan_create_inf(bool create){
    std::lock_guard<std::mutex> l(_m);
    _crt_vxlan_intf = create;
}

static inline bool _create_vxlan_intf(){
    std::lock_guard<std::mutex> l(_m);
    return _crt_vxlan_intf == true;
}

static bool _register_vxlan_intf(const std::string & name, const hal_ifindex_t & ifindex, bool add) {

    hal_intf_reg_op_type_t reg_op;

    interface_ctrl_t reg_block;
    memset(&reg_block,0,sizeof(reg_block));

    strncpy(reg_block.if_name,name.c_str(),sizeof(reg_block.if_name));
    reg_block.if_index = ifindex;
    reg_block.int_type = nas_int_type_VXLAN;
    reg_op = add ? HAL_INTF_OP_REG : HAL_INTF_OP_DEREG;
    EV_LOGGING(INTERFACE,INFO,"NAS-VXLAN", "%s vxlan interface %s with ifindex %d",add ?
                "Register" : "Deregister",reg_block.if_name,ifindex);

    if (dn_hal_if_register(reg_op,&reg_block) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-VXLAN",
            "interface register Error %s - mapping error or interface already present",reg_block.if_name);
        return false;
    }

    return true;
}

static cps_api_return_code_t _vxlan_create(cps_api_object_t obj){

    std_mutex_simple_lock_guard lock(get_vxlan_mutex());
    EV_LOGGING(INTERFACE,DEBUG,"VXLAN","Vxlan create");

    cps_api_object_attr_t name_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);
    cps_api_object_attr_t vni_attr = cps_api_object_attr_get(obj, DELL_IF_IF_INTERFACES_INTERFACE_VNI);
    cps_api_object_attr_t ip_attr = cps_api_object_attr_get(obj, DELL_IF_IF_INTERFACES_INTERFACE_SOURCE_IP_ADDR);
    cps_api_object_attr_t ip_family_attr = cps_api_object_attr_get(obj, DELL_IF_IF_INTERFACES_INTERFACE_SOURCE_IP_ADDR_FAMILY);

    if(!name_attr || !vni_attr || !ip_attr || !ip_family_attr) {
        EV_LOGGING(INTERFACE, ERR, "VXLAN","Missing VXLAN interface name/vni/ip for creating vxlan");
        return cps_api_ret_code_ERR;
    }

    std::string vxlan_name = std::string((const char *)cps_api_object_attr_data_bin(name_attr));
    NAS_VXLAN_INTERFACE * vxlan_obj = dynamic_cast<NAS_VXLAN_INTERFACE *>(nas_interface_map_obj_get(vxlan_name));

    if(vxlan_obj != nullptr){
        EV_LOGGING(INTERFACE,ERR,"VXLAN","Entry for vxlan interface %s already exist",vxlan_name.c_str());
        return cps_api_ret_code_ERR;
    }

    hal_ifindex_t ifindex = default_ifindex;

    if(_create_vxlan_intf()){
        if(nas_os_create_vxlan_interface(obj)!=STD_ERR_OK){
            EV_LOGGING(INTERFACE,ERR,"VXLAN","Failed creating vxlan interface in the os");
        }
        cps_api_object_attr_t ifindex_attr = cps_api_object_attr_get(obj,
                                        DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
        if(ifindex_attr){
            ifindex = cps_api_object_attr_data_uint(ifindex_attr);
        }
    }

    BASE_CMN_AF_TYPE_t af_type;
    hal_ip_addr_t local_ip;

    af_type = (BASE_CMN_AF_TYPE_t)cps_api_object_attr_data_u32(ip_family_attr);

    if(af_type == AF_INET) {
         struct in_addr *inp = (struct in_addr *) cps_api_object_attr_data_bin(ip_attr);
         std_ip_from_inet(&local_ip,inp);
    } else {
        struct in6_addr *inp6 = (struct in6_addr *) cps_api_object_attr_data_bin(ip_attr);
        std_ip_from_inet6(&local_ip,inp6);
    }
    BASE_CMN_VNI_t vni = (BASE_CMN_VNI_t ) cps_api_object_attr_data_u32(vni_attr);

    if (nas_interface_utils_vxlan_create(vxlan_name, ifindex, vni, local_ip,&vxlan_obj) != STD_ERR_OK) {
        /*
         * This shouldn't fail. If this fails think about rollback if the interface
         * was created in kernel
         */
        EV_LOGGING(INTERFACE,INFO,"NAS-INT","Failed to create vxlan cache entry");
        return cps_api_ret_code_ERR;
    }

    if(!_create_vxlan_intf()){
        vxlan_obj->set_create_in_os(false);
    }

    if(!_register_vxlan_intf(vxlan_name,ifindex,true)){
        return cps_api_ret_code_ERR;
    }

    nas_interface_cps_publish_event(vxlan_name,nas_int_type_VXLAN, cps_api_oper_CREATE);

    return cps_api_ret_code_OK;

}


static cps_api_return_code_t _vxlan_delete(cps_api_object_t obj){

    std::lock_guard<std::mutex> (get_vxlan_mutex());
    EV_LOGGING(INTERFACE,DEBUG,"VXLAN","Vxlan delete");

    cps_api_object_attr_t name_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);

    if(!name_attr) {
        EV_LOGGING(INTERFACE, ERR, "VXLAN","Missing VXLAN interface name for deleting vxlan");
        return cps_api_ret_code_ERR;
    }

    std::string vxlan_name = std::string((const char *)cps_api_object_attr_data_bin(name_attr));
    NAS_VXLAN_INTERFACE * vxlan_obj = dynamic_cast<NAS_VXLAN_INTERFACE *>(nas_interface_map_obj_get(vxlan_name));

    if(vxlan_obj == nullptr){
        EV_LOGGING(INTERFACE,ERR,"VXLAN","No entry for vxlan interface %s exist",vxlan_name.c_str());
        return cps_api_ret_code_ERR;
    }

    if(vxlan_obj->create_in_os()){
        if(nas_os_del_interface(vxlan_obj->get_ifindex())!=STD_ERR_OK){
            EV_LOGGING(INTERFACE,ERR,"VXLAN","Failed deleting vxlan interface in the os");
            return cps_api_ret_code_ERR;
        }
    }

    if (nas_interface_utils_vxlan_delete(vxlan_name) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,INFO,"NAS-INT","Failed to create vxlan cache entry");
        return cps_api_ret_code_ERR;
    }

    if(!_register_vxlan_intf(vxlan_name,vxlan_obj->get_ifindex(),false)){
        return cps_api_ret_code_ERR;
    }
    nas_interface_cps_publish_event(vxlan_name,nas_int_type_VXLAN, cps_api_oper_DELETE);

    return cps_api_ret_code_OK;
}


static t_std_error nas_add_mac_learn_mode_to_remote_endpt (const std::string &vxlan_intf_name, hal_ip_addr_t &remote_ip,
                   BASE_IF_MAC_LEARN_MODE_t &mac_mode)
{
    t_std_error rc = STD_ERR(INTERFACE, FAIL, 0);
    char buff[HAL_INET6_TEXT_LEN + 1];

    NAS_VXLAN_INTERFACE *vxlan_obj = (NAS_VXLAN_INTERFACE *)nas_interface_map_obj_get(vxlan_intf_name);
    if (vxlan_obj == nullptr) {
        EV_LOGGING(INTERFACE,ERR,"NAS-VXLAN", "nas_add_mac_learn_mode_to_remote_endpt failed %s",
          vxlan_intf_name.c_str());
        return rc;
    }

    remote_endpoint_t rem_endpt;
    memset(&rem_endpt,0, sizeof(remote_endpoint_t));
    rem_endpt.remote_ip = remote_ip;

    std_ip_to_string((const hal_ip_addr_t*) &rem_endpt.remote_ip, buff, HAL_INET6_TEXT_LEN);

    rc  = vxlan_obj->nas_interface_get_remote_endpoint(&rem_endpt);
    if (rc != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-VXLAN", "ADD mac_learn mode :no remote endpoint found for vxlan %s, ip %s", vxlan_intf_name.c_str(), buff);
        return STD_ERR(INTERFACE,FAIL,0);

    }

    rem_endpt.mac_learn_mode = mac_mode;
    /* Find remote endpt and set the mac_mode in its data structure. if it has a bridge associated set it in NDI also */
    rc  = vxlan_obj->nas_interface_set_mac_learn_remote_endpt(&rem_endpt);
    if (rc != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-VXLAN", "nas_add_mac_learn_mode_to_remote_endpt failed %s",
           vxlan_intf_name.c_str());
        return rc;

    }
    rc  = vxlan_obj->nas_interface_update_remote_endpoint(&rem_endpt);
    if (rc != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-VXLAN", "ADD mac_learn mode :Update remote endpt failed %s, ip %s", vxlan_intf_name.c_str(), buff);
        return STD_ERR(INTERFACE,FAIL,0);

    }

    return STD_ERR_OK;


}

static bool _nas_vxlan_handle_remote_endpoints(const std::string & vxlan_name,
                                                cps_api_object_t obj,const cps_api_object_it_t & it){

    cps_api_object_attr_t member_op_attr = cps_api_object_attr_get(obj,DELL_BASE_IF_CMN_SET_INTERFACE_INPUT_MEMBER_OP);


    size_t index = 0;

    cps_api_object_it_t it_lvl = it;
    cps_api_object_it_inside(&it_lvl);

    cps_api_attr_id_t ids[3] = {DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT,0,
                                  DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR_FAMILY };

    const size_t ids_len = sizeof(ids)/sizeof(ids[0]);

    std::list<remote_endpoint_t> _rem_ep_list;
    char buff[HAL_INET6_TEXT_LEN + 1];

    for ( ; cps_api_object_it_valid(&it_lvl) ; cps_api_object_it_next(&it_lvl)) {
        index =  cps_api_object_attr_id(it_lvl.attr);
        remote_endpoint_t cur_ep;
        ids[1] = index;
        ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR_FAMILY;
        cps_api_object_attr_t ip_family_attr = cps_api_object_e_get(obj,ids,ids_len);

        ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_ADDR;
        cps_api_object_attr_t ip_addr_attr = cps_api_object_e_get(obj,ids,ids_len);

        ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_MAC_LEARNING_MODE;
        cps_api_object_attr_t mac_lrn_mode_attr = cps_api_object_e_get(obj,ids,ids_len);

        ids[2] = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT_FLOODING_ENABLED;
        cps_api_object_attr_t flood_attr = cps_api_object_e_get(obj,ids,ids_len);

        if ((!ip_family_attr) || (!ip_addr_attr)){
            EV_LOGGING(INTERFACE, ERR , "NAS-VXLAN", "Required attr ip addr and addr family missing "
                                                                       "to add delete remote endpoint");
            continue;
        }


        cur_ep.remote_ip.af_index = cps_api_object_attr_data_u32(ip_family_attr);
        if(cur_ep.remote_ip.af_index == AF_INET){
            memcpy(&cur_ep.remote_ip.u.ipv4,cps_api_object_attr_data_bin(ip_addr_attr),
                    sizeof(cur_ep.remote_ip.u.ipv4));
        } else {
            memcpy(&cur_ep.remote_ip.u.ipv6,cps_api_object_attr_data_bin(ip_addr_attr),
                 sizeof(cur_ep.remote_ip.u.ipv4));
        }

        std_ip_to_string((const hal_ip_addr_t*) &cur_ep.remote_ip, buff, HAL_INET6_TEXT_LEN);
        EV_LOGGING(INTERFACE, DEBUG , "NAS-VXLAN", "VXLAN set read_remote_endpt read ip addr %s is_true set_mac_learn %d \n",
                               buff, mac_lrn_mode_attr ? true: false);

        if (mac_lrn_mode_attr) {
            cur_ep.mac_learn_mode = BASE_IF_MAC_LEARN_MODE_t (cps_api_object_attr_data_uint(mac_lrn_mode_attr));
            if (member_op_attr == nullptr) {
                /* Set MAC learn attribute in NPU. This is not remote endpoint add or del */
                if (nas_add_mac_learn_mode_to_remote_endpt(vxlan_name, cur_ep.remote_ip,  cur_ep.mac_learn_mode)
                   != STD_ERR_OK ) {
                    EV_LOGGING(INTERFACE, ERR , "NAS-VXLAN", "Failed to set remote end poit mac_learn_mode for IP %s vtep %s",
                         buff, vxlan_name.c_str());
                    return STD_ERR(INTERFACE,FAIL,0);
                }
            }
        }

        if(flood_attr){
            cur_ep.flooding_enabled = cps_api_object_attr_data_uint(flood_attr);
        }

        _rem_ep_list.push_back(cur_ep);
    }

    if(member_op_attr == nullptr) {
        return true;
    }

    DELL_BASE_IF_CMN_UPDATE_TYPE_t member_op = (DELL_BASE_IF_CMN_UPDATE_TYPE_t)
                                                    cps_api_object_attr_data_uint(member_op_attr);
    bool add = member_op == DELL_BASE_IF_CMN_UPDATE_TYPE_ADD ? true : false;

    for(auto it : _rem_ep_list){
        if(add){
           if(nas_bridge_utils_add_remote_endpoint(vxlan_name.c_str(),it) != STD_ERR_OK) return false;
        }else{
            if(nas_bridge_utils_remove_remote_endpoint(vxlan_name.c_str(),it) != STD_ERR_OK) return false;
        }
    }

    return true;

}

static cps_api_return_code_t _vxlan_set(cps_api_object_t obj){
    std::lock_guard<std::mutex> (get_vxlan_mutex());
    EV_LOGGING(INTERFACE,DEBUG,"NAS-VXLAN","Vxlan remote endpoint");

    cps_api_object_attr_t name_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);

    if(!name_attr) {
        EV_LOGGING(INTERFACE, ERR, "VXLAN","Missing VXLAN interface name for updating vxlan");
        return cps_api_ret_code_ERR;
    }

    std::string vxlan_name = std::string((const char *)cps_api_object_attr_data_bin(name_attr));
    NAS_VXLAN_INTERFACE * vxlan_obj = dynamic_cast<NAS_VXLAN_INTERFACE *>(nas_interface_map_obj_get(vxlan_name));

    if(vxlan_obj == nullptr){
        EV_LOGGING(INTERFACE,ERR,"VXLAN","No entry for vxlan interface %s exist",vxlan_name.c_str());
        return cps_api_ret_code_ERR;
    }

    cps_api_object_it_t it;
    cps_api_attr_id_t rem_endpoint_id = DELL_IF_IF_INTERFACES_INTERFACE_REMOTE_ENDPOINT;
    if(!cps_api_object_it(obj,&rem_endpoint_id,1,&it)){
        EV_LOGGING(INTERFACE,ERR,"NAS-VXLAN","No remote endpoint attribute found in the object");
        return cps_api_ret_code_ERR;
    }


    if(!_nas_vxlan_handle_remote_endpoints(vxlan_name,obj,it)){
        return cps_api_ret_code_ERR;
    }

    return cps_api_ret_code_OK;
}

cps_api_return_code_t nas_vxlan_add_cps_attr_for_interface(cps_api_object_t obj) {


    std::lock_guard<std::mutex> (get_vxlan_mutex());
    EV_LOGGING(INTERFACE,DEBUG,"NAS-VXLAN","Vxlan  gel all remote endpoint");

    cps_api_object_attr_t name_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);

    if(!name_attr) {
        EV_LOGGING(INTERFACE, ERR, "VXLAN","Missing VXLAN interface name for adding all remote endpt vxlan");
        return cps_api_ret_code_ERR;
    }

    std::string vxlan_name = std::string((const char *)cps_api_object_attr_data_bin(name_attr));
    NAS_VXLAN_INTERFACE * vxlan_obj = dynamic_cast<NAS_VXLAN_INTERFACE *>(nas_interface_map_obj_get(vxlan_name));

    if(vxlan_obj == nullptr){
        EV_LOGGING(INTERFACE,ERR,"VXLAN","No entry for vxlan interface %s exist",vxlan_name.c_str());
        return cps_api_ret_code_ERR;
    }
    vxlan_obj->nas_vxlan_add_attr_for_interface_obj(obj);
    return cps_api_ret_code_OK;

}

using call_bk = std::function<cps_api_return_code_t (cps_api_object_t obj)>;

static cps_api_return_code_t _vxlan_set_handler (void * context, cps_api_transaction_params_t * param,
                                           size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));


    static std::unordered_map<size_t, call_bk> cps_cb_map =
    {
        { cps_api_oper_DELETE,_vxlan_delete},
        { cps_api_oper_CREATE,_vxlan_create},
        { cps_api_oper_SET, _vxlan_set},
    };

    auto it = cps_cb_map.find(op);
    if(it != cps_cb_map.end()){
        return it->second(obj);
    }

    return cps_api_ret_code_ERR;
}


static cps_api_return_code_t _vxlan_get_handler (void * context, cps_api_get_params_t * param,
                                           size_t ix) {
    return nas_interface_com_if_get(context, param, ix);
}


t_std_error nas_vxlan_init(cps_api_operation_handle_t handle) {

    if (intf_obj_handler_registration(obj_INTF, nas_int_type_VXLAN,
            _vxlan_get_handler, _vxlan_set_handler) != STD_ERR_OK) {
        EV_LOGGING (INTERFACE, ERR,"NAS-VXLAN-INIT", "Failed to register VXLAN CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }
    EV_LOGGING(INTERFACE,ERR,"NAS-VXLAN-INIT", "Registered the vxlan interface");
    _set_vxlan_create_inf(true);

    return STD_ERR_OK;
}
