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
 */

/*
 * filename: nas_interface_bridge_utils.cpp
 */

#include "iana-if-type.h"
#include "dell-interface.h"
#include "dell-base-if-vlan.h"
#include "dell-base-if.h"
#include "bridge-model.h"
#include "nas_os_interface.h"
#include "bridge/nas_interface_bridge.h"
#include "bridge/nas_interface_bridge_utils.h"
#include "bridge/nas_interface_bridge_map.h"
#include "bridge/nas_interface_bridge_com.h"
#include "interface/nas_interface.h"
#include "interface/nas_interface_utils.h"

#include "nas_os_vlan.h"
#include <functional>
#include <utility>

typedef struct _mem {
    std::string name;
    nas_int_type_t type;
} mem_t;

typedef std::list<mem_t> list_t;


typedef std::unordered_map<hal_vlan_id_t, std::string>  vlan_to_bridge_map_t;

static vlan_to_bridge_map_t g_vlan_to_bridge_map;

bool nas_bridge_vlan_to_bridge_get(hal_vlan_id_t vlan_id, std::string &bridge_name) {
    auto it = g_vlan_to_bridge_map.find(vlan_id);
    if (it == g_vlan_to_bridge_map.end()) {
        return false;
    }
    bridge_name = it->second;
    return true;
}

bool nas_bridge_vlan_in_use(hal_vlan_id_t vlan_id)
{
    std::string bridge_name;
    return nas_bridge_vlan_to_bridge_get(vlan_id, bridge_name);
}

bool nas_bridge_vlan_to_bridge_map_add(hal_vlan_id_t vlan_id, std::string &bridge_name)
{
    auto it = g_vlan_to_bridge_map.find(vlan_id);
    if (it != g_vlan_to_bridge_map.end()) {
        // TODO Already present
        return false;
    }
    g_vlan_to_bridge_map[vlan_id] = bridge_name;
    return true;
}

bool nas_bridge_vlan_to_bridge_map_del(hal_vlan_id_t vlan_id) {
    auto it = g_vlan_to_bridge_map.find(vlan_id);
    if (it == g_vlan_to_bridge_map.end()) {
        // Not present
        return false;
    }
    g_vlan_to_bridge_map.erase(it);
    return true;
}

t_std_error nas_bridge_utils_if_bridge_exists(const char *name, NAS_BRIDGE **bridge_obj) {
    std::string _name(name);
    NAS_BRIDGE *br_obj;
    if (nas_bridge_map_obj_get(_name, &br_obj) == STD_ERR_OK) {
        if (bridge_obj != nullptr) *bridge_obj = br_obj;
        return STD_ERR_OK;
    }
    return STD_ERR(INTERFACE, FAIL, 0);
}

