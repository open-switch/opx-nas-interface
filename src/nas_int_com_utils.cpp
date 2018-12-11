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
 * filename: nas_int_com_utils.cpp
 */

/*
 * nas_intf_handle_intf_mode_change is to update nas-l3 about the interface mode
 * change(BASE_IF_MODE_MODE_NONE/BASE_IF_MODE_MODE_L2)
 */

#include "dell-interface.h"
#include "dell-base-if.h"
#include "event_log.h"
#include "event_log_types.h"
#include "nas_int_base_if.h"
#include "nas_int_utils.h"
#include "nas_os_interface.h"
#include "dell-base-routing.h"
#include "dell-base-if-phy.h"
#include "cps_api_operation.h"
#include "cps_api_object_key.h"
#include "cps_class_map.h"
#include "l2-multicast.h"
#include "std_utils.h"

t_std_error nas_get_if_type_from_name_or_ifindex (const char *if_name, hal_ifindex_t *ifindex, nas_int_type_t *type) {
    interface_ctrl_t if_info;
    memset(&if_info,0,sizeof(if_info));

    if( if_name != nullptr) {
        safestrncpy(if_info.if_name,if_name,sizeof(if_info.if_name));
        if_info.q_type = HAL_INTF_INFO_FROM_IF_NAME;
    }
    else if( ifindex != nullptr) {
        if_info.if_index = *ifindex;
        if_info.q_type = HAL_INTF_INFO_FROM_IF;

    }

    if (dn_hal_get_interface_info(&if_info) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE, ERR,"INTF-C","Failed to get if_info");
        return STD_ERR(INTERFACE,FAIL, 0);
    }
    *ifindex = if_info.if_index;
    *type = if_info.int_type;
    return STD_ERR_OK;
};


bool nas_intf_handle_intf_mode_change (const char * if_name, BASE_IF_MODE_t mode)
{
    cps_api_transaction_params_t params;
    cps_api_object_t             obj;
    cps_api_key_t                keys;
    bool                         rc = true;

    EV_LOGGING(INTERFACE, DEBUG, "IF_CONT", "Interface mode change update called for %s, mode is  %d", if_name, mode);
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

        if (ifx != 0) {
            cps_api_object_attr_add_u32(obj, BASE_L2_MCAST_CLEANUP_L2MC_MEMBER_INPUT_IFINDEX, ifx);
        }
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

t_std_error nas_set_intf_admin_state_os(hal_ifindex_t if_index, bool admin_state)
{

    char buff[CPS_API_MIN_OBJ_LEN];
    cps_api_object_t obj = cps_api_object_init(buff,sizeof(buff));
    if (!cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                DELL_BASE_IF_CMN_IF_INTERFACES_STATE_INTERFACE_OBJ,
                cps_api_qualifier_TARGET)) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF-REG", "Failure to create cps obj admin set operation %d", if_index);
        return STD_ERR(INTERFACE,FAIL, 0);
    }
    cps_api_object_attr_add_u32(obj,DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX,if_index);
    cps_api_object_attr_add_u32(obj,IF_INTERFACES_INTERFACE_ENABLED, admin_state);
    if (nas_os_interface_set_attribute(obj, IF_INTERFACES_INTERFACE_ENABLED) != STD_ERR_OK) {
        EV_LOGGING(INTERFACE,ERR,"NAS-IF", "Failed to set admin state for %d ",if_index);
        return STD_ERR(INTERFACE,FAIL, 0);
    }
    return STD_ERR_OK;
}

bool if_data_from_obj(obj_intf_cat_t obj_cat, cps_api_object_t o, interface_ctrl_t& i) {
    cps_api_object_attr_t _name = cps_api_get_key_data(o,(obj_cat == obj_INTF) ?
                                                  (uint)IF_INTERFACES_INTERFACE_NAME:
                                                  (uint)IF_INTERFACES_STATE_INTERFACE_NAME);
    cps_api_object_attr_t _ifix = cps_api_object_attr_get(o,(obj_cat == obj_INTF) ?
                                (uint)DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_IF_INDEX:
                                (uint)IF_INTERFACES_STATE_INTERFACE_IF_INDEX);


    cps_api_object_attr_t _npu = nullptr;
    cps_api_object_attr_t _port = nullptr;

    if (obj_cat == obj_INTF) {
        _npu = cps_api_object_attr_get(o,BASE_IF_PHY_IF_INTERFACES_INTERFACE_NPU_ID);
        _port = cps_api_object_attr_get(o,BASE_IF_PHY_IF_INTERFACES_INTERFACE_PORT_ID);
    }

    if (_ifix!=nullptr) {
        memset(&i,0,sizeof(i));
        i.if_index = cps_api_object_attr_data_u32(_ifix);
        i.q_type = HAL_INTF_INFO_FROM_IF;
        if (dn_hal_get_interface_info(&i)==STD_ERR_OK) return true;
    }

    if (_name!=nullptr) {
        memset(&i,0,sizeof(i));
        strncpy(i.if_name,(const char *)cps_api_object_attr_data_bin(_name),sizeof(i.if_name)-1);
        i.q_type = HAL_INTF_INFO_FROM_IF_NAME;
        if (dn_hal_get_interface_info(&i)==STD_ERR_OK) return true;
    }

    if (_npu!=nullptr && _port!=nullptr &&
        cps_api_object_attr_len(_npu) > 0 && cps_api_object_attr_len(_port) > 0) {
        memset(&i,0,sizeof(i));
        i.npu_id = cps_api_object_attr_data_u32(_npu);
        i.port_id = cps_api_object_attr_data_u32(_port);
        i.q_type = HAL_INTF_INFO_FROM_PORT;
        if (dn_hal_get_interface_info(&i)==STD_ERR_OK) return true;
    }

    EV_LOGGING(INTERFACE,ERR,"IF-CPS-CREATE","Invalid fields - can't locate specified port");
    return false;
}

#define SPEED_1MBPS (1000*1000)
#define SPEED_1GIGE (uint64_t)(1000*1000*1000)
static std::unordered_map<BASE_IF_SPEED_t, uint64_t ,std::hash<int>>
_base_to_ietf64bit_speed = {
    {BASE_IF_SPEED_0MBPS,                     0},
    {BASE_IF_SPEED_10MBPS,       10*SPEED_1MBPS},
    {BASE_IF_SPEED_100MBPS,     100*SPEED_1MBPS},
    {BASE_IF_SPEED_1GIGE,         1*SPEED_1GIGE},
    {BASE_IF_SPEED_10GIGE,       10*SPEED_1GIGE},
    {BASE_IF_SPEED_20GIGE,       20*SPEED_1GIGE},
    {BASE_IF_SPEED_25GIGE,       25*SPEED_1GIGE},
    {BASE_IF_SPEED_40GIGE,       40*SPEED_1GIGE},
    {BASE_IF_SPEED_50GIGE,       50*SPEED_1GIGE},
    {BASE_IF_SPEED_100GIGE,     100*SPEED_1GIGE},
};

bool nas_base_to_ietf_state_speed(BASE_IF_SPEED_t speed, uint64_t *ietf_speed) {
    auto it = _base_to_ietf64bit_speed.find(speed);
    if (it != _base_to_ietf64bit_speed.end()) {
        *ietf_speed = it->second;
        return true;
    }
    return false;
}

