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
 * nas_vlan_bridge_cps.cpp
 */


#include "iana-if-type.h"
#include "dell-base-if-vlan.h"
#include "dell-base-if.h"
#include "dell-interface.h"
#include "interface_obj.h"
#include "interface/nas_interface_utils.h"
#include "bridge/nas_interface_bridge_utils.h"
#include "bridge/nas_interface_bridge_map.h"
#include "bridge/nas_vlan_bridge_cps.h"
#include "bridge/nas_interface_bridge_com.h"

#include "cps_api_object_key.h"
#include "cps_api_object_tools.h"
#include "cps_api_operation.h"
#include "cps_class_map.h"
#include "std_mac_utils.h"
#include "cps_api_events.h"
#include "event_log.h"
#include "event_log_types.h"
#include "nas_int_utils.h"
#include "nas_int_com_utils.h"
#include "std_config_node.h"
#include "std_mutex_lock.h"
#include <unordered_set>

#define NUM_INT_CPS_API_THREAD 1
static cps_api_operation_handle_t nas_if_global_handle;
static auto _l3_vlan_set = * new std::unordered_set<hal_vlan_id_t>;
static bool nas_bridge_process_port_association(const char *if_name, npu_id_t npu, port_t port,bool add){

    t_std_error rc = STD_ERR_OK;

    EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN-MAP","Got Interface association for interface %s ", if_name);
    std_mutex_simple_lock_guard _lg(nas_bridge_mtx_lock());
    hal_ifindex_t ifindex;
    if (nas_int_name_to_if_index(&ifindex, if_name) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN","Missing interface %s", if_name);
        return true; //always return true
    }

    auto  m_list = nas_intf_get_master(ifindex);
    for(auto it : m_list){
        if(it.type == nas_int_type_LAG){
            continue;
        }else if(it.type == nas_int_type_VLAN){
            ndi_port_t ndi_port = { npu, port };
            char bridge_name[HAL_IF_NAME_SZ];
            memset(bridge_name, 0 , sizeof(bridge_name));
            if ((rc = nas_int_get_if_index_to_name(it.m_if_idx, bridge_name, HAL_IF_NAME_SZ)) == STD_ERR_OK) {
                nas_bridge_utils_associate_npu_port(bridge_name, if_name, &ndi_port, it.mode, add);
            }
        }
    }
    return true;
}
/*  use CPS request obj and publish bASE-IF-VLAN object for internal modules like MAC and STP  */
static void _nas_publish_vlan_cps_req_obj(cps_api_object_t obj) {

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj),BASE_IF_VLAN_IF_INTERFACES_INTERFACE_OBJ,
        cps_api_qualifier_OBSERVED);

    cps_api_object_set_type_operation(cps_api_object_key(obj), op);
    cps_api_event_thread_publish(obj);
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj), DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
        cps_api_qualifier_OBSERVED);
    cps_api_object_set_type_operation(cps_api_object_key(obj), op);
    cps_api_event_thread_publish(obj);
    cps_api_key_set_qualifier(cps_api_object_key(obj), cps_api_qualifier_TARGET);

}
static bool _nas_bridge_handle_mode_change(const char *br_name, BASE_IF_MODE_t mode)
{

    std_mutex_simple_lock_guard _lg(nas_vlan_mode_mtx());
    if(nas_g_scaled_vlan_get() == false) return true;

    nas_bridge_utils_l3_mode_set(br_name, mode);
    if(mode == BASE_IF_MODE_MODE_L3){

        /*  get the list of all tagged members  */

        memberlist_t tagged_list;
        if( nas_bridge_utils_mem_list_get(br_name, tagged_list, NAS_PORT_TAGGED) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge %s tagged member list get failed  ", br_name);
            return false;
        }
        hal_vlan_id_t vlan_id = 0;
        nas_bridge_utils_vlan_id_get(br_name, &vlan_id);

        /*  create sub interfaces  */
        /*  add all sub interfaces to the bridge in the kernel
         *  IT should already be in the local cache */
        if (nas_interface_os_vlan_subintf_list_create(tagged_list,vlan_id)  != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge %s tagged member list create failed  ", br_name);
            return false;
        }

        if (nas_bridge_utils_os_add_remove_memberlist(br_name, tagged_list, NAS_PORT_TAGGED, true) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge %s tagged member list create failed  ", br_name);
            return false;
        }
    }
    return true;
}

