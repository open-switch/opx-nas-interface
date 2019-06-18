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
 * nas_interface_bridge_com.cpp

 */


#include "dell-base-if-vlan.h"
#include "dell-base-if.h"
#include "dell-interface.h"
#include "nas_ndi_vlan.h"
#include "bridge/nas_interface_bridge_com.h"
#include "nas_switch.h"
#include "nas_com_bridge_utils.h"
#include "hal_if_mapping.h"
#include "interface/nas_interface_vlan.h"

#include "cps_class_map.h"
#include "cps_api_object_key.h"
#include "cps_api_events.h"
#include "std_config_node.h"
#include <unordered_set>
#include <mutex>


static bool _scaled_vlan = false;
static hal_vlan_id_t vn_untagged_vlan=1;
static std::mutex _m;
static auto _l3_vlan_set = * new std::unordered_set<hal_vlan_id_t>;
static std_mutex_lock_create_static_init_rec(_vlan_mode_mtx);
static std_mutex_lock_create_static_init_rec(_nas_bridge_mtx);

std_mutex_type_t *nas_vlan_mode_mtx()
{
    return &_vlan_mode_mtx;
}
std_mutex_type_t *nas_bridge_mtx_lock()
{
    return &_nas_bridge_mtx;
}

static void nas_reserved_vlan_list_add(hal_vlan_id_t vlan_id) {

    _l3_vlan_set.insert(vlan_id);
}
bool nas_check_reserved_vlan_id(hal_vlan_id_t vlan_id)  {
    auto it = _l3_vlan_set.find(vlan_id);
    if(it != _l3_vlan_set.end())
        return true;

    return false;
}

bool nas_g_scaled_vlan_get(void)
{
    return _scaled_vlan;
}
void nas_g_scaled_vlan_set(bool enable)
{
    _scaled_vlan = enable;
}

bool nas_vlan_interface_get_vn_untagged_vlan(hal_vlan_id_t * vlan_id){
    std::lock_guard<std::mutex> l(_m);
    *vlan_id = vn_untagged_vlan;
    return true;
}


cps_api_return_code_t nas_interface_handle_global_set (void * context,
                                                       cps_api_transaction_params_t *param,
                                                      size_t ix) {
     cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
     cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

     EV_LOGGING(INTERFACE, DEBUG, "NAS-Vlan","nas_if_handle_global_set");

     if( op == cps_api_oper_SET){
         cps_api_object_attr_t _scaled_vlan_attr = cps_api_object_attr_get(obj,
                                     DELL_IF_IF_INTERFACES_VLAN_GLOBALS_SCALED_VLAN);
         if (_scaled_vlan_attr != NULL) {
             std_mutex_simple_lock_guard _lg(&_vlan_mode_mtx);
             _scaled_vlan = cps_api_object_attr_data_uint(_scaled_vlan_attr);
             EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN","Changed the default scaled vlan mode to %d",
                         _scaled_vlan);

         }

         cps_api_object_attr_t _vn_untagged_vlan_attr = cps_api_object_attr_get(obj,
                                     DELL_IF_IF_INTERFACES_VLAN_GLOBALS_VN_UNTAGGED_VLAN);
         if (_vn_untagged_vlan_attr != NULL) {
             hal_vlan_id_t nas_com_ut_id;
             {
                 std::lock_guard<std::mutex> l(_m);
                 nas_com_ut_id = vn_untagged_vlan = cps_api_object_attr_data_uint(_vn_untagged_vlan_attr);
                     EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN","Changed the vn untagged vlan to %d",
                             vn_untagged_vlan);
             }
             nas_com_set_1d_br_reserved_vid(nas_com_ut_id);
         }

     }

     cps_api_object_attr_t _def_vlan_name = cps_api_object_attr_get(obj,
                                 DELL_IF_IF_INTERFACES_VLAN_GLOBALS_DEFAULT_VLAN_ID);
     cps_api_object_attr_t _def_vlan_id = cps_api_object_attr_get(obj,
                                 DELL_IF_IF_INTERFACES_VLAN_GLOBALS_DEFAULT_VLAN);

     if (_def_vlan_name) {
         npu_id_t npu_id = 0;   //@TODO to retrive NPU ID in multi npu case
         const char * default_vlan_name = (const char *)cps_api_object_attr_data_bin(_def_vlan_name);
         EV_LOGGING(INTERFACE,INFO,"NAS-VLAN","Default VLAN Name %s",default_vlan_name);
         interface_ctrl_t entry;
         memset(&entry,0,sizeof(entry));
         entry.q_type = HAL_INTF_INFO_FROM_IF_NAME;
         strncpy(entry.if_name, default_vlan_name, sizeof(entry.if_name) - 1);
         t_std_error rc  = dn_hal_get_interface_info(&entry);
         if ((rc != STD_ERR_OK) && _def_vlan_id) {
            entry.vlan_id = cps_api_object_attr_data_u32(_def_vlan_id);
            EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN","Failed to find vlan interface %s and vlan id %d",
                     entry.if_name, entry.vlan_id);
         } else if ((rc != STD_ERR_OK) || (entry.int_type != nas_int_type_VLAN)) {
            EV_LOGGING(INTERFACE,ERR,"NAS-VLAN"," Interface %s doesn't exist or is not a VLAN type", entry.if_name);
            return cps_api_ret_code_ERR;

         }
         nas_vlan_interface_set_default_vlan(entry.vlan_id);
         ndi_set_default_vlan_id (npu_id, entry.vlan_id);
         EV_LOGGING(INTERFACE,INFO,"NAS-VLAN","Default VLAN is %d",entry.vlan_id);
     }

     /*  Publish the object */
    cps_api_key_set(cps_api_object_key(obj),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_OBSERVED);
    cps_api_event_thread_publish(obj);
    cps_api_key_set(cps_api_object_key(obj),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_TARGET);
    return cps_api_ret_code_OK;
}

void process_vlan_config_file(){

    std_config_hdl_t _hdl = std_config_load(INTERFACE_VLAN_CFG_FILE);
    if(_hdl == NULL){
        EV_LOGGING(INTERFACE,ERR,"NAS-VLAN","Error loading Interface VLAN config file");
        return;
    }
    std_config_node_t _node = std_config_get_root(_hdl);
    if(_node == NULL){
        return;
    }
    for(_node = std_config_get_child(_node); _node != NULL ; _node = std_config_next_node(_node)){
        const char * vlan_id = std_config_attr_get(_node,"id");
        nas_reserved_vlan_list_add(atoi(vlan_id));
    }

    std_config_unload(_hdl);

}


t_std_error nas_default_vlan_cache_init(void)
{
    const nas_switches_t *switches = nas_switch_inventory();

    for (size_t ix = 0; ix < switches->number_of_switches; ++ix) {
        const nas_switch_detail_t * sd = nas_switch((nas_switch_id_t) ix);

        if (sd == NULL) {
            EV_LOGGING(INTERFACE,ERR,"NAS-VLAN","Switch Details Configuration file is erroneous");
            return STD_ERR(INTERFACE,PARAM,0);
        }

        for (size_t sd_ix = 0; sd_ix < sd->number_of_npus; ++sd_ix) {
            if(ndi_del_new_member_from_default_vlan(sd->npus[sd_ix],0,true) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE,ERR,"NAS-VLAN","Failed to remove members"
                        " from default vlan for NPU %d.",sd->npus[sd_ix]);
            }
        }
    }

    return STD_ERR_OK;
}
