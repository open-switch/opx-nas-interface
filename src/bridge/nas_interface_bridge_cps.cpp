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
 * filename: nas_interface_bridge_cps.cpp
 */


#include "cps_api_object.h"
#include "ds_common_types.h"
#include "cps_api_object_key.h"
#include "cps_api_object_tools.h"
#include "cps_api_events.h"
#include "cps_class_map.h"
#include "bridge/nas_interface_bridge_cps.h"
#include "bridge/nas_interface_bridge_com.h"
#include "bridge/nas_interface_1d_bridge.h"
#include "std_mutex_lock.h"


#include <list>


static cps_api_return_code_t _nas_bridge_name_get(cps_api_object_t obj, char *bridge_name, size_t len)
{
    if (bridge_name == NULL) return cps_api_ret_code_ERR;
    const char *br_name = NULL;
    cps_api_object_attr_t _name = cps_api_get_key_data(obj,BRIDGE_DOMAIN_BRIDGE_NAME);
    if (_name == nullptr) {
       cps_api_object_attr_t _id = cps_api_object_attr_get(obj, BRIDGE_DOMAIN_BRIDGE_ID);
        if (_id == nullptr) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT-SET", "Create request received without bridge name or ID");
            return cps_api_ret_code_ERR;
        }
        uint32_t bridge_id = cps_api_object_attr_data_u32(_id);
        snprintf(bridge_name, len, "vn%d", bridge_id);
        cps_api_set_key_data(obj,BRIDGE_DOMAIN_BRIDGE_NAME,
                           cps_api_object_ATTR_T_BIN, bridge_name, strlen(bridge_name)+1);
    } else {
        br_name = (const char*)cps_api_object_attr_data_bin(_name);
        safestrncpy(bridge_name, br_name, len);
    }
    return cps_api_ret_code_OK;
}

