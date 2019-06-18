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
 * filename: nas_interface_bridge_update_vlan.cpp
 */

#include "cps_api_object.h"
#include "ds_common_types.h"
#include "cps_api_object_key.h"
#include "cps_api_events.h"
#include "cps_class_map.h"
#include "nas_int_bridge.h"
#include "bridge/nas_interface_1d_bridge.h"
#include "bridge/nas_interface_1q_bridge.h"
#include "bridge/nas_interface_bridge_cps.h"
#include "bridge/nas_interface_bridge_map.h"
#include "bridge/nas_interface_bridge_utils.h"


cps_api_return_code_t attach_vlan(cps_api_object_t obj)
{
    NAS_BRIDGE *br_obj   = nullptr;
    NAS_BRIDGE *vlan_obj = nullptr;

    auto bridge_name_attr = cps_api_object_attr_get(obj, BRIDGE_DOMAIN_BRIDGE_VLAN_UPDATE_INPUT_BRIDGE_NAME);
    if (bridge_name_attr == nullptr) {
        return  cps_api_ret_code_ERR;
    }
    std::string br_name = static_cast<char*>(cps_api_object_attr_data_bin(bridge_name_attr));

    if (nas_bridge_map_obj_get(br_name, &br_obj) != STD_ERR_OK) {
        return  cps_api_ret_code_ERR;
    }

    auto vlan_name_attr = cps_api_object_attr_get(obj, BRIDGE_DOMAIN_BRIDGE_VLAN_UPDATE_INPUT_VLAN_NAME);
    if (vlan_name_attr == nullptr) {
        return  cps_api_ret_code_ERR;
    }
    std::string vlan_name = static_cast<char*>(cps_api_object_attr_data_bin(vlan_name_attr));

    if (nas_bridge_map_obj_get(vlan_name, &vlan_obj) != STD_ERR_OK) {
        return  cps_api_ret_code_ERR;
    }

    /* Attach VLAN to a Virtual Network with no members */
    if(br_obj->bridge_mode == BASE_IF_BRIDGE_MODE_1Q) {

        if(br_obj->num_attached_vlans == 0) {
            // Check for VLAN existence in NPU and kernel before proceeding
            if (nas_get_bridge_node_from_name(vlan_name.c_str()) == NULL) {
                /* vlan object doesn't exists */
                return cps_api_ret_code_ERR;
            }

            // Set proxy bridge name to link VN Bridge to VLAN (P,V)
            br_obj->nas_bridge_add_vlan_in_attached_list(vlan_name);

            // maintain a list of attached vlans
            br_obj->num_attached_vlans += 1;

        } else {
            // need a mode change before a second attach VLAN request, Reject request
            return cps_api_ret_code_ERR;
        }

    }

    /* Attach VLAN to a Virtual Network with one or more members */
    if(br_obj->bridge_mode == BASE_IF_BRIDGE_MODE_1D) {

        // Migrate the VLAN members (P,V) from VLAN Bridge to VN Bridge (1D) in NPU
        /*if ((rc = nas_npu_migrate_bridge_members(br_obj, vlan_obj)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge member migration failed for bridge %s ", vlan_name);
            return STD_ERR(INTERFACE, FAIL, 0);
        }*/

        // Push VLAN config to VN Bridge (1D) in NPU
        // Retain the VLAN Bridge in NPU without any members


        // Add the VLAN members (P,V) to VN Bridge in kernel
        // TODO : Add nas_os_migrate_bridge_members
        /*if ((rc = nas_os_migrate_bridge_members(br_obj, vlan_obj)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge member migration failed for bridge %s ", vlan_name);
            return STD_ERR(INTERFACE, FAIL, 0);
        }*/

        // Push VLAN config to VN bridge in kernel
        // Retain the VLAN Bridge in kernel without any members

        // Set proxy bridge name to link VN Bridge to VLAN (P,V)
        br_obj->nas_bridge_add_vlan_in_attached_list(vlan_name);

        // maintain a list of attached vlans
        br_obj->num_attached_vlans += 1;
    }
    return cps_api_ret_code_OK;
}


