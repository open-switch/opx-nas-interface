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
 * nas_vxlan_bridge_cps.cpp
 */


#include "iana-if-type.h"
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
#include "cps_api_events.h"
#include "event_log.h"
#include "event_log_types.h"
#include "nas_int_utils.h"
#include "nas_int_com_utils.h"
#include "std_mutex_lock.h"


static cps_api_return_code_t nas_vn_intf_cps_get(void * context, cps_api_get_params_t *param, size_t ix) {
    return cps_api_ret_code_OK;
}


void nas_publish_vn_intf_object(cps_api_object_t obj) {


    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    cps_api_key_from_attr_with_qual(cps_api_object_key(obj), DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
        cps_api_qualifier_OBSERVED);
    cps_api_object_set_type_operation(cps_api_object_key(obj), op);
    cps_api_event_thread_publish(obj);
    cps_api_key_set_qualifier(cps_api_object_key(obj), cps_api_qualifier_TARGET);
}


static cps_api_return_code_t nas_cps_update_vn(const char *br_name, cps_api_object_t obj)
{
    if (br_name == NULL) return cps_api_ret_code_ERR;
    hal_ifindex_t  if_index;
    cps_api_object_it_t it;

    cps_api_object_attr_t _ifndex = cps_api_object_attr_get(obj,
                                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);
    if (_ifndex  == nullptr) {
        if (nas_bridge_utils_ifindex_get(br_name, &if_index) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR,"NAS-VN-SET", " Bridge with vxlan does not Exists for %s", br_name);
            return cps_api_ret_code_ERR;
        }
        cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, if_index);
    } else {
        if_index =  cps_api_object_attr_data_u32(_ifndex);
    }

    cps_api_object_it_begin(obj,&it);
    for ( ; cps_api_object_it_valid(&it) ; cps_api_object_it_next(&it) ) {

        int id = (int) cps_api_object_attr_id(it.attr);
        switch (id) {
        case DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS:
        case DELL_IF_IF_INTERFACES_INTERFACE_MTU:
        case IF_INTERFACES_INTERFACE_ENABLED:
           if (nas_bridge_utils_set_attribute(br_name, obj , it) !=STD_ERR_OK) {
               EV_LOGGING(INTERFACE, ERR, "NAS-VN-SET", "attrib %d setting failed  for VN  %s", id, br_name);
               return  cps_api_ret_code_ERR;
            }
            break;
        default:
            EV_LOGGING(INTERFACE, INFO, "NAS-VN-SET", "Received attrib %d", id);
            break;
        }
    }
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t nas_cps_set_vn(cps_api_object_t obj)
{

    char br_name[HAL_IF_NAME_SZ];
    cps_api_object_attr_t vlan_name_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);
    memset(br_name,0,sizeof(br_name));
    if(vlan_name_attr != NULL) {
        strncpy(br_name,(char *)cps_api_object_attr_data_bin(vlan_name_attr),sizeof(br_name)-1);
    }
    std_mutex_simple_lock_guard _lg(nas_bridge_mtx_lock());
    EV_LOGGING(INTERFACE, INFO, "NAS-VN-SET", "SET VN %s using CPS", br_name);
    if (nas_cps_update_vn((const char *)br_name, obj) != cps_api_ret_code_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VN-SET", "CPS SET Request for VN %s Failed", br_name);
        return cps_api_ret_code_ERR;
    }
    nas_publish_vn_intf_object(obj);
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t nas_cps_create_vn(cps_api_object_t obj)
{

    t_std_error rc = STD_ERR_OK;
    char bridge_name[HAL_IF_NAME_SZ] = "\0";
    const char *br_name = NULL;
    NAS_DOT1D_BRIDGE *dot1d_br_obj = nullptr;
    hal_ifindex_t idx;

    cps_api_object_attr_t _name = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);
    if (_name == nullptr) {
       /* TODO: add new ID in yang if required*/
       cps_api_object_attr_t _id = cps_api_object_attr_get(obj, BRIDGE_DOMAIN_BRIDGE_ID);
        if (_id == nullptr) {
            EV_LOGGING(INTERFACE,ERR,"NAS-VN-CREATE", "Create request received without bridge name or ID");
            return cps_api_ret_code_ERR;
        }
        uint32_t bridge_id = cps_api_object_attr_data_u32(_id);
        snprintf(bridge_name, sizeof(bridge_name), "vn%d", bridge_id);
    } else {
        br_name = (const char*)cps_api_object_attr_data_bin(_name);
        safestrncpy(bridge_name, br_name, sizeof(bridge_name));
    }
    bool exist =false;
    /*  Take the bridge lock  */
    std_mutex_simple_lock_guard _lg(nas_bridge_mtx_lock());

    /*  If bridge already exists set mode correctly  , else continue as bridge doesn't exist */
    if (nas_bridge_set_mode_if_bridge_exists(bridge_name, INT_MOD_CREATE, exist) != STD_ERR_OK) {
        cps_api_set_object_return_attrs(obj, rc, "Virtual Interface exists as 1Q");
        EV_LOGGING(INTERFACE, ERR, "NAS-VN-CREATE", "Bridge exists as 1Q %s", bridge_name);
        return cps_api_ret_code_ERR;

    }
    if (exist == false) {

        EV_LOGGING(INTERFACE, DEBUG, "NAS-VN-CREATE", "Create virtual network INT for %s ",bridge_name );
        if ((rc = nas_bridge_utils_os_create_bridge(bridge_name, obj, &idx))  != STD_ERR_OK) {
            return cps_api_ret_code_ERR;
        }
         cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,idx);
        /*  Create object and in the NPU with mode 1D  by default */
        /*  Register with hal control block if not registered */
        if (nas_create_bridge(bridge_name, BASE_IF_BRIDGE_MODE_1D, idx, (NAS_BRIDGE **)&dot1d_br_obj) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-VN-CREATE", " Bridge obj create failed %s", bridge_name);
            cps_api_set_object_return_attrs(obj, rc, "Virtual Interface creation failed");
            return cps_api_ret_code_ERR;
        }
        /* Set how this one was  created */
       dot1d_br_obj->set_create_flag(INT_MOD_CREATE);
    } else {
       /* If bridge exists means it is created from bridge model(even in attached case) and will not have subintf
        * so create tagged sub-int and add to bridge in kernel
        */
        memberlist_t tagged_list;
        if( nas_bridge_utils_mem_list_get(br_name, tagged_list, NAS_PORT_TAGGED) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-VN-INT", " Bridge %s tagged member list get failed  ", br_name);
            return false;
        }

        if (nas_interface_create_subintfs(tagged_list)  != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-VN-INT", " Bridge %s tagged member list create in os failed  ", br_name);
            return false;
        }

        if (nas_bridge_utils_os_add_remove_memberlist(br_name, tagged_list, NAS_PORT_TAGGED, true) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-VN-INT", " Bridge %s tagged member list add failed  ", br_name);
            return false;
        }
        /* Create vtep and add to bridge in os*/
        std::string name(br_name);
        if (nas_bridge_os_vxlan_create_add_to_br(name) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-VN-INT", " Bridge %s vtep member list create in os failed  ", br_name);
            return false;
        }
        /* By default ipv6 is enabled on VN , but bridge model create of vn makes it disable, so enable it again*/
        nas_os_interface_ipv6_config_handle(br_name, true);
    }
    nas_intf_handle_intf_mode_change(bridge_name, BASE_IF_MODE_MODE_NONE);
    if (nas_cps_update_vn((const char *)bridge_name, obj) != cps_api_ret_code_OK) {
        return cps_api_ret_code_ERR;
    }
    nas_publish_vn_intf_object(obj);


    EV_LOGGING(INTERFACE, NOTICE, "NAS-VN-CREATE", "CPS Create VN request for %s Successful",bridge_name );
    return cps_api_ret_code_OK;
}
static cps_api_return_code_t nas_cps_delete_vn(cps_api_object_t obj)
{

    char br_name[HAL_IF_NAME_SZ];
    memset(br_name,0,sizeof(br_name));

    hal_ifindex_t if_index;
    cps_api_object_attr_t vn_name_attr = cps_api_get_key_data(obj, IF_INTERFACES_INTERFACE_NAME);
    cps_api_object_attr_t vn_if_attr = cps_api_object_attr_get(obj, DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX);

    if ((vn_name_attr == nullptr)  && (vn_if_attr == nullptr)) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VN-DELETE", "Missing VN interface name or if_index");
        return cps_api_ret_code_ERR;
    }

    if( vn_name_attr == nullptr) {
        if_index =  cps_api_object_attr_data_u32(vn_if_attr);
        if( nas_int_get_if_index_to_name(if_index, br_name,  sizeof(br_name) != STD_ERR_OK)) {
            EV_LOGGING(INTERFACE, ERR, "NAS-VN-DELETE", "Missing VN interface name ");
            return cps_api_ret_code_ERR;
       }
    }else{
         safestrncpy(br_name, (const char *)cps_api_object_attr_data_bin(vn_name_attr),sizeof(br_name));
    }
    /*  Acquire bridge lock */
    std_mutex_simple_lock_guard _lg(nas_bridge_mtx_lock());

    if (vn_if_attr == nullptr)  {
        if (nas_int_name_to_if_index(&if_index, br_name) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE, ERR, "NAS-VN-DELETE","Missing interface %s", br_name);
            return cps_api_ret_code_ERR;
        }
        cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,if_index);
    }

    EV_LOGGING(INTERFACE,INFO,"NAS-VN-DELETE", " Delete bridge %s ", br_name);
    /* If there exists a VN  intf del subintf int from bridge and os */
    memberlist_t tagged_list;
    NAS_BRIDGE *br_obj;
    if (nas_bridge_map_obj_get(br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,DEBUG,"NAS-INT", " Bridge not in cache %s", br_name);
        return cps_api_ret_code_ERR;
    }
    if (br_obj->nas_bridge_tagged_member_present()) {

        if( nas_bridge_utils_mem_list_get(br_name, tagged_list, NAS_PORT_TAGGED) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,DEBUG,"NAS-INT", " Bridge %s tagged member list get failed, may be it doesn't exist  ", br_name);
            return true;
        }
        if (nas_bridge_utils_os_add_remove_memberlist(br_name, tagged_list, NAS_PORT_TAGGED, false) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge %s tagged member list create failed  ", br_name);
            return false;
        }

        /*  Now del all sub interfaces in the kernel
         *  leave them in the intf map cache so del for subintf succeeds from apps.
         */
        if (nas_interface_delete_subintfs(tagged_list)  != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge %s tagged member list del in os failed  ", br_name);
            return false;
        }
    }
    std::string name(br_name);
    if (nas_bridge_os_vxlan_del_frm_br(name) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-VN-INT", " Bridge %s vtep  del in os failed  ", br_name);
        return false;
    }


    /* Whether the bridge is created by bridge model(as l2 intf) or interface model(as l3 intf)
     * set mode to L2 mode at delete as, as an L3 interface it is getting deleted.
     */
    if (nas_intf_handle_intf_mode_change(br_name, BASE_IF_MODE_MODE_L2) == false) {
        EV_LOGGING(INTERFACE,ERR,"NAS-VN-DELETE", " Mode change  failed %s", br_name);
    }
    if (nas_bridge_delete_vn_bridge(br_name, obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-VN-DELETE", "VN   bridge del failed %s", br_name);
        return cps_api_ret_code_ERR;
    }

    nas_publish_vn_intf_object(obj);
    return cps_api_ret_code_OK;
}