static cps_api_return_code_t nas_vlan_set_parent_bridge(const char *br_name, cps_api_object_t obj, cps_api_object_it_t & it){

    size_t len = cps_api_object_attr_len(it.attr);
    cps_api_return_code_t rc = cps_api_ret_code_OK;
    std::string cur_parent;
    nas_bridge_utils_parent_bridge_get(br_name, cur_parent);
    hal_vlan_id_t vlan_id = 0;
    nas_bridge_utils_vlan_id_get(br_name, &vlan_id);
    if(!len && cur_parent.size()){
        /*
         * treat attribute parent name attr len= 0 as the detach
         * if there was a valid parent bridge do clean up
         */
        if (nas_bridge_utils_detach_vlan(br_name, cur_parent.c_str()) != STD_ERR_OK) {
            rc  = cps_api_ret_code_ERR;
        }
    }else if(len && cur_parent.size()){
        EV_LOGGING(INTERFACE,ERR,"NAS-VLAN-ATTACH","Cannot add new parent bridge to vlan %s"
                "it already has a valid parent bridge %s",br_name, cur_parent.c_str());
        rc = cps_api_ret_code_ERR;
        cps_api_set_object_return_attrs(obj, rc, "Vlan %d already attached to the Virtual-Network %s",vlan_id, cur_parent.c_str());
    }else if ((len != 0) && (cur_parent.size() ==0)) {

        /*
         * this is when existing members needs to be migrated to parent bridge
         * for now assume when parent bridge is being set you can not
         * modify any other properties
         */
        std::string new_parent = std::string((const char *)(cps_api_object_attr_data_bin(it.attr)));

        if (nas_bridge_utils_is_l2_bridge(new_parent) != STD_ERR_OK) {
            cps_api_set_object_return_attrs(obj, rc, "Virtual-Network  %s does not exist",new_parent.c_str());
            return cps_api_ret_code_ERR;
        }

        if(!nas_bridge_is_empty(new_parent)) {
            EV_LOGGING(INTERFACE,ERR,"NAS-VLAN-ATTACH","Can't attach parent bridge %s to vlan %s as "
                      "parent bridge already has members",new_parent.c_str(),br_name);

            rc = cps_api_ret_code_ERR;
            cps_api_set_object_return_attrs(obj, rc, "Virtual-Network %s already has members",new_parent.c_str());
            return rc;
        }
        if (nas_bridge_utils_attach_vlan(br_name, new_parent.c_str()) != STD_ERR_OK) {
            rc  = cps_api_ret_code_ERR;
        }
        EV_LOGGING(INTERFACE,INFO,"NAS-VLAN-ATTACH","new parent bridge is %s ", new_parent.c_str());
    } else {
        EV_LOGGING(INTERFACE,ERR,"NAS-VLAN-ATTACH"," Invalid case: length is 0 and parent is not present %s", br_name);
        rc  = cps_api_ret_code_ERR;
        cps_api_set_object_return_attrs(obj, rc, "Virtual-Network name not provided");
    }
    return rc;
}

