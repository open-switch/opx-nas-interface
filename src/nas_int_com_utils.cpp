/*
 * Copyright (c) 2017 Dell Inc.
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
 * filename: nas_int_com_utils.cpp
 */

/*
 * nas_intf_handle_intf_mode_change is to update nas-l3 about the interface mode
 * change(BASE_IF_MODE_MODE_NONE/BASE_IF_MODE_MODE_L2)
 */

#include "event_log.h"
#include "event_log_types.h"
#include "nas_int_base_if.h"
#include "nas_int_utils.h"
#include "dell-base-routing.h"
#include "cps_api_operation.h"
#include "cps_api_object_key.h"
#include "cps_class_map.h"
#include "l2-multicast.h"

bool nas_intf_handle_intf_mode_change (const char * if_name, BASE_IF_MODE_t mode)
{
    cps_api_transaction_params_t params;
    cps_api_object_t             obj;
    cps_api_key_t                keys;
    bool                         rc = true;

    EV_LOGGING(INTERFACE, DEBUG, "IF_CONT", "Interface mode change update called");
    do {
        if ((obj = cps_api_object_create()) == NULL) {
            EV_LOGGING(INTERFACE,ERR,"IF_CONT", "Interface mode change update failed");
            rc = false;
            break;
        }
        cps_api_object_guard obj_g (obj);
        if (cps_api_transaction_init(&params) != cps_api_ret_code_OK) {
            rc = false;
            break;
        }
        cps_api_transaction_guard tgd(&params);
        cps_api_key_from_attr_with_qual(&keys, BASE_ROUTE_INTERFACE_MODE_CHANGE_OBJ,
                                        cps_api_qualifier_TARGET);
        cps_api_object_set_key(obj, &keys);

        cps_api_object_attr_add(obj, BASE_ROUTE_INTERFACE_MODE_CHANGE_INPUT_IFNAME,
                                if_name, strlen(if_name) + 1);
        cps_api_object_attr_add_u32(obj, BASE_ROUTE_INTERFACE_MODE_CHANGE_INPUT_MODE,
                                    mode);

        if (cps_api_action(&params, obj) != cps_api_ret_code_OK) {
            rc = false;
            break;
        }

        obj_g.release();

        if (cps_api_commit(&params) != cps_api_ret_code_OK) {
            rc = false;
            break;
        }

    } while (false);

    EV_LOGGING(INTERFACE, DEBUG, "IF_CONT", "Interface mode change update returning  (%s)",
               rc == true ? "SUCCESS" : "FAILED");
    return rc;
}

bool nas_intf_handle_intf_mode_change (hal_ifindex_t ifx, BASE_IF_MODE_t mode)
{
    interface_ctrl_t intf_ctrl;
    memset(&intf_ctrl, 0, sizeof(intf_ctrl));
    intf_ctrl.q_type = HAL_INTF_INFO_FROM_IF;
    intf_ctrl.if_index = ifx;

    if (dn_hal_get_interface_info(&intf_ctrl) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR,"IF_CONT", "Interface (%d) not found", ifx);
        return false;
    }

    return nas_intf_handle_intf_mode_change(intf_ctrl.if_name, mode);
}

/*  Cleanup L2 Multicast membership for the interface */
bool nas_intf_cleanup_l2mc_config (hal_ifindex_t ifx,  hal_vlan_id_t vlan_id)
{
    cps_api_transaction_params_t params;
    cps_api_object_t             obj;
    cps_api_key_t                keys;
    bool                         rc = true;

    EV_LOGGING(INTERFACE, DEBUG, "IF_CONT", "Interface L2MC clean UP");
    do {
        if ((obj = cps_api_object_create()) == NULL) {
            EV_LOGGING(INTERFACE,ERR,"IF_CONT", "Interface L2MC clean UP failed ");
            rc = false;
            break;
        }
        cps_api_object_guard obj_g (obj);
        if (cps_api_transaction_init(&params) != cps_api_ret_code_OK) {
            rc = false;
            break;
        }
        cps_api_transaction_guard tgd(&params);
        cps_api_key_from_attr_with_qual(&keys, BASE_L2_MCAST_CLEANUP_L2MC_MEMBER_OBJ,
                                        cps_api_qualifier_TARGET);
        cps_api_object_set_key(obj, &keys);

        cps_api_object_attr_add_u32(obj, BASE_L2_MCAST_CLEANUP_L2MC_MEMBER_INPUT_IFINDEX, ifx);
        if (vlan_id != 0) {
            cps_api_object_attr_add_u32(obj,  BASE_L2_MCAST_CLEANUP_L2MC_MEMBER_INPUT_VLAN_ID, vlan_id);
        }

        if (cps_api_action(&params, obj) != cps_api_ret_code_OK) {
            rc = false;
            break;
        }

        obj_g.release();

        if (cps_api_commit(&params) != cps_api_ret_code_OK) {
            rc = false;
            break;
        }

    } while (false);

    EV_LOGGING(INTERFACE, DEBUG, "IF_CONT", "Interface L2MC clean UP (%s)",
               rc == true ? "SUCCESS" : "FAILED");
    return rc;
}