/*  Bridge Object Handler initialization */
static cps_api_return_code_t _bridge_update(cps_api_object_t req, cps_api_object_t prev)
{
    t_std_error rc = STD_ERR_OK;
    cps_api_object_it_t it;
    cps_api_object_attr_t _name = cps_api_get_key_data(req,BRIDGE_DOMAIN_BRIDGE_NAME);
    if (_name == nullptr) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-SET", "SET request received without bridge name");
        return cps_api_ret_code_ERR;
    }
    const char *br_name = (const char*)cps_api_object_attr_data_bin(_name);
    BASE_IF_BRIDGE_MODE_t br_mode;
    cps_api_object_attr_t _mem_op;
    BRIDGE_DOMAIN_OPERATION_TYPE_t mem_op;
    const char * mem_name = nullptr;

    cps_api_object_it_begin(req,&it);
    for ( ; cps_api_object_it_valid(&it) ; cps_api_object_it_next(&it) ) {

        int id = (int) cps_api_object_attr_id(it.attr);
        switch (id) {
        /* This is not used ??? can it be remove ?? */
        case BRIDGE_DOMAIN_BRIDGE_MODE:
            br_mode = (BASE_IF_BRIDGE_MODE_t) cps_api_object_attr_data_u32(it.attr);
            if ((rc = nas_bridge_utils_change_mode(br_name, br_mode)) != STD_ERR_OK) {
                return cps_api_ret_code_ERR;
            }
            break;
        case BRIDGE_DOMAIN_BRIDGE_MEMBER_INTERFACE:
            mem_name = (const char *)cps_api_object_attr_data_bin(it.attr);
            _mem_op = cps_api_object_attr_get(req, BRIDGE_DOMAIN_SET_BRIDGE_INPUT_OPERATION);
            if (_mem_op == nullptr) {
                EV_LOGGING(INTERFACE,ERR,"NAS-INT-SET", "Mem Operation type not present");
                return cps_api_ret_code_ERR;
            }
            mem_op = (BRIDGE_DOMAIN_OPERATION_TYPE_t )cps_api_object_attr_data_u32(_mem_op);
            if (mem_op == BRIDGE_DOMAIN_OPERATION_TYPE_ADD_MEMBER) {
                // Add member
                if (nas_bridge_utils_add_member(br_name, mem_name) != STD_ERR_OK) {
                     EV_LOGGING(INTERFACE,ERR,"NAS-INT-SET", "Failed to add member %s to %s",
                                 mem_name, br_name);
                     return cps_api_ret_code_ERR;
                }
                nas_bridge_utils_publish_member_event(std::string(br_name), std::string(mem_name),
                        cps_api_oper_CREATE);
            } else if (mem_op == BRIDGE_DOMAIN_OPERATION_TYPE_DELETE_MEMBER) {
                // remove member
                if (nas_bridge_utils_remove_member(br_name, mem_name) != STD_ERR_OK) {
                     EV_LOGGING(INTERFACE,ERR,"NAS-INT-SET", "Failed to remove member %s to %s",
                                 mem_name, br_name);
                     return cps_api_ret_code_ERR;
                }

                nas_bridge_utils_publish_member_event(std::string(br_name), std::string(mem_name),
                                 cps_api_oper_DELETE);
            } else {
                 EV_LOGGING(INTERFACE,ERR,"NAS-INT-SET", "Invalid operation member %s to %s",
                             mem_name, br_name);
            }
            break;
        default:
            EV_LOGGING(INTERFACE, INFO, "NAS-Vlan", "Received Bridge attrib %d", id);
            break;
        }
    }
    /*  Get the list of member in the objects and then add memberlist in the kernel
     *  Add in the and in the NPU */
    /*  May need to first find the differenc */
    /*  Member could be vxlan , untagged port/lag or vlan sub interface */
    /*  Need to search in all list */
    /*  Set bridge MTU in OS  */
    nas_bridge_utils_os_set_mtu(br_name);
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _bridge_create(cps_api_object_t obj)
{
    t_std_error rc = STD_ERR_OK;
    char br_name[HAL_IF_NAME_SZ] = "\0";
    /*  Create in the kernel  */
    NAS_DOT1D_BRIDGE *dot1d_br_obj = nullptr;
    hal_ifindex_t idx;
    bool exist;
    if (_nas_bridge_name_get(obj, br_name, sizeof(br_name)) != cps_api_ret_code_OK) {
        return cps_api_ret_code_ERR;
    }
    /*  Take the bridge lock  */
    std_mutex_simple_lock_guard _lg(nas_bridge_mtx_lock());
    if (nas_bridge_set_mode_if_bridge_exists(br_name, BRIDGE_MOD_CREATE, exist) != STD_ERR_OK) {
         cps_api_set_object_return_attrs(obj, rc, "Virtual Network exists as 1Q");
         return cps_api_ret_code_ERR;

    }
    if (exist == false) {

        EV_LOGGING(INTERFACE, DEBUG, "NAS-BRIDGE-CREATE", "Create Bridge for %s ",br_name );
        if ((rc = nas_bridge_utils_os_create_bridge(br_name, obj, &idx))  != STD_ERR_OK) {
            return cps_api_ret_code_ERR;
        }
        /*  Create object and in the NPU with mode 1D  by default */
        /*  Register with hal control block if not registered */
        if (nas_create_bridge(br_name, BASE_IF_BRIDGE_MODE_1D, idx, (NAS_BRIDGE **)&dot1d_br_obj) != STD_ERR_OK) {
           EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE-CREATE", " Bridge obj create failed %s", br_name);
           cps_api_set_object_return_attrs(obj, rc, "Virtual Network creation failed");
           return cps_api_ret_code_ERR;
        }
        dot1d_br_obj->set_create_flag(BRIDGE_MOD_CREATE);
        /* Send mode =l2  if create from here */
        if (nas_intf_handle_intf_mode_change(br_name, BASE_IF_MODE_MODE_L2) == false) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE-CREATE", "Update to NAS-L3 about interface mode change failed for %s",
                                                      br_name);
        }
        /* Set ipv6 to false if vn is created by bridge model only */
        nas_os_interface_ipv6_config_handle(br_name, false);
    }

    /*  Call _bridge_update for member additions assuming it is already created */
    if (_bridge_update(obj, NULL) != STD_ERR_OK) {
        // TODO delete bridge
        return cps_api_ret_code_ERR;
    }


    /*  Publish  the object */
    nas_bridge_utils_publish_event(br_name, cps_api_oper_CREATE);

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _bridge_set(cps_api_object_t req, cps_api_object_t prev)
{
    /*  Take the bridge lock  */
    std_mutex_simple_lock_guard _lg(nas_bridge_mtx_lock());
    return _bridge_update(req, prev);
}