cps_api_return_code_t detach_vlan(cps_api_object_t obj)
{
    NAS_BRIDGE *br_obj = nullptr;
    NAS_BRIDGE *vlan_obj = nullptr;

    auto bridge_name_attr = cps_api_object_attr_get(obj, BRIDGE_DOMAIN_BRIDGE_VLAN_UPDATE_INPUT_BRIDGE_NAME);
    if (bridge_name_attr == nullptr) {
        return  cps_api_ret_code_ERR;
    }
    std::string br_name = static_cast<char*>(cps_api_object_attr_data_bin(bridge_name_attr));

    if (nas_bridge_map_obj_get(br_name, &br_obj) != STD_ERR_OK) {
        return  cps_api_ret_code_ERR;
    }

    auto vlan_name_attr = cps_api_object_attr_get(obj, BRIDGE_DOMAIN_BRIDGE_VLAN_UPDATE_INPUT_VLAN_NAME);
    if (vlan_name_attr == nullptr) {
        return  cps_api_ret_code_ERR;
    }
    std::string vlan_name = static_cast<char*>(cps_api_object_attr_data_bin(vlan_name_attr));

    if (nas_bridge_map_obj_get(vlan_name, &vlan_obj) != STD_ERR_OK) {
        return  cps_api_ret_code_ERR;
    }

    // Check for VLAN existence in NPU and kernel before proceeding
    if (nas_get_bridge_node_from_name(vlan_name.c_str()) == NULL) {
        /* vlan object doesn't exists */
        return cps_api_ret_code_ERR;
    }

    // if 1q mode then just remove the reference in the bridge and in the vlan structure.
    if(br_obj->bridge_mode == BASE_IF_BRIDGE_MODE_1Q) {

        br_obj->nas_bridge_remove_vlan_member_from_attached_list(vlan_name);
        br_obj->num_attached_vlans -= 1;
    } else {

        // else if 1D mode then
        // Migrate the VLAN members (P,V) to NPU Bridge in NPU
        // TODO : migrate vlan specific members only
        /* if ((rc = nas_npu_migrate_bridge_members(vlan_obj, br_obj)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge member migration failed for bridge %s ", br_name);
            return STD_ERR(INTERFACE, FAIL, 0);
        }*/

        // Push VLAN config to NPU Bridge in NPU

        // Migrate the VLAN members (P,V) to NPU Bridge in kernel
        // TODO : Add nas_os_migrate_bridge_members, move vlan specific members
        /*if ((rc = nas_os_migrate_bridge_membersvlan_obj, br_obj)) != STD_ERR_OK) {
            EV_LOGGING(INTERFACE,ERR,"NAS-INT", " Bridge member migration failed for bridge %s ", vlan_name);
            return STD_ERR(INTERFACE, FAIL, 0);
        }*/

        // Push VLAN config to NPU Bridge in kernel
    }

    if (br_obj->num_attached_vlans <= 1) {
        // Change bridge mode from 1D to 1Q
    }
    return cps_api_ret_code_OK;
}

static cps_api_return_code_t bridge_vlan_update_handler (void *context, cps_api_transaction_params_t *param, size_t ix)
{
    cps_api_object_attr_t add_attr;

    if(param == NULL){
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-UPDATE", "VLAN update handler with no param");
        return cps_api_ret_code_ERR;
    }

    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);
    if (obj == NULL) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-UPDATE", "VLAN update handler operation object not present");
        return cps_api_ret_code_ERR;
    }

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    if (op != cps_api_oper_ACTION) {
        EV_LOGGING(INTERFACE, ERR, "NAS-VLAN-UPDATE", "Invalid VLAN update operation action");
        return cps_api_ret_code_ERR;
    }

    bool add = false;
    add_attr = cps_api_object_attr_get(obj, BRIDGE_DOMAIN_BRIDGE_VLAN_UPDATE_INPUT_ADD);
    if(add_attr != nullptr) {
        add = static_cast<bool>(cps_api_object_attr_data_bin(add_attr));
    }

    if (add == true) {
        attach_vlan(obj);
    } else {
        return detach_vlan(obj);
    }

    return cps_api_ret_code_OK;
}