static cps_api_return_code_t nas_vlan_intf_cps_process_get(void * context, cps_api_get_params_t *param, size_t ix, bool get_state)
{
    std::string _br_name;


    EV_LOGGING(INTERFACE, DEBUG, "NAS-Vlan", "nas_vlan_intf_cps_get");

    std_mutex_simple_lock_guard _lg(nas_bridge_mtx_lock());

    cps_api_return_code_t rc =  cps_api_ret_code_ERR;
    cps_api_object_t filt = cps_api_object_list_get(param->filters,ix);
    cps_api_object_attr_t _name;
    cps_api_object_attr_t _vlan_id = cps_api_object_attr_get(filt,
                                    BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID);
    if (get_state) {
        _name = cps_api_get_key_data(filt, IF_INTERFACES_STATE_INTERFACE_NAME);
    } else {
        _name = cps_api_get_key_data(filt, IF_INTERFACES_INTERFACE_NAME);
    }

    if ((_name == nullptr) && (_vlan_id == nullptr))  {
        /*  Get all */
        nas_fill_all_bridge_info(&param->list, INT_VLAN_MODEL, get_state);
        return cps_api_ret_code_OK;
    } else {
        /*  Get based on the bridge name of vlan id  */
        const char *br_name = NULL;
        if (_name != nullptr) {
            br_name = (const char*)cps_api_object_attr_data_bin(_name);
        } else if (_vlan_id != nullptr) {
            hal_vlan_id_t vid = cps_api_object_attr_data_u32(_vlan_id);
            if (nas_bridge_vlan_to_bridge_get(vid, _br_name) == false) {
                return cps_api_ret_code_ERR;
            }
            br_name = (const char *)_br_name.c_str();
        }

        cps_api_object_t obj = cps_api_object_create();
        cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
            DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ, (get_state) ? cps_api_qualifier_OBSERVED : cps_api_qualifier_TARGET);

        if (nas_bridge_fill_info(br_name, obj) == cps_api_ret_code_OK) {
            if (get_state) {
                cps_convert_to_state_attribute(obj);
            }

            if (cps_api_object_list_append(param->list,obj)) {
                rc = cps_api_ret_code_OK;
            }
        }
        if (rc == cps_api_ret_code_ERR) cps_api_object_delete(obj);
    }
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t nas_vlan_intf_cps_get(void * context, cps_api_get_params_t *param, size_t ix) {
    return nas_vlan_intf_cps_process_get(context, param, ix, false);
}

static cps_api_return_code_t nas_vlan_intf_cps_get_state(void * context, cps_api_get_params_t *param, size_t ix) {
    return nas_vlan_intf_cps_process_get(context, param, ix, true);
}