static cps_api_return_code_t _bridge_delete(cps_api_object_t obj)
{

    char br_name[HAL_IF_NAME_SZ] = "\0";
    if (_nas_bridge_name_get(obj, br_name, sizeof(br_name)) != cps_api_ret_code_OK) {
        return cps_api_ret_code_ERR;
    }
    /*  Take the bridge lock  */
    std_mutex_simple_lock_guard _lg(nas_bridge_mtx_lock());
    /*  Remove all its members  */
    /*  Delete the in the NPU */
    /*  Delete in the kernel  */
    /*  de-register from hal control block */
    if (nas_bridge_delete_bridge(br_name) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT-SET", "Failed to delete bridge %s", br_name);
        return cps_api_ret_code_ERR;
    }
    /*  publish event */
    nas_bridge_utils_publish_event(br_name, cps_api_oper_DELETE);
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t bridge_set(void * context, cps_api_transaction_params_t * param,size_t ix) {
    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    if (obj==nullptr) return cps_api_ret_code_ERR;

    cps_api_object_t prev = cps_api_object_list_create_obj_and_append(param->prev);
    if (prev==nullptr) return cps_api_ret_code_ERR;

    EV_LOGGING(INTERFACE,INFO,"NAS-INT-SET", "SET request received for physical interface");
    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    std_mutex_simple_lock_guard _lg(nas_bridge_mtx_lock());

    if (op ==cps_api_oper_CREATE) return _bridge_create(obj);
    else if (op ==cps_api_oper_SET) return _bridge_set(obj, prev);
    else if (op ==cps_api_oper_DELETE) return _bridge_delete(obj);


    return cps_api_ret_code_ERR;


}

static cps_api_return_code_t nas_bridge_cps_validate_member(const char *br_name, cps_api_object_t filt, cps_api_object_t obj) {

        /*  Check if there is a list of members in the get request for validation */
    std::list <std::string> intf_list;
    cps_api_object_it_t it;
    cps_api_object_it_begin(filt,&it);
    for ( ; cps_api_object_it_valid(&it); cps_api_object_it_next(&it)) {
        cps_api_attr_id_t id = cps_api_object_attr_id(it.attr);
        if (id == BRIDGE_DOMAIN_BRIDGE_MEMBER_INTERFACE) {
            const char *_mem = (const char *)cps_api_object_attr_data_bin(it.attr);
            intf_list.push_back(std::string(_mem));
        }
    }
    if (!intf_list.empty()) {
        std::list <std::string> mem_list;
        if (nas_bridge_utils_validate_bridge_mem_list(br_name, intf_list, mem_list) != STD_ERR_OK) {
            return cps_api_ret_code_ERR;
        }
        for (auto mem: mem_list) {
                cps_api_object_attr_add(obj, BRIDGE_DOMAIN_BRIDGE_MEMBER_INTERFACE, mem.c_str(),
                    strlen(mem.c_str())+1);
        }
        return cps_api_ret_code_OK;
    }
    return cps_api_ret_code_ERR;
}

static cps_api_return_code_t bridge_get (void * context, cps_api_get_params_t * param, size_t key_ix)
{

    cps_api_return_code_t rc =  cps_api_ret_code_ERR;
    cps_api_object_t filt = cps_api_object_list_get(param->filters,key_ix);
    cps_api_object_attr_t _name = cps_api_get_key_data(filt,BRIDGE_DOMAIN_BRIDGE_NAME);

    std_mutex_simple_lock_guard _lg(nas_bridge_mtx_lock());

    if (_name == nullptr) {
        /*  Get all */
        nas_fill_all_bridge_info(&param->list, BRIDGE_MODEL);
        return cps_api_ret_code_OK;
    } else {
        const char *br_name = (const char*)cps_api_object_attr_data_bin(_name);
        cps_api_object_t obj = cps_api_object_create();
        cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
            BRIDGE_DOMAIN_BRIDGE_OBJ, cps_api_qualifier_TARGET);
        cps_api_set_key_data(obj,BRIDGE_DOMAIN_BRIDGE_NAME,
                               cps_api_object_ATTR_T_BIN, br_name, strlen(br_name)+1);

        /*  Check if there is a list of members in the get request for validation */
        if ( nas_bridge_cps_validate_member(br_name, filt, obj) == cps_api_ret_code_OK)  {
            if (cps_api_object_list_append(param->list,obj)) {
                rc = cps_api_ret_code_OK;
            }
        } else if (nas_bridge_fill_info(br_name, obj) == cps_api_ret_code_OK) {
            if (cps_api_object_list_append(param->list,obj)) {
                rc = cps_api_ret_code_OK;
            }
        }
        if (rc == cps_api_ret_code_ERR) cps_api_object_delete(obj);
    }
    return rc;
}