t_std_error nas_bridge_utils_vlan_id_get(const char * br_name, hal_vlan_id_t *vlan_id )
{
    NAS_BRIDGE *br_obj;
    if (nas_bridge_map_obj_get(std::string(br_name), &br_obj) != STD_ERR_OK) {
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    if (br_obj->bridge_mode_get() != BASE_IF_BRIDGE_MODE_1Q)  {
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    NAS_DOT1Q_BRIDGE *dot1q_bridge = dynamic_cast<NAS_DOT1Q_BRIDGE *>(br_obj);
    *vlan_id = dot1q_bridge->nas_bridge_vlan_id_get();
    return STD_ERR_OK;
}

t_std_error nas_bridge_utils_ifindex_get(const char * br_name, hal_ifindex_t *id )
{
    NAS_BRIDGE *br_obj;
    if (nas_bridge_map_obj_get(std::string(br_name), &br_obj) != STD_ERR_OK) {
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    *(id) = br_obj->if_index;
    return STD_ERR_OK;
}


t_std_error nas_bridge_utils_l3_mode_get(const char * br_name, BASE_IF_MODE_t *mode)
{
    if (mode == NULL) return STD_ERR(INTERFACE, FAIL, 0);

    NAS_BRIDGE *br_obj;
    if (nas_bridge_map_obj_get(std::string(br_name), &br_obj) != STD_ERR_OK) {
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    *mode = br_obj->bridge_l3_mode_get();
    return STD_ERR_OK;
}
t_std_error nas_bridge_utils_l3_mode_set(const char * br_name, BASE_IF_MODE_t mode)
{
    NAS_BRIDGE *br_obj;
    if (nas_bridge_map_obj_get(std::string(br_name), &br_obj) != STD_ERR_OK) {
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    br_obj->bridge_l3_mode_set(mode);
    return STD_ERR_OK;
}
t_std_error nas_bridge_utils_vlan_type_set(const char * br_name, BASE_IF_VLAN_TYPE_t vlan_type)
{
    NAS_BRIDGE *br_obj;
    if (nas_bridge_map_obj_get(std::string(br_name), &br_obj) != STD_ERR_OK) {
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    NAS_DOT1Q_BRIDGE *dot1q_bridge = dynamic_cast<NAS_DOT1Q_BRIDGE *>(br_obj);
    dot1q_bridge->nas_bridge_sub_type_set(vlan_type);
    return STD_ERR_OK;
}
t_std_error nas_bridge_utils_validate_bridge_mem_list(const char *br_name, std::list <std::string> &intf_list,
                                                        std::list <std::string>&mem_list)
{
    NAS_BRIDGE *br_obj;
    if (nas_bridge_map_obj_get(std::string(br_name), &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR,"NAS-INT", " Bridge obj does not Exists for %s", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    for (auto mem : intf_list) {
        bool present = false;
        if (br_obj->nas_bridge_check_membership(mem, &present) != STD_ERR_OK) {
            return STD_ERR(INTERFACE, FAIL, 0);
        }
        if (present) {
            mem_list.push_back(mem);
        }
    }
    return STD_ERR_OK;
}

t_std_error nas_bridge_utils_create_obj(const char *name, BASE_IF_BRIDGE_MODE_t br_type, hal_ifindex_t idx, NAS_BRIDGE **bridge_obj) {


/*  Create bridge object based on the mode type and save in the map */
    std::string _name(name);
    NAS_BRIDGE *br_obj;
    if (nas_bridge_map_obj_get(_name, &br_obj) == STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR,"NAS-INT", " Bridge obj already Exists");
        if (bridge_obj)   *bridge_obj = br_obj;
        return STD_ERR_OK;
    }

    if (br_type == BASE_IF_BRIDGE_MODE_1Q) {
        br_obj = (NAS_BRIDGE *)new NAS_DOT1Q_BRIDGE(_name, br_type, idx);
    } else if (br_type == BASE_IF_BRIDGE_MODE_1D) {
        br_obj = (NAS_BRIDGE *)new NAS_DOT1D_BRIDGE(_name, br_type, idx);
    } else {
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    if (bridge_obj != nullptr) {
        *bridge_obj = br_obj;
    }
    /*  save in the name to bridge object map */
    EV_LOGGING(INTERFACE,NOTICE,"NAS-INT", " Bridge obj created ");
    return (nas_bridge_map_obj_add(_name, br_obj));
}

t_std_error nas_create_bridge(const char *name, BASE_IF_BRIDGE_MODE_t br_type, hal_ifindex_t idx, NAS_BRIDGE **bridge_obj)
{

    t_std_error rc = STD_ERR_OK;

    NAS_BRIDGE *br_obj = nullptr;
    if ((rc = nas_bridge_utils_create_obj(name, br_type, idx, &br_obj)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to create bridge object, name %s", name);
        return rc;
    }
    if ((rc = br_obj->nas_bridge_npu_create()) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to create bridge in the NPU, name %s", name);
        return rc;
    }
    if (bridge_obj) *bridge_obj = br_obj;
    return rc;
}

t_std_error nas_bridge_create_vlan(const char *br_name, hal_vlan_id_t vlan_id, cps_api_object_t obj, NAS_BRIDGE **bridge_obj)
{
    t_std_error rc = STD_ERR_OK;
    hal_ifindex_t if_index;

    // TODO check if bridge exists in the kernel if yes then get the index otherwise create bridge in the kernel


    EV_LOGGING(INTERFACE, DEBUG, "NAS-BRIDGE-CREATE", "Create Bridge for %s ",br_name );
    // TODO change the name of API
    if (nas_os_add_vlan(obj, &if_index) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge delete failed in the OS %s", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    EV_LOGGING(INTERFACE, NOTICE, "NAS-BRIDGE-CREATE", "Create Bridge for %s in kernel Successful",br_name );
    cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,if_index);
    NAS_DOT1Q_BRIDGE *dot1q_br_obj;
    if ((rc = nas_bridge_utils_create_obj(br_name, BASE_IF_BRIDGE_MODE_1Q, if_index, (NAS_BRIDGE **)&dot1q_br_obj)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to create bridge object, name %s",br_name);
        return rc;
    }

    dot1q_br_obj->nas_bridge_vlan_id_set(vlan_id);
    dot1q_br_obj->set_bridge_model(INT_VLAN_MODEL);

    if ((rc = dot1q_br_obj->nas_bridge_npu_create()) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to create bridge in the NPU, name %s", br_name);
        return rc;
    }
    if (bridge_obj != nullptr)  *bridge_obj = dot1q_br_obj;
    EV_LOGGING(INTERFACE, INFO, "NAS-BRIDGE-CREATE", "Create Bridge for %s is Successful",br_name );
    return rc;

}
t_std_error nas_bridge_utils_delete_obj(NAS_BRIDGE *br_obj) {
    t_std_error rc = STD_ERR_OK;
    if ((rc = br_obj->nas_bridge_npu_delete()) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to delete bridge in the NPU");
        return rc;
    }
    delete br_obj;
    return rc;
}

/*  Delete bridge   deletes from NPU and clean-up L2 mode i nas-L3*/
t_std_error nas_bridge_utils_delete(const char *name)
{
    std::string _name(name);
    NAS_BRIDGE *br_obj = NULL;
    if ((nas_bridge_map_obj_remove(_name, &br_obj)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to get bridge obj for %s", name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    if (nas_intf_handle_intf_mode_change(br_obj->get_bridge_intf_index(), BASE_IF_MODE_MODE_L2) == false) {
        EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN", "Update to NAS-L3 about interface mode change failed(%s)",
                name);
    }
    if (br_obj->bridge_mode_get() == BASE_IF_BRIDGE_MODE_1Q) {
        NAS_DOT1Q_BRIDGE *dot1q_bridge = dynamic_cast<NAS_DOT1Q_BRIDGE *>(br_obj);
        hal_vlan_id_t vlan_id = dot1q_bridge-> nas_bridge_vlan_id_get();
        if (!nas_intf_cleanup_l2mc_config(0, vlan_id)) {
            EV_LOGGING(INTERFACE, ERR, "NAS-Vlan", "Error cleaning L2MC membership for VLAN %d", vlan_id);
        }
    }
    EV_LOGGING(INTERFACE,NOTICE,"NAS-INT", " Bridge delete from NPU");

    if ((nas_bridge_utils_delete_obj(br_obj)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to delete bridge obj for %s", name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return STD_ERR_OK;
}

/*  remove member from the bridge */

t_std_error nas_bridge_utils_npu_remove_member(const char *br_name, nas_int_type_t mem_type, const char *mem_name)
{

    /* Check the member type and bridge's current mode */
    std::string _br_name(br_name);
    NAS_BRIDGE *br_obj = nullptr;
    if (nas_bridge_map_obj_get(_br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge %s not present in the map ", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    std::string _mem_name = std::string(mem_name);
    EV_LOGGING(INTERFACE,NOTICE,"NAS-INT", " Bridge member remove %s member %s ", br_name, mem_name);
    if( br_obj->nas_bridge_npu_add_remove_member(_mem_name, mem_type, false) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge member addition failed ");
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return STD_ERR_OK;
}

static bool nas_check_bridge_1q_validity(NAS_DOT1D_BRIDGE *dot1d_br_obj)
{
    if (dot1d_br_obj->nas_bridge_vxlan_intf_present()) {
        /*  If it has vxlan member then return false */
        return false;
    } else if(dot1d_br_obj->nas_bridge_multiple_vlans_present()) {
        /*  IF it has multiple vlans attached return false */
        return false;
    }
    /*  TODO If members have different vlan ID then return false */
    return true;
}
t_std_error nas_bridge_utils_parent_bridge_get(const char *br_name, std::string &parent_bridge )
{
    std::string _br_name(br_name);
    NAS_BRIDGE *br_obj = nullptr;
    if (nas_bridge_map_obj_get(_br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge %s not present in the map ", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    parent_bridge = br_obj->bridge_parent_bridge_get();
    return STD_ERR_OK;
}

t_std_error nas_bridge_utils_parent_bridge_set(const char *br_name, std::string &parent_bridge )
{
    std::string _br_name(br_name);
    NAS_BRIDGE *br_obj = nullptr;
    if (nas_bridge_map_obj_get(_br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge %s not present in the map ", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    br_obj->bridge_parent_bridge_set(parent_bridge);
    return STD_ERR_OK;
}
/*  Get the list of members for a given port mode  */
t_std_error nas_bridge_utils_mem_list_get(const char * br_name, memberlist_t &mem_list,
                                            nas_port_mode_t port_mode)
{
    std::string _br_name(br_name);
    NAS_BRIDGE *br_obj = nullptr;
    if (nas_bridge_map_obj_get(_br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge %s not present in the map ", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return br_obj->nas_bridge_get_member_list(port_mode, mem_list);
}
t_std_error nas_bridge_utils_mem_list_get(NAS_BRIDGE *br_obj, list_t &mem_list)
{
    br_obj->nas_bridge_for_each_member([&mem_list](std::string mem_name, nas_port_mode_t port_mode) {
        /*  Remove the member from src_br_obj */
        t_std_error rc = STD_ERR_OK;
        nas_int_type_t mem_type;
        if ((rc = nas_get_int_name_type(mem_name.c_str(), &mem_type)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " NAS OS L2 PORT Event: Failed to get member type %s ", mem_name.c_str());
            return rc ;
        }
        mem_t _member = {mem_name, mem_type};
        mem_list.push_back(_member);
        return STD_ERR_OK;
    });
    return STD_ERR_OK;
}

/*  Migrate all tagged and untagged members  */
static t_std_error nas_npu_migrate_bridge_members(NAS_BRIDGE *dest_br_obj, NAS_BRIDGE *src_br_obj)
{
    /*  For each member of src_br_obj  */

    list_t mem_list;
    // TODO replace with  the utility function
    src_br_obj->nas_bridge_for_each_member([&mem_list](std::string mem_name, nas_port_mode_t port_mode) {
        /*  Remove the member from src_br_obj */
        t_std_error rc = STD_ERR_OK;
        nas_int_type_t mem_type;
        if ((rc = nas_get_int_name_type(mem_name.c_str(), &mem_type)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " NAS OS L2 PORT Event: Failed to get member type %s ", mem_name.c_str());
            return rc ;
        }
        mem_t _member = {mem_name, mem_type};
        mem_list.push_back(_member);
        return STD_ERR_OK;
    });

    for (auto _member:mem_list) {

        t_std_error rc = STD_ERR_OK;
        if ((rc = src_br_obj->nas_bridge_npu_add_remove_member(_member.name, _member.type, false)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to migrate members from %s to %s  member name %s ",
                           src_br_obj->get_bridge_name().c_str(),dest_br_obj->get_bridge_name().c_str(), _member.name.c_str());
            return rc;
        }

        /*  Add member in the dest_br_obj */
        if ((rc = dest_br_obj->nas_bridge_npu_add_remove_member(_member.name, _member.type, true)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to migrate members from %s to %s  member name %s ",
                           src_br_obj->get_bridge_name().c_str(),dest_br_obj->get_bridge_name().c_str(), _member.name.c_str());
            return rc;
        }
    }
    return STD_ERR_OK;

}
/*  Change bridge mode  */
t_std_error nas_bridge_utils_change_mode(const char *br_name, BASE_IF_BRIDGE_MODE_t br_mode)
{

    t_std_error rc = STD_ERR_OK;
    std::string _br_name(br_name);
    NAS_BRIDGE *br_obj = nullptr, *new_br_obj = nullptr;
    if (nas_bridge_map_obj_get(_br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge %s not present in the map ", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    if (br_mode == br_obj->bridge_mode) {
        EV_LOGGING(INTERFACE,INFO,"NAS-INT", " Bridge %s current mode is same as new mode ", br_name);
        return STD_ERR_OK;
    }
    if ((br_mode == BASE_IF_BRIDGE_MODE_1Q)  &&
        (nas_check_bridge_1q_validity((NAS_DOT1D_BRIDGE *)br_obj) == false)) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge %s Can't change mode to 1Q", br_name);
            return STD_ERR(INTERFACE, FAIL, 0);
    }
    if (nas_bridge_map_obj_remove(_br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge boject delete from map failed %s ", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    if (br_obj->nas_bridge_intf_cntrl_block_register(HAL_INTF_OP_DEREG) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Dot 1Q Bridge deregistration failed %s", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    /*      create .1d bridge object  */
    /*      Add new bridge in the map and  */
    hal_ifindex_t idx = br_obj->if_index;
    /* create bridge obj */
    if (nas_create_bridge(br_name, br_mode, idx, &new_br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge obj create failed %s", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    /*      migrate all members from .1q bridge object to .1d bridge object  in the NPU*/
    if ((rc = nas_npu_migrate_bridge_members(new_br_obj, br_obj)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge member migration failed for bridge %s ", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    /*      delete .1q bridge which deletes vlan in the npu */
    if ((rc = nas_bridge_utils_delete_obj(br_obj)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Dot 1Q Bridge obj delete failed %s", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    return STD_ERR_OK;
}

/*  Add member to a bridge  */
t_std_error nas_bridge_utils_npu_add_member(const char *br_name, nas_int_type_t mem_type, const char *mem_name)
{
    t_std_error rc = STD_ERR(INTERFACE, FAIL, 0);

    /* Check the member type and bridge's current mode */
    std::string _br_name(br_name);
    NAS_BRIDGE *br_obj = nullptr;
    NAS_DOT1D_BRIDGE *dot1d_br_obj = nullptr;
    if (nas_bridge_map_obj_get(_br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge %s not present in the map ", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    BASE_IF_BRIDGE_MODE_t mode = br_obj->bridge_mode;

    EV_LOGGING(INTERFACE,NOTICE,"NAS-INT", " Bridge member add %s  member %s", br_name, mem_name);
    /*  If bridge is .1q and member is vxlan then migrate to .1d bridge */
    if ((mode == BASE_IF_BRIDGE_MODE_1Q) && (mem_type == nas_int_type_VXLAN)) {

        if (br_obj->nas_bridge_intf_cntrl_block_register(HAL_INTF_OP_DEREG) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Dot 1Q Bridge deregistration failed %s", br_name);
            return STD_ERR(INTERFACE, FAIL, 0);

        }
        hal_ifindex_t idx = br_obj->if_index;
    /*      create .1d bridge object  */
    /*      Add new bridge in the map and  */
        if (nas_bridge_map_obj_remove(_br_name, &br_obj) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge boject delete from map failed %s ", br_name);
            return STD_ERR(INTERFACE, FAIL, 0);
        }

        if (nas_create_bridge(br_name, BASE_IF_BRIDGE_MODE_1D, idx, (NAS_BRIDGE **)&dot1d_br_obj) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge obj create failed %s", br_name);
            return STD_ERR(INTERFACE, FAIL, 0);
        }

    /*      migrate all members from .1q bridge object to .1d bridge object */
        if ((rc = nas_npu_migrate_bridge_members((NAS_BRIDGE *)dot1d_br_obj, br_obj)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge migration failed ");
            return STD_ERR(INTERFACE, FAIL, 0);
        }

        // Delete dot1q bridge object. publish event is not required.
        nas_bridge_utils_delete_obj(br_obj);

        br_obj = (NAS_BRIDGE *)dot1d_br_obj;
        br_obj->nas_bridge_publish_event(cps_api_oper_SET); // Publish event for mode change
    }
    /*      Add interface to the bridge */
    /*      call bridge function to add the member */
    std::string _mem_name = std::string(mem_name);
    if( br_obj->nas_bridge_npu_add_remove_member(_mem_name, mem_type, true) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge member addition failed ");
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return STD_ERR_OK;
}

static t_std_error nas_bridge_utils_os_add_remove_member(const char *br_name, nas_port_mode_t port_mode, const char *mem_name, bool add)

{
    t_std_error rc = STD_ERR_OK;
    NAS_BRIDGE *br_obj = nullptr;
    std::string _br_name(br_name);
    std::string _mem_name(mem_name);
    if (nas_bridge_map_obj_get(_br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge %s not present in the map ", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    if ((rc = br_obj->nas_bridge_os_add_remove_member(_mem_name, port_mode, add)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " NAS OS L2 PORT Event: Failed to add member %s ", mem_name);
        return rc ;
    }
    return STD_ERR_OK;
}

/*  Add a member in the bridge in kernel as well as in the NPU */
t_std_error nas_bridge_utils_add_member(const char *br_name, const char *mem_name)
{
    t_std_error rc = STD_ERR_OK;
    nas_int_type_t mem_type;
    if ((rc = nas_get_int_name_type(mem_name, &mem_type)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " Failed to get the member type %s ", mem_name);
        return rc ;
    }
    nas_port_mode_t port_mode = NAS_PORT_UNTAGGED;
    if (mem_type == nas_int_type_VLANSUB_INTF) {
        port_mode = NAS_PORT_UNTAGGED;
    }

    if ((rc = nas_bridge_utils_os_add_remove_member(br_name, port_mode, mem_name, true)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " Failed to add member  in the kernel %s ", mem_name);
        return rc;
    }
    if ((rc = nas_bridge_utils_npu_add_member(br_name, mem_type, mem_name)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " NAS Failed to add member in the NPU %s ", mem_name);
        // Remove from the kernel in case of failure
        nas_bridge_utils_os_add_remove_member(br_name, port_mode, mem_name, false);
        return rc;
    }
    return STD_ERR_OK;
}
/*  Add a member in the bridge in kernel as well as in the NPU */
t_std_error nas_bridge_utils_remove_member(const char *br_name, const char *mem_name)
{
    t_std_error rc = STD_ERR_OK;
    nas_int_type_t mem_type;
    if ((rc = nas_get_int_name_type(mem_name, &mem_type)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " Failed to get the member type %s ", mem_name);
        return rc ;
    }
    nas_port_mode_t port_mode = NAS_PORT_UNTAGGED;
    if (mem_type == nas_int_type_VLANSUB_INTF) {
        port_mode = NAS_PORT_TAGGED;
    }

    if ((rc = nas_bridge_utils_npu_remove_member(br_name, mem_type, mem_name)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " NAS Failed to remove member %s in the NPU from bridge %s ",
                            mem_name, br_name);
        return rc;
    }
    if ((rc = nas_bridge_utils_os_add_remove_member(br_name, port_mode, mem_name, false)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " Failed to remove member %s in the kernel from bridge %s ",
                            mem_name, br_name);
        // Add it back in case of failure
         nas_bridge_utils_npu_add_member(br_name, mem_type, mem_name);
        return rc;
    }
    return STD_ERR_OK;
}

/* This publish is for nas internal components like nas-l2 */

static t_std_error
nas_bridge_utils_internal_pub_mem_update(const char *br_name, memberlist_t &list, nas_port_mode_t port_mode,  cps_api_operation_types_t op )
{
    cps_api_attr_id_t id =0;
    if (list.empty()) {
       return STD_ERR_OK;
    }

    cps_api_object_guard og(cps_api_object_create());
    if (og.get()==nullptr) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", "Mem publish for bridge %s: failed to create a new CPS object", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    std::string _br_name(br_name);
    NAS_BRIDGE *br_obj = nullptr;
    if (nas_bridge_map_obj_get(_br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge %s not present in the map ", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    hal_ifindex_t ifindex =  br_obj->get_bridge_intf_index();
    NAS_DOT1Q_BRIDGE *dot1q_bridge = dynamic_cast<NAS_DOT1Q_BRIDGE *>(br_obj);
    hal_vlan_id_t vlan_id = dot1q_bridge->nas_bridge_vlan_id_get();
    if(port_mode == NAS_PORT_TAGGED) {
        id = DELL_IF_IF_INTERFACES_INTERFACE_TAGGED_PORTS;
    }
    else {
        id = DELL_IF_IF_INTERFACES_INTERFACE_UNTAGGED_PORTS;
    }

    cps_api_key_from_attr_with_qual(cps_api_object_key(og.get()), id,
                                    cps_api_qualifier_OBSERVED);

    cps_api_set_key_data(og.get() ,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX, cps_api_object_ATTR_T_U32,
                         &ifindex, sizeof(hal_ifindex_t));

    cps_api_object_set_type_operation(cps_api_object_key(og.get()),op);
    hal_ifindex_t mem_ifindex = 0;
    std::string intf_name;
    char _intf_name[HAL_IF_NAME_SZ];

    for (auto mem : list) {
        if (port_mode == NAS_PORT_TAGGED) {
            memset(_intf_name, 0, sizeof(_intf_name));
            safestrncpy(_intf_name, mem.c_str(), sizeof(_intf_name));
            const char *_parent = strtok(_intf_name,".");
            if(_parent != nullptr){
               intf_name = std::string(_parent);
               EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Mem_publish :parent intf %s",  intf_name.c_str());
            } else {
               EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Mem_publish :failed to get parent for  %s",  _intf_name);
               return STD_ERR(INTERFACE, FAIL, 0);
            }

        } else {
            intf_name = mem;
        }
        if ( nas_int_name_to_if_index(&mem_ifindex, intf_name.c_str()) == STD_ERR_OK) {
            cps_api_object_attr_add_u32(og.get(), id, mem_ifindex);
        } else {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Mem_publish :failed to get index for intf %s",  intf_name);
        }
     }
    cps_api_object_attr_add_u32(og.get(), BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID, vlan_id);
    cps_api_event_thread_publish(og.get());
    return STD_ERR_OK;

}

t_std_error nas_bridge_utils_update_member_list(const char *br_name, memberlist_t &new_list,
                                                                memberlist_t &cur_list , nas_port_mode_t port_mode)
{

    t_std_error rc = STD_ERR_OK;
    intf_list_t add_list, remove_list;
    // Get the bridge object
    std::string _br_name(br_name);
    NAS_BRIDGE *br_obj = nullptr;
    if (nas_bridge_map_obj_get(_br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge %s not present in the map ", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    /*  Get the current list  */
    if (cur_list.empty()) {
        br_obj->nas_bridge_get_member_list(port_mode, cur_list);
    }

    // TODO check if either new_list or cur_list if empty
    // find out the members to be added
    //
    for (auto mem : new_list) {
        auto it = cur_list.find(mem);
        if (it == cur_list.end()) {
            add_list.insert(mem);
        }
    }
    // find out the members to be removed by looking for the
    // mmebers from current list not present in the new list
    for (auto mem : cur_list) {
        auto it = new_list.find(mem);
        if (it == new_list.end()) {
            remove_list.insert(mem);
        }
    }
    // In case if this vlan bridge attached to a parent bridge which is of 1D type then add all members to
    // the the parent bridge only. on the vlan bridge, just update the lsit
    bool add_to_parent = false;
    NAS_BRIDGE *oper_br_obj = br_obj;
    if (br_obj->bridge_is_parent_bridge_exists()) {
        std::string p_bridge = br_obj->bridge_parent_bridge_get();
        NAS_BRIDGE *p_br_obj = nullptr;
        if (nas_bridge_map_obj_get(p_bridge.c_str(), &p_br_obj) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " parent Bridge %s not present in the map ", p_bridge.c_str());
            return STD_ERR(INTERFACE, FAIL, 0);
        }
        if (p_br_obj->bridge_mode_get() == BASE_IF_BRIDGE_MODE_1D) {
            oper_br_obj = p_br_obj;
            add_to_parent = true;
        }
    }

    // if tagged mode then first create all sub interfaces if do not exists
    // Get the vlan id
    bool  in_os = true;
    if (port_mode == NAS_PORT_TAGGED) {
        if ((nas_g_scaled_vlan_get() == true) &&
            (oper_br_obj->bridge_l3_mode_get() != BASE_IF_MODE_MODE_L3)) {
            in_os = false;
        }
        NAS_DOT1Q_BRIDGE *dot1q_bridge = dynamic_cast<NAS_DOT1Q_BRIDGE *>(br_obj);
        hal_vlan_id_t vlan_id = dot1q_bridge->nas_bridge_vlan_id_get();

        if (nas_interface_vlan_subintf_list_create(add_list, vlan_id, in_os) != STD_ERR_OK) {
             EV_LOGGING(INTERFACE,ERR,"INT-DB-GET","Failed to create  a memberlist to the bridge %s ",
                                        br_name);
             return rc;
        }
    }

    /* Add the addition list first */
    if (!add_list.empty()) {
        if ((rc = oper_br_obj->nas_bridge_add_remove_memberlist(add_list , port_mode, true)) !=
                                STD_ERR_OK) {
         EV_LOGGING(INTERFACE,ERR,"INT-DB-GET","Failed to add a memberlist to the bridge %s ",
                 br_name);
         return rc;
        }
    }
    if (!remove_list.empty()) {
        if ((rc = oper_br_obj->nas_bridge_add_remove_memberlist(remove_list , port_mode, false)) !=
                                STD_ERR_OK) {
          // rollback add_list
            if(!add_list.empty())  {
                oper_br_obj->nas_bridge_add_remove_memberlist(add_list , port_mode, false);
            }
            EV_LOGGING(INTERFACE,ERR,"INT-DB-GET","Failed to add a memberlist to the bridge %s ",
                 br_name);
            return rc;
        }
    }
    // Now just update the members on the vlan bridge
    if (add_to_parent) {
        br_obj->nas_bridge_update_member_list(add_list, port_mode, true);
        br_obj->nas_bridge_update_member_list(remove_list, port_mode, false);
        /*  Publish the event against parent bridge as well */
        oper_br_obj->nas_bridge_publish_memberlist_event(add_list, cps_api_oper_CREATE);
        oper_br_obj->nas_bridge_publish_memberlist_event(remove_list, cps_api_oper_DELETE);
    }

    /*  remove sub interface list which are removed   */
    if (port_mode == NAS_PORT_TAGGED) {
        if ((rc = nas_interface_vlan_subintf_list_delete(remove_list)) != STD_ERR_OK) {
             EV_LOGGING(INTERFACE,ERR,"INT-DB-GET","Failed to delete  a memberlist to the bridge %s ",
                                        br_name);
             return rc;
        }
    }
    // Publish added and removed members
    nas_bridge_utils_internal_pub_mem_update(br_name, add_list,  port_mode, cps_api_oper_CREATE);
    nas_bridge_utils_internal_pub_mem_update(br_name, remove_list ,port_mode, cps_api_oper_DELETE);
    return STD_ERR_OK;
}

static t_std_error nas_bridge_migrate_bridge_members(NAS_BRIDGE *src_br_obj, NAS_BRIDGE *dst_br_obj,
                                                memberlist_t &tagged_list, memberlist_t &untagged_list)
{
    EV_LOGGING(INTERFACE,INFO,"BRIDGE-MIGRATE", "Migrate Bridge Members");
    if ((src_br_obj == nullptr ) || (dst_br_obj == nullptr))  { return STD_ERR(INTERFACE, FAIL, 0); }

    if (tagged_list.empty() && untagged_list.empty()) { return STD_ERR_OK; }

    if (src_br_obj->nas_bridge_add_remove_memberlist(tagged_list, NAS_PORT_TAGGED, false) != STD_ERR_OK)  {
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    if (src_br_obj->nas_bridge_add_remove_memberlist(untagged_list, NAS_PORT_UNTAGGED, false) != STD_ERR_OK)  {
        src_br_obj->nas_bridge_add_remove_memberlist(tagged_list, NAS_PORT_TAGGED, true);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
        // Now add members to the parent bridge
    if (dst_br_obj->nas_bridge_add_remove_memberlist(tagged_list, NAS_PORT_TAGGED, true) != STD_ERR_OK)  {
        src_br_obj->nas_bridge_add_remove_memberlist(tagged_list, NAS_PORT_TAGGED, true);
        src_br_obj->nas_bridge_add_remove_memberlist(untagged_list, NAS_PORT_UNTAGGED, true);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    if (dst_br_obj->nas_bridge_add_remove_memberlist(untagged_list, NAS_PORT_UNTAGGED, true) != STD_ERR_OK)  {
        dst_br_obj->nas_bridge_add_remove_memberlist(tagged_list, NAS_PORT_TAGGED, false);
        src_br_obj->nas_bridge_add_remove_memberlist(untagged_list, NAS_PORT_UNTAGGED, true);
        src_br_obj->nas_bridge_add_remove_memberlist(tagged_list, NAS_PORT_TAGGED, true);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return STD_ERR_OK;
}
t_std_error nas_bridge_utils_attach_vlan(const char *vlan_intf, const char *parent_bridge)
{

    /* Get the objects of vlan_obj and p_bridge_obj */
    EV_LOGGING(INTERFACE,INFO,"VLAN-ATTACH"," Attach Vlan %s to parent Bridge %s ",vlan_intf,parent_bridge);
    NAS_BRIDGE *p_br_obj = nullptr;
    if (nas_bridge_map_obj_get(std::string(parent_bridge), &p_br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Parent Bridge %s not present in the map ", parent_bridge);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    NAS_BRIDGE *vlan_br_obj = nullptr;
    if (nas_bridge_map_obj_get(std::string(vlan_intf), &vlan_br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Vlan Bridge %s not present in the map ", vlan_intf);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    // Do validation
    // TODO return error if not in the L3 mode
    if (vlan_br_obj->bridge_is_parent_bridge_exists())  {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " parent Bridge already  present for %s ", vlan_intf);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    //
//   if parent bridge is 1Q and it does not have any member
//      just mark the parent bridge and leave and update bridge object
    if (p_br_obj->bridge_mode_get() == BASE_IF_BRIDGE_MODE_1Q) {
        EV_LOGGING(INTERFACE,INFO,"NAS-BRIDGE", " parent Bridge is in 1Q mode");
        /*  update the parent bridge in the vlan object and add vlan in the parent's vlan list */
        std::string p_bridge(parent_bridge);
        vlan_br_obj->bridge_parent_bridge_set(p_bridge);
        p_br_obj->nas_bridge_remove_vlan_member_from_attached_list(std::string(vlan_intf));
        return STD_ERR_OK;
    }

    // Update bridge record
    std::string p_bridge(parent_bridge);
    vlan_br_obj->bridge_parent_bridge_set(p_bridge);
    p_br_obj->nas_bridge_add_vlan_in_attached_list(std::string(vlan_intf));
    NAS_DOT1D_BRIDGE * dot1d_br_obj = dynamic_cast<NAS_DOT1D_BRIDGE *>(p_br_obj);
    NAS_DOT1Q_BRIDGE * dot1q_br_obj = dynamic_cast<NAS_DOT1Q_BRIDGE *>(vlan_br_obj);
    dot1d_br_obj->nas_bridge_untagged_vlan_id_set(dot1q_br_obj->nas_bridge_vlan_id_get());

//     Get the complete member list
    memberlist_t tagged_list, untagged_list;
    vlan_br_obj->nas_bridge_get_member_list(NAS_PORT_TAGGED, tagged_list);
    vlan_br_obj->nas_bridge_get_member_list(NAS_PORT_UNTAGGED, untagged_list);

    if (!tagged_list.empty() || !untagged_list.empty()) {
        if(nas_bridge_migrate_bridge_members(vlan_br_obj, p_br_obj, tagged_list, untagged_list) != STD_ERR_OK)  {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Failed to move vlan %s members to bridge %s ",vlan_intf, parent_bridge);
        }
        //     Add the members back in to the vlan bridge memberlist and
        //     publish member addition into the 1D bridge.
        if (!tagged_list.empty()) {
            vlan_br_obj->nas_bridge_update_member_list(tagged_list, NAS_PORT_TAGGED, true);
            p_br_obj->nas_bridge_publish_memberlist_event(tagged_list, cps_api_oper_CREATE);
        }

        if (!untagged_list.empty()) {
            vlan_br_obj->nas_bridge_update_member_list(untagged_list, NAS_PORT_UNTAGGED, true);
            p_br_obj->nas_bridge_publish_memberlist_event(untagged_list, cps_api_oper_CREATE);
        }

    }

    return STD_ERR_OK;
}

t_std_error nas_bridge_utils_detach_vlan(const char *vlan_intf, const char *parent_bridge)
{
    /* Get the objects of vlan_obj and p_bridge_obj */
    EV_LOGGING(INTERFACE,INFO,"VLAN-ATTACH", " Detach Vlan %s from parent Bridge %s ", vlan_intf,parent_bridge);
    NAS_BRIDGE *vlan_br_obj = nullptr;
    std::string p_bridge(parent_bridge);
    if (nas_bridge_map_obj_get(std::string(vlan_intf), &vlan_br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge %s not present in the map ", vlan_intf);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    NAS_BRIDGE *p_br_obj = nullptr;
    if (nas_bridge_map_obj_get(std::string(parent_bridge), &p_br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Parent Bridge %s not present in the map ", parent_bridge);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    if (vlan_br_obj->bridge_is_parent_bridge(p_bridge) == false)  {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " parent Bridge for %s is not present or not same as %s ",
                             vlan_intf, parent_bridge);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    // TODO Do validation
    //
//   if parent bridge is 1Q and it does not have any member
//      just mark the parent bridge and leave and update bridge object
    if (p_br_obj->bridge_mode_get() == BASE_IF_BRIDGE_MODE_1Q) {
        /*  update the parent bridge in the vlan object and add vlan in the parent's vlan list */
        vlan_br_obj->bridge_parent_bridge_clear();
        p_br_obj->nas_bridge_remove_vlan_member_from_attached_list(std::string(vlan_intf));
        return STD_ERR_OK;
    }

//     Get the complete member list
    memberlist_t tagged_list, untagged_list;
    vlan_br_obj->nas_bridge_get_member_list(NAS_PORT_TAGGED, tagged_list);
    vlan_br_obj->nas_bridge_get_member_list(NAS_PORT_UNTAGGED, untagged_list);

    // remove all tagged and untagged members from the list. It will be added as part of migration
    vlan_br_obj->nas_bridge_memberlist_clear();
    if(nas_bridge_migrate_bridge_members(p_br_obj, vlan_br_obj, tagged_list, untagged_list) != STD_ERR_OK)  {
        EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Failed to move vlan %s members from bridge %s ",vlan_intf, parent_bridge);
    }

    //     publish member addition into the 1D bridge.
    p_br_obj->nas_bridge_publish_memberlist_event(tagged_list, cps_api_oper_DELETE);
    p_br_obj->nas_bridge_publish_memberlist_event(untagged_list, cps_api_oper_DELETE);

    // Update bridge record
    vlan_br_obj->bridge_parent_bridge_clear();
    p_br_obj->nas_bridge_remove_vlan_member_from_attached_list(std::string(vlan_intf));
    NAS_DOT1D_BRIDGE * dot1d_br_obj = dynamic_cast<NAS_DOT1D_BRIDGE *>(p_br_obj);
    if (dot1d_br_obj) dot1d_br_obj->nas_bridge_untagged_vlan_id_set(DEFAULT_UNTAGGED_VLAN_ID);
    return STD_ERR_OK;
}
t_std_error nas_bridge_utils_get_remote_endpoint_stats(const char *vxlan_intf_name, hal_ip_addr_t & remote_ip,
                                ndi_stat_id_t *ndi_stat_ids, uint64_t* stats_val, size_t len){
    /*TODO  Add vxlan lock    */
    t_std_error rc = STD_ERR(INTERFACE, FAIL, 0);

    /*  Get vxlan_obj from name  */

    NAS_VXLAN_INTERFACE *vxlan_obj = (NAS_VXLAN_INTERFACE *)nas_interface_map_obj_get(std::string(vxlan_intf_name));

    if (vxlan_obj == nullptr) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Vxlan stat get for %s failed",vxlan_intf_name);
        return rc;
    }

    remote_endpoint_t remote_endpoint;
    remote_endpoint.remote_ip = remote_ip;

    std::string bridge_name = vxlan_obj->get_bridge_name();
    hal_ip_addr_t source_ip = vxlan_obj->source_ip;
    NAS_BRIDGE *bridge_obj = nullptr;
    /*  TODO Add bridge lock */

        /*  vxlan is associated with a bridge */
    if (nas_bridge_map_obj_get(bridge_name, &bridge_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Vxlan Interface %s is not part of the bridge ",vxlan_intf_name);
        return rc;
    }
    NAS_DOT1D_BRIDGE *dot1d_bridge = dynamic_cast<NAS_DOT1D_BRIDGE *>(bridge_obj);

    if ( (rc= dot1d_bridge->nas_bridge_get_remote_endpoint_stats(source_ip, &remote_endpoint,
                        ndi_stat_ids,stats_val,len)) != STD_ERR_OK) {
        return rc;
    }

    return STD_ERR_OK;

}

static t_std_error nas_bridge_utils_set_remote_endpoint_attr(NAS_VXLAN_INTERFACE *vxlan_obj,
        remote_endpoint_t & new_rem_ep, remote_endpoint_t & cur_rem_ep)
{

    t_std_error rc = STD_ERR(INTERFACE, FAIL, 0);

    /*  Copy tunnel Id  */
    new_rem_ep.tunnel_id = cur_rem_ep.tunnel_id;

    if(cur_rem_ep.mac_learn_mode != new_rem_ep.mac_learn_mode){
        if((rc = vxlan_obj->nas_interface_set_mac_learn_remote_endpt(&new_rem_ep))!=STD_ERR_OK){
            return rc;
        }
    }

    if(cur_rem_ep.flooding_enabled != new_rem_ep.flooding_enabled){
        NAS_BRIDGE *bridge_obj = nullptr;
        if ((rc = nas_bridge_map_obj_get(vxlan_obj->bridge_name, &bridge_obj)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "No bridge object for %s exsist",
                                    vxlan_obj->bridge_name.c_str());
            return rc;
        }
        NAS_DOT1D_BRIDGE *dot1d_bridge = dynamic_cast<NAS_DOT1D_BRIDGE *>(bridge_obj);
        if((rc = dot1d_bridge->nas_bridge_set_flooding(&new_rem_ep))!=STD_ERR_OK){
            return rc;
        }
    }
    return STD_ERR_OK;
}

/*  Add remote endpoint to a vxlan interface */

t_std_error nas_bridge_utils_add_remote_endpoint(const char *vxlan_intf_name,remote_endpoint_t & rem_ep) {
    /*TODO  Add vxlan lock    */
    t_std_error rc = STD_ERR(INTERFACE, FAIL, 0);
    /*  Get vxlan_obj from name  */
    std::string _if_name = std::string(vxlan_intf_name);
    NAS_VXLAN_INTERFACE *vxlan_obj = (NAS_VXLAN_INTERFACE *)nas_interface_map_obj_get(_if_name);
    if (vxlan_obj == nullptr) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge mmber addition failed ");
        return rc;
    }


    /*  Add the remote endpoint to the vxlan */
    /*  Check if vxlan has 1d bridge associated */
    /*  IF yes  then Get the bridge object and call
     *  bridge method to create remote endpoint tunnel
     *  If flooding enable then add the remote endpoint to the L2MC flooding group
     *  */

    remote_endpoint_t remote_endpoint_entry;
    remote_endpoint_entry.remote_ip = rem_ep.remote_ip;

    bool entry_exist = false;
    bool tunnel_exist = false;

    if (vxlan_obj->nas_interface_get_remote_endpoint(&remote_endpoint_entry) == STD_ERR_OK) {
        entry_exist = true;
        /* If tunnel exists just set the flooding in NDI  and update flooding in remote_endpt_data */
        if (remote_endpoint_entry.tunnel_id != NAS_INVALID_TUNNEL_ID) {
            tunnel_exist = true;
        }
    }

    /*  Case where remote endpoint is created by interface object but further add is coming from an event
     *  from NAS-MAC then simply return */
    if ((entry_exist) && ((remote_endpoint_entry.rem_membership) && (!rem_ep.rem_membership))) {
        EV_LOGGING(INTERFACE,INFO ,"NAS-BRIDGE", " remote member is added by interface object and add is from MAC event.");
        return STD_ERR_OK;
    }
    std::string bridge_name = vxlan_obj->get_bridge_name();
    bool tunnel_created = false;
    if (bridge_name.empty()) {
        if (!entry_exist) {
            vxlan_obj->nas_interface_add_remote_endpoint(&rem_ep);
        } else {
            vxlan_obj->nas_interface_update_remote_endpoint(&rem_ep);
        }
        vxlan_obj->nas_interface_publish_remote_endpoint_event(&rem_ep, cps_api_oper_CREATE, tunnel_created);
        // The vxlan interface is not yet associated to any bridge. Return from here.
        return STD_ERR_OK;
    }

    uint32_t vni = vxlan_obj->vni;
    hal_ip_addr_t source_ip = vxlan_obj->source_ip;
    NAS_BRIDGE *bridge_obj = nullptr;

    /*  vxlan is associated with a bridge */
    if (nas_bridge_map_obj_get(bridge_name, &bridge_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge mmber addition failed ");
        return rc;
    }
    NAS_DOT1D_BRIDGE *dot1d_bridge = dynamic_cast<NAS_DOT1D_BRIDGE *>(bridge_obj);
    if (dot1d_bridge == nullptr) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE",
                " remote endpoint addition failed:bridge object not found %s ",
                bridge_name.c_str());
        return rc;
    }
    if (tunnel_exist == true) {
        if ( (rc= nas_bridge_utils_set_remote_endpoint_attr(vxlan_obj, rem_ep,remote_endpoint_entry)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " remote endpoint attribute setting failed for br %s",
                 bridge_name.c_str());
            return rc;
        }
        vxlan_obj->nas_interface_update_remote_endpoint(&rem_ep);
        return STD_ERR_OK;

    }
    if ( (rc= dot1d_bridge->nas_bridge_add_remote_endpoint(vni, source_ip, &rem_ep)) !=
                                          STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " remote endpoint addition failed for br %s ",
                  bridge_name.c_str());
        return rc;
    }
    tunnel_created = true;
    if (!entry_exist) {
       vxlan_obj->nas_interface_add_remote_endpoint(&rem_ep);
    } else {
       vxlan_obj->nas_interface_update_remote_endpoint(&rem_ep);
    }
    vxlan_obj->nas_interface_publish_remote_endpoint_event(&rem_ep, cps_api_oper_CREATE, tunnel_created);
    // TODO publish  remote endpoint addition
    return STD_ERR_OK;
}

/*  Add remote endpoint to a vxlan interface */
t_std_error nas_bridge_utils_update_remote_endpoint(const char *vxlan_intf_name,
                                                   remote_endpoint_t & rem_ep) {
    /*TODO  Add vxlan lock */
    t_std_error rc = STD_ERR(INTERFACE, FAIL, 0);

    std::string _if_name = std::string(vxlan_intf_name);
    NAS_VXLAN_INTERFACE *vxlan_obj = (NAS_VXLAN_INTERFACE *)nas_interface_map_obj_get(_if_name);
    if (vxlan_obj == nullptr) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", "No VXLAN interface %s found",vxlan_intf_name);
        return rc;
    }

    remote_endpoint_t cur_rem_ep;
    cur_rem_ep.remote_ip = rem_ep.remote_ip;

    if (vxlan_obj->nas_interface_get_remote_endpoint(&cur_rem_ep) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-VXLAN","Failed to get remote endpoint for updating");
        return rc;
    }

    /*  Case where remote endpoint is created by interface object but further add is coming from an event
     *  from NAS-MAC then simply return */
    if ((cur_rem_ep.rem_membership) && (!rem_ep.rem_membership)) {
        EV_LOGGING(INTERFACE,INFO ,"NAS-BRIDGE", " remote member is added by interface object but update is from MAC event.");
        return STD_ERR_OK;
    }
    if (vxlan_obj->bridge_name.empty()) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE","Vxlan interface %s is not part of any bridge",vxlan_intf_name);
        return rc;
    }

    if ( (rc= nas_bridge_utils_set_remote_endpoint_attr(vxlan_obj, rem_ep, cur_rem_ep)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " remote endpoint attribute setting failed for vxlan intf %s",
             vxlan_intf_name);
        return rc;
    }
    vxlan_obj->nas_interface_update_remote_endpoint(&rem_ep);
    vxlan_obj->nas_interface_publish_remote_endpoint_event(&rem_ep, cps_api_oper_SET, false);

    return STD_ERR_OK;
}

/*  remove a remote endpoint from a vxlan interface */
t_std_error nas_bridge_utils_remove_remote_endpoint(const char *vxlan_intf_name, remote_endpoint_t & rem_ep)
{
    /*TODO  Add vxlan lock    */
    t_std_error rc = STD_ERR(INTERFACE, FAIL, 0);
    /*  Get vxlan_obj from name  */
    std::string _if_name = std::string(vxlan_intf_name);
    NAS_VXLAN_INTERFACE *vxlan_obj = (NAS_VXLAN_INTERFACE *)nas_interface_map_obj_get(_if_name);
    if (vxlan_obj == nullptr) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " remote enpoint removal failed  vxlan interface not found %s", vxlan_intf_name);
        return rc;
    }

    char buff[HAL_INET6_TEXT_LEN + 1];
    std_ip_to_string((const hal_ip_addr_t*) &rem_ep.remote_ip, buff, HAL_INET6_TEXT_LEN);
    EV_LOGGING(INTERFACE,DEBUG,"NAS-BRIDGE", "Remove remote endpoint :vxlan_intf %s, remote ip-address %s", vxlan_intf_name, buff);
    remote_endpoint_t cur_rem_ep;
    cur_rem_ep.remote_ip = rem_ep.remote_ip;
    if (vxlan_obj->nas_interface_get_remote_endpoint(&cur_rem_ep) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,DEBUG,"NAS-BRIDGE", "Remove remote endpoint : not present vxlan_intf %s, remote ip-address %s", vxlan_intf_name, buff);
        return STD_ERR_OK;
    }
    /*  Case where remote endpoint is created by interface object but remove is coming from an event
     *  from NAS-MAC upon final remote MAC deletion then do not delete the remote mac simply return */
    if ((cur_rem_ep.rem_membership) && (!rem_ep.rem_membership)) {
        EV_LOGGING(INTERFACE,INFO ,"NAS-BRIDGE", " remote member is added by interface object but delete is from MAC event.");
        return STD_ERR_OK;
    }
    vxlan_obj->nas_interface_remove_remote_endpoint(&rem_ep);

    /*  Check if vxlan has 1d bridge associated */
    /*  IF yes  then Get the bridge object and call
     *  bridge method to create remote endpoint tunnel
     *  If flooding enable then add the remote endpoint to the L2MC flooding group
     *  */

    std::string bridge_name = vxlan_obj->get_bridge_name();
    if (bridge_name.empty()) {
        // The vxlan interface is not yet associated to any bridge. Return from here.
        vxlan_obj->nas_interface_publish_remote_endpoint_event(&rem_ep, cps_api_oper_DELETE, false);
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " No bridge associated with vxlan");
        return STD_ERR_OK;
    }

    uint32_t vni = vxlan_obj->vni;
    hal_ip_addr_t source_ip = vxlan_obj->source_ip;
    NAS_BRIDGE *bridge_obj = nullptr;
    /*  TODO Add bridge lock */

    /*  vxlan is associated with a bridge */
    if (nas_bridge_map_obj_get(vxlan_obj->bridge_name, &bridge_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge mmber addition failed ");
        return rc;
    }

    NAS_DOT1D_BRIDGE *dot1d_bridge = dynamic_cast<NAS_DOT1D_BRIDGE *> (bridge_obj);
    if (dot1d_bridge == nullptr) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE",
                " remote endpoint removal failed:bridge object not found %s ",
                vxlan_obj->bridge_name.c_str());
        return rc;
    }

    if ( (rc= dot1d_bridge->nas_bridge_remove_remote_endpoint(vni, source_ip, &rem_ep)) !=
                                          STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " remote endpoint removal failed ");
        return rc;
    }

    vxlan_obj->nas_interface_publish_remote_endpoint_event(&rem_ep, cps_api_oper_DELETE, true);

    return STD_ERR_OK;
}

// Publish bridge create/delete event with bridge name, bridge mode and operation type.
void nas_bridge_utils_publish_event(const char * bridge_name, cps_api_operation_types_t op)
{
    cps_api_object_guard og(cps_api_object_create());

    if (!cps_api_key_from_attr_with_qual(cps_api_object_key(og.get()),
                BRIDGE_DOMAIN_BRIDGE_OBJ, cps_api_qualifier_OBSERVED)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF","Could not translate to logical interface key ");
        return;
    }
    cps_api_object_set_type_operation(cps_api_object_key(og.get()), op);
    cps_api_set_key_data(og.get(),BRIDGE_DOMAIN_BRIDGE_NAME,
                               cps_api_object_ATTR_T_BIN, bridge_name, strlen(bridge_name)+1);
    if (op == cps_api_oper_CREATE) {
      if (nas_bridge_fill_info(std::string(bridge_name), og.get()) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF","Failed to add Bridge info");
      }
    }
    cps_api_event_thread_publish(og.get());
}

void nas_bridge_utils_publish_member_event(std::string bridge_name, std::string mem_name,
        cps_api_operation_types_t op)
{
    NAS_BRIDGE *bridge_obj = nullptr;

    /*  vxlan is associated with a bridge */
    if (nas_bridge_map_obj_get(bridge_name, &bridge_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge member get failed ");
        return;
    }
    bridge_obj->nas_bridge_publish_member_event(mem_name, op);
}

void nas_bridge_utils_publish_memberlist_event(std::string bridge_name, memberlist_t memlist,
        cps_api_operation_types_t op)
{
    NAS_BRIDGE *bridge_obj = nullptr;
    /*  TODO Add bridge lock */

    /*  vxlan is associated with a bridge */
    if (nas_bridge_map_obj_get(bridge_name, &bridge_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge member get failed ");
        return;
    }
    bridge_obj->nas_bridge_publish_memberlist_event(memlist, op);
}
bool nas_bridge_is_empty(const std::string & bridge_name){
     NAS_BRIDGE * parent_bridge_obj = nullptr;
     if(nas_bridge_map_obj_get(bridge_name,&parent_bridge_obj)!=STD_ERR_OK){
         EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE","Can't find bridge %s",bridge_name.c_str());
         return false;
     }

     if(parent_bridge_obj->attached_vlans.size() || parent_bridge_obj->tagged_members.size() ||
         parent_bridge_obj->untagged_members.size()){
            EV_LOGGING(INTERFACE,ERR,"NAS-VLAN-ATTACH","Bridge %s is not empty",bridge_name.c_str());
            return false;
     }

     return true;
}


t_std_error nas_bridge_utils_associate_npu_port(const char * br_name, const char *mem_name, ndi_port_t *ndi_port,
                                                    nas_port_mode_t mode, bool associate)
{

    NAS_BRIDGE *br_obj = nullptr;
    if (nas_bridge_map_obj_get(br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge object get failed %s", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    EV_LOGGING(INTERFACE,DEBUG,"NAS-VLAN-MAP"," Interface (dis)association for interface %s port %d and bridge %s",
                                mem_name, ndi_port->npu_port, br_name);
    std::string _mem(mem_name);
    return br_obj->nas_bridge_associate_npu_port(_mem, ndi_port, mode, associate);
}

t_std_error nas_bridge_utils_os_create_bridge(const char *br_name, hal_ifindex_t *if_index)
{
    cps_api_object_guard _og(cps_api_object_create());
    if(!_og.valid()){
        EV_LOGGING(INTERFACE,ERR,"INT-DB-GET","Failed to create object  for bridge deletion ");
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    cps_api_set_key_data(_og.get(),IF_INTERFACES_INTERFACE_NAME,
                               cps_api_object_ATTR_T_BIN, br_name, strlen(br_name)+1);
    // TODO change the name of API
    if (nas_os_add_vlan(_og.get(), if_index) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge delete failed in the OS %s", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    cps_api_object_attr_add_u32(_og.get(),DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,*if_index);
    cps_api_object_attr_add_u32(_og.get(),IF_INTERFACES_INTERFACE_ENABLED,true);

    nas_os_interface_set_attribute(_og.get(),IF_INTERFACES_INTERFACE_ENABLED);


    return STD_ERR_OK;
}

t_std_error nas_bridge_utils_os_delete_bridge(const char *br_name)
{
    cps_api_object_guard _og(cps_api_object_create());
    if(!_og.valid()){
        EV_LOGGING(INTERFACE,ERR,"INT-DB-GET","Failed to create object  for bridge deletion ");
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    cps_api_object_attr_add(_og.get(),IF_INTERFACES_INTERFACE_NAME,
                               br_name, strlen(br_name)+1);

    if (nas_os_delete_bridge(_og.get()) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge delete failed in the OS %s", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return STD_ERR_OK;
}

/*  Vlan interface publish */
void nas_bridge_utils_publish_vlan_intf_event(const char * bridge_name, cps_api_operation_types_t op)
{
    cps_api_object_guard og(cps_api_object_create());

    // TODO double fill
    cps_api_key_from_attr_with_qual(cps_api_object_key(og.get()), BASE_IF_VLAN_IF_INTERFACES_INTERFACE_OBJ,
                                      cps_api_qualifier_OBSERVED);
    cps_api_set_key_data(og.get(),IF_INTERFACES_INTERFACE_NAME,
                           cps_api_object_ATTR_T_BIN, bridge_name, strlen(bridge_name)+1);
    cps_api_object_attr_add(og.get(),IF_INTERFACES_INTERFACE_TYPE,
                            (const void *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
                            strlen(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN)+1);
    cps_api_object_set_type_operation(cps_api_object_key(og.get()), op);
    if (op == cps_api_oper_CREATE) {
      if (nas_bridge_fill_info(std::string(bridge_name), og.get()) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF","Failed to add Bridge info");
      }
    }

    cps_api_event_thread_publish(og.get());
}

/* DElete the 1Q bridge inthe NPU and kernel along with it all members */
t_std_error nas_bridge_delete_bridge(const char *br_name)
{
    t_std_error  rc = STD_ERR_OK;
    NAS_BRIDGE *br_obj = nullptr;
    memberlist_t tagged_list, untagged_list;
    EV_LOGGING(INTERFACE,INFO,"NAS-BRIDGE", " Delete bridge %s ", br_name);

    if (nas_bridge_map_obj_get(br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge object get failed %s", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    /*  Get the list of untagged and tagged members  */
    br_obj->nas_bridge_get_member_list(NAS_PORT_TAGGED, tagged_list);
    br_obj->nas_bridge_get_member_list(NAS_PORT_UNTAGGED, untagged_list);
    /*  Remove tagged and untagged mebers  */
    if ((rc= br_obj->nas_bridge_add_remove_memberlist(tagged_list, NAS_PORT_TAGGED, false)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to delete tagged memberlist %s", br_name);
        return rc;
    }

    if ((rc= br_obj->nas_bridge_add_remove_memberlist(untagged_list, NAS_PORT_UNTAGGED, false)) != STD_ERR_OK) {
        /*  Add back removed tagged members and return  */
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to delete untagged memberlist %s", br_name);
        br_obj->nas_bridge_add_remove_memberlist(untagged_list, NAS_PORT_UNTAGGED, true);
        return rc;
    }
    if (br_obj->bridge_mode_get() == BASE_IF_BRIDGE_MODE_1D)  {
        /*  Delete  VXLAN members if any */
        memberlist_t vxlan_list;
        NAS_DOT1D_BRIDGE *dot1d_bridge = dynamic_cast<NAS_DOT1D_BRIDGE *>(br_obj);
        dot1d_bridge->nas_bridge_get_vxlan_member_list(vxlan_list);
        if (!vxlan_list.empty()) {
            if ((rc = dot1d_bridge->nas_bridge_add_remove_memberlist(vxlan_list, NAS_PORT_TAGGED, false)) != STD_ERR_OK) {
                EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to delete vxlan memberlist %s", br_name);
                return rc;

            }
        }
    }
    /*  Delete bridge in the NPU and delete objects */
    if ((rc = nas_bridge_utils_delete(br_name)) !=STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to delete bridge %s in the npu", br_name);
        return rc;
    }
    if (( rc = nas_bridge_utils_os_delete_bridge(br_name)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to delete bridge %s in the os", br_name);
        return rc;
    }

    /*  Now delete the vlan sub interfaces  */
    intf_list_t & sub_intf_list = tagged_list;
    if ((rc = nas_interface_vlan_subintf_list_delete(sub_intf_list)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Failed to delete vlan sub interfaces of bridge %s in the kernel",
                                 br_name);
        return rc;
    }

    EV_LOGGING(INTERFACE,NOTICE,"NAS-BRIDGE", " Delete bridge %s Succesful", br_name);
    return STD_ERR_OK;
}


t_std_error nas_bridge_process_cps_memberlist(const char *br_name, memberlist_t *new_tagged_list,
                                                        memberlist_t *new_untagged_list)
{

    t_std_error rc = STD_ERR_OK;
    NAS_BRIDGE *br_obj = nullptr;
    memberlist_t old_tagged_list, old_untagged_list;
    if (nas_bridge_map_obj_get(br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge object get failed %s", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    if (br_obj->bridge_mode_get() == BASE_IF_BRIDGE_MODE_1D)  {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " 1D Bridge object not supported %s", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    /*  Get the list of untagged and tagged members  */
    if (new_tagged_list != nullptr) {
        if ((rc = nas_bridge_utils_update_member_list(br_name, *new_tagged_list, old_tagged_list,
                                                    NAS_PORT_TAGGED)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge tagged member update failed  %s", br_name);
            return STD_ERR(INTERFACE, FAIL, 0);
        }
    }

    if (new_untagged_list != nullptr) {
        if ((rc = nas_bridge_utils_update_member_list(br_name, *new_untagged_list, old_untagged_list,
                                                    NAS_PORT_UNTAGGED)) != STD_ERR_OK) {
            /*  If failed then rollback tagged member update */
            if (new_tagged_list != nullptr) {
                nas_bridge_utils_update_member_list(br_name, old_tagged_list, *new_tagged_list, NAS_PORT_TAGGED);
            }
            EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge untagged member update failed  %s", br_name);
            return STD_ERR(INTERFACE, FAIL, 0);
        }
    }
    return STD_ERR_OK;
}

t_std_error nas_bridge_utils_os_add_remove_memberlist(const char *br_name, memberlist_t & memlist, nas_port_mode_t port_mode, bool add)
{

    NAS_BRIDGE *br_obj = nullptr;
    if (nas_bridge_map_obj_get(br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge object get failed %s", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    if (br_obj->nas_bridge_os_add_remove_memberlist(memlist, port_mode, add) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge memberlist addition failed %s", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return STD_ERR_OK;
}
t_std_error nas_bridge_utils_set_attribute(const char *br_name, cps_api_object_t obj , cps_api_object_it_t & it)
{

    NAS_BRIDGE *br_obj = nullptr;
    if (nas_bridge_map_obj_get(br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge object get failed %s", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    return (br_obj->nas_bridge_set_attribute(obj, it));
}

t_std_error nas_bridge_utils_set_learning_disable(const char *br_name, bool disable){
     NAS_BRIDGE *br_obj = nullptr;
    if (nas_bridge_map_obj_get(br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge object get failed %s", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }

    return br_obj->nas_bridge_set_learning_disable(disable);
}

t_std_error nas_bridge_utils_check_membership(const char *br_name, const char *mem_name , bool *present)
{

    if (present == NULL) return STD_ERR(INTERFACE, FAIL, 0);

    NAS_BRIDGE *br_obj = nullptr;
    std::string _mem_name(mem_name);
    *present = false;
    if (nas_bridge_map_obj_get(br_name, &br_obj) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-BRIDGE", " Bridge object get failed %s", br_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    nas_int_type_t mem_type;
    if ((nas_get_int_name_type(mem_name, &mem_type)) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR, "NAS-BRIDGE", " NAS OS L2 PORT Event: Failed to get member type %s ", mem_name);
        return STD_ERR(INTERFACE, FAIL, 0);
    }
    nas_port_mode_t port_mode = (mem_type == nas_int_type_VLANSUB_INTF) ? NAS_PORT_TAGGED: NAS_PORT_UNTAGGED;

    if (mem_type != nas_int_type_VXLAN) {
        if (port_mode == NAS_PORT_TAGGED)  {
            br_obj->nas_bridge_check_tagged_membership(_mem_name, present);
        } else {
            br_obj->nas_bridge_check_untagged_membership(_mem_name, present);
        }
    } else {
        if (br_obj->bridge_mode_get() == BASE_IF_BRIDGE_MODE_1D) {
            NAS_DOT1D_BRIDGE *dot1d_bridge = dynamic_cast<NAS_DOT1D_BRIDGE *>(br_obj);
            dot1d_bridge->nas_bridge_check_vxlan_membership(_mem_name, present);
        } else {
            *present = false;
        }
    }
    return STD_ERR_OK;
}