static cps_api_return_code_t nas_vn_intf_cps_set(void *context, cps_api_transaction_params_t *param, size_t ix)
{

    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    EV_LOGGING(INTERFACE, DEBUG, "NAS-VN-Intf",
           "Virtual Network SET request");

    cps_api_object_t cloned = cps_api_object_list_create_obj_and_append(param->prev);
    if (cloned == NULL) return cps_api_ret_code_ERR;

    cps_api_object_clone(cloned,obj);

    if (op == cps_api_oper_CREATE)      return(nas_cps_create_vn(obj));
    else if (op == cps_api_oper_SET)    return(nas_cps_set_vn(obj));
    else if (op == cps_api_oper_DELETE) return(nas_cps_delete_vn(obj));

    return cps_api_ret_code_ERR;
}


t_std_error nas_vxlan_bridge_cps_init(cps_api_operation_handle_t handle) {

    if (intf_obj_handler_registration(obj_INTF, nas_int_type_DOT1D_BRIDGE,
                nas_vn_intf_cps_get, nas_vn_intf_cps_set) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VN-INIT", "Failed to register VXLAN interface CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }
#if 0
    if (intf_obj_handler_registration(obj_INTF_STATE, nas_int_type_DOT1D_BRIDGE,
                nas_vn_intf_cps_get_state, nas_vn_intf_cps_set_state) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-INIT", "Failed to register VLAN interface-state CPS handler");
        return STD_ERR(INTERFACE,FAIL,0);
    }
#endif

    return STD_ERR_OK;
}