static cps_api_return_code_t nas_cps_update_vlan(const char *br_name, cps_api_object_t obj)
{
    if (br_name == NULL) return cps_api_ret_code_ERR;

    cps_api_return_code_t rc =  cps_api_ret_code_ERR;
    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));
    cps_api_object_it_t it, mtu_it;
    bool untag_list_attr = false;
    bool tag_list_attr = false;
    memberlist_t tagged_list, untagged_list;
    bool mtu_set = false;

    BASE_IF_MODE_t mode;
    hal_vlan_id_t vlan_id;
    hal_ifindex_t  if_index;

    if (nas_bridge_utils_vlan_id_get(br_name, &vlan_id) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR,"NAS-VLAN", " Bridge with vlan does not Exists for %s", br_name);
        return cps_api_ret_code_ERR;
    }
    if (nas_bridge_utils_ifindex_get(br_name, &if_index) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR,"NAS-VLAN", " Bridge with vlan does not Exists for %s", br_name);
        return cps_api_ret_code_ERR;
    }

    cps_api_object_attr_t _vlan_id = cps_api_object_attr_get(obj,
                                    BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID);
    /*  Add vlan id in the object  */
    if (_vlan_id == nullptr) {
        cps_api_object_attr_add_u32(obj, BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, vlan_id);
    }
    cps_api_object_attr_t _ifndex = cps_api_object_attr_get(obj,
                                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
    if (_ifndex  == nullptr) {
        cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, if_index);
    }
    int id = 0;
    std::string cur_parent;
    cps_api_object_it_begin(obj,&it);
    for ( ; cps_api_object_it_valid(&it) ; cps_api_object_it_next(&it) ) {

        id = (int) cps_api_object_attr_id(it.attr);
        switch (id) {
        case DELL_IF_IF_INTERFACES_INTERFACE_PARENT_BRIDGE:
            if (nas_vlan_set_parent_bridge(br_name,obj, it) != cps_api_ret_code_OK) {
                return cps_api_ret_code_ERR;
            }
            break;
        case DELL_IF_IF_INTERFACES_INTERFACE_VLAN_MODE:
            mode = (BASE_IF_MODE_t)cps_api_object_attr_data_uint(it.attr);
            nas_bridge_utils_parent_bridge_get(br_name, cur_parent);
            if (cur_parent.size()) {
                /* Save the mode ,even if it is attached
                 * If error is retured for attached case ,
                 * reload fails when mode comes after parent.
                 */

                nas_bridge_utils_l3_mode_set(br_name, mode);
                EV_LOGGING(INTERFACE, ERR, "NAS-Vlan", "attrib VLAN_MODE setting failed  for vlan %d",  vlan_id);
            }
            if (!_nas_bridge_handle_mode_change(br_name, mode)) {
                rc = cps_api_ret_code_ERR;
                cps_api_set_object_return_attrs(obj, rc, "VLAN Mode setting Failed for %d", vlan_id);
                return rc;
            }
            break;
        case DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS:
            //for delete all, attribute will be sent but with zero length
            if (cps_api_object_attr_len(it.attr) != 0) {
                untagged_list.insert(std::string((const char *)cps_api_object_attr_data_bin(it.attr)));
            }
            untag_list_attr = true;
            break;

        case DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS:
            if (cps_api_object_attr_len(it.attr) != 0) {
                /*  Convert the tagged interface into vlan sub interface (eg. e101-001-0 -> e101-001-0.500) */
                char vlan_intf_name[HAL_IF_NAME_SZ];
                memset(vlan_intf_name,0,sizeof(vlan_intf_name));
                snprintf(vlan_intf_name,sizeof(vlan_intf_name),"%s.%d",
                                (const char *)cps_api_object_attr_data_bin(it.attr),vlan_id);
                tagged_list.insert(std::string(vlan_intf_name));
            }
            tag_list_attr = true;
            break;
        case DELL_IF_IF_INTERFACES_INTERFACE_LEARNING_MODE:
        {
            bool learn_disable = cps_api_object_attr_data_uint(it.attr) ? false : true;
            if(nas_bridge_utils_set_learning_disable(br_name,learn_disable) != STD_ERR_OK){
                rc = cps_api_ret_code_ERR;
                cps_api_set_object_return_attrs(obj, rc, "MAC Learning Mode setting Failed for vlan %d", vlan_id);
                return rc;
            }
        }
            break;
        case DELL_IF_IF_INTERFACES_INTERFACE_MTU:
            mtu_set = true;
            mtu_it = it;
            break;
        case DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS:
        case IF_INTERFACES_INTERFACE_ENABLED:
           if (op  == cps_api_oper_CREATE) break; /*  attributes already set during create time */
           if (nas_bridge_utils_set_attribute(br_name, obj , it) !=STD_ERR_OK) {
               EV_LOGGING(INTERFACE, ERR, "NAS-Vlan", "attrib %d setting failed  for vlan %d", id, vlan_id);
               return  cps_api_ret_code_ERR;
            }
            break;
        default:
            EV_LOGGING(INTERFACE, INFO, "NAS-Vlan", "Received attrib %d", id);
            break;
        }
    }
    memberlist_t *tag_list_ptr = nullptr, *untag_list_ptr = nullptr;
    if (tag_list_attr == true)  {
        tag_list_ptr =  &tagged_list;
        EV_LOGGING(INTERFACE, INFO, "NAS-Vlan", "Received %lu tagged ports for bridge %s ",
                            tagged_list.size(), br_name);
    }
    if (untag_list_attr == true)  {
        untag_list_ptr =  &untagged_list;
        EV_LOGGING(INTERFACE, INFO, "NAS-Vlan", "Received %lu untagged ports for bridge %s ",
                            untagged_list.size(), br_name);
    }
    if ( nas_bridge_process_cps_memberlist(br_name, tag_list_ptr, untag_list_ptr) != STD_ERR_OK) {
        rc = cps_api_ret_code_ERR;
        cps_api_set_object_return_attrs(obj, rc , "Member list processing failed for vlan %d", vlan_id);
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan", "Member list processing failed for bridge %s" , br_name);
        return rc;
    }

    /* Check is mtu config set need to handled, mtu_set flag will be set to true if mtu attr is specified
     * in cps set attr list.
     */
    if (mtu_set == true) {
        if (nas_bridge_utils_set_attribute(br_name, obj , mtu_it) !=STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan", "attrib %d setting failed  for vlan %d", id, vlan_id);
            return  cps_api_ret_code_ERR;
        }
    }
    if ((mtu_set == false) && ((tag_list_attr == true)
                || (untag_list_attr == true))) {
        nas_bridge_utils_os_set_mtu(br_name);
    }
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t nas_cps_set_vlan(cps_api_object_t obj)
{

    char br_name[HAL_IF_NAME_SZ];
    cps_api_object_attr_t vlan_name_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);
    memset(br_name,0,sizeof(br_name));
    if(vlan_name_attr != NULL) {
        strncpy(br_name,(char *)cps_api_object_attr_data_bin(vlan_name_attr),sizeof(br_name)-1);
    }
    std_mutex_simple_lock_guard _lg(nas_bridge_mtx_lock());
    EV_LOGGING(INTERFACE, INFO, "NAS-VLAN-SET", "SET VLAN %s using CPS", br_name);
    if (nas_cps_update_vlan((const char *)br_name, obj) != cps_api_ret_code_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan", "CPS SET Request for VLAN %s Failed", br_name);
        return cps_api_ret_code_ERR;
    }
    _nas_publish_vlan_cps_req_obj(obj);
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t nas_cps_create_vlan(cps_api_object_t obj)
{
    char name[HAL_IF_NAME_SZ] = "\0";
    cps_api_return_code_t rc = cps_api_ret_code_ERR;
    //bool create = false;
    //// TODO create processing to be different ?? ?

    std_mutex_simple_lock_guard _lg(nas_bridge_mtx_lock());

    cps_api_object_attr_t vlan_id_attr = cps_api_object_attr_get(obj, BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID);

    if (vlan_id_attr == NULL) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN","Missing VLAN ID during create vlan");
        return rc;
    }

    hal_vlan_id_t vlan_id = cps_api_object_attr_data_u32(vlan_id_attr);
    cps_api_object_attr_t vlan_name_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);

    cps_api_object_attr_t attr = cps_api_object_attr_get(obj, DELL_IF_IF_INTERFACES_INTERFACE_VLAN_TYPE);
    BASE_IF_VLAN_TYPE_t vlan_type = BASE_IF_VLAN_TYPE_DATA;

    if (attr != NULL) {
        vlan_type = (BASE_IF_VLAN_TYPE_t) cps_api_object_attr_data_u32(attr);
    }

    if( (vlan_id < MIN_VLAN_ID) || (vlan_id > MAX_VLAN_ID) || (nas_bridge_vlan_in_use(vlan_id))) {
        EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                           "Invalid or Used VLAN ID during create vlan %d", vlan_id);
        return rc;
    }
    EV_LOGGING(INTERFACE, INFO, "NAS-VLAN-CREATE", "Create VLAN %d using CPS", vlan_id);

    cps_api_object_attr_t mode_attr = cps_api_object_attr_get(obj, DELL_IF_IF_INTERFACES_INTERFACE_VLAN_MODE);
    BASE_IF_MODE_t mode = BASE_IF_MODE_MODE_L2;

    // Check in the reserved vlan list
    if (nas_check_reserved_vlan_id(vlan_id))  {
        mode = BASE_IF_MODE_MODE_L3;
    }

    if (mode_attr != NULL) {
        mode = (BASE_IF_MODE_t) cps_api_object_attr_data_u32(mode_attr);
    }

    memset(name,0,sizeof(name));

    if(vlan_name_attr != NULL) {
        strncpy(name,(char *)cps_api_object_attr_data_bin(vlan_name_attr),sizeof(name)-1);
    } else {
        /* Construct Bridge name for this vlan -
         * if vlan_id is 100 then bridge name is "br100"
        */
        snprintf(name, sizeof(name), "br%d", vlan_id);
        cps_api_set_key_data(obj, IF_INTERFACES_INTERFACE_NAME, cps_api_object_ATTR_T_BIN, name, strlen(name)+1);
    }
    EV_LOGGING(INTERFACE, INFO, "NAS-VLAN-CREATE", "CPS Create VLAN request for %s",name );

    do {
        rc = cps_api_ret_code_ERR;
        if (nas_bridge_create_vlan(name, vlan_id, obj) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan", "Bridge creation failed %s", name);
            break;
        }

        /*  Add the vlan id to bridge mapping  */
        std::string bridge_name(name);
        nas_bridge_vlan_to_bridge_map_add(vlan_id, bridge_name);
        //  set l3 mode and vlan type
        nas_bridge_utils_vlan_type_set(name, vlan_type);
        nas_bridge_utils_l3_mode_set(name, mode);
        if (nas_cps_update_vlan(name, obj) != cps_api_ret_code_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan",
                   "VLAN Attribute configuration failed for create bridge interface %s vlan %d",
                   name, vlan_id);
            break;
        }
        rc = cps_api_ret_code_OK;
    }while(0);

    _nas_publish_vlan_cps_req_obj(obj);
    EV_LOGGING(INTERFACE, NOTICE, "NAS-VLAN-CREATE", "CPS Create VLAN request for %s Successful",name );
    return rc;
}
static cps_api_return_code_t nas_cps_delete_vlan(cps_api_object_t obj)
{
    cps_api_return_code_t rc = cps_api_ret_code_OK;
    char br_name[HAL_IF_NAME_SZ] = "\0";
    memset(br_name,0,sizeof(br_name));

     hal_ifindex_t if_index;
    EV_LOGGING(INTERFACE, INFO, "NAS-VLAN-DELETE", "CPS Delete VLAN ");
    cps_api_object_attr_t vlan_name_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);
    cps_api_object_attr_t vlan_if_attr = cps_api_object_attr_get(obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);

    if ((vlan_name_attr == nullptr)  && (vlan_if_attr == nullptr)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-DELETE", "Missing Vlan interface name or if_index");
        return cps_api_ret_code_ERR;
    }

    if( vlan_name_attr == nullptr) {
        if_index =  cps_api_object_attr_data_u32(vlan_if_attr);
        if( nas_int_get_if_index_to_name(if_index, br_name,  sizeof(br_name) != STD_ERR_OK)) {
            EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-DELETE", "Missing Vlan interface name ");
            return cps_api_ret_code_ERR;
       }
    }else{
         safestrncpy(br_name, (const char *)cps_api_object_attr_data_bin(vlan_name_attr),sizeof(br_name));
    }
    /*  Acquire bridge lock */
    std_mutex_simple_lock_guard _lg(nas_bridge_mtx_lock());

    if (vlan_if_attr == nullptr)  {
        if (nas_int_name_to_if_index(&if_index, br_name) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-VLAN","Missing interface %s", br_name);
            return cps_api_ret_code_ERR; //always return true
        }
        cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,if_index);
    }
    /*  Add vlan id in the cps object */
    hal_vlan_id_t vlan_id;
    if (nas_bridge_utils_vlan_id_get(br_name, &vlan_id) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR,"NAS-VLAN", " Bridge with vlan does not Exists for %s", br_name);
        return cps_api_ret_code_ERR;
    }
    cps_api_object_attr_add_u32(obj, BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, vlan_id);
     /*  First check if the vlan is part of any parent bridge */
    std::string cur_parent;
    nas_bridge_utils_parent_bridge_get(br_name, cur_parent);
    if (cur_parent.size()) {
        if (nas_bridge_utils_detach_vlan(br_name, cur_parent.c_str()) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-DELETE", "Virtual Network detach Failure %s", br_name);
        }
    }
    EV_LOGGING(INTERFACE, INFO, "NAS-VLAN-DELETE", "CPS Delete VLAN request for %s",br_name );
    if (nas_bridge_delete_bridge(br_name) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-DELETE", "Bridge Deletion Failure %s", br_name);
        return cps_api_ret_code_ERR;
    }
    /*  Remove vlan id to bridge mapping */
    nas_bridge_vlan_to_bridge_map_del(vlan_id);
    /*  Publish delete event */
    _nas_publish_vlan_cps_req_obj(obj);
    EV_LOGGING(INTERFACE, NOTICE, "NAS-VLAN-DELETE", "Bridge Deletion Successful: %s", br_name);
    return rc;
}