cps_api_return_code_t nas_bridge_cps_publish_1q_object(NAS_DOT1Q_BRIDGE *p_bridge_node, cps_api_operation_types_t op)
{
    char buff[MAX_CPS_MSG_BUFF];
    memset(buff, 0, sizeof(buff));

    cps_api_object_t obj_pub = cps_api_object_init(buff, sizeof(buff));
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj_pub),
            BASE_IF_VLAN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_OBSERVED);

    cps_api_set_key_data(obj_pub,IF_INTERFACES_INTERFACE_NAME,
            cps_api_object_ATTR_T_BIN, p_bridge_node->bridge_name.c_str(),
            strlen(p_bridge_node->bridge_name.c_str())+1);
    cps_api_object_attr_add_u32(obj_pub,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, p_bridge_node->if_index);
    cps_api_object_attr_add_u32(obj_pub, BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, p_bridge_node->bridge_vlan_id);
    cps_api_object_attr_add_u32(obj_pub, DELL_IF_IF_INTERFACES_INTERFACE_VLAN_TYPE, p_bridge_node->bridge_sub_type);

    for (memberlist_t::iterator it = p_bridge_node->tagged_members.begin() ; it != p_bridge_node->tagged_members.end(); ++it) {
        //cps_api_object_attr_add(obj_pub, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS, (const void *)(*it->c_str()), strlen(*it->c_str())+1);
    }
    for (memberlist_t::iterator it = p_bridge_node->untagged_members.begin() ; it != p_bridge_node->untagged_members.end(); ++it) {
        //cps_api_object_attr_add(obj_pub, DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS, (const void *)(*it->c_str()), strlen(*it->c_str())+1);
    }
    cps_api_object_set_type_operation(cps_api_object_key(obj_pub), op);

    if (cps_api_event_thread_publish(obj_pub)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-BRIDGE", "Failed to send DOT-1Q publish event. Service issue");
        return cps_api_ret_code_ERR;
    }

    return cps_api_ret_code_OK;
}

cps_api_return_code_t nas_bridge_cps_publish_1d_object(NAS_DOT1D_BRIDGE *p_bridge_node, cps_api_operation_types_t op)
{
    char buff[MAX_CPS_MSG_BUFF];
    memset(buff, 0, sizeof(buff));

    cps_api_object_t obj_pub = cps_api_object_init(buff, sizeof(buff));
    cps_api_key_from_attr_with_qual(cps_api_object_key(obj_pub),
            BASE_IF_VLAN_IF_INTERFACES_INTERFACE_OBJ, cps_api_qualifier_OBSERVED);

    cps_api_set_key_data(obj_pub,IF_INTERFACES_INTERFACE_NAME,
            cps_api_object_ATTR_T_BIN, p_bridge_node->bridge_name.c_str(),
            strlen(p_bridge_node->bridge_name.c_str())+1);
    cps_api_object_attr_add_u32(obj_pub,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, p_bridge_node->if_index);

    for (memberlist_t::iterator it = p_bridge_node->tagged_members.begin() ; it != p_bridge_node->tagged_members.end(); ++it) {
        //cps_api_object_attr_add(obj_pub, DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS, (const void *)(*it->c_str()), strlen(*it->c_str())+1);
    }
    for (memberlist_t::iterator it = p_bridge_node->untagged_members.begin() ; it != p_bridge_node->untagged_members.end(); ++it) {
        //cps_api_object_attr_add(obj_pub, DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS, (const void *)(*it->c_str()), strlen(*it->c_str())+1);
    }
    cps_api_object_set_type_operation(cps_api_object_key(obj_pub), op);

    if (cps_api_event_thread_publish(obj_pub)!=STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR, "NAS-BRIDGE", "Failed to send DOT-1D publish event. Service issue");
        return cps_api_ret_code_ERR;
    }

    return cps_api_ret_code_OK;
}

t_std_error nas_bridge_cps_obj_init(cps_api_operation_handle_t handle)
{
    cps_api_registration_functions_t f;
    memset(&f, 0, sizeof(f));

    char buff[CPS_API_KEY_STR_MAX];
    if (!cps_api_key_from_attr_with_qual(&f.key, BRIDGE_DOMAIN_BRIDGE_OBJ, cps_api_qualifier_TARGET)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Could not translate to key %s", cps_api_key_print(&f.key,buff,sizeof(buff)-1));
        return STD_ERR(INTERFACE,FAIL,0);
    }

    f.handle = handle;
    f._read_function = bridge_get;
    f._write_function = bridge_set;

    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
        return STD_ERR(INTERFACE,FAIL,0);
    }
    return STD_ERR_OK;
}