t_std_error nas_bridge_vlan_update_init(cps_api_operation_handle_t handle)
{
    cps_api_registration_functions_t f;
    memset(&f, 0, sizeof(f));

    char buff[CPS_API_KEY_STR_MAX];
    if (!cps_api_key_from_attr_with_qual(&f.key, BRIDGE_DOMAIN_BRIDGE_VLAN_UPDATE_OBJ, cps_api_qualifier_TARGET)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG","Could not translate to key %s", cps_api_key_print(&f.key,buff,sizeof(buff)-1));
        return STD_ERR(INTERFACE,FAIL,0);
    }

    f.handle = handle;
    f._write_function = bridge_vlan_update_handler;

    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
        return STD_ERR(INTERFACE,FAIL,0);
    }
    return STD_ERR_OK;
}

/*
   Scenario 1.
   Attach and Detach VLAN to a Virtual-Network (with empty member list)
   ==============================================================================================
   Create Virtual-Network
   1. Bridge created in the kernel. Performed by nas-linux.
   2. Create Bridge object in the NAS-Interface and set the mode to .1Q, add to map. Performed by nas-interface
   3. Check for existence of the object in .1q map, return with failure if object doesn't exist

   Attach VLAN to Virtual-Network
   1. Set the proxy bridge_name = VLAN in the VN bridge object

   Push Virtual-Network Configuration
   1. Push the configuration on VLAN
   2. Save in the VN Bridge object in NAS-Interface

   Detach VLAN from Virtual-Network
   1. Push VLAN’s config in the kernel and in the NPU
   2. Update VN’s bridge object

   Attach another VLAN to the virtual Network
   1. Configuration failure if the VN bridge already has vlan member and VN Bridge is in .1Q mode.


   Scenario 2.
   Attach a VLANs to the Virtual-Network and change the Mode to .1D bridge
   ==============================================================================================
   Create Virtual-Network
   1. Bridge created in the kernel.
   2. Create Bridge object in the NAS-Interface and set the mode to .1Q

   Attach VLAN to Virtual-Network
   1. Set the proxy bridge_name = VLAN in the VN bridge object

   Move the VN mode to .1D
   1. Migrate VLAN’s members to VN’s bridge in the kernel
   2. Migrate VLAN’s members to the corresponding .1D bridge in the NPU.
   3. Migrate applicable VLAN’s configuration ?

   Add (P,V) member to VN
   1. Add the member to the VN’s bridge in the kernel and in the .1d bridge in the kernel.
   2. Addition of (P,V) fails if bridge is not in .1D mode.

   Detach VLAN from VN
   1. Remove VLAN’s members from the VN’s bridge.
   2. Remove VLAN’s members from .1D bridge in the NPU
   3. Add VLAN’s members in the NPU
   4. Add VLAN’s members in the kernel
   5. Re-apply VLAN’s configuration from VLAN’s bridge object in the kernel and in the NPU.

   Remove VN
   (Application can reject VN deletion is any VLAN is attached to the VN)
   1. Bridge is deleted from the kernel.
   2. If Bridge delete event comes from Kernel then remove Clean-up .1D bridge and its member in the NPU
   3. IF any VLAN is attached then restore its members in the kernel and in the NPU by creating VLAN and adding member to the VLAN.  No need to support initially.


   Scenario 3.
   Attach a VLAN to the Virtual-Network and Change the mode to .1D to .1Q
   ==============================================================================================
   Create Virtual-Network
   1. Bridge created in the kernel.
   2. Create Bridge object in the NAS-Interface and set the mode to .1Q

   Attach VLAN to Virtual-Network
   1. Set the proxy bridge_name = VLAN in the VN bridge object

   Move the VN mode to .1D
   1. Migrate VLAN’s members to VN’s bridge in the kernel
   2. Migrate VLAN’s members to the corresponding .1D bridge in the NPU.
   3. Migrate applicable VLAN’s configuration ?

Add (P,V) member to VN
1. Add the member to the VN’s bridge in the kernel and in the .1d bridge in the kernel.

Delete (P,V) member from the VN
1. Delete the member from kernel and from the .1D bridge

Move the VN mode to the .1Q
1. Since there is only one VLAN attached
2. Create VLAN in the NPU and migrate all members to the VLAN and delete .1D bridge.

Detach VLAN from VN
1. Remove VLAN’s members from the VN’s bridge.
2. Remove VLAN’s members from .1D bridge in the NPU if bridge mode is .1D.
3. Add VLAN’s members in the NPU
4. Add VLAN’s members in the kernel if members are not part of the VLAN bridge.
5. Re-apply VLAN’s configuration from VLAN’s bridge object in the kernel (member_move == true) and in the NPU.
*/