static cps_api_return_code_t nas_vlan_intf_cps_set(void *context, cps_api_transaction_params_t *param, size_t ix)
{

    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    EV_LOGGING(INTERFACE, DEBUG, "NAS-Vlan",
           "nas_process_cps_vlan_set");

    cps_api_object_t cloned = cps_api_object_list_create_obj_and_append(param->prev);
    if (cloned == NULL) return cps_api_ret_code_ERR;

    cps_api_object_clone(cloned,obj);

    if (op == cps_api_oper_CREATE)      return(nas_cps_create_vlan(obj));
    else if (op == cps_api_oper_SET)    return(nas_cps_set_vlan(obj));
    else if (op == cps_api_oper_DELETE) return(nas_cps_delete_vlan(obj));

    return cps_api_ret_code_ERR;
}

static cps_api_return_code_t nas_vlan_intf_cps_set_state(void *context, cps_api_transaction_params_t *param, size_t ix)
{
    return cps_api_ret_code_ERR;
}

static bool nas_bridge_if_set_handler(cps_api_object_t obj, void *context)
{
    nas_int_port_mapping_t port_mapping;
    if(!nas_get_phy_port_mapping_change( obj, &port_mapping)){
        EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN-MAP","Interface event is not an "
                "association/dis-association event");
        return true;
    }

    EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN-MAP","Got Interface event ");
    cps_api_object_attr_t npu_attr = cps_api_object_attr_get(obj,
                                    BASE_IF_PHY_IF_INTERFACES_INTERFACE_NPU_ID);
    cps_api_object_attr_t port_attr = cps_api_object_attr_get(obj,
                                    BASE_IF_PHY_IF_INTERFACES_INTERFACE_PORT_ID);
    cps_api_object_attr_t if_name_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);

    if (npu_attr == nullptr || port_attr == nullptr || if_name_attr == nullptr) {
        EV_LOGGING(INTERFACE,DEBUG, "NAS-VLAN-MAP", "Interface object does not have if-index/npu/port");
        return true;
    }

    const char *if_name = (const char *)cps_api_object_attr_data_bin(if_name_attr);
    npu_id_t npu = cps_api_object_attr_data_u32(npu_attr);
    npu_port_t port = cps_api_object_attr_data_u32(port_attr);

    bool add = (port_mapping == nas_int_phy_port_MAPPED) ? true : false;

    return nas_bridge_process_port_association(if_name,npu,port,add);

}

