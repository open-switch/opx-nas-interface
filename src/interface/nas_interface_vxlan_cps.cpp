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
 *
 * filename: nas_interface_vxlan_cps.cpp
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

    hal_ifindex_t ifindex = NAS_IF_INDEX_INVALID;

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

    /* VTEP is not created in OS . It is just saved in the interface_map. It will be created in OS only
     * if it gets added to L3  VN bridge later.
     */
    if (nas_interface_utils_vxlan_create(vxlan_name, ifindex, vni, local_ip,&vxlan_obj) != STD_ERR_OK) {
        /*
         * This shouldn't fail. If this fails think about rollback if the interface
         * was created in kernel
         */
        EV_LOGGING(INTERFACE,INFO,"NAS-INT","Failed to create vxlan cache entry");
        return cps_api_ret_code_ERR;
    }
    vxlan_obj->set_create_in_os(false);

    nas_interface_cps_publish_event(vxlan_name,nas_int_type_VXLAN, cps_api_oper_CREATE);

    return cps_api_ret_code_OK;

}


static cps_api_return_code_t _vxlan_delete(cps_api_object_t obj){

    std_mutex_simple_lock_guard lock(get_vxlan_mutex());
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
    hal_ifindex_t ifindex =  vxlan_obj->get_ifindex();
    if(ifindex != NAS_IF_INDEX_INVALID){
       if (vxlan_obj->nas_vxlan_del_in_os() != STD_ERR_OK) {
           EV_LOGGING(INTERFACE,ERR,"VXLAN","Failed deleting vxlan interface in the os");
           return cps_api_ret_code_ERR;
       }
    }

    nas_interface_cps_publish_event(vxlan_name,nas_int_type_VXLAN, cps_api_oper_DELETE);
    if (nas_interface_utils_vxlan_delete(vxlan_name) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,INFO,"NAS-INT","Failed to delete vxlan cache entry");
        return cps_api_ret_code_ERR;
    }

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
    memset(static_cast<void *>(&rem_endpt),0, sizeof(remote_endpoint_t));
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

static t_std_error  _nas_vxlan_handle_remote_endpoints(const std::string & vxlan_name,
                                                cps_api_object_t obj,const cps_api_object_it_t & it){

    cps_api_object_attr_t member_op_attr = cps_api_object_attr_get(obj,DELL_BASE_IF_CMN_SET_INTERFACE_INPUT_MEMBER_OP);
    if(member_op_attr == nullptr) {
        return STD_ERR_OK;
    }
    DELL_BASE_IF_CMN_UPDATE_TYPE_t member_op = (DELL_BASE_IF_CMN_UPDATE_TYPE_t)
                                                    cps_api_object_attr_data_uint(member_op_attr);

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

        if (member_op == DELL_BASE_IF_CMN_UPDATE_TYPE_UPDATE) {
            NAS_VXLAN_INTERFACE *vxlan_obj = (NAS_VXLAN_INTERFACE *)nas_interface_map_obj_get(vxlan_name);
            if (vxlan_obj == nullptr) {
                EV_LOGGING(INTERFACE,ERR,"NAS-VXLAN", "Failed to get VXLAN object %s", vxlan_name.c_str());
                return STD_ERR(INTERFACE,FAIL,0);
            }
            if (vxlan_obj->nas_interface_get_remote_endpoint(&cur_ep) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE,DEBUG,"NAS-BRIDGE",
                        "Remove remote endpoint : remote IP not present vxlan_intf %s ", vxlan_name);
                return STD_ERR(INTERFACE,FAIL,0);
            }
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
        cur_ep.rem_membership = true;
        _rem_ep_list.push_back(cur_ep);
    }
    t_std_error rc = STD_ERR_OK;
    for(auto it : _rem_ep_list){
        if( member_op == DELL_BASE_IF_CMN_UPDATE_TYPE_ADD){
           if((rc = nas_bridge_utils_add_remote_endpoint(vxlan_name.c_str(),it)) != STD_ERR_OK) return  rc;
        }else if (member_op == DELL_BASE_IF_CMN_UPDATE_TYPE_REMOVE) {
            if((rc = nas_bridge_utils_remove_remote_endpoint(vxlan_name.c_str(),it)) != STD_ERR_OK) return rc;
        } else if (member_op == DELL_BASE_IF_CMN_UPDATE_TYPE_UPDATE){
            if((rc = nas_bridge_utils_update_remote_endpoint(vxlan_name.c_str(),it)) != STD_ERR_OK) return rc;
        }
    }

    return STD_ERR_OK;
}


static cps_api_return_code_t _vxlan_set(cps_api_object_t obj){
    std_mutex_simple_lock_guard lock(get_vxlan_mutex());
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


    if(_nas_vxlan_handle_remote_endpoints(vxlan_name,obj,it) != STD_ERR_OK){
        return cps_api_ret_code_ERR;
    }

    return cps_api_ret_code_OK;
}

cps_api_return_code_t nas_vxlan_add_cps_attr_for_interface(cps_api_object_t obj) {


    std_mutex_simple_lock_guard lock(get_vxlan_mutex());
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

t_std_error nas_vxlan_init(cps_api_operation_handle_t handle) {

    if (intf_obj_handler_registration(obj_INTF, nas_int_type_VXLAN,
            nas_interface_if_get_handler, _vxlan_set_handler) != STD_ERR_OK) {
        EV_LOGGING (INTERFACE, ERR,"NAS-VXLAN-INIT", "Failed to register VXLAN CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (intf_obj_handler_registration(obj_INTF_STATE, nas_int_type_VXLAN,
            nas_interface_if_state_get_handler, nas_interface_com_if_state_set_handler) != STD_ERR_OK) {
        EV_LOGGING (INTERFACE, ERR,"NAS-VXLAN-INIT", "Failed to register VXLAN CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    EV_LOGGING(INTERFACE,ERR,"NAS-VXLAN-INIT", "Registered the vxlan interface/interface-state");
    return STD_ERR_OK;
}