t_std_error nas_vlan_bridge_cps_init(cps_api_operation_handle_t handle) {

    process_vlan_config_file();

    if (intf_obj_handler_registration(obj_INTF, nas_int_type_VLAN,
                nas_vlan_intf_cps_get, nas_vlan_intf_cps_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-INIT", "Failed to register VLAN interface CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    if (intf_obj_handler_registration(obj_INTF_STATE, nas_int_type_VLAN,
                nas_vlan_intf_cps_get_state, nas_vlan_intf_cps_set_state) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-INIT", "Failed to register VLAN interface-state CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    cps_api_event_reg_t reg;
    cps_api_key_t key;

    memset(&reg, 0, sizeof(cps_api_event_reg_t));

    if (!cps_api_key_from_attr_with_qual(&key,
            DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
            cps_api_qualifier_OBSERVED)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-INIT", "Cannot create a key for interface event");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    reg.objects = &key;
    reg.number_of_objects = 1;

    if (cps_api_event_thread_reg(&reg, nas_bridge_if_set_handler, NULL)
            != cps_api_ret_code_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-INIT", "Cannot register interface operation event");
        return STD_ERR(INTERFACE,FAIL,0);
    }

    cps_api_registration_functions_t f;
    memset(&f,0,sizeof(f));

    char buff[CPS_API_KEY_STR_MAX];
    memset(buff,0,sizeof(buff));

    //Create a handle for global INTERFACE objects
    if (cps_api_operation_subsystem_init(&nas_if_global_handle,NUM_INT_CPS_API_THREAD)!=cps_api_ret_code_OK) {
        return STD_ERR(CPSNAS,FAIL,0);
    }

    if (!cps_api_key_from_attr_with_qual(&f.key,DELL_BASE_IF_CMN_IF_INTERFACES_OBJ,
                                                            cps_api_qualifier_TARGET)) {
       EV_LOGGING(INTERFACE,ERR,"NAS-VLAN","Could not translate %d to key %s",
                   (int)(DELL_BASE_IF_CMN_IF_INTERFACES_OBJ),
                   cps_api_key_print(&f.key,buff,sizeof(buff)-1));
       return STD_ERR(INTERFACE,FAIL,0);
    }

    f.handle = nas_if_global_handle;
    f._write_function = nas_interface_handle_global_set;

    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
       return STD_ERR(INTERFACE,FAIL,0);
    }

    return STD_ERR_OK;
}
